#!/usr/bin/env python3
"""Read a Freya pull from a RaidObs trace: hard mode, add waves, who held Freya and the Snaplasher, deaths.

    freya.py <file>          every section
    freya.py <file> --tanks  who Freya and each Snaplasher were on, and every area taunt or Righteous
                             Defense a tank cast

What the generic views miss here:

- **Hard mode is the Elders, not the toggle.** `cfg.hardmode` has no Freya key. Each Elder alive at the
  pull leaves its empower in the trace: Ground Tremor casts (Stonebark), Strengthened Iron Roots
  (Ironbranch), Sun Beam stalkers (Brightleaf). Elders never die once the pull starts, so all three
  seen means a kill counts as hard mode.
- **A wave is the adds first seen together.** It ends when its last member is last sampled alive, and a
  member still alive at the last snapshot never died.
- **`--threat` folds both tanks into "tank".** Freya belongs on the main tank and the Snaplasher on the
  add tank, so `--tanks` names the holder.
"""
from __future__ import annotations

import collections
import pathlib
import sys

# Run as a script from bosses/, so the raidobs package one level up is not on the path yet.
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from raidobs.cli import run_sections  # noqa: E402
from raidobs.encounter import encounter_of  # noqa: E402
from raidobs.geometry import frames  # noqa: E402
from raidobs.trace import Trace, clock, combat_deaths, own_blow_spell  # noqa: E402

ENCOUNTER = "freya"

NPC_FREYA = 32906
NPC_SNAPLASHER = 32916
NPC_DETONATING_LASHER = 32918
NPC_CONSERVATOR = 33203
NPC_EONARS_GIFT = 33228
NPC_STRENGTHENED_IRON_ROOTS = 33168
NPC_SUN_BEAM = 33170
TRIO = (NPC_SNAPLASHER, 32919, 33202)
WAVE_KINDS = {NPC_DETONATING_LASHER: "lashers", NPC_CONSERVATOR: "conservator", **{e: "trio" for e in TRIO}}
GROUND_TREMOR = (62437, 62859)
# Spells that take every unit near the caster or hitting the target, rather than the caster's own target.
AREA_TAUNTS = {5209: "Challenging Roar", 1161: "Challenging Shout", 31789: "Righteous Defense"}

# One SpawnWave summons a block inside a tick, but sampling every 250 ms can split it across two snapshots.
SAME_WAVE_MS = 2000
# Lifebinder's Gift heals Freya and her adds once the Gift has lived this long.
GIFT_CAST_MS = 12000
# Shorter holds are a swing or two landing between snapshots, not a lost target.
HOLD_MIN_MS = 1000


def mine(trace: Trace) -> bool:
    return encounter_of(trace) == ENCOUNTER


def sightings(trace: Trace, entries) -> dict[int, dict]:
    """Per creature of these entries: first and last snapshot alive, health then, and its targets."""
    seen: dict[int, dict] = {}
    for snap in frames(trace):
        for row in snap.get("u", []):
            if trace.entries.get(row[0]) not in entries or row[5] <= 0:
                continue
            rec = seen.setdefault(row[0], {"g": row[0], "first": snap["t"], "spans": []})
            rec["last"], rec["hp"] = snap["t"], row[5]
            spans = rec["spans"]
            if spans and spans[-1][2] == row[7]:
                spans[-1][1] = snap["t"]
            else:
                spans.append([snap["t"], snap["t"], row[7]])
    return seen


def last_snapshot(trace: Trace) -> int:
    snaps = frames(trace)
    return snaps[-1]["t"] if snaps else 0


def elders(trace: Trace) -> list[str]:
    seen = set(trace.entries.values())
    tremor = any(rec.get("sp") in GROUND_TREMOR for rec in trace.of("cast"))
    return [name for name, found in (("Stonebark", tremor), ("Ironbranch", NPC_STRENGTHENED_IRON_ROOTS in seen),
                                     ("Brightleaf", NPC_SUN_BEAM in seen)) if found]


def show_banner(trace: Trace) -> None:
    print(f"Freya  {trace.path.name}")
    if not mine(trace):
        print("  not a Freya pull, every section below reads empty")
        return
    ends = trace.of("end")
    out = ends[-1].get("out", "?") if ends else "?"
    when = f" at {clock(ends[-1]['t'])}" if ends else ""
    found = elders(trace)
    print(f"  outcome {out}{when}, {len(combat_deaths(trace))} death(s)")
    print(f"  hard mode: {len(found)} of 3 Elders ({', '.join(found) or 'none'})")


