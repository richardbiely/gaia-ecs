#pragma once
#include "config_core.h"
#include "version.h"

//------------------------------------------------------------------------------
// General settings
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// General settings
//------------------------------------------------------------------------------

//! If enabled, additional debug and verification code is used which
//! slows things down but enables better security and diagnostics.
//! Suitable for debug builds first and foremost. Therefore, it is
//! enabled by default for debud builds.
#if !defined(NDEBUG) || defined(_DEBUG)
	#define GAIA_DEBUG 1
#else
	#define GAIA_DEBUG 0
#endif
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
// TODO: FIXME: 0 won't work until all std utility functions are replaced with custom onces
#define GAIA_USE_STL_COMPATIBLE_CONTAINERS 0
#if GAIA_USE_STL_CONTAINERS || GAIA_USE_STL_COMPATIBLE_CONTAINERS
	#define GAIA_UTIL std
#else
	#define GAIA_UTIL gaia::utils
#endif

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

#define GAIA_ECS_CHUNK_ALLOCATOR_CLEAN_MEMORY_WITH_GARBAGE GAIA_DEBUG
#define GAIA_ECS_VALIDATE_CHUNKS GAIA_DEBUG
#define GAIA_ECS_VALIDATE_ENTITY_LIST GAIA_DEBUG

#if defined(GAIA_ECS_DIAGS)
	#undef GAIA_ECS_DIAG_ARCHETYPES
	#undef GAIA_ECS_DIAG_REGISTERED_TYPES
	#undef GAIA_ECS_DIAG_DELETED_ENTITIES

	#define GAIA_ECS_DIAG_ARCHETYPES 1
	#define GAIA_ECS_DIAG_REGISTERED_TYPES 1
	#define GAIA_ECS_DIAG_DELETED_ENTITIES 1
#else
	#if !defined(GAIA_ECS_DIAG_ARCHETYPES)
		#define GAIA_ECS_DIAG_ARCHETYPES 0
	#endif
	#if !defined(GAIA_ECS_DIAG_REGISTERED_TYPES)
		#define GAIA_ECS_DIAG_REGISTERED_TYPES 0
	#endif
	#if !defined(GAIA_ECS_DIAG_DELETED_ENTITIES)
		#define GAIA_ECS_DIAG_DELETED_ENTITIES 0
	#endif
#endif
