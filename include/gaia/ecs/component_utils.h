#pragma once

#include "component.h"
#include "component_cache.h"

namespace gaia {
	namespace ecs {
		GAIA_NODISCARD inline ComponentMatcherHash CalculateMatcherHash(ComponentInfoSpan infos) noexcept {
			const auto infosSize = infos.size();
			if (infosSize == 0)
				return {0};

			const auto& cc = GetComponentCache();
			ComponentMatcherHash::Type hash = cc.GetComponentInfoFromIdx(infos[0])->matcherHash.hash;
			for (size_t i = 1; i < infosSize; ++i)
				hash = utils::combine_or(hash, cc.GetComponentInfoFromIdx(infos[i])->matcherHash.hash);
			return {hash};
		}

		GAIA_NODISCARD inline ComponentLookupHash CalculateLookupHash(ComponentInfoSpan infos) noexcept {
			const auto infosSize = infos.size();
			if (infosSize == 0)
				return {0};

			const auto& cc = GetComponentCache();
			ComponentLookupHash::Type hash = cc.GetComponentInfoFromIdx(infos[0])->lookupHash.hash;
			for (size_t i = 1; i < infosSize; ++i)
				hash = utils::hash_combine(hash, cc.GetComponentInfoFromIdx(infos[i])->lookupHash.hash);
			return {hash};
		}
	} // namespace ecs
} // namespace gaia