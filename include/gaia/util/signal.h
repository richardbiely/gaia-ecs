#pragma once
#include "../config/config.h"

// TODO: Needed only for std::invoke. Possibly replace it somehow?
#include <functional>

#include <tuple>
#include <typeinfo>
#include <utility>

#include "../cnt/darray.h"
#include "../core/utility.h"

namespace gaia {
	namespace util {
		namespace detail {
			template <typename Ret, typename... Args>
			auto func_ptr(Ret (*)(Args...)) -> Ret (*)(Args...);

			template <typename Ret, typename Type, typename... Args, typename Other>
			auto func_ptr(Ret (*)(Type, Args...), Other&&) -> Ret (*)(Args...);

			template <typename Class, typename Ret, typename... Args, typename... Other>
			auto func_ptr(Ret (Class::*)(Args...), Other&&...) -> Ret (*)(Args...);

			template <typename Class, typename Ret, typename... Args, typename... Other>
			auto func_ptr(Ret (Class::*)(Args...) const, Other&&...) -> Ret (*)(Args...);

			template <typename Class, typename Type, typename... Other>
			auto func_ptr(Type Class::*, Other&&...) -> Type (*)();

			template <typename... Type>
			using func_ptr_t = decltype(detail::func_ptr(std::declval<Type>()...));

			template <typename... Class, typename Ret, typename... Args>
			GAIA_NODISCARD constexpr auto index_sequence_for(Ret (*)(Args...)) {
				return std::index_sequence_for<Class..., Args...>{};
			}

			// Wraps a function or a member of a specified type
			template <auto>
			struct connect_arg_t {};
			// Function wrapper
			template <auto Func>
			inline constexpr connect_arg_t<Func> connect_arg{};
		} // namespace detail

		template <typename>
		class delegate;
		template <typename Function>
		class signal;
		template <typename Function>
		class sink;

		//------------------------------------------------------------------------------
		// delegate
		//------------------------------------------------------------------------------

		//! Delegate for function pointers and members.
		//! It can be used as a general-purpose invoker for any free function with no memory overhead.
		//! \warning The delegate isn't responsible for the connected object or its context.
		//!          User is in charge of disconnecting instances before deleting them and
		//!          guarantee the lifetime of the instance is longer than that of the delegate.
		//! \tparam Ret Return type of a function type.
		//! \tparam Args Types of arguments of a function type.
		template <typename Ret, typename... Args>
		class delegate<Ret(Args...)> {
			using func_type = Ret(const void*, Args...);

			func_type* m_fnc = nullptr;
			const void* m_ctx = nullptr;

		public:
			delegate() noexcept = default;

			//! Constructs a delegate by binding a free function or an unbound member to it.
			//! \tparam FuncToBind Function or member to bind to the delegate.
			template <auto FuncToBind>
			delegate(detail::connect_arg_t<FuncToBind>) noexcept {
				bind<FuncToBind>();
			}

			//! Constructs a delegate by binding a free function with context or a bound member to it.
			//! \tparam FuncToBind Function or member to bind to the delegate.
			//! \tparam Type Type of class or type of context.
			//! \param value_or_instance A valid object that fits the purpose.
			template <auto FuncToBind, typename Type>
			delegate(detail::connect_arg_t<FuncToBind>, Type&& value_or_instance) noexcept {
				bind<FuncToBind>(GAIA_FWD(value_or_instance));
			}

			//! Constructs a delegate by binding a function with optional context to it.
			//! \param func Function to bind to the delegate.
			//! \param data User defined arbitrary data.
			delegate(func_type* func, const void* data = nullptr) noexcept {
				bind(func, data);
			}

			//! Binds a free function or an unbound member to a delegate.
			//! \tparam FuncToBind Function or member to bind to the delegate.
			template <auto FuncToBind>
			void bind() noexcept {
				m_ctx = nullptr;

				if constexpr (std::is_invocable_r_v<Ret, decltype(FuncToBind), Args...>) {
					m_fnc = [](const void*, Args... args) -> Ret {
						return Ret(std::invoke(FuncToBind, GAIA_FWD(args)...));
					};
				} else if constexpr (std::is_member_pointer_v<decltype(FuncToBind)>) {
					m_fnc = wrap<FuncToBind>(detail::index_sequence_for<std::tuple_element_t<0, std::tuple<Args...>>>(
							detail::func_ptr_t<decltype(FuncToBind)>{}));
				} else {
					m_fnc = wrap<FuncToBind>(detail::index_sequence_for(detail::func_ptr_t<decltype(FuncToBind)>{}));
				}
			}

