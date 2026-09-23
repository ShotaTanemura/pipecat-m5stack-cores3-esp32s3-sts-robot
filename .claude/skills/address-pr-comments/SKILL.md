---
name: address-pr-comments
description: |
  Critically evaluate and address review comments on an existing pull request. Fetches threads via GraphQL, presents each thread individually for structured triage, ensures every thread gets a reply, and automatically resolves appropriate threads. Use when user says "address comments", "fix review feedback", "respond to PR comments", or shares a PR URL with pending reviews.
version: 3.0.0
---

# address-pr-comments

Read and address review comments on an existing pull request with comprehensive reply tracking and thread resolution.

## When to Use

**Explicit triggers:**
- User shares a PR URL with review comments
- User says "address comments"
- User says "fix review feedback"
- User mentions "pending review"

**Do NOT trigger on:**
- Creating a new PR (use `/commit-push-pr` plugin command instead)
- General PR questions without actionable comments

## Workflow

1. **Understand PR context:**
   Before reading any comments, establish the PR's purpose and scope:
   ```bash
   PR_NUMBER="<number>"
   gh pr view "$PR_NUMBER" --json title,body,baseRefName,headRefName,number,url
   gh pr diff "$PR_NUMBER"
   ```
   - If the PR body references issues, read them for additional context
   - Identify: stated purpose, scope of changes, files touched
   - For specific file details during comment evaluation, use full diff only when needed

2. **Fetch review threads and comments via GraphQL:**
   Use GraphQL to get structured thread data with thread ID (the `id` field in `reviewThreads.nodes`) needed for resolution:
   
   ```bash
   # Extract repository owner and name from git remote (faster than API call)
   REMOTE_URL=$(git config --get remote.origin.url)
   REMOTE_REPO="${REMOTE_URL##*github.com[:/]}"
   REMOTE_REPO="${REMOTE_REPO%.git}"
   OWNER="${REMOTE_REPO%%/*}"
   REPO="${REMOTE_REPO#*/}"
   
   # Fetch review threads with pagination
   threads_cursor=""
   all_threads="[]"
   
   while true; do
     result=$(gh api graphql -f query='
     query($owner: String!, $repo: String!, $number: Int!, $cursor: String) {
       repository(owner: $owner, name: $repo) {
         pullRequest(number: $number) {
           reviewThreads(first: 100, after: $cursor) {
             pageInfo { hasNextPage endCursor }
             nodes {
               id
               isResolved
               path
               line
               comments(first: 50) {
                 pageInfo { hasNextPage endCursor }
                 nodes {
                   id
                   databaseId
                   author { login }
                   body
                   createdAt
                 }
               }
             }
           }
         }
       }
     }' -f owner="$OWNER" -f repo="$REPO" -F number=$PR_NUMBER ${threads_cursor:+-f cursor="$threads_cursor"})
     
     # Accumulate nodes into all_threads array
     all_threads=$(echo "$all_threads" "$result" | jq -s '.[0] + (.[1].data.repository.pullRequest.reviewThreads.nodes // [])')
     
     has_next=$(echo "$result" | jq -r '.data.repository.pullRequest.reviewThreads.pageInfo.hasNextPage')
     if [ "$has_next" = "true" ]; then
       threads_cursor=$(echo "$result" | jq -r '.data.repository.pullRequest.reviewThreads.pageInfo.endCursor')
     else
       break
     fi
   done
   
   # For threads with >50 comments, fetch additional pages
   # (Most threads have <50 comments; only paginate when needed)
   for thread in all_threads where comments.pageInfo.hasNextPage; do
     thread_comments_cursor="${thread.comments.pageInfo.endCursor}"
     while [ -n "$thread_comments_cursor" ]; do
       result=$(gh api graphql -f query='
       query($owner: String!, $repo: String!, $threadId: ID!, $cursor: String) {
         node(id: $threadId) {
           ... on PullRequestReviewThread {
             comments(first: 50, after: $cursor) {
               pageInfo { hasNextPage endCursor }
               nodes {
                 id databaseId author { login } body createdAt
               }
             }
           }
         }
       }' -f owner="$OWNER" -f repo="$REPO" -f threadId="${thread.id}" -f cursor="$thread_comments_cursor")
       
       # Append additional comments to thread
       # Update thread_comments_cursor if hasNextPage is true
     done
   done
   
   # Fetch PR-level comments with separate pagination
   comments_cursor=""
   all_pr_comments="[]"
   
   while true; do
     result=$(gh api graphql -f query='
     query($owner: String!, $repo: String!, $number: Int!, $cursor: String) {
       repository(owner: $owner, name: $repo) {
         pullRequest(number: $number) {
           comments(first: 100, after: $cursor) {
             pageInfo { hasNextPage endCursor }
             nodes {
               id
               databaseId
               author { login }
               body
               createdAt
             }
           }
         }
       }
     }' -f owner="$OWNER" -f repo="$REPO" -F number=$PR_NUMBER ${comments_cursor:+-f cursor="$comments_cursor"})
     
     # Accumulate nodes into all_pr_comments array
     all_pr_comments=$(echo "$all_pr_comments" "$result" | jq -s '.[0] + (.[1].data.repository.pullRequest.comments.nodes // [])')
     
     has_next=$(echo "$result" | jq -r '.data.repository.pullRequest.comments.pageInfo.hasNextPage')
     if [ "$has_next" = "true" ]; then
       comments_cursor=$(echo "$result" | jq -r '.data.repository.pullRequest.comments.pageInfo.endCursor')
     else
       break
     fi
   done
   ```
   
   **Note:** GraphQL provides thread ID (the `id` field from `reviewThreads.nodes`) needed for resolution. If GraphQL fails, skill falls back to REST API but cannot resolve threads.
   
   **Degraded mode fallback:**
   If GraphQL fails, fetch **both**:
   - Inline review comments: `gh api repos/$OWNER/$REPO/pulls/$PR_NUMBER/comments`
   - PR-level conversation comments: `gh api repos/$OWNER/$REPO/issues/$PR_NUMBER/comments`
   
   This preserves comment coverage from normal mode as closely as possible, but without GraphQL thread ID the skill cannot resolve review threads.
   Warn user: "GraphQL unavailable - thread resolution disabled. Replies will still be sent for both inline review comments and PR-level comments."

