#pragma once

#include "../utils/utility.h"
#include "component.h"
#include "component_cache.h"

namespace gaia {
	namespace ecs {
		//! Calculates a component matcher hash from the provided component ids
		//! \param componentIds Span of component ids
		//! \return Component matcher hash
		GAIA_NODISCARD inline ComponentMatcherHash CalculateMatcherHash(ComponentIdSpan componentIds) noexcept {
			const auto infosSize = componentIds.size();
			if (infosSize == 0)
				return {0};

			const auto& cc = GetComponentCache();
			ComponentMatcherHash::Type hash = cc.GetComponentInfo(componentIds[0]).matcherHash.hash;
			for (size_t i = 1; i < infosSize; ++i)
				hash = utils::combine_or(hash, cc.GetComponentInfo(componentIds[i]).matcherHash.hash);
			return {hash};
		}

		//! Calculates a component lookup hash from the provided component ids
		//! \param componentIds Span of component ids
		//! \return Component lookup hash
		GAIA_NODISCARD inline ComponentLookupHash CalculateLookupHash(ComponentIdSpan componentIds) noexcept {
			const auto infosSize = componentIds.size();
			if (infosSize == 0)
				return {0};

			const auto& cc = GetComponentCache();
			ComponentLookupHash::Type hash = cc.GetComponentInfo(componentIds[0]).lookupHash.hash;
			for (size_t i = 1; i < infosSize; ++i)
				hash = utils::hash_combine(hash, cc.GetComponentInfo(componentIds[i]).lookupHash.hash);
			return {hash};
		}

		using SortComponentCond = utils::is_smaller<ComponentId>;

		//! Sorts component ids
		template <typename Container>
		inline void SortComponents(Container& c) {
			utils::sort(c, SortComponentCond{});
		}
	} // namespace ecs
} // namespace gaia