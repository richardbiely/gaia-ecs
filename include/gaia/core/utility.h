#pragma once
#include "../config/config.h"

#include <cstdio>
#include <tuple>
#include <type_traits>
#include <utility>

#if GAIA_PLATFORM_WINDOWS
	#include <malloc.h>
#else
	#include <alloca.h>
#endif

#include "iterator.h"

namespace gaia {
	constexpr uint32_t BadIndex = uint32_t(-1);

#if GAIA_COMPILER_MSVC || GAIA_PLATFORM_WINDOWS
	#define GAIA_STRCPY(var, max_len, text)                                                                              \
		strncpy_s((var), (text), (size_t)-1);                                                                              \
		(void)max_len
	#define GAIA_STRFMT(var, max_len, fmt, ...) sprintf_s((var), (max_len), fmt, __VA_ARGS__)
#else
	#define GAIA_STRCPY(var, max_len, text)                                                                              \
		{                                                                                                                  \
			strncpy((var), (text), (max_len));                                                                               \
			(var)[(max_len) - 1] = 0;                                                                                        \
		}
	#define GAIA_STRFMT(var, max_len, fmt, ...) snprintf((var), (max_len), fmt, __VA_ARGS__)
#endif

	namespace core {
		namespace detail {
			template <class T>
			struct rem_rp {
				using type = std::remove_reference_t<std::remove_pointer_t<T>>;
			};

			template <typename T>
			struct is_mut:
					std::bool_constant<
							!std::is_const_v<typename rem_rp<T>::type> &&
							(std::is_pointer<T>::value || std::is_reference<T>::value)> {};

			template <typename, typename = size_t>
			struct is_complete: std::false_type {};

			template <typename T>
			struct is_complete<T, decltype(sizeof(T))>: std::true_type {};

			template <typename C>
			constexpr auto size(C& c) noexcept -> decltype(c.size()) {
				return c.size();
			}
			template <typename C>
			constexpr auto size(const C& c) noexcept -> decltype(c.size()) {
				return c.size();
			}
			template <typename T, auto N>
			constexpr auto size(const T (&)[N]) noexcept {
				return N;
			}

			template <typename C>
			constexpr auto data(C& c) noexcept -> decltype(c.data()) {
				return c.data();
			}
			template <typename C>
			constexpr auto data(const C& c) noexcept -> decltype(c.data()) {
				return c.data();
			}
			template <typename T, auto N>
			constexpr T* data(T (&array)[N]) noexcept {
				return array;
			}
			template <typename E>
			constexpr const E* data(std::initializer_list<E> il) noexcept {
				return il.begin();
			}
		} // namespace detail

		struct zero_t {
			explicit constexpr zero_t() = default;
		};
		inline constexpr zero_t zero{};

		template <class T>
		using rem_rp_t = typename detail::rem_rp<T>::type;

		template <typename T>
		inline constexpr bool is_mut_v = detail::is_mut<T>::value;

		template <class T>
		using raw_t = typename std::decay_t<std::remove_pointer_t<T>>;

		template <typename T>
		inline constexpr bool is_raw_v = std::is_same_v<T, raw_t<T>> && !std::is_array_v<T>;

		template <typename T>
		inline constexpr bool is_complete_v = detail::is_complete<T>::value;

		//! Obtains the actual address of the object \param obj or function arg, even in presence of overloaded operator&.
		template <typename T>
		constexpr T* addressof(T& obj) noexcept {
			return &obj;
		}

		//! Rvalue overload is deleted to prevent taking the address of const rvalues.
		template <class T>
		const T* addressof(const T&&) = delete;

		template <typename T>
		struct lock_scope {
			T& m_ctx;

			lock_scope(T& ctx): m_ctx(ctx) {
				ctx.lock();
			}
			~lock_scope() {
				m_ctx.unlock();
			}

			lock_scope(const lock_scope&) = delete;
			lock_scope& operator=(const lock_scope&) = delete;
			lock_scope(lock_scope&&) = delete;
			lock_scope& operator=(lock_scope&&) = delete;
		};

		//----------------------------------------------------------------------
		// Container identification
		//----------------------------------------------------------------------

		template <typename, typename = void>
		struct has_data_size: std::false_type {};
		template <typename T>
		struct has_data_size<
				T, std::void_t< //
							 decltype(detail::data(std::declval<T>())), //
							 decltype(detail::size(std::declval<T>())) //
							 >>: std::true_type {};

		template <typename, typename = void>
		struct has_size_begin_end: std::false_type {};
		template <typename T>
		struct has_size_begin_end<
				T, std::void_t< //
							 decltype(std::declval<T>().begin()), //
							 decltype(std::declval<T>().end()), //
							 decltype(detail::size(std::declval<T>())) //
							 >>: std::true_type {};

		//----------------------------------------------------------------------
		// Bit-byte conversion
		//----------------------------------------------------------------------

		template <typename T>
		constexpr T as_bits(T value) {
			static_assert(std::is_integral_v<T>);
			return value * (T)8;
		}

		template <typename T>
		constexpr T as_bytes(T value) {
			static_assert(std::is_integral_v<T>);
			return value / (T)8;
		}

		//----------------------------------------------------------------------
		// Memory size helpers
		//----------------------------------------------------------------------

		template <typename T>
		constexpr uint32_t count_bits(T number) {
			uint32_t bits_needed = 0;
			while (number > 0U) {
				number >>= 1U;
				++bits_needed;
			}
			return bits_needed;
		}

		template <typename T>
		constexpr bool is_pow2(T number) {
			static_assert(std::is_integral<T>::value, "is_pow2 must be used with integer types");

			return (number & (number - 1)) == 0;
		}

		template <typename T>
		constexpr T closest_pow2(T number) {
			static_assert(std::is_integral<T>::value, "closest_pow2 must be used with integer types");

			if (is_pow2(number))
				return number;

			// Collapse all bits below the highest set bit
			number |= number >> 1;
			number |= number >> 2;
			number |= number >> 4;
			if constexpr (sizeof(T) > 1)
				number |= number >> 8;
			if constexpr (sizeof(T) > 2)
				number |= number >> 16;
			if constexpr (sizeof(T) > 4)
				number |= number >> 32;

			// The result is now one less than the next power of two, so shift back
			return number - (number >> 1);
		}

		//----------------------------------------------------------------------
		// Element construction / destruction
		//----------------------------------------------------------------------

		//! Constructs an object of type \tparam T in the uninitialized storage at the memory address \param pData.
		template <typename T>
		void call_ctor_raw(T* pData) {
			GAIA_ASSERT(pData != nullptr);
			(void)::new (const_cast<void*>(static_cast<const volatile void*>(core::addressof(*pData)))) T;
		}

		//! Constructs \param cnt objects of type \tparam T in the uninitialized storage at the memory address \param pData.
		template <typename T>
		void call_ctor_raw_n(T* pData, size_t cnt) {
			GAIA_ASSERT(pData != nullptr);
			for (size_t i = 0; i < cnt; ++i) {
				auto* ptr = pData + i;
				(void)::new (const_cast<void*>(static_cast<const volatile void*>(core::addressof(*ptr)))) T;
			}
		}

