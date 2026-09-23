# Use the PlatformIO CLI as the firmware build driver

* Status: proposed
* Deciders:
* Date: 2026-09-23

Technical Story: Pinning a developer toolchain (see [ADR-0003](0003-manage-developer-toolchain-with-mise.md))
requires deciding what actually builds the ESP32-S3 firmware, since that decision
determines which CLI gets pinned.

## Context and Problem Statement

`docs/adr/README.md` lists "firmware framework (ESP-IDF vs. Arduino)" as a decision
this repo expects to record. Pinning a build tool for the firmware side necessarily
takes a position on part of that space: whether the build is driven directly by
`idf.py` or through PlatformIO's CLI, which wraps ESP-IDF (or Arduino) with its own
project/dependency/build-target model. How should the ESP32-S3 firmware be built?

## Decision Drivers

* The pinned toolchain (ADR-0003) needs one CLI to actually invoke for firmware builds.
* This repo has no `platformio.ini` or firmware source yet, so the decision should not
  overreach into framework choice (Arduino vs. ESP-IDF), board configuration, or
  library selection — those belong to their own decision once real firmware work
  starts.

## Considered Options

* PlatformIO CLI (`pio run`, `pio run -t upload`, `pio device monitor`)
* Bare ESP-IDF (`idf.py build`, `idf.py flash`, `idf.py monitor`)

## Decision Outcome

Chosen option: "PlatformIO CLI", because it is installable and pinnable through mise's
`pipx` backend alongside the rest of the toolchain (ADR-0003), and it supports both
ESP-IDF and Arduino as underlying frameworks — so choosing it now does not foreclose
the still-open Arduino-vs-ESP-IDF decision.

This decision covers only the *build driver*. The firmware *framework*
(`framework = arduino` vs. `framework = espidf` in a future `platformio.ini`) remains
undecided and will get its own ADR when firmware source and `platformio.ini` land.

### Positive Consequences

* One CLI (`pio`), pinned in `mise.toml`, drives build/upload/monitor for the firmware.
* `.claude/commands/pr-session.md` step 4 can name `pio run` as a discoverable
  verification command once a `platformio.ini` exists.
* Framework choice (Arduino vs. ESP-IDF) stays open, since PlatformIO supports either.

### Negative Consequences

* Adds a layer (PlatformIO's project model) on top of the underlying framework's own
  tooling, which a contributor already fluent in raw `idf.py` needs to learn.

## Pros and Cons of the Options

### PlatformIO CLI

* Good, because it installs and pins cleanly through mise's `pipx` backend.
* Good, because it works with either ESP-IDF or Arduino as the framework, leaving that
  choice open.
* Good, because it standardizes build/upload/monitor commands across framework choices.
* Bad, because it's an additional abstraction over the framework's native tooling.

### Bare ESP-IDF

* Good, because it's the framework's own tooling, with no extra abstraction layer.
* Bad, because `idf.py` is installed via ESP-IDF's own installer script, not something
  mise can pin the same way as the rest of this repo's toolchain.
* Bad, because it forecloses Arduino as a framework option without that being an
  explicit decision.

## Links

* Related to [ADR-0003](0003-manage-developer-toolchain-with-mise.md)
