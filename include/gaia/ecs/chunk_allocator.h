#pragma once

#include <cstdint>

#include "../utils/span.h"

#if defined(__GLIBC__) || defined(__sun) || defined(__CYGWIN__)
	#include <alloca.h>
	#define GAIA_ALIGNED_ALLOC(alig, size) aligned_alloc(alig, size)
	#if !defined(aligned_free)
		#define GAIA_ALIGNED_FREE free
	#else
		#define GAIA_ALIGNED_FREE aligned_free
	#endif
#elif defined(_WIN32)
	#include <malloc.h>
	// Clang with MSVC codegen needs some remapping
	#if !defined(aligned_alloc)
		#define GAIA_ALIGNED_ALLOC(alig, size) _aligned_malloc(size, alig)
	#else
		#define GAIA_ALIGNED_ALLOC(alig, size) aligned_alloc(alig, size)
	#endif
	#if !defined(aligned_free)
		#define GAIA_ALIGNED_FREE _aligned_free
	#else
		#define GAIA_ALIGNED_FREE aligned_free
	#endif
#else
	#define GAIA_ALIGNED_ALLOC(alig, size) aligned_alloc(alig, size)
	#if !defined(aligned_free)
		#define GAIA_ALIGNED_FREE free
	#else
		#define GAIA_ALIGNED_FREE aligned_free
	#endif
#endif

#include "../config/config.h"
#include "../config/logging.h"
#include "../containers/darray.h"
#include "../containers/sarray_ext.h"
#include "../utils/containers.h"
#include "../utils/utility.h"

#if GAIA_ECS_CHUNK_ALLOCATOR_CLEAN_MEMORY_WITH_GARBAGE
	#include "../utils/mem.h"
#endif
#include "common.h"

namespace gaia {
	namespace ecs {
		static constexpr uint32_t MemoryBlockSize = 16384;
		// TODO: For component aligned to 64 bytes the offset of 16 is not enough for some reason, figure it out
		static constexpr uint32_t MemoryBlockUsableOffset = 32;
		static constexpr uint32_t ChunkMemorySize = MemoryBlockSize - MemoryBlockUsableOffset;

		struct ChunkAllocatorStats final {
			//! Total allocated memory
			uint64_t AllocatedMemory;
			//! Memory actively used
			uint64_t UsedMemory;
			//! Number of allocated pages
			uint32_t NumPages;
			//! Number of free pages
			uint32_t NumFreePages;
		};

		/*!
		Allocator for ECS Chunks. Memory is organized in pages of chunks.
		*/
		class ChunkAllocator {
			struct MemoryBlock {
				using MemoryBlockType = uint8_t;

				//! Active block : Index of the block within the page.
				//! Passive block: Index of the next free block in the implicit list.
				MemoryBlockType idx;
			};

			struct MemoryPage {
				static constexpr uint16_t NBlocks = 64;
				static constexpr uint32_t Size = NBlocks * MemoryBlockSize;
				static constexpr MemoryBlock::MemoryBlockType InvalidBlockId = (MemoryBlock::MemoryBlockType)-1;
				static constexpr uint32_t MemoryLockTypeSizeInBits = utils::as_bits(sizeof(MemoryBlock::MemoryBlockType));
				static_assert((uint32_t)NBlocks < (1 << MemoryLockTypeSizeInBits));
				using iterator = containers::darray<MemoryPage*>::iterator;

				//! Pointer to data managed by page
				void* m_data;
				//! Implicit list of blocks
				MemoryBlock m_blocks[NBlocks]{};
				//! Index in the list of pages
				uint32_t m_pageIdx;
				//! Number of blocks in the block array
				MemoryBlock::MemoryBlockType m_blockCnt;
				//! Number of used blocks out of NBlocks
				MemoryBlock::MemoryBlockType m_usedBlocks;
				//! Index of the next block to recycle
				MemoryBlock::MemoryBlockType m_nextFreeBlock;
				//! Number of blocks to recycle
				MemoryBlock::MemoryBlockType m_freeBlocks;

				MemoryPage(void* ptr):
						m_data(ptr), m_pageIdx(0), m_blockCnt(0), m_usedBlocks(0), m_nextFreeBlock(0), m_freeBlocks(0) {}

