#!/usr/bin/env python3
"""Score an Algalon the Observer pull from a RaidObs trace: Big Bang, stars, holes, constellations."""
from __future__ import annotations

import bisect
import collections
import functools
import math
import pathlib
import re
import statistics
import sys

# Run as a script from bosses/, so the raidobs package one level up is not on the path yet.
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from raidobs.cli import run_sections  # noqa: E402
from raidobs.encounter import encounter_of  # noqa: E402
from raidobs.geometry import NUMBER, Unknown, anchor, dist2, first_seen, frames, guids_of_entry, radius  # noqa: E402
from raidobs.paths import RAID_ROOT  # noqa: E402
from raidobs.probes import emitted_keys, holder_spans, latch_spans, silent_keys  # noqa: E402
from raidobs.records import STRIP_AT_DEATH_MS  # noqa: E402
from raidobs.space import show_share, threat_share  # noqa: E402
from raidobs.stuck import stall_windows  # noqa: E402
from raidobs.trace import Trace, clock, combat_deaths, death_records, notes, roster_guids  # noqa: E402
from raidobs.validity import DIFFICULTY  # noqa: E402

ENCOUNTER = "algalon"

NPC_ALGALON = 32871
NPC_ALGALON_ENTRIES = (NPC_ALGALON, 33070)
NPC_COLLAPSING_STAR = 32955
NPC_BLACK_HOLE = 32953
NPC_WORM_HOLE = 34099
NPC_LIVING_CONSTELLATION = 33052
NPC_COSMIC_SMASH_MARKERS = (33104, 33105)
NPC_UNLEASHED_DARK_MATTER = 34097
NPC_DARK_MATTER = 33089
HOLE_ENTRIES = (NPC_BLACK_HOLE, NPC_WORM_HOLE)

SPELL_BIG_BANG = (64443, 64584)
SPELL_BIG_BANG_UNPHASE = 64445
SPELL_PHASE_PUNCH = 64412
SPELL_PHASE_PUNCH_PHASE = 64417
SPELL_BLACK_HOLE_PHASE = 62168
SPELL_WORM_HOLE_PHASE = 65250
HOLE_PHASE = (SPELL_BLACK_HOLE_PHASE, SPELL_WORM_HOLE_PHASE)
PHASE_AURAS = HOLE_PHASE + (SPELL_PHASE_PUNCH_PHASE,)
SPELL_QUANTUM_STRIKE = (64395, 64592)
BOSS_SWINGS = (0,) + SPELL_QUANTUM_STRIKE
SPELL_COSMIC_SMASH = (62311, 64596)
SPELL_BLACK_HOLE_EXPLOSION = (64122, 65108)
SPELL_BLACK_HOLE_DOT = 62169
SPELL_CONSTELLATION_BARRAGE = (64599, 64607)
SPELL_ASCEND = 64487
SPELL_WAND_SHOOT = 5019
SPELL_DISPERSION = 47585
SPELL_PAIN_SUPPRESSION = 33206
SPELL_GUARDIAN_SPIRIT = 47788
SOAK_SPELLS = (SPELL_DISPERSION, SPELL_PAIN_SUPPRESSION, SPELL_GUARDIAN_SPIRIT)
SPELL_RIGHTEOUS_DEFENSE = 31789
SPELL_ICE_BLOCK = 45438
SPELL_TRICKS_OF_THE_TRADE = 57934
SPELL_MISDIRECTION = 34477
REDIRECT_IDS = {SPELL_TRICKS_OF_THE_TRADE: "Tricks of the Trade", SPELL_MISDIRECTION: "Misdirection"}
# matched by name, which covers every rank; the ids only cover a trace that names no spells
TAUNT_IDS = {355: "Taunt", 62124: "Hand of Reckoning", SPELL_RIGHTEOUS_DEFENSE: "Righteous Defense", 6795: "Growl",
             56222: "Dark Command", 49576: "Death Grip", 1161: "Challenging Shout", 5209: "Challenging Roar",
             **{rank: "Mocking Blow" for rank in (694, 7400, 7402, 20559, 20560, 25266)}}
TAUNT_NAMES = frozenset(TAUNT_IDS.values())
# no target of their own, so they count when he stood within AREA_TAUNT_YD of the caster
AREA_TAUNTS = frozenset(("Challenging Shout", "Challenging Roar"))
# The buttons the soak keeps for Big Bang: tank defensives, the paladin immunities and the externals.
RESERVED_IDS = {498: "Divine Protection", 642: "Divine Shield", 48788: "Lay on Hands", 33206: "Pain Suppression",
                47788: "Guardian Spirit", 6940: "Hand of Sacrifice", 64205: "Divine Sacrifice", 871: "Shield Wall",
                12975: "Last Stand", 48792: "Icebound Fortitude", 61336: "Survival Instincts", 22812: "Barkskin",
                55233: "Vampiric Blood", 22842: "Frenzied Regeneration", SPELL_DISPERSION: "Dispersion"}
RESERVED_NAMES = frozenset(RESERVED_IDS.values())
SOAK_NODES = ("algalon soaker prep action", "algalon soaker rescue action")
PRIEST_CLASS = (5, "priest")
SMASH_MOVER = "algalon cosmic smash action"
# A charge writes a cast row, never a move row. Feral Charge (cat) lands behind its target.
SPELL_CHARGES = {11578: "charge", 20252: "intercept", 49376: "feral charge - cat", 16979: "feral charge - bear"}


def _anchor_or(name: str, fallback):
    try:
        return anchor(name)
    except Unknown:
        return fallback


def _radius_or(name: str, fallback: float) -> float:
    try:
        return radius(name)
    except Unknown:
        return fallback


# Read from the source so a retune shows up here without a second edit. Fallbacks for a checkout that
# doesn't declare one, or not as a literal.
ROOM_CENTER = _anchor_or("ULDUAR_ALGALON_ROOM_CENTER", (1632.668, -302.7656, 417.3211))
# T, where the holding tank stands; A, where he is expected and the ring's centre; T2, the other pair tank.
TANK_SLOT = _anchor_or("ULDUAR_ALGALON_TANK_SLOT", (1637.957, -340.396, 417.321))
HOLD_SPOT = _anchor_or("ULDUAR_ALGALON_HOLD_SPOT", (1636.843, -332.474, 417.321))
OFFTANK_SLOT = _anchor_or("ULDUAR_ALGALON_OFFTANK_SLOT", (1633.300, -336.001, 417.321))
# where the pair waits for the intro to end
PICKUP_SLOT = _anchor_or("ULDUAR_ALGALON_PICKUP_SLOT", (1633.781, -310.688, 417.321))
PULL_OFFTANK_SLOT = _anchor_or("ULDUAR_ALGALON_PULL_OFFTANK_SLOT", (1628.668, -309.694, 417.321))
ROOM_RADIUS = _radius_or("ULDUAR_ALGALON_ROOM_RADIUS", 47.0)
SHELTER_RADIUS = _radius_or("ULDUAR_ALGALON_SHELTER_RADIUS", 6.0)
# combat reach 9 + 1.5 + 4/3 against a player
MELEE_REACH = _radius_or("ULDUAR_ALGALON_MELEE_REACH", 11.83)
DODGE_TRIGGER_YD = _radius_or("ULDUAR_ALGALON_COSMIC_SMASH_TRIGGER_RADIUS", 12.0)
CONSTELLATION_LOW_Z = _radius_or("ULDUAR_ALGALON_CONSTELLATION_LOW_Z", 5.0)
UNDER_LEAD_MS = int(_radius_or("ULDUAR_ALGALON_UNDER_LEAD_MS", 4000.0))
UNDER_TAIL_MS = int(_radius_or("ULDUAR_ALGALON_UNDER_TAIL_MS", 600.0))
UNDER_TEAM_PCT = _radius_or("ULDUAR_ALGALON_UNDER_TEAM_PCT_PER_SEC", 6.0)

ALGALON_SOURCE = RAID_ROOT / "Uld" / "Util" / "UldEncounter_Algalon.cpp"
RING_SLOT_TABLE = re.compile(r"\bULDUAR_ALGALON_RING_SLOTS\s*=\s*\{\{?(.*?)\}?\};", re.S)
RING_SLOT_ROW = re.compile(r"\bPosition\s*\(\s*" + NUMBER + r"\s*,\s*" + NUMBER + r"\s*,\s*" + NUMBER + r"\s*\)")
# A row's comment, `// healer, 19 yd from the hold at +85 deg`, or before the edge hold `// healer, 19 yd at 0 deg`.
RING_SLOT_NOTE = re.compile(r"//\s*([A-Za-z][A-Za-z ]*?)\s*,\s*(\d+(?:\.\d+)?)\s*yd\b[^\n]*?\bat\s+([+-]?\d+(?:\.\d+)?)"
                            r"\s*deg")

# uint32 in the source, or not named there at all.
BIG_BANG_CAST_MS = 8000
# the soaker latch clears to 0 a tick or two after the hit
SOAKER_TAIL_MS = 1500
SHELTER_WINDOW_MS = 30000
GATHER_LEAD_MS = 10000
# every class taunt the handler can be (Taunt, Hand of Reckoning, Growl, Dark Command)
TAUNT_COOLDOWN_MS = 8000
KITE_BLOCKED_MS = 5000
# a woken constellation comes down within 23 s
STUCK_MIN_MS = 25000
STAR_GAP_MS = 8000
ENRAGE_MS = 360000
PHASE_AURA_MS = 10000
LOOP_MS = 10500
P2_HP = 20.0
# He resets whenever no living, unphased player is inside this.
RESET_RANGE = 120.0
SMASH_DELAY_MS = 4800
SWAP_STACKS = 4
# Black Hole Explosion's top roll, by raid size. Someone with less max health than this is safe only at
# full health.
EXPLOSION_SAFE_HP = {10: 16913, 25: 21526}

PHASE_NAMES = {"0": "idle", "1": "intro", "2": "P1", "3": "P2", "4": "won"}

HIT_SLACK_MS = 3000
DEFENSIVE_LEAD_MS = 3000
EVADE_WINDOW_MS = 6000
# a re-application this soon after a removal continues the same phased stretch
CHAIN_MS = 1500
# a unit last sampled this close to the end is still up
GONE_MS = 1000
BURST_GAP_MS = 1000
HOLE_MATCH_MS = 3000
HOLE_MATCH_YD = 10.0
SMASH_MATCH_MS = 2000
SMASH_BANDS = ((6.0, "<6"), (10.0, "6-10"), (15.0, "10-15"), (math.inf, "15+"))
WAVE_MS = 1000
IMPACT_MIN_MS = 3500
IMPACT_MAX_MS = 6500
SMASH_NEAR_YD = 20.0
SMASH_INSIDE_YD = 10.0
SMASH_CLEAR_YD = 15.0
# a charge has landed by then
CHARGE_LAND_MS = 1000
METEOR_LIVE_MS = 5500
STALL_MS = 3000
PICKUP_MS = 1000
LOOSE_MS = 3000
OFF_TEAM_MS = 3000
KILLED_HP = 5.0
MAX_STEP_MS = 2000
TIMELINE_SHOWN = 16

ON_SLOT_YD = _radius_or("ULDUAR_ALGALON_HOLDER_ON_SPOT_RADIUS", 3.0)
# the melee release timer, a uint32 in the source
DRAG_DUE_MS = 8000
HOLD_STEP_MS = 5000
HOLDER_TARGET_MS = 1000
# a threat redirect this soon after phase 1 opened lands while the holder's lead is still thin
OPENER_MS = 20000
# a taunt took when his victim became the caster this soon; a later change is still shown
TAUNT_TAKE_MS = 600
TAUNT_LATE_MS = 3000
AREA_TAUNT_YD = 10.0
# one press of a button, however many cast rows it wrote
ECHO_MS = 1000
SOAK_AFTER_MS = 5000
SOAK_DAMAGE_MS = 3000
HEAL_WAIT_MS = 10000
HANDOVER_LEAD_MS = 20000
CHEWED_PCT = 10.0
# Mostly ticks, so it keeps going once the team moves off the star; bots hold these on stars.
STAR_DOT_NAMES = frozenset(("Corruption", "Unstable Affliction", "Curse of Agony", "Curse of Doom", "Seed of Corruption",
                            "Immolate", "Moonfire", "Insect Swarm", "Shadow Word: Pain", "Vampiric Touch",
                            "Devouring Plague", "Flame Shock", "Serpent Sting", "Living Bomb"))
# the probe's own allowance for drain-only health at death
DRAINED_SLACK = _radius_or("ULDUAR_ALGALON_STAR_DRAIN_SLACK_PCT", 3.0)
WORST_WINDOW_MS = 10000
TEAM_FAR_YD = 40.0
# a nearest hole further than this at the cast start can't be reached before the impact
HOLE_REACH_YD = 43.0
NEARHOLE_YD = _radius_or("ULDUAR_ALGALON_FAR_HOLE_YARDS", 35.0)
NEARHOLE_LEAD_MS = 15000

# The phase latch and the pull note trail his first swing by up to a tick.
INTRO_LEAD_MS = 1500
INTRO_CAUSE_MS = 2000
OPENING_MS = 10000
FIRST_SWINGS = 5
TAUNT_MS = 1000
# the snapshot names a new victim up to a sample after it changed
SAMPLE_LAG_MS = 300
CAUSE_MS = 1500
LOOSE_MIN_MS = 300
# he drops one victim a moment before he picks the next
BRIDGE_MS = 2000
KILL_MATCH_MS = 1000
SHARE_MS = 3000
EXPLOSION_SHARE = 0.5
MATTER_BANG_MS = 3000
BANG_LEAD_MS = 1000
REPHASE_MS = 2000
NEW_HOLE_MS = 1500
NEW_HOLE_YD = 7.0
STAR_NOTE_MS = 2000
STAR_NEAR_YD = 9.0
DEATH_ROLES = ("tank", "heal", "melee", "ranged")
# A tank death is read over this long before it, and its spike over the shorter span.
TANKHEAL_WINDOW_MS = 6000
TANKHEAL_BURST_MS = 2000
# More than this of an assigned tank healer's healing on anyone but the pair and itself is a fault.
TANKHEAL_OTHERS_SHARE = 0.30
TANKHEAL_RANGE_YD = 40.0
TANKHEAL_VETO = "algalon tank healer focus"
SPELL_BEACON_OF_LIGHT = 53563

SPELL_HAND_OF_SACRIFICE = 6940
SPELL_AURA_MASTERY = 31821
SPELL_DIVINE_SACRIFICE = 64205
SPELL_DIVINE_GUARDIAN = 70940
# The `tank` rows of the table in RaidTankDefensive.cpp.
TANK_BUTTONS = {12975: "Last Stand", 871: "Shield Wall", 498: "Divine Protection", 22812: "Barkskin",
                22842: "Frenzied Regeneration", 61336: "Survival Instincts", 48707: "Anti-Magic Shell",
                55233: "Vampiric Blood", 48792: "Icebound Fortitude"}
TANK_BUTTON_NAMES = frozenset(TANK_BUTTONS.values())
CUTTERS = {**TANK_BUTTONS, SPELL_PAIN_SUPPRESSION: "Pain Suppression", SPELL_HAND_OF_SACRIFICE: "Hand of Sacrifice",
           SPELL_GUARDIAN_SPIRIT: "Guardian Spirit"}
CUTTER_AURAS = {**CUTTERS, 12976: "Last Stand"}
TANK_SAVES = {**CUTTERS, **{rank: "Lay on Hands" for rank in (633, 2800, 10310, 27154, 48788)}}
TANK_SAVE_NAMES = frozenset(TANK_SAVES.values())
COVER_SPELLS = {SPELL_AURA_MASTERY: "Aura Mastery", SPELL_DIVINE_SACRIFICE: "Divine Sacrifice"}
SAVE_LIVED_MS = 10000
TANKDEF_NOTE_MS = 10000
# Aura Mastery and Divine Sacrifice both cool down this long; Aura Mastery and Divine Guardian last COVER_MS.
COVER_COOLDOWN_MS = 120000
COVER_MS = 6000
SACRIFICE_HELD_MS = 1000

SPELL_NITRO_BOOSTS = 54861
NITRO_LEAD_MS = 3000
IMMUNITIES = {SPELL_ICE_BLOCK: "Ice Block", 642: "Divine Shield", 31224: "Cloak of Shadows",
              48707: "Anti-Magic Shell"}
IMMUNITY_STRIP_MS = 1500
# The `PERSONAL_BUTTONS` of the explosion node by lower-cased name, with the cooldown each is judged ready after.
PERSONAL_COOLDOWN_MS = {"shadow ward": 30000, "anti magic shell": 45000, "barkskin": 60000,
                        "shamanistic rage": 60000, "cloak of shadows": 90000, "icebound fortitude": 120000,
                        "dispersion": 120000, "divine protection": 180000, "ice block": 300000,
                        "divine shield": 300000}
PERSONAL_IMMUNE = frozenset(("anti magic shell", "cloak of shadows", "ice block", "divine shield"))
PERSONAL_IDS = {**IMMUNITIES, 498: "Divine Protection", 22812: "Barkskin", 48792: "Icebound Fortitude",
                30823: "Shamanistic Rage", SPELL_DISPERSION: "Dispersion", 47891: "Shadow Ward"}
PERSONAL_LABELS = {name.lower().replace("-", " "): name for name in PERSONAL_IDS.values()}
# every bot of the class has these. Dispersion and Shamanistic Rage are talents, counted once seen cast
CLASS_BUTTONS = {2: ("divine protection", "divine shield"), 4: ("cloak of shadows",),
                 6: ("anti magic shell", "icebound fortitude"), 8: ("ice block",), 9: ("shadow ward",),
                 11: ("barkskin",)}
CLASS_IDS = {"paladin": 2, "rogue": 4, "death knight": 6, "deathknight": 6, "mage": 8, "warlock": 9, "druid": 11}
SPELL_FORBEARANCE = 25771
FORBEARANCE_BUTTONS = frozenset(("divine protection", "divine shield"))
# the "algalon explosion button hold" list (older traces: "algalon phased button hold", phased only). The immunities
# have a hold of their own, Dispersion none
BUTTONS_HELD = frozenset(("barkskin", "shadow ward", "icebound fortitude", "shamanistic rage", "divine protection"))
BUTTON_HOLD_PCT = _radius_or("ULDUAR_ALGALON_BUTTON_HOLD_PCT", _radius_or("ULDUAR_ALGALON_PHASED_BUTTON_PCT", 35.0))
# ULDUAR_ALGALON_EXPLOSION_COVER_MIN_UNDER, a uint32 the declaration reader can't see
COVER_MIN_UNDER = 2
COVERED_FACTOR = _radius_or("ULDUAR_ALGALON_COVERED_EXPLOSION_FACTOR", 0.8)
# by name, so it also reads the "healthstone"/"healing potion" an `algalon.personal` row names; the ids only
# for a trace that names no spells
CONSUMABLE = re.compile(r"healthstone|healing (?:potion|injector)|rejuvenation potion", re.I)
CONSUMABLE_IDS = {47875: "Master Healthstone", 47876: "Master Healthstone", 47877: "Master Healthstone",
                  43185: "Healing Potion", 67489: "Runic Healing Injector", 53761: "Rejuvenation Potion"}
COVER_NODE = "algalon explosion cover action"
# a press is for the star the scan says dies next, on a forecast that can run late, or for one already
# exploded that the scan still lists
PRESS_AHEAD_MS = 5000
PRESS_STALE_MS = 1000
PERSONAL_LOOK_MS = 5000
IMMUNITY_HOLD_MS = 20000
WINDOW_CUT_PCT = 80.0
STAR_PLAN_SLACK_MS = 1000
# the planner keeps a cast's under-stars until the strip is past
PLAN_KEEP_MS = 1200
UNDER_DROP_LATE_MS = 8000
TRIM_RESTART_MS = 2000
TRIM_RESTARTS = 3
STAR_BANG_MS = 60000
PARKED_MS = 3000
PARKED_HOLE_YD = 10.0
EATEN_SLACK_MS = HOLE_MATCH_MS
KITE_STAGES = frozenset(("taunt", "behind", "stage", "wait", "cross", "parked", "hole"))


def secs(ms: int) -> str:
    return f"{ms / 1000:.1f}s"


def applications(count: int) -> str:
    return f"{count} application{'s' if count != 1 else ''}"


def pull_end(trace: Trace) -> int:
    ends = trace.of("end")
    if ends:
        return ends[-1]["t"]
    return trace.records[-1].get("t", 0) if trace.records else 0


def boss_guid(trace: Trace) -> int | None:
    guids = sorted(guid for entry in NPC_ALGALON_ENTRIES for guid in guids_of_entry(trace, entry))
    return guids[0] if guids else None


_UNSET = object()


def cached(trace: Trace, name: str, build):
    """`build(trace)`, kept on the trace: every view asks for the same derived tracks."""
    found = getattr(trace, name, _UNSET)
    if found is _UNSET:
        found = build(trace)
        setattr(trace, name, found)
    return found


def as_guid(text) -> int:
    try:
        return int(text)
    except (TypeError, ValueError):
        return 0


def who(trace: Trace, guid) -> str:
    return f"{trace.name(guid)} ({trace.role(guid)})"


def label_of(trace: Trace, guid) -> str:
    """`who`, with a pet named as its owner's and a human starred."""
    owner = trace.owners.get(guid)
    if owner:
        return f"{trace.names.get(guid, trace.name(guid))} ({trace.name(owner)}'s pet)"
    return who(trace, guid) + ("*" if guid in trace.humans else "")


def spell_name(trace: Trace, spell, ids: dict[int, str]) -> str:
    """The spell's name from the trace, else from `ids`, else empty."""
    return trace.spells.get(spell) or ids.get(spell, "")


def class_of(trace: Trace, guid):
    classes = cached(trace, "_algalon_classes",
                     lambda trace: {member["g"]: member.get("c") for member in trace.header.get("roster", [])})
    return classes.get(guid)


def emitted(trace: Trace) -> set[str]:
    cached = getattr(trace, "_algalon_emitted", None)
    if cached is None:
        cached = emitted_keys(trace)
        trace._algalon_emitted = cached
    return cached


def absent(trace: Trace, *keys: str) -> list[str]:
    have = emitted(trace)
    return [key for key in keys if key not in have]


def show_absent(trace: Trace, *keys: str) -> None:
    gone = absent(trace, *keys)
    if gone:
        print(f"  absent from this trace: {', '.join(gone)}")


def value_at(spans, when: int):
    """What a `latch_spans`/`holder_spans` track held at `when`, or None outside every span."""
    for value, start, stop in spans:
        if start <= when < stop:
            return value
    return None


def last_set_in(spans, low: int, high: int) -> int:
    """The last non-zero guid a latch held anywhere in `[low, high]`."""
    found = 0
    for value, start, stop in spans:
        if start <= high and stop > low and as_guid(value):
            found = as_guid(value)
    return found


def latch_chain(trace: Trace, spans, low: int, high: int, impact: int) -> list[tuple[int, int, str]]:
    """`(t, guid, cause)` for the guid a raid-wide latch held at `low` and each change up to `high`, 0 a clear.
    `cause`: the old guid's hole phase or death within CAUSE_MS; a clear from `impact` on is the cast ending."""
    phased = hole_phases(trace)
    deaths = death_records(trace)
    out: list[tuple[int, int, str]] = []
    previous = 0
    for value, start, stop in spans:
        guid = as_guid(value)
        if start > high or stop <= low or not (out or guid):
            continue
        cause = ""
        if previous and (guid or start < impact):
            began = next((begin for begin, until, _ in phased.get(previous, ())
                          if begin <= start and until >= start - CAUSE_MS), None)
            died = next((rec["t"] for rec in deaths
                         if rec.get("g") == previous and start - CAUSE_MS <= rec["t"] <= start), None)
            if began is not None:
                cause = f"{trace.name(previous)} phased {clock(began)}"
            elif died is not None:
                cause = f"{trace.name(previous)} died {clock(died)}"
        out.append((start, guid, cause))
        previous = guid
    return out


def chain_label(trace: Trace, chain) -> str:
    return ", ".join(f"{clock(when)} {trace.name(guid) if guid else 'cleared'}" + (f" ({cause})" if cause else "")
                     for when, guid, cause in chain)


class Samples:
    """Every snapshot row by guid and by frame, so a view can ask for a row at a time, or for what one
    frame held, without rescanning."""

    def __init__(self, trace: Trace):
        self.stamps: list[int] = []
        self.units: list[dict[int, list]] = []
        self.rows: dict[int, tuple[list[int], list[list]]] = {}
        for snap in frames(trace):
            when = snap["t"]
            self.stamps.append(when)
            units = {}
            for row in snap.get("u", []):
                units[row[0]] = row
                stamps, rows = self.rows.setdefault(row[0], ([], []))
                stamps.append(when)
                rows.append(row)
            self.units.append(units)

    def row(self, guid: int, when: int):
        """The guid's last row at or before `when`, or None."""
        found = self.rows.get(guid)
        if not found:
            return None
        index = bisect.bisect_right(found[0], when)
        return found[1][index - 1] if index else None

    def between(self, guid: int, low: int, high: int) -> list[list]:
        found = self.rows.get(guid)
        if not found:
            return []
        return found[1][bisect.bisect_left(found[0], low):bisect.bisect_right(found[0], high)]

    def timed(self, guid: int, low: int, high: int) -> list[tuple[int, list]]:
        """`between`, each row with its stamp."""
        found = self.rows.get(guid)
        if not found:
            return []
        low, high = bisect.bisect_left(found[0], low), bisect.bisect_right(found[0], high)
        return list(zip(found[0][low:high], found[1][low:high]))

    def after(self, guid: int, when: int):
        """The guid's first row after `when`, or None."""
        found = self.rows.get(guid)
        if not found:
            return None
        index = bisect.bisect_right(found[0], when)
        return found[1][index] if index < len(found[1]) else None

    def frame(self, when: int) -> dict[int, list]:
        """Every row of the last snapshot at or before `when`."""
        index = bisect.bisect_right(self.stamps, when)
        return self.units[index - 1] if index else {}

    def seen(self, guid: int) -> tuple[int, int] | None:
        found = self.rows.get(guid)
        return (found[0][0], found[0][-1]) if found else None

    def present(self, guid: int, when: int) -> bool:
        """From the guid's first row until the snapshot after its last, gaps in between included."""
        seen = self.seen(guid)
        if not seen or when < seen[0]:
            return False
        index = bisect.bisect_right(self.stamps, seen[1])
        return index >= len(self.stamps) or when < self.stamps[index]

    def steps(self, low: int = -(1 << 62), high: int = 1 << 62):
        """`(index, t, weight)` per snapshot in range, weighted by the gap to the next one."""
        for index, when in enumerate(self.stamps):
            if when < low or when >= high:
                continue
            nxt = self.stamps[index + 1] if index + 1 < len(self.stamps) else when
            yield index, when, min(nxt - when, MAX_STEP_MS)

    def unit_steps(self, guid: int, cap: int | None = MAX_STEP_MS):
        """`(t, row, weight)` over one unit's own rows, weighted by the gap to its next row. `cap=None`
        carries a row across a gap in the sweep, for a unit that was there all along."""
        found = self.rows.get(guid)
        if not found:
            return
        stamps, rows = found
        for index, (when, row) in enumerate(zip(stamps, rows)):
            nxt = stamps[index + 1] if index + 1 < len(stamps) else when
            yield when, row, nxt - when if cap is None else min(nxt - when, cap)


def samples_of(trace: Trace) -> Samples:
    cached = getattr(trace, "_algalon_samples", None)
    if cached is None:
        cached = Samples(trace)
        trace._algalon_samples = cached
    return cached


def position_at(samples: Samples, guid: int, when: int) -> tuple[float, float] | None:
    """Where a unit stood at `when`, read linearly between its rows either side. Across a gap longer
    than MAX_STEP_MS the earlier row stands."""
    found = samples.rows.get(guid)
    if not found:
        return None
    stamps, rows = found
    index = bisect.bisect_right(stamps, when)
    if not index:
        return None
    before = rows[index - 1]
    gap = stamps[index] - stamps[index - 1] if index < len(stamps) else 0
    if gap <= 0 or gap > MAX_STEP_MS:
        return before[1], before[2]
    after = rows[index]
    share = (when - stamps[index - 1]) / gap
    return before[1] + (after[1] - before[1]) * share, before[2] + (after[2] - before[2]) * share


def alive(row) -> bool:
    return bool(row) and len(row) > 5 and row[5] > 0


def target_of(row) -> int:
    return row[7] if row and len(row) > 7 else 0


def hole_guids(trace: Trace) -> set[int]:
    return {guid for entry in HOLE_ENTRIES for guid in guids_of_entry(trace, entry)}


def final_despawn(trace: Trace) -> int | None:
    """When whatever was left of the room's adds vanished in one scan well before the file ended: the
    kill or the Ascend despawning them, not a removal or a kill of each."""
    samples = samples_of(trace)
    end = pull_end(trace)
    entries = (NPC_BLACK_HOLE, NPC_WORM_HOLE, NPC_LIVING_CONSTELLATION, NPC_COLLAPSING_STAR,
               NPC_UNLEASHED_DARK_MATTER)
    lasts = [seen[1] for entry in entries for guid in guids_of_entry(trace, entry) if (seen := samples.seen(guid))]
    if not lasts or max(lasts) >= end - GONE_MS:
        return None
    last = max(lasts)
    ascended = any(0 <= last - rec["t"] <= EVADE_WINDOW_MS for rec in trace.of("cast") if rec.get("sp") == SPELL_ASCEND)
    return last if ascended or lasts.count(last) >= 2 else None


def holes_at(samples: Samples, holes: set[int], when: int) -> list[int]:
    return sorted(guid for guid in holes if samples.present(guid, when))


def missing_probes(trace: Trace) -> list[str]:
    """Keys the source declares for Algalon that stayed silent, plus the ones this pull's events
    call for: a key the source does not declare yet is still owed."""
    silent = {key for key, _, _ in silent_keys(emitted_keys(trace), encounter_of(trace))}
    owed = {key for key in expected_probes(trace) if key not in emitted(trace)}
    return sorted(silent | owed)


def show_banner(trace: Trace) -> None:
    end = pull_end(trace)
    outcome = trace.of("end")[-1].get("out", "?") if trace.of("end") else "no end record"
    encounter = encounter_of(trace)
    mode = DIFFICULTY.get(trace.header.get("diff"), f"difficulty {trace.header.get('diff')}")
    print(f"{encounter}  {mode}  {outcome} at {clock(end)}  {len(combat_deaths(trace))} death(s)")

    # the probe check matches keys to the encounter, so on another boss it would pass on nothing
    if encounter != ENCOUNTER:
        print("  not an Algalon pull, every section below reads empty")
        return

    gone = missing_probes(trace)
    print("all algalon.* probes present" if not gone else f"probes absent from this trace: {', '.join(gone)}")


# --------------------------------------------------------------------------------------------- phases

def fallback_phases(trace: Trace) -> list[tuple[int, int, str]]:
    """Spans for a trace without `algalon.phase`. The intro leaves him unselectable, which no record
    shows, so P1 opens on his first cast."""
    boss = boss_guid(trace)
    if boss is None:
        return []
    end = pull_end(trace)
    casts = [rec["t"] for rec in trace.of("cast") if rec.get("s") == boss]
    p1 = min(casts) if casts else 0
    low = next((when for when, row, _ in samples_of(trace).unit_steps(boss) if 0 < row[5] <= P2_HP), None)
    worm = first_seen(trace, guids_of_entry(trace, NPC_WORM_HOLE))
    p2 = min((when for when in (low, worm) if when is not None), default=None)

    marks = [(0, "intro")] if p1 > 0 else []
    marks.append((p1, "P1"))
    if p2 is not None and p2 > p1:
        marks.append((p2, "P2"))
    return [(when, marks[index + 1][0] if index + 1 < len(marks) else end, label)
            for index, (when, label) in enumerate(marks)]


def phases(trace: Trace) -> tuple[list[tuple[int, int, str]], str]:
    """`((start, stop, label), ...)` and where they came from. A phase that comes round twice gets a
    letter, so each span keeps its own deaths."""
    spans = latch_spans(trace, "algalon.phase", pull_end(trace))
    if not spans:
        return fallback_phases(trace), "fallback"
    seen: collections.Counter = collections.Counter()
    out = []
    for value, start, stop in spans:
        label = PHASE_NAMES.get(value, value)
        seen[label] += 1
        if seen[label] > 1:
            label += chr(ord("a") + seen[label] - 1)
        out.append((start, stop, label))
    return out, "algalon.phase"


def phase_start(trace: Trace, label: str) -> int | None:
    return next((start for start, _, name in phases(trace)[0] if name == label), None)


def phase_at(spans, when: int) -> str:
    for start, stop, label in spans:
        if start <= when < stop:
            return label
    if spans and when >= spans[-1][1]:
        return spans[-1][2]
    return "pre"


def show_phases(trace: Trace) -> None:
    print("PHASES")
    spans, source = phases(trace)
    if not spans:
        print("  Algalon was never sampled")
        return
    if source == "fallback":
        print("  absent from this trace: algalon.phase; P1 from his first cast, P2 from 20 % health or"
              " the first Worm Hole")

    samples = samples_of(trace)
    boss = boss_guid(trace)
    dead: dict[str, list[dict]] = collections.defaultdict(list)
    for rec in combat_deaths(trace):
        dead[phase_at(spans, rec["t"])].append(rec)

    for start, stop, label in spans:
        row = samples.row(boss, start) if boss else None
        health = f"boss {row[5]:5.1f}%" if row else "boss     -"
        rows = dead.get(label, [])
        print(f"  {label:6} {clock(start)} .. {clock(stop)}  {(stop - start) / 1000:6.1f} s  {health}"
              f"  {len(rows):2} dead")
        for rec in rows:
            blow = rec.get("blow") or [0, 0]
            print(f"           {clock(rec['t'])}  {trace.name(rec['g'])[:14]:14} {trace.role(rec['g']):6}"
                  f"  by {trace.name(rec.get('killer'))} ({blow[1] if len(blow) > 1 else 0})")
    resets = sum(1 for rec in death_records(trace) if rec.get("cause") == "reset")
    if resets:
        print(f"  plus {resets} to the wipe command")

    end = pull_end(trace)
    p1 = phase_start(trace, "P1")
    if p1 is not None:
        due = p1 + ENRAGE_MS
        print(f"  enrage due {clock(due)}, 360 s after P1 began; the pull used {(end - p1) / 1000:.1f} s"
              f" of it and ended {abs(due - end) / 1000:.1f} s {'short of' if end < due else 'past'} it")
    ascends = sorted(rec["t"] for rec in trace.of("cast") if rec.get("sp") == SPELL_ASCEND)
    for when in ascends:
        since = f", {(when - p1) / 1000:.1f} s after P1 began" if p1 is not None else ""
        print(f"  Ascend to the Heavens at {clock(when)}{since}")
    if not ascends:
        print("  no Ascend to the Heavens cast")


