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
    batch.py --probes               note keys the module emits that no tool reads
    batch.py --verify               roll up the invariant checks across the selection

Retention is 7 days and 5 GB (Obs.RetentionDays, Obs.MaxDirMB), so a baseline worth keeping belongs
outside the log dir - pass its directory as an extra positional and it joins the selection.
"""
from __future__ import annotations

import argparse
import pathlib
import sys
from collections import Counter

from obstrace import Trace, boss_from_path, find_traces
from validity import REPO, boss_of, inspect, resolve_since
from views import verify_checks

DEFAULT_ROOT = REPO.parents[1] / "env" / "dist" / "logs" / "botobs"

# Note keys each scorer actually consumes. Kept as a declaration rather than derived by grep, because
# the point of --probes is to catch a probe the module publishes and no reader ever picked up - and a
# grep over the reader would happily "find" a key in a comment saying it is unread.
CONSUMED_KEYS: dict[str, tuple[str, ...]] = {
    "flame_leviathan.py": ("fl.station", "fl.vent"),
}

# One letter per disqualifier for the row table. Not the first letter of the kind: hardmode-off and
# human-in-raid would collide, and those two are the pair most worth telling apart.
FLAG = {"stale-build": "S", "hardmode-off": "M", "human-in-raid": "H"}

# What --valid drops. A human is in the raid in all 125 traces on disk, so treating that alone as
# disqualifying leaves an empty selection and says nothing. It is also the weakest of the three: the
# roster records a human's role as the literal string "human", so the trace cannot say whether they
# held one the strategy was meant to play, which is the thing that actually invalidated the Mimiron
# pull. --strict drops them anyway.
DECIDABLE = {"stale-build", "hardmode-off"}


def decidable(row: dict) -> list[tuple[str, str]]:
    return [w for w in row["warnings"] if w[0] in DECIDABLE]


def row_for(trace: Trace, ref) -> dict:
    """One trace reduced to the handful of facts a corpus view needs, so the Trace can be dropped."""
    facts, warnings = inspect(trace, ref)
    ends = trace.of("end")
    snaps = trace.of("snap")
    checks = verify_checks(trace)
    return {
        "name": trace.path.name,
        "boss": boss_of(trace) or boss_from_path(trace.path),
        "v": trace.header.get("v"),
        "outcome": ends[-1].get("out") if ends else "cut short",
        "ms": snaps[-1]["t"] if snaps else 0,
        "roster": len(trace.header.get("roster", [])),
        "humans": len(facts["humans"]),
        "hardmode": facts["hardmode"],
        "warnings": warnings,
        "failed": [label for label, count, _ in checks if count],
        "checks": len(checks),
        "deaths": len(trace.of("death")),
        "truncated": trace.truncated,
        "keys": Counter(rec.get("k", "") for rec in trace.of("note")),
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


def show_census(rows: list[dict]) -> None:
    total = len(rows)
    clean = sum(1 for r in rows if not decidable(r))
    print(f"\nvalidity   {clean}/{total} traces survive the checks that can be decided")
    reasons = Counter(kind for row in rows for kind, _ in row["warnings"])
    for kind, count in reasons.most_common():
        note = "" if kind in DECIDABLE else "   (informational)"
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
    valid_boss = Counter(r["boss"] for r in rows if not decidable(r))
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
    """Which note keys the recorder wrote that nothing reads.

    flame_leviathan.py is the precedent: the module publishes fl.pursued, fl.corner, fl.frozen,
    fl.lifetower, fl.interrupter and fl.pyrite, and the only scorer in the tree reads two keys. Its
    victim() still guesses by facing ray at a boss the trace names outright.
    """
    emitted: Counter = Counter()
    for row in rows:
        emitted.update(row["keys"])
    emitted.pop("", None)

    consumed = {key for keys in CONSUMED_KEYS.values() for key in keys}
    print(f"\nprobes     {len(emitted)} note key(s) across {len(rows)} trace(s)")
    for key, count in sorted(emitted.items()):
        reader = next((tool for tool, keys in CONSUMED_KEYS.items() if key in keys), None)
        print(f"           {count:7d}  {key:<24} {reader or 'UNREAD'}")

    unread = sorted(set(emitted) - consumed)
    if unread:
        print(f"\n           {len(unread)} emitted and never read: {', '.join(unread)}")
    stale = sorted(consumed - set(emitted))
    if stale:
        print(f"           {len(stale)} read but never emitted here: {', '.join(stale)}")


def main() -> int:
    parser = argparse.ArgumentParser(description="Read the RaidObs trace corpus.")
    parser.add_argument("roots", nargs="*", type=pathlib.Path, default=None,
                        help=f"trace files or directories (default: {DEFAULT_ROOT})")
    parser.add_argument("--boss", help="only traces whose slug matches, e.g. thorim")
    parser.add_argument("--since", metavar="REF",
                        help="commit-ish or ISO time the build must be newer than (default: HEAD)")
    parser.add_argument("--valid", action="store_true",
                        help="drop traces with a stale build or the wrong hard-mode setting")
    parser.add_argument("--strict", action="store_true",
                        help="--valid, and also drop any trace with a human in the raid")
    parser.add_argument("--census", action="store_true", help="totals only, no per-trace rows")
    parser.add_argument("--verify", action="store_true", help="roll up the invariant checks")
    parser.add_argument("--probes", action="store_true", help="note keys nothing reads")
    parser.add_argument("--limit", type=int, help="stop after N traces, newest first")
    args = parser.parse_args()

    roots = args.roots or [DEFAULT_ROOT]
    paths = find_traces(roots, args.boss)
    if args.limit:
        paths = paths[: args.limit]
    if not paths:
        where = args.boss and f" for boss {args.boss}" or ""
        print(f"no traces{where} under {', '.join(str(r) for r in roots)}", file=sys.stderr)
        return 1

    ref = resolve_since(REPO, args.since)
    # Reading the corpus takes minutes, so say where it is - but only to a terminal, since the
    # carriage returns turn a redirected run into one long line.
    progress = sys.stderr.isatty()
    rows = []
    for index, path in enumerate(paths, 1):
        if progress:
            print(f"\rreading {index}/{len(paths)} {path.name[:48]:<48}", end="", file=sys.stderr)
        rows.append(row_for(Trace(path), ref))
    if progress:
        print("\r" + " " * 60 + "\r", end="", file=sys.stderr)

    if args.valid or args.strict:
        rows = [r for r in rows if not (r["warnings"] if args.strict else decidable(r))]
        if not rows:
            print("nothing in the selection counts as evidence", file=sys.stderr)
            return 1

    label = f"{len(rows)} trace(s)" + (f", boss {args.boss}" if args.boss else "")
    print(f"{label}, build compared against {ref[0] if ref else 'nothing'}")

    if not args.census:
        show_rows(rows)
    show_census(rows)
    if args.verify:
        show_verify_rollup(rows)
    if args.probes:
        show_probes(rows)

    return 0


if __name__ == "__main__":
    sys.exit(main())
