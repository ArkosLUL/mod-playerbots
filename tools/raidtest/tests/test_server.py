"""The server lock, the dirty-tree guard, and the docker steps with subprocess stood in for.

    python -m unittest discover -s tools/raidtest/tests
"""
from __future__ import annotations

import pathlib
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

import server  # noqa: E402


def done(returncode: int = 0, stdout: str = "", stderr: str = "") -> subprocess.CompletedProcess:
    return subprocess.CompletedProcess([], returncode, stdout, stderr)


class FakeRun:
    """Answers each command by the first rule whose key is a sub-sequence of its arguments."""

    def __init__(self, rules=None, default=None):
        self.rules = rules or []
        self.default = default or done()
        self.calls = []

    def __call__(self, command, **kwargs):
        self.calls.append((command, kwargs))
        for key, answer in self.rules:
            if all(part in command for part in key):
                return answer(command, kwargs) if callable(answer) else answer
        return self.default


class TempDir(unittest.TestCase):
    def setUp(self):
        folder = tempfile.TemporaryDirectory()
        self.addCleanup(folder.cleanup)
        self.dir = pathlib.Path(folder.name)


class LockTest(TempDir):
    def test_acquire_writes_owner_and_release_removes(self):
        path = self.dir / "locks" / "server"
        lock = server.ServerLock(path, "cycle xt-002")
        lock.acquire()
        self.assertEqual((path / "owner").read_text(encoding="utf-8").strip(), "cycle xt-002")
        lock.release()
        self.assertFalse(path.exists())

    def test_second_taker_is_refused_with_the_owner(self):
        path = self.dir / "server"
        first = server.ServerLock(path, "orchestrator wave 3")
        first.acquire()
        with self.assertRaises(server.LockHeld) as caught:
            server.ServerLock(path, "someone else").acquire()
        self.assertIn("orchestrator wave 3", str(caught.exception))
        self.assertTrue(path.exists())
        first.release()

    def test_refused_taker_never_removes_the_lock(self):
        path = self.dir / "server"
        server.ServerLock(path, "first").acquire()
        second = server.ServerLock(path, "second")
        with self.assertRaises(server.LockHeld):
            second.acquire()
        second.release()
        self.assertTrue(path.exists())

    def test_context_releases_on_error(self):
        path = self.dir / "server"
        with self.assertRaises(RuntimeError):
            with server.ServerLock(path, "x"):
                raise RuntimeError("boom")
        self.assertFalse(path.exists())

    def test_someone_elses_lock_is_left_alone(self):
        path = self.dir / "server"
        lock = server.ServerLock(path, "mine")
        lock.acquire()
        (path / "owner").write_text("theirs\n", encoding="utf-8")
        lock.release()
        self.assertTrue(path.exists())

    def test_failed_owner_write_leaves_no_lock(self):
        path = self.dir / "server"
        lock = server.ServerLock(path, "x")
        with mock.patch.object(pathlib.Path, "write_text", side_effect=OSError("disk full")), \
                self.assertRaises(OSError):
            lock.acquire()
        self.assertFalse(path.exists())
        self.assertFalse(lock.held)

    def test_owner_text(self):
        self.assertIn("raidtest cycle", server.owner_text("raidtest cycle"))


class DirtyTreeTest(TempDir):
    def setUp(self):
        super().setUp()
        (self.dir / ".git").mkdir()
        modules = self.dir / "modules"
        for name, marker in (("mod-a", "dir"), ("mod-b", "file"), ("mod-c", None)):
            (modules / name).mkdir(parents=True)
            if marker == "dir":
                (modules / name / ".git").mkdir()
            elif marker == "file":
                (modules / name / ".git").write_text("gitdir: elsewhere\n", encoding="utf-8")

    def test_repos_are_the_core_and_module_checkouts(self):
        names = [repo.name for repo in server.git_repos(self.dir)]
        self.assertEqual(names, [self.dir.name, "mod-a", "mod-b"])

    def test_clean(self):
        run = FakeRun()
        self.assertEqual(server.dirty_trees(self.dir, run), [])
        repos = {command[2] for command, _ in run.calls}
        self.assertEqual(len(repos), 3)
        for command, kwargs in run.calls:
            self.assertIn("GIT_CEILING_DIRECTORIES", kwargs["env"])

    def test_modified_tracked_and_untracked_src(self):
        mod_a = str(self.dir / "modules" / "mod-a")
        run = FakeRun([
            ((str(self.dir), "status"), done(stdout=" M src/server/game/World/World.cpp\n")),
            ((mod_a, "ls-files"), done(stdout="src/NewFile.cpp\n")),
        ])
        problems = server.dirty_trees(self.dir, run)
        self.assertEqual(problems, ["core: modified src/server/game/World/World.cpp",
                                    "modules/mod-a: untracked src/NewFile.cpp"])

    def test_untracked_is_asked_only_under_src(self):
        run = FakeRun()
        server.dirty_trees(self.dir, run)
        ls = [command for command, _ in run.calls if "ls-files" in command]
        self.assertTrue(ls)
        for command in ls:
            self.assertEqual(command[-2:], ["--", "src"])
            self.assertIn("--exclude-standard", command)

    def test_git_failure_refuses(self):
        run = FakeRun([(("status",), done(128, stderr="fatal: not a git repository"))])
        problems = server.dirty_trees(self.dir, run)
        self.assertEqual(len(problems), 3)
        self.assertIn("not a git repository", problems[0])


