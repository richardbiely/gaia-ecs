#pragma once

#include "../core/utility.h"
#include "component.h"
#include "component_cache.h"
#include "gaia/core/hashing_policy.h"
#include "id.h"

namespace gaia {
	namespace ecs {
		//! Updates the provided component matcher hash based on the provided component id
		//! \param matcherHash Initial matcher hash
		//! \param comp Component id
		inline void update_matcher_hash(ComponentMatcherHash& matcherHash, Entity entity) noexcept {
			auto hash = calc_matcher_hash(core::calculate_hash64(entity.value())).hash;
			matcherHash.hash = core::combine_or(matcherHash.hash, hash);
		}

		//! Calculates a component matcher hash from the provided component ids
		//! \param comps Span of component ids
		//! \return Component matcher hash
		GAIA_NODISCARD inline ComponentMatcherHash matcher_hash(EntitySpan comps) noexcept {
			const auto compsSize = comps.size();
			if (compsSize == 0)
				return {0};

			auto hash = calc_matcher_hash(core::calculate_hash64(comps[0].value())).hash;
			GAIA_FOR2(1, compsSize) {
				hash = core::combine_or(hash, calc_matcher_hash(core::calculate_hash64(comps[i].value())).hash);
			}
			return {hash};
		}

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