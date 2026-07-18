#pragma once
#include "gaia/config/config.h"

#include <cstdint>

#include "gaia/ecs/chunk.h"
#include "gaia/ecs/component.h"

namespace gaia {
	namespace ecs {
		class World;
		struct ComponentRawView;

		//! Entity-scoped component accessor bound to a specific world, chunk and row.
		//! It is not a standalone chunk view and expects the referenced entity to remain valid.
		struct ComponentGetter {
			//! World used to resolve inherited and sparse component data.
			const World* m_pWorld;
			//! Chunk containing the entity's table-backed data.
			const Chunk* m_pChunk;
			//! Entity whose components are accessed.
			Entity m_entity;
			//! Entity row within the chunk.
			uint16_t m_row;

			//! Creates an accessor for one entity row.
			//! \param world World that owns the entity.
			//! \param pChunk Chunk containing the entity row.
			//! \param entity Entity being accessed.
			//! \param row Entity row within \p pChunk.
			ComponentGetter(const World& world, const Chunk* pChunk, Entity entity, uint16_t row):
					m_pWorld(&world), m_pChunk(pChunk), m_entity(entity), m_row(row) {}

			//! Returns the value stored in the component \a T on entity.
			//! \tparam T Component
			//! \return Value stored in the component.
			template <typename T>
			GAIA_NODISCARD decltype(auto) get() const {
				GAIA_ASSERT(m_pWorld != nullptr);
				GAIA_ASSERT(m_entity != EntityBad);
				GAIA_ASSERT(m_pChunk != nullptr);
				verify_comp<T>();

				if constexpr (actual_type_t<T>::Kind == EntityKind::EK_Gen)
					return m_pChunk->template get<T>(m_row);
				else
					return m_pChunk->template get<T>();
			}

			//! Returns the value stored in the component associated with \a type on entity.
			//! \tparam T Component
			//! \param type Entity associated with the component type
			//! \return Value stored in the selected component.
			template <typename T>
			GAIA_NODISCARD decltype(auto) get(Entity type) const;

			//! Returns a raw byte view for a runtime component on this entity.
			//! \param component Runtime component entity.
			//! \return Raw payload view, or an invalid view when unavailable.
			GAIA_NODISCARD ComponentRawView get_raw(Entity component) const;

			//! Returns one read-only SoA field value for a runtime component on this entity.
			//! \param component Runtime component entity.
			//! \param fieldIdx SoA field array index.
			//! \return Raw field value view, or an invalid view when unavailable.
			GAIA_NODISCARD ComponentRawView get_raw_field(Entity component, uint32_t fieldIdx) const;
		};
	} // namespace ecs
} // namespace gaia
