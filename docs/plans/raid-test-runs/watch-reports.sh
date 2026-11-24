#!/usr/bin/env bash
# usage: watch-reports.sh <report-dir> <window-log> [count]
set -u
DIR=$1 LOG=$2 COUNT=${3:-0}
seen=""
while true; do
  ended=0
  grep -q "^# Window load" "$LOG" 2>/dev/null && ended=1
  for f in "$DIR"/*.md; do
    [ -e "$f" ] || continue
    case " $seen " in *" $f "*) ;; *) echo "report ready: $f"; seen="$seen $f";; esac
  done
  n=$(echo $seen | wc -w)
  [ "$COUNT" -gt 0 ] && [ "$n" -ge "$COUNT" ] && exit 0
  [ "$ended" = 1 ] && { echo "window ended with $n report(s)"; exit 0; }
  if grep -qE "Traceback|^refused" "$LOG" 2>/dev/null; then grep -E "Traceback|^refused" "$LOG" | head -2; exit 1; fi
  sleep 15
done
