#pragma once
#include "gaia/config/config.h"

#include <cstdint>

#include "gaia/ecs/chunk.h"
#include "gaia/ecs/component.h"
#include "gaia/ecs/component_getter.h"

namespace gaia {
	namespace ecs {
		struct ComponentSetter: public ComponentGetter {
			//! Returns a mutable reference to component without triggering hooks, observers or world-version updates.
			//! Call `World::modify<T, true>(entity)` if the write should emit `OnSet`.
			//! \tparam T Component or pair
			//! \return Reference to data for AoS, or mutable accessor for SoA types
			template <typename T>
			decltype(auto) mut() {
				return const_cast<Chunk*>(m_pChunk)->template sset<T>(m_row);
			}

			//! Sets the value of the component \tparam T and then emits the normal post-write set notifications.
			//! \tparam T Component or pair
			//! \param value Value to set for the component
			//! \return ComponentSetter
			template <typename T, typename U = typename actual_type_t<T>::Type>
			ComponentSetter& set(U&& value) {
				smut<T>() = GAIA_FWD(value);
				auto& chunk = *const_cast<Chunk*>(m_pChunk);
				chunk.template modify<T, true>();
				world_notify_on_set(chunk.world(), chunk.template comp_entity<T>(), chunk, m_row, (uint16_t)(m_row + 1));
				return *this;
			}

			//! Returns a mutable reference to component without triggering hooks, observers or world-version updates.
			//! Call `World::modify<T, true>(entity, type)` if the write should emit `OnSet`.
			//! \tparam T Component or pair
			//! \return Reference to data for AoS, or mutable accessor for SoA types
			template <typename T>
			decltype(auto) mut(Entity type) {
				return const_cast<Chunk*>(m_pChunk)->template sset<T>(m_row, type);
			}

			//! Sets the value of the component @a type and then emits the normal post-write set notifications.
			//! \tparam T Component or pair
			//! \param type Entity associated with the type
			//! \param value Value to set for the component
			//! \return ComponentSetter
			template <typename T>
			ComponentSetter& set(Entity type, T&& value) {
				smut<T>(type) = GAIA_FWD(value);
				auto& chunk = *const_cast<Chunk*>(m_pChunk);
				const auto compIdx = chunk.comp_idx(type);
				(void)chunk.template comp_ptr_mut_gen<true>(compIdx, m_row);
				world_notify_on_set(chunk.world(), type, chunk, m_row, (uint16_t)(m_row + 1));
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
				return const_cast<Chunk*>(m_pChunk)->template sset<T>(m_row, type);
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