3. **Filter to unresolved threads:**
   Focus on threads where `isResolved: false`. Skip already-resolved threads unless they need additional response.
   
   **Default behavior:** Show only unresolved threads in triage UI.

3.5. **Generate recommendation for each thread:**
   
   Before presenting each thread, analyze using rule-based heuristics (apply in order, first match wins):
   
   **Rule 1: Explicit Scope Signal**
   - Comment contains "out of scope" / "separate PR" / "follow-up" → **Decline**
   - Comment contains "while you're here" / "also" → **Defer** (scope creep)
   
   **Rule 2: Specific vs. Vague**
   - Comment specifies exact action ("add null check", "rename X to Y", "extract method") → **Implement**
   - Comment is vague ("improve this", "reconsider approach") → **Discuss**
   
   **Rule 3: Question Detection**
   - Comment contains "?" or starts with "why/how/should/could" → **Discuss**
   - Note: Runs after Rule 2, so "Could you add X?" matches Rule 2 (specific action) first
   
   **Rule 4: Default Fallback**
   - If no rules matched → **Discuss** (safest default)
   
   For definitions of Implement/Decline/Defer/Discuss/Already Resolved, see "Comment Classification" section below.
   
   **Reasoning format:** "**[Classification]** - [Why]. [What to consider]."

