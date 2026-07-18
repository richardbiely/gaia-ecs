#pragma once
#include "gaia/config/config.h"

#include <cstddef>
#include <initializer_list>
#include <new>
#include <tuple>
#include <type_traits>
#include <utility>

#include "gaia/core/iterator.h"
#include "gaia/core/utility.h"
#include "gaia/mem/data_layout_policy.h"
#include "gaia/mem/mem_utils.h"
#include "gaia/mem/raw_data_holder.h"

namespace gaia {
	namespace cnt {
		//! \cond INTERNAL
		namespace sarr_ext_soa_detail {
			using difference_type = uint32_t;
			using size_type = uint32_t;
		} // namespace sarr_ext_soa_detail
		//! \endcond

		//! \cond INTERNAL
		template <typename T>
		struct sarr_ext_soa_iterator {
			//! Element type stored by the container.
			using value_type = T;
			// using pointer = T*; not supported
			// using reference = T&; not supported
			using difference_type = sarr_ext_soa_detail::size_type;
			//! Unsigned type used for sizes and indices.
			using size_type = sarr_ext_soa_detail::size_type;

			using iterator = sarr_ext_soa_iterator;
			//! Iterator category exposed by the container.
			using iterator_category = core::random_access_iterator_tag;

		private:
			uint8_t* m_ptr;
			uint32_t m_cnt;
			uint32_t m_idx;

		public:
			sarr_ext_soa_iterator(uint8_t* ptr, uint32_t cnt, uint32_t idx): m_ptr(ptr), m_cnt(cnt), m_idx(idx) {}

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
		struct const_sarr_ext_soa_iterator {
			using value_type = T;
			// using pointer = T*; not supported
			// using reference = T&; not supported
			using difference_type = sarr_ext_soa_detail::size_type;
			using size_type = sarr_ext_soa_detail::size_type;

			using iterator = const_sarr_ext_soa_iterator;
			using iterator_category = core::random_access_iterator_tag;

		private:
			const uint8_t* m_ptr;
			uint32_t m_cnt;
			uint32_t m_idx;

		public:
			const_sarr_ext_soa_iterator(const uint8_t* ptr, uint32_t cnt, uint32_t idx): m_ptr(ptr), m_cnt(cnt), m_idx(idx) {}

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
		//! \endcond

		//! Array of elements of type \tparam T with fixed capacity \tparam N and variable size allocated on stack.
		//! Interface compatiblity with std::array where it matters.
		template <typename T, sarr_ext_soa_detail::size_type N>
		class sarr_ext_soa {
			static_assert(mem::is_soa_layout_v<T>, "sarr_ext_soa can be used only with soa types");

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
			using view_policy = mem::data_view_policy_soa<T::gaia_Data_Layout, T>;
			//! Type used for iterator differences.
			using difference_type = sarr_ext_soa_detail::difference_type;
			//! Unsigned type used for sizes and indices.
			using size_type = sarr_ext_soa_detail::size_type;

			//! Mutable random-access iterator type.
			using iterator = sarr_ext_soa_iterator<T>;
			//! Read-only random-access iterator type.
			using const_iterator = const_sarr_ext_soa_iterator<T>;
			//! Iterator category exposed by the container.
			using iterator_category = core::random_access_iterator_tag;

			//! Fixed capacity of the container.
			static constexpr size_type extent = N;
			//! Number of bytes reserved by the inline storage.
			static constexpr uint32_t allocated_bytes = view_policy::get_min_byte_size(0, N);

		private:
			mem::raw_data_holder<T, allocated_bytes> m_data;
			size_type m_cnt = size_type(0);

		public:
			constexpr sarr_ext_soa() noexcept = default;
			//! Constructs a value-initialized container.
			constexpr sarr_ext_soa(core::zero_t) noexcept {}

			~sarr_ext_soa() = default;

			//! Constructs a container with copies of a value.
			//! \param count Number of elements.
			//! \param value Value assigned to each new element.
			constexpr sarr_ext_soa(size_type count, const_reference value) noexcept {
				resize(count, value);
			}

