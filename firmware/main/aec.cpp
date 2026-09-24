#include "aec.h"

#include <cstring>

#include "esp_afe_aec.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "main.h"

// Filter length recommendation from esp_afe_aec.h's afe_aec_create() doc comment, for
// esp32s3.
#define AEC_FILTER_LENGTH 4

// Bounds the input/output accumulators used to re-chunk between our fixed 320-sample
// (20ms @ 16kHz) frame cadence and the AEC's own internal frame size (queried at runtime
// via afe_aec_get_chunksize() -- the two need not match). Comfortably larger than either
// side is ever expected to be.
#define AEC_ACCUM_CAPACITY_SAMPLES 1024

static afe_aec_handle_t *aec_handle = NULL;
static int aec_chunksize = 0;  // samples per channel, per afe_aec_get_chunksize()

// Scratch buffer for one AEC output chunk. afe_aec_process() requires outdata to be
// 16-bit-aligned (esp_afe_aec.h); sized and allocated once aec_chunksize is known.
static int16_t *chunk_out = NULL;

// Interleaved mic/ref pairs (input_format "MR"), samples-per-channel granularity.
static int16_t in_accum[AEC_ACCUM_CAPACITY_SAMPLES * 2];
static size_t in_accum_frames = 0;

// Echo-cancelled mono output, buffered until pipecat_aec_process() has enough to satisfy
// the caller's requested `samples`.
static int16_t out_accum[AEC_ACCUM_CAPACITY_SAMPLES];
static size_t out_accum_samples = 0;

void pipecat_aec_init() {
  aec_handle = afe_aec_create("MR", AEC_FILTER_LENGTH, AFE_TYPE_VC, AFE_MODE_HIGH_PERF);
  if (aec_handle == NULL) {
    ESP_LOGE(LOG_TAG, "Failed to create AEC instance");
    return;
  }
  aec_chunksize = afe_aec_get_chunksize(aec_handle);
  chunk_out = (int16_t *)heap_caps_aligned_alloc(16, aec_chunksize * sizeof(int16_t),
                                                 MALLOC_CAP_DEFAULT);
}

void pipecat_aec_process(const int16_t *mic_frame, const int16_t *ref_frame, int16_t *out,
                         size_t samples) {
  if (aec_handle == NULL) {
    memset(out, 0, samples * sizeof(int16_t));
    return;
  }

  // Accumulate this frame as interleaved mic/ref pairs.
  for (size_t i = 0; i < samples; i++) {
    in_accum[(in_accum_frames + i) * 2] = mic_frame[i];
    in_accum[(in_accum_frames + i) * 2 + 1] = ref_frame[i];
  }
  in_accum_frames += samples;

  // Drain full AEC-sized chunks out of the input accumulator into the output accumulator.
  while (in_accum_frames >= (size_t)aec_chunksize) {
    afe_aec_process(aec_handle, in_accum, chunk_out);

    memcpy(out_accum + out_accum_samples, chunk_out,
          aec_chunksize * sizeof(int16_t));
    out_accum_samples += aec_chunksize;

    size_t remaining_frames = in_accum_frames - aec_chunksize;
    memmove(in_accum, in_accum + aec_chunksize * 2,
           remaining_frames * 2 * sizeof(int16_t));
    in_accum_frames = remaining_frames;
  }

  if (out_accum_samples >= samples) {
    memcpy(out, out_accum, samples * sizeof(int16_t));
    size_t remaining_samples = out_accum_samples - samples;
    memmove(out_accum, out_accum + samples, remaining_samples * sizeof(int16_t));
    out_accum_samples = remaining_samples;
  } else {
    // Startup transient: not enough AEC output produced yet for this call.
    memset(out, 0, samples * sizeof(int16_t));
  }
}
