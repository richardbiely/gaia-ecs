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
			SoA, // Structure Of Arrays
			AoS // Array Of Structures
		};

		// Helper templates
		namespace detail {

#pragma region "Byte offset of a member of SoA-organized data"

			constexpr static uint32_t SoADataAlignment = 16;

			template <uint32_t N, typename Tuple>
			constexpr static uint32_t
			soa_byte_offset(const uintptr_t address, const uint32_t size) {
				if constexpr (N == 0) {
					// Handle alignment to SoADataAlignment bytes for SSE
					return (uint32_t)(utils::align<SoADataAlignment>(address) - address);
				} else {
					// Handle alignment to SoADataAlignment bytes for SSE
					const auto offset =
							(uint32_t)(utils::align<SoADataAlignment>(address) - address);
					return sizeof(typename std::tuple_element<N - 1, Tuple>::type) *
										 size +
								 offset + soa_byte_offset<N - 1, Tuple>(address, size);
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

			constexpr static ValueType get(std::span<ValueType> s, uint32_t idx) {
				return get_internal((const ValueType*)s.data(), idx);
			}

			constexpr static void
			set(std::span<ValueType> s, uint32_t idx, ValueType&& val) {
				set_internal((ValueType*)s.data(), idx, std::forward<ValueType>(val));
			}

		private:
			constexpr static ValueType
			get_internal(const ValueType* data, const uint32_t idx) {
				return data[idx];
			}

			constexpr static void
			set_internal(ValueType* data, const uint32_t idx, ValueType&& val) {
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

			constexpr static ValueType
			get(std::span<ValueType> s, const uint32_t idx) {
				auto t = struct_to_tuple(ValueType{});
				return get_internal(
						t, s, idx,
						std::make_integer_sequence<
								size_t, std::tuple_size<decltype(t)>::value>());
			}

			constexpr static void
			set(std::span<ValueType> s, const uint32_t idx, ValueType&& val) {
				auto t = struct_to_tuple(std::forward<ValueType>(val));
				set_internal(
						t, s, idx,
						std::make_integer_sequence<
								size_t, std::tuple_size<decltype(t)>::value>());
			}

		private:
			template <typename Tuple, size_t... Ids>
			constexpr static ValueType get_internal(
					Tuple& t, std::span<ValueType> s, const uint32_t idx,
					std::integer_sequence<size_t, Ids...>) {
				(get_internal<
						 Tuple, Ids, typename std::tuple_element<Ids, Tuple>::type>(
						 t, (const char*)s.data(),
						 idx * sizeof(typename std::tuple_element<Ids, Tuple>::type) +
								 detail::soa_byte_offset<Ids, Tuple>(
										 (uintptr_t)s.data(), s.size())),
				 ...);
				return tuple_to_struct<ValueType, Tuple>(std::forward<Tuple>(t));
			}

			template <typename Tuple, size_t Ids, typename TMemberType>
			constexpr static void
			get_internal(Tuple& t, const char* data, const uint32_t idx) {
				std::get<Ids>(t) = *(TMemberType*)&data[idx];
			}

			template <typename Tuple, typename TValue, size_t... Ids>
			constexpr static void set_internal(
					Tuple& t, std::span<TValue> s, const uint32_t idx,
					std::integer_sequence<size_t, Ids...>) {
				(set_internal(
						 (char*)s.data(),
						 idx * sizeof(typename std::tuple_element<Ids, Tuple>::type) +
								 detail::soa_byte_offset<Ids, Tuple>(
										 (uintptr_t)s.data(), s.size()),
						 std::get<Ids>(t)),
				 ...);
			}

			template <typename TMemberValue>
			constexpr static void
			set_internal(char* data, const uint32_t idx, TMemberValue val) {
				// memcpy((void*)&data[idx], (const void*)&val, sizeof(val));
				*(TMemberValue*)&data[idx] = val;
			}
		};

		template <typename ValueType>
		using soa_view_policy = data_view_policy<DataLayout::SoA, ValueType>;

	} // namespace utils
} // namespace gaia
