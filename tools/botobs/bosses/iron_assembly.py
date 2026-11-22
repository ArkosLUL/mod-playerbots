#!/usr/bin/env python3
"""Read an Assembly of Iron pull from a RaidObs trace: kill order and hard mode, healer mana per phase,
Steelbreaker's empowered phase, deaths by mechanic.

    iron_assembly.py <file>            every section
    iron_assembly.py <file> --phases   each member's death, with every healer's mana at that moment
    iron_assembly.py <file> --punches  every Fusion Punch, every phase, with the verdict and the button
    iron_assembly.py <file> --revived  every tank that rose mid pull and the raid buffs put back on it

What the generic views miss here:

- **Hard mode is the kill order, not the toggle.** `hdr.cfg.hardmode` only says the server would allow
  it; the hard mode loot needs Steelbreaker to die last. The `ironassembly.alive` mask (bit 0
  Steelbreaker, 1 Molgeim, 2 Brundir) stops at the last member standing, so a last value of 1 on a kill
  is a hard mode kill.
- **Each phase-3 death feeds Steelbreaker.** Electrical Charge is +25% damage and a heal of ~9.7% of
  his bar per player death, a Meltdown included, so the count of charges is the clock the raid races.
- **High Voltage kills, but rarely starts it.** It hits every member every 3 s and scales with the
  charges, so the cascade's cause is whatever raised the count first, or the healers running dry.
"""
from __future__ import annotations

import bisect
import collections
import pathlib
import sys

# Run as a script from bosses/, so the raidobs package one level up is not on the path yet.
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from raidobs.cli import run_sections  # noqa: E402
from raidobs.encounter import encounter_of  # noqa: E402
from raidobs.geometry import dist2, frames  # noqa: E402
from raidobs.trace import Trace, clock, combat_deaths, notes, own_blow_spell  # noqa: E402

ENCOUNTER = "iron-assembly"

NPC_STEELBREAKER = 32867
NPC_MOLGEIM = 32927
NPC_BRUNDIR = 32857
MEMBERS = {NPC_STEELBREAKER: "Steelbreaker", NPC_MOLGEIM: "Molgeim", NPC_BRUNDIR: "Brundir"}
MASK_BITS = ((1, "Steelbreaker"), (2, "Molgeim"), (4, "Brundir"))

SPELL_FUSION_PUNCH = (61903, 63493)
SPELL_OVERWHELMING_POWER = (61888, 64637)
SPELL_ELECTRICAL_CHARGE = 61902
SPELL_REBIRTH = (48477, 26994, 20748, 20747, 20742, 20739, 20484)
# Each death is stamped on its own bot's tick, so a blast's victims land up to ~0.3 s either side of the
# carrier's aura ending.
BLAST_WINDOW_MS = 500

# Fusion Punch's 3 s cast plus a tick's slack: a verdict or a button noted this close before the hit was for it.
PUNCH_CAST_WINDOW_MS = 3500

# Anchor + 30 yd at 315 degrees, ULDUAR_IRON_ASSEMBLY_MELTDOWN_POINT in the strategy.
MELTDOWN_POINT = (1608.39, 99.81)
# A healer this dry casts nothing that matters to the next High Voltage tick.
DRY_MANA_PCT = 10.0
# Hymn of Hope's cast, not the 64904 buff it pulses. Innervate is read on its target.
SPELL_INNERVATE = 29166
MANA_RETURNS = {34433: "Shadowfiend", 64901: "Hymn of Hope", 16190: "Mana Tide Totem", SPELL_INNERVATE: "Innervate",
                54428: "Divine Plea"}

# ULDUAR_IRON_ASSEMBLY_REBUFF_WINDOW_MS: how long after a tank rises the strategy puts its raid buffs back.
REBUFF_WINDOW_MS = 30000
# A group buff and its single counterpart are one buff to the tank. Blessings fold Greater into the single.
RAID_BUFFS = {"Mark of the Wild": "Mark of the Wild", "Gift of the Wild": "Mark of the Wild",
              "Power Word: Fortitude": "Fortitude", "Prayer of Fortitude": "Fortitude",
              "Arcane Intellect": "Arcane Intellect", "Arcane Brilliance": "Arcane Intellect"}
HELD_AT_DEATH_MS = 250
CLASS_PALADIN = 2


def mine(trace: Trace) -> bool:
    return encounter_of(trace) == ENCOUNTER


