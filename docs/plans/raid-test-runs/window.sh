#!/usr/bin/env bash
# usage: window.sh <jobs.json> <head-sha> <out-dir> <label>
# Takes the server lock, does the capped rebuild, restarts, waits for ready, releases the lock and runs
# the window; on a failed build it keeps the lock and stops.
set -u
JOBS=$1 HEAD=$2 OUT=$3 LABEL=$4
LOCK=G:/DevStuff/GitHub/.wave-loop/locks/server
mkdir "$LOCK" || { echo "server lock held by: $(cat "$LOCK/owner")"; exit 1; }
echo "raid polish loop orchestrator: rebuild for $LABEL" > "$LOCK/owner"
cd /g/DevStuff/GitHub/azerothcore-wotlk-pb || exit 1
F="-f docker-compose.yml -f docker-compose.override.yml -f .agents/plans/worldserver-build/compose.build.yml"
if ! docker compose $F build --progress plain ac-worldserver > "$OUT/build-$LABEL.log" 2>&1; then
  echo "build failed, lock kept"; tail -20 "$OUT/build-$LABEL.log"; exit 1
fi
echo "build ok"
docker compose $F up -d --no-deps ac-worldserver 2>&1 | tail -2
sleep 5
until docker logs ac-worldserver 2>&1 | grep -q "worldserver-daemon) ready\|Segmentation\|core dumped"; do sleep 3; done
docker logs ac-worldserver 2>&1 | grep -E "ready|Segmentation|core dumped" | tail -1
rm "$LOCK/owner" && rmdir "$LOCK" || exit 1
cd /g/DevStuff/GitHub/azerothcore-wotlk-pb/modules/mod-playerbots || exit 1
python tools/raidtest/raidtest.py window --jobs "$JOBS" --head "$HEAD" --max-raids 6 --skip-rebuild \
  --owner "raid polish loop orchestrator: $LABEL" > "$OUT/$LABEL.out" 2>&1
echo "window exit $?"
