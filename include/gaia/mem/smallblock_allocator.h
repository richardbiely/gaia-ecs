#pragma once
#include "gaia/config/config.h"

#include <cinttypes>
#include <cstddef>
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
	namespace mem {
		static constexpr uint32_t SmallBlockAlignment = (uint32_t)alignof(std::max_align_t);
		static constexpr uint32_t SmallBlockGranularity = SmallBlockAlignment;
		static constexpr uint32_t SmallBlockMaxSize = 512;
		static constexpr uint32_t SmallBlockSizeTypeCount = SmallBlockMaxSize / SmallBlockGranularity;

		//! Returns the usable block size for the given size class.
		//! \param sizeType Size class index.
		//! \return Usable bytes available to the caller.
		constexpr uint32_t small_block_size(uint32_t sizeType) {
			GAIA_ASSERT(sizeType < SmallBlockSizeTypeCount);
			return (sizeType + 1) * SmallBlockGranularity;
		}

		//! Returns the size class for the requested byte count.
		//! \param sizeBytes Requested usable bytes.
		//! \return Size class index able to satisfy the request.
		constexpr uint8_t small_block_size_type(uint32_t sizeBytes) {
			GAIA_ASSERT(sizeBytes > 0);
			GAIA_ASSERT(sizeBytes <= SmallBlockMaxSize);
			return (uint8_t)((align(sizeBytes, SmallBlockGranularity) / SmallBlockGranularity) - 1);
		}

		namespace detail {
			struct SmallBlockHeader final {
				uintptr_t m_pageAddr = 0;
#if GAIA_DEBUG
				uint32_t m_requestedBytes = 0;
				uint32_t m_reserved = 0;
#else
				uint64_t m_reserved = 0;
#endif
			};
		} // namespace detail

		static constexpr uint32_t SmallBlockUsableOffset =
				align((uint32_t)sizeof(detail::SmallBlockHeader), SmallBlockAlignment);

		struct GAIA_API SmallBlockAllocatorPageStats final {
			//! Total allocated memory for this size class.
			uint64_t mem_total;
			//! Memory reserved by live blocks for this size class.
			uint64_t mem_used;
			//! Number of allocated pages.
			uint32_t num_pages;
			//! Number of reusable pages (partial + empty).
			uint32_t num_pages_free;
#if GAIA_DEBUG
			//! Bytes explicitly requested by callers.
			uint64_t mem_requested;
			//! Number of completely empty pages.
			uint32_t num_pages_empty;
#endif
		};

		struct GAIA_API SmallBlockAllocatorStats final {
			SmallBlockAllocatorPageStats stats[SmallBlockSizeTypeCount];
		};

		namespace detail {
			static_assert(sizeof(SmallBlockHeader) <= SmallBlockUsableOffset);

			constexpr uint32_t small_block_stride(uint32_t sizeType) {
				return SmallBlockUsableOffset + small_block_size(sizeType);
			}

			class SmallBlockAllocatorImpl;

			struct SmallBlockPage final: cnt::fwd_llist_base<SmallBlockPage> {
				static constexpr uint16_t NBlocks = 64;
				static constexpr uint16_t NBlocks_Bits = (uint16_t)core::count_bits(NBlocks);
				static constexpr uint16_t SizeTypeBits = (uint16_t)core::count_bits(SmallBlockSizeTypeCount - 1);
				static constexpr uint32_t InvalidBlockId = NBlocks + 1;
#if GAIA_DEBUG
				static constexpr uint8_t FreedBlockPattern = 0xDD;
#endif
				static constexpr uint32_t BlockArrayBytes = ((uint32_t)NBlocks_Bits * (uint32_t)NBlocks + 7) / 8;

				using BlockArray = cnt::sarray<uint8_t, BlockArrayBytes>;
				using BitView = core::bit_view<NBlocks_Bits>;

				void* m_data;
				BlockArray m_blocks;

				uint32_t m_sizeType : SizeTypeBits;
				uint32_t m_blockCnt : NBlocks_Bits;
				uint32_t m_usedBlocks : NBlocks_Bits;
				uint32_t m_nextFreeBlock : NBlocks_Bits;
				uint32_t m_freeBlocks : NBlocks_Bits;

#if GAIA_ASSERT_ENABLED
				uint64_t m_usedMask = 0;
#endif

				//! Constructs a page for the given size class.
				//! \param ptr Raw page data.
				//! \param sizeType Size class index.
				SmallBlockPage(void* ptr, uint8_t sizeType):
						m_data(ptr), m_sizeType(sizeType), m_blockCnt(0), m_usedBlocks(0), m_nextFreeBlock(0), m_freeBlocks(0) {}

				//! Returns the stride of one block in this page.
				GAIA_NODISCARD uint32_t block_stride() const {
					return small_block_stride(m_sizeType);
				}

				void write_block_idx(uint32_t blockIdx, uint32_t value) {
					const uint32_t bitPosition = blockIdx * NBlocks_Bits;

					GAIA_ASSERT(bitPosition < NBlocks * NBlocks_Bits);
					GAIA_ASSERT(value <= InvalidBlockId);

					BitView{{(uint8_t*)m_blocks.data(), BlockArrayBytes}}.set(bitPosition, (uint8_t)value);
				}

				GAIA_NODISCARD uint8_t read_block_idx(uint32_t blockIdx) const {
					const uint32_t bitPosition = blockIdx * NBlocks_Bits;

					GAIA_ASSERT(bitPosition < NBlocks * NBlocks_Bits);

					return BitView{{(uint8_t*)m_blocks.data(), BlockArrayBytes}}.get(bitPosition);
				}

				//! Allocates one block from this page.
				//! \param bytesWanted Requested usable bytes.
				//! \return Pointer to the usable storage.
				GAIA_NODISCARD void* alloc_block(
#if GAIA_DEBUG
						uint32_t bytesWanted
#endif
				) {
					auto store_block_address = [&](uint32_t index) {
						auto* pMemoryBlock = (uint8_t*)m_data + (index * block_stride());
						GAIA_ASSERT((uintptr_t)pMemoryBlock % SmallBlockAlignment == 0);
						auto& header = block_header(pMemoryBlock);
						header.m_pageAddr = (uintptr_t)this;
#if GAIA_DEBUG
						header.m_requestedBytes = bytesWanted;
						header.m_reserved = 0;
#else
						header.m_reserved = 0;
#endif
						auto* pData = pMemoryBlock + SmallBlockUsableOffset;
						GAIA_ASSERT((uintptr_t)pData % SmallBlockAlignment == 0);
						return (void*)pData;
					};

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

					return store_block_address(index);
				}

				//! Frees one block back to this page.
				//! \param pBlock Pointer previously returned by alloc_block().
				void free_block(void* pBlock) {
					GAIA_ASSERT(pBlock != nullptr);
					GAIA_ASSERT(m_usedBlocks > 0);
					GAIA_ASSERT(m_freeBlocks <= NBlocks);

					const auto* pMemoryBlock = (uint8_t*)pBlock - SmallBlockUsableOffset;
					const auto blckAddr = (uintptr_t)pMemoryBlock;
					GAIA_ASSERT(blckAddr % SmallBlockAlignment == 0);
					const auto dataAddr = (uintptr_t)m_data;
					const auto blockStride = (uintptr_t)block_stride();
					const auto pageSize = blockStride * NBlocks;
					GAIA_ASSERT(blckAddr >= dataAddr);
					GAIA_ASSERT(blckAddr < dataAddr + pageSize);
					GAIA_ASSERT((blckAddr - dataAddr) % blockStride == 0);
					const auto blockIdx = (uint32_t)((blckAddr - dataAddr) / blockStride);
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
					std::memset(pBlock, FreedBlockPattern, small_block_size(m_sizeType));
#endif

					if (m_freeBlocks == 0U)
						write_block_idx(blockIdx, InvalidBlockId);
					else
						write_block_idx(blockIdx, m_nextFreeBlock);
					m_nextFreeBlock = blockIdx;

					++m_freeBlocks;
					--m_usedBlocks;
				}

				//! Returns the number of live blocks.
				GAIA_NODISCARD uint32_t used_blocks_cnt() const {
					return m_usedBlocks;
				}

				//! Returns true when the page is full.
				GAIA_NODISCARD bool full() const {
					return used_blocks_cnt() >= NBlocks;
				}

				//! Returns true when the page is empty.
				GAIA_NODISCARD bool empty() const {
					return used_blocks_cnt() == 0;
				}

				//! Verifies internal page invariants.
				void verify() const {
#if GAIA_ASSERT_ENABLED
					GAIA_ASSERT(m_sizeType < SmallBlockSizeTypeCount);
					GAIA_ASSERT(m_blockCnt <= NBlocks);
					GAIA_ASSERT(m_usedBlocks <= m_blockCnt);
					GAIA_ASSERT(m_freeBlocks <= m_blockCnt);
					GAIA_ASSERT(m_usedBlocks + m_freeBlocks == m_blockCnt);
					GAIA_ASSERT(((uintptr_t)m_data % SmallBlockAlignment) == 0);

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
						const auto* pMemoryBlock = (const uint8_t*)m_data + (i * block_stride());
						const auto& header = block_header(pMemoryBlock);
						GAIA_ASSERT(header.m_pageAddr == (uintptr_t)this);
						GAIA_ASSERT(((uintptr_t)pMemoryBlock % SmallBlockAlignment) == 0);

	#if GAIA_DEBUG
						const bool isFree = (freeMask & (uint64_t(1) << i)) != 0;
						GAIA_ASSERT((header.m_requestedBytes == 0) == isFree);
	#endif
					}

	#if GAIA_DEBUG
					GAIA_ASSERT((m_usedMask & freeMask) == 0);
					const auto liveMask = m_blockCnt == 64 ? ~uint64_t(0) : ((uint64_t(1) << m_blockCnt) - 1);
					GAIA_ASSERT((m_usedMask | freeMask) == liveMask);
	#endif
#endif
				}

#if GAIA_DEBUG
				//! Returns the number of caller-requested bytes held by live blocks.
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

						const auto* pMemoryBlock = (const uint8_t*)m_data + (i * block_stride());
						requested += block_header(pMemoryBlock).m_requestedBytes;
					}

					return requested;
				}
