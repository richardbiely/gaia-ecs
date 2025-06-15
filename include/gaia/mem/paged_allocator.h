#pragma once
#include "../config/config.h"
#include "../config/logging.h"

#include <cinttypes>
#include <cstdint>
#include <type_traits>

#include "../cnt/fwd_llist.h"
#include "../cnt/sarray.h"
#include "../core/bit_utils.h"
#include "../core/dyn_singleton.h"
#include "../core/utility.h"
#include "../meta/type_info.h"
#include "mem_alloc.h"

namespace gaia {
	namespace mem {
		static constexpr uint32_t MemoryBlockAlignment = 16;
		//! Size in bytes of one memory block.
		//! 32 kiB per block by default.
		static constexpr uint32_t MemoryBlockBytesDefault = 32768;
		//! Unusable area at the beginning of the allocated block designated for special purposes
		static constexpr uint32_t MemoryBlockUsableOffset = sizeof(uintptr_t);

		struct MemoryPageHeader {
			//! Pointer to data managed by page
			void* m_data;

			MemoryPageHeader(void* ptr): m_data(ptr) {}
		};

		template <typename T, uint32_t RequestedBlockSize>
		struct MemoryPage: MemoryPageHeader, cnt::fwd_llist_base<MemoryPage<T, RequestedBlockSize>> {
			static constexpr uint32_t next_multiple_of_alignment(uint32_t num) {
				return (num + (MemoryBlockAlignment - 1)) & uint32_t(-(int32_t)MemoryBlockAlignment);
			}
			static constexpr uint32_t calculate_block_size() {
				if constexpr (RequestedBlockSize == 0)
					return next_multiple_of_alignment(MemoryBlockBytesDefault);
				else
					return next_multiple_of_alignment(RequestedBlockSize);
			}

			static constexpr uint32_t MemoryBlockBytes = calculate_block_size();
			static constexpr uint16_t NBlocks = 48;
			static constexpr uint16_t NBlocks_Bits = (uint16_t)core::count_bits(NBlocks);
			static constexpr uint32_t InvalidBlockId = NBlocks + 1;
			static constexpr uint32_t BlockArrayBytes = ((uint32_t)NBlocks_Bits * (uint32_t)NBlocks + 7) / 8;

			using Page = MemoryPage<T, RequestedBlockSize>;
			using BlockArray = cnt::sarray<uint8_t, BlockArrayBytes>;
			using BitView = core::bit_view<NBlocks_Bits>;

			//! Implicit list of blocks
			BlockArray m_blocks;

			//! Number of blocks in the block array
			uint32_t m_blockCnt: NBlocks_Bits;
			//! Number of used blocks out of NBlocks
			uint32_t m_usedBlocks: NBlocks_Bits;
			//! Index of the next block to recycle
			uint32_t m_nextFreeBlock: NBlocks_Bits;
			//! Number of blocks to recycle
			uint32_t m_freeBlocks: NBlocks_Bits;
			//! Free bits to use in the future
			// uint32_t m_unused : 8;

