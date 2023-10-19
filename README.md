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

Gaia-ECS is a fast and easy-to-use [ECS](#ecs) framework. Some of its current features and highlights are:
* very simple and safe API designed in such a way as to prevent you from using bad coding patterns
* based on modern [C++17](https://en.cppreference.com/w/cpp/17) technologies
* no external dependencies
* archetype / chunk-based storage for maximum iteration speed and easy code parallelization
* ability to [organize data as AoS or SoA](#data-layouts) on the component level with very few changes to your code
* compiles warning-free on [all major compilers](https://github.com/richardbiely/gaia-ecs/actions)
* compiles almost instantly
* stability secured by running thousands of [unit tests]
* thoroughly documented both public and internal code
* exists also as a [single-header](#single-header) library so you can simply drop it into your project and start using it

# Table of Contents

* [Introduction](#introduction)
  * [ECS](#ecs)
  * [Implementation](#implementation)
* [Usage](#usage)
  * [Minimum requirements](#minimum-requirements)
  * [Basic operations](#basic-operations)
    * [Create or delete entity](#create-or-delete-entity)
    * [Add or remove component](#add-or-remove-component)
    * [Set or get component value](#set-or-get-component-value)
    * [Component presence](#component-presence)
  * [Data processing](#data-processing)
    * [Query](#query)
    * [Simple iteration](#simple-iteration)
    * [Query iteration](#query-iteration)
    * [Iterator](#iterator)
    * [Constraints](#constraints)
    * [Change detection](#change-detection)
  * [Chunk components](#chunk-components)
  * [Delayed execution](#delayed-execution)
  * [Data layouts](#data-layouts)
  * [Serialization](#serialization)
  * [Multithreading](#multithreading)
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

## ECS
[Entity-Component-System (ECS)](https://en.wikipedia.org/wiki/Entity_component_system) is a software architectural pattern based on organizing your code around data which follows the principle of [composition over inheritance](https://en.wikipedia.org/wiki/Composition_over_inheritance).

Instead of looking at "items" in your program as objects you normally know from the real world (car, house, human) you look at them as pieces of data necessary for you to achieve some result.

This way of thinking is more natural for machines than people but when used correctly it allows you to write faster code (on most architectures). What is most important, however, is it allows you to write code that is easier to maintain, expand and reason about.

For instance, when moving some object from point A to point B you do not care if it is a house or a car. You only care about its position. If you want to move it at some specific speed you will consider also the object's velocity. Nothing else is necessary.

Three building blocks of ECS are:
* **Entity** - an index that uniquely identifies a group of components
* **Component** - a piece of data (position, velocity, age)
* **System** - a place where your program's logic is implemented

Following the example given above, a vehicle could be anything with Position and Velocity components. If it is a car we could attach the Driving component to it. If it is an airplane we would attach the Flying component.<br/>
The actual movement is handled by systems. Those that match against the Flying component will implement the logic for flying. Systems matching against the Driving component handle the land movement.

## Implementation
Gaia-ECS is an archetype-based entity component system. This means that unique combinations of components are grouped into archetypes. Each archetype consists of chunks - blocks of memory holding your entities and components. You can think of them as [database tables](https://en.wikipedia.org/wiki/Table_(database)) where components are columns and entities are rows. Each chunk is either 8 or 16 KiB big depending on how much data can be effectively used by it. This size is chosen so that the entire chunk at its fullest can fit into the L1 cache on most CPUs.

Chunk memory is preallocated in blocks organized into pages via the internal chunk allocator. Thanks to that all data is organized in a cache-friendly way which most computer architectures like and actual heap allocations which are slow are reduced to a minimum.

The main benefits of archetype-based architecture are fast iteration and good memory layout by default. They are also easy to parallelize. On the other hand, adding and removing components can be somewhat slower because it involves moving data around. In our case, this weakness is mitigated by building an archetype graph.

# Usage
## Minimum requirements

```cpp
#include <gaia.h>
```

The entire framework is placed in a namespace called **gaia**.
The ECS part of the library is found under **gaia::ecs** namespace.<br/>
In the code examples below we will assume we are inside the namespace already.

## Basic operations
### Create or delete entity

```cpp
ecs::World w;
auto e = w.Add();
... // do something with the created entity
w.Del(e);
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
auto e = w.Add();
w.Add<Position>(e, {0, 100, 0});
w.Add<Velocity>(e, {0, 0, 1});

// Remove Velocity from the entity.
w.Del<Velocity>(e);
```

### Set or get component value

```cpp
// Change Velocity's value.
w.Set<Velocity>(e, {0, 0, 2});
// Same as above but the world version is not updated so nobody gets notified of this change.
w.SetSilent<Velocity>(e, {4, 2, 0});
```

In case there are more different components on the entity you would like to change the value you can achieve it via Set chaining:

```cpp
// Change Velocity's value on entity "e"
w.Set<Velocity>(e, {0, 0, 2}).
// Change Position's value on entity "e"
  Set<Position>({0, 100, 0}).
// Change...
  Set...;
```

Components are returned by value for components with sizes up to 8 bytes (including). Bigger components are returned by const reference.

```cpp
// Read Velocity's value. As shown above Velocity is 12 bytes in size. Therefore, it is returned by const reference.
const auto& velRef = w.Get<Velocity>(e);
// However, it is easy to store a copy.
auto velCopy = w.Get<Velocity>(e);
```

Both read and write operations are also accessible via views. Check [simple iteration](#simple-iteration) and [query iteration](#query-iteration) sections to see how.

### Component presence
Whether or not a certain component is associated with an entity can be checked in two different ways. Either via an instance of a World object or by the means of ***Iterator*** which can be acquired when running [queries](#query).

```cpp
// Check if entity e has Velocity (via world).
const bool hasVelocity = w.Has<Velocity>(e);
...

// Check if entities hidden behind the iterator have Velocity (via iterator).
ecs::Query q = w.CreateQuery().Any<Position, Velocity>(); 
q.ForEach([&](ecs::Iterator iter) {
  const bool hasPosition = iter.Has<Position>();
  const bool hasVelocity = iter.Has<Velocity>();
  ...
});
```

## Data processing
### Query
For querying data you can use a Query. It can help you find all entities, components or chunks matching the specified set of components and constraints and iterate or return them in the form of an array. You can also use them to quickly check if any entities satisfying your requirements exist or calculate how many of them there are.

>**NOTE:**<br/>Every Query creates a cache internally. Therefore, the first usage is a little bit slower than the subsequent usage is going to be. You likely use the same query multiple times in your program, often without noticing. Because of that, caching becomes useful as it avoids wasting memory and performance when finding matches.

```cpp
Query q = w.CreateQuery();
q.All<Position>(); // Consider only entities with Position

// Fill the entities array with entities with a Position component.
cnt::darray<ecs::Entity> entities;
q.ToArray(entities);

// Fill the positions array with position data.
cnt::darray<Position> positions;
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

All queries are cached by default. This makes sense for queries which happen very often. They are fast to process but might take more time to prepare initially. If caching is not needed you should use uncached queries and save some resources. You would normally do this for one-time initializations or rarely used operations.

```cpp
// Create an uncache query taking into account all entities with either Positon or Velocity components
ecs::QueryUncached q = w.CreateQuery<false>().Any<Position, Velocity>(); 
q.ForEach([&](ecs::Iterator iter) {
  ...
});
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
Iteration using the iterator gives you even more expressive power and also opens doors for new kinds of optimizations. Iterator is an abstraction above archetype chunks responsible for holding entities and components.

```cpp
ecs::Query q = w.CreateQuery();
q.All<Position, const Velocity>();

q.ForEach([](ecs::Iterator iter) {
  auto p = iter.ViewRW<Position>(); // Read-write access to Position
  auto v = iter.View<Velocity>(); // Read-only access to Velocity

  // Iterate over all enabled entities and update their add 1.f to their x-axis position
  for (auto i: iter) {
    if (!iter.IsEnabled(i))
      continue;
    p[i].x += 1.f;
  }

  // Iterate over all entities in the chunk and update their position based on their velocity
  for (auto i: iter) {
    p[i].x += v[i].x * dt;
    p[i].y += v[i].y * dt;
    p[i].z += v[i].z * dt;
  }
});
```

### Constraints

Query behavior can also be modified by setting constraints. By default, only enabled entities are taken into account. However, by changing constraints, we can filter disabled entities exclusively or make the query consider both enabled and disabled entities at the same time.

Changing the enabled state of an entity is a special operation that marks the entity as invisible to queries by default. Archetype of the entity is not changed and therefore the operation is very fast (basically, just setting a flag).

```cpp
ecs::Entity e1, e2;

// Create 2 entities with Position component
w.Add(e1);
w.Add(e2);
w.Add<Position>(e1);
w.Add<Position>(e2);

// Disable the first entity
w.Enable(e1, false);

// Check if e1 is enabled
const bool is_e1_enabled = w.IsEnabled(e1);
if (is_e1_enabled) { ... }

// Prepare out query
ecs::Query q = w.CreateQuery().All<Position>();

// Fills the array with only e2 because e1 is disabled.
cnt::darray<ecs::Entity> entities;
q.ToArray(entities);

// Fills the array with both e1 and e2.
q.ToArray(entities, ecs::Query::Constraint::AcceptAll);

// Fills the array with only e1 because e1 is disabled.
q.ToArray(entities, ecs::Query::Constraint::DisabledOnly);

q.ForEach([](ecs::Iterator iter) {
  auto p = iter.ViewRW<Position>(); // Read-Write access to Position
  // Iterates over all entities
  for (auto i: iter) {
    if (iter.IsEnabled(i)) {
      p[i] = {}; // reset the position of each enabled entity
    }
  }
});
q.ForEach([](ecs::IteratorDisabled iter) {
  auto p = iter.ViewRW<Position>(); // Read-Write access to Position
  // Iterates only over disabled entities
  for (auto i: iter) {
    p[i] = {}; // reset the position of each disabled entity
  }
});
q.ForEach([](ecs::IteratorEnabled iter) {
  auto p = iter.ViewRW<Position>(); // Read-Write access to Position
  // Iterates only over enabled entities
  for (auto i: iter) {
    p[i] = {}; // reset the position of each enabled entity
  }
});
```

>**NOTE:<br/>**
***IteratorEnabled*** and ***IteratorDisabled*** don't give you access to ***Views*** nor give you access to indices. This is because both enabled and disabled entities are stored in the same chunk. Views would allow you access to data beyond the scope of constraints. If you need more flexibility, that is what ***Iterator*** is for.

If you do not wish to fragment entities inside the chunk you can simply create a tag component and assign it to your entity. This will move the entity to a new archetype so it is a lot slower. However, because disabled entities are now clearly separated calling some query operations might be slightly faster (no need to check if the entity is disabled or not internally).

```cpp
struct Disabled {};

...

e.Add<Disabled>(); // disable entity

ecs::Query q = w.CreateQuery().All<Position, Disabled>; 
q.ForEach([&](ecs::Iterator iter){
  // Processes all disabled entities
});

e.Del<Disabled>(); // enable entity
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
A chunk component is a special kind of data that exists at most once per chunk. In other words, you attach data to one chunk specifically. It survives entity removals and unlike generic components, they do not transfer to a new chunk along with their entity.

If you organize your data with care (which you should) this can save you some very precious memory or performance depending on your use case.

For instance, imagine you have a grid with fields of 100 meters squared.
Now if you create your entities carefully they get organized in grid fields implicitly on the data level already without you having to use any sort of spatial map container.

```cpp
w.Add<Position>(e1, {10,1});
w.Add<Position>(e2, {19,1});
w.Add<ecs::AsChunk<GridPosition>>(e1, {1, 0}); // Both e1 and e2 share a common grid position of {1,0} now
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
    cb.Del(e); // queue entity e for deletion if its position falls below zero
});
cb.Commit(&w); // after calling this all entities with y position bellow zero get deleted
```

If you try to make an unprotected structural change with GAIA_DEBUG enabled (set by default when Debug configuration is used) the framework will assert letting you know you are using it the wrong way.

## Data layouts
By default, all data inside components are treated as an array of structures (AoS) via an implicit

```cpp
static constexpr auto Layout = mem::DataLayout::AoS
```

This is the natural behavior of the language and what you would normally expect.

If we imagine an ordinary array of 4 Position components defined above with this layout they are organized like this in memory: xyz xyz xyz xyz.

However, in specific cases, you might want to consider organizing your component's internal data as structure or arrays (SoA):

```cpp
static constexpr auto Layout = mem::DataLayout::SoA
```

Using the example above will make Gaia-ECS treat Position components like this in memory: xxxx yyyy zzzz.

If used correctly this can have vast performance implications. Not only do you organize your data in the most cache-friendly way this usually also means you can simplify your loops which in turn allows the compiler to optimize your code better.

```cpp
struct PositionSoA {
  float x, y, z;
  static constexpr auto Layout = mem::DataLayout::SoA;
};
struct VelocitySoA {
  float x, y, z;
  static constexpr auto Layout = mem::DataLayout::SoA;
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
    Note, this is just an example not an optimal way to rewrite the loop.
    Also, most compilers will auto-vectorize this code in release builds anyway.
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

## Serialization
Serialization of arbitrary data is available via following functions:
- ***ser::size_bytes*** - calculates how many bytes the data needs to serialize
- ***ser::save*** - writes data to serialization buffer
- ***ser::load*** - loads data from serialization buffer

Any data structure can be serialized at compile-time into to provided serialization buffer. Native types, compound types, arrays and anything with data() + size() functions are supported out-of-the-box. If resize() is available it will be utilized as well.

Example:
```cpp
struct Position {
  float x, y, z;
};
struct Quaternion {
  float x, y, z, w;
};
struct Transform {
  Position p;
  Quaternion r;
};
struct Transform {
  cnt::darray<Transform> transforms;
  int some_int_data;
};

...
Transform in, out;
for (uint32_t i=0; i<10; ++i)
  t.transforms.push_back({});
t.some_int_data = 42069;

ecs::DataBuffer db;
ecs::DataBuffer_SerializationWrapper s(db);
// Calculate how many bytes is it necessary to serialize "in"
s.reserve(ser::size_bytes(in));
// Save "in" to our buffer
ser::save(s, in);
// Load the contents of buffer to "out" 
s.seek(0);
ser::load(s, out);
```

Customization is possible for data types which require special attention. We can guide the serializer by either external or internal means.

External specialization comes handy in cases where we can not or do not want to modify the source type:

```cpp
struct CustomStruct {
  char* ptr;
  uint32_t size;
};

namespace gaia::ser {
  template <>
  uint32_t size_bytes(const CustomStruct& data) {
    return data.size + sizeof(data.size);
  }
  
  template <typename Serializer>
  void save(Serializer& s, const CustomStruct& data) {
    // Save the size integer
    s.save(data.size);
    // Save data.size raw bytes staring at data.ptr
    s.save(data.ptr, data.size);
  }
  
  template <typename Serializer>
  void load(Serializer& s, CustomStruct& data) {
    // Load the size integer
    s.load(data.size);
    // Load data.size raw bytes to location pointed at by data.ptr
    data.ptr = new char[data.size];
    s.load(data.ptr, data.size);
  }
}

...
CustomStruct in, out;
in.ptr = new char[5];
in.ptr[0] = 'g';
in.ptr[1] = 'a';
in.ptr[2] = 'i';
in.ptr[3] = 'a';
in.ptr[4] = '\0';
in.size = 5;

ecs::DataBuffer db;
ecs::DataBuffer_SerializationWrapper s(db);
// Reserve enough bytes in the buffer so it can fit the entire in
s.reserve(ser::size_bytes(in));
ser::save(s, in);
// Move to the start of the buffer and load its contents to out
s.seek(0);
ser::load(s, out);
// Let's release the memory we allocated
delete in.ptr;
delete out.ptr;
```

You will usually use internal specialization when you have the access to your data container and at the same time do not want to expose its internal structure. Or if you simply like intrusive coding style better. In order to use it the following 3 member functions need to be provided:

```cpp
struct CustomStruct {
  char* ptr;
  uint32_t size;
  
  constexpr uint32_t size_bytes() const noexcept {
    return size + sizeof(size);
  }
  
  template <typename Serializer>
  void save(Serializer& s) const {
    s.save(size);
    s.save(ptr, size);
  }
  
  template <typename Serializer>
  void load(Serializer& s) {
    s.load(size);
    ptr = new char[size];
    s.load(ptr, size);
  }
};
```

 It doesn't matter which kind of specialization you use. However, note that if both are used the external one has priority.

## Multithreading
To fully utilize your system's potential Gaia-ECS allows you to spread your tasks into multiple threads. This can be achieved in multiple ways.

Tasks that can not be split into multiple parts or it does not make sense for them to be split can use ***Schedule***. It registers a job in the job system and immediately submits it so worker threads can pick it up:
```cpp
mt::Job job0 {[]() {
  InitializeScriptEngine();
}};
mt::Job job1 {[]() {
  InitializeAudioEngine();
}};

// Schedule jobs for parallel execution
mt::JobHandle jobHandle0 = tp.Schedule(job0);
mt::JobHandle jobHandle1 = tp.Schedule(job1);

// Wait for jobs to complete
tp.Complete(jobHandle0);
tp.Complete(jobHandle1);
```

>**NOTE:<br/>**
It is important to call ***Complete*** for each scheduled ***JobHandle*** because it also performs cleanup.

Instead of waiting for each job separately, we can also wait for all jobs to be completed using ***CompleteAll***. This however introduces a hard sync point so it should be used with caution. Ideally, you would want to schedule many jobs and have zero sync points. In most, cases this will not ever happen and at least some sync points are going to be introduced. For instance, before any character can move in the game, all physics calculations will need to be finished.
```cpp
// Wait for all jobs to complete
tp.CompleteAll();
```

When crunching larger data sets it is often beneficial to split the load among threads automatically. This is what ***ScheduleParallel*** is for. 

```cpp
static uint32_t SumNumbers(std::span<const uint32_t> arr) {
	uint32_t sum = 0;
	for (uint32_t val: arr) sum += val;
	return sum;
}

...
constexpr uint32_t N = 1'000'000;
cnt::darray<uint32_t> arr;
__fill_arr_with_N_values__();

std::atomic_uint32_t sum = 0;
mt::JobParallel job {[&arr, &sum](const mt::JobArgs& args) {
  sum += SumNumbers({arr.data() + args.idxStart, args.idxEnd - args.idxStart});
  }};

// Schedule multiple jobs to run in paralell. Make each job process up to 1234 items.
mt::JobHandle jobHandle = tp.ScheduleParallel(job, N, 1234);
// Alternatively, we can tell the job system to figure out the group size on its own by simply omitting the group size or using 0:
// mt::JobHandle jobHandle = tp.ScheduleParallel(job, N);
// mt::JobHandle jobHandle = tp.ScheduleParallel(job, N, 0);

// Wait for jobs to complete
tp.Complete(jobHandle);

// Use the result
GAIA_LOG("Sum: %u\n", sum);
```

A similar result can be achieved via ***Schedule***. It is a bit more complicated because we need to handle workload splitting ourselves. The most compact (and least efficient) version would look something like this:

```cpp
...
constexpr uint32_t ItemsPerJob = 1234;
constexpr uint32_t Jobs = (N + ItemsPerJob - 1) / ItemsPerJob;
std::atomic_uint32_t sum = 0;
for (uint32_t i = 0; i < Jobs; i++) {
  mt::Job job {[&arr, &sum, i, ItemsPerJob, func]() {
    const auto idxStart = i * ItemsPerJob;
    const auto idxEnd = std::min((i + 1) * ItemsPerJob, N);
    sum += SumNumbers({arr.data() + idxStart, idxEnd - idxStart});
  }};
  tp.Schedule(job);
}
// Wait for all previous tasks to complete
tp.CompleteAll();
```

Sometimes we need to wait for the result of another operation before we can proceed. To achieve this we need to use low-level API and handle job registration and submitting jobs on our own.
>**NOTE:<br/>** 
This is because once submitted we can not modify the job anymore. If we could, dependencies would not necessary be adhered to. Let us say there is a job A depending on job B. If job A is submitted before creating the dependency, a worker thread could execute the job before the dependency is created. As a result, the dependency would not be respected and job A would be free to finish before job B.

```cpp
mt::Job job0;
job0.func = [&arr, i]() {
  arr[i] = i;
};
mt::Job job1;
job1.func = [&arr, i]() {
  arr[i] *= i;
};
mt::Job job2;
job2.func = [&arr, i]() {
  arr[i] /= i;
};

// Register our jobs in the job system
auto job0Handle = tp.CreateJob(job0);
auto job1Handle = tp.CreateJob(job1);
auto job2Handle = tp.CreateJob(job2);

// Create dependencies
tp.AddDependency(job1Handle, job0Handle);
tp.AddDependency(job2Handle, job1Handle);

// Submit jobs so worker threads can pick them up. The order in which jobs are submitted does not matter.
tp.Submit(job2Handle);
tp.Submit(job1Handle);
tp.Submit(job0Handle);

// Wait for the last job to complete.
// Calling Complete for dependencies is not necessary because it will be done internally.
tp.Complete(job2Handle);
```

# Requirements

## Compiler
Compiler with a good support of C++17 is required.<br/>
The project is [continuously tested](https://github.com/richardbiely/gaia-ecs/actions/workflows/build.yml) and guaranteed to build warning-free on the following compilers:<br/>
- [MSVC](https://visualstudio.microsoft.com/) 15 (Visual Studio 2017) or later<br/>
- [Clang](https://clang.llvm.org/) 7 or later<br/>
- [GCC](https://www.gnu.org/software/gcc/) 7 or later<br/>
- [ICX](https://www.intel.com/content/www/us/en/developer/articles/tool/oneapi-standalone-components.html#dpcpp-cpp) 2021.1.2 or later<br/>

## Dependencies
[CMake](https://cmake.org) 3.12 or later is required to prepare the build. Other tools are officially not supported at the moment.

Unit testing is handled via [Catch2 v3.3.2](https://github.com/catchorg/Catch2/releases/tag/v3.3.2). It can be controlled via -DGAIA_BUILD_UNITTEST=ON/OFF when configuring the project (OFF by default).

# Installation

## CMake
The following shows the steps needed to build the library:

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
cmake -G Xcode ...
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
Note, that some options don't work together or might not be supported by all compilers.
```bash
cmake -DCMAKE_BUILD_TYPE=Release -DUSE_SANITIZER=address -S . -B "build"
```

### Single-header
Gaia-ECS is shipped also as a [single header file](https://github.com/richardbiely/gaia-ecs/blob/main/single_include/gaia.h) which you can simply drop into your project and start using. To generate the header we use a wonderful Python tool [Quom](https://github.com/Viatorus/quom).

To generate the header use the following command inside your root directory.
```bash
quom ./include/gaia.h ./single_include/gaia.h -I ./include
```

You can also use the attached make_single_header.sh or create your script for your platform.

Creation of the single header can be automated via -GAIA_MAKE_SINGLE_HEADER.

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
To be able to reason about the project's performance and prevent regressions benchmarks were created.

Benchmarking relies on a modified [picobench](https://github.com/iboB/picobench). It can be controlled via -DGAIA_BUILD_BENCHMARK=ON/OFF when configuring the project (OFF by default).

Project name | Description      
-|-
[Duel](https://github.com/richardbiely/gaia-ecs/tree/main/src/perf/duel)|Compares different coding approaches such as the basic model with uncontrolled OOP with data all-over-the heap, OOP where allocators are used to controlling memory fragmentation and different ways of data-oriented design and it puts them to test against our ECS framework itself. DOD performance is the target level we want to reach or at least be as close as possible to with this project because it does not get any faster than that.
[Iteration](https://github.com/richardbiely/gaia-ecs/tree/main/src/perf/iter)|Covers iteration performance with different numbers of entities and archetypes.
[Entity](https://github.com/richardbiely/gaia-ecs/tree/main/src/perf/entity)|Focuses on performance of creating and removing entities and components of various sizes.
[Multithreading](https://github.com/richardbiely/gaia-ecs/tree/main/src/perf/mt)|Measures performance of the job system.

## Profiling
It is possible to measure the performance and memory usage of the framework via any 3rd party tool. However, support for [Tracy](https://github.com/wolfpld/tracy) is added by default.

CPU part can be controlled via -DGAIA_PROF_CPU=ON/OFF (OFF by default).

Memory part can be controlled via -DGAIA_PROF_MEM=ON/OFF (OFF by default).

Building the profiler server can be controlled via -DGAIA_PROF_CPU=ON (OFF by default).
>**NOTE:<br/>** This is a low-level feature mostly targeted for maintainers. However, if paired with your own profiler code it can become a very helpful tool.

## Unit testing
The project is thoroughly unit-tested and includes thousands of unit tests covering essentially every feature of the framework. Benchmarking relies on a modified [picobench](https://github.com/iboB/picobench).

It can be controlled via -DGAIA_BUILD_UNITTEST=ON/OFF (OFF by default).

# Future
To see what the future holds for this project navigate [here](https://github.com/users/richardbiely/projects/1/views/1)

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