# ------------------------------------------------------------------------------------- his victim

def swings(trace: Trace) -> list[dict]:
    """His melee and Quantum Strike rows, in time order."""
    def build(trace: Trace) -> list[dict]:
        boss = boss_guid(trace)
        return [rec for rec in trace.of("dmg")
                if boss is not None and rec.get("s") == boss and rec.get("sp") in BOSS_SWINGS]
    return cached(trace, "_algalon_swings", build)


def taunt_spell(trace: Trace, rec: dict) -> str:
    """The taunt a cast row pressed, or empty. A triggered row is the spell's own echo, not a press."""
    name = spell_name(trace, rec.get("sp"), TAUNT_IDS)
    return name if name in TAUNT_NAMES and not rec.get("tr") else ""


def taunts(trace: Trace) -> list[tuple[int, int]]:
    """`(t, caster)` per taunt a raid member cast at him. Righteous Defense goes on the friend he is
    hitting, so on a raid member it counts too."""
    def build(trace: Trace) -> list[tuple[int, int]]:
        boss = boss_guid(trace)
        roster = roster_guids(trace)
        return [(rec["t"], rec["s"]) for rec in trace.of("cast")
                if (name := taunt_spell(trace, rec)) and rec.get("s") in roster
                and (rec.get("tgt") == boss or (name == "Righteous Defense" and rec.get("tgt") in roster))]
    return cached(trace, "_algalon_taunts", build)


def taunt_near(trace: Trace, guid: int, when: int) -> int | None:
    """When `guid` taunted him within TAUNT_MS before `when`, or None."""
    return next((cast for cast, caster in taunts(trace)
                 if caster == guid and when - TAUNT_MS <= cast <= when + SAMPLE_LAG_MS), None)


def swap_pair(trace: Trace) -> tuple[frozenset, str]:
    """The two tanks that trade him and where that came from: `algalon.swappair`, else the roster's
    tanks."""
    def build(trace: Trace) -> tuple[frozenset, str]:
        tracks = holder_spans(trace, "algalon.swappair")
        pair = frozenset(guid for guid, spans in tracks.items() if any(value == "1" for value, _, _ in spans))
        if pair:
            return pair, "algalon.swappair"
        return frozenset(guid for guid in roster_guids(trace) if trace.role(guid) == "tank"), "roster tanks"
    return cached(trace, "_algalon_pair", build)


def intro_end(trace: Trace) -> int | None:
    """When he turned on the raid: `algalon.pull`, else P1's start, pulled in to a swing or a victim
    that came first."""
    def build(trace: Trace) -> int | None:
        pulls = notes(trace, "algalon.pull")
        boundary = pulls[0]["t"] if pulls else phase_start(trace, "P1")
        if boundary is None:
            return None
        early = [rec["t"] for rec in swings(trace) if boundary - INTRO_LEAD_MS <= rec["t"] <= boundary]
        early += [when for when, victim in boss_victims(trace)
                  if victim and boundary - INTRO_LEAD_MS <= when <= boundary]
        return min(early, default=boundary)
    return cached(trace, "_algalon_intro", build)


def victim_samples(trace: Trace) -> list[tuple[int, int]]:
    """`(t, victim)` per row of his while he lives and per swing he landed, in time order."""
    def build(trace: Trace) -> list[tuple[int, int]]:
        boss = boss_guid(trace)
        if boss is None:
            return []
        rows = [(when, target_of(row)) for when, row, _ in samples_of(trace).unit_steps(boss) if alive(row)]
        rows += [(rec["t"], rec.get("d", 0)) for rec in swings(trace)]
        return sorted(rows)
    return cached(trace, "_algalon_victims", build)


def all_phases(trace: Trace) -> dict[int, list[list[int]]]:
    return cached(trace, "_algalon_phased", phased_spans)


def hole_phases(trace: Trace) -> dict[int, list[list[int]]]:
    return cached(trace, "_algalon_hole_phased", lambda trace: phased_spans(trace, HOLE_PHASE))


def phased_during(spans, guid: int, low: int, high: int) -> bool:
    return any(start <= high and stop >= low for start, stop, _ in spans.get(guid, ()))


def died_during(trace: Trace, guid: int, low: int, high: int) -> bool:
    return any(rec.get("g") == guid and low <= rec["t"] <= high for rec in death_records(trace))


# ----------------------------------------------------------------------------------------------- pull

def parse_pull(text) -> tuple[int | None, int]:
    """`algalon.pull` reads `inreach=<n> victim=<guid>`."""
    fields = dict(part.split("=", 1) for part in str(text or "").split() if "=" in part)
    try:
        inreach = int(fields.get("inreach", ""))
    except ValueError:
        inreach = None
    return inreach, as_guid(fields.get("victim"))


def opening_frame(samples: Samples, when: int) -> dict[int, list]:
    """The snapshot at or before `when`, so whoever his first swing killed is still in it; the one
    after when nothing came before."""
    frame = samples.frame(when)
    if frame:
        return frame
    index = bisect.bisect_left(samples.stamps, when)
    return samples.units[index] if index < len(samples.units) else {}


def stage_share(trace: Trace, low: int, high: int) -> dict[int, float]:
    """Per melee bot, the share of `[low, high)` it spent on `algalon.spot=stage`."""
    spots = holder_spans(trace, "algalon.spot", pull_end(trace))
    out = {}
    for guid in roster_guids(trace):
        if trace.role(guid) != "melee" or guid in trace.humans or high <= low:
            continue
        held = sum(max(0, min(stop, high) - max(start, low)) for value, start, stop in spots.get(guid, [])
                   if value == "stage")
        out[guid] = held / (high - low)
    return out


def pull_row(trace: Trace) -> dict | None:
    """Where everyone stood when the intro ended, whom he took first, and how long the pair took to
    get him."""
    begin = intro_end(trace)
    boss = boss_guid(trace)
    if begin is None or boss is None:
        return None
    samples = samples_of(trace)
    probe = notes(trace, "algalon.pull")
    frame = opening_frame(samples, begin)
    him = frame.get(boss) or samples.row(boss, begin)
    if not him:
        return None
    pair, _ = swap_pair(trace)
    roster = roster_guids(trace)
    near = sorted((dist2(row[1:3], him[1:3]), guid) for guid, row in frame.items() if guid in roster and alive(row))
    inreach, victim = parse_pull(probe[0].get("txt")) if probe else (None, 0)
    after = [(when, guid) for when, guid in victim_samples(trace) if when >= begin and guid]
    intro = next(((start, stop) for start, stop, label in phases(trace)[0] if label == "intro"), None)
    return {
        "t": begin,
        "probe": bool(probe),
        "spot": tuple(him[1:3]),
        "near": near,
        "inside": [guid for gap, guid in near if gap <= MELEE_REACH and guid not in pair],
        "inreach": inreach,
        "victim": victim or (after[0][1] if after else 0),
        "swings": [rec for rec in swings(trace)
                   if rec["t"] >= begin - SAMPLE_LAG_MS and rec.get("sp") == 0][:FIRST_SWINGS],
        "tanked": next(((when, guid) for when, guid in after if guid in pair), None),
        "deaths": [rec for rec in combat_deaths(trace) if begin - SAMPLE_LAG_MS <= rec["t"] <= begin + OPENING_MS],
        "stage": stage_share(trace, intro[0], begin) if intro else {},
    }


def show_pull(trace: Trace) -> None:
    print("PULL")
    row = pull_row(trace)
    if row is None:
        print("  the intro never ended, or Algalon was never sampled")
        return
    pair, source = swap_pair(trace)
    print(f"  intro ended {clock(row['t'])}{'' if row['probe'] else ' (absent from this trace: algalon.pull)'};"
          f" Algalon at ({row['spot'][0]:.1f}, {row['spot'][1]:.1f}); swap pair from {source}")
    said = f", algalon.pull said {row['inreach']}" if row["inreach"] is not None else ""
    print(f"  inside {MELEE_REACH:.2f} yd outside the pair: {len(row['inside'])}{said}"
          + (": " + ", ".join(who(trace, guid) for guid in row["inside"]) if row["inside"] else ""))
    print("  by distance: " + ", ".join(
        f"{trace.name(guid)}{'*' if guid in pair else '!' if guid in row['inside'] else ''} {gap:.1f}"
        for gap, guid in row["near"]))
    print("    * swap pair, ! in his reach")
    print(f"  first victim: {who(trace, row['victim']) if row['victim'] else '-'}")
    print("  first swings: " + (", ".join(f"{clock(rec['t'])} {trace.name(rec.get('d'))} {rec.get('a', 0):,}"
                                          f"{' (lethal)' if rec.get('ok', 0) > 0 else ''}" for rec in row["swings"])
                                or "none"))
    tanked = row["tanked"]
    print(f"  a pair tank had him {(tanked[0] - row['t']) / 1000:.1f} s after the intro ended ({trace.name(tanked[1])})"
          if tanked else "  no pair tank had him after the intro")
    print(f"  deaths in the first {OPENING_MS // 1000} s: " + (", ".join(
        f"{clock(rec['t'])} {who(trace, rec['g'])}" for rec in row["deaths"]) or "none"))
    staged = [(share, guid) for guid, share in row["stage"].items()]
    if not any(value == "stage" for spans in holder_spans(trace, "algalon.spot").values() for value, _, _ in spans):
        print("  absent from this trace: algalon.spot=stage")
    elif staged:
        print(f"  melee bots on algalon.spot=stage before the intro ended:"
              f" {sum(share for share, _ in staged) * 100.0 / len(staged):.0f}% of their intro time ("
              + ", ".join(f"{trace.name(guid)} {share * 100.0:.0f}%" for share, guid in sorted(staged, reverse=True))
              + ")")


# ---------------------------------------------------------------------------------------------- loose

def victim_runs(trace: Trace, pair) -> list[list]:
    """`[start, stop, kind, victims]` per run of his victim after the intro, kind `pair`, `loose` or
    `none`. A short `none` beside a loose run joins it: he drops one victim before he picks the next."""
    begin = intro_end(trace)
    if begin is None:
        return []
    rows = [(when, victim) for when, victim in victim_samples(trace) if when >= begin]
    runs: list[list] = []
    for index, (when, victim) in enumerate(rows):
        stop = rows[index + 1][0] if index + 1 < len(rows) else when
        kind = "none" if not victim else "pair" if victim in pair else "loose"
        if runs and runs[-1][2] == kind:
            runs[-1][1] = stop
        else:
            runs.append([when, stop, kind, []])
        if victim and victim not in runs[-1][3]:
            runs[-1][3].append(victim)
    for index in range(1, len(runs) - 1):
        start, stop, kind, _ = runs[index]
        if kind == "none" and stop - start <= BRIDGE_MS and "loose" in (runs[index - 1][2], runs[index + 1][2]):
            runs[index][2] = "loose"
    return runs


def loose_cause(trace: Trace, pair, start: int, holder: int, first: int) -> tuple[str, str]:
    """Why he left the pair, read at the window's start."""
    intro = intro_end(trace)
    if intro is not None and start - intro <= INTRO_CAUSE_MS:
        return "intro end", ""
    low, high = start - CAUSE_MS, start + SAMPLE_LAG_MS
    punched = next((guid for when, guid in punched_out(trace) if guid in pair and low <= when <= high), 0)
    if punched:
        return "5th stack", trace.name(punched)
    if holder and died_during(trace, holder, low, high):
        return "holder died", trace.name(holder)
    if holder and phased_during(hole_phases(trace), holder, start, high):
        return "holder phased", trace.name(holder)
    if first and taunt_near(trace, first, start) is not None:
        return "other", f"{trace.name(first)} taunted"
    if holder:
        return "threat", f"off {trace.name(holder)}"
    return "other", ""


def loose_row(trace: Trace, pair, start: int, stop: int, victims: list[int], hits: list[dict]) -> dict:
    samples = victim_samples(trace)
    holder = next((victim for when, victim in reversed(samples) if when < start and victim in pair), 0)
    ender = next((victim for when, victim in samples if when >= stop), 0)
    ender = ender if ender in pair else 0
    cause, detail = loose_cause(trace, pair, start, holder, victims[0] if victims else 0)
    return {
        "start": start, "stop": stop, "victims": victims, "holder": holder, "cause": cause, "detail": detail,
        "hits": hits,
        "deaths": [rec for rec in combat_deaths(trace) if start <= rec["t"] <= stop + SAMPLE_LAG_MS],
        "ender": ender,
        "taunt": taunt_near(trace, ender, stop) if ender else None,
    }


def loose_windows(trace: Trace) -> list[dict]:
    """Every stretch after the intro he spent on someone outside the swap pair. One shorter than
    LOOSE_MIN_MS counts only if he landed a swing in it, since the target column flickers."""
    def build(trace: Trace) -> list[dict]:
        pair, _ = swap_pair(trace)
        merged: list[list] = []
        for start, stop, kind, victims in victim_runs(trace, pair):
            if kind != "loose":
                continue
            if merged and merged[-1][1] == start:
                merged[-1][1] = stop
                merged[-1][2] += [victim for victim in victims if victim not in merged[-1][2]]
            else:
                merged.append([start, stop, list(victims)])
        out = []
        for start, stop, victims in merged:
            hits = [rec for rec in swings(trace) if start <= rec["t"] <= stop and rec.get("d") not in pair]
            if stop - start >= LOOSE_MIN_MS or hits:
                out.append(loose_row(trace, pair, start, stop, victims, hits))
        return out
    return cached(trace, "_algalon_loose", build)


def window_cause(window: dict) -> str:
    return window["cause"] + (f", {window['detail']}" if window["detail"] else "")


def show_loose(trace: Trace) -> None:
    print("ALGALON LOOSE")
    if boss_guid(trace) is None or intro_end(trace) is None:
        print("  the intro never ended, or Algalon was never sampled")
        return
    pair, source = swap_pair(trace)
    print(f"  swap pair from {source}: {', '.join(who(trace, guid) for guid in sorted(pair, key=trace.name))}")
    windows = loose_windows(trace)
    boss = boss_guid(trace)
    for window in windows:
        span = window["stop"] - window["start"]
        print(f"  {clock(window['start'])} .. {clock(window['stop'])}  {span / 1000:5.1f} s  {window_cause(window)}")
        dealt: dict[int, list[int]] = collections.defaultdict(list)
        for rec in window["hits"]:
            dealt[rec.get("d", 0)].append(rec.get("a", 0))
        print(f"      on {', '.join(trace.name(guid) for guid in window['victims']) or '-'};"
              f" hit {', '.join(f'{trace.name(g)} {sum(a):,}' for g, a in dealt.items()) or 'nobody'}")
        if window["deaths"]:
            print("      died: " + ", ".join(trace.name(rec["g"]) + ("" if rec.get("killer") == boss else " (not his)")
                                         for rec in window["deaths"]))
        ended = (f"{trace.name(window['ender'])} took him, {'taunt' if window['taunt'] is not None else 'no taunt'}"
                 if window["ender"] else "no pair tank took him")
        print(f"      ended: {ended}")
    total = sum(window["stop"] - window["start"] for window in windows)
    deaths = [rec for window in windows for rec in window["deaths"]]
    print(f"  {len(windows)} window(s), {total / 1000:.1f} s loose, {len(deaths)} death(s) while loose,"
          f" {sum(1 for rec in deaths if rec.get('killer') == boss)} of them his")


# ---------------------------------------------------------------------------------------------- swaps

def swap_due(trace: Trace, pair, guid: int, when: int) -> dict:
    """`taken` is when the other pair tank became his victim; `missed` names what came first instead."""
    taken = next(((at, victim) for at, victim in victim_samples(trace)
                  if at >= when and victim in pair and victim != guid), None)
    ends = [(at, "5th stack") for at, holder in punched_out(trace) if holder == guid and at >= when]
    ends += [(rec["t"], "died") for rec in death_records(trace) if rec.get("g") == guid and rec["t"] >= when]
    first = min(ends + [(pull_end(trace), "pull ended")])
    if taken and taken[0] <= first[0]:
        return {"t": when, "tank": guid, "taken": taken[0], "by": taken[1], "missed": None}
    return {"t": when, "tank": guid, "taken": None, "by": 0, "missed": first}


def swap_dues(trace: Trace) -> list[dict]:
    """Each time a pair tank's Phase Punch reached SWAP_STACKS."""
    pair, _ = swap_pair(trace)
    out = []
    for guid, rows in phase_punch(trace).items():
        if guid not in pair:
            continue
        before = 0
        for when, count, removed in rows:
            count = 0 if removed else count
            if count >= SWAP_STACKS > before:
                out.append(swap_due(trace, pair, guid, when))
            before = count
    return sorted(out, key=lambda due: due["t"])


def threat_steals(trace: Trace) -> list[tuple[int, int, int]]:
    """`(t, from, to)` each time he went straight from one pair tank to the other with no taunt from
    the new one while the old one was still up and unphased."""
    pair, _ = swap_pair(trace)
    phased = all_phases(trace)
    out: list[tuple[int, int, int]] = []
    held = 0
    for when, victim in victim_samples(trace):
        if not victim:
            continue
        if victim not in pair:
            held = 0
            continue
        if (held and victim != held and taunt_near(trace, victim, when) is None
                and not died_during(trace, held, when - CAUSE_MS, when + SAMPLE_LAG_MS)
                and not phased_during(phased, held, when - CAUSE_MS, when)):
            out.append((when, held, victim))
        held = victim
    return out


def spot_distance(trace: Trace, guid: int, value: str, point, low: int = -(1 << 62),
                  high: int = 1 << 62) -> float | None:
    """Median distance to `point` while the bot's `algalon.spot` read `value`, inside `[low, high)`."""
    if point is None:
        return None
    samples = samples_of(trace)
    spans = [(max(start, low), min(stop, high))
             for held, start, stop in holder_spans(trace, "algalon.spot", pull_end(trace)).get(guid, [])
             if held == value and start < high and stop > low]
    gaps = [dist2(row[1:3], point) for start, stop in spans for row in samples.between(guid, start, stop - 1)
            if alive(row)]
    return statistics.median(gaps) if gaps else None


def show_swaps(trace: Trace) -> None:
    print("TANK SWAPS")
    if boss_guid(trace) is None:
        print("  Algalon was never sampled")
        return
    show_absent(trace, "algalon.swappair", "algalon.tankrole", "algalon.call")
    pair, source = swap_pair(trace)
    end = pull_end(trace)
    stacks = phase_punch(trace)
    print(f"  pair from {source}:")
    for guid in sorted(pair, key=trace.name):
        steps = [f"{clock(when)} {'off' if removed else count}" for when, count, removed in stacks.get(guid, [])]
        cast = sum(1 for _, caster in taunts(trace) if caster == guid)
        print(f"    {who(trace, guid)}{' human' if guid in trace.humans else ''}, {cast} taunt(s) at him;"
              f" Phase Punch {', '.join(steps[:TIMELINE_SHOWN]) or 'never'}")
    for due in swap_dues(trace):
        line = f"  swap due {clock(due['t'])}: {trace.name(due['tank'])} at {SWAP_STACKS} stacks, "
        if due["taken"] is not None:
            line += f"{trace.name(due['by'])} had him {(due['taken'] - due['t']) / 1000:.1f} s later"
        else:
            line += f"missed: {due['missed'][1]} at {clock(due['missed'][0])}"
        print(line)
    for when, guid in punched_out(trace):
        if guid in pair:
            print(f"  missed swap: {trace.name(guid)} phased out by the 5th stack at {clock(when)}")
    calls = notes(trace, "algalon.call")
    if calls:
        print("  algalon.call: " + ", ".join(f"{clock(rec['t'])} {rec.get('txt')} ({trace.name(rec.get('g'))})"
                                            for rec in calls))
    roles = holder_spans(trace, "algalon.tankrole", end)
    for guid, spans in sorted(roles.items(), key=lambda item: trace.name(item[0])):
        totals = span_totals({guid: spans})
        print(f"  algalon.tankrole {trace.name(guid)}: "
              + ", ".join(f"{value} {held / 1000:.1f} s" for value, held in totals.most_common())
              + "; " + ", ".join(f"{clock(start)} {value}" for value, start, _ in spans[:TIMELINE_SHOWN]))
    begin = intro_end(trace)
    split = begin if begin is not None else 0
    for guid in sorted(pair, key=trace.name):
        for value, point, low, high, when in (("pickup", PICKUP_SLOT, -(1 << 62), split, " in the intro"),
                                              ("offtank", PULL_OFFTANK_SLOT, -(1 << 62), split, " in the intro"),
                                              ("tank", TANK_SLOT, split, 1 << 62, ""),
                                              ("offtank", OFFTANK_SLOT, split, 1 << 62, "")):
            gap = spot_distance(trace, guid, value, point, low, high)
            if gap is not None:
                print(f"  {trace.name(guid)} on algalon.spot={value}{when}: {gap:.1f} yd median from its slot")
    for rec in notes(trace, "algalon.handover"):
        print(f"  algalon.handover {clock(rec['t'])}: {rec.get('txt')} ({trace.name(rec.get('g'))})")
    steals = threat_steals(trace)
    for when, old, new in steals:
        print(f"  threat steal {clock(when)}: {trace.name(new)} pulled him off {trace.name(old)} with no taunt")
    if not steals:
        print("  no threat steal between the pair")


# ----------------------------------------------------------------------------------------------- hold

def edge_hold(trace: Trace) -> bool:
    """Whether the pull ran the edge hold, which its pickup tank says with `algalon.spot=pickup`."""
    return cached(trace, "_algalon_edge", lambda trace: any(
        value == "pickup" for spans in holder_spans(trace, "algalon.spot").values() for value, _, _ in spans))


def holder_runs(trace: Trace) -> list[list]:
    """`[start, stop, victim]` per stretch after the intro on one victim. A blank target column up to
    BRIDGE_MS long stays inside the stretch around it."""
    def build(trace: Trace) -> list[list]:
        begin = intro_end(trace)
        if begin is None:
            return []
        runs: list[list] = []
        for when, victim in victim_samples(trace):
            if when < begin or not victim:
                continue
            if runs and runs[-1][2] == victim and when - runs[-1][1] <= BRIDGE_MS:
                runs[-1][1] = when
            else:
                runs.append([when, when, victim])
        end = pull_end(trace)
        for index, run in enumerate(runs):
            following = runs[index + 1][0] if index + 1 < len(runs) else end
            run[1] = min(following, run[1] + BRIDGE_MS)
        return runs
    return cached(trace, "_algalon_runs", build)


def hold_pickups(trace: Trace) -> list[dict]:
    """Each time a pair tank took him from someone else, the intro included: when it reached the tank slot, where
    he came to rest, who outside the pair stood in his reach on the way, and the holder's gap DRAG_DUE_MS in."""
    def build(trace: Trace) -> list[dict]:
        boss = boss_guid(trace)
        if boss is None:
            return []
        pair, _ = swap_pair(trace)
        samples = samples_of(trace)
        phased = all_phases(trace)
        outsiders = [guid for guid in roster_guids(trace) if guid not in pair]
        runs = holder_runs(trace)
        out = []
        for index, (start, stop, holder) in enumerate(runs):
            before = runs[index - 1][2] if index else 0
            if holder not in pair or before == holder:
                continue
            arrival = next((when for when, row in samples.timed(holder, start, stop - 1)
                            if alive(row) and dist2(row[1:3], TANK_SLOT) <= ON_SLOT_YD), None)
            until = arrival if arrival is not None else stop
            rest = next((row for _, row in samples.timed(boss, until, stop - 1) if len(row) > 8 and not row[8]),
                        None) or samples.row(boss, until)
            near: dict[int, tuple[float, int]] = {}
            for _, when, _ in samples.steps(start, until):
                him = samples.row(boss, when)
                if not alive(him):
                    continue
                for guid in outsiders:
                    row = samples.row(guid, when)
                    if not alive(row) or phased_at(phased, guid, when):
                        continue
                    gap = dist2(row[1:3], him[1:3])
                    if gap <= MELEE_REACH and gap < near.get(guid, (math.inf, 0))[0]:
                        near[guid] = (gap, when)
            due = samples.row(holder, start + DRAG_DUE_MS)
            out.append({
                "start": start, "stop": stop, "holder": holder, "before": before, "arrival": arrival,
                "rest": tuple(rest[1:3]) if rest else None,
                "near": near,
                "due_gap": dist2(due[1:3], TANK_SLOT) if alive(due) else None,
            })
        return out
    return cached(trace, "_algalon_pickups", build)


def bot_hold(trace: Trace) -> tuple[int, int]:
    """`(held, on)`: ms after the intro a bot of the swap pair was his victim, and of those the ms it
    stood within ON_SLOT_YD of the tank slot."""
    boss, begin = boss_guid(trace), intro_end(trace)
    if boss is None or begin is None:
        return 0, 0
    samples = samples_of(trace)
    pair, _ = swap_pair(trace)
    held = on = 0
    for _, when, weight in samples.steps(begin, pull_end(trace)):
        him = samples.row(boss, when)
        victim = target_of(him) if alive(him) else 0
        if victim not in pair or victim in trace.humans:
            continue
        held += weight
        row = samples.row(victim, when)
        if alive(row) and dist2(row[1:3], TANK_SLOT) <= ON_SLOT_YD:
            on += weight
    return held, on


def hold_timeline(trace: Trace) -> list[dict]:
    """Every HOLD_STEP_MS from the intro's end: his distance from the hold spot and the room centre,
    his victim, and that victim's distance from the tank slot."""
    boss, begin = boss_guid(trace), intro_end(trace)
    if boss is None or begin is None:
        return []
    samples = samples_of(trace)
    out = []
    for when in range(begin, pull_end(trace), HOLD_STEP_MS):
        him = samples.row(boss, when)
        if not alive(him):
            continue
        victim = target_of(him)
        row = samples.row(victim, when) if victim else None
        out.append({"t": when, "to_hold": dist2(him[1:3], HOLD_SPOT), "to_centre": dist2(him[1:3], ROOM_CENTER),
                    "victim": victim, "to_slot": dist2(row[1:3], TANK_SLOT) if alive(row) else None})
    return out


def victim_tag(trace: Trace, pair, guid: int) -> str:
    if not guid:
        return "-"
    if guid in pair:
        return f"{trace.name(guid)} (pair{', human' if guid in trace.humans else ''})"
    return f"{trace.name(guid)} (loose)"


def show_hold(trace: Trace) -> None:
    print("EDGE HOLD")
    if boss_guid(trace) is None or intro_end(trace) is None:
        print("  the intro never ended, or Algalon was never sampled")
        return
    print(f"  tank slot T ({TANK_SLOT[0]:.1f}, {TANK_SLOT[1]:.1f}), hold spot A ({HOLD_SPOT[0]:.1f},"
          f" {HOLD_SPOT[1]:.1f}), room centre C; on T within {ON_SLOT_YD:.0f} yd")
    if not edge_hold(trace):
        print("  absent from this trace: algalon.spot=pickup; it predates the edge hold, so T and A are spots"
              " its bots never used")
    pair, _ = swap_pair(trace)
    rows = hold_timeline(trace)
    if rows:
        print(f"  {'at':>9} {'from A':>7} {'from C':>7}  {'victim':32} {'to T':>6}")
    for row in rows:
        to_slot = "-" if row["to_slot"] is None else f"{row['to_slot']:.1f}"
        print(f"  {clock(row['t']):>9} {row['to_hold']:7.1f} {row['to_centre']:7.1f}"
              f"  {victim_tag(trace, pair, row['victim']):32} {to_slot:>6}")

    for pickup in hold_pickups(trace):
        source = trace.name(pickup["before"]) if pickup["before"] else "the intro"
        if pickup["arrival"] is not None:
            drag = f"on T after {(pickup['arrival'] - pickup['start']) / 1000:.1f} s"
        else:
            drag = f"never on T before {clock(pickup['stop'])}"
        rest = (f"he came to rest {dist2(pickup['rest'], ROOM_CENTER):.1f} yd from C,"
                f" {dist2(pickup['rest'], HOLD_SPOT):.1f} yd from A" if pickup["rest"] else "he was not sampled")
        print(f"  pickup {clock(pickup['start'])}: {label_of(trace, pickup['holder'])} from {source}, {drag}; {rest}")
        near = sorted((gap, when, guid) for guid, (gap, when) in pickup["near"].items())
        print("      in his reach on the way: " + (", ".join(f"{trace.name(guid)} {gap:.1f} yd @ {clock(when)}"
                                                         for gap, when, guid in near) or "nobody"))
    held, on = bot_hold(trace)
    if held:
        print(f"  a bot of the pair held him {held / 1000:.1f} s, {on * 100.0 / held:.0f}% of it on T")
    else:
        print("  no bot of the pair held him after the intro")


def hold_faults(trace: Trace) -> list[tuple[int, str]]:
    """A bot that picked him up and was still off the tank slot DRAG_DUE_MS later, while still holding
    him. Only on a trace that ran the edge hold."""
    if not edge_hold(trace):
        return []
    out = []
    for pickup in hold_pickups(trace):
        holder = pickup["holder"]
        late = pickup["arrival"] is None or pickup["arrival"] - pickup["start"] > DRAG_DUE_MS
        if holder in trace.humans or not late or pickup["stop"] - pickup["start"] < DRAG_DUE_MS:
            continue
        off = f", {pickup['due_gap']:.1f} yd off" if pickup["due_gap"] is not None else ""
        out.append((pickup["start"], f"{trace.name(holder)} not on the tank slot {DRAG_DUE_MS // 1000} s after picking"
                                     f" him up @ {secs(pickup['start'])}{off}"))
    return out


def holder_target_runs(trace: Trace) -> list[dict]:
    """`{start, stop, tank, target}` per stretch his swap tank victim targeted something else, `target` the one held
    longest (0 for none). A stretch ends at the tank's death when that beats the next sample."""
    def build(trace: Trace) -> list[dict]:
        boss = boss_guid(trace)
        if boss is None:
            return []
        samples = samples_of(trace)
        pair, _ = swap_pair(trace)
        runs: list[dict] = []
        current = None
        for when, him, weight in samples.unit_steps(boss):
            victim = target_of(him) if alive(him) else 0
            row = (samples.frame(when).get(victim) or samples.row(victim, when)) if victim in pair else None
            if not alive(row) or target_of(row) == boss:
                current = None
                continue
            if current is None or current["tank"] != victim:
                current = {"start": when, "stop": when, "tank": victim, "held": collections.Counter()}
                runs.append(current)
            current["stop"] = when + weight
            current["held"][target_of(row)] += weight
        deaths = death_records(trace)
        return [{"start": run["start"], "tank": run["tank"], "target": run["held"].most_common(1)[0][0],
                 "stop": min((rec["t"] for rec in deaths
                              if rec.get("g") == run["tank"] and run["start"] <= rec["t"] < run["stop"]),
                             default=run["stop"])}
                for run in runs]
    return cached(trace, "_algalon_holder_targets", build)


def holder_target_faults(trace: Trace) -> list[tuple[int, str]]:
    out = []
    for run in holder_target_runs(trace):
        span = run["stop"] - run["start"]
        if span < HOLDER_TARGET_MS:
            continue
        aim = f"targeted {trace.name(run['target'])}" if run["target"] else "had no target"
        out.append((run["start"], f"holder {trace.name(run['tank'])} {aim} for {span / 1000:.1f} s"
                                  f" @ {secs(run['start'])}"))
    return out


# --------------------------------------------------------------------------------------------- taunts

def taunt_casts(trace: Trace) -> list[dict]:
    """Every taunt cast at him, `{t, caster, name, before, took, outcome}`: `already` when he was on the caster,
    `took` when the caster became his victim within TAUNT_TAKE_MS, else `missed`."""
    def build(trace: Trace) -> list[dict]:
        boss = boss_guid(trace)
        if boss is None:
            return []
        samples = samples_of(trace)
        roster = roster_guids(trace)
        victims = victim_samples(trace)
        stamps = [when for when, _ in victims]
        out: list[dict] = []
        last: dict[tuple, int] = {}
        for rec in trace.of("cast"):
            name = taunt_spell(trace, rec)
            who_cast = rec.get("s")
            if not name or trace.owners.get(who_cast, who_cast) not in roster:
                continue
            when = rec["t"]
            index = bisect.bisect_left(stamps, when)
            before = victims[index - 1][1] if index else 0
            if name == "Righteous Defense":
                aimed = rec.get("tgt") == boss or (rec.get("tgt") in roster and rec.get("tgt") == before)
            elif name in AREA_TAUNTS:
                spot, him = position_at(samples, who_cast, when), position_at(samples, boss, when)
                aimed = bool(spot and him) and dist2(spot, him) <= AREA_TAUNT_YD
            else:
                aimed = rec.get("tgt") == boss
            key = (who_cast, name)
            if not aimed or when - last.get(key, -(1 << 62)) <= ECHO_MS:
                continue
            last[key] = when
            took = next((at for at, victim in victims[index:] if at <= when + TAUNT_LATE_MS and victim == who_cast),
                        None)
            if before == who_cast:
                outcome = "already"
            elif took is not None and took - when <= TAUNT_TAKE_MS:
                outcome = "took"
            else:
                outcome = "missed"
            out.append({"t": when, "caster": who_cast, "name": name, "before": before, "took": took,
                        "outcome": outcome})
        return out
    return cached(trace, "_algalon_taunt_casts", build)


