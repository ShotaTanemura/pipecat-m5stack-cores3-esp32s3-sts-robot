// Acoustic echo cancellation for CoreS3's full-duplex audio path (ESP-SR's AEC-only AFE,
// espressif/esp-sr). CoreS3 has no hardware echo-reference channel (the ES7210 only wires
// up MIC1/MIC2 -- see docs/adr/0008), so the reference is fed here from the exact PCM
// frame the caller just wrote to the speaker.
#pragma once

#include <cstddef>
#include <cstdint>

// Creates the AEC instance. Call once, after the I2S bus is up (audio_hw.h).
void pipecat_aec_init();

// Runs echo cancellation for one 20ms (samples-sized) frame: `mic_frame` is what the mic
// just captured, `ref_frame` is the frame written to the speaker in the same iteration.
// Writes up to `samples` echo-cancelled samples to `out`. The AEC's own internal frame
// size need not match `samples` -- this re-chunks internally -- so a call can return
// zero-filled output for the first frame or two while enough audio accumulates.
void pipecat_aec_process(const int16_t *mic_frame, const int16_t *ref_frame, int16_t *out,
                        size_t samples);
