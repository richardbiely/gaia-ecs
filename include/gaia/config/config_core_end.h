#pragma once

//------------------------------------------------------------------------------
// DO NOT MODIFY THIS FILE
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// General settings
//------------------------------------------------------------------------------

#if GAIA_USE_STL_CONTAINERS && !GAIA_USE_STL_COMPATIBLE_CONTAINERS
	#undef GAIA_USE_STL_COMPATIBLE_CONTAINERS
	#define GAIA_USE_STL_COMPATIBLE_CONTAINERS 1
#endif
#if GAIA_USE_STL_CONTAINERS || GAIA_USE_STL_COMPATIBLE_CONTAINERS
	#define GAIA_UTIL std
#else
	#define GAIA_UTIL gaia::utils
#endif

//------------------------------------------------------------------------------
// Debug features
//------------------------------------------------------------------------------

//! If enabled, additional debug and verification code is used which
//! slows things down but enables better security and diagnostics.
//! Suitable for debug builds first and foremost. Therefore, it is
//! enabled by default for debud builds.
#if !defined(GAIA_DEBUG)
	#if !defined(NDEBUG) || defined(_DEBUG) || GAIA_FORCE_DEBUG
		#define GAIA_DEBUG 1
	#else
		#define GAIA_DEBUG 0
	#endif
#endif

#if GAIA_DISABLE_ASSERTS
	#undef GAIA_ASSERT
	#define GAIA_ASSERT(condition) (void(0))
#elif !defined(GAIA_ASSERT)
	#include <cassert>
	#if GAIA_DEBUG
		#define GAIA_ASSERT(condition)                                                                                     \
			{                                                                                                                \
				bool cond_ret = (condition);                                                                                   \
				assert(cond_ret);                                                                                              \
				DoNotOptimize(cond_ret);                                                                                       \
			}
	#else
		#define GAIA_ASSERT(condition) assert(condition);
	#endif
#endif

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

#if !defined(GAIA_ECS_CHUNK_ALLOCATOR_CLEAN_MEMORY_WITH_GARBAGE)
	#define GAIA_ECS_CHUNK_ALLOCATOR_CLEAN_MEMORY_WITH_GARBAGE GAIA_DEBUG
#endif
#if !defined(GAIA_ECS_VALIDATE_CHUNKS)
	#define GAIA_ECS_VALIDATE_CHUNKS GAIA_DEBUG
#endif
#if !defined(GAIA_ECS_VALIDATE_ENTITY_LIST)
	#define GAIA_ECS_VALIDATE_ENTITY_LIST GAIA_DEBUG
#endif

//------------------------------------------------------------------------------
// Profiling features
//------------------------------------------------------------------------------

#if defined(GAIA_PROFILER)
// ...
#else
// ...
#endif
