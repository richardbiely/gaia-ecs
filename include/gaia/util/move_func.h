#pragma once
#include "gaia/config/config.h"

#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>
#include <utility>

#include "gaia/core/func.h"
#include "gaia/mem/mem_alloc.h"
#if GAIA_FUNC_WRAPPER_SMALLBLOCK
	#include "gaia/mem/smallblock_allocator.h"
#endif

namespace gaia {
	namespace util {
		template <typename Signature>
		class MoveFunc;

		//! Move-only function wrapper with inline storage and optional SmallBlockAllocator spill storage.
		//! Medium spill storage uses SmallBlockAllocator when GAIA_FUNC_WRAPPER_SMALLBLOCK is enabled.
		template <typename R, typename... Args>
		class MoveFunc<R(Args...)> {
			static constexpr uint32_t BufferSize = 24;

			using InvokeFn = R (*)(const void*, Args&&...);
			using DestroyFn = void (*)(void*);
			using MoveFn = void (*)(MoveFunc&, MoveFunc&) noexcept;

			struct Ops {
				InvokeFn invoke;
				DestroyFn destroy;
				MoveFn move;
			};

			union Storage {
				alignas(std::max_align_t) uint8_t inlineData[BufferSize];
				void* pHeap;

				constexpr Storage(): pHeap(nullptr) {}
			};

			void* m_ptr = nullptr;
			const Ops* m_ops = nullptr;
			Storage m_storage;

			template <typename Fn>
			static Fn* inline_ptr(MoveFunc& func) {
				return reinterpret_cast<Fn*>(func.m_ptr);
			}

			template <typename Fn>
			static const Fn* inline_ptr(const MoveFunc& func) {
				return reinterpret_cast<const Fn*>(func.m_ptr);
			}

			template <typename Fn>
			static Fn* ptr(const void* p) {
				return reinterpret_cast<Fn*>(const_cast<void*>(p));
			}

			template <typename Fn>
			static R invoke_inline(const void* p, Args&&... args) {
				auto* pFn = ptr<Fn>(p);
				GAIA_ASSERT(pFn != nullptr);
				if constexpr (std::is_void_v<R>) {
					gaia::invoke(*pFn, GAIA_FWD(args)...);
					return;
				} else
					return gaia::invoke(*pFn, GAIA_FWD(args)...);
			}

			template <typename Fn>
			static void destroy_inline(void* p) {
				auto* pFn = ptr<Fn>(p);
				GAIA_ASSERT(pFn != nullptr);
				if constexpr (!std::is_trivially_destructible_v<Fn>)
					pFn->~Fn();
			}

			template <typename Fn>
			static void move_inline(MoveFunc& dst, MoveFunc& src) noexcept {
				auto* pSrcFn = inline_ptr<Fn>(src);
				GAIA_ASSERT(pSrcFn != nullptr);
				new (dst.m_storage.inlineData) Fn(GAIA_MOV(*pSrcFn));
				pSrcFn->~Fn();
				dst.m_ptr = dst.m_storage.inlineData;
				dst.m_ops = src.m_ops;
				src.m_ptr = nullptr;
				src.m_ops = nullptr;
			}

			template <typename Fn>
			static R invoke_heap(const void* p, Args&&... args) {
				auto* pFn = ptr<Fn>(p);
				GAIA_ASSERT(pFn != nullptr);
				if constexpr (std::is_void_v<R>) {
					gaia::invoke(*pFn, GAIA_FWD(args)...);
					return;
				} else
					return gaia::invoke(*pFn, GAIA_FWD(args)...);
			}

			template <typename Fn>
			static void destroy_heap(void* p) {
				auto* pFn = ptr<Fn>(p);
				GAIA_ASSERT(pFn != nullptr);
				if constexpr (!std::is_trivially_destructible_v<Fn>)
					pFn->~Fn();
				mem::AllocHelper::free(pFn);
			}

#if GAIA_FUNC_WRAPPER_SMALLBLOCK
			template <typename Fn>
			static void destroy_smallblock(void* p) {
				auto* pFn = ptr<Fn>(p);
				GAIA_ASSERT(pFn != nullptr);
				if constexpr (!std::is_trivially_destructible_v<Fn>)
					pFn->~Fn();
				mem::SmallBlockAllocator::get().free(pFn);
			}
#endif

			template <typename Fn>
			static void move_heap(MoveFunc& dst, MoveFunc& src) noexcept {
				dst.m_ptr = src.m_ptr;
				dst.m_ops = src.m_ops;
				src.m_ptr = nullptr;
				src.m_ops = nullptr;
			}

			template <typename Fn>
			static const Ops& inline_ops() {
				static const Ops ops = {&invoke_inline<Fn>, &destroy_inline<Fn>, &move_inline<Fn>};
				return ops;
			}

#if GAIA_FUNC_WRAPPER_SMALLBLOCK
			template <typename Fn>
			static const Ops& smallblock_ops() {
				static const Ops ops = {&invoke_heap<Fn>, &destroy_smallblock<Fn>, &move_heap<Fn>};
				return ops;
			}
#endif

