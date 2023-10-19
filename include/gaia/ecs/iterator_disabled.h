#pragma once
#include "../config/config.h"

#include <cinttypes>
#include <type_traits>

#include "../cnt/bitset.h"
#include "../cnt/sarray_ext.h"
#include "chunk.h"
#include "chunk_accessor.h"

namespace gaia {
	namespace ecs {
		struct IteratorDisabled: public detail::ChunkAccessor {
		private:
			using Mask = detail::ChunkAccessorMask;

			cnt::sarr_ext<uint16_t, Mask::BitCount> m_indices;

		public:
			IteratorDisabled(archetype::Chunk& chunk): detail::ChunkAccessor(chunk) {
				// Build indices we use for iteration
				const auto& mask = chunk.GetDisabledEntityMask();
				const auto it0 = detail::ChunkAccessorIter(mask, 0, true);
				const auto it1 = detail::ChunkAccessorIter(mask, m_chunk.GetEntityCount(), false);
				for (auto it = it0; it != it1; ++it)
					m_indices.push_back((uint16_t)*it);
			}

			GAIA_NODISCARD auto begin() const {
				return m_indices.begin();
			}

			GAIA_NODISCARD auto end() const {
				return m_indices.end();
			}

			//! Returns the number of entities accessible via the iterator
			GAIA_NODISCARD uint32_t size() const {
				return (uint32_t)m_indices.size();
			}
		};
	} // namespace ecs
} // namespace gaia