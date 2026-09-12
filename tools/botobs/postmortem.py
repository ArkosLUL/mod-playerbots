#!/usr/bin/env python3
"""Read a RaidObs trace and explain what happened.

A trace is NDJSON: one record per line, `t` in milliseconds relative to the pull (negative during the
pre-roll). Schema and field meanings live in docs/systems/observability.md.

    postmortem.py <file>                 session summary + one block per death
    postmortem.py <file> --death N       full rewind for one death
    postmortem.py <file> --bot NAME      that bot's timeline
    postmortem.py <file> --track NAME    position track, with distance to each boss
    postmortem.py <file> --notes [KEY]   pull/phase/note/end records, optionally one key prefix
    postmortem.py <file> --stalls [MS]   held still while still asking to move - i.e. stuck
    postmortem.py <file> --clump [YARDS] how stacked the raid was, largest group in one circle
    postmortem.py <file> --verify        check the trace against the invariants the schema promises
"""
from __future__ import annotations

import argparse
import pathlib
import sys

from deathreport import show_death, summarise
from obstrace import Trace
from views import show_bot, show_clump, show_notes, show_stalls, show_track, show_verify


def main() -> int:
    parser = argparse.ArgumentParser(description="Explain a RaidObs trace.")
    parser.add_argument("file", type=pathlib.Path)
    parser.add_argument("--death", type=int, help="full detail for one death, by index")
    parser.add_argument("--bot", help="timeline for one bot")
    parser.add_argument("--track", help="position track for one bot")
    parser.add_argument(
        "--notes",
        nargs="?",
        const="",
        metavar="KEY",
        help="pull/note/hazard/end records only; pass a key prefix such as hodir. to narrow it",
    )
    parser.add_argument(
        "--stalls",
        nargs="?",
        type=int,
        const=6000,
        metavar="MS",
        help="windows where a bot held station while still issuing accepted moves (default 6000ms)",
    )
    parser.add_argument(
        "--clump",
        nargs="?",
        type=float,
        const=10.0,
        metavar="YARDS",
        help="how stacked the raid was, as the largest group inside one circle (default 10 yd)",
    )
    parser.add_argument(
        "--verify",
        action="store_true",
        help="check the trace against the schema's invariants; exits non-zero if any fail",
    )
    args = parser.parse_args()

    if not args.file.is_file():
        print(f"no such trace: {args.file}", file=sys.stderr)
        return 1

    trace = Trace(args.file)

    if args.death is not None:
        return show_death(trace, args.death)
    if args.bot:
        return show_bot(trace, args.bot)
    if args.track:
        return show_track(trace, args.track)
    if args.notes is not None:
        return show_notes(trace, args.notes or None)
    if args.stalls is not None:
        return show_stalls(trace, args.stalls)
    if args.clump is not None:
        return show_clump(trace, args.clump)
    if args.verify:
        return show_verify(trace)

    summarise(trace)
    return 0


if __name__ == "__main__":
    sys.exit(main())
