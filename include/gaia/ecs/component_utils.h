#pragma once

#include "../core/hashing_policy.h"
#include "../core/utility.h"
#include "component.h"
#include "component_cache.h"
#include "id.h"

namespace gaia {
	namespace ecs {
		//! Calculates a lookup hash from the provided entities
		//! \param comps Span of entities
		//! \return Lookup hash
		GAIA_NODISCARD inline ComponentLookupHash calc_lookup_hash(EntitySpan comps) noexcept {
			const auto compsSize = comps.size();
			if (compsSize == 0)
				return {0};

			auto hash = core::calculate_hash64(comps[0].value());
			GAIA_FOR2(1, compsSize) {
				hash = core::hash_combine(hash, core::calculate_hash64(comps[i].value()));
			}
			return {hash};
		}

		using SortComponentCond = core::is_smaller<Entity>;
	} // namespace ecs
} // namespace gaia