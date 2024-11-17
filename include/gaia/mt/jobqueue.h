#pragma once

#include "../config/config.h"

#include <atomic>
#include <mutex>

#include "../cnt/sarray.h"
#include "../cnt/sringbuffer.h"
#include "../config/profiler.h"
#include "../core/utility.h"
#include "jobhandle.h"

namespace gaia {
	namespace mt {
		//! Lock-less job stealing queue. FIFO, fixed size. Inspired heavily by:
		//! http://www.dre.vanderbilt.edu/~schmidt/PDF/work-stealing-dequeue.pdf
		template <const uint32_t N = 1 << 12>
		class JobQueue {
			static_assert(N >= 2);
			static_assert((N & (N - 1)) == 0, "Extent of JobQueue must be a power of 2");
			static constexpr uint32_t MASK = N - 1;

			// MSVC might warn about applying additional padding around alignas usage.
			// This is perfectly fine but can cause builds with warning-as-error turned on to fail.
			GAIA_MSVC_WARNING_PUSH()
			GAIA_MSVC_WARNING_DISABLE(4324)

			static_assert(sizeof(std::atomic_uint32_t) == sizeof(JobHandle));
			cnt::sarray<std::atomic_uint32_t, N> m_buffer;
			alignas(GAIA_CACHELINE_SIZE) std::atomic_uint32_t m_bottom;
			alignas(GAIA_CACHELINE_SIZE) std::atomic_uint32_t m_top;

			GAIA_MSVC_WARNING_POP()

		public:
			JobQueue() {
				clear();
			}

			~JobQueue() = default;
			JobQueue(const JobQueue&) = default;
			JobQueue& operator=(const JobQueue&) = default;
			JobQueue(JobQueue&&) noexcept = default;
			JobQueue& operator=(JobQueue&&) noexcept = default;

			void clear() {
				m_bottom.store(0);
				m_top.store(0);
				for (auto& val: m_buffer)
					val.store(((JobHandle)JobNull_t()).value());
			}

			//! Checks if there are any items in the queue.
			//! \return True if the queue is empty. False otherwise.
			bool empty() const {
				GAIA_PROF_SCOPE(JobQueue::empty);

				const uint32_t b = m_bottom.load(std::memory_order_relaxed);
				const uint32_t t = m_top.load(std::memory_order_relaxed);
				return int32_t(b - t) <= 0; // b<=t, but handles overflows, too
			}

			//! Tries adding a job to the queue. FIFO.
			//! \return True if the job was added. False otherwise (e.g. maximum capacity has been reached).
			GAIA_NODISCARD bool try_push(JobHandle jobHandle) {
				GAIA_PROF_SCOPE(JobQueue::try_push);

				const uint32_t b = m_bottom.load(std::memory_order_relaxed);
				const uint32_t t = m_top.load(std::memory_order_acquire);
				const uint32_t used = b - t;
				if (used > MASK)
					return false;

				m_buffer[b & MASK].store(jobHandle.value(), std::memory_order_relaxed);
				// Make sure the handle is written before we update the bottom
				std::atomic_thread_fence(std::memory_order_release);
				m_bottom.store(b + 1, std::memory_order_relaxed);

				return true;
			}

			//! Tries adding a job to the queue. FIFO.
			//! \return The number of handles that were successfully added.
			GAIA_NODISCARD uint32_t try_push(std::span<JobHandle> jobHandles) {
				GAIA_PROF_SCOPE(JobQueue::try_push);

				const uint32_t cnt = (uint32_t)jobHandles.size();
				uint32_t b = m_bottom.load(std::memory_order_relaxed);
				const uint32_t t = m_top.load(std::memory_order_acquire);
				const uint32_t used = b - t;
				const uint32_t free = (MASK + 1) - used;
				const uint32_t freeFinal = core::get_min(cnt, free);

				for (uint32_t i = 0; i < freeFinal; i++, b++)
					m_buffer[b & MASK].store(jobHandles[i].value(), std::memory_order_relaxed);
				// Make sure handles are written before we update the bottom
				std::atomic_thread_fence(std::memory_order_release);
				m_bottom.store(b, std::memory_order_relaxed);

				return freeFinal;
			}

