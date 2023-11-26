#pragma once
#include "../../config/config.h"

#include <cstddef>
#include <initializer_list>
#include <type_traits>
#include <utility>

#include "../../core/iterator.h"
#include "../../core/utility.h"
#include "../../mem/data_layout_policy.h"
#include "../../mem/mem_utils.h"

namespace gaia {
	namespace cnt {
		namespace darr_ext_detail {
			using difference_type = uint32_t;
			using size_type = uint32_t;
		} // namespace darr_ext_detail

		template <typename T>
		struct darr_ext_iterator {
			using iterator_category = core::random_access_iterator_tag;
			using value_type = T;
			using pointer = T*;
			using reference = T&;
			using difference_type = darr_ext_detail::difference_type;
			using size_type = darr_ext_detail::size_type;

			using iterator = darr_ext_iterator;

		private:
			pointer m_ptr;

		public:
			darr_ext_iterator(T* ptr): m_ptr(ptr) {}

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

		template <typename T>
		struct darr_ext_iterator_soa {
			using iterator_category = core::random_access_iterator_tag;
			using value_type = T;
			// using pointer = T*; not supported
			// using reference = T&; not supported
			using difference_type = darr_ext_detail::difference_type;
			using size_type = darr_ext_detail::size_type;

			using iterator = darr_ext_iterator_soa;

		private:
			uint8_t* m_ptr;
			uint32_t m_cnt;
			uint32_t m_idx;

		public:
			darr_ext_iterator_soa(uint8_t* ptr, uint32_t cnt, uint32_t idx): m_ptr(ptr), m_cnt(cnt), m_idx(idx) {}

