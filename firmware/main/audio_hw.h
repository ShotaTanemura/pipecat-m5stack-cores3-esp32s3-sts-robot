// Direct I2S/codec driver for the CoreS3 ES7210 mic ADC and AW88298 speaker amp.
//
// Replaces M5Unified's M5.Mic / M5.Speaker for this project: those classes tear down and
// rebuild the shared I2S_NUM_1 peripheral on every begin()/end() (see the issue this fixes),
// which is audible as dropouts. This driver brings the bus up once, in full duplex, and
// never tears it down.
#pragma once

#include <cstddef>
#include <cstdint>

// Brings up I2S_NUM_1 in full-duplex std mode and powers/configures the ES7210 (mic) and
// AW88298 (speaker) codecs. Call once, after M5.begin().
void pipecat_audio_hw_init();

// Reads one mono 16 kHz frame from the mic (blocking on the I2S DMA). `out` must hold at
// least `samples` int16_t.
void pipecat_audio_hw_read(int16_t *out, size_t samples);

// Writes one mono 16 kHz frame to the speaker (blocking on the I2S DMA). `in` must hold
// at least `samples` int16_t.
void pipecat_audio_hw_write(const int16_t *in, size_t samples);
