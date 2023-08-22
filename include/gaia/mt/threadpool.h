#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

#include "../containers/sarray.h"
#include "../containers/sarray_ext.h"

#define JOB_QUEUE_USE_LOCKS 1
#if JOB_QUEUE_USE_LOCKS
	#include "../containers/sringbuffer.h"
#endif

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

		struct JobHandle {
			uint32_t idx;
		};

		struct JobInternal {
			std::function<void()> func;
		};

		struct JobManager {
			static constexpr uint32_t N = 1 << 12;
			static constexpr uint32_t MASK = N - 1;

		private:
			containers::sarray<JobInternal, N> m_jobs;
			uint32_t m_jobsAllocated = 0;

		public:
			GAIA_NODISCARD JobHandle AllocateJob(const Job& job) {
				const uint32_t idx = (m_jobsAllocated++) & MASK;
				auto& jobBufferItem = m_jobs[idx];
				jobBufferItem.func = job.func;
				return {idx};
			}

			void Run(JobHandle jobHandle) {
				m_jobs[jobHandle.idx].func();
			}
		};

		class JobQueue {
#if JOB_QUEUE_USE_LOCKS
			std::mutex m_jobsLock;
			containers::sringbuffer<JobHandle, JobManager::N> m_buffer;
#else
			containers::sarray<JobHandle, JobManager::N> m_buffer;
			std::atomic_uint32_t m_bottom{};
			std::atomic_uint32_t m_top{};
#endif

		public:
			//! Tries adding a job to the queue. FIFO.
			//! \return True if the job was added. False otherwise (e.g. maximum capacity has been reached).
			GAIA_NODISCARD bool TryPush(JobHandle jobHandle) {
#if JOB_QUEUE_USE_LOCKS
				std::lock_guard<std::mutex> guard(m_jobsLock);
				if (m_buffer.size() >= m_buffer.max_size())
					return false;
				m_buffer.push_back(jobHandle);
#else
				const uint32_t b = m_bottom.load(std::memory_order_relaxed);
				uint32_t next_b = (b + 1) & JobManager::MASK;

				// Return false if the queue is full
				if (next_b == m_top.load(std::memory_order_acquire))
					return false;

				m_buffer[b] = jobHandle;
				m_bottom.store(next_b, std::memory_order_release);
#endif

				return true;
			}

			//! Tries retriving a job to the queue. FIFO.
			//! \return True if the job was retrived. False otherwise (e.g. there are no jobs).
			GAIA_NODISCARD bool TryPop(JobHandle& jobHandle) {
#if JOB_QUEUE_USE_LOCKS
				std::lock_guard<std::mutex> guard(m_jobsLock);
				if (m_buffer.empty())
					return false;

				m_buffer.pop_front(jobHandle);
#else
				const uint32_t t = m_top.load(std::memory_order_relaxed);

				// Return false when empty
				if (t == m_bottom.load(std::memory_order_acquire))
					return false;

				jobHandle = m_buffer[t];
				m_top.store((t + 1) & JobManager::MASK, std::memory_order_release);
#endif

				return true;
			}

			//! Tries stealing a job from the queue. LIFO.
			//! \return True if the job was stolen. False otherwise (e.g. there are no jobs).
			GAIA_NODISCARD bool TrySteal(JobHandle& jobHandle) {
#if JOB_QUEUE_USE_LOCKS
				std::lock_guard<std::mutex> guard(m_jobsLock);
				if (m_buffer.empty())
					return false;

				m_buffer.pop_back(jobHandle);
#else
				uint32_t t = m_top.load(std::memory_order_acquire);

				// Return false when empty
				if (t == m_bottom.load(std::memory_order_relaxed))
					return false;

				jobHandle = m_buffer[t];

				// Update m_top using CAS
				while (!m_top.compare_exchange_weak(
						t, (t + 1) & JobManager::MASK, std::memory_order_release, std::memory_order_relaxed)) {
					t = m_top.load(std::memory_order_relaxed);
					if (t == m_bottom.load(std::memory_order_acquire))
						return false;
				}
#endif
				return true;
			}
		};

		class ThreadPool final {
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
					workerCount = m_workers.max_size();

				m_workers.resize(workerCount);

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
					if (m_jobQueue.TryPop(jobHandle)) {
						m_jobManager.Run(jobHandle);
						--m_jobsPending;
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

			GAIA_NODISCARD uint32_t GetWorkersCount() const {
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

				++m_jobsPending;

				JobHandle jobHandle = m_jobManager.AllocateJob(job);

				// Try pushing a new job until it we succeed.
				// The thread is put to sleep if pushing the jobs fails.
				while (!m_jobQueue.TryPush(jobHandle))
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

				const uint32_t workerCount = (uint32_t)m_workers.size();

				// No group size was given, make a guess based on the set size
				if (groupSize == 0)
					groupSize = (itemsToProcess + workerCount - 1) / workerCount;

				const auto jobs = (itemsToProcess + groupSize - 1) / groupSize;
				m_jobsPending += jobs;

				for (uint32_t jobIndex = 0; jobIndex < jobs; ++jobIndex) {
					// Create one job per group
					auto groupJobFunc = [job, itemsToProcess, groupSize, jobIndex]() {
						const uint32_t groupJobIdxStart = jobIndex * groupSize;
						const uint32_t groupJobIdxEnd = std::min(groupJobIdxStart + groupSize, itemsToProcess);

						JobArgs args;
						args.idxStart = groupJobIdxStart;
						args.idxEnd = groupJobIdxEnd;
						job.func(args);
					};

					JobHandle jobHandle = m_jobManager.AllocateJob({groupJobFunc});

					// Try to pushing a new job until it we succeed.
					// The thread is put to sleep if pushing the jobs fails.
					while (!m_jobQueue.TryPush(jobHandle))
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
		};
	} // namespace mt
} // namespace gaia