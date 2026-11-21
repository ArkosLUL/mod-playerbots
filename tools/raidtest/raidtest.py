#!/usr/bin/env python3
"""Unattended raid test cycle on mod-dungeon-clear, with no human logged in.

    raidtest.py cycle --boss xt-002 --scenario <dc scenario> --raid uld [--diff 25n] [--pulls 3] [--cap 6]
                      [--skip-baseline] [--skip-rebuild]
    raidtest.py report --boss xt-002 [--diff 25n] [--pulls 3] [--runs tr-...,tr-...]

A cycle takes the server lock, runs baseline pulls on the live build when fewer than --pulls valid ones
exist from before HEAD, rebuilds and restarts the worldserver, then has DC pull the boss until --pulls
valid pulls built after HEAD exist or --cap pulls were run. The markdown report goes to stdout,
progress to stderr.

Exit codes: 0 enough valid pulls, 1 the cap ran out first, 2 refused before touching the server,
3 stopped on a failure (build, restart, driver, crash, a refused start).
"""
from __future__ import annotations

import argparse
import datetime
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
from pulls import DIFF_IDS, Corpus, match_traces
from raidobs.paths import REPO
from raidobs.validity import resolve_since
from report import PullRow, State, render

CORE = REPO.parents[1]
MODULE_REPO = CORE / "modules" / "mod-playerbots"
# Expects the server env to set DC_TESTRUNS_FILE onto the bind-mounted logs volume: DC's default, its
# working directory, sits in the container layer and is wiped on every recreate.
DC_RUNS_FILE = "/azerothcore/env/dist/logs/dc/dc_testruns.jsonl"
# A trace closes up to Obs.IdleCloseSeconds (30 s) after its pull, so wait that out before reading
SETTLE_S = 45.0


class Stop(Exception):
    def __init__(self, reason: str):
        super().__init__(reason)
        self.reason = reason


@dataclass
class Deps:
    call: Callable[[str], str]
    run: Callable[..., subprocess.CompletedProcess] = subprocess.run
    clock: Callable[[], float] = time.monotonic
    now: Callable[[], float] = time.time
    sleep: Callable[[float], None] = time.sleep
    out: TextIO = sys.stdout
    log: TextIO = sys.stderr


def say(deps: Deps, text: str) -> None:
    print(text, file=deps.log, flush=True)


def rows_for_run(phase: str, run: dc.Run, corpus: Corpus) -> list[PullRow]:
    if not run.started_ms or not run.ended_ms:
        return []
    if run.pulls is not None:
        windows = [(pull.start_ms, pull.end_ms) for pull in run.pulls]
        candidates = corpus.between(run.started_ms - 60000, run.ended_ms + 60000)
        traces = match_traces(windows, candidates)
        return [PullRow(phase, run.run_id, index, window, pull.outcome, trace)
                for index, (pull, window, trace) in enumerate(zip(run.pulls, windows, traces), 1)]
    return [PullRow(phase, run.run_id, index, None, "", trace)
            for index, trace in enumerate(corpus.between(run.started_ms, run.ended_ms), 1)]


def dc_run(phase: str, args, deps: Deps, corpus: Corpus, state: State, pulls: int) -> list[PullRow]:
    command = dc.start_command(args.scenario, args.raid, args.diff, pulls)
    unavailable = (soap.SoapUnavailable,)
    try:
        run_id = dc.start_run(deps.call, command, deps.clock, deps.sleep, unavailable)
    except dc.DriverOffline as exc:
        raise Stop(f"the DC test driver never got online: {exc}") from None
    except dc.StartRefused as exc:
        raise Stop(f"DC refused `{command}`: {exc}") from None
    say(deps, f"{phase}: {run_id} started for up to {pulls} pull(s)")

    try:
        run = dc.wait_run(deps.call, run_id, lambda: server.read_file(args.dc_runs_file, deps.run),
                          deps.clock, deps.sleep, unavailable)
    except dc.RunVanished as exc:
        raise Stop(f"{exc}; the worldserver may have crashed") from None
    except server.ContainerDown as exc:
        raise Stop(str(exc)) from None
    except BaseException:
        # don't leave a run going on a server the cycle is about to unlock
        try:
            deps.call(dc.stop_command(run_id))
        except Exception:
            pass
        raise

    say(deps, f"{phase}: {run_id} ended {run.result or '?'}")
    deps.sleep(SETTLE_S)
    state.runs.append((phase, run))
    rows = rows_for_run(phase, run, corpus)
    state.rows += rows
    if not rows:
        reason = f": {run.fail_reason}" if run.fail_reason else ""
        raise Stop(f"{run_id} ended {run.result or 'without a verdict'} without a pull{reason}")
    return rows


