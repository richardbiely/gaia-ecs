#pragma once
#include "config_core.h"
#include "version.h"

//------------------------------------------------------------------------------
// Entity settings
//------------------------------------------------------------------------------

// Check entity.h IdBits and GenBits for more info
#define GAIA_ENTITY_IDBITS 20
#define GAIA_ENTITY_GENBITS 12

//------------------------------------------------------------------------------
// General settings
//------------------------------------------------------------------------------

//! If enabled, GAIA_DEBUG is defined despite using a "Release" build configuration for example
#define GAIA_FORCE_DEBUG 0
//! If enabled, no asserts are thrown even in debug builds
#define GAIA_DISABLE_ASSERTS 0

//! If enabled, diagnostics are enabled
#define GAIA_ECS_DIAGS 1
//! If enabled, custom allocator is used for allocating archetype chunks.
#define GAIA_ECS_CHUNK_ALLOCATOR 1

#define GAIA_ECS_HASH GAIA_ECS_HASH_MURMUR2A

//! If enabled, STL containers are going to be used by the framework.
#define GAIA_USE_STL_CONTAINERS 0
//! If enabled, gaia containers stay compatible with STL by sticking to STL iterators.
#define GAIA_USE_STL_COMPATIBLE_CONTAINERS 0

//------------------------------------------------------------------------------
// TODO features
//------------------------------------------------------------------------------

//! If enabled, profiler traces are inserted into generated code
#define GAIA_PROFILER 0
//! If enabled, archetype graph is used to speed up component adding and removal.
//! NOTE: Not ready
#define GAIA_ARCHETYPE_GRAPH 0

//------------------------------------------------------------------------------
// Debug features
//------------------------------------------------------------------------------

#define GAIA_ECS_CHUNK_ALLOCATOR_CLEAN_MEMORY_WITH_GARBAGE (GAIA_DEBUG || GAIA_FORCE_DEBUG)
#define GAIA_ECS_VALIDATE_CHUNKS (GAIA_DEBUG || GAIA_FORCE_DEBUG)
#define GAIA_ECS_VALIDATE_ENTITY_LIST (GAIA_DEBUG || GAIA_FORCE_DEBUG)

#include "config_core_end.h"
