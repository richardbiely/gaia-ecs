#pragma once
#include "../config/config.h"

#include <cstdint>
#include <cstring>
#include <stdlib.h>
#include <type_traits>
#include <utility>

#include "../config/profiler.h"

#if GAIA_PLATFORM_WINDOWS
	#define GAIA_MEM_ALLC(size) ::malloc(size)
	#define GAIA_MEM_FREE(ptr) ::free(ptr)

	// Clang with MSVC codegen needs some remapping
	#if !defined(aligned_alloc)
		#define GAIA_MEM_ALLC_A(size, alig) ::_aligned_malloc(size, alig)
		#define GAIA_MEM_FREE_A(ptr) ::_aligned_free(ptr)
	#else
		#define GAIA_MEM_ALLC_A(size, alig) ::aligned_alloc(alig, size)
		#define GAIA_MEM_FREE_A(ptr) ::aligned_free(ptr)
	#endif
#else
	#define GAIA_MEM_ALLC(size) ::malloc(size)
	#define GAIA_MEM_ALLC_A(size, alig) ::aligned_alloc(alig, size)
	#define GAIA_MEM_FREE(ptr) ::free(ptr)
	#define GAIA_MEM_FREE_A(ptr) ::free(ptr)
#endif

namespace gaia {
	namespace mem {
		class DefaultAllocator {
		public:
			void* alloc(size_t size) {
				GAIA_ASSERT(size > 0);

				void* ptr = GAIA_MEM_ALLC(size);
				GAIA_ASSERT(ptr != nullptr);
				GAIA_PROF_ALLOC(ptr, size);
				return ptr;
			}

			void* alloc([[maybe_unused]] const char* name, size_t size) {
				GAIA_ASSERT(size > 0);

				void* ptr = GAIA_MEM_ALLC(size);
				GAIA_ASSERT(ptr != nullptr);
				GAIA_PROF_ALLOC2(ptr, size, name);
				return ptr;
			}

			void* alloc_alig(size_t size, size_t alig) {
				GAIA_ASSERT(size > 0);
				GAIA_ASSERT(alig > 0);

				// Make sure size is a multiple of the alignment
				size = (size + alig - 1) & ~(alig - 1);
				void* ptr = GAIA_MEM_ALLC_A(size, alig);
				GAIA_ASSERT(ptr != nullptr);
				GAIA_PROF_ALLOC(ptr, size);
				return ptr;
			}

			void* alloc_alig([[maybe_unused]] const char* name, size_t size, size_t alig) {
				GAIA_ASSERT(size > 0);
				GAIA_ASSERT(alig > 0);

				// Make sure size is a multiple of the alignment
				size = (size + alig - 1) & ~(alig - 1);
				void* ptr = GAIA_MEM_ALLC_A(size, alig);
				GAIA_ASSERT(ptr != nullptr);
				GAIA_PROF_ALLOC2(ptr, size, name);
				return ptr;
			}

			void free(void* ptr) {
				GAIA_ASSERT(ptr != nullptr);

				GAIA_MEM_FREE(ptr);
				GAIA_PROF_FREE(ptr);
			}

			void free([[maybe_unused]] const char* name, void* ptr) {
				GAIA_ASSERT(ptr != nullptr);

				GAIA_MEM_FREE(ptr);
				GAIA_PROF_FREE2(ptr, name);
			}

			void free_alig(void* ptr) {
				GAIA_ASSERT(ptr != nullptr);

				GAIA_MEM_FREE_A(ptr);
				GAIA_PROF_FREE(ptr);
			}

			void free_alig([[maybe_unused]] const char* name, void* ptr) {
				GAIA_ASSERT(ptr != nullptr);

				GAIA_MEM_FREE_A(ptr);
				GAIA_PROF_FREE2(ptr, name);
			}
		};

		struct DefaultAllocatorAdaptor {
			static DefaultAllocator& get() {
				static DefaultAllocator s_allocator;
				return s_allocator;
			}
		};

		struct AllocHelper {
			template <typename T, typename Adaptor = DefaultAllocatorAdaptor>
			static T* alloc(uint32_t cnt = 1) {
				return (T*)Adaptor::get().alloc(sizeof(T) * cnt);
			}
			template <typename T, typename Adaptor = DefaultAllocatorAdaptor>
			static T* alloc(const char* name, uint32_t cnt = 1) {
				return (T*)Adaptor::get().alloc(name, sizeof(T) * cnt);
			}
			template <typename T, typename Adaptor = DefaultAllocatorAdaptor>
			static T* alloc_alig(size_t alig, uint32_t cnt = 1) {
				return (T*)Adaptor::get().alloc_alig(sizeof(T) * cnt, alig);
			}
			template <typename T, typename Adaptor = DefaultAllocatorAdaptor>
			static T* alloc_alig(const char* name, size_t alig, uint32_t cnt = 1) {
				return (T*)Adaptor::get().alloc_alig(name, sizeof(T) * cnt, alig);
			}
			template <typename Adaptor = DefaultAllocatorAdaptor>
			static void free(void* ptr) {
				Adaptor::get().free(ptr);
			}
			template <typename Adaptor = DefaultAllocatorAdaptor>
			static void free(const char* name, void* ptr) {
				Adaptor::get().free(name, ptr);
			}
			template <typename Adaptor = DefaultAllocatorAdaptor>
			static void free_alig(void* ptr) {
				Adaptor::get().free_alig(ptr);
			}
			template <typename Adaptor = DefaultAllocatorAdaptor>
			static void free_alig(const char* name, void* ptr) {
				Adaptor::get().free_alig(name, ptr);
			}
		};

