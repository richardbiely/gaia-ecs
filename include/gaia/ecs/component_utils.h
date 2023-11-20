#pragma once

#include "../core/utility.h"
#include "component.h"
#include "component_cache.h"
#include "id.h"

namespace gaia {
	namespace ecs {
		//! Updates the provided component matcher hash based on the provided component id
		//! \param matcherHash Initial matcher hash
		//! \param comp Component id
		inline void matcher_hash(const ComponentCache& cc, ComponentMatcherHash& matcherHash, Component comp) noexcept {
			const auto componentHash = cc.get(comp.id()).matcherHash.hash;
			matcherHash.hash = core::combine_or(matcherHash.hash, componentHash);
		}

		//! Calculates a component matcher hash from the provided component ids
		//! \param comps Span of component ids
		//! \return Component matcher hash
		GAIA_NODISCARD inline ComponentMatcherHash matcher_hash(const ComponentCache& cc, ComponentSpan comps) noexcept {
			const auto compsSize = comps.size();
			if (compsSize == 0)
				return {0};

			ComponentMatcherHash::Type hash = cc.get(comps[0].id()).matcherHash.hash;
			GAIA_FOR2(1, compsSize) hash = core::combine_or(hash, cc.get(comps[i].id()).matcherHash.hash);
			return {hash};
		}

		//! Calculates a component lookup hash from the provided component ids
		//! \param comps Span of component ids
		//! \return Component lookup hash
		GAIA_NODISCARD inline ComponentLookupHash calc_lookup_hash(const ComponentCache& cc, ComponentSpan comps) noexcept {
			const auto compsSize = comps.size();
			if (compsSize == 0)
				return {0};

			ComponentLookupHash::Type hash = cc.get(comps[0].id()).hashLookup.hash;
			GAIA_FOR2(1, compsSize) hash = core::hash_combine(hash, cc.get(comps[i].id()).hashLookup.hash);
			return {hash};
		}

		using SortComponentCond = core::is_smaller<Component>;
		using SortComponentIdCond = core::is_smaller<ComponentId>;
	} // namespace ecs
} // namespace gaia