# Use ESP-SR's AEC-only AFE, with a software-fed reference, for voice barge-in

* Status: proposed
* Deciders:
* Date: 2026-09-25

Technical Story: [#10](https://github.com/ShotaTanemura/pipecat-m5stack-cores3-esp32s3-sts-robot/issues/10)
— half-duplex mic/speaker switching blocks voice barge-in (second half, after
[ADR-0008](0008-persistent-full-duplex-i2s-with-hand-rolled-codec-init.md) fixed the
audio dropouts and made the I2S bus full duplex).

## Context and Problem Statement

With the bus now full duplex, the mic is still muted towards the peer while the bot speaks
(`docs/adr/0008`'s PR left that gating in place deliberately). `agent/server/bot.py` runs
`OpenAILiveLLMService`, whose entire design point is handling interruption itself — but a
simultaneous-duplex mic on CoreS3 hears the speaker's own output at close range (the mic and
speaker are centimetres apart on this board) and the bot would end up interrupting itself.
How should the mic signal be cleaned of the speaker's own output before it reaches the peer?

## Decision Drivers

* CoreS3 has no hardware echo-reference channel: `_microphone_enabled_cb_cores3`
  (`managed_components/m5stack__m5unified/src/M5Unified.inl:1053`, `MIC34_PDN = 0xFF`)
  powers down ES7210 MIC3/MIC4 and enables only MIC1/MIC2 — nothing loops the amp output
  back into the ADC. Any AEC reference has to come from software, not silicon.
* The audio task already runs mic read and speaker write in the same 20ms iteration
  (`docs/edr/0008`'s change), so the exact frame just written to the speaker is available
  as the reference with no extra synchronization needed.
* Minimize new dependencies; prefer a maintained library over a hand-rolled adaptive filter.

## Considered Options

* `espressif/esp-sr`'s AEC-only AFE (`esp_afe_aec.h`: `afe_aec_create`/`afe_aec_process`),
  fed a software reference
* Hand-rolled adaptive filter (e.g. NLMS) in this project
* Ship without AEC; keep the mic-mute gating from `docs/adr/0008` and defer barge-in

## Decision Outcome

Chosen option: "`espressif/esp-sr`'s AEC-only AFE, fed a software reference", because it's a
maintained, Espressif-tuned implementation built for exactly this shape of problem (`AFE_TYPE_VC`
is documented as "Voice communication scenarios, 16kHz input, including nonlinear noise
suppression"), and its dedicated `esp_afe_aec.h` API needs no WakeNet/MultiNet model files —
confirmed by building this PR: the linked `espressif__esp-sr` static library pulls in only
`model_path.c`/`esp_sr_debug.c`/`esp_mn_speech_commands.c`/`esp_process_sdkconfig.c`, no model
blobs, and the binary still fits the `factory` partition with 9% flash free (down from 13%
before this PR, per `idf.py build`'s size report).

New `firmware/main/aec.h`/`aec.cpp` own the AEC instance and re-chunk between our fixed
20ms/320-sample pipeline cadence and the AEC's own frame size (queried at runtime via
`afe_aec_get_chunksize()` rather than assumed, since the two need not match). The reference
is the exact frame `pipecat_send_audio()` just handed to `pipecat_audio_hw_write()` in the
same loop iteration — full alignment up to the fixed acoustic + amplifier delay, which the
adaptive filter is designed to absorb.

A hand-rolled filter was ruled out: it would take real audio-engineering effort to reach
what `esp-sr` already provides, for a project whose value is the S2S pipeline, not DSP.

Shipping without AEC was ruled out: it leaves the actual point of `OpenAILiveLLMService`
(self-managed interruption, per ADR-0005) permanently unreachable — the mic-mute floor this
decision removes was always meant as a stopgap, not the destination.

### Positive Consequences

* Real voice barge-in becomes possible: the mic-mute gating from `docs/adr/0008` is removed
  entirely, and the mic runs continuously.
* No model partition or extra flash storage is needed — this is signal processing, not a
  neural-net inference path, confirmed by what actually got linked.

### Negative Consequences

* **Unverified against real acoustic coupling.** This PR is build-verified only (`idf.py
  build`, clean, no new warnings). Whether the AEC actually suppresses CoreS3's short
  mic-to-speaker acoustic path well enough to prevent the bot self-interrupting is only
  answerable on hardware, and is the primary risk this decision carries forward.
* A new external dependency (`espressif/esp-sr`, pulling in transitive `espressif/cjson`,
  `dl_fft`, `esp-dl`, `esp_new_jpeg` per the component manager's dependency resolution),
  with real CPU cost on core 0 alongside the existing audio task and libpeer.
* If tuning the stock `AFE_TYPE_VC`/`AFE_MODE_HIGH_PERF`/`filter_length=4` configuration
  isn't enough on hardware, the next lever is capping output level (AW88298 register
  `0x0C`, `docs/adr/0008`) before reaching for different AEC parameters — noted here so a
  future bring-up pass doesn't have to rediscover it.

## Pros and Cons of the Options

### `espressif/esp-sr` AEC-only AFE

* Good, because it's maintained and tuned by the chip vendor for this exact target.
* Good, because the dedicated AEC-only entry point avoids pulling in WakeNet/MultiNet models.
* Good, because `AFE_TYPE_VC` is documented for exactly this scenario (16kHz voice
  communication with nonlinear noise suppression).
* Bad, because its effectiveness against this board's specific acoustic coupling is unproven
  until hardware bring-up.

### Hand-rolled adaptive filter

* Good, because it would have no new external dependency.
* Bad, because reaching production-usable echo cancellation is a substantial DSP undertaking,
  disproportionate to this project's actual scope.

### Ship without AEC (keep the PR before this one's mic-mute gating)

* Good, because it's zero additional risk and zero additional dependency.
* Bad, because it leaves voice barge-in — the reason `OpenAILiveLLMService` was chosen in
  ADR-0005 — permanently out of reach.

## Links

* Supersedes the mic-mute gating floor introduced in the PR for
  [ADR-0008](0008-persistent-full-duplex-i2s-with-hand-rolled-codec-init.md) (that PR's
  `pipecat_send_audio()` muted the mic while `is_playing`; this one removes that gating).
* Related to ADR-0005's noted risk: "GPT-Live full-duplex turn detection is untested against
  the ESP32 device's actual audio path."