			T operator*() const {
				return mem::data_view_policy<T::Layout, T>::get({m_ptr, m_cnt}, m_idx);
			}
			T operator->() const {
				return mem::data_view_policy<T::Layout, T>::get({m_ptr, m_cnt}, m_idx);
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

		//! Array of elements of type \tparam T allocated on heap or stack. Stack capacity is \tparam N elements.
		//! If the number of elements is bellow \tparam N the stack storage is used.
		//! If the number of elements is above \tparam N the heap storage is used.
		//! Interface compatiblity with std::vector and std::array where it matters.
		template <typename T, darr_ext_detail::size_type N>
		class darr_ext {
		public:
			static_assert(N > 0);

			using iterator_category = core::random_access_iterator_tag;
			using value_type = T;
			using reference = T&;
			using const_reference = const T&;
			using pointer = T*;
			using const_pointer = T*;
			using view_policy = mem::auto_view_policy<T>;
			using difference_type = darr_ext_detail::difference_type;
			using size_type = darr_ext_detail::size_type;

			using iterator = darr_ext_iterator<T>;
			using iterator_soa = darr_ext_iterator_soa<T>;

			static constexpr size_type extent = N;
			static constexpr uint32_t allocated_bytes = view_policy::get_min_byte_size(0, N);

		private:
			//! Data allocated on the stack
			mem::raw_data_holder<T, allocated_bytes> m_data;
			//! Data allocated on the heap
			uint8_t* m_pDataHeap = nullptr;
			//! Pointer to the currently used data
			uint8_t* m_pData = m_data;
			//! Number of currently used items ín this container
			size_type m_cnt = size_type(0);
			//! Allocated capacity of m_dataDyn or the extend
			size_type m_cap = extent;

			void try_grow() {
				const auto cnt = size();
				const auto cap = capacity();

				// Unless we reached the capacity don't do anything
				if GAIA_LIKELY (cnt < cap)
					return;

				// We increase the capacity in multiples of 1.5 which is about the golden ratio (1.618).
				// This means we prefer more frequent allocations over memory fragmentation.
				m_cap = (cap * 3 + 1) / 2;

				if GAIA_UNLIKELY (m_pDataHeap == nullptr) {
					// If no heap memory is allocated yet we need to allocate it and move the old stack elements to it
					m_pDataHeap = view_policy::alloc_mem(m_cap);
					mem::move_elements<T>(m_pDataHeap, m_data, 0, cnt, m_cap, cap);
				} else {
					// Move items from the old heap array to the new one. Delete the old
					auto* pDataOld = m_pDataHeap;
					m_pDataHeap = view_policy::alloc_mem(m_cap);
					mem::move_elements<T>(m_pDataHeap, pDataOld, 0, cnt, m_cap, cap);
					view_policy::free_mem(pDataOld, cnt);
				}

				m_pData = m_pDataHeap;
			}

		public:
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
				const auto count = (size_type)core::distance(first, last);
				resize(count);

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

			darr_ext(std::initializer_list<T> il): darr_ext(il.begin(), il.end()) {}

			darr_ext(const darr_ext& other): darr_ext(other.begin(), other.end()) {}

			darr_ext(darr_ext&& other) noexcept: m_cnt(other.m_cnt), m_cap(other.m_cap) {
				GAIA_ASSERT(core::addressof(other) != this);

				if (other.m_pData == other.m_pDataHeap) {
					if (m_pData == m_pDataHeap)
						view_policy::free_mem(m_pDataHeap);
					m_pData = m_pDataHeap;
					m_pDataHeap = other.m_pDataHeap;
				} else {
					m_pData = m_data;
					mem::move_elements<T>(m_data, other.m_data, 0, other.size());
					m_pDataHeap = nullptr;
				}

				other.m_pDataHeap = nullptr;
				other.m_pData = m_data;
				other.m_cnt = size_type(0);
				other.m_cap = extent;
			}

			darr_ext& operator=(std::initializer_list<T> il) {
				*this = darr_ext(il.begin(), il.end());
				return *this;
			}

			darr_ext& operator=(const darr_ext& other) {
				GAIA_ASSERT(core::addressof(other) != this);

				resize(other.size());
				mem::copy_elements<T>(
						(uint8_t*)m_pData, (const uint8_t*)other.m_pData, 0, other.size(), capacity(), other.capacity());

				return *this;
			}

			darr_ext& operator=(darr_ext&& other) noexcept {
				GAIA_ASSERT(core::addressof(other) != this);

				// Moving from heap-allocated source
				if (other.m_pDataHeap != nullptr) {
					// Release current heap memory and replace it with the source
					if (m_pDataHeap != nullptr)
						view_policy::free(m_pDataHeap, size());
					m_pDataHeap = other.m_pDataHeap;
					m_pData = m_pDataHeap;
				} else
				// Moving from stack-allocated source
				{
					resize(other.size());
					mem::move_elements<T>(m_data, other.m_data, 0, other.m_data.size());
					m_pDataHeap = nullptr;
					m_pData = m_data;
				}
				m_cnt = other.m_cnt;
				m_cap = other.m_cap;

				other.m_cnt = size_type(0);
				other.m_cap = extent;
				other.m_pDataHeap = nullptr;
				other.m_pData = m_data;

				return *this;
			}

			~darr_ext() {
				view_policy::free_mem(m_pDataHeap, size());
			}

			GAIA_CLANG_WARNING_PUSH()
			// Memory is aligned so we can silence this warning
			GAIA_CLANG_WARNING_DISABLE("-Wcast-align")

			GAIA_NODISCARD pointer data() noexcept {
				return (pointer)m_pData;
			}

			GAIA_NODISCARD const_pointer data() const noexcept {
				return (const_pointer)m_pData;
			}

			GAIA_NODISCARD decltype(auto) operator[](size_type pos) noexcept {
				GAIA_ASSERT(pos < size());
				return view_policy::set({(typename view_policy::TargetCastType)m_pData, size()}, pos);
			}

			GAIA_NODISCARD decltype(auto) operator[](size_type pos) const noexcept {
				GAIA_ASSERT(pos < size());
				return view_policy::get({(typename view_policy::TargetCastType)m_pData, size()}, pos);
			}

			GAIA_CLANG_WARNING_POP()

			void reserve(size_type count) {
				if (count <= m_cap)
					return;

				if (m_pDataHeap) {
					auto* pDataOld = m_pDataHeap;
					m_pDataHeap = view_policy::alloc_mem(count);
					mem::move_elements<T>(m_pDataHeap, pDataOld, 0, size(), count, m_cap);
					view_policy::free_mem(pDataOld, size());
				} else {
					m_pDataHeap = view_policy::alloc_mem(count);
					mem::move_elements<T>(m_pDataHeap, m_data, 0, size(), count, m_cap);
				}

				m_cap = count;
				m_pData = m_pDataHeap;
			}

			void resize(size_type count) {
				// Resizing to a smaller size
				if (count <= m_cnt) {
					// Destroy elements at the endif (count <= m_cap) {
					if constexpr (!mem::is_soa_layout_v<T>)
						core::call_dtor_n(&data()[count], size() - count);

					m_cnt = count;
					return;
				}

				// Resizing to a bigger size but still within allocated capacity
				if (count <= m_cap) {
					// Constuct new elements
					if constexpr (!mem::is_soa_layout_v<T>)
						core::call_ctor_n(&data()[size()], count - size());

					m_cnt = count;
					return;
				}

				auto* pDataOld = m_pDataHeap;
				m_pDataHeap = view_policy::alloc_mem(count);
				if (pDataOld != nullptr) {
					mem::move_elements<T>(m_pDataHeap, pDataOld, 0, size(), count, m_cap);
					// Default-construct new items
					core::call_ctor_n(&data()[size()], count - size());
					// Release old memory
					view_policy::free_mem(pDataOld, size());
				} else {
					mem::move_elements<T>(m_pDataHeap, m_data, 0, size(), count, m_cap);
				}

				m_cap = count;
				m_cnt = count;
				m_pData = m_pDataHeap;
			}

			void push_back(const T& arg) {
				try_grow();

				if constexpr (mem::is_soa_layout_v<T>) {
					operator[](m_cnt++) = arg;
				} else {
					auto* ptr = &data()[m_cnt++];
					core::call_ctor(ptr, arg);
				}
			}

			void push_back(T&& arg) {
				try_grow();

				if constexpr (mem::is_soa_layout_v<T>) {
					operator[](m_cnt++) = GAIA_FWD(arg);
				} else {
					auto* ptr = &data()[m_cnt++];
					core::call_ctor(ptr, GAIA_FWD(arg));
				}
			}

			template <typename... Args>
			auto emplace_back(Args&&... args) {
				try_grow();

				if constexpr (mem::is_soa_layout_v<T>) {
					operator[](m_cnt++) = T(GAIA_FWD(args)...);
					return;
				} else {
					auto* ptr = &data()[m_cnt++];
					core::call_ctor(ptr, GAIA_FWD(args)...);
					return (reference)*ptr;
				}
			}

			void pop_back() noexcept {
				GAIA_ASSERT(!empty());

				if constexpr (!mem::is_soa_layout_v<T>)
					core::call_dtor(&data()[m_cnt]);

				--m_cnt;
			}

			//! Removes the element at pos
			//! \param pos Iterator to the element to remove
			iterator erase(iterator pos) noexcept {
				GAIA_ASSERT(pos >= data());
				GAIA_ASSERT(empty() || (pos < iterator(data() + size())));

				if (empty())
					return end();

				const auto idxSrc = (size_type)core::distance(begin(), pos);
				const auto idxDst = (size_type)core::distance(begin(), end()) - 1;

				mem::shift_elements_left<T>(m_pData, idxSrc, idxDst, m_cap);
				// Destroy if it's the last element
				if constexpr (!mem::is_soa_layout_v<T>)
					core::call_dtor(&data()[m_cnt - 1]);

				--m_cnt;

				return iterator(&data()[idxSrc]);
			}

			//! Removes the elements in the range [first, last)
			//! \param first Iterator to the element to remove
			//! \param last Iterator to the one beyond the last element to remove
			iterator erase(iterator first, iterator last) noexcept {
				GAIA_ASSERT(first >= data())
				GAIA_ASSERT(empty() || (first < iterator(data() + size())));
				GAIA_ASSERT(last > first);
				GAIA_ASSERT(last <= iterator(data() + size()));

				if (empty())
					return end();

				const auto idxSrc = (size_type)core::distance(begin(), first);
				const auto idxDst = size();
				const auto cnt = last - first;

				mem::shift_elements_left_n<T>(m_pData, idxSrc, idxDst, cnt, m_cap);
				// Destroy if it's the last element
				if constexpr (!mem::is_soa_layout_v<T>)
					core::call_dtor_n(&data()[m_cnt - cnt], cnt);

				m_cnt -= cnt;

				return iterator(&data()[idxSrc]);
			}

			void clear() {
				resize(0);
			}

			void shrink_to_fit() {
				if (capacity() == size())
					return;

				if (m_pDataHeap != nullptr) {
					auto* pDataOld = m_pDataHeap;

					if (size() < extent) {
						mem::move_elements<T>(m_data, pDataOld, 0, size());
						m_pData = m_data;
					} else {
						m_pDataHeap = view_policy::mem_alloc(m_cap = size());
						mem::move_elements<T>(m_pDataHeap, pDataOld, 0, size());
						m_pData = m_pDataHeap;
					}

					view_policy::mem_free(pDataOld);
				} else
					resize(size());
			}

			GAIA_NODISCARD size_type size() const noexcept {
				return m_cnt;
			}

			GAIA_NODISCARD bool empty() const noexcept {
				return size() == 0;
			}

			GAIA_NODISCARD size_type capacity() const noexcept {
				return m_cap;
			}

			GAIA_NODISCARD size_type max_size() const noexcept {
				return N;
			}

			GAIA_NODISCARD auto front() noexcept {
				GAIA_ASSERT(!empty());
				if constexpr (mem::is_soa_layout_v<T>)
					return *begin();
				else
					return (reference)*begin();
			}

			GAIA_NODISCARD auto front() const noexcept {
				GAIA_ASSERT(!empty());
				if constexpr (mem::is_soa_layout_v<T>)
					return *begin();
				else
					return (const_reference)*begin();
			}

			GAIA_NODISCARD auto back() noexcept {
				GAIA_ASSERT(!empty());
				if constexpr (mem::is_soa_layout_v<T>)
					return operator[](m_cnt - 1);
				else
					return (reference) operator[](m_cnt - 1);
			}

			GAIA_NODISCARD auto back() const noexcept {
				GAIA_ASSERT(!empty());
				if constexpr (mem::is_soa_layout_v<T>)
					return operator[](m_cnt - 1);
				else
					return (const_reference) operator[](m_cnt - 1);
			}

			GAIA_NODISCARD auto begin() const noexcept {
				if constexpr (mem::is_soa_layout_v<T>)
					return iterator_soa(m_pData, size(), 0);
				else
					return iterator(data());
			}

			GAIA_NODISCARD auto rbegin() const noexcept {
				if constexpr (mem::is_soa_layout_v<T>)
					return iterator_soa(m_pData, size(), size() - 1);
				else
					return iterator((pointer)&back());
			}

			GAIA_NODISCARD auto end() const noexcept {
				if constexpr (mem::is_soa_layout_v<T>)
					return iterator_soa(m_pData, size(), size());
				else
					return iterator(data() + size());
			}

			GAIA_NODISCARD auto rend() const noexcept {
				if constexpr (mem::is_soa_layout_v<T>)
					return iterator_soa(m_pData, size(), -1);
				else
					return iterator(data() - 1);
			}

			GAIA_NODISCARD bool operator==(const darr_ext& other) const {
				if (m_cnt != other.m_cnt)
					return false;
				const size_type n = size();
				for (size_type i = 0; i < n; ++i)
					if (!(operator[](i) == other[i]))
						return false;
				return true;
			}

			GAIA_NODISCARD constexpr bool operator!=(const darr_ext& other) const {
				return !operator==(other);
			}

			template <size_t Item>
			auto soa_view_mut() noexcept {
				return mem::data_view_policy<T::Layout, T>::template get<Item>(
						std::span<uint8_t>{(uint8_t*)m_pData, capacity()});
			}

			template <size_t Item>
			auto soa_view() const noexcept {
				return mem::data_view_policy<T::Layout, T>::template get<Item>(
						std::span<const uint8_t>{(const uint8_t*)m_pData, capacity()});
			}
		};

		namespace detail {
			template <typename T, uint32_t N, uint32_t... I>
			darr_ext<std::remove_cv_t<T>, N> to_sarray_impl(T (&a)[N], std::index_sequence<I...> /*no_name*/) {
				return {{a[I]...}};
			}
		} // namespace detail

		template <typename T, uint32_t N>
		darr_ext<std::remove_cv_t<T>, N> to_sarray(T (&a)[N]) {
			return detail::to_sarray_impl(a, std::make_index_sequence<N>{});
		}

	} // namespace cnt

} // namespace gaia
