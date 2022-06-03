#pragma once

#include "../config/config.h"

#if GAIA_USE_STL_COMPATIBLE_CONTAINERS
	#include <iterator>
#else
	#include <cstddef>
	#include <type_traits>

namespace gaia {
	namespace utils {
		struct input_iterator_tag {};
		struct output_iterator_tag {};

		struct forward_iterator_tag: input_iterator_tag {};
		struct bidirectional_iterator_tag: forward_iterator_tag {};
		struct random_access_iterator_tag: bidirectional_iterator_tag {};

		namespace detail {

			template <class, class = void>
			struct iterator_traits_base {}; // empty for non-iterators

			template <class It>
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

			template <class T, bool = std::is_object_v<T>>
			struct iterator_traits_pointer_base {
				using iterator_category = random_access_iterator_tag;
				using value_type = std::remove_cv_t<T>;
				using difference_type = ptrdiff_t;
				using pointer = T*;
				using reference = T&;
			};

			//! Iterator traits for pointers to non-object
			template <class T>
			struct iterator_traits_pointer_base<T, false> {};

			//! Iterator traits for iterators
			template <class It>
			struct iterator_traits: iterator_traits_base<It> {};

			// Iterator traits for pointers
			template <class T>
			struct iterator_traits<T*>: iterator_traits_pointer_base<T> {};

			template <class It>
			using iterator_cat_t = typename iterator_traits<It>::iterator_category;

			template <class T, class = void>
			inline constexpr bool is_iterator_v = false;

			template <class T>
			inline constexpr bool is_iterator_v<T, std::void_t<iterator_cat_t<T>>> = true;

			template <class T>
			struct is_iterator: std::bool_constant<is_iterator_v<T>> {};

			template <class It>
			inline constexpr bool is_input_iter_v = std::is_convertible_v<iterator_cat_t<It>, input_iterator_tag>;

			template <class It>
			inline constexpr bool is_fwd_iter_v = std::is_convertible_v<iterator_cat_t<It>, forward_iterator_tag>;

			template <class It>
			inline constexpr bool is_bidi_iter_v = std::is_convertible_v<iterator_cat_t<It>, bidirectional_iterator_tag>;

			template <class It>
			inline constexpr bool is_random_iter_v = std::is_convertible_v<iterator_cat_t<It>, random_access_iterator_tag>;
		} // namespace detail

		template <class It>
		using iterator_ref_t = typename detail::iterator_traits<It>::reference;

		template <class It>
		using iterator_value_t = typename detail::iterator_traits<It>::value_type;

		template <class It>
		using iterator_diff_t = typename detail::iterator_traits<It>::difference_type;

		template <class... It>
		using common_diff_t = std::common_type_t<iterator_diff_t<It>...>;

		template <class It>
		constexpr iterator_diff_t<It> distance(It first, It last) {
			if constexpr (detail::is_random_iter_v<It>)
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
	} // namespace utils
} // namespace gaia
#endif
