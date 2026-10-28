"""Tests for the Rotface reader.

    python -m unittest discover -s tools/botobs/tests

A synthetic 25H pull pins each section, and the shared v14 fixture shows every section reads empty on
a trace that is not Rotface rather than raising.
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

from bosses import rotface as rf  # noqa: E402
from raidobs.trace import Trace  # noqa: E402

FULL_V14 = BOTOBS / "fixtures" / "full-v14.ndjson"

CREATURE = 1 << 32
ROTFACE = CREATURE | 700
OOZE_A = CREATURE | 701
OOZE_B = CREATURE | 702
BULWARK = 7301  # main tank
AEGIS = 7302    # assist tank, the kiter
MEND = 7303     # healer
ARCANE = 7304   # ranged
ARKOS = 7305    # human ranged

ROSTER = ((BULWARK, "Bulwark", "tank", 0), (AEGIS, "Aegis", "tank", 0), (MEND, "Mend", "heal", 0),
          (ARCANE, "Arcane", "ranged", 0), (ARKOS, "Arkos", "ranged", 1))


def note(when: int, guid: int, key: str, value: str) -> dict:
    return {"t": when, "e": "note", "g": guid, "k": key, "txt": value}


def snap(when: int, ooze_a_on: int, ooze_b_on: int | None = None) -> dict:
    units = [[ROTFACE, 0.0, 0.0, 0.0, 0.0, 80.0, 0.0, BULWARK, 0, 0, 0, 0],
             [OOZE_A, 30.0, 0.0, 0.0, 0.0, 100.0, 0.0, ooze_a_on, 0, 0, 0, 0]]
    if ooze_b_on is not None:
        units.append([OOZE_B, 0.0, 30.0, 0.0, 0.0, 100.0, 0.0, ooze_b_on, 0, 0, 0, 0])
    return {"t": when, "e": "snap", "hz": [], "u": units}


def pull(boss: str = "rotface", diff: int = 3) -> list[dict]:
    return [
        {"e": "hdr", "v": 14, "ts": 1789800000000, "map": 631, "inst": 5, "diff": diff, "boss": boss,
         "bin": 1789799000000, "cfg": {"cheats": "food,taxi,raid", "mapthreads": 2, "iccbuffs": 0},
         "roster": [{"g": guid, "n": name, "r": role, "c": "warrior", "h": human}
                    for guid, name, role, human in ROSTER]},
        {"t": 0, "e": "pull", "boss": boss, "src": "engage"},
        {"t": 1, "e": "unit", "g": ROTFACE, "en": 36627, "n": "Rotface", "lvl": 83, "b": 1},
        {"t": 1, "e": "unit", "g": OOZE_A, "en": 36899, "n": "Big Ooze"},
        {"t": 1, "e": "unit", "g": OOZE_B, "en": 36899, "n": "Big Ooze"},
        # Ooze A sits on Mend for a second before Aegis picks it up; ooze B goes straight to Aegis.
        snap(10000, MEND), snap(11000, AEGIS), snap(12000, AEGIS, AEGIS), snap(13000, AEGIS, AEGIS),
        note(100, BULWARK, "rf.target", "boss"),
        note(100, ARCANE, "rf.target", "boss"),
        note(9000, AEGIS, "rf.target", "ooze"),
        note(100, 0, "rf.kiter", "0"),
        note(200, 0, "rf.kiter", str(AEGIS)),
        note(500, AEGIS, "rf.bigooze", "none"),
        note(8000, AEGIS, "rf.bigooze", "spot"),
        note(10000, AEGIS, "rf.bigooze", f"pickup {OOZE_A}"),
        note(11000, AEGIS, "rf.bigooze", f"tank {OOZE_A}"),
        note(30000, AEGIS, "rf.bigooze", f"kite {OOZE_A}"),
        # Arcane is infected, waits at the spot for a partner, then walks to the Big Ooze and gets cured.
        note(100, ARCANE, "rf.carrier", "none"),
        note(4000, ARCANE, "rf.carrier", "spot"),
        note(10000, ARCANE, "rf.carrier", "ooze"),
        note(14000, ARCANE, "rf.carrier", "none"),
        note(13000, ARCANE, "rf.dispel", str(MEND)),
        note(13500, ARCANE, "rf.dispel", "0"),
        note(100, MEND, "rf.carrier", "none"),
        # The flood moves on, Arcane's outer slot floods and it takes another.
        note(100, ARCANE, "rf.slot", "7"),
        note(100, MEND, "rf.slot", "1"),
        note(9000, ARCANE, "rf.slot", "12"),
        note(8000, 0, "rf.flood", "1"),
        note(33000, 0, "rf.flood", "2"),
        note(4000, 0, "rf.oozespot", "3"),
        note(40000, 0, "rf.oozespot", "0"),
        # An explosion: Arcane and Mend step off their marks, Mend finds nothing clear from a puddle.
        note(20100, ARCANE, "rf.dodge", "mark move 9"),
        note(20200, ARCANE, "rf.dodge", "hold"),
        note(20100, MEND, "rf.dodge", "mark tight 7"),
        note(26000, MEND, "rf.dodge", "sticky none"),
        note(27000, BULWARK, "rf.dodge", "spray move 3"),
        note(15000, ARCANE, "rf.vilegas", "73020"),
        note(35000, ARKOS, "rf.vilegas", "73020"),
        note(40000, ARCANE, "rf.vilegas", "73020"),
        {"t": 25000, "e": "cheat", "g": AEGIS, "k": "rf.spitefulfury", "n": 3},
        {"t": 26000, "e": "cheat", "g": MEND, "k": "pp.freeooze", "n": 1},
        {"t": 60000, "e": "end", "out": "wipe"},
    ]


def load(records: list[dict]) -> Trace:
    with tempfile.TemporaryDirectory() as folder:
        path = pathlib.Path(folder) / f"631_5_{records[0]['boss']}_1789800000.ndjson"
        path.write_text("".join(json.dumps(rec) + "\n" for rec in records), encoding="utf-8")
        return Trace(path)


def printed(show, *args) -> str:
    out = io.StringIO()
    with contextlib.redirect_stdout(out), contextlib.redirect_stderr(io.StringIO()):
        show(*args)
    return out.getvalue()


class Oozes(unittest.TestCase):
    def test_share_counts_each_ooze_until_the_next_snapshot(self):
        share = rf.ooze_share(load(pull()))
        self.assertEqual(share, {"tank": 75.0, "heal": 25.0})

    def test_duty_names_the_ooze(self):
        rows = rf.duty_rows(load(pull()))
        self.assertEqual(rows[AEGIS], [("none", 500, 8000), ("spot", 8000, 10000), ("pickup Big Ooze", 10000, 11000),
                                       ("tank Big Ooze", 11000, 30000), ("kite Big Ooze", 30000, 60000)])

    def test_section_names_the_kiter(self):
        self.assertIn("  kiter: none@0:00.100, Aegis@0:00.200\n", printed(rf.show_oozes, load(pull())))


class Carriers(unittest.TestCase):
    def test_walks_skip_none_and_count_rules(self):
        text = printed(rf.show_carriers, load(pull()))
        self.assertIn("  merge walks: ooze 1, spot 1\n", text)
        self.assertIn("       0:04.000   0:06.000  spot\n", text)
        self.assertNotIn("Mend\n      ", text)

    def test_a_release_is_no_duty(self):
        self.assertEqual(rf.dispel_rows(load(pull())), [(13000, ARCANE, MEND)])
        self.assertIn("    0:13.000  Arcane          by Mend\n", printed(rf.show_carriers, load(pull())))


class Dodges(unittest.TestCase):
    def test_counts_drop_yards_and_holds(self):
        self.assertEqual(rf.dodge_counts(load(pull())),
                         {"mark move": 1, "mark tight": 1, "sticky none": 1, "spray move": 1})

    def test_section(self):
        text = printed(rf.show_dodges, load(pull()))
        self.assertIn("  by bot: Mend 2, Arcane 1, Bulwark 1\n", text)


class Spread(unittest.TestCase):
    def test_section(self):
        self.assertEqual(printed(rf.show_spread, load(pull())), (
            "SPREAD\n"
            "  flood: NE@0:08.000, NW@0:33.000\n"
            "  ooze spot: SW@0:04.000, none@0:40.000\n"
            "\n"
            "  rf.slot per bot, each slot it held\n"
            "    Arcane          outer 7@0:00.100, outer 12@0:09.000\n"
            "    Mend            inner 1@0:00.100\n"))

    def test_slot_names(self):
        self.assertEqual([rf.slot_name(value) for value in ("0", "6", "19", "20")],
                         ["released", "inner 6", "outer 19", "stack"])


class VileGas(unittest.TestCase):
    def test_section_marks_humans(self):
        text = printed(rf.show_vilegas, load(pull()))
        self.assertIn("  3 cast(s)\n", text)
        self.assertIn("     0:35.000  Arkos (human)\n", text)
        self.assertIn("  by victim: Arcane 2, Arkos 1\n", text)

    def test_normal_mode_has_none(self):
        self.assertIn("normal mode: Putricide casts no Vile Gas on Rotface",
                      printed(rf.show_vilegas, load(pull(diff=1))))


class Cheats(unittest.TestCase):
    def test_own_keys_only_and_the_rest_counted(self):
        self.assertEqual(printed(rf.show_cheats, load(pull())), (
            "CHEATS\n"
            "  rf.spitefulfury  3 use(s) from 0:25.000\n"
            "    by  Aegis 3\n"
            "    on  self\n"
            "  1 use(s) under other encounters' keys: icecrown_citadel.py --cheats\n"))


class Banner(unittest.TestCase):
    def test_normal_mode_does_not_miss_the_heroic_probes(self):
        records = [rec for rec in pull(diff=1) if rec.get("k") != "rf.vilegas"]
        self.assertNotIn("rf.vilegas", rf.missing_probes(load(records)))

    def test_heroic_names_them(self):
        records = [rec for rec in pull() if rec.get("k") != "rf.vilegas"]
        self.assertIn("rf.vilegas", rf.missing_probes(load(records)))

    def test_every_probe_the_pull_writes_counts(self):
        gone = rf.missing_probes(load(pull()))
        for key in ("rf.target", "rf.kiter", "rf.bigooze", "rf.carrier", "rf.dispel", "rf.slot", "rf.flood",
                    "rf.oozespot", "rf.dodge", "rf.vilegas"):
            self.assertNotIn(key, gone)


class EveryView(unittest.TestCase):
    def test_every_section_reads_empty_on_another_raid(self):
        trace = Trace(FULL_V14)
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            rf.show_banner(trace)
            for _, _, section in rf.SECTIONS:
                section(trace)
        self.assertIn("not an Icecrown Citadel trace", out.getvalue())
        self.assertEqual(out.getvalue().count("not a Rotface pull"), len(rf.SECTIONS))


if __name__ == "__main__":
    unittest.main()
