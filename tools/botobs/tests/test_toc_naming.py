"""Tests for how the trace readers name and gate Trial of the Crusader.

    python -m unittest discover -s tools/botobs/tests

A ToC pull is filed under whichever creature swung first, so every beast, champion and twin has to
reach its encounter, and the prefix tables have to agree with the trigger names the C++ registers.
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

from raidobs import coverage, probes  # noqa: E402
from raidobs.encounter import (  # noqa: E402
    BOSS_ALIASES, TOC_PREFIXES, boss_key, canonical_boss, encounter_of, node_encounter,
    prefix_matches_boss, slugify,
)
from raidobs.paths import RAID_ROOT  # noqa: E402
from raidobs.probes import LATCH, emitted_keys, silent_keys  # noqa: E402
from raidobs.trace import Trace  # noqa: E402

TOC_SRC = RAID_ROOT / "ToC"
CHAMPIONS_HEADER = TOC_SRC / "Util" / "ToCHelpers_FactionChampions.h"

ENCOUNTERS = ("northrend-beasts", "lord-jaraxxus", "faction-champions", "val-kyr-twins", "anub-arak")

ENGAGE_SLUGS = {
    "gormok-the-impaler": "northrend-beasts",
    "acidmaw": "northrend-beasts",
    "dreadscale": "northrend-beasts",
    "icehowl": "northrend-beasts",
    "fire-bomb": "northrend-beasts",
    "fjola-lightbane": "val-kyr-twins",
    "eydis-darkbane": "val-kyr-twins",
}

CHAMPIONS = (
    "vivienne-blackwhisper", "thrakgar", "liandra-suncaller", "caiphus-the-stern", "ruj-kah",
    "ginselle-blightslinger", "harkzog", "birana-stormhoof", "narrhok-steelbreaker", "maz-dinah",
    "broln-stouthorn", "malithas-brightblade", "gorgrim-shadowcleave", "erin-misthoof",
    "kavina-grovesong", "tyrius-duskblade", "shaabad", "velanaa", "anthar-forgemender",
    "alyssia-moonstalker", "noozle-whizzlestick", "melador-valestrider", "saamul",
    "baelnor-lightbearer", "irieth-shadowstep", "brienna-nightfell", "serissa-grimdabbler", "shocuul",
)

# ToC triggers that belong to no encounter, so they lead with no TOC_PREFIXES key.
RAID_WIDE_TRIGGERS: frozenset[str] = frozenset({"toc restore rti cc"})

NODE_PER_PREFIX = {
    "gormok engaged by main tank": "northrend-beasts",
    "northrend worms sweep frontal": "northrend-beasts",
    "icehowl charge incoming": "northrend-beasts",
    "jaraxxus legion flame nearby": "lord-jaraxxus",
    "faction champions should focus": "faction-champions",
    "twin valkyr pact interruptible": "val-kyr-twins",
    "anubarak scarab on raid": "anub-arak",
}


def registered_triggers() -> set[str]:
    keys = set()
    for path in (TOC_SRC / "Trigger").rglob("*"):
        if path.suffix in (".h", ".cpp"):
            text = path.read_text(encoding="utf-8", errors="replace")
            keys.update(re.findall(r'creators\[\s*"([^"]+)"\s*\]', text))
    return keys


class Aliases(unittest.TestCase):
    def test_every_engage_slug_reaches_its_encounter(self):
        for slug, encounter in ENGAGE_SLUGS.items():
            self.assertEqual(canonical_boss(slug), encounter, slug)
        for slug in CHAMPIONS:
            self.assertEqual(canonical_boss(slug), "faction-champions", slug)

    def test_an_encounter_slug_is_its_own_name(self):
        for slug in ENCOUNTERS:
            self.assertEqual(canonical_boss(slug), slug)

    def test_the_lich_king_stays_unmapped(self):
        # ToC's Lich King is boss-flagged too, but the slug belongs to ICC.
        self.assertEqual(canonical_boss("the-lich-king"), "the-lich-king")

    def test_a_champion_filed_trace_selects_as_the_encounter(self):
        self.assertEqual(boss_key(pathlib.Path("649_1_ruj-kah_1789500000.ndjson")),
                         "faction-champions")

    def test_one_alias_per_champion_in_the_enum(self):
        text = CHAMPIONS_HEADER.read_text(encoding="utf-8")
        body = re.search(r"enum class ToCFactionChampions\b[^{]*\{(.*?)\};", text, re.S)
        self.assertIsNotNone(body, f"no ToCFactionChampions enum in {CHAMPIONS_HEADER}")
        entries = re.findall(r"^\s*\w+\s*=\s*\d+", body.group(1), re.M)
        aliased = [slug for slug, encounter in BOSS_ALIASES.items() if encounter == "faction-champions"]
        self.assertEqual(len(aliased), len(entries))


class Slugs(unittest.TestCase):
    def test_encounter_names_slug_as_the_recorder_files_them(self):
        self.assertEqual(slugify("Anub'arak"), "anub-arak")
        self.assertEqual(slugify("Lord Jaraxxus"), "lord-jaraxxus")
        self.assertEqual(slugify("Val'kyr Twins"), "val-kyr-twins")


class NodeOwners(unittest.TestCase):
    def test_each_prefix_owns_its_nodes(self):
        for node, encounter in NODE_PER_PREFIX.items():
            self.assertEqual(node_encounter(node), encounter, node)
        led = {prefix for prefix in TOC_PREFIXES if any(n.startswith(prefix) for n in NODE_PER_PREFIX)}
        self.assertEqual(led, set(TOC_PREFIXES))

    def test_namesakes_in_other_instances_stay_unowned(self):
        self.assertIsNone(node_encounter("anub'arak impale"))
        self.assertIsNone(node_encounter("anub'rekhan locust swarm"))

    def test_every_prefix_leads_a_registered_trigger(self):
        keys = registered_triggers()
        for prefix in TOC_PREFIXES:
            self.assertTrue(any(key.startswith(prefix) for key in keys), prefix)

    def test_every_registered_trigger_has_an_owner(self):
        # An unowned node reads NEVER on every other encounter's pull instead of folding into the gate.
        keys = registered_triggers()
        self.assertTrue(keys, f"no creators[...] keys under {TOC_SRC / 'Trigger'}")
        for key in sorted(keys - RAID_WIDE_TRIGGERS):
            self.assertIn(node_encounter(key), ENCOUNTERS, key)


class NoteKeyPrefixes(unittest.TestCase):
    OWN = {
        "nb": "northrend-beasts", "jaraxxus": "lord-jaraxxus", "fc": "faction-champions",
        "tv": "val-kyr-twins", "anub": "anub-arak",
    }

    def test_toc_is_raid_wide(self):
        for encounter in ENCOUNTERS:
            self.assertTrue(prefix_matches_boss("toc", encounter), encounter)

    def test_naxx_anub_rekhan_is_not_toc(self):
        self.assertFalse(prefix_matches_boss("toc", "anub-rekhan"))
        self.assertFalse(prefix_matches_boss("anub", "anub-rekhan"))

    def test_prefixes_no_slug_derives_match_their_own(self):
        self.assertTrue(prefix_matches_boss("jaraxxus", "lord-jaraxxus"))
        self.assertTrue(prefix_matches_boss("tv", "val-kyr-twins"))

    def test_each_encounter_prefix_matches_only_its_own(self):
        for prefix, own in self.OWN.items():
            for encounter in ENCOUNTERS:
                self.assertEqual(prefix_matches_boss(prefix, encounter), encounter == own,
                                 f"{prefix} / {encounter}")

    def test_the_ulduar_forms_still_derive(self):
        self.assertTrue(prefix_matches_boss("yogg", "yogg-saron"))
        self.assertTrue(prefix_matches_boss("fl", "flame-leviathan"))
        self.assertTrue(prefix_matches_boss("ironassembly", "iron-assembly"))
        self.assertTrue(prefix_matches_boss("xt002", "xt-002"))
        self.assertFalse(prefix_matches_boss("thorim", "mimiron"))
        self.assertFalse(prefix_matches_boss("thorim", "yogg-saron"))
        self.assertFalse(prefix_matches_boss("fl", "freya"))


class GatedFold(unittest.TestCase):
    """A Beasts pull walks every ToC node, and the other four encounters' nodes are gated, not silent."""

    NODES = (*NODE_PER_PREFIX, "medium aoe heal")

    DECLARED = {
        key: (LATCH, "fixture")
        for key in ("nb.snobold", "jaraxxus.flame", "fc.focus", "tv.essence", "anub.spike",
                    "toc.progress", "yogg.phase")
    }

    def trace(self, filed: str) -> Trace:
        records = [
            {"v": 13, "e": "hdr", "ts": 1789500000000, "map": 649, "inst": 1, "diff": 1, "boss": filed,
             "roster": [{"g": 1001, "n": "Tankbot", "c": 1, "r": "tank", "h": 0}]},
            {"t": 0, "e": "pull", "boss": filed, "src": "engage"},
            {"t": 60000, "e": "covdef",
             "d": [[index, node, "trialofthecrusader", "c"] for index, node in enumerate(self.NODES)]},
            {"t": 60000, "e": "cov", "g": 1001, "r": [[index, 50] for index in range(len(self.NODES))]},
        ]
        with tempfile.TemporaryDirectory() as folder:
            path = pathlib.Path(folder) / f"649_1_{filed}_1789500000.ndjson"
            path.write_text("".join(json.dumps(rec) + "\n" for rec in records), encoding="utf-8")
            return Trace(path)

    def test_walked_folds_the_other_encounters_nodes(self):
        rows, gated, boss = coverage.walked(self.trace("gormok-the-impaler"))
        self.assertEqual(boss, "northrend-beasts")
        self.assertEqual(sorted(entry["def"]["node"] for entry in rows),
                         ["gormok engaged by main tank", "icehowl charge incoming", "medium aoe heal",
                          "northrend worms sweep frontal"])
        self.assertEqual(gated, 4)

    def test_every_filed_name_gates_down_to_its_own_encounter(self):
        for filed in ("gormok-the-impaler", "lord-jaraxxus", "ruj-kah", "fjola-lightbane", "anub-arak"):
            rows, gated, boss = coverage.walked(self.trace(filed))
            owners = {node_encounter(entry["def"]["node"]) for entry in rows}
            self.assertEqual(owners, {boss, None}, filed)
            others = sum(1 for encounter in NODE_PER_PREFIX.values() if encounter != boss)
            self.assertEqual(gated, others, filed)

    def test_silent_keys_skip_the_other_encounters(self):
        trace = self.trace("gormok-the-impaler")
        with mock.patch.object(probes, "declared_keys", return_value=self.DECLARED):
            silent = silent_keys(emitted_keys(trace), encounter_of(trace))
        self.assertEqual([key for key, _, _ in silent], ["nb.snobold", "toc.progress"])


if __name__ == "__main__":
    unittest.main()
