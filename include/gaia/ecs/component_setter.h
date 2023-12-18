#pragma once
#include "../config/config.h"

#include <cstdint>

#include "chunk.h"
#include "component.h"

namespace gaia {
	namespace ecs {
		struct ComponentSetter {
			Chunk* m_pChunk;
			uint16_t m_row;

			//! Sets the value of the component \tparam T.
			//! \tparam T Component or pair
			//! \param value Value to set for the component
			//! \return ComponentSetter
			template <typename T, typename U = typename actual_type_t<T>::Type>
			ComponentSetter& set(U&& value) {
				m_pChunk->template set<T>(m_row, GAIA_FWD(value));
				return *this;
			}

			//! Sets the value of the component \param type.
			//! \tparam T Component or pair
			//! \param type Entity associated with the type
			//! \param value Value to set for the component
			//! \return ComponentSetter
			template <typename T>
			ComponentSetter& set(Entity type, T&& value) {
				m_pChunk->template set<T>(m_row, type, GAIA_FWD(value));
				return *this;
			}

			//! Sets the value of the component \tparam T without trigger a world version update.
			//! \tparam T Component or pair
			//! \param value Value to set for the component
			//! \return ComponentSetter
			template <typename T, typename U = typename actual_type_t<T>::Type>
			ComponentSetter& sset(U&& value) {
				m_pChunk->template sset<T>(m_row, GAIA_FWD(value));
				return *this;
			}

			//! Sets the value of the component \param type without trigger a world version update.
			//! \tparam T Component or pair
			//! \param type Entity associated with the type
			//! \param value Value to set for the component
			//! \return ComponentSetter
			template <typename T>
			ComponentSetter& sset(Entity object, T&& value) {
				m_pChunk->template sset<T>(m_row, object, GAIA_FWD(value));
				return *this;
			}
		};
	} // namespace ecs
} // namespace gaia