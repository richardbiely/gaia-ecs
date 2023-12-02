#pragma once

#include <cstdint>

#include "../cnt/map.h"
#include "../config/logging.h"
#include "archetype_common.h"
#include "component.h"
#include "id.h"

namespace gaia {
	namespace ecs {
		class World;
		const char* entity_name(const World& world, Entity entity);
		const char* entity_name(const World& world, EntityId entityId);

		class ArchetypeGraph {
			struct ArchetypeGraphEdge {
				ArchetypeId archetypeId;
			};

			//! Map of edges in the archetype graph when adding components
			cnt::map<EntityLookupKey, ArchetypeGraphEdge> m_edgesAdd;
			//! Map of edges in the archetype graph when removing components
			cnt::map<EntityLookupKey, ArchetypeGraphEdge> m_edgesDel;

		public:
			//! Creates an edge in the graph leading to the target archetype \param archetypeId via component \param
			//! comp of type \param kind.
			void add_edge_right(Entity entity, ArchetypeId archetypeId) {
				[[maybe_unused]] const auto ret =
						m_edgesAdd.try_emplace(EntityLookupKey(entity), ArchetypeGraphEdge{archetypeId});
				GAIA_ASSERT(ret.second);
			}

			//! Creates an edge in the graph leading to the target archetype \param archetypeId via component \param
			//! comp of type \param kind.
			void add_edge_left(Entity entity, ArchetypeId archetypeId) {
				[[maybe_unused]] const auto ret =
						m_edgesDel.try_emplace(EntityLookupKey(entity), ArchetypeGraphEdge{archetypeId});
				GAIA_ASSERT(ret.second);
			}

			//! Checks if an archetype graph "add" edge with entity \param entity exists.
			//! \return Archetype id of the target archetype if the edge is found. ArchetypeIdBad otherwise.
			GAIA_NODISCARD ArchetypeId find_edge_right(Entity entity) const {
				const auto it = m_edgesAdd.find(EntityLookupKey(entity));
				return it != m_edgesAdd.end() ? it->second.archetypeId : ArchetypeIdBad;
			}

			//! Checks if an archetype graph "del" edge with entity \param entity exists.
			//! \return Archetype id of the target archetype if the edge is found. ArchetypeIdBad otherwise.
			GAIA_NODISCARD ArchetypeId find_edge_left(Entity entity) const {
				const auto it = m_edgesDel.find(EntityLookupKey(entity));
				return it != m_edgesDel.end() ? it->second.archetypeId : ArchetypeIdBad;
			}

			void diag(const World& world) const {
				auto diagEdge = [&](const auto& edges, bool addEdge) {
					for (const auto& edge: edges) {
						const auto entity = edge.first.entity();
						if (entity.pair()) {
							const auto* name0 = entity_name(world, entity.id());
							const auto* name1 = entity_name(world, entity.gen());
							GAIA_LOG_N(
									"      pair [%u,%u] %s -> %s  (--%c Archetype ID:%u)", entity.id(), entity.gen(), name0, name1,
									addEdge ? '>' : '<', edge.second.archetypeId);
						} else {
							const auto* name = entity_name(world, entity);
							GAIA_LOG_N(
									"      ent [%u:%u] %s [%s] [] (--%c Archetype ID:%u)", entity.id(), entity.gen(), name,
									EntityKindString[entity.kind()], addEdge ? '>' : '<', edge.second.archetypeId);
						}
					}
				};

				// Add edges (movement towards the leafs)
				if (!m_edgesAdd.empty()) {
					GAIA_LOG_N("  Add edges - count:%u", (uint32_t)m_edgesAdd.size());
					diagEdge(m_edgesAdd, true);
				}

				// Delete edges (movement towards the root)
				if (!m_edgesDel.empty()) {
					GAIA_LOG_N("  Del edges - count:%u", (uint32_t)m_edgesDel.size());
					diagEdge(m_edgesDel, false);
				}
			}
		};
	} // namespace ecs
} // namespace gaia