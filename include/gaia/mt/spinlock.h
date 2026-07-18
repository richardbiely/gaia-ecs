#pragma once
#include "gaia/config/config.h"

#include <atomic>

namespace gaia {
	namespace mt {
		//! Non-recursive spin lock backed by an atomic flag.
		class GAIA_API SpinLock final {
			std::atomic_int32_t m_value{};

		public:
			SpinLock() = default;
			~SpinLock() = default;
			SpinLock(const SpinLock&) = delete;
			SpinLock& operator=(const SpinLock&) = delete;

			//! Attempts to acquire the lock without waiting.
			//! \return True when the lock was acquired.
			bool try_lock() {
				// Attempt to acquire the lock without waiting
				return 0 == m_value.exchange(1, std::memory_order_acquire);
			}

			//! Spins until the lock is acquired.
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

			//! Releases the lock.
			void unlock() {
				// Release the lock
				m_value.store(0, std::memory_order_release);
			}
		};
	} // namespace mt
} // namespace gaia