		//! Value-constructs an object of type \tparam T in the uninitialized storage at the memory address \param pData.
		template <typename T>
		void call_ctor_val(T* pData) {
			GAIA_ASSERT(pData != nullptr);
			(void)::new (const_cast<void*>(static_cast<const volatile void*>(core::addressof(*pData)))) T();
		}

		//! Value-constructs \param cnt objects of type \tparam T in the uninitialized storage at the memory address \param
		//! pData.
		template <typename T>
		void call_ctor_val_n(T* pData, size_t cnt) {
			GAIA_ASSERT(pData != nullptr);
			for (size_t i = 0; i < cnt; ++i) {
				auto* ptr = pData + i;
				(void)::new (const_cast<void*>(static_cast<const volatile void*>(core::addressof(*ptr)))) T();
			}
		}

		//! Constructs an object of type \tparam T in at the memory address \param pData.
		template <typename T>
		void call_ctor(T* pData) {
			GAIA_ASSERT(pData != nullptr);
			if constexpr (!std::is_trivially_constructible_v<T>) {
				(void)::new (pData) T();
			}
		}

		//! Constructs \param cnt objects of type \tparam T starting at the memory address \param pData.
		template <typename T>
		void call_ctor_n(T* pData, size_t cnt) {
			GAIA_ASSERT(pData != nullptr);
			if constexpr (!std::is_trivially_constructible_v<T>) {
				for (size_t i = 0; i < cnt; ++i)
					(void)::new (pData + i) T();
			}
		}

		template <typename T, typename... Args>
		void call_ctor(T* pData, Args&&... args) {
			GAIA_ASSERT(pData != nullptr);
			if constexpr (std::is_constructible_v<T, Args...>)
				(void)::new (pData) T(GAIA_FWD(args)...);
			else
				(void)::new (pData) T{GAIA_FWD(args)...};
		}

		//! Constructs an object of type \tparam T at the memory address \param pData.
		template <typename T>
		void call_dtor(T* pData) {
			GAIA_ASSERT(pData != nullptr);
			if constexpr (!std::is_trivially_destructible_v<T>) {
				pData->~T();
			}
		}

		//! Constructs \param cnt objects of type \tparam T starting at the memory address \param pData.
		template <typename T>
		void call_dtor_n(T* pData, size_t cnt) {
			GAIA_ASSERT(pData != nullptr);
			if constexpr (!std::is_trivially_destructible_v<T>) {
				for (size_t i = 0; i < cnt; ++i)
					pData[i].~T();
			}
		}

		//----------------------------------------------------------------------
		// Element swapping
		//----------------------------------------------------------------------

		template <typename T>
		constexpr void swap(T& left, T& right) {
			T tmp = GAIA_MOV(left);
			left = GAIA_MOV(right);
			right = GAIA_MOV(tmp);
		}

		template <typename T, typename TCmpFunc>
		constexpr void swap_if(T* c, size_t lhs, size_t rhs, TCmpFunc cmpFunc) noexcept {
			if (!cmpFunc(c[lhs], c[rhs]))
				core::swap(c[lhs], c[rhs]);
		}

		template <typename T, typename TCmpFunc>
		constexpr void swap_if(T& lhs, T& rhs, TCmpFunc cmpFunc) noexcept {
			if (!cmpFunc(lhs, rhs))
				core::swap(lhs, rhs);
		}

		template <typename T, typename TCmpFunc>
		constexpr void swap_if_not(T& lhs, T& rhs, TCmpFunc cmpFunc) noexcept {
			if (cmpFunc(lhs, rhs))
				core::swap(lhs, rhs);
		}

		template <typename T, typename TCmpFunc, typename TSwapFunc>
		constexpr void try_swap_if(T* c, uint32_t lhs, uint32_t rhs, TCmpFunc cmpFunc, TSwapFunc swapFunc) noexcept {
			if (!cmpFunc(c[lhs], c[rhs]))
				swapFunc(lhs, rhs);
		}

		template <typename C, typename TCmpFunc, typename TSwapFunc>
		constexpr void try_swap_if(
				C& c, typename C::size_type lhs, typename C::size_type rhs, TCmpFunc cmpFunc, TSwapFunc swapFunc) noexcept {
			if (!cmpFunc(c[lhs], c[rhs]))
				swapFunc(lhs, rhs);
		}

		template <typename C, typename TCmpFunc, typename TSwapFunc>
		constexpr void try_swap_if_not(
				C& c, typename C::size_type lhs, typename C::size_type rhs, TCmpFunc cmpFunc, TSwapFunc swapFunc) noexcept {
			if (cmpFunc(c[lhs], c[rhs]))
				swapFunc(lhs, rhs);
		}

		//----------------------------------------------------------------------
		// Value filling
		//----------------------------------------------------------------------

		template <class ForwardIt, class T>
		constexpr void fill(ForwardIt first, ForwardIt last, const T& value) {
			for (; first != last; ++first) {
				*first = value;
			}
		}

		//----------------------------------------------------------------------
		// Value range checking
		//----------------------------------------------------------------------

		template <class T>
		constexpr const T& get_min(const T& a, const T& b) {
			return (b < a) ? b : a;
		}

		template <class T>
		constexpr const T& get_max(const T& a, const T& b) {
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
		constexpr unsigned get_args_size(std::tuple<Args...> const& /*no_name*/) {
			return (sizeof(Args) + ...);
		}

		//----------------------------------------------------------------------
		// Function arguments type checks
		//----------------------------------------------------------------------

		template <typename... Type>
		struct func_type_list {};

		template <typename Class, typename Ret, typename... Args>
		func_type_list<Args...> func_args(Ret (Class::*)(Args...) const);

		//----------------------------------------------------------------------
		// Member function checks
		//----------------------------------------------------------------------

#if __cpp_concepts
	#define GAIA_DEFINE_HAS_MEMBER_FUNC(function_name)                                                                   \
		template <typename T, typename... Args>                                                                            \
		concept has_mfunc_check_##function_name = requires(T&& t, Args&&... args) { t.function_name(GAIA_FWD(args)...); }; \
                                                                                                                       \
		template <typename T, typename... Args>                                                                            \
		struct has_func_##function_name {                                                                                  \
			static constexpr bool value = has_mfunc_check_##function_name<T, Args...>;                                       \
		}
#else
		namespace detail {
			template <class Default, class AlwaysVoid, template <class...> class Op, class... Args>
			struct member_func_checker {
				using value_t = std::false_type;
				using type = Default;
			};

			template <class Default, template <class...> class Op, class... Args>
			struct member_func_checker<Default, std::void_t<Op<Args...>>, Op, Args...> {
				using value_t = std::true_type;
				using type = Op<Args...>;
			};

			struct member_func_none {
				~member_func_none() = delete;
				member_func_none(member_func_none const&) = delete;
				member_func_none(member_func_none&&) = delete;
				void operator=(member_func_none const&) = delete;
				void operator=(member_func_none&&) = delete;
			};
		} // namespace detail

		template <template <class...> class Op, typename... Args>
		using has_mfunc = typename detail::member_func_checker<detail::member_func_none, void, Op, Args...>::value_t;

	#define GAIA_DEFINE_HAS_MEMBER_FUNC(function_name)                                                                   \
		template <typename T, typename... Args>                                                                            \
		using has_mfunc_check_##function_name = decltype(std::declval<T>().function_name(std::declval<Args>()...));        \
                                                                                                                       \
		template <typename T, typename... Args>                                                                            \
		struct has_func_##function_name {                                                                                  \
			static constexpr bool value = gaia::core::has_mfunc<has_mfunc_check_##function_name, T, Args...>::value;         \
		}
#endif

