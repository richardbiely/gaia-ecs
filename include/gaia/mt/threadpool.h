#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

#include "../containers/sarray_ext.h"
#include "../containers/sringbuffer.h"

namespace gaia {
	namespace mt {
		struct Job {
			std::function<void()> func;
		};

		struct JobArgs {
			uint32_t idxStart;
			uint32_t idxEnd;
		};

		struct JobParallel {
			std::function<void(const JobArgs&)> func;
		};

		class JobQueue {
			std::mutex m_jobsLock;
			containers::sringbuffer<Job, 512> m_jobs;

		public:
			//! Tries adding a job to the queue.
			//! \return True if the job was added. False if it could not be added (e.g. maximum capacity has been reached).
			bool Push(const Job& job) {
				std::lock_guard<std::mutex> guard(m_jobsLock);
				if (m_jobs.size() >= m_jobs.max_size())
					return false;

				m_jobs.push_back(job);
				return true;
			}

			//! Tries retriving a job to the queue.
			//! \return True if the job was retrived. False if it could not be retrived (e.g. there are no jobs).
			bool Pop(Job& job) {
				std::lock_guard<std::mutex> guard(m_jobsLock);
				if (m_jobs.empty())
					return false;
				m_jobs.pop_front(std::move(job));
				return true;
			}
		};

		class ThreadPool final {
			containers::sarr_ext<std::thread, 64> m_workers;

			//! List of pending jobs
			JobQueue m_jobs;
			//! State of execution of the main thread.
			//! So long it is greater than m_workersCounter it means workers are doing work.
			std::uint32_t m_mainCounter;
			//! State of execution of worker threads
			std::atomic_uint32_t m_workersCounter;

			std::mutex m_cvLock;
			std::condition_variable m_cv;

			//! When true the pool is supposed to finish all work and terminate all threads
			bool m_stop;

		private:
			ThreadPool(): m_stop(false), m_mainCounter(0), m_workersCounter(0) {
				uint32_t workerCount = CalculateThreadCount(0);
				if (workerCount > m_workers.max_size())
					workerCount = m_workers.max_size();

				m_workers.resize(workerCount);
				for (uint32_t i = 0; i < workerCount; ++i) {
					m_workers[i] = std::thread([this]() {
						ThreadLoop(m_stop);
					});
				}
			}

			ThreadPool(ThreadPool&&) = delete;
			ThreadPool(const ThreadPool&) = delete;
			ThreadPool& operator=(ThreadPool&&) = delete;
			ThreadPool& operator=(const ThreadPool&) = delete;

			static uint32_t CalculateThreadCount(uint32_t threadsWanted) {
				// Make sure a reasonable amount of threads is used
				if (threadsWanted == 0) {
					// Subtract one (the main thread)
					threadsWanted = std::thread::hardware_concurrency() - 1;
					if (threadsWanted <= 0)
						threadsWanted = 1;
				}
				return threadsWanted;
			}

			void ThreadLoop(const bool& stop) {
				Job job;
				while (!stop) {
					if (m_jobs.Pop(job)) {
						job.func();
						++m_workersCounter;
					} else {
						std::unique_lock<std::mutex> lock(m_cvLock);
						m_cv.wait(lock);
					}
				}
			}

		public:
			static ThreadPool& Get() {
				static ThreadPool threadPool;
				return threadPool;
			}

			uint32_t GetWorkersCount() const {
				return (uint32_t)m_workers.size();
			}

			~ThreadPool() {
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

			//! Schedules a job to run on a worker thread.
			//! \warning Must be used form the main thread.
			void Schedule(const Job& job) {
				// Don't add new jobs once stop was requested
				if GAIA_UNLIKELY (m_stop)
					return;

				++m_mainCounter;

				// Try pushing a new job until it we succeed.
				// The thread is put to sleep if pushing the jobs fails.
				while (!m_jobs.Push(job))
					Poll();

				// Wake one worker thread
				m_cv.notify_one();
			}

			//! Schedules a job to run on worker threads in parallel.
			//! \warning Must be used form the main thread.
			void ScheduleParallel(const JobParallel& job, uint32_t itemsToProcess, uint32_t groupSize) {
				// Empty data set are considered wrong inputs
				GAIA_ASSERT(itemsToProcess != 0);
				if (itemsToProcess == 0)
					return;

				// Don't add new jobs once stop was requested
				if GAIA_UNLIKELY (m_stop)
					return;

				// No group size was given, make a guess based on the set size
				if (groupSize == 0)
					groupSize = (itemsToProcess + m_workers.size() - 1) / m_workers.size();

				const auto jobs = (itemsToProcess + groupSize - 1) / groupSize;
				m_mainCounter += jobs;

				for (uint32_t jobIndex = 0; jobIndex < jobs; ++jobIndex) {
					// Create one job per group
					auto groupJob = [job, itemsToProcess, groupSize, jobIndex]() {
						const uint32_t groupJobIdxStart = jobIndex * groupSize;
						const uint32_t groupJobIdxEnd = std::min(groupJobIdxStart + groupSize, itemsToProcess);

						JobArgs args;
						args.idxStart = groupJobIdxStart;
						args.idxEnd = groupJobIdxEnd;
						job.func(args);
					};

					// Try to pushing a new job until it we succeed.
					// The thread is put to sleep if pushing the jobs fails.
					while (!m_jobs.Push({groupJob}))
						Poll();

					// Wake some worker thread
					m_cv.notify_one();
				}
			}

			//! Wait for all jobs to finish.
			//! \warning Must be used form the main thread.
			void CompleteAll() {
				while (IsBusy())
					Poll();
			}

		private:
			//! Checks whether workers are busy doing work.
			//!	\return True if any workers are busy doing work.
			bool IsBusy() const {
				return m_workersCounter.load() < m_mainCounter;
			}

			//! Wakes up some worker thread and reschedules the current one.
			void Poll() {
				// Wake some worker thread
				m_cv.notify_one();

				// Allow this thread to be rescheduled
				std::this_thread::yield();
			}
		};
	} // namespace mt
} // namespace gaia