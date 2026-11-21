"""Which RaidObs traces count as pulls, judged by raidobs itself so this agrees with batch.py --strict.

A pull counts when the trace has no warning at all (batch.py --strict), its build is newer than the
reference commit and its header's difficulty is the one under test. Baseline pulls are the newest such
traces built before the reference instead.
"""
from __future__ import annotations

import datetime
import pathlib
import sys
from dataclasses import dataclass, field

BOTOBS = pathlib.Path(__file__).resolve().parents[1] / "botobs"
if str(BOTOBS) not in sys.path:
    sys.path.insert(0, str(BOTOBS))

from raidobs.corpus import find_traces, pull_time  # noqa: E402
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

    @property
    def name(self) -> str:
        return self.path.name

    @property
    def duration_ms(self) -> int | None:
        if self.start_ms is None or self.end_ms is None:
            return None
        return self.end_ms - self.start_ms

    def valid(self, diff_id: int) -> bool:
        return self.built is not None and not self.warnings and self.diff == diff_id


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
    )


def _newest_first(facts: list[TraceFacts]) -> list[TraceFacts]:
    return sorted(facts, key=lambda f: f.start_ms or 0, reverse=True)


class Corpus:
    """One boss's traces, read once each. An open trace is read again next time, since its validity can
    still change."""

    def __init__(self, boss: str, roots: list[pathlib.Path] | None = None):
        self.boss = canonical_boss(boss)
        self.roots = roots or [LOG_ROOT]
        self._cache: dict[pathlib.Path, TraceFacts] = {}

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

    def after(self, when: datetime.datetime) -> list[TraceFacts]:
        """Traces pulled after `when`. A pull comes after its build, so a file stamped earlier can't hold
        a build newer than `when` and is never opened."""
        out = []
        for path in self.paths():
            pulled = pull_time(path)
            if pulled is not None and pulled < when:
                continue
            out.append(self.facts(path))
        return out

    def counted(self, diff: str, head_time: datetime.datetime) -> list[TraceFacts]:
        """Valid pulls at `diff` built after head_time, newest first."""
        diff_id = DIFF_IDS[diff]
        return _newest_first([f for f in self.after(head_time)
                              if f.valid(diff_id) and f.built > head_time])

    def baseline(self, diff: str, head_time: datetime.datetime, wanted: int) -> list[TraceFacts]:
        """The newest `wanted` valid pulls at `diff` built before head_time."""
        diff_id = DIFF_IDS[diff]
        found = []
        epoch = datetime.datetime.min.replace(tzinfo=datetime.timezone.utc)
        for path in sorted(self.paths(), key=lambda p: pull_time(p) or epoch, reverse=True):
            facts = self.facts(path)
            if facts.valid(diff_id) and facts.built < head_time:
                found.append(facts)
                if len(found) >= wanted:
                    break
        return _newest_first(found)

    def between(self, start_ms: int, end_ms: int) -> list[TraceFacts]:
        """Traces that opened inside a window, oldest first."""
        when = datetime.datetime.fromtimestamp(start_ms / 1000, datetime.timezone.utc)
        inside = [f for f in self.after(when - datetime.timedelta(minutes=1))
                  if f.start_ms is not None and start_ms <= f.start_ms <= end_ms]
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