			MemoryPage(void* ptr):
					MemoryPageHeader(ptr), m_blockCnt(0), m_usedBlocks(0), m_nextFreeBlock(0), m_freeBlocks(0) {
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

			//! Allocate a new block for this page.
			GAIA_NODISCARD void* alloc_block() {
				auto StoreBlockAddress = [&](uint32_t index) {
					// Encode info about the block's page in the memory block.
					// The actual pointer returned is offset by MemoryBlockUsableOffset bytes
					uint8_t* pMemoryBlock = (uint8_t*)m_data + index * MemoryBlockBytes;
					GAIA_ASSERT((uintptr_t)pMemoryBlock % MemoryBlockAlignment == 0);
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

			//! Release the block allocated by this page.
			void free_block(void* pBlock) {
				GAIA_ASSERT(m_usedBlocks > 0);
				GAIA_ASSERT(m_freeBlocks <= NBlocks);

				auto ReadBlockAddress = [&](void* pMemory) {
					// Offset the chunk memory so we get the real block address
					const auto* pMemoryBlock = (uint8_t*)pMemory - MemoryBlockUsableOffset;
					// Page pointer is written to the start of the memory block
					[[maybe_unused]] const auto* pPage = (Page**)pMemoryBlock;
					GAIA_ASSERT(*pPage == this);
					const auto blckAddr = (uintptr_t)pMemoryBlock;
					GAIA_ASSERT(blckAddr % 16 == 0);
					const auto dataAddr = (uintptr_t)m_data;
					const auto blockIdx = (uint32_t)((blckAddr - dataAddr) / MemoryBlockBytes);
					return blockIdx;
				};
				const auto blockIdx = ReadBlockAddress(pBlock);

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

		template <typename T, uint32_t RequestedBlockSize>
		struct MemoryPageContainer {
			//! List of available pages
			cnt::fwd_llist<MemoryPage<T, RequestedBlockSize>> pagesFree;
			//! List of full pages
			cnt::fwd_llist<MemoryPage<T, RequestedBlockSize>> pagesFull;

			GAIA_NODISCARD bool empty() const {
				return pagesFree.empty() && pagesFull.empty();
			}
		};

		struct MemoryPageStats final {
			//! Total allocated memory
			uint64_t mem_total;
			//! Memory actively used
			uint64_t mem_used;
			//! Number of allocated pages
			uint32_t num_pages;
			//! Number of free pages
			uint32_t num_pages_free;
		};

		namespace detail {
			template <typename T, uint32_t RequestedBlockSize>
			class PagedAllocatorImpl;
		}

		template <typename T, uint32_t RequestedBlockSize = 0>
		using PagedAllocator = core::dyn_singleton<detail::PagedAllocatorImpl<T, RequestedBlockSize>>;

		namespace detail {

			template <typename T, uint32_t RequestedBlockSize>
			class PagedAllocatorImpl {
				friend ::gaia::mem::PagedAllocator<T, RequestedBlockSize>;

				inline static char s_strPageData[256]{};
				inline static char s_strMemPage[256]{};

				using Page = MemoryPage<T, RequestedBlockSize>;
				using PageContainer = MemoryPageContainer<T, RequestedBlockSize>;

				//! Container for pages storing various-sized chunks
				PageContainer m_pages;
				//! When true, destruction has been requested
				bool m_isDone = false;

			private:
				PagedAllocatorImpl() {
					// PagedAllocatorImpl is only used as a singleton so the constructor is going to be called just once.
					// Therefore, the strings are only going to be set once.
					auto ct_name = meta::type_info::name<T>();
					const auto ct_name_len = (uint32_t)ct_name.size();
					GAIA_STRCPY(s_strPageData, 256, "PageData_");
					memcpy((void*)&s_strPageData[9], (const void*)ct_name.data(), ct_name_len);
					s_strPageData[9 + ct_name_len] = 0;
					GAIA_STRCPY(s_strMemPage, 256, "MemPage_");
					memcpy((void*)&s_strMemPage[8], (const void*)ct_name.data(), ct_name_len);
					s_strMemPage[8 + ct_name_len] = 0;
				}

				void on_delete() {
					flush();

					// Make sure there are no leaks
					auto memStats = stats();
					if (memStats.mem_total != 0) {
						GAIA_ASSERT2(false, "Paged allocator leaking memory");
						GAIA_LOG_W("Paged allocator leaking memory!");
						diag();
					}
				}

			public:
				~PagedAllocatorImpl() {
					on_delete();
				}

				PagedAllocatorImpl(PagedAllocatorImpl&& world) = delete;
				PagedAllocatorImpl(const PagedAllocatorImpl& world) = delete;
				PagedAllocatorImpl& operator=(PagedAllocatorImpl&&) = delete;
				PagedAllocatorImpl& operator=(const PagedAllocatorImpl&) = delete;

				//! Allocates memory
				void* alloc([[maybe_unused]] uint32_t dummy) {
					void* pBlock = nullptr;

					// Find first page with available space
					auto* pPage = m_pages.pagesFree.first;
					GAIA_ASSERT(pPage == nullptr || !pPage->full());
					if (pPage == nullptr) {
						// Allocate a new page if no free page was found
						pPage = alloc_page();
						m_pages.pagesFree.link(pPage);
					}

					// Allocate a new chunk of memory
					pBlock = pPage->alloc_block();

					// Handle full pages
					if (pPage->full()) {
						// Remove the page from the open list
						m_pages.pagesFree.unlink(pPage);
						// Move our page to the full list
						m_pages.pagesFull.link(pPage);
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
					GAIA_ASSERT(pageAddr % MemoryBlockAlignment == 0);
					auto* pPage = (Page*)pageAddr;
					const bool wasFull = pPage->full();

#if GAIA_ASSERT_ENABLED
					if (wasFull) {
						const auto res = m_pages.pagesFull.has(pPage);
						GAIA_ASSERT(res && "Memory page couldn't be found among full pages");
					} else {
						const auto res = m_pages.pagesFree.has(pPage);
						GAIA_ASSERT(res && "Memory page couldn't be found among free pages");
					}
#endif

					// Free the chunk
					pPage->free_block(pBlock);

					// Update lists
					if (wasFull) {
						// Our page is no longer full
						m_pages.pagesFull.unlink(pPage);
						// Move our page to the open list
						m_pages.pagesFree.link(pPage);
					}

					// Special handling for the allocator signaled to destroy itself
					if (m_isDone) {
						// Remove the page right away
						if (pPage->empty()) {
							GAIA_ASSERT(!m_pages.pagesFree.empty());
							m_pages.pagesFree.unlink(pPage);
						}

						try_delete_this();
					}
				}

				GAIA_CLANG_WARNING_POP()

				//! Returns allocator statistics
				MemoryPageStats stats() const {
					MemoryPageStats stats{};

					stats.num_pages = (uint32_t)m_pages.pagesFree.size() + (uint32_t)m_pages.pagesFull.size();
					stats.num_pages_free = (uint32_t)m_pages.pagesFree.size();
					stats.mem_total = stats.num_pages * (size_t)Page::MemoryBlockBytes * Page::NBlocks;
					stats.mem_used = m_pages.pagesFull.size() * (size_t)Page::MemoryBlockBytes * Page::NBlocks;
					for (const auto& page: m_pages.pagesFree)
						stats.mem_used += page.used_blocks_cnt() * (size_t)Page::MemoryBlockBytes;

					return stats;
				}

				//! Flushes unused memory
				void flush() {
					for (auto it = m_pages.pagesFree.begin(); it != m_pages.pagesFree.end();) {
						auto* pPage = &(*it);
						++it;

						// Skip non-empty pages
						if (!pPage->empty())
							continue;

						m_pages.pagesFree.unlink(pPage);
						free_page(pPage);
					}
				}

				//! Performs diagnostics of the memory used.
				void diag() const {
					auto memStats = stats();
					GAIA_LOG_N("PagedAllocator %p stats", (void*)this);
					GAIA_LOG_N("  Allocated: %" PRIu64 " B", memStats.mem_total);
					GAIA_LOG_N("  Used: %" PRIu64 " B", memStats.mem_total - memStats.mem_used);
					GAIA_LOG_N("  Overhead: %" PRIu64 " B", memStats.mem_used);
					GAIA_LOG_N(
							"  Utilization: %.1f%%",
							memStats.mem_total != 0 ? 100.0 * ((double)memStats.mem_used / (double)memStats.mem_total) : 0.0);
					GAIA_LOG_N("  Pages: %u", memStats.num_pages);
					GAIA_LOG_N("  Free pages: %u", memStats.num_pages_free);
				}

			private:
				static Page* alloc_page() {
					const uint32_t size = Page::NBlocks * Page::MemoryBlockBytes;
					auto* pPageData = mem::AllocHelper::alloc_alig<uint8_t>(&s_strPageData[0], MemoryBlockAlignment, size);
					auto* pMemoryPage = mem::AllocHelper::alloc<Page>(&s_strMemPage[0]);
					return new (pMemoryPage) Page(pPageData);
				}

				static void free_page(Page* pMemoryPage) {
					GAIA_ASSERT(pMemoryPage != nullptr);

					mem::AllocHelper::free_alig(&s_strPageData[0], pMemoryPage->m_data);
					pMemoryPage->~MemoryPage();
					mem::AllocHelper::free(&s_strMemPage[0], pMemoryPage);
				}

				void done() {
					m_isDone = true;
				}

				void try_delete_this() {
					// When there is nothing left, delete the allocator
					if (m_pages.empty())
						delete this;
				}
			};

		} // namespace detail
	} // namespace mem
} // namespace gaia
