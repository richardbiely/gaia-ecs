#pragma once

#include "../config/config.h"

#if GAIA_PLATFORM_WINDOWS
	#include <windows.h>
#elif GAIA_PLATFORM_APPLE
	#include <TargetConditionals.h>
	#include <dispatch/dispatch.h>
	#include <pthread.h>
#else
	#include <pthread.h>
	#include <semaphore.h>
#endif

namespace gaia {
	namespace mt {
		class Semaphore final {
#if GAIA_PLATFORM_WINDOWS
			void* m_handle;
#elif GAIA_PLATFORM_APPLE
			dispatch_semaphore_t m_handle;
#else
			sem_t m_handle;
#endif

			Semaphore(Semaphore&&) = delete;
			Semaphore(const Semaphore&) = delete;
			Semaphore& operator=(Semaphore&&) = delete;
			Semaphore& operator=(const Semaphore&) = delete;

			//! Initializes the semaphore.
			//! \param count Initial count on semaphore.
			void init(int32_t count) {
#if GAIA_PLATFORM_WINDOWS
				m_handle = (void*)::CreateSemaphoreExW(NULL, count, INT_MAX, NULL, 0, SEMAPHORE_ALL_ACCESS);
				GAIA_ASSERT(m_handle != NULL);
#elif GAIA_PLATFORM_APPLE
				m_handle = dispatch_semaphore_create(count);
				GAIA_ASSERT(m_handle != nullptr);
#else
				int ret = sem_init(&m_handle, 0, count);
				GAIA_ASSERT(ret == 0);
#endif
			}

			//! Destroys the semaphore.
			void done() {
#if GAIA_PLATFORM_WINDOWS
				if (m_handle != NULL) {
					::CloseHandle((HANDLE)m_handle);
					m_handle = NULL;
				}
#elif GAIA_PLATFORM_APPLE
				// NOTE: Dispatch objects are reference counted.
				//       They are automatically released when no longer used.
				// -> dispatch_release(m_handle);
#else
				int ret = sem_destroy(&m_handle);
				GAIA_ASSERT(ret == 0);
#endif
			}

		public:
			explicit Semaphore(int32_t count = 0) {
				init(count);
			}

			~Semaphore() {
				done();
			}

			//! Increments semaphore count by the specified amount.
			void release(int32_t count) {
				GAIA_ASSERT(count > 0);

#if GAIA_PLATFORM_WINDOWS
				[[maybe_unused]] LONG prev = 0;
				BOOL res = ::ReleaseSemaphore(m_handle, count, &prev);
				if (res == 0) {
					DWORD err = ::GetLastError();
					(void))err;
				}
#elif GAIA_PLATFORM_APPLE
				do {
					dispatch_semaphore_signal(m_handle);
				} while ((--count) != 0);
#else
				do {
					[[maybe_unused]] const auto ret = sem_post(&m_handle);
					GAIA_ASSERT(ret == 0);
				} while ((--count) != 0);
#endif
			}

			//! Decrements semaphore count by 1.
			//! If the count is already 0, it waits indefinitely until semaphore count is incremented,
			//! then decrements and returns. Returns false when an error occurs, otherwise returns true.
			bool wait() {
#if GAIA_PLATFORM_WINDOWS
				GAIA_ASSERT(m_handle != (void*)ERROR_INVALID_HANDLE);
				DWORD ret = ::WaitForSingleObject(m_handle, INFINITE);
				GAIA_ASSERT(ret == WAIT_OBJECT_0);
				return (ret == WAIT_OBJECT_0);
#elif GAIA_PLATFORM_APPLE
				const auto res = dispatch_semaphore_wait(m_handle, DISPATCH_TIME_FOREVER);
				GAIA_ASSERT(res == 0);
				return (res == 0);
#else
				int res;
				do {
					res = sem_wait(&m_handle);
				} while (res == -1 && errno == EINTR); // handle interrupts

				GAIA_ASSERT(res == 0);
				return (res == 0);
#endif
			}
		};
	} // namespace mt
} // namespace gaia