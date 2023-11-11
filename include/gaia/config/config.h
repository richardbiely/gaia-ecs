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

//! If enabled, memory prefetching is used when querying chunks, possibly improving performance in edge-cases
#ifndef GAIA_USE_PREFETCH
	#define GAIA_USE_PREFETCH 1
#endif

//! If enabled, a very small hash table is stored in chunks internally which
//! is used to do ComponentId -> component index lookups.
//! \warning Experimental, don't use.
#ifndef GAIA_COMP_ID_PROBING
	#define GAIA_COMP_ID_PROBING 0
#endif

//------------------------------------------------------------------------------

#include "config_core_end.h"
