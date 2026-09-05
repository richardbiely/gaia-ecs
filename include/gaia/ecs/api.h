#pragma once
#include "gaia/config/config.h"

#include <cstdarg>

#include "gaia/core/span.h"
#include "gaia/ecs/command_buffer_fwd.h"
#include "gaia/ecs/id_fwd.h"
#include "gaia/ecs/query_fwd.h"

namespace gaia {
	namespace ecs {
		class World;
		class ComponentCache;
		class Archetype;
		struct ComponentCacheItem;
		struct EntityContainer;
		struct Entity;

		// Component API

		const ComponentCache& comp_cache(const World& world);
		ComponentCache& comp_cache_mut(World& world);
		//! Registers or retrieves metadata for a typed component.
		//! \tparam T Component type to register.
		//! \param world World whose component cache owns the registration.
		//! \return Metadata for the registered component.
		template <typename T>
		const ComponentCacheItem& comp_cache_add(World& world);

		// Entity API

		const EntityContainer& fetch(const World& world, Entity entity);
		EntityContainer& fetch_mut(World& world, Entity entity);

		void del(World& world, Entity entity);

		Entity entity_from_id(const World& world, EntityId id);
		Entity id_entity(const World& world, Entity id);
		Entity pair_rel(const World& world, Entity pair);
		Entity pair_tgt(const World& world, Entity pair);

		//! Returns whether \p entity currently exists and its generation matches the world's record.
		//! \param world World that owns the entity records.
		//! \param entity Entity to validate.
		//! \return True when the entity is alive in the world.
		bool valid(const World& world, Entity entity);

		bool is(const World& world, Entity entity, Entity baseEntity);
		bool is_base(const World& world, Entity entity);

		Archetype* archetype_from_entity(const World& world, Entity entity);

		util::str_view entity_name(const World& world, Entity entity);
		util::str_view entity_name(const World& world, EntityId entityId);
		Entity target(const World& world, Entity entity, Entity relation);
		//! Invokes \a func for each live target of \a entity through \a relation.
		//! This is a small C-style adapter used by header-only query/observer internals.
		void world_each_target(const World& world, Entity entity, Entity relation, void* ctx, void (*func)(void*, Entity));
		Entity world_pair_target_if_alive(const World& world, Entity pair);
		bool world_entity_enabled(const World& world, Entity entity);
		bool world_entity_enabled_hierarchy(const World& world, Entity entity, Entity relation);
		uint32_t world_enabled_hierarchy_version(const World& world);
		//! Returns the version that changes when an archetype enters or leaves the deletion-request set.
		//! \param world World whose archetype deletion state is inspected.
		//! \return Current archetype deletion-set version.
		uint32_t world_archetype_delete_version(const World& world);
		//! Returns the current world structural version.
		//! \param world World whose version is inspected.
		//! \return Current structural version.
		uint32_t world_version(const World& world);
		//! Returns the current version of \p relation-specific traversal metadata.
		//! \param world World that owns the traversal metadata.
		//! \param relation Relation whose metadata version is inspected.
		//! \return Current relation traversal version.
		uint32_t world_rel_version(const World& world, Entity relation);
		//! Returns whether \a relation is non-fragmenting.
		bool world_relation_is_non_fragmenting(const World& world, Entity relation);
		//! Returns whether \a relation is treated as a hierarchy relation by traversal helpers.
		bool world_relation_is_hierarchy(const World& world, Entity relation);
		//! Returns whether \a relation fragments archetypes.
		bool world_relation_is_fragmenting(const World& world, Entity relation);
		//! Returns whether \a relation is both hierarchy-like and fragmenting.
		bool world_relation_is_fragmenting_hierarchy(const World& world, Entity relation);
		//! Returns whether \a relation supports cached depth-order traversal.
		bool world_relation_supports_depth_order(const World& world, Entity relation);
		//! Returns whether depth-order traversal for \a relation skips disabled subtrees.
		bool world_relation_depth_order_prunes_disabled_subtrees(const World& world, Entity relation);
		template <typename T>
		decltype(auto) world_query_entity_arg_by_id(World& world, Entity entity, Entity id);

		// Traversal API

