#pragma once

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

#define GAIA_FOR(max) for (uint32_t i = 0; i < (max); ++i)
#define GAIA_FOR_(max, varname) for (uint32_t varname = 0; varname < (max); ++varname)
#define GAIA_FOR2(min, max) for (uint32_t i = (min); i < (max); ++i)
#define GAIA_FOR2_(min, max, varname) for (uint32_t varname = (min); varname < (max); ++varname)
#define GAIA_EACH(container) for (uint32_t i = 0; i < (container).size(); ++i)
#define GAIA_EACH_(container, varname) for (uint32_t varname = 0; varname < (container).size(); ++varname)

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

//! Replacement for std::move.
//! Rather than an intrinsic, older compilers would treat it an an ordinary function.
//! As a result, compilation times were longer and performance slower in non-optimized builds.
#define GAIA_MOV(x) static_cast<typename std::remove_reference<decltype(x)>::type&&>(x)
//! Replacement for std::forward.
//! Rather than an intrinsic, older compilers would treat it an an ordinary function.
//! As a result, compilation times were longer and performance slower in non-optimized builds.
#define GAIA_FWD(x) decltype(x)(x)

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

#ifdef __has_cpp_attribute
	#if __has_cpp_attribute(assume) >= 202207L
		#define GAIA_ASSUME(...) [[assume(__VA_ARGS__)]]
	#endif
#endif
#ifndef GAIA_ASSUME
	#if GAIA_COMPILER_CLANG
		#define GAIA_ASSUME(...) __builtin_assume(__VA_ARGS__)
	#elif GAIA_COMPILER_MSVC
		#define GAIA_ASSUME(...) __assume(__VA_ARGS__)
	#elif (GAIA_COMPILER_GCC && __GNUC__ >= 13)
		#define GAIA_ASSUME(...) __attribute__((__assume__(__VA_ARGS__)))
	#endif
#endif
#ifndef GAIA_ASSUME
	#define GAIA_ASSUME(...)
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
