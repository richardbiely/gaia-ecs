#pragma once

#include "../config/config.h"
#include "../config/profiler.h"

#if GAIA_PLATFORM_WINDOWS
	#include <cstdio>
	#include <windows.h>
	#define GAIA_THREAD std::thread
#elif GAIA_PLATFORM_APPLE
	#include <pthread.h>
	#include <pthread/sched.h>
	#include <sys/qos.h>
	#define GAIA_THREAD pthread_t
#elif GAIA_PLATFORM_LINUX || GAIA_PLATFORM_FREEBSD
	#include <pthread.h>
	#define GAIA_THREAD pthread_t
#endif

#if GAIA_PLATFORM_WINDOWS
	#include <malloc.h>
#else
	#include <alloca.h>
#endif
#include <atomic>
#include <thread>

#include "../cnt/sarray_ext.h"
#include "../config/logging.h"
#include "../core/span.h"
#include "../core/utility.h"

#include "event.h"
#include "futex.h"
#include "jobcommon.h"
#include "jobhandle.h"
#include "jobmanager.h"
#include "jobqueue.h"
#include "semaphore_fast.h"

namespace gaia {
	namespace mt {
#if GAIA_PLATFORM_WINDOWS
		extern "C" typedef HRESULT(WINAPI* TOSApiFunc_SetThreadDescription)(HANDLE, PCWSTR);

	#pragma pack(push, 8)
		typedef struct tagTHREADNAME_INFO {
			DWORD dwType; // Must be 0x1000.
			LPCSTR szName; // Pointer to name (in user addr space).
			DWORD dwThreadID; // Thread ID (-1=caller thread).
			DWORD dwFlags; // Reserved for future use, must be zero.
		} THREADNAME_INFO;
	#pragma pack(pop)
#endif

		namespace detail {
			//! Worker context for threads allocated by our thread pool
			inline thread_local ThreadCtx* tl_workerCtx;
		} // namespace detail

		class ThreadPool final {
			friend class JobManager;

			//! Maximum number of worker threads of a given priority we can create.
			//! TODO: The current implementation puts a hard limit on the number
			//!       of workers. In the future consider revisiting this because
			//!       the number of CPU cores is only going to increase.
			static constexpr uint32_t MaxWorkers = JobState::DEP_BITS;

			//! ID of the main thread
			std::thread::id m_mainThreadId;

			//! When true the pool is supposed to finish all work and terminate all threads
			std::atomic_bool m_stop{};
			//! Array of worker threads
			cnt::sarray_ext<GAIA_THREAD, MaxWorkers> m_workers;
			//! Array of data associated with workers
			ThreadCtx m_workerCtxMain;
			cnt::sarray_ext<ThreadCtx, MaxWorkers> m_workersCtx;
			//! Global job queue
			MpmcQueue<JobHandle, 1024> m_jobQueue[JobPriorityCnt];
			//! The number of workers dedicated for a given level of job priority
			uint32_t m_workersCnt[JobPriorityCnt]{};
			//! Semaphore use to
			SemaphoreFast m_sem;

			//! Futex counter
			std::atomic_uint32_t m_blockedInWorkUntil;

			//! Manager for internal jobs
			JobManager m_jobManager;
			//! Job allocation mutex
			GAIA_PROF_MUTEX(std::mutex, m_jobAllocMtx);

		private:
			ThreadPool() {
				m_stop.store(false);

				make_main_thread();

				const auto hwThreads = hw_thread_cnt();
				set_max_workers(hwThreads, hwThreads);
			}

			ThreadPool(ThreadPool&&) = delete;
			ThreadPool(const ThreadPool&) = delete;
			ThreadPool& operator=(ThreadPool&&) = delete;
			ThreadPool& operator=(const ThreadPool&) = delete;

		public:
			static ThreadPool& get() {
				static ThreadPool threadPool;
				return threadPool;
			}

			~ThreadPool() {
				reset();
			}

			//! Make the calling thread the effective main thread from the thread pool perspective
			void make_main_thread() {
				m_mainThreadId = std::this_thread::get_id();
			}

			//! Returns the number of worker threads
			GAIA_NODISCARD uint32_t workers() const {
				return m_workers.size();
			}

			//! Set the maximum number of workers for this system.
			//! \param count Requested number of worker threads to create
			//! \param countHighPrio HighPrio Number of high-priority workers to create.
			//!                      Calculated as Max(count, countHighPrio).
			//! \warning All jobs are finished first before threads are recreated.
			void set_max_workers(uint32_t count, uint32_t countHighPrio) {
				const auto workersCnt = core::get_max(core::get_min(MaxWorkers, count), 1U);
				if (countHighPrio > count)
					countHighPrio = count;

				// Stop all threads first
				reset();

				// Reset previous worker contexts
				for (auto& ctx: m_workersCtx)
					ctx.reset();

				// Resize or array
				m_workersCtx.resize(workersCnt);
				// We also have the main thread so there's always one less worker spawned
				m_workers.resize(workersCnt - 1);

				// First worker is considered the main thread.
				// It is also assigned high priority but it doesn't really matter.
				// The main thread can steal any jobs, both low and high priority.
				detail::tl_workerCtx = &m_workersCtx[0];
				m_workersCtx[0].tp = this;
				m_workersCtx[0].workerIdx = 0;
				m_workersCtx[0].prio = JobPriority::High;

				// Reset the workers
				for (auto& worker: m_workers)
					worker = {};

				// Create a new set of high and low priority threads (if any)
				uint32_t workerIdx = 1;
				set_workers_high_prio_inter(workerIdx, countHighPrio);
			}

