#pragma once
#include <cstdint>

#include "chunk.h"
#include "component.h"

namespace gaia {
	namespace ecs {
		struct ComponentSetter {
			Chunk* m_pChunk;
			uint32_t m_idx;

			//! Sets the value of the component \tparam T on \param entity.
			//! \tparam T Component
			//! \param value Value to set for the component
			//! \return ComponentSetter
			template <typename T, typename U = typename component_type_t<T>::Type>
			U& set() {
				verify_comp<T>();

				if constexpr (component_kind_v<T> == ComponentKind::CK_Gen)
					return m_pChunk->template set<T>(m_idx);
				else
					return m_pChunk->template set<T>();
			}

			//! Sets the value of the component \tparam T on \param entity.
			//! \tparam T Component
			//! \param value Value to set for the component
			//! \return ComponentSetter
			template <typename T, typename U = typename component_type_t<T>::Type>
			ComponentSetter& set(U&& data) {
				verify_comp<T>();

				if constexpr (component_kind_v<T> == ComponentKind::CK_Gen)
					m_pChunk->template set<T>(m_idx, GAIA_FWD(data));
				else
					m_pChunk->template set<T>(GAIA_FWD(data));
				return *this;
			}

			//! Sets the value of the component \tparam T on \param entity without trigger a world version update.
			//! \tparam T Component
			//! \param value Value to set for the component
			//! \return ComponentSetter
			template <typename T, typename U = typename component_type_t<T>::Type>
			ComponentSetter& sset(U&& data) {
				verify_comp<T>();

				if constexpr (component_kind_v<T> == ComponentKind::CK_Gen)
					m_pChunk->template sset<T>(m_idx, GAIA_FWD(data));
				else
					m_pChunk->template sset<T>(GAIA_FWD(data));
				return *this;
			}
		};
	} // namespace ecs
} // namespace gaia