def alive_rows(trace: Trace) -> list[tuple[int, int]]:
    """(t, mask) per `ironassembly.alive` row that parses."""
    out = []
    for rec in notes(trace, "ironassembly.alive"):
        text = str(rec.get("txt", "")).strip()
        if text.isdigit():
            out.append((rec["t"], int(text)))
    return out


def transitions(trace: Trace) -> list[tuple[int, int, int]]:
    """(t, before, after) for each change of the mask, repeated rows of one value folded."""
    out = []
    last = None
    for t, mask in alive_rows(trace):
        if last is not None and mask != last:
            out.append((t, last, mask))
        last = mask
    return out


def names_of(mask: int) -> list[str]:
    return [name for bit, name in MASK_BITS if mask & bit]


def last_standing(trace: Trace) -> str | None:
    rows = alive_rows(trace)
    if not rows:
        return None
    left = names_of(rows[-1][1])
    return left[0] if len(left) == 1 else None


def outcome(trace: Trace) -> str:
    ends = trace.of("end")
    return ends[-1].get("out", "?") if ends else "?"


def empowered_at(trace: Trace) -> int | None:
    """When Steelbreaker became the last one up, which is his empowered phase."""
    for t, _, after in transitions(trace):
        if after == 1:
            return t
    return None


def mask_at(trace: Trace, t: int) -> int:
    mask = 7
    for row_t, row_mask in alive_rows(trace):
        if row_t > t:
            break
        mask = row_mask
    return mask


def phase_label(mask: int) -> str:
    left = names_of(mask)
    if len(left) == 3:
        return "all three"
    return " + ".join(left) if left else "none"


def charges_before(trace: Trace, t: int) -> int:
    return sum(1 for rec in trace.of("cast")
               if rec.get("sp") == SPELL_ELECTRICAL_CHARGE and rec["t"] < t)


class Samples:
    """Snapshot hp, mana and position per roster guid, read back at any time."""

    def __init__(self, trace: Trace):
        roster = {member["g"] for member in trace.header.get("roster", [])}
        self.rows = collections.defaultdict(list)
        self.boss = {}
        for snap in frames(trace):
            for row in snap.get("u", []):
                if row[0] in roster:
                    self.rows[row[0]].append((snap["t"], row))
                elif trace.entries.get(row[0]) in MEMBERS:
                    self.boss.setdefault(trace.entries[row[0]], []).append((snap["t"], row))
        self.times = {guid: [t for t, _ in rows] for guid, rows in self.rows.items()}

    def at(self, guid: int, t: int):
        times = self.times.get(guid)
        if not times:
            return None
        index = bisect.bisect_right(times, t) - 1
        return self.rows[guid][index][1] if index >= 0 else None

    def boss_hp(self, entry: int, t: int) -> float | None:
        rows = [row for row_t, row in self.boss.get(entry, []) if row_t <= t]
        return rows[-1][5] if rows else None


def healers(trace: Trace) -> list[int]:
    return [member["g"] for member in trace.header.get("roster", []) if member.get("r") == "heal"]


def mana_line(trace: Trace, samples: Samples, t: int) -> str:
    parts = []
    for guid in healers(trace):
        row = samples.at(guid, t)
        if row is None:
            continue
        parts.append(f"{trace.name(guid)} {row[6]:.0f}%" if row[5] > 0 else f"{trace.name(guid)} dead")
    return ", ".join(parts)


def dry_at(trace: Trace, samples: Samples) -> dict[int, int]:
    """Per healer, the first sample it was alive under DRY_MANA_PCT mana."""
    out = {}
    for guid in healers(trace):
        for t, row in samples.rows.get(guid, []):
            if t >= 0 and row[5] > 0 and len(row) > 6 and row[6] < DRY_MANA_PCT:
                out[guid] = t
                break
    return out


def show_banner(trace: Trace) -> None:
    print(f"ASSEMBLY OF IRON  {trace.path.name}")
    if not mine(trace):
        print("  not an Assembly of Iron pull, every section below reads empty")
        return
    out = outcome(trace)
    last = last_standing(trace)
    toggle = (trace.header.get("cfg", {}).get("hardmode", {}) or {}).get(ENCOUNTER)
    if not alive_rows(trace):
        verdict = "no ironassembly.alive rows, kill order unknown"
    elif last == "Steelbreaker":
        verdict = ("hard mode triggered: Steelbreaker died last" if out == "kill"
                   else "hard mode order: Steelbreaker was the last one up")
    else:
        verdict = f"normal mode order: {last or 'more than one'} was the last one up"
    print(f"  outcome {out}, {verdict} (toggle {'on' if toggle else 'off'})")


