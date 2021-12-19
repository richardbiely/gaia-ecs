<!--
@cond TURN_OFF_DOXYGEN
-->
# gaia-ecs
[![Build Status](https://github.com/richardbiely/gaia-ecs/workflows/build/badge.svg)](https://github.com/richardbiely/gaia-ecs/actions)

Gaia is an archetype-based ECS library.

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
# ... or if you use CMake older than 3.13 you can do:
# cmake -E chdir "build" cmake -DCMAKE_BUILD_TYPE=Release ../
# Build the library
cmake --build "build" --config Release
```

You can also use sanitizers with the project via -USE_SANITIZERS.
```bash
cmake -DCMAKE_BUILD_TYPE=Release -USE_SANITIZERS=address -S . -B "build"
```
Possible options are listed in [cmake/sanitizers.cmake](https://github.com/richardbiely/gaia-ecs/blob/main/cmake/sanitizers.cmake).<br/>
Note, some options don't work together or might not be supported by all compilers.

## Contributions

Requests for features, PRs, suggestions and feedback are highly appreciated.

If you find you can help and want to contribute to the project feel free to contact
me directly (you can find the mail on my [profile page](https://github.com/richardbiely)).<br/>

## License

Code and documentation Copyright (c) 2021-2022 Richard Biely.<br/>

Code released under
[the MIT license](https://github.com/richardbiely/gaia-ecs/blob/master/LICENSE).<br/>
<!--
@endcond TURN_OFF_DOXYGEN
-->
