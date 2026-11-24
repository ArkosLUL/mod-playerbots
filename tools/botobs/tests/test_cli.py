from __future__ import annotations

import contextlib
import io
import pathlib
import sys
import unittest
from unittest import mock

BOTOBS = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BOTOBS))

from raidobs.cli import run_sections  # noqa: E402

FULL_V14 = BOTOBS / "fixtures" / "full-v14.ndjson"
SECTIONS = tuple((flag, flag, lambda trace, flag=flag: print(flag.upper())) for flag in ("phases", "deaths", "walk"))


class RunSectionsTest(unittest.TestCase):
    def run_reader(self, *flags: str, summary: tuple[str, ...] = ("phases", "walk")) -> str:
        out = io.StringIO()
        with mock.patch.object(sys, "argv", ["reader", str(FULL_V14), *flags]), contextlib.redirect_stdout(out):
            self.assertEqual(run_sections("A reader.", SECTIONS, lambda trace: print("BANNER"), summary), 0)
        return out.getvalue()

    def test_no_flag_prints_every_section(self):
        self.assertEqual(self.run_reader(), "BANNER\n\nPHASES\n\nDEATHS\n\nWALK\n")

    def test_summary_prints_the_banner_and_the_listed_sections(self):
        self.assertEqual(self.run_reader("--summary"), "BANNER\n\nPHASES\n\nWALK\n")

    def test_summary_adds_to_a_picked_section(self):
        self.assertEqual(self.run_reader("--summary", "--deaths"), "BANNER\n\nPHASES\n\nDEATHS\n\nWALK\n")

    def test_summary_without_a_list_prints_the_banner_only(self):
        self.assertEqual(self.run_reader("--summary", summary=()), "BANNER\n")

    def test_a_single_section(self):
        self.assertEqual(self.run_reader("--deaths"), "BANNER\n\nDEATHS\n")


if __name__ == "__main__":
    unittest.main()
