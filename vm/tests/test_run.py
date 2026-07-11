import importlib.util
import json
import sys
import tempfile
import unittest
from pathlib import Path


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
            "runtime": "docker",
            "workspace_root": "/var/tmp/gaia-runs",
        }}}))

        target = run.load_target(path, "arm-runner")

        self.assertEqual("arm-runner", target.name)
        self.assertEqual("ssh", target.transport)
        self.assertEqual("buildbox", target.ssh_host)
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


class CommandTests(unittest.TestCase):
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

    def test_remote_shell_uses_configured_ssh_host(self) -> None:
        target = run.Target("remote", "ssh", "other-host", "podman", "/tmp/runs", "gaiaecs-linux-builder")

        command = run.host_command(target, ["podman", "info"])

        self.assertEqual("ssh", command[0])
        self.assertIn("other-host", command)
        self.assertNotIn("krabicka1", command)

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
