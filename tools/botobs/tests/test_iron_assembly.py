"""Tests for the Assembly of Iron reader.

    python -m unittest discover -s tools/botobs/tests

A synthetic hard mode pull pins the kill order verdict, healer mana at each member's death, an
Overwhelming Power carrier's Meltdown, a Fusion Punch kill with the strategy's verdict on it, a punch
before Steelbreaker is alone, the tank defensives by node, a member sinking under the floor and the
deaths per phase. The shared v14
fixture shows every section reads empty elsewhere.
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

from bosses import iron_assembly as ia  # noqa: E402
from raidobs.trace import Trace  # noqa: E402

FULL_V14 = BOTOBS / "fixtures" / "full-v14.ndjson"

TANK = 7801
PARTNER = 7802
PRIEST = 7803
DRUID = 7804
STEEL = (1 << 32) | 1
MOLGEIM = (1 << 32) | 2
BRUNDIR = (1 << 32) | 3


def snap(t: int, priest_mana: float, steel_hp: float = 100.0) -> dict:
    return {"t": t, "e": "snap", "u": [
        [TANK, 1608.0, 101.0, 427.3, 0, 100, 100, STEEL],
        [PARTNER, 1575.9, 132.3, 427.3, 0, 100, 100, STEEL],
        [PRIEST, 1577.2, 121.0, 427.3, 0, 100, priest_mana, 0],
        [STEEL, 1574.0, 134.0, 427.3, 0, steel_hp, 0, PARTNER],
    ]}


def alive(t: int, mask: int) -> dict:
    return {"t": t, "e": "note", "g": PRIEST, "k": "ironassembly.alive", "txt": str(mask)}


def pull(last_mask: int = 1, out: str = "wipe") -> list[dict]:
    return [
        {"e": "hdr", "v": 14, "ts": 1795290000000, "map": 603, "inst": 4, "diff": 1,
         "boss": "stormcaller-brundir", "bin": 1795289000000,
         "cfg": {"cheats": "food,taxi,raid", "mapthreads": 6, "hardmode": {"iron-assembly": 1}, "iccbuffs": 0},
         "roster": [{"g": TANK, "n": "Bulwark", "r": "tank", "c": 2, "h": 0},
                    {"g": PARTNER, "n": "Rampart", "r": "tank", "c": 6, "h": 0},
                    {"g": PRIEST, "n": "Mender", "r": "heal", "c": 5, "h": 0},
                    {"g": DRUID, "n": "Grove", "r": "heal", "c": 11, "h": 0}]},
        {"t": 0, "e": "unit", "g": STEEL, "en": 32867, "n": "Steelbreaker", "b": 1},
        {"t": 0, "e": "unit", "g": MOLGEIM, "en": 32927, "n": "Runemaster Molgeim", "b": 1},
        {"t": 0, "e": "unit", "g": BRUNDIR, "en": 32857, "n": "Stormcaller Brundir", "b": 1},
        {"t": 0, "e": "spell", "sp": 63493, "n": "Fusion Punch"},
        {"t": 0, "e": "spell", "sp": 63526, "n": "High Voltage"},
        {"t": 0, "e": "pull", "boss": "stormcaller-brundir", "src": "engage"},
        alive(10, 7),
        {"t": 9000, "e": "cast", "s": PRIEST, "sp": 34433, "tgt": BRUNDIR},
        snap(5000, 95.0),
        alive(100000, 3),
        snap(100000, 40.0),
        alive(200000, last_mask),
        snap(200000, 5.0, 99.0),
        {"t": 208000, "e": "aura", "d": TANK, "s": STEEL, "sp": 61888, "st": 1, "dur": 35000},
        {"t": 243000, "e": "aura", "d": TANK, "s": STEEL, "sp": 61888, "r": 1, "st": 1, "dur": 0},
        {"t": 243000, "e": "death", "g": TANK, "killer": TANK, "x": 1607.0, "y": 101.0, "z": 427.3,
         "auras": [[61888, 1, 0, STEEL, 208000, -1, 0]], "rewind": []},
        {"t": 243000, "e": "cast", "s": STEEL, "sp": 61902},
        snap(250000, 3.0, 60.0),
        {"t": 260000, "e": "dmg", "s": STEEL, "d": PARTNER, "sp": 63493, "a": 43618, "hp": 90.9, "rs": 10904},
        {"t": 260010, "e": "death", "g": PARTNER, "killer": STEEL, "x": 1575.9, "y": 132.3, "z": 427.3,
         "rewind": [[260000, STEEL, 63493, 43618]]},
        {"t": 270000, "e": "death", "g": PRIEST, "killer": STEEL, "x": 1577.2, "y": 121.0, "z": 427.3,
         "rewind": [[270000, STEEL, 63526, 6111]]},
        {"t": 280000, "e": "end", "out": out},
    ]


def load(records: list[dict]) -> Trace:
    with tempfile.TemporaryDirectory() as folder:
        path = pathlib.Path(folder) / "603_4_stormcaller-brundir_1795290000.ndjson"
        path.write_text("".join(json.dumps(rec) + "\n" for rec in records), encoding="utf-8")
        return Trace(path)


def printed(show, *args) -> str:
    out = io.StringIO()
    with contextlib.redirect_stdout(out), contextlib.redirect_stderr(io.StringIO()):
        show(*args)
    return out.getvalue()


class Banner(unittest.TestCase):
    def test_steelbreaker_last_on_a_kill_is_hard_mode(self):
        text = printed(ia.show_banner, load(pull(out="kill")))
        self.assertIn("outcome kill, hard mode triggered: Steelbreaker died last (toggle on)", text)

    def test_a_wipe_only_names_the_order(self):
        self.assertIn("hard mode order: Steelbreaker was the last one up", printed(ia.show_banner, load(pull())))

    def test_brundir_last_is_normal_mode(self):
        text = printed(ia.show_banner, load(pull(last_mask=4, out="kill")))
        self.assertIn("normal mode order: Brundir was the last one up", text)


class Phases(unittest.TestCase):
    def test_healer_mana_at_each_death_and_the_mana_returns(self):
        text = printed(ia.show_phases, load(pull()))
        self.assertIn("1:40.000  Brundir died, Steelbreaker + Molgeim left, healer mana Mender 40%", text)
        self.assertIn("3:20.000  Molgeim died, Steelbreaker left, healer mana Mender 5%", text)
        self.assertIn("Steelbreaker alone for 80s, lowest 60.0%, 60.0% at the end", text)
        self.assertIn("healers under 10% mana from: Mender 3:20.000", text)
        self.assertIn("0:09.000  Shadowfiend from Mender at 95% mana", text)


class Empowered(unittest.TestCase):
    def test_meltdown_and_punch(self):
        text = printed(ia.show_empowered, load(pull()))
        self.assertIn("3:28.000  Overwhelming Power on Bulwark: Meltdown at 4:03.000, 2 yd from the Meltdown point",
                      text)
        self.assertIn("4:20.000  Fusion Punch Rampart 43.6k at 91%, 1 charge(s), 10.9k resisted, no defensive  KILLED",
                      text)

    def test_the_verdict_and_button_on_a_punch(self):
        records = pull()
        records[-1:-1] = [
            {"t": 257200, "e": "note", "g": PARTNER, "k": "ironassembly.punch", "txt": "under 41k"},
            {"t": 257300, "e": "note", "g": PARTNER, "k": "ironassembly.punchdef", "txt": "icebound fortitude"},
        ]
        text = printed(ia.show_empowered, load(records))
        self.assertIn("4:20.000  Fusion Punch Rampart 43.6k at 91%, 1 charge(s), 10.9k resisted, expected 41k (under "
                      "the floor), defensive icebound fortitude  KILLED", text)

    def test_never_reached(self):
        self.assertIn("never reached", printed(ia.show_empowered, load(pull(last_mask=3))))


class Punches(unittest.TestCase):
    def test_every_phase_with_an_idle_row_between(self):
        records = pull()
        records[-1:-1] = [
            {"t": 147000, "e": "note", "g": TANK, "k": "ironassembly.punch", "txt": "safe 26k"},
            {"t": 150000, "e": "dmg", "s": STEEL, "d": TANK, "sp": 63493, "a": 27000, "hp": 100.0, "rs": 0},
            {"t": 150100, "e": "note", "g": TANK, "k": "ironassembly.punch", "txt": "idle"},
        ]
        text = printed(ia.show_punches, load(records))
        self.assertIn("2:30.000  Steelbreaker + Molgeim: Fusion Punch Bulwark 27.0k at 100%, 0 charge(s), "
                      "expected 26k (safe), no defensive", text)
        self.assertIn("4:20.000  Steelbreaker: Fusion Punch Rampart 43.6k at 91%, 1 charge(s), 10.9k resisted, "
                      "no defensive  KILLED", text)

    def test_who_else_the_blast_killed_and_the_rebirth_after(self):
        records = pull()
        records[1:1] = [{"t": 0, "e": "spell", "sp": 61889, "n": "Meltdown"}]
        records[-1:-1] = [
            {"t": 242700, "e": "death", "g": PRIEST, "killer": STEEL, "x": 1600.0, "y": 105.0, "z": 427.3,
             "rewind": [[242700, STEEL, 61889, 29250]]},
            {"t": 244000, "e": "cast", "s": DRUID, "sp": 48477, "tgt": TANK},
        ]
        text = printed(ia.show_empowered, load(records))
        self.assertIn("Meltdown at 4:03.000, 2 yd from the Meltdown point, 1 more killed", text)
        self.assertIn("4:04.000  Rebirth from Grove on Bulwark (tank)", text)


class Revived(unittest.TestCase):
    def test_buffs_back_inside_the_window_and_what_it_died_without(self):
        records = pull()
        records[1:1] = [{"t": 0, "e": "spell", "sp": sp, "n": name} for sp, name in
                        ((48469, "Mark of the Wild"), (20217, "Blessing of Kings"),
                         (25898, "Greater Blessing of Kings"), (48162, "Prayer of Fortitude"))]
        records[-1:-1] = [
            {"t": 150000, "e": "death", "g": PARTNER, "killer": STEEL, "x": 1575.9, "y": 132.3, "z": 427.3,
             "auras": [[25898, 1, 0, TANK, -1, -1, 1], [48162, 1, 0, PRIEST, -1, -1, 1]],
             "rewind": [[150000, STEEL, 63493, 50000]]},
            {"t": 160000, "e": "snap", "u": [[PARTNER, 1575.9, 132.3, 427.3, 0, 18, 100, 0]]},
            {"t": 161000, "e": "aura", "d": PARTNER, "s": DRUID, "sp": 48469, "st": 1, "dur": 1800000, "p": 1},
            {"t": 161200, "e": "note", "g": PRIEST, "k": "ironassembly.rebuff", "txt": f"none:nobody {PARTNER}"},
            {"t": 161500, "e": "aura", "d": PARTNER, "s": TANK, "sp": 20217, "st": 1, "dur": 600000, "p": 1},
            {"t": 200500, "e": "aura", "d": PARTNER, "s": PRIEST, "sp": 48162, "st": 1, "dur": 3600000, "p": 1},
        ]
        punched = next(rec for rec in records if rec.get("e") == "death" and rec["t"] == 260010)
        punched["auras"] = [[20217, 1, 0, TANK, 161500, -1, 1], [48469, 1, 0, DRUID, 161000, -1, 1]]
        text = printed(ia.show_revived, load(records))
        self.assertIn("2:40.000  Rampart up at 18%, dead since 2:30.000", text)
        self.assertIn("2:41.000    Mark of the Wild from Grove", text)
        self.assertIn("2:41.200    Mender: none:nobody", text)
        self.assertIn("2:41.500    Blessing of Kings from Bulwark", text)
        self.assertNotIn("Prayer of Fortitude from", text)
        self.assertIn("4:20.010  Rampart died again, without Fortitude", text)

    def test_a_buff_landing_before_the_first_live_sample_counts(self):
        records = pull()
        records[1:1] = [{"t": 0, "e": "spell", "sp": 48162, "n": "Prayer of Fortitude"}]
        records[-1:-1] = [
            {"t": 150000, "e": "death", "g": PARTNER, "killer": STEEL, "x": 1575.9, "y": 132.3, "z": 427.3,
             "auras": [[48162, 1, 0, PRIEST, -1, -1, 1]], "rewind": [[150000, STEEL, 63493, 50000]]},
            {"t": 159900, "e": "aura", "d": PARTNER, "s": PRIEST, "sp": 48162, "st": 1, "dur": 3600000, "p": 1},
            {"t": 160000, "e": "snap", "u": [[PARTNER, 1575.9, 132.3, 427.3, 0, 18, 100, 0]]},
        ]
        punched = next(rec for rec in records if rec.get("e") == "death" and rec["t"] == 260010)
        punched["auras"] = [[48162, 1, 0, PRIEST, 159900, -1, 1]]
        text = printed(ia.show_revived, load(records))
        self.assertIn("2:40.000  Rampart up at 18%, dead since 2:30.000", text)
        self.assertIn("2:39.900    Prayer of Fortitude from Mender", text)
        self.assertIn("4:20.010  Rampart died again, every raid buff back", text)

    def test_how_long_a_tank_risen_out_of_reach_took_to_reach_a_healer(self):
        records = pull()

        def at(t: int, x: float, y: float) -> dict:
            return {"t": t, "e": "snap", "u": [[PARTNER, x, y, 427.3, 0, 15, 100, 0],
                                               [PRIEST, 1560.0, 150.0, 427.3, 0, 100, 50, 0]]}

        records[-1:-1] = [
            {"t": 150000, "e": "death", "g": PARTNER, "killer": PARTNER, "x": 1608.4, "y": 99.8, "z": 427.3,
             "auras": [], "rewind": []},
            at(160000, 1608.4, 99.8), at(160500, 1608.4, 99.8), at(161000, 1608.4, 99.8),
            at(161500, 1600.0, 108.0), at(162000, 1592.0, 116.0), at(162500, 1585.0, 124.0),
        ]
        text = printed(ia.show_revived, load(records))
        self.assertIn("2:40.000    walk: a healer in 40 yd after 2.5s, stood 1.0s on the way", text)


class Defensives(unittest.TestCase):
    def test_who_was_on_the_tank_and_which_node_cast_it(self):
        records = pull()
        records[-1:-1] = [
            {"t": 249500, "e": "note", "g": PARTNER, "k": "ironassembly.punch", "txt": "under 41k"},
            {"t": 250200, "e": "cast", "s": TANK, "sp": 642, "tgt": 0},
            {"t": 250300, "e": "cast", "s": PARTNER, "sp": 48792, "tgt": 0},
            {"t": 250400, "e": "cast", "s": TANK, "sp": 48788, "tgt": TANK},
            {"t": 250500, "e": "cast", "s": TANK, "sp": 48788, "tgt": PRIEST},
        ]
        text = printed(ia.show_defensives, load(records))
        self.assertIn("4:10.200  Bulwark Divine Shield at 100%, nothing on it (class node)", text)
        self.assertIn("4:10.300  Rampart Icebound Fortitude at 100%, Steelbreaker on it (punch node)", text)
        self.assertIn("4:10.400  Bulwark Lay on Hands at 100%, nothing on it (class node)", text)
        self.assertEqual(text.count("Lay on Hands"), 1)
        self.assertIn("class casts with nothing on the tank: 2", text)


class Floor(unittest.TestCase):
    def test_a_member_walking_into_a_hole_in_the_floor(self):
        records = pull()

        def at(t: int, z: float) -> dict:
            return {"t": t, "e": "snap", "u": [[PRIEST, 1548.6, 147.9, z, 0, 90, 50, 0]]}

        records[-1:-1] = [
            {"t": 216000, "e": "note", "g": PRIEST, "k": "ironassembly.spot", "txt": "spread"},
            {"t": 216100, "e": "move", "g": PRIEST, "k": "point", "x": 1547.95, "y": 150.38, "z": 427.27, "tgt": 0,
             "ok": 1, "r": "", "by": "iron assembly raid position action", "pr": "combat"},
            at(217000, 425.5), at(217500, 421.0), at(218000, 400.0),
        ]
        text = printed(ia.show_floor, load(records))
        self.assertIn("3:37.500  Mender (heal) sank at (1548.6, 147.9), walking to (1548.0, 150.4) by iron assembly "
                      "raid position action, spot spread, down to Z 400, died at 4:30.000", text)

    def test_none_on_the_floor(self):
        self.assertIn("none", printed(ia.show_floor, load(pull())))


class Deaths(unittest.TestCase):
    def test_by_mechanic_and_the_first_one_that_was_not_a_meltdown(self):
        text = printed(ia.show_deaths, load(pull()))
        self.assertIn("Steelbreaker up: 3", text)
        self.assertIn("1  Meltdown", text)
        self.assertIn("1  Fusion Punch", text)
        self.assertIn("1  High Voltage", text)
        self.assertIn("not a Meltdown: Rampart (tank) at 4:20.010, Fusion Punch", text)

    def test_a_spell_the_victim_cast_on_itself_is_named(self):
        trace = load(pull())
        own = {"t": 250000, "g": PRIEST, "killer": PRIEST, "blow": [PRIEST, 6111],
               "rewind": [[249990, PRIEST, 63526, 6111]]}
        self.assertEqual(ia.mechanic(trace, own), "High Voltage")
        fall = {"t": 250000, "g": PRIEST, "killer": PRIEST, "blow": [PRIEST, 30000], "rewind": []}
        self.assertEqual(ia.mechanic(trace, fall), "own blow")


class OtherFights(unittest.TestCase):
    def test_every_section_reads_empty(self):
        trace = Trace(FULL_V14)
        self.assertIn("not an Assembly of Iron pull", printed(ia.show_banner, trace))
        for show in (ia.show_phases, ia.show_empowered, ia.show_punches, ia.show_revived, ia.show_defensives,
                     ia.show_floor, ia.show_deaths):
            self.assertEqual(len(printed(show, trace).splitlines()), 1)


if __name__ == "__main__":
    unittest.main()
