"""The cycle's control flow with SOAP, docker, git, DC's run log and the trace corpus stood in for.

    python -m unittest discover -s tools/raidtest/tests
"""
from __future__ import annotations

import datetime
import io
import json
import pathlib
import re
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

import dc as dcmod  # noqa: E402
import pools  # noqa: E402
import raidtest  # noqa: E402
import server  # noqa: E402
import soap  # noqa: E402
from pulls import TraceFacts  # noqa: E402

UTC = datetime.timezone.utc
HEAD = datetime.datetime(2026, 11, 21, 12, 0, tzinfo=UTC)
BEFORE = HEAD - datetime.timedelta(hours=2)
AFTER = HEAD + datetime.timedelta(hours=1)
T0 = int(HEAD.timestamp() * 1000) + 3_600_000
POOLS = ["uld1", "uld2", "uld3"]
ULD_POOLS = [f"uld{n}" for n in range(1, 31)]


class FakeCorpus:
    """Traces by instance, the way the real corpus finds them."""

    def __init__(self):
        self.boss = "xt-002"
        self.traces: list[TraceFacts] = []
        self.build = AFTER

    def add(self, instance: int, start: int, built: datetime.datetime, valid: bool) -> None:
        trace = TraceFacts(path=pathlib.Path(f"603_{instance}_xt-002_{start}.ndjson"), encounter="xt-002",
                           built=built, diff=1, warnings=[] if valid else [("raid-dead", "dead")],
                           hardmode=True, start_ms=start, end_ms=start + 60_000, outcome="wipe", closed=True,
                           map_id=603, instance=instance)
        self.traces.append(trace)

    def rescan(self) -> None:
        pass

    def flush(self) -> None:
        pass

    def facts(self, path):
        return next(t for t in self.traces if t.path == path)

    def in_instance(self, map_id, instance, start, end):
        return sorted((t for t in self.traces if t.instance == instance and start <= t.start_ms <= end),
                      key=lambda t: t.start_ms)


class FakeDc:
    """SOAP and DC's run log. A start answers with a run that has already finished; every run started
    before the next status call shares one stretch of time, the way parallel runs overlap."""

    def __init__(self, corpus: FakeCorpus, kill_on: int = 0, bad: dict[str, int] | None = None):
        self.corpus = corpus
        self.kill_on = kill_on
        self.bad = dict(bad or {})
        # pool: how many of its next pulls the boss engages before the start step
        self.engaged: dict[str, int] = {}
        self.records: list[dict] = []
        self.commands: list[str] = []
        self.now = T0
        self.instance = 100
        self.serial = 0

    def write_run(self, pool: str, count: int, built: datetime.datetime, start: int | None = None) -> dict:
        start = self.now if start is None else start
        self.instance += 1
        pulls = []
        for index in range(count):
            begin = start + 60_000 + index * 100_000
            valid = self.bad.get(pool, 0) <= 0
            self.bad[pool] = self.bad.get(pool, 0) - 1
            self.corpus.add(self.instance, begin, built, valid)
            outcome = "kill" if self.kill_on and index == count - 1 else "wipe"
            kind = "engaged" if self.engaged.get(pool, 0) > 0 else "step"
            self.engaged[pool] = self.engaged.get(pool, 0) - 1
            pulls.append({"n": index + 1, "startMs": begin, "endMs": begin + 60_000, "outcome": outcome,
                          "start": kind})
        self.serial += 1
        record = {"runId": f"tr-20261121-1300{self.serial:02d}-{self.serial}", "pool": pool,
                  "mapId": 603, "instanceId": self.instance, "difficulty": "25n", "result": "success",
                  "failReason": "", "startedAtMs": start, "endedAtMs": start + 60_000 + count * 100_000,
                  "bossPulls": pulls}
        self.records.append(record)
        return record

    def seed_baseline(self, pool: str, count: int = 1) -> None:
        self.write_run(pool, count, BEFORE, start=T0 - 30_000_000 - len(self.records) * 1_000_000)

    def call(self, command: str) -> str:
        self.commands.append(command)
        if command.startswith(".dc test start"):
            pool = re.search(r"roster=(\w+)", command).group(1)
            asked = int(command.rsplit("pulls=", 1)[1])
            count = min(asked, self.kill_on) if self.kill_on else asked
            record = self.write_run(pool, count, self.corpus.build)
            return f"Test run started: {record['runId']} uld-xt002 25n [Setup] elapsed 0s, pool {pool}"
        if command == dcmod.STATUS_COMMAND:
            self.now += 10_000_000
        return "no test runs active"

    def read(self, path=None) -> str:
        return "\n".join(json.dumps(r) for r in self.records)


