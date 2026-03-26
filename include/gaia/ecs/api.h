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
		template <typename T>
		const ComponentCacheItem& comp_cache_add(World& world);

		// Entity API

		const EntityContainer& fetch(const World& world, Entity entity);
		EntityContainer& fetch_mut(World& world, Entity entity);

		void del(World& world, Entity entity);

		Entity entity_from_id(const World& world, EntityId id);

		bool valid(const World& world, Entity entity);

		bool is(const World& world, Entity entity, Entity baseEntity);
		bool is_base(const World& world, Entity entity);

		Archetype* archetype_from_entity(const World& world, Entity entity);

		util::str_view entity_name(const World& world, Entity entity);
		util::str_view entity_name(const World& world, EntityId entityId);
		Entity target(const World& world, Entity entity, Entity relation);
		Entity world_pair_target_if_alive(const World& world, Entity pair);
		bool world_entity_enabled(const World& world, Entity entity);
		bool world_entity_enabled_hierarchy(const World& world, Entity entity, Entity relation);
		bool world_is_hierarchy_relation(const World& world, Entity relation);
		template <typename T>
		GAIA_NODISCARD decltype(auto) world_query_entity_arg_by_id(World& world, Entity entity, Entity id);

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
		template <typename Func>
		void children(const World& world, Entity parent, Func func);
		template <typename Func>
		void children_if(const World& world, Entity parent, Func func);
		template <typename Func>
		void children_bfs(const World& world, Entity root, Func func);
		template <typename Func>
		bool children_bfs_if(const World& world, Entity root, Func func);

		// Query API

		QuerySerBuffer& query_buffer(World& world, QueryId& serId);
		void query_buffer_reset(World& world, QueryId& serId);

		Entity expr_to_entity(const World& world, va_list& args, std::span<const char> exprRaw);

		GroupId group_by_func_default(const World& world, const Archetype& archetype, Entity groupBy);

		// Locking API

		void lock(World& world);
		void unlock(World& world);
		bool locked(const World& world);

		// CommandBuffer API

		CommandBufferST& cmd_buffer_st_get(World& world);
		CommandBufferMT& cmd_buffer_mt_get(World& world);
		void commit_cmd_buffer_st(World& world);
		void commit_cmd_buffer_mt(World& world);
	} // namespace ecs
} // namespace gaia
