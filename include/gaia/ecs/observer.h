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
		struct ObserverPlan {
			enum class ExecKind : uint8_t { DirectQuery, DirectFast, DiffLocal, DiffPropagated, DiffFallback };
			enum class FastPath : uint8_t { None, SinglePositiveTerm, SingleNegativeTerm, Disabled };
			using TObserverIterFunc = std::function<void(Iter&)>;

			struct DiffPlan {
				enum class DispatchKind : uint8_t { LocalTargets, PropagatedTraversal, GlobalFallback };

				
				//! Bound variable used by the supported traversal/source diff narrowing shape.
				Entity bindingVar = EntityBad;
				//! Fixed pair relation that binds the traversal source variable.
				Entity bindingRelation = EntityBad;
				//! Traversal relation used by the source term.
				Entity traversalRelation = EntityBad;

				//! Chosen diff-dispatch strategy for dynamic observers.
				DispatchKind dispatchKind = DispatchKind::LocalTargets;
				//! Traversal direction mask used by the source term.
				QueryTravKind travKind = QueryTravKind::None;
				//! Dynamic terms require full query diffing across structural changes.
				bool enabled = false;
				//! Traversal depth cap used by the source term.
				uint8_t travDepth = QueryTermOptions::TravDepthUnlimited;
				//! Traversed term ids that can trigger the bound-up-traversal narrowing path.
				QueryEntityArray traversalTriggerTerms{};
				//! Number of populated traversal trigger terms.
				uint8_t traversalTriggerTermCount = 0;
				//! Traversal relations referenced by dynamic diff terms.
				QueryEntityArray traversalRelations{};
				//! Number of populated traversal relations.
				uint8_t traversalRelationCount = 0;
			};

			//! Fast-path classification for trivial single-term observers.
			FastPath fastPath = FastPath::None;
			//! Number of terms added to the observer query.
			uint8_t termCount = 0;
			//! Chosen observer execution class.
			ExecKind execKind = ExecKind::DirectQuery;
			//! Dynamic/propagated execution metadata.
			DiffPlan diff;

			void refresh_exec_kind() {
				if (diff.enabled) {
					switch (diff.dispatchKind) {
						case DiffPlan::DispatchKind::LocalTargets:
							execKind = ExecKind::DiffLocal;
							break;
						case DiffPlan::DispatchKind::PropagatedTraversal:
							execKind = ExecKind::DiffPropagated;
							break;
						case DiffPlan::DispatchKind::GlobalFallback:
							execKind = ExecKind::DiffFallback;
							break;
					}
				} else if (fastPath == FastPath::SinglePositiveTerm || fastPath == FastPath::SingleNegativeTerm)
					execKind = ExecKind::DirectFast;
				else
					execKind = ExecKind::DirectQuery;
			}

			GAIA_NODISCARD ExecKind exec_kind() const {
				return execKind;
			}

			GAIA_NODISCARD bool uses_diff_dispatch() const {
				switch (exec_kind()) {
					case ExecKind::DiffLocal:
					case ExecKind::DiffPropagated:
					case ExecKind::DiffFallback:
						return true;
					case ExecKind::DirectQuery:
					case ExecKind::DirectFast:
						return false;
				}

				return false;
			}

			GAIA_NODISCARD bool uses_direct_dispatch() const {
				return !uses_diff_dispatch();
			}

			GAIA_NODISCARD bool uses_local_diff_targets() const {
				return exec_kind() == ExecKind::DiffLocal;
			}

			GAIA_NODISCARD bool uses_propagated_diff_targets() const {
				return exec_kind() == ExecKind::DiffPropagated;
			}

			GAIA_NODISCARD bool uses_fallback_diff_dispatch() const {
				return exec_kind() == ExecKind::DiffFallback;
			}

			GAIA_NODISCARD bool is_fast_positive() const {
				return fastPath == FastPath::SinglePositiveTerm;
			}

			GAIA_NODISCARD bool is_fast_negative() const {
				return fastPath == FastPath::SingleNegativeTerm;
			}

			void add_term_descriptor(QueryOpKind op, bool allowFastPath) {
				++termCount;

				if (!allowFastPath) {
					fastPath = FastPath::Disabled;
					return;
				}

				if (fastPath == FastPath::Disabled)
					return;

				if (termCount != 1) {
					fastPath = FastPath::Disabled;
					return;
				}

				switch (op) {
					case QueryOpKind::All:
					case QueryOpKind::Any:
					case QueryOpKind::Or:
						fastPath = FastPath::SinglePositiveTerm;
						break;
					case QueryOpKind::Not:
						fastPath = FastPath::SingleNegativeTerm;
						break;
					default:
						fastPath = FastPath::Disabled;
						break;
				}

				refresh_exec_kind();
			}
		};

		//! Runtime payload for observers kept out-of-line from ECS component storage.
		struct ObserverRuntimeData {
			using TObserverIterFunc = std::function<void(Iter&)>;

			//! Entity identifying the observer
			Entity entity = EntityBad;
			//! Called every time the observer ticked
			TObserverIterFunc on_each_func;
			//! Query associated with the system
			Query query;
			//! Precomputed observer execution plan.
			ObserverPlan plan;

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
