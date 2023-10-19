#pragma once
#include "../config/config.h"

#include <cinttypes>
#include <cstdint>

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


namespace gaia {
	namespace ecs {
		//! Size of one allocated block of memory
		static constexpr uint32_t MaxMemoryBlockSize = 16384;
		//! Unusable area at the beggining of the allocated block designated for special pruposes
		static constexpr uint32_t MemoryBlockUsableOffset = sizeof(uintptr_t);

		struct ChunkAllocatorPageStats final {
			//! Total allocated memory
			uint64_t AllocatedMemory;
			//! Memory actively used
			uint64_t UsedMemory;
			//! Number of allocated pages
			uint32_t NumPages;
			//! Number of free pages
			uint32_t NumFreePages;
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
					//! Implicit list of blocks
					BlockArray m_blocks;
					//! Chunk block size
					uint8_t m_sizeType;
					//! Index in the list of pages
					uint32_t m_pageIdx;
					//! Number of blocks in the block array
					uint32_t m_blockCnt: NBlocks_Bits;
					//! Number of used blocks out of NBlocks
					uint32_t m_usedBlocks: NBlocks_Bits;
					//! Index of the next block to recycle
					uint32_t m_nextFreeBlock: NBlocks_Bits;
					//! Number of blocks to recycle
					uint32_t m_freeBlocks: NBlocks_Bits;

					MemoryPage(void* ptr, uint8_t sizeType):
							m_data(ptr), m_sizeType(sizeType), m_pageIdx(0), m_blockCnt(0), m_usedBlocks(0), m_nextFreeBlock(0),
							m_freeBlocks(0) {
						// One cacheline long on x86. The point is for this to be as small as possible
						static_assert(sizeof(MemoryPage) <= 64);
					}

					void WriteBlockIdx(uint32_t blockIdx, uint32_t value) {
						const uint32_t bitPosition = blockIdx * NBlocks_Bits;

						GAIA_ASSERT(bitPosition < NBlocks * NBlocks_Bits);
						GAIA_ASSERT(value <= InvalidBlockId);

						BitView{{(uint8_t*)m_blocks.data(), BlockArrayBytes}}.set(bitPosition, (uint8_t)value);
					}

					uint8_t ReadBlockIdx(uint32_t blockIdx) const {
						const uint32_t bitPosition = blockIdx * NBlocks_Bits;

						GAIA_ASSERT(bitPosition < NBlocks * NBlocks_Bits);

						return BitView{{(uint8_t*)m_blocks.data(), BlockArrayBytes}}.get(bitPosition);
					}

					GAIA_NODISCARD void* AllocChunk() {
						auto StoreChunkAddress = [&](uint32_t index) {
							// Encode info about chunk's page in the memory block.
							// The actual pointer returned is offset by UsableOffset bytes
							uint8_t* pMemoryBlock = (uint8_t*)m_data + index * GetMemoryBlockSize(m_sizeType);
							*mem::unaligned_pointer<uintptr_t>{pMemoryBlock} = (uintptr_t)this;
							return (void*)(pMemoryBlock + MemoryBlockUsableOffset);
						};

						if (m_freeBlocks == 0U) {
							// We don't want to go out of range for new blocks
							GAIA_ASSERT(!IsFull() && "Trying to allocate too many blocks!");

							const auto index = m_blockCnt;
							++m_usedBlocks;
							++m_blockCnt;
							WriteBlockIdx(index, index);

							return StoreChunkAddress(index);
						}

						GAIA_ASSERT(m_nextFreeBlock < m_blockCnt && "Block allocator recycle cnt::list broken!");

						++m_usedBlocks;
						--m_freeBlocks;

						const auto index = m_nextFreeBlock;
						m_nextFreeBlock = ReadBlockIdx(m_nextFreeBlock);

						return StoreChunkAddress(index);
					}

					void FreeChunk(void* pChunk) {
						GAIA_ASSERT(m_freeBlocks <= NBlocks);

						// Offset the chunk memory so we get the real block address
						const auto* pMemoryBlock = (uint8_t*)pChunk - MemoryBlockUsableOffset;
						const auto blckAddr = (uintptr_t)pMemoryBlock;
						const auto dataAddr = (uintptr_t)m_data;
						const auto blockIdx = (uint32_t)((blckAddr - dataAddr) / GetMemoryBlockSize(m_sizeType));

						// Update our implicit cnt::list
						if (m_freeBlocks == 0U) {
							WriteBlockIdx(blockIdx, InvalidBlockId);
							m_nextFreeBlock = blockIdx;
						} else {
							WriteBlockIdx(blockIdx, m_nextFreeBlock);
							m_nextFreeBlock = blockIdx;
						}

						++m_freeBlocks;
						--m_usedBlocks;
					}

