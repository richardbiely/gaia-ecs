#pragma once

#include "../config/config.h"

#include <mutex>

#include "../cnt/sarray.h"
#include "../cnt/sringbuffer.h"
#include "../config/profiler.h"
#include "jobhandle.h"

namespace gaia {
	namespace mt {
		class JobQueue {
			//! The maximum number of jobs fitting in the queue at the same time
			static constexpr uint32_t N = 1 << 12;

			GAIA_PROF_MUTEX(std::mutex, m_bufferLock);
			cnt::sringbuffer<JobHandle, N> m_buffer;

		public:
			//! Checks if there are any items in the queue.
			//! \return True if the queue is empty. False otherwise.
			bool empty() {
				GAIA_PROF_SCOPE(JobQueue::empty);

				std::scoped_lock lock(m_bufferLock);
				return m_buffer.empty();
			}

			//! Tries adding a job to the queue. FIFO.
			//! \return True if the job was added. False otherwise (e.g. maximum capacity has been reached).
			GAIA_NODISCARD bool try_push(JobHandle jobHandle) {
				GAIA_PROF_SCOPE(JobQueue::try_push);

				std::scoped_lock lock(m_bufferLock);
				if (m_buffer.size() >= m_buffer.max_size())
					return false;

				m_buffer.push_back(jobHandle);
				return true;
			}

			//! Tries retrieving a job to the queue. FIFO.
			//! \return True if the job was retrieved. False otherwise (e.g. there are no jobs).
			GAIA_NODISCARD bool try_pop(JobHandle& jobHandle) {
				GAIA_PROF_SCOPE(JobQueue::try_pop);

				std::scoped_lock lock(m_bufferLock);
				if (m_buffer.empty())
					return false;

				m_buffer.pop_front(jobHandle);
				return true;
			}

			//! Tries stealing a job from the queue. LIFO.
			//! \return True if the job was stolen. False otherwise (e.g. there are no jobs).
			GAIA_NODISCARD bool try_steal(JobHandle& jobHandle) {
				GAIA_PROF_SCOPE(JobQueue::try_steal);

				std::scoped_lock lock(m_bufferLock);
				if (m_buffer.empty())
					return false;

				m_buffer.pop_back(jobHandle);
				return true;
			}
		};

		class JobQueueLockFree {
			//! The maximum number of jobs fitting in the queue at the same time
			static constexpr uint32_t N = 1 << 12;
			static constexpr uint32_t MASK = N - 1;

			// Lock-less version is inspired heavily by:
			// http://www.dre.vanderbilt.edu/~schmidt/PDF/work-stealing-dequeue.pdf

			alignas(GAIA_CACHELINE_SIZE) std::atomic_uint32_t m_bottom = 0;
			alignas(GAIA_CACHELINE_SIZE) std::atomic_uint32_t m_top = 0;

			static_assert(sizeof(std::atomic_uint32_t) == sizeof(JobHandle));
			cnt::sarray<std::atomic_uint32_t, N> m_buffer;

		public:
			//! Checks if there are any items in the queue.
			//! \return True if the queue is empty. False otherwise.
			bool empty() const {
				GAIA_PROF_SCOPE(JobQueue::empty);

				const uint32_t b = m_bottom.load(std::memory_order_acquire);
				const uint32_t t = m_top.load(std::memory_order_acquire);
				return (t >= b);
			}

			//! Tries adding a job to the queue. FIFO.
			//! \return True if the job was added. False otherwise (e.g. maximum capacity has been reached).
			GAIA_NODISCARD bool try_push(JobHandle jobHandle) {
				GAIA_PROF_SCOPE(JobQueue::try_push);

				const uint32_t b = m_bottom.load(std::memory_order_relaxed);
				const uint32_t t = m_top.load(std::memory_order_acquire);

				if (b - t > MASK)
					return false;

				m_buffer[b & MASK].store(jobHandle.value(), std::memory_order_relaxed);

				// Make sure the handle is written before we update the bottom
				std::atomic_thread_fence(std::memory_order_release);

				m_bottom.store(b + 1, std::memory_order_relaxed);
				return true;
			}

			//! Tries retrieving a job to the queue. FIFO.
			//! \return True if the job was retrieved. False otherwise (e.g. there are no jobs).
			GAIA_NODISCARD bool try_pop(JobHandle& jobHandle) {
				GAIA_PROF_SCOPE(JobQueue::try_pop);

				const uint32_t b = m_bottom.load(std::memory_order_relaxed) - 1;
				m_bottom.store(b, std::memory_order_relaxed);

				std::atomic_thread_fence(std::memory_order_seq_cst);

				uint32_t jobHandleValue = ((JobHandle)JobNull_t{}).value();

				uint32_t t = m_top.load(std::memory_order_relaxed);
				if (t <= b) {
					// non-empty queue
					jobHandleValue = m_buffer[b & MASK].load(std::memory_order_relaxed);

					if (t == b) {
						// last element in the queue
						const bool ret =
								m_top.compare_exchange_strong(t, t + 1, std::memory_order_seq_cst, std::memory_order_relaxed);
						m_bottom.store(b + 1, std::memory_order_relaxed);
						jobHandle = JobHandle(jobHandleValue);
						return ret; // false = failed race, don't use jobHandle; true = found a result
					}
				} else {
					// empty queue
					m_bottom.store(b + 1, std::memory_order_relaxed);
					return false; // false = empty, don't use jobHandle
				}

				jobHandle = JobHandle(jobHandleValue);

				// Make sure an invalid handle is not returned for true
				GAIA_ASSERT(jobHandle != (JobHandle)JobNull_t{});

				return true; // true = found a result
			}

			//! Tries stealing a job from the queue. LIFO.
			//! \return True if the job was stolen. False otherwise (e.g. there are no jobs).
			GAIA_NODISCARD bool try_steal(JobHandle& jobHandle) {
				GAIA_PROF_SCOPE(JobQueue::try_steal);

				uint32_t t = m_top.load(std::memory_order_acquire);

				std::atomic_thread_fence(std::memory_order_seq_cst);

				const uint32_t b = m_bottom.load(std::memory_order_acquire);
				if (t >= b)
					return false; // false = empty, don't use jobHandle

				uint32_t jobHandleValue = m_buffer[t & MASK].load(std::memory_order_relaxed);

				// We fail if concurrent pop()/steal() operation changed the current top
				const bool ret = m_top.compare_exchange_strong(t, t + 1, std::memory_order_seq_cst, std::memory_order_relaxed);
				jobHandle = JobHandle(jobHandleValue);
				return ret; // false = failed race, don't use jobHandle; true = found a result
			}
		};
	} // namespace mt
} // namespace gaia