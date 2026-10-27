"""Tests for how the trace readers name and gate Icecrown Citadel.

    python -m unittest discover -s tools/botobs/tests

An ICC pull files under its contract slug, or under a DBC encounter or creature name when no table
named it, so every name has to reach its encounter, and the prefix tables have to agree with the
trigger names the C++ registers and with the key prefixes it writes.
"""
from __future__ import annotations

import json
import pathlib
import re
import sys
import tempfile
import unittest
from unittest import mock

BOTOBS = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BOTOBS))

from raidobs import coverage, probes, validity  # noqa: E402
from raidobs.encounter import (  # noqa: E402
    BOSS_ALIASES, ICC_KEY_PREFIXES, ICC_PREFIXES, MAP_SLUGS, TOC_PREFIXES, ULD_PREFIXES, boss_key,
    canonical_boss, encounter_of, node_encounter, prefix_matches_boss, slugify,
)
from raidobs.paths import RAID_ROOT  # noqa: E402
from raidobs.probes import LATCH, emitted_keys, silent_keys  # noqa: E402
from raidobs.trace import Trace  # noqa: E402

ICC_ROOT = RAID_ROOT / "ICC"
DEFINITIONS = ICC_ROOT / "Definition"

CLASS_OPEN = re.compile(r"^class (\w+)\b")
NAME_CONSTANT = re.compile(r'static constexpr char const\* Name = "([^"]+)"')
ROW_TRIGGER_CLASS = re.compile(r"\bNode<\s*(\w+)\s*,")
ROW_TRIGGER_NAME = re.compile(r'\.Node\(\s*"([^"]+)"')

ENCOUNTERS = tuple(ICC_KEY_PREFIXES)

# ICC triggers that belong to no encounter, so they lead with no ICC_PREFIXES key.
RAID_WIDE_TRIGGERS: frozenset[str] = frozenset({"icc opener swing", "icc opener redirect", "icc resistance aura"})

# The names a pull can file under when no boss-id table named it: DungeonEncounter.dbc's names on
# normal, the creature's on heroic, the captain's for the gunship and the creature MarkPull was given.
ENGAGE_NAMES = {
    "Blood Council": "blood-prince-council",
    "Prince Keleseth": "blood-prince-council",
    "Prince Taldaram": "blood-prince-council",
    "Prince Valanar": "blood-prince-council",
    "Queen Lana'thel": "blood-queen-lana-thel",
    "Blood-Queen Lana'thel": "blood-queen-lana-thel",
    "Muradin Bronzebeard": "icecrown-gunship-battle",
    "High Overlord Saurfang": "icecrown-gunship-battle",
    "Icecrown Gunship Battle": "icecrown-gunship-battle",
    "Stinky": "stinky-and-precious",
    "Precious": "stinky-and-precious",
    "Rotting Frost Giant": "rotting-frost-giant",
    "Lord Marrowgar": "lord-marrowgar",
    "Lady Deathwhisper": "lady-deathwhisper",
    "Deathbringer Saurfang": "deathbringer-saurfang",
    "Professor Putricide": "professor-putricide",
    "Valithria Dreamwalker": "valithria-dreamwalker",
    "The Lich King": "the-lich-king",
}

NODE_PER_PREFIX = {
    "icc lm": "lord-marrowgar",
    "icc lady deathwhisper": "lady-deathwhisper",
    "icc dark reckoning": "lady-deathwhisper",
    "icc rotting frost giant tank position": "rotting-frost-giant",
    "icc gunship cannon near": "icecrown-gunship-battle",
    "icc in cannon": "icecrown-gunship-battle",
    "icc dbs": "deathbringer-saurfang",
    "icc dogs": "stinky-and-precious",
    "icc festergut spore": "festergut",
    "icc rotface avoid vile gas": "rotface",
    "icc putricide gas cloud": "professor-putricide",
    "icc bpc kinetic bomb": "blood-prince-council",
    "icc bql vampiric bite": "blood-queen-lana-thel",
    "icc valkyre spear": "sister-svalna",
    "icc sister svalna": "sister-svalna",
    "icc valithria portal": "valithria-dreamwalker",
    "icc sindragosa frost beacon": "sindragosa",
    "icc lich king winter": "the-lich-king",
}


def registered_triggers() -> set[str]:
    """Every trigger name an ICC definition row registers: a `Node<Trigger, Action>` row by its trigger
    class's `Name`, a row built under explicit names by its first argument."""
    names: dict[str, str] = {}
    for path in ICC_ROOT.rglob("*.h"):
        current = None
        for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
            opened = CLASS_OPEN.match(line)
            if opened:
                current = opened.group(1)
            named = NAME_CONSTANT.search(line)
            if named and current:
                names[current] = named.group(1)

    keys: set[str] = set()
    for path in DEFINITIONS.glob("*.cpp"):
        text = path.read_text(encoding="utf-8", errors="replace")
        keys |= {names[cls] for cls in ROW_TRIGGER_CLASS.findall(text)}
        keys |= set(ROW_TRIGGER_NAME.findall(text))
    return keys


