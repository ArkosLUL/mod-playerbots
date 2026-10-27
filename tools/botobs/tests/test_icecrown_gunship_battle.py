"""Tests for the Gunship Battle reader.

    python -m unittest discover -s tools/botobs/tests

A synthetic pull pins the freeze steps, the deck times, the star rules and the captain-name alias,
and the shared v14 fixture shows every section reads empty on a trace that is not the gunship.
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

from bosses import icecrown_gunship_battle as gsb  # noqa: E402
from raidobs.trace import Trace  # noqa: E402

FULL = BOTOBS / "fixtures" / "full-v14.ndjson"

BULWARK = 6301  # main tank
ARROW = 6302    # ranged
SHIV = 6303     # melee
ADD = (1 << 32) | 900


def note(when: int, guid: int, key: str, value: str) -> dict:
    return {"t": when, "e": "note", "g": guid, "k": key, "txt": value}


def pull(boss: str = "icecrown-gunship-battle") -> list[dict]:
    return [
        {"e": "hdr", "v": 14, "ts": 1789900000000, "map": 631, "inst": 3, "diff": 1, "boss": boss,
         "cfg": {"cheats": "food,taxi,raid", "iccbuffs": 0},
         "roster": [{"g": guid, "n": name, "r": role, "c": "x", "h": 0}
                    for guid, name, role in ((BULWARK, "Bulwark", "tank"), (ARROW, "Arrow", "ranged"),
                                             (SHIV, "Shiv", "melee"))]},
        {"t": 0, "e": "pull", "boss": boss, "src": "bossstate"},
        note(100, ARROW, gsb.PHASE, "fight"),
        note(100, SHIV, gsb.DECK, "friendly"),
        note(100, ARROW, gsb.DECK, "friendly"),
        note(30000, SHIV, gsb.PHASE, "frozen"),
        note(30000, SHIV, gsb.SKULL, "mage"),
        note(31000, SHIV, gsb.DECK, "between"),
        note(33000, SHIV, gsb.DECK, "enemy"),
        note(50000, SHIV, gsb.DECK, "between"),
        note(52000, SHIV, gsb.DECK, "friendly"),
        note(50000, ARROW, gsb.PHASE, "fight"),
        note(50000, SHIV, gsb.SKULL, "none"),
        note(55000, ARROW, gsb.STAR, f"fresh {ADD}"),
        note(61000, ARROW, gsb.STAR, f"handoff {ADD + 1}"),
        note(70000, ARROW, gsb.STAR, "none"),
        {"t": 35000, "e": "cheat", "g": BULWARK, "k": "gsb.battlefury", "n": 4, "tgt": ADD + 9},
        {"t": 90000, "e": "end", "out": "kill"},
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

    def test_the_freeze_is_a_phase_step(self):
        self.assertEqual(gsb.phase_steps(self.trace),
                         [("fight", 100, 30000), ("frozen", 30000, 50000), ("fight", 50000, 90000)])

    def test_deck_time_and_trips(self):
        shiv = gsb.deck_rows(self.trace)[SHIV]
        self.assertEqual(shiv["held"], {"friendly": 30900 + 38000, "between": 4000, "enemy": 17000})
        self.assertEqual(shiv["trips"], 1)

    def test_star_moves_count_by_rule(self):
        self.assertEqual(gsb.star_picks(self.trace), {"fresh": 1, "handoff": 1, "none": 1})

    def test_cheats(self):
        self.assertEqual([(key, entry["n"]) for key, entry in gsb.own_cheats(self.trace)], [("gsb.battlefury", 4)])

    def test_a_captain_named_trace_is_the_gunship(self):
        self.assertTrue(gsb.mine(load(pull(boss="muradin-bronzebeard"))))


class EveryView(unittest.TestCase):
    def test_every_section_reads_empty_on_another_boss(self):
        for trace in (Trace(FULL), load(pull(boss="deathbringer-saurfang"))):
            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                gsb.show_banner(trace)
                for _, _, section in gsb.SECTIONS:
                    section(trace)
            self.assertIn("not a Gunship Battle pull", out.getvalue())
            self.assertNotIn("trips", out.getvalue())


if __name__ == "__main__":
    unittest.main()