		GAIA_DEFINE_HAS_MEMBER_FUNC(find);
		GAIA_DEFINE_HAS_MEMBER_FUNC(find_if);
		GAIA_DEFINE_HAS_MEMBER_FUNC(find_if_not);

		//----------------------------------------------------------------------
		// Special function checks
		//----------------------------------------------------------------------

		namespace detail {
			template <typename T>
			constexpr auto has_mfunc_equals_check(int)
					-> decltype(std::declval<T>().operator==(std::declval<T>()), std::true_type{});
			template <typename T, typename... Args>
			constexpr std::false_type has_mfunc_equals_check(...);

			template <typename T>
			constexpr auto has_ffunc_equals_check(int)
					-> decltype(operator==(std::declval<T>(), std::declval<T>()), std::true_type{});
			template <typename T, typename... Args>
			constexpr std::false_type has_ffunc_equals_check(...);
		} // namespace detail

		template <typename T>
		struct has_func_equals {
			static constexpr bool value = decltype(detail::has_mfunc_equals_check<T>(0))::value;
		};

		template <typename T>
		struct has_ffunc_equals {
			static constexpr bool value = decltype(detail::has_ffunc_equals_check<T>(0))::value;
		};

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
		// Looping
		//----------------------------------------------------------------------

		namespace detail {
			template <auto FirstIdx, auto Iters, typename Func, auto... Is>
			constexpr void each_impl(Func func, std::integer_sequence<decltype(Iters), Is...> /*no_name*/) {
				if constexpr ((std::is_invocable_v<Func&&, std::integral_constant<decltype(Is), Is>> && ...))
					(func(std::integral_constant<decltype(Is), FirstIdx + Is>{}), ...);
				else
					(((void)Is, func()), ...);
			}

			template <auto FirstIdx, typename Tuple, typename Func, auto... Is>
			void each_tuple_impl(Func func, std::integer_sequence<decltype(FirstIdx), Is...> /*no_name*/) {
				if constexpr ((std::is_invocable_v<
													 Func&&, decltype(std::tuple_element_t<FirstIdx + Is, Tuple>{}),
													 std::integral_constant<decltype(FirstIdx), Is>> &&
											 ...))
					// func(Args&& arg, uint32_t idx)
					(func(
							 std::tuple_element_t<FirstIdx + Is, Tuple>{},
							 std::integral_constant<decltype(FirstIdx), FirstIdx + Is>{}),
					 ...);
				else
					// func(Args&& arg)
					(func(std::tuple_element_t<FirstIdx + Is, Tuple>{}), ...);
			}

			template <auto FirstIdx, typename Tuple, typename Func, auto... Is>
			void each_tuple_impl(Tuple&& tuple, Func func, std::integer_sequence<decltype(FirstIdx), Is...> /*no_name*/) {
				if constexpr ((std::is_invocable_v<
													 Func&&, decltype(std::get<FirstIdx + Is>(tuple)),
													 std::integral_constant<decltype(FirstIdx), Is>> &&
											 ...))
					// func(Args&& arg, uint32_t idx)
					(func(std::get<FirstIdx + Is>(tuple), std::integral_constant<decltype(FirstIdx), FirstIdx + Is>{}), ...);
				else
					// func(Args&& arg)
					(func(std::get<FirstIdx + Is>(tuple)), ...);
			}
		} // namespace detail

		//! Compile-time for loop. Performs \tparam Iters iterations.
		//!
		//! Example 1 (index argument):
		//! sarray<int, 10> arr = { ... };
		//! each<arr.size()>([&arr](auto i) {
		//!    GAIA_LOG_N("%d", i);
		//! });
		//!
		//! Example 2 (no index argument):
		//! uint32_t cnt = 0;
		//! each<10>([&cnt]() {
		//!    GAIA_LOG_N("Invocation number: %u", cnt++);
		//! });
		template <auto Iters, typename Func>
		constexpr void each(Func func) {
			using TIters = decltype(Iters);
			constexpr TIters First = 0;
			detail::each_impl<First, Iters, Func>(func, std::make_integer_sequence<TIters, Iters>());
		}

		//! Compile-time for loop with adjustable range.
		//! Iteration starts at \tparam FirstIdx and ends at \tparam LastIdx (excluding).
		//!
		//! Example 1 (index argument):
		//! sarray<int, 10> arr;
		//! each_ext<0, 10>([&arr](auto i) {
		//!    GAIA_LOG_N("%d", i);
		//! });
		//!
		//! Example 2 (no argument):
		//! uint32_t cnt = 0;
		//! each_ext<0, 10>([&cnt]() {
		//!    GAIA_LOG_N("Invocation number: %u", cnt++);
		//! });
		template <auto FirstIdx, auto LastIdx, typename Func>
		constexpr void each_ext(Func func) {
			static_assert(LastIdx >= FirstIdx);
			const auto Iters = LastIdx - FirstIdx;
			detail::each_impl<FirstIdx, Iters, Func>(func, std::make_integer_sequence<decltype(Iters), Iters>());
		}

		//! Compile-time for loop with adjustable range and iteration size.
		//! Iteration starts at \tparam FirstIdx and ends at \tparam LastIdx
		//! (excluding) at increments of \tparam Inc.
		//!
		//! Example 1 (index argument):
		//! sarray<int, 10> arr;
		//! each_ext<0, 10, 2>([&arr](auto i) {
		//!    GAIA_LOG_N("%d", i);
		//! });
		//!
		//! Example 2 (no argument):
		//! uint32_t cnt = 0;
		//! each_ext<0, 10, 2>([&cnt]() {
		//!    GAIA_LOG_N("Invocation number: %u", cnt++);
		//! });
		template <auto FirstIdx, auto LastIdx, auto Inc, typename Func>
		constexpr void each_ext(Func func) {
			if constexpr (FirstIdx < LastIdx) {
				if constexpr (std::is_invocable_v<Func&&, std::integral_constant<decltype(FirstIdx), FirstIdx>>)
					func(std::integral_constant<decltype(FirstIdx), FirstIdx>());
				else
					func();

				each_ext<FirstIdx + Inc, LastIdx, Inc>(func);
			}
		}

		//! Compile-time for loop over parameter packs.
		//!
		//! Example:
		//! template<typename... Args>
		//! void print(const Args&... args) {
		//!  each_pack([](const auto& value) {
		//!    std::cout << value << std::endl;
		//!  });
		//! }
		//! print(69, "likes", 420.0f);
		template <typename Func, typename... Args>
		constexpr void each_pack(Func func, Args&&... args) {
			(func(GAIA_FWD(args)), ...);
		}

		//! Compile-time for loop over tuples and other objects implementing
		//! tuple_size (sarray, std::pair etc).
		//!
		//! Example:
		//! each_tuple(
		//!		std::make_tuple(69, "likes", 420.0f),
		//!		[](const auto& value) {
		//! 		std::cout << value << std::endl;
		//! 	});
		//! Output:
		//! 69
		//! likes
		//! 420.0f
		template <typename Tuple, typename Func>
		constexpr void each_tuple(Tuple&& tuple, Func func) {
			using TTSize = uint32_t;
			constexpr auto TSize = (TTSize)std::tuple_size<std::remove_reference_t<Tuple>>::value;
			detail::each_tuple_impl<(TTSize)0>(GAIA_FWD(tuple), func, std::make_integer_sequence<TTSize, TSize>{});
		}

