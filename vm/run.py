#!/usr/bin/env python3

import argparse
import json
import os
import platform
import shlex
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import List, Optional, Sequence


REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_CONFIG = Path.home() / ".config" / "gaia-ecs" / "targets.json"
DEFAULT_IMAGE = "gaiaecs-linux-builder"


class ConfigError(ValueError):
    pass


@dataclass(frozen=True)
class Target:
    name: str
    transport: str
    ssh_host: Optional[str]
    runtime: str
    workspace_root: str
    image: str


def load_target(path: Path, name: str) -> Target:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError as exc:
        raise ConfigError(f"target configuration not found: {path}") from exc
    except json.JSONDecodeError as exc:
        raise ConfigError(f"invalid target configuration {path}: {exc}") from exc

    try:
        values = data["targets"][name]
    except (KeyError, TypeError) as exc:
        raise ConfigError(f'target "{name}" is not defined in {path}') from exc

    transport = values.get("transport")
    runtime = values.get("runtime")
    ssh_host = values.get("ssh_host")
    workspace_root = values.get("workspace_root", "/tmp/gaia-ecs-runs")
    image = values.get("image", DEFAULT_IMAGE)

    if transport not in ("local", "ssh"):
        raise ConfigError(f'target "{name}" transport must be local or ssh')
    if runtime not in ("docker", "podman"):
        raise ConfigError(f'target "{name}" runtime must be docker or podman')
    if transport == "ssh" and not ssh_host:
        raise ConfigError(f'target "{name}" requires ssh_host')
    if transport == "local" and ssh_host:
        raise ConfigError(f'target "{name}" must not set ssh_host for local transport')
    if not isinstance(workspace_root, str) or not workspace_root.startswith("/"):
        raise ConfigError(f'target "{name}" workspace_root must be an absolute path')
    if not isinstance(image, str) or not image:
        raise ConfigError(f'target "{name}" image must be a non-empty string')

    return Target(name, transport, ssh_host, runtime, workspace_root, image)


def runtime_probe_command(target: Target) -> List[str]:
    return [target.runtime, "info"]


def host_command(target: Target, command: Sequence[str], interactive: bool = False) -> List[str]:
    if target.transport == "local":
        return list(command)
    assert target.ssh_host is not None
    result = ["ssh", "-o", "BatchMode=yes"]
    if interactive:
        result.append("-tt")
    result.extend(("--", target.ssh_host, shlex.join(command)))
    return result


def container_command(
    target: Target,
    workspace: str,
    command: Sequence[str],
    run_id: str = "run",
    user: Optional[str] = None,
    interactive: bool = False,
    workdir: str = "/gaia-ecs",
) -> List[str]:
    result = [
        target.runtime,
        "run",
        "--rm",
        "--pull=never",
        "--name",
        f"gaiaecs-{run_id}",
        "--volume",
        f"{workspace}:/gaia-ecs",
    ]
    if interactive:
        result.extend(("--interactive", "--tty"))
    result.extend(("--workdir", workdir))
    if user is not None:
        result.extend(("--user", user))
    result.extend((target.image, *command))
    return result


def run_checked(command: Sequence[str], description: str) -> None:
    try:
        result = subprocess.run(command, check=False)
    except FileNotFoundError as exc:
        raise RuntimeError(f"{description}: executable not found: {command[0]}") from exc
    if result.returncode != 0:
        raise RuntimeError(f"{description} failed with exit code {result.returncode}")


def capture_checked(command: Sequence[str], description: str) -> str:
    try:
        result = subprocess.run(command, check=False, stdout=subprocess.PIPE, text=True)
    except FileNotFoundError as exc:
        raise RuntimeError(f"{description}: executable not found: {command[0]}") from exc
    if result.returncode != 0:
        raise RuntimeError(f"{description} failed with exit code {result.returncode}")
    return result.stdout.strip()


def probe_runtime(target: Target) -> None:
    try:
        capture_checked(host_command(target, runtime_probe_command(target)), f'{target.runtime} on target "{target.name}"')
    except RuntimeError as exc:
        raise RuntimeError(
            f'target "{target.name}" requires accessible {target.runtime}; '
            "the runner will not install, configure, or replace container runtimes"
        ) from exc


