#pragma once

#include "../config/config.h"

#if defined(__GLIBC__) || defined(__sun) || defined(__CYGWIN__)
	#include <alloca.h>
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
#endif

#include <list>

#include "../config/logging.h"
#include "../utils/sarray.h"
#include "../utils/utility.h"
#include "common.h"

namespace gaia {
	namespace ecs {
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
				static constexpr uint32_t Size = CHUNK_SIZE;

				//! For active block: Index of the block within page.
				//! For passive block: Index of the next free block in the implicit
				//! list.
				uint16_t idx;
			};

			struct MemoryPage {
				static constexpr uint32_t NBlocks = 64;
				static constexpr uint32_t Size = NBlocks * MemoryBlock::Size;
				static constexpr uint16_t InvalidBlockId = (uint16_t)-1;

				// Pointer to data managed by page
				void* m_data;
				//! Implicit list of blocks
				utils::sarray<MemoryBlock, (uint16_t)NBlocks> m_blocks;
				//! Numer of used blocks out of NBlocks
				uint16_t m_usedBlocks;
				//! Index of the next block to recycle
				uint16_t m_nextFreeBlock;
				//! Number of blocks to recycle
				uint16_t m_freeBlocks;

				MemoryPage(void* ptr):
						m_data(ptr), m_usedBlocks(0), m_nextFreeBlock(0), m_freeBlocks(0) {}

				[[nodiscard]] void* AllocChunk() {
					if (!m_freeBlocks) {
						// We don't want to go out of range for new blocks
						GAIA_ASSERT(!IsFull() && "Trying to allocate too many blocks!");

						++m_usedBlocks;

						const uint16_t index = m_blocks.size();
						m_blocks.push_back({m_blocks.size()});
						return (void*)((uint8_t*)m_data + index * MemoryBlock::Size);
					} else {
						GAIA_ASSERT(
								m_nextFreeBlock < m_blocks.size() &&
								"Block allocator recycle list broken!");

						++m_usedBlocks;
						--m_freeBlocks;

						const uint32_t index = m_nextFreeBlock;
						m_nextFreeBlock = m_blocks[m_nextFreeBlock].idx;
						return (void*)((uint8_t*)m_data + index * MemoryBlock::Size);
					}
				}

				void FreeChunk(void* chunk) {
					GAIA_ASSERT(m_freeBlocks <= NBlocks);
					const auto chunkAddr = (uintptr_t)chunk;
					const auto dataAddr = (uintptr_t)m_data;
					GAIA_ASSERT(
							chunkAddr >= dataAddr && chunkAddr < dataAddr + MemoryPage::Size);
					MemoryBlock block = {
							uint16_t((chunkAddr - dataAddr) / MemoryBlock::Size)};

					auto& blockContainer = m_blocks[block.idx];

					// Update our implicit list
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
			std::list<MemoryPage*> m_pagesFree;
			//! List of full pages
			std::list<MemoryPage*> m_pagesFull;
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
				void* chunk;

				auto itFree = m_pagesFree.begin();
				if (itFree == m_pagesFree.end()) {
					// Initial allocation
					auto page = AllocPage();
					m_pagesFree.push_back(page);
					chunk = page->AllocChunk();
				} else {
					auto page = *itFree;
					// Later allocation
					chunk = page->AllocChunk();
					if (page->IsFull()) {
						// If our page is full move it to a different list
						m_pagesFull.push_back(page);
						m_pagesFree.erase(itFree);
					}
				}

#if 0
				 // Fill allocated memory by 0xbaadf00d as MSVC does
				utils::fill_array(
						(uint32_t*)chunk, (uint32_t)((CHUNK_SIZE + 3) / sizeof(uint32_t)),
						0x7fcdf00dU);
#endif

				return chunk;
			}

			/*!
			Releases memory allocated for pointer
			*/
			void Release(void* chunk) {
#if 0
				 // Fill freed memory by 0xfeeefeee as MSVC does
				utils::fill_array(
						(uint32_t*)chunk, (int)(CHUNK_SIZE / sizeof(uint32_t)),
						0xfeeefeeeU);
#endif

				auto belongsToPage = [](void* mem, MemoryPage* page) {
					const auto chunkAddr = (uintptr_t)mem;
					const auto pageAddr = (uintptr_t)page->m_data;
					return chunkAddr >= pageAddr &&
								 chunkAddr < pageAddr + MemoryPage::Size;
				};

				// Search in the list of free pages
				{
					auto it = std::find_if(
							m_pagesFree.begin(), m_pagesFree.end(),
							[&](auto page) { return belongsToPage(chunk, page); });
					if (it != m_pagesFree.end()) {
						auto page = *it;
						page->FreeChunk(chunk);
						return;
					}
				}

				// Search in the list of full pages
				{
					auto it = std::find_if(
							m_pagesFull.begin(), m_pagesFull.end(),
							[&](auto page) { return belongsToPage(chunk, page); });
					if (it != m_pagesFull.end()) {
						auto page = *it;
						page->FreeChunk(chunk);
						m_pagesFree.push_back(page);
						m_pagesFull.erase(it);
						return;
					}
				}

				GAIA_ASSERT(
						false &&
						"ChunkAllocator delete request couldn't find the request memory");
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
				stats.NumPages =
						(uint32_t)m_pagesFree.size() + (uint32_t)m_pagesFull.size();
				stats.NumFreePages = (uint32_t)m_pagesFree.size();
				stats.AllocatedMemory = stats.NumPages * MemoryPage::Size;
				stats.UsedMemory = m_pagesFull.size() * MemoryPage::Size;
				for (auto* page: m_pagesFree)
					stats.UsedMemory += page->GetUsedBlocks() * MemoryBlock::Size;
			}

			/*!
			Flushes unused memory
			*/
			void Flush() {
				auto it = m_pagesFree.begin();
				while (it != m_pagesFree.end()) {
					auto page = *it;

					// Skip non-empty pages
					if (!page->IsEmpty()) {
						++it;
						continue;
					}

					// Page with no chunks needs to be freed
					FreePage(page);
					m_pagesFree.erase(it);
					++it;
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
