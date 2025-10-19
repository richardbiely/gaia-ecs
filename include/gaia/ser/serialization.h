#pragma once
#include "../config/config.h"

#include <type_traits>
#include <utility>

#include "../core/utility.h"
#include "../meta/reflection.h"

namespace gaia {
	namespace ser {
		GAIA_DEFINE_HAS_MEMBER_FUNC(resize);
		GAIA_DEFINE_HAS_MEMBER_FUNC(save);
		GAIA_DEFINE_HAS_MEMBER_FUNC(load);

		// --------------------
		// Customization tags
		// --------------------

		struct save_tag {};
		struct load_tag {};
		inline constexpr save_tag save_v{};
		inline constexpr load_tag load_v{};

		// --------------------
		// Detection traits
		// --------------------

		template <typename S, typename T>
		auto has_tag_save_impl(int)
				-> decltype(tag_invoke(save_v, std::declval<S&>(), std::declval<const T&>()), std::true_type{});
		template <typename, typename>
		std::false_type has_tag_save_impl(...);
		template <typename S, typename T>
		using has_tag_save = decltype(has_tag_save_impl<S, T>(0));

		template <typename S, typename T>
		auto has_tag_load_impl(int)
				-> decltype(tag_invoke(load_v, std::declval<S&>(), std::declval<T&>()), std::true_type{});
		template <typename, typename>
		std::false_type has_tag_load_impl(...);
		template <typename S, typename T>
		using has_tag_load = decltype(has_tag_load_impl<S, T>(0));

		namespace detail {
			enum class serialization_type_id : uint8_t {
				// Integer types
				s8 = 1,
				u8 = 2,
				s16 = 3,
				u16 = 4,
				s32 = 5,
				u32 = 6,
				s64 = 7,
				u64 = 8,

				// Boolean
				b = 40,

				// Character types
				c8 = 41,
				c16 = 42,
				c32 = 43,
				cw = 44,

				// Floating point types
				f8 = 81,
				f16 = 82,
				f32 = 83,
				f64 = 84,
				f128 = 85,

				// Special
				trivial_wrapper = 200,
				data_and_size = 201,

				Last = 255,
			};

			template <typename T>
			struct is_trivially_serializable {
			private:
				static constexpr bool update() {
					return std::is_enum_v<T> || std::is_fundamental_v<T> || std::is_trivially_copyable_v<T>;
				}

			public:
				static constexpr bool value = update();
			};

			template <typename T>
			struct is_int_kind_id:
					std::disjunction<
							std::is_same<T, int8_t>, std::is_same<T, uint8_t>, //
							std::is_same<T, int16_t>, std::is_same<T, uint16_t>, //
							std::is_same<T, int32_t>, std::is_same<T, uint32_t>, //
							std::is_same<T, int64_t>, std::is_same<T, uint64_t>, //
							std::is_same<T, bool>> {};

			template <typename T>
			struct is_flt_kind_id:
					std::disjunction<
							// std::is_same<T, float8_t>, //
							// std::is_same<T, float16_t>, //
							std::is_same<T, float>, //
							std::is_same<T, double>, //
							std::is_same<T, long double>> {};

			template <typename T>
			GAIA_NODISCARD constexpr serialization_type_id int_kind_id() {
				static_assert(is_int_kind_id<T>::value, "Unsupported integral type");

				if constexpr (std::is_same_v<int8_t, T>) {
					return serialization_type_id::s8;
				} else if constexpr (std::is_same_v<uint8_t, T>) {
					return serialization_type_id::u8;
				} else if constexpr (std::is_same_v<int16_t, T>) {
					return serialization_type_id::s16;
				} else if constexpr (std::is_same_v<uint16_t, T>) {
					return serialization_type_id::u16;
				} else if constexpr (std::is_same_v<int32_t, T>) {
					return serialization_type_id::s32;
				} else if constexpr (std::is_same_v<uint32_t, T>) {
					return serialization_type_id::u32;
				} else if constexpr (std::is_same_v<int64_t, T>) {
					return serialization_type_id::s64;
				} else if constexpr (std::is_same_v<uint64_t, T>) {
					return serialization_type_id::u64;
				} else { // if constexpr (std::is_same_v<bool, T>) {
					return serialization_type_id::b;
				}
			}

			template <typename T>
			GAIA_NODISCARD constexpr serialization_type_id flt_type_id() {
				static_assert(is_flt_kind_id<T>::value, "Unsupported floating type");

				// if constexpr (std::is_same_v<float8_t, T>) {
				// 	return serialization_type_id::f8;
				// } else if constexpr (std::is_same_v<float16_t, T>) {
				// 	return serialization_type_id::f16;
				// } else
				if constexpr (std::is_same_v<float, T>) {
					return serialization_type_id::f32;
				} else if constexpr (std::is_same_v<double, T>) {
					return serialization_type_id::f64;
				} else { // if constexpr (std::is_same_v<long double, T>) {
					return serialization_type_id::f128;
				}
			}

			template <typename T>
			GAIA_NODISCARD constexpr serialization_type_id type_id() {
				if constexpr (std::is_enum_v<T>)
					return int_kind_id<std::underlying_type_t<T>>();
				else if constexpr (std::is_integral_v<T>)
					return int_kind_id<T>();
				else if constexpr (std::is_floating_point_v<T>)
					return flt_type_id<T>();
				else if constexpr (core::has_size_begin_end<T>::value)
					return serialization_type_id::data_and_size;
				else if constexpr (std::is_class_v<T>)
					return serialization_type_id::trivial_wrapper;

				return serialization_type_id::Last;
			}

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
				else if constexpr (detail::is_trivially_serializable<U>::value) {
					s.save(arg);
				}
				// Types which have size(), begin() and end() member functions
				else if constexpr (core::has_size_begin_end<U>::value) {
					const auto size = arg.size();
					s.save(size);

					for (const auto& e: arg)
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
				else if constexpr (detail::is_trivially_serializable<U>::value) {
					s.load(arg);
				}
				// Types which have size(), begin() and end() member functions
				else if constexpr (core::has_size_begin_end<U>::value) {
					auto size = arg.size();
					s.load(size);

					if constexpr (has_func_resize<U, size_t>::value) {
						// If resize is present, use it
						arg.resize(size);
						for (auto& e: arg)
							load_one(s, e);
					} else {
						// With no resize present, write directly into memory
						GAIA_FOR(size) {
							using arg_type = typename std::remove_pointer<decltype(arg.data())>::type;
							auto& e_ref = (arg_type&)arg[i];
							load_one(s, e_ref);
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
		//!
		//! \warning Writer has to implement a save function as follows:
		//! 					template <typename T> void save(const T& arg);
		template <typename Writer, typename T>
		void save(Writer& writer, const T& data) {
			detail::save_one(writer, data);
		}

		//! Read \param data using \tparam Reader at compile-time.
		//!
		//! \warning Reader has to implement a save function as follows:
		//! 					template <typename T> void load(T& arg);
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