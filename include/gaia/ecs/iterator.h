#pragma once
#include "../config/config.h"

#include <cinttypes>
#include <type_traits>

#include "chunk.h"
#include "chunk_accessor.h"

namespace gaia {
	namespace ecs {
		struct IteratorByIndex: public ChunkAccessorExt {
		public:
			IteratorByIndex(archetype::Chunk& chunk): ChunkAccessorExt(chunk, 0) {}
			IteratorByIndex(archetype::Chunk& chunk, uint32_t pos): ChunkAccessorExt(chunk, pos) {}

			GAIA_NODISCARD ChunkAccessorExtByIndexIt begin() const {
				return ChunkAccessorExtByIndexIt::CreateBegin(m_chunk);
			}

			GAIA_NODISCARD ChunkAccessorExtByIndexIt end() const {
				return ChunkAccessorExtByIndexIt::CreateEnd(m_chunk);
			}

			GAIA_NODISCARD uint32_t size() const {
				return m_chunk.GetEntityCount();
			}
		};

		struct Iterator: public ChunkAccessorExt {
		public:
			Iterator(archetype::Chunk& chunk): ChunkAccessorExt(chunk, 0) {}
			Iterator(archetype::Chunk& chunk, uint32_t pos): ChunkAccessorExt(chunk, pos) {}

			GAIA_NODISCARD IteratorByIndex Indices() const {
				return IteratorByIndex(m_chunk);
			}

			GAIA_NODISCARD ChunkAccessorExtIt begin() const {
				return ChunkAccessorExtIt::CreateBegin(m_chunk);
			}

			GAIA_NODISCARD ChunkAccessorExtIt end() const {
				return ChunkAccessorExtIt::CreateEnd(m_chunk);
			}

			GAIA_NODISCARD uint32_t size() const {
				return m_chunk.GetEntityCount();
			}
		};
	} // namespace ecs
} // namespace gaia