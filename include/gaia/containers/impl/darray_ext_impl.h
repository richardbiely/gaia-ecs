#pragma once
#include "../../config/config.h"

#include <cstddef>
#include <type_traits>
#include <utility>

#include "../../utils/iterator.h"
#include "../../utils/mem.h"

namespace gaia {
	namespace containers {
		//! Array of elements of type \tparam T allocated on heap or stack. Stack capacity is \tparam N elements.
		//! If the number of elements is bellow \tparam N the stack storage is used.
		//! If the number of elements is above \tparam N the heap storage is used.
		//! Interface compatiblity with std::vector and std::array where it matters.
		template <typename T, uint32_t N>
		class darr_ext {
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
			//! Data allocated on the stack
			T m_data[N != 0U ? N : 1]; // support zero-size arrays
			//! Data allocated on the heap
			T* m_pDataHeap = nullptr;
			//! Pointer to the currently used data
			T* m_pData = m_data;
			//! Number of currently used items ín this container
			size_type m_cnt = size_type(0);
			//! Allocated capacity of m_dataDyn or the extend
			size_type m_cap = extent;

			void push_back_prepare() noexcept {
				// Unless we are above stack allocated size don't do anything
				const auto cnt = size();
				if (cnt < extent)
					return;

				// Unless we reached the capacity don't do anything
				const auto cap = capacity();
				if GAIA_LIKELY (cnt != cap)
					return;

				// If no data is allocated go with at least 4 elements
				if GAIA_UNLIKELY (m_pDataHeap == nullptr) {
					m_pDataHeap = new T[m_cap = 4];
					return;
				}

				// Increase the size of an existing array.
				// We increase the capacity in multiples of 1.5 which is about the golden ratio (1.618).
				// This means we prefer more frequent allocations over memory fragmentation.
				T* old = m_pDataHeap;
				m_pDataHeap = new T[m_cap = (cap * 3) / 2 + 1];
				utils::transfer_elements(m_pDataHeap, old, cnt);
				delete[] old;
			}

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
				iterator(T* ptr): m_ptr(ptr) {}

				T& operator*() const {
					return *m_ptr;
				}
				T* operator->() const {
					return m_ptr;
				}
				iterator operator[](size_type offset) const {
					return {m_ptr + offset};
				}

				iterator& operator+=(size_type diff) {
					m_ptr += diff;
					return *this;
				}
				iterator& operator-=(size_type diff) {
					m_ptr -= diff;
					return *this;
				}
				iterator& operator++() {
					++m_ptr;
					return *this;
				}
				iterator operator++(int) {
					iterator temp(*this);
					++*this;
					return temp;
				}
				iterator& operator--() {
					--m_ptr;
					return *this;
				}
				iterator operator--(int) {
					iterator temp(*this);
					--*this;
					return temp;
				}

				iterator operator+(size_type offset) const {
					return {m_ptr + offset};
				}
				iterator operator-(size_type offset) const {
					return {m_ptr - offset};
				}
				difference_type operator-(const iterator& other) const {
					return m_ptr - other.m_ptr;
				}

				bool operator==(const iterator& other) const {
					return m_ptr == other.m_ptr;
				}
				bool operator!=(const iterator& other) const {
					return m_ptr != other.m_ptr;
				}
				bool operator>(const iterator& other) const {
					return m_ptr > other.m_ptr;
				}
				bool operator>=(const iterator& other) const {
					return m_ptr >= other.m_ptr;
				}
				bool operator<(const iterator& other) const {
					return m_ptr < other.m_ptr;
				}
				bool operator<=(const iterator& other) const {
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
				const_iterator(pointer ptr): m_ptr(ptr) {}

				reference operator*() const {
					return *m_ptr;
				}
				pointer operator->() const {
					return m_ptr;
				}
				const_iterator operator[](size_type offset) const {
					return {m_ptr + offset};
				}

				const_iterator& operator+=(size_type diff) {
					m_ptr += diff;
					return *this;
				}
				const_iterator& operator-=(size_type diff) {
					m_ptr -= diff;
					return *this;
				}
				const_iterator& operator++() {
					++m_ptr;
					return *this;
				}
				const_iterator operator++(int) {
					const_iterator temp(*this);
					++*this;
					return temp;
				}
				const_iterator& operator--() {
					--m_ptr;
					return *this;
				}
				const_iterator operator--(int) {
					const_iterator temp(*this);
					--*this;
					return temp;
				}

				const_iterator operator+(size_type offset) const {
					return {m_ptr + offset};
				}
				const_iterator operator-(size_type offset) const {
					return {m_ptr - offset};
				}
				difference_type operator-(const const_iterator& other) const {
					return m_ptr - other.m_ptr;
				}

				bool operator==(const const_iterator& other) const {
					return m_ptr == other.m_ptr;
				}
				bool operator!=(const const_iterator& other) const {
					return m_ptr != other.m_ptr;
				}
				bool operator>(const const_iterator& other) const {
					return m_ptr > other.m_ptr;
				}
				bool operator>=(const const_iterator& other) const {
					return m_ptr >= other.m_ptr;
				}
				bool operator<(const const_iterator& other) const {
					return m_ptr < other.m_ptr;
				}
				bool operator<=(const const_iterator& other) const {
					return m_ptr <= other.m_ptr;
				}
			};

