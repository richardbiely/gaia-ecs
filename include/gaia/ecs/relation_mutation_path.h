#pragma once
#include "gaia/config/config.h"

#include <cstdint>

namespace gaia {
	namespace ecs {
		namespace detail {
			//! Classifies a pair add/remove by the relation's storage model.
			enum class RelationMutationPath : uint8_t {
				//! Relation target participates in archetype identity.
				Fragmenting,
				//! Exclusive non-fragmenting relation stored outside archetype identity.
				NonFragmentingExclusive,
				//! Non-fragmenting pair stored through the generic archetype path.
				NonFragmentingArchetypePair
			};
		} // namespace detail
	} // namespace ecs
} // namespace gaia
