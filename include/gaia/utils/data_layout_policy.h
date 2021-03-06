#pragma once
#include <tuple>
#include <type_traits>
#include <utility>

#include "../utils/span.h"
#include "mem.h"
#include "reflection.h"

namespace gaia {
	namespace utils {
		enum class DataLayout {
			AoS, //< Array Of Structures
			SoA, //< Structure Of Arrays
		};

		// Helper templates
		namespace detail {

			//----------------------------------------------------------------------
			// Byte offset of a member of SoA-organized data
			//----------------------------------------------------------------------

			template <size_t N, size_t Alignment, typename Tuple>
			constexpr static size_t soa_byte_offset(const uintptr_t address, [[maybe_unused]] const size_t size) {
				if constexpr (N == 0) {
					return utils::align<Alignment>(address) - address;
				} else {
					const auto offset = utils::align<Alignment>(address) - address;
					using tt = typename std::tuple_element<N - 1, Tuple>::type;
					return sizeof(tt) * size + offset + soa_byte_offset<N - 1, Alignment, Tuple>(address, size);
				}
			}

		} // namespace detail

		template <DataLayout TDataLayout>
		struct data_layout_properties;

		template <DataLayout TDataLayout, typename TItem>
		struct data_view_policy;

		template <DataLayout TDataLayout, typename TItem>
		struct data_view_policy_get;
		template <DataLayout TDataLayout, typename TItem>
		struct data_view_policy_set;

		template <DataLayout TDataLayout, typename TItem, size_t Ids>
		struct data_view_policy_get_idx;
		template <DataLayout TDataLayout, typename TItem, size_t Ids>
		struct data_view_policy_set_idx;

		template <>
		struct data_layout_properties<DataLayout::AoS> {
			constexpr static uint32_t PackSize = 1;
		};
		template <>
		struct data_layout_properties<DataLayout::SoA> {
			constexpr static uint32_t PackSize = 4;
		};

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

			[[nodiscard]] constexpr static ValueType getc(std::span<const ValueType> s, size_t idx) {
				return s[idx];
			}

			[[nodiscard]] constexpr static ValueType get(std::span<ValueType> s, size_t idx) {
				return s[idx];
			}

			[[nodiscard]] constexpr static const ValueType& getc_constref(std::span<const ValueType> s, size_t idx) {
				return (const ValueType&)s[idx];
			}

			[[nodiscard]] constexpr static const ValueType& get_constref(std::span<ValueType> s, size_t idx) {
				return (const ValueType&)s[idx];
			}

			[[nodiscard]] constexpr static ValueType& get_ref(std::span<ValueType> s, size_t idx) {
				return s[idx];
			}

			constexpr static void set(std::span<ValueType> s, size_t idx, ValueType&& val) {
				s[idx] = std::forward<ValueType>(val);
			}
		};

		template <typename ValueType>
		struct data_view_policy_get<DataLayout::AoS, ValueType> {
			//! Raw data pointed to by the view policy
			std::span<const ValueType> m_data;

			using view_policy = data_view_policy<DataLayout::AoS, ValueType>;

			[[nodiscard]] const ValueType& operator[](size_t idx) const {
				return view_policy::getc_constref(m_data, idx);
			}

			[[nodiscard]] const ValueType* data() const {
				return m_data.data();
			}

			[[nodiscard]] auto view() const {
				return m_data;
			}
		};

		template <typename ValueType>
		struct data_view_policy_set<DataLayout::AoS, ValueType> {
			//! Raw data pointed to by the view policy
			std::span<ValueType> m_data;

			using view_policy = data_view_policy<DataLayout::AoS, ValueType>;

			[[nodiscard]] ValueType& operator[](size_t idx) {
				return view_policy::get_ref(m_data, idx);
			}

			[[nodiscard]] const ValueType& operator[](size_t idx) const {
				return view_policy::getc_constref(m_data, idx);
			}

			[[nodiscard]] ValueType* data() const {
				return m_data.data();
			}

			[[nodiscard]] auto view() const {
				return m_data;
			}
		};

