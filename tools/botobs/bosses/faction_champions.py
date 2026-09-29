#!/usr/bin/env python3
"""Score a Faction Champions pull from a RaidObs trace: focus, CC, kicks, burst, fears, dispels, AoE.

    faction_champions.py <file>            every section
    faction_champions.py <file> --kill     each kill target hold: why, how long, health, who was on it
    faction_champions.py <file> --cc       each bot's CC assignment, and where its CC casts went
    faction_champions.py <file> --heals    champion cast-time heals, and the interrupts that followed
    faction_champions.py <file> --burst    lust, each bot's first burst cooldown, the burst vetoes
    faction_champions.py <file> --fear     fears on the raid, Fear Ward and Tremor Totem
    faction_champions.py <file> --dispel   champion CC on the raid, the dispels on it, UA backlash
    faction_champions.py <file> --purge    raid offensive dispels on champions, kill target or not
    faction_champions.py <file> --hop      champion shields, fc.physical holds, Mass Dispel, the return
    faction_champions.py <file> --aoe      raid victims per champion crowd-AoE cast
    faction_champions.py <file> --vetoes   the eight Faction Champions multipliers, by action

What the generic views get wrong here, and what this reads instead:

- **No champion is the boss.** All 28 carry the boss flag, the trace is filed under whichever one
  swung first, and the one that matters changes as they die. `fc.kill` names it, so every section
  keys on that.
- **`postmortem.py --threat` says who the script picked, not who held them.** Champions reset
  threat on a timer and weigh it by distance, health and armour, so time on a tank measures nothing
  a tank did.
- **A creature's death is never recorded.** `death` wants a player and the sweep drops a dead
  creature, so a champion died when it left the snapshots while the fight went on.
- **Heals and auras on a creature are never recorded.** A heal is its cast start and a kick is its
  own cast start, so both counts are upper bounds. Divine Shield, Ice Block or a Cyclone on a
  champion shows only as an `immune` switch, the shields and Hand of Protection also as the
  champion's cast, and a purge or Mass Dispel only as its own cast: `--hop` reads a `back` to the
  shielded champion before the shield's own end as the sign it came off early.
- **`fc.kill` and `fc.physical` are raid-wide latches and `fc.switch` an event**, so the `g` on any
  of them is whichever bot's tick made the call. `fc.cc` is per bot, keyed on the bot, `0` on a
  release.
- **Mass Dispel is cast at a point**, so its `cast` row names no target. `--hop` pairs it with a
  shield by time, and `--dispel` counts it apart from the dispels aimed at a raider, splitting off
  the casts inside a champion shield's duration, which are most likely the shield duty's.
- **A removal's `dur` is what was left.** A CC removed with time left was dispelled, broken by
  damage, or its holder died; the trace can't tell which.
- **A crowd-AoE victim is a raid player.** `dmg` rows cover players only, while the champion's 3+
  check counts pets too, so a cast can fire on fewer victims than it needed.
- **Hellfire, Fan of Knives, Arcane Explosion and the Unstable Affliction backlash remap**, so every
  id of their spelldifficulty row counts.
"""
from __future__ import annotations

import bisect
import collections
import functools
import json
import pathlib
import re
import statistics
import sys

# Run as a script from bosses/, so the raidobs package one level up is not on the path yet.
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from raidobs.cli import run_sections  # noqa: E402
from raidobs.encounter import encounter_of  # noqa: E402
from raidobs.geometry import frames  # noqa: E402
from raidobs.paths import SRC_ROOT  # noqa: E402
from raidobs.probes import emitted_keys, holder_spans, latch_spans, silent_keys  # noqa: E402
from raidobs.trace import Trace, clock, combat_deaths, notes, roster_guids  # noqa: E402
from raidobs.validity import DIFFICULTY  # noqa: E402

ENCOUNTER = "faction-champions"

# Kill order, which is also the CC order: (spec, healer, Alliance entry, Horde entry).
CHAMPIONS = (
    ("Holy Paladin", True, 34465, 34445),
    ("Disc Priest", True, 34466, 34447),
    ("Resto Shaman", True, 34470, 34444),
    ("Resto Druid", True, 34469, 34459),
    ("Rogue", False, 34472, 34454),
    ("Warrior", False, 34475, 34453),
    ("Hunter", False, 34467, 34448),
    ("Enhancement Shaman", False, 34463, 34455),
    ("Death Knight", False, 34461, 34458),
    ("Retribution Paladin", False, 34471, 34456),
    ("Warlock", False, 34474, 34450),
    ("Shadow Priest", False, 34473, 34441),
    ("Mage", False, 34468, 34449),
    ("Balance Druid", False, 34460, 34451),
)
SPEC_OF = {entry: (rank, spec, healer)
           for rank, (spec, healer, *entries) in enumerate(CHAMPIONS) for entry in entries}
# Felhunter, Cat
CHAMPION_PETS = (35465, 35610)

KILL = "fc.kill"
SWITCH = "fc.switch"
CC = "fc.cc"
SWITCH_REASONS = ("first", "dead", "immune", "back", "reset")

# Champion cast-time heals, every difficulty's id: (name, cast ms, ids). The cast row's own `ct` wins
# when set, and a channel's reads 0.
HEAL_SPELLS = (
    ("Lesser Healing Wave", 1500, (66055, 68115, 68116, 68117)),
    ("Nourish", 1500, (66066, 67965, 67966, 67967)),
    ("Regrowth", 2000, (66067, 67968, 67969, 67970)),
    ("Flash Heal", 1500, (66104, 68023, 68024, 68025)),
    ("Holy Light", 2000, (66112, 68011, 68012, 68013)),
    ("Flash of Light", 1300, (66113, 68008, 68009, 68010)),
    ("Tranquility", 10000, (66086, 67974, 67975, 67976)),
)
HEALS = {spell: (name, ms) for name, ms, ids in HEAL_SPELLS for spell in ids}

