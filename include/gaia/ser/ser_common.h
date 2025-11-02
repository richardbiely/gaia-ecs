#pragma once
#include "../config/config.h"

#include <cstdint>
#include <type_traits>

#include "../core/utility.h"

namespace gaia {
	namespace ser {
		enum class serialization_type_id : uint8_t {
			// Dummy
			ignore = 0,

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
			b = 9,

			// Character types
			c8 = 10,
			c16 = 11,
			c32 = 12,
			cw = 13,

			// Floating point types
			f8 = 14,
			f16 = 15,
			f32 = 16,
			f64 = 17,
			f128 = 18,

			// Special
			special_begin = 19,
			trivial_wrapper = special_begin,
			data_and_size = 20,

			Last = data_and_size,
		};

		inline uint32_t serialization_type_size(serialization_type_id id, uint32_t size) {
			static const uint32_t sizes[] = {
					// Dummy
					0, // ignore

					// Integer types
					1, // s8
					1, // u8
					2, // s16
					2, // u16
					4, // s32
					4, // u32
					8, // s64
					8, // u64

					// Boolean
					1, // b

					// Character types
					1, // c8
					2, // c16
					4, // c32
					8, // cw

					// Floating point types
					1, // f8
					2, // f16
					4, // f32
					8, // f64
					16, // f128

					// Special
					size, // trivial_wrapper
					sizeof(uintptr_t), // data_and_size, assume natural alignment
			};

			const auto s = sizes[(uint32_t)id];
			// Make sure we do not return an invalid value
			GAIA_ASSERT(s != (uint32_t)-1);
			return s;
		}

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
						std::is_same<T, size_t>, std::is_same<T, bool>> {};

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
			} else if constexpr (std::is_same_v<size_t, T>) {
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

		// --------------------
		// Define function detectors
		// --------------------

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
	} // namespace ser
} // namespace gaia
