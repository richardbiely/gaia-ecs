#pragma once
#include "../../config/config.h"

#include <cstddef>
#include <initializer_list>
#include <type_traits>
#include <utility>

#include "../../utils/iterator.h"
#include "../../utils/mem.h"

namespace gaia {
	namespace containers {
		//! Array with variable size of elements of type \tparam T allocated on heap.
		//! Interface compatiblity with std::vector where it matters.
		template <typename T>
		class darr {
		public:
			using iterator_category = GAIA_UTIL::random_access_iterator_tag;
			using value_type = T;
			using reference = T&;
			using const_reference = const T&;
			using pointer = T*;
			using const_pointer = T*;
			using difference_type = std::ptrdiff_t;
			using size_type = size_t;

		private:
			pointer m_pData = nullptr;
			size_type m_cnt = size_type(0);
			size_type m_cap = size_type(0);

			void push_back_prepare() {
				const auto cnt = size();
				const auto cap = capacity();

				// Unless we reached the capacity don't do anything
				if GAIA_LIKELY (cap != 0 && cap != cnt)
					return;

				// If no data is allocated go with at least 4 elements
				if GAIA_UNLIKELY (m_pData == nullptr) {
					m_pData = new T[m_cap = 4];
					return;
				}

				// Increase the size of an existing array.
				// We increase the capacity in multiples of 1.5 which is about the golden ratio (1.618).
				// This means we prefer more frequent allocations over memory fragmentation.
				T* old = m_pData;
				m_pData = new T[m_cap = (cap * 3) / 2 + 1];
				utils::transfer_elements(m_pData, old, cnt);
				delete[] old;
			}

		public:
			class iterator {
				friend class darr;

			public:
				using iterator_category = GAIA_UTIL::random_access_iterator_tag;
				using value_type = T;
				using difference_type = std::ptrdiff_t;
				using pointer = T*;
				using reference = T&;
				using size_type = darr::size_type;

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
				friend class darr;

			public:
				using iterator_category = GAIA_UTIL::random_access_iterator_tag;
				using value_type = const T;
				using difference_type = std::ptrdiff_t;
				using pointer = const T*;
				using reference = const T&;
				using size_type = darr::size_type;

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

			darr() noexcept = default;

			darr(size_type count, const T& value) {
				resize(count);
				for (auto it: *this)
					*it = value;
			}

			darr(size_type count) {
				resize(count);
			}

			template <typename InputIt>
			darr(InputIt first, InputIt last) {
				const auto count = (size_type)GAIA_UTIL::distance(first, last);
				resize(count);
				size_type i = 0;
				for (auto it = first; it != last; ++it)
					m_pData[i++] = *it;
			}

			darr(std::initializer_list<T> il): darr(il.begin(), il.end()) {}

			darr(const darr& other): darr(other.begin(), other.end()) {}

			darr(darr&& other) noexcept: m_pData(other.m_pData), m_cnt(other.m_cnt), m_cap(other.m_cap) {
				other.m_cnt = size_type(0);
				other.m_cap = size_type(0);
				other.m_pData = nullptr;
			}

			GAIA_NODISCARD darr& operator=(std::initializer_list<T> il) {
				*this = darr(il.begin(), il.end());
				return *this;
			}

			GAIA_NODISCARD darr& operator=(const darr& other) {
				GAIA_ASSERT(GAIA_UTIL::addressof(other) != this);

				resize(other.size());
				utils::copy_elements(m_pData, other.m_pData, other.size());

				return *this;
			}

			GAIA_NODISCARD darr& operator=(darr&& other) noexcept {
				GAIA_ASSERT(GAIA_UTIL::addressof(other) != this);

				m_cnt = other.m_cnt;
				m_cap = other.m_cap;
				m_pData = other.m_pData;

				other.m_cnt = size_type(0);
				other.m_cap = size_type(0);
				other.m_pData = nullptr;

				return *this;
			}

			~darr() {
				delete[] m_pData;
			}

			GAIA_NODISCARD pointer data() noexcept {
				return (pointer)m_pData;
			}

			GAIA_NODISCARD const_pointer data() const noexcept {
				return (const_pointer)m_pData;
			}

			GAIA_NODISCARD reference operator[](size_type pos) noexcept {
				GAIA_ASSERT(pos < size());
				return (reference)m_pData[pos];
			}

			GAIA_NODISCARD const_reference operator[](size_type pos) const noexcept {
				GAIA_ASSERT(pos < size());
				return (const_reference)m_pData[pos];
			}

			void reserve(size_type count) {
				if (count <= m_cap)
					return;

				if (m_pData) {
					T* old = m_pData;
					m_pData = new T[count];
					utils::transfer_elements(m_pData, old, size());
					delete[] old;
				} else {
					m_pData = new T[count];
				}

				m_cap = count;
			}

			void resize(size_type count) {
				if (count <= m_cap) {
					m_cnt = count;
					return;
				}

				if (m_pData) {
					T* old = m_pData;
					m_pData = new T[count];
					utils::transfer_elements(m_pData, old, size());
					delete[] old;
				} else {
					m_pData = new T[count];
				}

				m_cap = count;
				m_cnt = count;
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

			void clear() noexcept {
				resize(0);
			}

			void shirk_to_fit() {
				if (capacity() == size())
					return;
				T* old = m_pData;
				m_pData = new T[m_cap = size()];
				transfer_elements(m_pData, old, size());
				delete[] old;
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
				return static_cast<size_type>(-1);
			}

			GAIA_NODISCARD reference front() noexcept {
				GAIA_ASSERT(!empty());
				return *begin();
			}

			GAIA_NODISCARD const_reference front() const noexcept {
				GAIA_ASSERT(!empty());
				return *begin();
			}

			GAIA_NODISCARD reference back() noexcept {
				GAIA_ASSERT(!empty());
				return m_pData[m_cnt - 1];
			}

			GAIA_NODISCARD const_reference back() const noexcept {
				GAIA_ASSERT(!empty());
				return m_pData[m_cnt - 1];
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

			GAIA_NODISCARD bool operator==(const darr& other) const {
				if (m_cnt != other.m_cnt)
					return false;
				const size_type n = size();
				for (size_type i = 0; i < n; ++i)
					if (m_pData[i] != other.m_pData[i])
						return false;
				return true;
			}
		};
	} // namespace containers

} // namespace gaia