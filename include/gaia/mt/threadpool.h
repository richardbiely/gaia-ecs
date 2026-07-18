#pragma once

#include "gaia/config/config.h"
#include "gaia/config/profiler.h"

#if GAIA_PLATFORM_WINDOWS
	#include <cstdio>
	#include <windows.h>
#endif

#define GAIA_THREAD_OFF 0
#define GAIA_THREAD_STD 1
#define GAIA_THREAD_PTHREAD 2

#if GAIA_PLATFORM_WINDOWS || GAIA_PLATFORM_WASM
	#include <thread>
	// Emscripten supports std::thread if compiled with -sUSE_PTHREADS=1.
	// Otherwise, std::thread calls are no-ops that compile but do not run concurrently.
	#define GAIA_THREAD std::thread
	#define GAIA_THREAD_PLATFORM GAIA_THREAD_STD
#elif GAIA_PLATFORM_APPLE
	#include <pthread.h>
	#include <pthread/sched.h>
	#include <sys/qos.h>
	#include <sys/sysctl.h>
	#define GAIA_THREAD pthread_t
	#define GAIA_THREAD_PLATFORM GAIA_THREAD_PTHREAD
#elif GAIA_PLATFORM_LINUX
	#include <dirent.h>
	#include <fcntl.h>
	#include <pthread.h>
	#include <unistd.h>
	#define GAIA_THREAD pthread_t
	#define GAIA_THREAD_PLATFORM GAIA_THREAD_PTHREAD
#elif GAIA_PLATFORM_FREEBSD
	#include <pthread.h>
	#include <sys/sysctl.h>
	#define GAIA_THREAD pthread_t
	#define GAIA_THREAD_PLATFORM GAIA_THREAD_PTHREAD
#endif

#if GAIA_PLATFORM_WINDOWS
	#include <malloc.h>
#else
	#include <alloca.h>
#endif
#include <atomic>
#include <thread>

#include "gaia/cnt/sarray_ext.h"
#include "gaia/core/span.h"
#include "gaia/core/utility.h"
#include "gaia/util/logging.h"