			//! Updates the number of worker threads participating at high priority workloads
			//! \param count Number of high priority workers
			//! \warning All jobs are finished first before threads are recreated.
			void set_workers_high_prio_inter(uint32_t& workerIdx, uint32_t count) {
				count = gaia::core::get_min(count, m_workers.size());
				if (count == 0) {
					m_workersCnt[0] = 1; // main thread
					m_workersCnt[1] = 0;
				} else {
					m_workersCnt[0] = count + 1; // Main thread is always a priority worker
					m_workersCnt[1] = m_workers.size() - count;
				}

				// Create a new set of high and low priority threads (if any)
				create_worker_threads(workerIdx, JobPriority::High, m_workersCnt[0] - 1);
				create_worker_threads(workerIdx, JobPriority::Low, m_workersCnt[1]);
			}

			//! Updates the number of worker threads participating at low priority workloads
			//! \warning All jobs are finished first before threads are recreated.
			void set_workers_low_prio_inter(uint32_t& workerIdx, uint32_t count) {
				const uint32_t realCnt = gaia::core::get_max(count, m_workers.size());
				if (realCnt == 0) {
					m_workersCnt[0] = 0;
					m_workersCnt[1] = 1; // main thread
				} else {
					m_workersCnt[0] = m_workers.size() - realCnt;
					m_workersCnt[1] = realCnt + 1; // Main thread is always a priority worker;
				}

				// Create a new set of high and low priority threads (if any)
				create_worker_threads(workerIdx, JobPriority::High, m_workersCnt[0]);
				create_worker_threads(workerIdx, JobPriority::Low, m_workersCnt[1]);
			}

			//! Updates the number of worker threads participating at high priority workloads
			//! \warning All jobs are finished first before threads are recreated.
			void set_workers_high_prio(uint32_t count) {
				// Stop all threads first
				reset();

				uint32_t workerIdx = 1;
				set_workers_high_prio_inter(workerIdx, count);
			}

			//! Updates the number of worker threads participating at low priority workloads
			//! \warning All jobs are finished first before threads are recreated.
			void set_workers_low_prio(uint32_t count) {
				// Stop all threads first
				reset();

				uint32_t workerIdx = 1;
				set_workers_low_prio_inter(workerIdx, count);
			}

			//! Makes \param jobSecond depend on \param jobFirst.
			//! This means \param jobSecond will run only after \param jobFirst finishes.
			//! \warning Must be used from the main thread.
			//! \warning Needs to be called before any of the listed jobs are scheduled.
			void dep(JobHandle jobFirst, JobHandle jobSecond) {
				GAIA_ASSERT(main_thread());
				m_jobManager.dep(std::span(&jobFirst, 1), jobSecond);
			}

			//! Makes \param jobHandle depend on the jobs listed in \param dependsOnSpan.
			//! This means \param jobHandle will run only after all \param dependsOnSpan jobs finish.
			//! \warning Must be used from the main thread.
			//! \warning Needs to be called before any of the listed jobs are scheduled.
			void dep(std::span<JobHandle> jobsFirst, JobHandle jobSecond) {
				GAIA_ASSERT(main_thread());
				m_jobManager.dep(jobsFirst, jobSecond);
			}

			//! Creates a threadpool job from \param job.
			//! \warning Must be used from the main thread.
			//! \return Job handle of the scheduled job.
			JobHandle add(Job& job) {
				GAIA_ASSERT(main_thread());

				// Don't add new jobs once stop was requested
				if GAIA_UNLIKELY (m_stop)
					return JobNull;

				job.priority = final_prio(job);

				// auto& mtx = GAIA_PROF_EXTRACT_MUTEX(std::mutex, m_jobAllocMtx);
				// std::lock_guard lock(mtx);
				return m_jobManager.alloc_job(job);
			}

			//! Deletes a job handle \param jobHandle from the threadpool.
			//! \warning Must be used from the main thread.
			//! \warning Job handle must not be used by any worker thread and can not be used
			//!          by any active job handles as a dependency.
			// TODO: Figure out how to do this automatically without user intervention.
			//       Only the private free_job should be used.
			void del(JobHandle jobHandle) {
				m_jobManager.free_job(jobHandle);
			}

