"""DC's console replies, its run records, and the start and wait loops.

    python -m unittest discover -s tools/raidtest/tests
"""
from __future__ import annotations

import json
import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

import dc  # noqa: E402


class FakeClock:
    def __init__(self):
        self.now = 0.0

    def __call__(self) -> float:
        return self.now

    def sleep(self, seconds: float) -> None:
        self.now += seconds


class Down(Exception):
    pass


def scripted(replies):
    """A SOAP stand-in answering from a list; an exception instance in the list is raised."""
    sent = []

    def call(command: str) -> str:
        sent.append(command)
        reply = replies.pop(0) if len(replies) > 1 else replies[0]
        if isinstance(reply, BaseException):
            raise reply
        return reply

    return call, sent


class CommandTest(unittest.TestCase):
    def test_start(self):
        self.assertEqual(dc.start_command("uld-xt002", "uld", "25n", 3),
                         ".dc test start uld-xt002 roster=uld diff=25n pulls=3")

    def test_start_rejects_bad_values(self):
        with self.assertRaises(ValueError):
            dc.start_command("x", "uld", "25", 3)
        with self.assertRaises(ValueError):
            dc.start_command("x", "naxx", "25n", 3)
        with self.assertRaises(ValueError):
            dc.start_command("x", "uld", "25n", 0)

    def test_stop(self):
        self.assertEqual(dc.stop_command("tr-20261121-120000"), ".dc test stop tr-20261121-120000")


class StartReplyTest(unittest.TestCase):
    def test_started(self):
        reply = "Test run started: tr-20261121-120102-3 uld-xt002 [Setup] elapsed 0s, bosses 0/1"
        self.assertEqual(dc.parse_start_reply(reply), ("started", "tr-20261121-120102-3"))

    def test_started_without_the_usual_id(self):
        self.assertEqual(dc.parse_start_reply("Test run started: run7 uld [Setup]"), ("started", "run7"))

    def test_pending(self):
        for reply in ("test driver 'Dcdriver' is logging in — retry in a few seconds",
                      "test driver 'Dcdriver' was just created and is still being written to the database"
                      " — retry in a few seconds",
                      "created the test driver character 'Dcdriver' — it is logging in — retry in a few seconds"):
            self.assertEqual(dc.parse_start_reply(reply)[0], "pending", reply)

    def test_refused(self):
        for reply in ("Test run not started: unknown dungeon 'x' — see .dc test list",
                      "Test run not started: max concurrent test runs reached (1) — .dc test stop <run> first",
                      "DungeonClear is disabled"):
            self.assertEqual(dc.parse_start_reply(reply), ("refused", reply))

    def test_not_started_never_reads_as_started(self):
        reply = "Test run not started: tr-20261121-120102 is still tearing down"
        self.assertEqual(dc.parse_start_reply(reply)[0], "refused")


class StatusTest(unittest.TestCase):
    def test_run_active(self):
        status = "1 test run active (max 1):\n  tr-20261121-120102 uld-xt002 [Monitoring] elapsed 30s"
        self.assertTrue(dc.run_active(status, "tr-20261121-120102"))
        self.assertFalse(dc.run_active("no test runs active", "tr-20261121-120102"))

    def test_a_longer_id_is_another_run(self):
        status = "1 test run active (max 2):\n  tr-20261121-120102-12 uld-xt002 [Monitoring] elapsed 30s"
        self.assertFalse(dc.run_active(status, "tr-20261121-120102-1"))
        self.assertFalse(dc.run_active(status, "tr-20261121-120102"))
        self.assertTrue(dc.run_active(status, "tr-20261121-120102-12"))


def record(run_id: str, **extra) -> dict:
    base = {"schema": 15, "runId": run_id, "result": "success", "failReason": "", "startedAtMs": 1000,
            "endedAtMs": 9000,
            # the trash governor's log, which is not a boss pull list
            "pulls": [{"t": 3, "entry": 33113, "predicted": 1, "observed": 1, "advanced": True}]}
    base.update(extra)
    return base


