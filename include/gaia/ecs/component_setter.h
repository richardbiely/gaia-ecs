#pragma once
#include "gaia/config/config.h"

#include <cstdint>

#include "gaia/ecs/chunk.h"
#include "gaia/ecs/component.h"
#include "gaia/ecs/component_getter.h"

namespace gaia {
	namespace ecs {
		struct ComponentRawMutView;

		//! Entity-scoped mutable component accessor bound to a specific world, chunk and row.
		//! It is not a standalone chunk view and expects the referenced entity to remain valid.
		struct ComponentSetter: public ComponentGetter {
			using ComponentGetter::ComponentGetter;

			//! Returns a mutable reference to component without triggering hooks, observers or world-version updates.
			//! Call `World::modify<T, true>(entity)` if the write should emit `OnSet`.
			//! \tparam T Component or pair
			//! \return Reference to data for AoS, or mutable accessor for SoA types
			template <typename T>
			decltype(auto) mut() {
				GAIA_ASSERT(m_pWorld != nullptr);
				GAIA_ASSERT(m_entity != EntityBad);
				GAIA_ASSERT(m_pChunk != nullptr);
				const auto row = (uint16_t)(m_row * (actual_type_t<T>::Kind == EntityKind::EK_Gen));
				return const_cast<Chunk*>(m_pChunk)->template sset<T>(row);
			}

			//! Sets the value of the component \tparam T and then emits the normal post-write set notifications.
			//! \tparam T Component or pair
			//! \param value Value to set for the component
			//! \return ComponentSetter
			template <typename T, typename U = typename actual_type_t<T>::Type>
			ComponentSetter& set(U&& value) {
				GAIA_ASSERT(m_pWorld != nullptr);
				GAIA_ASSERT(m_entity != EntityBad);
				GAIA_ASSERT(m_pChunk != nullptr);
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
			decltype(auto) mut(Entity type);

			//! Sets the value of the component @a type and then emits the normal post-write set notifications.
			//! \tparam T Component or pair
			//! \param type Entity associated with the type
			//! \param value Value to set for the component
			//! \return ComponentSetter
			template <typename T>
			ComponentSetter& set(Entity type, T&& value);

			//! Returns a mutable reference to component without triggering a world version update.
			//! \tparam T Component or pair
			//! \return Reference to data for AoS, or mutable accessor for SoA types
			template <typename T>
			decltype(auto) smut() {
				GAIA_ASSERT(m_pWorld != nullptr);
				GAIA_ASSERT(m_entity != EntityBad);
				GAIA_ASSERT(m_pChunk != nullptr);
				const auto row = (uint16_t)(m_row * (actual_type_t<T>::Kind == EntityKind::EK_Gen));
				return const_cast<Chunk*>(m_pChunk)->template sset<T>(row);
			}

			//! Sets the value of the component without triggering a world version update.
			//! \tparam T Component or pair
			//! \param value Value to set for the component
			//! \return ComponentSetter
			template <typename T, typename U = typename actual_type_t<T>::Type>
			ComponentSetter& sset(U&& value) {
				GAIA_ASSERT(m_pWorld != nullptr);
				GAIA_ASSERT(m_entity != EntityBad);
				GAIA_ASSERT(m_pChunk != nullptr);
				smut<T>() = GAIA_FWD(value);
				return *this;
			}

			//! Returns a mutable reference to component without triggering a world version update.
			//! \tparam T Component or pair
			//! \param type Entity associated with the type
			//! \return Reference to data for AoS, or mutable accessor for SoA types
			template <typename T>
			decltype(auto) smut(Entity type);

			//! Sets the value of the component without triggering a world version update.
			//! \tparam T Component or pair
			//! \param type Entity associated with the type
			//! \param value Value to set for the component
			//! \return ComponentSetter
			template <typename T>
			ComponentSetter& sset(Entity type, T&& value);

			//! Returns a mutable raw byte view for a runtime component without finishing the write.
			//! Pair with modify_raw(component) when set hooks or `OnSet` observers should run.
			//! \param component Runtime component entity.
			//! \return Mutable raw payload view, or an invalid view when unavailable.
			GAIA_NODISCARD ComponentRawMutView mut_raw(Entity component);

			//! Returns one mutable SoA field value without finishing the component write.
			//! Pair with modify_raw(component) when set hooks or `OnSet` observers should run.
			//! \param component Runtime component entity.
			//! \param fieldIdx SoA field array index.
			//! \return Mutable raw field value view, or an invalid view when unavailable.
			GAIA_NODISCARD ComponentRawMutView mut_raw_field(Entity component, uint32_t fieldIdx);

			//! Replaces a runtime component payload and emits normal post-write set notifications.
			//! \param component Runtime component entity.
			//! \param data Payload bytes. Must be non-null when the component size is non-zero.
			//! \param size Payload byte count. Must match the registered component size.
			//! \return ComponentSetter.
			ComponentSetter& set_raw(Entity component, const void* data, uint32_t size);

			//! Marks a raw payload or SoA field returned by a mutable raw view as modified.
			//! \param component Runtime component entity.
			//! \return ComponentSetter.
			ComponentSetter& modify_raw(Entity component);
		};
	} // namespace ecs
} // namespace gaia
