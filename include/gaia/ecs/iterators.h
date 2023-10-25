#pragma once
#include "../config/config.h"

#include <cinttypes>
#include <type_traits>

#include "chunk.h"
#include "chunk_accessor.h"

namespace gaia {
	namespace ecs {
		namespace detail {
			template <typename Func>
			void each(const uint32_t idxFrom, const uint32_t idxStop, Func func) noexcept {
				if constexpr (std::is_invocable_v<Func, uint32_t>) {
					for (auto i = idxFrom; i < idxStop; ++i)
						func(i);
				} else {
					for (auto i = idxFrom; i < idxStop; ++i)
						func();
				}
			}
		} // namespace detail

		struct Iterator: public detail::ChunkAccessor {
			using Iter = ChunkAccessorIt;

			Iterator(archetype::Chunk& chunk) noexcept: detail::ChunkAccessor(chunk) {}

			//! Returns the number of entities accessible via the iterator
			GAIA_NODISCARD uint32_t size() const noexcept {
				return m_chunk.size_enabled();
			}

			template <typename Func>
			void each(Func func) noexcept {
				const auto idxFrom = m_chunk.size_disabled();
				const auto idxStop = m_chunk.size();
				detail::each(idxFrom, idxStop, func);
			}
		};

		struct IteratorDisabled: public detail::ChunkAccessor {
			using Iter = ChunkAccessorIt;

			IteratorDisabled(archetype::Chunk& chunk) noexcept: detail::ChunkAccessor(chunk) {}

			//! Returns the number of entities accessible via the iterator
			GAIA_NODISCARD uint32_t size() const noexcept {
				return m_chunk.size_disabled();
			}

			template <typename Func>
			void each(Func func) noexcept {
				const auto idxFrom = 0;
				const auto idxStop = m_chunk.size_disabled();
				detail::each(idxFrom, idxStop, func);
			}
		};

		struct IteratorAll: public detail::ChunkAccessor {
			using Iter = ChunkAccessorIt;

			IteratorAll(archetype::Chunk& chunk) noexcept: detail::ChunkAccessor(chunk) {}

			//! Returns the number of entities accessible via the iterator
			GAIA_NODISCARD uint32_t size() const noexcept {
				return m_chunk.size();
			}

			template <typename Func>
			void each(Func func) noexcept {
				const auto idxFrom = 0;
				const auto idxStop = m_chunk.size();
				detail::each(idxFrom, idxStop, func);
			}
		};
	} // namespace ecs
} // namespace gaia