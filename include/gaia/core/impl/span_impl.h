//////////////////////////////////////////////////////////////////////////////////////////////////
// Span-compatible interface for c++17 based on:
// https://github.com/tcbrindle/span
// Copyright Tristan Brindle 2018.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file ../../LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "gaia/config/config.h"

#include <tuple>
#include <type_traits>

#include "gaia/core/iterator.h"
#include "gaia/core/utility.h"

namespace gaia {
	namespace core {
		//! Signed type used for span distances.
		using span_diff_type = size_t;
		//! Unsigned type used for span sizes and extents.
		using span_size_type = size_t;
	} // namespace core

	namespace cnt {
		template <typename T, uint32_t N>
		class sarr;
		template <typename T, uint32_t N>
		using sarray = cnt::sarr<T, N>;
	} // namespace cnt

	namespace core {
		//! Sentinel extent indicating that a span size is stored at runtime.
		inline constexpr span_size_type DynamicSpanExtent = (span_size_type)-1;

		//! Non-owning view over a contiguous sequence of objects.
		//! \tparam T Element type.
		//! \tparam Extent Compile-time element count, or DynamicSpanExtent for a runtime count.
		template <typename T, span_size_type Extent = DynamicSpanExtent>
		class span;

		//! \cond INTERNAL
		namespace detail {
			template <typename T, span_size_type Extent>
			struct span_storage {
				constexpr span_storage() noexcept = default;
				constexpr span_storage(const span_storage&) noexcept = default;
				constexpr span_storage(span_storage&&) noexcept = default;
				constexpr span_storage& operator=(const span_storage&) noexcept = default;
				constexpr span_storage& operator=(span_storage&&) noexcept = default;
				constexpr span_storage(T* ptr_, span_size_type /*unused*/) noexcept: beg(ptr_), end(ptr_ + Extent) {}
				~span_storage() noexcept = default;

				T* beg = nullptr;
				T* end = nullptr;
				static constexpr span_size_type size = Extent;
			};

			template <typename T>
			struct span_storage<T, DynamicSpanExtent> {
				constexpr span_storage() noexcept = default;
				constexpr span_storage(const span_storage&) noexcept = default;
				constexpr span_storage(span_storage&&) noexcept = default;
				constexpr span_storage& operator=(const span_storage&) noexcept = default;
				constexpr span_storage& operator=(span_storage&&) noexcept = default;
				constexpr span_storage(T* ptr_, span_size_type size_) noexcept: beg(ptr_), end(ptr_ + size_) {}
				~span_storage() noexcept = default;

				T* beg = nullptr;
				T* end = nullptr;
			};

			template <typename>
			struct is_span: std::false_type {};

			template <typename T, span_size_type S>
			struct is_span<span<T, S>>: std::true_type {};

			template <typename>
			struct is_std_array: std::false_type {};

			template <typename T, auto N>
			struct is_std_array<gaia::cnt::sarray<T, N>>: std::true_type {};

			template <typename C, typename U = raw_t<C>>
			struct is_container {
				static constexpr bool value =
						!is_span<U>::value && !is_std_array<U>::value && !std::is_array<U>::value && has_data_size<C>::value;
			};

			template <typename, typename, typename = void>
			struct is_container_element_kind_compatible: std::false_type {};

			template <typename T, typename E>
			struct is_container_element_kind_compatible<
					T, E,
					typename std::enable_if<
							!std::is_same<typename std::remove_cv<decltype(detail::data(std::declval<T>()))>::type, void>::value &&
							std::is_convertible<
									typename std::remove_pointer_t<decltype(detail::data(std::declval<T>()))> (*)[],
									E (*)[]>::value>::type>: std::true_type {};
		} // namespace detail
		//! \endcond

		//! Non-owning view over a contiguous sequence of objects.
		//! \tparam T Element type.
		//! \tparam Extent Compile-time element count, or DynamicSpanExtent for a runtime count.
		template <typename T, span_size_type Extent>
		class span {
			static_assert(
					std::is_object<T>::value, "A span's T must be an object type (not a "
																		"reference type or void)");
			static_assert(
					detail::is_complete<T>::value, "A span's T must be a complete type (not a forward "
																				 "declaration)");
			static_assert(!std::is_abstract<T>::value, "A span's T cannot be an abstract class type");

