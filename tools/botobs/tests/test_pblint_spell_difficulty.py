"""Tests for pblint's --spell-difficulty sweep.

    python -m unittest discover -s tools/botobs/tests

No DB: the merge is fed rows directly and the sweep runs over a temp src/Ai/Raid tree.
"""
from __future__ import annotations

import importlib.util
import os
import pathlib
import tempfile
import textwrap
import unittest
from unittest import mock

REPO = pathlib.Path(__file__).resolve().parents[3]
_spec = importlib.util.spec_from_file_location("pblint", REPO / "tools" / "pblint" / "pblint.py")
pblint = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(pblint)

LIGHT_ESSENCE = [405, 65686, 67222, 67223, 67224]
LIGHT_TOUCH = [464, 65950, 67296, 67297, 67298]
PLASMA_BLAST = [62997, 62997, 64529, 0, 0]

TABLE = pblint.merge_spell_difficulty([LIGHT_ESSENCE, LIGHT_TOUCH], [PLASMA_BLAST])


class MergeTest(unittest.TestCase):
    def test_both_sources_contribute(self):
        self.assertEqual(TABLE.rows[405], (65686, 67222, 67223, 67224))
        self.assertEqual(TABLE.rows[62997], (62997, 64529))

    def test_db_wins_on_a_shared_row_id(self):
        table = pblint.merge_spell_difficulty([[7, 11111, 22222, 33333, 44444]],
                                              [[7, 55555, 66666, 0, 0]])
        self.assertEqual(table.rows, {7: (55555, 66666)})
        self.assertNotIn(11111, table.row_of)

    def test_a_degenerate_db_row_still_replaces_the_dbc_row(self):
        table = pblint.merge_spell_difficulty([[7, 11111, 22222, 0, 0]], [[7, 11111, 0, 0, 0]])
        self.assertEqual(table.rows, {})
        self.assertEqual(table.row_of, {})

    def test_degenerate_rows_are_dropped(self):
        table = pblint.merge_spell_difficulty([
            [1, 0, 22222, 33333, 0],       # first id unset
            [2, 11111, 0, 33333, 0],       # second id unset
            [3, 11111, -1, 33333, 0],      # second id negative
            [4, 44444, 44444, 0, 0],       # one distinct id
            [5, 55555, 55556, -1, 0],      # valid, the negative id is left out
        ], [])
        self.assertEqual(table.rows, {5: (55555, 55556)})

    def test_every_member_indexes_its_row(self):
        for spell in LIGHT_TOUCH[1:]:
            self.assertEqual(TABLE.row_of[spell], 464)
        self.assertEqual(TABLE.row_of[64529], 62997)

    def test_a_member_of_two_rows_indexes_the_higher_row_id(self):
        table = pblint.merge_spell_difficulty([[9, 11111, 22222, 0, 0], [3, 11111, 33333, 0, 0]],
                                              [])
        self.assertEqual(table.row_of[11111], 9)


