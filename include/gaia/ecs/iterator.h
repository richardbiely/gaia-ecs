#pragma once
#include "../config/config.h"

#include <cinttypes>
#include <type_traits>

#include "chunk.h"
#include "chunk_accessor.h"

namespace gaia {
	namespace ecs {
		struct Iterator: public ChunkAccessor {
		public:
			Iterator(archetype::Chunk& chunk): ChunkAccessor(chunk, 0) {}

			GAIA_NODISCARD ChunkAccessorIt begin() const {
				return ChunkAccessorIt(0);
			}

			GAIA_NODISCARD ChunkAccessorIt end() const {
				return ChunkAccessorIt(m_chunk.GetEntityCount());
			}

			GAIA_NODISCARD uint32_t size() const {
				return m_chunk.GetEntityCount();
			}
		};
	} // namespace ecs
} // namespace gaia