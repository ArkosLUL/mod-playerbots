"""A test window: a list of jobs pulled on one build, each roster pool running one DC run at a time.

Job order is priority. Whenever a pool is free it takes the earliest job that still wants pulls on it,
and max_raids caps the DC runs in flight across both raids. A job wants pulls on a pool until the pool
has `pulls` valid pulls built after the job's head or has run `cap` pulls in this window. A refused
start, or a run that ends without a pull or without a record, fails that job alone; a build older than
a job's head fails every job whose head it predates. SOAP going away or a crash raises Stop, and every
run still going is stopped first.

The jobs file is a JSON list of objects:

    boss      RaidObs trace slug (required)
    scenario  DC scenario token (required)
    raid      uld or toc (required)
    pools     list of roster pools, default uld1-uld6 or toc1-toc3
    diff      10n, 10h, 25n or 25h, default 25n for uld and 25h for toc
    pulls     valid pulls wanted per pool, default 1
    cap       most pulls per pool, default 2
    head      commit-ish whose later builds count, default the window's --head
    label     free text such as polish, confirm or check-10
"""
from __future__ import annotations

import datetime
import json
import pathlib
from dataclasses import dataclass, field
from typing import Callable

import dc
import soap
from pools import DEFAULT_DIFF, Ledger, PullRow, pool_names, rows_for_run
from pulls import Corpus
from report import State
from server import Stop

FIELDS = ("boss", "scenario", "raid", "pools", "diff", "pulls", "cap", "head", "label")
POLL_S = 10.0
# SOAP silent this long while runs are going reads as a crash
DOWN_AFTER_S = 180.0
# DC writes the record as the run leaves status; past this the run is taken as lost
RECORD_WAIT_S = 120.0
STOP_WAIT_S = 300.0


@dataclass
class Job:
    number: int
    boss: str
    scenario: str
    raid: str
    pools: list[str]
    diff: str
    pulls: int
    cap: int
    head: str
    label: str = ""
    head_sha: str = ""
    head_time: datetime.datetime | None = None
    corpus: Corpus | None = None
    ledger: Ledger | None = None
    state: State | None = None
    pulled: dict[str, int] = field(default_factory=dict)
    failed: str | None = None
    reported: bool = False

    @property
    def name(self) -> str:
        return f"job {self.number} ({' '.join(part for part in (self.label, self.boss, self.diff) if part)})"

    def valid(self, pool: str) -> int:
        return len(self.ledger.counted(pool, self.head_time))

    def wants(self, pool: str) -> int:
        """How many pulls to ask DC for on `pool` now, 0 once the pool is done for this job."""
        if self.failed:
            return 0
        valid, pulled = self.valid(pool), self.pulled.get(pool, 0)
        if valid >= self.pulls or pulled >= self.cap:
            return 0
        return min(self.pulls - valid, self.cap - pulled)


def _text(entry: dict, key: str, where: str, default: str | None = None) -> str:
    value = entry.get(key, default)
    if not isinstance(value, str) or not value.strip():
        raise ValueError(f"{where}: {key} must be a non-empty string")
    return value.strip()


def _count(entry: dict, key: str, where: str, default: int) -> int:
    value = entry.get(key, default)
    # JSON true is an int to Python
    if isinstance(value, bool) or not isinstance(value, int) or value < 1:
        raise ValueError(f"{where}: {key} must be a whole number of at least 1")
    return value


def parse_jobs(data, default_head: str) -> list[Job]:
    if not isinstance(data, list) or not data:
        raise ValueError("a jobs file holds a non-empty JSON list of jobs")
    jobs = []
    for number, entry in enumerate(data, 1):
        where = f"job {number}"
        if not isinstance(entry, dict):
            raise ValueError(f"{where} is not a JSON object")
        unknown = sorted(set(entry) - set(FIELDS))
        if unknown:
            raise ValueError(f"{where}: unknown field {', '.join(unknown)}; a job takes {', '.join(FIELDS)}")
        boss = _text(entry, "boss", where)
        scenario = _text(entry, "scenario", where)
        if len(scenario.split()) != 1:
            raise ValueError(f"{where}: scenario must be one DC token")
        raid = entry.get("raid")
        if raid not in dc.RAIDS:
            raise ValueError(f"{where}: raid must be one of {', '.join(dc.RAIDS)}")
        pools = entry.get("pools", pool_names(raid))
        if not isinstance(pools, list) or not pools or \
                not all(isinstance(pool, str) and dc.valid_pool(pool) for pool in pools):
            raise ValueError(f"{where}: pools must be a non-empty list of pool names, each a lowercase letter "
                             "then up to 5 lowercase letters or digits")
        if len(set(pools)) != len(pools):
            raise ValueError(f"{where}: a pool is named twice")
        diff = entry.get("diff", DEFAULT_DIFF[raid])
        if diff not in dc.DIFFS:
            raise ValueError(f"{where}: diff must be one of {', '.join(dc.DIFFS)}")
        pulls = _count(entry, "pulls", where, 1)
        cap = _count(entry, "cap", where, 2)
        if cap < pulls:
            raise ValueError(f"{where}: cap {cap} can't be below pulls {pulls}")
        label = entry.get("label", "")
        if not isinstance(label, str):
            raise ValueError(f"{where}: label must be a string")
        jobs.append(Job(number, boss, scenario, raid, list(pools), diff, pulls, cap,
                        _text(entry, "head", where, default_head), label.strip()))
    return jobs


