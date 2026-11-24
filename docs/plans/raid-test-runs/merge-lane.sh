#!/usr/bin/env bash
# usage: merge-lane.sh <branch> <subject> [repo]
set -u
BRANCH=$1 SUBJECT=$2
REPO=${3:-/g/DevStuff/GitHub/azerothcore-wotlk-pb/modules/mod-playerbots}
LOCK=G:/DevStuff/GitHub/.wave-loop/locks/server
if [ -f "$LOCK/owner" ] && grep -q "rebuild" "$LOCK/owner"; then
  echo "refused: a rebuild reads the source tree: $(cat "$LOCK/owner")"; exit 1
fi
cd "$REPO" || exit 1
COMMITS=$(git log --oneline "HEAD..$BRANCH") || exit 1
if [ -z "$COMMITS" ]; then echo "refused: $BRANCH has no commits ahead of $(git branch --show-current)"; exit 1; fi
echo "$COMMITS"
if ! TREE=$(git merge-tree --write-tree --name-only HEAD "$BRANCH"); then
  echo "refused: the merge conflicts in:"; echo "$TREE" | tail -n +2 | sed '/^$/,$d'; exit 1
fi
ADDED=$(git diff "HEAD...$BRANCH" -- src | grep -E '^\+\s*(//|/\*)')
if [ -n "$ADDED" ]; then echo "refused: the lane adds comment lines under src/:"; echo "$ADDED" | head -10; exit 1; fi
git merge --no-ff -q -m "Merge $BRANCH: $SUBJECT" "$BRANCH" || exit 1
git log -1 --format='%h %s'
git diff --stat HEAD^1 HEAD | tail -1
