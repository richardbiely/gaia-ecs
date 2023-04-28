#pragma once
#include "config.h"

#if GAIA_PROFILER_CPU || GAIA_PROFILER_MEM
// Keep it small on Windows
// TODO: What if user doesn't want this?
// #if defined(_WIN32) && !defined(WIN32_LEAN_AND_MEAN)
// 	#define WIN32_LEAN_AND_MEAN
// #endif
GAIA_MSVC_WARNING_PUSH()
GAIA_MSVC_WARNING_DISABLE(4668)
	#include <tracy/Tracy.hpp>
	#include <tracy/TracyC.h>
GAIA_MSVC_WARNING_POP()
#endif

#if GAIA_PROFILER_CPU

namespace tracy {

	//! Zone used for tracking zones with names first available in run-time
	struct ZoneRT {
		TracyCZoneCtx m_ctx;

		ZoneRT(const char* name, const char* file, uint32_t line, const char* function) {
			const auto srcloc =
					___tracy_alloc_srcloc_name(line, file, strlen(file), function, strlen(function), name, strlen(name));
			m_ctx = ___tracy_emit_zone_begin_alloc(srcloc, 1);
		}
		~ZoneRT() {
			TracyCZoneEnd(m_ctx);
		}
	};

	struct ScopeStack {
		static constexpr uint32_t StackSize = 64;

		uint32_t count;
		TracyCZoneCtx buffer[StackSize];
	};

	inline thread_local ScopeStack t_ScopeStack;

	void ZoneBegin(const ___tracy_source_location_data* srcloc) {
		auto& stack = t_ScopeStack;
		const auto pos = stack.count++;
		if (pos < ScopeStack::StackSize) {
			stack.buffer[pos] = ___tracy_emit_zone_begin(srcloc, 1);
		}
	}

	void ZoneRTBegin(uint64_t srcloc) {
		auto& stack = t_ScopeStack;
		const auto pos = stack.count++;
		if (pos < ScopeStack::StackSize)
			stack.buffer[pos] = ___tracy_emit_zone_begin_alloc(srcloc, 1);
	}

	void ZoneEnd() {
		auto& stack = t_ScopeStack;
		GAIA_ASSERT(stack.count > 0);
		const auto pos = --stack.count;
		if (pos < ScopeStack::StackSize)
			___tracy_emit_zone_end(stack.buffer[pos]);
	}
} // namespace tracy

	#define TRACY_ZoneNamedRT(name, function)                                                                            \
		tracy::ZoneRT TracyConcat(__tracy_zone_dynamic, __LINE__)(name, __FILE__, __LINE__, function);

	#define TRACY_ZoneNamedRTBegin(name, function)                                                                       \
		tracy::ZoneRTBegin(___tracy_alloc_srcloc_name(                                                                     \
				__LINE__, __FILE__, strlen(__FILE__), function, strlen(function), name, strlen(name)));

	#define TRACY_ZoneBegin(name, function)                                                                              \
		static constexpr ___tracy_source_location_data TracyConcat(__tracy_source_location, __LINE__) {                    \
			name "", function, __FILE__, uint32_t(__LINE__), 0,                                                              \
		}
	#define TRACY_ZoneEnd() tracy::ZoneEnd

	#define GAIA_PROF_START_IMPL(name, function)                                                                         \
		TRACY_ZoneBegin(name, function);                                                                                   \
		tracy::ZoneBegin(&TracyConcat(__tracy_source_location, __LINE__));

	#define GAIA_PROF_STOP_IMPL() TRACY_ZoneEnd()

	#define GAIA_PROF_SCOPE_IMPL(name) ZoneNamedN(GAIA_CONCAT(___tracy_scoped_zone_, __LINE__), name "", 1)
	#define GAIA_PROF_SCOPE_DYN_IMPL(name) TRACY_ZoneNamedRT(name, GAIA_PRETTY_FUNCTION)

