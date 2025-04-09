#pragma once
#include "../config/config.h"

#include <type_traits>

#include "../cnt/darray_ext.h"
#include "../mem/mem_utils.h"
#include "component_cache.h"

namespace gaia {
	namespace ecs {
		namespace detail {
			static constexpr uint32_t SerializationBufferCapacityIncreaseSize = 128U;

			template <typename DataContainer>
			class SerializationBufferImpl {
				// Increase the capacity by multiples of CapacityIncreaseSize
				static constexpr uint32_t CapacityIncreaseSize = SerializationBufferCapacityIncreaseSize;

				//! Buffer holding raw data
				DataContainer m_data;
				//! Current position in the buffer
				uint32_t m_dataPos = 0;

			public:
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
					mem::unaligned_ref<T> mem((void*)&m_data[m_dataPos]);
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
				void save_comp(const ComponentCacheItem& item, T&& value) {
					const bool isManualDestroyNeeded = item.func_copy_ctor != nullptr || item.func_move_ctor != nullptr;
					constexpr bool isRValue = std::is_rvalue_reference_v<decltype(value)>;

					reserve(sizeof(isManualDestroyNeeded) + sizeof(T));
					save(isManualDestroyNeeded);
					m_data.resize(m_dataPos + sizeof(T));

					auto* pSrc = (void*)&value; // TODO: GAIA_FWD(value)?
					auto* pDst = (void*)&m_data[m_dataPos];
					if (isRValue && item.func_move_ctor != nullptr) {
						if constexpr (mem::is_movable<T>())
							mem::detail::move_ctor_element_aos<T>((T*)pDst, (T*)pSrc, 0, 0);
						else
							mem::detail::copy_ctor_element_aos<T>((T*)pDst, (const T*)pSrc, 0, 0);
					} else
						mem::detail::copy_ctor_element_aos<T>((T*)pDst, (const T*)pSrc, 0, 0);

					m_dataPos += sizeof(T);
				}

				//! Loads \param value from the buffer
				template <typename T>
				void load(T& value) {
					GAIA_ASSERT(m_dataPos + sizeof(T) <= bytes());

					const auto& cdata = std::as_const(m_data);
					value = mem::unaligned_ref<T>((void*)&cdata[m_dataPos]);

					m_dataPos += sizeof(T);
				}

				//! Loads \param size bytes of data from the buffer and writes them to the address \param pDst
				void load(void* pDst, uint32_t size) {
					GAIA_ASSERT(m_dataPos + size <= bytes());

					const auto& cdata = std::as_const(m_data);
					memmove(pDst, (const void*)&cdata[m_dataPos], size);

					m_dataPos += size;
				}

				//! Loads \param value from the buffer
				void load_comp(const ComponentCache& cc, void* pDst, Entity entity) {
					bool isManualDestroyNeeded = false;
					load(isManualDestroyNeeded);

					const auto& desc = cc.get(entity);
					GAIA_ASSERT(m_dataPos + desc.comp.size() <= bytes());
					const auto& cdata = std::as_const(m_data);
					auto* pSrc = (void*)&cdata[m_dataPos];
					desc.move(pDst, pSrc, 0, 0, 1, 1);
					if (isManualDestroyNeeded)
						desc.dtor(pSrc);

					m_dataPos += desc.comp.size();
				}
			};
		} // namespace detail

		using SerializationBuffer_DArrExt = cnt::darray_ext<uint8_t, detail::SerializationBufferCapacityIncreaseSize>;
		using SerializationBuffer_DArr = cnt::darray<uint8_t>;

		class SerializationBuffer: public detail::SerializationBufferImpl<SerializationBuffer_DArrExt> {};
		class SerializationBufferDyn: public detail::SerializationBufferImpl<SerializationBuffer_DArr> {};
	} // namespace ecs
} // namespace gaia