"""Tests for the Gunship Battle reader.

    python -m unittest discover -s tools/botobs/tests

A synthetic pull pins the freeze steps, the thaw stacks, the roster, the deck times, the boarding windows,
the cannon aims, the dodges and the captain-name alias, and the shared v14 fixture shows every section
reads empty on a trace that is not the gunship.
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

BULWARK = 6301  # board tank
ARROW = 6302    # ranged cannoneer
SHIV = 6303     # melee boarder
WARD = 6304     # home tank
ADD = (1 << 32) | 900


def note(when: int, guid: int, key: str, value: str) -> dict:
    return {"t": when, "e": "note", "g": guid, "k": key, "txt": value}


def pull(boss: str = "icecrown-gunship-battle") -> list[dict]:
    return [
        {"e": "hdr", "v": 14, "ts": 1789900000000, "map": 631, "inst": 3, "diff": 1, "boss": boss,
         "cfg": {"cheats": "food,taxi,raid", "iccbuffs": 0},
         "roster": [{"g": guid, "n": name, "r": role, "c": "x", "h": 0}
                    for guid, name, role in ((BULWARK, "Bulwark", "tank"), (ARROW, "Arrow", "ranged"),
                                             (SHIV, "Shiv", "melee"), (WARD, "Ward", "tank"))]},
        {"t": 0, "e": "pull", "boss": boss, "src": "bossstate"},
        note(100, BULWARK, gsb.DUTY, "boardtank"),
        note(100, ARROW, gsb.DUTY, "cannon"),
        note(100, SHIV, gsb.DUTY, "boarder"),
        note(100, ARROW, gsb.PHASE, "fight"),
        note(100, SHIV, gsb.DECK, "friendly"),
        note(100, ARROW, gsb.DECK, "cannon"),
        note(1000, ARROW, gsb.CANNON, "mortar"),
        note(30000, SHIV, gsb.PHASE, "frozen"),
        note(30000, BULWARK, gsb.BOARD, "go"),
        note(30000, SHIV, gsb.BOARD, "wait"),
        note(30000, ARROW, gsb.CANNON, "none"),
        note(30500, ARROW, gsb.DECK, "friendly"),
        note(31000, SHIV, gsb.BOARD, "go"),
        note(31000, SHIV, gsb.DECK, "between"),
        note(33000, SHIV, gsb.DECK, "enemy"),
        note(33000, SHIV, gsb.TARGET, "mage"),
        note(33000, BULWARK, gsb.TARGET, "captain"),
        note(50000, ARROW, gsb.PHASE, "fight"),
        note(50000, BULWARK, gsb.FURY, "12"),
        note(50000, SHIV, gsb.BOARD, "back"),
        note(50000, SHIV, gsb.TARGET, "none"),
        note(50000, SHIV, gsb.DECK, "between"),
        note(52000, SHIV, gsb.DECK, "friendly"),
        note(52000, SHIV, gsb.BOARD, "home"),
        note(53000, ARROW, gsb.DECK, "cannon"),
        note(53000, ARROW, gsb.CANNON, "gunner"),
        note(60000, SHIV, gsb.DODGE, "rocket"),
        note(61000, SHIV, gsb.DODGE, "none"),
        {"e": "spell", "sp": 871, "n": "Shield Wall"},
        {"e": "spell", "sp": 12975, "n": "Last Stand"},
        {"e": "spell", "sp": 498, "n": "Divine Protection"},
        note(100, WARD, gsb.DECK, "friendly"),
        note(100, BULWARK, gsb.DECK, "enemy"),
        note(38000, BULWARK, gsb.FURY_WINDOW, "1"),
        note(38500, BULWARK, gsb.DEFENSIVE, "shield wall"),
        {"t": 38600, "e": "cast", "s": BULWARK, "sp": 871},
        note(39000, BULWARK, gsb.DEFENSIVE, "covered"),
        note(44000, BULWARK, gsb.FURY_WINDOW, "2"),
        note(44200, BULWARK, gsb.DEFENSIVE, "last stand"),
        {"t": 44300, "e": "cast", "s": BULWARK, "sp": 12975},
        note(66000, BULWARK, gsb.FURY_WINDOW, "0"),
        note(66000, BULWARK, gsb.DEFENSIVE, "idle"),
        note(75000, WARD, gsb.DEFENSIVE, "divine protection"),
        {"t": 75100, "e": "cast", "s": WARD, "sp": 498},
        note(75500, ARROW, gsb.HUMAN_CALL, "Shield Wall"),
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
        self.assertEqual(gsb.freezes(self.trace), [(30000, 50000)])

    def test_thaw_stacks_and_no_retreat(self):
        self.assertEqual(gsb.instance_marks(self.trace, gsb.FURY), [(50000, 12)])
        self.assertEqual(gsb.instance_marks(self.trace, gsb.RETREAT), [])

    def test_roster(self):
        self.assertEqual(gsb.duties(self.trace), {"boardtank": [BULWARK], "cannon": [ARROW], "boarder": [SHIV]})

    def test_deck_time_and_trips(self):
        shiv = gsb.deck_rows(self.trace)[SHIV]
        self.assertEqual(shiv["held"], {"friendly": 30900 + 38000, "between": 4000, "enemy": 17000})
        self.assertEqual(shiv["trips"], 1)

    def test_boarding_window(self):
        [row] = gsb.boarding(self.trace)
        self.assertEqual((row["go"], row["back"], row["went"], row["retreats"]), (30000, 50000, [BULWARK, SHIV], 0))

    def test_cannon_aims(self):
        self.assertEqual(set(gsb.span_time(self.trace, gsb.CANNON)), {"mortar", "none", "gunner"})

    def test_dodges(self):
        self.assertEqual(gsb.entries(self.trace, gsb.DODGE), {"rocket": (1, 1)})

    def test_the_stacks_open_one_stretch_of_two_windows(self):
        [stretch] = gsb.fury_stretches(self.trace)
        self.assertEqual((stretch["start"], stretch["stop"], stretch["levels"]),
                         (38000, 66000, [(38000, 1), (44000, 2)]))

    def test_defensives_section(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            gsb.show_defensives(self.trace)
        self.assertEqual(out.getvalue(), (
            "TANK DEFENSIVES\n"
            "  1  stacks past a window for 0:28.000: window 1 from 0:38.000, window 2 from 0:44.000\n"
            "      cast     Shield Wall Bulwark +0.6s, Last Stand Bulwark +6.3s\n"
            "      offered  Bulwark covered x1, Bulwark last stand x1, Bulwark shield wall x1\n"
            "  off the windows  Divine Protection Ward 1:15.100 on the friendly deck\n"
            "  calls to a human tank  Shield Wall 1:15.500\n"
            "\n"
            "  1 of 1 stack windows saw a tank button; a window opens when his stacks add 50% to his hit,"
            " the second at 100%\n"))

    def test_a_pull_with_no_window_says_so(self):
        quiet = [rec for rec in pull() if rec.get("k") not in (gsb.FURY_WINDOW, gsb.DEFENSIVE, gsb.HUMAN_CALL)
                 and rec.get("e") != "cast"]
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            gsb.show_defensives(load(quiet))
        self.assertIn("no tank button cast, and the captain's stacks never reached a window", out.getvalue())

    def test_no_cheat(self):
        self.assertEqual(gsb.own_cheats(self.trace), [])

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

    def test_every_section_prints_on_the_gunship(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            trace = load(pull())
            gsb.show_banner(trace)
            for _, _, section in gsb.SECTIONS:
                section(trace)
        self.assertIn("stacks at the thaw: 12", out.getvalue())
        self.assertIn("go +0:00", out.getvalue())


if __name__ == "__main__":
    unittest.main()
