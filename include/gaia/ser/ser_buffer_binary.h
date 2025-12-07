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

				//! Makes sure there is enough capacity in our data container to hold another @a size bytes of data.
				//! \param size Minimum number of free bytes at the end of the buffer.
				void reserve(uint32_t size) {
					const auto nextSize = m_dataPos + size;
					if (nextSize <= bytes())
						return;

					// Make sure there is enough capacity to hold our data
					const auto newSize = bytes() + size;
					const auto newCapacity = ((newSize / CapacityIncreaseSize) * CapacityIncreaseSize) + CapacityIncreaseSize;
					m_data.reserve(newCapacity);
				}

				//! Resizes the internal buffer to @a size bytes.
				//! \param size Position in the buffer to move to.
				void resize(uint32_t size) {
					m_data.resize(size);
				}

				//! Changes the current position in the buffer.
				//! \param pos Position in the buffer to move to.
				void seek(uint32_t pos) {
					m_dataPos = pos;
				}

				//! Advances @a size bytes from the current buffer position.
				//! \param size Number of bytes to skip
				void skip(uint32_t size) {
					m_dataPos += size;
				}

				//! Returns the current position in the buffer
				GAIA_NODISCARD uint32_t tell() const {
					return m_dataPos;
				}

				//! Writes @a value to the buffer
				//! \param value Value to store
				template <typename T>
				void save(T&& value) {
					reserve((uint32_t)sizeof(T));

					const auto cnt = m_dataPos + (uint32_t)sizeof(T);
					if (cnt > m_data.size())
						m_data.resize(cnt);
					mem::unaligned_ref<T> mem((void*)&m_data[m_dataPos]);
					mem = GAIA_FWD(value);

					m_dataPos += (uint32_t)sizeof(T);
				}

				//! Writes @a size bytes of data starting at the address @a pSrc to the buffer
				//! \param pSrc Pointer to serialized data
				//! \param size Size of serialized data in bytes
				//! \param id Type of serialized data
				void save_raw(const void* pSrc, uint32_t size, [[maybe_unused]] ser::serialization_type_id id) {
					if (size == 0)
						return;

					reserve(size);

					// Copy "size" bytes of raw data starting at pSrc
					const auto cnt = m_dataPos + size;
					if (cnt > m_data.size())
						m_data.resize(cnt);
					memcpy((void*)&m_data[m_dataPos], pSrc, size);

					m_dataPos += size;
				}

				//! Loads @a value from the buffer
				//! \param[out] value Value to load
				template <typename T>
				void load(T& value) {
					GAIA_ASSERT(m_dataPos + (uint32_t)sizeof(T) <= bytes());

					const auto& cdata = std::as_const(m_data);
					value = mem::unaligned_ref<T>((void*)&cdata[m_dataPos]);

					m_dataPos += (uint32_t)sizeof(T);
				}

				//! Loads @a size bytes of data from the buffer and writes it to the address @a pDst
				//! \param[out] pDst Pointer to where deserialized data is written
				//! \param size Size of serialized data in bytes
				//! \param id Type of serialized data
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
