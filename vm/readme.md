# VM Container Setup and Build Instructions

Prepare a docker container (inside the "vm" folder):

* MacOS: bash ./setup.sh
* Windows: bash ./setup.bat

Prepare a podman container (inside the "vm" folder):

* MacOS: bash ./setup_podman.sh
* Windows: bash ./setup_podman.bat

Once inside the virtual machine you can build the project:

* Both Clang & GCC: ./build.sh
* Clang only: ./build_clang.sh
* GCC only: ./build_gcc.sh
* Cachegrid: ./build_clang_cachegrind.sh

Meant for internal usage primarily on Windows and MacOS.
On Linux you likely already have both Clang or GCC set up.
If you do not, it is also fine.