#pragma once
#include "gaia/config/config.h"

#include <cstddef>
#include <type_traits>
#include <utility>

namespace gaia {
	namespace core {
		struct input_iterator_tag {};
		struct output_iterator_tag {};

		struct forward_iterator_tag: input_iterator_tag {};
		struct reverse_iterator_tag: input_iterator_tag {};
		struct bidirectional_iterator_tag: forward_iterator_tag {};
		struct random_access_iterator_tag: bidirectional_iterator_tag {};
		struct contiguous_iterator_tag: random_access_iterator_tag {};

		//! Random-access iterator that traverses an underlying iterator in reverse.
		//! The stored iterator denotes the element following the value returned by dereference.
		//! \tparam It Underlying random-access iterator type.
		//! \tparam Diff Signed type used for iterator distances.
		template <typename It, typename Diff>
		class reverse_iterator {
		public:
			//! Underlying iterator type.
			using iterator_type = It;
			//! Element type returned by dereference.
			using value_type = std::remove_cv_t<std::remove_reference_t<decltype(*std::declval<It&>())>>;
			//! Mutable pointer type when the underlying iterator is a pointer.
			using pointer = std::conditional_t<std::is_pointer_v<It>, It, void>;
			//! Reference or proxy type returned by dereference.
			using reference = decltype(*std::declval<It&>());
			//! Signed iterator distance type.
			using difference_type = Diff;
			//! Iterator category exposed by the adapter.
			using iterator_category = random_access_iterator_tag;

		private:
			It m_current{};

		public:
			constexpr reverse_iterator() = default;

			//! Constructs a reverse iterator from its forward base iterator.
			//! \param current Iterator following the value returned by dereference.
			constexpr explicit reverse_iterator(It current): m_current(current) {}

			//! Returns the underlying forward iterator.
			//! \return Iterator following the value returned by dereference.
			GAIA_NODISCARD constexpr It base() const {
				return m_current;
			}

			//! Accesses the current element.
			//! \return Reference or proxy for the current element.
			GAIA_NODISCARD constexpr decltype(auto) operator*() const {
				auto current = m_current;
				--current;
				return *current;
			}

			//! Accesses the current element through the underlying iterator.
			//! \return Pointer or proxy produced by the underlying iterator.
			GAIA_NODISCARD constexpr decltype(auto) operator->() const {
				auto current = m_current;
				--current;
				if constexpr (std::is_pointer_v<It>)
					return current;
				else
					return current.operator->();
			}

			//! Accesses a reverse-relative element.
			//! \param offset Number of reverse positions from the current element.
			//! \return Reference or proxy for the selected element.
			GAIA_NODISCARD constexpr decltype(auto) operator[](difference_type offset) const {
				return *(*this + offset);
			}

			//! Advances one position in reverse traversal order.
			//! \return This iterator after advancement.
			constexpr reverse_iterator& operator++() {
				--m_current;
				return *this;
			}

			//! Advances one position in reverse traversal order.
			//! \return Iterator value before advancement.
			constexpr reverse_iterator operator++(int) {
				auto tmp = *this;
				++*this;
				return tmp;
			}

			//! Moves one position opposite to reverse traversal order.
			//! \return This iterator after movement.
			constexpr reverse_iterator& operator--() {
				++m_current;
				return *this;
			}

			//! Moves one position opposite to reverse traversal order.
			//! \return Iterator value before movement.
			constexpr reverse_iterator operator--(int) {
				auto tmp = *this;
				--*this;
				return tmp;
			}

			//! Advances by a number of reverse positions.
			//! \param offset Number of positions to advance.
			//! \return This iterator after advancement.
			constexpr reverse_iterator& operator+=(difference_type offset) {
				m_current -= offset;
				return *this;
			}

			//! Moves opposite to reverse traversal by a number of positions.
			//! \param offset Number of positions to retreat.
			//! \return This iterator after movement.
			constexpr reverse_iterator& operator-=(difference_type offset) {
				m_current += offset;
				return *this;
			}

