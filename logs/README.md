# logs/

Raw Claude Code session transcripts for this assignment, one `*.jsonl` file
per session (filename = Claude Code session UUID). These are the unedited
machine logs; the *annotated* AI-interaction narrative lives in
`../PROMPTLOG.md`, which is the graded deliverable.

## Provenance

Copied verbatim from `~/.claude/projects/-Users-jayrecklez-rbtree-lab/`.

## Re-syncing before submission

Each `.jsonl` grows while its session is open, and new sessions add new
files. Re-run this before you submit so the committed logs are complete:

    ./logs/sync.sh

Then commit any changes under `logs/`.
