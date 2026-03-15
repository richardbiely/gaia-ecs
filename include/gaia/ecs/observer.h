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
		util::str_view entity_name(const World& world, Entity entity);
	#endif

		//! Observer event types
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

		//! Runtime payload for observers kept out-of-line from ECS component storage.
		struct ObserverRuntimeData {
			using TObserverIterFunc = std::function<void(Iter&)>;
			enum class MatchFastPath : uint8_t { None, SinglePositiveTerm, SingleNegativeTerm, Disabled };

			//! Entity identifying the observer
			Entity entity = EntityBad;
			//! Called every time the observer ticked
			TObserverIterFunc on_each_func;
			//! Query associated with the system
			QueryUncached query;
			//! Fast-path classification for trivial single-term observers.
			MatchFastPath fastPath = MatchFastPath::None;
			//! Number of terms added to the observer query.
			uint8_t termCount = 0;

			void add_term_descriptor(QueryOpKind op, bool allowFastPath) {
				++termCount;

				if (!allowFastPath) {
					fastPath = MatchFastPath::Disabled;
					return;
				}

				if (fastPath == MatchFastPath::Disabled)
					return;

				if (termCount != 1) {
					fastPath = MatchFastPath::Disabled;
					return;
				}

				switch (op) {
					case QueryOpKind::All:
					case QueryOpKind::Any:
					case QueryOpKind::Or:
						fastPath = MatchFastPath::SinglePositiveTerm;
						break;
					case QueryOpKind::Not:
						fastPath = MatchFastPath::SingleNegativeTerm;
						break;
					default:
						fastPath = MatchFastPath::Disabled;
						break;
				}
			}

			void exec(Iter& iter, EntitySpan targets);
		};

		//! Compact ECS-stored observer header.
		struct Observer_ {
			//! Entity identifying the observer
			Entity entity = EntityBad;
			//! Event type
			ObserverEvent event = ObserverEvent::OnAdd;
			//! Hot-path stamp used for O(1) deduplication during observer candidate collection.
			uint64_t lastMatchStamp = 0;

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
