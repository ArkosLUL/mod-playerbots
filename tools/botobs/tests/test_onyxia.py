"""Tests for the Onyxia reader.

    python -m unittest discover -s tools/botobs/tests

A synthetic 25-man pull with no `onyxia.*` probe pins the raw-record fallback behind each section, a
second one carrying the probes pins the paths that read them, and the shared fixture shows every
section reads empty on a trace that is not Onyxia rather than raising.
"""
from __future__ import annotations

import contextlib
import io
import json
import math
import pathlib
import sys
import tempfile
import unittest

BOTOBS = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BOTOBS))

from bosses import onyxia as ony  # noqa: E402
from raidobs.geometry import Unknown  # noqa: E402
from raidobs.trace import Trace  # noqa: E402

FULL = BOTOBS / "fixtures" / "full-v13.ndjson"

BOSS = 4294968500
WHELP_EGG = 4294968501
WHELP_CAVE = 4294968502
WHELP_LATE = 4294968503
GUARD = 4294968504
GUARD2 = 4294968505
TANK = 7001
MELEE = 7002
MAGE = 7003
HUNTER = 7004
PRIEST = 7005

WP5 = (22.876, -217.152, -60.0)
WP1 = (-64.496, -214.906, -60.0)
LAND = (-63.496, -214.906, -84.4)
EGG = (-60.0, -166.0, -93.5)
HOP = 18352


def her_spot(when: int) -> tuple[float, float, float]:
    """Held at the hold spot, thrown 20 yd south with the tank at 10 s, then the air waypoints."""
    if when < 11000:
        return (17.0, -212.5, -85.6)
    if when < 25000:
        return (-3.0, -212.5, -85.6)
    if when <= 32000:
        return WP5
    if when < 34000:
        return (-20.0, -216.0, -60.0)
    if when <= 40000:
        return WP1
    if when < 45000:
        return (-63.5, -214.9, -70.0)
    return LAND


def her_hp(when: int) -> float:
    if when < 20000:
        return 100.0 - when / 1000.0
    if when < 25000:
        return 64.0
    if when < 41000:
        return 50.0
    if when < 45000:
        return 39.0
    return 38.0 - (when - 45000) / 1000.0


def her_victim(when: int) -> int:
    if when == 0:
        return 0
    if 4000 <= when < 7000:
        return MELEE
    if when < 20000:
        return TANK
    if when < 45000:
        return 0
    if when < 48000:
        return MELEE
    return TANK


def grounded(when: int) -> bool:
    return when < 25000 or when >= 45000


def around(when: int, turn: float, reach: float) -> tuple[float, float, float]:
    """A spot `reach` yd out at `turn` degrees off her facing, which is north on the ground."""
    x, y, _ = her_spot(when)
    return (x + reach * math.cos(math.radians(turn)), y + reach * math.sin(math.radians(turn)), -85.0)


def row(guid, spot, hp=100.0, target=0, dealt=0, facing=0.0):
    return [guid, spot[0], spot[1], spot[2], facing, hp, 100.0, target, 0, 0, 0, dealt, 0, 100.0]


def snap(when: int) -> dict:
    if when <= 10000:
        tank = (35.0, -212.5, -83.5)
    elif when < 25000:
        tank = (15.0, -212.5, -85.0)
    elif when < 45000:
        tank = (30.0, -212.0, -84.0)
    else:
        tank = (-45.496, -214.906, -84.4)
    # Shiv walks onto the egg at 14-16 s and dies to Blast Nova at 47.5 s.
    if 14000 <= when <= 16000:
        melee = (-60.5, -165.5, -92.5)
    elif grounded(when):
        melee = around(when, 110.0, 5.0)
    else:
        melee = (5.0, -210.0, -86.0)
    units = [
        row(BOSS, her_spot(when), her_hp(when), her_victim(when)),
        row(TANK, tank, target=BOSS if grounded(when) else 0),
        row(MELEE, melee, 0.0 if when >= 48000 else 100.0, BOSS, 0 if when < 2000 else 1000),
        row(MAGE, around(when, 114.0, 30.0) if grounded(when) else (10.0, -217.0, -86.0),
            target=BOSS if grounded(when) else 0),
        row(HUNTER, around(when, -126.0, 36.0) if grounded(when) else (-40.0, -214.0, -86.0),
            target=BOSS if grounded(when) or 34000 <= when <= 40000 else 0),
        row(PRIEST, around(when, 102.0, 24.0) if grounded(when) else (0.0, -200.0, -86.0), target=TANK),
    ]
    if 15000 <= when < 25000:
        units.append(row(WHELP_EGG, (-59.0, -165.0, -93.0)))
    if 27000 <= when <= 33000:
        units.append(row(WHELP_CAVE, (-33.0, -259.5, -89.0)))
    if 30000 <= when <= 33000:
        units.append(row(WHELP_LATE, (-32.0, -169.5, -89.0)))
    if when >= 35000:
        units.append(row(GUARD, (-101.654, -214.491, -80.7)))
    if when >= 50000:
        units.append(row(GUARD2, (-100.0, -210.0, -80.7)))
    return {"t": when, "e": "snap", "u": units, "hz": []}


