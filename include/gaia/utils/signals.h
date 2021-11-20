#pragma once
#include "../utils/vector.h"
#include <functional>
#include <inttypes.h>
#include <tuple>
#include <typeinfo>
#include <utility>


namespace gaia {
	namespace utils {
		namespace internal {
			template <typename Ret, typename... Args>
			auto func_ptr(Ret (*)(Args...)) -> Ret (*)(Args...);

			template <typename Ret, typename Type, typename... Args, typename Other>
			auto func_ptr(Ret (*)(Type, Args...), Other&&) -> Ret (*)(Args...);

			template <
					typename Class, typename Ret, typename... Args, typename... Other>
			auto func_ptr(Ret (Class::*)(Args...), Other&&...) -> Ret (*)(Args...);

			template <
					typename Class, typename Ret, typename... Args, typename... Other>
			auto func_ptr(Ret (Class::*)(Args...) const, Other&&...)
					-> Ret (*)(Args...);

			template <typename Class, typename Type, typename... Other>
			auto func_ptr(Type Class::*, Other&&...) -> Type (*)();

			template <typename... Type>
			using func_ptr_t = decltype(internal::func_ptr(std::declval<Type>()...));

			template <typename... Class, typename Ret, typename... Args>
			[[nodiscard]] constexpr auto index_sequence_for(Ret (*)(Args...)) {
				return std::index_sequence_for<Class..., Args...>{};
			}

			// Wraps a function or a member of a specified type
			template <auto>
			struct connect_arg_t {};
			// Function wrapper
			template <auto Func>
			inline constexpr connect_arg_t<Func> connect_arg{};
		} // namespace internal

		template <typename>
		class delegate;
		template <typename Function>
		class signal;
		template <typename Function>
		class sink;

#pragma region Delegate

		// TODO: This can replace gaia_delegate...

		/**
		 \brief Delegate for function pointers and members.
		 It can be used as a general-purpose invoker for any free function with no
		 memory overhead. \warning The delegate isn't responsible for the connected
		 object or its context. User is in charge of disconnecting instances before
		 deleting them and guarantee the lifetime of the instance is longer than
		 that of the delegate. \tparam Ret Return type of a function type. \tparam
		 Args Types of arguments of a function type.
		 */
		template <typename Ret, typename... Args>
		class delegate<Ret(Args...)> {
			using func_type = Ret(const void*, Args...);

			func_type* fnc = nullptr;
			const void* ctx = nullptr;

		public:
			delegate() noexcept = default;

			/**
			 \brief Constructs a delegate by binding a free function or an unbound
			 member to it. \tparam FuncToBind Function or member to bind to the
			 delegate.
			 */
			template <auto FuncToBind>
			delegate(internal::connect_arg_t<FuncToBind>) noexcept {
				bind<FuncToBind>();
			}

			/**
			 \brief Constructs a delegate by binding a free function with context or a
			 bound member to it. \tparam FuncToBind Function or member to bind to the
			 delegate. \tparam Type Type of class or type of context. \param
			 value_or_instance A valid object that fits the purpose.
			 */
			template <auto FuncToBind, typename Type>
			delegate(
					internal::connect_arg_t<FuncToBind>,
					Type&& value_or_instance) noexcept {
				bind<FuncToBind>(std::forward<Type>(value_or_instance));
			}

			/**
			 \brief Constructs a delegate by binding a function with optional context
			 to it. \param func Function to bind to the delegate. \param data User
			 defined arbitrary data.
			 */
			delegate(func_type* func, const void* data = nullptr) noexcept {
				bind(func, data);
			}

			/**
			 \brief Binds a free function or an unbound member to a delegate.
			 \tparam FuncToBind Function or member to bind to the delegate.
			 */
			template <auto FuncToBind>
			void bind() noexcept {
				ctx = nullptr;

				if constexpr (std::is_invocable_r_v<
													Ret, decltype(FuncToBind), Args...>) {
					fnc = [](const void*, Args... args) -> Ret {
						return Ret(std::invoke(FuncToBind, std::forward<Args>(args)...));
					};
				} else if constexpr (std::is_member_pointer_v<decltype(FuncToBind)>) {
					fnc = wrap<FuncToBind>(internal::index_sequence_for<
																 std::tuple_element_t<0, std::tuple<Args...>>>(
							internal::func_ptr_t<decltype(FuncToBind)>{}));
				} else {
					fnc = wrap<FuncToBind>(internal::index_sequence_for(
							internal::func_ptr_t<decltype(FuncToBind)>{}));
				}
			}