def other_raid_slugs() -> set[str]:
    """Every slug the readers know outside the ICC contract: Ulduar's and ToC's encounters, the other
    raids' aliases, and every map name, ICC's own included."""
    icc = set(ICC_KEY_PREFIXES)
    slugs = set(ULD_PREFIXES.values()) | set(TOC_PREFIXES.values()) | set(MAP_SLUGS.values())
    slugs |= {slug for slug, encounter in BOSS_ALIASES.items() if encounter not in icc}
    return slugs - icc


class Aliases(unittest.TestCase):
    def test_every_name_a_pull_can_file_under_reaches_its_encounter(self):
        for name, encounter in ENGAGE_NAMES.items():
            self.assertEqual(canonical_boss(slugify(name)), encounter, name)

    def test_every_contract_slug_is_its_own_name(self):
        for slug in ENCOUNTERS:
            self.assertEqual(canonical_boss(slug), slug)

    def test_the_lich_king_stays_unaliased(self):
        self.assertNotIn("the-lich-king", BOSS_ALIASES)

    def test_no_icc_alias_shadows_another_raids_encounter(self):
        others = set(ULD_PREFIXES.values()) | set(TOC_PREFIXES.values())
        for slug, encounter in BOSS_ALIASES.items():
            if encounter in ICC_KEY_PREFIXES:
                self.assertNotIn(slug, others, slug)
                self.assertNotIn(slug, MAP_SLUGS.values(), slug)

    def test_a_council_filed_trace_selects_as_the_encounter(self):
        self.assertEqual(boss_key(pathlib.Path("631_7_blood-council_1789800000.ndjson")),
                         "blood-prince-council")
        self.assertEqual(boss_key(pathlib.Path("631_7_prince-taldaram_1789800000.ndjson")),
                         "blood-prince-council")


class NodeOwners(unittest.TestCase):
    def test_each_prefix_owns_its_nodes(self):
        for node, encounter in NODE_PER_PREFIX.items():
            self.assertEqual(node_encounter(node), encounter, node)
        led = {prefix for prefix in ICC_PREFIXES if any(n.startswith(prefix) for n in NODE_PER_PREFIX)}
        self.assertEqual(led, set(ICC_PREFIXES))

    def test_every_prefix_leads_a_registered_trigger(self):
        keys = registered_triggers()
        for prefix in ICC_PREFIXES:
            self.assertTrue(any(key.startswith(prefix) for key in keys), prefix)

    def test_every_registered_trigger_has_an_owner(self):
        # An unowned node reads NEVER on every other encounter's pull instead of folding into the gate.
        keys = registered_triggers()
        self.assertTrue(keys, f"no definition rows in {DEFINITIONS}")
        for key in sorted(keys - RAID_WIDE_TRIGGERS):
            self.assertIn(node_encounter(key), ENCOUNTERS, key)

    def test_every_owner_is_a_contract_slug(self):
        for prefix, encounter in ICC_PREFIXES.items():
            self.assertIn(encounter, ICC_KEY_PREFIXES, prefix)

    def test_no_other_raids_table_claims_an_icc_trigger(self):
        for key in registered_triggers():
            for table in (ULD_PREFIXES, TOC_PREFIXES):
                self.assertFalse(any(key.startswith(prefix) for prefix in table), key)


