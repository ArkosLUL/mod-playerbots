"""The default report, and the per-death rewind that usually answers why a bot fell.

Ordered the way the answer is normally found: what was on the bot, then what it was standing in,
then what hit it, then what it was doing instead of moving.
"""
from __future__ import annotations

import sys
from collections import defaultdict

from analysis import combat_deaths, damage_summary, hazards_at
from obstrace import Trace, clock
from records import act_row, aura_line, held_to_death, is_debuff


def summarise(trace: Trace) -> None:
    hdr = trace.header
    print(f"trace   {trace.path.name}")
    # hdr.boss is line one of a file that is only appended to, so a session that opened before its boss
    # was engaged still carries the map name there. The rename record is the corrected one.
    renames = [p for p in trace.of("pull") if p.get("src") == "rename"]
    boss = renames[-1].get("boss") if renames else hdr.get("boss", "?")
    print(f"boss    {boss}  map {hdr.get('map')} instance {hdr.get('inst')} diff {hdr.get('diff')}")

    roster = hdr.get("roster", [])
    by_role: dict[str, int] = defaultdict(int)
    for member in roster:
        by_role[member.get("r", "?")] += 1
    comp = " ".join(f"{n}x{role}" for role, n in sorted(by_role.items()))
    print(f"raid    {len(roster)} members ({comp}), {len(trace.humans)} human")

    pulls = trace.of("pull")
    ends = trace.of("end")
    if pulls:
        line = f"pull    {pulls[0].get('boss')} via {pulls[0].get('src')}"
        if renames:
            line += f", renamed to {renames[-1].get('boss')}"
        print(line)
    if ends:
        print(f"outcome {ends[0].get('out')} at {clock(ends[0]['t'])}")
    else:
        print("outcome (no end record - trace cut short)")

    snaps = trace.of("snap")
    if snaps:
        print(f"span    {clock(snaps[0]['t'])} .. {clock(snaps[-1]['t'])}  ({len(snaps)} snapshots)")

    if trace.truncated:
        print("        *** trace hit the size cap and stopped early ***")

    combat = combat_deaths(trace)
    reset = len(trace.of("death")) - len(combat)
    print(f"deaths  {len(combat)}" + (f"  (+{reset} to a wipe command)" if reset else ""))
    print()

    for index, death in enumerate(combat):
        death_block(trace, death, index, brief=True)


