# VM Container Setup and Build Instructions

Build the image and open an interactive Docker container:

* MacOS: bash ./setup.sh
* Windows: setup.bat

Build the image and open an interactive Podman container:

* MacOS: bash ./setup_podman.sh
* Windows: setup_podman.bat

The setup scripts are local convenience wrappers around `run.py`. They use the committed local targets from `targets.example.json`, open the shell in `/gaia-ecs/vm`, and require Python 3 plus an already usable container runtime. They do not install, start, or reconfigure Docker or Podman. In particular, start a Podman machine yourself before invoking the Podman wrapper when the host requires one.

The container image configures Git `safe.directory='*'` so CMake `FetchContent` can clone and checkout dependencies inside the bind-mounted Gaia-ECS workspace on Docker and Podman.

The image build context is intentionally limited by `vm/.dockerignore` to the two Dockerfiles. Local build trees under `vm/build-*` are bind-mounted only when the container runs and are never uploaded to the image builder.

Once inside the virtual machine you can build the project:

* Both Clang & GCC: ./build.sh
* Clang only: ./build_clang.sh
* GCC only: ./build_gcc.sh
* Cachegrid: ./build_clang_cachegrind.sh

Meant for internal usage primarily on Windows and MacOS.
On Linux you likely already have both Clang or GCC set up.
If you do not, it is also fine.

## Configurable container targets

`run.py` executes a command in a Docker or Podman container, either locally or on a configurable SSH host. Native execution is not supported.

Copy `targets.example.json` to `~/.config/gaia-ecs/targets.json` and customize the target name, host, username, and port. Do not store passwords or private keys in the target file.

Remote commands use non-interactive SSH authentication and require the host key to already exist in the user's known-hosts file. Unlock credentials through an SSH agent or operating-system keychain before starting a run.

Run a command in an existing image:

```text
python3 vm/run.py --target local-docker -- bash vm/build_gcc.sh -c
python3 vm/run.py --target remote-docker -- bash vm/build_gcc.sh -c
```

Add `--build-image` to build `gaiaecs-linux-builder` before running the command. The target can override the image with an `image` field. The runner selects `amd64.Dockerfile` on x86-64 hosts and `Dockerfile` on other architectures.

`profiles.json` is the machine-readable list of supported integration profiles used by tooling such as Gaia-ECS Control Room. It includes a non-mutating environment smoke test, complete Clang and GCC matrices, sanitizer-only compiler profiles, and Cachegrind analysis. The sanitizer-only profiles skip unsanitized C++17/20/23 builds and tests.

The Clang and GCC profiles build each configuration once, then run independent sanitized executables as separate processes with a default concurrency of two. Set `GAIA_SANITIZER_JOBS` inside the workflow environment to a value from 1 through 64 to adjust that bound. Child output is grouped by task and limited to the final one MiB per task. All requested sanitizer tasks finish even if one fails, and the profile exits unsuccessfully when any task fails. Cancellation first terminates each active process group, then forcibly kills groups that remain after two seconds.

Control processes running inside another container can pass `--local-workspace` for the host-visible bind source and `--host-user uid:gid` to preserve ownership of generated build files. These options apply only to local targets. Direct host usage normally needs neither option.

Integrations can assign a collision-resistant identifier with `--run-id`. Cancellation uses the same identifier with `--stop-run` to force-remove the named container and clean a staged remote workspace.

Remote targets stage the current working tree under the configured absolute `workspace_root`, bind-mount that remote directory at `/gaia-ecs`, and remove it after a successful run. Failed workspaces are retained for diagnosis. Use `--keep-workspace` to retain successful runs as well.

The configured runtime is authoritative. The runner checks `<runtime> info` before staging source or building an image. If Docker or Podman is missing, inaccessible, or not usable by the SSH user, the run fails without installing a runtime, using `sudo`, or falling back to another runtime.