			//! Tries retrieving a job to the queue. FIFO.
			//! \return True if the job was retrieved. False otherwise (e.g. there are no jobs).
			GAIA_NODISCARD bool try_pop(JobHandle& jobHandle) {
				GAIA_PROF_SCOPE(JobQueue::try_pop);

				uint32_t jobHandleValue = ((JobHandle)JobNull_t{}).value();

				const uint32_t b = m_bottom.load(std::memory_order_relaxed) - 1;
				m_bottom.store(b, std::memory_order_relaxed);
				std::atomic_thread_fence(std::memory_order_seq_cst);
				uint32_t t = m_top.load(std::memory_order_relaxed);

				if (int(t - b) <= 0) { // t <= b, but handles overflows, too
					// non-empty queue
					jobHandleValue = m_buffer[b & MASK].load(std::memory_order_relaxed);

					if (t == b) {
						// last element in the queue
						const bool ret =
								m_top.compare_exchange_strong(t, t + 1, std::memory_order_seq_cst, std::memory_order_relaxed);
						m_bottom.store(b + 1, std::memory_order_relaxed);
						jobHandle = JobHandle(jobHandleValue);
						GAIA_ASSERT(jobHandle != (JobHandle)JobNull_t{});
						return ret; // false = failed race, don't use jobHandle; true = found a result
					}

					jobHandle = JobHandle(jobHandleValue);
					GAIA_ASSERT(jobHandle != (JobHandle)JobNull_t{});
					return true;
				}

				// empty queue
				m_bottom.store(b + 1, std::memory_order_relaxed);
				return false; // false = empty, don't use jobHandle
			}

			//! Tries stealing a job from the queue. LIFO.
			//! \return True if the job was stolen. False otherwise (e.g. there are no jobs).
			GAIA_NODISCARD bool try_steal(JobHandle& jobHandle) {
				GAIA_PROF_SCOPE(JobQueue::try_steal);

				uint32_t t = m_top.load(std::memory_order_acquire);
				std::atomic_thread_fence(std::memory_order_seq_cst);
				const uint32_t b = m_bottom.load(std::memory_order_acquire);

				if (int(b - t) <= 0) { // t >= b, but handles overflows, too
					jobHandle = (JobHandle)JobNull_t{};
					return true; // true + JobNull = empty, don't use jobHandle
				}

				const uint32_t jobHandleValue = m_buffer[t & MASK].load(std::memory_order_relaxed);

				// We fail if concurrent pop()/steal() operation changed the current top
				const bool ret = m_top.compare_exchange_strong(t, t + 1, std::memory_order_seq_cst, std::memory_order_relaxed);
				jobHandle = JobHandle(jobHandleValue);
				GAIA_ASSERT(jobHandle != (JobHandle)JobNull_t{});
				return ret; // false = failed race, don't use jobHandle; true = found a result
			}
		};

		//! Multi-producer-multi-consumer queue. FIFO, fixed size. Inspired heavily by:
		//! http://www.1024cores.net/home/lock-free-algorithms/queues/bounded-mpmc-queue
		template <class T, const uint32_t N = 1 << 12>
		class MpmcQueue {
			static_assert(N >= 2);
			static_assert((N & (N - 1)) == 0, "Extent of MpmcQueue must be a power of 2");
			static constexpr uint32_t MASK = N - 1;

			struct Node {
				std::atomic_uint32_t sequence{};
				T item;
			};
			using view_policy = mem::data_view_policy_aos<Node>;

			static constexpr uint32_t extent = N;
			static constexpr uint32_t allocated_bytes = view_policy::get_min_byte_size(0, N);

			// MSVC might warn about applying additional padding to an instance of StackAllocator.
			// This is perfectly fine, but might make builds with warning-as-error turned on to fail.
			GAIA_MSVC_WARNING_PUSH()
			GAIA_MSVC_WARNING_DISABLE(4324)

			mem::raw_data_holder<Node, allocated_bytes> m_data;
			alignas(GAIA_CACHELINE_SIZE) std::atomic_uint32_t m_pushPos;
			alignas(GAIA_CACHELINE_SIZE) std::atomic_uint32_t m_popPos;

			GAIA_MSVC_WARNING_POP()

