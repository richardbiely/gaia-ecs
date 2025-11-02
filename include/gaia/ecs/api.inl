#include "../config/config.h"

namespace gaia {
	namespace ecs {
		// Component API

		GAIA_NODISCARD inline const ComponentCache& comp_cache(const World& world) {
			return world.comp_cache();
		}

		GAIA_NODISCARD inline ComponentCache& comp_cache_mut(World& world) {
			return world.comp_cache_mut();
		}

		template <typename T>
		GAIA_NODISCARD inline const ComponentCacheItem& comp_cache_add(World& world) {
			return world.add<T>();
		}

		// Entity API

		GAIA_NODISCARD inline const EntityContainer& fetch(const World& world, Entity entity) {
			return world.fetch(entity);
		}

		GAIA_NODISCARD inline EntityContainer& fetch_mut(World& world, Entity entity) {
			return world.fetch(entity);
		}

		inline void del(World& world, Entity entity) {
			world.del(entity);
		}

		GAIA_NODISCARD inline Entity entity_from_id(const World& world, EntityId id) {
			return world.get(id);
		}

		GAIA_NODISCARD inline bool valid(const World& world, Entity entity) {
			return world.valid(entity);
		}

		GAIA_NODISCARD inline bool is(const World& world, Entity entity, Entity baseEntity) {
			return world.is(entity, baseEntity);
		}

		GAIA_NODISCARD inline bool is_base(const World& world, Entity entity) {
			return world.is_base(entity);
		}

		GAIA_NODISCARD inline Archetype* archetype_from_entity(const World& world, Entity entity) {
			const auto& ec = world.fetch(entity);
			if (World::is_req_del(ec))
				return nullptr;

			return ec.pArchetype;
		}

		GAIA_NODISCARD inline const char* entity_name(const World& world, Entity entity) {
			return world.name(entity);
		}

		GAIA_NODISCARD inline const char* entity_name(const World& world, EntityId entityId) {
			return world.name(entityId);
		}

		// Traversal API

		template <typename Func>
		inline void as_relations_trav(const World& world, Entity target, Func func) {
			world.as_relations_trav(target, func);
		}

		template <typename Func>
		inline bool as_relations_trav_if(const World& world, Entity target, Func func) {
			return world.as_relations_trav_if(target, func);
		}

		template <typename Func>
		inline void as_targets_trav(const World& world, Entity relation, Func func) {
			world.as_targets_trav(relation, func);
		}

		template <typename Func>
		inline bool as_targets_trav_if(const World& world, Entity relation, Func func) {
			return world.as_targets_trav_if(relation, func);
		}

		// Query API

		GAIA_NODISCARD inline QuerySerBuffer& query_buffer(World& world, QueryId& serId) {
			return world.query_buffer(serId);
		}

		inline void query_buffer_reset(World& world, QueryId& serId) {
			world.query_buffer_reset(serId);
		}

		GAIA_NODISCARD inline Entity expr_to_entity(const World& world, va_list& args, std::span<const char> exprRaw) {
			return world.expr_to_entity(args, exprRaw);
		}

		// Locking API

		inline void lock(World& world) {
			world.lock();
		}
		inline void unlock(World& world) {
			world.unlock();
		}
		GAIA_NODISCARD inline bool locked(const World& world) {
			return world.locked();
		}

		// CommandBuffer API

		GAIA_NODISCARD inline CommandBufferST& cmd_buffer_st_get(World& world) {
			return world.cmd_buffer_st();
		}

		GAIA_NODISCARD inline CommandBufferMT& cmd_buffer_mt_get(World& world) {
			return world.cmd_buffer_mt();
		}

		inline void commit_cmd_buffer_st(World& world) {
			if (world.locked())
				return;
			cmd_buffer_commit(world.cmd_buffer_st());
		}

		inline void commit_cmd_buffer_mt(World& world) {
			if (world.locked())
				return;
			cmd_buffer_commit(world.cmd_buffer_mt());
		}
	} // namespace ecs
} // namespace gaia