def remote_workspace(target: Target, run_id: str) -> str:
    return f"{target.workspace_root.rstrip('/')}/{run_id}/src"


def stage_remote_workspace(target: Target, workspace: str) -> None:
    assert target.ssh_host is not None
    run_checked(host_command(target, ["mkdir", "-p", workspace]), "remote workspace creation")
    source = f"{REPO_ROOT}/"
    destination = f"{target.ssh_host}:{workspace}/"
    command = [
        "rsync",
        "-az",
        "--delete",
        "--exclude=.git/",
        "--exclude=build*/",
        "--exclude=vm/build*/",
        "--exclude=.cache/",
        "--exclude=__pycache__/",
        source,
        destination,
    ]
    run_checked(command, "remote source synchronization")


def remove_remote_run(target: Target, run_id: str) -> None:
    run_root = f"{target.workspace_root.rstrip('/')}/{run_id}"
    run_checked(host_command(target, ["rm", "-rf", run_root]), "remote workspace cleanup")


def host_architecture(target: Target) -> str:
    if target.transport == "local":
        return platform.machine()
    return capture_checked(host_command(target, ["uname", "-m"]), "target architecture detection")


def local_host_identity(os_name: str, user_id: Optional[int], group_id: Optional[int]) -> Optional[str]:
    if os_name == "nt" or user_id is None or group_id is None:
        return None
    return f"{user_id}:{group_id}"


def host_identity(target: Target) -> Optional[str]:
    if target.transport == "local":
        getuid = getattr(os, "getuid", lambda: None)
        getgid = getattr(os, "getgid", lambda: None)
        return local_host_identity(os.name, getuid(), getgid())
    user_id = capture_checked(host_command(target, ["id", "-u"]), "target user detection")
    group_id = capture_checked(host_command(target, ["id", "-g"]), "target group detection")
    return f"{user_id}:{group_id}"


def build_image(target: Target, workspace: str) -> None:
    architecture = host_architecture(target)
    dockerfile = "amd64.Dockerfile" if architecture in ("x86_64", "amd64") else "Dockerfile"
    command = [
        target.runtime,
        "build",
        "--file",
        f"{workspace}/vm/{dockerfile}",
        "--tag",
        target.image,
        f"{workspace}/vm",
    ]
    run_checked(host_command(target, command), "container image build")


def execute(
    target: Target,
    command: Sequence[str],
    rebuild_image: bool,
    keep_workspace: bool,
    interactive: bool,
    workdir: str,
) -> None:
    probe_runtime(target)
    run_id = f"{int(time.time())}-{os.getpid()}"
    workspace = str(REPO_ROOT)
    staged = target.transport == "ssh"

    if staged:
        workspace = remote_workspace(target, run_id)
        stage_remote_workspace(target, workspace)

    succeeded = False
    try:
        if rebuild_image:
            build_image(target, workspace)
        user = host_identity(target)
        run_checked(
            host_command(
                target,
                container_command(target, workspace, command, run_id, user, interactive, workdir),
                interactive,
            ),
            f'container command on target "{target.name}"',
        )
        succeeded = True
    finally:
        if staged and succeeded and not keep_workspace:
            remove_remote_run(target, run_id)
        elif staged:
            print(f"Remote workspace retained at {target.ssh_host}:{workspace}", file=sys.stderr)


def parse_args(argv: Optional[Sequence[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run Gaia-ECS commands in a local or remote container")
    parser.add_argument("--config", type=Path, default=Path(os.environ.get("GAIA_ECS_TARGETS", DEFAULT_CONFIG)))
    parser.add_argument("--target", required=True)
    parser.add_argument("--build-image", action="store_true")
    parser.add_argument("--keep-workspace", action="store_true")
    parser.add_argument("--interactive", action="store_true")
    parser.add_argument("--container-workdir", default="/gaia-ecs")
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args(argv)
    if args.command[:1] == ["--"]:
        args.command = args.command[1:]
    if not args.command:
        parser.error("a container command is required after --")
    return args


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = parse_args(argv)
    try:
        target = load_target(args.config, args.target)
        execute(
            target,
            args.command,
            args.build_image,
            args.keep_workspace,
            args.interactive,
            args.container_workdir,
        )
    except (ConfigError, RuntimeError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