		template <typename Func>
		void as_relations_trav(const World& world, Entity target, Func func);
		template <typename Func>
		bool as_relations_trav_if(const World& world, Entity target, Func func);
		template <typename Func>
		void as_targets_trav(const World& world, Entity relation, Func func);
		template <typename Func>
		bool as_targets_trav_if(const World& world, Entity relation, Func func);
		template <typename Func>
		void targets_trav(const World& world, Entity relation, Entity source, Func func);
		template <typename Func>
		bool targets_trav_if(const World& world, Entity relation, Entity source, Func func);
		const cnt::darray<Entity>& targets_trav_cache(const World& world, Entity relation, Entity source);
		template <typename Func>
		void sources(const World& world, Entity relation, Entity target, Func func);
		template <typename Func>
		void sources_if(const World& world, Entity relation, Entity target, Func func);
		const cnt::darray<Entity>& sources_bfs_trav_cache(const World& world, Entity relation, Entity rootTarget);
		template <typename Func>
		void sources_bfs(const World& world, Entity relation, Entity rootTarget, Func func);
		template <typename Func>
		bool sources_bfs_if(const World& world, Entity relation, Entity rootTarget, Func func);
		// Query API

		QuerySerBuffer& query_buffer(World& world, QueryId& serId);
		void query_buffer_reset(World& world, QueryId& serId);

		Entity expr_to_entity(const World& world, va_list& args, std::span<const char> exprRaw);

		GroupId group_by_func_default(const World& world, const Archetype& archetype, Entity groupBy);

		// Locking API

		void lock(World& world);
		void unlock(World& world);
		bool locked(const World& world);

#if GAIA_OBSERVERS_ENABLED
		// Deferred observer-notification API
		//
		// Observer dispatch mutates world-owned state and runs user callbacks, so it cannot happen
		// on a worker thread. Regions whose writes may run in parallel record notifications instead
		// and let the coordinator deliver them after the region joins.

		void world_defer_on_set_begin(World& world, uint32_t slotCount);
		void world_defer_on_set_end(World& world);
#endif

		// Deferred sorted-query invalidation API
		//
		// Sorted-query invalidation flips a shared `SortEntities` dirty bit on every sorted query
		// keyed by the written component. That shared flag is not safe to touch from a worker
		// thread, so regions whose writes may run in parallel record which component entities were
		// written and the coordinator applies the invalidation once the region joins. Work items
		// identify themselves with the shared deferral slot below, which the observer (`OnSet`)
		// channel uses as well.

		//! Slot value used while no deferred region is active on this thread.
		inline static constexpr uint32_t BadDeferSlot = (uint32_t)-1;

		void world_defer_sort_inv_begin(World& world, uint32_t slotCount);
		void world_defer_sort_inv_end(World& world);

		//! Opens both the `OnSet` and sorted-query-invalidation deferral regions of a parallel
		//! region as one unit; see world.h for the definitions.
		void world_defer_parallel_begin(World& world, uint32_t itemCount);
		void world_defer_parallel_end(World& world);

		//! Work-item slot the calling thread records deferred notifications into. Both the
		//! observer (`OnSet`) and sorted-query-invalidation channels read it, so a work item
		//! writes into one slot-aligned queue per channel. Thread-local so a work item keeps
		//! its own slot regardless of which thread runs it, including schedulers that execute
		//! work inline on the caller thread.
		GAIA_NODISCARD inline uint32_t& defer_slot_ref() {
			static thread_local uint32_t s_slot = BadDeferSlot;
			return s_slot;
		}

		//! Returns the deferred-notification slot owned by the calling thread.
		GAIA_NODISCARD inline uint32_t defer_slot() {
			return defer_slot_ref();
		}

		//! Binds the calling thread to a work-item deferral slot for the lifetime of the scope.
		class DeferSlotScope final {
			uint32_t m_prev;

		public:
			explicit DeferSlotScope(uint32_t slot): m_prev(defer_slot_ref()) {
				defer_slot_ref() = slot;
			}
			~DeferSlotScope() {
				defer_slot_ref() = m_prev;
			}

			DeferSlotScope(DeferSlotScope&&) = delete;
			DeferSlotScope(const DeferSlotScope&) = delete;
			DeferSlotScope& operator=(DeferSlotScope&&) = delete;
			DeferSlotScope& operator=(const DeferSlotScope&) = delete;
		};

		// CommandBuffer API

		CommandBufferST& cmd_buffer_st_get(World& world);
		CommandBufferMT& cmd_buffer_mt_get(World& world);
		void commit_cmd_buffer_st(World& world);
		void commit_cmd_buffer_mt(World& world);
	} // namespace ecs
} // namespace gaia
