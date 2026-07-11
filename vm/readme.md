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

Copy `targets.example.json` to `~/.config/gaia-ecs/targets.json` and customize the target names and SSH aliases. SSH connection details belong in `~/.ssh/config`; do not store passwords or private keys in the target file.

Run a command in an existing image:

```text
python3 vm/run.py --target local-docker -- bash vm/build_gcc.sh -c
python3 vm/run.py --target remote-docker -- bash vm/build_gcc.sh -c
```

Add `--build-image` to build `gaiaecs-linux-builder` before running the command. The target can override the image with an `image` field. The runner selects `amd64.Dockerfile` on x86-64 hosts and `Dockerfile` on other architectures.

Remote targets stage the current working tree under the configured absolute `workspace_root`, bind-mount that remote directory at `/gaia-ecs`, and remove it after a successful run. Failed workspaces are retained for diagnosis. Use `--keep-workspace` to retain successful runs as well.

The configured runtime is authoritative. The runner checks `<runtime> info` before staging source or building an image. If Docker or Podman is missing, inaccessible, or not usable by the SSH user, the run fails without installing a runtime, using `sudo`, or falling back to another runtime.
