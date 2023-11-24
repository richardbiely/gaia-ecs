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
				using CT = component_type_t<T>;
				using FT = typename CT::TypeFull;
				constexpr auto kind = CT::Kind;

				verify_comp<T>();

				if constexpr (kind == EntityKind::EK_Gen)
					return m_pChunk->template set<FT>(m_idx);
				else
					return m_pChunk->template set<FT>();
			}

			//! Sets the value of the component \tparam T on \param entity.
			//! \tparam T Component
			//! \param value Value to set for the component
			//! \return ComponentSetter
			template <typename T, typename U = typename component_type_t<T>::Type>
			ComponentSetter& set(U&& data) {
				using CT = component_type_t<T>;
				using FT = typename CT::TypeFull;
				constexpr auto kind = CT::Kind;

				verify_comp<T>();

				if constexpr (kind == EntityKind::EK_Gen)
					m_pChunk->template set<FT>(m_idx, GAIA_FWD(data));
				else
					m_pChunk->template set<FT>(GAIA_FWD(data));
				return *this;
			}

			//! Sets the value of the component \tparam T on \param entity without trigger a world version update.
			//! \tparam T Component
			//! \param value Value to set for the component
			//! \return ComponentSetter
			template <typename T, typename U = typename component_type_t<T>::Type>
			ComponentSetter& sset(U&& data) {
				using CT = component_type_t<T>;
				using FT = typename CT::TypeFull;
				constexpr auto kind = CT::Kind;

				verify_comp<T>();

				if constexpr (kind == EntityKind::EK_Gen)
					m_pChunk->template sset<FT>(m_idx, GAIA_FWD(data));
				else
					m_pChunk->template sset<FT>(GAIA_FWD(data));
				return *this;
			}
		};
	} // namespace ecs
} // namespace gaia