def run_phase(phase: str, args, deps: Deps, corpus: Corpus, state: State) -> None:
    """DC runs until --pulls valid pulls exist for this phase, or --cap pulls were run."""
    def have() -> int:
        if phase == "test":
            return len(corpus.counted(args.diff, state.head_time))
        return len(corpus.baseline(args.diff, state.head_time, args.pulls))

    pulled = 0
    while True:
        valid = have()
        say(deps, f"{phase}: {valid}/{args.pulls} valid, {pulled}/{args.cap} pulled")
        if valid >= args.pulls:
            return
        if pulled >= args.cap:
            state.notes.append(f"The {phase} cap of {args.cap} pulls ran out with {valid} valid.")
            return
        rows = dc_run(phase, args, deps, corpus, state, min(args.pulls - valid, args.cap - pulled))
        pulled += len(rows)
        if phase == "baseline" and any(row.trace and row.trace.built and row.trace.built > state.head_time
                                       for row in rows):
            state.notes.append("The live build already holds HEAD, so it can't give a baseline.")
            return
        if phase == "test" and any(row.trace and row.trace.built and row.trace.built < state.head_time
                                   for row in rows):
            raise Stop(f"the running build predates {state.head_sha}, so none of its pulls can count")


def rebuild(args, deps: Deps, state: State) -> None:
    problems = server.dirty_trees(args.core, deps.run)
    if problems:
        raise Stop("refused to build, uncommitted files would ship: " + "; ".join(problems))

    stamp = datetime.datetime.fromtimestamp(deps.now()).strftime("%Y%m%d-%H%M%S")
    log_path = pathlib.Path(args.log_dir) / f"worldserver-build-{stamp}.log"
    state.notes.append(f"Build log: `{log_path}`")
    say(deps, f"building, log in {log_path}")
    code = server.build(args.core, log_path, deps.run)
    if code != 0:
        raise Stop(f"the worldserver build failed (docker compose exit {code}), see {log_path}")

    code, since, recreated = server.restart(args.core, deps.run, deps.now)
    if code != 0:
        raise Stop(f"docker compose up failed (exit {code})")
    if not recreated:
        state.notes.append("The build gave the running image again, so the worldserver kept running.")
        return
    say(deps, "waiting for the worldserver")
    if not server.wait_ready(since, args.ready_timeout, deps.run, deps.clock, deps.sleep):
        raise Stop(f"no `{server.READY_LINE}` in the worldserver log after {args.ready_timeout:.0f} s")


def finish(state: State, corpus: Corpus) -> None:
    for row in state.rows:
        if row.trace:
            try:
                row.trace = corpus.facts(row.trace.path)
            except OSError:
                pass
    state.counted = corpus.counted(state.diff, state.head_time)
    state.baseline = corpus.baseline(state.diff, state.head_time, state.wanted)


def head_of(args) -> tuple[str, datetime.datetime] | None:
    return resolve_since(pathlib.Path(args.repo), args.head)


def cycle(args, deps: Deps) -> int:
    head = head_of(args)
    if not head:
        print(f"cannot resolve {args.head} in {args.repo}", file=deps.log)
        return 2
    corpus = Corpus(args.boss)
    state = State(boss=corpus.boss, diff=args.diff, head_sha=head[0], head_time=head[1], wanted=args.pulls,
                  cap=args.cap)

    owner = f"raidtest cycle {corpus.boss} {args.diff}" + (f": {args.owner}" if args.owner else "")
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
                run_phase("baseline", args, deps, corpus, state)
            if not args.skip_rebuild:
                rebuild(args, deps, state)
            run_phase("test", args, deps, corpus, state)
        except Stop as stop:
            state.stopped = stop.reason
        except soap.SoapError as exc:
            state.stopped = f"SOAP: {exc}"
        except KeyboardInterrupt:
            state.stopped = "interrupted"
        except Exception as exc:
            traceback.print_exc(file=deps.log)
            state.stopped = f"{type(exc).__name__}: {exc}"
        finish(state, corpus)
        render(state, deps.run, deps.out)
    finally:
        lock.release()

    if state.stopped:
        return 3
    return 0 if len(state.counted) >= args.pulls else 1


