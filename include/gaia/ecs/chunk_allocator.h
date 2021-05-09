#pragma once

#include <cassert>
#if !defined(alloca)
  #if defined(__GLIBC__) || defined(__sun) || defined(__CYGWIN__)
    #include <alloca.h> // alloca
  #elif defined(_WIN32)
    #include <malloc.h> // alloca
    #if !defined(alloca)
      #define alloca _alloca // for clang with MS Codegen
    #endif
  #else
    #include <stdlib.h> // alloca
  #endif
#endif

#include "../config/config.h"
#include "../config/logging.h"
#include "../utils/utils_std.h"
#include "common.h"

#define ECS_ALLOCATOR_MAINTHREAD 0
#if ECS_ALLOCATOR_MAINTHREAD
  // TODO: Replace with some code checking the thread
  #define ECS_ASSERT_MAIN_THREAD ((void)0)
#else
  #define ECS_ASSERT_MAIN_THREAD ((void)0)
#endif

#define MM_ALLOC(x) malloc(x)
#define MM_FREE(x) free(x)

namespace gaia {
  namespace ecs {
    class DummyCounter {
    public:
      uint32_t operator++(int) { return 0; }
      uint32_t operator--(int) { return 0; }
      uint32_t operator()() const { return 0; }
      void Reset() {}
    };

    class CCounter {
      uint32_t c = 0;

    public:
      uint32_t operator++(int) { return ++c; }
      uint32_t operator--(int) { return --c; }
      uint32_t operator()() const { return c; }
      void Reset() { c = 0; }
    };

    template <class T> struct LListItem {
      T* next = nullptr;
      T** prev = nullptr;

      bool IsLinked() const { return (next != nullptr || prev != nullptr); }

      void Remove() {
        if (next)
          next->prev = prev;
        if (prev)
          *prev = next;
        next = nullptr;
        prev = nullptr;
      }

      void Insert(T* l) {
        l->next = next;
        if (next)
          next->prev = &l->next;
        next = l;
        l->prev = &next;
      }

      void InsertBefore(T* l) {
        next = l->next;
        prev = l;
        l->next = this;
        l->next->prev = this;
      }
    };

    template <class T, class Counter = DummyCounter> struct LList {
      Counter count;
      T* first = nullptr;

      void Clear() { first = nullptr; }

      bool IsEmpty() const { return first == nullptr; }
    };

#define LListInsert(llist, item, link)                                         \
  (item)->link.next = llist.first;                                             \
    if (llist.first) {                                                         \
      llist.first->link.prev = &((item)->link.next);                           \
      llist.first = item;                                                      \
  }                                                                            \
  (item)->link.prev = &llist.first;                                            \
  llist.first = item;                                                          \
  llist.count++;

#define LListRemove(llist, item, link)                                         \
  *((item)->link.prev) = (item)->link.next;                                    \
  if ((item)->link.next)                                                       \
    (item)->link.next->link.prev = (item)->link.prev;                          \
  llist.count--;

    struct SmallBlockAllocatorStats final {
      //! Total allocated memory
      uint64_t Allocated;
      //! Overhead and cached allocations
      uint64_t Overhead;
      //! Number of allocated blocks
      uint64_t NumAllocations;
      //! Number of internal heap allocations
      uint64_t NumBlocks;
    };

    /*!
    Archetype chunk allocator. It's used when an archetype tries to allocate a
    new chunk. Every time we're out of space this allocator creates a page of 16
    MiB. Each page consists of blocks of 1 MiB in size which are added or
    removed as necessary. Each block then stores archetype chunks which are 16
    kiB big (CHUNK_SIZE) and therefore can take 64 chunks. This way we keep
    everything tight in memory and avoid both unnecessary allocation and
    fragmentation.
    */
    class ChunkAllocator {
      struct MemoryPage;
      struct MemItem {
        MemItem* next;
      };

