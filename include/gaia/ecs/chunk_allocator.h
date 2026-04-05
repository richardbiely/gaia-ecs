#pragma once
#include "gaia/config/config.h"

#include <cinttypes>
#include <cstdint>
#include <cstring>

#include "gaia/cnt/fwd_llist.h"
#include "gaia/cnt/sarray.h"
#include "gaia/core/bit_utils.h"
#include "gaia/core/dyn_singleton.h"
#include "gaia/core/utility.h"
#include "gaia/mem/mem_alloc.h"
#include "gaia/util/logging.h"

namespace gaia {
	namespace ecs {
		namespace detail {
			struct MemoryBlockHeader final {
				uintptr_t m_pageAddr = 0;
				uint32_t m_reserved = 0;
#if GAIA_DEBUG
				uint32_t m_requestedBytes = 0;
#endif
			};

			class ChunkAllocatorImpl;
		} // namespace detail

		static constexpr uint32_t MemoryBlockAlignment = 64;
		static constexpr uint32_t MinMemoryBlockSize = 1024 * 8;
		//! Size of one allocated block of memory in kiB
		static constexpr uint32_t MaxMemoryBlockSize = MinMemoryBlockSize * 4;
		//! Reserved bytes at the start of each block for allocator metadata and chunk header alignment headroom.
		//! Validated against the actual chunk layout in Chunk::chunk_header_size().
		static constexpr uint32_t MemoryBlockUsableOffset = 40;

		constexpr uint16_t mem_block_size(uint32_t sizeType) {
			constexpr uint16_t sizes[] = {MinMemoryBlockSize, MinMemoryBlockSize * 2, MaxMemoryBlockSize};
			return sizes[sizeType];
		}

		constexpr uint8_t mem_block_size_type(uint32_t sizeBytes) {
			GAIA_ASSERT(sizeBytes > 0);
			// Ceil division by smallest block size
			const uint32_t blocks = (sizeBytes + MinMemoryBlockSize - 1) / MinMemoryBlockSize;
			return blocks > 2 ? 2 : static_cast<uint8_t>(blocks - 1);
		}

#if GAIA_ECS_CHUNK_ALLOCATOR
		struct GAIA_API ChunkAllocatorPageStats final {
			//! Total allocated memory
			uint64_t mem_total;
			//! Memory actively reserved by live blocks
			uint64_t mem_used;
			//! Number of allocated pages
			uint32_t num_pages;
			//! Number of reusable pages (partial + empty)
			uint32_t num_pages_free;
	#if GAIA_DEBUG
			//! Bytes explicitly requested by callers
			uint64_t mem_requested;
			//! Number of completely empty pages
			uint32_t num_pages_empty;
	#endif
		};

		struct GAIA_API ChunkAllocatorStats final {
			ChunkAllocatorPageStats stats[3];
		};

		using ChunkAllocator = core::dyn_singleton<detail::ChunkAllocatorImpl>;

		namespace detail {
			static_assert(sizeof(MemoryBlockHeader) <= MemoryBlockUsableOffset);

			struct MemoryPageHeader {
				//! Pointer to data managed by page
				void* m_data;

				MemoryPageHeader(void* ptr): m_data(ptr) {}
			};

			struct MemoryPage: MemoryPageHeader, cnt::fwd_llist_base<MemoryPage> {
				static constexpr uint16_t NBlocks = 48;
				static constexpr uint16_t NBlocks_Bits = (uint16_t)core::count_bits(NBlocks);
				static constexpr uint32_t InvalidBlockId = NBlocks + 1;
	#if GAIA_DEBUG
				static constexpr uint8_t FreedBlockPattern = 0xDD;
	#endif
				static constexpr uint32_t BlockArrayBytes = ((uint32_t)NBlocks_Bits * (uint32_t)NBlocks + 7) / 8;
				using BlockArray = cnt::sarray<uint8_t, BlockArrayBytes>;
				using BitView = core::bit_view<NBlocks_Bits>;

				//! Implicit list of blocks
				BlockArray m_blocks;

				//! Block size type, 0=8K, 1=16K blocks, 2=32K blocks
				uint32_t m_sizeType : 2;
				//! Number of blocks in the block array
				uint32_t m_blockCnt : NBlocks_Bits;
				//! Number of used blocks out of NBlocks
				uint32_t m_usedBlocks : NBlocks_Bits;
				//! Index of the next block to recycle
				uint32_t m_nextFreeBlock : NBlocks_Bits;
				//! Number of blocks to recycle
				uint32_t m_freeBlocks : NBlocks_Bits;
				//! Free bits to use in the future
				// uint32_t m_unused : 6;

