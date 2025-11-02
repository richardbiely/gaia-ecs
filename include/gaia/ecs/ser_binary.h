#pragma once
#include "../config/config.h"

#include "../mem/mem_alloc.h"
#include "../ser/ser_buffer_binary.h"
#include "../ser/ser_rt.h"

namespace gaia {
	namespace ecs {
		class BinarySerializer: public ser::ISerializer {
			ser::ser_buffer_binary m_buffer;

			//! Makes sure data is aligned
			void align(uint32_t size, ser::serialization_type_id id) {
				const auto pos = m_buffer.tell();
				const auto posAligned = mem::align(pos, ser::serialization_type_size(id, size));
				const auto offset = posAligned - pos;
				m_buffer.reserve(offset + size);
				m_buffer.skip(offset);
			}

		public:
			void save_raw(const void* src, uint32_t size, ser::serialization_type_id id) override {
				align(size, id);
				m_buffer.save_raw((const char*)src, size, id);
			}

			void load_raw(void* src, uint32_t size, ser::serialization_type_id id) override {
				align(size, id);
				m_buffer.load_raw((char*)src, size, id);
			}

			const char* data() const override {
				return (const char*)m_buffer.data();
			}

			void reset() override {
				m_buffer.reset();
			}

			uint32_t tell() const override {
				return m_buffer.tell();
			}

			uint32_t bytes() const override {
				return m_buffer.bytes();
			}

			void seek(uint32_t pos) override {
				m_buffer.seek(pos);
			}
		};
	} // namespace ecs
} // namespace gaia
