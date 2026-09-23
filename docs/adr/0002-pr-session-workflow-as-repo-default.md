# Adopt /pr-session as the default workflow for every session in this repo

* Status: proposed
* Deciders:
* Date: 2026-09-23

Technical Story: Each Claude Code session in this repo started from scratch, with no committed
guarantee that it would interview requirements, review its own work, or honour the ADR convention
before opening a PR — the gap where miscommunication between the user and Claude Code leaks in.

## Context and Problem Statement

This repo is greenfield and headed for a Pipecat voice server plus ESP32-S3 firmware, where most
early PRs will make decisions that are costly to reverse. Whether a session grilled requirements,
reviewed its own diff, or wrote an ADR for a significant decision depended entirely on someone
remembering to ask for it. How should this repo guarantee a consistent workflow across sessions,
and does that guarantee itself rise to the level this repo's ADR convention is meant to capture?

## Decision Drivers

* The workflow needs to be the *default* for every session, not something invoked ad hoc, or it
  doesn't close the gap it's meant to close.
* `SessionStart` hooks can only inject context, not invoke commands or change settings — so no
  single mechanism can both enforce the workflow and guarantee it fires on every kind of session
  start (fresh, resumed, forked, compacted).
* Whatever is chosen becomes something every contributor (human or Claude Code) is expected to
  follow going forward; reversing it means unwinding an established habit, not just a config file.

## Considered Options

* Committed `.claude/settings.json` (plan mode + a `SessionStart` hook on `startup` only) driving a
  `/pr-session` command, with a `CLAUDE.md` pointer as the fallback for resumed/forked sessions the
  hook doesn't cover
* A `CLAUDE.md` pointer only, with the user invoking `/pr-session` manually each time
* A `SessionStart` hook with no matcher restriction, firing on every session-start variant
  (startup, resume, clear, compact, fork)

## Decision Outcome

Chosen option: "Committed `.claude/settings.json` (plan mode + a `SessionStart` hook on `startup`
only) driving a `/pr-session` command, with a `CLAUDE.md` pointer as the fallback", because it's
the only option that's both a genuine repo-wide default (not opt-in per session) and safe against
the hook re-firing "start from step 0" mid-implementation after a compaction. It cost more design
work than a plain pointer, but a pointer alone was rejected specifically because it was the weakest
enforcement of the three — exactly the failure mode this decision exists to close.

### Positive Consequences

* Every fresh session in this repo opens in plan mode and is pointed at the workflow without the
  user needing to type anything.
* The escape hatch (say so and confirm before skipping, for a trivial change) keeps the workflow
  from being a tax on one-line fixes.
* The workflow itself runs `/code-review` and `address-pr-comments` before any PR is marked ready,
  so review findings get an explicit disposition rather than being silently dropped.

### Negative Consequences

* Two places (the hook script and `CLAUDE.md`) state a version of the same directive, and can drift
  out of sync if edited independently — noted and accepted during this PR's own review as
  deliberate (they cover different session-start paths), not accidental duplication.
* A `SessionStart` hook injects only prose context; nothing prevents the running session from
  ignoring it. The guarantee is "the reminder always fires," not "the workflow cannot be skipped."

## Pros and Cons of the Options

### Committed settings.json + startup-only hook + CLAUDE.md fallback

* Good, because it's a genuine default — no session needs the user to remember to invoke anything
* Good, because restricting the hook to `startup` avoids re-asserting "begin at step 0" after a
  mid-implementation compaction, resume, or fork
* Good, because `CLAUDE.md` (loaded every session regardless of matcher) covers the sessions the
  hook doesn't
* Bad, because it's two files stating the same rule, which can drift

### CLAUDE.md pointer only

* Good, because it's the simplest possible option — one file, no hook, no settings change
* Bad, because it depends on the user (or a future session) reading and acting on it; nothing makes
  it the default the way a hook or `permissions.defaultMode` does
* Bad, because it was the exact failure mode already observed before this PR: a documented
  convention with no mechanism forcing it to be followed

### Unrestricted SessionStart hook (fires on every matcher)

* Good, because it never misses a session-start event of any kind
* Bad, because it re-injects "begin at step 0" after every resume, compaction, and fork —
  actively wrong advice in the middle of an in-progress implementation

## Links

* Established by [PR #2](https://github.com/ShotaTanemura/pipecat-m5stack-cores3-esp32s3-sts-robot/pull/2)
