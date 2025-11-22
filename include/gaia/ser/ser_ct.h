#pragma once
#include "gaia/config/config.h"

#include <type_traits>
#include <utility>

#include "gaia/core/utility.h"
#include "gaia/meta/reflection.h"
#include "gaia/ser/ser_common.h"

namespace gaia {
	namespace ser {
		namespace detail {
			template <typename Writer, typename T>
			void save_one(Writer& s, const T& arg) {
				using U = core::raw_t<T>;

				// Custom save() has precedence
				if constexpr (has_func_save<U, Writer&>::value) {
					arg.save(s);
				} else if constexpr (has_tag_save<Writer, U>::value) {
					tag_invoke(save_v, s, static_cast<const U&>(arg));
				}
				// Trivially serializable types
				else if constexpr (is_trivially_serializable<U>::value) {
					s.save(arg);
				}
				// Types which have size(), begin() and end() member functions
				else if constexpr (core::has_size_begin_end<U>::value) {
					const auto size = arg.size();
					s.save(size);

					for (const auto& e: std::as_const(arg))
						save_one(s, e);
				}
				// Classes
				else if constexpr (std::is_class_v<U>) {
					meta::each_member(GAIA_FWD(arg), [&s](auto&&... items) {
						// TODO: Handle contiguous blocks of trivially copyable types
						(save_one(s, items), ...);
					});
				} else
					static_assert(!sizeof(U), "Type is not supported for serialization, yet");
			}

			template <typename Reader, typename T>
			void load_one(Reader& s, T& arg) {
				using U = core::raw_t<T>;

				// Custom load() has precedence
				if constexpr (has_func_load<U, Reader&>::value) {
					arg.load(s);
				} else if constexpr (has_tag_load<Reader, U>::value) {
					tag_invoke(load_v, s, static_cast<U&>(arg));
				}
				// Trivially serializable types
				else if constexpr (is_trivially_serializable<U>::value) {
					s.load(arg);
				}
				// Types which have size(), begin() and end() member functions
				else if constexpr (core::has_size_begin_end<U>::value) {
					auto size = arg.size();
					s.load(size);

					if constexpr (has_func_resize<U, size_t>::value) {
						// If resize is present, use it
						arg.resize(size);
						for (auto&& e: arg)
							load_one(s, e);
					} else {
						// With no resize present, write directly into memory
						GAIA_FOR(size) {
							using arg_type = typename std::remove_pointer<decltype(arg.data())>::type;
							arg_type val;
							load_one(s, val);
							arg[i] = val;
						}
					}
				}
				// Classes
				else if constexpr (std::is_class_v<U>) {
					meta::each_member(GAIA_FWD(arg), [&s](auto&&... items) {
						// TODO: Handle contiguous blocks of trivially copyable types
						(load_one(s, items), ...);
					});
				} else
					static_assert(!sizeof(U), "Type is not supported for serialization, yet");
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
		} // namespace detail

		//! Write \param data using \tparam Writer at compile-time.
		//! \warning Writer has to implement a save function as follows:
		//! 					 template <typename T> void save(const T& arg);
		template <typename Writer, typename T>
		void save(Writer& writer, const T& data) {
			detail::save_one(writer, data);
		}

		//! Read \param data using \tparam Reader at compile-time.
		//! \warning Reader has to implement a save function as follows:
		//! 					 template <typename T> void load(T& arg);
		template <typename Reader, typename T>
		void load(Reader& reader, T& data) {
			detail::load_one(reader, data);
		}

#if GAIA_ASSERT_ENABLED
		//! Write \param data using \tparam Writer at compile-time, then read it afterwards.
		//! Used to verify that both save and load work correctly.
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