			//! Deletes job handles \param jobHandles from the threadpool.
			//! \warning Must be used from the main thread.
			//! \warning Job handles must not be used by any worker thread and can not be used
			//!          by any active job handles as a dependency.
			// TODO: Figure out how to do this automatically without user intervention.
			//       Only the private free_job should be used.
			void del(std::span<JobHandle> jobHandles) {
				for (auto jobHandle: jobHandles)
					m_jobManager.free_job(jobHandle);
			}

			//! Pushes \param jobHandles into the internal queue so worker threads
			//! can pick them up and execute them.
			//! If there are more jobs than the queue can handle it puts the calling
			//! thread to sleep until workers consume enough jobs.
			//! \warning Once submitted, dependencies can't be modified for this job.
			void submit(std::span<JobHandle> jobHandles) {
				if (jobHandles.empty())
					return;

				GAIA_PROF_SCOPE(tp::submit);

				auto* pHandles = (JobHandle*)alloca(sizeof(JobHandle) * jobHandles.size());

				uint32_t cnt = 0;
				for (auto handle: jobHandles) {
					auto& jobData = m_jobManager.data(handle);
					const auto state = m_jobManager.submit(jobData) & JobState::DEP_BITS_MASK;
					// Jobs that were already submitted won't be submitted again.
					// We can only accept the job if it has no pending dependencies.
					if (state != 0)
						continue;

					pHandles[cnt++] = handle;
				}

				auto* ctx = detail::tl_workerCtx;
				process(std::span(pHandles, cnt), ctx);
			}

			//! Pushes \param jobHandle into the internal queue so worker threads
			//! can pick it up and execute it.
			//! If there are more jobs than the queue can handle it puts the calling
			//! thread to sleep until workers consume enough jobs.
			//! \warning Once submitted, dependencies can't be modified for this job.
			void submit(JobHandle jobHandle) {
				submit(std::span(&jobHandle, 1));
			}

			//! Schedules a job to run on a worker thread.
			//! \param job Job descriptor
			//! \warning Must be used from the main thread.
			//! \warning Dependencies can't be modified for this job.
			//! \return Job handle of the scheduled job.
			JobHandle sched(Job& job) {
				JobHandle jobHandle = add(job);
				submit(jobHandle);
				return jobHandle;
			}

			//! Schedules a job to run on a worker thread.
			//! \param job Job descriptor
			//! \param dependsOn Job we depend on
			//! \warning Must be used from the main thread.
			//! \warning Dependencies can't be modified for this job.
			//! \return Job handle of the scheduled job.
			JobHandle sched(Job& job, JobHandle dependsOn) {
				JobHandle jobHandle = add(job);
				dep(jobHandle, dependsOn);
				submit(jobHandle);
				return jobHandle;
			}

			//! Schedules a job to run on worker threads in parallel.
			//! \param job Job descriptor
			//! \param itemsToProcess Total number of work items
			//! \param groupSize Group size per created job. If zero the threadpool decides the group size.
			//! \warning Must be used from the main thread.
			//! \warning Dependencies can't be modified for this job.
			//! \return Job handle of the scheduled batch of jobs.
			JobHandle sched_par(JobParallel& job, uint32_t itemsToProcess, uint32_t groupSize) {
				GAIA_ASSERT(main_thread());

				// Empty data set are considered wrong inputs
				GAIA_ASSERT(itemsToProcess != 0);
				if (itemsToProcess == 0)
					return JobNull;

				// Don't add new jobs once stop was requested
				if GAIA_UNLIKELY (m_stop)
					return JobNull;

				// Make sure the right priority is selected
				const auto prio = job.priority = final_prio(job);

				// No group size was given, make a guess based on the set size
				if (groupSize == 0) {
					const auto cntWorkers = m_workersCnt[(uint32_t)prio];
					groupSize = (itemsToProcess + cntWorkers - 1) / cntWorkers;

					// If there are too many items we split them into multiple jobs.
					// This way, if we wait for the result and some workers finish
					// with our task faster, the finished worker can pick up a new
					// job faster.
					// On the other hand, too little items probably don't deserve
					// multiple jobs.
					constexpr uint32_t maxUnitsOfWorkPerGroup = 8;
					groupSize = groupSize / maxUnitsOfWorkPerGroup;
					if (groupSize <= 0)
						groupSize = 1;
				}

				const auto jobs = (itemsToProcess + groupSize - 1) / groupSize;

				// Create one job per group
				uint32_t jobIndex = 0;
				auto groupJobFunc = [job, itemsToProcess, groupSize, jobIndex]() {
					const uint32_t groupJobIdxStart = jobIndex * groupSize;
					const uint32_t groupJobIdxStartPlusGroupSize = groupJobIdxStart + groupSize;
					const uint32_t groupJobIdxEnd =
							groupJobIdxStartPlusGroupSize < itemsToProcess ? groupJobIdxStartPlusGroupSize : itemsToProcess;

					JobArgs args;
					args.idxStart = groupJobIdxStart;
					args.idxEnd = groupJobIdxEnd;
					job.func(args);
				};

				// Only one job is created, use the job directly.
				// Generally, this is the case we would want to avoid because it means this particular case
				// is not worth of being scheduled via sched_par. However, we can never know for sure what
				// the reason for that is so let's stay silent.
				if (jobs == 1) {
					auto handle = m_jobManager.alloc_job({groupJobFunc, prio});
					submit(handle);
					return handle;
				}

				// Multiple jobs need to be parallelized.
				// Create a sync job and assign it as their dependency.
				auto* pHandles = (JobHandle*)alloca(sizeof(JobHandle) * (jobs + 1));

				// Sync job
				auto syncHandle = m_jobManager.alloc_job({{}, prio});
				GAIA_ASSERT(m_jobManager.is_clear(syncHandle));

				// Work jobs
				for (jobIndex = 0; jobIndex < jobs; ++jobIndex) {
					pHandles[jobIndex] = m_jobManager.alloc_job({groupJobFunc, prio});
					GAIA_ASSERT(m_jobManager.is_clear(pHandles[jobIndex]));
				}
				pHandles[jobs] = syncHandle;

				// Assign the sync jobs as a dependency for work jobs
				dep(std::span(pHandles, jobs), pHandles[jobs]);

				// Sumbit the jobs to the threadpool.
				// This is a point of no return. After this point no more changes to jobs are possible.
				submit(std::span(pHandles, jobs + 1));
				return pHandles[jobs];
			}

