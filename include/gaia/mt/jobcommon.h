#pragma once

#include <cstddef>
#include <inttypes.h>
#include <new>
#include <type_traits>
#include <utility>

#include "gaia/core/utility.h"
#include "gaia/mem/mem_alloc.h"
#include "gaia/mt/event.h"
#include "gaia/mt/jobqueue.h"

namespace gaia {
	namespace util {
		class SmallFunc;
		template <typename Signature>
		class MoveFunc;
	} // namespace util

	namespace mt {
		//! Scheduling priority assigned to a job.
		enum class JobPriority : uint8_t {
			//! High priority job. If available it should target the CPU's performance cores.
			High = 0,
			//! Low priority job. If available it should target the CPU's efficiency cores.
			Low = 1
		};
		//! Number of supported job-priority queues.
		static inline constexpr uint32_t JobPriorityCnt = 2;

		//! Flags controlling the scheduling and lifetime of a job.
		enum JobCreationFlags : uint8_t {
			//! Uses automatic deletion and the regular frame queue.
			Default = 0,
			//! The job is not deleted automatically. Has to be done by the user.
			ManualDelete = 0x01,
			//! The job can wait for other job (one not set as dependency).
			CanWait = 0x02,
			//! The job is executed through the background queue and may span multiple frames.
			Background = 0x04
		};

		//! Allocation metadata used when reserving a job slot.
		struct JobAllocCtx {
			//! Priority encoded into the allocated job handle.
			JobPriority priority;
		};

		//! Half-open item range passed to a parallel job callback.
		struct JobArgs {
			//! First item index processed by this invocation.
			uint32_t idxStart;
			//! One-past-the-last item index processed by this invocation.
			uint32_t idxEnd;
		};

		//! \cond INTERNAL
		namespace detail {
			template <typename T>
			struct is_move_func: std::false_type {};
			template <typename Signature>
			struct is_move_func<util::MoveFunc<Signature>>: std::true_type {};
		} // namespace detail
		//! \endcond

		//! Move-only type-erased callable owned by a job.
		//! Callables of up to InlineSize bytes are stored inline. Larger ones are allocated with mem::AllocHelper.
		//! The storage never comes from the process-wide arena allocators (mem::SmallBlockAllocator and friends),
		//! so a job callable can be destroyed on whichever thread finishes with the job.
		//! \tparam TArg Type passed by const reference to the callable, or void for a callable taking no arguments.
		template <typename TArg>
		class JobCallable {
		public:
			//! Size in bytes of the inline callable storage.
			static constexpr uint32_t InlineSize = 24;

		private:
			enum class Op : uint8_t { Invoke, Finish, Destroy, Move };
			using OpFn = void (*)(Op op, JobCallable* dst, JobCallable* src, const void* pArg);

			OpFn m_func = nullptr;
			alignas(std::max_align_t) uint8_t m_storage[InlineSize];

			//! Operations on heap storage whose callable finish() already destroyed. Only the storage is left.
			static void op_storage(Op op, JobCallable* dst, JobCallable* src, [[maybe_unused]] const void* pArg) {
				auto*& pStorage = *reinterpret_cast<void**>(dst->m_storage);
				switch (op) {
					case Op::Invoke:
						GAIA_ASSERT(false && "The callable was already destroyed by finish()");
						break;
					case Op::Finish:
						break;
					case Op::Destroy:
						mem::AllocHelper::free(pStorage);
						break;
					case Op::Move:
						GAIA_ASSERT(src != nullptr);
						pStorage = *reinterpret_cast<void**>(src->m_storage);
						dst->m_func = src->m_func;
						src->m_func = nullptr;
						break;
				}
			}

			template <typename Fn>
			static constexpr bool invocable() {
				if constexpr (std::is_void_v<TArg>)
					return std::is_invocable_r_v<void, Fn&>;
				else
					return std::is_invocable_r_v<void, Fn&, const TArg&>;
			}

			template <typename Fn>
			static void invoke(Fn& fn, [[maybe_unused]] const void* pArg) {
				if constexpr (std::is_void_v<TArg>)
					fn();
				else {
					GAIA_ASSERT(pArg != nullptr);
					fn(*static_cast<const TArg*>(pArg));
				}
			}

			void destroy() {
				if (m_func != nullptr) {
					m_func(Op::Destroy, this, nullptr, nullptr);
					m_func = nullptr;
				}
			}

			void exec_inter(const void* pArg) const {
				GAIA_ASSERT(m_func != nullptr);
				m_func(Op::Invoke, const_cast<JobCallable*>(this), nullptr, pArg);
			}

