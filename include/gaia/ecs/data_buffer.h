#pragma once
#include "../config/config.h"

#include <type_traits>

#include "../cnt/darray_ext.h"
#include "component_cache.h"

namespace gaia {
	namespace ecs {
		class SerializationBuffer {
			// Increase the capacity by multiples of CapacityIncreaseSize
			static constexpr uint32_t CapacityIncreaseSize = 128U;
			// TODO: Replace with some memory allocator
			using DataContainer = cnt::darray_ext<uint8_t, CapacityIncreaseSize>;

			const ComponentCache* m_cc;
			//! Buffer holding raw data
			DataContainer m_data;
			//! Current position in the buffer
			uint32_t m_dataPos = 0;

		public:
			SerializationBuffer() = default;
			SerializationBuffer(const ComponentCache* cc): m_cc(cc) {}
			~SerializationBuffer() = default;

			SerializationBuffer(const SerializationBuffer&) = default;
			SerializationBuffer(SerializationBuffer&&) = default;
			SerializationBuffer& operator=(const SerializationBuffer&) = default;
			SerializationBuffer& operator=(SerializationBuffer&&) = default;

			void reset() {
				m_dataPos = 0;
				m_data.clear();
			}

			//! Returns the number of bytes written in the buffer
			GAIA_NODISCARD uint32_t bytes() const {
				return (uint32_t)m_data.size();
			}

			//! Returns true if there is no data written in the buffer
			GAIA_NODISCARD bool empty() const {
				return m_data.empty();
			}

			//! Makes sure there is enough capacity in our data container to hold another \param size bytes of data
			void reserve(uint32_t size) {
				const auto nextSize = m_dataPos + size;
				if (nextSize <= bytes())
					return;

				// Make sure there is enough capacity to hold our data
				const auto newSize = bytes() + size;
				const auto newCapacity = (newSize / CapacityIncreaseSize) * CapacityIncreaseSize + CapacityIncreaseSize;
				m_data.reserve(newCapacity);
			}

			//! Changes the current position in the buffer
			void seek(uint32_t pos) {
				m_dataPos = pos;
			}

			//! Returns the current position in the buffer
			GAIA_NODISCARD uint32_t tell() const {
				return m_dataPos;
			}

			//! Writes \param value to the buffer
			template <typename T>
			void save(T&& value) {
				reserve(sizeof(T));

				m_data.resize(m_dataPos + sizeof(T));
				mem::unaligned_ref<T> mem(&m_data[m_dataPos]);
				mem = GAIA_FWD(value);

				m_dataPos += sizeof(T);
			}

			//! Writes \param size bytes of data starting at the address \param pSrc to the buffer
			void save(const void* pSrc, uint32_t size) {
				reserve(size);

				// Copy "size" bytes of raw data starting at pSrc
				m_data.resize(m_dataPos + size);
				memcpy((void*)&m_data[m_dataPos], pSrc, size);

				m_dataPos += size;
			}

			//! Writes \param value to the buffer
			template <typename T>
			void save_comp(T&& value) {
				const auto& desc = m_cc->get<T>();
				const bool isManualDestroyNeeded = desc.func_copy_ctor != nullptr || desc.func_move_ctor != nullptr;
				constexpr bool isRValue = std::is_rvalue_reference_v<decltype(value)>;

				reserve(sizeof(isManualDestroyNeeded) + sizeof(T));
				save(isManualDestroyNeeded);
				m_data.resize(m_dataPos + sizeof(T));

				auto* pSrc = (void*)&value;
				auto* pDst = (void*)&m_data[m_dataPos];
				if (isRValue && desc.func_move_ctor != nullptr)
					desc.func_move_ctor(pSrc, pDst);
				else if (desc.func_copy_ctor != nullptr)
					desc.func_copy_ctor(pSrc, pDst);
				else
					memmove(pDst, (const void*)pSrc, sizeof(T));

				m_dataPos += sizeof(T);
			}

			//! Loads \param value from the buffer
			template <typename T>
			void load(T& value) {
				GAIA_ASSERT(m_dataPos + sizeof(T) <= bytes());

				value = mem::unaligned_ref<T>((void*)&m_data[m_dataPos]);

				m_dataPos += sizeof(T);
			}

			//! Loads \param size bytes of data from the buffer and writes them to the address \param pDst
			void load(void* pDst, uint32_t size) {
				GAIA_ASSERT(m_dataPos + size <= bytes());

				memcpy(pDst, (void*)&m_data[m_dataPos], size);

				m_dataPos += size;
			}

			//! Loads \param value from the buffer
			void load_comp(void* pDst, ComponentId compId) {
				bool isManualDestroyNeeded = false;
				load(isManualDestroyNeeded);

				const auto& desc = m_cc->get(compId);
				GAIA_ASSERT(m_dataPos + desc.comp.size() <= bytes());
				auto* pSrc = (void*)&m_data[m_dataPos];
				desc.move(pSrc, pDst);
				if (isManualDestroyNeeded)
					desc.dtor(pSrc);

				m_dataPos += desc.comp.size();
			}
		};
	} // namespace ecs
} // namespace gaia