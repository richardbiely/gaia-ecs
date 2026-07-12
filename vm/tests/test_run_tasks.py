import json
import os
import signal
import subprocess
import sys
import tempfile
import time
import unittest
from pathlib import Path


RUNNER = Path(__file__).parents[1] / "run_tasks.py"
EVENT_PREFIX = "GAIA_TASK_EVENT "


def task_args(task_id: str, label: str, code: str, *args: str) -> list[str]:
    return ["--task", task_id, label, sys.executable, "-c", code, *args]


def events(output: str) -> list[dict[str, object]]:
    return [
        json.loads(line.removeprefix(EVENT_PREFIX))
        for line in output.splitlines()
        if line.startswith(EVENT_PREFIX)
    ]


class ParallelTaskRunnerTests(unittest.TestCase):
    def run_tasks(self, jobs: int, *tasks: list[str]) -> subprocess.CompletedProcess[str]:
        command = [sys.executable, str(RUNNER), "--jobs", str(jobs)]
        for task in tasks:
            command.extend(task)
        return subprocess.run(command, check=False, capture_output=True, text=True, timeout=10)

    def test_bounds_concurrency_and_groups_child_output(self) -> None:
        code = "import sys,time; print('begin-' + sys.argv[1], flush=True); time.sleep(0.1); print('end-' + sys.argv[1])"
        result = self.run_tasks(2, *(
            task_args(f"task-{index}", f"Task {index}", code, str(index))
            for index in range(4)
        ))

        self.assertEqual(0, result.returncode, result.stderr)
        active = 0
        maximum_active = 0
        for event in events(result.stdout):
            active += 1 if event["state"] == "running" else -1
            maximum_active = max(maximum_active, active)
        self.assertEqual(2, maximum_active)
        self.assertEqual(0, active)
        for index in range(4):
            begin = result.stdout.index(f"begin-{index}")
            end = result.stdout.index(f"end-{index}")
            self.assertLess(begin, end)
            self.assertNotIn("begin-", result.stdout[begin + len(f"begin-{index}"):end])

    def test_waits_for_every_task_and_fails_the_aggregate(self) -> None:
        result = self.run_tasks(
            2,
            task_args("failure", "Failure", "import sys; print('failed output'); sys.exit(3)"),
            task_args("success", "Success", "print('successful output')"),
        )

        self.assertEqual(1, result.returncode)
        completed = {event["id"]: event for event in events(result.stdout) if event["state"] in ("passed", "failed")}
        self.assertEqual(3, completed["failure"]["exitCode"])
        self.assertEqual(0, completed["success"]["exitCode"])
        self.assertIn("failed output", result.stdout)
        self.assertIn("successful output", result.stdout)

    def test_bounds_retained_output_without_losing_the_end(self) -> None:
        code = "import sys; sys.stdout.write('BEGIN-' + 'x' * (2 * 1024 * 1024) + '-END\\n')"

        result = self.run_tasks(1, task_args("noisy", "Noisy", code))

        self.assertEqual(0, result.returncode, result.stderr)
        self.assertLess(len(result.stdout.encode()), 1024 * 1024 + 4096)
        self.assertNotIn("BEGIN-", result.stdout)
        self.assertIn("output truncated", result.stdout)
        self.assertIn("-END", result.stdout)

    def test_rejects_invalid_job_counts_and_task_ids(self) -> None:
        invalid_jobs = self.run_tasks(0, task_args("valid", "Valid", "pass"))
        invalid_id = self.run_tasks(1, task_args("Invalid id", "Invalid", "pass"))

        self.assertNotEqual(0, invalid_jobs.returncode)
        self.assertIn("jobs", invalid_jobs.stderr)
        self.assertNotEqual(0, invalid_id.returncode)
        self.assertIn("task id", invalid_id.stderr)

    def test_termination_stops_active_children_without_starting_queued_tasks(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            started = root / "started"
            queued = root / "queued"
            child_pid = root / "child.pid"
            long_task = task_args(
                "active",
                "Active",
                "import os,sys,time; from pathlib import Path; Path(sys.argv[1]).write_text('yes'); Path(sys.argv[2]).write_text(str(os.getpid())); time.sleep(30)",
                str(started),
                str(child_pid),
            )
            queued_task = task_args("queued", "Queued", "import sys; from pathlib import Path; Path(sys.argv[1]).write_text('yes')", str(queued))
            process = subprocess.Popen(
                [sys.executable, str(RUNNER), "--jobs", "1", *long_task, *queued_task],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
            )
            for _ in range(100):
                if started.exists():
                    break
                time.sleep(0.02)
            self.assertTrue(started.exists())

            process.send_signal(signal.SIGTERM)
            process.communicate(timeout=5)

            self.assertNotEqual(0, process.returncode)
            self.assertFalse(queued.exists())
            pid = int(child_pid.read_text())
            with self.assertRaises(ProcessLookupError):
                os.kill(pid, 0)

    def test_termination_escalates_for_a_process_group_ignoring_sigterm(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            child_pid = Path(directory) / "child.pid"
            task = task_args(
                "stubborn",
                "Stubborn",
                "import os,signal,sys,time; from pathlib import Path; signal.signal(signal.SIGTERM, signal.SIG_IGN); Path(sys.argv[1]).write_text(str(os.getpid())); time.sleep(30)",
                str(child_pid),
            )
            process = subprocess.Popen(
                [sys.executable, str(RUNNER), "--jobs", "1", *task],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
            )
            for _ in range(100):
                if child_pid.exists():
                    break
                time.sleep(0.02)
            self.assertTrue(child_pid.exists())

            process.send_signal(signal.SIGTERM)
            process.communicate(timeout=5)

            self.assertNotEqual(0, process.returncode)
            pid = int(child_pid.read_text())
            with self.assertRaises(ProcessLookupError):
                os.kill(pid, 0)

    def test_termination_escalates_when_the_group_leader_exits_first(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            descendant_pid = Path(directory) / "descendant.pid"
            descendant_code = (
                "import os,signal,sys,time; from pathlib import Path; "
                "signal.signal(signal.SIGTERM, signal.SIG_IGN); "
                "Path(sys.argv[1]).write_text(str(os.getpid())); print('descendant-ready', flush=True); time.sleep(30)"
            )
            leader_code = (
                "import subprocess,sys,time; "
                "subprocess.Popen([sys.executable, '-c', sys.argv[1], sys.argv[2]], stdout=sys.stdout, stderr=sys.stderr); "
                "time.sleep(30)"
            )
            task = task_args("tree", "Tree", leader_code, descendant_code, str(descendant_pid))
            process = subprocess.Popen(
                [sys.executable, str(RUNNER), "--jobs", "1", *task],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
            )
            for _ in range(100):
                if descendant_pid.exists():
                    break
                time.sleep(0.02)
            self.assertTrue(descendant_pid.exists())

            process.send_signal(signal.SIGTERM)
            process.communicate(timeout=5)

            self.assertNotEqual(0, process.returncode)
            pid = int(descendant_pid.read_text())
            with self.assertRaises(ProcessLookupError):
                os.kill(pid, 0)


if __name__ == "__main__":
    unittest.main()
