#pragma once

//------------------------------------------------------------------------------
// DO NOT MODIFY THIS FILE
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// External code settings
//------------------------------------------------------------------------------

#ifndef TCB_SPAN_NO_CONTRACT_CHECKING
// #define TCB_SPAN_NO_CONTRACT_CHECKING
#endif

//------------------------------------------------------------------------------
// Debug features
//------------------------------------------------------------------------------

#if !defined(NDEBUG) || defined(_DEBUG)
	#define GAIA_DEBUG_BUILD 1
#else
	#define GAIA_DEBUG_BUILD 0
#endif

//! If enabled, additional debug and verification code is used which
//! slows things down but enables better security and diagnostics.
//! Suitable for debug builds first and foremost. Therefore, it is
//! enabled by default for debug builds.
#if !defined(GAIA_DEBUG)
	#if GAIA_DEBUG_BUILD || GAIA_FORCE_DEBUG
		#define GAIA_DEBUG 1
	#else
		#define GAIA_DEBUG 0
	#endif
#endif

#if GAIA_DISABLE_ASSERTS
	#ifdef GAIA_ASSERT_ENABLED
		#undef
	#endif
	#define GAIA_ASSERT_ENABLED 0

	#ifdef GAIA_ASSERT
		#undef GAIA_ASSERT
	#endif
	#define GAIA_ASSERT(cond)

	#ifdef GAIA_ASSERT2
		#undef GAIA_ASSERT2
	#endif
	#define GAIA_ASSERT2(cond, msg)
