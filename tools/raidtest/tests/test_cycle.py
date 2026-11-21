"""The cycle's control flow with SOAP, docker, git and the trace corpus stood in for.

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

import raidtest  # noqa: E402
import server  # noqa: E402
import soap  # noqa: E402
from pulls import TraceFacts  # noqa: E402

UTC = datetime.timezone.utc
HEAD = datetime.datetime(2026, 11, 21, 12, 0, tzinfo=UTC)
BEFORE = HEAD - datetime.timedelta(hours=2)
AFTER = HEAD + datetime.timedelta(hours=1)
T0 = int(HEAD.timestamp() * 1000) + 3_600_000


def facts(name: str, start: int, built: datetime.datetime) -> TraceFacts:
    return TraceFacts(path=pathlib.Path(name), encounter="xt-002", built=built, diff=1, warnings=[],
                      hardmode=True, start_ms=start, end_ms=start + 60_000, outcome="wipe", closed=True)


class FakeCorpus:
    """Each DC run appends its pulls' traces; counted and baseline read them back."""

    def __init__(self, baseline: int = 3, valid_every: int = 1):
        self.boss = "xt-002"
        self.traces: list[TraceFacts] = [facts(f"old{i}", 0, BEFORE) for i in range(baseline)]
        self.valid_every = valid_every
        self.added = 0
        self.build = AFTER

    def add(self, start: int, end: int, count: int) -> None:
        for index in range(count):
            begin = start + index * 100_000
            trace = facts(f"t{begin}", begin, self.build)
            self.added += 1
            if self.added % self.valid_every:
                trace.warnings = [("raid-dead", "dead")]
            self.traces.append(trace)

    def facts(self, path):
        return next(t for t in self.traces if t.path == path)

    def counted(self, diff, head_time):
        return [t for t in self.traces if t.valid(1) and t.built > head_time]

    def baseline(self, diff, head_time, wanted):
        return [t for t in self.traces if t.valid(1) and t.built < head_time][:wanted]

    def between(self, start, end):
        return [t for t in self.traces if t.start_ms and start <= t.start_ms <= end]


class FakeDc:
    """SOAP and DC's run log: a start answers with a run that has already finished all its pulls."""

    def __init__(self, corpus: FakeCorpus, kill_on: int = 0):
        self.corpus = corpus
        self.kill_on = kill_on
        self.records: list[dict] = []
        self.commands: list[str] = []
        self.next_start = T0

    def call(self, command: str) -> str:
        self.commands.append(command)
        if command.startswith(".dc test start"):
            asked = int(command.rsplit("pulls=", 1)[1])
            count = min(asked, self.kill_on) if self.kill_on else asked
            run_id = f"tr-20261121-1300{len(self.records):02d}"
            start = self.next_start
            pulls = [{"startMs": start + i * 100_000, "endMs": start + i * 100_000 + 60_000,
                      "outcome": "kill" if self.kill_on and i == count - 1 else "wipe"} for i in range(count)]
            self.corpus.add(start, start + count * 100_000, count)
            self.records.append({"runId": run_id, "result": "success", "failReason": "", "startedAtMs": start,
                                 "endedAtMs": start + count * 100_000, "bossPulls": pulls})
            self.next_start += 1_000_000
            return f"Test run started: {run_id} uld-xt002 [Setup]"
        return "no test runs active"

    def read(self, path, run=None) -> str:
        return "\n".join(json.dumps(r) for r in self.records)


