#pragma once
#include <cstdint>

#include "chunk.h"
#include "comp/component.h"

namespace gaia {
	namespace ecs {
		struct ComponentGetter {
			const archetype::Chunk* m_pChunk;
			uint32_t m_idx;

			//! Returns the value stored in the component \tparam T on \param entity.
			//! \tparam T Component
			//! \return Value stored in the component.
			template <typename T>
			GAIA_NODISCARD auto get() const {
				comp::verify_comp<T>();

				if constexpr (comp::component_type_v<T> == comp::ComponentType::CT_Generic)
					return m_pChunk->template get<T>(m_idx);
				else
					return m_pChunk->template get<T>();
			}

			//! Tells if \param entity contains the component \tparam T.
			//! \tparam T Component
			//! \return True if the component is present on entity.
			template <typename T>
			GAIA_NODISCARD bool has() const {
				comp::verify_comp<T>();

				return m_pChunk->template has<T>();
			}
		};
	} // namespace ecs
} // namespace gaia