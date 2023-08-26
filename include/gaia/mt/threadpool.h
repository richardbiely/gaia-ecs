#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "../config/config_core_end.h"
#include "../containers/sarray_ext.h"
#include "../utils/span.h"

#include "jobcommon.h"
#include "jobhandle.h"
#include "jobmanager.h"
#include "jobqueue.h"

namespace gaia {
	namespace mt {

		class ThreadPool final {
			std::thread::id m_mainThreadId;

			//! List of worker threads
			containers::sarr_ext<std::thread, 64> m_workers;
			//! Manager for internal jobs
			JobManager m_jobManager;
			//! List of pending user jobs
			JobQueue m_jobQueue;

			//! How many jobs are currently being processed
			std::atomic_uint32_t m_jobsPending;

			std::mutex m_cvLock;
			std::condition_variable m_cv;

			//! When true the pool is supposed to finish all work and terminate all threads
			bool m_stop;

		private:
			ThreadPool(): m_stop(false), m_jobsPending(0) {
				uint32_t workerCount = CalculateThreadCount(0);
				if (workerCount > m_workers.max_size())
					workerCount = (uint32_t)m_workers.max_size();

				m_workers.resize(workerCount);

				m_mainThreadId = std::this_thread::get_id();

				for (uint32_t i = 0; i < workerCount; ++i) {
					m_workers[i] = std::thread([this]() {
						ThreadLoop();
					});
				}
			}

			ThreadPool(ThreadPool&&) = delete;
			ThreadPool(const ThreadPool&) = delete;
			ThreadPool& operator=(ThreadPool&&) = delete;
			ThreadPool& operator=(const ThreadPool&) = delete;

		public:
			static ThreadPool& Get() {
				static ThreadPool threadPool;
				return threadPool;
			}

			GAIA_NODISCARD uint32_t GetWorkersCount() const {
				return (uint32_t)m_workers.size();
			}

			~ThreadPool() {
				Reset();
			}

			//! Schedules a job to run on a worker thread.
			//! \warning Must be used form the main thread.
			//! \return Job handle of the scheduled job.
			JobHandle Schedule(const Job& job) {
				GAIA_ASSERT(VerifyMainThread());

				// Don't add new jobs once stop was requested
				if GAIA_UNLIKELY (m_stop)
					return JobNull;

				++m_jobsPending;

				JobHandle jobHandle = m_jobManager.AllocateJob(job);
				Submit(jobHandle);
				return jobHandle;
			}

#if GAIA_ENABLE_JOB_DEPENDENCIES
			//! Schedules a job to run on a worker thread.
			//! \warning Must be used form the main thread.
			//! \return Job handle of the scheduled job.
			JobHandle Schedule(const Job& job, JobHandle dependsOn) {
				GAIA_ASSERT(VerifyMainThread());

				// Don't add new jobs once stop was requested
				if GAIA_UNLIKELY (m_stop)
					return JobNull;

				++m_jobsPending;

				JobHandle jobHandle = m_jobManager.AllocateJob(job);
				m_jobManager.AddDependency(jobHandle, dependsOn);
				Submit(jobHandle);
				return jobHandle;
			}

			//! Schedules a job to run on a worker thread.
			//! \warning Must be used form the main thread.
			//! \return Job handle of the scheduled job.
			JobHandle Schedule(const Job& job, std::span<const JobHandle> dependsOnSpan) {
				GAIA_ASSERT(VerifyMainThread());

				// Don't add new jobs once stop was requested
				if GAIA_UNLIKELY (m_stop)
					return JobNull;

				++m_jobsPending;

				JobHandle jobHandle = m_jobManager.AllocateJob(job);
				m_jobManager.AddDependencies(jobHandle, dependsOnSpan);
				Submit(jobHandle);
				return jobHandle;
			}
#endif

