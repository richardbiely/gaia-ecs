<!--
@cond TURN_OFF_DOXYGEN
-->
![gaia-ecs](docs/img/logo.png)

[![Version][badge.version]][version]
[![license][badge.license]][license]
[![language][badge.language]][language]
[![Build Status][badge.actions]][actions]
[![discord][badge.discord]][discord]
[![codacy][badge.codacy]][codacy]
[![sponsors][badge.sponsors]][sponsors]

[badge.version]: https://img.shields.io/github/v/release/richardbiely/gaia-ecs?style=for-the-badge
[badge.license]: https://img.shields.io/badge/license-MIT-blue?style=for-the-badge
[badge.language]: https://img.shields.io/badge/language-C%2B%2B17-yellow?style=for-the-badge&color=blue
[badge.actions]: https://img.shields.io/github/actions/workflow/status/richardbiely/gaia-ecs/build.yml?branch=main&style=for-the-badge
[badge.discord]: https://img.shields.io/discord/1183706605108330516?style=for-the-badge
[badge.codacy]: https://img.shields.io/codacy/grade/bb28fa362fce4054bbaf7a6ba9aed140?style=for-the-badge
[badge.sponsors]: https://img.shields.io/github/sponsors/richardbiely?style=for-the-badge&color=red

[version]: https://github.com/richardbiely/gaia-ecs/releases
[license]: https://en.wikipedia.org/wiki/MIT_License
[language]: https://en.wikipedia.org/wiki/C%2B%2B17
[actions]: https://github.com/richardbiely/gaia-ecs/actions
[discord]: https://discord.gg/YdWV84GG
[codacy]: https://app.codacy.com/gh/richardbiely/gaia-ecs/dashboard?utm_source=gh&utm_medium=referral&utm_content=&utm_campaign=Badge_grade
[sponsors]: https://github.com/sponsors/richardbiely