			/**
			 \brief Binds a free function with context or a bound member to a
			 delegate. When used to bind a free function with context, its signature
			 must be such that the instance is the first argument before the ones used
			 to define the delegate itself. \tparam FuncToBind Function or member to
			 bind to the delegate. \tparam Type Type of class or type of context.
			 \param value_or_instance A valid reference that fits the purpose.
			 */
			template <auto FuncToBind, typename Type>
			void bind(Type& value_or_instance) noexcept {
				ctx = &value_or_instance;

				if constexpr (std::is_invocable_r_v<
													Ret, decltype(FuncToBind), Type&, Args...>) {
					fnc = [](const void* context, Args... args) -> Ret {
						auto pType = static_cast<Type*>(
								const_cast<std::conditional_t<
										std::is_const_v<Type>, const void*, void*>>(context));
						return Ret(
								std::invoke(FuncToBind, *pType, std::forward<Args>(args)...));
					};
				} else {
					fnc = wrap<FuncToBind>(
							value_or_instance,
							internal::index_sequence_for(
									internal::func_ptr_t<decltype(FuncToBind), Type>{}));
				}
			}

			/**
			 \brief Binds a free function with context or a bound member to a
			 delegate. \sa bind(Type&) \tparam FuncToBind Function or member to bind
			 to the delegate. \tparam Type Type of class or type of context. \param
			 value_or_instance A valid pointer that fits the purpose.
			 */
			template <auto FuncToBind, typename Type>
			void bind(Type* value_or_instance) noexcept {
				ctx = value_or_instance;

				if constexpr (std::is_invocable_r_v<
													Ret, decltype(FuncToBind), Type*, Args...>) {
					fnc = [](const void* context, Args... args) -> Ret {
						auto pType = static_cast<Type*>(
								const_cast<std::conditional_t<
										std::is_const_v<Type>, const void*, void*>>(context));
						return Ret(
								std::invoke(FuncToBind, pType, std::forward<Args>(args)...));
					};
				} else {
					fnc = wrap<FuncToBind>(
							value_or_instance,
							internal::index_sequence_for(
									internal::func_ptr_t<decltype(FuncToBind), Type>{}));
				}
			}

			/**
			 \brief Binds an user defined function with optional context to a
			 delegate. The context is returned as the first argument to the target
			 function in all cases. \param function Function to bind to the delegate.
			 \param context User defined arbitrary ctx.
			 */
			void bind(func_type* function, const void* context = nullptr) noexcept {
				fnc = function;
				ctx = context;
			}

			/**
			 \brief Resets a delegate.
			 After a reset, a delegate cannot be invoked anymore.
			 */
			void reset() noexcept {
				fnc = nullptr;
				ctx = nullptr;
			}

			/**
			 \brief Returns the instance or the context linked to a delegate, if any.
			 \return An opaque pointer to the underlying data.
			 */
			[[nodiscard]] const void* instance() const noexcept {
				return ctx;
			}

			/**
			 \brief The delegate invokes the underlying function and returns the
			 result. \param args Arguments to use to invoke the underlying function.
			 \return The value returned by the underlying function.
			 */
			Ret operator()(Args... args) const {
				gaia_assert2(fnc != nullptr, "Trying to call an unbound delegate!");
				return fnc(ctx, std::forward<Args>(args)...);
			}

			/**
			 \brief Checks whether a delegate actually points to something.
			 \return False if the delegate is empty, true otherwise.
			 */
			[[nodiscard]] explicit operator bool() const noexcept {
				return fnc != nullptr; // there's no way to set just ctx so it's enough
															 // to test fnc
			}

			/**
			 \brief Compares the contents of two delegates.
			 \param other Delegate with which to compare.
			 \return True if the two contents are equal, false otherwise.
			 */
			[[nodiscard]] bool
			operator==(const delegate<Ret(Args...)>& other) const noexcept {
				return fnc == other.fnc && ctx == other.ctx;
			}

			/**
			 \brief Compares the contents of two delegates.
			 \param other Delegate with which to compare.
			 \return True if the two contents differ, false otherwise.
			 */
			[[nodiscard]] bool
			operator!=(const delegate<Ret(Args...)>& other) const noexcept {
				return !operator==(other);
			}

		private:
			template <auto FuncToBind, std::size_t... Index>
			[[nodiscard]] auto wrap(std::index_sequence<Index...>) noexcept {
				return [](const void*, Args... args) -> Ret {
					[[maybe_unused]] const auto forwardedArgs =
							std::forward_as_tuple(std::forward<Args>(args)...);

					return Ret(std::invoke(
							FuncToBind,
							std::forward<std::tuple_element_t<Index, std::tuple<Args...>>>(
									std::get<Index>(forwardedArgs))...));
				};
			}