def waves(trace: Trace) -> list[dict]:
    """Lasher packs, trio waves and Conservators in spawn order, with when each ended and how many of it
    were still alive when the pull did."""
    end = last_snapshot(trace)
    out: list[dict] = []
    for rec in sorted(sightings(trace, WAVE_KINDS).values(), key=lambda r: r["first"]):
        kind = WAVE_KINDS[trace.entries[rec["g"]]]
        wave = next((w for w in out if w["kind"] == kind and rec["first"] - w["t"] <= SAME_WAVE_MS), None)
        if wave is None:
            wave = {"kind": kind, "t": rec["first"], "members": []}
            out.append(wave)
        wave["members"].append(rec)
    for wave in out:
        wave["end"] = max(m["last"] for m in wave["members"])
        wave["alive"] = sum(1 for m in wave["members"] if m["last"] >= end)
    return out


def show_waves(trace: Trace) -> None:
    print("WAVES")
    if not mine(trace):
        return
    deaths = combat_deaths(trace)
    for wave in waves(trace):
        dead = sum(1 for d in deaths if wave["t"] <= d["t"] <= wave["end"])
        left = f", {wave['alive']} alive at the end" if wave["alive"] else ""
        print(f"  {clock(wave['t'])} .. {clock(wave['end'])}  {wave['kind']:<11} {len(wave['members'])} add(s)"
              f"{left}, {dead} raid death(s) meanwhile")
    end = last_snapshot(trace)
    for gift in sorted(sightings(trace, (NPC_EONARS_GIFT,)).values(), key=lambda r: r["first"]):
        lived = gift["last"] - gift["first"]
        if lived >= GIFT_CAST_MS:
            verdict = f"lived to heal, {gift['hp']:.0f}% left"
        else:
            verdict = "alive at the end" if gift["last"] >= end else "killed"
        print(f"  {clock(gift['first'])} Eonar's Gift {lived / 1000:.1f}s, {verdict}")


def holder_label(trace: Trace, guid) -> str:
    role = trace.role(guid)
    return f"{trace.name(guid)}/{role}" if role != "?" else trace.name(guid)


def show_tanks(trace: Trace) -> None:
    print("TANKS")
    if not mine(trace):
        return
    for freya in sightings(trace, (NPC_FREYA,)).values():
        held: collections.Counter = collections.Counter()
        for t0, t1, target in freya["spans"]:
            held[target] += t1 - t0
        main = max((g for g in held if trace.role(g) == "tank"), key=held.get, default=None)
        total = sum(held.values()) or 1
        print(f"  Freya on {holder_label(trace, main)} {100.0 * held[main] / total:.1f}% of the pull")
        for t0, t1, target in freya["spans"]:
            if target != main and target and t1 - t0 >= HOLD_MIN_MS:
                print(f"    {clock(t0)} .. {clock(t1)} on {holder_label(trace, target)}")
    for snap in sorted(sightings(trace, (NPC_SNAPLASHER,)).values(), key=lambda r: r["first"]):
        held = collections.Counter()
        for t0, t1, target in snap["spans"]:
            held[target] += t1 - t0
        total = sum(held.values()) or 1
        share = ", ".join(f"{holder_label(trace, g)} {100.0 * ms / total:.0f}%" for g, ms in held.most_common(4))
        print(f"  Snaplasher {clock(snap['first'])} .. {clock(snap['last'])}: {share}")
    for rec in trace.of("cast"):
        if rec.get("sp") in AREA_TAUNTS and trace.role(rec.get("s")) == "tank" and not rec.get("tr"):
            target = f" on {holder_label(trace, rec['tgt'])}" if rec.get("tgt") else ""
            print(f"  {clock(rec['t'])} {trace.name(rec['s'])} {AREA_TAUNTS[rec['sp']]}{target}")


def mechanic(trace: Trace, death: dict) -> str:
    own = own_blow_spell(death)
    if own:
        return f"own {trace.spells.get(own, f'spell {own}')}"
    rewind = sorted(death.get("rewind") or [], key=lambda row: row[0])
    spell_id = rewind[-1][2] if rewind else 0
    spell = trace.spells.get(spell_id, f"spell {spell_id}") if spell_id else "melee"
    return f"{trace.name(death.get('killer'))} {spell}"


def show_deaths(trace: Trace) -> None:
    print("DEATHS BY MECHANIC")
    if not mine(trace):
        return
    counts = collections.Counter(mechanic(trace, death) for death in combat_deaths(trace))
    if not counts:
        print("  none")
    for name, n in counts.most_common():
        print(f"  {n:3d}  {name}")


SECTIONS = (
    ("waves", "each add wave and Eonar's Gift: when it came, when it ended, raid deaths meanwhile", show_waves),
    ("tanks", "who held Freya and each Snaplasher, and every area taunt or Righteous Defense", show_tanks),
    ("deaths", "deaths by killer and spell", show_deaths),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
