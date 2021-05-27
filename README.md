# gaia-ecs
Gaia is a header-only ECS library.

## Requirements

Gaia-ECS requires a compiler compatible with C++17. Currently, VS2017 and later or equivalent GCC or Clang are supported.

## Instalation

Following shows the steps needed to build the library:

```bash
# Check out the library
git clone https://github.com/richardbiely/gaia-ecs.git
# Go to the library root
cd gaia-ecs
# Make a build directory
cmake -E make_directory "build"
# Generate cmake build files (Release for Release configuration)
cmake -DCMAKE_BUILD_TYPE=Release -S . -B "build"
# Build the library
cmake --build "build" --config Release
```
