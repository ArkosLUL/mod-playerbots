#!/usr/bin/env python3
"""Unattended raid test cycle on mod-dungeon-clear across a raid's roster pools, with no human logged in.

    raidtest.py cycle --boss xt-002 --scenario <dc scenario> --raid uld [--pools uld1,uld2,uld3]
                      [--diff 25n] [--pulls 1] [--cap 2] [--skip-baseline] [--skip-rebuild]
    raidtest.py window --jobs <file.json> [--head HEAD] [--max-raids 6] [--skip-rebuild] [--report-dir DIR]
    raidtest.py report --boss xt-002 --raid uld [--pools ...] [--diff 25n] [--pulls 1] [--runs tr-...,tr-...]
                       [--full] [--out FILE]

A cycle takes the server lock, gets baseline pulls on the live build for every pool with fewer than
--pulls valid ones from before HEAD, rebuilds and restarts the worldserver, then pulls the boss until
every pool has --pulls valid pulls built after HEAD or ran --cap pulls. Each round starts one DC run per
pool still short, all at once, and waits for all of them. A pool's pulls are only counted for, and
compared with, that pool. --pools defaults to <raid>1-<raid>6 and --diff to 25h for toc and 25n
for uld, whose hard modes come from the server's toggles. The markdown report goes to stdout, progress
to stderr. Reports are digests (pulls, the done rule per composition, deaths by mechanic, the trend over
recent builds, each boss reader's --summary); --full prints every pool's sections and readers in full.

A window runs a list of jobs (window.py has the file format) on one build under one lock: the same
dirty-tree check, rebuild and restart once at the start, no baseline phase, then every pool pulls the
earliest job that still wants it, at most --max-raids runs at once. With --report-dir each job's report
is written to <dir>/<dir name>-<boss>.md as soon as the job is finished, and stdout carries only the
window's header, the report files and the load line.

Exit codes: 0 every pool got enough valid pulls (in a window, every job's), 1 a cap ran out first, 2
refused before touching the server, 3 stopped on a failure (build, restart, driver, crash, a refused
start) or, in a window, a job failed.
"""
from __future__ import annotations

import argparse
import datetime
import io
import os
import pathlib
import subprocess
import sys
import tempfile
import time
import traceback
from dataclasses import dataclass
from typing import Callable, TextIO

import dc
import server
import soap
import window
from pools import DEFAULT_DIFF, Ledger, PullRow, pool_names, rows_for_run
from pulls import DIFF_IDS, Corpus
from raidobs.paths import REPO
from raidobs.validity import resolve_since
from report import JobLine, State, WindowState, render, render_window_header, render_window_load
from server import Stop

CORE = REPO.parents[1]
MODULE_REPO = CORE / "modules" / "mod-playerbots"
# The host side of DC_TESTRUNS_FILE, which the server env points at the bind-mounted logs volume: DC's
# default, its working directory, sits in the container layer and is wiped on every recreate.
DC_RUNS_FILE = CORE / "env" / "dist" / "logs" / "dc" / "dc_testruns.jsonl"
# A trace closes up to Obs.IdleCloseSeconds (30 s) after its pull, so wait that out before reading
SETTLE_S = 45.0
REPORT_WAIT_S = 900.0


@dataclass
class Deps:
    call: Callable[[str], str]
    run: Callable[..., subprocess.CompletedProcess] = subprocess.run
    clock: Callable[[], float] = time.monotonic
    now: Callable[[], float] = time.time
    sleep: Callable[[float], None] = time.sleep
    out: TextIO = sys.stdout
    log: TextIO = sys.stderr
    spawn: Callable[..., subprocess.Popen] = subprocess.Popen


def say(deps: Deps, text: str) -> None:
    print(text, file=deps.log, flush=True)


def read_records(args) -> list[dict]:
    return dc.parse_records(dc.read_log(args.dc_runs_file))