# Counterspell, Kick, Pummel, Shield Bash, Mind Freeze, Strangulate, Wind Shear, Spell Lock (both
# felhunter ranks), Silence, Silencing Shot, Hammer of Justice (every rank).
INTERRUPTS = frozenset((2139, 1766, 6552, 72, 47528, 47476, 57994, 19244, 19647, 15487, 34490,
                        853, 5588, 5589, 10308))

# every rank and Polymorph variant a bot can know
CC_SPELLS = {
    **dict.fromkeys((118, 12824, 12825, 12826, 28271, 28272, 61025, 61305, 61721, 61780), "Polymorph"),
    **dict.fromkeys((5782, 6213, 6215), "Fear"),
    33786: "Cyclone",
    **dict.fromkeys((339, 1062, 5195, 5196, 9852, 9853, 26989, 53308), "Entangling Roots"),
}

# Heroism, Bloodlust. The champions' own are 65983 and 65980.
LUST_SPELLS = (32182, 2825)

FEARS = {65809: "Fear", 65543: "Psychic Scream", 65930: "Intimidating Shout"}
SPELL_FEAR_WARD = 6346
SPELL_TREMOR_TOTEM = 8143
WARDS = {SPELL_FEAR_WARD: "Fear Ward", SPELL_TREMOR_TOTEM: "Tremor Totem"}

HOLD_BURST = "hold burst until tank engaged"
TOC_BURST = "toc burst window"
BURST_VETOES = (HOLD_BURST, TOC_BURST)
FC_MULTIPLIERS = (
    "faction champions suppress aoe multiplier",
    "faction champions target guard multiplier",
    "faction champions threat redirect veto multiplier",
    "faction champions anti fear totem guard multiplier",
    "faction champions aoe guard multiplier",
    "faction champions physical switch guard multiplier",
    "faction champions dispel guard multiplier",
    "faction champions purge guard multiplier",
)
COUNTERSPELL_ACTION = "faction champions counterspell kill target"
ANTI_FEAR_ACTION = "faction champions anti fear"
DISPEL_CC_ACTION = "faction champions dispel cc"
PURGE_ACTION = "faction champions purge kill target"
AVOID_AOE_ACTION = "faction champions avoid aoe"

MAGIC, CURSE, POISON, DISEASE = "magic", "curse", "poison", "disease"

# The CC the dispel duty answers and its dispel type, the two silences on healers only. None of them
# remaps.
COUNTED_CC = {
    65801: ("Polymorph", MAGIC),
    65809: ("Fear", MAGIC),
    65543: ("Psychic Scream", MAGIC),
    66008: ("Repentance", MAGIC),
    66613: ("Hammer of Justice", MAGIC),
    66007: ("Hammer of Justice", MAGIC),
    66054: ("Hex", CURSE),
    65877: ("Wyvern Sting", POISON),
    65542: ("Silence", MAGIC),
    66018: ("Strangulate", MAGIC),
}
HEALER_ONLY_CC = (65542, 66018)
# a removal with more than this left came off early
EARLY_MS = 500

# Every rank a bot casts of each raid dispel, and the types it removes
FRIENDLY_DISPELS = {
    **dict.fromkeys((527, 988), frozenset((MAGIC,))),  # Dispel Magic
    4987: frozenset((MAGIC, POISON, DISEASE)),  # Cleanse
    **dict.fromkeys((475, 2782), frozenset((CURSE,))),  # Remove Curse, mage and druid
    51886: frozenset((CURSE, POISON, DISEASE)),  # Cleanse Spirit
    **dict.fromkeys((2893, 8946), frozenset((POISON,))),  # Abolish Poison, Cure Poison
    526: frozenset((POISON, DISEASE)),  # Cure Toxins
}
SPELL_MASS_DISPEL = 32375
# spelldifficulty row, 10N/25N/10H/25H
UA_BACKLASH = (65813, 68157, 68158, 68159)

OFFENSIVE_DISPELS = {
    **dict.fromkeys((370, 8012), "Purge"),
    **dict.fromkeys((527, 988), "Dispel Magic"),
    30449: "Spellsteal",
    19801: "Tranquilizing Shot",
    # the felhunter's ranks 1-7
    **dict.fromkeys((19505, 19731, 19734, 19736, 27276, 27277, 48011), "Devour Magic"),
}

PHYSICAL = "fc.physical"
SPELL_HAND_OF_PROTECTION = 66009
SPELL_DIVINE_SHIELD = 66010
SPELL_ICE_BLOCK = 65802
# spell -> (name, lasts ms)
SHIELDS = {
    SPELL_HAND_OF_PROTECTION: ("Hand of Protection", 10000),
    SPELL_DIVINE_SHIELD: ("Divine Shield", 12000),
    SPELL_ICE_BLOCK: ("Ice Block", 5000),
}
# the shields that suspend fc.kill. Hand of Protection leaves magic open, so the latch stays put
SUSPENDING_SHIELDS = (SPELL_DIVINE_SHIELD, SPELL_ICE_BLOCK)
# fc.kill returns on the first refresh the champion is attackable, so give the shield's end some room
BACK_SLACK_MS = 2000

# (name, cast ids, victim ids, victim record, window ms from the cast start). A victim is a `dmg` row,
# or an aura apply for the two fears. Every spelldifficulty id of a row counts.
CROWD_AOE = (
    ("Bladestorm", (65947,), (65946,), "dmg", 9000),
    ("Hellfire", (65816, 68145, 68146, 68147), (65817, 68142, 68143, 68144), "dmg", 16000),
    ("Fan of Knives", (65955, 68097, 68098, 68099), (65955, 68097, 68098, 68099), "dmg", 1000),
    ("Divine Storm", (66006,), (66006,), "dmg", 1000),
    ("Arcane Explosion", (65800, 68000, 68001, 68002), (65800, 68000, 68001, 68002), "dmg", 1000),
    ("Frost Nova", (65792,), (65792,), "dmg", 1000),
    ("Psychic Scream", (65543,), (65543,), "aura", 1000),
    ("Intimidating Shout", (65930,), (65930,), "aura", 1000),
)
# ticking once a second for the whole window, so hits per victim mean something
CHANNELS = ("Bladestorm", "Hellfire")