			//! Wait until a job associated with the jobHandle finishes executing.
			//! Cleans up any job allocations and dependencies associated with \param jobHandle
			//! The calling thread participate in job processing until \param jobHandle is done.
			void wait(JobHandle jobHandle) {
				GAIA_PROF_SCOPE(tp::wait);

				GAIA_ASSERT(main_thread());

				auto* ctx = detail::tl_workerCtx;
				auto& jobData = m_jobManager.data(jobHandle);
				auto state = jobData.state.load();

				// Waiting for a job that has not been initialized is nonsense.
				GAIA_ASSERT(state != 0);

				// Wait until done
				for (; (state & JobState::STATE_BITS_MASK) != JobState::Done; state = jobData.state.load()) {
					// The job we are waiting for is not finished yet, try running some other job in the meantime
					JobHandle otherJobHandle;
					if (try_fetch_job(*ctx, otherJobHandle)) {
						if (run(otherJobHandle, ctx)) {
							free_job(otherJobHandle);
							continue;
						}
					}

					// The job we are waiting for is already running.
					// Wait until it signals it's finished.
					if ((state & JobState::STATE_BITS_MASK) == JobState::Executing) {
						const auto workerId = (state & JobState::DEP_BITS_MASK);
						auto* jobDoneEvent = &m_workersCtx[workerId].event;
						jobDoneEvent->wait();
						continue;
					}

					// The worst case scenario.
					// We have nothing to do and the job we are waiting for is not executing still.
					// Let's wait for any job to start executing.
					const auto workerBit = 1U << ctx->workerIdx;
					const auto oldBlockedMask = m_blockedInWorkUntil.fetch_or(workerBit);
					if (jobData.state.load() == state) // still not JobState::Done?
						Futex::wait(&m_blockedInWorkUntil, oldBlockedMask | workerBit, detail::WaitMaskAny);
					m_blockedInWorkUntil.fetch_and(~workerBit);
				}

				// Deallocate the job itself
				free_job(jobHandle);
			}

			//! Uses the main thread to help with jobs processing.
			void update() {
				GAIA_ASSERT(main_thread());
				main_thread_tick();
			}

			//! Returns the number of HW threads available on the system. 1 is minimum.
			//! \return The number of hardware threads or 1 if failed.
			GAIA_NODISCARD static uint32_t hw_thread_cnt() {
				auto hwThreads = (uint32_t)std::thread::hardware_concurrency();
				return core::get_max(1U, hwThreads);
			}

		private:
			static void* thread_func(void* pCtx) {
				auto& ctx = *(ThreadCtx*)pCtx;

				detail::tl_workerCtx = &ctx;

				// Set the worker thread name.
				// Needs to be called from inside the thread because some platforms
				// can change the name only when run from the specific thread.
				ctx.tp->set_thread_name(ctx.workerIdx, ctx.prio);

				// Set the worker thread priority
				ctx.tp->set_thread_priority(ctx.workerIdx, ctx.prio);

				// Process jobs
				ctx.tp->worker_loop(ctx);

				detail::tl_workerCtx = nullptr;

				return nullptr;
			}

