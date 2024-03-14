#pragma once
#include "../config/config.h"

#include <cstdint>

#if GAIA_ECS_CHUNK_ALLOCATOR
	#include <cinttypes>

	#include "../cnt/darray.h"
	#include "../cnt/sarray.h"
	#include "../cnt/sarray_ext.h"
	#include "../config/logging.h"
	#include "../config/profiler.h"
	#include "../core/bit_utils.h"
	#include "../core/dyn_singleton.h"
	#include "../core/span.h"
	#include "../core/utility.h"
	#include "../mem/mem_alloc.h"
	#include "common.h"
#endif

namespace gaia {
	namespace ecs {
		//! Size of one allocated block of memory
		static constexpr uint32_t MaxMemoryBlockSize = 16384;
		//! Unusable area at the beggining of the allocated block designated for special purposes
		static constexpr uint32_t MemoryBlockUsableOffset = sizeof(uintptr_t);

		inline constexpr uint16_t mem_block_size(uint32_t sizeType) {
			return sizeType != 0 ? MaxMemoryBlockSize : MaxMemoryBlockSize / 2;
		}

		inline constexpr uint8_t mem_block_size_type(uint32_t sizeBytes) {
			return (uint8_t)(sizeBytes > MaxMemoryBlockSize / 2);
		}

#if GAIA_ECS_CHUNK_ALLOCATOR
		struct ChunkAllocatorPageStats final {
			//! Total allocated memory
			uint64_t mem_total;
			//! Memory actively used
			uint64_t mem_used;
			//! Number of allocated pages
			uint32_t num_pages;
			//! Number of free pages
			uint32_t num_pages_free;
		};

		struct ChunkAllocatorStats final {
			ChunkAllocatorPageStats stats[2];
		};

		namespace detail {
			class ChunkAllocatorImpl;
		}
		using ChunkAllocator = core::dyn_singleton<detail::ChunkAllocatorImpl>;

		namespace detail {

			/*!
			Allocator for ECS Chunks. Memory is organized in pages of chunks.
			*/
			class ChunkAllocatorImpl {
				friend gaia::ecs::ChunkAllocator;

				struct MemoryPage {
					static constexpr uint16_t NBlocks = 62;
					static constexpr uint16_t NBlocks_Bits = (uint16_t)core::count_bits(NBlocks);
					static constexpr uint32_t InvalidBlockId = NBlocks + 1;
					static constexpr uint32_t BlockArrayBytes = ((uint32_t)NBlocks_Bits * (uint32_t)NBlocks + 7) / 8;
					using BlockArray = cnt::sarray<uint8_t, BlockArrayBytes>;
					using BitView = core::bit_view<NBlocks_Bits>;

					//! Pointer to data managed by page
					void* m_data;
					//! Index in the list of pages
					uint32_t m_idx;
					//! Block size type, 0=8K, 1=16K blocks
					uint32_t m_sizeType : 1;
					//! Number of blocks in the block array
					uint32_t m_blockCnt: NBlocks_Bits;
					//! Number of used blocks out of NBlocks
					uint32_t m_usedBlocks: NBlocks_Bits;
					//! Index of the next block to recycle
					uint32_t m_nextFreeBlock: NBlocks_Bits;
					//! Number of blocks to recycle
					uint32_t m_freeBlocks: NBlocks_Bits;
					//! Free bits to use in the future
					uint32_t m_unused : 7;
					//! Implicit list of blocks
					BlockArray m_blocks;

					MemoryPage(void* ptr, uint8_t sizeType):
							m_data(ptr), m_idx(0), m_sizeType(sizeType), m_blockCnt(0), m_usedBlocks(0), m_nextFreeBlock(0),
							m_freeBlocks(0), m_unused(0) {
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
							uint8_t* pMemoryBlock = (uint8_t*)m_data + index * mem_block_size(m_sizeType);
							GAIA_ASSERT((uintptr_t)pMemoryBlock % 16 == 0);
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
						GAIA_ASSERT(m_usedBlocks > 0);
						GAIA_ASSERT(m_freeBlocks <= NBlocks);

						// Offset the chunk memory so we get the real block address
						const auto* pMemoryBlock = (uint8_t*)pBlock - MemoryBlockUsableOffset;
						const auto blckAddr = (uintptr_t)pMemoryBlock;
						GAIA_ASSERT(blckAddr % 16 == 0);
						const auto dataAddr = (uintptr_t)m_data;
						const auto blockIdx = (uint32_t)((blckAddr - dataAddr) / mem_block_size(m_sizeType));

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
					//! List of available pages
					cnt::darray<MemoryPage*> pagesFree;
					//! List of full pages
					cnt::darray<MemoryPage*> pagesFull;

					GAIA_NODISCARD bool empty() const {
						return pagesFree.empty() && pagesFull.empty();
					}
				};

				//! Container for pages storing various-sized chunks
				MemoryPageContainer m_pages[2];

				//! When true, destruction has been requested
				bool m_isDone = false;

			private:
				ChunkAllocatorImpl() = default;