		//! Compile-time for loop over tuples and other objects implementing
		//! tuple_size (sarray, std::pair etc).
		//! \warning This does not use a tuple instance, only the type.
		//!          Use for compile-time operations only.
		//!
		//! Example:
		//! each_tuple(
		//!		std::make_tuple(69, "likes", 420.0f),
		//!		[](const auto& value) {
		//! 		std::cout << value << std::endl;
		//! 	});
		//! Output:
		//! 0
		//! nullptr
		//! 0.0f
		template <typename Tuple, typename Func>
		constexpr void each_tuple(Func func) {
			using TTSize = uint32_t;
			constexpr auto TSize = (TTSize)std::tuple_size<std::remove_reference_t<Tuple>>::value;
			detail::each_tuple_impl<(TTSize)0, Tuple>(func, std::make_integer_sequence<TTSize, TSize>{});
		}

		//! Compile-time for loop over tuples and other objects implementing
		//! tuple_size (sarray, std::pair etc).
		//! Iteration starts at \tparam FirstIdx and ends at \tparam LastIdx (excluding).
		//!
		//! Example:
		//! each_tuple_ext<1, 3>(
		//!		std::make_tuple(69, "likes", 420.0f),
		//!		[](const auto& value) {
		//! 		std::cout << value << std::endl;
		//! 	});
		//! Output:
		//! likes
		//! 420.0f
		template <auto FirstIdx, auto LastIdx, typename Tuple, typename Func>
		constexpr void each_tuple_ext(Tuple&& tuple, Func func) {
			constexpr auto TSize = std::tuple_size<std::remove_reference_t<Tuple>>::value;
			static_assert(LastIdx >= FirstIdx);
			static_assert(LastIdx <= TSize);
			constexpr auto Iters = LastIdx - FirstIdx;
			detail::each_tuple_impl<FirstIdx>(GAIA_FWD(tuple), func, std::make_integer_sequence<decltype(FirstIdx), Iters>{});
		}

		//! Compile-time for loop over tuples and other objects implementing
		//! tuple_size (sarray, std::pair etc).
		//! Iteration starts at \tparam FirstIdx and ends at \tparam LastIdx (excluding).
		//! \warning This does not use a tuple instance, only the type.
		//!          Use for compile-time operations only.
		//!
		//! Example:
		//! each_tuple(
		//!		1, 3, std::make_tuple(69, "likes", 420.0f),
		//!		[](const auto& value) {
		//! 		std::cout << value << std::endl;
		//! 	});
		//! Output:
		//! nullptr
		//! 0.0f
		template <auto FirstIdx, auto LastIdx, typename Tuple, typename Func>
		constexpr void each_tuple_ext(Func func) {
			constexpr auto TSize = std::tuple_size<std::remove_reference_t<Tuple>>::value;
			static_assert(LastIdx >= FirstIdx);
			static_assert(LastIdx <= TSize);
			constexpr auto Iters = LastIdx - FirstIdx;
			detail::each_tuple_impl<FirstIdx, Tuple>(func, std::make_integer_sequence<decltype(FirstIdx), Iters>{});
		}

		template <typename InputIt, typename Func>
		constexpr Func each(InputIt first, InputIt last, Func func) {
			for (; first != last; ++first)
				func(*first);
			return func;
		}

		template <typename C, typename Func>
		constexpr auto each(const C& arr, Func func) {
			return each(arr.begin(), arr.end(), func);
		}

		//----------------------------------------------------------------------
		// Lookups
		//----------------------------------------------------------------------

		template <typename InputIt, typename T>
		constexpr InputIt find(InputIt first, InputIt last, const T& value) {
			if constexpr (std::is_pointer_v<InputIt>) {
				auto size = distance(first, last);
				for (decltype(size) i = 0; i < size; ++i) {
					if (first[i] == value)
						return &first[i];
				}
			} else if constexpr (is_random_iter_v<InputIt>) {
				auto size = distance(first, last);
				for (decltype(size) i = 0; i < size; ++i) {
					if (*(first[i]) == value)
						return first[i];
				}
			} else {
				for (; first != last; ++first) {
					if (*first == value)
						return first;
				}
			}
			return last;
		}

		template <typename C, typename V>
		constexpr auto find(const C& arr, const V& item) {
			if constexpr (has_func_find<C>::value)
				return arr.find(item);
			else
				return core::find(arr.begin(), arr.end(), item);
		}

		template <typename InputIt, typename Func>
		constexpr InputIt find_if(InputIt first, InputIt last, Func func) {
			if constexpr (std::is_pointer_v<InputIt>) {
				auto size = distance(first, last);
				for (decltype(size) i = 0; i < size; ++i) {
					if (func(first[i]))
						return &first[i];
				}
			} else if constexpr (std::is_same_v<typename InputIt::iterator_category, core::random_access_iterator_tag>) {
				auto size = distance(first, last);
				for (decltype(size) i = 0; i < size; ++i) {
					if (func(*(first[i])))
						return first[i];
				}
			} else {
				for (; first != last; ++first) {
					if (func(*first))
						return first;
				}
			}
			return last;
		}

		template <typename UnaryPredicate, typename C>
		constexpr auto find_if(const C& arr, UnaryPredicate predicate) {
			if constexpr (has_func_find_if<C, UnaryPredicate>::value)
				return arr.find_id(predicate);
			else
				return core::find_if(arr.begin(), arr.end(), predicate);
		}

		template <typename InputIt, typename Func>
		constexpr InputIt find_if_not(InputIt first, InputIt last, Func func) {
			if constexpr (std::is_pointer_v<InputIt>) {
				auto size = distance(first, last);
				for (decltype(size) i = 0; i < size; ++i) {
					if (!func(first[i]))
						return &first[i];
				}
			} else if constexpr (std::is_same_v<typename InputIt::iterator_category, core::random_access_iterator_tag>) {
				auto size = distance(first, last);
				for (decltype(size) i = 0; i < size; ++i) {
					if (!func(*(first[i])))
						return first[i];
				}
			} else {
				for (; first != last; ++first) {
					if (!func(*first))
						return first;
				}
			}
			return last;
		}

		template <typename UnaryPredicate, typename C>
		constexpr auto find_if_not(const C& arr, UnaryPredicate predicate) {
			if constexpr (has_func_find_if_not<C, UnaryPredicate>::value)
				return arr.find_if_not(predicate);
			else
				return core::find_if_not(arr.begin(), arr.end(), predicate);
		}

		//----------------------------------------------------------------------

		template <typename C, typename V>
		constexpr bool has(const C& arr, const V& item) {
			const auto it = find(arr, item);
			return it != arr.end();
		}

		template <typename UnaryPredicate, typename C>
		constexpr bool has_if(const C& arr, UnaryPredicate predicate) {
			const auto it = find_if(arr, predicate);
			return it != arr.end();
		}

		//----------------------------------------------------------------------

		template <typename C>
		constexpr auto get_index(const C& arr, typename C::const_reference item) {
			const auto it = find(arr, item);
			if (it == arr.end())
				return BadIndex;

			return (decltype(BadIndex))core::distance(arr.begin(), it);
		}