class FakeClock:
    def __init__(self):
        self.now = 0.0

    def __call__(self) -> float:
        return self.now

    def sleep(self, seconds: float) -> None:
        self.now += seconds


class CycleTest(unittest.TestCase):
    def setUp(self):
        folder = tempfile.TemporaryDirectory()
        self.addCleanup(folder.cleanup)
        self.lock = pathlib.Path(folder.name) / "locks" / "server"
        self.rendered = []
        patches = [
            mock.patch.object(raidtest, "resolve_since", return_value=("abc1234", HEAD)),
            mock.patch.object(raidtest, "render", side_effect=lambda state, run, out, **_: self.rendered.append(state)),
            mock.patch.dict(pools.POOLS_PER_RAID, {"uld": 3, "toc": 3}),
            mock.patch.object(server, "dirty_trees", return_value=[]),
            mock.patch.object(server, "build", return_value=0),
            mock.patch.object(server, "restart", return_value=(0, "2026-11-21T13:00:00Z", True)),
            mock.patch.object(server, "wait_ready", return_value=True),
        ]
        for patch in patches:
            patch.start()
            self.addCleanup(patch.stop)
        self.corpus = FakeCorpus()
        self.dc = FakeDc(self.corpus)

    def args(self, *extra: str):
        with mock.patch.object(soap, "read_env", side_effect=soap.SoapError("no .env")):
            return raidtest.parse_args(["cycle", "--boss", "xt-002", "--scenario", "uld-xt002", "--raid", "uld",
                                        "--lock", str(self.lock), *extra])

    def cycle(self, *extra: str) -> int:
        clock = FakeClock()
        deps = raidtest.Deps(call=lambda command: self.dc.call(command), clock=clock, sleep=clock.sleep,
                             out=io.StringIO(), log=io.StringIO())
        with mock.patch.object(raidtest, "Corpus", return_value=self.corpus), \
                mock.patch.object(dcmod, "read_log", side_effect=lambda path: self.dc.read(path)):
            return raidtest.cycle(self.args(*extra), deps)

    def baseline_for(self, *pools: str) -> None:
        for pool in pools:
            self.dc.seed_baseline(pool)

    def starts(self) -> list[str]:
        return [c for c in self.dc.commands if c.startswith(".dc test start")]

    def started(self) -> list[tuple[str, str]]:
        return [(re.search(r"roster=(\w+)", c).group(1), c.rsplit("=", 1)[1]) for c in self.starts()]

    @property
    def state(self):
        return self.rendered[0]

    def test_every_pool_starts_before_any_is_waited_on(self):
        self.baseline_for(*POOLS)
        self.assertEqual(self.cycle(), 0)
        self.assertEqual(self.dc.commands[:4], [".dc test start uld-xt002 roster=uld1 diff=25n pulls=1",
                                                ".dc test start uld-xt002 roster=uld2 diff=25n pulls=1",
                                                ".dc test start uld-xt002 roster=uld3 diff=25n pulls=1",
                                                ".dc test status"])
        self.assertEqual(len(self.starts()), 3)
        self.assertEqual({pool: len(found) for pool, found in self.state.counted.items()},
                         {"uld1": 1, "uld2": 1, "uld3": 1})
        self.assertFalse(self.lock.exists())
        server.build.assert_called_once()

    def test_parallel_runs_keep_their_own_traces(self):
        self.baseline_for(*POOLS)
        self.cycle()
        test_runs = [run for phase, run in self.state.runs if phase == "test"]
        self.assertEqual(len({run.started_ms for run in test_runs}), 1, "the three runs overlap in time")
        instance_of = {run.run_id: run.instance_id for run in test_runs}
        self.assertEqual(len(set(instance_of.values())), 3)
        test_rows = [row for row in self.state.rows if row.phase == "test"]
        self.assertEqual(sorted(row.pool for row in test_rows), POOLS)
        for row in test_rows:
            self.assertEqual(row.trace.instance, instance_of[row.run_id])

    def test_only_a_short_pool_runs_again(self):
        self.baseline_for(*POOLS)
        self.dc.bad = {"uld2": 1}
        self.assertEqual(self.cycle(), 0)
        self.assertEqual(self.started(), [("uld1", "1"), ("uld2", "1"), ("uld3", "1"), ("uld2", "1")])

    def test_a_pull_whose_boss_engaged_first_is_pulled_again(self):
        self.baseline_for(*POOLS)
        self.dc.engaged = {"uld2": 1}
        self.assertEqual(self.cycle(), 0)
        self.assertEqual(self.started(), [("uld1", "1"), ("uld2", "1"), ("uld3", "1"), ("uld2", "1")])
        uld2 = [row for row in self.state.rows if row.pool == "uld2"]
        self.assertEqual([row.engaged_first for row in uld2], [True, False])
        self.assertEqual(self.state.counted["uld2"], [uld2[1].trace])

    def test_cap_is_per_pool(self):
        self.baseline_for(*POOLS)
        self.dc.bad = {"uld2": 5}
        self.assertEqual(self.cycle(), 1)
        self.assertEqual(self.started(), [("uld1", "1"), ("uld2", "1"), ("uld3", "1"), ("uld2", "1")])
        self.assertEqual(self.state.notes[-1], "uld2: the test cap of 2 pulls ran out with 0 valid.")
        self.assertEqual({pool: len(found) for pool, found in self.state.counted.items()},
                         {"uld1": 1, "uld2": 0, "uld3": 1})

    def test_more_pulls_per_pool(self):
        self.baseline_for(*POOLS)
        self.baseline_for(*POOLS)
        self.dc.kill_on = 1
        self.assertEqual(self.cycle("--pulls", "2", "--cap", "3"), 0)
        self.assertEqual([n for _, n in self.started()], ["2", "2", "2", "1", "1", "1"])

    def test_baseline_runs_only_the_short_pools_in_parallel_first(self):
        self.dc.seed_baseline("uld1", count=3)
        self.corpus.build = BEFORE

        def build(*args, **kwargs):
            self.corpus.build = AFTER
            return 0

        server.build.side_effect = build
        self.assertEqual(self.cycle(), 0)
        self.assertEqual(self.started(), [("uld2", "1"), ("uld3", "1"), ("uld1", "1"), ("uld2", "1"), ("uld3", "1")])
        self.assertEqual(self.dc.commands[2], dcmod.STATUS_COMMAND)
        self.assertEqual([(phase, run.pool) for phase, run in self.state.runs],
                         [("baseline", "uld2"), ("baseline", "uld3"), ("test", "uld1"), ("test", "uld2"),
                          ("test", "uld3")])
        self.assertEqual({pool: len(found) for pool, found in self.state.baseline.items()},
                         {"uld1": 1, "uld2": 1, "uld3": 1})

    def test_live_build_already_past_head_ends_the_baseline(self):
        self.assertEqual(self.cycle("--skip-rebuild"), 0)
        self.assertEqual(len(self.starts()), 3)
        self.assertIn("baseline", " ".join(self.state.notes))

    def test_a_test_pull_on_a_build_before_head_stops(self):
        self.baseline_for(*POOLS)
        self.corpus.build = BEFORE
        self.assertEqual(self.cycle("--skip-rebuild"), 3)
        self.assertEqual(len(self.starts()), 3)
        self.assertIn("predates abc1234", self.state.stopped)

    def test_skip_flags(self):
        self.assertEqual(self.cycle("--skip-baseline", "--skip-rebuild"), 0)
        server.build.assert_not_called()
        server.dirty_trees.assert_not_called()
        self.assertEqual(len(self.starts()), 3)

    def test_lock_held(self):
        self.lock.mkdir(parents=True)
        (self.lock / "owner").write_text("another cycle\n", encoding="utf-8")
        self.assertEqual(self.cycle(), 2)
        self.assertEqual(self.dc.commands, [])
        self.assertTrue(self.lock.exists())

    def test_dirty_tree_refuses_before_anything(self):
        server.dirty_trees.return_value = ["core: modified src/x.cpp"]
        self.assertEqual(self.cycle(), 2)
        self.assertEqual(self.dc.commands, [])
        server.build.assert_not_called()
        self.assertFalse(self.lock.exists())

    def test_failed_build_stops_with_a_report(self):
        self.baseline_for(*POOLS)
        server.build.return_value = 1
        self.assertEqual(self.cycle(), 3)
        self.assertEqual(self.dc.commands, [])
        self.assertIn("build failed", self.state.stopped)
        server.restart.assert_not_called()
        self.assertFalse(self.lock.exists())

    def test_server_never_ready(self):
        self.baseline_for(*POOLS)
        server.wait_ready.return_value = False
        self.assertEqual(self.cycle(), 3)
        self.assertIn("ready", self.state.stopped)

    def test_a_refused_start_stops_the_runs_already_started(self):
        self.baseline_for(*POOLS)
        original = self.dc.call

        def refuse_uld2(command):
            if "roster=uld2" in command:
                self.dc.commands.append(command)
                return "Test run not started: no roster pool 'uld2' (no rosters/uld2.json): .dc test roster create uld2"
            return original(command)

        self.dc.call = refuse_uld2
        self.assertEqual(self.cycle(), 3)
        first = self.dc.records[-1]["runId"]
        self.assertEqual(self.dc.commands[-1], f".dc test stop {first}")
        self.assertNotIn(dcmod.STATUS_COMMAND, self.dc.commands)
        self.assertIn("no roster pool 'uld2'", self.state.stopped)
        self.assertFalse(self.lock.exists())

    def test_soap_fault_stops(self):
        def fault(command):
            raise soap.SoapFault("There is no such command.")

        self.dc.call = fault
        self.assertEqual(self.cycle("--skip-baseline"), 3)
        self.assertIn("no such command", self.state.stopped)
        self.assertFalse(self.lock.exists())

    def test_soap_error_while_waiting_stops_every_run(self):
        self.baseline_for(*POOLS)
        original = self.dc.call

        def status_refused(command):
            if command == dcmod.STATUS_COMMAND:
                self.dc.commands.append(command)
                raise soap.SoapAuthError("SOAP refused the account in .env (HTTP 403)")
            return original(command)

        self.dc.call = status_refused
        self.assertEqual(self.cycle(), 3)
        run_ids = [r["runId"] for r in self.dc.records[-3:]]
        self.assertEqual(self.dc.commands[-3:], [f".dc test stop {run_id}" for run_id in run_ids])
        self.assertIn("HTTP 403", self.state.stopped)
        self.assertFalse(self.lock.exists())

    def test_a_server_that_stops_answering_still_gets_every_run_stopped(self):
        self.baseline_for(*POOLS)
        original = self.dc.call

        def gone(command):
            if command == dcmod.STATUS_COMMAND:
                self.dc.commands.append(command)
                raise soap.SoapUnavailable("no SOAP answer from the worldserver: refused")
            return original(command)

        self.dc.call = gone
        self.assertEqual(self.cycle(), 3)
        run_ids = [r["runId"] for r in self.dc.records[-3:]]
        self.assertEqual(self.dc.commands[-3:], [f".dc test stop {run_id}" for run_id in run_ids])
        self.assertIn("may have crashed", self.state.stopped)
        self.assertFalse(self.lock.exists())

    def test_unexpected_error_still_reports_and_releases(self):
        self.baseline_for(*POOLS)
        original = self.dc.read

        def read(path=None):
            if any(c == dcmod.STATUS_COMMAND for c in self.dc.commands):
                raise RuntimeError("boom")
            return original(path)

        self.dc.read = read
        self.assertEqual(self.cycle(), 3)
        self.assertTrue(all(c.startswith(".dc test stop tr-") for c in self.dc.commands[-3:]))
        self.assertEqual(self.state.stopped, "RuntimeError: boom")
        self.assertIn("Could not read DC's run log", " ".join(self.state.notes))
        self.assertFalse(self.lock.exists())

    def test_a_pool_run_without_pulls_stops_and_keeps_the_others(self):
        self.baseline_for(*POOLS)
        original = self.dc.call

        def setup_failure(command):
            reply = original(command)
            if "roster=uld2" in command:
                self.dc.records[-1].update(result="setup_failed:login", failReason="bots never logged in",
                                           bossPulls=[], instanceId=0)
            return reply

        self.dc.call = setup_failure
        self.assertEqual(self.cycle(), 3)
        self.assertIn("on uld2 ended setup_failed:login without a pull: bots never logged in", self.state.stopped)
        self.assertEqual(sorted(row.pool for row in self.state.rows if row.phase == "test"), ["uld1", "uld3"])

    def test_a_run_that_never_wrote_its_record_stops(self):
        self.baseline_for(*POOLS)
        original = self.dc.call

        def lose_uld3(command):
            reply = original(command)
            if "roster=uld3" in command:
                self.lost = self.dc.records.pop()["runId"]
            return reply

        self.dc.call = lose_uld3
        self.assertEqual(self.cycle(), 3)
        self.assertIn(f"{self.lost} left .dc test status without a record", self.state.stopped)
        self.assertEqual(sorted(row.pool for row in self.state.rows if row.phase == "test"), ["uld1", "uld2"])