			darr_ext() noexcept = default;

			darr_ext(size_type count, const T& value) {
				resize(count);

				for (auto it: *this)
					*it = value;
			}

			darr_ext(size_type count) {
				resize(count);
			}

			template <typename InputIt>
			darr_ext(InputIt first, InputIt last) {
				const auto count = (size_type)GAIA_UTIL::distance(first, last);
				resize(count);

				if constexpr (std::is_pointer_v<InputIt>) {
					for (size_t i = 0; i < count; ++i)
						m_pData[i] = first[i];
				} else if constexpr (std::is_same_v<
																 typename InputIt::iterator_category, GAIA_UTIL::random_access_iterator_tag>) {
					for (size_t i = 0; i < count; ++i)
						m_pData[i] = *(first[i]);
				} else {
					size_type i = 0;
					for (auto it = first; it != last; ++it)
						m_pData[i++] = *it;
				}
			}

			darr_ext(std::initializer_list<T> il): darr_ext(il.begin(), il.end()) {}

			darr_ext(const darr_ext& other): darr_ext(other.begin(), other.end()) {}

			darr_ext(darr_ext&& other) noexcept: m_cnt(other.m_cnt), m_cap(other.m_cap) {
				GAIA_ASSERT(GAIA_UTIL::addressof(other) != this);

				if (other.m_pData == other.m_pDataHeap) {
					m_pData = m_pDataHeap;
					m_pDataHeap = other.m_pDataHeap;
				} else {
					m_pData = m_data;
					utils::transfer_elements(m_data, other.m_data, other.size());
					m_pDataHeap = nullptr;
				}

				other.m_cnt = size_type(0);
				other.m_cap = extent;
				other.m_pDataHeap = nullptr;
				other.m_pData = m_data;
			}

			GAIA_NODISCARD darr_ext& operator=(std::initializer_list<T> il) {
				*this = darr_ext(il.begin(), il.end());
				return *this;
			}

			GAIA_NODISCARD darr_ext& operator=(const darr_ext& other) {
				GAIA_ASSERT(GAIA_UTIL::addressof(other) != this);

				resize(other.size());
				utils::copy_elements(m_pData, other.m_pData, other.size());

				return *this;
			}

			GAIA_NODISCARD darr_ext& operator=(darr_ext&& other) noexcept {
				GAIA_ASSERT(GAIA_UTIL::addressof(other) != this);

				m_cnt = other.m_cnt;
				m_cap = other.m_cap;
				if (other.m_pData == other.m_pDataHeap) {
					m_pData = m_pDataHeap;
					m_pDataHeap = other.m_pDataHeap;
				} else {
					m_pData = m_data;
					utils::transfer_elements(m_data, other.m_data, other.m_data.size());
					m_pDataHeap = nullptr;
				}

				other.m_cnt = size_type(0);
				other.m_cap = extent;
				other.m_pDataHeap = nullptr;
				other.m_pData = m_data;

				return *this;
			}

			~darr_ext() {
				delete[] m_pDataHeap;
			}

			GAIA_NODISCARD pointer data() noexcept {
				return (pointer)m_pData;
			}

			GAIA_NODISCARD const_pointer data() const noexcept {
				return (const_pointer)m_pData;
			}

			GAIA_NODISCARD reference operator[](size_type pos) noexcept {
				return (reference)m_pData[pos];
			}

			GAIA_NODISCARD const_reference operator[](size_type pos) const noexcept {
				return (const_reference)m_pData[pos];
			}

			void reserve(size_type count) {
				if (count <= m_cap)
					return;

				if (m_pDataHeap) {
					T* old = m_pDataHeap;
					m_pDataHeap = new T[count];
					utils::transfer_elements(m_pDataHeap, old, size());
					delete[] old;
				} else {
					m_pDataHeap = new T[count];
					utils::transfer_elements(m_pDataHeap, m_data, size());
				}

				m_cap = count;
				m_pData = m_pDataHeap;
			}

			void resize(size_type count) {
				if (count <= extent) {
					m_cnt = count;
					return;
				}

				if (count <= m_cap) {
					m_cnt = count;
					return;
				}

				if (m_pDataHeap) {
					T* old = m_pDataHeap;
					m_pDataHeap = new T[count];
					utils::transfer_elements(m_pDataHeap, old, size());
					delete[] old;
				} else {
					m_pDataHeap = new T[count];
					utils::transfer_elements(m_pDataHeap, m_data, size());
				}

				m_cap = count;
				m_cnt = count;
				m_pData = m_pDataHeap;
			}

