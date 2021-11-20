#pragma once
#include <tuple>
#include <type_traits>
#include <utility>

#include "../utils/span.h"
#include "reflection.h"
#include "utils_mem.h"

namespace gaia {
	namespace utils {
		enum class DataLayout {
			AoS, //< Array Of Structures
			SoA, //< Structure Of Arrays
		};

		// Helper templates
		namespace detail {

#pragma region "Byte offset of a member of SoA-organized data"

			template <size_t N, size_t Alignment, typename Tuple>
			constexpr static size_t soa_byte_offset(
					const uintptr_t address, [[maybe_unused]] const size_t size) {
				if constexpr (N == 0) {
					return utils::align<Alignment>(address) - address;
				} else {
					const auto offset = utils::align<Alignment>(address) - address;
					using tt = typename std::tuple_element<N - 1, Tuple>::type;
					return sizeof(tt) * size + offset +
								 soa_byte_offset<N - 1, Alignment, Tuple>(address, size);
				}
			}

#pragma endregion
		} // namespace detail

		template <DataLayout TDataLayout, typename TItem>
		struct data_view_policy;

		/*!
		 * data_view_policy for accessing and storing data in the AoS way
		 *	Good for random access and when acessing data together.
		 *
		 * struct Foo { int x; int y; int z; };
		 * using fooViewPolicy = data_view_policy<DataLayout::AoS, Foo>;
		 *
		 * Memory is going be be organized as:
		 *		xyz xyz xyz xyz
		 */
		template <typename ValueType>
		struct data_view_policy<DataLayout::AoS, ValueType> {
			constexpr static DataLayout Layout = DataLayout::AoS;
			constexpr static size_t Alignment = alignof(ValueType);

			constexpr static ValueType get(std::span<const ValueType> s, size_t idx) {
				return get_internal((const ValueType*)s.data(), idx);
			}

			constexpr static void
			set(std::span<ValueType> s, size_t idx, ValueType&& val) {
				set_internal((ValueType*)s.data(), idx, std::forward<ValueType>(val));
			}

		private:
			[[nodiscard]] constexpr static ValueType
			get_internal(const ValueType* data, const size_t idx) {
				return data[idx];
			}

			constexpr static void
			set_internal(ValueType* data, const size_t idx, ValueType&& val) {
				data[idx] = std::forward<ValueType>(val);
			}
		};

		template <typename ValueType>
		using aos_view_policy = data_view_policy<DataLayout::AoS, ValueType>;

		/*!
		 * data_view_policy for accessing and storing data in the SoA way
		 *	Good for SIMD processing.
		 *
		 * struct Foo { int x; int y; int z; };
		 * using fooViewPolicy = data_view_policy<DataLayout::SoA, Foo>;
		 *
		 * Memory is going be be organized as:
		 *		xxxx yyyy zzzz
		 */
		template <typename ValueType>
		struct data_view_policy<DataLayout::SoA, ValueType> {
			constexpr static DataLayout Layout = DataLayout::SoA;
			constexpr static size_t Alignment = 16;

			constexpr static ValueType
			get(std::span<const ValueType> s, const size_t idx) {
				auto t = struct_to_tuple(ValueType{});
				return get_internal(
						t, s, idx,
						std::make_integer_sequence<
								size_t, std::tuple_size<decltype(t)>::value>());
			}

			template <size_t Ids>
			constexpr static auto
			get(std::span<const ValueType> s, const size_t idx = 0) {
				using Tuple = decltype(struct_to_tuple(ValueType{}));
				using MemberType = typename std::tuple_element<Ids, Tuple>::type;
				const auto* ret = (const char*)s.data() + idx * sizeof(MemberType) +
													detail::soa_byte_offset<Ids, Alignment, Tuple>(
															(uintptr_t)s.data(), s.size());
				return std::span{(const MemberType*)ret, s.size() - idx};
			}

			constexpr static void
			set(std::span<ValueType> s, const size_t idx, ValueType&& val) {
				auto t = struct_to_tuple(std::forward<ValueType>(val));
				set_internal(
						t, s, idx,
						std::make_integer_sequence<
								size_t, std::tuple_size<decltype(t)>::value>());
			}

			template <size_t Ids>
			constexpr static auto set(std::span<ValueType> s, const size_t idx = 0) {
				using Tuple = decltype(struct_to_tuple(ValueType{}));
				using MemberType = typename std::tuple_element<Ids, Tuple>::type;
				auto* ret = (char*)s.data() + idx * sizeof(MemberType) +
										detail::soa_byte_offset<Ids, Alignment, Tuple>(
												(uintptr_t)s.data(), s.size());
				return std::span{(MemberType*)ret, s.size() - idx};
			}

		private:
			template <typename Tuple, size_t... Ids>
			[[nodiscard]] constexpr static ValueType get_internal(
					Tuple& t, std::span<const ValueType> s, const size_t idx,
					std::integer_sequence<size_t, Ids...>) {
				(get_internal<
						 Tuple, Ids, typename std::tuple_element<Ids, Tuple>::type>(
						 t, (const char*)s.data(),
						 idx * sizeof(typename std::tuple_element<Ids, Tuple>::type) +
								 detail::soa_byte_offset<Ids, Alignment, Tuple>(
										 (uintptr_t)s.data(), s.size())),
				 ...);
				return tuple_to_struct<ValueType, Tuple>(std::forward<Tuple>(t));
			}

			template <typename Tuple, size_t Ids, typename TMemberType>
			constexpr static void
			get_internal(Tuple& t, const char* data, const size_t idx) {
				std::get<Ids>(t) = *(TMemberType*)&data[idx];
			}

			template <typename Tuple, typename TValue, size_t... Ids>
			constexpr static void set_internal(
					Tuple& t, std::span<TValue> s, const size_t idx,
					std::integer_sequence<size_t, Ids...>) {
				(set_internal(
						 (char*)s.data(),
						 idx * sizeof(typename std::tuple_element<Ids, Tuple>::type) +
								 detail::soa_byte_offset<Ids, Alignment, Tuple>(
										 (uintptr_t)s.data(), s.size()),
						 std::get<Ids>(t)),
				 ...);
			}

			template <typename MemberType>
			constexpr static void
			set_internal(char* data, const size_t idx, MemberType val) {
				unaligned_ref<MemberType> writer((void*)&data[idx]);
				writer = val;
			}
		};

		template <typename ValueType>
		using soa_view_policy = data_view_policy<DataLayout::SoA, ValueType>;

#pragma region Helpers

		template <typename, typename = void>
		struct is_soa_layout: std::false_type {};

		template <typename T>
		struct is_soa_layout<
				T, typename std::enable_if<T::Layout == DataLayout::SoA>::type>:
				std::true_type {};

		template <typename T>
		using auto_view_policy = std::conditional_t<
				is_soa_layout<T>::value, data_view_policy<DataLayout::SoA, T>,
				data_view_policy<DataLayout::AoS, T>>;

#pragma endregion
	} // namespace utils
} // namespace gaia