def dc_round(phase: str, wants: dict[str, int], args, deps: Deps, corpus: Corpus, state: State) \
        -> dict[str, list[PullRow]]:
    """One DC run per pool in `wants` (pool: pulls), all started before any is waited on. Returns the
    rows of each pool's run."""
    unavailable = (soap.SoapUnavailable,)
    started: dict[str, str] = {}
    try:
        for pool, pulls in wants.items():
            command = dc.start_command(args.scenario, pool, args.diff, pulls)
            try:
                run_id = dc.start_run(deps.call, command, deps.clock, deps.sleep, unavailable)
            except dc.DriverOffline as exc:
                raise Stop(f"the DC test driver never got online: {exc}") from None
            except dc.StartRefused as exc:
                raise Stop(f"DC refused `{command}`: {exc}") from None
            started[run_id] = pool
            say(deps, f"{phase}: {run_id} started on {pool} for up to {pulls} pull(s)")
        runs = dc.wait_runs(deps.call, list(started), lambda: dc.read_log(args.dc_runs_file), deps.clock,
                            deps.sleep, unavailable)
    except BaseException as exc:
        # don't leave runs going on a server the cycle is about to unlock, also when only SOAP went quiet
        for run_id in started:
            try:
                deps.call(dc.stop_command(run_id))
            except Exception:
                pass
        if isinstance(exc, dc.RunVanished):
            raise Stop(f"{exc}; the worldserver may have crashed") from None
        raise

    deps.sleep(SETTLE_S)
    corpus.rescan()
    rows: dict[str, list[PullRow]] = {}
    for run_id, pool in started.items():
        run = runs.get(run_id)
        if run is None:
            continue
        run.pool = run.pool or pool
        say(deps, f"{phase}: {run_id} on {pool} ended {run.result or '?'}")
        state.runs.append((phase, run))
        rows[pool] = rows_for_run(phase, run, corpus)
        state.rows += rows[pool]

    missing = [run_id for run_id in started if run_id not in runs]
    if missing:
        raise Stop(f"{', '.join(missing)} left .dc test status without a record in {args.dc_runs_file}; "
                   "the worldserver may have crashed")
    for run_id, pool in started.items():
        if not rows[pool]:
            run = runs[run_id]
            reason = f": {run.fail_reason}" if run.fail_reason else ""
            raise Stop(f"{run_id} on {pool} ended {run.result or 'without a verdict'} without a pull{reason}")
    return rows


def run_phase(phase: str, args, deps: Deps, corpus: Corpus, ledger: Ledger, state: State) -> None:
    """Rounds of DC runs until every pool has --pulls valid pulls for this phase or ran --cap pulls."""
    pulled = dict.fromkeys(args.pools, 0)
    capped: set[str] = set()
    while True:
        ledger.load(read_records(args))
        wants: dict[str, int] = {}
        for pool in args.pools:
            if phase == "test":
                valid = len(ledger.counted(pool, state.head_time))
            else:
                valid = len(ledger.baseline(pool, state.head_time, args.pulls))
            say(deps, f"{phase} {pool}: {valid}/{args.pulls} valid, {pulled[pool]}/{args.cap} pulled")
            if valid >= args.pulls:
                continue
            if pulled[pool] >= args.cap:
                if pool not in capped:
                    capped.add(pool)
                    state.notes.append(f"{pool}: the {phase} cap of {args.cap} pulls ran out with {valid} valid.")
                continue
            wants[pool] = min(args.pulls - valid, args.cap - pulled[pool])
        if not wants:
            return

        rows = dc_round(phase, wants, args, deps, corpus, state)
        for pool, pool_rows in rows.items():
            pulled[pool] += len(pool_rows)
        built = [row.trace.built for pool_rows in rows.values() for row in pool_rows
                 if row.trace and row.trace.built]
        if phase == "baseline" and any(when > state.head_time for when in built):
            state.notes.append("The live build already holds HEAD, so it can't give a baseline.")
            return
        if phase == "test" and any(when < state.head_time for when in built):
            raise Stop(f"the running build predates {state.head_sha}, so none of its pulls can count")