			template <typename Fn>
			static const Ops& heap_ops() {
				static const Ops ops = {&invoke_heap<Fn>, &destroy_heap<Fn>, &move_heap<Fn>};
				return ops;
			}

			void destroy() {
				if (m_ops != nullptr) {
					m_ops->destroy(m_ptr);
					m_ptr = nullptr;
					m_ops = nullptr;
				}
			}

			template <typename F>
			void init(F&& f) {
				using Fn = std::decay_t<F>;
				static_assert(std::is_invocable_r_v<R, Fn&, Args...>, "MoveFunc requires a compatible callable");
				static_assert(std::is_move_constructible_v<Fn>, "Callable must be move-constructible");
				static_assert(
						alignof(Fn) <= alignof(std::max_align_t), "Over-aligned callables are not supported for MoveFunc");

				if constexpr (sizeof(Fn) <= BufferSize) {
					new (m_storage.inlineData) Fn(GAIA_FWD(f));
					m_ptr = m_storage.inlineData;
					m_ops = &inline_ops<Fn>();
#if GAIA_FUNC_WRAPPER_SMALLBLOCK
				} else if constexpr (sizeof(Fn) <= mem::SmallBlockMaxSize) {
					auto* pStorage = mem::SmallBlockAllocator::get().alloc((uint32_t)sizeof(Fn));
					GAIA_ASSERT((uintptr_t)pStorage % alignof(Fn) == 0);
					auto* pFunc = new (pStorage) Fn(GAIA_FWD(f));
					m_ptr = pFunc;
					m_ops = &smallblock_ops<Fn>();
#endif
				} else {
					auto* pStorage = mem::AllocHelper::alloc<Fn>();
					GAIA_ASSERT((uintptr_t)pStorage % alignof(Fn) == 0);
					auto* pFunc = new (pStorage) Fn(GAIA_FWD(f));
					m_ptr = pFunc;
					m_ops = &heap_ops<Fn>();
				}
			}

		public:
			//! Constructs an empty function wrapper.
			MoveFunc() = default;

			//! Constructs an empty function wrapper.
			MoveFunc(std::nullptr_t) {}

			//! Destroys the stored callable, if any.
			~MoveFunc() {
				destroy();
			}

			MoveFunc(const MoveFunc&) = delete;
			MoveFunc& operator=(const MoveFunc&) = delete;

			//! Move-constructs the wrapper.
			//! \param other Source wrapper.
			MoveFunc(MoveFunc&& other) noexcept {
				if (other.m_ops != nullptr)
					other.m_ops->move(*this, other);
			}

			//! Move-assigns the wrapper.
			//! \param other Source wrapper.
			//! \return Reference to this wrapper.
			MoveFunc& operator=(MoveFunc&& other) noexcept {
				if (this != &other) {
					destroy();
					if (other.m_ops != nullptr)
						other.m_ops->move(*this, other);
				}
				return *this;
			}

			//! Clears the wrapper.
			//! \return Reference to this wrapper.
			MoveFunc& operator=(std::nullptr_t) {
				reset();
				return *this;
			}

			template <typename F, typename = std::enable_if_t<!std::is_same_v<std::decay_t<F>, MoveFunc>>>
			MoveFunc(F&& f) {
				init(GAIA_FWD(f));
			}

			template <typename F, typename = std::enable_if_t<!std::is_same_v<std::decay_t<F>, MoveFunc>>>
			MoveFunc& operator=(F&& f) {
				destroy();
				init(GAIA_FWD(f));
				return *this;
			}

			//! Creates a wrapper from a callable compatible with the function signature.
			//! \tparam F Callable type.
			//! \param f Callable object.
			//! \return New function wrapper.
			template <typename F>
			static MoveFunc create(F&& f) {
				MoveFunc func;
				func.init(GAIA_FWD(f));
				return func;
			}

			//! Destroys the callable held by the wrapper.
			//! \param func Wrapper to clear.
			static void destroy(MoveFunc& func) {
				func.destroy();
			}

			//! Executes the stored callable.
			//! \param args Arguments forwarded to the callable.
			R exec(Args... args) const {
				GAIA_ASSERT(m_ops != nullptr);
				if constexpr (std::is_void_v<R>) {
					m_ops->invoke(m_ptr, GAIA_FWD(args)...);
					return;
				} else
					return m_ops->invoke(m_ptr, GAIA_FWD(args)...);
			}

			//! Executes the stored callable.
			//! \param args Arguments forwarded to the callable.
			R operator()(Args... args) const {
				if constexpr (std::is_void_v<R>) {
					exec(GAIA_FWD(args)...);
					return;
				} else
					return exec(GAIA_FWD(args)...);
			}

			//! Clears the wrapper.
			void reset() {
				destroy();
			}

			//! Returns true when a callable is stored.
			explicit operator bool() const {
				return m_ops != nullptr;
			}
		};
	} // namespace util
} // namespace gaia