			//! Constructs a container with the requested number of value-initialized elements.
			//! \param count Number of elements.
			constexpr sarr_ext_soa(size_type count) noexcept {
				resize(count);
			}

			//! Constructs a container from an iterator range.
			//! \tparam InputIt Input iterator type.
			//! \param first Iterator to the first source element.
			//! \param last Iterator one past the last source element.
			template <typename InputIt>
			constexpr sarr_ext_soa(InputIt first, InputIt last) noexcept {
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
			constexpr sarr_ext_soa(std::initializer_list<T> il): sarr_ext_soa(il.begin(), il.end()) {}

			//! Copy-constructs a container.
			//! \param other Container to copy or move from.
			constexpr sarr_ext_soa(const sarr_ext_soa& other): sarr_ext_soa(other.begin(), other.end()) {}

			//! Move-constructs a container.
			//! \param other Container to copy or move from.
			constexpr sarr_ext_soa(sarr_ext_soa&& other) noexcept: m_cnt(other.m_cnt) {
				GAIA_ASSERT(core::addressof(other) != this);

				mem::move_elements<T, true>(m_data, other.m_data, other.size(), 0, extent, other.extent);

				other.m_cnt = size_type(0);
			}

			//! Replaces the elements from an initializer list.
			//! \param il Initializer list supplying the elements.
			//! \return Reference to this container.
			sarr_ext_soa& operator=(std::initializer_list<T> il) {
				*this = sarr_ext_soa(il.begin(), il.end());
				return *this;
			}

			//! Copy-assigns the container.
			//! \param other Container to copy or move from.
			//! \return Reference to this container.
			constexpr sarr_ext_soa& operator=(const sarr_ext_soa& other) {
				GAIA_ASSERT(core::addressof(other) != this);

				resize(other.size());
				mem::copy_elements<T, true>(
						GAIA_ACC((uint8_t*)&m_data[0]), GAIA_ACC((const uint8_t*)&other.m_data[0]), other.size(), 0, extent,
						other.extent);

				return *this;
			}

			//! Move-assigns the container.
			//! \param other Container to copy or move from.
			//! \return Reference to this container.
			constexpr sarr_ext_soa& operator=(sarr_ext_soa&& other) noexcept {
				GAIA_ASSERT(core::addressof(other) != this);

				resize(other.m_cnt);
				mem::move_elements<T, true>(
						GAIA_ACC((uint8_t*)&m_data[0]), GAIA_ACC((uint8_t*)&other.m_data[0]), other.size(), 0, extent,
						other.extent);

				other.m_cnt = size_type(0);

				return *this;
			}

			GAIA_CLANG_WARNING_PUSH()
			// Memory is aligned so we can silence this warning
			GAIA_CLANG_WARNING_DISABLE("-Wcast-align")

			//! Returns a pointer to the element storage.
			//! \return Pointer to the first element storage location.
			GAIA_NODISCARD constexpr pointer data() noexcept {
				return GAIA_ACC((pointer)&m_data[0]);
			}

			//! Returns a pointer to the element storage.
			//! \return Pointer to the first element storage location.
			GAIA_NODISCARD constexpr const_pointer data() const noexcept {
				return GAIA_ACC((const_pointer)&m_data[0]);
			}

			//! Accesses an element without bounds checking in optimized builds.
			//! \param pos Zero-based element index.
			//! \return Reference to the selected element.
			GAIA_NODISCARD constexpr decltype(auto) operator[](size_type pos) noexcept {
				GAIA_ASSERT(pos < size());
				return view_policy::set({GAIA_ACC((typename view_policy::TargetCastType) & m_data[0]), extent}, pos);
			}

			//! Accesses an element without bounds checking in optimized builds.
			//! \param pos Zero-based element index.
			//! \return Reference to the selected element.
			GAIA_NODISCARD constexpr decltype(auto) operator[](size_type pos) const noexcept {
				GAIA_ASSERT(pos < size());
				return view_policy::get({GAIA_ACC((typename view_policy::TargetCastType) & m_data[0]), extent}, pos);
			}

			GAIA_CLANG_WARNING_POP()

			//! Appends an element.
			//! \param arg Element value to append.
			constexpr void push_back(const T& arg) noexcept {
				GAIA_ASSERT(size() < N);

				operator[](m_cnt++) = arg;
			}

			//! Appends an element.
			//! \param arg Element value to append.
			constexpr void push_back(T&& arg) noexcept {
				GAIA_ASSERT(size() < N);

				operator[](m_cnt++) = GAIA_MOV(arg);
			}

			//! Constructs and appends an element.
			//! \tparam Args Types of the forwarded constructor arguments.
			//! \param args Arguments forwarded to the element constructor.
			//! \return No value. The deduced return type is void.
			template <typename... Args>
			constexpr decltype(auto) emplace_back(Args&&... args) noexcept {
				GAIA_ASSERT(size() < N);

				operator[](m_cnt++) = T(GAIA_FWD(args)...);
			}

			//! Removes the last element.
			constexpr void pop_back() noexcept {
				GAIA_ASSERT(!empty());

				--m_cnt;
			}

			//! Insert the element to the position given by iterator \a pos
			//! \return Iterator to the inserted element.
			//! \param pos Position in the container
			//! \param arg Data to insert
			iterator insert(iterator pos, const T& arg) noexcept {
				GAIA_ASSERT(size() < N);
				GAIA_ASSERT(pos >= data());
				GAIA_ASSERT(empty() || (pos < iterator(data() + size())));

				const auto idxSrc = (size_type)core::distance(begin(), pos);
				const auto idxDst = (size_type)core::distance(begin(), end());

				mem::shift_elements_right<T, true>(m_data, idxDst, idxSrc, extent);

				operator[](idxSrc) = arg;

				++m_cnt;

				return iterator(GAIA_ACC(&m_data[0]), extent, idxSrc);
			}

			//! Insert the element to the position given by iterator \a pos
			//! \return Iterator to the inserted element.
			//! \param pos Position in the container
			//! \param arg Data to insert
			iterator insert(iterator pos, T&& arg) noexcept {
				GAIA_ASSERT(size() < N);
				GAIA_ASSERT(pos >= data());
				GAIA_ASSERT(empty() || (pos < iterator(data() + size())));

				const auto idxSrc = (size_type)core::distance(begin(), pos);
				const auto idxDst = (size_type)core::distance(begin(), end());

				mem::shift_elements_right<T, true>(m_data, idxDst, idxSrc, extent);

				operator[](idxSrc) = GAIA_MOV(arg);

				++m_cnt;

				return iterator(GAIA_ACC(&m_data[0]), extent, idxSrc);
			}

			//! Removes the element at pos
			//! \return Iterator to the element following the removed element or range.
			//! \param pos Iterator to the element to remove
			constexpr iterator erase(iterator pos) noexcept {
				GAIA_ASSERT(pos >= data());
				GAIA_ASSERT(empty() || (pos < iterator(data() + size())));

				if (empty())
					return end();

				const auto idxSrc = (size_type)core::distance(begin(), pos);
				const auto idxDst = (size_type)core::distance(begin(), end()) - 1;

				mem::shift_elements_left<T, true>(m_data, idxDst, idxSrc, extent);

				--m_cnt;

				return iterator(GAIA_ACC(&m_data[0]), extent, idxSrc);
			}

			//! Removes the elements in the range [first, last)
			//! \return Iterator to the element following the removed element or range.
			//! \param first Iterator to the element to remove
			//! \param last Iterator to the one beyond the last element to remove
			iterator erase(iterator first, iterator last) noexcept {
				GAIA_ASSERT(first >= data())
				GAIA_ASSERT(empty() || (first < iterator(data() + size())));
				GAIA_ASSERT(last > first);
				GAIA_ASSERT(last <= (data() + size()));

				if (empty())
					return end();

				const auto idxSrc = (size_type)core::distance(begin(), first);
				const auto idxDst = size();
				const auto cnt = (size_type)(last - first);

				mem::shift_elements_left_fast<T, true>(m_data, idxDst, idxSrc, cnt, extent);

				m_cnt -= cnt;

				return iterator(GAIA_ACC(&m_data[0]), extent, idxSrc);
			}

			//! \return Iterator to the element following the removed element or range.
			//! Removes the element at index \param pos
			iterator erase_at(size_type pos) noexcept {
				GAIA_ASSERT(pos < size());

				const auto idxSrc = pos;
				const auto idxDst = (size_type)core::distance(begin(), end()) - 1;

				mem::shift_elements_left<T, true>(m_data, idxDst, idxSrc, extent);

				--m_cnt;

				return iterator(GAIA_ACC(&m_data[0]), extent, idxSrc);
			}

			//! Removes all elements.
			constexpr void clear() noexcept {
				resize(0);
			}

			//! Changes the number of elements.
			//! \param count Number of elements.
			constexpr void resize(size_type count) noexcept {
				GAIA_ASSERT(count <= max_size());

				m_cnt = count;
			}

			//! Changes the size and initializes new elements from a value.
			//! \param count Number of elements.
			//! \param value Value assigned to each new element.
			constexpr void resize(size_type count, const_reference value) noexcept {
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
							mem::move_element<T, true>(ptr, ptr, idxDst, idxSrc, max_size(), max_size());
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

				m_cnt -= erased;
				return idxDst;
			}

			//! Returns the number of elements.
			//! \return Current element count.
			GAIA_NODISCARD constexpr size_type size() const noexcept {
				return m_cnt;
			}

			//! Checks whether the container has no elements.
			//! \return True if the container contains no elements.
			GAIA_NODISCARD constexpr bool empty() const noexcept {
				return size() == 0;
			}

			//! Returns the number of elements that fit without reallocation.
			//! \return Current element capacity.
			GAIA_NODISCARD constexpr size_type capacity() const noexcept {
				return N;
			}

			//! Returns the maximum number of elements supported by this container.
			//! \return Maximum supported element count.
			GAIA_NODISCARD constexpr size_type max_size() const noexcept {
				return N;
			}

			//! Accesses the first element.
			//! \return Reference to the first element.
			GAIA_NODISCARD constexpr decltype(auto) front() noexcept {
				GAIA_ASSERT(!empty());
				return *begin();
			}

			//! Accesses the first element.
			//! \return Reference to the first element.
			GAIA_NODISCARD constexpr decltype(auto) front() const noexcept {
				GAIA_ASSERT(!empty());
				return *begin();
			}

			//! Accesses the last element.
			//! \return Reference to the last element.
			GAIA_NODISCARD constexpr decltype(auto) back() noexcept {
				GAIA_ASSERT(!empty());
				return (operator[])(m_cnt - 1);
			}

			//! Accesses the last element.
			//! \return Reference to the last element.
			GAIA_NODISCARD constexpr decltype(auto) back() const noexcept {
				GAIA_ASSERT(!empty());
				return operator[](m_cnt - 1);
			}

			//! Returns an iterator to the first element.
			//! \return Iterator to the first element.
			GAIA_NODISCARD constexpr auto begin() noexcept {
				return iterator(GAIA_ACC(&m_data[0]), extent, 0);
			}

			//! Returns an iterator to the first element.
			//! \return Iterator to the first element.
			GAIA_NODISCARD constexpr auto begin() const noexcept {
				return const_iterator(GAIA_ACC(&m_data[0]), extent, 0);
			}

			//! Returns a read-only iterator to the first element.
			//! \return Iterator to the first element.
			GAIA_NODISCARD constexpr auto cbegin() const noexcept {
				return const_iterator(GAIA_ACC(&m_data[0]), extent, 0);
			}

			//! Returns a reverse traversal iterator to the last element.
			//! \return Iterator to the last element.
			GAIA_NODISCARD constexpr auto rbegin() noexcept {
				return iterator(GAIA_ACC(&m_data[0]), extent, size() - 1);
			}

			//! Returns a reverse traversal iterator to the last element.
			//! \return Iterator to the last element.
			GAIA_NODISCARD constexpr auto rbegin() const noexcept {
				return const_iterator(GAIA_ACC(&m_data[0]), extent, size() - 1);
			}

			//! Returns a read-only reverse traversal iterator to the last element.
			//! \return Iterator to the last element.
			GAIA_NODISCARD constexpr auto crbegin() const noexcept {
				return const_iterator(GAIA_ACC(&m_data[0]), extent, size() - 1);
			}

			//! Returns an iterator one past the last element.
			//! \return Iterator one past the last element.
			GAIA_NODISCARD constexpr auto end() noexcept {
				return iterator(GAIA_ACC(&m_data[0]), extent, size());
			}

			//! Returns an iterator one past the last element.
			//! \return Iterator one past the last element.
			GAIA_NODISCARD constexpr auto end() const noexcept {
				return const_iterator(GAIA_ACC(&m_data[0]), extent, size());
			}

			//! Returns a read-only iterator one past the last element.
			//! \return Iterator one past the last element.
			GAIA_NODISCARD constexpr auto cend() const noexcept {
				return const_iterator(GAIA_ACC(&m_data[0]), extent, size());
			}

			//! Returns the reverse traversal sentinel preceding the first element.
			//! \return Reverse traversal sentinel preceding the first element.
			GAIA_NODISCARD constexpr auto rend() noexcept {
				return iterator(GAIA_ACC(&m_data[0]), extent, -1);
			}

			//! Returns the reverse traversal sentinel preceding the first element.
			//! \return Reverse traversal sentinel preceding the first element.
			GAIA_NODISCARD constexpr auto rend() const noexcept {
				return const_iterator(GAIA_ACC(&m_data[0]), extent, -1);
			}

			//! Returns the read-only reverse traversal sentinel preceding the first element.
			//! \return Reverse traversal sentinel preceding the first element.
			GAIA_NODISCARD constexpr auto crend() const noexcept {
				return const_iterator(GAIA_ACC(&m_data[0]), extent, -1);
			}

			//! Compares two containers element by element.
			//! \param other Container to copy or move from.
			//! \return True if both containers contain equal elements.
			GAIA_NODISCARD constexpr bool operator==(const sarr_ext_soa& other) const noexcept {
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
			GAIA_NODISCARD constexpr bool operator!=(const sarr_ext_soa& other) const noexcept {
				return !operator==(other);
			}

			//! Returns a mutable view of one structure-of-arrays member.
			//! \tparam Item Zero-based member index in the structure-of-arrays element.
			//! \return Mutable view of the selected member.
			template <size_t Item>
			auto view_mut() noexcept {
				return mem::data_view_policy<T::gaia_Data_Layout, T>::template set<Item>(
						std::span<uint8_t>{GAIA_ACC((uint8_t*)&m_data[0]), extent});
			}

			//! Returns a read-only view of one structure-of-arrays member.
			//! \tparam Item Zero-based member index in the structure-of-arrays element.
			//! \return Read-only view of the selected member.
			template <size_t Item>
			auto view() const noexcept {
				return mem::data_view_policy<T::gaia_Data_Layout, T>::template get<Item>(
						std::span<const uint8_t>{GAIA_ACC((const uint8_t*)&m_data[0]), extent});
			}
		};

		//! \cond INTERNAL
		namespace detail {
			template <typename T, uint32_t N, uint32_t... I>
			constexpr sarr_ext_soa<std::remove_cv_t<T>, N> to_sarray_impl(T (&a)[N], std::index_sequence<I...> /*no_name*/) {
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
		constexpr sarr_ext_soa<std::remove_cv_t<T>, N> to_sarray(T (&a)[N]) {
			return detail::to_sarray_impl(a, std::make_index_sequence<N>{});
		}

	} // namespace cnt

} // namespace gaia

//! \cond INTERNAL
namespace std {
	template <typename T, uint32_t N>
	struct tuple_size<gaia::cnt::sarr_ext_soa<T, N>>: std::integral_constant<uint32_t, N> {};

	template <size_t I, typename T, uint32_t N>
	struct tuple_element<I, gaia::cnt::sarr_ext_soa<T, N>> {
		using type = T;
	};
} // namespace std
//! \endcond
