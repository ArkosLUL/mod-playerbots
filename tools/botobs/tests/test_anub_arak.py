"""Tests for the Anub'arak reader.

    python -m unittest discover -s tools/botobs/tests

A synthetic 10H pull pins the arithmetic behind each section, a 25H one the side tank, grip, kite
stage and allowance probes, a second 25H one the gather and the burrower window, a third the
Permafrost time, the focus picks and the tank patches spent, a fourth the tank spots, a dropped grip,
a burrower's submerge, the speeds at a cover, the healer and tank views, and the shared fixture shows
every section reads empty on a trace that is not Anub'arak rather than raising.
"""
from __future__ import annotations

import contextlib
import io
import json
import math
import pathlib
import statistics
import sys
import tempfile
import unittest

BOTOBS = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BOTOBS))

from bosses import anub_arak as ab  # noqa: E402
from raidobs.trace import Trace  # noqa: E402

FULL = BOTOBS / "fixtures" / "full-v12.ndjson"

TANK = 5001
HEAL = 5002
KITE = 5003   # ranged, marked twice
MELEE = 5004  # human, marked once, impaled
LATE = 5005   # west of the Web Door at the pull
FLOAT = 5006  # still up in the coliseum at the pull, lands at 5 s, dies to the spike
PALADIN = 5007

CREATURE = 1 << 32
SPIKE = CREATURE | 800  # lower guid than the boss, same name, also boss-flagged
BOSS = CREATURE | 900
SPHERES = [CREATURE | n for n in range(911, 917)]
S1, S2, S3 = SPHERES[:3]
B1 = CREATURE | 921
B2 = CREATURE | 922

X, Y, Z = ab.ROOM_CENTER
PATCH_A = (X + 10.0, Y + 12.0)
PATCH_B = (X + 10.0, Y - 12.0)
KITE_PATCH = (X - 20.0, Y + 14.0)
FLYING_Z = 155.67
PATCH_Z = 142.7
FALLING_Z = 143.0
PERMAFROST_10H = 67856
IMPALE_10H = 67859
COLD_10H = 68509
SWARM_10H = 68646


def phase(when: int) -> int:
    if when < 0:
        return 0
    if when < 20000:
        return 1
    if when < 40000:
        return 2
    return 1 if when < 50000 else 3


def boss_target(when: int) -> int:
    if when < 0 or phase(when) == 2:
        return 0
    if when < 2000:
        return KITE
    if 40000 <= when < 43000:
        return HEAL
    return TANK


def spheres_at(when: int) -> list[tuple[int, tuple[float, float], float]]:
    """`(guid, xy, z)` per sphere alive at `when`. S1 and S2 land at 8 s; S3 falls at 22 s, lands at 23 s,
    and the spike takes it, last sampled at 31 s."""
    if when < 0:
        return []
    out = [(S1, PATCH_A, PATCH_Z if when >= 8000 else FLYING_Z),
           (S2, PATCH_B, PATCH_Z if when >= 8000 else FLYING_Z)]
    if when <= 31000:
        out.append((S3, KITE_PATCH, PATCH_Z if when >= 23000 else FALLING_Z if when >= 22000 else FLYING_Z))
    out += [(guid, (X + 5.0 * index, Y + 30.0), FLYING_Z) for index, guid in enumerate(SPHERES[3:])]
    return out


# phase 3 x offset and moving flag: 6 yd east, a 1 yd shuffle, 4 yd west, then 4 yd east with a
# 1 yd step back before he stops
P3_WALK = {50000: (0.0, 1), 51000: (3.0, 1), 52000: (6.0, 0), 53000: (6.0, 1), 54000: (7.0, 0),
           55000: (3.0, 1), 56000: (-1.0, 0), 57000: (-1.0, 1), 58000: (4.0, 1), 59000: (3.0, 0)}


def boss_spot(when: int) -> tuple[float, float]:
    """He walks 10 yd north at 12 s, while the tank is on a submerge leg, and stays there to the
    submerge."""
    if 12000 <= when < 20000:
        return X + 10.0, Y + 15.0
    return X + 10.0 + P3_WALK.get(when, (0.0, 0))[0], Y + 5.0


def boss_moving(when: int) -> int:
    return 1 if when == 11000 else P3_WALK.get(when, (0.0, 0))[1]


def burrowers_at(when: int) -> list[list]:
    rows = []
    if 2000 <= when <= 19000:
        spot = (PATCH_A[0] + 2.0, PATCH_A[1]) if when >= 8000 else (X + 10.0, Y + 20.0)
        rows.append([B1, spot[0], spot[1], Z, 0.0, 100.0 - (when - 2000) / 200.0, 0.0, TANK, 0, 0, 0, 0])
    # B2: gone 6-11 s and back full, then a jump from 60 to 100 at 17 s.
    b2 = {2000: 100.0, 3000: 90.0, 4000: 80.0, 5000: 70.0, 12000: 100.0, 13000: 90.0, 14000: 80.0,
          15000: 70.0, 16000: 60.0, 17000: 100.0, 18000: 95.0, 19000: 90.0}
    if when in b2:
        rows.append([B2, X - 20.0, Y, Z, 0.0, b2[when], 0.0, KITE, 0, 0, 0, 0])
    return rows


def player(guid: int, when: int) -> list:
    spots = {TANK: (X + 10.0, Y + 3.0, Z), HEAL: (X - 10.0, Y, Z), KITE: (X - 20.0, Y + 10.0, Z),
             MELEE: (X + 10.0, Y + 6.0, Z), LATE: (640.0, 144.7, 144.0), PALADIN: (X + 5.0, Y - 5.0, Z)}
    if guid == FLOAT:
        spot = (745.0, 135.0, 395.0) if when < 5000 else (X, Y - 10.0, Z)
    else:
        spot = spots[guid]
    hp = 100.0
    if guid == FLOAT and when >= 39000:
        hp = 0.0
    if guid == MELEE and when == 54000:
        hp = 40.0
    return [guid, spot[0], spot[1], spot[2], 0.0, hp, 100.0, 0, 0, 0, 0, 0]


def snap(when: int) -> dict:
    units = [player(guid, when) for guid in (TANK, HEAL, KITE, MELEE, LATE, FLOAT, PALADIN)]
    units.append([BOSS, *boss_spot(when), Z, 0.0, 100.0 - max(when, 0) / 1000.0, 0.0,
                  boss_target(when), boss_moving(when), 0, 0, 0])
    if phase(when) == 2:
        units.append([SPIKE, X - 30.0, Y + 14.0, Z, 0.0, 100.0, 0.0, KITE, 0, 0, 0, 0])
    hz = []
    for guid, (x, y), z in spheres_at(when):
        units.append([guid, x, y, z, 0.0, 100.0, 0.0, 0, 0, 0, 0, 0])
        if z == PATCH_Z:
            hz.append([PERMAFROST_10H, x, y, PATCH_Z, 6.0, 1])
    units += burrowers_at(when)
    return {"t": when, "e": "snap", "u": units, "hz": hz}


def note(when: int, guid: int, key: str, txt) -> dict:
    return {"t": when, "e": "note", "g": guid, "k": key, "txt": str(txt)}


def aura(when: int, guid: int, spell: int, removed: bool, caster: int = BOSS) -> dict:
    return {"t": when, "e": "aura", "d": guid, "s": caster, "sp": spell, "r": 1 if removed else 0}


def dmg(when: int, source: int, victim: int, spell: int, amount: int) -> dict:
    return {"t": when, "e": "dmg", "s": source, "d": victim, "sp": spell, "a": amount}


def cast(when: int, source: int, spell: int, target: int) -> dict:
    return {"t": when, "e": "cast", "s": source, "sp": spell, "tgt": target, "ct": 0}


def veto(when: int, guid: int, multiplier: str, action: str) -> dict:
    return {"t": when, "e": "veto", "g": guid, "m": multiplier, "a": action}


def drop(when: int, guid: int) -> dict:
    return {"t": when, "e": "act", "g": guid, "a": "drop target", "rel": 99.0, "vd": "OK"}


def anub_pull(boss: str = "anub-arak", map_id: int = ab.MAP_TOC) -> list[dict]:
    records = [
        {"e": "hdr", "v": 13, "ts": 1789500000000, "map": map_id, "inst": 4, "diff": 2, "boss": boss,
         "roster": [
             {"g": TANK, "n": "Bulwark", "r": "tank", "c": "warrior", "h": 0},
             {"g": HEAL, "n": "Mercy", "r": "heal", "c": "priest", "h": 0},
             {"g": KITE, "n": "Trueshot", "r": "ranged", "c": "hunter", "h": 0},
             {"g": MELEE, "n": "Arkos", "r": "melee", "c": "druid", "h": 1},
             {"g": LATE, "n": "Straggler", "r": "ranged", "c": "mage", "h": 0},
             {"g": FLOAT, "n": "Drifter", "r": "melee", "c": "warrior", "h": 0},
             {"g": PALADIN, "n": "Justice", "r": "heal", "c": "paladin", "h": 0}]},
        {"t": 0, "e": "pull", "boss": boss, "src": "engage"},
        {"t": -2000, "e": "unit", "g": SPIKE, "en": 34660, "n": "Anub'arak", "b": 1},
        {"t": -2000, "e": "unit", "g": BOSS, "en": ab.NPC_ANUBARAK, "n": "Anub'arak", "b": 1},
        {"t": 2000, "e": "unit", "g": B1, "en": ab.NPC_BURROWER, "n": "Nerubian Burrower"},
        {"t": 2000, "e": "unit", "g": B2, "en": ab.NPC_BURROWER, "n": "Nerubian Burrower"},
        *({"t": 0, "e": "unit", "g": guid, "en": ab.NPC_FROST_SPHERE, "n": "Frost Sphere"}
          for guid in SPHERES),

        note(-30000, TANK, "toc.progress", 8),
        note(-20000, TANK, "toc.progress", 9),
        note(0, TANK, "anub.phase", 1),
        note(20000, TANK, "anub.phase", 2),
        note(40000, TANK, "anub.phase", 1),
        note(50000, TANK, "anub.phase", 3),
        note(10000, TANK, "anub.patch0", S1),
        note(10000, TANK, "anub.patch1", S2),
        # side 1 loses its latch through the later marks, so S2 counts as a patch nobody latched
        note(30500, TANK, "anub.patch1", 0),
        note(39000, TANK, "anub.patch1", S2),

        # Kite shoots both tank patches down.
        note(7000, KITE, "anub.sphere", 0),
        note(7500, KITE, "anub.sphere", 1),
        note(8100, KITE, "anub.sphere", "none"),
        cast(7000, KITE, 75, S1),
        cast(7600, KITE, 75, S2),
        # Late stocks S3 just before the first mark, then Kite shoots one while Melee kites
        note(21000, LATE, "anub.sphere", "stock"),
        drop(21200, LATE),
        cast(21500, LATE, 133, S3),
        note(26000, LATE, "anub.sphere", "none"),
        note(34000, KITE, "anub.sphere", "kite"),
        drop(34200, KITE),
        cast(34500, KITE, 75, SPHERES[3]),
        note(35500, KITE, "anub.sphere", "none"),
        drop(50000, KITE),

        # B2 lands one Shadow Strike on Kite and has the second kicked.
        {"t": 3000, "e": "cast", "s": B2, "sp": ab.SPELL_SHADOW_STRIKE, "tgt": KITE, "ct": 8000},
        dmg(11000, B2, KITE, ab.SPELL_SHADOW_STRIKE, 40000),
        {"t": 13000, "e": "cast", "s": B2, "sp": ab.SPELL_SHADOW_STRIKE, "tgt": KITE, "ct": 8000},
        note(12500, MELEE, "anub.interrupter", 1),
        note(12500, TANK, "anub.interrupter", 0),
        note(14000, MELEE, "anub.interrupter", 0),

        # Four marks: a patch, an Impale, a death and one the emerge ends.
        aura(22000, KITE, ab.SPELL_MARK, False, SPIKE),
        note(22100, KITE, "anub.kite", "patch"),
        note(27000, KITE, "anub.kite", "hold"),
        aura(30000, KITE, ab.SPELL_MARK, True, SPIKE),
        note(30000, KITE, "anub.kite", "none"),
        aura(33000, MELEE, ab.SPELL_MARK, False, SPIKE),
        dmg(35000, SPIKE, MELEE, IMPALE_10H, 18000),
        aura(36000, MELEE, ab.SPELL_MARK, True, SPIKE),
        aura(36500, FLOAT, ab.SPELL_MARK, False, SPIKE),
        aura(38500, FLOAT, ab.SPELL_MARK, True, SPIKE),
        {"t": 38500, "e": "death", "g": FLOAT, "killer": SPIKE, "x": X, "y": Y - 10.0, "z": Z,
         "blow": [SPIKE, 20000]},
        aura(38800, KITE, ab.SPELL_MARK, False, SPIKE),
        note(39000, KITE, "anub.kite", "ring"),
        aura(39600, KITE, ab.SPELL_MARK, True, SPIKE),
        # The tank stood in the lane while Kite ran.
        dmg(29000, SPIKE, TANK, IMPALE_10H, 15000),
        note(28000, MELEE, "anub.dodge", "spike"),
        note(28500, TANK, "anub.dodge", "lane"),
        note(29500, MELEE, "anub.dodge", "none"),

        # Paladin covers Melee too late, Kite blocks its own last kite, and a Hand of Protection on
        # somebody unmarked counts for nobody
        note(34000, PALADIN, "anub.protect", "hop"),
        cast(35500, PALADIN, 10278, MELEE),
        note(36000, PALADIN, "anub.protect", "none"),
        cast(37000, PALADIN, 10278, HEAL),
        note(39000, KITE, "anub.protect", "self"),
        cast(39100, KITE, 45438, KITE),
        note(39600, KITE, "anub.protect", "none"),

        note(0, TANK, "anub.pickup", "attack"),
        note(11500, TANK, "anub.pickup", "submerge"),
        note(40500, TANK, "anub.pickup", "taunt"),
        cast(41000, PALADIN, ab.SPELL_RIGHTEOUS_DEFENSE, HEAL),
        cast(42500, TANK, 355, BOSS),
        note(43000, TANK, "anub.pickup", "hold"),
        # he is back on the tank, so this one never reaches him
        cast(44000, PALADIN, ab.SPELL_RIGHTEOUS_DEFENSE, HEAL),
        cast(45000, MELEE, 6795, BOSS),

        veto(30000, KITE, ab.THREAT_CAP, "auto shot"),
        veto(45000, KITE, ab.THREAT_CAP, "auto shot"),
        veto(45500, KITE, "anubarak sphere guard", "polymorph"),
        veto(46000, KITE, ab.THREAT_CAP, "multi-shot"),
        veto(46500, LATE, ab.THREAT_CAP, "frostbolt"),

        aura(50200, TANK, SWARM_10H, False),
        aura(50200, MELEE, SWARM_10H, False),
        note(50500, TANK, "anub.defensive", "shield wall"),
        note(51000, TANK, "anub.defensive", "covered"),
        dmg(51000, BOSS, TANK, ab.SPELL_LEECHING_SWARM_DMG, 1000),
        dmg(51000, BOSS, MELEE, ab.SPELL_LEECHING_SWARM_DMG, 2000),
        dmg(52000, BOSS, TANK, ab.SPELL_LEECHING_SWARM_DMG, 1000),
        dmg(53000, BOSS, TANK, ab.SPELL_LEECHING_SWARM_DMG, 1000),
        {"t": 51500, "e": "cast", "s": KITE, "sp": 32182, "tgt": 0, "ct": 0},

        aura(52000, MELEE, COLD_10H, False),
        {"t": 53000, "e": "heal", "s": HEAL, "d": MELEE, "sp": 48071, "a": 5000, "oh": 0},
        {"t": 54000, "e": "heal", "s": HEAL, "d": TANK, "sp": 48071, "a": 3000, "oh": 0},
        {"t": 55000, "e": "heal", "s": HEAL, "d": MELEE, "sp": 48071, "a": 4000, "oh": 1000},
        aura(58000, MELEE, COLD_10H, True),
        {"t": 60000, "e": "end", "out": "wipe"},
    ]
    records += [snap(when) for when in range(-2000, 60000, 1000)]
    return records