4. **Present threads one-by-one for user decision:**
   Show each thread individually with full context:
   
   ````markdown
   ## Review Thread 1 of 5
   
   **Location:** src/auth.rs:42
   **Author:** @alice (2 hours ago)
   **Comment:**
   > Add null check here - this will crash if user is None
   
   **Code context:**
   ```rust
   40: fn validate_user(user: User) {
   41:     // Validation logic
   42:     println!("Validating {}", user.name);
   43:     if user.is_active {
   44:         return true;
   45:     }
   ```
   
   💡 **Recommendation: Implement** - Comment specifies concrete action ("add null check") on code in this PR's diff.
   
   **Classification options:**
   1. **Implement** - Fix in this PR ← Recommended
   2. **Decline** - Out of scope (provide reason)
   3. **Defer** - Valid but separate PR (provide reason)
   4. **Discuss** - Ask for clarification or explain
   5. **Already Resolved** - Fixed in commit <SHA>
   
   Your decision:
   ````
   
   **Code snippet configuration:**
   ```bash
   CONTEXT_LINES=5  # Lines before/after comment location
   MAX_SNIPPET_LINES=15  # Maximum total snippet size
   ```
   
   - Show ±$CONTEXT_LINES around the commented line from current HEAD
   - If path/line unavailable (outdated diff, deleted file): Show "[Code no longer at this location - may have been moved or deleted]"
   - If file not readable: Show "[Unable to read file - may need to fetch latest changes]"
   - Truncate with "..." if snippet exceeds $MAX_SNIPPET_LINES
   
   **Accept response formats:**
   - Simple: `1`
   - With reason: `2 - refactoring unrelated code`
   - With context: `4 - which validation cases should be covered?`
   - With SHA: `5 - fixed in abc1234`
   
   **Multi-comment threads:**
   If a thread has multiple comments (back-and-forth discussion), show the most recent comment but note: "This thread has 3 comments - will summarize all unaddressed points in reply."
   
   Repeat for each unresolved thread and PR-level comment.

5. **Build reply manifest:**
   Create tracking structure for all planned actions:
   
   ```json
   {
     "threads": [
       {
         "thread_id": "PRRT_kwDOAbc123",
         "comment_db_id": 123456789,
         "type": "inline",
         "location": "src/auth.rs:42",
         "author": "alice",
         "classification": "Implement",
         "commit_sha": null,
         "reply_body": null,
         "should_resolve": true,
         "status": {
           "implemented": false,
           "reply_sent": false,
           "resolved": false
         }
       }
     ],
     "pr_comments": [
       {
         "comment_id": "IC_kwDOGhi789",
         "comment_db_id": 123456791,
         "author": "carol",
         "classification": "Discuss",
         "reply_body": "Library X doesn't support our OAuth2 flow...",
         "ask_resolve": true,
         "status": {
           "reply_sent": false
         }
       }
     ]
   }
   ```
   
   **Manifest persistence:**
   - Store in memory during execution
   - Before sending replies, save snapshot:
     ```bash
     mkdir -p "$HOME/.cache/claude/address-pr-comments" && chmod 700 "$HOME/.cache/claude/address-pr-comments"
     MANIFEST_FILE="$HOME/.cache/claude/address-pr-comments/${PR_NUMBER}-$(date +%s).json"
     echo "$manifest_json" > "$MANIFEST_FILE"
     ```
   - If user requests retry after failures, load manifest and retry only failed items
   - Check for duplicate replies (idempotency): Fetch ALL existing replies ONCE before reply loop, cache in memory, check against cache
   - Delete only this run's manifest file after successful completion
   
   Show summary before proceeding:
   ```markdown
   ## Summary
   
   To implement (2):
   - src/auth.rs:42: Add null check
   - src/auth.rs:58: Add error handling
   
   To defer (1):
   - src/config.rs:15: Add validation (separate PR)
   
   To discuss (1):
   - PR comment: Explain library choice (will ask about resolution)
   
   Proceed with implementation? [yes/no]
   ```

6. **Implement approved changes:**
   Make the approved code changes, then create one or more commits using Conventional Commits format, batching related fixes together according to the commit strategy:
   ```bash
   git add <files>
   git commit -m "fix: address review feedback on <aspect>
   
   <what was changed and why>
   
   Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>"
   ```

7. **Push changes:**
   ```bash
   git push
   ```