		template <typename ValueType>
		using aos_view_policy = data_view_policy<DataLayout::AoS, ValueType>;
		template <typename ValueType>
		using aos_view_policy_get = data_view_policy_get<DataLayout::AoS, ValueType>;
		template <typename ValueType>
		using aos_view_policy_set = data_view_policy_get<DataLayout::AoS, ValueType>;

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

			template <size_t Ids>
			using value_type = typename std::tuple_element<Ids, decltype(struct_to_tuple(ValueType{}))>::type;
			template <size_t Ids>
			using const_value_type = typename std::add_const<value_type<Ids>>::type;

			[[nodiscard]] constexpr static ValueType get(std::span<const ValueType> s, const size_t idx) {
				auto t = struct_to_tuple(ValueType{});
				return get_internal(t, s, idx, std::make_integer_sequence<size_t, std::tuple_size<decltype(t)>::value>());
			}

			template <size_t Ids>
			[[nodiscard]] constexpr static auto get(std::span<const ValueType> s, const size_t idx = 0) {
				using Tuple = decltype(struct_to_tuple(ValueType{}));
				using MemberType = typename std::tuple_element<Ids, Tuple>::type;
				const auto* ret = (const uint8_t*)s.data() + idx * sizeof(MemberType) +
													detail::soa_byte_offset<Ids, Alignment, Tuple>((uintptr_t)s.data(), s.size());
				return std::span{(const MemberType*)ret, s.size() - idx};
			}

			constexpr static void set(std::span<ValueType> s, const size_t idx, ValueType&& val) {
				auto t = struct_to_tuple(std::forward<ValueType>(val));
				set_internal(t, s, idx, std::make_integer_sequence<size_t, std::tuple_size<decltype(t)>::value>());
			}

			template <size_t Ids>
			constexpr static auto set(std::span<ValueType> s, const size_t idx = 0) {
				using Tuple = decltype(struct_to_tuple(ValueType{}));
				using MemberType = typename std::tuple_element<Ids, Tuple>::type;
				auto* ret = (uint8_t*)s.data() + idx * sizeof(MemberType) +
										detail::soa_byte_offset<Ids, Alignment, Tuple>((uintptr_t)s.data(), s.size());
				return std::span{(MemberType*)ret, s.size() - idx};
			}

		private:
			template <typename Tuple, size_t... Ids>
			[[nodiscard]] constexpr static ValueType
			get_internal(Tuple& t, std::span<const ValueType> s, const size_t idx, std::integer_sequence<size_t, Ids...>) {
				(get_internal<Tuple, Ids, typename std::tuple_element<Ids, Tuple>::type>(
						 t, (const uint8_t*)s.data(),
						 idx * sizeof(typename std::tuple_element<Ids, Tuple>::type) +
								 detail::soa_byte_offset<Ids, Alignment, Tuple>((uintptr_t)s.data(), s.size())),
				 ...);
				return tuple_to_struct<ValueType, Tuple>(std::forward<Tuple>(t));
			}

			template <typename Tuple, size_t Ids, typename TMemberType>
			constexpr static void get_internal(Tuple& t, const uint8_t* data, const size_t idx) {
				unaligned_ref<TMemberType> reader((void*)&data[idx]);
				std::get<Ids>(t) = reader;
			}

			template <typename Tuple, typename TValue, size_t... Ids>
			constexpr static void
			set_internal(Tuple& t, std::span<TValue> s, const size_t idx, std::integer_sequence<size_t, Ids...>) {
				(set_internal(
						 (uint8_t*)s.data(),
						 idx * sizeof(typename std::tuple_element<Ids, Tuple>::type) +
								 detail::soa_byte_offset<Ids, Alignment, Tuple>((uintptr_t)s.data(), s.size()),
						 std::get<Ids>(t)),
				 ...);
			}

