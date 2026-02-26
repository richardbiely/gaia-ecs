#pragma once
#include "gaia/config/config.h"

#include <cinttypes>
#include <cstdint>

#include "gaia/cnt/fwd_llist.h"
#include "gaia/cnt/sarray.h"
#include "gaia/core/bit_utils.h"
#include "gaia/core/dyn_singleton.h"
#include "gaia/core/utility.h"
#include "gaia/mem/mem_alloc.h"
#include "gaia/util/logging.h"

namespace gaia {
	namespace ecs {
		static constexpr uint32_t MinMemoryBlockSize = 1024 * 8;
		//! Size of one allocated block of memory in kiB
		static constexpr uint32_t MaxMemoryBlockSize = MinMemoryBlockSize * 4;
		//! Unusable area at the beginning of the allocated block designated for special purposes
		static constexpr uint32_t MemoryBlockUsableOffset = sizeof(uintptr_t);

		constexpr uint16_t mem_block_size(uint32_t sizeType) {
			constexpr uint16_t sizes[] = {MinMemoryBlockSize, MinMemoryBlockSize * 2, MaxMemoryBlockSize};
			return sizes[sizeType];
		}

		constexpr uint8_t mem_block_size_type(uint32_t sizeBytes) {
			// Ceil division by smallest block size
			const uint32_t blocks = (sizeBytes + MinMemoryBlockSize - 1) / MinMemoryBlockSize;
			return blocks > 2 ? 2 : static_cast<uint8_t>(blocks - 1);
		}

#if GAIA_ECS_CHUNK_ALLOCATOR
		struct GAIA_API ChunkAllocatorPageStats final {
			//! Total allocated memory
			uint64_t mem_total;
			//! Memory actively used
			uint64_t mem_used;
			//! Number of allocated pages
			uint32_t num_pages;
			//! Number of free pages
			uint32_t num_pages_free;
		};

		struct GAIA_API ChunkAllocatorStats final {
			ChunkAllocatorPageStats stats[3];
		};

		namespace detail {
			class ChunkAllocatorImpl;
		}
		using ChunkAllocator = core::dyn_singleton<detail::ChunkAllocatorImpl>;

		namespace detail {

			struct MemoryPageHeader {
				//! Pointer to data managed by page
				void* m_data;

				MemoryPageHeader(void* ptr): m_data(ptr) {}
			};

			struct MemoryPage: MemoryPageHeader, cnt::fwd_llist_base<MemoryPage> {
				static constexpr uint16_t NBlocks = 48;
				static constexpr uint16_t NBlocks_Bits = (uint16_t)core::count_bits(NBlocks);
				static constexpr uint32_t InvalidBlockId = NBlocks + 1;
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

