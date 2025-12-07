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

namespace gaia {
	namespace cnt {
		namespace darr_soa_detail {
			using difference_type = uint32_t;
			using size_type = uint32_t;
		} // namespace darr_soa_detail

		template <typename T>
		struct darr_soa_iterator {
			using value_type = T;
			// using pointer = T*; not supported
			// using reference = T&; not supported
			using difference_type = darr_soa_detail::difference_type;
			using size_type = darr_soa_detail::size_type;

			using iterator = darr_soa_iterator;
			using iterator_category = core::random_access_iterator_tag;

		private:
			uint8_t* m_ptr;
			uint32_t m_cnt;
			uint32_t m_idx;

		public:
			darr_soa_iterator(uint8_t* ptr, uint32_t cnt, uint32_t idx): m_ptr(ptr), m_cnt(cnt), m_idx(idx) {}

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
		struct const_darr_soa_iterator {
			using value_type = T;
			// using pointer = T*; not supported
			// using reference = T&; not supported
			using difference_type = darr_soa_detail::difference_type;
			using size_type = darr_soa_detail::size_type;

			using iterator = const_darr_soa_iterator;
			using iterator_category = core::random_access_iterator_tag;

		private:
			const uint8_t* m_ptr;
			uint32_t m_cnt;
			uint32_t m_idx;

		public:
			const_darr_soa_iterator(const uint8_t* ptr, uint32_t cnt, uint32_t idx): m_ptr(ptr), m_cnt(cnt), m_idx(idx) {}

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

		//! Array with variable size of elements of type \tparam T allocated on heap.
		//! Interface compatiblity with std::vector where it matters.
		//! Can be used only with SoA types.
		template <typename T, typename Allocator = mem::DefaultAllocatorAdaptor>
		class darr_soa {
			static_assert(mem::is_soa_layout_v<T>, "darr_soa can be used only with soa types");

		public:
			using value_type = T;
			using reference = T&;
			using const_reference = const T&;
			using pointer = T*;
			using const_pointer = const T*;
			using view_policy = mem::data_view_policy_soa<T::gaia_Data_Layout, T>;
			using difference_type = darr_soa_detail::difference_type;
			using size_type = darr_soa_detail::size_type;

			using iterator = darr_soa_iterator<T>;
			using const_iterator = const_darr_soa_iterator<T>;
			using iterator_category = core::random_access_iterator_tag;

		private:
			uint8_t* m_pData = nullptr;
			size_type m_cnt = size_type(0);
			size_type m_cap = size_type(0);

			void try_grow() {
				const auto cnt = size();
				const auto cap = capacity();

				// Unless we reached the capacity don't do anything
				if GAIA_LIKELY (cap != 0 && cnt < cap)
					return;

				// If no data is allocated go with at least 4 elements
				if GAIA_UNLIKELY (m_pData == nullptr) {
					m_pData = view_policy::template alloc<Allocator>(m_cap = 4);
					return;
				}

				// We increase the capacity in multiples of 1.5 which is about the golden ratio (1.618).
				// This effectively means we prefer more frequent allocations over memory fragmentation.
				m_cap = (cap * 3 + 1) / 2;

				auto* pDataOld = m_pData;
				m_pData = view_policy::template alloc<Allocator>(m_cap);
				view_policy::mem_add_block(m_pData, m_cap, cnt);
				mem::move_elements<T, true>(m_pData, pDataOld, cnt, 0, m_cap, cap);
				view_policy::template free<Allocator>(pDataOld, cap, cnt);
			}

		public:
			darr_soa() noexcept = default;
			darr_soa(core::zero_t) noexcept {}

			darr_soa(size_type count, const_reference value) {
				resize(count);
				for (auto it: *this)
					*it = value;
			}

			darr_soa(size_type count) {
				resize(count);
			}

