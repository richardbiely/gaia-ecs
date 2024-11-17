#pragma once
#include "../config/config.h"
#include "../config/profiler.h"

#define GAIA_USE_MT_STD 1
#if GAIA_USE_MT_STD
	#include <condition_variable>
	#include <mutex>
#else
	#include <pthread.h>
#endif

namespace gaia {
	namespace mt {
		class Event final {
#if GAIA_USE_MT_STD
			GAIA_PROF_MUTEX(std::mutex, m_mtx);
			std::condition_variable m_cv;
			bool m_set = false;
#else
			pthread_cond_t m_hCondHandle;
			pthread_mutex_t m_hMutexHandle;
			bool m_bReady;
			bool m_bManualReset;
#endif

		public:
#if !GAIA_USE_MT_STD
			Event(bool manualReset = false, bool initialState = false) {
				m_bManualReset = manualReset;
				m_bReady = false;

				int ret = pthread_mutex_init(&m_hMutexHandle, nullptr);
				GAIA_ASSERT(ret == 0);
				if (ret == 0) {
					ret = pthread_cond_init(&m_hCondHandle, nullptr);
					GAIA_ASSERT(ret == 0);
					if (ret == 0 && initialState) {
						set();
					}
				}

				(void)ret;
				// return (ret == 0);
			}

			~Event() {
				int ret = pthread_cond_destroy(&m_hCondHandle);
				GAIA_ASSERT(ret == 0);

				ret = pthread_mutex_destroy(&m_hMutexHandle);
				GAIA_ASSERT(ret == 0);
			}
#endif

			void set() {
#if GAIA_USE_MT_STD
				auto& mtx = GAIA_PROF_EXTRACT_MUTEX(std::mutex, m_mtx);
				std::unique_lock lock(mtx);
				m_set = true;
				m_cv.notify_one();
#else
				[[maybe_unused]] int ret = pthread_mutex_lock(&m_hMutexHandle);
				GAIA_ASSERT(ret == 0);
				m_bReady = true;

				// Depending on the event type, we either trigger everyone waiting or just one
				if (m_bManualReset) {
					ret = pthread_cond_broadcast(&m_hCondHandle);
					GAIA_ASSERT(ret == 0);
				} else {
					ret = pthread_cond_signal(&m_hCondHandle);
					GAIA_ASSERT(ret == 0);
				}

				ret = pthread_mutex_unlock(&m_hMutexHandle);
				GAIA_ASSERT(ret == 0);
#endif
			}

			void reset() {
#if GAIA_USE_MT_STD
				auto& mtx = GAIA_PROF_EXTRACT_MUTEX(std::mutex, m_mtx);
				std::unique_lock lock(mtx);
				m_set = false;
#else
				[[maybe_unused]] int ret = pthread_mutex_lock(&m_hMutexHandle);
				GAIA_ASSERT(ret == 0);
				m_bReady = false;
				ret = pthread_mutex_unlock(&m_hMutexHandle);
				GAIA_ASSERT(ret == 0);
#endif
			}

			GAIA_NODISCARD bool is_set() {
#if GAIA_USE_MT_STD
				auto& mtx = GAIA_PROF_EXTRACT_MUTEX(std::mutex, m_mtx);
				std::unique_lock lock(mtx);
				return m_set;
#else
				bool ready{};
				[[maybe_unused]] int ret = pthread_mutex_lock(&m_hMutexHandle);
				GAIA_ASSERT(ret == 0);
				ready = m_bReady;
				ret = pthread_mutex_unlock(&m_hMutexHandle);
				GAIA_ASSERT(ret == 0);
				return ready;
#endif
			}

			void wait() {
#if GAIA_USE_MT_STD
				auto& mtx = GAIA_PROF_EXTRACT_MUTEX(std::mutex, m_mtx);
				std::unique_lock lock(mtx);
				m_cv.wait(lock, [&] {
					return m_set;
				});
#else
				[[maybe_unused]] int ret{};
				auto wait = [&]() {
					if (!m_bReady) {
						do {
							ret = pthread_cond_wait(&m_hCondHandle, &m_hMutexHandle);
						} while (!ret && !m_bReady);

						GAIA_ASSERT(ret != EINVAL);
						if (!ret && !m_bManualReset)
							m_bReady = false;
					} else if (!m_bManualReset) {
						m_bReady = false;
						ret = 0;
					}

					return ret;
				};

				ret = pthread_mutex_lock(&m_hMutexHandle);
				GAIA_ASSERT(ret == 0);

				int res = wait(); // true: signaled, false: timeout or error
				(void)res;

				ret = pthread_mutex_unlock(&m_hMutexHandle);
				GAIA_ASSERT(ret == 0);
#endif
			}
		}; // namespace mt
	} // namespace mt
} // namespace gaia