#elif !defined(GAIA_ASSERT)
	#include <cassert>
	// For Debug builds use system's native assertion capabilities
	#if GAIA_DEBUG_BUILD
		#define GAIA_ASSERT_ENABLED 1
		#define GAIA_ASSERT(cond)                                                                                          \
			{                                                                                                                \
				GAIA_MSVC_WARNING_PUSH()                                                                                       \
				GAIA_MSVC_WARNING_DISABLE(4127)                                                                                \
				if GAIA_UNLIKELY (!(cond))                                                                                     \
					[&] {                                                                                                        \
						assert((cond));                                                                                            \
					}();                                                                                                         \
				GAIA_MSVC_WARNING_POP()                                                                                        \
			}
		#define GAIA_ASSERT2(cond, msg)                                                                                    \
			{                                                                                                                \
				GAIA_MSVC_WARNING_PUSH()                                                                                       \
				GAIA_MSVC_WARNING_DISABLE(4127)                                                                                \
				if GAIA_UNLIKELY (!(cond))                                                                                     \
					[&] {                                                                                                        \
						assert((cond) && (msg));                                                                                   \
					}();                                                                                                         \
				GAIA_MSVC_WARNING_POP()                                                                                        \
			}
	#else
		// For non-Debug builds simulate asserts
		#if GAIA_DEBUG
			#define GAIA_ASSERT_ENABLED 1
			#define GAIA_ASSERT(cond)                                                                                        \
				{                                                                                                              \
					GAIA_MSVC_WARNING_PUSH()                                                                                     \
					GAIA_MSVC_WARNING_DISABLE(4127)                                                                              \
					if GAIA_UNLIKELY (!(cond))                                                                                   \
						[] {                                                                                                       \
							GAIA_LOG_E("%s:%d: Assertion failed: '%s'.", __FILE__, __LINE__, #cond);                                 \
						}();                                                                                                       \
					GAIA_MSVC_WARNING_POP()                                                                                      \
				}
			#define GAIA_ASSERT2(cond, msg)                                                                                  \
				{                                                                                                              \
					GAIA_MSVC_WARNING_PUSH()                                                                                     \
					GAIA_MSVC_WARNING_DISABLE(4127)                                                                              \
					if GAIA_UNLIKELY (!(cond))                                                                                   \
						[] {                                                                                                       \
							GAIA_LOG_E("%s:%d: Assertion failed: '%s'.", __FILE__, __LINE__, (msg));                                 \
						}();                                                                                                       \
					GAIA_MSVC_WARNING_POP()                                                                                      \
				}
		#else
			#define GAIA_ASSERT_ENABLED 0
			#define GAIA_ASSERT(cond)
			#define GAIA_ASSERT2(cond, msg)
		#endif
	#endif
#endif

#if !defined(GAIA_ECS_VALIDATE_CHUNKS)
	#define GAIA_ECS_VALIDATE_CHUNKS (GAIA_DEBUG && GAIA_DEVMODE)
#endif
#if !defined(GAIA_ECS_VALIDATE_ENTITY_LIST)
	#define GAIA_ECS_VALIDATE_ENTITY_LIST (GAIA_DEBUG && GAIA_DEVMODE)
#endif

//------------------------------------------------------------------------------
// Prefetching
//------------------------------------------------------------------------------

namespace gaia {

	enum PrefetchHint : int {
		//! Temporal data — prefetch data into all levels of the cache hierarchy
		PREFETCH_HINT_T0 = 3,
		//! Temporal data with respect to first level cache misses — prefetch data into L2 cache and higher
		PREFETCH_HINT_T1 = 2,
		//! Temporal data with respect to second level cache misses — prefetch data into L3 cache and higher,
		//! or an implementation-specific choice
		PREFETCH_HINT_T2 = 1,
		//! Non-temporal data with respect to all cache levels — prefetch data into non-temporal cache structure
		//! and into a location close to the processor, minimizing cache pollution
		PREFETCH_HINT_NTA = 0
	};

	//! Prefetch intrinsic
	inline void prefetch([[maybe_unused]] const void* x, [[maybe_unused]] int hint) {
#if GAIA_USE_PREFETCH
	#if GAIA_COMPILER_CLANG
		// In the gcc version of prefetch(), hint is only a constant _after_ inlining
		// (assumed to have been successful).  llvm views things differently, and
		// checks constant-ness _before_ inlining.  This leads to compilation errors
		// with using the other version of this code with llvm.
		//
		// One way round this is to use a switch statement to explicitly match
		// prefetch hint enumerations, and invoke __builtin_prefetch for each valid
		// value.  llvm's optimization removes the switch and unused case statements
		// after inlining, so that this boils down in the end to the same as for gcc;
		// that is, a single inlined prefetchX instruction.
		//
		// Note that this version of prefetch() cannot verify constant-ness of hint.
		// If client code calls prefetch() with a variable value for hint, it will
		// receive the full expansion of the switch below, perhaps also not inlined.
		// This should however not be a problem in the general case of well behaved
		// caller code that uses the supplied prefetch hint enumerations.
		switch (hint) {
			case PREFETCH_HINT_T0:
				__builtin_prefetch(x, 0, PREFETCH_HINT_T0);
				break;
			case PREFETCH_HINT_T1:
				__builtin_prefetch(x, 0, PREFETCH_HINT_T1);
				break;
			case PREFETCH_HINT_T2:
				__builtin_prefetch(x, 0, PREFETCH_HINT_T2);
				break;
			case PREFETCH_HINT_NTA:
				__builtin_prefetch(x, 0, PREFETCH_HINT_NTA);
				break;
			default:
				__builtin_prefetch(x);
				break;
		}
	#elif GAIA_COMPILER_GCC
		#if !defined(__i386) || defined(__SSE__)
		if (__builtin_constant_p(hint)) {
			__builtin_prefetch(x, 0, hint);
		} else {
			// Defaults to PREFETCH_HINT_T0
			__builtin_prefetch(x);
		}
		#elif !GAIA_HAS_NO_INLINE_ASSEMBLY
		// We want a __builtin_prefetch, but we build with the default -march=i386
		// where __builtin_prefetch quietly turns into nothing.
		// Once we crank up to -march=pentium3 or higher the __SSE__
		// clause above will kick in with the builtin.
		if (hint == PREFETCH_HINT_NTA)
			asm volatile("prefetchnta (%0)" : : "r"(x));
		#endif
	#else
			// Not implemented
	#endif
#endif
	}

} // namespace gaia