def rebuild(args, deps: Deps, notes: list[str]) -> None:
    problems = server.dirty_trees(args.core, deps.run)
    if problems:
        raise Stop("refused to build, uncommitted files would ship: " + "; ".join(problems))

    stamp = datetime.datetime.fromtimestamp(deps.now()).strftime("%Y%m%d-%H%M%S")
    log_path = pathlib.Path(args.log_dir) / f"worldserver-build-{stamp}.log"
    notes.append(f"Build log: `{log_path}`")
    say(deps, f"building, log in {log_path}")
    code = server.build(args.core, log_path, deps.run)
    if code != 0:
        raise Stop(f"the worldserver build failed (docker compose exit {code}), see {log_path}")

    code, since, recreated = server.restart(args.core, deps.run, deps.now)
    if code != 0:
        raise Stop(f"docker compose up failed (exit {code})")
    if not recreated:
        notes.append("The build gave the running image again, so the worldserver kept running.")
        return
    say(deps, "waiting for the worldserver")
    if not server.wait_ready(since, args.ready_timeout, deps.run, deps.clock, deps.sleep):
        raise Stop(f"no `{server.READY_LINE}` in the worldserver log after {args.ready_timeout:.0f} s")


def finish(state: State, corpus: Corpus, ledger: Ledger, records: list[dict]) -> None:
    for row in state.rows:
        if row.trace:
            try:
                row.trace = corpus.facts(row.trace.path)
            except OSError:
                pass
    ledger.load(records)
    state.counted = {pool: ledger.counted(pool, state.head_time) for pool in state.pools}
    state.baseline = {pool: ledger.baseline(pool, state.head_time, state.wanted) for pool in state.pools}
    state.history = ledger.history()


def resolve_head(repo: pathlib.Path, head: str) -> tuple[str, datetime.datetime] | None:
    """(name, time) of a head. resolve_since names a timestamp "given", but the report hands the name to
    batch.py --since and --split-at, which read the timestamp itself."""
    resolved = resolve_since(repo, head)
    if not resolved:
        return None
    sha, when = resolved
    return (head if sha == "given" else sha), when


def head_of(args) -> tuple[str, datetime.datetime] | None:
    return resolve_head(pathlib.Path(args.repo), args.head)


def stop_reason(exc: BaseException, deps: Deps) -> str:
    """The report's reason for a failure that ended a cycle or window; call it inside the except."""
    if isinstance(exc, Stop):
        return exc.reason
    if isinstance(exc, soap.SoapError):
        return f"SOAP: {exc}"
    if isinstance(exc, KeyboardInterrupt):
        return "interrupted"
    traceback.print_exc(file=deps.log)
    return f"{type(exc).__name__}: {exc}"


def cycle(args, deps: Deps) -> int:
    head = head_of(args)
    if not head:
        print(f"cannot resolve {args.head} in {args.repo}", file=deps.log)
        return 2
    corpus = Corpus(args.boss)
    ledger = Ledger(corpus, args.pools, args.diff)
    state = State(boss=corpus.boss, diff=args.diff, head_sha=head[0], head_time=head[1], wanted=args.pulls,
                  pools=list(args.pools), cap=args.cap)

    owner = f"raidtest cycle {corpus.boss} {args.diff} on {','.join(args.pools)}" + \
        (f": {args.owner}" if args.owner else "")
    lock = server.ServerLock(pathlib.Path(args.lock), server.owner_text(owner))
    try:
        lock.acquire()
    except server.LockHeld as exc:
        print(f"refused: {exc}", file=deps.log)
        return 2

    try:
        if not args.skip_rebuild:
            problems = server.dirty_trees(args.core, deps.run)
            if problems:
                print(f"# Raid test: {corpus.boss} {args.diff}\n", file=deps.out)
                print("Refused: these would ship in the build.\n", file=deps.out)
                for problem in problems:
                    print(f"- {problem}", file=deps.out)
                return 2
        try:
            if not args.skip_baseline:
                run_phase("baseline", args, deps, corpus, ledger, state)
            if not args.skip_rebuild:
                rebuild(args, deps, state.notes)
            run_phase("test", args, deps, corpus, ledger, state)
        except (Exception, KeyboardInterrupt) as exc:
            state.stopped = stop_reason(exc, deps)
        try:
            records = read_records(args)
        except Exception as exc:
            records = []
            state.notes.append(f"Could not read DC's run log `{args.dc_runs_file}`: {exc}")
        finish(state, corpus, ledger, records)
        render(state, deps.run, deps.out)
    finally:
        lock.release()

    if state.stopped:
        return 3
    return 0 if all(len(state.counted[pool]) >= args.pulls for pool in state.pools) else 1


