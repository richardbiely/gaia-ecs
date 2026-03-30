#include "gaia/config/config.h"

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
			return world.reg_comp<T>();
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
			if (!world.valid(entity))
				return nullptr;

			const auto& ec = world.fetch(entity);
			if (World::is_req_del(ec))
				return nullptr;

			return ec.pArchetype;
		}

		GAIA_NODISCARD inline util::str_view entity_name(const World& world, Entity entity) {
			const auto name = world.name(entity);
			if (!name.empty())
				return name;

			if (!entity.pair()) {
				const auto display = world.display_name(entity);
				if (!display.empty())
					return display;
			}

			return {};
		}

		GAIA_NODISCARD inline util::str_view entity_name(const World& world, EntityId entityId) {
			return entity_name(world, world.get(entityId));
		}

		GAIA_NODISCARD inline Entity target(const World& world, Entity entity, Entity relation) {
			return world.target(entity, relation);
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

		template <typename Func>
		inline void targets_trav(const World& world, Entity relation, Entity source, Func func) {
			world.targets_trav(relation, source, func);
		}

		template <typename Func>
		inline bool targets_trav_if(const World& world, Entity relation, Entity source, Func func) {
			return world.targets_trav_if(relation, source, func);
		}

		GAIA_NODISCARD inline const cnt::darray<Entity>&
		targets_trav_cache(const World& world, Entity relation, Entity source) {
			return world.targets_trav_cache(relation, source);
		}

		template <typename Func>
		inline void sources(const World& world, Entity relation, Entity target, Func func) {
			world.sources(relation, target, func);
		}

		template <typename Func>
		inline void sources_if(const World& world, Entity relation, Entity target, Func func) {
			world.sources_if(relation, target, func);
		}

		GAIA_NODISCARD inline const cnt::darray<Entity>&
		sources_bfs_trav_cache(const World& world, Entity relation, Entity rootTarget) {
			return world.sources_bfs_trav_cache(relation, rootTarget);
		}

		template <typename Func>
		inline void sources_bfs(const World& world, Entity relation, Entity rootTarget, Func func) {
			world.sources_bfs(relation, rootTarget, func);
		}

		template <typename Func>
		inline bool sources_bfs_if(const World& world, Entity relation, Entity rootTarget, Func func) {
			return world.sources_bfs_if(relation, rootTarget, func);
		}

		template <typename Func>
		inline void children(const World& world, Entity parent, Func func) {
			world.children(parent, func);
		}

		template <typename Func>
		inline void children_if(const World& world, Entity parent, Func func) {
			world.children_if(parent, func);
		}

		template <typename Func>
		inline void children_bfs(const World& world, Entity root, Func func) {
			world.children_bfs(root, func);
		}

		template <typename Func>
		inline bool children_bfs_if(const World& world, Entity root, Func func) {
			return world.children_bfs_if(root, func);
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