			//! Binds a free function with context or a bound member to a delegate. When used to bind a ree function with
			//! context, its signature must be such that the instance is the first argument before the ones used to define the
			//! delegate itself.
			//! \tparam FuncToBind Function or member to bind to the delegate.
			//! \tparam Type Type of class or type of context.
			//! \param value_or_instance* A valid* reference that fits the purpose.
			template <auto FuncToBind, typename Type>
			void bind(Type& value_or_instance) noexcept {
				m_ctx = &value_or_instance;

				if constexpr (std::is_invocable_r_v<Ret, decltype(FuncToBind), Type&, Args...>) {
					using const_or_not_type = std::conditional_t<std::is_const_v<Type>, const void*, void*>;
					m_fnc = [](const void* ctx, Args... args) -> Ret {
						auto pType = static_cast<Type*>(const_cast<const_or_not_type>(ctx));
						return Ret(std::invoke(FuncToBind, *pType, GAIA_FWD(args)...));
					};
				} else {
					m_fnc = wrap<FuncToBind>(
							value_or_instance, detail::index_sequence_for(detail::func_ptr_t<decltype(FuncToBind), Type>{}));
				}
			}

			//! Binds a free function with context or a bound member to a delegate.
			//! \tparam FuncToBind Function or member to bind to the delegate.
			//! \tparam Type Type of class or type of context.
			//! \param value_or_instance A valid pointer that fits the purpose.
			template <auto FuncToBind, typename Type>
			void bind(Type* value_or_instance) noexcept {
				m_ctx = value_or_instance;

				if constexpr (std::is_invocable_r_v<Ret, decltype(FuncToBind), Type*, Args...>) {
					using const_or_not_type = std::conditional_t<std::is_const_v<Type>, const void*, void*>;
					m_fnc = [](const void* ctx, Args... args) -> Ret {
						auto pType = static_cast<Type*>(const_cast<const_or_not_type>(ctx));
						return Ret(std::invoke(FuncToBind, pType, GAIA_FWD(args)...));
					};
				} else {
					m_fnc = wrap<FuncToBind>(
							value_or_instance, detail::index_sequence_for(detail::func_ptr_t<decltype(FuncToBind), Type>{}));
				}
			}

			//! Binds an user defined function with optional context to a delegate.
			//! The context is returned as the first argument to the target function in all cases.
			//! \param function Function to bind* to* the delegate.
			//! \param context User defined arbitrary ctx.
			void bind(func_type* function, const void* context = nullptr) noexcept {
				m_fnc = function;
				m_ctx = context;
			}

			//! Resets a delegate. After a reset, a delegate cannot be invoked anymore.
			void reset() noexcept {
				m_fnc = nullptr;
				m_ctx = nullptr;
			}

			//! Returns the functor pointer linked to a delegate, if any.
			//! \return An opaque pointer to the underlying data.
			GAIA_NODISCARD bool has_func() const noexcept {
				return m_fnc != nullptr;
			}

			//! Returns the instance or the context linked to a delegate, if any.
			//! \return An opaque pointer to the underlying data.
			GAIA_NODISCARD const void* instance() const noexcept {
				return m_ctx;
			}

			//! The delegate invokes the underlying function and returns the result.
			//! \param args* Arguments* to use to invoke the underlying function.
			//! \return The value returned by the underlying * function.
			Ret operator()(Args... args) const {
				GAIA_ASSERT(m_fnc != nullptr && "Trying to call an unbound delegate!");
				return m_fnc(m_ctx, GAIA_FWD(args)...);
			}

			//! Checks whether a delegate actually points to something.
			//! \return False if the delegate is empty, true otherwise.
			GAIA_NODISCARD explicit operator bool() const noexcept {
				// There's no way to set just m_ctx so it's enough to test m_fnc
				return m_fnc != nullptr;
			}

			//! Compares the contents of two delegates.
			//! \param other Delegate with which to compare.
			//! \return True if the two contents are equal, false otherwise.
			GAIA_NODISCARD bool operator==(const delegate<Ret(Args...)>& other) const noexcept {
				return m_fnc == other.m_fnc && m_ctx == other.m_ctx;
			}

			//! Compares the contents of two delegates.
			//! \param other Delegate with which to compare.
			//! \return True if the two contents differ, false otherwise.
			GAIA_NODISCARD bool operator!=(const delegate<Ret(Args...)>& other) const noexcept {
				return !operator==(other);
			}

		private:
			template <auto FuncToBind, std::size_t... Index>
			GAIA_NODISCARD auto wrap(std::index_sequence<Index...>) noexcept {
				return [](const void*, Args... args) -> Ret {
					[[maybe_unused]] const auto argsFwd = std::forward_as_tuple(GAIA_FWD(args)...);
					return Ret(std::invoke(FuncToBind, GAIA_FWD(std::get<Index>(argsFwd))...));
				};
			}

