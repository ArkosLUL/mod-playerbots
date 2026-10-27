"""Tests for the Lady Deathwhisper reader.

    python -m unittest discover -s tools/botobs/tests

A synthetic 25N pull pins the phase steps, the skull steps, the Dominate Mind rows and the cheat
filter, and the shared v14 fixture shows every section reads empty on a trace that is not hers.
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

from bosses import lady_deathwhisper as ldw  # noqa: E402
from raidobs.trace import Trace  # noqa: E402

FULL = BOTOBS / "fixtures" / "full-v14.ndjson"

BULWARK = 6201  # tank
AEGIS = 6202    # tank
MEND = 6203     # healer, dominated and cycloned
ARKOS = 6204    # human, dominated, no Cyclone


def note(when: int, guid: int, key: str, value: str) -> dict:
    return {"t": when, "e": "note", "g": guid, "k": key, "txt": value}


def aura(when: int, target: int, spell: int, removed: int = 0) -> dict:
    return {"t": when, "e": "aura", "d": target, "s": target, "sp": spell, "r": removed}


def pull(boss: str = "lady-deathwhisper") -> list[dict]:
    return [
        {"e": "hdr", "v": 14, "ts": 1789900000000, "map": 631, "inst": 3, "diff": 1, "boss": boss,
         "cfg": {"cheats": "food,taxi,raid", "iccbuffs": 0},
         "roster": [{"g": guid, "n": name, "r": role, "c": "x", "h": human}
                    for guid, name, role, human in ((BULWARK, "Bulwark", "tank", 0), (AEGIS, "Aegis", "tank", 0),
                                                    (MEND, "Mend", "heal", 0), (ARKOS, "Arkos", "melee", 1))]},
        {"t": 0, "e": "pull", "boss": boss, "src": "bossstate"},
        note(50, BULWARK, ldw.PHASE, "1"),
        note(60, MEND, ldw.PHASE, "1"),
        note(500, BULWARK, ldw.SKULL, "boss"),
        note(6000, AEGIS, ldw.SKULL, "add"),
        note(6100, BULWARK, ldw.SKULL, "add"),
        note(30000, AEGIS, ldw.SKULL, "boss"),
        note(95000, MEND, ldw.PHASE, "2"),
        note(95300, BULWARK, ldw.PHASE, "2"),
        aura(31000, MEND, ldw.SPELL_DOMINATE_MIND),
        aura(31200, MEND, ldw.SPELL_CYCLONE),
        aura(31000, ARKOS, ldw.SPELL_DOMINATE_MIND),
        {"t": 31200, "e": "cheat", "g": MEND, "k": "ldw.cyclone", "n": 1},
        {"t": 31300, "e": "cheat", "g": BULWARK, "k": "ldw.cyclone", "n": 1, "tgt": ARKOS},
        {"t": 40000, "e": "cheat", "g": BULWARK, "k": "dbs.example", "n": 1},
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

    def test_phase_two_starts_at_the_first_bot_to_see_the_barrier_break(self):
        self.assertEqual(ldw.phase_steps(self.trace), [("1", 50, 95000), ("2", 95000, 120000)])

    def test_skull_steps_merge_the_tanks(self):
        self.assertIn("     0:06.000   0:24.000  add\n", printed(ldw.show_skull, self.trace))

    def test_a_cyclone_inside_the_window_is_paired(self):
        self.assertEqual(ldw.dominations(self.trace), [(31000, MEND, 31200), (31000, ARKOS, None)])

    def test_dominate_section_marks_the_human(self):
        self.assertIn("Arkos human  no Cyclone", printed(ldw.show_dominate, self.trace))

    def test_only_this_encounters_cheats(self):
        rows = ldw.own_cheats(self.trace)
        self.assertEqual([key for key, _ in rows], ["ldw.cyclone"])
        self.assertEqual(rows[0][1]["n"], 2)


class EveryView(unittest.TestCase):
    def test_every_section_reads_empty_on_another_boss(self):
        for trace in (Trace(FULL), load(pull(boss="lord-marrowgar"))):
            text = printed(lambda t: [ldw.show_banner(t)] + [section(t) for _, _, section in ldw.SECTIONS], trace)
            self.assertIn("not a Lady Deathwhisper pull", text)
            self.assertNotIn("Cyclone", text)


if __name__ == "__main__":
    unittest.main()
