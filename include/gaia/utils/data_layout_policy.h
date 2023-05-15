#pragma once
#include "../config/config.h"

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
			SoA, //< Structure Of Arrays, 4 packed items, good for SSE and similar
			SoA8, //< Structure Of Arrays, 8 packed items, good for AVX and similar
			SoA16 //< Structure Of Arrays, 16 packed items, good for AVX512 and similar
		};

		// Helper templates
		namespace detail {

			//----------------------------------------------------------------------
			// Byte offset of a member of SoA-organized data
			//----------------------------------------------------------------------

			template <size_t N, size_t Alignment, typename Tuple>
			constexpr static size_t soa_byte_offset(const uintptr_t address, [[maybe_unused]] const size_t size) {
				const auto addressAligned = utils::align<Alignment>(address) - address;
				if constexpr (N == 0) {
					return addressAligned;
				} else {
					using tt = typename std::tuple_element<N - 1, Tuple>::type;
					return addressAligned + sizeof(tt) * size + soa_byte_offset<N - 1, Alignment, Tuple>(address, size);
				}
			}

		} // namespace detail

		template <DataLayout TDataLayout, typename TItem>
		struct data_layout_properties;
		template <typename TItem>
		struct data_layout_properties<DataLayout::AoS, TItem> {
			constexpr static DataLayout Layout = DataLayout::AoS;
			constexpr static size_t PackSize = 1;
			constexpr static size_t Alignment = alignof(TItem);
		};
		template <typename TItem>
		struct data_layout_properties<DataLayout::SoA, TItem> {
			constexpr static DataLayout Layout = DataLayout::SoA;
			constexpr static size_t PackSize = 4;
			constexpr static size_t Alignment = PackSize * 4;
		};
		template <typename TItem>
		struct data_layout_properties<DataLayout::SoA8, TItem> {
			constexpr static DataLayout Layout = DataLayout::SoA8;
			constexpr static size_t PackSize = 8;
			constexpr static size_t Alignment = PackSize * 4;
		};
		template <typename TItem>
		struct data_layout_properties<DataLayout::SoA16, TItem> {
			constexpr static DataLayout Layout = DataLayout::SoA16;
			constexpr static size_t PackSize = 16;
			constexpr static size_t Alignment = PackSize * 4;
		};

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
		struct data_view_policy_aos {
			constexpr static DataLayout Layout = data_layout_properties<DataLayout::AoS, ValueType>::Layout;
			constexpr static size_t Alignment = data_layout_properties<DataLayout::AoS, ValueType>::Alignment;

			GAIA_NODISCARD constexpr static ValueType getc(std::span<const ValueType> s, size_t idx) {
				return s[idx];
			}

			GAIA_NODISCARD constexpr static ValueType get(std::span<ValueType> s, size_t idx) {
				return s[idx];
			}

			GAIA_NODISCARD constexpr static const ValueType& getc_constref(std::span<const ValueType> s, size_t idx) {
				return (const ValueType&)s[idx];
			}

			GAIA_NODISCARD constexpr static const ValueType& get_constref(std::span<ValueType> s, size_t idx) {
				return (const ValueType&)s[idx];
			}

			GAIA_NODISCARD constexpr static ValueType& get_ref(std::span<ValueType> s, size_t idx) {
				return s[idx];
			}

			constexpr static void set(std::span<ValueType> s, size_t idx, ValueType&& val) {
				s[idx] = std::forward<ValueType>(val);
			}
		};

		template <typename ValueType>
		struct data_view_policy<DataLayout::AoS, ValueType>: data_view_policy_aos<ValueType> {};

		template <typename ValueType>
		struct data_view_policy_aos_get {
			using view_policy = data_view_policy_aos<ValueType>;

			//! Raw data pointed to by the view policy
			std::span<const ValueType> m_data;

			GAIA_NODISCARD const ValueType& operator[](size_t idx) const {
				return view_policy::getc_constref(m_data, idx);
			}

			GAIA_NODISCARD auto data() const {
				return m_data.data();
			}

			GAIA_NODISCARD auto size() const {
				return m_data.size();
			}
		};

		template <typename ValueType>
		struct data_view_policy_get<DataLayout::AoS, ValueType>: data_view_policy_aos_get<ValueType> {};

		template <typename ValueType>
		struct data_view_policy_aos_set {
			using view_policy = data_view_policy_aos<ValueType>;

			//! Raw data pointed to by the view policy
			std::span<ValueType> m_data;

			GAIA_NODISCARD ValueType& operator[](size_t idx) {
				return view_policy::get_ref(m_data, idx);
			}

			GAIA_NODISCARD const ValueType& operator[](size_t idx) const {
				return view_policy::getc_constref(m_data, idx);
			}

			GAIA_NODISCARD auto data() const {
				return m_data.data();
			}

			GAIA_NODISCARD auto size() const {
				return m_data.size();
			}
		};

		template <typename ValueType>
		struct data_view_policy_set<DataLayout::AoS, ValueType>: data_view_policy_aos_set<ValueType> {};

		template <typename ValueType>
		using aos_view_policy = data_view_policy_aos<ValueType>;
		template <typename ValueType>
		using aos_view_policy_get = data_view_policy_aos_get<ValueType>;
		template <typename ValueType>
		using aos_view_policy_set = data_view_policy_aos_set<ValueType>;

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
		template <DataLayout TDataLayout, typename ValueType>
		struct data_view_policy_soa {
			constexpr static DataLayout Layout = data_layout_properties<TDataLayout, ValueType>::Layout;
			constexpr static size_t Alignment = data_layout_properties<TDataLayout, ValueType>::Alignment;

			template <size_t Ids>
			using value_type = typename std::tuple_element<Ids, decltype(struct_to_tuple(ValueType{}))>::type;
			template <size_t Ids>
			using const_value_type = typename std::add_const<value_type<Ids>>::type;

			GAIA_NODISCARD constexpr static ValueType get(std::span<const ValueType> s, const size_t idx) {
				auto t = struct_to_tuple(ValueType{});
				return get_internal(t, s, idx, std::make_integer_sequence<size_t, std::tuple_size<decltype(t)>::value>());
			}

			template <size_t Ids>
			GAIA_NODISCARD constexpr static auto get(std::span<const ValueType> s, const size_t idx = 0) {
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
			GAIA_NODISCARD constexpr static ValueType get_internal(
					Tuple& t, std::span<const ValueType> s, const size_t idx, std::integer_sequence<size_t, Ids...> /*no_name*/) {
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
			set_internal(Tuple& t, std::span<TValue> s, const size_t idx, std::integer_sequence<size_t, Ids...> /*no_name*/) {
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
		struct data_view_policy<DataLayout::SoA, ValueType>: data_view_policy_soa<DataLayout::SoA, ValueType> {};
		template <typename ValueType>
		struct data_view_policy<DataLayout::SoA8, ValueType>: data_view_policy_soa<DataLayout::SoA8, ValueType> {};
		template <typename ValueType>
		struct data_view_policy<DataLayout::SoA16, ValueType>: data_view_policy_soa<DataLayout::SoA16, ValueType> {};

		template <typename ValueType>
		using soa_view_policy = data_view_policy<DataLayout::SoA, ValueType>;
		template <typename ValueType>
		using soa8_view_policy = data_view_policy<DataLayout::SoA8, ValueType>;
		template <typename ValueType>
		using soa16_view_policy = data_view_policy<DataLayout::SoA16, ValueType>;

		template <DataLayout TDataLayout, typename ValueType>
		struct data_view_policy_soa_get {
			using view_policy = data_view_policy_soa<TDataLayout, ValueType>;

			template <size_t Ids>
			struct data_view_policy_idx_info {
				using const_value_type = typename view_policy::template const_value_type<Ids>;
			};

			//! Raw data pointed to by the view policy
			std::span<const ValueType> m_data;

			GAIA_NODISCARD constexpr auto operator[](size_t idx) const {
				return view_policy::get(m_data, idx);
			}

			template <size_t Ids>
			GAIA_NODISCARD constexpr auto get() const {
				return std::span<typename data_view_policy_idx_info<Ids>::const_value_type>(
						view_policy::template get<Ids>(m_data).data(), view_policy::template get<Ids>(m_data).size());
			}

			GAIA_NODISCARD auto data() const {
				return m_data.data();
			}

			GAIA_NODISCARD auto size() const {
				return m_data.size();
			}
		};

		template <typename ValueType>
		struct data_view_policy_get<DataLayout::SoA, ValueType>: data_view_policy_soa_get<DataLayout::SoA, ValueType> {};
		template <typename ValueType>
		struct data_view_policy_get<DataLayout::SoA8, ValueType>: data_view_policy_soa_get<DataLayout::SoA8, ValueType> {};
		template <typename ValueType>
		struct data_view_policy_get<DataLayout::SoA16, ValueType>:
				data_view_policy_soa_get<DataLayout::SoA16, ValueType> {};

		template <typename ValueType>
		using soa_view_policy_get = data_view_policy_get<DataLayout::SoA, ValueType>;
		template <typename ValueType>
		using soa8_view_policy_get = data_view_policy_get<DataLayout::SoA8, ValueType>;
		template <typename ValueType>
		using soa16_view_policy_get = data_view_policy_get<DataLayout::SoA16, ValueType>;

		template <DataLayout TDataLayout, typename ValueType>
		struct data_view_policy_soa_set {
			using view_policy = data_view_policy_soa<TDataLayout, ValueType>;

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

			GAIA_NODISCARD constexpr auto operator[](size_t idx) const {
				return view_policy::get(m_data, idx);
			}
			GAIA_NODISCARD constexpr auto operator[](size_t idx) {
				return setter(m_data, idx);
			}

			template <size_t Ids>
			GAIA_NODISCARD constexpr auto get() const {
				using value_type = typename data_view_policy_idx_info<Ids>::const_value_type;
				const std::span<const ValueType> data((const ValueType*)m_data.data(), m_data.size());
				return std::span<value_type>(
						view_policy::template get<Ids>(data).data(), view_policy::template get<Ids>(data).size());
			}

			template <size_t Ids>
			GAIA_NODISCARD constexpr auto set() {
				return std::span<typename data_view_policy_idx_info<Ids>::value_type>(
						view_policy::template set<Ids>(m_data).data(), view_policy::template set<Ids>(m_data).size());
			}

			GAIA_NODISCARD auto data() const {
				return m_data.data();
			}

			GAIA_NODISCARD auto size() const {
				return m_data.size();
			}
		};

		template <typename ValueType>
		struct data_view_policy_set<DataLayout::SoA, ValueType>: data_view_policy_soa_set<DataLayout::SoA, ValueType> {};
		template <typename ValueType>
		struct data_view_policy_set<DataLayout::SoA8, ValueType>: data_view_policy_soa_set<DataLayout::SoA8, ValueType> {};
		template <typename ValueType>
		struct data_view_policy_set<DataLayout::SoA16, ValueType>:
				data_view_policy_soa_set<DataLayout::SoA16, ValueType> {};

		template <typename ValueType>
		using soa_view_policy_set = data_view_policy_set<DataLayout::SoA, ValueType>;
		template <typename ValueType>
		using soa8_view_policy_set = data_view_policy_set<DataLayout::SoA8, ValueType>;
		template <typename ValueType>
		using soa16_view_policy_set = data_view_policy_set<DataLayout::SoA16, ValueType>;

		//----------------------------------------------------------------------
		// Helpers
		//----------------------------------------------------------------------

		namespace detail {
			template <typename T, typename = void>
			struct auto_view_policy_internal {
				static constexpr DataLayout data_layout_type = DataLayout::AoS;
			};
			template <typename T>
			struct auto_view_policy_internal<T, std::void_t<decltype(T::Layout)>> {
				static constexpr DataLayout data_layout_type = T::Layout;
			};

			template <typename, typename = void>
			struct is_soa_layout: std::false_type {};
			template <typename T>
			struct is_soa_layout<T, typename std::enable_if_t<T::Layout != DataLayout::AoS>>: std::true_type {};
		} // namespace detail

		template <typename T>
		using auto_view_policy = data_view_policy<detail::auto_view_policy_internal<T>::data_layout_type, T>;
		template <typename T>
		using auto_view_policy_get = data_view_policy_get<detail::auto_view_policy_internal<T>::data_layout_type, T>;
		template <typename T>
		using auto_view_policy_set = data_view_policy_set<detail::auto_view_policy_internal<T>::data_layout_type, T>;

		template <typename T>
		inline constexpr bool is_soa_layout_v = detail::is_soa_layout<T>::value;

	} // namespace utils
} // namespace gaia
