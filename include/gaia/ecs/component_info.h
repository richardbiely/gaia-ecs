#pragma once
#include <type_traits>

#include "../meta/type_info.h"
#include "component.h"

namespace gaia {
	namespace ecs {
		namespace component {
			struct ComponentInfo final {
				//! Complex hash used for look-ups
				ComponentLookupHash lookupHash;
				//! Simple hash used for matching component
				ComponentMatcherHash matcherHash;
				//! Unique component identifier
				ComponentId componentId;

				GAIA_NODISCARD bool operator==(const ComponentInfo& other) const {
					GAIA_ASSERT(lookupHash == other.lookupHash);
					return componentId == other.componentId;
				}
				GAIA_NODISCARD bool operator!=(const ComponentInfo& other) const {
					GAIA_ASSERT(lookupHash != other.lookupHash);
					return componentId != other.componentId;
				}
				GAIA_NODISCARD bool operator<(const ComponentInfo& other) const {
					return componentId < other.componentId;
				}

				template <typename T>
				GAIA_NODISCARD static constexpr ComponentInfo Calculate() {
					using U = typename component_type_t<T>::Type;

					ComponentInfo info{};
					info.lookupHash = {meta::type_info::hash<U>()};
					info.matcherHash = CalculateMatcherHash<U>();
					info.componentId = GetComponentId<T>();

					return info;
				}

				template <typename T>
				static const ComponentInfo* Create() {
					using U = std::decay_t<T>;
					return new ComponentInfo{Calculate<U>()};
				}
			};
		} // namespace component
	} // namespace ecs
} // namespace gaia