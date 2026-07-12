#!/usr/bin/env python3

import json
import os
import re
import signal
import subprocess
import sys
import threading
import time
from dataclasses import dataclass
from typing import BinaryIO, Dict, List, Optional, Sequence


EVENT_PREFIX = "GAIA_TASK_EVENT "
TASK_ID_PATTERN = re.compile(r"^[a-z0-9][a-z0-9-]*$")
MAX_TASK_OUTPUT_BYTES = 1024 * 1024
TERMINATE_TIMEOUT_SECONDS = 2.0


class UsageError(ValueError):
    pass


@dataclass(frozen=True)
class Task:
    task_id: str
    label: str
    command: List[str]


@dataclass
class ActiveTask:
    task: Task
    process: subprocess.Popen
    output: "BoundedOutput"
    reader: threading.Thread
    started_at: float
    group_termination_deadline: Optional[float] = None
    group_kill_sent: bool = False


class BoundedOutput:
    def __init__(self, limit: int) -> None:
        self.data = bytearray()
        self.limit = limit
        self.truncated = False

    def append(self, chunk: bytes) -> None:
        if len(chunk) >= self.limit:
            self.data = bytearray(chunk[-self.limit:])
            self.truncated = True
            return
        overflow = len(self.data) + len(chunk) - self.limit
        if overflow > 0:
            del self.data[:overflow]
            self.truncated = True
        self.data.extend(chunk)


def parse_args(argv: Sequence[str]) -> tuple:
    if len(argv) < 2 or argv[0] != "--jobs":
        raise UsageError("--jobs is required")
    try:
        jobs = int(argv[1])
    except ValueError as exc:
        raise UsageError("jobs must be an integer") from exc
    if jobs < 1 or jobs > 64:
        raise UsageError("jobs must be between 1 and 64")

    tasks: List[Task] = []
    index = 2
    while index < len(argv):
        if argv[index] != "--task" or index + 3 >= len(argv):
            raise UsageError("each task requires: --task ID LABEL COMMAND [ARG ...]")
        task_id = argv[index + 1]
        label = argv[index + 2]
        index += 3
        command_start = index
        while index < len(argv) and argv[index] != "--task":
            index += 1
        command = list(argv[command_start:index])
        if not TASK_ID_PATTERN.fullmatch(task_id):
            raise UsageError(f"invalid task id: {task_id}")
        if not label:
            raise UsageError(f"task label is empty: {task_id}")
        if not command:
            raise UsageError(f"task command is empty: {task_id}")
        if any(task.task_id == task_id for task in tasks):
            raise UsageError(f"duplicate task id: {task_id}")
        tasks.append(Task(task_id, label, command))

    if not tasks:
        raise UsageError("at least one task is required")
    return jobs, tasks


def emit_event(task: Task, state: str, duration_ms: Optional[int] = None, exit_code: Optional[int] = None) -> None:
    event: Dict[str, object] = {"id": task.task_id, "label": task.label, "state": state}
    if duration_ms is not None:
        event["durationMs"] = duration_ms
    if exit_code is not None:
        event["exitCode"] = exit_code
    print(f"{EVENT_PREFIX}{json.dumps(event, separators=(',', ':'), sort_keys=True)}", flush=True)


def signal_process(process: subprocess.Popen, signum: int) -> None:
    try:
        if os.name == "posix":
            os.killpg(process.pid, signum)
        elif process.poll() is not None:
            return
        elif signum == signal.SIGTERM:
            process.terminate()
        else:
            process.kill()
    except ProcessLookupError:
        pass


def start_task(task: Task) -> ActiveTask:
    process = subprocess.Popen(
        task.command,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        start_new_session=os.name == "posix",
    )
    assert process.stdout is not None
    output = BoundedOutput(MAX_TASK_OUTPUT_BYTES)
    reader = threading.Thread(target=_capture_output, args=(process.stdout, output), daemon=True)
    reader.start()
    emit_event(task, "running")
    return ActiveTask(task, process, output, reader, time.monotonic())


def _capture_output(stream: BinaryIO, output: BoundedOutput) -> None:
    try:
        while True:
            chunk = stream.read(65536)
            if not chunk:
                return
            output.append(chunk)
    finally:
        stream.close()


