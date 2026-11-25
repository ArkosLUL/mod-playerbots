"""The window: its jobs file, the scheduler (priority, one run per pool, the raid cap, a job failing
alone), per-job heads, failures that stop the window, and the load line. SOAP, docker, git, DC's run
log and the trace corpus are stood in for.

    python -m unittest discover -s tools/raidtest/tests
"""
from __future__ import annotations

import datetime
import io
import json
import pathlib
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

import dc as dcmod  # noqa: E402
import pools  # noqa: E402
import pulls  # noqa: E402
import raidtest  # noqa: E402
import report  # noqa: E402
import server  # noqa: E402
import soap  # noqa: E402
import window  # noqa: E402
from pulls import TraceFacts  # noqa: E402
from test_pulls import write_trace  # noqa: E402

UTC = datetime.timezone.utc
HEAD = datetime.datetime(2026, 11, 21, 12, 0, tzinfo=UTC)
OLD = HEAD - datetime.timedelta(days=1)
BETWEEN = HEAD - datetime.timedelta(hours=6)
AFTER = HEAD + datetime.timedelta(hours=1)
T0 = int(HEAD.timestamp() * 1000) + 3_600_000
HEADS = {"HEAD": ("abc1234", HEAD), "old1234": ("old1234", OLD), "2026-11-20T12:00": ("given", OLD)}
# scenario: (boss, map)
SCENARIOS = {"uld-xt002": ("xt-002", 603), "uld-ignis": ("ignis", 603), "totc-beasts": ("northrend-beasts", 649)}
DIFF_IDS = {"10n": 0, "25n": 1, "10h": 2, "25h": 3}


def job(scenario: str, **extra) -> dict:
    boss, map_id = SCENARIOS[scenario]
    out = {"boss": boss, "scenario": scenario, "raid": "uld" if map_id == 603 else "toc"}
    out.update(extra)
    return out


XT, IGNIS, BEASTS = "uld-xt002", "uld-ignis", "totc-beasts"


class FakeCorpus:
    def __init__(self, boss: str):
        self.boss = boss
        self.traces: list[TraceFacts] = []

    def rescan(self) -> None:
        pass

    def flush(self) -> None:
        pass

    def facts(self, path):
        return next(t for t in self.traces if t.path == path)

    def in_instance(self, map_id, instance, start, end):
        return sorted((t for t in self.traces if t.instance == instance and start <= t.start_ms <= end),
                      key=lambda t: t.start_ms)


class FakeServer:
    """SOAP and DC's run log. A run stays in `.dc test status` for `length[scenario]` status calls (default
    one), then writes its record and one trace per pull. Starting a pool that is already running is
    recorded in `overlap`."""

    def __init__(self, corpora: dict[str, FakeCorpus]):
        self.corpora = corpora
        self.records: list[dict] = []
        self.commands: list[str] = []
        self.runs: list[tuple[str, str]] = []
        self.going: dict[str, dict] = {}
        self.length: dict[str, int] = {}
        self.bad: dict[tuple[str, str], int] = {}
        self.engaged: dict[tuple[str, str], int] = {}
        self.refuse: dict[tuple[str, str], str] = {}
        self.setup_fail: set[tuple[str, str]] = set()
        self.lose: set[tuple[str, str]] = set()
        self.built = AFTER
        self.gaps = [250, 250]
        self.now = T0
        self.instance = 100
        self.serial = 0
        self.most = 0
        self.overlap: list[str] = []

    def call(self, command: str) -> str:
        self.commands.append(command)
        if command.startswith(".dc test start"):
            _, _, _, scenario, roster, diff, count = command.split()
            pool, diff, count = roster.split("=")[1], diff.split("=")[1], int(count.split("=")[1])
            if (scenario, pool) in self.refuse:
                return self.refuse[(scenario, pool)]
            if any(run["pool"] == pool for run in self.going.values()):
                self.overlap.append(pool)
            self.serial += 1
            run_id = f"tr-20261121-1400{self.serial:02d}-{self.serial}"
            self.runs.append((run_id, pool))
            self.going[run_id] = {"pool": pool, "scenario": scenario, "diff": diff, "pulls": count,
                                  "left": self.length.get(scenario, 1), "start": self.now}
            self.most = max(self.most, len(self.going))
            return f"Test run started: {run_id} {scenario} {diff} [Setup] elapsed 0s, pool {pool}"
        if command == dcmod.STATUS_COMMAND:
            self.now += 600_000
            for run_id, run in list(self.going.items()):
                run["left"] -= 1
                if run["left"] <= 0:
                    del self.going[run_id]
                    self.finish(run_id, run)
            if not self.going:
                return "no test runs active"
            return f"{len(self.going)} test runs active (max unlimited):" + "".join(
                f"\n  {run_id} {run['scenario']} {run['diff']} [Monitoring] elapsed 30s"
                for run_id, run in self.going.items())
        if command.startswith(".dc test stop"):
            run_id = command.split()[-1]
            run = self.going.pop(run_id, None)
            if run:
                self.finish(run_id, run, stopped=True)
            return f"stopping {run_id}"
        return ""

    def finish(self, run_id: str, run: dict, stopped: bool = False) -> None:
        key = (run["scenario"], run["pool"])
        if key in self.lose:
            return
        boss, map_id = SCENARIOS[run["scenario"]]
        self.instance += 1
        count = 0 if stopped or key in self.setup_fail else run["pulls"]
        entries = []
        for index in range(count):
            begin = run["start"] + 60_000 + index * 100_000
            valid = self.bad.get(key, 0) <= 0
            self.bad[key] = self.bad.get(key, 0) - 1
            self.corpora[boss].traces.append(TraceFacts(
                path=pathlib.Path(f"{map_id}_{self.instance}_{boss}_{begin}.ndjson"), encounter=boss,
                built=self.built, diff=DIFF_IDS[run["diff"]], warnings=[] if valid else [("raid-dead", "dead")],
                hardmode=True, start_ms=begin, end_ms=begin + 60_000, outcome="wipe", closed=True, map_id=map_id,
                instance=self.instance, snap_gaps=list(self.gaps)))
            kind = "engaged" if self.engaged.get(key, 0) > 0 else "step"
            self.engaged[key] = self.engaged.get(key, 0) - 1
            entries.append({"n": index + 1, "startMs": begin, "endMs": begin + 60_000, "outcome": "wipe",
                            "start": kind})
        result = "stopped" if stopped else ("setup_failed:login" if key in self.setup_fail else "success")
        self.records.append({
            "runId": run_id, "pool": run["pool"], "mapId": map_id,
            "instanceId": 0 if key in self.setup_fail else self.instance, "difficulty": run["diff"],
            "result": result, "failReason": "bots never logged in" if key in self.setup_fail else "",
            "startedAtMs": run["start"], "endedAtMs": run["start"] + 60_000 + count * 100_000,
            "bossPulls": entries})

    def seed(self, scenario: str, pool: str, built: datetime.datetime, diff: str = "25n") -> None:
        """A finished run from a day before, with one valid pull."""
        saved, self.built, self.now = (self.built, self.now), built, T0 - 86_400_000 - len(self.records) * 1_000_000
        run_id = f"tr-20261120-0900{len(self.records):02d}-{len(self.records)}"
        self.finish(run_id, {"pool": pool, "scenario": scenario, "diff": diff, "pulls": 1, "start": self.now})
        self.built, self.now = saved

    def read(self) -> str:
        return "\n".join(json.dumps(record) for record in self.records)