			//! Returns an iterator advanced in reverse traversal order.
			//! \param offset Number of positions to advance.
			//! \return Offset iterator.
			GAIA_NODISCARD constexpr reverse_iterator operator+(difference_type offset) const {
				auto tmp = *this;
				tmp += offset;
				return tmp;
			}

			//! Returns an iterator moved opposite to reverse traversal order.
			//! \param offset Number of positions to retreat.
			//! \return Offset iterator.
			GAIA_NODISCARD constexpr reverse_iterator operator-(difference_type offset) const {
				auto tmp = *this;
				tmp -= offset;
				return tmp;
			}

			//! Computes the reverse distance from another iterator.
			//! \param other Iterator used as the distance origin.
			//! \return Number of reverse increments from other to this iterator.
			GAIA_NODISCARD constexpr difference_type operator-(const reverse_iterator& other) const {
				return (difference_type)(other.m_current - m_current);
			}

			//! Checks whether two iterators have the same base position.
			//! \param other Iterator to compare.
			//! \return True when both iterators have the same base position.
			GAIA_NODISCARD constexpr bool operator==(const reverse_iterator& other) const {
				return m_current == other.m_current;
			}

			//! Checks whether two iterators have different base positions.
			//! \param other Iterator to compare.
			//! \return True when the iterators differ.
			GAIA_NODISCARD constexpr bool operator!=(const reverse_iterator& other) const {
				return !(*this == other);
			}

			//! Compares positions in reverse traversal order.
			//! \param other Iterator to compare.
			//! \return True when this iterator precedes other in reverse order.
			GAIA_NODISCARD constexpr bool operator<(const reverse_iterator& other) const {
				return other.m_current < m_current;
			}

			//! Compares positions in reverse traversal order.
			//! \param other Iterator to compare.
			//! \return True when this iterator follows other in reverse order.
			GAIA_NODISCARD constexpr bool operator>(const reverse_iterator& other) const {
				return other < *this;
			}

			//! Compares positions in reverse traversal order.
			//! \param other Iterator to compare.
			//! \return True when this iterator does not follow other in reverse order.
			GAIA_NODISCARD constexpr bool operator<=(const reverse_iterator& other) const {
				return !(other < *this);
			}

			//! Compares positions in reverse traversal order.
			//! \param other Iterator to compare.
			//! \return True when this iterator does not precede other in reverse order.
			GAIA_NODISCARD constexpr bool operator>=(const reverse_iterator& other) const {
				return !(*this < other);
			}
		};

		//! Returns a reverse iterator advanced by an offset.
		//! \tparam It Underlying random-access iterator type.
		//! \tparam Diff Signed type used for iterator distances.
		//! \param offset Number of positions to advance.
		//! \param it Iterator to offset.
		//! \return Offset iterator.
		template <typename It, typename Diff>
		GAIA_NODISCARD constexpr reverse_iterator<It, Diff> operator+(
				Diff offset, const reverse_iterator<It, Diff>& it) {
			return it + offset;
		}

		//! \cond INTERNAL
		namespace detail {
			template <typename, typename = void>
			struct iterator_traits_base {}; // empty for non-iterators

			template <typename It>
			struct iterator_traits_base<
					It, std::void_t<
									typename It::iterator_category, typename It::value_type, typename It::difference_type,
									typename It::pointer, typename It::reference>> {
				using iterator_category = typename It::iterator_category;
				using value_type = typename It::value_type;
				using difference_type = typename It::difference_type;
				using pointer = typename It::pointer;
				using reference = typename It::reference;
			};

			template <typename T, bool = std::is_object_v<T>>
			struct iterator_traits_pointer_base {
				using iterator_category = random_access_iterator_tag;
				using value_type = std::remove_cv_t<T>;
				using difference_type = std::ptrdiff_t;
				using pointer = T*;
				using reference = T&;
			};

