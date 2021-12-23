#pragma once
#include <cstddef>
#include <iterator>
#include <type_traits>
#include <utility>

namespace gaia {
	namespace containers {
		// Array with fixed size and capacity allocated on stack.
		// Interface compatiblity with std::array where it matters.
		// Can be used if STL containers are not an option for some reason.
		template <class T, size_t N>
		class sarr {
		public:
			using iterator_category = std::random_access_iterator_tag;
			using value_type = T;
			using reference = T&;
			using const_reference = const T&;
			using pointer = T*;
			using const_pointer = T*;
			using difference_type = std::ptrdiff_t;
			using size_type = decltype(N);

			T m_data[N ? N : 1]; // support zero-size arrays

			class iterator {
			public:
				using iterator_category = std::random_access_iterator_tag;
				using value_type = T;
				using difference_type = std::ptrdiff_t;
				using pointer = T*;
				using reference = T&;

				using size_type = decltype(N);

			private:
				T* m_ptr;

			public:
				constexpr iterator(T* ptr, size_type pos) {
					m_ptr = ptr + pos;
				}
				constexpr iterator(const iterator& other): m_ptr(other.m_ptr) {}
				constexpr void operator++() {
					++m_ptr;
				}
				constexpr void operator--() {
					--m_ptr;
				}
				constexpr bool operator==(const iterator& rhs) const {
					return m_ptr == rhs.m_ptr;
				}
				constexpr bool operator!=(const iterator& rhs) const {
					return m_ptr != rhs.m_ptr;
				}
				constexpr bool operator>(const iterator& rhs) const {
					return m_ptr > rhs.m_ptr;
				}
				constexpr bool operator<(const iterator& rhs) const {
					return m_ptr < rhs.m_ptr;
				}
				constexpr iterator operator+(size_type offset) const {
					return {m_ptr + offset};
				}
				constexpr iterator operator-(size_type offset) const {
					return {m_ptr - offset};
				}
				constexpr difference_type operator-(const iterator& rhs) const {
					return m_ptr - rhs.m_ptr;
				}
				constexpr T& operator*() const {
					return *m_ptr;
				}
				constexpr T* operator->() const {
					return m_ptr;
				}
			};

			class const_iterator {
			public:
				using iterator_category = std::random_access_iterator_tag;
				using value_type = T;
				using difference_type = std::ptrdiff_t;
				using pointer = T*;
				using reference = T&;

				using size_type = decltype(N);

			private:
				const T* m_ptr;

			public:
				constexpr const_iterator(const T* ptr, size_type pos) {
					m_ptr = ptr + pos;
				}
				constexpr const_iterator(const const_iterator& other): m_ptr(other.m_ptr) {}
				constexpr void operator++() {
					++m_ptr;
				}
				constexpr void operator--() {
					--m_ptr;
				}
				constexpr bool operator==(const const_iterator& rhs) const {
					return m_ptr == rhs.m_ptr;
				}
				constexpr bool operator!=(const const_iterator& rhs) const {
					return m_ptr != rhs.m_ptr;
				}
				constexpr bool operator<(const const_iterator& rhs) const {
					return m_ptr < rhs.m_ptr;
				}
				constexpr bool operator>(const const_iterator& rhs) const {
					return m_ptr > rhs.m_ptr;
				}
				constexpr const_iterator operator+(size_type offset) const {
					return {m_ptr + offset};
				}
				constexpr const_iterator operator-(size_type offset) const {
					return {m_ptr - offset};
				}
				constexpr difference_type operator-(const iterator& rhs) const {
					return m_ptr - rhs.m_ptr;
				}
				constexpr const T& operator*() const {
					return *(const T*)m_ptr;
				}
				constexpr const T* operator->() const {
					return (const T*)m_ptr;
				}
			};

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

			[[nodiscard]] constexpr size_type size() const noexcept {
				return N;
			}

			[[nodiscard]] constexpr bool empty() const noexcept {
				return begin() == end();
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
				return {(T*)m_data, size_type(0)};
			}

			constexpr const_iterator cbegin() const noexcept {
				return {(const T*)m_data, size_type(0)};
			}

			constexpr iterator rbegin() const noexcept {
				return {(T*)m_data, N - 1};
			}

			constexpr const_iterator crbegin() const noexcept {
				return {(const T*)m_data, N - 1};
			}

			constexpr iterator end() const noexcept {
				return {(T*)m_data, N};
			}

			constexpr const_iterator cend() const noexcept {
				return {(const T*)m_data, N};
			}

			constexpr iterator rend() const noexcept {
				return {(T*)m_data, size_type(-1)};
			}

			constexpr const_iterator crend() const noexcept {
				return {(const T*)m_data, size_type(-1)};
			}
		};

		namespace detail {
			template <class T, std::size_t N, std::size_t... I>
			constexpr sarr<std::remove_cv_t<T>, N> to_array_impl(T (&a)[N], std::index_sequence<I...>) {
				return {{a[I]...}};
			}
		} // namespace detail

		template <class T, std::size_t N>
		constexpr sarr<std::remove_cv_t<T>, N> to_array(T (&a)[N]) {
			return detail::to_array_impl(a, std::make_index_sequence<N>{});
		}

		template <class T, class... U>
		sarr(T, U...) -> sarr<T, 1 + sizeof...(U)>;

	} // namespace containers

} // namespace gaia

namespace std {
	template <class T, size_t N>
	struct tuple_size<gaia::containers::sarr<T, N>>: std::integral_constant<std::size_t, N> {};

	template <size_t I, class T, size_t N>
	struct tuple_element<I, gaia::containers::sarr<T, N>> {
		using type = T;
	};
} // namespace std