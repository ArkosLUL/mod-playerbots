"""Which roster pool and DC run each trace belongs to, and the valid pulls each pool has on either side
of HEAD.

A trace belongs to a run when it was recorded in the run's instance while the run was going. The pools'
runs of one boss overlap in time but never share an instance, so the instance id keeps them apart; the
time window keeps apart two runs that got the same instance id at different times.

A pull whose boss engaged before DC's start step never counts, whatever its trace says: DC drops the
pre-pull refresh then, so the raid goes in on the last pull's mana, cooldowns and Exhaustion. Records
from before DC's schema 18 don't say how a pull started, so those count as before.
"""
from __future__ import annotations

import datetime
import re
from dataclasses import dataclass

import dc
from pulls import DIFF_IDS, Corpus, TraceFacts, match_traces

POOLS_PER_RAID = {"uld": 6, "toc": 3}
# Ulduar's hard modes come from the server's toggles, so its default is 25n
DEFAULT_DIFF = {"uld": "25n", "toc": "25h"}
SLACK_MS = 60_000
COMPOSITIONS = "ABC"


def pool_names(raid: str, count: int | None = None) -> list[str]:
    return [f"{raid}{number}" for number in range(1, (count or POOLS_PER_RAID[raid]) + 1)]


def composition(pool: str) -> str:
    match = re.fullmatch(r"[a-z]+(\d+)", pool)
    if not match or int(match.group(1)) < 1:
        return ""
    return COMPOSITIONS[(int(match.group(1)) - 1) % len(COMPOSITIONS)]


@dataclass
class PullRow:
    phase: str
    pool: str
    run_id: str
    number: int
    window: tuple[int, int] | None
    dc_outcome: str
    trace: TraceFacts | None
    engaged_first: bool = False

    def valid(self, diff_id: int) -> bool:
        return self.trace is not None and self.trace.valid(diff_id) and not self.engaged_first


def rows_for_run(phase: str, run: dc.Run, corpus: Corpus) -> list[PullRow]:
    """One row per pull DC logged, each with the trace it produced. A record without a pull list gets
    a row per trace of its instance inside the run instead."""
    traces = []
    if run.instance_id and run.started_ms and run.ended_ms:
        traces = corpus.in_instance(run.map_id, run.instance_id, run.started_ms - SLACK_MS,
                                    run.ended_ms + SLACK_MS)
    if run.pulls is not None:
        # DC writes endMs 0 for a pull it never closed
        windows = [(pull.start_ms, pull.end_ms or run.ended_ms or pull.start_ms) for pull in run.pulls]
        matched = match_traces(windows, traces, SLACK_MS)
        return [PullRow(phase, run.pool, run.run_id, index, window, pull.outcome, trace, pull.engaged_first)
                for index, (pull, window, trace) in enumerate(zip(run.pulls, windows, matched), 1)]
    return [PullRow(phase, run.pool, run.run_id, index, None, "", trace) for index, trace in enumerate(traces, 1)]


def _newest_first(facts: list[TraceFacts]) -> list[TraceFacts]:
    return sorted(facts, key=lambda f: f.start_ms or 0, reverse=True)


class Ledger:
    """Every pull the pools' DC runs produced, so each pool is counted and compared only with itself."""

    def __init__(self, corpus: Corpus, pools: list[str], diff: str):
        self.corpus = corpus
        self.pools = pools
        self.diff_id = DIFF_IDS[diff]
        self.rows: list[PullRow] = []

    def load(self, records: list[dict]) -> None:
        self.corpus.rescan()
        self.rows = [row for run in dc.pool_runs(records, self.pools)
                     for row in rows_for_run("", run, self.corpus)]
        self.corpus.flush()

    def history(self) -> list[tuple[str, TraceFacts]]:
        pairs = [(pool, facts) for pool in self.pools for facts in self._valid(pool)]
        return sorted(pairs, key=lambda pair: pair[1].start_ms or 0, reverse=True)

    def _valid(self, pool: str) -> list[TraceFacts]:
        seen, out = set(), []
        for row in self.rows:
            facts = row.trace
            if row.pool != pool or facts is None or facts.path in seen:
                continue
            seen.add(facts.path)
            if row.valid(self.diff_id):
                out.append(facts)
        return _newest_first(out)

    def counted(self, pool: str, head_time: datetime.datetime) -> list[TraceFacts]:
        """The pool's valid pulls built after head_time, newest first."""
        return [facts for facts in self._valid(pool) if facts.built > head_time]

    def baseline(self, pool: str, head_time: datetime.datetime, wanted: int) -> list[TraceFacts]:
        """The pool's newest `wanted` valid pulls built before head_time."""
        return [facts for facts in self._valid(pool) if facts.built < head_time][:wanted]

    def rows_of(self, traces: list[TraceFacts]) -> list[PullRow]:
        paths = [facts.path for facts in traces]
        rows = {row.trace.path: row for row in self.rows if row.trace and row.trace.path in paths}
        return [rows[path] for path in paths if path in rows]
