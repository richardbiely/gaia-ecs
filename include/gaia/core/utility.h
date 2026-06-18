#pragma once
#include "gaia/config/config.h"

#include <cstdint>
#include <cstdio>
#include <tuple>
#include <type_traits>
#include <utility>

#if GAIA_PLATFORM_WINDOWS
	#include <malloc.h>
#else
	#include <alloca.h>
#endif

#include "gaia/core/iterator.h"

namespace gaia {
	//! Sentinel index value returned by helpers when a lookup fails.
	constexpr uint32_t BadIndex = uint32_t(-1);

#if GAIA_COMPILER_MSVC || GAIA_PLATFORM_WINDOWS
	#define GAIA_STRCPY(var, max_len, text)                                                                              \
		strncpy_s((var), (text), (max_len));                                                                               \
		(var)[(max_len) - 1] = 0;
	#define GAIA_STRFMT(var, max_len, fmt, ...) sprintf_s((var), (max_len), fmt, __VA_ARGS__)
	#define GAIA_STRLEN(var, max_len) strnlen_s((var), (max_len))
#else
	#define GAIA_STRCPY(var, max_len, text)                                                                              \
		{                                                                                                                  \
			strncpy((var), (text), (max_len));                                                                               \
			(var)[(max_len) - 1] = 0;                                                                                        \
		}
	#define GAIA_STRFMT(var, max_len, fmt, ...) snprintf((var), (max_len), fmt, __VA_ARGS__)
	#define GAIA_STRLEN(var, max_len) strnlen((var), (max_len))
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

		//! Tag type used to request zero-initialization in APIs that accept marker objects.
		struct zero_t {
			explicit constexpr zero_t() = default;
		};
		//! Shared zero-initialization tag instance.
		inline constexpr zero_t zero{};

		//! Removes both reference and pointer qualifiers from @a T.
		template <typename T>
		using rem_rp_t = typename detail::rem_rp<T>::type;

		//! True when @a T is a mutable pointer or reference type.
		template <typename T>
		inline constexpr bool is_mut_v = detail::is_mut<T>::value;

		//! Decayed value type after removing an optional pointer qualifier from @a T.
		template <typename T>
		using raw_t = typename std::decay_t<std::remove_pointer_t<T>>;

		//! True when @a T already is a non-array raw value type.
		template <typename T>
		inline constexpr bool is_raw_v = std::is_same_v<T, raw_t<T>> && !std::is_array_v<T>;

		//! True when @a T is a complete type at the point of instantiation.
		template <typename T>
		inline constexpr bool is_complete_v = detail::is_complete<T>::value;

		//! Obtains the actual address of the object \param obj or function arg, even in presence of overloaded operator&.
		template <typename T>
		constexpr T* addressof(T& obj) noexcept {
			return &obj;
		}

		//! Rvalue overload is deleted to prevent taking the address of const rvalues.
		template <typename T>
		const T* addressof(const T&&) = delete;

		//! Checks whether @a ptr satisfies the alignment requirements of @a T.
		//! \tparam T Pointee type used for alignment validation.
		//! \param ptr Pointer to validate.
		template <typename T>
		constexpr bool check_alignment(const T* ptr) noexcept {
			return (reinterpret_cast<uintptr_t>(ptr)) % alignof(T) == 0;
		}

		//! RAII helper that calls `lock()` on construction and `unlock()` on destruction.
		//! \tparam T Lockable type exposing `lock()` and `unlock()`.
		template <typename T>
		struct lock_scope {
			T& m_ctx;

			//! Acquires the lock represented by @a ctx.
			//! \param ctx Lockable object to guard for the lifetime of this scope.
			lock_scope(T& ctx): m_ctx(ctx) {
				ctx.lock();
			}
			//! Releases the guarded lock.
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

		//! Detects containers exposing both `data()` and `size()`.
		template <typename, typename = void>
		struct has_data_size: std::false_type {};
		template <typename T>
		struct has_data_size<
				T, std::void_t< //
							 decltype(detail::data(std::declval<T>())), //
							 decltype(detail::size(std::declval<T>())) //
							 >>: std::true_type {};

		//! Detects containers exposing `begin()`, `end()`, and `size()`.
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

		//! Converts a byte count to a bit count.
		//! \tparam T Integral value type.
		//! \param value Number of bytes.
		template <typename T>
		constexpr T as_bits(T value) {
			static_assert(std::is_integral_v<T>);
			return value * (T)8;
		}

		//! Converts a bit count to a byte count using integer division.
		//! \tparam T Integral value type.
		//! \param value Number of bits.
		template <typename T>
		constexpr T as_bytes(T value) {
			static_assert(std::is_integral_v<T>);
			return value / (T)8;
		}

		//----------------------------------------------------------------------
		// Memory size helpers
		//----------------------------------------------------------------------

		//! Counts how many bits are required to represent @a number.
		//! \tparam T Integral value type.
		//! \param number Value to inspect.
		template <typename T>
		constexpr uint32_t count_bits(T number) {
			uint32_t bits_needed = 0;
			while (number > 0U) {
				number >>= 1U;
				++bits_needed;
			}
			return bits_needed;
		}

		//! Checks whether @a number is a power of two.
		//! \tparam T Integral value type.
		//! \param number Value to inspect.
		template <typename T>
		constexpr bool is_pow2(T number) {
			static_assert(std::is_integral<T>::value, "is_pow2 must be used with integer types");

			return (number & (number - 1)) == 0;
		}

		//! Returns the highest power of two not greater than @a number.
		//! \tparam T Integral value type.
		//! \param number Value to inspect.
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

		//! Constructs an object of type @a T in the uninitialized storage at the memory address @a pData.
		//! \tparam T Type to construct.
		//! \param pData Pointer to where the object will be constructed; must not be null.
		template <typename T>
		void call_ctor_raw(T* pData) {
			GAIA_ASSERT(pData != nullptr);
			(void)::new (const_cast<void*>(static_cast<const volatile void*>(core::addressof(*pData)))) T;
		}

		//! Constructs @a cnt objects of type @a T in the uninitialized storage at the memory address @a pData.
		//! \tparam T Type to construct.
		//! \param pData Pointer to where the object will be constructed; must not be null.
		//! \param cnt Number of objects to construct.
		template <typename T>
		void call_ctor_raw_n(T* pData, size_t cnt) {
			GAIA_ASSERT(pData != nullptr);
			for (size_t i = 0; i < cnt; ++i) {
				auto* ptr = pData + i;
				(void)::new (const_cast<void*>(static_cast<const volatile void*>(core::addressof(*ptr)))) T;
			}
		}

		//! Value-constructs an object of type @a T in the uninitialized storage at the memory address @a pData.
		//! \tparam T Type to construct.
		//! \param pData Pointer to where the object will be constructed; must not be null.
		template <typename T>
		void call_ctor_val(T* pData) {
			GAIA_ASSERT(pData != nullptr);
			(void)::new (const_cast<void*>(static_cast<const volatile void*>(core::addressof(*pData)))) T();
		}

		//! Value-constructs @a cnt objects of type @a T in the uninitialized storage at the memory address @a pData.
		//! \tparam T Type to construct.
		//! \param pData Pointer to where the object will be constructed; must not be null.
		//! \param cnt Number of objects to construct.
		template <typename T>
		void call_ctor_val_n(T* pData, size_t cnt) {
			GAIA_ASSERT(pData != nullptr);
			for (size_t i = 0; i < cnt; ++i) {
				auto* ptr = pData + i;
				(void)::new (const_cast<void*>(static_cast<const volatile void*>(core::addressof(*ptr)))) T();
			}
		}

		//! Constructs an object of type @a T at the given memory address.
		//! \tparam T Type to construct
		//! \param pData Pointer to the memory where the object should be constructed.
		//! \warning pData must not be nullptr.
		template <typename T>
		void call_ctor([[maybe_unused]] T* pData) {
			GAIA_ASSERT(pData != nullptr);
			if constexpr (!std::is_trivially_constructible_v<T>) {
				(void)::new (pData) T();
			}
		}

		//! Constructs @a cnt objects of type @a T at the memory address @a pData.
		//! \tparam T Type to construct.
		//! \param pData Pointer to where the object will be constructed; must not be null.
		//! \param cnt Number of objects to construct.
		template <typename T>
		void call_ctor_n([[maybe_unused]] T* pData, [[maybe_unused]] size_t cnt) {
			GAIA_ASSERT(pData != nullptr);
			if constexpr (!std::is_trivially_constructible_v<T>) {
				for (size_t i = 0; i < cnt; ++i)
					(void)::new (pData + i) T();
			}
		}

		//! Constructs an object of type @a T at the memory address @a pData using forwarded arguments.
		//! \tparam T Type to construct.
		//! \tparam Args Constructor argument types.
		//! \param pData Pointer to the memory where the object should be constructed; must not be null.
		//! \param args Arguments forwarded to the constructor of @a T.
		template <typename T, typename... Args>
		void call_ctor(T* pData, Args&&... args) {
			GAIA_ASSERT(pData != nullptr);
			if constexpr (std::is_constructible_v<T, Args...>)
				(void)::new (pData) T(GAIA_FWD(args)...);
			else
				(void)::new (pData) T{GAIA_FWD(args)...};
		}

		//! Destructs an object of type @a T at the given memory address.
		//! \tparam T Type to destruct
		//! \param pData Pointer to the memory where the object should be destructed.
		//! \warning pData must not be nullptr.
		template <typename T>
		void call_dtor([[maybe_unused]] T* pData) {
			GAIA_ASSERT(pData != nullptr);
			if constexpr (!std::is_trivially_destructible_v<T>) {
				pData->~T();
			}
		}

		//! Destructs @a cnt objects of type @a T at the memory address @a pData.
		//! \tparam T Type to construct.
		//! \param pData Pointer to where the object will be destructed; must not be null.
		//! \param cnt Number of objects to construct.
		template <typename T>
		void call_dtor_n([[maybe_unused]] T* pData, [[maybe_unused]] size_t cnt) {
			GAIA_ASSERT(pData != nullptr);
			if constexpr (!std::is_trivially_destructible_v<T>) {
				for (size_t i = 0; i < cnt; ++i)
					pData[i].~T();
			}
		}

		//----------------------------------------------------------------------
		// Element swapping
		//----------------------------------------------------------------------

		//! Swaps @a left and @a right using move operations.
		//! \tparam T Value type.
		//! \param left Left operand.
		//! \param right Right operand.
		template <typename T>
		constexpr void swap(T& left, T& right) {
			T tmp = GAIA_MOV(left);
			left = GAIA_MOV(right);
			right = GAIA_MOV(tmp);
		}

		//! Swaps two elements in a contiguous range when they are out of order according to @a cmpFunc.
		//! \tparam T Element type.
		//! \tparam TCmpFunc Comparison functor type.
		//! \param c Pointer to the first element of the range.
		//! \param lhs Left index.
		//! \param rhs Right index.
		//! \param cmpFunc Ordering predicate.
		template <typename T, typename TCmpFunc>
		constexpr void swap_if(T* c, size_t lhs, size_t rhs, TCmpFunc cmpFunc) noexcept {
			if (!cmpFunc(c[lhs], c[rhs]))
				core::swap(c[lhs], c[rhs]);
		}

