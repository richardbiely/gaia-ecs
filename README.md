<!--
@cond TURN_OFF_DOXYGEN
-->
# gaia-ecs
[![Build Status](https://github.com/richardbiely/gaia-ecs/workflows/build/badge.svg)](https://github.com/richardbiely/gaia-ecs/actions)

Gaia-ECS is an entity component system framework. Some of its current features and highlights are:</br>
* easy-to-use and safe API designed in such a way it tries to prevent you from using bad coding patterns
* based on modern C++ technologies
* doesn't depend on STL containers by default (can be enabled via GAIA_USE_STL_CONTAINERS)
* archetype/chunk-based storage for maximum iteration speed and easy code parallelization
* ability to organize data as AoS or SoA on the component level with very little changes to your code
* tested on all major compilers continuously
* unit-tested for maximum stability
* each important change is benchmarked and checked on disassembly level on multiple compilers to ensure maximum performance

Being early in development, breaking changes to its API are possible. There are also many features to add. However, it is already stable and thoroughly tested. Therefore, using it should not be an issue.

# Table of Contents

* [Introduction](#introduction)
* [Usage](#usage)
  * [Minimum requirements](#minimum-requirements)
  * [Basic operations](#basic-operations)
  * [Simple iteration](#simple-iteration)
  * [Iteration over chunks](#iteration-over-chunks)
  * [Disabling entities](#disabling-entities)
  * [Making use of SoA component layout](#making-use-of-soa-component-layout)
  * [Delayed execution](#delayed-execution)
  * [Chunk components](#chunk-components)
* [Requirements](#requirements)
  * [Compiler](#compiler)
  * [Dependencies](#dependencies)
* [Installation](#installation)
* [Examples](#examples)
* [Benchmarks](#benchmarks)
* [Future](#future)
* [Contributions](#contributions)
* [License](#license)

# Introduction

Entity-Component-System (ECS) is a software architectural pattern based on organizing your code around data. Instead of looking at "items" in your program as objects you normally know from the real world (car, house, human) you look at them as pieces of data necessary for you to achieve some result.

This way of thinking is more natural for machines than people but when used correctly it allows you to write faster code (on most architectures). What is most important, however, is it allows you to write code that is easier to maintain, expand and reason about.

For instance, when moving some object from point A to point B you do not care if it is a house or a car. You only care about its' position. If you want to move it at some specific speed you will consider also the object's velocity. Nothing else is necessary.

Within ECS an entity is an index uniquely identifying components. A component is a piece of data (position, velocity, age). A system is where you implement your program's logic. It is a place that takes components as inputs and generates some output (basically transforms data into different data).

Many different approaches to how ECS should be implemented exist and each is strong in some areas and worse in others. Gaia-ECS is an archetype-based entity component system framework. Therefore, unique combinations of components are stored in things called archetypes.

Each archetype consists of blocks of memory called chunks. In our case, each chunk is 64 KiB big (not recommended but it can be changed via ecs::MemoryBlockSize). Chunks hold components and entities. You can think of them as SQL tables where components are columns and entities are rows.

All memory is preallocated in big blocks (pages) via the internal chunk allocator. Thanks to that all data is organized in a cache-friendly way which most computer architectures like and actual heap allocations which are slow are reduced to a minimum.

One of the benefits of archetype-based architectures is fast iteration and good memory layout by default. They are also easy to parallelize. On the other hand, adding and removing components is slower than with other architectures (in our case this is mitigated by a graph built for archetypes). Knowing the strengths and weaknesses of your system helps you work around their issues so this is not necessarily a problem per se.

# Usage
## Minimum requirements
```cpp
#include <gaia.h>
```
## Basic operations
### Creating and deleting entities
```cpp
ecs::World w;
auto e = w.CreateEntity();
... // do something with the created entity
w.DeleteEntity(e);
```

### Adding and removing components
```cpp
struct Position {
  float x, y, z;
};
struct Velocity {
  float x, y, z;
};

ecs::World w;

// Create an entity with Position and Velocity.
auto e = w.CreateEntity();
w.AddComponent<Position>(e, {0, 100, 0});
w.AddComponent<Velocity>(e, {0, 0, 1});

// Remove Velocity from the entity.
w.RemoveComponent<Velocity>(e);
```

### Setting component value
```cpp
// Change Velocity's value.
w.SetComponent<Velocity>(e, {0, 0, 2});
```

### Checking if component is attached to entity
```cpp
// Check if entity e has Velocity.
const auto* pChunkA = w.GetEntityChunk(e);
bool hasVelocity = pChunkA->HasComponent<Velocity>(e);

// Check if entity e has Position and modify its value if it does.
uint32_t entityIndexInChunk;
auto* pChunkB = w.GetEntityChunk(e, entityIndexInChunk);
if (pChunkB->HasComponent<Position>(e))
{
  auto pos = pChunkB->ViewRW<Position>();
  pos[entityIndexInChunk].y = 1234; // position.y changed from 100 to 1234
}
```

### Batched operations
```cpp
// You can also use a faster batched version of some operations.
e = w.CreateEntity();
w.AddComponent<Position, Velocity>(e, {1, 2, 3}, {0, 0, 0});
w.SetComponent<Position, Velocity>(e, {11, 22, 33}, {10, 5, 0});
w.RemoveComponent<Position, Velocity>(e);
```

## Simple iteration
You can perform operations on your data in multiple ways with ForEach being the simplest one.<br/>
It provides the least room for optimization (that does not mean the generated code is slow by any means) but is very easy to read.
```cpp
w.ForEach([&](Position& p, const Velocity& v) {
  p.x += v.x * dt;
  p.y += v.y * dt;
  p.z += v.z * dt;
});
```

You can also tell the framework you are looking for something more specific by using EntityQuery.<br/>
The example above created it internally from the arguments we provided to ForEach.<br/>
EntityQuery makes it possible for you to include or exclude specific archetypes based on the rules you define.
```cpp
ecs::EntityQuery q;
q.All<Position, Velocity>(); // this is also what ForEach does implicitly when no EntityQuery is provided
q.None<Player>();

// Iterate over all archetypes containing Position and Velocity but no Player.
w.ForEach(q, [&](Position& p, const Velocity& v) {
  p.x += v.x * dt;
  p.y += v.y * dt;
  p.z += v.z * dt;
});
```

Using WithChanged we can take it a step further and only perform the iteration if particular components change.<br/>
Note, if there are 100 Velocity and Position components in the chunk and only one Velocity changes, ForEach performs for the entire chunk.<br/>
This chunk-wide behavior is due to performance concerns as it is easier to reason about the entire chunk than each of its items separately.
```cpp
ecs::EntityQuery q;
q.All<Position, Velocity>(); // chunk must contain Position and Velocity
q.Any<Something, SomethingElse>(); // chunk must contain either Something or SomethingElse
q.None<Player>(); // chunk can not contain Player
q.WithChanged<Velocity>(); // only iterate when Velocity changes

w.ForEach(q, [&](Position& p, const Velocity& v) {
  p.x += v.x * dt;
  p.y += v.y * dt;
  p.z += v.z * dt;
});
```

## Iteration over chunks
ForEachChunk gives you more power than ForEach as it exposes to you the underlying chunk in which your data is contained.<br/>
That means you can perform more kinds of operations, and it also opens doors for new kinds of optimizations.
```cpp
ecs::EntityQuery q;
q.All<Position,Velocity>();

w.ForEachChunk(q, [](ecs::Chunk& ch) {
  auto p = ch.ViewRW<Position>(); // read-write access to Position
  auto v = ch.View<Velocity>(); // read-only access to Velocity

  // Iterate over all components in the chunk.
  for (auto i = 0U; i < size; ++i) {
    p[i].x += v[i].x * dt;
    p[i].y += v[i].y * dt;
    p[i].z += v[i].z * dt;
  }
});
```

I need to make a small but important note here. Analyzing the output of different compilers I quickly realized if you want your code vectorized for sure you need to be very clear and write the loop as a lambda or kernel if you will. It is quite surprising to see this but even with optimizations on and "fast-math"-like switches enabled some compilers simply will not vectorize the loop otherwise. Microsoft compilers are particularly sensitive in this regard. In the years to come maybe this gets better but for now, keep it in mind or use a good optimizing compiler such as Clang.
```cpp
w.ForEachChunk(q, [](ecs::Chunk& ch) {
 auto vp = ch.ViewRW<Position>(); // read-write access to Position
 auto vv = ch.View<Velocity>(); // read-only access to Velocity

 // Make our intentions very clear so even compilers which are weaker at optimization can vectorize the loop
 [&](Position* GAIA_RESTRICT p, const Velocity* GAIA_RESTRICT v, const uint32_t size) {
  for (auto i = 0U; i < size; ++i) {
    p[i].x += v[i].x * dt;
    p[i].y += v[i].y * dt;
    p[i].z += v[i].z * dt;
  }
 }(vp.data(), vv.data(), ch.GetItemCount());
});
```

### Disabling entities
Users are able to enable or disable entities when necessary.<br/>
Disabled entities are moved to chunks different from the rest. 
This also means that by default, disabled entities do not take part in queries. 
```cpp
ecs::EntityQuery q;
q.All<Position, Velocity>();

w.EnableEntity(e, false);
w.ForEach(q, [](Position& p, const Velocity& v) {
  // Entity e is not going to be included in this query.
  // ...
});

w.EnableEntity(e, true);
w.ForEach(q, [](Position& p, const Velocity& v) {
  // Entity e is going to be included in this query.
  // ...
});
```

Default query behavior can be modified by setting query constrains:
```cpp
w.EnableEntity(e, false);

q.SetConstraint(ecs::EntityQuery::Constraint::AcceptAll);
w.ForEach(q, [](Position& p, const Velocity& v) {
  // Both enabled and disabled entities are included in the query.
  // ...
});

q.SetConstraint(ecs::EntityQuery::Constraint::DisabledOnly);
w.ForEach(q, [](Position& p, const Velocity& v) {
  // Only disabled entities are included.
  // ...
});
```

Behavior of EnableEntity is similar to that of calling DeleteEntity/CreateEntity. However, the main benefit is disabling an entity keeps its ID intact which means you can reference it freely.

## Making use of SoA component layout
By default, all data inside components is treated as an array of structures (AoS) via an implicit
```cpp
static constexpr auto Layout = utils::DataLayout::AoS
```
This is the natural behavior of the language and what you would normally expect.<br/>
If we imagine an ordinary array of 4 Position components defined above with this layout they are organized like this in memory: xyz xyz xyz xyz.

However, in specific cases you might want to consider organizing your component's internal data as structure or arrays (SoA):
```cpp
static constexpr auto Layout = utils::DataLayout::SoA
```
Using the example above this will make Gaia-ECS treat Position components like this in memory: xxxx yyyy zzzz.

If used correctly this can have vast performance implications. Not only do you organize your data in the most cache-friendly way this usually also means you can simplify your loops which in turn allows the compiler to optimize your code better.
```cpp
struct PositionSoA {
  float x, y, z;
  static constexpr auto Layout = utils::DataLayout::SoA;
};
struct VelocitySoA {
  float x, y, z;
  static constexpr auto Layout = utils::DataLayout::SoA;
};
...
w.ForEachChunk(ecs::EntityQuery().All<PositionSoA,VelocitySoA>, [](ecs::Chunk& ch) {
  auto vp = ch.ViewRW<PositionSoA>(); // read-write access to PositionSoA
  auto vv = ch.View<VelocitySoA>(); // read-only access to VelocitySoA

  auto px = vp.set<0>(); // get access to a continuous block of "x" from PositionSoA
  auto py = vp.set<1>(); // get access to a continuous block of "y" from PositionSoA
  auto pz = vp.set<2>(); // get access to a continuous block of "z" from PositionSoA
  auto vx = vv.get<0>();
  auto vy = vv.get<1>();
  auto vz = vv.get<2>();

  // The loop becomes very simple now.
  auto exec = [&](float* GAIA_RESTRICT p, const float* GAIA_RESTRICT v, const size_t sz) {
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
  // Handle x coordinates
  exec(px.data(), vx.data(), size);
  // Handle y coordinates
  exec(py.data(), vy.data(), size);
  // Handle z coordinates
  exec(pz.data(), vz.data(), size);
});
```

## Delayed execution
Sometimes you need to delay executing a part of the code for later. This can be achieved via CommandBuffers.<br/>
CommandBuffer is a container used to record commands in the order in which they were requested at a later point in time.<br/>
Typically you use them when there is a need to perform a structural change (adding or removing an entity or component) while iterating chunks.<br/>
Performing an unprotected structural change is undefined behavior and most likely crashes the program.
However, using a CommandBuffer you can collect all requests first and commit them when it is safe.
```cpp
ecs::CommandBuffer cb;
w.ForEach(q, [&](Entity e, const Position& p) {
  if (p.y < 0.0f)
    cb.DeleteEntity(e); // queue entity e for deletion if its position falls below zero
});
cb.Commit(&w); // after calling this all entities with y position bellow zero get deleted
```
If you try to make an unprotected structural change with GAIA_DEBUG enabled (set by default when Debug configuration is used) the framework will assert letting you know you are using it the wrong way.

## Chunk components
A chunk component is a special kind of component which exists at most once per chunk.<br/>
In different words, you attach data to one chunk specifically.<br/>
Chunk components survive entity removals and unlike generic component they do not transfer to a new chunk along with their entity.<br/>
If you organize your data with care (which you should) this can save you some very precious memory or performance depending on your use case.<br/>

For instance, imagine you have a grid with fields of 100 meters squared.
Now if you create your entities carefully they get organized in grid fields implicitly on data level already without you having to use any sort of spatial map container.
```cpp
w.AddComponent<Position>(e1, {10,1});
w.AddComponent<Position>(e2, {19,1});
w.AddChunkComponent<GridPosition>(e1, {1, 0}); // Both e1 and e2 share a common grid position of {1,0} now
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
To use this, you must have the library installed on your machine. You can follow the steps [here](https://github.com/google/benchmark#installation) to do so.<br/>
It is planned to replace this with a header-only library or some custom solution to make the processes easier.

# Installation

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

# Examples
The repository contains some code examples for guidance.<br/>
Examples are built if GAIA_BUILD_EXAMPLES is enabled when configuring the project (ON by default).

* [Example external](https://github.com/richardbiely/gaia-ecs/tree/main/src/examples/example_external) - a dummy example explaining how to use the framework in an external project
* [Example 1](https://github.com/richardbiely/gaia-ecs/tree/main/src/examples/example1) - the same as the previous one but showing how Gaia-ECS is used as a standalone project
* [Example 2](https://github.com/richardbiely/gaia-ecs/tree/main/src/examples/example2) - simple example using some basic framework features
* [Example Rougelike](https://github.com/richardbiely/gaia-ecs/tree/main/src/examples/example_rougelike) - Rouglelike game putting all parts of the framework to use and represents a complex example of how everything would be used in practice; it is work-in-progress and changes and evolves with the project

# Benchmarks
To be able to reason about the project's performance benchmarks and prevent regressions benchmarks were created.<br/>
They can be enabled via GAIA_BUILD_BENCHMARK when configuring the project (OFF by default).

* [Duel](https://github.com/richardbiely/gaia-ecs/tree/main/src/perf/duel) - duel compares different coding approaches such as the basic model with uncontrolled OOP with data all-over-the heap, OOP where allocators are used to controlling memory fragmentation and different ways of data-oriented design and it puts them to test against our ECS framework itself; DOD performance is the target level we want to reach or at least be as close as possible to with this project because it does not get any faster than that 
* [Iter](https://github.com/richardbiely/gaia-ecs/tree/main/src/perf/iter) - this benchmark focuses on the performance of creating and removing entities and components of various sizes and also covers iteration performance with different numbers of entities and archetypes

# Future
Currently, many new features and improvements to the current system are planned.<br/>
Among the most prominent ones those are:
* scheduler - a system that would allow parallel execution of all systems by default, work stealing, and an easy setup of dependencies
* scenes - a way to serialize the state of chunks or entire worlds
* profiling scopes - to allow easy measurement of performance in production
* debugger - an editor that would give one an overview of worlds created by the framework (number of entities, chunk fragmentation, systems running, etc.)

# Contributions

Requests for features, PRs, suggestions, and feedback are highly appreciated.

If you find you can help and want to contribute to the project feel free to contact
me directly (you can find the mail on my [profile page](https://github.com/richardbiely)).<br/>

# License

Code and documentation Copyright (c) 2021-2022 Richard Biely.<br/>

Code released under
[the MIT license](https://github.com/richardbiely/gaia-ecs/blob/master/LICENSE).<br/>
<!--
@endcond TURN_OFF_DOXYGEN
-->