	#if GAIA_ASSERT_ENABLED
				uint64_t m_usedMask = 0;
	#endif

				MemoryPage(void* ptr, uint8_t sizeType):
						MemoryPageHeader(ptr), m_sizeType(sizeType), m_blockCnt(0), m_usedBlocks(0), m_nextFreeBlock(0),
						m_freeBlocks(0) {
	#if GAIA_ASSERT_ENABLED
					static_assert(sizeof(MemoryPage) <= 72);
	#else
					static_assert(sizeof(MemoryPage) <= 64);
	#endif
				}

				void write_block_idx(uint32_t blockIdx, uint32_t value) {
					const uint32_t bitPosition = blockIdx * NBlocks_Bits;

					GAIA_ASSERT(bitPosition < NBlocks * NBlocks_Bits);
					GAIA_ASSERT(value <= InvalidBlockId);

					BitView{{(uint8_t*)m_blocks.data(), BlockArrayBytes}}.set(bitPosition, (uint8_t)value);
				}

				uint8_t read_block_idx(uint32_t blockIdx) const {
					const uint32_t bitPosition = blockIdx * NBlocks_Bits;

					GAIA_ASSERT(bitPosition < NBlocks * NBlocks_Bits);

					return BitView{{(uint8_t*)m_blocks.data(), BlockArrayBytes}}.get(bitPosition);
				}

				GAIA_NODISCARD void* alloc_block(
	#if GAIA_DEBUG
						uint32_t bytesWanted
	#endif
				) {
					auto StoreBlockAddress = [&](uint32_t index) {
						// Encode info about chunk's page in the memory block.
						// The actual pointer returned is offset by MemoryBlockUsableOffset bytes
						auto* pMemoryBlock = (uint8_t*)m_data + (index * mem_block_size(m_sizeType));
						GAIA_ASSERT((uintptr_t)pMemoryBlock % MemoryBlockAlignment == 0);
						auto& header = block_header(pMemoryBlock);
						header.m_pageAddr = (uintptr_t)this;
						header.m_reserved = 0;
	#if GAIA_DEBUG
						header.m_requestedBytes = bytesWanted;
	#endif
						return (void*)(pMemoryBlock + MemoryBlockUsableOffset);
					};

					// We don't want to go out of range for new blocks
					GAIA_ASSERT(!full() && "Trying to allocate too many blocks!");

					uint32_t index = 0;
					if (m_freeBlocks == 0U) {
						index = m_blockCnt;
						++m_usedBlocks;
						++m_blockCnt;
						write_block_idx(index, index);
					} else {
						GAIA_ASSERT(m_nextFreeBlock < m_blockCnt && "Block allocator recycle list broken!");

						++m_usedBlocks;
						--m_freeBlocks;

						index = m_nextFreeBlock;
						m_nextFreeBlock = read_block_idx(m_nextFreeBlock);
					}

	#if GAIA_ASSERT_ENABLED
					GAIA_ASSERT((m_usedMask & (uint64_t(1) << index)) == 0 && "Block already marked as live");
					m_usedMask |= uint64_t(1) << index;
	#endif

					return StoreBlockAddress(index);
				}

				void free_block(void* pBlock) {
					GAIA_ASSERT(pBlock != nullptr);
					GAIA_ASSERT(m_usedBlocks > 0);
					GAIA_ASSERT(m_freeBlocks <= NBlocks);

					// Offset the chunk memory so we get the real block address
					const auto* pMemoryBlock = (uint8_t*)pBlock - MemoryBlockUsableOffset;
					const auto blckAddr = (uintptr_t)pMemoryBlock;
					GAIA_ASSERT(blckAddr % MemoryBlockAlignment == 0);
					const auto dataAddr = (uintptr_t)m_data;
					const auto blockSize = (uintptr_t)mem_block_size(m_sizeType);
					const auto pageSize = blockSize * NBlocks;
					GAIA_ASSERT(blckAddr >= dataAddr);
					GAIA_ASSERT(blckAddr < dataAddr + pageSize);
					GAIA_ASSERT((blckAddr - dataAddr) % blockSize == 0);
					const auto blockIdx = (uint32_t)((blckAddr - dataAddr) / blockSize);
					GAIA_ASSERT(blockIdx < m_blockCnt);

	#if GAIA_DEBUG
					auto& header = block_header((void*)pMemoryBlock);
					GAIA_ASSERT(header.m_requestedBytes > 0);
	#endif
	#if GAIA_ASSERT_ENABLED
					GAIA_ASSERT((m_usedMask & (uint64_t(1) << blockIdx)) != 0 && "Double free or corrupted block state");
					m_usedMask &= ~(uint64_t(1) << blockIdx);
	#endif

	#if GAIA_DEBUG
					header.m_requestedBytes = 0;
					std::memset(pBlock, FreedBlockPattern, blockSize - MemoryBlockUsableOffset);
	#endif

					// Update our implicit list
					if (m_freeBlocks == 0U)
						write_block_idx(blockIdx, InvalidBlockId);
					else
						write_block_idx(blockIdx, m_nextFreeBlock);
					m_nextFreeBlock = blockIdx;

					++m_freeBlocks;
					--m_usedBlocks;
				}

