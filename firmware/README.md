# firmware

ESP32-S3 device client for the M5Stack CoreS3, connecting to the `agent/` Pipecat
server over WebRTC (SmallWebRTC) on the local WiFi network. This is
[`pipecat-ai/pipecat-esp32`](https://github.com/pipecat-ai/pipecat-esp32)'s
`esp32-m5stack-cores3` client, vendored as a submodule under `deps/pipecat-esp32/`
with our own editable app in `main/` (see `docs/adr/0007`). Attribution and license
are in `NOTICE`.

## Toolchain: ESP-IDF (not mise)

Unlike this repo's other tools, ESP-IDF is **not pinned by `mise`** -- it ships its
own installer and there is no mise/asdf/vfox plugin for it (see `docs/adr/0006`).
Install it separately, once, following
[Espressif's instructions](https://docs.espressif.com/projects/esp-idf/en/v5.4.1/esp32/get-started/linux-macos-setup.html):

```sh
git clone -b v5.4.1 --recursive https://github.com/espressif/esp-idf.git ~/esp/esp-idf
~/esp/esp-idf/install.sh esp32s3
```

v5.4.1 is what upstream's `dependencies.lock` was solved against. Every new shell,
load the ESP-IDF environment before running `idf.py`:

```sh
. ~/esp/esp-idf/export.sh
```

## First-time setup

From the repo root, clone the submodule if you haven't already:

```sh
git submodule update --init --recursive firmware/deps/pipecat-esp32
```

Copy `firmware/.env.example` to `firmware/.env` and fill in your WiFi credentials and
the agent server's LAN URL, then load it. In bash/zsh:

```sh
cd firmware
cp .env.example .env   # edit it, then:
set -a; . ./.env; set +a
```

In fish:

```fish
cd firmware
cp .env.example .env   # edit it, then:
for line in (grep -v '^#' .env | grep =)
    set -l parts (string split -m1 = $line)
    set -gx $parts[1] $parts[2]
end
```

## Build

```sh
cd firmware
idf.py set-target esp32s3
idf.py build
```

## Flash and monitor

```sh
idf.py -p <port> flash monitor
```

On macOS `idf.py flash` alone will usually find the port; on Linux it's typically
`/dev/ttyACM0` (`sudo dmesg` to confirm, and add your user to the `dialout` group to
avoid needing root).

## Gotchas

- **Env vars are read at CMake *configure* time**, not build time. If you change
  `WIFI_SSID`, `WIFI_PASSWORD`, or `PIPECAT_SMALLWEBRTC_URL` after a build, run
  `idf.py fullclean` (or delete `build/`) before rebuilding, or the old values stay
  baked into the binary.
- **No STUN/TURN.** The device and the agent server must be on the same LAN --
  `PIPECAT_SMALLWEBRTC_URL` must point at the server's LAN IP, not `localhost` or a
  hostname unreachable from the device's WiFi.
- **Credentials are compiled into the binary.** There is no runtime WiFi
  provisioning; anyone with the `.bin` can extract the WiFi password and server URL.
  Fine for development; revisit before shipping a device to anyone else.

## Known upstream limitations

Carried as-is from `pipecat-esp32`; fixing these is follow-up work, not part of this
scaffold:

- **Half-duplex audio.** CoreS3 shares one I2S bus between mic and speaker, so
  `media.cpp` stops the mic while the bot is speaking -- the device cannot be
  interrupted by voice.
- **Reconnect is a reboot.** Any WebRTC disconnect, or a malformed signaling
  response, calls `esp_restart()` rather than retrying in place.
- **The display only shows the boot banner.** `rtvi.cpp` never sends
  `client-ready`, and the RTVI screen callbacks in `rtvi_callbacks.cpp` are
  commented out upstream, so nothing renders once the app is running.
