#pragma once
#include "../config/config.h"

#if GAIA_USE_STL_COMPATIBLE_CONTAINERS
	#include <algorithm>
#endif
#include <tuple>
#include <type_traits>

#include "../containers/sarray.h"

namespace gaia {
	namespace utils {

		template <typename T>
		constexpr void swap(T& left, T& right) {
#if GAIA_USE_STL_COMPATIBLE_CONTAINERS
			return std::swap(left, right);
#else
			T tmp = std::move(left);
			left = std::move(right);
			right = std::move(tmp);
#endif
		}

		template <typename InputIt, typename Func>
		constexpr Func for_each(InputIt first, InputIt last, Func func) {
#if GAIA_USE_STL_COMPATIBLE_CONTAINERS
			return std::for_each(first, last, func);
#else
			for (; first != last; ++first)
				func(*first);
			return func;
#endif
		}

		template <class ForwardIt, class T>
		void fill(ForwardIt first, ForwardIt last, const T& value) {
#if GAIA_USE_STL_COMPATIBLE_CONTAINERS
			std::fill(first, last, value);
#else
			for (; first != last; ++first) {
				*first = value;
			}
#endif
		}

		template <class T>
		const T& get_min(const T& a, const T& b) {
			return (b < a) ? b : a;
		}

		template <class T>
		const T& get_max(const T& a, const T& b) {
			return (b > a) ? b : a;
		}

		//----------------------------------------------------------------------
		// Checking if a template arg is unique among the rest
		//----------------------------------------------------------------------

		template <typename...>
		inline constexpr auto is_unique = std::true_type{};

		template <typename T, typename... Rest>
		inline constexpr auto is_unique<T, Rest...> =
				std::bool_constant<(!std::is_same_v<T, Rest> && ...) && is_unique<Rest...>>{};

		namespace detail {
			template <typename T>
			struct type_identity {
				using type = T;
			};
		} // namespace detail

		template <typename T, typename... Ts>
		struct unique: detail::type_identity<T> {}; // TODO: In C++20 we could use std::type_identity

		template <typename... Ts, typename U, typename... Us>
		struct unique<std::tuple<Ts...>, U, Us...>:
				std::conditional_t<
						(std::is_same_v<U, Ts> || ...), unique<std::tuple<Ts...>, Us...>, unique<std::tuple<Ts..., U>, Us...>> {};

		template <typename... Ts>
		using unique_tuple = typename unique<std::tuple<>, Ts...>::type;

		//----------------------------------------------------------------------
		// Calculating total size of all types of tuple
		//----------------------------------------------------------------------

		template <typename... Args>
		constexpr unsigned get_args_size(std::tuple<Args...> const&) {
			return (sizeof(Args) + ...);
		}

		//----------------------------------------------------------------------
		// Function helpers
		//----------------------------------------------------------------------

		template <typename... Type>
		struct func_type_list {};

		template <typename Class, typename Ret, typename... Args>
		func_type_list<Args...> func_args(Ret (Class::*)(Args...) const);

		//----------------------------------------------------------------------
		// Type helpers
		//----------------------------------------------------------------------

		template <typename... Type>
		struct type_list {
			using types = type_list;
			static constexpr auto size = sizeof...(Type);
		};

		template <typename TypesA, typename TypesB>
		struct type_list_concat;

		template <typename... TypesA, typename... TypesB>
		struct type_list_concat<type_list<TypesA...>, type_list<TypesB...>> {
			using type = type_list<TypesA..., TypesB...>;
		};

		//----------------------------------------------------------------------
		// Compile - time for each
		//----------------------------------------------------------------------

		namespace detail {
			template <typename Func, auto... Is>
			constexpr void for_each_impl(Func func, std::index_sequence<Is...>) {
				(func(std::integral_constant<decltype(Is), Is>{}), ...);
			}