def show_phases(trace: Trace) -> None:
    print("PHASES")
    if not mine(trace):
        return
    samples = Samples(trace)
    print(f"  0:00.000  all three up, healer mana {mana_line(trace, samples, 0)}")
    for t, before, after in transitions(trace):
        gone = [name for name in names_of(before) if name not in names_of(after)]
        print(f"  {clock(t)}  {', '.join(gone) or '?'} died, {phase_label(after)} left, "
              f"healer mana {mana_line(trace, samples, t)}")
    empowered = empowered_at(trace)
    ends = trace.of("end")
    if empowered is not None and ends:
        hp = samples.boss_hp(NPC_STEELBREAKER, ends[-1]["t"])
        low = min((row[5] for row_t, row in samples.boss.get(NPC_STEELBREAKER, []) if row_t >= empowered),
                  default=None)
        line = f"  Steelbreaker alone for {(ends[-1]['t'] - empowered) / 1000:.0f}s"
        if low is not None:
            line += f", lowest {low:.1f}%"
        if hp is not None and outcome(trace) != "kill":
            line += f", {hp:.1f}% at the end"
        print(line)
    dry = dry_at(trace, samples)
    if dry:
        shown = ", ".join(f"{trace.name(guid)} {clock(t)}" for guid, t in sorted(dry.items(), key=lambda kv: kv[1]))
        print(f"  healers under {DRY_MANA_PCT:.0f}% mana from: {shown}")
    team = set(healers(trace))
    for rec in trace.of("cast"):
        name = MANA_RETURNS.get(rec.get("sp"))
        target = rec.get("tgt") if rec.get("sp") == SPELL_INNERVATE else rec.get("s")
        if not name or target not in team:
            continue
        row = samples.at(target, rec["t"] - 1)
        mana = f" at {row[6]:.0f}% mana" if row is not None else ""
        on = f" on {trace.name(target)}" if target != rec.get("s") else ""
        print(f"  {clock(rec['t'])}  {name} from {trace.name(rec.get('s'))}{on}{mana}")


def punch_lines(trace: Trace, start: int, deaths: dict, phase: bool = False) -> list[tuple[int, str]]:
    """(t, line) per Fusion Punch from `start`: the strategy's verdict on it (`ironassembly.punch`, what it
    expected to land and whether that left the tank under the floor) and the button it spent."""
    verdict = collections.defaultdict(list)
    for rec in notes(trace, "ironassembly.punch"):
        verdict[rec["g"]].append(rec)
    defensive = collections.defaultdict(list)
    for rec in notes(trace, "ironassembly.punchdef"):
        defensive[rec["g"]].append(rec)
    lines = []
    for rec in trace.of("dmg"):
        if rec.get("sp") not in SPELL_FUSION_PUNCH or rec["t"] < start:
            continue
        target = rec["d"]
        seen = [note for note in verdict[target]
                if 0 <= rec["t"] - note["t"] <= PUNCH_CAST_WINDOW_MS and note.get("txt") != "idle"]
        pick = [note for note in defensive[target] if 0 <= rec["t"] - note["t"] <= PUNCH_CAST_WINDOW_MS]
        killed = any(0 <= d["t"] - rec["t"] <= 200 for d in deaths.get(target, []))
        where = f"{phase_label(mask_at(trace, rec['t']))}: " if phase else ""
        line = (f"  {clock(rec['t'])}  {where}Fusion Punch {trace.name(target)} {rec.get('a', 0) / 1000:.1f}k"
                f" at {rec.get('hp', 0):.0f}%, {charges_before(trace, rec['t'])} charge(s)")
        if rec.get("rs"):
            line += f", {rec['rs'] / 1000:.1f}k resisted"
        if rec.get("ab"):
            line += f", {rec['ab'] / 1000:.1f}k absorbed"
        if seen:
            call, _, expected = str(seen[-1].get("txt", "")).partition(" ")
            line += f", expected {expected} ({'under the floor' if call == 'under' else call})"
        line += f", defensive {pick[-1].get('txt')}" if pick else ", no defensive"
        if killed:
            line += "  KILLED"
        lines.append((rec["t"], line))
    return lines


