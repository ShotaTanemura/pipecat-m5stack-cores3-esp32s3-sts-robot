---
description: Interview, plan, implement, review, and open a PR for one change
argument-hint: <what you want built or changed>
---

Run the full PR workflow for: $ARGUMENTS

Follow these steps precisely:

0. **Orient.** Read `CLAUDE.md`, `docs/adr/README.md`, and the records in `docs/adr/` so the
   interview never re-litigates a settled decision.

1. **Grill.** Invoke the `grilling` skill on `$ARGUMENTS`. If, during the interview, a candidate
   decision meets the significance test in `docs/adr/README.md` (costly to reverse, cross-cutting,
   or externally visible), name it to the user rather than settling it silently — it becomes an ADR
   in step 3. The plan you write must end with a concrete verification section: what to run, what
   to expect. Get approval via `ExitPlanMode`.

2. **Branch.** Refuse to proceed if `git status --short` is not empty — ask the user to commit or
   stash before continuing. Otherwise `git switch -c <type>/<slug>` from the current HEAD
   (normally `main`), where `<type>` is `feat`/`fix`/`chore`/`docs`/`refactor` matching the change
   and `<slug>` is a kebab-case rendering of the approved plan's title.

3. **Implement.** Work the plan. Commit as you go using Conventional Commits
   (`feat:`/`fix:`/`docs:`/`refactor:`/`chore:`), one commit per logical unit, each carrying this
   session's `Co-Authored-By` trailer. If step 1 flagged a significant decision, run `/adr` for it
   during this step so the record ships in the same PR as the code it justifies.

4. **Verify.** Discover whatever verification the repo has right now — `Makefile` targets,
   `pyproject.toml`/`uv`, `pytest`, `idf.py build`, `.github/workflows/*` — and run what applies.
   Also run the plan's own verification section from step 1. If nothing runnable is found, say so
   explicitly in your report rather than silently skipping this step. Keep the output; it goes in
   the PR body.

5. **Draft PR.** This step publishes the branch to GitHub — confirm with the user before the first
   push. Then:
   ```
   git push -u origin HEAD
   gh pr create --draft --title "<title>" --body "<body>"
   ```
   The PR body must include: a summary of the change, the approved plan from step 1 (lightly
   trimmed), and the verification results from step 4.

6. **Review.** Run `/code-review` against the draft PR just opened. `code-review` returns its
   findings directly to this session rather than posting to GitHub — `address-pr-comments` in
   step 7 only operates on PR comments, so post the findings as one PR comment yourself
   (`gh pr comment <number> --body "<findings, formatted>"`) before moving on. Skip this step
   entirely if `code-review` reports no findings.

7. **Address.** Run the `address-pr-comments` skill against the same PR. Every finding gets an
   explicit disposition (implement / decline / defer / discuss); anything touching design or scope
   is brought back to the user per that skill's triage flow, never auto-applied.

   Known limitation: findings arrive as a single PR-level comment, not inline review threads.
   `address-pr-comments` still replies to it correctly, but its thread-resolution step only
   applies to inline threads and stays inert here — expected, not a bug to chase.

8. **Ready.** Once every finding from step 7 has a disposition and any resulting fixes are pushed,
   run `gh pr ready`. Report the PR URL, a summary of what was built, what the review found, and how
   each finding was resolved. Stop — do not merge.

## Escape hatch

For a change that is trivial and obviously correct (a typo, a one-line config fix, a doc
correction), say so and propose skipping straight to implement → commit → PR instead of the full
workflow above, and get the user's confirmation before skipping. Never skip silently.