			template <auto FuncToBind, typename Type, std::size_t... Index>
			[[nodiscard]] auto wrap(Type&, std::index_sequence<Index...>) noexcept {
				return [](const void* context, Args... args) -> Ret {
					[[maybe_unused]] const auto forwardedArgs =
							std::forward_as_tuple(std::forward<Args>(args)...);

					auto pType = static_cast<Type*>(
							const_cast<std::conditional_t<
									std::is_const_v<Type>, const void*, void*>>(context));
					return Ret(std::invoke(
							FuncToBind, *pType,
							std::forward<std::tuple_element_t<Index, std::tuple<Args...>>>(
									std::get<Index>(forwardedArgs))...));
				};
			}

			template <auto FuncToBind, typename Type, std::size_t... Index>
			[[nodiscard]] auto wrap(Type*, std::index_sequence<Index...>) noexcept {
				return [](const void* context, Args... args) -> Ret {
					[[maybe_unused]] const auto forwardedArgs =
							std::forward_as_tuple(std::forward<Args>(args)...);

					auto pType = static_cast<Type*>(
							const_cast<std::conditional_t<
									std::is_const_v<Type>, const void*, void*>>(context));
					return Ret(std::invoke(
							FuncToBind, pType,
							std::forward<std::tuple_element_t<Index, std::tuple<Args...>>>(
									std::get<Index>(forwardedArgs))...));
				};
			}
		};

		/**
		 \brief Compares the contents of two delegates.
		 \tparam Ret Return type of a function type.
		 \tparam Args Types of arguments of a function type.
		 \param lhs A valid delegate object.
		 \param rhs A valid delegate object.
		 \return True if the two contents are equal, false otherwise.
		 */
		template <typename Ret, typename... Args>
		[[nodiscard]] bool operator==(
				const delegate<Ret(Args...)>& lhs,
				const delegate<Ret(Args...)>& rhs) noexcept {
			return lhs == rhs;
		}

		/**
		 \brief Compares the contents of two delegates.
		 \tparam Ret Return type of a function type.
		 \tparam Args Types of arguments of a function type.
		 \param lhs A valid delegate object.
		 \param rhs A valid delegate object.
		 \return True if the two contents differ, false otherwise.
		 */
		template <typename Ret, typename... Args>
		[[nodiscard]] bool operator!=(
				const delegate<Ret(Args...)>& lhs,
				const delegate<Ret(Args...)>& rhs) noexcept {
			return lhs != rhs;
		}

		template <auto Func>
		delegate(internal::connect_arg_t<Func>) noexcept -> delegate<
				std::remove_pointer_t<internal::func_ptr_t<decltype(Func)>>>;

		template <auto Func, typename Type>
		delegate(internal::connect_arg_t<Func>, Type&&) noexcept -> delegate<
				std::remove_pointer_t<internal::func_ptr_t<decltype(Func), Type>>>;

		template <typename Ret, typename... Args>
		delegate(Ret (*)(const void*, Args...), const void* = nullptr) noexcept
				->delegate<Ret(Args...)>;

#pragma endregion

#pragma region Signal

		/**
		 \brief Signal is a container of listener which it can notify.
		 It works directly with references to classes and pointers to both free and
		 member functions. \tparam Ret Return type of a function type. \tparam Args
		 Types of arguments of a function type.
		 */
		template <typename Ret, typename... Args>
		class signal<Ret(Args...)> {
			friend class sink<Ret(Args...)>;
			// TODO:
			// Make container types optional. E.g. we might not want to pay with heap
			// memory if a fixed number of listeners is what we need.
			using container = utils::vector<delegate<Ret(Args...)>>;

			/**
			\brief Container storing listeners
			*/
			container listeners;

		public:
			using size_type = typename container::SP::SizeType;
			using sink_type = sink<Ret(Args...)>;

			/**
			 \brief Number of listeners connected to the signal.
			 \return Number of listeners currently connected.
			 */
			[[nodiscard]] size_type size() const noexcept {
				return listeners.size();
			}

			const container& getListeners() const {
				return listeners;
			}

			/**
			 \brief Check is there is any listener bound to the signal.
			 \return True if there is any listener bound to the signal, false
			 otherwise.
			 */
			[[nodiscard]] bool empty() const noexcept {
				return listeners.empty();
			}

			/**
			 \brief Signals all listeners.
			 \param args Arguments to use to invoke listeners.
			 */
			void invoke(Args... args) const {
				for (auto&& call: std::as_const(listeners))
					call(args...);
			}
		};

#pragma endregion

#pragma region Sink

		/**
		 \brief Sink is a helper class used to bind listeners to signals.
		 The separation between Signal and Sink makes it possible to store signals
		 as private data members without exposing their invoke functionality to the
		 users of the class. It also allows for less header files contention because
		 any binding can (and should) happen in source files. \warning Lifetime of
		 any sink must not be longer than that of the signal to which it refers.
		 \tparam Ret Return type of a function type. \tparam Args Types of arguments
		 of a function type.
		 */
		template <typename Ret, typename... Args>
		class sink<Ret(Args...)> {
			using signal_type = signal<Ret(Args...)>;
			using func_type = Ret(const void*, Args...);