class RecordTest(unittest.TestCase):
    def test_parse_skips_torn_and_blank_lines(self):
        text = "\n".join([json.dumps(record("tr-1")), "", '{"runId": "tr-2", "res', json.dumps(record("tr-3"))])
        self.assertEqual([r["runId"] for r in dc.parse_records(text)], ["tr-1", "tr-3"])

    def test_find_run_takes_the_last_record(self):
        records = [record("tr-1", result="wipe"), record("tr-1", result="success")]
        self.assertEqual(dc.find_run(records, "tr-1").result, "success")
        self.assertIsNone(dc.find_run(records, "tr-9"))

    def test_governor_log_is_not_a_pull_list(self):
        self.assertIsNone(dc.find_run([record("tr-1")], "tr-1").pulls)

    def test_pull_list(self):
        pulls = [{"startMs": 1000, "endMs": 4000, "outcome": "wipe"},
                 {"startMs": 5000, "endMs": 9000, "outcome": "kill"}]
        run = dc.find_run([record("tr-1", bossPulls=pulls)], "tr-1")
        self.assertEqual([(p.start_ms, p.end_ms, p.outcome) for p in run.pulls],
                         [(1000, 4000, "wipe"), (5000, 9000, "kill")])
        self.assertEqual(run.pulls[0].duration_ms, 3000)

    def test_pull_list_under_the_old_key_with_other_field_names(self):
        pulls = [{"startedAtMs": 1000, "endedAtMs": 4000, "result": "kill"}]
        run = dc.find_run([record("tr-1", pulls=pulls)], "tr-1")
        self.assertEqual(run.pulls[0].outcome, "kill")

    def test_pull_without_outcome(self):
        run = dc.run_of(record("tr-1", attempts=[{"start": 1, "end": 2}]))
        self.assertEqual(run.pulls[0].outcome, "?")


class StartRunTest(unittest.TestCase):
    def test_retries_while_the_driver_logs_in(self):
        clock = FakeClock()
        call, sent = scripted(["test driver 'Dcdriver' is logging in — retry in a few seconds", Down("refused"),
                               "Test run started: tr-20261121-120102 uld-xt002 [Setup]"])
        run_id = dc.start_run(call, "cmd", clock, clock.sleep, unavailable=(Down,))
        self.assertEqual(run_id, "tr-20261121-120102")
        self.assertEqual(len(sent), 3)

    def test_gives_up_after_two_minutes(self):
        clock = FakeClock()
        call, sent = scripted(["test driver 'Dcdriver' is logging in — retry in a few seconds"])
        with self.assertRaises(dc.DriverOffline):
            dc.start_run(call, "cmd", clock, clock.sleep)
        self.assertGreaterEqual(clock.now, 120.0)
        self.assertLess(clock.now, 130.0)

    def test_refusal_is_not_retried(self):
        clock = FakeClock()
        call, sent = scripted(["Test run not started: unknown dungeon 'x'"])
        with self.assertRaises(dc.StartRefused):
            dc.start_run(call, "cmd", clock, clock.sleep)
        self.assertEqual(len(sent), 1)

    def test_other_errors_propagate(self):
        clock = FakeClock()
        call, _ = scripted([ValueError("fault")])
        with self.assertRaises(ValueError):
            dc.start_run(call, "cmd", clock, clock.sleep, unavailable=(Down,))


class WaitRunTest(unittest.TestCase):
    ACTIVE = "1 test run active (max 1):\n  tr-1 uld-xt002 [Monitoring] elapsed 30s"

    def test_returns_the_record_once_the_run_leaves_status(self):
        clock = FakeClock()
        call, sent = scripted([self.ACTIVE, Down("blip"), self.ACTIVE, "no test runs active"])
        reads = ["", json.dumps(record("tr-1"))]
        run = dc.wait_run(call, "tr-1", lambda: reads.pop(0), clock, clock.sleep, unavailable=(Down,))
        self.assertEqual(run.run_id, "tr-1")
        self.assertEqual(sent, [dc.STATUS_COMMAND] * 4)

    def test_server_gone_for_too_long(self):
        clock = FakeClock()
        call, _ = scripted([Down("refused")])
        with self.assertRaises(dc.RunVanished):
            dc.wait_run(call, "tr-1", lambda: "", clock, clock.sleep, unavailable=(Down,))

    def test_run_left_status_without_a_record(self):
        clock = FakeClock()
        call, _ = scripted(["no test runs active"])
        with self.assertRaises(dc.RunVanished):
            dc.wait_run(call, "tr-1", lambda: json.dumps(record("tr-0")), clock, clock.sleep)


if __name__ == "__main__":
    unittest.main()
