#pragma once

#include "../config/config.h"

#if GAIA_PLATFORM_WINDOWS
	#include <windows.h>
	#include <cstdio>
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

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "../cnt/sarray_ext.h"
#include "../config/logging.h"
#include "../core/span.h"
#include "../core/utility.h"

#include "jobcommon.h"
#include "jobhandle.h"
#include "jobmanager.h"
#include "jobqueue.h"

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

		class ThreadPool final {
			static constexpr uint32_t MaxWorkers = 31;

			//! ID of the main thread
			std::thread::id m_mainThreadId;
			//! When true the pool is supposed to finish all work and terminate all threads
			bool m_stop{};
			//! List of worker threads
			cnt::sarr_ext<GAIA_THREAD, MaxWorkers> m_workers;
			//! The number of workers dedicated for a given level of job priority
			uint32_t m_workerCnt[JobPriorityCnt]{};

			//! Manager for internal jobs
			JobManager m_jobManager;

			//! How many jobs are currently being processed
			std::atomic_uint32_t m_jobsPending[JobPriorityCnt]{};
			//! Mutex protecting the access to a given queue
			GAIA_PROF_MUTEX(std::mutex, m_cvLock0);
			GAIA_PROF_MUTEX(std::mutex, m_cvLock1);
			//! Signals for given workers to wake up
			std::condition_variable m_cv[JobPriorityCnt];
			//! List of pending user jobs
			JobQueue m_jobQueue[JobPriorityCnt];

		private:
			ThreadPool() {
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

			//! Make the calling thread the effective main thread from the thread pool perscpetive
			void make_main_thread() {
				m_mainThreadId = std::this_thread::get_id();
			}

			//! Returns the number of worker threads
			GAIA_NODISCARD uint32_t workers() const {
				return m_workers.size();
			}

			void update_workers_cnt(uint32_t count) {
				if (count == 0) {
					// Use one because we still have the main thread
					m_workerCnt[0] = 1;
					m_workerCnt[1] = 1;
				} else {
					m_workerCnt[0] = count;
					m_workerCnt[1] = m_workers.size() - count;
				}
			}

			//! Set the maximum number of works for this system.
			//! \param count Requested number of worker threads to create
			//! \param countHighPrio HighPrio Number of high-priority workers to create.
			//!                      Calculated as Max(count, countHighPrio).
			//! \warning All jobs are finished first before threads are recreated.
			void set_max_workers(uint32_t count, uint32_t countHighPrio) {
				const auto workersCnt = core::get_min(MaxWorkers, count);
				countHighPrio = core::get_max(count, countHighPrio);

				// Stop all threads first
				reset();
				m_workers.resize(workersCnt);
				GAIA_EACH(m_workers) m_workers[i] = {};

				// Create a new set of high and low priority threads (if any)
				set_workers_high_prio(countHighPrio);
			}

			//! Updates the number of worker threads participating at high priority workloads
			//! \warning All jobs are finished first before threads are recreated.
			void set_workers_high_prio(uint32_t count) {
				const uint32_t realCnt = gaia::core::get_min(count, m_workers.size());

				// Stop all threads first
				reset();

				update_workers_cnt(realCnt);

				// Create a new set of high and low priority threads (if any)
				set_workers_high_prio_inter(0, m_workerCnt[0]);
				set_workers_low_prio_inter(m_workerCnt[0], m_workerCnt[1]);
			}

			//! Updates the number of worker threads participating at low priority workloads
			//! \warning All jobs are finished first before threads are recreated.
			void set_workers_low_prio(uint32_t count) {
				const uint32_t realCnt = gaia::core::get_max(count, m_workers.size());

				// Stop all threads first
				reset();

				update_workers_cnt(m_workers.size() - realCnt);

				// Create a new set of high and low priority threads (if any)
				set_workers_high_prio_inter(0, m_workerCnt[0]);
				set_workers_low_prio_inter(m_workerCnt[0], m_workerCnt[1]);
			}

			//! Makes \param jobHandle depend on \param dependsOn.
			//! This means \param jobHandle will run only after \param dependsOn finishes.
			//! \warning Must be used from the main thread.
			//! \warning Needs to be called before any of the listed jobs are scheduled.
			void dep(JobHandle jobHandle, JobHandle dependsOn) {
				m_jobManager.dep(jobHandle, dependsOn);
			}

			//! Makes \param jobHandle depend on the jobs listed in \param dependsOnSpan.
			//! This means \param jobHandle will run only after all \param dependsOnSpan jobs finish.
			//! \warning Must be used from the main thread.
			//! \warning Needs to be called before any of the listed jobs are scheduled.
			void dep(JobHandle jobHandle, std::span<const JobHandle> dependsOnSpan) {
				m_jobManager.dep(jobHandle, dependsOnSpan);
			}

			//! Creates a job system job from \param job.
			//! \warning Must be used from the main thread.
			//! \return Job handle of the scheduled job.
			JobHandle add(Job& job) {
				GAIA_ASSERT(main_thread());

				// Don't add new jobs once stop was requested
				if GAIA_UNLIKELY (m_stop)
					return JobNull;

				job.priority = final_prio(job);

				++m_jobsPending[(uint32_t)job.priority];
				return m_jobManager.alloc_job(job);
			}

			//! Pushes \param jobHandle into the internal queue so worker threads
			//! can pick it up and execute it.
			//! If there are more jobs than the queue can handle it puts the calling
			//! thread to sleep until workers consume enough jobs.
			//! \warning Once submited, dependencies can't be modified for this job.
			void submit(JobHandle jobHandle) {
				m_jobManager.submit(jobHandle);

				const auto prio = (JobPriority)jobHandle.prio();
				auto& jobQueue = m_jobQueue[(uint32_t)prio];
				auto& cv = m_cv[(uint32_t)prio];

				if GAIA_UNLIKELY (m_workers.empty()) {
					(void)jobQueue.try_push(jobHandle);
					main_thread_tick(prio);
					return;
				}

				// Try pushing a new job until we succeed.
				// The thread is put to sleep if pushing the jobs fails.
				while (!jobQueue.try_push(jobHandle))
					poll(prio);

				// Wake some worker thread
				cv.notify_one();
			}

		private:
			//! Resubmits \param jobHandle into the internal queue so worker threads
			//! can pick it up and execute it.
			//! If there are more jobs than the queue can handle it puts the calling
			//! thread to sleep until workers consume enough jobs.
			//! \warning Internal usage only. Only worker theads can decide to resubmit.
			void resubmit(JobHandle jobHandle) {
				m_jobManager.resubmit(jobHandle);

				const auto prio = (JobPriority)jobHandle.prio();
				auto& jobQueue = m_jobQueue[(uint32_t)prio];
				auto& cv = m_cv[(uint32_t)prio];

				if GAIA_UNLIKELY (m_workers.empty()) {
					(void)jobQueue.try_push(jobHandle);
					// Let the other parts of the code handle resubmittion (submit, update).
					// Otherwise, we would enter an endless recursion and stack overflow here.
					// -->  main_thread_tick(prio);
					return;
				}

				// Try pushing a new job until we succeed.
				// The thread is put to sleep if pushing the jobs fails.
				while (!jobQueue.try_push(jobHandle))
					poll(prio);

				// Wake some worker thread
				cv.notify_one();
			}

		public:
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

			//! Schedules a job to run on a worker thread.
			//! \param job Job descriptor
			//! \param dependsOnSpan Jobs we depend on
			//! \warning Must be used from the main thread.
			//! \warning Dependencies can't be modified for this job.
			//! \return Job handle of the scheduled job.
			JobHandle sched(Job& job, std::span<const JobHandle> dependsOnSpan) {
				JobHandle jobHandle = add(job);
				dep(jobHandle, dependsOnSpan);
				submit(jobHandle);
				return jobHandle;
			}

			//! Schedules a job to run on worker threads in parallel.
			//! \param job Job descriptor
			//! \param itemsToProcess Total number of work items
			//! \param groupSize Group size per created job. If zero the job system decides the group size.
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

				// Make sure the right priorty is selected
				const auto prio = job.priority = final_prio(job);

				const uint32_t workerCount = m_workerCnt[(uint32_t)prio];

				// No group size was given, make a guess based on the set size
				if (groupSize == 0)
					groupSize = (itemsToProcess + workerCount - 1) / workerCount;

				const auto jobs = (itemsToProcess + groupSize - 1) / groupSize;
				// Internal jobs + 1 for the groupHandle
				m_jobsPending[(uint32_t)prio] += (jobs + 1U);

				JobHandle groupHandle = m_jobManager.alloc_job({{}, prio});

				GAIA_FOR_(jobs, jobIndex) {
					// Create one job per group
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

					JobHandle jobHandle = m_jobManager.alloc_job({groupJobFunc, prio});
					dep(groupHandle, jobHandle);
					submit(jobHandle);
				}

				submit(groupHandle);
				return groupHandle;
			}

			//! Waits for the job to finish.
			//! \param jobHandle Job handle
			//! \warning Must be used from the main thread.
			void wait(JobHandle jobHandle) {
				// When no workers are available, execute all we have.
				if GAIA_UNLIKELY (m_workers.empty()) {
					update();
					m_jobManager.reset();
					return;
				}

				GAIA_ASSERT(main_thread());

				while (m_jobManager.busy(jobHandle))
					poll((JobPriority)jobHandle.prio());

				GAIA_ASSERT(!m_jobManager.busy(jobHandle));
				m_jobManager.wait(jobHandle);
			}

			//! Waits for all jobs to finish.
			//! \warning Must be used from the main thread.
			void wait_all() {
				// When no workers are available, execute all we have.
				if GAIA_UNLIKELY (m_workers.empty()) {
					update();
					m_jobManager.reset();
					return;
				}

				GAIA_ASSERT(main_thread());

				busy_poll_all();

#if GAIA_ASSERT_ENABLED
				// No jobs should be pending at this point
				GAIA_FOR(JobPriorityCnt) {
					GAIA_ASSERT(!busy((JobPriority)i));
				}
#endif

				m_jobManager.reset();
			}

			//! Uses the main thread to help with jobs processing.
			void update() {
				GAIA_ASSERT(main_thread());

				bool moreWork = false;
				do {
					// Participate at processing of high-performance tasks.
					// They have higher priority so execute them first.
					moreWork = main_thread_tick(JobPriority::High);

					// Participate at processing of low-performance tasks.
					moreWork |= main_thread_tick(JobPriority::Low);
				} while (moreWork);
			}

			//! Returns the number of HW threads available on the system minus 1 (the main thread).
			//! \return 0 if failed. Otherwise, the number of hardware threads minus 1 (1 is minimum).
			GAIA_NODISCARD static uint32_t hw_thread_cnt() {
				auto hwThreads = (uint32_t)std::thread::hardware_concurrency();
				if (hwThreads > 0)
					return core::get_max(1U, hwThreads - 1U);
				return hwThreads;
			}

		private:
			struct ThreadFuncCtx {
				ThreadPool* tp;
				uint32_t workerIdx;
				JobPriority prio;
			};

			static void* thread_func(void* pCtx) {
				const auto& ctx = *(const ThreadFuncCtx*)pCtx;

				// Set the worker thread name.
				// Needs to be called from inside the thread because some platforms
				// can change the name only when run from the specific thread.
				ctx.tp->set_thread_name(ctx.workerIdx, ctx.prio);

				// Set the worker thread priority
				ctx.tp->set_thread_priority(ctx.workerIdx, ctx.prio);

				// Process jobs
				ctx.tp->worker_loop(ctx.prio);

#if !GAIA_PLATFORM_WINDOWS
				// Other platforms allocate the context dynamically
				delete &ctx;
#endif
				return nullptr;
			}

			//! Creates a thread
			//! \param workerIdx Worker index
			//! \param prio Priority used for the thread
			void create_thread(uint32_t workerIdx, JobPriority prio) {
				if GAIA_UNLIKELY (workerIdx >= m_workers.size())
					return;

#if GAIA_PLATFORM_WINDOWS
				ThreadFuncCtx ctx{this, workerIdx, prio};
				m_workers[workerIdx] = std::thread([ctx]() {
					thread_func((void*)&ctx);
				});
#else
				int ret;

				pthread_attr_t attr{};
				ret = pthread_attr_init(&attr);
				if (ret != 0) {
					GAIA_LOG_W("pthread_attr_init failed for worker thread %u. ErrCode = %d", workerIdx, ret);
					return;
				}

				///////////////////////////////////////////////////////////////////////////
				// Apple's recomendation for Apple Silicon for games / high-perf software
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
				auto* ctx = new ThreadFuncCtx{this, workerIdx, prio};
				ret = pthread_create(&m_workers[workerIdx], &attr, thread_func, ctx);
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
				if GAIA_UNLIKELY (workerIdx >= m_workers.size())
					return;

#if GAIA_PLATFORM_WINDOWS
				auto& t = m_workers[workerIdx];
				if (t.joinable())
					t.join();
#else
				auto& t = m_workers[workerIdx];
				pthread_join(t, nullptr);
#endif
			}

			void set_workers_high_prio_inter(uint32_t from, uint32_t count) {
				const uint32_t to = from + count;
				for (uint32_t i = from; i < to; ++i)
					create_thread(i, JobPriority::High);
			}

			void set_workers_low_prio_inter(uint32_t from, uint32_t count) {
				const uint32_t to = from + count;
				for (uint32_t i = from; i < to; ++i)
					create_thread(i, JobPriority::Low);
			}

			void set_thread_priority([[maybe_unused]] uint32_t workerIdx, [[maybe_unused]] JobPriority priority) {
#if GAIA_PLATFORM_WINDOWS
				HANDLE nativeHandle = (HANDLE)m_workers[workerIdx].native_handle();

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
				// 				HANDLE nativeHandle = (HANDLE)m_workers[workerIdx].native_handle();
				//
				// 				auto mask = SetThreadAffinityMask(nativeHandle, 1ULL << workerIdx);
				// 				if (mask <= 0)
				// 					GAIA_LOG_W("Issue setting thread affinity for worker thread %u!", workerIdx);
				// #elif GAIA_PLATFORM_APPLE
				// 				// Do not do affinity for MacOS. If is not supported for Apple Silion and
				// 				// Intel MACs are deprecated anyway.
				// 				// TODO: Consider supporting this at least for Intel MAC as there are still
				// 				//       quite of few of them out there.
				// #elif GAIA_PLATFORM_LINUX || GAIA_PLATFORM_FREEBSD
				// 				pthread_t nativeHandle = (pthread_t)m_workers[workerIdx].native_handle();
				//
				// 				cpu_set_t cpuset;
				// 				CPU_ZERO(&cpuset);
				// 				CPU_SET(workerIdx, &cpuset);
				//
				// 				auto ret = pthread_setaffinity_np(nativeHandle, sizeof(cpuset), &cpuset);
				// 				if (ret != 0)
				// 					GAIA_LOG_W("Issue setting thread affinity for worker thread %u!", workerIdx);
				//
				// 				ret = pthread_getaffinity_np(nativeHandle, sizeof(cpuset), &cpuset);
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
				auto nativeHandle = (HANDLE)m_workers[workerIdx].native_handle();

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
				auto nativeHandle = m_workers[workerIdx];

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
			bool main_thread_tick(JobPriority prio) {
				auto& jobQueue = m_jobQueue[(uint32_t)prio];

				JobHandle jobHandle;

				if (!jobQueue.try_pop(jobHandle))
					return false;

				GAIA_ASSERT(busy(prio));

				// Make sure we can execute the job.
				// If it has dependencies which were not completed we need
				// to reschedule and come back to it later.
				if (!m_jobManager.handle_deps(jobHandle)) {
					resubmit(jobHandle);
					return true;
				}

				m_jobManager.run(jobHandle);
				--m_jobsPending[(uint32_t)prio];
				return true;
			}

			//! Loop run by worker threads. Pops jobs from the given queue
			//! and executes it.
			//! \param prio Target worker queue defined by job priority
			void worker_loop(JobPriority prio) {
				auto& jobQueue = m_jobQueue[(uint32_t)prio];
				auto& cv = m_cv[prio];
				auto& cvLock = prio == 0 ? m_cvLock0 : m_cvLock1;

				while (!m_stop) {
					JobHandle jobHandle;

					if (!jobQueue.try_pop(jobHandle)) {
						auto& mtx = GAIA_PROF_EXTRACT_MUTEX(std::mutex, cvLock);
						std::unique_lock lock(mtx);
						cv.wait(lock);
						continue;
					}

					GAIA_ASSERT(busy(prio));

					// Make sure we can execute the job.
					// If it has dependencies which were not completed we need
					// to reschedule and come back to it later.
					if (!m_jobManager.handle_deps(jobHandle)) {
						resubmit(jobHandle);
						continue;
					}

					m_jobManager.run(jobHandle);
					--m_jobsPending[(uint32_t)prio];
				}
			}

			//! Finishes all jobs and stops all worker threads
			void reset() {
				// Request stopping
				m_stop = true;

				// complete all remaining work
				wait_all();

				// Wake up any threads that were put to sleep
				m_cv[0].notify_all();
				m_cv[1].notify_all();

				// Join threads with the main one
				GAIA_FOR(m_workers.size()) join_thread(i);

				// All threads have been stopped. Allow new threads to run if necessary.
				m_stop = false;
			}

			//! Checks whether workers are busy doing work.
			//!	\return True if any workers are busy doing work.
			GAIA_NODISCARD bool busy(JobPriority prio) const {
				return m_jobsPending[(uint32_t)prio] > 0;
			}

			//! Wakes up some worker thread and reschedules the current one.
			void poll(JobPriority prio) {
				// Wake some worker thread
				m_cv[(uint32_t)prio].notify_one();

				// Allow this thread to be rescheduled
				std::this_thread::yield();
			}

			//! Wakes up all worker threads and reschedules the current one.
			void busy_poll_all() {
				bool b0 = busy(JobPriority::High);
				bool b1 = busy(JobPriority::Low);

				while (b1 || b0) {
					// Wake some priority worker thread
					if (b0)
						m_cv[0].notify_all();
					// Wake some background worker thread
					if (b1)
						m_cv[1].notify_all();

					// Allow this thread to be rescheduled
					std::this_thread::yield();

					// Check the status again
					b0 = busy(JobPriority::High);
					b1 = busy(JobPriority::Low);
				}
			}

			//! Makes sure the priority is right for the given set of allocated workers
			template <typename TJob>
			JobPriority final_prio(const TJob& job) {
				const auto cnt = m_workerCnt[job.priority];
				return cnt > 0
									 // If there is enough workers, keep the priority
									 ? job.priority
									 // Not enough workers, use the other priority that has workers
									 : (JobPriority)((job.priority + 1U) % (uint32_t)JobPriorityCnt);
			}
		};
	} // namespace mt
} // namespace gaia