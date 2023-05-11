<!--
@cond TURN_OFF_DOXYGEN
-->
# Gaia-ECS
[![Build Status][badge.actions]][actions]
[![language][badge.language]][language]
[![license][badge.license]][license]
[![codacy][badge.codacy]][codacy]

[badge.actions]: https://github.com/richardbiely/gaia-ecs/workflows/build/badge.svg
[badge.language]: https://img.shields.io/badge/language-C%2B%2B17-yellow
[badge.license]: https://img.shields.io/badge/license-MIT-blue
[badge.codacy]: https://app.codacy.com/project/badge/Grade/bb28fa362fce4054bbaf7a6ba9aed140

[actions]: https://github.com/richardbiely/gaia-ecs/actions
[language]: https://en.wikipedia.org/wiki/C%2B%2B17
[license]: https://en.wikipedia.org/wiki/MIT_License
[codacy]: https://app.codacy.com/gh/richardbiely/gaia-ecs/dashboard?utm_source=gh&utm_medium=referral&utm_content=&utm_campaign=Badge_grade

Gaia-ECS is a fast and easy to use [entity component system](https://en.wikipedia.org/wiki/Entity_component_system) framework. Some of its current features and highlights are:
* very simple and safe API designed in such a way to prevent you from using bad coding patterns
* based on modern [C++17](https://en.cppreference.com/w/cpp/17) technologies
* no external dependencies, not even on STL (can be easily enabled if needed)
* compiles almost instantly
* archetype / chunk-based storage for maximum iteration speed and easy code parallelization
* ability to [organize data as AoS or SoA](#data-layouts) on the component level with very little changes to your code
* compiled and tested on all major compilers [continuously](https://github.com/richardbiely/gaia-ecs/actions)
* stability secured by running thousands of [unit tests](#testing)
* exists also as a [single-header](#single-header) library so you can simply drop it into your project and start using it

# Table of Contents

* [Introduction](#introduction)
* [Usage](#usage)
  * [Minimum requirements](#minimum-requirements)
  * [Basic operations](#basic-operations)
    * [Create or delete entity](#create-or-delete-entity)
    * [Enable or disable entity](#enable-or-disable-entity)
    * [Add or remove component](#add-or-remove-component)
    * [Set or get component value](#set-or-get-component-value)
    * [Component presence](#component-presence)
  * [Data processing](#data-processing)
    * [Query](#query)
    * [Simple iteration](#simple-iteration)
    * [Query iteration](#query-iteration)
    * [Iterator](#iterator)
    * [Change detection](#change-detection)
  * [Chunk components](#chunk-components)
  * [Delayed execution](#delayed-execution)
  * [Data layouts](#data-layouts)
* [Requirements](#requirements)
  * [Compiler](#compiler)
  * [Dependencies](#dependencies)
* [Installation](#installation)
  * [CMake](#cmake)
    * [Project settings](#project-settings)
    * [Sanitizers](#sanitizers)
    * [Single-header](#single-header)
* [Project structure](#project-structure)
  * [Examples](#examples)
  * [Benchmarks](#benchmarks)
  * [Profiling](#profiling)
  * [Testing](#testing)
* [Future](#future)
* [Contributions](#contributions)
* [License](#license)

# Introduction

[Entity-Component-System (ECS)](https://en.wikipedia.org/wiki/Entity_component_system) is a software architectural pattern based on organizing your code around data which follows the principle of [composition over inheritance](https://en.wikipedia.org/wiki/Composition_over_inheritance).

Instead of looking at "items" in your program as objects you normally know from the real world (car, house, human) you look at them as pieces of data necessary for you to achieve some result.

This way of thinking is more natural for machines than people but when used correctly it allows you to write faster code (on most architectures). What is most important, however, is it allows you to write code that is easier to maintain, expand and reason about.

For instance, when moving some object from point A to point B you do not care if it is a house or a car. You only care about its' position. If you want to move it at some specific speed you will consider also the object's velocity. Nothing else is necessary.

Three building blocks of ECS are:
* **Entity** - index that uniquely identifies a group of components
* **Component** - piece of data (position, velocity, age)
* **System** - place where your program's logic is implemented

Gaia-ECS is an archetype-based entity component system. Therefore, unique combinations of components are grouped into archetypes. Each archetype consists of chunks - blocks of memory holding your entities and components. You can think of them as SQL tables where components are columns and entities are rows. In our case, each chunk is 16 KiB big. This size is chosen so that the entire chunk can fit into L1 cache on most CPUs.
>**NOTE:**<br/>If needed you can alter the size of chunks by modifying the value of ***ecs::MemoryBlockSize***.

Chunk memory is preallocated in big blocks (pages) via the internal chunk allocator. Thanks to that all data is organized in a cache-friendly way which most computer architectures like and actual heap allocations which are slow are reduced to a minimum.

The main benefits of archetype-based architecture is fast iteration and good memory layout by default. They are also easy to parallelize. On the other hand, adding and removing components can be somewhat slower because it involves moving data around. In our case this weakness is mitigated by a graph built for archetypes.

# Usage
## Minimum requirements

```cpp
#include <gaia.h>
```

The entire framework is placed in a namespace called **gaia**.
The ECS part of the library is found under **gaia::ecs** namespace.<br/>
In code examples bellow we will assume we are inside the gaia namespace.

## Basic operations
### Create or delete entity

```cpp
ecs::World w;
auto e = w.CreateEntity();
... // do something with the created entity
w.DeleteEntity(e);
```

### Enable or disable entity
Disabled entities are moved to chunks different from the rest. Because of that they do not take part in queries by default.

Behavior of EnableEntity is similar to that of calling DeleteEntity/CreateEntity. However, the main benefit is that a disabled entity keeps its ID intact which means you can reference it freely.

```cpp
ecs::World w;
auto e = w.CreateEntity();
w.EnableEntity(e, false); // disable the entity
w.EnableEntity(e, true); // enable the entity again
```

### Add or remove component

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

### Set or get component value

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
// Read Velocity's value. As shown above Velocity is 12 bytes in size. Therefore, it is returned by const reference.
const auto& velRef = w.GetComponent<Velocity>(e);
// However, it is easy to store a copy.
auto velCopy = w.GetComponent<Velocity>(e);
```

Both read and write operations are also accessible via views. Check [simple iteration](#simple-iteration) and [query iteration](#query-iteration) sections to see how.

### Component presence
Whether or not a certain component is associated with an entity can be checked in two different ways. Either via an instance of a World object or by the means of Iterator which can be aquired when running queries.

```cpp
// Check if entity e has Velocity (via world).
const bool hasVelocity = w.HasComponent<Velocity>(e);
...

// Check if entities hidden behind the iterator have Velocity (via iterator).
ecs::Query q = w.CreateQuery().Any<Position, Velocity>(); 
q.ForEach([&](ecs::Iterator iter){
  const bool hasPosition = iter.HasComponent<Position>();
  const bool hasVelocity = iter.HasComponent<Velocity>();
  ...
});
```

## Data processing
### Query
For querying data you can use a Query. It can help you find all entities, components or chunks matching the specified set of components and constraints and iterate it or returns in the form of an array. You can also use them to quickly check if any entities satisfying your requirements exist or calculate how many of them there are.

>**NOTE:**<br/>Every Query creates a cache internally. Therefore, the first usage is a little bit slower than the subsequent usage is going to be. You likely use the same query multiple times in your program, often without noticing. Because of that, caching becomes useful as it avoids wasting memory and performance when finding matches.

```cpp
Query q = w.CreateQuery();
q.All<Position>(); // Consider only entities with Position

// Fill the entities array with entities with a Position component.
containers::darray<ecs::Entity> entities;
q.ToArray(entities);

// Fill the positions array with position data.
containers::darray<Position> positions;
q.ToArray(positions);

// Calculate the number of entities satisfying the query
const auto numberOfMatches = q.CalculateEntityCount();
```

More complex queries can be created by combining All, Any and None in any way you can imagine:

```cpp
ecs::Query q = w.CreateQuery();
q.All<Position, Velocity>(); // Take into account everything with Position and Velocity...
q.Any<Something, SomethingElse>(); // ... at least Something or SomethingElse...
q.None<Player>(); // ... and no Player component...
```

All Query operations can be chained and it is also possible to invoke various filters multiple times with unique components:

```cpp
ecs::Query q = w.CreateQuery();
q.All<Position>() // Take into account everything with Position...
 .All<Velocity>() // ... and at the same time everything with Velocity...
 .Any<Something, SomethingElse>() // ... at least Something or SomethingElse...
 .None<Player>(); // ... and no Player component...
```

Query behavior can also be modified by setting constraints. By default only enabled entities are taken into account. However, by changing constraints we can filter disabled entities exclusively or make the query consider both enabled and disabled entities at the same time:

```cpp
ecs::Entity e1, e2;

// Create 2 entities with Position component
w.CreateEntity(e1);
w.CreateEntity(e2);
w.AddComponent<Position>(e1);
w.AddComponent<Position>(e2);

// Disable the first entity
w.EnableEntity(e1, false);

ecs::Query q = w.CreateQuery().All<Position>();
containers::darray<ecs::Entity> entities;

// Fills the array with only e2 because e1 is disabled.
q.ToArray(entities);

// Fills the array with both e1 and e2.
q.SetConstraint(ecs::Query::Constraint::AcceptAll);
q.ToArray(entities);

// Fills the array with only e1 because e1 is disabled.
q.SetConstraint(ecs::Query::Constraint::DisabledOnly);
q.ToArray(entities);
```

### Simple iteration
The simplest way to iterate over data is using ecs::World::ForEach.<br/>
It provides the least room for optimization (that does not mean the generated code is slow by any means) but is very easy to read.

```cpp
w.ForEach([&](Position& p, const Velocity& v) {
  p.x += v.x * dt;
  p.y += v.y * dt;
  p.z += v.z * dt;
});
```

>**NOTE:**<br/>This creates a Query internally from the arguments provided to ForEach.

### Query iteration
For possibly better performance and more features, consider using explicit Query when possible.

```cpp
ecs::Query q = w.CreateQuery();
q.All<Position, const Velocity>(); // Take into account all entities with Position and Velocity...
q.None<Player>(); // ... but no Player component.

q.ForEach([&](Position& p, const Velocity& v) {
  // This operations runs for each entity with Position, Velocity and no Player component
  p.x += v.x * dt;
  p.y += v.y * dt;
  p.z += v.z * dt;
});
```

>**NOTE:**<br/>Iterating over components not present in the query is not supported and results in asserts and undefined behavior. This is done to prevent various logic errors which might sneak in otherwise.

### Iterator
Iteration using the iterator gives you even more expresive power and also opens doors for new kinds of optimizations. Iterator are an abstraction above chunk which is responsible for holding entities and components.

```cpp
ecs::Query q = w.CreateQuery();
q.All<Position, const Velocity>();

q.ForEach([](ecs::Iterator iter) {
  auto p = iter.ViewRW<Position>(); // Read-write access to Position
  auto v = iter.View<Velocity>(); // Read-only access to Velocity

  // Iterate over all entities in the chunk.
  for (uint32_t i: iter) {
    p[i].x += v[i].x * dt;
    p[i].y += v[i].y * dt;
    p[i].z += v[i].z * dt;
  }
});
```

>**NOTE:**<br/>
Analyzing the output of different compilers I quickly realized if you want your code vectorized for sure you need to be very clear and write the loop as a lambda or kernel if you will. It is quite surprising to see this but even with optimizations on and ***-fast_math***-like switches enabled some compilers simply will not vectorize the loop otherwise. Microsoft compilers are particularly sensitive in this regard. In the years to come maybe this gets better but for now, keep it in mind or use a good optimizing compiler such as Clang.

```cpp
q.ForEach([](ecs::Iterator iter) {
  auto vp = iter.ViewRW<Position>(); // Read-write access to Position
  auto vv = iter.View<Velocity>(); // Read-only access to Velocity

  // Make our intentions very clear so even compilers which are weaker at optimization can vectorize the loop
  [&](Position* p, const Velocity* v, uint32_t size) {
    for (uint32_t i = 0; i < size; ++i) {
      p[i].x += v[i].x * dt;
      p[i].y += v[i].y * dt;
      p[i].z += v[i].z * dt;
    }
  }(vp.data(), vv.data(), iter.size());
});
```

### Change detection
Using WithChanged we can make the iteration run only if particular components change. You can save quite a bit of performance using this technique.

```cpp
ecs::Query q = w.CreateQuery();
q.All<Position, const Velocity>(); // Take into account all entities with Position and Velocity...
q.None<Player>(); // ... no Player component...
q.WithChanged<Velocity>(); // ... but only iterate when Velocity changes

q.ForEach([&](Position& p, const Velocity& v) {
  // This operations runs for each entity with Position, Velocity and no Player component but ONLY when Velocity has changed
  p.x += v.x * dt;
  p.y += v.y * dt;
  p.z += v.z * dt;
});
```

>**NOTE:**<br/>If there are 100 Position components in the chunk and only one them changes, the other 99 are considered changed as well. This chunk-wide behavior might seem counter-intuitive but it is in fact a performance optimization. The reason why this works is because it is easier to reason about a group of entities than checking each of them separately.

## Chunk components
A chunk component is a special kind of data which exists at most once per chunk. In different words, you attach data to one chunk specifically. It survives entity removals and unlike generic components they do not transfer to a new chunk along with their entity.

If you organize your data with care (which you should) this can save you some very precious memory or performance depending on your use case.

For instance, imagine you have a grid with fields of 100 meters squared.
Now if you create your entities carefully they get organized in grid fields implicitly on data level already without you having to use any sort of spatial map container.

```cpp
w.AddComponent<Position>(e1, {10,1});
w.AddComponent<Position>(e2, {19,1});
w.AddComponent<ecs::AsChunk<GridPosition>>(e1, {1, 0}); // Both e1 and e2 share a common grid position of {1,0} now
```

## Delayed execution
Sometimes you need to delay executing a part of the code for later. This can be achieved via CommandBuffers.

CommandBuffer is a container used to record commands in the order in which they were requested at a later point in time.

Typically you use them when there is a need to perform structural changes (adding or removing an entity or component) while iterating chunks.

Performing an unprotected structural change is undefined behavior and most likely crashes the program. However, using a CommandBuffer you can collect all requests first and commit them when it is safe.

```cpp
ecs::CommandBuffer cb;
q.ForEach([&](Entity e, const Position& p) {
  if (p.y < 0.0f)
    cb.DeleteEntity(e); // queue entity e for deletion if its position falls below zero
});
cb.Commit(&w); // after calling this all entities with y position bellow zero get deleted
```

If you try to make an unprotected structural change with GAIA_DEBUG enabled (set by default when Debug configuration is used) the framework will assert letting you know you are using it the wrong way.

## Data layouts
By default, all data inside components is treated as an array of structures (AoS) via an implicit

```cpp
static constexpr auto Layout = utils::DataLayout::AoS
```

This is the natural behavior of the language and what you would normally expect.

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

ecs::Query q = w.Query().All<PositionSoA, const VelocitySoA>;
q.ForEach([](ecs::Iterator iter) {
  // Position
  auto vp = iter.ViewRW<PositionSoA>(); // read-write access to PositionSoA
  auto px = vp.set<0>(); // continuous block of "x" from PositionSoA
  auto py = vp.set<1>(); // continuous block of "y" from PositionSoA
  auto pz = vp.set<2>(); // continuous block of "z" from PositionSoA

  // Velocity
  auto vv = iter.View<VelocitySoA>(); // read-only access to VelocitySoA
  auto vx = vv.get<0>(); // continuous block of "x" from VelocitySoA
  auto vy = vv.get<1>(); // continuous block of "y" from VelocitySoA
  auto vz = vv.get<2>(); // continuous block of "z" from VelocitySoA

  // The loop becomes very simple now.
  auto exec = [&](float* GAIA_RESTRICT p, const float* GAIA_RESTRICT v, uint32_t size) {
    for (uint32_t i = 0; i < size; ++i)
      p[i] += v[i] * dt;
    /*
    You can even use SIMD intrinsics now without a worry. The code bellow uses x86 SIMD intrinsics.
    Note, this is just a simple example not an optimal way to rewrite the loop. Also, this could would normally get auto-vectorized on most compilers in release builds.
    for (uint32_t i = 0; i < size; i+=4) {
      const auto pVec = _mm_load_ps(p + i);
      const auto vVec = _mm_load_ps(v + i);
      const auto respVec = _mm_fmadd_ps(vVec, dtVec, pVec);
      _mm_store_ps(p + i, respVec);
    }*/
  };
  // Handle x coordinates
  exec(px.data(), vx.data(), iter.size());
  // Handle y coordinates
  exec(py.data(), vy.data(), iter.size());
  // Handle z coordinates
  exec(pz.data(), vz.data(), iter.size());
});
```

# Requirements

## Compiler
Compiler compatible with a good support of C++17 is required.<br/>
The project is [continuosly tested](https://github.com/richardbiely/gaia-ecs/actions/workflows/build.yml) and guaranteed to build warning-free on the following compilers:<br/>
- [MSVC](https://visualstudio.microsoft.com/) 15 (Visual Studio 2017) or later<br/>
- [Clang](https://clang.llvm.org/) 7 or later<br/>
- [GCC](https://www.gnu.org/software/gcc/) 7 or later<br/>

More compilers might work but are not supported out-of-the-box. Support for [ICC](https://www.intel.com/content/www/us/en/developer/tools/oneapi/dpc-compiler.html#gs.2nftun) is  [worked on](https://github.com/richardbiely/gaia-ecs/actions/workflows/icc.yml).

## Dependencies
[CMake](https://cmake.org) 3.12 or later is required to prepare the build. Other tools are officially not supported at the moment.

Unit testing is handled via [Catch2 v3.3.2](https://github.com/catchorg/Catch2/releases/tag/v3.3.2). It can be controlled via -DGAIA_BUILD_UNITTEST=ON/OFF when configuring the project (OFF by default).

# Installation

## CMake

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

To target a specific build system you can use the [***-G*** parameter](https://cmake.org/cmake/help/latest/manual/cmake-generators.7.html#manual:cmake-generators(7)):
```bash
# Microsoft Visual Studio 2022, 64-bit, x86 architecture 
cmake -G "Visual Studio 17 2022" -A x64 ...
# Microsoft Visual Studio 2022, 64-bit, ARM architecture 
cmake -G "Visual Studio 17 2022" -A ARM64 ...
# XCode
cmake -G XCode ...
# Ninja
cmake -G Ninja
```

### Project settings
Following is a list of parameters you can use to customize your build

Parameter | Description      
-|-
**GAIA_BUILD_UNITTEST** | Builds the [unit test project](#unit-testing)
**GAIA_BUILD_BENCHMARK** | Builds the [benchmark project](#benchmarks)
**GAIA_BUILD_EXAMPLES** | Builds [example projects](#examples)
**GAIA_GENERATE_CC** | Generates ***compile_commands.json***
**GAIA_MAKE_SINGLE_HEADER** | Generates a [single-header](#single-header-library) version of the framework
**GAIA_PROFILER_CPU** | Enables CPU [profiling](#profiling) features
**GAIA_PROFILER_MEM** | Enabled memory [profiling](#profiling) features
**GAIA_PROFILER_BUILD** | Builds the [profiler](#profiling) ([Tracy](https://github.com/wolfpld/tracy) by default)
**USE_SANITIZER** | Applies the specified set of [sanitizers](#sanitizers)

### Sanitizers
Possible options are listed in [cmake/sanitizers.cmake](https://github.com/richardbiely/gaia-ecs/blob/main/cmake/sanitizers.cmake).<br/>
Note, some options don't work together or might not be supported by all compilers.
```bash
cmake -DCMAKE_BUILD_TYPE=Release -DUSE_SANITIZER=address -S . -B "build"
```

### Single-header
Gaia-ECS is shipped also as a [single header file](https://github.com/richardbiely/gaia-ecs/blob/main/single_include/gaia.h) which you can simple drop into your project and start using. To generate the header we use a wonderful Python tool [Quom](https://github.com/Viatorus/quom).

In order to generate the header use the following command inside your root directory.
```bash
quom ./include/gaia.h ./single_include/gaia.h -I ./include
```

You can also used the attached make_single_header.sh or create your own script for your platfrom.

Creation of the single-header can be automated via -GAIA_MAKE_SINGLE_HEADER.

# Project structure

## Examples
The repository contains some code examples for guidance.<br/>
Examples are built if GAIA_BUILD_EXAMPLES is enabled when configuring the project (OFF by default).

Project name | Description      
-|-
[External](https://github.com/richardbiely/gaia-ecs/tree/main/src/examples/example_external)|A dummy example showing how to use the framework in an external project.
[Standalone](https://github.com/richardbiely/gaia-ecs/tree/main/src/examples/example1)|A dummy example showing how to use the framework in a standalone project.
[Basic](https://github.com/richardbiely/gaia-ecs/tree/main/src/examples/example2)|Simple example using some basic features of the framework.
[Roguelike](https://github.com/richardbiely/gaia-ecs/tree/main/src/examples/example_roguelike)|Roguelike game putting all parts of the framework to use and represents a complex example of how everything would be used in practice. It is work-in-progress and changes and evolves with the project.

## Benchmarks
To be able to reason about the project's performance benchmarks and prevent regressions benchmarks were created.

Benchmarking relies on a modified [picobench](https://github.com/iboB/picobench). It can be controlled via -DGAIA_BUILD_BENCHMARK=ON/OFF when configuring the project (OFF by default).

Project name | Description      
-|-
[Duel](https://github.com/richardbiely/gaia-ecs/tree/main/src/perf/duel)|Compares different coding approaches such as the basic model with uncontrolled OOP with data all-over-the heap, OOP where allocators are used to controlling memory fragmentation and different ways of data-oriented design and it puts them to test against our ECS framework itself. DOD performance is the target level we want to reach or at least be as close as possible to with this project because it does not get any faster than that.
[Iteration](https://github.com/richardbiely/gaia-ecs/tree/main/src/perf/iter)|Covers iteration performance with different numbers of entities and archetypes.
[Entity](https://github.com/richardbiely/gaia-ecs/tree/main/src/perf/entity)|Focuses on performance of creating and removing entities and components of various sizes.

## Profiling
It is possible to measure performance and memory usage of the gramework via any 3rd party tool. However, support for [Tracy](https://github.com/wolfpld/tracy) is added by default.

CPU part can be controlled via -DGAIA_PROF_CPU=ON/OFF (OFF by default).

Memory part can be controlle via -DGAIA_PROF_MEM=ON/OFF (OFF by default).

Building the profiler server can be controlled via -DGAIA_PROF_CPU=ON (OFF by default).
>**NOTE:<br/>** This is a low-level feature mostly targeted for maintainers. However, if paired with your own profiler code it can become a very helpful tool.

## Unit testing
The project is thorougly unit-tested and includes  thousand of unit test covering essentially every feature of the framework. Benchmarking relies on a modified [picobench](https://github.com/iboB/picobench).

It can be controlled via -DGAIA_BUILD_UNITTEST=ON/OFF (OFF by default).

# Future
Currently, many new features and improvements to the current system are planned.<br/>
Among the most prominent ones those are:
* scheduler - a system that would allow parallel execution of all systems by default, work stealing, and an easy setup of dependencies
* scenes - a way to serialize the state of chunks or entire worlds
* scripting - expose low-level structures of the framework so it can be implemented by various other languages including scripting ones
* debugger - an editor that would give one an overview of worlds created by the framework (number of entities, chunk fragmentation, systems running, etc.)

# Contributions

Requests for features, PRs, suggestions, and feedback are highly appreciated.

If you find you can help and want to contribute to the project feel free to contact
me directly (you can find the mail on my [profile page](https://github.com/richardbiely)).

# License

Code and documentation Copyright (c) 2021-2023 Richard Biely.

Code released under
[the MIT license](https://github.com/richardbiely/gaia-ecs/blob/master/LICENSE).
<!--
@endcond TURN_OFF_DOXYGEN
-->

