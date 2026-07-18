#pragma once
#include "gaia/config/config.h"

#include <cstddef>
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
		namespace sarr_soa_detail {
			using difference_type = uint32_t;
			using size_type = uint32_t;
		} // namespace sarr_soa_detail
		//! \endcond

		//! \cond INTERNAL
		template <typename T>
		struct sarr_soa_iterator {
			//! Element type stored by the container.
			using value_type = T;
			// using pointer = T*; not supported
			// using reference = T&; not supported
			//! Type used for iterator differences.
			using difference_type = sarr_soa_detail::difference_type;
			//! Unsigned type used for sizes and indices.
			using size_type = sarr_soa_detail::size_type;

			using iterator = sarr_soa_iterator;
			//! Iterator category exposed by the container.
			using iterator_category = core::random_access_iterator_tag;

		private:
			uint8_t* m_ptr;
			uint32_t m_cnt;
			uint32_t m_idx;

		public:
			sarr_soa_iterator(uint8_t* ptr, uint32_t cnt, uint32_t idx): m_ptr(ptr), m_cnt(cnt), m_idx(idx) {}

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
		struct const_sarr_soa_iterator {
			using value_type = T;
			// using pointer = T*; not supported
			// using reference = T&; not supported
			using difference_type = sarr_soa_detail::difference_type;
			using size_type = sarr_soa_detail::size_type;

			using iterator = const_sarr_soa_iterator;
			using iterator_category = core::random_access_iterator_tag;

		private:
			const uint8_t* m_ptr;
			uint32_t m_cnt;
			uint32_t m_idx;

		public:
			const_sarr_soa_iterator(const uint8_t* ptr, uint32_t cnt, uint32_t idx): m_ptr(ptr), m_cnt(cnt), m_idx(idx) {}

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

		//! Fixed-size stack array with SoA storage.
		//! Interface compatiblity with std::array where it matters.
		//! \tparam T Element type stored directly in the array.
		//! \tparam N Number of elements and fixed capacity.
		template <typename T, sarr_soa_detail::size_type N>
		class sarr_soa {
			static_assert(mem::is_soa_layout_v<T>, "sarr_soa can be used only with soa types");

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
			using const_pointer = T*;
			//! Data-layout access policy used by the container.
			using view_policy = mem::data_view_policy_soa<T::gaia_Data_Layout, T>;
			//! Type used for iterator differences.
			using difference_type = sarr_soa_detail::difference_type;
			//! Unsigned type used for sizes and indices.
			using size_type = sarr_soa_detail::size_type;

			//! Mutable random-access iterator type.
			using iterator = sarr_soa_iterator<T>;
			//! Read-only random-access iterator type.
			using const_iterator = const_sarr_soa_iterator<T>;
			//! Iterator category exposed by the container.
			using iterator_category = core::random_access_iterator_tag;

			//! Fixed capacity of the container.
			static constexpr size_type extent = N;
			//! Number of bytes reserved by the inline storage.
			static constexpr uint32_t allocated_bytes = view_policy::get_min_byte_size(0, N);

			//! Inline storage backing the container elements.
			mem::raw_data_holder<T, allocated_bytes> m_data;

			sarr_soa() noexcept = default;

			//! Zero-initialization constructor. Because sarr_soa is not aggretate type, doing: sarr_soa<int,10> tmp{} does
			//! not zero-initialize its internals. We need to be explicit about our intent and use a special constructor.
			sarr_soa(core::zero_t) noexcept {
				// explicit zeroing
				for (auto i = (size_type)0; i < extent; ++i)
					operator[](i) = {};
			}

			~sarr_soa() = default;

			//! Constructs a container from an iterator range.
			//! \tparam InputIt Input iterator type.
			//! \param first Iterator to the first source element.
			//! \param last Iterator one past the last source element.
			template <typename InputIt>
			sarr_soa(InputIt first, InputIt last) noexcept {
				const auto count = (size_type)core::distance(first, last);

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
			sarr_soa(std::initializer_list<T> il): sarr_soa(il.begin(), il.end()) {}

			//! Copy-constructs a container.
			//! \param other Container to copy or move from.
			sarr_soa(const sarr_soa& other): sarr_soa(other.begin(), other.end()) {}

			//! Move-constructs a container.
			//! \param other Container to copy or move from.
			sarr_soa(sarr_soa&& other) noexcept {
				GAIA_ASSERT(core::addressof(other) != this);

				mem::move_elements<T, true>((uint8_t*)m_data, (uint8_t*)other.m_data, other.size(), 0, extent, other.extent);
			}

			//! Replaces the elements from an initializer list.
			//! \param il Initializer list supplying the elements.
			//! \return Reference to this container.
			sarr_soa& operator=(std::initializer_list<T> il) {
				*this = sarr_soa(il.begin(), il.end());
				return *this;
			}

			//! Copy-assigns the container.
			//! \param other Container to copy or move from.
			//! \return Reference to this container.
			sarr_soa& operator=(const sarr_soa& other) {
				GAIA_ASSERT(core::addressof(other) != this);

				mem::copy_elements<T, true>(
						GAIA_ACC((uint8_t*)&m_data[0]), GAIA_ACC((const uint8_t*)&other.m_data[0]), other.size(), 0, extent,
						other.extent);

				return *this;
			}

			//! Move-assigns the container.
			//! \param other Container to copy or move from.
			//! \return Reference to this container.
			sarr_soa& operator=(sarr_soa&& other) noexcept {
				GAIA_ASSERT(core::addressof(other) != this);

				mem::move_elements<T, true>(
						GAIA_ACC((uint8_t*)&m_data[0]), GAIA_ACC((uint8_t*)&other.m_data[0]), other.size(), 0, extent,
						other.extent);

				return *this;
			}

			GAIA_CLANG_WARNING_PUSH()
			// Memory is aligned so we can silence this warning
			GAIA_CLANG_WARNING_DISABLE("-Wcast-align")

			//! Returns a pointer to the element storage.
			//! \return Pointer to the first element storage location.
			GAIA_NODISCARD pointer data() noexcept {
				return GAIA_ACC((pointer)&m_data[0]);
			}

			//! Returns a pointer to the element storage.
			//! \return Pointer to the first element storage location.
			GAIA_NODISCARD const_pointer data() const noexcept {
				return GAIA_ACC((const_pointer)&m_data[0]);
			}

			//! Accesses an element without bounds checking in optimized builds.
			//! \param pos Zero-based element index.
			//! \return Reference to the selected element.
			GAIA_NODISCARD decltype(auto) operator[](size_type pos) noexcept {
				GAIA_ASSERT(pos < size());
				return view_policy::set({GAIA_ACC((typename view_policy::TargetCastType) & m_data[0]), extent}, pos);
			}

			//! Accesses an element without bounds checking in optimized builds.
			//! \param pos Zero-based element index.
			//! \return Reference to the selected element.
			GAIA_NODISCARD decltype(auto) operator[](size_type pos) const noexcept {
				GAIA_ASSERT(pos < size());
				return view_policy::get({GAIA_ACC((typename view_policy::TargetCastType) & m_data[0]), extent}, pos);
			}

			GAIA_CLANG_WARNING_POP()

			//! Returns the number of elements.
			//! \return Current element count.
			GAIA_NODISCARD constexpr size_type size() const noexcept {
				return N;
			}

			//! Checks whether the container has no elements.
			//! \return True if the container contains no elements.
			GAIA_NODISCARD constexpr bool empty() const noexcept {
				return false;
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
			GAIA_NODISCARD decltype(auto) front() noexcept {
				return *begin();
			}

			//! Accesses the first element.
			//! \return Reference to the first element.
			GAIA_NODISCARD decltype(auto) front() const noexcept {
				return *begin();
			}

			//! Accesses the last element.
			//! \return Reference to the last element.
			GAIA_NODISCARD decltype(auto) back() noexcept {
				return (operator[])(N - 1);
			}

			//! Accesses the last element.
			//! \return Reference to the last element.
			GAIA_NODISCARD decltype(auto) back() const noexcept {
				return operator[](N - 1);
			}

			//! Returns an iterator to the first element.
			//! \return Iterator to the first element.
			GAIA_NODISCARD auto begin() noexcept {
				return iterator(GAIA_ACC(&m_data[0]), extent, 0);
			}

			//! Returns an iterator to the first element.
			//! \return Iterator to the first element.
			GAIA_NODISCARD auto begin() const noexcept {
				return const_iterator(GAIA_ACC(&m_data[0]), extent, 0);
			}

			//! Returns a read-only iterator to the first element.
			//! \return Iterator to the first element.
			GAIA_NODISCARD auto cbegin() const noexcept {
				return const_iterator(GAIA_ACC(&m_data[0]), extent, 0);
			}

			//! Returns a reverse traversal iterator to the last element.
			//! \return Iterator to the last element.
			GAIA_NODISCARD auto rbegin() noexcept {
				return iterator(GAIA_ACC(&m_data[0]), extent, size() - 1);
			}

			//! Returns a reverse traversal iterator to the last element.
			//! \return Iterator to the last element.
			GAIA_NODISCARD auto rbegin() const noexcept {
				return const_iterator(m_data, extent, size() - 1);
			}

			//! Returns a read-only reverse traversal iterator to the last element.
			//! \return Iterator to the last element.
			GAIA_NODISCARD auto crbegin() const noexcept {
				return const_iterator(m_data, extent, size() - 1);
			}

			//! Returns an iterator one past the last element.
			//! \return Iterator one past the last element.
			GAIA_NODISCARD auto end() noexcept {
				return iterator(GAIA_ACC(&m_data[0]), extent, size());
			}

			//! Returns an iterator one past the last element.
			//! \return Iterator one past the last element.
			GAIA_NODISCARD auto end() const noexcept {
				return const_iterator(GAIA_ACC(&m_data[0]), extent, size());
			}

			//! Returns a read-only iterator one past the last element.
			//! \return Iterator one past the last element.
			GAIA_NODISCARD auto cend() const noexcept {
				return const_iterator(GAIA_ACC(&m_data[0]), extent, size());
			}

			//! Returns the reverse traversal sentinel preceding the first element.
			//! \return Reverse traversal sentinel preceding the first element.
			GAIA_NODISCARD auto rend() noexcept {
				return iterator(GAIA_ACC(&m_data[0]), extent, -1);
			}

			//! Returns the reverse traversal sentinel preceding the first element.
			//! \return Reverse traversal sentinel preceding the first element.
			GAIA_NODISCARD auto rend() const noexcept {
				return const_iterator(GAIA_ACC(&m_data[0]), extent, -1);
			}

			//! Returns the read-only reverse traversal sentinel preceding the first element.
			//! \return Reverse traversal sentinel preceding the first element.
			GAIA_NODISCARD auto crend() const noexcept {
				return const_iterator(GAIA_ACC(&m_data[0]), extent, -1);
			}

			//! Compares two containers element by element.
			//! \param other Container to copy or move from.
			//! \return True if both containers contain equal elements.
			GAIA_NODISCARD bool operator==(const sarr_soa& other) const {
				for (size_type i = 0; i < N; ++i)
					if (!(operator[](i) == other[i]))
						return false;
				return true;
			}

			//! Checks whether two containers differ.
			//! \param other Container to copy or move from.
			//! \return True if the containers differ.
			GAIA_NODISCARD bool operator!=(const sarr_soa& other) const {
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
			sarr_soa<std::remove_cv_t<T>, N> to_array_impl(T (&a)[N], std::index_sequence<I...> /*no_name*/) {
				return {{a[I]...}};
			}
		} // namespace detail
		//! \endcond

		//! Converts a built-in array to a fixed-size Gaia-ECS array.
		//! \tparam T Element type.
		//! \tparam N Number of elements in the source array.
		//! \param a Built-in array to convert.
		//! \return Converted Gaia-ECS container.
		template <typename T, uint32_t N>
		sarr_soa<std::remove_cv_t<T>, N> to_array(T (&a)[N]) {
			return detail::to_array_impl(a, std::make_index_sequence<N>{});
		}

		//! Deduces the fixed-size container element type and extent.
		//! \tparam T Element type.
		//! \tparam U Types of the remaining deduction-guide arguments.
		template <typename T, typename... U>
		sarr_soa(T, U...) -> sarr_soa<T, 1 + (uint32_t)sizeof...(U)>;

	} // namespace cnt
} // namespace gaia

//! \cond INTERNAL
namespace std {
	template <typename T, uint32_t N>
	struct tuple_size<gaia::cnt::sarr_soa<T, N>>: std::integral_constant<uint32_t, N> {};

	template <size_t I, typename T, uint32_t N>
	struct tuple_element<I, gaia::cnt::sarr_soa<T, N>> {
		using type = T;
	};
} // namespace std
//! \endcond