BURST_SOURCE = SRC_ROOT / "Ai" / "Base" / "Combat" / "BurstCooldowns.cpp"

# The recorder writes `c` as the WoW class id
CLASS_NAMES = {1: "warrior", 2: "paladin", 3: "hunter", 4: "rogue", 5: "priest", 6: "death knight",
               7: "shaman", 8: "mage", 9: "warlock", 11: "druid"}

# fc.kill and fc.switch are written in one refresh, in either order
SWITCH_SLACK_MS = 500
# a creature gone from the snapshots this long before the last one died; a stall gap runs ~2 s
GONE_SLACK_MS = 2000


@functools.lru_cache(maxsize=2)
def burst_actions(path: pathlib.Path = BURST_SOURCE) -> frozenset[str]:
    """The action names the burst gates count as cooldowns, read off `burstCooldownNames` so a new
    one shows up here without a second edit."""
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return frozenset()
    body = re.search(r"burstCooldownNames\s*=\s*\{(.*?)\};", text, re.S)
    return frozenset(re.findall(r'"([^"]+)"', body.group(1))) if body else frozenset()


def pull_end(trace: Trace) -> int:
    ends = trace.of("end")
    if ends:
        return ends[-1]["t"]
    return trace.records[-1].get("t", 0) if trace.records else 0


def pull_start(trace: Trace) -> int:
    pulls = trace.of("pull")
    return pulls[0]["t"] if pulls else 0


def as_guid(value) -> int:
    text = str(value).strip()
    return int(text) if text.isdigit() else 0


def champions(trace: Trace) -> dict[int, int]:
    """guid -> entry for every champion the trace named, pets left out."""
    return {guid: entry for guid, entry in trace.entries.items() if entry in SPEC_OF}


def spec(trace: Trace, guid: int) -> str:
    found = SPEC_OF.get(trace.entries.get(guid))
    return found[1] if found else "-"


def lineup(trace: Trace) -> list[tuple[int, str, bool]]:
    """`(guid, spec, healer)` per champion, in kill order."""
    rows = [(SPEC_OF[entry], guid) for guid, entry in champions(trace).items()]
    return [(guid, found[1], found[2]) for found, guid in sorted(rows, key=lambda row: (row[0][0], row[1]))]


def class_name(value) -> str:
    if isinstance(value, str) and not value.isdigit():
        return value
    try:
        return CLASS_NAMES.get(int(value), str(value))
    except (TypeError, ValueError):
        return "?"


@functools.lru_cache(maxsize=4)
def classes(trace: Trace) -> dict[int, str]:
    """guid -> class name, from the header roster and from `unit` rows for anyone who joined later.
    `Trace` keeps no class, so the unit rows are read off the file again."""
    out = {}
    try:
        with trace.path.open("r", encoding="utf-8", errors="replace") as fh:
            for line in fh:
                if '"unit"' not in line:
                    continue
                try:
                    rec = json.loads(line)
                except json.JSONDecodeError:
                    continue
                if rec.get("e") == "unit" and "c" in rec:
                    out[rec["g"]] = class_name(rec["c"])
    except OSError:
        pass
    out.update((member["g"], class_name(member.get("c"))) for member in trace.header.get("roster", []))
    return out


def raid_caster(trace: Trace, guid) -> int | None:
    """The roster member behind a cast: the caster, or the owner of its pet."""
    roster = roster_guids(trace)
    if guid in roster:
        return guid
    owner = trace.owners.get(guid)
    return owner if owner in roster else None


def held_at(spans, when: int) -> int:
    """The guid a `(guid, start, stop)` span list held at a time, else 0."""
    for guid, start, stop in spans:
        if start <= when < stop:
            return guid
    return 0


def kill_spans(trace: Trace) -> list[tuple[int, int, int]]:
    """`(guid, start, stop)` per value `fc.kill` held, a cleared latch as guid 0."""
    return [(as_guid(value), start, stop) for value, start, stop in latch_spans(trace, KILL, pull_end(trace))]


def cc_holds(trace: Trace) -> dict[int, list[tuple[int, int, int]]]:
    """Per bot, `(target, start, stop)` for each `fc.cc` assignment it held."""
    out = {}
    for bot, spans in holder_spans(trace, CC, pull_end(trace)).items():
        held = [(as_guid(value), start, stop) for value, start, stop in spans if as_guid(value)]
        if held:
            out[bot] = held
    return out


def aura_spans(trace: Trace, spell: int) -> dict[int, list[tuple[int, int]]]:
    """Per target, apply to remove. An aura still up when the file ends runs to the end."""
    end = pull_end(trace)
    opened: dict[int, int] = {}
    out: dict[int, list[tuple[int, int]]] = collections.defaultdict(list)
    for rec in trace.of("aura"):
        if rec.get("sp") != spell:
            continue
        guid = rec.get("d", 0)
        if rec.get("r"):
            if guid in opened:
                out[guid].append((opened.pop(guid), rec["t"]))
        elif guid not in opened:
            opened[guid] = rec["t"]
    for guid, start in opened.items():
        out[guid].append((start, end))
    return out


class Samples:
    """Snapshot rows by guid, so a view can ask for one unit over a span without rescanning."""

    def __init__(self, trace: Trace):
        self.stamps: list[int] = []
        self.rows: dict[int, tuple[list[int], list[list]]] = {}
        for snap in frames(trace):
            self.stamps.append(snap["t"])
            for row in snap.get("u", []):
                stamps, rows = self.rows.setdefault(row[0], ([], []))
                stamps.append(snap["t"])
                rows.append(row)

    def between(self, guid: int, low: int, high: int) -> list[list]:
        """Rows in `[low, high)`."""
        found = self.rows.get(guid)
        if not found:
            return []
        return found[1][bisect.bisect_left(found[0], low):bisect.bisect_left(found[0], high)]

    def last_seen(self, guid: int) -> int | None:
        found = self.rows.get(guid)
        return found[0][-1] if found else None