class NoteKeyPrefixes(unittest.TestCase):
    def test_each_prefix_matches_exactly_its_contract_slugs(self):
        for prefix in set(ICC_KEY_PREFIXES.values()):
            for slug, own in ICC_KEY_PREFIXES.items():
                self.assertEqual(prefix_matches_boss(prefix, slug), prefix in (own, "icc"),
                                 f"{prefix} / {slug}")

    def test_icc_is_raid_wide(self):
        for slug in ENCOUNTERS:
            self.assertTrue(prefix_matches_boss("icc", slug), slug)
        self.assertFalse(prefix_matches_boss("icc", "icecrown-citadel"))

    def test_the_giant_has_its_own_prefix(self):
        self.assertEqual(ICC_KEY_PREFIXES["rotting-frost-giant"], "rfg")
        self.assertTrue(prefix_matches_boss("rfg", "rotting-frost-giant"))
        self.assertFalse(prefix_matches_boss("rfg", "festergut"))
        self.assertNotIn("icc", ICC_KEY_PREFIXES.values())

    def test_shared_prefixes_cover_the_gauntlet_and_the_trash(self):
        self.assertTrue(prefix_matches_boss("sg", "sindragosa-gauntlet"))
        self.assertTrue(prefix_matches_boss("bpc", "blood-prince-trash"))

    def test_no_icc_prefix_matches_another_raids_slug(self):
        others = other_raid_slugs()
        self.assertIn("anub-arak", others)
        for prefix in set(ICC_KEY_PREFIXES.values()):
            for slug in sorted(others):
                self.assertFalse(prefix_matches_boss(prefix, slug), f"{prefix} / {slug}")

    def test_lk_stays_off_toc(self):
        for encounter in TOC_PREFIXES.values():
            self.assertFalse(prefix_matches_boss("lk", encounter), encounter)
        self.assertFalse(prefix_matches_boss("toc", "the-lich-king"))
        self.assertFalse(prefix_matches_boss("lk", "trial-of-the-crusader"))

    def test_no_other_raids_prefix_matches_an_icc_slug(self):
        for prefix in ("toc", "nb", "jaraxxus", "fc", "tv", "anub", "yogg", "fl", "thorim", "mimiron"):
            for slug in ENCOUNTERS:
                self.assertFalse(prefix_matches_boss(prefix, slug), f"{prefix} / {slug}")


class GatedFold(unittest.TestCase):
    """A Festergut pull walks every ICC node, and the other encounters' nodes are gated, not silent."""

    NODES = (*NODE_PER_PREFIX, "medium aoe heal")

    DECLARED = {
        key: (LATCH, "fixture")
        for key in ("fg.phase", "rf.phase", "icc.state", "lk.phase", "rfg.plague", "toc.progress")
    }

    def trace(self, filed: str) -> Trace:
        records = [
            {"v": 14, "e": "hdr", "ts": 1789800000000, "map": 631, "inst": 2, "diff": 3, "boss": filed,
             "roster": [{"g": 1001, "n": "Tankbot", "c": 1, "r": "tank", "h": 0}]},
            {"t": 0, "e": "pull", "boss": filed, "src": "bossstate"},
            {"t": 60000, "e": "covdef",
             "d": [[index, node, "icc", "c"] for index, node in enumerate(self.NODES)]},
            {"t": 60000, "e": "cov", "g": 1001, "r": [[index, 50] for index in range(len(self.NODES))]},
        ]
        with tempfile.TemporaryDirectory() as folder:
            path = pathlib.Path(folder) / f"631_2_{filed}_1789800000.ndjson"
            path.write_text("".join(json.dumps(rec) + "\n" for rec in records), encoding="utf-8")
            return Trace(path)

    def test_walked_folds_the_other_encounters_nodes(self):
        rows, gated, boss = coverage.walked(self.trace("festergut"))
        self.assertEqual(boss, "festergut")
        self.assertEqual(sorted(entry["def"]["node"] for entry in rows),
                         ["icc festergut spore", "medium aoe heal"])
        self.assertEqual(gated, len(NODE_PER_PREFIX) - 1)

    def test_a_pull_with_no_triggers_of_its_own_folds_them_all(self):
        rows, gated, boss = coverage.walked(self.trace("blood-prince-trash"))
        self.assertEqual(boss, "blood-prince-trash")
        self.assertEqual([entry["def"]["node"] for entry in rows], ["medium aoe heal"])
        self.assertEqual(gated, len(NODE_PER_PREFIX))

    def test_every_filed_name_gates_down_to_its_own_encounter(self):
        for filed in ("lord-marrowgar", "blood-council", "queen-lana-thel", "muradin-bronzebeard", "stinky"):
            rows, gated, boss = coverage.walked(self.trace(filed))
            owners = {node_encounter(entry["def"]["node"]) for entry in rows}
            self.assertEqual(owners, {boss, None}, filed)
            others = sum(1 for encounter in NODE_PER_PREFIX.values() if encounter != boss)
            self.assertEqual(gated, others, filed)

    def test_an_icc_pull_whose_own_nodes_never_fired_has_no_strategy(self):
        self.assertEqual(validity.strategy_silent(self.trace("festergut")), (1, 50))
        self.assertIsNone(validity.strategy_silent(self.trace("blood-prince-trash")))

    def test_silent_keys_skip_the_other_encounters(self):
        trace = self.trace("festergut")
        with mock.patch.object(probes, "declared_keys", return_value=self.DECLARED):
            silent = silent_keys(emitted_keys(trace), encounter_of(trace))
        self.assertEqual([key for key, _, _ in silent], ["fg.phase", "icc.state"])


if __name__ == "__main__":
    unittest.main()