def cast(when, caster, spell, target=0, ct=0, triggered=False) -> dict:
    rec = {"t": when, "e": "cast", "s": caster, "sp": spell, "tgt": target, "ct": ct}
    if triggered:
        rec["tr"] = 1
    return rec


def dmg(when, source, target, spell, amount) -> dict:
    return {"t": when, "e": "dmg", "s": source, "d": target, "sp": spell, "a": amount, "ok": 0, "sc": 4,
            "ab": 0, "rs": 0, "hp": 100.0}


def header(boss: str = "onyxia") -> dict:
    return {"e": "hdr", "v": 13, "ts": 1792000000000, "map": 249, "inst": 9, "diff": 1, "boss": boss,
            "roster": [
                {"g": TANK, "n": "Bulwark", "r": "tank", "c": "warrior", "h": 0},
                {"g": MELEE, "n": "Shiv", "r": "melee", "c": "rogue", "h": 0},
                {"g": MAGE, "n": "Frost", "r": "ranged", "c": "mage", "h": 0},
                {"g": HUNTER, "n": "Trueshot", "r": "ranged", "c": "hunter", "h": 0},
                {"g": PRIEST, "n": "Mend", "r": "heal", "c": "priest", "h": 0}]}


def onyxia_pull() -> list[dict]:
    records = [
        header(),
        {"t": 0, "e": "pull", "boss": "onyxia", "src": "engage"},
        {"t": 1, "e": "unit", "g": BOSS, "en": ony.NPC_ONYXIA, "n": "Onyxia", "b": 1},
        {"t": 1, "e": "unit", "g": WHELP_EGG, "en": ony.NPC_WHELP, "n": "Onyxian Whelp"},
        {"t": 1, "e": "unit", "g": WHELP_CAVE, "en": ony.NPC_WHELP, "n": "Onyxian Whelp"},
        {"t": 1, "e": "unit", "g": WHELP_LATE, "en": ony.NPC_WHELP, "n": "Onyxian Whelp"},
        {"t": 1, "e": "unit", "g": GUARD, "en": ony.NPC_LAIR_GUARD, "n": "Onyxian Lair Guard"},
        {"t": 1, "e": "unit", "g": GUARD2, "en": ony.NPC_LAIR_GUARD, "n": "Onyxian Lair Guard"},
        {"t": 1, "e": "spell", "sp": HOP, "n": "Breath"},
        {"t": 1, "e": "spell", "sp": 18351, "n": "Breath"},
        {"t": 100, "e": "haz", "sp": ony.EGG_TRAP, "shape": "circle", "x": EGG[0], "y": EGG[1], "z": EGG[2],
         "ttl": 0, "rad": 8.0},

        # First hits: Bulwark's Heroic Strike, Frost's Frostbolt inside the dwell, Shiv's melee off his
        # dealt column at 2 s, Trueshot after the dwell. Tricks goes to the tank.
        cast(500, TANK, 78, BOSS),
        cast(800, MELEE, ony.TRICKS, TANK),
        cast(1500, MAGE, 42842, BOSS, 2500),
        cast(3700, MAGE, 2825),
        cast(5000, HUNTER, 49052, BOSS, 2000),

        # Cleave twice, Tail Sweep, Wing Buffet throwing the tank and her 20 yd south, Flame Breath.
        cast(3000, BOSS, 68868, TANK),
        dmg(3000, BOSS, TANK, 68868, 9000),
        cast(8000, BOSS, 68868, TANK),
        dmg(8000, BOSS, MELEE, 68868, 9000),
        cast(9000, BOSS, 69286),
        dmg(9000, BOSS, HUNTER, 69286, 5000),
        cast(10000, BOSS, 69293, 0, 500),
        dmg(10500, BOSS, TANK, 69293, 3000),
        cast(12000, BOSS, 68970, 0, 2000),
        dmg(14000, BOSS, TANK, 68970, 15000),
        dmg(14000, BOSS, PRIEST, 68970, 15000),

        # Shiv tries to dodge out of the pit and finds no path.
        {"t": 15200, "e": "move", "g": MELEE, "k": "point", "x": -50.0, "y": -170.0, "z": -88.0, "ok": 0,
         "r": "nopath", "by": ony.DODGE},

        # A cave summon right before the cave whelp shows; the late whelp near the west cave has none.
        cast(26800, BOSS, ony.SUMMON_WHELP, 0, 0, True),

        # Two Fireballs: the first one's hits land at 30.1 s, after the second already started.
        cast(26000, BOSS, 68926, MAGE, 3000),
        cast(30000, BOSS, 68926, PRIEST, 3000),
        dmg(30100, BOSS, MAGE, 68926, 20000),
        dmg(30100, BOSS, HUNTER, 68926, 15000),
        dmg(34100, BOSS, PRIEST, 68926, 18000),

        # Deep Breath off waypoint 1, two hops on Trueshot.
        cast(35000, BOSS, 18351, 0, 0),
        dmg(37000, BOSS, HUNTER, HOP, 9000),
        dmg(38000, BOSS, HUNTER, HOP, 9000),

        # Blast Nova: one kicked, one landed that kills Shiv, and a hit from a guard with no cast row.
        cast(36000, GUARD, ony.BLAST_NOVA, 0, 5000),
        cast(37000, MAGE, 2139, GUARD),
        cast(41900, HUNTER, 34490, GUARD),
        cast(42000, GUARD, ony.BLAST_NOVA, 0, 5000),
        dmg(47000, GUARD, TANK, ony.BLAST_NOVA, 12000),
        dmg(47000, GUARD, MELEE, ony.BLAST_NOVA, 20000),
        {"t": 47500, "e": "death", "g": MELEE, "killer": GUARD, "x": 0.0, "y": 0.0, "z": 0.0,
         "blow": [GUARD, 20000]},
        dmg(52000, GUARD2, PRIEST, ony.BLAST_NOVA, 9000),

        # Phase 3: a roar fears two, Eruption hits Frost.
        cast(46000, BOSS, ony.BELLOWING_ROAR),
        {"t": 46100, "e": "aura", "d": PRIEST, "s": BOSS, "sp": ony.BELLOWING_ROAR, "r": 0, "st": 1, "dur": 3000,
         "p": 0},
        {"t": 46100, "e": "aura", "d": HUNTER, "s": BOSS, "sp": ony.BELLOWING_ROAR, "r": 0, "st": 1, "dur": 3000,
         "p": 0},
        {"t": 49000, "e": "aura", "d": PRIEST, "s": BOSS, "sp": ony.BELLOWING_ROAR, "r": 1, "st": 1, "dur": 0,
         "p": 0},
        dmg(46500, BOSS, MAGE, 69294, 8000),

        {"t": 60000, "e": "end", "out": "wipe"},
    ]
    return records + [snap(when) for when in range(0, 61000, 1000)]