**Gaia-ECS** is a fast and easy-to-use [ECS](#ecs) framework. Some of its current features and highlights are:
* very simple and safe API
* based on [C++17](https://en.cppreference.com/w/cpp/17) with no external dependencies (no STL strings or containers)
* compiles warning-free on [all major compilers](https://github.com/richardbiely/gaia-ecs/actions)
* archetype / chunk-based storage for maximum iteration speed and easy code parallelization
* supports [run-time defined tags](#create-or-delete-entity)
* supports [entity relationships](#relationships)
* supports applications with large number of components and archetypes
* automatic component registration
* integrated [compile-time serialization](#serialization)
* comes with [multithreading](#multithreading) support with job-dependencies 
* ability to [organize data as AoS or SoA](#data-layouts) on the component level with very few changes to your code
* compiles almost instantly
* stability and correctness secured by running thousands of [unit tests](#unit-testing) and debug-mode asserts in the code
* thoroughly documented both public and internal code
* exists also as a [single-header](#single-header) library so you can simply drop it into your project and start using it

# Table of Contents

* [Introduction](#introduction)
  * [ECS](#ecs)
  * [Implementation](#implementation)
  * [Project structure](#project-structure)
* [Usage](#usage)
  * [Minimum requirements](#minimum-requirements)
  * [Basic operations](#basic-operations)
    * [Create or delete entity](#create-or-delete-entity)
    * [Name entity](#name-entity)
    * [Add or remove component](#add-or-remove-component)
    * [Bulk editing](#bulk-editing)
    * [Set or get component value](#set-or-get-component-value)
    * [Component presence](#component-presence)
  * [Data processing](#data-processing)
    * [Query](#query)
    * [Iteration](#iteration)
    * [Constraints](#constraints)
    * [Change detection](#change-detection)
  * [Relationships](#relationships)
    * [Basics](#basics)
    * [Entity dependencies](#entity-dependencies)
    * [Entity constraints](#entity-constraints)
    * [Entity aliasing](#entity-aliasing)
    * [Targets](#targets)
    * [Relations](#relations)
    * [Cleanup rules](#cleanup-rules)
  * [Unique components](#unique-components)
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
* [Repository structure](#repository-structure)
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

On the outside ECS is not much different from database engines. The main difference is it does not need to follow the [ACID](https://en.wikipedia.org/wiki/ACID) principle which allows it to be optimized beyond what an ordinary database engine could even be both in terms of latency and absolute performance. At the cost of data safety.

## Implementation
**Gaia-ECS** is an archetype-based entity component system. This means that unique combinations of components are grouped into archetypes. Each archetype consists of chunks - blocks of memory holding your entities and components. You can think of them as [database tables](https://en.wikipedia.org/wiki/Table_(database)) where components are columns and entities are rows. Each chunk is either 8 or 16 KiB big depending on how much data can be effectively used by it. This size is chosen so that the entire chunk at its fullest can fit into the L1 cache on most CPUs.

Chunk memory is preallocated in blocks organized into pages via the internal chunk allocator. Thanks to that all data is organized in a cache-friendly way which most computer architectures like and actual heap allocations which are slow are reduced to a minimum.

The main benefits of archetype-based architecture are fast iteration and good memory layout by default. They are also easy to parallelize. On the other hand, adding and removing components can be somewhat slower because it involves moving data around. In our case, this weakness is mitigated by building an archetype graph and having the ability to add and remove components in batches.

In this project, components are entities with the ***Component*** component attached to them. Treating components as entities allows for great design simplification and big features.

## Project structure
The entire project is implemented inside gaia ***namespace***. It is further split into multiple subprojects each with a separate namespaces.
- ***core*** - core functionality, use by all other parts of the code
- ***mem*** - memory-related operations, memory allocators
- ***cnt*** - data containers
- ***meta*** - reflection framework
- ***ser*** - serialization framework
- ***mt*** - multithreading framework
- ***ecs*** - the ECS part of the project

A special part of the project is ***external***. It contains 3rd-party code such as C++17 [implementation of std::span](https://github.com/tcbrindle/span) and a modified [robin-hood](https://github.com/martinus/robin-hood-hashing) hash-map.

# Usage
## Minimum requirements

```cpp
#include <gaia.h>
```

The entire framework is placed in a namespace called **gaia**.
The ECS part of the library is found under **gaia::ecs** namespace.<br/>
In the code examples below we will assume we are inside gaia namespace.

## Basic operations
### Create or delete entity

Entity a unique "thing" in ***World***. Creating an entity at runtime is as easy as calling ***World::add***. Deleting is done via ***World::del***. Once deleted, entity is no longer valid and if used with some APIs it is going to trigger a debug-mode assert. Verifying that an entity is valid can be done by calling ***World::valid***.

```cpp
ecs::World w;
// Create a new entity
ecs::Entity e = w.add();
// Check if "e" is valid. Returns true.
bool isValid = w.valid(e); // true
// Delete the entity
w.del(e);
// Check if "e" is still valid. Return false.
isValid = w.valid(e); // false
```

It is also possible to attach entities to entities. This effectively means you are able to create your own components/tags at runtime.

```cpp
ecs::Entity player0 = w.add();
ecs::Entity player1 = w.add();
ecs::Entity player2 = w.add();
ecs::Entity teamA = w.add();
ecs::Entity teamB = w.add();
// Add player0 and player1 to teamA
w.add(player0, teamA);
w.add(player1, teamA);
// Add player2 to teamB
w.add(player2, teamB);
```

### Name entity

Each entity can be assigned a unique name. This is useful for debugging or entity lookup when entity id is not present for any reason.

```cpp
ecs::World w;
ecs::Entity e = w.add();

// Entity "e" named "my_unique_name".
// The string is copied and stored internally.
w.name(e, "my_unique_name");

// If you know the length of the string, you can provide it as well
w.name(e, "my_unique_name", 14);

// Pointer to the string used as entity name for entity "e"
const char* name = w.name(e);

// Entity identified by the string returned.
// In this case, "e_by_name" and "e" are equal.
ecs::Entity e_by_name = w.get("my_unique_name");

// The name can be unset by setting it to nullptr
w.name(e, nullptr);
```

If you already have a dedicated string storage it would be a waste to duplicate the memory. In this case you can use ***World::name_raw*** to name entities. It does NOT copy and does NOT store the string internally which means you are responsible for its lifetime. The pointer should be stable. Otherwise, any time your storage tries to move the string to a different place you have to unset the name before it happens and set it anew after the move is done.

```cpp
const char* pUserManagedString = ...;
w.name_raw(e, pUserManagedString);

// If you now the length, you can provide it
w.name_raw(e, pUserManagedString, userManagedStringLength);

// If the user-managed string pointer is not stable, you need to unset the name before the pointer changes location
w.name_raw(e, nullptr);
...
// ... the change of pointer happens
...
// After the user-managed string changed location and obtained a new pointer, you set the name again
w.name_raw(e, pUserManagedString);
```

### Add or remove component

Components can be created using ***World::add<T>***. This function returns a descriptor of the object which is created and stored in the component cache. Each component is assigned one entity to uniquely identify it. You do not have to do this yourself, the framework performs this operation automatically behind the scences any time you call some compile-time API where you interact with your structure. However, you can use this API to quickly fetch the component's entity if necessary.

```cpp
struct Position {
  float x, y, z;
};
const ecs::ComponentCacheItem& cci = w.add<Position>();
ecs::Entity position_entity = cci.entity;
```

Because components are entites as well, adding them is very similar to what we have seen previously.

```cpp
struct Position {
  float x, y, z;
};
struct Velocity {
  float x, y, z;
};

ecs::World w;

// Create an entity with Position and Velocity.
ecs::Entity e = w.add();
w.add<Position>(e, {0, 100, 0});
w.add<Velocity>(e, {0, 0, 1});

// Remove Velocity from the entity.
w.del<Velocity>(e);
```

This also means the code above could be rewritten as following:

```cpp
// Create Position and Velocity entities
ecs::Entity position = w.add<Position>().entity;
ecs::Entity velocity = w.add<Velocity>().entity;

// Create an entity with Position and Velocity.
ecs::Entity e = w.add();
w.add(e, position, Position{0, 100, 0});
w.add(e, velocity, Position{0, 0, 1});

// Remove Velocity from the entity.
w.del(e, velocity);
```

### Bulk editing

Adding an entity to entity means it becomes a part of a new archetype. Like mentioned [previously](#implementation), becoming a part of a new archetype means all data associated with the entity needs to be moved to a new place. The more ids in the archetype the slower the move (empty components/tags are an exception because they do not carry any data). For this reason it is not advised to perform large number of separate additons / removals per frame.

Instead, when adding or removing multiple entities/components at once it is more efficient doing it via bulk operations. This way only one archetype movement is performed in total rather than one per added/removed entity.

```cpp
ecs::World w;

// Create an entity with Position. This is one archetype movement.
ecs::Entity e = w.add();
w.add<Position>();

// Add and remove multiple components. 
// This does one archetype movement rather than 6 compared to doing these operations separate.
w.bulk(e)
 // add Velocity to entity e
 .add<Velocity>()
 // remove Position from entity e
 .del<Position>()
 // add Rotation to entity e
 .add<Rotation>()
 // add a bunch of other components to entity e
 .add<Something1, Something2, Something3>();
```

It is also possible to manually commit all changes by calling ***EntityBuilder::commit***. This is useful in scenarios where you have some branching and do not want to duplicate your code for both branches or simply need to add/remove components based on some complex logic.

```cpp
ecs::EntityBuilder builder = w.bulk(e);
builder
  .add<Velocity>()
  .del<Position>()
  .add<Rotation>();
if (some_conditon) {
  bulider.add<Something1, Something2, Something3>();
}
builder.commit();
```

>**NOTE:**<br/>Once ***EntityBuilder::commit*** is called (either manually or internally when the builder's destructor is invoked) the contents of builder are returned to its default state.

### Set or get component value

```cpp
// Change Velocity's value.
w.set<Velocity>(e, {0, 0, 2});
// Same as above but the world version is not updated so nobody gets notified of this change.
w.sset<Velocity>(e, {4, 2, 0});
```

When setting multiple component values at once it is more efficient doing it via chaining:

```cpp
w.set(e)
// Change Velocity's value on entity "e"
  .set<Velocity>({0, 0, 2})
// Change Position's value on entity "e"
  .set<Position>({0, 100, 0})
// Change...
  .set...;
```

Similar to ***EntityBuilder::bulk*** you can also use the setter object in scenarios with complex logic.

```cpp
ecs::ComponentSetter setter = w.set(e);
setter.set<Velocity>({0, 0, 2});
if (some_condition)
  setter.set<Position>({0, 100, 0});
setter.set...;
```

Components up to 8 bytes (including) are returned by value. Bigger components are returned by const reference.

```cpp
// Read Velocity's value. As shown above Velocity is 12 bytes in size.
// Therefore, it is returned by const reference.
const auto& velRef = w.get<Velocity>(e);
// However, it is easy to store a copy.
auto velCopy = w.get<Velocity>(e);
```

Both read and write operations are also accessible via views. Check the [iteration](#iteration) sections to see how.

### Component presence
Whether or not a certain component is associated with an entity can be checked in two different ways. Either via an instance of a World object or by the means of ***Iterator*** which can be acquired when running [queries](#query).

```cpp
// Check if entity e has Velocity (via world).
const bool hasVelocity = w.has<Velocity>(e);
// Check if entity wheel is attached to the car
const bool hasWheel = w.has(car, wheel);
...

// Check if entities hidden behind the iterator have Velocity (via iterator).
ecs::Query q = w.query().any<Position, Velocity>(); 
q.each([&](ecs::Iterator iter) {
  const bool hasPosition = iter.has<Position>();
  const bool hasVelocity = iter.has<Velocity>();
  ...
});
```

Providing entities is supported as well.

```cpp
auto p = w.add<Position>().entity;
auto v = w.add<Velocity>().entity;

// Check if entities hidden behind the iterator have Velocity (via iterator).
ecs::Query q = w.query().any(p).any(v); 
q.each([&](ecs::Iterator iter) {
  const bool hasPosition = iter.has(p);
  const bool hasVelocity = iter.has(v);
  ...
});
```

## Data processing
### Query
For querying data you can use a Query. It can help you find all entities, components or chunks matching the specified set of components and constraints and iterate or return them in the form of an array. You can also use them to quickly check if any entities satisfying your requirements exist or calculate how many of them there are.

>**NOTE:**<br/>Every Query creates a cache internally. Therefore, the first usage is a little bit slower than the subsequent usage is going to be. You likely use the same query multiple times in your program, often without noticing. Because of that, caching becomes useful as it avoids wasting memory and performance when finding matches.

```cpp
ecs::Query q = w.query();
q.all<Position>(); // Consider only entities with Position

// Fill the entities array with entities with a Position component.
cnt::darray<ecs::Entity> entities;
q.arr(entities);

// Fill the positions array with position data.
cnt::darray<Position> positions;
q.arr(positions);

// Calculate the number of entities satisfying the query
const auto numberOfMatches = q.count();

// Check if any entities satisfy the query.
// Possibly faster than count() because it stops on the first match.
const bool hasMatches = !q.empty();
```

More complex queries can be created by combining All, Any and None in any way you can imagine:

```cpp
ecs::Query q = w.query();
// Take into account everything with Position and Velocity (mutable access for both)...
q.all<Position&, Velocity&>();
// ... at least Something or SomethingElse (immutable access for both)...
q.any<Something, SomethingElse>();
// ... and no Player component... (no access done for none())
q.none<Player>();
```

All Query operations can be chained and it is also possible to invoke various filters multiple times with unique components:

```cpp
ecs::Query q = w.query();
// Take into account everything with Position (mutable access)...
q.all<Position&>()
// ... and at the same time everything with Velocity (mutable access)...
 .all<Velocity&>()
 // ... at least Something or SomethingElse (immutable access)...
 .any<Something, SomethingElse>()
 // ... and no Player component (no access)...
 .none<Player>(); 
```

All queries are cached by default. This makes sense for queries which happen very often. They are fast to process but might take more time to prepare initially. If caching is not needed you should use uncached queries and save some resources. You would normally do this for one-time initializations or rarely used operations.

```cpp
// Create an uncached query taking into account all entities with either Positon or Velocity components
ecs::QueryUncached q = w.query<false>().any<Position, Velocity>(); 
```

Queries can be defined using a low-level API (used internally).

```cpp
ecs::Entity p = w.add<Position>().entity;
ecs::Entity v = w.add<Velocity>().entity;
ecs::Entity s = w.add<Something>().entity;
ecs::Entity se = w.add<SomethingElse>().entity;
ecs::Entity pl = w.add<Player>().entity;

ecs::Query q = w.query();
// Take into account everything with Position (mutable access)...
q.add({p, QueryOp::All, QueryAccess::Write})
// ... and at the same time everything with Velocity (mutable access)...
 .add({v, QueryOp::All, QueryAccess::Write})
 // ... at least Something or SomethingElse (immutable access)..
 .add({s, QueryOp::Any, QueryAccess::Read})
 .add({se, QueryOp::Any, QueryAccess::Read})
 // ... and no Player component (no access)...
 .add({pl, QueryOp::None, QueryAccess::None}); 
```

Another way to define queries is using string notation. This allows you to define the entire query or its parts using a string composed of simple expressions. Any spaces in between modifiers and expressions are trimmed.

Supported modifiers:
* ***;*** - separates expressions
* ***?*** - query::any
* ***!*** - query::none
* ***&*** - read-write access
* ***%e*** - entity value
* ***(rel,tgt)*** - relationship pair, a wildcard character in either rel or tgt is translated into ***All***

```cpp
// Some context for the example
struct Position {...};
struct Velocity {...};
struct RigidBody {...};
struct Fuel {...};
auto player = w.add();
ecs::Entity player = w.add();

// Create the query from a string expression.
ecs::Query q = w.query()
  .add("&Position; !Velocity; ?RigidBody; (Fuel,*); %e", player.value());

// It does not matter how we split the expressions. This query is the same as the above.
ecs::Query q1 = w.query()
  .add("&Position; !Velocity;")
  .add("?RigidBody; (Fuel,*)")
  .add("%e", player.value());

// The queries above can be rewritten as following:
ecs::Query q2 = w.query()
  .all<Position&>()
  .none<Velocity>()
  .any<RigidBody>()
  .all(ecs::Pair(w.add<Fuel>().entity, All)>()
  .all(player);
```

### Iteration
To process data from queries one uses the ***Query::each*** function.
It accepts either a list of components or an iterator as its argument.

```cpp
ecs::Query q = w.query();
// Take into account all entities with Position and Velocity...
q.all<Position&, Velocity>();
// ... but no Player component.
q.none<Player>();

q.each([&](Position& p, const Velocity& v) {
  // Run the scope for each entity with Position, Velocity and no Player component
  p.x += v.x * dt;
  p.y += v.y * dt;
  p.z += v.z * dt;
});
```

>**NOTE:**<br/>Iterating over components not present in the query is not supported and results in asserts and undefined behavior. This is done to prevent various logic errors which might sneak in otherwise.

Processing via an iterator gives you even more expressive power and also opens doors for new kinds of optimizations.
Iterator is an abstraction above underlying data structures and gives you access to their public API.

There are three types of iterators:
1) ***Iterator*** - iterates over enabled entities
2) ***IteratorDisabled*** - iterates over distabled entities
3) ***IteratorAll*** - iterate over all entities

```cpp
ecs::Query q = w.query();
q.all<Position&, Velocity>();

q.each([](ecs::IteratorAll iter) {
  auto p = iter.view_mut<Position>(); // Read-write access to Position
  auto v = iter.view<Velocity>(); // Read-only access to Velocity

  // Iterate over all enabled entities and update their x-axis position
  GAIA_EACH(iter) {
    if (!iter.enabled(i))
      return;
    p[i].x += 1.f;
  }

  // Iterate over all entities and update their position based on their velocity
  GAIA_EACH(iter) {
    p[i].x += v[i].x * dt;
    p[i].y += v[i].y * dt;
    p[i].z += v[i].z * dt;
  }
});
```

***GAIA_EACH(iter)*** is simply a shortcut for
```
for (uint32_t i=0; i<iter.size(); ++i)
```
A similar macro exists where you can specify the variable name. It is called ***GAIA_EACH_(iter,k)***
```
for (uint32_t k=0; k<iter.size(); ++k)
```

### Constraints

Query behavior can also be modified by setting constraints. By default, only enabled entities are taken into account. However, by changing constraints, we can filter disabled entities exclusively or make the query consider both enabled and disabled entities at the same time.

Disabling/enabling an entity is a special operation that marks it invisible to queries by default. Archetype of the entity is not changed afterwards so it can be considered fast.

```cpp
ecs::Entity e1, e2;

// Create 2 entities with Position component
w.add(e1);
w.add(e2);
w.add<Position>(e1);
w.add<Position>(e2);

// Disable the first entity
w.enable(e1, false);

// Check if e1 is enabled
const bool is_e1_enabled = w.enabled(e1);
if (is_e1_enabled) { ... }

// Prepare out query
ecs::Query q = w.query().all<Position&>();

// Fills the array with only e2 because e1 is disabled.
cnt::darray<ecs::Entity> entities;
q.arr(entities);

// Fills the array with both e1 and e2.
q.arr(entities, ecs::Query::Constraint::AcceptAll);

// Fills the array with only e1 because e1 is disabled.
q.arr(entities, ecs::Query::Constraint::DisabledOnly);

q.each([](ecs::Iterator iter) {
  auto p = iter.view_mut<Position>(); // Read-Write access to Position
  // Iterates over enabled entities
  GAIA_EACH(iter) p[i] = {}; // reset the position of each enabled entity
});
q.each([](ecs::IteratorDisabled iter) {
  auto p = iter.view_mut<Position>(); // Read-Write access to Position
  // Iterates over disabled entities
  GAIA_EACH(iter) p[i] = {}; // reset the position of each disabled entity
});
q.each([](ecs::IteratorAll iter) {
  auto p = iter.view_mut<Position>(); // Read-Write access to Position
  // Iterates over all entities
  GAIA_EACH(iter) {
    if (iter.enabled(i)) {
      p[i] = {}; // reset the position of each enabled entity
    }
  }
});
```

If you do not wish to fragment entities inside the chunk you can simply create a tag component and assign it to your entity. This will move the entity to a new archetype so it is a lot slower. However, because disabled entities are now clearly separated calling some query operations might be slightly faster (no need to check if the entity is disabled or not internally).

```cpp
struct Disabled {};

...

e.add<Disabled>(); // disable entity

ecs::Query q = w.query().all<Position, Disabled>; 
q.each([&](ecs::Iterator iter){
  // Processes all disabled entities
});

e.del<Disabled>(); // enable entity
```

### Change detection
Using changed we can make the iteration run only if particular components change. You can save quite a bit of performance using this technique.

```cpp
ecs::Query q = w.query();
// Take into account all entities with Position and Velocity...
q.all<Position&, Velocity>();
// ... no Player component...
q.none<Player>(); 
// ... but only iterate when Velocity changes
q.changed<Velocity>();

q.each([&](Position& p, const Velocity& v) {
  // This scope runs for each entity with Position, Velocity and no Player component
  // but only when Velocity has changed.
  p.x += v.x * dt;
  p.y += v.y * dt;
  p.z += v.z * dt;
});
```

>**NOTE:**<br/>If there are 100 Position components in the chunk and only one of them changes, the other 99 are considered changed as well. This chunk-wide behavior might seem counter-intuitive but it is in fact a performance optimization. The reason why this works is because it is easier to reason about a group of entities than checking each of them separately.

## Relationships
### Basics
Entity relationship is a feature that allows users to model simple relations, hierarchies or graphs in an ergonomic, easy and safe way.
Each relationship is expressed as following: "source, (relation, target)". All three elements of a relationship are entities. We call the "(relation, target)" part a relationship pair.

Relationship pair is a special kind of entity where the id of the "relation" entity becomes the pair's id and the "target" entity's id becomes the pairs generation. The pair is created by calling ***ecs::Pair(relation, target***) with two valid entities as its arguments.

Adding a relationship to any entity is as simple as adding any other entity.

```cpp
ecs::World w;
ecs::Entity rabbit = w.add();
ecs::Entity hare = w.add();
ecs::Entity carrot = w.add();
ecs::Entity eats = w.add();

w.add(rabbit, ecs::Pair(eats, carrot));
w.add(hare, ecs::Pair(eats, carrot));

ecs::Query q = w.query().all(ecs::Pair(eats, carrot));
q.each([](ecs::Entity entity)) {
  // Called for each entity implementing (eats, carrot) relationship.
  // Triggers for rabbit and hare.
}
```

This by itself would not be much different from adding entities/component to entities. A similar result can be achieved by creating a "eats_carrot" tag and assigning it to "hare" and "rabbit". What sets relationships apart is the ability to use wildcards in queries.

There are three kinds of wildcard queries possible:
- ***( X, * )*** - X that does anything
- ***( * , X )*** - anything that does X
- ***( * , * )*** - anything that does anything (aka any relationship)

The "*" wildcard is expressed via ***All*** entity.

```cpp
w.add(rabbit, ecs::Pair(eats, carrot));
w.add(hare, ecs::Pair(eats, carrot));
w.add(wolf, ecs::Pair(eats, rabbit));

ecs::Query q1 = w.query().all(ecs::Pair(eats, All));
q1.each([]()) {
  // Called for each entity implementing (eats, *) relationship.
  // This can be read as "entity that eats anything".
  // Triggers for rabbit, hare and wolf.
}

ecs::Query q2 = w.query().all(ecs::Pair(All, eats));
q2.each([]()) {
  // Called for each entity implementing (*, carrot) relationship.
  // This can be read as "anything that has something with carrot".
  // Triggers for rabbit and hare.
}

ecs::Query q3 = w.query().all(ecs::Pair(All, All));
q3.each([]()) {
  // Called for each entity implementing (*, *) relationship.
  // This can be read as "anything that does/has anything".
  // Triggers for rabbit, hare and wolf.
}
```

Relationships can be ended by calling ***World::del*** (just like it is done for regular entities/components)..

```cpp
// Rabbit no longer eats carrot
w.del(rabbit, ecs::Pair(eats, carrot));
```

Whether a relationship exists can be check via ***World::has*** (just like it is done for regular entities/components).

```cpp
// Checks if rabbit eats carrot
w.has(rabbit, ecs::Pair(eats, carrot));
// Checks if rabbit eats anything
w.has(rabbit, ecs::Pair(eats, All));
```
A nice side-effect of relationships is they allow for multiple components/entities of the same kind be added to one entity.

```cpp
// "eats" is added twice to the entity "rabbit"
w.add(rabbit, ecs::Pair(eats, carrot));
w.add(rabbit, ecs::Pair(eats, salad));
```

### Targets

Targets of a relationship can be retrieved via ***World::target*** and ***World::targets***.

```cpp
w.add(rabbit, ecs::Pair(eats, carrot));
w.add(rabbit, ecs::Pair(eats, salad));

// Returns whatever the first found target of the rabbit(eats, *) relationship is.
// In our case it is the carrot entity because it was created before salad.
ecs::Entity first_target = w.target(rabbit, eats);

// Appends carrot and salad entities to the array
cnt::sarr_ext<ecs::Entity, 32> what_rabbit_eats;
w.target(rabbit, eats, what_rabbit_eats);
```

### Relations

Relations of a relationhip can be retrived via ***World::relation*** and ***World::relations***.

```cpp
w.add(rabbit, ecs::Pair(eats, carrot));
w.add(rabbit, ecs::Pair(eats, salad));

// Returns whatever the first found relation of the rabbit(*, salad) relationship is.
// In our case it is eats.
ecs::Entity first_relation = w.relation(rabbit, salad);

// Appends eats to the array
cnt::sarr_ext<ecs::Entity, 32> related_to_salad;
w.relations(rabbit, salad, related_to_salad);
```

### Entity dependencies
Defining dependencies among entities is made possible via the (DependsOn, target) relationship.

When adding an entity with a dependency to some source it is guaranteed the dependency will always be present on the source as well. It will also be impossible to delete it.

```cpp
ecs::World w;
ecs::Entity rabbit = w.add();
ecs::Entity animal = w.add();
ecs::Entity herbivore = w.add();
ecs::Entity carrot = w.add();
w.add(carrot, ecs::Pair(ecs::DependsOn, herbivore));
w.add(herbivore, ecs::Pair(ecs::DependsOn, animal));

// carrot depends on herbivore so the later is added as well.
// At the same time, herbivore depends on animal so animal is added, too.
w.add(rabbit, carrot);
const bool isHerbivore = w.has(rabbit, herbivore)); // true
const bool isAnimal = w.has(rabbit, animal); // true

// Animal will not be removed from rabbit because of the dependency chain.
// Carrot depends on herbivore which depends on animal.
w.del(rabbit, animal); // does nothing
// Herbivore will not be removed from rabbit because of the dependency chain.
// Carrot depends on herbivore.
w.del(rabbit, herbivore); // does nothing

// Carrot can be deleted. It requires herbivore is present which is true.
w.del(rabbit, carrot); // removes carrot from rabbit
```

### Entity constraints
Entity constrains are used to define what entities can not be combined with others.

```cpp
ecs::World w;
ecs::Entity weak = w.add();
ecs::Entity strong = w.add();
w.add(weak, ecs::Pair(ecs::CantCombine, strong));

ecs::Entity e = w.add();
w.add(e, strong);
// Following line is an invalid operation.
w.add(e, weak);
```

### Entity aliasing
Entities can alias another entities by using the (AliasOf, target) relationship. This is a powerful feature that helps you identify an entire group of entities using a single entity.

```cpp
ecs::World w;
ecs::Entity animal = w.add();
ecs::Entity herbivore = w.add();
ecs::Entity rabbit = w.add();
ecs::Entity hare = w.add();

w.add(herbivore, ecs::Pair(ecs::AliasOf, animal)); // w.alias(herbivore, animal)
w.add(rabbit, ecs::Pair(ecs::AliasOf, herbivore)); // w.alias(rabbit, herbivore)
w.add(hare, ecs::Pair(ecs::AliasOf, herbivore)); // w.alias(hare, herbivore)

ecs::Query q = w.query().all(animal);
q.each([](ecs::Entity entity) {
  // runs for herbivore, rabbit and hare
});
```

### Cleanup rules
When deleting an entity we might want to define how the deletion is going to happen. Do we simply want to remove the entity or does everything connected to it need to get deleted as well? This behavior can be customized via relationships called cleanup rules.

Cleanup rules are defined as ecs::Pair(Condition, Reaction).

Condition is one of the following:
* ***OnDelete*** - deleting an entity/pair
* ***OnDeleteTarget*** - deleting a pair's target

Reaction is one of the following:
* ***Remove*** - removes the entity/pair from anything referencing it
* ***Delete*** - delete everything referencing the entity
* ***Error*** - error out when deleted

The default behavior of deleting an entity is to simply remove it from the parent entity. This is an equivalent of Pair(OnDelete, Remove) relationship pair attached to the entity getting deleted.

Additionally, a behavior which can not be changed, all relationship pairs formed by this entity need to be deleted as well. This is needed because entity ids are recycled internally and we could not guarantee that the relationship entity would be be used for something unrelated later.

All core entities are defined with (OnDelete,Error). This means that instead of deleting the entity an error is thrown when an attempt to delete the entity is made. 

```cpp
ecs::Entity rabbit = w.add();
ecs::Entity hare = w.add();
ecs::Entity eats = w.add();
ecs::Entity carrot = w.add();
w.add(rabbit, ecs::Pair(eats, carrot));
w.add(hare, ecs::Pair(eats, carrot));

// Delete the rabbit. Everything else is unaffected.
w.del(rabbit);
// Delete eats. Deletes eats and all associated relationships.
w.del(eats); 
```

Creating custom rules is just a matter of adding the relationship to an entity.

```cpp
ecs::Entity bomb_exploding_on_del = w.add();
w.add(bomb_exploding_on_del, ecs::Pair(OnDelete, Delete));

// Attach a bomb to our rabbit
w.add(rabbit, bomb_exploding_on_del);

// Deleting the bomb will take out all entities associated with it along. Rabbit included.
w.del(bomb_exploding_on_del); 
```

A core ***ChildOf*** entity is defined that can be used to express a physical hierarchy. It defines as (OnDeleteTarget, Delete) relationship so if the parent is deleted, all the children are deleted as well.

```cpp
ecs::Entity parent = w.add();
ecs::Entity child1 = w.add();
ecs::Entity child2 = w.add();
w.add(child1, ecs::Pair(ecs::ChildOf, parent));
w.add(child2, ecs::Pair(ecs::ChildOf, parent));

// Deleting parent deletes child1 and child2 as well.
w.del(parent); 
```


## Unique components
Unique component is a special kind of data that exists at most once per chunk. In other words, you attach data to one chunk specifically. It survives entity removals and unlike generic components, they do not transfer to a new chunk along with their entity.

If you organize your data with care (which you should) this can save you some very precious memory or performance depending on your use case.

For instance, imagine you have a grid with fields of 100 meters squared.
Now if you create your entities carefully they get organized in grid fields implicitly on the data level already without you having to use any sort of spatial map container.

```cpp
w.add<Position>(e1, {10,1});
w.add<Position>(e2, {19,1});
// Make both e1 and e2 share a common grid position of {1,0}
w.add<ecs::uni<GridPosition>>(e1, {1, 0});
```

## Delayed execution
Sometimes you need to delay executing a part of the code for later. This can be achieved via CommandBuffers.

CommandBuffer is a container used to record commands in the order in which they were requested at a later point in time.

Typically you use them when there is a need to perform structural changes (adding or removing an entity or component) while iterating chunks.

Performing an unprotected structural change is undefined behavior and most likely crashes the program. However, using a CommandBuffer you can collect all requests first and commit them when it is safe.

```cpp
ecs::CommandBuffer cb;
q.each([&](Entity e, const Position& p) {
  if (p.y < 0.0f) {
    // Queue entity e for deletion if its position falls below zero
    cb.del(e);
  }
});
// Make the queued command happen
cb.commit(&w);
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

Using the example above will make **Gaia-ECS** treat Position components like this in memory: xxxx yyyy zzzz.

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

ecs::Query q = w.query().all<PositionSoA&, VelocitySoA>;
q.each([](ecs::Iterator iter) {
  // Position
  auto vp = iter.view_mut<PositionSoA>(); // read-write access to PositionSoA
  auto px = vp.set<0>(); // continuous block of "x" from PositionSoA
  auto py = vp.set<1>(); // continuous block of "y" from PositionSoA
  auto pz = vp.set<2>(); // continuous block of "z" from PositionSoA

  // Velocity
  auto vv = iter.view<VelocitySoA>(); // read-only access to VelocitySoA
  auto vx = vv.get<0>(); // continuous block of "x" from VelocitySoA
  auto vy = vv.get<1>(); // continuous block of "y" from VelocitySoA
  auto vz = vv.get<2>(); // continuous block of "z" from VelocitySoA

  // Handle x coordinates
  GAIA_EACH(iter) px[i] += vx[i] * dt;
  // Handle y coordinates
  GAIA_EACH(iter) py[i] += vy[i] * dt;
  // Handle z coordinates
  GAIA_EACH(iter) pz[i] += vz[i] * dt;

  /*
  You can even use SIMD intrinsics now without a worry.
  Note, this is just an example not an optimal way to rewrite the loop.
  Also, most compilers will auto-vectorize this code in release builds anyway.

  The code bellow uses x86 SIMD intrinsics.
  uint32_t i = 0;
  // Process SSE-sized blocks first
  for (; i < iter.size(); i+=4) {
    const auto pVec = _mm_load_ps(px.data() + i);
    const auto vVec = _mm_load_ps(vx.data() + i);
    const auto respVec = _mm_fmadd_ps(vVec, dtVec, pVec);
    _mm_store_ps(px.data() + i, respVec);
  }
  // Process the rest of the elements
  for (; i < iter.size(); ++i) {
    ...
  }
  */
});
```

## Serialization
Serialization of arbitrary data is available via following functions:
- ***ser::bytes*** - calculates how many bytes the data needs to serialize
- ***ser::save*** - writes data to serialization buffer
- ***ser::load*** - loads data from serialization buffer

Any data structure can be serialized at compile-time into the provided serialization buffer. Native types, compound types, arrays and anything with data() + size() functions are supported out-of-the-box. If resize() is available it will be utilized.

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
GAIA_FOR(10) t.transforms.push_back({});
t.some_int_data = 42069;

ecs::SerializationBuffer s;
// Calculate how many bytes is it necessary to serialize "in"
s.reserve(ser::bytes(in));
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
  uint32_t bytes(const CustomStruct& data) {
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

ecs::SerializationBuffer s;
// Reserve enough bytes in the buffer so it can fit the entire in
s.reserve(ser::bytes(in));
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
  
  constexpr uint32_t bytes() const noexcept {
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
To fully utilize your system's potential **Gaia-ECS** allows you to spread your tasks into multiple threads. This can be achieved in multiple ways.

Tasks that can not be split into multiple parts or it does not make sense for them to be split can use ***ThreadPool::sched***. It registers a job in the job system and immediately submits it so worker threads can pick it up:
```cpp
mt::Job job0 {[]() {
  InitializeScriptEngine();
}};
mt::Job job1 {[]() {
  InitializeAudioEngine();
}};

ThreadPool &tp = ThreadPool::get();

// Schedule jobs for parallel execution
mt::JobHandle jobHandle0 = tp.sched(job0);
mt::JobHandle jobHandle1 = tp.sched(job1);

// Wait for jobs to complete
tp.wait(jobHandle0);
tp.wait(jobHandle1);
```

>**NOTE:<br/>**
It is important to call ***ThreadPool::wait*** for each scheduled ***JobHandle*** because it also performs cleanup.

Instead of waiting for each job separately, we can also wait for all jobs to be completed using ***ThreadPool::wait_all***. This however introduces a hard sync point so it should be used with caution. Ideally, you would want to schedule many jobs and have zero sync points. In most, cases this will not ever happen and at least some sync points are going to be introduced. For instance, before any character can move in the game, all physics calculations will need to be finished.
```cpp
// Wait for all jobs to complete
tp.wait_all();
```

When crunching larger data sets it is often beneficial to split the load among threads automatically. This is what ***ThreadPool::sched_par*** is for. 

```cpp
static uint32_t SumNumbers(std::span<const uint32_t> arr) {
  uint32_t sum = 0;
  for (uint32_t val: arr)
    sum += val;
  return sum;
}
...

constexpr uint32_t N = 1'000'000;
cnt::darray<uint32_t> arr;
...

std::atomic_uint32_t sum = 0;
mt::JobParallel job {[&arr, &sum](const mt::JobArgs& args) {
  sum += SumNumbers({arr.data() + args.idxStart, args.idxEnd - args.idxStart});
  }};

// Schedule multiple jobs to run in paralell. Make each job process up to 1234 items.
mt::JobHandle jobHandle = tp.sched_par(job, N, 1234);
// Alternatively, we can tell the job system to figure out the group size on its own
// by simply omitting the group size or using 0:
// mt::JobHandle jobHandle = tp.sched_par(job, N);
// mt::JobHandle jobHandle = tp.sched_par(job, N, 0);

// Wait for jobs to complete
tp.wait(jobHandle);

// Use the result
GAIA_LOG("Sum: %u\n", sum);
```

A similar result can be achieved via ***ThreadPool::sched***. It is a bit more complicated because we need to handle workload splitting ourselves. The most compact (and least efficient) version would look something like this:

```cpp
...
constexpr uint32_t ItemsPerJob = 1234;
constexpr uint32_t Jobs = (N + ItemsPerJob - 1) / ItemsPerJob;

std::atomic_uint32_t sum = 0;
GAIA_FOR(Jobs) { // for (uint32_t i=0; i<Jobs; ++i)
  mt::Job job {[&arr, &sum, i, ItemsPerJob, func]() {
    const auto idxStart = i * ItemsPerJob;
    const auto idxEnd = std::min((i + 1) * ItemsPerJob, N);
    sum += SumNumbers({arr.data() + idxStart, idxEnd - idxStart});
  }};
  tp.sched(job);
}

// Wait for all previous tasks to complete
tp.wait_all();
```

Sometimes we need to wait for the result of another operation before we can proceed. To achieve this we need to use low-level API and handle job registration and submitting jobs on our own.
>**NOTE:<br/>** 
This is because once submitted we can not modify the job anymore. If we could, dependencies would not necessary be adhered to.<br/>
Let us say there is a job A depending on job B. If job A is submitted before creating the dependency, a worker thread could execute the job before the dependency is created. As a result, the dependency would not be respected and job A would be free to finish before job B.

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
  arr[i] += i;
};

// Register our jobs in the job system
auto job0Handle = tp.add(job0);
auto job1Handle = tp.add(job1);
auto job2Handle = tp.add(job2);

// Create dependencies
tp.dep(job1Handle, job0Handle);
tp.dep(job2Handle, job1Handle);

// Submit jobs so worker threads can pick them up.
// The order in which jobs are submitted does not matter.
tp.submit(job2Handle);
tp.submit(job1Handle);
tp.submit(job0Handle);

// Wait for the last job to complete.
// Calling wait() for dependencies is not necessary. It is be done internally.
tp.wait(job2Handle);
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

Unit testing is handled via [Catch2 v3.5.0](https://github.com/catchorg/Catch2/releases/tag/v3.5.0). It can be controlled via -DGAIA_BUILD_UNITTEST=ON/OFF when configuring the project (OFF by default).

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
**Gaia-ECS** is shipped also as a [single header file](https://github.com/richardbiely/gaia-ecs/blob/main/single_include/gaia.h) which you can simply drop into your project and start using. To generate the header we use a wonderful Python tool [Quom](https://github.com/Viatorus/quom).

To generate the header use the following command inside your root directory.
```bash
quom ./include/gaia.h ./single_include/gaia.h -I ./include
```

You can also use the attached make_single_header.sh or create your script for your platform.

Creation of the single header can be automated via -GAIA_MAKE_SINGLE_HEADER.

# Repository structure

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

Benchmarking relies on [picobench](https://github.com/iboB/picobench). It can be controlled via -DGAIA_BUILD_BENCHMARK=ON/OFF when configuring the project (OFF by default).

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

Thank you for using the project! :)

# License

Code and documentation Copyright (c) 2021-2023 Richard Biely.

Code released under
[the MIT license](https://github.com/richardbiely/gaia-ecs/blob/master/LICENSE).

![gaia-ecs-small](docs/img/logo_small.png)
<!--
@endcond TURN_OFF_DOXYGEN
-->

