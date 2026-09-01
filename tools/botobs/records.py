"""Rendering one record out of a trace.

Each helper turns a single raw row - an aura, a verdict row, a move - into something a reader
recognises, and absorbs the schema drift between versions on the way.
"""
from __future__ import annotations

from obstrace import Trace

# Unit::Kill strips a dying unit's auras through the same hook the recorder listens on, so an aura
# held right up to the death is stamped with the death's own timestamp. Anything inside this window
# was still on when the bot fell, not something that had worn off.
STRIP_AT_DEATH_MS = 250


def is_debuff(row: list) -> bool:
    """Whether this hurt.

    v5 carries the answer as a 7th column, straight off the spell. v4 has to guess from the caster's
    guid tag, which is wrong both ways - totems and pets buff from a creature guid, and Biting Cold is
    applied to the player by the player - so it is only the fallback.
    """
    if len(row) >= 7:
        return not row[6]

    caster = row[3]
    return not caster or (caster >> 32) != 0


def aura_line(trace: Trace, row: list, death_t: int) -> str:
    spell, stacks, duration, caster, applied, removed = row[:6]

    # -1 means the recorder never saw the apply, which is every raid buff cast before the pull. Older
    # traces wrote -startMs there instead; both are unknown, and a made-up duration is worse than none.
    held = f"{'?':>6}" if applied < 0 else f"{(death_t - applied) / 1000:5.1f}s"
    left = "-" if duration < 0 else f"{duration / 1000:.1f}s left"
    if removed < 0 or death_t - removed <= STRIP_AT_DEATH_MS:
        gone = ""
    else:
        gone = f", fell off {(death_t - removed) / 1000:.1f}s before"
    return f"{trace.spell(spell)} x{stacks:<3} {left:>10}  held {held}  from {trace.name(caster)}{gone}"


def held_to_death(row: list, death_t: int) -> bool:
    removed = row[5]
    return removed < 0 or death_t - removed <= STRIP_AT_DEATH_MS


def act_row(row: list) -> tuple:
    """One `death.acts` row as (firstT, lastT, action, relevance, verdict, repeats).

    v3 wrote one row per verdict with no span and no repeat count; reading both shapes is what lets a
    v4 run be compared against the trace that motivated it.
    """
    if len(row) >= 6:
        return tuple(row[:6])

    first, action, relevance, verdict = row[:4]
    return first, first, action, relevance, verdict, 1


# The only two reasons that mean the movement layer turned a destination down. "dup" and "wait" are the
# ordinary state while a bot walks to a destination its action re-offers every tick, and "there" means
# it is already standing on it.
REFUSALS = {"blocked", "nopath"}


def note_text(trace: Trace, rec: dict) -> str:
    """A note's payload is free text, so a guid written into one arrives as a bare number.

    Joined here rather than at the recorder because the unit records are already in the file and only
    the join is missing, the same way every other guid in the schema is resolved on read. Substitutes
    only when the whole payload is a guid the trace knows, so counters like hodir.slot and coordinate
    strings like hodir.anchor are left alone.
    """
    txt = str(rec.get("txt", ""))
    body = txt.strip()
    if body.isdigit() and int(body) in trace.names:
        return trace.name(int(body))
    return txt


def move_line(trace: Trace, rec: dict) -> str:
    reason = rec.get("r", "")
    if not reason:
        status = "ok"
    elif reason in REFUSALS:
        status = f"REFUSED: {reason}"
    else:
        status = reason

    # The gate yields only to a strictly higher priority, so a "wait" is a contest and the holder is
    # the other half of it. Absent before v6, and absent on Follow and Chase, which never face the gate.
    priority = rec.get("pr")
    if priority:
        status = f"{status}, {priority}"
    elif priority == "":
        status = f"{status}, no priority"

    holder = rec.get("hpr")
    if holder:
        status = f"{status}, held by {holder} {rec.get('hms', 0) / 1000:.1f}s"

    where = f"({rec['x']}, {rec['y']}, {rec['z']})"
    target = rec.get("tgt")
    if target:
        where = f"{trace.name(target)} at {where}"

    return f"{rec.get('k', 'point'):<6} -> {where} by '{rec['by']}' [{status}]"