//------------------------------------------------------------------------
// Tracy profiler GAIA implementation
//------------------------------------------------------------------------

	//! Marks the end of frame
	#if !defined(GAIA_PROF_FRAME)
		#define GAIA_PROF_FRAME() FrameMark
	#endif
	//! Profiling zone bounded by the scope. The zone is named after a unique compile-time string
	#if !defined(GAIA_PROF_SCOPE)
		#define GAIA_PROF_SCOPE(zoneName) GAIA_PROF_SCOPE_IMPL(#zoneName)
	#endif
	//! Profiling zone bounded by the scope. The zone is named after a run-time string
	#if !defined(GAIA_PROF_SCOPE2)
		#define GAIA_PROF_SCOPE2(zoneName) GAIA_PROF_SCOPE_DYN_IMPL(zoneName)
	#endif
	//! Profiling zone with user-defined scope - start. The zone is named after a unique compile-time string
	#if !defined(GAIA_PROF_START)
		#define GAIA_PROF_START(zoneName) GAIA_PROF_START_IMPL(#zoneName, GAIA_PRETTY_FUNCTION)
	#endif
	//! Profiling zone with user-defined scope - stop.
	#if !defined(GAIA_PROF_STOP)
		#define GAIA_PROF_STOP() GAIA_PROF_STOP_IMPL()
	#endif
#else
	//! Marks the end of frame
	#if !defined(GAIA_PROF_FRAME)
		#define GAIA_PROF_FRAME()
	#endif
	//! Profiling zone bounded by the scope. The zone is named after a unique compile-time string
	#if !defined(GAIA_PROF_SCOPE)
		#define GAIA_PROF_SCOPE(zoneName)
	#endif
	//! Profiling zone bounded by the scope. The zone is named after a run-time string
	#if !defined(GAIA_PROF_SCOPE2)
		#define GAIA_PROF_SCOPE2(zoneName)
	#endif
	//! Profiling zone with user-defined scope - start. The zone is named after a unique compile-time string
	#if !defined(GAIA_PROF_START)
		#define GAIA_PROF_START(zoneName)
	#endif
	//! Profiling zone with user-defined scope - stop.
	#if !defined(GAIA_PROF_STOP)
		#define GAIA_PROF_STOP()
	#endif
#endif

#if GAIA_PROFILER_MEM
	//! Marks a memory allocation event. The event is named after a unique compile-time string
	#if !defined(GAIA_PROF_ALLOC)
		#define GAIA_PROF_ALLOC(ptr, size) TracyAlloc(ptr, size)
	#endif
	//! Marks a memory allocation event. The event is named after a run-time string
	#if !defined(GAIA_PROF_ALLOC2)
		#define GAIA_PROF_ALLOC2(ptr, size, name) TracyAllocN(ptr, size, name)
	#endif
	//! Marks a memory release event. The event is named after a unique compile-time string
	#if !defined(GAIA_PROF_FREE)
		#define GAIA_PROF_FREE(ptr) TracyFree(ptr)
	#endif
	//! Marks a memory release event. The event is named after a run-time string
	#if !defined(GAIA_PROF_FREE2)
		#define GAIA_PROF_FREE2(ptr, name) TracyFreeN(ptr, name)
	#endif
#else
	//! Marks a memory allocation event. The event is named after a unique compile-time string
	#if !defined(GAIA_PROF_ALLOC)
		#define GAIA_PROF_ALLOC(ptr, size)
	#endif
	//! Marks a memory allocation event. The event is named after a run-time string
	#if !defined(GAIA_PROF_ALLOC2)
		#define GAIA_PROF_ALLOC2(ptr, size, name)
	#endif
	//! Marks a memory release event. The event is named after a unique compile-time string
	#if !defined(GAIA_PROF_FREE)
		#define GAIA_PROF_FREE(ptr)
	#endif
	//! Marks a memory release event. The event is named after a run-time string
	#if !defined(GAIA_PROF_FREE2)
		#define GAIA_PROF_FREE2(ptr, name)
	#endif
#endif