#pragma once

#include "../config/config.h"

#if GAIA_PLATFORM_WINDOWS
	#include <windows.h>
	#include <cstdio>
#elif GAIA_PLATFORM_APPLE
	#include <mach/mach_types.h>
	#include <mach/thread_act.h>
#elif GAIA_PLATFORM_LINUX || GAIA_PLATFORM_FREEBSD
	#include <pthread.h>
#endif

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "../config/logging.h"
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
					m_workers[i] = std::thread([this, i]() {
						// Set the worker thread name.
						// Needs to be called from inside the thread because some platforms
						// can change the name only when run from the specific thread.
						SetThreadName(i);

						// Process jobs
						ThreadLoop();
					});

					// Stick each thread to a specific CPU core
					SetThreadAffinity(i);
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

			//! Makes \param jobHandle depend on \param dependsOn.
			//! This means \param jobHandle will run only after \param dependsOn finishes.
			//! \warning Must be used from the main thread.
			//! \warning Needs to be called before any of the listed jobs are scheduled.
			void AddDependency(JobHandle jobHandle, JobHandle dependsOn) {
				m_jobManager.AddDependency(jobHandle, dependsOn);
			}

			//! Makes \param jobHandle depend on the jobs listed in \param dependsOnSpan.
			//! This means \param jobHandle will run only after all \param dependsOnSpan jobs finish.
			//! \warning Must be used from the main thread.
			//! \warning Needs to be called before any of the listed jobs are scheduled.
			void AddDependencies(JobHandle jobHandle, std::span<const JobHandle> dependsOnSpan) {
				m_jobManager.AddDependencies(jobHandle, dependsOnSpan);
			}

			//! Creates a job system job from \param job.
			//! \warning Must be used from the main thread.
			//! \return Job handle of the scheduled job.
			JobHandle CreateJob(const Job& job) {
				GAIA_ASSERT(IsMainThread());

				// Don't add new jobs once stop was requested
				if GAIA_UNLIKELY (m_stop)
					return JobNull;

				++m_jobsPending;

				return m_jobManager.AllocateJob(job);
			}

			//! Pushes \param jobHandle into the internal queue so worker threads
			//! can pick it up and execute it.
			//! If there are more jobs than the queue can handle it puts the calling
			//! thread to sleep until workers consume enough jobs.
			//! \warning Once submited, dependencies can't be modified for this job.
			void Submit(JobHandle jobHandle) {
				m_jobManager.Submit(jobHandle);

				// Try pushing a new job until it we succeed.
				// The thread is put to sleep if pushing the jobs fails.
				while (!m_jobQueue.TryPush(jobHandle))
					Poll();

				// Wake some worker thread
				m_cv.notify_one();
			}

		private:
			//! Resubmits \param jobHandle into the internal queue so worker threads
			//! can pick it up and execute it.
			//! If there are more jobs than the queue can handle it puts the calling
			//! thread to sleep until workers consume enough jobs.
			//! \warning Internal usage only. Only worker theads can decide to resubmit.
			void ReSubmit(JobHandle jobHandle) {
				m_jobManager.ReSubmit(jobHandle);

				// Try pushing a new job until it we succeed.
				// The thread is put to sleep if pushing the jobs fails.
				while (!m_jobQueue.TryPush(jobHandle))
					Poll();

				// Wake some worker thread
				m_cv.notify_one();
			}

		public:
			//! Schedules a job to run on a worker thread.
			//! \param job Job descriptor
			//! \warning Must be used from the main thread.
			//! \warning Dependencies can't be modified for this job.
			//! \return Job handle of the scheduled job.
			JobHandle Schedule(const Job& job) {
				JobHandle jobHandle = CreateJob(job);
				Submit(jobHandle);
				return jobHandle;
			}

			//! Schedules a job to run on a worker thread.
			//! \param job Job descriptor
			//! \param dependsOn Job we depend on
			//! \warning Must be used from the main thread.
			//! \warning Dependencies can't be modified for this job.
			//! \return Job handle of the scheduled job.
			JobHandle Schedule(const Job& job, JobHandle dependsOn) {
				JobHandle jobHandle = CreateJob(job);
				AddDependency(jobHandle, dependsOn);
				Submit(jobHandle);
				return jobHandle;
			}

			//! Schedules a job to run on a worker thread.
			//! \param job Job descriptor
			//! \param dependsOnSpan Jobs we depend on
			//! \warning Must be used from the main thread.
			//! \warning Dependencies can't be modified for this job.
			//! \return Job handle of the scheduled job.
			JobHandle Schedule(const Job& job, std::span<const JobHandle> dependsOnSpan) {
				JobHandle jobHandle = CreateJob(job);
				AddDependencies(jobHandle, dependsOnSpan);
				Submit(jobHandle);
				return jobHandle;
			}

			//! Schedules a job to run on worker threads in parallel.
			//! \param job Job descriptor
			//! \param itemsToProcess Total number of work items
			//! \param groupSize Group size per created job. If zero the job system decides the group size.
			//! \warning Must be used from the main thread.
			//! \warning Dependencies can't be modified for this job.
			//! \return Job handle of the scheduled batch of jobs.
			JobHandle ScheduleParallel(const JobParallel& job, uint32_t itemsToProcess, uint32_t groupSize) {
				GAIA_ASSERT(IsMainThread());

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
				// Internal jobs + 1 for the groupHandle
				m_jobsPending += (jobs + 1U);

				JobHandle groupHandle = m_jobManager.AllocateJob({});

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
					AddDependency(groupHandle, jobHandle);
					Submit(jobHandle);
				}

				Submit(groupHandle);
				return groupHandle;
			}

			//! Wait for the job to finish.
			//! \param jobHandle Job handle
			//! \warning Must be used from the main thread.
			void Complete(JobHandle jobHandle) {
				GAIA_ASSERT(IsMainThread());

				while (m_jobManager.IsBusy(jobHandle))
					Poll();

				GAIA_ASSERT(!m_jobManager.IsBusy(jobHandle));
				m_jobManager.Complete(jobHandle);
			}

			//! Wait for all jobs to finish.
			//! \warning Must be used from the main thread.
			void CompleteAll() {
				GAIA_ASSERT(IsMainThread());

				while (IsBusy())
					PollAll();

				GAIA_ASSERT(m_jobsPending == 0);
				m_jobManager.Reset();
			}

		private:
			void SetThreadAffinity(uint32_t threadID) {
				// TODO:
				// Some cores might have multiple logic threads, there might be
				// more socket and some cores might even be physically different
				// form others (performance vs efficiency cores).
				// Therefore, we either need some more advanced logic here or we
				// should completly drop the idea of assigning affinity and simply
				// let the OS scheduler figure things out.
#if GAIA_PLATFORM_WINDOWS
				auto nativeHandle = (HANDLE)m_workers[threadID].native_handle();

				auto mask = SetThreadAffinityMask(nativeHandle, 1ULL << threadID);
				GAIA_ASSERT(mask > 0);
				if (mask <= 0)
					GAIA_LOG_W("Issue setting thread affinity for worker thread %u!", threadID);
#elif GAIA_PLATFORM_APPLE
				auto nativeHandle = (pthread_t)m_workers[threadID].native_handle();

				mach_port_t mach_thread = pthread_mach_thread_np(nativeHandle);
				thread_affinity_policy_data_t policy_data = {(int)threadID};
				auto ret = thread_policy_set(
						mach_thread, THREAD_AFFINITY_POLICY, (thread_policy_t)&policy_data, THREAD_AFFINITY_POLICY_COUNT);
				GAIA_ASSERT(ret != 0);
				if (ret == 0)
					GAIA_LOG_W("Issue setting thread affinity for worker thread %u!", threadID);
#elif GAIA_PLATFORM_LINUX || GAIA_PLATFORM_FREEBSD
				auto nativeHandle = (pthread_t)m_workers[threadID].native_handle();

				cpu_set_t cpuset;
				CPU_ZERO(&cpuset);
				CPU_SET(threadID, &cpuset);

				auto ret = pthread_setaffinity_np(nativeHandle, sizeof(cpuset), &cpuset);
				GAIA_ASSERT(ret == 0);
				if (ret != 0)
					GAIA_LOG_W("Issue setting thread affinity for worker thread %u!", threadID);

				ret = pthread_getaffinity_np(nativeHandle, sizeof(cpuset), &cpuset);
				GAIA_ASSERT(ret == 0);
				if (ret != 0)
					GAIA_LOG_W("Thread affinity could not be set for worker thread %u!", threadID);
#endif
			}

			void SetThreadName(uint32_t threadID) {
#if GAIA_PLATFORM_WINDOWS
				auto nativeHandle = (HANDLE)m_workers[threadID].native_handle();

				wchar_t threadName[10]{};
				swprintf_s(threadName, L"worker_%u", threadID);
				auto hr = SetThreadDescription(nativeHandle, threadName);
				GAIA_ASSERT(SUCCEEDED(hr));
				if (FAILED(hr))
					GAIA_LOG_W("Issue setting worker thread name!");
#elif GAIA_PLATFORM_APPLE
				auto nativeHandle = (pthread_t)m_workers[threadID].native_handle();

				char threadName[10]{};
				snprintf(threadName, 10, "worker_%u", threadID);
				auto ret = pthread_setname_np(threadName);
				GAIA_ASSERT(ret == 0);
				if (ret != 0)
					GAIA_LOG_W("Issue setting name for worker thread %u!", threadID);
#elif GAIA_PLATFORM_LINUX || GAIA_PLATFORM_FREEBSD
				auto nativeHandle = (pthread_t)m_workers[threadID].native_handle();

				char threadName[10]{};
				snprintf(threadName, 10, "worker_%u", threadID);
				auto ret = pthread_setname_np(nativeHandle, threadName);
				GAIA_ASSERT(ret == 0);
				if (ret != 0)
					GAIA_LOG_W("Issue setting name for worker thread %u!", threadID);
#endif
			}

			GAIA_NODISCARD bool IsMainThread() const {
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

					// Make sure we can execute the job.
					// If it has dependencies which were not completed we need to reschedule
					// and come back to it later.
					if (!m_jobManager.HandleDependencies(jobHandle)) {
						ReSubmit(jobHandle);
						continue;
					}

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