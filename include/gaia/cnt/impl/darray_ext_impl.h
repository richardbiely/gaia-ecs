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
		//! \cond INTERNAL
		namespace darr_ext_detail {
			using difference_type = uint32_t;
			using size_type = uint32_t;
		} // namespace darr_ext_detail
		//! \endcond

		//! Array of elements of type \tparam T allocated on heap or stack. Stack capacity is \tparam N elements.
		//! If the number of elements is bellow \tparam N the stack storage is used.
		//! If the number of elements is above \tparam N the heap storage is used.
		//! Interface compatiblity with std::vector and std::array where it matters.
		template <typename T, darr_ext_detail::size_type N, typename Allocator = mem::DefaultAllocatorAdaptor>
		class darr_ext {
		public:
			static_assert(N > 0);

			//! Element type stored by the container.
			using value_type = T;
			//! Mutable element reference type.
			using reference = T&;
			//! Read-only element reference type.
			using const_reference = const T&;
			//! Mutable element pointer type.
			using pointer = T*;
			//! Read-only element pointer type.
			using const_pointer = const T*;
			//! Data-layout access policy used by the container.
			using view_policy = mem::data_view_policy_aos<T>;
			//! Type used for iterator differences.
			using difference_type = darr_ext_detail::difference_type;
			//! Unsigned type used for sizes and indices.
			using size_type = darr_ext_detail::size_type;

			//! Mutable random-access iterator type.
			using iterator = pointer;
			//! Read-only random-access iterator type.
			using const_iterator = const_pointer;
			//! Iterator category exposed by the container.
			using iterator_category = core::random_access_iterator_tag;

			//! Size of one element in bytes.
			static constexpr size_t value_size = sizeof(T);
			//! Fixed capacity of the container.
			static constexpr size_type extent = N;
			//! Number of bytes reserved by the inline storage.
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
					GAIA_MEM_SANI_ADD_BLOCK(value_size, m_pDataHeap, m_cap, cnt);
					mem::move_elements<T, false>(m_pDataHeap, m_data, cnt, 0, m_cap, cap);
				} else {
					// Move items from the old heap array to the new one. Delete the old
					auto* pDataOld = m_pDataHeap;
					m_pDataHeap = view_policy::template alloc<Allocator>(m_cap);
					GAIA_MEM_SANI_ADD_BLOCK(value_size, m_pDataHeap, m_cap, cnt);
					mem::move_elements<T, false>(m_pDataHeap, pDataOld, cnt, 0, m_cap, cap);
					view_policy::template free<Allocator>(pDataOld, cap, cnt);
				}

				m_pData = m_pDataHeap;
			}

		public:
			darr_ext() noexcept = default;
			//! Constructs a value-initialized container.
			darr_ext(core::zero_t) noexcept {}

			//! Constructs a container with copies of a value.
			//! \param count Number of elements.
			//! \param value Value assigned to each new element.
			darr_ext(size_type count, const_reference value) {
				resize(count, value);
			}

			//! Constructs a container with the requested number of value-initialized elements.
			//! \param count Number of elements.
			darr_ext(size_type count) {
				resize(count);
			}

			//! Constructs a container from an iterator range.
			//! \tparam InputIt Input iterator type.
			//! \param first Iterator to the first source element.
			//! \param last Iterator one past the last source element.
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

			//! Constructs a container from an initializer list.
			//! \param il Initializer list supplying the elements.
			darr_ext(std::initializer_list<T> il): darr_ext(il.begin(), il.end()) {}

			//! Copy-constructs a container.
			//! \param other Container to copy or move from.
			darr_ext(const darr_ext& other): darr_ext(other.begin(), other.end()) {}

			//! Move-constructs a container.
			//! \param other Container to copy or move from.
			darr_ext(darr_ext&& other) noexcept {
				GAIA_ASSERT(core::addressof(other) != this);

				// Moving from stack-allocated source
				if (other.m_pDataHeap == nullptr) {
					GAIA_MEM_SANI_ADD_BLOCK(value_size, m_data, extent, other.size());
					mem::move_elements<T, false>(m_data, other.m_data, other.size(), 0, extent, other.extent);
					GAIA_MEM_SANI_DEL_BLOCK(value_size, other.m_data, extent, other.size());
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

			//! Replaces the elements from an initializer list.
			//! \param il Initializer list supplying the elements.
			//! \return Reference to this container.
			darr_ext& operator=(std::initializer_list<T> il) {
				*this = darr_ext(il.begin(), il.end());
				return *this;
			}

			//! Copy-assigns the container.
			//! \param other Container to copy or move from.
			//! \return Reference to this container.
			darr_ext& operator=(const darr_ext& other) {
				GAIA_ASSERT(core::addressof(other) != this);

				resize(other.size());
				mem::copy_elements<T, false>(
						m_pData, (const uint8_t*)other.m_pData, other.size(), 0, capacity(), other.capacity());

				return *this;
			}

			//! Move-assigns the container.
			//! \param other Container to copy or move from.
			//! \return Reference to this container.
			darr_ext& operator=(darr_ext&& other) noexcept {
				GAIA_ASSERT(core::addressof(other) != this);

				// Release previously allocated memory or its stack-container annotation.
				if (m_pDataHeap != nullptr)
					view_policy::template free<Allocator>(m_pDataHeap, m_cap, m_cnt);
				else
					GAIA_MEM_SANI_DEL_BLOCK(value_size, m_data, extent, m_cnt);

				// Moving from stack-allocated source
				if (other.m_pDataHeap == nullptr) {
					GAIA_MEM_SANI_ADD_BLOCK(value_size, m_data, extent, other.size());
					mem::move_elements<T, false>(m_data, other.m_data, other.size(), 0, extent, other.extent);
					GAIA_MEM_SANI_DEL_BLOCK(value_size, other.m_data, extent, other.size());
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

				return *this;
			}

			~darr_ext() {
				if (m_pDataHeap != nullptr)
					view_policy::template free<Allocator>(m_pDataHeap, m_cap, m_cnt);
				else
					GAIA_MEM_SANI_DEL_BLOCK(value_size, m_data, extent, m_cnt);
			}

			GAIA_CLANG_WARNING_PUSH()
			// Memory is aligned so we can silence this warning
			GAIA_CLANG_WARNING_DISABLE("-Wcast-align")

			//! Returns a pointer to the element storage.
			//! \return Pointer to the first element storage location.
			GAIA_NODISCARD pointer data() noexcept {
				return reinterpret_cast<pointer>(m_pData);
			}

			//! Returns a pointer to the element storage.
			//! \return Pointer to the first element storage location.
			GAIA_NODISCARD const_pointer data() const noexcept {
				return reinterpret_cast<const_pointer>(m_pData);
			}

			//! Accesses an element without bounds checking in optimized builds.
			//! \param pos Zero-based element index.
			//! \return Reference to the selected element.
			GAIA_NODISCARD decltype(auto) operator[](size_type pos) noexcept {
				GAIA_ASSERT(pos < size());
				return view_policy::set({(typename view_policy::TargetCastType)m_pData, size()}, pos);
			}

			//! Accesses an element without bounds checking in optimized builds.
			//! \param pos Zero-based element index.
			//! \return Reference to the selected element.
			GAIA_NODISCARD decltype(auto) operator[](size_type pos) const noexcept {
				GAIA_ASSERT(pos < size());
				return view_policy::get({(typename view_policy::TargetCastType)m_pData, size()}, pos);
			}

			GAIA_CLANG_WARNING_POP()

			//! Ensures storage for at least the requested number of elements.
			//! \param cap Requested element capacity.
			void reserve(size_type cap) {
				if (cap <= m_cap)
					return;

				auto* pDataOld = m_pDataHeap;
				m_pDataHeap = view_policy::template alloc<Allocator>(cap);
				GAIA_MEM_SANI_ADD_BLOCK(value_size, m_pDataHeap, cap, m_cnt);
				if (pDataOld != nullptr) {
					mem::move_elements<T, false>(m_pDataHeap, pDataOld, m_cnt, 0, cap, m_cap);
					view_policy::template free<Allocator>(pDataOld, m_cap, m_cnt);
				} else {
					mem::move_elements<T, false>(m_pDataHeap, m_data, m_cnt, 0, cap, m_cap);
					GAIA_MEM_SANI_DEL_BLOCK(value_size, m_data, m_cap, m_cnt);
				}

				m_cap = cap;
				m_pData = m_pDataHeap;
			}

			//! Changes the number of elements.
			//! \param count Number of elements.
			void resize(size_type count) {
				if (count == m_cnt)
					return;

				// Resizing to a smaller size
				if (count < m_cnt) {
					// Destroy elements at the end
					core::call_dtor_n(&data()[count], m_cnt - count);
					GAIA_MEM_SANI_POP_N(value_size, data(), m_cap, m_cnt, m_cnt - count);

					m_cnt = count;
					return;
				}

				// Resizing to a bigger size but still within allocated capacity
				if (count <= m_cap) {
					// Construct new elements
					GAIA_MEM_SANI_PUSH_N(value_size, data(), m_cap, m_cnt, count - m_cnt);
					core::call_ctor_n(&data()[m_cnt], count - m_cnt);

					m_cnt = count;
					return;
				}

				auto* pDataOld = m_pDataHeap;
				m_pDataHeap = view_policy::template alloc<Allocator>(count);
				GAIA_MEM_SANI_ADD_BLOCK(value_size, m_pDataHeap, count, count);
				auto* pDataNew = reinterpret_cast<pointer>(m_pDataHeap);
				if (pDataOld != nullptr) {
					mem::move_elements<T, false>(m_pDataHeap, pDataOld, m_cnt, 0, count, m_cap);
					core::call_ctor_n(&pDataNew[m_cnt], count - m_cnt);
					view_policy::template free<Allocator>(pDataOld, m_cap, m_cnt);
				} else {
					mem::move_elements<T, false>(m_pDataHeap, m_data, m_cnt, 0, count, m_cap);
					GAIA_MEM_SANI_DEL_BLOCK(value_size, m_data, m_cap, m_cnt);
				}

				m_cap = count;
				m_cnt = count;
				m_pData = m_pDataHeap;
			}

			//! Changes the size and initializes new elements from a value.
			//! \param count Number of elements.
			//! \param value Value assigned to each new element.
			void resize(size_type count, const_reference value) {
				const auto oldCount = m_cnt;
				resize(count);

				if constexpr (std::is_copy_constructible_v<value_type>) {
					const value_type valueCopy = value;
					for (size_type i = oldCount; i < m_cnt; ++i)
						operator[](i) = valueCopy;
				} else {
					for (size_type i = oldCount; i < m_cnt; ++i)
						operator[](i) = value;
				}
			}

			//! Appends an element.
			//! \param arg Element value to append.
			void push_back(const T& arg) {
				try_grow();

				GAIA_MEM_SANI_PUSH(value_size, data(), m_cap, m_cnt);
				auto* ptr = &data()[m_cnt++];
				core::call_ctor(ptr, arg);
			}

			//! Appends an element.
			//! \param arg Element value to append.
			void push_back(T&& arg) {
				try_grow();

				GAIA_MEM_SANI_PUSH(value_size, data(), m_cap, m_cnt);
				auto* ptr = &data()[m_cnt++];
				core::call_ctor(ptr, GAIA_MOV(arg));
			}

			//! Constructs and appends an element.
			//! \tparam Args Types of the forwarded constructor arguments.
			//! \param args Arguments forwarded to the element constructor.
			//! \return Reference to the appended element.
			template <typename... Args>
			decltype(auto) emplace_back(Args&&... args) {
				try_grow();

				GAIA_MEM_SANI_PUSH(value_size, data(), m_cap, m_cnt);
				auto* ptr = &data()[m_cnt++];
				core::call_ctor(ptr, GAIA_FWD(args)...);
				return (reference)*ptr;
			}

			//! Removes the last element.
			void pop_back() noexcept {
				GAIA_ASSERT(!empty());

				auto* ptr = &data()[m_cnt - 1];
				core::call_dtor(ptr);
				GAIA_MEM_SANI_POP(value_size, data(), m_cap, m_cnt);

				--m_cnt;
			}

			//! Insert the element to the position given by iterator \a pos
			//! \return Iterator to the inserted element.
			//! \param pos Position in the container
			//! \param arg Data to insert
			iterator insert(iterator pos, const T& arg) {
				GAIA_ASSERT(pos >= data());
				GAIA_ASSERT(empty() || (pos < iterator(data() + size())));

				const auto idxSrc = (size_type)core::distance(begin(), pos);
				try_grow();
				const auto idxDst = (size_type)core::distance(begin(), end()) + 1;

				GAIA_MEM_SANI_PUSH(value_size, data(), m_cap, m_cnt);
				mem::shift_elements_right<T, false>(m_pData, idxDst, idxSrc, m_cap);
				auto* ptr = &data()[idxSrc];
				core::call_ctor(ptr, arg);

				++m_cnt;

				return iterator(ptr);
			}

			//! Insert the element to the position given by iterator \a pos
			//! \return Iterator to the inserted element.
			//! \param pos Positing in the container
			//! \param arg Data to insert
			iterator insert(iterator pos, T&& arg) {
				GAIA_ASSERT(pos >= data());
				GAIA_ASSERT(empty() || (pos < iterator(data() + size())));

				const auto idxSrc = (size_type)core::distance(begin(), pos);
				try_grow();
				const auto idxDst = (size_type)core::distance(begin(), end());

				GAIA_MEM_SANI_PUSH(value_size, data(), m_cap, m_cnt);
				mem::shift_elements_right<T, false>(m_pData, idxDst, idxSrc, m_cap);
				auto* ptr = &data()[idxSrc];
				core::call_ctor(ptr, GAIA_MOV(arg));

				++m_cnt;

				return iterator(ptr);
			}

			//! Removes the element at pos
			//! \return Iterator to the element following the removed element or range.
			//! \param pos Iterator to the element to remove
			iterator erase(iterator pos) noexcept {
				GAIA_ASSERT(pos >= data());
				GAIA_ASSERT(empty() || (pos < iterator(data() + size())));

				if (empty())
					return end();

				const auto idxSrc = (size_type)core::distance(begin(), pos);
				const auto idxDst = (size_type)core::distance(begin(), end()) - 1;

				mem::shift_elements_left<T, false>(m_pData, idxDst, idxSrc, m_cap);
				// Destroy if it's the last element
				auto* ptr = &data()[m_cnt - 1];
				core::call_dtor(ptr);
				GAIA_MEM_SANI_POP(value_size, data(), m_cap, m_cnt);

				--m_cnt;

				return iterator(&data()[idxSrc]);
			}

			//! Removes the elements in the range [first, last)
			//! \return Iterator to the element following the removed element or range.
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

				mem::shift_elements_left_fast<T, false>(m_pData, idxDst, idxSrc, cnt, m_cap);
				// Destroy if it's the last element
				core::call_dtor_n(&data()[m_cnt - cnt], cnt);
				GAIA_MEM_SANI_POP_N(value_size, data(), m_cap, m_cnt, cnt);

				m_cnt -= cnt;

				return iterator(&data()[idxSrc]);
			}

			//! Removes all elements.
			void clear() noexcept {
				resize(0);
			}

			//! Reduces allocated storage to match the current size when possible.
			void shrink_to_fit() {
				const auto cap = capacity();
				const auto cnt = size();

				if (cap == cnt)
					return;

				if (m_pDataHeap != nullptr) {
					auto* pDataOld = m_pDataHeap;

					if (cnt < extent) {
						mem::move_elements<T, false>(m_data, pDataOld, cnt, 0);
						m_pData = m_data;
						m_cap = extent;
					} else {
						m_pDataHeap = view_policy::template alloc<Allocator>(m_cap = cnt);
						GAIA_MEM_SANI_ADD_BLOCK(value_size, m_pDataHeap, m_cap, m_cnt);
						mem::move_elements<T, false>(m_pDataHeap, pDataOld, cnt, 0);
						m_pData = m_pDataHeap;
					}

					GAIA_MEM_SANI_DEL_BLOCK(value_size, pDataOld, cap, cnt);
					view_policy::template free<Allocator>(pDataOld);
				} else
					resize(cnt);
			}

			//! Removes all elements that fail the predicate.
			//! \tparam Func Predicate callable type.
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
							auto* ptr = (uint8_t*)data();
							mem::move_element<T, false>(ptr, ptr, idxDst, idxSrc, m_cap, m_cap);
							auto* ptr2 = &data()[idxSrc];
							core::call_dtor(ptr2);
						}
						++idxDst;
					} else {
						auto* ptr = &data()[idxSrc];
						core::call_dtor(ptr);
						++erased;
					}

					++idxSrc;
				}

				GAIA_MEM_SANI_POP_N(value_size, data(), m_cap, m_cnt, erased);

				m_cnt -= erased;
				return idxDst;
			}

			//! Returns the number of elements.
			//! \return Current element count.
			GAIA_NODISCARD size_type size() const noexcept {
				return m_cnt;
			}

			//! Checks whether the container has no elements.
			//! \return True if the container contains no elements.
			GAIA_NODISCARD bool empty() const noexcept {
				return size() == 0;
			}

			//! Returns the number of elements that fit without reallocation.
			//! \return Current element capacity.
			GAIA_NODISCARD size_type capacity() const noexcept {
				return m_cap;
			}

			//! Returns the maximum number of elements supported by this container.
			//! \return Maximum supported element count.
			GAIA_NODISCARD size_type max_size() const noexcept {
				return N;
			}

			//! Accesses the first element.
			//! \return Reference to the first element.
			GAIA_NODISCARD decltype(auto) front() noexcept {
				GAIA_ASSERT(!empty());
				return (reference)*begin();
			}

			//! Accesses the first element.
			//! \return Reference to the first element.
			GAIA_NODISCARD decltype(auto) front() const noexcept {
				GAIA_ASSERT(!empty());
				return (const_reference)*begin();
			}

			//! Accesses the last element.
			//! \return Reference to the last element.
			GAIA_NODISCARD decltype(auto) back() noexcept {
				GAIA_ASSERT(!empty());
				return (reference) operator[](m_cnt - 1);
			}

			//! Accesses the last element.
			//! \return Reference to the last element.
			GAIA_NODISCARD decltype(auto) back() const noexcept {
				GAIA_ASSERT(!empty());
				return (const_reference) operator[](m_cnt - 1);
			}

			//! Returns an iterator to the first element.
			//! \return Iterator to the first element.
			GAIA_NODISCARD auto begin() noexcept {
				return iterator(data());
			}

			//! Returns an iterator to the first element.
			//! \return Iterator to the first element.
			GAIA_NODISCARD auto begin() const noexcept {
				return const_iterator(data());
			}

			//! Returns a read-only iterator to the first element.
			//! \return Iterator to the first element.
			GAIA_NODISCARD auto cbegin() const noexcept {
				return const_iterator(data());
			}

			//! Returns a reverse traversal iterator to the last element.
			//! \return Iterator to the last element.
			GAIA_NODISCARD auto rbegin() noexcept {
				return iterator((pointer)&back());
			}

			//! Returns a reverse traversal iterator to the last element.
			//! \return Iterator to the last element.
			GAIA_NODISCARD auto rbegin() const noexcept {
				return const_iterator((const_pointer)&back());
			}

			//! Returns a read-only reverse traversal iterator to the last element.
			//! \return Iterator to the last element.
			GAIA_NODISCARD auto crbegin() const noexcept {
				return const_iterator((const_pointer)&back());
			}

			//! Returns an iterator one past the last element.
			//! \return Iterator one past the last element.
			GAIA_NODISCARD auto end() noexcept {
				return iterator(data() + size());
			}

			//! Returns an iterator one past the last element.
			//! \return Iterator one past the last element.
			GAIA_NODISCARD auto end() const noexcept {
				return const_iterator(data() + size());
			}

			//! Returns a read-only iterator one past the last element.
			//! \return Iterator one past the last element.
			GAIA_NODISCARD auto cend() const noexcept {
				return const_iterator(data() + size());
			}

			//! Returns the reverse traversal sentinel preceding the first element.
			//! \return Reverse traversal sentinel preceding the first element.
			GAIA_NODISCARD auto rend() noexcept {
				return iterator(data() - 1);
			}

			//! Returns the reverse traversal sentinel preceding the first element.
			//! \return Reverse traversal sentinel preceding the first element.
			GAIA_NODISCARD auto rend() const noexcept {
				return const_iterator(data() - 1);
			}

			//! Returns the read-only reverse traversal sentinel preceding the first element.
			//! \return Reverse traversal sentinel preceding the first element.
			GAIA_NODISCARD auto crend() const noexcept {
				return const_iterator(data() - 1);
			}

			//! Compares two containers element by element.
			//! \param other Container to copy or move from.
			//! \return True if both containers contain equal elements.
			GAIA_NODISCARD bool operator==(const darr_ext& other) const noexcept {
				if (m_cnt != other.m_cnt)
					return false;
				const size_type n = size();
				for (size_type i = 0; i < n; ++i)
					if (!(operator[](i) == other[i]))
						return false;
				return true;
			}

			//! Checks whether two containers differ.
			//! \param other Container to copy or move from.
			//! \return True if the containers differ.
			GAIA_NODISCARD constexpr bool operator!=(const darr_ext& other) const noexcept {
				return !operator==(other);
			}
		};

		//! \cond INTERNAL
		namespace detail {
			template <typename T, uint32_t N, uint32_t... I>
			darr_ext<std::remove_cv_t<T>, N> to_sarray_impl(T (&a)[N], std::index_sequence<I...> /*no_name*/) {
				return {{a[I]...}};
			}
		} // namespace detail
		//! \endcond

		//! Converts a built-in array to a Gaia-ECS array.
		//! \tparam T Element type.
		//! \tparam N Number of elements in the source array.
		//! \param a Built-in array to convert.
		//! \return Converted Gaia-ECS container.
		template <typename T, uint32_t N>
		darr_ext<std::remove_cv_t<T>, N> to_sarray(T (&a)[N]) {
			return detail::to_sarray_impl(a, std::make_index_sequence<N>{});
		}

	} // namespace cnt

} // namespace gaia
