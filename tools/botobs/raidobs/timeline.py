"""One unit's records, or the pull's own, in time order: --bot, --track, --notes.

Each returns the process exit code, so main() can hand it straight to sys.exit.
"""
from __future__ import annotations

import math
import sys

from .records import move_line, note_text
from .trace import Trace, clock


def show_bot(trace: Trace, name: str) -> int:
    guid = trace.guid_by_name(name)
    if guid is None:
        print(f"no unit matching '{name}'", file=sys.stderr)
        return 1

    print(f"timeline for {trace.name(guid)} ({trace.roles.get(guid, '?')})")
    for rec in trace.records:
        event = rec.get("e")
        stamp = clock(rec["t"])
        if event == "act" and rec.get("g") == guid:
            print(f"{stamp}  act    {rec['vd']:<10} rel {rec['rel']:<8} {rec['a']}")
        elif event == "veto" and rec.get("g") == guid:
            print(f"{stamp}  VETO   {rec['m']} killed {rec['a']}")
        elif event == "move" and rec.get("g") == guid:
            print(f"{stamp}  move   {move_line(trace, rec)}")
        elif event == "note" and rec.get("g") == guid:
            print(f"{stamp}  note   {rec['k']} = {note_text(trace, rec)}")
        elif event == "dmg" and rec.get("d") == guid:
            # "at", not "->": a damage log goes out before the core applies the hit, so this is the
            # health the hit landed on. An arrow here reads as the outcome and shifts the whole
            # trajectory down one row, hiding the killing blow. See docs/systems/observability.md.
            lethal = " LETHAL" if rec.get("ok") else ""
            print(
                f"{stamp}  dmg    {rec['a']:>7} from {trace.name(rec['s'])} {trace.spell(rec['sp'])}"
                f" (at {rec['hp']}%){lethal}"
            )
        elif event == "heal" and rec.get("d") == guid:
            print(f"{stamp}  heal   {rec['a']:>7} from {trace.name(rec['s'])} {trace.spell(rec['sp'])} -> {rec['hp']}%")
        elif event == "abs" and rec.get("d") == guid:
            print(f"{stamp}  absorb {rec['a']:>7} by {trace.name(rec['s'])} shield {trace.spell(rec['sp'])}")
        elif event == "aura" and rec.get("d") == guid:
            verb = "lost" if rec.get("r") else "got"
            stacks = f" x{rec['st']}" if "st" in rec else ""
            print(f"{stamp}  aura   {verb} {trace.spell(rec['sp'])}{stacks} from {trace.name(rec['s'])}")
        elif event == "death" and rec.get("g") == guid:
            print(f"{stamp}  DIED   killed by {trace.name(rec.get('killer'))}")

    return 0


def show_track(trace: Trace, name: str) -> int:
    guid = trace.guid_by_name(name)
    if guid is None:
        print(f"no unit matching '{name}'", file=sys.stderr)
        return 1

    print(f"track for {trace.name(guid)}  (step = distance moved since previous sample)")
    header = "time      x        y        z       hp    step   " + "  ".join(
        f"d:{trace.name(b)[:10]}" for b in sorted(trace.bosses)
    )
    print(header)

    previous = None
    for snap in trace.of("snap"):
        rows = {row[0]: row for row in snap.get("u", [])}
        row = rows.get(guid)
        if not row:
            continue

        _, x, y, z, _o, hp = row[0], row[1], row[2], row[3], row[4], row[5]
        step = 0.0 if previous is None else math.dist((x, y, z), previous)
        previous = (x, y, z)

        dists = []
        for boss in sorted(trace.bosses):
            brow = rows.get(boss)
            dists.append(f"{math.dist((x, y, z), (brow[1], brow[2], brow[3])):8.1f}" if brow else "       -")

        print(f"{clock(snap['t']):>9} {x:8.1f} {y:8.1f} {z:7.1f} {hp:5.1f} {step:6.2f}  " + " ".join(dists))

    return 0


def show_notes(trace: Trace, prefix: str | None = None) -> int:
    for rec in trace.records:
        event = rec.get("e")
        # A note key is prefixed with the encounter that owns it, so one raid's latches can be read
        # without the others - Iron Assembly legitimately spans three bosses under one slug.
        if prefix and event == "note" and not rec.get("k", "").startswith(prefix):
            continue

        if event in ("pull", "end"):
            detail = rec.get("boss") or rec.get("out")
            print(f"{clock(rec['t']):>9}  {event.upper():<6} {detail}")
        elif event == "note":
            print(
                f"{clock(rec['t']):>9}  note   {trace.name(rec.get('g')):<16} {rec['k']} = {note_text(trace, rec)}"
            )
        elif event == "haz":
            shape = rec.get("shape", "?")
            # Shape fields differ per shape, so print whatever the probe attached rather than a
            # fixed set: "side" for a lane, "rad" for a circle, "lead"/"sweep"/"rate" for a sweep.
            extra = " ".join(
                f"{k}={v}"
                for k, v in rec.items()
                if k not in ("t", "e", "sp", "shape", "x", "y", "z", "ttl")
            )
            place = f"({rec.get('x')}, {rec.get('y')}, {rec.get('z')})"
            print(f"{clock(rec['t']):>9}  haz    {trace.spell(rec['sp'])} {shape} at {place} ttl {rec.get('ttl')} {extra}")

    return 0
