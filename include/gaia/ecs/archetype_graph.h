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
				[[maybe_unused]] const auto ret = m_edgesAdd.try_emplace({entity}, ArchetypeGraphEdge{archetypeId});
				GAIA_ASSERT(ret.second);
			}

			//! Creates an edge in the graph leading to the target archetype \param archetypeId via component \param
			//! comp of type \param kind.
			void add_edge_left(Entity entity, ArchetypeId archetypeId) {
				[[maybe_unused]] const auto ret = m_edgesDel.try_emplace({entity}, ArchetypeGraphEdge{archetypeId});
				GAIA_ASSERT(ret.second);
			}

			//! Checks if an archetype graph "add" edge with entity \param entity exists.
			//! \return Archetype id of the target archetype if the edge is found. ArchetypeIdBad otherwise.
			GAIA_NODISCARD ArchetypeId find_edge_right(Entity entity) const {
				const auto it = m_edgesAdd.find({entity});
				return it != m_edgesAdd.end() ? it->second.archetypeId : ArchetypeIdBad;
			}

			//! Checks if an archetype graph "del" edge with entity \param entity exists.
			//! \return Archetype id of the target archetype if the edge is found. ArchetypeIdBad otherwise.
			GAIA_NODISCARD ArchetypeId find_edge_left(Entity entity) const {
				const auto it = m_edgesDel.find({entity});
				return it != m_edgesDel.end() ? it->second.archetypeId : ArchetypeIdBad;
			}

			void diag(const World& world) const {
				// Add edges (movement towards the leafs)
				if (!m_edgesAdd.empty()) {
					GAIA_LOG_N("  Add edges - count:%u", (uint32_t)m_edgesAdd.size());

					for (const auto& edge: m_edgesAdd) {
						const auto entity = edge.first.entity();
						const auto* name = entity_name(world, entity);
						GAIA_LOG_N(
								"      %s [%s] (--> Archetype ID:%u)", name, EntityKindString[entity.kind()], edge.second.archetypeId);
					}
				}

				// Delete edges (movement towards the root)
				if (!m_edgesDel.empty()) {
					GAIA_LOG_N("  Del edges - count:%u", (uint32_t)m_edgesDel.size());

					for (const auto& edge: m_edgesDel) {
						const auto entity = edge.first.entity();
						const auto* name = entity_name(world, entity);
						GAIA_LOG_N(
								"      %s [%s] (--> Archetype ID:%u)", name, EntityKindString[entity.kind()], edge.second.archetypeId);
					}
				}
			}
		};
	} // namespace ecs
} // namespace gaia