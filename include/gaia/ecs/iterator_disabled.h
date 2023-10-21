#pragma once
#include "../config/config.h"

#include <cinttypes>
#include <type_traits>

#include "chunk.h"
#include "chunk_accessor.h"

namespace gaia {
	namespace ecs {
		struct IteratorDisabled: public detail::ChunkAccessor {
			using Iter = ChunkAccessorIt;

		public:
			IteratorDisabled(archetype::Chunk& chunk): detail::ChunkAccessor(chunk) {}

			GAIA_NODISCARD Iter begin() const {
				return Iter(0);
			}

			GAIA_NODISCARD Iter end() const {
				return Iter(m_chunk.GetDisabledEntityCount());
			}

			//! Calculates the number of entities accessible via the iterator
			GAIA_NODISCARD uint32_t size() const {
				return m_chunk.GetDisabledEntityCount();
			}
		};
	} // namespace ecs
} // namespace gaia