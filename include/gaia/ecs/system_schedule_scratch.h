#pragma once
#include "gaia/config/config.h"

#include <cstdint>

#include "gaia/cnt/darray.h"
#include "gaia/core/utility.h"
#include "gaia/ecs/id.h"
#include "gaia/ecs/sched.h"

namespace gaia {
	namespace ecs {
		class World;

		namespace detail {
			//! Pending scheduler-backed system job owned by World::systems_run().
			struct PendingSystemJob {
				//! System entity that created \a job.
				Entity entity = EntityBad;
				//! Deferred system work.
				SchedJob job;

				PendingSystemJob() = default;
				//! Creates a pending entry for \a systemEntity.
				//! \param systemEntity System entity that created \a systemJob.
				//! \param systemJob Deferred system job moved into this entry.
				PendingSystemJob(Entity systemEntity, SchedJob&& systemJob): entity(systemEntity), job(GAIA_MOV(systemJob)) {}
			};

			//! Scheduling key for one enabled system entity.
			struct SystemScheduleItem {
				//! System entity to run.
				Entity entity = EntityBad;
				//! Phase entity assigned with SystemBuilder::phase(), or EntityBad for unphased systems.
				Entity phase = EntityBad;
				//! Depth of \a phase in the phase DependsOn graph.
				uint32_t phaseDepth = 0;
				//! Depth of the system in the DependsOn graph, excluding the phase marker target.
				uint32_t systemDepth = 0;
				//! Deterministic child-before-target order of \a phase.
				uint32_t phaseOrder = 0;
				//! Deterministic child-before-target order of \a entity inside its scheduling group.
				uint32_t systemOrder = 0;
				//! True when \a phase is valid.
				bool hasPhase = false;
			};

			//! Scheduling key for one explicit phase entity.
			struct SystemPhaseScheduleItem {
				//! Phase entity.
				Entity phase = EntityBad;
				//! Depth of \a phase in the phase DependsOn graph.
				uint32_t depth = 0;
				//! Deterministic child-before-target order of \a phase.
				uint32_t order = 0;
			};

			//! Direct child-before-target scheduling edge between collected systems.
			struct SystemScheduleEdge {
				//! Item index that must run first.
				uint32_t child = 0;
				//! Item index that must run after \a child.
				uint32_t target = 0;
				//! Next edge index in the same child adjacency list.
				uint32_t next = UINT32_MAX;
			};

			//! Reusable scratch arrays for one system schedule ordering pass.
			struct SystemScheduleScratch {
				//! Collected systems for the current run.
				cnt::darray<SystemScheduleItem> items;
				//! Collected unique phases for the current run.
				cnt::darray<SystemPhaseScheduleItem> phases;
				//! Explicit dependency edges for the current run.
				cnt::darray<SystemScheduleEdge> edges;
				//! Ordered item output used when explicit edges exist.
				cnt::darray<SystemScheduleItem> ordered;
				//! Shared entity traversal stack.
				cnt::darray<Entity> entityStack;
				//! Item indices sorted by entity id.
				cnt::darray<uint32_t> entityIndices;
				//! Item indices sorted inside the active group.
				cnt::darray<uint32_t> sortedGroupIndices;
				//! Primary dependency target per item.
				cnt::darray<uint32_t> primaryTargets;
				//! First child index per target item.
				cnt::darray<uint32_t> firstChildren;
				//! Next sibling index per child item.
				cnt::darray<uint32_t> nextSiblings;
				//! Phase indices sorted by entity id.
				cnt::darray<uint32_t> sortedPhases;
				//! Primary dependency target per phase.
				cnt::darray<uint32_t> primaryPhases;
				//! Reused group item indices.
				cnt::darray<uint32_t> groupIndices;
				//! Direct child count per target item.
				cnt::darray<uint32_t> childCounts;
				//! Item indices sorted by final deterministic key.
				cnt::darray<uint32_t> sortedIndices;
				//! First edge index per child item.
				cnt::darray<uint32_t> firstEdges;
				//! Next item in the ready list.
				cnt::darray<uint32_t> readyNext;
				//! Visit states reused by group and phase traversal.
				cnt::darray<uint8_t> states;
				//! Visit states for the final topological pass.
				cnt::darray<uint8_t> visited;
			};

			//! Context used while collecting system scheduling keys.
			struct SystemCollectCtx {
				//! World that owns collected systems.
				World* pWorld = nullptr;
				//! Output array receiving scheduling keys.
				cnt::darray<SystemScheduleItem>* pItems = nullptr;
			};

			//! Context used by the erased system run callback.
			struct SystemRunCtx {
				//! World that owns collected systems.
				World* pWorld = nullptr;
				//! Pending scheduler jobs in the current phase/system-dependency batch.
				cnt::darray<PendingSystemJob>* pPending = nullptr;
				//! Current scheduling batch key.
				SystemScheduleItem current{};
				//! True once \a current has been initialized.
				bool hasCurrent = false;
				//! True when the active scheduler can prepare dependency-ready jobs.
				bool canScheduleSystems = false;
			};
		} // namespace detail
	} // namespace ecs
} // namespace gaia