			void push_back(const T& arg) {
				push_back_prepare();
				m_pData[m_cnt++] = arg;
			}

			void push_back(T&& arg) {
				push_back_prepare();
				m_pData[m_cnt++] = std::forward<T>(arg);
			}

			void pop_back() noexcept {
				GAIA_ASSERT(!empty());
				--m_cnt;
			}

			GAIA_NODISCARD iterator erase(iterator pos) noexcept {
				GAIA_ASSERT(pos.m_ptr >= &m_pData[0] && pos.m_ptr < &m_pData[m_cap - 1]);

				const auto idxSrc = (size_type)GAIA_UTIL::distance(pos, begin());
				const auto idxDst = size() - 1;

				utils::shift_elements_left(&m_pData[idxSrc], idxDst - idxSrc);
				--m_cnt;

				return iterator((T*)m_pData + idxSrc);
			}

			GAIA_NODISCARD const_iterator erase(const_iterator pos) noexcept {
				GAIA_ASSERT(pos.m_ptr >= &m_pData[0] && pos.m_ptr < &m_pData[m_cap - 1]);

				const auto idxSrc = (size_type)GAIA_UTIL::distance(pos, begin());
				const auto idxDst = size() - 1;

				utils::shift_elements_left(&m_pData[idxSrc], idxDst - idxSrc);
				--m_cnt;

				return iterator((const T*)m_pData + idxSrc);
			}

			GAIA_NODISCARD iterator erase(iterator first, iterator last) noexcept {
				GAIA_ASSERT(first.m_cnt >= 0 && first.m_cnt < size());
				GAIA_ASSERT(last.m_cnt >= 0 && last.m_cnt < size());
				GAIA_ASSERT(last.m_cnt >= last.m_cnt);

				const auto size = last.m_cnt - first.m_cnt;
				utils::shift_elements_left(&m_pData[first.cnt], size);
				m_cnt -= size;

				return {(T*)m_pData + size_type(last.m_cnt)};
			}

			void clear() {
				resize(0);
			}

			void shirk_to_fit() {
				if (capacity() == size())
					return;

				if (m_pData == m_pDataHeap) {
					T* old = m_pDataHeap;

					if (size() < extent) {
						utils::transfer_elements(m_data, old, size());
						m_pData = m_data;
					} else {
						m_pDataHeap = new T[m_cap = size()];
						utils::transfer_elements(m_pDataHeap, old, size());
						m_pData = m_pDataHeap;
					}

					delete[] old;
				} else
					resize(size());
			}

			GAIA_NODISCARD size_type size() const noexcept {
				return m_cnt;
			}

			GAIA_NODISCARD size_type capacity() const noexcept {
				return m_cap;
			}

			GAIA_NODISCARD bool empty() const noexcept {
				return size() == 0;
			}

			GAIA_NODISCARD size_type max_size() const noexcept {
				return N;
			}

			GAIA_NODISCARD reference front() noexcept {
				return *begin();
			}

			GAIA_NODISCARD const_reference front() const noexcept {
				return *begin();
			}

			GAIA_NODISCARD reference back() noexcept {
				return N != 0U ? *(end() - 1) : *end();
			}

			GAIA_NODISCARD const_reference back() const noexcept {
				return N != 0U ? *(end() - 1) : *end();
			}

			GAIA_NODISCARD iterator begin() const noexcept {
				return {(T*)m_pData};
			}

			GAIA_NODISCARD const_iterator cbegin() const noexcept {
				return {(const T*)m_pData};
			}

			GAIA_NODISCARD iterator end() const noexcept {
				return {(T*)m_pData + size()};
			}

			GAIA_NODISCARD const_iterator cend() const noexcept {
				return {(const T*)m_pData + size()};
			}

			GAIA_NODISCARD bool operator==(const darr_ext& other) const {
				if (m_cnt != other.m_cnt)
					return false;
				const size_type n = size();
				for (size_type i = 0; i < n; ++i)
					if (m_pData[i] != other.m_pData[i])
						return false;
				return true;
			}
		};

		namespace detail {
			template <typename T, std::size_t N, std::size_t... I>
			darr_ext<std::remove_cv_t<T>, N> to_sarray_impl(T (&a)[N], std::index_sequence<I...> /*no_name*/) {
				return {{a[I]...}};
			}
		} // namespace detail

		template <typename T, std::size_t N>
		darr_ext<std::remove_cv_t<T>, N> to_sarray(T (&a)[N]) {
			return detail::to_sarray_impl(a, std::make_index_sequence<N>{});
		}

	} // namespace containers

} // namespace gaia

namespace std {
	template <typename T, size_t N>
	struct tuple_size<gaia::containers::darr_ext<T, N>>: std::integral_constant<std::size_t, N> {};

	template <size_t I, typename T, size_t N>
	struct tuple_element<I, gaia::containers::darr_ext<T, N>> {
		using type = T;
	};
} // namespace std