			template <typename F>
			void init(F&& f) {
				using Fn = std::decay_t<F>;
				static_assert(
						!std::is_same_v<Fn, util::SmallFunc> && !detail::is_move_func<Fn>::value,
						"Pass the callable itself. SmallFunc and MoveFunc may keep it in SmallBlockAllocator, which job "
						"threads must not free from");
				static_assert(invocable<Fn>(), "JobCallable requires a callable compatible with its signature");
				static_assert(std::is_move_constructible_v<Fn>, "Callable must be move-constructible");
				static_assert(
						alignof(Fn) <= alignof(std::max_align_t), "Over-aligned callables are not supported for JobCallable");

				if constexpr (stores_inline<Fn>()) {
					new (m_storage) Fn(GAIA_FWD(f));

					m_func = [](Op op, JobCallable* dst, JobCallable* src, const void* pArg) {
						auto* pFn = reinterpret_cast<Fn*>(dst->m_storage);
						switch (op) {
							case Op::Invoke:
								invoke(*pFn, pArg);
								break;
							case Op::Finish:
								// A trivially destructible callable has nothing to end and is left untouched.
								if constexpr (!std::is_trivially_destructible_v<Fn>) {
									pFn->~Fn();
									dst->m_func = nullptr;
								}
								break;
							case Op::Destroy:
								if constexpr (!std::is_trivially_destructible_v<Fn>)
									pFn->~Fn();
								break;
							case Op::Move: {
								GAIA_ASSERT(src != nullptr);
								auto* pSrcFn = reinterpret_cast<Fn*>(src->m_storage);
								new (dst->m_storage) Fn(GAIA_MOV(*pSrcFn));
								if constexpr (!std::is_trivially_destructible_v<Fn>)
									pSrcFn->~Fn();
								dst->m_func = src->m_func;
								src->m_func = nullptr;
								break;
							}
						}
					};
				} else {
					auto* pStorage = mem::AllocHelper::alloc<Fn>();
					GAIA_ASSERT((uintptr_t)pStorage % alignof(Fn) == 0);
					auto* pFunc = new (pStorage) Fn(GAIA_FWD(f));
					*reinterpret_cast<Fn**>(m_storage) = pFunc;

					m_func = [](Op op, JobCallable* dst, JobCallable* src, const void* pArg) {
						auto*& pFn = *reinterpret_cast<Fn**>(dst->m_storage);
						switch (op) {
							case Op::Invoke:
								GAIA_ASSERT(pFn != nullptr);
								invoke(*pFn, pArg);
								break;
							case Op::Finish:
								// A trivially destructible callable has nothing to end and is left untouched.
								if constexpr (!std::is_trivially_destructible_v<Fn>) {
									GAIA_ASSERT(pFn != nullptr);
									pFn->~Fn();
									dst->m_func = &op_storage;
								}
								break;
							case Op::Destroy:
								GAIA_ASSERT(pFn != nullptr);
								if constexpr (!std::is_trivially_destructible_v<Fn>)
									pFn->~Fn();
								mem::AllocHelper::free(pFn);
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
			}

		public:
			//! Checks whether a callable of type \a F is stored inline, without a heap allocation.
			//! \tparam F Callable type.
			//! \return True when \a F fits the inline storage.
			template <typename F>
			GAIA_NODISCARD static constexpr bool stores_inline() {
				using Fn = std::decay_t<F>;
				return sizeof(Fn) <= InlineSize && alignof(Fn) <= alignof(std::max_align_t);
			}

			JobCallable() = default;
			~JobCallable() {
				if (m_func != nullptr)
					m_func(Op::Destroy, this, nullptr, nullptr);
			}

			JobCallable(const JobCallable&) = delete;
			JobCallable& operator=(const JobCallable&) = delete;

			//! Takes over the callable of \a other, leaving it empty.
			//! \param other Wrapper to move from.
			JobCallable(JobCallable&& other) noexcept {
				if (other.m_func != nullptr)
					other.m_func(Op::Move, this, &other, nullptr);
			}

			//! Destroys the stored callable and takes over the callable of \a other, leaving it empty.
			//! \param other Wrapper to move from.
			//! \return This wrapper.
			JobCallable& operator=(JobCallable&& other) noexcept {
				if (this != &other) {
					destroy();
					if (other.m_func != nullptr)
						other.m_func(Op::Move, this, &other, nullptr);
				}
				return *this;
			}

			//! Wraps the callable \a f.
			//! \tparam F Callable type.
			//! \param f Callable object.
			template <typename F, typename = std::enable_if_t<!std::is_same_v<std::decay_t<F>, JobCallable>>>
			JobCallable(F&& f) {
				init(GAIA_FWD(f));
			}

			//! Destroys the stored callable and wraps the callable \a f.
			//! \tparam F Callable type.
			//! \param f Callable object.
			//! \return This wrapper.
			template <typename F, typename = std::enable_if_t<!std::is_same_v<std::decay_t<F>, JobCallable>>>
			JobCallable& operator=(F&& f) {
				destroy();
				init(GAIA_FWD(f));
				return *this;
			}

			//! Creates a wrapper for the callable \a f.
			//! \tparam F Callable type.
			//! \param f Callable object.
			//! \return New wrapper.
			template <typename F>
			static JobCallable create(F&& f) {
				JobCallable func;
				func.init(GAIA_FWD(f));
				return func;
			}

			//! Invokes the stored callable.
			template <typename T = TArg, typename = std::enable_if_t<std::is_void_v<T>>>
			void exec() const {
				exec_inter(nullptr);
			}

			//! Invokes the stored callable with \a arg.
			//! \param arg Argument forwarded to the callable.
			template <typename T = TArg, typename = std::enable_if_t<!std::is_void_v<T>>>
			void exec(const T& arg) const {
				exec_inter(&arg);
			}

			//! Invokes the stored callable.
			template <typename T = TArg, typename = std::enable_if_t<std::is_void_v<T>>>
			void operator()() const {
				exec_inter(nullptr);
			}

			//! Invokes the stored callable with \a arg.
			//! \param arg Argument forwarded to the callable.
			template <typename T = TArg, typename = std::enable_if_t<!std::is_void_v<T>>>
			void operator()(const T& arg) const {
				exec_inter(&arg);
			}

			//! Destroys the stored callable and everything it captured, and releases its storage.
			void reset() {
				destroy();
			}

			//! Ends the lifetime of the stored callable and everything it captured, but keeps any heap storage until
			//! reset() or the destructor releases it. A trivially destructible callable has nothing to end, so it is left
			//! untouched and this does not write to the wrapper at all.
			//! \warning The callable must not be invoked afterwards.
			void finish() {
				if (m_func != nullptr)
					m_func(Op::Finish, this, nullptr, nullptr);
			}

			//! Checks whether the wrapper holds a callable, or the heap storage of one ended by finish().
			//! \return True when a callable or its storage is held.
			explicit operator bool() const {
				return m_func != nullptr;
			}
		};

		//! Callable of a single job.
		using JobFunc = JobCallable<void>;
		//! Callable of a parallel job, invoked once for each scheduled item range.
		using JobArgsFunc = JobCallable<JobArgs>;

		//! Callable and scheduling options for a single job.
		struct Job {
			//! Callable executed by the worker. The job owns it until the job is deleted. An automatically deleted job
			//! ends the callable's lifetime, and that of everything it captured, on the executing thread right after it
			//! runs and before the job is reported done; heap storage of a callable larger than JobFunc::InlineSize is
			//! released with the job slot right after that. A JobCreationFlags::ManualDelete job keeps its callable, so
			//! it can be reset and run again, until ThreadPool::del() destroys it.
			JobFunc func;
			//! Queue priority used to schedule the callable.
			JobPriority priority = JobPriority::High;
			//! Creation and lifetime options.
			JobCreationFlags flags = JobCreationFlags::Default;
		};

		//! Callable and priority for a range-partitioned parallel job.
		struct JobParallel {
			//! Callable invoked once for each scheduled range.
			JobArgsFunc func;
			//! Queue priority used for each range job.
			JobPriority priority = JobPriority::High;
		};

		//! Non-owning callback descriptor for parallel jobs.
		//! \warning The pointed-to context must stay alive until the scheduled job completes.
		struct JobParallelRef {
			//! Non-owning callback context.
			void* pCtx = nullptr;
			//! Function that invokes the callback stored in the context.
			void (*invoke)(void*, const JobArgs&) = nullptr;
			//! Queue priority used for each range job.
			JobPriority priority = JobPriority::High;
		};

		class ThreadPool;

		//! Per-thread execution state owned by ThreadPool.
		struct ThreadCtx {
			//! Thread pool pointer
			ThreadPool* tp;
			//! Worker index
			uint32_t workerIdx;
			//! Job priority
			JobPriority prio;
			//! True when the worker executes background jobs.
			bool background = false;
			//! True when the worker thread has been successfully created.
			bool threadCreated = false;
			//! Event signaled when a job is executed
			Event event;
			//! Lock-free work stealing queue for the jobs
			JobQueue<512> jobQueue;

			ThreadCtx() = default;
			~ThreadCtx() = default;

			void reset() {
				background = false;
				threadCreated = false;
				event.reset();
				jobQueue.clear();
			}

			ThreadCtx(const ThreadCtx& other) = delete;
			ThreadCtx& operator=(const ThreadCtx& other) = delete;
			ThreadCtx(ThreadCtx&& other) = delete;
			ThreadCtx& operator=(ThreadCtx&& other) = delete;
		};
	} // namespace mt
} // namespace gaia