def write_whole(target: pathlib.Path, text: str, sleep: Callable[[float], None] = time.sleep) -> None:
    spare = target.with_name(f"{target.name}.{os.getpid()}.tmp")
    spare.write_text(text, encoding="utf-8")
    for attempt in range(10):
        try:
            os.replace(spare, target)
            return
        except PermissionError:
            if attempt == 9:
                spare.unlink(missing_ok=True)
                raise
            sleep(0.5)


def rerun_command(args) -> str:
    command = (f"python tools/raidtest/raidtest.py report --boss {args.boss} --raid {args.raid} "
               f"--pools {','.join(args.pools)} --diff {args.diff} --pulls {args.pulls} --head {args.head}")
    return command + (f" --runs {','.join(args.runs)}" if args.runs else "")


def report_only(args, deps: Deps) -> int:
    if not args.out:
        return build_report(args, deps, deps.out)
    text = io.StringIO()
    try:
        code = build_report(args, deps, text)
    except Exception as exc:
        traceback.print_exc(file=deps.log)
        text, code = io.StringIO(), 3
        print(f"The report failed: {type(exc).__name__}: {exc}", file=text)
    if code:
        failed = text.getvalue().strip() or f"The report could not be built (exit {code}), see the window log."
        title = f"# Raid test: {args.boss} {args.diff}" + (f" ({args.label})" if args.label else "")
        text = io.StringIO()
        print(f"{title}\n\n**{failed}**\n\nRerun: `{rerun_command(args)}`", file=text)
    write_whole(pathlib.Path(args.out), text.getvalue(), deps.sleep)
    return code


def build_report(args, deps: Deps, out: TextIO) -> int:
    head = head_of(args)
    if not head:
        print(f"cannot resolve {args.head} in {args.repo}", file=deps.log)
        return 2
    try:
        records = read_records(args)
    except OSError as exc:
        print(f"cannot read {args.dc_runs_file}: {exc}", file=deps.log)
        return 2
    corpus = Corpus(args.boss)
    state = State(boss=corpus.boss, diff=args.diff, head_sha=head[0], head_time=head[1], wanted=args.pulls,
                  pools=list(args.pools), label=args.label, notes=list(args.note))
    if args.runs:
        for run_id in args.runs:
            run = dc.find_run(records, run_id)
            if run is None:
                state.notes.append(f"No DC record for {run_id}.")
                continue
            if not run.pool:
                state.notes.append(f"{run_id} has no pool: DC fielded it without roster=.")
                continue
            if run.pool not in state.pools:
                state.pools.append(run.pool)
            state.runs.append(("run", run))
            state.rows += rows_for_run("run", run, corpus)
    ledger = Ledger(corpus, state.pools, args.diff)
    if not args.runs:
        ledger.load(records)
        for pool in state.pools:
            for row in ledger.rows_of(ledger.counted(pool, state.head_time)[:args.pulls]):
                row.phase = "test"
                state.rows.append(row)
    finish(state, corpus, ledger, records)
    render(state, deps.run, out, full=args.full)
    return 0


