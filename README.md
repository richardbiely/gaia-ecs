<!--
@cond TURN_OFF_DOXYGEN
-->
# gaia-ecs
[![Build Status][badge.actions]][actions]
[![language][badge.language]][language]
[![license][badge.license]][license]

[badge.actions]: https://github.com/richardbiely/gaia-ecs/workflows/build/badge.svg
[badge.language]: https://img.shields.io/badge/language-C%2B%2B17-yellow
[badge.license]: https://img.shields.io/badge/license-MIT-blue

[actions]: https://github.com/richardbiely/gaia-ecs/actions
[language]: https://en.wikipedia.org/wiki/C%2B%2B17
[license]: https://en.wikipedia.org/wiki/MIT_License

Gaia-ECS is an entity component system framework. Some of its current features and highlights are:</br>
* easy-to-use and safe API designed in such a way it tries to prevent you from using bad coding patterns
* based on modern C++ technologies
* doesn't depend on STL containers by default (can be enabled via GAIA_USE_STL_CONTAINERS)
* archetype/chunk-based storage for maximum iteration speed and easy code parallelization
* ability to organize data as AoS or SoA on the component level with very little changes to your code
* tested on all major compilers continuously
* unit-tested for maximum stability
* each important change is benchmarked and checked on disassembly level on multiple compilers to ensure maximum performance
* exists also as a single-header library which means you can simply drop it into your project and start using it

Being early in development, breaking changes to its API are possible. There are also many features to add. However, it is already stable and thoroughly tested. Therefore, using it should not be an issue.

# Table of Contents