				void on_delete() {
					flush();

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

				/*!
				Allocates memory
				*/
				void* alloc(uint32_t bytesWanted) {
					GAIA_ASSERT(bytesWanted <= MaxMemoryBlockSize);

					void* pBlock = nullptr;
					MemoryPage* pPage = nullptr;

					const auto sizeType = mem_block_size_type(bytesWanted);
					auto& container = m_pages[sizeType];

					// Find first page with available space
					for (auto* p: container.pagesFree) {
						if (p->full())
							continue;
						pPage = p;
						break;
					}
					if (pPage == nullptr) {
						// Allocate a new page if no free page was found
						pPage = alloc_page(sizeType);
						container.pagesFree.push_back(pPage);
					}

					// Allocate a new chunk of memory
					pBlock = pPage->alloc_block();

					// Handle full pages
					if (pPage->full()) {
						// Remove the page from the open list and update the swapped page's pointer
						container.pagesFree.back()->m_idx = 0;
						core::erase_fast(container.pagesFree, 0);

						// Move our page to the full list
						pPage->m_idx = (uint32_t)container.pagesFull.size();
						container.pagesFull.push_back(pPage);
					}

					return pBlock;
				}

				GAIA_CLANG_WARNING_PUSH()
				// Memory is aligned so we can silence this warning
				GAIA_CLANG_WARNING_DISABLE("-Wcast-align")

				/*!
				Releases memory allocated for pointer
				*/
				void free(void* pBlock) {
					// Decode the page from the address
					const auto pageAddr = *(uintptr_t*)((uint8_t*)pBlock - MemoryBlockUsableOffset);
					GAIA_ASSERT(pageAddr % 16 == 0);
					auto* pPage = (MemoryPage*)pageAddr;
					const bool wasFull = pPage->full();

					auto& container = m_pages[pPage->m_sizeType];

	#if GAIA_ASSERT_ENABLED
					if (wasFull) {
						const auto res = core::has_if(container.pagesFull, [&](auto* page) {
							return page == pPage;
						});
						GAIA_ASSERT(res && "Memory page couldn't be found among full pages");
					} else {
						const auto res = core::has_if(container.pagesFree, [&](auto* page) {
							return page == pPage;
						});
						GAIA_ASSERT(res && "Memory page couldn't be found among free pages");
					}
	#endif

					// Free the chunk
					pPage->free_block(pBlock);

					// Update lists
					if (wasFull) {
						// Our page is no longer full. Remove it from the full list and update the swapped page's pointer
						container.pagesFull.back()->m_idx = pPage->m_idx;
						core::erase_fast(container.pagesFull, pPage->m_idx);

						// Move our page to the open list
						pPage->m_idx = (uint32_t)container.pagesFree.size();
						container.pagesFree.push_back(pPage);
					}

					// Special handling for the allocator signaled to destroy itself
					if (m_isDone) {
						// Remove the page right away
						if (pPage->empty()) {
							GAIA_ASSERT(!container.pagesFree.empty());
							container.pagesFree.back()->m_idx = pPage->m_idx;
							core::erase_fast(container.pagesFree, pPage->m_idx);
						}

						try_delete_this();
					}
				}

				GAIA_CLANG_WARNING_POP()

				/*!
				Returns allocator statistics
				*/
				ChunkAllocatorStats stats() const {
					ChunkAllocatorStats stats;
					stats.stats[0] = page_stats(0);
					stats.stats[1] = page_stats(1);
					return stats;
				}

				/*!
				Flushes unused memory
				*/
				void flush() {
					auto flushPages = [](MemoryPageContainer& container) {
						for (uint32_t i = 0; i < container.pagesFree.size();) {
							auto* pPage = container.pagesFree[i];

							// Skip non-empty pages
							if (!pPage->empty()) {
								++i;
								continue;
							}

							GAIA_ASSERT(pPage->m_idx == i);
							container.pagesFree.back()->m_idx = i;
							core::erase_fast(container.pagesFree, i);
							free_page(pPage);
						}
					};

					for (auto& c: m_pages)
						flushPages(c);
				}

				/*!
				Performs diagnostics of the memory used.
				*/
				void diag() const {
					auto diagPage = [](const ChunkAllocatorPageStats& memstats) {
						GAIA_LOG_N("ChunkAllocator stats");
						GAIA_LOG_N("  Allocated: %" PRIu64 " B", memstats.mem_total);
						GAIA_LOG_N("  Used: %" PRIu64 " B", memstats.mem_total - memstats.mem_used);
						GAIA_LOG_N("  Overhead: %" PRIu64 " B", memstats.mem_used);
						GAIA_LOG_N("  Utilization: %.1f%%", 100.0 * ((double)memstats.mem_used / (double)memstats.mem_total));
						GAIA_LOG_N("  Pages: %u", memstats.num_pages);
						GAIA_LOG_N("  Free pages: %u", memstats.num_pages_free);
					};

					diagPage(page_stats(0));
					diagPage(page_stats(1));
				}

			private:
				static MemoryPage* alloc_page(uint8_t sizeType) {
					const uint32_t size = mem_block_size(sizeType) * MemoryPage::NBlocks;
					auto* pPageData = mem::AllocHelper::alloc_alig<uint8_t>(16U, size);
					auto* pMemoryPage = mem::AllocHelper::alloc<MemoryPage>();
					return new (pMemoryPage) MemoryPage(pPageData, sizeType);
				}

				static void free_page(MemoryPage* pMemoryPage) {
					mem::AllocHelper::free_alig(pMemoryPage->m_data);
					pMemoryPage->~MemoryPage();
					mem::AllocHelper::free(pMemoryPage);
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
					const MemoryPageContainer& container = m_pages[sizeType];
					stats.num_pages = (uint32_t)container.pagesFree.size() + (uint32_t)container.pagesFull.size();
					stats.num_pages_free = (uint32_t)container.pagesFree.size();
					stats.mem_total = stats.num_pages * (size_t)mem_block_size(sizeType) * MemoryPage::NBlocks;
					stats.mem_used = container.pagesFull.size() * (size_t)mem_block_size(sizeType) * MemoryPage::NBlocks;
					for (auto* page: container.pagesFree)
						stats.mem_used += page->used_blocks_cnt() * (size_t)MaxMemoryBlockSize;
					return stats;
				};
			};
		} // namespace detail

#endif

	} // namespace ecs
} // namespace gaia
