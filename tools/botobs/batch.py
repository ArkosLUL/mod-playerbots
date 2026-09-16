#!/usr/bin/env python3
"""Read the whole trace corpus at once, rather than one pull at a time.

Strategy decisions here have been taken on one to five pulls, because reading a sixth meant another
run of another tool: twelve Lightning Charge shelters were solved against three Thorim pulls, and two
later pulls pushed four of the twelve rows outside their own cone margin. There are 125 traces on
disk. This makes the corpus the unit of analysis instead of the pull.

    batch.py                        every trace under the default log dir
    batch.py --boss thorim          one boss
    batch.py --boss thorim --valid  only the pulls that count as evidence
    batch.py --census               validity and outcome totals, no per-trace rows
    batch.py --probes               probe keys declared in source that never reach a trace
    batch.py --verify               roll up the invariant checks across the selection
    batch.py --boss X --split-at R  did the change help: pulls built before R against after

Retention is 7 days and 5 GB (Obs.RetentionDays, Obs.MaxDirMB), so a baseline worth keeping belongs
outside the log dir - pass its directory as an extra positional and it joins the selection.
"""
from __future__ import annotations

import argparse
import pathlib
import sys
from collections import Counter

from raidobs.corpus import find_traces, pull_time
from raidobs.encounter import encounter_of
from raidobs.metrics import Side, show_compare, trace_metrics
from raidobs.paths import LOG_ROOT, REPO
from raidobs.probes import silent_keys
from raidobs.trace import Trace, combat_deaths
from raidobs.validity import ON_ASK, decidable_kinds, inspect, resolve_since
from raidobs.verify import verify_checks

# One letter per disqualifier for the row table. Not the first letter of the kind: hardmode-off and
# human-in-raid would collide, and those two are the pair most worth telling apart.
FLAG = {"stale-build": "S", "hardmode-off": "M", "human-role": "R", "human-in-raid": "H"}

# --valid drops what validity.decidable_kinds names; --strict drops any human at all. A human who held tank
# or heal is only decidable on traces written after the recorder learned to read a human's talent tab,
# since older ones record every human's role as the literal "human".


def decidable(row: dict, decisive: set[str]) -> list[tuple[str, str]]:
    return [w for w in row["warnings"] if w[0] in decisive]


def row_for(trace: Trace, ref, with_metrics: bool = False) -> dict:
    """One trace reduced to the handful of facts a corpus view needs, so the Trace can be dropped.

    Metrics cost roughly another second on a 28 MB trace, so they are only assembled when something
    is going to compare them."""
    facts, warnings = inspect(trace, ref)
    ends = trace.of("end")
    snaps = trace.of("snap")
    checks = verify_checks(trace)
    return {
        "name": trace.path.name,
        "boss": encounter_of(trace),
        "v": trace.header.get("v"),
        "outcome": ends[-1].get("out") if ends else "cut short",
        "ms": snaps[-1]["t"] if snaps else 0,
        "roster": len(trace.header.get("roster", [])),
        "humans": len(facts["humans"]),
        "hardmode": facts["hardmode"],
        "warnings": warnings,
        "failed": [label for label, count, _ in checks if count],
        "checks": len(checks),
        "deaths": len(combat_deaths(trace)),
        "truncated": trace.truncated,
        "keys": Counter(rec.get("k", "") for rec in trace.of("note")),
        "built": facts["built"],
        "pulled": pull_time(trace.path),
        "metrics": trace_metrics(trace) if with_metrics else None,
    }


def show_rows(rows: list[dict]) -> None:
    width = max((len(r["name"]) for r in rows), default=10)
    print(f"  {'trace':<{width}}  {'boss':<22} {'out':<10} {'mins':>5} {'raid':>4} "
          f"{'dead':>4} {'bad':>3} {'fail':>4}")
    for row in rows:
        flags = "".join(FLAG[kind] for kind, _ in row["warnings"])
        print(f"  {row['name']:<{width}}  {row['boss']:<22} {row['outcome']:<10} "
              f"{row['ms'] / 60000:5.1f} {row['roster']:4d} {row['deaths']:4d} "
              f"{flags or '-':>3} {len(row['failed']) or '-':>4}")
    print("\n  bad: S stale build, M hard mode off, H human in the raid")