				GAIA_NODISCARD void* AllocChunk() {
					auto GetChunkAddress = [&](size_t index) {
						// Encode info about chunk's page in the memory block.
						// The actual pointer returned is offset by UsableOffset bytes
						uint8_t* pMemoryBlock = (uint8_t*)m_data + index * MemoryBlockSize;
						*(uintptr_t*)pMemoryBlock = (uintptr_t)this;
						return (void*)(pMemoryBlock + MemoryBlockUsableOffset);
					};

					if (m_freeBlocks == 0U) {
						// We don't want to go out of range for new blocks
						GAIA_ASSERT(!IsFull() && "Trying to allocate too many blocks!");

						++m_usedBlocks;

						const size_t index = m_blockCnt;
						GAIA_ASSERT(index < NBlocks);
						m_blocks[index].idx = (MemoryBlock::MemoryBlockType)index;
						++m_blockCnt;

						return GetChunkAddress(index);
					}

					GAIA_ASSERT(m_nextFreeBlock < m_blockCnt && "Block allocator recycle containers::list broken!");

					++m_usedBlocks;
					--m_freeBlocks;

					const size_t index = m_nextFreeBlock;
					m_nextFreeBlock = m_blocks[m_nextFreeBlock].idx;

					return GetChunkAddress(index);
				}

