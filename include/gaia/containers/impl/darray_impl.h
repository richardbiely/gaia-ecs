#pragma once
#include <cstddef>
#include <initializer_list>
#include <utility>

#include "../../utils/iterator.h"

namespace gaia {
	namespace containers {
		// Array with variable size allocated on heap.
		// Interface compatiblity with std::vector where it matters.
		// Can be used if STL containers are not an option for some reason.
		template <class T>
		class darr {
		public:
			using iterator_category = GAIA_UTIL(random_access_iterator_tag);
			using value_type = T;
			using reference = T&;
			using const_reference = const T&;
			using pointer = T*;
			using const_pointer = T*;
			using difference_type = std::ptrdiff_t;
			using size_type = uint32_t;

		private:
			T* m_data = nullptr;
			size_type m_cnt = size_type(0);
			size_type m_cap = size_type(0);

		public:
			class iterator {
				friend class darr;

			public:
				using iterator_category = GAIA_UTIL(random_access_iterator_tag);
				using value_type = T;
				using difference_type = std::ptrdiff_t;
				using pointer = T*;
				using reference = T&;
				using size_type = darr::size_type;

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
				constexpr iterator& operator++(int) {
					iterator temp(*this);
					++*this;
					return temp;
				}
				constexpr iterator& operator--() {
					--m_ptr;
					return *this;
				}
				constexpr iterator& operator--(int) {
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
				friend class darr;

			public:
				using iterator_category = GAIA_UTIL(random_access_iterator_tag);
				using value_type = T;
				using difference_type = std::ptrdiff_t;
				using pointer = T*;
				using reference = T&;
				using size_type = darr::size_type;

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
				constexpr const_iterator& operator++(int) {
					const_iterator temp(*this);
					++*this;
					return temp;
				}
				constexpr const_iterator& operator--() {
					--m_ptr;
					return *this;
				}
				constexpr const_iterator& operator--(int) {
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

			darr() noexcept {
				clear();
			}

			darr(size_type count, const T& value) noexcept {
				resize(count);
				for (auto it: *this)
					*it = value;
			}

			darr(size_type count) {
				resize(count);
			}

			template <class InputIt>
			darr(InputIt first, InputIt last) noexcept {
				const auto count = (size_type)GAIA_UTIL(distance)(first, last);
				resize(count);
				size_type i = 0;
				for (auto it = first; it != last; ++it)
					m_data[i++] = *it;
			}

			darr(std::initializer_list<T> il): darr(il.begin(), il.end()) {}

			darr(const darr& other): darr(other.begin(), other.end()) {}

			darr(darr&& other) noexcept {
				m_cnt = other.m_cnt;
				m_cap = other.m_cap;
				m_data = other.m_data;

				other.m_cnt = 0;
				other.m_cap = 0;
				other.m_data = nullptr;
			}

			darr& operator=(std::initializer_list<T> il) {
				auto it = il.begin();
				for (; it != il.end(); ++it)
					push_back(std::move(*it));

				return *this;
			}

			darr& operator=(const darr& other) {
				GAIA_ASSERT(std::addressof(other) != this);

				resize(other.size());
				for (size_type i = 0; i < other.size(); ++i)
					m_data[i] = other[i];
				return *this;
			}

			darr& operator=(darr&& other) noexcept {
				GAIA_ASSERT(std::addressof(other) != this);

				m_cnt = other.m_cnt;
				m_cap = other.m_cap;
				m_data = other.m_data;

				other.m_cnt = 0;
				other.m_cap = 0;
				other.m_data = nullptr;
				return *this;
			}

			~darr() {
				if (m_data != nullptr)
					delete[] m_data;
				m_data = nullptr;
			}

			constexpr pointer data() noexcept {
				return (pointer)m_data;
			}

			constexpr const_pointer data() const noexcept {
				return (const_pointer)m_data;
			}

			reference operator[](size_type pos) noexcept {
				GAIA_ASSERT(pos < size());
				return (reference)m_data[pos];
			}

			const_reference operator[](size_type pos) const noexcept {
				GAIA_ASSERT(pos < size());
				return (const_reference)m_data[pos];
			}

			void reserve(size_type count) {
				if (count <= m_cap)
					return;
				m_cap = count;
				if (m_data) {
					T* old = m_data;
					m_data = new T[m_cap];
					for (size_type i = 0; i < size(); ++i)
						m_data[i] = old[i];
					delete[] old;
				} else {
					m_data = new T[m_cap];
				}
			}

			void resize(size_type count) {
				if (count <= m_cap)
					m_cnt = count;
				else {
					m_cap = count;
					if (m_data) {
						T* old = m_data;
						m_data = new T[m_cap];
						for (size_type i = 0; i < size(); ++i)
							m_data[i] = old[i];
						delete[] old;
					} else {
						m_data = new T[m_cap];
					}
					m_cnt = count;
				}
			}

		private:
			void push_back_prepare() noexcept {
				// Unless we reached the capacity don't do anything
				if (size() != capacity())
					return;

				// If no data is allocated go with at least 4 elements
				if (m_data == nullptr) {
					m_data = new T[m_cap = 4];
				}
				// Increase the size of an existing array in multiples of 1.5
				else {
					T* old = m_data;
					m_data = new T[m_cap = (capacity() * 3) / 2 + 1];
					for (size_type i = 0; i < size(); ++i)
						m_data[i] = old[i];
					delete[] old;
				}
			}

		public:
			void push_back(const T& arg) noexcept {
				push_back_prepare();
				m_data[m_cnt++] = arg;
			}

			void push_back(T&& arg) noexcept {
				push_back_prepare();
				m_data[m_cnt++] = std::forward<T>(arg);
			}

			void pop_back() noexcept {
				GAIA_ASSERT(!empty());
				--m_cnt;
			}

			iterator erase(iterator pos) {
				GAIA_ASSERT(pos.m_ptr >= &m_data[0] && pos.m_ptr < &m_data[m_cap - 1]);

				const auto idxStart = (size_type)GAIA_UTIL(distance(pos, begin()));
				for (size_type i = idxStart; i < size() - 1; ++i)
					m_data[i] = m_data[i + 1];
				--m_cnt;
				return iterator((T*)m_data + idxStart);
			}

			const_iterator erase(const_iterator pos) {
				GAIA_ASSERT(pos.m_ptr >= &m_data[0] && pos.m_ptr < &m_data[m_cap - 1]);

				const auto idxStart = (size_type)GAIA_UTIL(distance(pos, begin()));
				for (size_type i = idxStart; i < size() - 1; ++i)
					m_data[i] = m_data[i + 1];
				--m_cnt;
				return iterator((const T*)m_data + idxStart);
				;
			}

			iterator erase(iterator first, iterator last) {
				GAIA_ASSERT(first.m_pos >= 0 && first.m_pos < size());
				GAIA_ASSERT(last.m_pos >= 0 && last.m_pos < size());
				GAIA_ASSERT(last.m_pos >= last.m_pos);

				for (size_type i = first.m_pos; i < last.m_pos; ++i)
					m_data[i] = m_data[i + 1];
				--m_cnt;
				return {(T*)m_data + size_type(last.m_pos)};
			}

			void clear() noexcept {
				m_cnt = 0;
			}

			void shirk_to_fit() {
				if (m_cnt >= m_cap)
					return;
				T* old = m_data;
				m_data = new T[m_cap = m_cnt];
				for (size_type i = 0; i < size(); ++i)
					m_data[i] = old[i];
				delete[] old;
			}

			[[nodiscard]] size_type size() const noexcept {
				return m_cnt;
			}

			[[nodiscard]] size_type capacity() const noexcept {
				return m_cap;
			}

			[[nodiscard]] bool empty() const noexcept {
				return size() == 0;
			}

			[[nodiscard]] constexpr size_type max_size() const noexcept {
				return 10'000'000;
			}

			reference front() noexcept {
				GAIA_ASSERT(!empty());
				return *begin();
			}

			const_reference front() const noexcept {
				GAIA_ASSERT(!empty());
				return *begin();
			}

			reference back() noexcept {
				GAIA_ASSERT(!empty());
				return m_data[m_cnt - 1];
			}

			const_reference back() const noexcept {
				GAIA_ASSERT(!empty());
				return m_data[m_cnt - 1];
			}

			iterator begin() const noexcept {
				return {(T*)m_data};
			}

			const_iterator cbegin() const noexcept {
				return {(const T*)m_data};
			}

			iterator end() const noexcept {
				return {(T*)m_data + size()};
			}

			const_iterator cend() const noexcept {
				return {(const T*)m_data + size()};
			}
		};
	} // namespace containers

} // namespace gaia