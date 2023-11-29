#pragma once
#include "../config/config.h"

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
			ComponentSetter& set(U&& value) {
				using CT = component_type_t<T>;
				using FT = typename CT::TypeFull;

				verify_comp<T>();

				m_pChunk->template set<FT>(m_idx, GAIA_FWD(value));
				return *this;
			}

			template <typename T>
			ComponentSetter& set(Entity object, T&& value) {
				static_assert(core::is_raw_v<T>);

				m_pChunk->template set<T>(m_idx, object, GAIA_FWD(value));
				return *this;
			}

			//! Sets the value of the component \tparam T on \param entity without trigger a world version update.
			//! \tparam T Component
			//! \param value Value to set for the component
			//! \return ComponentSetter
			template <typename T, typename U = typename component_type_t<T>::Type>
			ComponentSetter& sset(U&& value) {
				using CT = component_type_t<T>;
				using FT = typename CT::TypeFull;

				verify_comp<T>();

				m_pChunk->template sset<FT>(m_idx, GAIA_FWD(value));
				return *this;
			}

			template <typename T>
			ComponentSetter& sset(Entity object, T&& value) {
				static_assert(core::is_raw_v<T>);

				m_pChunk->template sset<T>(m_idx, object, GAIA_FWD(value));
				return *this;
			}
		};
	} // namespace ecs
} // namespace gaia