class ReportOnlyTest(unittest.TestCase):
    def setUp(self):
        self.rendered = []
        patches = [
            mock.patch.object(raidtest, "resolve_since", return_value=("abc1234", HEAD)),
            mock.patch.object(raidtest, "render", side_effect=lambda state, run, out, **_: self.rendered.append(state)),
            mock.patch.dict(pools.POOLS_PER_RAID, {"uld": 3, "toc": 3}),
        ]
        for patch in patches:
            patch.start()
            self.addCleanup(patch.stop)
        self.corpus = FakeCorpus()
        self.dc = FakeDc(self.corpus)

    def report(self, *extra: str) -> int:
        with mock.patch.object(soap, "read_env", side_effect=soap.SoapError("no .env")):
            args = raidtest.parse_args(["report", "--boss", "xt-002", "--raid", "uld", *extra])
        deps = raidtest.Deps(call=lambda command: "", out=io.StringIO(), log=io.StringIO())
        with mock.patch.object(raidtest, "Corpus", return_value=self.corpus), \
                mock.patch.object(dcmod, "read_log", side_effect=lambda path: self.dc.read(path)):
            return raidtest.report_only(args, deps)

    def test_newest_valid_pulls_of_each_pool(self):
        self.dc.seed_baseline("uld1")
        newest = self.dc.write_run("uld1", 2, AFTER)
        self.dc.write_run("uld2", 1, AFTER)
        self.dc.write_run("toc1", 1, AFTER)
        self.assertEqual(self.report(), 0)
        state = self.rendered[0]
        self.assertEqual([(row.pool, row.run_id, row.number, row.phase) for row in state.rows],
                         [("uld1", newest["runId"], 2, "test"), ("uld2", self.dc.records[2]["runId"], 1, "test")])
        self.assertEqual({pool: len(found) for pool, found in state.baseline.items()},
                         {"uld1": 1, "uld2": 0, "uld3": 0})

    def test_named_runs(self):
        other = self.dc.write_run("toc2", 1, AFTER)
        self.assertEqual(self.report("--runs", f"{other['runId']},tr-missing"), 0)
        state = self.rendered[0]
        self.assertEqual(state.pools, POOLS + ["toc2"])
        self.assertEqual([(row.pool, row.phase) for row in state.rows], [("toc2", "run")])
        self.assertEqual(len(state.counted["toc2"]), 1)
        self.assertIn("No DC record for tr-missing.", state.notes)

    def test_a_timestamp_head_reaches_the_report_as_given(self):
        raidtest.resolve_since.return_value = ("given", HEAD)
        self.assertEqual(self.report("--head", "2026-11-21T12:00"), 0)
        self.assertEqual(self.rendered[0].head_sha, "2026-11-21T12:00")

    def test_a_named_run_without_a_pool_is_left_out(self):
        loose = self.dc.write_run("", 1, AFTER)
        self.assertEqual(self.report("--runs", loose["runId"]), 0)
        state = self.rendered[0]
        self.assertEqual((state.pools, state.runs, state.rows), (POOLS, [], []))
        self.assertIn(f"{loose['runId']} has no pool: DC fielded it without roster=.", state.notes)

    def test_out_writes_the_report_whole_with_label_and_notes(self):
        self.dc.write_run("uld1", 1, AFTER)
        raidtest.render.side_effect = lambda state, run, out, **_: print(f"{state.label}|{state.notes}", file=out)
        with tempfile.TemporaryDirectory() as folder:
            target = pathlib.Path(folder) / "w-xt-002.md"
            self.assertEqual(self.report("--out", str(target), "--label", "job 1", "--note", "a note"), 0)
            self.assertEqual(target.read_text(encoding="utf-8"), "job 1|['a note']\n")
            self.assertEqual(list(pathlib.Path(folder).iterdir()), [target])

    def test_out_leaves_a_stub_naming_the_failure(self):
        raidtest.render.side_effect = RuntimeError("reader blew up")
        with tempfile.TemporaryDirectory() as folder:
            target = pathlib.Path(folder) / "w-xt-002.md"
            self.assertEqual(self.report("--out", str(target), "--label", "job 1"), 3)
            text = target.read_text(encoding="utf-8")
        self.assertIn("# Raid test: xt-002 25n (job 1)", text)
        self.assertIn("**The report failed: RuntimeError: reader blew up**", text)
        self.assertIn("Rerun: `python tools/raidtest/raidtest.py report --boss xt-002 --raid uld", text)

    def test_out_leaves_a_stub_when_the_head_is_unknown(self):
        raidtest.resolve_since.return_value = None
        with tempfile.TemporaryDirectory() as folder:
            target = pathlib.Path(folder) / "w-xt-002.md"
            self.assertEqual(self.report("--out", str(target)), 2)
            self.assertIn("could not be built (exit 2)", target.read_text(encoding="utf-8"))


