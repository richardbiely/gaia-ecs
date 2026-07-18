#pragma once
#include "gaia/config/config.h"
#include "gaia/config/profiler.h"

#include <cstdint>
#include <cstring>
#include <stdlib.h>
#include <type_traits>
#include <utility>

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
		//! Stateless allocator backed by the platform heap.
		struct DefaultAllocator {
			//! Allocates an unaligned memory region.
			//! \param size Number of bytes to allocate.
			//! \return Pointer to the allocated region.
			GAIA_NODISCARD static void* alloc(size_t size) {
				GAIA_ASSERT(size > 0);

				void* ptr = GAIA_MEM_ALLC(size);
				GAIA_ASSERT(ptr != nullptr);
				GAIA_PROF_ALLOC(ptr, size);
				return ptr;
			}

			//! Allocates a named unaligned memory region.
			//! \param name Allocation name reported to the profiler.
			//! \param size Number of bytes to allocate.
			//! \return Pointer to the allocated region.
			GAIA_NODISCARD static void* alloc([[maybe_unused]] const char* name, size_t size) {
				GAIA_ASSERT(size > 0);

				void* ptr = GAIA_MEM_ALLC(size);
				GAIA_ASSERT(ptr != nullptr);
				GAIA_PROF_ALLOC2(ptr, size, name);
				return ptr;
			}

			//! Allocates an aligned memory region.
			//! \param size Number of bytes to allocate.
			//! \param alig Required byte alignment.
			//! \return Pointer to the allocated region.
			GAIA_NODISCARD static void* alloc_alig(size_t size, size_t alig) {
				GAIA_ASSERT(size > 0);
				GAIA_ASSERT(alig > 0);

				// Make sure size is a multiple of the alignment
				size = (size + alig - 1) & ~(alig - 1);
				void* ptr = GAIA_MEM_ALLC_A(size, alig);
				GAIA_ASSERT(ptr != nullptr);
				GAIA_PROF_ALLOC(ptr, size);
				return ptr;
			}

			//! Allocates a named aligned memory region.
			//! \param name Allocation name reported to the profiler.
			//! \param size Number of bytes to allocate.
			//! \param alig Required byte alignment.
			//! \return Pointer to the allocated region.
			GAIA_NODISCARD static void* alloc_alig([[maybe_unused]] const char* name, size_t size, size_t alig) {
				GAIA_ASSERT(size > 0);
				GAIA_ASSERT(alig > 0);

				// Make sure size is a multiple of the alignment
				size = (size + alig - 1) & ~(alig - 1);
				void* ptr = GAIA_MEM_ALLC_A(size, alig);
				GAIA_ASSERT(ptr != nullptr);
				GAIA_PROF_ALLOC2(ptr, size, name);
				return ptr;
			}

			//! Releases an unaligned memory region.
			//! \param ptr Pointer returned by alloc().
			static void free(void* ptr) {
				GAIA_ASSERT(ptr != nullptr);

				GAIA_MEM_FREE(ptr);
				GAIA_PROF_FREE(ptr);
			}

			//! Releases a named unaligned memory region.
			//! \param name Allocation name reported to the profiler.
			//! \param ptr Pointer returned by alloc().
			static void free([[maybe_unused]] const char* name, void* ptr) {
				GAIA_ASSERT(ptr != nullptr);

				GAIA_MEM_FREE(ptr);
				GAIA_PROF_FREE2(ptr, name);
			}

			//! Releases an aligned memory region.
			//! \param ptr Pointer returned by alloc_alig().
			static void free_alig(void* ptr) {
				GAIA_ASSERT(ptr != nullptr);

				GAIA_MEM_FREE_A(ptr);
				GAIA_PROF_FREE(ptr);
			}

			//! Releases a named aligned memory region.
			//! \param name Allocation name reported to the profiler.
			//! \param ptr Pointer returned by alloc_alig().
			static void free_alig([[maybe_unused]] const char* name, void* ptr) {
				GAIA_ASSERT(ptr != nullptr);

				GAIA_MEM_FREE_A(ptr);
				GAIA_PROF_FREE2(ptr, name);
			}
		};

		//! Provides the shared default allocator instance expected by AllocHelper.
		struct DefaultAllocatorAdaptor {
			//! Returns the process-local default allocator.
			//! \return Default allocator instance.
			static DefaultAllocator& get() {
				static DefaultAllocator s_allocator;
				return s_allocator;
			}
		};

		//! Typed allocation facade for allocator adaptors.
		struct AllocHelper {
			//! Allocates storage for objects without constructing them.
			//! \tparam T Object type.
			//! \tparam Adaptor Allocator adaptor type.
			//! \param cnt Number of objects to reserve storage for.
			//! \return Typed pointer to the allocated storage.
			template <typename T, typename Adaptor = DefaultAllocatorAdaptor>
			GAIA_NODISCARD static T* alloc(uint32_t cnt = 1) {
				return (T*)Adaptor::get().alloc(sizeof(T) * cnt);
			}
			//! Allocates named storage for objects without constructing them.
			//! \tparam T Object type.
			//! \tparam Adaptor Allocator adaptor type.
			//! \param name Allocation name reported to the profiler.
			//! \param cnt Number of objects to reserve storage for.
			//! \return Typed pointer to the allocated storage.
			template <typename T, typename Adaptor = DefaultAllocatorAdaptor>
			GAIA_NODISCARD static T* alloc(const char* name, uint32_t cnt = 1) {
				return (T*)Adaptor::get().alloc(name, sizeof(T) * cnt);
			}
			//! Allocates aligned storage for objects without constructing them.
			//! \tparam T Object type.
			//! \tparam Adaptor Allocator adaptor type.
			//! \param alig Required byte alignment.
			//! \param cnt Number of objects to reserve storage for.
			//! \return Typed pointer to the allocated storage.
			template <typename T, typename Adaptor = DefaultAllocatorAdaptor>
			GAIA_NODISCARD static T* alloc_alig(size_t alig, uint32_t cnt = 1) {
				return (T*)Adaptor::get().alloc_alig(sizeof(T) * cnt, alig);
			}
			//! Allocates named aligned storage for objects without constructing them.
			//! \tparam T Object type.
			//! \tparam Adaptor Allocator adaptor type.
			//! \param name Allocation name reported to the profiler.
			//! \param alig Required byte alignment.
			//! \param cnt Number of objects to reserve storage for.
			//! \return Typed pointer to the allocated storage.
			template <typename T, typename Adaptor = DefaultAllocatorAdaptor>
			GAIA_NODISCARD static T* alloc_alig(const char* name, size_t alig, uint32_t cnt = 1) {
				return (T*)Adaptor::get().alloc_alig(name, sizeof(T) * cnt, alig);
			}
			//! Releases storage allocated through an adaptor.
			//! \tparam Adaptor Allocator adaptor type.
			//! \param ptr Pointer to release.
			template <typename Adaptor = DefaultAllocatorAdaptor>
			static void free(void* ptr) {
				Adaptor::get().free(ptr);
			}
			//! Releases named storage allocated through an adaptor.
			//! \tparam Adaptor Allocator adaptor type.
			//! \param name Allocation name reported to the profiler.
			//! \param ptr Pointer to release.
			template <typename Adaptor = DefaultAllocatorAdaptor>
			static void free(const char* name, void* ptr) {
				Adaptor::get().free(name, ptr);
			}
			//! Releases aligned storage allocated through an adaptor.
			//! \tparam Adaptor Allocator adaptor type.
			//! \param ptr Pointer to release.
			template <typename Adaptor = DefaultAllocatorAdaptor>
			static void free_alig(void* ptr) {
				Adaptor::get().free_alig(ptr);
			}
			//! Releases named aligned storage allocated through an adaptor.
			//! \tparam Adaptor Allocator adaptor type.
			//! \param name Allocation name reported to the profiler.
			//! \param ptr Pointer to release.
			template <typename Adaptor = DefaultAllocatorAdaptor>
			static void free_alig(const char* name, void* ptr) {
				Adaptor::get().free_alig(name, ptr);
			}
		};

		//! Allocate \a size bytes of memory using the default allocator.
		//! \param size Number of bytes to allocate
		//! \return Pointer to the allocated memory.
		GAIA_NODISCARD inline void* mem_alloc(size_t size) {
			return DefaultAllocatorAdaptor::get().alloc(size);
		}

		//! Allocate \a size bytes of memory using the default allocator.
		//! \param name Allocation name for debug purposes
		//! \param size Number of bytes to allocate
		//! \return Pointer to the allocated memory.
		GAIA_NODISCARD inline void* mem_alloc(const char* name, size_t size) {
			return DefaultAllocatorAdaptor::get().alloc(name, size);
		}

		//! Allocate \a size bytes of memory using the default allocator.
		//! The memory is aligned to \a alig boundary.
		//! \param size Number of bytes to allocate
		//! \param alig Allocated data alignment
		//! \return Pointer to the allocated memory.
		GAIA_NODISCARD inline void* mem_alloc_alig(size_t size, size_t alig) {
			return DefaultAllocatorAdaptor::get().alloc_alig(size, alig);
		}

		//! Allocate \a size bytes of memory using the default allocator.
		//! The memory is aligned to \a alig boundary.
		//! \param name Allocation name for debug purposes
		//! \param size Number of bytes to allocate
		//! \param alig Allocated data alignment
		//! \return Pointer to the allocated memory.
		GAIA_NODISCARD inline void* mem_alloc_alig(const char* name, size_t size, size_t alig) {
			return DefaultAllocatorAdaptor::get().alloc_alig(name, size, alig);
		}

		//! Release memory allocated by the default allocator.
		//! \param ptr Allocated data
		inline void mem_free(void* ptr) {
			DefaultAllocatorAdaptor::get().free(ptr);
		}

		//! Release memory allocated by the default allocator.
		//! \param name Allocation name for debug purposes
		//! \param ptr Allocated data
		inline void mem_free(const char* name, void* ptr) {
			DefaultAllocatorAdaptor::get().free(name, ptr);
		}

		//! Release aligned memory allocated by the default allocator.
		//! \param ptr Allocated data
		inline void mem_free_alig(void* ptr) {
			DefaultAllocatorAdaptor::get().free_alig(ptr);
		}

		//! Release aligned memory allocated by the default allocator.
		//! \param name Allocation name for debug purposes
		//! \param ptr Allocated data
		inline void mem_free_alig(const char* name, void* ptr) {
			DefaultAllocatorAdaptor::get().free_alig(name, ptr);
		}

		//! Align a number to the requested byte alignment
		//! \param num Number to align
		//! \param alignment Requested alignment
		//! \return Aligned number
		template <typename T, typename V>
		GAIA_NODISCARD constexpr T align(T num, V alignment) {
			return alignment == 0 ? num : ((num + (alignment - 1)) / alignment) * alignment;
		}

		//! Align a number to the requested byte alignment
		//! \tparam alignment Requested alignment in bytes
		//! \tparam T Data type
		//! \param num Number to align
		//! \return Aligned number
		template <size_t alignment, typename T>
		GAIA_NODISCARD constexpr T align(T num) {
			return ((num + (alignment - 1)) & ~(alignment - 1));
		}

		//! Returns the padding
		//! \param num Number to align
		//! \param alignment Requested alignment
		//! \return Padding in bytes
		template <typename T, typename V>
		GAIA_NODISCARD constexpr uint32_t padding(T num, V alignment) {
			return (uint32_t)(align(num, alignment) - num);
		}

		//! Returns the padding
		//! \tparam alignment Requested alignment in bytes
		//! \param num Number to align
		//! \return Aligned number
		template <size_t alignment, typename T>
		GAIA_NODISCARD constexpr uint32_t padding(T num) {
			return (uint32_t)(align<alignment>(num) - num);
		}

		//! Convert form type \a Src to type \a Dst without causing an undefined behavior
		//! \tparam Dst Destination data type
		//! \tparam Src Source data type
		//! \param src Source
		//! \return Converted destination type
		template <typename Dst, typename Src>
		GAIA_NODISCARD Dst bit_cast(const Src& src) {
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
		//! \tparam T Stored value type.
		template <typename T>
		class unaligned_ref {
			void* m_p;

		public:
			//! Creates a reference to potentially unaligned storage.
			//! \param p Storage address.
			unaligned_ref(void* p): m_p(p) {}

			//! Stores a value using byte-wise copying.
			//! \param value Value to store.
			//! \return This reference.
			unaligned_ref& operator=(const T& value) {
				memmove(m_p, (const void*)&value, sizeof(T));
				return *this;
			}

			//! Loads a value using byte-wise copying.
			//! \return Value read from storage.
			GAIA_NODISCARD operator T() const {
				T tmp;
				memmove((void*)&tmp, (const void*)m_p, sizeof(T));
				return tmp;
			}
		};
	} // namespace mem
} // namespace gaia