				GAIA_NODISCARD uint32_t used_blocks_cnt() const {
					return m_usedBlocks;
				}

				GAIA_NODISCARD bool full() const {
					return used_blocks_cnt() >= NBlocks;
				}

				GAIA_NODISCARD bool empty() const {
					return used_blocks_cnt() == 0;
				}

				void verify() const {
	#if GAIA_ASSERT_ENABLED
					GAIA_ASSERT(m_sizeType < 3);
					GAIA_ASSERT(m_blockCnt <= NBlocks);
					GAIA_ASSERT(m_usedBlocks <= m_blockCnt);
					GAIA_ASSERT(m_freeBlocks <= m_blockCnt);
					GAIA_ASSERT(m_usedBlocks + m_freeBlocks == m_blockCnt);

					const auto blockSize = (uintptr_t)mem_block_size(m_sizeType);

					const auto pageAddr = (uintptr_t)m_data;
					GAIA_ASSERT(pageAddr % MemoryBlockAlignment == 0);

					[[maybe_unused]] uint64_t freeMask = 0;

					if (m_freeBlocks != 0) {
						uint32_t next = m_nextFreeBlock;
						GAIA_FOR(m_freeBlocks) {
							GAIA_ASSERT(next < m_blockCnt);
		#if GAIA_DEBUG
							const auto bit = uint64_t(1) << next;
							GAIA_ASSERT((freeMask & bit) == 0 && "Free list contains a cycle");
							freeMask |= bit;
		#endif
							next = read_block_idx(next);
						}

						GAIA_ASSERT(next == InvalidBlockId);
					}

					GAIA_FOR(m_blockCnt) {
						const auto* pMemoryBlock = (const uint8_t*)m_data + (i * blockSize);
						const auto& header = block_header(pMemoryBlock);
						GAIA_ASSERT(header.m_pageAddr == (uintptr_t)this);
						GAIA_ASSERT(((uintptr_t)pMemoryBlock % MemoryBlockAlignment) == 0);

		#if GAIA_DEBUG
						const bool isFree = (freeMask & (uint64_t(1) << i)) != 0;
						GAIA_ASSERT((header.m_requestedBytes == 0) == isFree);
		#endif
					}

					GAIA_ASSERT((m_usedMask & freeMask) == 0);
					const auto liveMask = m_blockCnt == 64 ? ~uint64_t(0) : ((uint64_t(1) << m_blockCnt) - 1);
					GAIA_ASSERT((m_usedMask | freeMask) == liveMask);
	#endif
				}

	#if GAIA_DEBUG
				GAIA_NODISCARD uint64_t requested_bytes() const {
					if (m_usedBlocks == 0)
						return 0;

					uint64_t freeMask = 0;
					uint32_t next = m_nextFreeBlock;
					GAIA_FOR(m_freeBlocks) {
						GAIA_ASSERT(next < m_blockCnt);
						const auto bit = uint64_t(1) << next;
						GAIA_ASSERT((freeMask & bit) == 0 && "Free list contains a cycle");
						freeMask |= bit;
						next = read_block_idx(next);
					}

					uint64_t requested = 0;
					GAIA_FOR(m_blockCnt) {
						if ((freeMask & (uint64_t(1) << i)) != 0)
							continue;

						const auto* pMemoryBlock = (const uint8_t*)m_data + (i * mem_block_size(m_sizeType));
						requested += block_header(pMemoryBlock).m_requestedBytes;
					}

					return requested;
				}
	#endif

