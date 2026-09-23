# Manage the developer toolchain with mise

* Status: proposed
* Deciders:
* Date: 2026-09-23

Technical Story: This repo is greenfield and heading toward two distinct toolchains — a
Python/uv-based Pipecat voice server and an ESP32-S3 firmware build — with no committed
version pins for either, so "clone and start working" has no defined meaning yet.

## Context and Problem Statement

Nothing in the repo pins Python, uv, or the PlatformIO CLI. Each contributor (human or
Claude Code session) would otherwise install these independently, at whatever version
happens to be on their machine. How should this repo pin and install its developer
toolchain so every clone resolves to the same versions?

## Decision Drivers

* The repo needs one version-pinning mechanism, not a different one per tool
  (pyenv for Python, a separate installer for PlatformIO, etc.).
* Firmware toolchains are where version drift is most expensive to debug, so pins need
  to be exact and checked in, not "whatever is latest."
* The repo will likely accumulate more tooling beyond Python and PlatformIO as the
  Pipecat server and firmware take shape; whatever is chosen should not need replacing
  when that happens.

## Considered Options

* mise, with all pinned tools declared in one checked-in `mise.toml`
* Per-tool managers: pyenv for Python, uv for its own version, a standalone PlatformIO
  installer
* uv alone (manages Python interpreters and can run arbitrary tools via `uv tool run`)
* Nix

## Decision Outcome

Chosen option: "mise, with all pinned tools declared in one checked-in `mise.toml`",
because it is the only option that pins Python, uv, and PlatformIO through a single
mechanism and a single file, and leaves room for whatever tooling this repo adds next
without introducing a second version manager.

### Positive Consequences

* One command (`mise install`) provisions the entire pinned toolchain for a fresh clone.
* Exact pins in `mise.toml` mean a toolchain bump is a reviewable diff, not silent drift.
* PlatformIO installs through mise's `pipx` backend into its own isolated environment,
  keeping its tightly-pinned dependencies out of the future Pipecat server's venv.

### Negative Consequences

* Contributors need mise installed as a prerequisite (documented in `README.md`).
* `mise.toml` shadows any globally-installed copies of these tools while working inside
  this repo, which can surprise a contributor who already has them installed elsewhere.

## Pros and Cons of the Options

### mise

* Good, because one file (`mise.toml`) pins every tool this repo needs, present and
  future.
* Good, because its `pipx` backend installs PlatformIO into an isolated environment
  without requiring `pipx` itself to be installed separately.
* Bad, because it is one more tool contributors must install before anything else works.

### Per-tool managers

* Good, because each manager is purpose-built for its one tool.
* Bad, because it means N tools to install and N different config files/conventions to
  keep in sync, with no shared story for future tooling.

### uv alone

* Good, because uv is already needed for the Python side, so this would avoid adding a
  second tool.
* Bad, because uv has no first-class way to pin or install PlatformIO or other
  non-Python tooling — it would still need a second mechanism for that.

### Nix

* Good, because it gives fully reproducible, hermetic environments.
* Bad, because it's substantially heavier (language, learning curve, build times) than
  this repo's current needs justify.

## Links

* Related to [ADR-0004](0004-platformio-as-firmware-build-driver.md)
