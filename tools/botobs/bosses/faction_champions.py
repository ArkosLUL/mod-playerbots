#!/usr/bin/env python3
"""Score a Faction Champions pull from a RaidObs trace: focus, CC, kicks, burst, fears, dispels, AoE, adds.

    faction_champions.py <file>            every section
    faction_champions.py <file> --kill     each kill target hold: why, how long, health, who was on it
    faction_champions.py <file> --cc       CC duties, CC held on each champion, casts started and landed
    faction_champions.py <file> --heals    champion cast-time heals, and the interrupts that followed
    faction_champions.py <file> --burst    lust, Exhaustion carried in, first burst cooldowns, burst vetoes
    faction_champions.py <file> --fear     fears on the raid, Fear Ward and Tremor Totem
    faction_champions.py <file> --dispel   champion CC on the raid, the dispels on it, UA backlash
    faction_champions.py <file> --purge    raid offensive dispels on champions, kill target or not
    faction_champions.py <file> --hop      champion shields, fc.physical holds, Mass Dispel, the return
    faction_champions.py <file> --aoe      raid victims per champion crowd-AoE cast
    faction_champions.py <file> --adds     champion totems and treants, treant damage, fc.totem holds
    faction_champions.py <file> --pack     champions round each role, early melee deaths, first kill, stalls, grips
    faction_champions.py <file> --peel     snares and peels on melee champions, Disarm against Bladestorm,
                                           externals, battle res, taunts
    faction_champions.py <file> --ball     the death ball: who started it and whom the champions picked first,
                                           who they chase, how far the kill target and the raid stand off the
                                           pack, escapees, opener and trinkets
    faction_champions.py <file> --vetoes   the Faction Champions multipliers and rules, by action

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
  champion shows as an `immune` switch, the shields and Hand of Protection also as the champion's
  cast, and a purge or Mass Dispel only as its own cast: `--hop` reads a `back` to the shielded
  champion before the shield's own end as the sign it came off early.
- **`fc.ccaura` is the only record of CC on a champion.** It holds the spell of a player's fear,
  stun or confuse aura on the champion (Cyclone, Polymorph, a stun), `0` when none; a fear also
  shows as snapshot moveGen 9, fleeing. Roots aren't in it. `--cc` reads a Fear's landing off
  moveGen and a Cyclone's or Polymorph's off this key, `n/a` without it and always for Roots. A CC
  broken between two samples shows in neither: on 25N 52 of 67 Fears showed.
- **`fc.kill` and `fc.physical` are raid-wide latches and `fc.switch` an event**, so the `g` on any
  of them is whichever bot's tick made the call. `fc.cc`, `fc.peel` and `fc.totem` are per bot, keyed
  on the bot, `0` on a release; `fc.ccaura` is keyed on the champion. An `fc.cc` hold is one cast: it
  ends when the cast lands or 3 s pass with none going. A `skull` switch is a player's skull
  taking the latch: bots place no skull here, so the `g` is still a bot, never the player who marked.
- **Exhaustion or Sated applied before the trace shows only as its removal.** `--burst` reads a
  member whose first row of either is a removal as carrying it into the pull. A fresh lust writes
  the lust caster's copy as a removal with time left and the member's own apply a few ms later, so
  that pair is no carry.
- **Mass Dispel is cast at a point**, so its `cast` row names no target. `--hop` pairs it with a
  shield by time, and `--dispel` counts it apart from the dispels aimed at a raider, splitting off
  the casts inside a champion shield's duration, which are most likely the shield duty's.
- **A removal's `dur` is what was left.** A CC removed with time left was dispelled, broken by
  damage, or its holder died; the trace can't tell which.
- **A crowd-AoE victim is a raid player.** `dmg` rows cover players only, while the champion's 3+
  check counts pets too, so a cast can fire on fewer victims than it needed.
- **An instant AoE's `dmg` rows land 0-6 ms before its own `cast` row**, so pairing a hit with the
  latest cast gives it to the cast before or to none. `--aoe` gives a row up to 50 ms before a cast
  to that cast.
- **Hellfire, Fan of Knives, Arcane Explosion and the Unstable Affliction backlash remap**, so every
  id of their spelldifficulty row counts.
- **A champion's victim is its snapshot target.** `--peel` judges a snare by the champion's target in
  the last sample at or before the cast, and `--pack` counts champions within 8 yd centre to centre
  off the same samples, so both lag up to one sample.
- **A corpse is where its death record put it.** `--peel` counts champions round a Rebirth's target
  at that spot, in the last sample before the cast.
- **No champion row before the engage.** The snapshots before it carry raiders only, so the landing
  and the release never show: `--ball` reads the start off the raid's first cast on a champion.
- **A disarm on a champion never shows.** `--peel` reads Disarm, Dismantle and Psychic Horror casts on
  the Warrior champion against its own Bladestorm casts, and a Frost Nova, which names no target, by
  its caster's `fc.peel` hold.
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
CC_AURA = "fc.ccaura"
TOTEM = "fc.totem"
SWITCH_REASONS = ("first", "dead", "immune", "back", "skull", "pulled", "reset")

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
    **dict.fromkeys((710, 18647), "Banish"),
}
FEAR = "Fear"
# fc.ccaura records fear, stun and confuse auras only, so a root never shows on it
ON_CC_AURA = ("Polymorph", "Cyclone", "Banish")
# the guide's pace: a CC landed at least this often
CC_PACE_MS = 10000
# MotionMaster's FLEEING_MOTION_TYPE, snapshot column 9
MOVEGEN_COLUMN = 9
FLEEING = 9
# a CC cast lands inside its cast time plus this, snapshot and refresh lag included
LAND_SLACK_MS = 1000

# Heroism, Bloodlust. The champions' own are 65983 and 65980.
LUST_SPELLS = (32182, 2825)
LUST_DEBUFFS = {57723: "Exhaustion", 57724: "Sated"}
# a fresh lust's removal row and the member's own apply are a few ms apart
REAPPLY_MS = 100

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
    "faction champions cc guard multiplier",
    "faction champions totem guard multiplier",
    "faction champions grip hold",
    "faction champions battle res guard",
    "faction champions heroic taunt guard",
)
COUNTERSPELL_ACTION = "faction champions counterspell kill target"
ANTI_FEAR_ACTION = "faction champions anti fear"
DISPEL_CC_ACTION = "faction champions dispel cc"
PURGE_ACTION = "faction champions purge kill target"
AVOID_AOE_ACTION = "faction champions avoid aoe"
KILL_TOTEM_ACTION = "faction champions kill totem"

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
# an instant AoE's victim rows land 0-6 ms before its cast row
AOE_LEAD_MS = 50

# The Enhancement Shaman's totems and the Balance Druid's Force of Nature, cast id -> name
SUMMONS = {65989: "Grounding Totem", 65990: "Windfury Totem", 65992: "Tremor Totem", 65861: "Force of Nature"}
NPC_TREANT = 36070
# Grounding, Windfury and Tremor Totem, and the treant. The raid's own totems share these entries, so
# a unit counts only with a champion owner.
ADD_ENTRIES = frozenset((5925, 6112, 5913, NPC_TREANT))

GRIP = "fc.grip"
SPELL_DEATH_GRIP = 49576
PACK_RADIUS = 8.0
PACK_ROLES = ("tank", "melee", "ranged", "heal")
FIRST_MINUTE_MS = 60000
# a stall: health moving under this many points over the span, with this many bots on the kill target
STALL_HP = 3.0
STALL_ATTACKERS = 10
STALL_MS = 20000
# how long after a grip the landing is read
GRIP_SETTLE_MS = 1000

PACK_TANK = "fc.packtank"
OPENER = "fc.opener"
# the pack: champions this close to where they landed, the kill target left out
BALL_ANCHOR_RADIUS = 25.0
# a champion healer's heal reach
BALL_HEAL_REACH = 40.0
# off the landing spot by this much and not on a tank: loose on the raid
BALL_ESCAPE = 20.0
BALL_WINDOW_MS = 10000
SPELL_PVP_TRINKET = 65547

SNARES = {1715: "Hamstring", 12323: "Piercing Howl", 45524: "Chains of Ice", 2974: "Wing Clip",
          5116: "Concussive Shot",
          **dict.fromkeys((8056, 8058, 10472, 10473, 25464, 49235, 49236), "Frost Shock")}
SNARE_HEALTH = 50.0
PEEL = "fc.peel"
PEEL_SPELLS = {676: "Disarm", 51722: "Dismantle", 64044: "Psychic Horror", 49576: "Death Grip",
               **dict.fromkeys((122, 865, 6131, 10230, 27088, 42917), "Frost Nova"),
               **dict.fromkeys((339, 1062, 5195, 5196, 9852, 9853, 26989, 53308), "Entangling Roots")}
# the grip chain's Death Grip and a Frost Nova, which names no target, count only inside a peel hold
HOLD_ONLY_PEELS = ("Death Grip", "Frost Nova")
DISARMS = frozenset((676, 51722, 64044))
SPELL_BLADESTORM = 65947
WARRIOR_CHAMPIONS = (34475, 34453)
EXTERNALS = {**dict.fromkeys((1022, 5599, 10278), "Hand of Protection"), 33206: "Pain Suppression",
             47788: "Guardian Spirit", 6940: "Hand of Sacrifice"}
EXTERNAL_WATCH_MS = 10000
REBIRTH = frozenset((20484, 20739, 20742, 20747, 20748, 26994, 48477))
# The class taunts, the druid's and the hunter pet's Growl, and the area ones
TAUNTS = {355: "Taunt", 62124: "Hand of Reckoning", 31789: "Righteous Defense", 56222: "Dark Command",
          6795: "Growl", 694: "Mocking Blow", 1161: "Challenging Shout", 5209: "Challenging Roar",
          **dict.fromkeys((2649, 14916, 14917, 14918, 14919, 14920, 14921, 27047, 61676), "Growl (pet)")}

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

    def first_seen(self, guid: int) -> int | None:
        found = self.rows.get(guid)
        return found[0][0] if found else None

    def last_seen(self, guid: int) -> int | None:
        found = self.rows.get(guid)
        return found[0][-1] if found else None

    def onsets(self, guid: int, column: int, value) -> list[int]:
        """Stamps where a column turned to `value`, a first sample already at it included."""
        found = self.rows.get(guid)
        if not found:
            return []
        out = []
        was = None
        for when, row in zip(*found):
            now = row[column] if len(row) > column else None
            if now == value and was != value:
                out.append(when)
            was = now
        return out


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


def cc_aura_onsets(trace: Trace) -> dict[int, list[tuple[int, int]]]:
    """Per champion, `(t, spell)` each time `fc.ccaura` turned to a spell."""
    return {guid: [(start, as_guid(value)) for value, start, _ in spans if as_guid(value)]
            for guid, spans in holder_spans(trace, CC_AURA).items()}


def cc_aura_held(trace: Trace) -> dict[int, collections.Counter]:
    """Per champion, ms `fc.ccaura` held each spell. One still held at the end runs to it."""
    out = {}
    for guid, spans in holder_spans(trace, CC_AURA, pull_end(trace)).items():
        held = collections.Counter()
        for value, start, stop in spans:
            if as_guid(value):
                held[as_guid(value)] += stop - start
        if held:
            out[guid] = held
    return out


def fear_uptime(trace: Trace) -> dict[int, tuple[int, int]]:
    """Per champion, `(feared, samples)` over its snapshot samples from the pull while alive."""
    targets = champions(trace)
    start = pull_start(trace)
    out: dict[int, list[int]] = {}
    for snap in frames(trace):
        if snap["t"] < start:
            continue
        for row in snap.get("u", []):
            if row[0] in targets and row[5] > 0:
                counts = out.setdefault(row[0], [0, 0])
                counts[0] += len(row) > MOVEGEN_COLUMN and row[MOVEGEN_COLUMN] == FLEEING
                counts[1] += 1
    return {guid: (feared, total) for guid, (feared, total) in out.items()}


def cc_casts(trace: Trace) -> dict[int, dict]:
    """Per roster caster, its CC casts on champions, where they went against the assignment, and
    `landed`, spell name to casts landed, None where the trace can't say. A cast lands when, within
    its cast time plus LAND_SLACK_MS, its target turned to fleeing (Fear) or `fc.ccaura` on it turned
    to that spell (Cyclone, Polymorph); each such turn goes to the first cast whose window holds it."""
    targets = champions(trace)
    holds = cc_holds(trace)
    kill = kill_spans(trace)
    roster = roster_guids(trace)
    samples = Samples(trace)
    fears = {guid: samples.onsets(guid, MOVEGEN_COLUMN, FLEEING) for guid in targets}
    auras = cc_aura_onsets(trace)
    has_aura = bool(notes(trace, CC_AURA))
    claimed: set[tuple[str, int, int]] = set()
    out: dict[int, dict] = {}
    for rec in trace.of("cast"):
        caster, target = rec.get("s"), rec.get("tgt")
        if rec.get("tr") or rec.get("sp") not in CC_SPELLS or target not in targets or caster not in roster:
            continue
        name = CC_SPELLS[rec["sp"]]
        row = out.setdefault(caster, {"casts": 0, "assigned": 0, "kill": 0, "spells": collections.Counter(),
                                      "landed": {}})
        row["casts"] += 1
        row["assigned"] += held_at(holds.get(caster, []), rec["t"]) == target
        row["kill"] += held_at(kill, rec["t"]) == target
        row["spells"][name] += 1

        if name == FEAR:
            kind, turns = "fear", fears[target]
        elif has_aura and name in ON_CC_AURA:
            kind, turns = "aura", [when for when, spell in auras.get(target, []) if spell == rec["sp"]]
        else:
            row["landed"][name] = None
            continue
        high = rec["t"] + (rec.get("ct") or 0) + LAND_SLACK_MS
        turn = next((when for when in turns if rec["t"] <= when <= high and (kind, target, when) not in claimed),
                    None)
        if turn is not None:
            claimed.add((kind, target, turn))
        row["landed"][name] = row["landed"].get(name, 0) + (turn is not None)
    return out


def cc_duties(trace: Trace) -> dict[int, dict]:
    """Per bot, its `fc.cc` holds: `duties`, `cast` (those its own CC cast on the champion went out in),
    `spells` (a Counter of the first such cast's spell per hold) and `median` hold length in ms."""
    casts = [(rec["t"], rec.get("s"), rec.get("tgt"), CC_SPELLS[rec["sp"]]) for rec in trace.of("cast")
             if rec.get("sp") in CC_SPELLS and not rec.get("tr")]
    out = {}
    for bot, spans in cc_holds(trace).items():
        row = {"duties": len(spans), "cast": 0, "spells": collections.Counter(),
               "median": statistics.median(stop - start for _, start, stop in spans)}
        for target, start, stop in spans:
            names = [name for when, by, aim, name in casts if by == bot and aim == target and start <= when <= stop]
            if names:
                row["cast"] += 1
                row["spells"][names[0]] += 1
        out[bot] = row
    return out


def cc_pace(trace: Trace, rows: dict[int, dict] | None = None) -> tuple[int, float]:
    """`(landed, seconds)` from the pull to the end, the casts whose landing can't be read left out."""
    rows = cc_casts(trace) if rows is None else rows
    landed = sum(count for row in rows.values() for count in row["landed"].values() if count is not None)
    return landed, (pull_end(trace) - pull_start(trace)) / 1000


def cc_spell_name(trace: Trace, spell: int) -> str:
    return f"{CC_SPELLS[spell]} {spell}" if spell in CC_SPELLS else trace.spell(spell)


def show_cc(trace: Trace) -> None:
    print("CC")
    duties = cc_duties(trace)
    klass = classes(trace)
    if duties:
        print(f"  {'fc.cc holder':14} {'class':8} {'duties':>6} {'cast':>5} {'median':>7}  first cast per duty")
        for bot in sorted(duties, key=trace.name):
            row = duties[bot]
            spells = ", ".join(f"{name} {count}" for name, count in row["spells"].most_common()) or "-"
            print(f"  {trace.name(bot)[:14]:14} {klass.get(bot, '?'):8} {row['duties']:6} {row['cast']:5}"
                  f" {row['median'] / 1000:6.1f}s  {spells}")
    else:
        print("  fc.cc never gave anyone a duty")

    held = cc_aura_held(trace)
    uptime = fear_uptime(trace)
    has_aura = bool(notes(trace, CC_AURA))
    shown = [(guid, name) for guid, name, _ in lineup(trace) if guid in held or guid in uptime]
    if shown:
        print(f"\n  {'on the champion':20} {'spec':20} {'feared':>15}  fc.ccaura held")
        for guid, name in shown:
            feared, total = uptime.get(guid, (0, 0))
            share = f"{feared * 100.0 / total:.1f}% of {total}" if total else "-"
            spells = (", ".join(f"{cc_spell_name(trace, spell)} {ms / 1000:.1f}s"
                                for spell, ms in held[guid].most_common()) if guid in held
                      else "-" if has_aura else "n/a")
            print(f"  {trace.name(guid)[:20]:20} {name[:20]:20} {share:>15}  {spells}")

    rows = cc_casts(trace)
    if not rows:
        print("  no Polymorph, Fear, Cyclone, Banish or Entangling Roots cast on a champion")
        return
    print(f"\n  {'bot':14} {'class':8} {'casts':>5} {'landed':>6} {'on assigned':>12} {'on kill':>8}  spells")
    for bot in sorted(rows, key=trace.name):
        row = rows[bot]
        known = [count for count in row["landed"].values() if count is not None]
        landed = str(sum(known)) if len(known) == len(row["landed"]) else "n/a"
        spells = ", ".join(
            f"{name} {count} ({'landed n/a' if row['landed'][name] is None else str(row['landed'][name]) + ' landed'})"
            for name, count in row["spells"].most_common())
        print(f"  {trace.name(bot)[:14]:14} {klass.get(bot, '?'):8} {row['casts']:5} {landed:>6}"
              f" {row['assigned'] * 100.0 / row['casts']:11.0f}% {row['kill']:8}  {spells}")
    landed, seconds = cc_pace(trace, rows)
    rate = landed * CC_PACE_MS / 1000 / seconds if seconds > 0 else 0.0
    print(f"  landed per 10 s: {rate:.2f} ({landed} in {seconds:.0f} s)")
    print(f"  feared is the share of the champion's live samples from the pull at moveGen {FLEEING}, fleeing;"
          f" a cast landed when, inside its cast time + {LAND_SLACK_MS / 1000:.0f} s, its target turned to"
          f" fleeing (Fear) or {CC_AURA} on it to that spell (Cyclone, Polymorph, Banish), n/a without"
          f" {CC_AURA} and for Roots; a CC broken between two samples never shows, so landed is a lower bound")


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


def lust_carriers(trace: Trace) -> list[dict]:
    """Each roster member who carried Exhaustion or Sated into the pull, in removal order: `member`,
    `spell`, `t` it came off, `left` ms at the removal (0 expired, else cleared by a death)."""
    roster = roster_guids(trace)
    start = pull_start(trace)
    rows = [rec for rec in trace.of("aura") if rec.get("sp") in LUST_DEBUFFS and rec.get("d") in roster]
    first: dict[int, int] = {}
    for index, rec in enumerate(rows):
        first.setdefault(rec["d"], index)

    out = []
    for member, index in first.items():
        rec = rows[index]
        if not rec.get("r") or rec["t"] < start:
            continue
        fresh = False
        for later in rows[index + 1:]:
            if later["t"] - rec["t"] > REAPPLY_MS:
                break
            fresh = fresh or (later["d"] == member and later["sp"] == rec["sp"] and not later.get("r"))
        if not fresh:
            out.append({"member": member, "spell": rec["sp"], "t": rec["t"], "left": rec.get("dur") or 0})
    return sorted(out, key=lambda row: (row["t"], row["member"]))


def show_burst(trace: Trace) -> None:
    print("BURST")
    start = pull_start(trace)
    lust, first, vetoes = burst_rows(trace)
    if lust:
        print(f"  lust: {trace.spell(lust['sp'])} by {trace.name(lust.get('s'))} at {clock(lust['t'])}"
              f" ({(lust['t'] - start) / 1000:+.1f} s from the pull)")
    else:
        print("  no raid lust cast")

    carried = lust_carriers(trace)
    if carried:
        expired = [row["t"] for row in carried if not row["left"]]
        last = f"the last expired at {clock(max(expired))}" if expired else "none expired"
        print(f"  Exhaustion or Sated carried into the pull: {len(carried)} of {len(roster_guids(trace))}, {last}")
        for row in carried:
            how = "expired" if not row["left"] else f"cleared, {row['left'] / 1000:.0f} s left"
            print(f"    {trace.name(row['member'])[:14]:14} {LUST_DEBUFFS[row['spell']]:10}"
                  f" {clock(row['t']):>9}  {how}")
    else:
        print("  nobody carried Exhaustion or Sated into the pull")

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
    to rows from that cast. A row goes to its caster's latest cast of that spell starting no later
    than AOE_LEAD_MS after it, if the row falls before that cast's start plus its window."""
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
        found = bisect.bisect_right(stamps.get(key, []), rec["t"] + AOE_LEAD_MS) - 1
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
    print("  a victim is a raid player hit by that cast (a `dmg` row), or feared by it for the two fears,"
          f" from {AOE_LEAD_MS} ms before its cast row; hits per victim is for the two channels, which tick"
          " once a second")


def summon_casts(trace: Trace) -> list[dict]:
    """Every champion totem and Force of Nature cast: `t`, `caster`, `spell`."""
    targets = champions(trace)
    return [{"t": rec["t"], "caster": rec["s"], "spell": rec["sp"]} for rec in trace.of("cast")
            if rec.get("sp") in SUMMONS and rec.get("s") in targets and not rec.get("tr")]


def add_units(trace: Trace) -> list[dict]:
    """Every champion totem and treant the trace named, by first snapshot: `guid`, `entry`, `first`
    and `last` snapshot (None if never sampled), and its `dmg` rows on the roster as `damage` and
    `victims`."""
    targets = champions(trace)
    roster = roster_guids(trace)
    samples = Samples(trace)
    adds = {guid: entry for guid, entry in trace.entries.items()
            if entry in ADD_ENTRIES and trace.owners.get(guid) in targets}
    damage = collections.Counter()
    victims: dict[int, set[int]] = collections.defaultdict(set)
    for rec in trace.of("dmg"):
        if rec.get("s") in adds and rec.get("d") in roster:
            damage[rec["s"]] += rec.get("a", 0)
            victims[rec["s"]].add(rec["d"])
    out = [{"guid": guid, "entry": entry, "first": samples.first_seen(guid), "last": samples.last_seen(guid),
            "damage": damage[guid], "victims": len(victims[guid])} for guid, entry in adds.items()]
    return sorted(out, key=lambda row: (row["first"] is None, row["first"] or 0, row["guid"]))


def totem_holds(trace: Trace) -> list[dict]:
    """Each `fc.totem` hold: `bot`, `totem`, `start`, `stop`, and `gone`, whether the totem's last
    snapshot fell inside the hold before the trace's last one."""
    samples = Samples(trace)
    final = samples.stamps[-1] if samples.stamps else None
    out = []
    for bot, spans in holder_spans(trace, TOTEM, pull_end(trace)).items():
        for value, start, stop in spans:
            totem = as_guid(value)
            if not totem:
                continue
            last = samples.last_seen(totem)
            out.append({"bot": bot, "totem": totem, "start": start, "stop": stop,
                        "gone": last is not None and start <= last < stop and last != final})
    return sorted(out, key=lambda row: (row["start"], row["bot"]))


def show_adds(trace: Trace) -> None:
    print("TOTEMS AND TREANTS")
    casts = summon_casts(trace)
    if casts:
        print("  " + ", ".join(f"{SUMMONS[row['spell']]} {clock(row['t'])} ({trace.name(row['caster'])})"
                               for row in casts))
    else:
        print("  no champion cast a totem or Force of Nature")

    units = add_units(trace)
    if units:
        print(f"\n  {'unit':32} {'first':>9} {'last':>9} {'lived':>7}  damage to the raid")
        for row in units:
            seen = row["first"] is not None
            lived = f"{(row['last'] - row['first']) / 1000:6.1f}s" if seen else "-"
            hurt = f"{row['damage']} to {row['victims']} raider(s)" if row["entry"] == NPC_TREANT else ""
            print(f"  {trace.name(row['guid'])[:32]:32} {clock(row['first']) if seen else '-':>9}"
                  f" {clock(row['last']) if seen else '-':>9} {lived:>7}  {hurt}".rstrip())
        treants = [row for row in units if row["entry"] == NPC_TREANT]
        if treants:
            print(f"  treants: {len(treants)}, {sum(row['damage'] for row in treants)} damage to the raid")

    holds = totem_holds(trace)
    if holds:
        print(f"\n  {'fc.totem holder':14} {'at':>9} {'length':>7} {'totem':32} {'gone':>5}")
        for row in holds:
            print(f"  {trace.name(row['bot'])[:14]:14} {clock(row['start']):>9}"
                  f" {(row['stop'] - row['start']) / 1000:6.1f}s {trace.name(row['totem'])[:32]:32}"
                  f" {'yes' if row['gone'] else '-':>5}")
    else:
        print("  fc.totem never held a totem")
    print(f"  {KILL_TOTEM_ACTION}: {ok_rows(trace, KILL_TOTEM_ACTION)} OK row(s)")
    print("  first and last are the unit's snapshots; gone means the totem left the snapshots inside the"
          " hold, killed or run out")


def distance(a: list, b: list) -> float:
    """2D, centre to centre, between two snapshot rows or `[_, x, y]` spots."""
    return ((a[1] - b[1]) ** 2 + (a[2] - b[2]) ** 2) ** 0.5


def live_rows(snap: dict, guids) -> list[list]:
    return [row for row in snap.get("u", []) if row[0] in guids and row[5] > 0]


def snap_at(trace: Trace, when: int) -> dict | None:
    """The last snapshot at or before `when`."""
    snaps = frames(trace)
    stamps = getattr(trace, "_fc_snap_stamps", None)
    if stamps is None or len(stamps) != len(snaps):
        stamps = [snap["t"] for snap in snaps]
        trace._fc_snap_stamps = stamps
    index = bisect.bisect_right(stamps, when) - 1
    return snaps[index] if index >= 0 else None


def pack_by_role(trace: Trace) -> dict[str, tuple[float, int]]:
    """Per role, `(mean champions within 8 yd, samples)` over the roster's live samples from the pull."""
    targets = champions(trace)
    roster = roster_guids(trace)
    start = pull_start(trace)
    counts: dict[str, list[int]] = collections.defaultdict(list)
    for snap in frames(trace):
        if snap["t"] < start:
            continue
        near = live_rows(snap, targets)
        for row in live_rows(snap, roster):
            counts[trace.role(row[0])].append(sum(1 for champion in near if distance(row, champion) <= PACK_RADIUS))
    return {role: (statistics.mean(values), len(values)) for role, values in counts.items()}


def early_melee_deaths(trace: Trace) -> list[dict]:
    start = pull_start(trace)
    return [death for death in combat_deaths(trace)
            if trace.role(death["g"]) == "melee" and 0 <= death["t"] - start < FIRST_MINUTE_MS]


def melee_to_healer(trace: Trace) -> float | None:
    """Median distance from each live melee sample to the nearest live healer, from the pull."""
    roster = roster_guids(trace)
    start = pull_start(trace)
    gaps = []
    for snap in frames(trace):
        if snap["t"] < start:
            continue
        rows = live_rows(snap, roster)
        healers = [row for row in rows if trace.role(row[0]) == "heal"]
        if not healers:
            continue
        gaps += [min(distance(row, healer) for healer in healers) for row in rows if trace.role(row[0]) == "melee"]
    return statistics.median(gaps) if gaps else None


def first_kill(trace: Trace) -> int | None:
    """ms from the pull to the first kill target that died."""
    died = [hold["stop"] for hold in kill_holds(trace) if hold["died"]]
    return min(died) - pull_start(trace) if died else None


def longest_stall(trace: Trace) -> dict | None:
    """The longest span inside one `fc.kill` hold with 10+ bots on the kill target at every sample and its
    health moving under 3 points: `guid`, `start`, `stop`, `low`, `high`, or None without one."""
    bots = {guid for guid in roster_guids(trace) if guid not in trace.humans}
    series: dict[tuple[int, int], list[tuple[int, float, int]]] = collections.defaultdict(list)
    for guid, start, stop in kill_spans(trace):
        if not guid:
            continue
        for snap in frames(trace):
            if not start <= snap["t"] < stop:
                continue
            target = next((row for row in snap.get("u", []) if row[0] == guid and row[5] > 0), None)
            if target is None:
                continue
            on = sum(1 for row in live_rows(snap, bots) if len(row) > 7 and row[7] == guid)
            series[(guid, start)].append((snap["t"], target[5], on))

    best = None
    for (guid, _), samples in series.items():
        for first in range(len(samples)):
            if samples[first][2] < STALL_ATTACKERS:
                continue
            low = high = samples[first][1]
            last = first
            while last + 1 < len(samples) and samples[last + 1][2] >= STALL_ATTACKERS:
                nlow, nhigh = min(low, samples[last + 1][1]), max(high, samples[last + 1][1])
                if nhigh - nlow >= STALL_HP:
                    break
                low, high, last = nlow, nhigh, last + 1
            length = samples[last][0] - samples[first][0]
            if best is None or length > best["stop"] - best["start"]:
                best = {"guid": guid, "start": samples[first][0], "stop": samples[last][0], "low": low, "high": high}
    return best


def grips(trace: Trace) -> list[dict]:
    """Every raid Death Grip on a champion in time order: `t`, `by`, `target`, `kill`, the other champions
    within 8 yd of it in the last sample before (`before`) and a second after (`after`), how far it moved
    (`moved`, None without both samples), and ms since the raid's grip before it (`since`, None for the
    first)."""
    targets = champions(trace)
    roster = roster_guids(trace)
    kill = kill_spans(trace)
    out = []
    for rec in trace.of("cast"):
        target = rec.get("tgt")
        if rec.get("sp") != SPELL_DEATH_GRIP or rec.get("s") not in roster or target not in targets:
            continue
        counts, spots = [], []
        for snap in (snap_at(trace, rec["t"]), snap_at(trace, rec["t"] + GRIP_SETTLE_MS)):
            rows = live_rows(snap, targets) if snap else []
            row = next((row for row in rows if row[0] == target), None)
            spots.append(row)
            counts.append(None if row is None else
                          sum(1 for other in rows if other[0] != target and distance(row, other) <= PACK_RADIUS))
        out.append({"t": rec["t"], "by": rec["s"], "target": target, "kill": held_at(kill, rec["t"]) == target,
                    "before": counts[0], "after": counts[1],
                    "moved": distance(*spots) if all(spot is not None for spot in spots) else None})
    out.sort(key=lambda row: row["t"])
    for previous, row in zip([None] + out, out):
        row["since"] = None if previous is None else row["t"] - previous["t"]
    return out


def grip_holds(trace: Trace) -> list[dict]:
    """Each `fc.grip` hold: `bot`, `target`, `start`, `stop`, and whether the bot's Death Grip went out
    inside it (`cast`)."""
    casts = [(rec["t"], rec.get("s")) for rec in trace.of("cast") if rec.get("sp") == SPELL_DEATH_GRIP]
    out = []
    for bot, spans in holder_spans(trace, GRIP, pull_end(trace)).items():
        for value, start, stop in spans:
            if as_guid(value):
                out.append({"bot": bot, "target": as_guid(value), "start": start, "stop": stop,
                            "cast": any(by == bot and start <= when <= stop for when, by in casts)})
    return sorted(out, key=lambda row: (row["start"], row["bot"]))


def show_pack(trace: Trace) -> None:
    print("THE PACK")
    by_role = pack_by_role(trace)
    if by_role:
        print("  champions within 8 yd, per live sample: " + ", ".join(
            f"{role} {by_role[role][0]:.2f} ({by_role[role][1]})" for role in PACK_ROLES if role in by_role))
    deaths = early_melee_deaths(trace)
    print(f"  melee deaths in the first {FIRST_MINUTE_MS // 1000} s: {len(deaths)}"
          + (f" ({', '.join(trace.name(death['g']) for death in deaths)})" if deaths else ""))
    gap = melee_to_healer(trace)
    print(f"  melee to the nearest healer, median: {'-' if gap is None else f'{gap:.1f} yd'}")
    kill_ms = first_kill(trace)
    print(f"  first kill: {'none' if kill_ms is None else f'{kill_ms / 1000:.1f} s from the pull'}")
    stall = longest_stall(trace)
    if stall:
        length = (stall["stop"] - stall["start"]) / 1000
        print(f"  longest flat span: {trace.name(stall['guid'])} {clock(stall['start'])} for {length:.1f} s,"
              f" {stall['low']:.0f}-{stall['high']:.0f}%" + (" STALL" if length * 1000 >= STALL_MS else ""))
    else:
        print(f"  longest flat span: never {STALL_ATTACKERS}+ bots on the kill target")

    rows = grips(trace)
    if rows:
        print(f"\n  {'grip at':>9} {'by':14} {'on':20} {'kill':>4} {'pack before':>11} {'after':>5} {'moved':>6}"
              f" {'since prev':>10}")
        for row in rows:
            moved = "-" if row["moved"] is None else f"{row['moved']:.1f}"
            before = "-" if row["before"] is None else str(row["before"])
            after = "-" if row["after"] is None else str(row["after"])
            since = "-" if row["since"] is None else f"{row['since'] / 1000:.1f}s"
            who = trace.name(row["by"])[:14] + ("*" if row["by"] in trace.humans else "")
            print(f"  {clock(row['t']):>9} {who:14} {trace.name(row['target'])[:20]:20}"
                  f" {'yes' if row['kill'] else '-':>4} {before:>11} {after:>5} {moved:>6} {since:>10}")
    else:
        print("  no raid Death Grip on a champion")
    holds = grip_holds(trace)
    if holds:
        cast = sum(1 for hold in holds if hold["cast"])
        print(f"  {GRIP} holds: {len(holds)}, {cast} ending in the bot's grip, median"
              f" {statistics.median(hold['stop'] - hold['start'] for hold in holds) / 1000:.1f} s")
    print(f"  the pack is the other champions within {PACK_RADIUS:.0f} yd centre to centre; a stall is"
          f" {STALL_MS // 1000} s or more with {STALL_ATTACKERS}+ bots on the kill target and its health"
          f" moving under {STALL_HP:.0f} points; moved is the target's travel from the sample before the grip"
          f" to one {GRIP_SETTLE_MS / 1000:.0f} s after; since prev counts from the raid's grip before it,"
          f" a human's (*) included")


def victim_at(trace: Trace, champion: int, when: int) -> list | None:
    """The roster row of the champion's target in the last sample at or before `when`."""
    snap = snap_at(trace, when)
    if not snap:
        return None
    rows = {row[0]: row for row in snap.get("u", [])}
    row = rows.get(champion)
    if row is None or len(row) <= 7:
        return None
    victim = rows.get(row[7])
    return victim if victim is not None and row[7] in roster_guids(trace) else None


def snare_casts(trace: Trace) -> list[dict]:
    """Every raid snare on a champion: `t`, `by`, `spell`, `target`, and whether its victim then was a
    healer or under half health (`needed`, None when the sample can't say)."""
    targets = champions(trace)
    roster = roster_guids(trace)
    out = []
    for rec in trace.of("cast"):
        if rec.get("sp") not in SNARES or rec.get("tgt") not in targets or rec.get("s") not in roster:
            continue
        victim = victim_at(trace, rec["tgt"], rec["t"])
        needed = None if victim is None else trace.role(victim[0]) == "heal" or victim[5] < SNARE_HEALTH
        out.append({"t": rec["t"], "by": rec["s"], "spell": SNARES[rec["sp"]], "target": rec["tgt"], "needed": needed})
    return out


def external_casts(trace: Trace) -> list[dict]:
    """Every raid external on a raid member: `t`, `by`, `spell`, `target`, and whether the target died
    within 10 s (`died`)."""
    roster = roster_guids(trace)
    deaths = [(death["t"], death["g"]) for death in combat_deaths(trace)]
    out = []
    for rec in trace.of("cast"):
        target = rec.get("tgt") or rec.get("s")
        if rec.get("sp") not in EXTERNALS or rec.get("s") not in roster or target not in roster:
            continue
        died = any(guid == target and rec["t"] <= when <= rec["t"] + EXTERNAL_WATCH_MS for when, guid in deaths)
        out.append({"t": rec["t"], "by": rec["s"], "spell": EXTERNALS[rec["sp"]], "target": target, "died": died})
    return out


def battle_res_casts(trace: Trace) -> list[dict]:
    """Every raid Rebirth: `t`, `by`, `target`, and the champions within 8 yd of where the target died, in
    the last sample before the cast (`near`, None without a death record)."""
    targets = champions(trace)
    roster = roster_guids(trace)
    deaths = sorted((death["t"], death["g"], death) for death in combat_deaths(trace))
    out = []
    for rec in trace.of("cast"):
        if rec.get("sp") not in REBIRTH or rec.get("s") not in roster or rec.get("tr"):
            continue
        target = rec.get("tgt")
        death = next((found for when, guid, found in reversed(deaths) if guid == target and when <= rec["t"]), None)
        near = None
        snap = snap_at(trace, rec["t"])
        if death is not None and "x" in death and snap:
            spot = [0, death["x"], death["y"]]
            near = sum(1 for row in live_rows(snap, targets) if distance(row, spot) <= PACK_RADIUS)
        out.append({"t": rec["t"], "by": rec["s"], "target": target, "near": near})
    return out


def taunt_casts(trace: Trace) -> collections.Counter:
    """`(spell name, bot or human)` -> casts by the raid and its pets on champions. Righteous Defense
    names a friend, so it counts whatever its target."""
    targets = champions(trace)
    out = collections.Counter()
    for rec in trace.of("cast"):
        name = TAUNTS.get(rec.get("sp"))
        caster = raid_caster(trace, rec.get("s"))
        if name is None or caster is None or rec.get("tr"):
            continue
        if name != "Righteous Defense" and rec.get("tgt") not in targets:
            continue
        out[(name, "human" if caster in trace.humans else "bot")] += 1
    return out


def peel_casts(trace: Trace) -> list[dict]:
    """Every raid peel on a champion: `t`, `by`, `spell`, `target`, and `duty`, cast inside the caster's
    `fc.peel` hold on that champion. Death Grip and Frost Nova count only inside a hold, the target being
    the hold's champion."""
    targets = champions(trace)
    roster = roster_guids(trace)
    holds: dict[int, list[tuple[int, int, int]]] = collections.defaultdict(list)
    for hold in duty_holds(trace, PEEL):
        holds[hold["bot"]].append((as_guid(hold["value"]), hold["start"], hold["stop"]))
    out = []
    for rec in trace.of("cast"):
        name = PEEL_SPELLS.get(rec.get("sp"))
        caster = rec.get("s")
        if name is None or caster not in roster or rec.get("tr"):
            continue
        held = held_at(holds.get(caster, []), rec["t"])
        target = rec.get("tgt") if rec.get("tgt") in targets else 0
        if name in HOLD_ONLY_PEELS:
            target = held if held and target in (0, held) else 0
        if not target:
            continue
        out.append({"t": rec["t"], "by": caster, "spell": name, "target": target, "duty": held == target})
    return out


def warrior_disarms(trace: Trace) -> list[dict]:
    """Per Warrior champion: `guid`, its Bladestorm casts (`bladestorms`) and the raid's Disarm, Dismantle
    and Psychic Horror casts on it (`disarms`), each a list of times."""
    roster = roster_guids(trace)
    out = []
    for guid, entry in sorted(champions(trace).items()):
        if entry not in WARRIOR_CHAMPIONS:
            continue
        casts = trace.of("cast")
        out.append({"guid": guid,
                    "bladestorms": [rec["t"] for rec in casts
                                    if rec.get("s") == guid and rec.get("sp") == SPELL_BLADESTORM],
                    "disarms": [rec["t"] for rec in casts if rec.get("tgt") == guid and rec.get("sp") in DISARMS
                                and rec.get("s") in roster and not rec.get("tr")]})
    return out


def show_peel(trace: Trace) -> None:
    print("PEEL")
    klass = classes(trace)
    snares = snare_casts(trace)
    if snares:
        print(f"  {len(snares)} snare(s) on champions, by spell and class (victim a healer or under"
              f" {SNARE_HEALTH:.0f}% / not / unknown):")
        groups: dict[tuple[str, str], list[bool | None]] = collections.defaultdict(list)
        for row in snares:
            groups[(row["spell"], klass.get(row["by"], "?"))].append(row["needed"])
        for (spell, name), needed in sorted(groups.items()):
            print(f"    {spell:16} {name:12} {len(needed):3}  {needed.count(True)} / {needed.count(False)}"
                  f" / {needed.count(None)}")
    else:
        print("  no snare on a champion")

    peels = peel_casts(trace)
    if peels:
        print(f"\n  {len(peels)} peel(s) on champions, by spell and class (on an {PEEL} duty / not):")
        counted: dict[tuple[str, str], list[bool]] = collections.defaultdict(list)
        for row in peels:
            counted[(row["spell"], klass.get(row["by"], "?"))].append(row["duty"])
        for (spell, name), duty in sorted(counted.items()):
            print(f"    {spell:16} {name:12} {len(duty):3}  {duty.count(True)} / {duty.count(False)}")
    else:
        print(f"\n  no peel on a champion")
    peel_holds = duty_holds(trace, PEEL)
    print(f"  {PEEL} holds: {len(peel_holds)}")
    for row in warrior_disarms(trace):
        storms, disarms = row["bladestorms"], row["disarms"]
        first = ("; disarmed before the first Bladestorm: " + ("yes" if disarms and disarms[0] < storms[0] else "no")
                 if storms else "")
        print(f"  {trace.name(row['guid'])} (Warrior): Bladestorm {len(storms)}"
              + (f" (first {clock(storms[0])})" if storms else "")
              + f", disarms {len(disarms)}" + (f" (first {clock(disarms[0])})" if disarms else "") + first)

    externals = external_casts(trace)
    if externals:
        print(f"\n  {'external at':>11} {'spell':18} {'by':14} {'on':14} {'role':6} {'died in 10 s':>12}")
        for row in externals:
            died = "yes" if row["died"] else "-"
            print(f"  {clock(row['t']):>11} {row['spell']:18} {trace.name(row['by'])[:14]:14}"
                  f" {trace.name(row['target'])[:14]:14} {trace.role(row['target']):6} {died:>12}")
    else:
        print("  no Hand of Protection, Pain Suppression, Guardian Spirit or Hand of Sacrifice on the raid")

    rezzes = battle_res_casts(trace)
    if rezzes:
        print(f"\n  {'rebirth at':>10} {'by':14} {'on':14} {'role':6} {'champions within 8 yd':>21}")
        for row in rezzes:
            near = "-" if row["near"] is None else str(row["near"])
            print(f"  {clock(row['t']):>10} {trace.name(row['by'])[:14]:14} {trace.name(row['target'])[:14]:14}"
                  f" {trace.role(row['target']):6} {near:>21}")
    else:
        print("  no Rebirth")

    taunts = taunt_casts(trace)
    print("  taunts on champions: " + (", ".join(f"{name} {who} {count}" for (name, who), count
                                               in sorted(taunts.items())) or "none"))
    print("  a snare's victim is the champion's target in the last sample before the cast; disarms are casts,"
          " the disarm itself never shows on a champion; champions round a Rebirth are counted at the target's"
          " death spot")


def fc_vetoes(trace: Trace) -> collections.Counter:
    return collections.Counter((rec.get("m"), rec.get("a")) for rec in trace.of("veto")
                               if rec.get("m") in FC_MULTIPLIERS)


def show_vetoes(trace: Trace) -> None:
    print("FACTION CHAMPIONS VETOES")
    counts = fc_vetoes(trace)
    if not counts:
        print(f"  none of the {len(FC_MULTIPLIERS)} multipliers and rules vetoed anything")
        return
    for multiplier in FC_MULTIPLIERS:
        rows = sorted(((count, action) for (name, action), count in counts.items() if name == multiplier),
                      reverse=True)
        if rows:
            print(f"  {multiplier}")
            for count, action in rows:
                print(f"    {count:5}  {action}")
    print("  veto rows, not ticks: one is written when the verdict changes and again every 10 s it holds")


def pack_anchor(trace: Trace) -> tuple[float, float] | None:
    """Where the champions landed: their centroid in the first snapshot from the pull."""
    targets = champions(trace)
    start = pull_start(trace)
    for snap in frames(trace):
        if snap["t"] < start:
            continue
        rows = live_rows(snap, targets)
        if rows:
            return (statistics.mean(row[1] for row in rows), statistics.mean(row[2] for row in rows))
    return None


def pack_centre(rows: list[list], anchor: tuple[float, float], kill: int) -> tuple[float, float] | None:
    """The middle of the champions within 25 yd of the landing spot, the kill target left out; of all the
    others when none is that close."""
    others = [row for row in rows if row[0] != kill]
    near = [row for row in others if distance(row, [0, *anchor]) <= BALL_ANCHOR_RADIUS]
    pick = near or others
    if not pick:
        return None
    return (statistics.mean(row[1] for row in pick), statistics.mean(row[2] for row in pick))


def ball_windows(trace: Trace) -> list[dict]:
    """Per 10 s from the pull: `start`, champion targets by role (`targets`, a Counter, `other` for
    anything not on the roster), and the median distance of live `ranged` and `heal` to the pack centre."""
    anchor = pack_anchor(trace)
    if anchor is None:
        return []
    targets = champions(trace)
    roster = roster_guids(trace)
    kill = kill_spans(trace)
    start = pull_start(trace)
    windows: dict[int, dict] = {}
    for snap in frames(trace):
        if snap["t"] < start:
            continue
        index = (snap["t"] - start) // BALL_WINDOW_MS
        window = windows.setdefault(index, {"start": start + index * BALL_WINDOW_MS,
                                            "targets": collections.Counter(), "ranged": [], "heal": []})
        rows = live_rows(snap, targets)
        for row in rows:
            victim = row[7] if len(row) > 7 else 0
            window["targets"][trace.role(victim) if victim in roster else "other"] += 1
        centre = pack_centre(rows, anchor, held_at(kill, snap["t"]))
        if centre is None:
            continue
        for row in live_rows(snap, roster):
            role = trace.role(row[0])
            if role in ("ranged", "heal"):
                window[role].append(distance(row, [0, *centre]))
    return [{**window, "ranged": statistics.median(window["ranged"]) if window["ranged"] else None,
             "heal": statistics.median(window["heal"]) if window["heal"] else None}
            for _, window in sorted(windows.items())]


def kill_isolation(trace: Trace) -> list[dict]:
    """Per kill target hold: `guid`, `start`, `stop`, the median distance to the pack centre (`pack`) and the
    share of its samples with no other champion healer within 40 yd (`clear`), None without samples."""
    anchor = pack_anchor(trace)
    if anchor is None:
        return []
    targets = champions(trace)
    healers = {guid for guid, _, healer in lineup(trace) if healer}
    out = []
    for guid, start, stop in kill_spans(trace):
        if not guid:
            continue
        gaps, clear = [], []
        for snap in frames(trace):
            if not start <= snap["t"] < stop:
                continue
            rows = live_rows(snap, targets)
            row = next((row for row in rows if row[0] == guid), None)
            centre = pack_centre(rows, anchor, guid)
            if row is None or centre is None:
                continue
            gaps.append(distance(row, [0, *centre]))
            clear.append(all(distance(row, other) > BALL_HEAL_REACH
                             for other in rows if other[0] in healers and other[0] != guid))
        out.append({"guid": guid, "start": start, "stop": stop,
                    "pack": statistics.median(gaps) if gaps else None,
                    "clear": sum(clear) / len(clear) if clear else None})
    return out


def escapee_seconds(trace: Trace) -> dict[int, float]:
    """Per champion, seconds spent past 20 yd of where the pack landed with a victim that isn't a tank."""
    anchor = pack_anchor(trace)
    if anchor is None:
        return {}
    targets = champions(trace)
    start = pull_start(trace)
    out: dict[int, float] = collections.defaultdict(float)
    previous = None
    for snap in frames(trace):
        if snap["t"] < start:
            continue
        if previous is not None:
            step = (snap["t"] - previous) / 1000
            for row in live_rows(snap, targets):
                victim = row[7] if len(row) > 7 else 0
                if distance(row, [0, *anchor]) > BALL_ESCAPE and trace.role(victim) != "tank":
                    out[row[0]] += step
        previous = snap["t"]
    return dict(out)


def champion_trinkets(trace: Trace) -> list[tuple[int, int]]:
    """`(t, champion)` for each champion PvP trinket cast."""
    targets = champions(trace)
    return [(rec["t"], rec.get("s")) for rec in trace.of("cast")
            if rec.get("sp") == SPELL_PVP_TRINKET and rec.get("s") in targets]


def duty_holds(trace: Trace, key: str) -> list[dict]:
    """Each hold of a per-bot probe: `bot`, `value`, `start`, `stop`."""
    out = []
    for bot, spans in holder_spans(trace, key, pull_end(trace)).items():
        for value, start, stop in spans:
            if str(value) not in ("", "0"):
                out.append({"bot": bot, "value": value, "start": start, "stop": stop})
    return sorted(out, key=lambda row: (row["start"], row["bot"]))


def first_engage(trace: Trace) -> dict | None:
    """The raid's first cast on a champion: `t`, `by` (the roster member, a pet's owner), `spell`, `target`."""
    targets = champions(trace)
    found = None
    for rec in trace.of("cast"):
        caster = raid_caster(trace, rec.get("s"))
        if rec.get("tr") or rec.get("tgt") not in targets or caster is None:
            continue
        if found is None or rec["t"] < found["t"]:
            found = {"t": rec["t"], "by": caster, "spell": rec.get("sp"), "target": rec["tgt"]}
    return found


def first_picks(trace: Trace) -> list[tuple[int, int, int]]:
    """`(t, champion, victim)` for each champion's first sampled target on the roster, in time order."""
    targets = champions(trace)
    roster = roster_guids(trace)
    seen: dict[int, tuple[int, int]] = {}
    for snap in frames(trace):
        for row in live_rows(snap, targets):
            victim = row[7] if len(row) > 7 else 0
            if row[0] not in seen and victim in roster:
                seen[row[0]] = (snap["t"], victim)
    return sorted((when, guid, victim) for guid, (when, victim) in seen.items())


def show_ball(trace: Trace) -> None:
    print("THE DEATH BALL")
    engage = first_engage(trace)
    if engage:
        who = "human" if engage["by"] in trace.humans else "bot"
        print(f"  first raid cast on a champion: {clock(engage['t'])} {trace.name(engage['by'])} ({who})"
              f" {trace.spell(engage['spell'])} on {trace.name(engage['target'])}")
    picks = first_picks(trace)
    if picks:
        roles = collections.Counter(trace.role(victim) for _, _, victim in picks)
        print(f"  first picks: tank {roles['tank']} of {len(picks)} ({100 * roles['tank'] // len(picks)}%), "
              + ", ".join(f"{role} {roles[role]}" for role in PACK_ROLES[1:]))
    windows = ball_windows(trace)
    if not windows:
        print("  no champion in the snapshots")
        return
    print(f"  {'from':>8} {'champion targets tank/melee/ranged/heal/other %':47} {'ranged':>7} {'heal':>6}")
    for window in windows:
        total = sum(window["targets"].values()) or 1
        shares = "/".join(f"{100 * window['targets'][role] // total:3d}"
                          for role in ("tank", "melee", "ranged", "heal", "other"))
        ranged = "-" if window["ranged"] is None else f"{window['ranged']:.1f}"
        heal = "-" if window["heal"] is None else f"{window['heal']:.1f}"
        print(f"  {clock(window['start']):>8} {shares:47} {ranged:>7} {heal:>6}")

    rows = kill_isolation(trace)
    if rows:
        print(f"\n  {'kill target':20} {'from':>9} {'length':>7} {'off pack':>8} {'clear of healers':>16}")
        for row in rows:
            pack = "-" if row["pack"] is None else f"{row['pack']:.1f}"
            clear = "-" if row["clear"] is None else f"{100 * row['clear']:.0f}%"
            print(f"  {trace.name(row['guid'])[:20]:20} {clock(row['start']):>9}"
                  f" {(row['stop'] - row['start']) / 1000:6.1f}s {pack:>8} {clear:>16}")

    loose = escapee_seconds(trace)
    if loose:
        print("\n  escapees, s off the landing spot not on a tank: " + ", ".join(
            f"{trace.name(guid)} {seconds:.0f}" for guid, seconds in sorted(loose.items(), key=lambda kv: -kv[1])
            if seconds >= 1))
    tanks = duty_holds(trace, PACK_TANK)
    print(f"  {PACK_TANK}: " + (", ".join(sorted({trace.name(hold['bot']) for hold in tanks})) if tanks else "none"))
    openers = duty_holds(trace, OPENER)
    for hold in openers:
        spell = as_guid(hold["value"])
        print(f"  {OPENER}: {trace.name(hold['bot'])} {trace.spell(spell) if spell else hold['value']}"
              f" {clock(hold['start'])}-{clock(hold['stop'])}")
    if not openers:
        print(f"  {OPENER}: none")
    trinkets = champion_trinkets(trace)
    print("  champion PvP trinkets: " + (", ".join(f"{trace.name(guid)} {clock(when)}" for when, guid in trinkets)
                                         if trinkets else "none"))
    print(f"  the pack centre is the champions within {BALL_ANCHOR_RADIUS:.0f} yd of where they landed (their"
          f" centroid in the first snapshot), the kill target left out; clear means no other champion healer"
          f" within {BALL_HEAL_REACH:.0f} yd; ranged and heal are medians to the pack centre in yd")


SECTIONS = (
    ("kill", "each kill target hold: why, how long, health, who was on it", show_kill),
    ("cc", "CC duties, CC held and fear uptime per champion, casts started and landed, the pace", show_cc),
    ("heals", "champion cast-time heals and the interrupts that followed", show_heals),
    ("burst", "lust, Exhaustion or Sated carried in, first burst cooldown per bot, burst vetoes", show_burst),
    ("fear", "fears on the raid, Fear Ward and Tremor Totem", show_fear),
    ("dispel", "champion CC on the raid, the dispels on it, Unstable Affliction backlash", show_dispel),
    ("purge", "raid offensive dispels on champions, kill target or not, by spell and class", show_purge),
    ("hop", "champion shields, fc.physical holds, Mass Dispel after a shield, the return", show_hop),
    ("aoe", "raid victims per champion crowd-AoE cast, hits per victim for the channels", show_aoe),
    ("adds", "champion totems and treants, treant damage, fc.totem holds", show_adds),
    ("pack", "champions round each role, early melee deaths, first kill, longest stall, grips", show_pack),
    ("peel", "snares and peels on melee champions, Disarm against Bladestorm, externals, battle res, taunts",
     show_peel),
    ("ball", "the death ball: the start and first picks, champion targets by role, kill target off the pack,"
     " escapees, opener", show_ball),
    ("vetoes", "the Faction Champions multipliers and rules, by action", show_vetoes),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
