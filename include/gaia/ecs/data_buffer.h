#pragma once
#include "../config/config.h"

#include <type_traits>

#include "../containers/darray_ext.h"
#include "component_cache.h"

namespace gaia {
	namespace ecs {
		class DataBuffer {
			// Increase the capacity by multiples of CapacityIncreaseSize
			static constexpr uint32_t CapacityIncreaseSize = 128U;
			// TODO: Replace with some memory allocator
			using DataContainer = containers::darray_ext<uint8_t, CapacityIncreaseSize>;

			//! Buffer holding raw data
			DataContainer m_data;
			//! Current position in the buffer
			uint32_t m_dataPos = 0;

		public:
			DataBuffer() {}

			void Reset() {
				m_dataPos = 0;
				m_data.clear();
			}

			//! Returns the number of bytes written in the buffer
			GAIA_NODISCARD uint32_t Size() const {
				return (uint32_t)m_data.size();
			}

			//! Makes sure there is enough capacity in our data container to hold another \param size bytes of data
			void EnsureCapacity(uint32_t size) {
				const auto nextSize = m_dataPos + size;
				if (nextSize <= (uint32_t)m_data.size())
					return;

				// Make sure there is enough capacity to hold our data
				const auto newSize = m_data.size() + size;
				const auto newCapacity = (newSize / CapacityIncreaseSize) * CapacityIncreaseSize + CapacityIncreaseSize;
				m_data.reserve(newCapacity);
			}

			//! Changes the current position in the buffer
			void Seek(uint32_t pos) {
				m_dataPos = pos;
			}

			//! Returns the current position in the buffer
			GAIA_NODISCARD uint32_t GetPos() const {
				return m_dataPos;
			}

			//! Writes \param value to the buffer
			template <typename T>
			void Save(T&& value) {
				EnsureCapacity(sizeof(T));

				m_data.resize(m_dataPos + sizeof(T));
				utils::unaligned_ref<T> mem(&m_data[m_dataPos]);
				mem = std::forward<T>(value);

				m_dataPos += sizeof(T);
			}

			//! Writes \param value to the buffer
			template <typename T>
			void SaveComponent(T&& value) {
				const auto componentId = component::GetComponentId<T>();
				const auto& desc = ComponentCache::Get().GetComponentDesc(componentId);
				const bool isManualDestroyNeeded = desc.ctor_copy != nullptr || desc.ctor_move != nullptr;
				constexpr bool isRValue = std::is_rvalue_reference_v<decltype(value)>;

				EnsureCapacity(sizeof(isManualDestroyNeeded) + sizeof(T));
				Save(isManualDestroyNeeded);
				m_data.resize(m_dataPos + sizeof(T));

				auto* pSrc = (void*)&value;
				auto* pDst = (void*)&m_data[m_dataPos];
				if (isRValue && desc.ctor_move != nullptr)
					desc.ctor_move(pSrc, pDst);
				else if (desc.ctor_copy != nullptr)
					desc.ctor_copy(pSrc, pDst);
				else
					memmove(pDst, (const void*)pSrc, sizeof(T));

				m_dataPos += sizeof(T);
			}

			//! Writes \param size bytes of data starting at the address \param pSrc to the buffer
			void Save(const void* pSrc, uint32_t size) {
				EnsureCapacity(size);

				// Copy "size" bytes of raw data starting at pSrc
				m_data.resize(m_dataPos + size);
				memcpy((void*)&m_data[m_dataPos], pSrc, size);

				m_dataPos += size;
			}

			//! Loads \param value from the buffer
			void LoadComponent(void* pDst, component::ComponentId componentId) {
				bool isManualDestroyNeeded = false;
				Load(isManualDestroyNeeded);

				const auto& desc = ComponentCache::Get().GetComponentDesc(componentId);
				GAIA_ASSERT(m_dataPos + desc.properties.size <= m_data.size());

				auto* pSrc = (void*)&m_data[m_dataPos];
				desc.Move(pSrc, pDst);
				if (isManualDestroyNeeded)
					desc.Dtor(pSrc);

				m_dataPos += desc.properties.size;
			}

			//! Loads \param value from the buffer
			template <typename T>
			void Load(T& value) {
				GAIA_ASSERT(m_dataPos + sizeof(T) <= m_data.size());

				value = utils::unaligned_ref<T>((void*)&m_data[m_dataPos]);

				m_dataPos += sizeof(T);
			}

			//! Loads \param size bytes of data from the buffer and writes them to the address \param pDst
			void Load(void* pDst, uint32_t size) {
				GAIA_ASSERT(m_dataPos + size <= m_data.size());

				memcpy(pDst, (void*)&m_data[m_dataPos], size);

				m_dataPos += size;
			}
		};

		class DataBuffer_SerializationWrapper {
			ecs::DataBuffer& m_buffer;

		public:
			DataBuffer_SerializationWrapper(ecs::DataBuffer& buffer): m_buffer(buffer) {}

			ecs::DataBuffer& buffer() {
				return m_buffer;
			}

			void reserve(uint32_t size) {
				m_buffer.EnsureCapacity(size);
			}

			void seek(uint32_t pos) {
				m_buffer.Seek(pos);
			}

			template <typename T>
			void save(T&& arg) {
				m_buffer.Save(std::forward<T>(arg));
			}

			void save(const void* pSrc, uint32_t size) {
				m_buffer.Save(pSrc, size);
			}

			template <typename T>
			void load(T& arg) {
				m_buffer.Load(arg);
			}

			void load(void* pDst, uint32_t size) {
				m_buffer.Load(pDst, size);
			}
		};
	} // namespace ecs
} // namespace gaia