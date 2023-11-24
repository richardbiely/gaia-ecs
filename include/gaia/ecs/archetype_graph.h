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
			cnt::map<EntityLookupKey, ArchetypeGraphEdge> m_edgesAdd[EntityKind::EK_Count];
			//! Map of edges in the archetype graph when removing components
			cnt::map<EntityLookupKey, ArchetypeGraphEdge> m_edgesDel[EntityKind::EK_Count];

		public:
			//! Creates an edge in the graph leading to the target archetype \param archetypeId via component \param
			//! comp of type \param kind.
			void add_edge_right(EntityKind kind, Entity entity, ArchetypeId archetypeId) {
				[[maybe_unused]] const auto ret = m_edgesAdd[kind].try_emplace({entity}, ArchetypeGraphEdge{archetypeId});
				GAIA_ASSERT(ret.second);
			}

			//! Creates an edge in the graph leading to the target archetype \param archetypeId via component \param
			//! comp of type \param kind.
			void add_edge_left(EntityKind kind, Entity entity, ArchetypeId archetypeId) {
				[[maybe_unused]] const auto ret = m_edgesDel[kind].try_emplace({entity}, ArchetypeGraphEdge{archetypeId});
				GAIA_ASSERT(ret.second);
			}

			//! Checks if the graph edge for component type \param kind contains the component \param comp.
			//! \return Archetype id of the target archetype if the edge is found. ArchetypeIdBad otherwise.
			GAIA_NODISCARD ArchetypeId find_edge_right(EntityKind kind, Entity entity) const {
				const auto& edges = m_edgesAdd[kind];
				const auto it = edges.find({entity});
				return it != edges.end() ? it->second.archetypeId : ArchetypeIdBad;
			}

			//! Checks if the graph edge for component type \param kind contains the component \param comp.
			//! \return Archetype id of the target archetype if the edge is found. ArchetypeIdBad otherwise.
			GAIA_NODISCARD ArchetypeId find_edge_left(EntityKind kind, Entity entity) const {
				const auto& edges = m_edgesDel[kind];
				const auto it = edges.find({entity});
				return it != edges.end() ? it->second.archetypeId : ArchetypeIdBad;
			}

			void diag(const World& world) const {
				// Add edges (movement towards the leafs)
				{
					const auto& edgesG = m_edgesAdd[EntityKind::EK_Gen];
					const auto& edgesC = m_edgesAdd[EntityKind::EK_Uni];
					const auto edgeCount = (uint32_t)(edgesG.size() + edgesC.size());
					if (edgeCount > 0) {
						GAIA_LOG_N("  Add edges - count:%u", edgeCount);

						if (!edgesG.empty()) {
							GAIA_LOG_N("    Generic - count:%u", (uint32_t)edgesG.size());
							for (const auto& edge: edgesG) {
								const auto* name = entity_name(world, edge.first.entity());
								GAIA_LOG_N("      %s (--> Archetype ID:%u)", name, edge.second.archetypeId);
							}
						}

						if (!edgesC.empty()) {
							GAIA_LOG_N("    Unique - count:%u", (uint32_t)edgesC.size());
							for (const auto& edge: edgesC) {
								const auto* name = entity_name(world, edge.first.entity());
								GAIA_LOG_N("      %s (--> Archetype ID:%u)", name, edge.second.archetypeId);
							}
						}
					}
				}

				// Delete edges (movement towards the root)
				{
					const auto& edgesG = m_edgesDel[EntityKind::EK_Gen];
					const auto& edgesC = m_edgesDel[EntityKind::EK_Uni];
					const auto edgeCount = (uint32_t)(edgesG.size() + edgesC.size());
					if (edgeCount > 0) {
						GAIA_LOG_N("  del edges - count:%u", edgeCount);

						if (!edgesG.empty()) {
							GAIA_LOG_N("    Generic - count:%u", (uint32_t)edgesG.size());
							for (const auto& edge: edgesG) {
								const auto* name = entity_name(world, edge.first.entity());
								GAIA_LOG_N("      %s (--> Archetype ID:%u)", name, edge.second.archetypeId);
							}
						}

						if (!edgesC.empty()) {
							GAIA_LOG_N("    Chunk - count:%u", (uint32_t)edgesC.size());
							for (const auto& edge: edgesC) {
								const auto* name = entity_name(world, edge.first.entity());
								GAIA_LOG_N("      %s (--> Archetype ID:%u)", name, edge.second.archetypeId);
							}
						}
					}
				}
			}
		};
	} // namespace ecs
} // namespace gaia