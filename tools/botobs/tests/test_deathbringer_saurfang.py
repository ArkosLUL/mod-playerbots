"""Tests for the Deathbringer Saurfang reader.

    python -m unittest discover -s tools/botobs/tests

A synthetic 10N pull pins the phase steps, the stands, the pick totals, a swapped and yielded rune and
an unswapped one, a shared Blood Nova, the kill and control duties, a healed and an unhealed Mark, the tank
windows with a call to a human tank and a marked bot's button, a Hand of Protection duty and the cheat
filter, and the shared v14 fixture shows every section reads empty on a trace that is not his.
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

BULWARK = 6501  # main tank, tank spot 1
AEGIS = 6502    # off tank, tank spot 2, swaps
ARROW = 6503    # hunter, slot 7
FEL = 6504      # ranged, slot 6, then the stack; marked and dies
MEND = 6505     # healer, slot 1
SHIV = 6506     # melee
BEAST_A = (1 << 32) | 9001
BEAST_B = (1 << 32) | 9002


def note(when: int, guid: int, key: str, value) -> dict:
    return {"t": when, "e": "note", "g": guid, "k": key, "txt": str(value)}


def aura(when: int, target: int, spell: int) -> dict:
    return {"t": when, "e": "aura", "d": target, "s": 1 << 32, "sp": spell, "r": 0}


def nova(when: int, target: int) -> dict:
    return {"t": when, "e": "dmg", "s": 1 << 32, "d": target, "sp": 72380, "a": 8000}


def pull(boss: str = "deathbringer-saurfang") -> list[dict]:
    return [
        {"e": "hdr", "v": 14, "ts": 1789900000000, "map": 631, "inst": 3, "diff": 0, "boss": boss,
         "cfg": {"cheats": "food,taxi,raid", "iccbuffs": 0},
         "roster": [{"g": guid, "n": name, "r": role, "c": "x", "h": 0}
                    for guid, name, role in ((BULWARK, "Bulwark", "tank"), (AEGIS, "Aegis", "tank"),
                                             (ARROW, "Arrow", "ranged"), (FEL, "Fel", "ranged"),
                                             (MEND, "Mend", "heal"), (SHIV, "Shiv", "melee"))]},
        {"t": 0, "e": "pull", "boss": boss, "src": "bossstate"},
        {"t": 30000, "e": "unit", "g": BEAST_A, "en": 38508, "n": "Blood Beast A", "b": 0},
        {"t": 30000, "e": "unit", "g": BEAST_B, "en": 38508, "n": "Blood Beast B", "b": 0},
        note(50, BULWARK, dbs.PHASE, 1),
        note(100, ARROW, dbs.SLOT, 7),
        note(100, FEL, dbs.SLOT, 6),
        note(100, MEND, dbs.SLOT, 1),
        note(100, BULWARK, dbs.SLOT, 13),
        note(100, AEGIS, dbs.SLOT, 14),
        note(60000, FEL, dbs.SLOT, 12),
        note(200, ARROW, dbs.TARGET, "boss"),
        note(30500, ARROW, dbs.TARGET, "kill"),
        note(38000, ARROW, dbs.TARGET, "boss"),
        note(200, SHIV, dbs.TARGET, "boss"),
        note(33000, SHIV, dbs.TARGET, "reach"),
        note(36000, SHIV, dbs.TARGET, "boss"),
        nova(17000, ARROW),
        aura(20000, BULWARK, dbs.SPELL_RUNE_OF_BLOOD),
        note(20050, AEGIS, dbs.SWAP, 1),
        {"t": 20100, "e": "act", "g": AEGIS, "a": dbs.SWAP_ACTION, "rel": 67.0, "vd": "OK"},
        note(20400, AEGIS, dbs.SWAP, 0),
        note(20600, BULWARK, dbs.YIELD, 1),
        note(40000, BULWARK, dbs.YIELD, 0),
        note(30200, ARROW, dbs.KILL, BEAST_A),
        note(30200, FEL, dbs.KILL, BEAST_A),
        note(36000, ARROW, dbs.KILL, 0),
        note(30300, MEND, dbs.CONTROL, BEAST_B),
        note(31500, MEND, dbs.CONTROL, 0),
        nova(37000, FEL),
        nova(37002, MEND),
        aura(42000, AEGIS, dbs.SPELL_RUNE_OF_BLOOD),
        aura(50000, FEL, dbs.SPELL_MARK_OF_THE_FALLEN_CHAMPION),
        note(50100, FEL, dbs.MARKHEAL, MEND),
        {"t": 70000, "e": "death", "g": FEL, "killer": 1 << 32},
        aura(80000, ARROW, dbs.SPELL_MARK_OF_THE_FALLEN_CHAMPION),
        {"e": "spell", "sp": 871, "n": "Shield Wall"},
        {"e": "spell", "sp": 12975, "n": "Last Stand"},
        {"e": "spell", "sp": 10278, "n": "Hand of Protection"},
        note(50200, MEND, dbs.PROTECT, FEL),
        {"t": 50300, "e": "cast", "s": MEND, "sp": 10278},
        note(53000, MEND, dbs.PROTECT, 0),
        note(55000, FEL, dbs.PERSONAL, "Barkskin"),
        note(56000, FEL, dbs.PERSONAL, "none"),
        note(90000, BULWARK, dbs.DEFENSIVE, "last stand"),
        {"t": 90100, "e": "cast", "s": BULWARK, "sp": 12975},
        note(91000, BULWARK, dbs.DEFENSIVE, "covered"),
        note(93000, BULWARK, dbs.DEFENSIVE, "idle"),
        note(95000, ARROW, dbs.HUMAN_CALL, "Shield Wall"),
        note(100000, BULWARK, dbs.PHASE, 2),
        note(105000, AEGIS, dbs.DEFENSIVE, "shield wall"),
        {"t": 105050, "e": "cast", "s": AEGIS, "sp": 871},
        note(106000, AEGIS, dbs.DEFENSIVE, "idle"),
        {"t": 40000, "e": "cheat", "g": BULWARK, "k": "ldw.nothreat", "n": 1},
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

    def test_phase_steps(self):
        self.assertEqual(printed(dbs.show_phases, self.trace), (
            "PHASES\n"
            "     0:00.050   1:39.950  1\n"
            "     1:40.000   0:20.000  2\n"))

    def test_stands_keep_order_and_moves(self):
        self.assertEqual(dbs.stands(self.trace), {ARROW: [7], FEL: [6, 12], MEND: [1], BULWARK: [13], AEGIS: [14]})
        text = printed(dbs.show_slots, self.trace)
        self.assertIn("  slot 6   Fel  then stack\n", text)
        self.assertIn("  tank 2   Aegis\n", text)

    def test_target_totals(self):
        rows = dbs.target_time(self.trace)
        self.assertEqual(rows["kill"], (1, 7500))
        self.assertEqual(rows["reach"], (1, 3000))

    def test_swap_and_missed_swap(self):
        self.assertEqual(dbs.swaps(self.trace),
                         [(20000, BULWARK, AEGIS, 20100, 20600), (42000, AEGIS, None, None, None)])
        text = printed(dbs.show_swaps, self.trace)
        self.assertIn("Bulwark  Aegis taunted 0.1 s after, yielded 0.6 s after", text)
        self.assertIn("Aegis  nobody elected, never yielded", text)

    def test_a_shared_nova(self):
        self.assertEqual(dbs.novas(self.trace), [(17000, [ARROW]), (37000, [FEL, MEND])])
        self.assertIn("2 nova(s), 1 hit more than one player", printed(dbs.show_novas, self.trace))

    def test_kill_and_control_duties(self):
        self.assertEqual(dbs.duty_by_target(self.trace, dbs.KILL), {BEAST_A: (30200, {ARROW, FEL})})
        self.assertIn("Blood Beast A  2 on it: Arrow, Fel", printed(dbs.show_kills, self.trace))
        self.assertIn("0:30.300  Mend on Blood Beast B  held 0:01.200", printed(dbs.show_control, self.trace))

    def test_marks(self):
        self.assertEqual(dbs.marks(self.trace), [(50000, FEL, [MEND], 70000), (80000, ARROW, [], None)])
        text = printed(dbs.show_marks, self.trace)
        self.assertIn("Fel  Mend  died 1:10.000", text)
        self.assertIn("Arrow  no healer  alive", text)

    def test_tank_windows_run_from_the_first_offer_to_idle(self):
        self.assertEqual(dbs.tank_windows(self.trace), [
            (90000, 93000, BULWARK, ["last stand", "covered"]),
            (105000, 106000, AEGIS, ["shield wall"]),
        ])

    def test_defensives_section(self):
        self.assertEqual(printed(dbs.show_defensives, self.trace), (
            "TANK DEFENSIVES\n"
            "  1:30.000  Bulwark     0:03.000  peak or low offered last stand, covered; cast Last Stand +0.1s\n"
            "  1:45.000  Aegis       0:01.000  frenzy      offered shield wall; cast Shield Wall +0.1s\n"
            "  calls to a human tank  Shield Wall 1:35.000\n"
            "  marked bots' own buttons  Barkskin Fel 0:55.000\n"))

    def test_protect_duty_and_its_cast(self):
        self.assertEqual(dbs.protect_duties(self.trace), [(50200, 53000, MEND, FEL, 50300)])
        self.assertIn("0:50.200  Mend on Fel  held 0:02.800  cast +0.1s", printed(dbs.show_protect, self.trace))

    def test_cheats_filter_other_encounters(self):
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
            self.assertNotIn("Rune of Blood", out.getvalue().replace("RUNE OF BLOOD", ""))
            self.assertNotIn("nova(s)", out.getvalue())


if __name__ == "__main__":
    unittest.main()