			//! Creates a thread
			//! \param workerIdx Worker index
			//! \param prio Priority used for the thread
			void create_thread(uint32_t workerIdx, JobPriority prio) {
				// Idx 0 is reserved for the main thread
				GAIA_ASSERT(workerIdx > 0);

				auto& ctx = m_workersCtx[workerIdx];
				ctx.tp = this;
				ctx.workerIdx = workerIdx;
				ctx.prio = prio;

#if GAIA_PLATFORM_WINDOWS
				m_workers[workerIdx - 1] = std::thread([&ctx]() {
					thread_func((void*)&ctx);
				});
#else
				pthread_attr_t attr{};
				int ret = pthread_attr_init(&attr);
				if (ret != 0) {
					GAIA_LOG_W("pthread_attr_init failed for worker thread %u. ErrCode = %d", workerIdx, ret);
					return;
				}

				///////////////////////////////////////////////////////////////////////////
				// Apple's recommendation for Apple Silicon for games / high-perf software
				// ========================================================================
				// Per frame              | Scheduling policy     | QoS class / Priority
				// ========================================================================
				// Main thread            |     SCHED_OTHER       | QOS_CLASS_USER_INTERACTIVE (47)
				// Render/Audio thread    |     SCHED_RR          | 45
				// Workers High Prio      |     SCHED_RR          | 39-41
				// Workers Low Prio       |     SCHED_OTHER       | QOS_CLASS_USER_INTERACTIVE (38)
				// ========================================================================
				// Multiple-frames        |                       |
				// ========================================================================
				// Async Workers High Prio|     SCHED_OTHER       | QOS_CLASS_USER_INITIATED (37)
				// Async Workers Low Prio |     SCHED_OTHER       | QOS_CLASS_DEFAULT (31)
				// Prefetching/Streaming  |     SCHED_OTHER       | QOS_CLASS_UTILITY (20)
				// ========================================================================

				if (prio == JobPriority::Low) {
	#if GAIA_PLATFORM_APPLE
					ret = pthread_attr_set_qos_class_np(&attr, QOS_CLASS_USER_INTERACTIVE, -9); // 47-9=38
					if (ret != 0) {
						GAIA_LOG_W(
								"pthread_attr_set_qos_class_np failed for worker thread %u [prio=%u]. ErrCode = %d", workerIdx,
								(uint32_t)prio, ret);
					}
	#else
					ret = pthread_attr_setschedpolicy(&attr, SCHED_OTHER);
					if (ret != 0) {
						GAIA_LOG_W(
								"pthread_attr_setschedpolicy SCHED_RR failed for worker thread %u [prio=%u]. ErrCode = %d", workerIdx,
								(uint32_t)prio, ret);
					}

					int prioMax = core::get_min(38, sched_get_priority_max(SCHED_OTHER));
					int prioMin = core::get_min(prioMax, sched_get_priority_min(SCHED_OTHER));
					int prioUse = core::get_min(prioMin + 5, prioMax);
					prioUse = core::get_max(prioUse, prioMin);
					sched_param param{};
					param.sched_priority = prioUse;

					ret = pthread_attr_setschedparam(&attr, &param);
					if (ret != 0) {
						GAIA_LOG_W(
								"pthread_attr_setschedparam %d failed for worker thread %u [prio=%u]. ErrCode = %d",
								param.sched_priority, workerIdx, (uint32_t)prio, ret);
					}
	#endif
				} else {
					ret = pthread_attr_setschedpolicy(&attr, SCHED_RR);
					if (ret != 0) {
						GAIA_LOG_W(
								"pthread_attr_setschedpolicy SCHED_RR failed for worker thread %u [prio=%u]. ErrCode = %d", workerIdx,
								(uint32_t)prio, ret);
					}

					int prioMax = core::get_min(41, sched_get_priority_max(SCHED_RR));
					int prioMin = core::get_min(prioMax, sched_get_priority_min(SCHED_RR));
					int prioUse = core::get_max(prioMax - 5, prioMin);
					prioUse = core::get_min(prioUse, prioMax);
					sched_param param{};
					param.sched_priority = prioUse;

					ret = pthread_attr_setschedparam(&attr, &param);
					if (ret != 0) {
						GAIA_LOG_W(
								"pthread_attr_setschedparam %d failed for worker thread %u [prio=%u]. ErrCode = %d",
								param.sched_priority, workerIdx, (uint32_t)prio, ret);
					}
				}

				// Create the thread with given attributes
				ret = pthread_create(&m_workers[workerIdx - 1], &attr, thread_func, (void*)&ctx);
				if (ret != 0) {
					GAIA_LOG_W("pthread_create failed for worker thread %u. ErrCode = %d", workerIdx, ret);
				}

				pthread_attr_destroy(&attr);
#endif

				// Stick each thread to a specific CPU core if possible
				set_thread_affinity(workerIdx);
			}

			//! Joins a worker thread with the main thread (graceful thread termination)
			//! \param workerIdx Worker index
			void join_thread(uint32_t workerIdx) {
				if GAIA_UNLIKELY (workerIdx > m_workers.size())
					return;

#if GAIA_PLATFORM_WINDOWS
				auto& t = m_workers[workerIdx - 1];
				if (t.joinable())
					t.join();
#else
				auto& t = m_workers[workerIdx - 1];
				pthread_join(t, nullptr);
#endif
			}

			void create_worker_threads(uint32_t& workerIdx, JobPriority prio, uint32_t count) {
				for (uint32_t i = 0; i < count; ++i)
					create_thread(workerIdx++, prio);
			}

