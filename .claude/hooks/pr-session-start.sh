#!/usr/bin/env bash
# SessionStart hook (startup only — see .claude/settings.json matcher).
# Injects a pointer to the repo's PR workflow; cannot invoke it directly
# because SessionStart hooks can only add context, not run commands.
set -euo pipefail

DIRECTIVE='This repo uses the workflow in .claude/commands/pr-session.md. Unless the user'"'"'s request is a question, or a trivial and obviously-correct change, follow that workflow from step 0. If you judge the change trivial enough to skip the interview, say so and get the user'"'"'s confirmation before skipping — do not skip silently.'

jq -n --arg ctx "$DIRECTIVE" '{
  hookSpecificOutput: {
    hookEventName: "SessionStart",
    additionalContext: $ctx
  }
}'