      struct BlockHead {
        //! Page owning the block
        MemoryPage* m_pOwner = nullptr;
        //! Pointer to first free item in block
        MemItem* m_pFirstFree = nullptr;
        //! Either link in MemoryBlock, or in MemoryPage
        LListItem<BlockHead> m_Link;
        //! Initial free offset
        uint32_t m_iFreeOffset = BlockHeadSize;
        //! Number of used items. If equal to m_iMaxItems it is necessary to
        //! create a new block
        uint16_t m_iUsedItems = 0;
      };

      static constexpr size_t BlockHeadSize = ((sizeof(BlockHead) + 15) & ~15);
      static void* BlockHeadData(BlockHead& head) {
        return (uint8_t*)&head + BlockHeadSize;
      }

      struct MemoryBlock {
        static constexpr uint32_t Size = 1024 * 1024; // 1 MiB per block

        //! Number of items per block
        uint32_t m_iMaxItems = 0;
        //! Number of allocations in block. Total memory = CHUNK_SIZE *
        //! m_iNumAllocations
        uint32_t m_iNumAllocations = 0;
        //! Number of block currently used
        uint32_t m_iNumBlocks = 0;
        //! List of free blocks
        LList<BlockHead> m_FreeBlocks;
        //! List of full blocks
        LList<BlockHead> m_FullBlocks;

        [[nodiscard]] size_t GetInstanceSize() const {
          uint32_t num = 0;

          BlockHead* pblock = m_FreeBlocks.first;
            while (pblock) {
              pblock = pblock->m_Link.next;
              num++;
            }

          pblock = m_FullBlocks.first;
            while (pblock) {
              pblock = pblock->m_Link.next;
              num++;
            }

          return sizeof(MemoryBlock) + num * Size;
        }
      };

      struct MemoryPage {
        static constexpr uint32_t Size =
            16 * MemoryBlock::Size; // 16 MiB per page

        void* data;
        LList<BlockHead, CCounter> m_Blocks;
        LListItem<MemoryPage> m_Link;
        MemoryPage(void* ptr) : data(ptr) {}
      };

      LList<MemoryPage, CCounter> m_FreePages;
      LList<MemoryPage, CCounter> m_FullPages;
      MemoryBlock m_Block;

    public:
      ChunkAllocator() {
        ECS_ASSERT_MAIN_THREAD;

        m_Block.m_iMaxItems =
            (uint16_t)((MemoryBlock::Size - BlockHeadSize) / CHUNK_SIZE);
      }

      ~ChunkAllocator() {
        ECS_ASSERT_MAIN_THREAD;

        FreeAll();
      }

      /*!
      Allocates memory
      */
      void* Alloc() {
        ECS_ASSERT_MAIN_THREAD;

        m_Block.m_iNumAllocations++;

        BlockHead* freeHead = m_Block.m_FreeBlocks.first;
          if (!freeHead) {
            // No free block?
            freeHead = AllocBlock();
            LListInsert(m_Block.m_FreeBlocks, freeHead, m_Link);
        }

        MemItem* pfree = freeHead->m_pFirstFree;
          if (pfree) {
            // Recycled allocation
            freeHead->m_pFirstFree = pfree->next;
            assert(
                ((uintptr_t)freeHead + MemoryBlock::Size) >=
                (uintptr_t)pfree + CHUNK_SIZE);
          } else {
            // Initial allocation
            pfree = (MemItem*)((uint8_t*)freeHead + freeHead->m_iFreeOffset);
            freeHead->m_iFreeOffset += CHUNK_SIZE;
            assert(freeHead->m_iFreeOffset <= MemoryBlock::Size);
          }

        freeHead->m_iUsedItems++;
          // Just made it full, move to full list
          if (freeHead->m_iUsedItems == m_Block.m_iMaxItems) {
            LListRemove(m_Block.m_FreeBlocks, freeHead, m_Link);
            LListInsert(m_Block.m_FullBlocks, freeHead, m_Link);
        }

#ifdef _DEBUG
        // Fill allocated memory by 0xbaadf00d as MSVC does
        utils::fill_array(
            (uint32_t*)pfree, (uint32_t)((CHUNK_SIZE + 3) / sizeof(uint32_t)),
            0x7fcdf00dU);
#endif

        return pfree;
      }