			signal_type* s;

		public:
			/**
			 \brief Constructs a sink that is allowed to modify a given signal.
			 \param ref A valid reference to a signal object.
			 */
			sink(signal<Ret(Args...)>& ref) noexcept: s{&ref} {}

			/**
			 \brief Moves signals from another sink to this one. Result is stored in
			 this object. The sink we merge from is cleared. \param other Sink to move
			 signals from
			 */
			void move_from(sink& other) {
				for (auto i = 0u; i < other.s->listeners.size(); i++)
					s->listeners.Append(std::move(other.s->listeners[i]));

				other.unbind();
			}

			/**
			 \brief Binds a free function or an unbound member to a signal.
			 The signal handler performs checks to avoid multiple connections for the
			 same function. \tparam FuncToBind Function or member to bind to the
			 signal.
			 */
			template <auto FuncToBind>
			void bind() {
				unbind<FuncToBind>();

				delegate<Ret(Args...)> call{};
				call.template bind<FuncToBind>();
				s->listeners.Append(std::move(call));
			}

			/**
			 \brief Binds a free function with context or a bound member to a signal.
			 When used to bind a free function with context, its signature must be
			 such that the instance is the first argument before the ones used to
			 define the signal itself.
			 \tparam FuncToBind Function or member to bind to the signal.
			 \tparam Type Type of class or type of context.
			 \param value_or_instance A valid object that fits the purpose.
			 */
			template <auto FuncToBind, typename Type>
			void bind(Type& value_or_instance) {
				unbind<FuncToBind>(value_or_instance);

				delegate<Ret(Args...)> call{};
				call.template bind<FuncToBind>(value_or_instance);
				s->listeners.Append(std::move(call));
			}

			/**
			 \brief Binds an user defined function with optional context to a signal.
			 The context is returned as the first argument to the target function in
			 all cases. \param function Function to bind to the signal. \param context
			 User defined arbitrary ctx.
			 */
			void bind(func_type* func, const void* data = nullptr) {
				delegate<Ret(Args...)> call{};
				call.bind(func, data);
				s->listeners.Append(std::move(call));
			}

			/**
			 \brief Unbinds a free function or an unbound member from a signal.
			 \tparam FuncToUnbind Function or member to unbind from the signal.
			 */
			template <auto FuncToUnbind>
			void unbind() {
				delegate<Ret(Args...)> call{};
				call.template bind<FuncToUnbind>();

				auto& listeners = s->listeners;
				for (auto i = 0; i < listeners.size();) {
					if (listeners[i] != call) {
						++i;
						continue;
					}

					listeners.fast_erase(i);
				}
			}

			/**
			 \brief Unbinds a free function with context or a bound member from a
			 signal. \tparam FuncToUnbind Function or member to unbind from the
			 signal. \tparam Type Type of class or type of context. \param
			 value_or_instance A valid object that fits the purpose.
			 */
			template <auto FuncToUnbind, typename Type>
			void unbind(Type& value_or_instance) {
				delegate<Ret(Args...)> call{};
				call.template bind<FuncToUnbind>(value_or_instance);

				auto& listeners = s->listeners;
				for (auto i = 0; i < listeners.size();) {
					if (listeners[i] != call) {
						++i;
						continue;
					}

					listeners.fast_erase(i);
				}
			}

			/**
			 \brief Unbinds a free function with context or bound members from a
			 signal. \tparam Type Type of class or type of context. \param
			 value_or_instance A valid object that fits the purpose.
			 */
			template <typename Type>
			void unbind(Type& value_or_instance) {
				auto& listeners = s->listeners;
				for (auto i = 0; i < listeners.size();) {
					if (listeners[i].instance() != &value_or_instance) {
						++i;
						continue;
					}

					listeners.fast_erase(i);
				}
			}

			/**
			 \brief Unbinds a free function with context or bound members from a
			 signal. \tparam Type Type of class or type of context. \param
			 value_or_instance A valid object that fits the purpose.
			 */
			template <typename Type>
			void unbind(Type* value_or_instance) {
				if (value_or_instance)
					unbind(*value_or_instance);
			}

			/**
			\brief Unbinds all listeners from a signal.
			*/
			void unbind() {
				s->listeners.clear();
			}

			/**
			 \brief Signals all listeners.
			 \param args Arguments to use to invoke listeners.
			 */
			void invoke(Args... args) const {
				for (auto&& call: std::as_const(s->listeners))
					call(args...);
			}
		};

		template <typename Ret, typename... Args>
		sink(signal<Ret(Args...)>&) noexcept->sink<Ret(Args...)>;

#pragma endregion
	} // namespace utils
} // namespace gaia