			template <auto FuncToBind, typename Type, std::size_t... Index>
			GAIA_NODISCARD auto wrap(Type&, std::index_sequence<Index...>) noexcept {
				using const_or_not_type = std::conditional_t<std::is_const_v<Type>, const void*, void*>;
				return [](const void* ctx, Args... args) -> Ret {
					[[maybe_unused]] const auto argsFwd = std::forward_as_tuple(GAIA_FWD(args)...);
					auto pType = static_cast<Type*>(const_cast<const_or_not_type>(ctx));
					return Ret(std::invoke(FuncToBind, *pType, GAIA_FWD(std::get<Index>(argsFwd))...));
				};
			}

			template <auto FuncToBind, typename Type, std::size_t... Index>
			GAIA_NODISCARD auto wrap(Type*, std::index_sequence<Index...>) noexcept {
				using const_or_not_type = std::conditional_t<std::is_const_v<Type>, const void*, void*>;
				return [](const void* ctx, Args... args) -> Ret {
					[[maybe_unused]] const auto argsFwd = std::forward_as_tuple(GAIA_FWD(args)...);
					auto pType = static_cast<Type*>(const_cast<const_or_not_type>(ctx));
					return Ret(std::invoke(FuncToBind, pType, GAIA_FWD(std::get<Index>(argsFwd))...));
				};
			}
		};

		//! Compares the contents of two delegates.
		//! \tparam Ret Return type of a function type.
		//! \tparam Args Types of arguments of a function type.
		//! \param lhs A valid delegate object.
		//! \param rhs A valid delegate object.
		//! \return True if the two contents are equal, false otherwise.
		template <typename Ret, typename... Args>
		GAIA_NODISCARD bool operator==(const delegate<Ret(Args...)>& lhs, const delegate<Ret(Args...)>& rhs) noexcept {
			return lhs == rhs;
		}

		//! Compares the contents of two delegates.
		//! \tparam Ret Return type of a function type.
		//! \tparam Args Types of arguments of a function type.
		//! \param lhs A valid delegate object.
		//! \param rhs A valid delegate object.
		//! \return True if the two contents differ, false otherwise.
		template <typename Ret, typename... Args>
		GAIA_NODISCARD bool operator!=(const delegate<Ret(Args...)>& lhs, const delegate<Ret(Args...)>& rhs) noexcept {
			return lhs != rhs;
		}

		template <auto Func>
		delegate(detail::connect_arg_t<Func>) noexcept
				-> delegate<std::remove_pointer_t<detail::func_ptr_t<decltype(Func)>>>;

		template <auto Func, typename Type>
		delegate(detail::connect_arg_t<Func>, Type&&) noexcept
				-> delegate<std::remove_pointer_t<detail::func_ptr_t<decltype(Func), Type>>>;

		template <typename Ret, typename... Args>
		delegate(Ret (*)(const void*, Args...), const void* = nullptr) noexcept -> delegate<Ret(Args...)>;

		//------------------------------------------------------------------------------
		// signal
		//------------------------------------------------------------------------------

		template <typename Ret, typename... Args>
		class signal<Ret(Args...)>;

		namespace detail {
			template <typename Ret, typename... Args>
			using container = cnt::darray<delegate<Ret(Args...)>>;
		} // namespace detail

		//! Signal is a container of listener which it can notify.
		//! It works directly with references to classes and pointers to both free and member functions.
		//! \tparam Ret Return type of a function type.
		//! \tparam* Args Types of arguments of a function type.
		template <typename Ret, typename... Args>
		class signal<Ret(Args...)> {
			friend class sink<Ret(Args...)>;

		private:
			//! Container storing listeners
			detail::container<Ret, Args...> m_listeners;

		public:
			using size_type = typename detail::container<Ret, Args...>::size_type;
			using sink_type = sink<Ret(Args...)>;

			//! Number of listeners connected to the signal.
			//! \return Number of listeners currently connected.
			GAIA_NODISCARD size_type size() const noexcept {
				return m_listeners.size();
			}

			//! Check is there is any listener bound to the signal.
			//! \return True if there is any listener bound to the signal, false otherwise.
			GAIA_NODISCARD bool empty() const noexcept {
				return m_listeners.empty();
			}

			//! Signals all listeners.
			//! \param args Arguments to use to trigger listeners.
			void emit(Args... args) {
				for (auto&& call: std::as_const(m_listeners))
					call(args...);
			}
		};

		//------------------------------------------------------------------------------
		// Sink
		//------------------------------------------------------------------------------

		//! Sink is a helper class used to bind listeners to signals.
		//! The separation between signal and sink makes it possible to store signals as private data members
		//! without exposing their invoke functionality to the users of the class.
		//! It also allows for less header files contention because any binding can (and should) happen in source files.
		//! \warning Lifetime of any sink must not be longer than that of the signal to which it refers.
		//! \tparam Ret Return type of a function type.
		//! \tparam Args Types of arguments of a function type.
		template <typename Ret, typename... Args>
		class sink<Ret(Args...)> {
			using signal_type = signal<Ret(Args...)>;
			using func_type = Ret(const void*, Args...);

