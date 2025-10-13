#pragma once

#include "../config/config.h"

#include "semaphore.h"
#include <atomic>

namespace gaia {
	namespace mt {
		//! An optimized version of Semaphore that avoids expensive system calls when the counter is greater than 0.
		class GAIA_API SemaphoreFast final {
			Semaphore m_sem;
			std::atomic_int32_t m_cnt;

			SemaphoreFast(SemaphoreFast&&) = delete;
			SemaphoreFast(const SemaphoreFast&) = delete;
			SemaphoreFast& operator=(SemaphoreFast&&) = delete;
			SemaphoreFast& operator=(const SemaphoreFast&) = delete;

		public:
			explicit SemaphoreFast(int32_t count = 0): m_sem(count), m_cnt(0) {}
			~SemaphoreFast() = default;

			//! Increments semaphore count by the specified amount.
			void release(int32_t count = 1) {
				const int32_t prevCount = m_cnt.fetch_add(count, std::memory_order_release);
				int32_t toRelease = -prevCount;
				if (count < toRelease)
					toRelease = count;

				if (toRelease > 0)
					m_sem.release(toRelease);
			}

			//! Decrements semaphore count by 1.
			//! If the count is already 0, it waits indefinitely until semaphore count is incremented,
			//! then decrements and returns. Returns false when an error occurs, otherwise returns true.
			bool wait() {
				const int32_t oldCount = m_cnt.fetch_sub(1, std::memory_order_acquire);
				bool result = true;
				if (oldCount <= 0)
					result = m_sem.wait();

				return result;
			}
		};
	} // namespace mt
} // namespace gaia