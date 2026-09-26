"""Tests for the Anub'arak reader.

    python -m unittest discover -s tools/botobs/tests

A synthetic 10H pull pins the arithmetic behind each section, and the shared fixture shows every
section reads empty on a trace that is not Anub'arak rather than raising.
"""
from __future__ import annotations

import contextlib
import io
import json
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
MELEE = 5004  # marked once, impaled
LATE = 5005   # west of the Web Door at the pull
FLOAT = 5006  # still up in the coliseum at the pull, lands at 5 s, dies to the spike

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
    """`(guid, xy, z)` per sphere alive at `when`. S1 and S2 land at 8 s, S3 at 25 s, and the spike
    takes S3, last sampled at 31 s."""
    if when < 0:
        return []
    out = [(S1, PATCH_A, PATCH_Z if when >= 8000 else FLYING_Z),
           (S2, PATCH_B, PATCH_Z if when >= 8000 else FLYING_Z)]
    if when <= 31000:
        out.append((S3, KITE_PATCH, PATCH_Z if when >= 25000 else FLYING_Z))
    out += [(guid, (X + 5.0 * index, Y + 30.0), FLYING_Z) for index, guid in enumerate(SPHERES[3:])]
    return out


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
             MELEE: (X + 10.0, Y + 6.0, Z), LATE: (640.0, 144.7, 144.0)}
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
    units = [player(guid, when) for guid in (TANK, HEAL, KITE, MELEE, LATE, FLOAT)]
    units.append([BOSS, X + 10.0, Y + 5.0, Z, 0.0, 100.0 - max(when, 0) / 1000.0, 0.0,
                  boss_target(when), 0, 0, 0, 0])
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


def anub_pull(boss: str = "anub-arak", map_id: int = ab.MAP_TOC) -> list[dict]:
    records = [
        {"e": "hdr", "v": 13, "ts": 1789500000000, "map": map_id, "inst": 4, "diff": 2, "boss": boss,
         "roster": [
             {"g": TANK, "n": "Bulwark", "r": "tank", "c": "warrior", "h": 0},
             {"g": HEAL, "n": "Mercy", "r": "heal", "c": "priest", "h": 0},
             {"g": KITE, "n": "Trueshot", "r": "ranged", "c": "hunter", "h": 0},
             {"g": MELEE, "n": "Arkos", "r": "melee", "c": "rogue", "h": 1},
             {"g": LATE, "n": "Straggler", "r": "ranged", "c": "mage", "h": 0},
             {"g": FLOAT, "n": "Drifter", "r": "melee", "c": "warrior", "h": 0}]},
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

        # Kite shoots both tank patches down.
        note(7000, KITE, "anub.sphere", 0),
        note(7500, KITE, "anub.sphere", 1),
        note(8100, KITE, "anub.sphere", "none"),
        {"t": 7000, "e": "cast", "s": KITE, "sp": 75, "tgt": S1, "ct": 0},
        {"t": 7600, "e": "cast", "s": KITE, "sp": 75, "tgt": S2, "ct": 0},

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

        note(0, TANK, "anub.pickup", "attack"),
        note(40500, TANK, "anub.pickup", "taunt"),
        {"t": 42500, "e": "cast", "s": TANK, "sp": 355, "tgt": BOSS, "ct": 0},
        note(43000, TANK, "anub.pickup", "hold"),

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

    def test_impale_on_anyone_but_the_kiter_is_stray(self):
        stray = ab.stray_impales(self.trace, ab.mark_windows(self.trace))
        self.assertEqual([(rec["t"], rec["d"]) for rec in stray], [(29000, TANK)])

    def test_spheres_split_flying_from_patches_by_height(self):
        self.assertEqual(ab.sphere_timeline(ab.Samples(self.trace)),
                         [(-2000, 0, 0, 0), (0, 6, 0, 0), (8000, 4, 2, 2), (25000, 3, 3, 3),
                          (32000, 3, 2, 2)])

    def test_only_a_patch_gone_just_after_a_mark_is_consumed(self):
        samples = ab.Samples(self.trace)
        self.assertEqual(ab.patch_lives(self.trace, samples),
                         {S1: (8000, 59000), S2: (8000, 59000), S3: (25000, 31000)})
        self.assertEqual(ab.consumed_patches(self.trace, samples), {S3: (31000, 30000)})

    def test_heroic_spheres_left_at_each_submerge_and_phase_3(self):
        left = ab.spheres_left(self.trace, ab.Samples(self.trace))
        self.assertEqual(left, [("P2", 20000, 4), ("P3", 50000, 3), ("end", 59000, 3)])

    def test_sphere_shots_count_casts_aimed_at_one(self):
        self.assertEqual(ab.sphere_shots(self.trace), {KITE: 2})

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
        self.assertEqual([(rec["t"], rec["s"]) for rec in emerge["taunts"]], [(42500, TANK)])

    def test_the_drag_anchor_is_the_patch_midpoint_once_both_latch(self):
        rows = ab.threat_rows(self.trace)
        self.assertAlmostEqual(rows[1]["off_anchor"], 5.0)

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