def report_only(args, deps: Deps) -> int:
    head = head_of(args)
    if not head:
        print(f"cannot resolve {args.head} in {args.repo}", file=deps.log)
        return 2
    corpus = Corpus(args.boss)
    state = State(boss=corpus.boss, diff=args.diff, head_sha=head[0], head_time=head[1], wanted=args.pulls)
    if args.runs:
        try:
            records = dc.parse_records(server.read_file(args.dc_runs_file, deps.run))
        except server.ContainerDown as exc:
            print(str(exc), file=deps.log)
            return 2
        for run_id in args.runs:
            run = dc.find_run(records, run_id)
            if run is None:
                state.notes.append(f"No DC record for {run_id}.")
                continue
            state.runs.append(("run", run))
            state.rows += rows_for_run("run", run, corpus)
    else:
        state.rows = [PullRow("test", "", index, None, "", facts)
                      for index, facts in enumerate(corpus.counted(args.diff, state.head_time)[:args.pulls], 1)]
    finish(state, corpus)
    render(state, deps.run, deps.out)
    return 0


def env_setting(key: str, default: str) -> str:
    try:
        return soap.read_env().get(key) or default
    except (soap.SoapError, OSError, UnicodeError):
        return default


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Unattended raid test cycle on mod-dungeon-clear.")
    commands = parser.add_subparsers(dest="command", required=True)

    def common(sub: argparse.ArgumentParser) -> None:
        sub.add_argument("--boss", required=True, help="RaidObs trace slug, e.g. xt-002 or northrend-beasts")
        sub.add_argument("--diff", choices=sorted(DIFF_IDS), default="25n")
        sub.add_argument("--pulls", type=int, default=3, help="valid pulls wanted (default 3)")
        sub.add_argument("--head", default="HEAD", help="commit-ish the build must be newer than")
        sub.add_argument("--repo", default=str(MODULE_REPO), help="repo --head resolves in")
        sub.add_argument("--dc-runs-file", default=env_setting("DC_RUNS_FILE", DC_RUNS_FILE),
                         help="DC's run log inside the container (.env DC_RUNS_FILE)")

    run = commands.add_parser("cycle", help="baseline, rebuild, pulls, report")
    common(run)
    run.add_argument("--scenario", required=True, help="DC scenario token")
    run.add_argument("--raid", required=True, choices=dc.RAIDS, help="roster pool")
    run.add_argument("--cap", type=int, default=6, help="most pulls per phase (default 6)")
    run.add_argument("--skip-baseline", action="store_true")
    run.add_argument("--skip-rebuild", action="store_true", help="test the build that is running")
    run.add_argument("--core", default=str(CORE), help="core checkout the build compiles")
    run.add_argument("--lock", default=str(server.LOCK_DIR))
    run.add_argument("--owner", default="", help="who runs this, for the lock's owner file")
    run.add_argument("--log-dir", default=str(pathlib.Path(tempfile.gettempdir()) / "raidtest"))
    run.add_argument("--ready-timeout", type=float, default=1800.0, help="seconds to wait for the restart")

    rep = commands.add_parser("report", help="only the report, from the traces on disk")
    common(rep)
    rep.add_argument("--runs", type=lambda text: [part for part in text.split(",") if part],
                     help="DC run ids to report pull by pull")

    args = parser.parse_args(argv)
    if args.pulls < 1:
        parser.error("--pulls must be at least 1")
    if args.command == "cycle" and args.cap < args.pulls:
        parser.error("--cap can't be below --pulls")
    if args.command == "cycle":
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
    return cycle(args, Deps(call=client))


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
