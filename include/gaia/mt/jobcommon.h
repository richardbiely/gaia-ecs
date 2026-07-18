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
#include "gaia/util/small_func.h"

namespace gaia {
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

		//! Callable and scheduling options for a single job.
		struct Job {
			//! Callable executed by the worker.
			util::SmallFunc func;
			//! Queue priority used to schedule the callable.
			JobPriority priority = JobPriority::High;
			//! Creation and lifetime options.
			JobCreationFlags flags = JobCreationFlags::Default;
		};

		//! Half-open item range passed to a parallel job callback.
		struct JobArgs {
			//! First item index processed by this invocation.
			uint32_t idxStart;
			//! One-past-the-last item index processed by this invocation.
			uint32_t idxEnd;
		};

		//! Move-only callback wrapper specialized for parallel job ranges.
		class JobArgsFunc {
			static constexpr uint32_t BufferSize = 24;

			enum class Op : uint8_t { Invoke, Destroy, Move };
			using OpFn = void (*)(Op op, JobArgsFunc* dst, JobArgsFunc* src, const JobArgs* pArgs);

			OpFn m_func = nullptr;
			alignas(std::max_align_t) uint8_t m_storage[BufferSize];

			void destroy() {
				if (m_func != nullptr) {
					m_func(Op::Destroy, this, nullptr, nullptr);
					m_func = nullptr;
				}
			}

			template <typename F>
			void init(F&& f) {
				using Fn = std::decay_t<F>;
				static_assert(std::is_invocable_r_v<void, Fn&, const JobArgs&>, "JobArgsFunc requires a compatible callable");
				static_assert(std::is_move_constructible_v<Fn>, "Callable must be move-constructible");
				static_assert(
						alignof(Fn) <= alignof(std::max_align_t), "Over-aligned callables are not supported for JobArgsFunc");

				if constexpr (sizeof(Fn) <= BufferSize) {
					new (m_storage) Fn(GAIA_FWD(f));

					m_func = [](Op op, JobArgsFunc* dst, JobArgsFunc* src, const JobArgs* pArgs) {
						auto* pFn = reinterpret_cast<Fn*>(dst->m_storage);
						switch (op) {
							case Op::Invoke:
								GAIA_ASSERT(pArgs != nullptr);
								(*pFn)(*pArgs);
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

					m_func = [](Op op, JobArgsFunc* dst, JobArgsFunc* src, const JobArgs* pArgs) {
						auto*& pFn = *reinterpret_cast<Fn**>(dst->m_storage);
						switch (op) {
							case Op::Invoke:
								GAIA_ASSERT(pArgs != nullptr);
								GAIA_ASSERT(pFn != nullptr);
								(*pFn)(*pArgs);
								break;
							case Op::Destroy:
								GAIA_ASSERT(pFn != nullptr);
								if constexpr (!std::is_trivially_destructible_v<Fn>)
									pFn->~Fn();
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
			}

		public:
			JobArgsFunc() = default;
			~JobArgsFunc() {
				destroy();
			}

			JobArgsFunc(const JobArgsFunc&) = delete;
			JobArgsFunc& operator=(const JobArgsFunc&) = delete;

			JobArgsFunc(JobArgsFunc&& other) noexcept {
				if (other.m_func != nullptr)
					other.m_func(Op::Move, this, &other, nullptr);
			}

			JobArgsFunc& operator=(JobArgsFunc&& other) noexcept {
				if (this != &other) {
					destroy();
					if (other.m_func != nullptr)
						other.m_func(Op::Move, this, &other, nullptr);
				}
				return *this;
			}

			template <typename F, typename = std::enable_if_t<!std::is_same_v<std::decay_t<F>, JobArgsFunc>>>
			JobArgsFunc(F&& f) {
				init(GAIA_FWD(f));
			}

			template <typename F, typename = std::enable_if_t<!std::is_same_v<std::decay_t<F>, JobArgsFunc>>>
			JobArgsFunc& operator=(F&& f) {
				destroy();
				init(GAIA_FWD(f));
				return *this;
			}

			template <typename F>
			static JobArgsFunc create(F&& f) {
				JobArgsFunc func;
				func.init(GAIA_FWD(f));
				return func;
			}

			void exec(const JobArgs& args) const {
				GAIA_ASSERT(m_func != nullptr);
				m_func(Op::Invoke, const_cast<JobArgsFunc*>(this), nullptr, &args);
			}

			void operator()(const JobArgs& args) const {
				exec(args);
			}

			void reset() {
				destroy();
			}

			explicit operator bool() const {
				return m_func != nullptr;
			}
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
