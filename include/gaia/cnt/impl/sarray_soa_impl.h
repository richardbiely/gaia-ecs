#pragma once
#include "../../config/config.h"

#include <cstddef>
#include <new>
#include <tuple>
#include <type_traits>
#include <utility>

#include "../../core/iterator.h"
#include "../../core/utility.h"
#include "../../mem/data_layout_policy.h"
#include "../../mem/mem_utils.h"
#include "../../mem/raw_data_holder.h"

namespace gaia {
	namespace cnt {
		namespace sarr_soa_detail {
			using difference_type = uint32_t;
			using size_type = uint32_t;
		} // namespace sarr_soa_detail

		template <typename T>
		struct sarr_soa_iterator {
			using value_type = T;
			// using pointer = T*; not supported
			// using reference = T&; not supported
			using difference_type = sarr_soa_detail::difference_type;
			using size_type = sarr_soa_detail::size_type;

			using iterator = sarr_soa_iterator;
			using iterator_category = core::random_access_iterator_tag;

		private:
			uint8_t* m_ptr;
			uint32_t m_cnt;
			uint32_t m_idx;

		public:
			sarr_soa_iterator(uint8_t* ptr, uint32_t cnt, uint32_t idx): m_ptr(ptr), m_cnt(cnt), m_idx(idx) {}

			T operator*() const {
				return mem::data_view_policy<T::gaia_Data_Layout, T>::get({m_ptr, m_cnt}, m_idx);
			}
			T operator->() const {
				return mem::data_view_policy<T::gaia_Data_Layout, T>::get({m_ptr, m_cnt}, m_idx);
			}
			iterator operator[](size_type offset) const {
				return iterator(m_ptr, m_cnt, m_idx + offset);
			}

			iterator& operator+=(size_type diff) {
				m_idx += diff;
				return *this;
			}
			iterator& operator-=(size_type diff) {
				m_idx -= diff;
				return *this;
			}
			iterator& operator++() {
				++m_idx;
				return *this;
			}
			iterator operator++(int) {
				iterator temp(*this);
				++*this;
				return temp;
			}
			iterator& operator--() {
				--m_idx;
				return *this;
			}
			iterator operator--(int) {
				iterator temp(*this);
				--*this;
				return temp;
			}

			iterator operator+(size_type offset) const {
				return iterator(m_ptr, m_cnt, m_idx + offset);
			}
			iterator operator-(size_type offset) const {
				return iterator(m_ptr, m_cnt, m_idx + offset);
			}
			difference_type operator-(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return (difference_type)(m_idx - other.m_idx);
			}

			GAIA_NODISCARD bool operator==(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return m_idx == other.m_idx;
			}
			GAIA_NODISCARD bool operator!=(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return m_idx != other.m_idx;
			}
			GAIA_NODISCARD bool operator>(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return m_idx > other.m_idx;
			}
			GAIA_NODISCARD bool operator>=(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return m_idx >= other.m_idx;
			}
			GAIA_NODISCARD bool operator<(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return m_idx < other.m_idx;
			}
			GAIA_NODISCARD bool operator<=(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return m_idx <= other.m_idx;
			}
		};

		template <typename T>
		struct const_sarr_soa_iterator {
			using value_type = T;
			// using pointer = T*; not supported
			// using reference = T&; not supported
			using difference_type = sarr_soa_detail::difference_type;
			using size_type = sarr_soa_detail::size_type;

			using iterator = const_sarr_soa_iterator;
			using iterator_category = core::random_access_iterator_tag;

		private:
			const uint8_t* m_ptr;
			uint32_t m_cnt;
			uint32_t m_idx;

		public:
			const_sarr_soa_iterator(const uint8_t* ptr, uint32_t cnt, uint32_t idx): m_ptr(ptr), m_cnt(cnt), m_idx(idx) {}

			T operator*() const {
				return mem::data_view_policy<T::gaia_Data_Layout, T>::get({m_ptr, m_cnt}, m_idx);
			}
			T operator->() const {
				return mem::data_view_policy<T::gaia_Data_Layout, T>::get({m_ptr, m_cnt}, m_idx);
			}
			iterator operator[](size_type offset) const {
				return iterator(m_ptr, m_cnt, m_idx + offset);
			}

			iterator& operator+=(size_type diff) {
				m_idx += diff;
				return *this;
			}
			iterator& operator-=(size_type diff) {
				m_idx -= diff;
				return *this;
			}
			iterator& operator++() {
				++m_idx;
				return *this;
			}
			iterator operator++(int) {
				iterator temp(*this);
				++*this;
				return temp;
			}
			iterator& operator--() {
				--m_idx;
				return *this;
			}
			iterator operator--(int) {
				iterator temp(*this);
				--*this;
				return temp;
			}

			iterator operator+(size_type offset) const {
				return iterator(m_ptr, m_cnt, m_idx + offset);
			}
			iterator operator-(size_type offset) const {
				return iterator(m_ptr, m_cnt, m_idx + offset);
			}
			difference_type operator-(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return (difference_type)(m_idx - other.m_idx);
			}

