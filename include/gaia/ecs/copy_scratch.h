#pragma once
#include "gaia/config/config.h"

#include <cstdint>

#include "gaia/cnt/darray_ext.h"
#include "gaia/ecs/id.h"

namespace gaia {
	namespace ecs {
		class Archetype;
		class Chunk;

		namespace detail {
			//! Contiguous destination rows emitted as one CopyIter callback range.
			struct CopyIterGroupState {
				//! Destination archetype for the pending range.
				Archetype* pArchetype = nullptr;
				//! Destination chunk for the pending range.
				Chunk* pChunk = nullptr;
				//! First destination row in \a pChunk.
				uint16_t startRow = 0;
				//! Number of contiguous rows in the pending range.
				uint16_t count = 0;
			};

			//! Prefab child edge discovered while building an instantiation or synchronization plan.
			struct PrefabChildEdge {
				//! Child prefab entity.
				Entity prefab = EntityBad;
				//! Hierarchy relation connecting the child prefab to its parent prefab.
				Entity relation = EntityBad;
			};

			//! One node in a prefab instantiation or synchronization plan.
			struct PrefabInstantiatePlanNode {
				//! Prefab entity represented by this node.
				Entity prefab = EntityBad;
				//! Parent node index in the plan, or BadIndex for a root node.
				uint32_t parentIdx = BadIndex;
				//! Hierarchy relation used to attach this node to its parent node.
				Entity parentRelation = EntityBad;
				//! Destination archetype used for instances of \a prefab.
				Archetype* pDstArchetype = nullptr;
				//! Non-fragmenting sparse component ids copied from \a prefab.
				cnt::darray_ext<Entity, 16> copiedSparseIds;
				//! Component or pair ids added to instances of \a prefab.
				cnt::darray_ext<Entity, 16> addedIds;
				//! Archetype component or pair ids reported to add hooks and observers.
				cnt::darray_ext<Entity, 16> addHookIds;
			};
		} // namespace detail
	} // namespace ecs
} // namespace gaia