				MemoryPage(void* ptr, uint8_t sizeType):
						MemoryPageHeader(ptr), m_sizeType(sizeType), m_blockCnt(0), m_usedBlocks(0), m_nextFreeBlock(0),
						m_freeBlocks(0) {
					// One cacheline long on x86. The point is for this to be as small as possible
					static_assert(sizeof(MemoryPage) <= 64);
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

				GAIA_NODISCARD void* alloc_block() {
					auto StoreBlockAddress = [&](uint32_t index) {
						// Encode info about chunk's page in the memory block.
						// The actual pointer returned is offset by MemoryBlockUsableOffset bytes
						uint8_t* pMemoryBlock = (uint8_t*)m_data + (index * mem_block_size(m_sizeType));
						GAIA_ASSERT((uintptr_t)pMemoryBlock % sizeof(uintptr_t) == 0);
						mem::unaligned_ref<uintptr_t>{pMemoryBlock} = (uintptr_t)this;
						return (void*)(pMemoryBlock + MemoryBlockUsableOffset);
					};

					// We don't want to go out of range for new blocks
					GAIA_ASSERT(!full() && "Trying to allocate too many blocks!");

					if (m_freeBlocks == 0U) {
						const auto index = m_blockCnt;
						++m_usedBlocks;
						++m_blockCnt;
						write_block_idx(index, index);

						return StoreBlockAddress(index);
					}

					GAIA_ASSERT(m_nextFreeBlock < m_blockCnt && "Block allocator recycle list broken!");

					++m_usedBlocks;
					--m_freeBlocks;

					const auto index = m_nextFreeBlock;
					m_nextFreeBlock = read_block_idx(m_nextFreeBlock);

					return StoreBlockAddress(index);
				}

				void free_block(void* pBlock) {
					GAIA_ASSERT(pBlock != nullptr);
					GAIA_ASSERT(m_usedBlocks > 0);
					GAIA_ASSERT(m_freeBlocks <= NBlocks);

					// Offset the chunk memory so we get the real block address
					const auto* pMemoryBlock = (uint8_t*)pBlock - MemoryBlockUsableOffset;
					const auto blckAddr = (uintptr_t)pMemoryBlock;
					GAIA_ASSERT(blckAddr % sizeof(uintptr_t) == 0);
					const auto dataAddr = (uintptr_t)m_data;
					const auto blockSize = (uintptr_t)mem_block_size(m_sizeType);
					const auto pageSize = blockSize * NBlocks;
					GAIA_ASSERT(blckAddr >= dataAddr);
					GAIA_ASSERT(blckAddr < dataAddr + pageSize);
					GAIA_ASSERT((blckAddr - dataAddr) % blockSize == 0);
					const auto blockIdx = (uint32_t)((blckAddr - dataAddr) / blockSize);
					GAIA_ASSERT(blockIdx < m_blockCnt);

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
			};

				struct MemoryPageContainer {
					//! Array of available pages
					cnt::fwd_llist<MemoryPage> pagesFree;
					//! Array of full pages
					cnt::fwd_llist<MemoryPage> pagesFull;

					GAIA_NODISCARD bool empty() const {
						return pagesFree.empty() && pagesFull.empty();
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
					GAIA_ASSERT(bytesWanted <= MaxMemoryBlockSize);

					void* pBlock = nullptr;
					const auto sizeType = mem_block_size_type(bytesWanted);
					auto& container = m_pages[sizeType];

					// Free list contains only pages with available capacity.
					auto* pPage = container.pagesFree.first;
					GAIA_ASSERT(pPage == nullptr || !pPage->full());
					if (pPage == nullptr) {
						// Allocate a new page if no free page was found
						pPage = alloc_page(sizeType);
						container.pagesFree.link(pPage);
					}

					// Allocate a new chunk of memory
					pBlock = pPage->alloc_block();

					// Handle full pages
					if (pPage->full()) {
						// Remove the page from the open list
						container.pagesFree.unlink(pPage);
						// Move our page to the full list
						container.pagesFull.link(pPage);
					}

					return pBlock;
				}

				GAIA_CLANG_WARNING_PUSH()
				// Memory is aligned so we can silence this warning
				GAIA_CLANG_WARNING_DISABLE("-Wcast-align")

				//! Releases memory allocated for pointer
				void free(void* pBlock) {
					// Decode the page from the address
					const auto pageAddr = *(uintptr_t*)((uint8_t*)pBlock - MemoryBlockUsableOffset);
					GAIA_ASSERT(pageAddr % sizeof(uintptr_t) == 0);
					auto* pPage = (MemoryPage*)pageAddr;
					const bool wasFull = pPage->full();

					auto& container = m_pages[pPage->m_sizeType];

	#if GAIA_ASSERT_ENABLED
					if (wasFull) {
						const auto res = container.pagesFull.has(pPage);
						GAIA_ASSERT(res && "Memory page couldn't be found among full pages");
					} else {
						const auto res = container.pagesFree.has(pPage);
						GAIA_ASSERT(res && "Memory page couldn't be found among free pages");
					}
	#endif

					// Free the chunk
					pPage->free_block(pBlock);

					// Update lists
					if (wasFull) {
						// Our page is no longer full
						container.pagesFull.unlink(pPage);
						// Move our page to the open list
						container.pagesFree.link(pPage);
					}

					// Special handling for the allocator signaled to destroy itself
					if (m_isDone) {
						// Remove the page right away
						if (pPage->empty()) {
							GAIA_ASSERT(!container.pagesFree.empty());
							container.pagesFree.unlink(pPage);
							free_page(pPage);
						}

						try_delete_this();
					}
				}

				GAIA_CLANG_WARNING_POP()

				//! Returns allocator statistics
				ChunkAllocatorStats stats() const {
					ChunkAllocatorStats stats;
					stats.stats[0] = page_stats(0);
					stats.stats[1] = page_stats(1);
					stats.stats[2] = page_stats(2);
					return stats;
				}

				//! Flushes unused memory.
				//! By default keeps one empty page per size class warm to reduce allocation churn.
				void flush(bool releaseAll = false) {
					auto flushPages = [releaseAll](MemoryPageContainer& container) {
						bool keptWarmPage = false;
						for (auto it = container.pagesFree.begin(); it != container.pagesFree.end();) {
							auto* pPage = &(*it);
							++it;

							// Skip non-empty pages
							if (!pPage->empty())
								continue;
							if (!releaseAll && !keptWarmPage) {
								keptWarmPage = true;
								continue;
							}

							container.pagesFree.unlink(pPage);
							free_page(pPage);
						}
					};

					for (auto& c: m_pages)
						flushPages(c);
				}

				//! Performs diagnostics of the memory used.
				void diag() const {
					auto diagPage = [](const ChunkAllocatorPageStats& stats, uint32_t sizeType) {
						GAIA_LOG_N("ChunkAllocator %uK stats", mem_block_size(sizeType) / 1024);
						GAIA_LOG_N("  Allocated: %" PRIu64 " B", stats.mem_total);
						GAIA_LOG_N("  Used: %" PRIu64 " B", stats.mem_used);
						GAIA_LOG_N("  Overhead: %" PRIu64 " B", stats.mem_total - stats.mem_used);
						GAIA_LOG_N(
								"  Utilization: %.1f%%",
								stats.mem_total ? 100.0 * ((double)stats.mem_used / (double)stats.mem_total) : 0);
						GAIA_LOG_N("  Pages: %u", stats.num_pages);
						GAIA_LOG_N("  Free pages: %u", stats.num_pages_free);
					};

					auto memStats = stats();
					diagPage(memStats.stats[0], 0);
					diagPage(memStats.stats[1], 1);
					diagPage(memStats.stats[2], 2);
				}

			private:
				static constexpr const char* s_strChunkAlloc_Chunk = "Chunk";
				static constexpr const char* s_strChunkAlloc_MemPage = "MemoryPage";

				static MemoryPage* alloc_page(uint8_t sizeType) {
					const uint32_t size = mem_block_size(sizeType) * MemoryPage::NBlocks;
					auto* pPageData = mem::AllocHelper::alloc_alig<uint8_t>(s_strChunkAlloc_Chunk, 16U, size);
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

				ChunkAllocatorPageStats page_stats(uint32_t sizeType) const {
					ChunkAllocatorPageStats stats{};
					const auto& container = m_pages[sizeType];
					const auto blockSize = (size_t)mem_block_size(sizeType);

					stats.num_pages = (uint32_t)container.pagesFree.size() + (uint32_t)container.pagesFull.size();
					stats.num_pages_free = (uint32_t)container.pagesFree.size();
					stats.mem_total = stats.num_pages * blockSize * MemoryPage::NBlocks;
					stats.mem_used = container.pagesFull.size() * blockSize * MemoryPage::NBlocks;

					const auto& pagesFree = container.pagesFree;
					for (const auto& page: pagesFree)
						stats.mem_used += page.used_blocks_cnt() * blockSize;
					return stats;
				}
			};
		} // namespace detail

#endif

	} // namespace ecs
} // namespace gaia
