#pragma once
#include <cinttypes>
#include <cstring>
#include <type_traits>
#include <utility>

#include "../config/profiler.h"

#if defined(__GLIBC__) || defined(__sun) || defined(__CYGWIN__)
	#include <alloca.h>
#elif defined(_WIN32)
	#include <malloc.h>
#endif

namespace gaia {
	namespace utils {
		void* alloc(size_t size) {
			void* ptr = ::malloc(size);
			GAIA_PROF_ALLOC(ptr, size);
			return ptr;
		}

		void* alloc_alig(size_t alig, size_t size) {
#if defined(__GLIBC__) || defined(__sun) || defined(__CYGWIN__)
			void* ptr = ::aligned_alloc(alig, size);
#elif defined(_WIN32)
	// Clang with MSVC codegen needs some remapping
	#if !defined(aligned_alloc)
			void* ptr = ::_aligned_malloc(size, alig);
	#else
			void* ptr = ::aligned_alloc(alig, size);
	#endif
#else
			void* ptr = ::aligned_alloc(alig, size);
#endif

			GAIA_PROF_ALLOC(ptr, size);
			return ptr;
		}

		void free(void* ptr) {
			::free(ptr);
			GAIA_PROF_FREE(ptr);
		}

		void free_alig(void* ptr) {
#if defined(__GLIBC__) || defined(__sun) || defined(__CYGWIN__)
	#if !defined(aligned_free)
			::free(ptr);
	#else
			::aligned_free(ptr);
	#endif
#elif defined(_WIN32)
	#if !defined(aligned_free)
			::_aligned_free(ptr);
	#else
			::aligned_free(ptr);
	#endif
#else
	#if !defined(aligned_free)
			::free(ptr);
	#else
			::aligned_free(ptr);
	#endif
#endif

			GAIA_PROF_FREE(ptr);
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

		//! Convert form type \tparam From to type \tparam To without causing an undefined behavior
		template <
				typename To, typename From,
				typename = std::enable_if_t<
						(sizeof(To) == sizeof(From)) && std::is_trivially_copyable_v<To> && std::is_trivially_copyable_v<From>>>
		To bit_cast(const From& from) {
			// int i = {};
			// float f = *(*float)&i; // undefined behavior
			// memcpy(&f, &i, sizeof(float)); // okay
			To to;
			memmove(&to, &from, sizeof(To));
			return to;
		}

		//! Pointer wrapper for reading memory in defined way (not causing undefined behavior)
		template <typename T>
		class const_unaligned_pointer {
			const uint8_t* from;

		public:
			const_unaligned_pointer(): from(nullptr) {}
			const_unaligned_pointer(const void* p): from((const uint8_t*)p) {}

			T operator*() const {
				T to;
				memmove(&to, from, sizeof(T));
				return to;
			}

			T operator[](ptrdiff_t d) const {
				return *(*this + d);
			}

			const_unaligned_pointer operator+(ptrdiff_t d) const {
				return const_unaligned_pointer(from + d * sizeof(T));
			}
			const_unaligned_pointer operator-(ptrdiff_t d) const {
				return const_unaligned_pointer(from - d * sizeof(T));
			}
		};

		//! Pointer wrapper for writing memory in defined way (not causing undefined behavior)
		template <typename T>
		class unaligned_ref {
			void* m_p;

		public:
			unaligned_ref(void* p): m_p(p) {}

			void operator=(const T& rvalue) {
				memmove(m_p, &rvalue, sizeof(T));
			}

			operator T() const {
				T tmp;
				memmove(&tmp, m_p, sizeof(T));
				return tmp;
			}
		};

		//! Pointer wrapper for writing memory in defined way (not causing undefined behavior)
		template <typename T>
		class unaligned_pointer {
			uint8_t* m_p;

		public:
			unaligned_pointer(): m_p(nullptr) {}
			unaligned_pointer(void* p): m_p((uint8_t*)p) {}

			unaligned_ref<T> operator*() const {
				return unaligned_ref<T>(m_p);
			}

			unaligned_ref<T> operator[](ptrdiff_t d) const {
				return *(*this + d);
			}

			unaligned_pointer operator+(ptrdiff_t d) const {
				return unaligned_pointer(m_p + d * sizeof(T));
			}
			unaligned_pointer operator-(ptrdiff_t d) const {
				return unaligned_pointer(m_p - d * sizeof(T));
			}
		};

		template <typename T>
		constexpr T* addressof(T& obj) noexcept {
			return &obj;
		}

		template <class T>
		const T* addressof(const T&&) = delete;

		//! Copy \param size elements of type \tparam T from the address pointer to by \param src to \param dst
		template <typename T>
		void copy_elements(T* GAIA_RESTRICT dst, const T* GAIA_RESTRICT src, size_t size) {
			GAIA_MSVC_WARNING_PUSH()
			GAIA_MSVC_WARNING_DISABLE(6385)

			static_assert(std::is_copy_assignable_v<T>);
			for (size_t i = 0; i < size; ++i)
				dst[i] = src[i];

			GAIA_MSVC_WARNING_POP()
		}

		//! Copy or move \param size elements of type \tparam T from the address pointer to by \param src to \param dst
		template <typename T>
		void transfer_elements(T* GAIA_RESTRICT dst, const T* GAIA_RESTRICT src, size_t size) {
			GAIA_MSVC_WARNING_PUSH()
			GAIA_MSVC_WARNING_DISABLE(6385)

			if constexpr (std::is_move_assignable_v<T>) {
				for (size_t i = 0; i < size; ++i)
					dst[i] = std::move(src[i]);
			} else {
				for (size_t i = 0; i < size; ++i)
					dst[i] = src[i];
			}

			GAIA_MSVC_WARNING_POP()
		}

		//! Shift \param size elements at address pointed to by \param dst to the left by one
		template <typename T>
		void shift_elements_left(T* GAIA_RESTRICT dst, size_t size) {
			GAIA_MSVC_WARNING_PUSH()
			GAIA_MSVC_WARNING_DISABLE(6385)

			if constexpr (std::is_move_assignable_v<T>) {
				for (size_t i = 0; i < size; ++i)
					dst[i] = std::move(dst[i + 1]);
			} else {
				for (size_t i = 0; i < size; ++i)
					dst[i] = dst[i + 1];
			}

			GAIA_MSVC_WARNING_POP()
		}
	} // namespace utils
} // namespace gaia