def missing_probes(trace: Trace) -> list[str]:
    """`fc.` keys the source declares and this pull never wrote; raid-wide `toc.` keys are not ours."""
    return [key for key, _, _ in silent_keys(emitted_keys(trace), encounter_of(trace))
            if key.startswith("fc.")]


def show_banner(trace: Trace) -> None:
    end = pull_end(trace)
    outcome = trace.of("end")[-1].get("out", "?") if trace.of("end") else "no end record"
    encounter = encounter_of(trace)
    mode = DIFFICULTY.get(trace.header.get("diff"), f"difficulty {trace.header.get('diff')}")
    print(f"{encounter}  {mode}  {outcome} at {clock(end)}  {len(combat_deaths(trace))} death(s)")

    # the probe check matches keys to the encounter, so on another boss it would pass on nothing
    if encounter != ENCOUNTER:
        print("  not a Faction Champions pull, every section below reads empty")
        return

    rows = lineup(trace)
    healers = sum(1 for _, _, healer in rows if healer)
    print(f"lineup, kill order: {healers} healer(s), {len(rows) - healers} dps")
    for guid, name, healer in rows:
        print(f"  {'healer' if healer else 'dps':6} {name:20} {trace.name(guid)}")
    pets = sorted(guid for guid, entry in trace.entries.items() if entry in CHAMPION_PETS)
    if pets:
        print(f"  pets, never picked: {', '.join(trace.name(guid) for guid in pets)}")

    gone = missing_probes(trace)
    print("all fc.* probes present" if not gone else f"probes absent from this trace: {', '.join(gone)}")


def kill_holds(trace: Trace) -> list[dict]:
    """Each span `fc.kill` held a champion, with the switch that opened it and what the raid did."""
    samples = Samples(trace)
    last = samples.stamps[-1] if samples.stamps else pull_end(trace)
    switches = sorted((rec["t"], str(rec.get("txt", ""))) for rec in notes(trace, SWITCH))
    roster = roster_guids(trace)
    # the latch steers bots only, so healers and humans stay out of the share
    focus = {guid for guid in roster if trace.role(guid) != "heal" and guid not in trace.humans}
    snaps = frames(trace)
    end = pull_end(trace)
    # a kill leaves the last hold open to the end, and that champion's dead either way
    killed = bool(trace.of("end")) and trace.of("end")[-1].get("out") == "kill"

    out = []
    for guid, start, stop in kill_spans(trace):
        if not guid:
            continue
        near = [(abs(when - start), reason) for when, reason in switches
                if abs(when - start) <= SWITCH_SLACK_MS]
        rows = samples.between(guid, start, stop)
        gone = samples.last_seen(guid)

        on = total = 0
        for snap in snaps:
            if not start <= snap["t"] < stop:
                continue
            for row in snap.get("u", []):
                if row[0] in focus and len(row) > 7 and row[5] > 0:
                    total += 1
                    on += row[7] == guid

        out.append({
            "guid": guid,
            "start": start,
            "stop": stop,
            "reason": min(near)[1] if near else None,
            "hp_start": rows[0][5] if rows else None,
            "hp_end": rows[-1][5] if rows else None,
            "died": (killed and stop >= end)
                    or (gone is not None and gone + GONE_SLACK_MS < last and gone <= stop + GONE_SLACK_MS),
            "share": on / total if total else None,
        })
    return out


def show_kill(trace: Trace) -> None:
    print("KILL TARGET")
    holds = kill_holds(trace)
    if not holds:
        print("  fc.kill never held a champion")
        return

    print(f"  {'at':>9} {'length':>7} {'target':20} {'spec':20} {'why':7} {'hp start>end':>13}"
          f" {'died':>5} {'on it':>6}")
    for hold in holds:
        hp = ("-" if hold["hp_start"] is None
              else f"{hold['hp_start']:.0f} > {hold['hp_end']:.0f}")
        share = "-" if hold["share"] is None else f"{hold['share'] * 100:.0f}%"
        print(f"  {clock(hold['start']):>9} {(hold['stop'] - hold['start']) / 1000:6.1f}s"
              f" {trace.name(hold['guid'])[:20]:20} {spec(trace, hold['guid'])[:20]:20}"
              f" {hold['reason'] or 'carried':7} {hp:>13} {'yes' if hold['died'] else '-':>5} {share:>6}")

    reasons = collections.Counter(str(rec.get("txt", "")) for rec in notes(trace, SWITCH))
    ordered = ([reason for reason in SWITCH_REASONS if reasons[reason]]
               + sorted(reason for reason in reasons if reason not in SWITCH_REASONS))
    print("\n  fc.switch: " + (", ".join(f"{reason} {reasons[reason]}" for reason in ordered) or "none"))
    print("  on it is the share of non-healer bot samples whose target was the kill target; why dead"
          " includes the target leaving combat; died is the champion leaving the snapshots mid-fight,"
          " or the hold still open at a kill")


def cc_casts(trace: Trace) -> dict[int, dict]:
    """Per roster caster, its CC casts on champions and where they landed against the assignment."""
    targets = champions(trace)
    holds = cc_holds(trace)
    kill = kill_spans(trace)
    roster = roster_guids(trace)
    out: dict[int, dict] = {}
    for rec in trace.of("cast"):
        caster, target = rec.get("s"), rec.get("tgt")
        if rec.get("tr") or rec.get("sp") not in CC_SPELLS or target not in targets or caster not in roster:
            continue
        row = out.setdefault(caster, {"casts": 0, "assigned": 0, "kill": 0, "spells": collections.Counter()})
        row["casts"] += 1
        row["assigned"] += held_at(holds.get(caster, []), rec["t"]) == target
        row["kill"] += held_at(kill, rec["t"]) == target
        row["spells"][CC_SPELLS[rec["sp"]]] += 1
    return out


