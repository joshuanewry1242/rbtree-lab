#!/bin/sh
# Copy the current Claude Code session transcripts for this project into logs/.
# Safe to re-run; overwrites the snapshots with the latest content.
set -eu

src="$HOME/.claude/projects/-Users-jayrecklez-rbtree-lab"
dst="$(cd "$(dirname "$0")" && pwd)"

if [ ! -d "$src" ]; then
	echo "no Claude project dir at $src" >&2
	exit 1
fi

count=0
for f in "$src"/*.jsonl; do
	[ -e "$f" ] || continue
	cp "$f" "$dst/"
	count=$((count + 1))
done

echo "synced $count transcript(s) into $dst"
