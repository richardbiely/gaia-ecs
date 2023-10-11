#pragma once

#include "../config/config_core.h"

#include <type_traits>
#include <utility>

#include "reflection.h"
#include "utility.h"

namespace gaia {
	namespace serialization {
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

			template <typename C>
			constexpr auto size(const C& c) noexcept -> decltype(c.size()) {
				return c.size();
			}
			template <typename T, auto N>
			constexpr std::size_t size(const T (&)[N]) noexcept {
				return N;
			}

			template <typename C>
			constexpr auto data(C& c) noexcept -> decltype(c.data()) {
				return c.data();
			}
			template <typename C>
			constexpr auto data(const C& c) noexcept -> decltype(c.data()) {
				return c.data();
			}
			template <typename T, auto N>
			constexpr T* data(T (&array)[N]) noexcept {
				return array;
			}
			template <typename E>
			constexpr const E* data(std::initializer_list<E> il) noexcept {
				return il.begin();
			}

			template <typename, typename = void>
			struct has_data_and_size: std::false_type {};
			template <typename T>
			struct has_data_and_size<T, std::void_t<decltype(data(std::declval<T>())), decltype(size(std::declval<T>()))>>:
					std::true_type {};

			GAIA_DEFINE_HAS_FUNCTION(resize);
			GAIA_DEFINE_HAS_FUNCTION(size_bytes);
			GAIA_DEFINE_HAS_FUNCTION(save);
			GAIA_DEFINE_HAS_FUNCTION(load);

			template <typename T>
			struct is_trivially_serializable {
			private:
				static constexpr bool update() {
					return std::is_enum_v<T> || std::is_fundamental_v<T> || std::is_trivially_copyable_v<T>;
				}

			public:
				static inline constexpr bool value = update();
			};

			template <typename T>
			GAIA_NODISCARD constexpr serialization_type_id get_integral_type() {
				if constexpr (std::is_same_v<int8_t, T> || std::is_same_v<signed char, T>) {
					return serialization_type_id::s8;
				} else if constexpr (std::is_same_v<uint8_t, T> || std::is_same_v<unsigned char, T>) {
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
				} else if constexpr (std::is_same_v<bool, T>) {
					return serialization_type_id::b;
				}

				static_assert("Unsupported integral type");
			}

			template <typename T>
			GAIA_NODISCARD constexpr serialization_type_id get_floating_point_type() {
				// if constexpr (std::is_same_v<float8_t, T>) {
				// 	return serialization_type_id::f8;
				// } else if constexpr (std::is_same_v<float16_t, T>) {
				// 	return serialization_type_id::f16;
				// } else
				if constexpr (std::is_same_v<float, T>) {
					return serialization_type_id::f32;
				} else if constexpr (std::is_same_v<double, T>) {
					return serialization_type_id::f64;
				} else if constexpr (std::is_same_v<long double, T>) {
					return serialization_type_id::f128;
				}

				static_assert("Unsupported floating point type");
				return serialization_type_id::Last;
			}

			template <typename T>
			GAIA_NODISCARD constexpr serialization_type_id get_type_id() {
				if constexpr (std::is_enum_v<T>)
					return get_integral_type<std::underlying_type_t<T>>();
				else if constexpr (std::is_integral_v<T>)
					return get_integral_type<T>();
				else if constexpr (std::is_floating_point_v<T>)
					return get_floating_point_type<T>();
				else if constexpr (detail::has_data_and_size<T>::value)
					return serialization_type_id::data_and_size;
				else if constexpr (std::is_class_v<T>)
					return serialization_type_id::trivial_wrapper;

				static_assert("Unsupported serialization type");
				return serialization_type_id::Last;
			}

			template <typename T>
			GAIA_NODISCARD constexpr uint32_t size_bytes_one(const T& item) noexcept {
				using type = typename std::decay_t<typename std::remove_pointer_t<T>>;

				constexpr auto id = detail::get_type_id<type>();
				static_assert(id != detail::serialization_type_id::Last);
				uint32_t size_in_bytes{};

				// Custom size_bytes() has precedence
				if constexpr (has_size_bytes<type>::value) {
					size_in_bytes = (uint32_t)item.size_bytes();
				}
				// Trivially serializable types
				else if constexpr (is_trivially_serializable<type>::value) {
					size_in_bytes = (uint32_t)sizeof(type);
				}
				// Types which have data() and size() member functions
				else if constexpr (detail::has_data_and_size<type>::value) {
					size_in_bytes = (uint32_t)item.size();
				}
				// Classes
				else if constexpr (std::is_class_v<type>) {
					utils::for_each_member(item, [&](auto&&... items) {
						size_in_bytes += (size_bytes_one(items) + ...);
					});
				} else
					static_assert(!sizeof(type), "Type is not supported for serialization, yet");

				return size_in_bytes;
			}

			template <bool Write, typename Serializer, typename T>
			void serialize_data_one(Serializer& s, T&& arg) {
				using type = typename std::decay_t<typename std::remove_pointer_t<T>>;

				// Custom save() & load() have precedence
				if constexpr (Write && has_save<type, Serializer&>::value) {
					arg.save(s);
				} else if constexpr (!Write && has_load<type, Serializer&>::value) {
					arg.load(s);
				}
				// Trivially serializable types
				else if constexpr (is_trivially_serializable<type>::value) {
					if constexpr (Write)
						s.save(std::forward<T>(arg));
					else
						s.load(std::forward<T>(arg));
				}
				// Types which have data() and size() member functions
				else if constexpr (detail::has_data_and_size<type>::value) {
					if constexpr (Write) {
						if constexpr (has_resize<type>::value) {
							const auto size = arg.size();
							s.save(size);
						}
						for (const auto& e: arg)
							serialize_data_one<Write>(s, e);
					} else {
						if constexpr (has_resize<type>::value) {
							auto size = arg.size();
							s.load(size);
							arg.resize(size);
						}
						for (auto& e: arg)
							serialize_data_one<Write>(s, e);
					}
				}
				// Classes
				else if constexpr (std::is_class_v<type>) {
					utils::for_each_member(std::forward<T>(arg), [&s](auto&&... items) {
						// TODO: Handle contiguous blocks of trivially copiable types
						(serialize_data_one<Write>(s, items), ...);
					});
				} else
					static_assert(!sizeof(type), "Type is not supported for serialization, yet");
			}
		} // namespace detail

		//! Calculates the number of bytes necessary to serialize data using the "save" function.
		//! \warning Compile-time.
		template <typename T>
		GAIA_NODISCARD uint32_t size_bytes(const T& data) {
			return detail::size_bytes_one(data);
		}

		//! Write \param data using \tparam Writer at compile-time.
		//!
		//! \warning Writer has to implement a save function as follows:
		//! 					template <typename T> void save(const T& arg);
		template <typename Writer, typename T>
		void save(Writer& writer, const T& data) {
			detail::serialize_data_one<true>(writer, data);
		}

		//! Read \param data using \tparam Reader at compile-time.
		//!
		//! \warning Reader has to implement a save function as follows:
		//! 					template <typename T> void load(T& arg);
		template <typename Reader, typename T>
		void load(Reader& reader, T& data) {
			detail::serialize_data_one<false>(reader, data);
		}
	} // namespace serialization
} // namespace gaia