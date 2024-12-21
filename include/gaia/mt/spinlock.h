#pragma once
#include "../config/config.h"

#include <atomic>

namespace gaia {
	namespace mt {
		class SpinLock final {
			std::atomic_int32_t m_value{};

		public:
			SpinLock() = default;
			SpinLock(const SpinLock&) = delete;
			SpinLock& operator=(const SpinLock&) = delete;

			bool try_lock() {
				// Attempt to acquire the lock without waiting
				return 0 == m_value.exchange(1, std::memory_order_acquire);
			}

			void lock() {
				while (true) {
					// The value has been changed, we successfully entered the lock
					if (0 == m_value.exchange(1, std::memory_order_acquire))
						break;

					// Yield until unlocked
					while (m_value.load(std::memory_order_relaxed) != 0)
						GAIA_YIELD_CPU;
				}
			}

			void unlock() {
				// Release the lock
				m_value.store(0, std::memory_order_release);
			}
		};
	} // namespace mt
} // namespace gaia