			template <typename Tuple, typename Func, auto... Is>
			void for_each_tuple_impl(Tuple&& tuple, Func func, std::index_sequence<Is...>) {
				(func(std::get<Is>(tuple)), ...);
			}
		} // namespace detail

		//! Compile-time for loop. Performs \tparam Iters iterations.
		//!
		//! Example:
		//! sarray<int, 10> arr = { ... };
		//! for_each<arr.size()>([&arr][auto i]) {
		//!    std::cout << arr[i] << std::endl;
		//! }
		template <auto Iters, typename Func>
		constexpr void for_each(Func func) {
			detail::for_each_impl(func, std::make_index_sequence<Iters>());
		}

		//! Compile-time for loop over containers.
		//! Iteration starts at \tparam FirstIdx and end at \tparam LastIdx
		//! (excluding) in increments of \tparam Inc.
		//!
		//! Example:
		//! sarray<int, 10> arr;
		//! for_each_ext<0, 10, 1>([&arr][auto i]) {
		//!    std::cout << arr[i] << std::endl;
		//! }
		//! print(69, "likes", 420.0f);
		template <auto FirstIdx, auto LastIdx, auto Inc, typename Func>
		constexpr void for_each_ext(Func func) {
			if constexpr (FirstIdx < LastIdx) {
				func(std::integral_constant<decltype(FirstIdx), FirstIdx>());
				for_each_ext<FirstIdx + Inc, LastIdx, Inc>(func);
			}
		}

		//! Compile-time for loop over parameter packs.
		//!
		//! Example:
		//! template<typename... Args>
		//! void print(const Args&... args) {
		//!  for_each_pack([][const auto& value]) {
		//!    std::cout << value << std::endl;
		//!  }
		//! }
		//! print(69, "likes", 420.0f);
		template <typename Func, typename... Args>
		constexpr void for_each_pack(Func func, Args&&... args) {
			(func(std::forward<Args>(args)), ...);
		}

		//! Compile-time for loop over tuples and other objects implementing
		//! tuple_size (sarray, std::pair etc).
		//!
		//! Example:
		//! for_each_tuple(const auto& value) {
		//!  std::cout << value << std::endl;
		//!  }, std::make(69, "likes", 420.0f);
		template <typename Tuple, typename Func>
		constexpr void for_each_tuple(Tuple&& tuple, Func func) {
			detail::for_each_tuple_impl(
					std::forward<Tuple>(tuple), func,
					std::make_index_sequence<std::tuple_size<std::remove_reference_t<Tuple>>::value>{});
		}

		//----------------------------------------------------------------------
		// Lookups
		//----------------------------------------------------------------------

		template <typename InputIt, typename T>
		constexpr InputIt find(InputIt first, InputIt last, const T& value) {
#if GAIA_USE_STL_COMPATIBLE_CONTAINERS
			return std::find(first, last, value);
#else
			for (; first != last; ++first) {
				if (*first == value) {
					return first;
				}
			}
			return last;
#endif
		}

		template <typename InputIt, typename Func>
		constexpr InputIt find_if(InputIt first, InputIt last, Func func) {
#if GAIA_USE_STL_COMPATIBLE_CONTAINERS
			return std::find_if(first, last, func);
#else
			for (; first != last; ++first) {
				if (func(*first)) {
					return first;
				}
			}
			return last;
#endif
		}

		template <typename InputIt, typename Func>
		constexpr InputIt find_if_not(InputIt first, InputIt last, Func func) {
#if GAIA_USE_STL_COMPATIBLE_CONTAINERS
			return std::find_if_not(first, last, func);
#else
			for (; first != last; ++first) {
				if (!func(*first)) {
					return first;
				}
			}
			return last;
#endif
		}

		//----------------------------------------------------------------------
		// Sorting
		//----------------------------------------------------------------------

		template <typename T>
		struct is_smaller {
			constexpr bool operator()(const T& lhs, const T& rhs) const {
				return lhs < rhs;
			}
		};