			using m_datatype = detail::span_storage<T, Extent>;

		public:
			//! Element type, including const qualification.
			using element_kind = T;
			//! Element type without const qualification.
			using value_type = typename std::remove_cv<T>::type;
			//! Type used for element counts.
			using size_type = span_size_type;
			//! Type used for iterator differences.
			using difference_type = span_diff_type;
			//! Pointer to an element.
			using pointer = element_kind*;
			//! Pointer to a const element.
			using const_pointer = const element_kind*;
			//! Reference to an element.
			using reference = element_kind&;
			//! Reference to a const element.
			using const_reference = const element_kind&;

			//! Mutable iterator type.
			using iterator = pointer;
			//! Const iterator type.
			using const_iterator = const_pointer;
			//! Gaia-ECS iterator category exposed by span iterators.
			using iterator_type = core::random_access_iterator_tag;

			//! Compile-time extent of the view.
			static constexpr size_type extent = Extent;

		private:
			m_datatype m_data{};

		public:
			//! Constructs an empty span when its extent permits an empty view.
			//! \tparam E Extent checked by the constructor constraint.
			template <span_size_type E = Extent, typename std::enable_if<(E == DynamicSpanExtent || E <= 0), int>::type = 0>
			constexpr span() noexcept {}

			//! Constructs a view over count elements starting at ptr.
			//! \param ptr Pointer to the first element.
			//! \param count Number of elements in the view.
			constexpr span(pointer ptr, size_type count): m_data(ptr, count) {
				GAIA_ASSERT(extent == DynamicSpanExtent || extent == count);
			}

			//! Constructs a view over the half-open range [begin, end).
			//! \param begin Pointer to the first element.
			//! \param end Pointer one past the final element.
			constexpr span(pointer begin, pointer end): m_data(begin, end - begin) {
				GAIA_ASSERT(extent == DynamicSpanExtent || ((uintptr_t)(end - begin) == (uintptr_t)extent));
			}

			//! Constructs a view over a compatible built-in array.
			//! \tparam N Array element count.
			//! \tparam E Extent checked by the constructor constraint.
			//! \param arr Array to view.
			template <
					span_size_type N, span_size_type E = Extent,
					typename std::enable_if<
							(E == DynamicSpanExtent || N == E) &&
									detail::is_container_element_kind_compatible<element_kind (&)[N], T>::value,
							int>::type = 0>
			constexpr span(element_kind (&arr)[N]) noexcept: m_data(arr, N) {}

			//! Constructs a dynamic-extent view over a compatible mutable container.
			//! \tparam Container Container type exposing contiguous data and size.
			//! \tparam E Extent checked by the constructor constraint.
			//! \param cont Container to view.
			template <
					typename Container, span_size_type E = Extent,
					typename std::enable_if<
							E == DynamicSpanExtent && detail::is_container<Container>::value &&
									detail::is_container_element_kind_compatible<Container&, T>::value,
							int>::type = 0>
			constexpr span(Container& cont): m_data(detail::data(cont), detail::size(cont)) {}

			//! Constructs a dynamic-extent view over a compatible const container.
			//! \tparam Container Container type exposing contiguous data and size.
			//! \tparam E Extent checked by the constructor constraint.
			//! \param cont Container to view.
			template <
					typename Container, span_size_type E = Extent,
					typename std::enable_if<
							E == DynamicSpanExtent && detail::is_container<Container>::value &&
									detail::is_container_element_kind_compatible<const Container&, T>::value,
							int>::type = 0>
			constexpr span(const Container& cont): m_data(detail::data(cont), detail::size(cont)) {}

			constexpr span(const span&) noexcept = default;
			constexpr span& operator=(const span&) noexcept = default;

