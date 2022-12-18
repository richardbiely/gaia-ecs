#pragma once
#include <cstddef>
#include <type_traits>
#include <utility>

#include "../../utils/iterator.h"

namespace gaia {
	namespace containers {
		// SArray with fixed capacity and variable size allocated on stack.
		// Interface compatiblity with std::array where it matters.
		// Can be used if STL containers are not an option for some reason.
		template <typename T, size_t N>
		class sarr_ext {
		public:
			using iterator_category = GAIA_UTIL::random_access_iterator_tag;
			using value_type = T;
			using reference = T&;
			using const_reference = const T&;
			using pointer = T*;
			using const_pointer = T*;
			using difference_type = std::ptrdiff_t;
			using size_type = decltype(N);

		private:
			T m_data[N ? N : 1]; // support zero-size arrays
			size_type m_pos;

		public:
			class iterator {
			public:
				using iterator_category = GAIA_UTIL::random_access_iterator_tag;
				using value_type = T;
				using difference_type = std::ptrdiff_t;
				using pointer = T*;
				using reference = T&;

				using size_type = decltype(N);

			private:
				T* m_ptr;

			public:
				constexpr iterator(T* ptr): m_ptr(ptr) {}

				constexpr iterator(const iterator& other): m_ptr(other.m_ptr) {}
				constexpr iterator& operator=(const iterator& other) {
					m_ptr = other.m_ptr;
					return *this;
				}

				constexpr T& operator*() const {
					return *m_ptr;
				}
				constexpr T* operator->() const {
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
					--this;
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

				constexpr bool operator==(const iterator& other) const {
					return m_ptr == other.m_ptr;
				}
				constexpr bool operator!=(const iterator& other) const {
					return m_ptr != other.m_ptr;
				}
				constexpr bool operator>(const iterator& other) const {
					return m_ptr > other.m_ptr;
				}
				constexpr bool operator>=(const iterator& other) const {
					return m_ptr >= other.m_ptr;
				}
				constexpr bool operator<(const iterator& other) const {
					return m_ptr < other.m_ptr;
				}
				constexpr bool operator<=(const iterator& other) const {
					return m_ptr <= other.m_ptr;
				}
			};

			class const_iterator {
			public:
				using iterator_category = GAIA_UTIL::random_access_iterator_tag;
				using value_type = T;
				using difference_type = std::ptrdiff_t;
				using pointer = T*;
				using reference = T&;

				using size_type = decltype(N);

			private:
				const T* m_ptr;

			public:
				constexpr const_iterator(T* ptr): m_ptr(ptr) {}

				constexpr const_iterator(const const_iterator& other): m_ptr(other.m_ptr) {}
				constexpr iterator& operator=(const const_iterator& other) {
					m_ptr = other.m_ptr;
					return *this;
				}

				constexpr const T& operator*() const {
					return *(const T*)m_ptr;
				}
				constexpr const T* operator->() const {
					return (const T*)m_ptr;
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
					--this;
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

			sarr_ext() noexcept {
				clear();
			}

			constexpr pointer data() noexcept {
				return (pointer)m_data;
			}

			constexpr const_pointer data() const noexcept {
				return (const_pointer)m_data;
			}

			constexpr reference operator[](size_type pos) noexcept {
				return (reference)m_data[pos];
			}

			constexpr const_reference operator[](size_type pos) const noexcept {
				return (const_reference)m_data[pos];
			}

			void push_back(const T& arg) noexcept {
				GAIA_ASSERT(size() < N);
				m_data[++m_pos] = arg;
			}

			constexpr void push_back_ct(const T& arg) noexcept {
				m_data[++m_pos] = arg;
			}

			void push_back(T&& arg) noexcept {
				GAIA_ASSERT(size() < N);
				m_data[++m_pos] = std::forward<T>(arg);
			}

			constexpr void push_back_ct(T&& arg) noexcept {
				m_data[++m_pos] = std::forward<T>(arg);
			}

			void pop_back() noexcept {
				GAIA_ASSERT(!empty());
				--m_pos;
			}

			constexpr void pop_back_ct() noexcept {
				--m_pos;
			}

			constexpr void clear() noexcept {
				m_pos = size_type(-1);
			}

			void resize(size_type size) noexcept {
				GAIA_ASSERT(size < N);
				resize_ct(size);
			}

			constexpr void resize_ct(size_type size) noexcept {
				m_pos = size - 1;
			}

			[[nodiscard]] constexpr size_type size() const noexcept {
				return m_pos + 1;
			}

			[[nodiscard]] constexpr bool empty() const noexcept {
				return size() == 0;
			}

			[[nodiscard]] constexpr size_type max_size() const noexcept {
				return N;
			}

			constexpr reference front() noexcept {
				return *begin();
			}

			constexpr const_reference front() const noexcept {
				return *begin();
			}

			constexpr reference back() noexcept {
				return N ? *(end() - 1) : *end();
			}

			constexpr const_reference back() const noexcept {
				return N ? *(end() - 1) : *end();
			}

			constexpr iterator begin() const noexcept {
				return {(T*)m_data};
			}

			constexpr const_iterator cbegin() const noexcept {
				return {(const T*)m_data};
			}

			iterator end() const noexcept {
				return {(T*)m_data + size()};
			}

			const_iterator cend() const noexcept {
				return {(const T*)m_data + size()};
			}

			bool operator==(const sarr_ext& other) const {
				if (m_pos != other.m_pos)
					return false;
				const size_type n = size();
				for (size_type i = 0; i < n; ++i)
					if (m_data[i] != other.m_data[i])
						return false;
				return true;
			}
		};

		namespace detail {
			template <typename T, std::size_t N, std::size_t... I>
			constexpr sarr_ext<std::remove_cv_t<T>, N> to_sarray_impl(T (&a)[N], std::index_sequence<I...>) {
				return {{a[I]...}};
			}
		} // namespace detail

		template <typename T, std::size_t N>
		constexpr sarr_ext<std::remove_cv_t<T>, N> to_sarray(T (&a)[N]) {
			return detail::to_sarray_impl(a, std::make_index_sequence<N>{});
		}

	} // namespace containers

} // namespace gaia

namespace std {
	template <typename T, size_t N>
	struct tuple_size<gaia::containers::sarr_ext<T, N>>: std::integral_constant<std::size_t, N> {};

	template <size_t I, typename T, size_t N>
	struct tuple_element<I, gaia::containers::sarr_ext<T, N>> {
		using type = T;
	};
} // namespace std