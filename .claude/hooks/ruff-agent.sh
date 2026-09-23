#!/usr/bin/env bash
# PostToolUse hook (agent/**/*.py only — filtered by the `if` rules in .claude/settings.json).
# Keeps agent/ clean with Ruff: format + autofix on every edited file, and fail loudly
# (exit 2, PostToolUse's contract for feeding a message back to Claude) on what's left.
set -euo pipefail

file_path=$(jq -r '.tool_input.file_path // empty')
[ -n "$file_path" ] || exit 0
[ -f "$file_path" ] || exit 0

ruff_bin="$CLAUDE_PROJECT_DIR/agent/server/.venv/bin/ruff"
if [ ! -x "$ruff_bin" ]; then
  if command -v ruff >/dev/null 2>&1; then
    ruff_bin="ruff"
  else
    echo "ruff-agent.sh: no ruff found (run 'uv sync' in agent/server); skipping" >&2
    exit 0
  fi
fi

cd "$CLAUDE_PROJECT_DIR/agent/server"

"$ruff_bin" format "$file_path"

if ! check_output=$("$ruff_bin" check --fix "$file_path" 2>&1); then
  echo "$check_output" >&2
  exit 2
fi
