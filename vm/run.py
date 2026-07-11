#!/usr/bin/env python3

import argparse
import json
import os
import platform
import shlex
import subprocess
import sys
import uuid
from dataclasses import dataclass
from pathlib import Path
from typing import List, Optional, Sequence


REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_CONFIG = Path.home() / ".config" / "gaia-ecs" / "targets.json"
DEFAULT_IMAGE = "gaiaecs-linux-builder"
TARGET_FIELDS = {"transport", "runtime", "ssh_host", "workspace_root", "image"}


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

    if not isinstance(data, dict) or set(data) != {"targets"}:
        raise ConfigError(f"target configuration {path} must contain only the targets object")

    try:
        values = data["targets"][name]
    except (KeyError, TypeError) as exc:
        raise ConfigError(f'target "{name}" is not defined in {path}') from exc

    if not isinstance(values, dict):
        raise ConfigError(f'target "{name}" must be an object')
    if not name or len(name) > 63 or not name[0].isalnum() or any(not (char.islower() or char.isdigit() or char == "-") for char in name):
        raise ConfigError(f'target "{name}" has an invalid name')
    unsupported = set(values) - TARGET_FIELDS
    if unsupported:
        raise ConfigError(f'target "{name}" contains unsupported field: {sorted(unsupported)[0]}')

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
    if ssh_host and (
        not isinstance(ssh_host, str)
        or not ssh_host[0].isalnum()
        or any(not (char.isalnum() or char in "@._:[]-") for char in ssh_host)
    ):
        raise ConfigError(f'target "{name}" ssh_host must be a safe OpenSSH destination or alias')
    if transport == "local" and ssh_host:
        raise ConfigError(f'target "{name}" must not set ssh_host for local transport')
    workspace_chars = "/._-"
    if (
        not isinstance(workspace_root, str)
        or not workspace_root.startswith("/")
        or not all(char.isalnum() or char in workspace_chars for char in workspace_root)
        or ".." in workspace_root.split("/")
    ):
        raise ConfigError(f'target "{name}" workspace_root must be a safe absolute path')
    if not isinstance(image, str) or not image or not image[0].isalnum() or any(not (char.isalnum() or char in "/_.:@-") for char in image):
        raise ConfigError(f'target "{name}" image must be a valid container image reference')

    return Target(name, transport, ssh_host, runtime, workspace_root, image)


def runtime_probe_command(target: Target) -> List[str]:
    return [target.runtime, "info"]


def host_command(target: Target, command: Sequence[str], interactive: bool = False) -> List[str]:
    if target.transport == "local":
        return list(command)
    assert target.ssh_host is not None
    result = ["ssh", "-o", "BatchMode=yes", "-o", "StrictHostKeyChecking=yes"]
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


def local_workspace(target: Target, override: Optional[Path]) -> str:
    if target.transport == "ssh" and override is not None:
        raise ConfigError("--local-workspace is only valid for local targets")
    if override is not None and not override.is_absolute():
        raise ConfigError("--local-workspace must be an absolute path")
    return str(override if override is not None else REPO_ROOT)


def remote_sync_command(target: Target, workspace: str) -> List[str]:
    assert target.ssh_host is not None
    source = f"{REPO_ROOT}/"
    destination = f"{target.ssh_host}:{workspace}/"
    return [
        "rsync",
        "-az",
        "--delete",
        "-e",
        "ssh -o BatchMode=yes -o StrictHostKeyChecking=yes",
        "--exclude=.git/",
        "--exclude=build*/",
        "--exclude=vm/build*/",
        "--exclude=.cache/",
        "--exclude=__pycache__/",
        source,
        destination,
    ]


def stage_remote_workspace(target: Target, workspace: str) -> None:
    run_checked(host_command(target, ["mkdir", "-p", workspace]), "remote workspace creation")
    run_checked(remote_sync_command(target, workspace), "remote source synchronization")


def remove_remote_run(target: Target, run_id: str) -> None:
    run_root = f"{target.workspace_root.rstrip('/')}/{run_id}"
    run_checked(host_command(target, ["rm", "-rf", run_root]), "remote workspace cleanup")


def validate_run_id(run_id: str) -> str:
    if not run_id or len(run_id) > 64 or any(not (char.islower() or char.isdigit() or char == "-") for char in run_id):
        raise ConfigError("run id must contain only lowercase letters, digits, and hyphens")
    return run_id


def container_stop_command(target: Target, run_id: str) -> List[str]:
    return [target.runtime, "rm", "--force", f"gaiaecs-{validate_run_id(run_id)}"]


def stop_run(target: Target, run_id: str) -> None:
    subprocess.run(host_command(target, container_stop_command(target, run_id)), check=False)
    if target.transport == "ssh":
        run_root = f"{target.workspace_root.rstrip('/')}/{validate_run_id(run_id)}"
        subprocess.run(host_command(target, ["rm", "-rf", run_root]), check=False)


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


def execution_identity(target: Target, override: Optional[str]) -> Optional[str]:
    if override is None:
        return host_identity(target)
    if target.transport != "local":
        raise ConfigError("--host-user is only valid for local targets")
    values = override.split(":")
    if len(values) != 2 or not all(value.isdigit() for value in values):
        raise ConfigError("--host-user must be a numeric uid:gid pair")
    return override


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
    local_workspace_override: Optional[Path],
    host_user_override: Optional[str],
    run_id: str,
) -> None:
    probe_runtime(target)
    validate_run_id(run_id)
    workspace = local_workspace(target, local_workspace_override)
    staged = target.transport == "ssh"

    if staged:
        workspace = remote_workspace(target, run_id)
        stage_remote_workspace(target, workspace)

    succeeded = False
    try:
        if rebuild_image:
            build_image(target, workspace)
        user = execution_identity(target, host_user_override)
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
    parser.add_argument(
        "--local-workspace",
        type=Path,
        help="host-visible absolute Gaia-ECS path used as the local container bind source",
    )
    parser.add_argument("--host-user", help="numeric uid:gid used for local bind-mounted container output")
    parser.add_argument("--run-id", default=uuid.uuid4().hex, help="caller-provided collision-resistant run identifier")
    parser.add_argument("--stop-run", help="stop and clean up a previously named run")
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args(argv)
    if args.command[:1] == ["--"]:
        args.command = args.command[1:]
    if args.stop_run and args.command:
        parser.error("--stop-run does not accept a container command")
    if not args.stop_run and not args.command:
        parser.error("a container command is required after --")
    return args


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = parse_args(argv)
    try:
        target = load_target(args.config, args.target)
        if args.stop_run:
            stop_run(target, validate_run_id(args.stop_run))
        else:
            execute(
                target,
                args.command,
                args.build_image,
                args.keep_workspace,
                args.interactive,
                args.container_workdir,
                args.local_workspace,
                args.host_user,
                validate_run_id(args.run_id),
            )
    except (ConfigError, RuntimeError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
