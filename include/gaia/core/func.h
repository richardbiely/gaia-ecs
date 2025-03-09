#pragma once
#include "../config/config.h"
#include <type_traits>

namespace gaia {
	namespace detail {
		template <class T>
		struct is_reference_wrapper: std::false_type {};
		template <class U>
		struct is_reference_wrapper<std::reference_wrapper<U>>: std::true_type {};

		template <class C, class Pointed, class Object, class... Args>
		constexpr decltype(auto) invoke_memptr(Pointed C::* member, Object&& object, Args&&... args) {
			using object_t = std::decay_t<Object>;
			constexpr bool is_member_function = std::is_function_v<Pointed>;
			constexpr bool is_wrapped = is_reference_wrapper<object_t>::value;
			constexpr bool is_derived_object = std::is_same_v<C, object_t> || std::is_base_of_v<C, object_t>;

			if constexpr (is_member_function) {
				if constexpr (is_derived_object)
					return (GAIA_FWD(object).*member)(GAIA_FWD(args)...);
				else if constexpr (is_wrapped)
					return (object.get().*member)(GAIA_FWD(args)...);
				else
					return ((*GAIA_FWD(object)).*member)(GAIA_FWD(args)...);
			} else {
				static_assert(std::is_object_v<Pointed> && sizeof...(args) == 0);
				if constexpr (is_derived_object)
					return GAIA_FWD(object).*member;
				else if constexpr (is_wrapped)
					return object.get().*member;
				else
					return (*GAIA_FWD(object)).*member;
			}
		}
	} // namespace detail

	template <class F, class... Args>
	constexpr decltype(auto)
	invoke(F&& f, Args&&... args) noexcept(std::is_nothrow_invocable_v<F, Args...>) {
		if constexpr (std::is_member_pointer_v<std::decay_t<F>>)
			return detail::invoke_memptr(f, GAIA_FWD(args)...);
		else
			return GAIA_FWD(f)(GAIA_FWD(args)...);
	}
} // namespace gaia