"""Tests for the Twin Val'kyr reader.

    python -m unittest discover -s tools/botobs/tests

A synthetic pull pins the arithmetic behind each section, and the shared fixture shows every section
reads empty on a trace that is not the Twins rather than raising.
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

from bosses import val_kyr_twins as vk  # noqa: E402
from raidobs.trace import Trace  # noqa: E402

FULL = BOTOBS / "fixtures" / "full-v12.ndjson"

FJOLA = 4294968001
EYDIS = 4294968002
BULWARK = 5001  # tank on Fjola, Dark all pull with no essence row
AEGIS = 5002    # tank on Eydis, Light all pull with no essence row
TIDE = 5003     # healer, never placed
SHIV = 5004     # melee, Dark to Light for the Vortex and back
FROST = 5005    # ranged, no essence until 5 s, then Dark, then Light for its Touch

KICK, COUNTERSPELL, HEROISM, TAUNT = 1766, 2139, 32182, 355
CX, CY, CZ = vk.ARENA_CENTER


def player(guid: int, target: int) -> list:
    return [guid, CX, CY + 10.0, CZ, 0.0, 100.0, 100.0, target, 0, 0, 0, 0]


def snap(when: int) -> dict:
    fjola_hp = 60.0 if when <= 50000 else 80.0
    units = [
        [FJOLA, CX + 5.0, CY, CZ, 0.0, fjola_hp, 0.0, TIDE if when in (30000, 31000) else BULWARK, 0, 0,
         65876 if 37000 <= when <= 50000 else 0, 0],
        [EYDIS, CX - 5.0, CY, CZ, 0.0, fjola_hp, 0.0, AEGIS, 0, 0,
         65875 if 27000 <= when <= 33000 else 0, 0],
        player(BULWARK, FJOLA),
        player(AEGIS, EYDIS),
        player(TIDE, 0),
        player(SHIV, FJOLA),
        player(FROST, EYDIS),
    ]
    return {"t": when, "e": "snap", "u": units, "hz": []}


def note(when: int, guid: int, key: str, text: str) -> dict:
    return {"t": when, "e": "note", "g": guid, "k": key, "txt": text}


def aura(when: int, guid: int, spell: int, removed: int, stacks: int = 1) -> dict:
    return {"t": when, "e": "aura", "d": guid, "s": 0, "sp": spell, "r": removed, "st": stacks}


def dmg(when: int, source: int, guid: int, spell: int, amount: int, absorbed: int = 0) -> dict:
    return {"t": when, "e": "dmg", "s": source, "d": guid, "sp": spell, "a": amount, "ab": absorbed}


def cast(when: int, source: int, spell: int, target: int = 0, cast_ms: int = 0) -> dict:
    return {"t": when, "e": "cast", "s": source, "sp": spell, "tgt": target, "ct": cast_ms}


def twins_pull() -> list[dict]:
    records = [
        {"e": "hdr", "v": 12, "ts": 1789500000000, "map": 649, "inst": 1, "diff": 2,
         "boss": "fjola-lightbane", "roster": [
             {"g": BULWARK, "n": "Bulwark", "r": "tank", "c": "warrior", "h": 0},
             {"g": AEGIS, "n": "Aegis", "r": "tank", "c": "paladin", "h": 0},
             {"g": TIDE, "n": "Tide", "r": "heal", "c": "shaman", "h": 0},
             {"g": SHIV, "n": "Shiv", "r": "melee", "c": "rogue", "h": 0},
             {"g": FROST, "n": "Frost", "r": "ranged", "c": "mage", "h": 0}]},
        {"t": 0, "e": "pull", "boss": "fjola-lightbane", "src": "engage"},
        {"t": 1, "e": "unit", "g": FJOLA, "en": vk.NPC_FJOLA, "n": "Fjola Lightbane", "b": 1},
        {"t": 1, "e": "unit", "g": EYDIS, "en": vk.NPC_EYDIS, "n": "Eydis Darkbane", "b": 1},
        {"t": 1, "e": "spell", "sp": KICK, "n": "Kick"},
        {"t": 1, "e": "spell", "sp": COUNTERSPELL, "n": "Counterspell"},
        {"t": 1, "e": "spell", "sp": HEROISM, "n": "Heroism"},
        {"t": 1, "e": "spell", "sp": TAUNT, "n": "Taunt"},
        note(0, BULWARK, "tv.tank", "fjola"),
        note(0, AEGIS, "tv.tank", "eydis"),

        # Only Fjola's Surge reaches Bulwark and only Eydis's reaches Aegis. Frost takes both before
        # its first essence, but its own rows decide.
        dmg(2000, FJOLA, BULWARK, 65767, 1500),
        dmg(4000, FJOLA, BULWARK, 65767, 1500),
        dmg(2000, EYDIS, AEGIS, 65769, 1500),
        dmg(2000, FJOLA, FROST, 65767, 1500),
        dmg(2000, EYDIS, FROST, 65769, 1500),

        note(1000, FROST, "tv.essence", "base:dark"),
        aura(5000, FROST, 65684, 0),

        # Light Vortex from 6 s, channel 14 s to 19 s. Shiv swaps in for it and back after.
        cast(6000, FJOLA, 66046, 0, 8000),
        note(6500, SHIV, "tv.essence", "vortex:light"),
        aura(8000, SHIV, 65684, 1),
        aura(8000, SHIV, 65686, 0),
        dmg(15000, FJOLA, TIDE, 66048, 6000),
        dmg(15000, FJOLA, SHIV, 66048, 0, 6000),
        dmg(15000, FJOLA, BULWARK, 66048, 6000),
        dmg(16000, FJOLA, BULWARK, 66048, 6000),
        note(19500, SHIV, "tv.essence", "base:dark"),
        aura(20000, SHIV, 65686, 1),
        aura(20000, SHIV, 65684, 0),

        # Touch of Light on Frost 21 s to 25 s, ended by taking Light.
        aura(21000, FROST, 67297, 0),
        note(21100, FROST, "tv.essence", "touch:light"),
        dmg(23000, FJOLA, FROST, 67297, 3000),
        dmg(23000, FJOLA, SHIV, 67297, 3000),
        dmg(23000, FJOLA, AEGIS, 67297, 0, 3000),
        aura(25000, FROST, 67297, 1),
        aura(25000, FROST, 65684, 1),
        aura(25000, FROST, 65686, 0),

        # Eydis's Pact from 26 s: the shield breaks at 30 s, Shiv's kick lands on it at 28 s, Frost's
        # Counterspell cuts the cast at 33.5 s. Heroism goes out inside the window.
        cast(26000, EYDIS, 65875, 0, 15000),
        note(26100, SHIV, "tv.shield", "2"),
        cast(26500, TIDE, HEROISM),
        cast(28000, SHIV, KICK, EYDIS),
        note(30000, SHIV, "tv.shield", "0"),
        note(30100, FROST, "tv.interrupt", "duty"),
        note(30100, SHIV, "tv.interrupt", "standby"),
        cast(32000, BULWARK, TAUNT, FJOLA),
        cast(33500, FROST, COUNTERSPELL, EYDIS),

        # Fjola's Pact from 36 s: the shield outlasts it and shared health jumps 20 points at 51 s.
        cast(36000, FJOLA, 65876, 0, 15000),
        note(36100, BULWARK, "tv.shield", "1"),
        note(52000, BULWARK, "tv.shield", "0"),

        # Orbs: Aegis soaks a Light one, Shiv eats it, Frost eats a Dark one.
        note(39500, SHIV, "tv.orb", "wrong"),
        {"t": 39600, "e": "move", "g": SHIV, "k": "point", "x": CX, "y": CY, "z": CZ, "ok": 1, "r": "",
         "by": vk.ORB_DODGE},
        dmg(40000, 0, AEGIS, 65795, 0, 8000),
        dmg(40000, 0, SHIV, 65795, 8000),
        aura(40000, AEGIS, 67590, 0, 1),
        aura(40001, AEGIS, 67590, 0, 8),
        note(40500, FROST, "tv.orb", "splash"),
        dmg(41000, 0, FROST, 65808, 8000),
        note(42000, SHIV, "tv.orb", "none"),
        aura(45000, AEGIS, 67590, 0, 40),
        # The same Empowered written twice in one tick, then a second one.
        aura(46000, SHIV, 65748, 0),
        aura(46050, SHIV, 65748, 0),
        aura(50000, SHIV, 65748, 0),
        {"t": 55000, "e": "end", "out": "kill"},
    ]
    records += [snap(when) for when in range(0, 56000, 1000)]
    return records


def pact_pull() -> list[dict]:
    """Two of Eydis's Pacts, both shields broken. Frost holds the duty in the first and dies before the
    second, so its last note never gets rewritten."""
    records = [
        {"e": "hdr", "v": 12, "ts": 1789500000000, "map": 649, "inst": 1, "diff": 1,
         "boss": "eydis-darkbane", "roster": [
             {"g": SHIV, "n": "Shiv", "r": "melee", "c": "rogue", "h": 0},
             {"g": FROST, "n": "Frost", "r": "ranged", "c": "mage", "h": 0}]},
        {"t": 0, "e": "pull", "boss": "eydis-darkbane", "src": "engage"},
        {"t": 1, "e": "unit", "g": EYDIS, "en": vk.NPC_EYDIS, "n": "Eydis Darkbane", "b": 1},
        {"t": 1, "e": "spell", "sp": KICK, "n": "Kick"},
        {"t": 1, "e": "spell", "sp": COUNTERSPELL, "n": "Counterspell"},
        cast(10000, EYDIS, 65875, 0, 15000),
        note(10100, SHIV, "tv.shield", "2"),
        note(12000, SHIV, "tv.shield", "0"),
        note(12100, FROST, "tv.interrupt", "duty"),
        note(12100, SHIV, "tv.interrupt", "standby"),
        cast(13000, FROST, COUNTERSPELL, EYDIS),
        {"t": 20000, "e": "death", "g": FROST, "killer": EYDIS, "cause": "melee"},
        cast(30000, EYDIS, 65875, 0, 15000),
        note(30100, SHIV, "tv.shield", "2"),
        note(32000, SHIV, "tv.shield", "0"),
        note(32100, SHIV, "tv.interrupt", "duty"),
        cast(33000, SHIV, KICK, EYDIS),
        {"t": 40000, "e": "end", "out": "kill"},
    ]
    for when in range(0, 41000, 1000):
        casting = 65875 if 10000 <= when <= 13000 or 30000 <= when <= 33000 else 0
        records.append({"t": when, "e": "snap", "hz": [], "u": [
            [EYDIS, CX, CY, CZ, 0.0, 50.0, 0.0, SHIV, 0, 0, casting, 0],
            player(SHIV, EYDIS),
            player(FROST, EYDIS)]})
    return records


def load(folder: str, records: list[dict]) -> Trace:
    path = pathlib.Path(folder) / "649_1_fjola-lightbane_1789500000.ndjson"
    path.write_text("\n".join(json.dumps(rec) for rec in records) + "\n", encoding="utf-8")
    return Trace(path)


class SyntheticPull(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.trace = load(cls.folder.name, twins_pull())

    @classmethod
    def tearDownClass(cls):
        cls.folder.cleanup()

    def test_a_first_removal_names_the_colour_held_before_the_trace(self):
        times, colours = vk.essence_timelines(self.trace)[SHIV]
        self.assertEqual(times[1:], [8000, 20000])
        self.assertEqual(colours, [vk.DARK, vk.LIGHT, vk.DARK])

    def test_a_first_apply_means_no_essence_before_it(self):
        self.assertEqual(vk.essence_timelines(self.trace)[FROST][1],
                         [vk.NONE, vk.DARK, vk.LIGHT])

    def test_a_bot_without_essence_rows_is_placed_by_the_surges(self):
        timelines = vk.essence_timelines(self.trace)
        self.assertEqual(timelines[BULWARK][1], [vk.DARK])
        self.assertEqual(timelines[AEGIS][1], [vk.LIGHT])
        self.assertEqual(timelines[TIDE][1], [vk.UNKNOWN])

    def test_time_without_essence_and_swaps_by_reason(self):
        rows = {row["guid"]: row for row in vk.essence_rows(self.trace)}
        self.assertEqual(rows[FROST]["bare"], 5000)
        self.assertEqual(rows[FROST]["swaps"], {"base": 1, "touch": 1})
        self.assertEqual(rows[SHIV]["bare"], 0)
        self.assertEqual(rows[SHIV]["swaps"], {"vortex": 1, "base": 1})
        self.assertIsNone(rows[TIDE]["bare"])
        self.assertEqual((rows[FROST]["start"], rows[FROST]["end"]), (vk.NONE, vk.LIGHT))

    def test_a_vortex_counts_only_unabsorbed_hits(self):
        casts = vk.vortexes(self.trace)
        self.assertEqual(len(casts), 1)
        self.assertEqual(casts[0]["colour"], vk.LIGHT)
        self.assertEqual((casts[0]["matched"], casts[0]["known"]), (2, 4))
        self.assertEqual(casts[0]["hits"], {TIDE: 1, BULWARK: 2})
        self.assertEqual(casts[0]["damage"], {TIDE: 6000, BULWARK: 12000})

    def test_a_touch_ended_by_the_swap_and_its_raid_damage(self):
        rows = vk.touches(self.trace)
        self.assertEqual(len(rows), 1)
        touch = rows[0]
        self.assertEqual((touch["guid"], touch["colour"], touch["held"], touch["ended"]),
                         (FROST, vk.LIGHT, 4000, "swap"))
        self.assertEqual((touch["ticks"], touch["victims"], touch["damage"], touch["spent"]),
                         (3, 3, 6000, 3000))

    def test_a_pact_cut_after_its_shield_broke(self):
        pact = vk.pacts(self.trace)[0]
        self.assertEqual((pact["twin"], pact["outcome"], pact["close"]), (EYDIS, "kicked", 34000))
        self.assertEqual((pact["up"], pact["down"], pact["broke"]), (26100, 30000, True))
        self.assertEqual([rec["s"] for rec in pact["shielded"]], [SHIV])
        self.assertEqual([rec["s"] for rec in pact["after"]], [FROST])
        self.assertEqual(pact["duty"], [FROST])
        self.assertEqual([rec["s"] for rec in pact["lust"]], [TIDE])

    def test_a_pact_that_went_through_reads_as_a_health_jump(self):
        pact = vk.pacts(self.trace)[1]
        self.assertEqual((pact["twin"], pact["outcome"], pact["close"]), (FJOLA, "healed", 51000))
        self.assertAlmostEqual(pact["jump"], 20.0)
        self.assertFalse(pact["broke"])
        # Frost's duty latch from the first Pact is stale here, and no shield broke to open a window.
        self.assertEqual(pact["duty"], [])
        self.assertEqual(pact["lust"], [])

    def test_orbs_split_taken_from_absorbed_and_fold_a_doubled_empowered(self):
        rows = {row["guid"]: row for row in vk.orb_rows(self.trace)}
        self.assertEqual((rows[SHIV]["taken"], rows[SHIV]["damage"], rows[SHIV]["dodges"]), (1, 8000, 1))
        self.assertEqual(rows[SHIV]["empowered"], 2)
        self.assertEqual((rows[AEGIS]["absorbed"], rows[AEGIS]["taken"], rows[AEGIS]["peak"]), (1, 0, 40))
        self.assertEqual(rows[FROST]["taken"], 1)

    def test_orb_rules_name_bots_not_rows(self):
        self.assertEqual(vk.orb_rules(self.trace), {"wrong": {SHIV}, "splash": {FROST}, "none": {SHIV}})

    def test_each_twin_against_her_assigned_tank(self):
        rows, apart = vk.tank_rows(self.trace)
        rows = {row["name"]: row for row in rows}
        self.assertEqual((rows["fjola"]["mine"], rows["fjola"]["alive"]), (53000, 55000))
        self.assertEqual(rows["fjola"]["taunts"], {BULWARK: 1})
        self.assertEqual(rows["eydis"]["mine"], 55000)
        self.assertAlmostEqual(rows["fjola"]["centre"], 5.0, places=3)
        self.assertAlmostEqual(min(apart), 10.0, places=3)

    def test_targets_follow_the_colour_and_the_pact(self):
        rows = {row["guid"]: row for row in vk.target_rows(self.trace)}
        self.assertEqual(set(rows), {SHIV, FROST})
        self.assertEqual((rows[SHIV]["right"], rows[SHIV]["wrong"]), (35000, 20000))
        self.assertEqual((rows[FROST]["right"], rows[FROST]["wrong"]), (15000, 40000))

    def test_every_section_prints(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            vk.show_banner(self.trace)
            for _, _, section in vk.SECTIONS:
                section(self.trace)
        self.assertIn("val-kyr-twins", out.getvalue())


class LoneTank(unittest.TestCase):
    def test_a_lone_tank_sends_every_dps_bot_to_fjola(self):
        with tempfile.TemporaryDirectory() as folder:
            trace = load(folder, twins_pull() + [note(44000, BULWARK, "tv.tank", "both")])
            rows = {row["guid"]: row for row in vk.target_rows(trace)}
        # Frost stays Light on Eydis, wrong from Fjola's Pact ending at 51 s until the end at 55 s
        self.assertEqual((rows[FROST]["right"], rows[FROST]["wrong"]), (11000, 44000))
        self.assertEqual((rows[SHIV]["right"], rows[SHIV]["wrong"]), (35000, 20000))


class TwoBrokenPacts(unittest.TestCase):
    def test_a_dead_bots_duty_does_not_carry_into_the_next_pact(self):
        with tempfile.TemporaryDirectory() as folder:
            rows = vk.pacts(load(folder, pact_pull()))
        self.assertEqual([row["broke"] for row in rows], [True, True])
        self.assertEqual(rows[0]["duty"], [FROST])
        self.assertEqual(rows[1]["duty"], [SHIV])


class EveryView(unittest.TestCase):
    def test_every_section_reads_empty_on_another_boss(self):
        trace = Trace(FULL)
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            vk.show_banner(trace)
            for _, _, section in vk.SECTIONS:
                section(trace)
        text = out.getvalue()
        self.assertIn("not a Twin Val'kyr pull", text)
        self.assertIn("no essence or Surge rows", text)
        self.assertIn("no Pact cast", text)
        self.assertIn("Fjola and Eydis were never sampled alive", text)


if __name__ == "__main__":
    unittest.main()
