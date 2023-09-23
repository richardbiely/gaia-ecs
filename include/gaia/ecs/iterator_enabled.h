#pragma once
#include "../config/config.h"

#include <cinttypes>
#include <type_traits>

#include "../containers/bitset.h"
#include "chunk.h"
#include "chunk_accessor.h"

namespace gaia {
	namespace ecs {
		struct IteratorEnabled: public ChunkAccessorWithMask {
		private:
			using Mask = ChunkAccessorWithMask::Mask;
			using Iter = ChunkAccessorWithMaskItInverse;
			uint32_t m_size;

		public:
			IteratorEnabled(archetype::Chunk& chunk): ChunkAccessorWithMask(chunk, chunk.GetDisabledEntityMask()) {
				const auto bits = (Mask::BitCount - m_mask.count());
				m_size = bits > m_chunk.GetEntityCount() ? m_chunk.GetEntityCount() : bits;
			}

			GAIA_NODISCARD Iter begin() const {
				return Iter(m_mask, 0, true);
			}

			GAIA_NODISCARD Iter end() const {
				return Iter(m_mask, m_chunk.GetEntityCount(), false);
			}

			GAIA_NODISCARD uint32_t size() const {
				return m_size;
			}
		};
	} // namespace ecs
} // namespace gaia