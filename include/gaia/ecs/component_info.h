#pragma once
#include <type_traits>

#include "../meta/type_info.h"
#include "component.h"

namespace gaia {
	namespace ecs {
		struct ComponentInfo final {
			//! Complex hash used for look-ups
			ComponentLookupHash lookupHash;
			//! Simple hash used for matching component
			ComponentMatcherHash matcherHash;
			//! Unique component identifier
			ComponentId compId;

			GAIA_NODISCARD bool operator==(const ComponentInfo& other) const {
				GAIA_ASSERT(lookupHash == other.lookupHash);
				return compId == other.compId;
			}
			GAIA_NODISCARD bool operator!=(const ComponentInfo& other) const {
				GAIA_ASSERT(lookupHash != other.lookupHash);
				return compId != other.compId;
			}
			GAIA_NODISCARD bool operator<(const ComponentInfo& other) const {
				return compId < other.compId;
			}

			template <typename T>
			GAIA_NODISCARD static constexpr ComponentInfo init() {
				using U = typename component_type_t<T>::Type;

				ComponentInfo info{};
				info.lookupHash = {meta::type_info::hash<U>()};
				info.matcherHash = calc_matcher_hash<U>();
				info.compId = comp_id<T>();

				return info;
			}

			template <typename T>
			static const ComponentInfo* create() {
				using U = std::decay_t<T>;
				return new ComponentInfo{init<U>()};
			}
		};
	} // namespace ecs
} // namespace gaia