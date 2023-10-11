#pragma once
#include "../../config/config.h"

#include <cstddef>
#include <initializer_list>
#include <type_traits>
#include <utility>

#include "../../utils/data_layout_policy.h"
#include "../../utils/iterator.h"
#include "../../utils/mem_utils.h"

namespace gaia {
	namespace containers {
		//! Array with variable size of elements of type \tparam T allocated on heap.
		//! Interface compatiblity with std::vector where it matters.
		template <typename T>
		class darr {
		public:
			using value_type = T;
			using reference = T&;
			using const_reference = const T&;
			using pointer = T*;
			using const_pointer = T*;
			using difference_type = uint32_t;
			using size_type = uint32_t;
			using view_policy = utils::auto_view_policy<T>;

		private:
			uint8_t* m_pData = nullptr;
			size_type m_cnt = size_type(0);
			size_type m_cap = size_type(0);

			void try_grow() {
				const auto cnt = size();
				const auto cap = capacity();

				// Unless we reached the capacity don't do anything
				if GAIA_LIKELY (cap != 0 && cap != cnt)
					return;

				// If no data is allocated go with at least 4 elements
				if GAIA_UNLIKELY (m_pData == nullptr) {
					m_pData = view_policy::alloc_mem(m_cap = 4);
					return;
				}

				// Increase the size of an existing array.
				// We increase the capacity in multiples of 1.5 which is about the golden ratio (1.618).
				// This means we prefer more frequent allocations over memory fragmentation.
				uint8_t* pDataOld = m_pData;
				m_pData = view_policy::alloc_mem(m_cap = (cap * 3) / 2 + 1);
				utils::move_elements<T>(m_pData, pDataOld, 0, cnt, m_cap, cap);
				view_policy::free_mem(pDataOld, cnt);
			}

		public:
			class iterator {
				friend class darr;

			public:
				using iterator_category = GAIA_UTIL::random_access_iterator_tag;
				using value_type = T;
				using difference_type = darr::size_type;
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
					return (difference_type)(m_ptr - other.m_ptr);
				}

				GAIA_NODISCARD bool operator==(const iterator& other) const {
					return m_ptr == other.m_ptr;
				}
				GAIA_NODISCARD bool operator!=(const iterator& other) const {
					return m_ptr != other.m_ptr;
				}
				GAIA_NODISCARD bool operator>(const iterator& other) const {
					return m_ptr > other.m_ptr;
				}
				GAIA_NODISCARD bool operator>=(const iterator& other) const {
					return m_ptr >= other.m_ptr;
				}
				GAIA_NODISCARD bool operator<(const iterator& other) const {
					return m_ptr < other.m_ptr;
				}
				GAIA_NODISCARD bool operator<=(const iterator& other) const {
					return m_ptr <= other.m_ptr;
				}
			};

			class iterator_soa {
				friend class darr;

			public:
				using iterator_category = GAIA_UTIL::random_access_iterator_tag;
				using value_type = T;
				using difference_type = darr::size_type;
				// using pointer = T*; not supported
				// using reference = T&; not supported
				using size_type = darr::size_type;

			private:
				uint8_t* m_ptr;
				uint32_t m_cnt;
				uint32_t m_idx;

			public:
				iterator_soa(uint8_t* ptr, uint32_t cnt, uint32_t idx): m_ptr(ptr), m_cnt(cnt), m_idx(idx) {}

				T operator*() const {
					return utils::soa_view_policy<T>::get({m_ptr, m_cnt}, m_idx);
				}

				iterator_soa operator[](size_type offset) const {
					return iterator_soa(m_ptr, m_cnt, m_idx + offset);
				}

				iterator_soa& operator+=(size_type diff) {
					m_idx += diff;
					return *this;
				}
				iterator& operator-=(size_type diff) {
					m_idx -= diff;
					return *this;
				}
				iterator_soa& operator++() {
					++m_idx;
					return *this;
				}
				iterator_soa operator++(int) {
					iterator_soa temp(*this);
					++*this;
					return temp;
				}
				iterator_soa& operator--() {
					--m_idx;
					return *this;
				}
				iterator_soa operator--(int) {
					iterator_soa temp(*this);
					--*this;
					return temp;
				}

				iterator_soa operator+(size_type offset) const {
					return iterator_soa(m_ptr, m_cnt, m_idx + offset);
				}
				iterator_soa operator-(size_type offset) const {
					return iterator_soa(m_ptr, m_cnt, m_idx + offset);
				}
				difference_type operator-(const iterator_soa& other) const {
					GAIA_ASSERT(m_ptr == other.m_ptr);
					return (difference_type)(m_idx - other.m_idx);
				}