			//! Iterator traits for pointers to non-object
			template <typename T>
			struct iterator_traits_pointer_base<T, false> {};

			//! Iterator traits for iterators
			template <typename It>
			struct iterator_traits: iterator_traits_base<It> {};

			// Iterator traits for pointers
			template <typename T>
			struct iterator_traits<T*>: iterator_traits_pointer_base<T> {};

			template <typename It>
			using iterator_cat_t = typename iterator_traits<It>::iterator_category;
		} // namespace detail
		//! \endcond

		//! Indicates whether T satisfies Gaia-ECS iterator trait requirements.
		//! \tparam T Type to inspect.
		template <typename T, typename = void>
		[[maybe_unused]] constexpr bool is_iterator_v = false;

		//! Specialization selected for types with a valid iterator category.
		//! \tparam T Iterator type.
		template <typename T>
		[[maybe_unused]] constexpr bool is_iterator_v<T, std::void_t<detail::iterator_cat_t<T>>> = true;

		template <typename T>
		struct is_iterator: std::bool_constant<is_iterator_v<T>> {};

		//! Indicates whether an iterator is an input iterator.
		//! \tparam It Iterator type.
		template <typename It>
		[[maybe_unused]] constexpr bool is_input_iter_v =
				std::is_convertible_v<detail::iterator_cat_t<It>, input_iterator_tag>;

		//! Indicates whether an iterator is a forward iterator.
		//! \tparam It Iterator type.
		template <typename It>
		[[maybe_unused]] constexpr bool is_fwd_iter_v =
				std::is_convertible_v<detail::iterator_cat_t<It>, forward_iterator_tag>;

		//! Indicates whether an iterator is a reverse iterator.
		//! \tparam It Iterator type.
		template <typename It>
		[[maybe_unused]] constexpr bool is_rev_iter_v =
				std::is_convertible_v<detail::iterator_cat_t<It>, reverse_iterator_tag>;

		//! Indicates whether an iterator is bidirectional.
		//! \tparam It Iterator type.
		template <typename It>
		[[maybe_unused]] constexpr bool is_bidi_iter_v =
				std::is_convertible_v<detail::iterator_cat_t<It>, bidirectional_iterator_tag>;

		//! Indicates whether an iterator supports random access.
		//! \tparam It Iterator type.
		template <typename It>
		[[maybe_unused]] constexpr bool is_random_iter_v =
				std::is_convertible_v<detail::iterator_cat_t<It>, random_access_iterator_tag>;

		//! Reference type yielded by an iterator.
		//! \tparam It Iterator type.
		template <typename It>
		using iterator_ref_t = typename detail::iterator_traits<It>::reference;

		//! Value type yielded by an iterator.
		//! \tparam It Iterator type.
		template <typename It>
		using iterator_value_t = typename detail::iterator_traits<It>::value_type;

		//! Difference type used by an iterator.
		//! \tparam It Iterator type.
		template <typename It>
		using iterator_diff_t = typename detail::iterator_traits<It>::difference_type;

		//! Common difference type shared by a set of iterators.
		//! \tparam It Iterator types.
		template <typename... It>
		using common_diff_t = std::common_type_t<iterator_diff_t<It>...>;

		//! Computes the number of increments from first to last.
		//! \tparam It Iterator type.
		//! \param first Beginning iterator.
		//! \param last Ending iterator.
		//! \return Distance from first to last in iterator difference units.
		template <typename It>
		constexpr iterator_diff_t<It> distance(It first, It last) {
			if constexpr (std::is_pointer_v<It> || is_random_iter_v<It>)
				return last - first;
			else {
				iterator_diff_t<It> offset{};
				while (first != last) {
					++first;
					++offset;
				}
				return offset;
			}
		}
	} // namespace core
} // namespace gaia
