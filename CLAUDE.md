# CLAUDE.md

## PR workflow

Changes go through the workflow in `.claude/commands/pr-session.md`. For a trivial,
obviously-correct change, say so and confirm before skipping it.

## Architecture Decision Records

Before proposing or changing architecture, read `docs/adr/` for prior decisions.

When a decision meets the significance test in `docs/adr/README.md` (costly to reverse,
cross-cutting, or externally visible), write a record with `Status: proposed` and leave
`Deciders` empty.

Conventions live in `docs/adr/README.md` — do not duplicate them here.
