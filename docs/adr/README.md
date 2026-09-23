# Architecture Decision Records

This directory records the architecture decisions for this project using the
[MADR](https://github.com/architecture-decision-record/architecture-decision-record) format.
Start from [`template.md`](template.md); see
[`0001-record-architecture-decisions.md`](0001-record-architecture-decisions.md) for a filled
example.

## When to write one

Write an ADR when a decision is **costly to reverse**, **cross-cutting**, or **externally
visible**. For this project that includes things like:

* Pipecat vs. a hand-rolled voice pipeline
* Speech-to-speech model and vendor choice
* On-device vs. cloud speech-to-text
* The ESP32-S3 ↔ server transport (e.g. WebSocket vs. WebRTC)
* Audio format and sample rate
* Firmware framework (ESP-IDF vs. Arduino)

Do **not** write one for library micro-choices, formatting, or anything a code comment already
explains.

## Numbering

Filenames are `NNNN-kebab-case-title.md`, zero-padded to four digits (`0001`, `0002`, …). The
next number is the highest existing number on `main` plus one.

Because this repo is worked on from multiple git worktrees, two branches can pick the same next
number concurrently. Whichever branch merges second renumbers its file (filename, the title
inside the file, and any inbound links) before merging.

## Status lifecycle

* `proposed` — drafted, not yet agreed
* `accepted` — the team has agreed to follow it
* `rejected` — considered and explicitly declined
* `deprecated` — no longer applies, and nothing has replaced it
* `superseded by [ADR-000Y](000Y-example.md)` — replaced by a later decision

## Superseding a decision

Never delete, renumber, or rewrite the reasoning in an existing record — a decision that turned
out wrong is still valuable history. To reverse one:

1. Write a new ADR describing the new decision. Under its `Links` section add
   `Supersedes [ADR-000X](000X-example.md)`.
2. In the old ADR, change only its `Status` line to
   `superseded by [ADR-000Y](000Y-example.md)`. Leave the rest of the file untouched.

## Authorship

Claude Code may create an ADR unprompted when a decision meets the trigger above, or on request
via `/adr`. It always drafts with `Status: proposed` and `Deciders` left blank — a human reviews
the reasoning and flips the status to `accepted` (or `rejected`).