def show_cc(trace: Trace) -> None:
    print("CC")
    holds = cc_holds(trace)
    klass = classes(trace)
    if holds:
        print("  fc.cc assignments:")
        for bot in sorted(holds, key=trace.name):
            for target, start, stop in holds[bot]:
                print(f"    {trace.name(bot)[:14]:14} {klass.get(bot, '?'):8} {clock(start):>9}"
                      f" {(stop - start) / 1000:6.1f}s  {trace.name(target)} ({spec(trace, target)})")
    else:
        print("  fc.cc never assigned anyone")

    rows = cc_casts(trace)
    if not rows:
        print("  no Polymorph, Fear, Cyclone or Entangling Roots cast on a champion")
        return
    print(f"\n  {'bot':14} {'class':8} {'casts':>5} {'on assigned':>12} {'on kill':>8}  spells")
    for bot in sorted(rows, key=trace.name):
        row = rows[bot]
        print(f"  {trace.name(bot)[:14]:14} {klass.get(bot, '?'):8} {row['casts']:5}"
              f" {row['assigned'] * 100.0 / row['casts']:11.0f}% {row['kill']:8}  "
              + ", ".join(f"{name} {count}" for name, count in row["spells"].most_common()))


def heal_rows(trace: Trace) -> list[dict]:
    """Every champion cast-time heal, and the first roster or pet interrupt aimed at its caster
    inside the cast."""
    targets = champions(trace)
    kill = kill_spans(trace)
    interrupts = [(rec["t"], rec.get("tgt"), raid_caster(trace, rec.get("s")))
                  for rec in trace.of("cast") if rec.get("sp") in INTERRUPTS]
    interrupts = [row for row in interrupts if row[2] is not None]

    out = []
    for rec in trace.of("cast"):
        caster = rec.get("s")
        if rec.get("tr") or rec.get("sp") not in HEALS or caster not in targets:
            continue
        start = rec["t"]
        name, ms = HEALS[rec["sp"]]
        window = rec.get("ct") or ms
        by = next((who for when, tgt, who in interrupts if tgt == caster and start <= when <= start + window),
                  None)
        out.append({"t": start, "caster": caster, "spell": rec["sp"], "name": name, "window": window,
                    "kill": held_at(kill, start) == caster, "by": by})
    return out


def show_heals(trace: Trace) -> None:
    print("HEALS")
    rows = heal_rows(trace)
    if not rows:
        print("  no champion cast-time heal started")
        return

    followed = [row for row in rows if row["by"] is not None]
    print(f"  {len(rows)} cast-time heal(s) started, {len(followed)} followed by an interrupt inside the cast")
    for label, kill in (("the kill target", True), ("another champion", False)):
        part = [row for row in rows if row["kill"] == kill]
        print(f"  on {label}: {sum(1 for row in part if row['by'] is not None)} of {len(part)} followed")

    klass = classes(trace)
    by_class = collections.Counter(klass.get(row["by"], "?") for row in followed)
    if by_class:
        print("  first interrupter by class: " + ", ".join(f"{name} {count}" for name, count in by_class.most_common()))

    per_spell: dict[str, list[int]] = collections.defaultdict(lambda: [0, 0])
    for row in rows:
        entry = per_spell[row["name"]]
        entry[0] += 1
        entry[1] += row["by"] is not None
    print("  " + ", ".join(f"{name} {started} ({stopped} followed)"
                           for name, (started, stopped) in sorted(per_spell.items())))

    print(f"  {COUNTERSPELL_ACTION}: {ok_rows(trace, COUNTERSPELL_ACTION)} OK row(s)")
    print("  a cast row is a start and nothing records a kick landing, so started bounds heals landed"
          " and followed bounds heals stopped")


def burst_rows(trace: Trace) -> tuple[dict | None, dict[int, tuple[int, str]], collections.Counter]:
    """The first raid lust from the pull, each bot's first burst cooldown, and the burst vetoes."""
    start = pull_start(trace)
    roster = roster_guids(trace)
    lust = next((rec for rec in trace.of("cast")
                 if rec.get("sp") in LUST_SPELLS and rec.get("s") in roster and not rec.get("tr")
                 and rec["t"] >= start), None)

    names = burst_actions()
    first: dict[int, tuple[int, str]] = {}
    for rec in trace.of("act"):
        if rec.get("vd") == "OK" and rec.get("a") in names and rec["t"] >= start:
            first.setdefault(rec["g"], (rec["t"], rec["a"]))

    vetoes = collections.Counter((rec.get("m"), rec.get("a")) for rec in trace.of("veto")
                                 if rec.get("m") in BURST_VETOES)
    return lust, first, vetoes


def show_burst(trace: Trace) -> None:
    print("BURST")
    start = pull_start(trace)
    lust, first, vetoes = burst_rows(trace)
    if lust:
        print(f"  lust: {trace.spell(lust['sp'])} by {trace.name(lust.get('s'))} at {clock(lust['t'])}"
              f" ({(lust['t'] - start) / 1000:+.1f} s from the pull)")
    else:
        print("  no raid lust cast")

    klass = classes(trace)
    if first:
        print("  first burst cooldown per bot:")
        for guid, (when, action) in sorted(first.items(), key=lambda item: item[1][0]):
            print(f"    {trace.name(guid)[:14]:14} {klass.get(guid, '?'):8} {action:22} {clock(when):>9}"
                  f" ({(when - start) / 1000:+.1f} s)")
    else:
        print("  no bot ran a burst cooldown")
    never = sorted(trace.name(guid) for guid in roster_guids(trace)
                   if guid not in first and guid not in trace.humans)
    if first and never:
        print(f"  never: {', '.join(never)}")

    if vetoes:
        print("  burst vetoes (rows):")
        for (multiplier, action), count in sorted(vetoes.items(), key=lambda item: (-item[1], item[0])):
            print(f"    {count:5}  {multiplier} -> {action}")
    else:
        print(f"  no {HOLD_BURST} or {TOC_BURST} veto")