				GAIA_NODISCARD bool operator==(const iterator_soa& other) const {
					GAIA_ASSERT(m_ptr == other.m_ptr);
					return m_idx == other.m_idx;
				}
				GAIA_NODISCARD bool operator!=(const iterator_soa& other) const {
					GAIA_ASSERT(m_ptr == other.m_ptr);
					return m_idx != other.m_idx;
				}
				GAIA_NODISCARD bool operator>(const iterator_soa& other) const {
					GAIA_ASSERT(m_ptr == other.m_ptr);
					return m_idx > other.m_idx;
				}
				GAIA_NODISCARD bool operator>=(const iterator_soa& other) const {
					GAIA_ASSERT(m_ptr == other.m_ptr);
					return m_idx >= other.m_idx;
				}
				GAIA_NODISCARD bool operator<(const iterator_soa& other) const {
					GAIA_ASSERT(m_ptr == other.m_ptr);
					return m_idx < other.m_idx;
				}
				GAIA_NODISCARD bool operator<=(const iterator_soa& other) const {
					GAIA_ASSERT(m_ptr == other.m_ptr);
					return m_idx <= other.m_idx;
				}
			};

			constexpr darr() noexcept = default;

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

				if constexpr (std::is_pointer_v<InputIt>) {
					for (size_type i = 0; i < count; ++i)
						operator[](i) = first[i];
				} else if constexpr (std::is_same_v<
																 typename InputIt::iterator_category, GAIA_UTIL::random_access_iterator_tag>) {
					for (size_type i = 0; i < count; ++i)
						operator[](i) = *(first[i]);
				} else {
					size_type i = 0;
					for (auto it = first; it != last; ++it)
						operator[](i++) = *it;
				}
			}

			darr(std::initializer_list<T> il): darr(il.begin(), il.end()) {}

			darr(const darr& other): darr(other.begin(), other.end()) {}

			darr(darr&& other) noexcept: m_pData(other.m_pData), m_cnt(other.m_cnt), m_cap(other.m_cap) {
				other.m_cnt = size_type(0);
				other.m_cap = size_type(0);
				other.m_pData = nullptr;
			}

			darr& operator=(std::initializer_list<T> il) {
				*this = darr(il.begin(), il.end());
				return *this;
			}

			darr& operator=(const darr& other) {
				GAIA_ASSERT(GAIA_UTIL::addressof(other) != this);

				resize(other.size());
				utils::copy_elements<T>(
						(uint8_t*)m_pData, (const uint8_t*)other.m_pData, 0, other.size(), capacity(), other.capacity());

				return *this;
			}

			darr& operator=(darr&& other) noexcept {
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
				view_policy::free_mem(m_pData, size());
			}

			GAIA_NODISCARD pointer data() noexcept {
				return (pointer)m_pData;
			}

			GAIA_NODISCARD const_pointer data() const noexcept {
				return (const_pointer)m_pData;
			}

			GAIA_NODISCARD auto operator[](size_type pos) noexcept -> decltype(view_policy::set({}, 0)) {
				GAIA_ASSERT(pos < size());
				return view_policy::set({(typename view_policy::TargetCastType)m_pData, capacity()}, pos);
			}

			GAIA_NODISCARD auto operator[](size_type pos) const noexcept -> decltype(view_policy::get({}, 0)) {
				GAIA_ASSERT(pos < size());
				return view_policy::get({(typename view_policy::TargetCastType)m_pData, capacity()}, pos);
			}

			void reserve(size_type count) {
				if (count <= m_cap)
					return;

				auto* pDataOld = m_pData;
				m_pData = view_policy::alloc_mem(count);
				if (pDataOld != nullptr) {
					utils::move_elements<T>(m_pData, pDataOld, 0, size(), count, m_cap);
					view_policy::free_mem(pDataOld, size());
				}

				m_cap = count;
			}

			void resize(size_type count) {
				if (count <= m_cap) {
					m_cnt = count;
					return;
				}

				auto* pDataOld = m_pData;
				m_pData = view_policy::alloc_mem(count);
				if GAIA_LIKELY (pDataOld != nullptr) {
					utils::move_elements<T>(m_pData, pDataOld, 0, size(), count, m_cap);
					view_policy::free_mem(pDataOld, size());
				}

				m_cap = count;
				m_cnt = count;
			}

			void push_back(const T& arg) {
				try_grow();

				if constexpr (utils::is_soa_layout_v<T>) {
					operator[](m_cnt++) = arg;
				} else {
					auto* ptr = m_pData + sizeof(T) * (m_cnt++);
					::new (ptr) T(arg);
				}
			}

			void push_back(T&& arg) {
				try_grow();

				if constexpr (utils::is_soa_layout_v<T>) {
					operator[](m_cnt++) = std::forward<T>(arg);
				} else {
					auto* ptr = m_pData + sizeof(T) * (m_cnt++);
					::new (ptr) T(std::forward<T>(arg));
				}
			}

			template <typename... Args>
			auto emplace_back(Args&&... args) {
				try_grow();

				if constexpr (utils::is_soa_layout_v<T>) {
					operator[](m_cnt++) = T(std::forward<Args>(args)...);
					return;
				} else {
					auto* ptr = m_pData + sizeof(T) * (m_cnt++);
					::new (ptr) T(std::forward<Args>(args)...);
					return (reference)*ptr;
				}
			}