def load_jobs(path: str | pathlib.Path, default_head: str) -> list[Job]:
    try:
        # PowerShell 5.1 writes UTF-8 with a BOM, json.loads chokes on it
        data = json.loads(pathlib.Path(path).read_text(encoding="utf-8-sig"))
    except json.JSONDecodeError as exc:
        raise ValueError(f"{path} is not valid JSON: {exc}") from None
    return parse_jobs(data, default_head)


@dataclass
class Flight:
    run_id: str
    job: Job
    pool: str
    ended_at: float | None = None


class Window:
    """Runs the jobs to the end. Each job needs its corpus, ledger, state, head and pulled filled in."""

    def __init__(self, jobs: list[Job], max_raids: int, deps, read_records: Callable[[], list[dict]],
                 crashed: Callable[[], bool], settle_s: float, poll_s: float = POLL_S,
                 job_done: Callable[[Job], None] | None = None):
        self.jobs = jobs
        self.max_raids = max_raids
        self.deps = deps
        self.read_records = read_records
        self.crashed = crashed
        self.settle_s = settle_s
        self.poll_s = poll_s
        self.job_done = job_done
        self.flights: dict[str, Flight] = {}
        self.most_in_flight = 0
        self.notes: list[str] = []
        self._down_since: float | None = None

    def say(self, text: str) -> None:
        print(text, file=self.deps.log, flush=True)

    def going(self) -> list[Flight]:
        return [flight for flight in self.flights.values() if flight.ended_at is None]

    @staticmethod
    def names(flights: list[Flight]) -> str:
        return ", ".join(f"{flight.run_id} ({flight.job.name} on {flight.pool})" for flight in flights)

    def run(self) -> None:
        try:
            self.refresh(self.read_records())
            while True:
                self.start_free_pools()
                self.report_finished()
                if not self.flights:
                    return
                self.poll()
                if not self.collect():
                    self.deps.sleep(self.poll_s)
        except BaseException:
            self.stop_all()
            raise

    def refresh(self, records: list[dict]) -> None:
        for job in self.jobs:
            job.ledger.load(records)

    def finished(self, job: Job) -> bool:
        if any(flight.job is job for flight in self.flights.values()):
            return False
        return bool(job.failed) or not any(job.wants(pool) for pool in job.pools)

    def report_finished(self) -> None:
        for job in self.jobs:
            if not job.reported and self.finished(job):
                job.reported = True
                if self.job_done:
                    self.job_done(job)

    def plan(self) -> list[tuple[Job, str, int]]:
        """(job, pool, pulls) to start now: each free pool to the earliest job that wants it, jobs in
        order, until the raid cap is reached. A pool is free once its run has left status; the job that
        ran it waits until that run is read, the other jobs don't."""
        taken = {flight.pool for flight in self.going()}
        unread = {(id(flight.job), flight.pool) for flight in self.flights.values() if flight.ended_at is not None}
        room = self.max_raids - len(self.going())
        starts: list[tuple[Job, str, int]] = []
        for job in self.jobs:
            for pool in job.pools:
                if len(starts) >= room:
                    return starts
                if pool in taken or (id(job), pool) in unread:
                    continue
                count = job.wants(pool)
                if count:
                    taken.add(pool)
                    starts.append((job, pool, count))
        return starts

    def start_free_pools(self) -> None:
        # a refused start fails its job and frees the pool for the next job, so plan again
        while starts := self.plan():
            for job, pool, count in starts:
                if not job.failed:
                    self.start(job, pool, count)

    def start(self, job: Job, pool: str, count: int) -> None:
        command = dc.start_command(job.scenario, pool, job.diff, count)
        try:
            run_id = dc.start_run(self.deps.call, command, self.deps.clock, self.deps.sleep,
                                  (soap.SoapUnavailable,))
        except dc.DriverOffline as exc:
            raise Stop(f"the DC test driver never got online: {exc}") from None
        except dc.StartRefused as exc:
            self.fail(job, f"DC refused `{command}`: {exc}")
            return
        self.flights[run_id] = Flight(run_id, job, pool)
        self.most_in_flight = max(self.most_in_flight, len(self.going()))
        self.say(f"{job.name}: {run_id} started on {pool} for up to {count} pull(s)")

    def fail(self, job: Job, reason: str) -> None:
        if job.failed is None:
            job.failed = reason
            self.say(f"{job.name} failed, its other pools stop taking it: {reason}")

    def poll(self) -> None:
        """Mark every run that left .dc test status as ended."""
        going = self.going()
        if not going:
            return
        try:
            status = self.deps.call(dc.STATUS_COMMAND)
        except soap.SoapUnavailable as exc:
            now = self.deps.clock()
            if self._down_since is None:
                self._down_since = now
            if now - self._down_since >= DOWN_AFTER_S:
                raise Stop(f"the worldserver stopped answering during {', '.join(f.run_id for f in going)}: "
                           f"{exc}; it may have crashed") from None
            return
        self._down_since = None
        now = self.deps.clock()
        for flight in going:
            if not dc.run_active(status, flight.run_id):
                flight.ended_at = now

    def collect(self) -> bool:
        """Read in every ended run whose traces have had time to close. True when any was read."""
        now = self.deps.clock()
        due = [flight for flight in self.flights.values()
               if flight.ended_at is not None and now - flight.ended_at >= self.settle_s]
        if not due:
            return False
        records = self.read_records()
        runs = {flight.run_id: dc.find_run(records, flight.run_id) for flight in due}
        found = [(flight, runs[flight.run_id]) for flight in due if runs[flight.run_id]]
        lost = [flight for flight in due if not runs[flight.run_id] and now - flight.ended_at >= RECORD_WAIT_S]
        if not found and not lost:
            return False
        if lost and self.crashed():
            raise Stop(f"{', '.join(flight.run_id for flight in lost)} left .dc test status without a record "
                       "and the worldserver restarted meanwhile: it crashed")

        self.refresh(records)
        for flight, run in found:
            del self.flights[flight.run_id]
            self.judge(flight, run, self.take(flight, run))
        for flight in lost:
            del self.flights[flight.run_id]
            self.fail(flight.job, f"{flight.run_id} on {flight.pool} left .dc test status without a record "
                                  "in DC's run log")
        return True

    def take(self, flight: Flight, run: dc.Run) -> list[PullRow]:
        job = flight.job
        run.pool = run.pool or flight.pool
        rows = rows_for_run("test", run, job.corpus)
        job.state.runs.append(("test", run))
        job.state.rows += rows
        job.pulled[flight.pool] = job.pulled.get(flight.pool, 0) + len(rows)
        self.say(f"{job.name}: {run.run_id} on {flight.pool} ended {run.result or '?'} with {len(rows)} pull(s)")
        return rows

    def judge(self, flight: Flight, run: dc.Run, rows: list[PullRow]) -> None:
        job = flight.job
        if not rows:
            reason = f": {run.fail_reason}" if run.fail_reason else ""
            self.fail(job, f"{run.run_id} on {flight.pool} ended {run.result or 'without a verdict'} "
                           f"without a pull{reason}")
            return
        built = [row.trace.built for row in rows if row.trace and row.trace.built]
        if not any(when < job.head_time for when in built):
            return
        # any job whose head is newer than this build would just burn a round per pool finding the same
        oldest = min(built)
        for other in self.jobs:
            if other.head_time > oldest and other.failed is None:
                self.fail(other, f"the running build predates {other.head_sha}, so none of its pulls can count")
                for going in self.going():
                    if going.job is other:
                        self.stop(going)

    def stop(self, flight: Flight) -> None:
        try:
            self.deps.call(dc.stop_command(flight.run_id))
            self.say(f"{flight.job.name}: stopped {flight.run_id} on {flight.pool}")
        except Exception as exc:
            self.say(f"{flight.job.name}: could not stop {flight.run_id} on {flight.pool}: {exc}")

    def stop_all(self) -> None:
        """Stop every run still going and wait for them to leave status, so the lock is released on a quiet
        server; then read in whatever ended. Best effort, since SOAP may be what failed."""
        try:
            going = self.going()
            for flight in going:
                self.stop(flight)
            deadline = self.deps.clock() + STOP_WAIT_S
            unread = ""
            while going:
                try:
                    status = self.deps.call(dc.STATUS_COMMAND)
                except Exception as exc:
                    unread = f" (status unreadable: {exc})"
                    break
                for flight in going:
                    if not dc.run_active(status, flight.run_id):
                        flight.ended_at = self.deps.clock()
                going = self.going()
                if not going or self.deps.clock() >= deadline:
                    break
                self.deps.sleep(self.poll_s)
            if going:
                self.notes.append(f"Not seen to leave .dc test status when the window gave up the server{unread}: "
                                  + self.names(going) + ".")
            records = self.read_records()
            for corpus in {id(flight.job.corpus): flight.job.corpus for flight in self.flights.values()}.values():
                corpus.rescan()
            for flight in list(self.flights.values()):
                run = dc.find_run(records, flight.run_id) if flight.ended_at is not None else None
                if run is not None:
                    del self.flights[flight.run_id]
                    self.take(flight, run)
            unrecorded = [flight for flight in self.flights.values() if flight.ended_at is not None]
            if unrecorded:
                self.notes.append("Ended with no record in DC's run log when the window gave up the server: "
                                  + self.names(unrecorded) + ".")
        except Exception as exc:
            self.say(f"while stopping the window: {type(exc).__name__}: {exc}")