      /*!
      Releases memory allocated for pointer
      */
      size_t Free(void* pointer) {
        ECS_ASSERT_MAIN_THREAD;

        BlockHead* head = BlockFromPtr(pointer);

#if GAIA_DEBUG
        [[maybe_unused]] const auto offset =
            (uintptr_t)pointer -
            (uintptr_t)ChunkAllocator::BlockHeadData(*head);
        assert(
            (offset % CHUNK_SIZE) == 0 &&
            "Attempt to deallocate mismatched pointer");
        assert(
            ((uintptr_t)head + MemoryBlock::Size) >=
            (uintptr_t)pointer + CHUNK_SIZE);
#endif

#ifdef _DEBUG
        // Fill freed memory by 0xfeeefeee as MSVC does
        utils::fill_array(
            (uint32_t*)pointer, (int)(CHUNK_SIZE / sizeof(uint32_t)),
            0xfeeefeeeU);
#endif

        m_Block.m_iNumAllocations--;

        // Retype and link with free items
        ((MemItem*)pointer)->next = head->m_pFirstFree;
        head->m_pFirstFree = (MemItem*)pointer;

          if (head->m_iUsedItems == m_Block.m_iMaxItems) {
            // just made it free. Move to free list
            LListRemove(m_Block.m_FullBlocks, head, m_Link);
            LListInsert(m_Block.m_FreeBlocks, head, m_Link);
        }

        head->m_iUsedItems--;

        // Pointer removed more than once?!
        assert(head->m_iUsedItems >= 0);

          if (head->m_iUsedItems == 0) {
              // If it's empty but it's not the last one, release it
              if (head != m_Block.m_FreeBlocks.first ||
                  head->m_Link.next != NULL) {
                LListRemove(m_Block.m_FreeBlocks, head, m_Link);
                m_Block.m_iNumBlocks--;

                FreeBlock(head);
                return CHUNK_SIZE;
              } else {
                head->m_pFirstFree = nullptr;
                head->m_iFreeOffset = BlockHeadSize;
              }
        }

        return CHUNK_SIZE;
      }

      /*!
      Releases all allocated memory
      */
      void FreeAll() {
        ECS_ASSERT_MAIN_THREAD;

          // Release free pages
          for (MemoryPage *next, *page = m_FreePages.first; page; page = next) {
            next = page->m_Link.next;
            FreePage(page);
          }

          // Release full pages
          for (MemoryPage *next, *page = m_FullPages.first; page; page = next) {
            next = page->m_Link.next;
            FreePage(page);
          }

        m_Block.m_FreeBlocks.Clear();
        m_Block.m_FullBlocks.Clear();
        m_Block.m_iNumBlocks = 0;
        m_Block.m_iNumAllocations = 0;
      }

      /*!
      Returns allocator statistics
      */
      void GetStats(SmallBlockAllocatorStats& stats) const {
        ECS_ASSERT_MAIN_THREAD;

        stats.Allocated = m_Block.m_iNumAllocations * CHUNK_SIZE;
        stats.NumAllocations = m_Block.m_iNumAllocations;
        stats.NumBlocks = m_Block.m_iNumBlocks;
        stats.Overhead =
            (m_Block.m_iNumBlocks * MemoryBlock::Size) - stats.Allocated;
      }

      /*!
      Flushes unused memory
      */
      void Flush() {
        ECS_ASSERT_MAIN_THREAD;

          for (BlockHead *next, *head = m_Block.m_FreeBlocks.first; head;
               head = next) {
            next = head->m_Link.next;

              if (head->m_iUsedItems == 0) {
                ReleaseBlock(head);
                FreeBlock(head);
            }
          }

          for (MemoryPage *next, *page = m_FreePages.first; page; page = next) {
            next = page->m_Link.next;
              if (page->m_Blocks.count() ==
                  (MemoryPage::Size / MemoryBlock::Size)) {
                LListRemove(m_FreePages, page, m_Link);
                FreePage(page);
            }
          }
      }