		template <typename C>
		constexpr auto get_index_unsafe(const C& arr, typename C::const_reference item) {
			return (decltype(BadIndex))core::distance(arr.begin(), find(arr, item));
		}

		template <typename UnaryPredicate, typename C>
		constexpr auto get_index_if(const C& arr, UnaryPredicate predicate) {
			const auto it = find_if(arr, predicate);
			if (it == arr.end())
				return BadIndex;

			return (decltype(BadIndex))core::distance(arr.begin(), it);
		}

		template <typename UnaryPredicate, typename C>
		constexpr auto get_index_if_unsafe(const C& arr, UnaryPredicate predicate) {
			return (decltype(BadIndex))core::distance(arr.begin(), find_if(arr, predicate));
		}

		//----------------------------------------------------------------------
		// Erasure
		//----------------------------------------------------------------------

		//! Replaces the item at \param idx in the array \param arr with the last item of the array if possible and
		//! removes its last item. Use when shifting of the entire array is not wanted. \warning If the item order is
		//! important and the size of the array changes after calling this function you need to sort the array.
		//! \warning Does not do bound checks. Undefined behavior when \param idx is out of bounds.
		template <typename C>
		void swap_erase_unsafe(C& arr, typename C::size_type idx) {
			GAIA_ASSERT(idx < arr.size());

			if (idx + 1 != arr.size())
				arr[idx] = arr[arr.size() - 1];

			arr.pop_back();
		}

		//! Replaces the item at \param idx in the array \param arr with the last item of the array if possible and
		//! removes its last item. Use when shifting of the entire array is not wanted. \warning If the item order is
		//! important and the size of the array changes after calling this function you need to sort the array.
		template <typename C>
		void swap_erase(C& arr, typename C::size_type idx) {
			if (idx >= arr.size())
				return;

			if (idx + 1 != arr.size())
				arr[idx] = arr[arr.size() - 1];

			arr.pop_back();
		}

		//----------------------------------------------------------------------
		// Comparison
		//----------------------------------------------------------------------

		template <typename T>
		struct equal_to {
			constexpr bool operator()(const T& lhs, const T& rhs) const {
				return lhs == rhs;
			}
		};

		template <typename T>
		struct is_smaller {
			constexpr bool operator()(const T& lhs, const T& rhs) const {
				return lhs < rhs;
			}
		};

		template <typename T>
		struct is_smaller_or_equal {
			constexpr bool operator()(const T& lhs, const T& rhs) const {
				return lhs <= rhs;
			}
		};

		template <typename T>
		struct is_greater {
			constexpr bool operator()(const T& lhs, const T& rhs) const {
				return lhs > rhs;
			}
		};

		//----------------------------------------------------------------------
		// Sorting
		//----------------------------------------------------------------------

		namespace detail {
			template <typename Container, typename TCmpFunc>
			int quick_sort_partition(Container& arr, int low, int high, TCmpFunc cmpFunc) {
				const auto& pivot = arr[(uint32_t)high];
				int i = low - 1;
				for (int j = low; j <= high - 1; ++j) {
					if (cmpFunc(arr[(uint32_t)j], pivot))
						core::swap(arr[(uint32_t)++i], arr[(uint32_t)j]);
				}
				core::swap(arr[(uint32_t)++i], arr[(uint32_t)high]);
				return i;
			}

			template <typename Container, typename TCmpFunc, typename TSwapFunc>
			int quick_sort_partition(Container& arr, int low, int high, TCmpFunc cmpFunc, TSwapFunc swapFunc) {
				const auto& pivot = arr[(uint32_t)high];
				int i = low - 1;
				for (int j = low; j <= high - 1; ++j) {
					if (cmpFunc(arr[(uint32_t)j], pivot))
						swapFunc((uint32_t)++i, (uint32_t)j);
				}
				swapFunc((uint32_t)++i, (uint32_t)high);
				return i;
			}

