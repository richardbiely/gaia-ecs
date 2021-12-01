#pragma once

#include "../config/config.h"
#include "gaia/utils/utils_containers.h"

#if defined(__GLIBC__) || defined(__sun) || defined(__CYGWIN__)
	#include <alloca.h>
	#if !defined(aligned_free)
		#define aligned_free free
	#endif
#elif defined(_WIN32)
	#include <malloc.h>
	// Clang with MSVC codegen needes some remapping
	#if !defined(alloca)
		#define alloca _alloca
	#endif
	#if !defined(aligned_alloc)
		#define aligned_alloc(alig, size) _aligned_malloc(size, alig)
	#endif
	#if !defined(aligned_free)
		#define aligned_free _aligned_free
	#endif
#else
	#include <cstdlib>
	#if !defined(aligned_free)
		#define aligned_free free
	#endif
#endif

#include "../containers/darray.h"

#include "../config/logging.h"
#include "../containers/sarray_ext.h"
#include "../utils/utility.h"
#if GAIA_ECS_CHUNK_ALLOCATOR_CLEAN_MEMORY_WITH_GARBAGE
	#include "../utils/utils_mem.h"
#endif
#include "common.h"

namespace gaia {
	namespace ecs {
		static constexpr uint32_t MemoryBlockSize = 16384;
		static constexpr uint32_t MemoryBlockUsableOffset = 16;
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
				//! For active block: Index of the block within page.
				//! For passive block: Index of the next free block in the implicit list.
				uint16_t idx;
			};

			struct MemoryPage {
				static constexpr uint32_t NBlocks = 64;
				static constexpr uint32_t Size = NBlocks * MemoryBlockSize;
				static constexpr uint16_t InvalidBlockId = (uint16_t)-1;
				using iterator = containers::darray<MemoryPage*>::iterator;

				//! Pointer to data managed by page
				void* m_data;
				//! Implicit list of blocks
				containers::sarray_ext<MemoryBlock, (uint16_t)NBlocks> m_blocks;
				//! Index in the list of pages
				uint32_t m_idx;
				//! Number of used blocks out of NBlocks
				uint16_t m_usedBlocks;
				//! Index of the next block to recycle
				uint16_t m_nextFreeBlock;
				//! Number of blocks to recycle
				uint16_t m_freeBlocks;

				MemoryPage(void* ptr): m_data(ptr), m_usedBlocks(0), m_nextFreeBlock(0), m_freeBlocks(0) {}

				[[nodiscard]] void* AllocChunk() {
					if (!m_freeBlocks) {
						// We don't want to go out of range for new blocks
						GAIA_ASSERT(!IsFull() && "Trying to allocate too many blocks!");

						++m_usedBlocks;

						const uint16_t index = m_blocks.size();
						m_blocks.push_back({m_blocks.size()});

						// Encode info about chunk's page in the memory block.
						// The actual pointer returned is offset by UsableOffset bytes
						uint8_t* pMemoryBlock = (uint8_t*)m_data + index * MemoryBlockSize;
						*(uintptr_t*)pMemoryBlock = (uintptr_t)this;
						return (void*)(pMemoryBlock + MemoryBlockUsableOffset);
					} else {
						GAIA_ASSERT(m_nextFreeBlock < m_blocks.size() && "Block allocator recycle containers::list broken!");

						++m_usedBlocks;
						--m_freeBlocks;

						const uint32_t index = m_nextFreeBlock;
						m_nextFreeBlock = m_blocks[m_nextFreeBlock].idx;

						// Encode info about chunk's page in the memory block.
						// The actual pointer returned is offset by UsableOffset bytes
						uint8_t* pMemoryBlock = (uint8_t*)m_data + index * MemoryBlockSize;
						*(uintptr_t*)pMemoryBlock = (uintptr_t)this;
						return (void*)(pMemoryBlock + MemoryBlockUsableOffset);
					}
				}

				void FreeChunk(void* chunk) {
					GAIA_ASSERT(m_freeBlocks <= NBlocks);

					// Offset the chunk memory so we get the real block address
					const uint8_t* pMemoryBlock = (uint8_t*)chunk - MemoryBlockUsableOffset;

					const auto blckAddr = (uintptr_t)pMemoryBlock;
					const auto dataAddr = (uintptr_t)m_data;
					GAIA_ASSERT(blckAddr >= dataAddr && blckAddr < dataAddr + MemoryPage::Size);
					MemoryBlock block = {uint16_t((blckAddr - dataAddr) / MemoryBlockSize)};

					auto& blockContainer = m_blocks[block.idx];

					// Update our implicit containers::list
					if (!m_freeBlocks) {
						m_nextFreeBlock = block.idx;
						blockContainer.idx = InvalidBlockId;
					} else {
						blockContainer.idx = m_nextFreeBlock;
						m_nextFreeBlock = block.idx;
					}

					++m_freeBlocks;
					--m_usedBlocks;
				}

				[[nodiscard]] uint32_t GetUsedBlocks() const {
					return m_usedBlocks;
				}
				[[nodiscard]] bool IsFull() const {
					return m_usedBlocks == NBlocks;
				}
				[[nodiscard]] bool IsEmpty() const {
					return m_usedBlocks == 0;
				}
			};