    private:
      MemoryPage* AllocPage() {
        const size_t pageSize = MemoryPage::Size + MemoryBlock::Size - 1;
        auto* pagePtr = (uint8_t*)MM_ALLOC(pageSize);

        auto* mpage = new MemoryPage(pagePtr);

        uint8_t* firstBlock =
            (uint8_t*)BlockFromPtr(pagePtr + MemoryBlock::Size - 1);
        const uint32_t numBlocks = MemoryPage::Size / MemoryBlock::Size;
          for (int n = numBlocks - 1; n >= 0; n--) {
            BlockHead* blockHead =
                (BlockHead*)(firstBlock + MemoryBlock::Size * n);
            new (blockHead) BlockHead;
            blockHead->m_pOwner = mpage;
            LListInsert(mpage->m_Blocks, blockHead, m_Link);
          }

        LListInsert(m_FreePages, mpage, m_Link);
        return mpage;
      }

      void FreePage(MemoryPage* page) {
        MM_FREE(page->data);
        delete page;
      }

      BlockHead* AllocBlock() {
        MemoryPage* page = m_FreePages.first;
        if (page == nullptr)
          page = AllocPage();

        BlockHead* newone = page->m_Blocks.first;
        LListRemove(page->m_Blocks, newone, m_Link);
          if (page->m_Blocks.IsEmpty()) {
            LListRemove(m_FreePages, page, m_Link);
            LListInsert(m_FullPages, page, m_Link);
        }

        m_Block.m_iNumBlocks++;
        newone->m_iFreeOffset = BlockHeadSize;
        newone->m_pFirstFree = nullptr;
        newone->m_iUsedItems = 0;
        return newone;
      }

      void FreeBlock(BlockHead* head) {
        MemoryPage* page = head->m_pOwner;
        assert(
            (uintptr_t)head >= (uintptr_t)page->data &&
            (uintptr_t)head < ((uintptr_t)page->data + MemoryPage::Size));
        LListInsert(page->m_Blocks, head, m_Link);

          if (page->m_Blocks.count() == 1) {
            LListRemove(m_FullPages, page, m_Link);
            LListInsert(m_FreePages, page, m_Link);
          } else if (
              page->m_Blocks.count() ==
              (MemoryPage::Size / MemoryBlock::Size)) {
              if (m_FreePages.count() > 1) {
                LListRemove(m_FreePages, page, m_Link);
                FreePage(page);
            }
        }
      }

      void ReleaseBlock(BlockHead* head) {
        LListRemove(m_Block.m_FreeBlocks, head, m_Link);
        m_Block.m_iNumBlocks--;
      }

      static BlockHead* BlockFromPtr(void* ptr) {
        return (BlockHead*)((uintptr_t)ptr & ~uintptr_t(MemoryBlock::Size - 1));
      }
    };

    //-----------------------------------------------------------------------------------

    extern ChunkAllocator g_ChunkAllocator;

    GAIA_API void DumpAllocatorStats() {
      SmallBlockAllocatorStats memstats;
      g_ChunkAllocator.GetStats(memstats);
        if (memstats.NumAllocations != 0) {
          LOG_N("ChunkAllocator stats");
          LOG_N("  Allocations: %llu", memstats.NumAllocations);
          LOG_N("  Blocks: %llu", memstats.NumBlocks);
          LOG_N("  Allocated: %llu B", memstats.Allocated);
          LOG_N("  Overhead: %llu B", memstats.Overhead);
      }
    }

#define GAIA_ECS_CHUNK_ALLOCATOR_H_INIT                                        \
  gaia::ecs::ChunkAllocator gaia::ecs::g_ChunkAllocator;

    //-----------------------------------------------------------------------------------
  } // namespace ecs
} // namespace gaia