class CycleTest(unittest.TestCase):
    def setUp(self):
        folder = tempfile.TemporaryDirectory()
        self.addCleanup(folder.cleanup)
        self.lock = pathlib.Path(folder.name) / "locks" / "server"
        self.rendered = []
        patches = [
            mock.patch.object(raidtest, "resolve_since", return_value=("abc1234", HEAD)),
            mock.patch.object(raidtest, "render", side_effect=lambda state, run, out: self.rendered.append(state)),
            mock.patch.object(server, "dirty_trees", return_value=[]),
            mock.patch.object(server, "build", return_value=0),
            mock.patch.object(server, "restart", return_value=(0, "2026-11-21T13:00:00Z", True)),
            mock.patch.object(server, "wait_ready", return_value=True),
        ]
        for patch in patches:
            patch.start()
            self.addCleanup(patch.stop)

    def args(self, *extra: str):
        with mock.patch.object(soap, "read_env", side_effect=soap.SoapError("no .env")):
            return raidtest.parse_args(["cycle", "--boss", "xt-002", "--scenario", "uld-xt002", "--raid", "uld",
                                        "--lock", str(self.lock), *extra])

    def cycle(self, corpus: FakeCorpus, dc: FakeDc, *extra: str) -> int:
        deps = raidtest.Deps(call=dc.call, clock=lambda: 0.0, sleep=lambda s: None, out=io.StringIO(),
                             log=io.StringIO())
        with mock.patch.object(raidtest, "Corpus", return_value=corpus), \
                mock.patch.object(server, "read_file", side_effect=dc.read):
            return raidtest.cycle(self.args(*extra), deps)

    def starts(self, dc: FakeDc) -> list[str]:
        return [c for c in dc.commands if c.startswith(".dc test start")]

    def test_three_valid_pulls_in_one_run(self):
        corpus = FakeCorpus()
        dc = FakeDc(corpus)
        self.assertEqual(self.cycle(corpus, dc), 0)
        self.assertEqual(self.starts(dc), [".dc test start uld-xt002 roster=uld diff=25n pulls=3"])
        state = self.rendered[0]
        self.assertEqual([row.trace.name for row in state.rows], [f"t{T0}", f"t{T0 + 100_000}",
                                                                   f"t{T0 + 200_000}"])
        self.assertEqual(len(state.counted), 3)
        self.assertFalse(self.lock.exists())
        server.build.assert_called_once()

    def test_a_kill_ends_the_run_and_the_next_asks_for_the_rest(self):
        corpus = FakeCorpus()
        dc = FakeDc(corpus, kill_on=1)
        self.assertEqual(self.cycle(corpus, dc), 0)
        self.assertEqual([c.rsplit("=", 1)[1] for c in self.starts(dc)], ["3", "2", "1"])

    def test_invalid_pulls_are_replaced_until_the_cap(self):
        corpus = FakeCorpus(baseline=3, valid_every=3)
        dc = FakeDc(corpus)
        self.assertEqual(self.cycle(corpus, dc), 1)
        self.assertEqual([c.rsplit("=", 1)[1] for c in self.starts(dc)], ["3", "2", "1"])
        self.assertEqual(len(self.rendered[0].counted), 2)
        self.assertIn("cap", " ".join(self.rendered[0].notes))

    def test_baseline_runs_first_when_short(self):
        corpus = FakeCorpus(baseline=1)
        corpus.build = BEFORE
        dc = FakeDc(corpus)

        def build(*args, **kwargs):
            corpus.build = AFTER
            return 0

        server.build.side_effect = build
        self.assertEqual(self.cycle(corpus, dc), 0)
        self.assertEqual([c.rsplit("=", 1)[1] for c in self.starts(dc)], ["2", "3"])
        self.assertEqual([phase for phase, _ in self.rendered[0].runs], ["baseline", "test"])

    def test_live_build_already_past_head_ends_the_baseline(self):
        corpus = FakeCorpus(baseline=0)
        dc = FakeDc(corpus)
        self.assertEqual(self.cycle(corpus, dc, "--skip-rebuild"), 0)
        self.assertEqual(len(self.starts(dc)), 1)
        self.assertIn("baseline", " ".join(self.rendered[0].notes))

    def test_a_test_pull_on_a_build_before_head_stops(self):
        corpus = FakeCorpus(baseline=3)
        corpus.build = BEFORE
        dc = FakeDc(corpus)
        self.assertEqual(self.cycle(corpus, dc, "--skip-rebuild"), 3)
        self.assertEqual(len(self.starts(dc)), 1)
        self.assertIn("predates abc1234", self.rendered[0].stopped)

    def test_skip_flags(self):
        corpus = FakeCorpus(baseline=0)
        dc = FakeDc(corpus)
        self.assertEqual(self.cycle(corpus, dc, "--skip-baseline", "--skip-rebuild"), 0)
        server.build.assert_not_called()
        server.dirty_trees.assert_not_called()
        self.assertEqual(len(self.starts(dc)), 1)

    def test_lock_held(self):
        self.lock.mkdir(parents=True)
        (self.lock / "owner").write_text("another cycle\n", encoding="utf-8")
        corpus = FakeCorpus()
        dc = FakeDc(corpus)
        self.assertEqual(self.cycle(corpus, dc), 2)
        self.assertEqual(dc.commands, [])
        self.assertTrue(self.lock.exists())

    def test_dirty_tree_refuses_before_anything(self):
        server.dirty_trees.return_value = ["core: modified src/x.cpp"]
        corpus = FakeCorpus()
        dc = FakeDc(corpus)
        self.assertEqual(self.cycle(corpus, dc), 2)
        self.assertEqual(dc.commands, [])
        server.build.assert_not_called()
        self.assertFalse(self.lock.exists())

    def test_failed_build_stops_with_a_report(self):
        server.build.return_value = 1
        corpus = FakeCorpus()
        dc = FakeDc(corpus)
        self.assertEqual(self.cycle(corpus, dc), 3)
        self.assertEqual(dc.commands, [])
        self.assertIn("build failed", self.rendered[0].stopped)
        server.restart.assert_not_called()
        self.assertFalse(self.lock.exists())

    def test_server_never_ready(self):
        server.wait_ready.return_value = False
        corpus = FakeCorpus()
        dc = FakeDc(corpus)
        self.assertEqual(self.cycle(corpus, dc), 3)
        self.assertIn("ready", self.rendered[0].stopped)

    def test_refused_start_stops(self):
        corpus = FakeCorpus()
        dc = FakeDc(corpus)
        dc.call = lambda command: "Test run not started: unknown dungeon 'uld-xt002'"
        self.assertEqual(self.cycle(corpus, dc), 3)
        self.assertIn("unknown dungeon", self.rendered[0].stopped)

    def test_soap_fault_stops(self):
        corpus = FakeCorpus()
        dc = FakeDc(corpus)

        def fault(command):
            raise soap.SoapFault("There is no such command.")

        dc.call = fault
        self.assertEqual(self.cycle(corpus, dc), 3)
        self.assertIn("no such command", self.rendered[0].stopped)
        self.assertFalse(self.lock.exists())

    def test_soap_error_while_waiting_stops_the_dc_run(self):
        corpus = FakeCorpus()
        dc = FakeDc(corpus)
        original = dc.call

        def status_refused(command):
            if command == ".dc test status":
                dc.commands.append(command)
                raise soap.SoapAuthError("SOAP refused the account in .env (HTTP 403)")
            return original(command)

        dc.call = status_refused
        self.assertEqual(self.cycle(corpus, dc), 3)
        run_id = dc.records[0]["runId"]
        self.assertEqual(dc.commands[-1], f".dc test stop {run_id}")
        self.assertIn("HTTP 403", self.rendered[0].stopped)
        self.assertFalse(self.lock.exists())

    def test_unexpected_error_still_reports_and_releases(self):
        corpus = FakeCorpus()
        dc = FakeDc(corpus)
        dc.read = mock.Mock(side_effect=RuntimeError("boom"))
        self.assertEqual(self.cycle(corpus, dc), 3)
        self.assertTrue(dc.commands[-1].startswith(".dc test stop tr-"))
        self.assertEqual(self.rendered[0].stopped, "RuntimeError: boom")
        self.assertFalse(self.lock.exists())

    def test_run_without_pulls_stops(self):
        corpus = FakeCorpus()
        dc = FakeDc(corpus)
        original = dc.call

        def setup_failure(command):
            reply = original(command)
            if command.startswith(".dc test start"):
                dc.records[-1].update(result="setup_failed:login", failReason="bots never logged in",
                                      bossPulls=[])
                del corpus.traces[-3:]
            return reply

        dc.call = setup_failure
        self.assertEqual(self.cycle(corpus, dc), 3)
        self.assertIn("bots never logged in", self.rendered[0].stopped)


class ArgsTest(unittest.TestCase):
    def parse(self, *argv: str):
        with mock.patch.object(soap, "read_env", return_value={"DC_RUNS_FILE": "/srv/dc_testruns.jsonl"}):
            return raidtest.parse_args(list(argv))

    def test_defaults(self):
        args = self.parse("cycle", "--boss", "northrend-beasts", "--scenario", "toc-beasts", "--raid", "toc")
        self.assertEqual((args.diff, args.pulls, args.cap), ("25n", 3, 6))
        self.assertEqual(args.dc_runs_file, "/srv/dc_testruns.jsonl")
        self.assertFalse(args.skip_baseline or args.skip_rebuild)

    def test_report_runs(self):
        args = self.parse("report", "--boss", "xt-002", "--runs", "tr-1,tr-2")
        self.assertEqual(args.runs, ["tr-1", "tr-2"])

    def test_cap_below_pulls(self):
        with self.assertRaises(SystemExit), mock.patch("sys.stderr", io.StringIO()):
            self.parse("cycle", "--boss", "x", "--scenario", "s", "--raid", "uld", "--cap", "2")


if __name__ == "__main__":
    unittest.main()
