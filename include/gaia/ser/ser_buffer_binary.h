#pragma once
#include "gaia/config/config.h"

#include <type_traits>

#include "gaia/cnt/darray.h"
#include "gaia/cnt/darray_ext.h"

namespace gaia {
	namespace ser {
		enum class serialization_type_id : uint8_t;

		namespace detail {
			static constexpr uint32_t SerializationBufferCapacityIncreaseSize = 128U;

			template <typename DataContainer>
			class ser_buffer_binary_impl {
			protected:
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

				//! Returns the pointer to the data in the buffer
				GAIA_NODISCARD const auto* data() const {
					return m_data.data();
				}

				//! Makes sure there is enough capacity in our data container to hold another \param size bytes of data
				void reserve(uint32_t size) {
					const auto nextSize = m_dataPos + size;
					if (nextSize <= bytes())
						return;

					// Make sure there is enough capacity to hold our data
					const auto newSize = bytes() + size;
					const auto newCapacity = ((newSize / CapacityIncreaseSize) * CapacityIncreaseSize) + CapacityIncreaseSize;
					m_data.reserve(newCapacity);
				}

				void resize(uint32_t size) {
					m_data.resize(size);
				}

				//! Changes the current position in the buffer
				void seek(uint32_t pos) {
					m_dataPos = pos;
				}

				//! Advances \param size bytes from the current buffer position
				void skip(uint32_t size) {
					m_dataPos += size;
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
				void save_raw(const void* pSrc, uint32_t size, [[maybe_unused]] ser::serialization_type_id id) {
					if (size == 0)
						return;

					reserve(size);

					// Copy "size" bytes of raw data starting at pSrc
					m_data.resize(m_dataPos + size);
					memcpy((void*)&m_data[m_dataPos], pSrc, size);

					m_dataPos += size;
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
				void load_raw(void* pDst, uint32_t size, [[maybe_unused]] ser::serialization_type_id id) {
					if (size == 0)
						return;

					GAIA_ASSERT(m_dataPos + size <= bytes());

					const auto& cdata = std::as_const(m_data);
					memmove(pDst, (const void*)&cdata[m_dataPos], size);

					m_dataPos += size;
				}
			};
		} // namespace detail

		using ser_buffer_binary_storage = gaia::cnt::darray_ext<uint8_t, detail::SerializationBufferCapacityIncreaseSize>;
		using ser_buffer_binary_storage_dyn = gaia::cnt::darray<uint8_t>;

		//! Minimal binary serializer meant to runtime data.
		//! It does not offer any versioning, or type information.
		class ser_buffer_binary: public detail::ser_buffer_binary_impl<ser_buffer_binary_storage> {};
		class ser_buffer_binary_dyn: public detail::ser_buffer_binary_impl<ser_buffer_binary_storage_dyn> {};
	} // namespace ser
} // namespace gaia