			void set_thread_priority([[maybe_unused]] uint32_t workerIdx, [[maybe_unused]] JobPriority priority) {
#if GAIA_PLATFORM_WINDOWS
				HANDLE nativeHandle = (HANDLE)m_workers[workerIdx - 1].native_handle();

				THREAD_POWER_THROTTLING_STATE state{};
				state.Version = THREAD_POWER_THROTTLING_CURRENT_VERSION;
				if (priority == JobPriority::High) {
					// HighQoS
					// Turn EXECUTION_SPEED throttling off.
					// ControlMask selects the mechanism and StateMask is set to zero as mechanisms should be turned off.
					state.ControlMask = THREAD_POWER_THROTTLING_EXECUTION_SPEED;
					state.StateMask = 0;
				} else {
					// EcoQoS
					// Turn EXECUTION_SPEED throttling on.
					// ControlMask selects the mechanism and StateMask declares which mechanism should be on or off.
					state.ControlMask = THREAD_POWER_THROTTLING_EXECUTION_SPEED;
					state.StateMask = THREAD_POWER_THROTTLING_EXECUTION_SPEED;
				}

				BOOL ret = SetThreadInformation(nativeHandle, ThreadPowerThrottling, &state, sizeof(state));
				if (ret != TRUE) {
					GAIA_LOG_W("SetThreadInformation failed for thread %u", workerIdx);
					return;
				}
#else
				// Done when the thread is created
#endif
			}

			void set_thread_affinity([[maybe_unused]] uint32_t workerIdx) {
				// NOTE:
				// Some cores might have multiple logic threads, there might be
				// more sockets and some cores might even be physically different
				// form others (performance vs efficiency cores).
				// Because of that, do not handle affinity and let the OS figure it out.
				// All treads created by the pool are setting thread priorities to make
				// it easier for the OS.

				// #if GAIA_PLATFORM_WINDOWS
				// 				HANDLE nativeHandle = (HANDLE)m_workers[workerIdx-1].native_handle();
				//
				// 				auto mask = SetThreadAffinityMask(nativeHandle, 1ULL << workerIdx);
				// 				if (mask <= 0)
				// 					GAIA_LOG_W("Issue setting thread affinity for worker thread %u!", workerIdx);
				// #elif GAIA_PLATFORM_APPLE
				// 				// Do not do affinity for MacOS. If is not supported for Apple Silicon and
				// 				// Intel MACs are deprecated anyway.
				// 				// TODO: Consider supporting this at least for Intel MAC as there are still
				// 				//       quite of few of them out there.
				// #elif GAIA_PLATFORM_LINUX || GAIA_PLATFORM_FREEBSD
				// 				pthread_t nativeHandle = (pthread_t)m_workers[workerIdx-1].native_handle();
				//
				// 				cpu_set_t cpuSet;
				// 				CPU_ZERO(&cpuSet);
				// 				CPU_SET(workerIdx, &cpuSet);
				//
				// 				auto ret = pthread_setaffinity_np(nativeHandle, sizeof(cpuSet), &cpuSet);
				// 				if (ret != 0)
				// 					GAIA_LOG_W("Issue setting thread affinity for worker thread %u!", workerIdx);
				//
				// 				ret = pthread_getaffinity_np(nativeHandle, sizeof(cpuSet), &cpuSet);
				// 				if (ret != 0)
				// 					GAIA_LOG_W("Thread affinity could not be set for worker thread %u!", workerIdx);
				// #endif
			}