def fear_rows(trace: Trace) -> tuple[dict[int, dict], dict[int, collections.Counter]]:
    """Per fear, how many landed on the raid and how long they held; per ward spell, casts by bot."""
    roster = roster_guids(trace)
    fears = {}
    for spell in FEARS:
        spans = [span for guid, found in aura_spans(trace, spell).items() if guid in roster for span in found]
        fears[spell] = {"count": len(spans), "held": sum(stop - start for start, stop in spans)}

    wards: dict[int, collections.Counter] = {spell: collections.Counter() for spell in WARDS}
    for rec in trace.of("cast"):
        if rec.get("sp") in wards and rec.get("s") in roster and not rec.get("tr"):
            wards[rec["sp"]][rec["s"]] += 1
    return fears, wards


def show_fear(trace: Trace) -> None:
    print("FEARS")
    fears, wards = fear_rows(trace)
    for spell, name in FEARS.items():
        row = fears[spell]
        if row["count"]:
            print(f"  {name} {spell}: {row['count']} on the raid, {row['held'] / 1000:.1f} s held")
        else:
            print(f"  {name} {spell}: none on the raid")
    for spell, casts in wards.items():
        who = ", ".join(f"{trace.name(guid)} {count}" for guid, count in casts.most_common())
        print(f"  {WARDS[spell]} {spell}: {who or 'never cast'}")
    print(f"  {ANTI_FEAR_ACTION}: {ok_rows(trace, ANTI_FEAR_ACTION)} OK row(s)")


def ok_rows(trace: Trace, action: str) -> int:
    return sum(1 for rec in trace.of("act") if rec.get("a") == action and rec.get("vd") == "OK")


def cc_auras(trace: Trace) -> list[dict]:
    """Every counted CC on a roster member, in apply order: `member`, `spell`, `start`, `stop`, `left`
    (ms left at the removal, None when unknown or still up at the end), and the first raid dispel cast
    on the member inside it that can remove its type (`at`, `by`), else None."""
    roster = roster_guids(trace)
    end = pull_end(trace)
    opened: dict[tuple[int, int], int] = {}
    spans = []
    for rec in trace.of("aura"):
        spell, member = rec.get("sp"), rec.get("d", 0)
        if spell not in COUNTED_CC or member not in roster:
            continue
        if spell in HEALER_ONLY_CC and trace.role(member) != "heal":
            continue
        if rec.get("r"):
            if (member, spell) in opened:
                spans.append((opened.pop((member, spell)), member, spell, rec["t"], rec.get("dur")))
        elif (member, spell) not in opened:
            opened[(member, spell)] = rec["t"]
    spans += [(start, member, spell, end, None) for (member, spell), start in opened.items()]

    dispels = [(rec["t"], rec.get("tgt"), rec.get("s"), FRIENDLY_DISPELS[rec["sp"]]) for rec in trace.of("cast")
               if rec.get("sp") in FRIENDLY_DISPELS and rec.get("s") in roster and not rec.get("tr")]
    out = []
    for start, member, spell, stop, left in sorted(spans):
        kind = COUNTED_CC[spell][1]
        # an instant dispel and the removal it causes share a stamp
        first = next(((when, who) for when, target, who, removes in dispels
                      if target == member and start <= when <= stop and kind in removes), (None, None))
        out.append({"member": member, "spell": spell, "start": start, "stop": stop, "left": left,
                    "at": first[0], "by": first[1]})
    return out


def mass_dispels_under_cc(trace: Trace, auras: list[dict] | None = None) -> tuple[int, int]:
    """Raid Mass Dispel casts started while a counted CC held anyone on the roster, and how many of
    those started inside a champion shield's duration, where the shield duty is the likelier cause."""
    auras = cc_auras(trace) if auras is None else auras
    roster = roster_guids(trace)
    shields = [(row["t"], row["t"] + SHIELDS[row["spell"]][1]) for row in shield_rows(trace)]
    starts = [rec["t"] for rec in trace.of("cast")
              if rec.get("sp") == SPELL_MASS_DISPEL and rec.get("s") in roster and not rec.get("tr")
              and any(aura["start"] <= rec["t"] <= aura["stop"] for aura in auras)]
    inside = sum(1 for when in starts if any(start <= when < stop for start, stop in shields))
    return len(starts), inside


def backlashes(trace: Trace) -> list[tuple[int, int]]:
    """`(t, member)` per Unstable Affliction backlash landing on the roster."""
    roster = roster_guids(trace)
    return sorted((start, guid) for spell in UA_BACKLASH for guid, spans in aura_spans(trace, spell).items()
                  if guid in roster for start, _ in spans)


def show_dispel(trace: Trace) -> None:
    print("DISPEL")
    auras = cc_auras(trace)
    if not auras:
        print("  no counted CC on the raid")
    else:
        print(f"  {'counted CC on the raid':26} {'auras':>5} {'held':>7} {'removed early':>14} {'drew a dispel':>14}")
        for spell in sorted({aura["spell"] for aura in auras}):
            part = [aura for aura in auras if aura["spell"] == spell]
            held = sum(aura["stop"] - aura["start"] for aura in part) / 1000
            early = sum(1 for aura in part if aura["left"] is not None and aura["left"] > EARLY_MS)
            drew = sum(1 for aura in part if aura["at"] is not None)
            print(f"  {COUNTED_CC[spell][0] + ' ' + str(spell):26} {len(part):5} {held:6.1f}s {early:14} {drew:14}")

        klass = classes(trace)
        latency: dict[str, list[int]] = collections.defaultdict(list)
        for aura in auras:
            if aura["at"] is not None:
                latency[klass.get(aura["by"], "?")].append(aura["at"] - aura["start"])
        if latency:
            print("  first dispel cast inside each, by class: " + ", ".join(
                f"{name} {len(waits)} (median {statistics.median(waits) / 1000:.1f} s, max {max(waits) / 1000:.1f} s)"
                for name, waits in sorted(latency.items(), key=lambda item: -len(item[1]))))
        else:
            print("  no dispel cast on a raider under counted CC")

    under_cc, in_shield = mass_dispels_under_cc(trace, auras)
    print(f"  Mass Dispel: {under_cc} cast(s) while counted CC held a raider, {in_shield} of them inside a"
          " champion shield window, none counted above since a point cast names nobody")
    hits = backlashes(trace)
    who = collections.Counter(trace.name(guid) for _, guid in hits)
    print(f"  Unstable Affliction backlash on the raid: {len(hits)}"
          + (f" ({', '.join(f'{name} {count}' for name, count in who.most_common())})" if hits else ""))
    print(f"  {DISPEL_CC_ACTION}: {ok_rows(trace, DISPEL_CC_ACTION)} OK row(s)")
    print(f"  removed early is a removal with over {EARLY_MS} ms left: dispelled, broken by damage, or"
          " its holder died")


