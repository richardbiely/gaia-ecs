#pragma once

#include <cinttypes>

#include "../cnt/map.h"
#include "../config/logging.h"
#include "archetype_common.h"
#include "component.h"
#include "component_cache.h"

namespace gaia {
	namespace ecs {
		namespace archetype {
			class ArchetypeGraph {
				struct ArchetypeGraphEdge {
					ArchetypeId archetypeId;
				};

				//! Map of edges in the archetype graph when adding components
				cnt::map<component::ComponentId, ArchetypeGraphEdge> m_edgesAdd[component::ComponentType::CT_Count];
				//! Map of edges in the archetype graph when removing components
				cnt::map<component::ComponentId, ArchetypeGraphEdge> m_edgesDel[component::ComponentType::CT_Count];

			public:
				//! Creates an edge in the graph leading to the target archetype \param archetypeId via component \param
				//! componentId of type \param compType.
				void
				add_edge_right(component::ComponentType compType, component::ComponentId componentId, ArchetypeId archetypeId) {
					[[maybe_unused]] const auto ret =
							m_edgesAdd[compType].try_emplace({componentId}, ArchetypeGraphEdge{archetypeId});
					GAIA_ASSERT(ret.second);
				}

				//! Creates an edge in the graph leading to the target archetype \param archetypeId via component \param
				//! componentId of type \param compType.
				void
				add_edge_left(component::ComponentType compType, component::ComponentId componentId, ArchetypeId archetypeId) {
					[[maybe_unused]] const auto ret =
							m_edgesDel[compType].try_emplace({componentId}, ArchetypeGraphEdge{archetypeId});
					GAIA_ASSERT(ret.second);
				}

				//! Checks if the graph edge for component type \param compType contains the component \param componentId.
				//! \return Archetype id of the target archetype if the edge is found. ArchetypeIdBad otherwise.
				GAIA_NODISCARD ArchetypeId
				find_edge_right(component::ComponentType compType, const component::ComponentId componentId) const {
					const auto& edges = m_edgesAdd[compType];
					const auto it = edges.find({componentId});
					return it != edges.end() ? it->second.archetypeId : ArchetypeIdBad;
				}

				//! Checks if the graph edge for component type \param compType contains the component \param componentId.
				//! \return Archetype id of the target archetype if the edge is found. ArchetypeIdBad otherwise.
				GAIA_NODISCARD ArchetypeId
				find_edge_left(component::ComponentType compType, const component::ComponentId componentId) const {
					const auto& edges = m_edgesDel[compType];
					const auto it = edges.find({componentId});
					return it != edges.end() ? it->second.archetypeId : ArchetypeIdBad;
				}

				void diag() const {
					// const auto& cc = ComponentCache::get();

					// // Add edges (movement towards the leafs)
					// {
					// 	const auto& edgesG = m_edgesAdd[component::ComponentType::CT_Generic];
					// 	const auto& edgesC = m_edgesAdd[component::ComponentType::CT_Chunk];
					// 	const auto edgeCount = (uint32_t)(edgesG.size() + edgesC.size());
					// 	if (edgeCount > 0) {
					// 		GAIA_LOG_N("  Add edges - count:%u", edgeCount);

					// 		if (!edgesG.empty()) {
					// 			GAIA_LOG_N("    Generic - count:%u", (uint32_t)edgesG.size());
					// 			for (const auto& edge: edgesG) {
					// 				const auto& info = cc.comp_info(edge.first);
					// 				const auto& infoCreate = cc.comp_desc(info.componentId);
					// 				GAIA_LOG_N(
					// 						"      %.*s (--> Archetype ID:%u)", (uint32_t)infoCreate.name.size(), infoCreate.name.data(),
					// 						edge.second.archetypeId);
					// 			}
					// 		}

					// 		if (!edgesC.empty()) {
					// 			GAIA_LOG_N("    Chunk - count:%u", (uint32_t)edgesC.size());
					// 			for (const auto& edge: edgesC) {
					// 				const auto& info = cc.comp_info(edge.first);
					// 				const auto& infoCreate = cc.comp_desc(info.componentId);
					// 				GAIA_LOG_N(
					// 						"      %.*s (--> Archetype ID:%u)", (uint32_t)infoCreate.name.size(), infoCreate.name.data(),
					// 						edge.second.archetypeId);
					// 			}
					// 		}
					// 	}
					// }

					// // Delete edges (movement towards the root)
					// {
					// 	const auto& edgesG = m_edgesDel[component::ComponentType::CT_Generic];
					// 	const auto& edgesC = m_edgesDel[component::ComponentType::CT_Chunk];
					// 	const auto edgeCount = (uint32_t)(edgesG.size() + edgesC.size());
					// 	if (edgeCount > 0) {
					// 		GAIA_LOG_N("  del edges - count:%u", edgeCount);

					// 		if (!edgesG.empty()) {
					// 			GAIA_LOG_N("    Generic - count:%u", (uint32_t)edgesG.size());
					// 			for (const auto& edge: edgesG) {
					// 				const auto& info = cc.comp_info(edge.first);
					// 				const auto& infoCreate = cc.comp_desc(info.componentId);
					// 				GAIA_LOG_N(
					// 						"      %.*s (--> Archetype ID:%u)", (uint32_t)infoCreate.name.size(), infoCreate.name.data(),
					// 						edge.second.archetypeId);
					// 			}
					// 		}

					// 		if (!edgesC.empty()) {
					// 			GAIA_LOG_N("    Chunk - count:%u", (uint32_t)edgesC.size());
					// 			for (const auto& edge: edgesC) {
					// 				const auto& info = cc.comp_info(edge.first);
					// 				const auto& infoCreate = cc.comp_desc(info.componentId);
					// 				GAIA_LOG_N(
					// 						"      %.*s (--> Archetype ID:%u)", (uint32_t)infoCreate.name.size(), infoCreate.name.data(),
					// 						edge.second.archetypeId);
					// 			}
					// 		}
					// 	}
					// }
				}
			};
		} // namespace archetype
	} // namespace ecs
} // namespace gaia