			//! Schedules a job to run on worker threads in parallel.
			//! \warning Must be used form the main thread.
			JobHandle ScheduleParallel(const JobParallel& job, uint32_t itemsToProcess, uint32_t groupSize) {
				GAIA_ASSERT(VerifyMainThread());

				// Empty data set are considered wrong inputs
				GAIA_ASSERT(itemsToProcess != 0);
				if (itemsToProcess == 0)
					return JobNull;

				// Don't add new jobs once stop was requested
				if GAIA_UNLIKELY (m_stop)
					return JobNull;

				const uint32_t workerCount = (uint32_t)m_workers.size();

				// No group size was given, make a guess based on the set size
				if (groupSize == 0)
					groupSize = (itemsToProcess + workerCount - 1) / workerCount;

				const auto jobs = (itemsToProcess + groupSize - 1) / groupSize;
				m_jobsPending += jobs;

#if GAIA_ENABLE_JOB_DEPENDENCIES
				JobHandle groupHandle = m_jobManager.AllocateJob({});
#endif

				for (uint32_t jobIndex = 0; jobIndex < jobs; ++jobIndex) {
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

					JobHandle jobHandle = m_jobManager.AllocateJob({groupJobFunc});
#if GAIA_ENABLE_JOB_DEPENDENCIES
					m_jobManager.AddDependency(groupHandle, jobHandle);
#endif
					Submit(jobHandle);
				}

#if GAIA_ENABLE_JOB_DEPENDENCIES
				Submit(groupHandle);
				return groupHandle;
#else
				return JobNull;
#endif
			}

			//! Wait for the job to finish.
			//! \param jobHandle Job handle
			//! \warning Must be used form the main thread.
			void Complete(JobHandle jobHandle) {
				GAIA_ASSERT(VerifyMainThread());

				while (m_jobManager.IsBusy(jobHandle))
					Poll();

				GAIA_ASSERT(!m_jobManager.IsBusy(jobHandle));
				m_jobManager.DeallocateJob(jobHandle);
			}

			//! Wait for all jobs to finish.
			//! \warning Must be used form the main thread.
			void CompleteAll() {
				GAIA_ASSERT(VerifyMainThread());

				while (IsBusy())
					PollAll();

				GAIA_ASSERT(m_jobsPending == 0);
				m_jobManager.DeallocateAllJobs();
			}

		private:
			GAIA_NODISCARD bool VerifyMainThread() const {
				return std::this_thread::get_id() == m_mainThreadId;
			}

			GAIA_NODISCARD static uint32_t CalculateThreadCount(uint32_t threadsWanted) {
				// Make sure a reasonable amount of threads is used
				if (threadsWanted == 0) {
					// Subtract one (the main thread)
					threadsWanted = std::thread::hardware_concurrency() - 1;
					if (threadsWanted <= 0)
						threadsWanted = 1;
				}
				return threadsWanted;
			}

			void ThreadLoop() {
				while (!m_stop) {
					JobHandle jobHandle;

					if (!m_jobQueue.TryPop(jobHandle)) {
						std::unique_lock<std::mutex> lock(m_cvLock);
						m_cv.wait(lock);
						continue;
					}

					GAIA_ASSERT(m_jobsPending > 0);

#if GAIA_ENABLE_JOB_DEPENDENCIES
					// Make sure we can execute the job.
					// If it has dependencies which were not completed we need to reschedule
					// and come back to it later.
					if (!m_jobManager.HandleDependencies(jobHandle)) {
						Submit(jobHandle);
						continue;
					}
#endif

					m_jobManager.Run(jobHandle);
					--m_jobsPending;
				}
			}

			void Reset() {
				// Request stopping
				m_stop = true;
				// Complete all remaining work
				CompleteAll();
				// Wake up any threads that were put to sleep
				m_cv.notify_all();
				// Join threads with the main one
				for (auto& w: m_workers)
					w.join();
			}

			void Submit(JobHandle jobHandle) {
				m_jobManager.Submit(jobHandle);

				// Try pushing a new job until it we succeed.
				// The thread is put to sleep if pushing the jobs fails.
				while (!m_jobQueue.TryPush(jobHandle))
					Poll();

				// Wake some worker thread
				m_cv.notify_one();
			}

			//! Checks whether workers are busy doing work.
			//!	\return True if any workers are busy doing work.
			GAIA_NODISCARD bool IsBusy() const {
				return m_jobsPending > 0;
			}

			//! Wakes up some worker thread and reschedules the current one.
			void Poll() {
				// Wake some worker thread
				m_cv.notify_one();

				// Allow this thread to be rescheduled
				std::this_thread::yield();
			}

			//! Wakes up all worker threads and reschedules the current one.
			void PollAll() {
				// Wake some worker thread
				m_cv.notify_all();

				// Allow this thread to be rescheduled
				std::this_thread::yield();
			}
		};
	} // namespace mt
} // namespace gaia