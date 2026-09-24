"""Tests for pblint reading encounter definition rows.

    python -m unittest discover -s tools/botobs/tests

Each case writes a small src tree to a temp dir and runs one check over it.
"""
from __future__ import annotations

import importlib.util
import pathlib
import tempfile
import textwrap
import unittest
from unittest import mock

REPO = pathlib.Path(__file__).resolve().parents[3]
_spec = importlib.util.spec_from_file_location("pblint", REPO / "tools" / "pblint" / "pblint.py")
pblint = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(pblint)

CLASSES = """
    class XDodgeTrigger : public Trigger
    {
    public:
        static constexpr char const* Name = "x dodge";
        XDodgeTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    };

    class XDodgeAction : public MovementAction
    {
    public:
        static constexpr char const* Name = "x dodge action";
        XDodgeAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}
    };
"""

CONTEXT = """
    class XActionContext : public NamedObjectContext<Action>
    {
    public:
        XActionContext() { creators["heroism"] = &heroism; }
    };
"""


def definition(body: str) -> str:
    return f"""
        void DefineX(EncounterBuilder& e)
        {{
            {body}
        }}
    """


class RowTest(unittest.TestCase):
    def run_check(self, check, files: dict[str, str]) -> list:
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp).resolve()
            for rel, text in files.items():
                path = root / "src" / rel
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(textwrap.dedent(text).lstrip("\n"), encoding="utf-8")
            with mock.patch.object(pblint, "REPO", root):
                sources = pblint.collect([root / "src"])
            return check(sources, pblint.registry(sources))

    def tree(self, body: str, **extra: str) -> dict[str, str]:
        files = {"Raid/X/XClasses.h": CLASSES, "Raid/X/XContext.h": CONTEXT,
                 "Raid/X/XDefinition.cpp": definition(body)}
        files.update({name.replace("__", "/") + ".h": text for name, text in extra.items()})
        return files

    def test_a_template_row_registers_both_names(self):
        files = self.tree("e.Node<XDodgeTrigger, XDodgeAction>(ACTION_RAID, EncounterRow::Mover);",
                          Raid__X__XOther="""
                              void Init() { triggers.push_back(new TriggerNode("x dodge",
                                  { NextAction("x dodge action", 1.0f) })); }
                          """)
        self.assertEqual(self.run_check(pblint.check_unresolved, files), [])
        self.assertEqual(self.run_check(pblint.check_ctor_name, files), [])

    def test_a_class_no_row_names_is_reported(self):
        findings = self.run_check(pblint.check_ctor_name, self.tree(""))
        self.assertEqual(sorted(f.message.split(" names")[0] for f in findings),
                         ["XDodgeAction", "XDodgeTrigger"])

    def test_an_explicit_row_reads_its_names_past_the_lambdas(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp).resolve()
            path = root / "src" / "Raid" / "X" / "XDefinition.cpp"
            path.parent.mkdir(parents=True)
            path.write_text(textwrap.dedent(definition("""
                e.Node(
                    "x resistance",
                    [](PlayerbotAI* ai) -> Trigger* { return new ResistTrigger(ai, "boss, x"); },
                    "x resistance action",
                    [](PlayerbotAI* ai) -> Action* { return new ResistAction(ai, "boss"); },
                    ACTION_RAID + 1);
            """)), encoding="utf-8")
            with mock.patch.object(pblint, "REPO", root):
                rows = pblint.registry(pblint.collect([root / "src"])).rows
        self.assertEqual([(r.trigger, r.action) for r in rows], [("x resistance", "x resistance action")])

    def test_an_inherited_name_is_the_base_name(self):
        files = self.tree("e.Node<XDodgeTrigger, XLateDodgeAction>(ACTION_RAID);", Raid__X__XLate="""
            class XLateDodgeAction : public XDodgeAction
            {
            };
        """)
        rows = self.run_check(lambda sources, reg: reg.rows, files)
        self.assertEqual([(r.trigger, r.action) for r in rows], [("x dodge", "x dodge action")])

    def test_a_rule_naming_an_unknown_action_is_an_error(self):
        findings = self.run_check(pblint.check_unresolved, self.tree("""
            e.Node<XDodgeTrigger, XDodgeAction>(ACTION_RAID, EncounterRow::Mover);
            e.Block("x hold", Role::Any, XActive, 0, {"heroism", "bloodlst", "x dodge action"});
        """))
        self.assertEqual([f.check for f in findings], ["unresolved-rule-action"])
        self.assertIn('"bloodlst"', findings[0].message)

    def test_the_rule_name_itself_is_not_an_action(self):
        findings = self.run_check(pblint.check_unresolved, self.tree("""
            e.OwnMovement("x control movement multiplier", Role::Ranged, XActive, Family::Attack,
                          {"x dodge action"});
            e.Node<XDodgeTrigger, XDodgeAction>(ACTION_RAID, EncounterRow::Mover);
        """))
        self.assertEqual(findings, [])

    def test_a_creator_left_behind_for_a_row_is_an_error(self):
        files = self.tree("e.Node<XDodgeTrigger, XDodgeAction>(ACTION_RAID);", Raid__X__XTriggers="""
            class XTriggerContext : public NamedObjectContext<Trigger>
            {
            public:
                XTriggerContext() { creators["x dodge"] = &dodge; }
            };
        """)
        findings = self.run_check(pblint.check_definition_rows, files)
        self.assertEqual([(f.check, f.rel) for f in findings], [("row-duplicate", "src/Raid/X/XTriggers.h")])

    def test_a_row_name_is_never_an_orphan(self):
        findings = self.run_check(pblint.check_orphan_creators,
                                  self.tree("e.Node<XDodgeTrigger, XDodgeAction>(ACTION_RAID);"))
        self.assertEqual([f.message for f in findings], ['creators["heroism"] is never named by any node'])

    def test_a_name_constant_interval_in_seconds_is_reported(self):
        findings = self.run_check(lambda sources, reg: pblint.check_trigger_interval(sources), {
            "Ai/Raid/X/XSoak.h": """
                class XSoakTrigger : public Trigger
                {
                public:
                    static constexpr char const* Name = "x soak";
                    XSoakTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name, 2) {}
                };
            """})
        self.assertEqual([f.check for f in findings], ["trigger-interval"])


if __name__ == "__main__":
    unittest.main()
