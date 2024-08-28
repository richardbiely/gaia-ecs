#pragma once

#include "../config/config.h"

#include "../cnt/sarray.h"
#include "../config/profiler.h"

#define JOB_QUEUE_USE_LOCKS 1
#if JOB_QUEUE_USE_LOCKS
	#include "../cnt/sringbuffer.h"
	#include <mutex>
#endif

#include "jobcommon.h"
#include "jobmanager.h"

namespace gaia {
	namespace mt {
		class JobQueue {
			//! The maximum number of jobs fitting in the queue at the same time
			static constexpr uint32_t N = 1 << 12;
#if !JOB_QUEUE_USE_LOCKS
			static constexpr uint32_t MASK = N - 1;
#endif

#if JOB_QUEUE_USE_LOCKS
			GAIA_PROF_MUTEX(std::mutex, m_bufferLock);
			cnt::sringbuffer<JobHandle, N> m_buffer;
#else
			cnt::sarray<JobHandle, N> m_buffer;
			std::atomic_uint32_t m_bottom{};
			std::atomic_uint32_t m_top{};
#endif

		public:
			//! Tries adding a job to the queue. FIFO.
			//! \return True if the job was added. False otherwise (e.g. maximum capacity has been reached).
			GAIA_NODISCARD bool try_push(JobHandle jobHandle) {
				GAIA_PROF_SCOPE(JobQueue::try_push);

#if JOB_QUEUE_USE_LOCKS
				std::scoped_lock lock(m_bufferLock);
				if (m_buffer.size() >= m_buffer.max_size())
					return false;
				m_buffer.push_back(jobHandle);
#else
				const uint32_t b = m_bottom.load(std::memory_order_acquire);

				if (b >= m_buffer.size())
					return false;

				m_buffer[b & MASK] = jobHandle;

				// Make sure the handle is written before we update the bottom
				std::atomic_thread_fence(std::memory_order_release);

				m_bottom.store(b + 1, std::memory_order_release);
#endif

				return true;
			}

			//! Tries retrieving a job to the queue. FIFO.
			//! \return True if the job was retrieved. False otherwise (e.g. there are no jobs).
			GAIA_NODISCARD bool try_pop(JobHandle& jobHandle) {
				GAIA_PROF_SCOPE(JobQueue::try_pop);

#if JOB_QUEUE_USE_LOCKS
				std::scoped_lock lock(m_bufferLock);
				if (m_buffer.empty())
					return false;

				m_buffer.pop_front(jobHandle);
#else
				uint32_t b = m_bottom.load(std::memory_order_acquire);
				if (b > 0) {
					b = b - 1;
					m_bottom.store(b, std::memory_order_release);
				}

				std::atomic_thread_fence(std::memory_order_release);

				uint32_t t = m_top.load(std::memory_order_acquire);

				// Queue already empty
				if (t > b) {
					m_bottom.store(t, std::memory_order_release);
					return false;
				}

				jobHandle = m_buffer[b & MASK];

				// The last item in the queue
				if (t == b) {
					if (t == 0) {
						return false; // Queue is empty, nothing to pop
					}
					// Should multiple thread fight for the last item the atomic
					// CAS ensures this last item is extracted only once.

					uint32_t expectedTop = t;
					const uint32_t nextTop = t + 1;
					const uint32_t desiredTop = nextTop;

					bool ret = m_top.compare_exchange_strong(expectedTop, desiredTop, std::memory_order_acq_rel);
					m_bottom.store(nextTop, std::memory_order_release);
					return ret;
				}
#endif

				return true;
			}

			//! Tries stealing a job from the queue. LIFO.
			//! \return True if the job was stolen. False otherwise (e.g. there are no jobs).
			GAIA_NODISCARD bool try_steal(JobHandle& jobHandle) {
				GAIA_PROF_SCOPE(JobQueue::try_steal);

#if JOB_QUEUE_USE_LOCKS
				std::scoped_lock lock(m_bufferLock);
				if (m_buffer.empty())
					return false;

				m_buffer.pop_back(jobHandle);
#else
				uint32_t t = m_top.load(std::memory_order_acquire);

				std::atomic_thread_fence(std::memory_order_release);

				uint32_t b = m_bottom.load(std::memory_order_acquire);

				// Return false when empty
				if (t >= b)
					return false;

				jobHandle = m_buffer[t & MASK];

				const uint32_t tNext = t + 1;
				uint32_t tDesired = tNext;
				// We fail if concurrent pop()/steal() operation changed the current top
				if (!m_top.compare_exchange_weak(t, tDesired, std::memory_order_acq_rel))
					return false;
#endif

				return true;
			}
		};
	} // namespace mt
} // namespace gaia