"""Tests for the Lady Deathwhisper reader.

    python -m unittest discover -s tools/botobs/tests

A synthetic 25N pull pins the phase steps, the pick totals, the Dominate Mind rows with their duty and
CC, the Frostbolt kicks, the dodge counts and the cheat filter, and the shared v14 fixture shows every
section reads empty on a trace that is not hers.
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
MEND = 6203     # healer, dominated, cycloned on duty
ARKOS = 6204    # human, dominated, no CC
BRAMBLE = 6205  # druid, holds the CC duty
BOSS = 7001


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
                                                    (MEND, "Mend", "heal", 0), (ARKOS, "Arkos", "melee", 1),
                                                    (BRAMBLE, "Bramble", "ranged", 0))]},
        {"t": 0, "e": "pull", "boss": boss, "src": "bossstate"},
        {"t": 0, "e": "unit", "g": BOSS, "en": ldw.ENTRY, "n": "Lady Deathwhisper", "b": 1},
        note(50, BULWARK, ldw.PHASE, "1"),
        note(60, MEND, ldw.PHASE, "1"),
        note(500, BULWARK, ldw.TARGET, "loose"),
        note(500, BRAMBLE, ldw.TARGET, "fanatic"),
        note(6000, BULWARK, ldw.TARGET, "boss"),
        note(30000, BRAMBLE, ldw.TARGET, "boss"),
        note(31050, BRAMBLE, ldw.CC, str(MEND)),
        note(32500, BRAMBLE, ldw.CC, "0"),
        aura(31000, MEND, ldw.SPELL_DOMINATE_MIND),
        aura(32400, MEND, 33786),
        aura(31000, ARKOS, ldw.SPELL_DOMINATE_MIND),
        note(40000, BRAMBLE, ldw.DODGE, "dnd"),
        note(41000, BRAMBLE, ldw.DODGE, "none"),
        note(42000, MEND, ldw.DODGE, "dnd"),
        note(43000, MEND, ldw.DODGE, "none"),
        note(44000, MEND, ldw.DODGE, "shade"),
        note(95000, MEND, ldw.PHASE, "2"),
        note(95300, BULWARK, ldw.PHASE, "2"),
        {"t": 100000, "e": "cast", "s": BOSS, "sp": 72007, "tgt": BULWARK, "ct": 2000},
        note(100100, AEGIS, ldw.INTERRUPTER, "1"),
        note(100400, AEGIS, ldw.INTERRUPTER, "0"),
        {"t": 112000, "e": "cast", "s": BOSS, "sp": 72007, "tgt": BULWARK, "ct": 2000},
        {"t": 40000, "e": "cheat", "g": BULWARK, "k": "ldw.nothreat", "n": 1},
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

    def test_picks_sum_over_bots(self):
        rows = ldw.target_time(self.trace)
        self.assertEqual(rows["boss"], (2, 114000 + 90000))
        self.assertEqual(rows["fanatic"], (1, 29500))

    def test_dominate_rows_pair_the_duty_and_the_cyclone(self):
        self.assertEqual(ldw.dominations(self.trace),
                         [(31000, MEND, [BRAMBLE], (32400, "Cyclone")), (31000, ARKOS, [], None)])

    def test_cc_section_marks_the_human(self):
        text = printed(ldw.show_cc, self.trace)
        self.assertIn("Mend  duty Bramble  Cyclone 1400 ms after", text)
        self.assertIn("Arkos human  duty no duty  no CC", text)

    def test_a_frostbolt_with_no_latch_is_counted(self):
        self.assertEqual(ldw.frostbolts(self.trace), [(100000, [AEGIS]), (112000, [])])
        self.assertIn("2 cast(s), 1 with nobody latched", printed(ldw.show_kicks, self.trace))

    def test_dodges_count_entries_not_exits(self):
        self.assertEqual(ldw.dodge_counts(self.trace), {"dnd": (2, 2), "shade": (1, 1)})

    def test_only_this_encounters_cheats(self):
        self.assertEqual([key for key, _ in ldw.own_cheats(self.trace)], ["ldw.nothreat"])


class EveryView(unittest.TestCase):
    def test_every_section_reads_empty_on_another_boss(self):
        for trace in (Trace(FULL), load(pull(boss="lord-marrowgar"))):
            text = printed(lambda t: [ldw.show_banner(t)] + [section(t) for _, _, section in ldw.SECTIONS], trace)
            self.assertIn("not a Lady Deathwhisper pull", text)
            self.assertNotIn("Cyclone", text)
            self.assertNotIn("bot(s)", text)


if __name__ == "__main__":
    unittest.main()
