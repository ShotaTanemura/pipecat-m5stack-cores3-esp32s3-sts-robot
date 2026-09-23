# Build the firmware with ESP-IDF (`idf.py`) directly

* Status: proposed
* Deciders:
* Date: 2026-09-23

Technical Story: Initializing `firmware/`, the ESP32-S3 (M5Stack CoreS3) device
client, from upstream `pipecat-ai/pipecat-esp32`.

## Context and Problem Statement

ADR-0004 chose PlatformIO as the firmware build driver, but explicitly deferred the
framework choice (Arduino vs. ESP-IDF) to whenever real firmware source landed --
`docs/adr/README.md` still lists "firmware framework (ESP-IDF vs. Arduino)" as an open
decision. That source has now landed: `pipecat-ai/pipecat-esp32`, the device client
this project is built on, is a pure ESP-IDF project (`idf.py`, `EXTRA_COMPONENT_DIRS`
reaching across sibling directories, submodule-backed components). How should this
repo's firmware actually be built, now that the framework is fixed by what we're
building on?

## Decision Drivers

* `firmware/main/` is seeded from `pipecat-esp32`'s `esp32-m5stack-cores3/src/`, and
  its `esp32-m5stack-cores3/CMakeLists.txt` depends on `EXTRA_COMPONENT_DIRS` pointing
  at `esp32-s3-box-3/components/{srtp,peer,esp-libopus}` in the same upstream repo --
  a CMake project structure PlatformIO's `platformio.ini`/library model has no clean
  way to express without re-deriving that structure by hand.
* Minimize divergence from upstream: `idf.py build`/`flash`/`monitor` is what
  upstream's own README, CI, and any future upstream sync already assume.
* ADR-0004 anticipated this moment ("Framework choice ... remains undecided and will
  get its own ADR when firmware source and `platformio.ini` land") -- no source has
  ever run through PlatformIO in this repo, so nothing is lost by not writing one.

## Considered Options

* Bare ESP-IDF (`idf.py build`, `idf.py flash`, `idf.py monitor`)
* PlatformIO CLI with `framework = espidf`, re-mapping upstream's layout into a
  `platformio.ini` project

## Decision Outcome

Chosen option: "Bare ESP-IDF", because it is what `pipecat-esp32` already is --
adopting it costs nothing but pinning the toolchain outside mise, while PlatformIO
would cost re-deriving upstream's cross-directory component layout by hand and
maintaining that translation against every future upstream sync.

This settles `docs/adr/README.md`'s "firmware framework (ESP-IDF vs. Arduino)"
question as ESP-IDF, and supersedes ADR-0004's PlatformIO choice.

### Positive Consequences

* `firmware/` builds and updates the same way upstream does -- no translation layer
  to keep in sync when `deps/pipecat-esp32` is bumped.
* `idf.py`'s own project model (`EXTRA_COMPONENT_DIRS`, `idf_component.yml`) is used
  as designed, rather than fought.

### Negative Consequences

* ESP-IDF cannot be pinned by `mise` -- there is no maintained mise/asdf/vfox plugin
  for it, and mise's registry no longer accepts new asdf plugin submissions. The
  toolchain is no longer fully reproducible from `mise install` alone;
  `firmware/README.md` documents the separate manual install instead.
* `pipx:platformio` is removed from `mise.toml`, so ADR-0003's toolchain-pinning
  scope shrinks to Python/uv only.

## Pros and Cons of the Options

### Bare ESP-IDF

* Good, because it's exactly what `pipecat-esp32` (the code this project is built on)
  already is -- zero translation.
* Good, because `idf.py`'s multi-project `EXTRA_COMPONENT_DIRS` model is a first-class
  feature, not a workaround.
* Bad, because it can't be pinned by `mise`, unlike the rest of this repo's toolchain.

### PlatformIO CLI

* Good, because it stays pinnable through mise's `pipx` backend (ADR-0003).
* Bad, because `pipecat-esp32`'s CoreS3 project has no components of its own -- it
  reaches into a sibling project's submodule-backed components -- and PlatformIO's
  project/library model has no direct equivalent, forcing a hand-maintained
  translation that upstream doesn't test or guarantee to stay compatible with.
* Bad, because it diverges from upstream's own build/CI instructions, making future
  upstream syncs harder to verify.

## Links

* Supersedes [ADR-0004](0004-platformio-as-firmware-build-driver.md)
* Related to [ADR-0007](0007-vendor-pipecat-esp32-as-a-submodule.md)