			GAIA_NODISCARD bool operator==(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return m_idx == other.m_idx;
			}
			GAIA_NODISCARD bool operator!=(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return m_idx != other.m_idx;
			}
			GAIA_NODISCARD bool operator>(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return m_idx > other.m_idx;
			}
			GAIA_NODISCARD bool operator>=(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return m_idx >= other.m_idx;
			}
			GAIA_NODISCARD bool operator<(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return m_idx < other.m_idx;
			}
			GAIA_NODISCARD bool operator<=(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return m_idx <= other.m_idx;
			}
		};

		//! Array of elements of type \tparam T with fixed size and capacity \tparam N allocated on stack.
		//! Interface compatiblity with std::array where it matters.
		template <typename T, sarr_soa_detail::size_type N>
		class sarr_soa {
			static_assert(mem::is_soa_layout_v<T>, "sarr_soa can be used only with soa types");

		public:
			static_assert(N > 0);

			using value_type = T;
			using reference = T&;
			using const_reference = const T&;
			using pointer = T*;
			using const_pointer = T*;
			using view_policy = mem::data_view_policy_soa<T::gaia_Data_Layout, T>;
			using difference_type = sarr_soa_detail::difference_type;
			using size_type = sarr_soa_detail::size_type;

			using iterator = sarr_soa_iterator<T>;
			using const_iterator = const_sarr_soa_iterator<T>;
			using iterator_category = core::random_access_iterator_tag;

			static constexpr size_type extent = N;
			static constexpr uint32_t allocated_bytes = view_policy::get_min_byte_size(0, N);

			mem::raw_data_holder<T, allocated_bytes> m_data;

			constexpr sarr_soa() noexcept = default;

			//! Zero-initialization constructor. Because sarr_soa is not aggretate type, doing: sarr_soa<int,10> tmp{} does
			//! not zero-initialize its internals. We need to be explicit about our intent and use a special constructor.
			constexpr sarr_soa(core::zero_t) noexcept {
				// explicit zeroing
				for (auto i = (size_type)0; i < extent; ++i)
					operator[](i) = {};
			}

			~sarr_soa() = default;

			template <typename InputIt>
			constexpr sarr_soa(InputIt first, InputIt last) noexcept {
				const auto count = (size_type)core::distance(first, last);

				if constexpr (std::is_pointer_v<InputIt>) {
					for (size_type i = 0; i < count; ++i)
						operator[](i) = first[i];
				} else if constexpr (std::is_same_v<typename InputIt::iterator_category, core::random_access_iterator_tag>) {
					for (size_type i = 0; i < count; ++i)
						operator[](i) = *(first[i]);
				} else {
					size_type i = 0;
					for (auto it = first; it != last; ++it)
						operator[](++i) = *it;
				}
			}

			constexpr sarr_soa(std::initializer_list<T> il): sarr_soa(il.begin(), il.end()) {}

			constexpr sarr_soa(const sarr_soa& other): sarr_soa(other.begin(), other.end()) {}

			constexpr sarr_soa(sarr_soa&& other) noexcept {
				GAIA_ASSERT(core::addressof(other) != this);

				mem::move_elements<T>((uint8_t*)m_data, (uint8_t*)other.m_data, other.size(), 0, extent, other.extent);
			}

			sarr_soa& operator=(std::initializer_list<T> il) {
				*this = sarr_soa(il.begin(), il.end());
				return *this;
			}

			constexpr sarr_soa& operator=(const sarr_soa& other) {
				GAIA_ASSERT(core::addressof(other) != this);

				mem::copy_elements<T>(
						GAIA_ACC((uint8_t*)&m_data[0]), GAIA_ACC((const uint8_t*)&other.m_data[0]), other.size(), 0, extent,
						other.extent);

				return *this;
			}

			constexpr sarr_soa& operator=(sarr_soa&& other) noexcept {
				GAIA_ASSERT(core::addressof(other) != this);

				mem::move_elements<T>(
						GAIA_ACC((uint8_t*)&m_data[0]), GAIA_ACC((uint8_t*)&other.m_data[0]), other.size(), 0, extent,
						other.extent);

				return *this;
			}

			GAIA_CLANG_WARNING_PUSH()
			// Memory is aligned so we can silence this warning
			GAIA_CLANG_WARNING_DISABLE("-Wcast-align")

			GAIA_NODISCARD constexpr pointer data() noexcept {
				return GAIA_ACC((pointer)&m_data[0]);
			}

			GAIA_NODISCARD constexpr const_pointer data() const noexcept {
				return GAIA_ACC((const_pointer)&m_data[0]);
			}

			GAIA_NODISCARD constexpr decltype(auto) operator[](size_type pos) noexcept {
				GAIA_ASSERT(pos < size());
				return view_policy::set({GAIA_ACC((typename view_policy::TargetCastType) & m_data[0]), extent}, pos);
			}