				void FreeChunk(void* pChunk) {
					GAIA_ASSERT(m_freeBlocks <= NBlocks);

					// Offset the chunk memory so we get the real block address
					const auto* pMemoryBlock = (uint8_t*)pChunk - MemoryBlockUsableOffset;

					const auto blckAddr = (uintptr_t)pMemoryBlock;
					const auto dataAddr = (uintptr_t)m_data;
					GAIA_ASSERT(blckAddr >= dataAddr && blckAddr < dataAddr + MemoryPage::Size);
					MemoryBlock block = {MemoryBlock::MemoryBlockType((blckAddr - dataAddr) / MemoryBlockSize)};

					auto& blockContainer = m_blocks[block.idx];

					// Update our implicit containers::list
					if (m_freeBlocks == 0U) {
						blockContainer.idx = InvalidBlockId;
						m_nextFreeBlock = block.idx;
					} else {
						blockContainer.idx = m_nextFreeBlock;
						m_nextFreeBlock = block.idx;
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

			//! List of available pages
			//! Note, this currently only contains at most 1 item
			containers::darray<MemoryPage*> m_pagesFree;
			//! List of full pages
			containers::darray<MemoryPage*> m_pagesFull;
			//! Allocator statistics
			ChunkAllocatorStats m_stats{};

		public:
			~ChunkAllocator() {
				FreeAll();
			}

			/*!
			Allocates memory
			*/
			void* Allocate() {
				void* pChunk = nullptr;

				if (m_pagesFree.empty()) {
					// Initial allocation
					auto* pPage = AllocPage();
					m_pagesFree.push_back(pPage);
					pPage->m_pageIdx = (uint32_t)m_pagesFree.size() - 1;
					pChunk = pPage->AllocChunk();
				} else {
					auto* pPage = m_pagesFree[0];
					GAIA_ASSERT(!pPage->IsFull());
					// Allocate a new chunk
					pChunk = pPage->AllocChunk();

					// Handle full pages
					if (pPage->IsFull()) {
						// Remove the page from the open list and update the swapped page's pointer
						utils::erase_fast(m_pagesFree, 0);
						if (!m_pagesFree.empty())
							m_pagesFree[0]->m_pageIdx = 0;

						// Move our page to the full list
						m_pagesFull.push_back(pPage);
						pPage->m_pageIdx = (uint32_t)m_pagesFull.size() - 1;
					}
				}

#if GAIA_ECS_CHUNK_ALLOCATOR_CLEAN_MEMORY_WITH_GARBAGE
				// Fill allocated memory with garbage.
				// This way we always know if we treat the memory correctly.
				constexpr uint32_t AllocMemDefValue = 0x7fcdf00dU;
				constexpr uint32_t AllocSpanSize = (uint32_t)((ChunkMemorySize + 3) / sizeof(uint32_t));
				std::span<uint32_t, AllocSpanSize> s((uint32_t*)pChunk, AllocSpanSize);
				for (auto& val: s)
					val = AllocMemDefValue;
#endif

				return pChunk;
			}

			/*!
			Releases memory allocated for pointer
			*/
			void Release(void* pChunk) {
				// Decode the page from the address
				uintptr_t pageAddr = *(uintptr_t*)((uint8_t*)pChunk - MemoryBlockUsableOffset);
				auto* pPage = (MemoryPage*)pageAddr;

#if GAIA_ECS_CHUNK_ALLOCATOR_CLEAN_MEMORY_WITH_GARBAGE
				// Fill freed memory with garbage.
				// This way we always know if we treat the memory correctly.
				constexpr uint32_t FreedMemDefValue = 0xfeeefeeeU;
				constexpr uint32_t FreedSpanSize = (uint32_t)((ChunkMemorySize + 3) / sizeof(uint32_t));
				std::span<uint32_t, FreedSpanSize> s((uint32_t*)pChunk, FreedSpanSize);
				for (auto& val: s)
					val = FreedMemDefValue;
#endif

				const bool pageFull = pPage->IsFull();

#if GAIA_DEBUG
				if (pageFull) {
					[[maybe_unused]] auto it = utils::find_if(m_pagesFull.begin(), m_pagesFull.end(), [&](auto page) {
						return page == pPage;
					});
					GAIA_ASSERT(
							it != m_pagesFull.end() && "ChunkAllocator delete couldn't find the memory page expected "
																				 "in the full pages containers::list");
				} else {
					[[maybe_unused]] auto it = utils::find_if(m_pagesFree.begin(), m_pagesFree.end(), [&](auto page) {
						return page == pPage;
					});
					GAIA_ASSERT(
							it != m_pagesFree.end() && "ChunkAllocator delete couldn't find memory page expected in "
																				 "the free pages containers::list");
				}
#endif

				// Update lists
				if (pageFull) {
					// Our page is no longer full. Remove it from the list and update the swapped page's pointer
					if (m_pagesFull.size() > 1)
						m_pagesFull.back()->m_pageIdx = pPage->m_pageIdx;
					utils::erase_fast(m_pagesFull, pPage->m_pageIdx);

					// Move our page to the open list
					pPage->m_pageIdx = (uint32_t)m_pagesFree.size();
					m_pagesFree.push_back(pPage);
				}

				// Free the chunk
				pPage->FreeChunk(pChunk);
			}

			/*!
			Releases all allocated memory
			*/
			void FreeAll() {
				// Release free pages
				for (auto* page: m_pagesFree)
					FreePage(page);
				// Release full pages
				for (auto* page: m_pagesFull)
					FreePage(page);

				m_pagesFree = {};
				m_pagesFull = {};
				m_stats = {};
			}

			/*!
			Returns allocator statistics
			*/
			ChunkAllocatorStats GetStats() const {
				ChunkAllocatorStats stats{};
				stats.NumPages = (uint32_t)m_pagesFree.size() + (uint32_t)m_pagesFull.size();
				stats.NumFreePages = (uint32_t)m_pagesFree.size();
				stats.AllocatedMemory = stats.NumPages * (size_t)MemoryPage::Size;
				stats.UsedMemory = m_pagesFull.size() * (size_t)MemoryPage::Size;
				for (auto* page: m_pagesFree)
					stats.UsedMemory += page->GetUsedBlocks() * (size_t)MemoryBlockSize;
				return stats;
			}

			/*!
			Flushes unused memory
			*/
			void Flush() {
				for (size_t i = 0; i < m_pagesFree.size();) {
					auto* pPage = m_pagesFree[i];

					// Skip non-empty pages
					if (!pPage->IsEmpty()) {
						++i;
						continue;
					}

					utils::erase_fast(m_pagesFree, i);
					FreePage(pPage);
					if (!m_pagesFree.empty())
						m_pagesFree[i]->m_pageIdx = (uint32_t)i;
				}
			}

		private:
			static MemoryPage* AllocPage() {
				auto* pageData = GAIA_ALIGNED_ALLOC(16, MemoryPage::Size);
				return new MemoryPage(pageData);
			}

			static void FreePage(MemoryPage* page) {
				GAIA_ALIGNED_FREE(page->m_data);
				delete page;
			}
		};

	} // namespace ecs
} // namespace gaia