class DockerTest(TempDir):
    def test_build_is_judged_by_compose_exit_code(self):
        def compose(command, kwargs):
            kwargs["stdout"].write(b"#12 ERROR: process did not complete successfully\n")
            return done(17)

        run = FakeRun([(("build",), compose)])
        log = self.dir / "logs" / "build.log"
        self.assertEqual(server.build(self.dir, log, run), 17)
        self.assertIn(b"ERROR", log.read_bytes())
        command, kwargs = run.calls[0]
        self.assertEqual(command[:2], ["docker", "compose"])
        self.assertEqual(command.count("-f"), 3)
        self.assertIn(".agents/plans/worldserver-build/compose.build.yml", command)
        self.assertEqual(command[-4:], ["build", "--progress", "plain", "ac-worldserver"])
        self.assertEqual(kwargs["cwd"], str(self.dir))
        self.assertIs(kwargs["stderr"], subprocess.STDOUT)

    def test_restart_reports_a_recreated_container(self):
        ids = iter(["old\n", "new\n"])
        run = FakeRun([(("inspect",), lambda c, k: done(stdout=next(ids)))])
        code, since, recreated = server.restart(self.dir, run, now=lambda: 1_700_000_005.0)
        self.assertEqual((code, recreated), (0, True))
        self.assertEqual(since, "2023-11-14T22:13:20Z")
        up = [command for command, _ in run.calls if "up" in command][0]
        self.assertEqual(up[-4:], ["up", "-d", "--no-deps", "ac-worldserver"])

    def test_restart_same_container(self):
        run = FakeRun([(("inspect",), done(stdout="same 2026-11-21T10:00:00Z\n"))])
        self.assertFalse(server.restart(self.dir, run)[2])

    def test_restart_of_a_stopped_container_is_a_start(self):
        ids = iter(["same 2026-11-21T10:00:00Z\n", "same 2026-11-21T13:00:00Z\n"])
        run = FakeRun([(("inspect",), lambda c, k: done(stdout=next(ids)))])
        self.assertTrue(server.restart(self.dir, run)[2])
        inspect = [command for command, _ in run.calls if "inspect" in command][0]
        self.assertIn("{{.State.StartedAt}}", inspect[-2])

    def test_wait_ready_polls_until_the_line(self):
        logs = iter(["starting\n", "loading maps\n", "AzerothCore rev. (worldserver-daemon) ready...\n"])
        run = FakeRun([(("logs",), lambda c, k: done(stdout=next(logs)))])
        clock = [0.0]
        ok = server.wait_ready("2026-11-21T12:00:00Z", 600, run, lambda: clock[0],
                               lambda s: clock.__setitem__(0, clock[0] + s))
        self.assertTrue(ok)
        self.assertEqual(len(run.calls), 3)
        self.assertEqual(run.calls[0][0], ["docker", "logs", "--since", "2026-11-21T12:00:00Z", "ac-worldserver"])

    def test_wait_ready_times_out(self):
        run = FakeRun([(("logs",), done(stdout="still loading\n"))])
        clock = [0.0]
        self.assertFalse(server.wait_ready("x", 60, run, lambda: clock[0],
                                           lambda s: clock.__setitem__(0, clock[0] + s)))

    def test_read_file(self):
        run = FakeRun([(("exec",), done(stdout='{"runId":"tr-1"}\n'))])
        self.assertEqual(server.read_file("/azerothcore/dc_testruns.jsonl", run), '{"runId":"tr-1"}\n')
        self.assertEqual(run.calls[0][0],
                         ["docker", "exec", "ac-worldserver", "cat", "/azerothcore/dc_testruns.jsonl"])

    def test_read_missing_file_is_empty(self):
        run = FakeRun([(("exec",), done(1, stderr="cat: /x: No such file or directory"))])
        self.assertEqual(server.read_file("/x", run), "")

    def test_read_from_a_stopped_container(self):
        run = FakeRun([(("exec",), done(1, stderr="Error response from daemon: container is not running"))])
        with self.assertRaises(server.ContainerDown):
            server.read_file("/x", run)


if __name__ == "__main__":
    unittest.main()
