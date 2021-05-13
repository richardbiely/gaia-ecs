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

			constexpr static unsigned SoADataAlignment = 16;

			template <unsigned N, typename Tuple>
			constexpr static unsigned
			soa_byte_offset(const uintptr_t address, const unsigned size) {
					if constexpr (N == 0) {
						// Handle alignment to SoADataAlignment bytes for SSE
						return (
								unsigned)(utils::align<SoADataAlignment>(address) - address);
					} else {
						// Handle alignment to SoADataAlignment bytes for SSE
						const auto offset =
								(unsigned)(utils::align<SoADataAlignment>(address) - address);
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
			constexpr static ValueType get(std::span<ValueType> s, unsigned idx) {
				return get_internal((const ValueType*)s.data(), idx);
			}
			constexpr static void
			set(std::span<ValueType> s, unsigned idx, ValueType&& val) {
				set_internal((ValueType*)s.data(), idx, std::forward<ValueType>(val));
			}

		private:
			constexpr static ValueType
			get_internal(const ValueType* data, const unsigned idx) {
				return data[idx];
			}
			constexpr static void
			set_internal(ValueType* data, const unsigned idx, ValueType&& val) {
				data[idx] = std::forward<ValueType>(val);
			}
		};

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
			constexpr static ValueType
			get(std::span<ValueType> s, const unsigned idx) {
				auto t = struct_to_tuple(ValueType{});
				return get_internal(
						t, s, idx,
						std::make_integer_sequence<
								unsigned, std::tuple_size<decltype(t)>::value>());
			}

			constexpr static void
			set(std::span<ValueType> s, const unsigned idx, ValueType&& val) {
				auto t = struct_to_tuple(std::forward<ValueType>(val));
				set_internal(
						t, s, idx,
						std::make_integer_sequence<
								unsigned, std::tuple_size<decltype(t)>::value>(),
						std::forward<ValueType>(val));
			}

		private:
			template <typename Tuple, unsigned... Ids>
			constexpr static ValueType get_internal(
					Tuple& t, std::span<ValueType> s, const unsigned idx,
					std::integer_sequence<unsigned, Ids...>) {
				(get_internal<
						 Tuple, Ids, typename std::tuple_element<Ids, Tuple>::type>(
						 t, (const char*)s.data(),
						 idx * sizeof(typename std::tuple_element<Ids, Tuple>::type) +
								 detail::soa_byte_offset<Ids, Tuple>(
										 (uintptr_t)s.data(), s.size())),
				 ...);
				return tuple_to_struct<ValueType, Tuple>(std::forward<Tuple>(t));
			}

			template <typename Tuple, unsigned Ids, typename TMemberType>
			constexpr static void
			get_internal(Tuple& t, const char* data, const unsigned idx) {
				std::get<Ids>(t) = *(TMemberType*)&data[idx];
			}

			template <typename Tuple, typename TValue, unsigned... Ids>
			constexpr static void set_internal(
					Tuple& t, std::span<TValue> s, const unsigned idx,
					std::integer_sequence<unsigned, Ids...>, TValue&& val) {
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
			set_internal(char* data, const unsigned idx, TMemberValue val) {
				// memcpy((void*)&data[idx], (const void*)&val, sizeof(val));
				*(TMemberValue*)&data[idx] = val;
			}
		};
	} // namespace utils
} // namespace gaia
