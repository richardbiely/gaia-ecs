#pragma once
#include "../config/config.h"

#include <cinttypes>
#include <type_traits>

#include "../containers/bitset.h"
#include "chunk.h"
#include "chunk_accessor.h"
#include "chunk_header.h"

namespace gaia {
	namespace ecs {
		struct IteratorEnabled {
		private:
			using Mask = ChunkAccessorIt::Mask;
			using Iter = ChunkAccessorIt::Iter;

			archetype::Chunk& m_chunk;
			Mask m_mask;

			static Mask FlipMask(archetype::Chunk& chunk) {
				return Mask(chunk.GetDisabledEntityMask()).flip(0, chunk.HasEntities() ? chunk.GetEntityCount() - 1 : 0);
			}

		public:
			IteratorEnabled(archetype::Chunk& chunk): m_chunk(chunk), m_mask(FlipMask(chunk)) {}

			GAIA_NODISCARD ChunkAccessorIt begin() const {
				return ChunkAccessorIt::CreateBegin(m_chunk, m_mask);
			}

			GAIA_NODISCARD ChunkAccessorIt end() const {
				return ChunkAccessorIt::CreateEnd(m_chunk, m_mask);
			}

			GAIA_NODISCARD uint32_t size() const {
				return m_mask.count();
			}
		};
	} // namespace ecs
} // namespace gaia