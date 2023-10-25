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

#include "../cnt/sarray_ext.h"
#include "../config/logging.h"
#include "../core/span.h"

#include "jobcommon.h"
#include "jobhandle.h"
#include "jobmanager.h"
#include "jobqueue.h"

namespace gaia {
	namespace mt {

		class ThreadPool final {
			static constexpr uint32_t MaxWorkers = 32;

			//! ID of the main thread
			std::thread::id m_mainThreadId;
			//! List of worker threads
			cnt::sarr_ext<std::thread, MaxWorkers> m_workers;
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
			ThreadPool(): m_jobsPending(0), m_stop(false) {
				uint32_t workersCnt = calc_thread_cnt(0);
				if (workersCnt > MaxWorkers)
					workersCnt = MaxWorkers;

				m_workers.resize(workersCnt);

				m_mainThreadId = std::this_thread::get_id();

				for (uint32_t i = 0; i < workersCnt; ++i) {
					m_workers[i] = std::thread([this, i]() {
						// Set the worker thread name.
						// Needs to be called from inside the thread because some platforms
						// can change the name only when run from the specific thread.
						set_thread_name(i);

						// Process jobs
						worker_loop();
					});

					// Stick each thread to a specific CPU core
					set_thread_affinity(i);
				}
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

			GAIA_NODISCARD uint32_t workers() const {
				return m_workers.size();
			}

			~ThreadPool() {
				reset();
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
			JobHandle add(const Job& job) {
				GAIA_ASSERT(main_thread());

				// Don't add new jobs once stop was requested
				if GAIA_UNLIKELY (m_stop)
					return JobNull;

				++m_jobsPending;

				return m_jobManager.alloc_job(job);
			}

			//! Pushes \param jobHandle into the internal queue so worker threads
			//! can pick it up and execute it.
			//! If there are more jobs than the queue can handle it puts the calling
			//! thread to sleep until workers consume enough jobs.
			//! \warning Once submited, dependencies can't be modified for this job.
			void submit(JobHandle jobHandle) {
				m_jobManager.submit(jobHandle);

				// Try pushing a new job until it we succeed.
				// The thread is put to sleep if pushing the jobs fails.
				while (!m_jobQueue.try_push(jobHandle))
					poll();

				// Wake some worker thread
				m_cv.notify_one();
			}

		private:
			//! Resubmits \param jobHandle into the internal queue so worker threads
			//! can pick it up and execute it.
			//! If there are more jobs than the queue can handle it puts the calling
			//! thread to sleep until workers consume enough jobs.
			//! \warning Internal usage only. Only worker theads can decide to resubmit.
			void resubmit(JobHandle jobHandle) {
				m_jobManager.resubmit(jobHandle);

				// Try pushing a new job until it we succeed.
				// The thread is put to sleep if pushing the jobs fails.
				while (!m_jobQueue.try_push(jobHandle))
					poll();

				// Wake some worker thread
				m_cv.notify_one();
			}

		public:
			//! Schedules a job to run on a worker thread.
			//! \param job Job descriptor
			//! \warning Must be used from the main thread.
			//! \warning Dependencies can't be modified for this job.
			//! \return Job handle of the scheduled job.
			JobHandle sched(const Job& job) {
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
			JobHandle sched(const Job& job, JobHandle dependsOn) {
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
			JobHandle sched(const Job& job, std::span<const JobHandle> dependsOnSpan) {
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
			JobHandle sched_par(const JobParallel& job, uint32_t itemsToProcess, uint32_t groupSize) {
				GAIA_ASSERT(main_thread());

				// Empty data set are considered wrong inputs
				GAIA_ASSERT(itemsToProcess != 0);
				if (itemsToProcess == 0)
					return JobNull;

				// Don't add new jobs once stop was requested
				if GAIA_UNLIKELY (m_stop)
					return JobNull;

				const uint32_t workerCount = m_workers.size();

				// No group size was given, make a guess based on the set size
				if (groupSize == 0)
					groupSize = (itemsToProcess + workerCount - 1) / workerCount;

				const auto jobs = (itemsToProcess + groupSize - 1) / groupSize;
				// Internal jobs + 1 for the groupHandle
				m_jobsPending += (jobs + 1U);

				JobHandle groupHandle = m_jobManager.alloc_job({});

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

					JobHandle jobHandle = m_jobManager.alloc_job({groupJobFunc});
					dep(groupHandle, jobHandle);
					submit(jobHandle);
				}

				submit(groupHandle);
				return groupHandle;
			}

			//! Wait for the job to finish.
			//! \param jobHandle Job handle
			//! \warning Must be used from the main thread.
			void wait(JobHandle jobHandle) {
				GAIA_ASSERT(main_thread());

				while (m_jobManager.busy(jobHandle))
					poll();

				GAIA_ASSERT(!m_jobManager.busy(jobHandle));
				m_jobManager.wait(jobHandle);
			}

			//! Wait for all jobs to finish.
			//! \warning Must be used from the main thread.
			void wait_all() {
				GAIA_ASSERT(main_thread());

				while (busy())
					poll_all();

				GAIA_ASSERT(m_jobsPending == 0);
				m_jobManager.reset();
			}

		private:
			void set_thread_affinity(uint32_t threadID) {
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
				if (mask <= 0)
					GAIA_LOG_W("Issue setting thread affinity for worker thread %u!", threadID);
#elif GAIA_PLATFORM_APPLE
				pthread_t nativeHandle = (pthread_t)m_workers[threadID].native_handle();

				mach_port_t mach_thread = pthread_mach_thread_np(nativeHandle);
				thread_affinity_policy_data_t policy_data = {(int)threadID};
				auto ret = thread_policy_set(
						mach_thread, THREAD_AFFINITY_POLICY, (thread_policy_t)&policy_data, THREAD_AFFINITY_POLICY_COUNT);
				if (ret == 0)
					GAIA_LOG_W("Issue setting thread affinity for worker thread %u!", threadID);
#elif GAIA_PLATFORM_LINUX || GAIA_PLATFORM_FREEBSD
				pthread_t nativeHandle = (pthread_t)m_workers[threadID].native_handle();

				cpu_set_t cpuset;
				CPU_ZERO(&cpuset);
				CPU_SET(threadID, &cpuset);

				auto ret = pthread_setaffinity_np(nativeHandle, sizeof(cpuset), &cpuset);
				if (ret != 0)
					GAIA_LOG_W("Issue setting thread affinity for worker thread %u!", threadID);

				ret = pthread_getaffinity_np(nativeHandle, sizeof(cpuset), &cpuset);
				if (ret != 0)
					GAIA_LOG_W("Thread affinity could not be set for worker thread %u!", threadID);
#endif
			}

			void set_thread_name(uint32_t threadID) {
#if GAIA_PLATFORM_WINDOWS
				auto nativeHandle = (HANDLE)m_workers[threadID].native_handle();

				wchar_t threadName[10]{};
				swprintf_s(threadName, L"worker_%u", threadID);
				auto hr = SetThreadDescription(nativeHandle, threadName);
				if (FAILED(hr))
					GAIA_LOG_W("Issue setting worker thread name!");
#elif GAIA_PLATFORM_APPLE
				char threadName[10]{};
				snprintf(threadName, 10, "worker_%u", threadID);
				auto ret = pthread_setname_np(threadName);
				if (ret != 0)
					GAIA_LOG_W("Issue setting name for worker thread %u!", threadID);
#elif GAIA_PLATFORM_LINUX || GAIA_PLATFORM_FREEBSD
				auto nativeHandle = (pthread_t)m_workers[threadID].native_handle();

				char threadName[10]{};
				snprintf(threadName, 10, "worker_%u", threadID);
				auto ret = pthread_setname_np(nativeHandle, threadName);
				if (ret != 0)
					GAIA_LOG_W("Issue setting name for worker thread %u!", threadID);
#endif
			}

			GAIA_NODISCARD bool main_thread() const {
				return std::this_thread::get_id() == m_mainThreadId;
			}

			GAIA_NODISCARD static uint32_t calc_thread_cnt(uint32_t threadsWanted) {
				// Make sure a reasonable amount of threads is used
				if (threadsWanted == 0) {
					// Subtract one (the main thread)
					threadsWanted = std::thread::hardware_concurrency() - 1;
					if (threadsWanted <= 0)
						threadsWanted = 1;
				}
				return threadsWanted;
			}

			void worker_loop() {
				while (!m_stop) {
					JobHandle jobHandle;

					if (!m_jobQueue.try_pop(jobHandle)) {
						std::unique_lock<std::mutex> lock(m_cvLock);
						m_cv.wait(lock);
						continue;
					}

					GAIA_ASSERT(m_jobsPending > 0);

					// Make sure we can execute the job.
					// If it has dependencies which were not completed we need to reschedule
					// and come back to it later.
					if (!m_jobManager.handle_deps(jobHandle)) {
						resubmit(jobHandle);
						continue;
					}

					m_jobManager.run(jobHandle);
					--m_jobsPending;
				}
			}

			void reset() {
				// Request stopping
				m_stop = true;
				// complete all remaining work
				wait_all();
				// Wake up any threads that were put to sleep
				m_cv.notify_all();
				// Join threads with the main one
				for (auto& w: m_workers) {
					if (w.joinable())
						w.join();
				}
			}

			//! Checks whether workers are busy doing work.
			//!	\return True if any workers are busy doing work.
			GAIA_NODISCARD bool busy() const {
				return m_jobsPending > 0;
			}

			//! Wakes up some worker thread and reschedules the current one.
			void poll() {
				// Wake some worker thread
				m_cv.notify_one();

				// Allow this thread to be rescheduled
				std::this_thread::yield();
			}

			//! Wakes up all worker threads and reschedules the current one.
			void poll_all() {
				// Wake some worker thread
				m_cv.notify_all();

				// Allow this thread to be rescheduled
				std::this_thread::yield();
			}
		};
	} // namespace mt
} // namespace gaia