# pipecat-m5stack-cores3-esp32s3-sts-robot

## Development setup

This repo pins its developer toolchain (Python and [uv](https://docs.astral.sh/uv/))
with [mise](https://mise.jdx.dev/).

1. [Install mise](https://mise.jdx.dev/getting-started.html).
2. From the repo root, trust and install the pinned tools:

   ```sh
   mise trust
   mise install
   ```

   `mise trust` is required once per clone — mise refuses to load an untrusted config,
   and skipping this step silently leaves the pinned tools unavailable.

This provides `python` and `uv` at the versions pinned in `mise.toml`, which is the
source of truth for this toolchain. The firmware's ESP-IDF toolchain is separate and
not managed by mise — see `firmware/README.md`.

## Agent server

`agent/` is the Pipecat voice-agent server the ESP32 device talks to over
SmallWebRTC. See `agent/README.md` for setup and how to run it:

```sh
cd agent/server
uv sync
uv run bot.py -t webrtc --esp32 --host <server LAN IP>
```

`--esp32` and a LAN-reachable `--host` (not `localhost`) are required — the device
firmware needs SDP munging that only applies in this mode, and it connects to the
server's LAN IP, not the server's own loopback address.

## Firmware

`firmware/` is the M5Stack CoreS3 (ESP32-S3) device client that connects to the agent
server above. It's built with ESP-IDF's `idf.py`, not mise. See `firmware/README.md`
for toolchain setup, configuration, and build/flash instructions.