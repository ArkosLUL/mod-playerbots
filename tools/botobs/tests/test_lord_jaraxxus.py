"""Tests for the Lord Jaraxxus reader.

    python -m unittest discover -s tools/botobs/tests

A synthetic 25H pull pins the arithmetic behind each section, and the shared fixture shows every section
reads empty on a trace that is not Jaraxxus rather than raising.
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

from bosses import lord_jaraxxus as lj  # noqa: E402
from raidobs.trace import Trace  # noqa: E402

FULL = BOTOBS / "fixtures" / "full-v12.ndjson"

BOSS = 4294968300
MISTRESS = 4294968301
INFERNAL = 4294968302
PORTAL = 4294968303
FLAME = 4294968304
TANK = 6001
OT = 6002
MAGE = 6003
ROGUE = 6004
PRIEST = 6005

# 25H ids, so a reader that only knew the 10N id of a row would miss every one
FEL_FIREBALL = 66965
FEL_LIGHTNING = 67031
INCINERATE = 67051
BURNING_INFERNO = 67061
NETHER_POWER = 67108
LEGION_FLAME = 68125
LEGION_FLAME_TRAIL = 68128
FLAME_TICK = 67072
KISS = 67907
KISS_PUNISH = 67075
KISS_CAST = 67078
# rank 5, the one a level-80 rogue has
KICK = 38768
SPELLSTEAL = 30449
DISPEL_MAGIC = 988
GREATER_HEAL = 48063


def mage_x(when: int) -> float:
    """The mage carries Legion Flame 35 s to 43 s and walks 2 yd a second along x."""
    if when < 35000:
        return 20.0
    if when <= 43000:
        return 20.0 + 2.0 * (when - 35000) / 1000.0
    return 36.0


def snap(when: int) -> dict:
    # The rogue steps into the mage's trail at 38-39 s, next to the flame dropped at 26,0.
    rogue = (27.0, 1.0) if 38000 <= when <= 39000 else (2.0, 1.0)
    units = [
        [BOSS, 0.0, 0.0, 0.0, 0.0, 100.0 - when / 2000.0, 0.0, TANK, 0, 0, 0, 0],
        [TANK, 1.0, 0.0, 0.0, 0.0, 100.0, 0.0, BOSS, 0, 0, 0, 0],
        [OT, 5.0, 10.0, 0.0, 0.0, 100.0, 0.0, MISTRESS, 0, 0, 0, 0],
        [MAGE, mage_x(when), 0.0, 0.0, 0.0, 100.0, 80.0, BOSS, 0, 0, 0, 0],
        [ROGUE, rogue[0], rogue[1], 0.0, 0.0, 100.0, 0.0, BOSS, 0, 0, 0, 0],
        [PRIEST, 24.0, 6.0, 0.0, 0.0, 100.0, 90.0, MAGE, 0, 0, 0, 0],
    ]
    # Spinning Pain Spike puts the Mistress on the mage 30-34 s; Fel Streak the Infernal on the rogue.
    if 20000 <= when <= 50000:
        units.append([MISTRESS, 10.0, 10.0, 0.0, 0.0, 100.0 - (when - 20000) / 1000.0, 0.0,
                      MAGE if 30000 <= when <= 34000 else OT, 0, 0, 0, 0])
    if 40000 <= when <= 60000:
        units.append([INFERNAL, -10.0, 0.0, 0.0, 0.0, 100.0, 0.0, ROGUE if when <= 41000 else OT, 0, 0, 0, 0])
    if 20000 <= when <= 34000:
        units.append([PORTAL, 15.0, 0.0, 0.0, 0.0, 100.0, 0.0, 0, 0, 0, 0, 0])
    if when >= 38000:
        units.append([FLAME, 26.0, 0.0, 0.0, 0.0, 100.0, 0.0, 0, 0, 0, 0, 0])
    return {"t": when, "e": "snap", "u": units, "hz": []}


def aura(when: int, target: int, caster: int, spell: int, removed: bool, dur: int) -> dict:
    return {"t": when, "e": "aura", "d": target, "s": caster, "sp": spell, "r": int(removed), "st": 1,
            "dur": dur, "p": 0}


def jaraxxus_pull() -> list[dict]:
    records = [
        {"e": "hdr", "v": 12, "ts": 1789600000000, "map": 649, "inst": 7, "diff": 3,
         "boss": "lord-jaraxxus", "roster": [
             {"g": TANK, "n": "Bulwark", "r": "tank", "c": "warrior", "h": 0},
             {"g": OT, "n": "Aegis", "r": "tank", "c": "paladin", "h": 0},
             {"g": MAGE, "n": "Frost", "r": "ranged", "c": "mage", "h": 0},
             {"g": ROGUE, "n": "Shiv", "r": "melee", "c": "rogue", "h": 0},
             {"g": PRIEST, "n": "Mend", "r": "heal", "c": "priest", "h": 0}]},
        {"t": 0, "e": "pull", "boss": "lord-jaraxxus", "src": "engage"},
        {"t": 1, "e": "unit", "g": BOSS, "en": lj.NPC_JARAXXUS, "n": "Lord Jaraxxus", "b": 1},
        {"t": 1, "e": "unit", "g": MISTRESS, "en": lj.NPC_MISTRESS_OF_PAIN, "n": "Mistress of Pain"},
        {"t": 1, "e": "unit", "g": INFERNAL, "en": lj.NPC_FELFLAME_INFERNAL, "n": "Felflame Infernal"},
        {"t": 1, "e": "unit", "g": PORTAL, "en": lj.NPC_NETHER_PORTAL, "n": "Nether Portal"},
        {"t": 1, "e": "unit", "g": FLAME, "en": lj.NPC_LEGION_FLAME, "n": "Legion Flame"},

        # Three Fel Fireballs: the first lands (its DoT tick too), the second is kicked, and the third
        # lands with a kick that came after the cast went off.
        {"t": 5000, "e": "cast", "s": BOSS, "sp": FEL_FIREBALL, "tgt": TANK, "ct": 2000},
        {"t": 7200, "e": "dmg", "s": BOSS, "d": TANK, "sp": FEL_FIREBALL, "a": 34000},
        {"t": 8200, "e": "dmg", "s": BOSS, "d": TANK, "sp": FEL_FIREBALL, "a": 10700},
        {"t": 17000, "e": "cast", "s": BOSS, "sp": FEL_FIREBALL, "tgt": TANK, "ct": 2000},
        {"t": 17100, "e": "note", "g": ROGUE, "k": "jaraxxus.interrupter", "txt": "1"},
        {"t": 17800, "e": "cast", "s": ROGUE, "sp": KICK, "tgt": BOSS, "ct": 0},
        # the kick ends the cast, and the rogue's next tick sees no cast
        {"t": 17850, "e": "note", "g": ROGUE, "k": "jaraxxus.interrupter", "txt": "0"},
        {"t": 30000, "e": "cast", "s": BOSS, "sp": FEL_FIREBALL, "tgt": TANK, "ct": 2000},
        {"t": 30100, "e": "note", "g": MAGE, "k": "jaraxxus.interrupter", "txt": "1"},
        {"t": 32000, "e": "note", "g": MAGE, "k": "jaraxxus.interrupter", "txt": "0"},
        {"t": 32200, "e": "dmg", "s": BOSS, "d": TANK, "sp": FEL_FIREBALL, "a": 34000},
        {"t": 32500, "e": "cast", "s": ROGUE, "sp": KICK, "tgt": BOSS, "ct": 0},

        # Fel Lightning chains to three, then one, and a stray hit matches neither cast.
        {"t": 10000, "e": "cast", "s": BOSS, "sp": FEL_LIGHTNING, "tgt": MAGE, "ct": 0},
        {"t": 10000, "e": "dmg", "s": BOSS, "d": MAGE, "sp": FEL_LIGHTNING, "a": 11700},
        {"t": 10000, "e": "dmg", "s": BOSS, "d": PRIEST, "sp": FEL_LIGHTNING, "a": 11700},
        {"t": 10001, "e": "dmg", "s": BOSS, "d": ROGUE, "sp": FEL_LIGHTNING, "a": 11700},
        {"t": 20000, "e": "cast", "s": BOSS, "sp": FEL_LIGHTNING, "tgt": TANK, "ct": 0},
        {"t": 20000, "e": "dmg", "s": BOSS, "d": TANK, "sp": FEL_LIGHTNING, "a": 11700},
        {"t": 25000, "e": "dmg", "s": BOSS, "d": MAGE, "sp": FEL_LIGHTNING, "a": 11700},

        # Nether Power: restated at 0 when the trace opens, 10 at 25 s, a Spellsteal takes two and a
        # Dispel Magic one, gone at 40 s. The second Spellsteal comes with nothing up.
        {"t": -500, "e": "note", "g": TANK, "k": "jaraxxus.netherpower", "txt": "0"},
        {"t": 25000, "e": "cast", "s": BOSS, "sp": NETHER_POWER, "tgt": BOSS, "ct": 0},
        {"t": 25000, "e": "note", "g": TANK, "k": "jaraxxus.netherpower", "txt": "10"},
        {"t": 26500, "e": "cast", "s": MAGE, "sp": SPELLSTEAL, "tgt": BOSS, "ct": 0},
        aura(26500, MAGE, MAGE, NETHER_POWER, False, 30000),
        {"t": 27000, "e": "note", "g": MAGE, "k": "jaraxxus.netherpower", "txt": "8"},
        {"t": 29500, "e": "cast", "s": PRIEST, "sp": DISPEL_MAGIC, "tgt": BOSS, "ct": 0},
        {"t": 30000, "e": "note", "g": PRIEST, "k": "jaraxxus.netherpower", "txt": "7"},
        {"t": 40000, "e": "note", "g": TANK, "k": "jaraxxus.netherpower", "txt": "0"},
        {"t": 45000, "e": "cast", "s": MAGE, "sp": SPELLSTEAL, "tgt": BOSS, "ct": 0},
        aura(56500, MAGE, MAGE, NETHER_POWER, True, 0),

        # Incinerate Flesh: healed off the mage after two heals, then runs its 12 s on the rogue and
        # Burning Inferno follows. The 31 s heal lands after the first window closed.
        aura(24000, MAGE, BOSS, INCINERATE, False, 12000),
        {"t": 25000, "e": "heal", "s": PRIEST, "d": MAGE, "sp": GREATER_HEAL, "a": 8000, "oh": 0, "hp": 100.0},
        {"t": 27000, "e": "heal", "s": PRIEST, "d": MAGE, "sp": GREATER_HEAL, "a": 9000, "oh": 0, "hp": 100.0},
        aura(30000, MAGE, BOSS, INCINERATE, True, 6000),
        {"t": 31000, "e": "heal", "s": PRIEST, "d": MAGE, "sp": GREATER_HEAL, "a": 5000, "oh": 5000, "hp": 100.0},
        aura(46000, ROGUE, BOSS, INCINERATE, False, 12000),
        aura(58000, ROGUE, BOSS, INCINERATE, True, 0),
        {"t": 59000, "e": "dmg", "s": ROGUE, "d": TANK, "sp": BURNING_INFERNO, "a": 7800},
        {"t": 60000, "e": "dmg", "s": ROGUE, "d": MAGE, "sp": BURNING_INFERNO, "a": 7800},

        # Legion Flame on the mage, 2 s of the debuff and 6 s of the trail. One leg is refused.
        aura(35000, MAGE, BOSS, LEGION_FLAME, False, 2000),
        {"t": 35100, "e": "note", "g": MAGE, "k": "jaraxxus.flame", "txt": "carrier"},
        {"t": 35200, "e": "move", "g": MAGE, "k": "point", "x": 32.0, "y": 0.0, "z": 0.0, "ok": 1, "r": "",
         "by": lj.AVOID},
        aura(37000, MAGE, BOSS, LEGION_FLAME, True, 0),
        aura(37000, MAGE, MAGE, LEGION_FLAME_TRAIL, False, 6000),
        {"t": 38000, "e": "move", "g": MAGE, "k": "point", "x": 38.0, "y": 0.0, "z": 0.0, "ok": 1, "r": "",
         "by": lj.AVOID},
        {"t": 38000, "e": "note", "g": ROGUE, "k": "jaraxxus.flame", "txt": "dodge"},
        {"t": 38500, "e": "dmg", "s": FLAME, "d": ROGUE, "sp": FLAME_TICK, "a": 7300},
        {"t": 39000, "e": "note", "g": MAGE, "k": "jaraxxus.flame", "txt": "relaxed"},
        {"t": 39000, "e": "move", "g": MAGE, "k": "point", "x": 40.0, "y": 0.0, "z": 0.0, "ok": 0,
         "r": "wait", "by": lj.AVOID},
        {"t": 39500, "e": "dmg", "s": FLAME, "d": ROGUE, "sp": FLAME_TICK, "a": 7300},
        {"t": 40000, "e": "note", "g": ROGUE, "k": "jaraxxus.flame", "txt": "none"},
        {"t": 41000, "e": "note", "g": MAGE, "k": "jaraxxus.flame", "txt": "carrier"},
        {"t": 41000, "e": "move", "g": MAGE, "k": "point", "x": 44.0, "y": 0.0, "z": 0.0, "ok": 1, "r": "",
         "by": lj.AVOID},
        {"t": 42500, "e": "dmg", "s": FLAME, "d": MAGE, "sp": FLAME_TICK, "a": 7300},
        aura(43000, MAGE, MAGE, LEGION_FLAME_TRAIL, True, 0),
        {"t": 43500, "e": "note", "g": MAGE, "k": "jaraxxus.flame", "txt": "none"},

        # Focus goes portal, Mistress, Infernal, none; the assist tank holds the Mistress, then the
        # Infernal, then nothing.
        {"t": 20100, "e": "note", "g": ROGUE, "k": "jaraxxus.focus", "txt": str(PORTAL)},
        {"t": 20200, "e": "note", "g": OT, "k": "jaraxxus.addtank", "txt": str(MISTRESS)},
        {"t": 34500, "e": "note", "g": MAGE, "k": "jaraxxus.focus", "txt": str(MISTRESS)},
        {"t": 50500, "e": "note", "g": ROGUE, "k": "jaraxxus.focus", "txt": str(INFERNAL)},
        {"t": 50600, "e": "note", "g": OT, "k": "jaraxxus.addtank", "txt": str(INFERNAL)},
        {"t": 60500, "e": "note", "g": ROGUE, "k": "jaraxxus.focus", "txt": "0"},
        {"t": 60500, "e": "note", "g": OT, "k": "jaraxxus.addtank", "txt": "0"},

        # Two kisses: the priest's is punished mid-cast, the mage's runs out under two cast holds. The
        # priest's veto comes after its kiss was spent and must not count.
        {"t": 43500, "e": "cast", "s": MISTRESS, "sp": KISS_CAST, "tgt": 0, "ct": 1500},
        aura(45000, PRIEST, MISTRESS, KISS, False, 15000),
        {"t": 47000, "e": "dmg", "s": MISTRESS, "d": PRIEST, "sp": KISS_PUNISH, "a": 13600},
        aura(47000, PRIEST, MISTRESS, KISS, True, 0),
        aura(52000, MAGE, MISTRESS, KISS, False, 15000),
        {"t": 53000, "e": "veto", "g": MAGE, "m": lj.KISS_HOLD, "a": "frostbolt"},
        {"t": 54000, "e": "veto", "g": MAGE, "m": lj.KISS_HOLD, "a": "frostbolt"},
        {"t": 55000, "e": "veto", "g": PRIEST, "m": lj.KISS_HOLD, "a": "greater heal"},
        aura(67000, MAGE, MISTRESS, KISS, True, 0),

        {"t": 70000, "e": "end", "out": "wipe"},
    ]
    records += [snap(when) for when in range(0, 71000, 1000)]
    return records


class SyntheticPull(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        path = pathlib.Path(cls.folder.name) / "649_7_lord-jaraxxus_1789600000.ndjson"
        path.write_text("\n".join(json.dumps(rec) for rec in jaraxxus_pull()) + "\n", encoding="utf-8")
        cls.trace = Trace(path)

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_a_kicked_fireball_is_not_counted_as_landed_and_a_late_kick_is_not_counted(self):
        rows = lj.fel_fireballs(self.trace)
        self.assertEqual([row["outcome"] for row in rows], ["landed", "kicked", "landed"])
        self.assertEqual(rows[1]["kicks"], [(17800, ROGUE, KICK)])
        self.assertEqual(rows[2]["kicks"], [])

    def test_the_interrupter_latch_is_read_inside_each_cast(self):
        self.assertEqual([row["latched"] for row in lj.fel_fireballs(self.trace)], [[], [ROGUE], [MAGE]])

    def test_lightning_hits_group_by_cast(self):
        rows, stray = lj.fel_lightning(self.trace)
        self.assertEqual([len(row["victims"]) for row in rows], [3, 1])
        self.assertEqual(rows[0]["damage"], 35100)
        self.assertEqual(stray, 1)

    def test_nether_uptime_clips_to_the_pull_and_uses_the_heroic_cap(self):
        self.assertEqual(lj.nether_uptime(self.trace), (15000, 2000, 10))

    def test_a_removal_counts_only_while_he_has_stacks(self):
        rows = lj.nether_removals(self.trace)
        self.assertEqual({guid: (row["casts"], row["up"], row["dropped"]) for guid, row in rows.items()},
                         {MAGE: (2, 1, 1), PRIEST: (1, 1, 1)})

    def test_a_steal_shows_as_nether_power_on_the_mage(self):
        self.assertEqual(lj.nether_steals(self.trace), {MAGE: 1})

    def test_incinerate_windows_count_heals_inside_and_inferno_after(self):
        rows = lj.incinerate_windows(self.trace)
        self.assertEqual([(row["guid"], row["ended"]) for row in rows], [(MAGE, "healed"), (ROGUE, "expired")])
        self.assertEqual((rows[0]["heals"], rows[0]["healed"], rows[0]["healers"]), (2, 17000, 1))
        self.assertEqual(rows[0]["inferno"], 0)
        self.assertEqual((rows[1]["inferno"], rows[1]["inferno_hits"]), (15600, 2))

    def test_debuff_and_trail_make_one_carrier_window(self):
        rows = lj.carrier_windows(self.trace, raid_clear=8.0, boss_clear=15.0)
        self.assertEqual(len(rows), 1)
        row = rows[0]
        self.assertEqual((row["guid"], row["start"], row["stop"]), (MAGE, 35000, 43000))
        self.assertAlmostEqual(row["walked"], 16.0)
        self.assertEqual(row["legs"], 2 + 1)
        self.assertAlmostEqual(row["near_min"], math.sqrt(2))
        self.assertAlmostEqual(row["near_share"], 5 / 9)
        self.assertAlmostEqual(row["boss_min"], 20.0)
        self.assertEqual(row["boss_share"], 0.0)
        self.assertEqual(row["branches"], {"carrier": 2, "relaxed": 1})
        self.assertFalse(row["tank"])

    def test_branch_counts_are_switches_across_every_bot(self):
        self.assertEqual(lj.flame_branches(self.trace), {"carrier": 2, "relaxed": 1, "dodge": 1, "none": 2})

    def test_fire_ticks_per_bot_with_the_nearest_flame(self):
        rows = {row["guid"]: row for row in lj.fire_rows(self.trace)}
        self.assertEqual((rows[ROGUE]["hits"], rows[ROGUE]["damage"], rows[ROGUE]["carrying"]), (2, 14600, 0))
        self.assertEqual((rows[MAGE]["hits"], rows[MAGE]["carrying"]), (1, 1))
        for gap in rows[ROGUE]["gaps"]:
            self.assertAlmostEqual(gap, math.sqrt(2))
        self.assertEqual(rows[MAGE]["gaps"], [8.0])

    def test_add_rows_share_samples_on_a_non_tank(self):
        rows = {row["guid"]: row for row in lj.add_rows(self.trace)}
        self.assertEqual([row["kind"] for row in lj.add_rows(self.trace)], ["portal", "mistress", "infernal"])
        self.assertEqual((rows[MISTRESS]["first"], rows[MISTRESS]["last"]), (20000, 50000))
        self.assertAlmostEqual(rows[MISTRESS]["share"], 5 / 31)
        self.assertAlmostEqual(rows[INFERNAL]["share"], 2 / 21)
        self.assertIsNone(rows[PORTAL]["share"])

    def test_focus_and_addtank_changes(self):
        self.assertEqual(lj.focus_changes(self.trace),
                         [(20100, PORTAL), (34500, MISTRESS), (50500, INFERNAL), (60500, 0)])
        self.assertEqual(lj.addtank_changes(self.trace), {OT: [(20200, MISTRESS), (50600, INFERNAL), (60500, 0)]})

    def test_kiss_windows_name_the_punish_and_the_holds_inside(self):
        rows = lj.kiss_windows(self.trace)
        self.assertEqual([(row["guid"], row["ended"], row["punish"], row["vetoes"]) for row in rows],
                         [(PRIEST, "punished", 13600, 0), (MAGE, "expired", 0, 2)])
        self.assertEqual(lj.kiss_punishes(self.trace), ({PRIEST: (1, 13600)}, 0))

    def test_every_section_prints_on_the_pull(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            for _, _, section in lj.SECTIONS:
                section(self.trace)
        self.assertIn("Fel Fireball: 3 cast(s), 2 landed, 1 kicked", out.getvalue())


class EveryView(unittest.TestCase):
    def test_every_section_reads_empty_on_another_boss(self):
        trace = Trace(FULL)
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            lj.show_banner(trace)
            for _, _, section in lj.SECTIONS:
                section(trace)
        self.assertIn("not a Jaraxxus pull", out.getvalue())
        self.assertIn("Lord Jaraxxus was never named", out.getvalue())


if __name__ == "__main__":
    unittest.main()