		//! Swaps @a lhs and @a rhs when they are out of order according to @a cmpFunc.
		//! \tparam T Value type.
		//! \tparam TCmpFunc Comparison functor type.
		//! \param lhs Left operand.
		//! \param rhs Right operand.
		//! \param cmpFunc Ordering predicate.
		template <typename T, typename TCmpFunc>
		constexpr void swap_if(T& lhs, T& rhs, TCmpFunc cmpFunc) noexcept {
			if (!cmpFunc(lhs, rhs))
				core::swap(lhs, rhs);
		}

		//! Swaps @a lhs and @a rhs when @a cmpFunc reports the current order should be inverted.
		//! \tparam T Value type.
		//! \tparam TCmpFunc Comparison functor type.
		//! \param lhs Left operand.
		//! \param rhs Right operand.
		//! \param cmpFunc Ordering predicate.
		template <typename T, typename TCmpFunc>
		constexpr void swap_if_not(T& lhs, T& rhs, TCmpFunc cmpFunc) noexcept {
			if (cmpFunc(lhs, rhs))
				core::swap(lhs, rhs);
		}

		//! Invokes @a swapFunc for two indexed elements when they are out of order according to @a cmpFunc.
		//! \tparam T Element type.
		//! \tparam TCmpFunc Comparison functor type.
		//! \tparam TSwapFunc Swap callback type.
		//! \param c Pointer to the first element of the range.
		//! \param lhs Left index.
		//! \param rhs Right index.
		//! \param cmpFunc Ordering predicate.
		//! \param swapFunc Callback performing the actual swap.
		template <typename T, typename TCmpFunc, typename TSwapFunc>
		constexpr void try_swap_if(T* c, uint32_t lhs, uint32_t rhs, TCmpFunc cmpFunc, TSwapFunc swapFunc) noexcept {
			if (!cmpFunc(c[lhs], c[rhs]))
				swapFunc(lhs, rhs);
		}

		//! Invokes @a swapFunc for container indices when the elements are out of order according to @a cmpFunc.
		//! \tparam C Container type.
		//! \tparam TCmpFunc Comparison functor type.
		//! \tparam TSwapFunc Swap callback type.
		//! \param c Container to inspect.
		//! \param lhs Left index.
		//! \param rhs Right index.
		//! \param cmpFunc Ordering predicate.
		//! \param swapFunc Callback performing the actual swap.
		template <typename C, typename TCmpFunc, typename TSwapFunc>
		constexpr void try_swap_if(
				C& c, typename C::size_type lhs, typename C::size_type rhs, TCmpFunc cmpFunc, TSwapFunc swapFunc) noexcept {
			if (!cmpFunc(c[lhs], c[rhs]))
				swapFunc(lhs, rhs);
		}

		//! Invokes @a swapFunc for container indices when @a cmpFunc reports the current order should be inverted.
		//! \tparam C Container type.
		//! \tparam TCmpFunc Comparison functor type.
		//! \tparam TSwapFunc Swap callback type.
		//! \param c Container to inspect.
		//! \param lhs Left index.
		//! \param rhs Right index.
		//! \param cmpFunc Ordering predicate.
		//! \param swapFunc Callback performing the actual swap.
		template <typename C, typename TCmpFunc, typename TSwapFunc>
		constexpr void try_swap_if_not(
				C& c, typename C::size_type lhs, typename C::size_type rhs, TCmpFunc cmpFunc, TSwapFunc swapFunc) noexcept {
			if (cmpFunc(c[lhs], c[rhs]))
				swapFunc(lhs, rhs);
		}

		//----------------------------------------------------------------------
		// Value filling
		//----------------------------------------------------------------------

		//! Assigns @a value to every element in the range [`first`, `last`).
		//! \tparam ForwardIt Iterator type.
		//! \tparam T Value type.
		//! \param first First element in the range.
		//! \param last One-past-last element in the range.
		//! \param value Value to assign.
		template <class ForwardIt, class T>
		constexpr void fill(ForwardIt first, ForwardIt last, const T& value) {
			for (; first != last; ++first) {
				*first = value;
			}
		}

		//----------------------------------------------------------------------
		// Value range checking
		//----------------------------------------------------------------------

		//! Returns the smaller of two values.
		//! \tparam T Value type.
		//! \param a Left operand.
		//! \param b Right operand.
		template <class T>
		constexpr const T& get_min(const T& a, const T& b) {
			return (b < a) ? b : a;
		}

		//! Returns the greater of two values.
		//! \tparam T Value type.
		//! \param a Left operand.
		//! \param b Right operand.
		template <class T>
		constexpr const T& get_max(const T& a, const T& b) {
			return (b > a) ? b : a;
		}

		//----------------------------------------------------------------------
		// Checking if a template arg is unique among the rest
		//----------------------------------------------------------------------

		//! Compile-time predicate checking whether every type in a pack is unique.
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

		//! Builds a tuple type from the first occurrence of each type in a parameter pack.
		template <typename T, typename... Ts>
		struct unique: detail::type_identity<T> {}; // TODO: In C++20 we could use std::type_identity

		template <typename... Ts, typename U, typename... Us>
		struct unique<std::tuple<Ts...>, U, Us...>:
				std::conditional_t<
						(std::is_same_v<U, Ts> || ...), unique<std::tuple<Ts...>, Us...>, unique<std::tuple<Ts..., U>, Us...>> {};

		//! Tuple type containing only the first occurrence of each type in @a Ts.
		template <typename... Ts>
		using unique_tuple = typename unique<std::tuple<>, Ts...>::type;

		//----------------------------------------------------------------------
		// Calculating total size of all types of tuple
		//----------------------------------------------------------------------

		//! Returns the sum of `sizeof(...)` for all types stored in the tuple type.
		//! \tparam Args Tuple element types.
		template <typename... Args>
		constexpr unsigned get_args_size(std::tuple<Args...> const& /*no_name*/) {
			return (sizeof(Args) + ...);
		}

		//----------------------------------------------------------------------
		// Function arguments type checks
		//----------------------------------------------------------------------

		//! Lightweight carrier for a function argument type pack.
		template <typename... Type>
		struct func_type_list {};

		//! Extracts the argument list from a const member function pointer.
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

		//! Detects whether @a T defines `operator==` as a member function.
		template <typename T>
		struct has_func_equals {
			static constexpr bool value = decltype(detail::has_mfunc_equals_check<T>(0))::value;
		};

		//! Detects whether @a T supports a free `operator==`.
		template <typename T>
		struct has_ffunc_equals {
			static constexpr bool value = decltype(detail::has_ffunc_equals_check<T>(0))::value;
		};

		//----------------------------------------------------------------------
		// Type helpers
		//----------------------------------------------------------------------

		//! Compile-time list of types.
		template <typename... Type>
		struct type_list {
			using types = type_list;
			static constexpr auto size = sizeof...(Type);
		};