class FakeClock:
    def __init__(self):
        self.now = 0.0

    def __call__(self) -> float:
        return self.now

    def sleep(self, seconds: float) -> None:
        self.now += seconds


class WindowTest(unittest.TestCase):
    def setUp(self):
        folder = tempfile.TemporaryDirectory()
        self.addCleanup(folder.cleanup)
        self.dir = pathlib.Path(folder.name)
        self.lock = self.dir / "locks" / "server"
        self.rendered = []
        patches = [
            mock.patch.object(raidtest, "resolve_since", side_effect=lambda repo, since: HEADS.get(since)),
            mock.patch.object(raidtest, "render", side_effect=lambda state, run, out, **_: self.rendered.append(state)),
            mock.patch.dict(pools.POOLS_PER_RAID, {"uld": 3, "toc": 3}),
            mock.patch.object(server, "dirty_trees", return_value=[]),
            mock.patch.object(server, "build", return_value=0),
            mock.patch.object(server, "restart", return_value=(0, "2026-11-21T13:00:00Z", True)),
            mock.patch.object(server, "wait_ready", return_value=True),
            mock.patch.object(server, "container_id", return_value="c1 2026-11-21T13:00:00Z"),
        ]
        for patch in patches:
            patch.start()
            self.addCleanup(patch.stop)
        self.corpora = {boss: FakeCorpus(boss) for boss, _ in SCENARIOS.values()}
        self.server = FakeServer(self.corpora)
        self.clock = FakeClock()
        self.sleep = self.clock.sleep
        self.spawned: list[tuple[list[str], int]] = []

    def spawn(self, command, **kwargs):
        self.spawned.append((command, len(self.server.commands)))
        process = mock.Mock()
        process.wait.return_value = 0
        return process

    def window(self, jobs, *extra: str) -> int:
        path = self.dir / "jobs.json"
        path.write_text(json.dumps(jobs), encoding="utf-8")
        with mock.patch.object(soap, "read_env", side_effect=soap.SoapError("no .env")):
            args = raidtest.parse_args(["window", "--jobs", str(path), "--lock", str(self.lock), *extra])
        self.out, self.log = io.StringIO(), io.StringIO()
        deps = raidtest.Deps(call=lambda command: self.server.call(command), clock=self.clock,
                             sleep=lambda seconds: self.sleep(seconds), out=self.out, log=self.log,
                             spawn=self.spawn)
        with mock.patch.object(raidtest, "Corpus", side_effect=lambda boss: self.corpora[boss]), \
                mock.patch.object(dcmod, "read_log", side_effect=lambda path: self.server.read()):
            return raidtest.run_window(args, deps)

    def started(self) -> list[tuple[str, str]]:
        return [(command.split()[3], command.split()[4].split("=")[1])
                for command in self.server.commands if command.startswith(".dc test start")]

    def started_ids(self) -> list[str]:
        return [run_id for run_id, _ in self.server.runs]

    def stops(self) -> list[str]:
        return [command.split()[-1] for command in self.server.commands if command.startswith(".dc test stop")]

    def state(self, number: int):
        return self.rendered[number - 1]

    def counted(self, number: int) -> dict[str, int]:
        return {pool: len(found) for pool, found in self.state(number).counted.items()}

    def header_row(self, number: int) -> str:
        return next(line for line in self.out.getvalue().splitlines() if line.startswith(f"| {number} |"))

    def test_jobs_run_in_priority_order(self):
        self.assertEqual(self.window([job(XT, label="polish"), job(IGNIS, label="check")]), 0)
        self.assertEqual(self.started(), [(XT, "uld1"), (XT, "uld2"), (XT, "uld3"),
                                          (IGNIS, "uld1"), (IGNIS, "uld2"), (IGNIS, "uld3")])
        self.assertEqual(self.server.commands[3], dcmod.STATUS_COMMAND)
        self.assertEqual(self.counted(1), {"uld1": 1, "uld2": 1, "uld3": 1})
        self.assertEqual(self.counted(2), {"uld1": 1, "uld2": 1, "uld3": 1})
        self.assertEqual([state.label for state in self.rendered], ["job 1, polish", "job 2, check"])
        self.assertFalse(self.lock.exists())
        server.build.assert_called_once()

    def test_a_pool_is_free_for_other_jobs_once_its_run_leaves_status(self):
        self.server.bad = {(XT, "uld1"): 1}
        self.assertEqual(self.window([job(XT, pools=["uld1"]), job(IGNIS)]), 0)
        self.assertEqual(self.started(), [(XT, "uld1"), (IGNIS, "uld2"), (IGNIS, "uld3"),
                                          (IGNIS, "uld1"), (XT, "uld1")])
        status_calls = [i for i, c in enumerate(self.server.commands) if c == dcmod.STATUS_COMMAND]
        ignis_uld1 = self.server.commands.index(".dc test start uld-ignis roster=uld1 diff=25n pulls=1")
        self.assertEqual(ignis_uld1, status_calls[0] + 1)

    def test_a_job_waits_for_its_own_run_to_be_read(self):
        self.server.bad = {(XT, "uld1"): 1}
        self.assertEqual(self.window([job(XT, pools=["uld1"])]), 0)
        self.assertEqual(self.started(), [(XT, "uld1"), (XT, "uld1")])
        second = [i for i, c in enumerate(self.server.commands) if c.startswith(".dc test start")][1]
        self.assertGreaterEqual(self.clock.now, raidtest.SETTLE_S)
        self.assertEqual(self.server.overlap, [])
        self.assertGreater(second, self.server.commands.index(dcmod.STATUS_COMMAND))

    def test_a_member_still_held_by_the_last_run_is_waited_for(self):
        replies = iter(["Test run not started: pool member 'Uldxaa' is logged in"])
        original = self.server.call

        def held(command):
            if command.startswith(".dc test start"):
                reply = next(replies, None)
                if reply:
                    self.server.commands.append(command)
                    return reply
            return original(command)

        self.server.call = held
        self.assertEqual(self.window([job(XT, pools=["uld1"])]), 0)
        self.assertEqual(self.started(), [(XT, "uld1"), (XT, "uld1")])
        self.assertEqual(len(self.server.runs), 1)

    def test_a_member_held_past_the_busy_wait_fails_the_job(self):
        self.server.refuse = {(XT, "uld1"): "Test run not started: pool member 'Uldxaa' is logged in"}
        self.assertEqual(self.window([job(XT, pools=["uld1"])]), 3)
        self.assertIn("is logged in", self.state(1).stopped)
        self.assertGreaterEqual(self.clock.now, 60)

    def test_a_pull_whose_boss_engaged_first_is_pulled_again(self):
        self.server.engaged = {(XT, "uld1"): 1}
        self.assertEqual(self.window([job(XT, pools=["uld1"])]), 0)
        self.assertEqual(self.started(), [(XT, "uld1"), (XT, "uld1")])
        rows = self.state(1).rows
        self.assertEqual([row.engaged_first for row in rows], [True, False])
        self.assertEqual(self.state(1).counted["uld1"], [rows[1].trace])
        self.assertEqual(report.verdict(rows[0], "25n", HEAD), "no: the boss engaged before the start step, no refresh")
        self.assertIn("| uld1 1/2 |", self.header_row(1))

    def test_a_pool_runs_one_run_at_a_time(self):
        self.server.length = {XT: 4}
        self.assertEqual(self.window([job(XT, pools=["uld1"]), job(IGNIS, pools=["uld1", "uld2"])]), 0)
        self.assertEqual(self.started(), [(XT, "uld1"), (IGNIS, "uld2"), (IGNIS, "uld1")])
        self.assertEqual(self.server.overlap, [])
        ignis_uld1 = [i for i, c in enumerate(self.server.commands) if c.startswith(".dc test start uld-ignis")][1]
        self.assertGreaterEqual(self.server.commands[:ignis_uld1].count(dcmod.STATUS_COMMAND), 4)

    def test_the_raid_cap_holds_across_both_raids(self):
        self.assertEqual(self.window([job(XT), job(BEASTS)], "--max-raids", "2"), 0)
        self.assertEqual(self.server.most, 2)
        self.assertEqual(self.started(), [(XT, "uld1"), (XT, "uld2"), (XT, "uld3"),
                                          (BEASTS, "toc1"), (BEASTS, "toc2"), (BEASTS, "toc3")])
        self.assertIn("at most 2 DC runs in flight at once (cap 2)", self.out.getvalue())

    def test_both_raids_run_at_once_by_default(self):
        self.assertEqual(self.window([job(XT), job(BEASTS)]), 0)
        self.assertEqual(self.server.most, 6)
        self.assertTrue(all(c.startswith(".dc test start") for c in self.server.commands[:6]))
        self.assertIn("diff=25h", self.server.commands[3])

    def test_at_most_six_runs_start_per_poll(self):
        with mock.patch.dict(pools.POOLS_PER_RAID, {"uld": 8}):
            self.assertEqual(self.window([job(XT)]), 0)
        starts = [index for index, command in enumerate(self.server.commands) if command.startswith(".dc test start")]
        self.assertEqual(starts[:6], list(range(6)))
        self.assertEqual(self.server.commands[6], dcmod.STATUS_COMMAND)
        self.assertEqual(sorted(pool for _, pool in self.started()), sorted(f"uld{n}" for n in range(1, 9)))

    def test_a_refused_start_fails_that_job_alone(self):
        self.server.refuse = {(XT, "uld2"): "Test run not started: no roster pool 'uld2'"}
        self.assertEqual(self.window([job(XT), job(IGNIS)]), 3)
        self.assertEqual(self.started(), [(XT, "uld1"), (XT, "uld2"), (IGNIS, "uld2"), (IGNIS, "uld3"),
                                          (IGNIS, "uld1")])
        self.assertEqual(self.stops(), [])
        self.assertIn("no roster pool 'uld2'", self.state(1).stopped)
        self.assertIsNone(self.state(2).stopped)
        self.assertEqual(self.counted(1), {"uld1": 1, "uld2": 0, "uld3": 0})
        self.assertIn("| failed: DC refused", self.header_row(1))
        self.assertIn("| done | uld1 1/1, uld2 1/1, uld3 1/1 |", self.header_row(2))
        self.assertNotIn("**Stopped:**", self.out.getvalue())

    def test_a_run_without_a_pull_fails_its_job_alone(self):
        self.server.setup_fail = {(XT, "uld2")}
        self.assertEqual(self.window([job(XT), job(IGNIS)]), 3)
        self.assertIn("on uld2 ended setup_failed:login without a pull: bots never logged in", self.state(1).stopped)
        self.assertEqual(self.started().count((XT, "uld2")), 1)
        self.assertEqual(self.counted(2), {"uld1": 1, "uld2": 1, "uld3": 1})

    def test_a_run_lost_without_a_record_fails_its_job_alone(self):
        self.server.lose = {(XT, "uld3")}
        self.assertEqual(self.window([job(XT), job(IGNIS)]), 3)
        self.assertIn("left .dc test status without a record", self.state(1).stopped)
        self.assertEqual(self.stops(), [])
        self.assertEqual(self.counted(2), {"uld1": 1, "uld2": 1, "uld3": 1})

    def test_a_lost_run_after_a_restart_stops_the_window(self):
        self.server.lose = {(XT, "uld3")}
        self.server.length = {BEASTS: 30}
        ids = iter(["c1 2026-11-21T13:00:00Z"])
        server.container_id.side_effect = lambda run: next(ids, "c2 2026-11-21T15:00:00Z")
        self.assertEqual(self.window([job(XT), job(BEASTS)]), 3)
        toc_runs = [run_id for run_id, pool in self.server.runs if pool.startswith("toc")]
        self.assertEqual(len(toc_runs), 3)
        self.assertEqual(sorted(self.stops()), sorted(toc_runs))
        self.assertIn("crashed", self.state(1).stopped)
        self.assertIn("| stopped |", self.header_row(2))
        self.assertIn("**Stopped:**", self.out.getvalue())
        self.assertFalse(self.lock.exists())

    def test_soap_going_down_stops_every_run(self):
        original = self.server.call

        def refused(command):
            if command == dcmod.STATUS_COMMAND:
                self.server.commands.append(command)
                raise soap.SoapAuthError("SOAP refused the account in .env (HTTP 403)")
            return original(command)

        self.server.call = refused
        self.assertEqual(self.window([job(XT), job(BEASTS)]), 3)
        self.assertEqual(sorted(self.stops()), sorted(self.started_ids()))
        self.assertEqual(len(self.stops()), 6)
        self.assertIn("HTTP 403", self.state(1).stopped)
        self.assertFalse(self.lock.exists())

    def test_a_server_gone_quiet_stops_the_window(self):
        original = self.server.call

        def gone(command):
            if command == dcmod.STATUS_COMMAND:
                raise soap.SoapUnavailable("no SOAP answer from the worldserver: refused")
            return original(command)

        self.server.call = gone
        self.assertEqual(self.window([job(XT)]), 3)
        self.assertEqual(len(self.stops()), 3)
        self.assertIn("may have crashed", self.state(1).stopped)
        self.assertGreaterEqual(self.clock.now, window.DOWN_AFTER_S)

    def test_an_interrupt_stops_and_awaits_every_run(self):
        self.server.length = {XT: 3}

        def interrupt(seconds):
            raise KeyboardInterrupt

        self.sleep = interrupt
        self.assertEqual(self.window([job(XT)]), 3)
        self.assertEqual(len(self.stops()), 3)
        self.assertEqual(self.server.commands[-1], dcmod.STATUS_COMMAND)
        self.assertEqual(self.server.going, {})
        self.assertEqual([run.result for _, run in self.state(1).runs], ["stopped"] * 3)
        self.assertEqual(self.state(1).stopped, "interrupted")

    def test_a_stopped_run_that_writes_no_record_is_named(self):
        self.server.length = {XT: 3}
        self.server.lose = {(XT, "uld2")}

        def interrupt(seconds):
            raise KeyboardInterrupt

        self.sleep = interrupt
        self.assertEqual(self.window([job(XT)]), 3)
        lost = self.started_ids()[1]
        self.assertIn(f"Ended with no record in DC's run log when the window gave up the server: {lost} "
                      "(job 1 (xt-002 25n) on uld2).", self.out.getvalue())

    def test_a_driver_that_never_gets_online_stops_every_run(self):
        self.server.refuse = {(BEASTS, "toc1"): "The test driver is logging in, retry in a few seconds."}
        self.assertEqual(self.window([job(XT), job(BEASTS)]), 3)
        self.assertEqual([pool for _, pool in self.server.runs], ["uld1", "uld2", "uld3"])
        self.assertEqual(sorted(self.stops()), sorted(self.started_ids()))
        self.assertIn("**Stopped:** the DC test driver never got online", self.out.getvalue())
        self.assertFalse(self.lock.exists())

    def test_per_job_head(self):
        self.server.built = BETWEEN
        self.server.seed(IGNIS, "uld1", OLD - datetime.timedelta(hours=1))
        self.assertEqual(self.window([job(XT), job(IGNIS, head="old1234")]), 3)
        self.assertIn("the running build predates abc1234", self.state(1).stopped)
        self.assertEqual(self.counted(1), {"uld1": 0, "uld2": 0, "uld3": 0})
        self.assertIsNone(self.state(2).stopped)
        self.assertEqual(self.counted(2), {"uld1": 1, "uld2": 1, "uld3": 1})
        self.assertEqual({pool: len(found) for pool, found in self.state(2).baseline.items()},
                         {"uld1": 1, "uld2": 0, "uld3": 0})
        self.assertEqual((self.state(2).head_sha, self.state(2).head_time), ("old1234", OLD))
        self.assertIn("| `old1234` | done |", self.header_row(2))

    def test_a_build_before_a_head_fails_every_job_with_that_head_at_once(self):
        self.server.built = BETWEEN
        self.server.length = {IGNIS: 30}
        self.assertEqual(self.window([job(XT), job(IGNIS), job(BEASTS, head="old1234")]), 3)
        ignis = [run_id for (run_id, _), (scenario, _) in zip(self.server.runs, self.started()) if scenario == IGNIS]
        self.assertEqual(self.started()[:6], [(XT, "uld1"), (XT, "uld2"), (XT, "uld3"),
                                              (BEASTS, "toc1"), (BEASTS, "toc2"), (BEASTS, "toc3")])
        self.assertEqual(sorted(self.stops()), sorted(ignis))
        self.assertIn("the running build predates abc1234", self.state(2).stopped)
        self.assertIsNone(self.state(3).stopped)
        self.assertEqual(self.counted(3), {"toc1": 1, "toc2": 1, "toc3": 1})

    def test_a_timestamp_head_reaches_the_report_as_given(self):
        self.server.built = BETWEEN
        self.assertEqual(self.window([job(XT, head="2026-11-20T12:00")]), 0)
        self.assertEqual(self.state(1).head_sha, "2026-11-20T12:00")
        self.assertIn("| `2026-11-20T12:00` | done |", self.header_row(1))

    def test_a_job_whose_pools_already_count_starts_nothing(self):
        for pool in ("uld1", "uld2", "uld3"):
            self.server.seed(XT, pool, AFTER)
        self.assertEqual(self.window([job(XT), job(IGNIS)]), 0)
        self.assertEqual(self.started(), [(IGNIS, "uld1"), (IGNIS, "uld2"), (IGNIS, "uld3")])
        self.assertIn("| done | uld1 1/0, uld2 1/0, uld3 1/0 |", self.header_row(1))

    def test_a_cap_that_runs_out(self):
        self.server.bad = {(XT, "uld2"): 5}
        self.assertEqual(self.window([job(XT)]), 1)
        self.assertEqual(self.started(), [(XT, "uld1"), (XT, "uld2"), (XT, "uld3"), (XT, "uld2")])
        self.assertIn("uld2: the cap of 2 pulls ran out with 0 valid.", self.state(1).notes)
        self.assertIn("| cap ran out | uld1 1/1, uld2 0/2, uld3 1/1 |", self.header_row(1))

    def test_more_pulls_per_pool(self):
        self.assertEqual(self.window([job(XT, pools=["uld1"], pulls=2, cap=3)]), 0)
        self.assertEqual(self.server.commands[0], ".dc test start uld-xt002 roster=uld1 diff=25n pulls=2")

    def test_skip_rebuild(self):
        self.assertEqual(self.window([job(XT)], "--skip-rebuild"), 0)
        server.build.assert_not_called()
        server.dirty_trees.assert_not_called()
        self.assertIn("not rebuilt (--skip-rebuild)", self.out.getvalue())

    def test_failed_build_stops_before_any_run(self):
        server.build.return_value = 1
        self.assertEqual(self.window([job(XT), job(BEASTS)]), 3)
        self.assertEqual(self.server.commands, [])
        server.restart.assert_not_called()
        text = self.out.getvalue()
        self.assertIn("the rebuild did not finish", text)
        self.assertIn("**Stopped:** the worldserver build failed", text)
        self.assertIn("| stopped |", self.header_row(2))
        self.assertFalse(self.lock.exists())

    def test_dirty_tree_refuses(self):
        server.dirty_trees.return_value = ["core: modified src/x.cpp"]
        self.assertEqual(self.window([job(XT)]), 2)
        self.assertEqual(self.server.commands, [])
        server.build.assert_not_called()
        self.assertIn("- core: modified src/x.cpp", self.out.getvalue())
        self.assertFalse(self.lock.exists())

    def test_lock_held(self):
        self.lock.mkdir(parents=True)
        (self.lock / "owner").write_text("a cycle\n", encoding="utf-8")
        self.assertEqual(self.window([job(XT)]), 2)
        self.assertEqual(self.server.commands, [])
        self.assertTrue(self.lock.exists())

    def test_bad_jobs_file_refuses_before_the_lock(self):
        self.assertEqual(self.window([job(XT, pull=1)]), 2)
        self.assertIn("unknown field pull", self.log.getvalue())
        self.assertFalse(self.lock.parent.exists())

    def test_unknown_head_refuses(self):
        self.assertEqual(self.window([job(XT, head="nosuch")]), 2)
        self.assertIn("cannot resolve nosuch (job 1", self.log.getvalue())

    def test_each_job_report_starts_as_soon_as_the_job_is_finished(self):
        self.server.length = {IGNIS: 30}
        folder = self.dir / "window28"
        self.assertEqual(self.window([job(XT, label="polish"), job(IGNIS, pools=["uld1"]),
                                      job(XT, diff="10n", pools=["uld2"])], "--report-dir", str(folder)), 0)
        self.assertEqual(self.rendered, [])
        (xt, at_xt), (xt10, _), (ignis, at_ignis) = self.spawned
        self.assertLess(at_xt, len(self.server.commands) - 1)
        self.assertLess(at_xt, at_ignis)
        self.assertEqual(xt[2:6], ["report", "--boss", "xt-002", "--raid"])
        self.assertIn("--runs", xt)
        self.assertEqual(xt[xt.index("--pools") + 1], "uld1,uld2,uld3")
        self.assertEqual(xt[xt.index("--head") + 1], "abc1234")
        self.assertEqual(xt[xt.index("--out") + 1], str(folder / "window28-xt-002-25n-job1.md"))
        self.assertEqual(xt10[xt10.index("--out") + 1], str(folder / "window28-xt-002-10n-job3.md"))
        self.assertEqual(ignis[ignis.index("--out") + 1], str(folder / "window28-ignis.md"))
        text = self.out.getvalue()
        self.assertIn("\n# Job reports\n", text)
        self.assertIn(f"- job 2 (ignis 25n): `{folder / 'window28-ignis.md'}`", text)
        self.assertLess(text.index("# Job reports"), text.index("# Window load"))

    def test_a_failed_job_still_gets_its_report(self):
        self.server.refuse = {(XT, "uld2"): "Test run not started: no roster pool 'uld2'"}
        self.assertEqual(self.window([job(XT)], "--report-dir", str(self.dir / "w")), 3)
        (command, _), = self.spawned
        notes = [command[i + 1] for i, part in enumerate(command) if part == "--note"]
        self.assertTrue(any(note.startswith("The job failed: DC refused") for note in notes))

    def test_a_report_carries_the_cap_note(self):
        self.server.bad = {(XT, "uld2"): 5}
        self.assertEqual(self.window([job(XT)], "--report-dir", str(self.dir / "w")), 1)
        (command, _), = self.spawned
        self.assertIn("uld2: the cap of 2 pulls ran out with 0 valid.",
                      [command[i + 1] for i, part in enumerate(command) if part == "--note"])

    def test_reports_are_awaited_after_the_lock_is_released(self):
        held = []

        def spawn(command, **kwargs):
            process = mock.Mock()
            process.wait.side_effect = lambda timeout: held.append(self.lock.exists()) or 0
            return process

        self.spawn = spawn
        self.assertEqual(self.window([job(XT)], "--report-dir", str(self.dir / "w")), 0)
        self.assertEqual(held, [False])

    def test_a_report_folder_with_old_reports_refuses_before_the_lock(self):
        folder = self.dir / "w"
        folder.mkdir()
        (folder / "w-xt-002.md").write_text("old", encoding="utf-8")
        self.assertEqual(self.window([job(XT)], "--report-dir", str(folder)), 2)
        self.assertIn("already holds reports", self.log.getvalue())
        self.assertEqual(self.server.commands, [])
        self.assertFalse(self.lock.parent.exists())

    def test_an_unusable_report_folder_refuses_before_the_lock(self):
        blocker = self.dir / "w"
        blocker.write_text("a file, not a folder", encoding="utf-8")
        self.assertEqual(self.window([job(XT)], "--report-dir", str(blocker / "sub")), 2)
        self.assertIn("can't use --report-dir", self.log.getvalue())
        self.assertFalse(self.lock.parent.exists())

    def test_a_stop_file_starts_no_new_runs_and_lets_the_going_ones_finish(self):
        stop = self.dir / "window.stop"
        original = self.server.call

        def call(command):
            if command == dcmod.STATUS_COMMAND:
                stop.touch()
            return original(command)

        self.server.call = call
        self.server.length = {XT: 3}
        folder = self.dir / "w"
        self.assertEqual(self.window([job(XT), job(IGNIS)], "--stop-file", str(stop), "--report-dir", str(folder)), 1)
        self.assertEqual(self.started(), [(XT, "uld1"), (XT, "uld2"), (XT, "uld3")])
        self.assertEqual(self.stops(), [])
        self.assertIn("| done | uld1 1/1, uld2 1/1, uld3 1/1 |", self.header_row(1))
        self.assertIn("| cut short | uld1 0/0, uld2 0/0, uld3 0/0 |", self.header_row(2))
        self.assertIn(f"{stop} appeared: starting no new runs, 3 going finish", self.log.getvalue())
        self.assertIn(f"The window started no new runs once `{stop}` appeared.", self.out.getvalue())
        (ignis, _), (xt, _) = self.spawned
        notes = {command[command.index("--boss") + 1]: [command[i + 1] for i, part in enumerate(command)
                                                        if part == "--note"] for command in (ignis, xt)}
        self.assertTrue(any(note.startswith("No new run started once") for note in notes["ignis"]))
        self.assertFalse(any(note.startswith("No new run started once") for note in notes["xt-002"]))

    def test_a_stop_file_that_exists_already_refuses_before_the_lock(self):
        stop = self.dir / "window.stop"
        stop.touch()
        self.assertEqual(self.window([job(XT)], "--stop-file", str(stop)), 2)
        self.assertIn("exists already", self.log.getvalue())
        self.assertEqual(self.server.commands, [])
        self.assertFalse(self.lock.parent.exists())

    def test_header_and_load_line(self):
        self.server.gaps = [250] * 17 + [260, 300, 450]
        self.assertEqual(self.window([job(XT, label="polish")]), 0)
        text = self.out.getvalue()
        self.assertTrue(text.startswith("# Raid test window\n"))
        self.assertIn("Build HEAD `abc1234` committed", text)
        self.assertIn("rebuilt for this window", text)
        self.assertIn("| 1 | polish | xt-002 | 25n | `abc1234` | done | uld1 1/1, uld2 1/1, uld3 1/1 |", text)
        load = text.split("\n# Window load\n", 1)[1]
        self.assertIn("RaidObs snapshot gaps over 3 traces (60 gaps, cadence 250 ms): mean 263 ms, p95 300, "
                      "p99 450, max 450, 3 over 400 ms; at most 3 DC runs in flight at once (cap 30).", load)