def probed_pull() -> list[dict]:
    """A short pull carrying the probes: phases, the tank, threat states, a slot, dodges."""
    def probe_snap(when):
        her = row(BOSS, (0.0, 0.0, -85.0), 90.0, TANK)
        mage = row(MAGE, (30.0 * math.cos(math.radians(-102.0)), 30.0 * math.sin(math.radians(-102.0)), -85.0))
        return {"t": when, "e": "snap", "u": [her, row(TANK, (18.0, 0.0, -85.0), target=BOSS), mage,
                                             row(HUNTER, (-20.0, 20.0, -85.0))], "hz": []}

    def note(when, guid, key, text):
        return {"t": when, "e": "note", "g": guid, "k": key, "txt": text}

    records = [
        header(),
        {"t": 0, "e": "pull", "boss": "onyxia", "src": "engage"},
        {"t": 1, "e": "unit", "g": BOSS, "en": ony.NPC_ONYXIA, "n": "Onyxia", "b": 1},
        # restated as the trace opens, which is not a phase of the pull
        note(-100, TANK, ony.PHASE_KEY, "0"),
        note(0, TANK, ony.PHASE_KEY, "1"),
        note(20000, MAGE, ony.PHASE_KEY, "2"),
        note(25000, TANK, ony.PHASE_KEY, "3"),
        note(41000, TANK, ony.PHASE_KEY, "4"),
        note(45000, TANK, ony.PHASE_KEY, "5"),
        # the probe names the main tank though another one held her longer
        note(0, HUNTER, ony.TANK_KEY, "pickup"),
        note(2000, HUNTER, ony.TANK_KEY, "hold"),
        note(1000, MAGE, ony.THREAT_KEY, "1"),
        note(4000, MAGE, ony.THREAT_KEY, "2"),
        note(9000, MAGE, ony.THREAT_KEY, "0"),
        note(1000, HUNTER, ony.THREAT_KEY, "1"),
        note(4000, HUNTER, ony.THREAT_KEY, "0"),
        note(0, MAGE, ony.SLOT_KEY, "7"),
        note(5000, MAGE, ony.SLOTFAIL_KEY, "7"),
        note(6000, MAGE, ony.SLOTFAIL_KEY, "none"),
        note(15000, HUNTER, ony.DODGE_KEY, "egg"),
        note(30000, MAGE, ony.DODGE_KEY, "breath+fireball"),
        note(31000, MAGE, ony.DODGE_KEY, "none"),
        {"t": 60000, "e": "end", "out": "wipe"},
    ]
    return records + [probe_snap(when) for when in range(0, 10000, 1000)]