			//! List of available pages
			containers::darray<MemoryPage*> m_pagesFree;
			//! List of full pages
			containers::darray<MemoryPage*> m_pagesFull;
			//! Allocator statistics
			ChunkAllocatorStats m_stats;

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
					auto pPage = AllocPage();
					m_pagesFree.push_back(pPage);
					pPage->m_idx = m_pagesFree.size() - 1;
					pChunk = pPage->AllocChunk();
				} else {
					auto pPage = m_pagesFree[0];
					GAIA_ASSERT(!pPage->IsFull());
					// Allocate a new chunk
					pChunk = pPage->AllocChunk();

					// Handle full pages
					if (pPage->IsFull()) {
						// Remove the page from the open list and update the swapped page's pointer
						utils::erase_fast(m_pagesFree, 0);
						if (!m_pagesFree.empty())
							m_pagesFree[0]->m_idx = 0;

						// Move our page to full list
						m_pagesFull.push_back(pPage);
						pPage->m_idx = m_pagesFull.size() - 1;
					}
				}

#if GAIA_ECS_CHUNK_ALLOCATOR_CLEAN_MEMORY_WITH_GARBAGE
				// Fill allocated memory with 0xbaadf00d.
				// This way we always know if we treat the memory correctly.
				utils::fill_array((uint32_t*)pChunk, (uint32_t)((ChunkMemorySize + 3) / sizeof(uint32_t)), 0x7fcdf00dU);
#endif

				return pChunk;
			}

			/*!
			Releases memory allocated for pointer
			*/
			void Release(void* chunk) {
				// Decode the page from the address
				uintptr_t pageAddr = *(uintptr_t*)((uint8_t*)chunk - MemoryBlockUsableOffset);
				auto* pPage = (MemoryPage*)pageAddr;

#if GAIA_ECS_CHUNK_ALLOCATOR_CLEAN_MEMORY_WITH_GARBAGE
				// Fill freed memory with 0xfeeefeee.
				// This way we always know if we treat the memory correctly.
				utils::fill_array((uint32_t*)chunk, (int)(ChunkMemorySize / sizeof(uint32_t)), 0xfeeefeeeU);
#endif

				const bool pageFull = pPage->IsFull();

#if GAIA_DEBUG
				if (pageFull) {
					[[maybe_unused]] auto it = std::find_if(m_pagesFull.begin(), m_pagesFull.end(), [&](auto page) {
						return page == pPage;
					});
					GAIA_ASSERT(
							it != m_pagesFull.end() && "ChunkAllocator delete couldn't find the memory page expected "
																				 "in the full pages containers::list");
				} else {
					[[maybe_unused]] auto it = std::find_if(m_pagesFree.begin(), m_pagesFree.end(), [&](auto page) {
						return page == pPage;
					});
					GAIA_ASSERT(
							it != m_pagesFree.end() && "ChunkAllocator delete couldn't find memory page expected in "
																				 "the free pages containers::list");
				}
#endif

				// Update lists
				if (pageFull) {
					// Our page is no longer full remove it from the list and update the swapped page's pointer
					utils::erase_fast(m_pagesFull, pPage->m_idx);
					if (!m_pagesFull.empty())
						m_pagesFull[pPage->m_idx]->m_idx = pPage->m_idx;

					// Move our page the the open list
					m_pagesFree.push_back(pPage);
					pPage->m_idx = m_pagesFree.size() - 1;
				}

				// Free the chunk
				pPage->FreeChunk(chunk);
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

				m_pagesFree.clear();
				m_pagesFull.clear();
				m_stats = {};
			}

			/*!
			Returns allocator statistics
			*/
			void GetStats(ChunkAllocatorStats& stats) const {
				stats.NumPages = (uint32_t)m_pagesFree.size() + (uint32_t)m_pagesFull.size();
				stats.NumFreePages = (uint32_t)m_pagesFree.size();
				stats.AllocatedMemory = stats.NumPages * MemoryPage::Size;
				stats.UsedMemory = m_pagesFull.size() * MemoryPage::Size;
				for (auto* page: m_pagesFree)
					stats.UsedMemory += page->GetUsedBlocks() * MemoryBlockSize;
			}

			/*!
			Flushes unused memory
			*/
			void Flush() {
				for (size_t i = 0; i < m_pagesFree.size();) {
					auto page = m_pagesFree[i];

					// Skip non-empty pages
					if (!page->IsEmpty()) {
						++i;
						continue;
					}

					utils::erase_fast(m_pagesFree, i);
					FreePage(page);
					if (!m_pagesFree.empty())
						m_pagesFree[i]->m_idx = i;
				}
			}

		private:
			MemoryPage* AllocPage() {
				auto* pageData = (uint8_t*)aligned_alloc(16, MemoryPage::Size);
				return new MemoryPage(pageData);
			}

			void FreePage(MemoryPage* page) {
				aligned_free(page->m_data);
				delete page;
			}
		};

	} // namespace ecs
} // namespace gaia
