#pragma once

#include "gaia/config/config.h"

#include <cinttypes>
// TODO: Currently necessary due to std::function. Replace them!
#include <functional>

#include "gaia/ecs/id.h"
#include "gaia/ecs/query.h"

#if GAIA_OBSERVERS_ENABLED
namespace gaia {
	namespace ecs {
		class World;
		class Archetype;

	#if GAIA_PROFILER_CPU
		inline constexpr const char* sc_observer_query_func_str = "Observer_exec";
		const char* entity_name(const World& world, Entity entity);
	#endif

		//! Event types matching flecs
		enum class ObserverEvent : uint8_t {
			OnAdd, // Entity enters matching archetype
			OnDel, // Entity leaves matching archetype
			OnSet, // Component value changed
		};

		//! Observer context passed to callbacks
		struct ObserverContext {
			World* world;
			Entity entity;
			ObserverEvent event;
			Archetype* archetype; // Current archetype
			Archetype* archetype_prev; // Previous archetype (for OnAdd/OnDel)
			uint32_t index_in_chunk;
			Chunk* chunk;
			const void* old_ptr; // Previous component value (OnSet only)
		};

		//! Storage for observer data
		struct Observer_ {
			using TObserverIterFunc = std::function<void(Iter&)>;

			//! Entity identifying the observer
			Entity entity = EntityBad;
			//! Called every time the observer ticked
			TObserverIterFunc on_each_func;
			//! Event type
			ObserverEvent event;
			//! Query associated with the system
			QueryUncached query;

			void exec(Iter& iter, EntitySpan targets) {
	#if GAIA_PROFILER_CPU
				const auto& queryInfo = query.fetch();
				const char* pName = entity_name(*queryInfo.world(), entity);
				const char* pScopeName = pName != nullptr ? pName : sc_observer_query_func_str;
				GAIA_PROF_SCOPE2(pScopeName);
	#endif

				for (auto e: targets)
					on_each_func(iter);
			}

			//! Disable automatic Observer_ serialization
			template <typename Serializer>
			void save(Serializer& s) const {
				(void)s;
			}
			//! Disable automatic Observer_ serialization
			template <typename Serializer>
			void load(Serializer& s) {
				(void)s;
			}
		};

	} // namespace ecs
} // namespace gaia
#else
namespace gaia {
	namespace ecs {
		struct Observer_ {};
	} // namespace ecs
} // namespace gaia
#endif