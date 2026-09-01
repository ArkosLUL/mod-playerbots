"""Loading a RaidObs trace: the NDJSON reader, and the guid and spell lookups every view needs.

A trace is one record per line, `t` in milliseconds relative to the pull and negative during the
pre-roll. Schema and field meanings live in docs/systems/observability.md.
"""
from __future__ import annotations

import json
import pathlib
import sys

SUPPORTED_SCHEMA = 9

# Old traces stay readable: every addition through v6 is a new field or a new record, so an older file
# only loses the detail those carry. v7 gave an existing column a -1 sentinel, but what it replaces was
# nonsense in older files too, so one render serves both. v8 appends to the end of a snapshot row and
# adds an optional cast field, so a pre-v8 row is just a short one.
READABLE_SCHEMAS = (4, 5, 6, 7, 8, 9)


def clock(ms: int) -> str:
    sign = "-" if ms < 0 else ""
    ms = abs(int(ms))
    return f"{sign}{ms // 60000:d}:{(ms % 60000) / 1000:06.3f}"


class Trace:
    def __init__(self, path: pathlib.Path):
        self.path = path
        self.header: dict = {}
        self.records: list[dict] = []
        self.names: dict[int, str] = {}
        # creature_template entry per guid. Names repeat across unrelated creatures and change with
        # locale, so anything keying off "which creature is this" wants the entry instead.
        self.entries: dict[int, int] = {}
        self.spells: dict[int, str] = {}
        self.roles: dict[int, str] = {}
        self.humans: set[int] = set()
        self.bosses: set[int] = set()
        self.truncated = False
        self._load()

    def _load(self) -> None:
        bad = 0
        with self.path.open("r", encoding="utf-8", errors="replace") as fh:
            for line in fh:
                line = line.strip()
                if not line:
                    continue
                try:
                    rec = json.loads(line)
                except json.JSONDecodeError:
                    # A trace truncated by a crash or the size cap ends mid-line; everything before it
                    # is still good, so keep it rather than failing the whole read.
                    bad += 1
                    continue

                if rec.get("e") == "hdr":
                    self.header = rec
                    for member in rec.get("roster", []):
                        self.names[member["g"]] = member["n"]
                        self.roles[member["g"]] = member.get("r", "?")
                        if member.get("h"):
                            self.humans.add(member["g"])
                    continue

                if rec.get("e") == "unit":
                    self.names[rec["g"]] = rec.get("n", "?")
                    if rec.get("en"):
                        self.entries[rec["g"]] = rec["en"]
                    # Players carry a role; a creature does not. Somebody who zoned in after the
                    # header was written only ever appears here.
                    if "r" in rec:
                        self.roles[rec["g"]] = rec["r"]
                    if rec.get("h"):
                        self.humans.add(rec["g"])
                    if rec.get("b"):
                        self.bosses.add(rec["g"])
                    continue

                if rec.get("e") == "spell":
                    self.spells[rec["sp"]] = rec.get("n", "?")
                    continue

                if rec.get("e") == "truncated":
                    self.truncated = True
                    continue

                self.records.append(rec)

        # Verdicts are written when the engine pass that produced them ends, so their line order can
        # trail their own timestamps by a tick. Sorting once here keeps every view chronological.
        self.records.sort(key=lambda r: r.get("t", 0))

        if bad:
            print(f"note: skipped {bad} unparsable line(s) (trace likely cut short)", file=sys.stderr)

        version = self.header.get("v")
        if version is not None and version not in READABLE_SCHEMAS:
            print(
                f"warning: trace schema v{version}, this tool understands v{SUPPORTED_SCHEMA}",
                file=sys.stderr,
            )

    def name(self, guid) -> str:
        if not guid:
            return "-"
        if guid in self.names:
            return self.names[guid]
        # A guid is a type tag in the high half plus the counter. Printing the bare counter would make
        # a creature and a player that share one look like the same unit.
        return f"#{guid >> 32}:{guid & 0xFFFFFFFF}"

    def spell(self, spell_id) -> str:
        """A spell as a reader recognises it. v4 traces carry no names, so those stay bare numbers."""
        if not spell_id:
            return "melee"
        name = self.spells.get(spell_id)
        return f"{name} {spell_id}" if name else f"spell {spell_id}"

    def of(self, *events: str) -> list[dict]:
        return [r for r in self.records if r.get("e") in events]

    def guid_by_name(self, needle: str) -> int | None:
        needle = needle.lower()
        for guid, name in self.names.items():
            if name.lower() == needle:
                return guid
        for guid, name in self.names.items():
            if needle in name.lower():
                return guid
        return None