			template <typename T, typename TCmpFunc>
			bool sort_nwk(T* beg, T* end, TCmpFunc cmpFunc) {
				const auto n = (uint32_t)(end - beg);
				if (n <= 1) {
					// Nothing to sort with just one item
				} else if (n == 2) {
					swap_if(beg, 0, 1, cmpFunc);
				} else if (n == 3) {
					swap_if(beg, 1, 2, cmpFunc);
					swap_if(beg, 0, 2, cmpFunc);
					swap_if(beg, 0, 1, cmpFunc);
				} else if (n == 4) {
					swap_if(beg, 0, 1, cmpFunc);
					swap_if(beg, 2, 3, cmpFunc);

					swap_if(beg, 0, 2, cmpFunc);
					swap_if(beg, 1, 3, cmpFunc);

					swap_if(beg, 1, 2, cmpFunc);
				} else if (n == 5) {
					swap_if(beg, 0, 1, cmpFunc);
					swap_if(beg, 3, 4, cmpFunc);

					swap_if(beg, 2, 4, cmpFunc);

					swap_if(beg, 2, 3, cmpFunc);
					swap_if(beg, 1, 4, cmpFunc);

					swap_if(beg, 0, 3, cmpFunc);

					swap_if(beg, 0, 2, cmpFunc);
					swap_if(beg, 1, 3, cmpFunc);

					swap_if(beg, 1, 2, cmpFunc);
				} else if (n == 6) {
					swap_if(beg, 1, 2, cmpFunc);
					swap_if(beg, 4, 5, cmpFunc);

					swap_if(beg, 0, 2, cmpFunc);
					swap_if(beg, 3, 5, cmpFunc);

					swap_if(beg, 0, 1, cmpFunc);
					swap_if(beg, 3, 4, cmpFunc);
					swap_if(beg, 2, 5, cmpFunc);

					swap_if(beg, 0, 3, cmpFunc);
					swap_if(beg, 1, 4, cmpFunc);

					swap_if(beg, 2, 4, cmpFunc);
					swap_if(beg, 1, 3, cmpFunc);

					swap_if(beg, 2, 3, cmpFunc);
				} else if (n == 7) {
					swap_if(beg, 1, 2, cmpFunc);
					swap_if(beg, 3, 4, cmpFunc);
					swap_if(beg, 5, 6, cmpFunc);

					swap_if(beg, 0, 2, cmpFunc);
					swap_if(beg, 3, 5, cmpFunc);
					swap_if(beg, 4, 6, cmpFunc);

					swap_if(beg, 0, 1, cmpFunc);
					swap_if(beg, 4, 5, cmpFunc);
					swap_if(beg, 2, 6, cmpFunc);

					swap_if(beg, 0, 4, cmpFunc);
					swap_if(beg, 1, 5, cmpFunc);

					swap_if(beg, 0, 3, cmpFunc);
					swap_if(beg, 2, 5, cmpFunc);

					swap_if(beg, 1, 3, cmpFunc);
					swap_if(beg, 2, 4, cmpFunc);

					swap_if(beg, 2, 3, cmpFunc);
				} else if (n == 8) {
					swap_if(beg, 0, 1, cmpFunc);
					swap_if(beg, 2, 3, cmpFunc);
					swap_if(beg, 4, 5, cmpFunc);
					swap_if(beg, 6, 7, cmpFunc);

					swap_if(beg, 0, 2, cmpFunc);
					swap_if(beg, 1, 3, cmpFunc);
					swap_if(beg, 4, 6, cmpFunc);
					swap_if(beg, 5, 7, cmpFunc);

					swap_if(beg, 1, 2, cmpFunc);
					swap_if(beg, 5, 6, cmpFunc);
					swap_if(beg, 0, 4, cmpFunc);
					swap_if(beg, 3, 7, cmpFunc);

					swap_if(beg, 1, 5, cmpFunc);
					swap_if(beg, 2, 6, cmpFunc);

					swap_if(beg, 1, 4, cmpFunc);
					swap_if(beg, 3, 6, cmpFunc);

					swap_if(beg, 2, 4, cmpFunc);
					swap_if(beg, 3, 5, cmpFunc);

					swap_if(beg, 3, 4, cmpFunc);
				} else if (n == 9) {
					swap_if(beg, 0, 1, cmpFunc);
					swap_if(beg, 3, 4, cmpFunc);
					swap_if(beg, 6, 7, cmpFunc);
					swap_if(beg, 1, 2, cmpFunc);
					swap_if(beg, 4, 5, cmpFunc);
					swap_if(beg, 7, 8, cmpFunc);
					swap_if(beg, 0, 1, cmpFunc);
					swap_if(beg, 3, 4, cmpFunc);
					swap_if(beg, 6, 7, cmpFunc);
					swap_if(beg, 0, 3, cmpFunc);
					swap_if(beg, 3, 6, cmpFunc);
					swap_if(beg, 0, 3, cmpFunc);
					swap_if(beg, 1, 4, cmpFunc);
					swap_if(beg, 4, 7, cmpFunc);
					swap_if(beg, 1, 4, cmpFunc);
					swap_if(beg, 2, 5, cmpFunc);
					swap_if(beg, 5, 8, cmpFunc);
					swap_if(beg, 2, 5, cmpFunc);
					swap_if(beg, 1, 3, cmpFunc);
					swap_if(beg, 5, 7, cmpFunc);
					swap_if(beg, 2, 6, cmpFunc);
					swap_if(beg, 4, 6, cmpFunc);
					swap_if(beg, 2, 4, cmpFunc);
					swap_if(beg, 2, 3, cmpFunc);
					swap_if(beg, 5, 6, cmpFunc);
				} else if (n == 10) {
					swap_if(beg, 0, 8, cmpFunc);
					swap_if(beg, 1, 9, cmpFunc);
					swap_if(beg, 2, 7, cmpFunc);
					swap_if(beg, 3, 6, cmpFunc);
					swap_if(beg, 4, 5, cmpFunc);

					swap_if(beg, 0, 2, cmpFunc);
					swap_if(beg, 1, 3, cmpFunc);
					swap_if(beg, 4, 8, cmpFunc);
					swap_if(beg, 5, 7, cmpFunc);
					swap_if(beg, 6, 9, cmpFunc);

					swap_if(beg, 0, 4, cmpFunc);
					swap_if(beg, 1, 5, cmpFunc);
					swap_if(beg, 2, 6, cmpFunc);
					swap_if(beg, 3, 7, cmpFunc);

					swap_if(beg, 0, 1, cmpFunc);
					swap_if(beg, 2, 3, cmpFunc);
					swap_if(beg, 4, 6, cmpFunc);
					swap_if(beg, 5, 9, cmpFunc);
					swap_if(beg, 7, 8, cmpFunc);

					swap_if(beg, 1, 2, cmpFunc);
					swap_if(beg, 3, 5, cmpFunc);
					swap_if(beg, 6, 7, cmpFunc);
					swap_if(beg, 8, 9, cmpFunc);

					swap_if(beg, 2, 4, cmpFunc);
					swap_if(beg, 3, 6, cmpFunc);
					swap_if(beg, 5, 8, cmpFunc);

					swap_if(beg, 1, 2, cmpFunc);
					swap_if(beg, 3, 4, cmpFunc);
					swap_if(beg, 5, 6, cmpFunc);
					swap_if(beg, 7, 8, cmpFunc);
				} else if (n <= 32) {
					for (uint32_t i = 0; i < n - 1; ++i)
						for (uint32_t j = 0; j < n - i - 1; ++j)
							swap_if(beg, j, j + 1, cmpFunc);
				}

				return n <= 32;
			}

