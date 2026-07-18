#pragma once
#include "gaia/config/config.h"

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

#include "gaia/core/iterator.h"
#include "gaia/core/utility.h"
#include "gaia/mem/data_layout_policy.h"

namespace gaia {
	namespace cnt {
		//! \cond INTERNAL
		namespace sarr_detail {
			using difference_type = uint32_t;
			using size_type = uint32_t;
		} // namespace sarr_detail
		//! \endcond

		//! Fixed-size stack array with AoS storage.
		//! Interface compatiblity with std::array where it matters.
		//! \tparam T Element type stored directly in the array.
		//! \tparam N Number of elements and fixed capacity.
		//! \note This container can be used in constexpr environment.
		template <typename T, sarr_detail::size_type N>
		class sarr {
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
			using difference_type = sarr_detail::difference_type;
			//! Unsigned type used for sizes and indices.
			using size_type = sarr_detail::size_type;

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

			//! Inline storage backing the container elements.
			T m_data[N];

			constexpr sarr() noexcept = default;

			//! Explicit value-initialization constructor for call sites that want zero/value initialization to be obvious.
			//! \note Even though sarr is an aggregate type and we could zero-initialize by doing sarr<int,10> tmp{}, we keep
			//!       this function around for design compatibility with other containers that are not aggregate types.
			constexpr sarr(core::zero_t) noexcept {
				for (auto i = (size_type)0; i < extent; ++i)
					m_data[i] = {};
			}

			GAIA_CONSTEXPR_DTOR ~sarr() = default;