		template <typename T>
		struct is_greater {
			constexpr bool operator()(const T& lhs, const T& rhs) const {
				return lhs > rhs;
			}
		};

		template <typename T, typename Func>
		constexpr void swap_if(T& lhs, T& rhs, Func func) noexcept {
			T t = func(lhs, rhs) ? std::move(lhs) : std::move(rhs);
			rhs = func(lhs, rhs) ? std::move(rhs) : std::move(lhs);
			lhs = std::move(t);
		}

		namespace detail {
			template <typename Array, typename TSortFunc>
			constexpr void comb_sort_impl(Array& array_, TSortFunc func) noexcept {
				using size_type = typename Array::size_type;
				size_type gap = array_.size();
				bool swapped = false;
				while ((gap > size_type{1}) || swapped) {
					if (gap > size_type{1}) {
						gap = static_cast<size_type>(gap / 1.247330950103979);
					}
					swapped = false;
					for (size_type i = size_type{0}; gap + i < static_cast<size_type>(array_.size()); ++i) {
						if (func(array_[i], array_[i + gap])) {
							auto swap = array_[i];
							array_[i] = array_[i + gap];
							array_[i + gap] = swap;
							swapped = true;
						}
					}
				}
			}

			template <typename Container>
			int quick_sort_partition(Container& arr, int low, int high) {
				const auto& pivot = arr[high];
				int i = low - 1;
				for (int j = low; j <= high - 1; j++) {
					if (arr[j] < pivot)
						utils::swap(arr[++i], arr[j]);
				}
				utils::swap(arr[++i], arr[high]);
				return i;
			}

			template <typename Container>
			void quick_sort(Container& arr, int low, int high) {
				if (low >= high)
					return;
				auto pos = quick_sort_partition(arr, low, high);
				quick_sort(arr, low, pos - 1);
				quick_sort(arr, pos + 1, high);
			}
		} // namespace detail