def load(folder: str, name: str, records: list[dict]) -> Trace:
    path = pathlib.Path(folder) / name
    path.write_text("\n".join(json.dumps(rec) for rec in records) + "\n", encoding="utf-8")
    return Trace(path)


class SourceTables(unittest.TestCase):
    def test_the_lanes_and_fissures_parse_out_of_onydata(self):
        self.assertEqual(sorted(ony.LANES), list(range(1, 9)))
        self.assertEqual(ony.LANES[5]["spell"], 17086)
        self.assertEqual(ony.LANE_SPELLS[18351], 1)
        self.assertEqual(ony.LANES[1]["hover"], WP1)
        self.assertEqual(len(ony.FISSURES), 52)

    def test_a_lane_row_reads_waypoint_spell_and_three_points(self):
        text = ("inline std::array<BreathLane, 8> const BREATH_LANES = {{\n"
                "    {3, 18609, {-29.450f, -180.600f, -60.0f}, {-37.7f, -188.6f, -86.0f},\n"
                "        {-37.7f, -243.7f, -86.0f}},\n"
                "}};")
        self.assertEqual(ony.parse_lanes(text), {3: {"spell": 18609, "hover": (-29.45, -180.6, -60.0),
                                                     "from": (-37.7, -188.6, -86.0), "to": (-37.7, -243.7, -86.0)}})

    def test_an_integer_constant_reads_and_a_missing_one_refuses(self):
        self.assertEqual(ony.source_int("ONYXIA_X_MS", "constexpr uint32 ONYXIA_X_MS = 1234;"), 1234)
        with self.assertRaises(Unknown):
            ony.source_int("ONYXIA_Y_MS", "constexpr uint32 ONYXIA_X_MS = 1234;")

    def test_slot_numbers_map_to_ring_radius_and_side(self):
        self.assertEqual(ony.slot_spot(0), (24.0, 102.0))
        self.assertEqual(ony.slot_spot(1), (24.0, -102.0))
        self.assertEqual(ony.slot_spot(4), (24.0, 126.0))
        self.assertEqual(ony.slot_spot(7), (30.0, -102.0))
        self.assertEqual(ony.slot_spot(14), (36.0, 114.0))
        self.assertEqual(ony.slot_spot(18), (42.0, 102.0))
        self.assertEqual(ony.slot_spot(25), (48.0, -102.0))

    def test_bearing_is_positive_to_the_west_while_she_faces_north(self):
        her = [BOSS, 0.0, 0.0, 0.0, 0.0]
        self.assertAlmostEqual(ony.bearing(her, (0.0, 10.0)), 90.0)
        self.assertAlmostEqual(ony.bearing(her, (0.0, -10.0)), -90.0)
        self.assertAlmostEqual(abs(ony.bearing(her, (-10.0, 0.0))), 180.0)
        facing_south = [BOSS, 0.0, 0.0, 0.0, math.pi]
        self.assertAlmostEqual(ony.bearing(facing_south, (-10.0, 0.0)), 0.0)


