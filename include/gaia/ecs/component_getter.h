#pragma once
#include "gaia/config/config.h"

#include <cstdint>

#include "gaia/ecs/chunk.h"
#include "gaia/ecs/component.h"

namespace gaia {
	namespace ecs {
		class World;

		struct ComponentGetter {
			const World* m_pWorld = nullptr;
			Entity m_entity = EntityBad;
			const Chunk* m_pChunk = nullptr;
			uint16_t m_row = 0;

			ComponentGetter() = default;
			ComponentGetter(const World& world, Entity entity, const Chunk* pChunk, uint16_t row):
					m_pWorld(&world), m_entity(entity), m_pChunk(pChunk), m_row(row) {}

			//! Returns the value stored in the component @a T on entity.
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

			//! Returns the value stored in the component associated with @a type on entity.
			//! \tparam T Component
			//! \param type Entity associated with the component type
			template <typename T>
			GAIA_NODISCARD decltype(auto) get(Entity type) const;
		};
	} // namespace ecs
} // namespace gaia