			//! Constructs a compatible span from another element type or extent.
			//! \tparam T2 Source element type.
			//! \tparam Extent2 Source extent.
			//! \param other Source view.
			template <
					typename T2, span_size_type Extent2,
					typename std::enable_if<
							(Extent == DynamicSpanExtent || Extent2 == DynamicSpanExtent || Extent == Extent2) &&
									std::is_convertible<T2 (*)[], T (*)[]>::value,
							int>::type = 0>
			constexpr span(const span<T2, Extent2>& other) noexcept: m_data(other.data(), other.size()) {}

			~span() noexcept = default;

			//! Returns a pointer to the first element.
			//! \return Pointer to the first element, or nullptr for a default-constructed empty span.
			GAIA_NODISCARD constexpr pointer data() const noexcept {
				return m_data.beg;
			}

			//! Returns the number of elements in the view.
			//! \return Element count.
			GAIA_NODISCARD constexpr size_type size() const noexcept {
				return size_type(m_data.end - m_data.beg);
			}

			//! Checks whether the view is empty.
			//! \return True when size() is zero.
			GAIA_NODISCARD constexpr bool empty() const noexcept {
				return m_data.beg == m_data.end;
			}

			//! Returns the element at index.
			//! \param index Zero-based element index.
			//! \return Reference to the selected element.
			GAIA_NODISCARD constexpr reference operator[](size_type index) const {
				GAIA_ASSERT((uintptr_t)m_data.beg + index < (uintptr_t)m_data.end);
				return *(m_data.beg + index);
			}

			//! Returns an iterator to the first element.
			//! \return Beginning iterator.
			GAIA_NODISCARD constexpr iterator begin() noexcept {
				return {m_data.beg};
			}

			//! Returns an iterator to the first element.
			//! \return Beginning iterator.
			GAIA_NODISCARD constexpr iterator begin() const noexcept {
				return {m_data.beg};
			}

			//! Returns a const iterator to the first element.
			//! \return Const beginning iterator.
			GAIA_NODISCARD constexpr const_iterator cbegin() const noexcept {
				return {m_data.beg};
			}

			//! Returns an iterator one past the final element.
			//! \return Ending iterator.
			GAIA_NODISCARD constexpr iterator end() noexcept {
				return {m_data.end};
			}

			//! Returns a const iterator one past the final element.
			//! \return Const ending iterator.
			GAIA_NODISCARD constexpr const_iterator end() const noexcept {
				return {m_data.end};
			}

			//! Returns a const iterator one past the final element.
			//! \return Const ending iterator.
			GAIA_NODISCARD constexpr const_iterator cend() const noexcept {
				return {m_data.end};
			}

			//! Returns the first Count elements with a compile-time extent.
			//! \tparam Count Number of elements in the result.
			//! \return Fixed-extent view over the first Count elements.
			template <span_size_type Count>
			GAIA_NODISCARD constexpr span<element_kind, Count> first() const {
				GAIA_ASSERT(Count <= size());
				return {m_data.beg, Count};
			}

			//! Returns a dynamic-extent view over the first count elements.
			//! \param count Number of elements in the result.
			//! \return View over the requested prefix.
			GAIA_NODISCARD constexpr span<element_kind, DynamicSpanExtent> first(size_type count) const {
				GAIA_ASSERT(count <= size());
				return {m_data.beg, count};
			}

			//! Returns the last Count elements with a compile-time extent.
			//! \tparam Count Number of elements in the result.
			//! \return Fixed-extent view over the last Count elements.
			template <span_size_type Count>
			GAIA_NODISCARD constexpr span<element_kind, Count> last() const {
				GAIA_ASSERT(Count <= size());
				return {m_data.beg + (size() - Count), Count};
			}

			//! Returns a dynamic-extent view over the last count elements.
			//! \param count Number of elements in the result.
			//! \return View over the requested suffix.
			GAIA_NODISCARD constexpr span<element_kind, DynamicSpanExtent> last(size_type count) const {
				GAIA_ASSERT(count <= size());
				return {m_data.beg + (size() - count), count};
			}

			//! Returns the first element.
			//! \return Reference to the first element.
			GAIA_NODISCARD constexpr reference front() const {
				GAIA_ASSERT(!empty());
				return *m_data.beg;
			}