def show_census(rows: list[dict], decisive: set[str]) -> None:
    total = len(rows)
    clean = sum(1 for r in rows if not decidable(r, decisive))
    print(f"\nvalidity   {clean}/{total} traces survive the checks that can be decided")
    reasons = Counter(kind for row in rows for kind, _ in row["warnings"])
    for kind, count in reasons.most_common():
        note = "" if kind in decisive else (f"   (pass {ON_ASK[kind]} to decide)"
                                            if kind in ON_ASK else "   (informational)")
        print(f"           {count:4d}  {kind}{note}")

    # Three of the four disqualifiers live in hdr.bin and hdr.cfg, which arrived in v11. Without
    # saying so, a corpus of older traces reads as one where nothing is wrong.
    dark = sum(1 for r in rows if r["v"] is None or r["v"] < 11)
    if dark:
        print(f"           {dark:4d}  predate v11: build and hard mode unknown, not verified")

    print("\noutcome")
    for outcome, count in Counter(r["outcome"] for r in rows).most_common():
        print(f"           {count:4d}  {outcome}")

    per_boss = Counter(r["boss"] for r in rows)
    valid_boss = Counter(r["boss"] for r in rows if not decidable(r, decisive))
    print("\nper boss   traces / of which valid")
    for boss, count in per_boss.most_common():
        print(f"           {count:4d} / {valid_boss[boss]:<4d} {boss}")

    cut = sum(1 for r in rows if r["truncated"])
    if cut:
        print(f"\n           {cut} trace(s) hit the size cap and stopped early")


def show_verify_rollup(rows: list[dict]) -> None:
    failures = Counter(label for row in rows for label in row["failed"])
    clean = sum(1 for r in rows if not r["failed"])
    print(f"\nverify     {clean}/{len(rows)} traces pass all {rows[0]['checks']} checks")
    for label, count in failures.most_common():
        print(f"           {count:4d}  {label}")


def show_probes(rows: list[dict]) -> None:
    """Probe keys the source declares that never once reached a trace of their own boss.

    Over one pull a silent key usually means the thing did not happen. Over every pull of a boss it
    means the recorder is dropping it, and nothing else says so: `fl.frozen` is written each pass,
    keyed on a vehicle guid, and NoteAssignment resolves its key with FindPlayer and returns early
    when it is not a player - 7 Flame Leviathan pulls, 1,556 rows across its six sibling keys, zero
    for that one.
    """
    emitted: Counter = Counter()
    seen_per_boss: dict[str, set[str]] = {}
    for row in rows:
        emitted.update(row["keys"])
        seen_per_boss.setdefault(row["boss"], set()).update(row["keys"])
    emitted.pop("", None)

    print(f"\nprobes     {len(emitted)} key(s) emitted across {len(rows)} trace(s)")
    for key, count in emitted.most_common(10):
        print(f"           {count:7d}  {key}")
    if len(emitted) > 10:
        print(f"           ... {len(emitted) - 10} more")

    mute: dict[str, list[str]] = {}
    for boss, keys in sorted(seen_per_boss.items()):
        for key, _, where in silent_keys(keys, boss):
            mute.setdefault(boss, []).append(f"{key} ({where})")
    if not mute:
        print("\n           every key declared for a boss in this selection reached a trace")
        return
    total = sum(len(v) for v in mute.values())
    print(f"\n           {total} key(s) declared and never emitted in any pull of that boss:")
    for boss, keys in mute.items():
        for entry in keys:
            print(f"           {boss:<18} {entry}")


def read_rows(paths: list[pathlib.Path], ref, with_metrics: bool) -> list[dict]:
    # Reading the corpus takes minutes, so say where it is - but only to a terminal, since the
    # carriage returns turn a redirected run into one long line.
    progress = sys.stderr.isatty()
    rows = []
    for index, path in enumerate(paths, 1):
        if progress:
            print(f"\rreading {index}/{len(paths)} {path.name[:48]:<48}", end="", file=sys.stderr)
        rows.append(row_for(Trace(path), ref, with_metrics))
    if progress:
        print("\r" + " " * 60 + "\r", end="", file=sys.stderr)
    return rows


