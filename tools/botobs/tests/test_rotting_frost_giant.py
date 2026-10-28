"""Tests for the Rotting Frost Giant reader.

    python -m unittest discover -s tools/botobs/tests

A synthetic pull pins the plague count, a double from two casters, a cure and a death held to the
end, and the shared v14 fixture shows every section reads empty on a trace that is not the giant.
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

from bosses import rotting_frost_giant as rfg  # noqa: E402
from raidobs.trace import Trace  # noqa: E402

FULL = BOTOBS / "fixtures" / "full-v14.ndjson"

GIANT = (1 << 32) | 700
MEND = 6501   # infected by the giant, spreads to Arrow, cured
ARROW = 6502  # infected by Mend, then by the giant on top, dies carrying it


def aura(when: int, target: int, caster: int, spell: int, removed: int = 0) -> dict:
    return {"t": when, "e": "aura", "d": target, "s": caster, "sp": spell, "r": removed}


def pull(boss: str = "rotting-frost-giant") -> list[dict]:
    return [
        {"e": "hdr", "v": 14, "ts": 1789900000000, "map": 631, "inst": 3, "diff": 1, "boss": boss,
         "cfg": {"cheats": "food,taxi", "iccbuffs": 0},
         "roster": [{"g": MEND, "n": "Mend", "r": "heal", "c": "x", "h": 0},
                    {"g": ARROW, "n": "Arrow", "r": "ranged", "c": "x", "h": 0}]},
        {"t": 0, "e": "pull", "boss": boss, "src": "mark"},
        aura(15000, MEND, GIANT, rfg.SPELL_DEATH_PLAGUE),
        aura(30000, ARROW, MEND, rfg.SPELL_DEATH_PLAGUE),
        aura(30000, MEND, MEND, rfg.SPELL_RECENTLY_INFECTED),
        aura(30000, MEND, GIANT, rfg.SPELL_DEATH_PLAGUE, removed=1),
        aura(31000, ARROW, GIANT, rfg.SPELL_DEATH_PLAGUE),
        {"t": 15200, "e": "note", "g": MEND, "k": rfg.PARTNER, "txt": str(ARROW)},
        {"t": 30000, "e": "note", "g": MEND, "k": rfg.PARTNER, "txt": "0"},
        {"t": 30100, "e": "note", "g": ARROW, "k": rfg.PARTNER, "txt": str(MEND)},
        {"t": 45000, "e": "death", "g": ARROW, "killer": ARROW,
         "auras": [[rfg.SPELL_DEATH_PLAGUE, 1, 0, MEND, 30000, 45000, 0]]},
        {"t": 15010, "e": "cheat", "g": MEND, "k": "rfg.deathplague", "n": 1},
        {"t": 60000, "e": "end", "out": "wipe"},
    ]


def load(records: list[dict]) -> Trace:
    with tempfile.TemporaryDirectory() as folder:
        path = pathlib.Path(folder) / f"631_3_{records[0]['boss']}_1789900000.ndjson"
        path.write_text("".join(json.dumps(rec) + "\n" for rec in records), encoding="utf-8")
        return Trace(path)


class SyntheticPull(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.trace = load(pull())

    def test_infections_doubles_cures_and_deaths(self):
        self.assertEqual(rfg.plague_rows(self.trace), {
            MEND: {"infected": 1, "doubles": 0, "cured": 1, "died": 0},
            ARROW: {"infected": 2, "doubles": 1, "cured": 0, "died": 1},
        })

    def test_a_released_pair_ends_its_span(self):
        self.assertEqual(rfg.pairings(self.trace), [(15200, MEND, ARROW, 30000), (30100, ARROW, MEND, 60000)])

    def test_the_cheat_is_under_the_giants_own_prefix(self):
        self.assertEqual([key for key, _ in rfg.own_cheats(self.trace)], ["rfg.deathplague"])


class EveryView(unittest.TestCase):
    def test_every_section_reads_empty_on_another_boss(self):
        for trace in (Trace(FULL), load(pull(boss="festergut"))):
            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                rfg.show_banner(trace)
                for _, _, section in rfg.SECTIONS:
                    section(trace)
            self.assertIn("not a Rotting Frost Giant pull", out.getvalue())
            self.assertNotIn("infected", out.getvalue())


if __name__ == "__main__":
    unittest.main()