class ArgsTest(unittest.TestCase):
    def parse(self, *argv: str):
        with mock.patch.object(soap, "read_env", return_value={"DC_RUNS_FILE": "G:/srv/dc_testruns.jsonl"}):
            return raidtest.parse_args(list(argv))

    def refused(self, *argv: str) -> None:
        with self.assertRaises(SystemExit), mock.patch("sys.stderr", io.StringIO()):
            self.parse(*argv)

    def test_uld_defaults(self):
        args = self.parse("cycle", "--boss", "xt-002", "--scenario", "uld-xt002", "--raid", "uld")
        self.assertEqual((args.diff, args.pulls, args.cap, args.pools), ("25n", 1, 2, ULD_POOLS))
        self.assertEqual(args.dc_runs_file, "G:/srv/dc_testruns.jsonl")
        self.assertFalse(args.skip_baseline or args.skip_rebuild)

    def test_toc_defaults_to_heroic(self):
        args = self.parse("cycle", "--boss", "northrend-beasts", "--scenario", "toc-beasts", "--raid", "toc")
        self.assertEqual((args.diff, args.pools), ("25h", [f"toc{n}" for n in range(1, 31)]))

    def test_overrides(self):
        args = self.parse("cycle", "--boss", "x", "--scenario", "s", "--raid", "toc", "--diff", "25n",
                          "--pools", "toc2,toc3", "--pulls", "2", "--cap", "4")
        self.assertEqual((args.diff, args.pools, args.pulls, args.cap), ("25n", ["toc2", "toc3"], 2, 4))

    def test_default_runs_file_is_on_the_host(self):
        with mock.patch.object(soap, "read_env", side_effect=soap.SoapError("no .env")):
            args = raidtest.parse_args(["report", "--boss", "x", "--raid", "uld"])
        self.assertEqual(pathlib.Path(args.dc_runs_file).parts[-5:],
                         ("env", "dist", "logs", "dc", "dc_testruns.jsonl"))

    def test_bad_pools(self):
        for pools in ("Uld1", "uld-1", "uld1,uld1", "toolong1"):
            self.refused("cycle", "--boss", "x", "--scenario", "s", "--raid", "uld", "--pools", pools)

    def test_report_runs(self):
        args = self.parse("report", "--boss", "xt-002", "--raid", "uld", "--runs", "tr-1,tr-2")
        self.assertEqual((args.runs, args.pools), (["tr-1", "tr-2"], ULD_POOLS))
        self.assertEqual((args.full, args.out, args.label, args.note), (False, None, "", []))

    def test_cap_below_pulls(self):
        self.refused("cycle", "--boss", "x", "--scenario", "s", "--raid", "uld", "--pulls", "3")


if __name__ == "__main__":
    unittest.main()