			GAIA_NODISCARD constexpr decltype(auto) operator[](size_type pos) const noexcept {
				GAIA_ASSERT(pos < size());
				return view_policy::get({GAIA_ACC((typename view_policy::TargetCastType) & m_data[0]), extent}, pos);
			}

			GAIA_CLANG_WARNING_POP()

			GAIA_NODISCARD constexpr size_type size() const noexcept {
				return N;
			}

			GAIA_NODISCARD constexpr bool empty() const noexcept {
				return begin() == end();
			}

			GAIA_NODISCARD constexpr size_type max_size() const noexcept {
				return N;
			}

			GAIA_NODISCARD constexpr decltype(auto) front() noexcept {
				return *begin();
			}

			GAIA_NODISCARD constexpr decltype(auto) front() const noexcept {
				return *begin();
			}

			GAIA_NODISCARD constexpr decltype(auto) back() noexcept {
				return (operator[])(N - 1);
			}

			GAIA_NODISCARD constexpr decltype(auto) back() const noexcept {
				return operator[](N - 1);
			}

			GAIA_NODISCARD constexpr auto begin() noexcept {
				return iterator(GAIA_ACC(&m_data[0]), size(), 0);
			}

			GAIA_NODISCARD constexpr decltype(auto) begin() const noexcept {
				return const_iterator(GAIA_ACC(&m_data[0]), size(), 0);
			}

			GAIA_NODISCARD constexpr auto cbegin() const noexcept {
				return const_iterator(GAIA_ACC(&m_data[0]), size(), 0);
			}

			GAIA_NODISCARD constexpr auto rbegin() noexcept {
				return iterator(GAIA_ACC(&m_data[0]), size(), size() - 1);
			}

			GAIA_NODISCARD constexpr decltype(auto) rbegin() const noexcept {
				return const_iterator(m_data, size(), size() - 1);
			}

			GAIA_NODISCARD constexpr auto crbegin() const noexcept {
				return const_iterator(m_data, size(), size() - 1);
			}

			GAIA_NODISCARD constexpr auto end() noexcept {
				return iterator(GAIA_ACC(&m_data[0]), size(), size());
			}

			GAIA_NODISCARD constexpr decltype(auto) end() const noexcept {
				return const_iterator(GAIA_ACC(&m_data[0]), size(), size());
			}

			GAIA_NODISCARD constexpr auto cend() const noexcept {
				return const_iterator(GAIA_ACC(&m_data[0]), size(), size());
			}

			GAIA_NODISCARD constexpr auto rend() noexcept {
				return iterator(GAIA_ACC(&m_data[0]), size(), -1);
			}

			GAIA_NODISCARD constexpr decltype(auto) rend() const noexcept {
				return const_iterator(GAIA_ACC(&m_data[0]), size(), -1);
			}

			GAIA_NODISCARD constexpr auto crend() const noexcept {
				return const_iterator(GAIA_ACC(&m_data[0]), size(), -1);
			}

			GAIA_NODISCARD constexpr bool operator==(const sarr_soa& other) const {
				for (size_type i = 0; i < N; ++i)
					if (!(operator[](i) == other[i]))
						return false;
				return true;
			}

			GAIA_NODISCARD constexpr bool operator!=(const sarr_soa& other) const {
				return !operator==(other);
			}

			template <size_t Item>
			auto view_mut() noexcept {
				return mem::data_view_policy<T::gaia_Data_Layout, T>::template set<Item>(
						std::span<uint8_t>{GAIA_ACC((uint8_t*)&m_data[0]), extent});
			}

			template <size_t Item>
			auto view() const noexcept {
				return mem::data_view_policy<T::gaia_Data_Layout, T>::template get<Item>(
						std::span<const uint8_t>{GAIA_ACC((const uint8_t*)&m_data[0]), extent});
			}
		};

		namespace detail {
			template <typename T, uint32_t N, uint32_t... I>
			constexpr sarr_soa<std::remove_cv_t<T>, N> to_array_impl(T (&a)[N], std::index_sequence<I...> /*no_name*/) {
				return {{a[I]...}};
			}
		} // namespace detail

		template <typename T, uint32_t N>
		constexpr sarr_soa<std::remove_cv_t<T>, N> to_array(T (&a)[N]) {
			return detail::to_array_impl(a, std::make_index_sequence<N>{});
		}

		template <typename T, typename... U>
		sarr_soa(T, U...) -> sarr_soa<T, 1 + (uint32_t)sizeof...(U)>;

	} // namespace cnt
} // namespace gaia

namespace std {
	template <typename T, uint32_t N>
	struct tuple_size<gaia::cnt::sarr_soa<T, N>>: std::integral_constant<uint32_t, N> {};

	template <size_t I, typename T, uint32_t N>
	struct tuple_element<I, gaia::cnt::sarr_soa<T, N>> {
		using type = T;
	};
} // namespace std
