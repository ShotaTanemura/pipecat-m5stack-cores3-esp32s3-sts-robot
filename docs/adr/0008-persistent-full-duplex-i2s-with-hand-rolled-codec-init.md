# Drive CoreS3 audio on one persistent full-duplex I2S bus, bypassing M5Unified's Mic/Speaker classes

* Status: proposed
* Deciders:
* Date: 2026-09-24

Technical Story: [#10](https://github.com/ShotaTanemura/pipecat-m5stack-cores3-esp32s3-sts-robot/issues/10)
— half-duplex mic/speaker switching causes audio dropouts and blocks voice barge-in.

## Context and Problem Statement

`firmware/main/media.cpp`'s `set_is_playing()` called `M5.Speaker.end()` / `M5.Mic.begin()`
(and back) on every speech/silence transition, tearing down and rebuilding the shared
I2S_NUM_1 peripheral each time. PR #9's hardware bring-up logged
`i2s_platform: i2s controller 1 has been occupied by i2s_driver` and
`i2s_common: GPIO 33/34 is not usable` right as playback started, audible as dropouts in the
bot's speech. How should CoreS3 drive its mic (ES7210) and speaker (AW88298), given they share
one I2S bus and one set of clock pins?

## Decision Drivers

* The dropouts are a direct, observed consequence of repeatedly bringing the I2S peripheral up
  and down — the fix has to stop doing that, not tune around it.
* Voice barge-in (the other half of #10) needs mic and speaker running at the same time, which
  M5Unified's `M5.Mic`/`M5.Speaker` classes don't support on this board: their `begin()`/`end()`
  calls install/uninstall the shared driver, so only one can be "on" at once.
* Minimize new dependencies and hand-rolled code where a maintained option exists.

## Considered Options

* Drive the ESP32-S3 I2S peripheral directly (`driver/i2s_std.h`) in one persistent full-duplex
  channel pair, with our own ES7210/AW88298 register init
* `espressif/esp_codec_dev` for the codec drivers
* Keep `M5.Mic`/`M5.Speaker` but stop calling `begin()`/`end()` on transitions (always-on)

## Decision Outcome

Chosen option: "Drive the ESP32-S3 I2S peripheral directly, with our own codec init", because
it's the only option that gives a single, persistent, full-duplex I2S channel pair — a
prerequisite for both fixing the dropouts and (in a following PR) real voice barge-in — while
reusing the exact register sequences already vendored in this project's own
`managed_components/m5stack__m5unified/src/M5Unified.inl` (`_microphone_enabled_cb_cores3`,
`_speaker_enabled_cb_cores3`), known-good on this exact board.

`esp_codec_dev` was ruled out: it has no AW88298 (speaker amp) driver, so the speaker half
would be hand-rolled either way, and mixing a maintained mic driver with a hand-rolled speaker
driver on one shared I2S bus adds complexity without removing any.

Keeping `M5.Mic`/`M5.Speaker` but never calling `end()` was ruled out: their internal I2S setup
is board-specific and not exposed as "give me a full-duplex pair" — each class independently
assumes it owns the whole bus when active, which is the same assumption that caused the bug.

New files `firmware/main/audio_hw.h`/`audio_hw.cpp` now own all hardware audio I/O. Verified by
`idf.py build` (clean, no warnings in the new/changed files, 13% flash headroom); full
verification is the hardware bring-up described in the PR (dropout log lines must not
reappear across several speak/listen turns).

### Positive Consequences

* The I2S peripheral is brought up once and never torn down — the dropout's direct cause is
  gone by construction, not by tuning.
* Mic and speaker can now run simultaneously, which is the prerequisite the next PR (echo
  cancellation + real barge-in) needs.
* No new external dependency.

### Negative Consequences

* This project now owns two vendor codec register sequences directly instead of going through
  M5Unified's API — a maintenance burden if M5Unified later changes CoreS3 pin assignments or
  fixes an errata in those sequences upstream.
* Unverified until the next hardware bring-up pass (this PR is build-verified only, per
  `.claude/commands/pr-session.md` step 4 — there is no host test harness for this firmware).

## Pros and Cons of the Options

### Direct `driver/i2s_std.h` + hand-rolled codec init

* Good, because it's the only option giving one persistent full-duplex channel pair.
* Good, because the register sequences are already vendored and known-good on this board.
* Bad, because it duplicates logic that a codec driver library would otherwise maintain.

### `esp_codec_dev`

* Good, because it's maintained upstream and would reduce hand-rolled code for the mic half.
* Bad, because it has no AW88298 driver — the speaker half is hand-rolled regardless, at which
  point the mixed maintained/hand-rolled split adds complexity for no real gain.

### Keep `M5.Mic`/`M5.Speaker`, stop toggling `begin()`/`end()`

* Good, because it would be the smallest possible diff.
* Bad, because these classes don't expose a "run both directions at once" mode on this board —
  the assumption that only one side is active at a time is baked into their internal I2S setup,
  not just in how #10's code called them.

## Links

* Related to `docs/adr/README.md`'s "Audio format and sample rate" example of an ADR-worthy
  decision.
* Precedes a follow-up ADR for acoustic echo cancellation (ESP-SR), which this decision's
  full-duplex bus is a prerequisite for.
