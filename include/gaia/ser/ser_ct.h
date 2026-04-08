#pragma once
#include "gaia/config/config.h"

#include <type_traits>
#include <utility>

#include "gaia/core/utility.h"
#include "gaia/ser/impl/ser_dispatch.h"
#include "gaia/ser/ser_common.h"

namespace gaia {
	namespace ser {
		//! Compile-time serialization entry points.
		//! Uses static dispatch and concrete writer/reader types (no virtual interface).
		//! Best suited when the serializer type is known at compile time.
		//! This is a binary traversal API; JSON document I/O uses ser::ser_json.
		namespace detail {
			template <typename Writer, typename T>
			void save_one(Writer& s, const T& arg) {
				auto saveTrivial = [](auto& writer, const auto& value) {
					writer.save(value);
				};
				save_dispatch(s, arg, saveTrivial);
			}

			template <typename Reader, typename T>
			void load_one(Reader& s, T& arg) {
				auto loadTrivial = [](auto& reader, auto& value) {
					reader.load(value);
				};
				load_dispatch(s, arg, loadTrivial);
			}

#if GAIA_ASSERT_ENABLED
			template <typename Writer, typename T>
			void check_one(Writer& s, const T& arg) {
				T tmp{};

				// Make sure that we write just as many bytes as we read.
				// If the positions are the same there is a good chance that save and load match.
				const auto pos0 = s.tell();
				save_one(s, arg);
				const auto pos1 = s.tell();
				s.seek(pos0);
				load_one(s, tmp);
				GAIA_ASSERT(s.tell() == pos1);

				// Return back to the original position in the buffer.
				s.seek(pos0);
			}
#endif

			//! Minimal writer used by ser::bytes to count produced bytes without storing data.
			class size_counter {
				uint32_t m_pos = 0;

			public:
				template <typename T>
				void save(const T&) {
					m_pos += (uint32_t)sizeof(T);
				}

				void save_raw(const void*, uint32_t size, [[maybe_unused]] ser::serialization_type_id id) {
					m_pos += size;
				}

				void seek(uint32_t pos) {
					m_pos = pos;
				}

				GAIA_NODISCARD uint32_t tell() const {
					return m_pos;
				}
			};
		} // namespace detail

		//! Calculates how many bytes @a data would need when serialized via ser::save.
		//! Useful when a destination storage wants to reserve memory in advance.
		template <typename T>
		GAIA_NODISCARD uint32_t bytes(const T& data) {
			detail::size_counter counter;
			detail::save_one(counter, data);
			return counter.tell();
		}

		//! Write @a data using @a Writer at compile-time.
		//! \tparam Writer Type of writer
		//! \param writer Writer used for serialization
		//! \param data Data to serialize
		//! \warning Writer has to implement a save function as follows:
		//! 					 `template <typename T> void save(const T& arg);`
		template <typename Writer, typename T>
		void save(Writer& writer, const T& data) {
			detail::save_one(writer, data);
		}

		//! Read @a data using @a Reader at compile-time.
		//! \tparam Reader Type of reader
		//! \param reader Reader used for deserialization
		//! \param[out] data Data to deserialize
		//! \warning Reader has to implement a save function as follows:
		//! 					 `template <typename T> void load(T& arg);`
		template <typename Reader, typename T>
		void load(Reader& reader, T& data) {
			detail::load_one(reader, data);
		}

#if GAIA_ASSERT_ENABLED
		//! Write \param data using \tparam Writer at compile-time, then read it afterwards.
		//! Used to verify that both save and load work correctly.
		//! \param writer Writer used to serialize @a data.
		//!
		//! \warning Writer has to implement a save function as follows:
		//! 					template <typename T> void save(const T& arg);
		//! \warning Reader has to implement a save function as follows:
		//! 					template <typename T> void load(T& arg);
		template <typename Writer, typename T>
		void check(Writer& writer, const T& data) {
			detail::check_one(writer, data);
		}
#endif
	} // namespace ser
} // namespace gaia
