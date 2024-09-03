#pragma once
#include "../config/config.h"

#include <cinttypes>

#include "raw_data_holder.h"

namespace gaia {
	namespace mem {
		namespace detail {
			struct AllocationInfo {
				//! Byte offset of the previous allocation
				uint32_t prev;
				//! Offset of data area from info area in bytes
				uint32_t off : 8;
				//! The number of requested bytes to allocate
				uint32_t cnt : 24;
				void (*dtor)(void*, uint32_t);
			};
		} // namespace detail

		// MSVC might warn about applying additional padding to an instance of StackAllocator.
		// This is perfectly fine, but might make builds with warning-as-error turned on to fail.
		GAIA_MSVC_WARNING_PUSH()
		GAIA_MSVC_WARNING_DISABLE(4324)

		//! Stack allocator capable of instantiating any default-constructible object on stack.
		//! Every allocation comes with a 16-bytes long sentinel object.
		template <uint32_t CapacityInBytes = 1024>
		class StackAllocator {
			using alloc_info = detail::AllocationInfo;

			//! Internal stack buffer aligned to 16B boundary
			detail::raw_data_holder<CapacityInBytes, 16> m_buffer;
			//! Current byte offset
			uint32_t m_pos = 0;
			//! Byte offset of the previous allocation
			uint32_t m_posPrev = 0;
			//! Number of allocations made
			uint32_t m_allocs = 0;

		public:
			StackAllocator() {
				// Aligned used so the sentinel object can be stored properly
				const auto bufferMemAddr = (uintptr_t)((uint8_t*)m_buffer);
				m_posPrev = m_pos = padding<alignof(alloc_info)>(bufferMemAddr);
			}

			~StackAllocator() {
				reset();
			}

			StackAllocator(const StackAllocator&) = delete;
			StackAllocator(StackAllocator&&) = delete;
			StackAllocator& operator=(const StackAllocator&) = delete;
			StackAllocator& operator=(StackAllocator&&) = delete;

			//! Allocates \param cnt objects of type \tparam T inside the buffer.
			//! No default initialization is done so the object is returned in a non-initialized
			//! state unless a custom constructor is provided.
			//! \return Pointer to the first allocated object
			template <typename T>
			GAIA_NODISCARD T* alloc(uint32_t cnt) {
				constexpr auto sizeT = (uint32_t)sizeof(T);
				const auto addrBuff = (uintptr_t)((uint8_t*)m_buffer);
				const auto addrAllocInfo = align<alignof(alloc_info)>(addrBuff + m_pos);
				const auto addrAllocData = align<alignof(T)>(addrAllocInfo + sizeof(alloc_info));
				const auto off = (uint32_t)(addrAllocData - addrAllocInfo);

				// There has to be some space left in the buffer
				const bool isFull = (uint32_t)(addrAllocData - addrBuff) + sizeT * cnt >= CapacityInBytes;
				if GAIA_UNLIKELY (isFull) {
					GAIA_ASSERT(!isFull && "Allocation space exceeded on StackAllocator");
					return nullptr;
				}

				// Memory sentinel
				auto* pInfo = (alloc_info*)addrAllocInfo;
				pInfo->prev = m_posPrev;
				pInfo->off = off;
				pInfo->cnt = cnt;
				pInfo->dtor = [](void* ptr, uint32_t cnt) {
					core::call_dtor_n((T*)ptr, cnt);
				};

				// Constructing the object is necessary
				auto* pData = (T*)addrAllocData;
				core::call_ctor_raw_n(pData, cnt);

				// Allocation start offset
				m_posPrev = (uint32_t)(addrAllocInfo - addrBuff);
				// Point to the next free space (not necessary aligned yet)
				m_pos = m_posPrev + pInfo->off + sizeT * cnt;

				++m_allocs;
				return pData;
			}

			//! Frees the last allocated object from the stack.
			//! \param pData Pointer to the last allocated object on the stack
			//! \param cnt Number of objects that were allocated on the given memory address
			void free([[maybe_unused]] void* pData, [[maybe_unused]] uint32_t cnt) {
				GAIA_ASSERT(pData != nullptr);
				GAIA_ASSERT(cnt > 0);
				GAIA_ASSERT(m_allocs > 0);

				const auto addrBuff = (uintptr_t)((uint8_t*)m_buffer);

				// Destroy the last allocated object
				const auto addrAllocInfo = addrBuff + m_posPrev;
				auto* pInfo = (alloc_info*)addrAllocInfo;
				const auto addrAllocData = addrAllocInfo + pInfo->off;
				void* pInfoData = (void*)addrAllocData;
				GAIA_ASSERT(pData == pInfoData);
				GAIA_ASSERT(pInfo->cnt == cnt);
				pInfo->dtor(pInfoData, pInfo->cnt);

				m_pos = m_posPrev;
				m_posPrev = pInfo->prev;
				--m_allocs;
			}

			//! Frees all allocated objects from the buffer
			void reset() {
				const auto addrBuff = (uintptr_t)((uint8_t*)m_buffer);

				// Destroy allocated objects back-to-front
				auto pos = m_posPrev;
				while (m_allocs > 0) {
					const auto addrAllocInfo = addrBuff + pos;
					auto* pInfo = (alloc_info*)addrAllocInfo;
					const auto addrAllocData = addrAllocInfo + pInfo->off;
					pInfo->dtor((void*)addrAllocData, pInfo->cnt);
					pos = pInfo->prev;

					--m_allocs;
				}

				GAIA_ASSERT(m_allocs == 0);

				m_pos = 0;
				m_posPrev = 0;
				m_allocs = 0;
			}

			GAIA_NODISCARD constexpr uint32_t capacity() {
				return CapacityInBytes;
			}
		};

		GAIA_MSVC_WARNING_POP()
	} // namespace mem
} // namespace gaia