					GAIA_NODISCARD uint32_t GetUsedBlocks() const {
						return m_usedBlocks;
					}

					GAIA_NODISCARD bool IsFull() const {
						return m_usedBlocks == NBlocks;
					}

					GAIA_NODISCARD bool IsEmpty() const {
						return m_usedBlocks == 0;
					}
				};

				struct MemoryPageContainer {
					//! List of available pages
					//! Note, this currently only contains at most 1 item
					cnt::darray<MemoryPage*> pagesFree;
					//! List of full pages
					cnt::darray<MemoryPage*> pagesFull;
				};

				//! Container for pages storing various-sized chunks
				MemoryPageContainer m_pages[2];

				//! When true, destruction has been requested
				bool m_isDone = false;

				ChunkAllocatorImpl() noexcept = default;

			public:
				~ChunkAllocatorImpl() = default;

				ChunkAllocatorImpl(ChunkAllocatorImpl&& world) = delete;
				ChunkAllocatorImpl(const ChunkAllocatorImpl& world) = delete;
				ChunkAllocatorImpl& operator=(ChunkAllocatorImpl&&) = delete;
				ChunkAllocatorImpl& operator=(const ChunkAllocatorImpl&) = delete;

				static uint16_t GetMemoryBlockSize(uint32_t sizeType) {
					return sizeType != 0 ? MaxMemoryBlockSize : MaxMemoryBlockSize / 2;
				}

				static uint8_t GetMemoryBlockSizeType(uint32_t sizeBytes) {
					return (uint8_t)(sizeBytes > MaxMemoryBlockSize / 2);
				}

				/*!
				Allocates memory
				*/
				void* Allocate(uint32_t bytesWanted) {
					GAIA_ASSERT(bytesWanted <= MaxMemoryBlockSize);

					void* pChunk = nullptr;

					const auto sizeType = GetMemoryBlockSizeType(bytesWanted);
					auto& container = m_pages[sizeType];
					if (container.pagesFree.empty()) {
						// Initial allocation
						auto* pPage = AllocPage(sizeType);
						pChunk = pPage->AllocChunk();
						container.pagesFree.push_back(pPage);
					} else {
						auto* pPage = container.pagesFree[0];
						GAIA_ASSERT(!pPage->IsFull());
						// Allocate a new chunk of memory
						pChunk = pPage->AllocChunk();

						// Handle full pages
						if (pPage->IsFull()) {
							// Remove the page from the open list and update the swapped page's pointer
							core::erase_fast(container.pagesFree, 0);
							if (!container.pagesFree.empty())
								container.pagesFree[0]->m_pageIdx = 0;

							// Move our page to the full list
							pPage->m_pageIdx = (uint32_t)container.pagesFull.size();
							container.pagesFull.push_back(pPage);
						}
					}

					return pChunk;
				}

				/*!
				Releases memory allocated for pointer
				*/
				void Release(void* pChunk) {
					// Decode the page from the address
					const auto pageAddr = *(uintptr_t*)((uint8_t*)pChunk - MemoryBlockUsableOffset);
					auto* pPage = (MemoryPage*)pageAddr;

					auto releaseChunk = [&](MemoryPageContainer& container) {
						const bool pageFull = pPage->IsFull();

#if GAIA_DEBUG
						if (pageFull) {
							[[maybe_unused]] auto it =
									core::find_if(container.pagesFull.begin(), container.pagesFull.end(), [&](auto page) {
										return page == pPage;
									});
							GAIA_ASSERT(
									it != container.pagesFull.end() && "ChunkAllocator delete couldn't find the memory page expected "
																										 "in the full pages cnt::list");
						} else {
							[[maybe_unused]] auto it =
									core::find_if(container.pagesFree.begin(), container.pagesFree.end(), [&](auto page) {
										return page == pPage;
									});
							GAIA_ASSERT(
									it != container.pagesFree.end() && "ChunkAllocator delete couldn't find memory page expected in "
																										 "the free pages cnt::list");
						}
#endif

						// Update lists
						if (pageFull) {
							// Our page is no longer full. Remove it from the list and update the swapped page's pointer
							container.pagesFull.back()->m_pageIdx = pPage->m_pageIdx;
							core::erase_fast(container.pagesFull, pPage->m_pageIdx);

							// Move our page to the open list
							pPage->m_pageIdx = (uint32_t)container.pagesFree.size();
							container.pagesFree.push_back(pPage);
						}

						// Free the chunk
						pPage->FreeChunk(pChunk);

						if (m_isDone) {
							// Remove the page right away
							if (pPage->IsEmpty()) {
								GAIA_ASSERT(!container.pagesFree.empty());
								container.pagesFree.back()->m_pageIdx = pPage->m_pageIdx;
								core::erase_fast(container.pagesFree, pPage->m_pageIdx);
							}

							// When there is nothing left, delete the allocator
							if (container.pagesFree.empty() && container.pagesFull.empty())
								delete this;
						}
					};

					releaseChunk(m_pages[pPage->m_sizeType]);
				}

