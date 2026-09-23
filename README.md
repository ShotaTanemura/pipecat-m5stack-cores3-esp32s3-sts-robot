# pipecat-m5stack-cores3-esp32s3-sts-robot

## Development setup

This repo pins its developer toolchain (Python, [uv](https://docs.astral.sh/uv/), and
the [PlatformIO](https://platformio.org/) CLI) with [mise](https://mise.jdx.dev/).

1. [Install mise](https://mise.jdx.dev/getting-started.html).
2. From the repo root, trust and install the pinned tools:

   ```sh
   mise trust
   mise install
   ```

   `mise trust` is required once per clone — mise refuses to load an untrusted config,
   and skipping this step silently leaves the pinned tools unavailable.

This provides `python`, `uv`, and `pio` at the versions pinned in `mise.toml`, which is
the source of truth for this toolchain.

## Agent server

`agent/` is the Pipecat voice-agent server the ESP32 device talks to over
SmallWebRTC. See `agent/README.md` for setup and how to run it (`cd agent/server &&
uv sync && uv run bot.py`).