class JobsTest(unittest.TestCase):
    def parse(self, *jobs):
        return window.parse_jobs(list(jobs), "HEAD")

    def refused(self, data, message: str) -> None:
        with self.assertRaises(ValueError) as caught:
            window.parse_jobs(data, "HEAD")
        self.assertIn(message, str(caught.exception))

    def test_defaults(self):
        uld, toc = self.parse(job(XT), job(BEASTS))
        self.assertEqual((uld.number, uld.pools, uld.diff, uld.pulls, uld.cap, uld.head, uld.label),
                         (1, [f"uld{n}" for n in range(1, 31)], "25n", 1, 2, "HEAD", ""))
        self.assertEqual((toc.number, toc.pools, toc.diff), (2, [f"toc{n}" for n in range(1, 31)], "25h"))

    def test_overrides(self):
        (parsed,) = self.parse(job(BEASTS, pools=["toc2"], diff="10h", pulls=2, cap=4, head="abc", label="check-10"))
        self.assertEqual((parsed.pools, parsed.diff, parsed.pulls, parsed.cap, parsed.head, parsed.label),
                         (["toc2"], "10h", 2, 4, "abc", "check-10"))
        self.assertEqual(parsed.name, "job 1 (check-10 northrend-beasts 10h)")

    def test_refusals(self):
        self.refused({"jobs": []}, "non-empty JSON list")
        self.refused([], "non-empty JSON list")
        self.refused(["xt"], "job 1 is not a JSON object")
        self.refused([job(XT), job(XT, pull=2)], "job 2: unknown field pull")
        self.refused([{"scenario": XT, "raid": "uld"}], "boss must be")
        self.refused([{**job(XT), "scenario": "uld xt"}], "one DC token")
        self.refused([job(XT, raid="naxx")], "raid must be one of uld, toc")
        self.refused([job(XT, pools="uld1")], "pools must be")
        self.refused([job(XT, pools=["Uld1"])], "pools must be")
        self.refused([job(XT, pools=["uld1", "uld1"])], "a pool is named twice")
        self.refused([job(XT, diff="25")], "diff must be")
        self.refused([job(XT, pulls=0)], "pulls must be")
        self.refused([job(XT, pulls=True)], "pulls must be")
        self.refused([job(XT, pulls=3)], "cap 2 can't be below pulls 3")
        self.refused([job(XT, head="")], "head must be")
        self.refused([job(XT, label=3)], "label must be")

    def test_load_jobs(self):
        with tempfile.TemporaryDirectory() as folder:
            path = pathlib.Path(folder) / "jobs.json"
            path.write_text(json.dumps([job(XT)]), encoding="utf-8")
            self.assertEqual(window.load_jobs(path, "HEAD")[0].scenario, XT)
            path.write_text(json.dumps([job(IGNIS)]), encoding="utf-8-sig")
            self.assertEqual(window.load_jobs(path, "HEAD")[0].scenario, IGNIS)
            path.write_text("[{", encoding="utf-8")
            with self.assertRaises(ValueError):
                window.load_jobs(path, "HEAD")