class JobReports:
    def __init__(self, folder: pathlib.Path, args, deps: Deps):
        self.folder = folder
        self.args = args
        self.deps = deps
        self.spawned: list[tuple[window.Job, pathlib.Path, subprocess.Popen | None]] = []

    def path_for(self, job: window.Job, jobs: list[window.Job]) -> pathlib.Path:
        shared = sum(1 for other in jobs if other.boss == job.boss) > 1
        suffix = f"-{job.diff}-job{job.number}" if shared else ""
        return self.folder / f"{self.folder.name}-{job.boss}{suffix}.md"

    @staticmethod
    def notes(job: window.Job, stopped: str | None) -> list[str]:
        notes = list(job.state.notes)
        for pool in job.pools:
            valid = job.valid(pool)
            line = f"{pool}: the cap of {job.cap} pulls ran out with {valid} valid."
            if valid < job.pulls and job.pulled.get(pool, 0) >= job.cap and line not in notes:
                notes.append(line)
        if job.failed:
            notes.append(f"The job failed: {job.failed}")
        elif stopped:
            notes.append(f"The window stopped: {stopped}")
        elif job.cut:
            notes.append(job.cut)
        return notes

    def command(self, job: window.Job, target: pathlib.Path, stopped: str | None = None) -> list[str]:
        command = [sys.executable, str(pathlib.Path(__file__).resolve()), "report", "--boss", job.boss,
                   "--raid", job.raid, "--pools", ",".join(job.pools), "--diff", job.diff,
                   "--pulls", str(job.pulls), "--head", job.head_sha, "--repo", str(self.args.repo),
                   "--dc-runs-file", str(self.args.dc_runs_file), "--label", job.state.label,
                   "--out", str(target)]
        runs = [run.run_id for _, run in job.state.runs if run.run_id]
        if runs:
            command += ["--runs", ",".join(runs)]
        for note in self.notes(job, stopped):
            command += ["--note", note]
        return command

    def start(self, job: window.Job, jobs: list[window.Job], stopped: str | None = None) -> None:
        target = self.path_for(job, jobs)
        env = {**os.environ, "PYTHONIOENCODING": "utf-8", "PYTHONUTF8": "1"}
        try:
            process = self.deps.spawn(self.command(job, target, stopped), env=env)
        except OSError as exc:
            say(self.deps, f"{job.name} done; its report could not start: {exc}")
            process = None
        else:
            say(self.deps, f"{job.name} done; report goes to {target}")
        self.spawned.append((job, target, process))

    def wait(self) -> list[str]:
        lines = []
        deadline = self.deps.clock() + REPORT_WAIT_S
        for job, target, process in self.spawned:
            if process is None:
                lines.append(f"- {job.name}: the report did not start")
                continue
            try:
                code = process.wait(timeout=max(1.0, deadline - self.deps.clock()))
            except subprocess.TimeoutExpired:
                lines.append(f"- {job.name}: `{target}` still being written after {REPORT_WAIT_S:.0f} s")
                continue
            lines.append(f"- {job.name}: `{target}`" + (f" (report exit {code})" if code else ""))
        return lines


def job_verdict(job: window.Job, stopped: str | None) -> str:
    if job.failed:
        return f"failed: {job.failed}"
    if all(len(job.state.counted.get(pool, [])) >= job.pulls for pool in job.pools):
        return "done"
    if stopped:
        return "stopped"
    return "cut short" if job.cut else "cap ran out"


