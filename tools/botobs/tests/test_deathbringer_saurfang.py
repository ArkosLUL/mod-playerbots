"""Tests for the Deathbringer Saurfang reader.

    python -m unittest discover -s tools/botobs/tests

A synthetic pull pins the first-slot rule, the skull steps and the rune and mark rows, and the shared
v14 fixture shows every section reads empty on a trace that is not Saurfang.
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

from bosses import deathbringer_saurfang as dbs  # noqa: E402
from raidobs.trace import Trace  # noqa: E402

FULL = BOTOBS / "fixtures" / "full-v14.ndjson"

BULWARK = 6401  # tank
AEGIS = 6402    # tank
ARROW = 6403    # ranged, slot 3
FEL = 6404      # ranged, slot 7, then gives it up
SHIV = 6405     # melee


def note(when: int, guid: int, key: str, value: str) -> dict:
    return {"t": when, "e": "note", "g": guid, "k": key, "txt": value}


def aura(when: int, target: int, spell: int) -> dict:
    return {"t": when, "e": "aura", "d": target, "s": 1 << 32, "sp": spell, "r": 0}


def pull(boss: str = "deathbringer-saurfang") -> list[dict]:
    return [
        {"e": "hdr", "v": 14, "ts": 1789900000000, "map": 631, "inst": 3, "diff": 0, "boss": boss,
         "cfg": {"cheats": "food,taxi,raid", "iccbuffs": 0},
         "roster": [{"g": guid, "n": name, "r": role, "c": "x", "h": 0}
                    for guid, name, role in ((BULWARK, "Bulwark", "tank"), (AEGIS, "Aegis", "tank"),
                                             (ARROW, "Arrow", "ranged"), (FEL, "Fel", "ranged"),
                                             (SHIV, "Shiv", "melee"))]},
        {"t": 0, "e": "pull", "boss": boss, "src": "bossstate"},
        note(100, ARROW, dbs.SLOT, "3"),
        note(100, FEL, dbs.SLOT, "7"),
        note(90000, FEL, dbs.SLOT, "0"),
        note(200, SHIV, dbs.SKULL, "boss"),
        note(40000, SHIV, dbs.SKULL, "beast"),
        note(52000, SHIV, dbs.SKULL, "boss"),
        aura(20000, BULWARK, dbs.SPELL_RUNE_OF_BLOOD),
        aura(40000, AEGIS, dbs.SPELL_RUNE_OF_BLOOD),
        aura(30000, ARROW, dbs.SPELL_MARK_OF_THE_FALLEN_CHAMPION),
        {"t": 120000, "e": "end", "out": "kill"},
    ]


def load(records: list[dict]) -> Trace:
    with tempfile.TemporaryDirectory() as folder:
        path = pathlib.Path(folder) / f"631_3_{records[0]['boss']}_1789900000.ndjson"
        path.write_text("".join(json.dumps(rec) + "\n" for rec in records), encoding="utf-8")
        return Trace(path)


def printed(show, trace: Trace) -> str:
    out = io.StringIO()
    with contextlib.redirect_stdout(out):
        show(trace)
    return out.getvalue()


class SyntheticPull(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.trace = load(pull())

    def test_an_erase_does_not_read_as_slot_zero(self):
        self.assertEqual(dbs.slots(self.trace), {ARROW: 3, FEL: 7})

    def test_skull_steps(self):
        self.assertEqual(printed(dbs.show_skull, self.trace), (
            "SKULL\n"
            "     0:00.200   0:39.800  boss\n"
            "     0:40.000   0:12.000  beast\n"
            "     0:52.000   1:08.000  boss\n"))

    def test_runes_and_marks(self):
        text = printed(dbs.show_runes, self.trace)
        self.assertIn("Rune of Blood 2 time(s)", text)
        self.assertIn("    0:40.000  Aegis\n", text)
        self.assertIn("Mark of the Fallen Champion 1 time(s): Arrow", text)

    def test_no_cheat_fired(self):
        self.assertIn("no dbs.* or icc.* cheat fired", printed(dbs.show_cheats, self.trace))


class EveryView(unittest.TestCase):
    def test_every_section_reads_empty_on_another_boss(self):
        for trace in (Trace(FULL), load(pull(boss="icecrown-gunship-battle"))):
            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                dbs.show_banner(trace)
                for _, _, section in dbs.SECTIONS:
                    section(trace)
            self.assertIn("not a Deathbringer Saurfang pull", out.getvalue())
            self.assertNotIn("Rune of Blood", out.getvalue())


if __name__ == "__main__":
    unittest.main()
