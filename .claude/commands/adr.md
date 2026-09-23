---
description: Draft an Architecture Decision Record for the given topic
argument-hint: <topic or decision>
---

Draft an Architecture Decision Record (ADR) for: $ARGUMENTS

Follow these steps:

1. Read `docs/adr/README.md` for the conventions (numbering, status lifecycle, superseding).
2. Read the existing files in `docs/adr/` so the new record neither duplicates nor silently
   contradicts a prior decision. If it reverses one, note that it supersedes it.
3. Determine the next number: the highest existing `NNNN` in `docs/adr/` plus one, zero-padded to
   four digits.
4. Copy `docs/adr/template.md` to `docs/adr/NNNN-kebab-case-title.md` and fill in Context and
   Problem Statement, Decision Drivers, Considered Options, Decision Outcome, and Consequences
   using the actual reasoning from this session — do not invent alternatives that weren't
   discussed. Drop optional sections that don't apply rather than leaving placeholder text.
5. Set `Status: proposed` and today's date. Leave `Deciders` blank — a human accepts the record.
6. If this ADR supersedes an existing one, add `Supersedes [ADR-000X](000X-example.md)` under
   `Links` in the new file, and update *only* the old file's `Status` line to
   `superseded by [ADR-000Y](000Y-example.md)`.
7. Do not run any git commands (no add/commit) — report the path you created and stop.
