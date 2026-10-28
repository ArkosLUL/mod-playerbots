"""Tests for the Lady Deathwhisper reader.

    python -m unittest discover -s tools/botobs/tests

A synthetic 25N pull pins the phase steps, the pick totals, the Dominate Mind rows with their duty, CC
chain and free time, the Frostbolt kicks, the dodge counts, the tank defensives, the threat brake and the
cheat filter, and the shared v14 fixture shows every section reads empty on a trace that is not hers.
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
        aura(38400, MEND, 33786, removed=1),
        aura(38500, MEND, 853),
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
        note(100500, BULWARK, ldw.DEFENSIVE, "shield wall"),
        note(101000, BULWARK, ldw.DEFENSIVE, "covered"),
        note(112500, BULWARK, ldw.DEFENSIVE, "none"),
        note(113000, AEGIS, ldw.DEFENSIVE, "none"),
        note(113400, BRAMBLE, ldw.HUMAN_CALL, "Shield Wall"),
        note(105000, BRAMBLE, ldw.BRAKE, "1"),
        note(108000, BRAMBLE, ldw.BRAKE, "0"),
        note(110000, BRAMBLE, ldw.BRAKE, "1"),
        note(111000, BRAMBLE, ldw.BRAKE, "0"),
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

    def test_dominate_rows_pair_the_duty_and_the_chain(self):
        self.assertEqual(ldw.dominations(self.trace),
                         [(31000, MEND, [BRAMBLE], [(32400, "Cyclone"), (38500, "Hammer of Justice")], 1500),
                          (31000, ARKOS, [], [], 12000)])

    def test_free_time_is_the_window_no_control_aura_covers(self):
        # The Cyclone ends at 38400 and the stun lands 100 ms later, so only the lead-in and that gap are free.
        auras = [aura(32400, MEND, 33786), aura(38400, MEND, 33786, removed=1), aura(38500, MEND, 853)]
        self.assertEqual(ldw.control_chain(auras, MEND, 31000, 43000),
                         ([(32400, "Cyclone"), (38500, "Hammer of Justice")], 1500))
        # A domination cut short at 35000 leaves only the lead-in free.
        self.assertEqual(ldw.control_chain(auras, MEND, 31000, 35000)[1], 1400)

    def test_cc_section_marks_the_human(self):
        text = printed(ldw.show_cc, self.trace)
        self.assertIn("Mend  duty Bramble  Cyclone +1400 ms, Hammer of Justice +7500 ms  free 1500 ms", text)
        self.assertIn("Arkos human  duty no duty  no CC  free 12000 ms", text)

    def test_defensives_count_each_answer_and_the_human_calls(self):
        self.assertEqual(ldw.defensive_rows(self.trace),
                         {BULWARK: {"shield wall": 1, "covered": 1, "none": 1}, AEGIS: {"none": 1}})
        self.assertEqual(ldw.human_calls(self.trace), [(113400, BRAMBLE, "Shield Wall")])
        text = printed(ldw.show_defensives, self.trace)
        self.assertIn("Bulwark", text)
        self.assertIn("Bramble told a human tank: Shield Wall", text)

    def test_brake_holds_sum_the_time_held(self):
        self.assertEqual(ldw.brake_holds(self.trace), {BRAMBLE: (2, 4000)})
        self.assertIn("2 hold(s)", printed(ldw.show_brake, self.trace))

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
