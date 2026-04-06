#pragma once
#include "gaia/config/config.h"

#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>
#include <utility>

#include "gaia/mem/smallblock_allocator.h"

namespace gaia {
	namespace util {
		//! Move-only function wrapper with inline storage, SmallBlockAllocator spill storage,
		//! and default-heap fallback for oversized callables.
		class SmallFunc {
			static constexpr uint32_t MaxSize = mem::SmallBlockMaxSize;
			static constexpr uint32_t BufferSize = 24;

			enum class Op : uint8_t { Invoke, Destroy, Move };
			using OpFn = void (*)(Op op, SmallFunc* dst, SmallFunc* src);

			OpFn m_func = nullptr;
			alignas(std::max_align_t) uint8_t m_storage[BufferSize]{};

			void destroy() {
				if (m_func != nullptr) {
					m_func(Op::Destroy, this, nullptr);
					m_func = nullptr;
				}
			}

		public:
			//! Constructs an empty function wrapper.
			SmallFunc() = default;

			//! Destroys the stored callable, if any.
			~SmallFunc() {
				destroy();
			}

			SmallFunc(const SmallFunc&) = delete;
			SmallFunc& operator=(const SmallFunc&) = delete;

			//! Move-constructs the wrapper.
			//! \param other Source wrapper.
			SmallFunc(SmallFunc&& other) noexcept {
				if (other.m_func != nullptr)
					other.m_func(Op::Move, this, &other);
			}

			//! Move-assigns the wrapper.
			//! \param other Source wrapper.
			//! \return Reference to this wrapper.
			SmallFunc& operator=(SmallFunc&& other) noexcept {
				if (this != &other) {
					destroy();
					if (other.m_func != nullptr)
						other.m_func(Op::Move, this, &other);
				}
				return *this;
			}

			template <typename F, typename = std::enable_if_t<!std::is_same_v<std::decay_t<F>, SmallFunc>>>
			SmallFunc(F&& f): SmallFunc(create(GAIA_FWD(f))) {}

			template <typename F, typename = std::enable_if_t<!std::is_same_v<std::decay_t<F>, SmallFunc>>>
			SmallFunc& operator=(F&& f) {
				*this = create(GAIA_FWD(f));
				return *this;
			}

			//! Creates a wrapper from a callable compatible with void().
			//! \tparam F Callable type.
			//! \param f Callable object.
			//! \return New function wrapper.
			template <typename F>
			static SmallFunc create(F&& f) {
				using Fn = std::decay_t<F>;
				static_assert(std::is_invocable_r_v<void, Fn&>, "SmallFunc requires a callable compatible with void()");
				static_assert(std::is_move_constructible_v<Fn>, "Callable must be move-constructible");

				SmallFunc func;

				if constexpr (sizeof(Fn) <= BufferSize && alignof(Fn) <= alignof(std::max_align_t)) {
					new (func.m_storage) Fn(GAIA_FWD(f));

					func.m_func = [](Op op, SmallFunc* dst, SmallFunc* src) {
						auto* pFn = reinterpret_cast<Fn*>(dst->m_storage);

						switch (op) {
							case Op::Invoke:
								(*pFn)();
								break;
							case Op::Destroy:
								pFn->~Fn();
								break;
							case Op::Move: {
								GAIA_ASSERT(src != nullptr);
								auto* pSrcFn = reinterpret_cast<Fn*>(src->m_storage);
								new (dst->m_storage) Fn(GAIA_MOV(*pSrcFn));
								pSrcFn->~Fn();
								dst->m_func = src->m_func;
								src->m_func = nullptr;
								break;
							}
						}
					};
				} else {
					static_assert(
							alignof(Fn) <= alignof(std::max_align_t), "Over-aligned callables are not supported for SmallFunc");

					constexpr bool UseSmallBlock = sizeof(Fn) <= MaxSize;
					auto* pStorage = UseSmallBlock ? mem::SmallBlockAllocator::get().alloc((uint32_t)sizeof(Fn))
																				 : mem::AllocHelper::alloc<Fn>();
					GAIA_ASSERT((uintptr_t)pStorage % alignof(Fn) == 0);
					auto* pFunc = new (pStorage) Fn(GAIA_FWD(f));
					*reinterpret_cast<Fn**>(func.m_storage) = pFunc;

					func.m_func = [](Op op, SmallFunc* dst, SmallFunc* src) {
						auto*& pFn = *reinterpret_cast<Fn**>(dst->m_storage);

						switch (op) {
							case Op::Invoke:
								GAIA_ASSERT(pFn != nullptr);
								(*pFn)();
								break;
							case Op::Destroy:
								GAIA_ASSERT(pFn != nullptr);
								pFn->~Fn();
								if constexpr (UseSmallBlock)
									mem::SmallBlockAllocator::get().free(pFn);
								else
									mem::AllocHelper::free(pFn);
								pFn = nullptr;
								break;
							case Op::Move:
								GAIA_ASSERT(src != nullptr);
								*reinterpret_cast<Fn**>(dst->m_storage) = *reinterpret_cast<Fn**>(src->m_storage);
								dst->m_func = src->m_func;
								*reinterpret_cast<Fn**>(src->m_storage) = nullptr;
								src->m_func = nullptr;
								break;
						}
					};
				}

				return func;
			}

			//! Destroys the callable held by the wrapper.
			//! \param func Wrapper to clear.
			static void destroy(SmallFunc& func) {
				func.destroy();
			}

			//! Executes the stored callable.
			void exec() {
				GAIA_ASSERT(m_func != nullptr);
				m_func(Op::Invoke, this, nullptr);
			}

			//! Executes the stored callable.
			void operator()() {
				exec();
			}

			//! Clears the wrapper.
			void reset() {
				destroy();
			}

			//! Returns true when a callable is stored.
			explicit operator bool() const {
				return m_func != nullptr;
			}
		};

#if __cplusplus < 202002L
	#define CreateSmallFunc(x)                                                                                           \
		::gaia::util::SmallFunc::create([y = GAIA_MOV(x)]() {                                                              \
			y();                                                                                                             \
		})
#else
	#define CreateSmallFunc(x) ::gaia::util::SmallFunc::create(GAIA_MOV(x))
#endif
	} // namespace util
} // namespace gaia