			//! Updates the name of a given thread. It is based on the index and priority of the worker.
			//! \param workerIdx Index of the worker
			//! \param prio Worker priority
			void set_thread_name(uint32_t workerIdx, JobPriority prio) {
#if GAIA_PROF_USE_PROFILER_THREAD_NAME
				char threadName[16]{};
				snprintf(threadName, 16, "worker_%s_%u", prio == JobPriority::High ? "HI" : "LO", workerIdx);
				GAIA_PROF_THREAD_NAME(threadName);
#elif GAIA_PLATFORM_WINDOWS
				auto nativeHandle = (HANDLE)m_workers[workerIdx - 1].native_handle();

				TOSApiFunc_SetThreadDescription pSetThreadDescFunc = nullptr;
				if (auto* pModule = GetModuleHandleA("kernel32.dll"))
					pSetThreadDescFunc = (TOSApiFunc_SetThreadDescription)GetProcAddress(pModule, "SetThreadDescription");
				if (pSetThreadDescFunc != nullptr) {
					wchar_t threadName[16]{};
					swprintf_s(threadName, L"worker_%s_%u", prio == JobPriority::High ? L"HI" : L"LO", workerIdx);

					auto hr = pSetThreadDescFunc(nativeHandle, threadName);
					if (FAILED(hr)) {
						GAIA_LOG_W(
								"Issue setting name for worker %s thread %u!", prio == JobPriority::High ? "HI" : "LO", workerIdx);
					}
				} else {
	#if defined _MSC_VER
					char threadName[16]{};
					snprintf(threadName, 16, "worker_%s_%u", prio == JobPriority::High ? "HI" : "LO", workerIdx);

					THREADNAME_INFO info{};
					info.dwType = 0x1000;
					info.szName = threadName;
					info.dwThreadID = GetThreadId(nativeHandle);

					__try {
						RaiseException(0x406D1388, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
					} __except (EXCEPTION_EXECUTE_HANDLER) {
					}
	#endif
				}
#elif GAIA_PLATFORM_APPLE
				char threadName[16]{};
				snprintf(threadName, 16, "worker_%s_%u", prio == JobPriority::High ? "HI" : "LO", workerIdx);
				auto ret = pthread_setname_np(threadName);
				if (ret != 0)
					GAIA_LOG_W("Issue setting name for worker %s thread %u!", prio == JobPriority::High ? "HI" : "LO", workerIdx);
#elif GAIA_PLATFORM_LINUX || GAIA_PLATFORM_FREEBSD
				auto nativeHandle = m_workers[workerIdx - 1];

				char threadName[16]{};
				snprintf(threadName, 16, "worker_%s_%u", prio == JobPriority::High ? "HI" : "LO", workerIdx);
				GAIA_PROF_THREAD_NAME(threadName);
				auto ret = pthread_setname_np(nativeHandle, threadName);
				if (ret != 0)
					GAIA_LOG_W("Issue setting name for worker %s thread %u!", prio == JobPriority::High ? "HI" : "LO", workerIdx);
#endif
			}

			//! Checks if the calling thread is considered the main thread
			//! \return True if the calling thread is the main thread. False otherwise.
			GAIA_NODISCARD bool main_thread() const {
				return std::this_thread::get_id() == m_mainThreadId;
			}

			//! Loop run by main thread. Pops jobs from the given queue
			//! and executes it.
			//! \param prio Target worker queue defined by job priority
			//! \return True if a job was resubmitted or executed. False otherwise.
			void main_thread_tick() {
				auto& ctx = *detail::tl_workerCtx;

				// Keep executing while there is work
				while (true) {
					JobHandle jobHandle;
					if (!try_fetch_job(ctx, jobHandle))
						break;

					if (run(jobHandle, &ctx))
						free_job(jobHandle);
				}
			}

			bool try_fetch_job(ThreadCtx& ctx, JobHandle& jobHandle) {
				// Try getting a job from the local queue
				if (ctx.jobQueue.try_pop(jobHandle))
					return true;

				// Try getting a job from the global queue
				if (m_jobQueue[(uint32_t)ctx.prio].try_pop(jobHandle))
					return true;

				// Could not get a job, try stealing from other workers.
				const auto workerCnt = m_workersCtx.size();
				for (uint32_t i = 0; i < workerCnt;) {
					// We need to skip our worker
					if (i == ctx.workerIdx) {
						++i;
						continue;
					}

					// Try stealing a job
					const auto res = m_workersCtx[i].jobQueue.try_steal(jobHandle);

					// Race condition, try again from the same context
					if (!res)
						continue;

					// Stealing can return true if the queue is empty.
					// We return right away only if we receive a valid handle which means
					// when there was an idle job in the queue.
					if (res && jobHandle != (JobHandle)JobNull_t{})
						return true;

					++i;
				}

				return false;
			}

			//! Loop run by worker threads. Pops jobs from the given queue
			//! and executes it.
			//! \param prio Target worker queue defined by job priority
			void worker_loop(ThreadCtx& ctx) {
				while (true) {
					// Wait for work
					m_sem.wait();

					// Keep executing while there is work
					while (true) {
						JobHandle jobHandle;
						if (!try_fetch_job(ctx, jobHandle))
							break;

						if (run(jobHandle, detail::tl_workerCtx))
							free_job(jobHandle);
					}

					// Check if the worker can keep running
					const bool stop = m_stop.load();
					if (stop)
						break;
				}
			}

			//! Finishes all jobs and stops all worker threads
			void reset() {
				if (m_workers.empty())
					return;

				// Request stopping
				m_stop.store(true);

				// Signal all threads
				m_sem.release((int32_t)m_workers.size());

				auto* ctx = detail::tl_workerCtx;

				// Finish remaining jobs
				JobHandle jobHandle;
				while (try_fetch_job(*ctx, jobHandle)) {
					if (run(jobHandle, ctx))
						free_job(jobHandle);
				}

				detail::tl_workerCtx = nullptr;

				// Join threads with the main one
				GAIA_FOR(m_workers.size()) join_thread(i + 1);

				// All threads have been stopped. Allow new threads to run if necessary.
				m_stop.store(false);
			}

			//! Makes sure the priority is right for the given set of allocated workers
			template <typename TJob>
			JobPriority final_prio(const TJob& job) {
				const auto cntWorkers = m_workersCnt[(uint32_t)job.priority];
				return cntWorkers > 0
									 // If there is enough workers, keep the priority
									 ? job.priority
									 // Not enough workers, use the other priority that has workers
									 : (JobPriority)(((uint32_t)job.priority + 1U) % (uint32_t)JobPriorityCnt);
			}

		private:
			void free_job([[maybe_unused]] JobHandle jobHandle) {
				// Allocs are done only from the main thread while there are no jobs running.
				// Freeing can happen at any point from any thread. Therefore, we need to lock it.
				// auto& mtx = GAIA_PROF_EXTRACT_MUTEX(std::mutex, m_jobAllocMtx);
				// std::lock_guard lock(mtx);
				// m_jobManager.free_job(jobHandle);
			}

			void signal_edges(JobContainer& jobData) {
				const auto max = jobData.edges.depCnt;

				// Nothing to do if there are no dependencies
				if (max == 0)
					return;

				auto* ctx = detail::tl_workerCtx;

				// One dependency
				if (max == 1) {
					auto depHandle = jobData.edges.dep;
#if GAIA_LOG_JOB_STATES
					GAIA_LOG_N("SIGNAL %u.%u -> %u.%u", jobData.idx, jobData.gen, depHandle.id(), depHandle.gen());
#endif

					// See the conditions can't be satisfied for us to submit the job we skip
					auto& depData = m_jobManager.data(depHandle);
					if (!JobManager::signal_edge(depData))
						return;

					// Submit all jobs that can are ready
					process(std::span(&depHandle, 1), ctx);
					return;
				}

				// Multiple dependencies. The array has to be set
				GAIA_ASSERT(jobData.edges.pDeps != nullptr);

				auto* pHandles = (JobHandle*)alloca(sizeof(JobHandle) * max);
				uint32_t cnt = 0;
				GAIA_FOR2(0, max) {
					auto depHandle = jobData.edges.pDeps[i];

					// See if all conditions were satisfied for us to submit the job
					auto& depData = m_jobManager.data(depHandle);
					if (!JobManager::signal_edge(depData))
						continue;

					pHandles[cnt++] = depHandle;
				}

				// Submit all jobs that can are ready
				process(std::span(pHandles, cnt), ctx);
			}

			void process(std::span<JobHandle> jobHandles, ThreadCtx* ctx) {
				auto* pHandles = (JobHandle*)alloca(sizeof(JobHandle) * jobHandles.size());
				uint32_t handlesCnt = 0;

				for (auto handle: jobHandles) {
					auto& jobData = m_jobManager.data(handle);
					m_jobManager.processing(jobData);

					// Jobs that have no functor assigned don't need to be enqueued.
					// We can "run" them right away. The only time where it makes
					// sense to create such a job is to create a sync job. E.g. when you
					// need to wait for N jobs, rather than waiting for each of them
					// separately you make them a dependency of a dummy/sync job and
					// wait just for that one.
					if (!jobData.func.operator bool()) {
						if (run(handle, ctx))
							free_job(handle);
					} else
						pHandles[handlesCnt++] = handle;
				}

				std::span handles(pHandles, handlesCnt);
				while (!handles.empty()) {
					// Try pushing all jobs
					uint32_t pushed = 0;
					if (ctx != nullptr) {
						pushed = ctx->jobQueue.try_push(handles);
					} else {
						for (auto handle: handles) {
							if (m_jobQueue[(uint32_t)handle.prio()].try_push(handle))
								pushed++;
							else
								break;
						}
					}

					// Lock the semaphore with the number of jobs me managed to push.
					// Number of workers if the upper bound.
					const auto cntWorkers = m_workersCnt[(uint32_t)ctx->prio];
					const auto cnt = (int32_t)core::get_min(pushed, cntWorkers);
					m_sem.release(cnt);

					handles = handles.subspan(pushed);
					if (!handles.empty()) {
						// The queue was full. Execute the job right away.
						if (run(handles[0], ctx))
							free_job(handles[0]);
						handles = handles.subspan(1);
					}
				}
			}

			bool run(JobHandle jobHandle, ThreadCtx* ctx) {
				if (jobHandle == (JobHandle)JobNull_t{})
					return false;

				auto& jobData = m_jobManager.data(jobHandle);
				const bool canWait = jobData.canWait;
				m_jobManager.executing(jobData, ctx->workerIdx);

				if (m_blockedInWorkUntil.load() != 0) {
					const auto blockedCnt = m_blockedInWorkUntil.exchange(0);
					if (blockedCnt != 0)
						Futex::wake(&m_blockedInWorkUntil, detail::WaitMaskAll);
				}

				GAIA_ASSERT(jobData.idx != (uint32_t)-1 && jobData.gen != (uint32_t)-1);

				// Run the functor associated with the job
				m_jobManager.run(jobData);

				// Signal the edges and release memory allocated for them if possible
				signal_edges(jobData);
				m_jobManager.free_edges(jobData);

				// Signal we finished
				ctx->event.set();
				if (canWait) {
					const auto* pFutexValue = &jobData.state;
					Futex::wake(pFutexValue, detail::WaitMaskAll);
				}

				return true;
			}
		};
	} // namespace mt
} // namespace gaia