// Adapted from pipecat-ai/pipecat-esp32 (MIT License), esp32-m5stack-cores3/src/media.cpp.
// https://github.com/pipecat-ai/pipecat-esp32 @ e70e3b1

#include <opus.h>

#include <atomic>
#include <cstring>
#include <cstdio>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "esp_check.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "audio_hw.h"
#include "main.h"

#define SAMPLE_RATE (16000)

#define OPUS_BUFFER_SIZE 1276  // 1276 bytes is recommended by opus_encode
#define PCM_BUFFER_SIZE 640

#define OPUS_ENCODER_BITRATE 30000
#define OPUS_ENCODER_COMPLEXITY 0

// Depth chosen for ~200ms of buffering (10 * 20ms frames) between the decode callback
// (arbitrary libpeer task) and the audio task that drains it into the speaker.
#define PLAYBACK_QUEUE_DEPTH 10

std::atomic<bool> is_playing = false;
unsigned int silence_count = 0;

// Decoded PCM frames queued here by pipecat_audio_decode() and drained by
// pipecat_send_audio(), which is the only thing that touches the (now persistent) I2S TX
// channel. Frames are fixed-size (PCM_BUFFER_SIZE bytes = 320 samples) so a queue of
// byte blobs is sufficient; opus_decode always fills up to that many samples per call.
static QueueHandle_t playback_queue = NULL;

void set_is_playing(int16_t *in_buf, size_t in_samples) {
  bool any_set = false;
  for (size_t i = 0; i < in_samples; i++) {
    if (in_buf[i] != -1 && in_buf[i] != 0 && in_buf[i] != 1) {
      any_set = true;
    }
  }

  if (any_set) {
    silence_count = 0;
  } else {
    silence_count++;
  }

  if (silence_count >= 20 && is_playing) {
    is_playing = false;
  } else if (any_set && !is_playing) {
    is_playing = true;
  }
}

void pipecat_init_audio_capture() {
  playback_queue =
      xQueueCreate(PLAYBACK_QUEUE_DEPTH, PCM_BUFFER_SIZE);
  pipecat_audio_hw_init();
}

opus_int16 *decoder_buffer = NULL;
OpusDecoder *opus_decoder = NULL;

void pipecat_init_audio_decoder() {
  int decoder_error = 0;
  opus_decoder = opus_decoder_create(SAMPLE_RATE, 1, &decoder_error);
  if (decoder_error != OPUS_OK) {
    printf("Failed to create OPUS decoder");
    return;
  }

  decoder_buffer = (opus_int16 *)malloc(PCM_BUFFER_SIZE);
}

void double_volume(int16_t *samples, size_t num_samples) {
    for (size_t i = 0; i < num_samples; i++) {
        int32_t amplified = (int32_t)samples[i] * 2;

        // Clamp to 16-bit range
        if (amplified > INT16_MAX) {
            amplified = INT16_MAX;
        } else if (amplified < INT16_MIN) {
            amplified = INT16_MIN;
        }

        samples[i] = (int16_t)amplified;
    }
}

void pipecat_audio_decode(uint8_t *data, size_t size) {
  int decoded_size = opus_decode(opus_decoder, data, size, decoder_buffer,
                                 PCM_BUFFER_SIZE / sizeof(opus_int16), 0);

  if (decoded_size > 0) {
    set_is_playing(decoder_buffer, decoded_size);
    if (is_playing) {
      double_volume(decoder_buffer, decoded_size);
      // decoder_buffer holds up to PCM_BUFFER_SIZE/sizeof(opus_int16) samples; zero-pad
      // a short frame so the queued blob is always a full PCM_BUFFER_SIZE frame.
      int16_t frame[PCM_BUFFER_SIZE / sizeof(int16_t)] = {0};
      memcpy(frame, decoder_buffer, decoded_size * sizeof(int16_t));
      // Drop the frame rather than block: a full queue means playback is already
      // behind, and blocking here would stall the datachannel/webrtc task calling us.
      xQueueSend(playback_queue, frame, 0);
    }
  }
}

OpusEncoder *opus_encoder = NULL;
uint8_t *encoder_output_buffer = NULL;
int16_t *read_buffer = NULL;

void pipecat_init_audio_encoder() {
  int encoder_error;
  opus_encoder = opus_encoder_create(SAMPLE_RATE, 1, OPUS_APPLICATION_VOIP,
                                     &encoder_error);
  if (encoder_error != OPUS_OK) {
    printf("Failed to create OPUS encoder");
    return;
  }

  if (opus_encoder_init(opus_encoder, SAMPLE_RATE, 1, OPUS_APPLICATION_VOIP) !=
      OPUS_OK) {
    printf("Failed to initialize OPUS encoder");
    return;
  }

  opus_encoder_ctl(opus_encoder, OPUS_SET_BITRATE(OPUS_ENCODER_BITRATE));
  opus_encoder_ctl(opus_encoder, OPUS_SET_COMPLEXITY(OPUS_ENCODER_COMPLEXITY));
  opus_encoder_ctl(opus_encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));

  read_buffer = (int16_t *)heap_caps_malloc(PCM_BUFFER_SIZE, MALLOC_CAP_DEFAULT);
  encoder_output_buffer = (uint8_t *)malloc(OPUS_BUFFER_SIZE);
}

void pipecat_send_audio(PeerConnection *peer_connection) {
  // TX must be kept fed every iteration to hold the shared I2S clock steady -- silence
  // on underrun rather than skipping the write.
  int16_t playback_frame[PCM_BUFFER_SIZE / sizeof(int16_t)] = {0};
  xQueueReceive(playback_queue, playback_frame, 0);
  pipecat_audio_hw_write(playback_frame, PCM_BUFFER_SIZE / sizeof(int16_t));

  // RX is always read too (full duplex, one persistent bus); the mic is still muted
  // towards the peer while the bot is speaking -- true barge-in is a later change.
  pipecat_audio_hw_read(read_buffer, PCM_BUFFER_SIZE / sizeof(int16_t));
  if (is_playing) {
    memset(read_buffer, 0, PCM_BUFFER_SIZE);
  }

  auto encoded_size = opus_encode(opus_encoder, (const opus_int16 *)read_buffer,
                                  PCM_BUFFER_SIZE / sizeof(uint16_t),
                                  encoder_output_buffer, OPUS_BUFFER_SIZE);
  peer_connection_send_audio(peer_connection, encoder_output_buffer,
                             encoded_size);
}