* [Introduction](#introduction)
* [Usage](#usage)
  * [Minimum requirements](#minimum-requirements)
  * [Basic operations](#basic-operations)
  * [Data queries](#data-queries)
  * [Simple iteration](#simple-iteration)
  * [Chunk iteration](#chunk-iteration)
  * [Data layouts](#data-layouts)
  * [Delayed execution](#delayed-execution)
  * [Chunk components](#chunk-components)
* [Requirements](#requirements)
  * [Compiler](#compiler)
  * [Dependencies](#dependencies)
* [Installation](#installation)
* [Examples](#examples)
* [Benchmarks](#benchmarks)
* [Profiling](#profiling)
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
The entire framework is placed in a namespace called <b>gaia</b>.

## Basic operations
### Creating and deleting entities
```cpp
ecs::World w;
auto e = w.CreateEntity();
... // do something with the created entity
w.DeleteEntity(e);
```

### Enabling and disabling entities
Disabled entities are moved to chunks different from the rest. Because of that they do not take part in queries by default.<br/>
Behavior of EnableEntity is similar to that of calling DeleteEntity/CreateEntity. However, the main benefit is that a disabled entity keeps its ID intact which means you can reference it freely.

```cpp
ecs::World w;
auto e = w.CreateEntity();
w.EnableEntity(e, false); // disable the entity
w.EnableEntity(e, true); // enable the entity again
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

### Set and get component value
```cpp
// Change Velocity's value.
w.SetComponent<Velocity>(e, {0, 0, 2});
// Same as above but the world version is not updated so nobody gets notified of this change.
w.SetComponentSilent<Velocity>(e, {4, 2, 0});
```

In case there are more different components on your entity you would like to change the value of you can achive it via SetComponent chaining:
```cpp
// Change Velocity's value on entity "e"
w.SetComponent<Velocity>(e, {0, 0, 2}).
// Change Position's value on entity "e"
  SetComponent<Position>({0, 100, 0}).
// Change...
  SetComponent...;
```

Components are returned by value for components with size up to 8 bytes (including). Bigger components are returned by const reference.
```cpp
// Read Velocity's value. The value is returned by const reference
const auto& velNew = w.GetComponent<Velocity>(e);
// However, it is easy to store a copy.
auto velNewByVal = w.GetComponent<Velocity>(e);
```

Both read and write operations are also accessible via views. These are what SetComponent / GetComponent uses internaly.
```cpp
uint32_t entityIndexInChunk;
auto *pChunk = w.GetChunk(e, entityIndexInChunk);

// Read-only view. We can only use this to read values form our chunk.
auto velocity_view = pChunk->View<Velocity>();
Velocity vel = velocity_view[entityIndexInChunk];

// Read-write view. We can use this one to also modify contents of our chunk.
// These views automatically update the world version and can be used to detect
// changes in chunk which is usually desirable.
auto velocity_view_rw = pChunk->ViewRW<Velocity>();
velocity_view_rw[entityIndexInChunk] = Velocity{};

// Read-write silent view. Esentially the same as ViewRW with the exception
// that it does not modify the world version and changes made by it can not
// be detected.
auto velocity_view_rws = pChunk–>ViewRWSilent<Velocity>();
velocity_view_rws[entityIndexInChunk] = Velocity{};
```

### Checking if component is attached to entity
```cpp
// Check if entity e has Velocity (via world).
const bool hasVelocity = w.HasComponent<Velocity>(e);
...

// Check if entity e has Velocity (via chunk).
const auto* pChunkA = w.GetChunk(e);
const bool hasVelocity = pChunkA->HasComponent<Velocity>();
...

// Check if entity e has Position and modify its value if it does.
uint32_t entityIndexInChunk;
auto* pChunkB = w.GetChunk(e, entityIndexInChunk);
if (pChunkB->HasComponent<Position>())
{
  auto pos = pChunkB->ViewRW<Position>();
  pos[entityIndexInChunk].y = 1234; // position.y changed to 1234
}
```

## Data queries
For querying data you can use an EntityQuery. It can help you find all entities, components or chunks matching the specified set of components and constraints and returns them in the form of an array. You can also use them to quickly check is entities satisfing the given set of rules exist or calculate how many of them there are.<br/> 
```cpp
EntityQuery q;
q.All<Position>(); // consider only entities with Position

// Fill the entities array with entities with a Position component.
containers::darray<ecs::Entity> entities;
w.FromQuery(q).ToArray(entities);

// Fill the positions array with position data.
containers::darray<Position> positions;
w.FromQuery(q).ToArray(positions);

// Print the result
for (size_t i = 0; i < entities.size(); ++i)
{
  const auto& e = entities[i];
  const auto& p = positions[i];
  printf("Entity %u is located at [x,y,z]=[%f,%f,%f]\n", e.id(), p.x, p.y, p.z);
}

// Fill the chunk array with chunks matching the query.
containers::darray<ecs::Chunk*> chunks;
w.FromQuery(q).ToChunkArray(chunks);
for (const auto* pChunk: chunks) {
  // ... do something
}

// Print the number of entities matching the query. For demonstration purposes only.
// Because we already called ToArray we would normally use entities.size() or positions.size().
printf("Number of results: %u", q.CalculateItemCount());
```

More complex queries can be created by combining All, Any and None in any way you can imagine:
```cpp
ecs::EntityQuery q;
q.All<Position, Velocity>();       // Take into account everything with Position and Velocity...
q.Any<Something, SomethingElse>(); // ... at least Something or SomethingElse...
q.None<Player>();                  // ... and no Player component...
```

All EntityQuery perations can be chained and it is also possible to invoke various filters multiple times with unique components:
```cpp
ecs::EntityQuery q;
q.All<Position>()                 // Take into account everything with Position...
 .All<Velocity>()                 // ... and at the same time everything with Velocity...
 .Any<Something, SomethingElse>() // ... at least Something or SomethingElse...
 .None<Player>();                 // ... and no Player component...
```

Using WithChanged we can take it a step further and filter only data which actually changed. This becomes particulary useful when iterating as you will see later on.<br/>
Note, if there are 100 Position components in the chunk and only one them changes, all or them are considered changed. This chunk-wide behavior is due to performance concerns as it is easier to reason about the entire chunk than each of its items separately.
```cpp
ecs::EntityQuery q;
q.All<Position, Velocity>();       // Take into account everything with Position and Velocity...
q.Any<Something, SomethingElse>(); // ... at least Something or SomethingElse...
q.None<Player>();                  // ... and no Player component...
q.WithChanged<Velocity>();         // ... but only such with their Velocity changed
```

Query behavior can be modified by setting constraints. By default only enabled entities are taken into account. However, by changing constraints we can filter disabled entities exclusively or make the query consider both enabled and disabled entities at the same time:
```cpp
ecs::Entity e1, e2;

// Create 2 entities with Position component
w.CreateEntity(e1);
w.CreateEntity(e2);
w.AddComponent<Position>(e1);
w.AddComponent<Position>(e2);

// Disable the first entity
w.EnableEntity(e1, false);

ecs::EntityQuery q;
q.All<Position>();
containers::darray<ecs::Entity> entities;

// Fills the array with only e2 because e1 is disabled.
w.FromQuery(q).ToArray(entities);

// Fills the array with both e1 and e2.
q.SetConstraint(ecs::EntityQuery::Constraint::AcceptAll);
w.FromQuery(q).ToArray(entities);

// Fills the array with only e1 because e1 is disabled.
q.SetConstraint(ecs::EntityQuery::Constraint::DisabledOnly);
w.FromQuery(q).ToArray(entities);
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

You can also be more specific by providing an EntityQuery.<br/>
The example above creates an EntityQuery internally from the arguments provided to ForEach. However, this version can also be slower because it needs to do a lookup in the EntityQuery cache. Therefore, consider it only for non-critical parts of your code. Even though the code is longer, it's a good pratice to use explicit EntityQueries when possible.
```cpp
ecs::EntityQuery q;
q.All<Position, Velocity>(); // Take into account all chunks with Position and Velocity...
q.None<Player>();            // ... but no Player component.

w.ForEach(q, [&](Position& p, const Velocity& v) {
  // This operations runs for each entity with Position, Velocity and no Player
  p.x += v.x * dt;
  p.y += v.y * dt;
  p.z += v.z * dt;
});
```

As mentioned earlier, using WithChanged we can make the iteration run only if particular components change. You can save quite a bit of performance using this technique.<br/>
```cpp
ecs::EntityQuery q;
q.All<Position, Velocity>(); // Take into account all chunks with Position and Velocity...
q.None<Player>();            // ... no Player component...
q.WithChanged<Velocity>();   // ... but only iterate when Velocity changes

w.ForEach(q, [&](Position& p, const Velocity& v) {
  p.x += v.x * dt;
  p.y += v.y * dt;
  p.z += v.z * dt;
});
```

Using constrains we can alter the iteration behavior:
```cpp
w.EnableEntity(e, false);

q.SetConstraint(ecs::EntityQuery::Constraint::AcceptAll);
w.ForEach(q, [](Position& p, const Velocity& v) {
  // Both enabled and disabled entities are included in the query.
  // ...
});
```

A very important thing to remember is that iterating over components not present in the query is not supported. This is done to prevent various logic errors which might sneak in otherwise.

## Chunk iteration
Iteration over chunks gives you more power as it exposes to you the underlying chunk in which your data is contained.<br/>
That means you can perform more kinds of operations, and it also opens doors for new kinds of optimizations.
```cpp
ecs::EntityQuery q;
q.All<Position,Velocity>();

w.ForEach(q, [](ecs::Chunk& ch) {
  auto p = ch.ViewRW<Position>(); // Read-write access to Position
  auto v = ch.View<Velocity>(); // Read-only access to Velocity

  // Iterate over all entities in the chunk.
  for (size_t i = 0; i < ch.GetItemCount(); ++i) {
    p[i].x += v[i].x * dt;
    p[i].y += v[i].y * dt;
    p[i].z += v[i].z * dt;
  }
});
```

I need to make a small but important note here. Analyzing the output of different compilers I quickly realized if you want your code vectorized for sure you need to be very clear and write the loop as a lambda or kernel if you will. It is quite surprising to see this but even with optimizations on and "fast-math"-like switches enabled some compilers simply will not vectorize the loop otherwise. Microsoft compilers are particularly sensitive in this regard. In the years to come maybe this gets better but for now, keep it in mind or use a good optimizing compiler such as Clang.
```cpp
w.ForEach(q, [](ecs::Chunk& ch) {
  auto vp = ch.ViewRW<Position>(); // Read-Write access to Position
  auto vv = ch.View<Velocity>(); // Read-Only access to Velocity

  // Make our intentions very clear so even compilers which are weaker at optimization can vectorize the loop
  [&](Position* GAIA_RESTRICT p, const Velocity* GAIA_RESTRICT v, const size_t size) {
    for (size_t i = 0; i < size; ++i) {
      p[i].x += v[i].x * dt;
      p[i].y += v[i].y * dt;
      p[i].z += v[i].z * dt;
    }
  }(vp.data(), vv.data(), ch.GetItemCount());
});
```

## Data layouts
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
w.ForEach(ecs::EntityQuery().All<PositionSoA,VelocitySoA>, [](ecs::Chunk& ch) {
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
    for (size_t i = 0; i < sz; ++i)
      p[i] += v[i] * dt;
    /*
    You can even use intrinsics now without a worry.
    Note, this is just a simple example not an optimal way to rewrite the loop above using intrinsics.
    for (size_t i = 0; i < sz; i+=4) {
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
A chunk component is a special kind of data which exists at most once per chunk.<br/>
In different words, you attach data to one chunk specifically.<br/>
Chunk components survive entity removals and unlike generic component they do not transfer to a new chunk along with their entity.<br/>
If you organize your data with care (which you should) this can save you some very precious memory or performance depending on your use case.<br/>

For instance, imagine you have a grid with fields of 100 meters squared.
Now if you create your entities carefully they get organized in grid fields implicitly on data level already without you having to use any sort of spatial map container.
```cpp
w.AddComponent<Position>(e1, {10,1});
w.AddComponent<Position>(e2, {19,1});
w.AddComponent<ecs::AsChunk<GridPosition>>(e1, {1, 0}); // Both e1 and e2 share a common grid position of {1,0} now
```

# Requirements

## Compiler
Gaia-ECS requires a compiler compatible with C++17.<br/>
Currently, all major compilers are supported:<br/>
- [MSVC](https://visualstudio.microsoft.com/) 15 (Visual Studio 2017) or later<br/>
- [Clang](https://clang.llvm.org/) 7 or later<br/>
- [GCC](https://www.gnu.org/software/gcc/) 7 or later<br/>

More compilers might work but the above are guaranteed and [continuosly tested](https://github.com/richardbiely/gaia-ecs/actions/workflows/build.yml).<br/>
[ICC](https://www.intel.com/content/www/us/en/developer/tools/oneapi/dpc-compiler.html#gs.2nftun) support is also [worked on](https://github.com/richardbiely/gaia-ecs/actions/workflows/icc.yml).

## Dependencies
[CMake](https://cmake.org) 3.12 or later is required to prepare the build. Other tools are officially not supported at the moment.

Unit testing is handled via [Catch2 v2.x](https://github.com/catchorg/Catch2/tree/v2.x). It can be controlled via -DGAIA_BUILD_UNITTEST=ON/OFF when configuring the project (OFF by default).<br/>
You can either install Catch2 on your machine manually or use -DGAIA_FIND_CATCH2_PACKAGE=ON/OFF when generating your build files and have CMake download and prepare the dependency for you (ON by default).

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

You can also use sanitizers with the project via -DUSE_SANITIZER.
```bash
cmake -DCMAKE_BUILD_TYPE=Release -DUSE_SANITIZER=address -S . -B "build"
```
Possible options are listed in [cmake/sanitizers.cmake](https://github.com/richardbiely/gaia-ecs/blob/main/cmake/sanitizers.cmake).<br/>
Note, some options don't work together or might not be supported by all compilers.

## Single-header library
Gaia-ECS is shipped also as a [single header file](https://github.com/richardbiely/gaia-ecs/blob/main/single_include/gaia.h) which you can simple drop into your project and start using. To generate the header we use a wonderful Python tool [Quom](https://github.com/Viatorus/quom).

In order to generate the header use the following command inside your root directory.
```bash
quom ./include/gaia.h ./single_include/gaia.h -I ./include
```

You can also used the attached make_single_header.sh or create your own script for your platfrom.

# Examples
The repository contains some code examples for guidance.<br/>
Examples are built if GAIA_BUILD_EXAMPLES is enabled when configuring the project (OFF by default).

* [External](https://github.com/richardbiely/gaia-ecs/tree/main/src/examples/example_external) - a dummy example explaining how to use the framework in an external project
* [Standalone](https://github.com/richardbiely/gaia-ecs/tree/main/src/examples/example1) - the same as the previous one but showing how Gaia-ECS is used as a standalone project
* [Basic](https://github.com/richardbiely/gaia-ecs/tree/main/src/examples/example2) - simple example using some basic framework features
* [Roguelike](https://github.com/richardbiely/gaia-ecs/tree/main/src/examples/example_rougelike) - Roguelike game putting all parts of the framework to use and represents a complex example of how everything would be used in practice; it is work-in-progress and changes and evolves with the project

# Benchmarks
To be able to reason about the project's performance benchmarks and prevent regressions benchmarks were created.<br/>
Benchmarking relies on a modified [picobench](https://github.com/iboB/picobench). It can be controlled via -DGAIA_BUILD_BENCHMARK=ON/OFF when configuring the project (OFF by default).

* [Duel](https://github.com/richardbiely/gaia-ecs/tree/main/src/perf/duel) - duel compares different coding approaches such as the basic model with uncontrolled OOP with data all-over-the heap, OOP where allocators are used to controlling memory fragmentation and different ways of data-oriented design and it puts them to test against our ECS framework itself; DOD performance is the target level we want to reach or at least be as close as possible to with this project because it does not get any faster than that 
* [Iteration](https://github.com/richardbiely/gaia-ecs/tree/main/src/perf/iter) - this benchmark focuses on the performance of creating and removing entities and components of various sizes and also covers iteration performance with different numbers of entities and archetypes

# Profiling
It is possible to measure performance and memory usage of the gramework via any 3rd party tool. However, support for [Tracy](https://github.com/wolfpld/tracy) is added by default.<br/>
The CPU part can be controlled via -DGAIA_PROF_CPU=ON/OFF (OFF by default) while -DGAIA_PROF_MEM=ON/OFF is responsible for profiling memory allocations (OFF by default). If you want to build the profiler server yourself you can use -DGAIA_PROF_CPU=ON (OFF by default).<br/>
This is a low-level feature mostly targeted for maintainers. However, if paired with your own profiler code it can become a very helpful tool.

# Future
Currently, many new features and improvements to the current system are planned.<br/>
Among the most prominent ones those are:
* scheduler - a system that would allow parallel execution of all systems by default, work stealing, and an easy setup of dependencies
* scenes - a way to serialize the state of chunks or entire worlds
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