def purge_rows(trace: Trace) -> list[dict]:
    """Every raid or pet offensive dispel cast on a champion: `t`, `by` (the pet's owner for a pet),
    `spell`, and whether the target held `fc.kill` then."""
    targets = champions(trace)
    kill = kill_spans(trace)
    out = []
    for rec in trace.of("cast"):
        target = rec.get("tgt")
        if rec.get("tr") or rec.get("sp") not in OFFENSIVE_DISPELS or target not in targets:
            continue
        by = raid_caster(trace, rec.get("s"))
        if by is None:
            continue
        out.append({"t": rec["t"], "by": by, "spell": rec["sp"], "kill": held_at(kill, rec["t"]) == target})
    return out


def show_purge(trace: Trace) -> None:
    print("PURGE")
    rows = purge_rows(trace)
    if not rows:
        print("  no offensive dispel cast on a champion")
    else:
        on_kill = sum(1 for row in rows if row["kill"])
        print(f"  {len(rows)} offensive dispel(s) on champions, {on_kill} on the kill target")
        klass = classes(trace)
        counts = collections.Counter((row["spell"], klass.get(row["by"], "?"), row["kill"]) for row in rows)
        print(f"    {'spell':26} {'class':8} {'kill':>5} {'other':>6}")
        for spell, name in sorted({(spell, name) for spell, name, _ in counts}):
            print(f"    {OFFENSIVE_DISPELS[spell] + ' ' + str(spell):26} {name:8}"
                  f" {counts[(spell, name, True)]:5} {counts[(spell, name, False)]:6}")
    print(f"  {PURGE_ACTION}: {ok_rows(trace, PURGE_ACTION)} OK row(s)")
    print("  class is the caster's, the owner's for a pet")


def shield_rows(trace: Trace) -> list[dict]:
    """Every champion Hand of Protection, Divine Shield and Ice Block cast: `t`, `caster`, `spell`,
    `target`, whether that held `fc.kill`, the raid Mass Dispels inside its duration (`mass` the
    first, `masses` how many). For the two that suspend the latch, cast on the kill target: the first
    `back` returning the latch to that champion by the shield's end plus slack (`back`), and whether it
    came before the shield's own end (`early`)."""
    targets = champions(trace)
    kill = kill_spans(trace)
    roster = roster_guids(trace)
    mass = sorted(rec["t"] for rec in trace.of("cast")
                  if rec.get("sp") == SPELL_MASS_DISPEL and rec.get("s") in roster and not rec.get("tr"))
    backs = sorted(rec["t"] for rec in notes(trace, SWITCH) if str(rec.get("txt", "")) == "back")

    def returned_to(guid: int, when: int) -> bool:
        # fc.kill and fc.switch land in one refresh, in either order
        return any(held == guid and abs(opened - when) <= SWITCH_SLACK_MS for held, opened, _ in kill)

    out = []
    for rec in trace.of("cast"):
        caster = rec.get("s")
        if rec.get("tr") or rec.get("sp") not in SHIELDS or caster not in targets:
            continue
        start = rec["t"]
        lasts = SHIELDS[rec["sp"]][1]
        target = rec.get("tgt") or caster
        on_kill = held_at(kill, start) == target
        back = None
        if rec["sp"] in SUSPENDING_SHIELDS and on_kill:
            back = next((when for when in backs
                         if start <= when <= start + lasts + BACK_SLACK_MS and returned_to(target, when)), None)
        inside = [when for when in mass if start <= when < start + lasts]
        out.append({"t": start, "caster": caster, "spell": rec["sp"], "target": target, "kill": on_kill,
                    "mass": inside[0] if inside else None, "masses": len(inside), "back": back,
                    "early": back is not None and back < start + lasts})
    return out


def physical(role: str, klass: str) -> bool:
    """Who the physical switch steers: tanks, melee and hunters."""
    return role in ("tank", "melee") or klass == "hunter"


def physical_holds(trace: Trace) -> list[dict]:
    """Each span `fc.physical` held a champion, and the share of physical bots' samples on it."""
    klass = classes(trace)
    bots = {guid for guid in roster_guids(trace)
            if guid not in trace.humans and physical(trace.role(guid), klass.get(guid, "?"))}
    snaps = frames(trace)
    out = []
    for value, start, stop in latch_spans(trace, PHYSICAL, pull_end(trace)):
        guid = as_guid(value)
        if not guid:
            continue
        on = total = 0
        for snap in snaps:
            if not start <= snap["t"] < stop:
                continue
            for row in snap.get("u", []):
                if row[0] in bots and len(row) > 7 and row[5] > 0:
                    total += 1
                    on += row[7] == guid
        out.append({"guid": guid, "start": start, "stop": stop, "share": on / total if total else None})
    return out