#endif

			private:
				static SmallBlockHeader& block_header(void* pMemoryBlock) {
					return *(SmallBlockHeader*)pMemoryBlock;
				}

				static const SmallBlockHeader& block_header(const void* pMemoryBlock) {
					return *(const SmallBlockHeader*)pMemoryBlock;
				}
			};

			enum class SmallBlockPageState : uint8_t { Detached, Empty, Partial, Full };

			struct SmallBlockPageContainer final {
				cnt::fwd_llist<SmallBlockPage> pagesEmpty;
				cnt::fwd_llist<SmallBlockPage> pagesPartial;
				cnt::fwd_llist<SmallBlockPage> pagesFull;
			};
		} // namespace detail

		using SmallBlockAllocator = core::dyn_singleton<detail::SmallBlockAllocatorImpl>;

		namespace detail {
			//! General-purpose allocator for small, variable-sized allocations up to 512 bytes.
			class SmallBlockAllocatorImpl final {
				friend ::gaia::mem::SmallBlockAllocator;

				SmallBlockPageContainer m_pages[SmallBlockSizeTypeCount];
				bool m_isDone = false;

				SmallBlockAllocatorImpl() = default;

			public:
				static constexpr uint32_t MAX_SIZE = SmallBlockMaxSize;

				~SmallBlockAllocatorImpl() {
					flush(true);

#if GAIA_ASSERT_ENABLED
					for (const auto& container: m_pages) {
						const bool hasPages = container.pagesEmpty.first != nullptr || container.pagesPartial.first != nullptr ||
																	container.pagesFull.first != nullptr;
						GAIA_ASSERT(!hasPages && "SmallBlockAllocator leaking memory");
					}
#endif
				}

				SmallBlockAllocatorImpl(SmallBlockAllocatorImpl&&) = delete;
				SmallBlockAllocatorImpl(const SmallBlockAllocatorImpl&) = delete;
				SmallBlockAllocatorImpl& operator=(SmallBlockAllocatorImpl&&) = delete;
				SmallBlockAllocatorImpl& operator=(const SmallBlockAllocatorImpl&) = delete;

				//! Allocates storage for up to 512 bytes.
				//! \param bytesWanted Requested usable bytes.
				//! \return Pointer to aligned usable storage.
				GAIA_NODISCARD void* alloc(uint32_t bytesWanted) {
					GAIA_ASSERT(bytesWanted > 0);
					GAIA_ASSERT(bytesWanted <= MAX_SIZE);
					if (bytesWanted == 0 || bytesWanted > MAX_SIZE)
						return nullptr;

					const auto sizeType = small_block_size_type(bytesWanted);
					auto& container = m_pages[sizeType];

					SmallBlockPageState prevState = SmallBlockPageState::Partial;
					auto* pPage = container.pagesPartial.first;
					if (pPage == nullptr) {
						prevState = SmallBlockPageState::Empty;
						pPage = container.pagesEmpty.first;
						if (pPage == nullptr) {
							prevState = SmallBlockPageState::Detached;
							pPage = alloc_page(sizeType);
						}
					}

#if GAIA_DEBUG
					void* pBlock = pPage->alloc_block(bytesWanted);
#else
					void* pBlock = pPage->alloc_block();
#endif
					move_page(container, pPage, prevState, state_for(*pPage));
					verify();
					return pBlock;
				}

				//! Releases storage allocated for the given pointer.
				//! \param pBlock Pointer previously returned by alloc().
				void free(void* pBlock) {
					GAIA_ASSERT(pBlock != nullptr);
					if (pBlock == nullptr)
						return;

					const auto& header = *(const SmallBlockHeader*)((uint8_t*)pBlock - SmallBlockUsableOffset);
					const auto pageAddr = header.m_pageAddr;
					GAIA_ASSERT(pageAddr % sizeof(uintptr_t) == 0);
#if GAIA_DEBUG
					GAIA_ASSERT(header.m_requestedBytes > 0);
#endif
					auto* pPage = (SmallBlockPage*)pageAddr;
					const auto prevState = state_for(*pPage);
					auto& container = m_pages[pPage->m_sizeType];

					pPage->free_block(pBlock);
					move_page(container, pPage, prevState, state_for(*pPage));
					verify();

					if (m_isDone) {
						if (pPage->empty()) {
							container.pagesEmpty.unlink(pPage);
							free_page(pPage);
						}

						try_delete_this();
					}
				}

				//! Flushes unused pages.
				//! \param releaseAll When true, all empty pages are released.
				void flush(bool releaseAll = false) {
					for (uint32_t i = 0; i < SmallBlockSizeTypeCount; ++i)
						flush_pages(m_pages[i], releaseAll);
					verify();
				}

				//! Returns allocator statistics per size class.
				GAIA_NODISCARD SmallBlockAllocatorStats stats() const {
					SmallBlockAllocatorStats stats{};
					for (uint32_t sizeType = 0; sizeType < SmallBlockSizeTypeCount; ++sizeType)
						stats.stats[sizeType] = page_stats(sizeType);
					return stats;
				}

				//! Performs diagnostics of allocator memory usage.
				void diag() const {
					const auto allStats = stats();
					for (uint32_t sizeType = 0; sizeType < SmallBlockSizeTypeCount; ++sizeType) {
						const auto& stats = allStats.stats[sizeType];
						if (stats.num_pages == 0)
							continue;

						GAIA_LOG_N("SmallBlockAllocator %u B stats", small_block_size(sizeType));
						GAIA_LOG_N("  Allocated: %" PRIu64 " B", stats.mem_total);
						GAIA_LOG_N("  Reserved by live blocks: %" PRIu64 " B", stats.mem_used);
						GAIA_LOG_N("  Pages: %u", stats.num_pages);
						GAIA_LOG_N("  Reusable pages: %u", stats.num_pages_free);
#if !GAIA_DEBUG
						GAIA_LOG_N(
								"  Utilization: %.1f%%",
								stats.mem_total ? 100.0 * ((double)stats.mem_used / (double)stats.mem_total) : 0.0);
#else
						GAIA_LOG_N("  Requested: %" PRIu64 " B", stats.mem_requested);
						GAIA_LOG_N("  Free capacity: %" PRIu64 " B", stats.mem_total - stats.mem_used);
						GAIA_LOG_N("  Internal slack: %" PRIu64 " B", stats.mem_used - stats.mem_requested);
						GAIA_LOG_N(
								"  Utilization: %.1f%%",
								stats.mem_total ? 100.0 * ((double)stats.mem_requested / (double)stats.mem_total) : 0.0);
						GAIA_LOG_N("  Empty pages: %u", stats.num_pages_empty);
#endif
					}
				}

				//! Verifies allocator invariants.
				void verify() const {
#if GAIA_ASSERT_ENABLED
					for (uint32_t sizeType = 0; sizeType < SmallBlockSizeTypeCount; ++sizeType)
						verify_container(m_pages[sizeType], sizeType);
#endif
				}

			private:
				static constexpr const char* s_strSmallBlockData = "SmallBlockData";
				static constexpr const char* s_strSmallBlockPage = "SmallBlockPage";

				static SmallBlockPage* alloc_page(uint8_t sizeType) {
					const uint32_t size = small_block_stride(sizeType) * SmallBlockPage::NBlocks;
					auto* pPageData = AllocHelper::alloc_alig<uint8_t>(s_strSmallBlockData, SmallBlockAlignment, size);
					auto* pMemoryPage = AllocHelper::alloc<SmallBlockPage>(s_strSmallBlockPage);
					return new (pMemoryPage) SmallBlockPage(pPageData, sizeType);
				}

				static void free_page(SmallBlockPage* pPage) {
					GAIA_ASSERT(pPage != nullptr);

					AllocHelper::free_alig(s_strSmallBlockData, pPage->m_data);
					pPage->~SmallBlockPage();
					AllocHelper::free(s_strSmallBlockPage, pPage);
				}

				void done() {
					m_isDone = true;
				}

				void try_delete_this() {
					bool allEmpty = true;
					for (const auto& container: m_pages) {
						const bool hasPages = container.pagesEmpty.first != nullptr || container.pagesPartial.first != nullptr ||
																	container.pagesFull.first != nullptr;
						allEmpty = allEmpty && !hasPages;
					}

					if (allEmpty)
						delete this;
				}

				static constexpr uint32_t warm_pages_to_keep() {
					return 1;
				}

				static SmallBlockPageState state_for(const SmallBlockPage& page) {
					if (page.empty())
						return SmallBlockPageState::Empty;
					if (page.full())
						return SmallBlockPageState::Full;
					return SmallBlockPageState::Partial;
				}

				static cnt::fwd_llist<SmallBlockPage>&
				page_list(SmallBlockPageContainer& container, SmallBlockPageState state) {
					switch (state) {
						case SmallBlockPageState::Empty:
							return container.pagesEmpty;
						case SmallBlockPageState::Partial:
							return container.pagesPartial;
						default:
							GAIA_ASSERT(state == SmallBlockPageState::Full);
							return container.pagesFull;
					}
				}

				static void move_page(
						SmallBlockPageContainer& container, SmallBlockPage* pPage, SmallBlockPageState fromState,
						SmallBlockPageState toState) {
					if (fromState == toState)
						return;

					if (fromState != SmallBlockPageState::Detached)
						page_list(container, fromState).unlink(pPage);
					page_list(container, toState).link(pPage);
				}

				static void
				verify_page_membership(const SmallBlockPage& page, uint32_t sizeType, SmallBlockPageState expectedState) {
					GAIA_ASSERT(page.m_sizeType == sizeType);
					GAIA_ASSERT(state_for(page) == expectedState);
					GAIA_ASSERT(page.get_fwd_llist_link().linked());
				}

				static void verify_container(const SmallBlockPageContainer& container, uint32_t sizeType) {
					for (const auto& page: container.pagesEmpty) {
						verify_page_membership(page, sizeType, SmallBlockPageState::Empty);
						page.verify();
					}

					for (const auto& page: container.pagesPartial) {
						verify_page_membership(page, sizeType, SmallBlockPageState::Partial);
						page.verify();
					}

					for (const auto& page: container.pagesFull) {
						verify_page_membership(page, sizeType, SmallBlockPageState::Full);
						page.verify();
					}
				}

				GAIA_NODISCARD SmallBlockAllocatorPageStats page_stats(uint32_t sizeType) const {
					SmallBlockAllocatorPageStats stats{};
					const auto& container = m_pages[sizeType];
					const auto blockStride = (uint64_t)small_block_stride(sizeType);
					const auto pageSize = blockStride * SmallBlockPage::NBlocks;

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
						stats.mem_used += page.used_blocks_cnt() * blockStride;
						stats.mem_requested += page.requested_bytes();
					}
#else
					for (const auto& page: container.pagesPartial)
						stats.mem_used += page.used_blocks_cnt() * blockStride;
#endif

					return stats;
				}

				static void flush_pages(SmallBlockPageContainer& container, bool releaseAll) {
					const bool keepWarmPage = !releaseAll && warm_pages_to_keep() != 0;
					bool keptWarmPage = false;

					for (auto it = container.pagesEmpty.begin(); it != container.pagesEmpty.end();) {
						auto* pPage = &(*it);
						++it;

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
	} // namespace mem
} // namespace gaia
