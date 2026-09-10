# logs/

Raw Claude Code session transcripts for this assignment, one `*.jsonl` file
per session (filename = Claude Code session UUID). These are the unedited
machine logs required by the assignment (Section 11, "Transcript
submission"); the *annotated* AI-interaction narrative lives in
`../PROMPTLOG.md`, which is the graded deliverable.

## Provenance

Copied verbatim from `~/.claude/projects/<sanitized-repo-path>/`. Not
edited, reformatted, retyped, or hand-transcribed.

## How they stay current

`.claude/settings.json` registers a `Stop` hook that runs `logs/sync.sh`
after every Claude turn, so the committed copy tracks the live session as
the conversation grows. New sessions add new `.jsonl` files here on their
first turn.

The hook keeps the *files* fresh; it does not commit them. Before you
submit, run:

    ./logs/sync.sh
    git add logs .claude/settings.json
    git commit -m "M<n>: refresh session logs"

so the final transcripts are in the git history you hand in.
