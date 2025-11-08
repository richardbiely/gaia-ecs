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

		const char* entity_name(const World& world, Entity entity);
		const char* entity_name(const World& world, EntityId entityId);

		// Traversal API

		template <typename Func>
		void as_relations_trav(const World& world, Entity target, Func func);
		template <typename Func>
		bool as_relations_trav_if(const World& world, Entity target, Func func);
		template <typename Func>
		void as_targets_trav(const World& world, Entity relation, Func func);
		template <typename Func>
		bool as_targets_trav_if(const World& world, Entity relation, Func func);

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