			//! Constructs a container from an iterator range.
			//! \tparam InputIt Input iterator type.
			//! \param first Iterator to the first source element.
			//! \param last Iterator one past the last source element.
			template <typename InputIt>
			constexpr sarr(InputIt first, InputIt last) noexcept: sarr() {
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
			constexpr sarr(std::initializer_list<T> il): sarr(il.begin(), il.end()) {}

			constexpr sarr(const sarr&) = default;

			constexpr sarr(sarr&&) noexcept = default;

			//! Replaces the elements from an initializer list.
			//! \param il Initializer list supplying the elements.
			//! \return Reference to this container.
			sarr& operator=(std::initializer_list<T> il) {
				*this = sarr(il.begin(), il.end());
				return *this;
			}

			//! Copy-assigns the container.
			//! \param other Container to copy or move from.
			//! \return Reference to this container.
			constexpr sarr& operator=(const sarr& other) {
				GAIA_ASSERT(core::addressof(other) != this);

				for (size_type i = 0; i < extent; ++i)
					m_data[i] = other.m_data[i];

				return *this;
			}

			//! Move-assigns the container.
			//! \param other Container to copy or move from.
			//! \return Reference to this container.
			constexpr sarr& operator=(sarr&& other) noexcept {
				GAIA_ASSERT(core::addressof(other) != this);

				for (size_type i = 0; i < extent; ++i)
					m_data[i] = GAIA_MOV(other.m_data[i]);

				return *this;
			}

			GAIA_CLANG_WARNING_PUSH()
			// Memory is aligned so we can silence this warning
			GAIA_CLANG_WARNING_DISABLE("-Wcast-align")

			//! Returns a pointer to the element storage.
			//! \return Pointer to the first element storage location.
			GAIA_NODISCARD constexpr pointer data() noexcept {
				return &m_data[0];
			}

			//! Returns a pointer to the element storage.
			//! \return Pointer to the first element storage location.
			GAIA_NODISCARD constexpr const_pointer data() const noexcept {
				return &m_data[0];
			}

			//! Accesses an element without bounds checking in optimized builds.
			//! \param pos Zero-based element index.
			//! \return Reference to the selected element.
			GAIA_NODISCARD constexpr decltype(auto) operator[](size_type pos) noexcept {
				GAIA_ASSERT(pos < size());
				return m_data[pos];
			}

			//! Accesses an element without bounds checking in optimized builds.
			//! \param pos Zero-based element index.
			//! \return Reference to the selected element.
			GAIA_NODISCARD constexpr decltype(auto) operator[](size_type pos) const noexcept {
				GAIA_ASSERT(pos < size());
				return m_data[pos];
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
			GAIA_NODISCARD constexpr decltype(auto) front() noexcept {
				return (reference)*begin();
			}

			//! Accesses the first element.
			//! \return Reference to the first element.
			GAIA_NODISCARD constexpr decltype(auto) front() const noexcept {
				return (const_reference)*begin();
			}

			//! Accesses the last element.
			//! \return Reference to the last element.
			GAIA_NODISCARD constexpr decltype(auto) back() noexcept {
				return (reference) operator[](N - 1);
			}

			//! Accesses the last element.
			//! \return Reference to the last element.
			GAIA_NODISCARD constexpr decltype(auto) back() const noexcept {
				return (const_reference) operator[](N - 1);
			}

			//! Returns an iterator to the first element.
			//! \return Iterator to the first element.
			GAIA_NODISCARD constexpr auto begin() noexcept {
				return iterator(&m_data[0]);
			}

			//! Returns an iterator to the first element.
			//! \return Iterator to the first element.
			GAIA_NODISCARD constexpr auto begin() const noexcept {
				return const_iterator(&m_data[0]);
			}

			//! Returns a read-only iterator to the first element.
			//! \return Iterator to the first element.
			GAIA_NODISCARD constexpr auto cbegin() const noexcept {
				return const_iterator(&m_data[0]);
			}

			//! Returns a reverse traversal iterator to the last element.
			//! \return Iterator to the last element.
			GAIA_NODISCARD constexpr auto rbegin() noexcept {
				return iterator((pointer)&back());
			}

			//! Returns a reverse traversal iterator to the last element.
			//! \return Iterator to the last element.
			GAIA_NODISCARD constexpr auto rbegin() const noexcept {
				return const_iterator((const_pointer)&back());
			}

			//! Returns a read-only reverse traversal iterator to the last element.
			//! \return Iterator to the last element.
			GAIA_NODISCARD constexpr auto crbegin() const noexcept {
				return const_iterator((const_pointer)&back());
			}

			//! Returns an iterator one past the last element.
			//! \return Iterator one past the last element.
			GAIA_NODISCARD constexpr auto end() noexcept {
				return iterator(&m_data[0] + size());
			}

			//! Returns an iterator one past the last element.
			//! \return Iterator one past the last element.
			GAIA_NODISCARD constexpr auto end() const noexcept {
				return const_iterator(&m_data[0] + size());
			}

			//! Returns a read-only iterator one past the last element.
			//! \return Iterator one past the last element.
			GAIA_NODISCARD constexpr auto cend() const noexcept {
				return const_iterator(&m_data[0] + size());
			}

			//! Returns the reverse traversal sentinel preceding the first element.
			//! \return Reverse traversal sentinel preceding the first element.
			GAIA_NODISCARD constexpr auto rend() noexcept {
				return iterator(&m_data[0] - 1);
			}

			//! Returns the reverse traversal sentinel preceding the first element.
			//! \return Reverse traversal sentinel preceding the first element.
			GAIA_NODISCARD constexpr auto rend() const noexcept {
				return const_iterator(&m_data[0] - 1);
			}

			//! Returns the read-only reverse traversal sentinel preceding the first element.
			//! \return Reverse traversal sentinel preceding the first element.
			GAIA_NODISCARD constexpr auto crend() const noexcept {
				return const_iterator(&m_data[0] - 1);
			}

			//! Compares two containers element by element.
			//! \param other Container to copy or move from.
			//! \return True if both containers contain equal elements.
			GAIA_NODISCARD constexpr bool operator==(const sarr& other) const {
				for (size_type i = 0; i < N; ++i)
					if (!(operator[](i) == other[i]))
						return false;
				return true;
			}

			//! Checks whether two containers differ.
			//! \param other Container to copy or move from.
			//! \return True if the containers differ.
			GAIA_NODISCARD constexpr bool operator!=(const sarr& other) const {
				return !operator==(other);
			}
		};

		//! \cond INTERNAL
		namespace detail {
			template <typename T, uint32_t N, uint32_t... I>
			constexpr sarr<std::remove_cv_t<T>, N> to_array_impl(T (&a)[N], std::index_sequence<I...> /*no_name*/) {
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
		constexpr sarr<std::remove_cv_t<T>, N> to_array(T (&a)[N]) {
			return detail::to_array_impl(a, std::make_index_sequence<N>{});
		}

		//! Deduces the fixed-size container element type and extent.
		//! \tparam T Element type.
		//! \tparam U Types of the remaining deduction-guide arguments.
		template <typename T, typename... U>
		sarr(T, U...) -> sarr<T, 1 + (uint32_t)sizeof...(U)>;

	} // namespace cnt
} // namespace gaia

//! \cond INTERNAL
namespace std {
	template <typename T, uint32_t N>
	struct tuple_size<gaia::cnt::sarr<T, N>>: std::integral_constant<uint32_t, N> {};

	template <size_t I, typename T, uint32_t N>
	struct tuple_element<I, gaia::cnt::sarr<T, N>> {
		using type = T;
	};
} // namespace std
//! \endcond
