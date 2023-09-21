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
			using Iter = ChunkAccessorWithMaskIt;

			static Mask FlipMask(archetype::Chunk& chunk) {
				return Mask(chunk.GetDisabledEntityMask()).flip(0, chunk.HasEntities() ? chunk.GetEntityCount() - 1 : 0);
			}

		public:
			IteratorEnabled(archetype::Chunk& chunk): ChunkAccessorWithMask(chunk, FlipMask(chunk)) {}

			GAIA_NODISCARD Iter begin() const {
				return Iter(m_mask, 0, true);
			}

			GAIA_NODISCARD Iter end() const {
				return Iter(m_mask, m_chunk.GetEntityCount(), false);
			}

			GAIA_NODISCARD uint32_t size() const {
				return m_mask.count();
			}
		};
	} // namespace ecs
} // namespace gaia