			signal_type* m_s;

		public:
			//! Constructs a sink that is allowed to modify a given signal.
			//! \param ref A valid reference to a signal object.
			sink(signal<Ret(Args...)>& ref) noexcept: m_s{&ref} {}

			//! Moves signals from another sink to this one. Result is stored in this object.
			//! The sink we merge from is cleared.
			//! \param other Sink to move signals from
			//! \param compact If true the detail container is reallocated so as little memory as possible is used by it.
			void move_from(sink& other) {
				m_s->m_listeners.reserve(m_s->m_listeners.size() + other.m_s->m_listeners.size());
				for (auto&& listener: other.m_s->m_listeners)
					m_s->m_listeners.push_back(GAIA_MOV(listener));

				other.reset();
			}

			//! Binds a free function or an unbound member to a signal.
			//! The signal handler performs checks to avoid multiple connections for the same function.
			//! \tparam FuncToBind Function or member to bind to the signal.
			template <auto FuncToBind>
			void bind() {
				delegate<Ret(Args...)> call{};
				call.template bind<FuncToBind>();
				bind_internal(call);
			}

			//! Binds a free function with context or a bound member to a signal. When used to bind a free function with
			//! context, its signature must be such that the instance is the first argument before the ones used to define the
			//! signal itself.
			//! \tparam FuncToBind Function or member to bind to the signal.
			//! \tparam Type Type of class or type of context.
			//! \param value_or_instance A valid object that fits the purpose.
			template <auto FuncToBind, typename Type>
			void bind(Type& value_or_instance) {
				delegate<Ret(Args...)> call{};
				call.template bind<FuncToBind>(value_or_instance);
				bind_internal(call);
			}

			//! Binds an user defined function with optional context to a signal.
			//! The context is returned as the first  argument to the target function in all cases.
			//! \param func Function to bind to the signal.
			//! \param data User defined arbitrary ctx.
			void bind(func_type* func, const void* data = nullptr) {
				if (!func && data == nullptr)
					return;

				delegate<Ret(Args...)> call{};
				call.bind(func, data);
				bind_internal(call);
			}

			//! Unbinds a free function or an unbound member from a signal.
			//! \tparam FuncToUnbind* Function* or member to unbind from the signal.
			template <auto FuncToUnbind>
			void unbind() {
				delegate<Ret(Args...)> call{};
				call.template bind<FuncToUnbind>();

				m_s->m_listeners.retain([&](const auto& l) {
					return l != call;
				});
			}

			//! Unbinds a free function with context or a bound member from a signal.
			//! \tparam** FuncToUnbind Function or member to unbind from the signal.
			//! \tparam Type Type of class or type of** context.
			//! \param value_or_instance A valid object that fits the purpose.
			template <auto FuncToUnbind, typename Type>
			void unbind(Type& value_or_instance) {
				delegate<Ret(Args...)> call{};
				call.template bind<FuncToUnbind>(value_or_instance);

				auto& listeners = m_s->m_listeners;
				for (uint32_t i = 0; i < listeners.size();) {
					if (listeners[i] != call) {
						++i;
						continue;
					}

					core::swap_erase_unsafe(listeners, i);
				}
			}

			//! Unbinds a free function with context or bound members from a signal.
			//! \tparam Type Type of class or type of context.
			//! \param value_or_instance A valid object that fits the purpose.
			template <typename Type>
			void unbind(Type* value_or_instance) {
				if (!value_or_instance)
					return;

				auto& listeners = m_s->m_listeners;
				for (uint32_t i = 0; i < listeners.size();) {
					if (listeners[i].instance() != value_or_instance) {
						++i;
						continue;
					}

					core::swap_erase_unsafe(listeners, i);
				}
			}

			//! Unbinds a free function with context or bound members from a signal.
			//! \tparam Type Type of class or type of context.
			//! \param value_or_instance A valid object that fits the purpose.
			template <typename Type>
			void unbind(Type& value_or_instance) {
				unbind(&value_or_instance);
			}

			//! Unbinds all listeners from a signal.
			void reset() {
				m_s->m_listeners.clear();
			}

		private:
			void bind_internal(const delegate<Ret(Args...)>& call) {
				if (!core::has(m_s->m_listeners, call))
					m_s->m_listeners.push_back(GAIA_MOV(call));
			}
		};

		template <typename Ret, typename... Args>
		sink(signal<Ret(Args...)>&) noexcept -> sink<Ret(Args...)>;
	} // namespace util
} // namespace gaia