def show_taunts(trace: Trace) -> None:
    print("TAUNTS AT HIM")
    if boss_guid(trace) is None:
        print("  Algalon was never sampled")
        return
    pair, source = swap_pair(trace)
    rows = taunt_casts(trace)
    if not rows:
        print("  nobody taunted him")
    for row in rows:
        guid = row["caster"]
        lag = f"+{(row['took'] - row['t']) / 1000:.1f} s" if row["took"] is not None else ""
        if guid in pair and row["outcome"] == "already":
            verdict = "swap pair, already his"
        elif guid in pair and row["outcome"] == "took":
            verdict = f"swap pair, took {lag}"
        elif guid in pair:
            verdict = "swap pair, MISSED" + (f", his at {lag}" if lag else "")
        elif trace.role(guid) == "tank":
            verdict = "tank outside the pair"
        else:
            verdict = "NON-TANK"
        print(f"  {clock(row['t']):>9}  {label_of(trace, guid):34} {row['name']:18}  on {trace.name(row['before'])};"
              f" {verdict}")
    print(f"  swap pair from {source}; a taunt took when his victim became the caster within"
          f" {TAUNT_TAKE_MS / 1000:.1f} s")
    show_vetoes(trace, "algalon taunt guard")


def taunt_faults(trace: Trace) -> list[tuple[int, str]]:
    """A non-tank taunting him, one line per caster and spell; a pair tank's taunt that missed. A pet
    counts only when one of its taunts took."""
    pair, _ = swap_pair(trace)
    out = []
    loose: dict[tuple, list[dict]] = collections.defaultdict(list)
    for row in taunt_casts(trace):
        if trace.role(row["caster"]) != "tank":
            loose[(row["caster"], row["name"])].append(row)
        elif row["caster"] in pair and row["outcome"] == "missed":
            late = f", his at +{(row['took'] - row['t']) / 1000:.1f} s" if row["took"] is not None else ""
            out.append((row["t"], f"pickup taunt missed @ {secs(row['t'])}: {trace.name(row['caster'])} {row['name']}"
                                  f" while he was on {trace.name(row['before'])}{late}"))
    for (guid, name), rows in loose.items():
        first = rows[0]["t"]
        line = f"non-tank taunt at Algalon: {label_of(trace, guid)} {name} {len(rows)}x from {secs(first)}"
        if guid in trace.owners:
            # pets autocast Growl all fight, only one that took did any harm
            took = sum(1 for row in rows if row["outcome"] == "took")
            if not took:
                continue
            line += f", {took} took"
        out.append((first, line))
    return out


def redirect_faults(trace: Trace) -> list[tuple[int, str]]:
    """Tricks of the Trade or Misdirection cast in the first OPENER_MS of phase 1 on anyone outside the
    swap pair, one line per press."""
    begin = phase_start(trace, "P1")
    if begin is None:
        return []
    pair, _ = swap_pair(trace)
    out = []
    last: dict[tuple, int] = {}
    for rec in trace.of("cast"):
        spell, when = rec.get("sp"), rec["t"]
        if spell not in REDIRECT_IDS or rec.get("tr") or not begin <= when <= begin + OPENER_MS:
            continue
        key = (rec.get("s"), spell)
        if when - last.get(key, -(1 << 62)) <= ECHO_MS:
            continue
        last[key] = when
        target = rec.get("tgt") or 0
        if target not in pair:
            on = who(trace, target) if target else "nobody"
            out.append((when, f"{spell_name(trace, spell, REDIRECT_IDS)} on {on}, not a swap tank, @ {secs(when)}"
                              f" ({trace.name(rec.get('s'))})"))
    return out


def vetoes(trace: Trace, multiplier: str) -> collections.Counter:
    """`(bot, action)` counts of the veto rows a multiplier wrote."""
    return collections.Counter((rec.get("g", 0), rec.get("a") or "?") for rec in trace.of("veto")
                               if rec.get("m") == multiplier)


def show_vetoes(trace: Trace, multiplier: str) -> None:
    counted = vetoes(trace, multiplier)
    if not counted:
        print(f"  no {multiplier} veto row in this trace")
        return
    print(f"  {multiplier} vetoes: " + ", ".join(f"{trace.name(guid)} {action} {n}"
                                                  for (guid, action), n in counted.most_common()))


# ---------------------------------------------------------------------------------------------- reach

def reach_rows(trace: Trace) -> dict[int, list[int]]:
    """Per raid member outside the pair, `[held, loose]` ms alive and unphased within MELEE_REACH of
    him after the intro, split by whether he was inside a loose window."""
    samples = samples_of(trace)
    boss = boss_guid(trace)
    begin = intro_end(trace)
    if boss is None or begin is None:
        return {}
    pair, _ = swap_pair(trace)
    phased = all_phases(trace)
    windows = [(window["start"], window["stop"]) for window in loose_windows(trace)]
    members = [guid for guid in roster_guids(trace) if guid not in pair]
    out: dict[int, list[int]] = collections.defaultdict(lambda: [0, 0])
    for _, when, weight in samples.steps(begin, pull_end(trace)):
        him = samples.row(boss, when)
        if not alive(him):
            continue
        loose = any(start <= when < stop for start, stop in windows)
        for guid in members:
            row = samples.row(guid, when)
            if alive(row) and not phased_at(phased, guid, when) and dist2(row[1:3], him[1:3]) <= MELEE_REACH:
                out[guid][1 if loose else 0] += weight
    return dict(out)


def show_reach(trace: Trace) -> None:
    print("IN HIS REACH")
    if boss_guid(trace) is None or intro_end(trace) is None:
        print("  the intro never ended, or Algalon was never sampled")
        return
    rows = reach_rows(trace)
    if not rows:
        print(f"  nobody outside the swap pair stood within {MELEE_REACH:.2f} yd of him after the intro")
        return
    print(f"  alive and unphased within {MELEE_REACH:.2f} yd of him after the intro, the swap pair left out:")
    for guid, (held, loose) in sorted(rows.items(), key=lambda item: -sum(item[1])):
        print(f"    {who(trace, guid):28} {held / 1000:6.1f} s while the pair held him, {loose / 1000:5.1f} s loose")
    print(f"  total {sum(held for held, _ in rows.values()) / 1000:.1f} s held,"
          f" {sum(loose for _, loose in rows.values()) / 1000:.1f} s loose")


# -------------------------------------------------------------------------------------------- bigbang

def big_bang_casts(trace: Trace) -> list[dict]:
    """`{n, start, end, src}` per Big Bang. Cast rows first; without them the `algalon.bigbang` latch,
    then the damage, which cannot see a Big Bang that hit nobody."""
    starts: list[tuple[int, int, str]] = []
    for rec in trace.of("cast"):
        if rec.get("sp") in SPELL_BIG_BANG:
            if starts and rec["t"] - starts[-1][0] < BIG_BANG_CAST_MS:
                continue
            starts.append((rec["t"], rec.get("ct") or BIG_BANG_CAST_MS, "cast"))
    if not starts:
        for value, start, _ in latch_spans(trace, "algalon.bigbang"):
            if as_guid(value) > 0:
                starts.append((start, BIG_BANG_CAST_MS, "algalon.bigbang"))
    if not starts:
        for when in sorted(rec["t"] for rec in trace.of("dmg") if rec.get("sp") in SPELL_BIG_BANG):
            if not starts or when - (starts[-1][0] + BIG_BANG_CAST_MS) > HIT_SLACK_MS:
                starts.append((when - BIG_BANG_CAST_MS, BIG_BANG_CAST_MS, "damage"))
    return [{"n": index + 1, "start": start, "end": start + cast, "src": src}
            for index, (start, cast, src) in enumerate(starts)]


def phased_spans(trace: Trace, spells=PHASE_AURAS, chain_ms: int = CHAIN_MS) -> dict[int, list[list[int]]]:
    """Per player, `[start, stop, applications]` per stretch wearing any of `spells`. A row while held is the
    field's 1 s refresh; only an application within `chain_ms` of a removal adds one."""
    end = pull_end(trace)
    held: dict[int, set[int]] = collections.defaultdict(set)
    out: dict[int, list[list[int]]] = collections.defaultdict(list)
    for rec in trace.of("aura"):
        spell = rec.get("sp")
        if spell not in spells:
            continue
        guid, when = rec.get("d", 0), rec["t"]
        spans, mine = out[guid], held[guid]
        if rec.get("r"):
            if spell in mine:
                mine.discard(spell)
                if not mine and spans:
                    spans[-1][1] = when
            continue
        if not mine:
            if spans and spans[-1][1] is not None and when - spans[-1][1] <= chain_ms:
                spans[-1][1] = None
                spans[-1][2] += 1
            else:
                spans.append([when, None, 1])
        mine.add(spell)
    for spans in out.values():
        for span in spans:
            if span[1] is None:
                span[1] = end
    return out


def phased_at(spans, guid: int, when: int) -> bool:
    return any(start <= when < stop for start, stop, _ in spans.get(guid, ()))


def rephase_loops(trace: Trace) -> list[dict]:
    """Hole phasing that went round again: an application chained onto a removal, or a stretch past the aura's
    own 10 s, which only a bot still standing in the field keeps up."""
    out = []
    for guid, spans in phased_spans(trace, HOLE_PHASE).items():
        for start, stop, applications in spans:
            if applications >= 2 or stop - start > LOOP_MS:
                out.append({"guid": guid, "start": start, "stop": stop, "applications": applications})
    return sorted(out, key=lambda loop: loop["start"])


def healer_return(phased, healers, victims, stop: int, end: int) -> int | None:
    """When the first healer was back beside the soaker: the impact for one that never hid, else the
    end of the phase it wore at the impact. A phase still on when the file ends returned nobody."""
    back = []
    for guid in healers:
        if guid in victims:
            back.append(stop)
            continue
        back.extend(until for start, until, _ in phased.get(guid, ()) if start <= stop < until < end)
    return min(back) if back else None


def nobody_near(trace: Trace, samples: Samples, phased, boss: int | None, low: int, high: int) -> int | None:
    """The first sample in `[low, high)` with nobody alive and unphased within reset range of him."""
    if boss is None:
        return None
    roster = roster_guids(trace)
    for _, when, _ in samples.steps(low, high):
        him = samples.row(boss, when)
        if not him:
            continue
        if not any(alive(row) and not phased_at(phased, guid, when) and dist2(row[1:3], him[1:3]) <= RESET_RANGE
                   for guid, row in ((guid, samples.row(guid, when)) for guid in roster)):
            return when
    return None


def ice_blocks(trace: Trace) -> dict[int, list[tuple[int, int]]]:
    """Per player, `(on, off)` per Ice Block it wore. One still on when the file ends runs to the end."""
    end = pull_end(trace)
    out: dict[int, list[tuple[int, int]]] = collections.defaultdict(list)
    wearing: dict[int, int] = {}
    for rec in trace.of("aura"):
        if spell_name(trace, rec.get("sp"), {SPELL_ICE_BLOCK: "Ice Block"}) != "Ice Block":
            continue
        guid = rec.get("d", 0)
        if rec.get("r"):
            if guid in wearing:
                out[guid].append((wearing.pop(guid), rec["t"]))
        else:
            wearing.setdefault(guid, rec["t"])
    for guid, on in wearing.items():
        out[guid].append((on, end))
    return out


def big_bang_rows(trace: Trace) -> list[dict]:
    return cached(trace, "_algalon_bang_rows", build_big_bang_rows)


def build_big_bang_rows(trace: Trace) -> list[dict]:
    samples = samples_of(trace)
    roster = roster_guids(trace)
    boss = boss_guid(trace)
    end = pull_end(trace)
    holes = hole_guids(trace)
    soakers = latch_spans(trace, "algalon.soaker", end)
    backups = latch_spans(trace, "algalon.backup", end)
    dispersers = latch_spans(trace, "algalon.disperser", end)
    dispersion_era = "algalon.disperser" in emitted(trace)
    hole_count = latch_spans(trace, "algalon.holes", end)
    hide = holder_spans(trace, "algalon.hide", end)
    kiteholes = holder_spans(trace, "algalon.kitehole", end)
    lost = notes(trace, "algalon.holelost")
    kits = notes(trace, "algalon.soakkit")
    dispersions = [rec for rec in trace.of("cast") if rec.get("sp") == SPELL_DISPERSION and not rec.get("tr")]
    pair, _ = swap_pair(trace)
    phased = phased_spans(trace)
    loops = rephase_loops(trace)
    hits = [rec for rec in trace.of("dmg") if rec.get("sp") in SPELL_BIG_BANG]
    deaths = death_records(trace)
    defensives = notes(trace, "algalon.defensive")
    externals = [rec for rec in trace.of("cast") if rec.get("sp") in SOAK_SPELLS]
    matter = guids_of_entry(trace, NPC_DARK_MATTER)
    matter_hits = [rec for rec in trace.of("dmg") if rec.get("s") in matter]
    ascends = [rec["t"] for rec in trace.of("cast") if rec.get("sp") == SPELL_ASCEND]
    finish = trace.of("end")[-1] if trace.of("end") else None
    healers = {guid for guid in roster if trace.role(guid) == "heal"}
    blocks = ice_blocks(trace)
    shelters = notes(trace, "algalon.shelter")

    casts = big_bang_casts(trace)
    out = []
    for index, cast in enumerate(casts):
        start, stop = cast["start"], cast["end"]
        following = casts[index + 1]["start"] if index + 1 < len(casts) else end + 1
        after = min(stop + HIT_SLACK_MS, following)
        tail = stop + PHASE_AURA_MS + CHAIN_MS

        soaker, soaker_from = last_set_in(soakers, start, stop), "algalon.soaker"
        if not soakers:
            held = target_of(samples.row(boss, stop - 1)) if boss else 0
            soaker, soaker_from = (held, "his victim") if trace.role(held) == "tank" else (0, "-")
        backup = last_set_in(backups, start, stop)
        at_impact = {guid: value_at(spans, stop - 1) for guid, spans in hide.items()}
        holding = {guid for guid, value in at_impact.items() if value == "hold"}
        sanctioned = ({soaker, backup} | (set() if dispersion_era else holding)) - {0}

        kit = [parse_soakkit(rec.get("txt")).get("def") for rec in kits
               if rec.get("g") == soaker and start - BANG_LEAD_MS <= rec["t"] <= stop]
        if soaker and (last_set_in(dispersers, start, stop) == soaker or "dispersion" in kit
                       or any(rec.get("s") == soaker and start <= rec["t"] <= stop for rec in dispersions)):
            kind = "dispersion"
        elif soaker:
            kind = "tank" if soaker in pair else trace.role(soaker)
        else:
            kind = "-"

        # Per bot that was on `wait` inside the cast: ms left before the impact when it set off, or
        # None if it was still waiting at the impact.
        waited: dict[int, int | None] = {}
        for guid, spans in hide.items():
            for value, begin, until in spans:
                if value == "wait" and begin < stop and until > start:
                    waited[guid] = None if until >= stop else stop - until

        victims: dict[int, dict] = {}
        for rec in hits:
            if start <= rec["t"] <= after:
                victims.setdefault(rec.get("d"), rec)
        killed = {guid for guid, rec in victims.items()
                  if rec.get("ok", 0) > 0
                  or any(d.get("g") == guid and rec["t"] <= d["t"] <= rec["t"] + CHAIN_MS for d in deaths)}
        impact = min((rec["t"] for rec in victims.values()), default=stop)

        back = healer_return(phased, healers, victims, stop, end)
        until = back if back is not None else stop + PHASE_AURA_MS
        soaker_death = next((d["t"] for d in deaths if soaker and d.get("g") == soaker and start <= d["t"] <= until),
                            None)
        before = samples.row(soaker, start) if soaker else None
        later = samples.row(soaker, stop + 1000) if soaker else None

        ended = None
        if finish and stop <= finish["t"] <= stop + EVADE_WINDOW_MS and finish.get("out") in ("reset", "wipe"):
            ended = (finish["t"], finish["out"])

        taken: collections.Counter = collections.Counter()
        for rec in matter_hits:
            if start <= rec["t"] <= tail:
                taken[rec.get("d")] += rec.get("a", 0)

        present = holes_at(samples, holes, start)
        hole_spots = [found[1:3] for hole in present if (found := samples.row(hole, start))]
        gaps = {}
        for guid in roster:
            found = samples.row(guid, start)
            if hole_spots and alive(found) and not phased_at(phased, guid, start):
                gaps[guid] = min(dist2(found[1:3], spot) for spot in hole_spots)

        watched = sanctioned | {soaker}
        row = {
            **cast,
            "impact": impact,
            "kind": kind,
            "handlers": hold_handlers(trace, hide, kiteholes, shelters, phased, start, impact),
            "lost": [(rec["t"], str(rec.get("txt", ""))) for rec in lost
                     if start <= rec["t"] <= stop + SOAKER_TAIL_MS],
            "holes": present,
            "hole_gaps": gaps,
            "unreachable": sorted(((gap, guid) for guid, gap in gaps.items()
                                   if gap > HOLE_REACH_YD and guid not in sanctioned), reverse=True),
            "iceblock": sorted((on, off, guid) for guid, spans in blocks.items() for on, off in spans
                               if on < stop and off > start),
            "holes_probe": value_at(hole_count, start),
            "soaker": soaker,
            "soaker_from": soaker_from,
            "soakers": latch_chain(trace, soakers, start, impact + SOAKER_TAIL_MS, impact),
            "backups": latch_chain(trace, backups, start, impact + SOAKER_TAIL_MS, impact),
            "elected": bool(soakers),
            "soaker_phased": bool(soaker) and phased_at(phased, soaker, stop),
            "backup": backup,
            "holding": holding,
            "waited": waited,
            "at_impact": at_impact,
            "soaker_hp": (before[5] if before else None, later[5] if later else None),
            "soaker_death": soaker_death,
            "defensives": [(rec["t"], rec.get("g"), str(rec.get("txt", ""))) for rec in defensives
                           if start - DEFENSIVE_LEAD_MS <= rec["t"] <= stop],
            "externals": [(rec["t"], rec.get("s"), rec.get("sp"), rec.get("tgt")) for rec in externals
                          if start - DEFENSIVE_LEAD_MS <= rec["t"] <= stop
                          and (rec.get("s") in watched or rec.get("tgt") in watched)],
            "victims": victims,
            "killed": killed,
            "stray": sorted((guid for guid in victims if guid not in sanctioned), key=trace.name),
            "back": back,
            "nobody": nobody_near(trace, samples, phased, boss, stop, stop + PHASE_AURA_MS),
            "ended": ended,
            "ascend": next((when for when in ascends if stop - 500 <= when <= stop + EVADE_WINDOW_MS), None),
            "loops": [loop for loop in loops if loop["start"] < tail and loop["stop"] > start],
            "matter": taken,
            "hide": collections.Counter(value for value in at_impact.values() if value is not None),
        }
        row["runners"] = runner_holes(trace, row, shelters)
        out.append(row)
    return out


def hold_handlers(trace: Trace, hide, kiteholes, shelters: list[dict], phased, start: int,
                  impact: int) -> list[dict]:
    """Per bot on `algalon.hide=hold` in the cast: first hold, set-off (`run`/`in` after its last hold, None past the
    impact), its hole (`algalon.shelter`, else `algalon.kitehole`), its distance then, whether phased at the impact."""
    samples = samples_of(trace)
    out = []
    for guid, spans in hide.items():
        held = [index for index, (value, begin, until) in enumerate(spans)
                if value == "hold" and begin < impact and until > start]
        if not held:
            continue
        after = spans[held[-1] + 1] if held[-1] + 1 < len(spans) else None
        went = after[1] if after and after[0] in ("run", "in") and after[1] <= impact else None
        named = [hole for rec in shelters if rec.get("g") == guid and start <= rec["t"] <= impact
                 if (hole := as_guid(rec.get("txt")))]
        hole = named[-1] if named else last_set_in(kiteholes.get(guid, []), start, impact)
        found = (samples.row(hole, start) or samples.after(hole, start)) if hole else None
        spot = tuple(found[1:3]) if found else None
        stood = position_at(samples, guid, went if went is not None else impact)
        out.append({"guid": guid, "hold": spans[held[0]][1], "went": went, "hole": hole, "spot": spot,
                    "gap": dist2(spot, stood) if spot and stood else None,
                    "phased": phased_at(phased, guid, impact)})
    return sorted(out, key=lambda handler: handler["hold"])


def runner_holes(trace: Trace, row: dict, shelters: list[dict]) -> list[dict]:
    """Per non-soaker hit while on a hide run: the hole its last `algalon.shelter` inside the cast named
    and the nearest hole present at the cast start, each `(spot, yd)` from where the runner stood then."""
    samples = samples_of(trace)
    start = row["start"]
    spots = {hole: tuple(found[1:3]) for hole in row["holes"] if (found := samples.row(hole, start))}
    out = []
    for guid in row["stray"]:
        if row["at_impact"].get(guid) != "run":
            continue
        named = [hole for rec in shelters if rec.get("g") == guid and start <= rec["t"] <= row["end"]
                 if (hole := as_guid(rec.get("txt")))]
        found = (samples.row(named[-1], start) or samples.after(named[-1], start)) if named else None
        ran = tuple(found[1:3]) if found else None
        stood = samples.row(guid, start)
        here = tuple(stood[1:3]) if stood else None
        nearest = min(spots.values(), key=lambda spot: dist2(spot, here)) if spots and here else None
        out.append({"guid": guid, "hole": named[-1] if named else 0,
                    "ran": (ran, dist2(ran, here) if ran and here else None),
                    "nearest": (nearest, dist2(nearest, here) if nearest else None)})
    return out


def hole_label(spot, gap) -> str:
    if spot is None:
        return "-"
    return f"({spot[0]:.0f},{spot[1]:.0f})" + (f" {gap:.1f} yd" if gap is not None else "")


def nearhole_shares(trace: Trace, row: dict) -> list[tuple[int, float | None, float]]:
    """`(bot, nearest hole yd, share of the lead)` per bot outside the pair on `algalon.spot=nearhole` in the
    NEARHOLE_LEAD_MS before the cast, or over NEARHOLE_YD from every hole at its start."""
    spots = holder_spans(trace, "algalon.spot", pull_end(trace))
    pair, _ = swap_pair(trace)
    low, high = row["start"] - NEARHOLE_LEAD_MS, row["start"]
    out = []
    for guid in roster_guids(trace):
        if guid in trace.humans or guid in pair:
            continue
        held = sum(max(0, min(stop, high) - max(start, low)) for value, start, stop in spots.get(guid, [])
                   if value == "nearhole")
        gap = row["hole_gaps"].get(guid)
        if held or (gap is not None and gap > NEARHOLE_YD):
            out.append((guid, gap, held / NEARHOLE_LEAD_MS))
    return sorted(out, key=lambda entry: (-(entry[1] or 0.0), trace.name(entry[0])))


def show_hole_reach(trace: Trace, row: dict) -> None:
    gaps = sorted(((gap, guid) for guid, gap in row["hole_gaps"].items()), reverse=True)
    if not gaps:
        print("      nearest hole at the cast: no hole, or nobody unphased")
    else:
        print(f"      nearest hole at the cast, unphased: median {statistics.median(gap for gap, _ in gaps):.1f} yd,"
              " farthest " + ", ".join(f"{trace.name(guid)} {gap:.1f}" for gap, guid in gaps[:5]))
    if row["unreachable"]:
        print(f"      beyond {HOLE_REACH_YD:.0f} yd of every hole, soaker and backup aside: "
              + ", ".join(f"{trace.name(guid)} {gap:.1f}" for gap, guid in row["unreachable"]))
    for on, off, guid in row["iceblock"]:
        print(f"      Ice Block during the cast: {trace.name(guid)} {clock(on)} .. {clock(off)}")
    if not any(value == "nearhole" for spans in holder_spans(trace, "algalon.spot").values() for value, _, _ in spans):
        print("      absent from this trace: algalon.spot=nearhole")
        return
    shares = nearhole_shares(trace, row)
    print(f"      on algalon.spot=nearhole in the {NEARHOLE_LEAD_MS // 1000} s before: "
          + (", ".join(f"{trace.name(guid)} {'-' if gap is None else f'{gap:.1f} yd'} {share * 100.0:.0f}%"
                       for guid, gap, share in shares) or "nobody, and no bot over"
             f" {NEARHOLE_YD:.0f} yd from a hole"))


def nitro_uses(trace: Trace) -> dict[int, list[int]]:
    """Per bot, when it used Nitro Boosts: its `algalon.nitro` rows, plus any 54861 applied to it that no row
    stands beside. A trace from before the note still shows the aura."""
    def build(trace: Trace) -> dict[int, list[int]]:
        out: dict[int, list[int]] = collections.defaultdict(list)
        for rec in notes(trace, "algalon.nitro"):
            out[rec.get("g", 0)].append(rec["t"])
        for rec in trace.of("aura"):
            guid = rec.get("d", 0)
            if (rec.get("sp") == SPELL_NITRO_BOOSTS and not rec.get("r") and guid not in trace.humans
                    and not any(abs(rec["t"] - when) <= ECHO_MS for when in out.get(guid, ()))):
                out[guid].append(rec["t"])
        return {guid: sorted(times) for guid, times in out.items()}
    return cached(trace, "_algalon_nitro", build)


def bang_hiders(trace: Trace, row: dict) -> list[int]:
    """Bots on a hide `run` or `in` at any point from the cast start to the impact."""
    hide = holder_spans(trace, "algalon.hide", pull_end(trace))
    return sorted((guid for guid, spans in hide.items()
                   if guid not in trace.humans and any(value in ("run", "in") and begin < row["impact"]
                                                       and until > row["start"] for value, begin, until in spans)),
                  key=trace.name)


def show_nitro(trace: Trace, row: dict) -> None:
    hiders = bang_hiders(trace, row)
    if not hiders:
        return
    uses = nitro_uses(trace)
    low, high = row["start"] - NITRO_LEAD_MS, row["impact"]
    used = {guid: times[0] for guid in hiders
            if (times := [when for when in uses.get(guid, ()) if low <= when <= high])}
    if not uses:
        print(f"      Nitro Boosts: no algalon.nitro row or 54861 aura in this trace, {len(hiders)} hider(s)")
        return
    print(f"      Nitro Boosts: used by "
          + (", ".join(f"{trace.name(guid)} {clock(when)}" for guid, when in sorted(used.items(), key=lambda i: i[1]))
             or "nobody")
          + "; hid without: " + (", ".join(trace.name(guid) for guid in hiders if guid not in used) or "nobody"))


def show_bigbang(trace: Trace) -> None:
    print("BIG BANG")
    rows = big_bang_rows(trace)
    if not rows:
        print("  no Big Bang was cast")
        return
    show_absent(trace, "algalon.bigbang", "algalon.soaker", "algalon.backup", "algalon.hide", "algalon.defensive",
                "algalon.shelter", "algalon.disperser")
    # Older traces carry the block under the Ice Block name it had before Cloak and Divine Shield joined it.
    show_vetoes(trace, next((name for name in ("algalon phase immunity hold", "algalon ice block hold")
                             if vetoes(trace, name)), "algalon phase immunity hold"))

    for row in rows:
        src = "" if row["src"] == "cast" else f"  (from {row['src']})"
        probe = f", algalon.holes {row['holes_probe']}" if row["holes_probe"] is not None else ""
        print(f"  #{row['n']}  {clock(row['start'])} .. {clock(row['end'])}{src}"
              f"  holes at the cast: {len(row['holes'])}{probe}")

        if row["soaker"]:
            first, then = row["soaker_hp"]
            health = f"{first:.0f}% -> {then:.0f}%" if first is not None and then is not None else "health -"
            died = f", died {clock(row['soaker_death'])}" if row["soaker_death"] is not None else ""
            print(f"      soaker {who(trace, row['soaker'])} from {row['soaker_from']}, {row['kind']} soak,"
                  f" {health}{died}")
        else:
            print("      soaker unknown")
        if row["soakers"]:
            print(f"      algalon.soaker during the cast: {chain_label(trace, row['soakers'])}")
        print(f"      backup {who(trace, row['backup']) if row['backup'] else '-'}")
        if row["backups"]:
            print(f"      algalon.backup during the cast: {chain_label(trace, row['backups'])}")
        for handler in row["handlers"]:
            went = (f"went {clock(handler['went'])}, {(row['impact'] - handler['went']) / 1000:.1f} s before the"
                    f" impact" if handler["went"] is not None else "still on hold at the impact")
            print(f"      on hold: {trace.name(handler['guid'])} from {clock(handler['hold'])}, {went}, hole"
                  f" {hole_label(handler['spot'], handler['gap'])}; {'phased' if handler['phased'] else 'unphased'}"
                  f" at the impact")
        if row["lost"]:
            print("      algalon.holelost during the cast: " + ", ".join(f"{clock(when)} {reason}"
                                                                      for when, reason in row["lost"]))

        saves = [f"{clock(when)} {text} ({trace.name(guid)})" for when, guid, text in row["defensives"]]
        saves += [f"{clock(when)} {trace.name(caster)} {trace.spell(spell)} -> {trace.name(target)}"
                  for when, caster, spell, target in row["externals"]]
        print(f"      defensives: {'; '.join(saves) if saves else 'none'}")
        show_hole_reach(trace, row)

        hit = []
        for guid, rec in sorted(row["victims"].items(), key=lambda item: trace.name(item[0])):
            tag = ("soaker" if guid == row["soaker"] else "backup" if guid == row["backup"]
                   else "hold" if guid in row["holding"] else trace.role(guid))
            hit.append(f"{trace.name(guid)} ({tag}) {rec.get('a', 0):,}{' DIED' if guid in row['killed'] else ''}")
        print(f"      hit unphased: {', '.join(hit) if hit else 'nobody'}")
        if row["stray"]:
            print(f"      {len(row['stray'])} non-soaker(s) hit: " + ", ".join(
                f"{trace.name(g)} (hide {row['at_impact'].get(g) or '-'})" for g in row["stray"]))
        for run in row["runners"]:
            print(f"        {trace.name(run['guid'])} ran to {hole_label(*run['ran'])},"
                  f" nearest hole {hole_label(*run['nearest'])}")
        if row["waited"]:
            left = sorted((ms, trace.name(g)) for g, ms in row["waited"].items() if ms is not None)
            still = sorted(trace.name(g) for g, ms in row["waited"].items() if ms is None)
            latest = f", the latest {left[0][1]} {left[0][0] / 1000:.1f} s before the impact" if left else ""
            print(f"      left the wait: {len(left)}{latest}; still waiting at the impact:"
                  f" {', '.join(still) or 'nobody'}")

        show_nitro(trace, row)
        if row["soaker_phased"]:
            alone = "soaker phased at the impact"
        else:
            alone = "tank alone " + (f"{(row['back'] - row['end']) / 1000:.1f} s" if row["back"] is not None else "-")
        nobody = (f"nobody alive and unphased within {RESET_RANGE:.0f} yd from {clock(row['nobody'])}"
                  if row["nobody"] is not None else "someone alive and unphased in range throughout")
        print(f"      {alone}; {nobody}")
        for loop in row["loops"]:
            print(f"      re-phase loop: {trace.name(loop['guid'])} {applications(loop['applications'])},"
                  f" {(loop['stop'] - loop['start']) / 1000:.1f} s phased from {clock(loop['start'])}")
        matter = [(guid, amount) for guid, amount in row["matter"].most_common() if amount]
        if matter:
            print("      Dark Matter on phased players: " + ", ".join(f"{trace.name(g)} {a:,}" for g, a in matter))
        if row["hide"]:
            print("      algalon.hide at the impact: " + ", ".join(f"{v} {n}" for v, n in row["hide"].most_common()))
        after = []
        if row["ascend"] is not None:
            after.append(f"Ascend {(row['ascend'] - row['end']) / 1000:+.1f} s")
        if row["ended"]:
            after.append(f"pull ended {row['ended'][1]} {(row['ended'][0] - row['end']) / 1000:+.1f} s")
        print(f"      after: {', '.join(after) if after else 'the pull went on'}")


def bang_faults(trace: Trace) -> list[tuple[int, str]]:
    """Raid members other than the soaker and backup with every hole out of reach at a cast."""
    return [(row["start"], f"Big Bang {row['n']} @ {secs(row['start'])}: nearest hole beyond {HOLE_REACH_YD:.0f} yd"
                           f" for " + ", ".join(f"{trace.name(guid)} {gap:.1f}" for gap, guid in row["unreachable"]))
            for row in big_bang_rows(trace) if row["unreachable"]]


def immunity_uses(trace: Trace) -> list[tuple[int, int, str]]:
    """`(t, bot, name)` per Ice Block, Divine Shield, Cloak of Shadows or Anti-Magic Shell a bot put up: from
    its cast, or from the aura when no cast row stands beside it."""
    def build(trace: Trace) -> list[tuple[int, int, str]]:
        names = frozenset(IMMUNITIES.values())
        out: list[tuple[int, int, str]] = []
        for rec in sorted(trace.of("cast", "aura"), key=lambda rec: rec["t"]):
            if rec.get("r") or rec.get("tr"):
                continue
            guid = rec.get("s") if rec["e"] == "cast" else rec.get("d")
            name = spell_name(trace, rec.get("sp"), IMMUNITIES)
            if name in names and guid in trace.roles and guid not in trace.humans and not any(
                    seen == guid and label == name and abs(rec["t"] - when) <= ECHO_MS for when, seen, label in out):
                out.append((rec["t"], guid, name))
        return out
    return cached(trace, "_algalon_immunity_uses", build)


