<!--
@cond TURN_OFF_DOXYGEN
-->
# gaia-ecs
[![Build Status](https://github.com/richardbiely/gaia-ecs/workflows/build/badge.svg)](https://github.com/richardbiely/gaia-ecs/actions)

Gaia-ECS is a Entity Component System framework. Some of its current features and highlights are:</br>
* easy-to-use and safe API designed in such a way it tries to prevent you from using bad coding patterns
* based on modern C++ technologies
* doesn't depend on STL containers by default (can be enabled via GAIA_USE_STL_CONTAINERS)
* archetype-based storage for maximum iteration speed and easy code parallelization
* ability to organize data as AoS or SoA on the component level with almost no changes to your code! 
* tested on all major compilers continuously
* unit-tesed for maximum stabilty
* each important change is benchmarked and checked on disassembly level no multiple compilers in order to ensure maximum performance

It is still early in development and breaking changes to its API are possible. There are also a lot of features to add. However, it is already stable and thoroughly tested so stability should not be an issue.

# Table of Contents

* [Requirements](#requirements)
  * [Compiler](#compiler)
  * [Dependencies](#dependencies)
* [Instalation](#instalation)
* [Contributions](#contributions)
* [License](#license)

# Requirements

## Compiler
Gaia-ECS requires a compiler compatible with C++17.<br/>
Currently, all major compilers are supported:<br/>
- MSVC 15 (MS Visual Studio 2017) and later<br/>
- Clang 7 and later<br/>
- GCC 7 and later<br/>

More compilers might work but the above are guaranteed and [continuosly tested](https://github.com/richardbiely/gaia-ecs/actions/workflows/build.yml).<br/>
ICC support is also [worked on](https://github.com/richardbiely/gaia-ecs/actions/workflows/icc.yml).

## Dependencies
CMake version 3.12 or later is required to prepare the build. Other tools are officially not supported at the moment.

Unit testing is handled via [Catch2 v2.x](https://github.com/catchorg/Catch2/tree/v2.x). It is ON by default and can be controlled via -DGAIA_BUILD_UNITTEST=ON/OFF.<br/>
You can either install Catch2 on your machine manually or use -DGAIA_FIND_CATCH2_PACKAGE when generating your build files and have CMake download and prepare the dependency for you.

Benchmarking relies on [googlebenchmark](https://github.com/google/benchmark). It is OFF by default and can be controlled via -DGAIA_BUILD_BENCHMARK=ON/OFF.<br/>
In order to use this you must have the library installed on your machine. You can follow the steps [here](https://github.com/google/benchmark#installation) to do so.<br/>
It is planned to replace this with a header-only library or some custom solution in order to make the processes easier.

# Instalation

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

# Contributions

Requests for features, PRs, suggestions and feedback are highly appreciated.

If you find you can help and want to contribute to the project feel free to contact
me directly (you can find the mail on my [profile page](https://github.com/richardbiely)).<br/>

# License

Code and documentation Copyright (c) 2021-2022 Richard Biely.<br/>

Code released under
[the MIT license](https://github.com/richardbiely/gaia-ecs/blob/master/LICENSE).<br/>
<!--
@endcond TURN_OFF_DOXYGEN
-->
