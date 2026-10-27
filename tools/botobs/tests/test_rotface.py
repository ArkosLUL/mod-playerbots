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
AEGIS = 7302    # assist tank
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
        note(100, BULWARK, "rf.skull", f"boss {ROTFACE}"),
        note(500, AEGIS, "rf.bigooze", "none"),
        note(500, BULWARK, "rf.bigooze", "none"),
        note(10000, AEGIS, "rf.bigooze", f"pickup {OOZE_A}"),
        note(11000, AEGIS, "rf.bigooze", f"kite {OOZE_A}"),
        {"t": 20000, "e": "cast", "s": OOZE_A, "sp": rf.SPELL_UNSTABLE_OOZE_EXPLOSION, "tgt": 0, "ct": 4000},
        note(20100, ARCANE, "rf.explosionslot", "3"),
        note(20100, MEND, "rf.explosionslot", "4"),
        note(30000, ARCANE, "rf.explosionslot", "5"),
        # Vile Gas on Arcane, which runs; on the human, who has no spot; Mend runs from the splash.
        note(15000, ARCANE, "rf.vilegas", "73020"),
        note(15050, ARCANE, "rf.vilegasspot", "offset 2"),
        note(15100, MEND, "rf.vilegasspot", "blocked"),
        note(35000, ARKOS, "rf.vilegas", "73020"),
        note(40000, ARCANE, "rf.vilegas", "73020"),
        note(40050, ARCANE, "rf.vilegasspot", "none"),
        note(40050, ARCANE, "rf.vilegasspot", "offset 0"),
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
        self.assertEqual(rows[AEGIS], [("none", 500, 10000), ("pickup Big Ooze", 10000, 11000),
                                       ("kite Big Ooze", 11000, 60000)])
        self.assertEqual(rows[BULWARK], [("none", 500, 60000)])


class Explosion(unittest.TestCase):
    def test_section(self):
        self.assertEqual(printed(rf.show_explosion, load(pull())), (
            "EXPLOSION\n"
            "  1 Unstable Ooze Explosion cast(s): 0:20.000\n"
            "\n"
            "  rf.explosionslot per bot, each slot it held\n"
            "    Arcane          3@0:20.100, 5@0:30.000\n"
            "    Mend            4@0:20.100\n"))


class VileGas(unittest.TestCase):
    def test_each_cast_finds_its_victims_spot(self):
        rows = rf.vile_gas_rows(load(pull()))
        self.assertEqual([(row["t"], row["victim"], row["spot"]) for row in rows], [
            (15000, ARCANE, "offset 2"), (35000, ARKOS, None), (40000, ARCANE, "offset 0"),
        ])

    def test_section_marks_humans_and_the_splash(self):
        text = printed(rf.show_vilegas, load(pull()))
        self.assertIn("     0:35.000  Arkos           human\n", text)
        self.assertIn("  ran without being the target: Mend\n", text)

    def test_normal_mode_has_none(self):
        self.assertIn("normal mode: Putricide casts no Vile Gas on Rotface",
                      printed(rf.show_vilegas, load(pull(diff=1))))


class Skull(unittest.TestCase):
    def test_section(self):
        self.assertEqual(printed(rf.show_skull, load(pull())), "SKULL\n     0:00.100  Bulwark         boss Rotface\n")


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
        records = [rec for rec in pull(diff=1) if not str(rec.get("k", "")).startswith("rf.vilegas")]
        self.assertIn("all rf.* probes present", printed(rf.show_banner, load(records)))

    def test_heroic_names_them(self):
        records = [rec for rec in pull() if not str(rec.get("k", "")).startswith("rf.vilegas")]
        self.assertIn("probes absent from this trace: rf.vilegas, rf.vilegasspot",
                      printed(rf.show_banner, load(records)))


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