def run_window(args, deps: Deps) -> int:
    try:
        jobs = window.load_jobs(args.jobs, args.head)
    except (OSError, ValueError) as exc:
        print(f"refused: {exc}", file=deps.log)
        return 2
    repo = pathlib.Path(args.repo)
    heads: dict[str, tuple[str, datetime.datetime] | None] = {}
    for job in jobs:
        if job.head not in heads:
            heads[job.head] = resolve_head(repo, job.head)
        if not heads[job.head]:
            print(f"cannot resolve {job.head} ({job.name}) in {args.repo}", file=deps.log)
            return 2
        job.head_sha, job.head_time = heads[job.head]
    build = heads.get("HEAD") or resolve_head(repo, "HEAD")

    corpora: dict[str, Corpus] = {}
    for job in jobs:
        if job.boss not in corpora:
            corpora[job.boss] = Corpus(job.boss)
        job.corpus = corpora[job.boss]
        job.ledger = Ledger(job.corpus, job.pools, job.diff)
        job.pulled = dict.fromkeys(job.pools, 0)
        job.state = State(boss=job.corpus.boss, diff=job.diff, head_sha=job.head_sha, head_time=job.head_time,
                          wanted=job.pulls, pools=list(job.pools), cap=job.cap,
                          label=f"job {job.number}" + (f", {job.label}" if job.label else ""))
    summary = WindowState(build_sha=build[0] if build else "?", build_time=build[1] if build else None,
                          build_note="not rebuilt (--skip-rebuild), the jobs ran on the build already up",
                          max_raids=args.max_raids)

    reports = None
    if args.report_dir:
        folder = pathlib.Path(args.report_dir)
        try:
            folder.mkdir(parents=True, exist_ok=True)
            old = sorted(folder.glob("*.md"))
        except OSError as exc:
            print(f"refused: can't use --report-dir {folder}: {exc}", file=deps.log)
            return 2
        if old:
            print(f"refused: {folder} already holds reports ({old[0].name}); give the window a new label",
                  file=deps.log)
            return 2
        reports = JobReports(folder, args, deps)
    stop_file = pathlib.Path(args.stop_file) if args.stop_file else None
    if stop_file and stop_file.exists():
        print(f"refused: the stop file {stop_file} exists already; delete it or name a new one", file=deps.log)
        return 2

    owner = "raidtest window: " + "; ".join(job.name for job in jobs) + (f": {args.owner}" if args.owner else "")
    lock = server.ServerLock(pathlib.Path(args.lock), server.owner_text(owner))
    try:
        lock.acquire()
    except server.LockHeld as exc:
        print(f"refused: {exc}", file=deps.log)
        return 2

    runner = None
    try:
        if not args.skip_rebuild:
            problems = server.dirty_trees(args.core, deps.run)
            if problems:
                print("# Raid test window\n", file=deps.out)
                print("Refused: these would ship in the build.\n", file=deps.out)
                for problem in problems:
                    print(f"- {problem}", file=deps.out)
                return 2
        try:
            if not args.skip_rebuild:
                summary.build_note = "the rebuild did not finish"
                rebuild(args, deps, summary.notes)
                summary.build_note = "rebuilt for this window"
            container = server.container_id(deps.run)
            job_done = (lambda job: reports.start(job, jobs)) if reports else None
            runner = window.Window(jobs, args.max_raids, deps, lambda: read_records(args),
                                   lambda: not container or server.container_id(deps.run) != container, SETTLE_S,
                                   job_done=job_done, stop_file=stop_file)
            runner.run()
        except (Exception, KeyboardInterrupt) as exc:
            summary.stopped = stop_reason(exc, deps)
        if runner:
            summary.notes += runner.notes
            summary.most_in_flight = runner.most_in_flight
        try:
            records = read_records(args)
        except Exception as exc:
            records = []
            summary.notes.append(f"Could not read DC's run log `{args.dc_runs_file}`: {exc}")

        traces = {}
        for job in jobs:
            finish(job.state, job.corpus, job.ledger, records)
            for pool in job.pools:
                valid = len(job.state.counted.get(pool, []))
                if valid < job.pulls and job.pulled[pool] >= job.cap:
                    job.state.notes.append(f"{pool}: the cap of {job.cap} pulls ran out with {valid} valid.")
            verdict = job_verdict(job, summary.stopped)
            job.state.stopped = job.failed or (summary.stopped if verdict == "stopped" else None)
            summary.jobs.append(JobLine(job.number, job.label, job.state.boss, job.diff, job.head_sha, verdict,
                                        [(pool, len(job.state.counted.get(pool, [])), job.pulled[pool])
                                         for pool in job.pools]))
            traces.update((row.trace.path, row.trace) for row in job.state.rows if row.trace)
        summary.traces = list(traces.values())
        render_window_header(summary, deps.out)
        if reports:
            for job in jobs:
                if not job.reported:
                    job.reported = True
                    reports.start(job, jobs, job.state.stopped)
        else:
            for job in jobs:
                print(file=deps.out)
                render(job.state, deps.run, deps.out)
    finally:
        lock.release()

    if reports:
        print("\n# Job reports\n", file=deps.out)
        print("\n".join(reports.wait()), file=deps.out)
    render_window_load(summary, deps.out)
    if summary.stopped or any(job.failed for job in jobs):
        return 3
    return 0 if all(line.verdict == "done" for line in summary.jobs) else 1


def env_setting(key: str, default: str) -> str:
    try:
        return soap.read_env().get(key) or default
    except (soap.SoapError, OSError, UnicodeError):
        return default