			void pop_back() noexcept {
				GAIA_ASSERT(!empty());
				if constexpr (utils::is_soa_layout_v<T>) {
					--m_cnt;
				} else {
					auto* ptr = m_pData + sizeof(T) * (--m_cnt);
					((pointer)ptr)->~T();
				}
			}

			iterator erase(iterator pos) noexcept {
				GAIA_ASSERT(pos.m_ptr >= data() && pos.m_ptr < (data() + m_cap - 1));

				const auto idxSrc = (size_type)GAIA_UTIL::distance(pos, begin());
				const auto idxDst = size() - 1;

				utils::shift_elements_left<T>(m_pData, idxSrc, idxDst, m_cap);
				--m_cnt;

				return iterator((pointer)m_pData + idxSrc);
			}

			iterator_soa erase(iterator_soa pos) noexcept {
				const auto idxSrc = pos.m_idx;
				const auto idxDst = size() - 1;

				utils::shift_elements_left<T>(m_pData, idxSrc, idxDst);
				--m_cnt;

				return iterator_soa(m_pData, m_cnt, idxSrc);
			}

			iterator erase(iterator first, iterator last) noexcept {
				static_assert(!utils::is_soa_layout_v<T>);
				GAIA_ASSERT(first.m_cnt >= 0 && first.m_cnt < size());
				GAIA_ASSERT(last.m_cnt >= 0 && last.m_cnt < size());
				GAIA_ASSERT(last.m_cnt >= first.m_cnt);

				const auto cnt = last.m_cnt - first.m_cnt;
				utils::shift_elements_left<T>(m_pData, first.cnt, last.cnt);
				m_cnt -= cnt;

				return {(pointer)m_pData + size_type(last.m_cnt)};
			}

			iterator_soa erase(iterator_soa first, iterator_soa last) noexcept {
				static_assert(!utils::is_soa_layout_v<T>);
				GAIA_ASSERT(first.m_idx >= 0 && first.m_idx < size());
				GAIA_ASSERT(last.m_idx >= 0 && last.m_idx < size());
				GAIA_ASSERT(last.m_idx >= first.m_idx);

				const auto cnt = last.m_idx - first.m_idx;
				utils::shift_elements_left<T>(m_pData, first.cnt, last.cnt);
				m_cnt -= cnt;

				return iterator_soa(m_pData, m_cnt, last.m_cnt);
			}

			void clear() {
				resize(0);
			}

			void shirk_to_fit() {
				if (capacity() == size())
					return;

				auto* pDataOld = m_pData;
				m_pData = view_policy::mem_alloc(m_cap = size());
				utils::move_elements<T>(m_pData, pDataOld, 0, size());
				view_policy::mem_free(pDataOld);
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

			GAIA_NODISCARD auto front() noexcept {
				GAIA_ASSERT(!empty());
				if constexpr (utils::is_soa_layout_v<T>)
					return *begin();
				else
					return (reference)*begin();
			}

			GAIA_NODISCARD auto front() const noexcept {
				GAIA_ASSERT(!empty());
				if constexpr (utils::is_soa_layout_v<T>)
					return *begin();
				else
					return (const_reference)*begin();
			}

			GAIA_NODISCARD auto back() noexcept {
				GAIA_ASSERT(!empty());
				if constexpr (utils::is_soa_layout_v<T>)
					return operator[](m_cnt - 1);
				else
					return (reference)(operator[](m_cnt - 1));
			}

			GAIA_NODISCARD auto back() const noexcept {
				GAIA_ASSERT(!empty());
				if constexpr (utils::is_soa_layout_v<T>)
					return operator[](m_cnt - 1);
				else
					return (const_reference) operator[](m_cnt - 1);
			}

			GAIA_NODISCARD auto begin() const noexcept {
				if constexpr (utils::is_soa_layout_v<T>)
					return iterator_soa(m_pData, size(), 0);
				else
					return iterator((pointer)m_pData);
			}

			GAIA_NODISCARD auto rbegin() const noexcept {
				if constexpr (utils::is_soa_layout_v<T>)
					return iterator_soa(m_pData, size(), size() - 1);
				else
					return iterator((pointer)&back());
			}

			GAIA_NODISCARD auto end() const noexcept {
				if constexpr (utils::is_soa_layout_v<T>)
					return iterator_soa(m_pData, size(), size());
				else
					return iterator((pointer)m_pData + size());
			}

			GAIA_NODISCARD auto rend() const noexcept {
				if constexpr (utils::is_soa_layout_v<T>)
					return iterator_soa(m_pData, size(), -1);
				else
					return iterator((pointer)m_pData - 1);
			}

			GAIA_NODISCARD bool operator==(const darr& other) const {
				if (m_cnt != other.m_cnt)
					return false;
				const size_type n = size();
				for (size_type i = 0; i < n; ++i)
					if (!(operator[](i) == other[i]))
						return false;
				return true;
			}
		};
	} // namespace containers

} // namespace gaia