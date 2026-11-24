"""Pairing traces with DC runs and pools: parallel runs told apart by instance, an instance id reused at
another time, and each pool counted and baselined only against itself.

    python -m unittest discover -s tools/raidtest/tests
"""
from __future__ import annotations

import datetime
import pathlib
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

import dc  # noqa: E402
from pools import Ledger, composition, pool_names, rows_for_run  # noqa: E402
from pulls import Corpus  # noqa: E402
from test_pulls import HEAD, isolate_cache, ms, write_trace  # noqa: E402


def at(minutes: float) -> datetime.datetime:
    return HEAD + datetime.timedelta(minutes=minutes)


def record(run_id: str, pool: str, instance: int, start: float, end: float, pulls=None, map_id: int = 603) -> dict:
    """A DC run record; start, end and the pull windows in minutes after HEAD. A pull's optional fourth
    field is DC's `start`, which records before schema 18 leave out."""
    out = {"runId": run_id, "pool": pool, "mapId": map_id, "instanceId": instance, "difficulty": "25n",
           "result": "success", "failReason": "", "startedAtMs": ms(at(start)), "endedAtMs": ms(at(end)),
           "bossPulls": []}
    if pulls is not None:
        out["bossPulls"] = [{"n": n, "startMs": ms(at(a)), "endMs": ms(at(b)) if b else 0, "outcome": outcome,
                             **({"start": kind[0]} if kind else {})}
                            for n, (a, b, outcome, *kind) in enumerate(pulls, 1)]
    return out


class PoolNamesTest(unittest.TestCase):
    def test_six_pools_per_raid(self):
        self.assertEqual(pool_names("uld"), ["uld1", "uld2", "uld3", "uld4", "uld5", "uld6"])
        self.assertEqual(pool_names("toc"), ["toc1", "toc2", "toc3", "toc4", "toc5", "toc6"])
        self.assertEqual(pool_names("uld", 2), ["uld1", "uld2"])

    def test_pool_n_copies_design_n_minus_one_mod_three(self):
        self.assertEqual([composition(pool) for pool in pool_names("uld")], list("ABCABC"))
        self.assertEqual([composition(pool) for pool in pool_names("toc")], list("ABCABC"))
        self.assertEqual((composition("uld0"), composition("lab"), composition("x9")), ("", "", "C"))


class TraceDir(unittest.TestCase):
    def setUp(self):
        folder = tempfile.TemporaryDirectory()
        self.addCleanup(folder.cleanup)
        self.dir = pathlib.Path(folder.name)
        isolate_cache(self)
        self.corpus = Corpus("xt-002", [self.dir])

    def trace(self, minutes: float, inst: int, built: float = 20, **extra) -> pathlib.Path:
        return write_trace(self.dir, at(minutes), at(built), inst=inst, **extra)


class PairingTest(TraceDir):
    def test_parallel_runs_get_their_own_instance_traces(self):
        # three pools pulling at once, their traces interleaved in time
        traces = {inst: [self.trace(31 + offset, inst), self.trace(36 + offset, inst)]
                  for inst, offset in ((7, 0.1), (8, 0.0), (9, 0.2))}
        runs = [dc.run_of(record(f"tr-{inst}", f"uld{index}", inst, 30, 40,
                                 [(31, 32, "wipe"), (36, 37.5, "kill")]))
                for index, inst in enumerate((7, 8, 9), 1)]
        for run, inst in zip(runs, (7, 8, 9)):
            rows = rows_for_run("test", run, self.corpus)
            self.assertEqual([row.trace.path for row in rows], traces[inst], inst)
            self.assertEqual([(row.pool, row.number, row.dc_outcome) for row in rows],
                             [(run.pool, 1, "wipe"), (run.pool, 2, "kill")])

    def test_a_reused_instance_id_belongs_to_the_run_of_its_time(self):
        old = self.trace(-600, 7, built=-700)
        new = self.trace(31, 7)
        earlier = dc.run_of(record("tr-old", "uld1", 7, -601, -590, [(-600, -599, "wipe")]))
        later = dc.run_of(record("tr-new", "uld2", 7, 30, 40, [(31, 32, "wipe")]))
        self.assertEqual([row.trace.path for row in rows_for_run("", earlier, self.corpus)], [old])
        self.assertEqual([row.trace.path for row in rows_for_run("", later, self.corpus)], [new])

    def test_a_pull_without_an_end_runs_to_the_end_of_the_run(self):
        path = self.trace(31, 7, length_ms=300_000)
        run = dc.run_of(record("tr-1", "uld1", 7, 30, 40, [(31, None, "stalled")]))
        rows = rows_for_run("", run, self.corpus)
        self.assertEqual(rows[0].window, (ms(at(31)), ms(at(40))))
        self.assertEqual(rows[0].trace.path, path)

    def test_a_pull_with_no_trace_still_gets_a_row(self):
        run = dc.run_of(record("tr-1", "uld1", 7, 30, 40, [(31, 32, "wipe")]))
        rows = rows_for_run("", run, self.corpus)
        self.assertEqual([(row.number, row.trace) for row in rows], [(1, None)])

    def test_record_without_a_pull_list_takes_its_instance_traces(self):
        first, second = self.trace(31, 7), self.trace(35, 7)
        self.trace(35, 8)
        rows = rows_for_run("", dc.run_of(record("tr-1", "uld1", 7, 30, 40)), self.corpus)
        self.assertEqual([row.trace.path for row in rows], [first, second])

    def test_each_row_says_whether_its_own_pull_engaged_first(self):
        first, second = self.trace(31, 7), self.trace(36, 7)
        run = dc.run_of(record("tr-1", "uld1", 7, 30, 40, [(31, 32, "wipe", "step"), (36, 37, "kill", "engaged")]))
        rows = rows_for_run("", run, self.corpus)
        self.assertEqual([(row.trace.path, row.engaged_first) for row in rows], [(first, False), (second, True)])

    def test_setup_failure_has_no_rows(self):
        self.trace(31, 7)
        self.assertEqual(rows_for_run("", dc.run_of(record("tr-1", "uld1", 0, 30, 40)), self.corpus), [])