			private:
				static MemoryBlockHeader& block_header(void* pMemoryBlock) {
					return *(MemoryBlockHeader*)pMemoryBlock;
				}

				static const MemoryBlockHeader& block_header(const void* pMemoryBlock) {
					return *(const MemoryBlockHeader*)pMemoryBlock;
				}
			};

			enum class MemoryPageState : uint8_t { Detached, Empty, Partial, Full };

			struct MemoryPageContainer {
				//! Pages with no live blocks
				cnt::fwd_llist<MemoryPage> pagesEmpty;
				//! Pages with some live blocks and spare capacity
				cnt::fwd_llist<MemoryPage> pagesPartial;
				//! Array of full pages
				cnt::fwd_llist<MemoryPage> pagesFull;

				GAIA_NODISCARD bool empty() const {
					return pagesEmpty.empty() && pagesPartial.empty() && pagesFull.empty();
				}
			};

			//! Allocator for ECS Chunks. Memory is organized in pages of chunks.
			class ChunkAllocatorImpl {
				friend ::gaia::ecs::ChunkAllocator;

				//! Container for pages storing various-sized chunks
				MemoryPageContainer m_pages[3];

				//! When true, destruction has been requested
				bool m_isDone = false;

			private:
				ChunkAllocatorImpl() = default;

				void on_delete() {
					flush(true);

					// Make sure there are no leaks
					auto memStats = stats();
					for (const auto& s: memStats.stats) {
						if (s.mem_total != 0) {
							GAIA_ASSERT2(false, "ECS leaking memory");
							GAIA_LOG_W("ECS leaking memory!");
							diag();
						}
					}
				}

			public:
				~ChunkAllocatorImpl() {
					on_delete();
				}

				ChunkAllocatorImpl(ChunkAllocatorImpl&& world) = delete;
				ChunkAllocatorImpl(const ChunkAllocatorImpl& world) = delete;
				ChunkAllocatorImpl& operator=(ChunkAllocatorImpl&&) = delete;
				ChunkAllocatorImpl& operator=(const ChunkAllocatorImpl&) = delete;

				//! Allocates memory
				void* alloc(uint32_t bytesWanted) {
					GAIA_ASSERT(bytesWanted > 0);
					GAIA_ASSERT(bytesWanted <= MaxMemoryBlockSize);
					if (bytesWanted == 0 || bytesWanted > MaxMemoryBlockSize)
						return nullptr;

					const auto sizeType = mem_block_size_type(bytesWanted);
					auto& container = m_pages[sizeType];

					MemoryPageState prevState = MemoryPageState::Partial;
					auto* pPage = container.pagesPartial.first;
					if (pPage == nullptr) {
						prevState = MemoryPageState::Empty;
						pPage = container.pagesEmpty.first;
						if (pPage == nullptr) {
							prevState = MemoryPageState::Detached;
							pPage = alloc_page(sizeType);
						}
					}

					// Allocate a new chunk of memory
	#if GAIA_DEBUG
					void* pBlock = pPage->alloc_block(bytesWanted);
	#else
					void* pBlock = pPage->alloc_block();
	#endif

					move_page(container, pPage, prevState, state_for(*pPage));
					verify();
					return pBlock;
				}

				GAIA_CLANG_WARNING_PUSH()
				// Memory is aligned so we can silence this warning
				GAIA_CLANG_WARNING_DISABLE("-Wcast-align")

				//! Releases memory allocated for pointer
				void free(void* pBlock) {
					GAIA_ASSERT(pBlock != nullptr);
					if (pBlock == nullptr)
						return;

					// Decode the page from the address
					const auto& header = *(const MemoryBlockHeader*)((uint8_t*)pBlock - MemoryBlockUsableOffset);
					const auto pageAddr = header.m_pageAddr;
					GAIA_ASSERT(pageAddr % sizeof(uintptr_t) == 0);
	#if GAIA_DEBUG
					GAIA_ASSERT(header.m_requestedBytes > 0);
	#endif
					auto* pPage = (MemoryPage*)pageAddr;
					const auto prevState = state_for(*pPage);

					auto& container = m_pages[pPage->m_sizeType];

	#if GAIA_ASSERT_ENABLED
					if (prevState == MemoryPageState::Full) {
						const auto res = container.pagesFull.has(pPage);
						GAIA_ASSERT(res && "Memory page couldn't be found among full pages");
					} else if (prevState == MemoryPageState::Partial) {
						const auto res = container.pagesPartial.has(pPage);
						GAIA_ASSERT(res && "Memory page couldn't be found among partial pages");
					} else {
						GAIA_ASSERT(false && "Allocated block can't belong to an empty page");
					}
	#endif

					// Free the chunk
					pPage->free_block(pBlock);

					// Update lists
					move_page(container, pPage, prevState, state_for(*pPage));
					verify();

					// Special handling for the allocator signaled to destroy itself
					if (m_isDone) {
						if (pPage->empty()) {
							container.pagesEmpty.unlink(pPage);
							free_page(pPage);
						}

						try_delete_this();
					}
				}

