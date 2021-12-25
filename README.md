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
* unit-tested for maximum stabilty
* each important change is benchmarked and checked on disassembly level no multiple compilers in order to ensure maximum performance

It is still early in development and breaking changes to its API are possible. There are also a lot of features to add. However, it is already stable and thoroughly tested so stability should not be an issue.

# Table of Contents

* [Usage examples](#usage-examples)
  * [Minimum requirements](#minimum-requirements)
  * [Entity and component creation](#entity-and-component-creation)
  * [Simple iteration](#simple-iteration)
  * [Iteration over chunks](#iteration-over-chunks)
  * [Making use of SoA component layout ](#making-use-of-soa-component-layout)
* [Requirements](#requirements)
  * [Compiler](#compiler)
  * [Dependencies](#dependencies)
* [Instalation](#instalation)
* [Contributions](#contributions)
* [License](#license)

# Usage examples
## Minimum requirements
```cpp
#include <gaia.h>
GAIA_INIT
```
## Entity and component creation
```cpp
struct Position {
  float x, y, z;
};
struct Velocity {
  float x, y, z;
};

ecs::World w;
auto e = w.CreateEntity();
w.AddComponent<Position>(e, {0, 100, 0});
w.AddComponent<Velocity>(e, {0, 0, 1});
```
## Simple iteration
You can perform operations on your data in multiple ways with ForEach being the simplest one.<br/>
It provides the least room for optimization (that does not mean the generated code is slow by any means) but is very easy to read.
```cpp
w.ForEach([&](Position& p, const Velocity& v) {
  p.x += v.x * dt;
  p.y += v.y * dt;
  p.z += v.z * dt;
}).Run();
```

You can also be more specifc and tell the framework you are looking for something more specific using an EntityQuery.<br/>
The example above created it internally from the arguments we provided for ForEach.<br/>
EntityQuery makes it possible for you to include or exclude specific archetypes based on the rules you define.
```cpp
ecs::EntityQuery q;
q.All<Position, Velocity>();
q.None<Player>();
// Iterate over all archytpes containing Position and Velocity but no Player
w.ForEach(q, [&](Position& p, const Velocity& v) {
  p.x += v.x * dt;
  p.y += v.y * dt;
  p.z += v.z * dt;
}).Run();
```

We can even take it a step further only only perform the iteration if some particular components changes.<br/>
Beware this check is chunk-wide meaning if there are 100 Velocity and Position components in the chunk and only one Velocity changes ForEach performs for the entire chunk. This is due performance concerns as it is easier to reason about the entire chunk than each item in separately.
```cpp
ecs::EntityQuery q;
q.All<Position, Velocity>();
q.None<Player>();
q.WithChanged<Velocity>(); // Only iterate when Velocity changes
// Iterate over all archytpes containing Position and Velocity but no Player
w.ForEach(q, [&](Position& p, const Velocity& v) {
  p.x += v.x * dt;
  p.y += v.y * dt;
  p.z += v.z * dt;
}).Run();
```

## Iteration over chunks
ForEachChunk gives you more power than ForEach as it exposes the underlying chunk in which your data is container to you.<br/>
This means you can perform more kinds of operations and opens doors for new kinds of optimizations.
```cpp
w.ForEachChunk([](ecs::Chunk& ch) {
  auto p = ch.ViewRW<Position>(); // Read-write access to Position
  auto v = ch.View<Velocity>(); // Read-only access to Velocity

  // Iterate over all components in the chunk.
  // Note this could be written as a simple for loop. However, analysing the output of different compilers I quickly
  // realised if you want your code vectorized for sure this is the only way to go (MSVC is particularly sensitive).
  [&](Position* GAIA_RESTRICT p, const Velocity* GAIA_RESTRICT v, const uint32_t size) {
    for (auto i = 0U; i < size; ++i) {
      p[i].x += v[i].x * dt;
      p[i].y += v[i].y * dt;
      p[i].z += v[i].z * dt;
    }
  }(p.data(), v.data(), ch.GetItemCount());
}).Run();
```
## Making use of SoA component layout 
For specific cases you might consider oranizing your component's internal data in SoA way.<br/>
For instance, in the code bellow me mark PositionSoA and VelocitySoA with
```cpp
static constexpr auto Layout = utils::DataLayout::SoA
```
Now if you imagine an ordinary array of 4 PositionSoAs they would be organized as this in memory: xyz xyz xyz xyz.<br/>
However, using utils::DataLayout::SoA makes Gaia-ECS treat them as: xxxx yyyy zzzz.<br/>
This can have vast performance implication because your code can be fully vectorized by the compiler.
```cpp
struct PositionSoA {
	float x, y, z;
 // Following makes Gaia-ECS treat the component as blocks organized in memory in SoA-way: xxxx yyyy zzzz
	static constexpr auto Layout = utils::DataLayout::SoA;
};
struct VelocitySoA {
	float x, y, z;
	static constexpr auto Layout = utils::DataLayout::SoA;
};
...
w.ForEachChunk(ecs::EntityQuery().All<PositionSoA,VelocitySoA>, [](ecs::Chunk& ch) {
  auto p = ch.ViewRW<PositionSoA>(); // Read-write access to PositionSoA
  auto v = ch.View<VelocitySoA>(); // Read-only access to VelocitySoA

  auto ppx = p.set<0>(); // Get access to a continuous block of "x" from PositionSoA
  auto ppy = p.set<1>(); // Get access to a continuous block of "y" from PositionSoA
  auto ppz = p.set<2>(); // Get access to a continuous block of "z" from PositionSoA
  auto vvx = v.get<0>();
  auto vvy = v.get<1>();
  auto vvz = v.get<2>();

  auto exec = [](float* GAIA_RESTRICT p, const float* GAIA_RESTRICT v, const size_t sz) {
    for (size_t i = 0U; i < sz; ++i)
      p[i] += v[i] * dt;
    /*
    You can even use intrinsics now without a worry.
    Note, this is just a simple example not an optimal way to rewrite the loop above using intrinsics.
    for (size_t i = 0U; i < sz; i+=4) {
      const auto pVec = _mm_load_ps(p + i);
      const auto vVec = _mm_load_ps(v + i);
      const auto respVec = _mm_fmadd_ps(vVec, dtVec, pVec);
      _mm_store_ps(p + i, respVec);
    }*/
  };
  const auto size = ch.GetItemCount();
  exec(ppx.data(), vvx.data(), size);
  exec(ppy.data(), vvy.data(), size);
  exec(ppz.data(), vvz.data(), size);
}).Run();
```

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
