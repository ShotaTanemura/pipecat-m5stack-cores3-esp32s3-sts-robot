# Record architecture decisions

* Status: accepted
* Deciders: Shota Tanemura
* Date: 2026-09-23

Technical Story: Establish how architecture decisions for this project are captured, before any
firmware or service code is written.

## Context and Problem Statement

This project spans ESP32-S3 firmware (M5Stack CoreS3) and a Pipecat-based speech-to-speech
service. It will require a series of decisions that are expensive to reverse once code depends on
them — transport protocol, audio format, model/vendor choice, on-device vs. cloud processing,
firmware framework. Both a human developer and Claude Code need a durable place to record why
each choice was made, so later work (by either) doesn't have to guess at or accidentally
contradict prior reasoning.

## Decision Drivers

* The reasoning behind a decision (not just the decision itself) needs to survive past the
  session or PR that made it.
* Both a human developer and Claude Code should be able to read past decisions and write new ones.
* The format should be lightweight enough to actually get used on a small project.

## Considered Options

* MADR-style Architecture Decision Records in `docs/adr/`
* No formal record — rely on commit messages and code comments
* A wiki or Notion page outside the repository

## Decision Outcome

Chosen option: "MADR-style Architecture Decision Records in `docs/adr/`", because it keeps the
record next to the code it explains (versioned, reviewed in the same PRs, and readable offline),
and the MADR template is structured enough to force out alternatives and tradeoffs without being
heavyweight.

### Positive Consequences

* Decisions and their reasoning are searchable and versioned alongside the code.
* Claude Code can read `docs/adr/` before proposing architecture changes, reducing contradictory
  or repeated proposals.
* New contributors get a decision history instead of having to reverse-engineer intent from code.

### Negative Consequences

* Adds a small amount of process overhead per significant decision.
* Records can go stale if a decision changes without a corresponding superseding ADR (mitigated
  by the superseding convention in `docs/adr/README.md`).

## Pros and Cons of the Options

### MADR-style Architecture Decision Records in `docs/adr/`

* Good, because it lives in the repo and is reviewed alongside the code it affects
* Good, because the template's "Considered Options" / "Pros and Cons" structure forces
  alternatives to be written down, not just the final choice
* Good, because both humans and Claude Code can read and write plain Markdown files
* Bad, because it requires discipline to keep up to date

### No formal record

* Good, because it requires no extra process
* Bad, because reasoning gets scattered across commit messages, PR descriptions, and chat history,
  and is hard to find later
* Bad, because there is no single place for Claude Code to check before proposing something that
  contradicts a past decision

### A wiki or Notion page outside the repository

* Good, because it can have richer formatting and cross-linking than Markdown
* Bad, because it drifts out of sync with the code and isn't reviewed in the same PRs
* Bad, because it isn't visible to Claude Code working in the repository without extra tooling

## Links

* Reference: [MADR project](https://github.com/architecture-decision-record/architecture-decision-record)
