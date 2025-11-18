#pragma once
#include "gaia/config/config.h"

#include <cstddef>
#include <initializer_list>
#include <type_traits>
#include <utility>

#include "gaia/core/iterator.h"
#include "gaia/core/utility.h"
#include "gaia/mem/data_layout_policy.h"
#include "gaia/mem/mem_sani.h"
#include "gaia/mem/mem_utils.h"
#include "gaia/mem/raw_data_holder.h"

namespace gaia {
	namespace cnt {
		namespace darr_ext_soa_detail {
			using difference_type = uint32_t;
			using size_type = uint32_t;
		} // namespace darr_ext_soa_detail

		template <typename T>
		struct darr_ext_soa_iterator {
			using value_type = T;
			// using pointer = T*; not supported
			// using reference = T&; not supported
			using difference_type = darr_ext_soa_detail::difference_type;
			using size_type = darr_ext_soa_detail::size_type;

			using iterator = darr_ext_soa_iterator;
			using iterator_category = core::random_access_iterator_tag;

		private:
			uint8_t* m_ptr;
			uint32_t m_cnt;
			uint32_t m_idx;

		public:
			darr_ext_soa_iterator(uint8_t* ptr, uint32_t cnt, uint32_t idx): m_ptr(ptr), m_cnt(cnt), m_idx(idx) {}