			template <typename InputIt>
			darr_soa(InputIt first, InputIt last) {
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

			darr_soa(std::initializer_list<T> il): darr_soa(il.begin(), il.end()) {}

			darr_soa(const darr_soa& other): darr_soa(other.begin(), other.end()) {}

			darr_soa(darr_soa&& other) noexcept {
				// This is a newly constructed object.
				// It can't have any memory allocated, yet.
				GAIA_ASSERT(m_pData == nullptr);

				m_pData = other.m_pData;
				m_cnt = other.m_cnt;
				m_cap = other.m_cap;

				other.m_cnt = size_type(0);
				other.m_cap = size_type(0);
				other.m_pData = nullptr;
			}

			darr_soa& operator=(std::initializer_list<T> il) {
				*this = darr_soa(il.begin(), il.end());
				return *this;
			}

			darr_soa& operator=(const darr_soa& other) {
				GAIA_ASSERT(core::addressof(other) != this);

				resize(other.size());
				mem::copy_elements<T, true>(
						(uint8_t*)m_pData, (const uint8_t*)other.m_pData, other.size(), 0, capacity(), other.capacity());

				return *this;
			}

			darr_soa& operator=(darr_soa&& other) noexcept {
				GAIA_ASSERT(core::addressof(other) != this);

				// Release previously allocated memory if there was anything
				view_policy::template free<Allocator>(m_pData, m_cap, m_cnt);

				m_pData = other.m_pData;
				m_cnt = other.m_cnt;
				m_cap = other.m_cap;

				other.m_pData = nullptr;
				other.m_cnt = size_type(0);
				other.m_cap = size_type(0);

				return *this;
			}

			~darr_soa() {
				view_policy::template free<Allocator>(m_pData, m_cap, m_cnt);
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
				return view_policy::set({(typename view_policy::TargetCastType)m_pData, capacity()}, pos);
			}

			GAIA_NODISCARD decltype(auto) operator[](size_type pos) const noexcept {
				GAIA_ASSERT(pos < size());
				return view_policy::get({(typename view_policy::TargetCastType)m_pData, capacity()}, pos);
			}

			GAIA_CLANG_WARNING_POP()

			void reserve(size_type cap) {
				if (cap <= m_cap)
					return;

				auto* pDataOld = m_pData;
				m_pData = view_policy::template alloc<Allocator>(cap);

				if (pDataOld != nullptr) {
					view_policy::mem_add_block(m_pData, cap, m_cnt);
					mem::move_elements<T, true>(m_pData, pDataOld, m_cnt, 0, cap, m_cap);
					view_policy::template free<Allocator>(pDataOld, m_cap, m_cnt);
				}

				m_cap = cap;
			}

			void resize(size_type count) {
				if (count == m_cnt)
					return;

				// Fresh allocation
				if (m_pData == nullptr) {
					if (count > 0) {
						m_pData = view_policy::template alloc<Allocator>(count);
						view_policy::mem_add_block(m_pData, count, count);
						m_cap = count;
						m_cnt = count;
					}
					return;
				}

				// Resizing to a smaller size
				if (count < m_cnt) {
					view_policy::mem_pop_block(m_pData, m_cap, m_cnt, m_cnt - count);

					m_cnt = count;
					return;
				}

				// Resizing to a bigger size but still within allocated capacity
				if (count <= m_cap) {
					view_policy::mem_pop_block(m_pData, m_cap, m_cnt, count - m_cnt);

					m_cnt = count;
					return;
				}

				auto* pDataOld = m_pData;
				m_pData = view_policy::template alloc<Allocator>(count);
				view_policy::mem_add_block(m_pData, count, count);
				// Move old data to the new location
				mem::move_elements<T, true>(m_pData, pDataOld, m_cnt, 0, count, m_cap);
				// Release old memory
				view_policy::template free<Allocator>(pDataOld, m_cap, m_cnt);

				m_cap = count;
				m_cnt = count;
			}

			void push_back(const T& arg) {
				try_grow();

				operator[](m_cnt++) = arg;
			}

			void push_back(T&& arg) {
				try_grow();

				view_policy::mem_push_block(m_pData, m_cap, m_cnt, 1);
				operator[](m_cnt++) = GAIA_MOV(arg);
			}

			template <typename... Args>
			decltype(auto) emplace_back(Args&&... args) {
				try_grow();

				view_policy::mem_push_block(m_pData, m_cap, m_cnt, 1);
				operator[](m_cnt++) = T(GAIA_FWD(args)...);
			}

			void pop_back() noexcept {
				GAIA_ASSERT(!empty());

				view_policy::mem_pop_block(m_pData, m_cap, m_cnt, 1);

				--m_cnt;
			}

			//! Insert the element to the position given by iterator @a pos
			//! \param pos Position in the container
			//! \param arg Data to insert
			iterator insert(iterator pos, const T& arg) {
				GAIA_ASSERT(pos >= data());
				GAIA_ASSERT(empty() || (pos < iterator(data() + size())));

				const auto idxSrc = (size_type)core::distance(begin(), pos);
				try_grow();
				const auto idxDst = (size_type)core::distance(begin(), end()) + 1;

				view_policy::mem_push_block(m_pData, m_cap, m_cnt, 1);
				mem::shift_elements_right<T, true>(m_pData, idxDst, idxSrc, m_cap);

				operator[](idxSrc) = arg;

				++m_cnt;

				return iterator(m_pData, capacity(), idxSrc);
			}

			//! Insert the element to the position given by iterator @a pos
			//! \param pos Position in the container
			//! \param arg Data to insert
			iterator insert(iterator pos, T&& arg) {
				GAIA_ASSERT(pos >= data());
				GAIA_ASSERT(empty() || (pos < iterator(data() + size())));

				const auto idxSrc = (size_type)core::distance(begin(), pos);
				try_grow();
				const auto idxDst = (size_type)core::distance(begin(), end());

				view_policy::mem_push_block(m_pData, m_cap, m_cnt, 1);
				mem::shift_elements_right<T, true>(m_pData, idxDst, idxSrc, m_cap);

				operator[](idxSrc) = GAIA_MOV(arg);

				++m_cnt;

				return iterator(m_pData, capacity(), idxSrc);
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

				mem::shift_elements_left<T, true>(m_pData, idxDst, idxSrc, m_cap);
				view_policy::mem_pop_block(m_pData, m_cap, m_cnt, 1);

				--m_cnt;

				return iterator(m_pData, capacity(), idxSrc);
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

				mem::shift_elements_left_fast<T, true>(m_pData, idxDst, idxSrc, cnt, m_cap);
				view_policy::mem_pop_block(m_pData, m_cap, m_cnt, cnt);

				m_cnt -= cnt;

				return iterator(&data()[idxSrc]);
			}

			void clear() noexcept {
				resize(0);
			}

			void shrink_to_fit() {
				const auto cap = capacity();
				const auto cnt = size();

				if (cap == cnt)
					return;

				auto* pDataOld = m_pData;
				m_pData = view_policy::template alloc<Allocator>(m_cap = cnt);
				view_policy::mem_add_block(m_pData, m_cap, m_cnt);
				mem::move_elements<T, true>(m_pData, pDataOld, cnt, 0);
				view_policy::template free<Allocator>(pDataOld, cap, cnt);
			}

			//! Removes all elements that fail the predicate.
			//! \param func A lambda or a functor with the bool operator()(Container::value_type&) overload.
			//! \return The new size of the array.
			template <typename Func>
			auto retain(Func&& func) noexcept {
				size_type erased = 0;
				size_type idxDst = 0;
				size_type idxSrc = 0;

				while (idxSrc < m_cnt) {
					if (func(operator[](idxSrc))) {
						if (idxDst < idxSrc) {
							mem::move_element<T, true>(m_pData, m_pData, idxDst, idxSrc, m_cap, m_cap);
							auto* ptr = &data()[idxSrc];
							core::call_dtor(ptr);
						}
						++idxDst;
					} else {
						auto* ptr = &data()[idxSrc];
						core::call_dtor(ptr);
						++erased;
					}

					++idxSrc;
				}

				view_policy::mem_pop_block(m_pData, m_cap, m_cnt, erased);

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
				return static_cast<size_type>(-1);
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
				return iterator(m_pData, capacity(), 0);
			}

			GAIA_NODISCARD auto begin() const noexcept {
				return const_iterator(m_pData, capacity(), 0);
			}

			GAIA_NODISCARD auto cbegin() const noexcept {
				return const_iterator(m_pData, capacity(), 0);
			}

			GAIA_NODISCARD auto rbegin() noexcept {
				return iterator(m_pData, capacity(), size() - 1);
			}

			GAIA_NODISCARD auto rbegin() const noexcept {
				return const_iterator(m_pData, capacity(), size() - 1);
			}

			GAIA_NODISCARD auto crbegin() const noexcept {
				return const_iterator(m_pData, capacity(), size() - 1);
			}

			GAIA_NODISCARD auto end() noexcept {
				return iterator(m_pData, capacity(), size());
			}

			GAIA_NODISCARD auto end() const noexcept {
				return const_iterator(m_pData, capacity(), size());
			}

			GAIA_NODISCARD auto cend() const noexcept {
				return const_iterator(m_pData, capacity(), size());
			}

			GAIA_NODISCARD auto rend() noexcept {
				return iterator(m_pData, capacity(), -1);
			}

			GAIA_NODISCARD auto rend() const noexcept {
				return const_iterator(m_pData, capacity(), -1);
			}

			GAIA_NODISCARD auto crend() const noexcept {
				return const_iterator(m_pData, capacity(), -1);
			}

			GAIA_NODISCARD bool operator==(const darr_soa& other) const noexcept {
				if (m_cnt != other.m_cnt)
					return false;
				const size_type n = size();
				for (size_type i = 0; i < n; ++i)
					if (!(operator[](i) == other[i]))
						return false;
				return true;
			}

			GAIA_NODISCARD constexpr bool operator!=(const darr_soa& other) const noexcept {
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
	} // namespace cnt

} // namespace gaia