8. **Send all replies per manifest:**
   Use the manifest to send tracked replies:
   
   ```bash
   # For each review thread (inline comments):
   jq -c '.threads[]' "$MANIFEST_FILE" | while IFS= read -r thread_json; do
       thread_id=$(echo "$thread_json" | jq -r '.thread_id')
       comment_db_id=$(echo "$thread_json" | jq -r '.comment_db_id')
       reply_body=$(echo "$thread_json" | jq -r '.reply_body')
       location=$(echo "$thread_json" | jq -r '.location')
       
       # Reply to inline comment
       gh api repos/$OWNER/$REPO/pulls/comments/$comment_db_id/replies \
           -X POST \
           -f body="$reply_body"
       
       # Mark as sent in manifest
       if [ $? -eq 0 ]; then
           jq --arg tid "$thread_id" '(.threads[] | select(.thread_id == $tid) | .status.reply_sent) = true' "$MANIFEST_FILE" > "$MANIFEST_FILE.tmp" && mv "$MANIFEST_FILE.tmp" "$MANIFEST_FILE"
           echo "✓ Replied to thread at $location"
       else
           echo "✗ Failed to reply to thread at $location"
       fi
   done
   
   # For each PR-level comment:
   jq -c '.pr_comments[]' "$MANIFEST_FILE" | while IFS= read -r comment_json; do
       comment_id=$(echo "$comment_json" | jq -r '.comment_id')
       author=$(echo "$comment_json" | jq -r '.author')
       reply_body=$(echo "$comment_json" | jq -r '.reply_body')
       
       # Reply as new PR comment (reference author)
       gh pr comment $PR_NUMBER \
           --body "Re: @${author}'s comment: $reply_body"
       
       # Mark as sent in manifest
       if [ $? -eq 0 ]; then
           jq --arg cid "$comment_id" '(.pr_comments[] | select(.comment_id == $cid) | .status.reply_sent) = true' "$MANIFEST_FILE" > "$MANIFEST_FILE.tmp" && mv "$MANIFEST_FILE.tmp" "$MANIFEST_FILE"
           echo "✓ Replied to @$author"
       else
           echo "✗ Failed to reply to @$author"
       fi
   done
   ```
   
   **Reply templates by classification:**
   
   **Implement:**
   ```
   Fixed in {commit_sha}
   
   Changes:
   - {specific change 1}
   - {specific change 2}
   
   {optional: Added tests / Considered edge cases}
   ```
   
   **Decline:**
   ```
   Thanks for the suggestion! I think this is out of scope for this PR because {specific reason}.
   
   This PR focuses on {stated goal}, and {suggested change} would {why out of scope: expand scope / touch unrelated systems / etc}.
   ```
   
   **Defer:**
   ```
   Great point! This deserves its own PR because {specific reason: complexity / separate testing needs / architectural implications}.
   
   I've {created issue #X / noted for follow-up} to track this. The current PR focuses on {stated goal}.
   ```
   
   **Discuss:**
   ```
   {direct answer to question / explanation of rationale}
   
   {technical reasoning for decision / trade-offs considered}
   ```
   
   **Already Resolved:**
   ```
   This was addressed in {commit_sha}
   
   {brief description of fix}
   ```
   
   **Idempotency check before sending:**
   Fetch ALL existing replies ONCE before the loop to avoid repeated API calls:
   ```bash
   # Determine the authenticated bot/account login once
   BOT_USER=$(gh api user --jq .login)
   
   # Fetch all existing replies once across every page (not per-comment)
   existing_replies=$(gh api --paginate repos/$OWNER/$REPO/pulls/$PR_NUMBER/comments | jq -s --arg bot_user "$BOT_USER" '[.[][] | select(.user.login == $bot_user and .in_reply_to_id != null) | {in_reply_to_id, body}]')
   
   # Then in the loop, check against cached data for the current thread:
   comment_db_id="${thread.comment_db_id}"
   location="${thread.location}"
   reply_exists=$(echo "$existing_replies" | jq -e --argjson comment_db_id "$comment_db_id" '.[] | select(.in_reply_to_id == $comment_db_id)' > /dev/null && echo "yes" || echo "no")
   
   if [ "$reply_exists" = "yes" ]; then
       echo "✓ Reply already exists for $location, skipping"
       # Mark as sent in manifest
       jq --arg tid "$thread_id" '(.threads[] | select(.thread_id == $tid) | .status.reply_sent) = true' "$MANIFEST_FILE" > "$MANIFEST_FILE.tmp" && mv "$MANIFEST_FILE.tmp" "$MANIFEST_FILE"
       continue
   fi
   ```