		//! Concatenates two @ref type_list instances.
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
				if constexpr (
						(std::is_invocable_v<
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
				if constexpr (
						(std::is_invocable_v<
								 Func&&, decltype(std::get<FirstIdx + Is>(tuple)), std::integral_constant<decltype(FirstIdx), Is>> &&
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

		//! Applies @a func to every element in the iterator range [`first`, `last`).
		//! \tparam InputIt Iterator type.
		//! \tparam Func Callable type.
		//! \param first First element in the range.
		//! \param last One-past-last element in the range.
		//! \param func Callable invoked for each dereferenced element.
		//! \return The callable object after iteration.
		template <typename InputIt, typename Func>
		constexpr Func each(InputIt first, InputIt last, Func func) {
			for (; first != last; ++first)
				func(*first);
			return func;
		}

		//! Applies @a func to every element in @a arr.
		//! \tparam C Container type.
		//! \tparam Func Callable type.
		//! \param arr Container to iterate.
		//! \param func Callable invoked for each element.
		//! \return The callable object after iteration.
		template <typename C, typename Func>
		constexpr auto each(const C& arr, Func func) {
			return each(arr.begin(), arr.end(), func);
		}

		//----------------------------------------------------------------------
		// Lookups
		//----------------------------------------------------------------------

		//! Searches the iterator range [`first`, `last`) for the first element equal to @a value.
		//! \tparam InputIt Iterator type.
		//! \tparam T Lookup value type.
		//! \param first First element in the range.
		//! \param last One-past-last element in the range.
		//! \param value Value to find.
		//! \return Iterator to the first matching element or @a last when no match is found.
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

		//! Searches @a arr for the first element equal to @a item.
		//! \tparam C Container type.
		//! \tparam V Lookup value type.
		//! \param arr Container to inspect.
		//! \param item Value to find.
		//! \return Iterator returned by the container lookup or `arr.end()` when no match is found.
		template <typename C, typename V>
		constexpr auto find(const C& arr, const V& item) {
			if constexpr (has_func_find<C>::value)
				return arr.find(item);
			else
				return core::find(arr.begin(), arr.end(), item);
		}

		//! Searches the iterator range [`first`, `last`) for the first element satisfying @a func.
		//! \tparam InputIt Iterator type.
		//! \tparam Func Predicate type.
		//! \param first First element in the range.
		//! \param last One-past-last element in the range.
		//! \param func Predicate used for matching.
		//! \return Iterator to the first matching element or @a last when no match is found.
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

		//! Searches @a arr for the first element satisfying @a predicate.
		//! \tparam UnaryPredicate Predicate type.
		//! \tparam C Container type.
		//! \param arr Container to inspect.
		//! \param predicate Predicate used for matching.
		//! \return Iterator returned by the container lookup or `arr.end()` when no match is found.
		template <typename UnaryPredicate, typename C>
		constexpr auto find_if(const C& arr, UnaryPredicate predicate) {
			if constexpr (has_func_find_if<C, UnaryPredicate>::value)
				return arr.find_id(predicate);
			else
				return core::find_if(arr.begin(), arr.end(), predicate);
		}

		//! Searches the iterator range [`first`, `last`) for the first element that does not satisfy @a func.
		//! \tparam InputIt Iterator type.
		//! \tparam Func Predicate type.
		//! \param first First element in the range.
		//! \param last One-past-last element in the range.
		//! \param func Predicate used for matching.
		//! \return Iterator to the first non-matching element or @a last when all elements match.
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

		//! Searches @a arr for the first element that does not satisfy @a predicate.
		//! \tparam UnaryPredicate Predicate type.
		//! \tparam C Container type.
		//! \param arr Container to inspect.
		//! \param predicate Predicate used for matching.
		//! \return Iterator returned by the container lookup or `arr.end()` when all elements match.
		template <typename UnaryPredicate, typename C>
		constexpr auto find_if_not(const C& arr, UnaryPredicate predicate) {
			if constexpr (has_func_find_if_not<C, UnaryPredicate>::value)
				return arr.find_if_not(predicate);
			else
				return core::find_if_not(arr.begin(), arr.end(), predicate);
		}

		//----------------------------------------------------------------------

		//! Checks whether @a arr contains @a item.
		//! \tparam C Container type.
		//! \tparam V Lookup value type.
		//! \param arr Container to inspect.
		//! \param item Value to find.
		template <typename C, typename V>
		constexpr bool has(const C& arr, const V& item) {
			const auto it = find(arr, item);
			return it != arr.end();
		}

		//! Checks whether any element in @a arr satisfies @a predicate.
		//! \tparam UnaryPredicate Predicate type.
		//! \tparam C Container type.
		//! \param arr Container to inspect.
		//! \param predicate Predicate used for matching.
		template <typename UnaryPredicate, typename C>
		constexpr bool has_if(const C& arr, UnaryPredicate predicate) {
			const auto it = find_if(arr, predicate);
			return it != arr.end();
		}

		//----------------------------------------------------------------------

		//! Returns the index of @a item in @a arr or @ref BadIndex when the item is not present.
		//! \tparam C Container type.
		//! \param arr Container to inspect.
		//! \param item Value to locate.
		template <typename C>
		constexpr auto get_index(const C& arr, typename C::const_reference item) {
			const auto it = find(arr, item);
			if (it == arr.end())
				return BadIndex;

			return (decltype(BadIndex))core::distance(arr.begin(), it);
		}

		//! Returns the index of @a item in @a arr without checking whether the item exists.
		//! \tparam C Container type.
		//! \param arr Container to inspect.
		//! \param item Value to locate.
		//! \warning Returns an invalid index when @a item is not present.
		template <typename C>
		constexpr auto get_index_unsafe(const C& arr, typename C::const_reference item) {
			return (decltype(BadIndex))core::distance(arr.begin(), find(arr, item));
		}

		//! Returns the index of the first element satisfying @a predicate or @ref BadIndex when none matches.
		//! \tparam UnaryPredicate Predicate type.
		//! \tparam C Container type.
		//! \param arr Container to inspect.
		//! \param predicate Predicate used for matching.
		template <typename UnaryPredicate, typename C>
		constexpr auto get_index_if(const C& arr, UnaryPredicate predicate) {
			const auto it = find_if(arr, predicate);
			if (it == arr.end())
				return BadIndex;

			return (decltype(BadIndex))core::distance(arr.begin(), it);
		}

		//! Returns the index of the first element satisfying @a predicate without checking whether a match exists.
		//! \tparam UnaryPredicate Predicate type.
		//! \tparam C Container type.
		//! \param arr Container to inspect.
		//! \param predicate Predicate used for matching.
		//! \warning Returns an invalid index when no element satisfies @a predicate.
		template <typename UnaryPredicate, typename C>
		constexpr auto get_index_if_unsafe(const C& arr, UnaryPredicate predicate) {
			return (decltype(BadIndex))core::distance(arr.begin(), find_if(arr, predicate));
		}

		//----------------------------------------------------------------------
		// Erasure
		//----------------------------------------------------------------------

		//! Replaces the item at @a idx in the array @a arr with the last item of the array if possible and
		//! removes its last item. Use when shifting of the entire array is not wanted.
		//! \param arr Array
		//! \param idx Array index
		//! \warning If the item order is important and the size of the array changes after calling this function you need
		//!          to sort the array.
		//! \warning Does not do bound checks. Undefined behavior when @a idx is out of bounds.
		template <typename C>
		void swap_erase_unsafe(C& arr, typename C::size_type idx) {
			GAIA_ASSERT(idx < arr.size());

			if (idx + 1 != arr.size())
				arr[idx] = arr[arr.size() - 1];

			arr.pop_back();
		}

		//! Replaces the item at @a idx in the array @a arr with the last item of the array if possible and
		//! removes its last item. Use when shifting of the entire array is not wanted.
		//! \param arr Array
		//! \param idx Array index
		//! \warning If the item order is important and the size of the array changes after calling this function you need
		//!          to sort the array.
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

		//! Equality comparison functor using `operator==`.
		template <typename T>
		struct equal_to {
			constexpr bool operator()(const T& lhs, const T& rhs) const {
				return lhs == rhs;
			}
		};

		//! Strict weak ordering functor using `operator<`.
		template <typename T>
		struct is_smaller {
			constexpr bool operator()(const T& lhs, const T& rhs) const {
				return lhs < rhs;
			}
		};

		//! Comparison functor using `operator<=`.
		template <typename T>
		struct is_smaller_or_equal {
			constexpr bool operator()(const T& lhs, const T& rhs) const {
				return lhs <= rhs;
			}
		};

		//! Strict weak ordering functor using `operator>`.
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
			int sort_median_of_three(Container& arr, int low, int mid, int high, TCmpFunc cmpFunc) {
				if (cmpFunc(arr[(uint32_t)low], arr[(uint32_t)mid])) {
					if (cmpFunc(arr[(uint32_t)mid], arr[(uint32_t)high]))
						return mid;
					return cmpFunc(arr[(uint32_t)low], arr[(uint32_t)high]) ? high : low;
				}

				if (cmpFunc(arr[(uint32_t)low], arr[(uint32_t)high]))
					return low;
				return cmpFunc(arr[(uint32_t)mid], arr[(uint32_t)high]) ? high : mid;
			}

			template <typename Container, typename TCmpFunc>
			int sort_choose_pivot(Container& arr, int low, int high, TCmpFunc cmpFunc) {
				const int size = high - low + 1;
				const int mid = low + (size >> 1);
				if (size > 1024) {
					const int step = size >> 3;
					const int lowMed = sort_median_of_three(arr, low, low + step, low + step + step, cmpFunc);
					const int midMed = sort_median_of_three(arr, mid - step, mid, mid + step, cmpFunc);
					const int highMed = sort_median_of_three(arr, high - step - step, high - step, high, cmpFunc);
					return sort_median_of_three(arr, lowMed, midMed, highMed, cmpFunc);
				}

				return sort_median_of_three(arr, low, mid, high, cmpFunc);
			}

			template <typename Container, typename TCmpFunc, typename TSwapFunc>
			void insertion_sort_range(Container& arr, int low, int high, TCmpFunc cmpFunc, TSwapFunc swapFunc) {
				for (int i = low + 1; i <= high; ++i) {
					for (int j = i; j > low && cmpFunc(arr[(uint32_t)j], arr[(uint32_t)(j - 1)]); --j)
						swapFunc((uint32_t)(j - 1), (uint32_t)j);
				}
			}

			template <typename Container, typename TCmpFunc>
			void insertion_sort_range(Container& arr, int low, int high, TCmpFunc cmpFunc) {
				for (int i = low + 1; i <= high; ++i) {
					if (cmpFunc(arr[(uint32_t)i], arr[(uint32_t)(i - 1)])) {
						auto tmp = GAIA_MOV(arr[(uint32_t)i]);
						int j = i;
						do {
							arr[(uint32_t)j] = GAIA_MOV(arr[(uint32_t)(j - 1)]);
							--j;
						} while (j > low && cmpFunc(tmp, arr[(uint32_t)(j - 1)]));
						arr[(uint32_t)j] = GAIA_MOV(tmp);
					}
				}
			}

			template <typename Container, typename TCmpFunc, typename TSwapFunc>
			void sort_order_three(Container& arr, int lhs, int mid, int rhs, TCmpFunc cmpFunc, TSwapFunc swapFunc) {
				if (cmpFunc(arr[(uint32_t)mid], arr[(uint32_t)lhs]))
					swapFunc((uint32_t)lhs, (uint32_t)mid);
				if (cmpFunc(arr[(uint32_t)rhs], arr[(uint32_t)mid])) {
					swapFunc((uint32_t)mid, (uint32_t)rhs);
					if (cmpFunc(arr[(uint32_t)mid], arr[(uint32_t)lhs]))
						swapFunc((uint32_t)lhs, (uint32_t)mid);
				}
			}

			template <typename Container, typename TCmpFunc, typename TSwapFunc>
			void sort_prepare_pivot_first(Container& arr, int low, int high, TCmpFunc cmpFunc, TSwapFunc swapFunc) {
				constexpr int NintherThreshold = 128;
				const int size = high - low + 1;
				const int mid = low + (size >> 1);

				if (size > NintherThreshold) {
					sort_order_three(arr, low, mid, high, cmpFunc, swapFunc);
					sort_order_three(arr, low + 1, mid - 1, high - 1, cmpFunc, swapFunc);
					sort_order_three(arr, low + 2, mid + 1, high - 2, cmpFunc, swapFunc);
					sort_order_three(arr, mid - 1, mid, mid + 1, cmpFunc, swapFunc);
					swapFunc((uint32_t)low, (uint32_t)mid);
				} else {
					sort_order_three(arr, mid, low, high, cmpFunc, swapFunc);
				}
			}

			template <typename Container, typename TCmpFunc, typename TSwapFunc>
			int quick_sort_partition_pivot_first(Container& arr, int low, int high, TCmpFunc cmpFunc, TSwapFunc swapFunc) {
				const int begin = low;
				int first = low;
				int last = high + 1;
				const int end = last;
				auto pivot = GAIA_MOV(arr[(uint32_t)first]);

				do {
					++first;
				} while (first != end && cmpFunc(arr[(uint32_t)first], pivot));

				if (begin == first - 1) {
					while (first < last && !cmpFunc(arr[(uint32_t)(--last)], pivot)) {
					}
				} else {
					do {
						--last;
					} while (!cmpFunc(arr[(uint32_t)last], pivot));
				}

				while (first < last) {
					swapFunc((uint32_t)first, (uint32_t)last);
					do {
						++first;
					} while (cmpFunc(arr[(uint32_t)first], pivot));
					do {
						--last;
					} while (!cmpFunc(arr[(uint32_t)last], pivot));
				}

				const int pivotPos = first - 1;
				if (begin != pivotPos)
					arr[(uint32_t)begin] = GAIA_MOV(arr[(uint32_t)pivotPos]);
				arr[(uint32_t)pivotPos] = GAIA_MOV(pivot);
				return pivotPos;
			}

			template <typename T, typename TCmpFunc, typename TSwapFunc>
			bool try_sorted_or_reversed(T* beg, uint32_t n, TCmpFunc cmpFunc, TSwapFunc swapFunc) {
				if (n <= 1)
					return true;

				bool sorted = true;
				for (uint32_t i = 1; i < n; ++i) {
					if (cmpFunc(beg[i], beg[i - 1])) {
						sorted = false;
						break;
					}
				}
				if (sorted)
					return true;

				bool reversed = true;
				for (uint32_t i = 1; i < n; ++i) {
					if (cmpFunc(beg[i - 1], beg[i])) {
						reversed = false;
						break;
					}
				}
				if (!reversed)
					return false;

				for (uint32_t i = 0, j = n - 1; i < j; ++i, --j)
					swapFunc(i, j);
				return true;
			}

			template <typename Container, typename TCmpFunc, typename TSwapFunc>
			int quick_sort_partition_hoare(Container& arr, int low, int high, TCmpFunc cmpFunc, TSwapFunc swapFunc) {
				const int pivotIdx = sort_choose_pivot(arr, low, high, cmpFunc);
				const auto pivot = arr[(uint32_t)pivotIdx];

				int i = low - 1;
				int j = high + 1;

				while (true) {
					do {
						++i;
					} while (cmpFunc(arr[(uint32_t)i], pivot));

					do {
						--j;
					} while (cmpFunc(pivot, arr[(uint32_t)j]));

					if (i >= j)
						return j;

					swapFunc((uint32_t)i, (uint32_t)j);
				}
			}

			template <typename Container, typename TCmpFunc, typename TSwapFunc>
			void heap_sift_down(Container& arr, int low, int root, int high, TCmpFunc cmpFunc, TSwapFunc swapFunc) {
				while (true) {
					const int left = low + ((root - low) << 1) + 1;
					if (left > high)
						return;

					int child = left;
					const int right = left + 1;
					if (right <= high && cmpFunc(arr[(uint32_t)child], arr[(uint32_t)right]))
						child = right;

					if (!cmpFunc(arr[(uint32_t)root], arr[(uint32_t)child]))
						return;

					swapFunc((uint32_t)root, (uint32_t)child);
					root = child;
				}
			}

			template <typename Container, typename TCmpFunc, typename TSwapFunc>
			void heap_sort_range(Container& arr, int low, int high, TCmpFunc cmpFunc, TSwapFunc swapFunc) {
				for (int start = low + ((high - low - 1) >> 1); start >= low; --start)
					heap_sift_down(arr, low, start, high, cmpFunc, swapFunc);

				for (int end = high; end > low; --end) {
					swapFunc((uint32_t)low, (uint32_t)end);
					heap_sift_down(arr, low, low, end - 1, cmpFunc, swapFunc);
				}
			}

			inline uint32_t sort_depth_limit(uint32_t n) {
				uint32_t depth = 0;
				while (n > 1) {
					++depth;
					n >>= 1;
				}
				return depth * 2;
			}

			template <typename Container, typename TCmpFunc, typename TSwapFunc>
			void
			quick_sort_impl(Container& arr, int low, int high, TCmpFunc cmpFunc, TSwapFunc swapFunc, uint32_t depthLimit) {
				constexpr int InsertionSortThreshold = 24;

				while (high - low > InsertionSortThreshold) {
					if (depthLimit == 0) {
						heap_sort_range(arr, low, high, cmpFunc, swapFunc);
						return;
					}
					--depthLimit;

					const int split = quick_sort_partition_hoare(arr, low, high, cmpFunc, swapFunc);
					const int leftSize = split - low + 1;
					const int rightSize = high - split;

					if (leftSize < rightSize) {
						quick_sort_impl(arr, low, split, cmpFunc, swapFunc, depthLimit);
						low = split + 1;
					} else {
						quick_sort_impl(arr, split + 1, high, cmpFunc, swapFunc, depthLimit);
						high = split;
					}
				}

				if (low < high)
					insertion_sort_range(arr, low, high, cmpFunc, swapFunc);
			}

			template <typename Container, typename TCmpFunc>
			void quick_sort_impl(Container& arr, int low, int high, TCmpFunc cmpFunc, uint32_t depthLimit) {
				constexpr int InsertionSortThreshold = 24;
				auto swapFunc = [&arr](uint32_t a, uint32_t b) {
					core::swap(arr[a], arr[b]);
				};

				while (high - low > InsertionSortThreshold) {
					if (depthLimit == 0) {
						heap_sort_range(arr, low, high, cmpFunc, swapFunc);
						return;
					}
					--depthLimit;

					sort_prepare_pivot_first(arr, low, high, cmpFunc, swapFunc);
					const int pivot = quick_sort_partition_pivot_first(arr, low, high, cmpFunc, swapFunc);
					const int leftSize = pivot - low;
					const int rightSize = high - pivot;

					if (leftSize < rightSize) {
						quick_sort_impl(arr, low, pivot - 1, cmpFunc, depthLimit);
						low = pivot + 1;
					} else {
						quick_sort_impl(arr, pivot + 1, high, cmpFunc, depthLimit);
						high = pivot - 1;
					}
				}

				if (low < high)
					insertion_sort_range(arr, low, high, cmpFunc);
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
				} else if (n == 11) {
					swap_if(beg, 0, 9, cmpFunc);
					swap_if(beg, 1, 6, cmpFunc);
					swap_if(beg, 2, 4, cmpFunc);
					swap_if(beg, 3, 7, cmpFunc);
					swap_if(beg, 5, 8, cmpFunc);
					swap_if(beg, 0, 1, cmpFunc);
					swap_if(beg, 3, 5, cmpFunc);
					swap_if(beg, 4, 10, cmpFunc);
					swap_if(beg, 6, 9, cmpFunc);
					swap_if(beg, 7, 8, cmpFunc);
					swap_if(beg, 1, 3, cmpFunc);
					swap_if(beg, 2, 5, cmpFunc);
					swap_if(beg, 4, 7, cmpFunc);
					swap_if(beg, 8, 10, cmpFunc);
					swap_if(beg, 0, 4, cmpFunc);
					swap_if(beg, 1, 2, cmpFunc);
					swap_if(beg, 3, 7, cmpFunc);
					swap_if(beg, 5, 9, cmpFunc);
					swap_if(beg, 6, 8, cmpFunc);
					swap_if(beg, 0, 1, cmpFunc);
					swap_if(beg, 2, 6, cmpFunc);
					swap_if(beg, 4, 5, cmpFunc);
					swap_if(beg, 7, 8, cmpFunc);
					swap_if(beg, 9, 10, cmpFunc);
					swap_if(beg, 2, 4, cmpFunc);
					swap_if(beg, 3, 6, cmpFunc);
					swap_if(beg, 5, 7, cmpFunc);
					swap_if(beg, 8, 9, cmpFunc);
					swap_if(beg, 1, 2, cmpFunc);
					swap_if(beg, 3, 4, cmpFunc);
					swap_if(beg, 5, 6, cmpFunc);
					swap_if(beg, 7, 8, cmpFunc);
					swap_if(beg, 2, 3, cmpFunc);
					swap_if(beg, 4, 5, cmpFunc);
					swap_if(beg, 6, 7, cmpFunc);
				} else if (n == 12) {
					swap_if(beg, 0, 8, cmpFunc);
					swap_if(beg, 1, 7, cmpFunc);
					swap_if(beg, 2, 6, cmpFunc);
					swap_if(beg, 3, 11, cmpFunc);
					swap_if(beg, 4, 10, cmpFunc);
					swap_if(beg, 5, 9, cmpFunc);
					swap_if(beg, 0, 1, cmpFunc);
					swap_if(beg, 2, 5, cmpFunc);
					swap_if(beg, 3, 4, cmpFunc);
					swap_if(beg, 6, 9, cmpFunc);
					swap_if(beg, 7, 8, cmpFunc);
					swap_if(beg, 10, 11, cmpFunc);
					swap_if(beg, 0, 2, cmpFunc);
					swap_if(beg, 1, 6, cmpFunc);
					swap_if(beg, 5, 10, cmpFunc);
					swap_if(beg, 9, 11, cmpFunc);
					swap_if(beg, 0, 3, cmpFunc);
					swap_if(beg, 1, 2, cmpFunc);
					swap_if(beg, 4, 6, cmpFunc);
					swap_if(beg, 5, 7, cmpFunc);
					swap_if(beg, 8, 11, cmpFunc);
					swap_if(beg, 9, 10, cmpFunc);
					swap_if(beg, 1, 4, cmpFunc);
					swap_if(beg, 3, 5, cmpFunc);
					swap_if(beg, 6, 8, cmpFunc);
					swap_if(beg, 7, 10, cmpFunc);
					swap_if(beg, 1, 3, cmpFunc);
					swap_if(beg, 2, 5, cmpFunc);
					swap_if(beg, 6, 9, cmpFunc);
					swap_if(beg, 8, 10, cmpFunc);
					swap_if(beg, 2, 3, cmpFunc);
					swap_if(beg, 4, 5, cmpFunc);
					swap_if(beg, 6, 7, cmpFunc);
					swap_if(beg, 8, 9, cmpFunc);
					swap_if(beg, 4, 6, cmpFunc);
					swap_if(beg, 5, 7, cmpFunc);
					swap_if(beg, 3, 4, cmpFunc);
					swap_if(beg, 5, 6, cmpFunc);
					swap_if(beg, 7, 8, cmpFunc);
				} else if (n == 13) {
					swap_if(beg, 0, 12, cmpFunc);
					swap_if(beg, 1, 10, cmpFunc);
					swap_if(beg, 2, 9, cmpFunc);
					swap_if(beg, 3, 7, cmpFunc);
					swap_if(beg, 5, 11, cmpFunc);
					swap_if(beg, 6, 8, cmpFunc);
					swap_if(beg, 1, 6, cmpFunc);
					swap_if(beg, 2, 3, cmpFunc);
					swap_if(beg, 4, 11, cmpFunc);
					swap_if(beg, 7, 9, cmpFunc);
					swap_if(beg, 8, 10, cmpFunc);
					swap_if(beg, 0, 4, cmpFunc);
					swap_if(beg, 1, 2, cmpFunc);
					swap_if(beg, 3, 6, cmpFunc);
					swap_if(beg, 7, 8, cmpFunc);
					swap_if(beg, 9, 10, cmpFunc);
					swap_if(beg, 11, 12, cmpFunc);
					swap_if(beg, 4, 6, cmpFunc);
					swap_if(beg, 5, 9, cmpFunc);
					swap_if(beg, 8, 11, cmpFunc);
					swap_if(beg, 10, 12, cmpFunc);
					swap_if(beg, 0, 5, cmpFunc);
					swap_if(beg, 3, 8, cmpFunc);
					swap_if(beg, 4, 7, cmpFunc);
					swap_if(beg, 6, 11, cmpFunc);
					swap_if(beg, 9, 10, cmpFunc);
					swap_if(beg, 0, 1, cmpFunc);
					swap_if(beg, 2, 5, cmpFunc);
					swap_if(beg, 6, 9, cmpFunc);
					swap_if(beg, 7, 8, cmpFunc);
					swap_if(beg, 10, 11, cmpFunc);
					swap_if(beg, 1, 3, cmpFunc);
					swap_if(beg, 2, 4, cmpFunc);
					swap_if(beg, 5, 6, cmpFunc);
					swap_if(beg, 9, 10, cmpFunc);
					swap_if(beg, 1, 2, cmpFunc);
					swap_if(beg, 3, 4, cmpFunc);
					swap_if(beg, 5, 7, cmpFunc);
					swap_if(beg, 6, 8, cmpFunc);
					swap_if(beg, 2, 3, cmpFunc);
					swap_if(beg, 4, 5, cmpFunc);
					swap_if(beg, 6, 7, cmpFunc);
					swap_if(beg, 8, 9, cmpFunc);
					swap_if(beg, 3, 4, cmpFunc);
					swap_if(beg, 5, 6, cmpFunc);
				} else if (n == 14) {
					swap_if(beg, 0, 1, cmpFunc);
					swap_if(beg, 2, 3, cmpFunc);
					swap_if(beg, 4, 5, cmpFunc);
					swap_if(beg, 6, 7, cmpFunc);
					swap_if(beg, 8, 9, cmpFunc);
					swap_if(beg, 10, 11, cmpFunc);
					swap_if(beg, 12, 13, cmpFunc);
					swap_if(beg, 0, 2, cmpFunc);
					swap_if(beg, 1, 3, cmpFunc);
					swap_if(beg, 4, 8, cmpFunc);
					swap_if(beg, 5, 9, cmpFunc);
					swap_if(beg, 10, 12, cmpFunc);
					swap_if(beg, 11, 13, cmpFunc);
					swap_if(beg, 0, 4, cmpFunc);
					swap_if(beg, 1, 2, cmpFunc);
					swap_if(beg, 3, 7, cmpFunc);
					swap_if(beg, 5, 8, cmpFunc);
					swap_if(beg, 6, 10, cmpFunc);
					swap_if(beg, 9, 13, cmpFunc);
					swap_if(beg, 11, 12, cmpFunc);
					swap_if(beg, 0, 6, cmpFunc);
					swap_if(beg, 1, 5, cmpFunc);
					swap_if(beg, 3, 9, cmpFunc);
					swap_if(beg, 4, 10, cmpFunc);
					swap_if(beg, 7, 13, cmpFunc);
					swap_if(beg, 8, 12, cmpFunc);
					swap_if(beg, 2, 10, cmpFunc);
					swap_if(beg, 3, 11, cmpFunc);
					swap_if(beg, 4, 6, cmpFunc);
					swap_if(beg, 7, 9, cmpFunc);
					swap_if(beg, 1, 3, cmpFunc);
					swap_if(beg, 2, 8, cmpFunc);
					swap_if(beg, 5, 11, cmpFunc);
					swap_if(beg, 6, 7, cmpFunc);
					swap_if(beg, 10, 12, cmpFunc);
					swap_if(beg, 1, 4, cmpFunc);
					swap_if(beg, 2, 6, cmpFunc);
					swap_if(beg, 3, 5, cmpFunc);
					swap_if(beg, 7, 11, cmpFunc);
					swap_if(beg, 8, 10, cmpFunc);
					swap_if(beg, 9, 12, cmpFunc);
					swap_if(beg, 2, 4, cmpFunc);
					swap_if(beg, 3, 6, cmpFunc);
					swap_if(beg, 5, 8, cmpFunc);
					swap_if(beg, 7, 10, cmpFunc);
					swap_if(beg, 9, 11, cmpFunc);
					swap_if(beg, 3, 4, cmpFunc);
					swap_if(beg, 5, 6, cmpFunc);
					swap_if(beg, 7, 8, cmpFunc);
					swap_if(beg, 9, 10, cmpFunc);
					swap_if(beg, 6, 7, cmpFunc);
				} else if (n == 15) {
					swap_if(beg, 1, 2, cmpFunc);
					swap_if(beg, 3, 10, cmpFunc);
					swap_if(beg, 4, 14, cmpFunc);
					swap_if(beg, 5, 8, cmpFunc);
					swap_if(beg, 6, 13, cmpFunc);
					swap_if(beg, 7, 12, cmpFunc);
					swap_if(beg, 9, 11, cmpFunc);
					swap_if(beg, 0, 14, cmpFunc);
					swap_if(beg, 1, 5, cmpFunc);
					swap_if(beg, 2, 8, cmpFunc);
					swap_if(beg, 3, 7, cmpFunc);
					swap_if(beg, 6, 9, cmpFunc);
					swap_if(beg, 10, 12, cmpFunc);
					swap_if(beg, 11, 13, cmpFunc);
					swap_if(beg, 0, 7, cmpFunc);
					swap_if(beg, 1, 6, cmpFunc);
					swap_if(beg, 2, 9, cmpFunc);
					swap_if(beg, 4, 10, cmpFunc);
					swap_if(beg, 5, 11, cmpFunc);
					swap_if(beg, 8, 13, cmpFunc);
					swap_if(beg, 12, 14, cmpFunc);
					swap_if(beg, 0, 6, cmpFunc);
					swap_if(beg, 2, 4, cmpFunc);
					swap_if(beg, 3, 5, cmpFunc);
					swap_if(beg, 7, 11, cmpFunc);
					swap_if(beg, 8, 10, cmpFunc);
					swap_if(beg, 9, 12, cmpFunc);
					swap_if(beg, 13, 14, cmpFunc);
					swap_if(beg, 0, 3, cmpFunc);
					swap_if(beg, 1, 2, cmpFunc);
					swap_if(beg, 4, 7, cmpFunc);
					swap_if(beg, 5, 9, cmpFunc);
					swap_if(beg, 6, 8, cmpFunc);
					swap_if(beg, 10, 11, cmpFunc);
					swap_if(beg, 12, 13, cmpFunc);
					swap_if(beg, 0, 1, cmpFunc);
					swap_if(beg, 2, 3, cmpFunc);
					swap_if(beg, 4, 6, cmpFunc);
					swap_if(beg, 7, 9, cmpFunc);
					swap_if(beg, 10, 12, cmpFunc);
					swap_if(beg, 11, 13, cmpFunc);
					swap_if(beg, 1, 2, cmpFunc);
					swap_if(beg, 3, 5, cmpFunc);
					swap_if(beg, 8, 10, cmpFunc);
					swap_if(beg, 11, 12, cmpFunc);
					swap_if(beg, 3, 4, cmpFunc);
					swap_if(beg, 5, 6, cmpFunc);
					swap_if(beg, 7, 8, cmpFunc);
					swap_if(beg, 9, 10, cmpFunc);
					swap_if(beg, 2, 3, cmpFunc);
					swap_if(beg, 4, 5, cmpFunc);
					swap_if(beg, 6, 7, cmpFunc);
					swap_if(beg, 8, 9, cmpFunc);
					swap_if(beg, 10, 11, cmpFunc);
					swap_if(beg, 5, 6, cmpFunc);
					swap_if(beg, 7, 8, cmpFunc);
				} else if (n == 16) {
					swap_if(beg, 0, 13, cmpFunc);
					swap_if(beg, 1, 12, cmpFunc);
					swap_if(beg, 2, 15, cmpFunc);
					swap_if(beg, 3, 14, cmpFunc);
					swap_if(beg, 4, 8, cmpFunc);
					swap_if(beg, 5, 6, cmpFunc);
					swap_if(beg, 7, 11, cmpFunc);
					swap_if(beg, 9, 10, cmpFunc);
					swap_if(beg, 0, 5, cmpFunc);
					swap_if(beg, 1, 7, cmpFunc);
					swap_if(beg, 2, 9, cmpFunc);
					swap_if(beg, 3, 4, cmpFunc);
					swap_if(beg, 6, 13, cmpFunc);
					swap_if(beg, 8, 14, cmpFunc);
					swap_if(beg, 10, 15, cmpFunc);
					swap_if(beg, 11, 12, cmpFunc);
					swap_if(beg, 0, 1, cmpFunc);
					swap_if(beg, 2, 3, cmpFunc);
					swap_if(beg, 4, 5, cmpFunc);
					swap_if(beg, 6, 8, cmpFunc);
					swap_if(beg, 7, 9, cmpFunc);
					swap_if(beg, 10, 11, cmpFunc);
					swap_if(beg, 12, 13, cmpFunc);
					swap_if(beg, 14, 15, cmpFunc);
					swap_if(beg, 0, 2, cmpFunc);
					swap_if(beg, 1, 3, cmpFunc);
					swap_if(beg, 4, 10, cmpFunc);
					swap_if(beg, 5, 11, cmpFunc);
					swap_if(beg, 6, 7, cmpFunc);
					swap_if(beg, 8, 9, cmpFunc);
					swap_if(beg, 12, 14, cmpFunc);
					swap_if(beg, 13, 15, cmpFunc);
					swap_if(beg, 1, 2, cmpFunc);
					swap_if(beg, 3, 12, cmpFunc);
					swap_if(beg, 4, 6, cmpFunc);
					swap_if(beg, 5, 7, cmpFunc);
					swap_if(beg, 8, 10, cmpFunc);
					swap_if(beg, 9, 11, cmpFunc);
					swap_if(beg, 13, 14, cmpFunc);
					swap_if(beg, 1, 4, cmpFunc);
					swap_if(beg, 2, 6, cmpFunc);
					swap_if(beg, 5, 8, cmpFunc);
					swap_if(beg, 7, 10, cmpFunc);
					swap_if(beg, 9, 13, cmpFunc);
					swap_if(beg, 11, 14, cmpFunc);
					swap_if(beg, 2, 4, cmpFunc);
					swap_if(beg, 3, 6, cmpFunc);
					swap_if(beg, 9, 12, cmpFunc);
					swap_if(beg, 11, 13, cmpFunc);
					swap_if(beg, 3, 5, cmpFunc);
					swap_if(beg, 6, 8, cmpFunc);
					swap_if(beg, 7, 9, cmpFunc);
					swap_if(beg, 10, 12, cmpFunc);
					swap_if(beg, 3, 4, cmpFunc);
					swap_if(beg, 5, 6, cmpFunc);
					swap_if(beg, 7, 8, cmpFunc);
					swap_if(beg, 9, 10, cmpFunc);
					swap_if(beg, 11, 12, cmpFunc);
					swap_if(beg, 6, 7, cmpFunc);
					swap_if(beg, 8, 9, cmpFunc);
				} else if (n == 17) {
					swap_if(beg, 0, 11, cmpFunc);
					swap_if(beg, 1, 15, cmpFunc);
					swap_if(beg, 2, 10, cmpFunc);
					swap_if(beg, 3, 5, cmpFunc);
					swap_if(beg, 4, 6, cmpFunc);
					swap_if(beg, 8, 12, cmpFunc);
					swap_if(beg, 9, 16, cmpFunc);
					swap_if(beg, 13, 14, cmpFunc);
					swap_if(beg, 0, 6, cmpFunc);
					swap_if(beg, 1, 13, cmpFunc);
					swap_if(beg, 2, 8, cmpFunc);
					swap_if(beg, 4, 14, cmpFunc);
					swap_if(beg, 5, 15, cmpFunc);
					swap_if(beg, 7, 11, cmpFunc);
					swap_if(beg, 0, 8, cmpFunc);
					swap_if(beg, 3, 7, cmpFunc);
					swap_if(beg, 4, 9, cmpFunc);
					swap_if(beg, 6, 16, cmpFunc);
					swap_if(beg, 10, 11, cmpFunc);
					swap_if(beg, 12, 14, cmpFunc);
					swap_if(beg, 0, 2, cmpFunc);
					swap_if(beg, 1, 4, cmpFunc);
					swap_if(beg, 5, 6, cmpFunc);
					swap_if(beg, 7, 13, cmpFunc);
					swap_if(beg, 8, 9, cmpFunc);
					swap_if(beg, 10, 12, cmpFunc);
					swap_if(beg, 11, 14, cmpFunc);
					swap_if(beg, 15, 16, cmpFunc);
					swap_if(beg, 0, 3, cmpFunc);
					swap_if(beg, 2, 5, cmpFunc);
					swap_if(beg, 6, 11, cmpFunc);
					swap_if(beg, 7, 10, cmpFunc);
					swap_if(beg, 9, 13, cmpFunc);
					swap_if(beg, 12, 15, cmpFunc);
					swap_if(beg, 14, 16, cmpFunc);
					swap_if(beg, 0, 1, cmpFunc);
					swap_if(beg, 3, 4, cmpFunc);
					swap_if(beg, 5, 10, cmpFunc);
					swap_if(beg, 6, 9, cmpFunc);
					swap_if(beg, 7, 8, cmpFunc);
					swap_if(beg, 11, 15, cmpFunc);
					swap_if(beg, 13, 14, cmpFunc);
					swap_if(beg, 1, 2, cmpFunc);
					swap_if(beg, 3, 7, cmpFunc);
					swap_if(beg, 4, 8, cmpFunc);
					swap_if(beg, 6, 12, cmpFunc);
					swap_if(beg, 11, 13, cmpFunc);
					swap_if(beg, 14, 15, cmpFunc);
					swap_if(beg, 1, 3, cmpFunc);
					swap_if(beg, 2, 7, cmpFunc);
					swap_if(beg, 4, 5, cmpFunc);
					swap_if(beg, 9, 11, cmpFunc);
					swap_if(beg, 10, 12, cmpFunc);
					swap_if(beg, 13, 14, cmpFunc);
					swap_if(beg, 2, 3, cmpFunc);
					swap_if(beg, 4, 6, cmpFunc);
					swap_if(beg, 5, 7, cmpFunc);
					swap_if(beg, 8, 10, cmpFunc);
					swap_if(beg, 3, 4, cmpFunc);
					swap_if(beg, 6, 8, cmpFunc);
					swap_if(beg, 7, 9, cmpFunc);
					swap_if(beg, 10, 12, cmpFunc);
					swap_if(beg, 5, 6, cmpFunc);
					swap_if(beg, 7, 8, cmpFunc);
					swap_if(beg, 9, 10, cmpFunc);
					swap_if(beg, 11, 12, cmpFunc);
					swap_if(beg, 4, 5, cmpFunc);
					swap_if(beg, 6, 7, cmpFunc);
					swap_if(beg, 8, 9, cmpFunc);
					swap_if(beg, 10, 11, cmpFunc);
					swap_if(beg, 12, 13, cmpFunc);
				} else if (n <= 32) {
					insertion_sort_range(beg, 0, (int)n - 1, cmpFunc, [beg](uint32_t lhs, uint32_t rhs) {
						core::swap(beg[lhs], beg[rhs]);
					});
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
				} else if (n == 11) {
					try_swap_if(beg, 0, 9, cmpFunc, swapFunc);
					try_swap_if(beg, 1, 6, cmpFunc, swapFunc);
					try_swap_if(beg, 2, 4, cmpFunc, swapFunc);
					try_swap_if(beg, 3, 7, cmpFunc, swapFunc);
					try_swap_if(beg, 5, 8, cmpFunc, swapFunc);
					try_swap_if(beg, 0, 1, cmpFunc, swapFunc);
					try_swap_if(beg, 3, 5, cmpFunc, swapFunc);
					try_swap_if(beg, 4, 10, cmpFunc, swapFunc);
					try_swap_if(beg, 6, 9, cmpFunc, swapFunc);
					try_swap_if(beg, 7, 8, cmpFunc, swapFunc);
					try_swap_if(beg, 1, 3, cmpFunc, swapFunc);
					try_swap_if(beg, 2, 5, cmpFunc, swapFunc);
					try_swap_if(beg, 4, 7, cmpFunc, swapFunc);
					try_swap_if(beg, 8, 10, cmpFunc, swapFunc);
					try_swap_if(beg, 0, 4, cmpFunc, swapFunc);
					try_swap_if(beg, 1, 2, cmpFunc, swapFunc);
					try_swap_if(beg, 3, 7, cmpFunc, swapFunc);
					try_swap_if(beg, 5, 9, cmpFunc, swapFunc);
					try_swap_if(beg, 6, 8, cmpFunc, swapFunc);
					try_swap_if(beg, 0, 1, cmpFunc, swapFunc);
					try_swap_if(beg, 2, 6, cmpFunc, swapFunc);
					try_swap_if(beg, 4, 5, cmpFunc, swapFunc);
					try_swap_if(beg, 7, 8, cmpFunc, swapFunc);
					try_swap_if(beg, 9, 10, cmpFunc, swapFunc);
					try_swap_if(beg, 2, 4, cmpFunc, swapFunc);
					try_swap_if(beg, 3, 6, cmpFunc, swapFunc);
					try_swap_if(beg, 5, 7, cmpFunc, swapFunc);
					try_swap_if(beg, 8, 9, cmpFunc, swapFunc);
					try_swap_if(beg, 1, 2, cmpFunc, swapFunc);
					try_swap_if(beg, 3, 4, cmpFunc, swapFunc);
					try_swap_if(beg, 5, 6, cmpFunc, swapFunc);
					try_swap_if(beg, 7, 8, cmpFunc, swapFunc);
					try_swap_if(beg, 2, 3, cmpFunc, swapFunc);
					try_swap_if(beg, 4, 5, cmpFunc, swapFunc);
					try_swap_if(beg, 6, 7, cmpFunc, swapFunc);
				} else if (n == 12) {
					try_swap_if(beg, 0, 8, cmpFunc, swapFunc);
					try_swap_if(beg, 1, 7, cmpFunc, swapFunc);
					try_swap_if(beg, 2, 6, cmpFunc, swapFunc);
					try_swap_if(beg, 3, 11, cmpFunc, swapFunc);
					try_swap_if(beg, 4, 10, cmpFunc, swapFunc);
					try_swap_if(beg, 5, 9, cmpFunc, swapFunc);
					try_swap_if(beg, 0, 1, cmpFunc, swapFunc);
					try_swap_if(beg, 2, 5, cmpFunc, swapFunc);
					try_swap_if(beg, 3, 4, cmpFunc, swapFunc);
					try_swap_if(beg, 6, 9, cmpFunc, swapFunc);
					try_swap_if(beg, 7, 8, cmpFunc, swapFunc);
					try_swap_if(beg, 10, 11, cmpFunc, swapFunc);
					try_swap_if(beg, 0, 2, cmpFunc, swapFunc);
					try_swap_if(beg, 1, 6, cmpFunc, swapFunc);
					try_swap_if(beg, 5, 10, cmpFunc, swapFunc);
					try_swap_if(beg, 9, 11, cmpFunc, swapFunc);
					try_swap_if(beg, 0, 3, cmpFunc, swapFunc);
					try_swap_if(beg, 1, 2, cmpFunc, swapFunc);
					try_swap_if(beg, 4, 6, cmpFunc, swapFunc);
					try_swap_if(beg, 5, 7, cmpFunc, swapFunc);
					try_swap_if(beg, 8, 11, cmpFunc, swapFunc);
					try_swap_if(beg, 9, 10, cmpFunc, swapFunc);
					try_swap_if(beg, 1, 4, cmpFunc, swapFunc);
					try_swap_if(beg, 3, 5, cmpFunc, swapFunc);
					try_swap_if(beg, 6, 8, cmpFunc, swapFunc);
					try_swap_if(beg, 7, 10, cmpFunc, swapFunc);
					try_swap_if(beg, 1, 3, cmpFunc, swapFunc);
					try_swap_if(beg, 2, 5, cmpFunc, swapFunc);
					try_swap_if(beg, 6, 9, cmpFunc, swapFunc);
					try_swap_if(beg, 8, 10, cmpFunc, swapFunc);
					try_swap_if(beg, 2, 3, cmpFunc, swapFunc);
					try_swap_if(beg, 4, 5, cmpFunc, swapFunc);
					try_swap_if(beg, 6, 7, cmpFunc, swapFunc);
					try_swap_if(beg, 8, 9, cmpFunc, swapFunc);
					try_swap_if(beg, 4, 6, cmpFunc, swapFunc);
					try_swap_if(beg, 5, 7, cmpFunc, swapFunc);
					try_swap_if(beg, 3, 4, cmpFunc, swapFunc);
					try_swap_if(beg, 5, 6, cmpFunc, swapFunc);
					try_swap_if(beg, 7, 8, cmpFunc, swapFunc);
				} else if (n == 13) {
					try_swap_if(beg, 0, 12, cmpFunc, swapFunc);
					try_swap_if(beg, 1, 10, cmpFunc, swapFunc);
					try_swap_if(beg, 2, 9, cmpFunc, swapFunc);
					try_swap_if(beg, 3, 7, cmpFunc, swapFunc);
					try_swap_if(beg, 5, 11, cmpFunc, swapFunc);
					try_swap_if(beg, 6, 8, cmpFunc, swapFunc);
					try_swap_if(beg, 1, 6, cmpFunc, swapFunc);
					try_swap_if(beg, 2, 3, cmpFunc, swapFunc);
					try_swap_if(beg, 4, 11, cmpFunc, swapFunc);
					try_swap_if(beg, 7, 9, cmpFunc, swapFunc);
					try_swap_if(beg, 8, 10, cmpFunc, swapFunc);
					try_swap_if(beg, 0, 4, cmpFunc, swapFunc);
					try_swap_if(beg, 1, 2, cmpFunc, swapFunc);
					try_swap_if(beg, 3, 6, cmpFunc, swapFunc);
					try_swap_if(beg, 7, 8, cmpFunc, swapFunc);
					try_swap_if(beg, 9, 10, cmpFunc, swapFunc);
					try_swap_if(beg, 11, 12, cmpFunc, swapFunc);
					try_swap_if(beg, 4, 6, cmpFunc, swapFunc);
					try_swap_if(beg, 5, 9, cmpFunc, swapFunc);
					try_swap_if(beg, 8, 11, cmpFunc, swapFunc);
					try_swap_if(beg, 10, 12, cmpFunc, swapFunc);
					try_swap_if(beg, 0, 5, cmpFunc, swapFunc);
					try_swap_if(beg, 3, 8, cmpFunc, swapFunc);
					try_swap_if(beg, 4, 7, cmpFunc, swapFunc);
					try_swap_if(beg, 6, 11, cmpFunc, swapFunc);
					try_swap_if(beg, 9, 10, cmpFunc, swapFunc);
					try_swap_if(beg, 0, 1, cmpFunc, swapFunc);
					try_swap_if(beg, 2, 5, cmpFunc, swapFunc);
					try_swap_if(beg, 6, 9, cmpFunc, swapFunc);
					try_swap_if(beg, 7, 8, cmpFunc, swapFunc);
					try_swap_if(beg, 10, 11, cmpFunc, swapFunc);
					try_swap_if(beg, 1, 3, cmpFunc, swapFunc);
					try_swap_if(beg, 2, 4, cmpFunc, swapFunc);
					try_swap_if(beg, 5, 6, cmpFunc, swapFunc);
					try_swap_if(beg, 9, 10, cmpFunc, swapFunc);
					try_swap_if(beg, 1, 2, cmpFunc, swapFunc);
					try_swap_if(beg, 3, 4, cmpFunc, swapFunc);
					try_swap_if(beg, 5, 7, cmpFunc, swapFunc);
					try_swap_if(beg, 6, 8, cmpFunc, swapFunc);
					try_swap_if(beg, 2, 3, cmpFunc, swapFunc);
					try_swap_if(beg, 4, 5, cmpFunc, swapFunc);
					try_swap_if(beg, 6, 7, cmpFunc, swapFunc);
					try_swap_if(beg, 8, 9, cmpFunc, swapFunc);
					try_swap_if(beg, 3, 4, cmpFunc, swapFunc);
					try_swap_if(beg, 5, 6, cmpFunc, swapFunc);
				} else if (n == 14) {
					try_swap_if(beg, 0, 1, cmpFunc, swapFunc);
					try_swap_if(beg, 2, 3, cmpFunc, swapFunc);
					try_swap_if(beg, 4, 5, cmpFunc, swapFunc);
					try_swap_if(beg, 6, 7, cmpFunc, swapFunc);
					try_swap_if(beg, 8, 9, cmpFunc, swapFunc);
					try_swap_if(beg, 10, 11, cmpFunc, swapFunc);
					try_swap_if(beg, 12, 13, cmpFunc, swapFunc);
					try_swap_if(beg, 0, 2, cmpFunc, swapFunc);
					try_swap_if(beg, 1, 3, cmpFunc, swapFunc);
					try_swap_if(beg, 4, 8, cmpFunc, swapFunc);
					try_swap_if(beg, 5, 9, cmpFunc, swapFunc);
					try_swap_if(beg, 10, 12, cmpFunc, swapFunc);
					try_swap_if(beg, 11, 13, cmpFunc, swapFunc);
					try_swap_if(beg, 0, 4, cmpFunc, swapFunc);
					try_swap_if(beg, 1, 2, cmpFunc, swapFunc);
					try_swap_if(beg, 3, 7, cmpFunc, swapFunc);
					try_swap_if(beg, 5, 8, cmpFunc, swapFunc);
					try_swap_if(beg, 6, 10, cmpFunc, swapFunc);
					try_swap_if(beg, 9, 13, cmpFunc, swapFunc);
					try_swap_if(beg, 11, 12, cmpFunc, swapFunc);
					try_swap_if(beg, 0, 6, cmpFunc, swapFunc);
					try_swap_if(beg, 1, 5, cmpFunc, swapFunc);
					try_swap_if(beg, 3, 9, cmpFunc, swapFunc);
					try_swap_if(beg, 4, 10, cmpFunc, swapFunc);
					try_swap_if(beg, 7, 13, cmpFunc, swapFunc);
					try_swap_if(beg, 8, 12, cmpFunc, swapFunc);
					try_swap_if(beg, 2, 10, cmpFunc, swapFunc);
					try_swap_if(beg, 3, 11, cmpFunc, swapFunc);
					try_swap_if(beg, 4, 6, cmpFunc, swapFunc);
					try_swap_if(beg, 7, 9, cmpFunc, swapFunc);
					try_swap_if(beg, 1, 3, cmpFunc, swapFunc);
					try_swap_if(beg, 2, 8, cmpFunc, swapFunc);
					try_swap_if(beg, 5, 11, cmpFunc, swapFunc);
					try_swap_if(beg, 6, 7, cmpFunc, swapFunc);
					try_swap_if(beg, 10, 12, cmpFunc, swapFunc);
					try_swap_if(beg, 1, 4, cmpFunc, swapFunc);
					try_swap_if(beg, 2, 6, cmpFunc, swapFunc);
					try_swap_if(beg, 3, 5, cmpFunc, swapFunc);
					try_swap_if(beg, 7, 11, cmpFunc, swapFunc);
					try_swap_if(beg, 8, 10, cmpFunc, swapFunc);
					try_swap_if(beg, 9, 12, cmpFunc, swapFunc);
					try_swap_if(beg, 2, 4, cmpFunc, swapFunc);
					try_swap_if(beg, 3, 6, cmpFunc, swapFunc);
					try_swap_if(beg, 5, 8, cmpFunc, swapFunc);
					try_swap_if(beg, 7, 10, cmpFunc, swapFunc);
					try_swap_if(beg, 9, 11, cmpFunc, swapFunc);
					try_swap_if(beg, 3, 4, cmpFunc, swapFunc);
					try_swap_if(beg, 5, 6, cmpFunc, swapFunc);
					try_swap_if(beg, 7, 8, cmpFunc, swapFunc);
					try_swap_if(beg, 9, 10, cmpFunc, swapFunc);
					try_swap_if(beg, 6, 7, cmpFunc, swapFunc);
				} else if (n == 15) {
					try_swap_if(beg, 1, 2, cmpFunc, swapFunc);
					try_swap_if(beg, 3, 10, cmpFunc, swapFunc);
					try_swap_if(beg, 4, 14, cmpFunc, swapFunc);
					try_swap_if(beg, 5, 8, cmpFunc, swapFunc);
					try_swap_if(beg, 6, 13, cmpFunc, swapFunc);
					try_swap_if(beg, 7, 12, cmpFunc, swapFunc);
					try_swap_if(beg, 9, 11, cmpFunc, swapFunc);
					try_swap_if(beg, 0, 14, cmpFunc, swapFunc);
					try_swap_if(beg, 1, 5, cmpFunc, swapFunc);
					try_swap_if(beg, 2, 8, cmpFunc, swapFunc);
					try_swap_if(beg, 3, 7, cmpFunc, swapFunc);
					try_swap_if(beg, 6, 9, cmpFunc, swapFunc);
					try_swap_if(beg, 10, 12, cmpFunc, swapFunc);
					try_swap_if(beg, 11, 13, cmpFunc, swapFunc);
					try_swap_if(beg, 0, 7, cmpFunc, swapFunc);
					try_swap_if(beg, 1, 6, cmpFunc, swapFunc);
					try_swap_if(beg, 2, 9, cmpFunc, swapFunc);
					try_swap_if(beg, 4, 10, cmpFunc, swapFunc);
					try_swap_if(beg, 5, 11, cmpFunc, swapFunc);
					try_swap_if(beg, 8, 13, cmpFunc, swapFunc);
					try_swap_if(beg, 12, 14, cmpFunc, swapFunc);
					try_swap_if(beg, 0, 6, cmpFunc, swapFunc);
					try_swap_if(beg, 2, 4, cmpFunc, swapFunc);
					try_swap_if(beg, 3, 5, cmpFunc, swapFunc);
					try_swap_if(beg, 7, 11, cmpFunc, swapFunc);
					try_swap_if(beg, 8, 10, cmpFunc, swapFunc);
					try_swap_if(beg, 9, 12, cmpFunc, swapFunc);
					try_swap_if(beg, 13, 14, cmpFunc, swapFunc);
					try_swap_if(beg, 0, 3, cmpFunc, swapFunc);
					try_swap_if(beg, 1, 2, cmpFunc, swapFunc);
					try_swap_if(beg, 4, 7, cmpFunc, swapFunc);
					try_swap_if(beg, 5, 9, cmpFunc, swapFunc);
					try_swap_if(beg, 6, 8, cmpFunc, swapFunc);
					try_swap_if(beg, 10, 11, cmpFunc, swapFunc);
					try_swap_if(beg, 12, 13, cmpFunc, swapFunc);
					try_swap_if(beg, 0, 1, cmpFunc, swapFunc);
					try_swap_if(beg, 2, 3, cmpFunc, swapFunc);
					try_swap_if(beg, 4, 6, cmpFunc, swapFunc);
					try_swap_if(beg, 7, 9, cmpFunc, swapFunc);
					try_swap_if(beg, 10, 12, cmpFunc, swapFunc);
					try_swap_if(beg, 11, 13, cmpFunc, swapFunc);
					try_swap_if(beg, 1, 2, cmpFunc, swapFunc);
					try_swap_if(beg, 3, 5, cmpFunc, swapFunc);
					try_swap_if(beg, 8, 10, cmpFunc, swapFunc);
					try_swap_if(beg, 11, 12, cmpFunc, swapFunc);
					try_swap_if(beg, 3, 4, cmpFunc, swapFunc);
					try_swap_if(beg, 5, 6, cmpFunc, swapFunc);
					try_swap_if(beg, 7, 8, cmpFunc, swapFunc);
					try_swap_if(beg, 9, 10, cmpFunc, swapFunc);
					try_swap_if(beg, 2, 3, cmpFunc, swapFunc);
					try_swap_if(beg, 4, 5, cmpFunc, swapFunc);
					try_swap_if(beg, 6, 7, cmpFunc, swapFunc);
					try_swap_if(beg, 8, 9, cmpFunc, swapFunc);
					try_swap_if(beg, 10, 11, cmpFunc, swapFunc);
					try_swap_if(beg, 5, 6, cmpFunc, swapFunc);
					try_swap_if(beg, 7, 8, cmpFunc, swapFunc);
				} else if (n == 16) {
					try_swap_if(beg, 0, 13, cmpFunc, swapFunc);
					try_swap_if(beg, 1, 12, cmpFunc, swapFunc);
					try_swap_if(beg, 2, 15, cmpFunc, swapFunc);
					try_swap_if(beg, 3, 14, cmpFunc, swapFunc);
					try_swap_if(beg, 4, 8, cmpFunc, swapFunc);
					try_swap_if(beg, 5, 6, cmpFunc, swapFunc);
					try_swap_if(beg, 7, 11, cmpFunc, swapFunc);
					try_swap_if(beg, 9, 10, cmpFunc, swapFunc);
					try_swap_if(beg, 0, 5, cmpFunc, swapFunc);
					try_swap_if(beg, 1, 7, cmpFunc, swapFunc);
					try_swap_if(beg, 2, 9, cmpFunc, swapFunc);
					try_swap_if(beg, 3, 4, cmpFunc, swapFunc);
					try_swap_if(beg, 6, 13, cmpFunc, swapFunc);
					try_swap_if(beg, 8, 14, cmpFunc, swapFunc);
					try_swap_if(beg, 10, 15, cmpFunc, swapFunc);
					try_swap_if(beg, 11, 12, cmpFunc, swapFunc);
					try_swap_if(beg, 0, 1, cmpFunc, swapFunc);
					try_swap_if(beg, 2, 3, cmpFunc, swapFunc);
					try_swap_if(beg, 4, 5, cmpFunc, swapFunc);
					try_swap_if(beg, 6, 8, cmpFunc, swapFunc);
					try_swap_if(beg, 7, 9, cmpFunc, swapFunc);
					try_swap_if(beg, 10, 11, cmpFunc, swapFunc);
					try_swap_if(beg, 12, 13, cmpFunc, swapFunc);
					try_swap_if(beg, 14, 15, cmpFunc, swapFunc);
					try_swap_if(beg, 0, 2, cmpFunc, swapFunc);
					try_swap_if(beg, 1, 3, cmpFunc, swapFunc);
					try_swap_if(beg, 4, 10, cmpFunc, swapFunc);
					try_swap_if(beg, 5, 11, cmpFunc, swapFunc);
					try_swap_if(beg, 6, 7, cmpFunc, swapFunc);
					try_swap_if(beg, 8, 9, cmpFunc, swapFunc);
					try_swap_if(beg, 12, 14, cmpFunc, swapFunc);
					try_swap_if(beg, 13, 15, cmpFunc, swapFunc);
					try_swap_if(beg, 1, 2, cmpFunc, swapFunc);
					try_swap_if(beg, 3, 12, cmpFunc, swapFunc);
					try_swap_if(beg, 4, 6, cmpFunc, swapFunc);
					try_swap_if(beg, 5, 7, cmpFunc, swapFunc);
					try_swap_if(beg, 8, 10, cmpFunc, swapFunc);
					try_swap_if(beg, 9, 11, cmpFunc, swapFunc);
					try_swap_if(beg, 13, 14, cmpFunc, swapFunc);
					try_swap_if(beg, 1, 4, cmpFunc, swapFunc);
					try_swap_if(beg, 2, 6, cmpFunc, swapFunc);
					try_swap_if(beg, 5, 8, cmpFunc, swapFunc);
					try_swap_if(beg, 7, 10, cmpFunc, swapFunc);
					try_swap_if(beg, 9, 13, cmpFunc, swapFunc);
					try_swap_if(beg, 11, 14, cmpFunc, swapFunc);
					try_swap_if(beg, 2, 4, cmpFunc, swapFunc);
					try_swap_if(beg, 3, 6, cmpFunc, swapFunc);
					try_swap_if(beg, 9, 12, cmpFunc, swapFunc);
					try_swap_if(beg, 11, 13, cmpFunc, swapFunc);
					try_swap_if(beg, 3, 5, cmpFunc, swapFunc);
					try_swap_if(beg, 6, 8, cmpFunc, swapFunc);
					try_swap_if(beg, 7, 9, cmpFunc, swapFunc);
					try_swap_if(beg, 10, 12, cmpFunc, swapFunc);
					try_swap_if(beg, 3, 4, cmpFunc, swapFunc);
					try_swap_if(beg, 5, 6, cmpFunc, swapFunc);
					try_swap_if(beg, 7, 8, cmpFunc, swapFunc);
					try_swap_if(beg, 9, 10, cmpFunc, swapFunc);
					try_swap_if(beg, 11, 12, cmpFunc, swapFunc);
					try_swap_if(beg, 6, 7, cmpFunc, swapFunc);
					try_swap_if(beg, 8, 9, cmpFunc, swapFunc);
				} else if (n == 17) {
					try_swap_if(beg, 0, 11, cmpFunc, swapFunc);
					try_swap_if(beg, 1, 15, cmpFunc, swapFunc);
					try_swap_if(beg, 2, 10, cmpFunc, swapFunc);
					try_swap_if(beg, 3, 5, cmpFunc, swapFunc);
					try_swap_if(beg, 4, 6, cmpFunc, swapFunc);
					try_swap_if(beg, 8, 12, cmpFunc, swapFunc);
					try_swap_if(beg, 9, 16, cmpFunc, swapFunc);
					try_swap_if(beg, 13, 14, cmpFunc, swapFunc);
					try_swap_if(beg, 0, 6, cmpFunc, swapFunc);
					try_swap_if(beg, 1, 13, cmpFunc, swapFunc);
					try_swap_if(beg, 2, 8, cmpFunc, swapFunc);
					try_swap_if(beg, 4, 14, cmpFunc, swapFunc);
					try_swap_if(beg, 5, 15, cmpFunc, swapFunc);
					try_swap_if(beg, 7, 11, cmpFunc, swapFunc);
					try_swap_if(beg, 0, 8, cmpFunc, swapFunc);
					try_swap_if(beg, 3, 7, cmpFunc, swapFunc);
					try_swap_if(beg, 4, 9, cmpFunc, swapFunc);
					try_swap_if(beg, 6, 16, cmpFunc, swapFunc);
					try_swap_if(beg, 10, 11, cmpFunc, swapFunc);
					try_swap_if(beg, 12, 14, cmpFunc, swapFunc);
					try_swap_if(beg, 0, 2, cmpFunc, swapFunc);
					try_swap_if(beg, 1, 4, cmpFunc, swapFunc);
					try_swap_if(beg, 5, 6, cmpFunc, swapFunc);
					try_swap_if(beg, 7, 13, cmpFunc, swapFunc);
					try_swap_if(beg, 8, 9, cmpFunc, swapFunc);
					try_swap_if(beg, 10, 12, cmpFunc, swapFunc);
					try_swap_if(beg, 11, 14, cmpFunc, swapFunc);
					try_swap_if(beg, 15, 16, cmpFunc, swapFunc);
					try_swap_if(beg, 0, 3, cmpFunc, swapFunc);
					try_swap_if(beg, 2, 5, cmpFunc, swapFunc);
					try_swap_if(beg, 6, 11, cmpFunc, swapFunc);
					try_swap_if(beg, 7, 10, cmpFunc, swapFunc);
					try_swap_if(beg, 9, 13, cmpFunc, swapFunc);
					try_swap_if(beg, 12, 15, cmpFunc, swapFunc);
					try_swap_if(beg, 14, 16, cmpFunc, swapFunc);
					try_swap_if(beg, 0, 1, cmpFunc, swapFunc);
					try_swap_if(beg, 3, 4, cmpFunc, swapFunc);
					try_swap_if(beg, 5, 10, cmpFunc, swapFunc);
					try_swap_if(beg, 6, 9, cmpFunc, swapFunc);
					try_swap_if(beg, 7, 8, cmpFunc, swapFunc);
					try_swap_if(beg, 11, 15, cmpFunc, swapFunc);
					try_swap_if(beg, 13, 14, cmpFunc, swapFunc);
					try_swap_if(beg, 1, 2, cmpFunc, swapFunc);
					try_swap_if(beg, 3, 7, cmpFunc, swapFunc);
					try_swap_if(beg, 4, 8, cmpFunc, swapFunc);
					try_swap_if(beg, 6, 12, cmpFunc, swapFunc);
					try_swap_if(beg, 11, 13, cmpFunc, swapFunc);
					try_swap_if(beg, 14, 15, cmpFunc, swapFunc);
					try_swap_if(beg, 1, 3, cmpFunc, swapFunc);
					try_swap_if(beg, 2, 7, cmpFunc, swapFunc);
					try_swap_if(beg, 4, 5, cmpFunc, swapFunc);
					try_swap_if(beg, 9, 11, cmpFunc, swapFunc);
					try_swap_if(beg, 10, 12, cmpFunc, swapFunc);
					try_swap_if(beg, 13, 14, cmpFunc, swapFunc);
					try_swap_if(beg, 2, 3, cmpFunc, swapFunc);
					try_swap_if(beg, 4, 6, cmpFunc, swapFunc);
					try_swap_if(beg, 5, 7, cmpFunc, swapFunc);
					try_swap_if(beg, 8, 10, cmpFunc, swapFunc);
					try_swap_if(beg, 3, 4, cmpFunc, swapFunc);
					try_swap_if(beg, 6, 8, cmpFunc, swapFunc);
					try_swap_if(beg, 7, 9, cmpFunc, swapFunc);
					try_swap_if(beg, 10, 12, cmpFunc, swapFunc);
					try_swap_if(beg, 5, 6, cmpFunc, swapFunc);
					try_swap_if(beg, 7, 8, cmpFunc, swapFunc);
					try_swap_if(beg, 9, 10, cmpFunc, swapFunc);
					try_swap_if(beg, 11, 12, cmpFunc, swapFunc);
					try_swap_if(beg, 4, 5, cmpFunc, swapFunc);
					try_swap_if(beg, 6, 7, cmpFunc, swapFunc);
					try_swap_if(beg, 8, 9, cmpFunc, swapFunc);
					try_swap_if(beg, 10, 11, cmpFunc, swapFunc);
					try_swap_if(beg, 12, 13, cmpFunc, swapFunc);
				} else if (n <= 32) {
					insertion_sort_range(beg, 0, (int)n - 1, cmpFunc, swapFunc);
				}

				return n <= 32;
			}
		} // namespace detail

		template <typename Container, typename TCmpFunc, typename TSwapFunc>
		void quick_sort(Container& arr, int low, int high, TCmpFunc cmpFunc, TSwapFunc swapFunc);

		//! Recursively quick-sorts the index range [`low`, `high`] in @a arr.
		//! \tparam Container Container type.
		//! \tparam TCmpFunc Comparison functor type.
		//! \param arr Container to sort.
		//! \param low First index in the range.
		//! \param high Last index in the range.
		//! \param cmpFunc Ordering predicate.
		template <typename Container, typename TCmpFunc>
		void quick_sort(Container& arr, int low, int high, TCmpFunc cmpFunc) {
			if (low >= high)
				return;
			detail::quick_sort_impl(arr, low, high, cmpFunc, detail::sort_depth_limit((uint32_t)(high - low + 1)));
		}

		//! Recursively quick-sorts the index range [`low`, `high`] in @a arr using a custom swap callback.
		//! \tparam Container Container type.
		//! \tparam TCmpFunc Comparison functor type.
		//! \tparam TSwapFunc Swap callback type.
		//! \param arr Container to sort.
		//! \param low First index in the range.
		//! \param high Last index in the range.
		//! \param cmpFunc Ordering predicate.
		//! \param swapFunc Callback performing the actual swap.
		template <typename Container, typename TCmpFunc, typename TSwapFunc>
		void quick_sort(Container& arr, int low, int high, TCmpFunc cmpFunc, TSwapFunc swapFunc) {
			if (low >= high)
				return;
			detail::quick_sort_impl(arr, low, high, cmpFunc, swapFunc, detail::sort_depth_limit((uint32_t)(high - low + 1)));
		}

		//! Sorts a range of elements.
		//! Sorting networks are used up to 17 elements. Insertion sort is used up to 32 elements.
		//! For larger ranges, introsort-style quicksort is used with heapsort fallback.
		//! Use when it is necessary to sort multiple arrays at once.
		//! \tparam T Element type
		//! \tparam TCmpFunc Functor type for comparison: bool cmpFunc(i, j)
		//! \param beg Pointer to first element
		//! \param end Pointer to one-past-last element
		//! \param cmpFunc Comparision function
		template <typename T, typename TCmpFunc>
		void sort(T* beg, T* end, TCmpFunc cmpFunc) {
			const auto n = (uintptr_t)(end - beg);
			if (detail::try_sorted_or_reversed(beg, (uint32_t)n, cmpFunc, [beg](uint32_t lhs, uint32_t rhs) {
						core::swap(beg[lhs], beg[rhs]);
					}))
				return;
			if (detail::sort_nwk(beg, end, cmpFunc))
				return;

			quick_sort(beg, 0, (int)n - 1, cmpFunc);
		}

		//! Sorts all elements in @a c using @a cmpFunc.
		//! \tparam C Container type.
		//! \tparam TCmpFunc Comparison functor type.
		//! \param c Container to sort.
		//! \param cmpFunc Ordering predicate.
		template <typename C, typename TCmpFunc>
		void sort(C& c, TCmpFunc cmpFunc) {
			sort(c.begin(), c.end(), cmpFunc);
		}

		//! Sorts a range of elements given a comparison function @a cmpFunc.
		//! If @a cmpFunc returns true it performs @a swapFunc which can perform the sorting.
		//! Sorting networks are used up to 17 elements. Insertion sort is used up to 32 elements.
		//! For larger ranges, introsort-style quicksort is used with heapsort fallback.
		//! Use when it is necessary to sort multiple arrays at once.
		//! \tparam T Element type
		//! \tparam TCmpFunc Functor type for comparison: bool cmpFunc(i, j)
		//! \tparam TSwapFunc Functor type for swapping: void swapFunc(i, j)
		//! \param beg Pointer to first element
		//! \param end Pointer to one-past-last element
		//! \param cmpFunc Comparision function
		//! \param swapFunc Sorting function
		template <typename T, typename TCmpFunc, typename TSwapFunc>
		void sort(T* beg, T* end, TCmpFunc cmpFunc, TSwapFunc swapFunc) {
			const auto n = (uintptr_t)(end - beg);
			if (detail::try_sorted_or_reversed(beg, (uint32_t)n, cmpFunc, swapFunc))
				return;
			if (detail::sort_nwk(beg, end, cmpFunc, swapFunc))
				return;

			quick_sort(beg, 0, (int)n - 1, cmpFunc, swapFunc);
		}

		//! Sorts all elements in @a c using @a cmpFunc and a custom swap callback.
		//! \tparam C Container type.
		//! \tparam TCmpFunc Comparison functor type.
		//! \tparam TSwapFunc Swap callback type.
		//! \param c Container to sort.
		//! \param cmpFunc Ordering predicate.
		//! \param swapFunc Callback performing the actual swap.
		template <typename C, typename TCmpFunc, typename TSwapFunc>
		void sort(C& c, TCmpFunc cmpFunc, TSwapFunc swapFunc) {
			sort(c.begin(), c.end(), cmpFunc, swapFunc);
		}
	} // namespace core
} // namespace gaia
