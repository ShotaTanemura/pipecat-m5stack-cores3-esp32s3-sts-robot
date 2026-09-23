# Consume `pipecat-esp32` as a git submodule with our own app component

* Status: proposed
* Deciders:
* Date: 2026-09-23

Technical Story: Initializing `firmware/`, the ESP32-S3 (M5Stack CoreS3) device
client, from upstream `pipecat-ai/pipecat-esp32`.

## Context and Problem Statement

`pipecat-ai/pipecat-esp32` (MIT-licensed) already has a working `esp32-m5stack-cores3`
client, but its project has no components of its own -- its `CMakeLists.txt` reaches
into a sibling `esp32-s3-box-3/components/{srtp,peer,esp-libopus}` tree, which is
itself backed by git submodules (libpeer, srtp, esp-libopus, esp-protocols). Copying
only the CoreS3 subdirectory would not build. How should this heavy, C-level WebRTC
stack enter this repo, and where does robot-specific app code go?

## Decision Drivers

* Minimize hand-rolled WebRTC/SRTP/Opus code -- none of it is robot-specific, and
  reimplementing or hand-vendoring it is pure risk with no benefit (mirrors the
  reasoning in ADR-0005 for the server side).
* `firmware/main/` needs to be ours to edit as the robot grows (display, sensors,
  behavior) -- it can't live inside a vendored, unmodified upstream tree.
* Upstream should stay updatable without re-deriving a merge by hand each time.

## Considered Options

* Git submodule (upstream in full) + our own `firmware/main/` app component
* Git submodule, building upstream's `esp32-m5stack-cores3` project unmodified
* Full vendor copy (upstream's CoreS3 source plus the C components it depends on,
  no submodules)

## Decision Outcome

Chosen option: "Git submodule + our own app component" -- upstream is added whole as
a recursive submodule at `firmware/deps/pipecat-esp32/`, pinned at `e70e3b1`. Our own
`firmware/` is a separate ESP-IDF project whose `main/` component is seeded from
upstream's CoreS3 `src/` (with attribution headers and a `NOTICE` file) and whose
`EXTRA_COMPONENT_DIRS` points at the submodule's shared `srtp`/`peer`/`esp-libopus`
components, left unmodified. This gets upstream's WebRTC stack unmodified and
updatable via `git submodule update`, while leaving `firmware/main/` as ordinary,
ours-to-edit application code -- the natural place for anything robot-specific later.

Configuration stays as upstream's build-time environment variables
(`WIFI_SSID`, `WIFI_PASSWORD`, `PIPECAT_SMALLWEBRTC_URL`), documented in
`firmware/.env.example` and `firmware/README.md`. This is provisional: credentials
end up compiled into the binary, and changing them requires `idf.py fullclean`. Moving
to Kconfig-based configuration or runtime NVS provisioning is deferred to when it's
actually needed (e.g. flashing more than one device, or shipping to someone else) --
doing it now would be speculative for a scaffold with no hardware pass yet.

### Positive Consequences

* Upstream's WebRTC/SRTP/Opus stack is never hand-edited, so `git submodule update`
  can pull improvements or fixes with no merge conflicts on our side.
* `firmware/main/` is a normal, small ESP-IDF component -- easy to extend for
  robot-specific behavior without touching vendored code.
* The MIT attribution trail (per-file header comments, `firmware/NOTICE`) stays
  intact as the seeded files diverge from upstream over time.

### Negative Consequences

* The repo carries the full weight of upstream's submodule tree (libpeer, srtp,
  esp-libopus, esp-protocols -- the last unused by our `esp32s3` target but pulled in
  regardless, since it's upstream's own submodule set), not just the CoreS3 subset we
  need.
* `firmware/main/`'s copies of upstream's CoreS3 sources will drift from
  `deps/pipecat-esp32/esp32-m5stack-cores3/src/` as we edit them -- future upstream
  fixes to those specific files must be re-applied by hand, not picked up by
  `git submodule update`.
* Build-time env-var configuration means no NVS-based runtime provisioning yet --
  every WiFi network change needs a rebuild.

## Pros and Cons of the Options

### Git submodule + our own app component

* Good, because upstream's heavy C components stay unmodified and updatable.
* Good, because robot-specific code has an obvious, unencumbered home.
* Bad, because the two CoreS3 source trees (upstream's and ours) will diverge with no
  automatic way to reconcile them.

### Git submodule, upstream's project unmodified

* Good, because it is the smallest possible first step -- no `firmware/main/` to
  maintain at all.
* Bad, because there is nowhere to put robot-specific code without forking upstream's
  own `esp32-m5stack-cores3/` directory later, which is a strictly worse version of
  this decision deferred, not avoided.

### Full vendor copy

* Good, because the repo is fully self-contained -- no submodule init step, no
  network dependency on GitHub at clone time.
* Bad, because it drags in libpeer/srtp/esp-libopus sources we would then own the
  maintenance of, despite never intending to modify them.
* Bad, because upstream fixes become manual copy-paste merges indefinitely.

## Links

* Related to [ADR-0006](0006-build-firmware-with-esp-idf-directly.md)
* Related to [ADR-0005](0005-pipecat-agent-server-smallwebrtc-and-openai-gpt-live.md)