			template <typename T, typename TCmpFunc, typename TSwapFunc>
			bool sort_nwk(T* beg, T* end, TCmpFunc cmpFunc, TSwapFunc swapFunc) {
				const auto n = (uint32_t)(end - beg);
				if (n <= 1) {
					// Nothing to sort with just one item
				} else if (n == 2) {
					try_swap_if(beg, 0, 1, cmpFunc, swapFunc);
				} else if (n == 3) {
					try_swap_if(beg, 1, 2, cmpFunc, swapFunc);
					try_swap_if(beg, 0, 2, cmpFunc, swapFunc);
					try_swap_if(beg, 0, 1, cmpFunc, swapFunc);
				} else if (n == 4) {
					try_swap_if(beg, 0, 1, cmpFunc, swapFunc);
					try_swap_if(beg, 2, 3, cmpFunc, swapFunc);

					try_swap_if(beg, 0, 2, cmpFunc, swapFunc);
					try_swap_if(beg, 1, 3, cmpFunc, swapFunc);

					try_swap_if(beg, 1, 2, cmpFunc, swapFunc);
				} else if (n == 5) {
					try_swap_if(beg, 0, 1, cmpFunc, swapFunc);
					try_swap_if(beg, 3, 4, cmpFunc, swapFunc);

					try_swap_if(beg, 2, 4, cmpFunc, swapFunc);

					try_swap_if(beg, 2, 3, cmpFunc, swapFunc);
					try_swap_if(beg, 1, 4, cmpFunc, swapFunc);

					try_swap_if(beg, 0, 3, cmpFunc, swapFunc);

					try_swap_if(beg, 0, 2, cmpFunc, swapFunc);
					try_swap_if(beg, 1, 3, cmpFunc, swapFunc);

					try_swap_if(beg, 1, 2, cmpFunc, swapFunc);
				} else if (n == 6) {
					try_swap_if(beg, 1, 2, cmpFunc, swapFunc);
					try_swap_if(beg, 4, 5, cmpFunc, swapFunc);

					try_swap_if(beg, 0, 2, cmpFunc, swapFunc);
					try_swap_if(beg, 3, 5, cmpFunc, swapFunc);

					try_swap_if(beg, 0, 1, cmpFunc, swapFunc);
					try_swap_if(beg, 3, 4, cmpFunc, swapFunc);
					try_swap_if(beg, 2, 5, cmpFunc, swapFunc);

					try_swap_if(beg, 0, 3, cmpFunc, swapFunc);
					try_swap_if(beg, 1, 4, cmpFunc, swapFunc);

					try_swap_if(beg, 2, 4, cmpFunc, swapFunc);
					try_swap_if(beg, 1, 3, cmpFunc, swapFunc);

					try_swap_if(beg, 2, 3, cmpFunc, swapFunc);
				} else if (n == 7) {
					try_swap_if(beg, 1, 2, cmpFunc, swapFunc);
					try_swap_if(beg, 3, 4, cmpFunc, swapFunc);
					try_swap_if(beg, 5, 6, cmpFunc, swapFunc);

					try_swap_if(beg, 0, 2, cmpFunc, swapFunc);
					try_swap_if(beg, 3, 5, cmpFunc, swapFunc);
					try_swap_if(beg, 4, 6, cmpFunc, swapFunc);

					try_swap_if(beg, 0, 1, cmpFunc, swapFunc);
					try_swap_if(beg, 4, 5, cmpFunc, swapFunc);
					try_swap_if(beg, 2, 6, cmpFunc, swapFunc);

					try_swap_if(beg, 0, 4, cmpFunc, swapFunc);
					try_swap_if(beg, 1, 5, cmpFunc, swapFunc);

					try_swap_if(beg, 0, 3, cmpFunc, swapFunc);
					try_swap_if(beg, 2, 5, cmpFunc, swapFunc);

					try_swap_if(beg, 1, 3, cmpFunc, swapFunc);
					try_swap_if(beg, 2, 4, cmpFunc, swapFunc);

					try_swap_if(beg, 2, 3, cmpFunc, swapFunc);
				} else if (n == 8) {
					try_swap_if(beg, 0, 1, cmpFunc, swapFunc);
					try_swap_if(beg, 2, 3, cmpFunc, swapFunc);
					try_swap_if(beg, 4, 5, cmpFunc, swapFunc);
					try_swap_if(beg, 6, 7, cmpFunc, swapFunc);

					try_swap_if(beg, 0, 2, cmpFunc, swapFunc);
					try_swap_if(beg, 1, 3, cmpFunc, swapFunc);
					try_swap_if(beg, 4, 6, cmpFunc, swapFunc);
					try_swap_if(beg, 5, 7, cmpFunc, swapFunc);

					try_swap_if(beg, 1, 2, cmpFunc, swapFunc);
					try_swap_if(beg, 5, 6, cmpFunc, swapFunc);
					try_swap_if(beg, 0, 4, cmpFunc, swapFunc);
					try_swap_if(beg, 3, 7, cmpFunc, swapFunc);

					try_swap_if(beg, 1, 5, cmpFunc, swapFunc);
					try_swap_if(beg, 2, 6, cmpFunc, swapFunc);

					try_swap_if(beg, 1, 4, cmpFunc, swapFunc);
					try_swap_if(beg, 3, 6, cmpFunc, swapFunc);

					try_swap_if(beg, 2, 4, cmpFunc, swapFunc);
					try_swap_if(beg, 3, 5, cmpFunc, swapFunc);

					try_swap_if(beg, 3, 4, cmpFunc, swapFunc);
				} else if (n == 9) {
					try_swap_if(beg, 0, 1, cmpFunc, swapFunc);
					try_swap_if(beg, 3, 4, cmpFunc, swapFunc);
					try_swap_if(beg, 6, 7, cmpFunc, swapFunc);
					try_swap_if(beg, 1, 2, cmpFunc, swapFunc);
					try_swap_if(beg, 4, 5, cmpFunc, swapFunc);
					try_swap_if(beg, 7, 8, cmpFunc, swapFunc);
					try_swap_if(beg, 0, 1, cmpFunc, swapFunc);
					try_swap_if(beg, 3, 4, cmpFunc, swapFunc);
					try_swap_if(beg, 6, 7, cmpFunc, swapFunc);
					try_swap_if(beg, 0, 3, cmpFunc, swapFunc);
					try_swap_if(beg, 3, 6, cmpFunc, swapFunc);
					try_swap_if(beg, 0, 3, cmpFunc, swapFunc);
					try_swap_if(beg, 1, 4, cmpFunc, swapFunc);
					try_swap_if(beg, 4, 7, cmpFunc, swapFunc);
					try_swap_if(beg, 1, 4, cmpFunc, swapFunc);
					try_swap_if(beg, 2, 5, cmpFunc, swapFunc);
					try_swap_if(beg, 5, 8, cmpFunc, swapFunc);
					try_swap_if(beg, 2, 5, cmpFunc, swapFunc);
					try_swap_if(beg, 1, 3, cmpFunc, swapFunc);
					try_swap_if(beg, 5, 7, cmpFunc, swapFunc);
					try_swap_if(beg, 2, 6, cmpFunc, swapFunc);
					try_swap_if(beg, 4, 6, cmpFunc, swapFunc);
					try_swap_if(beg, 2, 4, cmpFunc, swapFunc);
					try_swap_if(beg, 2, 3, cmpFunc, swapFunc);
					try_swap_if(beg, 5, 6, cmpFunc, swapFunc);
				} else if (n == 10) {
					try_swap_if(beg, 0, 8, cmpFunc, swapFunc);
					try_swap_if(beg, 1, 9, cmpFunc, swapFunc);
					try_swap_if(beg, 2, 7, cmpFunc, swapFunc);
					try_swap_if(beg, 3, 6, cmpFunc, swapFunc);
					try_swap_if(beg, 4, 5, cmpFunc, swapFunc);

					try_swap_if(beg, 0, 2, cmpFunc, swapFunc);
					try_swap_if(beg, 1, 3, cmpFunc, swapFunc);
					try_swap_if(beg, 4, 8, cmpFunc, swapFunc);
					try_swap_if(beg, 5, 7, cmpFunc, swapFunc);
					try_swap_if(beg, 6, 9, cmpFunc, swapFunc);

					try_swap_if(beg, 0, 4, cmpFunc, swapFunc);
					try_swap_if(beg, 1, 5, cmpFunc, swapFunc);
					try_swap_if(beg, 2, 6, cmpFunc, swapFunc);
					try_swap_if(beg, 3, 7, cmpFunc, swapFunc);

					try_swap_if(beg, 0, 1, cmpFunc, swapFunc);
					try_swap_if(beg, 2, 3, cmpFunc, swapFunc);
					try_swap_if(beg, 4, 6, cmpFunc, swapFunc);
					try_swap_if(beg, 5, 9, cmpFunc, swapFunc);
					try_swap_if(beg, 7, 8, cmpFunc, swapFunc);

					try_swap_if(beg, 1, 2, cmpFunc, swapFunc);
					try_swap_if(beg, 3, 5, cmpFunc, swapFunc);
					try_swap_if(beg, 6, 7, cmpFunc, swapFunc);
					try_swap_if(beg, 8, 9, cmpFunc, swapFunc);

					try_swap_if(beg, 2, 4, cmpFunc, swapFunc);
					try_swap_if(beg, 3, 6, cmpFunc, swapFunc);
					try_swap_if(beg, 5, 8, cmpFunc, swapFunc);

					try_swap_if(beg, 1, 2, cmpFunc, swapFunc);
					try_swap_if(beg, 3, 4, cmpFunc, swapFunc);
					try_swap_if(beg, 5, 6, cmpFunc, swapFunc);
					try_swap_if(beg, 7, 8, cmpFunc, swapFunc);
				} else if (n <= 32) {
					for (uint32_t i = 0; i < n - 1; ++i)
						for (uint32_t j = 0; j < n - i - 1; ++j)
							try_swap_if(beg, j, j + 1, cmpFunc, swapFunc);
				}

				return n <= 32;
			}
		} // namespace detail

