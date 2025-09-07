//////////////////////////////////////////////////////////////////////////////////////////////////
// Span-compatible interface for c++17 based on:
// https://github.com/tcbrindle/span
// Copyright Tristan Brindle 2018.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file ../../LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "../../config/config.h"

#include <tuple>
#include <type_traits>

#include "../iterator.h"
#include "../utility.h"

namespace gaia {
	namespace core {
		using span_diff_type = size_t;
		using span_size_type = size_t;
	} // namespace core

	namespace cnt {
		template <typename T, uint32_t N>
		class sarr;
		template <typename T, uint32_t N>
		using sarray = cnt::sarr<T, N>;
	} // namespace cnt

	namespace core {
		inline constexpr span_size_type DynamicSpanExtent = (span_size_type)-1;

		template <typename T, span_size_type Extent = DynamicSpanExtent>
		class span;

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
			using element_kind = T;
			using value_type = typename std::remove_cv<T>::type;
			using size_type = span_size_type;
			using difference_type = span_diff_type;
			using pointer = element_kind*;
			using const_pointer = const element_kind*;
			using reference = element_kind&;
			using const_reference = const element_kind&;

			using iterator = pointer;
			using iterator_type = core::random_access_iterator_tag;

			static constexpr size_type extent = Extent;

		private:
			m_datatype m_data{};

		public:
			template <span_size_type E = Extent, typename std::enable_if<(E == DynamicSpanExtent || E <= 0), int>::type = 0>
			constexpr span() noexcept {}

			constexpr span(pointer ptr, size_type count): m_data(ptr, count) {
				GAIA_ASSERT(extent == DynamicSpanExtent || extent == count);
			}

			constexpr span(pointer begin, pointer end): m_data(begin, end - begin) {
				GAIA_ASSERT(extent == DynamicSpanExtent || ((uintptr_t)(end - begin) == (uintptr_t)extent));
			}

			template <
					span_size_type N, span_size_type E = Extent,
					typename std::enable_if<
							(E == DynamicSpanExtent || N == E) &&
									detail::is_container_element_kind_compatible<element_kind (&)[N], T>::value,
							int>::type = 0>
			constexpr span(element_kind (&arr)[N]) noexcept: m_data(arr, N) {}

			template <
					typename Container, span_size_type E = Extent,
					typename std::enable_if<
							E == DynamicSpanExtent && detail::is_container<Container>::value &&
									detail::is_container_element_kind_compatible<Container&, T>::value,
							int>::type = 0>
			constexpr span(Container& cont): m_data(detail::data(cont), detail::size(cont)) {}

			template <
					typename Container, span_size_type E = Extent,
					typename std::enable_if<
							E == DynamicSpanExtent && detail::is_container<Container>::value &&
									detail::is_container_element_kind_compatible<const Container&, T>::value,
							int>::type = 0>
			constexpr span(const Container& cont): m_data(detail::data(cont), detail::size(cont)) {}

			constexpr span(const span& other) noexcept = default;
			constexpr span& operator=(const span& other) noexcept = default;

			template <
					typename T2, span_size_type Extent2,
					typename std::enable_if<
							(Extent == DynamicSpanExtent || Extent2 == DynamicSpanExtent || Extent == Extent2) &&
									std::is_convertible<T2 (*)[], T (*)[]>::value,
							int>::type = 0>
			constexpr span(const span<T2, Extent2>& other) noexcept: m_data(other.data(), other.size()) {}

			~span() noexcept = default;

			GAIA_NODISCARD constexpr pointer data() const noexcept {
				return m_data.beg;
			}

			GAIA_NODISCARD constexpr size_type size() const noexcept {
				return size_type(m_data.end - m_data.beg);
			}

			GAIA_NODISCARD constexpr bool empty() const noexcept {
				return m_data.beg == m_data.end;
			}

			GAIA_NODISCARD constexpr reference operator[](size_type index) const {
				GAIA_ASSERT((uintptr_t)m_data.beg + index < (uintptr_t)m_data.end);
				return *(m_data.beg + index);
			}

			GAIA_NODISCARD constexpr iterator begin() const noexcept {
				return {m_data.beg};
			}

			GAIA_NODISCARD constexpr iterator end() const noexcept {
				return {m_data.end};
			}

			template <span_size_type Count>
			GAIA_NODISCARD constexpr span<element_kind, Count> first() const {
				GAIA_ASSERT(Count <= size());
				return {m_data.beg, Count};
			}

			GAIA_NODISCARD constexpr span<element_kind, DynamicSpanExtent> first(size_type count) const {
				GAIA_ASSERT(count <= size());
				return {m_data.beg, count};
			}

			template <span_size_type Count>
			GAIA_NODISCARD constexpr span<element_kind, Count> last() const {
				GAIA_ASSERT(Count <= size());
				return {m_data.beg + (size() - Count), Count};
			}

			GAIA_NODISCARD constexpr span<element_kind, DynamicSpanExtent> last(size_type count) const {
				GAIA_ASSERT(count <= size());
				return {m_data.beg + (size() - count), count};
			}

			GAIA_NODISCARD constexpr reference front() const {
				GAIA_ASSERT(!empty());
				return *m_data.beg;
			}

			GAIA_NODISCARD constexpr reference back() const {
				GAIA_ASSERT(!empty());
				return *(m_data.beg + (size() - 1));
			}

			template <span_size_type Offset, span_size_type Count = DynamicSpanExtent>
			using subspan_return_t = span<
					T, Count != DynamicSpanExtent ? Count : (Extent != DynamicSpanExtent ? Extent - Offset : DynamicSpanExtent)>;

			template <span_size_type Offset, span_size_type Count = DynamicSpanExtent>
			GAIA_NODISCARD constexpr subspan_return_t<Offset, Count> subspan() const {
				GAIA_ASSERT(Offset <= size() && (Count == DynamicSpanExtent || Offset + Count <= size()));
				return {m_data.beg + Offset, Count != DynamicSpanExtent ? Count : size() - Offset};
			}

			GAIA_NODISCARD constexpr span<element_kind, DynamicSpanExtent>
			subspan(size_type offset, size_type count = DynamicSpanExtent) const {
				GAIA_ASSERT(offset <= size() && (count == DynamicSpanExtent || offset + count <= size()));
				return {m_data.beg + offset, count != DynamicSpanExtent ? count : size() - offset};
			}
		};

		template <typename T, size_t N>
		span(T (&)[N]) -> span<T, N>;

		template <typename T, size_t N>
		span(gaia::cnt::sarray<T, N>&) -> span<T, N>;

		template <typename T, size_t N>
		span(const gaia::cnt::sarray<T, N>&) -> span<const T, N>;

		template <typename Container>
		span(Container&) -> span<typename std::remove_reference<decltype(*detail::data(std::declval<Container&>()))>::type>;

		template <typename Container>
		span(const Container&) -> span<const typename Container::value_type>;

		template <span_size_type N, typename E, span_size_type S>
		constexpr auto get(span<E, S> s) -> decltype(s[N]) {
			return s[N];
		}
	} // namespace core
} // namespace gaia

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
