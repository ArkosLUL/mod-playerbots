"""Which RaidObs traces count as pulls, judged by raidobs itself so this agrees with batch.py --strict.

A pull counts when the trace has no warning at all (batch.py --strict), its build is newer than the
reference commit and its header's difficulty is the one under test. Baseline pulls are the newest such
traces built before the reference instead. Which pool a trace belongs to is pools.py's business.
"""
from __future__ import annotations

import datetime
import pathlib
import sys
from dataclasses import dataclass, field

BOTOBS = pathlib.Path(__file__).resolve().parents[1] / "botobs"
if str(BOTOBS) not in sys.path:
    sys.path.insert(0, str(BOTOBS))

from raidobs.corpus import find_traces  # noqa: E402
from raidobs.encounter import canonical_boss  # noqa: E402
from raidobs.paths import LOG_ROOT  # noqa: E402
from raidobs.trace import Trace, clock, combat_deaths, death_records  # noqa: E402
from raidobs.validity import inspect  # noqa: E402

# RaidObs `hdr.diff`, the core's raid difficulty ids
DIFF_IDS = {"10n": 0, "25n": 1, "10h": 2, "25h": 3}
DC_MULTIPLIERS = ("dungeon clear", "dungeon clear combat")


@dataclass
class Veto:
    at_ms: int
    bot: str
    multiplier: str
    action: str


@dataclass
class TraceFacts:
    path: pathlib.Path
    encounter: str
    built: datetime.datetime | None
    diff: int | None
    warnings: list[tuple[str, str]]
    hardmode: bool | None
    start_ms: int | None
    end_ms: int | None
    outcome: str
    closed: bool
    deaths: list[str] = field(default_factory=list)
    wipe_commands: int = 0
    vetoes: list[Veto] = field(default_factory=list)
    map_id: int | None = None
    instance: int | None = None

    @property
    def name(self) -> str:
        return self.path.name

    @property
    def duration_ms(self) -> int | None:
        if self.start_ms is None or self.end_ms is None:
            return None
        return self.end_ms - self.start_ms

    @property
    def dc_interfered(self) -> bool:
        """A DC multiplier vetoed an action once the encounter was on, so DC ran part of the fight."""
        return self.start_ms is not None and any(veto.at_ms >= self.start_ms for veto in self.vetoes)

    def valid(self, diff_id: int) -> bool:
        return self.built is not None and not self.warnings and self.diff == diff_id and not self.dc_interfered


def death_cause(trace: Trace, death: dict) -> str:
    """The same reading deathreport gives a death, cut down to one line."""
    guid = death.get("g")
    cause = death.get("cause")
    if cause == "reset":
        return "the wipe command"
    if cause == "self":
        return "a script kill, no blow recorded"
    blow = death.get("blow") or []
    if blow and blow[0] == guid:
        return "its own blow: a fall, lava or .die"
    line = f"{trace.name(death.get('killer'))}"
    rewind = sorted(death.get("rewind") or [], key=lambda row: row[0])
    if rewind:
        _, source, spell, amount = rewind[-1][:4]
        line += f", last hit {trace.spell(spell)} for {amount} from {trace.name(source)}"
    return line


def read_facts(path: pathlib.Path) -> TraceFacts:
    trace = Trace(path)
    facts, warnings = inspect(trace, None)
    header = trace.header
    start = header.get("ts")
    ends = trace.of("end")
    last_t = ends[-1]["t"] if ends else (trace.records[-1].get("t", 0) if trace.records else 0)
    deaths = [
        f"{trace.name(d['g'])} ({trace.role(d['g'])}) at {clock(d['t'])}: {death_cause(trace, d)}"
        for d in combat_deaths(trace)
    ]
    vetoes = [
        Veto(int(start) + int(rec.get("t", 0)), trace.name(rec.get("g")), rec.get("m", ""), rec.get("a", ""))
        for rec in trace.of("veto")
        if rec.get("m") in DC_MULTIPLIERS and start is not None
    ]
    return TraceFacts(
        path=path,
        encounter=facts["encounter"],
        built=facts["built"],
        diff=header.get("diff"),
        warnings=warnings,
        hardmode=facts["hardmode"],
        start_ms=int(start) if start is not None else None,
        end_ms=int(start) + int(last_t) if start is not None else None,
        outcome=ends[-1].get("out", "?") if ends else "cut short",
        closed=bool(ends),
        deaths=deaths,
        wipe_commands=len(death_records(trace)) - len(combat_deaths(trace)),
        vetoes=vetoes,
        map_id=_int_or_none(header.get("map")),
        instance=_int_or_none(header.get("inst")),
    )


