# Use Pipecat + SmallWebRTC + OpenAI GPT-Live for the agent server

* Status: proposed
* Deciders:
* Date: 2026-09-23

Technical Story: Scaffolding a voice-agent server (`agent/`) that the ESP32-S3 device
client (`pipecat-esp32`) will talk to over WebRTC.

## Context and Problem Statement

`docs/adr/README.md` names both "Pipecat vs. hand-rolled pipeline", "STS model/vendor",
and "ESP32-S3↔server transport" as decisions this repo expects to record. Standing up
the first agent server scaffold forces all three at once: which framework runs the
server, which transport it exposes, and which speech-to-speech service answers the
device. How should the agent server be built, connected to, and driven?

## Decision Drivers

* `pipecat-esp32`'s `esp32-m5stack-cores3` firmware is fixed: it connects by POSTing an
  SDP offer to a `PIPECAT_SMALLWEBRTC_URL` (e.g. `http://host:7860/api/offer`) — there is
  no browser client and no other transport option on the device side.
* The user wants to start from OpenAI's GPT-Live model (`gpt-live-1`, the OpenAI Live
  API) as the speech-to-speech vendor, with the explicit intent to try others later.
* Minimize hand-rolled server code: prefer the officially maintained framework and its
  own scaffolding tool over writing a WebRTC/pipeline stack from scratch.

## Considered Options

* Framework: Pipecat (via `pipecat init`) vs. a hand-rolled WebRTC + STT/LLM/TTS stack
* Transport: SmallWebRTC vs. Daily vs. a telephony transport (Twilio/Telnyx)
* S2S vendor: OpenAI GPT-Live vs. OpenAI Realtime (API) vs. Gemini Live vs. AWS Nova Sonic

## Decision Outcome

Chosen options: **Pipecat**, scaffolded into `agent/` with the `pipecat` CLI; **SmallWebRTC**
as the transport; **OpenAI GPT-Live** (`OpenAILiveLLMService`) as the initial
speech-to-speech service.

Pipecat is the framework `pipecat-esp32` (the device client) is built to interoperate
with, and its CLI generates a runnable server rather than a blank skeleton — hand-rolling
the same WebRTC signaling and pipeline plumbing would be pure risk with no benefit.
SmallWebRTC isn't really a choice among alternatives: it's the only transport the fixed
device firmware speaks. GPT-Live is the user's explicit starting point, understood as
provisional — the realtime pipeline isolates the vendor to one service construction in
`agent/server/bot.py`, so swapping to Gemini Live, Nova Sonic, or OpenAI's older Realtime
API later is a small, local change, not a redesign.

**Known deviation from the CLI's own scaffold:** `pipecat init`'s `--realtime` registry
has no `openai_live` option (only `openai_realtime`, the older Realtime API). The server
was scaffolded with `openai_realtime` as a placeholder, then hand-swapped to
`OpenAILiveLLMService` in a follow-up commit. Re-running `pipecat init` in place would
regenerate the Realtime version and silently revert this decision — check this ADR before
re-scaffolding.

**ESP32 SDP munging is a run-time flag, not a code gap.** An earlier version of this ADR
claimed the CLI's generated `bot.py` needed a follow-up code change to enable
`SmallWebRTCRequestHandler`'s `esp32_mode`. That was wrong: Pipecat 1.11's development
runner (`pipecat.runner.run`) already exposes this as `--esp32`, which it wires straight
into `esp32_mode` when building the transport — no `bot.py` change needed. The runner also
refuses `--esp32` with `--host localhost`, since SDP munging needs a LAN-reachable address.
Launch with:

```sh
uv run bot.py -t webrtc --esp32 --host <server LAN IP>
```

### Positive Consequences

* `agent/server/bot.py` is a working, runnable bot from commit one, not a stub.
* The transport matches the device client with no adapter layer needed.
* Changing the S2S vendor later touches one service construction, not the pipeline shape.

### Negative Consequences

* The repo now carries a hand-applied deviation from what `pipecat init` would generate,
  which a careless re-scaffold could silently undo.
* GPT-Live full-duplex turn detection is untested against the ESP32 device's actual audio
  path (mic gain, VAD-free interruption handling) until the next PR's hardware pass.
* An actual handshake and end-to-end call with the physical device is unverified by this
  PR — the server was confirmed to start and serve `/api/offer` under `--esp32 --host
  <LAN IP>` (via curl), but not against real device firmware.

## Pros and Cons of the Options

### Pipecat (framework)

* Good, because `pipecat-esp32` is built against it — no protocol/adapter mismatch.
* Good, because `pipecat init` scaffolds a runnable bot, complete dependencies, and an
  agent-facing `AGENTS.md`/`CLAUDE.md` guide.
* Bad, because the CLI's service registry lags the framework's own service catalog (see
  the GPT-Live gap above), so its scaffolds aren't always current.

### Hand-rolled server

* Good, because it would have no framework version-lag risk.
* Bad, because it duplicates WebRTC signaling, pipeline sequencing, and turn-detection
  logic Pipecat already solves and `pipecat-esp32` already expects.

### SmallWebRTC (transport)

* Good, because it's exactly what `pipecat-esp32` speaks — a plain `POST /api/offer`.
* Good, because it needs no external service account (unlike Daily) for local development.
* Good, because its ESP32-specific SDP munging is a runner flag (`--esp32 --host <LAN IP>`),
  not code the server needs to carry.

### OpenAI GPT-Live (S2S vendor)

* Good, because it's the user's stated starting point.
* Good, because it's full-duplex with no local VAD/turn-strategy wiring required.
* Bad, because the CLI can't scaffold it directly, requiring a manual swap (documented
  above) that a future `pipecat init` re-run would silently revert.

### OpenAI Realtime (older API)

* Good, because it's what the CLI scaffolds out of the box, needing no manual swap.
* Bad, because it isn't the model the user wants to start from.

## Links

* Related to `docs/adr/README.md`'s "STS model/vendor" and "ESP32-S3↔server transport"
  examples of ADR-worthy decisions.