			//! Returns the final element.
			//! \return Reference to the final element.
			GAIA_NODISCARD constexpr reference back() const {
				GAIA_ASSERT(!empty());
				return *(m_data.beg + (size() - 1));
			}

			//! Result type for a compile-time subspan selection.
			//! \tparam Offset Starting element index.
			//! \tparam Count Requested element count, or DynamicSpanExtent for the remaining elements.
			template <span_size_type Offset, span_size_type Count = DynamicSpanExtent>
			using subspan_return_t = span<
					T, Count != DynamicSpanExtent ? Count : (Extent != DynamicSpanExtent ? Extent - Offset : DynamicSpanExtent)>;

			//! Returns a subspan selected with compile-time offset and count.
			//! \tparam Offset Starting element index.
			//! \tparam Count Requested element count, or DynamicSpanExtent for the remaining elements.
			//! \return View over the selected elements.
			template <span_size_type Offset, span_size_type Count = DynamicSpanExtent>
			GAIA_NODISCARD constexpr subspan_return_t<Offset, Count> subspan() const {
				GAIA_ASSERT(Offset <= size() && (Count == DynamicSpanExtent || Offset + Count <= size()));
				return {m_data.beg + Offset, Count != DynamicSpanExtent ? Count : size() - Offset};
			}

			//! Returns a dynamic-extent subspan.
			//! \param offset Starting element index.
			//! \param count Requested element count, or DynamicSpanExtent for the remaining elements.
			//! \return View over the selected elements.
			GAIA_NODISCARD constexpr span<element_kind, DynamicSpanExtent>
			subspan(size_type offset, size_type count = DynamicSpanExtent) const {
				GAIA_ASSERT(offset <= size() && (count == DynamicSpanExtent || offset + count <= size()));
				return {m_data.beg + offset, count != DynamicSpanExtent ? count : size() - offset};
			}
		};

		//! Deduces a fixed-extent span from a built-in array.
		//! \tparam T Element type.
		//! \tparam N Array element count.
		//! \param arr Array to view.
		template <typename T, size_t N>
		span(T (&arr)[N]) -> span<T, N>;

		//! Deduces a fixed-extent span from a mutable Gaia-ECS static array.
		//! \tparam T Element type.
		//! \tparam N Array element count.
		//! \param arr Array to view.
		template <typename T, size_t N>
		span(gaia::cnt::sarray<T, N>& arr) -> span<T, N>;

		//! Deduces a fixed-extent const span from a const Gaia-ECS static array.
		//! \tparam T Element type.
		//! \tparam N Array element count.
		//! \param arr Array to view.
		template <typename T, size_t N>
		span(const gaia::cnt::sarray<T, N>& arr) -> span<const T, N>;

		//! Deduces a dynamic-extent span from a mutable contiguous container.
		//! \tparam Container Container type.
		//! \param cont Container to view.
		template <typename Container>
		span(Container& cont)
				-> span<typename std::remove_reference<decltype(*detail::data(std::declval<Container&>()))>::type>;

		//! Deduces a dynamic-extent const span from a const contiguous container.
		//! \tparam Container Container type.
		//! \param cont Container to view.
		template <typename Container>
		span(const Container& cont) -> span<const typename Container::value_type>;

		//! Returns the element at compile-time index N.
		//! \tparam N Element index.
		//! \tparam E Element type.
		//! \tparam S Span extent.
		//! \param s Span to access.
		//! \return Reference to element N.
		template <span_size_type N, typename E, span_size_type S>
		constexpr auto get(span<E, S> s) -> decltype(s[N]) {
			return s[N];
		}
	} // namespace core
} // namespace gaia

//! \cond INTERNAL
namespace std {
	template <typename T, size_t Extent>
	struct tuple_size<gaia::core::span<T, Extent>>: public integral_constant<size_t, Extent> {};

	template <typename T>
	struct tuple_size<gaia::core::span<T, gaia::core::DynamicSpanExtent>>; // not defined

	template <size_t I, typename T, size_t Extent>
	struct tuple_element<I, gaia::core::span<T, Extent>> {
		static_assert(Extent != gaia::core::DynamicSpanExtent && I < Extent, "");
		using type = T;
	};
} // end namespace std
//! \endcond