#include "gaia/mt/event.h"
#include "gaia/mt/futex.h"
#include "gaia/mt/jobcommon.h"
#include "gaia/mt/jobhandle.h"
#include "gaia/mt/jobmanager.h"
#include "gaia/mt/jobqueue.h"
#include "gaia/mt/semaphore_fast.h"
#include "gaia/mt/spinlock.h"

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

		GAIA_MSVC_WARNING_PUSH()
		GAIA_MSVC_WARNING_DISABLE(4324)

		//! Process-wide worker pool for dependent frame and background jobs.
		class GAIA_API ThreadPool final {
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
			GAIA_ALIGNAS(128) cnt::sarray_ext<ThreadCtx, MaxWorkers> m_workersCtx;
			//! Global job queue
			MpmcQueue<JobHandle, 1024> m_jobQueue[JobPriorityCnt];
			//! Global queue for background jobs that may span multiple frames.
			MpmcQueue<JobHandle, 1024> m_jobQueueBackground;
			//! The number of spawned frame worker threads.
			uint32_t m_frameWorkersCnt = 0;
			//! The number of spawned background worker threads.
			uint32_t m_backgroundWorkersCnt = 0;
			//! The number of workers dedicated for a given level of job priority
			uint32_t m_workersCnt[JobPriorityCnt]{};
			//! The number of spawned worker threads dedicated for a given level of job priority.
			//! Unlike m_workersCnt this excludes the main thread.
			//! Due to some nuances, it makes things easier to keep this a separate variable
			//! along m_workerCnt.
			uint32_t m_workerThreadsCnt[JobPriorityCnt]{};
			//! Semaphores controlling if the worker threads are allowed to run
			SemaphoreFast m_sem[JobPriorityCnt];
			//! Semaphore controlling if background worker threads are allowed to run
			SemaphoreFast m_semBackground;

			//! Futex counter
			std::atomic_uint32_t m_blockedInWorkUntil;

			//! Manager for internal jobs
			JobManager m_jobManager;
			//! Job allocation mutex
			//! \note Job creation is expected from the main thread. Freeing can happen from any thread,
			//! so allocation metadata remains locked.
			//! \note Job storage uses a fixed page table for the full handle range, so adding a job while
			//! unrelated background jobs are running does not move existing job containers.
			GAIA_PROF_MUTEX(SpinLock, m_jobAllocMtx);

		private:
			ThreadPool() {
				m_stop.store(false);

				make_main_thread();

				const auto hwThreads = hw_thread_cnt();
				const auto hwEffThreads = hw_efficiency_cores_cnt();
				uint32_t hiPrioWorkers = hwThreads;
				if (hwEffThreads < hwThreads)
					hiPrioWorkers -= hwEffThreads;

				set_max_workers(hwThreads, hiPrioWorkers);
			}

			ThreadPool(ThreadPool&&) = delete;
			ThreadPool(const ThreadPool&) = delete;
			ThreadPool& operator=(ThreadPool&&) = delete;
			ThreadPool& operator=(const ThreadPool&) = delete;

		public:
			//! Returns the process-wide thread-pool instance.
			//! \return Singleton thread pool.
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

			//! Returns the number of frame worker threads
			//! \return Spawned frame worker count, excluding the main thread.
			GAIA_NODISCARD uint32_t workers() const {
				return m_frameWorkersCnt;
			}

			//! Returns the number of background worker threads
			//! \return Spawned background worker count.
			GAIA_NODISCARD uint32_t background_workers() const {
				return m_backgroundWorkersCnt;
			}

			//! Set the maximum number of frame execution contexts for this system.
			//! \param count Requested frame execution contexts, including the main thread.
			//!              The number of spawned frame worker threads is one less.
			//! \param countHighPrio Number of high-priority frame execution contexts.
			//!                      Values larger than \a count are clamped.
			//! \warning All jobs are finished first before threads are recreated.
			void set_max_workers(uint32_t count, uint32_t countHighPrio) {
				const auto maxFrameWorkers = MaxWorkers - m_backgroundWorkersCnt;
				const auto workersCnt = core::get_max(core::get_min(maxFrameWorkers, count), 1U);
				countHighPrio = core::get_min(countHighPrio, workersCnt);

				// Stop all threads first
				reset();

				// Reset previous worker contexts
				for (auto& ctx: m_workersCtx)
					ctx.reset();

				m_frameWorkersCnt = workersCnt - 1;

				// The main thread uses context 0. Frame workers follow, and
				// background workers are appended after them.
				m_workersCtx.resize(workersCnt + m_backgroundWorkersCnt);
				// We also have the main thread so there's always one less worker spawned
				m_workers.resize(m_frameWorkersCnt + m_backgroundWorkersCnt);

				// First worker is considered the main thread.
				// It is also assigned high priority but it doesn't really matter.
				// The main thread can steal any jobs, both low and high priority.
				detail::tl_workerCtx = m_workersCtx.data();
				m_workersCtx[0].tp = this;
				m_workersCtx[0].workerIdx = 0;
				m_workersCtx[0].prio = JobPriority::High;

				// Reset the workers
				for (auto& worker: m_workers)
					worker = {};

				// Create a new set of high and low priority threads (if any)
				uint32_t workerIdx = 1;
				set_workers_high_prio_inter(workerIdx, countHighPrio);
				create_background_worker_threads(workerIdx);
			}

			//! Updates the number of worker threads participating at high priority workloads.
			//! \param[out] workerIdx Number of high priority workers.
			//! \param count Requested number of high priority workers.
			//! \warning All jobs are finished first before threads are recreated.
			void set_workers_high_prio_inter(uint32_t& workerIdx, uint32_t count) {
				count = gaia::core::get_min(count, m_frameWorkersCnt);
				m_workerThreadsCnt[0] = count;
				m_workerThreadsCnt[1] = m_frameWorkersCnt - count;
				m_workersCnt[0] = count + 1; // Main thread is always a priority worker
				m_workersCnt[1] = m_workerThreadsCnt[1];

				// Create a new set of high and low priority threads (if any)
				create_worker_threads(workerIdx, JobPriority::High, m_workerThreadsCnt[0]);
				create_worker_threads(workerIdx, JobPriority::Low, m_workerThreadsCnt[1]);
			}

			//! Updates the number of worker threads participating at low priority workloads.
			//! \param[out] workerIdx Number of low priority workers.
			//! \param count Requested number of low priority workers.
			//! \warning All jobs are finished first before threads are recreated.
			void set_workers_low_prio_inter(uint32_t& workerIdx, uint32_t count) {
				const uint32_t realCnt = gaia::core::get_min(count, m_frameWorkersCnt);
				m_workerThreadsCnt[0] = m_frameWorkersCnt - realCnt;
				m_workerThreadsCnt[1] = realCnt;
				m_workersCnt[0] = m_workerThreadsCnt[0] + 1; // Main thread is always a priority worker
				m_workersCnt[1] = m_workerThreadsCnt[1];

				// Create a new set of high and low priority threads (if any)
				create_worker_threads(workerIdx, JobPriority::High, m_workerThreadsCnt[0]);
				create_worker_threads(workerIdx, JobPriority::Low, m_workerThreadsCnt[1]);
			}

			//! Updates the number of worker threads participating at high priority workloads
			//! \warning All jobs are finished first before threads are recreated.
			void set_workers_high_prio(uint32_t count) {
				// Stop all threads first
				reset();
				detail::tl_workerCtx = m_workersCtx.data();

				uint32_t workerIdx = 1;
				set_workers_high_prio_inter(workerIdx, count);
				create_background_worker_threads(workerIdx);
			}

			//! Updates the number of worker threads participating at low priority workloads
			//! \warning All jobs are finished first before threads are recreated.
			void set_workers_low_prio(uint32_t count) {
				// Stop all threads first
				reset();
				detail::tl_workerCtx = m_workersCtx.data();

				uint32_t workerIdx = 1;
				set_workers_low_prio_inter(workerIdx, count);
				create_background_worker_threads(workerIdx);
			}

			//! Updates the number of worker threads dedicated to background jobs.
			//! Background workers run jobs submitted through sched_background() and
			//! are not used by update(). They share the MaxWorkers limit with frame workers.
			//! \param count Requested number of background worker threads.
			//! \warning All jobs are finished first before threads are recreated.
			void set_background_workers(uint32_t count) {
				const auto maxBackgroundWorkers = MaxWorkers - 1;
				count = core::get_min(maxBackgroundWorkers, count);

				const auto frameWorkersCntOld = m_frameWorkersCnt;
				const auto highWorkersCntOld = m_workerThreadsCnt[0];

				// Stop all threads first
				reset();

				m_backgroundWorkersCnt = count;

				const auto maxFrameWorkers = MaxWorkers - m_backgroundWorkersCnt - 1;
				m_frameWorkersCnt = core::get_min(frameWorkersCntOld, maxFrameWorkers);

				for (auto& ctx: m_workersCtx)
					ctx.reset();

				m_workersCtx.resize(m_frameWorkersCnt + 1 + m_backgroundWorkersCnt);
				m_workers.resize(m_frameWorkersCnt + m_backgroundWorkersCnt);

				detail::tl_workerCtx = m_workersCtx.data();
				m_workersCtx[0].tp = this;
				m_workersCtx[0].workerIdx = 0;
				m_workersCtx[0].prio = JobPriority::High;

				for (auto& worker: m_workers)
					worker = {};

				uint32_t workerIdx = 1;
				set_workers_high_prio_inter(workerIdx, highWorkersCntOld);
				create_background_worker_threads(workerIdx);
			}

			//! Makes \a jobSecond depend on \a jobFirst.
			//! This means \a jobSecond will not run until \a jobFirst finishes.
			//! \param jobFirst The job that must complete first.
			//! \param jobSecond The job that will run after \a jobFirst.
			//! \warning This must be called before any of the listed jobs are scheduled.
			void dep(JobHandle jobFirst, JobHandle jobSecond) {
				GAIA_ASSERT(main_thread());

				m_jobManager.dep(std::span(&jobFirst, 1), jobSecond);
			}

			//! Makes \a jobSecond depend on the jobs listed in \a jobsFirst.
			//! This means \a jobSecond will not run until all jobs from \a jobsFirst finish.
			//! \param jobsFirst Jobs that must complete first.
			//! \param jobSecond The job that will run after \a jobsFirst.
			//! \warning This must must to be called before any of the listed jobs are scheduled.
			void dep(std::span<JobHandle> jobsFirst, JobHandle jobSecond) {
				GAIA_ASSERT(main_thread());

				m_jobManager.dep(jobsFirst, jobSecond);
			}

			//! Makes \a jobSecond depend on \a jobFirst.
			//! This means \a jobSecond will not run until \a jobFirst finishes.
			//! \param jobFirst The job that must complete first.
			//! \param jobSecond The job that will run after \a jobFirst.
			//! \note Unlike dep() this function needs to be called when job handles are reused.
			//! \warning This must be called before any of the listed jobs are scheduled.
			//! \warning This must be called from the main thread.
			void dep_refresh(JobHandle jobFirst, JobHandle jobSecond) {
				GAIA_ASSERT(main_thread());

				m_jobManager.dep_refresh(std::span(&jobFirst, 1), jobSecond);
			}

			//! Makes \a jobSecond depend on the jobs listed in \a jobsFirst.
			//! This means \a jobSecond will not run until all jobs from \a jobsFirst finish.
			//! \param jobsFirst Jobs that must complete first.
			//! \param jobSecond The job that will run after \a jobsFirst.
			//! \note Unlike dep() this function needs to be called when job handles are reused.
			//! \warning This must be called before any of the listed jobs are scheduled.
			//! \warning This must be called from the main thread.
			void dep_refresh(std::span<JobHandle> jobsFirst, JobHandle jobSecond) {
				GAIA_ASSERT(main_thread());

				m_jobManager.dep_refresh(jobsFirst, jobSecond);
			}

			//! Creates a threadpool job from \a job.
			//! \tparam TJob Job descriptor type convertible to Job.
			//! \param job Job descriptor to allocate.
			//! \warning Must be used from the main thread.
			//! \warning Frame jobs should be created before frame work is submitted.
			//!          It is valid to create new frame jobs while unrelated background jobs are running.
			//! \return Job handle of the scheduled job.
			template <typename TJob>
			JobHandle add(TJob job) {
				GAIA_ASSERT(main_thread());

				job.priority = final_prio(job);

				auto& mtx = GAIA_PROF_EXTRACT_MUTEX(m_jobAllocMtx);
				core::lock_scope lock(mtx);
				GAIA_PROF_LOCK_MARK(m_jobAllocMtx);

				return m_jobManager.alloc_job(GAIA_MOV(job));
			}

		private:
			void add_n(JobPriority prio, std::span<JobHandle> jobHandles) {
				GAIA_ASSERT(main_thread());
				GAIA_ASSERT(!jobHandles.empty());

				auto& mtx = GAIA_PROF_EXTRACT_MUTEX(m_jobAllocMtx);
				core::lock_scope lock(mtx);
				GAIA_PROF_LOCK_MARK(m_jobAllocMtx);

				for (auto& jobHandle: jobHandles)
					jobHandle = m_jobManager.alloc_job({{}, prio, JobCreationFlags::Default});
			}

			GAIA_NODISCARD ParallelCallbackHandle add_parallel_callback(JobArgsFunc callback, uint32_t refs) {
				auto& mtx = GAIA_PROF_EXTRACT_MUTEX(m_jobAllocMtx);
				core::lock_scope lock(mtx);
				GAIA_PROF_LOCK_MARK(m_jobAllocMtx);

				return m_jobManager.alloc_parallel_callback(GAIA_MOV(callback), refs);
			}

			void release_parallel_callback(ParallelCallbackHandle handle) {
				if (!m_jobManager.release_parallel_callback_ref(handle))
					return;

				auto& mtx = GAIA_PROF_EXTRACT_MUTEX(m_jobAllocMtx);
				core::lock_scope lock(mtx);
				GAIA_PROF_LOCK_MARK(m_jobAllocMtx);

				m_jobManager.free_parallel_callback(handle);
			}

		public:
			//! Deletes a job handle \a jobHandle from the threadpool.
			//! \param jobHandle Completed or clear job to delete.
			//! \warning Job handle must not be used by any worker thread and can not be used
			//!          by any active job handles as a dependency.
			void del([[maybe_unused]] JobHandle jobHandle) {
				GAIA_ASSERT(jobHandle != (JobHandle)JobNull_t{});

#if GAIA_ASSERT_ENABLED
				{
					const auto& jobData = m_jobManager.data(jobHandle);
					GAIA_ASSERT(jobData.state == 0 || m_jobManager.done(jobData));
				}
#endif

				auto& mtx = GAIA_PROF_EXTRACT_MUTEX(m_jobAllocMtx);
				core::lock_scope lock(mtx);
				GAIA_PROF_LOCK_MARK(m_jobAllocMtx);

				m_jobManager.free_job(jobHandle);
			}

			//! Pushes \a jobHandles into the internal queue so worker threads
			//! can pick them up and execute them.
			//! If there are more jobs than the queue can handle it puts the calling
			//! thread to sleep until workers consume enough jobs.
			//! \warning Once submitted, dependencies can't be modified for this job.
			//! \param jobHandles Jobs to submit.
			void submit(std::span<JobHandle> jobHandles) {
				if (jobHandles.empty())
					return;

				GAIA_PROF_SCOPE(tp::submit);

				auto* pHandles = (JobHandle*)alloca(sizeof(JobHandle) * jobHandles.size());

				uint32_t cnt = 0;
				for (auto handle: jobHandles) {
					GAIA_ASSERT(handle != (JobHandle)JobNull_t{});

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

			//! Pushes \a jobHandle into the internal queue so worker threads
			//! can pick it up and execute it.
			//! If there are more jobs than the queue can handle it puts the calling
			//! thread to sleep until workers consume enough jobs.
			//! \warning Once submitted, dependencies can't be modified for this job.
			//! \param jobHandle Job to submit.
			void submit(JobHandle jobHandle) {
				submit(std::span(&jobHandle, 1));
			}

			//! Resets completed jobs to the clear reusable state without waiting.
			//! \param jobHandles Completed jobs to reset.
			void reset_state(std::span<JobHandle> jobHandles) {
				if (jobHandles.empty())
					return;

				GAIA_PROF_SCOPE(tp::reset);

				for (auto handle: jobHandles) {
					auto& jobData = m_jobManager.data(handle);
					m_jobManager.reset_state(jobData);
				}
			}

			//! Resets a completed job to the clear reusable state without waiting.
			//! \param jobHandle Completed job to reset.
			void reset_state(JobHandle jobHandle) {
				reset_state(std::span(&jobHandle, 1));
			}

			//! Waits for \a jobHandles to finish and resets them to a reusable state.
			//! \param jobHandles Jobs to wait for and reset.
			//! \warning Handles that were auto-deleted (non-manual jobs) are skipped.
			void reset(std::span<JobHandle> jobHandles) {
				if (jobHandles.empty())
					return;

				GAIA_ASSERT(main_thread());
				GAIA_PROF_SCOPE(tp::reset_wait);

				// Wait first to avoid resetting one handle while another one still depends on it.
				for (auto handle: jobHandles) {
					if (handle == (JobHandle)JobNull_t{})
						continue;
					wait(handle);
				}

				for (auto handle: jobHandles) {
					if (handle == (JobHandle)JobNull_t{})
						continue;

					auto& jobData = m_jobManager.data(handle);
					const auto state = jobData.state.load() & JobState::STATE_BITS_MASK;
					// Auto-deleted jobs are released and cannot be reused through reset_state().
					if (state == JobState::Released)
						continue;

					m_jobManager.reset_state(jobData);
				}
			}

			//! Waits for \a jobHandle to finish and resets it to a reusable state.
			//! \param jobHandle Job to wait for and reset.
			void reset(JobHandle jobHandle) {
				reset(std::span(&jobHandle, 1));
			}

			//! Schedules a job to run on a worker thread.
			//! \param job Job descriptor
			//! \warning Must be used from the main thread.
			//! \warning Dependencies can't be modified for this job.
			//! \return Job handle of the scheduled job.
			JobHandle sched(Job job) {
				JobHandle jobHandle = add(GAIA_MOV(job));
				submit(jobHandle);
				return jobHandle;
			}

			//! Schedules a job to run on background workers.
			//! Background jobs are not drained by update() and may span multiple frames.
			//! If no background workers are configured, wait() is the explicit fallback
			//! path that can execute queued background work on the calling thread.
			//! \param job Job descriptor.
			//! \warning Must be used from the main thread.
			//! \warning Dependencies can't be modified for this job.
			//! \return Job handle of the scheduled background job.
			JobHandle sched_background(Job job) {
				job.flags = (JobCreationFlags)((uint8_t)job.flags | (uint8_t)JobCreationFlags::Background);
				JobHandle jobHandle = add(GAIA_MOV(job));
				submit(jobHandle);
				return jobHandle;
			}

			//! Schedules a job to run on a worker thread.
			//! \param job Job descriptor
			//! \param dependsOn Job we depend on
			//! \warning Must be used from the main thread.
			//! \warning Dependencies can't be modified for this job.
			//! \return Job handle of the scheduled job.
			JobHandle sched(Job job, JobHandle dependsOn) {
				JobHandle jobHandle = add(GAIA_MOV(job));
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
			JobHandle sched_par(JobParallel job, uint32_t itemsToProcess, uint32_t groupSize) {
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

				// Only one job is created, use the job directly.
				// Generally, this is the case we would want to avoid because it means this particular case
				// is not worth of being scheduled via sched_par. However, we can never know for sure what
				// the reason for that is so let's stay silent.
				if (jobs == 1) {
					const uint32_t groupJobIdxEnd = groupSize < itemsToProcess ? groupSize : itemsToProcess;
					auto groupFunc = GAIA_MOV(job.func);
					auto groupJobFunc = [func = GAIA_MOV(groupFunc), groupJobIdxEnd]() mutable {
						JobArgs args;
						args.idxStart = 0;
						args.idxEnd = groupJobIdxEnd;
						func(args);
					};

					auto handle = add(Job{GAIA_MOV(groupJobFunc), prio, JobCreationFlags::Default});
					submit(handle);
					return handle;
				}

				// Multiple jobs need to be parallelized.
				// Create a sync job and assign it as their dependency.
				auto callbackHandle = add_parallel_callback(GAIA_MOV(job.func), jobs);

				auto* pHandles = (JobHandle*)alloca(sizeof(JobHandle) * (jobs + 1));
				std::span<JobHandle> handles(pHandles, jobs + 1);

				add_n(prio, handles);

#if GAIA_ASSERT_ENABLED
				for (auto jobHandle: handles)
					GAIA_ASSERT(m_jobManager.is_clear(jobHandle));
#endif

				// Work jobs
				for (uint32_t jobIndex = 0; jobIndex < jobs; ++jobIndex) {
					const uint32_t groupJobIdxStart = jobIndex * groupSize;
					const uint32_t groupJobIdxStartPlusGroupSize = groupJobIdxStart + groupSize;
					const uint32_t groupJobIdxEnd =
							groupJobIdxStartPlusGroupSize < itemsToProcess ? groupJobIdxStartPlusGroupSize : itemsToProcess;

					auto groupJobFunc = [this, callbackHandle, groupJobIdxStart, groupJobIdxEnd]() {
						JobArgs args;
						args.idxStart = groupJobIdxStart;
						args.idxEnd = groupJobIdxEnd;
						m_jobManager.invoke_parallel_callback(callbackHandle, args);
						release_parallel_callback(callbackHandle);
					};

					auto& jobData = m_jobManager.data(pHandles[jobIndex]);
					jobData.func = util::SmallFunc::create(GAIA_MOV(groupJobFunc));
					jobData.prio = prio;
				}
				// Sync job
				{
					auto& jobData = m_jobManager.data(pHandles[jobs]);
					jobData.prio = prio;
				}

				// Assign the sync jobs as a dependency for work jobs
				dep(handles.subspan(0, jobs), pHandles[jobs]);

				// Sumbit the jobs to the threadpool.
				// This is a point of no return. After this point no more changes to jobs are possible.
				submit(handles);
				return pHandles[jobs];
			}

			//! Schedules a non-owning parallel job descriptor on worker threads.
			//! \param job Non-owning job descriptor.
			//! \param itemsToProcess Total number of work items.
			//! \param groupSize Group size per created job. If zero the threadpool decides the group size.
			//! \warning Must be used from the main thread.
			//! \warning The pointed-to context must remain alive until the returned handle completes.
			//! \return Job handle of the scheduled batch of jobs.
			JobHandle sched_par(JobParallelRef job, uint32_t itemsToProcess, uint32_t groupSize) {
				GAIA_ASSERT(main_thread());
				GAIA_ASSERT(job.pCtx != nullptr);
				GAIA_ASSERT(job.invoke != nullptr);

				GAIA_ASSERT(itemsToProcess != 0);
				if (itemsToProcess == 0)
					return JobNull;

				if GAIA_UNLIKELY (m_stop)
					return JobNull;

				const auto prio = job.priority = final_prio(job);

				if (groupSize == 0) {
					const auto cntWorkers = m_workersCnt[(uint32_t)prio];
					groupSize = (itemsToProcess + cntWorkers - 1) / cntWorkers;

					constexpr uint32_t maxUnitsOfWorkPerGroup = 8;
					groupSize = groupSize / maxUnitsOfWorkPerGroup;
					if (groupSize <= 0)
						groupSize = 1;
				}

				const auto jobs = (itemsToProcess + groupSize - 1) / groupSize;

				if (jobs == 1) {
					const uint32_t groupJobIdxEnd = groupSize < itemsToProcess ? groupSize : itemsToProcess;
					auto* pCtx = job.pCtx;
					auto invoke = job.invoke;
					auto groupJobFunc = [pCtx, invoke, groupJobIdxEnd]() {
						JobArgs args;
						args.idxStart = 0;
						args.idxEnd = groupJobIdxEnd;
						invoke(pCtx, args);
					};

					auto handle = add(Job{GAIA_MOV(groupJobFunc), prio, JobCreationFlags::Default});
					submit(handle);
					return handle;
				}

				auto* pHandles = (JobHandle*)alloca(sizeof(JobHandle) * (jobs + 1));
				std::span<JobHandle> handles(pHandles, jobs + 1);

				add_n(prio, handles);

#if GAIA_ASSERT_ENABLED
				for (auto jobHandle: handles)
					GAIA_ASSERT(m_jobManager.is_clear(jobHandle));
#endif

				for (uint32_t jobIndex = 0; jobIndex < jobs; ++jobIndex) {
					const uint32_t groupJobIdxStart = jobIndex * groupSize;
					const uint32_t groupJobIdxStartPlusGroupSize = groupJobIdxStart + groupSize;
					const uint32_t groupJobIdxEnd =
							groupJobIdxStartPlusGroupSize < itemsToProcess ? groupJobIdxStartPlusGroupSize : itemsToProcess;

					auto* pCtx = job.pCtx;
					auto invoke = job.invoke;
					auto groupJobFunc = [pCtx, invoke, groupJobIdxStart, groupJobIdxEnd]() {
						JobArgs args;
						args.idxStart = groupJobIdxStart;
						args.idxEnd = groupJobIdxEnd;
						invoke(pCtx, args);
					};

					auto& jobData = m_jobManager.data(pHandles[jobIndex]);
					jobData.func = util::SmallFunc::create(GAIA_MOV(groupJobFunc));
					jobData.prio = prio;
				}
				{
					auto& jobData = m_jobManager.data(pHandles[jobs]);
					jobData.prio = prio;
				}

				dep(handles.subspan(0, jobs), pHandles[jobs]);
				submit(handles);
				return pHandles[jobs];
			}

			//! Wait until a job associated with the jobHandle finishes executing.
			//! Cleans up any job allocations and dependencies associated with \a jobHandle.
			//! The calling thread participates in frame job processing until \a jobHandle is done.
			//! For background jobs, the calling thread only runs background work when no
			//! background workers are configured.
			//! \param jobHandle Job handle to wait for
			void wait(JobHandle jobHandle) {
				GAIA_PROF_SCOPE(tp::wait);

				GAIA_ASSERT(main_thread());

				// Skip waitinig for unset job handles.
				if (jobHandle == (JobHandle)JobNull_t{})
					return;

				auto* ctx = detail::tl_workerCtx;
				auto& jobData = m_jobManager.data(jobHandle);
				const bool waitBackground = is_background(jobData);
				auto state = jobData.state.load();

				// Waiting for a job that has not been initialized is nonsense.
				GAIA_ASSERT(state != 0);

				// Wait until done
				for (; (state & JobState::STATE_BITS_MASK) < JobState::Done; state = jobData.state.load()) {
					// The job we are waiting for is not finished yet, try running some other job in the meantime
					JobHandle otherJobHandle;
					const bool canHelpBackground = waitBackground && m_backgroundWorkersCnt == 0;
					const bool hasBackgroundJob = canHelpBackground && try_fetch_background_job(otherJobHandle);
					const bool hasJob = hasBackgroundJob || try_fetch_job(*ctx, otherJobHandle);
					if (hasJob) {
						if (run(otherJobHandle, ctx))
							continue;
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
					const auto newState = jobData.state.load();
					if (newState == state) // still not JobState::Done?
						Futex::wait(&m_blockedInWorkUntil, oldBlockedMask | workerBit, detail::WaitMaskAny);
					m_blockedInWorkUntil.fetch_and(~workerBit);
				}
			}

			//! Uses the main thread to help with frame job processing.
			//! Background jobs are intentionally excluded from update().
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

			//! Returns the number of efficiency cores of the system.
			//! \return The number of efficiency cores. 0 if failed, or if there are no such cores.
			GAIA_NODISCARD static uint32_t hw_efficiency_cores_cnt() {
				uint32_t efficiencyCores = 0;
#if GAIA_PLATFORM_APPLE
				size_t size = sizeof(efficiencyCores);
				if (sysctlbyname("hw.perflevel1.logicalcpu", &efficiencyCores, &size, nullptr, 0) != 0)
					return 0;
#elif GAIA_PLATFORM_FREEBSD
				int cpuIndex = 0;
				char oidName[32];
				int coreType;
				size_t size = sizeof(coreType);
				while (true) {
					GAIA_STRFMT(oidName, sizeof(oidName), "dev.cpu.%d.coretype", cpuIndex);
					if (sysctlbyname(oidName, &coreType, &size, nullptr, 0) != 0)
						break; // Stop on the last CPU index

					// 0 = performance core
					// 1 = efficiency core
					if (coreType == 1)
						++efficiencyCores;

					++cpuIndex;
				}
#elif GAIA_PLATFORM_WINDOWS
				DWORD length = 0;

				// First, determine required buffer size
				if (!GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &length))
					return 0;

				// Allocate enough memory
				auto* pBuffer = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*)malloc(length);
				if (pBuffer == nullptr)
					return 0;

				// Retrieve the data
				if (!GetLogicalProcessorInformationEx(RelationProcessorCore, pBuffer, &length)) {
					free(pBuffer);
					return 0;
				}

				uint32_t heterogenousCnt = 0;

				// Iterate over processor core entries.
				// On Windows we can't directly tell  whether a core is an efficiency core or a performance core.
				// Instead:
				// - lower EfficiencyClass values correspond to more efficient cores
				// - higher EfficiencyClass values correspond to higher performance cores
				// - EfficiencyClass is zero for homogeneous CPU architectures
				// Therefore, to count efficiency cores on Windows, we will count cores where EfficiencyClass == 0.
				// On heterogeneous this should gives us the correct results.
				// On homogenous architectures, the value is always 0 so rather than calculating the number of efficiency
				// cores, we would calculate the number of performance cores. For the sake of correctness, if all cores return
				// 0, we use 0 for the number of efficiency cores.
				for (char* ptr = (char*)pBuffer; ptr < (char*)pBuffer + length;
						 ptr += ((SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*)ptr)->Size) {
					auto* entry = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*)ptr;
					if (entry->Relationship == RelationProcessorCore) {
						if (entry->Processor.EfficiencyClass == 0)
							++efficiencyCores;
						else
							++heterogenousCnt;
					}
				}

				if (heterogenousCnt == 0)
					efficiencyCores = 0;

				free(pBuffer);
#elif GAIA_PLATFORM_LINUX
				{
					// Intel has /sys/devices/cpu_core/cpus, /sys/devices/cpu_atom/cpus on some systems
					DIR* dir = opendir("/sys/devices/cpu_atom/cpus/");
					if (dir == nullptr)
						return 0;

					dirent* entry;
					while ((entry = readdir(dir)) != nullptr) {
						if (strncmp(entry->d_name, "cpu", 3) == 0 && entry->d_name[3] >= '0' && entry->d_name[3] <= '9')
							++efficiencyCores;
					}

					closedir(dir);
				}

				if (efficiencyCores == 0) {
					// TODO: Go through all CPUs packages and CPUs and determine the differences between them.
					//       There are many metrics.
					//       1) We will assume all CPUs to be of the same architecture.
					//       2) Same CPU architecture but different cache sizes. Smaller ones are "efficiency" cores.
					//          This is the AMD way. Still, these are about the same things so maybe we would just treat
					//          all such cores as performance cores.
					//       3) Different max frequencies on different cores. This might be indicative enough.
					//       There is also an optional parameter present on ARM CPUs:
					//       https://www.kernel.org/doc/Documentation/devicetree/bindings/arm/cpu-capacity.txt
					//       In this case, we'd treat CPUs with the highest capacity-dmips-mhz as performance cores,
					//       and consider the rest as efficiency cores.

					// ...
				}
#endif
				return efficiencyCores;
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
			//! \param background True when creating a background worker
			void create_thread(uint32_t workerIdx, JobPriority prio, bool background) {
				// Idx 0 is reserved for the main thread
				GAIA_ASSERT(workerIdx > 0);

				auto& ctx = m_workersCtx[workerIdx];
				ctx.tp = this;
				ctx.workerIdx = workerIdx;
				ctx.prio = prio;
				ctx.background = background;
				ctx.threadCreated = false;

#if GAIA_THREAD_PLATFORM == GAIA_THREAD_STD
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
				} else {
					ctx.threadCreated = true;
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

#if GAIA_THREAD_PLATFORM == GAIA_THREAD_STD
				auto& t = m_workers[workerIdx - 1];
				if (t.joinable())
					t.join();
#else
				auto& ctx = m_workersCtx[workerIdx];
				if (!ctx.threadCreated)
					return;

				auto& t = m_workers[workerIdx - 1];
				pthread_join(t, nullptr);
				ctx.threadCreated = false;
#endif
			}

			void create_worker_threads(uint32_t& workerIdx, JobPriority prio, uint32_t count) {
				for (uint32_t i = 0; i < count; ++i)
					create_thread(workerIdx++, prio, false);
			}

			void create_background_worker_threads(uint32_t& workerIdx) {
				for (uint32_t i = 0; i < m_backgroundWorkersCnt; ++i)
					create_thread(workerIdx++, JobPriority::Low, true);
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
				const bool background = m_workersCtx[workerIdx].background;
				const char* workerKind = background ? "BG" : prio == JobPriority::High ? "HI" : "LO";
#if GAIA_PROF_USE_PROFILER_THREAD_NAME
				char threadName[16]{};
				GAIA_STRFMT(threadName, 16, "worker_%s_%u", workerKind, workerIdx);
				GAIA_PROF_THREAD_NAME(threadName);
#elif GAIA_PLATFORM_WINDOWS
				auto nativeHandle = (HANDLE)m_workers[workerIdx - 1].native_handle();
				const wchar_t* workerKindW = background ? L"BG" : prio == JobPriority::High ? L"HI" : L"LO";

				TOSApiFunc_SetThreadDescription pSetThreadDescFunc = nullptr;
				if (auto* pModule = GetModuleHandleA("kernel32.dll")) {
					auto* pFunc = GetProcAddress(pModule, "SetThreadDescription");
					pSetThreadDescFunc = reinterpret_cast<TOSApiFunc_SetThreadDescription>(reinterpret_cast<void*>(pFunc));
				}
				if (pSetThreadDescFunc != nullptr) {
					wchar_t threadName[16]{};
					swprintf_s(threadName, L"worker_%s_%u", workerKindW, workerIdx);

					auto hr = pSetThreadDescFunc(nativeHandle, threadName);
					if (FAILED(hr)) {
						GAIA_LOG_W("Issue setting name for worker %s thread %u!", workerKind, workerIdx);
					}
				} else {
	#if defined _MSC_VER
					char threadName[16]{};
					GAIA_STRFMT(threadName, 16, "worker_%s_%u", workerKind, workerIdx);

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
				GAIA_STRFMT(threadName, 16, "worker_%s_%u", workerKind, workerIdx);
				auto ret = pthread_setname_np(threadName);
				if (ret != 0)
					GAIA_LOG_W("Issue setting name for worker %s thread %u!", workerKind, workerIdx);
#elif GAIA_PLATFORM_LINUX || GAIA_PLATFORM_FREEBSD
				auto nativeHandle = m_workers[workerIdx - 1];

				char threadName[16]{};
				GAIA_STRFMT(threadName, 16, "worker_%s_%u", workerKind, workerIdx);
				GAIA_PROF_THREAD_NAME(threadName);
				auto ret = pthread_setname_np(nativeHandle, threadName);
				if (ret != 0)
					GAIA_LOG_W("Issue setting name for worker %s thread %u!", workerKind, workerIdx);
#endif
			}

			//! Checks if the calling thread is considered the main thread
			//! \return True if the calling thread is the main thread. False otherwise.
			GAIA_NODISCARD bool main_thread() const {
				return std::this_thread::get_id() == m_mainThreadId;
			}

			//! Runs one main-thread work-drain pass.
			//! Pops ready jobs from the queues and executes them until no more work is immediately available.
			void main_thread_tick() {
				auto& ctx = *detail::tl_workerCtx;

				// Keep executing while there is work
				while (true) {
					JobHandle jobHandle;
					if (!try_fetch_job(ctx, jobHandle))
						break;

					(void)run(jobHandle, &ctx);
				}
			}

			//! Attempts to steal work from worker-local queues of the requested priority class.
			//! \param ctx Worker requesting more work.
			//! \param prio Priority class to search.
			//! \param[out] jobHandle Receives the stolen job handle when one is available.
			//! \return True when a valid job was obtained. False otherwise.
			GAIA_NODISCARD bool try_steal_job(ThreadCtx& ctx, JobPriority prio, JobHandle& jobHandle) {
				const auto workerCnt = m_workersCtx.size();
				for (uint32_t i = 0; i < workerCnt;) {
					// Keep stealing within the same priority class and skip our own queue
					if (i == ctx.workerIdx || m_workersCtx[i].background || m_workersCtx[i].prio != prio) {
						++i;
						continue;
					}

					const auto res = m_workersCtx[i].jobQueue.try_steal(jobHandle);
					// Race condition, try again from the same context
					if (!res)
						continue;

					// Stealing can return true if the queue is empty.
					// We return right away only if we receive a valid handle which means
					// when there was an idle job in the queue.
					if (jobHandle != (JobHandle)JobNull_t{})
						return true;

					++i;
				}

				return false;
			}

			//! Attempts to fetch work from the global queue or peer workers of one priority class.
			//! \param ctx Worker requesting more work.
			//! \param prio Priority class to search.
			//! \param[out] jobHandle Receives the ready job handle when one is available.
			//! \return True when a valid job was obtained. False otherwise.
			GAIA_NODISCARD bool try_fetch_prio(ThreadCtx& ctx, JobPriority prio, JobHandle& jobHandle) {
				if (m_jobQueue[(uint32_t)prio].try_pop(jobHandle))
					return true;

				return try_steal_job(ctx, prio, jobHandle);
			}

			//! Attempts to fetch background work.
			//! \param[out] jobHandle Receives the next ready background job when one is available.
			//! \return True when a valid background job was obtained. False otherwise.
			GAIA_NODISCARD bool try_fetch_background_job(JobHandle& jobHandle) {
				return m_jobQueueBackground.try_pop(jobHandle);
			}

			//! Attempts to fetch the next runnable job for a worker.
			//! \param ctx Worker requesting more work.
			//! \param[out] jobHandle Receives the next ready job handle when one is available.
			//! \return True when a valid job was obtained. False otherwise.
			GAIA_NODISCARD bool try_fetch_job(ThreadCtx& ctx, JobHandle& jobHandle) {
				if (ctx.background)
					return try_fetch_background_job(jobHandle);

				// Try getting a job from the local queue
				if (ctx.jobQueue.try_pop(jobHandle))
					return true;

				// The main thread may help with both queues while waiting or updating
				if (ctx.workerIdx == 0) {
					if (try_fetch_prio(ctx, JobPriority::High, jobHandle))
						return true;

					return try_fetch_prio(ctx, JobPriority::Low, jobHandle);
				}

				return try_fetch_prio(ctx, ctx.prio, jobHandle);
			}

			//! Checks whether \a ctx is allowed to execute \a jobData inline.
			//! \param ctx Calling worker context. Null when the submission comes from outside worker execution.
			//! \param jobData Job being considered.
			//! \return True when inline execution is allowed for forward progress.
			GAIA_NODISCARD bool can_run_inline(const ThreadCtx* ctx, const JobContainer& jobData) const {
				const bool background = is_background(jobData);
				if (background)
					return (ctx != nullptr && ctx->background) || m_backgroundWorkersCnt == 0;

				// The main thread is allowed to help with both priority classes.
				if (ctx == nullptr || ctx->workerIdx == 0)
					return true;

				// Matching worker classes may execute their own overflow inline.
				if (!ctx->background && ctx->prio == jobData.prio)
					return true;

				// If there are no spawned workers for the target priority we need to preserve
				// the forward-progress guarantee and allow inline fallback.
				return m_workerThreadsCnt[(uint32_t)jobData.prio] == 0;
			}

			//! Helps the scheduler make progress when a priority queue is temporarily full.
			//! \param ctx Calling worker context.
			//! \param jobData Job whose target queue is saturated.
			void wait_for_queue_space(ThreadCtx& ctx, const JobContainer& jobData) {
				const bool background = is_background(jobData);

				// Wake one worker from the target class in case all of them are asleep while
				// the producer is waiting for queue space to become available.
				if (background) {
					if (m_backgroundWorkersCnt != 0)
						m_semBackground.release(1);
				} else {
					const auto prioIdx = (uint32_t)jobData.prio;
					if (m_workerThreadsCnt[prioIdx] != 0)
						m_sem[prioIdx].release(1);
				}

				// Keep the current worker productive without violating the priority boundary.
				JobHandle otherJobHandle;
				const bool hasWork =
						ctx.background ? try_fetch_background_job(otherJobHandle) : try_fetch_prio(ctx, ctx.prio, otherJobHandle);
				if (hasWork) {
					(void)run(otherJobHandle, &ctx);
					return;
				}

				std::this_thread::yield();
			}

			//! Main worker-thread loop.
			//! Pops ready jobs from the queues and executes them until the pool shuts down.
			//! \param ctx Thread-local worker context.
			void worker_loop(ThreadCtx& ctx) {
				while (true) {
					// Wait for work
					if (ctx.background)
						m_semBackground.wait();
					else
						m_sem[(uint32_t)ctx.prio].wait();

					// Keep executing while there is work
					while (true) {
						JobHandle jobHandle;
						if (!try_fetch_job(ctx, jobHandle))
							break;

						(void)run(jobHandle, detail::tl_workerCtx);
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
				GAIA_FOR(JobPriorityCnt) {
					if (m_workerThreadsCnt[i] != 0)
						m_sem[i].release((int32_t)m_workerThreadsCnt[i]);
				}
				if (m_backgroundWorkersCnt != 0)
					m_semBackground.release((int32_t)m_backgroundWorkersCnt);

				auto* ctx = detail::tl_workerCtx;
				if (ctx == nullptr) {
					// reset() can be reached during static teardown from a thread that never
					// entered the pool and therefore has no TLS worker context bound.
					ctx = &m_workersCtx[0];
					detail::tl_workerCtx = ctx;
				}

				// Finish remaining jobs
				JobHandle jobHandle;
				while (try_fetch_job(*ctx, jobHandle)) {
					run(jobHandle, ctx);
				}
				while (try_fetch_background_job(jobHandle)) {
					run(jobHandle, ctx);
				}

				detail::tl_workerCtx = nullptr;

				// Join threads with the main one
				GAIA_FOR(m_workers.size()) join_thread(i + 1);

				// All threads have been stopped. Allow new threads to run if necessary.
				m_stop.store(false);
			}

			//! Makes sure the priority is right for the given set of allocated frame workers
			JobPriority final_frame_prio(JobPriority priority) {
				const auto cntWorkers = m_workersCnt[(uint32_t)priority];
				return cntWorkers > 0
									 // If there is enough workers, keep the priority
									 ? priority
									 // Not enough workers, use the other priority that has workers
									 : (JobPriority)(((uint32_t)priority + 1U) % (uint32_t)JobPriorityCnt);
			}

			//! Makes sure the priority is right for the given set of allocated workers
			JobPriority final_prio(const Job& job) {
				if ((job.flags & JobCreationFlags::Background) != 0U)
					return job.priority;

				return final_frame_prio(job.priority);
			}

			//! Makes sure the priority is right for the given set of allocated workers
			template <typename TJob>
			JobPriority final_prio(const TJob& job) {
				return final_frame_prio(job.priority);
			}

			//! Checks whether \a jobData is routed through the background worker queue.
			//! \param jobData Job to inspect.
			//! \return True when the job is a background job.
			GAIA_NODISCARD static bool is_background(const JobContainer& jobData) {
				return (jobData.flags & JobCreationFlags::Background) != 0U;
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
				GAIA_FOR(max) {
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

			//! Moves ready jobs into execution queues or runs them inline when queue capacity is exhausted.
			//! \param jobHandles Ready job handles to process.
			//! \param ctx Calling worker context. Null when the submission comes from outside worker execution.
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
					if (!jobData.func.operator bool())
						(void)run(handle, ctx);
					else
						pHandles[handlesCnt++] = handle;
				}

				std::span handles(pHandles, handlesCnt);
				while (!handles.empty()) {
					// Try pushing all jobs while preserving their priority queue ownership.
					uint32_t pushed = 0;
					uint32_t released[JobPriorityCnt]{};
					uint32_t backgroundReleased = 0;
					for (; pushed < handles.size(); ++pushed) {
						const auto handle = handles[pushed];
						const auto& jobData = m_jobManager.data(handle);
						if (is_background(jobData)) {
							if (!m_jobQueueBackground.try_push(handle))
								break;

							++backgroundReleased;
							continue;
						}

						const auto prio = jobData.prio;
						// Worker-local queues are reserved for work that matches the worker's own
						// priority class. Cross-priority releases must go through the matching
						// global queue so the right workers can pick them up.
						const bool useLocalQueue = ctx != nullptr && !ctx->background && ctx->workerIdx != 0 && ctx->prio == prio;
						const bool res =
								useLocalQueue ? ctx->jobQueue.try_push(handle) : m_jobQueue[(uint32_t)prio].try_push(handle);
						if (!res)
							break;

						released[(uint32_t)prio]++;
					}

					GAIA_FOR(JobPriorityCnt) {
						// Only spawned worker threads block on semaphores. The main thread helps by
						// draining queues opportunistically from wait() and update().
						const auto cnt = core::get_min(released[i], m_workerThreadsCnt[i]);
						if (cnt != 0)
							m_sem[i].release((int32_t)cnt);
					}
					const auto backgroundCnt = core::get_min(backgroundReleased, m_backgroundWorkersCnt);
					if (backgroundCnt != 0)
						m_semBackground.release((int32_t)backgroundCnt);

					handles = handles.subspan(pushed);
					if (!handles.empty()) {
						const auto handle = handles[0];
						const auto& jobData = m_jobManager.data(handle);

						if (can_run_inline(ctx, jobData)) {
							// The queue was full. Execute the job right away only when the
							// current execution context is allowed to run this priority class.
							run(handle, ctx);
							handles = handles.subspan(1);
						} else {
							GAIA_ASSERT(ctx != nullptr);
							wait_for_queue_space(*ctx, jobData);
						}
					}
				}
			}

			bool run(JobHandle jobHandle, ThreadCtx* ctx) {
				if (jobHandle == (JobHandle)JobNull_t{})
					return false;

				auto& jobData = m_jobManager.data(jobHandle);
				const bool manualDelete = (jobData.flags & JobCreationFlags::ManualDelete) != 0U;
				const bool canWait = (jobData.flags & JobCreationFlags::CanWait) != 0U;

				m_jobManager.executing(jobData, ctx->workerIdx);

				if (m_blockedInWorkUntil.load() != 0) {
					const auto blockedCnt = m_blockedInWorkUntil.exchange(0);
					if (blockedCnt != 0)
						Futex::wake(&m_blockedInWorkUntil, detail::WaitMaskAll);
				}

				GAIA_ASSERT(jobData.idx != (uint32_t)-1 && jobData.data.gen != (uint32_t)-1);

				// Run the functor associated with the job
				m_jobManager.run(jobData);

				// Signal the edges and release memory allocated for them if possible
				signal_edges(jobData);
				JobManager::free_edges(jobData);

				// Signal we finished
				ctx->event.set();
				if (canWait) {
					const auto* pFutexValue = &jobData.state;
					Futex::wake(pFutexValue, detail::WaitMaskAll);
				}

				if (!manualDelete)
					del(jobHandle);

				return true;
			}
		};

		GAIA_MSVC_WARNING_POP()
	} // namespace mt
} // namespace gaia
