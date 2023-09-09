#pragma once
#include "config_core.h"
#include "version.h"

//------------------------------------------------------------------------------
// General settings.
// You are free to modify these.
//------------------------------------------------------------------------------

//! If enabled, GAIA_DEBUG is defined despite using a "Release" build configuration for example
#if !defined(GAIA_FORCE_DEBUG)
	#define GAIA_FORCE_DEBUG 0
#endif
//! If enabled, no asserts are thrown even in debug builds
#if !defined(GAIA_DISABLE_ASSERTS)
	#define GAIA_DISABLE_ASSERTS 0
#endif

//! If enabled, STL containers are going to be used by the framework.
#if !defined(GAIA_USE_STL_CONTAINERS)
	#define GAIA_USE_STL_CONTAINERS 0
#endif
//! If enabled, internal containers stay compatible with STL by sticking to STL iterators.
#if !defined(GAIA_USE_STL_COMPATIBLE_CONTAINERS)
	#define GAIA_USE_STL_COMPATIBLE_CONTAINERS (GAIA_USE_STL_CONTAINERS || 0)
#endif

//! Number of bits used for the entity identifier.
//! You should only touch this if you need more than 2^20 entities which should be fairly
//! difficult to achieve.
//! Check entity.h IdBits and GenBits for more info.
#if !defined(GAIA_ENTITY_IDBITS)
	#define GAIA_ENTITY_IDBITS 20
#endif
//! Number of bits used for the entity generation.
//! You should only touch this if you need more than 2^12 generations for your entities
//! which should be fairly difficult to achive.
//! Check entity.h IdBits and GenBits for more info.
#if !defined(GAIA_ENTITY_GENBITS)
	#define GAIA_ENTITY_GENBITS 12
#endif

//------------------------------------------------------------------------------
// Internal features.
// You are free to modify these but you probably should not.
// It is expected to only use them when something doesn't work as expected
// or as some sort of workaround.
//------------------------------------------------------------------------------

//! If enabled, custom allocator is used for allocating archetype chunks.
#ifndef GAIA_ECS_CHUNK_ALLOCATOR
	#define GAIA_ECS_CHUNK_ALLOCATOR 1
#endif

//! Hashing algorithm. GAIA_ECS_HASH_FNV1A or GAIA_ECS_HASH_MURMUR2A
#ifndef GAIA_ECS_HASH
	#define GAIA_ECS_HASH GAIA_ECS_HASH_MURMUR2A
#endif

//! If enabled, explicit memory prefetching is used when querying chunks, possibly improving performance in
//! edge-cases
#ifndef GAIA_USE_PREFETCH
	#define GAIA_USE_PREFETCH 1
#endif

//! When searching for empty chunk we have two options:
//! 1) pick any non-empty one first
//!    - fragments memory, progresivelly slower peformance over time possible
//!    - fast removals because entity swaps happen within the same chunk
//! 2) always take from the last chunk
//!    - avoids memory fragmentation altogether
//!    - makes entity removal and component movement slower because we always swap with the last chunk
//!
//! We stick with (1) as a default because in general it is better to take a small performance hit
//! for defragmeneting every once in a while than having consistently slower performance all the time.
#ifndef GAIA_AVOID_CHUNK_FRAGMENTATION
	#define GAIA_AVOID_CHUNK_FRAGMENTATION 0
#endif

//------------------------------------------------------------------------------

#include "config_core_end.h"
