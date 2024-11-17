#pragma once
#include "../config/config.h"
#include "../config/profiler.h"

#include <atomic>
#include <mutex>

#include "event.h"

namespace gaia {
	namespace mt {
		namespace detail {
			inline static constexpr uint32_t WaitMaskAll = 0x7FFFFFFF;
			inline static constexpr uint32_t WaitMaskAny = ~0u;

			struct FutexWaitNode {
				FutexWaitNode* pNext = nullptr;
				const std::atomic_uint32_t* pFutexValue = nullptr;
				uint32_t waitMask = WaitMaskAny;
				Event evt;
			};

			struct FutexBucket {
				GAIA_PROF_MUTEX(std::mutex, mtx);
				FutexWaitNode* pFirst = nullptr;

				// Since there shouldn't be that many threads waiting at any one time, this seems like a good
				// number of hash table buckets. Making it prime number for better spread.
				static constexpr uint32_t BUCKET_SIZE = 37;

				static FutexBucket& get(const std::atomic_uint32_t* pFutexValue) {
					static FutexBucket s_buckets[BUCKET_SIZE];
					return s_buckets[(uintptr_t(pFutexValue) >> 2) % BUCKET_SIZE];
				}
			};

			inline thread_local FutexWaitNode t_WaitNode;

		} // namespace detail

		//! An implementation of a simple futex (fast userspace mutex).
		//! Only wait and wake are implemented.
		//!
		//! The main advantage of futex is performance. It avoids kernel involvement in uncontended cases.
		//! When there’s no contention, futexes allow threads to lock and unlock in userspace without entering
		//! the kernel, making operations significantly faster and reducing context-switch overhead.
		//! Only when there is contention does a futex use the kernel to put threads to sleep and wake them up,
		//! resulting in a hybrid model that is more efficient than mutexes, which always require kernel calls.
		//!
		//! TODO: Consider using WaitOnAddress for Windows, futex call for Linux etc.
		//!       The current solution is platform-agnostic but platform-specific solutions might be more performant.
		struct Futex {
			enum class Result {
				//! Futex value didn't match the expected one
				Change,
				//! Futex woken up as a result of wake()
				WakeUp
			};

			//! \param pFutexValue Target futex
			//! \param expected Expected futex value
			//! \param wakeMask Mask of waiters to wait for
			static Result wait(const std::atomic_uint32_t* pFutexValue, uint32_t expected, uint32_t waitMask) {
				GAIA_PROF_SCOPE(futex::wait);

				GAIA_ASSERT(waitMask != 0);

				auto& bucket = detail::FutexBucket::get(pFutexValue);
				auto& node = detail::t_WaitNode;
				node.pFutexValue = pFutexValue;
				node.waitMask = waitMask;

				{
					auto& mtx = GAIA_PROF_EXTRACT_MUTEX(std::mutex, bucket.mtx);
					std::lock_guard lock(mtx);

					const uint32_t futexValue = pFutexValue->load(std::memory_order_relaxed);
					if (futexValue != expected)
						return Result::Change;

					node.pNext = bucket.pFirst;
					bucket.pFirst = &node;
				}

				node.evt.wait();
				return Result::WakeUp;
			}

			//! \param pFutexValue Target futex
			//! \param wakeCount How many waiters are supposed to make up
			//! \param wakeMask Mask of callers to wake
			static uint32_t
			wake(const std::atomic_uint32_t* pFutexValue, uint32_t wakeCount, uint32_t wakeMask = detail::WaitMaskAny) {
				GAIA_PROF_SCOPE(futex::wake);

				GAIA_ASSERT(wakeMask != 0);

				auto& bucket = detail::FutexBucket::get(pFutexValue);
				auto& mtx = GAIA_PROF_EXTRACT_MUTEX(std::mutex, bucket.mtx);
				std::lock_guard lock(mtx);

				uint32_t numAwoken = 0;
				auto** ppNode = &bucket.pFirst;
				for (auto* pNode = *ppNode; numAwoken < wakeCount && pNode != nullptr; pNode = *ppNode) {
					if (pNode->pFutexValue == pFutexValue && (pNode->waitMask & wakeMask) != 0) {
						++numAwoken;

						// Unlink the node
						*ppNode = pNode->pNext;
						pNode->pNext = nullptr;

						pNode->evt.set();
					} else {
						ppNode = &pNode->pNext;
					}
				}

				return numAwoken;
			}
		};
	} // namespace mt
} // namespace gaia