			template <typename MemberType>
			constexpr static void set_internal(uint8_t* data, const size_t idx, MemberType val) {
				unaligned_ref<MemberType> writer((void*)&data[idx]);
				writer = val;
			}
		};

		template <typename ValueType>
		struct data_view_policy_get<DataLayout::SoA, ValueType> {
			using view_policy = data_view_policy<DataLayout::SoA, ValueType>;

			template <size_t Ids>
			struct data_view_policy_idx_info {
				using const_value_type = typename view_policy::template const_value_type<Ids>;
			};

			//! Raw data pointed to by the view policy
			std::span<const ValueType> m_data;

			[[nodiscard]] constexpr auto operator[](size_t idx) const {
				return view_policy::get(m_data, idx);
			}

			template <size_t Ids>
			[[nodiscard]] constexpr auto get() const {
				return std::span<typename data_view_policy_idx_info<Ids>::const_value_type>(
						view_policy::template get<Ids>(m_data).data(), view_policy::template get<Ids>(m_data).size());
			}

			[[nodiscard]] const ValueType* data() const {
				return m_data.data();
			}

			[[nodiscard]] auto view() const {
				return m_data;
			}
		};

		template <typename ValueType>
		struct data_view_policy_set<DataLayout::SoA, ValueType> {
			using view_policy = data_view_policy<DataLayout::SoA, ValueType>;

			template <size_t Ids>
			struct data_view_policy_idx_info {
				using value_type = typename view_policy::template value_type<Ids>;
				using const_value_type = typename view_policy::template const_value_type<Ids>;
			};

			//! Raw data pointed to by the view policy
			std::span<ValueType> m_data;

			struct setter {
				const std::span<ValueType>& m_data;
				const size_t m_idx;

				constexpr setter(const std::span<ValueType>& data, const size_t idx): m_data(data), m_idx(idx) {}
				constexpr void operator=(ValueType&& val) {
					view_policy::set(m_data, m_idx, std::forward<ValueType>(val));
				}
			};

			[[nodiscard]] constexpr auto operator[](size_t idx) const {
				return view_policy::get(m_data, idx);
			}
			[[nodiscard]] constexpr auto operator[](size_t idx) {
				return setter(m_data, idx);
			}

			template <size_t Ids>
			[[nodiscard]] constexpr auto get() const {
				using value_type = typename data_view_policy_idx_info<Ids>::const_value_type;
				const std::span<const ValueType> data((const ValueType*)m_data.data(), m_data.size());
				return std::span<value_type>(
						view_policy::template get<Ids>(data).data(), view_policy::template get<Ids>(data).size());
			}

			template <size_t Ids>
			[[nodiscard]] constexpr auto set() {
				return std::span<typename data_view_policy_idx_info<Ids>::value_type>(
						view_policy::template set<Ids>(m_data).data(), view_policy::template set<Ids>(m_data).size());
			}

			[[nodiscard]] ValueType* data() const {
				return m_data.data();
			}

			[[nodiscard]] auto view() const {
				return m_data;
			}
		};

		template <typename ValueType>
		using soa_view_policy = data_view_policy<DataLayout::SoA, ValueType>;
		template <typename ValueType>
		using soa_view_policy_get = data_view_policy_get<DataLayout::SoA, ValueType>;
		template <typename ValueType>
		using soa_view_policy_set = data_view_policy_get<DataLayout::SoA, ValueType>;

		//----------------------------------------------------------------------
		// Helpers
		//----------------------------------------------------------------------

		template <typename, typename = void>
		struct is_soa_layout: std::false_type {};

		template <typename T>
		struct is_soa_layout<T, typename std::enable_if<T::Layout == DataLayout::SoA>::type>: std::true_type {};

		template <typename T>
		using auto_view_policy = std::conditional_t<
				is_soa_layout<T>::value, data_view_policy<DataLayout::SoA, T>, data_view_policy<DataLayout::AoS, T>>;
		template <typename T>
		using auto_view_policy_get = std::conditional_t<
				is_soa_layout<T>::value, data_view_policy_get<DataLayout::SoA, T>, data_view_policy_get<DataLayout::AoS, T>>;
		template <typename T>
		using auto_view_policy_set = std::conditional_t<
				is_soa_layout<T>::value, data_view_policy_set<DataLayout::SoA, T>, data_view_policy_set<DataLayout::AoS, T>>;

	} // namespace utils
} // namespace gaia
