#pragma once
#include <cstddef>
#include <iterator>
#include <utility>

namespace gaia {
	namespace utils {
		// Array with fixed capacity and variable size allocated on stack.
		// TODO: Use<memory_resouce>and pmr instead of this madness.
		template <class T, std::size_t N>
		class sarray {
		public:
			using iterator_category = std::random_access_iterator_tag;
			using value_type = T;
			using reference = T&;
			using const_reference = const T&;
			using pointer = T*;
			using const_pointer = T*;
			using difference_type = std::ptrdiff_t;
			using size_type = decltype(N);

		private:
			T m_data[N];
			size_type m_pos;

		public:
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
				size_type m_pos;

			public:
				constexpr iterator(T* ptr, size_type pos) {
					m_ptr = ptr;
					m_pos = pos;
				}
				constexpr iterator(const iterator& other):
						m_ptr(other.m_ptr), m_pos(other.m_pos) {}
				constexpr void operator++() {
					++m_pos;
				}
				constexpr void operator--() {
					--m_pos;
				}
				constexpr bool operator>(const iterator& rhs) const {
					return m_pos > rhs.m_pos;
				}
				constexpr bool operator<(const iterator& rhs) const {
					return m_pos < rhs.m_pos;
				}
				constexpr iterator operator+(size_type offset) const {
					return {m_ptr, m_pos + offset};
				}
				constexpr iterator operator-(size_type offset) const {
					return {m_ptr, m_pos - offset};
				}
				constexpr difference_type operator-(const iterator& rhs) const {
					return m_pos - rhs.m_pos;
				}
				constexpr bool operator==(const iterator& rhs) const {
					return m_pos == rhs.m_pos;
				}
				constexpr bool operator!=(const iterator& rhs) const {
					return m_pos != rhs.m_pos;
				}
				constexpr T& operator*() const {
					return *(T*)(m_ptr + m_pos);
				}
				constexpr T* operator->() const {
					return (T*)(m_ptr + m_pos);
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
				size_type m_pos;

			public:
				constexpr const_iterator(const T* ptr, size_type pos) {
					m_ptr = ptr;
					m_pos = pos;
				}
				constexpr const_iterator(const const_iterator& other):
						m_ptr(other.m_ptr), m_pos(other.m_pos) {}
				constexpr void operator++() {
					++m_pos;
				}
				constexpr void operator--() {
					--m_pos;
				}
				constexpr bool operator==(const const_iterator& rhs) const {
					return m_pos == rhs.m_pos;
				}
				constexpr bool operator!=(const const_iterator& rhs) const {
					return m_pos != rhs.m_pos;
				}
				constexpr bool operator<(const const_iterator& rhs) const {
					return m_pos < rhs.m_pos;
				}
				constexpr bool operator>(const const_iterator& rhs) const {
					return m_pos > rhs.m_pos;
				}
				constexpr const_iterator operator+(size_type offset) const {
					return {m_ptr, m_pos + offset};
				}
				constexpr const_iterator operator-(size_type offset) const {
					return {m_ptr, m_pos - offset};
				}
				constexpr difference_type operator-(const iterator& rhs) const {
					return m_pos - rhs.m_pos;
				}
				constexpr const T& operator*() const {
					return *(const T*)(m_ptr + m_pos);
				}
				constexpr const T* operator->() const {
					return (const T*)(m_ptr + m_pos);
				}
			};

			sarray() {
				clear();
			}

			constexpr T* data() noexcept {
				return m_data;
			}

			constexpr const T* data() const noexcept {
				return m_data;
			}

			constexpr const T& operator[](size_type pos) noexcept {
				return m_data[pos];
			}

			constexpr const T& operator[](size_type pos) const noexcept {
				return m_data[pos];
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
				m_pos = size_t(-1);
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

			constexpr iterator begin() const noexcept {
				return {(T*)m_data, size_type(0)};
			}

			constexpr const_iterator cbegin() const noexcept {
				return {(const T*)m_data, size_type(0)};
			}

			constexpr iterator rbegin() const noexcept {
				return {(T*)m_data, m_pos - 1};
			}

			constexpr const_iterator crbegin() const noexcept {
				return {(const T*)m_data, m_pos - 1};
			}

			constexpr iterator end() const noexcept {
				return {(T*)m_data, m_pos + 1};
			}

			constexpr const_iterator cend() const noexcept {
				return {(const T*)m_data, m_pos + 1};
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
			constexpr sarray<std::remove_cv_t<T>, N>
			to_sarray_impl(T (&a)[N], std::index_sequence<I...>) {
				return {{a[I]...}};
			}
		} // namespace detail

		template <class T, std::size_t N>
		constexpr sarray<std::remove_cv_t<T>, N> to_sarray(T (&a)[N]) {
			return detail::to_sarray_impl(a, std::make_index_sequence<N>{});
		}

	} // namespace utils

} // namespace gaia

namespace std {
	template <class T, size_t N>
	struct tuple_size<gaia::utils::sarray<T, N>>:
			std::integral_constant<std::size_t, N> {};

	template <size_t I, class T, size_t N>
	struct tuple_element<I, gaia::utils::sarray<T, N>> {
		using type = T;
	};
} // namespace std