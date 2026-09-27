#!/usr/bin/env bash
# Wave procedure checks in [int], over the files changed since BASE. Logs in [loop]/logs/<log-name>.
# usage: int-check.sh <base-ref> <log-name>
set -u
export MSYS_NO_PATHCONV=1
INT=G:/DevStuff/GitHub/azerothcore-wotlk-pb/build-bis-wt/int
REFORGE=G:/DevStuff/GitHub/azerothcore-wotlk-pb/modules/mod-reforging
CHECK=~/.claude/scripts/pb-syntax-check.sh
BASE=$1
NAME=$2
LOGS=G:/DevStuff/GitHub/.wave-loop/pb-bis/logs/$NAME
mkdir -p "$LOGS"
cd "$INT" || exit 1
failed=0

changed_cpp=$(git diff --name-only --diff-filter=AM "$BASE" HEAD -- 'src/*.cpp' | tr '\n' ' ')
changed_h=$(git diff --name-only --diff-filter=AM "$BASE" HEAD -- 'src/*.h' | tr '\n' ' ')
changed_src=$(git diff --name-only --diff-filter=AM "$BASE" HEAD -- 'src/' | tr '\n' ' ')

echo "== syntax ($(echo $changed_cpp | wc -w) .cpp, $(echo $changed_h | wc -w) .h)"
: >"$LOGS/syntax.log"
# The fan-out cap keeps the first N TUs alphabetically and can cut the changed .cpp themselves,
# so those run uncapped and only the headers' includers are capped.
[ -n "$changed_cpp" ] && PB_REPO=$INT PB_MAX_FANOUT=999 $CHECK $changed_cpp >>"$LOGS/syntax.log" 2>&1
[ -n "$changed_h" ] && PB_REPO=$INT PB_MAX_FANOUT=60 $CHECK $changed_h >>"$LOGS/syntax.log" 2>&1
if [ -s "$LOGS/syntax.log" ]; then
	# Always fails here from the case-insensitive bind mount, never in the real build.
	real=$(grep '^FAIL' "$LOGS/syntax.log" | grep -v 'BuildSharedStrategyContexts.cpp')
	grep -cE '^PASS' "$LOGS/syntax.log" | sed 's/^/pass: /'
	[ -n "$real" ] && { echo "$real"; failed=1; }
fi

echo "== pblint"
if [ -n "$changed_src" ]; then
	python tools/pblint/pblint.py $changed_src >"$LOGS/pblint.log" 2>&1 || failed=1
	tail -15 "$LOGS/pblint.log"
fi

echo "== nativetest"
PB_SANITIZE=address,undefined sh tools/nativetest/run.sh >"$LOGS/nativetest.log" 2>&1 || failed=1
tail -5 "$LOGS/nativetest.log"

if [ -n "$(git -C "$REFORGE" status --porcelain -- src)" ]; then
	echo "== mod-reforging syntax"
	cpps=$(cd "$REFORGE" && ls src/*.cpp | tr '\n' ' ')
	PB_REPO=$REFORGE $CHECK $cpps >"$LOGS/reforge-syntax.log" 2>&1 || failed=1
	grep -E '^(PASS|FAIL)' "$LOGS/reforge-syntax.log"
fi

echo "== done: $([ $failed = 0 ] && echo green || echo RED)"
exit $failed