		template <typename Container, typename TCmpFunc>
		void quick_sort(Container& arr, int low, int high, TCmpFunc cmpFunc) {
			if (low >= high)
				return;
			auto pos = detail::quick_sort_partition(arr, low, high, cmpFunc);
			quick_sort(arr, low, pos - 1, cmpFunc);
			quick_sort(arr, pos + 1, high, cmpFunc);
		}

		template <typename Container, typename TCmpFunc, typename TSwapFunc>
		void quick_sort(Container& arr, int low, int high, TCmpFunc cmpFunc, TSwapFunc swapFunc) {
			if (low >= high)
				return;
			auto pos = detail::quick_sort_partition(arr, low, high, cmpFunc, swapFunc);
			quick_sort(arr, low, pos - 1, cmpFunc, swapFunc);
			quick_sort(arr, pos + 1, high, cmpFunc, swapFunc);
		}

		//! A special version of the quick sort algorithm.
		//! Instead of relying on recursion it allocates an acceleration structure on the stack.
		//! \tparam Container Container to sort
		//! \tparam TCmpFunc Comparison function
		//! \tparam TSwapFunc Swap function
		//! \param arr Container to sort
		//! \param low Low index of the array to sort
		//! \param high High index of the array to sort
		//! \param cmpFunc Comparison function
		//! \param swapFunc Swap function
		//! \param maxStackSize Maximum depth of the stack used for the acceleration structure.
		//! \warning If the input container is larger than \param maxStackSize the function might end up accessing memory
		//!          out-of-bounds. This would usually happen e.g. when sorting an already sorted array. This is because
		//!					 you can not easily predict the depth of the stack used by the algorithm.
		template <typename Container, typename TCmpFunc, typename TSwapFunc>
		void
		quick_sort_stack(Container& arr, int low, int high, TCmpFunc cmpFunc, TSwapFunc swapFunc, uint32_t maxStackSize) {
			GAIA_ASSERT(high < (int)arr.size() && "quick_sort_stack: 'high' has to be smaller than the container size.");
			GAIA_ASSERT(
					(uint32_t)arr.size() <= maxStackSize && "quick_sort_stack: used with too large input array. Stack overflow.");

			struct Range {
				int low, high;
			};

			auto* stack = (Range*)alloca(sizeof(Range) * maxStackSize);
			GAIA_ASSERT(stack != nullptr && "quick_sort_stack: failed to allocate stack memory.");
			uint32_t sp = 0; // stack pointer
			stack[sp++] = {low, high};

			auto median_of_three = [&](int a, int b, int c) {
				if (cmpFunc(arr[a], arr[b])) {
					if (cmpFunc(arr[b], arr[c]))
						return b;
					if (cmpFunc(arr[a], arr[c]))
						return c;
					return a;
				} else {
					if (cmpFunc(arr[a], arr[c]))
						return a;
					if (cmpFunc(arr[b], arr[c]))
						return c;
					return b;
				}
			};

			while (sp > 0) {
				const Range r = stack[--sp];
				if (r.low >= r.high)
					continue;

				// Use optimized sort for small partitions
				const auto size = r.high - r.low + 1;
				if (size <= 32) {
					auto* base = &arr[r.low];
					detail::sort_nwk(base, base + size, cmpFunc, [&](uint32_t i, uint32_t j) {
						swapFunc(r.low + i, r.low + j);
					});
					continue;
				}

				// Median-of-three pivot selection
				int mid = r.low + ((r.high - r.low) >> 1);
				int pivotIdx = median_of_three(r.low, mid, r.high);
				swapFunc(pivotIdx, r.high);

				const auto& pivot = arr[r.high];
				int i = r.low - 1;
				for (int j = r.low; j < r.high; ++j) {
					if (cmpFunc(arr[j], pivot))
						swapFunc(++i, j);
				}
				swapFunc(++i, r.high);

				// Push smaller partition first to reduce max stack usage
				if (i - 1 - r.low > r.high - (i + 1)) {
					if (r.low < i - 1)
						stack[sp++] = {r.low, i - 1};
					if (i + 1 < r.high)
						stack[sp++] = {i + 1, r.high};
				} else {
					if (i + 1 < r.high)
						stack[sp++] = {i + 1, r.high};
					if (r.low < i - 1)
						stack[sp++] = {r.low, i - 1};
				}
			}
		}

		//! A special version of the quick sort algorithm.
		//! Instead of relying on recursion it allocates an acceleration structure on the stack.
		//! \tparam Container Container to sort
		//! \tparam TCmpFunc Comparison function
		//! \tparam TSwapFunc Swap function
		//! \param arr Container to sort
		//! \param low Low index of the array to sort
		//! \param high High index of the array to sort
		//! \param cmpFunc Comparison function
		//! \param maxStackSize Maximum depth of the stack used for the acceleration structure.
		//! \warning If the input container is larger than \param maxStackSize the function might end up accessing memory
		//!          out-of-bounds. This would usually happen e.g. when sorting an already sorted array. This is because
		//!					 you can not easily predict the depth of the stack used by the algorithm.
		template <typename Container, typename TCmpFunc>
		void quick_sort_stack(Container& arr, int low, int high, TCmpFunc cmpFunc, uint32_t maxStackSize) {
			quick_sort_stack(
					arr, low, high, cmpFunc,
					[&arr](uint32_t a, uint32_t b) {
						core::swap(arr[a], arr[b]);
					},
					maxStackSize);
		}

		//! Sort the array \param arr given a comparison function \param cmpFunc.
		//! Sorts using a sorting network up to 8 elements. Quick sort above 32.
		//! \tparam Container Container to sort
		//! \tparam TCmpFunc Comparision function
		//! \param arr Container to sort
		//! \param cmpFunc Comparision function
		template <typename T, typename TCmpFunc>
		void sort(T* beg, T* end, TCmpFunc cmpFunc) {
			const auto n = (uintptr_t)(end - beg);
			if (detail::sort_nwk(beg, end, cmpFunc))
				return;

			quick_sort(beg, 0, (int)n - 1, cmpFunc);
		}

		template <typename C, typename TCmpFunc>
		void sort(C& c, TCmpFunc cmpFunc) {
			sort(c.begin(), c.end(), cmpFunc);
		}

		//! Sort the array \param arr given a comparison function \param cmpFunc.
		//! If cmpFunc returns true it performs \param swapFunc which can perform the sorting.
		//! Use when it is necessary to sort multiple arrays at once.
		//! Sorts using a sorting network up to 8 elements.
		//! \warning Currently only up to 32 elements are supported.
		//! \tparam Container Container to sort
		//! \tparam TCmpFunc Comparision function
		//! \tparam TSwapFunc Sorting function
		//! \param arr Container to sort
		//! \param cmpFunc Comparision function
		//! \param swapFunc Sorting function
		template <typename T, typename TCmpFunc, typename TSwapFunc>
		void sort(T* beg, T* end, TCmpFunc cmpFunc, TSwapFunc swapFunc) {
			const auto n = (uintptr_t)(end - beg);
			if (detail::sort_nwk(beg, end, cmpFunc, swapFunc))
				return;

			quick_sort(beg, 0, (int)n - 1, cmpFunc, swapFunc);
		}

		template <typename C, typename TCmpFunc, typename TSwapFunc>
		void sort(C& c, TCmpFunc cmpFunc, TSwapFunc swapFunc) {
			sort(c.begin(), c.end(), cmpFunc, swapFunc);
		}
	} // namespace core
} // namespace gaia