				GAIA_CLANG_WARNING_POP()

				//! Returns allocator statistics
				ChunkAllocatorStats stats() const {
					ChunkAllocatorStats stats{};
					stats.stats[0] = page_stats(0);
					stats.stats[1] = page_stats(1);
					stats.stats[2] = page_stats(2);
					return stats;
				}

				//! Flushes unused memory.
				//! Keeps a small, size-class-specific empty-page cache warm by default.
				void flush(bool releaseAll = false) {
					uint32_t i = 0;
					for (auto& page: m_pages)
						flushPages(page, i++, releaseAll);
					verify();
				}

				//! Performs diagnostics of the memory used.
				void diag() const {
					auto diagPage = [](const ChunkAllocatorPageStats& stats, uint32_t sizeType) {
						GAIA_LOG_N("ChunkAllocator %uK stats", mem_block_size(sizeType) / 1024);
						GAIA_LOG_N("  Allocated: %" PRIu64 " B", stats.mem_total);
						GAIA_LOG_N("  Reserved by live blocks: %" PRIu64 " B", stats.mem_used);
						GAIA_LOG_N("  Pages: %u", stats.num_pages);
						GAIA_LOG_N("  Reusable pages: %u", stats.num_pages_free);
	#if !GAIA_DEBUG
						GAIA_LOG_N(
								"  Utilization: %.1f%%",
								stats.mem_total ? 100.0 * ((double)stats.mem_used / (double)stats.mem_total) : 0);
	#else
						GAIA_LOG_N("  Requested: %" PRIu64 " B", stats.mem_requested);
						GAIA_LOG_N("  Free capacity: %" PRIu64 " B", stats.mem_total - stats.mem_used);
						GAIA_LOG_N("  Internal slack: %" PRIu64 " B", stats.mem_used - stats.mem_requested);
						GAIA_LOG_N(
								"  Utilization: %.1f%%",
								stats.mem_total ? 100.0 * ((double)stats.mem_requested / (double)stats.mem_total) : 0);
						GAIA_LOG_N("  Empty pages: %u", stats.num_pages_empty);
	#endif
					};

					auto memStats = stats();
					diagPage(memStats.stats[0], 0);
					diagPage(memStats.stats[1], 1);
					diagPage(memStats.stats[2], 2);
				}

				void verify() const {
	#if GAIA_ASSERT_ENABLED
					for (uint32_t sizeType = 0; sizeType < 3; ++sizeType)
						verify_container(m_pages[sizeType], sizeType);
	#endif
				}

			private:
				static constexpr const char* s_strChunkAlloc_Chunk = "Chunk";
				static constexpr const char* s_strChunkAlloc_MemPage = "MemoryPage";

				static MemoryPage* alloc_page(uint8_t sizeType) {
					const uint32_t size = mem_block_size(sizeType) * MemoryPage::NBlocks;
					auto* pPageData = mem::AllocHelper::alloc_alig<uint8_t>(s_strChunkAlloc_Chunk, MemoryBlockAlignment, size);
					auto* pMemoryPage = mem::AllocHelper::alloc<MemoryPage>(s_strChunkAlloc_MemPage);
					return new (pMemoryPage) MemoryPage(pPageData, sizeType);
				}

				static void free_page(MemoryPage* pMemoryPage) {
					GAIA_ASSERT(pMemoryPage != nullptr);

					mem::AllocHelper::free_alig(s_strChunkAlloc_Chunk, pMemoryPage->m_data);
					pMemoryPage->~MemoryPage();
					mem::AllocHelper::free(s_strChunkAlloc_MemPage, pMemoryPage);
				}

				void done() {
					m_isDone = true;
				}

				void try_delete_this() {
					// When there is nothing left, delete the allocator
					bool allEmpty = true;
					for (const auto& c: m_pages)
						allEmpty = allEmpty && c.empty();
					if (allEmpty)
						delete this;
				}

