import importlib.util
import json
import os
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch


MODULE_PATH = Path(__file__).parents[1] / "run.py"
SPEC = importlib.util.spec_from_file_location("gaia_vm_run", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
run = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = run
SPEC.loader.exec_module(run)


class TargetConfigTests(unittest.TestCase):
    def write_config(self, text: str) -> Path:
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        path = Path(directory.name) / "targets.json"
        path.write_text(text, encoding="utf-8")
        return path

    def test_loads_configurable_ssh_container_target(self) -> None:
        path = self.write_config(json.dumps({"targets": {"arm-runner": {
            "transport": "ssh",
            "ssh_host": "buildbox",
            "ssh_user": "builder",
            "ssh_port": 2222,
            "runtime": "docker",
            "workspace_root": "/var/tmp/gaia-runs",
        }}}))

        target = run.load_target(path, "arm-runner")

        self.assertEqual("arm-runner", target.name)
        self.assertEqual("ssh", target.transport)
        self.assertEqual("buildbox", target.ssh_host)
        self.assertEqual("builder", target.ssh_user)
        self.assertEqual(2222, target.ssh_port)
        self.assertEqual("docker", target.runtime)
        self.assertEqual("/var/tmp/gaia-runs", target.workspace_root)

    def test_rejects_native_environment(self) -> None:
        path = self.write_config(json.dumps({"targets": {"bad": {
            "transport": "ssh", "ssh_host": "buildbox", "runtime": "native",
        }}}))

        with self.assertRaisesRegex(run.ConfigError, "docker or podman"):
            run.load_target(path, "bad")

    def test_requires_ssh_host_for_ssh_transport(self) -> None:
        path = self.write_config(json.dumps({"targets": {"bad": {
            "transport": "ssh", "runtime": "podman",
        }}}))

        with self.assertRaisesRegex(run.ConfigError, "ssh_host"):
            run.load_target(path, "bad")

    def test_rejects_ssh_host_for_local_transport(self) -> None:
        path = self.write_config(json.dumps({"targets": {"bad": {
            "transport": "local", "ssh_host": "buildbox", "runtime": "docker",
        }}}))

        with self.assertRaisesRegex(run.ConfigError, "ssh_host"):
            run.load_target(path, "bad")

    def test_rejects_unsafe_remote_workspace_path(self) -> None:
        path = self.write_config(json.dumps({"targets": {"bad": {
            "transport": "ssh", "ssh_host": "builder", "runtime": "podman",
            "workspace_root": "/tmp/runs with spaces",
        }}}))

        with self.assertRaisesRegex(run.ConfigError, "workspace_root"):
            run.load_target(path, "bad")

    def test_rejects_unknown_and_credential_target_fields(self) -> None:
        path = self.write_config(json.dumps({"targets": {"remote": {
            "transport": "ssh", "ssh_host": "builder", "runtime": "podman",
            "password": "never-store-this",
        }}}))

        with self.assertRaisesRegex(run.ConfigError, "unsupported field"):
            run.load_target(path, "remote")

    def test_rejects_unsafe_target_identity_fields(self) -> None:
        for name, values in (
            ("Bad Name", {"transport": "local", "runtime": "podman"}),
            ("remote", {"transport": "ssh", "ssh_host": "-oProxyCommand=bad", "runtime": "podman"}),
            ("local", {"transport": "local", "runtime": "podman", "image": "bad image"}),
        ):
            path = self.write_config(json.dumps({"targets": {name: values}}))
            with self.assertRaises(run.ConfigError):
                run.load_target(path, name)

    def test_local_workspace_can_use_a_host_visible_path(self) -> None:
        target = run.Target("local", "local", None, "podman", "/tmp/runs", "image")

        self.assertEqual(
            "/Users/example/gaia-ecs",
            run.local_workspace(target, Path("/Users/example/gaia-ecs")),
        )

    def test_remote_target_rejects_a_local_workspace_override(self) -> None:
        target = run.Target("remote", "ssh", "builder", "podman", "/tmp/runs", "image")

        with self.assertRaisesRegex(run.ConfigError, "local-workspace"):
            run.local_workspace(target, Path("/Users/example/gaia-ecs"))

    def test_local_workspace_override_must_be_absolute(self) -> None:
        target = run.Target("local", "local", None, "podman", "/tmp/runs", "image")

        with self.assertRaisesRegex(run.ConfigError, "absolute"):
            run.local_workspace(target, Path("relative/path"))

    def test_explicit_local_host_identity_preserves_bind_mount_ownership(self) -> None:
        target = run.Target("local", "local", None, "podman", "/tmp/runs", "image")

        self.assertEqual("501:20", run.execution_identity(target, "501:20"))

    def test_remote_target_rejects_a_local_host_identity_override(self) -> None:
        target = run.Target("remote", "ssh", "builder", "podman", "/tmp/runs", "image")

        with self.assertRaisesRegex(run.ConfigError, "host-user"):
            run.execution_identity(target, "501:20")


class CommandTests(unittest.TestCase):
    def test_profiles_are_named_commands_owned_by_gaia_ecs(self) -> None:
        profiles = json.loads((MODULE_PATH.parent / "profiles.json").read_text(encoding="utf-8"))["profiles"]

        self.assertEqual(["smoke", "full", "clang", "gcc", "cachegrind"], list(profiles))
        for profile in profiles.values():
            self.assertIsInstance(profile["command"], list)
            self.assertTrue((MODULE_PATH.parents[1] / profile["command"][1]).is_file())

    def test_image_build_context_excludes_workspace_artifacts(self) -> None:
        text = (MODULE_PATH.parent / ".dockerignore").read_text(encoding="utf-8")

        self.assertEqual("*\n!Dockerfile\n!amd64.Dockerfile\n", text)

    def test_windows_local_container_does_not_request_a_posix_user(self) -> None:
        self.assertIsNone(run.local_host_identity("nt", 1000, 1000))

    def test_posix_local_container_uses_the_host_user(self) -> None:
        self.assertEqual("1000:20", run.local_host_identity("posix", 1000, 20))

    def test_container_images_forward_the_requested_command(self) -> None:
        expected = 'ENTRYPOINT ["sh", "-c", "[ \\"$(id -u)\\" -ne 0 ] || service ssh restart; exec \\"$@\\"", "--"]'

        for name in ("Dockerfile", "amd64.Dockerfile"):
            text = (MODULE_PATH.parent / name).read_text(encoding="utf-8")
            self.assertIn(expected, text, name)

    def test_runtime_probe_does_not_install_or_fallback(self) -> None:
        target = run.Target("remote", "ssh", "buildbox", "docker", "/tmp/runs", "gaiaecs-linux-builder")

        command = run.runtime_probe_command(target)

        self.assertEqual(["docker", "info"], command)
        self.assertNotIn("podman", command)
        self.assertNotIn("sudo", command)

    def test_stop_command_targets_only_the_named_container(self) -> None:
        target = run.Target("local", "local", None, "podman", "/tmp/runs", "image")

        self.assertEqual(["podman", "rm", "--force", "gaiaecs-a1b2"], run.container_stop_command(target, "a1b2"))

    def test_parse_accepts_an_external_run_id_and_stop_mode(self) -> None:
        execute_args = run.parse_args(["--target", "local", "--run-id", "a1b2", "--", "true"])
        stop_args = run.parse_args(["--target", "local", "--stop-run", "a1b2", "--run-token", "12345678-1234-1234-1234-123456789abc"])

        self.assertEqual("a1b2", execute_args.run_id)
        self.assertEqual("a1b2", stop_args.stop_run)
        self.assertEqual([], stop_args.command)

    def test_remote_run_claim_is_exclusive_and_token_owned(self) -> None:
        target = run.Target("remote", "ssh", "builder", "podman", "/tmp/runs", "image")
        token = "12345678-1234-1234-1234-123456789abc"

        commands = run.remote_claim_commands(target, "/tmp/runs/a1b2/src", "a1b2", token)
        container = run.container_command(target, "/tmp/runs/a1b2/src", ["true"], "a1b2", run_token=token)

        self.assertEqual(["mkdir", "/tmp/runs/a1b2"], commands[1])
        self.assertIn(f"/tmp/runs/a1b2/token-{token}", commands[2])
        self.assertIn(f"gaiaecs.run-token={token}", container)

    def test_remote_shell_uses_configured_ssh_host(self) -> None:
        target = run.Target("remote", "ssh", "other-host", "podman", "/tmp/runs", "gaiaecs-linux-builder")

        command = run.host_command(target, ["podman", "info"])

        self.assertEqual("ssh", command[0])
        self.assertIn("other-host", command)
        self.assertIn("StrictHostKeyChecking=yes", command)
        self.assertIn("/dev/null", command)
        self.assertNotIn("krabicka1", command)

    def test_password_authentication_uses_sshpass_without_command_line_secret(self) -> None:
        target = run.Target("remote", "ssh", "builder", "podman", "/tmp/runs", "image", "ci", 2222)

        with patch.dict(os.environ, {"SSHPASS": "write-only-value"}):
            command = run.host_command(target, ["podman", "info"])
            sync = run.remote_sync_command(target, "/tmp/runs/a1b2/src")

        self.assertEqual(["sshpass", "-e", "ssh"], command[:3])
        self.assertIn("BatchMode=no", command)
        self.assertTrue(any("sshpass -e ssh" in value for value in sync))
        self.assertNotIn("write-only-value", " ".join(command + sync))

    def test_remote_sync_is_non_interactive_and_requires_a_known_host(self) -> None:
        target = run.Target("remote", "ssh", "builder", "podman", "/tmp/runs", "image")

        command = run.remote_sync_command(target, "/tmp/runs/a1b2/src")

        self.assertIn("ssh -F /dev/null -o BatchMode=yes -o StrictHostKeyChecking=yes -p 22", command)
        self.assertIn("-F /dev/null", command[4])
        self.assertEqual("builder:/tmp/runs/a1b2/src/", command[-1])

    def test_remote_interactive_shell_allocates_an_ssh_tty(self) -> None:
        target = run.Target("remote", "ssh", "other-host", "docker", "/tmp/runs", "gaiaecs-linux-builder")

        command = run.host_command(target, ["docker", "run"], interactive=True)

        self.assertIn("-tt", command)

    def test_container_command_mounts_staged_workspace(self) -> None:
        target = run.Target("remote", "ssh", "buildbox", "docker", "/tmp/runs", "custom-image")

        command = run.container_command(
            target, "/tmp/runs/42/src", ["bash", "vm/build_gcc.sh", "-c"], user="1000:1000"
        )

        self.assertEqual("docker", command[0])
        self.assertIn("/tmp/runs/42/src:/gaia-ecs", command)
        self.assertIn("1000:1000", command)
        self.assertEqual(["bash", "vm/build_gcc.sh", "-c"], command[-3:])
        self.assertNotIn("--privileged", command)


    def test_interactive_container_allocates_a_tty(self) -> None:
        target = run.Target("local", "local", None, "docker", "/tmp/runs", "custom-image")

        command = run.container_command(target, "/repo", ["bash"], interactive=True)

        self.assertIn("--interactive", command)
        self.assertIn("--tty", command)


class SetupScriptTests(unittest.TestCase):
    def test_shell_wrappers_delegate_to_the_container_runner(self) -> None:
        for name, target in (("setup.sh", "local-docker"), ("setup_podman.sh", "local-podman")):
            text = (MODULE_PATH.parent / name).read_text(encoding="utf-8")
            self.assertIn("run.py", text, name)
            self.assertIn(f"--target {target}", text, name)
            self.assertIn("--interactive", text, name)
            self.assertNotIn("--privileged", text, name)
            self.assertNotIn("machine start", text, name)

    def test_windows_wrappers_delegate_to_the_container_runner(self) -> None:
        for name, target in (("setup.bat", "local-docker"), ("setup_podman.bat", "local-podman")):
            text = (MODULE_PATH.parent / name).read_text(encoding="utf-8")
            self.assertIn("run.py", text, name)
            self.assertIn(f"--target {target}", text, name)
            self.assertIn("--interactive", text, name)
            self.assertNotIn("--privileged", text, name)
            self.assertNotIn("machine start", text, name)


if __name__ == "__main__":
    unittest.main()