9. **Resolve appropriate threads:**
   After all replies sent, batch resolve threads using single GraphQL mutation:
   
   ```bash
   echo "Resolving addressed review threads..."
   
   # For Discuss threads, ask user confirmation first
   jq -c '.threads[] | select(.classification == "Discuss" and .should_resolve == true)' "$MANIFEST_FILE" | while IFS= read -r thread_json; do
       location=$(echo "$thread_json" | jq -r '.location')
       thread_id=$(echo "$thread_json" | jq -r '.thread_id')
       
       read -p "Should I resolve the Discuss thread at $location? [yes/no] " answer
       if [ "$answer" != "yes" ]; then
           jq --arg tid "$thread_id" '(.threads[] | select(.thread_id == $tid) | .should_resolve) = false' "$MANIFEST_FILE" > "$MANIFEST_FILE.tmp" && mv "$MANIFEST_FILE.tmp" "$MANIFEST_FILE"
       fi
   done
   
   # Build batch mutation for all threads to resolve
   mutation_body=""
   alias_counter=0
   declare -a thread_ids
   
   jq -c '.threads[] | select(.should_resolve == true)' "$MANIFEST_FILE" | while IFS= read -r thread_json; do
       thread_id=$(echo "$thread_json" | jq -r '.thread_id')
       thread_ids[$alias_counter]="$thread_id"
       mutation_body="${mutation_body}
       t${alias_counter}: resolveReviewThread(input: {threadId: \"${thread_id}\"}) {
         thread { id isResolved }
       }"
       ((alias_counter++))
   done
   
   # Execute batch mutation (all threads in one API call)
   if [ -n "$mutation_body" ]; then
       result=$(gh api graphql -f query="mutation { ${mutation_body} }")
       
       # Mark successful resolutions
       for i in "${!thread_ids[@]}"; do
           thread_id="${thread_ids[$i]}"
           is_resolved=$(echo "$result" | jq -r ".data.t${i}.thread.isResolved")
           location=$(jq -r --arg tid "$thread_id" '.threads[] | select(.thread_id == $tid) | .location' "$MANIFEST_FILE")
           
           if [ "$is_resolved" = "true" ]; then
               jq --arg tid "$thread_id" '(.threads[] | select(.thread_id == $tid) | .status.resolved) = true' "$MANIFEST_FILE" > "$MANIFEST_FILE.tmp" && mv "$MANIFEST_FILE.tmp" "$MANIFEST_FILE"
               echo "✓ Resolved thread at $location"
           else
               echo "✗ Failed to resolve thread at $location"
           fi
       done
   fi
   ```
   
   **Resolution policy:**
   - **Implement**: Resolve after push + reply (work done)
   - **Decline**: Do NOT resolve (let reviewer close if satisfied)
   - **Defer**: Do NOT resolve (tracks follow-up commitment)
   - **Discuss**: **Ask user** "Should I resolve this Discuss thread? [yes/no]". Only resolve on explicit confirmation.
   - **Already Resolved**: Resolve immediately (confirming prior fix)
   
   **Note:** Only inline review threads can be resolved. PR-level comments remain as part of the conversation history.