class SyntheticPull(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = load(cls.folder.name, "249_9_onyxia_1792000000.ndjson", onyxia_pull())

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_phases_derive_from_her_health_and_height(self):
        spans, source = ony.phase_spans(self.trace)
        self.assertEqual(source, "her health and height")
        self.assertEqual(spans, [(ony.GROUND, 0, 20000), (ony.LIFTOFF, 20000, 25000), (ony.AIR, 25000, 41000),
                                 (ony.LANDING, 41000, 45000), (ony.FINAL, 45000, 60000)])
        self.assertEqual(ony.liftoff_time(self.trace), 20000)
        self.assertEqual(ony.touchdown_time(self.trace), 45000)

    def test_the_main_tank_is_who_she_held_longest_on_the_ground(self):
        self.assertEqual(ony.main_tank(self.trace), TANK)
        self.assertEqual(ony.tank_hold(self.trace, TANK), 1000)

    def test_aggro_losses_are_ground_only_and_name_who_took_her(self):
        losses = ony.aggro_losses(self.trace)
        self.assertEqual([(loss["start"], loss["stop"], loss["guid"]) for loss in losses],
                         [(4000, 7000, MELEE), (45000, 48000, MELEE)])
        self.assertAlmostEqual(losses[0]["tank_gap"], 18.0)

    def test_tank_spot_counts_phase_1_only(self):
        spot = ony.tank_spot_stats(self.trace, TANK)
        self.assertAlmostEqual(spot["min"], 1.0)
        self.assertAlmostEqual(spot["on"], 11 / 20)
        self.assertEqual(spot["first_on"], 0)

    def test_rests_split_on_her_moving_and_refit_uses_the_ones_with_the_tank_on_his_spot(self):
        stands = ony.rests(self.trace, TANK)
        self.assertEqual([(rest["start"], rest["stop"]) for rest in stands],
                         [(48000, 60000), (11000, 20000), (7000, 11000), (1000, 4000)])
        self.assertAlmostEqual(stands[1]["tank_gap"], 19.0)
        refit = ony.refit_spot(stands)
        for got, want in zip(refit, (17.0, -212.5, -85.6)):
            self.assertAlmostEqual(got, want)

    def test_a_wing_buffet_reads_her_drift_and_the_tank_throw(self):
        rows = ony.wing_buffets(self.trace, TANK)
        self.assertEqual(len(rows), 1)
        self.assertAlmostEqual(rows[0]["north"], -20.0)
        self.assertAlmostEqual(rows[0]["tank"], 20.0)

    def test_first_hits_take_a_cast_or_a_dealt_rise_and_flag_the_dwell(self):
        tank, hold, rows = ony.threat_rows(self.trace)
        self.assertEqual((tank, hold), (TANK, 1000))
        got = {row["guid"]: (row["t"], row["how"], row["early"]) for row in rows}
        self.assertEqual(got, {TANK: (500, "cast", False), MAGE: (1500, "cast", True),
                               MELEE: (2000, "dealt", True), HUNTER: (5000, "cast", False)})

    def test_lust_and_redirects(self):
        self.assertEqual([(row["t"], row["caster"], row["phase"]) for row in ony.lust_casts(self.trace)],
                         [(3700, MAGE, "ground")])
        self.assertEqual(ony.redirect_casts(self.trace), {(MELEE, TANK): [800]})

    def test_layout_reads_bearing_and_range_off_her_facing(self):
        groups = ony.layout(self.trace)
        self.assertEqual(set(groups), {"tank", "melee", "caster", "hunter", "heal"})
        for group, turn, reach in (("tank", 0.0, 18.0), ("caster", 114.0, 30.0), ("hunter", -126.0, 36.0),
                                   ("heal", 102.0, 24.0)):
            for value in groups[group]["bearings"]:
                self.assertAlmostEqual(value, turn, places=3)
            for value in groups[group]["gaps"]:
                self.assertAlmostEqual(value, reach, places=3)

    def test_cone_hits_count_by_group(self):
        rows = {row["label"]: (row["casts"], dict(row["hits"])) for row in ony.cone_hits(self.trace)}
        self.assertEqual(rows, {"Flame Breath": (1, {"tank": 1, "heal": 1}), "Cleave": (2, {"tank": 1, "melee": 1}),
                                "Wing Buffet": (1, {"tank": 1}), "Tail Sweep": (1, {"hunter": 1})})

    def test_a_whelp_is_from_a_cave_only_right_after_her_summon(self):
        kinds = {row["guid"]: (row["t"], row["kind"]) for row in ony.whelp_rows(self.trace)}
        self.assertEqual(kinds, {WHELP_EGG: (15000, "egg"), WHELP_CAVE: (27000, "cave"), WHELP_LATE: (30000, "egg")})

    def test_a_hatch_names_the_nearest_member_in_3d(self):
        bursts = ony.hatch_bursts(self.trace, ony.whelp_rows(self.trace))
        self.assertEqual([(burst["t"], burst["count"], burst["nearest"]) for burst in bursts],
                         [(15000, 1, MELEE), (30000, 1, PRIEST)])
        self.assertAlmostEqual(bursts[0]["gap"], math.dist((-60.5, -165.5, -92.5), (-59.0, -165.0, -93.0)))

    def test_egg_exposure_and_the_pit(self):
        eggs = ony.egg_spots(self.trace)
        self.assertEqual(eggs, [EGG])
        self.assertEqual(ony.egg_exposure(self.trace, eggs), {MELEE: (3000, 14000)})
        self.assertEqual([(row["guid"], row["ms"], row["first"], row["longest"], row["moves"], row["refused"],
                           row["by"]) for row in ony.pit_rows(self.trace)],
                         [(MELEE, 3000, 14000, 3000, 1, 1, ony.DODGE)])

    def test_visits_are_stays_at_a_hover_point(self):
        self.assertEqual([(stay["waypoint"], stay["start"], stay["stop"]) for stay in ony.visits(self.trace)],
                         [(5, 25000, 32000), (1, 34000, 40000)])

    def test_reach_and_targeting_per_visit(self):
        self.assertEqual(ony.reach_shares(self.trace, 25000, 32000), {"ranged": (16, 8, 0), "heal": (8, 8, 0)})
        self.assertEqual(ony.reach_shares(self.trace, 34000, 40000), {"ranged": (14, 7, 7), "heal": (7, 0, 0)})
        self.assertEqual(ony.airborne(self.trace), (25000, 45000))
        self.assertEqual(ony.targeting_share(self.trace, 25000, 45000), {HUNTER: 7 / 20})

    def test_a_fireball_hit_goes_to_the_cast_already_finished(self):
        rows = ony.fireballs(self.trace)
        self.assertEqual([(row["t"], row["victims"], row["damage"]) for row in rows],
                         [(26000, [MAGE, HUNTER], 35000), (30000, [PRIEST], 18000)])

    def test_deep_breath_counts_the_named_hops(self):
        self.assertEqual([(row["t"], row["waypoint"], row["victims"]) for row in ony.breaths(self.trace)],
                         [(35000, 1, [HUNTER, HUNTER])])

    def test_blast_nova_outcomes_and_kills(self):
        casts, stray = ony.novas(self.trace)
        self.assertEqual([(row["t"], row["outcome"], row["victims"]) for row in casts],
                         [(36000, "kicked", []), (42000, "landed", [TANK, MELEE])])
        self.assertEqual(casts[0]["kicks"], [(37000, MAGE, 2139)])
        self.assertEqual(stray, [{"t": 52000, "guard": GUARD2, "victims": [PRIEST]}])
        self.assertEqual([death["g"] for death in ony.nova_kills(self.trace)], [MELEE])

    def test_roars_and_aggro_after_touch_down(self):
        rows = ony.roars(self.trace)
        self.assertEqual([(row["t"], row["feared"], row["erupted"]) for row in rows],
                         [(46000, [HUNTER, PRIEST], [MAGE])])
        self.assertIn(MELEE, rows[0]["near"])
        self.assertNotIn(MAGE, rows[0]["near"])
        landed = ony.after_touchdown(self.trace)
        self.assertEqual((landed["touch"], landed["first"], landed["tank_at"]), (45000, (45000, MELEE), 48000))
        self.assertAlmostEqual(landed["pickup_gap"], math.dist((-45.496, -214.906), (-32.0, -205.0)))
        self.assertEqual([loss["start"] for loss in landed["losses"]], [45000])

    def test_every_probe_the_reader_reads_shows_as_missing(self):
        self.assertTrue(set(ony.PROBE_KEYS) <= set(ony.missing_probes(self.trace)))

    def test_every_section_prints_on_the_pull(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            ony.show_banner(self.trace)
            for _, _, section in ony.SECTIONS:
                section(self.trace)
        text = out.getvalue()
        self.assertIn("every section reads raw records", text)
        self.assertIn("Fireball: 2 cast(s), 3 hit(s)", text)
        self.assertIn("Blast Nova from 2 Lair Guard(s) named: 2 cast(s) seen, 1 landed, 1 kicked, 0 neither;"
                      " 1 more landed with no cast row", text)
        self.assertIn("killed: Shiv", text)
        self.assertIn("refit: with the tank on his spot she stood at (17.0, -212.5, -85.60)", text)


class ProbedPull(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = load(cls.folder.name, "249_9_onyxia_1792000001.ndjson", probed_pull())

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_phases_come_from_the_probe_clipped_to_the_pull(self):
        spans, source = ony.phase_spans(self.trace)
        self.assertEqual(source, ony.PHASE_KEY)
        self.assertEqual(spans, [(ony.GROUND, 0, 20000), (ony.LIFTOFF, 20000, 25000), (ony.AIR, 25000, 41000),
                                 (ony.LANDING, 41000, 45000), (ony.FINAL, 45000, 60000)])

    def test_the_tank_probe_names_the_main_tank(self):
        self.assertEqual(ony.main_tank(self.trace), HUNTER)

    def test_threat_states_count_bots_and_time(self):
        self.assertEqual(ony.threat_states(self.trace),
                         {"dwell": (2, 6000), "cap": (1, 5000), "free": (2, 107000)})

    def test_a_slot_compares_against_where_the_bot_stood(self):
        rows = ony.slot_rows(self.trace)
        self.assertEqual(len(rows), 1)
        row = rows[0]
        self.assertEqual((row["guid"], row["slot"], row["radius"], row["bearing"], row["fails"]),
                         (MAGE, 7, 30.0, -102.0, 1))
        self.assertAlmostEqual(row["gap"], 30.0)
        self.assertAlmostEqual(row["turn"], -102.0)

    def test_dodge_reasons_count_bots(self):
        self.assertEqual(ony.dodge_bots(self.trace), {"egg": 1, "breath": 1, "fireball": 1})

    def test_emitted_probes_drop_out_of_the_missing_line(self):
        missing = ony.missing_probes(self.trace)
        self.assertNotIn(ony.PHASE_KEY, missing)
        self.assertIn(ony.ANCHOR_KEY, missing)


class EveryView(unittest.TestCase):
    def test_every_section_reads_empty_on_another_boss(self):
        trace = Trace(FULL)
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            ony.show_banner(trace)
            for _, _, section in ony.SECTIONS:
                section(trace)
        self.assertIn("not an Onyxia pull", out.getvalue())
        self.assertIn("Onyxia was never named", out.getvalue())


if __name__ == "__main__":
    unittest.main()
