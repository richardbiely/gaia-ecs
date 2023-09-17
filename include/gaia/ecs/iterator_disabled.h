#pragma once
#include "../config/config.h"

#include <cinttypes>
#include <type_traits>

#include "chunk_header.h"
#include "component.h"
#include "component_setter.h"
#include "query_info.h"

namespace gaia {
	namespace ecs {
		struct IteratorDisabled {
		private:
			using Mask = archetype::ChunkHeader::DisabledEntityMask;
			using Iter = Mask::const_iterator;

			query::QueryInfo& m_info;
			archetype::Chunk& m_chunk;
			Mask m_mask;
			Iter m_iter;

			IteratorDisabled(query::QueryInfo& info, archetype::Chunk& chunk, uint32_t pos):
					m_info(info), m_chunk(chunk), m_mask(chunk.GetDisabledEntityMask()), m_iter(m_mask, pos, false) {}

		public:
			IteratorDisabled(query::QueryInfo& info, archetype::Chunk& chunk):
					m_info(info), m_chunk(chunk), m_mask(chunk.GetDisabledEntityMask()), m_iter(m_mask, 0, true) {}

			uint32_t operator*() const {
				return *m_iter;
			}

			uint32_t operator->() const {
				return *m_iter;
			}

			IteratorDisabled& operator++() {
				++m_iter;
				return *this;
			}

			GAIA_NODISCARD IteratorDisabled operator++(int) {
				IteratorDisabled temp(*this);
				++*this;
				return temp;
			}

			GAIA_NODISCARD bool operator==(const IteratorDisabled& other) const {
				return m_iter == other.m_iter;
			}

			GAIA_NODISCARD bool operator!=(const IteratorDisabled& other) const {
				return m_iter != other.m_iter;
			}

			GAIA_NODISCARD IteratorDisabled begin() const {
				return *this;
			}

			GAIA_NODISCARD IteratorDisabled end() const {
				return {m_info, m_chunk, m_chunk.GetEntityCount()};
			}

			GAIA_NODISCARD uint32_t size() const {
				return m_mask.count();
			}

			//! Checks if component \tparam T is present in the chunk.
			//! \tparam T Component
			//! \return True if the component is present. False otherwise.
			template <typename T>
			GAIA_NODISCARD bool HasComponent() const {
				return m_chunk.HasComponent<T>();
			}

			//! Sets the value of the component \tparam T on \param entity.
			//! \tparam T Component
			//! \param value Value to set for the component
			//! \return ComponentSetter
			template <typename T, typename U = typename component::DeduceComponent<T>::Type>
			ComponentSetter& SetComponent(U&& data) {
				ComponentSetter setter{&m_chunk, *m_iter};
				return setter.SetComponent<T, U>(std::forward<U>(data));
			}

			//! Sets the value of the component \tparam T on \param entity without trigger a world version update.
			//! \tparam T Component
			//! \param value Value to set for the component
			//! \return ComponentSetter
			template <typename T, typename U = typename component::DeduceComponent<T>::Type>
			ComponentSetter& SetComponentSilent(U&& data) {
				ComponentSetter setter{&m_chunk, *m_iter};
				return setter.SetComponentSilent<T, U>(std::forward<U>(data));
			}
		};
	} // namespace ecs
} // namespace gaia