10. **Verify all comments addressed:**
    Check reply manifest for any failures:
    
    ```bash
    failed_replies=()
    failed_resolutions=()
    resolved_count=0
    
    jq -c '.threads[]' "$MANIFEST_FILE" | while IFS= read -r thread_json; do
        location=$(echo "$thread_json" | jq -r '.location')
        reply_sent=$(echo "$thread_json" | jq -r '.status.reply_sent')
        should_resolve=$(echo "$thread_json" | jq -r '.should_resolve')
        resolved=$(echo "$thread_json" | jq -r '.status.resolved')
        
        if [ "$reply_sent" != "true" ]; then
            failed_replies+=("Thread at $location")
        fi
        if [ "$should_resolve" = "true" ] && [ "$resolved" != "true" ]; then
            failed_resolutions+=("Thread at $location")
        elif [ "$resolved" = "true" ]; then
            ((resolved_count++))
        fi
    done
    
    jq -c '.pr_comments[]' "$MANIFEST_FILE" | while IFS= read -r comment_json; do
        author=$(echo "$comment_json" | jq -r '.author')
        reply_sent=$(echo "$comment_json" | jq -r '.status.reply_sent')
        
        if [ "$reply_sent" != "true" ]; then
            failed_replies+=("PR comment by @$author")
        fi
    done
    
    if [ ${#failed_replies[@]} -gt 0 ]; then
        echo "ERROR: Failed to reply to:"
        for item in "${failed_replies[@]}"; do
            echo "  - $item"
        done
        echo ""
        read -p "Retry failed replies? [yes/no] " retry
        if [ "$retry" = "yes" ]; then
            # Reload manifest and retry only failed items
            # Rerun step 8 with idempotency checks
        fi
    else
        echo "✓ All comments addressed"
        echo "✓ $resolved_count threads resolved"
        
        if [ ${#failed_resolutions[@]} -gt 0 ]; then
            echo ""
            echo "⚠ Warning: Some threads could not be resolved (replies were sent):"
            for item in "${failed_resolutions[@]}"; do
                echo "  - $item"
            done
        fi
        
        # Clean up only the manifest file created for this run
        if [ -n "${MANIFEST_FILE:-}" ] && [ -f "$MANIFEST_FILE" ]; then
            rm -f -- "$MANIFEST_FILE"
        fi
    fi
    ```

## Comment Classification

Classify each comment into one of 5 categories:

| Category | Meaning | Action | Resolve Thread? |
|----------|---------|--------|----------------|
| **Implement** | Concrete issue in code this PR introduced/modified | Fix in this PR | Yes (after fix) |
| **Decline** | Out of scope, preference-based, or incorrect | Reply explaining why | No (reviewer closes) |
| **Defer** | Valid but belongs in a separate PR | Reply suggesting follow-up | No (tracks commitment) |
| **Discuss** | Needs clarification or is a design question | Reply to discuss | **Ask user** (after reply) |
| **Already Resolved** | Fixed in a subsequent commit | Reply with commit SHA | Yes (confirm fix) |

**Examples:**

**Implement:**
- "This function should handle null values"
- "Add error handling here"
- "This breaks on edge case X"
- "Use a more descriptive variable name"

**Decline:**
- "Refactor this unrelated component while you're at it"
- Stylistic preference not backed by project conventions
- Suggestion based on misunderstanding the code

**Defer:**
- "Add comprehensive input validation" (when PR only fixes one validation bug)
- "Optimize this function's performance" (when PR is about correctness, not performance)
- Valid architectural improvement that would significantly expand the diff

**Discuss:**
- "Why did you choose approach X over Y?"
- "Should we consider Z for the future?"
- Vague or unclear feedback that needs clarification

**Already Resolved:**
- Check if the concern was addressed in a later commit
- Reply with the commit that fixed it

**Precedence rule** (when a comment fits multiple categories): Discuss > Decline/Defer > Implement. If unclear, classify as Discuss first — asking for clarification is always safer than implementing a misunderstood suggestion or prematurely declining valid feedback.

**Scope heuristics** (signals, not rules — override when judgment says otherwise):
- Comment targets code not in this PR's diff → likely out of scope
- Implementing would touch files not in the diff → likely separate-PR material
- Suggestion is stylistic with no project convention backing it → likely preference

