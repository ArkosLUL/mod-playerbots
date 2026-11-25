#!/usr/bin/env bash
# usage: window.sh <jobs.json> <head-sha> <out-dir> <label> [prebuilt]
set -u
JOBS=$1 HEAD=$2 OUT=$3 LABEL=$4 PREBUILT=${5:-}
ROOT=${RT_ROOT:-/g/DevStuff/GitHub/azerothcore-wotlk-pb}
LOCK=${RT_LOCKS:-G:/DevStuff/GitHub/.wave-loop/locks}/server
mkdir "$LOCK" || { echo "server lock held by: $(cat "$LOCK/owner")"; exit 1; }
cd "$ROOT" || exit 1
F="-f docker-compose.yml -f docker-compose.override.yml -f .agents/plans/worldserver-build/compose.build.yml"
if [ "$PREBUILT" = prebuilt ]; then
  echo "raid polish loop orchestrator: restart for $LABEL" > "$LOCK/owner"
  NOW="$(git -C modules/mod-playerbots rev-parse HEAD) $(git -C modules/mod-dungeon-clear rev-parse HEAD) $(git rev-parse HEAD)"
  BUILT=$(cat "$OUT/build-$LABEL.head" 2>/dev/null)
  WANT=$(git -C modules/mod-playerbots rev-parse "$HEAD" 2>/dev/null)
  if [ "$BUILT" != "$NOW" ] || [ "${BUILT%% *}" != "$WANT" ]; then
    echo "refused: build.sh built '$BUILT', the trees are at '$NOW' and the head is '$WANT'; run without prebuilt"
    rm "$LOCK/owner" && rmdir "$LOCK"
    exit 1
  fi
else
  echo "raid polish loop orchestrator: rebuild for $LABEL" > "$LOCK/owner"
  if ! docker compose $F build --progress plain ac-worldserver > "$OUT/build-$LABEL.log" 2>&1; then
    echo "build failed, lock kept"; tail -20 "$OUT/build-$LABEL.log"; exit 1
  fi
  echo "build ok"
fi
docker compose $F up -d --no-deps ac-worldserver 2>&1 | tail -2
sleep 5
until docker logs ac-worldserver 2>&1 | grep -q "worldserver-daemon) ready\|Segmentation\|core dumped"; do sleep 3; done
docker logs ac-worldserver 2>&1 | grep -E "ready|Segmentation|core dumped" | tail -1
rm "$LOCK/owner" && rmdir "$LOCK" || exit 1
cd "$ROOT/modules/mod-playerbots" || exit 1
python tools/raidtest/raidtest.py window --jobs "$JOBS" --head "$HEAD" --max-raids 30 --skip-rebuild \
  --report-dir "$OUT/$LABEL" --stop-file "$OUT/$LABEL.stop" --owner "raid polish loop orchestrator: $LABEL" \
  > "$OUT/$LABEL.out" 2>&1
echo "window exit $?"
