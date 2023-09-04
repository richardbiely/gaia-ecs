#pragma once
#include <cstddef>
#include <type_traits>
#include <utility>

#include "../../utils/iterator.h"

namespace gaia {
	namespace containers {
		//! Array of elements of type \tparam T with fixed size and capacity \tparam N allocated on stack.
		//! Interface compatiblity with std::array where it matters.
		template <typename T, size_t N>
		class sarr {
		public:
			using iterator_category = GAIA_UTIL::random_access_iterator_tag;
			using value_type = T;
			using reference = T&;
			using const_reference = const T&;
			using pointer = T*;
			using const_pointer = T*;
			using difference_type = std::ptrdiff_t;
			using size_type = decltype(N);

			static constexpr size_type extent = N;

			T m_data[N != 0U ? N : 1]; // support zero-size arrays

			class iterator {
			public:
				using iterator_category = GAIA_UTIL::random_access_iterator_tag;
				using value_type = T;
				using difference_type = std::ptrdiff_t;
				using pointer = T*;
				using reference = T&;

				using size_type = decltype(N);

			private:
				pointer m_ptr;

			public:
				constexpr iterator(pointer ptr): m_ptr(ptr) {}

				constexpr reference operator*() const {
					return *m_ptr;
				}
				constexpr pointer operator->() const {
					return m_ptr;
				}
				constexpr iterator operator[](size_type offset) const {
					return {m_ptr + offset};
				}

				constexpr iterator& operator+=(size_type diff) {
					m_ptr += diff;
					return *this;
				}
				constexpr iterator& operator-=(size_type diff) {
					m_ptr -= diff;
					return *this;
				}
				constexpr iterator& operator++() {
					++m_ptr;
					return *this;
				}
				constexpr iterator operator++(int) {
					iterator temp(*this);
					++*this;
					return temp;
				}
				constexpr iterator& operator--() {
					--m_ptr;
					return *this;
				}
				constexpr iterator operator--(int) {
					iterator temp(*this);
					--*this;
					return temp;
				}

				constexpr iterator operator+(size_type offset) const {
					return {m_ptr + offset};
				}
				constexpr iterator operator-(size_type offset) const {
					return {m_ptr - offset};
				}
				constexpr difference_type operator-(const iterator& other) const {
					return m_ptr - other.m_ptr;
				}

				GAIA_NODISCARD constexpr bool operator==(const iterator& other) const {
					return m_ptr == other.m_ptr;
				}
				GAIA_NODISCARD constexpr bool operator!=(const iterator& other) const {
					return m_ptr != other.m_ptr;
				}
				GAIA_NODISCARD constexpr bool operator>(const iterator& other) const {
					return m_ptr > other.m_ptr;
				}
				GAIA_NODISCARD constexpr bool operator>=(const iterator& other) const {
					return m_ptr >= other.m_ptr;
				}
				GAIA_NODISCARD constexpr bool operator<(const iterator& other) const {
					return m_ptr < other.m_ptr;
				}
				GAIA_NODISCARD constexpr bool operator<=(const iterator& other) const {
					return m_ptr <= other.m_ptr;
				}
			};

			class const_iterator {
			public:
				using iterator_category = GAIA_UTIL::random_access_iterator_tag;
				using value_type = const T;
				using difference_type = std::ptrdiff_t;
				using pointer = const T*;
				using reference = const T&;

				using size_type = decltype(N);

			private:
				pointer m_ptr;

			public:
				constexpr const_iterator(pointer ptr): m_ptr(ptr) {}

				constexpr reference operator*() const {
					return *m_ptr;
				}
				constexpr pointer operator->() const {
					return m_ptr;
				}
				constexpr const_iterator operator[](size_type offset) const {
					return {m_ptr + offset};
				}

				constexpr const_iterator& operator+=(size_type diff) {
					m_ptr += diff;
					return *this;
				}
				constexpr const_iterator& operator-=(size_type diff) {
					m_ptr -= diff;
					return *this;
				}
				constexpr const_iterator& operator++() {
					++m_ptr;
					return *this;
				}
				constexpr const_iterator operator++(int) {
					const_iterator temp(*this);
					++*this;
					return temp;
				}
				constexpr const_iterator& operator--() {
					--m_ptr;
					return *this;
				}
				constexpr const_iterator operator--(int) {
					const_iterator temp(*this);
					--*this;
					return temp;
				}

