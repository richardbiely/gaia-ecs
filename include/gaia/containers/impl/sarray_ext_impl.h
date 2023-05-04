#pragma once
#include "../../config/config.h"

#include <cstddef>
#include <type_traits>
#include <utility>
#if !GAIA_DISABLE_ASSERTS
	#include <memory>
#endif

#include "../../utils/iterator.h"
#include "../../utils/mem.h"

namespace gaia {
	namespace containers {
		//! Array of elements of type \tparam T with fixed capacity \tparam N and variable size allocated on stack.
		//! Interface compatiblity with std::array where it matters.
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

			static constexpr size_type extent = N;

		private:
			T m_data[N != 0U ? N : 1]; // support zero-size arrays
			size_type m_cnt = size_type(0);

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
				pointer m_ptr;

			public:
				constexpr iterator(T* ptr): m_ptr(ptr) {}

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

			sarr_ext() noexcept = default;

			constexpr sarr_ext(size_type count, const T& value) noexcept {
				resize(count);

				for (auto it: *this)
					*it = value;
			}

			constexpr sarr_ext(size_type count) noexcept {
				resize(count);
			}

			template <typename InputIt>
			constexpr sarr_ext(InputIt first, InputIt last) noexcept {
				const auto count = (size_type)GAIA_UTIL::distance(first, last);
				resize(count);

				size_type i = 0;
				for (auto it = first; it != last; ++it)
					m_data[i++] = *it;
			}

			constexpr sarr_ext(std::initializer_list<T> il) noexcept: sarr_ext(il.begin(), il.end()) {}

			constexpr sarr_ext(const sarr_ext& other) noexcept: sarr_ext(other.begin(), other.end()) {}

			constexpr sarr_ext(sarr_ext&& other) noexcept: m_cnt(other.m_cnt) {
				GAIA_ASSERT(std::addressof(other) != this);

				utils::transfer_elements(m_data, other.m_data, other.size());

				other.m_cnt = size_type(0);
			}

			GAIA_NODISCARD sarr_ext& operator=(std::initializer_list<T> il) {
				*this = sarr_ext(il.begin(), il.end());
				return *this;
			}

			GAIA_NODISCARD constexpr sarr_ext& operator=(const sarr_ext& other) noexcept {
				GAIA_ASSERT(std::addressof(other) != this);

				resize(other.size());
				utils::copy_elements(m_data, other.m_data, other.size());

				return *this;
			}

			GAIA_NODISCARD constexpr sarr_ext& operator=(sarr_ext&& other) noexcept {
				GAIA_ASSERT(std::addressof(other) != this);

				resize(other.m_cnt);
				utils::transfer_elements(m_data, other.m_data, other.size());

				other.m_cnt = size_type(0);

				return *this;
			}

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

			constexpr void push_back(const T& arg) noexcept {
				GAIA_ASSERT(size() < N);
				m_data[m_cnt++] = arg;
			}

			constexpr void push_back(T&& arg) noexcept {
				GAIA_ASSERT(size() < N);
				m_data[m_cnt++] = std::forward<T>(arg);
			}

			constexpr void pop_back() noexcept {
				GAIA_ASSERT(!empty());
				--m_cnt;
			}

			GAIA_NODISCARD constexpr iterator erase(iterator pos) noexcept {
				GAIA_ASSERT(pos.m_ptr >= &m_data[0] && pos.m_ptr < &m_data[N - 1]);

				const auto idxSrc = (size_type)GAIA_UTIL::distance(pos, begin());
				const auto idxDst = size() - 1;

				utils::shift_elements_left(&m_data[idxSrc], idxDst - idxSrc);
				--m_cnt;

				return iterator((T*)m_data + idxSrc);
			}

			GAIA_NODISCARD constexpr const_iterator erase(const_iterator pos) noexcept {
				GAIA_ASSERT(pos.m_ptr >= &m_data[0] && pos.m_ptr < &m_data[N - 1]);

				const auto idxSrc = (size_type)GAIA_UTIL::distance(pos, begin());
				const auto idxDst = size() - 1;

				utils::shift_elements_left(&m_data[idxSrc], idxDst - idxSrc);
				--m_cnt;

				return iterator((const T*)m_data + idxSrc);
			}

			GAIA_NODISCARD constexpr iterator erase(iterator first, iterator last) noexcept {
				GAIA_ASSERT(first.m_cnt >= 0 && first.m_cnt < size());
				GAIA_ASSERT(last.m_cnt >= 0 && last.m_cnt < size());
				GAIA_ASSERT(last.m_cnt >= last.m_cnt);

				const auto size = last.m_cnt - first.m_cnt;
				utils::shift_elements_left(&m_data[first.cnt], size);
				m_cnt -= size;

				return {(T*)m_data + size_type(last.m_cnt)};
			}

			constexpr void clear() noexcept {
				resize(0);
			}

			constexpr void resize(size_type size) noexcept {
				GAIA_ASSERT(size < N);
				m_cnt = size;
			}

			GAIA_NODISCARD constexpr size_type size() const noexcept {
				return m_cnt;
			}

			GAIA_NODISCARD constexpr bool empty() const noexcept {
				return size() == 0;
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

			GAIA_NODISCARD iterator end() const noexcept {
				return {(T*)m_data + size()};
			}

			GAIA_NODISCARD const_iterator cend() const noexcept {
				return {(const T*)m_data + size()};
			}

			GAIA_NODISCARD bool operator==(const sarr_ext& other) const {
				if (m_cnt != other.m_cnt)
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
			constexpr sarr_ext<std::remove_cv_t<T>, N> to_sarray_impl(T (&a)[N], std::index_sequence<I...> /*no_name*/) {
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