def main() -> int:
    parser = argparse.ArgumentParser(description="Read the RaidObs trace corpus.")
    parser.add_argument("roots", nargs="*", type=pathlib.Path, default=None,
                        help=f"trace files or directories (default: {LOG_ROOT})")
    parser.add_argument("--boss", help="only traces whose slug matches, e.g. thorim")
    parser.add_argument("--since", metavar="REF",
                        help="commit-ish or ISO time (local without an offset) the build must be "
                             "newer than; naming one makes a "
                             "stale build disqualify (without it, builds are compared to HEAD for "
                             "information only)")
    parser.add_argument("--valid", action="store_true",
                        help="drop traces where a human tanked or healed, plus a stale build with "
                             "--since and hard mode off with --hardmode")
    parser.add_argument("--strict", action="store_true",
                        help="--valid, and also drop any trace with a human in the raid")
    parser.add_argument("--census", action="store_true", help="totals only, no per-trace rows")
    parser.add_argument("--verify", action="store_true", help="roll up the invariant checks")
    parser.add_argument("--probes", action="store_true",
                        help="probe keys declared in source that never reach a trace")
    parser.add_argument("--split-at", metavar="REF",
                        help="compare pulls built before REF against pulls built after it; REF reads "
                             "like --since")
    parser.add_argument("--baseline", metavar="DIR", type=pathlib.Path,
                        help="compare the selection against the traces kept in DIR")
    parser.add_argument("--limit", type=int, help="stop after N traces, newest first")
    parser.add_argument("--hardmode", action="store_true",
                        help="the pulls were meant to be hard mode, so hard mode off disqualifies")
    args = parser.parse_args()

    ref = resolve_since(REPO, args.since)
    if args.since and not ref:
        print(f"cannot resolve --since {args.since} to a commit or a time", file=sys.stderr)
        return 1

    roots = args.roots or [LOG_ROOT]
    paths = find_traces(roots, args.boss)
    if args.limit:
        paths = paths[: args.limit]
    if not paths:
        where = args.boss and f" for boss {args.boss}" or ""
        print(f"no traces{where} under {', '.join(str(r) for r in roots)}", file=sys.stderr)
        return 1

    decisive = decidable_kinds(args.since, args.hardmode)
    comparing = bool(args.split_at or args.baseline)

    split = None
    if args.split_at:
        split = resolve_since(REPO, args.split_at)
        if not split:
            print(f"cannot resolve {args.split_at} to a commit", file=sys.stderr)
            return 1

    baseline_rows: list[dict] = []
    if args.baseline:
        for path in find_traces([args.baseline], args.boss):
            baseline_rows.append(row_for(Trace(path), ref, with_metrics=True))
        if not baseline_rows:
            print(f"no traces under {args.baseline}", file=sys.stderr)
            return 1

    rows = read_rows(paths, ref, comparing)

    if args.valid or args.strict:
        rows = [r for r in rows
                if not (r["warnings"] if args.strict else decidable(r, decisive))]
        if not rows:
            print("nothing in the selection counts as evidence", file=sys.stderr)
            return 1

    label = f"{len(rows)} trace(s)" + (f", boss {args.boss}" if args.boss else "")
    print(f"{label}, build compared against {ref[0] if ref else 'nothing'}")

    if not args.census:
        show_rows(rows)
    show_census(rows, decisive)
    if args.verify:
        show_verify_rollup(rows)
    if args.probes:
        show_probes(rows)

    if args.baseline:
        return show_compare(Side(str(args.baseline), baseline_rows), Side("selection", rows))
    if split:
        sha, when = split
        # hdr.bin says what the binary was and is the honest split, but it only arrived in v11 and
        # most traces on disk predate it. Falling back to when the pull was recorded answers the same
        # question one assumption weaker: that the build happened before the pull.
        stamped = [r for r in rows if r["built"]]
        basis = "built" if len(stamped) == len(rows) else "pulled"
        if basis == "pulled":
            print(f"\n{len(rows) - len(stamped)} of {len(rows)} trace(s) carry no build stamp, so the"
                  f" split is on when the pull was recorded")
        dated = [r for r in rows if r[basis]]
        return show_compare(
            Side(f"{basis} before {sha}", [r for r in dated if r[basis] < when]),
            Side(f"{basis} after {sha}", [r for r in dated if r[basis] >= when]),
        )

    return 0


if __name__ == "__main__":
    sys.exit(main())
