"""Tests for the Blood Prince trash reader.

    python -m unittest discover -s tools/botobs/tests

A synthetic trash pull pins the cheats section, and the shared v14 fixture shows every section reads
empty on a trace that is not the trash rather than raising.
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

from bosses import blood_prince_trash as trash  # noqa: E402
from raidobs.trace import Trace  # noqa: E402

FULL_V14 = BOTOBS / "fixtures" / "full-v14.ndjson"

BULWARK = 7701
SHIV = 7702


def pull(boss: str = "blood-prince-trash") -> list[dict]:
    return [
        {"e": "hdr", "v": 14, "ts": 1789800000000, "map": 631, "inst": 5, "diff": 0, "boss": boss,
         "bin": 1789799000000, "cfg": {"cheats": "food,taxi,raid", "mapthreads": 2, "iccbuffs": 0},
         "roster": [{"g": BULWARK, "n": "Bulwark", "r": "tank", "c": "warrior", "h": 0},
                    {"g": SHIV, "n": "Shiv", "r": "melee", "c": "rogue", "h": 0}]},
        {"t": 0, "e": "pull", "boss": boss, "src": "mark"},
        {"t": 4000, "e": "cheat", "g": SHIV, "k": "bpc.example", "n": 1},
        {"t": 5000, "e": "cheat", "g": BULWARK, "k": "rf.example", "n": 3, "tgt": SHIV},
        {"t": 60000, "e": "end", "out": "kill"},
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


class Cheats(unittest.TestCase):
    def test_every_key_listed_and_a_foreign_prefix_tagged(self):
        self.assertEqual(printed(trash.show_cheats, load(pull())), (
            "CHEATS\n"
            "  bpc.example  1 use(s) from 0:04.000\n"
            "    by  Shiv 1\n"
            "    on  self\n"
            "  rf.example  3 use(s) from 0:05.000  other encounter\n"
            "    by  Bulwark 3\n"
            "    on  Shiv\n"))

    def test_council_pull_is_refused(self):
        self.assertIn("not a Blood Prince trash pull", printed(trash.show_cheats, load(pull("blood-prince-council"))))


class EveryView(unittest.TestCase):
    def test_every_section_reads_empty_on_another_raid(self):
        trace = Trace(FULL_V14)
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            trash.show_banner(trace)
            for _, _, section in trash.SECTIONS:
                section(trace)
        self.assertIn("not an Icecrown Citadel trace", out.getvalue())
        self.assertEqual(out.getvalue().count("not a Blood Prince trash pull"), len(trash.SECTIONS))


if __name__ == "__main__":
    unittest.main()