def show_hop(trace: Trace) -> None:
    print("HAND OF PROTECTION AND IMMUNITY SHIELDS")
    rows = shield_rows(trace)
    if rows:
        print(f"  {'at':>9} {'shield':18} {'caster':20} {'on':20} {'kill':>4} {'Mass Dispel':>11} {'back':>12}")
        for row in rows:
            mass = "-" if row["mass"] is None else f"+{(row['mass'] - row['t']) / 1000:.1f}s x{row['masses']}"
            back = ("n/a" if row["spell"] not in SUSPENDING_SHIELDS or not row["kill"]
                    else "-" if row["back"] is None
                    else f"+{(row['back'] - row['t']) / 1000:.1f}s" + (" early" if row["early"] else ""))
            print(f"  {clock(row['t']):>9} {SHIELDS[row['spell']][0]:18} {trace.name(row['caster'])[:20]:20}"
                  f" {trace.name(row['target'])[:20]:20} {'yes' if row['kill'] else '-':>4} {mass:>11} {back:>12}")
    else:
        print("  no champion cast Hand of Protection, Divine Shield or Ice Block")

    holds = physical_holds(trace)
    if holds:
        print(f"\n  {'fc.physical at':>14} {'length':>7} {'target':20} {'spec':20} {'on it':>6}")
        for hold in holds:
            share = "-" if hold["share"] is None else f"{hold['share'] * 100:.0f}%"
            print(f"  {clock(hold['start']):>14} {(hold['stop'] - hold['start']) / 1000:6.1f}s"
                  f" {trace.name(hold['guid'])[:20]:20} {spec(trace, hold['guid'])[:20]:20} {share:>6}")
    else:
        print("  fc.physical never held a champion")
    print("  Mass Dispel: the first raid cast inside the shield's duration, x how many started inside it")
    print(f"  back: the first `back` to the shielded champion by {BACK_SLACK_MS / 1000:.0f} s past the shield's"
          " end, only for the two that suspend fc.kill and only on the kill target; early means before the"
          " shield's own end, so something took it off")
    print("  on it: the share of physical bot samples (tank and melee roles, hunters) whose target was the hold")


def aoe_casts(trace: Trace) -> list[dict]:
    """Every champion crowd-AoE cast, in time order: `t`, `caster`, `name`, and `hits`, roster victim
    to rows from that cast. A row goes to its caster's latest cast of that spell still inside the
    window."""
    targets = champions(trace)
    roster = roster_guids(trace)
    cast_of = {spell: index for index, (_, ids, *_) in enumerate(CROWD_AOE) for spell in ids}
    victim_of = {(kind, spell): index for index, (_, _, ids, kind, _) in enumerate(CROWD_AOE) for spell in ids}

    casts: dict[tuple[int, int], list[dict]] = collections.defaultdict(list)
    out = []
    for rec in trace.of("cast"):
        index = cast_of.get(rec.get("sp"))
        if index is None or rec.get("tr") or rec.get("s") not in targets:
            continue
        row = {"t": rec["t"], "caster": rec["s"], "name": CROWD_AOE[index][0], "hits": collections.Counter()}
        casts[(rec["s"], index)].append(row)
        out.append(row)
    stamps = {key: [row["t"] for row in rows] for key, rows in casts.items()}

    for rec in trace.of("dmg", "aura"):
        index = victim_of.get((rec["e"], rec.get("sp")))
        if index is None or rec.get("d") not in roster or rec.get("r"):
            continue
        key = (rec.get("s"), index)
        found = bisect.bisect_right(stamps.get(key, []), rec["t"]) - 1
        if found >= 0 and rec["t"] < casts[key][found]["t"] + CROWD_AOE[index][4]:
            casts[key][found]["hits"][rec["d"]] += 1
    return out


def show_aoe(trace: Trace) -> None:
    print("CROWD AOE")
    rows = aoe_casts(trace)
    if not rows:
        print("  no champion cast a crowd AoE")
    else:
        print(f"  {'spell':18} {'casts':>5} {'victims per cast':>17}  {'hits per victim':>15}  victims x casts")
        for name, *_ in CROWD_AOE:
            part = [row for row in rows if row["name"] == name]
            if not part:
                continue
            victims = [len(row["hits"]) for row in part]
            spread = collections.Counter(victims)
            per_cast = f"{statistics.mean(victims):.1f}, max {max(victims)}"
            per_victim = "-"
            hits = [count for row in part for count in row["hits"].values()]
            if name in CHANNELS and hits:
                per_victim = f"{statistics.mean(hits):.1f}, max {max(hits)}"
            print(f"  {name:18} {len(part):5} {per_cast:>17}  {per_victim:>15}  "
                  + ", ".join(f"{count} x{casts}" for count, casts in sorted(spread.items())))
    print(f"  {AVOID_AOE_ACTION}: {ok_rows(trace, AVOID_AOE_ACTION)} OK row(s)")
    print("  a victim is a raid player hit by that cast (a `dmg` row), or feared by it for the two fears;"
          " hits per victim is for the two channels, which tick once a second")


def fc_vetoes(trace: Trace) -> collections.Counter:
    return collections.Counter((rec.get("m"), rec.get("a")) for rec in trace.of("veto")
                               if rec.get("m") in FC_MULTIPLIERS)


def show_vetoes(trace: Trace) -> None:
    print("FACTION CHAMPIONS VETOES")
    counts = fc_vetoes(trace)
    if not counts:
        print(f"  none of the {len(FC_MULTIPLIERS)} multipliers vetoed anything")
        return
    for multiplier in FC_MULTIPLIERS:
        rows = sorted(((count, action) for (name, action), count in counts.items() if name == multiplier),
                      reverse=True)
        if rows:
            print(f"  {multiplier}")
            for count, action in rows:
                print(f"    {count:5}  {action}")
    print("  veto rows, not ticks: one is written when the verdict changes and again every 10 s it holds")


SECTIONS = (
    ("kill", "each kill target hold: why, how long, health, who was on it", show_kill),
    ("cc", "CC assignments, and where the CC casts went", show_cc),
    ("heals", "champion cast-time heals and the interrupts that followed", show_heals),
    ("burst", "lust, first burst cooldown per bot, burst vetoes", show_burst),
    ("fear", "fears on the raid, Fear Ward and Tremor Totem", show_fear),
    ("dispel", "champion CC on the raid, the dispels on it, Unstable Affliction backlash", show_dispel),
    ("purge", "raid offensive dispels on champions, kill target or not, by spell and class", show_purge),
    ("hop", "champion shields, fc.physical holds, Mass Dispel after a shield, the return", show_hop),
    ("aoe", "raid victims per champion crowd-AoE cast, hits per victim for the channels", show_aoe),
    ("vetoes", "the eight Faction Champions multipliers, by action", show_vetoes),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
