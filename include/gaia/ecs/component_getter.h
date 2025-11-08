#pragma once
#include "gaia/config/config.h"

#include <cstdint>

#include "chunk.h"
#include "component.h"

namespace gaia {
	namespace ecs {
		struct ComponentGetter {
			const Chunk* m_pChunk;
			uint16_t m_row;

			//! Returns the value stored in the component \tparam T on \param entity.
			//! \tparam T Component
			//! \return Value stored in the component.
			template <typename T>
			GAIA_NODISCARD decltype(auto) get() const {
				verify_comp<T>();

				if constexpr (entity_kind_v<T> == EntityKind::EK_Gen)
					return m_pChunk->template get<T>(m_row);
				else
					return m_pChunk->template get<T>();
			}
		};
	} // namespace ecs
} // namespace gaia