class SourceTest(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.tmp = pathlib.Path(tmp.name)
        self.csv = self.tmp / "spelldifficulty.reference.csv"
        self.csv.write_text("ID,DifficultySpellID_1,DifficultySpellID_2,DifficultySpellID_3,"
                            "DifficultySpellID_4\n405,65686,67222,67223,67224\n", encoding="utf-8")

    def test_reads_the_client_csv(self):
        with mock.patch.dict(os.environ, {"PB_SPELL_DIFFICULTY_CSV": str(self.csv)}):
            self.assertEqual(pblint._client_csv_rows(), [LIGHT_ESSENCE])

    def test_a_missing_csv_is_an_error(self):
        env = {"PB_SPELL_DIFFICULTY_CSV": str(self.tmp / "missing.csv")}
        with mock.patch.dict(os.environ, env), self.assertRaises(RuntimeError):
            pblint.spell_difficulty_table()

    def test_a_missing_db_is_an_error_even_with_the_csv(self):
        env = {"PB_SPELL_DIFFICULTY_CSV": str(self.csv), "PB_MYSQL": "pblint-no-such-mysql-client"}
        with mock.patch.dict(os.environ, env), self.assertRaises(RuntimeError):
            pblint.spell_difficulty_table()


class SweepTest(unittest.TestCase):
    def sweep(self, files: dict[str, str]) -> list:
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp).resolve()
            for rel, text in files.items():
                path = root / "src" / "Ai" / "Raid" / rel
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(textwrap.dedent(text).lstrip("\n"), encoding="utf-8")
            with mock.patch.object(pblint, "REPO", root):
                sources = pblint.collect([root / "src" / "Ai" / "Raid"])
            return pblint.check_spell_difficulty(sources, TABLE)

    def essence(self, declaration: str, use: str = "bot->HasAura(SPELL_LIGHT_ESSENCE);") -> list:
        return self.sweep({"X/Data.h": f"""
            enum XSpells
            {{
                {declaration}
            }};
            """, "X/Action.cpp": f"""
            bool Check()
            {{
                return {use}
            }}
            """})

    def test_a_row_with_unreferenced_ids_is_reported(self):
        findings = self.essence("SPELL_LIGHT_ESSENCE = 65686,")
        self.assertEqual(len(findings), 1)
        finding = findings[0]
        self.assertEqual((finding.rel, finding.line), ("src/Ai/Raid/X/Data.h", 3))
        self.assertFalse(finding.error)
        for part in ("SPELL_LIGHT_ESSENCE (65686)", "row 405", "67222, 67223, 67224"):
            self.assertIn(part, finding.message)

    def test_a_qualified_constant_in_the_remap_argument_is_handled(self):
        use = "bot->HasAura(sSpellMgr->GetSpellIdForDifficulty(XSpells::SPELL_LIGHT_ESSENCE, bot));"
        self.assertEqual(self.essence("SPELL_LIGHT_ESSENCE = 65686,", use), [])

    def test_a_cast_constant_in_the_remap_argument_is_handled(self):
        use = ("bot->HasAura(sSpellMgr->GetSpellIdForDifficulty(\n"
               "        static_cast<uint32>(SPELL_LIGHT_ESSENCE), bot));")
        self.assertEqual(self.essence("SPELL_LIGHT_ESSENCE = 65686,", use), [])

    def test_a_bare_literal_in_the_remap_argument_is_handled(self):
        findings = self.sweep({"X/Action.cpp": """
            bool Check() { return bot->HasAura(sSpellMgr->GetSpellIdForDifficulty(65686, bot)); }
            """})
        self.assertEqual(findings, [])

    def test_a_nested_comma_stays_inside_the_remap_argument(self):
        use = ("bot->HasAura(sSpellMgr->GetSpellIdForDifficulty("
               "std::max<uint32>(SPELL_OTHER, SPELL_LIGHT_ESSENCE), bot));")
        self.assertEqual(self.essence("SPELL_LIGHT_ESSENCE = 65686,", use), [])

    def test_a_constant_after_the_remap_call_is_not_handled(self):
        use = ("bot->HasAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_OTHER, bot)) && "
               "bot->HasAura(SPELL_LIGHT_ESSENCE);")
        self.assertEqual(len(self.essence("SPELL_LIGHT_ESSENCE = 65686,", use)), 1)

    def test_a_constant_declared_and_never_used_is_handled(self):
        self.assertEqual(self.essence("SPELL_LIGHT_ESSENCE = 65686,", "true;"), [])

    def test_a_name_that_is_not_a_spell_is_handled(self):
        self.assertEqual(self.essence("NPC_LIGHT_ESSENCE = 65686,",
                                      "bot->FindNearestCreature(NPC_LIGHT_ESSENCE, 50.0f);"), [])

    def test_a_brace_list_named_for_entries_is_handled(self):
        findings = self.sweep({"X/Data.h": """
            static const std::array<uint32, 2> addEntriesLady = {
                65686,
                12345,
            };
            bool IsAdd(uint32 entry)
            {
                return std::find(addEntriesLady.begin(), addEntriesLady.end(), entry);
            }
            """})
        self.assertEqual(findings, [])

    def test_a_brace_list_named_for_spells_is_reported(self):
        findings = self.sweep({"X/Data.h": """
            inline constexpr uint32 ESSENCE_AURAS[] = {
                65686,
            };
            bool Has() { for (uint32 id : ESSENCE_AURAS) if (bot->HasAura(id)) return true; }
            """})
        self.assertEqual([(f.rel, f.line) for f in findings], [("src/Ai/Raid/X/Data.h", 2)])

    def test_the_marker_on_the_same_line_is_handled(self):
        declaration = "SPELL_LIGHT_ESSENCE = 65686, // pblint: spell-difficulty-ok, 10N only"
        self.assertEqual(self.essence(declaration), [])

    def test_the_marker_on_the_line_above_is_handled(self):
        self.assertEqual(self.essence("// pblint: spell-difficulty-ok\n"
                                      "                SPELL_LIGHT_ESSENCE = 65686,"), [])

    def test_the_marker_two_lines_above_is_not_handled(self):
        findings = self.essence("// pblint: spell-difficulty-ok\n"
                                "                SPELL_OTHER = 1234,\n"
                                "                SPELL_LIGHT_ESSENCE = 65686,")
        self.assertEqual(len(findings), 1)

    def test_a_free_text_difficulty_comment_does_not_silence(self):
        findings = self.essence("// 25-man difficulty remaps this through spelldifficulty_dbc\n"
                                "                SPELL_LIGHT_ESSENCE = 65686,")
        self.assertEqual(len(findings), 1)

    def test_a_non_base_member_resolves_its_row(self):
        findings = self.sweep({"X/Data.h": """
            enum XSpells
            {
                SPELL_LIGHT_TOUCH = 67297,
            };
            bool Has() { return bot->HasAura(SPELL_LIGHT_TOUCH); }
            """})
        self.assertEqual(len(findings), 1)
        self.assertIn("65950, 67296, 67298", findings[0].message)

    def test_a_row_is_reported_once_at_its_first_member(self):
        findings = self.sweep({"X/Data.h": """
            enum XSpells
            {
                SPELL_LIGHT_TOUCH_10H = 67297,
                SPELL_LIGHT_TOUCH_10N = 65950,
            };
            bool Has()
            {
                return bot->HasAura(SPELL_LIGHT_TOUCH_10N) || bot->HasAura(SPELL_LIGHT_TOUCH_10H);
            }
            """})
        self.assertEqual([(f.line, "SPELL_LIGHT_TOUCH_10H (67297)" in f.message) for f in findings],
                         [(3, True)])
        self.assertIn("67296, 67298", findings[0].message)

    def test_a_fully_referenced_row_is_clean(self):
        findings = self.sweep({"X/Data.h": """
            bool Has()
            {
                return bot->HasAura(65686) || bot->HasAura(67222) || bot->HasAura(67223) ||
                       bot->HasAura(67224);
            }
            """})
        self.assertEqual(findings, [])

    def test_raid_directories_do_not_pool_references(self):
        findings = self.sweep({
            "X/Action.cpp": "bool A() { return bot->HasAura(65686) || bot->HasAura(67222); }\n",
            "Y/Action.cpp": "bool B() { return bot->HasAura(67223) || bot->HasAura(67224); }\n",
        })
        self.assertEqual(sorted(f.rel for f in findings),
                         ["src/Ai/Raid/X/Action.cpp", "src/Ai/Raid/Y/Action.cpp"])

    def test_a_float_coordinate_is_not_an_id(self):
        findings = self.sweep({"X/Data.h": "Position const P = { 65686.5f, 67297.0f, 12.0f };\n"})
        self.assertEqual(findings, [])


if __name__ == "__main__":
    unittest.main()