## Recommendation Examples

- "Add null check here" → **Implement** (specific action)
- "While you're here, refactor auth module" → **Defer** (scope creep signal)
- "Why use library X?" → **Discuss** (question)
- "Nice work!" → **Discuss** (no clear action)

## Commit Strategy

**Group related fixes:**
If multiple comments point to the same underlying issue, group them into a single commit.

**Use appropriate commit type:**
- `fix`: Bug fixes identified in review
- `refactor`: Style or structure changes
- `docs`: Documentation improvements
- `test`: Test additions requested in review

**Example:**
```
fix: handle null values in user input validation

Review feedback identified that null inputs cause crashes.
Added null checks and tests for edge cases.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
```

## Error Handling

### GraphQL query fails

If GraphQL query for review threads fails:
- Fall back to REST API: `gh api repos/$OWNER/$REPO/pulls/$PR_NUMBER/comments`
- Warn user: "GraphQL unavailable - thread resolution disabled. Replies will still be sent."
- Continue with manifest tracking, reply sending, and verification
- Skip thread resolution step entirely

### Pagination needed

If `hasNextPage: true` for threads or comments:
- Automatically fetch additional pages using separate cursors
- Most PRs fit in first page (100 threads, 100 PR comments)
- Warn if hitting GitHub rate limits (5000/hour authenticated, 60/hour unauthenticated)

### Reply fails

If a reply fails to send:
- Track failure in manifest with error message
- Continue with other replies (don't abort)
- Show complete list of failures at end with exact locations
- Offer retry option with idempotency checks

### Resolution fails - permission denied

If thread resolution fails with permission error:
- Log error: "Missing permissions to resolve threads. Ensure 'write' access on repository."
- Skip remaining resolution attempts (likely all will fail)
- Replies have already been sent (primary goal achieved)

### Resolution fails - other errors

If thread resolution fails for other reasons:
- Non-critical (reply was successfully sent)
- Log warning with thread location
- Continue with other resolutions
- Report failures in verification step

### Thread already resolved (race condition)

If attempting to resolve a thread that's already resolved:
- Skip resolution mutation call
- Still send reply (adds context even if resolved)
- Log: "Thread at {location} already resolved by another user"
- Mark as success (goal achieved)

### Multi-comment threads

If a thread has multiple comments (back-and-forth discussion):
- Reply to the most recent comment (newest `createdAt` timestamp)
- Resolution resolves the entire thread, not individual comments
- If multiple comments have different questions, address all in one comprehensive reply
- Note in UI: "This thread has N comments - will summarize all unaddressed points"

### Force-push / outdated diff

If code location referenced in comment no longer exists:
- Show in UI: "[Code no longer at this location - may have been moved or deleted]"
- Still allow classification - user can choose:
  - **Decline**: "Code has been refactored"
  - **Already Resolved**: "Fixed in {recent commit}"
  - **Discuss**: Ask reviewer for clarification on new location

### Deleted comment

If a comment is deleted between fetch and reply:
- API call will return 404
- Log: "Comment at {location} no longer exists (may have been deleted)"
- Skip reply attempt
- Mark as complete in manifest
- Not counted as failure (comment doesn't need response)

### Duplicate retry (idempotency)

Before posting each reply, check for existing replies:
```bash
gh api repos/$OWNER/$REPO/pulls/$PR_NUMBER/comments \
    | jq ".[] | select(.in_reply_to_id == $COMMENT_ID and .user.login == \"$BOT_USER\")"
```

If reply already exists:
- Skip posting (idempotent operation)
- Mark as sent in manifest
- Log: "Reply already exists for {location}, skipping"
- Continue with next comment

### No comments

If the PR has no comments:
- Inform user: "No unresolved comments found on this PR"
- Check if review is still pending or if all threads are resolved
- Ask if there are other changes needed

### Comment on outdated diff

If a comment references code that has changed since the comment was posted:
- Check the commit history to see if it was already addressed
- Reply to the comment explaining what changed and when
- If not addressed, apply the feedback to the current code location
