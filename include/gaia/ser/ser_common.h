#pragma once
#include "gaia/config/config.h"

#include <cstdint>
#include <type_traits>

#include "gaia/core/utility.h"

namespace gaia {
	namespace ser {
		//! Runtime identifier describing the representation and alignment of serialized data.
		enum class serialization_type_id : uint8_t {
			//! Value is intentionally excluded from serialization.
			ignore = 0,

			//! Signed 8-bit integer.
			s8 = 1,
			//! Unsigned 8-bit integer.
			u8 = 2,
			//! Signed 16-bit integer.
			s16 = 3,
			//! Unsigned 16-bit integer.
			u16 = 4,
			//! Signed 32-bit integer.
			s32 = 5,
			//! Unsigned 32-bit integer.
			u32 = 6,
			//! Signed 64-bit integer.
			s64 = 7,
			//! Unsigned 64-bit integer.
			u64 = 8,

			//! Boolean value.
			b = 9,

			//! 8-bit character.
			c8 = 10,
			//! 16-bit character.
			c16 = 11,
			//! 32-bit character.
			c32 = 12,

			//! Reserved 8-bit floating-point representation.
			f8 = 13,
			//! Reserved 16-bit floating-point representation.
			f16 = 14,
			//! 32-bit floating-point value.
			f32 = 15,
			//! 64-bit floating-point value.
			f64 = 16,

			//! First identifier whose byte size depends on additional metadata.
			special_begin = 17,
			//! Trivially copied wrapper whose size is supplied by the caller.
			trivial_wrapper = special_begin,
			//! Pointer-like or contiguous value represented by data and size.
			data_and_size = 18,

			//! Highest valid serialization identifier.
			Last = data_and_size,
		};

		//! Resolves the storage size associated with a serialization identifier.
		//! \param id Serialization representation to inspect.
		//! \param size Caller-provided size used for trivial wrappers.
		//! \return Required size and alignment unit in bytes.
		inline uint32_t serialization_type_size(serialization_type_id id, uint32_t size) {
			static constexpr uint32_t sizes[] = {
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

					// Floating point types
					1, // f8
					2, // f16
					4, // f32
					8, // f64

					// Special
					(uint32_t)-1, // trivial_wrapper, resolved from the size argument
					sizeof(uintptr_t), // data_and_size, assume natural alignment
			};

			const auto s = id == serialization_type_id::trivial_wrapper ? size : sizes[(uint32_t)id];
			GAIA_ASSERT(s != (uint32_t)-1);
			return s;
		}

		//! Reports whether a type may be serialized by copying its representation.
		//! \tparam T Type to inspect.
		template <typename T>
		struct is_trivially_serializable {
		private:
			static constexpr bool update() {
				return std::is_enum_v<T> || std::is_fundamental_v<T> || std::is_trivially_copyable_v<T>;
			}

		public:
			//! True when the type can be serialized trivially.
			static constexpr bool value = update();
		};

		template <typename T>
		struct is_int_kind_id:
				std::disjunction<
						std::is_same<T, char>, std::is_same<T, unsigned char>, //
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

		//! Returns the serialization identifier for an integral type.
		//! \tparam T Supported integral type.
		//! \return Matching integral serialization identifier.
		template <typename T>
		GAIA_NODISCARD constexpr serialization_type_id int_kind_id() {
			static_assert(is_int_kind_id<T>::value, "Unsupported integral type");

			if constexpr (std::is_same_v<char, T>) {
				return serialization_type_id::s8;
			} else if constexpr (std::is_same_v<unsigned char, T>) {
				return serialization_type_id::u8;
			} else if constexpr (std::is_same_v<int8_t, T>) {
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

		//! Returns the serialization identifier for a floating-point type.
		//! \tparam T Supported floating-point type.
		//! \return Matching floating-point serialization identifier.
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
			} else { // if constexpr (std::is_same_v<double, T>) {
				return serialization_type_id::f64;
			}
		}

		//! Returns the serialization identifier selected for a C++ type.
		//! \tparam T Type to classify.
		//! \return Matching serialization identifier, or ignore when unsupported.
		template <typename T>
		GAIA_NODISCARD constexpr serialization_type_id type_id() {
			if constexpr (std::is_enum_v<T>)
				return int_kind_id<std::underlying_type_t<T>>();
			else if constexpr (std::is_integral_v<T>)
				return int_kind_id<T>();
			else if constexpr (std::is_floating_point_v<T>)
				return flt_type_id<T>();
			else if constexpr (std::is_pointer_v<T>)
				return serialization_type_id::data_and_size;
			else if constexpr (core::has_size_begin_end<T>::value)
				return serialization_type_id::data_and_size;
			else if constexpr (std::is_class_v<T>)
				return serialization_type_id::trivial_wrapper;
			else
				return serialization_type_id::ignore;
		}

		//! \cond INTERNAL

		GAIA_DEFINE_HAS_MEMBER_FUNC(save);
		GAIA_DEFINE_HAS_MEMBER_FUNC(load);
		GAIA_DEFINE_HAS_MEMBER_FUNC(resize);

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
		//! \endcond
	} // namespace ser
} // namespace gaia