class LedgerTest(TraceDir):
    def ledger(self, records: list[dict], pools=("uld1", "uld2", "uld3")) -> Ledger:
        ledger = Ledger(self.corpus, list(pools), "25n")
        ledger.load(records)
        return ledger

    def test_baseline_pairs_per_pool(self):
        older = self.trace(-200, 7, built=-400)
        newer = self.trace(-100, 8, built=-400)
        self.trace(-50, 9, built=-400, human=True)
        records = [record("tr-a", "uld1", 7, -201, -190, [(-200, -199, "wipe")]),
                   record("tr-b", "uld1", 8, -101, -90, [(-100, -99, "kill")]),
                   record("tr-c", "uld2", 9, -51, -40, [(-50, -49, "kill")])]
        ledger = self.ledger(records)
        self.assertEqual([f.path for f in ledger.baseline("uld1", HEAD, 3)], [newer, older])
        self.assertEqual([f.path for f in ledger.baseline("uld1", HEAD, 1)], [newer])
        self.assertEqual(ledger.baseline("uld2", HEAD, 3), [])
        self.assertEqual(ledger.baseline("uld3", HEAD, 3), [])

    def test_counted_per_pool_after_head_at_the_difficulty(self):
        good = self.trace(31, 7)
        self.trace(31.2, 8, built=-20)          # binary predates HEAD
        self.trace(31.4, 9, diff=0)             # 10-man
        other = self.trace(31.1, 10)
        records = [record("tr-1", "uld1", 7, 30, 40, [(31, 32, "kill")]),
                   record("tr-2", "uld2", 8, 30, 40, [(31.2, 32, "kill")]),
                   record("tr-3", "uld3", 9, 30, 40, [(31.4, 32, "kill")]),
                   record("tr-4", "toc1", 10, 30, 40, [(31.1, 32, "kill")])]
        ledger = self.ledger(records)
        self.assertEqual([f.path for f in ledger.counted("uld1", HEAD)], [good])
        self.assertEqual(ledger.counted("uld2", HEAD), [])
        self.assertEqual(ledger.counted("uld3", HEAD), [])
        self.assertNotIn(other, [row.trace.path for row in ledger.rows if row.trace])

    def test_a_pull_whose_boss_engaged_first_counts_nowhere(self):
        refreshed, unrefreshed, unsaid = self.trace(31, 7), self.trace(36, 7), self.trace(41, 7)
        base = self.trace(-100, 8, built=-400)
        records = [record("tr-1", "uld1", 7, 30, 45, [(31, 32, "wipe", "step"), (36, 37, "wipe", "engaged"),
                                                      (41, 42, "kill")]),
                   record("tr-0", "uld1", 8, -101, -90, [(-100, -99, "kill", "engaged")])]
        ledger = self.ledger(records)
        self.assertEqual([f.path for f in ledger.counted("uld1", HEAD)], [unsaid, refreshed])
        self.assertEqual(ledger.baseline("uld1", HEAD, 3), [])
        self.assertEqual({row.trace.path for row in ledger.rows if row.trace},
                         {refreshed, unrefreshed, unsaid, base})

    def test_a_trace_outside_every_pool_run_counts_for_none(self):
        self.trace(31, 7)
        self.assertEqual(self.ledger([]).counted("uld1", HEAD), [])

    def test_rows_of(self):
        first, second = self.trace(31, 7), self.trace(36, 7)
        ledger = self.ledger([record("tr-1", "uld1", 7, 30, 40, [(31, 32, "wipe"), (36, 37, "kill")])])
        rows = ledger.rows_of(ledger.counted("uld1", HEAD))
        self.assertEqual([(row.run_id, row.number, row.trace.path) for row in rows],
                         [("tr-1", 2, second), ("tr-1", 1, first)])


if __name__ == "__main__":
    unittest.main()
