#pragma once

#include <cinttypes>

#include "../config/logging.h"
#include "../containers/map.h"
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
				containers::map<component::ComponentId, ArchetypeGraphEdge> m_edgesAdd[component::ComponentType::CT_Count];
				//! Map of edges in the archetype graph when removing components
				containers::map<component::ComponentId, ArchetypeGraphEdge> m_edgesDel[component::ComponentType::CT_Count];

			public:
				//! Creates an edge in the graph leading to the target archetype \param archetypeId via component \param
				//! componentId of type \param componentType.
				void AddGraphEdgeRight(
						component::ComponentType componentType, component::ComponentId componentId, ArchetypeId archetypeId) {
					[[maybe_unused]] const auto ret =
							m_edgesAdd[componentType].try_emplace({componentId}, ArchetypeGraphEdge{archetypeId});
					GAIA_ASSERT(ret.second);
				}

				//! Creates an edge in the graph leading to the target archetype \param archetypeId via component \param
				//! componentId of type \param componentType.
				void AddGraphEdgeLeft(
						component::ComponentType componentType, component::ComponentId componentId, ArchetypeId archetypeId) {
					[[maybe_unused]] const auto ret =
							m_edgesDel[componentType].try_emplace({componentId}, ArchetypeGraphEdge{archetypeId});
					GAIA_ASSERT(ret.second);
				}

				//! Checks if the graph edge for component type \param componentType contains the component \param componentId.
				//! \return Archetype id of the target archetype if the edge is found. ArchetypeIdBad otherwise.
				GAIA_NODISCARD ArchetypeId
				FindGraphEdgeRight(component::ComponentType componentType, const component::ComponentId componentId) const {
					const auto& edges = m_edgesAdd[componentType];
					const auto it = edges.find({componentId});
					return it != edges.end() ? it->second.archetypeId : ArchetypeIdBad;
				}

				//! Checks if the graph edge for component type \param componentType contains the component \param componentId.
				//! \return Archetype id of the target archetype if the edge is found. ArchetypeIdBad otherwise.
				GAIA_NODISCARD ArchetypeId
				FindGraphEdgeLeft(component::ComponentType componentType, const component::ComponentId componentId) const {
					const auto& edges = m_edgesDel[componentType];
					const auto it = edges.find({componentId});
					return it != edges.end() ? it->second.archetypeId : ArchetypeIdBad;
				}

				void Diag() const {
					const auto& cc = ComponentCache::Get();

					// Add edges (movement towards the leafs)
					{
						const auto& edgesG = m_edgesAdd[component::ComponentType::CT_Generic];
						const auto& edgesC = m_edgesAdd[component::ComponentType::CT_Chunk];
						const auto edgeCount = (uint32_t)(edgesG.size() + edgesC.size());
						if (edgeCount > 0) {
							GAIA_LOG_N("  Add edges - count:%u", edgeCount);

							if (!edgesG.empty()) {
								GAIA_LOG_N("    Generic - count:%u", (uint32_t)edgesG.size());
								for (const auto& edge: edgesG) {
									const auto& info = cc.GetComponentInfo(edge.first);
									const auto& infoCreate = cc.GetComponentDesc(info.componentId);
									GAIA_LOG_N(
											"      %.*s (--> Archetype ID:%u)", (uint32_t)infoCreate.name.size(), infoCreate.name.data(),
											edge.second.archetypeId);
								}
							}

							if (!edgesC.empty()) {
								GAIA_LOG_N("    Chunk - count:%u", (uint32_t)edgesC.size());
								for (const auto& edge: edgesC) {
									const auto& info = cc.GetComponentInfo(edge.first);
									const auto& infoCreate = cc.GetComponentDesc(info.componentId);
									GAIA_LOG_N(
											"      %.*s (--> Archetype ID:%u)", (uint32_t)infoCreate.name.size(), infoCreate.name.data(),
											edge.second.archetypeId);
								}
							}
						}
					}

					// Delete edges (movement towards the root)
					{
						const auto& edgesG = m_edgesDel[component::ComponentType::CT_Generic];
						const auto& edgesC = m_edgesDel[component::ComponentType::CT_Chunk];
						const auto edgeCount = (uint32_t)(edgesG.size() + edgesC.size());
						if (edgeCount > 0) {
							GAIA_LOG_N("  Del edges - count:%u", edgeCount);

							if (!edgesG.empty()) {
								GAIA_LOG_N("    Generic - count:%u", (uint32_t)edgesG.size());
								for (const auto& edge: edgesG) {
									const auto& info = cc.GetComponentInfo(edge.first);
									const auto& infoCreate = cc.GetComponentDesc(info.componentId);
									GAIA_LOG_N(
											"      %.*s (--> Archetype ID:%u)", (uint32_t)infoCreate.name.size(), infoCreate.name.data(),
											edge.second.archetypeId);
								}
							}

							if (!edgesC.empty()) {
								GAIA_LOG_N("    Chunk - count:%u", (uint32_t)edgesC.size());
								for (const auto& edge: edgesC) {
									const auto& info = cc.GetComponentInfo(edge.first);
									const auto& infoCreate = cc.GetComponentDesc(info.componentId);
									GAIA_LOG_N(
											"      %.*s (--> Archetype ID:%u)", (uint32_t)infoCreate.name.size(), infoCreate.name.data(),
											edge.second.archetypeId);
								}
							}
						}
					}
				}
			};
		} // namespace archetype
	} // namespace ecs
} // namespace gaia