		//! Compile-time sort.
		//! Implements a sorting network for \tparam N up to 8
		template <typename Container, typename TSortFunc>
		constexpr void sort_ct(Container& arr, TSortFunc func) noexcept {
			constexpr size_t NItems = std::tuple_size<Container>::value;
			if constexpr (NItems <= 1) {
				return;
			} else if constexpr (NItems == 2) {
				swap_if(arr[0], arr[1], func);
			} else if constexpr (NItems == 3) {
				swap_if(arr[1], arr[2], func);
				swap_if(arr[0], arr[2], func);
				swap_if(arr[0], arr[1], func);
			} else if constexpr (NItems == 4) {
				swap_if(arr[0], arr[1], func);
				swap_if(arr[2], arr[3], func);

				swap_if(arr[0], arr[2], func);
				swap_if(arr[1], arr[3], func);

				swap_if(arr[1], arr[2], func);
			} else if constexpr (NItems == 5) {
				swap_if(arr[0], arr[1], func);
				swap_if(arr[3], arr[4], func);

				swap_if(arr[2], arr[4], func);

				swap_if(arr[2], arr[3], func);
				swap_if(arr[1], arr[4], func);

				swap_if(arr[0], arr[3], func);

				swap_if(arr[0], arr[2], func);
				swap_if(arr[1], arr[3], func);

				swap_if(arr[1], arr[2], func);
			} else if constexpr (NItems == 6) {
				swap_if(arr[1], arr[2], func);
				swap_if(arr[4], arr[5], func);

				swap_if(arr[0], arr[2], func);
				swap_if(arr[3], arr[5], func);

				swap_if(arr[0], arr[1], func);
				swap_if(arr[3], arr[4], func);
				swap_if(arr[2], arr[5], func);

				swap_if(arr[0], arr[3], func);
				swap_if(arr[1], arr[4], func);

				swap_if(arr[2], arr[4], func);
				swap_if(arr[1], arr[3], func);

				swap_if(arr[2], arr[3], func);
			} else if constexpr (NItems == 7) {
				swap_if(arr[1], arr[2], func);
				swap_if(arr[3], arr[4], func);
				swap_if(arr[5], arr[6], func);

				swap_if(arr[0], arr[2], func);
				swap_if(arr[3], arr[5], func);
				swap_if(arr[4], arr[6], func);

				swap_if(arr[0], arr[1], func);
				swap_if(arr[4], arr[5], func);
				swap_if(arr[2], arr[6], func);

				swap_if(arr[0], arr[4], func);
				swap_if(arr[1], arr[5], func);

				swap_if(arr[0], arr[3], func);
				swap_if(arr[2], arr[5], func);

				swap_if(arr[1], arr[3], func);
				swap_if(arr[2], arr[4], func);

				swap_if(arr[2], arr[3], func);
			} else if constexpr (NItems == 8) {
				swap_if(arr[0], arr[1], func);
				swap_if(arr[2], arr[3], func);
				swap_if(arr[4], arr[5], func);
				swap_if(arr[6], arr[7], func);

				swap_if(arr[0], arr[2], func);
				swap_if(arr[1], arr[3], func);
				swap_if(arr[4], arr[6], func);
				swap_if(arr[5], arr[7], func);

				swap_if(arr[1], arr[2], func);
				swap_if(arr[5], arr[6], func);
				swap_if(arr[0], arr[4], func);
				swap_if(arr[3], arr[7], func);

				swap_if(arr[1], arr[5], func);
				swap_if(arr[2], arr[6], func);

				swap_if(arr[1], arr[4], func);
				swap_if(arr[3], arr[6], func);

				swap_if(arr[2], arr[4], func);
				swap_if(arr[3], arr[5], func);

				swap_if(arr[3], arr[4], func);
			} else {
#if GAIA_USE_STL_COMPATIBLE_CONTAINERS
				//! TODO: replace with std::sort for c++20
				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(4244)
				detail::comb_sort_impl(arr, func);
				GAIA_MSVC_WARNING_POP()
#else
				detail::comb_sort_impl(arr, func);
#endif
			}
		}