		public:
			MpmcQueue() {
				init();
			}
			~MpmcQueue() {
				free();
			}

			MpmcQueue(MpmcQueue&&) = delete;
			MpmcQueue(const MpmcQueue&) = delete;
			MpmcQueue& operator=(MpmcQueue&&) = delete;
			MpmcQueue& operator=(const MpmcQueue&) = delete;

		private:
			GAIA_NODISCARD constexpr Node* data() noexcept {
				return GAIA_ACC((Node*)&m_data[0]);
			}

			GAIA_NODISCARD constexpr const Node* data() const noexcept {
				return GAIA_ACC((const Node*)&m_data[0]);
			}

			void init() {
				Node* pNodes = data();

				GAIA_FOR(extent) {
					Node* pNode = &pNodes[i];
					core::call_ctor(&pNode->sequence, i);
				}

				m_pushPos.store(0, std::memory_order_relaxed);
				m_popPos.store(0, std::memory_order_relaxed);
			}

			void free() {
				Node* pNodes = data();

				uint32_t enqPos = m_pushPos.load(std::memory_order_relaxed);
				uint32_t deqPos = m_popPos.load(std::memory_order_relaxed);
				for (uint32_t pos = deqPos; pos != enqPos; ++pos) {
					Node* pNode = &pNodes[pos & MASK];
					if (pNode->sequence.load(std::memory_order_relaxed) == pos + 1)
						pNode->item.~T();
				}

				GAIA_FOR(extent) {
					Node* pNode = &pNodes[i];
					core::call_dtor(&pNode->sequence);
				}
			}

		public:
			//! Checks if there are any items in the queue.
			//! \return True if the queue is empty. False otherwise.
			bool empty() const {
				GAIA_PROF_SCOPE(MpmcQueue::empty);

				const uint32_t pos = m_popPos.load(std::memory_order_relaxed);
				const auto* pNode = &data()[pos & MASK];
				const uint32_t seq = pNode->sequence.load(std::memory_order_acquire);
				return pos >= seq;
			}

			//! Tries to push an item onto the queue.
			//! \param item Item that will be moved onto the queue if possible.
			//! \return True if an item was popped. False otherwise (aka full).
			template <typename TT>
			bool try_push(TT&& item) {
				GAIA_PROF_SCOPE(MpmcQueue::try_push);

				Node* pNodes = data();

				Node* pNode = nullptr;
				uint32_t pos = m_pushPos.load(std::memory_order_relaxed);
				while (true) {
					pNode = &pNodes[pos & MASK];
					uint32_t seq = pNode->sequence.load(std::memory_order_acquire);
					int32_t diff = int32_t(seq) - int32_t(pos);
					if (diff == 0) {
						if (m_pushPos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
							break;
					} else if (diff < 0) {
						// The queue is full, we can't push
						return false;
					} else {
						pos = m_pushPos.load(std::memory_order_relaxed);
					}
				}

				core::call_ctor(&pNode->item, GAIA_FWD(item));
				pNode->sequence.store(pos + 1, std::memory_order_release);
				return true;
			}

			//! Tries to pop an item from the queue.
			//! \param[out] item Destination to which the output will be moved if possible.
			//! \return True if an item was popped. False otherwise (aka empty).
			bool try_pop(T& item) {
				GAIA_PROF_SCOPE(MpmcQueue::try_pop);

				Node* pNodes = data();

				Node* pNode = nullptr;
				uint32_t pos = m_popPos.load(std::memory_order_relaxed);
				while (true) {
					pNode = &pNodes[pos & MASK];
					uint32_t seq = pNode->sequence.load(std::memory_order_acquire);
					int32_t diff = int32_t(seq) - int32_t(pos + 1);
					if (diff == 0) {
						if (m_popPos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
							break;
					} else if (diff < 0) {
						// The queue is empty, we can't pop
						return false;
					} else {
						pos = m_popPos.load(std::memory_order_relaxed);
					}
				}

				item = GAIA_MOV(pNode->item);
				core::call_dtor(&pNode->item);
				pNode->sequence.store(pos + MASK + 1, std::memory_order_release);
				return true;
			}
		};
	} // namespace mt
} // namespace gaia