				static constexpr uint32_t warm_pages_to_keep(uint32_t sizeType) {
					constexpr uint8_t WarmPagesPerSizeClass[] = {1, 1, 0};
					return WarmPagesPerSizeClass[sizeType];
				}

				static MemoryPageState state_for(const MemoryPage& page) {
					if (page.empty())
						return MemoryPageState::Empty;
					if (page.full())
						return MemoryPageState::Full;
					return MemoryPageState::Partial;
				}

				static cnt::fwd_llist<MemoryPage>& page_list(MemoryPageContainer& container, MemoryPageState state) {
					switch (state) {
						case MemoryPageState::Empty:
							return container.pagesEmpty;
						case MemoryPageState::Partial:
							return container.pagesPartial;
						default:
							GAIA_ASSERT(state == MemoryPageState::Full);
							return container.pagesFull;
					}
				}

				static void move_page(
						MemoryPageContainer& container, MemoryPage* pPage, MemoryPageState fromState, MemoryPageState toState) {
					if (fromState == toState)
						return;

					if (fromState != MemoryPageState::Detached)
						page_list(container, fromState).unlink(pPage);
					page_list(container, toState).link(pPage);
				}

				static void verify_page_membership(
						const MemoryPageContainer& container, const MemoryPage& page, MemoryPageState expectedState) {
					(void)container;
					GAIA_ASSERT(state_for(page) == expectedState);
					GAIA_ASSERT(page.get_fwd_llist_link().linked());
				}

				static void verify_container(const MemoryPageContainer& container, uint32_t sizeType) {
					(void)sizeType;
					for (const auto& page: container.pagesEmpty) {
						GAIA_ASSERT(page.m_sizeType == sizeType);
						verify_page_membership(container, page, MemoryPageState::Empty);
						page.verify();
					}

					for (const auto& page: container.pagesPartial) {
						GAIA_ASSERT(page.m_sizeType == sizeType);
						verify_page_membership(container, page, MemoryPageState::Partial);
						page.verify();
					}

					for (const auto& page: container.pagesFull) {
						GAIA_ASSERT(page.m_sizeType == sizeType);
						verify_page_membership(container, page, MemoryPageState::Full);
						page.verify();
					}
				}

				ChunkAllocatorPageStats page_stats(uint32_t sizeType) const {
					ChunkAllocatorPageStats stats{};
					const auto& container = m_pages[sizeType];
					const auto blockSize = (uint64_t)mem_block_size(sizeType);
					const auto pageSize = blockSize * MemoryPage::NBlocks;

					stats.num_pages = (uint32_t)container.pagesEmpty.size() + (uint32_t)container.pagesPartial.size() +
														(uint32_t)container.pagesFull.size();
					stats.num_pages_free = (uint32_t)container.pagesEmpty.size() + (uint32_t)container.pagesPartial.size();
					stats.mem_total = stats.num_pages * pageSize;
					stats.mem_used = container.pagesFull.size() * pageSize;

	#if GAIA_DEBUG
					stats.num_pages_empty = (uint32_t)container.pagesEmpty.size();

					for (const auto& page: container.pagesFull)
						stats.mem_requested += page.requested_bytes();

					for (const auto& page: container.pagesPartial) {
						stats.mem_used += page.used_blocks_cnt() * blockSize;
						stats.mem_requested += page.requested_bytes();
					}
	#else
					for (const auto& page: container.pagesPartial)
						stats.mem_used += page.used_blocks_cnt() * blockSize;
	#endif

					return stats;
				}

				//! Flushes unused memory.
				//! Keeps a small, size-class-specific empty-page cache warm by default.
				void flushPages(MemoryPageContainer& container, uint32_t sizeType, bool releaseAll) {
					const bool keepWarmPage = !releaseAll && warm_pages_to_keep(sizeType) != 0;
					bool keptWarmPage = false;
					for (auto it = container.pagesEmpty.begin(); it != container.pagesEmpty.end();) {
						auto* pPage = &(*it);
						++it;

						// Skip non-empty pages
						if (!pPage->empty())
							continue;

						if (keepWarmPage && !keptWarmPage) {
							keptWarmPage = true;
							continue;
						}

						container.pagesEmpty.unlink(pPage);
						free_page(pPage);
					}
				}
			};
		} // namespace detail

#endif

	} // namespace ecs
} // namespace gaia