		//! Sorting including a sorting network for containers up to 8 elements in size.
		//! Ordinary sorting used for bigger containers.
		template <typename Container, typename TSortFunc>
		void sort(Container& arr, TSortFunc func) {
			if (arr.size() <= 1) {
				return;
			} else if (arr.size() == 2) {
				swap_if(arr[0], arr[1], func);
			} else if (arr.size() == 3) {
				swap_if(arr[1], arr[2], func);
				swap_if(arr[0], arr[2], func);
				swap_if(arr[0], arr[1], func);
			} else if (arr.size() == 4) {
				swap_if(arr[0], arr[1], func);
				swap_if(arr[2], arr[3], func);

				swap_if(arr[0], arr[2], func);
				swap_if(arr[1], arr[3], func);

				swap_if(arr[1], arr[2], func);
			} else if (arr.size() == 5) {
				swap_if(arr[0], arr[1], func);
				swap_if(arr[3], arr[4], func);

				swap_if(arr[2], arr[4], func);

				swap_if(arr[2], arr[3], func);
				swap_if(arr[1], arr[4], func);

				swap_if(arr[0], arr[3], func);

				swap_if(arr[0], arr[2], func);
				swap_if(arr[1], arr[3], func);

				swap_if(arr[1], arr[2], func);
			} else if (arr.size() == 6) {
				swap_if(arr[1], arr[2], func);
				swap_if(arr[4], arr[5], func);

				swap_if(arr[0], arr[2], func);
				swap_if(arr[3], arr[5], func);

				swap_if(arr[0], arr[1], func);
				swap_if(arr[3], arr[4], func);
				swap_if(arr[2], arr[5], func);

				swap_if(arr[0], arr[3], func);
				swap_if(arr[1], arr[4], func);

				swap_if(arr[2], arr[4], func);
				swap_if(arr[1], arr[3], func);

				swap_if(arr[2], arr[3], func);
			} else if (arr.size() == 7) {
				swap_if(arr[1], arr[2], func);
				swap_if(arr[3], arr[4], func);
				swap_if(arr[5], arr[6], func);

				swap_if(arr[0], arr[2], func);
				swap_if(arr[3], arr[5], func);
				swap_if(arr[4], arr[6], func);

				swap_if(arr[0], arr[1], func);
				swap_if(arr[4], arr[5], func);
				swap_if(arr[2], arr[6], func);

				swap_if(arr[0], arr[4], func);
				swap_if(arr[1], arr[5], func);

				swap_if(arr[0], arr[3], func);
				swap_if(arr[2], arr[5], func);

				swap_if(arr[1], arr[3], func);
				swap_if(arr[2], arr[4], func);

				swap_if(arr[2], arr[3], func);
			} else if (arr.size() == 8) {
				swap_if(arr[0], arr[1], func);
				swap_if(arr[2], arr[3], func);
				swap_if(arr[4], arr[5], func);
				swap_if(arr[6], arr[7], func);

				swap_if(arr[0], arr[2], func);
				swap_if(arr[1], arr[3], func);
				swap_if(arr[4], arr[6], func);
				swap_if(arr[5], arr[7], func);

				swap_if(arr[1], arr[2], func);
				swap_if(arr[5], arr[6], func);
				swap_if(arr[0], arr[4], func);
				swap_if(arr[3], arr[7], func);

				swap_if(arr[1], arr[5], func);
				swap_if(arr[2], arr[6], func);

				swap_if(arr[1], arr[4], func);
				swap_if(arr[3], arr[6], func);

				swap_if(arr[2], arr[4], func);
				swap_if(arr[3], arr[5], func);

				swap_if(arr[3], arr[4], func);
			} else if (arr.size() <= 32) {
				size_t i, j;
				size_t n = arr.size();
				for (i = 0; i < n - 1; i++) {
					for (j = 0; j < n - i - 1; j++) {
						if (arr[j] > arr[j + 1])
							utils::swap(arr[j], arr[j + 1]);
					}
				}
			} else {
#if GAIA_USE_STL_COMPATIBLE_CONTAINERS
				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(4244)
				std::sort(arr.begin(), arr.end(), func);
				GAIA_MSVC_WARNING_POP()
#else
				const int n = (int)arr.size();
				detail::quick_sort(arr, 0, n - 1);

#endif
			}
		}

		// The DoNotOptimize(...) function can be used to prevent a value or
// expression from being optimized away by the compiler. This function is
// intended to add little to no overhead.
// See: https://youtu.be/nXaxk27zwlk?t=2441
#if GAIA_HAS_NO_INLINE_ASSEMBLY
		template <class T>
		inline GAIA_FORCEINLINE void DoNotOptimize(T const& value) {
			asm volatile("" : : "r,m"(value) : "memory");
		}

		template <class T>
		inline GAIA_FORCEINLINE void DoNotOptimize(T& value) {
	#if defined(__clang__)
			asm volatile("" : "+r,m"(value) : : "memory");
	#else
			asm volatile("" : "+m,r"(value) : : "memory");
	#endif
		}
#else
		namespace internal {
			inline GAIA_FORCEINLINE void UseCharPointer(char const volatile* var) {
				(void)var;
			}
		} // namespace internal

	#if defined(_MSC_VER)
		template <class T>
		inline GAIA_FORCEINLINE void DoNotOptimize(T const& value) {
			internal::UseCharPointer(&reinterpret_cast<char const volatile&>(value));
			_ReadWriteBarrier();
		}
	#else
		template <class T>
		inline GAIA_FORCEINLINE void DoNotOptimize(T const& value) {
			internal::UseCharPointer(&reinterpret_cast<char const volatile&>(value));
		}
	#endif
#endif

	} // namespace utils
} // namespace gaia
