#pragma once
#include "../config/config.h"

#include <cinttypes>
#include <type_traits>

#include "chunk.h"
#include "chunk_accessor.h"

namespace gaia {
	namespace ecs {
		struct IteratorEnabled: public detail::ChunkAccessor {
			using Iter = ChunkAccessorIt;

		public:
			IteratorEnabled(archetype::Chunk& chunk): detail::ChunkAccessor(chunk) {}

			GAIA_NODISCARD Iter begin() const {
				return Iter(m_chunk.GetDisabledEntityCount());
			}

			GAIA_NODISCARD Iter end() const {
				return Iter(m_chunk.GetEntityCount());
			}

			//! Calculates the number of entities accessible via the iterator
			GAIA_NODISCARD uint32_t size() const {
				return m_chunk.GetEnabledEntityCount();
			}
		};
	} // namespace ecs
} // namespace gaia