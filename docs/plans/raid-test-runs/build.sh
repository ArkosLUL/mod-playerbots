#!/usr/bin/env bash
# usage: build.sh <out-dir> <label>
set -u
OUT=$1 LABEL=$2
ROOT=${RT_ROOT:-/g/DevStuff/GitHub/azerothcore-wotlk-pb}
BUILD=${RT_LOCKS:-G:/DevStuff/GitHub/.wave-loop/locks}/build
mkdir "$BUILD" || { echo "build lock held by: $(cat "$BUILD/owner")"; exit 1; }
echo "raid polish loop orchestrator: build ahead for $LABEL" > "$BUILD/owner"
cd "$ROOT" || exit 1
heads() { echo "$(git -C modules/mod-playerbots rev-parse HEAD) $(git -C modules/mod-dungeon-clear rev-parse HEAD) $(git rev-parse HEAD)"; }
BEFORE=$(heads)
F="-f docker-compose.yml -f docker-compose.override.yml -f .agents/plans/worldserver-build/compose.build.yml"
docker compose $F build --progress plain ac-worldserver > "$OUT/build-$LABEL.log" 2>&1
CODE=$?
AFTER=$(heads)
rm "$BUILD/owner" && rmdir "$BUILD" || exit 1
[ "$CODE" = 0 ] || { echo "build failed"; tail -20 "$OUT/build-$LABEL.log"; exit 1; }
[ "$BEFORE" = "$AFTER" ] || { echo "a tree moved during the build ($BEFORE -> $AFTER): build again"; exit 1; }
echo "$BEFORE" > "$OUT/build-$LABEL.head"
echo "build ok: $BEFORE"
