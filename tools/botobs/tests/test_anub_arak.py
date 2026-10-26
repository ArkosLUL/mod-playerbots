"""Tests for the Anub'arak reader.

    python -m unittest discover -s tools/botobs/tests

A synthetic 10H pull pins the arithmetic behind each section, a 25H one the side tank, grip, kite
stage and allowance probes, and the shared fixture shows every section reads empty on a trace that
is not Anub'arak rather than raising.
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
        self.assertIn("Ice Block +0.3 s unwanted; anub.protect Trueshot self +0.2 s", text)
        self.assertIn("spans with a cast: side 0 1 of 1, side 1 1 of 1, kite 1 of 1, stock 1 of 1", text)


def focus_pull() -> list[dict]:
    """B1 holds the focus from 1 to 5 s, nothing to 6 s, then B2 to the end at 10 s. Trueshot leaves B1
    for the boss at 4 s, Drifter stays on the boss through B2 and is dead at 9 s, and the human and the
    healer never count."""
    def target(guid: int, when: int) -> int:
        if guid in (MELEE, FLOAT) and 1000 <= when < 5000:
            return B1
        if guid == KITE:
            return B2 if when >= 6000 else (BOSS if when == 4000 else B1)
        return BOSS if guid == FLOAT else 0

    def focus_snap(when: int) -> dict:
        units = [[guid, X, Y, Z, 0.0, 0.0 if guid == FLOAT and when == 9000 else 100.0, 100.0,
                  target(guid, when), 0, 0, 0, 0] for guid in (KITE, FLOAT, MELEE, HEAL)]
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


MAIN, SIDE0, SIDE1, DK, MAGE, LOCK, ROGUE, DRUID, HUNTER, DRAGON = range(6001, 6011)
IMP = (2 << 32) | 7001
G0, G1, K1, K2, IDLE = (CREATURE | n for n in range(911, 916))
BN1, BN2, BS1, BS2 = (CREATURE | n for n in range(921, 925))
WN, WS = CREATURE | 931, CREATURE | 932
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
        self.assertEqual(ab.allowance_rows(self.trace, ab.sphere_deaths(self.trace)),
                         [{"label": "P2", "start": 30000, "allow": 1, "tank": 0, "other": 2}])

    def test_kite_stages_and_boosts_per_window(self):
        windows = ab.mark_windows(self.trace)
        self.assertEqual([window["stages"] for window in windows],
                         [{"run": 2000, "boost:11305": 2000, "immune": 4000, "patch": 1000},
                          {"run": 1000, "boost:55004": 4000, "patch": 2000}])
        self.assertEqual([[(rec["t"], rec["sp"]) for rec in window["boosts"]] for window in windows],
                         [[(33000, 11305)], [(42000, 55004)]])
        self.assertEqual([[rec["sp"] for rec in window["own"]] for window in windows], [[], [5384]])

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
        self.assertIn("P2     0:30.000  allowance  1  tank 0  other 2  over", text)
        self.assertIn("anub.grip: gripping from 0:00.000, shooting from 0:11.000", text)
        self.assertRegex(text, r"Deathsong +grip1 +#912 +missed, closest \d+\.\d yd\n")
        self.assertRegex(text, r"Deathsong +grip0 +#911 +landed \+0\.5 s, latched side 0\n")
        self.assertIn("no tank patch with burrowers out while he was up: side 1 34.5 s", text)
        self.assertRegex(text, r"#922 +1 N 694\.5,185\.4 0>1 ")
        self.assertIn("kitestage run 2.0 s, boost Sprint 2.0 s, immune 4.0 s, patch 1.0 s; boosts Sprint +2.0 s",
                      text)
        self.assertIn("kitestage run 1.0 s, boost Nitro Boosts 4.0 s, patch 2.0 s; boosts Nitro Boosts +1.0 s", text)
        self.assertIn("Feign Death +3.0 s unwanted", text)
        self.assertIn("Ice Block or Feign Death on a kiter: Feign Death 1", text)
        self.assertIn("wave 1 0:05.000: focus due 0:20.000, 30.0 s before wave 2 at 0:50.000", text)
        self.assertIn("wave 2 0:50.000: focus due 0:55.000, the last wave", text)
        self.assertIn("main tank Bulwark in a patch 33% of 60 samples", text)
        self.assertIn(f"submerged 0:30.000 at {SUBMERGE_AT[0]:.1f}, {SUBMERGE_AT[1]:.1f}: patch0 12.0 yd under",
                      text)
        self.assertIn("anub.pickup: Bulwark attack 1, stepout 1, hold 1", text)


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

    def test_azjol_nerubs_anubarak_is_not_this_one(self):
        with tempfile.TemporaryDirectory() as folder:
            trace = write_trace(folder, "601_4_anub-arak_1789500000.ndjson", anub_pull(map_id=601))
            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                ab.show_banner(trace)
        self.assertIn("not a Trial of the Crusader Anub'arak pull", out.getvalue())


if __name__ == "__main__":
    unittest.main()
