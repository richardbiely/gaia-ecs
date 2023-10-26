

#if __has_include(<version.h>)
	#include <version.h>
#endif
#include <cinttypes>

//------------------------------------------------------------------------------
// DO NOT MODIFY THIS FILE
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Features
//------------------------------------------------------------------------------

#if defined(_MSVC_LANG)
	#define GAIA_CPP_VERSION(version) (__cplusplus >= (version) || _MSVC_LANG >= (version))
#else
	#define GAIA_CPP_VERSION(version) (__cplusplus >= (version))
#endif

#if GAIA_CPP_VERSION(201703L)
#else
// We use some C++17+ features such as folding expressions, compile-time ifs
// and similar which makes it impossible to use Gaia-ECS with old compilers.
	#error "To build Gaia-ECS a compiler capable of at least C++17 is necesary"
#endif

#define GAIA_SAFE_CONSTEXPR constexpr

#define GAIA_USE_STD_SPAN (GAIA_CPP_VERSION(202002L) && __has_include(<span>))

//------------------------------------------------------------------------------
// Features
//------------------------------------------------------------------------------

#define GAIA_ECS_HASH_FNV1A 0
#define GAIA_ECS_HASH_MURMUR2A 1

//------------------------------------------------------------------------------
// Compiler
//------------------------------------------------------------------------------
#define GAIA_COMPILER_CLANG 0
#define GAIA_COMPILER_GCC 0
#define GAIA_COMPILER_MSVC 0
#define GAIA_COMPILER_ICC 0
#define GAIA_COMPILER_DETECTED 0

#if !GAIA_COMPILER_DETECTED && (defined(__clang__))
// Clang check is performed first as it might pretend to be MSVC or GCC by
// defining their predefined macros.
	#undef GAIA_COMPILER_CLANG
	#define GAIA_COMPILER_CLANG 1
	#undef GAIA_COMPILER_DETECTED
	#define GAIA_COMPILER_DETECTED 1
#endif
#if !GAIA_COMPILER_DETECTED &&                                                                                         \
		(defined(__INTEL_COMPILER) || defined(__ICC) || defined(__ICL) || defined(__INTEL_LLVM_COMPILER))
	#undef GAIA_COMPILER_ICC
	#define GAIA_COMPILER_ICC 1
	#undef GAIA_COMPILER_DETECTED
	#define GAIA_COMPILER_DETECTED 1
#endif
#if !GAIA_COMPILER_DETECTED && (defined(__SNC__) || defined(__GNUC__))
	#undef GAIA_COMPILER_GCC
	#define GAIA_COMPILER_GCC 1
	#undef GAIA_COMPILER_DETECTED
	#define GAIA_COMPILER_DETECTED 1
	#if __GNUC__ <= 7
		// In some contexts, e.g. when evaluating PRETTY_FUNCTION, GCC has a bug
		// where the string is not defined a constexpr and thus can't be evaluated
		// in constexpr expressions.
		#undef GAIA_SAFE_CONSTEXPR
		#define GAIA_SAFE_CONSTEXPR const
	#endif
#endif
#if !GAIA_COMPILER_DETECTED && (defined(_MSC_VER))
	#undef GAIA_COMPILER_MSVC
	#define GAIA_COMPILER_MSVC 1
	#undef GAIA_COMPILER_DETECTED
	#define GAIA_COMPILER_DETECTED 1
#endif
#if !GAIA_COMPILER_DETECTED
	#error "Unrecognized compiler"
#endif

//------------------------------------------------------------------------------
// Platform
//------------------------------------------------------------------------------

#define GAIA_PLATFORM_UNKNOWN 0
#define GAIA_PLATFORM_WINDOWS 0
#define GAIA_PLATFORM_LINUX 0
#define GAIA_PLATFORM_APPLE 0
#define GAIA_PLATFORM_FREEBSD 0

#ifdef _WIN32
	#undef GAIA_PLATFORM_WINDOWS
	#define GAIA_PLATFORM_WINDOWS 1
#elif __APPLE__
// MacOS, iOS, tvOS etc.
// We could tell the platforms apart using #include "TargetConditionals.h"
// but that is probably way more than we need to know.
	#undef GAIA_PLATFORM_APPLE
	#define GAIA_PLATFORM_APPLE 1
#elif __linux__
	#undef GAIA_PLATFORM_LINUX
	#define GAIA_PLATFORM_LINUX 1
#elif __FreeBSD__
	#undef GAIA_PLATFORM_FREEBSD
	#define GAIA_PLATFORM_FREEBSD 1
#else
	#undef GAIA_PLATFORM_UNKNOWN
	#define GAIA_PLATFORM_UNKNOWN 1
#endif

//------------------------------------------------------------------------------
// Architecture features
//------------------------------------------------------------------------------
#define GAIA_64 0
#if defined(_WIN64) || defined(_M_X64) || defined(_M_AMD64) || defined(__x86_64) || defined(__amd64) ||                \
		defined(__aarch64__)
	#undef GAIA_64
	#define GAIA_64 1
#endif

#define GAIA_ARCH_X86 0
#define GAIA_ARCH_ARM 1
#define GAIA_ARCH GAIA_ARCH_X86

#if defined(_M_IX86) || defined(_M_X64) || defined(_M_AMD64) || defined(__i386__) || defined(__i386) ||                \
		defined(i386) || defined(__x86_64__) || defined(_X86_)
	#undef GAIA_ARCH
	#define GAIA_ARCH GAIA_ARCH_X86
#elif defined(__arm__) || defined(__aarch64__) || defined(_M_ARM64) || defined(_M_ARM64EC)
	#undef GAIA_ARCH
	#define GAIA_ARCH GAIA_ARCH_ARM
#else
	#error "Unrecognized target architecture."
#endif

//------------------------------------------------------------------------------

#if GAIA_COMPILER_MSVC
	#define GAIA_PRETTY_FUNCTION __FUNCSIG__
	#define GAIA_PRETTY_FUNCTION_PREFIX '<'
	#define GAIA_PRETTY_FUNCTION_SUFFIX '>'
#else
	#define GAIA_PRETTY_FUNCTION __PRETTY_FUNCTION__
	#define GAIA_PRETTY_FUNCTION_PREFIX '='
	#define GAIA_PRETTY_FUNCTION_SUFFIX ']'
#endif

#define GAIA_STRINGIZE(x) #x
#define GAIA_CONCAT_IMPL(x, y) x##y
#define GAIA_CONCAT(x, y) GAIA_CONCAT_IMPL(x, y)

//------------------------------------------------------------------------------
// Endianess
// Endianess detection is hell as far as compile-time is concerned.
// There is a bloat of macros and header files across different compilers,
// platforms and C++ standard implementations.
//------------------------------------------------------------------------------

#if GAIA_COMPILER_MSVC
	// Whether it is ARM or x86 we consider both litte endian.
	// It is very unlikely that any modern "big" CPU would use big endian these days
	// as it is more efficient to be little endian on HW level.
	#define GAIA_LITTLE_ENDIAN true
	#define GAIA_BIG_ENDIAN false
#else
	#define GAIA_LITTLE_ENDIAN (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
	#define GAIA_BIG_ENDIAN (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#endif

//! Checks if endianess was detected correctly at compile-time.
//! \return True if endianess defined in GAIA_LITTLE_ENDIAN/GAIA_END_ENDIAN is correct. False otherwise.
//! \warning If false is returned, flip the values in GAIA_LITTLE_ENDIAN and GAIA_BIG_ENDIAN.
namespace gaia {
	inline bool CheckEndianess() {
		const uint16_t testWord = 0x1234;
		const bool isLittleEndian(*reinterpret_cast<const uint8_t*>(&testWord) == 0x34);
		return isLittleEndian && GAIA_LITTLE_ENDIAN;
	}
} // namespace gaia

//------------------------------------------------------------------------------

#if (GAIA_COMPILER_MSVC && _MSC_VER >= 1400) || GAIA_COMPILER_GCC || GAIA_COMPILER_CLANG
	#define GAIA_RESTRICT __restrict
#else
	#define GAIA_RESTRICT
#endif

#if GAIA_CPP_VERSION(202002L)
	#if __has_cpp_attribute(nodiscard)
		#define GAIA_NODISCARD [[nodiscard]]
	#endif
	#if __has_cpp_attribute(likely)
		#define GAIA_LIKELY(cond) (cond) [[likely]]
	#endif
	#if __has_cpp_attribute(unlikely)
		#define GAIA_UNLIKELY(cond) (cond) [[unlikely]]
	#endif
#endif
#ifndef GAIA_NODISCARD
	#define GAIA_NODISCARD
#endif
#if !defined(GAIA_LIKELY) && (GAIA_COMPILER_GCC || GAIA_COMPILER_CLANG)
	#define GAIA_LIKELY(cond) (__builtin_expect((cond), 1))
#endif
#if !defined(GAIA_UNLIKELY) && (GAIA_COMPILER_GCC || GAIA_COMPILER_CLANG)
	#define GAIA_UNLIKELY(cond) (__builtin_expect((cond), 0))
#endif
#ifndef GAIA_LIKELY
	#define GAIA_LIKELY(cond) (cond)
#endif
#ifndef GAIA_UNLIKELY
	#define GAIA_UNLIKELY(cond) (cond)
#endif

// GCC 7 and some later versions had a bug that would aritificaly restrict alignas for stack
// variables to 16 bytes.
// However, using the compiler custom attribute would still work. Therefore, because it is
// more portable, we shall introduce the GAIA_ALIGNAS macro.
// Read about the bug here: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=89357
#if GAIA_COMPILER_GCC
	#define GAIA_ALIGNAS(alignment) __attribute__((aligned(alignment)))
#else
	#define GAIA_ALIGNAS(alignment) alignas(alignment)
#endif

#if GAIA_COMPILER_MSVC
	#if _MSV_VER <= 1916
		#include <intrin.h>
	#endif
	// MSVC doesn't implement __popcnt for ARM so we need to do it ourselves
	#if GAIA_ARCH == GAIA_ARCH_ARM
		#include <arm_neon.h>
	//! Returns the number of set bits in \param x
		#define GAIA_POPCNT(x)                                                                                             \
			([](uint32_t value) noexcept {                                                                                   \
				const __n64 tmp = neon_cnt(__uint64ToN64_v(value));                                                            \
				return (uint32_t)neon_addv8(tmp).n8_i8[0];                                                                     \
			}(x))
	//! Returns the number of set bits in \param x
		#define GAIA_POPCNT64(x)                                                                                           \
			([](uint64_t value) noexcept {                                                                                   \
				const __n64 tmp = neon_cnt(__uint64ToN64_v(value));                                                            \
				return (uint32_t)neon_addv8(tmp).n8_i8[0];                                                                     \
			}(x))
	#else
	//! Returns the number of set bits in \param x
		#define GAIA_POPCNT(x) ((uint32_t)__popcnt(x))
	//! Returns the number of set bits in \param x
		#define GAIA_POPCNT64(x) ((uint32_t)__popcnt64(x))
	#endif

	#pragma intrinsic(_BitScanForward)
	//! Returns the number of leading zeros of \param x or 32 if \param x is 0.
	//! \warning Little-endian format.
	#define GAIA_CLZ(x)                                                                                                  \
		([](uint32_t value) noexcept {                                                                                     \
			unsigned long index;                                                                                             \
			return _BitScanForward(&index, value) ? (uint32_t)index : (uint32_t)32;                                          \
		}(x))
	#pragma intrinsic(_BitScanForward64)
	//! Returns the number of leading zeros of \param x or 64 if \param x is 0.
	//! \warning Little-endian format.
	#define GAIA_CLZ64(x)                                                                                                \
		([](uint64_t value) noexcept {                                                                                     \
			unsigned long index;                                                                                             \
			return _BitScanForward64(&index, value) ? (uint32_t)index : (uint32_t)64;                                        \
		}(x))

	#pragma intrinsic(_BitScanReverse)
	//! Returns the number of trailing zeros of \param x or 32 if \param x is 0.
	//! \warning Little-endian format.
	#define GAIA_CTZ(x)                                                                                                  \
		([](uint32_t value) noexcept {                                                                                     \
			unsigned long index;                                                                                             \
			return _BitScanReverse(&index, value) ? 31U - (uint32_t)index : (uint32_t)32;                                    \
		}(x))
	#pragma intrinsic(_BitScanReverse64)
	//! Returns the number of trailing zeros of \param x or 64 if \param x is 0.
	//! \warning Little-endian format.
	#define GAIA_CTZ64(x)                                                                                                \
		([](uint64_t value) noexcept {                                                                                     \
			unsigned long index;                                                                                             \
			return _BitScanReverse64(&index, value) ? 63U - (uint32_t)index : (uint32_t)64;                                  \
		}(x))

	#pragma intrinsic(_BitScanForward)
	//! Returns 1 plus the index of the least significant set bit of \param x, or 0 if \param x is 0.
	//! \warning Little-endian format.
	#define GAIA_FFS(x)                                                                                                  \
		([](uint32_t value) noexcept {                                                                                     \
			unsigned long index;                                                                                             \
			if (_BitScanForward(&index, value))                                                                              \
				return (uint32_t)(index + 1);                                                                                  \
			return (uint32_t)0;                                                                                              \
		}(x))
	#pragma intrinsic(_BitScanForward64)
	//! Returns 1 plus the index of the least significant set bit of \param x, or 0 if \param x is 0.
	//! \warning Little-endian format.
	#define GAIA_FFS64(x)                                                                                                \
		([](uint64_t value) noexcept {                                                                                     \
			unsigned long index;                                                                                             \
			if (_BitScanForward64(&index, value))                                                                            \
				return (uint32_t)(index + 1);                                                                                  \
			return (uint32_t)0;                                                                                              \
		}(x))
#elif GAIA_COMPILER_CLANG || GAIA_COMPILER_GCC
	//! Returns the number of set bits in \param x
	#define GAIA_POPCNT(x) ((uint32_t)__builtin_popcount(x))
	//! Returns the number of set bits in \param x
	#define GAIA_POPCNT64(x) ((uint32_t)__builtin_popcountll(x))

//! Returns the number of leading zeros of \param x or 32 if \param x is 0.
//! \warning Little-endian format.
	#define GAIA_CLZ(x) ((x) ? (uint32_t)__builtin_ctz(x) : (uint32_t)32)
	//! Returns the number of leading zeros of \param x or 64 if \param x is 0.
//! \warning Little-endian format.
	#define GAIA_CLZ64(x) ((x) ? (uint32_t)__builtin_ctzll(x) : (uint32_t)64)

//! Returns the number of trailing zeros of \param x or 32 if \param x is 0.
//! \warning Little-endian format.
	#define GAIA_CTZ(x) ((x) ? (uint32_t)__builtin_clz(x) : (uint32_t)32)
	//! Returns the number of trailing zeros of \param x or 64 if \param x is 0.
//! \warning Little-endian format.
	#define GAIA_CTZ64(x) ((x) ? (uint32_t)__builtin_clzll(x) : (uint32_t)64)

//! Returns 1 plus the index of the least significant set bit of \param x, or 0 if \param x is 0.
//! \warning Little-endian format.
	#define GAIA_FFS(x) ((uint32_t)__builtin_ffs(x))
	//! Returns 1 plus the index of the least significant set bit of \param x, or 0 if \param x is 0.
//! \warning Little-endian format.
	#define GAIA_FFS64(x) ((uint32_t)__builtin_ffsll(x))
#else
	//! Returns the number of set bits in \param x
	#define GAIA_POPCNT(x)                                                                                               \
		([](uint32_t value) noexcept -> uint32_t {                                                                         \
			uint32_t bitsSet = 0;                                                                                            \
			while (value != 0) {                                                                                             \
				value &= (value - 1);                                                                                          \
				++bitsSet;                                                                                                     \
			}                                                                                                                \
			return bitsSet;                                                                                                  \
		}(x))
	//! Returns the number of set bits in \param x
	#define GAIA_POPCNT64(x)                                                                                             \
		([](uint64_t value) noexcept -> uint32_t {                                                                         \
			uint32_t bitsSet = 0;                                                                                            \
			while (value != 0) {                                                                                             \
				value &= (value - 1);                                                                                          \
				++bitsSet;                                                                                                     \
			}                                                                                                                \
			return bitsSet;                                                                                                  \
		}(x))

//! Returns the number of leading zeros of \param x or 32 if \param x is 0.
//! \warning Little-endian format.
	#define GAIA_CLZ(x)                                                                                                  \
		([](uint32_t value) noexcept -> uint32_t {                                                                         \
			if (value == 0)                                                                                                  \
				return 32;                                                                                                     \
			uint32_t index = 0;                                                                                              \
			while (((value >> index) & 1) == 0)                                                                              \
				++index;                                                                                                       \
			return index;                                                                                                    \
		}(x))
	//! Returns the number of leading zeros of \param x or 64 if \param x is 0.
//! \warning Little-endian format.
	#define GAIA_CLZ64(x)                                                                                                \
		([](uint64_t value) noexcept -> uint32_t {                                                                         \
			if (value == 0)                                                                                                  \
				return 64;                                                                                                     \
			uint32_t index = 0;                                                                                              \
			while (((value >> index) & 1) == 0)                                                                              \
				++index;                                                                                                       \
			return index;                                                                                                    \
		}(x))

//! Returns the number of trailing zeros of \param x or 32 if \param x is 0.
//! \warning Little-endian format.
	#define GAIA_CTZ(x)                                                                                                  \
		([](uint32_t value) noexcept -> uint32_t {                                                                         \
			if (value == 0)                                                                                                  \
				return 32;                                                                                                     \
			uint32_t index = 0;                                                                                              \
			while (((value << index) & 0x80000000) == 0)                                                                     \
				++index;                                                                                                       \
			return index;                                                                                                    \
		}(x))
	//! Returns the number of trailing zeros of \param x or 64 if \param x is 0.
//! \warning Little-endian format.
	#define GAIA_CTZ64(x)                                                                                                \
		([](uint64_t value) noexcept -> uint32_t {                                                                         \
			if (value == 0)                                                                                                  \
				return 64;                                                                                                     \
			uint32_t index = 0;                                                                                              \
			while (((value << index) & 0x8000000000000000LL) == 0)                                                           \
				++index;                                                                                                       \
			return index;                                                                                                    \
		}(x))

//! Returns 1 plus the index of the least significant set bit of \param x, or 0 if \param x is 0.
//! \warning Little-endian format.
	#define GAIA_FFS(x)                                                                                                  \
		([](uint32_t value) noexcept -> uint32_t {                                                                         \
			if (value == 0)                                                                                                  \
				return 0;                                                                                                      \
			uint32_t index = 0;                                                                                              \
			while (((value >> index) & 1) == 0)                                                                              \
				++index;                                                                                                       \
			return index + 1;                                                                                                \
		}(x))
	//! Returns 1 plus the index of the least significant set bit of \param x, or 0 if \param x is 0.
//! \warning Little-endian format.
	#define GAIA_FFS64(x)                                                                                                \
		([](uint64_t value) noexcept -> uint32_t {                                                                         \
			if (value == 0)                                                                                                  \
				return 0;                                                                                                      \
			uint32_t index = 0;                                                                                              \
			while (((value >> index) & 1) == 0)                                                                              \
				++index;                                                                                                       \
			return index + 1;                                                                                                \
		}(x))
#endif

//------------------------------------------------------------------------------

#if GAIA_COMPILER_MSVC || GAIA_COMPILER_ICC
	#define GAIA_FORCEINLINE __forceinline
	#define GAIA_NOINLINE __declspec(noinline)
#elif GAIA_COMPILER_CLANG || GAIA_COMPILER_GCC
	#define GAIA_FORCEINLINE inline __attribute__((always_inline))
	#define GAIA_NOINLINE inline __attribute__((noinline))
#else
	#define GAIA_FORCEINLINE
	#define GAIA_NOINLINE
#endif

#if GAIA_COMPILER_MSVC
	#if _MSC_VER >= 1927 && _MSVC_LANG > 202002L // MSVC 16.7 or newer &&�/std:c++latest
		#define GAIA_LAMBDAINLINE [[msvc::forceinline]]
	#else
		#define GAIA_LAMBDAINLINE
	#endif
#elif GAIA_COMPILER_CLANG || GAIA_COMPILER_GCC
	#define GAIA_LAMBDAINLINE __attribute__((always_inline))
#else
	#define GAIA_LAMBDAINLINE
#endif

//------------------------------------------------------------------------------

#if GAIA_COMPILER_MSVC
	#define GAIA_IMPORT __declspec(dllimport)
	#define GAIA_EXPORT __declspec(dllexport)
	#define GAIA_HIDDEN
#elif GAIA_COMPILER_CLANG || GAIA_COMPILER_GCC
	#define GAIA_IMPORT _attribute__((visibility("default")))
	#define GAIA_EXPORT _attribute__((visibility("default")))
	#define GAIA_HIDDEN _attribute__((visibility("hidden")))
#endif

#if defined(GAIA_DLL)
	#define GAIA_API GAIA_IMPORT
#elif defined(GAIA_DLL_EXPORT)
	#define GAIA_API GAIA_EXPORT
#else
	#define GAIA_API
#endif

//------------------------------------------------------------------------------
// Warning-related macros and settings
// We always set warnings as errors and disable ones we don't care about.
// Sometimes in only limited range of code or around 3rd party includes.
//------------------------------------------------------------------------------

#if GAIA_COMPILER_MSVC
	#define GAIA_MSVC_WARNING_PUSH() __pragma(warning(push))
	#define GAIA_MSVC_WARNING_POP() __pragma(warning(pop))
	#define GAIA_MSVC_WARNING_DISABLE(warningId) __pragma(warning(disable : warningId))
	#define GAIA_MSVC_WARNING_ERROR(warningId) __pragma(warning(error : warningId))
#else
	#define GAIA_MSVC_WARNING_PUSH()
	#define GAIA_MSVC_WARNING_POP()
	#define GAIA_MSVC_WARNING_DISABLE(warningId)
	#define GAIA_MSVC_WARNING_ERROR(warningId)
#endif

#if GAIA_COMPILER_CLANG
	#define DO_PRAGMA_(x) _Pragma(#x)
	#define DO_PRAGMA(x) DO_PRAGMA_(x)
	#define GAIA_CLANG_WARNING_PUSH() _Pragma("clang diagnostic push")
	#define GAIA_CLANG_WARNING_POP() _Pragma("clang diagnostic pop")
	#define GAIA_CLANG_WARNING_DISABLE(warningId) DO_PRAGMA(clang diagnostic ignored warningId)
	#define GAIA_CLANG_WARNING_ERROR(warningId) DO_PRAGMA(clang diagnostic error warningId)
	#define GAIA_CLANG_WARNING_ALLOW(warningId) DO_PRAGMA(clang diagnostic warning warningId)
#else
	#define GAIA_CLANG_WARNING_PUSH()
	#define GAIA_CLANG_WARNING_POP()
	#define GAIA_CLANG_WARNING_DISABLE(warningId)
	#define GAIA_CLANG_WARNING_ERROR(warningId)
	#define GAIA_CLANG_WARNING_ALLOW(warningId)
#endif

#if GAIA_COMPILER_GCC
	#define DO_PRAGMA_(x) _Pragma(#x)
	#define DO_PRAGMA(x) DO_PRAGMA_(x)
	#define GAIA_GCC_WARNING_PUSH() _Pragma("GCC diagnostic push")
	#define GAIA_GCC_WARNING_POP() _Pragma("GCC diagnostic pop")
	#define GAIA_GCC_WARNING_ERROR(warningId) DO_PRAGMA(GCC diagnostic error warningId)
	#define GAIA_GCC_WARNING_DISABLE(warningId) DO_PRAGMA(GCC diagnostic ignored warningId)
#else
	#define GAIA_GCC_WARNING_PUSH()
	#define GAIA_GCC_WARNING_POP()
	#define GAIA_GCC_WARNING_DISABLE(warningId)
#endif

#define GAIA_HAS_NO_INLINE_ASSEMBLY 0
#if (!defined(__GNUC__) && !defined(__clang__)) || defined(__pnacl__) || defined(__EMSCRIPTEN__)
	#undef GAIA_HAS_NO_INLINE_ASSEMBLY
	#define GAIA_HAS_NO_INLINE_ASSEMBLY 1
#endif

namespace gaia {
// The dont_optimize(...) function can be used to prevent a value or
// expression from being optimized away by the compiler. This function is
// intended to add little to no overhead.
// See: https://youtu.be/nXaxk27zwlk?t=2441
#if !GAIA_HAS_NO_INLINE_ASSEMBLY
	template <class T>
	inline void dont_optimize(T const& value) {
		asm volatile("" : : "r,m"(value) : "memory");
	}

	template <class T>
	inline void dont_optimize(T& value) {
	#if defined(__clang__)
		asm volatile("" : "+r,m"(value) : : "memory");
	#else
		asm volatile("" : "+m,r"(value) : : "memory");
	#endif
	}
#else
	namespace detail {
		inline void use_char_pointer(char const volatile* var) {
			(void)var;
		}
	} // namespace detail

	#if defined(_MSC_VER)
	template <class T>
	inline void dont_optimize(T const& value) {
		detail::use_char_pointer(&reinterpret_cast<char const volatile&>(value));
		::_ReadWriteBarrier();
	}
	#else
	template <class T>
	inline void dont_optimize(T const& value) {
		detail::use_char_pointer(&reinterpret_cast<char const volatile&>(value));
	}
	#endif
#endif
} // namespace gaia

// Breaking changes and big features
#define GAIA_VERSION_MAJOR 0
// Smaller changes and features
#define GAIA_VERSION_MINOR 7
// Fixes and tweaks
#define GAIA_VERSION_PATCH 6

//------------------------------------------------------------------------------
// General settings.
// You are free to modify these.
//------------------------------------------------------------------------------

//! If enabled, GAIA_DEBUG is defined despite using a "Release" build configuration for example
#if !defined(GAIA_FORCE_DEBUG)
	#define GAIA_FORCE_DEBUG 0
#endif
//! If enabled, no asserts are thrown even in debug builds
#if !defined(GAIA_DISABLE_ASSERTS)
	#define GAIA_DISABLE_ASSERTS 0
#endif

//! Number of bits used for the entity identifier.
//! You should only touch this if you need more than 2^20 entities which should be fairly
//! difficult to achieve.
//! Check entity.h IdBits and GenBits for more info.
#if !defined(GAIA_ENTITY_IDBITS)
	#define GAIA_ENTITY_IDBITS 20
#endif
//! Number of bits used for the entity generation.
//! You should only touch this if you need more than 2^12 generations for your entities
//! which should be fairly difficult to achive.
//! Check entity.h IdBits and GenBits for more info.
#if !defined(GAIA_ENTITY_GENBITS)
	#define GAIA_ENTITY_GENBITS 12
#endif

//------------------------------------------------------------------------------
// Internal features.
// You are free to modify these but you probably should not.
// It is expected to only use them when something doesn't work as expected
// or as some sort of workaround.
//------------------------------------------------------------------------------

//! If enabled, custom allocator is used for allocating archetype chunks.
#ifndef GAIA_ECS_CHUNK_ALLOCATOR
	#define GAIA_ECS_CHUNK_ALLOCATOR 1
#endif

//! Hashing algorithm. GAIA_ECS_HASH_FNV1A or GAIA_ECS_HASH_MURMUR2A
#ifndef GAIA_ECS_HASH
	#define GAIA_ECS_HASH GAIA_ECS_HASH_MURMUR2A
#endif

//! If enabled, explicit memory prefetching is used when querying chunks, possibly improving performance in
//! edge-cases
#ifndef GAIA_USE_PREFETCH
	#define GAIA_USE_PREFETCH 1
#endif

//! Maximum number of entities to defragment per frame
#ifndef GAIA_DEFRAG_ENTITIES_PER_FRAME
	#define GAIA_DEFRAG_ENTITIES_PER_FRAME 100
#endif

//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// DO NOT MODIFY THIS FILE
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// External code settings
//------------------------------------------------------------------------------

#ifndef TCB_SPAN_NO_CONTRACT_CHECKING
	#define TCB_SPAN_NO_CONTRACT_CHECKING
#endif

//------------------------------------------------------------------------------
// Debug features
//------------------------------------------------------------------------------

#if !defined(NDEBUG) || defined(_DEBUG)
	#define GAIA_DEBUG_BUILD 1
#else
	#define GAIA_DEBUG_BUILD 0
#endif

//! If enabled, additional debug and verification code is used which
//! slows things down but enables better security and diagnostics.
//! Suitable for debug builds first and foremost. Therefore, it is
//! enabled by default for debud builds.
#if !defined(GAIA_DEBUG)
	#if GAIA_DEBUG_BUILD || GAIA_FORCE_DEBUG
		#define GAIA_DEBUG 1
	#else
		#define GAIA_DEBUG 0
	#endif
#endif

#if GAIA_DISABLE_ASSERTS
	#undef GAIA_ASSERT
	#define GAIA_ASSERT(cond) (void(0))
#elif !defined(GAIA_ASSERT)
	#include <cassert>
	// For Debug builds use system's native assertion capabilities
	#if GAIA_DEBUG_BUILD
		#define GAIA_ASSERT_ENABLED 1
		#define GAIA_ASSERT(cond)                                                                                          \
			{                                                                                                                \
				GAIA_MSVC_WARNING_PUSH()                                                                                       \
				GAIA_MSVC_WARNING_DISABLE(4127)                                                                                \
				if GAIA_LIKELY (cond)                                                                                          \
					(void(0));                                                                                                   \
				else                                                                                                           \
					[] {                                                                                                         \
						assert(!#cond);                                                                                            \
					}();                                                                                                         \
				GAIA_MSVC_WARNING_POP()                                                                                        \
			}
	#else
		// For non-Debug builds simulate asserts
		#if GAIA_DEBUG
			#define GAIA_ASSERT_ENABLED 1
			#define GAIA_ASSERT(cond)                                                                                        \
				{                                                                                                              \
					GAIA_MSVC_WARNING_PUSH()                                                                                     \
					GAIA_MSVC_WARNING_DISABLE(4127)                                                                              \
					if GAIA_LIKELY (cond)                                                                                        \
						(void(0));                                                                                                 \
					else                                                                                                         \
						[] {                                                                                                       \
							GAIA_LOG_E("%s:%d: Assertion failed: '%s'.", __FILE__, __LINE__, #cond);                                 \
						}();                                                                                                       \
					GAIA_MSVC_WARNING_POP()                                                                                      \
				}
		#else
			#define GAIA_ASSERT_ENABLED 0
			#define GAIA_ASSERT(cond) (void(0))
		#endif
	#endif
#endif

#if !defined(GAIA_ECS_VALIDATE_CHUNKS)
	#define GAIA_ECS_VALIDATE_CHUNKS (GAIA_DEBUG)
#endif
#if !defined(GAIA_ECS_VALIDATE_ENTITY_LIST)
	#define GAIA_ECS_VALIDATE_ENTITY_LIST (GAIA_DEBUG)
#endif

//------------------------------------------------------------------------------
// Prefetching
//------------------------------------------------------------------------------

namespace gaia {

	enum PrefetchHint : int {
		//! Temporal data — prefetch data into all levels of the cache hierarchy
		PREFETCH_HINT_T0 = 3,
		//! Temporal data with respect to first level cache misses — prefetch data into L2 cache and higher
		PREFETCH_HINT_T1 = 2,
		//! Temporal data with respect to second level cache misses — prefetch data into L3 cache and higher,
		//! or an implementation-specific choice
		PREFETCH_HINT_T2 = 1,
		//! Non-temporal data with respect to all cache levels — prefetch data into non-temporal cache structure
		//! and into a location close to the processor, minimizing cache pollution
		PREFETCH_HINT_NTA = 0
	};

	//! Prefetch intrinsic
	extern inline void prefetch([[maybe_unused]] const void* x, [[maybe_unused]] int hint) {
#if GAIA_USE_PREFETCH
	#if GAIA_COMPILER_CLANG
		// In the gcc version of prefetch(), hint is only a constant _after_ inlining
		// (assumed to have been successful).  llvm views things differently, and
		// checks constant-ness _before_ inlining.  This leads to compilation errors
		// with using the other version of this code with llvm.
		//
		// One way round this is to use a switch statement to explicitly match
		// prefetch hint enumerations, and invoke __builtin_prefetch for each valid
		// value.  llvm's optimization removes the switch and unused case statements
		// after inlining, so that this boils down in the end to the same as for gcc;
		// that is, a single inlined prefetchX instruction.
		//
		// Note that this version of prefetch() cannot verify constant-ness of hint.
		// If client code calls prefetch() with a variable value for hint, it will
		// receive the full expansion of the switch below, perhaps also not inlined.
		// This should however not be a problem in the general case of well behaved
		// caller code that uses the supplied prefetch hint enumerations.
		switch (hint) {
			case PREFETCH_HINT_T0:
				__builtin_prefetch(x, 0, PREFETCH_HINT_T0);
				break;
			case PREFETCH_HINT_T1:
				__builtin_prefetch(x, 0, PREFETCH_HINT_T1);
				break;
			case PREFETCH_HINT_T2:
				__builtin_prefetch(x, 0, PREFETCH_HINT_T2);
				break;
			case PREFETCH_HINT_NTA:
				__builtin_prefetch(x, 0, PREFETCH_HINT_NTA);
				break;
			default:
				__builtin_prefetch(x);
				break;
		}
	#elif GAIA_COMPILER_GCC
		#if !defined(__i386) || defined(__SSE__)
		if (__builtin_constant_p(hint)) {
			__builtin_prefetch(x, 0, hint);
		} else {
			// Defaults to PREFETCH_HINT_T0
			__builtin_prefetch(x);
		}
		#elif !GAIA_HAS_NO_INLINE_ASSEMBLY
		// We want a __builtin_prefetch, but we build with the default -march=i386
		// where __builtin_prefetch quietly turns into nothing.
		// Once we crank up to -march=pentium3 or higher the __SSE__
		// clause above will kick in with the builtin.
		if (hint == PREFETCH_HINT_NTA)
			asm volatile("prefetchnta (%0)" : : "r"(x));
		#endif
	#else
			// Not implemented
	#endif
#endif
	}

} // namespace gaia

#include <cstdio> // vsnprintf, sscanf, printf

//! Log - debug
#ifndef GAIA_LOG_D
	#define GAIA_LOG_D(...)                                                                                              \
		{                                                                                                                  \
			fprintf(stdout, "%s %s, D: ", __DATE__, __TIME__);                                                               \
			fprintf(stdout, __VA_ARGS__);                                                                                    \
			fprintf(stdout, "\n");                                                                                           \
		}
#endif

//! Log - normal/informational
#ifndef GAIA_LOG_N
	#define GAIA_LOG_N(...)                                                                                              \
		{                                                                                                                  \
			fprintf(stdout, "%s %s, I: ", __DATE__, __TIME__);                                                               \
			fprintf(stdout, __VA_ARGS__);                                                                                    \
			fprintf(stdout, "\n");                                                                                           \
		}
#endif

//! Log - warning
#ifndef GAIA_LOG_W
	#define GAIA_LOG_W(...)                                                                                              \
		{                                                                                                                  \
			fprintf(stdout, "%s %s, W: ", __DATE__, __TIME__);                                                               \
			fprintf(stderr, __VA_ARGS__);                                                                                    \
			fprintf(stderr, "\n");                                                                                           \
		}
#endif

//! Log - error
#ifndef GAIA_LOG_E
	#define GAIA_LOG_E(...)                                                                                              \
		{                                                                                                                  \
			fprintf(stdout, "%s %s, E: ", __DATE__, __TIME__);                                                               \
			fprintf(stderr, __VA_ARGS__);                                                                                    \
			fprintf(stderr, "\n");                                                                                           \
		}
#endif

#if GAIA_PROFILER_CPU || GAIA_PROFILER_MEM
// Keep it small on Windows
// TODO: What if user doesn't want this?
// #if GAIA_PLATFORM_WINDOWS && !defined(WIN32_LEAN_AND_MEAN)
// 	#define WIN32_LEAN_AND_MEAN
// #endif
GAIA_MSVC_WARNING_PUSH()
GAIA_MSVC_WARNING_DISABLE(4668)
	#include <tracy/Tracy.hpp>
	#include <tracy/TracyC.h>
GAIA_MSVC_WARNING_POP()
#endif

#if GAIA_PROFILER_CPU

namespace tracy {

	//! Zone used for tracking zones with names first available in run-time
	struct ZoneRT {
		TracyCZoneCtx m_ctx;

		ZoneRT(const char* name, const char* file, uint32_t line, const char* function) {
			const auto srcloc =
					___tracy_alloc_srcloc_name(line, file, strlen(file), function, strlen(function), name, strlen(name));
			m_ctx = ___tracy_emit_zone_begin_alloc(srcloc, 1);
		}
		~ZoneRT() {
			TracyCZoneEnd(m_ctx);
		}
	};

	struct ScopeStack {
		static constexpr uint32_t StackSize = 64;

		uint32_t count;
		TracyCZoneCtx buffer[StackSize];
	};

	inline thread_local ScopeStack t_ScopeStack;

	void ZoneBegin(const ___tracy_source_location_data* srcloc) {
		auto& stack = t_ScopeStack;
		const auto pos = stack.count++;
		if (pos < ScopeStack::StackSize) {
			stack.buffer[pos] = ___tracy_emit_zone_begin(srcloc, 1);
		}
	}

	void ZoneRTBegin(uint64_t srcloc) {
		auto& stack = t_ScopeStack;
		const auto pos = stack.count++;
		if (pos < ScopeStack::StackSize)
			stack.buffer[pos] = ___tracy_emit_zone_begin_alloc(srcloc, 1);
	}

	void ZoneEnd() {
		auto& stack = t_ScopeStack;
		GAIA_ASSERT(stack.count > 0);
		const auto pos = --stack.count;
		if (pos < ScopeStack::StackSize)
			___tracy_emit_zone_end(stack.buffer[pos]);
	}
} // namespace tracy

	#define TRACY_ZoneNamedRT(name, function)                                                                            \
		tracy::ZoneRT TracyConcat(__tracy_zone_dynamic, __LINE__)(name, __FILE__, __LINE__, function);

	#define TRACY_ZoneNamedRTBegin(name, function)                                                                       \
		tracy::ZoneRTBegin(___tracy_alloc_srcloc_name(                                                                     \
				__LINE__, __FILE__, strlen(__FILE__), function, strlen(function), name, strlen(name)));

	#define TRACY_ZoneBegin(name, function)                                                                              \
		static constexpr ___tracy_source_location_data TracyConcat(__tracy_source_location, __LINE__) {                    \
			name "", function, __FILE__, uint32_t(__LINE__), 0,                                                              \
		}
	#define TRACY_ZoneEnd() tracy::ZoneEnd

	#define GAIA_PROF_START_IMPL(name, function)                                                                         \
		TRACY_ZoneBegin(name, function);                                                                                   \
		tracy::ZoneBegin(&TracyConcat(__tracy_source_location, __LINE__));

	#define GAIA_PROF_STOP_IMPL() TRACY_ZoneEnd()

	#define GAIA_PROF_SCOPE_IMPL(name) ZoneNamedN(GAIA_CONCAT(___tracy_scoped_zone_, __LINE__), name "", 1)
	#define GAIA_PROF_SCOPE_DYN_IMPL(name) TRACY_ZoneNamedRT(name, GAIA_PRETTY_FUNCTION)

//------------------------------------------------------------------------
// Tracy profiler GAIA implementation
//------------------------------------------------------------------------

	//! Marks the end of frame
	#if !defined(GAIA_PROF_FRAME)
		#define GAIA_PROF_FRAME() FrameMark
	#endif
	//! Profiling zone bounded by the scope. The zone is named after a unique compile-time string
	#if !defined(GAIA_PROF_SCOPE)
		#define GAIA_PROF_SCOPE(zoneName) GAIA_PROF_SCOPE_IMPL(#zoneName)
	#endif
	//! Profiling zone bounded by the scope. The zone is named after a run-time string
	#if !defined(GAIA_PROF_SCOPE2)
		#define GAIA_PROF_SCOPE2(zoneName) GAIA_PROF_SCOPE_DYN_IMPL(zoneName)
	#endif
	//! Profiling zone with user-defined scope - start. The zone is named after a unique compile-time string
	#if !defined(GAIA_PROF_START)
		#define GAIA_PROF_START(zoneName) GAIA_PROF_START_IMPL(#zoneName, GAIA_PRETTY_FUNCTION)
	#endif
	//! Profiling zone with user-defined scope - stop.
	#if !defined(GAIA_PROF_STOP)
		#define GAIA_PROF_STOP() GAIA_PROF_STOP_IMPL()
	#endif
#else
	//! Marks the end of frame
	#if !defined(GAIA_PROF_FRAME)
		#define GAIA_PROF_FRAME()
	#endif
	//! Profiling zone bounded by the scope. The zone is named after a unique compile-time string
	#if !defined(GAIA_PROF_SCOPE)
		#define GAIA_PROF_SCOPE(zoneName)
	#endif
	//! Profiling zone bounded by the scope. The zone is named after a run-time string
	#if !defined(GAIA_PROF_SCOPE2)
		#define GAIA_PROF_SCOPE2(zoneName)
	#endif
	//! Profiling zone with user-defined scope - start. The zone is named after a unique compile-time string
	#if !defined(GAIA_PROF_START)
		#define GAIA_PROF_START(zoneName)
	#endif
	//! Profiling zone with user-defined scope - stop.
	#if !defined(GAIA_PROF_STOP)
		#define GAIA_PROF_STOP()
	#endif
#endif

#if GAIA_PROFILER_MEM
	//! Marks a memory allocation event. The event is named after a unique compile-time string
	#if !defined(GAIA_PROF_ALLOC)
		#define GAIA_PROF_ALLOC(ptr, size) TracyAlloc(ptr, size)
	#endif
	//! Marks a memory allocation event. The event is named after a run-time string
	#if !defined(GAIA_PROF_ALLOC2)
		#define GAIA_PROF_ALLOC2(ptr, size, name) TracyAllocN(ptr, size, name)
	#endif
	//! Marks a memory release event. The event is named after a unique compile-time string
	#if !defined(GAIA_PROF_FREE)
		#define GAIA_PROF_FREE(ptr) TracyFree(ptr)
	#endif
	//! Marks a memory release event. The event is named after a run-time string
	#if !defined(GAIA_PROF_FREE2)
		#define GAIA_PROF_FREE2(ptr, name) TracyFreeN(ptr, name)
	#endif
#else
	//! Marks a memory allocation event. The event is named after a unique compile-time string
	#if !defined(GAIA_PROF_ALLOC)
		#define GAIA_PROF_ALLOC(ptr, size)
	#endif
	//! Marks a memory allocation event. The event is named after a run-time string
	#if !defined(GAIA_PROF_ALLOC2)
		#define GAIA_PROF_ALLOC2(ptr, size, name)
	#endif
	//! Marks a memory release event. The event is named after a unique compile-time string
	#if !defined(GAIA_PROF_FREE)
		#define GAIA_PROF_FREE(ptr)
	#endif
	//! Marks a memory release event. The event is named after a run-time string
	#if !defined(GAIA_PROF_FREE2)
		#define GAIA_PROF_FREE2(ptr, name)
	#endif
#endif

#if GAIA_USE_STD_SPAN
	#include <span>
#else
	
/*
This is an implementation of C++20's std::span
http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2019/n4820.pdf
*/

//          Copyright Tristan Brindle 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file ../../LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <tuple>
#include <type_traits>

#ifndef TCB_SPAN_NO_EXCEPTIONS
	// Attempt to discover whether we're being compiled with exception support
	#if !(defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND))
		#define TCB_SPAN_NO_EXCEPTIONS
	#endif
#endif

#ifndef TCB_SPAN_NO_EXCEPTIONS
	#include <cstdio>
	#include <stdexcept>
#endif

namespace gaia {
	namespace cnt {
		template <typename T, uint32_t N>
		class sarr;
		template <typename T, uint32_t N>
		using sarray = cnt::sarr<T, N>;
	} // namespace cnt
} // namespace gaia

// Various feature test macros

#ifndef TCB_SPAN_NAMESPACE_NAME
	#define TCB_SPAN_NAMESPACE_NAME gaia::utils
#endif

#if GAIA_CPP_VERSION(201703L)
	#define TCB_SPAN_HAVE_CPP17
#endif

#if GAIA_CPP_VERSION(201402L)
	#define TCB_SPAN_HAVE_CPP14
#endif

namespace TCB_SPAN_NAMESPACE_NAME {

// Establish default contract checking behavior
#if !defined(TCB_SPAN_THROW_ON_CONTRACT_VIOLATION) && !defined(TCB_SPAN_TERMINATE_ON_CONTRACT_VIOLATION) &&            \
		!defined(TCB_SPAN_NO_CONTRACT_CHECKING)
	#if defined(NDEBUG) || !defined(TCB_SPAN_HAVE_CPP14)
		#define TCB_SPAN_NO_CONTRACT_CHECKING
	#else
		#define TCB_SPAN_TERMINATE_ON_CONTRACT_VIOLATION
	#endif
#endif

#if defined(TCB_SPAN_THROW_ON_CONTRACT_VIOLATION)
	struct contract_violation_error: std::logic_error {
		explicit contract_violation_error(const char* msg): std::logic_error(msg) {}
	};

	inline void contract_violation(const char* msg) {
		throw contract_violation_error(msg);
	}

#elif defined(TCB_SPAN_TERMINATE_ON_CONTRACT_VIOLATION)
	[[noreturn]] inline void contract_violation(const char* /*unused*/) {
		std::terminate();
	}
#endif

#if !defined(TCB_SPAN_NO_CONTRACT_CHECKING)
	#define TCB_SPAN_STRINGIFY(cond) #cond
	#define TCB_SPAN_EXPECT(cond) cond ? (void)0 : contract_violation("Expected " TCB_SPAN_STRINGIFY(cond))
#else
	#define TCB_SPAN_EXPECT(cond)
#endif

#if defined(TCB_SPAN_HAVE_CPP17) || defined(__cpp_inline_variables)
	#define TCB_SPAN_INLINE_VAR inline
#else
	#define TCB_SPAN_INLINE_VAR
#endif

#if defined(TCB_SPAN_HAVE_CPP14) || (defined(__cpp_constexpr) && __cpp_constexpr >= 201304)
	#define TCB_SPAN_HAVE_CPP14_CONSTEXPR
#endif

#if defined(TCB_SPAN_HAVE_CPP14_CONSTEXPR)
	#define TCB_SPAN_CONSTEXPR14 constexpr
#else
	#define TCB_SPAN_CONSTEXPR14
#endif

#if defined(TCB_SPAN_HAVE_CPP14_CONSTEXPR) && (!defined(_MSC_VER) || _MSC_VER > 1900)
	#define TCB_SPAN_CONSTEXPR_ASSIGN constexpr
#else
	#define TCB_SPAN_CONSTEXPR_ASSIGN
#endif

#if defined(TCB_SPAN_NO_CONTRACT_CHECKING)
	#define TCB_SPAN_CONSTEXPR11 constexpr
#else
	#define TCB_SPAN_CONSTEXPR11 TCB_SPAN_CONSTEXPR14
#endif

#if defined(TCB_SPAN_HAVE_CPP17) || defined(__cpp_deduction_guides)
	#define TCB_SPAN_HAVE_DEDUCTION_GUIDES
#endif

#if defined(TCB_SPAN_HAVE_CPP17) || defined(__cpp_lib_byte)
	#define TCB_SPAN_HAVE_STD_BYTE
#endif

#if defined(TCB_SPAN_HAVE_CPP17) || defined(__cpp_lib_array_constexpr)
	#define TCB_SPAN_HAVE_CONSTEXPR_STD_ARRAY_ETC
#endif

#if defined(TCB_SPAN_HAVE_CONSTEXPR_STD_ARRAY_ETC)
	#define TCB_SPAN_ARRAY_CONSTEXPR constexpr
#else
	#define TCB_SPAN_ARRAY_CONSTEXPR
#endif

#ifdef TCB_SPAN_HAVE_STD_BYTE
	using byte = std::byte;
#else
	using byte = unsigned char;
#endif

#if defined(TCB_SPAN_HAVE_CPP17)
	#define TCB_SPAN_NODISCARD [[nodiscard]]
#else
	#define TCB_SPAN_NODISCARD
#endif

	TCB_SPAN_INLINE_VAR constexpr std::size_t dynamic_extent = SIZE_MAX;

	template <typename ElementKind, std::size_t Extent = dynamic_extent>
	class span;

	namespace detail {

		template <typename E, std::size_t S>
		struct span_storage {
			constexpr span_storage() noexcept = default;

			constexpr span_storage(E* p_ptr, std::size_t /*unused*/) noexcept: ptr(p_ptr) {}

			E* ptr = nullptr;
			static constexpr std::size_t size = S;
		};

		template <typename E>
		struct span_storage<E, dynamic_extent> {
			constexpr span_storage() noexcept = default;

			constexpr span_storage(E* p_ptr, std::size_t p_size) noexcept: ptr(p_ptr), size(p_size) {}

			E* ptr = nullptr;
			std::size_t size = 0;
		};

// Reimplementation of C++17 std::size() and std::data()
#if 0 // defined(TCB_SPAN_HAVE_CPP17) || defined(__cpp_lib_nonmember_container_access)
		using std::data;
		using std::size;
#else
		template <typename C>
		constexpr auto size(const C& c) -> decltype(c.size()) {
			return c.size();
		}

		template <typename T, std::size_t N>
		constexpr std::size_t size(const T (&)[N]) noexcept {
			return N;
		}

		template <typename C>
		constexpr auto data(C& c) -> decltype(c.data()) {
			return c.data();
		}

		template <typename C>
		constexpr auto data(const C& c) -> decltype(c.data()) {
			return c.data();
		}

		template <typename T, std::size_t N>
		constexpr T* data(T (&array)[N]) noexcept {
			return array;
		}

		template <typename E>
		constexpr const E* data(std::initializer_list<E> il) noexcept {
			return il.begin();
		}
#endif // TCB_SPAN_HAVE_CPP17

#if defined(TCB_SPAN_HAVE_CPP17) || defined(__cpp_lib_void_t)
		using std::void_t;
#else
		template <typename...>
		using void_t = void;
#endif

		template <typename T>
		using uncvref_t = typename std::remove_cv<typename std::remove_reference<T>::type>::type;

		template <typename>
		struct is_span: std::false_type {};

		template <typename T, std::size_t S>
		struct is_span<span<T, S>>: std::true_type {};

		template <typename>
		struct is_std_array: std::false_type {};

		template <typename T, auto N>
		struct is_std_array<gaia::cnt::sarray<T, N>>: std::true_type {};

		template <typename, typename = void>
		struct has_size_and_data: std::false_type {};

		template <typename T>
		struct has_size_and_data<
				T, void_t<decltype(detail::size(std::declval<T>())), decltype(detail::data(std::declval<T>()))>>:
				std::true_type {};

		template <typename C, typename U = uncvref_t<C>>
		struct is_container {
			static constexpr bool value =
					!is_span<U>::value && !is_std_array<U>::value && !std::is_array<U>::value && has_size_and_data<C>::value;
		};

		template <typename T>
		using remove_pointer_t = typename std::remove_pointer<T>::type;

		template <typename, typename, typename = void>
		struct is_container_element_kind_compatible: std::false_type {};

		template <typename T, typename E>
		struct is_container_element_kind_compatible<
				T, E,
				typename std::enable_if<
						!std::is_same<typename std::remove_cv<decltype(detail::data(std::declval<T>()))>::type, void>::value &&
						std::is_convertible<remove_pointer_t<decltype(detail::data(std::declval<T>()))> (*)[], E (*)[]>::value>::
						type>: std::true_type {};

		template <typename, typename = size_t>
		struct is_complete: std::false_type {};

		template <typename T>
		struct is_complete<T, decltype(sizeof(T))>: std::true_type {};

	} // namespace detail

	template <typename ElementKind, std::size_t Extent>
	class span {
		static_assert(
				std::is_object<ElementKind>::value, "A span's ElementKind must be an object type (not a "
																						"reference type or void)");
		static_assert(
				detail::is_complete<ElementKind>::value, "A span's ElementKind must be a complete type (not a forward "
																								 "declaration)");
		static_assert(!std::is_abstract<ElementKind>::value, "A span's ElementKind cannot be an abstract class type");

		using storage_type = detail::span_storage<ElementKind, Extent>;

	public:
		// constants and types
		using element_kind = ElementKind;
		using value_type = typename std::remove_cv<ElementKind>::type;
		using size_type = std::size_t;
		using difference_type = std::ptrdiff_t;
		using pointer = element_kind*;
		using const_pointer = const element_kind*;
		using reference = element_kind&;
		using const_reference = const element_kind&;
		using iterator = pointer;
		// using reverse_iterator = std::reverse_iterator<iterator>;

		static constexpr size_type extent = Extent;

		// [span.cons], span constructors, copy, assignment, and destructor
		template <std::size_t E = Extent, typename std::enable_if<(E == dynamic_extent || E <= 0), int>::type = 0>
		constexpr span() noexcept {}

		TCB_SPAN_CONSTEXPR11 span(pointer ptr, size_type count): storage_(ptr, count) {
			TCB_SPAN_EXPECT(extent == dynamic_extent || count == extent);
		}

		TCB_SPAN_CONSTEXPR11 span(pointer first_elem, pointer last_elem): storage_(first_elem, last_elem - first_elem) {
			TCB_SPAN_EXPECT(extent == dynamic_extent || last_elem - first_elem == static_cast<std::ptrdiff_t>(extent));
		}

		template <
				std::size_t N, std::size_t E = Extent,
				typename std::enable_if<
						(E == dynamic_extent || N == E) &&
								detail::is_container_element_kind_compatible<element_kind (&)[N], ElementKind>::value,
						int>::type = 0>
		constexpr span(element_kind (&arr)[N]) noexcept: storage_(arr, N) {}

		// template <
		// 		typename T, std::size_t N, std::size_t E = Extent,
		// 		typename std::enable_if<
		// 				(E == dynamic_extent || N == E) &&
		// 						detail::is_container_element_kind_compatible<gaia::cnt::sarray<T, N>&, ElementKind>::value,
		// 				int>::type = 0>
		// TCB_SPAN_ARRAY_CONSTEXPR span(gaia::cnt::sarray<T, N>& arr) noexcept: storage_(arr.data(), N) {}

		// template <
		// 		typename T, std::size_t N, std::size_t E = Extent,
		// 		typename std::enable_if<
		// 				(E == dynamic_extent || N == E) &&
		// 						detail::is_container_element_kind_compatible<const gaia::cnt::sarray<T, N>&,
		// ElementKind>::value, 				int>::type = 0> TCB_SPAN_ARRAY_CONSTEXPR span(const gaia::cnt::sarray<T, N>&
		// arr) noexcept: storage_(arr.data(), N) {}

		template <
				typename Container, std::size_t E = Extent,
				typename std::enable_if<
						E == dynamic_extent && detail::is_container<Container>::value &&
								detail::is_container_element_kind_compatible<Container&, ElementKind>::value,
						int>::type = 0>
		constexpr span(Container& cont): storage_(detail::data(cont), detail::size(cont)) {}

		template <
				typename Container, std::size_t E = Extent,
				typename std::enable_if<
						E == dynamic_extent && detail::is_container<Container>::value &&
								detail::is_container_element_kind_compatible<const Container&, ElementKind>::value,
						int>::type = 0>
		constexpr span(const Container& cont): storage_(detail::data(cont), detail::size(cont)) {}

		constexpr span(const span& other) noexcept = default;

		template <
				typename OtherElementKind, std::size_t OtherExtent,
				typename std::enable_if<
						(Extent == dynamic_extent || OtherExtent == dynamic_extent || Extent == OtherExtent) &&
								std::is_convertible<OtherElementKind (*)[], ElementKind (*)[]>::value,
						int>::type = 0>
		constexpr span(const span<OtherElementKind, OtherExtent>& other) noexcept: storage_(other.data(), other.size()) {}

		~span() noexcept = default;

		TCB_SPAN_CONSTEXPR_ASSIGN span& operator=(const span& other) noexcept = default;

		// [span.sub], span subviews
		template <std::size_t Count>
		TCB_SPAN_CONSTEXPR11 span<element_kind, Count> first() const {
			TCB_SPAN_EXPECT(Count <= size());
			return {data(), Count};
		}

		template <std::size_t Count>
		TCB_SPAN_CONSTEXPR11 span<element_kind, Count> last() const {
			TCB_SPAN_EXPECT(Count <= size());
			return {data() + (size() - Count), Count};
		}

		template <std::size_t Offset, std::size_t Count = dynamic_extent>
		using subspan_return_t = span<
				ElementKind, Count != dynamic_extent ? Count : (Extent != dynamic_extent ? Extent - Offset : dynamic_extent)>;

		template <std::size_t Offset, std::size_t Count = dynamic_extent>
		TCB_SPAN_CONSTEXPR11 subspan_return_t<Offset, Count> subspan() const {
			TCB_SPAN_EXPECT(Offset <= size() && (Count == dynamic_extent || Offset + Count <= size()));
			return {data() + Offset, Count != dynamic_extent ? Count : size() - Offset};
		}

		TCB_SPAN_CONSTEXPR11 span<element_kind, dynamic_extent> first(size_type count) const {
			TCB_SPAN_EXPECT(count <= size());
			return {data(), count};
		}

		TCB_SPAN_CONSTEXPR11 span<element_kind, dynamic_extent> last(size_type count) const {
			TCB_SPAN_EXPECT(count <= size());
			return {data() + (size() - count), count};
		}

		TCB_SPAN_CONSTEXPR11 span<element_kind, dynamic_extent>
		subspan(size_type offset, size_type count = dynamic_extent) const {
			TCB_SPAN_EXPECT(offset <= size() && (count == dynamic_extent || offset + count <= size()));
			return {data() + offset, count == dynamic_extent ? size() - offset : count};
		}

		// [span.obs], span observers
		constexpr size_type size() const noexcept {
			return storage_.size;
		}

		constexpr size_type bytes() const noexcept {
			return size() * sizeof(element_kind);
		}

		TCB_SPAN_NODISCARD constexpr bool empty() const noexcept {
			return size() == 0;
		}

		// [span.elem], span element access
		TCB_SPAN_CONSTEXPR11 reference operator[](size_type idx) const {
			TCB_SPAN_EXPECT(idx < size());
			return *(data() + idx);
		}

		TCB_SPAN_CONSTEXPR11 reference front() const {
			TCB_SPAN_EXPECT(!empty());
			return *data();
		}

		TCB_SPAN_CONSTEXPR11 reference back() const {
			TCB_SPAN_EXPECT(!empty());
			return *(data() + (size() - 1));
		}

		constexpr pointer data() const noexcept {
			return storage_.ptr;
		}

		// [span.iterators], span iterator support
		constexpr iterator begin() const noexcept {
			return data();
		}

		constexpr iterator end() const noexcept {
			return data() + size();
		}

		// TCB_SPAN_ARRAY_CONSTEXPR reverse_iterator rbegin() const noexcept {
		// 	return reverse_iterator(end());
		// }

		// TCB_SPAN_ARRAY_CONSTEXPR reverse_iterator rend() const noexcept {
		// 	return reverse_iterator(begin());
		// }

	private:
		storage_type storage_{};
	};

#ifdef TCB_SPAN_HAVE_DEDUCTION_GUIDES

	/* Deduction Guides */
	template <typename T, size_t N>
	span(T (&)[N]) -> span<T, N>;

	template <typename T, size_t N>
	span(gaia::cnt::sarray<T, N>&) -> span<T, N>;

	template <typename T, size_t N>
	span(const gaia::cnt::sarray<T, N>&) -> span<const T, N>;

	template <typename Container>
	span(Container&) -> span<typename std::remove_reference<decltype(*detail::data(std::declval<Container&>()))>::type>;

	template <typename Container>
	span(const Container&) -> span<const typename Container::value_type>;

#endif // TCB_HAVE_DEDUCTION_GUIDES

	template <typename ElementKind, std::size_t Extent>
	constexpr span<ElementKind, Extent> make_span(span<ElementKind, Extent> s) noexcept {
		return s;
	}

	template <typename T, std::size_t N>
	constexpr span<T, N> make_span(T (&arr)[N]) noexcept {
		return {arr};
	}

	template <typename T, std::size_t N>
	TCB_SPAN_ARRAY_CONSTEXPR span<T, N> make_span(gaia::cnt::sarray<T, N>& arr) noexcept {
		return {arr};
	}

	template <typename T, std::size_t N>
	TCB_SPAN_ARRAY_CONSTEXPR span<const T, N> make_span(const gaia::cnt::sarray<T, N>& arr) noexcept {
		return {arr};
	}

	template <typename Container>
	constexpr span<typename std::remove_reference<decltype(*detail::data(std::declval<Container&>()))>::type>
	make_span(Container& cont) {
		return {cont};
	}

	template <typename Container>
	constexpr span<const typename Container::value_type> make_span(const Container& cont) {
		return {cont};
	}

	template <typename ElementKind, std::size_t Extent>
	span<const byte, ((Extent == dynamic_extent) ? dynamic_extent : sizeof(ElementKind) * Extent)>
	as_bytes(span<ElementKind, Extent> s) noexcept {
		return {reinterpret_cast<const byte*>(s.data()), s.bytes()};
	}

	template <
			class ElementKind, size_t Extent, typename std::enable_if<!std::is_const<ElementKind>::value, int>::type = 0>
	span<byte, ((Extent == dynamic_extent) ? dynamic_extent : sizeof(ElementKind) * Extent)>
	as_writable_bytes(span<ElementKind, Extent> s) noexcept {
		return {reinterpret_cast<byte*>(s.data()), s.bytes()};
	}

	template <std::size_t N, typename E, std::size_t S>
	constexpr auto get(span<E, S> s) -> decltype(s[N]) {
		return s[N];
	}

} // namespace TCB_SPAN_NAMESPACE_NAME

namespace std {

	template <typename ElementKind, size_t Extent>
	struct tuple_size<TCB_SPAN_NAMESPACE_NAME::span<ElementKind, Extent>>: public integral_constant<size_t, Extent> {};

	template <typename ElementKind>
	struct tuple_size<TCB_SPAN_NAMESPACE_NAME::span<ElementKind, TCB_SPAN_NAMESPACE_NAME::dynamic_extent>>; // not defined

	template <size_t I, typename ElementKind, size_t Extent>
	struct tuple_element<I, TCB_SPAN_NAMESPACE_NAME::span<ElementKind, Extent>> {
		static_assert(Extent != TCB_SPAN_NAMESPACE_NAME::dynamic_extent && I < Extent, "");
		using type = ElementKind;
	};

} // end namespace std

namespace std {
	using TCB_SPAN_NAMESPACE_NAME::span;
}
#endif

#include <tuple>
#include <type_traits>
#include <utility>

#include <cstddef>
#include <type_traits>

namespace gaia {
	namespace core {
		struct input_iterator_tag {};
		struct output_iterator_tag {};

		struct forward_iterator_tag: input_iterator_tag {};
		struct reverse_iterator_tag: input_iterator_tag {};
		struct bidirectional_iterator_tag: forward_iterator_tag {};
		struct random_access_iterator_tag: bidirectional_iterator_tag {};
		struct contiguous_iterator_tag: random_access_iterator_tag {};

		namespace detail {

			template <typename, typename = void>
			struct iterator_traits_base {}; // empty for non-iterators

			template <typename It>
			struct iterator_traits_base<
					It, std::void_t<
									typename It::iterator_category, typename It::value_type, typename It::difference_type,
									typename It::pointer, typename It::reference>> {
				using iterator_category = typename It::iterator_category;
				using value_type = typename It::value_type;
				using difference_type = typename It::difference_type;
				using pointer = typename It::pointer;
				using reference = typename It::reference;
			};

			template <typename T, bool = std::is_object_v<T>>
			struct iterator_traits_pointer_base {
				using iterator_category = random_access_iterator_tag;
				using value_type = std::remove_cv_t<T>;
				using difference_type = std::ptrdiff_t;
				using pointer = T*;
				using reference = T&;
			};

			//! Iterator traits for pointers to non-object
			template <typename T>
			struct iterator_traits_pointer_base<T, false> {};

			//! Iterator traits for iterators
			template <typename It>
			struct iterator_traits: iterator_traits_base<It> {};

			// Iterator traits for pointers
			template <typename T>
			struct iterator_traits<T*>: iterator_traits_pointer_base<T> {};

			template <typename It>
			using iterator_cat_t = typename iterator_traits<It>::iterator_category;

			template <typename T, typename = void>
			[[maybe_unused]] constexpr bool is_iterator_v = false;

			template <typename T>
			[[maybe_unused]] constexpr bool is_iterator_v<T, std::void_t<iterator_cat_t<T>>> = true;

			template <typename T>
			struct is_iterator: std::bool_constant<is_iterator_v<T>> {};

			template <typename It>
			[[maybe_unused]] constexpr bool is_input_iter_v = std::is_convertible_v<iterator_cat_t<It>, input_iterator_tag>;

			template <typename It>
			[[maybe_unused]] constexpr bool is_fwd_iter_v = std::is_convertible_v<iterator_cat_t<It>, forward_iterator_tag>;

			template <typename It>
			[[maybe_unused]] constexpr bool is_rev_iter_v = std::is_convertible_v<iterator_cat_t<It>, reverse_iterator_tag>;

			template <typename It>
			[[maybe_unused]] constexpr bool is_bidi_iter_v = std::is_convertible_v<iterator_cat_t<It>, bidirectional_iterator_tag>;

			template <typename It>
			[[maybe_unused]] constexpr bool is_random_iter_v =
					std::is_convertible_v<iterator_cat_t<It>, random_access_iterator_tag>;
		} // namespace detail

		template <typename It>
		using iterator_ref_t = typename detail::iterator_traits<It>::reference;

		template <typename It>
		using iterator_value_t = typename detail::iterator_traits<It>::value_type;

		template <typename It>
		using iterator_diff_t = typename detail::iterator_traits<It>::difference_type;

		template <typename... It>
		using common_diff_t = std::common_type_t<iterator_diff_t<It>...>;

		template <typename It>
		constexpr iterator_diff_t<It> distance(It first, It last) {
			if constexpr (std::is_pointer_v<It> || detail::is_random_iter_v<It>)
				return last - first;
			else {
				iterator_diff_t<It> offset{};
				while (first != last) {
					++first;
					++offset;
				}
				return offset;
			}
		}
	} // namespace core
} // namespace gaia

namespace gaia {
	constexpr uint32_t BadIndex = uint32_t(-1);

	namespace core {
		//----------------------------------------------------------------------
		// Bit-byte conversion
		//----------------------------------------------------------------------

		template <typename T>
		constexpr T as_bits(T value) {
			static_assert(std::is_integral_v<T>);
			return value * 8;
		}

		template <typename T>
		constexpr T as_bytes(T value) {
			static_assert(std::is_integral_v<T>);
			return value / 8;
		}

		//----------------------------------------------------------------------
		// Memory size helpers
		//----------------------------------------------------------------------

		template <typename T>
		constexpr uint32_t count_bits(T number) {
			uint32_t bits_needed = 0;
			while (number > 0) {
				number >>= 1;
				++bits_needed;
			}
			return bits_needed;
		}

		//----------------------------------------------------------------------
		// Element construction / destruction
		//----------------------------------------------------------------------

		template <typename T>
		void call_ctor(T* pData, size_t cnt) {
			if constexpr (!std::is_trivially_constructible_v<T>) {
				for (size_t i = 0; i < cnt; ++i)
					(void)::new (pData + i) T();
			}
		}

		template <typename T>
		void call_dtor(T* pData, size_t cnt) {
			if constexpr (!std::is_trivially_constructible_v<T>) {
				for (size_t i = 0; i < cnt; ++i)
					pData[i].~T();
			}
		}

		//----------------------------------------------------------------------
		// Element swapping
		//----------------------------------------------------------------------

		template <typename T>
		constexpr void swap(T& left, T& right) {
			T tmp = std::move(left);
			left = std::move(right);
			right = std::move(tmp);
		}

		template <typename T, typename TCmpFunc>
		constexpr void swap_if(T& lhs, T& rhs, TCmpFunc cmpFunc) noexcept {
			if (!cmpFunc(lhs, rhs))
				core::swap(lhs, rhs);
		}

		template <typename T, typename TCmpFunc>
		constexpr void swap_if_not(T& lhs, T& rhs, TCmpFunc cmpFunc) noexcept {
			if (cmpFunc(lhs, rhs))
				core::swap(lhs, rhs);
		}

		template <typename C, typename TCmpFunc, typename TSortFunc>
		constexpr void try_swap_if(
				C& c, typename C::size_type lhs, typename C::size_type rhs, TCmpFunc cmpFunc, TSortFunc sortFunc) noexcept {
			if (!cmpFunc(c[lhs], c[rhs]))
				sortFunc(lhs, rhs);
		}

		template <typename C, typename TCmpFunc, typename TSortFunc>
		constexpr void try_swap_if_not(
				C& c, typename C::size_type lhs, typename C::size_type rhs, TCmpFunc cmpFunc, TSortFunc sortFunc) noexcept {
			if (cmpFunc(c[lhs], c[rhs]))
				sortFunc(lhs, rhs);
		}

		//----------------------------------------------------------------------
		// Value filling
		//----------------------------------------------------------------------

		template <class ForwardIt, class T>
		constexpr void fill(ForwardIt first, ForwardIt last, const T& value) {
			for (; first != last; ++first) {
				*first = value;
			}
		}

		//----------------------------------------------------------------------
		// Value range checking
		//----------------------------------------------------------------------

		template <class T>
		constexpr const T& get_min(const T& a, const T& b) {
			return (b < a) ? b : a;
		}

		template <class T>
		constexpr const T& get_max(const T& a, const T& b) {
			return (b > a) ? b : a;
		}

		//----------------------------------------------------------------------
		// Checking if a template arg is unique among the rest
		//----------------------------------------------------------------------

		template <typename...>
		inline constexpr auto is_unique = std::true_type{};

		template <typename T, typename... Rest>
		inline constexpr auto is_unique<T, Rest...> =
				std::bool_constant<(!std::is_same_v<T, Rest> && ...) && is_unique<Rest...>>{};

		namespace detail {
			template <typename T>
			struct type_identity {
				using type = T;
			};
		} // namespace detail

		template <typename T, typename... Ts>
		struct unique: detail::type_identity<T> {}; // TODO: In C++20 we could use std::type_identity

		template <typename... Ts, typename U, typename... Us>
		struct unique<std::tuple<Ts...>, U, Us...>:
				std::conditional_t<
						(std::is_same_v<U, Ts> || ...), unique<std::tuple<Ts...>, Us...>, unique<std::tuple<Ts..., U>, Us...>> {};

		template <typename... Ts>
		using unique_tuple = typename unique<std::tuple<>, Ts...>::type;

		//----------------------------------------------------------------------
		// Calculating total size of all types of tuple
		//----------------------------------------------------------------------

		template <typename... Args>
		constexpr unsigned get_args_size(std::tuple<Args...> const& /*no_name*/) {
			return (sizeof(Args) + ...);
		}

		//----------------------------------------------------------------------
		// Member function checks
		//----------------------------------------------------------------------

		template <typename... Type>
		struct func_type_list {};

		template <typename Class, typename Ret, typename... Args>
		func_type_list<Args...> func_args(Ret (Class::*)(Args...) const);

#define GAIA_DEFINE_HAS_FUNCTION(function_name)                                                                        \
	template <typename T, typename... Args>                                                                              \
	constexpr auto has_##function_name##_check(int)                                                                      \
			-> decltype(std::declval<T>().function_name(std::declval<Args>()...), std::true_type{});                         \
                                                                                                                       \
	template <typename T, typename... Args>                                                                              \
	constexpr std::false_type has_##function_name##_check(...);                                                          \
                                                                                                                       \
	template <typename T, typename... Args>                                                                              \
	struct has_##function_name {                                                                                         \
		static constexpr bool value = decltype(has_##function_name##_check<T, Args...>(0))::value;                         \
	};

		GAIA_DEFINE_HAS_FUNCTION(find)
		GAIA_DEFINE_HAS_FUNCTION(find_if)
		GAIA_DEFINE_HAS_FUNCTION(find_if_not)

		//----------------------------------------------------------------------
		// Type helpers
		//----------------------------------------------------------------------

		template <typename... Type>
		struct type_list {
			using types = type_list;
			static constexpr auto size = sizeof...(Type);
		};

		template <typename TypesA, typename TypesB>
		struct type_list_concat;

		template <typename... TypesA, typename... TypesB>
		struct type_list_concat<type_list<TypesA...>, type_list<TypesB...>> {
			using type = type_list<TypesA..., TypesB...>;
		};

		//----------------------------------------------------------------------
		// Compile - time for each
		//----------------------------------------------------------------------

		namespace detail {
			template <auto FirstIdx, auto Iters, typename Func, auto... Is>
			constexpr void each_impl(Func func, std::integer_sequence<decltype(Iters), Is...> /*no_name*/) {
				if constexpr ((std::is_invocable_v<Func&&, std::integral_constant<decltype(Is), Is>> && ...))
					(func(std::integral_constant<decltype(Is), FirstIdx + Is>{}), ...);
				else
					(((void)Is, func()), ...);
			}

			template <auto FirstIdx, typename Tuple, typename Func, auto... Is>
			void each_tuple_impl(Func func, std::index_sequence<Is...> /*no_name*/) {
				if constexpr ((std::is_invocable_v<Func&&, std::integral_constant<decltype(Is), FirstIdx + Is>> && ...))
					(func(std::integral_constant<decltype(Is), FirstIdx + Is>{}), ...);
				else
					(func(std::tuple_element_t<FirstIdx + Is, Tuple>{}), ...);
			}

			template <auto FirstIdx, typename Tuple, typename Func, auto... Is>
			void each_tuple_impl(Tuple&& tuple, Func func, std::index_sequence<Is...> /*no_name*/) {
				(func(std::get<FirstIdx + Is>(tuple)), ...);
			}
		} // namespace detail

		//----------------------------------------------------------------------
		// Looping
		//----------------------------------------------------------------------

		//! Compile-time for loop. Performs \tparam Iters iterations.
		//!
		//! Example 1 (index argument):
		//! sarray<int, 10> arr = { ... };
		//! each<arr.size()>([&arr](auto i) {
		//!    GAIA_LOG_N("%d", i);
		//! });
		//!
		//! Example 2 (no index argument):
		//! uint32_t cnt = 0;
		//! each<10>([&cnt]() {
		//!    GAIA_LOG_N("Invocation number: %u", cnt++);
		//! });
		template <auto Iters, typename Func>
		constexpr void each(Func func) {
			using TIters = decltype(Iters);
			constexpr TIters First = 0;
			detail::each_impl<First, Iters, Func>(func, std::make_integer_sequence<TIters, Iters>());
		}

		//! Compile-time for loop with adjustable range.
		//! Iteration starts at \tparam FirstIdx and ends at \tparam LastIdx (excluding).
		//!
		//! Example 1 (index argument):
		//! sarray<int, 10> arr;
		//! each_ext<0, 10>([&arr](auto i) {
		//!    GAIA_LOG_N("%d", i);
		//! });
		//!
		//! Example 2 (no argument):
		//! uint32_t cnt = 0;
		//! each_ext<0, 10>([&cnt]() {
		//!    GAIA_LOG_N("Invocation number: %u", cnt++);
		//! });
		template <auto FirstIdx, auto LastIdx, typename Func>
		constexpr void each_ext(Func func) {
			static_assert(LastIdx >= FirstIdx);
			const auto Iters = LastIdx - FirstIdx;
			detail::each_impl<FirstIdx, Iters, Func>(func, std::make_integer_sequence<decltype(Iters), Iters>());
		}

		//! Compile-time for loop with adjustable range and iteration size.
		//! Iteration starts at \tparam FirstIdx and ends at \tparam LastIdx
		//! (excluding) at increments of \tparam Inc.
		//!
		//! Example 1 (index argument):
		//! sarray<int, 10> arr;
		//! each_ext<0, 10, 2>([&arr](auto i) {
		//!    GAIA_LOG_N("%d", i);
		//! });
		//!
		//! Example 2 (no argument):
		//! uint32_t cnt = 0;
		//! each_ext<0, 10, 2>([&cnt]() {
		//!    GAIA_LOG_N("Invocation number: %u", cnt++);
		//! });
		template <auto FirstIdx, auto LastIdx, auto Inc, typename Func>
		constexpr void each_ext(Func func) {
			if constexpr (FirstIdx < LastIdx) {
				if constexpr (std::is_invocable_v<Func&&, std::integral_constant<decltype(FirstIdx), FirstIdx>>)
					func(std::integral_constant<decltype(FirstIdx), FirstIdx>());
				else
					func();

				each_ext<FirstIdx + Inc, LastIdx, Inc>(func);
			}
		}

		//! Compile-time for loop over parameter packs.
		//!
		//! Example:
		//! template<typename... Args>
		//! void print(const Args&... args) {
		//!  each_pack([](const auto& value) {
		//!    std::cout << value << std::endl;
		//!  });
		//! }
		//! print(69, "likes", 420.0f);
		template <typename Func, typename... Args>
		constexpr void each_pack(Func func, Args&&... args) {
			(func(std::forward<Args>(args)), ...);
		}

		//! Compile-time for loop over tuples and other objects implementing
		//! tuple_size (sarray, std::pair etc).
		//!
		//! Example:
		//! each_tuple(
		//!		std::make_tuple(69, "likes", 420.0f),
		//!		[](const auto& value) {
		//! 		std::cout << value << std::endl;
		//! 	});
		template <typename Tuple, typename Func>
		constexpr void each_tuple(Tuple&& tuple, Func func) {
			constexpr auto TSize = std::tuple_size<std::remove_reference_t<Tuple>>::value;
			detail::each_tuple_impl<(size_t)0>(std::forward<Tuple>(tuple), func, std::make_index_sequence<TSize>{});
		}

		template <typename Tuple, typename Func>
		constexpr void each_tuple(Func func) {
			constexpr auto TSize = std::tuple_size<std::remove_reference_t<Tuple>>::value;
			detail::each_tuple_impl<(size_t)0, Tuple>(func, std::make_index_sequence<TSize>{});
		}

		//! Compile-time for loop over tuples and other objects implementing
		//! tuple_size (sarray, std::pair etc).
		//! Iteration starts at \tparam FirstIdx and ends at \tparam LastIdx (excluding).
		//!
		//! Example:
		//! each_tuple(
		//!		std::make_tuple(69, "likes", 420.0f),
		//!		[](const auto& value) {
		//! 		std::cout << value << std::endl;
		//! 	});
		template <auto FirstIdx, auto LastIdx, typename Tuple, typename Func>
		constexpr void each_tuple_ext(Tuple&& tuple, Func func) {
			constexpr auto TSize = std::tuple_size<std::remove_reference_t<Tuple>>::value;
			static_assert(LastIdx >= FirstIdx);
			static_assert(LastIdx <= TSize);
			constexpr auto Iters = LastIdx - FirstIdx;
			detail::each_tuple_impl<FirstIdx>(std::forward<Tuple>(tuple), func, std::make_index_sequence<Iters>{});
		}

		template <auto FirstIdx, auto LastIdx, typename Tuple, typename Func>
		constexpr void each_tuple_ext(Func func) {
			constexpr auto TSize = std::tuple_size<std::remove_reference_t<Tuple>>::value;
			static_assert(LastIdx >= FirstIdx);
			static_assert(LastIdx <= TSize);
			constexpr auto Iters = LastIdx - FirstIdx;
			detail::each_tuple_impl<FirstIdx, Tuple>(func, std::make_index_sequence<Iters>{});
		}

		template <typename InputIt, typename Func>
		constexpr Func each(InputIt first, InputIt last, Func func) {
			for (; first != last; ++first)
				func(*first);
			return func;
		}

		template <typename C, typename Func>
		constexpr auto each(const C& arr, Func func) {
			return each(arr.begin(), arr.end(), func);
		}

		//----------------------------------------------------------------------
		// Lookups
		//----------------------------------------------------------------------

		template <typename InputIt, typename T>
		constexpr InputIt find(InputIt first, InputIt last, const T& value) {
			if constexpr (std::is_pointer_v<InputIt>) {
				auto size = distance(first, last);
				for (decltype(size) i = 0; i < size; ++i) {
					if (first[i] == value)
						return &first[i];
				}
			} else if constexpr (std::is_same_v<typename InputIt::iterator_category, core::random_access_iterator_tag>) {
				auto size = distance(first, last);
				for (decltype(size) i = 0; i < size; ++i) {
					if (*(first[i]) == value)
						return first[i];
				}
			} else {
				for (; first != last; ++first) {
					if (*first == value)
						return first;
				}
			}
			return last;
		}

		template <typename C, typename V>
		constexpr auto find(const C& arr, const V& item) {
			if constexpr (has_find<C>::value)
				return arr.find(item);
			else
				return core::find(arr.begin(), arr.end(), item);
		}

		template <typename InputIt, typename Func>
		constexpr InputIt find_if(InputIt first, InputIt last, Func func) {
			if constexpr (std::is_pointer_v<InputIt>) {
				auto size = distance(first, last);
				for (decltype(size) i = 0; i < size; ++i) {
					if (func(first[i]))
						return &first[i];
				}
			} else if constexpr (std::is_same_v<typename InputIt::iterator_category, core::random_access_iterator_tag>) {
				auto size = distance(first, last);
				for (decltype(size) i = 0; i < size; ++i) {
					if (func(*(first[i])))
						return first[i];
				}
			} else {
				for (; first != last; ++first) {
					if (func(*first))
						return first;
				}
			}
			return last;
		}

		template <typename UnaryPredicate, typename C>
		constexpr auto find_if(const C& arr, UnaryPredicate predicate) {
			if constexpr (has_find_if<C, UnaryPredicate>::value)
				return arr.find_id(predicate);
			else
				return core::find_if(arr.begin(), arr.end(), predicate);
		}

		template <typename InputIt, typename Func>
		constexpr InputIt find_if_not(InputIt first, InputIt last, Func func) {
			if constexpr (std::is_pointer_v<InputIt>) {
				auto size = distance(first, last);
				for (decltype(size) i = 0; i < size; ++i) {
					if (!func(first[i]))
						return &first[i];
				}
			} else if constexpr (std::is_same_v<typename InputIt::iterator_category, core::random_access_iterator_tag>) {
				auto size = distance(first, last);
				for (decltype(size) i = 0; i < size; ++i) {
					if (!func(*(first[i])))
						return first[i];
				}
			} else {
				for (; first != last; ++first) {
					if (!func(*first))
						return first;
				}
			}
			return last;
		}

		template <typename UnaryPredicate, typename C>
		constexpr auto find_if_not(const C& arr, UnaryPredicate predicate) {
			if constexpr (has_find_if_not<C, UnaryPredicate>::value)
				return arr.find_if_not(predicate);
			else
				return core::find_if_not(arr.begin(), arr.end(), predicate);
		}

		//----------------------------------------------------------------------

		template <typename C, typename V>
		constexpr bool has(const C& arr, const V& item) {
			const auto it = find(arr, item);
			return it != arr.end();
		}

		template <typename UnaryPredicate, typename C>
		constexpr bool has_if(const C& arr, UnaryPredicate predicate) {
			const auto it = find_if(arr, predicate);
			return it != arr.end();
		}

		//----------------------------------------------------------------------

		template <typename C>
		constexpr auto get_index(const C& arr, typename C::const_reference item) {
			const auto it = find(arr, item);
			if (it == arr.end())
				return BadIndex;

			return (decltype(BadIndex))core::distance(arr.begin(), it);
		}

		template <typename C>
		constexpr auto get_index_unsafe(const C& arr, typename C::const_reference item) {
			return (decltype(BadIndex))core::distance(arr.begin(), find(arr, item));
		}

		template <typename UnaryPredicate, typename C>
		constexpr auto get_index_if(const C& arr, UnaryPredicate predicate) {
			const auto it = find_if(arr, predicate);
			if (it == arr.end())
				return BadIndex;

			return (decltype(BadIndex))core::distance(arr.begin(), it);
		}

		template <typename UnaryPredicate, typename C>
		constexpr auto get_index_if_unsafe(const C& arr, UnaryPredicate predicate) {
			return (decltype(BadIndex))core::distance(arr.begin(), find_if(arr, predicate));
		}

		//----------------------------------------------------------------------
		// Erasure
		//----------------------------------------------------------------------

		//! Replaces the item at \param idx in the array \param arr with the last item of the array if possible and removes
		//! its last item.
		//! Use when shifting of the entire erray is not wanted.
		//! \warning If the item order is important and the size of the array changes after calling this function you need
		//! to sort the array.
		template <typename C>
		void erase_fast(C& arr, typename C::size_type idx) {
			if (idx >= arr.size())
				return;

			if (idx + 1 != arr.size())
				arr[idx] = arr[arr.size() - 1];

			arr.pop_back();
		}

		//----------------------------------------------------------------------
		// Comparison
		//----------------------------------------------------------------------

		template <typename T>
		struct equal_to {
			constexpr bool operator()(const T& lhs, const T& rhs) const {
				return lhs == rhs;
			}
		};

		template <typename T>
		struct is_smaller {
			constexpr bool operator()(const T& lhs, const T& rhs) const {
				return lhs < rhs;
			}
		};

		template <typename T>
		struct is_greater {
			constexpr bool operator()(const T& lhs, const T& rhs) const {
				return lhs > rhs;
			}
		};

		//----------------------------------------------------------------------
		// Sorting
		//----------------------------------------------------------------------

		namespace detail {
			template <typename Array, typename TSortFunc>
			constexpr void comb_sort_impl(Array& array_, TSortFunc func) noexcept {
				constexpr double Factor = 1.247330950103979;
				using size_type = typename Array::size_type;

				size_type gap = array_.size();
				bool swapped = false;
				while ((gap > size_type{1}) || swapped) {
					if (gap > size_type{1}) {
						gap = static_cast<size_type>(gap / Factor);
					}
					swapped = false;
					for (size_type i = size_type{0}; gap + i < static_cast<size_type>(array_.size()); ++i) {
						if (!func(array_[i], array_[i + gap])) {
							auto swap = array_[i];
							array_[i] = array_[i + gap];
							array_[i + gap] = swap;
							swapped = true;
						}
					}
				}
			}

			template <typename Container, typename TSortFunc>
			int quick_sort_partition(Container& arr, TSortFunc func, int low, int high) {
				const auto& pivot = arr[high];
				int i = low - 1;
				for (int j = low; j <= high - 1; ++j) {
					if (func(arr[j], pivot))
						core::swap(arr[++i], arr[j]);
				}
				core::swap(arr[++i], arr[high]);
				return i;
			}

			template <typename Container, typename TSortFunc>
			void quick_sort(Container& arr, TSortFunc func, int low, int high) {
				if (low >= high)
					return;
				auto pos = quick_sort_partition(arr, func, low, high);
				quick_sort(arr, func, low, pos - 1);
				quick_sort(arr, func, pos + 1, high);
			}
		} // namespace detail

		//! Compile-time sort.
		//! Implements a sorting network for \tparam N up to 8
		template <typename Container, typename TSortFunc>
		constexpr void sort_ct(Container& arr, TSortFunc func) noexcept {
			constexpr size_t NItems = std::tuple_size<Container>::value;
			if constexpr (NItems <= 1) {
				return;
			} else if constexpr (NItems == 2) {
				swap_if(arr[0], arr[1], func);
			} else if constexpr (NItems == 3) {
				swap_if(arr[1], arr[2], func);
				swap_if(arr[0], arr[2], func);
				swap_if(arr[0], arr[1], func);
			} else if constexpr (NItems == 4) {
				swap_if(arr[0], arr[1], func);
				swap_if(arr[2], arr[3], func);

				swap_if(arr[0], arr[2], func);
				swap_if(arr[1], arr[3], func);

				swap_if(arr[1], arr[2], func);
			} else if constexpr (NItems == 5) {
				swap_if(arr[0], arr[1], func);
				swap_if(arr[3], arr[4], func);

				swap_if(arr[2], arr[4], func);

				swap_if(arr[2], arr[3], func);
				swap_if(arr[1], arr[4], func);

				swap_if(arr[0], arr[3], func);

				swap_if(arr[0], arr[2], func);
				swap_if(arr[1], arr[3], func);

				swap_if(arr[1], arr[2], func);
			} else if constexpr (NItems == 6) {
				swap_if(arr[1], arr[2], func);
				swap_if(arr[4], arr[5], func);

				swap_if(arr[0], arr[2], func);
				swap_if(arr[3], arr[5], func);

				swap_if(arr[0], arr[1], func);
				swap_if(arr[3], arr[4], func);
				swap_if(arr[2], arr[5], func);

				swap_if(arr[0], arr[3], func);
				swap_if(arr[1], arr[4], func);

				swap_if(arr[2], arr[4], func);
				swap_if(arr[1], arr[3], func);

				swap_if(arr[2], arr[3], func);
			} else if constexpr (NItems == 7) {
				swap_if(arr[1], arr[2], func);
				swap_if(arr[3], arr[4], func);
				swap_if(arr[5], arr[6], func);

				swap_if(arr[0], arr[2], func);
				swap_if(arr[3], arr[5], func);
				swap_if(arr[4], arr[6], func);

				swap_if(arr[0], arr[1], func);
				swap_if(arr[4], arr[5], func);
				swap_if(arr[2], arr[6], func);

				swap_if(arr[0], arr[4], func);
				swap_if(arr[1], arr[5], func);

				swap_if(arr[0], arr[3], func);
				swap_if(arr[2], arr[5], func);

				swap_if(arr[1], arr[3], func);
				swap_if(arr[2], arr[4], func);

				swap_if(arr[2], arr[3], func);
			} else if constexpr (NItems == 8) {
				swap_if(arr[0], arr[1], func);
				swap_if(arr[2], arr[3], func);
				swap_if(arr[4], arr[5], func);
				swap_if(arr[6], arr[7], func);

				swap_if(arr[0], arr[2], func);
				swap_if(arr[1], arr[3], func);
				swap_if(arr[4], arr[6], func);
				swap_if(arr[5], arr[7], func);

				swap_if(arr[1], arr[2], func);
				swap_if(arr[5], arr[6], func);
				swap_if(arr[0], arr[4], func);
				swap_if(arr[3], arr[7], func);

				swap_if(arr[1], arr[5], func);
				swap_if(arr[2], arr[6], func);

				swap_if(arr[1], arr[4], func);
				swap_if(arr[3], arr[6], func);

				swap_if(arr[2], arr[4], func);
				swap_if(arr[3], arr[5], func);

				swap_if(arr[3], arr[4], func);
			} else if constexpr (NItems == 9) {
				swap_if(arr[0], arr[1], func);
				swap_if(arr[3], arr[4], func);
				swap_if(arr[6], arr[7], func);

				swap_if(arr[1], arr[2], func);
				swap_if(arr[4], arr[5], func);
				swap_if(arr[7], arr[8], func);

				swap_if(arr[0], arr[1], func);
				swap_if(arr[3], arr[4], func);
				swap_if(arr[6], arr[7], func);

				swap_if(arr[0], arr[3], func);
				swap_if(arr[3], arr[6], func);
				swap_if(arr[0], arr[3], func);

				swap_if(arr[1], arr[4], func);
				swap_if(arr[4], arr[7], func);
				swap_if(arr[1], arr[4], func);

				swap_if(arr[5], arr[8], func);
				swap_if(arr[2], arr[5], func);
				swap_if(arr[5], arr[8], func);

				swap_if(arr[2], arr[4], func);
				swap_if(arr[4], arr[6], func);
				swap_if(arr[2], arr[4], func);

				swap_if(arr[1], arr[3], func);
				swap_if(arr[2], arr[3], func);
				swap_if(arr[5], arr[7], func);
				swap_if(arr[5], arr[6], func);
			} else {
				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(4244)
				detail::comb_sort_impl(arr, func);
				GAIA_MSVC_WARNING_POP()
			}
		}

		//! Sort the array \param arr given a comparison function \param cmpFunc.
		//! Sorts using a sorting network up to 8 elements. Quick sort above 32.
		//! \tparam Container Container to sort
		//! \tparam TCmpFunc Comparision function
		//! \param arr Container to sort
		//! \param func Comparision function
		template <typename Container, typename TCmpFunc>
		void sort(Container& arr, TCmpFunc func) {
			if (arr.size() <= 1) {
				// Nothing to sort with just one item
			} else if (arr.size() == 2) {
				swap_if(arr[0], arr[1], func);
			} else if (arr.size() == 3) {
				swap_if(arr[1], arr[2], func);
				swap_if(arr[0], arr[2], func);
				swap_if(arr[0], arr[1], func);
			} else if (arr.size() == 4) {
				swap_if(arr[0], arr[1], func);
				swap_if(arr[2], arr[3], func);

				swap_if(arr[0], arr[2], func);
				swap_if(arr[1], arr[3], func);

				swap_if(arr[1], arr[2], func);
			} else if (arr.size() == 5) {
				swap_if(arr[0], arr[1], func);
				swap_if(arr[3], arr[4], func);

				swap_if(arr[2], arr[4], func);

				swap_if(arr[2], arr[3], func);
				swap_if(arr[1], arr[4], func);

				swap_if(arr[0], arr[3], func);

				swap_if(arr[0], arr[2], func);
				swap_if(arr[1], arr[3], func);

				swap_if(arr[1], arr[2], func);
			} else if (arr.size() == 6) {
				swap_if(arr[1], arr[2], func);
				swap_if(arr[4], arr[5], func);

				swap_if(arr[0], arr[2], func);
				swap_if(arr[3], arr[5], func);

				swap_if(arr[0], arr[1], func);
				swap_if(arr[3], arr[4], func);
				swap_if(arr[2], arr[5], func);

				swap_if(arr[0], arr[3], func);
				swap_if(arr[1], arr[4], func);

				swap_if(arr[2], arr[4], func);
				swap_if(arr[1], arr[3], func);

				swap_if(arr[2], arr[3], func);
			} else if (arr.size() == 7) {
				swap_if(arr[1], arr[2], func);
				swap_if(arr[3], arr[4], func);
				swap_if(arr[5], arr[6], func);

				swap_if(arr[0], arr[2], func);
				swap_if(arr[3], arr[5], func);
				swap_if(arr[4], arr[6], func);

				swap_if(arr[0], arr[1], func);
				swap_if(arr[4], arr[5], func);
				swap_if(arr[2], arr[6], func);

				swap_if(arr[0], arr[4], func);
				swap_if(arr[1], arr[5], func);

				swap_if(arr[0], arr[3], func);
				swap_if(arr[2], arr[5], func);

				swap_if(arr[1], arr[3], func);
				swap_if(arr[2], arr[4], func);

				swap_if(arr[2], arr[3], func);
			} else if (arr.size() == 8) {
				swap_if(arr[0], arr[1], func);
				swap_if(arr[2], arr[3], func);
				swap_if(arr[4], arr[5], func);
				swap_if(arr[6], arr[7], func);

				swap_if(arr[0], arr[2], func);
				swap_if(arr[1], arr[3], func);
				swap_if(arr[4], arr[6], func);
				swap_if(arr[5], arr[7], func);

				swap_if(arr[1], arr[2], func);
				swap_if(arr[5], arr[6], func);
				swap_if(arr[0], arr[4], func);
				swap_if(arr[3], arr[7], func);

				swap_if(arr[1], arr[5], func);
				swap_if(arr[2], arr[6], func);

				swap_if(arr[1], arr[4], func);
				swap_if(arr[3], arr[6], func);

				swap_if(arr[2], arr[4], func);
				swap_if(arr[3], arr[5], func);

				swap_if(arr[3], arr[4], func);
			} else if (arr.size() <= 32) {
				auto n = arr.size();
				for (decltype(n) i = 0; i < n - 1; ++i) {
					for (decltype(n) j = 0; j < n - i - 1; ++j)
						swap_if(arr[j], arr[j + 1], func);
				}
			} else {
				const auto n = (int)arr.size();
				detail::quick_sort(arr, func, 0, n - 1);
			}
		}

		//! Sort the array \param arr given a comparison function \param cmpFunc.
		//! If cmpFunc returns true it performs \param sortFunc which can perform the sorting.
		//! Use when it is necessary to sort multiple arrays at once.
		//! Sorts using a sorting network up to 8 elements.
		//! \warning Currently only up to 32 elements are supported.
		//! \tparam Container Container to sort
		//! \tparam TCmpFunc Comparision function
		//! \tparam TSortFunc Sorting function
		//! \param arr Container to sort
		//! \param func Comparision function
		//! \param sortFunc Sorting function
		template <typename Container, typename TCmpFunc, typename TSortFunc>
		void sort(Container& arr, TCmpFunc func, TSortFunc sortFunc) {
			if (arr.size() <= 1) {
				// Nothing to sort with just one item
			} else if (arr.size() == 2) {
				try_swap_if(arr, 0, 1, func, sortFunc);
			} else if (arr.size() == 3) {
				try_swap_if(arr, 1, 2, func, sortFunc);
				try_swap_if(arr, 0, 2, func, sortFunc);
				try_swap_if(arr, 0, 1, func, sortFunc);
			} else if (arr.size() == 4) {
				try_swap_if(arr, 0, 1, func, sortFunc);
				try_swap_if(arr, 2, 3, func, sortFunc);

				try_swap_if(arr, 0, 2, func, sortFunc);
				try_swap_if(arr, 1, 3, func, sortFunc);

				try_swap_if(arr, 1, 2, func, sortFunc);
			} else if (arr.size() == 5) {
				try_swap_if(arr, 0, 1, func, sortFunc);
				try_swap_if(arr, 3, 4, func, sortFunc);

				try_swap_if(arr, 2, 4, func, sortFunc);

				try_swap_if(arr, 2, 3, func, sortFunc);
				try_swap_if(arr, 1, 4, func, sortFunc);

				try_swap_if(arr, 0, 3, func, sortFunc);

				try_swap_if(arr, 0, 2, func, sortFunc);
				try_swap_if(arr, 1, 3, func, sortFunc);

				try_swap_if(arr, 1, 2, func, sortFunc);
			} else if (arr.size() == 6) {
				try_swap_if(arr, 1, 2, func, sortFunc);
				try_swap_if(arr, 4, 5, func, sortFunc);

				try_swap_if(arr, 0, 2, func, sortFunc);
				try_swap_if(arr, 3, 5, func, sortFunc);

				try_swap_if(arr, 0, 1, func, sortFunc);
				try_swap_if(arr, 3, 4, func, sortFunc);
				try_swap_if(arr, 2, 5, func, sortFunc);

				try_swap_if(arr, 0, 3, func, sortFunc);
				try_swap_if(arr, 1, 4, func, sortFunc);

				try_swap_if(arr, 2, 4, func, sortFunc);
				try_swap_if(arr, 1, 3, func, sortFunc);

				try_swap_if(arr, 2, 3, func, sortFunc);
			} else if (arr.size() == 7) {
				try_swap_if(arr, 1, 2, func, sortFunc);
				try_swap_if(arr, 3, 4, func, sortFunc);
				try_swap_if(arr, 5, 6, func, sortFunc);

				try_swap_if(arr, 0, 2, func, sortFunc);
				try_swap_if(arr, 3, 5, func, sortFunc);
				try_swap_if(arr, 4, 6, func, sortFunc);

				try_swap_if(arr, 0, 1, func, sortFunc);
				try_swap_if(arr, 4, 5, func, sortFunc);
				try_swap_if(arr, 2, 6, func, sortFunc);

				try_swap_if(arr, 0, 4, func, sortFunc);
				try_swap_if(arr, 1, 5, func, sortFunc);

				try_swap_if(arr, 0, 3, func, sortFunc);
				try_swap_if(arr, 2, 5, func, sortFunc);

				try_swap_if(arr, 1, 3, func, sortFunc);
				try_swap_if(arr, 2, 4, func, sortFunc);

				try_swap_if(arr, 2, 3, func, sortFunc);
			} else if (arr.size() == 8) {
				try_swap_if(arr, 0, 1, func, sortFunc);
				try_swap_if(arr, 2, 3, func, sortFunc);
				try_swap_if(arr, 4, 5, func, sortFunc);
				try_swap_if(arr, 6, 7, func, sortFunc);

				try_swap_if(arr, 0, 2, func, sortFunc);
				try_swap_if(arr, 1, 3, func, sortFunc);
				try_swap_if(arr, 4, 6, func, sortFunc);
				try_swap_if(arr, 5, 7, func, sortFunc);

				try_swap_if(arr, 1, 2, func, sortFunc);
				try_swap_if(arr, 5, 6, func, sortFunc);
				try_swap_if(arr, 0, 4, func, sortFunc);
				try_swap_if(arr, 3, 7, func, sortFunc);

				try_swap_if(arr, 1, 5, func, sortFunc);
				try_swap_if(arr, 2, 6, func, sortFunc);

				try_swap_if(arr, 1, 4, func, sortFunc);
				try_swap_if(arr, 3, 6, func, sortFunc);

				try_swap_if(arr, 2, 4, func, sortFunc);
				try_swap_if(arr, 3, 5, func, sortFunc);

				try_swap_if(arr, 3, 4, func, sortFunc);
			} else if (arr.size() <= 32) {
				auto n = arr.size();
				for (decltype(n) i = 0; i < n - 1; ++i)
					for (decltype(n) j = 0; j < n - i - 1; ++j)
						try_swap_if(arr, j, j + 1, func, sortFunc);
			} else {
				GAIA_ASSERT(false && "sort currently supports at most 32 items in the array");
				// const int n = (int)arr.size();
				// detail::quick_sort(arr, 0, n - 1);
			}
		}
	} // namespace core
} // namespace gaia

namespace gaia {
	namespace core {
		template <uint32_t BlockBits>
		struct bit_view {
			static constexpr uint32_t MaxValue = (1 << BlockBits) - 1;

			std::span<uint8_t> m_data;

			void set(uint32_t bitPosition, uint8_t value) noexcept {
				GAIA_ASSERT(bitPosition < (m_data.size() * 8));
				GAIA_ASSERT(value <= MaxValue);

				const uint32_t idxByte = bitPosition / 8;
				const uint32_t idxBit = bitPosition % 8;

				const uint8_t mask = ~(MaxValue << idxBit);
				m_data[idxByte] = (m_data[idxByte] & mask) | (value << idxBit);

				const bool overlaps = idxBit + BlockBits > 8;
				if (overlaps) {
					// Value spans over two bytes
					const uint8_t shift2 = uint8_t(8U - idxBit);
					const uint8_t mask2 = ~(MaxValue >> shift2);
					m_data[idxByte + 1] = (m_data[idxByte + 1] & mask2) | (value >> shift2);
				}
			}

			uint8_t get(uint32_t bitPosition) const noexcept {
				GAIA_ASSERT(bitPosition < (m_data.size() * 8));

				const uint32_t idxByte = bitPosition / 8;
				const uint32_t idxBit = bitPosition % 8;

				const uint8_t byte1 = (m_data[idxByte] >> idxBit) & MaxValue;

				const bool overlaps = idxBit + BlockBits > 8;
				if (overlaps) {
					// Value spans over two bytes
					const uint8_t shift2 = uint8_t(8U - idxBit);
					const uint8_t mask2 = MaxValue >> shift2;
					const uint8_t byte2 = (m_data[idxByte + 1] & mask2) << shift2;
					return byte1 | byte2;
				}

				return byte1;
			}
		};
	} // namespace core
} // namespace gaia

#include <cstdint>
#include <type_traits>

namespace gaia {
	namespace core {

		namespace detail {
			template <typename, typename = void>
			struct is_direct_hash_key: std::false_type {};
			template <typename T>
			struct is_direct_hash_key<T, std::void_t<decltype(T::IsDirectHashKey)>>: std::true_type {};

			//-----------------------------------------------------------------------------------

			constexpr void hash_combine2_out(uint32_t& lhs, uint32_t rhs) {
				lhs ^= rhs + 0x9e3779b9 + (lhs << 6) + (lhs >> 2);
			}
			constexpr void hash_combine2_out(uint64_t& lhs, uint64_t rhs) {
				lhs ^= rhs + 0x9e3779B97f4a7c15ULL + (lhs << 6) + (lhs >> 2);
			}

			template <typename T>
			GAIA_NODISCARD constexpr T hash_combine2(T lhs, T rhs) {
				hash_combine2_out(lhs, rhs);
				return lhs;
			}
		} // namespace detail

		template <typename T>
		inline constexpr bool is_direct_hash_key_v = detail::is_direct_hash_key<T>::value;

		template <typename T>
		struct direct_hash_key {
			using Type = T;

			static_assert(std::is_integral_v<T>);
			static constexpr bool IsDirectHashKey = true;

			T hash;
			bool operator==(direct_hash_key other) const {
				return hash == other.hash;
			}
			bool operator!=(direct_hash_key other) const {
				return hash != other.hash;
			}
		};

		//! Combines values via OR.
		template <typename... T>
		constexpr auto combine_or([[maybe_unused]] T... t) {
			return (... | t);
		}

		//! Combines hashes into another complex one
		template <typename T, typename... Rest>
		constexpr T hash_combine(T first, T next, Rest... rest) {
			auto h = detail::hash_combine2(first, next);
			(detail::hash_combine2_out(h, rest), ...);
			return h;
		}

#if GAIA_ECS_HASH == GAIA_ECS_HASH_FNV1A

		namespace detail {
			namespace fnv1a {
				constexpr uint64_t val_64_const = 0xcbf29ce484222325;
				constexpr uint64_t prime_64_const = 0x100000001b3;
			} // namespace fnv1a
		} // namespace detail

		constexpr uint64_t calculate_hash64(const char* const str) noexcept {
			uint64_t hash = detail::fnv1a::val_64_const;

			uint64_t i = 0;
			while (str[i] != '\0') {
				hash = (hash ^ uint64_t(str[i])) * detail::fnv1a::prime_64_const;
				++i;
			}

			return hash;
		}

		constexpr uint64_t calculate_hash64(const char* const str, const uint64_t length) noexcept {
			uint64_t hash = detail::fnv1a::val_64_const;

			for (uint64_t i = 0; i < length; ++i)
				hash = (hash ^ uint64_t(str[i])) * detail::fnv1a::prime_64_const;

			return hash;
		}

#elif GAIA_ECS_HASH == GAIA_ECS_HASH_MURMUR2A

		// Thank you https://gist.github.com/oteguro/10538695

		GAIA_MSVC_WARNING_PUSH()
		GAIA_MSVC_WARNING_DISABLE(4592)

		namespace detail {
			namespace murmur2a {
				constexpr uint64_t seed_64_const = 0xe17a1465ULL;
				constexpr uint64_t m = 0xc6a4a7935bd1e995ULL;
				constexpr uint64_t r = 47;

				constexpr uint64_t Load8(const char* data) {
					return (uint64_t(data[7]) << 56) | (uint64_t(data[6]) << 48) | (uint64_t(data[5]) << 40) |
								 (uint64_t(data[4]) << 32) | (uint64_t(data[3]) << 24) | (uint64_t(data[2]) << 16) |
								 (uint64_t(data[1]) << 8) | (uint64_t(data[0]) << 0);
				}

				constexpr uint64_t StaticHashValueLast64(uint64_t h) {
					return (((h * m) ^ ((h * m) >> r)) * m) ^ ((((h * m) ^ ((h * m) >> r)) * m) >> r);
				}

				constexpr uint64_t StaticHashValueLast64_(uint64_t h) {
					return (((h) ^ ((h) >> r)) * m) ^ ((((h) ^ ((h) >> r)) * m) >> r);
				}

				constexpr uint64_t StaticHashValue64Tail1(uint64_t h, const char* data) {
					return StaticHashValueLast64((h ^ uint64_t(data[0])));
				}

				constexpr uint64_t StaticHashValue64Tail2(uint64_t h, const char* data) {
					return StaticHashValue64Tail1((h ^ uint64_t(data[1]) << 8), data);
				}

				constexpr uint64_t StaticHashValue64Tail3(uint64_t h, const char* data) {
					return StaticHashValue64Tail2((h ^ uint64_t(data[2]) << 16), data);
				}

				constexpr uint64_t StaticHashValue64Tail4(uint64_t h, const char* data) {
					return StaticHashValue64Tail3((h ^ uint64_t(data[3]) << 24), data);
				}

				constexpr uint64_t StaticHashValue64Tail5(uint64_t h, const char* data) {
					return StaticHashValue64Tail4((h ^ uint64_t(data[4]) << 32), data);
				}

				constexpr uint64_t StaticHashValue64Tail6(uint64_t h, const char* data) {
					return StaticHashValue64Tail5((h ^ uint64_t(data[5]) << 40), data);
				}

				constexpr uint64_t StaticHashValue64Tail7(uint64_t h, const char* data) {
					return StaticHashValue64Tail6((h ^ uint64_t(data[6]) << 48), data);
				}

				constexpr uint64_t StaticHashValueRest64(uint64_t h, uint64_t len, const char* data) {
					return ((len & 7) == 7)		? StaticHashValue64Tail7(h, data)
								 : ((len & 7) == 6) ? StaticHashValue64Tail6(h, data)
								 : ((len & 7) == 5) ? StaticHashValue64Tail5(h, data)
								 : ((len & 7) == 4) ? StaticHashValue64Tail4(h, data)
								 : ((len & 7) == 3) ? StaticHashValue64Tail3(h, data)
								 : ((len & 7) == 2) ? StaticHashValue64Tail2(h, data)
								 : ((len & 7) == 1) ? StaticHashValue64Tail1(h, data)
																		: StaticHashValueLast64_(h);
				}

				constexpr uint64_t StaticHashValueLoop64(uint64_t i, uint64_t h, uint64_t len, const char* data) {
					return (
							i == 0 ? StaticHashValueRest64(h, len, data)
										 : StaticHashValueLoop64(
													 i - 1, (h ^ (((Load8(data) * m) ^ ((Load8(data) * m) >> r)) * m)) * m, len, data + 8));
				}

				constexpr uint64_t hash_murmur2a_64_ct(const char* key, uint64_t len, uint64_t seed) {
					return StaticHashValueLoop64(len / 8, seed ^ (len * m), (len), key);
				}
			} // namespace murmur2a
		} // namespace detail

		constexpr uint64_t calculate_hash64(uint64_t value) {
			value ^= value >> 33U;
			value *= 0xff51afd7ed558ccdULL;
			value ^= value >> 33U;

			value *= 0xc4ceb9fe1a85ec53ULL;
			value ^= value >> 33U;
			return value;
		}

		constexpr uint64_t calculate_hash64(const char* str) {
			uint64_t size = 0;
			while (str[size] != '\0')
				++size;

			return detail::murmur2a::hash_murmur2a_64_ct(str, size, detail::murmur2a::seed_64_const);
		}

		constexpr uint64_t calculate_hash64(const char* str, uint64_t length) {
			return detail::murmur2a::hash_murmur2a_64_ct(str, length, detail::murmur2a::seed_64_const);
		}

		GAIA_MSVC_WARNING_POP()

#else
	#error "Unknown hashing type defined"
#endif

	} // namespace core
} // namespace gaia

#include <tuple>
#include <type_traits>
#include <utility>

#include <tuple>
#include <type_traits>
#include <utility>

namespace gaia {
	namespace meta {

		namespace detail {
			// Check if type T is constructible via T{Args...}
			struct any_type {
				template <typename T>
				constexpr operator T(); // non explicit
			};

			template <typename T, typename... TArgs>
			decltype(void(T{std::declval<TArgs>()...}), std::true_type{}) is_braces_constructible(int);

			template <typename, typename...>
			std::false_type is_braces_constructible(...);

			template <typename T, typename... TArgs>
			using is_braces_constructible_t = decltype(detail::is_braces_constructible<T, TArgs...>(0));
		} // namespace detail

		//----------------------------------------------------------------------
		// Tuple to struct conversion
		//----------------------------------------------------------------------

		template <typename S, size_t... Is, typename Tuple>
		GAIA_NODISCARD S tuple_to_struct(std::index_sequence<Is...> /*no_name*/, Tuple&& tup) {
			return {std::get<Is>(std::forward<Tuple>(tup))...};
		}

		template <typename S, typename Tuple>
		GAIA_NODISCARD S tuple_to_struct(Tuple&& tup) {
			using T = std::remove_reference_t<Tuple>;

			return tuple_to_struct<S>(std::make_index_sequence<std::tuple_size<T>{}>{}, std::forward<Tuple>(tup));
		}

		//----------------------------------------------------------------------
		// Struct to tuple conversion
		//----------------------------------------------------------------------

		//! The number of bits necessary to fit the maximum supported number of members in a struct
		static constexpr uint32_t StructToTupleMaxTypes_Bits = 4;
		static constexpr uint32_t StructToTupleMaxTypes = (1 << StructToTupleMaxTypes_Bits) - 1;

		//! Converts a struct to a tuple (struct must support initialization via:
		//! Struct{x,y,...,z})
		template <typename T>
		auto struct_to_tuple(T&& object) noexcept {
			using type = typename std::decay_t<typename std::remove_pointer_t<T>>;

			if constexpr (std::is_empty_v<type>) {
				return std::make_tuple();
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15] = object;
				return std::make_tuple(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14] = object;
				return std::make_tuple(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13] = object;
				return std::make_tuple(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12] = object;
				return std::make_tuple(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11] = object;
				return std::make_tuple(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9, p10] = object;
				return std::make_tuple(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9] = object;
				return std::make_tuple(p1, p2, p3, p4, p5, p6, p7, p8, p9);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6, p7, p8] = object;
				return std::make_tuple(p1, p2, p3, p4, p5, p6, p7, p8);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6, p7] = object;
				return std::make_tuple(p1, p2, p3, p4, p5, p6, p7);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6] = object;
				return std::make_tuple(p1, p2, p3, p4, p5, p6);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type>{}) {
				auto&& [p1, p2, p3, p4, p5] = object;
				return std::make_tuple(p1, p2, p3, p4, p5);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type>{}) {
				auto&& [p1, p2, p3, p4] = object;
				return std::make_tuple(p1, p2, p3, p4);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type>{}) {
				auto&& [p1, p2, p3] = object;
				return std::make_tuple(p1, p2, p3);
			} else if constexpr (detail::is_braces_constructible_t<type, detail::any_type, detail::any_type>{}) {
				auto&& [p1, p2] = object;
				return std::make_tuple(p1, p2);
			} else if constexpr (detail::is_braces_constructible_t<type, detail::any_type>{}) {
				auto&& [p1] = object;
				return std::make_tuple(p1);
			}

			static_assert("Unsupported number of members");
		}

		//----------------------------------------------------------------------
		// Struct to tuple conversion
		//----------------------------------------------------------------------

		template <typename T>
		auto struct_member_count() {
			using type = std::decay_t<T>;

			if constexpr (std::is_empty_v<type>) {
				return 0;
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type>{}) {
				return 15;
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type>{}) {
				return 14;
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type>{}) {
				return 13;
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type>{}) {
				return 12;
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type>{}) {
				return 11;
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type>{}) {
				return 10;
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type>{}) {
				return 9;
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type>{}) {
				return 8;
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type>{}) {
				return 7;
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type>{}) {
				return 6;
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type>{}) {
				return 5;
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type>{}) {
				return 4;
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type>{}) {
				return 3;
			} else if constexpr (detail::is_braces_constructible_t<type, detail::any_type, detail::any_type>{}) {
				return 2;
			} else if constexpr (detail::is_braces_constructible_t<type, detail::any_type>{}) {
				return 1;
			}

			static_assert("Unsupported number of members");
		}

		template <typename T, typename Func>
		auto each_member(T&& object, Func&& visitor) {
			using type = std::decay_t<T>;

			if constexpr (std::is_empty_v<type>) {
				visitor();
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15] = object;
				return visitor(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14] = object;
				return visitor(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13] = object;
				return visitor(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12] = object;
				return visitor(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11] = object;
				return visitor(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9, p10] = object;
				return visitor(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9] = object;
				return visitor(p1, p2, p3, p4, p5, p6, p7, p8, p9);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6, p7, p8] = object;
				return visitor(p1, p2, p3, p4, p5, p6, p7, p8);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6, p7] = object;
				return visitor(p1, p2, p3, p4, p5, p6, p7);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6] = object;
				return visitor(p1, p2, p3, p4, p5, p6);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type>{}) {
				auto&& [p1, p2, p3, p4, p5] = object;
				return visitor(p1, p2, p3, p4, p5);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type>{}) {
				auto&& [p1, p2, p3, p4] = object;
				return visitor(p1, p2, p3, p4);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type>{}) {
				auto&& [p1, p2, p3] = object;
				return visitor(p1, p2, p3);
			} else if constexpr (detail::is_braces_constructible_t<type, detail::any_type, detail::any_type>{}) {
				auto&& [p1, p2] = object;
				return visitor(p1, p2);
			} else if constexpr (detail::is_braces_constructible_t<type, detail::any_type>{}) {
				auto&& [p1] = object;
				return visitor(p1);
			}

			static_assert("Unsupported number of members");
		}

	} // namespace meta
} // namespace gaia

#include <cinttypes>
#include <cstring>
#include <stdlib.h>
#include <type_traits>
#include <utility>

#if GAIA_PLATFORM_WINDOWS && GAIA_COMPILER_MSVC
	#define GAIA_MEM_ALLC(size) malloc(size)
	#define GAIA_MEM_FREE(ptr) free(ptr)

	// Clang with MSVC codegen needs some remapping
	#if !defined(aligned_alloc)
		#define GAIA_MEM_ALLC_A(size, alig) _aligned_malloc(size, alig)
		#define GAIA_MEM_FREE_A(ptr) _aligned_free(ptr)
	#else
		#define GAIA_MEM_ALLC_A(size, alig) aligned_alloc(alig, size)
		#define GAIA_MEM_FREE_A(ptr) aligned_free(ptr)
	#endif
#else
	#define GAIA_MEM_ALLC(size) malloc(size)
	#define GAIA_MEM_ALLC_A(size, alig) aligned_alloc(alig, size)
	#define GAIA_MEM_FREE(ptr) free(ptr)
	#define GAIA_MEM_FREE_A(ptr) free(ptr)
#endif

namespace gaia {
	namespace mem {
		inline void* mem_alloc(size_t size) {
			GAIA_ASSERT(size > 0);

			void* ptr = GAIA_MEM_ALLC(size);
			GAIA_PROF_ALLOC(ptr, size);
			return ptr;
		}

		inline void* mem_alloc_alig(size_t size, size_t alig) {
			GAIA_ASSERT(size > 0);
			GAIA_ASSERT(alig > 0);

			// Make sure size is a multiple of the alignment
			size = (size + alig - 1) & ~(alig - 1);
			void* ptr = GAIA_MEM_ALLC_A(size, alig);
			GAIA_PROF_ALLOC(ptr, size);
			return ptr;
		}

		inline void mem_free(void* ptr) {
			GAIA_MEM_FREE(ptr);
			GAIA_PROF_FREE(ptr);
		}

		inline void mem_free_alig(void* ptr) {
			GAIA_MEM_FREE_A(ptr);
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

		//! Pointer wrapper for reading memory in defined way (not causing undefined behavior)
		template <typename T>
		class const_unaligned_pointer {
			const uint8_t* from;

		public:
			const_unaligned_pointer(): from(nullptr) {}
			const_unaligned_pointer(const void* p): from((const uint8_t*)p) {}

			T operator*() const {
				T to;
				memmove((void*)&to, (const void*)from, sizeof(T));
				return to;
			}

			T operator[](std::ptrdiff_t d) const {
				return *(*this + d);
			}

			const_unaligned_pointer operator+(std::ptrdiff_t d) const {
				return const_unaligned_pointer(from + d * sizeof(T));
			}
			const_unaligned_pointer operator-(std::ptrdiff_t d) const {
				return const_unaligned_pointer(from - d * sizeof(T));
			}
		};

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

			unaligned_ref<T> operator[](std::ptrdiff_t d) const {
				return *(*this + d);
			}

			unaligned_pointer operator+(std::ptrdiff_t d) const {
				return unaligned_pointer(m_p + d * sizeof(T));
			}
			unaligned_pointer operator-(std::ptrdiff_t d) const {
				return unaligned_pointer(m_p - d * sizeof(T));
			}
		};

		template <typename T>
		constexpr T* addressof(T& obj) noexcept {
			return &obj;
		}

		template <class T>
		const T* addressof(const T&&) = delete;
	} // namespace mem
} // namespace gaia

namespace gaia {
	namespace mem {
		enum class DataLayout : uint32_t {
			AoS, //< Array Of Structures
			SoA, //< Structure Of Arrays, 4 packed items, good for SSE and similar
			SoA8, //< Structure Of Arrays, 8 packed items, good for AVX and similar
			SoA16, //< Structure Of Arrays, 16 packed items, good for AVX512 and similar

			Count = 4
		};

		// Helper templates
		namespace detail {
			//----------------------------------------------------------------------
			// Byte offset of a member of SoA-organized data
			//----------------------------------------------------------------------

			inline constexpr size_t get_aligned_byte_offset(uintptr_t address, size_t alig, size_t itemSize, size_t cnt) {
				const auto padding = mem::padding(address, alig);
				address += padding + itemSize * cnt;
				return address;
			}

			template <typename T, size_t Alignment>
			constexpr size_t get_aligned_byte_offset(uintptr_t address, size_t cnt) {
				const auto padding = mem::padding<Alignment>(address);
				const auto itemSize = sizeof(T);
				address += padding + itemSize * cnt;
				return address;
			}

			//----------------------------------------------------------------------
			// Data storage
			//----------------------------------------------------------------------

			template <uint32_t Size, uint32_t Aligment>
			struct raw_data_holder {
				alignas(Aligment) uint8_t data[Size];

				constexpr operator uint8_t*() noexcept {
					return data;
				}

				constexpr operator const uint8_t*() const noexcept {
					return data;
				}
			};

			template <uint32_t Size>
			struct raw_data_holder<Size, 0> {
				uint8_t data[Size];

				constexpr operator uint8_t*() noexcept {
					return data;
				}

				constexpr operator const uint8_t*() const noexcept {
					return data;
				}
			};
		} // namespace detail

		template <DataLayout TDataLayout, typename TItem>
		struct data_layout_properties;
		template <typename TItem>
		struct data_layout_properties<DataLayout::AoS, TItem> {
			constexpr static DataLayout Layout = DataLayout::AoS;
			constexpr static size_t PackSize = 1;
			constexpr static size_t Alignment = alignof(TItem);
			constexpr static size_t ArrayAlignment = 0;
		};
		template <typename TItem>
		struct data_layout_properties<DataLayout::SoA, TItem> {
			constexpr static DataLayout Layout = DataLayout::SoA;
			constexpr static size_t PackSize = 4;
			constexpr static size_t Alignment = PackSize * 4;
			constexpr static size_t ArrayAlignment = Alignment;
		};
		template <typename TItem>
		struct data_layout_properties<DataLayout::SoA8, TItem> {
			constexpr static DataLayout Layout = DataLayout::SoA8;
			constexpr static size_t PackSize = 8;
			constexpr static size_t Alignment = PackSize * 4;
			constexpr static size_t ArrayAlignment = Alignment;
		};
		template <typename TItem>
		struct data_layout_properties<DataLayout::SoA16, TItem> {
			constexpr static DataLayout Layout = DataLayout::SoA16;
			constexpr static size_t PackSize = 16;
			constexpr static size_t Alignment = PackSize * 4;
			constexpr static size_t ArrayAlignment = Alignment;
		};

		template <DataLayout TDataLayout, typename TItem>
		struct data_view_policy;

		template <DataLayout TDataLayout, typename TItem>
		struct data_view_policy_get;
		template <DataLayout TDataLayout, typename TItem>
		struct data_view_policy_set;

		template <DataLayout TDataLayout, typename TItem, size_t Ids>
		struct data_view_policy_get_idx;
		template <DataLayout TDataLayout, typename TItem, size_t Ids>
		struct data_view_policy_set_idx;

		/*!
		 * data_view_policy for accessing and storing data in the AoS way
		 *	Good for random access and when acessing data together.
		 *
		 * struct Foo { int x; int y; int z; };
		 * using fooViewPolicy = data_view_policy<DataLayout::AoS, Foo>;
		 *
		 * Memory is going be be organized as:
		 *		xyz xyz xyz xyz
		 */
		template <typename ValueType>
		struct data_view_policy_aos {
			using TargetCastType = std::add_pointer_t<ValueType>;

			constexpr static DataLayout Layout = data_layout_properties<DataLayout::AoS, ValueType>::Layout;
			constexpr static size_t Alignment = data_layout_properties<DataLayout::AoS, ValueType>::Alignment;
			constexpr static size_t ArrayAlignment = data_layout_properties<DataLayout::AoS, ValueType>::ArrayAlignment;

			GAIA_NODISCARD static constexpr uint32_t get_min_byte_size(uintptr_t addr, size_t cnt) noexcept {
				const auto offset = detail::get_aligned_byte_offset<ValueType, Alignment>(addr, cnt);
				return (uint32_t)(offset - addr);
			}

			GAIA_NODISCARD static uint8_t* alloc_mem(size_t cnt) noexcept {
				const auto bytes = get_min_byte_size(0, cnt);
				auto* pData = (uint8_t*)mem::mem_alloc(bytes);
				core::call_ctor((ValueType*)pData, cnt);
				return pData;
			}

			static void free_mem(uint8_t* pData, size_t cnt) noexcept {
				if (pData == nullptr)
					return;
				core::call_dtor((ValueType*)pData, cnt);
				return mem::mem_free(pData);
			}

			GAIA_NODISCARD constexpr static ValueType get_value(std::span<const ValueType> s, size_t idx) noexcept {
				return s[idx];
			}

			GAIA_NODISCARD constexpr static const ValueType& get(std::span<const ValueType> s, size_t idx) noexcept {
				return s[idx];
			}

			GAIA_NODISCARD constexpr static ValueType& set(std::span<ValueType> s, size_t idx) noexcept {
				return s[idx];
			}

			constexpr static void set(std::span<ValueType> s, size_t idx, ValueType&& val) noexcept {
				s[idx] = std::forward<ValueType>(val);
			}
		};

		template <typename ValueType>
		struct data_view_policy<DataLayout::AoS, ValueType>: data_view_policy_aos<ValueType> {};

		template <typename ValueType>
		struct data_view_policy_aos_get {
			using view_policy = data_view_policy_aos<ValueType>;

			//! Raw data pointed to by the view policy
			std::span<const uint8_t> m_data;

			data_view_policy_aos_get(std::span<uint8_t> data): m_data({(const uint8_t*)data.data(), data.size()}) {}
			data_view_policy_aos_get(std::span<const uint8_t> data): m_data({(const uint8_t*)data.data(), data.size()}) {}
			template <typename C>
			data_view_policy_aos_get(const C& c): m_data({(const uint8_t*)c.data(), c.size()}) {
				static_assert(!std::is_same_v<C, data_view_policy_aos_get>);
			}

			GAIA_NODISCARD const ValueType& operator[](size_t idx) const noexcept {
				return std::span<const ValueType>{(const ValueType*)m_data.data(), m_data.size()}[idx];
			}

			GAIA_NODISCARD auto data() const noexcept {
				return m_data.data();
			}

			GAIA_NODISCARD auto size() const noexcept {
				return m_data.size();
			}
		};

		template <typename ValueType>
		struct data_view_policy_get<DataLayout::AoS, ValueType>: data_view_policy_aos_get<ValueType> {};

		template <typename ValueType>
		struct data_view_policy_aos_set {
			using view_policy = data_view_policy_aos<ValueType>;

			//! Raw data pointed to by the view policy
			std::span<uint8_t> m_data;

			data_view_policy_aos_set(std::span<uint8_t> data): m_data({(uint8_t*)data.data(), data.size()}) {}
			data_view_policy_aos_set(std::span<const uint8_t> data): m_data({(uint8_t*)data.data(), data.size()}) {}
			template <typename C>
			data_view_policy_aos_set(const C& c): m_data({(uint8_t*)c.data(), c.size()}) {
				static_assert(!std::is_same_v<C, data_view_policy_aos_set>);
			}

			GAIA_NODISCARD ValueType& operator[](size_t idx) noexcept {
				return std::span<ValueType>{(ValueType*)m_data.data(), m_data.size()}[idx];
			}

			GAIA_NODISCARD const ValueType& operator[](size_t idx) const noexcept {
				return std::span<const ValueType>{(const ValueType*)m_data.data(), m_data.size()}[idx];
			}

			GAIA_NODISCARD auto data() const noexcept {
				return m_data.data();
			}

			GAIA_NODISCARD auto size() const noexcept {
				return m_data.size();
			}
		};

		template <typename ValueType>
		struct data_view_policy_set<DataLayout::AoS, ValueType>: data_view_policy_aos_set<ValueType> {};

		/*!
		 * data_view_policy for accessing and storing data in the SoA way
		 *	Good for SIMD processing.
		 *
		 * struct Foo { int x; int y; int z; };
		 * using fooViewPolicy = data_view_policy<DataLayout::SoA, Foo>;
		 *
		 * Memory is going be be organized as:
		 *		xxxx yyyy zzzz
		 */
		template <DataLayout TDataLayout, typename ValueType>
		struct data_view_policy_soa {
			static_assert(std::is_copy_assignable_v<ValueType>);

			using TTuple = decltype(meta::struct_to_tuple(ValueType{}));
			using TargetCastType = uint8_t*;

			constexpr static DataLayout Layout = data_layout_properties<TDataLayout, ValueType>::Layout;
			constexpr static size_t Alignment = data_layout_properties<TDataLayout, ValueType>::Alignment;
			constexpr static size_t ArrayAlignment = data_layout_properties<TDataLayout, ValueType>::ArrayAlignment;
			constexpr static size_t TTupleItems = std::tuple_size<TTuple>::value;
			static_assert(Alignment > 0, "SoA data can't be zero-aligned");
			static_assert(sizeof(ValueType) > 0, "SoA data can't be zero-size");

			template <size_t Item>
			using value_type = typename std::tuple_element<Item, TTuple>::type;
			template <size_t Item>
			using const_value_type = typename std::add_const<value_type<Item>>::type;

			GAIA_NODISCARD constexpr static uint32_t get_min_byte_size(uintptr_t addr, size_t cnt) noexcept {
				const auto offset = get_aligned_byte_offset<TTupleItems>(addr, cnt);
				return (uint32_t)(offset - addr);
			}

			GAIA_NODISCARD static uint8_t* alloc_mem(size_t cnt) noexcept {
				const auto bytes = get_min_byte_size(0, cnt);
				return (uint8_t*)mem::mem_alloc_alig(bytes, Alignment);
			}

			static void free_mem(uint8_t* pData, [[maybe_unused]] size_t cnt) noexcept {
				if (pData == nullptr)
					return;
				return mem::mem_free_alig(pData);
			}

			GAIA_NODISCARD constexpr static ValueType get(std::span<const uint8_t> s, size_t idx) noexcept {
				auto t = meta::struct_to_tuple(ValueType{});
				return get_inter(t, s, idx, std::make_index_sequence<TTupleItems>());
			}

			template <size_t Item>
			constexpr static auto get(std::span<const uint8_t> s, size_t idx = 0) noexcept {
				const auto offset = get_aligned_byte_offset<Item>((uintptr_t)s.data(), s.size());
				const auto& ref = get_ref<const value_type<Item>>((const uint8_t*)offset, idx);
				return std::span{&ref, s.size() - idx};
			}

			class accessor {
				std::span<uint8_t> m_data;
				size_t m_idx;

			public:
				accessor(std::span<uint8_t> data, size_t idx): m_data(data), m_idx(idx) {}

				constexpr void operator=(const ValueType& val) noexcept {
					auto t = meta::struct_to_tuple(val);
					set_inter(t, m_data, m_idx, std::make_index_sequence<TTupleItems>());
				}

				constexpr void operator=(ValueType&& val) noexcept {
					auto t = meta::struct_to_tuple(std::forward<ValueType>(val));
					set_inter(t, m_data, m_idx, std::make_index_sequence<TTupleItems>());
				}

				GAIA_NODISCARD constexpr operator ValueType() const noexcept {
					auto t = meta::struct_to_tuple(ValueType{});
					return get_inter(
							t, {(const uint8_t*)m_data.data(), m_data.size()}, m_idx, std::make_index_sequence<TTupleItems>());
				}
			};

			constexpr static auto set(std::span<uint8_t> s, size_t idx) noexcept {
				return accessor(s, idx);
			}

			template <size_t Item>
			constexpr static auto set(std::span<uint8_t> s, size_t idx = 0) noexcept {
				const auto offset = get_aligned_byte_offset<Item>((uintptr_t)s.data(), s.size());
				auto& ref = get_ref<value_type<Item>>((const uint8_t*)offset, idx);
				return std::span{&ref, s.size() - idx};
			}

		private:
			template <size_t... Ids>
			constexpr static size_t
			get_aligned_byte_offset_seq(uintptr_t address, size_t cnt, std::index_sequence<Ids...> /*no_name*/) {
				// Align the address for the first type
				address = mem::align<Alignment>(address);
				// Offset and align the rest
				((address = mem::align<Alignment>(address + sizeof(value_type<Ids>) * cnt)), ...);
				return address;
			}

			template <uint32_t N>
			constexpr static size_t get_aligned_byte_offset(uintptr_t address, size_t cnt) {
				return get_aligned_byte_offset_seq(address, cnt, std::make_index_sequence<N>());
			}

			template <typename TMemberType>
			constexpr static TMemberType& get_ref(const uint8_t* data, size_t idx) noexcept {
				// Write the value directly to the memory address.
				// Usage of unaligned_ref is not necessary because the memory is aligned.
				auto* pCastData = (TMemberType*)data;
				return pCastData[idx];
			}

			template <size_t... Ids>
			GAIA_NODISCARD constexpr static ValueType
			get_inter(TTuple& t, std::span<const uint8_t> s, size_t idx, std::index_sequence<Ids...> /*no_name*/) noexcept {
				auto address = mem::align<Alignment>((uintptr_t)s.data());
				((
						 // Put the value at the address into our tuple. Data is aligned so we can read directly.
						 std::get<Ids>(t) = get_ref<value_type<Ids>>((const uint8_t*)address, idx),
						 // Skip towards the next element and make sure the address is aligned properly
						 address = mem::align<Alignment>(address + sizeof(value_type<Ids>) * s.size())),
				 ...);
				return meta::tuple_to_struct<ValueType, TTuple>(std::forward<TTuple>(t));
			}

			template <size_t... Ids>
			constexpr static void
			set_inter(TTuple& t, std::span<uint8_t> s, size_t idx, std::index_sequence<Ids...> /*no_name*/) noexcept {
				auto address = mem::align<Alignment>((uintptr_t)s.data());
				((
						 // Set the tuple value. Data is aligned so we can write directly.
						 get_ref<value_type<Ids>>((uint8_t*)address, idx) = std::get<Ids>(t),
						 // Skip towards the next element and make sure the address is aligned properly
						 address = mem::align<Alignment>(address + sizeof(value_type<Ids>) * s.size())),
				 ...);
			}
		};

		template <typename ValueType>
		struct data_view_policy<DataLayout::SoA, ValueType>: data_view_policy_soa<DataLayout::SoA, ValueType> {};
		template <typename ValueType>
		struct data_view_policy<DataLayout::SoA8, ValueType>: data_view_policy_soa<DataLayout::SoA8, ValueType> {};
		template <typename ValueType>
		struct data_view_policy<DataLayout::SoA16, ValueType>: data_view_policy_soa<DataLayout::SoA16, ValueType> {};

		template <DataLayout TDataLayout, typename ValueType>
		struct data_view_policy_soa_get {
			static_assert(std::is_copy_assignable_v<ValueType>);

			using view_policy = data_view_policy_soa<TDataLayout, ValueType>;

			template <size_t Item>
			struct data_view_policy_idx_info {
				using const_value_type = typename view_policy::template const_value_type<Item>;
			};

			//! Raw data pointed to by the view policy
			std::span<const uint8_t> m_data;

			data_view_policy_soa_get(std::span<uint8_t> data): m_data({(const uint8_t*)data.data(), data.size()}) {}
			data_view_policy_soa_get(std::span<const uint8_t> data): m_data({(const uint8_t*)data.data(), data.size()}) {}
			template <typename C>
			data_view_policy_soa_get(const C& c): m_data({(const uint8_t*)c.data(), c.size()}) {
				static_assert(!std::is_same_v<C, data_view_policy_soa_get>);
			}

			GAIA_NODISCARD constexpr auto operator[](size_t idx) const noexcept -> decltype(view_policy::get({}, 0)) {
				return view_policy::get(m_data, idx);
			}

			template <size_t Item>
			GAIA_NODISCARD constexpr auto get() const noexcept {
				auto s = view_policy::template get<Item>(m_data);
				return std::span(s.data(), s.size());
			}

			GAIA_NODISCARD auto data() const noexcept {
				return m_data.data();
			}

			GAIA_NODISCARD auto size() const noexcept {
				return m_data.size();
			}
		};

		template <typename ValueType>
		struct data_view_policy_get<DataLayout::SoA, ValueType>: data_view_policy_soa_get<DataLayout::SoA, ValueType> {};
		template <typename ValueType>
		struct data_view_policy_get<DataLayout::SoA8, ValueType>: data_view_policy_soa_get<DataLayout::SoA8, ValueType> {};
		template <typename ValueType>
		struct data_view_policy_get<DataLayout::SoA16, ValueType>:
				data_view_policy_soa_get<DataLayout::SoA16, ValueType> {};

		template <DataLayout TDataLayout, typename ValueType>
		struct data_view_policy_soa_set {
			static_assert(std::is_copy_assignable_v<ValueType>);

			using view_policy = data_view_policy_soa<TDataLayout, ValueType>;

			template <size_t Item>
			struct data_view_policy_idx_info {
				using value_type = typename view_policy::template value_type<Item>;
				using const_value_type = typename view_policy::template const_value_type<Item>;
			};

			//! Raw data pointed to by the view policy
			std::span<uint8_t> m_data;

			data_view_policy_soa_set(std::span<uint8_t> data): m_data({(uint8_t*)data.data(), data.size()}) {}
			data_view_policy_soa_set(std::span<const uint8_t> data): m_data({(uint8_t*)data.data(), data.size()}) {}
			template <typename C>
			data_view_policy_soa_set(const C& c): m_data({(uint8_t*)c.data(), c.size()}) {
				static_assert(!std::is_same_v<C, data_view_policy_soa_set>);
			}

			struct accessor {
				std::span<uint8_t> m_data;
				size_t m_idx;

				constexpr void operator=(const ValueType& val) noexcept {
					view_policy::set(m_data, m_idx) = val;
				}
				constexpr void operator=(ValueType&& val) noexcept {
					view_policy::set(m_data, m_idx) = std::forward<ValueType>(val);
				}
			};

			GAIA_NODISCARD constexpr auto operator[](size_t idx) const noexcept -> decltype(view_policy::get({}, 0)) {
				return view_policy::get({(const uint8_t*)m_data.data(), m_data.size()}, idx);
			}
			GAIA_NODISCARD constexpr auto operator[](size_t idx) noexcept {
				return accessor{m_data, idx};
			}

			template <size_t Item>
			GAIA_NODISCARD constexpr auto get() const noexcept {
				auto s = view_policy::template get<Item>(m_data);
				return std::span(s.data(), s.size());
			}

			template <size_t Item>
			GAIA_NODISCARD constexpr auto set() noexcept {
				auto s = view_policy::template set<Item>(m_data);
				return std::span(s.data(), s.size());
			}

			GAIA_NODISCARD auto data() const noexcept {
				return m_data.data();
			}

			GAIA_NODISCARD auto size() const noexcept {
				return m_data.size();
			}
		};

		template <typename ValueType>
		struct data_view_policy_set<DataLayout::SoA, ValueType>: data_view_policy_soa_set<DataLayout::SoA, ValueType> {};
		template <typename ValueType>
		struct data_view_policy_set<DataLayout::SoA8, ValueType>: data_view_policy_soa_set<DataLayout::SoA8, ValueType> {};
		template <typename ValueType>
		struct data_view_policy_set<DataLayout::SoA16, ValueType>:
				data_view_policy_soa_set<DataLayout::SoA16, ValueType> {};

		//----------------------------------------------------------------------
		// Helpers
		//----------------------------------------------------------------------

		namespace detail {
			template <typename, typename = void>
			struct auto_view_policy_inter {
				static constexpr DataLayout data_layout_type = DataLayout::AoS;
			};
			template <typename T>
			struct auto_view_policy_inter<T, std::void_t<decltype(T::Layout)>> {
				static constexpr DataLayout data_layout_type = T::Layout;
			};

			template <typename, typename = void>
			struct is_soa_layout: std::false_type {};
			template <typename T>
			struct is_soa_layout<T, std::void_t<decltype(T::Layout)>>: std::bool_constant<(T::Layout != DataLayout::AoS)> {};
		} // namespace detail

		template <typename T>
		using auto_view_policy = data_view_policy<detail::auto_view_policy_inter<T>::data_layout_type, T>;
		template <typename T>
		using auto_view_policy_get = data_view_policy_get<detail::auto_view_policy_inter<T>::data_layout_type, T>;
		template <typename T>
		using auto_view_policy_set = data_view_policy_set<detail::auto_view_policy_inter<T>::data_layout_type, T>;

		template <typename T>
		inline constexpr bool is_soa_layout_v = detail::is_soa_layout<T>::value;

		template <typename T, uint32_t N>
		using raw_data_holder = detail::raw_data_holder<N, auto_view_policy<T>::ArrayAlignment>;

	} // namespace mem
} // namespace gaia

#include <cinttypes>
#include <type_traits>
#include <utility>

namespace gaia {
	namespace mem {
		namespace detail {
			template <typename T>
			void copy_elements_aos(T* GAIA_RESTRICT dst, const T* GAIA_RESTRICT src, uint32_t idxSrc, uint32_t idxDst) {
				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(6385)

				static_assert(std::is_copy_assignable_v<T>);
				static_assert(!mem::is_soa_layout_v<T>);

				for (uint32_t i = idxSrc; i < idxDst; ++i)
					dst[i] = src[i];

				GAIA_MSVC_WARNING_POP()
			}

			template <typename T>
			void copy_elements_soa(
					uint8_t* GAIA_RESTRICT dst, const uint8_t* GAIA_RESTRICT src, uint32_t idxSrc, uint32_t idxDst,
					uint32_t sizeDst, uint32_t sizeSrc) {
				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(6385)

				for (uint32_t i = idxSrc; i < idxDst; ++i)
					(data_view_policy_set<T::Layout, T>({std::span<uint8_t>{dst, sizeDst}}))[i] =
							(data_view_policy_set<T::Layout, T>({std::span<const uint8_t>{(const uint8_t*)src, sizeSrc}}))[i];

				GAIA_MSVC_WARNING_POP()
			}

			template <typename T>
			void move_elements_aos(T* GAIA_RESTRICT dst, const T* GAIA_RESTRICT src, uint32_t idxSrc, uint32_t idxDst) {
				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(6385)

				static_assert(std::is_move_assignable_v<T>);
				static_assert(!mem::is_soa_layout_v<T>);

				for (uint32_t i = idxSrc; i < idxDst; ++i)
					dst[i] = std::move(src[i]);

				GAIA_MSVC_WARNING_POP()
			}

			//! Shift elements at the address pointed to by \param dst to the left by one
			template <typename T>
			void shift_elements_left_aos(T* GAIA_RESTRICT dst, uint32_t idxSrc, uint32_t idxDst) {
				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(6385)

				static_assert(!mem::is_soa_layout_v<T>);

				if constexpr (std::is_move_assignable_v<T>) {
					for (uint32_t i = idxSrc; i < idxDst; ++i)
						dst[i] = std::move(dst[i + 1]);
				} else {
					for (uint32_t i = idxSrc; i < idxDst; ++i)
						dst[i] = dst[i + 1];
				}

				GAIA_MSVC_WARNING_POP()
			}
			//! Shift elements at the address pointed to by \param dst to the left by one
			template <typename T>
			void shift_elements_left_soa(uint8_t* GAIA_RESTRICT dst, uint32_t size, uint32_t idxSrc, uint32_t idxDst) {
				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(6385)

				for (uint32_t i = idxSrc; i < idxDst; ++i)
					(data_view_policy_set<T::Layout, T>({std::span<uint8_t>{dst, size}}))[i] =
							(data_view_policy_get<T::Layout, T>({std::span<const uint8_t>{(const uint8_t*)dst, size}}))[i + 1];

				GAIA_MSVC_WARNING_POP()
			}
		} // namespace detail

		template <typename T>
		void construct_elements(T* pData, size_t cnt) {
			if constexpr (!mem::is_soa_layout_v<T>)
				core::call_ctor(pData, cnt);
		}

		template <typename T>
		void destruct_elements(T* pData, size_t cnt) {
			if constexpr (!mem::is_soa_layout_v<T>)
				core::call_ctor(pData, cnt);
		}

		//! Copy \param size elements of type \tparam T from the address pointer to by \param src to \param dst
		template <typename T>
		void copy_elements(
				uint8_t* GAIA_RESTRICT dst, const uint8_t* GAIA_RESTRICT src, uint32_t idxSrc, uint32_t idxDst,
				[[maybe_unused]] uint32_t sizeDst, [[maybe_unused]] uint32_t sizeSrc) {
			if constexpr (mem::is_soa_layout_v<T>)
				detail::copy_elements_soa<T>(dst, src, sizeDst, sizeSrc, idxSrc, idxDst);
			else
				detail::copy_elements_aos<T>((T*)dst, (const T*)src, idxSrc, idxDst);
		}

		//! Move or copy \param cnt elements of type \tparam T from the address pointer to by \param src to \param dst
		template <typename T>
		void move_elements(
				uint8_t* GAIA_RESTRICT dst, const uint8_t* GAIA_RESTRICT src, uint32_t idxSrc, uint32_t idxDst,
				[[maybe_unused]] uint32_t sizeDst, [[maybe_unused]] uint32_t sizeSrc) {
			if constexpr (std::is_move_assignable_v<T> && !mem::is_soa_layout_v<T>)
				detail::move_elements_aos<T>((T*)dst, (const T*)src, idxSrc, idxDst);
			else
				copy_elements<T>(dst, src, idxSrc, idxDst, sizeDst, sizeSrc);
		}

		//! Shift elements at the address pointed to by \param dst to the left by one
		template <typename T>
		void
		shift_elements_left(uint8_t* GAIA_RESTRICT dst, uint32_t idxSrc, uint32_t idxDst, [[maybe_unused]] uint32_t size) {
			if constexpr (mem::is_soa_layout_v<T>)
				detail::shift_elements_left_soa<T>(*dst, idxSrc, idxDst, size);
			else
				detail::shift_elements_left_aos<T>((T*)dst, idxSrc, idxDst);
		}
	} // namespace mem
} // namespace gaia

namespace gaia {
	namespace meta {

		//! Provides statically generated unique identifier for a given group of types.
		template <typename...>
		class type_group {
			inline static uint32_t s_identifier{};

		public:
			template <typename... Type>
			inline static const uint32_t id = s_identifier++;
		};

		template <>
		class type_group<void>;

		//----------------------------------------------------------------------
		// Type meta data
		//----------------------------------------------------------------------

		struct type_info final {
		private:
			constexpr static size_t find_first_of(const char* data, size_t len, char toFind, size_t startPos = 0) {
				for (size_t i = startPos; i < len; ++i) {
					if (data[i] == toFind)
						return i;
				}
				return size_t(-1);
			}

			constexpr static size_t find_last_of(const char* data, size_t len, char c, size_t startPos = size_t(-1)) {
				const auto minValue = startPos <= len - 1 ? startPos : len - 1;
				for (int64_t i = (int64_t)minValue; i >= 0; --i) {
					if (data[i] == c)
						return i;
				}
				return size_t(-1);
			}

		public:
			template <typename T>
			static uint32_t id() noexcept {
				return type_group<type_info>::id<T>;
			}

			template <typename T>
			GAIA_NODISCARD static constexpr const char* full_name() noexcept {
				return GAIA_PRETTY_FUNCTION;
			}

			template <typename T>
			GAIA_NODISCARD static constexpr auto name() noexcept {
				// MSVC:
				//		const char* __cdecl ecs::ComponentInfo::name<struct ecs::EnfEntity>(void)
				//   -> ecs::EnfEntity
				// Clang/GCC:
				//		const ecs::ComponentInfo::name() [T = ecs::EnfEntity]
				//   -> ecs::EnfEntity

				// Note:
				//		We don't want to use std::string_view here because it would only make it harder on compile-times.
				//		In fact, even if we did, we need to be afraid of compiler issues.
				// 		Clang 8 and older wouldn't compile because their string_view::find_last_of doesn't work
				//		in constexpr context. Tested with and without LIBCPP
				//		https://stackoverflow.com/questions/56484834/constexpr-stdstring-viewfind-last-of-doesnt-work-on-clang-8-with-libstdc
				//		As a workaround find_first_of and find_last_of were implemented

				size_t strLen = 0;
				while (GAIA_PRETTY_FUNCTION[strLen] != '\0')
					++strLen;

				std::span<const char> name{GAIA_PRETTY_FUNCTION, strLen};
				const auto prefixPos = find_first_of(name.data(), name.size(), GAIA_PRETTY_FUNCTION_PREFIX);
				const auto start = find_first_of(name.data(), name.size(), ' ', prefixPos + 1);
				const auto end = find_last_of(name.data(), name.size(), GAIA_PRETTY_FUNCTION_SUFFIX);
				return name.subspan(start + 1, end - start - 1);
			}

			template <typename T>
			GAIA_NODISCARD static constexpr auto hash() noexcept {
#if GAIA_COMPILER_MSVC && _MSV_VER <= 1916
				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(4307)
#endif

				auto n = name<T>();
				return core::calculate_hash64(n.data(), n.size());

#if GAIA_COMPILER_MSVC && _MSV_VER <= 1916
				GAIA_MSVC_WARNING_PUSH()
#endif
			}
		};

	} // namespace meta
} // namespace gaia

#include <type_traits>
#include <utility>

namespace gaia {
	namespace ser {
		namespace detail {
			enum class serialization_type_id : uint8_t {
				// Integer types
				s8 = 1,
				u8 = 2,
				s16 = 3,
				u16 = 4,
				s32 = 5,
				u32 = 6,
				s64 = 7,
				u64 = 8,

				// Boolean
				b = 40,

				// Character types
				c8 = 41,
				c16 = 42,
				c32 = 43,
				cw = 44,

				// Floating point types
				f8 = 81,
				f16 = 82,
				f32 = 83,
				f64 = 84,
				f128 = 85,

				// Special
				trivial_wrapper = 200,
				data_and_size = 201,

				Last = 255,
			};

			template <typename C>
			constexpr auto size(const C& c) noexcept -> decltype(c.size()) {
				return c.size();
			}
			template <typename T, auto N>
			constexpr std::size_t size(const T (&)[N]) noexcept {
				return N;
			}

			template <typename C>
			constexpr auto data(C& c) noexcept -> decltype(c.data()) {
				return c.data();
			}
			template <typename C>
			constexpr auto data(const C& c) noexcept -> decltype(c.data()) {
				return c.data();
			}
			template <typename T, auto N>
			constexpr T* data(T (&array)[N]) noexcept {
				return array;
			}
			template <typename E>
			constexpr const E* data(std::initializer_list<E> il) noexcept {
				return il.begin();
			}

			template <typename, typename = void>
			struct has_data_and_size: std::false_type {};
			template <typename T>
			struct has_data_and_size<T, std::void_t<decltype(data(std::declval<T>())), decltype(size(std::declval<T>()))>>:
					std::true_type {};

			GAIA_DEFINE_HAS_FUNCTION(resize);
			GAIA_DEFINE_HAS_FUNCTION(bytes);
			GAIA_DEFINE_HAS_FUNCTION(save);
			GAIA_DEFINE_HAS_FUNCTION(load);

			template <typename T>
			struct is_trivially_serializable {
			private:
				static constexpr bool update() {
					return std::is_enum_v<T> || std::is_fundamental_v<T> || std::is_trivially_copyable_v<T>;
				}

			public:
				static inline constexpr bool value = update();
			};

			template <typename T>
			GAIA_NODISCARD constexpr serialization_type_id int_kind_id() {
				if constexpr (std::is_same_v<int8_t, T> || std::is_same_v<signed char, T>) {
					return serialization_type_id::s8;
				} else if constexpr (std::is_same_v<uint8_t, T> || std::is_same_v<unsigned char, T>) {
					return serialization_type_id::u8;
				} else if constexpr (std::is_same_v<int16_t, T>) {
					return serialization_type_id::s16;
				} else if constexpr (std::is_same_v<uint16_t, T>) {
					return serialization_type_id::u16;
				} else if constexpr (std::is_same_v<int32_t, T>) {
					return serialization_type_id::s32;
				} else if constexpr (std::is_same_v<uint32_t, T>) {
					return serialization_type_id::u32;
				} else if constexpr (std::is_same_v<int64_t, T>) {
					return serialization_type_id::s64;
				} else if constexpr (std::is_same_v<uint64_t, T>) {
					return serialization_type_id::u64;
				} else if constexpr (std::is_same_v<bool, T>) {
					return serialization_type_id::b;
				}

				static_assert("Unsupported integral type");
			}

			template <typename T>
			GAIA_NODISCARD constexpr serialization_type_id flt_type_id() {
				// if constexpr (std::is_same_v<float8_t, T>) {
				// 	return serialization_type_id::f8;
				// } else if constexpr (std::is_same_v<float16_t, T>) {
				// 	return serialization_type_id::f16;
				// } else
				if constexpr (std::is_same_v<float, T>) {
					return serialization_type_id::f32;
				} else if constexpr (std::is_same_v<double, T>) {
					return serialization_type_id::f64;
				} else if constexpr (std::is_same_v<long double, T>) {
					return serialization_type_id::f128;
				}

				static_assert("Unsupported floating point type");
				return serialization_type_id::Last;
			}

			template <typename T>
			GAIA_NODISCARD constexpr serialization_type_id type_id() {
				if constexpr (std::is_enum_v<T>)
					return int_kind_id<std::underlying_type_t<T>>();
				else if constexpr (std::is_integral_v<T>)
					return int_kind_id<T>();
				else if constexpr (std::is_floating_point_v<T>)
					return flt_type_id<T>();
				else if constexpr (detail::has_data_and_size<T>::value)
					return serialization_type_id::data_and_size;
				else if constexpr (std::is_class_v<T>)
					return serialization_type_id::trivial_wrapper;

				static_assert("Unsupported serialization type");
				return serialization_type_id::Last;
			}

			template <typename T>
			GAIA_NODISCARD constexpr uint32_t bytes_one(const T& item) noexcept {
				using type = typename std::decay_t<typename std::remove_pointer_t<T>>;

				constexpr auto id = detail::type_id<type>();
				static_assert(id != detail::serialization_type_id::Last);
				uint32_t size_in_bytes{};

				// Custom bytes() has precedence
				if constexpr (has_bytes<type>::value) {
					size_in_bytes = (uint32_t)item.bytes();
				}
				// Trivially serializable types
				else if constexpr (is_trivially_serializable<type>::value) {
					size_in_bytes = (uint32_t)sizeof(type);
				}
				// Types which have data() and size() member functions
				else if constexpr (detail::has_data_and_size<type>::value) {
					size_in_bytes = (uint32_t)item.size();
				}
				// Classes
				else if constexpr (std::is_class_v<type>) {
					meta::each_member(item, [&](auto&&... items) {
						size_in_bytes += (bytes_one(items) + ...);
					});
				} else
					static_assert(!sizeof(type), "Type is not supported for serialization, yet");

				return size_in_bytes;
			}

			template <bool Write, typename Serializer, typename T>
			void ser_data_one(Serializer& s, T&& arg) {
				using type = typename std::decay_t<typename std::remove_pointer_t<T>>;

				// Custom save() & load() have precedence
				if constexpr (Write && has_save<type, Serializer&>::value) {
					arg.save(s);
				} else if constexpr (!Write && has_load<type, Serializer&>::value) {
					arg.load(s);
				}
				// Trivially serializable types
				else if constexpr (is_trivially_serializable<type>::value) {
					if constexpr (Write)
						s.save(std::forward<T>(arg));
					else
						s.load(std::forward<T>(arg));
				}
				// Types which have data() and size() member functions
				else if constexpr (detail::has_data_and_size<type>::value) {
					if constexpr (Write) {
						if constexpr (has_resize<type>::value) {
							const auto size = arg.size();
							s.save(size);
						}
						for (const auto& e: arg)
							ser_data_one<Write>(s, e);
					} else {
						if constexpr (has_resize<type>::value) {
							auto size = arg.size();
							s.load(size);
							arg.resize(size);
						}
						for (auto& e: arg)
							ser_data_one<Write>(s, e);
					}
				}
				// Classes
				else if constexpr (std::is_class_v<type>) {
					meta::each_member(std::forward<T>(arg), [&s](auto&&... items) {
						// TODO: Handle contiguous blocks of trivially copiable types
						(ser_data_one<Write>(s, items), ...);
					});
				} else
					static_assert(!sizeof(type), "Type is not supported for serialization, yet");
			}
		} // namespace detail

		//! Calculates the number of bytes necessary to serialize data using the "save" function.
		//! \warning Compile-time.
		template <typename T>
		GAIA_NODISCARD uint32_t bytes(const T& data) {
			return detail::bytes_one(data);
		}

		//! Write \param data using \tparam Writer at compile-time.
		//!
		//! \warning Writer has to implement a save function as follows:
		//! 					template <typename T> void save(const T& arg);
		template <typename Writer, typename T>
		void save(Writer& writer, const T& data) {
			detail::ser_data_one<true>(writer, data);
		}

		//! Read \param data using \tparam Reader at compile-time.
		//!
		//! \warning Reader has to implement a save function as follows:
		//! 					template <typename T> void load(T& arg);
		template <typename Reader, typename T>
		void load(Reader& reader, T& data) {
			detail::ser_data_one<false>(reader, data);
		}
	} // namespace ser
} // namespace gaia

#include <cinttypes>
#include <type_traits>

#include <cinttypes>
#include <type_traits>

namespace gaia {
	namespace cnt {
		template <typename TBitset, bool IsInverse>
		class bitset_const_iterator {
		public:
			using value_type = uint32_t;
			using size_type = typename TBitset::size_type;

		private:
			const TBitset& m_bitset;
			value_type m_pos;

			size_type item(uint32_t wordIdx) const {
				if constexpr (IsInverse)
					return ~m_bitset.data(wordIdx);
				else
					return m_bitset.data(wordIdx);
			}

			uint32_t find_next_set_bit(uint32_t pos) const {
				value_type wordIndex = pos / TBitset::BitsPerItem;
				const auto item_count = m_bitset.items();
				GAIA_ASSERT(wordIndex < item_count);
				size_type word = 0;

				const size_type posInWord = pos % TBitset::BitsPerItem + 1;
				if GAIA_LIKELY (posInWord < TBitset::BitsPerItem) {
					const size_type mask = (size_type(1) << posInWord) - 1;
					word = item(wordIndex) & (~mask);
				}

				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(4244)
				while (true) {
					if (word != 0) {
						if constexpr (TBitset::BitsPerItem == 32)
							return wordIndex * TBitset::BitsPerItem + GAIA_FFS(word) - 1;
						else
							return wordIndex * TBitset::BitsPerItem + GAIA_FFS64(word) - 1;
					}

					// No set bit in the current word, move to the next one
					if (++wordIndex >= item_count)
						return pos;

					word = item(wordIndex);
				}
				GAIA_MSVC_WARNING_POP()
			}

			uint32_t find_prev_set_bit(uint32_t pos) const {
				value_type wordIndex = pos / TBitset::BitsPerItem;
				GAIA_ASSERT(wordIndex < m_bitset.items());

				const size_type posInWord = pos % TBitset::BitsPerItem;
				const size_type mask = (size_type(1) << posInWord) - 1;
				size_type word = item(wordIndex) & mask;

				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(4244)
				while (true) {
					if (word != 0) {
						if constexpr (TBitset::BitsPerItem == 32)
							return TBitset::BitsPerItem * (wordIndex + 1) - GAIA_CTZ(word) - 1;
						else
							return TBitset::BitsPerItem * (wordIndex + 1) - GAIA_CTZ64(word) - 1;
					}

					// No set bit in the current word, move to the previous one
					if (wordIndex == 0)
						return pos;

					word = item(--wordIndex);
				}
				GAIA_MSVC_WARNING_POP()
			}

		public:
			bitset_const_iterator(const TBitset& bitset, value_type pos, bool fwd): m_bitset(bitset), m_pos(pos) {
				if (fwd) {
					if constexpr (IsInverse) {
						// Find the first set bit
						if (pos != 0 || bitset.test(0)) {
							pos = find_next_set_bit(m_pos);
							// Point beyond the last item if no set bit was found
							if (pos == m_pos)
								pos = bitset.size();
						}
					} else {
						// Find the first set bit
						if (pos != 0 || !bitset.test(0)) {
							pos = find_next_set_bit(m_pos);
							// Point beyond the last item if no set bit was found
							if (pos == m_pos)
								pos = bitset.size();
						}
					}
					m_pos = pos;
				} else {
					const auto bitsetSize = bitset.size();
					const auto lastBit = bitsetSize - 1;

					// Stay inside bounds
					if (pos >= bitsetSize)
						pos = bitsetSize - 1;

					if constexpr (IsInverse) {
						// Find the last set bit
						if (pos != lastBit || bitset.test(pos)) {
							const uint32_t newPos = find_prev_set_bit(pos);
							// Point one beyond the last found bit
							pos = (newPos == pos) ? bitsetSize : newPos + 1;
						}
						// Point one beyond the last found bit
						else
							++pos;
					} else {
						// Find the last set bit
						if (pos != lastBit || !bitset.test(pos)) {
							const uint32_t newPos = find_prev_set_bit(pos);
							// Point one beyond the last found bit
							pos = (newPos == pos) ? bitsetSize : newPos + 1;
						}
						// Point one beyond the last found bit
						else
							++pos;
					}

					m_pos = pos;
				}
			}

			GAIA_NODISCARD value_type operator*() const {
				return m_pos;
			}

			GAIA_NODISCARD value_type operator->() const {
				return m_pos;
			}

			GAIA_NODISCARD value_type index() const {
				return m_pos;
			}

			bitset_const_iterator& operator++() {
				uint32_t newPos = find_next_set_bit(m_pos);
				// Point one past the last item if no new bit was found
				if (newPos == m_pos)
					++newPos;
				m_pos = newPos;
				return *this;
			}

			GAIA_NODISCARD bitset_const_iterator operator++(int) {
				bitset_const_iterator temp(*this);
				++*this;
				return temp;
			}

			GAIA_NODISCARD bool operator==(const bitset_const_iterator& other) const {
				return m_pos == other.m_pos;
			}

			GAIA_NODISCARD bool operator!=(const bitset_const_iterator& other) const {
				return m_pos != other.m_pos;
			}
		};
	} // namespace cnt
} // namespace gaia

namespace gaia {
	namespace cnt {

		template <uint32_t NBits>
		class bitset {
		public:
			static constexpr uint32_t BitCount = NBits;
			static_assert(NBits > 0);

		private:
			template <bool Use32Bit>
			struct size_type_selector {
				using type = std::conditional_t<Use32Bit, uint32_t, uint64_t>;
			};

			static constexpr uint32_t BitsPerItem = (NBits / 64) > 0 ? 64 : 32;
			static constexpr uint32_t Items = (NBits + BitsPerItem - 1) / BitsPerItem;
			using size_type = typename size_type_selector<BitsPerItem == 32>::type;
			static constexpr bool HasTrailingBits = (NBits % BitsPerItem) != 0;
			static constexpr size_type LastItemMask = ((size_type)1 << (NBits % BitsPerItem)) - 1;

			size_type m_data[Items]{};

			//! Returns the number of words used by the bitset internally
			GAIA_NODISCARD constexpr uint32_t items() const {
				return Items;
			}

			//! Returns the word stored at the index \param wordIdx
			size_type data(uint32_t wordIdx) const {
				return m_data[wordIdx];
			}

		public:
			using const_iterator = bitset_const_iterator<bitset<NBits>, false>;
			friend const_iterator;
			using const_iterator_inverse = bitset_const_iterator<bitset<NBits>, true>;
			friend const_iterator_inverse;

			const_iterator begin() const {
				return const_iterator(*this, 0, true);
			}

			const_iterator end() const {
				return const_iterator(*this, NBits, false);
			}

			const_iterator_inverse begin_inverse() const {
				return const_iterator_inverse(*this, 0, true);
			}

			const_iterator_inverse end_inverse() const {
				return const_iterator_inverse(*this, NBits, false);
			}

			GAIA_NODISCARD constexpr bool operator[](uint32_t pos) const {
				return test(pos);
			}

			GAIA_NODISCARD constexpr bool operator==(const bitset& other) const {
				for (uint32_t i = 0; i < Items; ++i)
					if (m_data[i] != other.m_data[i])
						return false;
				return true;
			}

			GAIA_NODISCARD constexpr bool operator!=(const bitset& other) const {
				for (uint32_t i = 0; i < Items; ++i)
					if (m_data[i] == other.m_data[i])
						return false;
				return true;
			}

			//! Sets all bits
			constexpr void set() {
				if constexpr (HasTrailingBits) {
					for (uint32_t i = 0; i < Items - 1; ++i)
						m_data[i] = (size_type)-1;
					m_data[Items - 1] = LastItemMask;
				} else {
					for (uint32_t i = 0; i < Items; ++i)
						m_data[i] = (size_type)-1;
				}
			}

			//! Sets the bit at the postion \param pos to value \param value
			constexpr void set(uint32_t pos, bool value = true) {
				GAIA_ASSERT(pos < NBits);
				if (value)
					m_data[pos / BitsPerItem] |= ((size_type)1 << (pos % BitsPerItem));
				else
					m_data[pos / BitsPerItem] &= ~((size_type)1 << (pos % BitsPerItem));
			}

			//! Flips all bits
			constexpr bitset& flip() {
				if constexpr (HasTrailingBits) {
					for (uint32_t i = 0; i < Items - 1; ++i)
						m_data[i] = ~m_data[i];
					m_data[Items - 1] = (~m_data[Items - 1]) & LastItemMask;
				} else {
					for (uint32_t i = 0; i < Items; ++i)
						m_data[i] = ~m_data[i];
				}
				return *this;
			}

			//! Flips the bit at the postion \param pos
			constexpr void flip(uint32_t pos) {
				GAIA_ASSERT(pos < NBits);
				const auto wordIdx = pos / BitsPerItem;
				const auto bitIdx = pos % BitsPerItem;
				m_data[wordIdx] ^= ((size_type)1 << bitIdx);
			}

			//! Flips all bits from \param bitFrom to \param bitTo (including)
			constexpr bitset& flip(uint32_t bitFrom, uint32_t bitTo) {
				GAIA_ASSERT(bitFrom <= bitTo);
				GAIA_ASSERT(bitTo < size());

				// The followign can't happen because we always have at least 1 bit
				// if GAIA_UNLIKELY (size() == 0)
				// 	return *this;

				const uint32_t wordIdxFrom = bitFrom / BitsPerItem;
				const uint32_t wordIdxTo = bitTo / BitsPerItem;

				auto getMask = [](uint32_t from, uint32_t to) -> size_type {
					const auto diff = to - from;
					// Set all bits when asking for the full range
					if (diff == BitsPerItem - 1)
						return (size_type)-1;

					return ((size_type(1) << (diff + 1)) - 1) << from;
				};

				if (wordIdxFrom == wordIdxTo) {
					m_data[wordIdxTo] ^= getMask(bitFrom % BitsPerItem, bitTo % BitsPerItem);
				} else {
					// First word
					m_data[wordIdxFrom] ^= getMask(bitFrom % BitsPerItem, BitsPerItem - 1);
					// Middle
					for (uint32_t i = wordIdxFrom + 1; i <= wordIdxTo - 1; ++i)
						m_data[i] = ~m_data[i];
					// Last word
					m_data[wordIdxTo] ^= getMask(0, bitTo % BitsPerItem);
				}

				return *this;
			}

			//! Unsets all bits
			constexpr void reset() {
				for (uint32_t i = 0; i < Items; ++i)
					m_data[i] = 0;
			}

			//! Unsets the bit at the postion \param pos
			constexpr void reset(uint32_t pos) {
				GAIA_ASSERT(pos < NBits);
				m_data[pos / BitsPerItem] &= ~((size_type)1 << (pos % BitsPerItem));
			}

			//! Returns the value of the bit at the position \param pos
			GAIA_NODISCARD constexpr bool test(uint32_t pos) const {
				GAIA_ASSERT(pos < NBits);
				return (m_data[pos / BitsPerItem] & ((size_type)1 << (pos % BitsPerItem))) != 0;
			}

			//! Checks if all bits are set
			GAIA_NODISCARD constexpr bool all() const {
				if constexpr (HasTrailingBits) {
					for (uint32_t i = 0; i < Items - 1; ++i)
						if (m_data[i] != (size_type)-1)
							return false;
					return (m_data[Items - 1] & LastItemMask) == LastItemMask;
				} else {
					for (uint32_t i = 0; i < Items; ++i)
						if (m_data[i] != (size_type)-1)
							return false;
					return true;
				}
			}

			//! Checks if any bit is set
			GAIA_NODISCARD constexpr bool any() const {
				for (uint32_t i = 0; i < Items; ++i)
					if (m_data[i] != 0)
						return true;
				return false;
			}

			//! Checks if all bits are reset
			GAIA_NODISCARD constexpr bool none() const {
				for (uint32_t i = 0; i < Items; ++i)
					if (m_data[i] != 0)
						return false;
				return true;
			}

			//! Returns the number of set bits
			GAIA_NODISCARD uint32_t count() const {
				uint32_t total = 0;

				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(4244)
				if constexpr (sizeof(size_type) == 4) {
					for (uint32_t i = 0; i < Items; ++i)
						total += GAIA_POPCNT(m_data[i]);
				} else {
					for (uint32_t i = 0; i < Items; ++i)
						total += GAIA_POPCNT64(m_data[i]);
				}
				GAIA_MSVC_WARNING_POP()

				return total;
			}

			//! Returns the number of bits the bitset can hold
			GAIA_NODISCARD constexpr uint32_t size() const {
				return NBits;
			}
		};
	} // namespace cnt
} // namespace gaia

#include <cstddef>
#include <initializer_list>
#include <type_traits>
#include <utility>

namespace gaia {
	namespace cnt {
		namespace darr_detail {
			using difference_type = uint32_t;
			using size_type = uint32_t;
		} // namespace darr_detail

		template <typename T>
		struct darr_iterator {
			using iterator_category = core::random_access_iterator_tag;
			using value_type = T;
			using pointer = T*;
			using reference = T&;
			using difference_type = darr_detail::difference_type;
			using size_type = darr_detail::size_type;

			using iterator = darr_iterator;

		private:
			pointer m_ptr;

		public:
			darr_iterator(T* ptr): m_ptr(ptr) {}

			T& operator*() const {
				return *m_ptr;
			}
			T* operator->() const {
				return m_ptr;
			}
			iterator operator[](size_type offset) const {
				return {m_ptr + offset};
			}

			iterator& operator+=(size_type diff) {
				m_ptr += diff;
				return *this;
			}
			iterator& operator-=(size_type diff) {
				m_ptr -= diff;
				return *this;
			}
			iterator& operator++() {
				++m_ptr;
				return *this;
			}
			iterator operator++(int) {
				iterator temp(*this);
				++*this;
				return temp;
			}
			iterator& operator--() {
				--m_ptr;
				return *this;
			}
			iterator operator--(int) {
				iterator temp(*this);
				--*this;
				return temp;
			}

			iterator operator+(size_type offset) const {
				return {m_ptr + offset};
			}
			iterator operator-(size_type offset) const {
				return {m_ptr - offset};
			}
			difference_type operator-(const iterator& other) const {
				return (difference_type)(m_ptr - other.m_ptr);
			}

			GAIA_NODISCARD bool operator==(const iterator& other) const {
				return m_ptr == other.m_ptr;
			}
			GAIA_NODISCARD bool operator!=(const iterator& other) const {
				return m_ptr != other.m_ptr;
			}
			GAIA_NODISCARD bool operator>(const iterator& other) const {
				return m_ptr > other.m_ptr;
			}
			GAIA_NODISCARD bool operator>=(const iterator& other) const {
				return m_ptr >= other.m_ptr;
			}
			GAIA_NODISCARD bool operator<(const iterator& other) const {
				return m_ptr < other.m_ptr;
			}
			GAIA_NODISCARD bool operator<=(const iterator& other) const {
				return m_ptr <= other.m_ptr;
			}
		};

		template <typename T>
		struct darr_iterator_soa {
			using iterator_category = core::random_access_iterator_tag;
			using value_type = T;
			// using pointer = T*; not supported
			// using reference = T&; not supported
			using difference_type = darr_detail::difference_type;
			using size_type = darr_detail::size_type;

			using iterator = darr_iterator_soa;

		private:
			uint8_t* m_ptr;
			uint32_t m_cnt;
			uint32_t m_idx;

		public:
			darr_iterator_soa(uint8_t* ptr, uint32_t cnt, uint32_t idx): m_ptr(ptr), m_cnt(cnt), m_idx(idx) {}

			T operator*() const {
				return mem::data_view_policy<T::Layout, T>::get({m_ptr, m_cnt}, m_idx);
			}

			iterator operator[](size_type offset) const {
				return iterator(m_ptr, m_cnt, m_idx + offset);
			}

			iterator& operator+=(size_type diff) {
				m_idx += diff;
				return *this;
			}
			iterator& operator-=(size_type diff) {
				m_idx -= diff;
				return *this;
			}
			iterator& operator++() {
				++m_idx;
				return *this;
			}
			iterator operator++(int) {
				iterator temp(*this);
				++*this;
				return temp;
			}
			iterator& operator--() {
				--m_idx;
				return *this;
			}
			iterator operator--(int) {
				iterator temp(*this);
				--*this;
				return temp;
			}

			iterator operator+(size_type offset) const {
				return iterator(m_ptr, m_cnt, m_idx + offset);
			}
			iterator operator-(size_type offset) const {
				return iterator(m_ptr, m_cnt, m_idx + offset);
			}
			difference_type operator-(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return (difference_type)(m_idx - other.m_idx);
			}

			GAIA_NODISCARD bool operator==(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return m_idx == other.m_idx;
			}
			GAIA_NODISCARD bool operator!=(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return m_idx != other.m_idx;
			}
			GAIA_NODISCARD bool operator>(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return m_idx > other.m_idx;
			}
			GAIA_NODISCARD bool operator>=(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return m_idx >= other.m_idx;
			}
			GAIA_NODISCARD bool operator<(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return m_idx < other.m_idx;
			}
			GAIA_NODISCARD bool operator<=(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return m_idx <= other.m_idx;
			}
		};

		//! Array with variable size of elements of type \tparam T allocated on heap.
		//! Interface compatiblity with std::vector where it matters.
		template <typename T>
		class darr {
		public:
			using value_type = T;
			using reference = T&;
			using const_reference = const T&;
			using pointer = T*;
			using const_pointer = T*;
			using view_policy = mem::auto_view_policy<T>;
			using difference_type = darr_detail::difference_type;
			using size_type = darr_detail::size_type;

			using iterator = darr_iterator<T>;
			using iterator_soa = darr_iterator_soa<T>;

		private:
			uint8_t* m_pData = nullptr;
			size_type m_cnt = size_type(0);
			size_type m_cap = size_type(0);

			void try_grow() {
				const auto cnt = size();
				const auto cap = capacity();

				// Unless we reached the capacity don't do anything
				if GAIA_LIKELY (cap != 0 && cap != cnt)
					return;

				// If no data is allocated go with at least 4 elements
				if GAIA_UNLIKELY (m_pData == nullptr) {
					m_pData = view_policy::alloc_mem(m_cap = 4);
					return;
				}

				// Increase the size of an existing array.
				// We increase the capacity in multiples of 1.5 which is about the golden ratio (1.618).
				// This means we prefer more frequent allocations over memory fragmentation.
				uint8_t* pDataOld = m_pData;
				m_pData = view_policy::alloc_mem(m_cap = (cap * 3) / 2 + 1);
				mem::move_elements<T>(m_pData, pDataOld, 0, cnt, m_cap, cap);
				view_policy::free_mem(pDataOld, cnt);
			}

		public:
			constexpr darr() noexcept = default;

			darr(size_type count, const T& value) {
				resize(count);
				for (auto it: *this)
					*it = value;
			}

			darr(size_type count) {
				resize(count);
			}

			template <typename InputIt>
			darr(InputIt first, InputIt last) {
				const auto count = (size_type)core::distance(first, last);
				resize(count);

				if constexpr (std::is_pointer_v<InputIt>) {
					for (size_type i = 0; i < count; ++i)
						operator[](i) = first[i];
				} else if constexpr (std::is_same_v<typename InputIt::iterator_category, core::random_access_iterator_tag>) {
					for (size_type i = 0; i < count; ++i)
						operator[](i) = *(first[i]);
				} else {
					size_type i = 0;
					for (auto it = first; it != last; ++it)
						operator[](++i) = *it;
				}
			}

			darr(std::initializer_list<T> il): darr(il.begin(), il.end()) {}

			darr(const darr& other): darr(other.begin(), other.end()) {}

			darr(darr&& other) noexcept: m_pData(other.m_pData), m_cnt(other.m_cnt), m_cap(other.m_cap) {
				other.m_cnt = size_type(0);
				other.m_cap = size_type(0);
				other.m_pData = nullptr;
			}

			darr& operator=(std::initializer_list<T> il) {
				*this = darr(il.begin(), il.end());
				return *this;
			}

			darr& operator=(const darr& other) {
				GAIA_ASSERT(gaia::mem::addressof(other) != this);

				resize(other.size());
				mem::copy_elements<T>(
						(uint8_t*)m_pData, (const uint8_t*)other.m_pData, 0, other.size(), capacity(), other.capacity());

				return *this;
			}

			darr& operator=(darr&& other) noexcept {
				GAIA_ASSERT(gaia::mem::addressof(other) != this);

				view_policy::free_mem(m_pData, size());
				m_pData = other.m_pData;
				m_cnt = other.m_cnt;
				m_cap = other.m_cap;

				other.m_pData = nullptr;
				other.m_cnt = size_type(0);
				other.m_cap = size_type(0);

				return *this;
			}

			~darr() {
				view_policy::free_mem(m_pData, size());
			}

			GAIA_NODISCARD pointer data() noexcept {
				return (pointer)m_pData;
			}

			GAIA_NODISCARD const_pointer data() const noexcept {
				return (const_pointer)m_pData;
			}

			GAIA_NODISCARD auto operator[](size_type pos) noexcept -> decltype(view_policy::set({}, 0)) {
				GAIA_ASSERT(pos < size());
				return view_policy::set({(typename view_policy::TargetCastType)m_pData, capacity()}, pos);
			}

			GAIA_NODISCARD auto operator[](size_type pos) const noexcept -> decltype(view_policy::get({}, 0)) {
				GAIA_ASSERT(pos < size());
				return view_policy::get({(typename view_policy::TargetCastType)m_pData, capacity()}, pos);
			}

			void reserve(size_type count) {
				if (count <= m_cap)
					return;

				auto* pDataOld = m_pData;
				m_pData = view_policy::alloc_mem(count);
				if (pDataOld != nullptr) {
					mem::move_elements<T>(m_pData, pDataOld, 0, size(), count, m_cap);
					view_policy::free_mem(pDataOld, size());
				}

				m_cap = count;
			}

			void resize(size_type count) {
				if (count <= m_cap) {
					m_cnt = count;
					return;
				}

				auto* pDataOld = m_pData;
				m_pData = view_policy::alloc_mem(count);
				if GAIA_LIKELY (pDataOld != nullptr) {
					mem::move_elements<T>(m_pData, pDataOld, 0, size(), count, m_cap);
					view_policy::free_mem(pDataOld, size());
				}

				m_cap = count;
				m_cnt = count;
			}

			void push_back(const T& arg) {
				try_grow();

				if constexpr (mem::is_soa_layout_v<T>) {
					operator[](m_cnt++) = arg;
				} else {
					auto* ptr = m_pData + sizeof(T) * (m_cnt++);
					::new (ptr) T(arg);
				}
			}

			void push_back(T&& arg) {
				try_grow();

				if constexpr (mem::is_soa_layout_v<T>) {
					operator[](m_cnt++) = std::forward<T>(arg);
				} else {
					auto* ptr = m_pData + sizeof(T) * (m_cnt++);
					::new (ptr) T(std::forward<T>(arg));
				}
			}

			template <typename... Args>
			auto emplace_back(Args&&... args) {
				try_grow();

				if constexpr (mem::is_soa_layout_v<T>) {
					operator[](m_cnt++) = T(std::forward<Args>(args)...);
					return;
				} else {
					auto* ptr = m_pData + sizeof(T) * (m_cnt++);
					::new (ptr) T(std::forward<Args>(args)...);
					return (reference)*ptr;
				}
			}

			void pop_back() noexcept {
				GAIA_ASSERT(!empty());

				if constexpr (!mem::is_soa_layout_v<T>) {
					auto* ptr = m_pData + sizeof(T) * m_cnt;
					((pointer)ptr)->~T();
				}

				--m_cnt;
			}

			iterator erase(iterator pos) noexcept {
				GAIA_ASSERT(pos >= data() && pos < (data() + m_cap - 1));

				const auto idxSrc = (size_type)core::distance(pos, begin());
				const auto idxDst = size() - 1;

				mem::shift_elements_left<T>(m_pData, idxSrc, idxDst, m_cap);
				--m_cnt;

				return iterator((pointer)m_pData + idxSrc);
			}

			iterator_soa erase(iterator_soa pos) noexcept {
				const auto idxSrc = pos.m_idx;
				const auto idxDst = size() - 1;

				mem::shift_elements_left<T>(m_pData, idxSrc, idxDst);
				--m_cnt;

				return iterator_soa(m_pData, m_cnt, idxSrc);
			}

			iterator erase(iterator first, iterator last) noexcept {
				static_assert(!mem::is_soa_layout_v<T>);
				GAIA_ASSERT(first.m_cnt >= 0 && first.m_cnt < size());
				GAIA_ASSERT(last.m_cnt >= 0 && last.m_cnt < size());
				GAIA_ASSERT(last.m_cnt >= first.m_cnt);

				const auto cnt = last.m_cnt - first.m_cnt;
				mem::shift_elements_left<T>(m_pData, first.cnt, last.cnt);
				m_cnt -= cnt;

				return {(pointer)m_pData + size_type(last.m_cnt)};
			}

			iterator_soa erase(iterator_soa first, iterator_soa last) noexcept {
				static_assert(!mem::is_soa_layout_v<T>);
				GAIA_ASSERT(first.m_idx >= 0 && first.m_idx < size());
				GAIA_ASSERT(last.m_idx >= 0 && last.m_idx < size());
				GAIA_ASSERT(last.m_idx >= first.m_idx);

				const auto cnt = last.m_idx - first.m_idx;
				mem::shift_elements_left<T>(m_pData, first.cnt, last.cnt);
				m_cnt -= cnt;

				return iterator_soa(m_pData, m_cnt, last.m_cnt);
			}

			void clear() {
				resize(0);
			}

			void shirk_to_fit() {
				if (capacity() == size())
					return;

				auto* pDataOld = m_pData;
				m_pData = view_policy::mem_alloc(m_cap = size());
				mem::move_elements<T>(m_pData, pDataOld, 0, size());
				view_policy::mem_free(pDataOld);
			}

			GAIA_NODISCARD size_type size() const noexcept {
				return m_cnt;
			}

			GAIA_NODISCARD size_type capacity() const noexcept {
				return m_cap;
			}

			GAIA_NODISCARD bool empty() const noexcept {
				return size() == 0;
			}

			GAIA_NODISCARD size_type max_size() const noexcept {
				return static_cast<size_type>(-1);
			}

			GAIA_NODISCARD auto front() noexcept {
				GAIA_ASSERT(!empty());
				if constexpr (mem::is_soa_layout_v<T>)
					return *begin();
				else
					return (reference)*begin();
			}

			GAIA_NODISCARD auto front() const noexcept {
				GAIA_ASSERT(!empty());
				if constexpr (mem::is_soa_layout_v<T>)
					return *begin();
				else
					return (const_reference)*begin();
			}

			GAIA_NODISCARD auto back() noexcept {
				GAIA_ASSERT(!empty());
				if constexpr (mem::is_soa_layout_v<T>)
					return operator[](m_cnt - 1);
				else
					return (reference)(operator[](m_cnt - 1));
			}

			GAIA_NODISCARD auto back() const noexcept {
				GAIA_ASSERT(!empty());
				if constexpr (mem::is_soa_layout_v<T>)
					return operator[](m_cnt - 1);
				else
					return (const_reference) operator[](m_cnt - 1);
			}

			GAIA_NODISCARD auto begin() const noexcept {
				if constexpr (mem::is_soa_layout_v<T>)
					return iterator_soa(m_pData, size(), 0);
				else
					return iterator((pointer)m_pData);
			}

			GAIA_NODISCARD auto rbegin() const noexcept {
				if constexpr (mem::is_soa_layout_v<T>)
					return iterator_soa(m_pData, size(), size() - 1);
				else
					return iterator((pointer)&back());
			}

			GAIA_NODISCARD auto end() const noexcept {
				if constexpr (mem::is_soa_layout_v<T>)
					return iterator_soa(m_pData, size(), size());
				else
					return iterator((pointer)m_pData + size());
			}

			GAIA_NODISCARD auto rend() const noexcept {
				if constexpr (mem::is_soa_layout_v<T>)
					return iterator_soa(m_pData, size(), -1);
				else
					return iterator((pointer)m_pData - 1);
			}

			GAIA_NODISCARD bool operator==(const darr& other) const {
				if (m_cnt != other.m_cnt)
					return false;
				const size_type n = size();
				for (size_type i = 0; i < n; ++i)
					if (!(operator[](i) == other[i]))
						return false;
				return true;
			}

			template <size_t Item>
			auto soa_view_mut() noexcept {
				return mem::data_view_policy<T::Layout, T>::template get<Item>(
						std::span<uint8_t>{(uint8_t*)m_pData, capacity()});
			}

			template <size_t Item>
			auto soa_view() const noexcept {
				return mem::data_view_policy<T::Layout, T>::template get<Item>(
						std::span<const uint8_t>{(const uint8_t*)m_pData, capacity()});
			}
		};
	} // namespace cnt

} // namespace gaia

namespace gaia {
	namespace cnt {
		template <typename T>
		using darray = cnt::darr<T>;
	} // namespace cnt
} // namespace gaia

#include <cstddef>
#include <initializer_list>
#include <type_traits>
#include <utility>

namespace gaia {
	namespace cnt {
		namespace darr_ext_detail {
			using difference_type = uint32_t;
			using size_type = uint32_t;
		} // namespace darr_ext_detail

		template <typename T>
		struct darr_ext_iterator {
			using iterator_category = core::random_access_iterator_tag;
			using value_type = T;
			using pointer = T*;
			using reference = T&;
			using difference_type = darr_ext_detail::difference_type;
			using size_type = darr_ext_detail::size_type;

			using iterator = darr_ext_iterator;

		private:
			pointer m_ptr;

		public:
			darr_ext_iterator(T* ptr): m_ptr(ptr) {}

			T& operator*() const {
				return *m_ptr;
			}
			T* operator->() const {
				return m_ptr;
			}
			iterator operator[](size_type offset) const {
				return {m_ptr + offset};
			}

			iterator& operator+=(size_type diff) {
				m_ptr += diff;
				return *this;
			}
			iterator& operator-=(size_type diff) {
				m_ptr -= diff;
				return *this;
			}
			iterator& operator++() {
				++m_ptr;
				return *this;
			}
			iterator operator++(int) {
				iterator temp(*this);
				++*this;
				return temp;
			}
			iterator& operator--() {
				--m_ptr;
				return *this;
			}
			iterator operator--(int) {
				iterator temp(*this);
				--*this;
				return temp;
			}

			iterator operator+(size_type offset) const {
				return {m_ptr + offset};
			}
			iterator operator-(size_type offset) const {
				return {m_ptr - offset};
			}
			difference_type operator-(const iterator& other) const {
				return (difference_type)(m_ptr - other.m_ptr);
			}

			GAIA_NODISCARD bool operator==(const iterator& other) const {
				return m_ptr == other.m_ptr;
			}
			GAIA_NODISCARD bool operator!=(const iterator& other) const {
				return m_ptr != other.m_ptr;
			}
			GAIA_NODISCARD bool operator>(const iterator& other) const {
				return m_ptr > other.m_ptr;
			}
			GAIA_NODISCARD bool operator>=(const iterator& other) const {
				return m_ptr >= other.m_ptr;
			}
			GAIA_NODISCARD bool operator<(const iterator& other) const {
				return m_ptr < other.m_ptr;
			}
			GAIA_NODISCARD bool operator<=(const iterator& other) const {
				return m_ptr <= other.m_ptr;
			}
		};

		template <typename T>
		struct darr_ext_iterator_soa {
			using iterator_category = core::random_access_iterator_tag;
			using value_type = T;
			// using pointer = T*; not supported
			// using reference = T&; not supported
			using difference_type = darr_ext_detail::difference_type;
			using size_type = darr_ext_detail::size_type;

			using iterator = darr_ext_iterator_soa;

		private:
			uint8_t* m_ptr;
			uint32_t m_cnt;
			uint32_t m_idx;

		public:
			darr_ext_iterator_soa(uint8_t* ptr, uint32_t cnt, uint32_t idx): m_ptr(ptr), m_cnt(cnt), m_idx(idx) {}

			T operator*() const {
				return mem::data_view_policy<T::Layout, T>::get({m_ptr, m_cnt}, m_idx);
			}

			iterator operator[](size_type offset) const {
				return iterator(m_ptr, m_cnt, m_idx + offset);
			}

			iterator& operator+=(size_type diff) {
				m_idx += diff;
				return *this;
			}
			iterator& operator-=(size_type diff) {
				m_idx -= diff;
				return *this;
			}
			iterator& operator++() {
				++m_idx;
				return *this;
			}
			iterator operator++(int) {
				iterator temp(*this);
				++*this;
				return temp;
			}
			iterator& operator--() {
				--m_idx;
				return *this;
			}
			iterator operator--(int) {
				iterator temp(*this);
				--*this;
				return temp;
			}

			iterator operator+(size_type offset) const {
				return iterator(m_ptr, m_cnt, m_idx + offset);
			}
			iterator operator-(size_type offset) const {
				return iterator(m_ptr, m_cnt, m_idx + offset);
			}
			difference_type operator-(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return (difference_type)(m_idx - other.m_idx);
			}

			GAIA_NODISCARD bool operator==(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return m_idx == other.m_idx;
			}
			GAIA_NODISCARD bool operator!=(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return m_idx != other.m_idx;
			}
			GAIA_NODISCARD bool operator>(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return m_idx > other.m_idx;
			}
			GAIA_NODISCARD bool operator>=(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return m_idx >= other.m_idx;
			}
			GAIA_NODISCARD bool operator<(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return m_idx < other.m_idx;
			}
			GAIA_NODISCARD bool operator<=(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return m_idx <= other.m_idx;
			}
		};

		//! Array of elements of type \tparam T allocated on heap or stack. Stack capacity is \tparam N elements.
		//! If the number of elements is bellow \tparam N the stack storage is used.
		//! If the number of elements is above \tparam N the heap storage is used.
		//! Interface compatiblity with std::vector and std::array where it matters.
		template <typename T, darr_ext_detail::size_type N>
		class darr_ext {
		public:
			static_assert(N > 0);

			using iterator_category = core::random_access_iterator_tag;
			using value_type = T;
			using reference = T&;
			using const_reference = const T&;
			using pointer = T*;
			using const_pointer = T*;
			using view_policy = mem::auto_view_policy<T>;
			using difference_type = darr_ext_detail::difference_type;
			using size_type = darr_ext_detail::size_type;

			using iterator = darr_ext_iterator<T>;
			using iterator_soa = darr_ext_iterator_soa<T>;

			static constexpr size_type extent = N;
			static constexpr uint32_t allocated_bytes = view_policy::get_min_byte_size(0, N);

		private:
			//! Data allocated on the stack
			mem::raw_data_holder<T, allocated_bytes> m_data;
			//! Data allocated on the heap
			T* m_pDataHeap = nullptr;
			//! Pointer to the currently used data
			T* m_pData = m_data;
			//! Number of currently used items ín this container
			size_type m_cnt = size_type(0);
			//! Allocated capacity of m_dataDyn or the extend
			size_type m_cap = extent;

			void try_grow() noexcept {
				// Unless we are above stack allocated size don't do anything
				const auto cnt = size();
				if (cnt < extent)
					return;

				// Unless we reached the capacity don't do anything
				const auto cap = capacity();
				if GAIA_LIKELY (cnt != cap)
					return;

				// If no data is allocated go with at least 4 elements
				if GAIA_UNLIKELY (m_pDataHeap == nullptr) {
					m_pDataHeap = view_policy::alloc_mem(m_cap = 4);
					return;
				}

				// Increase the size of an existing array.
				// We increase the capacity in multiples of 1.5 which is about the golden ratio (1.618).
				// This means we prefer more frequent allocations over memory fragmentation.
				auto* pDataOld = m_pDataHeap;
				m_pDataHeap = view_policy::alloc_mem(m_cap = (cap * 3) / 2 + 1);
				mem::move_elements<T>(m_pDataHeap, pDataOld, 0, cnt);
				view_policy::free_mem(pDataOld, cnt);
			}

		public:
			darr_ext() noexcept = default;

			darr_ext(size_type count, const T& value) {
				resize(count);
				for (auto it: *this)
					*it = value;
			}

			darr_ext(size_type count) {
				resize(count);
			}

			template <typename InputIt>
			darr_ext(InputIt first, InputIt last) {
				const auto count = (size_type)core::distance(first, last);
				resize(count);

				if constexpr (std::is_pointer_v<InputIt>) {
					for (size_type i = 0; i < count; ++i)
						operator[](i) = first[i];
				} else if constexpr (std::is_same_v<typename InputIt::iterator_category, core::random_access_iterator_tag>) {
					for (size_type i = 0; i < count; ++i)
						operator[](i) = *(first[i]);
				} else {
					size_type i = 0;
					for (auto it = first; it != last; ++it)
						operator[](++i) = *it;
				}
			}

			darr_ext(std::initializer_list<T> il): darr_ext(il.begin(), il.end()) {}

			darr_ext(const darr_ext& other): darr_ext(other.begin(), other.end()) {}

			darr_ext(darr_ext&& other) noexcept: m_cnt(other.m_cnt), m_cap(other.m_cap) {
				GAIA_ASSERT(gaia::mem::addressof(other) != this);

				if (other.m_pData == other.m_pDataHeap) {
					if (m_pData == m_pDataHeap)
						view_policy::free_mem(m_pDataHeap);
					m_pData = m_pDataHeap;
					m_pDataHeap = other.m_pDataHeap;
				} else {
					m_pData = m_data;
					mem::move_elements<T>(m_data, other.m_data, 0, other.size());
					m_pDataHeap = nullptr;
				}

				other.m_pDataHeap = nullptr;
				other.m_pData = m_data;
				other.m_cnt = size_type(0);
				other.m_cap = extent;
			}

			darr_ext& operator=(std::initializer_list<T> il) {
				*this = darr_ext(il.begin(), il.end());
				return *this;
			}

			darr_ext& operator=(const darr_ext& other) {
				GAIA_ASSERT(gaia::mem::addressof(other) != this);

				resize(other.size());
				mem::copy_elements<T>(
						(uint8_t*)m_pData, (const uint8_t*)other.m_pData, 0, other.size(), capacity(), other.capacity());

				return *this;
			}

			darr_ext& operator=(darr_ext&& other) noexcept {
				GAIA_ASSERT(gaia::mem::addressof(other) != this);

				m_cnt = other.m_cnt;
				m_cap = other.m_cap;
				if (other.m_pData == other.m_pDataHeap) {
					m_pData = m_pDataHeap;
					m_pDataHeap = other.m_pDataHeap;
				} else {
					m_pData = m_data;
					mem::move_elements<T>(m_data, other.m_data, 0, other.m_data.size());
					m_pDataHeap = nullptr;
				}

				other.m_cnt = size_type(0);
				other.m_cap = extent;
				other.m_pDataHeap = nullptr;
				other.m_pData = m_data;

				return *this;
			}

			~darr_ext() {
				view_policy::free_mem(m_pDataHeap, size());
			}

			GAIA_NODISCARD pointer data() noexcept {
				return (pointer)m_pData;
			}

			GAIA_NODISCARD const_pointer data() const noexcept {
				return (const_pointer)m_pData;
			}

			GAIA_NODISCARD auto operator[](size_type pos) noexcept -> decltype(view_policy::set({}, 0)) {
				GAIA_ASSERT(pos < size());
				return view_policy::set({(typename view_policy::TargetCastType)m_pData, size()}, pos);
			}

			GAIA_NODISCARD auto operator[](size_type pos) const noexcept -> decltype(view_policy::get({}, 0)) {
				GAIA_ASSERT(pos < size());
				return view_policy::get({(typename view_policy::TargetCastType)m_pData, size()}, pos);
			}

			void reserve(size_type count) {
				if (count <= m_cap)
					return;

				if (m_pDataHeap) {
					auto* pDataOld = m_pDataHeap;
					m_pDataHeap = view_policy::alloc_mem(count);
					mem::move_elements<T>(m_pDataHeap, pDataOld, 0, size(), count, m_cap);
					view_policy::free_mem(pDataOld, size());
				} else {
					m_pDataHeap = view_policy::alloc_mem(count);
					mem::move_elements<T>(m_pDataHeap, m_data, 0, size(), count, m_cap);
				}

				m_cap = count;
				m_pData = m_pDataHeap;
			}

			void resize(size_type count) {
				if (count <= extent || count <= m_cap) {
					m_cnt = count;
					return;
				}

				auto* pDataOld = m_pDataHeap;
				m_pDataHeap = view_policy::alloc_mem(count);
				if (pDataOld != nullptr) {
					mem::move_elements<T>(m_pDataHeap, pDataOld, 0, size(), count, m_cap);
					view_policy::free_mem(pDataOld, size());
				} else
					mem::move_elements<T>(m_pDataHeap, m_data, 0, size(), count, m_cap);

				m_cap = count;
				m_cnt = count;
				m_pData = m_pDataHeap;
			}

			void push_back(const T& arg) {
				try_grow();

				if constexpr (mem::is_soa_layout_v<T>) {
					operator[](m_cnt++) = arg;
				} else {
					auto* ptr = m_pData + sizeof(T) * (m_cnt++);
					::new (ptr) T(arg);
				}
			}

			void push_back(T&& arg) {
				try_grow();

				if constexpr (mem::is_soa_layout_v<T>) {
					operator[](m_cnt++) = std::forward<T>(arg);
				} else {
					auto* ptr = m_pData + sizeof(T) * (m_cnt++);
					::new (ptr) T(std::forward<T>(arg));
				}
			}

			template <typename... Args>
			auto emplace_back(Args&&... args) {
				try_grow();

				if constexpr (mem::is_soa_layout_v<T>) {
					operator[](m_cnt++) = T(std::forward<Args>(args)...);
					return;
				} else {
					auto* ptr = m_pData + sizeof(T) * (m_cnt++);
					::new (ptr) T(std::forward<Args>(args)...);
					return (reference)*ptr;
				}
			}

			void pop_back() noexcept {
				GAIA_ASSERT(!empty());

				if constexpr (!mem::is_soa_layout_v<T>) {
					auto* ptr = m_pData + sizeof(T) * m_cnt;
					((pointer)ptr)->~T();
				}

				--m_cnt;
			}

			iterator erase(iterator pos) noexcept {
				GAIA_ASSERT(pos.m_ptr >= data() && pos.m_ptr < (data() + m_cap - 1));

				const auto idxSrc = (size_type)core::distance(pos, begin());
				const auto idxDst = size() - 1;

				mem::shift_elements_left<T>(m_pData, idxSrc, idxDst);
				--m_cnt;

				return iterator((pointer)m_pData + idxSrc);
			}

			iterator_soa erase(iterator_soa pos) noexcept {
				const auto idxSrc = pos.m_idx;
				const auto idxDst = size() - 1;

				mem::shift_elements_left<T>(m_pData, idxSrc, idxDst);
				--m_cnt;

				return iterator_soa(m_pData, m_cnt, idxSrc);
			}

			iterator erase(iterator first, iterator last) noexcept {
				GAIA_ASSERT(first.m_cnt >= 0 && first.m_cnt < size());
				GAIA_ASSERT(last.m_cnt >= 0 && last.m_cnt < size());
				GAIA_ASSERT(last.m_cnt >= first.m_cnt);

				const auto size = last.m_cnt - first.m_cnt;
				mem::shift_elements_left<T>(m_pData, first.cnt, last.cnt);
				m_cnt -= size;

				return {(pointer)m_pData + size_type(last.m_cnt)};
			}

			iterator_soa erase(iterator_soa first, iterator_soa last) noexcept {
				static_assert(!mem::is_soa_layout_v<T>);
				GAIA_ASSERT(first.m_idx >= 0 && first.m_idx < size());
				GAIA_ASSERT(last.m_idx >= 0 && last.m_idx < size());
				GAIA_ASSERT(last.m_idx >= first.m_idx);

				const auto cnt = last.m_idx - first.m_idx;
				mem::shift_elements_left<T>(m_pData, first.cnt, last.cnt);
				m_cnt -= cnt;

				return iterator_soa(m_pData, m_cnt, last.m_cnt);
			}

			void clear() {
				resize(0);
			}

			void shirk_to_fit() {
				if (capacity() == size())
					return;

				if (m_pData == m_pDataHeap) {
					auto* pDataOld = m_pDataHeap;

					if (size() < extent) {
						mem::move_elements<T>(m_data, pDataOld, 0, size());
						m_pData = m_data;
					} else {
						m_pDataHeap = view_policy::mem_alloc(m_cap = size());
						mem::move_elements<T>(m_pDataHeap, pDataOld, 0, size());
						m_pData = m_pDataHeap;
					}

					view_policy::mem_free(pDataOld);
				} else
					resize(size());
			}

			GAIA_NODISCARD size_type size() const noexcept {
				return m_cnt;
			}

			GAIA_NODISCARD size_type capacity() const noexcept {
				return m_cap;
			}

			GAIA_NODISCARD bool empty() const noexcept {
				return size() == 0;
			}

			GAIA_NODISCARD size_type max_size() const noexcept {
				return N;
			}

			GAIA_NODISCARD auto front() noexcept {
				GAIA_ASSERT(!empty());
				if constexpr (mem::is_soa_layout_v<T>)
					return *begin();
				else
					return (reference)*begin();
			}

			GAIA_NODISCARD auto front() const noexcept {
				GAIA_ASSERT(!empty());
				if constexpr (mem::is_soa_layout_v<T>)
					return *begin();
				else
					return (const_reference)*begin();
			}

			GAIA_NODISCARD auto back() noexcept {
				GAIA_ASSERT(!empty());
				if constexpr (mem::is_soa_layout_v<T>)
					return operator[](m_cnt - 1);
				else
					return (reference) operator[](m_cnt - 1);
			}

			GAIA_NODISCARD auto back() const noexcept {
				GAIA_ASSERT(!empty());
				if constexpr (mem::is_soa_layout_v<T>)
					return operator[](m_cnt - 1);
				else
					return (const_reference) operator[](m_cnt - 1);
			}

			GAIA_NODISCARD auto begin() const noexcept {
				if constexpr (mem::is_soa_layout_v<T>)
					return iterator_soa(m_pData, size(), 0);
				else
					return iterator((pointer)m_pData);
			}

			GAIA_NODISCARD auto rbegin() const noexcept {
				if constexpr (mem::is_soa_layout_v<T>)
					return iterator_soa(m_pData, size(), size() - 1);
				else
					return iterator((pointer)&back());
			}

			GAIA_NODISCARD auto end() const noexcept {
				if constexpr (mem::is_soa_layout_v<T>)
					return iterator_soa(m_pData, size(), size());
				else
					return iterator((pointer)m_pData + size());
			}

			GAIA_NODISCARD auto rend() const noexcept {
				if constexpr (mem::is_soa_layout_v<T>)
					return iterator_soa(m_pData, size(), -1);
				else
					return iterator((pointer)m_pData - 1);
			}

			GAIA_NODISCARD bool operator==(const darr_ext& other) const {
				if (m_cnt != other.m_cnt)
					return false;
				const size_type n = size();
				for (size_type i = 0; i < n; ++i)
					if (!(m_pData[i] == other.m_pData[i]))
						return false;
				return true;
			}

			template <size_t Item>
			auto soa_view_mut() noexcept {
				return mem::data_view_policy<T::Layout, T>::template get<Item>(
						std::span<uint8_t>{(uint8_t*)m_pData, capacity()});
			}

			template <size_t Item>
			auto soa_view() const noexcept {
				return mem::data_view_policy<T::Layout, T>::template get<Item>(
						std::span<const uint8_t>{(const uint8_t*)m_pData, capacity()});
			}
		};

		namespace detail {
			template <typename T, uint32_t N, uint32_t... I>
			darr_ext<std::remove_cv_t<T>, N> to_sarray_impl(T (&a)[N], std::index_sequence<I...> /*no_name*/) {
				return {{a[I]...}};
			}
		} // namespace detail

		template <typename T, uint32_t N>
		darr_ext<std::remove_cv_t<T>, N> to_sarray(T (&a)[N]) {
			return detail::to_sarray_impl(a, std::make_index_sequence<N>{});
		}

	} // namespace cnt

} // namespace gaia

namespace gaia {
	namespace cnt {
		template <typename T, darr_ext_detail::size_type N>
		using darray_ext = cnt::darr_ext<T, N>;
	} // namespace cnt
} // namespace gaia

#include <cinttypes>
#include <type_traits>

namespace gaia {
	namespace cnt {
		class dbitset {
		private:
			struct size_type_selector {
				static constexpr bool Use32Bit = sizeof(size_t) == 4;
				using type = std::conditional_t<Use32Bit, uint32_t, uint64_t>;
			};

			using difference_type = typename size_type_selector::type;
			using size_type = typename size_type_selector::type;
			using value_type = size_type;
			using reference = size_type&;
			using const_reference = const size_type&;
			using pointer = size_type*;
			using const_pointer = size_type*;

			static constexpr uint32_t BitsPerItem = sizeof(size_type_selector::type) * 8;

			pointer m_pData = nullptr;
			uint32_t m_cnt = uint32_t(0);
			uint32_t m_cap = uint32_t(0);

			uint32_t items() const {
				return (m_cnt + BitsPerItem - 1) / BitsPerItem;
			}

			bool has_trailing_bits() const {
				return (m_cnt % BitsPerItem) != 0;
			}

			size_type last_item_mask() const {
				return ((size_type)1 << (m_cnt % BitsPerItem)) - 1;
			}

			void try_grow(uint32_t bitsWanted) {
				uint32_t itemsOld = items();
				if GAIA_UNLIKELY (bitsWanted > size())
					m_cnt = bitsWanted;
				if GAIA_LIKELY (m_cnt <= capacity())
					return;

				// Increase the size of an existing array.
				// We are pessimistic with our allocations and only allocate as much as we need.
				// If we know the expected size ahead of the time a manual call to reserve is necessary.
				const uint32_t itemsNew = (m_cnt + BitsPerItem - 1) / BitsPerItem;
				m_cap = itemsNew * BitsPerItem;

				pointer pDataOld = m_pData;
				m_pData = new size_type[itemsNew];
				if (pDataOld == nullptr) {
					// Make sure the new data is set to zeros
					for (uint32_t i = itemsOld; i < itemsNew; ++i)
						m_pData[i] = 0;
				} else {
					// Copy the old data over and set the old data to zeros
					mem::copy_elements<size_type>((uint8_t*)m_pData, (const uint8_t*)pDataOld, 0, itemsOld, 0, 0);
					for (uint32_t i = itemsOld; i < itemsNew; ++i)
						m_pData[i] = 0;

					// Release the old data
					delete[] pDataOld;
				}
			}

			//! Returns the word stored at the index \param wordIdx
			size_type data(uint32_t wordIdx) const {
				return m_pData[wordIdx];
			}

		public:
			using const_iterator = bitset_const_iterator<dbitset, false>;
			friend const_iterator;
			using const_iterator_inverse = bitset_const_iterator<dbitset, true>;
			friend const_iterator_inverse;

			dbitset(): m_cnt(1) {
				// Allocate at least 128 bits
				reserve(128);
			}

			dbitset(uint32_t reserveBits): m_cnt(1) {
				reserve(reserveBits);
			}

			~dbitset() {
				delete[] m_pData;
			}

			dbitset(const dbitset& other) {
				*this = other;
			}

			dbitset& operator=(const dbitset& other) {
				GAIA_ASSERT(gaia::mem::addressof(other) != this);

				resize(other.m_cnt);
				mem::copy_elements<size_type>((uint8_t*)m_pData, (const uint8_t*)other.m_pData, 0, other.items(), 0, 0);
				return *this;
			}

			dbitset(dbitset&& other) noexcept {
				*this = std::move(other);
			}

			dbitset& operator=(dbitset&& other) noexcept {
				GAIA_ASSERT(gaia::mem::addressof(other) != this);

				m_pData = other.m_pData;
				m_cnt = other.m_cnt;
				m_cap = other.m_cap;

				other.m_pData = nullptr;
				other.m_cnt = 0;
				other.m_cap = 0;
				return *this;
			}

			void reserve(uint32_t bitsWanted) {
				// Make sure at least one bit is requested
				GAIA_ASSERT(bitsWanted > 0);
				if (bitsWanted < 1)
					bitsWanted = 1;

				// Nothing to do if the capacity is already bigger than requested
				if (bitsWanted <= capacity())
					return;

				const uint32_t itemsNew = (bitsWanted + BitsPerItem - 1) / BitsPerItem;
				auto* pDataOld = m_pData;
				m_pData = new size_type[itemsNew];
				if (pDataOld == nullptr) {
					// Make sure the new data is set to zeros
					for (uint32_t i = 0; i < itemsNew; ++i)
						m_pData[i] = 0;
				} else {
					const uint32_t itemsOld = items();
					// Copy the old data over and set the old data to zeros
					mem::copy_elements<size_type>((uint8_t*)m_pData, (const uint8_t*)pDataOld, 0, itemsOld, 0, 0);
					for (uint32_t i = itemsOld; i < itemsNew; ++i)
						m_pData[i] = 0;

					// Release old data
					delete[] pDataOld;
				}

				m_cap = itemsNew * BitsPerItem;
			}

			void resize(uint32_t bitsWanted) {
				// Make sure at least one bit is requested
				GAIA_ASSERT(bitsWanted > 0);
				if (bitsWanted < 1)
					bitsWanted = 1;

				const uint32_t itemsOld = items();

				// Nothing to do if the capacity is already bigger than requested
				if (bitsWanted <= capacity()) {
					m_cnt = bitsWanted;
					return;
				}

				const uint32_t itemsNew = (bitsWanted + BitsPerItem - 1) / BitsPerItem;
				auto* pDataOld = m_pData;
				m_pData = new size_type[itemsNew];
				if (pDataOld == nullptr) {
					// Make sure the new data is set to zeros
					for (uint32_t i = 0; i < itemsNew; ++i)
						m_pData[i] = 0;
				} else {
					// Copy the old data over and set the old data to zeros
					mem::copy_elements<size_type>((uint8_t*)m_pData, (const uint8_t*)pDataOld, 0, itemsOld, 0, 0);
					for (uint32_t i = itemsOld; i < itemsNew; ++i)
						m_pData[i] = 0;

					// Release old data
					delete[] pDataOld;
				}

				m_cnt = bitsWanted;
				m_cap = itemsNew * BitsPerItem;
			}

			const_iterator begin() const {
				return const_iterator(*this, 0, true);
			}

			const_iterator end() const {
				return const_iterator(*this, size(), false);
			}

			const_iterator_inverse begin_inverse() const {
				return const_iterator_inverse(*this, 0, true);
			}

			const_iterator_inverse end_inverse() const {
				return const_iterator_inverse(*this, size(), false);
			}

			GAIA_NODISCARD bool operator[](uint32_t pos) const {
				return test(pos);
			}

			GAIA_NODISCARD bool operator==(const dbitset& other) const {
				const uint32_t item_count = items();
				for (uint32_t i = 0; i < item_count; ++i)
					if (m_pData[i] != other.m_pData[i])
						return false;
				return true;
			}

			GAIA_NODISCARD bool operator!=(const dbitset& other) const {
				const uint32_t item_count = items();
				for (uint32_t i = 0; i < item_count; ++i)
					if (m_pData[i] == other.m_pData[i])
						return false;
				return true;
			}

			//! Sets all bits
			void set() {
				if GAIA_UNLIKELY (size() == 0)
					return;

				const auto item_count = items();
				const auto lastItemMask = last_item_mask();

				if (lastItemMask != 0) {
					uint32_t i = 0;
					for (; i < item_count - 1; ++i)
						m_pData[i] = (size_type)-1;
					m_pData[i] = lastItemMask;
				} else {
					for (uint32_t i = 0; i < item_count; ++i)
						m_pData[i] = (size_type)-1;
				}
			}

			//! Sets the bit at the postion \param pos to value \param value
			void set(uint32_t pos, bool value = true) {
				try_grow(pos + 1);

				if (value)
					m_pData[pos / BitsPerItem] |= ((size_type)1 << (pos % BitsPerItem));
				else
					m_pData[pos / BitsPerItem] &= ~((size_type)1 << (pos % BitsPerItem));
			}

			//! Flips all bits
			void flip() {
				if GAIA_UNLIKELY (size() == 0)
					return;

				const auto item_count = items();
				const auto lastItemMask = last_item_mask();

				if (lastItemMask != 0) {
					uint32_t i = 0;
					for (; i < item_count - 1; ++i)
						m_pData[i] = ~m_pData[i];
					m_pData[i] = (~m_pData[i]) & lastItemMask;
				} else {
					for (uint32_t i = 0; i <= item_count; ++i)
						m_pData[i] = ~m_pData[i];
				}
			}

			//! Flips the bit at the postion \param pos
			void flip(uint32_t pos) {
				GAIA_ASSERT(pos < size());
				m_pData[pos / BitsPerItem] ^= ((size_type)1 << (pos % BitsPerItem));
			}

			//! Flips all bits from \param bitFrom to \param bitTo (including)
			dbitset& flip(uint32_t bitFrom, uint32_t bitTo) {
				GAIA_ASSERT(bitFrom <= bitTo);
				GAIA_ASSERT(bitTo < size());

				if GAIA_UNLIKELY (size() == 0)
					return *this;

				const uint32_t wordIdxFrom = bitFrom / BitsPerItem;
				const uint32_t wordIdxTo = bitTo / BitsPerItem;

				auto getMask = [](uint32_t from, uint32_t to) -> size_type {
					const auto diff = to - from;
					// Set all bits when asking for the full range
					if (diff == BitsPerItem - 1)
						return (size_type)-1;

					return ((size_type(1) << (diff + 1)) - 1) << from;
				};

				if (wordIdxFrom == wordIdxTo) {
					m_pData[wordIdxTo] ^= getMask(bitFrom % BitsPerItem, bitTo % BitsPerItem);
				} else {
					// First word
					m_pData[wordIdxFrom] ^= getMask(bitFrom % BitsPerItem, BitsPerItem - 1);
					// Middle
					for (uint32_t i = wordIdxFrom + 1; i <= wordIdxTo - 1; ++i)
						m_pData[i] = ~m_pData[i];
					// Last word
					m_pData[wordIdxTo] ^= getMask(0, bitTo % BitsPerItem);
				}

				return *this;
			}

			//! Unsets all bits
			void reset() {
				const auto item_count = items();
				for (uint32_t i = 0; i < item_count; ++i)
					m_pData[i] = 0;
			}

			//! Unsets the bit at the postion \param pos
			void reset(uint32_t pos) {
				GAIA_ASSERT(pos < size());
				m_pData[pos / BitsPerItem] &= ~((size_type)1 << (pos % BitsPerItem));
			}

			//! Returns the value of the bit at the position \param pos
			GAIA_NODISCARD bool test(uint32_t pos) const {
				GAIA_ASSERT(pos < size());
				return (m_pData[pos / BitsPerItem] & ((size_type)1 << (pos % BitsPerItem))) != 0;
			}

			//! Checks if all bits are set
			GAIA_NODISCARD bool all() const {
				const auto item_count = items() - 1;
				const auto lastItemMask = last_item_mask();

				for (uint32_t i = 0; i < item_count; ++i)
					if (m_pData[i] != (size_type)-1)
						return false;

				if (has_trailing_bits())
					return (m_pData[item_count] & lastItemMask) == lastItemMask;

				return m_pData[item_count] == (size_type)-1;
			}

			//! Checks if any bit is set
			GAIA_NODISCARD bool any() const {
				const auto item_count = items();
				for (uint32_t i = 0; i < item_count; ++i)
					if (m_pData[i] != 0)
						return true;
				return false;
			}

			//! Checks if all bits are reset
			GAIA_NODISCARD bool none() const {
				const auto item_count = items();
				for (uint32_t i = 0; i < item_count; ++i)
					if (m_pData[i] != 0)
						return false;
				return true;
			}

			//! Returns the number of set bits
			GAIA_NODISCARD uint32_t count() const {
				uint32_t total = 0;

				const auto item_count = items();

				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(4244)
				if constexpr (sizeof(size_type) == 4) {
					for (uint32_t i = 0; i < item_count; ++i)
						total += GAIA_POPCNT(m_pData[i]);
				} else {
					for (uint32_t i = 0; i < item_count; ++i)
						total += GAIA_POPCNT64(m_pData[i]);
				}
				GAIA_MSVC_WARNING_POP()

				return total;
			}

			//! Returns the number of bits the dbitset holds
			GAIA_NODISCARD constexpr uint32_t size() const {
				return m_cnt;
			}

			//! Returns the number of bits the dbitset can hold
			GAIA_NODISCARD constexpr uint32_t capacity() const {
				return m_cap;
			}
		};
	} // namespace cnt
} // namespace gaia

#include <cinttypes>
#include <type_traits>

namespace gaia {
	namespace cnt {
		struct ilist_item_base {};

		struct ilist_item: public ilist_item_base {
			//! Allocated items: Index in the list.
			//! Deleted items: Index of the next deleted item in the list.
			uint32_t idx;
			//! Generation ID
			uint32_t gen;

			ilist_item() = default;
			ilist_item(uint32_t index, uint32_t generation): idx(index), gen(generation) {}
		};

		//! Implicit list. Rather than with pointers, items \tparam TListItem are linked
		//! together through an internal indexing mechanism. To the outside world they are
		//! presented as \tparam TItemHandle.
		//! \tparam TListItem needs to have idx and gen variables and expose a constructor
		//! that initializes them.
		template <typename TListItem, typename TItemHandle>
		struct ilist {
			using internal_storage = cnt::darray<TListItem>;
			// TODO: replace this iterator with a real list iterator
			using iterator = typename internal_storage::iterator;

			using iterator_category = typename internal_storage::iterator::iterator_category;
			using value_type = TListItem;
			using reference = TListItem&;
			using const_reference = const TListItem&;
			using pointer = TListItem*;
			using const_pointer = TListItem*;
			using difference_type = typename internal_storage::difference_type;
			using size_type = typename internal_storage::size_type;

			static_assert(std::is_base_of<ilist_item_base, TListItem>::value);
			//! Implicit list items
			internal_storage m_items;
			//! Index of the next item to recycle
			size_type m_nextFreeIdx = (size_type)-1;
			//! Number of items to recycle
			size_type m_freeItems = 0;

			GAIA_NODISCARD pointer data() noexcept {
				return (pointer)m_items.data();
			}

			GAIA_NODISCARD const_pointer data() const noexcept {
				return (const_pointer)m_items.data();
			}

			GAIA_NODISCARD reference operator[](size_type index) {
				return m_items[index];
			}
			GAIA_NODISCARD const_reference operator[](size_type index) const {
				return m_items[index];
			}

			void clear() {
				m_items.clear();
				m_nextFreeIdx = (size_type)-1;
				m_freeItems = 0;
			}

			GAIA_NODISCARD size_type get_next_free_item() const noexcept {
				return m_nextFreeIdx;
			}

			GAIA_NODISCARD size_type get_free_items() const noexcept {
				return m_freeItems;
			}

			GAIA_NODISCARD size_type item_count() const noexcept {
				return size() - m_freeItems;
			}

			GAIA_NODISCARD size_type size() const noexcept {
				return (size_type)m_items.size();
			}

			GAIA_NODISCARD size_type capacity() const noexcept {
				return (size_type)m_items.capacity();
			}

			GAIA_NODISCARD bool empty() const noexcept {
				return size() == 0;
			}

			GAIA_NODISCARD iterator begin() const noexcept {
				return {(pointer)m_items.data()};
			}

			GAIA_NODISCARD iterator end() const noexcept {
				return {(pointer)m_items.data() + size()};
			}

			//! Allocates a new item in the list
			//! \return Handle to the new item
			GAIA_NODISCARD TItemHandle alloc() {
				if GAIA_UNLIKELY (m_freeItems == 0U) {
					// We don't want to go out of range for new item
					const auto itemCnt = (size_type)m_items.size();
					GAIA_ASSERT(itemCnt < TItemHandle::IdMask && "Trying to allocate too many items!");

					GAIA_GCC_WARNING_PUSH()
					GAIA_CLANG_WARNING_PUSH()
					GAIA_GCC_WARNING_DISABLE("-Wmissing-field-initializers");
					GAIA_CLANG_WARNING_DISABLE("-Wmissing-field-initializers");
					m_items.push_back(TListItem(itemCnt, 0U));
					return {itemCnt, 0U};
					GAIA_GCC_WARNING_POP()
					GAIA_CLANG_WARNING_POP()
				}

				// Make sure the list is not broken
				GAIA_ASSERT(m_nextFreeIdx < (size_type)m_items.size() && "Item recycle list broken!");

				--m_freeItems;
				const auto index = m_nextFreeIdx;
				auto& j = m_items[m_nextFreeIdx];
				m_nextFreeIdx = j.idx;
				return {index, m_items[index].gen};
			}

			//! Invalidates \param handle.
			//! Everytime an item is deallocated its generation is increased by one.
			TListItem& free(TItemHandle handle) {
				auto& item = m_items[handle.id()];

				// New generation
				const auto gen = ++item.gen;

				// Update our implicit list
				if GAIA_UNLIKELY (m_freeItems == 0) {
					m_nextFreeIdx = handle.id();
					item.idx = TItemHandle::IdMask;
					item.gen = gen;
				} else {
					item.idx = m_nextFreeIdx;
					item.gen = gen;
					m_nextFreeIdx = handle.id();
				}
				++m_freeItems;

				return item;
			}

			//! Verifies that the implicit linked list is valid
			void validate() const {
				bool hasThingsToRemove = m_freeItems > 0;
				if (!hasThingsToRemove)
					return;

				// If there's something to remove there has to be at least one entity left
				GAIA_ASSERT(!m_items.empty());

				auto freeItems = m_freeItems;
				auto nextFreeItem = m_nextFreeIdx;
				while (freeItems > 0) {
					GAIA_ASSERT(nextFreeItem < m_items.size() && "Item recycle list broken!");

					nextFreeItem = m_items[nextFreeItem].idx;
					--freeItems;
				}

				// At this point the index of the last item in list should
				// point to -1 because that's the tail of our implicit list.
				GAIA_ASSERT(nextFreeItem == TItemHandle::IdMask);
			}
		};
	} // namespace cnt
} // namespace gaia

//                 ______  _____                 ______                _________
//  ______________ ___  /_ ___(_)_______         ___  /_ ______ ______ ______  /
//  __  ___/_  __ \__  __ \__  / __  __ \        __  __ \_  __ \_  __ \_  __  /
//  _  /    / /_/ /_  /_/ /_  /  _  / / /        _  / / // /_/ // /_/ // /_/ /
//  /_/     \____/ /_.___/ /_/   /_/ /_/ ________/_/ /_/ \____/ \____/ \__,_/
//                                      _/_____/
//
// Fast & memory efficient hashtable based on robin hood hashing for C++11/14/17/20
// https://github.com/martinus/robin-hood-hashing
//
// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2018-2021 Martin Ankerl <http://martin.ankerl.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef ROBIN_HOOD_H_INCLUDED
#define ROBIN_HOOD_H_INCLUDED

// see https://semver.org/
#define ROBIN_HOOD_VERSION_MAJOR 3 // for incompatible API changes
#define ROBIN_HOOD_VERSION_MINOR 11 // for adding functionality in a backwards-compatible manner
#define ROBIN_HOOD_VERSION_PATCH 5 // for backwards-compatible bug fixes

#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <new>
#include <tuple>
#include <type_traits>
#include <utility>

// #define ROBIN_HOOD_STD_SMARTPOINTERS
#if defined(ROBIN_HOOD_STD_SMARTPOINTERS)
	#include <memory>
#endif

// #define ROBIN_HOOD_LOG_ENABLED
#ifdef ROBIN_HOOD_LOG_ENABLED
	#include <iostream>
	#define ROBIN_HOOD_LOG(...) std::cout << __FUNCTION__ << "@" << __LINE__ << ": " << __VA_ARGS__ << std::endl;
#else
	#define ROBIN_HOOD_LOG(x)
#endif

// #define ROBIN_HOOD_TRACE_ENABLED
#ifdef ROBIN_HOOD_TRACE_ENABLED
	#include <iostream>
	#define ROBIN_HOOD_TRACE(...) std::cout << __FUNCTION__ << "@" << __LINE__ << ": " << __VA_ARGS__ << std::endl;
#else
	#define ROBIN_HOOD_TRACE(x)
#endif

// #define ROBIN_HOOD_COUNT_ENABLED
#ifdef ROBIN_HOOD_COUNT_ENABLED
	#include <iostream>
	#define ROBIN_HOOD_COUNT(x) ++counts().x;
namespace robin_hood {
	struct Counts {
		uint64_t shiftUp{};
		uint64_t shiftDown{};
	};
	inline std::ostream& operator<<(std::ostream& os, Counts const& c) {
		return os << c.shiftUp << " shiftUp" << std::endl << c.shiftDown << " shiftDown" << std::endl;
	}

	static Counts& counts() {
		static Counts counts{};
		return counts;
	}
} // namespace robin_hood
#else
	#define ROBIN_HOOD_COUNT(x)
#endif

// all non-argument macros should use this facility. See
// https://www.fluentcpp.com/2019/05/28/better-macros-better-flags/
#define ROBIN_HOOD(x) ROBIN_HOOD_PRIVATE_DEFINITION_##x()

// mark unused members with this macro
#define ROBIN_HOOD_UNUSED(identifier)

// bitness
#if SIZE_MAX == UINT32_MAX
	#define ROBIN_HOOD_PRIVATE_DEFINITION_BITNESS() 32
#elif SIZE_MAX == UINT64_MAX
	#define ROBIN_HOOD_PRIVATE_DEFINITION_BITNESS() 64
#else
	#error Unsupported bitness
#endif

// exceptions
#if !defined(__cpp_exceptions) && !defined(__EXCEPTIONS) && !defined(_CPPUNWIND)
	#define ROBIN_HOOD_PRIVATE_DEFINITION_HAS_EXCEPTIONS() 0
	#define ROBIN_HOOD_STD_OUT_OF_RANGE void
#else
	#include <stdexcept>
	#define ROBIN_HOOD_PRIVATE_DEFINITION_HAS_EXCEPTIONS() 1
	#define ROBIN_HOOD_STD_OUT_OF_RANGE std::out_of_range
#endif

// count leading/trailing bits
#if !defined(ROBIN_HOOD_DISABLE_INTRINSICS)
	#if ROBIN_HOOD_PRIVATE_DEFINITION_BITNESS() == 32
		#define ROBIN_HOOD_COUNT_TRAILING_ZEROES(x) GAIA_CLZ(x)
		#define ROBIN_HOOD_COUNT_LEADING_ZEROES(x) GAIA_CTZ(x)
	#else
		#define ROBIN_HOOD_COUNT_TRAILING_ZEROES(x) GAIA_CLZ64(x)
		#define ROBIN_HOOD_COUNT_LEADING_ZEROES(x) GAIA_CTZ64(x)
	#endif
#endif

// fallthrough
#ifndef __has_cpp_attribute // For backwards compatibility
	#define __has_cpp_attribute(x) 0
#endif
#if __has_cpp_attribute(fallthrough)
	#define ROBIN_HOOD_PRIVATE_DEFINITION_FALLTHROUGH() [[fallthrough]]
#else
	#define ROBIN_HOOD_PRIVATE_DEFINITION_FALLTHROUGH()
#endif

// detect if native wchar_t type is availiable in MSVC
#ifdef _MSC_VER
	#ifdef _NATIVE_WCHAR_T_DEFINED
		#define ROBIN_HOOD_PRIVATE_DEFINITION_HAS_NATIVE_WCHART() 1
	#else
		#define ROBIN_HOOD_PRIVATE_DEFINITION_HAS_NATIVE_WCHART() 0
	#endif
#else
	#define ROBIN_HOOD_PRIVATE_DEFINITION_HAS_NATIVE_WCHART() 1
#endif

// detect if MSVC supports the pair(std::piecewise_construct_t,...) constructor being constexpr
#ifdef _MSC_VER
	#if _MSC_VER <= 1900
		#define ROBIN_HOOD_PRIVATE_DEFINITION_BROKEN_CONSTEXPR() 1
	#else
		#define ROBIN_HOOD_PRIVATE_DEFINITION_BROKEN_CONSTEXPR() 0
	#endif
#else
	#define ROBIN_HOOD_PRIVATE_DEFINITION_BROKEN_CONSTEXPR() 0
#endif

// workaround missing "is_trivially_copyable" in g++ < 5.0
// See https://stackoverflow.com/a/31798726/48181
#if GAIA_COMPILER_GCC && __GNUC__ < 5
	#define ROBIN_HOOD_IS_TRIVIALLY_COPYABLE(...) __has_trivial_copy(__VA_ARGS__)
#else
	#define ROBIN_HOOD_IS_TRIVIALLY_COPYABLE(...) std::is_trivially_copyable<__VA_ARGS__>::value
#endif

namespace robin_hood {

	namespace detail {

// make sure we static_cast to the correct type for hash_int
#if ROBIN_HOOD(BITNESS) == 64
		using SizeT = uint64_t;
#else
		using SizeT = uint32_t;
#endif

		template <typename T>
		T rotr(T x, unsigned k) {
			return (x >> k) | (x << (8U * sizeof(T) - k));
		}

		// This cast gets rid of warnings like "cast from 'uint8_t*' {aka 'unsigned char*'} to
		// 'uint64_t*' {aka 'long unsigned int*'} increases required alignment of target type". Use with
		// care!
		template <typename T>
		inline T reinterpret_cast_no_cast_align_warning(void* ptr) noexcept {
			return reinterpret_cast<T>(ptr);
		}

		template <typename T>
		inline T reinterpret_cast_no_cast_align_warning(void const* ptr) noexcept {
			return reinterpret_cast<T>(ptr);
		}

		template <typename, typename = void>
		struct has_hash: public std::false_type {};

		template <typename T>
		struct has_hash<T, std::void_t<decltype(T::hash)>>: public std::true_type {};

		// make sure this is not inlined as it is slow and dramatically enlarges code, thus making other
		// inlinings more difficult. Throws are also generally the slow path.
		template <typename E, typename... Args>
		[[noreturn]] GAIA_NOINLINE
#if ROBIN_HOOD(HAS_EXCEPTIONS)
				void
				doThrow(Args&&... args) {
			// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
			throw E(std::forward<Args>(args)...);
		}
#else
				void
				doThrow(Args&&... ROBIN_HOOD_UNUSED(args) /*unused*/) {
			abort();
		}
#endif

		template <typename E, typename T, typename... Args>
		T* assertNotNull(T* t, Args&&... args) {
			if GAIA_UNLIKELY (nullptr == t) {
				doThrow<E>(std::forward<Args>(args)...);
			}
			return t;
		}

		template <typename T>
		inline T unaligned_load(void const* ptr) noexcept {
			// using memcpy so we don't get into unaligned load problems.
			// compiler should optimize this very well anyways.
			T t;
			std::memcpy(&t, ptr, sizeof(T));
			return t;
		}

		// Allocates bulks of memory for objects of type T. This deallocates the memory in the destructor,
		// and keeps a linked list of the allocated memory around. Overhead per allocation is the size of a
		// pointer.
		template <typename T, size_t MinNumAllocs = 4, size_t MaxNumAllocs = 256>
		class BulkPoolAllocator {
		public:
			BulkPoolAllocator() noexcept = default;

			// does not copy anything, just creates a new allocator.
			BulkPoolAllocator(const BulkPoolAllocator& ROBIN_HOOD_UNUSED(o) /*unused*/) noexcept:
					mHead(nullptr), mListForFree(nullptr) {}

			BulkPoolAllocator(BulkPoolAllocator&& o) noexcept: mHead(o.mHead), mListForFree(o.mListForFree) {
				o.mListForFree = nullptr;
				o.mHead = nullptr;
			}

			BulkPoolAllocator& operator=(BulkPoolAllocator&& o) noexcept {
				reset();
				mHead = o.mHead;
				mListForFree = o.mListForFree;
				o.mListForFree = nullptr;
				o.mHead = nullptr;
				return *this;
			}

			BulkPoolAllocator&
			// NOLINTNEXTLINE(bugprone-unhandled-self-assignment,cert-oop54-cpp)
			operator=(const BulkPoolAllocator & ROBIN_HOOD_UNUSED(o) /*unused*/) noexcept {
				// does not do anything
				return *this;
			}

			~BulkPoolAllocator() noexcept {
				reset();
			}

			// Deallocates all allocated memory.
			void reset() noexcept {
				while (mListForFree) {
					T* tmp = *mListForFree;
					ROBIN_HOOD_LOG("std::free")
					gaia::mem::mem_free(mListForFree);
					mListForFree = reinterpret_cast_no_cast_align_warning<T**>(tmp);
				}
				mHead = nullptr;
			}

			// allocates, but does NOT initialize. Use in-place new constructor, e.g.
			//   T* obj = pool.allocate();
			//   ::new (static_cast<void*>(obj)) T();
			T* allocate() {
				T* tmp = mHead;
				if (!tmp) {
					tmp = performAllocation();
				}

				mHead = *reinterpret_cast_no_cast_align_warning<T**>(tmp);
				return tmp;
			}

			// does not actually deallocate but puts it in store.
			// make sure you have already called the destructor! e.g. with
			//  obj->~T();
			//  pool.deallocate(obj);
			void deallocate(T* obj) noexcept {
				*reinterpret_cast_no_cast_align_warning<T**>(obj) = mHead;
				mHead = obj;
			}

			// Adds an already allocated block of memory to the allocator. This allocator is from now on
			// responsible for freeing the data (with mem_free()). If the provided data is not large enough to
			// make use of, it is immediately freed. Otherwise it is reused and freed in the destructor.
			void addOrFree(void* ptr, const size_t numBytes) noexcept {
				// calculate number of available elements in ptr
				if (numBytes < ALIGNMENT + ALIGNED_SIZE) {
					// not enough data for at least one element. Free and return.
					ROBIN_HOOD_LOG("std::free")
					gaia::mem::mem_free(ptr);
				} else {
					ROBIN_HOOD_LOG("add to buffer")
					add(ptr, numBytes);
				}
			}

			void swap(BulkPoolAllocator<T, MinNumAllocs, MaxNumAllocs>& other) noexcept {
				using std::swap;
				swap(mHead, other.mHead);
				swap(mListForFree, other.mListForFree);
			}

		private:
			// iterates the list of allocated memory to calculate how many to alloc next.
			// Recalculating this each time saves us a size_t member.
			// This ignores the fact that memory blocks might have been added manually with addOrFree. In
			// practice, this should not matter much.
			GAIA_NODISCARD size_t calcNumElementsToAlloc() const noexcept {
				auto tmp = mListForFree;
				size_t numAllocs = MinNumAllocs;

				while (numAllocs * 2 <= MaxNumAllocs && tmp) {
					auto x = reinterpret_cast<T***>(tmp);
					tmp = *x;
					numAllocs *= 2;
				}

				return numAllocs;
			}

			// WARNING: Underflow if numBytes < ALIGNMENT! This is guarded in addOrFree().
			void add(void* ptr, const size_t numBytes) noexcept {
				const size_t numElements = (numBytes - ALIGNMENT) / ALIGNED_SIZE;

				auto data = reinterpret_cast<T**>(ptr);

				// link free list
				auto x = reinterpret_cast<T***>(data);
				*x = mListForFree;
				mListForFree = data;

				// create linked list for newly allocated data
				auto* const headT = reinterpret_cast_no_cast_align_warning<T*>(reinterpret_cast<char*>(ptr) + ALIGNMENT);

				auto* const head = reinterpret_cast<char*>(headT);

				// Visual Studio compiler automatically unrolls this loop, which is pretty cool
				for (size_t i = 0; i < numElements; ++i) {
					*reinterpret_cast_no_cast_align_warning<char**>(head + i * ALIGNED_SIZE) = head + (i + 1) * ALIGNED_SIZE;
				}

				// last one points to 0
				*reinterpret_cast_no_cast_align_warning<T**>(head + (numElements - 1) * ALIGNED_SIZE) = mHead;
				mHead = headT;
			}

			// Called when no memory is available (mHead == 0).
			// Don't inline this slow path.
			GAIA_NOINLINE T* performAllocation() {
				size_t const numElementsToAlloc = calcNumElementsToAlloc();

				// alloc new memory: [prev |T, T, ... T]
				size_t const bytes = ALIGNMENT + ALIGNED_SIZE * numElementsToAlloc;
				ROBIN_HOOD_LOG(
						"gaia::mem::mem_alloc " << bytes << " = " << ALIGNMENT << " + " << ALIGNED_SIZE << " * "
																		<< numElementsToAlloc)
				add(assertNotNull<std::bad_alloc>(gaia::mem::mem_alloc(bytes)), bytes);
				return mHead;
			}

			// enforce byte alignment of the T's
			static constexpr size_t ALIGNMENT =
					gaia::core::get_max(std::alignment_of<T>::value, std::alignment_of<T*>::value);

			static constexpr size_t ALIGNED_SIZE = ((sizeof(T) - 1) / ALIGNMENT + 1) * ALIGNMENT;

			static_assert(MinNumAllocs >= 1, "MinNumAllocs");
			static_assert(MaxNumAllocs >= MinNumAllocs, "MaxNumAllocs");
			static_assert(ALIGNED_SIZE >= sizeof(T*), "ALIGNED_SIZE");
			static_assert(0 == (ALIGNED_SIZE % sizeof(T*)), "ALIGNED_SIZE mod");
			static_assert(ALIGNMENT >= sizeof(T*), "ALIGNMENT");

			T* mHead{nullptr};
			T** mListForFree{nullptr};
		};

		template <typename T, size_t MinSize, size_t MaxSize, bool IsFlat>
		struct NodeAllocator;

		// dummy allocator that does nothing
		template <typename T, size_t MinSize, size_t MaxSize>
		struct NodeAllocator<T, MinSize, MaxSize, true> {

			// we are not using the data, so just free it.
			void addOrFree(void* ptr, size_t ROBIN_HOOD_UNUSED(numBytes) /*unused*/) noexcept {
				ROBIN_HOOD_LOG("std::free")
				gaia::mem::mem_free(ptr);
			}
		};

		template <typename T, size_t MinSize, size_t MaxSize>
		struct NodeAllocator<T, MinSize, MaxSize, false>: public BulkPoolAllocator<T, MinSize, MaxSize> {};

		namespace swappable {
			template <typename T>
			struct nothrow {
				static const bool value = std::is_nothrow_swappable<T>::value;
			};
		} // namespace swappable

	} // namespace detail

	struct is_transparent_tag {};

	// A custom pair implementation is used in the map because std::pair is not is_trivially_copyable,
	// which means it would  not be allowed to be used in std::memcpy. This struct is copyable, which is
	// also tested.
	template <typename T1, typename T2>
	struct pair {
		using first_type = T1;
		using second_type = T2;

		template <
				typename U1 = T1, typename U2 = T2,
				typename = typename std::enable_if<
						std::is_default_constructible<U1>::value && std::is_default_constructible<U2>::value>::type>
		constexpr pair() noexcept(noexcept(U1()) && noexcept(U2())): first(), second() {}

		// pair constructors are explicit so we don't accidentally call this ctor when we don't have to.
		explicit constexpr pair(std::pair<T1, T2> const& o) noexcept(
				noexcept(T1(std::declval<T1 const&>())) && noexcept(T2(std::declval<T2 const&>()))):
				first(o.first),
				second(o.second) {}

		// pair constructors are explicit so we don't accidentally call this ctor when we don't have to.
		explicit constexpr pair(std::pair<T1, T2>&& o) noexcept(
				noexcept(T1(std::move(std::declval<T1&&>()))) && noexcept(T2(std::move(std::declval<T2&&>())))):
				first(std::move(o.first)),
				second(std::move(o.second)) {}

		constexpr pair(T1&& a, T2&& b) noexcept(
				noexcept(T1(std::move(std::declval<T1&&>()))) && noexcept(T2(std::move(std::declval<T2&&>())))):
				first(std::move(a)),
				second(std::move(b)) {}

		template <typename U1, typename U2>
		constexpr pair(U1&& a, U2&& b) noexcept(
				noexcept(T1(std::forward<U1>(std::declval<U1&&>()))) && noexcept(T2(std::forward<U2>(std::declval<U2&&>())))):
				first(std::forward<U1>(a)),
				second(std::forward<U2>(b)) {}

		template <typename... U1, typename... U2>
		// MSVC 2015 produces error "C2476: ‘constexpr’ constructor does not initialize all members"
		// if this constructor is constexpr
#if !ROBIN_HOOD(BROKEN_CONSTEXPR)
		constexpr
#endif
				pair(std::piecewise_construct_t /*unused*/, std::tuple<U1...> a, std::tuple<U2...> b) noexcept(noexcept(pair(
						std::declval<std::tuple<U1...>&>(), std::declval<std::tuple<U2...>&>(), std::index_sequence_for<U1...>(),
						std::index_sequence_for<U2...>()))):
				pair(a, b, std::index_sequence_for<U1...>(), std::index_sequence_for<U2...>()) {
		}

		// constructor called from the std::piecewise_construct_t ctor
		template <typename... U1, size_t... I1, typename... U2, size_t... I2>
		pair(
				std::tuple<U1...>& a, std::tuple<U2...>& b, std::index_sequence<I1...> /*unused*/,
				std::index_sequence<
						I2...> /*unused*/) noexcept(noexcept(T1(std::
																												forward<U1>(std::get<I1>(
																														std::declval<std::tuple<
																																U1...>&>()))...)) && noexcept(T2(std::
																																																		 forward<
																																																				 U2>(std::get<
																																																						 I2>(
																																																				 std::declval<std::tuple<
																																																						 U2...>&>()))...))):
				first(std::forward<U1>(std::get<I1>(a))...),
				second(std::forward<U2>(std::get<I2>(b))...) {
			// make visual studio compiler happy about warning about unused a & b.
			// Visual studio's pair implementation disables warning 4100.
			(void)a;
			(void)b;
		}

		void
		swap(pair<T1, T2>& o) noexcept((detail::swappable::nothrow<T1>::value) && (detail::swappable::nothrow<T2>::value)) {
			using std::swap;
			swap(first, o.first);
			swap(second, o.second);
		}

		T1 first; // NOLINT(misc-non-private-member-variables-in-classes)
		T2 second; // NOLINT(misc-non-private-member-variables-in-classes)
	};

	template <typename A, typename B>
	inline void
	swap(pair<A, B>& a, pair<A, B>& b) noexcept(noexcept(std::declval<pair<A, B>&>().swap(std::declval<pair<A, B>&>()))) {
		a.swap(b);
	}

	template <typename A, typename B>
	inline constexpr bool operator==(pair<A, B> const& x, pair<A, B> const& y) {
		return (x.first == y.first) && (x.second == y.second);
	}
	template <typename A, typename B>
	inline constexpr bool operator!=(pair<A, B> const& x, pair<A, B> const& y) {
		return !(x == y);
	}
	template <typename A, typename B>
	inline constexpr bool operator<(pair<A, B> const& x, pair<A, B> const& y) noexcept(noexcept(
			std::declval<A const&>() <
			std::declval<A const&>()) && noexcept(std::declval<B const&>() < std::declval<B const&>())) {
		return x.first < y.first || (!(y.first < x.first) && x.second < y.second);
	}
	template <typename A, typename B>
	inline constexpr bool operator>(pair<A, B> const& x, pair<A, B> const& y) {
		return y < x;
	}
	template <typename A, typename B>
	inline constexpr bool operator<=(pair<A, B> const& x, pair<A, B> const& y) {
		return !(x > y);
	}
	template <typename A, typename B>
	inline constexpr bool operator>=(pair<A, B> const& x, pair<A, B> const& y) {
		return !(x < y);
	}

	inline size_t hash_bytes(void const* ptr, size_t len) noexcept {
		static constexpr uint64_t m = UINT64_C(0xc6a4a7935bd1e995);
		static constexpr uint64_t seed = UINT64_C(0xe17a1465);
		static constexpr unsigned int r = 47;

		auto const* const data64 = static_cast<uint64_t const*>(ptr);
		uint64_t h = seed ^ (len * m);

		size_t const n_blocks = len / 8;
		for (size_t i = 0; i < n_blocks; ++i) {
			auto k = detail::unaligned_load<uint64_t>(data64 + i);

			k *= m;
			k ^= k >> r;
			k *= m;

			h ^= k;
			h *= m;
		}

		auto const* const data8 = reinterpret_cast<uint8_t const*>(data64 + n_blocks);
		switch (len & 7U) {
			case 7:
				h ^= static_cast<uint64_t>(data8[6]) << 48U;
				ROBIN_HOOD(FALLTHROUGH); // FALLTHROUGH
			case 6:
				h ^= static_cast<uint64_t>(data8[5]) << 40U;
				ROBIN_HOOD(FALLTHROUGH); // FALLTHROUGH
			case 5:
				h ^= static_cast<uint64_t>(data8[4]) << 32U;
				ROBIN_HOOD(FALLTHROUGH); // FALLTHROUGH
			case 4:
				h ^= static_cast<uint64_t>(data8[3]) << 24U;
				ROBIN_HOOD(FALLTHROUGH); // FALLTHROUGH
			case 3:
				h ^= static_cast<uint64_t>(data8[2]) << 16U;
				ROBIN_HOOD(FALLTHROUGH); // FALLTHROUGH
			case 2:
				h ^= static_cast<uint64_t>(data8[1]) << 8U;
				ROBIN_HOOD(FALLTHROUGH); // FALLTHROUGH
			case 1:
				h ^= static_cast<uint64_t>(data8[0]);
				h *= m;
				ROBIN_HOOD(FALLTHROUGH); // FALLTHROUGH
			default:
				break;
		}

		h ^= h >> r;

		// not doing the final step here, because this will be done by keyToIdx anyways
		// h *= m;
		// h ^= h >> r;
		return static_cast<size_t>(h);
	}

	inline size_t hash_int(uint64_t x) noexcept {
		// tried lots of different hashes, let's stick with murmurhash3. It's simple, fast, well tested,
		// and doesn't need any special 128bit operations.
		x ^= x >> 33U;
		x *= UINT64_C(0xff51afd7ed558ccd);
		x ^= x >> 33U;

		// not doing the final step here, because this will be done by keyToIdx anyways
		// x *= UINT64_C(0xc4ceb9fe1a85ec53);
		// x ^= x >> 33U;
		return static_cast<size_t>(x);
	}

	template <typename T, typename Enable = void>
	struct hash {
		size_t operator()(T const& obj) const noexcept {
			return hash_bytes(&obj, sizeof(T));
		}
	};

	template <typename T>
	struct hash<T*> {
		size_t operator()(T* ptr) const noexcept {
			return hash_int(reinterpret_cast<detail::SizeT>(ptr));
		}
	};

#ifdef ROBIN_HOOD_STD_SMARTPOINTERS
	template <typename T>
	struct hash<std::unique_ptr<T>> {
		size_t operator()(std::unique_ptr<T> const& ptr) const noexcept {
			return hash_int(reinterpret_cast<detail::SizeT>(ptr.get()));
		}
	};

	template <typename T>
	struct hash<std::shared_ptr<T>> {
		size_t operator()(std::shared_ptr<T> const& ptr) const noexcept {
			return hash_int(reinterpret_cast<detail::SizeT>(ptr.get()));
		}
	};
#endif

	template <typename Enum>
	struct hash<Enum, typename std::enable_if<std::is_enum<Enum>::value>::type> {
		size_t operator()(Enum e) const noexcept {
			using Underlying = typename std::underlying_type<Enum>::type;
			return hash<Underlying>{}(static_cast<Underlying>(e));
		}
	};

	template <typename T>
	struct hash<T, typename std::enable_if<gaia::core::is_direct_hash_key_v<T>>::type> {
		size_t operator()(const T& obj) const noexcept {
			if constexpr (detail::has_hash<T>::value)
				return obj.hash;
			else
				return obj.hash();
		}
	};

#define ROBIN_HOOD_HASH_INT(T)                                                                                         \
	template <>                                                                                                          \
	struct hash<T> {                                                                                                     \
		size_t operator()(T const& obj) const noexcept {                                                                   \
			return hash_int(static_cast<uint64_t>(obj));                                                                     \
		}                                                                                                                  \
	}

#if defined(__GNUC__) && !defined(__clang__)
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wuseless-cast"
#endif
	// see https://en.cppreference.com/w/cpp/utility/hash
	ROBIN_HOOD_HASH_INT(bool);
	ROBIN_HOOD_HASH_INT(char);
	ROBIN_HOOD_HASH_INT(signed char);
	ROBIN_HOOD_HASH_INT(unsigned char);
	ROBIN_HOOD_HASH_INT(char16_t);
	ROBIN_HOOD_HASH_INT(char32_t);
#if ROBIN_HOOD(HAS_NATIVE_WCHART)
	ROBIN_HOOD_HASH_INT(wchar_t);
#endif
	ROBIN_HOOD_HASH_INT(short);
	ROBIN_HOOD_HASH_INT(unsigned short);
	ROBIN_HOOD_HASH_INT(int);
	ROBIN_HOOD_HASH_INT(unsigned int);
	ROBIN_HOOD_HASH_INT(long);
	ROBIN_HOOD_HASH_INT(long long);
	ROBIN_HOOD_HASH_INT(unsigned long);
	ROBIN_HOOD_HASH_INT(unsigned long long);
#if defined(__GNUC__) && !defined(__clang__)
	#pragma GCC diagnostic pop
#endif
	namespace detail {
		template <typename, typename = void>
		struct has_is_transparent: public std::false_type {};

		template <typename T>
		struct has_is_transparent<T, std::void_t<decltype(T::is_transparent)>>: public std::true_type {};

		// using wrapper classes for hash and key_equal prevents the diamond problem when the same type
		// is used. see https://stackoverflow.com/a/28771920/48181
		template <typename T>
		struct WrapHash: public T {
			WrapHash() = default;
			explicit WrapHash(T const& o) noexcept(noexcept(T(std::declval<T const&>()))): T(o) {}
		};

		template <typename T>
		struct WrapKeyEqual: public T {
			WrapKeyEqual() = default;
			explicit WrapKeyEqual(T const& o) noexcept(noexcept(T(std::declval<T const&>()))): T(o) {}
		};

		// A highly optimized hashmap implementation, using the Robin Hood algorithm.
		//
		// In most cases, this map should be usable as a drop-in replacement for std::unordered_map, but
		// be about 2x faster in most cases and require much less allocations.
		//
		// This implementation uses the following memory layout:
		//
		// [Node, Node, ... Node | info, info, ... infoSentinel ]
		//
		// * Node: either a DataNode that directly has the std::pair<key, val> as member,
		//   or a DataNode with a pointer to std::pair<key,val>. Which DataNode representation to use
		//   depends on how fast the swap() operation is. Heuristically, this is automatically choosen
		//   based on sizeof(). there are always 2^n Nodes.
		//
		// * info: Each Node in the map has a corresponding info byte, so there are 2^n info bytes.
		//   Each byte is initialized to 0, meaning the corresponding Node is empty. Set to 1 means the
		//   corresponding node contains data. Set to 2 means the corresponding Node is filled, but it
		//   actually belongs to the previous position and was pushed out because that place is already
		//   taken.
		//
		// * infoSentinel: Sentinel byte set to 1, so that iterator's ++ can stop at end() without the
		//   need for a idx variable.
		//
		// According to STL, order of templates has effect on throughput. That's why I've moved the
		// boolean to the front.
		// https://www.reddit.com/r/cpp/comments/ahp6iu/compile_time_binary_size_reductions_and_cs_future/eeguck4/
		template <bool IsFlat, size_t MaxLoadFactor100, typename Key, typename T, typename Hash, typename KeyEqual>
		class Table:
				public WrapHash<Hash>,
				public WrapKeyEqual<KeyEqual>,
				detail::NodeAllocator<
						typename std::conditional<
								std::is_void<T>::value, Key,
								robin_hood::pair<typename std::conditional<IsFlat, Key, Key const>::type, T>>::type,
						4, 16384, IsFlat> {
		public:
			static constexpr bool is_flat = IsFlat;
			static constexpr bool is_map = !std::is_void<T>::value;
			static constexpr bool is_set = !is_map;
			static constexpr bool is_transparent = has_is_transparent<Hash>::value && has_is_transparent<KeyEqual>::value;

			using key_type = Key;
			using mapped_type = T;
			using value_type = typename std::conditional<
					is_set, Key, robin_hood::pair<typename std::conditional<is_flat, Key, Key const>::type, T>>::type;
			using size_type = size_t;
			using hasher = Hash;
			using key_equal = KeyEqual;
			using Self = Table<IsFlat, MaxLoadFactor100, key_type, mapped_type, hasher, key_equal>;

		private:
			static_assert(MaxLoadFactor100 > 10 && MaxLoadFactor100 < 100, "MaxLoadFactor100 needs to be >10 && < 100");

			using WHash = WrapHash<Hash>;
			using WKeyEqual = WrapKeyEqual<KeyEqual>;

			// configuration defaults

			// make sure we have 8 elements, needed to quickly rehash mInfo
			static constexpr size_t InitialNumElements = sizeof(uint64_t);
			static constexpr uint32_t InitialInfoNumBits = 5;
			static constexpr uint8_t InitialInfoInc = 1U << InitialInfoNumBits;
			static constexpr size_t InfoMask = InitialInfoInc - 1U;
			static constexpr uint8_t InitialInfoHashShift = 64U - InitialInfoNumBits;
			using DataPool = detail::NodeAllocator<value_type, 4, 16384, IsFlat>;

			// type needs to be wider than uint8_t.
			using InfoType = uint32_t;

			// DataNode ////////////////////////////////////////////////////////

			// Primary template for the data node. We have special implementations for small and big
			// objects. For large objects it is assumed that swap() is fairly slow, so we allocate these
			// on the heap so swap merely swaps a pointer.
			template <typename M, bool>
			class DataNode {};

			// Small: just allocate on the stack.
			template <typename M>
			class DataNode<M, true> final {
			public:
				template <typename... Args>
				explicit DataNode(M& ROBIN_HOOD_UNUSED(map) /*unused*/, Args&&... args) noexcept(
						noexcept(value_type(std::forward<Args>(args)...))):
						mData(std::forward<Args>(args)...) {}

				DataNode(M& ROBIN_HOOD_UNUSED(map) /*unused*/, DataNode<M, true>&& n) noexcept(
						std::is_nothrow_move_constructible<value_type>::value):
						mData(std::move(n.mData)) {}

				// doesn't do anything
				void destroy(M& ROBIN_HOOD_UNUSED(map) /*unused*/) noexcept {}
				void destroyDoNotDeallocate() noexcept {}

				value_type const* operator->() const noexcept {
					return &mData;
				}
				value_type* operator->() noexcept {
					return &mData;
				}

				const value_type& operator*() const noexcept {
					return mData;
				}

				value_type& operator*() noexcept {
					return mData;
				}

				template <typename VT = value_type>
				GAIA_NODISCARD typename std::enable_if<is_map, typename VT::first_type&>::type getFirst() noexcept {
					return mData.first;
				}
				template <typename VT = value_type>
				GAIA_NODISCARD typename std::enable_if<is_set, VT&>::type getFirst() noexcept {
					return mData;
				}

				template <typename VT = value_type>
				GAIA_NODISCARD typename std::enable_if<is_map, typename VT::first_type const&>::type getFirst() const noexcept {
					return mData.first;
				}
				template <typename VT = value_type>
				GAIA_NODISCARD typename std::enable_if<is_set, VT const&>::type getFirst() const noexcept {
					return mData;
				}

				template <typename MT = mapped_type>
				GAIA_NODISCARD typename std::enable_if<is_map, MT&>::type getSecond() noexcept {
					return mData.second;
				}

				template <typename MT = mapped_type>
				GAIA_NODISCARD typename std::enable_if<is_set, MT const&>::type getSecond() const noexcept {
					return mData.second;
				}

				void
				swap(DataNode<M, true>& o) noexcept(noexcept(std::declval<value_type>().swap(std::declval<value_type>()))) {
					mData.swap(o.mData);
				}

			private:
				value_type mData;
			};

			// big object: allocate on heap.
			template <typename M>
			class DataNode<M, false> {
			public:
				template <typename... Args>
				explicit DataNode(M& map, Args&&... args): mData(map.allocate()) {
					::new (static_cast<void*>(mData)) value_type(std::forward<Args>(args)...);
				}

				DataNode(M& ROBIN_HOOD_UNUSED(map) /*unused*/, DataNode<M, false>&& n) noexcept: mData(std::move(n.mData)) {}

				void destroy(M& map) noexcept {
					// don't deallocate, just put it into list of datapool.
					mData->~value_type();
					map.deallocate(mData);
				}

				void destroyDoNotDeallocate() noexcept {
					mData->~value_type();
				}

				value_type const* operator->() const noexcept {
					return mData;
				}

				value_type* operator->() noexcept {
					return mData;
				}

				const value_type& operator*() const {
					return *mData;
				}

				value_type& operator*() {
					return *mData;
				}

				template <typename VT = value_type>
				GAIA_NODISCARD typename std::enable_if<is_map, typename VT::first_type&>::type getFirst() noexcept {
					return mData->first;
				}
				template <typename VT = value_type>
				GAIA_NODISCARD typename std::enable_if<is_set, VT&>::type getFirst() noexcept {
					return *mData;
				}

				template <typename VT = value_type>
				GAIA_NODISCARD typename std::enable_if<is_map, typename VT::first_type const&>::type getFirst() const noexcept {
					return mData->first;
				}
				template <typename VT = value_type>
				GAIA_NODISCARD typename std::enable_if<is_set, VT const&>::type getFirst() const noexcept {
					return *mData;
				}

				template <typename MT = mapped_type>
				GAIA_NODISCARD typename std::enable_if<is_map, MT&>::type getSecond() noexcept {
					return mData->second;
				}

				template <typename MT = mapped_type>
				GAIA_NODISCARD typename std::enable_if<is_map, MT const&>::type getSecond() const noexcept {
					return mData->second;
				}

				void swap(DataNode<M, false>& o) noexcept {
					using std::swap;
					swap(mData, o.mData);
				}

			private:
				value_type* mData;
			};

			using Node = DataNode<Self, IsFlat>;

			// helpers for insertKeyPrepareEmptySpot: extract first entry (only const required)
			GAIA_NODISCARD key_type const& getFirstConst(Node const& n) const noexcept {
				return n.getFirst();
			}

			// in case we have void mapped_type, we are not using a pair, thus we just route k through.
			// No need to disable this because it's just not used if not applicable.
			GAIA_NODISCARD key_type const& getFirstConst(key_type const& k) const noexcept {
				return k;
			}

			// in case we have non-void mapped_type, we have a standard robin_hood::pair
			template <typename Q = mapped_type>
			GAIA_NODISCARD typename std::enable_if<!std::is_void<Q>::value, key_type const&>::type
			getFirstConst(value_type const& vt) const noexcept {
				return vt.first;
			}

			// Cloner //////////////////////////////////////////////////////////

			template <typename M, bool UseMemcpy>
			struct Cloner;

			// fast path: Just copy data, without allocating anything.
			template <typename M>
			struct Cloner<M, true> {
				void operator()(M const& source, M& target) const {
					auto const* const src = reinterpret_cast<char const*>(source.mKeyVals);
					auto* tgt = reinterpret_cast<char*>(target.mKeyVals);
					auto const numElementsWithBuffer = target.calcNumElementsWithBuffer(target.mMask + 1);
					memcpy(tgt, src, src + target.calcNumBytesTotal(numElementsWithBuffer));
				}
			};

			template <typename M>
			struct Cloner<M, false> {
				void operator()(M const& s, M& t) const {
					auto const numElementsWithBuffer = t.calcNumElementsWithBuffer(t.mMask + 1);
					memcpy(t.mInfo, s.mInfo, s.mInfo + t.calcNumBytesInfo(numElementsWithBuffer));

					for (size_t i = 0; i < numElementsWithBuffer; ++i) {
						if (t.mInfo[i]) {
							::new (static_cast<void*>(t.mKeyVals + i)) Node(t, *s.mKeyVals[i]);
						}
					}
				}
			};

			// Destroyer ///////////////////////////////////////////////////////

			template <typename M, bool IsFlatAndTrivial>
			struct Destroyer {};

			template <typename M>
			struct Destroyer<M, true> {
				void nodes(M& m) const noexcept {
					m.mNumElements = 0;
				}

				void nodesDoNotDeallocate(M& m) const noexcept {
					m.mNumElements = 0;
				}
			};

			template <typename M>
			struct Destroyer<M, false> {
				void nodes(M& m) const noexcept {
					m.mNumElements = 0;
					// clear also resets mInfo to 0, that's sometimes not necessary.
					auto const numElementsWithBuffer = m.calcNumElementsWithBuffer(m.mMask + 1);

					for (size_t idx = 0; idx < numElementsWithBuffer; ++idx) {
						if (0 != m.mInfo[idx]) {
							Node& n = m.mKeyVals[idx];
							n.destroy(m);
							n.~Node();
						}
					}
				}

				void nodesDoNotDeallocate(M& m) const noexcept {
					m.mNumElements = 0;
					// clear also resets mInfo to 0, that's sometimes not necessary.
					auto const numElementsWithBuffer = m.calcNumElementsWithBuffer(m.mMask + 1);
					for (size_t idx = 0; idx < numElementsWithBuffer; ++idx) {
						if (0 != m.mInfo[idx]) {
							Node& n = m.mKeyVals[idx];
							n.destroyDoNotDeallocate();
							n.~Node();
						}
					}
				}
			};

			// Iter ////////////////////////////////////////////////////////////

			struct fast_forward_tag {};

			// generic iterator for both const_iterator and iterator.
			template <bool IsConst>
			// NOLINTNEXTLINE(hicpp-special-member-functions,cppcoreguidelines-special-member-functions)
			class Iter {
			private:
				using NodePtr = typename std::conditional<IsConst, Node const*, Node*>::type;

			public:
				using difference_type = std::ptrdiff_t;
				using value_type = typename Self::value_type;
				using reference = typename std::conditional<IsConst, value_type const&, value_type&>::type;
				using pointer = typename std::conditional<IsConst, value_type const*, value_type*>::type;
				using iterator_category = gaia::core::forward_iterator_tag;

				// default constructed iterator can be compared to itself, but WON'T return true when
				// compared to end().
				Iter() = default;

				// Rule of zero: nothing specified. The conversion constructor is only enabled for
				// iterator to const_iterator, so it doesn't accidentally work as a copy ctor.

				// Conversion constructor from iterator to const_iterator.
				template <bool OtherIsConst, typename = typename std::enable_if<IsConst && !OtherIsConst>::type>
				// NOLINTNEXTLINE(hicpp-explicit-conversions)
				Iter(Iter<OtherIsConst> const& other) noexcept: mKeyVals(other.mKeyVals), mInfo(other.mInfo) {}

				Iter(NodePtr valPtr, uint8_t const* infoPtr) noexcept: mKeyVals(valPtr), mInfo(infoPtr) {}

				Iter(NodePtr valPtr, uint8_t const* infoPtr, fast_forward_tag ROBIN_HOOD_UNUSED(tag) /*unused*/) noexcept:
						mKeyVals(valPtr), mInfo(infoPtr) {
					fastForward();
				}

				template <bool OtherIsConst, typename = typename std::enable_if<IsConst && !OtherIsConst>::type>
				Iter& operator=(Iter<OtherIsConst> const& other) noexcept {
					mKeyVals = other.mKeyVals;
					mInfo = other.mInfo;
					return *this;
				}

				// prefix increment. Undefined behavior if we are at end()!
				Iter& operator++() noexcept {
					++mInfo;
					++mKeyVals;
					fastForward();
					return *this;
				}

				Iter operator++(int) noexcept {
					Iter tmp = *this;
					++(*this);
					return tmp;
				}

				reference operator*() const {
					return **mKeyVals;
				}

				pointer operator->() const {
					return &**mKeyVals;
				}

				template <bool O>
				bool operator==(Iter<O> const& o) const noexcept {
					return mKeyVals == o.mKeyVals;
				}

				template <bool O>
				bool operator!=(Iter<O> const& o) const noexcept {
					return mKeyVals != o.mKeyVals;
				}

			private:
				// fast forward to the next non-free info byte
				// I've tried a few variants that don't depend on intrinsics, but unfortunately they are
				// quite a bit slower than this one. So I've reverted that change again. See map_benchmark.
				void fastForward() noexcept {
					size_t n = 0;
					while (0U == (n = detail::unaligned_load<size_t>(mInfo))) {
						GAIA_MSVC_WARNING_PUSH()
						GAIA_MSVC_WARNING_DISABLE(6305)
						mInfo += sizeof(size_t);
						mKeyVals += sizeof(size_t);
						GAIA_MSVC_WARNING_POP()
					}
#if defined(ROBIN_HOOD_DISABLE_INTRINSICS)
					// we know for certain that within the next 8 bytes we'll find a non-zero one.
					if GAIA_UNLIKELY (0U == detail::unaligned_load<uint32_t>(mInfo)) {
						mInfo += 4;
						mKeyVals += 4;
					}
					if GAIA_UNLIKELY (0U == detail::unaligned_load<uint16_t>(mInfo)) {
						mInfo += 2;
						mKeyVals += 2;
					}
					if GAIA_UNLIKELY (0U == *mInfo) {
						mInfo += 1;
						mKeyVals += 1;
					}
#else
	#if GAIA_LITTLE_ENDIAN
					auto inc = ROBIN_HOOD_COUNT_TRAILING_ZEROES(n) / 8;
	#else
					auto inc = ROBIN_HOOD_COUNT_LEADING_ZEROES(n) / 8;
	#endif
					mInfo += inc;
					mKeyVals += inc;
#endif
				}

				friend class Table<IsFlat, MaxLoadFactor100, key_type, mapped_type, hasher, key_equal>;
				NodePtr mKeyVals{nullptr};
				uint8_t const* mInfo{nullptr};
			};

			////////////////////////////////////////////////////////////////////

			// highly performance relevant code.
			// Lower bits are used for indexing into the array (2^n size)
			// The upper 1-5 bits need to be a reasonable good hash, to save comparisons.
			template <typename HashKey>
			void keyToIdx(HashKey&& key, size_t* idx, InfoType* info) const {
				auto h = static_cast<uint64_t>(WHash::operator()(key));

				// direct_hash_key is expected to be a proper hash. No additional hash tricks are required
				using HashKeyRaw = std::decay_t<HashKey>;
				if constexpr (!gaia::core::is_direct_hash_key_v<HashKeyRaw>) {
					// In addition to whatever hash is used, add another mul & shift so we get better hashing.
					// This serves as a bad hash prevention, if the given data is
					// badly mixed.
					h *= mHashMultiplier;
					h ^= h >> 33U;
				}

				// the lower InitialInfoNumBits are reserved for info.
				*info = mInfoInc + static_cast<InfoType>(h >> mInfoHashShift);
				*idx = (static_cast<size_t>(h)) & mMask;
			}

			// forwards the index by one, wrapping around at the end
			void next(InfoType* info, size_t* idx) const noexcept {
				*idx = *idx + 1;
				*info += mInfoInc;
			}

			void nextWhileLess(InfoType* info, size_t* idx) const noexcept {
				// unrolling this by hand did not bring any speedups.
				while (*info < mInfo[*idx]) {
					next(info, idx);
				}
			}

			// Shift everything up by one element. Tries to move stuff around.
			void shiftUp(size_t startIdx, size_t const insertion_idx) noexcept(std::is_nothrow_move_assignable<Node>::value) {
				auto idx = startIdx;
				::new (static_cast<void*>(mKeyVals + idx)) Node(std::move(mKeyVals[idx - 1]));
				while (--idx != insertion_idx) {
					mKeyVals[idx] = std::move(mKeyVals[idx - 1]);
				}

				idx = startIdx;
				while (idx != insertion_idx) {
					ROBIN_HOOD_COUNT(shiftUp)
					mInfo[idx] = static_cast<uint8_t>(mInfo[idx - 1] + mInfoInc);
					if GAIA_UNLIKELY (mInfo[idx] + mInfoInc > 0xFF) {
						mMaxNumElementsAllowed = 0;
					}
					--idx;
				}
			}

			void shiftDown(size_t idx) noexcept(std::is_nothrow_move_assignable<Node>::value) {
				// until we find one that is either empty or has zero offset.
				// TODO(martinus) we don't need to move everything, just the last one for the same
				// bucket.
				mKeyVals[idx].destroy(*this);

				// until we find one that is either empty or has zero offset.
				while (mInfo[idx + 1] >= 2 * mInfoInc) {
					ROBIN_HOOD_COUNT(shiftDown)
					mInfo[idx] = static_cast<uint8_t>(mInfo[idx + 1] - mInfoInc);
					mKeyVals[idx] = std::move(mKeyVals[idx + 1]);
					++idx;
				}

				mInfo[idx] = 0;
				// don't destroy, we've moved it
				// mKeyVals[idx].destroy(*this);
				mKeyVals[idx].~Node();
			}

			// copy of find(), except that it returns iterator instead of const_iterator.
			template <typename Other>
			GAIA_NODISCARD size_t findIdx(Other const& key) const {
				size_t idx{};
				InfoType info{};
				keyToIdx(key, &idx, &info);

				do {
					// unrolling this twice gives a bit of a speedup. More unrolling did not help.
					if (info == mInfo[idx]) {
						if GAIA_LIKELY (WKeyEqual::operator()(key, mKeyVals[idx].getFirst())) {
							return idx;
						}
					}
					next(&info, &idx);
					if (info == mInfo[idx]) {
						if GAIA_LIKELY (WKeyEqual::operator()(key, mKeyVals[idx].getFirst())) {
							return idx;
						}
					}
					next(&info, &idx);
				} while (info <= mInfo[idx]);

				// nothing found!
				return mMask == 0 ? 0
													: static_cast<size_t>(
																gaia::core::distance(mKeyVals, reinterpret_cast_no_cast_align_warning<Node*>(mInfo)));
			}

			void cloneData(const Table& o) {
				Cloner<Table, IsFlat && ROBIN_HOOD_IS_TRIVIALLY_COPYABLE(Node)>()(o, *this);
			}

			// inserts a keyval that is guaranteed to be new, e.g. when the hashmap is resized.
			// @return True on success, false if something went wrong
			void insert_move(Node&& keyval) {
				// we don't retry, fail if overflowing
				// don't need to check max num elements
				if (0 == mMaxNumElementsAllowed && !try_increase_info()) {
					throwOverflowError();
				}

				size_t idx{};
				InfoType info{};
				keyToIdx(keyval.getFirst(), &idx, &info);

				// skip forward. Use <= because we are certain that the element is not there.
				while (info <= mInfo[idx]) {
					idx = idx + 1;
					info += mInfoInc;
				}

				// key not found, so we are now exactly where we want to insert it.
				auto const insertion_idx = idx;
				auto const insertion_info = static_cast<uint8_t>(info);
				if GAIA_UNLIKELY (insertion_info + mInfoInc > 0xFF) {
					mMaxNumElementsAllowed = 0;
				}

				// find an empty spot
				while (0 != mInfo[idx]) {
					next(&info, &idx);
				}

				auto& l = mKeyVals[insertion_idx];
				if (idx == insertion_idx) {
					::new (static_cast<void*>(&l)) Node(std::move(keyval));
				} else {
					shiftUp(idx, insertion_idx);
					l = std::move(keyval);
				}

				// put at empty spot
				mInfo[insertion_idx] = insertion_info;

				++mNumElements;
			}

		public:
			using iterator = Iter<false>;
			using const_iterator = Iter<true>;

			Table() noexcept(noexcept(Hash()) && noexcept(KeyEqual())): WHash(), WKeyEqual() {
				ROBIN_HOOD_TRACE(this)
				GAIA_ASSERT(gaia::CheckEndianess());
			}

			// Creates an empty hash map. Nothing is allocated yet, this happens at the first insert.
			// This tremendously speeds up ctor & dtor of a map that never receives an element. The
			// penalty is payed at the first insert, and not before. Lookup of this empty map works
			// because everybody points to DummyInfoByte::b. parameter bucket_count is dictated by the
			// standard, but we can ignore it.
			explicit Table(
					size_t ROBIN_HOOD_UNUSED(bucket_count) /*unused*/, const Hash& h = Hash{},
					const KeyEqual& equal = KeyEqual{}) noexcept(noexcept(Hash(h)) && noexcept(KeyEqual(equal))):
					WHash(h),
					WKeyEqual(equal) {
				ROBIN_HOOD_TRACE(this);
				GAIA_ASSERT(gaia::CheckEndianess());
			}

			template <typename Iter>
			Table(
					Iter first, Iter last, size_t ROBIN_HOOD_UNUSED(bucket_count) /*unused*/ = 0, const Hash& h = Hash{},
					const KeyEqual& equal = KeyEqual{}):
					WHash(h),
					WKeyEqual(equal) {
				ROBIN_HOOD_TRACE(this);
				GAIA_ASSERT(gaia::CheckEndianess());
				insert(first, last);
			}

			Table(
					std::initializer_list<value_type> initlist, size_t ROBIN_HOOD_UNUSED(bucket_count) /*unused*/ = 0,
					const Hash& h = Hash{}, const KeyEqual& equal = KeyEqual{}):
					WHash(h),
					WKeyEqual(equal) {
				ROBIN_HOOD_TRACE(this);
				GAIA_ASSERT(gaia::CheckEndianess());
				insert(initlist.begin(), initlist.end());
			}

			Table(Table&& o) noexcept:
					WHash(std::move(static_cast<WHash&>(o))), WKeyEqual(std::move(static_cast<WKeyEqual&>(o))),
					DataPool(std::move(static_cast<DataPool&>(o))) {
				ROBIN_HOOD_TRACE(this)
				if (o.mMask) {
					mHashMultiplier = std::move(o.mHashMultiplier);
					mKeyVals = std::move(o.mKeyVals);
					mInfo = std::move(o.mInfo);
					mNumElements = std::move(o.mNumElements);
					mMask = std::move(o.mMask);
					mMaxNumElementsAllowed = std::move(o.mMaxNumElementsAllowed);
					mInfoInc = std::move(o.mInfoInc);
					mInfoHashShift = std::move(o.mInfoHashShift);
					// set other's mask to 0 so its destructor won't do anything
					o.init();
				}
			}

			Table& operator=(Table&& o) noexcept {
				ROBIN_HOOD_TRACE(this)
				if (&o != this) {
					if (o.mMask) {
						// only move stuff if the other map actually has some data
						destroy();
						mHashMultiplier = std::move(o.mHashMultiplier);
						mKeyVals = std::move(o.mKeyVals);
						mInfo = std::move(o.mInfo);
						mNumElements = std::move(o.mNumElements);
						mMask = std::move(o.mMask);
						mMaxNumElementsAllowed = std::move(o.mMaxNumElementsAllowed);
						mInfoInc = std::move(o.mInfoInc);
						mInfoHashShift = std::move(o.mInfoHashShift);
						WHash::operator=(std::move(static_cast<WHash&>(o)));
						WKeyEqual::operator=(std::move(static_cast<WKeyEqual&>(o)));
						DataPool::operator=(std::move(static_cast<DataPool&>(o)));

						o.init();

					} else {
						// nothing in the other map => just clear us.
						clear();
					}
				}
				return *this;
			}

			Table(const Table& o):
					WHash(static_cast<const WHash&>(o)), WKeyEqual(static_cast<const WKeyEqual&>(o)),
					DataPool(static_cast<const DataPool&>(o)) {
				ROBIN_HOOD_TRACE(this)
				if (!o.empty()) {
					// not empty: create an exact copy. it is also possible to just iterate through all
					// elements and insert them, but copying is probably faster.

					auto const numElementsWithBuffer = calcNumElementsWithBuffer(o.mMask + 1);
					auto const numBytesTotal = calcNumBytesTotal(numElementsWithBuffer);

					ROBIN_HOOD_LOG(
							"gaia::mem::mem_alloc " << numBytesTotal << " = calcNumBytesTotal(" << numElementsWithBuffer << ")")
					mHashMultiplier = o.mHashMultiplier;
					mKeyVals = static_cast<Node*>(detail::assertNotNull<std::bad_alloc>(gaia::mem::mem_alloc(numBytesTotal)));
					// no need for calloc because clonData does memcpy
					mInfo = reinterpret_cast<uint8_t*>(mKeyVals + numElementsWithBuffer);
					mNumElements = o.mNumElements;
					mMask = o.mMask;
					mMaxNumElementsAllowed = o.mMaxNumElementsAllowed;
					mInfoInc = o.mInfoInc;
					mInfoHashShift = o.mInfoHashShift;
					cloneData(o);
				}
			}

			// Creates a copy of the given map. Copy constructor of each entry is used.
			// Not sure why clang-tidy thinks this doesn't handle self assignment, it does
			// NOLINTNEXTLINE(bugprone-unhandled-self-assignment,cert-oop54-cpp)
			Table& operator=(Table const& o) {
				ROBIN_HOOD_TRACE(this)
				if (&o == this) {
					// prevent assigning of itself
					return *this;
				}

				// we keep using the old allocator and not assign the new one, because we want to keep
				// the memory available. when it is the same size.
				if (o.empty()) {
					if (0 == mMask) {
						// nothing to do, we are empty too
						return *this;
					}

					// not empty: destroy what we have there
					// clear also resets mInfo to 0, that's sometimes not necessary.
					destroy();
					init();
					WHash::operator=(static_cast<const WHash&>(o));
					WKeyEqual::operator=(static_cast<const WKeyEqual&>(o));
					DataPool::operator=(static_cast<DataPool const&>(o));

					return *this;
				}

				// clean up old stuff
				Destroyer<Self, IsFlat && std::is_trivially_destructible<Node>::value>{}.nodes(*this);

				if (mMask != o.mMask) {
					// no luck: we don't have the same array size allocated, so we need to realloc.
					if (0 != mMask) {
						// only deallocate if we actually have data!
						ROBIN_HOOD_LOG("std::free")
						gaia::mem::mem_free(mKeyVals);
					}

					auto const numElementsWithBuffer = calcNumElementsWithBuffer(o.mMask + 1);
					auto const numBytesTotal = calcNumBytesTotal(numElementsWithBuffer);
					ROBIN_HOOD_LOG(
							"gaia::mem::mem_alloc " << numBytesTotal << " = calcNumBytesTotal(" << numElementsWithBuffer << ")")
					mKeyVals = static_cast<Node*>(detail::assertNotNull<std::bad_alloc>(gaia::mem::mem_alloc(numBytesTotal)));

					// no need for calloc here because cloneData performs a memcpy.
					mInfo = reinterpret_cast<uint8_t*>(mKeyVals + numElementsWithBuffer);
					// sentinel is set in cloneData
				}
				WHash::operator=(static_cast<const WHash&>(o));
				WKeyEqual::operator=(static_cast<const WKeyEqual&>(o));
				DataPool::operator=(static_cast<DataPool const&>(o));
				mHashMultiplier = o.mHashMultiplier;
				mNumElements = o.mNumElements;
				mMask = o.mMask;
				mMaxNumElementsAllowed = o.mMaxNumElementsAllowed;
				mInfoInc = o.mInfoInc;
				mInfoHashShift = o.mInfoHashShift;
				cloneData(o);

				return *this;
			}

			// Swaps everything between the two maps.
			void swap(Table& o) {
				ROBIN_HOOD_TRACE(this)
				using std::swap;
				swap(o, *this);
			}

			// Clears all data, without resizing.
			void clear() {
				ROBIN_HOOD_TRACE(this)
				if (empty()) {
					// don't do anything! also important because we don't want to write to
					// DummyInfoByte::b, even though we would just write 0 to it.
					return;
				}

				Destroyer<Self, IsFlat && std::is_trivially_destructible<Node>::value>{}.nodes(*this);

				auto const numElementsWithBuffer = calcNumElementsWithBuffer(mMask + 1);
				// clear everything, then set the sentinel again
				uint8_t const z = 0;
				gaia::core::fill(mInfo, mInfo + calcNumBytesInfo(numElementsWithBuffer), z);
				mInfo[numElementsWithBuffer] = 1;

				mInfoInc = InitialInfoInc;
				mInfoHashShift = InitialInfoHashShift;
			}

			// Destroys the map and all it's contents.
			~Table() {
				ROBIN_HOOD_TRACE(this)
				destroy();
			}

			// Checks if both tables contain the same entries. Order is irrelevant.
			bool operator==(const Table& other) const {
				ROBIN_HOOD_TRACE(this)
				if (other.size() != size()) {
					return false;
				}
				for (auto const& otherEntry: other) {
					if (!has(otherEntry)) {
						return false;
					}
				}

				return true;
			}

			bool operator!=(const Table& other) const {
				ROBIN_HOOD_TRACE(this)
				return !operator==(other);
			}

			template <typename Q = mapped_type>
			typename std::enable_if<!std::is_void<Q>::value, Q&>::type operator[](const key_type& key) {
				ROBIN_HOOD_TRACE(this)
				auto idxAndState = insertKeyPrepareEmptySpot(key);
				switch (idxAndState.second) {
					case InsertionState::key_found:
						break;

					case InsertionState::new_node:
						::new (static_cast<void*>(&mKeyVals[idxAndState.first]))
								Node(*this, std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple());
						break;

					case InsertionState::overwrite_node:
						mKeyVals[idxAndState.first] =
								Node(*this, std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple());
						break;

					case InsertionState::overflow_error:
						throwOverflowError();
				}

				return mKeyVals[idxAndState.first].getSecond();
			}

			template <typename Q = mapped_type>
			typename std::enable_if<!std::is_void<Q>::value, Q&>::type operator[](key_type&& key) {
				ROBIN_HOOD_TRACE(this)
				auto idxAndState = insertKeyPrepareEmptySpot(key);
				switch (idxAndState.second) {
					case InsertionState::key_found:
						break;

					case InsertionState::new_node:
						::new (static_cast<void*>(&mKeyVals[idxAndState.first]))
								Node(*this, std::piecewise_construct, std::forward_as_tuple(std::move(key)), std::forward_as_tuple());
						break;

					case InsertionState::overwrite_node:
						mKeyVals[idxAndState.first] =
								Node(*this, std::piecewise_construct, std::forward_as_tuple(std::move(key)), std::forward_as_tuple());
						break;

					case InsertionState::overflow_error:
						throwOverflowError();
				}

				return mKeyVals[idxAndState.first].getSecond();
			}

			template <typename Iter>
			void insert(Iter first, Iter last) {
				for (; first != last; ++first) {
					// value_type ctor needed because this might be called with std::pair's
					insert(value_type(*first));
				}
			}

			void insert(std::initializer_list<value_type> ilist) {
				for (auto&& vt: ilist) {
					insert(std::move(vt));
				}
			}

			template <typename... Args>
			std::pair<iterator, bool> emplace(Args&&... args) {
				ROBIN_HOOD_TRACE(this)
				Node n{*this, std::forward<Args>(args)...};
				auto idxAndState = insertKeyPrepareEmptySpot(getFirstConst(n));
				switch (idxAndState.second) {
					case InsertionState::key_found:
						n.destroy(*this);
						break;

					case InsertionState::new_node:
						::new (static_cast<void*>(&mKeyVals[idxAndState.first])) Node(*this, std::move(n));
						break;

					case InsertionState::overwrite_node:
						mKeyVals[idxAndState.first] = std::move(n);
						break;

					case InsertionState::overflow_error:
						n.destroy(*this);
						throwOverflowError();
						break;
				}

				return std::make_pair(
						iterator(mKeyVals + idxAndState.first, mInfo + idxAndState.first),
						InsertionState::key_found != idxAndState.second);
			}

			template <typename... Args>
			iterator emplace_hint(const_iterator position, Args&&... args) {
				(void)position;
				return emplace(std::forward<Args>(args)...).first;
			}

			template <typename... Args>
			std::pair<iterator, bool> try_emplace(const key_type& key, Args&&... args) {
				return try_emplace_impl(key, std::forward<Args>(args)...);
			}

			template <typename... Args>
			std::pair<iterator, bool> try_emplace(key_type&& key, Args&&... args) {
				return try_emplace_impl(std::move(key), std::forward<Args>(args)...);
			}

			template <typename... Args>
			iterator try_emplace(const_iterator hint, const key_type& key, Args&&... args) {
				(void)hint;
				return try_emplace_impl(key, std::forward<Args>(args)...).first;
			}

			template <typename... Args>
			iterator try_emplace(const_iterator hint, key_type&& key, Args&&... args) {
				(void)hint;
				return try_emplace_impl(std::move(key), std::forward<Args>(args)...).first;
			}

			template <typename Mapped>
			std::pair<iterator, bool> insert_or_assign(const key_type& key, Mapped&& obj) {
				return insertOrAssignImpl(key, std::forward<Mapped>(obj));
			}

			template <typename Mapped>
			std::pair<iterator, bool> insert_or_assign(key_type&& key, Mapped&& obj) {
				return insertOrAssignImpl(std::move(key), std::forward<Mapped>(obj));
			}

			template <typename Mapped>
			iterator insert_or_assign(const_iterator hint, const key_type& key, Mapped&& obj) {
				(void)hint;
				return insertOrAssignImpl(key, std::forward<Mapped>(obj)).first;
			}

			template <typename Mapped>
			iterator insert_or_assign(const_iterator hint, key_type&& key, Mapped&& obj) {
				(void)hint;
				return insertOrAssignImpl(std::move(key), std::forward<Mapped>(obj)).first;
			}

			std::pair<iterator, bool> insert(const value_type& keyval) {
				ROBIN_HOOD_TRACE(this)
				return emplace(keyval);
			}

			iterator insert(const_iterator hint, const value_type& keyval) {
				(void)hint;
				return emplace(keyval).first;
			}

			std::pair<iterator, bool> insert(value_type&& keyval) {
				return emplace(std::move(keyval));
			}

			iterator insert(const_iterator hint, value_type&& keyval) {
				(void)hint;
				return emplace(std::move(keyval)).first;
			}

			// Returns 1 if key is found, 0 otherwise.
			GAIA_NODISCARD size_t count(const key_type& key) const {
				ROBIN_HOOD_TRACE(this)
				auto kv = mKeyVals + findIdx(key);
				if (kv != reinterpret_cast_no_cast_align_warning<Node*>(mInfo)) {
					return 1;
				}
				return 0;
			}

			template <typename OtherKey, typename Self_ = Self>
			GAIA_NODISCARD typename std::enable_if<Self_::is_transparent, size_t>::type count(const OtherKey& key) const {
				ROBIN_HOOD_TRACE(this)
				auto kv = mKeyVals + findIdx(key);
				if (kv != reinterpret_cast_no_cast_align_warning<Node*>(mInfo)) {
					return 1;
				}
				return 0;
			}

			GAIA_NODISCARD bool contains(const key_type& key) const {
				return 1U == count(key);
			}

			template <typename OtherKey, typename Self_ = Self>
			GAIA_NODISCARD typename std::enable_if<Self_::is_transparent, bool>::type contains(const OtherKey& key) const {
				return 1U == count(key);
			}

			// Returns a reference to the value found for key.
			// Throws std::out_of_range if element cannot be found
			template <typename Q = mapped_type>
			GAIA_NODISCARD typename std::enable_if<!std::is_void<Q>::value, Q&>::type at(key_type const& key) {
				ROBIN_HOOD_TRACE(this)
				auto kv = mKeyVals + findIdx(key);
				if (kv == reinterpret_cast_no_cast_align_warning<Node*>(mInfo)) {
					doThrow<ROBIN_HOOD_STD_OUT_OF_RANGE>("key not found");
				}
				return kv->getSecond();
			}

			// Returns a reference to the value found for key.
			// Throws std::out_of_range if element cannot be found
			template <typename Q = mapped_type>
			GAIA_NODISCARD typename std::enable_if<!std::is_void<Q>::value, Q const&>::type at(key_type const& key) const {
				ROBIN_HOOD_TRACE(this)
				auto kv = mKeyVals + findIdx(key);
				if (kv == reinterpret_cast_no_cast_align_warning<Node*>(mInfo)) {
					doThrow<ROBIN_HOOD_STD_OUT_OF_RANGE>("key not found");
				}
				return kv->getSecond();
			}

			GAIA_NODISCARD const_iterator find(const key_type& key) const {
				ROBIN_HOOD_TRACE(this)
				const size_t idx = findIdx(key);
				return const_iterator{mKeyVals + idx, mInfo + idx};
			}

			template <typename OtherKey>
			GAIA_NODISCARD const_iterator find(const OtherKey& key, is_transparent_tag /*unused*/) const {
				ROBIN_HOOD_TRACE(this)
				const size_t idx = findIdx(key);
				return const_iterator{mKeyVals + idx, mInfo + idx};
			}

			template <typename OtherKey, typename Self_ = Self>
			GAIA_NODISCARD typename std::enable_if<Self_::is_transparent, const_iterator>::type
			find(const OtherKey& key) const {
				ROBIN_HOOD_TRACE(this)
				const size_t idx = findIdx(key);
				return const_iterator{mKeyVals + idx, mInfo + idx};
			}

			GAIA_NODISCARD iterator find(const key_type& key) {
				ROBIN_HOOD_TRACE(this)
				const size_t idx = findIdx(key);
				return iterator{mKeyVals + idx, mInfo + idx};
			}

			template <typename OtherKey>
			GAIA_NODISCARD iterator find(const OtherKey& key, is_transparent_tag /*unused*/) {
				ROBIN_HOOD_TRACE(this)
				const size_t idx = findIdx(key);
				return iterator{mKeyVals + idx, mInfo + idx};
			}

			template <typename OtherKey, typename Self_ = Self>
			GAIA_NODISCARD typename std::enable_if<Self_::is_transparent, iterator>::type find(const OtherKey& key) {
				ROBIN_HOOD_TRACE(this)
				const size_t idx = findIdx(key);
				return iterator{mKeyVals + idx, mInfo + idx};
			}

			GAIA_NODISCARD iterator begin() {
				ROBIN_HOOD_TRACE(this)
				if (empty()) {
					return end();
				}
				return iterator(mKeyVals, mInfo, fast_forward_tag{});
			}
			GAIA_NODISCARD const_iterator begin() const {
				ROBIN_HOOD_TRACE(this)
				return cbegin();
			}
			GAIA_NODISCARD const_iterator cbegin() const {
				ROBIN_HOOD_TRACE(this)
				if (empty()) {
					return cend();
				}
				return const_iterator(mKeyVals, mInfo, fast_forward_tag{});
			}

			GAIA_NODISCARD iterator end() {
				ROBIN_HOOD_TRACE(this)
				// no need to supply valid info pointer: end() must not be dereferenced, and only node
				// pointer is compared.
				return iterator{reinterpret_cast_no_cast_align_warning<Node*>(mInfo), nullptr};
			}
			GAIA_NODISCARD const_iterator end() const {
				ROBIN_HOOD_TRACE(this)
				return cend();
			}
			GAIA_NODISCARD const_iterator cend() const {
				ROBIN_HOOD_TRACE(this)
				return const_iterator{reinterpret_cast_no_cast_align_warning<Node*>(mInfo), nullptr};
			}

			iterator erase(const_iterator pos) {
				ROBIN_HOOD_TRACE(this)
				// its safe to perform const cast here
				// NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
				return erase(iterator{const_cast<Node*>(pos.mKeyVals), const_cast<uint8_t*>(pos.mInfo)});
			}

			// Erases element at pos, returns iterator to the next element.
			iterator erase(iterator pos) {
				ROBIN_HOOD_TRACE(this)
				// we assume that pos always points to a valid entry, and not end().
				auto const idx = static_cast<size_t>(pos.mKeyVals - mKeyVals);

				shiftDown(idx);
				--mNumElements;

				if (*pos.mInfo) {
					// we've backward shifted, return this again
					return pos;
				}

				// no backward shift, return next element
				return ++pos;
			}

			size_t erase(const key_type& key) {
				ROBIN_HOOD_TRACE(this)
				size_t idx{};
				InfoType info{};
				keyToIdx(key, &idx, &info);

				// check while info matches with the source idx
				do {
					if (info == mInfo[idx] && WKeyEqual::operator()(key, mKeyVals[idx].getFirst())) {
						shiftDown(idx);
						--mNumElements;
						return 1;
					}
					next(&info, &idx);
				} while (info <= mInfo[idx]);

				// nothing found to delete
				return 0;
			}

			// reserves space for the specified number of elements. Makes sure the old data fits.
			// exactly the same as reserve(c).
			void rehash(size_t c) {
				// forces a reserve
				reserve(c, true);
			}

			// reserves space for the specified number of elements. Makes sure the old data fits.
			// Exactly the same as rehash(c). Use rehash(0) to shrink to fit.
			void reserve(size_t c) {
				// reserve, but don't force rehash
				reserve(c, false);
			}

			// If possible reallocates the map to a smaller one. This frees the underlying table.
			// Does not do anything if load_factor is too large for decreasing the table's size.
			void compact() {
				ROBIN_HOOD_TRACE(this)
				auto newSize = InitialNumElements;
				while (calcMaxNumElementsAllowed(newSize) < mNumElements && newSize != 0) {
					newSize *= 2;
				}
				if GAIA_UNLIKELY (newSize == 0) {
					throwOverflowError();
				}

				ROBIN_HOOD_LOG("newSize > mMask + 1: " << newSize << " > " << mMask << " + 1")

				// only actually do anything when the new size is bigger than the old one. This prevents to
				// continuously allocate for each reserve() call.
				if (newSize < mMask + 1) {
					rehashPowerOfTwo(newSize, true);
				}
			}

			GAIA_NODISCARD size_type size() const noexcept {
				ROBIN_HOOD_TRACE(this)
				return mNumElements;
			}

			GAIA_NODISCARD size_type max_size() const noexcept {
				ROBIN_HOOD_TRACE(this)
				return static_cast<size_type>(-1);
			}

			GAIA_NODISCARD bool empty() const noexcept {
				ROBIN_HOOD_TRACE(this)
				return 0 == mNumElements;
			}

			GAIA_NODISCARD float max_load_factor() const noexcept {
				ROBIN_HOOD_TRACE(this)
				return MaxLoadFactor100 / 100.0F;
			}

			// Average number of elements per bucket. Since we allow only 1 per bucket
			GAIA_NODISCARD float load_factor() const noexcept {
				ROBIN_HOOD_TRACE(this)
				return static_cast<float>(size()) / static_cast<float>(mMask + 1);
			}

			GAIA_NODISCARD size_t mask() const noexcept {
				ROBIN_HOOD_TRACE(this)
				return mMask;
			}

			GAIA_NODISCARD size_t calcMaxNumElementsAllowed(size_t maxElements) const noexcept {
				if GAIA_LIKELY (maxElements <= (size_t(-1) / 100)) {
					return maxElements * MaxLoadFactor100 / 100;
				}

				// we might be a bit inprecise, but since maxElements is quite large that doesn't matter
				return (maxElements / 100) * MaxLoadFactor100;
			}

			GAIA_NODISCARD size_t calcNumBytesInfo(size_t numElements) const noexcept {
				// we add a uint64_t, which houses the sentinel (first byte) and padding so we can load
				// 64bit types.
				return numElements + sizeof(uint64_t);
			}

			GAIA_NODISCARD size_t calcNumElementsWithBuffer(size_t numElements) const noexcept {
				auto maxNumElementsAllowed = calcMaxNumElementsAllowed(numElements);
				return numElements + gaia::core::get_min(maxNumElementsAllowed, (static_cast<size_t>(0xFF)));
			}

			// calculation only allowed for 2^n values
			GAIA_NODISCARD size_t calcNumBytesTotal(size_t numElements) const {
#if ROBIN_HOOD(BITNESS) == 64
				return numElements * sizeof(Node) + calcNumBytesInfo(numElements);
#else
				// make sure we're doing 64bit operations, so we are at least safe against 32bit overflows.
				auto const ne = static_cast<uint64_t>(numElements);
				auto const s = static_cast<uint64_t>(sizeof(Node));
				auto const infos = static_cast<uint64_t>(calcNumBytesInfo(numElements));

				auto const total64 = ne * s + infos;
				auto const total = static_cast<size_t>(total64);

				if GAIA_UNLIKELY (static_cast<uint64_t>(total) != total64) {
					throwOverflowError();
				}
				return total;
#endif
			}

		private:
			template <typename Q = mapped_type>
			GAIA_NODISCARD typename std::enable_if<!std::is_void<Q>::value, bool>::type has(const value_type& e) const {
				ROBIN_HOOD_TRACE(this)
				auto it = find(e.first);
				return it != end() && it->second == e.second;
			}

			template <typename Q = mapped_type>
			GAIA_NODISCARD typename std::enable_if<std::is_void<Q>::value, bool>::type has(const value_type& e) const {
				ROBIN_HOOD_TRACE(this)
				return find(e) != end();
			}

			void reserve(size_t c, bool forceRehash) {
				ROBIN_HOOD_TRACE(this)
				auto const minElementsAllowed = gaia::core::get_max(c, mNumElements);
				auto newSize = InitialNumElements;
				while (calcMaxNumElementsAllowed(newSize) < minElementsAllowed && newSize != 0) {
					newSize *= 2;
				}
				if GAIA_UNLIKELY (newSize == 0) {
					throwOverflowError();
				}

				ROBIN_HOOD_LOG("newSize > mMask + 1: " << newSize << " > " << mMask << " + 1")

				// only actually do anything when the new size is bigger than the old one. This prevents to
				// continuously allocate for each reserve() call.
				if (forceRehash || newSize > mMask + 1) {
					rehashPowerOfTwo(newSize, false);
				}
			}

			// reserves space for at least the specified number of elements.
			// only works if numBuckets if power of two
			// True on success, false otherwise
			void rehashPowerOfTwo(size_t numBuckets, bool forceFree) {
				ROBIN_HOOD_TRACE(this)

				Node* const oldKeyVals = mKeyVals;
				uint8_t const* const oldInfo = mInfo;

				const size_t oldMaxElementsWithBuffer = calcNumElementsWithBuffer(mMask + 1);

				// resize operation: move stuff
				initData(numBuckets);
				if (oldMaxElementsWithBuffer > 1) {
					for (size_t i = 0; i < oldMaxElementsWithBuffer; ++i) {
						if (oldInfo[i] != 0) {
							// might throw an exception, which is really bad since we are in the middle of
							// moving stuff.
							insert_move(std::move(oldKeyVals[i]));
							// destroy the node but DON'T destroy the data.
							oldKeyVals[i].~Node();
						}
					}

					// this check is not necessary as it's guarded by the previous if, but it helps
					// silence g++'s overeager "attempt to free a non-heap object 'map'
					// [-Werror=free-nonheap-object]" warning.
					if (oldKeyVals != reinterpret_cast_no_cast_align_warning<Node*>(&mMask)) {
						// don't destroy old data: put it into the pool instead
						if (forceFree) {
							gaia::mem::mem_free(oldKeyVals);
						} else {
							DataPool::addOrFree(oldKeyVals, calcNumBytesTotal(oldMaxElementsWithBuffer));
						}
					}
				}
			}

			GAIA_NOINLINE void throwOverflowError() const {
#if ROBIN_HOOD(HAS_EXCEPTIONS)
				throw std::overflow_error("robin_hood::map overflow");
#else
				abort();
#endif
			}

			template <typename OtherKey, typename... Args>
			std::pair<iterator, bool> try_emplace_impl(OtherKey&& key, Args&&... args) {
				ROBIN_HOOD_TRACE(this)
				auto idxAndState = insertKeyPrepareEmptySpot(key);
				switch (idxAndState.second) {
					case InsertionState::key_found:
						break;

					case InsertionState::new_node:
						::new (static_cast<void*>(&mKeyVals[idxAndState.first])) Node(
								*this, std::piecewise_construct, std::forward_as_tuple(std::forward<OtherKey>(key)),
								std::forward_as_tuple(std::forward<Args>(args)...));
						break;

					case InsertionState::overwrite_node:
						mKeyVals[idxAndState.first] = Node(
								*this, std::piecewise_construct, std::forward_as_tuple(std::forward<OtherKey>(key)),
								std::forward_as_tuple(std::forward<Args>(args)...));
						break;

					case InsertionState::overflow_error:
						throwOverflowError();
						break;
				}

				return std::make_pair(
						iterator(mKeyVals + idxAndState.first, mInfo + idxAndState.first),
						InsertionState::key_found != idxAndState.second);
			}

			template <typename OtherKey, typename Mapped>
			std::pair<iterator, bool> insertOrAssignImpl(OtherKey&& key, Mapped&& obj) {
				ROBIN_HOOD_TRACE(this)
				auto idxAndState = insertKeyPrepareEmptySpot(key);
				switch (idxAndState.second) {
					case InsertionState::key_found:
						mKeyVals[idxAndState.first].getSecond() = std::forward<Mapped>(obj);
						break;

					case InsertionState::new_node:
						::new (static_cast<void*>(&mKeyVals[idxAndState.first])) Node(
								*this, std::piecewise_construct, std::forward_as_tuple(std::forward<OtherKey>(key)),
								std::forward_as_tuple(std::forward<Mapped>(obj)));
						break;

					case InsertionState::overwrite_node:
						mKeyVals[idxAndState.first] = Node(
								*this, std::piecewise_construct, std::forward_as_tuple(std::forward<OtherKey>(key)),
								std::forward_as_tuple(std::forward<Mapped>(obj)));
						break;

					case InsertionState::overflow_error:
						throwOverflowError();
						break;
				}

				return std::make_pair(
						iterator(mKeyVals + idxAndState.first, mInfo + idxAndState.first),
						InsertionState::key_found != idxAndState.second);
			}

			void initData(size_t max_elements) {
				mNumElements = 0;
				mMask = max_elements - 1;
				mMaxNumElementsAllowed = calcMaxNumElementsAllowed(max_elements);

				auto const numElementsWithBuffer = calcNumElementsWithBuffer(max_elements);

				// malloc & zero mInfo. Faster than calloc everything.
				auto const numBytesTotal = calcNumBytesTotal(numElementsWithBuffer);
				ROBIN_HOOD_LOG("std::calloc " << numBytesTotal << " = calcNumBytesTotal(" << numElementsWithBuffer << ")")
				mKeyVals = reinterpret_cast<Node*>(detail::assertNotNull<std::bad_alloc>(gaia::mem::mem_alloc(numBytesTotal)));
				mInfo = reinterpret_cast<uint8_t*>(mKeyVals + numElementsWithBuffer);
				std::memset(mInfo, 0, numBytesTotal - numElementsWithBuffer * sizeof(Node));

				// set sentinel
				mInfo[numElementsWithBuffer] = 1;

				mInfoInc = InitialInfoInc;
				mInfoHashShift = InitialInfoHashShift;
			}

			enum class InsertionState { overflow_error, key_found, new_node, overwrite_node };

			// Finds key, and if not already present prepares a spot where to pot the key & value.
			// This potentially shifts nodes out of the way, updates mInfo and number of inserted
			// elements, so the only operation left to do is create/assign a new node at that spot.
			template <typename OtherKey>
			std::pair<size_t, InsertionState> insertKeyPrepareEmptySpot(OtherKey&& key) {
				for (int i = 0; i < 256; ++i) {
					size_t idx{};
					InfoType info{};
					keyToIdx(key, &idx, &info);
					nextWhileLess(&info, &idx);

					// while we potentially have a match
					while (info == mInfo[idx]) {
						if (WKeyEqual::operator()(key, mKeyVals[idx].getFirst())) {
							// key already exists, do NOT insert.
							// see http://en.cppreference.com/w/cpp/container/unordered_map/insert
							return std::make_pair(idx, InsertionState::key_found);
						}
						next(&info, &idx);
					}

					// unlikely that this evaluates to true
					if GAIA_UNLIKELY (mNumElements >= mMaxNumElementsAllowed) {
						if (!increase_size()) {
							return std::make_pair(size_t(0), InsertionState::overflow_error);
						}
						continue;
					}

					// key not found, so we are now exactly where we want to insert it.
					auto const insertion_idx = idx;
					auto const insertion_info = info;
					if GAIA_UNLIKELY (insertion_info + mInfoInc > 0xFF) {
						mMaxNumElementsAllowed = 0;
					}

					// find an empty spot
					while (0 != mInfo[idx]) {
						next(&info, &idx);
					}

					if (idx != insertion_idx) {
						shiftUp(idx, insertion_idx);
					}
					// put at empty spot
					mInfo[insertion_idx] = static_cast<uint8_t>(insertion_info);
					++mNumElements;
					return std::make_pair(
							insertion_idx, idx == insertion_idx ? InsertionState::new_node : InsertionState::overwrite_node);
				}

				// enough attempts failed, so finally give up.
				return std::make_pair(size_t(0), InsertionState::overflow_error);
			}

			bool try_increase_info() {
				ROBIN_HOOD_LOG(
						"mInfoInc=" << mInfoInc << ", numElements=" << mNumElements
												<< ", maxNumElementsAllowed=" << calcMaxNumElementsAllowed(mMask + 1))
				if (mInfoInc <= 2) {
					// need to be > 2 so that shift works (otherwise undefined behavior!)
					return false;
				}
				// we got space left, try to make info smaller
				mInfoInc = static_cast<uint8_t>(mInfoInc >> 1U);

				// remove one bit of the hash, leaving more space for the distance info.
				// This is extremely fast because we can operate on 8 bytes at once.
				++mInfoHashShift;
				auto const numElementsWithBuffer = calcNumElementsWithBuffer(mMask + 1);

				for (size_t i = 0; i < numElementsWithBuffer; i += 8) {
					auto val = unaligned_load<uint64_t>(mInfo + i);
					val = (val >> 1U) & UINT64_C(0x7f7f7f7f7f7f7f7f);
					std::memcpy(mInfo + i, &val, sizeof(val));
				}
				// update sentinel, which might have been cleared out!
				mInfo[numElementsWithBuffer] = 1;

				mMaxNumElementsAllowed = calcMaxNumElementsAllowed(mMask + 1);
				return true;
			}

			// True if resize was possible, false otherwise
			bool increase_size() {
				// nothing allocated yet? just allocate InitialNumElements
				if (0 == mMask) {
					initData(InitialNumElements);
					return true;
				}

				auto const maxNumElementsAllowed = calcMaxNumElementsAllowed(mMask + 1);
				if (mNumElements < maxNumElementsAllowed && try_increase_info()) {
					return true;
				}

				ROBIN_HOOD_LOG(
						"mNumElements=" << mNumElements << ", maxNumElementsAllowed=" << maxNumElementsAllowed << ", load="
														<< (static_cast<double>(mNumElements) * 100.0 / (static_cast<double>(mMask) + 1)))

				if (mNumElements * 2 < calcMaxNumElementsAllowed(mMask + 1)) {
					// we have to resize, even though there would still be plenty of space left!
					// Try to rehash instead. Delete freed memory so we don't steadyily increase mem in case
					// we have to rehash a few times
					nextHashMultiplier();
					rehashPowerOfTwo(mMask + 1, true);
				} else {
					// we've reached the capacity of the map, so the hash seems to work nice. Keep using it.
					rehashPowerOfTwo((mMask + 1) * 2, false);
				}
				return true;
			}

			void nextHashMultiplier() {
				// adding an *even* number, so that the multiplier will always stay odd. This is necessary
				// so that the hash stays a mixing function (and thus doesn't have any information loss).
				mHashMultiplier += UINT64_C(0xc4ceb9fe1a85ec54);
			}

			void destroy() {
				if (0 == mMask) {
					// don't deallocate!
					return;
				}

				Destroyer<Self, IsFlat && std::is_trivially_destructible<Node>::value>{}.nodesDoNotDeallocate(*this);

				// This protection against not deleting mMask shouldn't be needed as it's sufficiently
				// protected with the 0==mMask check, but I have this anyways because g++ 7 otherwise
				// reports a compile error: attempt to free a non-heap object 'fm'
				// [-Werror=free-nonheap-object]
				if (mKeyVals != reinterpret_cast_no_cast_align_warning<Node*>(&mMask)) {
					ROBIN_HOOD_LOG("std::free")
					gaia::mem::mem_free(mKeyVals);
				}
			}

			void init() noexcept {
				mKeyVals = reinterpret_cast_no_cast_align_warning<Node*>(&mMask);
				mInfo = reinterpret_cast<uint8_t*>(&mMask);
				mNumElements = 0;
				mMask = 0;
				mMaxNumElementsAllowed = 0;
				mInfoInc = InitialInfoInc;
				mInfoHashShift = InitialInfoHashShift;
			}

			// members are sorted so no padding occurs
			uint64_t mHashMultiplier = UINT64_C(0xc4ceb9fe1a85ec53); // 8 byte  8
			Node* mKeyVals = reinterpret_cast_no_cast_align_warning<Node*>(&mMask); // 8 byte 16
			uint8_t* mInfo = reinterpret_cast<uint8_t*>(&mMask); // 8 byte 24
			size_t mNumElements = 0; // 8 byte 32
			size_t mMask = 0; // 8 byte 40
			size_t mMaxNumElementsAllowed = 0; // 8 byte 48
			InfoType mInfoInc = InitialInfoInc; // 4 byte 52
			InfoType mInfoHashShift = InitialInfoHashShift; // 4 byte 56
																											// 16 byte 56 if NodeAllocator
		};

	} // namespace detail

	// map

	template <
			typename Key, typename T, typename Hash = hash<Key>, typename KeyEqual = gaia::core::equal_to<Key>,
			size_t MaxLoadFactor100 = 80>
	using unordered_flat_map = detail::Table<true, MaxLoadFactor100, Key, T, Hash, KeyEqual>;

	template <
			typename Key, typename T, typename Hash = hash<Key>, typename KeyEqual = gaia::core::equal_to<Key>,
			size_t MaxLoadFactor100 = 80>
	using unordered_node_map = detail::Table<false, MaxLoadFactor100, Key, T, Hash, KeyEqual>;

	template <
			typename Key, typename T, typename Hash = hash<Key>, typename KeyEqual = gaia::core::equal_to<Key>,
			size_t MaxLoadFactor100 = 80>
	using unordered_map = detail::Table<
			sizeof(robin_hood::pair<Key, T>) <= sizeof(size_t) * 6 &&
					std::is_nothrow_move_constructible<robin_hood::pair<Key, T>>::value &&
					std::is_nothrow_move_assignable<robin_hood::pair<Key, T>>::value,
			MaxLoadFactor100, Key, T, Hash, KeyEqual>;

	// set

	template <
			typename Key, typename Hash = hash<Key>, typename KeyEqual = gaia::core::equal_to<Key>,
			size_t MaxLoadFactor100 = 80>
	using unordered_flat_set = detail::Table<true, MaxLoadFactor100, Key, void, Hash, KeyEqual>;

	template <
			typename Key, typename Hash = hash<Key>, typename KeyEqual = gaia::core::equal_to<Key>,
			size_t MaxLoadFactor100 = 80>
	using unordered_node_set = detail::Table<false, MaxLoadFactor100, Key, void, Hash, KeyEqual>;

	template <
			typename Key, typename Hash = hash<Key>, typename KeyEqual = gaia::core::equal_to<Key>,
			size_t MaxLoadFactor100 = 80>
	using unordered_set = detail::Table<
			sizeof(Key) <= sizeof(size_t) * 6 && std::is_nothrow_move_constructible<Key>::value &&
					std::is_nothrow_move_assignable<Key>::value,
			MaxLoadFactor100, Key, void, Hash, KeyEqual>;

} // namespace robin_hood

#endif

namespace gaia {
	namespace cnt {
		template <typename Key, typename Data>
		using map = robin_hood::unordered_flat_map<Key, Data>;
	} // namespace cnt
} // namespace gaia

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace gaia {
	namespace cnt {
		namespace sarr_detail {
			using difference_type = uint32_t;
			using size_type = uint32_t;
		} // namespace sarr_detail

		template <typename T>
		struct sarr_iterator {
			using iterator_category = core::random_access_iterator_tag;
			using value_type = T;
			using pointer = T*;
			using reference = T&;
			using difference_type = sarr_detail::difference_type;
			using size_type = sarr_detail::size_type;

			using iterator = sarr_iterator;

		private:
			pointer m_ptr;

		public:
			constexpr sarr_iterator(pointer ptr): m_ptr(ptr) {}

			constexpr reference operator*() const {
				return *m_ptr;
			}
			constexpr pointer operator->() const {
				return m_ptr;
			}
			constexpr iterator operator[](size_type offset) const {
				return {m_ptr + offset};
			}

			constexpr iterator& operator+=(size_type diff) {
				m_ptr += diff;
				return *this;
			}
			constexpr iterator& operator-=(size_type diff) {
				m_ptr -= diff;
				return *this;
			}
			constexpr iterator& operator++() {
				++m_ptr;
				return *this;
			}
			constexpr iterator operator++(int) {
				iterator temp(*this);
				++*this;
				return temp;
			}
			constexpr iterator& operator--() {
				--m_ptr;
				return *this;
			}
			constexpr iterator operator--(int) {
				iterator temp(*this);
				--*this;
				return temp;
			}

			constexpr iterator operator+(size_type offset) const {
				return {m_ptr + offset};
			}
			constexpr iterator operator-(size_type offset) const {
				return {m_ptr - offset};
			}
			constexpr difference_type operator-(const iterator& other) const {
				return (difference_type)(m_ptr - other.m_ptr);
			}

			GAIA_NODISCARD constexpr bool operator==(const iterator& other) const {
				return m_ptr == other.m_ptr;
			}
			GAIA_NODISCARD constexpr bool operator!=(const iterator& other) const {
				return m_ptr != other.m_ptr;
			}
			GAIA_NODISCARD constexpr bool operator>(const iterator& other) const {
				return m_ptr > other.m_ptr;
			}
			GAIA_NODISCARD constexpr bool operator>=(const iterator& other) const {
				return m_ptr >= other.m_ptr;
			}
			GAIA_NODISCARD constexpr bool operator<(const iterator& other) const {
				return m_ptr < other.m_ptr;
			}
			GAIA_NODISCARD constexpr bool operator<=(const iterator& other) const {
				return m_ptr <= other.m_ptr;
			}
		};

		template <typename T>
		struct sarr_iterator_soa {
			using iterator_category = core::random_access_iterator_tag;
			using value_type = T;
			// using pointer = T*; not supported
			// using reference = T&; not supported
			using difference_type = sarr_detail::difference_type;
			using size_type = sarr_detail::size_type;

			using iterator = sarr_iterator_soa;

		private:
			uint8_t* m_ptr;
			uint32_t m_cnt;
			uint32_t m_idx;

		public:
			sarr_iterator_soa(uint8_t* ptr, uint32_t cnt, uint32_t idx): m_ptr(ptr), m_cnt(cnt), m_idx(idx) {}

			T operator*() const {
				return mem::data_view_policy<T::Layout, T>::get({m_ptr, m_cnt}, m_idx);
			}

			iterator operator[](size_type offset) const {
				return iterator(m_ptr, m_cnt, m_idx + offset);
			}

			iterator& operator+=(size_type diff) {
				m_idx += diff;
				return *this;
			}
			iterator& operator-=(size_type diff) {
				m_idx -= diff;
				return *this;
			}
			iterator& operator++() {
				++m_idx;
				return *this;
			}
			iterator operator++(int) {
				iterator temp(*this);
				++*this;
				return temp;
			}
			iterator& operator--() {
				--m_idx;
				return *this;
			}
			iterator operator--(int) {
				iterator temp(*this);
				--*this;
				return temp;
			}

			iterator operator+(size_type offset) const {
				return iterator(m_ptr, m_cnt, m_idx + offset);
			}
			iterator operator-(size_type offset) const {
				return iterator(m_ptr, m_cnt, m_idx + offset);
			}
			difference_type operator-(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return (difference_type)(m_idx - other.m_idx);
			}

			GAIA_NODISCARD bool operator==(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return m_idx == other.m_idx;
			}
			GAIA_NODISCARD bool operator!=(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return m_idx != other.m_idx;
			}
			GAIA_NODISCARD bool operator>(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return m_idx > other.m_idx;
			}
			GAIA_NODISCARD bool operator>=(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return m_idx >= other.m_idx;
			}
			GAIA_NODISCARD bool operator<(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return m_idx < other.m_idx;
			}
			GAIA_NODISCARD bool operator<=(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return m_idx <= other.m_idx;
			}
		};

		//! Array of elements of type \tparam T with fixed size and capacity \tparam N allocated on stack.
		//! Interface compatiblity with std::array where it matters.
		template <typename T, sarr_detail::size_type N>
		class sarr {
		public:
			static_assert(N > 0);

			using value_type = T;
			using reference = T&;
			using const_reference = const T&;
			using pointer = T*;
			using const_pointer = T*;
			using view_policy = mem::auto_view_policy<T>;
			using difference_type = sarr_detail::difference_type;
			using size_type = sarr_detail::size_type;

			using iterator = sarr_iterator<T>;
			using iteartor_soa = sarr_iterator_soa<T>;

			static constexpr size_type extent = N;
			static constexpr uint32_t allocated_bytes = view_policy::get_min_byte_size(0, N);

			mem::raw_data_holder<T, allocated_bytes> m_data;

			constexpr sarr() noexcept {
				mem::construct_elements((pointer)&m_data[0], extent);
			}

			~sarr() {
				mem::destruct_elements((pointer)&m_data[0], extent);
			}

			template <typename InputIt>
			constexpr sarr(InputIt first, InputIt last) noexcept {
				mem::construct_elements((pointer)&m_data[0], extent);

				const auto count = (size_type)core::distance(first, last);

				if constexpr (std::is_pointer_v<InputIt>) {
					for (size_type i = 0; i < count; ++i)
						operator[](i) = first[i];
				} else if constexpr (std::is_same_v<typename InputIt::iterator_category, core::random_access_iterator_tag>) {
					for (size_type i = 0; i < count; ++i)
						operator[](i) = *(first[i]);
				} else {
					size_type i = 0;
					for (auto it = first; it != last; ++it)
						operator[](++i) = *it;
				}
			}

			constexpr sarr(std::initializer_list<T> il): sarr(il.begin(), il.end()) {}

			constexpr sarr(const sarr& other): sarr(other.begin(), other.end()) {}

			constexpr sarr(sarr&& other) noexcept {
				GAIA_ASSERT(gaia::mem::addressof(other) != this);

				mem::construct_elements((pointer)&m_data[0], extent);
				mem::move_elements<T>((uint8_t*)m_data, (const uint8_t*)other.m_data, 0, other.size(), extent, other.extent);

				other.m_cnt = size_type(0);
			}

			sarr& operator=(std::initializer_list<T> il) {
				*this = sarr(il.begin(), il.end());
				return *this;
			}

			constexpr sarr& operator=(const sarr& other) {
				GAIA_ASSERT(gaia::mem::addressof(other) != this);

				mem::construct_elements((pointer)&m_data[0], extent);
				mem::copy_elements<T>(
						(uint8_t*)&m_data[0], (const uint8_t*)&other.m_data[0], 0, other.size(), extent, other.extent);

				return *this;
			}

			constexpr sarr& operator=(sarr&& other) noexcept {
				GAIA_ASSERT(gaia::mem::addressof(other) != this);

				mem::construct_elements((pointer)&m_data[0], extent);
				mem::move_elements<T>(
						(uint8_t*)&m_data[0], (const uint8_t*)&other.m_data[0], 0, other.size(), extent, other.extent);

				return *this;
			}

			GAIA_NODISCARD constexpr pointer data() noexcept {
				return (pointer)&m_data[0];
			}

			GAIA_NODISCARD constexpr const_pointer data() const noexcept {
				return (const_pointer)&m_data[0];
			}

			GAIA_NODISCARD constexpr auto operator[](size_type pos) noexcept -> decltype(view_policy::set({}, 0)) {
				GAIA_ASSERT(pos < size());
				return view_policy::set({(typename view_policy::TargetCastType) & m_data[0], extent}, pos);
			}

			GAIA_NODISCARD constexpr auto operator[](size_type pos) const noexcept -> decltype(view_policy::get({}, 0)) {
				GAIA_ASSERT(pos < size());
				return view_policy::get({(typename view_policy::TargetCastType) & m_data[0], extent}, pos);
			}

			GAIA_NODISCARD constexpr size_type size() const noexcept {
				return N;
			}

			GAIA_NODISCARD constexpr bool empty() const noexcept {
				return begin() == end();
			}

			GAIA_NODISCARD constexpr size_type max_size() const noexcept {
				return N;
			}

			GAIA_NODISCARD constexpr auto front() noexcept {
				if constexpr (mem::is_soa_layout_v<T>)
					return *begin();
				else
					return (reference)*begin();
			}

			GAIA_NODISCARD constexpr auto front() const noexcept {

				if constexpr (mem::is_soa_layout_v<T>)
					return *begin();
				else
					return (const_reference)*begin();
			}

			GAIA_NODISCARD constexpr auto back() noexcept {
				if constexpr (mem::is_soa_layout_v<T>)
					return (operator[])(N - 1);
				else
					return (reference) operator[](N - 1);
			}

			GAIA_NODISCARD constexpr auto back() const noexcept {
				if constexpr (mem::is_soa_layout_v<T>)
					return operator[](N - 1);
				else
					return (const_reference) operator[](N - 1);
			}

			GAIA_NODISCARD constexpr auto begin() const noexcept {
				if constexpr (mem::is_soa_layout_v<T>)
					return iterator_soa(m_data, size(), 0);
				else
					return iterator((pointer)&m_data[0]);
			}

			GAIA_NODISCARD constexpr auto rbegin() const noexcept {
				if constexpr (mem::is_soa_layout_v<T>)
					return iterator_soa(m_data, size(), size() - 1);
				else
					return iterator((pointer)&back());
			}

			GAIA_NODISCARD constexpr auto end() const noexcept {
				if constexpr (mem::is_soa_layout_v<T>)
					return iterator_soa(m_data, size(), size());
				else
					return iterator((pointer)&m_data[0] + size());
			}

			GAIA_NODISCARD constexpr auto rend() const noexcept {
				if constexpr (mem::is_soa_layout_v<T>)
					return iterator_soa(m_data, size(), -1);
				else
					return iterator((pointer)m_data - 1);
			}

			GAIA_NODISCARD constexpr bool operator==(const sarr& other) const {
				for (size_type i = 0; i < N; ++i)
					if (!(m_data[i] == other.m_data[i]))
						return false;
				return true;
			}

			template <size_t Item>
			auto soa_view_mut() noexcept {
				return mem::data_view_policy<T::Layout, T>::template get<Item>(
						std::span<uint8_t>{(uint8_t*)&m_data[0], extent});
			}

			template <size_t Item>
			auto soa_view() const noexcept {
				return mem::data_view_policy<T::Layout, T>::template get<Item>(
						std::span<const uint8_t>{(const uint8_t*)&m_data[0], extent});
			}
		};

		namespace detail {
			template <typename T, uint32_t N, uint32_t... I>
			constexpr sarr<std::remove_cv_t<T>, N> to_array_impl(T (&a)[N], std::index_sequence<I...> /*no_name*/) {
				return {{a[I]...}};
			}
		} // namespace detail

		template <typename T, uint32_t N>
		constexpr sarr<std::remove_cv_t<T>, N> to_array(T (&a)[N]) {
			return detail::to_array_impl(a, std::make_index_sequence<N>{});
		}

		template <typename T, typename... U>
		sarr(T, U...) -> sarr<T, 1 + (uint32_t)sizeof...(U)>;

	} // namespace cnt
} // namespace gaia

namespace std {
	template <typename T, uint32_t N>
	struct tuple_size<gaia::cnt::sarr<T, N>>: std::integral_constant<uint32_t, N> {};

	template <size_t I, typename T, uint32_t N>
	struct tuple_element<I, gaia::cnt::sarr<T, N>> {
		using type = T;
	};
} // namespace std

namespace gaia {
	namespace cnt {
		template <typename T, sarr_detail::size_type N>
		using sarray = cnt::sarr<T, N>;
	} // namespace cnt
} // namespace gaia

#include <cstddef>
#include <initializer_list>
#include <tuple>
#include <type_traits>
#include <utility>

namespace gaia {
	namespace cnt {
		namespace sarr_ext_detail {
			using difference_type = uint32_t;
			using size_type = uint32_t;
		} // namespace sarr_ext_detail

		template <typename T>
		struct sarr_ext_iterator {
			using iterator_category = core::random_access_iterator_tag;
			using value_type = T;
			using pointer = T*;
			using reference = T&;
			using difference_type = sarr_ext_detail::size_type;
			using size_type = sarr_ext_detail::size_type;

			using iterator = sarr_ext_iterator;

		private:
			pointer m_ptr;

		public:
			constexpr sarr_ext_iterator(T* ptr): m_ptr(ptr) {}

			constexpr T& operator*() const {
				return *m_ptr;
			}
			constexpr T* operator->() const {
				return m_ptr;
			}
			constexpr iterator operator[](size_type offset) const {
				return {m_ptr + offset};
			}

			constexpr iterator& operator+=(size_type diff) {
				m_ptr += diff;
				return *this;
			}
			constexpr iterator& operator-=(size_type diff) {
				m_ptr -= diff;
				return *this;
			}
			constexpr iterator& operator++() {
				++m_ptr;
				return *this;
			}
			constexpr iterator operator++(int) {
				iterator temp(*this);
				++*this;
				return temp;
			}
			constexpr iterator& operator--() {
				--m_ptr;
				return *this;
			}
			constexpr iterator operator--(int) {
				iterator temp(*this);
				--*this;
				return temp;
			}

			constexpr iterator operator+(size_type offset) const {
				return {m_ptr + offset};
			}
			constexpr iterator operator-(size_type offset) const {
				return {m_ptr - offset};
			}
			constexpr difference_type operator-(const iterator& other) const {
				return (difference_type)(m_ptr - other.m_ptr);
			}

			GAIA_NODISCARD constexpr bool operator==(const iterator& other) const {
				return m_ptr == other.m_ptr;
			}
			GAIA_NODISCARD constexpr bool operator!=(const iterator& other) const {
				return m_ptr != other.m_ptr;
			}
			GAIA_NODISCARD constexpr bool operator>(const iterator& other) const {
				return m_ptr > other.m_ptr;
			}
			GAIA_NODISCARD constexpr bool operator>=(const iterator& other) const {
				return m_ptr >= other.m_ptr;
			}
			GAIA_NODISCARD constexpr bool operator<(const iterator& other) const {
				return m_ptr < other.m_ptr;
			}
			GAIA_NODISCARD constexpr bool operator<=(const iterator& other) const {
				return m_ptr <= other.m_ptr;
			}
		};

		template <typename T>
		struct sarr_ext_iterator_soa {
			using iterator_category = core::random_access_iterator_tag;
			using value_type = T;
			// using pointer = T*; not supported
			// using reference = T&; not supported
			using difference_type = sarr_ext_detail::size_type;
			using size_type = sarr_ext_detail::size_type;

			using iterator = sarr_ext_iterator_soa;

		private:
			uint8_t* m_ptr;
			uint32_t m_cnt;
			uint32_t m_idx;

		public:
			sarr_ext_iterator_soa(uint8_t* ptr, uint32_t cnt, uint32_t idx): m_ptr(ptr), m_cnt(cnt), m_idx(idx) {}

			T operator*() const {
				return mem::data_view_policy<T::Layout, T>::get({m_ptr, m_cnt}, m_idx);
			}

			iterator operator[](size_type offset) const {
				return iterator(m_ptr, m_cnt, m_idx + offset);
			}

			iterator& operator+=(size_type diff) {
				m_idx += diff;
				return *this;
			}
			iterator& operator-=(size_type diff) {
				m_idx -= diff;
				return *this;
			}
			iterator& operator++() {
				++m_idx;
				return *this;
			}
			iterator operator++(int) {
				iterator temp(*this);
				++*this;
				return temp;
			}
			iterator& operator--() {
				--m_idx;
				return *this;
			}
			iterator operator--(int) {
				iterator temp(*this);
				--*this;
				return temp;
			}

			iterator operator+(size_type offset) const {
				return iterator(m_ptr, m_cnt, m_idx + offset);
			}
			iterator operator-(size_type offset) const {
				return iterator(m_ptr, m_cnt, m_idx + offset);
			}
			difference_type operator-(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return (difference_type)(m_idx - other.m_idx);
			}

			GAIA_NODISCARD bool operator==(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return m_idx == other.m_idx;
			}
			GAIA_NODISCARD bool operator!=(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return m_idx != other.m_idx;
			}
			GAIA_NODISCARD bool operator>(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return m_idx > other.m_idx;
			}
			GAIA_NODISCARD bool operator>=(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return m_idx >= other.m_idx;
			}
			GAIA_NODISCARD bool operator<(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return m_idx < other.m_idx;
			}
			GAIA_NODISCARD bool operator<=(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return m_idx <= other.m_idx;
			}
		};

		//! Array of elements of type \tparam T with fixed capacity \tparam N and variable size allocated on stack.
		//! Interface compatiblity with std::array where it matters.
		template <typename T, sarr_ext_detail::size_type N>
		class sarr_ext {
		public:
			static_assert(N > 0);

			using iterator_category = core::random_access_iterator_tag;
			using value_type = T;
			using reference = T&;
			using const_reference = const T&;
			using pointer = T*;
			using const_pointer = T*;
			using view_policy = mem::auto_view_policy<T>;
			using difference_type = sarr_ext_detail::difference_type;
			using size_type = sarr_ext_detail::size_type;

			using iterator = sarr_ext_iterator<T>;
			using iterator_soa = sarr_ext_iterator_soa<T>;

			static constexpr size_type extent = N;
			static constexpr uint32_t allocated_bytes = view_policy::get_min_byte_size(0, N);

		private:
			mem::raw_data_holder<T, allocated_bytes> m_data;
			size_type m_cnt = size_type(0);

		public:
			constexpr sarr_ext() noexcept {
				mem::construct_elements((pointer)&m_data[0], extent);
			}

			~sarr_ext() {
				mem::destruct_elements((pointer)&m_data[0], extent);
			}

			constexpr sarr_ext(size_type count, reference value) noexcept {
				resize(count);

				for (auto it: *this)
					*it = value;
			}

			constexpr sarr_ext(size_type count) noexcept {
				resize(count);
			}

			template <typename InputIt>
			constexpr sarr_ext(InputIt first, InputIt last) noexcept {
				mem::construct_elements((pointer)&m_data[0], extent);

				const auto count = (size_type)core::distance(first, last);
				resize(count);

				if constexpr (std::is_pointer_v<InputIt>) {
					for (size_type i = 0; i < count; ++i)
						operator[](i) = first[i];
				} else if constexpr (std::is_same_v<typename InputIt::iterator_category, core::random_access_iterator_tag>) {
					for (size_type i = 0; i < count; ++i)
						operator[](i) = *(first[i]);
				} else {
					size_type i = 0;
					for (auto it = first; it != last; ++it)
						operator[](++i) = *it;
				}
			}

			constexpr sarr_ext(std::initializer_list<T> il): sarr_ext(il.begin(), il.end()) {}

			constexpr sarr_ext(const sarr_ext& other): sarr_ext(other.begin(), other.end()) {}

			constexpr sarr_ext(sarr_ext&& other) noexcept: m_cnt(other.m_cnt) {
				GAIA_ASSERT(gaia::mem::addressof(other) != this);

				mem::construct_elements((pointer)&m_data[0], extent);
				mem::move_elements<T>(m_data, other.m_data, 0, other.size(), extent, other.extent);

				other.m_cnt = size_type(0);
			}

			sarr_ext& operator=(std::initializer_list<T> il) {
				*this = sarr_ext(il.begin(), il.end());
				return *this;
			}

			constexpr sarr_ext& operator=(const sarr_ext& other) {
				GAIA_ASSERT(gaia::mem::addressof(other) != this);

				mem::construct_elements((pointer)&m_data[0], extent);
				resize(other.size());
				mem::copy_elements<T>(
						(uint8_t*)&m_data[0], (const uint8_t*)&other.m_data[0], 0, other.size(), extent, other.extent);

				return *this;
			}

			constexpr sarr_ext& operator=(sarr_ext&& other) noexcept {
				GAIA_ASSERT(gaia::mem::addressof(other) != this);

				mem::construct_elements((pointer)&m_data[0], extent);
				resize(other.m_cnt);
				mem::move_elements<T>(
						(uint8_t*)&m_data[0], (const uint8_t*)&other.m_data[0], 0, other.size(), extent, other.extent);

				other.m_cnt = size_type(0);

				return *this;
			}

			GAIA_NODISCARD constexpr pointer data() noexcept {
				return (pointer)&m_data[0];
			}

			GAIA_NODISCARD constexpr const_pointer data() const noexcept {
				return (const_pointer)&m_data[0];
			}

			GAIA_NODISCARD constexpr auto operator[](size_type pos) noexcept -> decltype(view_policy::set({}, 0)) {
				GAIA_ASSERT(pos < size());
				return view_policy::set({(typename view_policy::TargetCastType) & m_data[0], extent}, pos);
			}

			GAIA_NODISCARD constexpr auto operator[](size_type pos) const noexcept -> decltype(view_policy::get({}, 0)) {
				GAIA_ASSERT(pos < size());
				return view_policy::get({(typename view_policy::TargetCastType) & m_data[0], extent}, pos);
			}

			constexpr void push_back(const T& arg) noexcept {
				GAIA_ASSERT(size() < N);

				if constexpr (mem::is_soa_layout_v<T>) {
					operator[](m_cnt++) = arg;
				} else {
					auto* ptr = m_data + sizeof(T) * (m_cnt++);
					::new (ptr) T(arg);
				}
			}

			constexpr void push_back(T&& arg) noexcept {
				GAIA_ASSERT(size() < N);

				if constexpr (mem::is_soa_layout_v<T>) {
					operator[](m_cnt++) = std::forward<T>(arg);
				} else {
					auto* ptr = m_data + sizeof(T) * (m_cnt++);
					::new (ptr) T(std::forward<T>(arg));
				}
			}

			template <typename... Args>
			constexpr auto emplace_back(Args&&... args) {
				GAIA_ASSERT(size() < N);

				if constexpr (mem::is_soa_layout_v<T>) {
					operator[](m_cnt++) = T(std::forward<Args>(args)...);
					return;
				} else {
					auto* ptr = m_data + sizeof(T) * (m_cnt++);
					::new (ptr) T(std::forward<Args>(args)...);
					return (reference)*ptr;
				}
			}

			constexpr void pop_back() noexcept {
				GAIA_ASSERT(!empty());

				if constexpr (!mem::is_soa_layout_v<T>) {
					auto* ptr = m_data + sizeof(T) * m_cnt;
					((pointer)ptr)->~T();
				}

				--m_cnt;
			}

			constexpr iterator erase(iterator pos) noexcept {
				GAIA_ASSERT(pos.m_ptr >= data() && pos.m_ptr < (data() + extent - 1));

				const auto idxSrc = (size_type)core::distance(pos, begin());
				const auto idxDst = size() - 1;

				mem::shift_elements_left<T>(&m_data[0], idxSrc, idxDst);
				--m_cnt;

				return iterator((pointer)m_data + idxSrc);
			}

			iterator_soa erase(iterator_soa pos) noexcept {
				const auto idxSrc = pos.m_idx;
				const auto idxDst = size() - 1;

				mem::shift_elements_left<T>(m_data, idxSrc, idxDst);
				--m_cnt;

				return iterator_soa(m_data, m_cnt, idxSrc);
			}

			constexpr iterator erase(iterator first, iterator last) noexcept {
				GAIA_ASSERT(first.m_cnt >= 0 && first.m_cnt < size());
				GAIA_ASSERT(last.m_cnt >= 0 && last.m_cnt < size());
				GAIA_ASSERT(last.m_cnt >= first.m_cnt);

				const auto cnt = last.m_cnt - first.m_cnt;
				mem::shift_elements_left<T>(&m_data[0], first.cnt, last.cnt);
				m_cnt -= cnt;

				return {(pointer)m_data + size_type(last.m_cnt)};
			}

			iterator_soa erase(iterator_soa first, iterator_soa last) noexcept {
				static_assert(!mem::is_soa_layout_v<T>);
				GAIA_ASSERT(first.m_idx >= 0 && first.m_idx < size());
				GAIA_ASSERT(last.m_idx >= 0 && last.m_idx < size());
				GAIA_ASSERT(last.m_idx >= first.m_idx);

				const auto cnt = last.m_idx - first.m_idx;
				mem::shift_elements_left<T>(m_data, first.cnt, last.cnt);
				m_cnt -= cnt;

				return iterator_soa(m_data, m_cnt, last.m_cnt);
			}

			constexpr void clear() noexcept {
				resize(0);
			}

			constexpr void resize(size_type size) noexcept {
				GAIA_ASSERT(size <= N);
				m_cnt = size;
			}

			GAIA_NODISCARD constexpr size_type size() const noexcept {
				return m_cnt;
			}

			GAIA_NODISCARD constexpr bool empty() const noexcept {
				return size() == 0;
			}

			GAIA_NODISCARD constexpr size_type max_size() const noexcept {
				return N;
			}

			GAIA_NODISCARD constexpr auto front() noexcept {
				GAIA_ASSERT(!empty());
				if constexpr (mem::is_soa_layout_v<T>)
					return *begin();
				else
					return (reference)*begin();
			}

			GAIA_NODISCARD constexpr auto front() const noexcept {
				GAIA_ASSERT(!empty());
				if constexpr (mem::is_soa_layout_v<T>)
					return *begin();
				else
					return (const_reference)*begin();
			}

			GAIA_NODISCARD constexpr auto back() noexcept {
				GAIA_ASSERT(!empty());
				if constexpr (mem::is_soa_layout_v<T>)
					return (operator[])(N - 1);
				else
					return (reference) operator[](N - 1);
			}

			GAIA_NODISCARD constexpr auto back() const noexcept {
				GAIA_ASSERT(!empty());
				if constexpr (mem::is_soa_layout_v<T>)
					return operator[](N - 1);
				else
					return (const_reference) operator[](N - 1);
			}

			GAIA_NODISCARD constexpr auto begin() const noexcept {
				if constexpr (mem::is_soa_layout_v<T>)
					return iterator_soa(m_data, size(), 0);
				else
					return iterator((pointer)&m_data[0]);
			}

			GAIA_NODISCARD constexpr auto rbegin() const noexcept {
				if constexpr (mem::is_soa_layout_v<T>)
					return iterator_soa(m_data, size(), size() - 1);
				else
					return iterator((pointer)&back());
			}

			GAIA_NODISCARD constexpr auto end() const noexcept {
				if constexpr (mem::is_soa_layout_v<T>)
					return iterator_soa(m_data, size(), size());
				else
					return iterator((pointer)&m_data[0] + size());
			}

			GAIA_NODISCARD constexpr auto rend() const noexcept {
				if constexpr (mem::is_soa_layout_v<T>)
					return iterator_soa(m_data, size(), -1);
				else
					return iterator((pointer)m_data - 1);
			}

			GAIA_NODISCARD constexpr bool operator==(const sarr_ext& other) const {
				if (m_cnt != other.m_cnt)
					return false;
				const size_type n = size();
				for (size_type i = 0; i < n; ++i)
					if (!(m_data[i] == other.m_data[i]))
						return false;
				return true;
			}

			template <size_t Item>
			auto soa_view_mut() noexcept {
				return mem::data_view_policy<T::Layout, T>::template get<Item>(
						std::span<uint8_t>{(uint8_t*)&m_data[0], extent});
			}

			template <size_t Item>
			auto soa_view() const noexcept {
				return mem::data_view_policy<T::Layout, T>::template get<Item>(
						std::span<const uint8_t>{(const uint8_t*)&m_data[0], extent});
			}
		};

		namespace detail {
			template <typename T, uint32_t N, uint32_t... I>
			constexpr sarr_ext<std::remove_cv_t<T>, N> to_sarray_impl(T (&a)[N], std::index_sequence<I...> /*no_name*/) {
				return {{a[I]...}};
			}
		} // namespace detail

		template <typename T, uint32_t N>
		constexpr sarr_ext<std::remove_cv_t<T>, N> to_sarray(T (&a)[N]) {
			return detail::to_sarray_impl(a, std::make_index_sequence<N>{});
		}

	} // namespace cnt

} // namespace gaia

namespace std {
	template <typename T, uint32_t N>
	struct tuple_size<gaia::cnt::sarr_ext<T, N>>: std::integral_constant<uint32_t, N> {};

	template <size_t I, typename T, uint32_t N>
	struct tuple_element<I, gaia::cnt::sarr_ext<T, N>> {
		using type = T;
	};
} // namespace std

namespace gaia {
	namespace cnt {
		template <typename T, sarr_ext_detail::size_type N>
		using sarray_ext = cnt::sarr_ext<T, N>;
	} // namespace cnt
} // namespace gaia

//                 ______  _____                 ______                _________
//  ______________ ___  /_ ___(_)_______         ___  /_ ______ ______ ______  /
//  __  ___/_  __ \__  __ \__  / __  __ \        __  __ \_  __ \_  __ \_  __  /
//  _  /    / /_/ /_  /_/ /_  /  _  / / /        _  / / // /_/ // /_/ // /_/ /
//  /_/     \____/ /_.___/ /_/   /_/ /_/ ________/_/ /_/ \____/ \____/ \__,_/
//                                      _/_____/
//
// Fast & memory efficient hashtable based on robin hood hashing for C++11/14/17/20
// https://github.com/martinus/robin-hood-hashing
//
// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2018-2021 Martin Ankerl <http://martin.ankerl.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef ROBIN_HOOD_H_INCLUDED
#define ROBIN_HOOD_H_INCLUDED

// see https://semver.org/
#define ROBIN_HOOD_VERSION_MAJOR 3 // for incompatible API changes
#define ROBIN_HOOD_VERSION_MINOR 11 // for adding functionality in a backwards-compatible manner
#define ROBIN_HOOD_VERSION_PATCH 5 // for backwards-compatible bug fixes

#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <new>
#include <tuple>
#include <type_traits>
#include <utility>

// #define ROBIN_HOOD_STD_SMARTPOINTERS
#if defined(ROBIN_HOOD_STD_SMARTPOINTERS)
	#include <memory>
#endif

// #define ROBIN_HOOD_LOG_ENABLED
#ifdef ROBIN_HOOD_LOG_ENABLED
	#include <iostream>
	#define ROBIN_HOOD_LOG(...) std::cout << __FUNCTION__ << "@" << __LINE__ << ": " << __VA_ARGS__ << std::endl;
#else
	#define ROBIN_HOOD_LOG(x)
#endif

// #define ROBIN_HOOD_TRACE_ENABLED
#ifdef ROBIN_HOOD_TRACE_ENABLED
	#include <iostream>
	#define ROBIN_HOOD_TRACE(...) std::cout << __FUNCTION__ << "@" << __LINE__ << ": " << __VA_ARGS__ << std::endl;
#else
	#define ROBIN_HOOD_TRACE(x)
#endif

// #define ROBIN_HOOD_COUNT_ENABLED
#ifdef ROBIN_HOOD_COUNT_ENABLED
	#include <iostream>
	#define ROBIN_HOOD_COUNT(x) ++counts().x;
namespace robin_hood {
	struct Counts {
		uint64_t shiftUp{};
		uint64_t shiftDown{};
	};
	inline std::ostream& operator<<(std::ostream& os, Counts const& c) {
		return os << c.shiftUp << " shiftUp" << std::endl << c.shiftDown << " shiftDown" << std::endl;
	}

	static Counts& counts() {
		static Counts counts{};
		return counts;
	}
} // namespace robin_hood
#else
	#define ROBIN_HOOD_COUNT(x)
#endif

// all non-argument macros should use this facility. See
// https://www.fluentcpp.com/2019/05/28/better-macros-better-flags/
#define ROBIN_HOOD(x) ROBIN_HOOD_PRIVATE_DEFINITION_##x()

// mark unused members with this macro
#define ROBIN_HOOD_UNUSED(identifier)

// bitness
#if SIZE_MAX == UINT32_MAX
	#define ROBIN_HOOD_PRIVATE_DEFINITION_BITNESS() 32
#elif SIZE_MAX == UINT64_MAX
	#define ROBIN_HOOD_PRIVATE_DEFINITION_BITNESS() 64
#else
	#error Unsupported bitness
#endif

// exceptions
#if !defined(__cpp_exceptions) && !defined(__EXCEPTIONS) && !defined(_CPPUNWIND)
	#define ROBIN_HOOD_PRIVATE_DEFINITION_HAS_EXCEPTIONS() 0
	#define ROBIN_HOOD_STD_OUT_OF_RANGE void
#else
	#include <stdexcept>
	#define ROBIN_HOOD_PRIVATE_DEFINITION_HAS_EXCEPTIONS() 1
	#define ROBIN_HOOD_STD_OUT_OF_RANGE std::out_of_range
#endif

// count leading/trailing bits
#if !defined(ROBIN_HOOD_DISABLE_INTRINSICS)
	#if ROBIN_HOOD_PRIVATE_DEFINITION_BITNESS() == 32
		#define ROBIN_HOOD_COUNT_TRAILING_ZEROES(x) GAIA_CLZ(x)
		#define ROBIN_HOOD_COUNT_LEADING_ZEROES(x) GAIA_CTZ(x)
	#else
		#define ROBIN_HOOD_COUNT_TRAILING_ZEROES(x) GAIA_CLZ64(x)
		#define ROBIN_HOOD_COUNT_LEADING_ZEROES(x) GAIA_CTZ64(x)
	#endif
#endif

// fallthrough
#ifndef __has_cpp_attribute // For backwards compatibility
	#define __has_cpp_attribute(x) 0
#endif
#if __has_cpp_attribute(fallthrough)
	#define ROBIN_HOOD_PRIVATE_DEFINITION_FALLTHROUGH() [[fallthrough]]
#else
	#define ROBIN_HOOD_PRIVATE_DEFINITION_FALLTHROUGH()
#endif

// detect if native wchar_t type is availiable in MSVC
#ifdef _MSC_VER
	#ifdef _NATIVE_WCHAR_T_DEFINED
		#define ROBIN_HOOD_PRIVATE_DEFINITION_HAS_NATIVE_WCHART() 1
	#else
		#define ROBIN_HOOD_PRIVATE_DEFINITION_HAS_NATIVE_WCHART() 0
	#endif
#else
	#define ROBIN_HOOD_PRIVATE_DEFINITION_HAS_NATIVE_WCHART() 1
#endif

// detect if MSVC supports the pair(std::piecewise_construct_t,...) constructor being constexpr
#ifdef _MSC_VER
	#if _MSC_VER <= 1900
		#define ROBIN_HOOD_PRIVATE_DEFINITION_BROKEN_CONSTEXPR() 1
	#else
		#define ROBIN_HOOD_PRIVATE_DEFINITION_BROKEN_CONSTEXPR() 0
	#endif
#else
	#define ROBIN_HOOD_PRIVATE_DEFINITION_BROKEN_CONSTEXPR() 0
#endif

// workaround missing "is_trivially_copyable" in g++ < 5.0
// See https://stackoverflow.com/a/31798726/48181
#if GAIA_COMPILER_GCC && __GNUC__ < 5
	#define ROBIN_HOOD_IS_TRIVIALLY_COPYABLE(...) __has_trivial_copy(__VA_ARGS__)
#else
	#define ROBIN_HOOD_IS_TRIVIALLY_COPYABLE(...) std::is_trivially_copyable<__VA_ARGS__>::value
#endif

namespace robin_hood {

	namespace detail {

// make sure we static_cast to the correct type for hash_int
#if ROBIN_HOOD(BITNESS) == 64
		using SizeT = uint64_t;
#else
		using SizeT = uint32_t;
#endif

		template <typename T>
		T rotr(T x, unsigned k) {
			return (x >> k) | (x << (8U * sizeof(T) - k));
		}

		// This cast gets rid of warnings like "cast from 'uint8_t*' {aka 'unsigned char*'} to
		// 'uint64_t*' {aka 'long unsigned int*'} increases required alignment of target type". Use with
		// care!
		template <typename T>
		inline T reinterpret_cast_no_cast_align_warning(void* ptr) noexcept {
			return reinterpret_cast<T>(ptr);
		}

		template <typename T>
		inline T reinterpret_cast_no_cast_align_warning(void const* ptr) noexcept {
			return reinterpret_cast<T>(ptr);
		}

		template <typename, typename = void>
		struct has_hash: public std::false_type {};

		template <typename T>
		struct has_hash<T, std::void_t<decltype(T::hash)>>: public std::true_type {};

		// make sure this is not inlined as it is slow and dramatically enlarges code, thus making other
		// inlinings more difficult. Throws are also generally the slow path.
		template <typename E, typename... Args>
		[[noreturn]] GAIA_NOINLINE
#if ROBIN_HOOD(HAS_EXCEPTIONS)
				void
				doThrow(Args&&... args) {
			// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
			throw E(std::forward<Args>(args)...);
		}
#else
				void
				doThrow(Args&&... ROBIN_HOOD_UNUSED(args) /*unused*/) {
			abort();
		}
#endif

		template <typename E, typename T, typename... Args>
		T* assertNotNull(T* t, Args&&... args) {
			if GAIA_UNLIKELY (nullptr == t) {
				doThrow<E>(std::forward<Args>(args)...);
			}
			return t;
		}

		template <typename T>
		inline T unaligned_load(void const* ptr) noexcept {
			// using memcpy so we don't get into unaligned load problems.
			// compiler should optimize this very well anyways.
			T t;
			std::memcpy(&t, ptr, sizeof(T));
			return t;
		}

		// Allocates bulks of memory for objects of type T. This deallocates the memory in the destructor,
		// and keeps a linked list of the allocated memory around. Overhead per allocation is the size of a
		// pointer.
		template <typename T, size_t MinNumAllocs = 4, size_t MaxNumAllocs = 256>
		class BulkPoolAllocator {
		public:
			BulkPoolAllocator() noexcept = default;

			// does not copy anything, just creates a new allocator.
			BulkPoolAllocator(const BulkPoolAllocator& ROBIN_HOOD_UNUSED(o) /*unused*/) noexcept:
					mHead(nullptr), mListForFree(nullptr) {}

			BulkPoolAllocator(BulkPoolAllocator&& o) noexcept: mHead(o.mHead), mListForFree(o.mListForFree) {
				o.mListForFree = nullptr;
				o.mHead = nullptr;
			}

			BulkPoolAllocator& operator=(BulkPoolAllocator&& o) noexcept {
				reset();
				mHead = o.mHead;
				mListForFree = o.mListForFree;
				o.mListForFree = nullptr;
				o.mHead = nullptr;
				return *this;
			}

			BulkPoolAllocator&
			// NOLINTNEXTLINE(bugprone-unhandled-self-assignment,cert-oop54-cpp)
			operator=(const BulkPoolAllocator & ROBIN_HOOD_UNUSED(o) /*unused*/) noexcept {
				// does not do anything
				return *this;
			}

			~BulkPoolAllocator() noexcept {
				reset();
			}

			// Deallocates all allocated memory.
			void reset() noexcept {
				while (mListForFree) {
					T* tmp = *mListForFree;
					ROBIN_HOOD_LOG("std::free")
					gaia::mem::mem_free(mListForFree);
					mListForFree = reinterpret_cast_no_cast_align_warning<T**>(tmp);
				}
				mHead = nullptr;
			}

			// allocates, but does NOT initialize. Use in-place new constructor, e.g.
			//   T* obj = pool.allocate();
			//   ::new (static_cast<void*>(obj)) T();
			T* allocate() {
				T* tmp = mHead;
				if (!tmp) {
					tmp = performAllocation();
				}

				mHead = *reinterpret_cast_no_cast_align_warning<T**>(tmp);
				return tmp;
			}

			// does not actually deallocate but puts it in store.
			// make sure you have already called the destructor! e.g. with
			//  obj->~T();
			//  pool.deallocate(obj);
			void deallocate(T* obj) noexcept {
				*reinterpret_cast_no_cast_align_warning<T**>(obj) = mHead;
				mHead = obj;
			}

			// Adds an already allocated block of memory to the allocator. This allocator is from now on
			// responsible for freeing the data (with mem_free()). If the provided data is not large enough to
			// make use of, it is immediately freed. Otherwise it is reused and freed in the destructor.
			void addOrFree(void* ptr, const size_t numBytes) noexcept {
				// calculate number of available elements in ptr
				if (numBytes < ALIGNMENT + ALIGNED_SIZE) {
					// not enough data for at least one element. Free and return.
					ROBIN_HOOD_LOG("std::free")
					gaia::mem::mem_free(ptr);
				} else {
					ROBIN_HOOD_LOG("add to buffer")
					add(ptr, numBytes);
				}
			}

			void swap(BulkPoolAllocator<T, MinNumAllocs, MaxNumAllocs>& other) noexcept {
				using std::swap;
				swap(mHead, other.mHead);
				swap(mListForFree, other.mListForFree);
			}

		private:
			// iterates the list of allocated memory to calculate how many to alloc next.
			// Recalculating this each time saves us a size_t member.
			// This ignores the fact that memory blocks might have been added manually with addOrFree. In
			// practice, this should not matter much.
			GAIA_NODISCARD size_t calcNumElementsToAlloc() const noexcept {
				auto tmp = mListForFree;
				size_t numAllocs = MinNumAllocs;

				while (numAllocs * 2 <= MaxNumAllocs && tmp) {
					auto x = reinterpret_cast<T***>(tmp);
					tmp = *x;
					numAllocs *= 2;
				}

				return numAllocs;
			}

			// WARNING: Underflow if numBytes < ALIGNMENT! This is guarded in addOrFree().
			void add(void* ptr, const size_t numBytes) noexcept {
				const size_t numElements = (numBytes - ALIGNMENT) / ALIGNED_SIZE;

				auto data = reinterpret_cast<T**>(ptr);

				// link free list
				auto x = reinterpret_cast<T***>(data);
				*x = mListForFree;
				mListForFree = data;

				// create linked list for newly allocated data
				auto* const headT = reinterpret_cast_no_cast_align_warning<T*>(reinterpret_cast<char*>(ptr) + ALIGNMENT);

				auto* const head = reinterpret_cast<char*>(headT);

				// Visual Studio compiler automatically unrolls this loop, which is pretty cool
				for (size_t i = 0; i < numElements; ++i) {
					*reinterpret_cast_no_cast_align_warning<char**>(head + i * ALIGNED_SIZE) = head + (i + 1) * ALIGNED_SIZE;
				}

				// last one points to 0
				*reinterpret_cast_no_cast_align_warning<T**>(head + (numElements - 1) * ALIGNED_SIZE) = mHead;
				mHead = headT;
			}

			// Called when no memory is available (mHead == 0).
			// Don't inline this slow path.
			GAIA_NOINLINE T* performAllocation() {
				size_t const numElementsToAlloc = calcNumElementsToAlloc();

				// alloc new memory: [prev |T, T, ... T]
				size_t const bytes = ALIGNMENT + ALIGNED_SIZE * numElementsToAlloc;
				ROBIN_HOOD_LOG(
						"gaia::mem::mem_alloc " << bytes << " = " << ALIGNMENT << " + " << ALIGNED_SIZE << " * "
																		<< numElementsToAlloc)
				add(assertNotNull<std::bad_alloc>(gaia::mem::mem_alloc(bytes)), bytes);
				return mHead;
			}

			// enforce byte alignment of the T's
			static constexpr size_t ALIGNMENT =
					gaia::core::get_max(std::alignment_of<T>::value, std::alignment_of<T*>::value);

			static constexpr size_t ALIGNED_SIZE = ((sizeof(T) - 1) / ALIGNMENT + 1) * ALIGNMENT;

			static_assert(MinNumAllocs >= 1, "MinNumAllocs");
			static_assert(MaxNumAllocs >= MinNumAllocs, "MaxNumAllocs");
			static_assert(ALIGNED_SIZE >= sizeof(T*), "ALIGNED_SIZE");
			static_assert(0 == (ALIGNED_SIZE % sizeof(T*)), "ALIGNED_SIZE mod");
			static_assert(ALIGNMENT >= sizeof(T*), "ALIGNMENT");

			T* mHead{nullptr};
			T** mListForFree{nullptr};
		};

		template <typename T, size_t MinSize, size_t MaxSize, bool IsFlat>
		struct NodeAllocator;

		// dummy allocator that does nothing
		template <typename T, size_t MinSize, size_t MaxSize>
		struct NodeAllocator<T, MinSize, MaxSize, true> {

			// we are not using the data, so just free it.
			void addOrFree(void* ptr, size_t ROBIN_HOOD_UNUSED(numBytes) /*unused*/) noexcept {
				ROBIN_HOOD_LOG("std::free")
				gaia::mem::mem_free(ptr);
			}
		};

		template <typename T, size_t MinSize, size_t MaxSize>
		struct NodeAllocator<T, MinSize, MaxSize, false>: public BulkPoolAllocator<T, MinSize, MaxSize> {};

		namespace swappable {
			template <typename T>
			struct nothrow {
				static const bool value = std::is_nothrow_swappable<T>::value;
			};
		} // namespace swappable

	} // namespace detail

	struct is_transparent_tag {};

	// A custom pair implementation is used in the map because std::pair is not is_trivially_copyable,
	// which means it would  not be allowed to be used in std::memcpy. This struct is copyable, which is
	// also tested.
	template <typename T1, typename T2>
	struct pair {
		using first_type = T1;
		using second_type = T2;

		template <
				typename U1 = T1, typename U2 = T2,
				typename = typename std::enable_if<
						std::is_default_constructible<U1>::value && std::is_default_constructible<U2>::value>::type>
		constexpr pair() noexcept(noexcept(U1()) && noexcept(U2())): first(), second() {}

		// pair constructors are explicit so we don't accidentally call this ctor when we don't have to.
		explicit constexpr pair(std::pair<T1, T2> const& o) noexcept(
				noexcept(T1(std::declval<T1 const&>())) && noexcept(T2(std::declval<T2 const&>()))):
				first(o.first),
				second(o.second) {}

		// pair constructors are explicit so we don't accidentally call this ctor when we don't have to.
		explicit constexpr pair(std::pair<T1, T2>&& o) noexcept(
				noexcept(T1(std::move(std::declval<T1&&>()))) && noexcept(T2(std::move(std::declval<T2&&>())))):
				first(std::move(o.first)),
				second(std::move(o.second)) {}

		constexpr pair(T1&& a, T2&& b) noexcept(
				noexcept(T1(std::move(std::declval<T1&&>()))) && noexcept(T2(std::move(std::declval<T2&&>())))):
				first(std::move(a)),
				second(std::move(b)) {}

		template <typename U1, typename U2>
		constexpr pair(U1&& a, U2&& b) noexcept(
				noexcept(T1(std::forward<U1>(std::declval<U1&&>()))) && noexcept(T2(std::forward<U2>(std::declval<U2&&>())))):
				first(std::forward<U1>(a)),
				second(std::forward<U2>(b)) {}

		template <typename... U1, typename... U2>
		// MSVC 2015 produces error "C2476: ‘constexpr’ constructor does not initialize all members"
		// if this constructor is constexpr
#if !ROBIN_HOOD(BROKEN_CONSTEXPR)
		constexpr
#endif
				pair(std::piecewise_construct_t /*unused*/, std::tuple<U1...> a, std::tuple<U2...> b) noexcept(noexcept(pair(
						std::declval<std::tuple<U1...>&>(), std::declval<std::tuple<U2...>&>(), std::index_sequence_for<U1...>(),
						std::index_sequence_for<U2...>()))):
				pair(a, b, std::index_sequence_for<U1...>(), std::index_sequence_for<U2...>()) {
		}

		// constructor called from the std::piecewise_construct_t ctor
		template <typename... U1, size_t... I1, typename... U2, size_t... I2>
		pair(
				std::tuple<U1...>& a, std::tuple<U2...>& b, std::index_sequence<I1...> /*unused*/,
				std::index_sequence<
						I2...> /*unused*/) noexcept(noexcept(T1(std::
																												forward<U1>(std::get<I1>(
																														std::declval<std::tuple<
																																U1...>&>()))...)) && noexcept(T2(std::
																																																		 forward<
																																																				 U2>(std::get<
																																																						 I2>(
																																																				 std::declval<std::tuple<
																																																						 U2...>&>()))...))):
				first(std::forward<U1>(std::get<I1>(a))...),
				second(std::forward<U2>(std::get<I2>(b))...) {
			// make visual studio compiler happy about warning about unused a & b.
			// Visual studio's pair implementation disables warning 4100.
			(void)a;
			(void)b;
		}

		void
		swap(pair<T1, T2>& o) noexcept((detail::swappable::nothrow<T1>::value) && (detail::swappable::nothrow<T2>::value)) {
			using std::swap;
			swap(first, o.first);
			swap(second, o.second);
		}

		T1 first; // NOLINT(misc-non-private-member-variables-in-classes)
		T2 second; // NOLINT(misc-non-private-member-variables-in-classes)
	};

	template <typename A, typename B>
	inline void
	swap(pair<A, B>& a, pair<A, B>& b) noexcept(noexcept(std::declval<pair<A, B>&>().swap(std::declval<pair<A, B>&>()))) {
		a.swap(b);
	}

	template <typename A, typename B>
	inline constexpr bool operator==(pair<A, B> const& x, pair<A, B> const& y) {
		return (x.first == y.first) && (x.second == y.second);
	}
	template <typename A, typename B>
	inline constexpr bool operator!=(pair<A, B> const& x, pair<A, B> const& y) {
		return !(x == y);
	}
	template <typename A, typename B>
	inline constexpr bool operator<(pair<A, B> const& x, pair<A, B> const& y) noexcept(noexcept(
			std::declval<A const&>() <
			std::declval<A const&>()) && noexcept(std::declval<B const&>() < std::declval<B const&>())) {
		return x.first < y.first || (!(y.first < x.first) && x.second < y.second);
	}
	template <typename A, typename B>
	inline constexpr bool operator>(pair<A, B> const& x, pair<A, B> const& y) {
		return y < x;
	}
	template <typename A, typename B>
	inline constexpr bool operator<=(pair<A, B> const& x, pair<A, B> const& y) {
		return !(x > y);
	}
	template <typename A, typename B>
	inline constexpr bool operator>=(pair<A, B> const& x, pair<A, B> const& y) {
		return !(x < y);
	}

	inline size_t hash_bytes(void const* ptr, size_t len) noexcept {
		static constexpr uint64_t m = UINT64_C(0xc6a4a7935bd1e995);
		static constexpr uint64_t seed = UINT64_C(0xe17a1465);
		static constexpr unsigned int r = 47;

		auto const* const data64 = static_cast<uint64_t const*>(ptr);
		uint64_t h = seed ^ (len * m);

		size_t const n_blocks = len / 8;
		for (size_t i = 0; i < n_blocks; ++i) {
			auto k = detail::unaligned_load<uint64_t>(data64 + i);

			k *= m;
			k ^= k >> r;
			k *= m;

			h ^= k;
			h *= m;
		}

		auto const* const data8 = reinterpret_cast<uint8_t const*>(data64 + n_blocks);
		switch (len & 7U) {
			case 7:
				h ^= static_cast<uint64_t>(data8[6]) << 48U;
				ROBIN_HOOD(FALLTHROUGH); // FALLTHROUGH
			case 6:
				h ^= static_cast<uint64_t>(data8[5]) << 40U;
				ROBIN_HOOD(FALLTHROUGH); // FALLTHROUGH
			case 5:
				h ^= static_cast<uint64_t>(data8[4]) << 32U;
				ROBIN_HOOD(FALLTHROUGH); // FALLTHROUGH
			case 4:
				h ^= static_cast<uint64_t>(data8[3]) << 24U;
				ROBIN_HOOD(FALLTHROUGH); // FALLTHROUGH
			case 3:
				h ^= static_cast<uint64_t>(data8[2]) << 16U;
				ROBIN_HOOD(FALLTHROUGH); // FALLTHROUGH
			case 2:
				h ^= static_cast<uint64_t>(data8[1]) << 8U;
				ROBIN_HOOD(FALLTHROUGH); // FALLTHROUGH
			case 1:
				h ^= static_cast<uint64_t>(data8[0]);
				h *= m;
				ROBIN_HOOD(FALLTHROUGH); // FALLTHROUGH
			default:
				break;
		}

		h ^= h >> r;

		// not doing the final step here, because this will be done by keyToIdx anyways
		// h *= m;
		// h ^= h >> r;
		return static_cast<size_t>(h);
	}

	inline size_t hash_int(uint64_t x) noexcept {
		// tried lots of different hashes, let's stick with murmurhash3. It's simple, fast, well tested,
		// and doesn't need any special 128bit operations.
		x ^= x >> 33U;
		x *= UINT64_C(0xff51afd7ed558ccd);
		x ^= x >> 33U;

		// not doing the final step here, because this will be done by keyToIdx anyways
		// x *= UINT64_C(0xc4ceb9fe1a85ec53);
		// x ^= x >> 33U;
		return static_cast<size_t>(x);
	}

	template <typename T, typename Enable = void>
	struct hash {
		size_t operator()(T const& obj) const noexcept {
			return hash_bytes(&obj, sizeof(T));
		}
	};

	template <typename T>
	struct hash<T*> {
		size_t operator()(T* ptr) const noexcept {
			return hash_int(reinterpret_cast<detail::SizeT>(ptr));
		}
	};

#ifdef ROBIN_HOOD_STD_SMARTPOINTERS
	template <typename T>
	struct hash<std::unique_ptr<T>> {
		size_t operator()(std::unique_ptr<T> const& ptr) const noexcept {
			return hash_int(reinterpret_cast<detail::SizeT>(ptr.get()));
		}
	};

	template <typename T>
	struct hash<std::shared_ptr<T>> {
		size_t operator()(std::shared_ptr<T> const& ptr) const noexcept {
			return hash_int(reinterpret_cast<detail::SizeT>(ptr.get()));
		}
	};
#endif

	template <typename Enum>
	struct hash<Enum, typename std::enable_if<std::is_enum<Enum>::value>::type> {
		size_t operator()(Enum e) const noexcept {
			using Underlying = typename std::underlying_type<Enum>::type;
			return hash<Underlying>{}(static_cast<Underlying>(e));
		}
	};

	template <typename T>
	struct hash<T, typename std::enable_if<gaia::core::is_direct_hash_key_v<T>>::type> {
		size_t operator()(const T& obj) const noexcept {
			if constexpr (detail::has_hash<T>::value)
				return obj.hash;
			else
				return obj.hash();
		}
	};

#define ROBIN_HOOD_HASH_INT(T)                                                                                         \
	template <>                                                                                                          \
	struct hash<T> {                                                                                                     \
		size_t operator()(T const& obj) const noexcept {                                                                   \
			return hash_int(static_cast<uint64_t>(obj));                                                                     \
		}                                                                                                                  \
	}

#if defined(__GNUC__) && !defined(__clang__)
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wuseless-cast"
#endif
	// see https://en.cppreference.com/w/cpp/utility/hash
	ROBIN_HOOD_HASH_INT(bool);
	ROBIN_HOOD_HASH_INT(char);
	ROBIN_HOOD_HASH_INT(signed char);
	ROBIN_HOOD_HASH_INT(unsigned char);
	ROBIN_HOOD_HASH_INT(char16_t);
	ROBIN_HOOD_HASH_INT(char32_t);
#if ROBIN_HOOD(HAS_NATIVE_WCHART)
	ROBIN_HOOD_HASH_INT(wchar_t);
#endif
	ROBIN_HOOD_HASH_INT(short);
	ROBIN_HOOD_HASH_INT(unsigned short);
	ROBIN_HOOD_HASH_INT(int);
	ROBIN_HOOD_HASH_INT(unsigned int);
	ROBIN_HOOD_HASH_INT(long);
	ROBIN_HOOD_HASH_INT(long long);
	ROBIN_HOOD_HASH_INT(unsigned long);
	ROBIN_HOOD_HASH_INT(unsigned long long);
#if defined(__GNUC__) && !defined(__clang__)
	#pragma GCC diagnostic pop
#endif
	namespace detail {
		template <typename, typename = void>
		struct has_is_transparent: public std::false_type {};

		template <typename T>
		struct has_is_transparent<T, std::void_t<decltype(T::is_transparent)>>: public std::true_type {};

		// using wrapper classes for hash and key_equal prevents the diamond problem when the same type
		// is used. see https://stackoverflow.com/a/28771920/48181
		template <typename T>
		struct WrapHash: public T {
			WrapHash() = default;
			explicit WrapHash(T const& o) noexcept(noexcept(T(std::declval<T const&>()))): T(o) {}
		};

		template <typename T>
		struct WrapKeyEqual: public T {
			WrapKeyEqual() = default;
			explicit WrapKeyEqual(T const& o) noexcept(noexcept(T(std::declval<T const&>()))): T(o) {}
		};

		// A highly optimized hashmap implementation, using the Robin Hood algorithm.
		//
		// In most cases, this map should be usable as a drop-in replacement for std::unordered_map, but
		// be about 2x faster in most cases and require much less allocations.
		//
		// This implementation uses the following memory layout:
		//
		// [Node, Node, ... Node | info, info, ... infoSentinel ]
		//
		// * Node: either a DataNode that directly has the std::pair<key, val> as member,
		//   or a DataNode with a pointer to std::pair<key,val>. Which DataNode representation to use
		//   depends on how fast the swap() operation is. Heuristically, this is automatically choosen
		//   based on sizeof(). there are always 2^n Nodes.
		//
		// * info: Each Node in the map has a corresponding info byte, so there are 2^n info bytes.
		//   Each byte is initialized to 0, meaning the corresponding Node is empty. Set to 1 means the
		//   corresponding node contains data. Set to 2 means the corresponding Node is filled, but it
		//   actually belongs to the previous position and was pushed out because that place is already
		//   taken.
		//
		// * infoSentinel: Sentinel byte set to 1, so that iterator's ++ can stop at end() without the
		//   need for a idx variable.
		//
		// According to STL, order of templates has effect on throughput. That's why I've moved the
		// boolean to the front.
		// https://www.reddit.com/r/cpp/comments/ahp6iu/compile_time_binary_size_reductions_and_cs_future/eeguck4/
		template <bool IsFlat, size_t MaxLoadFactor100, typename Key, typename T, typename Hash, typename KeyEqual>
		class Table:
				public WrapHash<Hash>,
				public WrapKeyEqual<KeyEqual>,
				detail::NodeAllocator<
						typename std::conditional<
								std::is_void<T>::value, Key,
								robin_hood::pair<typename std::conditional<IsFlat, Key, Key const>::type, T>>::type,
						4, 16384, IsFlat> {
		public:
			static constexpr bool is_flat = IsFlat;
			static constexpr bool is_map = !std::is_void<T>::value;
			static constexpr bool is_set = !is_map;
			static constexpr bool is_transparent = has_is_transparent<Hash>::value && has_is_transparent<KeyEqual>::value;

			using key_type = Key;
			using mapped_type = T;
			using value_type = typename std::conditional<
					is_set, Key, robin_hood::pair<typename std::conditional<is_flat, Key, Key const>::type, T>>::type;
			using size_type = size_t;
			using hasher = Hash;
			using key_equal = KeyEqual;
			using Self = Table<IsFlat, MaxLoadFactor100, key_type, mapped_type, hasher, key_equal>;

		private:
			static_assert(MaxLoadFactor100 > 10 && MaxLoadFactor100 < 100, "MaxLoadFactor100 needs to be >10 && < 100");

			using WHash = WrapHash<Hash>;
			using WKeyEqual = WrapKeyEqual<KeyEqual>;

			// configuration defaults

			// make sure we have 8 elements, needed to quickly rehash mInfo
			static constexpr size_t InitialNumElements = sizeof(uint64_t);
			static constexpr uint32_t InitialInfoNumBits = 5;
			static constexpr uint8_t InitialInfoInc = 1U << InitialInfoNumBits;
			static constexpr size_t InfoMask = InitialInfoInc - 1U;
			static constexpr uint8_t InitialInfoHashShift = 64U - InitialInfoNumBits;
			using DataPool = detail::NodeAllocator<value_type, 4, 16384, IsFlat>;

			// type needs to be wider than uint8_t.
			using InfoType = uint32_t;

			// DataNode ////////////////////////////////////////////////////////

			// Primary template for the data node. We have special implementations for small and big
			// objects. For large objects it is assumed that swap() is fairly slow, so we allocate these
			// on the heap so swap merely swaps a pointer.
			template <typename M, bool>
			class DataNode {};

			// Small: just allocate on the stack.
			template <typename M>
			class DataNode<M, true> final {
			public:
				template <typename... Args>
				explicit DataNode(M& ROBIN_HOOD_UNUSED(map) /*unused*/, Args&&... args) noexcept(
						noexcept(value_type(std::forward<Args>(args)...))):
						mData(std::forward<Args>(args)...) {}

				DataNode(M& ROBIN_HOOD_UNUSED(map) /*unused*/, DataNode<M, true>&& n) noexcept(
						std::is_nothrow_move_constructible<value_type>::value):
						mData(std::move(n.mData)) {}

				// doesn't do anything
				void destroy(M& ROBIN_HOOD_UNUSED(map) /*unused*/) noexcept {}
				void destroyDoNotDeallocate() noexcept {}

				value_type const* operator->() const noexcept {
					return &mData;
				}
				value_type* operator->() noexcept {
					return &mData;
				}

				const value_type& operator*() const noexcept {
					return mData;
				}

				value_type& operator*() noexcept {
					return mData;
				}

				template <typename VT = value_type>
				GAIA_NODISCARD typename std::enable_if<is_map, typename VT::first_type&>::type getFirst() noexcept {
					return mData.first;
				}
				template <typename VT = value_type>
				GAIA_NODISCARD typename std::enable_if<is_set, VT&>::type getFirst() noexcept {
					return mData;
				}

				template <typename VT = value_type>
				GAIA_NODISCARD typename std::enable_if<is_map, typename VT::first_type const&>::type getFirst() const noexcept {
					return mData.first;
				}
				template <typename VT = value_type>
				GAIA_NODISCARD typename std::enable_if<is_set, VT const&>::type getFirst() const noexcept {
					return mData;
				}

				template <typename MT = mapped_type>
				GAIA_NODISCARD typename std::enable_if<is_map, MT&>::type getSecond() noexcept {
					return mData.second;
				}

				template <typename MT = mapped_type>
				GAIA_NODISCARD typename std::enable_if<is_set, MT const&>::type getSecond() const noexcept {
					return mData.second;
				}

				void
				swap(DataNode<M, true>& o) noexcept(noexcept(std::declval<value_type>().swap(std::declval<value_type>()))) {
					mData.swap(o.mData);
				}

			private:
				value_type mData;
			};

			// big object: allocate on heap.
			template <typename M>
			class DataNode<M, false> {
			public:
				template <typename... Args>
				explicit DataNode(M& map, Args&&... args): mData(map.allocate()) {
					::new (static_cast<void*>(mData)) value_type(std::forward<Args>(args)...);
				}

				DataNode(M& ROBIN_HOOD_UNUSED(map) /*unused*/, DataNode<M, false>&& n) noexcept: mData(std::move(n.mData)) {}

				void destroy(M& map) noexcept {
					// don't deallocate, just put it into list of datapool.
					mData->~value_type();
					map.deallocate(mData);
				}

				void destroyDoNotDeallocate() noexcept {
					mData->~value_type();
				}

				value_type const* operator->() const noexcept {
					return mData;
				}

				value_type* operator->() noexcept {
					return mData;
				}

				const value_type& operator*() const {
					return *mData;
				}

				value_type& operator*() {
					return *mData;
				}

				template <typename VT = value_type>
				GAIA_NODISCARD typename std::enable_if<is_map, typename VT::first_type&>::type getFirst() noexcept {
					return mData->first;
				}
				template <typename VT = value_type>
				GAIA_NODISCARD typename std::enable_if<is_set, VT&>::type getFirst() noexcept {
					return *mData;
				}

				template <typename VT = value_type>
				GAIA_NODISCARD typename std::enable_if<is_map, typename VT::first_type const&>::type getFirst() const noexcept {
					return mData->first;
				}
				template <typename VT = value_type>
				GAIA_NODISCARD typename std::enable_if<is_set, VT const&>::type getFirst() const noexcept {
					return *mData;
				}

				template <typename MT = mapped_type>
				GAIA_NODISCARD typename std::enable_if<is_map, MT&>::type getSecond() noexcept {
					return mData->second;
				}

				template <typename MT = mapped_type>
				GAIA_NODISCARD typename std::enable_if<is_map, MT const&>::type getSecond() const noexcept {
					return mData->second;
				}

				void swap(DataNode<M, false>& o) noexcept {
					using std::swap;
					swap(mData, o.mData);
				}

			private:
				value_type* mData;
			};

			using Node = DataNode<Self, IsFlat>;

			// helpers for insertKeyPrepareEmptySpot: extract first entry (only const required)
			GAIA_NODISCARD key_type const& getFirstConst(Node const& n) const noexcept {
				return n.getFirst();
			}

			// in case we have void mapped_type, we are not using a pair, thus we just route k through.
			// No need to disable this because it's just not used if not applicable.
			GAIA_NODISCARD key_type const& getFirstConst(key_type const& k) const noexcept {
				return k;
			}

			// in case we have non-void mapped_type, we have a standard robin_hood::pair
			template <typename Q = mapped_type>
			GAIA_NODISCARD typename std::enable_if<!std::is_void<Q>::value, key_type const&>::type
			getFirstConst(value_type const& vt) const noexcept {
				return vt.first;
			}

			// Cloner //////////////////////////////////////////////////////////

			template <typename M, bool UseMemcpy>
			struct Cloner;

			// fast path: Just copy data, without allocating anything.
			template <typename M>
			struct Cloner<M, true> {
				void operator()(M const& source, M& target) const {
					auto const* const src = reinterpret_cast<char const*>(source.mKeyVals);
					auto* tgt = reinterpret_cast<char*>(target.mKeyVals);
					auto const numElementsWithBuffer = target.calcNumElementsWithBuffer(target.mMask + 1);
					memcpy(tgt, src, src + target.calcNumBytesTotal(numElementsWithBuffer));
				}
			};

			template <typename M>
			struct Cloner<M, false> {
				void operator()(M const& s, M& t) const {
					auto const numElementsWithBuffer = t.calcNumElementsWithBuffer(t.mMask + 1);
					memcpy(t.mInfo, s.mInfo, s.mInfo + t.calcNumBytesInfo(numElementsWithBuffer));

					for (size_t i = 0; i < numElementsWithBuffer; ++i) {
						if (t.mInfo[i]) {
							::new (static_cast<void*>(t.mKeyVals + i)) Node(t, *s.mKeyVals[i]);
						}
					}
				}
			};

			// Destroyer ///////////////////////////////////////////////////////

			template <typename M, bool IsFlatAndTrivial>
			struct Destroyer {};

			template <typename M>
			struct Destroyer<M, true> {
				void nodes(M& m) const noexcept {
					m.mNumElements = 0;
				}

				void nodesDoNotDeallocate(M& m) const noexcept {
					m.mNumElements = 0;
				}
			};

			template <typename M>
			struct Destroyer<M, false> {
				void nodes(M& m) const noexcept {
					m.mNumElements = 0;
					// clear also resets mInfo to 0, that's sometimes not necessary.
					auto const numElementsWithBuffer = m.calcNumElementsWithBuffer(m.mMask + 1);

					for (size_t idx = 0; idx < numElementsWithBuffer; ++idx) {
						if (0 != m.mInfo[idx]) {
							Node& n = m.mKeyVals[idx];
							n.destroy(m);
							n.~Node();
						}
					}
				}

				void nodesDoNotDeallocate(M& m) const noexcept {
					m.mNumElements = 0;
					// clear also resets mInfo to 0, that's sometimes not necessary.
					auto const numElementsWithBuffer = m.calcNumElementsWithBuffer(m.mMask + 1);
					for (size_t idx = 0; idx < numElementsWithBuffer; ++idx) {
						if (0 != m.mInfo[idx]) {
							Node& n = m.mKeyVals[idx];
							n.destroyDoNotDeallocate();
							n.~Node();
						}
					}
				}
			};

			// Iter ////////////////////////////////////////////////////////////

			struct fast_forward_tag {};

			// generic iterator for both const_iterator and iterator.
			template <bool IsConst>
			// NOLINTNEXTLINE(hicpp-special-member-functions,cppcoreguidelines-special-member-functions)
			class Iter {
			private:
				using NodePtr = typename std::conditional<IsConst, Node const*, Node*>::type;

			public:
				using difference_type = std::ptrdiff_t;
				using value_type = typename Self::value_type;
				using reference = typename std::conditional<IsConst, value_type const&, value_type&>::type;
				using pointer = typename std::conditional<IsConst, value_type const*, value_type*>::type;
				using iterator_category = gaia::core::forward_iterator_tag;

				// default constructed iterator can be compared to itself, but WON'T return true when
				// compared to end().
				Iter() = default;

				// Rule of zero: nothing specified. The conversion constructor is only enabled for
				// iterator to const_iterator, so it doesn't accidentally work as a copy ctor.

				// Conversion constructor from iterator to const_iterator.
				template <bool OtherIsConst, typename = typename std::enable_if<IsConst && !OtherIsConst>::type>
				// NOLINTNEXTLINE(hicpp-explicit-conversions)
				Iter(Iter<OtherIsConst> const& other) noexcept: mKeyVals(other.mKeyVals), mInfo(other.mInfo) {}

				Iter(NodePtr valPtr, uint8_t const* infoPtr) noexcept: mKeyVals(valPtr), mInfo(infoPtr) {}

				Iter(NodePtr valPtr, uint8_t const* infoPtr, fast_forward_tag ROBIN_HOOD_UNUSED(tag) /*unused*/) noexcept:
						mKeyVals(valPtr), mInfo(infoPtr) {
					fastForward();
				}

				template <bool OtherIsConst, typename = typename std::enable_if<IsConst && !OtherIsConst>::type>
				Iter& operator=(Iter<OtherIsConst> const& other) noexcept {
					mKeyVals = other.mKeyVals;
					mInfo = other.mInfo;
					return *this;
				}

				// prefix increment. Undefined behavior if we are at end()!
				Iter& operator++() noexcept {
					++mInfo;
					++mKeyVals;
					fastForward();
					return *this;
				}

				Iter operator++(int) noexcept {
					Iter tmp = *this;
					++(*this);
					return tmp;
				}

				reference operator*() const {
					return **mKeyVals;
				}

				pointer operator->() const {
					return &**mKeyVals;
				}

				template <bool O>
				bool operator==(Iter<O> const& o) const noexcept {
					return mKeyVals == o.mKeyVals;
				}

				template <bool O>
				bool operator!=(Iter<O> const& o) const noexcept {
					return mKeyVals != o.mKeyVals;
				}

			private:
				// fast forward to the next non-free info byte
				// I've tried a few variants that don't depend on intrinsics, but unfortunately they are
				// quite a bit slower than this one. So I've reverted that change again. See map_benchmark.
				void fastForward() noexcept {
					size_t n = 0;
					while (0U == (n = detail::unaligned_load<size_t>(mInfo))) {
						GAIA_MSVC_WARNING_PUSH()
						GAIA_MSVC_WARNING_DISABLE(6305)
						mInfo += sizeof(size_t);
						mKeyVals += sizeof(size_t);
						GAIA_MSVC_WARNING_POP()
					}
#if defined(ROBIN_HOOD_DISABLE_INTRINSICS)
					// we know for certain that within the next 8 bytes we'll find a non-zero one.
					if GAIA_UNLIKELY (0U == detail::unaligned_load<uint32_t>(mInfo)) {
						mInfo += 4;
						mKeyVals += 4;
					}
					if GAIA_UNLIKELY (0U == detail::unaligned_load<uint16_t>(mInfo)) {
						mInfo += 2;
						mKeyVals += 2;
					}
					if GAIA_UNLIKELY (0U == *mInfo) {
						mInfo += 1;
						mKeyVals += 1;
					}
#else
	#if GAIA_LITTLE_ENDIAN
					auto inc = ROBIN_HOOD_COUNT_TRAILING_ZEROES(n) / 8;
	#else
					auto inc = ROBIN_HOOD_COUNT_LEADING_ZEROES(n) / 8;
	#endif
					mInfo += inc;
					mKeyVals += inc;
#endif
				}

				friend class Table<IsFlat, MaxLoadFactor100, key_type, mapped_type, hasher, key_equal>;
				NodePtr mKeyVals{nullptr};
				uint8_t const* mInfo{nullptr};
			};

			////////////////////////////////////////////////////////////////////

			// highly performance relevant code.
			// Lower bits are used for indexing into the array (2^n size)
			// The upper 1-5 bits need to be a reasonable good hash, to save comparisons.
			template <typename HashKey>
			void keyToIdx(HashKey&& key, size_t* idx, InfoType* info) const {
				auto h = static_cast<uint64_t>(WHash::operator()(key));

				// direct_hash_key is expected to be a proper hash. No additional hash tricks are required
				using HashKeyRaw = std::decay_t<HashKey>;
				if constexpr (!gaia::core::is_direct_hash_key_v<HashKeyRaw>) {
					// In addition to whatever hash is used, add another mul & shift so we get better hashing.
					// This serves as a bad hash prevention, if the given data is
					// badly mixed.
					h *= mHashMultiplier;
					h ^= h >> 33U;
				}

				// the lower InitialInfoNumBits are reserved for info.
				*info = mInfoInc + static_cast<InfoType>(h >> mInfoHashShift);
				*idx = (static_cast<size_t>(h)) & mMask;
			}

			// forwards the index by one, wrapping around at the end
			void next(InfoType* info, size_t* idx) const noexcept {
				*idx = *idx + 1;
				*info += mInfoInc;
			}

			void nextWhileLess(InfoType* info, size_t* idx) const noexcept {
				// unrolling this by hand did not bring any speedups.
				while (*info < mInfo[*idx]) {
					next(info, idx);
				}
			}

			// Shift everything up by one element. Tries to move stuff around.
			void shiftUp(size_t startIdx, size_t const insertion_idx) noexcept(std::is_nothrow_move_assignable<Node>::value) {
				auto idx = startIdx;
				::new (static_cast<void*>(mKeyVals + idx)) Node(std::move(mKeyVals[idx - 1]));
				while (--idx != insertion_idx) {
					mKeyVals[idx] = std::move(mKeyVals[idx - 1]);
				}

				idx = startIdx;
				while (idx != insertion_idx) {
					ROBIN_HOOD_COUNT(shiftUp)
					mInfo[idx] = static_cast<uint8_t>(mInfo[idx - 1] + mInfoInc);
					if GAIA_UNLIKELY (mInfo[idx] + mInfoInc > 0xFF) {
						mMaxNumElementsAllowed = 0;
					}
					--idx;
				}
			}

			void shiftDown(size_t idx) noexcept(std::is_nothrow_move_assignable<Node>::value) {
				// until we find one that is either empty or has zero offset.
				// TODO(martinus) we don't need to move everything, just the last one for the same
				// bucket.
				mKeyVals[idx].destroy(*this);

				// until we find one that is either empty or has zero offset.
				while (mInfo[idx + 1] >= 2 * mInfoInc) {
					ROBIN_HOOD_COUNT(shiftDown)
					mInfo[idx] = static_cast<uint8_t>(mInfo[idx + 1] - mInfoInc);
					mKeyVals[idx] = std::move(mKeyVals[idx + 1]);
					++idx;
				}

				mInfo[idx] = 0;
				// don't destroy, we've moved it
				// mKeyVals[idx].destroy(*this);
				mKeyVals[idx].~Node();
			}

			// copy of find(), except that it returns iterator instead of const_iterator.
			template <typename Other>
			GAIA_NODISCARD size_t findIdx(Other const& key) const {
				size_t idx{};
				InfoType info{};
				keyToIdx(key, &idx, &info);

				do {
					// unrolling this twice gives a bit of a speedup. More unrolling did not help.
					if (info == mInfo[idx]) {
						if GAIA_LIKELY (WKeyEqual::operator()(key, mKeyVals[idx].getFirst())) {
							return idx;
						}
					}
					next(&info, &idx);
					if (info == mInfo[idx]) {
						if GAIA_LIKELY (WKeyEqual::operator()(key, mKeyVals[idx].getFirst())) {
							return idx;
						}
					}
					next(&info, &idx);
				} while (info <= mInfo[idx]);

				// nothing found!
				return mMask == 0 ? 0
													: static_cast<size_t>(
																gaia::core::distance(mKeyVals, reinterpret_cast_no_cast_align_warning<Node*>(mInfo)));
			}

			void cloneData(const Table& o) {
				Cloner<Table, IsFlat && ROBIN_HOOD_IS_TRIVIALLY_COPYABLE(Node)>()(o, *this);
			}

			// inserts a keyval that is guaranteed to be new, e.g. when the hashmap is resized.
			// @return True on success, false if something went wrong
			void insert_move(Node&& keyval) {
				// we don't retry, fail if overflowing
				// don't need to check max num elements
				if (0 == mMaxNumElementsAllowed && !try_increase_info()) {
					throwOverflowError();
				}

				size_t idx{};
				InfoType info{};
				keyToIdx(keyval.getFirst(), &idx, &info);

				// skip forward. Use <= because we are certain that the element is not there.
				while (info <= mInfo[idx]) {
					idx = idx + 1;
					info += mInfoInc;
				}

				// key not found, so we are now exactly where we want to insert it.
				auto const insertion_idx = idx;
				auto const insertion_info = static_cast<uint8_t>(info);
				if GAIA_UNLIKELY (insertion_info + mInfoInc > 0xFF) {
					mMaxNumElementsAllowed = 0;
				}

				// find an empty spot
				while (0 != mInfo[idx]) {
					next(&info, &idx);
				}

				auto& l = mKeyVals[insertion_idx];
				if (idx == insertion_idx) {
					::new (static_cast<void*>(&l)) Node(std::move(keyval));
				} else {
					shiftUp(idx, insertion_idx);
					l = std::move(keyval);
				}

				// put at empty spot
				mInfo[insertion_idx] = insertion_info;

				++mNumElements;
			}

		public:
			using iterator = Iter<false>;
			using const_iterator = Iter<true>;

			Table() noexcept(noexcept(Hash()) && noexcept(KeyEqual())): WHash(), WKeyEqual() {
				ROBIN_HOOD_TRACE(this)
				GAIA_ASSERT(gaia::CheckEndianess());
			}

			// Creates an empty hash map. Nothing is allocated yet, this happens at the first insert.
			// This tremendously speeds up ctor & dtor of a map that never receives an element. The
			// penalty is payed at the first insert, and not before. Lookup of this empty map works
			// because everybody points to DummyInfoByte::b. parameter bucket_count is dictated by the
			// standard, but we can ignore it.
			explicit Table(
					size_t ROBIN_HOOD_UNUSED(bucket_count) /*unused*/, const Hash& h = Hash{},
					const KeyEqual& equal = KeyEqual{}) noexcept(noexcept(Hash(h)) && noexcept(KeyEqual(equal))):
					WHash(h),
					WKeyEqual(equal) {
				ROBIN_HOOD_TRACE(this);
				GAIA_ASSERT(gaia::CheckEndianess());
			}

			template <typename Iter>
			Table(
					Iter first, Iter last, size_t ROBIN_HOOD_UNUSED(bucket_count) /*unused*/ = 0, const Hash& h = Hash{},
					const KeyEqual& equal = KeyEqual{}):
					WHash(h),
					WKeyEqual(equal) {
				ROBIN_HOOD_TRACE(this);
				GAIA_ASSERT(gaia::CheckEndianess());
				insert(first, last);
			}

			Table(
					std::initializer_list<value_type> initlist, size_t ROBIN_HOOD_UNUSED(bucket_count) /*unused*/ = 0,
					const Hash& h = Hash{}, const KeyEqual& equal = KeyEqual{}):
					WHash(h),
					WKeyEqual(equal) {
				ROBIN_HOOD_TRACE(this);
				GAIA_ASSERT(gaia::CheckEndianess());
				insert(initlist.begin(), initlist.end());
			}

			Table(Table&& o) noexcept:
					WHash(std::move(static_cast<WHash&>(o))), WKeyEqual(std::move(static_cast<WKeyEqual&>(o))),
					DataPool(std::move(static_cast<DataPool&>(o))) {
				ROBIN_HOOD_TRACE(this)
				if (o.mMask) {
					mHashMultiplier = std::move(o.mHashMultiplier);
					mKeyVals = std::move(o.mKeyVals);
					mInfo = std::move(o.mInfo);
					mNumElements = std::move(o.mNumElements);
					mMask = std::move(o.mMask);
					mMaxNumElementsAllowed = std::move(o.mMaxNumElementsAllowed);
					mInfoInc = std::move(o.mInfoInc);
					mInfoHashShift = std::move(o.mInfoHashShift);
					// set other's mask to 0 so its destructor won't do anything
					o.init();
				}
			}

			Table& operator=(Table&& o) noexcept {
				ROBIN_HOOD_TRACE(this)
				if (&o != this) {
					if (o.mMask) {
						// only move stuff if the other map actually has some data
						destroy();
						mHashMultiplier = std::move(o.mHashMultiplier);
						mKeyVals = std::move(o.mKeyVals);
						mInfo = std::move(o.mInfo);
						mNumElements = std::move(o.mNumElements);
						mMask = std::move(o.mMask);
						mMaxNumElementsAllowed = std::move(o.mMaxNumElementsAllowed);
						mInfoInc = std::move(o.mInfoInc);
						mInfoHashShift = std::move(o.mInfoHashShift);
						WHash::operator=(std::move(static_cast<WHash&>(o)));
						WKeyEqual::operator=(std::move(static_cast<WKeyEqual&>(o)));
						DataPool::operator=(std::move(static_cast<DataPool&>(o)));

						o.init();

					} else {
						// nothing in the other map => just clear us.
						clear();
					}
				}
				return *this;
			}

			Table(const Table& o):
					WHash(static_cast<const WHash&>(o)), WKeyEqual(static_cast<const WKeyEqual&>(o)),
					DataPool(static_cast<const DataPool&>(o)) {
				ROBIN_HOOD_TRACE(this)
				if (!o.empty()) {
					// not empty: create an exact copy. it is also possible to just iterate through all
					// elements and insert them, but copying is probably faster.

					auto const numElementsWithBuffer = calcNumElementsWithBuffer(o.mMask + 1);
					auto const numBytesTotal = calcNumBytesTotal(numElementsWithBuffer);

					ROBIN_HOOD_LOG(
							"gaia::mem::mem_alloc " << numBytesTotal << " = calcNumBytesTotal(" << numElementsWithBuffer << ")")
					mHashMultiplier = o.mHashMultiplier;
					mKeyVals = static_cast<Node*>(detail::assertNotNull<std::bad_alloc>(gaia::mem::mem_alloc(numBytesTotal)));
					// no need for calloc because clonData does memcpy
					mInfo = reinterpret_cast<uint8_t*>(mKeyVals + numElementsWithBuffer);
					mNumElements = o.mNumElements;
					mMask = o.mMask;
					mMaxNumElementsAllowed = o.mMaxNumElementsAllowed;
					mInfoInc = o.mInfoInc;
					mInfoHashShift = o.mInfoHashShift;
					cloneData(o);
				}
			}

			// Creates a copy of the given map. Copy constructor of each entry is used.
			// Not sure why clang-tidy thinks this doesn't handle self assignment, it does
			// NOLINTNEXTLINE(bugprone-unhandled-self-assignment,cert-oop54-cpp)
			Table& operator=(Table const& o) {
				ROBIN_HOOD_TRACE(this)
				if (&o == this) {
					// prevent assigning of itself
					return *this;
				}

				// we keep using the old allocator and not assign the new one, because we want to keep
				// the memory available. when it is the same size.
				if (o.empty()) {
					if (0 == mMask) {
						// nothing to do, we are empty too
						return *this;
					}

					// not empty: destroy what we have there
					// clear also resets mInfo to 0, that's sometimes not necessary.
					destroy();
					init();
					WHash::operator=(static_cast<const WHash&>(o));
					WKeyEqual::operator=(static_cast<const WKeyEqual&>(o));
					DataPool::operator=(static_cast<DataPool const&>(o));

					return *this;
				}

				// clean up old stuff
				Destroyer<Self, IsFlat && std::is_trivially_destructible<Node>::value>{}.nodes(*this);

				if (mMask != o.mMask) {
					// no luck: we don't have the same array size allocated, so we need to realloc.
					if (0 != mMask) {
						// only deallocate if we actually have data!
						ROBIN_HOOD_LOG("std::free")
						gaia::mem::mem_free(mKeyVals);
					}

					auto const numElementsWithBuffer = calcNumElementsWithBuffer(o.mMask + 1);
					auto const numBytesTotal = calcNumBytesTotal(numElementsWithBuffer);
					ROBIN_HOOD_LOG(
							"gaia::mem::mem_alloc " << numBytesTotal << " = calcNumBytesTotal(" << numElementsWithBuffer << ")")
					mKeyVals = static_cast<Node*>(detail::assertNotNull<std::bad_alloc>(gaia::mem::mem_alloc(numBytesTotal)));

					// no need for calloc here because cloneData performs a memcpy.
					mInfo = reinterpret_cast<uint8_t*>(mKeyVals + numElementsWithBuffer);
					// sentinel is set in cloneData
				}
				WHash::operator=(static_cast<const WHash&>(o));
				WKeyEqual::operator=(static_cast<const WKeyEqual&>(o));
				DataPool::operator=(static_cast<DataPool const&>(o));
				mHashMultiplier = o.mHashMultiplier;
				mNumElements = o.mNumElements;
				mMask = o.mMask;
				mMaxNumElementsAllowed = o.mMaxNumElementsAllowed;
				mInfoInc = o.mInfoInc;
				mInfoHashShift = o.mInfoHashShift;
				cloneData(o);

				return *this;
			}

			// Swaps everything between the two maps.
			void swap(Table& o) {
				ROBIN_HOOD_TRACE(this)
				using std::swap;
				swap(o, *this);
			}

			// Clears all data, without resizing.
			void clear() {
				ROBIN_HOOD_TRACE(this)
				if (empty()) {
					// don't do anything! also important because we don't want to write to
					// DummyInfoByte::b, even though we would just write 0 to it.
					return;
				}

				Destroyer<Self, IsFlat && std::is_trivially_destructible<Node>::value>{}.nodes(*this);

				auto const numElementsWithBuffer = calcNumElementsWithBuffer(mMask + 1);
				// clear everything, then set the sentinel again
				uint8_t const z = 0;
				gaia::core::fill(mInfo, mInfo + calcNumBytesInfo(numElementsWithBuffer), z);
				mInfo[numElementsWithBuffer] = 1;

				mInfoInc = InitialInfoInc;
				mInfoHashShift = InitialInfoHashShift;
			}

			// Destroys the map and all it's contents.
			~Table() {
				ROBIN_HOOD_TRACE(this)
				destroy();
			}

			// Checks if both tables contain the same entries. Order is irrelevant.
			bool operator==(const Table& other) const {
				ROBIN_HOOD_TRACE(this)
				if (other.size() != size()) {
					return false;
				}
				for (auto const& otherEntry: other) {
					if (!has(otherEntry)) {
						return false;
					}
				}

				return true;
			}

			bool operator!=(const Table& other) const {
				ROBIN_HOOD_TRACE(this)
				return !operator==(other);
			}

			template <typename Q = mapped_type>
			typename std::enable_if<!std::is_void<Q>::value, Q&>::type operator[](const key_type& key) {
				ROBIN_HOOD_TRACE(this)
				auto idxAndState = insertKeyPrepareEmptySpot(key);
				switch (idxAndState.second) {
					case InsertionState::key_found:
						break;

					case InsertionState::new_node:
						::new (static_cast<void*>(&mKeyVals[idxAndState.first]))
								Node(*this, std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple());
						break;

					case InsertionState::overwrite_node:
						mKeyVals[idxAndState.first] =
								Node(*this, std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple());
						break;

					case InsertionState::overflow_error:
						throwOverflowError();
				}

				return mKeyVals[idxAndState.first].getSecond();
			}

			template <typename Q = mapped_type>
			typename std::enable_if<!std::is_void<Q>::value, Q&>::type operator[](key_type&& key) {
				ROBIN_HOOD_TRACE(this)
				auto idxAndState = insertKeyPrepareEmptySpot(key);
				switch (idxAndState.second) {
					case InsertionState::key_found:
						break;

					case InsertionState::new_node:
						::new (static_cast<void*>(&mKeyVals[idxAndState.first]))
								Node(*this, std::piecewise_construct, std::forward_as_tuple(std::move(key)), std::forward_as_tuple());
						break;

					case InsertionState::overwrite_node:
						mKeyVals[idxAndState.first] =
								Node(*this, std::piecewise_construct, std::forward_as_tuple(std::move(key)), std::forward_as_tuple());
						break;

					case InsertionState::overflow_error:
						throwOverflowError();
				}

				return mKeyVals[idxAndState.first].getSecond();
			}

			template <typename Iter>
			void insert(Iter first, Iter last) {
				for (; first != last; ++first) {
					// value_type ctor needed because this might be called with std::pair's
					insert(value_type(*first));
				}
			}

			void insert(std::initializer_list<value_type> ilist) {
				for (auto&& vt: ilist) {
					insert(std::move(vt));
				}
			}

			template <typename... Args>
			std::pair<iterator, bool> emplace(Args&&... args) {
				ROBIN_HOOD_TRACE(this)
				Node n{*this, std::forward<Args>(args)...};
				auto idxAndState = insertKeyPrepareEmptySpot(getFirstConst(n));
				switch (idxAndState.second) {
					case InsertionState::key_found:
						n.destroy(*this);
						break;

					case InsertionState::new_node:
						::new (static_cast<void*>(&mKeyVals[idxAndState.first])) Node(*this, std::move(n));
						break;

					case InsertionState::overwrite_node:
						mKeyVals[idxAndState.first] = std::move(n);
						break;

					case InsertionState::overflow_error:
						n.destroy(*this);
						throwOverflowError();
						break;
				}

				return std::make_pair(
						iterator(mKeyVals + idxAndState.first, mInfo + idxAndState.first),
						InsertionState::key_found != idxAndState.second);
			}

			template <typename... Args>
			iterator emplace_hint(const_iterator position, Args&&... args) {
				(void)position;
				return emplace(std::forward<Args>(args)...).first;
			}

			template <typename... Args>
			std::pair<iterator, bool> try_emplace(const key_type& key, Args&&... args) {
				return try_emplace_impl(key, std::forward<Args>(args)...);
			}

			template <typename... Args>
			std::pair<iterator, bool> try_emplace(key_type&& key, Args&&... args) {
				return try_emplace_impl(std::move(key), std::forward<Args>(args)...);
			}

			template <typename... Args>
			iterator try_emplace(const_iterator hint, const key_type& key, Args&&... args) {
				(void)hint;
				return try_emplace_impl(key, std::forward<Args>(args)...).first;
			}

			template <typename... Args>
			iterator try_emplace(const_iterator hint, key_type&& key, Args&&... args) {
				(void)hint;
				return try_emplace_impl(std::move(key), std::forward<Args>(args)...).first;
			}

			template <typename Mapped>
			std::pair<iterator, bool> insert_or_assign(const key_type& key, Mapped&& obj) {
				return insertOrAssignImpl(key, std::forward<Mapped>(obj));
			}

			template <typename Mapped>
			std::pair<iterator, bool> insert_or_assign(key_type&& key, Mapped&& obj) {
				return insertOrAssignImpl(std::move(key), std::forward<Mapped>(obj));
			}

			template <typename Mapped>
			iterator insert_or_assign(const_iterator hint, const key_type& key, Mapped&& obj) {
				(void)hint;
				return insertOrAssignImpl(key, std::forward<Mapped>(obj)).first;
			}

			template <typename Mapped>
			iterator insert_or_assign(const_iterator hint, key_type&& key, Mapped&& obj) {
				(void)hint;
				return insertOrAssignImpl(std::move(key), std::forward<Mapped>(obj)).first;
			}

			std::pair<iterator, bool> insert(const value_type& keyval) {
				ROBIN_HOOD_TRACE(this)
				return emplace(keyval);
			}

			iterator insert(const_iterator hint, const value_type& keyval) {
				(void)hint;
				return emplace(keyval).first;
			}

			std::pair<iterator, bool> insert(value_type&& keyval) {
				return emplace(std::move(keyval));
			}

			iterator insert(const_iterator hint, value_type&& keyval) {
				(void)hint;
				return emplace(std::move(keyval)).first;
			}

			// Returns 1 if key is found, 0 otherwise.
			GAIA_NODISCARD size_t count(const key_type& key) const {
				ROBIN_HOOD_TRACE(this)
				auto kv = mKeyVals + findIdx(key);
				if (kv != reinterpret_cast_no_cast_align_warning<Node*>(mInfo)) {
					return 1;
				}
				return 0;
			}

			template <typename OtherKey, typename Self_ = Self>
			GAIA_NODISCARD typename std::enable_if<Self_::is_transparent, size_t>::type count(const OtherKey& key) const {
				ROBIN_HOOD_TRACE(this)
				auto kv = mKeyVals + findIdx(key);
				if (kv != reinterpret_cast_no_cast_align_warning<Node*>(mInfo)) {
					return 1;
				}
				return 0;
			}

			GAIA_NODISCARD bool contains(const key_type& key) const {
				return 1U == count(key);
			}

			template <typename OtherKey, typename Self_ = Self>
			GAIA_NODISCARD typename std::enable_if<Self_::is_transparent, bool>::type contains(const OtherKey& key) const {
				return 1U == count(key);
			}

			// Returns a reference to the value found for key.
			// Throws std::out_of_range if element cannot be found
			template <typename Q = mapped_type>
			GAIA_NODISCARD typename std::enable_if<!std::is_void<Q>::value, Q&>::type at(key_type const& key) {
				ROBIN_HOOD_TRACE(this)
				auto kv = mKeyVals + findIdx(key);
				if (kv == reinterpret_cast_no_cast_align_warning<Node*>(mInfo)) {
					doThrow<ROBIN_HOOD_STD_OUT_OF_RANGE>("key not found");
				}
				return kv->getSecond();
			}

			// Returns a reference to the value found for key.
			// Throws std::out_of_range if element cannot be found
			template <typename Q = mapped_type>
			GAIA_NODISCARD typename std::enable_if<!std::is_void<Q>::value, Q const&>::type at(key_type const& key) const {
				ROBIN_HOOD_TRACE(this)
				auto kv = mKeyVals + findIdx(key);
				if (kv == reinterpret_cast_no_cast_align_warning<Node*>(mInfo)) {
					doThrow<ROBIN_HOOD_STD_OUT_OF_RANGE>("key not found");
				}
				return kv->getSecond();
			}

			GAIA_NODISCARD const_iterator find(const key_type& key) const {
				ROBIN_HOOD_TRACE(this)
				const size_t idx = findIdx(key);
				return const_iterator{mKeyVals + idx, mInfo + idx};
			}

			template <typename OtherKey>
			GAIA_NODISCARD const_iterator find(const OtherKey& key, is_transparent_tag /*unused*/) const {
				ROBIN_HOOD_TRACE(this)
				const size_t idx = findIdx(key);
				return const_iterator{mKeyVals + idx, mInfo + idx};
			}

			template <typename OtherKey, typename Self_ = Self>
			GAIA_NODISCARD typename std::enable_if<Self_::is_transparent, const_iterator>::type
			find(const OtherKey& key) const {
				ROBIN_HOOD_TRACE(this)
				const size_t idx = findIdx(key);
				return const_iterator{mKeyVals + idx, mInfo + idx};
			}

			GAIA_NODISCARD iterator find(const key_type& key) {
				ROBIN_HOOD_TRACE(this)
				const size_t idx = findIdx(key);
				return iterator{mKeyVals + idx, mInfo + idx};
			}

			template <typename OtherKey>
			GAIA_NODISCARD iterator find(const OtherKey& key, is_transparent_tag /*unused*/) {
				ROBIN_HOOD_TRACE(this)
				const size_t idx = findIdx(key);
				return iterator{mKeyVals + idx, mInfo + idx};
			}

			template <typename OtherKey, typename Self_ = Self>
			GAIA_NODISCARD typename std::enable_if<Self_::is_transparent, iterator>::type find(const OtherKey& key) {
				ROBIN_HOOD_TRACE(this)
				const size_t idx = findIdx(key);
				return iterator{mKeyVals + idx, mInfo + idx};
			}

			GAIA_NODISCARD iterator begin() {
				ROBIN_HOOD_TRACE(this)
				if (empty()) {
					return end();
				}
				return iterator(mKeyVals, mInfo, fast_forward_tag{});
			}
			GAIA_NODISCARD const_iterator begin() const {
				ROBIN_HOOD_TRACE(this)
				return cbegin();
			}
			GAIA_NODISCARD const_iterator cbegin() const {
				ROBIN_HOOD_TRACE(this)
				if (empty()) {
					return cend();
				}
				return const_iterator(mKeyVals, mInfo, fast_forward_tag{});
			}

			GAIA_NODISCARD iterator end() {
				ROBIN_HOOD_TRACE(this)
				// no need to supply valid info pointer: end() must not be dereferenced, and only node
				// pointer is compared.
				return iterator{reinterpret_cast_no_cast_align_warning<Node*>(mInfo), nullptr};
			}
			GAIA_NODISCARD const_iterator end() const {
				ROBIN_HOOD_TRACE(this)
				return cend();
			}
			GAIA_NODISCARD const_iterator cend() const {
				ROBIN_HOOD_TRACE(this)
				return const_iterator{reinterpret_cast_no_cast_align_warning<Node*>(mInfo), nullptr};
			}

			iterator erase(const_iterator pos) {
				ROBIN_HOOD_TRACE(this)
				// its safe to perform const cast here
				// NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
				return erase(iterator{const_cast<Node*>(pos.mKeyVals), const_cast<uint8_t*>(pos.mInfo)});
			}

			// Erases element at pos, returns iterator to the next element.
			iterator erase(iterator pos) {
				ROBIN_HOOD_TRACE(this)
				// we assume that pos always points to a valid entry, and not end().
				auto const idx = static_cast<size_t>(pos.mKeyVals - mKeyVals);

				shiftDown(idx);
				--mNumElements;

				if (*pos.mInfo) {
					// we've backward shifted, return this again
					return pos;
				}

				// no backward shift, return next element
				return ++pos;
			}

			size_t erase(const key_type& key) {
				ROBIN_HOOD_TRACE(this)
				size_t idx{};
				InfoType info{};
				keyToIdx(key, &idx, &info);

				// check while info matches with the source idx
				do {
					if (info == mInfo[idx] && WKeyEqual::operator()(key, mKeyVals[idx].getFirst())) {
						shiftDown(idx);
						--mNumElements;
						return 1;
					}
					next(&info, &idx);
				} while (info <= mInfo[idx]);

				// nothing found to delete
				return 0;
			}

			// reserves space for the specified number of elements. Makes sure the old data fits.
			// exactly the same as reserve(c).
			void rehash(size_t c) {
				// forces a reserve
				reserve(c, true);
			}

			// reserves space for the specified number of elements. Makes sure the old data fits.
			// Exactly the same as rehash(c). Use rehash(0) to shrink to fit.
			void reserve(size_t c) {
				// reserve, but don't force rehash
				reserve(c, false);
			}

			// If possible reallocates the map to a smaller one. This frees the underlying table.
			// Does not do anything if load_factor is too large for decreasing the table's size.
			void compact() {
				ROBIN_HOOD_TRACE(this)
				auto newSize = InitialNumElements;
				while (calcMaxNumElementsAllowed(newSize) < mNumElements && newSize != 0) {
					newSize *= 2;
				}
				if GAIA_UNLIKELY (newSize == 0) {
					throwOverflowError();
				}

				ROBIN_HOOD_LOG("newSize > mMask + 1: " << newSize << " > " << mMask << " + 1")

				// only actually do anything when the new size is bigger than the old one. This prevents to
				// continuously allocate for each reserve() call.
				if (newSize < mMask + 1) {
					rehashPowerOfTwo(newSize, true);
				}
			}

			GAIA_NODISCARD size_type size() const noexcept {
				ROBIN_HOOD_TRACE(this)
				return mNumElements;
			}

			GAIA_NODISCARD size_type max_size() const noexcept {
				ROBIN_HOOD_TRACE(this)
				return static_cast<size_type>(-1);
			}

			GAIA_NODISCARD bool empty() const noexcept {
				ROBIN_HOOD_TRACE(this)
				return 0 == mNumElements;
			}

			GAIA_NODISCARD float max_load_factor() const noexcept {
				ROBIN_HOOD_TRACE(this)
				return MaxLoadFactor100 / 100.0F;
			}

			// Average number of elements per bucket. Since we allow only 1 per bucket
			GAIA_NODISCARD float load_factor() const noexcept {
				ROBIN_HOOD_TRACE(this)
				return static_cast<float>(size()) / static_cast<float>(mMask + 1);
			}

			GAIA_NODISCARD size_t mask() const noexcept {
				ROBIN_HOOD_TRACE(this)
				return mMask;
			}

			GAIA_NODISCARD size_t calcMaxNumElementsAllowed(size_t maxElements) const noexcept {
				if GAIA_LIKELY (maxElements <= (size_t(-1) / 100)) {
					return maxElements * MaxLoadFactor100 / 100;
				}

				// we might be a bit inprecise, but since maxElements is quite large that doesn't matter
				return (maxElements / 100) * MaxLoadFactor100;
			}

			GAIA_NODISCARD size_t calcNumBytesInfo(size_t numElements) const noexcept {
				// we add a uint64_t, which houses the sentinel (first byte) and padding so we can load
				// 64bit types.
				return numElements + sizeof(uint64_t);
			}

			GAIA_NODISCARD size_t calcNumElementsWithBuffer(size_t numElements) const noexcept {
				auto maxNumElementsAllowed = calcMaxNumElementsAllowed(numElements);
				return numElements + gaia::core::get_min(maxNumElementsAllowed, (static_cast<size_t>(0xFF)));
			}

			// calculation only allowed for 2^n values
			GAIA_NODISCARD size_t calcNumBytesTotal(size_t numElements) const {
#if ROBIN_HOOD(BITNESS) == 64
				return numElements * sizeof(Node) + calcNumBytesInfo(numElements);
#else
				// make sure we're doing 64bit operations, so we are at least safe against 32bit overflows.
				auto const ne = static_cast<uint64_t>(numElements);
				auto const s = static_cast<uint64_t>(sizeof(Node));
				auto const infos = static_cast<uint64_t>(calcNumBytesInfo(numElements));

				auto const total64 = ne * s + infos;
				auto const total = static_cast<size_t>(total64);

				if GAIA_UNLIKELY (static_cast<uint64_t>(total) != total64) {
					throwOverflowError();
				}
				return total;
#endif
			}

		private:
			template <typename Q = mapped_type>
			GAIA_NODISCARD typename std::enable_if<!std::is_void<Q>::value, bool>::type has(const value_type& e) const {
				ROBIN_HOOD_TRACE(this)
				auto it = find(e.first);
				return it != end() && it->second == e.second;
			}

			template <typename Q = mapped_type>
			GAIA_NODISCARD typename std::enable_if<std::is_void<Q>::value, bool>::type has(const value_type& e) const {
				ROBIN_HOOD_TRACE(this)
				return find(e) != end();
			}

			void reserve(size_t c, bool forceRehash) {
				ROBIN_HOOD_TRACE(this)
				auto const minElementsAllowed = gaia::core::get_max(c, mNumElements);
				auto newSize = InitialNumElements;
				while (calcMaxNumElementsAllowed(newSize) < minElementsAllowed && newSize != 0) {
					newSize *= 2;
				}
				if GAIA_UNLIKELY (newSize == 0) {
					throwOverflowError();
				}

				ROBIN_HOOD_LOG("newSize > mMask + 1: " << newSize << " > " << mMask << " + 1")

				// only actually do anything when the new size is bigger than the old one. This prevents to
				// continuously allocate for each reserve() call.
				if (forceRehash || newSize > mMask + 1) {
					rehashPowerOfTwo(newSize, false);
				}
			}

			// reserves space for at least the specified number of elements.
			// only works if numBuckets if power of two
			// True on success, false otherwise
			void rehashPowerOfTwo(size_t numBuckets, bool forceFree) {
				ROBIN_HOOD_TRACE(this)

				Node* const oldKeyVals = mKeyVals;
				uint8_t const* const oldInfo = mInfo;

				const size_t oldMaxElementsWithBuffer = calcNumElementsWithBuffer(mMask + 1);

				// resize operation: move stuff
				initData(numBuckets);
				if (oldMaxElementsWithBuffer > 1) {
					for (size_t i = 0; i < oldMaxElementsWithBuffer; ++i) {
						if (oldInfo[i] != 0) {
							// might throw an exception, which is really bad since we are in the middle of
							// moving stuff.
							insert_move(std::move(oldKeyVals[i]));
							// destroy the node but DON'T destroy the data.
							oldKeyVals[i].~Node();
						}
					}

					// this check is not necessary as it's guarded by the previous if, but it helps
					// silence g++'s overeager "attempt to free a non-heap object 'map'
					// [-Werror=free-nonheap-object]" warning.
					if (oldKeyVals != reinterpret_cast_no_cast_align_warning<Node*>(&mMask)) {
						// don't destroy old data: put it into the pool instead
						if (forceFree) {
							gaia::mem::mem_free(oldKeyVals);
						} else {
							DataPool::addOrFree(oldKeyVals, calcNumBytesTotal(oldMaxElementsWithBuffer));
						}
					}
				}
			}

			GAIA_NOINLINE void throwOverflowError() const {
#if ROBIN_HOOD(HAS_EXCEPTIONS)
				throw std::overflow_error("robin_hood::map overflow");
#else
				abort();
#endif
			}

			template <typename OtherKey, typename... Args>
			std::pair<iterator, bool> try_emplace_impl(OtherKey&& key, Args&&... args) {
				ROBIN_HOOD_TRACE(this)
				auto idxAndState = insertKeyPrepareEmptySpot(key);
				switch (idxAndState.second) {
					case InsertionState::key_found:
						break;

					case InsertionState::new_node:
						::new (static_cast<void*>(&mKeyVals[idxAndState.first])) Node(
								*this, std::piecewise_construct, std::forward_as_tuple(std::forward<OtherKey>(key)),
								std::forward_as_tuple(std::forward<Args>(args)...));
						break;

					case InsertionState::overwrite_node:
						mKeyVals[idxAndState.first] = Node(
								*this, std::piecewise_construct, std::forward_as_tuple(std::forward<OtherKey>(key)),
								std::forward_as_tuple(std::forward<Args>(args)...));
						break;

					case InsertionState::overflow_error:
						throwOverflowError();
						break;
				}

				return std::make_pair(
						iterator(mKeyVals + idxAndState.first, mInfo + idxAndState.first),
						InsertionState::key_found != idxAndState.second);
			}

			template <typename OtherKey, typename Mapped>
			std::pair<iterator, bool> insertOrAssignImpl(OtherKey&& key, Mapped&& obj) {
				ROBIN_HOOD_TRACE(this)
				auto idxAndState = insertKeyPrepareEmptySpot(key);
				switch (idxAndState.second) {
					case InsertionState::key_found:
						mKeyVals[idxAndState.first].getSecond() = std::forward<Mapped>(obj);
						break;

					case InsertionState::new_node:
						::new (static_cast<void*>(&mKeyVals[idxAndState.first])) Node(
								*this, std::piecewise_construct, std::forward_as_tuple(std::forward<OtherKey>(key)),
								std::forward_as_tuple(std::forward<Mapped>(obj)));
						break;

					case InsertionState::overwrite_node:
						mKeyVals[idxAndState.first] = Node(
								*this, std::piecewise_construct, std::forward_as_tuple(std::forward<OtherKey>(key)),
								std::forward_as_tuple(std::forward<Mapped>(obj)));
						break;

					case InsertionState::overflow_error:
						throwOverflowError();
						break;
				}

				return std::make_pair(
						iterator(mKeyVals + idxAndState.first, mInfo + idxAndState.first),
						InsertionState::key_found != idxAndState.second);
			}

			void initData(size_t max_elements) {
				mNumElements = 0;
				mMask = max_elements - 1;
				mMaxNumElementsAllowed = calcMaxNumElementsAllowed(max_elements);

				auto const numElementsWithBuffer = calcNumElementsWithBuffer(max_elements);

				// malloc & zero mInfo. Faster than calloc everything.
				auto const numBytesTotal = calcNumBytesTotal(numElementsWithBuffer);
				ROBIN_HOOD_LOG("std::calloc " << numBytesTotal << " = calcNumBytesTotal(" << numElementsWithBuffer << ")")
				mKeyVals = reinterpret_cast<Node*>(detail::assertNotNull<std::bad_alloc>(gaia::mem::mem_alloc(numBytesTotal)));
				mInfo = reinterpret_cast<uint8_t*>(mKeyVals + numElementsWithBuffer);
				std::memset(mInfo, 0, numBytesTotal - numElementsWithBuffer * sizeof(Node));

				// set sentinel
				mInfo[numElementsWithBuffer] = 1;

				mInfoInc = InitialInfoInc;
				mInfoHashShift = InitialInfoHashShift;
			}

			enum class InsertionState { overflow_error, key_found, new_node, overwrite_node };

			// Finds key, and if not already present prepares a spot where to pot the key & value.
			// This potentially shifts nodes out of the way, updates mInfo and number of inserted
			// elements, so the only operation left to do is create/assign a new node at that spot.
			template <typename OtherKey>
			std::pair<size_t, InsertionState> insertKeyPrepareEmptySpot(OtherKey&& key) {
				for (int i = 0; i < 256; ++i) {
					size_t idx{};
					InfoType info{};
					keyToIdx(key, &idx, &info);
					nextWhileLess(&info, &idx);

					// while we potentially have a match
					while (info == mInfo[idx]) {
						if (WKeyEqual::operator()(key, mKeyVals[idx].getFirst())) {
							// key already exists, do NOT insert.
							// see http://en.cppreference.com/w/cpp/container/unordered_map/insert
							return std::make_pair(idx, InsertionState::key_found);
						}
						next(&info, &idx);
					}

					// unlikely that this evaluates to true
					if GAIA_UNLIKELY (mNumElements >= mMaxNumElementsAllowed) {
						if (!increase_size()) {
							return std::make_pair(size_t(0), InsertionState::overflow_error);
						}
						continue;
					}

					// key not found, so we are now exactly where we want to insert it.
					auto const insertion_idx = idx;
					auto const insertion_info = info;
					if GAIA_UNLIKELY (insertion_info + mInfoInc > 0xFF) {
						mMaxNumElementsAllowed = 0;
					}

					// find an empty spot
					while (0 != mInfo[idx]) {
						next(&info, &idx);
					}

					if (idx != insertion_idx) {
						shiftUp(idx, insertion_idx);
					}
					// put at empty spot
					mInfo[insertion_idx] = static_cast<uint8_t>(insertion_info);
					++mNumElements;
					return std::make_pair(
							insertion_idx, idx == insertion_idx ? InsertionState::new_node : InsertionState::overwrite_node);
				}

				// enough attempts failed, so finally give up.
				return std::make_pair(size_t(0), InsertionState::overflow_error);
			}

			bool try_increase_info() {
				ROBIN_HOOD_LOG(
						"mInfoInc=" << mInfoInc << ", numElements=" << mNumElements
												<< ", maxNumElementsAllowed=" << calcMaxNumElementsAllowed(mMask + 1))
				if (mInfoInc <= 2) {
					// need to be > 2 so that shift works (otherwise undefined behavior!)
					return false;
				}
				// we got space left, try to make info smaller
				mInfoInc = static_cast<uint8_t>(mInfoInc >> 1U);

				// remove one bit of the hash, leaving more space for the distance info.
				// This is extremely fast because we can operate on 8 bytes at once.
				++mInfoHashShift;
				auto const numElementsWithBuffer = calcNumElementsWithBuffer(mMask + 1);

				for (size_t i = 0; i < numElementsWithBuffer; i += 8) {
					auto val = unaligned_load<uint64_t>(mInfo + i);
					val = (val >> 1U) & UINT64_C(0x7f7f7f7f7f7f7f7f);
					std::memcpy(mInfo + i, &val, sizeof(val));
				}
				// update sentinel, which might have been cleared out!
				mInfo[numElementsWithBuffer] = 1;

				mMaxNumElementsAllowed = calcMaxNumElementsAllowed(mMask + 1);
				return true;
			}

			// True if resize was possible, false otherwise
			bool increase_size() {
				// nothing allocated yet? just allocate InitialNumElements
				if (0 == mMask) {
					initData(InitialNumElements);
					return true;
				}

				auto const maxNumElementsAllowed = calcMaxNumElementsAllowed(mMask + 1);
				if (mNumElements < maxNumElementsAllowed && try_increase_info()) {
					return true;
				}

				ROBIN_HOOD_LOG(
						"mNumElements=" << mNumElements << ", maxNumElementsAllowed=" << maxNumElementsAllowed << ", load="
														<< (static_cast<double>(mNumElements) * 100.0 / (static_cast<double>(mMask) + 1)))

				if (mNumElements * 2 < calcMaxNumElementsAllowed(mMask + 1)) {
					// we have to resize, even though there would still be plenty of space left!
					// Try to rehash instead. Delete freed memory so we don't steadyily increase mem in case
					// we have to rehash a few times
					nextHashMultiplier();
					rehashPowerOfTwo(mMask + 1, true);
				} else {
					// we've reached the capacity of the map, so the hash seems to work nice. Keep using it.
					rehashPowerOfTwo((mMask + 1) * 2, false);
				}
				return true;
			}

			void nextHashMultiplier() {
				// adding an *even* number, so that the multiplier will always stay odd. This is necessary
				// so that the hash stays a mixing function (and thus doesn't have any information loss).
				mHashMultiplier += UINT64_C(0xc4ceb9fe1a85ec54);
			}

			void destroy() {
				if (0 == mMask) {
					// don't deallocate!
					return;
				}

				Destroyer<Self, IsFlat && std::is_trivially_destructible<Node>::value>{}.nodesDoNotDeallocate(*this);

				// This protection against not deleting mMask shouldn't be needed as it's sufficiently
				// protected with the 0==mMask check, but I have this anyways because g++ 7 otherwise
				// reports a compile error: attempt to free a non-heap object 'fm'
				// [-Werror=free-nonheap-object]
				if (mKeyVals != reinterpret_cast_no_cast_align_warning<Node*>(&mMask)) {
					ROBIN_HOOD_LOG("std::free")
					gaia::mem::mem_free(mKeyVals);
				}
			}

			void init() noexcept {
				mKeyVals = reinterpret_cast_no_cast_align_warning<Node*>(&mMask);
				mInfo = reinterpret_cast<uint8_t*>(&mMask);
				mNumElements = 0;
				mMask = 0;
				mMaxNumElementsAllowed = 0;
				mInfoInc = InitialInfoInc;
				mInfoHashShift = InitialInfoHashShift;
			}

			// members are sorted so no padding occurs
			uint64_t mHashMultiplier = UINT64_C(0xc4ceb9fe1a85ec53); // 8 byte  8
			Node* mKeyVals = reinterpret_cast_no_cast_align_warning<Node*>(&mMask); // 8 byte 16
			uint8_t* mInfo = reinterpret_cast<uint8_t*>(&mMask); // 8 byte 24
			size_t mNumElements = 0; // 8 byte 32
			size_t mMask = 0; // 8 byte 40
			size_t mMaxNumElementsAllowed = 0; // 8 byte 48
			InfoType mInfoInc = InitialInfoInc; // 4 byte 52
			InfoType mInfoHashShift = InitialInfoHashShift; // 4 byte 56
																											// 16 byte 56 if NodeAllocator
		};

	} // namespace detail

	// map

	template <
			typename Key, typename T, typename Hash = hash<Key>, typename KeyEqual = gaia::core::equal_to<Key>,
			size_t MaxLoadFactor100 = 80>
	using unordered_flat_map = detail::Table<true, MaxLoadFactor100, Key, T, Hash, KeyEqual>;

	template <
			typename Key, typename T, typename Hash = hash<Key>, typename KeyEqual = gaia::core::equal_to<Key>,
			size_t MaxLoadFactor100 = 80>
	using unordered_node_map = detail::Table<false, MaxLoadFactor100, Key, T, Hash, KeyEqual>;

	template <
			typename Key, typename T, typename Hash = hash<Key>, typename KeyEqual = gaia::core::equal_to<Key>,
			size_t MaxLoadFactor100 = 80>
	using unordered_map = detail::Table<
			sizeof(robin_hood::pair<Key, T>) <= sizeof(size_t) * 6 &&
					std::is_nothrow_move_constructible<robin_hood::pair<Key, T>>::value &&
					std::is_nothrow_move_assignable<robin_hood::pair<Key, T>>::value,
			MaxLoadFactor100, Key, T, Hash, KeyEqual>;

	// set

	template <
			typename Key, typename Hash = hash<Key>, typename KeyEqual = gaia::core::equal_to<Key>,
			size_t MaxLoadFactor100 = 80>
	using unordered_flat_set = detail::Table<true, MaxLoadFactor100, Key, void, Hash, KeyEqual>;

	template <
			typename Key, typename Hash = hash<Key>, typename KeyEqual = gaia::core::equal_to<Key>,
			size_t MaxLoadFactor100 = 80>
	using unordered_node_set = detail::Table<false, MaxLoadFactor100, Key, void, Hash, KeyEqual>;

	template <
			typename Key, typename Hash = hash<Key>, typename KeyEqual = gaia::core::equal_to<Key>,
			size_t MaxLoadFactor100 = 80>
	using unordered_set = detail::Table<
			sizeof(Key) <= sizeof(size_t) * 6 && std::is_nothrow_move_constructible<Key>::value &&
					std::is_nothrow_move_assignable<Key>::value,
			MaxLoadFactor100, Key, void, Hash, KeyEqual>;

} // namespace robin_hood

#endif

namespace gaia {
	namespace cnt {
		template <typename Key>
		using set = robin_hood::unordered_flat_set<Key>;
	} // namespace cnt
} // namespace gaia

#include <cstddef>
#include <type_traits>
#include <utility>

namespace gaia {
	namespace cnt {
		namespace sringbuffer_detail {
			using difference_type = uint32_t;
			using size_type = uint32_t;
		} // namespace sringbuffer_detail

		//! Array of elements of type \tparam T with fixed size and capacity \tparam N allocated on stack
		//! working as a ring buffer. That means the element at position N-1 is followed by the element
		//! at the position 0.
		//! Interface compatiblity with std::array where it matters.
		template <typename T, sringbuffer_detail::size_type N>
		class sringbuffer {
		public:
			static_assert(N > 1);
			static_assert(!mem::is_soa_layout_v<T>);

			using iterator_category = core::random_access_iterator_tag;
			using value_type = T;
			using reference = T&;
			using const_reference = const T&;
			using pointer = T*;
			using const_pointer = T*;
			using difference_type = sringbuffer_detail::size_type;
			using size_type = sringbuffer_detail::size_type;

			static constexpr size_type extent = N;

			size_type m_tail{};
			size_type m_size{};
			T m_data[N];

			sringbuffer() noexcept = default;

			template <typename InputIt>
			sringbuffer(InputIt first, InputIt last) {
				const auto count = (size_type)core::distance(first, last);
				GAIA_ASSERT(count <= max_size());
				if (count < 1)
					return;

				m_size = count;
				m_tail = 0;

				if constexpr (std::is_pointer_v<InputIt>) {
					for (size_type i = 0; i < count; ++i)
						m_data[i] = first[i];
				} else if constexpr (std::is_same_v<typename InputIt::iterator_category, core::random_access_iterator_tag>) {
					for (size_type i = 0; i < count; ++i)
						m_data[i] = *(first[i]);
				} else {
					size_type i = 0;
					for (auto it = first; it != last; ++it)
						m_data[i++] = *it;
				}
			}

			sringbuffer(std::initializer_list<T> il): sringbuffer(il.begin(), il.end()) {}

			sringbuffer(const sringbuffer& other): sringbuffer(other.begin(), other.end()) {}

			sringbuffer(sringbuffer&& other) noexcept: m_tail(other.m_tail), m_size(other.m_size) {
				GAIA_ASSERT(mem::addressof(other) != this);

				mem::move_elements<T>(m_data, other.m_data, 0, other.size(), extent, other.extent);

				other.m_tail = size_type(0);
				other.m_size = size_type(0);
			}

			sringbuffer& operator=(std::initializer_list<T> il) {
				*this = sringbuffer(il.begin(), il.end());
				return *this;
			}

			constexpr sringbuffer& operator=(const sringbuffer& other) {
				GAIA_ASSERT(mem::addressof(other) != this);

				mem::copy_elements<T>((uint8_t*)&m_data[0], other.m_data, 0, other.size(), extent, other.extent);

				m_tail = other.m_tail;
				m_size = other.m_size;

				return *this;
			}

			constexpr sringbuffer& operator=(sringbuffer&& other) noexcept {
				GAIA_ASSERT(mem::addressof(other) != this);

				mem::move_elements<T>(m_data, other.m_data, 0, other.size(), extent, other.extent);

				m_tail = other.m_tail;
				m_size = other.m_size;

				other.m_tail = size_type(0);
				other.m_size = size_type(0);

				return *this;
			}

			~sringbuffer() = default;

			void push_back(const T& arg) {
				GAIA_ASSERT(m_size < N);
				const auto head = (m_tail + m_size) % N;
				m_data[head] = arg;
				++m_size;
			}

			void push_back(T&& arg) {
				GAIA_ASSERT(m_size < N);
				const auto head = (m_tail + m_size) % N;
				m_data[head] = std::forward<T>(arg);
				++m_size;
			}

			void pop_front(T& out) {
				GAIA_ASSERT(!empty());
				out = m_data[m_tail];
				m_tail = (m_tail + 1) % N;
				--m_size;
			}

			void pop_front(T&& out) {
				GAIA_ASSERT(!empty());
				out = std::forward<T>(m_data[m_tail]);
				m_tail = (m_tail + 1) % N;
				--m_size;
			}

			void pop_back(T& out) {
				GAIA_ASSERT(m_size < N);
				const auto head = (m_tail + m_size - 1) % N;
				out = m_data[head];
				--m_size;
			}

			void pop_back(T&& out) {
				GAIA_ASSERT(m_size < N);
				const auto head = (m_tail + m_size - 1) % N;
				out = std::forward<T>(m_data[head]);
				--m_size;
			}

			GAIA_NODISCARD constexpr size_type size() const noexcept {
				return m_size;
			}

			GAIA_NODISCARD constexpr bool empty() const noexcept {
				return !m_size;
			}

			GAIA_NODISCARD constexpr size_type max_size() const noexcept {
				return N;
			}

			GAIA_NODISCARD constexpr reference front() noexcept {
				GAIA_ASSERT(!empty());
				return m_data[m_tail];
			}

			GAIA_NODISCARD constexpr const_reference front() const noexcept {
				GAIA_ASSERT(!empty());
				return m_data[m_tail];
			}

			GAIA_NODISCARD constexpr reference back() noexcept {
				GAIA_ASSERT(!empty());
				const auto head = (m_tail + m_size - 1) % N;
				return m_data[head];
			}

			GAIA_NODISCARD constexpr const_reference back() const noexcept {
				GAIA_ASSERT(!empty());
				const auto head = (m_tail + m_size - 1) % N;
				return m_data[head];
			}

			GAIA_NODISCARD bool operator==(const sringbuffer& other) const {
				for (size_type i = 0; i < N; ++i)
					if (!(m_data[i] == other.m_data[i]))
						return false;
				return true;
			}
		};

		namespace detail {
			template <typename T, uint32_t N, uint32_t... I>
			constexpr sringbuffer<std::remove_cv_t<T>, N>
			to_sringbuffer_impl(T (&a)[N], std::index_sequence<I...> /*no_name*/) {
				return {{a[I]...}};
			}
		} // namespace detail

		template <typename T, uint32_t N>
		constexpr sringbuffer<std::remove_cv_t<T>, N> to_sringbuffer(T (&a)[N]) {
			return detail::to_sringbuffer_impl(a, std::make_index_sequence<N>{});
		}

		template <typename T, typename... U>
		sringbuffer(T, U...) -> sringbuffer<T, 1 + sizeof...(U)>;

	} // namespace cnt

} // namespace gaia

#if GAIA_PLATFORM_WINDOWS
	#include <windows.h>
	#include <cstdio>
#elif GAIA_PLATFORM_APPLE
	#include <mach/mach_types.h>
	#include <mach/thread_act.h>
#elif GAIA_PLATFORM_LINUX || GAIA_PLATFORM_FREEBSD
	#include <pthread.h>
#endif

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#include <functional>
#include <inttypes.h>

namespace gaia {
	namespace mt {
		struct Job {
			std::function<void()> func;
		};

		struct JobArgs {
			uint32_t idxStart;
			uint32_t idxEnd;
		};

		struct JobParallel {
			std::function<void(const JobArgs&)> func;
		};
	} // namespace mt
} // namespace gaia

#include <cinttypes>
#include <type_traits>

namespace gaia {
	namespace mt {
		using JobInternalType = uint32_t;
		using JobId = JobInternalType;
		using JobGenId = JobInternalType;

		struct JobHandle final {
			static constexpr JobInternalType IdBits = 20;
			static constexpr JobInternalType GenBits = 12;
			static constexpr JobInternalType IdMask = (uint32_t)(uint64_t(1) << IdBits) - 1;
			static constexpr JobInternalType GenMask = (uint32_t)(uint64_t(1) << GenBits) - 1;

			using JobSizeType = std::conditional_t<(IdBits + GenBits > 32), uint64_t, uint32_t>;

			static_assert(IdBits + GenBits <= 64, "Job IdBits and GenBits must fit inside 64 bits");
			static_assert(IdBits <= 31, "Job IdBits must be at most 31 bits long");
			static_assert(GenBits > 10, "Job GenBits is recommended to be at least 10 bits long");

		private:
			struct JobData {
				//! Index in entity array
				JobInternalType id: IdBits;
				//! Generation index. Incremented every time an entity is deleted
				JobInternalType gen: GenBits;
			};

			union {
				JobData data;
				JobSizeType val;
			};

		public:
			JobHandle() noexcept = default;
			JobHandle(JobId id, JobGenId gen) {
				data.id = id;
				data.gen = gen;
			}
			~JobHandle() = default;

			JobHandle(JobHandle&&) noexcept = default;
			JobHandle(const JobHandle&) = default;
			JobHandle& operator=(JobHandle&&) noexcept = default;
			JobHandle& operator=(const JobHandle&) = default;

			GAIA_NODISCARD constexpr bool operator==(const JobHandle& other) const noexcept {
				return val == other.val;
			}
			GAIA_NODISCARD constexpr bool operator!=(const JobHandle& other) const noexcept {
				return val != other.val;
			}

			auto id() const {
				return data.id;
			}
			auto gen() const {
				return data.gen;
			}
			auto value() const {
				return val;
			}
		};

		struct JobNull_t {
			GAIA_NODISCARD operator JobHandle() const noexcept {
				return JobHandle(JobHandle::IdMask, JobHandle::GenMask);
			}

			GAIA_NODISCARD constexpr bool operator==([[maybe_unused]] const JobNull_t& null) const noexcept {
				return true;
			}
			GAIA_NODISCARD constexpr bool operator!=([[maybe_unused]] const JobNull_t& null) const noexcept {
				return false;
			}
		};

		GAIA_NODISCARD inline bool operator==(const JobNull_t& null, const JobHandle& entity) noexcept {
			return static_cast<JobHandle>(null).id() == entity.id();
		}

		GAIA_NODISCARD inline bool operator!=(const JobNull_t& null, const JobHandle& entity) noexcept {
			return static_cast<JobHandle>(null).id() != entity.id();
		}

		GAIA_NODISCARD inline bool operator==(const JobHandle& entity, const JobNull_t& null) noexcept {
			return null == entity;
		}

		GAIA_NODISCARD inline bool operator!=(const JobHandle& entity, const JobNull_t& null) noexcept {
			return null != entity;
		}

		inline constexpr JobNull_t JobNull{};
	} // namespace mt
} // namespace gaia

#include <functional>
#include <inttypes.h>
#include <mutex>

namespace gaia {
	namespace mt {
		enum class JobInternalState : uint32_t {
			//! No scheduled
			Idle = 0,
			//! Scheduled
			Submitted = 0x01,
			//! Being executed
			Running = 0x02,
			//! Finished executing
			Done = 0x04,
			//! Job released. Not to be used anymore
			Released = 0x08,

			//! Scheduled or being executed
			Busy = Submitted | Running,
		};

		struct JobContainer: cnt::ilist_item {
			uint32_t dependencyIdx;
			JobInternalState state;
			std::function<void()> func;

			JobContainer() = default;
			JobContainer(uint32_t index, uint32_t generation):
					cnt::ilist_item(index, generation), state(JobInternalState::Idle) {}
		};

		struct JobDependency: cnt::ilist_item {
			uint32_t dependencyIdxNext;
			JobHandle dependsOn;

			JobDependency() = default;
			JobDependency(uint32_t index, uint32_t generation): cnt::ilist_item(index, generation) {}
		};

		using DepHandle = JobHandle;

		class JobManager {
			std::mutex m_jobsLock;
			//! Implicit list of jobs
			cnt::ilist<JobContainer, JobHandle> m_jobs;

			std::mutex m_depsLock;
			//! List of job dependencies
			cnt::ilist<JobDependency, DepHandle> m_deps;

		public:
			//! Cleans up any job allocations and dependicies associated with \param jobHandle
			void wait(JobHandle jobHandle) {
				// We need to release any dependencies related to this job
				auto& job = m_jobs[jobHandle.id()];

				if (job.state == JobInternalState::Released)
					return;

				uint32_t depIdx = job.dependencyIdx;
				while (depIdx != (uint32_t)-1) {
					auto& dep = m_deps[depIdx];
					const uint32_t depIdxNext = dep.dependencyIdxNext;
					wait(dep.dependsOn);
					free_dep(DepHandle{depIdx, 0});
					depIdx = depIdxNext;
				}

				// Deallocate the job itself
				free_job(jobHandle);
			}

			//! Allocates a new job container identified by a unique JobHandle.
			//! \return JobHandle
			//! \warning Must be used from the main thread.
			GAIA_NODISCARD JobHandle alloc_job(const Job& job) {
				std::scoped_lock<std::mutex> lock(m_jobsLock);
				auto handle = m_jobs.alloc();
				auto& j = m_jobs[handle.id()];
				GAIA_ASSERT(j.state == JobInternalState::Idle || j.state == JobInternalState::Released);
				j.dependencyIdx = (uint32_t)-1;
				j.state = JobInternalState::Idle;
				j.func = job.func;
				return handle;
			}

			//! Invalidates \param jobHandle by resetting its index in the job pool.
			//! Everytime a job is deallocated its generation is increased by one.
			//! \warning Must be used from the main thread.
			void free_job(JobHandle jobHandle) {
				// No need to lock. Called from the main thread only when the job has finished already.
				// --> std::scoped_lock<std::mutex> lock(m_jobsLock);
				auto& job = m_jobs.free(jobHandle);
				job.state = JobInternalState::Released;
			}

			//! Allocates a new dependency identified by a unique DepHandle.
			//! \return DepHandle
			//! \warning Must be used from the main thread.
			GAIA_NODISCARD DepHandle alloc_dep() {
				return m_deps.alloc();
			}

			//! Invalidates \param depHandle by resetting its index in the dependency pool.
			//! Everytime a dependency is deallocated its generation is increased by one.
			//! \warning Must be used from the main thread.
			void free_dep(DepHandle depHandle) {
				m_deps.free(depHandle);
			}

			//! Resets the job pool.
			void reset() {
				{
					// No need to lock. Called from the main thread only when all jobs have finished already.
					// --> std::scoped_lock<std::mutex> lock(m_jobsLock);
					m_jobs.clear();
				}
				{
					// No need to lock. Called from the main thread only when all jobs must have ended already.
					// --> std::scoped_lock<std::mutex> lock(m_depsLock);
					m_deps.clear();
				}
			}

			void run(JobHandle jobHandle) {
				std::function<void()> func;

				{
					std::scoped_lock<std::mutex> lock(m_jobsLock);
					auto& job = m_jobs[jobHandle.id()];
					job.state = JobInternalState::Running;
					func = job.func;
				}
				if (func.operator bool())
					func();
				{
					std::scoped_lock<std::mutex> lock(m_jobsLock);
					auto& job = m_jobs[jobHandle.id()];
					job.state = JobInternalState::Done;
				}
			}

			//! Evaluates job dependencies.
			//! \return True if job dependencies are met. False otherwise
			GAIA_NODISCARD bool handle_deps(JobHandle jobHandle) {
				GAIA_PROF_SCOPE(JobManager::HandleDeps);
				std::scoped_lock<std::mutex> lockJobs(m_jobsLock);
				auto& job = m_jobs[jobHandle.id()];
				if (job.dependencyIdx == (uint32_t)-1)
					return true;

				uint32_t depsId = job.dependencyIdx;
				{
					std::scoped_lock<std::mutex> lockDeps(m_depsLock);

					// Iterate over all dependencies.
					// The first busy dependency breaks the loop. At this point we also update
					// the initial dependency index because we know all previous dependencies
					// have already finished and there's no need to check them.
					do {
						JobDependency dep = m_deps[depsId];
						if (!Isdone(dep.dependsOn)) {
							m_jobs[jobHandle.id()].dependencyIdx = depsId;
							return false;
						}

						depsId = dep.dependencyIdxNext;
					} while (depsId != (uint32_t)-1);
				}

				// No need to update the index because once we return true we execute the job.
				// --> job.dependencyIdx = JobHandleInvalid.idx;
				return true;
			}

			//! Makes \param jobHandle depend on \param dependsOn.
			//! This means \param jobHandle will run only after \param dependsOn finishes.
			//! \warning Must be used from the main thread.
			//! \warning Needs to be called before any of the listed jobs are scheduled.
			void dep(JobHandle jobHandle, JobHandle dependsOn) {
				std::scoped_lock<std::mutex> lockJobs(m_jobsLock);
				auto& job = m_jobs[jobHandle.id()];

#if GAIA_ASSERT_ENABLED
				GAIA_ASSERT(jobHandle != dependsOn);
				GAIA_ASSERT(!busy(jobHandle));
				GAIA_ASSERT(!busy(dependsOn));
#endif

				{
					GAIA_PROF_SCOPE(JobManager::AddDep);
					std::scoped_lock<std::mutex> lockDeps(m_depsLock);

					auto depHandle = alloc_dep();
					auto& dep = m_deps[depHandle.id()];
					dep.dependsOn = dependsOn;

					if (job.dependencyIdx == (uint32_t)-1)
						// First time adding a dependency to this job. Point it to the first allocated handle
						dep.dependencyIdxNext = (uint32_t)-1;
					else
						// We have existing dependencies. Point the last known one to the first allocated handle
						dep.dependencyIdxNext = job.dependencyIdx;

					job.dependencyIdx = depHandle.id();
				}
			}

			//! Makes \param jobHandle depend on the jobs listed in \param dependsOnSpan.
			//! This means \param jobHandle will run only after all \param dependsOnSpan jobs finish.
			//! \warning Must be used from the main thread.
			//! \warning Needs to be called before any of the listed jobs are scheduled.
			void dep(JobHandle jobHandle, std::span<const JobHandle> dependsOnSpan) {
				if (dependsOnSpan.empty())
					return;

				auto& job = m_jobs[jobHandle.id()];

#if GAIA_ASSERT_ENABLED
				GAIA_ASSERT(!busy(jobHandle));
				for (auto dependsOn: dependsOnSpan) {
					GAIA_ASSERT(jobHandle != dependsOn);
					GAIA_ASSERT(!busy(dependsOn));
				}
#endif

				GAIA_PROF_SCOPE(JobManager::AddDeps);
				std::scoped_lock<std::mutex> lockJobs(m_jobsLock);
				{
					std::scoped_lock<std::mutex> lockDeps(m_depsLock);

					for (uint32_t i = 0; i < dependsOnSpan.size(); ++i) {
						auto depHandle = alloc_dep();
						auto& dep = m_deps[depHandle.id()];
						dep.dependsOn = dependsOnSpan[i];

						if (job.dependencyIdx == (uint32_t)-1)
							// First time adding a dependency to this job. Point it to the first allocated handle
							dep.dependencyIdxNext = (uint32_t)-1;
						else
							// We have existing dependencies. Point the last known one to the first allocated handle
							dep.dependencyIdxNext = job.dependencyIdx;

						job.dependencyIdx = depHandle.id();
					}
				}
			}

			void submit(JobHandle jobHandle) {
				auto& job = m_jobs[jobHandle.id()];
				GAIA_ASSERT(job.state < JobInternalState::Submitted);
				job.state = JobInternalState::Submitted;
			}

			void resubmit(JobHandle jobHandle) {
				auto& job = m_jobs[jobHandle.id()];
				GAIA_ASSERT(job.state <= JobInternalState::Submitted);
				job.state = JobInternalState::Submitted;
			}

			GAIA_NODISCARD bool busy(JobHandle jobHandle) const {
				const auto& job = m_jobs[jobHandle.id()];
				return ((uint32_t)job.state & (uint32_t)JobInternalState::Busy) != 0;
			}

			GAIA_NODISCARD bool Isdone(JobHandle jobHandle) const {
				const auto& job = m_jobs[jobHandle.id()];
				return ((uint32_t)job.state & (uint32_t)JobInternalState::Done) != 0;
			}
		};
	} // namespace mt
} // namespace gaia

#define JOB_QUEUE_USE_LOCKS 1
#if JOB_QUEUE_USE_LOCKS
	
	#include <mutex>
#endif

namespace gaia {
	namespace mt {
		class JobQueue {
			static constexpr uint32_t N = 1 << 12;
#if !JOB_QUEUE_USE_LOCKS
			static constexpr uint32_t MASK = N - 1;
#endif

#if JOB_QUEUE_USE_LOCKS
			std::mutex m_bufferLock;
			cnt::sringbuffer<JobHandle, N> m_buffer;
#else
			cnt::sarray<JobHandle, N> m_buffer;
			std::atomic_uint32_t m_bottom{};
			std::atomic_uint32_t m_top{};
#endif

		public:
			//! Tries adding a job to the queue. FIFO.
			//! \return True if the job was added. False otherwise (e.g. maximum capacity has been reached).
			GAIA_NODISCARD bool try_push(JobHandle jobHandle) {
				GAIA_PROF_SCOPE(JobQueue::TryPush);

#if JOB_QUEUE_USE_LOCKS
				std::scoped_lock<std::mutex> lock(m_bufferLock);
				if (m_buffer.size() >= m_buffer.max_size())
					return false;
				m_buffer.push_back(jobHandle);
#else
				const uint32_t b = m_bottom.load(std::memory_order_acquire);

				if (b >= m_buffer.size())
					return false;

				m_buffer[b & MASK] = jobHandle;

				// Make sure the handle is written before we update the bottom
				std::atomic_thread_fence(std::memory_order_release);

				m_bottom.store(b + 1, std::memory_order_release);
#endif

				return true;
			}

			//! Tries retriving a job to the queue. FIFO.
			//! \return True if the job was retrived. False otherwise (e.g. there are no jobs).
			GAIA_NODISCARD bool try_pop(JobHandle& jobHandle) {
				GAIA_PROF_SCOPE(JobQueue::try_pop);

#if JOB_QUEUE_USE_LOCKS
				std::scoped_lock<std::mutex> lock(m_bufferLock);
				if (m_buffer.empty())
					return false;

				m_buffer.pop_front(jobHandle);
#else
				uint32_t b = m_bottom.load(std::memory_order_acquire);
				if (b > 0) {
					b = b - 1;
					m_bottom.store(b, std::memory_order_release);
				}

				std::atomic_thread_fence(std::memory_order_release);

				uint32_t t = m_top.load(std::memory_order_acquire);

				// Queue already empty
				if (t > b) {
					m_bottom.store(t, std::memory_order_release);
					return false;
				}

				jobHandle = m_buffer[b & MASK];

				// The last item in the queue
				if (t == b) {
					if (t == 0) {
						return false; // Queue is empty, nothing to pop
					}
					// Should multiple thread fight for the last item the atomic
					// CAS ensures this last item is extracted only once.

					uint32_t expectedTop = t;
					const uint32_t nextTop = t + 1;
					const uint32_t desiredTop = nextTop;

					bool ret = m_top.compare_exchange_strong(expectedTop, desiredTop, std::memory_order_acq_rel);
					m_bottom.store(nextTop, std::memory_order_release);
					return ret;
				}
#endif

				return true;
			}

			//! Tries stealing a job from the queue. LIFO.
			//! \return True if the job was stolen. False otherwise (e.g. there are no jobs).
			GAIA_NODISCARD bool try_steal(JobHandle& jobHandle) {
				GAIA_PROF_SCOPE(JobQueue::try_steal);

#if JOB_QUEUE_USE_LOCKS
				std::scoped_lock<std::mutex> lock(m_bufferLock);
				if (m_buffer.empty())
					return false;

				m_buffer.pop_back(jobHandle);
#else
				uint32_t t = m_top.load(std::memory_order_acquire);

				std::atomic_thread_fence(std::memory_order_release);

				uint32_t b = m_bottom.load(std::memory_order_acquire);

				// Return false when empty
				if (t >= b)
					return false;

				jobHandle = m_buffer[t & MASK];

				const uint32_t tNext = t + 1;
				uint32_t tDesired = tNext;
				// We fail if concurrent pop()/steal() operation changed the current top
				if (!m_top.compare_exchange_weak(t, tDesired, std::memory_order_acq_rel))
					return false;
#endif

				return true;
			}
		};
	} // namespace mt
} // namespace gaia

namespace gaia {
	namespace mt {

		class ThreadPool final {
			static constexpr uint32_t MaxWorkers = 32;

			//! ID of the main thread
			std::thread::id m_mainThreadId;
			//! List of worker threads
			cnt::sarr_ext<std::thread, MaxWorkers> m_workers;
			//! Manager for internal jobs
			JobManager m_jobManager;
			//! List of pending user jobs
			JobQueue m_jobQueue;

			//! How many jobs are currently being processed
			std::atomic_uint32_t m_jobsPending;

			std::mutex m_cvLock;
			std::condition_variable m_cv;

			//! When true the pool is supposed to finish all work and terminate all threads
			bool m_stop;

		private:
			ThreadPool(): m_jobsPending(0), m_stop(false) {
				uint32_t workersCnt = calc_thread_cnt(0);
				if (workersCnt > MaxWorkers)
					workersCnt = MaxWorkers;

				m_workers.resize(workersCnt);

				m_mainThreadId = std::this_thread::get_id();

				for (uint32_t i = 0; i < workersCnt; ++i) {
					m_workers[i] = std::thread([this, i]() {
						// Set the worker thread name.
						// Needs to be called from inside the thread because some platforms
						// can change the name only when run from the specific thread.
						set_thread_name(i);

						// Process jobs
						worker_loop();
					});

					// Stick each thread to a specific CPU core
					set_thread_affinity(i);
				}
			}

			ThreadPool(ThreadPool&&) = delete;
			ThreadPool(const ThreadPool&) = delete;
			ThreadPool& operator=(ThreadPool&&) = delete;
			ThreadPool& operator=(const ThreadPool&) = delete;

		public:
			static ThreadPool& get() {
				static ThreadPool threadPool;
				return threadPool;
			}

			GAIA_NODISCARD uint32_t workers() const {
				return m_workers.size();
			}

			~ThreadPool() {
				reset();
			}

			//! Makes \param jobHandle depend on \param dependsOn.
			//! This means \param jobHandle will run only after \param dependsOn finishes.
			//! \warning Must be used from the main thread.
			//! \warning Needs to be called before any of the listed jobs are scheduled.
			void dep(JobHandle jobHandle, JobHandle dependsOn) {
				m_jobManager.dep(jobHandle, dependsOn);
			}

			//! Makes \param jobHandle depend on the jobs listed in \param dependsOnSpan.
			//! This means \param jobHandle will run only after all \param dependsOnSpan jobs finish.
			//! \warning Must be used from the main thread.
			//! \warning Needs to be called before any of the listed jobs are scheduled.
			void dep(JobHandle jobHandle, std::span<const JobHandle> dependsOnSpan) {
				m_jobManager.dep(jobHandle, dependsOnSpan);
			}

			//! Creates a job system job from \param job.
			//! \warning Must be used from the main thread.
			//! \return Job handle of the scheduled job.
			JobHandle add(const Job& job) {
				GAIA_ASSERT(main_thread());

				// Don't add new jobs once stop was requested
				if GAIA_UNLIKELY (m_stop)
					return JobNull;

				++m_jobsPending;

				return m_jobManager.alloc_job(job);
			}

			//! Pushes \param jobHandle into the internal queue so worker threads
			//! can pick it up and execute it.
			//! If there are more jobs than the queue can handle it puts the calling
			//! thread to sleep until workers consume enough jobs.
			//! \warning Once submited, dependencies can't be modified for this job.
			void submit(JobHandle jobHandle) {
				m_jobManager.submit(jobHandle);

				// Try pushing a new job until it we succeed.
				// The thread is put to sleep if pushing the jobs fails.
				while (!m_jobQueue.try_push(jobHandle))
					poll();

				// Wake some worker thread
				m_cv.notify_one();
			}

		private:
			//! Resubmits \param jobHandle into the internal queue so worker threads
			//! can pick it up and execute it.
			//! If there are more jobs than the queue can handle it puts the calling
			//! thread to sleep until workers consume enough jobs.
			//! \warning Internal usage only. Only worker theads can decide to resubmit.
			void resubmit(JobHandle jobHandle) {
				m_jobManager.resubmit(jobHandle);

				// Try pushing a new job until it we succeed.
				// The thread is put to sleep if pushing the jobs fails.
				while (!m_jobQueue.try_push(jobHandle))
					poll();

				// Wake some worker thread
				m_cv.notify_one();
			}

		public:
			//! Schedules a job to run on a worker thread.
			//! \param job Job descriptor
			//! \warning Must be used from the main thread.
			//! \warning Dependencies can't be modified for this job.
			//! \return Job handle of the scheduled job.
			JobHandle sched(const Job& job) {
				JobHandle jobHandle = add(job);
				submit(jobHandle);
				return jobHandle;
			}

			//! Schedules a job to run on a worker thread.
			//! \param job Job descriptor
			//! \param dependsOn Job we depend on
			//! \warning Must be used from the main thread.
			//! \warning Dependencies can't be modified for this job.
			//! \return Job handle of the scheduled job.
			JobHandle sched(const Job& job, JobHandle dependsOn) {
				JobHandle jobHandle = add(job);
				dep(jobHandle, dependsOn);
				submit(jobHandle);
				return jobHandle;
			}

			//! Schedules a job to run on a worker thread.
			//! \param job Job descriptor
			//! \param dependsOnSpan Jobs we depend on
			//! \warning Must be used from the main thread.
			//! \warning Dependencies can't be modified for this job.
			//! \return Job handle of the scheduled job.
			JobHandle sched(const Job& job, std::span<const JobHandle> dependsOnSpan) {
				JobHandle jobHandle = add(job);
				dep(jobHandle, dependsOnSpan);
				submit(jobHandle);
				return jobHandle;
			}

			//! Schedules a job to run on worker threads in parallel.
			//! \param job Job descriptor
			//! \param itemsToProcess Total number of work items
			//! \param groupSize Group size per created job. If zero the job system decides the group size.
			//! \warning Must be used from the main thread.
			//! \warning Dependencies can't be modified for this job.
			//! \return Job handle of the scheduled batch of jobs.
			JobHandle sched_par(const JobParallel& job, uint32_t itemsToProcess, uint32_t groupSize) {
				GAIA_ASSERT(main_thread());

				// Empty data set are considered wrong inputs
				GAIA_ASSERT(itemsToProcess != 0);
				if (itemsToProcess == 0)
					return JobNull;

				// Don't add new jobs once stop was requested
				if GAIA_UNLIKELY (m_stop)
					return JobNull;

				const uint32_t workerCount = m_workers.size();

				// No group size was given, make a guess based on the set size
				if (groupSize == 0)
					groupSize = (itemsToProcess + workerCount - 1) / workerCount;

				const auto jobs = (itemsToProcess + groupSize - 1) / groupSize;
				// Internal jobs + 1 for the groupHandle
				m_jobsPending += (jobs + 1U);

				JobHandle groupHandle = m_jobManager.alloc_job({});

				for (uint32_t jobIndex = 0; jobIndex < jobs; ++jobIndex) {
					// Create one job per group
					auto groupJobFunc = [job, itemsToProcess, groupSize, jobIndex]() {
						const uint32_t groupJobIdxStart = jobIndex * groupSize;
						const uint32_t groupJobIdxStartPlusGroupSize = groupJobIdxStart + groupSize;
						const uint32_t groupJobIdxEnd =
								groupJobIdxStartPlusGroupSize < itemsToProcess ? groupJobIdxStartPlusGroupSize : itemsToProcess;

						JobArgs args;
						args.idxStart = groupJobIdxStart;
						args.idxEnd = groupJobIdxEnd;
						job.func(args);
					};

					JobHandle jobHandle = m_jobManager.alloc_job({groupJobFunc});
					dep(groupHandle, jobHandle);
					submit(jobHandle);
				}

				submit(groupHandle);
				return groupHandle;
			}

			//! Wait for the job to finish.
			//! \param jobHandle Job handle
			//! \warning Must be used from the main thread.
			void wait(JobHandle jobHandle) {
				GAIA_ASSERT(main_thread());

				while (m_jobManager.busy(jobHandle))
					poll();

				GAIA_ASSERT(!m_jobManager.busy(jobHandle));
				m_jobManager.wait(jobHandle);
			}

			//! Wait for all jobs to finish.
			//! \warning Must be used from the main thread.
			void wait_all() {
				GAIA_ASSERT(main_thread());

				while (busy())
					poll_all();

				GAIA_ASSERT(m_jobsPending == 0);
				m_jobManager.reset();
			}

		private:
			void set_thread_affinity(uint32_t threadID) {
				// TODO:
				// Some cores might have multiple logic threads, there might be
				// more socket and some cores might even be physically different
				// form others (performance vs efficiency cores).
				// Therefore, we either need some more advanced logic here or we
				// should completly drop the idea of assigning affinity and simply
				// let the OS scheduler figure things out.
#if GAIA_PLATFORM_WINDOWS
				auto nativeHandle = (HANDLE)m_workers[threadID].native_handle();

				auto mask = SetThreadAffinityMask(nativeHandle, 1ULL << threadID);
				if (mask <= 0)
					GAIA_LOG_W("Issue setting thread affinity for worker thread %u!", threadID);
#elif GAIA_PLATFORM_APPLE
				pthread_t nativeHandle = (pthread_t)m_workers[threadID].native_handle();

				mach_port_t mach_thread = pthread_mach_thread_np(nativeHandle);
				thread_affinity_policy_data_t policy_data = {(int)threadID};
				auto ret = thread_policy_set(
						mach_thread, THREAD_AFFINITY_POLICY, (thread_policy_t)&policy_data, THREAD_AFFINITY_POLICY_COUNT);
				if (ret == 0)
					GAIA_LOG_W("Issue setting thread affinity for worker thread %u!", threadID);
#elif GAIA_PLATFORM_LINUX || GAIA_PLATFORM_FREEBSD
				pthread_t nativeHandle = (pthread_t)m_workers[threadID].native_handle();

				cpu_set_t cpuset;
				CPU_ZERO(&cpuset);
				CPU_SET(threadID, &cpuset);

				auto ret = pthread_setaffinity_np(nativeHandle, sizeof(cpuset), &cpuset);
				if (ret != 0)
					GAIA_LOG_W("Issue setting thread affinity for worker thread %u!", threadID);

				ret = pthread_getaffinity_np(nativeHandle, sizeof(cpuset), &cpuset);
				if (ret != 0)
					GAIA_LOG_W("Thread affinity could not be set for worker thread %u!", threadID);
#endif
			}

			void set_thread_name(uint32_t threadID) {
#if GAIA_PLATFORM_WINDOWS
				auto nativeHandle = (HANDLE)m_workers[threadID].native_handle();

				wchar_t threadName[10]{};
				swprintf_s(threadName, L"worker_%u", threadID);
				auto hr = SetThreadDescription(nativeHandle, threadName);
				if (FAILED(hr))
					GAIA_LOG_W("Issue setting worker thread name!");
#elif GAIA_PLATFORM_APPLE
				char threadName[10]{};
				snprintf(threadName, 10, "worker_%u", threadID);
				auto ret = pthread_setname_np(threadName);
				if (ret != 0)
					GAIA_LOG_W("Issue setting name for worker thread %u!", threadID);
#elif GAIA_PLATFORM_LINUX || GAIA_PLATFORM_FREEBSD
				auto nativeHandle = (pthread_t)m_workers[threadID].native_handle();

				char threadName[10]{};
				snprintf(threadName, 10, "worker_%u", threadID);
				auto ret = pthread_setname_np(nativeHandle, threadName);
				if (ret != 0)
					GAIA_LOG_W("Issue setting name for worker thread %u!", threadID);
#endif
			}

			GAIA_NODISCARD bool main_thread() const {
				return std::this_thread::get_id() == m_mainThreadId;
			}

			GAIA_NODISCARD static uint32_t calc_thread_cnt(uint32_t threadsWanted) {
				// Make sure a reasonable amount of threads is used
				if (threadsWanted == 0) {
					// Subtract one (the main thread)
					threadsWanted = std::thread::hardware_concurrency() - 1;
					if (threadsWanted <= 0)
						threadsWanted = 1;
				}
				return threadsWanted;
			}

			void worker_loop() {
				while (!m_stop) {
					JobHandle jobHandle;

					if (!m_jobQueue.try_pop(jobHandle)) {
						std::unique_lock<std::mutex> lock(m_cvLock);
						m_cv.wait(lock);
						continue;
					}

					GAIA_ASSERT(m_jobsPending > 0);

					// Make sure we can execute the job.
					// If it has dependencies which were not completed we need to reschedule
					// and come back to it later.
					if (!m_jobManager.handle_deps(jobHandle)) {
						resubmit(jobHandle);
						continue;
					}

					m_jobManager.run(jobHandle);
					--m_jobsPending;
				}
			}

			void reset() {
				// Request stopping
				m_stop = true;
				// complete all remaining work
				wait_all();
				// Wake up any threads that were put to sleep
				m_cv.notify_all();
				// Join threads with the main one
				for (auto& w: m_workers) {
					if (w.joinable())
						w.join();
				}
			}

			//! Checks whether workers are busy doing work.
			//!	\return True if any workers are busy doing work.
			GAIA_NODISCARD bool busy() const {
				return m_jobsPending > 0;
			}

			//! Wakes up some worker thread and reschedules the current one.
			void poll() {
				// Wake some worker thread
				m_cv.notify_one();

				// Allow this thread to be rescheduled
				std::this_thread::yield();
			}

			//! Wakes up all worker threads and reschedules the current one.
			void poll_all() {
				// Wake some worker thread
				m_cv.notify_all();

				// Allow this thread to be rescheduled
				std::this_thread::yield();
			}
		};
	} // namespace mt
} // namespace gaia

#include <cinttypes>

#include <cinttypes>

namespace gaia {
	namespace ecs {
		class Archetype;

		using ArchetypeId = uint32_t;
		using ArchetypeList = cnt::darray<Archetype*>;

		static constexpr ArchetypeId ArchetypeIdBad = (ArchetypeId)-1;
	} // namespace ecs
} // namespace gaia

#include <cinttypes>

#include <cinttypes>

namespace gaia {
	namespace ecs {
		enum ComponentKind : uint8_t {
			// General purpose component
			CK_Generic = 0,
			// Chunk component
			CK_Chunk,
			// Number of component types
			CK_Count
		};

		inline const char* const ComponentKindString[ComponentKind::CK_Count] = {"Generic", "Chunk"};

		using ComponentId = uint32_t;
		using ComponentLookupHash = core::direct_hash_key<uint64_t>;
		using ComponentMatcherHash = core::direct_hash_key<uint64_t>;
		using ComponentIdSpan = std::span<const ComponentId>;

		static constexpr ComponentId ComponentIdBad = (ComponentId)-1;
		static constexpr uint32_t MAX_COMPONENTS_SIZE_BITS = 8;
		static constexpr uint32_t MAX_COMPONENTS_SIZE_IN_BYTES = (1 << MAX_COMPONENTS_SIZE_BITS) - 1;

		//----------------------------------------------------------------------
		// Component type deduction
		//----------------------------------------------------------------------

		namespace detail {
			template <typename, typename = void>
			struct has_component_kind: std::false_type {};
			template <typename T>
			struct has_component_kind<T, std::void_t<decltype(T::TComponentKind)>>: std::true_type {};

			template <typename T>
			struct ExtractComponentKind_NoComponentKind {
				using Kind = typename std::decay_t<typename std::remove_pointer_t<T>>;
				using KindOriginal = T;
				static constexpr ComponentKind TComponentKind = ComponentKind::CK_Generic;
			};
			template <typename T>
			struct ExtractComponentKind_WithComponentKind {
				using Kind = typename T::TKind;
				using KindOriginal = typename T::TKindOriginal;
				static constexpr ComponentKind TComponentKind = T::TComponentKind;
			};

			template <typename, typename = void>
			struct is_generic_component: std::true_type {};
			template <typename T>
			struct is_generic_component<T, std::void_t<decltype(T::TComponentKind)>>:
					std::bool_constant<T::TComponentKind == ComponentKind::CK_Generic> {};

			template <typename T>
			struct is_component_size_valid: std::bool_constant<sizeof(T) < MAX_COMPONENTS_SIZE_IN_BYTES> {};

			template <typename T>
			struct is_component_kind_valid:
					std::bool_constant<
							// SoA types need to be trivial. No restrictions otherwise.
							(!mem::is_soa_layout_v<T> || std::is_trivially_copyable_v<T>)> {};

			template <typename T, typename = void>
			struct component_kind {
				using type = typename detail::ExtractComponentKind_NoComponentKind<T>;
			};
			template <typename T>
			struct component_kind<T, std::void_t<decltype(T::TComponentKind)>> {
				using type = typename detail::ExtractComponentKind_WithComponentKind<T>;
			};

			template <typename T>
			struct is_component_mut:
					std::bool_constant<
							!std::is_const_v<std::remove_reference_t<std::remove_pointer_t<T>>> &&
							(std::is_pointer<T>::value || std::is_reference<T>::value)> {};
		} // namespace detail

		template <typename T>
		inline constexpr bool is_component_size_valid_v = detail::is_component_size_valid<T>::value;
		template <typename T>
		inline constexpr bool is_component_kind_valid_v = detail::is_component_kind_valid<T>::value;

		template <typename T>
		using component_kind_t = typename detail::component_kind<T>::type;
		template <typename T>
		inline constexpr ComponentKind component_kind_v = component_kind_t<T>::TComponentKind;

		//! Returns the component id for \tparam T
		//! \return Component id
		template <typename T>
		GAIA_NODISCARD inline ComponentId comp_id() {
			using U = typename component_kind_t<T>::Kind;
			return meta::type_info::id<U>();
		}

		template <typename T>
		inline constexpr bool is_component_mut_v = detail::is_component_mut<T>::value;

		//----------------------------------------------------------------------
		// Component verification
		//----------------------------------------------------------------------

		template <typename T>
		constexpr void verify_comp() {
			using U = typename component_kind_t<T>::Kind;
			// Make sure we only use this for "raw" types
			static_assert(!std::is_const_v<U>);
			static_assert(!std::is_pointer_v<U>);
			static_assert(!std::is_reference_v<U>);
			static_assert(!std::is_volatile_v<U>);
			static_assert(is_component_size_valid_v<U>, "MAX_COMPONENTS_SIZE_IN_BYTES in bytes is exceeded");
			static_assert(is_component_kind_valid_v<U>, "Component type restrictions not met");
		}

		//----------------------------------------------------------------------
		// Component hash operations
		//----------------------------------------------------------------------

		namespace detail {
			template <typename T>
			constexpr uint64_t calc_matcher_hash() noexcept {
				return (uint64_t(1) << (meta::type_info::hash<T>() % uint64_t(63)));
			}
		} // namespace detail

		template <typename = void, typename...>
		constexpr ComponentMatcherHash calc_matcher_hash() noexcept;

		template <typename T, typename... Rest>
		GAIA_NODISCARD constexpr ComponentMatcherHash calc_matcher_hash() noexcept {
			if constexpr (sizeof...(Rest) == 0)
				return {detail::calc_matcher_hash<T>()};
			else
				return {core::combine_or(detail::calc_matcher_hash<T>(), detail::calc_matcher_hash<Rest>()...)};
		}

		template <>
		GAIA_NODISCARD constexpr ComponentMatcherHash calc_matcher_hash() noexcept {
			return {0};
		}

		//-----------------------------------------------------------------------------------

		template <typename Container>
		GAIA_NODISCARD constexpr ComponentLookupHash calc_lookup_hash(Container arr) noexcept {
			constexpr auto arrSize = arr.size();
			if constexpr (arrSize == 0) {
				return {0};
			} else {
				ComponentLookupHash::Type hash = arr[0];
				core::each<arrSize - 1>([&hash, &arr](auto i) {
					hash = core::hash_combine(hash, arr[i + 1]);
				});
				return {hash};
			}
		}

		template <typename = void, typename...>
		constexpr ComponentLookupHash calc_lookup_hash() noexcept;

		template <typename T, typename... Rest>
		GAIA_NODISCARD constexpr ComponentLookupHash calc_lookup_hash() noexcept {
			if constexpr (sizeof...(Rest) == 0)
				return {meta::type_info::hash<T>()};
			else
				return {core::hash_combine(meta::type_info::hash<T>(), meta::type_info::hash<Rest>()...)};
		}

		template <>
		GAIA_NODISCARD constexpr ComponentLookupHash calc_lookup_hash() noexcept {
			return {0};
		}

		template <typename T>
		struct AsChunk {
			using TKind = typename std::decay_t<typename std::remove_pointer_t<T>>;
			using TKindOriginal = T;
			static constexpr ComponentKind TComponentKind = ComponentKind::CK_Chunk;
		};
	} // namespace ecs
} // namespace gaia

#include <cinttypes>
#include <type_traits>

#include <cinttypes>
#include <tuple>
#include <type_traits>

namespace gaia {
	namespace ecs {
		struct ComponentDesc final {
			static constexpr uint32_t MaxAlignment_Bits = 10;
			static constexpr uint32_t MaxAlignment = (1U << MaxAlignment_Bits) - 1;

			using FuncCtor = void(void*, uint32_t);
			using FuncDtor = void(void*, uint32_t);
			using FuncCopy = void(void*, void*);
			using FuncMove = void(void*, void*);
			using FuncSwap = void(void*, void*);

			//! Unique component identifier
			ComponentId compId = ComponentIdBad;

			//! Various component properties
			struct {
				//! Component alignment
				uint32_t alig: MaxAlignment_Bits;
				//! Component size
				uint32_t size: MAX_COMPONENTS_SIZE_BITS;
				//! SOA variables. If > 0 the component is laid out in SoA style
				uint32_t soa: meta::StructToTupleMaxTypes_Bits;
			} properties{};

			uint8_t soaSizes[meta::StructToTupleMaxTypes];

			//! Component name
			std::span<const char> name;
			//! Function to call when the component needs to be constructed
			FuncCtor* func_ctor = nullptr;
			//! Function to call when the component needs to be move constructed
			FuncMove* func_ctor_move = nullptr;
			//! Function to call when the component needs to be copy constructed
			FuncCopy* func_ctor_copy = nullptr;
			//! Function to call when the component needs to be destroyed
			FuncDtor* func_dtor = nullptr;
			//! Function to call when the component needs to be copied
			FuncCopy* func_copy = nullptr;
			//! Fucntion to call when the component needs to be moved
			FuncMove* func_move = nullptr;
			//! Function to call when the component needs to swap
			FuncSwap* func_swap = nullptr;

			void ctor_from(void* pSrc, void* pDst) const {
				if (func_ctor_move != nullptr)
					func_ctor_move(pSrc, pDst);
				else if (func_ctor_copy != nullptr)
					func_ctor_copy(pSrc, pDst);
				else
					memmove(pDst, (const void*)pSrc, properties.size);
			}

			void move(void* pSrc, void* pDst) const {
				if (func_move != nullptr)
					func_move(pSrc, pDst);
				else
					copy(pSrc, pDst);
			}

			void copy(void* pSrc, void* pDst) const {
				if (func_copy != nullptr)
					func_copy(pSrc, pDst);
				else
					memmove(pDst, (const void*)pSrc, properties.size);
			}

			void dtor(void* pSrc) const {
				if (func_dtor != nullptr)
					func_dtor(pSrc, 1);
			}

			void swap(void* pLeft, void* pRight) const {
				func_swap(pLeft, pRight);
			}

			GAIA_NODISCARD uint32_t calc_new_mem_offset(uint32_t addr, size_t N) const noexcept {
				if (properties.soa == 0) {
					addr = (uint32_t)mem::detail::get_aligned_byte_offset(addr, properties.alig, properties.size, N);
				} else {
					for (uint32_t i = 0; i < (uint32_t)properties.soa; ++i)
						addr = (uint32_t)mem::detail::get_aligned_byte_offset(addr, properties.alig, soaSizes[i], N);
				}
				return addr;
			}

			template <typename T>
			GAIA_NODISCARD static constexpr ComponentDesc build() {
				using U = typename component_kind_t<T>::Kind;

				ComponentDesc info{};
				info.compId = comp_id<T>();
				info.name = meta::type_info::name<U>();

				if constexpr (!std::is_empty_v<U>) {
					info.properties.size = (uint32_t)sizeof(U);

					static_assert(MaxAlignment_Bits, "Maximum supported alignemnt for a component is MaxAlignment");
					info.properties.alig = (uint32_t)mem::auto_view_policy<U>::Alignment;

					if constexpr (mem::is_soa_layout_v<U>) {
						uint32_t i = 0;
						using TTuple = decltype(meta::struct_to_tuple(U{}));
						core::each_tuple(TTuple{}, [&](auto&& item) {
							static_assert(sizeof(item) <= 255, "Each member of a SoA component can be at most 255 B long!");
							info.soaSizes[i] = (uint8_t)sizeof(item);
							++i;
						});
						info.properties.soa = i;
						GAIA_ASSERT(i <= meta::StructToTupleMaxTypes);
					} else {
						info.properties.soa = 0U;

						// Custom construction
						if constexpr (!std::is_trivially_constructible_v<U>) {
							info.func_ctor = [](void* ptr, uint32_t cnt) {
								core::call_ctor((U*)ptr, cnt);
							};
						}

						// Custom destruction
						if constexpr (!std::is_trivially_destructible_v<U>) {
							info.func_dtor = [](void* ptr, uint32_t cnt) {
								core::call_dtor((U*)ptr, cnt);
							};
						}

						// Copyability
						if (!std::is_trivially_copyable_v<U>) {
							if constexpr (std::is_copy_assignable_v<U>) {
								info.func_copy = [](void* from, void* to) {
									auto* src = (U*)from;
									auto* dst = (U*)to;
									*dst = *src;
								};
								info.func_ctor_copy = [](void* from, void* to) {
									auto* src = (U*)from;
									auto* dst = (U*)to;
									new (dst) U();
									*dst = *src;
								};
							} else if constexpr (std::is_copy_constructible_v<U>) {
								info.func_copy = [](void* from, void* to) {
									auto* src = (U*)from;
									auto* dst = (U*)to;
									*dst = U(*src);
								};
								info.func_ctor_copy = [](void* from, void* to) {
									auto* src = (U*)from;
									auto* dst = (U*)to;
									(void)new (dst) U(std::move(*src));
								};
							}
						}

						// Movability
						if constexpr (!std::is_trivially_move_assignable_v<U> && std::is_move_assignable_v<U>) {
							info.func_move = [](void* from, void* to) {
								auto* src = (U*)from;
								auto* dst = (U*)to;
								*dst = std::move(*src);
							};
							info.func_ctor_move = [](void* from, void* to) {
								auto* src = (U*)from;
								auto* dst = (U*)to;
								new (dst) U();
								*dst = std::move(*src);
							};
						} else if constexpr (!std::is_trivially_move_constructible_v<U> && std::is_move_constructible_v<U>) {
							info.func_move = [](void* from, void* to) {
								auto* src = (U*)from;
								auto* dst = (U*)to;
								*dst = U(std::move(*src));
							};
							info.func_ctor_move = [](void* from, void* to) {
								auto* src = (U*)from;
								auto* dst = (U*)to;
								(void)new (dst) U(std::move(*src));
							};
						}
					}

					// Value swap
					if constexpr (std::is_move_constructible_v<U> && std::is_move_assignable_v<U>) {
						info.func_swap = [](void* left, void* right) {
							auto* l = (U*)left;
							auto* r = (U*)right;
							U tmp = std::move(*l);
							*r = std::move(*l);
							*r = std::move(tmp);
						};
					} else {
						info.func_swap = [](void* left, void* right) {
							auto* l = (U*)left;
							auto* r = (U*)right;
							U tmp = *l;
							*r = *l;
							*r = tmp;
						};
					}
				}

				return info;
			}

			template <typename T>
			GAIA_NODISCARD static ComponentDesc create() {
				using U = std::decay_t<T>;
				return ComponentDesc::build<U>();
			}
		};
	} // namespace ecs
} // namespace gaia

#include <type_traits>

namespace gaia {
	namespace ecs {
		struct ComponentInfo final {
			//! Complex hash used for look-ups
			ComponentLookupHash lookupHash;
			//! Simple hash used for matching component
			ComponentMatcherHash matcherHash;
			//! Unique component identifier
			ComponentId compId;

			GAIA_NODISCARD bool operator==(const ComponentInfo& other) const {
				GAIA_ASSERT(lookupHash == other.lookupHash);
				return compId == other.compId;
			}
			GAIA_NODISCARD bool operator!=(const ComponentInfo& other) const {
				GAIA_ASSERT(lookupHash != other.lookupHash);
				return compId != other.compId;
			}
			GAIA_NODISCARD bool operator<(const ComponentInfo& other) const {
				return compId < other.compId;
			}

			template <typename T>
			GAIA_NODISCARD static constexpr ComponentInfo init() {
				using U = typename component_kind_t<T>::Kind;

				ComponentInfo info{};
				info.lookupHash = {meta::type_info::hash<U>()};
				info.matcherHash = calc_matcher_hash<U>();
				info.compId = comp_id<T>();

				return info;
			}

			template <typename T>
			static const ComponentInfo* create() {
				using U = std::decay_t<T>;
				return new ComponentInfo{init<U>()};
			}
		};
	} // namespace ecs
} // namespace gaia

namespace gaia {
	namespace ecs {
		class ComponentCache {
			cnt::darray<const ComponentInfo*> m_infoByIndex;
			cnt::darray<ComponentDesc> m_descByIndex;

			ComponentCache() {
				// Reserve enough storage space for most use-cases
				constexpr uint32_t DefaultComponentCacheSize = 2048;
				m_infoByIndex.reserve(DefaultComponentCacheSize);
				m_descByIndex.reserve(DefaultComponentCacheSize);
			}

		public:
			static ComponentCache& get() {
				static ComponentCache cache;
				return cache;
			}

			~ComponentCache() {
				clear();
			}

			ComponentCache(ComponentCache&&) = delete;
			ComponentCache(const ComponentCache&) = delete;
			ComponentCache& operator=(ComponentCache&&) = delete;
			ComponentCache& operator=(const ComponentCache&) = delete;

			//! Registers the component info for \tparam T. If it already exists it is returned.
			//! \return Component info
			template <typename T>
			GAIA_NODISCARD GAIA_FORCEINLINE const ComponentInfo& goc_comp_info() {
				using U = typename component_kind_t<T>::Kind;
				const auto compId = comp_id<T>();

				auto createInfo = [&]() GAIA_LAMBDAINLINE -> const ComponentInfo& {
					const auto* pInfo = ComponentInfo::create<U>();
					m_infoByIndex[compId] = pInfo;
					m_descByIndex[compId] = ComponentDesc::create<U>();
					return *pInfo;
				};

				if GAIA_UNLIKELY (compId >= m_infoByIndex.size()) {
					const auto oldSize = m_infoByIndex.size();
					const auto newSize = compId + 1U;

					// Increase the capacity by multiples of CapacityIncreaseSize
					constexpr uint32_t CapacityIncreaseSize = 128;
					const auto newCapacity = (newSize / CapacityIncreaseSize) * CapacityIncreaseSize + CapacityIncreaseSize;
					m_infoByIndex.reserve(newCapacity);

					// Update the size
					m_infoByIndex.resize(newSize);
					m_descByIndex.resize(newSize);

					// Make sure that unused memory is initialized to nullptr
					for (uint32_t i = oldSize; i < newSize; ++i)
						m_infoByIndex[i] = nullptr;

					return createInfo();
				}

				if GAIA_UNLIKELY (m_infoByIndex[compId] == nullptr) {
					return createInfo();
				}

				return *m_infoByIndex[compId];
			}

			//! Returns the component info given the \param compId.
			//! \warning It is expected the component info with a given component id exists! Undefined behavior otherwise.
			//! \return Component info
			GAIA_NODISCARD GAIA_FORCEINLINE const ComponentInfo& comp_info(ComponentId compId) const noexcept {
				GAIA_ASSERT(compId < m_infoByIndex.size());
				const auto* pInfo = m_infoByIndex[compId];
				GAIA_ASSERT(pInfo != nullptr);
				return *pInfo;
			}

			//! Returns the component creation info given the \param compId.
			//! \warning It is expected the component info with a given component id exists! Undefined behavior otherwise.
			//! \return Component info
			GAIA_NODISCARD const ComponentDesc& comp_desc(ComponentId compId) const noexcept {
				GAIA_ASSERT(compId < m_descByIndex.size());
				return m_descByIndex[compId];
			}

			//! Returns the component info for \tparam T.
			//! \warning It is expected the component already exists! Undefined behavior otherwise.
			//! \return Component info
			template <typename T>
			GAIA_NODISCARD const ComponentInfo& comp_info() const noexcept {
				const auto compId = comp_id<T>();
				return comp_info(compId);
			}

			void diag() const {
				const auto registeredTypes = (uint32_t)m_descByIndex.size();
				GAIA_LOG_N("Registered infos: %u", registeredTypes);

				for (const auto& desc: m_descByIndex)
					GAIA_LOG_N("  id:%010u, %.*s", desc.compId, (uint32_t)desc.name.size(), desc.name.data());
			}

		private:
			void clear() {
				for (const auto* pInfo: m_infoByIndex)
					delete pInfo;
				m_infoByIndex.clear();
				m_descByIndex.clear();
			}
		};
	} // namespace ecs
} // namespace gaia

namespace gaia {
	namespace ecs {
		class ArchetypeGraph {
			struct ArchetypeGraphEdge {
				ArchetypeId archetypeId;
			};

			//! Map of edges in the archetype graph when adding components
			cnt::map<ComponentId, ArchetypeGraphEdge> m_edgesAdd[ComponentKind::CK_Count];
			//! Map of edges in the archetype graph when removing components
			cnt::map<ComponentId, ArchetypeGraphEdge> m_edgesDel[ComponentKind::CK_Count];

		public:
			//! Creates an edge in the graph leading to the target archetype \param archetypeId via component \param
			//! compId of type \param compKind.
			void add_edge_right(ComponentKind compKind, ComponentId compId, ArchetypeId archetypeId) {
				[[maybe_unused]] const auto ret = m_edgesAdd[compKind].try_emplace({compId}, ArchetypeGraphEdge{archetypeId});
				GAIA_ASSERT(ret.second);
			}

			//! Creates an edge in the graph leading to the target archetype \param archetypeId via component \param
			//! compId of type \param compKind.
			void add_edge_left(ComponentKind compKind, ComponentId compId, ArchetypeId archetypeId) {
				[[maybe_unused]] const auto ret = m_edgesDel[compKind].try_emplace({compId}, ArchetypeGraphEdge{archetypeId});
				GAIA_ASSERT(ret.second);
			}

			//! Checks if the graph edge for component type \param compKind contains the component \param compId.
			//! \return Archetype id of the target archetype if the edge is found. ArchetypeIdBad otherwise.
			GAIA_NODISCARD ArchetypeId find_edge_right(ComponentKind compKind, const ComponentId compId) const {
				const auto& edges = m_edgesAdd[compKind];
				const auto it = edges.find({compId});
				return it != edges.end() ? it->second.archetypeId : ArchetypeIdBad;
			}

			//! Checks if the graph edge for component type \param compKind contains the component \param compId.
			//! \return Archetype id of the target archetype if the edge is found. ArchetypeIdBad otherwise.
			GAIA_NODISCARD ArchetypeId find_edge_left(ComponentKind compKind, const ComponentId compId) const {
				const auto& edges = m_edgesDel[compKind];
				const auto it = edges.find({compId});
				return it != edges.end() ? it->second.archetypeId : ArchetypeIdBad;
			}

			void diag() const {
				// const auto& cc = ComponentCache::get();

				// // Add edges (movement towards the leafs)
				// {
				// 	const auto& edgesG = m_edgesAdd[ComponentKind::CK_Generic];
				// 	const auto& edgesC = m_edgesAdd[ComponentKind::CK_Chunk];
				// 	const auto edgeCount = (uint32_t)(edgesG.size() + edgesC.size());
				// 	if (edgeCount > 0) {
				// 		GAIA_LOG_N("  Add edges - count:%u", edgeCount);

				// 		if (!edgesG.empty()) {
				// 			GAIA_LOG_N("    Generic - count:%u", (uint32_t)edgesG.size());
				// 			for (const auto& edge: edgesG) {
				// 				const auto& info = cc.comp_info(edge.first);
				// 				const auto& infoCreate = cc.comp_desc(info.compId);
				// 				GAIA_LOG_N(
				// 						"      %.*s (--> Archetype ID:%u)", (uint32_t)infoCreate.name.size(), infoCreate.name.data(),
				// 						edge.second.archetypeId);
				// 			}
				// 		}

				// 		if (!edgesC.empty()) {
				// 			GAIA_LOG_N("    Chunk - count:%u", (uint32_t)edgesC.size());
				// 			for (const auto& edge: edgesC) {
				// 				const auto& info = cc.comp_info(edge.first);
				// 				const auto& infoCreate = cc.comp_desc(info.compId);
				// 				GAIA_LOG_N(
				// 						"      %.*s (--> Archetype ID:%u)", (uint32_t)infoCreate.name.size(), infoCreate.name.data(),
				// 						edge.second.archetypeId);
				// 			}
				// 		}
				// 	}
				// }

				// // Delete edges (movement towards the root)
				// {
				// 	const auto& edgesG = m_edgesDel[ComponentKind::CK_Generic];
				// 	const auto& edgesC = m_edgesDel[ComponentKind::CK_Chunk];
				// 	const auto edgeCount = (uint32_t)(edgesG.size() + edgesC.size());
				// 	if (edgeCount > 0) {
				// 		GAIA_LOG_N("  del edges - count:%u", edgeCount);

				// 		if (!edgesG.empty()) {
				// 			GAIA_LOG_N("    Generic - count:%u", (uint32_t)edgesG.size());
				// 			for (const auto& edge: edgesG) {
				// 				const auto& info = cc.comp_info(edge.first);
				// 				const auto& infoCreate = cc.comp_desc(info.compId);
				// 				GAIA_LOG_N(
				// 						"      %.*s (--> Archetype ID:%u)", (uint32_t)infoCreate.name.size(), infoCreate.name.data(),
				// 						edge.second.archetypeId);
				// 			}
				// 		}

				// 		if (!edgesC.empty()) {
				// 			GAIA_LOG_N("    Chunk - count:%u", (uint32_t)edgesC.size());
				// 			for (const auto& edge: edgesC) {
				// 				const auto& info = cc.comp_info(edge.first);
				// 				const auto& infoCreate = cc.comp_desc(info.compId);
				// 				GAIA_LOG_N(
				// 						"      %.*s (--> Archetype ID:%u)", (uint32_t)infoCreate.name.size(), infoCreate.name.data(),
				// 						edge.second.archetypeId);
				// 			}
				// 		}
				// 	}
				// }
			}
		};
	} // namespace ecs
} // namespace gaia

#include <cinttypes>
#include <cstdint>
#include <tuple>
#include <type_traits>
#include <utility>

#include <cinttypes>
#include <cstdint>

namespace gaia {
	namespace core {
		//! Gaia-ECS is a header-only library which means we want to avoid using global
		//! static variables because they would get copied to each translation units.
		//! At the same time the goal is for users to not see any memory allocation used
		//! by the library. Therefore, the only solution is a static variable with local
		//! scope.
		//!
		//! Being a static variable with local scope which means the singleton is guaranteed
		//! to be younger than its caller. Because static variables are released in the reverse
		//! order in which they are created, if used with a static World it would mean we first
		//! release the singleton and only then proceed with the world itself. As a result, in
		//! its destructor the world could access memory that has already been released.
		//!
		//! Instead, we let the singleton allocate the object on the heap and once singleton's
		//! destructor is called we tell the internal object it should destroy itself. This way
		//! there are no memory leaks or access-after-freed issues on app exit reported.
		template <typename T>
		class dyn_singleton final {
			T* m_obj = new T();

			dyn_singleton() = default;

		public:
			static T& get() noexcept {
				static dyn_singleton<T> singleton;
				return *singleton.m_obj;
			}

			dyn_singleton(dyn_singleton&& world) = delete;
			dyn_singleton(const dyn_singleton& world) = delete;
			dyn_singleton& operator=(dyn_singleton&&) = delete;
			dyn_singleton& operator=(const dyn_singleton&) = delete;

			~dyn_singleton() {
				get().done();
			}
		};
	} // namespace core
} // namespace gaia

#include <cinttypes>

namespace gaia {
	namespace ecs {
		GAIA_NODISCARD inline bool version_changed(uint32_t changeVersion, uint32_t requiredVersion) {
			// When a system runs for the first time, everything is considered changed.
			if GAIA_UNLIKELY (requiredVersion == 0U)
				return true;

			// Supporting wrap-around for version numbers. ChangeVersion must be
			// bigger than requiredVersion (never detect change of something the
			// system itself changed).
			return (int)(changeVersion - requiredVersion) > 0;
		}

		inline void update_version(uint32_t& version) {
			++version;
			// Handle wrap-around, 0 is reserved for systems that have never run.
			if GAIA_UNLIKELY (version == 0U)
				++version;
		}
	} // namespace ecs
} // namespace gaia

namespace gaia {
	namespace ecs {
		//! Size of one allocated block of memory
		static constexpr uint32_t MaxMemoryBlockSize = 16384;
		//! Unusable area at the beggining of the allocated block designated for special pruposes
		static constexpr uint32_t MemoryBlockUsableOffset = sizeof(uintptr_t);

		struct ChunkAllocatorPageStats final {
			//! Total allocated memory
			uint64_t mem_total;
			//! Memory actively used
			uint64_t mem_used;
			//! Number of allocated pages
			uint32_t num_pages;
			//! Number of free pages
			uint32_t num_pages_free;
		};

		struct ChunkAllocatorStats final {
			ChunkAllocatorPageStats stats[2];
		};

		namespace detail {
			class ChunkAllocatorImpl;
		}
		using ChunkAllocator = core::dyn_singleton<detail::ChunkAllocatorImpl>;

		namespace detail {

			/*!
			Allocator for ECS Chunks. Memory is organized in pages of chunks.
			*/
			class ChunkAllocatorImpl {
				friend gaia::ecs::ChunkAllocator;

				struct MemoryPage {
					static constexpr uint16_t NBlocks = 62;
					static constexpr uint16_t NBlocks_Bits = (uint16_t)core::count_bits(NBlocks);
					static constexpr uint32_t InvalidBlockId = NBlocks + 1;
					static constexpr uint32_t BlockArrayBytes = ((uint32_t)NBlocks_Bits * (uint32_t)NBlocks + 7) / 8;
					using BlockArray = cnt::sarray<uint8_t, BlockArrayBytes>;
					using BitView = core::bit_view<NBlocks_Bits>;

					//! Pointer to data managed by page
					void* m_data;
					//! Implicit list of blocks
					BlockArray m_blocks;
					//! Chunk block size
					uint8_t m_sizeType;
					//! Index in the list of pages
					uint32_t m_pageIdx;
					//! Number of blocks in the block array
					uint32_t m_blockCnt: NBlocks_Bits;
					//! Number of used blocks out of NBlocks
					uint32_t m_usedBlocks: NBlocks_Bits;
					//! Index of the next block to recycle
					uint32_t m_nextFreeBlock: NBlocks_Bits;
					//! Number of blocks to recycle
					uint32_t m_freeBlocks: NBlocks_Bits;

					MemoryPage(void* ptr, uint8_t sizeType):
							m_data(ptr), m_sizeType(sizeType), m_pageIdx(0), m_blockCnt(0), m_usedBlocks(0), m_nextFreeBlock(0),
							m_freeBlocks(0) {
						// One cacheline long on x86. The point is for this to be as small as possible
						static_assert(sizeof(MemoryPage) <= 64);
					}

					void write_block_idx(uint32_t blockIdx, uint32_t value) {
						const uint32_t bitPosition = blockIdx * NBlocks_Bits;

						GAIA_ASSERT(bitPosition < NBlocks * NBlocks_Bits);
						GAIA_ASSERT(value <= InvalidBlockId);

						BitView{{(uint8_t*)m_blocks.data(), BlockArrayBytes}}.set(bitPosition, (uint8_t)value);
					}

					uint8_t read_block_idx(uint32_t blockIdx) const {
						const uint32_t bitPosition = blockIdx * NBlocks_Bits;

						GAIA_ASSERT(bitPosition < NBlocks * NBlocks_Bits);

						return BitView{{(uint8_t*)m_blocks.data(), BlockArrayBytes}}.get(bitPosition);
					}

					GAIA_NODISCARD void* alloc_chunk() {
						auto StoreChunkAddress = [&](uint32_t index) {
							// Encode info about chunk's page in the memory block.
							// The actual pointer returned is offset by UsableOffset bytes
							uint8_t* pMemoryBlock = (uint8_t*)m_data + index * mem_block_size(m_sizeType);
							*mem::unaligned_pointer<uintptr_t>{pMemoryBlock} = (uintptr_t)this;
							return (void*)(pMemoryBlock + MemoryBlockUsableOffset);
						};

						if (m_freeBlocks == 0U) {
							// We don't want to go out of range for new blocks
							GAIA_ASSERT(!full() && "Trying to allocate too many blocks!");

							const auto index = m_blockCnt;
							++m_usedBlocks;
							++m_blockCnt;
							write_block_idx(index, index);

							return StoreChunkAddress(index);
						}

						GAIA_ASSERT(m_nextFreeBlock < m_blockCnt && "Block allocator recycle cnt::list broken!");

						++m_usedBlocks;
						--m_freeBlocks;

						const auto index = m_nextFreeBlock;
						m_nextFreeBlock = read_block_idx(m_nextFreeBlock);

						return StoreChunkAddress(index);
					}

					void free_chunk(void* pChunk) {
						GAIA_ASSERT(m_freeBlocks <= NBlocks);

						// Offset the chunk memory so we get the real block address
						const auto* pMemoryBlock = (uint8_t*)pChunk - MemoryBlockUsableOffset;
						const auto blckAddr = (uintptr_t)pMemoryBlock;
						const auto dataAddr = (uintptr_t)m_data;
						const auto blockIdx = (uint32_t)((blckAddr - dataAddr) / mem_block_size(m_sizeType));

						// Update our implicit cnt::list
						if (m_freeBlocks == 0U) {
							write_block_idx(blockIdx, InvalidBlockId);
							m_nextFreeBlock = blockIdx;
						} else {
							write_block_idx(blockIdx, m_nextFreeBlock);
							m_nextFreeBlock = blockIdx;
						}

						++m_freeBlocks;
						--m_usedBlocks;
					}

					GAIA_NODISCARD uint32_t used_blocks_cnt() const {
						return m_usedBlocks;
					}

					GAIA_NODISCARD bool full() const {
						return m_usedBlocks == NBlocks;
					}

					GAIA_NODISCARD bool empty() const {
						return m_usedBlocks == 0;
					}
				};

				struct MemoryPageContainer {
					//! List of available pages
					//! Note, this currently only contains at most 1 item
					cnt::darray<MemoryPage*> pagesFree;
					//! List of full pages
					cnt::darray<MemoryPage*> pagesFull;
				};

				//! Container for pages storing various-sized chunks
				MemoryPageContainer m_pages[2];

				//! When true, destruction has been requested
				bool m_isDone = false;

				ChunkAllocatorImpl() = default;

			public:
				~ChunkAllocatorImpl() = default;

				ChunkAllocatorImpl(ChunkAllocatorImpl&& world) = delete;
				ChunkAllocatorImpl(const ChunkAllocatorImpl& world) = delete;
				ChunkAllocatorImpl& operator=(ChunkAllocatorImpl&&) = delete;
				ChunkAllocatorImpl& operator=(const ChunkAllocatorImpl&) = delete;

				static constexpr uint16_t mem_block_size(uint32_t sizeType) {
					return sizeType != 0 ? MaxMemoryBlockSize : MaxMemoryBlockSize / 2;
				}

				static constexpr uint8_t mem_block_size_type(uint32_t sizeBytes) {
					return (uint8_t)(sizeBytes > MaxMemoryBlockSize / 2);
				}

				/*!
				Allocates memory
				*/
				void* alloc(uint32_t bytesWanted) {
					GAIA_ASSERT(bytesWanted <= MaxMemoryBlockSize);

					void* pChunk = nullptr;

					const auto sizeType = mem_block_size_type(bytesWanted);
					auto& container = m_pages[sizeType];
					if (container.pagesFree.empty()) {
						// Initial allocation
						auto* pPage = alloc_page(sizeType);
						pChunk = pPage->alloc_chunk();
						container.pagesFree.push_back(pPage);
					} else {
						auto* pPage = container.pagesFree[0];
						GAIA_ASSERT(!pPage->full());
						// Allocate a new chunk of memory
						pChunk = pPage->alloc_chunk();

						// Handle full pages
						if (pPage->full()) {
							// Remove the page from the open list and update the swapped page's pointer
							core::erase_fast(container.pagesFree, 0);
							if (!container.pagesFree.empty())
								container.pagesFree[0]->m_pageIdx = 0;

							// Move our page to the full list
							pPage->m_pageIdx = (uint32_t)container.pagesFull.size();
							container.pagesFull.push_back(pPage);
						}
					}

					return pChunk;
				}

				/*!
				Releases memory allocated for pointer
				*/
				void free(void* pChunk) {
					// Decode the page from the address
					const auto pageAddr = *(uintptr_t*)((uint8_t*)pChunk - MemoryBlockUsableOffset);
					auto* pPage = (MemoryPage*)pageAddr;

					auto releaseChunk = [&](MemoryPageContainer& container) {
						const bool pageFull = pPage->full();

#if GAIA_DEBUG
						if (pageFull) {
							[[maybe_unused]] auto it =
									core::find_if(container.pagesFull.begin(), container.pagesFull.end(), [&](auto page) {
										return page == pPage;
									});
							GAIA_ASSERT(
									it != container.pagesFull.end() && "ChunkAllocator delete couldn't find the memory page expected "
																										 "in the full pages cnt::list");
						} else {
							[[maybe_unused]] auto it =
									core::find_if(container.pagesFree.begin(), container.pagesFree.end(), [&](auto page) {
										return page == pPage;
									});
							GAIA_ASSERT(
									it != container.pagesFree.end() && "ChunkAllocator delete couldn't find memory page expected in "
																										 "the free pages cnt::list");
						}
#endif

						// Update lists
						if (pageFull) {
							// Our page is no longer full. Remove it from the list and update the swapped page's pointer
							container.pagesFull.back()->m_pageIdx = pPage->m_pageIdx;
							core::erase_fast(container.pagesFull, pPage->m_pageIdx);

							// Move our page to the open list
							pPage->m_pageIdx = (uint32_t)container.pagesFree.size();
							container.pagesFree.push_back(pPage);
						}

						// Free the chunk
						pPage->free_chunk(pChunk);

						if (m_isDone) {
							// Remove the page right away
							if (pPage->empty()) {
								GAIA_ASSERT(!container.pagesFree.empty());
								container.pagesFree.back()->m_pageIdx = pPage->m_pageIdx;
								core::erase_fast(container.pagesFree, pPage->m_pageIdx);
							}

							// When there is nothing left, delete the allocator
							if (container.pagesFree.empty() && container.pagesFull.empty())
								delete this;
						}
					};

					releaseChunk(m_pages[pPage->m_sizeType]);
				}

				/*!
				Returns allocator statistics
				*/
				ChunkAllocatorStats stats() const {
					ChunkAllocatorStats stats;
					stats.stats[0] = page_stats(0);
					stats.stats[1] = page_stats(1);
					return stats;
				}

				/*!
				Flushes unused memory
				*/
				void flush() {
					auto flushPages = [](MemoryPageContainer& container) {
						for (uint32_t i = 0; i < container.pagesFree.size();) {
							auto* pPage = container.pagesFree[i];

							// Skip non-empty pages
							if (!pPage->empty()) {
								++i;
								continue;
							}

							core::erase_fast(container.pagesFree, i);
							free_page(pPage);
							if (!container.pagesFree.empty())
								container.pagesFree[i]->m_pageIdx = (uint32_t)i;
						}
					};

					for (auto& c: m_pages)
						flushPages(c);
				}

				/*!
				Performs diagnostics of the memory used.
				*/
				void diag() const {
					auto diagPage = [](const ChunkAllocatorPageStats& memstats) {
						GAIA_LOG_N("ChunkAllocator stats");
						GAIA_LOG_N("  Allocated: %" PRIu64 " B", memstats.mem_total);
						GAIA_LOG_N("  Used: %" PRIu64 " B", memstats.mem_total - memstats.mem_used);
						GAIA_LOG_N("  Overhead: %" PRIu64 " B", memstats.mem_used);
						GAIA_LOG_N("  Utilization: %.1f%%", 100.0 * ((double)memstats.mem_used / (double)memstats.mem_total));
						GAIA_LOG_N("  Pages: %u", memstats.num_pages);
						GAIA_LOG_N("  Free pages: %u", memstats.num_pages_free);
					};

					diagPage(page_stats(0));
					diagPage(page_stats(1));
				}

			private:
				static MemoryPage* alloc_page(uint8_t sizeType) {
					const auto size = mem_block_size(sizeType) * MemoryPage::NBlocks;
					auto* pPageData = mem::mem_alloc_alig(size, 16);
					return new MemoryPage(pPageData, sizeType);
				}

				static void free_page(MemoryPage* page) {
					mem::mem_free_alig(page->m_data);
					delete page;
				}

				void done() {
					m_isDone = true;
				}

				ChunkAllocatorPageStats page_stats(uint32_t sizeType) const {
					ChunkAllocatorPageStats stats{};
					const MemoryPageContainer& container = m_pages[sizeType];
					stats.num_pages = (uint32_t)container.pagesFree.size() + (uint32_t)container.pagesFull.size();
					stats.num_pages_free = (uint32_t)container.pagesFree.size();
					stats.mem_total = stats.num_pages * (size_t)mem_block_size(sizeType) * MemoryPage::NBlocks;
					stats.mem_used = container.pagesFull.size() * (size_t)mem_block_size(sizeType) * MemoryPage::NBlocks;
					for (auto* page: container.pagesFree)
						stats.mem_used += page->used_blocks_cnt() * (size_t)MaxMemoryBlockSize;
					return stats;
				};
			};
		} // namespace detail

	} // namespace ecs
} // namespace gaia

#include <cinttypes>
#include <cstdint>

#include <cinttypes>
#include <type_traits>

namespace gaia {
	namespace ecs {
		using EntityInternalType = uint32_t;
		using EntityId = EntityInternalType;
		using EntityGenId = EntityInternalType;

		struct Entity final {
			static constexpr EntityInternalType IdBits = GAIA_ENTITY_IDBITS;
			static constexpr EntityInternalType GenBits = GAIA_ENTITY_GENBITS;
			static constexpr EntityInternalType IdMask = (uint32_t)(uint64_t(1) << IdBits) - 1;
			static constexpr EntityInternalType GenMask = (uint32_t)(uint64_t(1) << GenBits) - 1;

			static constexpr uint32_t EntityBitsTotal = IdBits + GenBits;
			using EntitySizeType = std::conditional_t<(EntityBitsTotal > 32), uint64_t, uint32_t>;
			static_assert(EntityBitsTotal <= 64, "EntityBitsTotal must fit inside 64 bits");
			static_assert(IdBits <= 31, "Entity IdBits must be at most 31 bits long");
			static_assert(GenBits > 10, "Entity GenBits is recommended to be at least 10 bits long");

		private:
			struct EntityData {
				//! Index in entity array
				EntityInternalType id: IdBits;
				//! Generation index. Incremented every time an entity is deleted
				EntityInternalType gen: GenBits;
			};

			union {
				EntityData data;
				EntitySizeType val;
			};

		public:
			Entity() noexcept = default;
			Entity(EntityId id, EntityGenId gen) {
				data.id = id;
				data.gen = gen;
			}
			~Entity() = default;

			Entity(Entity&&) noexcept = default;
			Entity(const Entity&) = default;
			Entity& operator=(Entity&&) noexcept = default;
			Entity& operator=(const Entity&) = default;

			GAIA_NODISCARD constexpr bool operator==(const Entity& other) const noexcept {
				return val == other.val;
			}
			GAIA_NODISCARD constexpr bool operator!=(const Entity& other) const noexcept {
				return val != other.val;
			}

			auto id() const {
				return data.id;
			}
			auto gen() const {
				return data.gen;
			}
			auto value() const {
				return val;
			}
		};

		struct EntityNull_t {
			GAIA_NODISCARD operator Entity() const noexcept {
				return Entity(Entity::IdMask, Entity::GenMask);
			}

			GAIA_NODISCARD constexpr bool operator==([[maybe_unused]] const EntityNull_t& null) const noexcept {
				return true;
			}
			GAIA_NODISCARD constexpr bool operator!=([[maybe_unused]] const EntityNull_t& null) const noexcept {
				return false;
			}
		};

		GAIA_NODISCARD inline bool operator==(const EntityNull_t& null, const Entity& entity) noexcept {
			return static_cast<Entity>(null).id() == entity.id();
		}

		GAIA_NODISCARD inline bool operator!=(const EntityNull_t& null, const Entity& entity) noexcept {
			return static_cast<Entity>(null).id() != entity.id();
		}

		GAIA_NODISCARD inline bool operator==(const Entity& entity, const EntityNull_t& null) noexcept {
			return null == entity;
		}

		GAIA_NODISCARD inline bool operator!=(const Entity& entity, const EntityNull_t& null) noexcept {
			return null != entity;
		}

		inline constexpr EntityNull_t EntityNull{};

		class Chunk;

		struct EntityContainer: cnt::ilist_item_base {
			//! Allocated items: Index in the list.
			//! Deleted items: Index of the next deleted item in the list.
			uint32_t idx;
			//! Generation ID
			uint32_t gen : 31;
			//! Disabled
			uint32_t dis : 1;
			//! Chunk the entity currently resides in
			Chunk* pChunk;
#if !GAIA_64
			uint32_t pChunk_padding;
#endif

			EntityContainer() = default;
			EntityContainer(uint32_t index, uint32_t generation): idx(index), gen(generation), dis(0), pChunk(nullptr) {}
		};
	} // namespace ecs
} // namespace gaia

namespace gaia {
	namespace ecs {
		using ChunkVersionOffset = uint8_t;
		using ChunkComponentOffset = uint16_t;

		struct ChunkHeaderOffsets {
			//! Byte at which the first version number is located
			ChunkVersionOffset firstByte_Versions[ComponentKind::CK_Count]{};
			//! Byte at which the first component id is located
			ChunkComponentOffset firstByte_ComponentIds[ComponentKind::CK_Count]{};
			//! Byte at which the first component offset is located
			ChunkComponentOffset firstByte_CompOffs[ComponentKind::CK_Count]{};
			//! Byte at which the first entity is located
			ChunkComponentOffset firstByte_EntityData{};
		};

		struct ChunkHeader final {
			static constexpr uint16_t CHUNK_LIFESPAN_BITS = 4;
			//! Number of ticks before empty chunks are removed
			static constexpr uint16_t MAX_CHUNK_LIFESPAN = (1 << CHUNK_LIFESPAN_BITS) - 1;

			//! Maxiumum number of entities per chunk.
			//! Defined as sizeof(big_chunk) / sizeof(entity)
			static constexpr uint16_t MAX_CHUNK_ENTITIES =
					(detail::ChunkAllocatorImpl::mem_block_size(1) - 64) / sizeof(Entity);
			static constexpr uint16_t MAX_CHUNK_ENTITIES_BITS = (uint16_t)core::count_bits(MAX_CHUNK_ENTITIES);

			//! Archetype the chunk belongs to
			ArchetypeId archetypeId;
			//! Chunk index in its archetype list
			uint32_t index;
			//! Total number of entities in the chunk.
			uint16_t count;
			//! Number of enabled entities in the chunk.
			uint16_t countEnabled;
			//! Capacity (copied from the owner archetype).
			uint16_t capacity;

			//! Index of the first enabled entity in the chunk
			uint32_t firstEnabledEntityIndex: MAX_CHUNK_ENTITIES_BITS;
			//! Once removal is requested and it hits 0 the chunk is removed.
			uint32_t lifespanCountdown: CHUNK_LIFESPAN_BITS;
			//! Updated when chunks are being iterated. Used to inform of structural changes when they shouldn't happen.
			uint32_t structuralChangesLocked : 3;
			//! True if there's any generic component that requires custom construction
			uint32_t hasAnyCustomGenericCtor : 1;
			//! True if there's any chunk component that requires custom construction
			uint32_t hasAnyCustomChunkCtor : 1;
			//! True if there's any generic component that requires custom destruction
			uint32_t hasAnyCustomGenericDtor : 1;
			//! True if there's any chunk component that requires custom destruction
			uint32_t hasAnyCustomChunkDtor : 1;
			//! Chunk size type. This tells whether it's 8K or 16K
			uint32_t sizeType : 1;
			//! Empty space for future use
			uint32_t unused : 8;

			//! Offsets to various parts of data inside chunk
			ChunkHeaderOffsets offsets;

			//! Number of components on the archetype
			uint8_t componentCount[ComponentKind::CK_Count]{};
			//! Version of the world (stable pointer to parent world's world version)
			uint32_t& worldVersion;

			ChunkHeader(
					uint32_t aid, uint32_t chunkIndex, uint16_t cap, uint16_t st, const ChunkHeaderOffsets& offs,
					uint32_t& version):
					archetypeId(aid),
					index(chunkIndex), count(0), countEnabled(0), capacity(cap), firstEnabledEntityIndex(0), lifespanCountdown(0),
					structuralChangesLocked(0), hasAnyCustomGenericCtor(0), hasAnyCustomChunkCtor(0), hasAnyCustomGenericDtor(0),
					hasAnyCustomChunkDtor(0), sizeType(st), offsets(offs), worldVersion(version) {
				// Make sure the alignment is right
				GAIA_ASSERT(uintptr_t(this) % (sizeof(size_t)) == 0);
			}

			bool has_disabled_entities() const {
				return firstEnabledEntityIndex > 0;
			}

			bool has_enabled_entities() const {
				return countEnabled > 0;
			}
		};
	} // namespace ecs
} // namespace gaia

namespace gaia {
	namespace ecs {
		//! Updates the provided component matcher hash based on the provided component id
		//! \param matcherHash Initial matcher hash
		//! \param compId Component id
		inline void matcher_hash(ComponentMatcherHash& matcherHash, ComponentId compId) noexcept {
			const auto& cc = ComponentCache::get();
			const auto componentHash = cc.comp_info(compId).matcherHash.hash;
			matcherHash.hash = core::combine_or(matcherHash.hash, componentHash);
		}

		//! Calculates a component matcher hash from the provided component ids
		//! \param compIds Span of component ids
		//! \return Component matcher hash
		GAIA_NODISCARD inline ComponentMatcherHash matcher_hash(ComponentIdSpan compIds) noexcept {
			const auto infosSize = compIds.size();
			if (infosSize == 0)
				return {0};

			const auto& cc = ComponentCache::get();
			ComponentMatcherHash::Type hash = cc.comp_info(compIds[0]).matcherHash.hash;
			for (uint32_t i = 1; i < infosSize; ++i)
				hash = core::combine_or(hash, cc.comp_info(compIds[i]).matcherHash.hash);
			return {hash};
		}

		//! Calculates a component lookup hash from the provided component ids
		//! \param compIds Span of component ids
		//! \return Component lookup hash
		GAIA_NODISCARD inline ComponentLookupHash calc_lookup_hash(ComponentIdSpan compIds) noexcept {
			const auto infosSize = compIds.size();
			if (infosSize == 0)
				return {0};

			const auto& cc = ComponentCache::get();
			ComponentLookupHash::Type hash = cc.comp_info(compIds[0]).lookupHash.hash;
			for (uint32_t i = 1; i < infosSize; ++i)
				hash = core::hash_combine(hash, cc.comp_info(compIds[i]).lookupHash.hash);
			return {hash};
		}

		using SortComponentCond = core::is_smaller<ComponentId>;
	} // namespace ecs
} // namespace gaia

namespace gaia {
	namespace ecs {
		class Chunk final {
		public:
			static constexpr uint32_t MAX_COMPONENTS_BITS = 5U;
			//! Maximum number of components on archetype
			static constexpr uint32_t MAX_COMPONENTS = 1U << MAX_COMPONENTS_BITS;

			using ComponentIdArray = cnt::sarray_ext<ComponentId, MAX_COMPONENTS>;
			using ComponentOffsetArray = cnt::sarray_ext<ChunkComponentOffset, MAX_COMPONENTS>;

			// TODO: Make this private
			//! Chunk header
			ChunkHeader m_header;

		private:
			//! Pointer to where the chunk data starts.
			//! Data layed out as following:
			//!			1) ComponentVersions[ComponentKind::CK_Generic]
			//!			2) ComponentVersions[ComponentKind::CK_Chunk]
			//!     3) ComponentIds[ComponentKind::CK_Generic]
			//!			4) ComponentIds[ComponentKind::CK_Chunk]
			//!			5) CompOffs[ComponentKind::CK_Generic]
			//!			6) CompOffs[ComponentKind::CK_Chunk]
			//!			7) Entities
			//!			8) Components
			//! Note, root archetypes store only entites, therefore it is fully occupied with entities.
			uint8_t m_data[1];

			GAIA_MSVC_WARNING_PUSH()
			GAIA_MSVC_WARNING_DISABLE(26495)

			Chunk(
					uint32_t archetypeId, uint32_t chunkIndex, uint16_t capacity, uint16_t st, uint32_t& worldVersion,
					const ChunkHeaderOffsets& headerOffsets):
					m_header(archetypeId, chunkIndex, capacity, st, headerOffsets, worldVersion) {
				// Chunk data area consist of memory offsets + component data. Normally. we would initialize it.
				// However, the memory offsets part are all trivial types and components are initialized via their
				// constructors so we do not really need to do anything.
				// const uint32_t sizeBytes = ...;
				// auto* curr = (uint8_t*)&m_data[0] + 1;
				// for (uint32_t i = 0; i < sizeBytes; ++i, (void)++curr) {
				// 	const auto* addr = mem::addressof(*curr);
				// 	(void)new (const_cast<void*>(static_cast<const volatile void*>(addr))) uint8_t;
				// }
			}

			GAIA_MSVC_WARNING_POP()

			void init(
					const cnt::sarray<ComponentIdArray, ComponentKind::CK_Count>& compIds,
					const cnt::sarray<ComponentOffsetArray, ComponentKind::CK_Count>& compOffs) {
				m_header.componentCount[ComponentKind::CK_Generic] = (uint8_t)compIds[ComponentKind::CK_Generic].size();
				m_header.componentCount[ComponentKind::CK_Chunk] = (uint8_t)compIds[ComponentKind::CK_Chunk].size();

				const auto& cc = ComponentCache::get();

				const auto& compIdsGeneric = compIds[ComponentKind::CK_Generic];
				for (const auto compId: compIdsGeneric) {
					const auto& desc = cc.comp_desc(compId);
					m_header.hasAnyCustomGenericCtor |= (desc.func_ctor != nullptr);
					m_header.hasAnyCustomGenericDtor |= (desc.func_dtor != nullptr);
				}
				const auto& compIdsChunk = compIds[ComponentKind::CK_Chunk];
				for (const auto compId: compIdsChunk) {
					const auto& desc = cc.comp_desc(compId);
					m_header.hasAnyCustomChunkCtor |= (desc.func_ctor != nullptr);
					m_header.hasAnyCustomChunkDtor |= (desc.func_dtor != nullptr);
				}

				// Copy provided component id data to this chunk's data area
				{
					for (uint32_t i = 0; i < ComponentKind::CK_Count; ++i) {
						auto offset = m_header.offsets.firstByte_ComponentIds[i];
						for (const auto compId: compIds[i]) {
							// unaligned_ref not necessary because data is aligned
							*(ComponentId*)&m_data[offset] = compId;
							offset += sizeof(ComponentId);
						}
					}
				}

				// Copy provided component offset data to this chunk's data area
				{
					for (uint32_t i = 0; i < ComponentKind::CK_Count; ++i) {
						auto offset = m_header.offsets.firstByte_CompOffs[i];
						for (const auto compOff: compOffs[i]) {
							// unaligned_ref not necessary because data is aligned
							*(ChunkComponentOffset*)&m_data[offset] = compOff;
							offset += sizeof(ChunkComponentOffset);
						}
					}
				}
			}

			/*!
			Returns a read-only span of the component data.
			\warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			\tparam T Component
			\return Span of read-only component data.
			*/
			template <typename T>
			GAIA_NODISCARD GAIA_FORCEINLINE auto view_inter() const -> decltype(std::span<const uint8_t>{}) {
				using U = typename component_kind_t<T>::Kind;

				if constexpr (std::is_same_v<U, Entity>) {
					return {&data(m_header.offsets.firstByte_EntityData), size()};
				} else {
					static_assert(!std::is_empty_v<U>, "Attempting to get value of an empty component");

					const auto compId = comp_id<T>();
					constexpr auto compKind = component_kind_v<T>;

					// Find at what byte offset the first component of a given type is located
					uint32_t compIdx = 0;
					const auto offset = find_data_offset(compKind, compId, compIdx);

					if constexpr (compKind == ComponentKind::CK_Generic) {
						[[maybe_unused]] const auto maxOffset = offset + capacity() * sizeof(U);
						GAIA_ASSERT(maxOffset <= bytes());

						return {&data(offset), size()};
					} else {
						[[maybe_unused]] const auto maxOffset = offset + sizeof(U);
						GAIA_ASSERT(maxOffset <= bytes());

						return {&data(offset), 1};
					}
				}
			}

			/*!
			Returns a read-write span of the component data. Also updates the world version for the component.
			\warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			\tparam T Component
			\tparam WorldVersionUpdateWanted If true, the world version is updated as a result of the write access
			\return Span of read-write component data.
			*/
			template <typename T, bool WorldVersionUpdateWanted>
			GAIA_NODISCARD GAIA_FORCEINLINE auto view_mut_inter() -> decltype(std::span<uint8_t>{}) {
				using U = typename component_kind_t<T>::Kind;
#if GAIA_COMPILER_MSVC && _MSC_VER <= 1916
				// Workaround for MSVC 2017 bug where it incorrectly evaluates the static assert
				// even in context where it shouldn't.
				// Unfortunatelly, even runtime assert can't be used...
				// GAIA_ASSERT(!std::is_same_v<U, Entity>::value);
#else
				static_assert(!std::is_same_v<U, Entity>);
#endif
				static_assert(!std::is_empty_v<U>, "Attempting to set value of an empty component");

				const auto compId = comp_id<T>();
				constexpr auto compKind = component_kind_v<T>;

				// Find at what byte offset the first component of a given type is located
				uint32_t compIdx = 0;
				const auto offset = find_data_offset(compKind, compId, compIdx);

				// Update version number if necessary so we know RW access was used on the chunk
				if constexpr (WorldVersionUpdateWanted)
					this->update_world_version(compKind, compIdx);

				if constexpr (compKind == ComponentKind::CK_Generic) {
					[[maybe_unused]] const auto maxOffset = offset + capacity() * sizeof(U);
					GAIA_ASSERT(maxOffset <= bytes());

					return {&data(offset), size()};
				} else {
					[[maybe_unused]] const auto maxOffset = offset + sizeof(U);
					GAIA_ASSERT(maxOffset <= bytes());

					return {&data(offset), 1};
				}
			}

			/*!
			Returns the value stored in the component \tparam T on \param index in the chunk.
			\warning It is expected the \param index is valid. Undefined behavior otherwise.
			\warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			\tparam T Component
			\param index Index of entity in the chunk
			\return Value stored in the component if smaller than 8 bytes. Const reference to the value otherwise.
			*/
			template <typename T>
			GAIA_NODISCARD auto comp_inter(uint32_t index) const {
				using U = typename component_kind_t<T>::Kind;
				using RetValueType = decltype(view<T>()[0]);

				GAIA_ASSERT(index < m_header.count);
				if constexpr (sizeof(RetValueType) > 8)
					return (const U&)view<T>()[index];
				else
					return view<T>()[index];
			}

			/*!
			Removes the entity at from the chunk and updates the world versions
			*/
			void remove_last_entity_inter() {
				// Should never be called over an empty chunk
				GAIA_ASSERT(has_entities());
				--m_header.count;
				--m_header.countEnabled;
			}

		public:
			Chunk(const Chunk& chunk) = delete;
			Chunk(Chunk&& chunk) = delete;
			Chunk& operator=(const Chunk& chunk) = delete;
			Chunk& operator=(Chunk&& chunk) = delete;
			~Chunk() = default;

			static uint16_t chunk_total_bytes(uint16_t dataSize) {
				uint16_t header = (uint16_t)sizeof(ChunkHeader) + (uint16_t)MemoryBlockUsableOffset;
				return header + dataSize;
			}

			static uint16_t chunk_data_bytes(uint16_t totalSize) {
				uint16_t header = (uint16_t)sizeof(ChunkHeader) + (uint16_t)MemoryBlockUsableOffset;
				return totalSize - header;
			}

			/*!
			Allocates memory for a new chunk.
			\param chunkIndex Index of this chunk within the parent archetype
			\return Newly allocated chunk
			*/
			static Chunk* create(
					uint32_t archetypeId, uint32_t chunkIndex, uint16_t capacity, uint16_t dataBytes, uint32_t& worldVersion,
					const ChunkHeaderOffsets& offsets, const cnt::sarray<ComponentIdArray, ComponentKind::CK_Count>& compIds,
					const cnt::sarray<ComponentOffsetArray, ComponentKind::CK_Count>& compOffs) {
				const auto totalBytes = chunk_total_bytes(dataBytes);
				const auto sizeType = detail::ChunkAllocatorImpl::mem_block_size_type(totalBytes);
#if GAIA_ECS_CHUNK_ALLOCATOR
				auto* pChunk = (Chunk*)ChunkAllocator::get().alloc(totalBytes);
				new (pChunk) Chunk(archetypeId, chunkIndex, capacity, sizeType, worldVersion, offsets);
#else
				GAIA_ASSERT(totalBytes <= MaxMemoryBlockSize);
				const auto allocSize = detail::ChunkAllocatorImpl::mem_block_size(sizeType);
				auto* pChunkMem = new uint8_t[allocSize];
				auto* pChunk = new (pChunkMem) Chunk(archetypeId, chunkIndex, capacity, sizeType, worldVersion, offsets);
#endif

				pChunk->init(compIds, compOffs);

				return pChunk;
			}

			/*!
			Releases all memory allocated by \param pChunk.
			\param pChunk Chunk which we want to destroy
			*/
			static void free(Chunk* pChunk) {
				GAIA_ASSERT(pChunk != nullptr);

				// Call destructors for components that need it
				if (pChunk->has_custom_generic_dtor())
					pChunk->call_dtors(ComponentKind::CK_Generic, 0, pChunk->size());
				if (pChunk->has_custom_chunk_dtor())
					pChunk->call_dtors(ComponentKind::CK_Chunk, 0, 1);

#if GAIA_ECS_CHUNK_ALLOCATOR
				pChunk->~Chunk();
				ChunkAllocator::get().free(pChunk);
#else
				pChunk->~Chunk();
				auto* pChunkMem = (uint8_t*)pChunk;
				delete pChunkMem;
#endif
			}

			/*!
			Remove the last entity from chunk.
			\param chunksToRemove Container of chunks ready for removal
			*/
			void remove_last_entity(cnt::darray<Chunk*>& chunksToRemove) {
				GAIA_ASSERT(
						!has_structural_changes() && "Entities can't be removed while their chunk is being iterated "
																				 "(structural changes are forbidden during this time!)");

				remove_last_entity_inter();

				if (!dying() && !has_entities()) {
					// When the chunk is emptied we want it to be removed. We can't do it
					// right away and need to wait for world's GC to be called.
					//
					// However, we need to prevent the following:
					//    1) chunk is emptied, add it to some removal list
					//    2) chunk is reclaimed
					//    3) chunk is emptied, add it to some removal list again
					//
					// Therefore, we have a flag telling us the chunk is already waiting to
					// be removed. The chunk might be reclaimed before GC happens but it
					// simply ignores such requests. This way GC always has at most one
					// record for removal for any given chunk.
					prepare_to_die();

					chunksToRemove.push_back(this);
				}
			}

			//! Updates the version numbers for this chunk.
			void update_versions() {
				update_version(m_header.worldVersion);
				update_world_version(ComponentKind::CK_Generic);
				update_world_version(ComponentKind::CK_Chunk);
			}

			/*!
			Returns a read-only entity or component view.
			\warning If \tparam T is a component it is expected it is present. Undefined behavior otherwise.
			\tparam T Component or Entity
			\return Entity of component view with read-only access
			*/
			template <typename T>
			GAIA_NODISCARD auto view() const {
				using U = typename component_kind_t<T>::Kind;

				return mem::auto_view_policy_get<U>{view_inter<T>()};
			}

			/*!
			Returns a mutable entity or component view.
			\warning If \tparam T is a component it is expected it is present. Undefined behavior otherwise.
			\tparam T Component or Entity
			\return Entity or component view with read-write access
			*/
			template <typename T>
			GAIA_NODISCARD auto view_mut() {
				using U = typename component_kind_t<T>::Kind;
				static_assert(!std::is_same_v<U, Entity>);

				return mem::auto_view_policy_set<U>{view_mut_inter<T, true>()};
			}

			/*!
			Returns a mutable component view.
			Doesn't update the world version when the access is aquired.
			\warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			\tparam T Component
			\return Component view with read-write access
			*/
			template <typename T>
			GAIA_NODISCARD auto sview_mut() {
				using U = typename component_kind_t<T>::Kind;
				static_assert(!std::is_same_v<U, Entity>);

				return mem::auto_view_policy_set<U>{view_mut_inter<T, false>()};
			}

			/*!
			Make \param entity a part of the chunk at the version of the world
			\return Index of the entity within the chunk.
			*/
			GAIA_NODISCARD uint32_t add_entity(Entity entity) {
				const auto index = m_header.count++;
				++m_header.countEnabled;
				set_entity(index, entity);

				update_version(m_header.worldVersion);
				update_world_version(ComponentKind::CK_Generic);
				update_world_version(ComponentKind::CK_Chunk);

				return index;
			}

			/*!
			Copies all data associated with \param oldEntity into \param newEntity.
			*/
			static void copy_entity_data(Entity oldEntity, Entity newEntity, std::span<EntityContainer> entities) {
				GAIA_PROF_SCOPE(copy_entity_data);

				auto& oldEntityContainer = entities[oldEntity.id()];
				auto* pOldChunk = oldEntityContainer.pChunk;

				auto& newEntityContainer = entities[newEntity.id()];
				auto* pNewChunk = newEntityContainer.pChunk;

				GAIA_ASSERT(pOldChunk->archetype_id() == pNewChunk->archetype_id());

				const auto& cc = ComponentCache::get();
				auto oldIds = pOldChunk->comp_id_view(ComponentKind::CK_Generic);
				auto oldOffs = pOldChunk->comp_offset_view(ComponentKind::CK_Generic);

				// Copy generic component data from reference entity to our new entity
				for (uint32_t i = 0; i < oldIds.size(); ++i) {
					const auto& desc = cc.comp_desc(oldIds[i]);
					if (desc.properties.size == 0U)
						continue;

					const auto offset = oldOffs[i];
					const auto idxSrc = offset + desc.properties.size * (uint32_t)oldEntityContainer.idx;
					const auto idxDst = offset + desc.properties.size * (uint32_t)newEntityContainer.idx;

					GAIA_ASSERT(idxSrc < pOldChunk->bytes());
					GAIA_ASSERT(idxDst < pNewChunk->bytes());

					auto* pSrc = (void*)&pOldChunk->data(idxSrc);
					auto* pDst = (void*)&pNewChunk->data(idxDst);
					desc.copy(pSrc, pDst);
				}
			}

			/*!
			Moves all data associated with \param entity into the chunk so that it is stored at \param newEntityIdx.
			*/
			void move_entity_data(Entity entity, uint32_t newEntityIdx, std::span<EntityContainer> entities) {
				GAIA_PROF_SCOPE(CopyEntityFrom);

				auto& oldEntityContainer = entities[entity.id()];
				auto* pOldChunk = oldEntityContainer.pChunk;

				GAIA_ASSERT(pOldChunk->archetype_id() == archetype_id());

				const auto& cc = ComponentCache::get();
				auto oldIds = pOldChunk->comp_id_view(ComponentKind::CK_Generic);
				auto oldOffs = pOldChunk->comp_offset_view(ComponentKind::CK_Generic);

				// Copy generic component data from reference entity to our new entity
				for (uint32_t i = 0; i < oldIds.size(); ++i) {
					const auto& desc = cc.comp_desc(oldIds[i]);
					if (desc.properties.size == 0U)
						continue;

					const auto offset = oldOffs[i];
					const auto idxSrc = offset + desc.properties.size * (uint32_t)oldEntityContainer.idx;
					const auto idxDst = offset + desc.properties.size * newEntityIdx;

					GAIA_ASSERT(idxSrc < pOldChunk->bytes());
					GAIA_ASSERT(idxDst < bytes());

					auto* pSrc = (void*)&pOldChunk->data(idxSrc);
					auto* pDst = (void*)&data(idxDst);
					desc.ctor_from(pSrc, pDst);
				}
			}

			/*!
			Moves all data associated with \param entity into the chunk so that it is stored at index \param newEntityIdx.
			*/
			void move_foreign_entity_data(Entity entity, uint32_t newEntityIdx, std::span<EntityContainer> entities) {
				GAIA_PROF_SCOPE(move_foreign_entity_data);

				auto& oldEntityContainer = entities[entity.id()];
				auto* pOldChunk = oldEntityContainer.pChunk;

				const auto& cc = ComponentCache::get();

				// Find intersection of the two component lists.
				// We ignore chunk components here because they should't be influenced
				// by entities moving around.
				auto oldIds = pOldChunk->comp_id_view(ComponentKind::CK_Generic);
				auto oldOffs = pOldChunk->comp_offset_view(ComponentKind::CK_Generic);
				auto newIds = comp_id_view(ComponentKind::CK_Generic);
				auto newOffs = comp_offset_view(ComponentKind::CK_Generic);

				// Arrays are sorted so we can do linear intersection lookup
				{
					uint32_t i = 0;
					uint32_t j = 0;

					auto moveData = [&](const ComponentDesc& desc) {
						if (desc.properties.size == 0U)
							return;

						// Let's move all type data from oldEntity to newEntity
						const auto idxSrc = oldOffs[i] + desc.properties.size * (uint32_t)oldEntityContainer.idx;
						const auto idxDst = newOffs[j] + desc.properties.size * newEntityIdx;

						GAIA_ASSERT(idxSrc < pOldChunk->bytes());
						GAIA_ASSERT(idxDst < bytes());

						auto* pSrc = (void*)&pOldChunk->data(idxSrc);
						auto* pDst = (void*)&data(idxDst);
						desc.ctor_from(pSrc, pDst);
					};

					while (i < oldIds.size() && j < newIds.size()) {
						const auto& descOld = cc.comp_desc(oldIds[i]);
						const auto& descNew = cc.comp_desc(newIds[j]);

						if (&descOld == &descNew) {
							moveData(descOld);
							++i;
							++j;
						} else if (SortComponentCond{}.operator()(descOld.compId, descNew.compId))
							++i;
						else
							++j;
					}
				}
			}

			/*!
			Tries to remove the entity at index \param index.
			Removal is done via swapping with last entity in chunk.
			Upon removal, all associated data is also removed.
			If the entity at the given index already is the last chunk entity, it is removed directly.
			*/
			void remove_entity_inter(uint32_t index, std::span<EntityContainer> entities) {
				GAIA_PROF_SCOPE(remove_entity_inter);

				const auto left = index;
				const auto right = (uint32_t)m_header.count - 1;
				// The "left" entity is the one we are going to destroy so it needs to preceed the "right"
				GAIA_ASSERT(left <= right);

				const auto& cc = ComponentCache::get();

				// There must be at least 2 entities inside to swap
				if GAIA_LIKELY (left < right) {
					GAIA_ASSERT(m_header.count > 1);

					const auto entity = get_entity(right);
					auto& entityContainer = entities[entity.id()];
					const auto wasDisabled = entityContainer.dis;

					// Update entity index inside chunk
					set_entity(left, entity);

					auto compIds = comp_id_view(ComponentKind::CK_Generic);
					auto compOffs = comp_offset_view(ComponentKind::CK_Generic);

					for (uint32_t i = 0; i < compIds.size(); ++i) {
						const auto& desc = cc.comp_desc(compIds[i]);
						if (desc.properties.size == 0U)
							continue;

						const auto offset = compOffs[i];
						const auto idxSrc = offset + left * desc.properties.size;
						const auto idxDst = offset + right * desc.properties.size;

						GAIA_ASSERT(idxSrc < bytes());
						GAIA_ASSERT(idxDst < bytes());
						GAIA_ASSERT(idxSrc != idxDst);

						auto* pSrc = (void*)&m_data[idxSrc];
						auto* pDst = (void*)&m_data[idxDst];
						desc.move(pSrc, pDst);
						desc.dtor(pSrc);
					}

					// Entity has been replaced with the last one in our chunk.
					// Update its container record.
					entityContainer.idx = left;
					entityContainer.gen = entity.gen();
					entityContainer.dis = wasDisabled;
				} else {
					auto compIds = comp_id_view(ComponentKind::CK_Generic);
					auto compOffs = comp_offset_view(ComponentKind::CK_Generic);

					for (uint32_t i = 0; i < compIds.size(); ++i) {
						const auto& desc = cc.comp_desc(compIds[i]);
						if (desc.properties.size == 0U)
							continue;

						const auto offset = compOffs[i];
						const auto idxSrc = offset + left * desc.properties.size;

						GAIA_ASSERT(idxSrc < bytes());

						auto* pSrc = (void*)&m_data[idxSrc];
						desc.dtor(pSrc);
					}
				}
			}

			/*!
			Tries to remove the entity at index \param index.
			Removal is done via swapping with last entity in chunk.
			Upon removal, all associated data is also removed.
			If the entity at the given index already is the last chunk entity, it is removed directly.
			*/
			void remove_entity(uint32_t index, std::span<EntityContainer> entities, cnt::darray<Chunk*>& chunksToRemove) {
				GAIA_ASSERT(
						!has_structural_changes() && "Entities can't be removed while their chunk is being iterated "
																				 "(structural changes are forbidden during this time!)");

				const auto chunkEntityCount = size();
				if GAIA_UNLIKELY (chunkEntityCount == 0)
					return;

				GAIA_PROF_SCOPE(remove_entity);

				if (enabled(index)) {
					// Entity was previously enabled. Swap with the last entity
					remove_entity_inter(index, entities);
					// If this was the first enabled entity make sure to update the index
					if (m_header.firstEnabledEntityIndex > 0 && index == m_header.firstEnabledEntityIndex)
						--m_header.firstEnabledEntityIndex;
				} else {
					// Entity was previously disabled. Swap with the last disabled entity
					const auto pivot = size_disabled() - 1;
					swap_chunk_entities(index, pivot, entities);
					// Once swapped, try to swap with the last (enabled) entity in the chunk.
					remove_entity_inter(pivot, entities);
					--m_header.firstEnabledEntityIndex;
				}

				// At this point the last entity is no longer valid so remove it
				remove_last_entity(chunksToRemove);
			}

			/*!
			Tries to swap the entity at index \param left with the one at the index \param right.
			When swapping, all data associated with the two entities is swapped as well.
			If \param left equals \param right no swapping is performed.
			\warning "Left" must he smaller or equal to "right"
			*/
			void swap_chunk_entities(uint32_t left, uint32_t right, std::span<EntityContainer> entities) {
				// The "left" entity is the one we are going to destroy so it needs to preceed the "right".
				// Unlike remove_entity_inter, it is not technically necessary but we do it
				// anyway for the sake of consistency.
				GAIA_ASSERT(left <= right);

				// If there are at least two entities inside to swap
				if GAIA_UNLIKELY (m_header.count <= 1)
					return;
				if (left == right)
					return;

				GAIA_PROF_SCOPE(SwapEntitiesInsideChunk);

				// Update entity indices inside chunk
				const auto entityLeft = get_entity(left);
				const auto entityRight = get_entity(right);
				set_entity(left, entityRight);
				set_entity(right, entityLeft);

				const auto& cc = ComponentCache::get();
				auto compIds = comp_id_view(ComponentKind::CK_Generic);
				auto compOffs = comp_offset_view(ComponentKind::CK_Generic);

				for (uint32_t i = 0; i < compIds.size(); ++i) {
					const auto& desc = cc.comp_desc(compIds[i]);
					if (desc.properties.size == 0U)
						continue;

					const auto offset = compOffs[i];
					const auto idxSrc = offset + left * desc.properties.size;
					const auto idxDst = offset + right * desc.properties.size;

					GAIA_ASSERT(idxSrc < bytes());
					GAIA_ASSERT(idxDst < bytes());
					GAIA_ASSERT(idxSrc != idxDst);

					auto* pSrc = (void*)&m_data[idxSrc];
					auto* pDst = (void*)&m_data[idxDst];
					desc.swap(pSrc, pDst);
				}

				// Entities were swapped. Update their entity container records.
				auto& ecLeft = entities[entityLeft.id()];
				bool ecLeftWasDisabled = ecLeft.dis;
				auto& ecRight = entities[entityRight.id()];
				ecLeft.idx = right;
				ecLeft.gen = entityRight.gen();
				ecLeft.dis = ecRight.dis;
				ecRight.idx = left;
				ecRight.gen = entityLeft.gen();
				ecRight.dis = ecLeftWasDisabled;
			}

			/*!
			Makes the entity a part of a chunk on a given index.
			\param index Index of the entity
			\param entity Entity to store in the chunk
			*/
			void set_entity(uint32_t index, Entity entity) {
				GAIA_ASSERT(index < m_header.count && "Entity chunk index out of bounds!");

				const auto offset = sizeof(Entity) * index + m_header.offsets.firstByte_EntityData;
				// unaligned_ref not necessary because data is aligned
				auto* pMem = (Entity*)&m_data[offset];
				*pMem = entity;
			}

			/*!
			Returns the entity on a given index in the chunk.
			\param index Index of the entity
			\return Entity on a given index within the chunk.
			*/
			GAIA_NODISCARD Entity get_entity(uint32_t index) const {
				GAIA_ASSERT(index < m_header.count && "Entity chunk index out of bounds!");

				const auto offset = sizeof(Entity) * index + m_header.offsets.firstByte_EntityData;
				// unaligned_ref not necessary because data is aligned
				auto* pMem = (Entity*)&m_data[offset];
				return *pMem;
			}

			/*!
			Enables or disables the entity on a given index in the chunk.
			\param index Index of the entity
			\param enableEntity Enables or disabled the entity
			*/
			void enable_entity(uint32_t index, bool enableEntity, std::span<EntityContainer> entities) {
				GAIA_ASSERT(index < m_header.count && "Entity chunk index out of bounds!");

				if (enableEntity) {
					// Nothing to enable if there are no disabled entities
					if (!m_header.has_disabled_entities())
						return;
					// Trying to enable an already enabled entity
					if (enabled(index))
						return;
					// Try swapping our entity with the last disabled one
					swap_chunk_entities(--m_header.firstEnabledEntityIndex, index, entities);
					entities[get_entity(index).id()].dis = 0;
					++m_header.countEnabled;
				} else {
					// Nothing to disable if there are no enabled entities
					if (!m_header.has_enabled_entities())
						return;
					// Trying to disable an already disabled entity
					if (!enabled(index))
						return;
					// Try swapping our entity with the last one in our chunk
					swap_chunk_entities(m_header.firstEnabledEntityIndex++, index, entities);
					entities[get_entity(index).id()].dis = 1;
					--m_header.countEnabled;
				}
			}

			/*!
			Checks if the entity is enabled.
			\param index Index of the entity
			\return True if entity is enabled. False otherwise.
			*/
			bool enabled(uint32_t index) const {
				GAIA_ASSERT(m_header.count > 0);

				return index >= (uint32_t)m_header.firstEnabledEntityIndex;
			}

			/*!
			Returns a mutable pointer to chunk data.
			\param offset Offset into chunk data
			\return Pointer to chunk data.
			*/
			uint8_t& data(uint32_t offset) {
				return m_data[offset];
			}

			/*!
			Returns an immutable pointer to chunk data.
			\param offset Offset into chunk data
			\return Pointer to chunk data.
			*/
			const uint8_t& data(uint32_t offset) const {
				return m_data[offset];
			}

			/*!
			Returns an offset to chunk data at which the component is stored.
			\warning It is expected the component with \param compId is present. Undefined behavior otherwise.
			\param compKind Component type
			\param compId Component id
			\param compIdx Index of the component in this chunk's component array
			\return Offset from the start of chunk data area.
			*/
			GAIA_NODISCARD ChunkComponentOffset
			find_data_offset(ComponentKind compKind, ComponentId compId, uint32_t& compIdx) const {
				// Don't use this with empty components. It's impossible to write to them anyway.
				GAIA_ASSERT(ComponentCache::get().comp_desc(compId).properties.size != 0);

				compIdx = comp_idx(compKind, compId);

				auto compOffs = comp_offset_view(compKind);
				const auto offset = compOffs[compIdx];
				GAIA_ASSERT(offset >= m_header.offsets.firstByte_EntityData);
				return offset;
			}

			/*!
			Returns an offset to chunk data at which the component is stored.
			\warning It is expected the component with \param compId is present. Undefined behavior otherwise.
			\param compKind Component type
			\param compId Component id
			\return Offset from the start of chunk data area.
			*/
			GAIA_NODISCARD GAIA_FORCEINLINE ChunkComponentOffset
			find_data_offset(ComponentKind compKind, ComponentId compId) const {
				[[maybe_unused]] uint32_t compIdx = 0;
				return find_data_offset(compKind, compId, compIdx);
			}

			//----------------------------------------------------------------------
			// Component handling
			//----------------------------------------------------------------------

			bool has_custom_generic_ctor() const {
				return m_header.hasAnyCustomGenericCtor;
			}

			bool has_custom_chunk_ctor() const {
				return m_header.hasAnyCustomChunkCtor;
			}

			bool has_custom_generic_dtor() const {
				return m_header.hasAnyCustomGenericDtor;
			}

			bool has_custom_chunk_dtor() const {
				return m_header.hasAnyCustomChunkDtor;
			}

			void call_ctor(ComponentKind compKind, ComponentId compId, uint32_t entIdx) {
				GAIA_PROF_SCOPE(call_ctor);

				// Make sure only generic types are used with indices
				GAIA_ASSERT(compKind == ComponentKind::CK_Generic || entIdx == 0);

				const auto& cc = ComponentCache::get();
				const auto& desc = cc.comp_desc(compId);
				if (desc.func_ctor == nullptr)
					return;

				const auto idx = comp_idx(compKind, compId);

				auto compOffs = comp_offset_view(compKind);
				const auto offset = compOffs[idx];
				const auto idxSrc = offset + entIdx * desc.properties.size;
				GAIA_ASSERT(idxSrc < bytes());

				auto* pSrc = (void*)&m_data[idxSrc];
				desc.func_ctor(pSrc, 1);
			}

			void call_ctors(ComponentKind compKind, uint32_t entIdx, uint32_t entCnt) {
				GAIA_PROF_SCOPE(call_ctors);

				GAIA_ASSERT(
						compKind == ComponentKind::CK_Generic && has_custom_generic_ctor() ||
						compKind == ComponentKind::CK_Chunk && has_custom_chunk_ctor());

				// Make sure only generic types are used with indices
				GAIA_ASSERT(compKind == ComponentKind::CK_Generic || (entIdx == 0 && entCnt == 1));

				const auto& cc = ComponentCache::get();
				auto compIds = comp_id_view(compKind);
				auto compOffs = comp_offset_view(compKind);

				for (uint32_t i = 0; i < compIds.size(); ++i) {
					const auto& desc = cc.comp_desc(compIds[i]);
					if (desc.func_ctor == nullptr)
						continue;

					const auto offset = compOffs[i];
					const auto idxSrc = offset + entIdx * desc.properties.size;
					GAIA_ASSERT(idxSrc < bytes());

					auto* pSrc = (void*)&m_data[idxSrc];
					desc.func_ctor(pSrc, entCnt);
				}
			}

			void call_dtors(ComponentKind compKind, uint32_t entIdx, uint32_t entCnt) {
				GAIA_PROF_SCOPE(call_dtors);

				GAIA_ASSERT(
						compKind == ComponentKind::CK_Generic && has_custom_generic_dtor() ||
						compKind == ComponentKind::CK_Chunk && has_custom_chunk_dtor());

				// Make sure only generic types are used with indices
				GAIA_ASSERT(compKind == ComponentKind::CK_Generic || (entIdx == 0 && entCnt == 1));

				const auto& cc = ComponentCache::get();
				auto compIds = comp_id_view(compKind);
				auto compOffs = comp_offset_view(compKind);

				for (uint32_t i = 0; i < compIds.size(); ++i) {
					const auto& desc = cc.comp_desc(compIds[i]);
					if (desc.func_dtor == nullptr)
						continue;

					const auto offset = compOffs[i];
					const auto idxSrc = offset + entIdx * desc.properties.size;
					GAIA_ASSERT(idxSrc < bytes());

					auto* pSrc = (void*)&m_data[idxSrc];
					desc.func_dtor(pSrc, entCnt);
				}
			};

			//----------------------------------------------------------------------
			// Check component presence
			//----------------------------------------------------------------------

			/*!
			Checks if a component with \param compId and type \param compKind is present in the chunk.
			\param compId Component id
			\param compKind Component type
			\return True if found. False otherwise.
			*/
			GAIA_NODISCARD bool has(ComponentKind compKind, ComponentId compId) const {
				auto compIds = comp_id_view(compKind);
				return core::has(compIds, compId);
			}

			/*!
			Checks if component \tparam T is present in the chunk.
			\tparam T Component
			\return True if the component is present. False otherwise.
			*/
			template <typename T>
			GAIA_NODISCARD bool has() const {
				const auto compId = comp_id<T>();

				constexpr auto compKind = component_kind_v<T>;
				return has(compKind, compId);
			}

			//----------------------------------------------------------------------
			// Set component data
			//----------------------------------------------------------------------

			/*!
			Sets the value of the chunk component \tparam T on \param index in the chunk.
			\warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			\tparam T Component
			\param index Index of entity in the chunk
			\param value Value to set for the component
			*/
			template <typename T, typename U = typename component_kind_t<T>::Kind>
			U& set(uint32_t index) {
				static_assert(
						component_kind_v<T> == ComponentKind::CK_Generic,
						"Set providing an index can only be used with generic components");

				// Update the world version
				update_version(m_header.worldVersion);

				GAIA_ASSERT(index < m_header.capacity);
				return view_mut<T>()[index];
			}

			/*!
			Sets the value of the chunk component \tparam T on \param index in the chunk.
			\warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			\tparam T Component
			\param index Index of entity in the chunk
			\param value Value to set for the component
			*/
			template <typename T, typename U = typename component_kind_t<T>::Kind>
			U& set() {
				// Update the world version
				update_version(m_header.worldVersion);

				GAIA_ASSERT(0 < m_header.capacity);
				return view_mut<T>()[0];
			}

			/*!
			Sets the value of the chunk component \tparam T on \param index in the chunk.
			\warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			\tparam T Component
			\param index Index of entity in the chunk
			\param value Value to set for the component
			*/
			template <typename T, typename U = typename component_kind_t<T>::Kind>
			void set(uint32_t index, U&& value) {
				static_assert(
						component_kind_v<T> == ComponentKind::CK_Generic,
						"Set providing an index can only be used with generic components");

				// Update the world version
				update_version(m_header.worldVersion);

				GAIA_ASSERT(index < m_header.capacity);
				view_mut<T>()[index] = std::forward<U>(value);
			}

			/*!
			Sets the value of the chunk component \tparam T in the chunk.
			\warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			\tparam T Component
			\param value Value to set for the component
			*/
			template <typename T, typename U = typename component_kind_t<T>::Kind>
			void set(U&& value) {
				static_assert(
						component_kind_v<T> != ComponentKind::CK_Generic,
						"Set not providing an index can only be used with non-generic components");

				// Update the world version
				update_version(m_header.worldVersion);

				GAIA_ASSERT(0 < m_header.capacity);
				view_mut<T>()[0] = std::forward<U>(value);
			}

			/*!
			Sets the value of the chunk component \tparam T on \param index in the chunk.
			\warning World version is not updated so Query filters will not be able to catch this change.
			\warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			\tparam T Component
			\param index Index of entity in the chunk
			\param value Value to set for the component
			*/
			template <typename T, typename U = typename component_kind_t<T>::Kind>
			void sset(uint32_t index, U&& value) {
				static_assert(
						component_kind_v<T> == ComponentKind::CK_Generic,
						"SetSilent providing an index can only be used with generic components");

				GAIA_ASSERT(index < m_header.capacity);
				sview_mut<T>()[index] = std::forward<U>(value);
			}

			/*!
			Sets the value of the chunk component \tparam T in the chunk.
			\warning World version is not updated so Query filters will not be able to catch this change.
			\warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			\tparam T Component
			\param value Value to set for the component
			*/
			template <typename T, typename U = typename component_kind_t<T>::Kind>
			void sset(U&& value) {
				static_assert(
						component_kind_v<T> != ComponentKind::CK_Generic,
						"SetSilent not providing an index can only be used with non-generic components");

				GAIA_ASSERT(0 < m_header.capacity);
				sview_mut<T>()[0] = std::forward<U>(value);
			}

			//----------------------------------------------------------------------
			// Read component data
			//----------------------------------------------------------------------

			/*!
			Returns the value stored in the component \tparam T on \param index in the chunk.
			\warning It is expected the \param index is valid. Undefined behavior otherwise.
			\warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			\tparam T Component
			\param index Index of entity in the chunk
			\return Value stored in the component.
			*/
			template <typename T>
			GAIA_NODISCARD auto get(uint32_t index) const {
				static_assert(
						component_kind_v<T> == ComponentKind::CK_Generic,
						"Get providing an index can only be used with generic components");

				return comp_inter<T>(index);
			}

			/*!
			Returns the value stored in the chunk component \tparam T.
			\warning It is expected the chunk component \tparam T is present. Undefined behavior otherwise.
			\tparam T Component
			\return Value stored in the component.
			*/
			template <typename T>
			GAIA_NODISCARD auto get() const {
				static_assert(
						component_kind_v<T> != ComponentKind::CK_Generic,
						"Get not providing an index can only be used with non-generic components");

				return comp_inter<T>(0);
			}

			/*!
			Returns the internal index of a component based on the provided \param compId.
			\param compKind Component type
			\return Component index if the component was found. -1 otherwise.
			*/
			GAIA_NODISCARD uint32_t comp_idx(ComponentKind compKind, ComponentId compId) const {
				auto compIds = comp_id_view(compKind);
				const auto idx = core::get_index_unsafe(compIds, compId);
				GAIA_ASSERT(idx != BadIndex);
				return idx;
			}

			//----------------------------------------------------------------------
			// Iteration
			//----------------------------------------------------------------------

			template <typename T>
			GAIA_NODISCARD constexpr GAIA_FORCEINLINE auto comp_view() {
				using U = typename component_kind_t<T>::Kind;
				using UOriginal = typename component_kind_t<T>::KindOriginal;
				if constexpr (is_component_mut_v<UOriginal>) {
					auto s = view_mut_inter<U, true>();
					return std::span{(U*)s.data(), s.size()};
				} else {
					auto s = view_inter<U>();
					return std::span{(const U*)s.data(), s.size()};
				}
			}

			template <typename... T, typename Func>
			GAIA_FORCEINLINE void each([[maybe_unused]] core::func_type_list<T...> types, Func func) {
				const uint32_t idxFrom = m_header.firstEnabledEntityIndex;
				const uint32_t idxStop = m_header.count;
				GAIA_ASSERT(idxStop > idxFrom);
				GAIA_ASSERT(idxStop > 0);

				if constexpr (sizeof...(T) > 0) {
					// Pointers to the respective component types in the chunk, e.g
					// 		q.each([&](Position& p, const Velocity& v) {...}
					// Translates to:
					//  	auto p = iter.view_mut_inter<Position, true>();
					//		auto v = iter.view_inter<Velocity>();
					auto dataPointerTuple = std::make_tuple(comp_view<T>()...);

					// Iterate over each entity in the chunk.
					// Translates to:
					//		for (uint32_t i: iter)
					//			func(p[i], v[i]);

					for (uint32_t i = idxFrom; i < idxStop; ++i)
						func(std::get<decltype(comp_view<T>())>(dataPointerTuple)[i]...);
				} else {
					// No functor parameters. Do an empty loop.
					for (uint32_t i = idxFrom; i < idxStop; ++i)
						func();
				}
			}

			//----------------------------------------------------------------------

			GAIA_NODISCARD ArchetypeId archetype_id() const {
				return m_header.archetypeId;
			}

			//! Sets the index of this chunk in its archetype's storage
			void set_idx(uint32_t value) {
				m_header.index = value;
			}

			//! Returns the index of this chunk in its archetype's storage
			GAIA_NODISCARD uint32_t idx() const {
				return m_header.index;
			}

			//! Checks is this chunk has any enabled entities
			GAIA_NODISCARD bool has_enabled_entities() const {
				return m_header.has_enabled_entities();
			}

			//! Checks is this chunk has any disabled entities
			GAIA_NODISCARD bool has_disabled_entities() const {
				return m_header.has_disabled_entities();
			}

			//! Checks is this chunk is dying
			GAIA_NODISCARD bool dying() const {
				return m_header.lifespanCountdown > 0;
			}

			void prepare_to_die() {
				m_header.lifespanCountdown = ChunkHeader::MAX_CHUNK_LIFESPAN;
			}

			bool progress_death() {
				GAIA_ASSERT(dying());
				--m_header.lifespanCountdown;
				return dying();
			}

			void set_structural_changes(bool value) {
				if (value) {
					GAIA_ASSERT(m_header.structuralChangesLocked < 16);
					++m_header.structuralChangesLocked;
				} else {
					GAIA_ASSERT(m_header.structuralChangesLocked > 0);
					--m_header.structuralChangesLocked;
				}
			}

			bool has_structural_changes() const {
				return m_header.structuralChangesLocked != 0;
			}

			//! Checks is the full capacity of the has has been reached
			GAIA_NODISCARD bool full() const {
				return m_header.count >= m_header.capacity;
			}

			//! Checks is the chunk is semi-full.
			GAIA_NODISCARD bool is_semi() const {
				constexpr float Threshold = 0.7f;
				return ((float)m_header.count / (float)m_header.capacity) < Threshold;
			}

			//! Checks is there are any entities in the chunk
			GAIA_NODISCARD bool has_entities() const {
				return m_header.count > 0;
			}

			//! Returns the total number of entities in the chunk (both enabled and disabled)
			GAIA_NODISCARD uint32_t size() const {
				return m_header.count;
			}

			//! Return the number of entities in the chunk which are enabled
			GAIA_NODISCARD uint32_t size_enabled() const {
				return m_header.countEnabled;
			}

			//! Return the number of entities in the chunk which are enabled
			GAIA_NODISCARD uint32_t size_disabled() const {
				return m_header.firstEnabledEntityIndex;
			}

			//! Returns the number of entities in the chunk
			GAIA_NODISCARD uint32_t capacity() const {
				return m_header.capacity;
			}

			GAIA_NODISCARD uint32_t bytes() const {
				return detail::ChunkAllocatorImpl::mem_block_size(m_header.sizeType);
			}

			GAIA_NODISCARD std::span<uint32_t> comp_version_view(ComponentKind compKind) const {
				const auto offset = m_header.offsets.firstByte_Versions[compKind];
				return {(uint32_t*)(&m_data[offset]), m_header.componentCount[compKind]};
			}

			GAIA_NODISCARD std::span<const ComponentId> comp_id_view(ComponentKind compKind) const {
				using RetType = std::add_pointer_t<const ComponentId>;
				const auto offset = m_header.offsets.firstByte_ComponentIds[compKind];
				return {(RetType)&m_data[offset], m_header.componentCount[compKind]};
			}

			GAIA_NODISCARD std::span<const ChunkComponentOffset> comp_offset_view(ComponentKind compKind) const {
				using RetType = std::add_pointer_t<const ChunkComponentOffset>;
				const auto offset = m_header.offsets.firstByte_CompOffs[compKind];
				return {(RetType)&m_data[offset], m_header.componentCount[compKind]};
			}

			//! Returns true if the provided version is newer than the one stored internally
			GAIA_NODISCARD bool changed(ComponentKind compKind, uint32_t version, uint32_t compIdx) const {
				auto versions = comp_version_view(compKind);
				return version_changed(versions[compIdx], version);
			}

			//! Update version of a component at the index \param compIdx of a given \param compKind
			GAIA_FORCEINLINE void update_world_version(ComponentKind compKind, uint32_t compIdx) const {
				// Make sure only proper input is provided
				GAIA_ASSERT(compIdx >= 0 && compIdx < Chunk::MAX_COMPONENTS);

				auto versions = comp_version_view(compKind);
				versions[compIdx] = m_header.worldVersion;
			}

			//! Update version of all components of a given \param compKind
			GAIA_FORCEINLINE void update_world_version(ComponentKind compKind) const {
				auto versions = comp_version_view(compKind);
				for (auto& v: versions)
					v = m_header.worldVersion;
			}

			void diag(uint32_t index) const {
				GAIA_LOG_N(
						"  Chunk #%04u, entities:%u/%u, lifespanCountdown:%u", index, m_header.count, m_header.capacity,
						m_header.lifespanCountdown);
			}
		};
	} // namespace ecs
} // namespace gaia

namespace gaia {
	namespace ecs {
		class Archetype;

		class ArchetypeBase {
		protected:
			//! Archetype ID - used to address the archetype directly in the world's list or archetypes
			ArchetypeId m_archetypeId = ArchetypeIdBad;

		public:
			GAIA_NODISCARD ArchetypeId archetype_id() const {
				return m_archetypeId;
			}
		};

		class ArchetypeLookupChecker: public ArchetypeBase {
			friend class Archetype;

			//! List of component indices
			ComponentIdSpan m_compIds[ComponentKind::CK_Count];

		public:
			ArchetypeLookupChecker(ComponentIdSpan compIdsGeneric, ComponentIdSpan compIdsChunk) {
				m_compIds[ComponentKind::CK_Generic] = compIdsGeneric;
				m_compIds[ComponentKind::CK_Chunk] = compIdsChunk;
			}

			bool cmp_comp_ids(const ArchetypeLookupChecker& other) const {
				// Size has to match
				if (m_compIds[ComponentKind::CK_Generic].size() != other.m_compIds[ComponentKind::CK_Generic].size())
					return false;
				if (m_compIds[ComponentKind::CK_Chunk].size() != other.m_compIds[ComponentKind::CK_Chunk].size())
					return false;

				// Elements have to match
				for (size_t i = 0; i < m_compIds[ComponentKind::CK_Generic].size(); ++i) {
					if (m_compIds[ComponentKind::CK_Generic][i] != other.m_compIds[ComponentKind::CK_Generic][i])
						return false;
				}
				for (size_t i = 0; i < m_compIds[ComponentKind::CK_Chunk].size(); ++i) {
					if (m_compIds[ComponentKind::CK_Chunk][i] != other.m_compIds[ComponentKind::CK_Chunk][i])
						return false;
				}

				return true;
			}
		};

		class Archetype final: public ArchetypeBase {
		public:
			using LookupHash = core::direct_hash_key<uint64_t>;
			using GenericComponentHash = core::direct_hash_key<uint64_t>;
			using ChunkComponentHash = core::direct_hash_key<uint64_t>;

		private:
			struct {
				//! The number of entities this archetype can take (e.g 5 = 5 entities with all their components)
				uint16_t capacity;
				//! How many bytes of data is needed for a fully utilized chunk
				uint16_t chunkDataBytes;
			} m_properties{};
			static_assert(sizeof(m_properties) <= sizeof(uint32_t));
			//! Stable reference to parent world's world version
			uint32_t& m_worldVersion;

			//! List of chunks allocated by this archetype
			cnt::darray<Chunk*> m_chunks;
			//! Mask of chunks with disabled entities
			// cnt::dbitset m_disabledMask;
			//! Graph of archetypes linked with this one
			ArchetypeGraph m_graph;

			//! Offsets to various parts of data inside chunk
			ChunkHeaderOffsets m_dataOffsets;
			//! List of component indices
			cnt::sarray<Chunk::ComponentIdArray, ComponentKind::CK_Count> m_compIds;
			//! List of components offset indices
			cnt::sarray<Chunk::ComponentOffsetArray, ComponentKind::CK_Count> m_compOffs;

			//! Hash of generic components
			GenericComponentHash m_genericHash = {0};
			//! Hash of chunk components
			ChunkComponentHash m_chunkHash = {0};
			//! Hash of components within this archetype - used for lookups
			LookupHash m_lookupHash = {0};
			//! Hash of components within this archetype - used for matching
			ComponentMatcherHash m_matcherHash[ComponentKind::CK_Count]{};

			// Constructor is hidden. Create archetypes via Create
			Archetype(uint32_t& worldVersion): m_worldVersion(worldVersion) {}

			void UpdateDataOffsets(uintptr_t memoryAddress) {
				uint32_t offset = 0;

				// Versions
				// We expect versions to fit in the first 256 bytes.
				// With 64 components per archetype (32 generic + 32 chunk) this gives us some headroom.
				{
					offset += mem::padding<alignof(uint32_t)>(memoryAddress);

					if (!m_compIds[ComponentKind::CK_Generic].empty()) {
						GAIA_ASSERT(offset < 256);
						m_dataOffsets.firstByte_Versions[ComponentKind::CK_Generic] = (ChunkVersionOffset)offset;
						offset += sizeof(uint32_t) * m_compIds[ComponentKind::CK_Generic].size();
					}
					if (!m_compIds[ComponentKind::CK_Chunk].empty()) {
						GAIA_ASSERT(offset < 256);
						m_dataOffsets.firstByte_Versions[ComponentKind::CK_Chunk] = (ChunkVersionOffset)offset;
						offset += sizeof(uint32_t) * m_compIds[ComponentKind::CK_Chunk].size();
					}
				}

				// Component ids
				{
					offset += mem::padding<alignof(ComponentId)>(offset);

					if (!m_compIds[ComponentKind::CK_Generic].empty()) {
						m_dataOffsets.firstByte_ComponentIds[ComponentKind::CK_Generic] = (ChunkComponentOffset)offset;
						offset += sizeof(ComponentId) * m_compIds[ComponentKind::CK_Generic].size();
					}
					if (!m_compIds[ComponentKind::CK_Chunk].empty()) {
						m_dataOffsets.firstByte_ComponentIds[ComponentKind::CK_Chunk] = (ChunkComponentOffset)offset;
						offset += sizeof(ComponentId) * m_compIds[ComponentKind::CK_Chunk].size();
					}
				}

				// Component offsets
				{
					offset += mem::padding<alignof(ChunkComponentOffset)>(offset);

					if (!m_compIds[ComponentKind::CK_Generic].empty()) {
						m_dataOffsets.firstByte_CompOffs[ComponentKind::CK_Generic] = (ChunkComponentOffset)offset;
						offset += sizeof(ChunkComponentOffset) * m_compIds[ComponentKind::CK_Generic].size();
					}
					if (!m_compIds[ComponentKind::CK_Chunk].empty()) {
						m_dataOffsets.firstByte_CompOffs[ComponentKind::CK_Chunk] = (ChunkComponentOffset)offset;
						offset += sizeof(ChunkComponentOffset) * m_compIds[ComponentKind::CK_Chunk].size();
					}
				}

				// First entity offset
				{
					offset += mem::padding<alignof(Entity)>(offset);
					m_dataOffsets.firstByte_EntityData = (ChunkComponentOffset)offset;
				}
			}

			/*!
			Checks if a component with \param compId and type \param compKind is present in the archetype.
			\param compId Component id
			\param compKind Component type
			\return True if found. False otherwise.
			*/
			GAIA_NODISCARD bool has_inter(ComponentKind compKind, ComponentId compId) const {
				const auto& compIds = comp_ids(compKind);
				return core::has(compIds, compId);
			}

			/*!
			Estimates how many entities can fit into the chunk described by \param compIds components.
			*/
			static bool est_max_entities_per_archetype(
					uint32_t& dataOffset, uint32_t& maxItems, ComponentIdSpan compIds, uint32_t size, uint32_t maxDataOffset) {
				const auto& cc = ComponentCache::get();

				for (const auto compId: compIds) {
					const auto& desc = cc.comp_desc(compId);
					const auto alignment = desc.properties.alig;
					if (alignment == 0)
						continue;

					// If we're beyond what the chunk could take, subtract one entity
					const auto nextOffset = desc.calc_new_mem_offset(dataOffset, size);
					if (nextOffset >= maxDataOffset) {
						const auto subtractItems = (nextOffset - maxDataOffset + desc.properties.size) / desc.properties.size;
						GAIA_ASSERT(subtractItems > 0);
						GAIA_ASSERT(maxItems > subtractItems);
						maxItems -= subtractItems;
						return false;
					}

					dataOffset = nextOffset;
				}

				return true;
			};

		public:
			Archetype(Archetype&& world) = delete;
			Archetype(const Archetype& world) = delete;
			Archetype& operator=(Archetype&&) = delete;
			Archetype& operator=(const Archetype&) = delete;

			~Archetype() {
				// Delete all archetype chunks
				for (auto* pChunk: m_chunks)
					Chunk::free(pChunk);
			}

			bool cmp_comp_ids(const ArchetypeLookupChecker& other) const {
				// Size has to match
				if (m_compIds[ComponentKind::CK_Generic].size() != other.m_compIds[ComponentKind::CK_Generic].size())
					return false;
				if (m_compIds[ComponentKind::CK_Chunk].size() != other.m_compIds[ComponentKind::CK_Chunk].size())
					return false;

				// Elements have to match
				for (uint32_t i = 0; i < m_compIds[ComponentKind::CK_Generic].size(); ++i) {
					if (m_compIds[ComponentKind::CK_Generic][i] != other.m_compIds[ComponentKind::CK_Generic][i])
						return false;
				}
				for (uint32_t i = 0; i < m_compIds[ComponentKind::CK_Chunk].size(); ++i) {
					if (m_compIds[ComponentKind::CK_Chunk][i] != other.m_compIds[ComponentKind::CK_Chunk][i])
						return false;
				}

				return true;
			}

			GAIA_NODISCARD static LookupHash
			calc_lookup_hash(GenericComponentHash genericHash, ChunkComponentHash chunkHash) noexcept {
				return {core::hash_combine(genericHash.hash, chunkHash.hash)};
			}

			GAIA_NODISCARD static Archetype* create(
					ArchetypeId archetypeId, uint32_t& worldVersion, ComponentIdSpan compIdsGeneric,
					ComponentIdSpan compIdsChunk) {
				auto* newArch = new Archetype(worldVersion);
				newArch->m_archetypeId = archetypeId;
				const uint32_t maxEntities = archetypeId == 0 ? ChunkHeader::MAX_CHUNK_ENTITIES : 512;

				newArch->m_compIds[ComponentKind::CK_Generic].resize((uint32_t)compIdsGeneric.size());
				newArch->m_compIds[ComponentKind::CK_Chunk].resize((uint32_t)compIdsChunk.size());
				newArch->m_compOffs[ComponentKind::CK_Generic].resize((uint32_t)compIdsGeneric.size());
				newArch->m_compOffs[ComponentKind::CK_Chunk].resize((uint32_t)compIdsChunk.size());
				newArch->UpdateDataOffsets(sizeof(ChunkHeader) + MemoryBlockUsableOffset);

				const auto& cc = ComponentCache::get();
				const auto& dataOffset = newArch->m_dataOffsets;

				// Calculate the number of entities per chunks precisely so we can
				// fit as many of them into chunk as possible.

				// Total size of generic components
				uint32_t genericComponentListSize = 0;
				for (const auto compId: compIdsGeneric) {
					const auto& desc = cc.comp_desc(compId);
					genericComponentListSize += desc.properties.size;
				}

				// Total size of chunk components
				uint32_t chunkComponentListSize = 0;
				for (const auto compId: compIdsChunk) {
					const auto& desc = cc.comp_desc(compId);
					chunkComponentListSize += desc.properties.size;
				}

				const uint32_t size0 = Chunk::chunk_data_bytes(detail::ChunkAllocatorImpl::mem_block_size(0));
				const uint32_t size1 = Chunk::chunk_data_bytes(detail::ChunkAllocatorImpl::mem_block_size(1));
				const auto sizeM = (size0 + size1) / 2;

				uint32_t maxDataOffsetTarget = size1;
				// Theoretical maximum number of components we can fit into one chunk.
				// This can be further reduced due alignment and padding.
				auto maxGenericItemsInArchetype =
						(maxDataOffsetTarget - dataOffset.firstByte_EntityData - chunkComponentListSize - 1) /
						(genericComponentListSize + (uint32_t)sizeof(Entity));

				bool finalCheck = false;
			recalculate:
				auto currOff = dataOffset.firstByte_EntityData + (uint32_t)sizeof(Entity) * maxGenericItemsInArchetype;

				// Adjust the maximum number of entities. Recalculation happens at most once when the original guess
				// for entity count is not right (most likely because of padding or usage of SoA components).
				if (!est_max_entities_per_archetype(
								currOff, maxGenericItemsInArchetype, compIdsGeneric, maxGenericItemsInArchetype, maxDataOffsetTarget))
					goto recalculate;
				if (!est_max_entities_per_archetype(currOff, maxGenericItemsInArchetype, compIdsChunk, 1, maxDataOffsetTarget))
					goto recalculate;

				// Limit the number of entities to a ceratain number so we can make use of smaller
				// chunks where it makes sense.
				// TODO:
				// Tweak this so the full remaining capacity is used. So if we occupy 7000 B we still
				// have 1000 B left to fill.
				if (maxGenericItemsInArchetype > maxEntities) {
					maxGenericItemsInArchetype = maxEntities;
					goto recalculate;
				}

				// We create chunks of either 8K or 16K but might end up with requested capacity 8.1K. Allocating a 16K chunk
				// in this case would be wasteful. Therefore, let's find the middle ground. Anything 12K or smaller we'll
				// allocate into 8K chunks so we avoid wasting too much memory.
				if (!finalCheck && currOff < sizeM) {
					finalCheck = true;
					maxDataOffsetTarget = size0;

					maxGenericItemsInArchetype =
							(maxDataOffsetTarget - dataOffset.firstByte_EntityData - chunkComponentListSize - 1) /
							(genericComponentListSize + (uint32_t)sizeof(Entity));
					goto recalculate;
				}

				// Update the offsets according to the recalculated maxGenericItemsInArchetype
				currOff = dataOffset.firstByte_EntityData + (uint32_t)sizeof(Entity) * maxGenericItemsInArchetype;

				auto registerComponents = [&](ComponentIdSpan compIds, ComponentKind compKind, const uint32_t count) {
					auto& ids = newArch->m_compIds[compKind];
					auto& ofs = newArch->m_compOffs[compKind];

					for (uint32_t i = 0; i < compIds.size(); ++i) {
						const auto compId = compIds[i];
						const auto& desc = cc.comp_desc(compId);
						const auto alignment = desc.properties.alig;
						if (alignment == 0) {
							GAIA_ASSERT(desc.properties.size == 0);

							// Register the component info
							ids[i] = compId;
							ofs[i] = {};
						} else {
							currOff = mem::align(currOff, alignment);

							// Register the component info
							ids[i] = compId;
							ofs[i] = (ChunkComponentOffset)currOff;

							// Make sure the following component list is properly aligned
							currOff += desc.properties.size * count;
						}
					}
				};
				registerComponents(compIdsGeneric, ComponentKind::CK_Generic, maxGenericItemsInArchetype);
				registerComponents(compIdsChunk, ComponentKind::CK_Chunk, 1);

				newArch->m_properties.capacity = (uint16_t)maxGenericItemsInArchetype;
				newArch->m_properties.chunkDataBytes = (uint16_t)currOff;
				GAIA_ASSERT(Chunk::chunk_total_bytes((uint16_t)currOff) < detail::ChunkAllocatorImpl::mem_block_size(currOff));

				newArch->m_matcherHash[ComponentKind::CK_Generic] = ecs::matcher_hash(compIdsGeneric);
				newArch->m_matcherHash[ComponentKind::CK_Chunk] = ecs::matcher_hash(compIdsChunk);

				return newArch;
			}

			/*!
			Sets hashes for each component type and lookup.
			\param hashGeneric Generic components hash
			\param hashChunk Chunk components hash
			\param hashLookup Hash used for archetype lookup purposes
			*/
			void set_hashes(GenericComponentHash hashGeneric, ChunkComponentHash hashChunk, LookupHash hashLookup) {
				m_genericHash = hashGeneric;
				m_chunkHash = hashChunk;
				m_lookupHash = hashLookup;
			}

			/*!
			Enables or disables the entity on a given index in the chunk.
			\param pChunk Chunk the entity belongs to
			\param index Index of the entity
			\param enableEntity Enables the entity
			*/
			void enable_entity(Chunk* pChunk, uint32_t entityIdx, bool enableEntity, std::span<EntityContainer> entities) {
				pChunk->enable_entity(entityIdx, enableEntity, entities);
				// m_disabledMask.set(pChunk->idx(), enableEntity ? true : pChunk->has_disabled_entities());
			}

			/*!
			Removes a chunk from the list of chunks managed by their achetype.
			\param pChunk Chunk to remove from the list of managed archetypes
			*/
			void remove_chunk(Chunk* pChunk) {
				const auto chunkIndex = pChunk->idx();

				Chunk::free(pChunk);

				auto remove = [&](auto& chunkArray) {
					if (chunkArray.size() > 1)
						chunkArray.back()->set_idx(chunkIndex);
					GAIA_ASSERT(chunkIndex == core::get_index(chunkArray, pChunk));
					core::erase_fast(chunkArray, chunkIndex);
				};

				remove(m_chunks);
			}

			//! defragments the chunk.
			//! \param maxEntites Maximum number of entities moved per call
			//! \param chunksToRemove Container of chunks ready for removal
			//! \param entities Container with entities
			void defrag(uint32_t& maxEntities, cnt::darray<Chunk*>& chunksToRemove, std::span<EntityContainer> entities) {
				// Assuming the following chunk layout:
				//   Chunk_1: 10/10
				//   Chunk_2:  1/10
				//   Chunk_3:  7/10
				//   Chunk_4: 10/10
				//   Chunk_5:  9/10
				// After full defragmentation we end up with:
				//   Chunk_1: 10/10
				//   Chunk_2: 10/10 (7 entities from Chunk_3 + 2 entities from Chunk_5)
				//   Chunk_3:  0/10 (empty, ready for removal)
				//   Chunk_4: 10/10
				//   Chunk_5:  7/10
				// TODO:
				// Implement mask of semi-full chunks so we can pick one easily when searching
				// for a chunk to fill with a new entity and when defragmenting.
				// NOTE:
				// Even though entity movement might be present during defragmentation, we do
				// not update the world version here because no real structural changes happen.
				// All entites and components remain intact, they just move to a different place.

				if (m_chunks.empty())
					return;

				uint32_t front = 0;
				uint32_t back = m_chunks.size() - 1;

				// Find the first semi-empty chunk in the front
				while (front < back && !m_chunks[front++]->is_semi())
					;

				auto* pDstChunk = m_chunks[front];

				// Find the first semi-empty chunk in the back
				while (front < back && m_chunks[--back]->is_semi()) {
					auto* pSrcChunk = m_chunks[back];

					const uint32_t entitiesInChunk = pSrcChunk->size();
					const uint32_t entitiesToMove = entitiesInChunk > maxEntities ? maxEntities : entitiesInChunk;
					for (uint32_t i = 0; i < entitiesToMove; ++i) {
						const auto lastEntityIdx = entitiesInChunk - i - 1;
						auto entity = pSrcChunk->get_entity(lastEntityIdx);

						const auto& entityContainer = entities[entity.id()];

						const auto oldIndex = entityContainer.idx;
						const auto newIndex = pDstChunk->add_entity(entity);
						const bool wasEnabled = !entityContainer.dis;

						// Make sure the old entity becomes enabled now
						enable_entity(pSrcChunk, oldIndex, true, entities);
						// We go back-to-front in the chunk so enabling the entity is not expected to change its index
						GAIA_ASSERT(oldIndex == entityContainer.idx);

						// Transfer the original enabled state to the new chunk
						enable_entity(pDstChunk, newIndex, wasEnabled, entities);

						// Remove the entity record from the old chunk
						pSrcChunk->remove_entity(oldIndex, entities, chunksToRemove);

						// The destination chunk is full, we need to move to the next one
						if (pDstChunk->size() == m_properties.capacity) {
							++front;

							// We reached the source chunk which means this archetype has been defragmented
							if (front >= back) {
								maxEntities -= i + 1;
								return;
							}
						}
					}

					maxEntities -= entitiesToMove;
				}
			}

			//! Tries to locate a chunk that has some space left for a new entity.
			//! If not found a new chunk is created.
			GAIA_NODISCARD Chunk* foc_free_chunk() {
				const auto chunkCnt = m_chunks.size();

				if (chunkCnt > 0) {
					// Find first semi-empty chunk.
					// Picking the first non-full would only support fragmentation.
					Chunk* pEmptyChunk = nullptr;
					for (auto* pChunk: m_chunks) {
						GAIA_ASSERT(pChunk != nullptr);
						const auto entityCnt = pChunk->size();
						if GAIA_UNLIKELY (entityCnt == 0)
							pEmptyChunk = pChunk;
						else if (entityCnt < pChunk->capacity())
							return pChunk;
					}
					if (pEmptyChunk != nullptr)
						return pEmptyChunk;
				}

				// Make sure not too many chunks are allocated
				GAIA_ASSERT(chunkCnt < UINT32_MAX);

				// No free space found anywhere. Let's create a new chunk.
				auto* pChunk = Chunk::create(
						m_archetypeId, chunkCnt, m_properties.capacity, m_properties.chunkDataBytes, m_worldVersion, m_dataOffsets,
						m_compIds, m_compOffs);

				m_chunks.push_back(pChunk);
				return pChunk;
			}

			GAIA_NODISCARD uint32_t capacity() const {
				return m_properties.capacity;
			}

			GAIA_NODISCARD const cnt::darray<Chunk*>& chunks() const {
				return m_chunks;
			}

			GAIA_NODISCARD GenericComponentHash generic_hash() const {
				return m_genericHash;
			}

			GAIA_NODISCARD ChunkComponentHash chunk_hash() const {
				return m_chunkHash;
			}

			GAIA_NODISCARD LookupHash lookup_hash() const {
				return m_lookupHash;
			}

			GAIA_NODISCARD ComponentMatcherHash matcher_hash(ComponentKind compKind) const {
				return m_matcherHash[compKind];
			}

			GAIA_NODISCARD const Chunk::ComponentIdArray& comp_ids(ComponentKind compKind) const {
				return m_compIds[compKind];
			}

			GAIA_NODISCARD const Chunk::ComponentOffsetArray& comp_offsets(ComponentKind compKind) const {
				return m_compOffs[compKind];
			}

			/*!
			Checks if a component of type \tparam T is present in the archetype.
			\return True if found. False otherwise.
			*/
			template <typename T>
			GAIA_NODISCARD bool has() const {
				const auto compId = comp_id<T>();

				constexpr auto compKind = component_kind_v<T>;
				return has_inter(compKind, compId);
			}

			/*!
			Returns the internal index of a component based on the provided \param compId.
			\param compKind Component type
			\return Component index if the component was found. -1 otherwise.
			*/
			GAIA_NODISCARD uint32_t comp_idx(ComponentKind compKind, ComponentId compId) const {
				const auto idx = core::get_index_unsafe(m_compIds[compKind], compId);
				GAIA_ASSERT(idx != BadIndex);
				return (uint32_t)idx;
			}

			void build_graph_edges(Archetype* pArchetypeRight, ComponentKind compKind, ComponentId compId) {
				GAIA_ASSERT(pArchetypeRight != this);
				m_graph.add_edge_right(compKind, compId, pArchetypeRight->archetype_id());
				pArchetypeRight->build_graph_edges_left(this, compKind, compId);
			}

			void build_graph_edges_left(Archetype* pArchetypeLeft, ComponentKind compKind, ComponentId compId) {
				GAIA_ASSERT(pArchetypeLeft != this);
				m_graph.add_edge_left(compKind, compId, pArchetypeLeft->archetype_id());
			}

			//! Checks if the graph edge for component type \param compKind contains the component \param compId.
			//! \return Archetype id of the target archetype if the edge is found. ArchetypeIdBad otherwise.
			GAIA_NODISCARD ArchetypeId find_edge_right(ComponentKind compKind, const ComponentId compId) const {
				return m_graph.find_edge_right(compKind, compId);
			}

			//! Checks if the graph edge for component type \param compKind contains the component \param compId.
			//! \return Archetype id of the target archetype if the edge is found. ArchetypeIdBad otherwise.
			GAIA_NODISCARD ArchetypeId find_edge_left(ComponentKind compKind, const ComponentId compId) const {
				return m_graph.find_edge_left(compKind, compId);
			}

			static void diag_basic_info(const Archetype& archetype) {
				const auto& cc = ComponentCache::get();
				const auto& genericComponents = archetype.m_compIds[ComponentKind::CK_Generic];
				const auto& chunkComponents = archetype.m_compIds[ComponentKind::CK_Chunk];

				// Caclulate the number of entites in archetype
				uint32_t entCnt = 0;
				uint32_t entCntDisabled = 0;
				for (const auto* chunk: archetype.m_chunks) {
					entCnt += chunk->size();
					entCntDisabled += chunk->size_disabled();
				}

				// Calculate the number of components
				uint32_t genericComponentsSize = 0;
				uint32_t chunkComponentsSize = 0;
				for (const auto compId: genericComponents) {
					const auto& desc = cc.comp_desc(compId);
					genericComponentsSize += desc.properties.size;
				}
				for (const auto compId: chunkComponents) {
					const auto& desc = cc.comp_desc(compId);
					chunkComponentsSize += desc.properties.size;
				}

				GAIA_LOG_N(
						"Archetype ID:%u, "
						"lookupHash:%016" PRIx64 ", "
						"mask:%016" PRIx64 "/%016" PRIx64 ", "
						"chunks:%u (%uK), data:%u/%u/%u B, "
						"entities:%u/%u/%u",
						archetype.m_archetypeId, archetype.m_lookupHash.hash,
						archetype.m_matcherHash[ComponentKind::CK_Generic].hash,
						archetype.m_matcherHash[ComponentKind::CK_Chunk].hash, (uint32_t)archetype.m_chunks.size(),
						Chunk::chunk_total_bytes(archetype.m_properties.chunkDataBytes) <= 8192 ? 8 : 16, genericComponentsSize,
						chunkComponentsSize, archetype.m_properties.chunkDataBytes, entCnt, entCntDisabled,
						archetype.m_properties.capacity);

				auto logComponentInfo = [](const ComponentInfo& info, const ComponentDesc& desc) {
					GAIA_LOG_N(
							"    lookupHash:%016" PRIx64 ", mask:%016" PRIx64 ", size:%3u B, align:%3u B, %.*s", info.lookupHash.hash,
							info.matcherHash.hash, desc.properties.size, desc.properties.alig, (uint32_t)desc.name.size(),
							desc.name.data());
				};

				if (!genericComponents.empty()) {
					GAIA_LOG_N("  Generic components - count:%u", (uint32_t)genericComponents.size());
					for (const auto compId: genericComponents) {
						const auto& info = cc.comp_info(compId);
						logComponentInfo(info, cc.comp_desc(compId));
					}
					if (!chunkComponents.empty()) {
						GAIA_LOG_N("  Chunk components - count:%u", (uint32_t)chunkComponents.size());
						for (const auto compId: chunkComponents) {
							const auto& info = cc.comp_info(compId);
							logComponentInfo(info, cc.comp_desc(compId));
						}
					}
				}
			}

			static void diag_graph_info(const Archetype& archetype) {
				archetype.m_graph.diag();
			}

			static void diag_chunk_info(const Archetype& archetype) {
				auto logChunks = [](const auto& chunks) {
					for (uint32_t i = 0; i < chunks.size(); ++i) {
						const auto* pChunk = chunks[i];
						pChunk->diag((uint32_t)i);
					}
				};

				const auto& chunks = archetype.m_chunks;
				if (!chunks.empty())
					GAIA_LOG_N("  Chunks");

				logChunks(chunks);
			}

			/*!
			Performs diagnostics on a specific archetype. Prints basic info about it and the chunks it contains.
			\param archetype Archetype to run diagnostics on
			*/
			static void diag(const Archetype& archetype) {
				diag_basic_info(archetype);
				diag_graph_info(archetype);
				diag_chunk_info(archetype);
			}
		};

		class ArchetypeLookupKey final {
			Archetype::LookupHash m_hash;
			ArchetypeBase* m_pArchetypeBase;

		public:
			static constexpr bool IsDirectHashKey = true;

			ArchetypeLookupKey(): m_hash({0}), m_pArchetypeBase(nullptr) {}
			ArchetypeLookupKey(Archetype::LookupHash hash, ArchetypeBase* pArchetypeBase):
					m_hash(hash), m_pArchetypeBase(pArchetypeBase) {}

			size_t hash() const {
				return (size_t)m_hash.hash;
			}

			bool operator==(const ArchetypeLookupKey& other) const {
				// Hash doesn't match we don't have a match.
				// Hash collisions are expected to be very unlikely so optimize for this case.
				if GAIA_LIKELY (m_hash != other.m_hash)
					return false;

				const auto id = m_pArchetypeBase->archetype_id();
				if (id == ArchetypeIdBad) {
					const auto* pArchetype = (const Archetype*)other.m_pArchetypeBase;
					const auto* pArchetypeLookupChecker = (const ArchetypeLookupChecker*)m_pArchetypeBase;
					return pArchetype->cmp_comp_ids(*pArchetypeLookupChecker);
				}

				// Real ArchetypeID is given. Compare the pointers.
				// Normally we'd compare archetype IDs but because we do not allow archetype copies and all archetypes are
				// unique it's guaranteed that if pointers are the same we have a match.
				return m_pArchetypeBase == other.m_pArchetypeBase;
			}
		};
	} // namespace ecs
} // namespace gaia

#include <cinttypes>
#include <type_traits>

#include <cstdint>

namespace gaia {
	namespace ecs {
		struct ComponentGetter {
			const Chunk* m_pChunk;
			uint32_t m_idx;

			//! Returns the value stored in the component \tparam T on \param entity.
			//! \tparam T Component
			//! \return Value stored in the component.
			template <typename T>
			GAIA_NODISCARD auto get() const {
				verify_comp<T>();

				if constexpr (component_kind_v<T> == ComponentKind::CK_Generic)
					return m_pChunk->template get<T>(m_idx);
				else
					return m_pChunk->template get<T>();
			}

			//! Tells if \param entity contains the component \tparam T.
			//! \tparam T Component
			//! \return True if the component is present on entity.
			template <typename T>
			GAIA_NODISCARD bool has() const {
				verify_comp<T>();

				return m_pChunk->template has<T>();
			}
		};
	} // namespace ecs
} // namespace gaia

#include <cstdint>

namespace gaia {
	namespace ecs {
		struct ComponentSetter {
			Chunk* m_pChunk;
			uint32_t m_idx;

			//! Sets the value of the component \tparam T on \param entity.
			//! \tparam T Component
			//! \param value Value to set for the component
			//! \return ComponentSetter
			template <typename T, typename U = typename component_kind_t<T>::Kind>
			U& set() {
				verify_comp<T>();

				if constexpr (component_kind_v<T> == ComponentKind::CK_Generic)
					return m_pChunk->template set<T>(m_idx);
				else
					return m_pChunk->template set<T>();
			}

			//! Sets the value of the component \tparam T on \param entity.
			//! \tparam T Component
			//! \param value Value to set for the component
			//! \return ComponentSetter
			template <typename T, typename U = typename component_kind_t<T>::Kind>
			ComponentSetter& set(U&& data) {
				verify_comp<T>();

				if constexpr (component_kind_v<T> == ComponentKind::CK_Generic)
					m_pChunk->template set<T>(m_idx, std::forward<U>(data));
				else
					m_pChunk->template set<T>(std::forward<U>(data));
				return *this;
			}

			//! Sets the value of the component \tparam T on \param entity without trigger a world version update.
			//! \tparam T Component
			//! \param value Value to set for the component
			//! \return ComponentSetter
			template <typename T, typename U = typename component_kind_t<T>::Kind>
			ComponentSetter& sset(U&& data) {
				verify_comp<T>();

				if constexpr (component_kind_v<T> == ComponentKind::CK_Generic)
					m_pChunk->template sset<T>(m_idx, std::forward<U>(data));
				else
					m_pChunk->template sset<T>(std::forward<U>(data));
				return *this;
			}
		};
	} // namespace ecs
} // namespace gaia

namespace gaia {
	namespace ecs {
		namespace detail {
			class ChunkAccessor {
			protected:
				Chunk& m_chunk;

			public:
				ChunkAccessor(Chunk& chunk): m_chunk(chunk) {}

				//! Checks if component \tparam T is present in the chunk.
				//! \tparam T Component
				//! \return True if the component is present. False otherwise.
				template <typename T>
				GAIA_NODISCARD bool has() const {
					return m_chunk.has<T>();
				}

				//! Checks if the entity at the current iterator index is enabled.
				//! \return True it the entity is enabled. False otherwise.
				GAIA_NODISCARD bool enabled(uint32_t entityIdx) const {
					return entityIdx >= m_chunk.size_disabled() && entityIdx < m_chunk.size();
				}

				//! Returns a read-only entity or component view.
				//! \warning If \tparam T is a component it is expected it is present. Undefined behavior otherwise.
				//! \tparam T Component or Entity
				//! \return Entity of component view with read-only access
				template <typename T>
				GAIA_NODISCARD auto view() const {
					return m_chunk.view<T>();
				}

				//! Returns a mutable entity or component view.
				//! \warning If \tparam T is a component it is expected it is present. Undefined behavior otherwise.
				//! \tparam T Component or Entity
				//! \return Entity or component view with read-write access
				template <typename T>
				GAIA_NODISCARD auto view_mut() {
					return m_chunk.view_mut<T>();
				}

				//! Returns a mutable component view.
				//! Doesn't update the world version when the access is aquired.
				//! \warning It is expected the component \tparam T is present. Undefined behavior otherwise.
				//! \tparam T Component
				//! \return Component view with read-write access
				template <typename T>
				GAIA_NODISCARD auto sview_mut() {
					return m_chunk.sview_mut<T>();
				}
			};
		} // namespace detail

		struct ChunkAccessorIt {
			using value_type = uint32_t;

		protected:
			value_type m_pos;

		public:
			ChunkAccessorIt(value_type pos) noexcept: m_pos(pos) {}

			GAIA_NODISCARD value_type operator*() const noexcept {
				return m_pos;
			}

			GAIA_NODISCARD value_type operator->() const noexcept {
				return m_pos;
			}

			ChunkAccessorIt operator++() noexcept {
				++m_pos;
				return *this;
			}

			GAIA_NODISCARD ChunkAccessorIt operator++(int) noexcept {
				ChunkAccessorIt temp(*this);
				++*this;
				return temp;
			}

			GAIA_NODISCARD bool operator==(const ChunkAccessorIt& other) const noexcept {
				return m_pos == other.m_pos;
			}

			GAIA_NODISCARD bool operator!=(const ChunkAccessorIt& other) const noexcept {
				return m_pos != other.m_pos;
			}

			GAIA_NODISCARD bool operator<(const ChunkAccessorIt& other) const noexcept {
				return m_pos < other.m_pos;
			}
		};
	} // namespace ecs
} // namespace gaia

#include <cinttypes>
#include <type_traits>

#include <type_traits>

namespace gaia {
	namespace ecs {
		class SerializationBuffer {
			// Increase the capacity by multiples of CapacityIncreaseSize
			static constexpr uint32_t CapacityIncreaseSize = 128U;
			// TODO: Replace with some memory allocator
			using DataContainer = cnt::darray_ext<uint8_t, CapacityIncreaseSize>;

			//! Buffer holding raw data
			DataContainer m_data;
			//! Current position in the buffer
			uint32_t m_dataPos = 0;

		public:
			void reset() {
				m_dataPos = 0;
				m_data.clear();
			}

			//! Returns the number of bytes written in the buffer
			GAIA_NODISCARD uint32_t bytes() const {
				return (uint32_t)m_data.size();
			}

			//! Returns true if there is no data written in the buffer
			GAIA_NODISCARD bool empty() const {
				return m_data.empty();
			}

			//! Makes sure there is enough capacity in our data container to hold another \param size bytes of data
			void reserve(uint32_t size) {
				const auto nextSize = m_dataPos + size;
				if (nextSize <= bytes())
					return;

				// Make sure there is enough capacity to hold our data
				const auto newSize = bytes() + size;
				const auto newCapacity = (newSize / CapacityIncreaseSize) * CapacityIncreaseSize + CapacityIncreaseSize;
				m_data.reserve(newCapacity);
			}

			//! Changes the current position in the buffer
			void seek(uint32_t pos) {
				m_dataPos = pos;
			}

			//! Returns the current position in the buffer
			GAIA_NODISCARD uint32_t tell() const {
				return m_dataPos;
			}

			//! Writes \param value to the buffer
			template <typename T>
			void save(T&& value) {
				reserve(sizeof(T));

				m_data.resize(m_dataPos + sizeof(T));
				mem::unaligned_ref<T> mem(&m_data[m_dataPos]);
				mem = std::forward<T>(value);

				m_dataPos += sizeof(T);
			}

			//! Writes \param size bytes of data starting at the address \param pSrc to the buffer
			void save(const void* pSrc, uint32_t size) {
				reserve(size);

				// Copy "size" bytes of raw data starting at pSrc
				m_data.resize(m_dataPos + size);
				memcpy((void*)&m_data[m_dataPos], pSrc, size);

				m_dataPos += size;
			}

			//! Writes \param value to the buffer
			template <typename T>
			void save_comp(T&& value) {
				const auto compId = comp_id<T>();
				const auto& desc = ComponentCache::get().comp_desc(compId);
				const bool isManualDestroyNeeded = desc.func_ctor_copy != nullptr || desc.func_ctor_move != nullptr;
				constexpr bool isRValue = std::is_rvalue_reference_v<decltype(value)>;

				reserve(sizeof(isManualDestroyNeeded) + sizeof(T));
				save(isManualDestroyNeeded);
				m_data.resize(m_dataPos + sizeof(T));

				auto* pSrc = (void*)&value;
				auto* pDst = (void*)&m_data[m_dataPos];
				if (isRValue && desc.func_ctor_move != nullptr)
					desc.func_ctor_move(pSrc, pDst);
				else if (desc.func_ctor_copy != nullptr)
					desc.func_ctor_copy(pSrc, pDst);
				else
					memmove(pDst, (const void*)pSrc, sizeof(T));

				m_dataPos += sizeof(T);
			}

			//! Loads \param value from the buffer
			template <typename T>
			void load(T& value) {
				GAIA_ASSERT(m_dataPos + sizeof(T) <= bytes());

				value = mem::unaligned_ref<T>((void*)&m_data[m_dataPos]);

				m_dataPos += sizeof(T);
			}

			//! Loads \param size bytes of data from the buffer and writes them to the address \param pDst
			void load(void* pDst, uint32_t size) {
				GAIA_ASSERT(m_dataPos + size <= bytes());

				memcpy(pDst, (void*)&m_data[m_dataPos], size);

				m_dataPos += size;
			}

			//! Loads \param value from the buffer
			void load_comp(void* pDst, ComponentId compId) {
				bool isManualDestroyNeeded = false;
				load(isManualDestroyNeeded);

				const auto& desc = ComponentCache::get().comp_desc(compId);
				GAIA_ASSERT(m_dataPos + desc.properties.size <= bytes());

				auto* pSrc = (void*)&m_data[m_dataPos];
				desc.move(pSrc, pDst);
				if (isManualDestroyNeeded)
					desc.dtor(pSrc);

				m_dataPos += desc.properties.size;
			}
		};
	} // namespace ecs
} // namespace gaia

#include <cinttypes>
#include <type_traits>

#include <type_traits>

#include <cinttypes>
#include <type_traits>

namespace gaia {
	namespace ecs {
		namespace detail {
			template <typename Func>
			void each(const uint32_t idxFrom, const uint32_t idxStop, Func func) noexcept {
				if constexpr (std::is_invocable_v<Func, uint32_t>) {
					for (auto i = idxFrom; i < idxStop; ++i)
						func(i);
				} else {
					for (auto i = idxFrom; i < idxStop; ++i)
						func();
				}
			}
		} // namespace detail

		struct Iterator: public detail::ChunkAccessor {
			using Iter = ChunkAccessorIt;

			Iterator(Chunk& chunk) noexcept: detail::ChunkAccessor(chunk) {}

			//! Returns the number of entities accessible via the iterator
			GAIA_NODISCARD uint32_t size() const noexcept {
				return m_chunk.size_enabled();
			}

			template <typename Func>
			void each(Func func) noexcept {
				const auto idxFrom = m_chunk.size_disabled();
				const auto idxStop = m_chunk.size();
				detail::each(idxFrom, idxStop, func);
			}
		};

		struct IteratorDisabled: public detail::ChunkAccessor {
			using Iter = ChunkAccessorIt;

			IteratorDisabled(Chunk& chunk) noexcept: detail::ChunkAccessor(chunk) {}

			//! Returns the number of entities accessible via the iterator
			GAIA_NODISCARD uint32_t size() const noexcept {
				return m_chunk.size_disabled();
			}

			template <typename Func>
			void each(Func func) noexcept {
				const auto idxFrom = 0;
				const auto idxStop = m_chunk.size_disabled();
				detail::each(idxFrom, idxStop, func);
			}
		};

		struct IteratorAll: public detail::ChunkAccessor {
			using Iter = ChunkAccessorIt;

			IteratorAll(Chunk& chunk) noexcept: detail::ChunkAccessor(chunk) {}

			//! Returns the number of entities accessible via the iterator
			GAIA_NODISCARD uint32_t size() const noexcept {
				return m_chunk.size();
			}

			template <typename Func>
			void each(Func func) noexcept {
				const auto idxFrom = 0;
				const auto idxStop = m_chunk.size();
				detail::each(idxFrom, idxStop, func);
			}
		};
	} // namespace ecs
} // namespace gaia

#include <type_traits>

namespace gaia {
	namespace ecs {
		//! Number of components that can be a part of Query
		static constexpr uint32_t MAX_COMPONENTS_IN_QUERY = 8U;

		//! List type
		enum QueryListType : uint8_t { LT_None, LT_Any, LT_All, LT_Count };

		//! Exection mode
		enum class QueryExecMode : uint8_t {
			//! Run on the main thread
			Run,
			//! Run on a single worker thread
			Single,
			//! Run on as many worker threads as possible
			Parallel
		};

		using QueryId = uint32_t;
		using QueryLookupHash = core::direct_hash_key<uint64_t>;
		using QueryComponentIdArray = cnt::sarray_ext<ComponentId, MAX_COMPONENTS_IN_QUERY>;
		using QueryListTypeArray = cnt::sarray_ext<QueryListType, MAX_COMPONENTS_IN_QUERY>;
		using QueryChangeArray = cnt::sarray_ext<ComponentId, MAX_COMPONENTS_IN_QUERY>;

		static constexpr QueryId QueryIdBad = (QueryId)-1;

		struct QueryCtx {
			//! Lookup hash for this query
			QueryLookupHash hashLookup{};
			//! Query id
			QueryId queryId = QueryIdBad;

			struct Data {
				//! List of querried components
				QueryComponentIdArray compIds;
				//! Filtering rules for the components
				QueryListTypeArray rules;
				//! List of component matcher hashes
				ComponentMatcherHash hash[QueryListType::LT_Count];
				//! Array of indiices to the last checked archetype in the component-to-archetype map
				cnt::darray<uint32_t> lastMatchedArchetypeIndex;
				//! List of filtered components
				QueryChangeArray withChanged;
				//! Read-write mask. Bit 0 stands for component 0 in component arrays.
				//! A set bit means write access is requested.
				uint8_t readWriteMask;
				//! The number of components which are required for the query to match
				uint8_t rulesAllCount;
			} data[ComponentKind::CK_Count]{};
			static_assert(MAX_COMPONENTS_IN_QUERY == 8); // Make sure that MAX_COMPONENTS_IN_QUERY can fit into m_rw

			GAIA_NODISCARD bool operator==(const QueryCtx& other) const {
				// Fast path when cache ids are set
				if (queryId != QueryIdBad && queryId == other.queryId)
					return true;

				// Lookup hash must match
				if (hashLookup != other.hashLookup)
					return false;

				for (uint32_t i = 0; i < ComponentKind::CK_Count; ++i) {
					const auto& left = data[i];
					const auto& right = other.data[i];

					// Check array sizes first
					if (left.compIds.size() != right.compIds.size())
						return false;
					if (left.rules.size() != right.rules.size())
						return false;
					if (left.withChanged.size() != right.withChanged.size())
						return false;
					if (left.readWriteMask != right.readWriteMask)
						return false;

					// Matches hashes need to be the same
					for (uint32_t j = 0; j < QueryListType::LT_Count; ++j) {
						if (left.hash[j] != right.hash[j])
							return false;
					}

					// Components need to be the same
					for (uint32_t j = 0; j < left.compIds.size(); ++j) {
						if (left.compIds[j] != right.compIds[j])
							return false;
					}

					// Rules need to be the same
					for (uint32_t j = 0; j < left.rules.size(); ++j) {
						if (left.rules[j] != right.rules[j])
							return false;
					}

					// Filters need to be the same
					for (uint32_t j = 0; j < left.withChanged.size(); ++j) {
						if (left.withChanged[j] != right.withChanged[j])
							return false;
					}
				}

				return true;
			}

			GAIA_NODISCARD bool operator!=(const QueryCtx& other) const {
				return !operator==(other);
			};
		};

		//! Sorts internal component arrays
		inline void sort(QueryCtx& ctx) {
			for (uint32_t i = 0; i < ComponentKind::CK_Count; ++i) {
				auto& data = ctx.data[i];
				// Make sure the read-write mask remains correct after sorting
				core::sort(data.compIds, SortComponentCond{}, [&](uint32_t left, uint32_t right) {
					core::swap(data.compIds[left], data.compIds[right]);
					core::swap(data.rules[left], data.rules[right]);

					{
						// Swap the bits in the read-write mask
						const uint32_t b0 = (data.readWriteMask >> left) & 1U;
						const uint32_t b1 = (data.readWriteMask >> right) & 1U;
						// XOR the two bits
						const uint32_t bxor = b0 ^ b1;
						// Put the XOR bits back to their original positions
						const uint32_t mask = (bxor << left) | (bxor << right);
						// XOR mask with the original one effectivelly swapping the bits
						data.readWriteMask = data.readWriteMask ^ (uint8_t)mask;
					}
				});
			}
		}

		inline void matcher_hashes(QueryCtx& ctx) {
			// Sort the arrays if necessary
			sort(ctx);

			// Calculate the matcher hash
			for (auto& data: ctx.data) {
				for (uint32_t i = 0; i < data.rules.size(); ++i)
					matcher_hash(data.hash[data.rules[i]], data.compIds[i]);
			}
		}

		inline void calc_lookup_hash(QueryCtx& ctx) {
			// Make sure we don't calculate the hash twice
			GAIA_ASSERT(ctx.hashLookup.hash == 0);

			QueryLookupHash::Type hashLookup = 0;

			for (uint32_t i = 0; i < ComponentKind::CK_Count; ++i) {
				auto& data = ctx.data[i];

				// Components
				{
					QueryLookupHash::Type hash = 0;

					const auto& compIds = data.compIds;
					for (const auto compId: compIds)
						hash = core::hash_combine(hash, (QueryLookupHash::Type)compId);
					hash = core::hash_combine(hash, (QueryLookupHash::Type)compIds.size());

					hash = core::hash_combine(hash, (QueryLookupHash::Type)data.readWriteMask);
					hashLookup = core::hash_combine(hashLookup, hash);
				}

				// Rules
				{
					QueryLookupHash::Type hash = 0;

					const auto& rules = data.withChanged;
					for (auto listType: rules)
						hash = core::hash_combine(hash, (QueryLookupHash::Type)listType);
					hash = core::hash_combine(hash, (QueryLookupHash::Type)rules.size());

					hashLookup = core::hash_combine(hashLookup, hash);
				}

				// Filters
				{
					QueryLookupHash::Type hash = 0;

					const auto& withChanged = data.withChanged;
					for (auto compId: withChanged)
						hash = core::hash_combine(hash, (QueryLookupHash::Type)compId);
					hash = core::hash_combine(hash, (QueryLookupHash::Type)withChanged.size());

					hashLookup = core::hash_combine(hashLookup, hash);
				}
			}

			ctx.hashLookup = {core::calculate_hash64(hashLookup)};
		}
	} // namespace ecs
} // namespace gaia

namespace gaia {
	namespace ecs {
		struct Entity;

		using ComponentToArchetypeMap = cnt::map<ComponentId, ArchetypeList>;

		class QueryInfo {
		public:
			//! Query matching result
			enum class MatchArchetypeQueryRet : uint8_t { Fail, Ok, Skip };

		private:
			//! Lookup context
			QueryCtx m_lookupCtx;
			//! List of archetypes matching the query
			ArchetypeList m_archetypeCache;
			//! Index of the last archetype in the world we checked
			uint32_t m_lastArchetypeIdx = 1; // skip the root archetype
			//! Version of the world for which the query has been called most recently
			uint32_t m_worldVersion = 0;

			template <typename T>
			bool has_inter(
					[[maybe_unused]] QueryListType listType, [[maybe_unused]] ComponentKind compKind, bool isReadWrite) const {
				if constexpr (std::is_same_v<T, Entity>) {
					// Skip Entity input args
					return true;
				} else {
					const auto& data = m_lookupCtx.data[compKind];
					const auto& compIds = data.compIds;

					// Component id has to be present
					const auto compId = comp_id<T>();
					GAIA_ASSERT(core::has(compIds, compId));
					const auto idx = core::get_index_unsafe(compIds, compId);
					if (listType != QueryListType::LT_Count && listType != data.rules[idx])
						return false;

					// Read-write mask must match
					const uint32_t maskRW = (uint32_t)data.readWriteMask & (1U << (uint32_t)idx);
					const uint32_t maskXX = (uint32_t)isReadWrite << idx;
					return maskRW == maskXX;
				}
			}

			template <typename T>
			bool has_inter(QueryListType listType) const {
				using U = typename component_kind_t<T>::Kind;
				using UOriginal = typename component_kind_t<T>::KindOriginal;
				using UOriginalPR = std::remove_reference_t<std::remove_pointer_t<UOriginal>>;
				constexpr bool isReadWrite =
						std::is_same_v<U, UOriginal> || (!std::is_const_v<UOriginalPR> && !std::is_empty_v<U>);

				constexpr auto compKind = component_kind_v<T>;
				return has_inter<U>(listType, compKind, isReadWrite);
			}

			//! Tries to match query component ids with those in \param compIds given the rule \param func.
			//! \return True if there is a match, false otherwise.
			template <typename Func>
			GAIA_NODISCARD bool match_inter(
					ComponentKind compKind, const Chunk::ComponentIdArray& compIds, QueryListType listType, Func func) const {
				const auto& data = m_lookupCtx.data[compKind];

				// Arrays are sorted so we can do linear intersection lookup
				uint32_t i = 0;
				uint32_t j = 0;
				while (i < compIds.size() && j < data.compIds.size()) {
					if (data.rules[j] == listType) {
						const auto compIdArchetype = compIds[i];
						const auto compIdQuery = data.compIds[j];

						if (compIdArchetype == compIdQuery && func(compIdArchetype, compIdQuery))
							return true;

						if (SortComponentCond{}.operator()(compIdArchetype, compIdQuery)) {
							++i;
							continue;
						}
					}
					++j;
				}

				return false;
			}

			//! Tries to match all query component ids with those in \param compIds.
			//! \return True on the first match, false otherwise.
			GAIA_NODISCARD bool
			match_one(ComponentKind compKind, const Chunk::ComponentIdArray& compIds, QueryListType listType) const {
				return match_inter(compKind, compIds, listType, [](ComponentId compId, ComponentId compIdQuery) {
					return compId == compIdQuery;
				});
			}

			//! Tries to match all query component ids with those in \param compIds.
			//! \return True if all ids match, false otherwise.
			GAIA_NODISCARD bool match_all(ComponentKind compKind, const Chunk::ComponentIdArray& compIds) const {
				uint32_t matches = 0;
				const auto& data = m_lookupCtx.data[compKind];
				return match_inter(compKind, compIds, QueryListType::LT_All, [&](ComponentId compId, ComponentId compIdQuery) {
					return compId == compIdQuery && (++matches == data.rulesAllCount);
				});
			}

			//! Tries to match component with component type \param compKind from the archetype \param archetype with
			//! the query. \return MatchArchetypeQueryRet::Fail if there is no match, MatchArchetypeQueryRet::Ok for match
			//! or MatchArchetypeQueryRet::Skip is not relevant.
			GAIA_NODISCARD MatchArchetypeQueryRet match(const Archetype& archetype, ComponentKind compKind) const {
				const auto& matcherHash = archetype.matcher_hash(compKind);
				const auto& compIds = archetype.comp_ids(compKind);
				const auto& compData = data(compKind);

				const auto withNoneTest = matcherHash.hash & compData.hash[QueryListType::LT_None].hash;
				const auto withAnyTest = matcherHash.hash & compData.hash[QueryListType::LT_Any].hash;
				const auto withAllTest = matcherHash.hash & compData.hash[QueryListType::LT_All].hash;

				// If withAnyTest is empty but we wanted something
				if (withAnyTest == 0 && compData.hash[QueryListType::LT_Any].hash != 0)
					return MatchArchetypeQueryRet::Fail;

				// If withAllTest is empty but we wanted something
				if (withAllTest == 0 && compData.hash[QueryListType::LT_All].hash != 0)
					return MatchArchetypeQueryRet::Fail;

				// If there is any match with withNoneList we quit
				if (withNoneTest != 0) {
					if (match_one(compKind, compIds, QueryListType::LT_None))
						return MatchArchetypeQueryRet::Fail;
				}

				// If there is any match with withAnyTest
				if (withAnyTest != 0) {
					if (match_one(compKind, compIds, QueryListType::LT_Any))
						goto checkWithAllMatches;

					// At least one match necessary to continue
					return MatchArchetypeQueryRet::Fail;
				}

			checkWithAllMatches:
				// If withAllList is not empty there has to be an exact match
				if (withAllTest != 0) {
					if (
							// We skip archetypes with less components than the query requires
							compData.rulesAllCount <= compIds.size() &&
							// Match everything with LT_ALL
							match_all(compKind, compIds))
						return MatchArchetypeQueryRet::Ok;

					// No match found. We're done
					return MatchArchetypeQueryRet::Fail;
				}

				return (withAnyTest != 0) ? MatchArchetypeQueryRet::Ok : MatchArchetypeQueryRet::Skip;
			}

		public:
			GAIA_NODISCARD static QueryInfo create(QueryId id, QueryCtx&& ctx) {
				QueryInfo info;
				matcher_hashes(ctx);
				info.m_lookupCtx = std::move(ctx);
				info.m_lookupCtx.queryId = id;
				return info;
			}

			void set_world_version(uint32_t version) {
				m_worldVersion = version;
			}

			GAIA_NODISCARD uint32_t world_version() const {
				return m_worldVersion;
			}

			QueryLookupHash lookup_hash() const {
				return m_lookupCtx.hashLookup;
			}

			GAIA_NODISCARD bool operator==(const QueryCtx& other) const {
				return m_lookupCtx == other;
			}

			GAIA_NODISCARD bool operator!=(const QueryCtx& other) const {
				return !operator==(other);
			}

			//! Tries to match the query against archetypes in \param componentToArchetypeMap. For each matched archetype
			//! the archetype is cached. This is necessary so we do not iterate all chunks over and over again when running
			//! queries.
			void match(const ComponentToArchetypeMap& componentToArchetypeMap, uint32_t archetypeCount) {
				static cnt::set<Archetype*> s_tmpArchetypeMatches;

				// Skip if no new archetype appeared
				if (m_lastArchetypeIdx == archetypeCount)
					return;
				m_lastArchetypeIdx = archetypeCount;

				// Match against generic types
				{
					auto& data = m_lookupCtx.data[ComponentKind::CK_Generic];
					for (uint32_t i = 0; i < data.compIds.size(); ++i) {
						const auto compId = data.compIds[i];

						const auto it = componentToArchetypeMap.find(compId);
						if (it == componentToArchetypeMap.end())
							continue;

						for (uint32_t j = data.lastMatchedArchetypeIndex[i]; j < it->second.size(); ++j) {
							auto* pArchetype = it->second[j];
							// Early exit if generic query doesn't match
							const auto retGeneric = match(*pArchetype, ComponentKind::CK_Generic);
							if (retGeneric == MatchArchetypeQueryRet::Fail)
								continue;

							(void)s_tmpArchetypeMatches.emplace(pArchetype);
						}
						data.lastMatchedArchetypeIndex[i] = (uint32_t)it->second.size();
					}
				}

				// Match against chunk types
				{
					auto& data = m_lookupCtx.data[ComponentKind::CK_Chunk];
					for (uint32_t i = 0; i < data.compIds.size(); ++i) {
						const auto compId = data.compIds[i];

						const auto it = componentToArchetypeMap.find(compId);
						if (it == componentToArchetypeMap.end())
							continue;

						for (uint32_t j = data.lastMatchedArchetypeIndex[i]; j < it->second.size(); ++j) {
							auto* pArchetype = it->second[j];
							// Early exit if generic query doesn't match
							const auto retGeneric = match(*pArchetype, ComponentKind::CK_Chunk);
							if (retGeneric == MatchArchetypeQueryRet::Fail) {
								s_tmpArchetypeMatches.erase(pArchetype);
								continue;
							}

							(void)s_tmpArchetypeMatches.emplace(pArchetype);
						}
						data.lastMatchedArchetypeIndex[i] = (uint32_t)it->second.size();
					}
				}

				for (auto* pArchetype: s_tmpArchetypeMatches)
					m_archetypeCache.push_back(pArchetype);
				s_tmpArchetypeMatches.clear();
			}

			GAIA_NODISCARD QueryId id() const {
				return m_lookupCtx.queryId;
			}

			GAIA_NODISCARD const QueryCtx::Data& data(ComponentKind compKind) const {
				return m_lookupCtx.data[compKind];
			}

			GAIA_NODISCARD const QueryComponentIdArray& comp_ids(ComponentKind compKind) const {
				return m_lookupCtx.data[compKind].compIds;
			}

			GAIA_NODISCARD const QueryChangeArray& filters(ComponentKind compKind) const {
				return m_lookupCtx.data[compKind].withChanged;
			}

			GAIA_NODISCARD bool has_filters() const {
				return !m_lookupCtx.data[ComponentKind::CK_Generic].withChanged.empty() ||
							 !m_lookupCtx.data[ComponentKind::CK_Chunk].withChanged.empty();
			}

			template <typename... T>
			bool has() const {
				return (has_inter<T>(QueryListType::LT_Count) || ...);
			}

			template <typename... T>
			bool has_any() const {
				return (has_inter<T>(QueryListType::LT_Any) || ...);
			}

			template <typename... T>
			bool has_all() const {
				return (has_inter<T>(QueryListType::LT_All) && ...);
			}

			template <typename... T>
			bool has_none() const {
				return (!has_inter<T>(QueryListType::LT_None) && ...);
			}

			GAIA_NODISCARD ArchetypeList::iterator begin() {
				return m_archetypeCache.begin();
			}

			GAIA_NODISCARD ArchetypeList::iterator end() {
				return m_archetypeCache.end();
			}
		};
	} // namespace ecs
} // namespace gaia

namespace gaia {
	namespace ecs {
		class QueryCache {
			using QueryCacheLookupArray = cnt::darray<uint32_t>;

			cnt::map<QueryLookupHash, QueryCacheLookupArray> m_queryCache;
			cnt::darray<QueryInfo> m_queryArr;

		public:
			QueryCache() {
				m_queryArr.reserve(256);
			}

			~QueryCache() = default;

			QueryCache(QueryCache&&) = delete;
			QueryCache(const QueryCache&) = delete;
			QueryCache& operator=(QueryCache&&) = delete;
			QueryCache& operator=(const QueryCache&) = delete;

			//! Returns an already existing query info from the provided \param queryId.
			//! \warning It is expected that the query has already been registered. Undefined behavior otherwise.
			//! \param queryId Query used to search for query info
			//! \return Query info
			QueryInfo& get(QueryId queryId) {
				return m_queryArr[queryId];
			};

			//! Registers the provided query lookup context \param ctx. If it already exists it is returned.
			//! \return Query id
			uint32_t goc(QueryCtx&& ctx) {
				GAIA_ASSERT(ctx.hashLookup.hash != 0);

				// Check if the query info exists first
				auto ret = m_queryCache.try_emplace(ctx.hashLookup, QueryCacheLookupArray{});
				if (!ret.second) {
					const auto& queryIds = ret.first->second;

					// Record with the query info lookup hash exists but we need to check if the query itself is a part of it.
					if GAIA_LIKELY (ctx.queryId != QueryIdBad) {
						// Make sure the same hash gets us to the proper query
						for (const auto queryId: queryIds) {
							const auto& queryInfo = m_queryArr[queryId];
							if (queryInfo != ctx)
								continue;

							return queryId;
						}

						GAIA_ASSERT(false && "QueryInfo not found despite having its lookupHash and cacheId set!");
						return QueryIdBad;
					}
				}

				const auto queryId = (QueryId)m_queryArr.size();
				m_queryArr.push_back(QueryInfo::create(queryId, std::move(ctx)));
				ret.first->second.push_back(queryId);
				return queryId;
			};
		};
	} // namespace ecs
} // namespace gaia

namespace gaia {
	namespace ecs {
		namespace detail {
			template <bool Cached>
			struct QueryImplStorage {
				//! QueryImpl cache id
				QueryId m_queryId = QueryIdBad;
				//! QueryImpl cache (stable pointer to parent world's query cache)
				QueryCache* m_entityQueryCache{};
			};

			template <>
			struct QueryImplStorage<false> {
				QueryInfo m_queryInfo;
			};

			template <bool UseCaching = true>
			class QueryImpl final {
				static constexpr uint32_t ChunkBatchSize = 16;
				using CChunkSpan = std::span<const Chunk*>;
				using ChunkBatchedList = cnt::sarray_ext<Chunk*, ChunkBatchSize>;
				using CmdBufferCmdFunc = void (*)(SerializationBuffer& buffer, QueryCtx& ctx);

			public:
				//! QueryImpl constraints
				enum class Constraints : uint8_t { EnabledOnly, DisabledOnly, AcceptAll };

			private:
				//! Command buffer command type
				enum CommandBufferCmd : uint8_t { ADD_COMPONENT, ADD_FILTER };

				struct Command_AddComponent {
					static constexpr CommandBufferCmd Id = CommandBufferCmd::ADD_COMPONENT;

					ComponentId compId;
					ComponentKind compKind;
					QueryListType listType;
					bool isReadWrite;

					void exec(QueryCtx& ctx) const {
						auto& data = ctx.data[compKind];
						auto& compIds = data.compIds;
						auto& lastMatchedArchetypeIndex = data.lastMatchedArchetypeIndex;
						auto& rules = data.rules;

						// Unique component ids only
						GAIA_ASSERT(!core::has(compIds, compId));

#if GAIA_DEBUG
						// There's a limit to the amount of components which we can store
						if (compIds.size() >= MAX_COMPONENTS_IN_QUERY) {
							GAIA_ASSERT(false && "Trying to create an ECS query with too many components!");

							const auto& cc = ComponentCache::get();
							auto componentName = cc.comp_desc(compId).name;
							GAIA_LOG_E(
									"Trying to add ECS component '%.*s' to an already full ECS query!", (uint32_t)componentName.size(),
									componentName.data());

							return;
						}
#endif

						data.readWriteMask |= (uint8_t)isReadWrite << (uint8_t)compIds.size();
						compIds.push_back(compId);
						lastMatchedArchetypeIndex.push_back(0);
						rules.push_back(listType);

						if (listType == QueryListType::LT_All)
							++data.rulesAllCount;
					}
				};

				struct Command_Filter {
					static constexpr CommandBufferCmd Id = CommandBufferCmd::ADD_FILTER;

					ComponentId compId;
					ComponentKind compKind;

					void exec(QueryCtx& ctx) const {
						auto& data = ctx.data[compKind];
						auto& compIds = data.compIds;
						auto& withChanged = data.withChanged;
						const auto& rules = data.rules;

						GAIA_ASSERT(core::has(compIds, compId));
						GAIA_ASSERT(!core::has(withChanged, compId));

#if GAIA_DEBUG
						// There's a limit to the amount of components which we can store
						if (withChanged.size() >= MAX_COMPONENTS_IN_QUERY) {
							GAIA_ASSERT(false && "Trying to create an ECS filter query with too many components!");

							const auto& cc = ComponentCache::get();
							auto componentName = cc.comp_desc(compId).name;
							GAIA_LOG_E(
									"Trying to add ECS component %.*s to an already full filter query!", (uint32_t)componentName.size(),
									componentName.data());
							return;
						}
#endif

						const auto compIdx = core::get_index_unsafe(compIds, compId);

						// Component has to be present in anyList or allList.
						// NoneList makes no sense because we skip those in query processing anyway.
						if (rules[compIdx] != QueryListType::LT_None) {
							withChanged.push_back(compId);
							return;
						}

						GAIA_ASSERT(false && "SetChangeFilter trying to filter ECS component which is not a part of the query");
#if GAIA_DEBUG
						const auto& cc = ComponentCache::get();
						auto componentName = cc.comp_desc(compId).name;
						GAIA_LOG_E(
								"SetChangeFilter trying to filter ECS component %.*s but "
								"it's not a part of the query!",
								(uint32_t)componentName.size(), componentName.data());
#endif
					}
				};

				static constexpr CmdBufferCmdFunc CommandBufferRead[] = {
						// Add component
						[](SerializationBuffer& buffer, QueryCtx& ctx) {
							Command_AddComponent cmd;
							ser::load(buffer, cmd);
							cmd.exec(ctx);
						},
						// Add filter
						[](SerializationBuffer& buffer, QueryCtx& ctx) {
							Command_Filter cmd;
							ser::load(buffer, cmd);
							cmd.exec(ctx);
						}};

				//! Storage for data based on whether Caching is used or not
				QueryImplStorage<UseCaching> m_storage;
				//! Buffer with commands used to fetch the QueryInfo
				SerializationBuffer m_serBuffer;
				//! World version (stable pointer to parent world's world version)
				uint32_t* m_worldVersion{};
				//! List of achetypes (stable pointer to parent world's archetype array)
				const ArchetypeList* m_archetypes{};
				//! Map of component ids to archetypes (stable pointer to parent world's archetype component-to-archetype map)
				const ComponentToArchetypeMap* m_componentToArchetypeMap{};
				//! Execution mode
				QueryExecMode m_executionMode = QueryExecMode::Run;

				//--------------------------------------------------------------------------------
			public:
				QueryInfo& fetch_query_info() {
					if constexpr (UseCaching) {
						// Make sure the query was created by World.query()
						GAIA_ASSERT(m_storage.m_entityQueryCache != nullptr);

						// Lookup hash is present which means QueryInfo was already found
						if GAIA_LIKELY (m_storage.m_queryId != QueryIdBad) {
							auto& queryInfo = m_storage.m_entityQueryCache->get(m_storage.m_queryId);
							queryInfo.match(*m_componentToArchetypeMap, (uint32_t)m_archetypes->size());
							return queryInfo;
						}

						// No lookup hash is present which means QueryInfo needs to fetched or created
						QueryCtx ctx;
						commit(ctx);
						m_storage.m_queryId = m_storage.m_entityQueryCache->goc(std::move(ctx));
						auto& queryInfo = m_storage.m_entityQueryCache->get(m_storage.m_queryId);
						queryInfo.match(*m_componentToArchetypeMap, (uint32_t)m_archetypes->size());
						return queryInfo;
					} else {
						if GAIA_UNLIKELY (m_storage.m_queryInfo.id() == QueryIdBad) {
							QueryCtx ctx;
							commit(ctx);
							m_storage.m_queryInfo = QueryInfo::create(QueryId{}, std::move(ctx));
						}
						m_storage.m_queryInfo.match(*m_componentToArchetypeMap, (uint32_t)m_archetypes->size());
						return m_storage.m_queryInfo;
					}
				}

				//--------------------------------------------------------------------------------
			private:
				template <typename T>
				void add_inter(QueryListType listType) {
					using U = typename component_kind_t<T>::Kind;
					using UOriginal = typename component_kind_t<T>::KindOriginal;
					using UOriginalPR = std::remove_reference_t<std::remove_pointer_t<UOriginal>>;

					const auto compId = comp_id<T>();
					constexpr auto compKind = component_kind_v<T>;
					constexpr bool isReadWrite =
							std::is_same_v<U, UOriginal> || (!std::is_const_v<UOriginalPR> && !std::is_empty_v<U>);

					// Make sure the component is always registered
					auto& cc = ComponentCache::get();
					(void)cc.goc_comp_info<T>();

					Command_AddComponent cmd{compId, compKind, listType, isReadWrite};
					ser::save(m_serBuffer, Command_AddComponent::Id);
					ser::save(m_serBuffer, cmd);
				}

				template <typename T>
				void WithChanged_inter() {
					const auto compId = comp_id<T>();
					constexpr auto compKind = component_kind_v<T>;

					Command_Filter cmd{compId, compKind};
					ser::save(m_serBuffer, Command_Filter::Id);
					ser::save(m_serBuffer, cmd);
				}

				//--------------------------------------------------------------------------------

				void commit(QueryCtx& ctx) {
#if GAIA_ASSERT_ENABLED
					if constexpr (UseCaching) {
						GAIA_ASSERT(m_storage.m_queryId == QueryIdBad);
					} else {
						GAIA_ASSERT(m_storage.m_queryInfo.id() == QueryIdBad);
					}
#endif

					// Read data from buffer and execute the command stored in it
					m_serBuffer.seek(0);
					while (m_serBuffer.tell() < m_serBuffer.bytes()) {
						CommandBufferCmd id{};
						ser::load(m_serBuffer, id);
						CommandBufferRead[id](m_serBuffer, ctx);
					}

					// Calculate the lookup hash from the provided context
					calc_lookup_hash(ctx);

					// We can free all temporary data now
					m_serBuffer.reset();
				}

				//--------------------------------------------------------------------------------

				//! Unpacks the parameter list \param types into query \param query and performs All for each of them
				template <typename... T>
				void unpack_args_into_query_All(QueryImpl& query, [[maybe_unused]] core::func_type_list<T...> types) const {
					static_assert(sizeof...(T) > 0, "Inputs-less functors can not be unpacked to query");
					query.all<T...>();
				}

				//! Unpacks the parameter list \param types into query \param query and performs has_all for each of them
				template <typename... T>
				GAIA_NODISCARD bool unpack_args_into_query_has_all(
						const QueryInfo& queryInfo, [[maybe_unused]] core::func_type_list<T...> types) const {
					if constexpr (sizeof...(T) > 0)
						return queryInfo.has_all<T...>();
					else
						return true;
				}

				//--------------------------------------------------------------------------------

				GAIA_NODISCARD static bool match_filters(const Chunk& chunk, const QueryInfo& queryInfo) {
					GAIA_ASSERT(chunk.has_entities() && "match_filters called on an empty chunk");

					const auto queryVersion = queryInfo.world_version();

					// See if any generic component has changed
					{
						const auto& filtered = queryInfo.filters(ComponentKind::CK_Generic);
						for (const auto compId: filtered) {
							const auto compIdx = chunk.comp_idx(ComponentKind::CK_Generic, compId);
							if (chunk.changed(ComponentKind::CK_Generic, queryVersion, compIdx))
								return true;
						}
					}

					// See if any chunk component has changed
					{
						const auto& filtered = queryInfo.filters(ComponentKind::CK_Chunk);
						for (const auto compId: filtered) {
							const uint32_t compIdx = chunk.comp_idx(ComponentKind::CK_Chunk, compId);
							if (chunk.changed(ComponentKind::CK_Chunk, queryVersion, compIdx))
								return true;
						}
					}

					// Skip unchanged chunks.
					return false;
				}

				//! Execute functors in batches
				template <typename Func>
				static void run_func_batched(Func func, ChunkBatchedList& chunks) {
					GAIA_ASSERT(!chunks.empty());

					// This is what the function is doing:
					// for (auto *pChunk: chunks)
					//	func(*pChunk);
					// chunks.clear();

					GAIA_PROF_SCOPE(run_func_batched);

					// We only have one chunk to process
					if GAIA_UNLIKELY (chunks.size() == 1) {
						chunks[0]->set_structural_changes(true);
						func(*chunks[0]);
						chunks[0]->set_structural_changes(false);
						chunks.clear();
						return;
					}

					// We have many chunks to process.
					// Chunks might be located at different memory locations. Not even in the same memory page.
					// Therefore, to make it easier for the CPU we give it a hint that we want to prefetch data
					// for the next chunk explictely so we do not end up stalling later.
					// Note, this is a micro optimization and on average it bring no performance benefit. It only
					// helps with edge cases.
					// Let us be conservative for now and go with T2. That means we will try to keep our data at
					// least in L3 cache or higher.
					gaia::prefetch(&chunks[1], PrefetchHint::PREFETCH_HINT_T2);
					chunks[0]->set_structural_changes(true);
					func(*chunks[0]);
					chunks[0]->set_structural_changes(false);

					uint32_t chunkIdx = 1;
					for (; chunkIdx < chunks.size() - 1; ++chunkIdx) {
						gaia::prefetch(&chunks[chunkIdx + 1], PrefetchHint::PREFETCH_HINT_T2);
						chunks[chunkIdx]->set_structural_changes(true);
						func(*chunks[chunkIdx]);
						chunks[chunkIdx]->set_structural_changes(false);
					}

					chunks[chunkIdx]->set_structural_changes(true);
					func(*chunks[chunkIdx]);
					chunks[chunkIdx]->set_structural_changes(false);

					chunks.clear();
				}

				template <bool HasFilters, typename Func>
				void run_query_unconstrained(
						Func func, ChunkBatchedList& chunkBatch, const cnt::darray<Chunk*>& chunks, const QueryInfo& queryInfo) {
					uint32_t chunkOffset = 0;
					uint32_t itemsLeft = chunks.size();
					while (itemsLeft > 0) {
						const auto maxBatchSize = chunkBatch.max_size() - chunkBatch.size();
						const auto batchSize = itemsLeft > maxBatchSize ? maxBatchSize : itemsLeft;

						CChunkSpan chunkSpan((const Chunk**)&chunks[chunkOffset], batchSize);
						for (const auto* pChunk: chunkSpan) {
							if (!pChunk->has_entities())
								continue;
							if constexpr (HasFilters) {
								if (!match_filters(*pChunk, queryInfo))
									continue;
							}

							chunkBatch.push_back(const_cast<Chunk*>(pChunk));
						}

						if GAIA_UNLIKELY (chunkBatch.size() == chunkBatch.max_size())
							run_func_batched(func, chunkBatch);

						itemsLeft -= batchSize;
						chunkOffset += batchSize;
					}
				}

				template <bool HasFilters, typename Func>
				void run_query_constrained(
						Func func, ChunkBatchedList& chunkBatch, const cnt::darray<Chunk*>& chunks, const QueryInfo& queryInfo,
						bool enabledOnly) {
					uint32_t chunkOffset = 0;
					uint32_t itemsLeft = chunks.size();
					while (itemsLeft > 0) {
						const auto maxBatchSize = chunkBatch.max_size() - chunkBatch.size();
						const auto batchSize = itemsLeft > maxBatchSize ? maxBatchSize : itemsLeft;

						CChunkSpan chunkSpan((const Chunk**)&chunks[chunkOffset], batchSize);
						for (const auto* pChunk: chunkSpan) {
							if (!pChunk->has_entities())
								continue;

							if (enabledOnly && !pChunk->has_enabled_entities())
								continue;
							if (!enabledOnly && !pChunk->has_disabled_entities())
								continue;

							if constexpr (HasFilters) {
								if (!match_filters(*pChunk, queryInfo))
									continue;
							}

							chunkBatch.push_back(const_cast<Chunk*>(pChunk));
						}

						if GAIA_UNLIKELY (chunkBatch.size() == chunkBatch.max_size())
							run_func_batched(func, chunkBatch);

						itemsLeft -= batchSize;
						chunkOffset += batchSize;
					}
				}

				template <typename Func>
				void run_query_on_chunks(QueryInfo& queryInfo, Constraints constraints, Func func) {
					// Update the world version
					update_version(*m_worldVersion);

					ChunkBatchedList chunkBatch;

					const bool hasFilters = queryInfo.has_filters();
					if (hasFilters) {
						// Evaluation defaults to EnabledOnly changes. AcceptAll is something that has to be asked for explicitely
						if GAIA_UNLIKELY (constraints == Constraints::AcceptAll) {
							for (auto* pArchetype: queryInfo)
								run_query_unconstrained<true>(func, chunkBatch, pArchetype->chunks(), queryInfo);
						} else {
							const bool enabledOnly = constraints == Constraints::EnabledOnly;
							for (auto* pArchetype: queryInfo)
								run_query_constrained<true>(func, chunkBatch, pArchetype->chunks(), queryInfo, enabledOnly);
						}
					} else {
						if GAIA_UNLIKELY (constraints == Constraints::AcceptAll) {
							for (auto* pArchetype: queryInfo)
								run_query_unconstrained<false>(func, chunkBatch, pArchetype->chunks(), queryInfo);
						} else {
							const bool enabledOnly = constraints == Constraints::EnabledOnly;
							for (auto* pArchetype: queryInfo)
								run_query_constrained<false>(func, chunkBatch, pArchetype->chunks(), queryInfo, enabledOnly);
						}
					}

					if (!chunkBatch.empty())
						run_func_batched(func, chunkBatch);

					// Update the query version with the current world's version
					queryInfo.set_world_version(*m_worldVersion);
				}

				template <typename Func>
				void each_inter(QueryInfo& queryInfo, Func func) {
					using InputArgs = decltype(core::func_args(&Func::operator()));

#if GAIA_DEBUG
					// Make sure we only use components specified in the query
					GAIA_ASSERT(unpack_args_into_query_has_all(queryInfo, InputArgs{}));
#endif

					run_query_on_chunks(queryInfo, Constraints::EnabledOnly, [&](Chunk& chunk) {
						chunk.each(InputArgs{}, func);
					});
				}

				void invalidate() {
					if constexpr (UseCaching)
						m_storage.m_queryId = QueryIdBad;
				}

			public:
				QueryImpl() = default;

				template <bool FuncEnabled = UseCaching>
				QueryImpl(
						QueryCache& queryCache, uint32_t& worldVersion, const cnt::darray<Archetype*>& archetypes,
						const ComponentToArchetypeMap& componentToArchetypeMap):
						m_worldVersion(&worldVersion),
						m_archetypes(&archetypes), m_componentToArchetypeMap(&componentToArchetypeMap) {
					m_storage.m_entityQueryCache = &queryCache;
				}

				template <bool FuncEnabled = !UseCaching>
				QueryImpl(
						uint32_t& worldVersion, const cnt::darray<Archetype*>& archetypes,
						const ComponentToArchetypeMap& componentToArchetypeMap):
						m_worldVersion(&worldVersion),
						m_archetypes(&archetypes), m_componentToArchetypeMap(&componentToArchetypeMap) {}

				GAIA_NODISCARD uint32_t id() const {
					static_assert(UseCaching, "id() can be used only with cached queries");
					return m_storage.m_queryId;
				}

				template <typename... T>
				QueryImpl& all() {
					// Adding new rules invalidates the query
					invalidate();
					// Add commands to the command buffer
					(add_inter<T>(QueryListType::LT_All), ...);
					return *this;
				}

				template <typename... T>
				QueryImpl& any() {
					// Adding new rules invalidates the query
					invalidate();
					// Add commands to the command buffer
					(add_inter<T>(QueryListType::LT_Any), ...);
					return *this;
				}

				template <typename... T>
				QueryImpl& none() {
					// Adding new rules invalidates the query
					invalidate();
					// Add commands to the command buffer
					(add_inter<T>(QueryListType::LT_None), ...);
					return *this;
				}

				template <typename... T>
				QueryImpl& changed() {
					// Adding new rules invalidates the query
					invalidate();
					// Add commands to the command buffer
					(WithChanged_inter<T>(), ...);
					return *this;
				}

				QueryImpl& sched() {
					m_executionMode = QueryExecMode::Single;
					return *this;
				}

				QueryImpl& sched_par() {
					m_executionMode = QueryExecMode::Parallel;
					return *this;
				}

				template <typename Func>
				void each(Func func) {
					auto& queryInfo = fetch_query_info();

					if constexpr (std::is_invocable<Func, IteratorAll>::value)
						run_query_on_chunks(queryInfo, Constraints::AcceptAll, [&](Chunk& chunk) {
							func(IteratorAll(chunk));
						});
					else if constexpr (std::is_invocable<Func, Iterator>::value)
						run_query_on_chunks(queryInfo, Constraints::EnabledOnly, [&](Chunk& chunk) {
							func(Iterator(chunk));
						});
					else if constexpr (std::is_invocable<Func, IteratorDisabled>::value)
						run_query_on_chunks(queryInfo, Constraints::DisabledOnly, [&](Chunk& chunk) {
							func(IteratorDisabled(chunk));
						});
					else
						each_inter(queryInfo, func);
				}

				template <typename Func, bool FuncEnabled = UseCaching, typename std::enable_if<FuncEnabled>::type* = nullptr>
				void each(QueryId queryId, Func func) {
					// Make sure the query was created by World.query()
					GAIA_ASSERT(m_storage.m_entityQueryCache != nullptr);
					GAIA_ASSERT(queryId != QueryIdBad);

					auto& queryInfo = m_storage.m_entityQueryCache->get(queryId);
					each_inter(queryInfo, func);
				}

				template <bool UseFilters, Constraints c, typename ChunksContainer>
				GAIA_NODISCARD bool has_entities_inter(QueryInfo& queryInfo, const ChunksContainer& chunks) {
					return core::has_if(chunks, [&](Chunk* pChunk) {
						if constexpr (UseFilters) {
							if constexpr (c == Constraints::AcceptAll)
								return pChunk->has_entities() && match_filters(*pChunk, queryInfo);
							else if constexpr (c == Constraints::EnabledOnly)
								return pChunk->size_disabled() != pChunk->size() && match_filters(*pChunk, queryInfo);
							else // if constexpr (c == Constraints::DisabledOnly)
								return pChunk->size_disabled() > 0 && match_filters(*pChunk, queryInfo);
						} else {
							if constexpr (c == Constraints::AcceptAll)
								return pChunk->has_entities();
							else if constexpr (c == Constraints::EnabledOnly)
								return pChunk->size_disabled() != pChunk->size();
							else // if constexpr (c == Constraints::DisabledOnly)
								return pChunk->size_disabled() > 0;
						}
					});
				}

				template <bool UseFilters, Constraints c, typename ChunksContainer>
				GAIA_NODISCARD uint32_t calc_entity_cnt_inter(QueryInfo& queryInfo, const ChunksContainer& chunks) {
					uint32_t cnt = 0;

					for (auto* pChunk: chunks) {
						if (!pChunk->has_entities())
							continue;

						// Filters
						if constexpr (UseFilters) {
							if (!match_filters(*pChunk, queryInfo))
								continue;
						}

						// Entity count
						if constexpr (c == Constraints::EnabledOnly)
							cnt += pChunk->size_enabled();
						else if constexpr (c == Constraints::DisabledOnly)
							cnt += pChunk->size_disabled();
						else
							cnt += pChunk->size();
					}

					return cnt;
				}

				template <bool UseFilters, Constraints c, typename ChunksContainerIn, typename ChunksContainerOut>
				void arr_inter(QueryInfo& queryInfo, const ChunksContainerIn& chunks, ChunksContainerOut& outArray) {
					using ContainerItemType = typename ChunksContainerOut::value_type;

					for (auto* pChunk: chunks) {
						if (!pChunk->has_entities())
							continue;

						if constexpr (c == Constraints::EnabledOnly) {
							if (pChunk->has_disabled_entities())
								continue;
						} else if constexpr (c == Constraints::DisabledOnly) {
							if (!pChunk->has_disabled_entities())
								continue;
						}

						// Filters
						if constexpr (UseFilters) {
							if (!match_filters(*pChunk, queryInfo))
								continue;
						}

						const auto componentView = pChunk->template view<ContainerItemType>();
						for (uint32_t i = 0; i < pChunk->size(); ++i)
							outArray.push_back(componentView[i]);
					}
				}

				/*!
					Returns true or false depending on whether there are entities matching the query.
					\warning Only use if you only care if there are any entities matching the query.
									 The result is not cached and repeated calls to the function might be slow.
									 If you already called arr(), checking if it is empty is preferred.
					\return True if there are any entites matchine the query. False otherwise.
					*/
				bool has_entities(Constraints constraints = Constraints::EnabledOnly) {
					auto& queryInfo = fetch_query_info();
					const bool hasFilters = queryInfo.has_filters();

					if (hasFilters) {
						switch (constraints) {
							case Constraints::EnabledOnly: {
								for (auto* pArchetype: queryInfo)
									if (has_entities_inter<true, Constraints::EnabledOnly>(queryInfo, pArchetype->chunks()))
										return true;
							} break;
							case Constraints::DisabledOnly: {
								for (auto* pArchetype: queryInfo)
									if (has_entities_inter<true, Constraints::DisabledOnly>(queryInfo, pArchetype->chunks()))
										return true;
							} break;
							case Constraints::AcceptAll: {
								for (auto* pArchetype: queryInfo)
									if (has_entities_inter<true, Constraints::AcceptAll>(queryInfo, pArchetype->chunks()))
										return true;
							} break;
						}
					} else {
						switch (constraints) {
							case Constraints::EnabledOnly: {
								for (auto* pArchetype: queryInfo)
									if (has_entities_inter<false, Constraints::EnabledOnly>(queryInfo, pArchetype->chunks()))
										return true;
							} break;
							case Constraints::DisabledOnly: {
								for (auto* pArchetype: queryInfo)
									if (has_entities_inter<false, Constraints::DisabledOnly>(queryInfo, pArchetype->chunks()))
										return true;
							} break;
							case Constraints::AcceptAll: {
								for (auto* pArchetype: queryInfo)
									if (has_entities_inter<false, Constraints::AcceptAll>(queryInfo, pArchetype->chunks()))
										return true;
							} break;
						}
					}

					return false;
				}

				/*!
				Returns the number of entities matching the query
				\warning Only use if you only care about the number of entities matching the query.
								 The result is not cached and repeated calls to the function might be slow.
								 If you already called arr(), use the size provided by the array.
				\return The number of matching entities
				*/
				uint32_t calc_entity_cnt(Constraints constraints = Constraints::EnabledOnly) {
					auto& queryInfo = fetch_query_info();
					uint32_t entCnt = 0;

					const bool hasFilters = queryInfo.has_filters();
					if (hasFilters) {
						switch (constraints) {
							case Constraints::EnabledOnly: {
								for (auto* pArchetype: queryInfo)
									entCnt += calc_entity_cnt_inter<true, Constraints::EnabledOnly>(queryInfo, pArchetype->chunks());
							} break;
							case Constraints::DisabledOnly: {
								for (auto* pArchetype: queryInfo)
									entCnt += calc_entity_cnt_inter<true, Constraints::DisabledOnly>(queryInfo, pArchetype->chunks());
							} break;
							case Constraints::AcceptAll: {
								for (auto* pArchetype: queryInfo)
									entCnt += calc_entity_cnt_inter<true, Constraints::AcceptAll>(queryInfo, pArchetype->chunks());
							} break;
						}
					} else {
						switch (constraints) {
							case Constraints::EnabledOnly: {
								for (auto* pArchetype: queryInfo)
									entCnt += calc_entity_cnt_inter<false, Constraints::EnabledOnly>(queryInfo, pArchetype->chunks());
							} break;
							case Constraints::DisabledOnly: {
								for (auto* pArchetype: queryInfo)
									entCnt += calc_entity_cnt_inter<false, Constraints::DisabledOnly>(queryInfo, pArchetype->chunks());
							} break;
							case Constraints::AcceptAll: {
								for (auto* pArchetype: queryInfo)
									entCnt += calc_entity_cnt_inter<false, Constraints::AcceptAll>(queryInfo, pArchetype->chunks());
							} break;
						}
					}

					return entCnt;
				}

				/*!
				Appends all components or entities matching the query to the array
				\tparam outArray Container storing entities or components
				\param constraints QueryImpl constraints
				\return Array with entities or components
				*/
				template <typename Container>
				void arr(Container& outArray, Constraints constraints = Constraints::EnabledOnly) {
					const auto entCnt = calc_entity_cnt();
					if (entCnt == 0)
						return;

					outArray.reserve(entCnt);
					auto& queryInfo = fetch_query_info();

					const bool hasFilters = queryInfo.has_filters();
					if (hasFilters) {
						switch (constraints) {
							case Constraints::EnabledOnly:
								for (auto* pArchetype: queryInfo)
									arr_inter<true, Constraints::EnabledOnly>(queryInfo, pArchetype->chunks(), outArray);
								break;
							case Constraints::DisabledOnly:
								for (auto* pArchetype: queryInfo)
									arr_inter<true, Constraints::DisabledOnly>(queryInfo, pArchetype->chunks(), outArray);
								break;
							case Constraints::AcceptAll:
								for (auto* pArchetype: queryInfo)
									arr_inter<true, Constraints::AcceptAll>(queryInfo, pArchetype->chunks(), outArray);
								break;
						}
					} else {
						switch (constraints) {
							case Constraints::EnabledOnly:
								for (auto* pArchetype: queryInfo)
									arr_inter<false, Constraints::EnabledOnly>(queryInfo, pArchetype->chunks(), outArray);
								break;
							case Constraints::DisabledOnly:
								for (auto* pArchetype: queryInfo)
									arr_inter<false, Constraints::DisabledOnly>(queryInfo, pArchetype->chunks(), outArray);
								break;
							case Constraints::AcceptAll:
								for (auto* pArchetype: queryInfo)
									arr_inter<false, Constraints::AcceptAll>(queryInfo, pArchetype->chunks(), outArray);
								break;
						}
					}
				}
			};
		} // namespace detail

		using Query = typename detail::QueryImpl<true>;
		using QueryUncached = typename detail::QueryImpl<false>;
	} // namespace ecs
} // namespace gaia

namespace gaia {
	namespace ecs {
		class GAIA_API World final {
			friend class ECSSystem;
			friend class ECSSystemManager;
			friend class CommandBuffer;
			friend void* AllocateChunkMemory(World& world);
			friend void ReleaseChunkMemory(World& world, void* mem);

			//! Cache of queries
			QueryCache m_queryCache;
			//! Cache of query ids to speed up each
			cnt::map<ComponentLookupHash, QueryId> m_uniqueFuncQueryPairs;
			//! Map of compId -> archetype matches.
			ComponentToArchetypeMap m_componentToArchetypeMap;

			//! Map of archetypes mapping to the same hash - used for lookups
			cnt::map<ArchetypeLookupKey, Archetype*> m_archetypeMap;
			//! List of archetypes - used for iteration
			ArchetypeList m_archetypes;

			//! Implicit list of entities. Used for look-ups only when searching for
			//! entities in chunks + data validation
			cnt::ilist<EntityContainer, Entity> m_entities;

			//! List of chunks to delete
			cnt::darray<Chunk*> m_chunksToRemove;
			//! ID of the last defragmented archetype
			uint32_t m_defragLastArchetypeID = 0;

			//! With every structural change world version changes
			uint32_t m_worldVersion = 0;

		private:
			//! Remove an entity from chunk.
			//! \param pChunk Chunk we remove the entity from
			//! \param entityChunkIndex Index of entity within its chunk
			//! \tparam IsEntityDeleteWanted True if entity is to be deleted. False otherwise.
			template <bool IsEntityDeleteWanted>
			void remove_entity(Chunk* pChunk, uint32_t entityChunkIndex) {
				GAIA_PROF_SCOPE(remove_entity);

				const auto entity = pChunk->get_entity(entityChunkIndex);
				pChunk->remove_entity(entityChunkIndex, {m_entities.data(), m_entities.size()}, m_chunksToRemove);

				pChunk->update_versions();
				if constexpr (IsEntityDeleteWanted)
					del_entity(entity);
			}

			//! defragments chunks.
			//! \param maxEntites Maximum number of entities moved per call
			void defrag_chunks(uint32_t maxEntities) {
				const auto maxIters = (uint32_t)m_archetypes.size();
				for (uint32_t i = 0; i < maxIters; ++i) {
					m_defragLastArchetypeID = (m_defragLastArchetypeID + i) % maxIters;

					auto* pArchetype = m_archetypes[m_defragLastArchetypeID];
					pArchetype->defrag(maxEntities, m_chunksToRemove, {m_entities.data(), m_entities.size()});
					if (maxEntities == 0)
						return;
				}
			}

			//! Searches for archetype with a given set of components
			//! \param lookupHash Archetype lookup hash
			//! \param compIdsGeneric Span of generic component ids
			//! \param compIdsChunk Span of chunk component ids
			//! \return Pointer to archetype or nullptr.
			GAIA_NODISCARD Archetype*
			find_archetype(Archetype::LookupHash lookupHash, ComponentIdSpan compIdsGeneric, ComponentIdSpan compIdsChunk) {
				auto tmpArchetype = ArchetypeLookupChecker(compIdsGeneric, compIdsChunk);
				ArchetypeLookupKey key(lookupHash, &tmpArchetype);

				// Search for the archetype in the map
				const auto it = m_archetypeMap.find(key);
				if (it == m_archetypeMap.end())
					return nullptr;

				auto* pArchetype = it->second;
				return pArchetype;
			}

			//! Creates a new archetype from a given set of components
			//! \param compIdsGeneric Span of generic component infos
			//! \param compIdsChunk Span of chunk component infos
			//! \return Pointer to the new archetype.
			GAIA_NODISCARD Archetype* create_archetype(ComponentIdSpan compIdsGeneric, ComponentIdSpan compIdsChunk) {
				auto* pArchetype =
						Archetype::create((ArchetypeId)m_archetypes.size(), m_worldVersion, compIdsGeneric, compIdsChunk);

				auto registerComponentToArchetypePair = [&](ComponentId compId) {
					const auto it = m_componentToArchetypeMap.find(compId);
					if (it == m_componentToArchetypeMap.end())
						m_componentToArchetypeMap.try_emplace(compId, ArchetypeList{pArchetype});
					else if (!core::has(it->second, pArchetype))
						it->second.push_back(pArchetype);
				};

				for (const auto compId: compIdsGeneric)
					registerComponentToArchetypePair(compId);
				for (const auto compId: compIdsChunk)
					registerComponentToArchetypePair(compId);

				return pArchetype;
			}

			//! Registers the archetype in the world.
			//! \param pArchetype Archetype to register.
			void reg_archetype(Archetype* pArchetype) {
				// Make sure hashes were set already
				GAIA_ASSERT(
						(m_archetypes.empty() || pArchetype == m_archetypes[0]) ||
						(pArchetype->generic_hash().hash != 0 || pArchetype->chunk_hash().hash != 0));
				GAIA_ASSERT((m_archetypes.empty() || pArchetype == m_archetypes[0]) || pArchetype->lookup_hash().hash != 0);

				// Make sure the archetype is not registered yet
				GAIA_ASSERT(!core::has(m_archetypes, pArchetype));

				// Register the archetype
				m_archetypes.push_back(pArchetype);
				m_archetypeMap.emplace(ArchetypeLookupKey(pArchetype->lookup_hash(), pArchetype), pArchetype);
			}

#if GAIA_DEBUG
			static void
			verify_add(Archetype& archetype, Entity entity, ComponentKind compKind, const ComponentInfo& infoToAdd) {
				const auto& compIds = archetype.comp_ids(compKind);
				const auto& cc = ComponentCache::get();

				// Make sure not to add too many infos
				if GAIA_UNLIKELY (compIds.size() + 1 >= Chunk::MAX_COMPONENTS) {
					GAIA_ASSERT(false && "Trying to add too many components to entity!");
					GAIA_LOG_W(
							"Trying to add a component to entity [%u.%u] but there's no space left!", entity.id(), entity.gen());
					GAIA_LOG_W("Already present:");
					const uint32_t oldInfosCount = compIds.size();
					for (uint32_t i = 0; i < oldInfosCount; ++i) {
						const auto& info = cc.comp_desc(compIds[i]);
						GAIA_LOG_W("> [%u] %.*s", (uint32_t)i, (uint32_t)info.name.size(), info.name.data());
					}
					GAIA_LOG_W("Trying to add:");
					{
						const auto& info = cc.comp_desc(infoToAdd.compId);
						GAIA_LOG_W("> %.*s", (uint32_t)info.name.size(), info.name.data());
					}
				}

				// Don't add the same component twice
				for (uint32_t i = 0; i < compIds.size(); ++i) {
					const auto& info = cc.comp_desc(compIds[i]);
					if (info.compId == infoToAdd.compId) {
						GAIA_ASSERT(false && "Trying to add a duplicate component");

						GAIA_LOG_W(
								"Trying to add a duplicate of component %s to entity [%u.%u]", ComponentKindString[compKind],
								entity.id(), entity.gen());
						GAIA_LOG_W("> %.*s", (uint32_t)info.name.size(), info.name.data());
					}
				}
			}

			static void
			verify_del(Archetype& archetype, Entity entity, ComponentKind compKind, const ComponentInfo& infoToRemove) {
				const auto& compIds = archetype.comp_ids(compKind);
				if GAIA_UNLIKELY (!core::has(compIds, infoToRemove.compId)) {
					GAIA_ASSERT(false && "Trying to remove a component which wasn't added");
					GAIA_LOG_W(
							"Trying to remove a component from entity [%u.%u] but it was never added", entity.id(), entity.gen());
					GAIA_LOG_W("Currently present:");

					const auto& cc = ComponentCache::get();

					for (uint32_t k = 0; k < compIds.size(); k++) {
						const auto& info = cc.comp_desc(compIds[k]);
						GAIA_LOG_W("> [%u] %.*s", (uint32_t)k, (uint32_t)info.name.size(), info.name.data());
					}

					{
						GAIA_LOG_W("Trying to remove:");
						const auto& info = cc.comp_desc(infoToRemove.compId);
						GAIA_LOG_W("> %.*s", (uint32_t)info.name.size(), info.name.data());
					}
				}
			}
#endif

			//! Searches for an archetype which is formed by adding \param compKind to \param pArchetypeLeft.
			//! If no such archetype is found a new one is created.
			//! \param pArchetypeLeft Archetype we originate from.
			//! \param compKind Component infos.
			//! \param infoToAdd Component we want to add.
			//! \return Pointer to archetype.
			GAIA_NODISCARD Archetype*
			foc_archetype_add_comp(Archetype* pArchetypeLeft, ComponentKind compKind, const ComponentInfo& infoToAdd) {
				// We don't want to store edges for the root archetype because the more components there are the longer
				// it would take to find anything. Therefore, for the root archetype we always make a lookup.
				// Compared to an ordinary lookup this path is stripped as much as possible.
				if (pArchetypeLeft == m_archetypes[0]) {
					Archetype* pArchetypeRight = nullptr;

					if (compKind == ComponentKind::CK_Generic) {
						const auto genericHash = infoToAdd.lookupHash;
						const auto lookupHash = Archetype::calc_lookup_hash(genericHash, {0});
						pArchetypeRight = find_archetype(lookupHash, ComponentIdSpan(&infoToAdd.compId, 1), {});
						if (pArchetypeRight == nullptr) {
							pArchetypeRight = create_archetype(ComponentIdSpan(&infoToAdd.compId, 1), {});
							pArchetypeRight->set_hashes({genericHash}, {0}, lookupHash);
							pArchetypeRight->build_graph_edges_left(pArchetypeLeft, compKind, infoToAdd.compId);
							reg_archetype(pArchetypeRight);
						}
					} else {
						const auto chunkHash = infoToAdd.lookupHash;
						const auto lookupHash = Archetype::calc_lookup_hash({0}, chunkHash);
						pArchetypeRight = find_archetype(lookupHash, {}, ComponentIdSpan(&infoToAdd.compId, 1));
						if (pArchetypeRight == nullptr) {
							pArchetypeRight = create_archetype({}, ComponentIdSpan(&infoToAdd.compId, 1));
							pArchetypeRight->set_hashes({0}, {chunkHash}, lookupHash);
							pArchetypeRight->build_graph_edges_left(pArchetypeLeft, compKind, infoToAdd.compId);
							reg_archetype(pArchetypeRight);
						}
					}

					return pArchetypeRight;
				}

				// Check if the component is found when following the "add" edges
				{
					const auto archetypeId = pArchetypeLeft->find_edge_right(compKind, infoToAdd.compId);
					if (archetypeId != ArchetypeIdBad)
						return m_archetypes[archetypeId];
				}

				const uint32_t a = compKind;
				const uint32_t b = (compKind + 1) & 1;
				const cnt::sarray_ext<uint32_t, Chunk::MAX_COMPONENTS>* infos[2];

				cnt::sarray_ext<uint32_t, Chunk::MAX_COMPONENTS> infosNew;
				infos[a] = &infosNew;
				infos[b] = &pArchetypeLeft->comp_ids((ComponentKind)b);

				// Prepare a joint array of component infos of old + the newly added component
				{
					const auto& compIds = pArchetypeLeft->comp_ids((ComponentKind)a);
					const auto componentInfosSize = compIds.size();
					infosNew.resize(componentInfosSize + 1);

					for (uint32_t j = 0; j < componentInfosSize; ++j)
						infosNew[j] = compIds[j];
					infosNew[componentInfosSize] = infoToAdd.compId;
				}

				// Make sure to sort the component infos so we receive the same hash no matter the order in which components
				// are provided Bubble sort is okay. We're dealing with at most Chunk::MAX_COMPONENTS items.
				sort(infosNew, SortComponentCond{});

				// Once sorted we can calculate the hashes
				const Archetype::GenericComponentHash genericHash = {
						calc_lookup_hash({infos[0]->data(), infos[0]->size()}).hash};
				const Archetype::ChunkComponentHash chunkHash = {calc_lookup_hash({infos[1]->data(), infos[1]->size()}).hash};
				const auto lookupHash = Archetype::calc_lookup_hash(genericHash, chunkHash);

				auto* pArchetypeRight =
						find_archetype(lookupHash, {infos[0]->data(), infos[0]->size()}, {infos[1]->data(), infos[1]->size()});
				if (pArchetypeRight == nullptr) {
					pArchetypeRight =
							create_archetype({infos[0]->data(), infos[0]->size()}, {infos[1]->data(), infos[1]->size()});
					pArchetypeRight->set_hashes(genericHash, chunkHash, lookupHash);
					pArchetypeLeft->build_graph_edges(pArchetypeRight, compKind, infoToAdd.compId);
					reg_archetype(pArchetypeRight);
				}

				return pArchetypeRight;
			}

			//! Searches for an archetype which is formed by removing \param compKind from \param pArchetypeRight.
			//! If no such archetype is found a new one is created.
			//! \param pArchetypeRight Archetype we originate from.
			//! \param compKind Component infos.
			//! \param infoToRemove Component we want to remove.
			//! \return Pointer to archetype.
			GAIA_NODISCARD Archetype*
			foc_archetype_remove_comp(Archetype* pArchetypeRight, ComponentKind compKind, const ComponentInfo& infoToRemove) {
				// Check if the component is found when following the "del" edges
				{
					const auto archetypeId = pArchetypeRight->find_edge_left(compKind, infoToRemove.compId);
					if (archetypeId != ArchetypeIdBad)
						return m_archetypes[archetypeId];
				}

				const uint32_t a = compKind;
				const uint32_t b = (compKind + 1) & 1;
				const cnt::sarray_ext<uint32_t, Chunk::MAX_COMPONENTS>* infos[2];

				cnt::sarray_ext<uint32_t, Chunk::MAX_COMPONENTS> infosNew;
				infos[a] = &infosNew;
				infos[b] = &pArchetypeRight->comp_ids((ComponentKind)b);

				// Find the intersection
				for (const auto compId: pArchetypeRight->comp_ids((ComponentKind)a)) {
					if (compId == infoToRemove.compId)
						continue;

					infosNew.push_back(compId);
				}

				// Return if there's no change
				if (infosNew.size() == pArchetypeRight->comp_ids((ComponentKind)a).size())
					return nullptr;

				// Calculate the hashes
				const Archetype::GenericComponentHash genericHash = {
						calc_lookup_hash({infos[0]->data(), infos[0]->size()}).hash};
				const Archetype::ChunkComponentHash chunkHash = {calc_lookup_hash({infos[1]->data(), infos[1]->size()}).hash};
				const auto lookupHash = Archetype::calc_lookup_hash(genericHash, chunkHash);

				auto* pArchetype =
						find_archetype(lookupHash, {infos[0]->data(), infos[0]->size()}, {infos[1]->data(), infos[1]->size()});
				if (pArchetype == nullptr) {
					pArchetype = create_archetype({infos[0]->data(), infos[0]->size()}, {infos[1]->data(), infos[1]->size()});
					pArchetype->set_hashes(genericHash, lookupHash, lookupHash);
					pArchetype->build_graph_edges(pArchetypeRight, compKind, infoToRemove.compId);
					reg_archetype(pArchetype);
				}

				return pArchetype;
			}

			//! Returns an array of archetypes registered in the world
			//! \return Array or archetypes.
			const auto& get_archetypes() const {
				return m_archetypes;
			}

			//! Returns the archetype the entity belongs to.
			//! \param entity Entity
			//! \return Reference to the archetype.
			GAIA_NODISCARD Archetype& get_archetype(Entity entity) const {
				GAIA_ASSERT(valid(entity));

				const auto& entityContainer = m_entities[entity.id()];
				auto* pChunk = entityContainer.pChunk;
				return *m_archetypes[pChunk == nullptr ? ArchetypeId(0) : pChunk->archetype_id()];
			}

			//! Invalidates the entity record, effectivelly deleting it
			//! \param entity Entity to delete
			void del_entity(Entity entity) {
				auto& entityContainer = m_entities.free(entity);
				entityContainer.pChunk = nullptr;
			}

			//! Associates an entity with a chunk.
			//! \param entity Entity to associate with a chunk
			//! \param pChunk Chunk the entity is to become a part of
			void store_entity(Entity entity, Chunk* pChunk) {
				GAIA_ASSERT(pChunk != nullptr);
				GAIA_ASSERT(
						!pChunk->has_structural_changes() && "Entities can't be added while their chunk is being iterated "
																								 "(structural changes are forbidden during this time!)");

				auto& entityContainer = m_entities[entity.id()];
				entityContainer.pChunk = pChunk;
				entityContainer.idx = pChunk->add_entity(entity);
				entityContainer.gen = entity.gen();
				entityContainer.dis = 0;
			}

			/*!
			Moves an entity along with all its generic components from its current chunk to another one.
			\param oldEntity Entity to move
			\param targetChunk Target chunk
			*/
			void move_entity(Entity oldEntity, Chunk& targetChunk) {
				GAIA_PROF_SCOPE(move_entity);

				auto* pNewChunk = &targetChunk;

				auto& entityContainer = m_entities[oldEntity.id()];
				auto* pOldChunk = entityContainer.pChunk;

				const auto oldIndex0 = entityContainer.idx;
				const auto newIndex = pNewChunk->add_entity(oldEntity);
				const bool wasEnabled = !entityContainer.dis;

				auto& oldArchetype = *m_archetypes[pOldChunk->archetype_id()];
				auto& newArchetype = *m_archetypes[pNewChunk->archetype_id()];

				// Make sure the old entity becomes enabled now
				oldArchetype.enable_entity(pOldChunk, oldIndex0, true, {m_entities.data(), m_entities.size()});
				// Enabling the entity might have changed the index so fetch it again
				const auto oldIndex = entityContainer.idx;

				// No data movement necessary when dealing with the root archetype
				if GAIA_LIKELY (pNewChunk->archetype_id() + pOldChunk->archetype_id() != 0) {
					// Move data from the old chunk to the new one
					if (pOldChunk->archetype_id() == pNewChunk->archetype_id())
						pNewChunk->move_entity_data(oldEntity, newIndex, {m_entities.data(), m_entities.size()});
					else
						pNewChunk->move_foreign_entity_data(oldEntity, newIndex, {m_entities.data(), m_entities.size()});
				}

				// Transfer the original enabled state to the new archetype
				newArchetype.enable_entity(pNewChunk, newIndex, wasEnabled, {m_entities.data(), m_entities.size()});

				// Remove the entity record from the old chunk
				remove_entity<false>(pOldChunk, oldIndex);

				// Make the entity point to the new chunk
				entityContainer.pChunk = pNewChunk;
				entityContainer.idx = newIndex;
				entityContainer.gen = oldEntity.gen();
				GAIA_ASSERT((bool)entityContainer.dis == !wasEnabled);
				// entityContainer.dis = !wasEnabled;

				// End-state validation
				validate_chunk(pOldChunk);
				validate_chunk(pNewChunk);
				validate_entities();
			}

			/*!
			Moves an entity along with all its generic components from its current chunk to another one in a new archetype.
			\param oldEntity Entity to move
			\param newArchetype Target archetype
			*/
			void move_entity(Entity oldEntity, Archetype& newArchetype) {
				auto* pNewChunk = newArchetype.foc_free_chunk();
				return move_entity(oldEntity, *pNewChunk);
			}

			//! Verifies than the implicit linked list of entities is valid
			void validate_entities() const {
#if GAIA_ECS_VALIDATE_ENTITY_LIST
				m_entities.validate();
#endif
			}

			//! Verifies that the chunk is valid
			void validate_chunk([[maybe_unused]] Chunk* pChunk) const {
#if GAIA_ECS_VALIDATE_CHUNKS
				// Note: Normally we'd go [[maybe_unused]] instead of "(void)" but MSVC
				// 2017 suffers an internal compiler error in that case...
				(void)pChunk;
				GAIA_ASSERT(pChunk != nullptr);

				if (pChunk->has_entities()) {
					// Make sure a proper amount of entities reference the chunk
					uint32_t cnt = 0;
					for (const auto& e: m_entities) {
						if (e.pChunk != pChunk)
							continue;
						++cnt;
					}
					GAIA_ASSERT(cnt == pChunk->size());
				} else {
					// Make sure no entites reference the chunk
					for (const auto& e: m_entities) {
						(void)e;
						GAIA_ASSERT(e.pChunk != pChunk);
					}
				}
#endif
			}

			EntityContainer& add_inter(ComponentKind compKind, Entity entity, const ComponentInfo& infoToAdd) {
				GAIA_PROF_SCOPE(AddComponent);

				auto& entityContainer = m_entities[entity.id()];

				auto* pChunk = entityContainer.pChunk;

				GAIA_ASSERT(pChunk != nullptr);
				GAIA_ASSERT(
						!pChunk->has_structural_changes() && "New components can't be added while their chunk is being iterated "
																								 "(structural changes are forbidden during this time!)");

				// Adding a component to an entity which already is a part of some chunk
				{
					auto& archetype = *m_archetypes[pChunk->archetype_id()];

#if GAIA_DEBUG
					verify_add(archetype, entity, compKind, infoToAdd);
#endif

					auto* pTargetArchetype = foc_archetype_add_comp(&archetype, compKind, infoToAdd);
					move_entity(entity, *pTargetArchetype);
					pChunk = entityContainer.pChunk;
				}

				// Call the constructor for the newly added component if necessary
				if (compKind == ComponentKind::CK_Generic)
					pChunk->call_ctor(compKind, infoToAdd.compId, entityContainer.idx);
				else if (compKind == ComponentKind::CK_Chunk)
					pChunk->call_ctor(compKind, infoToAdd.compId, 0);

				return entityContainer;
			}

			ComponentSetter del_inter(ComponentKind compKind, Entity entity, const ComponentInfo& infoToRemove) {
				GAIA_PROF_SCOPE(del);

				auto& entityContainer = m_entities[entity.id()];
				auto* pChunk = entityContainer.pChunk;

				GAIA_ASSERT(pChunk != nullptr);
				GAIA_ASSERT(
						!pChunk->has_structural_changes() && "Components can't be removed while their chunk is being iterated "
																								 "(structural changes are forbidden during this time!)");

				auto& archetype = *m_archetypes[pChunk->archetype_id()];

#if GAIA_DEBUG
				verify_del(archetype, entity, compKind, infoToRemove);
#endif

				auto* pNewArchetype = foc_archetype_remove_comp(&archetype, compKind, infoToRemove);
				GAIA_ASSERT(pNewArchetype != nullptr);
				move_entity(entity, *pNewArchetype);

				return ComponentSetter{pChunk, entityContainer.idx};
			}

			void init() {
				auto* pRootArchetype = create_archetype({}, {});
				pRootArchetype->set_hashes({0}, {0}, Archetype::calc_lookup_hash({0}, {0}));
				reg_archetype(pRootArchetype);
			}

			void done() {
				cleanup();

				ChunkAllocator::get().flush();

#if GAIA_DEBUG && GAIA_ECS_CHUNK_ALLOCATOR
				// Make sure there are no leaks
				ChunkAllocatorStats memStats = ChunkAllocator::get().stats();
				for (const auto& s: memStats.stats) {
					if (s.mem_total != 0) {
						GAIA_ASSERT(false && "ECS leaking memory");
						GAIA_LOG_W("ECS leaking memory!");
						ChunkAllocator::get().diag();
					}
				}
#endif
			}

			//! Creates a new entity of a given archetype
			//! \param archetype Archetype the entity should inherit
			//! \return New entity
			GAIA_NODISCARD Entity add(Archetype& archetype) {
				const auto entity = m_entities.alloc();

				auto* pChunk = archetype.foc_free_chunk();
				store_entity(entity, pChunk);

				// Call constructors for the generic components on the newly added entity if necessary
				if (pChunk->has_custom_generic_ctor())
					pChunk->call_ctors(ComponentKind::CK_Generic, pChunk->size() - 1, 1);

				return entity;
			}

			//! Garbage collection. Checks all chunks and archetypes which are empty and have not been
			//! used for a while and tries to delete them and release memory allocated by them.
			void GC() {
				// Handle chunks
				for (uint32_t i = 0; i < m_chunksToRemove.size();) {
					auto* pChunk = m_chunksToRemove[i];

					// Skip reclaimed chunks
					if (pChunk->has_entities()) {
						pChunk->prepare_to_die();
						core::erase_fast(m_chunksToRemove, i);
						continue;
					}

					if (pChunk->progress_death()) {
						++i;
						continue;
					}
				}

				// Remove all dead chunks
				for (auto* pChunk: m_chunksToRemove) {
					auto& archetype = *m_archetypes[pChunk->archetype_id()];
					archetype.remove_chunk(pChunk);
				}
				m_chunksToRemove.clear();

				// Defragment chunks only now. If we did this at the begging of the function,
				// we would needlessly iterate chunks which have no way of being collected because
				// it would be their first frame dying. This way, the number of chunks to process
				// is lower.
				defrag_chunks(GAIA_DEFRAG_ENTITIES_PER_FRAME);
			}

		public:
			World() {
				init();
			}

			~World() {
				done();
			}

			World(World&&) = delete;
			World(const World&) = delete;
			World& operator=(World&&) = delete;
			World& operator=(const World&) = delete;

			//! Checks if \param entity is valid.
			//! \return True is the entity is valid. False otherwise.
			GAIA_NODISCARD bool valid(Entity entity) const {
				// Entity ID has to fit inside the entity array
				if (entity.id() >= m_entities.size())
					return false;

				const auto& entityContainer = m_entities[entity.id()];

				// Generation ID has to match the one in the array
				if (entityContainer.gen != entity.gen())
					return false;

				// The entity in the chunk must match the index in the entity container
				auto* pChunk = entityContainer.pChunk;
				return pChunk != nullptr && pChunk->get_entity(entityContainer.idx) == entity;
			}

			//! Checks if \param entity is currently used by the world.
			//! \return True is the entity is used. False otherwise.
			GAIA_NODISCARD bool has(Entity entity) const {
				// Entity ID has to fit inside the entity array
				if (entity.id() >= m_entities.size())
					return false;

				// Index of the entity must fit inside the chunk
				const auto& entityContainer = m_entities[entity.id()];
				auto* pChunk = entityContainer.pChunk;
				return pChunk != nullptr && entityContainer.idx < pChunk->size();
			}

			//! Clears the world so that all its entities and components are released
			void cleanup() {
				// Clear entities
				m_entities.clear();

				// Clear archetypes
				{
					// Delete all allocated chunks and their parent archetypes
					for (auto* pArchetype: m_archetypes)
						delete pArchetype;

					m_archetypes = {};
					m_archetypeMap = {};
					m_chunksToRemove = {};
				}
			}

			//----------------------------------------------------------------------

			//! Returns the current version of the world.
			//! \return World version number.
			GAIA_NODISCARD uint32_t& world_version() {
				return m_worldVersion;
			}

			//----------------------------------------------------------------------

			//! Creates a new empty entity
			//! \return New entity
			GAIA_NODISCARD Entity add() {
				return add(*m_archetypes[0]);
			}

			//! Creates a new entity by cloning an already existing one.
			//! \param entity Entity to clone
			//! \return New entity
			GAIA_NODISCARD Entity add(Entity entity) {
				auto& entityContainer = m_entities[entity.id()];

				auto* pChunk = entityContainer.pChunk;
				GAIA_ASSERT(pChunk != nullptr);

				auto& archetype = *m_archetypes[pChunk->archetype_id()];
				const auto newEntity = add(archetype);

				Chunk::copy_entity_data(entity, newEntity, {m_entities.data(), m_entities.size()});

				return newEntity;
			}

			//! Removes an entity along with all data associated with it.
			//! \param entity Entity to delete
			void del(Entity entity) {
				if (m_entities.item_count() == 0 || entity == EntityNull)
					return;

				GAIA_ASSERT(valid(entity));

				const auto& entityContainer = m_entities[entity.id()];

				// Remove entity from chunk
				if (auto* pChunk = entityContainer.pChunk) {
					remove_entity<true>(pChunk, entityContainer.idx);
					validate_chunk(pChunk);
					validate_entities();
				} else {
					del_entity(entity);
				}
			}

			//! Enables or disables an entire entity.
			//! \param entity Entity
			//! \param enable Enable or disable the entity
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			void enable(Entity entity, bool enable) {
				auto& entityContainer = m_entities[entity.id()];

				GAIA_ASSERT(
						(!entityContainer.pChunk || !entityContainer.pChunk->has_structural_changes()) &&
						"Entities can't be enabled/disabled while their chunk is being iterated "
						"(structural changes are forbidden during this time!)");

				if (auto* pChunk = entityContainer.pChunk) {
					auto& archetype = *m_archetypes[pChunk->archetype_id()];
					archetype.enable_entity(pChunk, entityContainer.idx, enable, {m_entities.data(), m_entities.size()});
				}
			}

			//! Checks if an entity is valid.
			//! \param entity Entity
			//! \return True it the entity is valid. False otherwise.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			bool enabled(Entity entity) const {
				GAIA_ASSERT(valid(entity));

				const auto& entityContainer = m_entities[entity.id()];
				const bool entityStateInContainer = !entityContainer.dis;
#if GAIA_ASSERT_ENABLED
				const bool entityStateInChunk = entityContainer.pChunk->enabled(entityContainer.idx);
				GAIA_ASSERT(entityStateInChunk == entityStateInContainer);
#endif
				return entityStateInContainer;
			}

			//! Returns the number of active entities
			//! \return Entity
			GAIA_NODISCARD GAIA_FORCEINLINE uint32_t size() const {
				return m_entities.item_count();
			}

			//! Returns an entity at the index \param idx
			//! \return Entity
			GAIA_NODISCARD Entity get(uint32_t idx) const {
				GAIA_ASSERT(idx < m_entities.size());
				const auto& entityContainer = m_entities[idx];
				return {idx, entityContainer.gen};
			}

			//! Returns a chunk containing the \param entity.
			//! \return Chunk or nullptr if not found.
			GAIA_NODISCARD Chunk* get_chunk(Entity entity) const {
				GAIA_ASSERT(entity.id() < m_entities.size());
				const auto& entityContainer = m_entities[entity.id()];
				return entityContainer.pChunk;
			}

			//! Returns a chunk containing the \param entity.
			//! Index of the entity is stored in \param indexInChunk
			//! \return Chunk or nullptr if not found
			GAIA_NODISCARD Chunk* get_chunk(Entity entity, uint32_t& indexInChunk) const {
				GAIA_ASSERT(entity.id() < m_entities.size());
				const auto& entityContainer = m_entities[entity.id()];
				indexInChunk = entityContainer.idx;
				return entityContainer.pChunk;
			}

			//----------------------------------------------------------------------

			//! Attaches a new component \tparam T to \param entity.
			//! \tparam T Component
			//! \param entity Entity
			//! \return ComponentSetter
			//! \warning It is expected the component is not present on \param entity yet. Undefined behavior otherwise.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			template <typename T>
			ComponentSetter add(Entity entity) {
				verify_comp<T>();
				GAIA_ASSERT(valid(entity));

				using U = typename component_kind_t<T>::Kind;
				const auto& info = ComponentCache::get().goc_comp_info<U>();

				constexpr auto compKind = component_kind_v<T>;
				auto& entityContainer = add_inter(compKind, entity, info);
				return ComponentSetter{entityContainer.pChunk, entityContainer.idx};
			}

			//! Attaches a new component \tparam T to \param entity. Also sets its value.
			//! \tparam T Component
			//! \param entity Entity
			//! \param value Value to set for the component
			//! \return ComponentSetter object.
			//! \warning It is expected the component is not present on \param entity yet. Undefined behavior otherwise.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			template <typename T, typename U = typename component_kind_t<T>::Kind>
			ComponentSetter add(Entity entity, U&& value) {
				verify_comp<T>();
				GAIA_ASSERT(valid(entity));

				const auto& info = ComponentCache::get().goc_comp_info<U>();

				if constexpr (component_kind_v<T> == ComponentKind::CK_Generic) {
					auto& entityContainer = add_inter(ComponentKind::CK_Generic, entity, info);
					auto* pChunk = entityContainer.pChunk;
					pChunk->template set<T>(entityContainer.idx, std::forward<U>(value));
					return ComponentSetter{entityContainer.pChunk, entityContainer.idx};
				} else {
					auto& entityContainer = add_inter(ComponentKind::CK_Chunk, entity, info);
					auto* pChunk = entityContainer.pChunk;
					pChunk->template set<T>(std::forward<U>(value));
					return ComponentSetter{entityContainer.pChunk, entityContainer.idx};
				}
			}

			//! Removes a component \tparam T from \param entity.
			//! \tparam T Component
			//! \param entity Entity
			//! \return ComponentSetter
			//! \warning It is expected the component is present on \param entity. Undefined behavior otherwise.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			template <typename T>
			ComponentSetter del(Entity entity) {
				verify_comp<T>();
				GAIA_ASSERT(valid(entity));

				using U = typename component_kind_t<T>::Kind;
				const auto& info = ComponentCache::get().goc_comp_info<U>();

				constexpr auto compKind = component_kind_v<T>;
				return del_inter(compKind, entity, info);
			}

			//----------------------------------------------------------------------

			//! Sets the value of the component \tparam T on \param entity.
			//! \tparam T Component
			//! \param entity Entity
			//! \param value Value to set for the component
			//! \return ComponentSetter
			//! \warning It is expected the component is present on \param entity. Undefined behavior otherwise.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			template <typename T, typename U = typename component_kind_t<T>::Kind>
			ComponentSetter set(Entity entity, U&& value) {
				GAIA_ASSERT(valid(entity));

				const auto& entityContainer = m_entities[entity.id()];
				return ComponentSetter{entityContainer.pChunk, entityContainer.idx}.set<T>(std::forward<U>(value));
			}

			//! Sets the value of the component \tparam T on \param entity without trigger a world version update.
			//! \tparam T Component
			//! \param entity Entity
			//! \param value Value to set for the component
			//! \return ComponentSetter
			//! \warning It is expected the component is present on \param entity. Undefined behavior otherwise.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			template <typename T, typename U = typename component_kind_t<T>::Kind>
			ComponentSetter sset(Entity entity, U&& value) {
				GAIA_ASSERT(valid(entity));

				const auto& entityContainer = m_entities[entity.id()];
				return ComponentSetter{entityContainer.pChunk, entityContainer.idx}.sset<T>(std::forward<U>(value));
			}

			//----------------------------------------------------------------------

			//! Returns the value stored in the component \tparam T on \param entity.
			//! \tparam T Component
			//! \param entity Entity
			//! \return Value stored in the component.
			//! \warning It is expected the component is present on \param entity. Undefined behavior otherwise.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			template <typename T>
			GAIA_NODISCARD auto get(Entity entity) const {
				GAIA_ASSERT(valid(entity));

				const auto& entityContainer = m_entities[entity.id()];
				return ComponentGetter{entityContainer.pChunk, entityContainer.idx}.get<T>();
			}

			//! Tells if \param entity contains the component \tparam T.
			//! \tparam T Component
			//! \param entity Entity
			//! \return True if the component is present on entity.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			template <typename T>
			GAIA_NODISCARD bool has(Entity entity) const {
				GAIA_ASSERT(valid(entity));

				const auto& entityContainer = m_entities[entity.id()];
				return ComponentGetter{entityContainer.pChunk, entityContainer.idx}.has<T>();
			}

		private:
			template <typename... T>
			static void unpack_args_into_query(Query& query, [[maybe_unused]] core::func_type_list<T...> types) {
				static_assert(sizeof...(T) > 0, "Inputs-less functors can not be unpacked to query");
				query.all<T...>();
			}

			template <typename... T>
			static void reg_comps_inter([[maybe_unused]] core::func_type_list<T...> types) {
				static_assert(sizeof...(T) > 0, "Empty Query is not supported in this context");
				auto& cc = ComponentCache::get();
				((void)cc.goc_comp_info<T>(), ...);
			}

			template <typename Func>
			static void reg_comps() {
				using InputArgs = decltype(core::func_args(&Func::operator()));
				reg_comps_inter(InputArgs{});
			}

			template <typename... T>
			static constexpr ComponentLookupHash
			calc_query_id_lookup_hash([[maybe_unused]] core::func_type_list<T...> types) {
				return calc_lookup_hash<T...>();
			}

		public:
			template <bool UseCache = true>
			auto query() {
				if constexpr (UseCache)
					return Query(m_queryCache, m_worldVersion, m_archetypes, m_componentToArchetypeMap);
				else
					return QueryUncached(m_worldVersion, m_archetypes, m_componentToArchetypeMap);
			}

			//! Iterates over all chunks satisfying conditions set by \param func and calls \param func for all of them.
			//! Query instance is generated internally from the input arguments of \param func.
			//! \warning Performance-wise it has less potential than iterating using ecs::Chunk or a comparable each
			//!          passing in a query because it needs to do cached query lookups on each invocation. However, it is
			//!          easier to use and for non-critical code paths it is the most elegant way of iterating your data.
			template <typename Func>
			void each(Func func) {
				using InputArgs = decltype(core::func_args(&Func::operator()));

				reg_comps<Func>();

				constexpr auto lookupHash = calc_query_id_lookup_hash(InputArgs{});
				if (m_uniqueFuncQueryPairs.count(lookupHash) == 0) {
					Query q = query();
					unpack_args_into_query(q, InputArgs{});
					(void)q.fetch_query_info();
					m_uniqueFuncQueryPairs.try_emplace(lookupHash, q.id());
					query().each(q.id(), func);
				} else {
					const auto queryId = m_uniqueFuncQueryPairs[lookupHash];
					query().each(queryId, func);
				}
			}

			//! Performs various internal operations related to the end of the frame such as
			//! memory cleanup and other various managment operations which keep the system healthy.
			void update() {
				GC();

				// Signal the end of the frame
				GAIA_PROF_FRAME();
			}

			//--------------------------------------------------------------------------------

			//! Performs diagnostics on archetypes. Prints basic info about them and the chunks they contain.
			void diag_archetypes() const {
				GAIA_LOG_N("Archetypes:%u", (uint32_t)m_archetypes.size());
				for (const auto* archetype: m_archetypes)
					Archetype::diag(*archetype);
			}

			//! Performs diagnostics on registered components.
			//! Prints basic info about them and reports and detected issues.
			static void diag_components() {
				ComponentCache::get().diag();
			}

			//! Performs diagnostics on entites of the world.
			//! Also performs validation of internal structures which hold the entities.
			void diag_entities() const {
				validate_entities();

				GAIA_LOG_N("Deleted entities: %u", (uint32_t)m_entities.get_free_items());
				if (m_entities.get_free_items() != 0U) {
					GAIA_LOG_N("  --> %u", (uint32_t)m_entities.get_next_free_item());

					uint32_t iters = 0;
					auto fe = m_entities[m_entities.get_next_free_item()].idx;
					while (fe != Entity::IdMask) {
						GAIA_LOG_N("  --> %u", m_entities[fe].idx);
						fe = m_entities[fe].idx;
						++iters;
						if (iters > m_entities.get_free_items())
							break;
					}

					if ((iters == 0U) || iters > m_entities.get_free_items())
						GAIA_LOG_E("  Entities recycle list contains inconsistent data!");
				}
			}

			/*!
			Performs all diagnostics.
			*/
			void diag() const {
				diag_archetypes();
				diag_components();
				diag_entities();
			}
		};
	} // namespace ecs
} // namespace gaia

namespace gaia {
	namespace ecs {
		struct TempEntity final {
			uint32_t id;
		};

		/*!
		Buffer for deferred execution of some operations on entities.

		Adding and removing components and entities inside World::each or can result
		in changes of archetypes or chunk structure. This would lead to undefined behavior.
		Therefore, such operations have to be executed after the loop is done.
		*/
		class CommandBuffer final {
			struct CommandBufferCtx: SerializationBuffer {
				ecs::World& world;
				uint32_t entities;
				cnt::map<uint32_t, Entity> entityMap;

				CommandBufferCtx(ecs::World& w): world(w), entities(0) {}

				using SerializationBuffer::reset;
				void reset() {
					SerializationBuffer::reset();
					entities = 0;
					entityMap.clear();
				}
			};

			enum CommandBufferCmd : uint8_t {
				CREATE_ENTITY,
				CREATE_ENTITY_FROM_ARCHETYPE,
				CREATE_ENTITY_FROM_ENTITY,
				DELETE_ENTITY,
				ADD_COMPONENT,
				ADD_COMPONENT_DATA,
				ADD_COMPONENT_TO_TEMPENTITY,
				ADD_COMPONENT_TO_TEMPENTITY_DATA,
				SET_COMPONENT,
				SET_COMPONENT_FOR_TEMPENTITY,
				REMOVE_COMPONENT
			};

			struct CommandBufferCmd_t {
				CommandBufferCmd id;
			};

			struct CREATE_ENTITY_t: CommandBufferCmd_t {};
			struct CREATE_ENTITY_FROM_ARCHETYPE_t: CommandBufferCmd_t {
				uint64_t archetypePtr;

				void commit(CommandBufferCtx& ctx) const {
					auto* pArchetype = (Archetype*)archetypePtr;
					[[maybe_unused]] const auto res = ctx.entityMap.try_emplace(ctx.entities++, ctx.world.add(*pArchetype));
					GAIA_ASSERT(res.second);
				}
			};
			struct CREATE_ENTITY_FROM_ENTITY_t: CommandBufferCmd_t {
				Entity entity;

				void commit(CommandBufferCtx& ctx) const {
					[[maybe_unused]] const auto res = ctx.entityMap.try_emplace(ctx.entities++, ctx.world.add(entity));
					GAIA_ASSERT(res.second);
				}
			};
			struct DELETE_ENTITY_t: CommandBufferCmd_t {
				Entity entity;

				void commit(CommandBufferCtx& ctx) const {
					ctx.world.del(entity);
				}
			};
			struct ADD_COMPONENT_t: CommandBufferCmd_t {
				Entity entity;
				ComponentId compId;
				ComponentKind compKind;

				void commit(CommandBufferCtx& ctx) const {
					const auto& info = ComponentCache::get().comp_info(compId);
					ctx.world.add_inter(compKind, entity, info);

#if GAIA_ASSERT_ENABLED
					[[maybe_unused]] uint32_t indexInChunk{};
					[[maybe_unused]] auto* pChunk = ctx.world.get_chunk(entity, indexInChunk);
					GAIA_ASSERT(pChunk != nullptr);
#endif
				}
			};
			struct ADD_COMPONENT_DATA_t: CommandBufferCmd_t {
				Entity entity;
				ComponentId compId;
				ComponentKind compKind;

				void commit(CommandBufferCtx& ctx) const {
					const auto& info = ComponentCache::get().comp_info(compId);
					ctx.world.add_inter(compKind, entity, info);

					uint32_t indexInChunk{};
					auto* pChunk = ctx.world.get_chunk(entity, indexInChunk);
					GAIA_ASSERT(pChunk != nullptr);

					if (compKind == ComponentKind::CK_Chunk)
						indexInChunk = 0;

					// Component data
					const auto& desc = ComponentCache::get().comp_desc(compId);
					const auto offset = pChunk->find_data_offset(compKind, info.compId);
					auto* pComponentData = (void*)&pChunk->data(offset + (uint32_t)indexInChunk * desc.properties.size);
					ctx.load_comp(pComponentData, compId);
				}
			};
			struct ADD_COMPONENT_TO_TEMPENTITY_t: CommandBufferCmd_t {
				TempEntity tempEntity;
				ComponentId compId;
				ComponentKind compKind;

				void commit(CommandBufferCtx& ctx) const {
					// For delayed entities we have to do a look in our map
					// of temporaries and find a link there
					const auto it = ctx.entityMap.find(tempEntity.id);
					// Link has to exist!
					GAIA_ASSERT(it != ctx.entityMap.end());

					Entity entity = it->second;

					const auto& info = ComponentCache::get().comp_info(compId);
					ctx.world.add_inter(compKind, entity, info);

#if GAIA_ASSERT_ENABLED
					[[maybe_unused]] uint32_t indexInChunk{};
					[[maybe_unused]] auto* pChunk = ctx.world.get_chunk(entity, indexInChunk);
					GAIA_ASSERT(pChunk != nullptr);
#endif
				}
			};
			struct ADD_COMPONENT_TO_TEMPENTITY_DATA_t: CommandBufferCmd_t {
				TempEntity tempEntity;
				ComponentId compId;
				ComponentKind compKind;

				void commit(CommandBufferCtx& ctx) const {
					// For delayed entities we have to do a look in our map
					// of temporaries and find a link there
					const auto it = ctx.entityMap.find(tempEntity.id);
					// Link has to exist!
					GAIA_ASSERT(it != ctx.entityMap.end());

					Entity entity = it->second;

					// Components
					const auto& info = ComponentCache::get().comp_info(compId);
					ctx.world.add_inter(compKind, entity, info);

					uint32_t indexInChunk{};
					auto* pChunk = ctx.world.get_chunk(entity, indexInChunk);
					GAIA_ASSERT(pChunk != nullptr);

					if (compKind == ComponentKind::CK_Chunk)
						indexInChunk = 0;

					// Component data
					const auto& desc = ComponentCache::get().comp_desc(compId);
					const auto offset = pChunk->find_data_offset(compKind, desc.compId);
					auto* pComponentData = (void*)&pChunk->data(offset + (uint32_t)indexInChunk * desc.properties.size);
					ctx.load_comp(pComponentData, compId);
				}
			};
			struct SET_COMPONENT_t: CommandBufferCmd_t {
				Entity entity;
				ComponentId compId;
				ComponentKind compKind;

				void commit(CommandBufferCtx& ctx) const {
					const auto& entityContainer = ctx.world.m_entities[entity.id()];
					auto* pChunk = entityContainer.pChunk;
					const auto indexInChunk = compKind == ComponentKind::CK_Chunk ? 0U : entityContainer.idx;

					// Component data
					const auto& desc = ComponentCache::get().comp_desc(compId);
					const auto offset = pChunk->find_data_offset(compKind, compId);
					auto* pComponentData = (void*)&pChunk->data(offset + (uint32_t)indexInChunk * desc.properties.size);
					ctx.load_comp(pComponentData, compId);
				}
			};
			struct SET_COMPONENT_FOR_TEMPENTITY_t: CommandBufferCmd_t {
				TempEntity tempEntity;
				ComponentId compId;
				ComponentKind compKind;

				void commit(CommandBufferCtx& ctx) const {
					// For delayed entities we have to do a look in our map
					// of temporaries and find a link there
					const auto it = ctx.entityMap.find(tempEntity.id);
					// Link has to exist!
					GAIA_ASSERT(it != ctx.entityMap.end());

					Entity entity = it->second;

					const auto& entityContainer = ctx.world.m_entities[entity.id()];
					auto* pChunk = entityContainer.pChunk;
					const auto indexInChunk = compKind == ComponentKind::CK_Chunk ? 0U : entityContainer.idx;

					// Component data
					const auto& desc = ComponentCache::get().comp_desc(compId);
					const auto offset = pChunk->find_data_offset(compKind, compId);
					auto* pComponentData = (void*)&pChunk->data(offset + (uint32_t)indexInChunk * desc.properties.size);
					ctx.load_comp(pComponentData, compId);
				}
			};
			struct REMOVE_COMPONENT_t: CommandBufferCmd_t {
				Entity entity;
				ComponentId compId;
				ComponentKind compKind;

				void commit(CommandBufferCtx& ctx) const {
					const auto& info = ComponentCache::get().comp_info(compId);
					ctx.world.del_inter(compKind, entity, info);
				}
			};

			friend class World;

			CommandBufferCtx m_ctx;
			uint32_t m_entities;

			/*!
			Requests a new entity to be created from archetype
			\return Entity that will be created. The id is not usable right away. It
			will be filled with proper data after commit()
			*/
			GAIA_NODISCARD TempEntity add(Archetype& archetype) {
				m_ctx.save(CREATE_ENTITY_FROM_ARCHETYPE);

				CREATE_ENTITY_FROM_ARCHETYPE_t cmd;
				cmd.archetypePtr = (uintptr_t)&archetype;
				ser::save(m_ctx, cmd);

				return {m_entities++};
			}

		public:
			CommandBuffer(World& world): m_ctx(world), m_entities(0) {}
			~CommandBuffer() = default;

			CommandBuffer(CommandBuffer&&) = delete;
			CommandBuffer(const CommandBuffer&) = delete;
			CommandBuffer& operator=(CommandBuffer&&) = delete;
			CommandBuffer& operator=(const CommandBuffer&) = delete;

			/*!
			Requests a new entity to be created
			\return Entity that will be created. The id is not usable right away. It
			will be filled with proper data after commit()
			*/
			GAIA_NODISCARD TempEntity add() {
				m_ctx.save(CREATE_ENTITY);

				return {m_entities++};
			}

			/*!
			Requests a new entity to be created by cloning an already existing
			entity \return Entity that will be created. The id is not usable right
			away. It will be filled with proper data after commit()
			*/
			GAIA_NODISCARD TempEntity add(Entity entityFrom) {
				m_ctx.save(CREATE_ENTITY_FROM_ENTITY);

				CREATE_ENTITY_FROM_ENTITY_t cmd;
				cmd.entity = entityFrom;
				ser::save(m_ctx, cmd);

				return {m_entities++};
			}

			/*!
			Requests an existing \param entity to be removed.
			*/
			void del(Entity entity) {
				m_ctx.save(DELETE_ENTITY);

				DELETE_ENTITY_t cmd;
				cmd.entity = entity;
				ser::save(m_ctx, cmd);
			}

			/*!
			Requests a component to be added to entity.
			*/
			template <typename T>
			void add(Entity entity) {
				// Make sure the component is registered
				const auto& info = ComponentCache::get().goc_comp_info<T>();

				using U = typename component_kind_t<T>::Kind;
				verify_comp<U>();

				m_ctx.save(ADD_COMPONENT);

				ADD_COMPONENT_t cmd;
				cmd.entity = entity;
				cmd.compKind = component_kind_v<T>;
				cmd.compId = info.compId;
				ser::save(m_ctx, cmd);
			}

			/*!
			Requests a component to be added to temporary entity.
			*/
			template <typename T>
			void add(TempEntity entity) {
				// Make sure the component is registered
				const auto& info = ComponentCache::get().goc_comp_info<T>();

				using U = typename component_kind_t<T>::Kind;
				verify_comp<U>();

				m_ctx.save(ADD_COMPONENT_TO_TEMPENTITY);

				ADD_COMPONENT_TO_TEMPENTITY_t cmd;
				cmd.tempEntity = entity;
				cmd.compKind = component_kind_v<T>;
				cmd.compId = info.compId;
				ser::save(m_ctx, cmd);
			}

			/*!
			Requests a component to be added to entity.
			*/
			template <typename T>
			void add(Entity entity, T&& value) {
				// Make sure the component is registered
				const auto& info = ComponentCache::get().goc_comp_info<T>();

				using U = typename component_kind_t<T>::Kind;
				verify_comp<U>();

				m_ctx.save(ADD_COMPONENT_DATA);

				ADD_COMPONENT_DATA_t cmd;
				cmd.entity = entity;
				cmd.compKind = component_kind_v<T>;
				cmd.compId = info.compId;
				ser::save(m_ctx, cmd);
				m_ctx.save_comp(std::forward<U>(value));
			}

			/*!
			Requests a component to be added to temporary entity.
			*/
			template <typename T>
			void add(TempEntity entity, T&& value) {
				// Make sure the component is registered
				const auto& info = ComponentCache::get().goc_comp_info<T>();

				using U = typename component_kind_t<T>::Kind;
				verify_comp<U>();

				m_ctx.save(ADD_COMPONENT_TO_TEMPENTITY_DATA);

				ADD_COMPONENT_TO_TEMPENTITY_t cmd;
				cmd.tempEntity = entity;
				cmd.compKind = component_kind_v<T>;
				cmd.compId = info.compId;
				ser::save(m_ctx, cmd);
				m_ctx.save_comp(std::forward<U>(value));
			}

			/*!
			Requests component data to be set to given values for a given entity.
			*/
			template <typename T>
			void set(Entity entity, T&& value) {
				// No need to check if the component is registered.
				// If we want to set the value of a component we must have created it already.
				// (void)ComponentCache::get().comp_info<T>();

				using U = typename component_kind_t<T>::Kind;
				verify_comp<U>();

				m_ctx.save(SET_COMPONENT);

				SET_COMPONENT_t cmd;
				cmd.entity = entity;
				cmd.compKind = component_kind_v<T>;
				cmd.compId = comp_id<T>();
				ser::save(m_ctx, cmd);
				m_ctx.save_comp(std::forward<U>(value));
			}

			/*!
			Requests component data to be set to given values for a given temp
			entity.
			*/
			template <typename T>
			void set(TempEntity entity, T&& value) {
				// No need to check if the component is registered.
				// If we want to set the value of a component we must have created it already.
				// (void)ComponentCache::get().goc_comp_info<T>();

				using U = typename component_kind_t<T>::Kind;
				verify_comp<U>();

				m_ctx.save(SET_COMPONENT_FOR_TEMPENTITY);

				SET_COMPONENT_FOR_TEMPENTITY_t cmd;
				cmd.tempEntity = entity;
				cmd.compKind = component_kind_v<T>;
				cmd.compId = comp_id<T>();
				ser::save(m_ctx, cmd);
				m_ctx.save_comp(std::forward<U>(value));
			}

			/*!
			Requests removal of a component from entity
			*/
			template <typename T>
			void del(Entity entity) {
				// No need to check if the component is registered.
				// If we want to remove a component we must have created it already.
				// (void)ComponentCache::get().goc_comp_info<T>();

				using U = typename component_kind_t<T>::Kind;
				verify_comp<U>();

				m_ctx.save(REMOVE_COMPONENT);

				REMOVE_COMPONENT_t cmd;
				cmd.entity = entity;
				cmd.compKind = component_kind_v<T>;
				cmd.compId = comp_id<T>();
				ser::save(m_ctx, cmd);
			}

		private:
			using CommandBufferReadFunc = void (*)(CommandBufferCtx& ctx);
			static constexpr CommandBufferReadFunc CommandBufferRead[] = {
					// CREATE_ENTITY
					[](CommandBufferCtx& ctx) {
						[[maybe_unused]] const auto res = ctx.entityMap.try_emplace(ctx.entities++, ctx.world.add());
						GAIA_ASSERT(res.second);
					},
					// CREATE_ENTITY_FROM_ARCHETYPE
					[](CommandBufferCtx& ctx) {
						CREATE_ENTITY_FROM_ARCHETYPE_t cmd;
						ser::load(ctx, cmd);
						cmd.commit(ctx);
					},
					// CREATE_ENTITY_FROM_ENTITY
					[](CommandBufferCtx& ctx) {
						CREATE_ENTITY_FROM_ENTITY_t cmd;
						ser::load(ctx, cmd);
						cmd.commit(ctx);
					},
					// DELETE_ENTITY
					[](CommandBufferCtx& ctx) {
						DELETE_ENTITY_t cmd;
						ser::load(ctx, cmd);
						cmd.commit(ctx);
					},
					// ADD_COMPONENT
					[](CommandBufferCtx& ctx) {
						ADD_COMPONENT_t cmd;
						ser::load(ctx, cmd);
						cmd.commit(ctx);
					},
					// ADD_COMPONENT_DATA
					[](CommandBufferCtx& ctx) {
						ADD_COMPONENT_DATA_t cmd;
						ser::load(ctx, cmd);
						cmd.commit(ctx);
					},
					// ADD_COMPONENT_TO_TEMPENTITY
					[](CommandBufferCtx& ctx) {
						ADD_COMPONENT_TO_TEMPENTITY_t cmd;
						ser::load(ctx, cmd);
						cmd.commit(ctx);
					},
					// ADD_COMPONENT_TO_TEMPENTITY_DATA
					[](CommandBufferCtx& ctx) {
						ADD_COMPONENT_TO_TEMPENTITY_DATA_t cmd;
						ser::load(ctx, cmd);
						cmd.commit(ctx);
					},
					// SET_COMPONENT
					[](CommandBufferCtx& ctx) {
						SET_COMPONENT_t cmd;
						ser::load(ctx, cmd);
						cmd.commit(ctx);
					},
					// SET_COMPONENT_FOR_TEMPENTITY
					[](CommandBufferCtx& ctx) {
						SET_COMPONENT_FOR_TEMPENTITY_t cmd;
						ser::load(ctx, cmd);
						cmd.commit(ctx);
					},
					// REMOVE_COMPONENT
					[](CommandBufferCtx& ctx) {
						REMOVE_COMPONENT_t cmd;
						ser::load(ctx, cmd);
						cmd.commit(ctx);
					}};

		public:
			/*!
			Commits all queued changes.
			*/
			void commit() {
				if (m_ctx.empty())
					return;

				// Extract data from the buffer
				m_ctx.seek(0);
				while (m_ctx.tell() < m_ctx.bytes()) {
					CommandBufferCmd id{};
					m_ctx.load(id);
					CommandBufferRead[id](m_ctx);
				}

				m_entities = 0;
				m_ctx.reset();
			} // namespace ecs
		}; // namespace gaia
	} // namespace ecs
} // namespace gaia

#include <cinttypes>

#include <cinttypes>
#include <cstring>

#if GAIA_DEBUG
	
#endif

namespace gaia {
	namespace ecs {
		class World;
		class BaseSystemManager;

#if GAIA_PROFILER_CPU
		constexpr uint32_t MaxSystemNameLength = 64;
#endif

		class BaseSystem {
			friend class BaseSystemManager;

			// A world this system belongs to
			World* m_world = nullptr;
#if GAIA_PROFILER_CPU
			//! System's name
			char m_name[MaxSystemNameLength]{};
#endif
			//! System's hash code
			uint64_t m_hash = 0;
			//! If true, the system is enabled and running
			bool m_enabled = true;
			//! If true, the system is to be destroyed
			bool m_destroy = false;

		protected:
			BaseSystem() = default;
			virtual ~BaseSystem() = default;

			BaseSystem(BaseSystem&&) = delete;
			BaseSystem(const BaseSystem&) = delete;
			BaseSystem& operator=(BaseSystem&&) = delete;
			BaseSystem& operator=(const BaseSystem&) = delete;

		public:
			GAIA_NODISCARD World& world() {
				return *m_world;
			}
			GAIA_NODISCARD const World& world() const {
				return *m_world;
			}

			//! Enable/disable system
			void enable(bool enable) {
				bool prev = m_enabled;
				m_enabled = enable;
				if (prev == enable)
					return;

				if (enable)
					OnStarted();
				else
					OnStopped();
			}

			//! Returns true if system is enabled
			GAIA_NODISCARD bool enabled() const {
				return m_enabled;
			}

		protected:
			//! Called when system is first created
			virtual void OnCreated() {}
			//! Called every time system is started (before the first run and after
			//! enable(true) is called
			virtual void OnStarted() {}

			//! Called right before every OnUpdate()
			virtual void BeforeOnUpdate() {}
			//! Called every time system is allowed to tick
			virtual void OnUpdate() {}
			//! Called aright after every OnUpdate()
			virtual void AfterOnUpdate() {}

			//! Called every time system is stopped (after enable(false) is called and
			//! before OnDestroyed when system is being destroyed
			virtual void OnStopped() {}
			//! Called when system are to be cleaned up.
			//! This always happens before OnDestroyed is called or at any point when
			//! simulation decides to bring the system back to the initial state
			//! without actually destroying it.
			virtual void OnCleanup() {}
			//! Called when system is being destroyed
			virtual void OnDestroyed() {}

			//! Returns true for systems this system depends on. False otherwise
			virtual bool DependsOn([[maybe_unused]] const BaseSystem* system) const {
				return false;
			}

		private:
			void set_destroyed(bool destroy) {
				m_destroy = destroy;
			}
			bool destroyed() const {
				return m_destroy;
			}
		};

		class BaseSystemManager {
		protected:
			using SystemHash = core::direct_hash_key<uint64_t>;

			World& m_world;
			//! Map of all systems - used for look-ups only
			cnt::map<SystemHash, BaseSystem*> m_systemsMap;
			//! List of systems - used for iteration
			cnt::darray<BaseSystem*> m_systems;
			//! List of new systems which need to be initialised
			cnt::darray<BaseSystem*> m_systemsToCreate;
			//! List of systems which need to be deleted
			cnt::darray<BaseSystem*> m_systemsToRemove;

		public:
			BaseSystemManager(World& world): m_world(world) {}
			virtual ~BaseSystemManager() {
				clear();
			}

			BaseSystemManager(BaseSystemManager&& world) = delete;
			BaseSystemManager(const BaseSystemManager& world) = delete;
			BaseSystemManager& operator=(BaseSystemManager&&) = delete;
			BaseSystemManager& operator=(const BaseSystemManager&) = delete;

			void clear() {
				for (auto* pSystem: m_systems)
					pSystem->enable(false);
				for (auto* pSystem: m_systems)
					pSystem->OnCleanup();
				for (auto* pSystem: m_systems)
					pSystem->OnDestroyed();
				for (auto* pSystem: m_systems)
					delete pSystem;

				m_systems.clear();
				m_systemsMap.clear();

				m_systemsToCreate.clear();
				m_systemsToRemove.clear();
			}

			void cleanup() {
				for (auto& s: m_systems)
					s->OnCleanup();
			}

			void update() {
				// Remove all systems queued to be destroyed
				for (auto* pSystem: m_systemsToRemove)
					pSystem->enable(false);
				for (auto* pSystem: m_systemsToRemove)
					pSystem->OnCleanup();
				for (auto* pSystem: m_systemsToRemove)
					pSystem->OnDestroyed();
				for (auto* pSystem: m_systemsToRemove)
					m_systemsMap.erase({pSystem->m_hash});
				for (auto* pSystem: m_systemsToRemove) {
					m_systems.erase(core::find(m_systems, pSystem));
				}
				for (auto* pSystem: m_systemsToRemove)
					delete pSystem;
				m_systemsToRemove.clear();

				if GAIA_UNLIKELY (!m_systemsToCreate.empty()) {
					// Sort systems if necessary
					sort();

					// Create all new systems
					for (auto* pSystem: m_systemsToCreate) {
						pSystem->OnCreated();
						if (pSystem->enabled())
							pSystem->OnStarted();
					}
					m_systemsToCreate.clear();
				}

				OnBeforeUpdate();

				for (auto* pSystem: m_systems) {
					if (!pSystem->enabled())
						continue;

					{
						GAIA_PROF_SCOPE2(&pSystem->m_name[0]);
						pSystem->BeforeOnUpdate();
						pSystem->OnUpdate();
						pSystem->AfterOnUpdate();
					}
				}

				OnAfterUpdate();
			}

			template <typename T>
			T* add([[maybe_unused]] const char* name = nullptr) {
				GAIA_SAFE_CONSTEXPR auto hash = meta::type_info::hash<std::decay_t<T>>();

				const auto res = m_systemsMap.try_emplace({hash}, nullptr);
				if GAIA_UNLIKELY (!res.second)
					return (T*)res.first->second;

				BaseSystem* pSystem = new T();
				pSystem->m_world = &m_world;

#if GAIA_PROFILER_CPU
				if (name == nullptr) {
					constexpr auto ct_name = meta::type_info::name<T>();
					const size_t len = ct_name.size() > MaxSystemNameLength - 1 ? MaxSystemNameLength - 1 : ct_name.size();

	#if GAIA_COMPILER_MSVC || GAIA_PLATFORM_WINDOWS
					strncpy_s(pSystem->m_name, ct_name.data(), len);
	#else
					strncpy(pSystem->m_name, ct_name.data(), len);
	#endif
				} else {
	#if GAIA_COMPILER_MSVC || GAIA_PLATFORM_WINDOWS
					strncpy_s(pSystem->m_name, name, (size_t)-1);
	#else
					strncpy(pSystem->m_name, name, MaxSystemNameLength - 1);
	#endif
				}

				pSystem->m_name[MaxSystemNameLength - 1] = 0;
#endif

				pSystem->m_hash = hash;
				res.first->second = pSystem;

				m_systems.push_back(pSystem);
				// Request initialization of the pSystem
				m_systemsToCreate.push_back(pSystem);

				return (T*)pSystem;
			}

			template <typename T>
			void del() {
				auto pSystem = find<T>();
				if (pSystem == nullptr || pSystem->destroyed())
					return;

				pSystem->set_destroyed(true);

				// Request removal of the system
				m_systemsToRemove.push_back(pSystem);
			}

			template <typename T>
			GAIA_NODISCARD T* find() {
				GAIA_SAFE_CONSTEXPR auto hash = meta::type_info::hash<std::decay_t<T>>();

				const auto it = m_systemsMap.find({hash});
				if (it != m_systemsMap.end())
					return (T*)it->second;

				return (T*)nullptr;
			}

		protected:
			virtual void OnBeforeUpdate() {}
			virtual void OnAfterUpdate() {}

		private:
			void sort() {
				for (uint32_t l = 0; l < m_systems.size() - 1; l++) {
					auto min = l;
					for (uint32_t p = l + 1; p < m_systems.size(); p++) {
						const auto* sl = m_systems[l];
						const auto* pl = m_systems[p];
						if (sl->DependsOn(pl))
							min = p;
					}

					auto* tmp = m_systems[min];
					m_systems[min] = m_systems[l];
					m_systems[l] = tmp;
				}

#if GAIA_DEBUG
				// Make sure there are no circular dependencies
				for (auto j = 1U; j < m_systems.size(); ++j) {
					if (!m_systems[j - 1]->DependsOn(m_systems[j]))
						continue;
					GAIA_ASSERT(false && "Wrong systems dependencies!");
					GAIA_LOG_E("Wrong systems dependencies!");
				}
#endif
			}
		};

	} // namespace ecs
} // namespace gaia

namespace gaia {
	namespace ecs {
		class System: public BaseSystem {
			friend class World;
			friend struct ExecutionContextBase;
		};

		class SystemManager final: public BaseSystemManager {
			CommandBuffer m_beforeUpdateCmdBuffer;
			CommandBuffer m_afterUpdateCmdBuffer;

		public:
			SystemManager(World& world):
					BaseSystemManager(world), m_beforeUpdateCmdBuffer(world), m_afterUpdateCmdBuffer(world) {}

			CommandBuffer& BeforeUpdateCmdBufer() {
				return m_beforeUpdateCmdBuffer;
			}
			CommandBuffer& AfterUpdateCmdBufer() {
				return m_afterUpdateCmdBuffer;
			}

		protected:
			void OnBeforeUpdate() final {
				m_beforeUpdateCmdBuffer.commit();
			}

			void OnAfterUpdate() final {
				m_afterUpdateCmdBuffer.commit();
			}
		};
	} // namespace ecs
} // namespace gaia