def print_output(active: ActiveTask) -> None:
    active.reader.join(timeout=TERMINATE_TIMEOUT_SECONDS)
    if active.reader.is_alive():
        print(f"warning: output pipe remained open for {active.task.label}", file=sys.stderr, flush=True)
        return
    print(f"--- {active.task.label} [{active.task.task_id}] ---")
    if active.output.truncated:
        print(f"[output truncated to last {MAX_TASK_OUTPUT_BYTES} bytes]")
    sys.stdout.flush()
    sys.stdout.buffer.write(active.output.data)
    if active.output.data and not active.output.data.endswith(b"\n"):
        print()
    sys.stdout.flush()


def run_tasks(jobs: int, tasks: Sequence[Task]) -> int:
    pending = list(tasks)
    active: Dict[str, ActiveTask] = {}
    failed = False
    interrupted_signal = 0
    termination_deadline: Optional[float] = None

    def interrupt(signum: int, _frame: object) -> None:
        nonlocal interrupted_signal, termination_deadline
        if interrupted_signal:
            for item in list(active.values()):
                signal_process(item.process, signal.SIGKILL)
            return
        interrupted_signal = signum
        termination_deadline = time.monotonic() + TERMINATE_TIMEOUT_SECONDS
        for item in list(active.values()):
            signal_process(item.process, signal.SIGTERM)

    previous_sigterm = signal.signal(signal.SIGTERM, interrupt)
    previous_sigint = signal.signal(signal.SIGINT, interrupt)
    try:
        while pending or active:
            while pending and len(active) < jobs and interrupted_signal == 0:
                task = pending.pop(0)
                try:
                    active[task.task_id] = start_task(task)
                except OSError as exc:
                    print(f"error: failed to start {task.label}: {exc}", file=sys.stderr, flush=True)
                    emit_event(task, "failed", 0, 127)
                    failed = True

            now = time.monotonic()
            for item in active.values():
                if item.process.poll() is not None and item.reader.is_alive() and item.group_termination_deadline is None and not item.group_kill_sent:
                    signal_process(item.process, signal.SIGTERM)
                    item.group_termination_deadline = now + TERMINATE_TIMEOUT_SECONDS
                if item.group_termination_deadline is not None and now >= item.group_termination_deadline:
                    signal_process(item.process, signal.SIGKILL)
                    item.group_termination_deadline = None
                    item.group_kill_sent = True

            completed = [
                item for item in active.values()
                if item.process.poll() is not None and not item.reader.is_alive()
            ]
            if not completed:
                if termination_deadline is not None and time.monotonic() >= termination_deadline:
                    for item in list(active.values()):
                        signal_process(item.process, signal.SIGKILL)
                    termination_deadline = None
                if interrupted_signal and not active:
                    break
                time.sleep(0.01)
                continue

            for item in completed:
                exit_code = item.process.returncode
                assert exit_code is not None
                duration_ms = int((time.monotonic() - item.started_at) * 1000)
                print_output(item)
                state = "cancelled" if interrupted_signal else ("passed" if exit_code == 0 else "failed")
                emit_event(item.task, state, duration_ms, exit_code)
                failed = failed or exit_code != 0
                del active[item.task.task_id]
    finally:
        for item in list(active.values()):
            signal_process(item.process, signal.SIGTERM)
        deadline = time.monotonic() + TERMINATE_TIMEOUT_SECONDS
        while any(item.process.poll() is None or item.reader.is_alive() for item in active.values()) and time.monotonic() < deadline:
            time.sleep(0.01)
        for item in list(active.values()):
            if item.process.poll() is None or item.reader.is_alive():
                signal_process(item.process, signal.SIGKILL)
        for item in list(active.values()):
            item.process.wait()
            print_output(item)
        signal.signal(signal.SIGTERM, previous_sigterm)
        signal.signal(signal.SIGINT, previous_sigint)

    if interrupted_signal:
        return 128 + interrupted_signal
    return 1 if failed else 0


def main(argv: Optional[Sequence[str]] = None) -> int:
    try:
        jobs, tasks = parse_args(list(sys.argv[1:] if argv is None else argv))
    except UsageError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    return run_tasks(jobs, tasks)


if __name__ == "__main__":
    sys.exit(main())