class LoadTest(unittest.TestCase):
    def test_gaps_from_the_pull_on(self):
        snaps = [{"e": "snap", "t": t, "u": []} for t in (250, 500, 900)]
        with tempfile.TemporaryDirectory() as folder:
            path = write_trace(pathlib.Path(folder), HEAD, OLD,
                               records=[{"e": "snap", "t": -250, "u": []}, *snaps])
            self.assertEqual(pulls.read_facts(path).snap_gaps, [250, 250, 400])

    def facts(self, gaps: list[int]) -> TraceFacts:
        return TraceFacts(path=pathlib.Path("a"), encounter="xt-002", built=None, diff=1, warnings=[],
                          hardmode=None, start_ms=0, end_ms=1, outcome="kill", closed=True, snap_gaps=gaps)

    def test_load_line(self):
        line = report.load_line([self.facts([250] * 98 + [410, 500]), self.facts([])], 4, 6)
        self.assertEqual(line, "RaidObs snapshot gaps over 2 traces (100 gaps, cadence 250 ms): mean 254 ms, "
                               "p95 250, p99 410, max 500, 2 over 400 ms; at most 4 DC runs in flight at once "
                               f"(cap 6). For reference, {report.LOAD_REFERENCE}.")

    def test_no_gaps(self):
        self.assertEqual(report.load_line([], 1, 6), "No snapshot gaps to read in this window's traces; "
                                                     "at most 1 DC run in flight at once (cap 6).")

    def test_percentile_is_nearest_rank(self):
        self.assertEqual(report.percentile([1, 2, 3, 4], 0.5), 2)
        self.assertEqual(report.percentile([7], 0.99), 7)


class WindowArgsTest(unittest.TestCase):
    def parse(self, *argv: str):
        with mock.patch.object(soap, "read_env", side_effect=soap.SoapError("no .env")):
            return raidtest.parse_args(list(argv))

    def test_defaults(self):
        args = self.parse("window", "--jobs", "jobs.json")
        self.assertEqual((args.jobs, args.max_raids, args.head, args.skip_rebuild), ("jobs.json", 30, "HEAD", False))
        self.assertIsInstance(args.core, pathlib.Path)

    def test_max_raids_at_least_one(self):
        with self.assertRaises(SystemExit), mock.patch("sys.stderr", io.StringIO()):
            self.parse("window", "--jobs", "jobs.json", "--max-raids", "0")


if __name__ == "__main__":
    unittest.main()