			T operator*() const {
				return mem::data_view_policy<T::gaia_Data_Layout, T>::get({m_ptr, m_cnt}, m_idx);
			}
			T operator->() const {
				return mem::data_view_policy<T::gaia_Data_Layout, T>::get({m_ptr, m_cnt}, m_idx);
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

		template <typename T>
		struct const_darr_ext_soa_iterator {
			using value_type = T;
			// using pointer = T*; not supported
			// using reference = T&; not supported
			using difference_type = darr_ext_soa_detail::difference_type;
			using size_type = darr_ext_soa_detail::size_type;

			using iterator = const_darr_ext_soa_iterator;
			using iterator_category = core::random_access_iterator_tag;

		private:
			const uint8_t* m_ptr;
			uint32_t m_cnt;
			uint32_t m_idx;

		public:
			const_darr_ext_soa_iterator(const uint8_t* ptr, uint32_t cnt, uint32_t idx): m_ptr(ptr), m_cnt(cnt), m_idx(idx) {}

			T operator*() const {
				return mem::data_view_policy<T::gaia_Data_Layout, T>::get({m_ptr, m_cnt}, m_idx);
			}
			T operator->() const {
				return mem::data_view_policy<T::gaia_Data_Layout, T>::get({m_ptr, m_cnt}, m_idx);
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
		template <typename T, darr_ext_soa_detail::size_type N, typename Allocator = mem::DefaultAllocatorAdaptor>
		class darr_ext_soa {
			static_assert(mem::is_soa_layout_v<T>, "darr_ext_soa can be used only with soa types");

		public:
			static_assert(N > 0);

			using value_type = T;
			using reference = T&;
			using const_reference = const T&;
			using pointer = T*;
			using const_pointer = const T*;
			using view_policy = mem::data_view_policy_soa<T::gaia_Data_Layout, T>;
			using difference_type = darr_ext_soa_detail::difference_type;
			using size_type = darr_ext_soa_detail::size_type;

			using iterator = darr_ext_soa_iterator<T>;
			using const_iterator = const_darr_ext_soa_iterator<T>;
			using iterator_category = core::random_access_iterator_tag;

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
					m_pDataHeap = view_policy::template alloc<Allocator>(m_cap);
					view_policy::mem_add_block(m_pDataHeap, m_cap, cnt);
					mem::move_elements<T>(m_pDataHeap, m_data, cnt, 0, m_cap, cap);
				} else {
					// Move items from the old heap array to the new one. Delete the old
					auto* pDataOld = m_pDataHeap;
					m_pDataHeap = view_policy::template alloc<Allocator>(m_cap);
					view_policy::mem_add_block(m_pDataHeap, m_cap, cnt);
					mem::move_elements<T>(m_pDataHeap, pDataOld, cnt, 0, m_cap, cap);
					view_policy::template free<Allocator>(pDataOld, cap, cnt);
				}

				m_pData = m_pDataHeap;
			}

		public:
			darr_ext_soa() noexcept = default;
			darr_ext_soa(core::zero_t) noexcept {}

			darr_ext_soa(size_type count, const T& value) {
				resize(count);
				for (auto it: *this)
					*it = value;
			}

			darr_ext_soa(size_type count) {
				resize(count);
			}

			template <typename InputIt>
			darr_ext_soa(InputIt first, InputIt last) {
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

			darr_ext_soa(std::initializer_list<T> il): darr_ext_soa(il.begin(), il.end()) {}

			darr_ext_soa(const darr_ext_soa& other): darr_ext_soa(other.begin(), other.end()) {}

			darr_ext_soa(darr_ext_soa&& other) noexcept {
				GAIA_ASSERT(core::addressof(other) != this);

				// Moving from stack-allocated source
				if (other.m_pDataHeap == nullptr) {
					view_policy::mem_add_block(m_data, extent, other.size());
					mem::move_elements<T>(m_data, other.m_data, other.size(), 0, extent, other.extent);
					view_policy::mem_del_block(other.m_data, extent, other.size());
					m_pDataHeap = nullptr;
					m_pData = m_data;
				} else {
					m_pDataHeap = other.m_pDataHeap;
					m_pData = m_pDataHeap;
				}

				m_cnt = other.m_cnt;
				m_cap = other.m_cap;

				other.m_pDataHeap = nullptr;
				other.m_pData = other.m_data;
				other.m_cnt = size_type(0);
				other.m_cap = extent;
			}

			darr_ext_soa& operator=(std::initializer_list<T> il) {
				*this = darr_ext_soa(il.begin(), il.end());
				return *this;
			}

			darr_ext_soa& operator=(const darr_ext_soa& other) {
				GAIA_ASSERT(core::addressof(other) != this);

				resize(other.size());
				mem::copy_elements<T>(
						(uint8_t*)m_pData, (const uint8_t*)other.m_pData, other.size(), 0, capacity(), other.capacity());

				return *this;
			}

			darr_ext_soa& operator=(darr_ext_soa&& other) noexcept {
				GAIA_ASSERT(core::addressof(other) != this);

				// Release previously allocated memory if there was anything
				view_policy::template free<Allocator>(m_pDataHeap, m_cap, m_cnt);

				// Moving from stack-allocated source
				if (other.m_pDataHeap == nullptr) {
					view_policy::mem_add_block(m_data, extent, other.size());
					mem::move_elements<T>(m_data, other.m_data, other.size(), 0, extent, other.extent);
					view_policy::mem_del_block(other.m_data, extent, other.size());
					m_pDataHeap = nullptr;
					m_pData = m_data;
				} else {
					m_pDataHeap = other.m_pDataHeap;
					m_pData = m_pDataHeap;
				}

				m_cnt = other.m_cnt;
				m_cap = other.m_cap;

				other.m_cnt = size_type(0);
				other.m_cap = extent;
				other.m_pDataHeap = nullptr;
				other.m_pData = other.m_data;

				return *this;
			}

			~darr_ext_soa() {
				view_policy::template free<Allocator>(m_pDataHeap, m_cap, m_cnt);
			}

			GAIA_CLANG_WARNING_PUSH()
			// Memory is aligned so we can silence this warning
			GAIA_CLANG_WARNING_DISABLE("-Wcast-align")

			GAIA_NODISCARD pointer data() noexcept {
				return reinterpret_cast<pointer>(m_pData);
			}

			GAIA_NODISCARD const_pointer data() const noexcept {
				return reinterpret_cast<const_pointer>(m_pData);
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

			void reserve(size_type cap) {
				if (cap <= m_cap)
					return;

				auto* pDataOld = m_pDataHeap;
				m_pDataHeap = view_policy::template alloc<Allocator>(cap);
				view_policy::mem_add_block(m_pDataHeap, cap, m_cnt);
				if (m_pDataHeap) {
					mem::move_elements<T>(m_pDataHeap, pDataOld, m_cnt, 0, cap, m_cap);
					view_policy::template free<Allocator>(pDataOld, m_cap, m_cnt);
				} else {
					m_pDataHeap = view_policy::template alloc<Allocator>(cap);
					mem::move_elements<T>(m_pDataHeap, m_data, m_cnt, 0, cap, m_cap);
				}

				m_cap = cap;
				m_pData = m_pDataHeap;
			}

			void resize(size_type count) {
				if (count == m_cnt)
					return;

				// Resizing to a smaller size
				if (count < m_cnt) {
					view_policy::mem_pop_block(data(), m_cap, m_cnt, m_cnt - count);

					m_cnt = count;
					return;
				}

				// Resizing to a bigger size but still within allocated capacity
				if (count <= m_cap) {
					view_policy::mem_push_block(data(), m_cap, m_cnt, count - m_cnt);

					m_cnt = count;
					return;
				}

				auto* pDataOld = m_pDataHeap;
				m_pDataHeap = view_policy::template alloc<Allocator>(count);
				view_policy::mem_add_block(m_pDataHeap, count, count);
				if (pDataOld != nullptr) {
					mem::move_elements<T>(m_pDataHeap, pDataOld, m_cnt, 0, count, m_cap);
					view_policy::template free<Allocator>(pDataOld, m_cap, m_cnt);
				} else {
					mem::move_elements<T>(m_pDataHeap, m_data, m_cnt, 0, count, m_cap);
					view_policy::mem_del_block(m_data, m_cap, m_cnt);
				}

				m_cap = count;
				m_cnt = count;
				m_pData = m_pDataHeap;
			}

			void push_back(const T& arg) {
				try_grow();

				view_policy::mem_push_block(data(), m_cap, m_cnt, 1);
				operator[](m_cnt++) = arg;
			}

			void push_back(T&& arg) {
				try_grow();

				view_policy::mem_push_block(data(), m_cap, m_cnt, 1);
				operator[](m_cnt++) = GAIA_MOV(arg);
			}

			template <typename... Args>
			decltype(auto) emplace_back(Args&&... args) {
				try_grow();

				view_policy::mem_push_block(data(), m_cap, m_cnt, 1);
				operator[](m_cnt++) = T(GAIA_FWD(args)...);
			}

			void pop_back() noexcept {
				GAIA_ASSERT(!empty());

				view_policy::mem_pop_block(data(), m_cap, m_cnt, 1);

				--m_cnt;
			}

			//! Insert the element to the position given by iterator \param pos
			iterator insert(iterator pos, const T& arg) noexcept {
				GAIA_ASSERT(pos >= data());
				GAIA_ASSERT(empty() || (pos < iterator(data() + size())));

				const auto idxSrc = (size_type)core::distance(begin(), pos);
				try_grow();
				const auto idxDst = (size_type)core::distance(begin(), end()) + 1;

				view_policy::mem_push_block(data(), m_cap, m_cnt, 1);
				mem::shift_elements_right<T>(m_pData, idxDst, idxSrc, m_cap);

				operator[](idxSrc) = arg;

				++m_cnt;

				return iterator(&data()[idxSrc]);
			}

			//! Insert the element to the position given by iterator \param pos
			iterator insert(iterator pos, T&& arg) noexcept {
				GAIA_ASSERT(pos >= data());
				GAIA_ASSERT(empty() || (pos < iterator(data() + size())));

				const auto idxSrc = (size_type)core::distance(begin(), pos);
				try_grow();
				const auto idxDst = (size_type)core::distance(begin(), end());

				view_policy::mem_push_block(data(), m_cap, m_cnt, 1);
				mem::shift_elements_right<T>(m_pData, idxDst, idxSrc, m_cap);

				operator[](idxSrc) = GAIA_MOV(arg);

				++m_cnt;

				return iterator(&data()[idxSrc]);
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

				mem::shift_elements_left<T>(m_pData, idxDst, idxSrc, m_cap);
				view_policy::mem_pop_block(data(), m_cap, m_cnt, 1);

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
				const auto cnt = (size_type)(last - first);

				mem::shift_elements_left_n<T>(m_pData, idxDst, idxSrc, cnt, m_cap);
				view_policy::mem_pop_block(data(), m_cap, m_cnt, cnt);

				m_cnt -= cnt;

				return iterator(&data()[idxSrc]);
			}

			void clear() {
				resize(0);
			}

			void shrink_to_fit() {
				const auto cap = capacity();
				const auto cnt = size();

				if (cap == cnt)
					return;

				if (m_pDataHeap != nullptr) {
					auto* pDataOld = m_pDataHeap;

					if (cnt < extent) {
						mem::move_elements<T>(m_data, pDataOld, cnt, 0);
						m_pData = m_data;
						m_cap = extent;
					} else {
						m_pDataHeap = view_policy::template alloc<Allocator>(m_cap = cnt);
						view_policy::mem_add_block(m_pDataHeap, m_cap, m_cnt);
						mem::move_elements<T>(m_pDataHeap, pDataOld, cnt, 0);
						m_pData = m_pDataHeap;
					}

					view_policy::mem_del_block(pDataOld, cap, cnt);
					view_policy::template free<Allocator>(pDataOld);
				} else
					resize(cnt);
			}

			//! Removes all elements that fail the predicate.
			//! \param func A lambda or a functor with the bool operator()(Container::value_type&) overload.
			//! \return The new size of the array.
			template <typename Func>
			auto retain(Func&& func) {
				size_type erased = 0;
				size_type idxDst = 0;
				size_type idxSrc = 0;

				while (idxSrc < m_cnt) {
					if (func(operator[](idxSrc))) {
						if (idxDst < idxSrc) {
							auto* ptr = (uint8_t*)data();
							mem::move_elements<T>(ptr, ptr, idxDst, idxSrc, m_cap, m_cap);
						}
						++idxDst;
					} else {
						++erased;
					}

					++idxSrc;
				}

				view_policy::mem_pop_block(data(), m_cap, m_cnt, erased);

				m_cnt -= erased;
				return idxDst;
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

			GAIA_NODISCARD decltype(auto) front() noexcept {
				GAIA_ASSERT(!empty());
				return *begin();
			}

			GAIA_NODISCARD decltype(auto) front() const noexcept {
				GAIA_ASSERT(!empty());
				return *begin();
			}

			GAIA_NODISCARD decltype(auto) back() noexcept {
				GAIA_ASSERT(!empty());
				return operator[](m_cnt - 1);
			}

			GAIA_NODISCARD decltype(auto) back() const noexcept {
				GAIA_ASSERT(!empty());
				return operator[](m_cnt - 1);
			}

			GAIA_NODISCARD auto begin() noexcept {
				return iterator(m_pData, size(), 0);
			}

			GAIA_NODISCARD auto begin() const noexcept {
				return const_iterator(m_pData, size(), 0);
			}

			GAIA_NODISCARD auto cbegin() const noexcept {
				return const_iterator(m_pData, size(), 0);
			}

			GAIA_NODISCARD auto rbegin() noexcept {
				return iterator(m_pData, size(), size() - 1);
			}

			GAIA_NODISCARD auto rbegin() const noexcept {
				return const_iterator(m_pData, size(), size() - 1);
			}

			GAIA_NODISCARD auto crbegin() const noexcept {
				return const_iterator(m_pData, size(), size() - 1);
			}

			GAIA_NODISCARD auto end() noexcept {
				return iterator(m_pData, size(), size());
			}

			GAIA_NODISCARD auto end() const noexcept {
				return const_iterator(m_pData, size(), size());
			}

			GAIA_NODISCARD auto cend() const noexcept {
				return const_iterator(m_pData, size(), size());
			}

			GAIA_NODISCARD auto rend() noexcept {
				return iterator(m_pData, size(), -1);
			}

			GAIA_NODISCARD auto rend() const noexcept {
				return const_iterator(m_pData, size(), -1);
			}

			GAIA_NODISCARD auto crend() const noexcept {
				return const_iterator(m_pData, size(), -1);
			}

			GAIA_NODISCARD bool operator==(const darr_ext_soa& other) const {
				if (m_cnt != other.m_cnt)
					return false;
				const size_type n = size();
				for (size_type i = 0; i < n; ++i)
					if (!(operator[](i) == other[i]))
						return false;
				return true;
			}

			GAIA_NODISCARD constexpr bool operator!=(const darr_ext_soa& other) const {
				return !operator==(other);
			}

			template <size_t Item>
			auto view_mut() noexcept {
				return mem::data_view_policy<T::gaia_Data_Layout, T>::template set<Item>(
						std::span<uint8_t>{GAIA_ACC((uint8_t*)m_pData), capacity()});
			}

			template <size_t Item>
			auto view() const noexcept {
				return mem::data_view_policy<T::gaia_Data_Layout, T>::template get<Item>(
						std::span<const uint8_t>{GAIA_ACC((const uint8_t*)m_pData), capacity()});
			}
		};

		namespace detail {
			template <typename T, uint32_t N, uint32_t... I>
			darr_ext_soa<std::remove_cv_t<T>, N> to_sarray_impl(T (&a)[N], std::index_sequence<I...> /*no_name*/) {
				return {{a[I]...}};
			}
		} // namespace detail

		template <typename T, uint32_t N>
		darr_ext_soa<std::remove_cv_t<T>, N> to_sarray(T (&a)[N]) {
			return detail::to_sarray_impl(a, std::make_index_sequence<N>{});
		}

	} // namespace cnt

} // namespace gaia
