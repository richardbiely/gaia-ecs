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
		struct IteratorEnabled {
		private:
			using Mask = archetype::ChunkHeader::DisabledEntityMask;
			using Iter = Mask::const_iterator;

			query::QueryInfo& m_info;
			archetype::Chunk& m_chunk;
			Mask m_mask;
			Iter m_iter;

			IteratorEnabled(query::QueryInfo& info, archetype::Chunk& chunk, uint32_t pos):
					m_info(info), m_chunk(chunk), m_mask(chunk.GetDisabledEntityMask()),
					m_iter(m_mask.flip(0, chunk.HasEntities() ? chunk.GetEntityCount() - 1 : 0), pos, false) {}

		public:
			IteratorEnabled(query::QueryInfo& info, archetype::Chunk& chunk):
					m_info(info), m_chunk(chunk), m_mask(chunk.GetDisabledEntityMask()),
					m_iter(m_mask.flip(0, chunk.HasEntities() ? chunk.GetEntityCount() - 1 : 0), 0, true) {}

			uint32_t operator*() const {
				return *m_iter;
			}

			uint32_t operator->() const {
				return *m_iter;
			}

			IteratorEnabled& operator++() {
				++m_iter;
				return *this;
			}

			GAIA_NODISCARD IteratorEnabled operator++(int) {
				IteratorEnabled temp(*this);
				++*this;
				return temp;
			}

			GAIA_NODISCARD bool operator==(const IteratorEnabled& other) const {
				return m_iter == other.m_iter;
			}

			GAIA_NODISCARD bool operator!=(const IteratorEnabled& other) const {
				return m_iter != other.m_iter;
			}

			GAIA_NODISCARD IteratorEnabled begin() const {
				return *this;
			}

			GAIA_NODISCARD IteratorEnabled end() const {
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

			template <typename T, typename U = typename component::DeduceComponent<T>::Type>
			ComponentSetter& SetComponent(U&& data) {
				ComponentSetter setter{&m_chunk, *m_iter};
				return setter.SetComponent<T, U>(std::forward<U>(data));
			}

			template <typename T, typename U = typename component::DeduceComponent<T>::Type>
			ComponentSetter& SetComponentSilent(U&& data) {
				ComponentSetter setter{&m_chunk, *m_iter};
				return setter.SetComponentSilent<T, U>(std::forward<U>(data));
			}
		};
	} // namespace ecs
} // namespace gaia