		//! Allocate \param size bytes of memory using the default allocator.
		inline void* mem_alloc(size_t size) {
			return DefaultAllocatorAdaptor::get().alloc(size);
		}

		//! Allocate \param size bytes of memory using the default allocator.
		inline void* mem_alloc(const char* name, size_t size) {
			return DefaultAllocatorAdaptor::get().alloc(name, size);
		}

		//! Allocate \param size bytes of memory using the default allocator.
		//! The memory is aligned to \param alig boundary.
		inline void* mem_alloc_alig(size_t size, size_t alig) {
			return DefaultAllocatorAdaptor::get().alloc_alig(size, alig);
		}

		//! Allocate \param size bytes of memory using the default allocator.
		//! The memory is aligned to \param alig boundary.
		inline void* mem_alloc_alig(const char* name, size_t size, size_t alig) {
			return DefaultAllocatorAdaptor::get().alloc_alig(name, size, alig);
		}

		//! Release memory allocated by the default allocator.
		inline void mem_free(void* ptr) {
			return DefaultAllocatorAdaptor::get().free(ptr);
		}

		//! Release memory allocated by the default allocator.
		inline void mem_free(const char* name, void* ptr) {
			return DefaultAllocatorAdaptor::get().free(name, ptr);
		}

		//! Release aligned memory allocated by the default allocator.
		inline void mem_free_alig(void* ptr) {
			return DefaultAllocatorAdaptor::get().free_alig(ptr);
		}

		//! Release aligned memory allocated by the default allocator.
		inline void mem_free_alig(const char* name, void* ptr) {
			return DefaultAllocatorAdaptor::get().free_alig(name, ptr);
		}

		//! Align a number to the requested byte alignment
		//! \param num Number to align
		//! \param alignment Requested alignment
		//! \return Aligned number
		template <typename T, typename V>
		constexpr T align(T num, V alignment) {
			return alignment == 0 ? num : ((num + (alignment - 1)) / alignment) * alignment;
		}

		//! Align a number to the requested byte alignment
		//! \tparam alignment Requested alignment in bytes
		//! \param num Number to align
		//! return Aligned number
		template <size_t alignment, typename T>
		constexpr T align(T num) {
			return ((num + (alignment - 1)) & ~(alignment - 1));
		}

		//! Returns the padding
		//! \param num Number to align
		//! \param alignment Requested alignment
		//! \return Padding in bytes
		template <typename T, typename V>
		constexpr uint32_t padding(T num, V alignment) {
			return (uint32_t)(align(num, alignment) - num);
		}

		//! Returns the padding
		//! \tparam alignment Requested alignment in bytes
		//! \param num Number to align
		//! return Aligned number
		template <size_t alignment, typename T>
		constexpr uint32_t padding(T num) {
			return (uint32_t)(align<alignment>(num) - num);
		}

		//! Convert form type \tparam Src to type \tparam Dst without causing an undefined behavior
		template <typename Dst, typename Src>
		Dst bit_cast(const Src& src) {
			static_assert(sizeof(Dst) == sizeof(Src));
			static_assert(std::is_trivially_copyable_v<Src>);
			static_assert(std::is_trivially_copyable_v<Dst>);

			// int i = {};
			// float f = *(*float)&i; // undefined behavior
			// memcpy(&f, &i, sizeof(float)); // okay
			Dst dst;
			memmove((void*)&dst, (const void*)&src, sizeof(Dst));
			return dst;
		}

		//! Pointer wrapper for writing memory in defined way (not causing undefined behavior)
		template <typename T>
		class unaligned_ref {
			void* m_p;

		public:
			unaligned_ref(void* p): m_p(p) {}

			unaligned_ref& operator=(const T& value) {
				memmove(m_p, (const void*)&value, sizeof(T));
				return *this;
			}

			operator T() const {
				T tmp;
				memmove((void*)&tmp, (const void*)m_p, sizeof(T));
				return tmp;
			}
		};
	} // namespace mem
} // namespace gaia
