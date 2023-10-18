#pragma once
#include <cstdint>

#include "chunk.h"
#include "component.h"

namespace gaia {
	namespace ecs {
		struct ComponentGetter {
			const archetype::Chunk* m_pChunk;
			uint32_t m_idx;

			//! Returns the value stored in the component \tparam T on \param entity.
			//! \tparam T Component
			//! \return Value stored in the component.
			template <typename T>
			GAIA_NODISCARD auto Get() const {
				component::VerifyComponent<T>();

				if constexpr (component::component_type_v<T> == component::ComponentType::CT_Generic)
					return m_pChunk->template Get<T>(m_idx);
				else
					return m_pChunk->template Get<T>();
			}

			//! Tells if \param entity contains the component \tparam T.
			//! \tparam T Component
			//! \return True if the component is present on entity.
			template <typename T>
			GAIA_NODISCARD bool Has() const {
				component::VerifyComponent<T>();

				return m_pChunk->template Has<T>();
			}
		};
	} // namespace ecs
} // namespace gaia