				/*!
				Returns allocator statistics
				*/
				ChunkAllocatorStats GetStats() const {
					ChunkAllocatorStats stats;
					stats.stats[0] = GetPageStats(0);
					stats.stats[1] = GetPageStats(1);
					return stats;
				}

				/*!
				Flushes unused memory
				*/
				void Flush() {
					auto flushPages = [](MemoryPageContainer& container) {
						for (uint32_t i = 0; i < container.pagesFree.size();) {
							auto* pPage = container.pagesFree[i];

							// Skip non-empty pages
							if (!pPage->IsEmpty()) {
								++i;
								continue;
							}

							core::erase_fast(container.pagesFree, i);
							FreePage(pPage);
							if (!container.pagesFree.empty())
								container.pagesFree[i]->m_pageIdx = (uint32_t)i;
						}
					};

					for (auto& c: m_pages)
						flushPages(c);
				}

				/*!
				Performs diagnostics of the memory used.
				*/
				void Diag() const {
					auto diagPage = [](const ChunkAllocatorPageStats& memstats) {
						GAIA_LOG_N("ChunkAllocator stats");
						GAIA_LOG_N("  Allocated: %" PRIu64 " B", memstats.AllocatedMemory);
						GAIA_LOG_N("  Used: %" PRIu64 " B", memstats.AllocatedMemory - memstats.UsedMemory);
						GAIA_LOG_N("  Overhead: %" PRIu64 " B", memstats.UsedMemory);
						GAIA_LOG_N(
								"  Utilization: %.1f%%", 100.0 * ((double)memstats.UsedMemory / (double)memstats.AllocatedMemory));
						GAIA_LOG_N("  Pages: %u", memstats.NumPages);
						GAIA_LOG_N("  Free pages: %u", memstats.NumFreePages);
					};

					diagPage(GetPageStats(0));
					diagPage(GetPageStats(1));
				}

			private:
				static MemoryPage* AllocPage(uint8_t sizeType) {
					const auto size = GetMemoryBlockSize(sizeType) * MemoryPage::NBlocks;
					auto* pPageData = mem::mem_alloc_alig(size, 16);
					return new MemoryPage(pPageData, sizeType);
				}

				static void FreePage(MemoryPage* page) {
					mem::mem_free_alig(page->m_data);
					delete page;
				}

				void Done() {
					m_isDone = true;
				}

				ChunkAllocatorPageStats GetPageStats(uint32_t sizeType) const {
					ChunkAllocatorPageStats stats{};
					const MemoryPageContainer& container = m_pages[sizeType];
					stats.NumPages = (uint32_t)container.pagesFree.size() + (uint32_t)container.pagesFull.size();
					stats.NumFreePages = (uint32_t)container.pagesFree.size();
					stats.AllocatedMemory = stats.NumPages * (size_t)GetMemoryBlockSize(sizeType) * MemoryPage::NBlocks;
					stats.UsedMemory = container.pagesFull.size() * (size_t)GetMemoryBlockSize(sizeType) * MemoryPage::NBlocks;
					for (auto* page: container.pagesFree)
						stats.UsedMemory += page->GetUsedBlocks() * (size_t)MaxMemoryBlockSize;
					return stats;
				};
			};
		} // namespace detail

	} // namespace ecs
} // namespace gaia
