#!/bin/sh
# Copy this project's Claude Code session transcripts into logs/.
# Safe to re-run; overwrites the snapshots with the latest content.
# Invoked automatically by the Stop hook in .claude/settings.json, and
# runnable by hand before submission.
set -eu

dst="$(cd "$(dirname "$0")" && pwd)"
repo="$(cd "$dst/.." && pwd)"

# Claude Code stores transcripts under ~/.claude/projects/<sanitized-cwd>/,
# where the sanitized name is the absolute repo path with every "/" -> "-".
sanitized="$(printf '%s' "$repo" | sed 's:/:-:g')"
src="$HOME/.claude/projects/$sanitized"

# Fall back to a glob if the sanitization scheme ever changes.
if [ ! -d "$src" ]; then
	for cand in "$HOME/.claude/projects/"*rbtree-lab; do
		[ -d "$cand" ] && src="$cand" && break
	done
fi

if [ ! -d "$src" ]; then
	echo "sync.sh: no Claude project dir found for $repo" >&2
	exit 1
fi

count=0
for f in "$src"/*.jsonl; do
	[ -e "$f" ] || continue
	cp "$f" "$dst/"
	count=$((count + 1))
done

echo "sync.sh: copied $count transcript(s) from $src into $dst"