				constexpr const_iterator operator+(size_type offset) const {
					return {m_ptr + offset};
				}
				constexpr const_iterator operator-(size_type offset) const {
					return {m_ptr - offset};
				}
				constexpr difference_type operator-(const const_iterator& other) const {
					return m_ptr - other.m_ptr;
				}

				constexpr bool operator==(const const_iterator& other) const {
					return m_ptr == other.m_ptr;
				}
				constexpr bool operator!=(const const_iterator& other) const {
					return m_ptr != other.m_ptr;
				}
				constexpr bool operator>(const const_iterator& other) const {
					return m_ptr > other.m_ptr;
				}
				constexpr bool operator>=(const const_iterator& other) const {
					return m_ptr >= other.m_ptr;
				}
				constexpr bool operator<(const const_iterator& other) const {
					return m_ptr < other.m_ptr;
				}
				constexpr bool operator<=(const const_iterator& other) const {
					return m_ptr <= other.m_ptr;
				}
			};

			GAIA_NODISCARD constexpr pointer data() noexcept {
				return (pointer)m_data;
			}

			GAIA_NODISCARD constexpr const_pointer data() const noexcept {
				return (const_pointer)m_data;
			}

			GAIA_NODISCARD constexpr reference operator[](size_type pos) noexcept {
				GAIA_ASSERT(pos < size());
				return (reference)m_data[pos];
			}

			GAIA_NODISCARD constexpr const_reference operator[](size_type pos) const noexcept {
				GAIA_ASSERT(pos < size());
				return (const_reference)m_data[pos];
			}

			GAIA_NODISCARD constexpr size_type size() const noexcept {
				return N;
			}

			GAIA_NODISCARD constexpr bool empty() const noexcept {
				return begin() == end();
			}

			GAIA_NODISCARD constexpr size_type max_size() const noexcept {
				return N;
			}

			GAIA_NODISCARD constexpr reference front() noexcept {
				return *begin();
			}

			GAIA_NODISCARD constexpr const_reference front() const noexcept {
				return *begin();
			}

			GAIA_NODISCARD constexpr reference back() noexcept {
				return N != 0U ? *(end() - 1) : *end();
			}

			GAIA_NODISCARD constexpr const_reference back() const noexcept {
				return N != 0U ? *(end() - 1) : *end();
			}

			GAIA_NODISCARD constexpr iterator begin() const noexcept {
				return {(T*)m_data};
			}

			GAIA_NODISCARD constexpr const_iterator cbegin() const noexcept {
				return {(const T*)m_data};
			}

			GAIA_NODISCARD constexpr iterator end() const noexcept {
				return {(T*)m_data + N};
			}

			GAIA_NODISCARD constexpr const_iterator cend() const noexcept {
				return {(const T*)m_data + N};
			}

			GAIA_NODISCARD bool operator==(const sarr& other) const {
				for (size_type i = 0; i < N; ++i)
					if (!(m_data[i] == other.m_data[i]))
						return false;
				return true;
			}
		};

		namespace detail {
			template <typename T, std::size_t N, std::size_t... I>
			constexpr sarr<std::remove_cv_t<T>, N> to_array_impl(T (&a)[N], std::index_sequence<I...> /*no_name*/) {
				return {{a[I]...}};
			}
		} // namespace detail

		template <typename T, std::size_t N>
		constexpr sarr<std::remove_cv_t<T>, N> to_array(T (&a)[N]) {
			return detail::to_array_impl(a, std::make_index_sequence<N>{});
		}

		template <typename T, typename... U>
		sarr(T, U...) -> sarr<T, 1 + sizeof...(U)>;

	} // namespace containers

} // namespace gaia

namespace std {
	template <typename T, size_t N>
	struct tuple_size<gaia::containers::sarr<T, N>>: std::integral_constant<std::size_t, N> {};

	template <size_t I, typename T, size_t N>
	struct tuple_element<I, gaia::containers::sarr<T, N>> {
		using type = T;
	};
} // namespace std
