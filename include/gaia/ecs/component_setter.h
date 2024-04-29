#pragma once
#include "../config/config.h"

#include <cstdint>

#include "chunk.h"
#include "component.h"
#include "component_getter.h"

namespace gaia {
	namespace ecs {
		struct ComponentSetter: public ComponentGetter {
			//! Returns a mutable reference to component.
			//! \tparam T Component or pair
			//! \return Reference to data for AoS, or mutable accessor for SoA types
			template <typename T>
			decltype(auto) mut() {
				return const_cast<Chunk*>(m_pChunk)->template set<T>(m_row);
			}

			//! Sets the value of the component \tparam T.
			//! \tparam T Component or pair
			//! \param value Value to set for the component
			//! \return ComponentSetter
			template <typename T, typename U = typename actual_type_t<T>::Type>
			ComponentSetter& set(U&& value) {
				mut<T>() = GAIA_FWD(value);
				return *this;
			}

			//! Returns a mutable reference to component.
			//! \tparam T Component or pair
			//! \return Reference to data for AoS, or mutable accessor for SoA types
			template <typename T>
			decltype(auto) mut(Entity type) {
				return const_cast<Chunk*>(m_pChunk)->template set<T>(m_row, type);
			}

			//! Sets the value of the component \param type.
			//! \tparam T Component or pair
			//! \param type Entity associated with the type
			//! \param value Value to set for the component
			//! \return ComponentSetter
			template <typename T>
			ComponentSetter& set(Entity type, T&& value) {
				mut<T>(type) = GAIA_FWD(value);
				return *this;
			}

			//! Returns a mutable reference to component without triggering a world version update.
			//! \tparam T Component or pair
			//! \return Reference to data for AoS, or mutable accessor for SoA types
			template <typename T>
			decltype(auto) smut() {
				return const_cast<Chunk*>(m_pChunk)->template sset<T>(m_row);
			}

			//! Sets the value of the component without triggering a world version update.
			//! \tparam T Component or pair
			//! \param value Value to set for the component
			//! \return ComponentSetter
			template <typename T, typename U = typename actual_type_t<T>::Type>
			ComponentSetter& sset(U&& value) {
				smut<T>() = GAIA_FWD(value);
				return *this;
			}

			//! Returns a mutable reference to component without triggering a world version update.
			//! \tparam T Component or pair
			//! \param type Entity associated with the type
			//! \return Reference to data for AoS, or mutable accessor for SoA types
			template <typename T>
			decltype(auto) smut(Entity type) {
				return const_cast<Chunk*>(m_pChunk)->template sset<T>(type);
			}

			//! Sets the value of the component without triggering a world version update.
			//! \tparam T Component or pair
			//! \param type Entity associated with the type
			//! \param value Value to set for the component
			//! \return ComponentSetter
			template <typename T>
			ComponentSetter& sset(Entity type, T&& value) {
				smut<T>(type) = GAIA_FWD(value);
				return *this;
			}
		};
	} // namespace ecs
} // namespace gaia