def show_punches(trace: Trace) -> None:
    """Every Fusion Punch, every phase: before Steelbreaker is alone one can still kill a tank that is low."""
    print("FUSION PUNCHES")
    if not mine(trace):
        return
    deaths = {}
    for death in combat_deaths(trace):
        deaths.setdefault(death["g"], []).append(death)
    lines = punch_lines(trace, 0, deaths, phase=True)
    if not lines:
        print("  none")
    for _, line in sorted(lines, key=lambda item: item[0]):
        print(line)


def show_empowered(trace: Trace) -> None:
    """Overwhelming Power carriers with whoever their blast killed, every Fusion Punch and every Rebirth once
    Steelbreaker is alone."""
    print("STEELBREAKER ALONE")
    if not mine(trace):
        return
    start = empowered_at(trace)
    if start is None:
        print("  never reached")
        return
    deaths = {}
    for death in combat_deaths(trace):
        deaths.setdefault(death["g"], []).append(death)
    lines = []
    for rec in trace.of("aura"):
        if rec.get("sp") not in SPELL_OVERWHELMING_POWER or rec.get("r"):
            continue
        carrier = rec.get("d")
        ended = next((other["t"] for other in trace.of("aura")
                      if other.get("sp") == rec["sp"] and other.get("d") == carrier and other.get("r")
                      and other["t"] > rec["t"]), None)
        fate = "still up at the end"
        if ended is not None:
            died = next((d for d in deaths.get(carrier, []) if abs(d["t"] - ended) <= BLAST_WINDOW_MS), None)
            if died is None:
                fate = f"off at {clock(ended)}, carrier alive"
            elif mechanic(trace, died) == "Meltdown":
                spot = dist2((died.get("x", 0), died.get("y", 0)), MELTDOWN_POINT)
                fate = f"Meltdown at {clock(ended)}, {spot:.0f} yd from the Meltdown point"
                caught = sum(1 for guid, rows in deaths.items() if guid != carrier for d in rows
                             if abs(d["t"] - ended) <= BLAST_WINDOW_MS and mechanic(trace, d) == "Meltdown")
                if caught:
                    fate += f", {caught} more killed"
            else:
                fate = (f"carrier died at {clock(ended)} to {mechanic(trace, died)}, "
                        f"{(ended - rec['t']) / 1000:.0f}s in")
        lines.append((rec["t"], f"  {clock(rec['t'])}  Overwhelming Power on {trace.name(carrier)}: {fate}"))
    lines.extend(punch_lines(trace, start, deaths))
    for rec in trace.of("cast"):
        if rec.get("sp") in SPELL_REBIRTH and rec["t"] >= start:
            target = rec.get("tgt")
            lines.append((rec["t"], f"  {clock(rec['t'])}  Rebirth from {trace.name(rec.get('s'))} on "
                                    f"{trace.name(target)} ({trace.role(target)})"))
    for _, line in sorted(lines, key=lambda item: item[0]):
        print(line)


def raid_buff(name: str | None) -> str | None:
    """The buff a tank loses on dying and the strategy puts back, or None for anything else."""
    if not name:
        return None
    if name.startswith("Greater Blessing of "):
        return name[len("Greater "):]
    return name if name.startswith("Blessing of ") else RAID_BUFFS.get(name)


def held_raid_buffs(trace: Trace, death: dict) -> set[str]:
    """Dying strips every buff, so one held to the end reads removed at the death or a tick before it."""
    return {buff for row in death.get("auras") or []
            if len(row) > 5 and (row[5] == -1 or row[5] >= death["t"] - HELD_AT_DEATH_MS)
            for buff in [raid_buff(trace.spells.get(row[0]))] if buff}


def raid_buffs_since(trace: Trace, guid: int, start: int, until: int) -> set[str]:
    """The raid buffs on `guid` at `until`, from its aura rows since `start` (a rise, so it held none then)."""
    held = set()
    for rec in trace.of("aura"):
        buff = raid_buff(trace.spells.get(rec.get("sp")))
        if rec.get("d") != guid or not buff or not start <= rec["t"] <= until:
            continue
        if rec.get("r"):
            held.discard(buff)
        else:
            held.add(buff)
    return held