def _int_or_none(value) -> int | None:
    return int(value) if isinstance(value, (int, float)) else None


def name_parts(path: pathlib.Path) -> tuple[int, int, int] | None:
    """(map, instance id, epoch seconds) out of <map>_<instanceId>_<boss-slug>_<epoch>.ndjson."""
    parts = path.stem.split("_")
    if len(parts) < 4 or not (parts[0].isdigit() and parts[1].isdigit() and parts[-1].isdigit()):
        return None
    return int(parts[0]), int(parts[1]), int(parts[-1])


class Corpus:
    """One boss's traces, read once each. An open trace is read again next time, since its validity can
    still change."""

    def __init__(self, boss: str, roots: list[pathlib.Path] | None = None):
        self.boss = canonical_boss(boss)
        self.roots = roots or [LOG_ROOT]
        self._cache: dict[pathlib.Path, TraceFacts] = {}
        self._by_instance: dict[int, list[tuple[int, int, pathlib.Path]]] | None = None

    def facts(self, path: pathlib.Path) -> TraceFacts:
        cached = self._cache.get(path)
        if cached:
            return cached
        facts = read_facts(path)
        if facts.closed:
            self._cache[path] = facts
        return facts

    def paths(self) -> list[pathlib.Path]:
        return find_traces(self.roots, self.boss)

    def rescan(self) -> None:
        """List the files again, so traces written since the last scan are found."""
        index: dict[int, list[tuple[int, int, pathlib.Path]]] = {}
        for path in self.paths():
            parts = name_parts(path)
            if parts:
                map_id, instance, epoch = parts
                index.setdefault(instance, []).append((map_id, epoch, path))
        self._by_instance = index

    def in_instance(self, map_id: int, instance: int, start_ms: int, end_ms: int) -> list[TraceFacts]:
        """Traces recorded in one instance that opened inside a window, oldest first. The file name says
        where and when, so no other file is opened."""
        if self._by_instance is None:
            self.rescan()
        inside = []
        for file_map, epoch, path in self._by_instance.get(instance, []):
            # the name's epoch is the header's ts cut to whole seconds
            if (map_id and file_map != map_id) or not start_ms - 1000 < epoch * 1000 <= end_ms:
                continue
            facts = self.facts(path)
            if facts.instance not in (None, instance) or facts.start_ms is None:
                continue
            if start_ms <= facts.start_ms <= end_ms:
                inside.append(facts)
        return sorted(inside, key=lambda f: f.start_ms)


def overlap(a: tuple[int, int], b: tuple[int, int]) -> int:
    return min(a[1], b[1]) - max(a[0], b[0])


def match_traces(windows: list[tuple[int, int]], facts: list[TraceFacts], slack_ms: int = 60000) \
        -> list[TraceFacts | None]:
    """The trace each pull window produced: the one overlapping it most, each trace used once. The slack
    covers the gap between DC starting a pull and the recorder opening on it."""
    used: set[pathlib.Path] = set()
    out: list[TraceFacts | None] = []
    for start, end in windows:
        best, best_overlap = None, 0
        for candidate in facts:
            if candidate.path in used or candidate.start_ms is None or candidate.end_ms is None:
                continue
            span = (candidate.start_ms, candidate.end_ms)
            if overlap((start - slack_ms, end + slack_ms), span) <= 0:
                continue
            score = max(overlap((start, end), span), 1)
            if score > best_overlap:
                best, best_overlap = candidate, score
        if best is not None:
            used.add(best.path)
        out.append(best)
    return out


def vetoes_inside(facts: TraceFacts, start_ms: int, end_ms: int) -> list[Veto]:
    return [veto for veto in facts.vetoes if start_ms <= veto.at_ms <= end_ms]