def pool_list(text: str) -> list[str]:
    pools = [part.strip() for part in text.split(",") if part.strip()]
    bad = [pool for pool in pools if not dc.valid_pool(pool)]
    if not pools or bad:
        raise argparse.ArgumentTypeError(
            f"pool names are a lowercase letter then up to 5 lowercase letters or digits: {', '.join(bad) or text!r}")
    if len(set(pools)) != len(pools):
        raise argparse.ArgumentTypeError(f"a pool is named twice: {text}")
    return pools


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Unattended raid test cycle on mod-dungeon-clear.")
    commands = parser.add_subparsers(dest="command", required=True)

    def sources(sub: argparse.ArgumentParser, head_help: str) -> None:
        sub.add_argument("--head", default="HEAD", help=head_help)
        sub.add_argument("--repo", default=str(MODULE_REPO), help="repo --head resolves in")
        sub.add_argument("--dc-runs-file", default=env_setting("DC_RUNS_FILE", str(DC_RUNS_FILE)),
                         help="DC's run log on the host (.env DC_RUNS_FILE)")

    def common(sub: argparse.ArgumentParser) -> None:
        sub.add_argument("--boss", required=True, help="RaidObs trace slug, e.g. xt-002 or northrend-beasts")
        sub.add_argument("--raid", required=True, choices=dc.RAIDS, help="the raid whose roster pools to use")
        sub.add_argument("--pools", type=pool_list, help="roster pools (default <raid>1-<raid>6)")
        sub.add_argument("--diff", choices=sorted(DIFF_IDS), help="default 25h for toc, 25n for uld")
        sub.add_argument("--pulls", type=int, default=1, help="valid pulls wanted per pool (default 1)")
        sources(sub, "commit-ish the build must be newer than")

    def live(sub: argparse.ArgumentParser) -> None:
        sub.add_argument("--skip-rebuild", action="store_true", help="test the build that is running")
        sub.add_argument("--core", default=str(CORE), help="core checkout the build compiles")
        sub.add_argument("--lock", default=str(server.LOCK_DIR))
        sub.add_argument("--owner", default="", help="who runs this, for the lock's owner file")
        sub.add_argument("--log-dir", default=str(pathlib.Path(tempfile.gettempdir()) / "raidtest"))
        sub.add_argument("--ready-timeout", type=float, default=1800.0, help="seconds to wait for the restart")

    run = commands.add_parser("cycle", help="baseline, rebuild, pulls, report")
    common(run)
    run.add_argument("--scenario", required=True, help="DC scenario token, passed to DC as given")
    run.add_argument("--cap", type=int, default=2, help="most pulls per pool in each phase (default 2)")
    run.add_argument("--skip-baseline", action="store_true")
    live(run)

    win = commands.add_parser("window", help="a list of jobs on one build: rebuild, pulls, report")
    win.add_argument("--jobs", required=True, help="JSON list of jobs, format in window.py")
    win.add_argument("--max-raids", type=int, default=6, help="most DC runs in flight at once (default 6)")
    win.add_argument("--report-dir", help="write each job's report here as soon as the job is finished")
    win.add_argument("--stop-file", help="once this file exists, start no new runs: the runs going finish, then "
                                         "the window ends (refused if the file exists already)")
    sources(win, "commit-ish whose later builds count, for a job that names no head")
    live(win)

    rep = commands.add_parser("report", help="only the report, from the traces and DC's run log on disk")
    common(rep)
    rep.add_argument("--runs", type=lambda text: [part for part in text.split(",") if part],
                     help="DC run ids to report pull by pull")
    rep.add_argument("--full", action="store_true", help="every pool's sections and boss readers in full")
    rep.add_argument("--out", help="write the report to this file, all at once, instead of stdout")
    rep.add_argument("--label", default="", help="text for the report's title")
    rep.add_argument("--note", action="append", default=[], help="a line to put under the title")

    args = parser.parse_args(argv)
    if args.command == "window":
        if args.max_raids < 1:
            parser.error("--max-raids must be at least 1")
    else:
        if args.pulls < 1:
            parser.error("--pulls must be at least 1")
        if args.command == "cycle" and args.cap < args.pulls:
            parser.error("--cap can't be below --pulls")
        args.pools = args.pools or pool_names(args.raid)
        args.diff = args.diff or DEFAULT_DIFF[args.raid]
    if args.command in ("cycle", "window"):
        args.core = pathlib.Path(args.core)
    return args


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    if args.command == "report":
        return report_only(args, Deps(call=lambda command: ""))
    try:
        client = soap.Client()
    except soap.SoapError as exc:
        print(f"refused: {exc}", file=sys.stderr)
        return 2
    if args.command == "window":
        return run_window(args, Deps(call=client))
    return cycle(args, Deps(call=client))


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