def show_revived(trace: Trace) -> None:
    """Every tank that stood up again mid pull, the raid buffs that went back on it inside the rebuff window,
    the `ironassembly.rebuff` verdicts that left one off, and which of the buffs it died with the first time
    it lacked at its next death or the pull's end."""
    print("REVIVED TANKS")
    if not mine(trace):
        return
    samples = Samples(trace)
    deaths = collections.defaultdict(list)
    for death in combat_deaths(trace):
        if trace.role(death["g"]) == "tank":
            deaths[death["g"]].append(death)
    ends = trace.of("end")
    classes = {member["g"]: member.get("c") for member in trace.header.get("roster", [])}
    lines = []
    for guid, rows in deaths.items():
        before = held_raid_buffs(trace, rows[0])
        if classes.get(guid) != CLASS_PALADIN:
            # the strategy only puts it back on a paladin tank
            before.discard("Arcane Intellect")
        for index, death in enumerate(rows):
            later = rows[index + 1] if index + 1 < len(rows) else None
            rose = next(((t, row) for t, row in samples.rows.get(guid, [])
                         if t > death["t"] and row[5] > 0 and (later is None or t < later["t"])), None)
            if rose is None:
                continue
            up, row = rose
            end = up + REBUFF_WINDOW_MS
            lines.append((up, f"  {clock(up)}  {trace.name(guid)} up at {row[5]:.0f}%, dead since {clock(death['t'])}"))
            for rec in trace.of("aura"):
                if rec.get("d") == guid and not rec.get("r") and up <= rec["t"] <= end \
                        and raid_buff(trace.spells.get(rec.get("sp"))):
                    lines.append((rec["t"], f"  {clock(rec['t'])}    {trace.spells[rec['sp']]} from "
                                            f"{trace.name(rec.get('s'))}"))
            for rec in notes(trace, "ironassembly.rebuff"):
                how, _, about = str(rec.get("txt", "")).partition(" ")
                if up <= rec["t"] <= end and about == str(guid) and how in ("none:nobody", "hold:cast"):
                    lines.append((rec["t"], f"  {clock(rec['t'])}    {trace.name(rec.get('g'))}: {how}"))
            if later is not None:
                t, label, held = later["t"], "died again", held_raid_buffs(trace, later)
            elif ends:
                t, label = ends[-1]["t"], "at the end"
                held = raid_buffs_since(trace, guid, up, t)
            else:
                continue
            lost = sorted(before - held)
            lines.append((t, f"  {clock(t)}  {trace.name(guid)} {label}, "
                             f"{'without ' + ', '.join(lost) if lost else 'every raid buff back'}"))
    if not lines:
        print("  none")
    for _, line in sorted(lines, key=lambda item: item[0]):
        print(line)


def mechanic(trace: Trace, death: dict) -> str:
    killer = death.get("killer")
    rewind = death.get("rewind") or []
    spell_id = rewind[-1][2] if rewind else 0
    if killer == death["g"]:
        held = {row[0] for row in death.get("auras") or []}
        if held & set(SPELL_OVERWHELMING_POWER):
            return "Meltdown"
        own = own_blow_spell(death)
        return (trace.spells.get(own) or f"spell {own}") if own else "own blow"
    if spell_id:
        return trace.spells.get(spell_id) or f"spell {spell_id}"
    who = MEMBERS.get(trace.entries.get(killer), trace.name(killer))
    return f"{who} melee on a {'tank' if trace.role(death['g']) == 'tank' else 'non-tank'}"


def show_deaths(trace: Trace) -> None:
    print("DEATHS BY MECHANIC")
    if not mine(trace):
        return
    per_phase = collections.defaultdict(collections.Counter)
    order = []
    for death in combat_deaths(trace):
        label = phase_label(mask_at(trace, death["t"]))
        if label not in order:
            order.append(label)
        per_phase[label][mechanic(trace, death)] += 1
    if not order:
        print("  none")
    for label in order:
        counts = per_phase[label]
        print(f"  {label} up: {sum(counts.values())}")
        for name, n in counts.most_common():
            print(f"    {n:3d}  {name}")
    first = next((d for d in combat_deaths(trace) if mask_at(trace, d["t"]) == 1
                  and mechanic(trace, d) != "Meltdown"), None)
    if first:
        print(f"  first death alone with Steelbreaker that was not a Meltdown: {trace.name(first['g'])} "
              f"({trace.role(first['g'])}) at {clock(first['t'])}, {mechanic(trace, first)}")


SECTIONS = (
    ("phases", "each member's death, with every healer's mana at that moment", show_phases),
    ("empowered", "Overwhelming Power carriers, Fusion Punches and Rebirths once Steelbreaker is alone",
     show_empowered),
    ("punches", "every Fusion Punch in every phase, with the strategy's verdict and the button spent", show_punches),
    ("revived", "every tank that rose mid pull and the raid buffs put back on it", show_revived),
    ("deaths", "deaths by mechanic per phase", show_deaths),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
