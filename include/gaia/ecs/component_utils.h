#pragma once

#include "../core/utility.h"
#include "component.h"
#include "component_cache.h"

namespace gaia {
	namespace ecs {
		//! Updates the provided component matcher hash based on the provided component id
		//! \param matcherHash Initial matcher hash
		//! \param compId Component id
		inline void matcher_hash(ComponentMatcherHash& matcherHash, ComponentId compId) noexcept {
			const auto& cc = ComponentCache::get();
			const auto componentHash = cc.comp_info(compId).matcherHash.hash;
			matcherHash.hash = core::combine_or(matcherHash.hash, componentHash);
		}

		//! Calculates a component matcher hash from the provided component ids
		//! \param compIds Span of component ids
		//! \return Component matcher hash
		GAIA_NODISCARD inline ComponentMatcherHash matcher_hash(ComponentIdSpan compIds) noexcept {
			const auto infosSize = compIds.size();
			if (infosSize == 0)
				return {0};

			const auto& cc = ComponentCache::get();
			ComponentMatcherHash::Type hash = cc.comp_info(compIds[0]).matcherHash.hash;
			for (uint32_t i = 1; i < infosSize; ++i)
				hash = core::combine_or(hash, cc.comp_info(compIds[i]).matcherHash.hash);
			return {hash};
		}

		//! Calculates a component lookup hash from the provided component ids
		//! \param compIds Span of component ids
		//! \return Component lookup hash
		GAIA_NODISCARD inline ComponentLookupHash calc_lookup_hash(ComponentIdSpan compIds) noexcept {
			const auto infosSize = compIds.size();
			if (infosSize == 0)
				return {0};

			const auto& cc = ComponentCache::get();
			ComponentLookupHash::Type hash = cc.comp_info(compIds[0]).lookupHash.hash;
			for (uint32_t i = 1; i < infosSize; ++i)
				hash = core::hash_combine(hash, cc.comp_info(compIds[i]).lookupHash.hash);
			return {hash};
		}

		using SortComponentCond = core::is_smaller<ComponentId>;
	} // namespace ecs
} // namespace gaia