def write_trace(folder: str, name: str, records: list[dict]) -> Trace:
    path = pathlib.Path(folder) / name
    path.write_text("\n".join(json.dumps(rec) for rec in records) + "\n", encoding="utf-8")
    return Trace(path)


class SyntheticPull(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = write_trace(cls.folder.name, "649_4_anub-arak_1789500000.ndjson", anub_pull())

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_the_boss_is_his_entry_not_the_spike_that_shares_his_name(self):
        self.assertEqual(ab.boss_guid(self.trace), BOSS)

    def test_phases_letter_the_repeats_and_read_his_health_at_each_edge(self):
        rows = ab.phase_rows(self.trace)
        self.assertEqual([row["label"] for row in rows], ["P1", "P2", "P1b", "P3"])
        self.assertEqual([row["hp"] for row in rows], [100.0, 80.0, 60.0, 50.0])
        self.assertEqual(sum(1 for row in rows if row["phase"] == 2), 1)

    def test_a_death_lands_in_the_phase_it_happened_in(self):
        dead = {row["label"]: [rec["g"] for rec in row["deaths"]] for row in ab.phase_rows(self.trace)}
        self.assertEqual(dead["P2"], [FLOAT])
        self.assertEqual(dead["P1b"], [])

    def test_stage_nine_after_an_earlier_stage_is_an_edge(self):
        self.assertEqual(ab.stage_nine(self.trace), (-20000, True))

    def test_pull_flags_the_locked_out_and_the_late_lander(self):
        rows = {row["guid"]: row for row in ab.pull_rows(self.trace)}
        self.assertEqual(set(rows), {LATE, FLOAT})
        self.assertEqual(rows[LATE]["why"], "locked out")
        self.assertIsNone(rows[LATE]["arrived"])
        self.assertEqual(rows[FLOAT]["why"], "not landed")
        self.assertEqual(rows[FLOAT]["arrived"], 5000)

    def test_each_mark_window_gets_its_outcome(self):
        windows = ab.mark_windows(self.trace)
        self.assertEqual([(w["guid"], w["outcome"]) for w in windows],
                         [(KITE, "patch"), (MELEE, "impale"), (FLOAT, "death"), (KITE, "emerge")])
        self.assertTrue(windows[0]["consumed"])
        self.assertEqual((windows[1]["impales"], windows[1]["damage"]), (1, 18000))

    def test_a_mark_starting_inside_a_patchs_slow_is_flagged(self):
        # Arkos stands 6 yd off S1 at its mark; S3 under Trueshot's first mark is still falling, no centre yet
        windows = ab.mark_windows(self.trace)
        self.assertEqual([window["in_frost"] for window in windows], [False, True, False, False])
        self.assertAlmostEqual(windows[1]["frost_yd"], 6.0)

    def test_a_window_never_opens_on_the_branch_an_earlier_one_closed_with(self):
        windows = ab.mark_windows(self.trace)
        self.assertEqual(windows[0]["branches"], ["patch", "hold"])
        self.assertEqual(windows[3]["branches"], ["ring"])

    def test_a_window_opens_on_a_branch_written_before_its_aura(self):
        self.assertEqual(ab.branches_in([(900, "patch"), (1500, "hold")], 1000, 2000), ["patch", "hold"])

    def test_each_window_reads_protection_on_the_kiter(self):
        windows = ab.mark_windows(self.trace)
        self.assertEqual([window["near"] for window in windows], [0, None, None, 200])
        self.assertEqual([[(rec["t"], rec["s"]) for rec in window["hop"]] for window in windows],
                         [[], [(35500, PALADIN)], [], []])
        self.assertEqual([[rec["sp"] for rec in window["own"]] for window in windows], [[], [], [], [45438]])
        self.assertEqual([[(rec["g"], rec["txt"]) for rec in window["protect"]] for window in windows],
                         [[], [(PALADIN, "hop"), (PALADIN, "none")], [], [(KITE, "self"), (KITE, "none")]])

    def test_impale_on_anyone_but_the_kiter_is_stray(self):
        stray = ab.stray_impales(self.trace, ab.mark_windows(self.trace))
        self.assertEqual([(rec["t"], rec["d"]) for rec in stray], [(29000, TANK)])

    def test_spheres_split_flying_from_patches_by_height(self):
        self.assertEqual(ab.sphere_timeline(ab.Samples(self.trace)),
                         [(-2000, 0, 0, 0), (0, 6, 0, 0), (8000, 4, 2, 2), (22000, 3, 3, 2),
                          (23000, 3, 3, 3), (32000, 3, 2, 2)])

    def test_a_low_sphere_with_no_permafrost_on_it_is_falling(self):
        samples = ab.Samples(self.trace)
        kinds = samples.sphere_kinds(22000)
        self.assertEqual((kinds[S1], kinds[S3], kinds[SPHERES[3]]), ("patch", "falling", "flying"))
        self.assertEqual(samples.sphere_kinds(23000)[S3], "patch")

    def test_only_a_patch_gone_just_after_a_mark_is_consumed(self):
        samples = ab.Samples(self.trace)
        self.assertEqual(ab.patch_lives(self.trace, samples),
                         {S1: (8000, 59000), S2: (8000, 59000), S3: (22000, 31000)})
        self.assertEqual(ab.consumed_patches(self.trace, samples), {S3: (31000, 30000)})

    def test_heroic_spheres_left_at_each_submerge_and_phase_3(self):
        left = ab.spheres_left(self.trace, ab.Samples(self.trace))
        self.assertEqual(left, [("P2", 20000, 4), ("P3", 50000, 3), ("end", 59000, 3)])

    def test_sphere_shots_count_casts_aimed_at_one(self):
        self.assertEqual(ab.sphere_shots(self.trace), {KITE: 3, LATE: 1})

    def test_patch_supply_at_each_mark(self):
        rows = ab.supply_rows(self.trace)
        self.assertEqual([(row["tank"], row["other"], row["flying"], row["falling"]) for row in rows],
                         [(2, 0, 3, 1), (1, 1, 3, 0), (1, 1, 3, 0), (1, 1, 3, 0)])
        self.assertEqual([row["holders"] for row in rows], [[(LATE, "stock")], [(KITE, "kite")], [], []])

    def test_each_duty_span_counts_its_holders_shots_and_drops(self):
        self.assertEqual([(row["guid"], row["label"], row["start"], row["stop"], row["casts"], row["drops"])
                          for row in ab.duty_spans(self.trace)],
                         [(KITE, "0", 7000, 7500, 1, 0), (KITE, "1", 7500, 8100, 1, 0),
                          (LATE, "stock", 21000, 26000, 1, 1), (KITE, "kite", 34000, 35500, 1, 1)])

    def test_burrower_submerges_read_from_a_gap_and_a_jump(self):
        rows = {row["guid"]: row for row in ab.burrower_rows(self.trace)}
        self.assertEqual(rows[B1]["submerges"], 0)
        self.assertEqual(rows[B2]["submerges"], 2)
        self.assertEqual((rows[B2]["low"], rows[B2]["end"]), (60.0, 90.0))
        self.assertEqual((rows[B1]["first"], rows[B1]["last"]), (2000, 19000))

    def test_burrower_time_on_permafrost(self):
        rows = {row["guid"]: row for row in ab.burrower_rows(self.trace)}
        self.assertAlmostEqual(rows[B1]["frost"], 12 / 18)
        self.assertEqual(rows[B2]["frost"], 0.0)

    def test_a_burrower_is_on_frost_only_within_its_own_reach_not_a_players(self):
        samples = ab.Samples(self.trace)
        self.assertTrue(ab.on_permafrost(samples, [B1, PATCH_A[0] + 6.0, PATCH_A[1], Z], 10000))
        self.assertFalse(ab.on_permafrost(samples, [B1, PATCH_A[0] + 7.0, PATCH_A[1], Z], 10000))

    def test_shadow_strike_lands_only_when_its_damage_follows(self):
        rows = {row["guid"]: row for row in ab.burrower_rows(self.trace)}
        self.assertEqual((rows[B2]["strikes"], rows[B2]["landed"]), (2, 1))

    def test_threat_times_each_surfaced_window_to_a_tank(self):
        rows = ab.threat_rows(self.trace)
        self.assertEqual([(row["label"], row["kind"], row["to_tank"]) for row in rows],
                         [("P1", "pull", 2000), ("P1b", "emerge", 3000), ("P3", "swarm", 0)])
        emerge = rows[1]
        self.assertEqual(dict(emerge["held"]), {"heal": 3000, "tank": 7000})

    def test_righteous_defense_counts_only_on_the_friend_he_is_hitting(self):
        emerge = ab.threat_rows(self.trace)[1]
        self.assertEqual([(rec["t"], rec["s"], rec["sp"]) for rec in emerge["taunts"]],
                         [(41000, PALADIN, ab.SPELL_RIGHTEOUS_DEFENSE), (42500, TANK, 355), (45000, MELEE, 6795)])

    def test_the_drag_anchor_is_the_patch_midpoint_once_both_latch(self):
        rows = ab.threat_rows(self.trace)
        self.assertAlmostEqual(rows[1]["off_anchor"], 5.0)

    def test_the_drag_anchor_is_none_with_a_side_unlatched(self):
        samples = ab.Samples(self.trace)
        latches = [ab.latch_spans(self.trace, key, 60000) for key in ("anub.patch0", "anub.patch1")]
        self.assertIsNone(ab.drag_anchor(latches, samples, 9000))
        self.assertIsNone(ab.drag_anchor(latches, samples, 33000))
        point = ab.drag_anchor(latches, samples, 10000)
        self.assertAlmostEqual(point[0], X + 10.0)
        self.assertAlmostEqual(point[1], Y)

    def test_off_anchor_leaves_out_samples_with_no_anchor_or_on_a_submerge_leg(self):
        # 10 and 11 s only: no anchor before 10 s, a submerge leg from 11.5 s
        self.assertAlmostEqual(ab.threat_rows(self.trace)[0]["off_anchor"], 5.0)

    def test_his_legs_and_reversals_per_window(self):
        rows = ab.threat_rows(self.trace)
        self.assertEqual([(row["label"], row["legs"], round(row["yards"], 1), row["reversals"]) for row in rows],
                         [("P1", 1, 10.0, 0), ("P1b", 0, 0, 0), ("P3", 3, 14.0, 2)])

    def test_a_leg_runs_while_he_is_flagged_moving_and_drops_under_2_yd(self):
        legs = ab.boss_legs(ab.Samples(self.trace), BOSS, 50000, 60000)
        self.assertEqual([(leg["start"], leg["stop"], round(leg["yards"], 1)) for leg in legs],
                         [(50000, 52000, 6.0), (55000, 56000, 4.0), (57000, 59000, 4.0)])

    def test_a_leg_the_window_cuts_ends_on_its_last_moving_sample(self):
        legs = ab.boss_legs(ab.Samples(self.trace), BOSS, 50000, 51500)
        self.assertEqual([(leg["start"], leg["stop"], round(leg["yards"], 1)) for leg in legs],
                         [(50000, 51000, 3.0)])

    def test_pickup_rows_and_threat_cap_vetoes_per_window(self):
        rows = ab.threat_rows(self.trace)
        self.assertEqual(dict(rows[0]["pickup"]), {(TANK, "attack"): 1, (TANK, "submerge"): 1})
        self.assertEqual(dict(rows[1]["pickup"]), {(TANK, "taunt"): 1, (TANK, "hold"): 1})
        self.assertEqual([dict(row["capped"]) for row in rows], [{}, {KITE: 2, LATE: 1}, {}])

    def test_the_submerge_spot_is_his_last_surfaced_sample_against_each_tank_patch(self):
        gaps = ab.threat_rows(self.trace)[0]["submerge"]
        self.assertEqual(gaps["spot"], (X + 10.0, Y + 15.0))
        self.assertEqual({side: round(yards, 1) for side, yards in gaps["patches"].items()}, {0: 3.0, 1: 27.0})

    def test_waves_fall_back_to_burrower_spawns_without_anub_wave(self):
        self.assertEqual(ab.wave_starts(self.trace), [(1, 2000)])

    def test_a_key_the_reader_reads_is_absent_whatever_the_source_declares(self):
        gone = ab.missing_probes(self.trace)
        self.assertIn("anub.wave", gone)
        self.assertIn("anub.spot0", gone)
        self.assertIn("anub.hstand", gone)
        self.assertNotIn("anub.phase", gone)

    def test_swarm_reads_the_probe_and_falls_back_to_the_aura(self):
        self.assertEqual(ab.swarm_span(self.trace), (50000, 60000, "anub.phase"))
        self.assertEqual(ab.swarm_span(self.trace, []), (50200, 60000, "Leeching Swarm"))

    def test_swarm_damage_per_bot(self):
        self.assertEqual(ab.swarm_damage(self.trace), {TANK: (3, 3000), MELEE: (1, 2000)})

    def test_lust_is_timed_from_phase_3(self):
        self.assertEqual([row["rel"] for row in ab.lust_casts(self.trace, 50000)], [1500])

    def test_a_cold_window_counts_only_heals_onto_the_carrier(self):
        rows = ab.cold_windows(self.trace)
        self.assertEqual(len(rows), 1)
        row = rows[0]
        self.assertEqual((row["guid"], row["phase"]), (MELEE, "P3"))
        self.assertEqual((row["heals"], row["amount"], row["overheal"]), (2, 9000, 1000))
        self.assertEqual(row["healers"], {HEAL})
        self.assertEqual(row["low"], 40.0)

    def test_every_section_prints(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            ab.show_banner(self.trace)
            for _, _, section in ab.SECTIONS:
                section(self.trace)
        text = out.getvalue()
        self.assertNotIn("not a Trial of the Crusader", text)
        self.assertIn(f"patch #{S3 & 0xFFFFFFFF} last seen 0:31.000", text)
        self.assertIn("3 taunt(s): bot 2, human 1", text)
        self.assertIn("anubarak threat cap vetoes: Trueshot 2, Straggler 1", text)
        self.assertIn("Hand of Protection by Justice +2.5 s; anub.protect Justice hop +1.0 s", text)
        self.assertIn("Ice Block +0.3 s; anub.protect Trueshot self +0.2 s", text)
        self.assertIn("spans with a cast: side 0 1 of 1, side 1 1 of 1, kite 1 of 1, stock 1 of 1", text)


def focus_pull() -> list[dict]:
    """B1 holds the focus from 1 to 5 s, nothing to 6 s, then B2 to the end at 10 s. Trueshot leaves B1
    for the boss at 4 s, Drifter stays on the boss through B2 and is dead at 9 s, and the human and the
    healer never count. B1 is last sampled at 2 s, the focus due drops at 2 and 4.5 s, and Trueshot puts
    Corruption on B2 twice around B2's Shadow Strike, which Drifter kicks."""
    def target(guid: int, when: int) -> int:
        if guid in (MELEE, FLOAT) and 1000 <= when < 5000:
            return B1
        if guid == KITE:
            return B2 if when >= 6000 else (BOSS if when == 4000 else B1)
        return BOSS if guid == FLOAT else 0

    def focus_snap(when: int) -> dict:
        units = [[guid, X, Y, Z, 0.0, 0.0 if guid == FLOAT and when == 9000 else 100.0, 100.0,
                  target(guid, when), 0, 0, 0, 0] for guid in (KITE, FLOAT, MELEE, HEAL)]
        if when <= 2000:
            units.append([B1, X + 5.0, Y, Z, 0.0, 100.0 - when / 100.0, 0.0, 0, 0, 0, 0, 0])
        units.append([B2, X - 5.0, Y, Z, 0.0, 100.0, 0.0, 0, 0, 0, 0, 0])
        return {"t": when, "e": "snap", "u": units, "hz": []}

    records = [
        {"e": "hdr", "v": 13, "ts": 1789500000000, "map": ab.MAP_TOC, "inst": 4, "diff": 1, "boss": "anub-arak",
         "roster": [
             {"g": KITE, "n": "Trueshot", "r": "ranged", "c": "hunter", "h": 0},
             {"g": FLOAT, "n": "Drifter", "r": "melee", "c": "warrior", "h": 0},
             {"g": MELEE, "n": "Arkos", "r": "melee", "c": "rogue", "h": 1},
             {"g": HEAL, "n": "Mercy", "r": "heal", "c": "priest", "h": 0}]},
        {"t": 0, "e": "pull", "boss": "anub-arak", "src": "engage"},
        {"t": 0, "e": "unit", "g": BOSS, "en": ab.NPC_ANUBARAK, "n": "Anub'arak", "b": 1},
        {"t": 0, "e": "unit", "g": B1, "en": ab.NPC_BURROWER, "n": "Nerubian Burrower"},
        {"t": 0, "e": "unit", "g": B2, "en": ab.NPC_BURROWER, "n": "Nerubian Burrower"},
        # a cleared latch restated at the trace's open is no hold
        note(-500, KITE, "anub.focus", 0),
        note(1000, KITE, "anub.focus", B1),
        note(5000, FLOAT, "anub.focus", 0),
        note(6000, KITE, "anub.focus", B2),
        note(1000, KITE, "anub.dps", "burrower"),
        note(4000, KITE, "anub.dps", "boss"),
        note(4500, KITE, "anub.dps", "burrower"),
        note(1000, FLOAT, "anub.dps", "burrower"),
        *(note(when, KITE, "anub.focusdue", due) for when, due in (
            (0, 0), (1000, 1), (2000, 0), (3000, 1), (4500, 0), (6500, 1))),
        {"e": "spell", "sp": 47813, "n": "Corruption"},
        cast(1500, KITE, 47813, B2),
        cast(1500, MELEE, 1766, B2),
        cast(2500, KITE, 49050, B1),
        {"t": 3000, "e": "cast", "s": B2, "sp": ab.SPELL_SHADOW_STRIKE, "tgt": KITE, "ct": 1000},
        cast(3500, FLOAT, 6552, B2),
        {**cast(3600, KITE, 47813, B2), "tr": 1},
        cast(4500, KITE, 47813, B2),
        {"t": 10000, "e": "end", "out": "wipe"},
    ]
    records += [focus_snap(when) for when in range(0, 10000, 1000)]
    return records


class BurrowerFocus(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = write_trace(cls.folder.name, "649_4_anub-arak_1789500001.ndjson", focus_pull())

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_each_focus_holds_its_time_and_the_living_dps_bots_share(self):
        # B1: Trueshot 3 of 4, Drifter 4 of 4. B2: Trueshot 4 of 4, Drifter 0 of 3 alive.
        rows = {row["guid"]: row for row in ab.focus_rows(self.trace)}
        self.assertEqual(set(rows), {B1, B2})
        self.assertEqual((rows[B1]["held"], rows[B2]["held"]), (4000, 4000))
        self.assertAlmostEqual(rows[B1]["share"], 7 / 8)
        self.assertAlmostEqual(rows[B2]["share"], 4 / 7)

    def test_the_section_prints_the_focus_and_the_dps_picks(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            ab.show_burrowers(self.trace)
        text = out.getvalue()
        self.assertRegex(text, rf"#{B1 & 0xFFFFFFFF} +4\.0s +88%")
        self.assertRegex(text, rf"#{B2 & 0xFFFFFFFF} +4\.0s +57%")
        self.assertIn("anub.dps: Drifter burrower 1; Trueshot burrower 2, boss 1", text)
        self.assertIn("DPS casts at a burrower other than the focus: 2, by Trueshot 2; most: Corruption 47813 2",
                      text)
        self.assertIn(f"anub.focusdue dropped with the focus alive: 0:02.000 #{B1 & 0xFFFFFFFF} at 80%", text)

    def test_casts_off_the_focus_leave_out_humans_triggers_and_a_kick_on_a_cast(self):
        self.assertEqual([(rec["t"], rec["s"], rec["tgt"]) for rec in ab.stray_burrower_casts(self.trace)],
                         [(1500, KITE, B2), (4500, KITE, B2)])

    def test_focus_due_drops_only_count_a_focus_still_sampled(self):
        rows = ab.focus_due_drops(self.trace, ab.Samples(self.trace))
        self.assertEqual([(row["t"], row["guid"], row["hp"]) for row in rows], [(2000, B1, 80.0)])


MAIN, SIDE0, SIDE1, DK, MAGE, LOCK, ROGUE, DRUID, HUNTER, DRAGON = range(6001, 6011)
IMP = (2 << 32) | 7001
G0, G1, K1, K2, IDLE = (CREATURE | n for n in range(911, 916))
BN1, BN2, BS1, BS2 = (CREATURE | n for n in range(921, 925))
WN, WS = CREATURE | 931, CREATURE | 932
SPK = CREATURE | 941
PERMAFROST_25H = 67857

O = ab.TANK_PATCH_OFFSET
GAP = ab.SIDE_SPOT_GAP
EXIT_N1, EXIT_N2, EXIT_S1, EXIT_S2 = (740.2, 193.4), (694.5, 185.4), (732.0, 83.4), (694.9, 102.5)
WAVE1 = {BN1: EXIT_N1, BN2: EXIT_N2, BS1: EXIT_S1, BS2: EXIT_S2}
WAVE2 = {WN: EXIT_N1, WS: EXIT_S1}
PATCH0 = (X + 3.0, Y + O)            # G0, 3 yd off side 0's spot
G1_LAND = (X + 3.0, Y + O - GAP - 2.0)  # 2 yd off side 1's spot once G0 latched
G1_FLY = (X + 20.0, Y - 20.0)
SUBMERGE_AT = (X - 9.0, Y + O)       # 12 yd off G0
SPIKE_ROGUE = (PATCH0[0] + 9.0, PATCH0[1])  # in the fail test's reach of G0
SPIKE_HUNTER = (X - 25.0, Y - 25.0)
SPOTS = {SIDE0: (X + 3.0, Y + O + 4.0), SIDE1: (X, Y - O - 2.0), DK: (X + 3.0, Y + O + 0.5),
         MAGE: (X - 10.0, Y - 3.0), LOCK: (X - 12.0, Y + 3.0), ROGUE: (X + 15.0, Y), DRUID: (X - 14.0, Y - 6.0),
         HUNTER: (X + 15.0, Y + 6.0), IMP: (X - 13.0, Y + 4.0)}


def refine_sphere(guid: int, when: int):
    """`(xy, z)` or None. The DK grips G0 onto itself at 4 s and it dies at 5 s; the grip on G1 at 8 s
    misses, and the imp kills it at 12.5 s; K1 and K2 die while he is under."""
    if when < 0:
        return None
    if guid == G0:
        if when < 4500:
            return (X + 10.0, Y + 20.0), FLYING_Z
        return PATCH0, PATCH_Z if when >= 6500 else Z + 0.5
    if guid == G1:
        if when < 12500:
            return (G1_FLY if when < 11500 else G1_LAND), FLYING_Z
        return G1_LAND, PATCH_Z if when >= 14000 else FALLING_Z
    for sphere, spot, killed in ((K1, (X - 20.0, Y + 20.0), 32500), (K2, (X - 20.0, Y - 20.0), 36500)):
        if guid == sphere:
            if when < killed:
                return spot, FLYING_Z
            return spot, PATCH_Z if when >= killed + 1500 else FALLING_Z
    return (X, Y + 30.0), FLYING_Z


def refine_burrower_target(guid: int, when: int) -> int:
    if guid == BN1:
        return MAGE if when < 7000 else SIDE0
    if guid == BN2:
        return 0 if when < 6000 else (MAIN if when < 9000 else SIDE1)
    if guid == BS1:
        return SIDE1
    if guid == BS2:
        return DRUID
    return 0 if when < 50500 else (SIDE0 if guid == WN else SIDE1)


def refine_snap(when: int) -> dict:
    main = (X + 3.0, Y + O - 3.0) if 20000 <= when < 30000 else (X - 5.0, Y)
    units = [[MAIN, *main, Z, 0.0, 100.0, 100.0, 0, 0, 0, 0, 0]]
    units += [[guid, *spot, Z, 0.0, 100.0, 100.0, 0, 0, 0, 0, 0] for guid, spot in SPOTS.items()]
    boss = SUBMERGE_AT if 28000 <= when < 50000 else (X - 5.0, Y + 2.0)
    target = MAIN if 1000 <= when < 30000 or when >= 51000 else 0
    units.append([BOSS, *boss, Z, 0.0, 100.0 - max(when, 0) / 1000.0, 0.0, target, 0, 0, 0, 0])
    if 31000 <= when < 48000:
        spike = SPIKE_ROGUE if when < 40000 else SPIKE_HUNTER
        units.append([SPK, *spike, Z, 0.0, 100.0, 0.0, ROGUE if when < 40000 else HUNTER, 0, 0, 0, 0])
    hz = []
    for guid in (G0, G1, K1, K2, IDLE):
        place = refine_sphere(guid, when)
        if place is None:
            continue
        (x, y), z = place
        units.append([guid, x, y, z, 0.0, 100.0, 0.0, 0, 0, 0, 0, 0])
        if z == PATCH_Z:
            hz.append([PERMAFROST_25H, x, y, PATCH_Z, 6.0, 1])
    for guid, (x, y) in {**WAVE1, **WAVE2}.items():
        if (guid in WAVE1 and 5000 <= when <= 44500) or (guid in WAVE2 and when >= 50000):
            units.append([guid, x, y, Z, 0.0, 100.0, 0.0, refine_burrower_target(guid, when), 0, 0, 0, 0])
    return {"t": when, "e": "snap", "u": units, "hz": hz}


def triggered(when: int, source: int, spell: int, target: int) -> dict:
    return {**cast(when, source, spell, target), "tr": 1}


def refine_pull() -> list[dict]:
    """A 25H pull on the new probes: P1 to 30 s, P2 to 50 s, then up again, burrower waves at 5 and 50 s."""
    records = [
        {"e": "hdr", "v": 13, "ts": 1789500000000, "map": ab.MAP_TOC, "inst": 5, "diff": 3, "boss": "anub-arak",
         "roster": [
             {"g": MAIN, "n": "Bulwark", "r": "tank", "c": "warrior", "h": 0},
             {"g": SIDE0, "n": "Ecoterrorist", "r": "tank", "c": "druid", "h": 0},
             {"g": SIDE1, "n": "Aegis", "r": "tank", "c": "paladin", "h": 0},
             {"g": DK, "n": "Deathsong", "r": "melee", "c": "deathknight", "h": 0},
             {"g": MAGE, "n": "Frostfire", "r": "ranged", "c": "mage", "h": 0},
             {"g": LOCK, "n": "Hellflame", "r": "ranged", "c": "warlock", "h": 0},
             {"g": ROGUE, "n": "Shadowstep", "r": "melee", "c": "rogue", "h": 0},
             {"g": DRUID, "n": "Druidica", "r": "ranged", "c": "druid", "h": 0},
             {"g": HUNTER, "n": "Trueshot", "r": "ranged", "c": "hunter", "h": 0}]},
        {"t": 0, "e": "pull", "boss": "anub-arak", "src": "engage"},
        {"t": -1000, "e": "unit", "g": BOSS, "en": ab.NPC_ANUBARAK, "n": "Anub'arak", "b": 1},
        {"t": -1000, "e": "unit", "g": IMP, "en": 416, "n": "Ruirin", "own": LOCK},
        {"t": 30000, "e": "unit", "g": SPK, "en": ab.NPC_SPIKE, "n": "Anub'arak", "b": 1},
        *({"t": 0, "e": "unit", "g": guid, "en": ab.NPC_FROST_SPHERE, "n": "Frost Sphere"}
          for guid in (G0, G1, K1, K2, IDLE)),
        *({"t": 5000, "e": "unit", "g": guid, "en": ab.NPC_BURROWER, "n": "Nerubian Burrower"} for guid in WAVE1),
        *({"t": 50000, "e": "unit", "g": guid, "en": ab.NPC_BURROWER, "n": "Nerubian Burrower"} for guid in WAVE2),
        *({"e": "spell", "sp": spell, "n": name} for spell, name in (
            (42842, "Frostbolt"), (47964, "Firebolt"), (47813, "Corruption"), (53190, "Starfall"))),

        note(0, MAIN, "anub.phase", 1),
        note(30000, MAIN, "anub.phase", 2),
        note(50000, MAIN, "anub.phase", 1),
        # a cleared latch restated at the trace's open
        note(-500, MAIN, "anub.side0", 0),
        note(0, MAIN, "anub.side0", SIDE0),
        note(0, SIDE1, "anub.side1", SIDE1),
        note(0, MAIN, "anub.patch0", 0),
        note(7000, DK, "anub.patch0", G0),
        note(0, MAIN, "anub.patch1", 0),
        note(5000, SIDE0, "anub.wave", 1),
        note(50000, SIDE1, "anub.wave", 2),
        note(0, MAIN, "anub.focusdue", 0),
        note(20000, MAGE, "anub.focusdue", 1),
        note(45000, MAGE, "anub.focusdue", 0),
        note(55000, MAGE, "anub.focusdue", 1),
        note(0, DK, "anub.grip", 1),
        note(11000, DK, "anub.grip", 2),
        note(30000, MAIN, "anub.allow", 1),

        # BN2 moves from side 0 to side 1, BS2 never gets a row so its exit decides
        note(5000, BN1, "anub.bside", 0),
        note(5000, BN2, "anub.bside", 0),
        note(8000, BN2, "anub.bside", 1),
        note(5000, BS1, "anub.bside", 1),
        note(50000, WN, "anub.bside", 0),
        note(50000, WS, "anub.bside", 1),

        note(3000, DK, "anub.sphere", "grip0"),
        note(7500, DK, "anub.sphere", "grip1"),
        note(11000, DK, "anub.sphere", "none"),
        note(11000, LOCK, "anub.sphere", 1),
        note(13000, LOCK, "anub.sphere", "none"),
        note(30500, MAGE, "anub.sphere", "kite"),
        note(33000, MAGE, "anub.sphere", "none"),
        *(note(when, DK, "anub.gripstep", step) for when, step in (
            (3000, "walk"), (3900, "cast"), (4500, "kill"), (6500, "none"), (7500, "walk"), (7900, "cast"),
            (11000, "none"))),
        note(0, SIDE0, "anub.sidetank", "ready"),
        note(7000, SIDE0, "anub.sidetank", "pickup"),
        note(8000, SIDE0, "anub.sidetank", "hold"),
        note(0, SIDE1, "anub.sidetank", "ready"),
        note(5000, SIDE1, "anub.sidetank", "pickup"),
        note(1000, MAIN, "anub.pickup", "attack"),
        note(25000, MAIN, "anub.pickup", "stepout"),
        note(26000, MAIN, "anub.pickup", "hold"),
        note(30500, MAGE, "anub.gather", "walk"),
        note(32000, MAGE, "anub.gather", "hold"),
        note(50000, MAGE, "anub.gather", "none"),

        # G0 takes a hit from the mage, then a later shadow bolt cast: the hit is the killer
        cast(4000, DK, ab.SPELL_DEATH_GRIP, G0),
        cast(4500, MAGE, 42842, G0),
        dmg(4700, MAGE, G0, 42842, 9000),
        cast(4800, LOCK, 47809, G0),
        # G1: the grip misses, then a proc and a Hunter's Mark after the imp's bolt are no killer
        cast(8000, DK, ab.SPELL_DEATH_GRIP, G1),
        cast(12000, IMP, 47964, G1),
        triggered(12200, MAGE, 12579, G1),
        cast(12300, HUNTER, 53338, G1),
        cast(31000, LOCK, 47813, K1),
        cast(32000, MAGE, 42842, K1),
        triggered(36000, DRUID, 53190, K2),
        triggered(36100, MAGE, 12579, K2),

        aura(31000, ROGUE, ab.SPELL_MARK, False),
        *(note(when, ROGUE, "anub.kitestage", stage) for when, stage in (
            (31000, "run"), (33000, "boost:11305"), (35000, "immune"), (39000, "patch"), (40000, "none"))),
        cast(33000, ROGUE, 11305, ROGUE),
        cast(35000, SIDE1, 10278, ROGUE),
        aura(40000, ROGUE, ab.SPELL_MARK, True),
        # one Nitro Boosts read three times: the use, its triggered speed and the aura
        aura(41000, HUNTER, ab.SPELL_MARK, False),
        *(note(when, HUNTER, "anub.kitestage", stage) for when, stage in (
            (41000, "run"), (42000, "boost:55004"), (46000, "patch"), (48000, "none"))),
        cast(42000, HUNTER, 55004, HUNTER),
        triggered(42010, HUNTER, 54861, HUNTER),
        aura(42020, HUNTER, 54861, False, HUNTER),
        cast(44000, HUNTER, 5384, HUNTER),
        aura(48000, HUNTER, ab.SPELL_MARK, True),
        {"t": 60000, "e": "end", "out": "wipe"},
    ]
    records += [refine_snap(when) for when in range(-1000, 60000, 500)]
    return records


class RefinedPull(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = write_trace(cls.folder.name, "649_5_anub-arak_1789500002.ndjson", refine_pull())

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_each_burrower_reads_its_exit_and_side_at_the_spawn(self):
        rows = {row["guid"]: row for row in ab.pickup_rows(self.trace)}
        self.assertEqual({guid: (row["wave"], row["exit"], row["side"], row["probed"]) for guid, row in rows.items()},
                         {BN1: (1, EXIT_N1, 0, True), BN2: (1, EXIT_N2, 0, True), BS1: (1, EXIT_S1, 1, True),
                          BS2: (1, EXIT_S2, 1, False), WN: (2, EXIT_N1, 0, True), WS: (2, EXIT_S1, 1, True)})

    def test_every_owner_in_turn(self):
        rows = {row["guid"]: row for row in ab.pickup_rows(self.trace)}
        self.assertEqual(rows[BN2]["owners"], [0, 1])
        self.assertEqual(rows[BS2]["owners"], [1])

    def test_pickup_times_any_tank_apart_from_its_owning_side_tank(self):
        rows = {row["guid"]: row for row in ab.pickup_rows(self.trace)}
        # BN2's side 1 owns it from 8 s, when its side tank picks it up at 9 s
        self.assertEqual({guid: (row["to_tank"], row["to_side"], row["tank"]) for guid, row in rows.items()},
                         {BN1: (2000, 2000, SIDE0), BN2: (1000, 4000, SIDE0), BS1: (0, 0, SIDE1),
                          BS2: (None, None, SIDE1), WN: (500, 500, SIDE0), WS: (500, 500, SIDE1)})

    def test_the_side_tank_is_measured_from_its_patch_else_its_spot(self):
        rows = {row["guid"]: row for row in ab.pickup_rows(self.trace)}
        self.assertEqual((rows[BN1]["from"], round(rows[BN1]["gap"], 2)), ("spot", 5.0))
        self.assertEqual((rows[BS1]["from"], round(rows[BS1]["gap"], 2)), ("spot", 2.0))
        self.assertEqual((rows[WN]["from"], round(rows[WN]["gap"], 2)), ("patch", 4.0))
        # side 1 has no patch, so its spot hangs off side 0's
        self.assertEqual(rows[WS]["from"], "spot")
        self.assertAlmostEqual(rows[WS]["gap"], math.dist(SPOTS[SIDE1], (PATCH0[0], PATCH0[1] - GAP)))

    def test_focus_due_per_wave_against_the_next_spawn(self):
        waves = ab.wave_starts(self.trace)
        self.assertEqual(waves, [(1, 5000), (2, 50000)])
        self.assertEqual(ab.focus_due_rows(self.trace, waves),
                         [{"wave": 1, "spawn": 5000, "due": 20000, "next": 50000},
                          {"wave": 2, "spawn": 50000, "due": 55000, "next": None}])

    def test_focus_due_only_after_the_next_wave_counts_as_never(self):
        rows = ab.focus_due_rows(self.trace, [(1, 5000), (2, 10000), (3, 50000)])
        self.assertEqual([row["due"] for row in rows], [None, 20000, 55000])

    def test_each_dead_sphere_names_its_killer(self):
        rows = ab.sphere_deaths(self.trace)
        self.assertEqual([(row["guid"], row["killed"], row["killer"], row["pet"], row["spell"], row["source"],
                           row["shape"]) for row in rows],
                         [(G0, 5000, MAGE, None, 42842, "dmg", ()),
                          (G1, 12500, LOCK, IMP, 47964, "cast", ()),
                          (K1, 32500, MAGE, None, 42842, "cast", ()),
                          (K2, 36500, DRUID, None, 53190, "cast", ("area",))])

    def test_a_sphere_lands_against_its_sides_spot(self):
        rows = {row["guid"]: row for row in ab.sphere_deaths(self.trace)}
        self.assertEqual({guid: (row["side"], row["latched"], row["duty"]) for guid, row in rows.items()},
                         {G0: (0, True, None), G1: (1, False, "1"), K1: (None, False, "kite"),
                          K2: (None, False, None)})
        self.assertAlmostEqual(rows[G0]["off_spot"], 3.0)
        # G0 had latched by then, so side 1's spot is GAP off it
        self.assertAlmostEqual(rows[G1]["off_spot"], 2.0)
        self.assertIsNone(rows[K1]["off_spot"])

    def test_a_grip_lands_only_within_reach_of_the_dk_in_time(self):
        rows = ab.grip_rows(self.trace)
        self.assertEqual([(row["t"], row["sphere"], row["duty"], row["landed"]) for row in rows],
                         [(4000, G0, "grip0", 500), (8000, G1, "grip1", None)])
        self.assertAlmostEqual(rows[1]["closest"], math.hypot(17.0, 20.0 + O + 0.5))

    def test_a_grip_reads_the_side_it_served_and_the_side_that_latched_it(self):
        self.assertEqual([(row["served"], row["latched"]) for row in ab.grip_rows(self.trace)],
                         [(0, 0), (1, None)])

    def test_a_side_without_a_tank_patch_while_burrowers_are_out(self):
        # side 0 latches 2 s after wave 1; side 1 never does, so all of wave 1's P1 and P1b count
        self.assertEqual(ab.unpatched_ms(self.trace, ab.Samples(self.trace)), {0: 2000, 1: 34500})

    def test_covered_counts_overlaps_once(self):
        self.assertEqual(ab.covered([(0, 10), (5, 20), (30, 40)], 2, 35), 23)
        self.assertEqual(ab.covered([(0, 10)], 20, 30), 0)

    def test_spheres_spent_per_submerge_against_the_allowance(self):
        # G0 outlives the trace, so no tank patch goes and the two kills under him are no spend
        self.assertEqual(ab.allowance_rows(self.trace, ab.sphere_deaths(self.trace), ab.spent_patches(self.trace)),
                         [{"label": "P2", "start": 30000, "allow": 1, "dropped": None, "tank": 0, "other": 2,
                           "spent": [], "over": False}])

    def test_kite_stages_and_boosts_per_window(self):
        windows = ab.mark_windows(self.trace)
        self.assertEqual([window["stages"] for window in windows],
                         [{"run": 2000, "boost:11305": 2000, "immune": 4000, "patch": 1000},
                          {"run": 1000, "boost:55004": 4000, "patch": 2000}])
        self.assertEqual([[(rec["t"], rec["sp"]) for rec in window["boosts"]] for window in windows],
                         [[(33000, 11305)], [(42000, 55004)]])
        self.assertEqual([[rec["sp"] for rec in window["own"]] for window in windows], [[], [5384]])

    def test_the_spikes_closest_approach_to_each_latched_tank_patch(self):
        # side 1 never latches, so only side 0 reads
        windows = ab.mark_windows(self.trace)
        self.assertEqual([{side: round(yards, 2) for side, yards in window["tank_near"].items()} for window in windows],
                         [{0: 9.0}, {0: round(math.dist(SPIKE_HUNTER, PATCH0), 2)}])

    def test_main_tank_share_in_a_patch_and_the_submerge_gaps(self):
        rows = ab.threat_rows(self.trace)
        self.assertEqual([(row["label"], row["main"], row["in_patch"], row["living"]) for row in rows],
                         [("P1", MAIN, 20, 60), ("P1b", MAIN, 0, 20)])
        self.assertEqual(rows[0]["submerge"]["spot"], SUBMERGE_AT)
        self.assertEqual({side: round(yards, 1) for side, yards in rows[0]["submerge"]["patches"].items()}, {0: 12.0})
        self.assertIsNone(rows[1]["submerge"])

    def test_both_bot_side_tanks_latched(self):
        self.assertEqual(ab.side_tanks(self.trace), (2, {0: [SIDE0], 1: [SIDE1]}))

    def test_every_section_prints(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            ab.show_banner(self.trace)
            for _, _, section in ab.SECTIONS:
                section(self.trace)
        text = out.getvalue()
        self.assertNotIn("side tanks latched", text)
        self.assertRegex(text, r"#911 .* Frostfire +Frostbolt 42842 +dmg +- +0 latched +3\.0 yd")
        self.assertRegex(text, r"#912 .* Hellflame's pet +Firebolt 47964 +cast +- +1 +2\.0 yd")
        self.assertIn("periodic or area spells aimed at a sphere: Hellflame Corruption 47813 1,"
                      " Druidica Starfall 53190 1", text)
        self.assertIn("P2     0:30.000  allowance  1  tank 0  other 2  spent 0\n", text)
        self.assertIn("anub.grip: gripping from 0:00.000, shooting from 0:11.000", text)
        self.assertRegex(text, r"Deathsong +grip1 +#912 +missed, closest \d+\.\d yd\n")
        self.assertRegex(text, r"Deathsong +grip0 +#911 +landed \+0\.5 s, latched side 0\n")
        self.assertIn("no tank patch with burrowers out while he was up: side 1 34.5 s", text)
        self.assertRegex(text, r"#922 +1 N 694\.5,185\.4 0>1 ")
        self.assertIn("kitestage run 2.0 s, boost Sprint 2.0 s, immune 4.0 s, patch 1.0 s; boosts Sprint +2.0 s",
                      text)
        self.assertIn("kitestage run 1.0 s, boost Nitro Boosts 4.0 s, patch 2.0 s; boosts Nitro Boosts +1.0 s", text)
        self.assertIn("Feign Death +3.0 s unasked", text)
        self.assertIn("own immunity on a kiter anub.protect never asked for: Feign Death 1", text)
        self.assertIn("wave 1 0:05.000: focus due 0:20.000, 30.0 s before wave 2 at 0:50.000", text)
        self.assertIn("wave 2 0:50.000: focus due 0:55.000, the last wave", text)
        self.assertIn("main tank Bulwark in a patch 33% of 60 samples", text)
        self.assertIn(f"submerged 0:30.000 at {SUBMERGE_AT[0]:.1f}, {SUBMERGE_AT[1]:.1f}: patch0 12.0 yd under",
                      text)
        self.assertIn("anub.pickup: Bulwark attack 1, stepout 1, hold 1", text)
        self.assertIn("spike to tank patch: side 0 9.0 yd in reach\n", text)
        self.assertIn("spike within 11.5 yd of a latched tank patch, its fail test's reach: 1 window(s)", text)
        self.assertIn("side 1  never latched one\n", text)


T0, R1, KA, KB = (CREATURE | n for n in range(951, 955))
BA, BB, BC, BD = (CREATURE | n for n in range(961, 965))
SPK2 = CREATURE | 942
TP0, TP1 = (X + 3.0, Y + O), (X + 3.0, Y - O)
R1_FLY = (X + 20.0, Y - 25.0)
KA_FLY = (X - 30.0, Y + 25.0)
DK_CAST = (X - 26.0, Y + 12.0)   # 20 yd off the first submerge's gather centre
DK_AFTER = (X - 20.0, Y + 12.0)  # 6 yd on from the cast
KB_SPOT = (X - 25.0, Y - 15.0)
SLOTS = {MAGE: (X - 13.0, Y), DRUID: (X - 7.0, Y), LOCK: (X - 10.0, Y + 3.0), HUNTER: (X - 10.0, Y - 3.0),
         ROGUE: (X - 10.0, Y + 3.0)}
# kiter, mark on, mark off, and where the spike sits meanwhile
KITES = ((ROGUE, 11000, 17000, (X + 2.0, Y)), (HUNTER, 18000, 25000, (X - 10.0, Y + 13.0)),
         (MAGE, 42000, 46000, (X + 17.0, Y - 20.0)), (LOCK, 46500, 52000, (X - 10.0, Y + 11.0)),
         (ROGUE, 52500, 53000, (X - 10.0, Y - 10.0)))
# guid, exit, first and last sample, emerge
BURROWERS = ((BA, EXIT_N1, 2000, 20000, None), (BB, EXIT_N2, 2000, 50000, 36000), (BC, EXIT_S1, 2000, 40000, None),
             (BD, EXIT_S2, 46000, 60000, None))


def kite_player(guid: int, when: int) -> list:
    """Mage, druid, warlock and hunter hold the gather in the first submerge, mage, druid and rogue in the
    second. The hunter dies on its kite at 25 s and the warlock stands off in the second submerge."""
    spot, hp = {MAIN: (X - 5.0, Y), SIDE0: (TP0[0], TP0[1] + 2.0), SIDE1: (TP1[0], TP1[1] - 2.0),
                DRUID: SLOTS[DRUID]}.get(guid, (X + 10.0, Y)), 100.0
    if guid == DK:
        spot = DK_AFTER if when >= 15500 else DK_CAST if when >= 13000 else (X - 5.0, Y + 5.0)
    elif guid == MAGE:
        spot = (X + 20.0, Y - 20.0) if 42000 < when <= 46000 else SLOTS[MAGE]
    elif guid == LOCK:
        spot = (X + 15.0, Y + 15.0) if when >= 30000 else SLOTS[LOCK]
    elif guid == HUNTER:
        spot = (X - 10.0, Y + 16.0) if when > 18000 else SLOTS[HUNTER]
        hp = 0.0 if when >= 25000 else 100.0
    elif guid == ROGUE and 40000 <= when < 52500:
        spot = SLOTS[ROGUE]
    return [guid, *spot, Z, 0.0, hp, 100.0, 0, 0, 0, 0, 0]


def kite_sphere(guid: int, when: int):
    """`(xy, z)` or None. T0 lies on side 0's spot from the pull. In the first submerge R1 lands on side 1's
    spot and latches, the DK grips KA at 15 s, which dies where the DK stood, and KB is shot down after."""
    if when < 0:
        return None
    if guid == T0:
        return TP0, PATCH_Z
    if guid == R1:
        return (R1_FLY, FLYING_Z) if when < 12500 else (TP1, PATCH_Z if when >= 14000 else FALLING_Z)
    if guid == KA:
        return (KA_FLY, FLYING_Z) if when <= 15000 else (DK_CAST, PATCH_Z if when >= 17500 else Z + 0.5)
    return KB_SPOT, FLYING_Z if when < 20000 else PATCH_Z if when >= 21500 else FALLING_Z


def kite_snap(when: int) -> dict:
    units = [kite_player(guid, when) for guid in (MAIN, SIDE0, SIDE1, DK, MAGE, LOCK, ROGUE, DRUID, HUNTER)]
    units.append([BOSS, X - 5.0, Y + 2.0, Z, 0.0, 100.0 - max(when, 0) / 1000.0, 0.0, MAIN, 0, 0, 0, 0])
    units += [[SPK2, *spot, Z, 0.0, 100.0, 0.0, kiter, 0, 0, 0, 0]
              for kiter, start, stop, spot in KITES if start <= when <= stop]
    hz = []
    for guid in (T0, R1, KA, KB):
        place = kite_sphere(guid, when)
        if place is None:
            continue
        (x, y), z = place
        units.append([guid, x, y, z, 0.0, 100.0, 0.0, 0, 0, 0, 0, 0])
        if z == PATCH_Z:
            hz.append([PERMAFROST_25H, x, y, PATCH_Z, 6.0, 1])
    for guid, spot, first, last, emerge in BURROWERS:
        if first <= when <= last:
            up = emerge if emerge and when >= emerge else first
            units.append([guid, *spot, Z, 0.0, 100.0 - (when - up) / 1000.0, 0.0, 0, 0, 0, 0, 0])
    return {"t": when, "e": "snap", "u": units, "hz": hz}


def kite_pull() -> list[dict]:
    """A 25H pull: P1 to 10 s, P2 to 30 s, P1b to 40 s, P2b to 55 s. The marks end on a Vanish and a death
    near the gather, an Ice Block far from it, a patch, and a Vanish as the emerge takes the spike."""
    records = [
        {"e": "hdr", "v": 13, "ts": 1789500000000, "map": ab.MAP_TOC, "inst": 7, "diff": 3, "boss": "anub-arak",
         "roster": [
             {"g": MAIN, "n": "Bulwark", "r": "tank", "c": "warrior", "h": 0},
             {"g": SIDE0, "n": "Ecoterrorist", "r": "tank", "c": "druid", "h": 0},
             {"g": SIDE1, "n": "Aegis", "r": "tank", "c": "paladin", "h": 0},
             {"g": DK, "n": "Deathsong", "r": "melee", "c": "deathknight", "h": 0},
             {"g": MAGE, "n": "Frostfire", "r": "ranged", "c": "mage", "h": 0},
             {"g": LOCK, "n": "Hellflame", "r": "ranged", "c": "warlock", "h": 0},
             {"g": ROGUE, "n": "Shadowstep", "r": "melee", "c": "rogue", "h": 0},
             {"g": DRUID, "n": "Druidica", "r": "ranged", "c": "druid", "h": 0},
             {"g": HUNTER, "n": "Trueshot", "r": "ranged", "c": "hunter", "h": 0}]},
        {"t": 0, "e": "pull", "boss": "anub-arak", "src": "engage"},
        {"t": -500, "e": "unit", "g": BOSS, "en": ab.NPC_ANUBARAK, "n": "Anub'arak", "b": 1},
        {"t": 11000, "e": "unit", "g": SPK2, "en": ab.NPC_SPIKE, "n": "Anub'arak", "b": 1},
        *({"t": 0, "e": "unit", "g": guid, "en": ab.NPC_FROST_SPHERE, "n": "Frost Sphere"}
          for guid in (T0, R1, KA, KB)),
        *({"t": first, "e": "unit", "g": guid, "en": ab.NPC_BURROWER, "n": "Nerubian Burrower"}
          for guid, _, first, _, _ in BURROWERS),
        *({"e": "spell", "sp": spell, "n": name} for spell, name in (
            (42842, "Frostbolt"), (47809, "Shadow Bolt"), (75, "Auto Shot"))),

        *(note(when, MAIN, "anub.phase", value) for when, value in ((0, 1), (10000, 2), (30000, 1), (40000, 2),
                                                                     (55000, 1))),
        note(0, MAIN, "anub.patch0", T0),
        note(0, MAIN, "anub.patch1", 0),
        note(13900, SIDE1, "anub.patch1", R1),
        note(13000, DK, "anub.grip", 1),
        note(16000, DK, "anub.grip", 0),
        note(13000, DK, "anub.sphere", "gripk"),
        note(16000, DK, "anub.sphere", "none"),
        note(15500, MAGE, "anub.sphere", "kite"),
        note(16500, MAGE, "anub.sphere", "none"),
        *(note(when, DK, "anub.gripstep", step) for when, step in (
            (13000, "walk"), (14900, "cast"), (15500, "kill"), (16000, "none"))),
        *(note(when, guid, "anub.gather", value) for guid in (MAGE, DRUID, LOCK, HUNTER)
          for when, value in ((10200, "walk"), (11000, "hold"))),
        *(note(when, guid, "anub.gather", value) for guid in (MAGE, DRUID, ROGUE)
          for when, value in ((40200, "walk"), (41000, "hold"))),

        cast(12500, HUNTER, 75, R1),
        cast(15000, DK, ab.SPELL_DEATH_GRIP, KA),
        cast(16000, MAGE, 42842, KA),
        dmg(16000, MAGE, KA, 42842, 9000),
        cast(20000, LOCK, 47809, KB),

        *(aura(when, kiter, ab.SPELL_MARK, removed, SPK2) for kiter, start, stop, _ in KITES
          for when, removed in ((start, False), (stop, True))),
        {"t": 25000, "e": "death", "g": HUNTER, "killer": SPK2, "x": X - 10.0, "y": Y + 16.0, "z": Z,
         "blow": [SPK2, 20000]},
        cast(17000, ROGUE, 26889, ROGUE),
        cast(45900, MAGE, 45438, MAGE),
        cast(53100, ROGUE, 26889, ROGUE),
        {"t": 57000, "e": "end", "out": "wipe"},
    ]
    records += [kite_snap(when) for when in range(-500, 57000, 500)]
    return records


class KitePull(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = write_trace(cls.folder.name, "649_7_anub-arak_1789500004.ndjson", kite_pull())

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def holds(self):
        return ab.holder_spans(self.trace, "anub.gather", ab.pull_end(self.trace))

    def centre(self, when: int, skip=frozenset()):
        return ab.gather_centre(self.holds(), ab.Samples(self.trace), ab.submerged_spans(self.trace), when, skip)

    def assertSpot(self, spot, expected):
        self.assertIsNotNone(spot)
        self.assertAlmostEqual(spot[0], expected[0])
        self.assertAlmostEqual(spot[1], expected[1])

    def test_a_grip_lands_against_the_dks_spot_at_the_cast_not_where_it_walked_after(self):
        # the DK is 6 yd on by the first sample after the cast, past the 3 yd test
        rows = ab.grip_rows(self.trace)
        self.assertEqual([(row["sphere"], row["duty"], row["cast_at"], row["landed"], row["served"]) for row in rows],
                         [(KA, "gripk", DK_CAST, 500, None)])
        self.assertAlmostEqual(rows[0]["closest"], 0.0)

    def test_the_gather_centre_is_the_mean_spot_of_the_bots_holding_it(self):
        self.assertSpot(self.centre(17000), (X - 10.0, Y))
        self.assertSpot(self.centre(17000, {MAGE}), (X - 9.0, Y))

    def test_the_gather_centre_leaves_out_the_dead_and_holds_from_an_earlier_submerge(self):
        # the dead hunter, then the warlock standing off on its first submerge's hold
        self.assertSpot(self.centre(28000), (X - 10.0, Y + 1.0))
        self.assertSpot(self.centre(41500), (X - 10.0, Y + 1.0))

    def test_the_gather_centre_is_none_with_nobody_holding_or_while_he_is_up(self):
        self.assertIsNone(self.centre(10500))
        self.assertIsNone(self.centre(35000))

    def test_each_mark_end_reads_the_spike_against_the_gather_and_the_kite_patches_alive(self):
        windows = ab.mark_windows(self.trace)
        self.assertEqual([window["guid"] for window in windows], [ROGUE, HUNTER, MAGE, LOCK, ROGUE])
        self.assertEqual([round(window["to_gather"], 2) for window in windows],
                         [12.0, 12.0, round(math.hypot(25.5, 21.5), 2), 10.0, 10.0])
        self.assertEqual([window["kite_patches"] for window in windows], [[], *[[KA, KB]] * 4])

    def test_a_kite_ended_by_a_death_or_an_own_immunity_near_the_gather_is_flagged(self):
        windows = ab.mark_windows(self.trace)[:4]
        self.assertEqual([(window["ended"], window["near_gather"]) for window in windows],
                         [("Vanish", True), ("death", True), ("Ice Block", False), (None, False)])

    def test_an_immunity_as_the_emerge_takes_the_spike_ends_nothing(self):
        window = ab.mark_windows(self.trace)[4]
        self.assertEqual((window["outcome"], window["ended"], window["near_gather"]), ("emerge", None, False))

    def test_a_burrower_dies_inside_30_s_of_its_first_sight_or_its_last_emerge(self):
        rows = {row["guid"]: row for row in ab.burrower_windows(self.trace)}
        self.assertEqual({guid: (row["wave"], row["up"], row["emerged"], row["died"], row["inside"])
                          for guid, row in rows.items()},
                         {BA: (1, 2000, False, 20000, True), BB: (1, 36000, True, 50000, True),
                          BC: (1, 2000, False, 40000, False), BD: (2, 46000, False, None, None)})

    def test_every_section_prints(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            ab.show_banner(self.trace)
            for _, _, section in ab.SECTIONS:
                section(self.trace)
        text = out.getvalue()
        self.assertRegex(text, r"Deathsong +gripk +#953 +landed \+0\.5 s\n")
        self.assertIn("at the end: spike 12.0 yd from the gather; no kite patch; ended by Vanish within 20 yd of"
                      " the gather\n", text)
        self.assertIn("at the end: spike 33.4 yd from the gather; kite patch #953, #954; ended by Ice Block\n", text)
        self.assertIn("kite ended by a death or an own immunity within 20 yd of the gather: Shadowstep 0:17.000"
                      " Vanish, Trueshot 0:25.000 death\n", text)
        self.assertRegex(text, r"#962 +1 +0:36\.000 emerge +0:50\.000 +14\.0 s yes\n")
        self.assertRegex(text, r"#964 +2 +0:46\.000 sight +alive +- -\n")
        self.assertIn("wave 1: 2 of 3 dead inside, 1 later, 0 alive\n", text)
        self.assertIn("wave 2: 0 of 1 dead inside, 0 later, 1 alive\n", text)


Q0, Q1, QR, QA, QB = (CREATURE | n for n in range(971, 976))
U1, U2, U3 = (CREATURE | n for n in range(981, 984))
QR_LAND = (X + 3.0, Y + O - GAP + 1.0)  # 1 yd off side 1's spot while side 0's patch is latched
RIM0 = (PATCH0[0] + 7.0, PATCH0[1])
RIM1 = (G1_LAND[0] + 7.0, G1_LAND[1])
RIM1B = (QR_LAND[0] + 7.0, QR_LAND[1])
U3_SPOT = (X - 3.0, Y)  # 2 yd off the main tank
IN_PATCH0 = (PATCH0[0] + 3.0, PATCH0[1])


def frost_phase(when: int) -> int:
    return 2 if 20000 <= when < 40000 or when >= 55000 else 1


def frost_sphere(guid: int, when: int):
    """`(xy, z)` or None. The DK grips Q0 at 3 s, the mage kills it at 4 s and it lies on side 0's spot until
    the hunter's kite spends it at 64 s; the hunter shoots Q1 down at 8 s and the rogue's kite spends it at
    31 s; the DK grips QR onto side 1's spot at 43 s and the mage kills it at 44 s."""
    if when < 0:
        return None
    if guid == Q0:
        if when > 64000:
            return None
        if when < 3500:
            return (X + 10.0, Y + 20.0), FLYING_Z
        return PATCH0, PATCH_Z if when >= 5500 else FALLING_Z if when >= 4000 else Z + 0.5
    if guid == Q1:
        if when > 31000:
            return None
        return (G1_FLY, FLYING_Z) if when < 8000 else (G1_LAND, PATCH_Z if when >= 9500 else FALLING_Z)
    if guid == QR:
        if when < 43500:
            return (X + 20.0, Y - 30.0), FLYING_Z
        return QR_LAND, PATCH_Z if when >= 45500 else FALLING_Z if when >= 44000 else Z + 0.5
    return (X + (5.0 if guid == QB else 0.0), Y + 30.0), FLYING_Z


def frost_player(guid: int, when: int) -> list:
    """Side 0's tank waits in the gather through the first submerge, side 1's once its patch is spent."""
    spot, target = (X - 12.0, Y), 0
    if guid == MAIN:
        spot = (X - 5.0, Y)
    elif guid == SIDE0:
        spot = (X - 10.0, Y + 1.0) if 20500 <= when < 40500 else RIM0
    elif guid == SIDE1:
        spot = RIM1 if when < 31000 else (X - 10.0, Y - 1.0) if when < 45500 else RIM1B
    elif guid == DK:
        spot = PATCH0 if 2500 <= when < 4500 else QR_LAND if 42500 <= when < 44500 else (X - 8.0, Y)
    elif guid == MAGE:
        target = BOSS
    elif guid == ROGUE:
        spot, target = ((PATCH0[0] + 2.0, PATCH0[1] + 3.0), U1) if 9000 <= when <= 14000 else ((X - 20.0, Y), BOSS)
    elif guid == HUNTER:
        spot = IN_PATCH0 if 56500 <= when <= 58000 else (X - 16.0, Y)
    elif guid == HEAL:
        spot = (X - 14.0, Y)
    elif guid == MELEE:
        spot = (X - 18.0, Y)
    return [guid, *spot, Z, 0.0, 100.0, 100.0, target, 0, 0, 0, 0]


def frost_burrowers(when: int) -> list[list]:
    """U1 walks from its exit onto side 0's tank at 9 s, 4 yd off the patch; U2 reaches side 1's tank at 10 s,
    5 yd off the patch, then 3 yd from 12.5 s; U3 sits on the main tank and comes back full at 49 s."""
    rows = []
    if 6000 <= when <= 14000:
        spot = EXIT_N1 if when < 9000 else (PATCH0[0] + 4.0, PATCH0[1])
        rows.append([U1, *spot, Z, 0.0, 100.0 - (when - 6000) / 100.0, 0.0, SIDE0, 0, 0, 0, 0])
    if 6000 <= when <= 15000:
        spot = EXIT_S1 if when < 10000 else (G1_LAND[0] + (5.0 if when <= 12000 else 3.0), G1_LAND[1])
        rows.append([U2, *spot, Z, 0.0, 100.0 - (when - 6000) / 200.0, 0.0, SIDE1, 0, 0, 0, 0])
    if 41000 <= when <= 44000 or 49000 <= when <= 53000:
        hp = 100.0 - (when - 41000) / 100.0 if when <= 44000 else 100.0 - (when - 49000) / 1000.0
        rows.append([U3, *U3_SPOT, Z, 0.0, hp, 0.0, MAIN, 0, 0, 0, 0])
    return rows


def frost_snap(when: int) -> dict:
    units = [frost_player(guid, when) for guid in (MAIN, SIDE0, SIDE1, DK, MAGE, ROGUE, HUNTER, HEAL, MELEE)]
    units.append([BOSS, X - 5.0, Y + 2.0, Z, 0.0, 100.0 - max(when, 0) / 1000.0, 0.0,
                  MAIN if when >= 0 and frost_phase(when) == 1 else 0, 0, 0, 0, 0])
    hz = []
    for guid in (Q0, Q1, QR, QA, QB):
        place = frost_sphere(guid, when)
        if place is None:
            continue
        (x, y), z = place
        units.append([guid, x, y, z, 0.0, 100.0, 0.0, 0, 0, 0, 0, 0])
        if z == PATCH_Z:
            hz.append([PERMAFROST_25H, x, y, PATCH_Z, 6.0, 1])
    units += frost_burrowers(when)
    return {"t": when, "e": "snap", "u": units, "hz": hz}


def frost_pull() -> list[dict]:
    """A 25H pull: P1 to 20 s, P2 to 40 s, P1b to 55 s, P2b to the end at 70 s, burrower waves at 6 and 41 s.
    The rogue's kite spends side 1's patch and a grip brings a new one 5.5 s after the emerge; the hunter,
    marked inside side 0's patch with no allowance left, spends that one."""
    # guid, patch, on, off; None runs to the end
    stints = ((SIDE0, Q0, 5500, 20500), (SIDE0, Q0, 40500, 64000), (SIDE1, Q1, 9500, 31000), (SIDE1, QR, 45500, None),
              (MAIN, Q0, 12000, 14000), (ROGUE, Q0, 10000, 16000), (MAGE, Q0, 18000, 22000), (HEAL, Q0, 25000, 27000),
              (MELEE, Q0, 11000, 15000), (HUNTER, Q0, 56500, 58000), (IMP, Q0, 10000, 20000))
    records = [
        {"e": "hdr", "v": 13, "ts": 1789500000000, "map": ab.MAP_TOC, "inst": 8, "diff": 3, "boss": "anub-arak",
         "roster": [
             {"g": MAIN, "n": "Bulwark", "r": "tank", "c": "warrior", "h": 0},
             {"g": SIDE0, "n": "Ecoterrorist", "r": "tank", "c": "druid", "h": 0},
             {"g": SIDE1, "n": "Aegis", "r": "tank", "c": "paladin", "h": 0},
             {"g": DK, "n": "Deathsong", "r": "melee", "c": "deathknight", "h": 0},
             {"g": MAGE, "n": "Frostfire", "r": "ranged", "c": "mage", "h": 0},
             {"g": ROGUE, "n": "Shadowstep", "r": "melee", "c": "rogue", "h": 0},
             {"g": HUNTER, "n": "Trueshot", "r": "ranged", "c": "hunter", "h": 0},
             {"g": HEAL, "n": "Mercy", "r": "heal", "c": "priest", "h": 0},
             {"g": MELEE, "n": "Arkos", "r": "melee", "c": "druid", "h": 1}]},
        {"t": 0, "e": "pull", "boss": "anub-arak", "src": "engage"},
        {"t": -500, "e": "unit", "g": BOSS, "en": ab.NPC_ANUBARAK, "n": "Anub'arak", "b": 1},
        {"t": -500, "e": "unit", "g": IMP, "en": 416, "n": "Ruirin", "own": MAGE},
        *({"t": 0, "e": "unit", "g": guid, "en": ab.NPC_FROST_SPHERE, "n": "Frost Sphere"}
          for guid in (Q0, Q1, QR, QA, QB)),
        *({"t": first, "e": "unit", "g": guid, "en": ab.NPC_BURROWER, "n": "Nerubian Burrower"}
          for guid, first in ((U1, 6000), (U2, 6000), (U3, 41000))),
        *({"e": "spell", "sp": spell, "n": name} for spell, name in ((42842, "Frostbolt"), (75, "Auto Shot"))),

        *(note(when, MAIN, "anub.phase", value) for when, value in ((0, 1), (20000, 2), (40000, 1), (55000, 2))),
        note(0, MAIN, "anub.side0", SIDE0),
        note(0, MAIN, "anub.side1", SIDE1),
        *(note(when, MAIN, "anub.patch0", value) for when, value in ((0, 0), (5500, Q0), (64100, 0))),
        *(note(when, MAIN, "anub.patch1", value) for when, value in ((0, 0), (9500, Q1), (31100, 0), (45500, QR))),
        # the spend drops the allowance, and the second submerge goes under with none
        *(note(when, MAIN, "anub.allow", value) for when, value in ((0, 0), (20000, 1), (31100, 0), (55000, 0))),
        note(6000, MAIN, "anub.wave", 1),
        note(41000, MAIN, "anub.wave", 2),
        *(note(when, MAIN, "anub.focus", value) for when, value in (
            (0, 0), (6500, U1), (7000, 0), (12000, U2), (15000, 0), (50000, U3), (52000, 0))),
        *(note(when, SIDE0, "anub.sidetank", value) for when, value in (
            (0, "ready"), (9000, "hold"), (21000, "gather"), (40500, "hold"))),
        note(10000, SIDE1, "anub.sidetank", "hold"),
        note(18500, MAGE, "anub.leave", "walk"),
        note(22500, MAGE, "anub.leave", "none"),
        note(12500, ROGUE, "anub.mstand", "walk"),
        note(14000, ROGUE, "anub.mstand", "none"),

        cast(3000, DK, ab.SPELL_DEATH_GRIP, Q0),
        dmg(4000, MAGE, Q0, 42842, 9000),
        cast(7500, HUNTER, 75, Q1),
        dmg(8000, HUNTER, Q1, 75, 9000),
        cast(43000, DK, ab.SPELL_DEATH_GRIP, QR),
        dmg(44000, MAGE, QR, 42842, 9000),

        aura(22000, ROGUE, ab.SPELL_MARK, False),
        aura(30000, ROGUE, ab.SPELL_MARK, True),
        *(note(when, ROGUE, "anub.kitestage", stage) for when, stage in (
            (22000, "run"), (27000, "patch"), (30000, "none"))),
        aura(57000, HUNTER, ab.SPELL_MARK, False),
        aura(63000, HUNTER, ab.SPELL_MARK, True),
        *(note(when, HUNTER, "anub.kitestage", stage) for when, stage in (
            (57000, "run"), (60000, "immune"), (63000, "none"))),
        {"t": 70000, "e": "end", "out": "wipe"},
    ]
    for guid, patch, start, stop in stints:
        records.append(aura(start, guid, PERMAFROST_25H, False, patch))
        if stop is not None:
            records.append(aura(stop, guid, PERMAFROST_25H, True, patch))
    records += [frost_snap(when) for when in range(-500, 70000, 500)]
    return records


class FrostPull(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = write_trace(cls.folder.name, "649_8_anub-arak_1789500005.ndjson", frost_pull())

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_permafrost_time_splits_at_the_phase_edges_and_leaves_out_pets(self):
        totals = ab.frost_totals(ab.frost_stints(self.trace))
        self.assertEqual({guid: (bot["group"], bot["up"], bot["down"], bot["stints"]) for guid, bot in totals.items()},
                         {SIDE0: ("side tank", 29000, 9500, 2), SIDE1: ("side tank", 20000, 26000, 2),
                          MAIN: ("main tank", 2000, 0, 1), ROGUE: ("melee", 6000, 0, 1),
                          MAGE: ("ranged", 2000, 2000, 1), HEAL: ("heal", 0, 2000, 1), MELEE: ("melee", 4000, 0, 1),
                          HUNTER: ("ranged", 0, 1500, 1)})

    def test_each_long_stint_reads_its_phase_and_the_bots_target_at_its_start(self):
        rows = [row for row in ab.frost_stints(self.trace) if row["stop"] - row["start"] > ab.FROST_STINT_MS]
        self.assertEqual([(row["guid"], row["start"], row["phase"], row["target"], row["closed"]) for row in rows],
                         [(SIDE0, 5500, "P1", 0, True), (SIDE1, 9500, "P1", 0, True), (ROGUE, 10000, "P1", U1, True),
                          (MELEE, 11000, "P1", 0, True), (MAGE, 18000, "P1", BOSS, True),
                          (SIDE0, 40500, "P1b", 0, True), (SIDE1, 45500, "P1b", 0, False)])

    def test_a_focus_pick_reads_its_time_up_and_where_it_stood(self):
        picks = ab.focus_picks(self.trace)
        self.assertEqual([(row["t"], row["guid"], row["hp"], row["up"], row["target"], row["settled"])
                          for row in picks],
                         [(6500, U1, 95.0, 500, SIDE0, False), (12000, U2, 70.0, 6000, SIDE1, True),
                          (50000, U3, 99.0, 1000, MAIN, True)])
        self.assertAlmostEqual(picks[0]["side_tank"], min(math.dist(EXIT_N1, RIM0), math.dist(EXIT_N1, RIM1)))
        self.assertAlmostEqual(picks[0]["patch"], math.dist(EXIT_N1, PATCH0))
        self.assertAlmostEqual(picks[1]["side_tank"], 2.0)
        self.assertAlmostEqual(picks[1]["patch"], 5.0)
        self.assertAlmostEqual(picks[2]["side_tank"], min(math.dist(U3_SPOT, RIM0), math.dist(U3_SPOT, RIM1B)))
        self.assertAlmostEqual(picks[2]["patch"], min(math.dist(U3_SPOT, PATCH0), math.dist(U3_SPOT, QR_LAND)))

    def test_a_burrower_on_a_side_tank_reads_its_median_distance_from_that_sides_patch(self):
        # U3 sits on the main tank, no side tank, so it never counts
        self.assertEqual([(row["guid"], row["held"], len(row["gaps"]), row["median"])
                          for row in ab.held_rows(self.trace)],
                         [(U1, 11, 11, 4.0), (U2, 11, 11, 3.0)])

    def test_each_side_reads_its_first_tank_patch_from_a_grip_or_shots(self):
        rows = ab.first_patches(self.trace)
        self.assertEqual([(row["side"], row["t"], row["guid"], row["grip"] and row["grip"]["t"], row["death"]["killer"])
                          for row in rows], [(0, 5500, Q0, 3000, MAGE), (1, 9500, Q1, None, HUNTER)])
        self.assertAlmostEqual(rows[0]["death"]["off_spot"], 3.0)
        self.assertAlmostEqual(rows[1]["death"]["off_spot"], 2.0)

    def test_a_tank_patch_gone_under_him_names_the_kite_that_spent_it(self):
        self.assertEqual([(row["side"], row["guid"], row["gone"], row["label"], row["kiter"], row["mark"], row["into"],
                           row["stage"]) for row in ab.spent_patches(self.trace)],
                         [(1, Q1, 31000, "P2", ROGUE, 22000, 8000, "patch"),
                          (0, Q0, 64000, "P2b", HUNTER, 57000, 6000, "immune")])

    def test_a_spent_sides_next_patch_reads_its_grip_against_the_emerge(self):
        rows = ab.spent_patches(self.trace)
        back = rows[0]["back"]
        self.assertEqual((back["t"] - rows[0]["emerge"], back["guid"], back["grip"]["t"], back["grip"]["landed"]),
                         (5500, QR, 43000, 500))
        self.assertAlmostEqual(back["death"]["off_spot"], 1.0)
        self.assertIsNone(rows[1]["back"])

    def test_a_spend_past_the_heroic_allowance_is_over(self):
        rows = ab.allowance_rows(self.trace, ab.sphere_deaths(self.trace), ab.spent_patches(self.trace))
        self.assertEqual([(row["label"], row["allow"], row["dropped"], len(row["spent"]), row["over"]) for row in rows],
                         [("P2", 1, 31100, 1, False), ("P2b", 0, None, 1, True)])

    def test_a_mark_starting_inside_a_patch_is_flagged(self):
        windows = ab.mark_windows(self.trace)
        self.assertEqual([(window["guid"], window["in_frost"]) for window in windows], [(ROGUE, False), (HUNTER, True)])
        self.assertAlmostEqual(windows[1]["frost_yd"], 3.0)

    def test_side_tank_gather_time_per_submerge(self):
        self.assertEqual(ab.side_tank_gather(self.trace), [("P2", 20000, {SIDE0: 19000}), ("P2b", 55000, {})])

    def test_the_newer_probes_read_empty_on_a_pull_without_their_rows(self):
        self.assertEqual(ab.spot_changes(self.trace), [])
        self.assertEqual(ab.hstand_time(self.trace), {})
        self.assertEqual(ab.engine_passes(self.trace), {})
        self.assertEqual(ab.healer_engine_coverage(self.trace), {})
        self.assertEqual(ab.healer_follows(self.trace), {})
        self.assertEqual(ab.defensive_rows(self.trace), [])
        self.assertEqual([row["step"] for row in ab.grip_rows(self.trace)], [None, None])
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            ab.show_healers(self.trace)
            ab.show_tanks(self.trace)
        self.assertIn("no cov records, so no engine passes and no raid healer engine coverage\n", out.getvalue())
        self.assertIn("anub.hstand: no rows\n", out.getvalue())
        self.assertIn("no anub.defensive cast\n", out.getvalue())

    def test_every_section_prints(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            ab.show_banner(self.trace)
            for _, _, section in ab.SECTIONS:
                section(self.trace)
        text = out.getvalue()
        self.assertRegex(text, r"\n  main tank +1 +2\.0 +0\.0 +2\.0\n")
        self.assertRegex(text, r"\n  side tank +2 +49\.0 +35\.5 +84\.5\n")
        # Arkos is human, so melee totals only Shadowstep
        self.assertRegex(text, r"\n  melee +1 +6\.0 +0\.0 +6\.0\n")
        self.assertRegex(text, r"\n  Arkos +melee +4\.0 +0\.0 +4\.0 +1 +4\.0  human\n")
        self.assertRegex(text, r"0:10\.000 Shadowstep +melee +6\.0 s P1 +burrower #981\n")
        self.assertRegex(text, r"0:18\.000 Frostfire +ranged +4\.0 s P1 +Anub'arak\n")
        self.assertRegex(text, r"0:45\.500 Aegis +side tank +24\.5 s P1b +-  on at the end\n")
        self.assertNotIn("Ruirin", text)
        self.assertIn("anub.leave: Frostfire walk 1, none 1\n", text)
        self.assertIn("anub.mstand: Shadowstep walk 1, none 1\n", text)
        self.assertIn("anub.sidetank gather while he was down: P2 Ecoterrorist 19.0 s; P2b none\n", text)
        self.assertRegex(text, r"0:06\.500 #981 +95% +0\.5 s Ecoterrorist .* before its tank\n")
        self.assertIn("1 of 3 pick(s) before its target was a tank within 5 yd", text)
        self.assertRegex(text, r"#982 +11 +11 +3\.0 yd\n")
        self.assertIn("side 0   0:05.500  #971 from a grip by Deathsong at 0:03.000, landed +0.5 s, killed by"
                      " Frostfire Frostbolt 42842, 3.0 yd off its spot\n", text)
        self.assertIn("side 1   0:09.500  #972 from shots, killed by Trueshot Auto Shot 75, 2.0 yd off its spot\n",
                      text)
        self.assertIn("P2     0:20.000  allowance  1  tank 0  other 0  spent 1, allowance 0 from 0:31.100\n", text)
        self.assertIn("side 1 #972 gone 0:31.000, Shadowstep's mark 8.0 s in, kitestage patch\n        back 0:45.500,"
                      " 5.5 s after the emerge: #973 from a grip by Deathsong at 0:43.000, landed +0.5 s, killed by"
                      " Frostfire Frostbolt 42842, 1.0 yd off its spot\n", text)
        self.assertIn("P2b    0:55.000  allowance  0  tank 0  other 0  spent 1  over\n", text)
        self.assertIn("side 0 #971 gone 1:04.000, Trueshot's mark 6.0 s in, kitestage immune\n"
                      "        not back by the end\n", text)
        self.assertRegex(text, r"0:22\.000 Shadowstep +melee +8\.0 +- +patch +0 +0 +-  tank patch 1 spent\n")
        self.assertIn("marked 3.0 yd from a Permafrost centre, inside its slow\n", text)
        self.assertIn("marked within 7.5 yd of a Permafrost centre: Trueshot 0:57.000 3.0 yd\n", text)


R0, RG, RR = (CREATURE | n for n in range(1001, 1004))
RX, RY = CREATURE | 1011, CREATURE | 1012
GHOUL = CREATURE | 1021
SCARAB = CREATURE | 1031
SPK3 = CREATURE | 1041
EXPOSE_25 = 67847
EXPOSE_10 = 67721

MAIN_AT = (X - 5.0, Y)
SIDE0_AT = (X + 3.0, Y + O + 4.0)
SIDE1_AT = (X, Y - O - 2.0)
H_NEAR, H_OUT, H_STAND = (X - 5.0, Y + 10.0), (X - 5.0, Y + 30.0), (X - 5.0, Y + 5.0)
PAL_AT = (X - 41.0, Y - 14.0)       # 41 yd from side 1's tank, 53 from side 0's
B_DOWN = (X - 20.0, Y)
K0 = (X - 22.0, Y + 3.0)
DK_GRIP0, DK_P2, DK_P1B = (X + 3.0, Y + O + 0.5), (X - 30.0, Y), (X - 10.0, Y + 5.0)
G_SPOT = (X + 3.0, Y - 4.0)         # 20 yd from side 0's tank, 16 from its patch
RG_FLY = (X - 10.0, Y + 25.0)
RR_FLY, RR_LAND = (X - 25.0, Y + 40.0), (X - 23.0, Y + 15.0)
SPOT0_NEW = (X - 25.0, Y + 15.0)    # 2 yd from where the refill lands


def refill_kiter(when: int) -> tuple[float, float]:
    """The hunter runs east at 7 yd/s from its mark at 22.5 s, stands still 26-28 s, then runs on to 32 s."""
    if when < 22500:
        return K0
    run = min(when, 26000) - 22500 + max(0, min(when, 32000) - 28000)
    return K0[0] + 7.0 * run / 1000.0, K0[1]


def refill_player(guid: int, when: int) -> list:
    hp, target = 100.0, 0
    if guid == MAIN:
        spot, target = MAIN_AT, BOSS
    elif guid == SIDE0:
        spot = SIDE0_AT
    elif guid == SIDE1:
        spot, hp = SIDE1_AT, 0.0 if when >= 15000 else 100.0
    elif guid == HEAL:
        spot = H_NEAR if when < 10000 else H_OUT if when < 40000 else H_STAND
    elif guid == PALADIN:
        spot = PAL_AT
    elif guid == HUNTER:
        spot = refill_kiter(when)
    elif guid == ROGUE:
        spot = (X - 18.0, Y - 6.0)
    elif guid == MELEE:
        spot = (X - 20.0, Y + 15.0)
    else:
        spot = DK_GRIP0 if when < 5000 else DK_P2 if when < 40000 else DK_P1B
    return [guid, *spot, Z, 0.0, hp, 100.0, target, 0, 0, 0, 0]


def refill_sphere(guid: int, when: int):
    """`(xy, z)` or None. The DK grips R0 at 2 s and it dies at 3 s onto side 0's spot, where the hunter's kite
    spends it at 32 s; the DK grips RG at 43 s, it lands, nobody kills it and it floats back up at 48.5 s; RR
    is shot down at 50 s 2 yd from side 0's new spot."""
    if when < 0:
        return None
    if guid == R0:
        if when > 32000:
            return None
        if when < 2500:
            return (X + 10.0, Y + 20.0), FLYING_Z
        return TP0, Z + 0.5 if when < 3000 else FALLING_Z if when < 4500 else PATCH_Z
    if guid == RG:
        return (DK_P1B, Z + 0.5) if 43500 <= when < 48500 else (RG_FLY, FLYING_Z)
    if when < 50000:
        return RR_FLY, FLYING_Z
    return RR_LAND, FALLING_Z if when < 51500 else PATCH_Z


def refill_burrowers(when: int) -> list[list]:
    """RX holds side 0's tank to 10 s, goes for the human's ghoul and goes under at 12 s, 20 yd from its tank,
    back up full at 22.5 s; RY sits on side 1's tank until it dies at 15 s."""
    rows = []
    spot = hp = None
    target = 0
    if 2000 <= when < 10000:
        spot, hp, target = (SIDE0_AT[0] + 1.0, SIDE0_AT[1]), 100.0 - (when - 2000) / 400.0, SIDE0
    elif when == 10000:
        spot, hp, target = (X + 3.0, Y + 6.0), 79.0, GHOUL
    elif when == 10500:
        spot, hp, target = (X + 3.0, Y + 1.0), 78.5, GHOUL
    elif 11000 <= when < 12000:
        spot, hp, target = G_SPOT, 78.0 if when == 11000 else 75.0, GHOUL
    elif 12000 <= when <= 22000:
        spot, hp = G_SPOT, 70.0
    elif 22500 <= when <= 30000:
        spot, hp = EXIT_N1, 100.0
    if spot is not None:
        rows.append([RX, *spot, Z, 0.0, hp, 0.0, target, 0, 0, 0, 0])
    if 2000 <= when <= 20000:
        rows.append([RY, SIDE1_AT[0] + 1.0, SIDE1_AT[1], Z, 0.0, 100.0 - (when - 2000) / 1000.0, 0.0,
                     SIDE1 if when < 15000 else 0, 0, 0, 0, 0])
    return rows


def refill_snap(when: int) -> dict:
    units = [refill_player(guid, when) for guid in (MAIN, SIDE0, SIDE1, HEAL, PALADIN, HUNTER, ROGUE, MELEE, DK)]
    up = 0 <= when < 20000 or when >= 40000
    units.append([BOSS, *(B_DOWN if 15000 <= when < 40000 else (X - 5.0, Y + 2.0)), Z, 0.0,
                  100.0 - max(when, 0) / 1000.0, 0.0, MAIN if up else 0, 0, 0, 0, 0])
    if 22500 <= when <= 32000:
        units.append([SPK3, K0[0] - 6.0 + 3.5 * (when - 22500) / 1000.0, K0[1], Z, 0.0, 100.0, 0.0, HUNTER,
                      0, 0, 0, 0])
    hz = []
    for guid in (R0, RG, RR):
        place = refill_sphere(guid, when)
        if place is None:
            continue
        (x, y), z = place
        units.append([guid, x, y, z, 0.0, 100.0, 0.0, 0, 0, 0, 0, 0])
        if z == PATCH_Z:
            hz.append([PERMAFROST_25H, x, y, PATCH_Z, 6.0, 1])
    units += refill_burrowers(when)
    return {"t": when, "e": "snap", "u": units, "hz": hz}


def expose(when: int, guid: int, spell: int, stacks: int) -> dict:
    return {"t": when, "e": "aura", "d": guid, "s": RX, "sp": spell, "r": 1 if not stacks else 0, "st": stacks}


def act(when: int, guid: int, action: str, verdict: str = "OK") -> dict:
    return {"t": when, "e": "act", "g": guid, "a": action, "rel": 1.0, "vd": verdict}


def refill_pull() -> list[dict]:
    """A 25H pull: P1 to 20 s, P2 to 40 s, P1b to the end at 60 s, burrower waves at 2 and 47 s. Side 1's tank
    dies at 15 s; the hunter's kite takes a Hand of Protection on a spike half its speed and spends side 0's
    patch while running."""
    spot = "{:.2f},{:.2f},142.60".format
    records = [
        {"e": "hdr", "v": 13, "ts": 1789500000000, "map": ab.MAP_TOC, "inst": 9, "diff": 3, "boss": "anub-arak",
         "roster": [
             {"g": MAIN, "n": "Ecoterrorist", "r": "tank", "c": "druid", "h": 0},
             {"g": SIDE0, "n": "Bulwark", "r": "tank", "c": "paladin", "h": 0},
             {"g": SIDE1, "n": "Felesta", "r": "tank", "c": "deathknight", "h": 0},
             {"g": HEAL, "n": "Mercy", "r": "heal", "c": "priest", "h": 0},
             {"g": PALADIN, "n": "Justice", "r": "heal", "c": "paladin", "h": 0},
             {"g": HUNTER, "n": "Trueshot", "r": "ranged", "c": "hunter", "h": 0},
             {"g": ROGUE, "n": "Shadowstep", "r": "melee", "c": "rogue", "h": 0},
             {"g": MELEE, "n": "Arkos", "r": "melee", "c": "deathknight", "h": 1},
             {"g": DK, "n": "Obliteration", "r": "melee", "c": "deathknight", "h": 0}]},
        {"t": 0, "e": "pull", "boss": "anub-arak", "src": "engage"},
        {"t": -500, "e": "unit", "g": BOSS, "en": ab.NPC_ANUBARAK, "n": "Anub'arak", "b": 1},
        {"t": 22000, "e": "unit", "g": SPK3, "en": ab.NPC_SPIKE, "n": "Anub'arak", "b": 1},
        *({"t": 0, "e": "unit", "g": guid, "en": ab.NPC_FROST_SPHERE, "n": "Frost Sphere"} for guid in (R0, RG, RR)),
        *({"t": 2000, "e": "unit", "g": guid, "en": ab.NPC_BURROWER, "n": "Nerubian Burrower"} for guid in (RX, RY)),
        {"t": 9000, "e": "unit", "g": GHOUL, "en": 24207, "n": "Army of the Dead Ghoul", "own": MELEE},
        {"t": 12000, "e": "unit", "g": SCARAB, "en": ab.NPC_SCARAB, "n": "Swarm Scarab"},
        {"e": "covdef", "d": [[1, "timer", "nc", "n"], [2, "party member low health", "holy", "c"],
                              [3, "party member medium health", "holy", "c"], [5, "often", "holy", "c"],
                              [6, "raid healer combat engine trigger", ab.HEALER_ENGINE, "c"],
                              [7, "raid healer combat engine trigger", ab.HEALER_ENGINE, "n"]]},

        *(note(when, MAIN, "anub.phase", value) for when, value in ((0, 1), (20000, 2), (40000, 1))),
        note(0, MAIN, "anub.side0", SIDE0),
        note(0, MAIN, "anub.side1", SIDE1),
        *(note(when, MAIN, "anub.patch0", value) for when, value in ((0, 0), (5000, R0), (32100, 0), (52000, RR))),
        note(0, MAIN, "anub.patch1", 0),
        # instance-level, x,y,z; side 1's written bare x,y
        note(0, MAIN, "anub.spot0", spot(X, Y + O)),
        note(0, MAIN, "anub.spot1", f"{X:.2f},{Y - O:.2f}"),
        note(33000, MAIN, "anub.spot0", spot(*SPOT0_NEW)),
        note(2000, MAIN, "anub.wave", 1),
        note(47000, MAIN, "anub.wave", 2),
        note(2000, RX, "anub.bside", 0),
        note(2000, RY, "anub.bside", 1),

        *(note(when, DK, "anub.gripstep", step) for when, step in (
            (1000, "walk"), (1900, "cast"), (2200, "wait"), (2500, "kill"), (4000, "none"), (41000, "walk"),
            (42900, "cast"), (43000, "none"))),
        *(note(when, DK, "anub.sphere", duty) for when, duty in (
            (1000, "grip0"), (2500, "0"), (4000, "none"), (41000, "grip1"), (43000, "none"))),
        cast(2000, DK, ab.SPELL_DEATH_GRIP, R0),
        dmg(2800, HUNTER, R0, 75, 9000),
        cast(43000, DK, ab.SPELL_DEATH_GRIP, RG),
        dmg(49800, HUNTER, RR, 75, 9000),

        aura(22500, HUNTER, ab.SPELL_MARK, False, SPK3),
        aura(32000, HUNTER, ab.SPELL_MARK, True, SPK3),
        note(22500, HUNTER, "anub.kitestage", "run"),
        note(32000, HUNTER, "anub.kitestage", "none"),
        cast(22700, PALADIN, 10278, HUNTER),

        *(expose(when, SIDE0, EXPOSE_25, stacks) for when, stacks in (
            (5000, 1), (6000, 2), (7000, 3), (8000, 4), (9000, 5), (10000, 6), (18000, 0))),
        *(expose(when, SIDE1, EXPOSE_10, stacks) for when, stacks in ((6000, 1), (8000, 2), (11000, 3), (15000, 0))),
        *(expose(when, HEAL, EXPOSE_25, stacks) for when, stacks in ((10500, 1), (11000, 2), (20000, 0))),
        note(1000, MAIN, "anub.defensive", "none"),
        note(9500, SIDE0, "anub.defensive", "divine protection"),
        note(11500, SIDE1, "anub.defensive", "anti magic shell"),
        note(12000, SIDE1, "anub.defensive", "covered"),

        # side 1's death: the hit at 2.5 s and the heal at 16 s fall outside its 12 s
        dmg(2500, RY, SIDE1, 0, 20000),
        *(dmg(when, RY, SIDE1, 0, 20000) for when in (5000, 10000, 14900)),
        dmg(12000, BOSS, SIDE1, 0, 10000),
        dmg(13000, SCARAB, SIDE1, 0, 5000),
        {"t": 8000, "e": "heal", "s": HEAL, "d": SIDE1, "sp": 48071, "a": 8000, "oh": 1000},
        {"t": 13000, "e": "heal", "s": HEAL, "d": SIDE1, "sp": 48071, "a": 6000, "oh": 0},
        {"t": 14000, "e": "heal", "s": PALADIN, "d": SIDE1, "sp": 48782, "a": 4000, "oh": 4000},
        {"t": 16000, "e": "heal", "s": HEAL, "d": SIDE1, "sp": 48071, "a": 5000, "oh": 0},
        {"t": 11000, "e": "abs", "d": SIDE1, "s": HEAL, "sp": 48066, "a": 3000},
        {"t": 15000, "e": "death", "g": SIDE1, "killer": RY, "x": SIDE1_AT[0], "y": SIDE1_AT[1], "z": Z,
         "blow": [RY, 20000]},

        act(-1000, HEAL, "follow"),
        act(30000, HEAL, "follow"),
        act(30500, HEAL, "follow", "FAILED"),
        act(31000, HEAL, "follow"),
        act(35000, PALADIN, "follow"),
        act(36000, HUNTER, "follow"),
        note(41000, HEAL, "anub.hstand", "walk"),
        note(44000, HEAL, "anub.hstand", "hold"),
        note(41000, PALADIN, "anub.hstand", "hold"),
        # Mercy's often node reads double; Justice never carries the non-combat raid healer node
        {"t": 60000, "e": "cov", "g": HEAL, "r": [[1, 50, 0, 0, 0, 0, 10], [2, 300, 20, 20, 15],
                                                  [3, 280, 0, 0, 0, 0, 20], [5, 600], [6, 300, 4, 4, 3],
                                                  [7, 60, 2, 2, 2]]},
        {"t": 60000, "e": "cov", "g": PALADIN, "r": [[1, 30], [2, 250], [6, 250]]},
        {"t": 60000, "e": "end", "out": "wipe"},
    ]
    records += [refill_snap(when) for when in range(-500, 60000, 500)]
    return records


class RefillPull(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = write_trace(cls.folder.name, "649_9_anub-arak_1789500006.ndjson", refill_pull())

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_a_spot_parses_x_and_y_with_or_without_z(self):
        self.assertEqual(ab.parse_spot("721.04,129.01,142.6"), (721.04, 129.01))
        self.assertEqual(ab.parse_spot("721.0,129.0"), (721.0, 129.0))
        self.assertIsNone(ab.parse_spot("721.0"))
        self.assertIsNone(ab.parse_spot("a,b"))

    def test_each_spot_change_reads_its_side_and_how_far_it_moved(self):
        rows = ab.spot_changes(self.trace)
        self.assertEqual([(row["side"], row["t"]) for row in rows], [(0, 0), (1, 0), (0, 33000)])
        self.assertAlmostEqual(rows[2]["spot"][0], SPOT0_NEW[0])
        self.assertAlmostEqual(rows[2]["moved"], math.dist(SPOT0_NEW, (X, Y + O)), places=2)
        self.assertIsNone(rows[0]["moved"])

    def test_a_refill_lands_against_the_spot_its_side_wrote(self):
        rows = {row["guid"]: row for row in ab.sphere_deaths(self.trace)}
        self.assertEqual((rows[RR]["side"], rows[RR]["killed"]), (0, 50000))
        self.assertAlmostEqual(rows[RR]["off_spot"], 2.0, places=2)
        self.assertAlmostEqual(rows[R0]["off_spot"], 3.0, places=2)

    def test_a_grip_reads_whether_its_duty_held_to_kill_and_a_sphere_back_up(self):
        self.assertEqual([(row["t"], row["sphere"], row["landed"], row["step"], row["step_ms"], row["rose"])
                          for row in ab.grip_rows(self.trace)],
                         [(2000, R0, 500, "kill", 500, None), (43000, RG, 500, "none", 0, 5500)])

    def test_a_burrower_submerge_reads_where_it_went_under(self):
        rows = ab.burrower_submerges(self.trace)
        self.assertEqual([(row["guid"], row["t"], row["back"], row["spot"], row["hp"], row["patches"], row["side"],
                           row["tank"], row["alive"], row["victim"]) for row in rows],
                         [(RX, 12000, 22500, G_SPOT, 70.0, 1, 0, SIDE0, True, GHOUL)])
        self.assertAlmostEqual(rows[0]["frost"], 16.0)
        self.assertAlmostEqual(rows[0]["tank_yd"], 20.0)
        self.assertEqual(ab.submerge_reasons(rows[0]), ["off its side tank"])

    def test_a_ghoul_reads_by_name_and_owner(self):
        self.assertEqual(ab.unit_label(self.trace, GHOUL), "Army of the Dead Ghoul (Arkos)")

    def test_a_cover_on_a_spike_no_faster_than_the_kiter_is_flagged(self):
        windows = ab.mark_windows(self.trace)
        self.assertEqual(len(windows), 1)
        cover = windows[0]["cover"]
        self.assertEqual([(row["rec"]["t"], row["slow"]) for row in cover], [(22700, True)])
        self.assertAlmostEqual(cover[0]["spike"], 3.5)
        self.assertAlmostEqual(cover[0]["kiter"], 7.0)

    def test_a_kiter_standing_still_while_marked_reads_stalled(self):
        self.assertEqual(ab.mark_windows(self.trace)[0]["stalled"], (2000, 2000))

    def test_a_tank_patch_spent_while_running_is_a_run_spend(self):
        rows = ab.spent_patches(self.trace)
        self.assertEqual([(row["side"], row["guid"], row["kiter"], row["stage"]) for row in rows],
                         [(0, R0, HUNTER, "run")])
        self.assertTrue(ab.run_spend(rows[0]))
        self.assertFalse(ab.run_spend({"stage": "patch"}))
        self.assertTrue(ab.run_spend({"stage": "boost:55004"}))

    def test_the_crowd_where_he_went_down_and_the_first_kiters_gap(self):
        rows = ab.submerge_crowds(self.trace)
        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0]["spot"], B_DOWN)
        self.assertEqual({yards: dict(counts) for yards, counts in rows[0]["near"].items()},
                         {8.0: {"ranged": 1, "melee": 1}, 12.0: {"ranged": 1, "melee": 2},
                          20.0: {"tank": 1, "ranged": 1, "melee": 3}})
        self.assertEqual((rows[0]["kiter"], rows[0]["mark"]), (HUNTER, 22500))
        self.assertAlmostEqual(rows[0]["spike_yd"], 6.0)

    def test_healer_distances_per_phase(self):
        gaps = ab.healer_rows(self.trace)["gaps"]
        self.assertAlmostEqual(statistics.median(gaps[(HEAL, "P1b")]["main"]), 5.0)
        self.assertNotIn(1, gaps[(HEAL, "P1b")])
        self.assertAlmostEqual(statistics.median(gaps[(PALADIN, "P1")]["main"]), math.dist(PAL_AT, MAIN_AT))

    def test_seconds_far_from_a_side_tank_a_burrower_is_on(self):
        far = ab.healer_rows(self.trace)["far"]
        self.assertEqual({key: ms for key, ms in far.items() if ms},
                         {(PALADIN, 1, "P1"): 13000, (PALADIN, 0, "P1"): 8000, (HEAL, 1, "P1"): 5000})

    def test_a_side_tank_with_no_healer_in_reach(self):
        bare = ab.healer_rows(self.trace)["bare"]
        self.assertEqual({key: ms for key, ms in bare.items() if ms}, {(1, SIDE1, "P1"): 5000})

    def test_follow_acts_count_only_ok_rows_from_a_healer_in_the_pull(self):
        self.assertEqual(ab.healer_follows(self.trace), {HEAL: [30000, 31000], PALADIN: [35000]})

    def test_engine_passes_take_the_most_common_node_sum(self):
        self.assertEqual(ab.engine_passes(self.trace), {HEAL: {"n": 60, "c": 300}, PALADIN: {"n": 30, "c": 250}})

    def test_raid_healer_engine_coverage_per_engine(self):
        self.assertEqual(ab.healer_engine_coverage(self.trace),
                         {"c": {"bots": {HEAL, PALADIN}, "checks": 550, "fires": 4, "won": 3},
                          "n": {"bots": {HEAL}, "checks": 60, "fires": 2, "won": 2}})

    def test_hstand_seconds_per_value(self):
        self.assertEqual(ab.hstand_time(self.trace), {HEAL: {"walk": 3000, "hold": 16000}, PALADIN: {"hold": 19000}})

    def test_expose_weakness_peak_per_wave_and_side_tank(self):
        self.assertEqual([(row["wave"], row["tank"], row["peak"], row["ms"]) for row in ab.expose_waves(self.trace)],
                         [(1, SIDE0, 6, 9000), (1, SIDE1, 3, 0), (2, SIDE0, 0, 0), (2, SIDE1, 0, 0)])

    def test_each_defensive_reads_the_stack_at_the_cast(self):
        rows = ab.defensive_rows(self.trace)
        self.assertEqual([(row["t"], row["guid"], row["spell"], row["stacks"], row["side"], row["phase"])
                          for row in rows],
                         [(9500, SIDE0, "divine protection", 5, 0, "P1"),
                          (11500, SIDE1, "anti magic shell", 3, 1, "P1")])
        self.assertEqual([ab.magic_on_side_tank(row) for row in rows], [False, True])

    def test_a_tank_death_reads_the_12_s_before_it(self):
        rows = ab.tank_deaths(self.trace)
        self.assertEqual(len(rows), 1)
        row = rows[0]
        self.assertEqual((row["guid"], row["side"], row["main"], dict(row["taken"])),
                         (SIDE1, 1, False, {"burrower": 60000, "Anub'arak": 10000, "scarab": 5000}))
        self.assertEqual((row["heals"], row["gross"], row["overheal"], row["absorbed"], row["expose"]),
                         (3, 18000, 5000, 3000, 3))

    def test_the_new_keys_count_as_present(self):
        gone = ab.missing_probes(self.trace)
        for key in ("anub.spot0", "anub.spot1", "anub.hstand"):
            self.assertNotIn(key, gone)

    def test_every_section_prints(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            ab.show_banner(self.trace)
            for _, _, section in ab.SECTIONS:
                section(self.trace)
        text = out.getvalue()
        self.assertIn("tank patch 0 spent in run\n", text)
        self.assertIn("speeds at the cover: Hand of Protection +0.2 s: spike 3.5 yd/s, kiter 7.0 yd/s, no faster\n",
                      text)
        self.assertIn("stood still 2.0 s while marked, longest 2.0 s\n", text)
        self.assertIn("on a spike no faster than the kiter: Trueshot 0:22.700 Hand of Protection (3.5 vs 7.0 yd/s)\n",
                      text)
        self.assertIn("tank patch spent while the kiter ran or boosted: Trueshot 0:32.000 side 0 run\n", text)
        self.assertIn(": 8 yd 2 (melee 1, ranged 1); 12 yd 3 (melee 2, ranged 1); 20 yd 5 (tank 1, melee 3,"
                      " ranged 1)\n          first mark Trueshot 0:22.500, 6.0 yd from the spike\n", text)
        self.assertIn("duty held to kill +0.5 s\n", text)
        self.assertIn("duty dropped to none +0.0 s, never at kill; sphere back above z 150 +5.5 s\n", text)
        self.assertIn("grip duty never at kill: 1 of 2; gripped sphere back above z 150: #1002 0:48.500\n", text)
        self.assertRegex(text, r"side 0 +0:33\.000 +\d+\.\d, \d+\.\d +25\.2 yd on\n")
        self.assertRegex(text, r"#1011 +0:12\.000 .* 70% +16\.0 yd +1 +0 Bulwark 20\.0 yd +Army of the Dead Ghoul"
                               r" \(Arkos\)  off its side tank\n")
        self.assertIn("1 of 1 went under with its side tank alive and a patch alive", text)
        self.assertIn("follow acts, OK and alive in the pull: Justice 1 (0:35.000-0:35.000), Mercy 2"
                      " (0:30.000-0:31.000)", text)
        self.assertIn("Justice combat 250, non-combat 30 (11%), dead 0; Mercy combat 300, non-combat 60 (17%),"
                      " dead 0\n", text)
        self.assertIn("raid healer engine in the coverage: combat 2 bot(s), 550 checks, 4 fires, 3 won; non-combat"
                      " 1 bot(s), 60 checks, 2 fires, 2 won\n", text)
        self.assertIn("anub.hstand: Justice hold 1; Mercy walk 1, hold 1\n", text)
        self.assertIn("anub.hstand seconds: Justice hold 19.0 s; Mercy walk 3.0 s, hold 16.0 s\n", text)
        self.assertRegex(text, r"\n    side 1 Felesta +P1 5\.0 s\n")
        self.assertIn("wave 1   0:02.000  Bulwark 6 (9.0 s), Felesta 3 (0.0 s)\n", text)
        self.assertIn("Expose Weakness on anyone else, peak over the pull: Mercy 2\n", text)
        self.assertRegex(text, r"anti magic shell +stack  3  hp 100%  magic only on a side tank\n")
        self.assertIn("from 5 stacks: 1 of 2 side tank cast(s); magic only on a side tank outside phase 3: 1\n", text)
        self.assertRegex(text, r"0:15\.000 Felesta +side 1 P1 +taken 75,000 \(burrower 60,000, Anub'arak 10,000,"
                               r" scarab 5,000\); healed 13,000 by 3 heal\(s\), 5,000 over; absorbed 3,000;"
                               r" Expose peak 3\n")


def side_pull(diff: int, side1: int, later: list[dict] | None = None) -> list[dict]:
    return [
        {"e": "hdr", "v": 13, "ts": 1789500000000, "map": ab.MAP_TOC, "inst": 6, "diff": diff, "boss": "anub-arak",
         "roster": [{"g": SIDE0, "n": "Ecoterrorist", "r": "tank", "c": "druid", "h": 0},
                    {"g": DRAGON, "n": "Dragon", "r": "tank", "c": "paladin", "h": 1}]},
        {"t": 0, "e": "pull", "boss": "anub-arak", "src": "engage"},
        note(0, SIDE0, "anub.side0", SIDE0),
        note(0, SIDE0, "anub.side1", side1),
        *(later or []),
        {"t": 1000, "e": "end", "out": "wipe"},
    ]


class SideTanks(unittest.TestCase):
    def banner(self, diff: int, side1: int, later: list[dict] | None = None) -> tuple[tuple, str]:
        with tempfile.TemporaryDirectory() as folder:
            trace = write_trace(folder, "649_6_anub-arak_1789500003.ndjson", side_pull(diff, side1, later))
            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                ab.show_banner(trace)
            return ab.side_tanks(trace), out.getvalue()

    def test_a_human_side_tank_is_named(self):
        latched, text = self.banner(1, DRAGON)
        self.assertEqual(latched, (2, {0: [SIDE0], 1: [DRAGON]}))
        self.assertIn("side tanks latched: 2 of 2 needed (side 0 Ecoterrorist, side 1 Dragon (human))", text)

    def test_a_swap_lists_each_side_in_order(self):
        latched, text = self.banner(1, DRAGON, [note(500, SIDE0, "anub.side0", DRAGON),
                                                note(500, SIDE0, "anub.side1", SIDE0)])
        self.assertEqual(latched, (2, {0: [SIDE0, DRAGON], 1: [DRAGON, SIDE0]}))
        self.assertIn("(side 0 Ecoterrorist > Dragon (human), side 1 Dragon (human) > Ecoterrorist)", text)

    def test_25_man_one_short(self):
        latched, text = self.banner(1, 0)
        self.assertEqual(latched, (2, {0: [SIDE0]}))
        self.assertIn("side tanks latched: 1 of 2 needed (side 0 Ecoterrorist, side 1 none)", text)

    def test_10_man_needs_one(self):
        latched, text = self.banner(2, 0)
        self.assertEqual(latched, (1, {0: [SIDE0]}))
        self.assertNotIn("side tanks latched", text)


class EveryView(unittest.TestCase):
    def test_every_section_reads_empty_on_another_boss(self):
        trace = Trace(FULL)
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            ab.show_banner(trace)
            for _, _, section in ab.SECTIONS:
                section(trace)
        self.assertIn("not a Trial of the Crusader Anub'arak pull", out.getvalue())
        self.assertNotIn("locked out", out.getvalue())
        self.assertIn("HEALERS\n  not an Anub'arak pull\n", out.getvalue())
        self.assertIn("TANKS\n  not an Anub'arak pull\n", out.getvalue())

    def test_azjol_nerubs_anubarak_is_not_this_one(self):
        with tempfile.TemporaryDirectory() as folder:
            trace = write_trace(folder, "601_4_anub-arak_1789500000.ndjson", anub_pull(map_id=601))
            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                ab.show_banner(trace)
        self.assertIn("not a Trial of the Crusader Anub'arak pull", out.getvalue())


if __name__ == "__main__":
    unittest.main()
