#pragma once
#include "gaia/config/config.h"

#include "gaia/mem/mem_alloc.h"
#include "gaia/ser/ser_buffer_binary.h"
#include "gaia/ser/ser_rt.h"

namespace gaia {
	namespace ser {
		//! Default in-memory binary backend used by ECS world/runtime serialization.
		//! Provides aligned raw read/write and can be wrapped into serializer via make_serializer().
		class bin_stream {
			ser_buffer_binary m_buffer;

			//! Makes sure data is aligned
			void align(uint32_t size, serialization_type_id id) {
				const auto pos = m_buffer.tell();
				const auto posAligned = mem::align(pos, serialization_type_size(id, size));
				const auto offset = posAligned - pos;
				m_buffer.reserve(offset + size);
				m_buffer.skip(offset);
			}

		public:
			//! Writes raw bytes with type-aware alignment.
			void save_raw(const void* src, uint32_t size, serialization_type_id id) {
				align(size, id);
				m_buffer.save_raw((const char*)src, size, id);
			}

			//! Reads raw bytes with type-aware alignment.
			void load_raw(void* src, uint32_t size, serialization_type_id id) {
				align(size, id);
				m_buffer.load_raw((char*)src, size, id);
			}

			//! Returns pointer to serialized bytes.
			const char* data() const {
				return (const char*)m_buffer.data();
			}

			//! Clears buffered data and resets stream position.
			void reset() {
				m_buffer.reset();
			}

			//! Returns current stream cursor position in bytes.
			uint32_t tell() const {
				return m_buffer.tell();
			}

			//! Returns total buffered byte count.
			uint32_t bytes() const {
				return m_buffer.bytes();
			}

			//! Moves stream cursor to an absolute byte position.
			void seek(uint32_t pos) {
				m_buffer.seek(pos);
			}

			//! Convenience typed save routed through serializer traversal.
			template <typename T>
			void save(const T& data) {
				auto s = make_serializer(*this);
				s.save(data);
			}

			//! Convenience typed load routed through serializer traversal.
			template <typename T>
			void load(T& data) {
				auto s = make_serializer(*this);
				s.load(data);
			}
		};
	} // namespace ser
} // namespace gaia