def immunity_strips(trace: Trace) -> list[dict]:
    """`{t, guid, name, start, stop}` per immunity put up while the hole's 62168 was on the bot that came off
    within IMMUNITY_STRIP_MS of it. A removal a Big Bang's 64445 strip explains is left out."""
    def build(trace: Trace) -> list[dict]:
        wearing = aura_spans(trace, (SPELL_BLACK_HOLE_PHASE,))
        casts = big_bang_casts(trace)
        out = []
        for when, guid, name in immunity_uses(trace):
            for start, stop, _, _ in wearing.get(guid, ()):
                if (start <= when < stop <= when + IMMUNITY_STRIP_MS
                        and not any(cast["end"] <= stop <= cast["end"] + PHASE_AURA_MS // 5 for cast in casts)):
                    out.append({"t": when, "guid": guid, "name": name, "start": start, "stop": stop})
        return out
    return cached(trace, "_algalon_immunity_strips", build)


def kite_faults(trace: Trace) -> list[tuple[int, str]]:
    out = []
    for row in loose_at_bang(trace):
        out.append((row["t"], f"Big Bang {row['n']} @ {secs(row['t'])}: a constellation loose on"
                              f" {trace.name(row['victim'])}, not the handler {trace.name(row['handler'])}"))
    for row in parked_stretches(trace):
        if not row["eaten"]:
            out.append((row["start"], f"{trace.name(row['guid'])} parked {(row['stop'] - row['start']) / 1000:.1f} s"
                                      f" @ {secs(row['start'])}, {row['gap']:.1f} yd from its kite hole, with no"
                                      f" constellation eaten"))
    for row in immunity_strips(trace):
        out.append((row["t"], f"{trace.name(row['guid'])} cast {row['name']} while phased @ {secs(row['t'])} and lost"
                              f" its phase aura"))
    blocked = kite_blocked(trace)
    if sum(blocked.values()) >= KITE_BLOCKED_MS:
        out.append((0, f"kite blocked {sum(blocked.values()) / 1000:.1f} s: the handler taunted with one held and a"
                       f" hole spare (none loose but one in the air {blocked['stuck'] / 1000:.1f} s, taunt cooling"
                       f" {blocked['cooling'] / 1000:.1f} s)"))
    return out


# ----------------------------------------------------------------------------------------------- soak

def parse_soakkit(text) -> dict[str, str]:
    """`algalon.soakkit` reads `def=<cast name|none> forbearance=<0|1>`; a cast name can hold spaces."""
    return dict(part.split("=", 1) for part in re.split(r"\s+(?=[a-z]+=)", str(text or "").strip()) if "=" in part)


def reserved_casts(trace: Trace) -> list[dict]:
    """`{t, caster, name, tgt}` per press, by anyone, of a button the soak keeps for Big Bang. A triggered
    row is the spell's echo on whoever it landed on, not a press."""
    def build(trace: Trace) -> list[dict]:
        out: list[dict] = []
        last: dict[tuple, int] = {}
        for rec in trace.of("cast"):
            name = spell_name(trace, rec.get("sp"), RESERVED_IDS)
            if name not in RESERVED_NAMES or rec.get("tr"):
                continue
            key = (rec.get("s"), name)
            if rec["t"] - last.get(key, -(1 << 62)) <= ECHO_MS:
                continue
            last[key] = rec["t"]
            out.append({"t": rec["t"], "caster": rec.get("s"), "name": name, "tgt": rec.get("tgt") or 0})
        return out
    return cached(trace, "_algalon_reserved", build)


def soak_window(row: dict) -> tuple[int, int]:
    """The stretch a press counts as part of a Big Bang's soak: the defensive lead before the cast, through
    the rescue after the impact."""
    return row["start"] - DEFENSIVE_LEAD_MS, row["impact"] + SOAK_AFTER_MS


def record_index(trace: Trace, rec: dict) -> int:
    """Where `rec` sits in the time-sorted records, so rows of the same millisecond keep the order they
    were written in."""
    low = bisect.bisect_left([r.get("t", 0) for r in trace.records], rec["t"])
    return next(index for index in range(low, len(trace.records)) if trace.records[index] is rec)


def soak_rows(trace: Trace) -> list[dict]:
    """Per Big Bang: the soaker's hit and the seconds after, the healers' way back, what went on the soaker in the
    cast, its kit, and every reserved button pressed since the last soak."""
    def build(trace: Trace) -> list[dict]:
        samples = samples_of(trace)
        roster = roster_guids(trace)
        healers = sorted((guid for guid in roster if trace.role(guid) == "heal"), key=trace.name)
        phased = all_phases(trace)
        kits = notes(trace, "algalon.soakkit")
        handovers = notes(trace, "algalon.handover")
        acts = [rec for rec in trace.of("act") if rec.get("a") in SOAK_NODES]
        reserved = reserved_casts(trace)
        out = []
        since = -(1 << 62)
        for bang in big_bang_rows(trace):
            start, soaker = bang["start"], bang["soaker"]
            hit = bang["victims"].get(soaker) if soaker else None
            impact = hit["t"] if hit else bang["end"]
            later: list[dict] = []
            heal = None
            if soaker:
                # rows of the impact's own millisecond count when they were written after the hit
                index = record_index(trace, hit) + 1 if hit else bisect.bisect_right(
                    [r.get("t", 0) for r in trace.records], impact)
                for rec in trace.records[index:]:
                    if rec.get("t", 0) > impact + HEAL_WAIT_MS:
                        break
                    if rec.get("d") != soaker:
                        continue
                    if rec.get("e") == "dmg" and rec["t"] <= impact + SOAK_DAMAGE_MS:
                        later.append(rec)
                    elif rec.get("e") == "heal" and heal is None:
                        heal = rec
            first = samples.row(soaker, impact) if soaker else None
            then = samples.after(soaker, impact) if soaker else None
            strip = {}
            for guid in healers:
                span = next((stop for begin, stop, _ in phased.get(guid, ()) if begin <= impact < stop), None)
                strip[guid] = span - impact if span is not None else None
            kit = [rec for rec in kits if start - BANG_LEAD_MS <= rec["t"] <= impact]
            defensive = [text for _, guid, text in bang["defensives"] if guid == soaker]
            prep = []
            seen: dict[tuple, int] = {}
            for rec in trace.of("cast"):
                key = (rec.get("s"), rec.get("sp"))
                if (soaker and rec.get("tgt") == soaker and rec.get("s") in healers and start <= rec["t"] <= impact
                        and not rec.get("tr") and rec["t"] - seen.get(key, -(1 << 62)) > ECHO_MS):
                    seen[key] = rec["t"]
                    prep.append((rec["t"], rec.get("s"), rec.get("sp")))
            row = {
                "n": bang["n"], "start": start, "end": bang["end"], "impact": impact, "soaker": soaker,
                "soakers": bang["soakers"],
                "kind": bang["kind"],
                "stray": [(guid, bang["victims"][guid].get("a", 0), bang["at_impact"].get(guid))
                          for guid in bang["stray"]],
                "hit": hit,
                "hp": (hit.get("hp") if hit else first[5] if alive(first) else None,
                       then[5] if then and len(then) > 5 else None),
                "later": later,
                "heal": heal,
                "strip": strip,
                "prep": prep,
                "kit": parse_soakkit(kit[-1].get("txt")) if kit else None,
                "defensive": defensive[-1] if defensive else None,
                "handover": [rec for rec in handovers if start - HANDOVER_LEAD_MS <= rec["t"] <= impact],
                "acts": collections.Counter((rec.get("a"), rec.get("g", 0), rec.get("vd") or "?") for rec in acts
                                            if start <= rec["t"] <= impact + SOAK_AFTER_MS),
                "died": next((rec["t"] for rec in death_records(trace)
                              if soaker and rec.get("g") == soaker and impact <= rec["t"] <= impact + SOAK_AFTER_MS),
                             None),
            }
            low, high = soak_window(row)
            row["earlier"] = [press for press in reserved if since <= press["t"] < low]
            row["pressed"] = [press for press in reserved if low <= press["t"] <= high]
            since = high
            out.append(row)
        return out
    return cached(trace, "_algalon_soak", build)


def kit_label(row: dict) -> str:
    if row["kit"] is not None:
        return f"def={row['kit'].get('def', '?')}" + (" forbearance" if row["kit"].get("forbearance") == "1" else "")
    if row["defensive"] is not None:
        return f"algalon.defensive {row['defensive']}"
    return "no kit probe"


def heal_label(row: dict) -> str:
    heal = row["heal"]
    if heal is None:
        return "no heal on him"
    return f"first heal +{(heal['t'] - row['impact']) / 1000:.1f} s"


def press_label(trace: Trace, press: dict) -> str:
    target = f" -> {trace.name(press['tgt'])}" if press["tgt"] and press["tgt"] != press["caster"] else ""
    return f"{clock(press['t'])} {trace.name(press['caster'])} {press['name']}{target}"


def show_soak(trace: Trace) -> None:
    print("BIG BANG SOAK")
    rows = soak_rows(trace)
    if not rows:
        print("  no Big Bang was cast")
        return
    show_absent(trace, "algalon.soaker", "algalon.soakkit", "algalon.handover", "algalon.disperser")
    if not any(rec.get("a") in SOAK_NODES for rec in trace.of("act")):
        print(f"  absent from this trace: act rows of {' and '.join(SOAK_NODES)}")
    show_vetoes(trace, "algalon cooldown reserve")
    for row in rows:
        print(f"  #{row['n']}  {clock(row['start'])} .. {clock(row['end'])}")
        if not row["soaker"]:
            print("      soaker unknown")
            continue
        first, then = row["hp"]
        hit = row["hit"]
        health = (f"{first:.0f}% -> {then:.0f}% at the next sample" if first is not None and then is not None
                  else "health -")
        landed = (f"hit {clock(hit['t'])} for {hit.get('a', 0):,}, {hit.get('ab', 0):,} absorbed" if hit
                  else "not hit")
        print(f"      soaker {label_of(trace, row['soaker'])}, {row['kind']} soak: {landed}; {health}")
        if row["soakers"]:
            print(f"      algalon.soaker during the cast: {chain_label(trace, row['soakers'])}")
        if row["stray"]:
            print("      hit unphased besides the soaker and backup: " + ", ".join(
                f"{trace.name(guid)} {amount:,} (hide {hide or '-'})" for guid, amount, hide in row["stray"]))
        taken: collections.Counter = collections.Counter()
        for rec in row["later"]:
            taken[trace.spell(rec.get("sp"))] += rec.get("a", 0)
        print(f"      taken in the {SOAK_DAMAGE_MS // 1000} s after: {sum(taken.values()):,}"
              + (" (" + ", ".join(f"{name} {amount:,}" for name, amount in taken.most_common()) + ")" if taken else ""))
        strips = [(back, guid) for guid, back in row["strip"].items() if back is not None]
        print("      healers unphased: " + (", ".join(f"{trace.name(guid)} +{back / 1000:.2f} s"
                                                      for back, guid in sorted(strips)) or "none phased at the hit"))
        heal = row["heal"]
        print(f"      first heal on him: " + (f"{trace.name(heal.get('s'))} {trace.spell(heal.get('sp'))}"
                                             f" {heal.get('a', 0):,} at +{(heal['t'] - row['impact']) / 1000:.2f} s"
                                             if heal else "none"))
        print("      put on him during the cast: " + (", ".join(
            f"{clock(when)} {trace.name(guid)} {trace.spell(spell)}" for when, guid, spell in row["prep"])
            or "nothing"))
        print(f"      kit: {kit_label(row)}")
        for rec in row["handover"]:
            print(f"      algalon.handover {rec.get('txt')} ({trace.name(rec.get('g'))}) @ {clock(rec['t'])}")
        for (node, guid, verdict), n in sorted(row["acts"].items(),
                                               key=lambda item: (item[0][0], trace.name(item[0][1]))):
            print(f"      {node}: {trace.name(guid)} {verdict} {n}")
        print("      reserved buttons pressed before the cast: " + (
            "; ".join(press_label(trace, press) for press in row["earlier"]) or "none"))
        print("      reserved buttons pressed for this soak: " + (
            "; ".join(press_label(trace, press) for press in row["pressed"]) or "none"))
        died = row["died"]
        print("      " + (f"soaker DIED +{(died - row['impact']) / 1000:.1f} s after the hit" if died is not None
                          else f"soaker alive {SOAK_AFTER_MS // 1000} s after the hit"))


def reserve_free(trace: Trace, press: dict) -> bool:
    """Whether the reserve let a press outside a soak through: one the tank defensive or explosion cover
    node noted, or, with a disperser latched, a damage cutter or Lay on Hands on a swap tank."""
    picks = cached(trace, "_algalon_node_picks", lambda trace: [
        rec for key in ("algalon.tankdef", "algalon.cover") for rec in notes(trace, key)])
    if any(rec.get("g") == press["caster"] and abs(rec["t"] - press["t"]) <= ECHO_MS for rec in picks):
        return True
    pair, _ = swap_pair(trace)
    target = press["caster"] if press["name"] in TANK_BUTTON_NAMES else press["tgt"] or press["caster"]
    dispersers = cached(trace, "_algalon_dispersers", lambda trace: latch_spans(trace, "algalon.disperser"))
    return bool(as_guid(value_at(dispersers, press["t"]))) and press["name"] in TANK_SAVE_NAMES and target in pair


def soak_faults(trace: Trace) -> list[tuple[int, str]]:
    """The soaker dying soon after the hit, and a pair tank or priest spending a reserved button outside
    every soak with a Big Bang still to come, unless the reserve let it through."""
    out = []
    rows = soak_rows(trace)
    for row in rows:
        if row["died"] is not None:
            out.append((row["died"], f"Big Bang {row['n']} @ {secs(row['start'])}: soaker {trace.name(row['soaker'])}"
                                     f" died {(row['died'] - row['impact']) / 1000:.1f} s after the hit"
                                     f" ({kit_label(row)}, {heal_label(row)})"))
    pair, _ = swap_pair(trace)
    for row in rows:
        for press in row["earlier"]:
            if reserve_free(trace, press):
                continue
            if press["caster"] in pair or class_of(trace, press["caster"]) in PRIEST_CLASS:
                target = f" on {trace.name(press['tgt'])}" if press["tgt"] and press["tgt"] != press["caster"] else ""
                out.append((press["t"], f"{trace.name(press['caster'])} spent {press['name']}{target}"
                                        f" @ {secs(press['t'])}, outside a Big Bang soak"))
    return out


# -------------------------------------------------------------------------------------------- phasing

def moves_of(trace: Trace) -> dict[int, list[dict]]:
    """Accepted `move` rows by bot, in time order."""
    def build(trace: Trace) -> dict[int, list[dict]]:
        out: dict[int, list[dict]] = collections.defaultdict(list)
        for rec in trace.of("move"):
            if rec.get("ok"):
                out[rec.get("g", 0)].append(rec)
        return out
    return cached(trace, "_algalon_moves", build)


def charges_of(trace: Trace) -> dict[int, list[dict]]:
    """Charge `cast` rows by caster, in time order."""
    def build(trace: Trace) -> dict[int, list[dict]]:
        out: dict[int, list[dict]] = collections.defaultdict(list)
        for rec in trace.of("cast"):
            if rec.get("sp") in SPELL_CHARGES and not rec.get("tr"):
                out[rec.get("s", 0)].append(rec)
        return out
    return cached(trace, "_algalon_charges", build)


def victim_hits(trace: Trace) -> dict[int, list[dict]]:
    """Every `dmg` row by victim, in time order."""
    def build(trace: Trace) -> dict[int, list[dict]]:
        out: dict[int, list[dict]] = collections.defaultdict(list)
        for rec in trace.of("dmg"):
            out[rec.get("d", 0)].append(rec)
        return out
    return cached(trace, "_algalon_victim_hits", build)


def unphased_at(trace: Trace) -> list[tuple[int, int]]:
    """`(t, guid)` per 64445 Big Bang put on a raid member."""
    return cached(trace, "_algalon_unphase", lambda trace: [
        (rec["t"], rec.get("d", 0)) for rec in trace.of("aura")
        if rec.get("sp") == SPELL_BIG_BANG_UNPHASE and not rec.get("r")])


def stray_cause(trace: Trace, guid: int, start: int) -> str:
    """What put a raid member into a hole outside a Big Bang: Big Bang's 64445 stripping it while it
    still stood in the field, a hole spawning on it, or a walk."""
    if any(target == guid and start - REPHASE_MS <= when <= start for when, target in unphased_at(trace)):
        return "re-phase"
    samples = samples_of(trace)
    spot = position_at(samples, guid, start)
    for hole in hole_guids(trace):
        seen = samples.seen(hole)
        if (spot and seen and abs(seen[0] - start) <= NEW_HOLE_MS
                and dist2(samples.row(hole, seen[0])[1:3], spot) <= NEW_HOLE_YD):
            return "new hole"
    walk = next((rec for rec in reversed(moves_of(trace).get(guid, [])) if rec["t"] <= start), None)
    return f"walked in ({walk.get('by') or '?'})" if walk else "walked in"


def phasing_rows(trace: Trace) -> list[dict]:
    """Every hole phase on a raid member, refreshes merged and each application its own row. `Big Bang`
    when it began inside a Big Bang cast, else `stray`."""
    bangs = [(cast["start"] - BANG_LEAD_MS, cast["end"]) for cast in big_bang_casts(trace)]
    matter = guids_of_entry(trace, NPC_DARK_MATTER)
    roster = roster_guids(trace)
    out = []
    for guid, stretches in phased_spans(trace, HOLE_PHASE, chain_ms=0).items():
        if guid not in roster:
            continue
        for start, stop, _ in stretches:
            bang = any(low <= start <= high for low, high in bangs)
            out.append({
                "guid": guid, "start": start, "stop": stop,
                "label": "Big Bang" if bang else "stray",
                "cause": "" if bang else stray_cause(trace, guid, start),
                "matter": sum(rec.get("a", 0) for rec in victim_hits(trace).get(guid, [])
                              if rec.get("s") in matter and start <= rec["t"] <= stop),
                "died": died_during(trace, guid, start, stop + SAMPLE_LAG_MS),
            })
    return sorted(out, key=lambda row: (row["start"], trace.name(row["guid"])))


def show_phasing(trace: Trace) -> None:
    print("PHASING")
    rows = phasing_rows(trace)
    if not rows:
        print("  nobody in the raid was phased by a hole")
    else:
        stray = [row for row in rows if row["label"] == "stray"]
        causes = collections.Counter(row["cause"].split(" (")[0] for row in stray)
        print(f"  {len(rows)} hole phase(s): {len(rows) - len(stray)} in a Big Bang, {len(stray)} stray"
              + (" (" + ", ".join(f"{cause} {n}" for cause, n in causes.most_common()) + ")" if stray else ""))
        print(f"  {'at':>9}  {'who':28} {'label':8} {'phased':>7} {'dark matter':>11}  {'died':4}  cause")
        for row in rows:
            print(f"  {clock(row['start']):>9}  {who(trace, row['guid']):28} {row['label']:8}"
                  f" {(row['stop'] - row['start']) / 1000:6.1f}s {row['matter']:11,}"
                  f"  {'yes' if row['died'] else '-':4}  {row['cause'] or '-'}")
    if boss_guid(trace) is None:
        return
    guarded = vetoes(trace, "algalon walk guard")
    if not guarded:
        print("  no algalon walk guard veto row in this trace")
        return
    per_bot: dict[int, collections.Counter] = collections.defaultdict(collections.Counter)
    for (guid, action), n in guarded.items():
        per_bot[guid][action] += n
    print("  algalon walk guard vetoes, only those no earlier multiplier had zeroed, one row per verdict change:")
    for guid, actions in sorted(per_bot.items(), key=lambda item: (-sum(item[1].values()), trace.name(item[0]))):
        print(f"    {who(trace, guid):28} {sum(actions.values()):4}  "
              + ", ".join(f"{action} {n}" for action, n in actions.most_common()))


# ---------------------------------------------------------------------------------------------- stars

def star_deaths(trace: Trace) -> list[dict]:
    """`{t, star, source, how, hits, damage, gap, raid_min}` per star death: `how` is `explosion` for a burst, `hole`
    for no burst but a hole on its last spot. `source` is the burst's `s`, often the Black Hole the star left."""
    samples = samples_of(trace)
    end = pull_end(trace)
    roster = roster_guids(trace)
    stars = guids_of_entry(trace, NPC_COLLAPSING_STAR)
    black = guids_of_entry(trace, NPC_BLACK_HOLE)

    # keyed by source: each explosion has its own, so stars dying close together never merge
    bursts: list[dict] = []
    latest: dict = {}
    for rec in sorted((rec for rec in trace.of("dmg") if rec.get("sp") in SPELL_BLACK_HOLE_EXPLOSION),
                      key=lambda rec: rec["t"]):
        source = rec.get("s")
        burst = latest.get(source)
        if burst and rec["t"] - burst["last"] <= BURST_GAP_MS:
            burst["hits"] += 1
            burst["damage"] += rec.get("a", 0)
            burst["last"] = rec["t"]
            burst["rows"].append(rec)
            continue
        burst = {"t": rec["t"], "last": rec["t"], "star": source if source in stars else None, "source": source,
                 "how": "explosion", "hits": 1, "damage": rec.get("a", 0), "rows": [rec]}
        bursts.append(burst)
        latest[source] = burst

    def spawned_on(burst: dict, spot) -> bool:
        seen_source = samples.seen(burst["source"]) if burst["source"] else None
        return bool(seen_source) and dist2(samples.row(burst["source"], seen_source[0])[1:3], spot) <= HOLE_MATCH_YD

    named = {burst["star"] for burst in bursts}
    gone_stars = sorted((seen[1], star) for star in stars - named
                        if (seen := samples.seen(star)) and seen[1] < end - GONE_MS)
    for gone, star in gone_stars:
        spot = samples.row(star, gone)[1:3]
        blind = min((burst for burst in bursts if burst["star"] is None and abs(burst["t"] - gone) <= HOLE_MATCH_MS),
                    key=lambda burst: (not spawned_on(burst, spot), abs(burst["t"] - gone)), default=None)
        if blind:
            blind["star"] = star
            continue
        hole = False
        for guid in black:
            seen_hole = samples.seen(guid)
            if seen_hole and gone - GONE_MS <= seen_hole[0] <= gone + HOLE_MATCH_MS:
                hole = hole or dist2(samples.row(guid, seen_hole[0])[1:3], spot) <= HOLE_MATCH_YD
        if hole:
            bursts.append({"t": gone, "last": gone, "star": star, "source": None, "how": "hole", "hits": 0,
                           "damage": 0, "rows": []})

    bursts.sort(key=lambda burst: burst["t"])
    previous = None
    for burst in bursts:
        frame = samples.frame(burst["t"] - 1)
        health = [row[5] for guid, row in frame.items() if guid in roster and alive(row)]
        burst["raid_min"] = min(health) if health else None
        burst["gap"] = burst["t"] - previous if previous is not None else None
        previous = burst["t"]
    return bursts


def span_totals(tracks) -> collections.Counter:
    """Milliseconds each value was held, summed over every holder of a per-bot key."""
    held: collections.Counter = collections.Counter()
    for spans in tracks.values():
        for value, start, stop in spans:
            held[value] += stop - start
    return held


def star_team(trace: Trace) -> dict:
    """The star team, each member's share of window-open time on the focus star, and dps off the team on any star.
    `drafted` is the ranged dps counted in while `algalon.urgent` read 1."""
    samples = samples_of(trace)
    end = pull_end(trace)
    roster = roster_guids(trace)
    stars = guids_of_entry(trace, NPC_COLLAPSING_STAR)
    team = holder_spans(trace, "algalon.starteam", end)
    focus = latch_spans(trace, "algalon.focusstar", end)
    urgent = latch_spans(trace, "algalon.urgent", end)
    members = sorted((guid for guid, spans in team.items() if any(value == "1" for value, _, _ in spans)),
                     key=trace.name)

    on: collections.Counter = collections.Counter()
    open_ms: collections.Counter = collections.Counter()
    off: collections.Counter = collections.Counter()
    off_from: dict[int, int] = {}
    drafted: set[int] = set()
    if stars:
        for _, when, weight in samples.steps(0, end):
            star = as_guid(value_at(focus, when))
            everyone_ranged = value_at(urgent, when) == "1"
            for guid in roster:
                row = samples.row(guid, when)
                if not alive(row):
                    continue
                listed = value_at(team.get(guid, []), when) == "1"
                if listed or (everyone_ranged and trace.role(guid) == "ranged"):
                    if not listed:
                        drafted.add(guid)
                    if star:
                        open_ms[guid] += weight
                        on[guid] += weight if target_of(row) == star else 0
                elif target_of(row) in stars and trace.role(guid) in ("melee", "ranged"):
                    off[guid] += weight
                    off_from.setdefault(guid, when)
    return {"members": members, "drafted": drafted, "on": on, "open": open_ms, "off": off, "off_from": off_from}


def parse_stardeath(text) -> tuple[str, float | None]:
    """`algalon.stardeath` reads `<killed|drained|chewed> <yd>`, older traces `<killed|collapsed> <yd>`,
    the yd `-` when nobody was unphased."""
    how, _, gap = str(text or "").strip().partition(" ")
    try:
        return how, float(gap)
    except ValueError:
        return how, None


def explosion_safe_hp(trace: Trace) -> int:
    return EXPLOSION_SAFE_HP[25 if trace.header.get("diff") in (1, 3) else 10]


def health_of(trace: Trace, guid: int, row) -> int | None:
    """Hit points from a snapshot row's percentage, or None without the unit's max health."""
    top = trace.maxhp.get(guid)
    return round(row[5] * top / 100.0) if top else None


def below_safe(trace: Trace, up: dict[int, list]) -> list[tuple[int, int, float]]:
    """`(hp, guid, pct)` for each of `up` a Black Hole Explosion could kill, weakest first: below its top
    roll, or below full health for someone whose max health is under it."""
    safe = explosion_safe_hp(trace)
    low = [(hp, guid, row[5]) for guid, row in up.items() if (hp := health_of(trace, guid, row)) is not None]
    return sorted(entry for entry in low if entry[0] < min(safe, trace.maxhp.get(entry[1], safe)))


def note_near(trace: Trace, key: str, when: int, slack: int) -> dict | None:
    found = [rec for rec in notes(trace, key) if abs(rec["t"] - when) <= slack]
    return min(found, key=lambda rec: abs(rec["t"] - when)) if found else None


def value_before(trace: Trace, key: str, when: int) -> str | None:
    """The latest value any bot wrote to `key` before `when`."""
    found = [rec for rec in notes(trace, key) if rec["t"] < when]
    return str(found[-1].get("txt", "")) if found else None


def drain_only(samples: Samples, star: int) -> tuple[float, float] | None:
    """`(health, expected)` at the star's last row: what it read, and what the drain alone leaves of its
    first reading by then."""
    found = samples.rows.get(star)
    if not found:
        return None
    stamps, rows = found
    return rows[-1][5], rows[0][5] - (stamps[-1] - stamps[0]) / 1000.0


STAR_PLAN_ROW = re.compile(r"^(\d+) (-?\d+)% (-?\d+)s(?: ([a-z]))?$")
UNDER_PART = re.compile(r"^under (-?\d+)\.\.(-?\d+)$")


def parse_starplan(text) -> dict:
    """`algalon.starplan` reads `<counter> <hp>% <s>s[ <u|s>]` per star, comma separated, soonest first, then
    blackouts (`smash 2..11`) and `under <a>..<b>`, joined by ` | `. Seconds from the note; older traces mark none."""
    stars: dict[int, dict] = {}
    under = None
    for part in str(text or "").split(" | "):
        part = part.strip()
        found = UNDER_PART.match(part)
        if found:
            under = (int(found.group(1)), int(found.group(2)))
            continue
        for item in part.split(", "):
            found = STAR_PLAN_ROW.match(item.strip())
            if found:
                stars[int(found.group(1))] = {"hp": int(found.group(2)), "secs": int(found.group(3)),
                                              "mark": found.group(4) or ""}
    return {"stars": stars, "under": under}


def starplans(trace: Trace) -> list[dict]:
    """Every `algalon.starplan` row, parsed, oldest first."""
    def build(trace: Trace) -> list[dict]:
        return [{"t": rec["t"], **parse_starplan(rec.get("txt"))}
                for rec in sorted(notes(trace, "algalon.starplan"), key=lambda rec: rec["t"])]
    return cached(trace, "_algalon_starplans", build)


def plan_of(trace: Trace, star: int | None, when: int) -> dict | None:
    """The star's entry in the last plan at or before `when` that listed it: `{t, hp, secs, mark, forecast,
    window}`, the forecast death and, for an under-star, the window in ms of the trace."""
    if not star:
        return None
    counter = star & 0xFFFFFFFF
    for plan in reversed(starplans(trace)):
        found = plan["stars"].get(counter)
        if plan["t"] <= when and found:
            window = ((plan["t"] + plan["under"][0] * 1000, plan["t"] + plan["under"][1] * 1000)
                      if found["mark"] == "u" and plan["under"] else None)
            return {**found, "t": plan["t"], "forecast": plan["t"] + found["secs"] * 1000, "window": window}
    return None


def parse_trim(text) -> tuple[str, int, float | None]:
    """`algalon.trim` reads `<start|stop> <star guid counter> <health %>`."""
    what, counter, pct = (str(text or "").strip().split(" ") + ["", "", ""])[:3]
    try:
        health = float(pct)
    except ValueError:
        health = None
    return what, as_guid(counter), health


def trim_runs(trace: Trace) -> dict[int, list[dict]]:
    """Per star guid counter, each wand run `{bot, start, start_hp, stop, stop_hp}` in order; `stop` is None
    for a run that never wrote one."""
    def build(trace: Trace) -> dict[int, list[dict]]:
        out: dict[int, list[dict]] = collections.defaultdict(list)
        for rec in sorted(notes(trace, "algalon.trim"), key=lambda rec: rec["t"]):
            what, counter, health = parse_trim(rec.get("txt"))
            bot = rec.get("g", 0)
            runs = out[counter]
            if what == "start":
                runs.append({"bot": bot, "start": rec["t"], "start_hp": health, "stop": None, "stop_hp": None})
            elif what == "stop":
                open_run = next((run for run in reversed(runs) if run["bot"] == bot and run["stop"] is None), None)
                if open_run is None:
                    open_run = {"bot": bot, "start": None, "start_hp": None, "stop": None, "stop_hp": None}
                    runs.append(open_run)
                open_run["stop"], open_run["stop_hp"] = rec["t"], health
        return dict(out)
    return cached(trace, "_algalon_trims", build)


def star_by_counter(trace: Trace) -> dict[int, int]:
    return cached(trace, "_algalon_star_counters",
                  lambda trace: {guid & 0xFFFFFFFF: guid for guid in guids_of_entry(trace, NPC_COLLAPSING_STAR)})


def counter_label(trace: Trace, counter: int) -> str:
    """`star N (counter)`, N from `star_labels`, the counter being how the star notes name it."""
    labels = cached(trace, "_algalon_star_labels", star_labels)
    guid = star_by_counter(trace).get(counter)
    return f"{labels[guid]} ({counter})" if guid in labels else str(counter)


def parse_underdrop(text) -> tuple[str, int, float | None, int | None]:
    """`algalon.underdrop` reads `<early|late|nosoak> <star guid counter> <health %> <ms to the next Big Bang
    cast>`."""
    why, counter, pct, ms = (str(text or "").strip().split(" ") + ["", "", "", ""])[:4]
    try:
        health = float(pct)
    except ValueError:
        health = None
    try:
        to_cast = int(ms)
    except ValueError:
        to_cast = None
    return why, as_guid(counter), health, to_cast


def bang_cycles(trace: Trace) -> list[dict]:
    """`{n, low, high}` per Big Bang the planner worked toward, `[low, high)` running from the previous
    cast's end to this one's, each plus PLAN_KEEP_MS. The last runs on for the cast that never came."""
    out = []
    low = -(1 << 62)
    for cast in big_bang_casts(trace):
        out.append({"n": cast["n"], "low": low, "high": cast["end"] + PLAN_KEEP_MS})
        low = out[-1]["high"]
    out.append({"n": len(out) + 1, "low": low, "high": 1 << 62})
    return out


def under_cycles(trace: Trace) -> list[dict]:
    """Per Big Bang with an under-star: the stars any plan marked `u` for it, their releases, focus switches between
    them (other stars skipped), each one's health at the cast, and `band`, the health then that dies in the window."""
    def build(trace: Trace) -> list[dict]:
        drops = []
        for rec in sorted(notes(trace, "algalon.underdrop"), key=lambda rec: rec["t"]):
            why, counter, health, to_cast = parse_underdrop(rec.get("txt"))
            drops.append((counter, {"t": rec["t"], "why": why, "hp": health, "to_cast": to_cast}))
        out = []
        for cycle in bang_cycles(trace):
            low, high = cycle["low"], cycle["high"]
            releases: dict[int, dict] = {}
            for counter, drop in drops:
                if low <= drop["t"] < high:
                    releases.setdefault(counter, drop)
            stars = set(releases) | {counter for plan in starplans(trace) if low <= plan["t"] < high
                                     for counter, entry in plan["stars"].items() if entry["mark"] == "u"}
            if not stars:
                continue
            held = [(row["start"], row["star"] & 0xFFFFFFFF) for row in focus_rates(trace)
                    if row["under"] and low <= row["start"] < high]
            switches = [start for (_, before), (start, after) in zip(held, held[1:]) if after != before]
            # the drain is 1% of max a second: a star at h% when the cast starts dies h-1 to h s later, by
            # where its clock stands, so a tick of slack on top
            cast = next((cast for cast in big_bang_casts(trace) if cast["n"] == cycle["n"]), None)
            band = ((cast["end"] - UNDER_LEAD_MS - cast["start"]) / 1000.0,
                    (cast["end"] + UNDER_TAIL_MS - cast["start"]) / 1000.0 + 1.0) if cast else None
            at_cast = {}
            for counter in stars:
                guid = star_by_counter(trace).get(counter)
                found = samples_of(trace).row(guid, cast["start"]) if cast and guid else None
                if found and alive(found) and samples_of(trace).present(guid, cast["start"]):
                    at_cast[counter] = found[5]
            out.append({"n": cycle["n"], "stars": sorted(stars), "releases": releases,
                        "visited": len({counter for _, counter in held}), "switches": len(switches),
                        "first_switch": switches[0] if switches else None, "cast": cast["start"] if cast else None,
                        "band": band, "at_cast": at_cast})
        return out
    return cached(trace, "_algalon_under_cycles", build)


def focus_rates(trace: Trace) -> list[dict]:
    """Per `algalon.focusstar` span: the star's health at its first and last sample, `beyond` its loss a second past
    the drain (None under two samples), and `under`, whether the plan then marked it `u`."""
    def build(trace: Trace) -> list[dict]:
        samples = samples_of(trace)
        windows = window_spans(trace)
        out = []
        for value, start, stop in latch_spans(trace, "algalon.focusstar", pull_end(trace)):
            star = as_guid(value)
            if not star:
                continue
            plan = plan_of(trace, star, start)
            row = {"star": star, "start": start, "stop": stop, "window": value_at(windows, start),
                   "under": bool(plan and plan["mark"] == "u"), "first": None, "last": None, "secs": 0.0,
                   "beyond": None}
            timed = samples.timed(star, start, stop)
            if timed:
                (low, first), (high, last) = timed[0], timed[-1]
                row.update(first=first[5], last=last[5], secs=(high - low) / 1000.0)
                if high > low:
                    row["beyond"] = (first[5] - last[5]) / row["secs"] - 1.0
            out.append(row)
        return out
    return cached(trace, "_algalon_focus_rates", build)


def trim_shots(trace: Trace) -> dict[int, list[dict]]:
    """Per star counter with a wand run, per trimmer: `{bot, starts, fired}`, when it started Shoot on the
    star and how many shots fired, from its first `algalon.trim start` to its last run's stop."""
    def build(trace: Trace) -> dict[int, list[dict]]:
        shoots = [rec for rec in trace.of("cast") if rec.get("sp") == SPELL_WAND_SHOOT]
        out: dict[int, list[dict]] = {}
        for counter, runs in trim_runs(trace).items():
            star = star_by_counter(trace).get(counter)
            rows = []
            for bot in dict.fromkeys(run["bot"] for run in runs):
                own = [run for run in runs if run["bot"] == bot]
                begun = [run["start"] for run in own if run["start"] is not None]
                if not begun:
                    continue
                low = min(begun)
                high = own[-1]["stop"] if own[-1]["stop"] is not None else pull_end(trace)
                mine = [rec for rec in shoots if rec.get("s") == bot and star and rec.get("tgt") == star
                        and low <= rec["t"] <= high]
                rows.append({"bot": bot, "starts": [rec["t"] for rec in mine if not rec.get("tr")],
                             "fired": sum(1 for rec in mine if rec.get("tr"))})
            out[counter] = rows
        return out
    return cached(trace, "_algalon_trim_shots", build)


def densest(stamps: list[int], span: int, least: int) -> tuple[int, int | None]:
    """`(count, from)`: the most of the sorted `stamps` within `span` ms of one another, and the first stamp
    opening `least` of them, None when none does."""
    best, found = 0, None
    for index, when in enumerate(stamps):
        count = bisect.bisect_right(stamps, when + span) - index
        best = max(best, count)
        if found is None and count >= least:
            found = when
    return best, found


def bang_hit(trace: Trace, when: int) -> dict | None:
    """The Big Bang whose hit lands nearest `when`, within STAR_BANG_MS: `{n, rel, inside}`, `rel` being
    `when` less the hit and `inside` whether `when` fell in the cast."""
    near = [(abs(when - cast["end"]), cast) for cast in big_bang_casts(trace)]
    near = [pair for pair in near if pair[0] <= STAR_BANG_MS]
    if not near:
        return None
    cast = min(near, key=lambda pair: pair[0])[1]
    return {"n": cast["n"], "rel": when - cast["end"], "inside": cast["start"] <= when <= cast["end"]}


def under_cause(how: str | None, plan: dict | None, when: int) -> str | None:
    """`under`, `early` or `late` for a star that was planned to die under the Big Bang: what the probe
    said, else the plan's window against the death, SLACK either side because the plan rounds its seconds."""
    if how in ("under", "early", "late"):
        return how
    if plan and plan["window"]:
        low, high = plan["window"]
        if when < low - STAR_PLAN_SLACK_MS:
            return "early"
        return "late" if when > high + STAR_PLAN_SLACK_MS else "under"
    return None


def under_outside(row: dict) -> tuple[str, int | None] | None:
    """`(early|late, ms outside the window or None)` for an under-star that died outside its window."""
    if row["cause"] not in ("early", "late"):
        return None
    window = row["plan"]["window"] if row["plan"] else None
    if not window:
        return row["cause"], None
    return row["cause"], window[0] - row["t"] if row["cause"] == "early" else row["t"] - window[1]


def star_rows(trace: Trace) -> list[dict]:
    """`star_deaths` plus what each landed into: probe note, window, meteor, Big Bang, nearest unphased member, who it
    could kill and did. `class`: `killed` as focus, `drained` within DRAINED_SLACK of the drain alone, else `chewed`."""
    def build(trace: Trace) -> list[dict]:
        samples = samples_of(trace)
        roster = roster_guids(trace)
        phased = all_phases(trace)
        casts = big_bang_casts(trace)
        pair, _ = swap_pair(trace)
        focus = latch_spans(trace, "algalon.focusstar", pull_end(trace))
        seen = [meteor["t"] for meteor in meteors(trace)[0]]
        killed = [row["t"] for row in death_rows(trace) if row["cause"] == "star explosion"]
        out = []
        for death in star_deaths(trace):
            when = death["t"]
            frame = samples.frame(when - 1)
            up = {guid: row for guid, row in frame.items()
                  if guid in roster and alive(row) and not phased_at(phased, guid, when)}
            last = samples.seen(death["star"]) if death["star"] else None
            spot = samples.row(death["star"], last[1])[1:3] if last else None
            weakest = min(((health_of(trace, guid, row), guid, row[5]) for guid, row in up.items()
                           if health_of(trace, guid, row) is not None), default=None)
            probe = note_near(trace, "algalon.stardeath", when, STAR_NOTE_MS)
            drain = drain_only(samples, death["star"]) if death["star"] else None
            if drain is None:
                kind = None
            elif as_guid(value_at(focus, when - 1)) == death["star"]:
                kind = "killed"
            else:
                kind = "drained" if drain[0] >= drain[1] - DRAINED_SLACK else "chewed"
            plan = plan_of(trace, death["star"], when)
            out.append({
                **death,
                "plan": plan,
                "cause": under_cause(parse_stardeath(probe.get("txt"))[0] if probe else None, plan, when),
                "bang_hit": bang_hit(trace, when),
                "unphased": sorted({rec["d"] for rec in death["rows"]
                                    if rec.get("d") in roster and not phased_at(phased, rec["d"], rec["t"])},
                                   key=trace.name),
                "trims": trim_runs(trace).get((death["star"] or 0) & 0xFFFFFFFF, []) if death["star"] else [],
                "note": parse_stardeath(probe.get("txt")) if probe else None,
                "window": value_before(trace, "algalon.starwindow", when),
                "meteor": any(when - METEOR_LIVE_MS <= appeared <= when for appeared in seen),
                "bang": next((cast["n"] for cast in casts if cast["start"] <= when <= cast["end"]), None),
                "nearest": (min(((dist2(row[1:3], spot), guid) for guid, row in up.items()), default=None)
                            if spot else None),
                "weakest": weakest,
                "below": below_safe(trace, up),
                "killed": sum(1 for at in killed if when - SAMPLE_LAG_MS <= at <= death["last"] + SHARE_MS),
                "drain": drain,
                "class": kind,
                # phased or not: the hole it leaves phases whoever stands there
                "pair": sorted((dist2(row[1:3], spot), guid) for guid, row in frame.items()
                               if spot and guid in pair and alive(row) and dist2(row[1:3], spot) <= STAR_NEAR_YD),
            })
        return out
    return cached(trace, "_algalon_star_rows", build)


def window_spans(trace: Trace) -> list[tuple[str, int, int]]:
    """`algalon.starwindow` as one timeline across every bot that tracked the stars, the latest row
    winning."""
    return latch_spans(trace, "algalon.starwindow", pull_end(trace))


def focus_near(spans, when: int) -> int:
    """The focus star at `when`, or the first one set within STAR_NOTE_MS after it: the window and the
    focus note can land a tick apart."""
    held = as_guid(value_at(spans, when))
    if held:
        return held
    return next((as_guid(value) for value, start, _ in spans if when <= start <= when + STAR_NOTE_MS
                 and as_guid(value)), 0)


def window_opens(trace: Trace) -> list[dict]:
    """Each kill window opening (`open`/`deadline` on the merged `algalon.starwindow`, or `algalon.urgent` turning
    1): the focus star, the team, its mean distance to the star and the star's distance from the team's centre."""
    def build(trace: Trace) -> list[dict]:
        samples = samples_of(trace)
        end = pull_end(trace)
        focus = latch_spans(trace, "algalon.focusstar", end)
        team = holder_spans(trace, "algalon.starteam", end)
        urgent = latch_spans(trace, "algalon.urgent", end)
        opens = [(start, value) for value, start, _ in window_spans(trace) if value in ("open", "deadline")]
        opens += [(start, "urgent") for value, start, _ in urgent if value == "1"]
        out = []
        for when, kind in sorted(opens):
            star = focus_near(focus, when)
            spot = position_at(samples, star, when) if star else None
            if spot is None:
                continue
            everyone_ranged = value_at(urgent, when) == "1"
            members = [guid for guid in roster_guids(trace)
                       if value_at(team.get(guid, []), when) == "1"
                       or (everyone_ranged and trace.role(guid) == "ranged")]
            spots = [row[1:3] for guid in members if alive(row := samples.row(guid, when))]
            if not spots:
                continue
            middle = (statistics.fmean(x for x, _ in spots), statistics.fmean(y for _, y in spots))
            out.append({"t": when, "kind": kind, "star": star, "members": len(spots),
                        "mean": statistics.fmean(dist2(point, spot) for point in spots),
                        "middle": dist2(middle, spot)})
        return out
    return cached(trace, "_algalon_window_opens", build)


PLAN_NAMES = {"u": "under", "s": "shelter", "o": "other"}


def trim_label(trace: Trace, run: dict) -> str:
    def at(when, pct):
        if when is None:
            return "-"
        return clock(when) + (f" {pct:.0f}%" if pct is not None else "")
    stop = at(run["stop"], run["stop_hp"]) if run["stop"] is not None else "never stopped"
    return f"{trace.name(run['bot'])} {at(run['start'], run['start_hp'])} .. {stop}"


def star_plan_line(trace: Trace, row: dict) -> str:
    """What the planner meant for a dead star beside what happened: its plan and window, the forecast
    against the death, the cause, the Big Bang hit, who the explosion hit unphased, and the wand runs."""
    parts = []
    plan = row["plan"]
    if plan:
        text = f"plan {PLAN_NAMES.get(plan['mark'], plan['mark']) or 'drain'}"
        if plan["window"]:
            text += f" window {clock(plan['window'][0])}..{clock(plan['window'][1])}"
        text += f", forecast death {(row['t'] - plan['forecast']) / 1000:+.1f} s off"
        parts.append(text)
    if row["cause"]:
        parts.append(f"died {row['cause']}")
    hit = row["bang_hit"]
    if hit:
        parts.append(f"{abs(hit['rel']) / 1000:.1f} s {'after' if hit['rel'] > 0 else 'before'} Big Bang {hit['n']}'s"
                     f" hit{', inside its cast' if hit['inside'] else ''}")
    if row["how"] == "explosion":
        parts.append("explosion hit unphased: " + (", ".join(trace.name(guid) for guid in row["unphased"])
                                                  or "nobody"))
    if row["trims"]:
        parts.append("trim " + "; ".join(trim_label(trace, run) for run in row["trims"]))
    return "; ".join(parts)


def release_label(drop: dict) -> str:
    health = "-" if drop["hp"] is None else f"{drop['hp']:.0f}%"
    before = "" if drop["to_cast"] is None else f", {drop['to_cast'] / 1000:.1f} s before the cast"
    return f"released {drop['why']} {clock(drop['t'])} at {health}{before}"


def show_under_cycles(trace: Trace) -> None:
    cycles = under_cycles(trace)
    if not cycles:
        return
    print("  under-stars per Big Bang, every star an algalon.starplan marked u toward it:")
    for cycle in cycles:
        print(f"    Big Bang {cycle['n']}: {len(cycle['stars'])} under-star(s), the focus on {cycle['visited']} of them"
              f" switched {cycle['switches']} time(s) between them")
        for counter in cycle["stars"]:
            drop = cycle["releases"].get(counter)
            health = cycle["at_cast"].get(counter)
            band = cycle["band"]
            where = (f"; at the cast {health:.1f}%, band {band[0]:.1f}..{band[1]:.1f}%"
                     if health is not None and band else "")
            print(f"      {counter_label(trace, counter)}: {release_label(drop) if drop else 'not released'}{where}")
    if "algalon.underdrop" not in emitted(trace):
        print("    no algalon.underdrop row: nothing released, or a trace from before the probe")


def show_focus_rates(trace: Trace) -> None:
    rows = focus_rates(trace)
    if not rows:
        return
    print("  focus spans, the star's health and what it lost beyond the 1 %/s drain, u for an under-star:")
    bulk = sorted(row["beyond"] for row in rows
                  if row["under"] and row["window"] == "under" and row["beyond"] is not None)
    if bulk:
        print(f"    bulk beyond the drain: median {bulk[len(bulk) // 2]:+.1f} %/s over {len(bulk)} span(s),"
              f" planned {UNDER_TEAM_PCT - 1.0:+.1f}")
    for row in rows:
        if row["first"] is None:
            health = "never sampled"
        else:
            beyond = "-" if row["beyond"] is None else f"{row['beyond']:+.1f} %/s"
            health = f"{row['first']:5.1f}% > {row['last']:5.1f}% in {row['secs']:4.1f} s, {beyond}"
        print(f"    {clock(row['start']):>9} .. {clock(row['stop']):>9}  {row['window'] or '-':9}"
              f" {counter_label(trace, row['star'] & 0xFFFFFFFF):16} {'u' if row['under'] else ' '}  {health}")


def star_flags(row: dict) -> str:
    flags = " <8s" if row["gap"] is not None and row["gap"] < STAR_GAP_MS else ""
    flags += " lowhp" if row["below"] else ""
    flags += " smash" if row["meteor"] else ""
    flags += " bigbang" if row["bang"] else ""
    flags += " near" if row["nearest"] and row["nearest"][0] < STAR_NEAR_YD else ""
    return flags


def show_stars(trace: Trace) -> None:
    print("COLLAPSING STARS")
    stars = guids_of_entry(trace, NPC_COLLAPSING_STAR)
    rows = star_rows(trace)
    if not stars and not rows:
        print("  no star was sampled")
        return
    show_absent(trace, "algalon.starwindow", "algalon.stardeath", "algalon.focusstar", "algalon.starteam")

    safe = explosion_safe_hp(trace)
    print(f"  {len(stars)} star(s) sampled, {len(rows)} died; an explosion can kill anyone unphased under {safe:,} hp")
    if rows:
        print(f"  {'at':>9} {'star':18} {'how':9} {'hits':>4} {'damage':>9} {'gap':>7} {'raid min':>8}")
    for row in rows:
        gap = "-" if row["gap"] is None else f"{row['gap'] / 1000:.1f}s"
        low = "-" if row["raid_min"] is None else f"{row['raid_min']:.0f}%"
        print(f"  {clock(row['t']):>9} {trace.name(row['star'])[:18]:18} {row['how']:9} {row['hits']:4}"
              f" {row['damage']:9,} {gap:>7} {low:>8}{star_flags(row)}")
        parts = []
        if row["note"]:
            old = " (the old probe's drained or chewed)" if row["note"][0] == "collapsed" else ""
            parts.append(f"algalon.stardeath {row['note'][0]} {'-' if row['note'][1] is None else row['note'][1]}{old}")
        if row["drain"]:
            parts.append(f"read as {row['class']}, {row['drain'][0]:.1f}% at its last sample against"
                         f" {row['drain'][1]:.1f}% on the drain alone")
        parts.append(f"window {row['window'] or '-'}")
        if row["nearest"]:
            parts.append(f"nearest {trace.name(row['nearest'][1])} {row['nearest'][0]:.1f} yd")
        if row["weakest"]:
            parts.append(f"weakest {trace.name(row['weakest'][1])} {row['weakest'][0]:,} hp ({row['weakest'][2]:.0f}%)")
        parts.append(f"{len(row['below'])} under {safe:,}")
        if row["meteor"]:
            parts.append("a meteor live")
        if row["bang"]:
            parts.append(f"Big Bang {row['bang']} casting")
        parts.append(f"{row['killed']} died to it")
        print(f"      {'; '.join(parts)}")
        line = star_plan_line(trace, row)
        if line:
            print(f"      {line}")

    window = span_totals({0: window_spans(trace)})
    total = sum(window.values())
    if total:
        print("  algalon.starwindow, every tracker's rows as one timeline: " + ", ".join(
            f"{value} {held / 1000:.1f} s ({held * 100.0 / total:.0f}%)" for value, held in window.most_common()))
    plans = latch_spans(trace, "algalon.starplan", pull_end(trace))
    if plans:
        windows = window_spans(trace)
        print("  algalon.starplan, every tracker's rows as one timeline, beside the window it was written under:")
        for value, start, _ in plans:
            print(f"    {clock(start):>9}  {value_at(windows, start) or '-':9} {value}")
    trims = trim_runs(trace)
    if trims:
        print(f"  algalon.trim: {sum(len(runs) for runs in trims.values())} wand run(s) on {len(trims)} star(s),"
              f" each trimmer's Shoot starts and the shots that fired:")
        for counter, rows in trim_shots(trace).items():
            print(f"    {counter_label(trace, counter)}: " + "; ".join(
                f"{trace.name(row['bot'])} {len(row['starts'])} start(s), {row['fired']} shot(s) fired"
                for row in rows))
    show_under_cycles(trace)
    show_focus_rates(trace)
    for opened in window_opens(trace):
        print(f"  window {opened['kind']} {clock(opened['t'])}: focus star {opened['middle']:.1f} yd from the team's"
              f" mean position, {opened['mean']:.1f} yd from its {opened['members']} member(s) on average")
    hazard = holder_spans(trace, "algalon.hazard", pull_end(trace))
    if not hazard:
        print("  absent from this trace: algalon.hazard")
    else:
        stepped = sorted(((sum(stop - start for value, start, stop in spans if value == "star"), guid)
                          for guid, spans in hazard.items()), reverse=True)
        print("  off a dying star (algalon.hazard=star): " + (", ".join(
            f"{trace.name(guid)} {held / 1000:.1f} s" for held, guid in stepped if held) or "nobody"))

    team = star_team(trace)
    if team["members"] or team["drafted"]:
        print("  star team, + for ranged counted in while algalon.urgent read 1:")
        for guid in team["members"] + sorted(team["drafted"] - set(team["members"]), key=trace.name):
            share = (f"{team['on'][guid] * 100.0 / team['open'][guid]:.0f}% of {team['open'][guid] / 1000:.1f} s"
                     if team["open"][guid] else "window never open while on the team")
            mark = "+" if guid not in team["members"] else " "
            print(f"   {mark}{who(trace, guid):28} on the focus star {share}")
    else:
        print("  no star team recorded")
    if team["off"]:
        print("  damage dealers off the team on a star: " + ", ".join(
            f"{trace.name(guid)} {held / 1000:.1f} s" for guid, held in team["off"].most_common()))


def window_faults(trace: Trace) -> list[tuple[int, str]]:
    """A deadline or urgent window opening on a focus star far from the star team."""
    return [(opened["t"], f"{opened['kind']} window @ {secs(opened['t'])}: focus star {opened['middle']:.1f} yd from"
                          f" the star team's mean position")
            for opened in window_opens(trace)
            if opened["kind"] in ("deadline", "urgent") and opened["middle"] > TEAM_FAR_YD]


def under_faults(trace: Trace) -> list[tuple[int, str]]:
    """An under-star released inside UNDER_DROP_LATE_MS of the cast or above its band at it, the focus going back to
    one (a latched bulk switches one fewer times than it visits), and a trimmer restarting Shoot (older traces)."""
    out = []
    for cycle in under_cycles(trace):
        for counter, drop in cycle["releases"].items():
            if drop["to_cast"] is not None and drop["to_cast"] < UNDER_DROP_LATE_MS:
                health = "-" if drop["hp"] is None else f"{drop['hp']:.0f}%"
                out.append((drop["t"], f"under-star {counter_label(trace, counter)} released {drop['why']}"
                                       f" {drop['to_cast'] / 1000:.1f} s before Big Bang {cycle['n']}'s cast, at"
                                       f" {health} @ {secs(drop['t'])}"))
        band = cycle["band"]
        for counter, health in sorted(cycle["at_cast"].items()):
            if band and counter not in cycle["releases"] and health > band[1]:
                out.append((cycle["cast"], f"under-star {counter_label(trace, counter)} at {health:.1f}% when Big Bang"
                                           f" {cycle['n']}'s cast began, above its band {band[0]:.1f}..{band[1]:.1f}%"
                                           f" @ {secs(cycle['cast'])}"))
        if cycle["visited"] and cycle["switches"] >= cycle["visited"]:
            out.append((cycle["first_switch"], f"Big Bang {cycle['n']}: the focus switched {cycle['switches']} times"
                                               f" between {cycle['visited']} under-stars, from"
                                               f" {secs(cycle['first_switch'])}"))
    for counter, rows in trim_shots(trace).items():
        for row in rows:
            count, at = densest(row["starts"], TRIM_RESTART_MS, TRIM_RESTARTS)
            if at is not None:
                out.append((at, f"{trace.name(row['bot'])} started Shoot {count} times within"
                                f" {TRIM_RESTART_MS // 1000} s on {counter_label(trace, counter)} @ {secs(at)}"
                                f" ({len(row['starts'])} starts, {row['fired']} shot(s) fired)"))
    return out


# ----------------------------------------------------------------------------------------- collateral

def star_labels(trace: Trace) -> dict[int, str]:
    """`star N` by the order the stars were first sampled, since they all share one name."""
    samples = samples_of(trace)
    order = sorted((seen[0], guid) for guid in guids_of_entry(trace, NPC_COLLAPSING_STAR)
                   if (seen := samples.seen(guid)))
    return {guid: f"star {index + 1}" for index, (_, guid) in enumerate(order)}


def worst_window(stamps: list[int], total: list[float]) -> tuple[float, int | None, int | None]:
    """`(amount, from, to)`: the largest rise of the running `total` within WORST_WINDOW_MS, the tightest
    such stretch where several tie."""
    best: tuple[float, int | None, int | None] = (0.0, None, None)
    for low in range(len(stamps)):
        for high in range(low + 1, len(stamps)):
            if stamps[high] - stamps[low] > WORST_WINDOW_MS:
                break
            amount = total[high] - total[low]
            tighter = best[1] is not None and stamps[high] - stamps[low] < best[2] - best[1]
            if amount > best[0] + 1e-6 or (abs(amount - best[0]) <= 1e-6 and tighter):
                best = (amount, stamps[low], stamps[high])
    return best


def star_excess(trace: Trace) -> list[dict]:
    """Per star, the health it lost beyond its 1 %/s drain while it was not the focus star: in total,
    in its worst WORST_WINDOW_MS, and when the total reached CHEWED_PCT."""
    def build(trace: Trace) -> list[dict]:
        samples = samples_of(trace)
        focus = latch_spans(trace, "algalon.focusstar", pull_end(trace))
        labels = star_labels(trace)
        out = []
        for star, label in labels.items():
            stamps, rows = samples.rows[star]
            total = [0.0]
            crossed = None
            for index in range(1, len(stamps)):
                excess = rows[index - 1][5] - rows[index][5] - (stamps[index] - stamps[index - 1]) / 1000.0
                unfocused = as_guid(value_at(focus, stamps[index - 1])) != star
                total.append(total[-1] + (excess if unfocused else 0.0))
                if crossed is None and total[-1] >= CHEWED_PCT:
                    crossed = stamps[index]
            amount, low, high = worst_window(stamps, total)
            out.append({"star": star, "label": label, "first": stamps[0], "last": stamps[-1], "excess": total[-1],
                        "worst": amount, "worst_from": low, "worst_to": high, "crossed": crossed})
        return out
    return cached(trace, "_algalon_excess", build)


STAR_GROUPS = ("star team on the focus star", "star team on another star", "off the star team")


def star_grouper(trace: Trace):
    """`group(owner, star, when)`: which of STAR_GROUPS a hit or cast on a star falls in."""
    end = pull_end(trace)
    team = holder_spans(trace, "algalon.starteam", end)
    focus = latch_spans(trace, "algalon.focusstar", end)
    urgent = latch_spans(trace, "algalon.urgent", end)

    def group(owner: int, star: int, when: int) -> str:
        listed = (value_at(team.get(owner, []), when) == "1"
                  or (value_at(urgent, when) == "1" and trace.role(owner) == "ranged"))
        if not listed:
            return STAR_GROUPS[2]
        return STAR_GROUPS[0] if star == as_guid(value_at(focus, when)) else STAR_GROUPS[1]
    return group


def star_casts(trace: Trace) -> dict[str, collections.Counter]:
    """`(caster, spell name)` counts of every cast on a star, split into the team on the focus star, the team on
    another star, and anyone off the team. A pet counts with its owner."""
    stars = guids_of_entry(trace, NPC_COLLAPSING_STAR)
    roster = roster_guids(trace)
    group = star_grouper(trace)
    out: dict[str, collections.Counter] = collections.defaultdict(collections.Counter)
    for rec in trace.of("cast"):
        target, who_cast = rec.get("tgt"), rec.get("s")
        owner = trace.owners.get(who_cast, who_cast)
        if target not in stars or owner not in roster:
            continue
        name = trace.spells.get(rec.get("sp")) or trace.spell(rec.get("sp"))
        out[group(owner, target, rec["t"])][(who_cast, name)] += 1
    return out


def star_hits(trace: Trace) -> tuple[dict[str, collections.Counter], dict[int, collections.Counter]]:
    """From the star hit `dmg` rows: damage per `(owner, spell name)` in each of STAR_GROUPS, and per star per
    owner. Both empty for a trace from before those rows."""
    stars = guids_of_entry(trace, NPC_COLLAPSING_STAR)
    roster = roster_guids(trace)
    group = star_grouper(trace)
    by_group: dict[str, collections.Counter] = collections.defaultdict(collections.Counter)
    by_star: dict[int, collections.Counter] = collections.defaultdict(collections.Counter)
    for rec in trace.of("dmg"):
        star, source = rec.get("d"), rec.get("s")
        owner = trace.owners.get(source, source)
        if star not in stars or owner not in roster:
            continue
        amount = rec.get("a", 0)
        name = trace.spells.get(rec.get("sp")) or trace.spell(rec.get("sp")) or "melee"
        by_group[group(owner, star, rec["t"])][(owner, name)] += amount
        by_star[star][owner] += amount
    return by_group, by_star


def bot_star_dots(trace: Trace) -> dict[int, tuple[int, collections.Counter]]:
    """Per bot, the first time it put a DoT on a star and its DoT casts on stars by name."""
    stars = guids_of_entry(trace, NPC_COLLAPSING_STAR)
    roster = roster_guids(trace)
    out: dict[int, tuple[int, collections.Counter]] = {}
    for rec in trace.of("cast"):
        caster = rec.get("s")
        owner = trace.owners.get(caster, caster)
        if rec.get("tgt") not in stars or rec.get("tr") or owner not in roster or owner in trace.humans:
            continue
        name = trace.spells.get(rec.get("sp")) or trace.spell(rec.get("sp"))
        if name not in STAR_DOT_NAMES:
            continue
        out.setdefault(owner, (rec["t"], collections.Counter()))[1][name] += 1
    return out


def show_star_hits(trace: Trace) -> None:
    by_group, by_star = star_hits(trace)
    if not by_star:
        print("  no star hit rows: a trace from before they were logged")
        return
    print("  damage on stars, a pet with its owner:")
    labels = star_labels(trace)
    for star, owners in sorted(by_star.items(), key=lambda item: labels.get(item[0], "")):
        top = ", ".join(f"{label_of(trace, guid)} {amount / 1000:.1f} k" for guid, amount in owners.most_common(4))
        print(f"    {labels.get(star, star)}: {sum(owners.values()) / 1000:.1f} k; {top}")
    for name in STAR_GROUPS:
        counted = by_group.get(name)
        if not counted:
            continue
        print(f"  damage on stars, {name}:")
        by_owner: dict[int, collections.Counter] = collections.defaultdict(collections.Counter)
        for (guid, spell), amount in counted.items():
            by_owner[guid][spell] += amount
        for guid, spells in sorted(by_owner.items(), key=lambda item: -sum(item[1].values())):
            print(f"    {label_of(trace, guid):34} {sum(spells.values()) / 1000:6.1f} k: "
                  + ", ".join(f"{spell} {amount / 1000:.1f} k" for spell, amount in spells.most_common(6)))


def show_collateral(trace: Trace) -> None:
    print("STAR COLLATERAL")
    rows = star_excess(trace)
    if not rows:
        print("  no star was sampled")
        return
    show_absent(trace, "algalon.focusstar", "algalon.starteam")
    deaths = {row["star"]: row["t"] for row in star_deaths(trace) if row["star"]}
    print("  health lost beyond the 1 %/s drain while not the focus star:")
    for row in rows:
        gone = f"died {clock(deaths[row['star']])}" if row["star"] in deaths else f"last seen {clock(row['last'])}"
        worst = (f", worst {WORST_WINDOW_MS // 1000} s {row['worst']:.1f}% from {clock(row['worst_from'])}"
                 f" to {clock(row['worst_to'])}" if row["worst_from"] is not None else "")
        print(f"    {row['label']:8} spawned {clock(row['first'])}, {gone}: {row['excess']:5.1f}%{worst}")
    show_star_hits(trace)
    groups = star_casts(trace)
    for group in STAR_GROUPS:
        counted = groups.get(group)
        if not counted:
            continue
        print(f"  casts on stars, {group}:")
        by_caster: dict[int, collections.Counter] = collections.defaultdict(collections.Counter)
        for (guid, name), n in counted.items():
            by_caster[guid][name] += n
        for guid, names in sorted(by_caster.items(), key=lambda item: (-sum(item[1].values()), trace.name(item[0]))):
            print(f"    {label_of(trace, guid):34} " + ", ".join(f"{name} {n}" for name, n in names.most_common()))
    if not groups:
        print("  no cast on a star")
    print("  * human")
    show_vetoes(trace, "algalon splash hold")


def collateral_faults(trace: Trace) -> list[tuple[int, str]]:
    out = [(row["crossed"], f"star chewed: {row['label']} lost {row['excess']:.1f}% beyond its drain while not the"
                            f" focus star, {row['worst']:.1f}% of it in {WORST_WINDOW_MS // 1000} s from"
                            f" {secs(row['worst_from'])}")
           for row in star_excess(trace) if row["crossed"] is not None]
    for guid, (first, names) in bot_star_dots(trace).items():
        out.append((first, f"DoT on a star: {trace.name(guid)} "
                           + ", ".join(f"{name} {n}" for name, n in names.most_common())))
    return out


# ---------------------------------------------------------------------------------------------- holes

def hole_counts(trace: Trace) -> list[int]:
    samples = samples_of(trace)
    holes = hole_guids(trace)
    return [len(holes_at(samples, holes, when)) for when in samples.stamps]


def hole_stock(trace: Trace) -> list[tuple[int, int]]:
    """`(t, count)` at every change in how many holes the sweep saw."""
    out: list[tuple[int, int]] = []
    for when, count in zip(samples_of(trace).stamps, hole_counts(trace)):
        if not out or out[-1][1] != count:
            out.append((when, count))
    return out


def empty_before(trace: Trace, counts: list[int], when: int) -> int:
    """Milliseconds with no hole in the shelter window before `when`."""
    return sum(weight for index, _, weight in samples_of(trace).steps(when - SHELTER_WINDOW_MS, when)
               if counts[index] == 0)


def show_holes(trace: Trace) -> None:
    print("HOLES")
    holes = hole_guids(trace)
    lost = notes(trace, "algalon.holelost")
    if not holes and not lost:
        print("  no hole was sampled")
        return
    show_absent(trace, "algalon.holes", "algalon.holelost", "algalon.urgent")

    stock = hole_stock(trace)
    shown = stock[:TIMELINE_SHOWN]
    print("  swept stock: " + ", ".join(f"{clock(when)} {count}" for when, count in shown)
          + (f" and {len(stock) - len(shown)} more" if len(stock) > len(shown) else ""))
    probe = latch_spans(trace, "algalon.holes", pull_end(trace))
    if probe:
        print("  algalon.holes: " + ", ".join(f"{clock(start)} {value}" for value, start, _ in probe[:TIMELINE_SHOWN])
              + (f" and {len(probe) - TIMELINE_SHOWN} more" if len(probe) > TIMELINE_SHOWN else ""))

    counts = hole_counts(trace)
    samples = samples_of(trace)
    for cast in big_bang_casts(trace):
        present = len(holes_at(samples, holes, cast["start"]))
        print(f"  Big Bang #{cast['n']} {clock(cast['start'])}: {present} hole(s) at the cast,"
              f" {empty_before(trace, counts, cast['start']) / 1000:.1f} s with none in the"
              f" {SHELTER_WINDOW_MS // 1000} s before")

    reasons = collections.Counter(str(rec.get("txt", "")) for rec in lost)
    if lost:
        print("  algalon.holelost: " + ", ".join(f"{reason} {n}" for reason, n in reasons.most_common())
              + "  (" + ", ".join(f"{clock(rec['t'])} {rec.get('txt')}" for rec in lost[:TIMELINE_SHOWN]) + ")")
    urgent = [(start, stop) for value, start, stop in latch_spans(trace, "algalon.urgent", pull_end(trace))
              if value == "1"]
    if urgent:
        print(f"  algalon.urgent: {len(urgent)} span(s), {sum(b - a for a, b in urgent) / 1000:.1f} s: "
              + ", ".join(f"{clock(a)}..{clock(b)}" for a, b in urgent))


# ------------------------------------------------------------------------------------- constellations

def victim_changes(samples: Samples, guid: int) -> list[tuple[int, int]]:
    """`(t, victim)` at each change of a unit's target column while it lives."""
    out: list[tuple[int, int]] = []
    for when, row, _ in samples.unit_steps(guid):
        victim = target_of(row)
        if alive(row) and (not out or out[-1][1] != victim):
            out.append((when, victim))
    return out


def constellation_rows(trace: Trace) -> list[dict]:
    """Per constellation: when it woke (first victim, cast or hit), who it chased, and how it left."""
    samples = samples_of(trace)
    end = pull_end(trace)
    holes = hole_guids(trace)
    handler = latch_spans(trace, "algalon.handler", end)
    p2 = phase_start(trace, "P2")
    despawn = final_despawn(trace)
    casts = trace.of("cast")
    hits = trace.of("dmg")

    out = []
    # removal is the last row, not the first gap: a unit can drop out of the sweep and come back
    for guid in sorted(guids_of_entry(trace, NPC_LIVING_CONSTELLATION)):
        found = samples.rows.get(guid)
        if not found:
            continue
        stamps, rows = found
        woke = [when for when, row in zip(stamps, rows) if target_of(row)][:1]
        woke += [rec["t"] for rec in casts if rec.get("s") == guid][:1]
        woke += [rec["t"] for rec in hits if rec.get("s") == guid][:1]
        active = min(woke) if woke else None

        removed = stamps[-1] if stamps[-1] < end - GONE_MS else None
        how, spent = None, []
        if removed is not None:
            spot = rows[-1][1:3]
            spent = [hole for hole in sorted(holes)
                     if (seen := samples.seen(hole)) and abs(seen[1] - removed) <= HOLE_MATCH_MS
                     and dist2(samples.row(hole, seen[1])[1:3], spot) <= HOLE_MATCH_YD]
            if p2 is not None and abs(removed - p2) <= 2000:
                how = "phase2"
            elif removed == despawn:
                how, spent = "despawn", []
            elif spent:
                how = "hole"
            elif min(row[5] for row in samples.between(guid, removed - 2000, removed)) <= KILLED_HP:
                how = "killed"
            else:
                how = "gone"
        out.append({
            "guid": guid,
            "active": active,
            "victims": [(when, victim) for when, victim in victim_changes(samples, guid)
                        if active is not None and when >= active],
            "removed": removed,
            "how": how,
            "spent": spent,
            "last_victim": target_of(rows[-1]),
            "handler": as_guid(value_at(handler, removed)) if removed is not None else 0,
        })
    return out


def eaten_between(trace: Trace, low: int, high: int) -> list[dict]:
    """The constellations that left through a hole in `[low, high]`."""
    return [row for row in constellation_rows(trace)
            if row["how"] == "hole" and row["removed"] is not None and low <= row["removed"] <= high]


def kite_runs(trace: Trace) -> list[dict]:
    """Per bot, each unbroken stretch of KITE_STAGES: `eaten`, the constellations through a hole by EATEN_SLACK_MS
    after it, and `retries`, each `stage` after a `parked`, a pass that missed."""
    def build(trace: Trace) -> list[dict]:
        out = []
        for guid, spans in handler_kite_tracks(trace).items():
            run: list[tuple[str, int, int]] = []
            for span in spans + [("", pull_end(trace), pull_end(trace))]:
                # another bot's stint in between ends the run too
                if span[0] in KITE_STAGES and (not run or run[-1][2] == span[1]):
                    run.append(span)
                    continue
                if run:
                    start, stop = run[0][1], run[-1][2]
                    out.append({
                        "guid": guid, "start": start, "stop": stop, "stages": run,
                        "eaten": [row for row in eaten_between(trace, start, stop + EATEN_SLACK_MS)
                                  if guid in (row["last_victim"], row["handler"])],
                        "retries": sum(1 for before, after in zip(run, run[1:])
                                       if before[0] == "parked" and after[0] == "stage"),
                    })
                run = [span] if span[0] in KITE_STAGES else []
        return sorted(out, key=lambda item: (item["start"], item["guid"]))
    return cached(trace, "_algalon_kite_runs", build)


def parked_stretches(trace: Trace) -> list[dict]:
    """Per `algalon.kite=parked` stretch of PARKED_MS or more with the bot within PARKED_HOLE_YD of its kite hole by
    then (`algalon.kitehole`, else the nearest), and whether a constellation went through a hole by its end."""
    def build(trace: Trace) -> list[dict]:
        samples = samples_of(trace)
        end = pull_end(trace)
        assigned = holder_spans(trace, "algalon.kitehole", end)
        holes = hole_guids(trace)
        out = []
        for guid, spans in holder_spans(trace, "algalon.kite", end).items():
            for value, start, stop in spans:
                if value != "parked" or stop - start < PARKED_MS:
                    continue
                when = start + PARKED_MS
                stood = position_at(samples, guid, when)
                hole = as_guid(value_at(assigned.get(guid, []), start))
                if not hole or not samples.present(hole, start):
                    spots = [(dist2(stood, found[1:3]), near) for near in holes_at(samples, holes, start)
                             if stood and (found := samples.row(near, start))]
                    hole = min(spots)[1] if spots else 0
                found = (samples.row(hole, when) or samples.row(hole, start)) if hole else None
                if not stood or not found:
                    continue
                gap = dist2(stood, found[1:3])
                if gap <= PARKED_HOLE_YD:
                    out.append({"guid": guid, "start": start, "stop": stop, "hole": hole, "gap": gap,
                                "eaten": bool(eaten_between(trace, start, stop + EATEN_SLACK_MS))})
        return sorted(out, key=lambda item: item["start"])
    return cached(trace, "_algalon_parked", build)


def handler_kite_spans(trace: Trace) -> list[tuple[str, int, int, int]]:
    """`(value, start, stop, guid)` of `algalon.kite` while its bot was the elected handler: a tank's last note
    otherwise runs on through the other tank's stints."""
    end = pull_end(trace)
    kite = holder_spans(trace, "algalon.kite", end)
    out = []
    for value, start, stop in latch_spans(trace, "algalon.handler", end):
        guid = as_guid(value)
        for held, low, high in kite.get(guid, []):
            low, high = max(low, start), min(high, stop)
            if low < high:
                out.append((held, low, high, guid))
    return out


def handler_kite_tracks(trace: Trace) -> dict[int, list[tuple[str, int, int]]]:
    """`handler_kite_spans` per bot; every bot's own notes on a trace without `algalon.handler`."""
    if not latch_spans(trace, "algalon.handler"):
        return holder_spans(trace, "algalon.kite", pull_end(trace))
    tracks: dict[int, list[tuple[str, int, int]]] = collections.defaultdict(list)
    for value, start, stop, guid in handler_kite_spans(trace):
        tracks[guid].append((value, start, stop))
    return dict(tracks)


def stuck_constellations(trace: Trace) -> dict[int, float]:
    """Per woken constellation that never came within CONSTELLATION_LOW_Z of the floor, the lowest it got
    above it: no hole can take one up there, and it evades home after each wave wakes it."""
    samples = samples_of(trace)
    out = {}
    for row in constellation_rows(trace):
        if row["active"] is None or row["how"] in ("phase2", "despawn"):
            continue
        until = row["removed"] if row["removed"] is not None else pull_end(trace)
        if until - row["active"] < STUCK_MIN_MS:
            continue
        heights = [found[3] - ROOM_CENTER[2] for found in samples.between(row["guid"], row["active"], until)
                   if alive(found) and len(found) > 3]
        if heights and min(heights) > CONSTELLATION_LOW_Z:
            out[row["guid"]] = min(heights)
    return out


def kite_blocked(trace: Trace) -> dict[str, int]:
    """Ms the handler sat in `taunt` holding a constellation low enough for a hole, a hole spare and no gather:
    `stuck` with every loose one up in the air, `cooling` with its class taunt cast within TAUNT_COOLDOWN_MS."""
    def build(trace: Trace) -> dict[str, int]:
        samples = samples_of(trace)
        end = pull_end(trace)
        holes = latch_spans(trace, "algalon.holes", end)
        casts = [cast["start"] for cast in big_bang_casts(trace)]
        constellations = guids_of_entry(trace, NPC_LIVING_CONSTELLATION)
        woke = {row["guid"]: row["active"] for row in constellation_rows(trace) if row["active"] is not None}
        taunts = collections.defaultdict(list)
        for rec in trace.of("cast"):
            if rec.get("sp") in TAUNT_IDS and rec.get("sp") != SPELL_RIGHTEOUS_DEFENSE and not rec.get("tr"):
                taunts[rec.get("s")].append(rec["t"])
        out = {"stuck": 0, "cooling": 0}
        for value, start, stop, handler in handler_kite_spans(trace):
            if value != "taunt":
                continue
            for index, when, weight in samples.steps(start, stop):
                ahead = [cast - when for cast in casts if cast + BIG_BANG_CAST_MS > when]
                lead = min(ahead) if ahead else None
                if lead is not None and lead <= GATHER_LEAD_MS:
                    continue
                reserved = 1 if lead is not None and lead <= SHELTER_WINDOW_MS else 0
                if int(value_at(holes, when) or 0) <= reserved:
                    continue
                frame = [(guid, row) for guid, row in samples.units[index].items()
                         if guid in constellations and alive(row) and len(row) > 3]
                low = [row for _, row in frame if target_of(row) and row[3] - ROOM_CENTER[2] <= CONSTELLATION_LOW_Z]
                high = [guid for guid, row in frame
                        if guid in woke and woke[guid] <= when and row[3] - ROOM_CENTER[2] > CONSTELLATION_LOW_Z]
                if not any(target_of(row) == handler for row in low):
                    continue
                if all(target_of(row) == handler for row in low):
                    # a stale `taunt` with nothing up there is not this
                    if high:
                        out["stuck"] += weight
                elif any(0 <= when - cast < TAUNT_COOLDOWN_MS for cast in taunts.get(handler, [])):
                    out["cooling"] += weight
        return out
    return cached(trace, "_algalon_kite_blocked", build)


def loose_at_bang(trace: Trace) -> list[dict]:
    """`{n, t, guid, victim, handler}` per awake constellation that was on someone other than the elected
    handler when a Big Bang's cast began. Nothing without a handler: with a human tank nobody kites."""
    samples = samples_of(trace)
    handlers = latch_spans(trace, "algalon.handler", pull_end(trace))
    out = []
    for cast in big_bang_casts(trace):
        handler = as_guid(value_at(handlers, cast["start"]))
        if not handler:
            continue
        for row in constellation_rows(trace):
            if row["active"] is None or row["active"] > cast["start"] or (
                    row["removed"] is not None and row["removed"] <= cast["start"]):
                continue
            seen = [victim for when, victim in victim_changes(samples, row["guid"]) if when <= cast["start"]]
            if seen and seen[-1] and seen[-1] != handler:
                out.append({"n": cast["n"], "t": cast["start"], "guid": row["guid"], "victim": seen[-1],
                            "handler": handler})
    return out


def kite_label(trace: Trace, run: dict) -> str:
    stages = " > ".join(f"{value} {clock(start)}" for value, start, _ in run["stages"])
    ate = (f"eaten {len(run['eaten'])} at " + ", ".join(clock(row["removed"]) for row in run["eaten"])
           if run["eaten"] else "none eaten")
    retry = f", {run['retries']} retr{'y' if run['retries'] == 1 else 'ies'}" if run["retries"] else ""
    return (f"{trace.name(run['guid'])} {clock(run['start'])} .. {clock(run['stop'])}: {stages}; {ate}{retry}")


def show_constellations(trace: Trace) -> None:
    print("LIVING CONSTELLATIONS")
    rows = constellation_rows(trace)
    if not rows:
        print("  no constellation was sampled")
        return
    show_absent(trace, "algalon.handler", "algalon.kite", "algalon.kitehole")

    woke = [row for row in rows if row["active"] is not None]
    print(f"  {len(rows)} sampled, {len(woke)} woke")
    for row in woke:
        chase = ", ".join(f"{clock(when)} {who(trace, victim)}" for when, victim in row["victims"][:6] if victim)
        left = "-"
        if row["removed"] is not None:
            left = (f"{row['how']} at {clock(row['removed'])}, {(row['removed'] - row['active']) / 1000:.1f} s awake,"
                    f" last on {trace.name(row['last_victim'])}")
        stuck = stuck_constellations(trace).get(row["guid"])
        air = f"; stuck in the air, never below +{stuck:.1f} yd" if stuck is not None else ""
        print(f"    woke {clock(row['active'])}  chased {chase or 'nobody'}\n      left: {left}{air}")

    held: collections.Counter = collections.Counter()
    for row in woke:
        # each over its own awake span, an inactive one has no victim to count
        until = row["removed"] if row["removed"] is not None else math.inf
        held += threat_share(trace, {row["guid"]}, lambda when, a=row["active"], b=until: a <= when < b)[0]
    if sum(held.values()):
        show_share(held, "victims by role while awake")

    handler = latch_spans(trace, "algalon.handler", pull_end(trace))
    if handler:
        print("  algalon.handler: " + ", ".join(f"{clock(start)} {trace.name(as_guid(value))}"
                                               for value, start, _ in handler))
    own = handler_kite_spans(trace)
    if own:
        totals: collections.Counter = collections.Counter()
        busy: collections.Counter = collections.Counter()
        for value, start, stop, guid in own:
            totals[value] += stop - start
            if value not in ("none", ""):
                busy[guid] += stop - start
        print("  algalon.kite, the handler's own: "
              + ", ".join(f"{value} {held / 1000:.1f} s" for value, held in totals.most_common()))
        print("    by bot, outside none: " + ", ".join(f"{trace.name(guid)} {held / 1000:.1f} s"
                                                     for guid, held in busy.most_common()))
    blocked = kite_blocked(trace)
    if sum(blocked.values()):
        print(f"  kite blocked {sum(blocked.values()) / 1000:.1f} s, taunting with one held, a hole spare and no Big"
              f" Bang gathering: none loose but one in the air {blocked['stuck'] / 1000:.1f} s, taunt cooling"
              f" {blocked['cooling'] / 1000:.1f} s")
    runs = kite_runs(trace)
    if runs:
        print(f"  kite runs, {sum(1 for run in runs if run['eaten'])} of {len(runs)} ended with a constellation"
              " eaten:")
        for run in runs:
            print(f"    {kite_label(trace, run)}")
    for stretch in parked_stretches(trace):
        print(f"  parked {(stretch['stop'] - stretch['start']) / 1000:.1f} s from {clock(stretch['start'])},"
              f" {trace.name(stretch['guid'])} {stretch['gap']:.1f} yd from its kite hole, "
              + ("a constellation eaten" if stretch["eaten"] else "none eaten"))
    assigned = [rec for rec in notes(trace, "algalon.kitehole") if as_guid(rec.get("txt"))]
    if assigned:
        print(f"  algalon.kitehole: {len(assigned)} assignment(s) to"
              f" {len({rec.get('txt') for rec in assigned})} hole(s)")
    spent = sum(len(row["spent"]) for row in rows if row["how"] == "hole")
    lost = collections.Counter(str(rec.get("txt", "")) for rec in notes(trace, "algalon.holelost"))
    print(f"  holes spent on constellations: {spent} seen swallowed, algalon.holelost kite {lost['kite']},"
          f" stray {lost['stray']}, bigbang {lost['bigbang']}")


# ---------------------------------------------------------------------------------------------- smash

def meteors(trace: Trace) -> tuple[list[dict], str]:
    """`{t, spot}` per meteor and where they came from: the `haz` rows, else each 33105 marker's first
    row, else each 33104's."""
    def build(trace: Trace) -> tuple[list[dict], str]:
        hazards = [rec for rec in trace.of("haz") if rec.get("sp") in SPELL_COSMIC_SMASH]
        if hazards:
            return [{"t": rec["t"], "spot": (rec.get("x", 0.0), rec.get("y", 0.0))} for rec in hazards], "haz"
        samples = samples_of(trace)
        for entry in reversed(NPC_COSMIC_SMASH_MARKERS):
            found = sorted((seen[0], guid) for guid in guids_of_entry(trace, entry) if (seen := samples.seen(guid)))
            if found:
                return ([{"t": when, "spot": tuple(samples.row(guid, when)[1:3])} for when, guid in found],
                        f"{entry} markers")
        return [], "-"
    return cached(trace, "_algalon_meteors", build)


def smash_waves(trace: Trace) -> list[dict]:
    """Meteors that fell together, `{seen, impact, meteors, hits}`. The impact is the wave's first
    Cosmic Smash row, or SMASH_DELAY_MS after it showed when nothing was hit."""
    def build(trace: Trace) -> list[dict]:
        hits = [rec for rec in trace.of("dmg") if rec.get("sp") in SPELL_COSMIC_SMASH]
        waves: list[dict] = []
        for meteor in sorted(meteors(trace)[0], key=lambda meteor: meteor["t"]):
            if waves and meteor["t"] - waves[-1]["seen"] <= WAVE_MS:
                waves[-1]["meteors"].append(meteor)
            else:
                waves.append({"seen": meteor["t"], "meteors": [meteor]})
        for wave in waves:
            first = next((rec["t"] for rec in hits
                          if wave["seen"] + IMPACT_MIN_MS <= rec["t"] <= wave["seen"] + IMPACT_MAX_MS), None)
            wave["impact"] = first if first is not None else wave["seen"] + SMASH_DELAY_MS
            wave["hits"] = [rec for rec in hits
                            if wave["impact"] - SAMPLE_LAG_MS <= rec["t"] <= wave["impact"] + SMASH_MATCH_MS]
        return waves
    return cached(trace, "_algalon_waves", build)


def to_meteor(wave: dict, spot) -> float | None:
    return min(dist2(spot, meteor["spot"]) for meteor in wave["meteors"]) if spot else None


def wave_dodge(spans, wave: dict):
    """The dodge branch this wave set. The probe falls back to `idle` once the bot is clear, so the
    value at the impact is usually that rather than how the dodge went."""
    found = None
    for value, start, stop in spans:
        if start <= wave["impact"] and stop > wave["seen"] and value != "idle":
            found = value
    return found


def wave_rows(trace: Trace, wave: dict) -> list[dict]:
    """Per raid member within SMASH_NEAR_YD of a meteor as it showed or hit: dodge latency, the probe's verdict,
    where it stood at the impact, and what brought it back: the last walk, or a charge that closed in."""
    samples = samples_of(trace)
    dodges = holder_spans(trace, "algalon.dodge", pull_end(trace))
    rows = []
    for guid in roster_guids(trace):
        first = samples.row(guid, wave["seen"])
        if not alive(first):
            continue
        at_seen = to_meteor(wave, first[1:3])
        at_impact = to_meteor(wave, position_at(samples, guid, wave["impact"]))
        if min(at_seen, at_impact if at_impact is not None else math.inf) > SMASH_NEAR_YD:
            continue
        moves = [rec for rec in moves_of(trace).get(guid, []) if wave["seen"] <= rec["t"] <= wave["impact"]]
        dodge = next((rec for rec in moves if rec.get("by") == SMASH_MOVER), None)
        back = next((rec for rec in reversed(moves)
                     if dodge and rec["t"] > dodge["t"] and rec.get("by") != SMASH_MOVER), None)
        charge = next((rec for rec in reversed(charges_of(trace).get(guid, []))
                       if dodge and dodge["t"] < rec["t"] <= wave["impact"]
                       and charged_in(samples, wave, guid, rec["t"])), None)
        rows.append({
            "guid": guid,
            "at_seen": at_seen,
            "moved": dodge["t"] - wave["seen"] if dodge else None,
            "dodge": wave_dodge(dodges.get(guid, []), wave),
            "at_impact": at_impact,
            "damage": sum(rec.get("a", 0) for rec in wave["hits"] if rec.get("d") == guid),
            "back": SPELL_CHARGES[charge["sp"]] if charge else back.get("by") if back else None,
        })
    return sorted(rows, key=lambda row: math.inf if row["at_impact"] is None else row["at_impact"])


def charged_in(samples: Samples, wave: dict, guid: int, when: int) -> bool:
    before = to_meteor(wave, position_at(samples, guid, when))
    after = to_meteor(wave, position_at(samples, guid, min(when + CHARGE_LAND_MS, wave["impact"])))
    return before is not None and after is not None and after < before


def band(distance: float) -> str:
    return next(name for edge, name in SMASH_BANDS if distance < edge)


def show_smash(trace: Trace) -> None:
    print("COSMIC SMASH")
    waves = smash_waves(trace)
    explained = {id(rec) for wave in waves for rec in wave["hits"]}
    stray = [rec for rec in trace.of("dmg") if rec.get("sp") in SPELL_COSMIC_SMASH and id(rec) not in explained]
    if not waves and not stray:
        print("  no meteor fell and nothing took Cosmic Smash damage")
        return
    source = meteors(trace)[1]
    print("  meteors from haz" if source == "haz" else f"  absent from this trace: haz meteors; read off {source}")
    show_absent(trace, "algalon.dodge")

    by_band: dict[str, list[int]] = collections.defaultdict(list)
    for wave in waves:
        print(f"  wave {clock(wave['seen'])}: {len(wave['meteors'])} meteor(s), impact {clock(wave['impact'])}"
              f" (+{(wave['impact'] - wave['seen']) / 1000:.1f} s)")
        rows = wave_rows(trace, wave)
        if rows:
            print(f"      {'who':28} {'at seen':>7} {'moved':>7} {'dodge':>6} {'at impact':>9} {'damage':>8}"
                  f"  brought back by")
        for row in rows:
            moved = "-" if row["moved"] is None else f"+{row['moved'] / 1000:.1f}s"
            impact = "-" if row["at_impact"] is None else f"{row['at_impact']:.1f}"
            name = who(trace, row["guid"]) + ("*" if row["guid"] in trace.humans else "")
            print(f"      {name:28} {row['at_seen']:7.1f} {moved:>7} {row['dodge'] or '-':>6}"
                  f" {impact:>9} {row['damage']:8,}  {row['back'] or '-'}")
            if row["damage"] and row["at_impact"] is not None:
                by_band[band(row["at_impact"])].append(row["damage"])
    if by_band:
        print("  damage by distance at the impact: " + ", ".join(
            f"{name} {len(by_band[name])} x {sum(by_band[name]):,}" for _, name in SMASH_BANDS if by_band[name]))
    if stray:
        print(f"  {len(stray)} Cosmic Smash hit(s) no wave explains: "
              + ", ".join(f"{clock(rec['t'])} {trace.name(rec.get('d'))} {rec.get('a', 0):,}" for rec in stray[:8]))


def smash_faults(trace: Trace) -> list[tuple[int, str]]:
    """Verdict lines: who stood inside SMASH_INSIDE_YD at an impact, and who dodged and was brought back
    inside SMASH_CLEAR_YD by something else before it."""
    out = []
    for wave in smash_waves(trace):
        rows = wave_rows(trace, wave)
        inside = [row for row in rows if row["at_impact"] is not None and row["at_impact"] < SMASH_INSIDE_YD]
        if inside:
            out.append((wave["impact"], f"Cosmic Smash @ {secs(wave['impact'])}: inside {SMASH_INSIDE_YD:.0f} yd at"
                                        f" the impact: " + ", ".join(f"{trace.name(row['guid'])} {row['at_impact']:.1f}"
                                                                     for row in inside)))
        back = [row for row in rows
                if row["back"] and row["at_impact"] is not None and row["at_impact"] < SMASH_CLEAR_YD]
        if back:
            out.append((wave["impact"], f"Cosmic Smash @ {secs(wave['impact'])}: dodged, then brought back inside"
                                        f" {SMASH_CLEAR_YD:.0f} yd by the impact: " + ", ".join(
                                            f"{trace.name(row['guid'])} {row['at_impact']:.1f} ({row['back']})"
                                            for row in back)))
    return out


# --------------------------------------------------------------------------------------------- deaths

def killing_row(trace: Trace, death: dict) -> dict | None:
    """The blow that killed: the lethal `dmg` row just before the death, else the rewind's last."""
    rows = [rec for rec in victim_hits(trace).get(death["g"], [])
            if death["t"] - KILL_MATCH_MS <= rec["t"] <= death["t"]]
    lethal = [rec for rec in rows if rec.get("ok", 0) > 0]
    if lethal or rows:
        return (lethal or rows)[-1]
    rewind = death.get("rewind") or []
    if rewind and len(rewind[-1]) >= 4:
        when, source, spell, amount = rewind[-1][:4]
        return {"t": when, "s": source, "d": death["g"], "sp": spell, "a": amount}
    return None


def explosion_share(trace: Trace, death: dict, since: int) -> float:
    """Black Hole Explosion's share of the damage taken in the SHARE_MS before the death, this life only."""
    rows = [rec for rec in victim_hits(trace).get(death["g"], [])
            if max(since, death["t"] - SHARE_MS) < rec["t"] <= death["t"]]
    total = sum(rec.get("a", 0) for rec in rows)
    burst = sum(rec.get("a", 0) for rec in rows if rec.get("sp") in SPELL_BLACK_HOLE_EXPLOSION)
    return burst / total if total else 0.0


def algalon_death(trace: Trace, death: dict) -> tuple[str, str]:
    pair, _ = swap_pair(trace)
    if death["g"] in pair:
        return "Algalon on tank", ""
    window = next((window for window in loose_windows(trace)
                   if window["start"] <= death["t"] <= window["stop"] + SAMPLE_LAG_MS), None)
    intro = intro_end(trace)
    if (window and window["cause"] == "intro end") or (intro is not None and 0 <= death["t"] - intro <= INTRO_CAUSE_MS):
        return "Algalon melee (intro end)", ""
    return "Algalon melee (loose)", window_cause(window) if window else "no loose window"


def smash_band_at(trace: Trace, death: dict, when: int) -> str:
    if "x" in death and "y" in death:
        spot = (death["x"], death["y"])
    else:
        spot = position_at(samples_of(trace), death["g"], when)
    gaps = [gap for wave in smash_waves(trace) if abs(wave["impact"] - when) <= SMASH_MATCH_MS
            if (gap := to_meteor(wave, spot)) is not None]
    return f"{band(min(gaps))} yd" if gaps else ""


def classify_death(trace: Trace, death: dict, since: int) -> tuple[str, str]:
    """The mechanic that killed, and a detail. A star explosion owns any death it did half the last
    SHARE_MS of damage to, whatever landed the blow."""
    if death.get("cause") == "reset":
        return "wipe command", ""
    blow = killing_row(trace, death)
    if blow is None:
        return "other", death.get("cause") or "no blow"
    spell, source = blow.get("sp"), blow.get("s")
    share = explosion_share(trace, death, since)
    if spell in SPELL_BLACK_HOLE_EXPLOSION:
        return "star explosion", ""
    if share >= EXPLOSION_SHARE:
        return "star explosion", f"{share:.0%} of the last {SHARE_MS // 1000} s"
    if trace.entries.get(source) == NPC_DARK_MATTER:
        bang = any(cast["start"] <= death["t"] <= cast["end"] + MATTER_BANG_MS for cast in big_bang_casts(trace))
        return f"Dark Matter while phased ({'Big Bang' if bang else 'stray'})", ""
    if spell in SPELL_BIG_BANG:
        return "Big Bang", ""
    if source == boss_guid(trace):
        return algalon_death(trace, death)
    if spell in SPELL_COSMIC_SMASH:
        return "Cosmic Smash", smash_band_at(trace, death, blow["t"])
    if spell in SPELL_CONSTELLATION_BARRAGE:
        return "Constellation Barrage", ""
    if spell == SPELL_BLACK_HOLE_DOT:
        return "Black Hole DoT", ""
    return "other", f"{trace.name(source)} {trace.spell(spell)}"


def death_rows(trace: Trace) -> list[dict]:
    """`{t, guid, cause, detail}` per death, the wipe command's included."""
    def build(trace: Trace) -> list[dict]:
        out, lives = [], {}
        for rec in death_records(trace):
            cause, detail = classify_death(trace, rec, lives.get(rec["g"], -(1 << 62)))
            lives[rec["g"]] = rec["t"]
            out.append({"t": rec["t"], "guid": rec["g"], "cause": cause, "detail": detail})
        return out
    return cached(trace, "_algalon_deaths", build)


def show_deaths(trace: Trace) -> None:
    print("DEATHS")
    if boss_guid(trace) is None:
        print("  Algalon was never sampled")
        return
    rows = death_rows(trace)
    if not rows:
        print("  nobody died")
        return
    roles = list(DEATH_ROLES) + sorted({trace.role(row["guid"]) for row in rows} - set(DEATH_ROLES))
    count: collections.Counter = collections.Counter()
    humans: collections.Counter = collections.Counter()
    for row in rows:
        count[(row["cause"], trace.role(row["guid"]))] += 1
        humans[(row["cause"], trace.role(row["guid"]))] += row["guid"] in trace.humans
    causes = collections.Counter(row["cause"] for row in rows)
    print(f"  {'cause':38}" + "".join(f" {role:>7}" for role in roles) + f" {'total':>6}")
    for cause, total in causes.most_common():
        cells = [str(count[(cause, role)]) + "*" * humans[(cause, role)] if count[(cause, role)] else "-"
                 for role in roles]
        print(f"  {cause:38}" + "".join(f" {cell:>7}" for cell in cells) + f" {total:6}")
    print("  each * is a human among them")
    for row in rows:
        human = "*" if row["guid"] in trace.humans else ""
        print(f"  {clock(row['t']):>9}  {who(trace, row['guid']) + human:28} {row['cause']}"
              + (f": {row['detail']}" if row["detail"] else ""))


# ---------------------------------------------------------------------------------------------- tanks

def phase_punch(trace: Trace) -> dict[int, list[tuple[int, int, bool]]]:
    """Per target, `(t, stacks, removed)` for every Phase Punch row."""
    out: dict[int, list[tuple[int, int, bool]]] = collections.defaultdict(list)
    for rec in trace.of("aura"):
        if rec.get("sp") == SPELL_PHASE_PUNCH:
            out[rec.get("d", 0)].append((rec["t"], rec.get("st", 1) or 1, bool(rec.get("r"))))
    return out


def punched_out(trace: Trace) -> list[tuple[int, int]]:
    return [(rec["t"], rec.get("d", 0)) for rec in trace.of("aura")
            if rec.get("sp") == SPELL_PHASE_PUNCH_PHASE and not rec.get("r")]


def boss_victims(trace: Trace) -> list[tuple[int, int]]:
    boss = boss_guid(trace)
    return victim_changes(samples_of(trace), boss) if boss else []


def pickups(trace: Trace) -> list[tuple[int, int, int]]:
    """`(start, stop, guid)` for every stretch he spent on someone who is not a tank."""
    victims = boss_victims(trace)
    end = pull_end(trace)
    out = []
    for index, (when, victim) in enumerate(victims):
        stop = victims[index + 1][0] if index + 1 < len(victims) else end
        if victim and trace.role(victim) != "tank":
            out.append((when, stop, victim))
    return out


def show_tanks(trace: Trace) -> None:
    print("TANKS")
    boss = boss_guid(trace)
    if boss is None:
        print("  Algalon was never sampled")
        return

    stacks = phase_punch(trace)
    if stacks:
        print("  Phase Punch:")
        for guid, rows in sorted(stacks.items(), key=lambda item: trace.name(item[0])):
            steps = [f"{clock(when)} {'off' if removed else count}" for when, count, removed in rows]
            peak = max((count for _, count, removed in rows if not removed), default=0)
            print(f"    {who(trace, guid):26} max {peak}: {', '.join(steps[:TIMELINE_SHOWN])}"
                  + (f" and {len(steps) - TIMELINE_SHOWN} more" if len(steps) > TIMELINE_SHOWN else ""))
    else:
        print("  no Phase Punch rows")
    for when, guid in punched_out(trace):
        print(f"  5th stack: {who(trace, guid)} phased out at {clock(when)}")

    victims = boss_victims(trace)
    swaps = sum(1 for index in range(1, len(victims))
                if trace.role(victims[index][1]) == "tank" and trace.role(victims[index - 1][1]) == "tank")
    print(f"  his victim, {swaps} tank swap(s): " + ", ".join(f"{clock(when)} {trace.name(victim)}"
                                                           for when, victim in victims[:TIMELINE_SHOWN]))
    for start, stop, guid in pickups(trace):
        print(f"    held by a non-tank: {who(trace, guid)} {(stop - start) / 1000:.1f} s from {clock(start)}")

    strikes: dict[int, list[int]] = collections.defaultdict(list)
    for rec in trace.of("dmg"):
        if rec.get("sp") in SPELL_QUANTUM_STRIKE:
            strikes[rec.get("d", 0)].append(rec.get("a", 0))
    if strikes:
        print("  Quantum Strike: " + ", ".join(
            f"{who(trace, guid)} {len(hits)} x {sum(hits):,} (max {max(hits):,})"
            for guid, hits in sorted(strikes.items(), key=lambda item: -sum(item[1]))))

    reach = [(dist2(row[1:3], ROOM_CENTER), when) for when, row, _ in samples_of(trace).unit_steps(boss)]
    if reach:
        far, when = max(reach)
        print(f"  furthest from the room centre: {far:.1f} yd at {clock(when)} (room {ROOM_RADIUS:.0f} yd)")


# --------------------------------------------------------------------------------------- tank healers

def tank_healer_spans(trace: Trace) -> dict[int, list[tuple[int, int]]]:
    """Per bot on `algalon.tankhealer`, the `(start, stop)` stretches it held the pair."""
    def build(trace: Trace) -> dict[int, list[tuple[int, int]]]:
        out = {}
        for guid, spans in holder_spans(trace, "algalon.tankhealer", pull_end(trace)).items():
            held = [(start, stop) for value, start, stop in spans if value == "1"]
            if held:
                out[guid] = held
        return out
    return cached(trace, "_algalon_tank_healers", build)


def assigned_at(spans, guid: int, when: int) -> bool:
    return any(start <= when < stop for start, stop in spans.get(guid, ()))


def effective(rec: dict) -> int:
    """A `heal` row's `a` is gross; overheal is in `oh`."""
    return rec.get("a", 0) - rec.get("oh", 0)


def roster_healers(trace: Trace) -> list[int]:
    return [guid for guid in roster_guids(trace) if trace.role(guid) == "heal"]


def healer_shares(trace: Trace) -> list[dict]:
    """Per healer, effective healing plus shields on the swap pair, on itself and on everyone else: over
    the whole pull, and over the stretches it was a tank healer."""
    pair, _ = swap_pair(trace)
    spans = tank_healer_spans(trace)
    healers = roster_healers(trace) + sorted(set(spans) - set(roster_healers(trace)))
    totals = {guid: {"all": collections.Counter(), "assigned": collections.Counter()} for guid in healers}
    rows = [(rec["t"], rec.get("s", 0), rec.get("d", 0), effective(rec)) for rec in trace.of("heal")]
    rows += [(rec["t"], rec.get("s", 0), rec.get("d", 0), rec.get("a", 0)) for rec in trace.of("abs")]
    for when, source, target, amount in rows:
        if source not in totals or amount <= 0:
            continue
        bucket = "self" if target == source else "tanks" if target in pair else "others"
        totals[source]["all"][bucket] += amount
        if assigned_at(spans, source, when):
            totals[source]["assigned"][bucket] += amount
    return [{"guid": guid, **totals[guid]} for guid in healers]


def share_of(counts: collections.Counter, bucket: str) -> float | None:
    total = sum(counts.values())
    return counts[bucket] / total if total else None


def tank_death_rows(trace: Trace) -> list[dict]:
    """Per swap tank his melee killed: what it took and got in the TANKHEAL_WINDOW_MS before, what the
    healers put into everyone else meanwhile, and where each tank healer was."""
    def build(trace: Trace) -> list[dict]:
        pair, _ = swap_pair(trace)
        spans = tank_healer_spans(trace)
        healers = set(roster_healers(trace)) | set(spans)
        samples = samples_of(trace)
        phased = all_phases(trace)
        branches = holder_spans(trace, "algalon.tankheal")
        vetoes = [rec for rec in trace.of("veto") if rec.get("m") == TANKHEAL_VETO]
        out = []
        for row in death_rows(trace):
            guid, when = row["guid"], row["t"]
            if guid not in pair or row["cause"] != "Algalon on tank":
                continue
            low = when - TANKHEAL_WINDOW_MS
            taken = [rec for rec in victim_hits(trace).get(guid, []) if low <= rec["t"] <= when]
            heals = [rec for rec in trace.of("heal") if low <= rec["t"] <= when]
            on_tank = [rec for rec in heals if rec.get("d") == guid]
            tank_spot = position_at(samples, guid, when)
            states = []
            for healer in sorted(spans):
                if not any(start <= when and stop >= low for start, stop in spans[healer]):
                    continue
                spot = position_at(samples, healer, when)
                states.append({
                    "guid": healer,
                    "alive": alive(samples.row(healer, when)),
                    "phased": phased_at(phased, healer, when),
                    "distance": dist2(spot, tank_spot) if spot and tank_spot else None,
                    "branch": value_at(branches.get(healer, []), when),
                    "vetoes": sum(1 for rec in vetoes if rec.get("g") == healer and low <= rec["t"] <= when),
                    "healed": sum(effective(rec) for rec in on_tank if rec.get("s") == healer),
                })
            up = [healer for healer in roster_healers(trace)
                  if alive(samples.row(healer, when)) and not phased_at(phased, healer, when)]
            out.append({
                "t": when, "guid": guid,
                "taken": sum(rec.get("a", 0) + rec.get("ab", 0) for rec in taken),
                "burst": sum(rec.get("a", 0) + rec.get("ab", 0) for rec in taken
                             if rec["t"] >= when - TANKHEAL_BURST_MS),
                "healed": sum(effective(rec) for rec in on_tank),
                "by_tank_healers": sum(effective(rec) for rec in on_tank
                                       if assigned_at(spans, rec.get("s", 0), rec["t"])),
                "absorbed": sum(rec.get("a", 0) for rec in trace.of("abs")
                                if rec.get("d") == guid and low <= rec["t"] <= when),
                "elsewhere": sum(effective(rec) for rec in heals
                                 if rec.get("s") in healers and rec.get("d") != guid),
                "healers": states,
                "up": len(up),
            })
        return out
    return cached(trace, "_algalon_tank_deaths", build)


def beacon_seconds(trace: Trace) -> tuple[int, int]:
    """Milliseconds Beacon of Light sat on the tank he was hitting and on any other swap tank."""
    pair, _ = swap_pair(trace)
    victims = victim_samples(trace)
    stamps = [when for when, _ in victims]
    end = pull_end(trace)
    opened: dict[int, int] = {}
    spans = []
    for rec in sorted(trace.of("aura"), key=lambda rec: rec["t"]):
        if rec.get("sp") != SPELL_BEACON_OF_LIGHT or rec.get("d") not in pair:
            continue
        target = rec["d"]
        if rec.get("r"):
            if target in opened:
                spans.append((target, opened.pop(target), rec["t"]))
        else:
            opened.setdefault(target, rec["t"])
    spans += [(target, start, end) for target, start in opened.items()]
    holder = other = 0
    for target, start, stop in spans:
        for when in range(start, stop, 500):
            index = bisect.bisect_right(stamps, when)
            victim = victims[index - 1][1] if index else 0
            if victim == target:
                holder += 500
            else:
                other += 500
    return holder, other


def tankheal_summary(trace: Trace) -> str | None:
    rows = tank_death_rows(trace)
    if not rows:
        return None

    def k(values) -> str:
        return f"{statistics.median(values) / 1000:.0f}k"

    healers = roster_healers(trace)
    shares = [share for row in healer_shares(trace) if row["guid"] in healers
              if (share := share_of(row["all"], "tanks")) is not None]
    spread = f"; healers' share on the pair {min(shares):.0%}-{max(shares):.0%}" if shares else ""
    return (f"{len(rows)} tank death{'s' if len(rows) != 1 else ''}; medians over the"
            f" {TANKHEAL_WINDOW_MS // 1000} s before: took {k(r['taken'] for r in rows)}"
            f" ({k(r['burst'] for r in rows)} in {TANKHEAL_BURST_MS // 1000} s), healed {k(r['healed'] for r in rows)}"
            f" + {k(r['absorbed'] for r in rows)} shields, {k(r['elsewhere'] for r in rows)} into others;"
            f" every healer alive and unphased in {sum(r['up'] == len(healers) for r in rows)} of {len(rows)}"
            + spread)


def show_tankheal(trace: Trace) -> None:
    print("TANK HEALERS")
    if boss_guid(trace) is None:
        print("  Algalon was never sampled")
        return
    spans = tank_healer_spans(trace)
    show_absent(trace, "algalon.tankhealer", "algalon.tankheal")
    for guid, held in sorted(spans.items(), key=lambda item: trace.name(item[0])):
        stretches = ", ".join(f"{clock(start)}-{clock(stop)}" for start, stop in held)
        print(f"  tank healer: {label_of(trace, guid)} {stretches}")

    print("  effective healing plus shields, on the swap pair / itself / everyone else:")
    for row in healer_shares(trace):
        total = sum(row["all"].values())
        if not total:
            continue
        cells = " ".join(f"{bucket} {share_of(row['all'], bucket):.0%}" for bucket in ("tanks", "self", "others"))
        line = f"    {label_of(trace, row['guid']):28} {total:>10,}  {cells}"
        if sum(row["assigned"].values()):
            line += "  assigned: " + " ".join(f"{bucket} {share_of(row['assigned'], bucket):.0%}"
                                              for bucket in ("tanks", "self", "others"))
        print(line)

    holder, other = beacon_seconds(trace)
    if holder or other:
        print(f"  Beacon of Light: {holder / 1000:.1f} s on the tank he was hitting, {other / 1000:.1f} s on the other")

    rows = tank_death_rows(trace)
    print(f"  swap tanks his melee killed: {len(rows)}")
    for row in rows:
        print(f"    {clock(row['t']):>9} {who(trace, row['guid'])}: took {row['taken']:,} in"
              f" {TANKHEAL_WINDOW_MS // 1000} s ({row['burst']:,} in {TANKHEAL_BURST_MS // 1000} s), healed"
              f" {row['healed']:,} ({row['by_tank_healers']:,} by tank healers), shields {row['absorbed']:,};"
              f" healers into others {row['elsewhere']:,}")
        for state in row["healers"]:
            where = "dead" if not state["alive"] else "phased" if state["phased"] else (
                f"{state['distance']:.1f} yd" if state["distance"] is not None else "unsampled")
            print(f"      {label_of(trace, state['guid']):28} {where}, {state['branch'] or '-'},"
                  f" healed it {state['healed']:,}, {state['vetoes']} veto rows")
    summary = tankheal_summary(trace)
    if summary:
        print(f"  {summary}")


def tankheal_faults(trace: Trace) -> list[tuple[int, str]]:
    out = []
    spans = tank_healer_spans(trace)
    for row in tank_death_rows(trace):
        for state in row["healers"]:
            if (state["alive"] and not state["phased"] and state["distance"] is not None
                    and state["distance"] <= TANKHEAL_RANGE_YD and assigned_at(spans, state["guid"], row["t"])):
                out.append((row["t"], f"{trace.name(row['guid'])} died @ {secs(row['t'])} with tank healer"
                                      f" {trace.name(state['guid'])} alive, unphased and {state['distance']:.0f} yd"
                                      f" away: healed {row['healed']:,} of {row['taken']:,} taken in"
                                      f" {TANKHEAL_WINDOW_MS // 1000} s"))
    for row in healer_shares(trace):
        others = share_of(row["assigned"], "others")
        if row["guid"] in spans and others is not None and others > TANKHEAL_OTHERS_SHARE:
            start = spans[row["guid"]][0][0]
            out.append((start, f"tank healer {trace.name(row['guid'])} put {others:.0%} of its healing on others"))
    for guid, held in spans.items():
        if guid in trace.humans:
            out.append((held[0][0], f"human {trace.name(guid)} assigned as a tank healer @ {secs(held[0][0])}"))
    cap = len(roster_healers(trace)) // 2
    edges = sorted([(start, 1) for held in spans.values() for start, _ in held]
                   + [(stop, -1) for held in spans.values() for _, stop in held])
    count = 0
    for when, step in edges:
        count += step
        if count > cap:
            out.append((when, f"{count} tank healers @ {secs(when)} with {len(roster_healers(trace))} healers"
                              f" in the raid"))
            break
    return out


# ---------------------------------------------------------------------------------------- defensives

def aura_spans(trace: Trace, spells) -> dict[int, list[tuple[int, int, int, int]]]:
    """Per unit, `(start, stop, spell, caster)` per stretch it wore one of `spells`. A row while the aura
    is held is a refresh; one still on when the file ends runs to the end."""
    end = pull_end(trace)
    wearing: dict[tuple[int, int], tuple[int, int]] = {}
    out: dict[int, list[tuple[int, int, int, int]]] = collections.defaultdict(list)
    for rec in trace.of("aura"):
        spell = rec.get("sp")
        if spell not in spells:
            continue
        key = (rec.get("d", 0), spell)
        if not rec.get("r"):
            wearing.setdefault(key, (rec["t"], rec.get("s", 0)))
        elif key in wearing:
            start, caster = wearing.pop(key)
            out[key[0]].append((start, rec["t"], spell, caster))
    for (guid, spell), (start, caster) in wearing.items():
        out[guid].append((start, end, spell, caster))
    return {guid: sorted(spans) for guid, spans in out.items()}


def wearing_at(spans, guid: int, when: int, slack: int = 0) -> list[tuple[int, int, int, int]]:
    """The `aura_spans` stretches on `guid` that cover `when`, or came off at most `slack` before it."""
    return [span for span in spans.get(guid, ()) if span[0] <= when < span[1] + slack]


def victim_at(trace: Trace, when: int) -> int:
    victims = victim_samples(trace)
    index = bisect.bisect_right(victims, (when, 1 << 62))
    return victims[index - 1][1] if index else 0


def parse_tankdef(text) -> tuple[str, float | None, int, str]:
    """`algalon.tankdef` reads `<why> <tank health %> <tank guid> <spell name>`, the name last since it can
    hold spaces."""
    why, pct, guid, name = (str(text or "").strip().split(" ", 3) + ["", "", "", ""])[:4]
    try:
        health = float(pct)
    except ValueError:
        health = None
    return why, health, as_guid(guid), name


def tankdef_notes(trace: Trace) -> list[dict]:
    def build(trace: Trace) -> list[dict]:
        out = []
        for rec in notes(trace, "algalon.tankdef"):
            why, pct, target, name = parse_tankdef(rec.get("txt"))
            out.append({"t": rec["t"], "caster": rec.get("g", 0), "why": why, "pct": pct, "target": target,
                        "name": name})
        return out
    return cached(trace, "_algalon_tankdef", build)


def tank_saves(trace: Trace) -> list[dict]:
    """Per damage cutter or Lay on Hands landing on a swap tank: its health then, its death within SAVE_LIVED_MS and
    the `algalon.tankdef` reason. A tank button goes on its caster only; a triggered row is an echo."""
    def build(trace: Trace) -> list[dict]:
        pair, _ = swap_pair(trace)
        samples = samples_of(trace)
        deaths = combat_deaths(trace)
        picks = tankdef_notes(trace)
        out = []
        last: dict[tuple, int] = {}
        for rec in trace.of("cast"):
            spell, caster, when = rec.get("sp"), rec.get("s"), rec["t"]
            if spell not in TANK_SAVES or rec.get("tr"):
                continue
            target = caster if spell in TANK_BUTTONS else rec.get("tgt") or caster
            if target not in pair or when - last.get((caster, spell), -(1 << 62)) <= ECHO_MS:
                continue
            last[(caster, spell)] = when
            row = samples.row(target, when)
            pick = next((note for note in picks if note["caster"] == caster and note["target"] == target
                         and abs(note["t"] - when) <= ECHO_MS), None)
            out.append({
                "t": when, "caster": caster, "target": target, "spell": spell,
                "name": spell_name(trace, spell, TANK_SAVES),
                "hp": row[5] if alive(row) else None,
                "died": next((death["t"] for death in deaths
                              if death.get("g") == target and when < death["t"] <= when + SAVE_LIVED_MS), None),
                "why": pick["why"] if pick else None,
            })
        return out
    return cached(trace, "_algalon_saves", build)


def tank_deaths(trace: Trace) -> list[dict]:
    """Per swap tank death the wipe command didn't cause: its last TANKHEAL_WINDOW_MS of hits, the cutters it wore,
    the saves cast on it then, and the `algalon.tankdef` notes naming it in the TANKDEF_NOTE_MS before."""
    def build(trace: Trace) -> list[dict]:
        pair, _ = swap_pair(trace)
        worn = aura_spans(trace, CUTTER_AURAS)
        phased = all_phases(trace)
        out = []
        for row in death_rows(trace):
            guid, when = row["guid"], row["t"]
            if guid not in pair or row["cause"] == "wipe command":
                continue
            low = when - TANKHEAL_WINDOW_MS
            out.append({
                "t": when, "guid": guid, "cause": row["cause"], "phased": phased_at(phased, guid, when),
                "hits": [rec for rec in victim_hits(trace).get(guid, []) if low <= rec["t"] <= when],
                "up": wearing_at(worn, guid, when, STRIP_AT_DEATH_MS),
                "cast": [save for save in tank_saves(trace) if save["target"] == guid and low <= save["t"] <= when],
                "notes": [note for note in tankdef_notes(trace)
                          if note["target"] == guid and when - TANKDEF_NOTE_MS <= note["t"] <= when],
            })
        return out
    return cached(trace, "_algalon_tank_cuts", build)


def cover_providers(trace: Trace) -> dict[int, list[int]]:
    """Per bot able to cover an explosion, its cover spells: Aura Mastery for the `algalon.shadowaura` paladin or
    anyone seen casting it, Divine Sacrifice for anyone whose cast put up Divine Guardian."""
    def build(trace: Trace) -> dict[int, list[int]]:
        bots = {guid for guid in roster_guids(trace) if guid not in trace.humans}
        mastery = {as_guid(value) for value, _, _ in latch_spans(trace, "algalon.shadowaura")}
        mastery |= {rec.get("s") for rec in trace.of("cast") if rec.get("sp") == SPELL_AURA_MASTERY}
        guardian = {rec.get("s") for rec in trace.of("aura") if rec.get("sp") == SPELL_DIVINE_GUARDIAN}
        out: dict[int, list[int]] = collections.defaultdict(list)
        for spell, casters in ((SPELL_AURA_MASTERY, mastery), (SPELL_DIVINE_SACRIFICE, guardian)):
            for guid in sorted(casters & bots):
                out[guid].append(spell)
        return dict(out)
    return cached(trace, "_algalon_cover_providers", build)


def cover_ready(trace: Trace, when: int) -> list[tuple[int, int]]:
    """`(bot, spell)` per cover provider that could have covered an explosion at `when`: alive, unphased,
    and that spell not cast in the COVER_COOLDOWN_MS before."""
    casts = cached(trace, "_algalon_cover_casts", lambda trace: [
        (rec["t"], rec.get("s"), rec.get("sp")) for rec in trace.of("cast") if rec.get("sp") in COVER_SPELLS])
    samples = samples_of(trace)
    phased = all_phases(trace)
    out = []
    for guid, spells in cover_providers(trace).items():
        if not alive(samples.row(guid, when - 1)) or phased_at(phased, guid, when):
            continue
        out += [(guid, spell) for spell in spells
                if not any(caster == guid and cast == spell and when - COVER_COOLDOWN_MS < at <= when
                           for at, caster, cast in casts)]
    return out


def explosion_rows(trace: Trace) -> list[dict]:
    """Per star explosion: hits, cover worn, `algalon.cover` notes, ready providers when none was up, and `under`: each
    unphased bot outside the pair at or under its line before the hit (top roll, times COVERED_FACTOR when covered)."""
    def build(trace: Trace) -> list[dict]:
        roster = roster_guids(trace)
        pair, _ = swap_pair(trace)
        samples = samples_of(trace)
        phased = all_phases(trace)
        safe = explosion_safe_hp(trace)
        deaths = death_records(trace)
        guardian = aura_spans(trace, (SPELL_DIVINE_GUARDIAN,))
        mastery = aura_spans(trace, (SPELL_AURA_MASTERY,))
        covers = notes(trace, "algalon.cover")
        out = []
        for star in star_rows(trace):
            if star["how"] != "explosion":
                continue
            when = star["t"]
            row = {
                "t": when, "bang": star["bang"], "hits": star["rows"],
                "guardians": sorted(guid for guid in guardian if guid in roster and wearing_at(guardian, guid, when)),
                "mastery": sorted(guid for guid in mastery if wearing_at(mastery, guid, when)),
                "notes": [(rec["t"], rec.get("g", 0), str(rec.get("txt", ""))) for rec in covers
                          if when - COVER_MS <= rec["t"] <= when],
            }
            row["covered"] = bool(row["guardians"] or row["mastery"])
            row["ready"] = [] if row["covered"] else cover_ready(trace, when)
            row["under"] = []
            for guid, unit in samples.frame(when - 1).items():
                if (guid not in roster or guid in trace.humans or guid in pair or not alive(unit)
                        or phased_at(phased, guid, when) or (hp := health_of(trace, guid, unit)) is None):
                    continue
                covered = row["mastery"] or wearing_at(guardian, guid, when, STRIP_AT_DEATH_MS)
                line = round(safe * (COVERED_FACTOR if covered else 1.0))
                if hp > line:
                    continue
                hits = [rec for rec in star["rows"] if rec.get("d") == guid]
                used = presses(trace, guid, when - PERSONAL_LOOK_MS, when)
                row["under"].append({
                    "guid": guid, "hp": hp, "line": line,
                    "landed": sum(rec.get("a", 0) for rec in hits) if hits else None,
                    "absorbed": sum(rec.get("ab", 0) for rec in hits),
                    "died": any(rec.get("ok", 0) > 0 for rec in hits) or bool(star["source"]) and any(
                        rec.get("g") == guid and rec.get("killer") == star["source"]
                        and when <= rec["t"] <= star["last"] + KILL_MATCH_MS for rec in deaths),
                    "pressed": [(at, name) for at, name in used if press_kind(name) != "consumable"],
                    "consumables": [(at, name) for at, name in used if press_kind(name) == "consumable"],
                    "buttons": button_states(trace, guid, when),
                })
            row["under"].sort(key=lambda entry: trace.name(entry["guid"]))
            out.append(row)
        return out
    return cached(trace, "_algalon_explosions", build)


def hit_means(hits: list[dict]) -> tuple[float | None, float | None]:
    """Mean damage per hit, absorbs included, and mean resisted, None where no hit carries `rs`."""
    if not hits:
        return None, None
    resisted = [rec["rs"] for rec in hits if "rs" in rec]
    return (statistics.fmean(rec.get("a", 0) + rec.get("ab", 0) for rec in hits),
            statistics.fmean(resisted) if resisted else None)


def held_sacrifices(trace: Trace) -> list[dict]:
    """`{t, caster, held, bots}` per bot's Divine Sacrifice that stayed on bots past SACRIFICE_HELD_MS, held
    being the longest. The tick cancels only a bot's own; a human's runs its course."""
    spans = sorted((start, caster, stop - start, guid)
                   for guid, items in aura_spans(trace, (SPELL_DIVINE_SACRIFICE,)).items() if guid not in trace.humans
                   for start, stop, _, caster in items
                   if caster not in trace.humans and stop - start > SACRIFICE_HELD_MS)
    out: list[dict] = []
    for start, caster, held, guid in spans:
        group = next((group for group in reversed(out)
                      if group["caster"] == caster and start - group["t"] <= ECHO_MS), None)
        if group is None:
            group = {"t": start, "caster": caster, "held": 0, "bots": []}
            out.append(group)
        group["held"] = max(group["held"], held)
        group["bots"].append(guid)
    return out


def personal_name(trace: Trace, spell) -> str:
    """The personal button a spell id is, in the table's lower-case spelling, else empty."""
    name = spell_name(trace, spell, PERSONAL_IDS).lower().replace("-", " ")
    return name if name in PERSONAL_COOLDOWN_MS else ""


def personal_casts(trace: Trace) -> dict[int, list[tuple[int, str, str]]]:
    """Per bot, `(t, button, spell name)` for each personal button it cast, the button in lower case."""
    def build(trace: Trace) -> dict[int, list[tuple[int, str, str]]]:
        out: dict[int, list[tuple[int, str, str]]] = collections.defaultdict(list)
        for rec in trace.of("cast"):
            guid = rec.get("s")
            if guid in trace.roles and guid not in trace.humans and not rec.get("tr") and (
                    name := personal_name(trace, rec.get("sp"))):
                out[guid].append((rec["t"], name, spell_name(trace, rec.get("sp"), PERSONAL_IDS)))
        return dict(out)
    return cached(trace, "_algalon_personal_casts", build)


def consumable_uses(trace: Trace) -> dict[int, list[tuple[int, str]]]:
    """Per bot, `(t, spell name)` for each healthstone, healing potion or injector it used."""
    def build(trace: Trace) -> dict[int, list[tuple[int, str]]]:
        out: dict[int, list[tuple[int, str]]] = collections.defaultdict(list)
        for rec in trace.of("cast"):
            guid, name = rec.get("s"), spell_name(trace, rec.get("sp"), CONSUMABLE_IDS)
            if guid in trace.roles and guid not in trace.humans and not rec.get("tr") and CONSUMABLE.search(name):
                out[guid].append((rec["t"], name))
        return dict(out)
    return cached(trace, "_algalon_consumables", build)


def press_kind(text: str) -> str:
    """The personal button a name is, in the table's lower-case spelling, `consumable` for a healthstone or
    potion, else the name itself."""
    name = text.lower().replace("-", " ")
    return name if name in PERSONAL_COOLDOWN_MS else "consumable" if CONSUMABLE.search(name) else name


def personal_notes(trace: Trace) -> dict[int, list[dict]]:
    def build(trace: Trace) -> dict[int, list[dict]]:
        out: dict[int, list[dict]] = collections.defaultdict(list)
        for rec in notes(trace, "algalon.personal"):
            out[rec.get("g", 0)].append(rec)
        return dict(out)
    return cached(trace, "_algalon_personal_notes", build)


def presses(trace: Trace, guid: int, low: int, high: int) -> list[tuple[int, str]]:
    """`(t, name)` per personal button, healthstone or potion `guid` used in `[low, high]`: its
    `algalon.personal` rows, then the cast rows with no row of the same kind within ECHO_MS."""
    used = [(rec["t"], str(rec.get("txt", ""))) for rec in personal_notes(trace).get(guid, ())
            if low <= rec["t"] <= high]
    casts = [(at, label) for at, _, label in personal_casts(trace).get(guid, ())] + consumable_uses(trace).get(guid, [])
    for at, label in sorted(casts):
        if low <= at <= high and not any(abs(at - seen) <= ECHO_MS and press_kind(name) == press_kind(label)
                                         for seen, name in used):
            used.append((at, label))
    return sorted(used)


def class_buttons(trace: Trace, guid: int) -> tuple[str, ...]:
    found = class_of(trace, guid)
    return CLASS_BUTTONS.get(CLASS_IDS.get(found.lower()) if isinstance(found, str) else found, ())


def button_states(trace: Trace, guid: int, when: int) -> list[tuple[str, str, int | None]]:
    """`(button, state, since)` per personal button of `guid` at `when`, its class's and any seen cast: `cooling`,
    `forbearance`, `held` (immunity within IMMUNITY_HOLD_MS of a Big Bang, the disperser's Dispersion) or `ready`."""
    casts = personal_casts(trace).get(guid, [])
    forbearance = cached(trace, "_algalon_forbearance", lambda trace: aura_spans(trace, (SPELL_FORBEARANCE,)))
    dispersers = {as_guid(value) for value, _, _ in
                  cached(trace, "_algalon_dispersers", lambda trace: latch_spans(trace, "algalon.disperser"))}
    starts = cached(trace, "_algalon_bang_starts", lambda trace: [cast["start"] for cast in big_bang_casts(trace)])
    bang_due = any(0 <= start - when <= IMMUNITY_HOLD_MS for start in starts)
    labels = {name: label for _, name, label in casts}
    out = []
    for name in sorted(set(class_buttons(trace, guid)) | set(labels)):
        last = max((at for at, seen, _ in casts if seen == name and at <= when), default=None)
        # slack: a death strips it at the death's own stamp
        worn = wearing_at(forbearance, guid, when, STRIP_AT_DEATH_MS) if name in FORBEARANCE_BUTTONS else []
        if last is not None and when - last < PERSONAL_COOLDOWN_MS[name]:
            state, since = "cooling", last
        elif worn:
            state, since = "forbearance", worn[0][0]
        elif (bang_due and name in PERSONAL_IMMUNE) or (name == "dispersion" and guid in dispersers):
            state, since = "held", None
        else:
            state, since = "ready", None
        out.append((labels.get(name) or PERSONAL_LABELS.get(name, name), state, since))
    return out


def button_label(button: tuple[str, str, int | None]) -> str:
    label, state, since = button
    return f"{label} {state}" + (f" since {clock(since)}" if since is not None else "")


def explosion_deaths(trace: Trace) -> list[dict]:
    """Per death a star explosion owns: `used`, its `presses` in the PERSONAL_LOOK_MS before, and `ready`, its ready
    `button_states` at the death when a bot outside the pair pressed nothing."""
    def build(trace: Trace) -> list[dict]:
        pair, _ = swap_pair(trace)
        out = []
        for row in death_rows(trace):
            if row["cause"] != "star explosion":
                continue
            guid, when = row["guid"], row["t"]
            used = presses(trace, guid, when - PERSONAL_LOOK_MS, when)
            ready = []
            if guid not in trace.humans and guid not in pair and not used:
                ready = [label for label, state, _ in button_states(trace, guid, when) if state == "ready"]
            out.append({"t": when, "guid": guid, "used": used, "ready": ready})
        return out
    return cached(trace, "_algalon_explosion_deaths", build)


def press_star(trace: Trace, when: int) -> dict | None:
    """The `star_rows` death a press or cover at `when` was for: the first within PRESS_AHEAD_MS after it,
    else the last within PRESS_STALE_MS before it, which had exploded already."""
    deaths = star_rows(trace)
    ahead = next((row for row in deaths if when < row["t"] <= when + PRESS_AHEAD_MS), None)
    return ahead or next((row for row in reversed(deaths) if when - PRESS_STALE_MS <= row["t"] <= when), None)


def press_rows(trace: Trace) -> list[dict]:
    """`{t, guid, kind, text, died, phased}` per `algalon.personal` (kind `personal`) and `algalon.cover` (kind
    `cover`) row, `died` the death of the `press_star` it was for, or None."""
    def build(trace: Trace) -> list[dict]:
        phased = hole_phases(trace)
        out = []
        for kind, key in (("personal", "algalon.personal"), ("cover", "algalon.cover")):
            for rec in notes(trace, key):
                star, guid = press_star(trace, rec["t"]), rec.get("g", 0)
                out.append({"t": rec["t"], "guid": guid, "kind": kind, "text": str(rec.get("txt", "")),
                            "died": star["t"] if star else None, "phased": phased_at(phased, guid, rec["t"])})
        return sorted(out, key=lambda row: row["t"])
    return cached(trace, "_algalon_presses", build)


def press_name(row: dict) -> str:
    if row["kind"] == "cover":
        return next((name for name in COVER_SPELLS.values() if name.lower() == row["text"].lower()), row["text"])
    return row["text"]


def failed_covers(trace: Trace) -> list[dict]:
    """`{t, guid, ready}` per FAILED run of the cover node, `ready` the other providers' `cover_ready` then."""
    def build(trace: Trace) -> list[dict]:
        return [{"t": rec["t"], "guid": rec.get("g", 0),
                 "ready": [(guid, spell) for guid, spell in cover_ready(trace, rec["t"]) if guid != rec.get("g")]}
                for rec in trace.of("act") if rec.get("a") == COVER_NODE and rec.get("vd") == "FAILED"]
    return cached(trace, "_algalon_failed_covers", build)


def spent_buttons(trace: Trace) -> list[dict]:
    """`{t, guid, label, pct, where}` per BUTTONS_HELD button a class node cast above BUTTON_HOLD_PCT, which the
    explosion node should have had: inside a hole's phase (`phased`, where no explosion reaches), or in P1 for a bot
    outside the swap pair (`P1`). A cast within ECHO_MS of the bot's `algalon.personal` row is the node's own."""
    def build(trace: Trace) -> list[dict]:
        pair, _ = swap_pair(trace)
        samples = samples_of(trace)
        phased = hole_phases(trace)
        spans = phases(trace)[0]
        pressed = personal_notes(trace)
        out = []
        for guid, casts in personal_casts(trace).items():
            for at, name, label in casts:
                row = samples.row(guid, at)
                if (name not in BUTTONS_HELD or not alive(row) or row[5] <= BUTTON_HOLD_PCT
                        or any(abs(rec["t"] - at) <= ECHO_MS for rec in pressed.get(guid, ()))):
                    continue
                if phased_at(phased, guid, at):
                    where = "phased"
                elif guid not in pair and phase_at(spans, at) == "P1":
                    where = "P1"
                else:
                    continue
                out.append({"t": at, "guid": guid, "label": label, "pct": row[5], "where": where})
        return sorted(out, key=lambda row: row["t"])
    return cached(trace, "_algalon_spent_buttons", build)


def thin_covers(trace: Trace) -> list[dict]:
    """`{t, guid, cover, under}` per `algalon.cover` cast with fewer than COVER_MIN_UNDER living, unphased raid
    members below the explosion's line."""
    def build(trace: Trace) -> list[dict]:
        samples = samples_of(trace)
        roster = roster_guids(trace)
        phased = all_phases(trace)
        out = []
        for rec in notes(trace, "algalon.cover"):
            when = rec["t"]
            up = {guid: row for guid, row in samples.frame(when - 1).items()
                  if guid in roster and alive(row) and not phased_at(phased, guid, when)}
            under = len(below_safe(trace, up))
            if under < COVER_MIN_UNDER:
                out.append({"t": when, "guid": rec.get("g", 0), "cover": str(rec.get("txt", "")), "under": under})
        return out
    return cached(trace, "_algalon_thin_covers", build)


def window_cuts(trace: Trace) -> list[dict]:
    """The `algalon.tankdef` `window` notes written for a holder at WINDOW_CUT_PCT or more: a cutter spent
    when the tank was nowhere near dying."""
    return [note for note in tankdef_notes(trace)
            if note["why"] == "window" and note["pct"] is not None and note["pct"] >= WINDOW_CUT_PCT]


def tankdef_label(trace: Trace, note: dict) -> str:
    health = "-" if note["pct"] is None else f"{note['pct']:.0f}%"
    return f"{clock(note['t'])} {trace.name(note['caster'])} {note['why']} {health} {note['name']}"


def cover_label(trace: Trace, ready: list[tuple[int, int]]) -> str:
    return ", ".join(f"{trace.name(guid)}'s {COVER_SPELLS[spell]}" for guid, spell in ready)


def show_defensives(trace: Trace) -> None:
    print("TANK DEFENSIVES AND EXPLOSION COVER")
    if boss_guid(trace) is None:
        print("  Algalon was never sampled")
        return
    show_absent(trace, "algalon.tankdef", "algalon.cover", "algalon.shadowaura")

    saves = tank_saves(trace)
    print(f"  damage cutters and Lay on Hands on a swap tank: {len(saves)}")
    for save in saves:
        health = "-" if save["hp"] is None else f"{save['hp']:.0f}%"
        fate = (f"died +{(save['died'] - save['t']) / 1000:.1f} s" if save["died"] is not None
                else f"lived {SAVE_LIVED_MS // 1000} s")
        why = f", algalon.tankdef {save['why']}" if save["why"] else ""
        print(f"    {clock(save['t']):>9} {trace.name(save['caster'])} {save['name']} -> {trace.name(save['target'])}"
              f" at {health}, {fate}{why}")

    deaths = tank_deaths(trace)
    print(f"  swap tank deaths: {len(deaths)}")
    for death in deaths:
        up = ", ".join(spell_name(trace, spell, CUTTER_AURAS) for _, _, spell, _ in death["up"]) or "none"
        cast = ", ".join(f"{clock(save['t'])} {trace.name(save['caster'])} {save['name']}"
                         for save in death["cast"]) or "none"
        print(f"    {clock(death['t']):>9} {label_of(trace, death['guid'])}, {death['cause']}"
              f"{', phased' if death['phased'] else ''}")
        print(f"      cutters up: {up}; cast on it in the {TANKHEAL_WINDOW_MS // 1000} s before: {cast}")
        for rec in death["hits"]:
            spell = trace.spells.get(rec.get("sp")) or trace.spell(rec.get("sp"))
            absorbed = f", {rec['ab']:,} absorbed" if rec.get("ab") else ""
            print(f"      {(rec['t'] - death['t']) / 1000:+5.1f} s  {spell:22} {rec.get('a', 0):>7,}{absorbed}"
                  f" -> {rec.get('hp', 0):.0f}%")
        print(f"      algalon.tankdef in the {TANKDEF_NOTE_MS // 1000} s before: "
              + ("; ".join(tankdef_label(trace, note) for note in death["notes"]) or "none"))

    providers = cover_providers(trace)
    print("  cover providers: " + ("; ".join(
        f"{trace.name(guid)} {', '.join(COVER_SPELLS[spell] for spell in spells)}"
        for guid, spells in sorted(providers.items(), key=lambda item: trace.name(item[0]))) or "none seen"))
    explosions = explosion_rows(trace)
    safe = explosion_safe_hp(trace)
    print(f"  star explosions: {len(explosions)}; under each, every unphased bot at or under its line ({safe:,},"
          f" {round(safe * COVERED_FACTOR):,} covered) before it: health, the hit, presses, buttons")
    for row in explosions:
        landed, resisted = hit_means(row["hits"])
        cover = []
        if row["guardians"]:
            cover.append(f"Divine Guardian on {len(row['guardians'])}")
        if row["mastery"]:
            cover.append("Aura Mastery on " + ", ".join(trace.name(guid) for guid in row["mastery"]))
        parts = [", ".join(cover) or "no cover"]
        parts += [f"algalon.cover {clock(when)} {text} ({trace.name(guid)})" for when, guid, text in row["notes"]]
        if row["ready"]:
            parts.append(f"ready: {cover_label(trace, row['ready'])}")
        if row["bang"]:
            parts.append(f"Big Bang {row['bang']} casting")
        mean = "-" if landed is None else f"{landed:,.0f}"
        rs = "-" if resisted is None else f"{resisted:,.0f}"
        print(f"    {clock(row['t']):>9} {len(row['hits']):3} hits, per hit {mean} landed, {rs} resisted;"
              f" {'; '.join(parts)}")
        for entry in row["under"]:
            hit = "not hit" if entry["landed"] is None else f"hit {entry['landed']:,}" + (
                f" +{entry['absorbed']:,} absorbed" if entry["absorbed"] else "")
            pressed = ", ".join(f"{name} {clock(at)}" for at, name in entry["pressed"]) or "none"
            buttons = ", ".join(button_label(button) for button in entry["buttons"]) or "no button"
            used = "".join(f"; used {name} {clock(at)}" for at, name in entry["consumables"])
            print(f"      {label_of(trace, entry['guid'])} {entry['hp']:,} of {entry['line']:,}, {hit}"
                  f"{', died' if entry['died'] else ''}; pressed {pressed}; {buttons}{used}")
    for label, group in (("covered", [row for row in explosions if row["covered"]]),
                         ("uncovered", [row for row in explosions if not row["covered"]])):
        hits = [rec for row in group for rec in row["hits"]]
        landed, resisted = hit_means(hits)
        if landed is not None:
            rs = "-" if resisted is None else f"{resisted:,.0f}"
            print(f"  {label + ':':10} {len(group)} explosion(s), {len(hits)} hits, per hit {landed:,.0f} landed"
                  f" (absorbs included), {rs} resisted")

    pressed = press_rows(trace)
    for kind, title in (("personal", "personal presses"), ("cover", "explosion covers")):
        group = [row for row in pressed if row["kind"] == kind]
        print(f"  {title} (algalon.{kind}): {len(group)}")
        for row in group:
            star = "no star died near it" if row["died"] is None else (
                f"its star died {clock(row['died'])} ({(row['died'] - row['t']) / 1000:+.2f} s)"
                + (", already dead" if row["died"] <= row["t"] else ""))
            print(f"    {clock(row['t']):>9} {trace.name(row['guid'])} {press_name(row)}: {star}"
                  f"{', phased' if row['phased'] else ''}")
    failed = failed_covers(trace)
    print(f"  cover node runs that FAILED: {len(failed)}")
    for row in failed:
        print(f"    {clock(row['t']):>9} {trace.name(row['guid'])}; other covers ready:"
              f" {cover_label(trace, row['ready']) or 'none'}")
    thin = thin_covers(trace)
    print(f"  covers cast with fewer than {COVER_MIN_UNDER} under the explosion's line: {len(thin)}")
    for row in thin:
        print(f"    {clock(row['t']):>9} {trace.name(row['guid'])} {row['cover']}, {row['under']} under")
    spent = spent_buttons(trace)
    print(f"  explosion buttons a class node spent above {BUTTON_HOLD_PCT:.0f}%, phased or in P1: {len(spent)}")
    for row in spent:
        print(f"    {clock(row['t']):>9} {trace.name(row['guid'])} {row['label']} at {row['pct']:.0f}%, {row['where']}")

    lost = explosion_deaths(trace)
    print(f"  deaths to a star explosion: {len(lost)}")
    for row in lost:
        used = ", ".join(f"{text} {clock(when)}" for when, text in row["used"]) or "none"
        ready = f"; ready and unpressed: {', '.join(row['ready'])}" if row["ready"] else ""
        print(f"    {clock(row['t']):>9} {label_of(trace, row['guid'])}: personal buttons pressed {used}{ready}")
    cuts = window_cuts(trace)
    print(f"  `window` cutters on a holder at {WINDOW_CUT_PCT:.0f}% or more: {len(cuts)}")
    for note in cuts:
        print(f"    {tankdef_label(trace, note)} -> {trace.name(note['target'])}")

    held = held_sacrifices(trace)
    print(f"  Divine Sacrifice up past {SACRIFICE_HELD_MS / 1000:.0f} s on a bot: " + ("; ".join(
        f"{clock(row['t'])} {trace.name(row['caster'])}'s {row['held'] / 1000:.1f} s on"
        f" {', '.join(sorted(trace.name(guid) for guid in row['bots']))}" for row in held) or "never"))


def defensive_faults(trace: Trace) -> list[tuple[int, str]]:
    out = []
    for row in tank_deaths(trace):
        if not row["up"] and not row["cast"] and not row["phased"]:
            out.append((row["t"], f"{trace.name(row['guid'])} died @ {secs(row['t'])} with no damage cutter up and"
                                  f" none cast on it in the {TANKHEAL_WINDOW_MS // 1000} s before"))
    for row in held_sacrifices(trace):
        out.append((row["t"], f"Divine Sacrifice from {trace.name(row['caster'])} up {row['held'] / 1000:.1f} s"
                              f" @ {secs(row['t'])} on {', '.join(sorted(trace.name(guid) for guid in row['bots']))}"))
    for row in explosion_deaths(trace):
        if row["ready"]:
            out.append((row["t"], f"{trace.name(row['guid'])} died to a star explosion @ {secs(row['t'])} with"
                                  f" {', '.join(row['ready'])} ready and no personal button pressed"))
    for row in explosion_rows(trace):
        # The cover node stands down through a Big Bang cast, with the raid in the holes.
        if row["ready"] and not row["bang"]:
            out.append((row["t"], f"star explosion @ {secs(row['t'])} with no cover up, ready:"
                                  f" {cover_label(trace, row['ready'])}"))
    for row in press_rows(trace):
        if row["died"] is not None and row["died"] <= row["t"]:
            what = f"pressed {press_name(row)}" if row["kind"] == "personal" else f"cast {press_name(row)} for cover"
            out.append((row["t"], f"{trace.name(row['guid'])} {what} @ {secs(row['t'])},"
                                  f" {(row['t'] - row['died']) / 1000:.2f} s after its star died"))
    for row in spent_buttons(trace):
        where = "inside a hole's phase" if row["where"] == "phased" else "in phase 1, not on a star"
        out.append((row["t"], f"{trace.name(row['guid'])} spent {row['label']} @ {secs(row['t'])} {where},"
                              f" at {row['pct']:.0f}%"))
    for row in thin_covers(trace):
        out.append((row["t"], f"{trace.name(row['guid'])} cast {row['cover']} @ {secs(row['t'])} with"
                              f" {row['under']} under the explosion's line"))
    for row in failed_covers(trace):
        if row["ready"]:
            out.append((row["t"], f"{trace.name(row['guid'])}'s cover failed @ {secs(row['t'])} with"
                                  f" {cover_label(trace, row['ready'])} ready"))
    return out


# ------------------------------------------------------------------------------------------ formation

def parse_ring_slots(text: str) -> list[tuple[float, float, float]]:
    """Every `Position(x, y, z)` in the `ULDUAR_ALGALON_RING_SLOTS` table, in slot order, or nothing when
    the source has no table."""
    found = RING_SLOT_TABLE.search(text)
    if not found:
        return []
    return [(float(x), float(y), float(z)) for x, y, z in RING_SLOT_ROW.findall(found.group(1))]


def parse_ring_notes(text: str) -> list[tuple[str, float, float] | None]:
    """`(role, yd, deg)` from each table row's comment, in slot order, None for a row without one. The
    edge hold measures from the hold spot with a signed angle, the ring before it from the room centre."""
    found = RING_SLOT_TABLE.search(text)
    if not found:
        return []
    out = []
    for line in found.group(1).splitlines():
        if not RING_SLOT_ROW.search(line):
            continue
        note = RING_SLOT_NOTE.search(line)
        out.append((note.group(1), float(note.group(2)), float(note.group(3))) if note else None)
    return out


def arc_offset(index: int, count: int, width: float) -> float:
    """Centre-out around the arc's middle, alternating sides: the same order the strategy filled in."""
    if count <= 1:
        return 0.0
    step = width / (count - 1)
    if count % 2:
        if index == 0:
            return 0.0
        offset = step * ((index + 1) // 2)
        return -offset if index % 2 == 0 else offset
    offset = step / 2 + step * (index // 2)
    return -offset if index % 2 else offset


def arc_slots() -> list[tuple[float, float]]:
    """The slots of a source from before the table: three arcs around the tank slot, 4, 6 and 8 wide.
    Empty when those constants are gone too."""
    out = []
    try:
        for name, count in (("HEALER", 4), ("RANGED_INNER", 6), ("RANGED_OUTER", 8)):
            ring = radius(f"ULDUAR_ALGALON_{name}_RADIUS")
            centre = radius(f"ULDUAR_ALGALON_{name}_ARC_CENTER")
            width = radius(f"ULDUAR_ALGALON_{name}_ARC_WIDTH")
            for index in range(count):
                bearing = centre + arc_offset(index, count, width)
                out.append((TANK_SLOT[0] + math.cos(bearing) * ring, TANK_SLOT[1] + math.sin(bearing) * ring))
    except (Unknown, ValueError):
        return []
    return out


def algalon_source() -> str:
    try:
        return ALGALON_SOURCE.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return ""


@functools.lru_cache(maxsize=1)
def ring_slots() -> tuple[tuple[float, float], ...]:
    """Each ring slot's spot by index: the source's table, else the older arcs."""
    table = parse_ring_slots(algalon_source())
    return tuple((x, y) for x, y, _ in table) if table else tuple(arc_slots())


@functools.lru_cache(maxsize=1)
def ring_notes() -> tuple[tuple[str, float, float] | None, ...]:
    return tuple(parse_ring_notes(algalon_source()))


def slot_position(index: int) -> tuple[float, float] | None:
    spots = ring_slots()
    return spots[index] if 0 <= index < len(spots) else None


def slot_note(index: int) -> tuple[str, float, float] | None:
    found = ring_notes()
    return found[index] if 0 <= index < len(found) else None


def slots(trace: Trace) -> dict[int, int]:
    """First `algalon.slot` per bot. First rather than latest: an erase is written as "0", which is
    also a real slot."""
    out: dict[int, int] = {}
    for rec in notes(trace, "algalon.slot"):
        try:
            out.setdefault(rec["g"], int(rec.get("txt", "")))
        except ValueError:
            continue
    return out


def ring_rows(trace: Trace) -> list[dict]:
    samples = samples_of(trace)
    end = pull_end(trace)
    boss = boss_guid(trace)
    begin = intro_end(trace)
    held = slots(trace)
    members = set(held) or {guid for guid in roster_guids(trace) if trace.role(guid) in ("heal", "ranged")}
    spot = holder_spans(trace, "algalon.spot", end)
    phased = phased_spans(trace)
    stalls = stall_windows(trace, STALL_MS)

    out = []
    for guid in sorted(members, key=trace.name):
        slot = held.get(guid)
        target = slot_position(slot) if slot is not None else None
        to_slot, to_hold, to_boss = [], [], []
        for _, when, _ in samples.steps(0, end):
            row = samples.row(guid, when)
            if not alive(row) or phased_at(phased, guid, when):
                continue
            him = samples.row(boss, when) if boss is not None and begin is not None and when >= begin else None
            if alive(him):
                to_boss.append(dist2(row[1:3], him[1:3]))
            if spot and value_at(spot.get(guid, []), when) not in (None, "slot"):
                continue
            to_hold.append(dist2(row[1:3], HOLD_SPOT))
            if target:
                to_slot.append(dist2(row[1:3], target))
        branches = collections.Counter()
        for value, start, stop in spot.get(guid, []):
            branches[value] += stop - start
        mine = [window for window in stalls if window["guid"] == guid]
        out.append({
            "guid": guid,
            "slot": slot,
            "note": slot_note(slot) if slot is not None else None,
            "to_slot": statistics.median(to_slot) if to_slot else None,
            "to_hold": statistics.median(to_hold) if to_hold else None,
            "to_boss": statistics.median(to_boss) if to_boss else None,
            "in_reach": sum(1 for gap in to_boss if gap <= MELEE_REACH) / len(to_boss) if to_boss else None,
            "branches": branches,
            "stalls": len(mine),
            "stalled": sum(window["end"] - window["start"] for window in mine),
        })
    return out


def show_formation(trace: Trace) -> None:
    print("FORMATION")
    if boss_guid(trace) is None:
        print("  Algalon was never sampled")
        return
    show_absent(trace, "algalon.slot", "algalon.spot")
    if not ring_slots():
        print("  no ring slot table or ring constants in the source: distance to the hold spot only")
    if not edge_hold(trace):
        print("  this trace predates the edge hold (no algalon.spot=pickup): to slot and to A measure against"
              " today's ring around the hold spot, which its bots never used")

    rows = ring_rows(trace)
    if not rows:
        print("  no ring bot")
        return
    print(f"  {'bot':14} {'role':6} {'slot':>4} {'slot is':24} {'to slot':>8} {'to A':>6} {'to boss':>8}"
          f" {'in reach':>8} {'stalls':>10}  spot")
    for row in rows:
        total = sum(row["branches"].values())
        shares = ", ".join(f"{value} {held * 100.0 / total:.0f}%" for value, held in row["branches"].most_common()) \
            if total else "-"
        to_slot = "-" if row["to_slot"] is None else f"{row['to_slot']:.1f}"
        to_hold = "-" if row["to_hold"] is None else f"{row['to_hold']:.1f}"
        to_boss = "-" if row["to_boss"] is None else f"{row['to_boss']:.1f}"
        in_reach = "-" if row["in_reach"] is None else f"{row['in_reach'] * 100.0:.0f}%"
        slot = "-" if row["slot"] is None else str(row["slot"])
        note = f"{row['note'][0]} {row['note'][1]:.0f} yd {row['note'][2]:+.0f} deg" if row["note"] else "-"
        stalled = f"{row['stalls']} / {row['stalled'] / 1000:.0f}s" if row["stalls"] else "-"
        print(f"  {trace.name(row['guid'])[:14]:14} {trace.role(row['guid']):6} {slot:>4} {note:24} {to_slot:>8}"
              f" {to_hold:>6} {to_boss:>8} {in_reach:>8} {stalled:>10}  {shares}")
    print("  slot is the table's own comment: role, yards from the hold spot A and the signed angle off A's"
          " line to the room centre")
    print("  medians over living, unphased samples; to slot and to A only while algalon.spot read slot"
          f" where it exists, to boss and in reach ({MELEE_REACH:.2f} yd) over every sample after the intro;"
          f" stalls are {STALL_MS // 1000} s+ stationary windows with accepted moves")


# ----------------------------------------------------------------------------------------- darkmatter

def dark_matter_rows(trace: Trace) -> list[dict]:
    """Per Unleashed Dark Matter: time on anyone but the handler, the damage it did, and its death.
    Without `algalon.handler`, any tank counts as the handler."""
    samples = samples_of(trace)
    end = pull_end(trace)
    handler = latch_spans(trace, "algalon.handler", end)
    guids = sorted(guids_of_entry(trace, NPC_UNLEASHED_DARK_MATTER))
    hits = [rec for rec in trace.of("dmg") if rec.get("s") in guids]
    despawn = final_despawn(trace)

    def handled(victim: int, when: int) -> bool:
        held = as_guid(value_at(handler, when))
        return victim == held if held else trace.role(victim) == "tank"

    out = []
    for guid in guids:
        seen = samples.seen(guid)
        if not seen:
            continue
        loose: collections.Counter = collections.Counter()
        loose_from = None
        # uncapped: one that drops out of the sweep is still on whoever it held
        for when, row, weight in samples.unit_steps(guid, cap=None):
            victim = target_of(row)
            if alive(row) and victim and not handled(victim, when):
                loose[victim] += weight
                loose_from = when if loose_from is None else loose_from
        mine = [rec for rec in hits if rec.get("s") == guid]
        out.append({
            "guid": guid,
            "spawn": seen[0],
            "loose": loose,
            "loose_from": loose_from,
            "on_handler": sum(rec.get("a", 0) for rec in mine if handled(rec.get("d", 0), rec["t"])),
            "elsewhere": sum(rec.get("a", 0) for rec in mine if not handled(rec.get("d", 0), rec["t"])),
            "killed": seen[1] if seen[1] < end - GONE_MS and seen[1] != despawn else None,
            "despawned": despawn if seen[1] == despawn else None,
        })
    return out


def show_darkmatter(trace: Trace) -> None:
    print("UNLEASHED DARK MATTER")
    rows = dark_matter_rows(trace)
    if not rows:
        print("  none was sampled")
        return
    show_absent(trace, "algalon.handler")

    for row in rows:
        loose = sum(row["loose"].values())
        on = ", ".join(f"{trace.name(g)} {ms / 1000:.1f} s" for g, ms in row["loose"].most_common(4))
        killed = (f"killed {clock(row['killed'])}" if row["killed"] is not None
                  else f"despawned {clock(row['despawned'])}" if row["despawned"] is not None else "alive at the end")
        print(f"  spawned {clock(row['spawn'])}: loose {loose / 1000:.1f} s{' (' + on + ')' if on else ''},"
              f" {row['on_handler']:,} on the handler, {row['elsewhere']:,} elsewhere, {killed}")


# -------------------------------------------------------------------------------------------- verdict

def expected_probes(trace: Trace) -> dict[str, str]:
    """Contract keys this pull's own events say should have been written, and why. Keys that are
    silent for honest reasons (no backup elected, no hole lost) are left out."""
    samples = samples_of(trace)
    boss = boss_guid(trace)
    out: dict[str, str] = {}
    if boss is not None and boss in samples.rows:
        out["algalon.phase"] = "Algalon was sampled"
        if any(trace.role(guid) in ("heal", "ranged") for guid in roster_guids(trace)):
            out["algalon.slot"] = out["algalon.spot"] = "ring roles were present"
    if big_bang_casts(trace):
        for key in ("algalon.bigbang", "algalon.soaker", "algalon.hide"):
            out[key] = "a Big Bang was cast"
    if any(guid in samples.rows for guid in hole_guids(trace)):
        out["algalon.holes"] = "a hole was sampled"
    if any(guid in samples.rows for guid in guids_of_entry(trace, NPC_COLLAPSING_STAR)):
        out["algalon.starwindow"] = "a star was sampled"
    if any(row["active"] is not None for row in constellation_rows(trace)):
        out["algalon.handler"] = "a constellation woke"
        # Only a bot can be the handler, so with a human tank it stays 0 and nobody kites.
        if any(as_guid(rec.get("txt")) for rec in notes(trace, "algalon.handler")):
            out["algalon.kite"] = "a handler was elected"
    if any(guid in samples.rows for guid in guids_of_entry(trace, NPC_UNLEASHED_DARK_MATTER)):
        out.setdefault("algalon.handler", "Unleashed Dark Matter spawned")
    out.update(tank_probes(trace))
    out.update(defensive_probes(trace))
    # Calls go to humans only, so a bot-only raid writes none.
    if big_bang_casts(trace) and any(guid in trace.humans for guid in roster_guids(trace)):
        out["algalon.call"] = "a Big Bang was cast with a human in the raid"
    if star_deaths(trace):
        out["algalon.stardeath"] = "a star died"
    if meteor_beside_bot(trace):
        out["algalon.dodge"] = out["algalon.hazard"] = "a meteor fell beside a bot"
    # The soaker writes its own kit, so a human soaker leaves none.
    if any(row["soaker"] and row["soaker"] not in trace.humans for row in big_bang_rows(trace)):
        out["algalon.soakkit"] = "a bot soaked a Big Bang"
    return out


def tank_probes(trace: Trace) -> dict[str, str]:
    boss = boss_guid(trace)
    if boss is None or boss not in samples_of(trace).rows:
        return {}
    out = {"algalon.swappair": "Algalon was sampled"}
    if any(guid not in trace.humans for guid in swap_pair(trace)[0]):
        out["algalon.tankrole"] = "a bot tank was in the swap pair"
    if intro_end(trace) is not None:
        out["algalon.pull"] = "the intro ended"
    # The pair gets at most half the healers, and only bots.
    healers = roster_healers(trace)
    if len(healers) >= 2 and any(guid not in trace.humans for guid in healers):
        out["algalon.tankhealer"] = "two healers or more, a bot among them"
    if any(guid not in trace.humans for guid in tank_healer_spans(trace)):
        out["algalon.tankheal"] = "a bot was a tank healer"
    return out


def defensive_probes(trace: Trace) -> dict[str, str]:
    boss = boss_guid(trace)
    if boss is None or boss not in samples_of(trace).rows:
        return {}
    out = {}
    bots = {guid for guid in roster_guids(trace) if guid not in trace.humans}
    casters = {(rec.get("s"), rec.get("sp")) for rec in trace.of("cast") if rec.get("s") in bots}
    # The roster names no spec, so a ranged priest stands for a shadow priest with Dispersion.
    if (any(class_of(trace, guid) in PRIEST_CLASS and trace.role(guid) == "ranged" for guid in bots)
            or any(spell == SPELL_DISPERSION for _, spell in casters)):
        out["algalon.disperser"] = "a shadow priest bot was in the raid"
    if any(spell == SPELL_AURA_MASTERY for _, spell in casters):
        out["algalon.shadowaura"] = "a paladin bot cast Aura Mastery"
    if cover_providers(trace) and explosion_rows(trace):
        out["algalon.cover"] = "a star exploded with a bot able to cover it"
    # During a cast the soak's own nodes spend these buttons, and they write no note.
    bangs = big_bang_casts(trace)
    if any(save["caster"] in bots and save["target"] == victim_at(trace, save["t"])
           and not any(cast["start"] <= save["t"] <= cast["end"] for cast in bangs) for save in tank_saves(trace)):
        out["algalon.tankdef"] = "a bot put a damage cutter on the tank he was hitting"
    return out


def meteor_beside_bot(trace: Trace) -> bool:
    samples = samples_of(trace)
    bots = [guid for guid in roster_guids(trace) if guid not in trace.humans]
    return any(alive(row := samples.row(guid, wave["seen"])) and to_meteor(wave, row[1:3]) <= DODGE_TRIGGER_YD
               for wave in smash_waves(trace) for guid in bots)


def tank_faults(trace: Trace) -> list[tuple[int, str]]:
    """Verdict lines for his reach at the intro, loose windows, swaps that never came, threat steals."""
    out = []
    pull = pull_row(trace)
    if pull and pull["inside"]:
        out.append((pull["t"], f"{len(pull['inside'])} player(s) inside {MELEE_REACH:.2f} yd of him when the intro"
                               f" ended @ {secs(pull['t'])} ({', '.join(trace.name(g) for g in pull['inside'])})"))
    for window in loose_windows(trace):
        span = window["stop"] - window["start"]
        if window["deaths"] or span >= PICKUP_MS:
            count = len(window["deaths"])
            out.append((window["start"], f"Algalon loose {span / 1000:.1f} s @ {secs(window['start'])}"
                                         f" ({window_cause(window)}): {count} death{'' if count == 1 else 's'}"))
    pair, _ = swap_pair(trace)
    for due in swap_dues(trace):
        if due["taken"] is None:
            partners = ", ".join(trace.name(guid) for guid in sorted(pair - {due["tank"]})) or "nobody"
            out.append((due["t"], f"swap due @ {secs(due['t'])}: {trace.name(due['tank'])} at {SWAP_STACKS} stacks,"
                                  f" {partners} never took him ({due['missed'][1]} @ {secs(due['missed'][0])})"))
    for when, old, new in threat_steals(trace):
        out.append((when, f"threat steal @ {secs(when)}: {trace.name(new)} pulled him off {trace.name(old)}"
                          f" with no taunt"))
    return out


def star_faults(trace: Trace) -> list[tuple[int, str]]:
    out = []
    safe = explosion_safe_hp(trace)
    for row in star_rows(trace):
        at = secs(row["t"])
        if row["gap"] is not None and row["gap"] < STAR_GAP_MS:
            out.append((row["t"], f"stars died {row['gap'] / 1000:.1f} s apart @ {at}"))
        if row["below"]:
            hp, guid, _ = row["below"][0]
            out.append((row["t"], f"star died with {len(row['below'])} raid member(s) under {safe:,} hp"
                                  f" (weakest {trace.name(guid)} {hp:,}) @ {at}"))
        if row["meteor"]:
            out.append((row["t"], f"star died with a Cosmic Smash meteor live @ {at}"))
        if row["bang"] and not (row["plan"] and row["plan"]["mark"] == "u") and row["cause"] != "under":
            out.append((row["t"], f"star died during Big Bang {row['bang']}'s cast @ {at}"))
        outside = under_outside(row)
        if outside:
            kind, gap = outside
            window = row["plan"]["window"] if row["plan"] else None
            detail = (f", {gap / 1000:.1f} s {'before' if kind == 'early' else 'after'} its window"
                      f" {clock(window[0])}..{clock(window[1])}" if gap is not None and window else "")
            out.append((row["t"], f"under-star died {kind} @ {at}{detail}"))
        named = 0
        if row["nearest"] and row["nearest"][0] < STAR_NEAR_YD:
            gap, named = row["nearest"]
            pair = " of the swap pair" if any(guid == named for _, guid in row["pair"]) else ""
            out.append((row["t"], f"star died {gap:.1f} yd from {trace.name(named)}{pair} @ {at}"))
        for gap, guid in row["pair"]:
            if guid != named:
                out.append((row["t"], f"star died {gap:.1f} yd from {trace.name(guid)} of the swap pair @ {at}"))
    return out


def failures(trace: Trace) -> list[tuple[int | None, str]]:
    """`(t, line)` per detected failure; t is None for what has no time of its own."""
    out: list[tuple[int | None, str]] = []
    have = emitted(trace)
    gone = sorted(key for key in expected_probes(trace) if key not in have)
    if gone:
        out.append((None, f"missing probes: {', '.join(gone)}"))

    explained: set[int] = set()
    for row in big_bang_rows(trace):
        n, at = row["n"], row["start"]
        if not row["holes"]:
            out.append((at, f"no hole at Big Bang {n} @ {secs(at)}"))
        if row["elected"] and not row["soaker"]:
            out.append((at, f"Big Bang {n} @ {secs(at)}: no soaker elected"))
        if row["soaker_phased"]:
            out.append((at, f"Big Bang {n} @ {secs(at)}: soaker {trace.name(row['soaker'])} phased at the impact"))
        if row["stray"]:
            count = len(row["stray"])
            out.append((at, f"Big Bang {n} @ {secs(at)}: {count} non-soaker{'s' if count > 1 else ''} hit unphased"
                            f" ({', '.join(trace.name(g) for g in row['stray'])})"))
        if row["soaker_death"] is not None:
            out.append((row["soaker_death"], f"Big Bang {n} @ {secs(at)}: soaker {trace.name(row['soaker'])} died"
                                             f" before the raid returned"))
        if row["nobody"] is not None:
            out.append((row["nobody"], f"Big Bang {n} @ {secs(at)}: nobody alive and unphased within"
                                       f" {RESET_RANGE:.0f} yd at +{(row['nobody'] - row['end']) / 1000:.1f} s"))
        if row["ended"] or row["ascend"] is not None:
            parts = []
            if row["ascend"] is not None:
                explained.add(row["ascend"])
                parts.append(f"Ascend at {(row['ascend'] - row['end']) / 1000:+.1f} s")
            if row["ended"]:
                explained.add(row["ended"][0])
                parts.append(f"pull ended {row['ended'][1]}")
            first = row["ended"][0] if row["ended"] else row["ascend"]
            kind = "evade" if not row["ended"] or row["ended"][1] == "reset" else "wipe"
            out.append((first, f"{kind} {(first - row['end']) / 1000:.1f} s after Big Bang {n} ({', '.join(parts)})"))

    for loop in rephase_loops(trace):
        out.append((loop["start"], f"re-phase loop: {trace.name(loop['guid'])} @ {secs(loop['start'])}"
                                   f" ({applications(loop['applications'])},"
                                   f" {(loop['stop'] - loop['start']) / 1000:.1f} s phased)"))

    out += [(row["start"], f"stray phase ended in a death: {trace.name(row['guid'])} @ {secs(row['start'])}"
                           f" ({row['cause']})")
            for row in phasing_rows(trace) if row["label"] == "stray" and row["died"]]

    out += star_faults(trace)
    out += collateral_faults(trace)
    out += window_faults(trace)
    out += under_faults(trace)
    out += soak_faults(trace)
    out += bang_faults(trace)
    out += taunt_faults(trace)
    out += redirect_faults(trace)
    out += hold_faults(trace)
    out += kite_faults(trace)
    out += holder_target_faults(trace)
    team = star_team(trace)
    if team["members"]:
        for guid, held in team["off"].most_common():
            if held >= OFF_TEAM_MS:
                since = team["off_from"][guid]
                out.append((since, f"{trace.name(guid)} on stars for {held / 1000:.1f} s outside the star team,"
                                   f" from {secs(since)}"))

    for rec in notes(trace, "algalon.holelost"):
        if rec.get("txt") == "stray":
            out.append((rec["t"], f"constellation removed by a stray hole @ {secs(rec['t'])}"))
    for row in constellation_rows(trace):
        if row["how"] == "killed":
            out.append((row["removed"], f"constellation killed by damage @ {secs(row['removed'])}"))
        elif row["how"] == "hole" and row["handler"] and row["last_victim"] != row["handler"]:
            out.append((row["removed"], f"constellation removed by {trace.name(row['last_victim'])}, not the"
                                        f" handler, @ {secs(row['removed'])}"))

    out += smash_faults(trace)
    out += tank_faults(trace)
    out += tankheal_faults(trace)
    out += defensive_faults(trace)

    for when, guid in punched_out(trace):
        out.append((when, f"Phase Punch phased {trace.name(guid)} out @ {secs(when)}"))
    for start, stop, guid in pickups(trace):
        if stop - start >= PICKUP_MS:
            out.append((start, f"Algalon on {who(trace, guid)} for {(stop - start) / 1000:.1f} s @ {secs(start)}"))
    boss = boss_guid(trace)
    if boss is not None:
        for when, row, _ in samples_of(trace).unit_steps(boss):
            if dist2(row[1:3], ROOM_CENTER) > ROOM_RADIUS:
                out.append((when, f"Algalon {dist2(row[1:3], ROOM_CENTER):.1f} yd from the room centre"
                                  f" @ {secs(when)}"))
                break

    for row in dark_matter_rows(trace):
        loose = sum(row["loose"].values())
        if loose >= LOOSE_MS:
            victim = row["loose"].most_common(1)[0][0]
            out.append((row["loose_from"], f"Unleashed Dark Matter loose for {loose / 1000:.1f} s (mostly on"
                                           f" {trace.name(victim)}) @ {secs(row['loose_from'])}"))

    p1 = phase_start(trace, "P1")
    for rec in trace.of("cast"):
        if rec.get("sp") == SPELL_ASCEND and rec["t"] not in explained:
            enraged = p1 is not None and rec["t"] >= p1 + ENRAGE_MS - 2000
            out.append((rec["t"], f"Ascend to the Heavens @ {secs(rec['t'])}{' (enrage)' if enraged else ''}"))
    finish = trace.of("end")[-1] if trace.of("end") else None
    if finish and finish.get("out") == "reset" and finish["t"] not in explained:
        out.append((finish["t"], f"pull ended reset @ {secs(finish['t'])}, not after a Big Bang"))

    return sorted(out, key=lambda item: (item[0] is not None, item[0] or 0))


def show_verdict(trace: Trace) -> None:
    print("VERDICT")
    if boss_guid(trace) is None and not big_bang_casts(trace):
        print("  Algalon was never sampled, nothing to judge")
        return
    rows = failures(trace)
    if not rows:
        print("  no failures detected")
        return
    for _, line in rows:
        print(f"  {line}")


SECTIONS = (
    ("phases", "phase spans, deaths per phase, boss health at each change, the enrage", show_phases),
    ("pull", "the intro's end: who stood in his reach, his first victim and swings", show_pull),
    ("deaths", "every death by the mechanic that killed it, cause by role", show_deaths),
    ("loose", "every stretch he spent off the swap pair: cause, hits, deaths, who took him back", show_loose),
    ("swaps", "the swap pair, Phase Punch, swaps due and their latency, calls, threat steals", show_swaps),
    ("hold", "the edge hold: his distance from A and C, every pickup's drag to the tank slot", show_hold),
    ("taunts", "every taunt cast at him, non-tanks flagged, pair taunts that missed", show_taunts),
    ("reach", "seconds each raid member outside the pair stood alive and unphased in his reach", show_reach),
    ("bigbang", "every Big Bang: holes and their reach, soaker and backup changes, who it hit and where its runners"
                " ran, tank-alone time", show_bigbang),
    ("soak", "every Big Bang's soakers: the hit, healers, kit, reserved buttons spent", show_soak),
    ("phasing", "every hole phase: Big Bang or stray, its cause, Dark Matter, walk guard vetoes", show_phasing),
    ("stars", "star deaths, health against the drain, the kill window, the star plan against the death and the Big"
              " Bang hit, wand trims and shots (older traces), under-star switches, releases and health at the cast,"
              " focus span loss and the bulk's median, the star team",
     show_stars),
    ("collateral", "star health lost beyond the drain while unfocused, every cast on a star", show_collateral),
    ("holes", "hole stock, empty seconds before each Big Bang, holes lost, urgent spans", show_holes),
    ("constellations", "activations, victims, one stuck in the air, the handler's kite stages, what each kite ate,"
                       " seconds the kite was blocked, parked stretches, removals", show_constellations),
    ("smash", "every Cosmic Smash wave: who stood near it, the dodge, distance at the impact", show_smash),
    ("tanks", "Phase Punch stacks, swaps, non-tank pickups, Quantum Strike", show_tanks),
    ("tankheal", "tank healers, each healer's share on the pair, Beacon, healing against damage before every tank"
                 " death", show_tankheal),
    ("defensives", "damage cutters and Lay on Hands on the swap tanks, every swap tank death, cover at every star"
                   " explosion and the bots under its line, every personal press and cover against its star's death,"
                   " personal buttons at each explosion death", show_defensives),
    ("formation", "each ring bot's distance to its slot and to him, spot branches, stalls", show_formation),
    ("darkmatter", "phase 2 Unleashed Dark Matter: loose seconds, handler damage, kills", show_darkmatter),
    ("verdict", "one line per detected failure, in time order", show_verdict),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