def death_block(trace: Trace, death: dict, index: int, brief: bool) -> None:
    guid = death["g"]
    print(f"[{index}] {trace.name(guid)} ({trace.roles.get(guid, '?')}) died at {clock(death['t'])}")
    cause = death.get("cause")
    if cause == "reset":
        print("     killed by the wipe command")
    elif cause == "self":
        # Environmental damage bypasses the combat log, so there is no blow and no rewind entry either.
        print("     died to self or environmental damage, no killing blow recorded")
    else:
        print(f"     killed by {trace.name(death.get('killer'))}")
    print(f"     at ({death.get('x')}, {death.get('y')}, {death.get('z')})")

    # First, because it is the answer far more often than anything else in the block: the core strips
    # a dying unit's auras before the death hook, so v3 traces could not show this at all. Held-to-death
    # ahead of worn-off, then most recent first - the one that killed the bot is rarely the oldest.
    auras = death.get("auras") or []
    debuffs = sorted(
        (a for a in auras if len(a) >= 6 and is_debuff(a)),
        key=lambda a: (not held_to_death(a, death["t"]), -a[4]),
    )
    if debuffs:
        shown = debuffs[: 5 if brief else 40]
        count = f"{len(debuffs)}" if len(shown) == len(debuffs) else f"{len(shown)} of {len(debuffs)} shown"
        print(f"     debuffs ({count}):")
        for row in shown:
            print(f"       {aura_line(trace, row, death['t'])}")

    covering, markers, units, friendly = hazards_at(trace, death)
    if covering:
        where = ", ".join(f"{trace.spell(sp)} ({gap:.1f} of {rad:.1f} yd)" for sp, gap, rad in covering)
        print(f"     STOOD IN {where}")
    if markers:
        where = ", ".join(f"{trace.spell(sp)} at {gap:.1f} yd" for sp, gap, _rad in markers)
        print(f"     NEAR     {where}")
    if units:
        where = ", ".join(f"{trace.name(g)} at {gap:.1f} yd" for g, gap in units)
        print(f"     ON TOP   {where}")
    if friendly and not brief:
        where = ", ".join(f"{trace.spell(sp)} ({gap:.1f} of {rad:.1f} yd)" for sp, gap, rad in friendly)
        print(f"     inside   {where}")

    dist = death.get("dist") or {}
    if dist:
        near = " ".join(f"{trace.name(int(g))}={d}" for g, d in sorted(dist.items(), key=lambda kv: kv[1]))
        print(f"     range    {near}")

    # The rewind holds the last 15 seconds and nothing older, so an empty one is a statement rather
    # than a gap - and it is exactly the case the killing blow below is there to answer.
    rewind = death.get("rewind") or []
    if rewind:
        # Sorted here rather than trusted: v5 writes the ring chronologically, v4 wrote it by amount,
        # and the question this line answers is what landed last.
        for when, source, spell, amount in sorted(rewind, key=lambda row: row[0])[-3:]:
            print(f"     hit at {clock(when)}  {amount:>8}  {trace.name(source)} {trace.spell(spell)}")

        rows = damage_summary(trace, rewind)
        total = sum(r[1] for r in rows)
        print(f"     took {total} over {len(rewind)} hits:")
        for label, amount, hits in rows[: 3 if brief else 20]:
            share = 100.0 * amount / total if total else 0.0
            print(f"       {amount:>8}  {share:5.1f}%  {hits:>3}x  {label}")
    else:
        print("     took no damage in the rewind window")

    blow = death.get("blow")
    hplast = death.get("hplast")
    if blow or hplast:
        parts = []
        if hplast:
            parts.append(f"{hplast[0]}% at {clock(hplast[1])}")
        parts.append(f"final blow {blow[1]} from {trace.name(blow[0])}" if blow else "no final blow recorded")
        print(f"     health   {', '.join(parts)}")

    move = death.get("lastmove")
    if move:
        state = "arrived" if move.get("arrived") else "STILL WALKING"
        print(f"     moving  -> ({move['x']}, {move['y']}, {move['z']}) by '{move['by']}' [{state}]")

    # One row per verdict per distinct engine pass, carrying how many passes running produced it.
    acts = [act_row(row) for row in death.get("acts") or []]
    if acts:
        last = acts[-1]
        print(f"     doing   '{last[2]}' rel {last[3]} -> {last[4]}")
        if not brief:
            print(f"     last {len(acts)} verdict rows:")
            for first, last_ms, action, rel, verdict, repeats in acts:
                span = clock(first) if repeats == 1 else f"{clock(first)}..{clock(last_ms)}"
                run = "" if repeats == 1 else f" x{repeats}"
                print(f"       {span:>21}  {verdict:<14}{run:<6} rel {rel:<8} {action}")

    buffs = [a for a in auras if len(a) >= 6 and not is_debuff(a)]
    if buffs and not brief:
        print(f"     buffs held: {', '.join(f'{trace.spell(row[0])} x{row[1]}' for row in buffs)}")

    print()


def show_death(trace: Trace, index: int) -> int:
    deaths = combat_deaths(trace)
    if index < 0 or index >= len(deaths):
        print(f"no death {index} (trace has {len(deaths)})", file=sys.stderr)
        return 1

    death_block(trace, deaths[index], index, brief=False)

    # Vetoes rarely coincide with a death by accident: they are the mechanism by which a bot that
    # "should have moved" did not.
    guid = deaths[index]["g"]
    start = deaths[index]["t"] - 10000
    vetoes = [r for r in trace.of("veto") if r["g"] == guid and start <= r["t"] <= deaths[index]["t"]]
    if vetoes:
        print("     vetoes in the last 10s:")
        for veto in vetoes:
            print(f"       {clock(veto['t'])}  {veto['m']} killed {veto['a']}")
        print()

    return 0
