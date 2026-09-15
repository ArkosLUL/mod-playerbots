#!/usr/bin/env python3
"""Read a RaidObs trace and explain what happened.

A trace is NDJSON: one record per line, `t` in milliseconds relative to the pull (negative during the
pre-roll). Schema and field meanings live in docs/systems/observability.md.

    postmortem.py <file>                 session summary + one block per death
    postmortem.py <file> --death N       full rewind for one death
    postmortem.py <file> --bot NAME      that bot's timeline
    postmortem.py <file> --track NAME    position track, with distance to each boss
    postmortem.py <file> --notes [KEY]   pull/phase/note/end records, optionally one key prefix
    postmortem.py <file> --probes [KEY]  what each probe key decided, ranked by churn
    postmortem.py <file> --stalls [MS]   held still while still asking to move - i.e. stuck
    postmortem.py <file> --idle [MS]     held a target and cast nothing - stuck, the other half
    postmortem.py <file> --vetoes        which multiplier zeroed which action, most often first
    postmortem.py <file> --moves [ACT]   what each mover did to the raid's distance from --from
    postmortem.py <file> --where SPEC    where deaths/casts/notes happened, relative to --from
    postmortem.py <file> --threat [ENT]  who the hostiles were on, by role, weighted by time held
    postmortem.py <file> --clump [YARDS] how stacked the raid was, largest group in one circle
    postmortem.py <file> --verify        check the trace against the invariants the schema promises
    postmortem.py <file> --coverage [P]  which strategy nodes did anything, and why the rest did not
    postmortem.py <file> --validity      only the banner: which build, which mode, who was human
    postmortem.py <file> --since REF     compare the build against REF instead of HEAD
"""
from __future__ import annotations

import argparse
import pathlib
import sys

from coverage import show_coverage
from deathreport import show_death, summarise
from obstrace import Trace
from probes import show_probes
from space import show_moves, show_threat, show_where
from validity import show_validity
from views import (show_bot, show_clump, show_idle, show_notes, show_stalls, show_track,
                   show_verify, show_vetoes)


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
        "--probes",
        nargs="?",
        const="",
        metavar="KEY",
        help="every probe key ranked by churn; name one key exactly for its full timeline",
    )
    parser.add_argument(
        "--during",
        metavar="KEY=VALUE",
        help="only while a latch held a value, e.g. mimiron.phase=1; scopes the views that take it",
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
        "--idle",
        nargs="?",
        type=int,
        const=10000,
        metavar="MS",
        help="bots that held a target and cast nothing at all (default 10000ms)",
    )
    parser.add_argument(
        "--vetoes",
        action="store_true",
        help="which multiplier zeroed which action, and how often",
    )
    parser.add_argument(
        "--moves",
        nargs="?",
        const="",
        metavar="ACTION",
        help="per mover, the radius it moved bots from and to; name one to see its role split",
    )
    parser.add_argument(
        "--where",
        metavar="SPEC",
        help="radius of each event from --from, as death, cast:<spell> or note:<key>",
    )
    parser.add_argument(
        "--threat",
        nargs="?",
        type=int,
        const=0,
        metavar="ENTRY",
        help="who the hostiles held as target, by role; pass a creature entry to narrow it",
    )
    parser.add_argument(
        "--from",
        dest="origin",
        metavar="ANCHOR",
        help="point to measure from: a Position constant's name, or entry:<N> for a creature",
    )
    parser.add_argument(
        "--band",
        metavar="RADIUS",
        help="with --moves or --where, a distance to score against: a float constant's name, or a number",
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
    parser.add_argument(
        "--coverage",
        nargs="?",
        const="",
        metavar="PREFIX",
        help="per-node checks/fires/runs; pass a node-name prefix such as hodir to narrow it",
    )
    parser.add_argument(
        "--by-bot",
        action="store_true",
        help="with --coverage, name the bots behind each node instead of only counting them",
    )
    parser.add_argument(
        "--validity",
        action="store_true",
        help="only the validity banner; exits non-zero if anything disqualifies the pull",
    )
    parser.add_argument(
        "--since",
        metavar="REF",
        help="commit-ish or ISO time the build must be newer than; naming one makes it disqualify",
    )
    parser.add_argument(
        "--hardmode",
        action="store_true",
        help="the pull was meant to be hard mode, so hard mode off disqualifies it",
    )
    args = parser.parse_args()

    if not args.file.is_file():
        print(f"no such trace: {args.file}", file=sys.stderr)
        return 1

    trace = Trace(args.file)

    if args.validity:
        return 1 if show_validity(trace, args.since, args.hardmode) else 0

    if args.death is not None:
        return show_death(trace, args.death)
    if args.bot:
        return show_bot(trace, args.bot)
    if args.track:
        return show_track(trace, args.track)
    if args.notes is not None:
        return show_notes(trace, args.notes or None)
    if args.probes is not None:
        return show_probes(trace, args.probes or None, args.during)
    if args.stalls is not None:
        return show_stalls(trace, args.stalls)
    if args.idle is not None:
        return show_idle(trace, args.idle)
    if args.vetoes:
        return show_vetoes(trace)
    if args.moves is not None or args.where:
        if not args.origin:
            print("--moves and --where need --from ANCHOR to measure against", file=sys.stderr)
            return 1
        if args.where:
            return show_where(trace, args.where, args.origin, args.band, args.during)
        return show_moves(trace, args.moves or None, args.origin, args.band, args.during)
    if args.threat is not None:
        return show_threat(trace, args.threat or None, args.during)
    if args.clump is not None:
        return show_clump(trace, args.clump)
    if args.verify:
        return show_verify(trace)
    if args.coverage is not None:
        return show_coverage(trace, args.coverage or None, args.by_bot)

    show_validity(trace, args.since, args.hardmode)
    summarise(trace)
    return 0


if __name__ == "__main__":
    sys.exit(main())
