
/*** Start of inlined file: config.h ***/
#pragma once

/*** Start of inlined file: config_core.h ***/
#pragma once

#if __has_include(<version.h>)
	#include <version.h>
#endif
#include <cstdint>

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
	#error "To build Gaia-ECS a compiler capable of at least C++17 is necessary"
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
	// Whether it is ARM or x86 we consider both little endian.
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
//! Wrapper around std::launder.
#define GAIA_ACC(x) std::launder(x)

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
	#if __has_cpp_attribute(fallthrough)
		#define GAIA_FALLTHROUGH [[fallthrough]]
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
#ifndef GAIA_FALLTHROUGH
	#define GAIA_FALLTHROUGH
#endif

// GCC 7 and some later versions had a bug that would artificially restrict alignas for stack
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
	#define GAIA_POPCNT(x) ((uint32_t)__builtin_popcount(static_cast<uint32_t>(x)))
	//! Returns the number of set bits in \param x
	#define GAIA_POPCNT64(x) ((uint32_t)__builtin_popcountll(static_cast<unsigned long long>(x)))

//! Returns the number of leading zeros of \param x or 32 if \param x is 0.
//! \warning Little-endian format.
	#define GAIA_CLZ(x) ((x) ? (uint32_t)__builtin_ctz(static_cast<uint32_t>(x)) : (uint32_t)32)
	//! Returns the number of leading zeros of \param x or 64 if \param x is 0.
//! \warning Little-endian format.
	#define GAIA_CLZ64(x) ((x) ? (uint32_t)__builtin_ctzll(static_cast<unsigned long long>(x)) : (uint32_t)64)

//! Returns the number of trailing zeros of \param x or 32 if \param x is 0.
//! \warning Little-endian format.
	#define GAIA_CTZ(x) ((x) ? (uint32_t)__builtin_clz(static_cast<uint32_t>(x)) : (uint32_t)32)
	//! Returns the number of trailing zeros of \param x or 64 if \param x is 0.
//! \warning Little-endian format.
	#define GAIA_CTZ64(x) ((x) ? (uint32_t)__builtin_clzll(static_cast<unsigned long long>(x)) : (uint32_t)64)

//! Returns 1 plus the index of the least significant set bit of \param x, or 0 if \param x is 0.
//! \warning Little-endian format.
	#define GAIA_FFS(x) ((uint32_t)__builtin_ffs(static_cast<int>(x)))
	//! Returns 1 plus the index of the least significant set bit of \param x, or 0 if \param x is 0.
//! \warning Little-endian format.
	#define GAIA_FFS64(x) ((uint32_t)__builtin_ffsll(static_cast<long long>(x)))
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

/*** End of inlined file: config_core.h ***/



/*** Start of inlined file: version.h ***/
#pragma once

// Breaking changes and big features
#define GAIA_VERSION_MAJOR 0
// Smaller changes and features
#define GAIA_VERSION_MINOR 8
// Fixes and tweaks
#define GAIA_VERSION_PATCH 9

/*** End of inlined file: version.h ***/

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

//! If enabled, memory prefetching is used when querying chunks, possibly improving performance in edge-cases
#ifndef GAIA_USE_PREFETCH
	#define GAIA_USE_PREFETCH 1
#endif

#ifndef GAIA_SYSTEMS_ENABLED
	#define GAIA_SYSTEMS_ENABLED 1
#endif

//------------------------------------------------------------------------------


/*** Start of inlined file: config_core_end.h ***/
#pragma once

//------------------------------------------------------------------------------
// DO NOT MODIFY THIS FILE
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// External code settings
//------------------------------------------------------------------------------

#ifndef TCB_SPAN_NO_CONTRACT_CHECKING
// #define TCB_SPAN_NO_CONTRACT_CHECKING
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
//! enabled by default for debug builds.
#if !defined(GAIA_DEBUG)
	#if GAIA_DEBUG_BUILD || GAIA_FORCE_DEBUG
		#define GAIA_DEBUG 1
	#else
		#define GAIA_DEBUG 0
	#endif
#endif

#if GAIA_DISABLE_ASSERTS
	#ifdef GAIA_ASSERT_ENABLED
		#undef
	#endif
	#define GAIA_ASSERT_ENABLED 0

	#ifdef GAIA_ASSERT
		#undef GAIA_ASSERT
	#endif
	#define GAIA_ASSERT(cond)

	#ifdef GAIA_ASSERT2
		#undef GAIA_ASSERT2
	#endif
	#define GAIA_ASSERT2(cond, msg)
#elif !defined(GAIA_ASSERT)
	#include <cassert>
	// For Debug builds use system's native assertion capabilities
	#if GAIA_DEBUG_BUILD
		#define GAIA_ASSERT_ENABLED 1
		#define GAIA_ASSERT(cond)                                                                                          \
			{                                                                                                                \
				GAIA_MSVC_WARNING_PUSH()                                                                                       \
				GAIA_MSVC_WARNING_DISABLE(4127)                                                                                \
				if GAIA_UNLIKELY (!(cond))                                                                                     \
					[&] {                                                                                                        \
						assert((cond));                                                                                            \
					}();                                                                                                         \
				GAIA_MSVC_WARNING_POP()                                                                                        \
			}
		#define GAIA_ASSERT2(cond, msg)                                                                                    \
			{                                                                                                                \
				GAIA_MSVC_WARNING_PUSH()                                                                                       \
				GAIA_MSVC_WARNING_DISABLE(4127)                                                                                \
				if GAIA_UNLIKELY (!(cond))                                                                                     \
					[&] {                                                                                                        \
						assert((cond) && (msg));                                                                                   \
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
					if GAIA_UNLIKELY (!(cond))                                                                                   \
						[] {                                                                                                       \
							GAIA_LOG_E("%s:%d: Assertion failed: '%s'.", __FILE__, __LINE__, #cond);                                 \
						}();                                                                                                       \
					GAIA_MSVC_WARNING_POP()                                                                                      \
				}
			#define GAIA_ASSERT2(cond, msg)                                                                                  \
				{                                                                                                              \
					GAIA_MSVC_WARNING_PUSH()                                                                                     \
					GAIA_MSVC_WARNING_DISABLE(4127)                                                                              \
					if GAIA_UNLIKELY (!(cond))                                                                                   \
						[] {                                                                                                       \
							GAIA_LOG_E("%s:%d: Assertion failed: '%s'.", __FILE__, __LINE__, (msg));                                 \
						}();                                                                                                       \
					GAIA_MSVC_WARNING_POP()                                                                                      \
				}
		#else
			#define GAIA_ASSERT_ENABLED 0
			#define GAIA_ASSERT(cond)
			#define GAIA_ASSERT2(cond, msg)
		#endif
	#endif
#endif

#if !defined(GAIA_ECS_VALIDATE_CHUNKS)
	#define GAIA_ECS_VALIDATE_CHUNKS (GAIA_DEBUG && GAIA_DEVMODE)
#endif
#if !defined(GAIA_ECS_VALIDATE_ENTITY_LIST)
	#define GAIA_ECS_VALIDATE_ENTITY_LIST (GAIA_DEBUG && GAIA_DEVMODE)
#endif
#if !defined(GAIA_ECS_VALIDATE_ARCHETYPE_GRAPH)
	#define GAIA_ECS_VALIDATE_ARCHETYPE_GRAPH (GAIA_DEBUG && GAIA_DEVMODE)
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
	inline void prefetch([[maybe_unused]] const void* x, [[maybe_unused]] int hint) {
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

/*** End of inlined file: config_core_end.h ***/

/*** End of inlined file: config.h ***/


/*** Start of inlined file: logging.h ***/
#pragma once

#include <cstdio> // vsnprintf, sscanf, printf

//! Log - debug
#ifndef GAIA_LOG_D
	#define GAIA_LOG_D(...)                                                                                              \
		{                                                                                                                  \
			fprintf(stdout, "\033[1;32m%s %s, D: ", __DATE__, __TIME__);                                                     \
			fprintf(stdout, __VA_ARGS__);                                                                                    \
			fprintf(stdout, "\033[0m\n");                                                                                    \
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
			fprintf(stderr, "\033[1;33m%s %s, W: ", __DATE__, __TIME__);                                                     \
			fprintf(stderr, __VA_ARGS__);                                                                                    \
			fprintf(stderr, "\033[0m\n");                                                                                    \
		}
#endif

//! Log - error
#ifndef GAIA_LOG_E
	#define GAIA_LOG_E(...)                                                                                              \
		{                                                                                                                  \
			fprintf(stderr, "\033[1;31m%s %s, E: ", __DATE__, __TIME__);                                                     \
			fprintf(stderr, __VA_ARGS__);                                                                                    \
			fprintf(stderr, "\033[0m\n");                                                                                    \
		}
#endif
/*** End of inlined file: logging.h ***/


/*** Start of inlined file: profiler.h ***/
#pragma once

#ifndef GAIA_PROFILER_CPU
	#define GAIA_PROFILER_CPU 0
#endif
#ifndef GAIA_PROFILER_MEM
	#define GAIA_PROFILER_MEM 0
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
					___tracy_alloc_srcloc_name(line, file, strlen(file), function, strlen(function), name, strlen(name), 0);
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
				__LINE__, __FILE__, strlen(__FILE__), function, strlen(function), name, strlen(name), 0));

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
	//! Profiling zone for mutex
	#if !defined(GAIA_PROF_MUTEX_BASE)
	//! Extracts reference to m_lockable from tracy::Lockable<T> because
	//! they don't provide a getter for the internal mutex object.
	//! Therefore, we would not be able to use conditional variables easily.
template <class TLockable, class T>
inline T& gaia_extract_lock_from_tracy_lockable(TLockable& lockable) {
	// Assuming TLockable is non-virtual and the memory layout is as follows:
	// 	T m_lockable;
	// 	LockableCtx m_ctx;
	//  ...
	//
	// m_lockable is the first member. We simply cast to it.
	T* ptr_lockable = reinterpret_cast<T*>(&lockable);
	return (T&)*(ptr_lockable + 0);
	// return (T&)lockable;
}
		#define GAIA_PROF_EXTRACT_MUTEX(type, name) gaia_extract_lock_from_tracy_lockable<decltype(name), type>(name)
		#define GAIA_PROF_MUTEX_BASE(type) LockableBase<type>
	#endif
	#if !defined(GAIA_PROF_MUTEX)
		#define GAIA_PROF_MUTEX(type, name) TracyLockable(type, name)
	#endif
	//! If set to 1 thread name will be set using the profiler's thread name setter function
	#if !defined(GAIA_PROF_USE_PROFILER_THREAD_NAME)
		#define GAIA_PROF_USE_PROFILER_THREAD_NAME 1
	#endif
	//! Sets the name of the thread for the profiler
	#if !defined(GAIA_PROF_THREAD_NAME)
		#define GAIA_PROF_THREAD_NAME(name) tracy::SetThreadName(name)
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
//! Profiling zone for mutex
	#if !defined(GAIA_PROF_MUTEX_BASE)
		#define GAIA_PROF_MUTEX_BASE(type) type
	#endif
	#if !defined(GAIA_PROF_MUTEX)
		#define GAIA_PROF_EXTRACT_MUTEX(type, name) name
		#define GAIA_PROF_MUTEX(type, name) GAIA_PROF_MUTEX_BASE(type) name
	#endif
	//! If set to 1 thread name will be set using the profiler's thread name setter function
	#if !defined(GAIA_PROF_USE_PROFILER_THREAD_NAME)
		#define GAIA_PROF_USE_PROFILER_THREAD_NAME 0
	#endif
	//! Sets the name of the thread for the profiler
	#if !defined(GAIA_PROF_THREAD_NAME)
		#define GAIA_PROF_THREAD_NAME(name)
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
/*** End of inlined file: profiler.h ***/


/*** Start of inlined file: bit_utils.h ***/
#pragma once


/*** Start of inlined file: span.h ***/
#pragma once

#if GAIA_USE_STD_SPAN
	#include <span>
#else

/*** Start of inlined file: span.hpp ***/
/*
This is an implementation of C++20's std::span
http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2019/n4820.pdf
*/

//          Copyright Tristan Brindle 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file ../../LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#pragma once

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
	#define TCB_SPAN_NAMESPACE_NAME gaia::core
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
	inline void contract_violation(const char* str) {
		GAIA_ASSERT(false && str);
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

/*** End of inlined file: span.hpp ***/


namespace std {
	using TCB_SPAN_NAMESPACE_NAME::span;
}
#endif

/*** End of inlined file: span.h ***/


/*** Start of inlined file: utility.h ***/
#pragma once

#include <cstdio>
#include <tuple>
#include <type_traits>
#include <utility>


/*** Start of inlined file: iterator.h ***/
#pragma once

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
			[[maybe_unused]] constexpr bool is_bidi_iter_v =
					std::is_convertible_v<iterator_cat_t<It>, bidirectional_iterator_tag>;

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

/*** End of inlined file: iterator.h ***/

namespace gaia {
	constexpr uint32_t BadIndex = uint32_t(-1);

#if GAIA_COMPILER_MSVC || GAIA_PLATFORM_WINDOWS
	#define GAIA_STRCPY(var, max_len, text)                                                                              \
		strncpy_s((var), (text), (size_t)-1);                                                                              \
		(void)max_len
	#define GAIA_STRFMT(var, max_len, fmt, ...) sprintf_s((var), (max_len), fmt, __VA_ARGS__)
#else
	#define GAIA_STRCPY(var, max_len, text)                                                                              \
		{                                                                                                                  \
			strncpy((var), (text), (max_len));                                                                               \
			(var)[(max_len) - 1] = 0;                                                                                        \
		}
	#define GAIA_STRFMT(var, max_len, fmt, ...) snprintf((var), (max_len), fmt, __VA_ARGS__)
#endif

	namespace core {
		namespace detail {
			template <class T>
			struct rem_rp {
				using type = std::remove_reference_t<std::remove_pointer_t<T>>;
			};

			template <typename T>
			struct is_mut:
					std::bool_constant<
							!std::is_const_v<typename rem_rp<T>::type> &&
							(std::is_pointer<T>::value || std::is_reference<T>::value)> {};
		} // namespace detail

		template <class T>
		using rem_rp_t = typename detail::rem_rp<T>::type;

		template <typename T>
		inline constexpr bool is_mut_v = detail::is_mut<T>::value;

		template <class T>
		using raw_t = typename std::decay_t<std::remove_pointer_t<T>>;

		template <typename T>
		inline constexpr bool is_raw_v = std::is_same_v<T, raw_t<T>> && !std::is_array_v<T>;

		//! Obtains the actual address of the object \param obj or function arg, even in presence of overloaded operator&.
		template <typename T>
		constexpr T* addressof(T& obj) noexcept {
			return &obj;
		}

		//! Rvalue overload is deleted to prevent taking the address of const rvalues.
		template <class T>
		const T* addressof(const T&&) = delete;

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

		//! Constructs an object of type \tparam T in the uninitialize storage at the memory address \param pData.
		template <typename T>
		void call_ctor_raw(T* pData) {
			GAIA_ASSERT(pData != nullptr);
			(void)::new (const_cast<void*>(static_cast<const volatile void*>(core::addressof(*pData)))) T;
		}

		//! Constructs \param cnt objects of type \tparam T in the uninitialize storage at the memory address \param pData.
		template <typename T>
		void call_ctor_raw_n(T* pData, size_t cnt) {
			GAIA_ASSERT(pData != nullptr);
			for (size_t i = 0; i < cnt; ++i) {
				auto* ptr = pData + i;
				(void)::new (const_cast<void*>(static_cast<const volatile void*>(core::addressof(*ptr)))) T;
			}
		}

		//! Value-constructs an object of type \tparam T in the uninitialize storage at the memory address \param pData.
		template <typename T>
		void call_ctor_val(T* pData) {
			GAIA_ASSERT(pData != nullptr);
			(void)::new (const_cast<void*>(static_cast<const volatile void*>(core::addressof(*pData)))) T();
		}

		//! Value-constructs \param cnt objects of type \tparam T in the uninitialize storage at the memory address \param
		//! pData.
		template <typename T>
		void call_ctor_val_n(T* pData, size_t cnt) {
			GAIA_ASSERT(pData != nullptr);
			for (size_t i = 0; i < cnt; ++i) {
				auto* ptr = pData + i;
				(void)::new (const_cast<void*>(static_cast<const volatile void*>(core::addressof(*ptr)))) T();
			}
		}

		//! Constructs an object of type \tparam T in at the memory address \param pData.
		template <typename T>
		void call_ctor(T* pData) {
			GAIA_ASSERT(pData != nullptr);
			if constexpr (!std::is_trivially_constructible_v<T>) {
				(void)::new (pData) T();
			}
		}

		//! Constructs \param cnt objects of type \tparam T starting at the memory address \param pData.
		template <typename T>
		void call_ctor_n(T* pData, size_t cnt) {
			GAIA_ASSERT(pData != nullptr);
			if constexpr (!std::is_trivially_constructible_v<T>) {
				for (size_t i = 0; i < cnt; ++i)
					(void)::new (pData + i) T();
			}
		}

		template <typename T, typename... Args>
		void call_ctor(T* pData, Args&&... args) {
			GAIA_ASSERT(pData != nullptr);
			if constexpr (std::is_constructible_v<T, Args...>)
				(void)::new (pData) T(GAIA_FWD(args)...);
			else
				(void)::new (pData) T{GAIA_FWD(args)...};
		}

		//! Constructs an object of type \tparam T at the memory address \param pData.
		template <typename T>
		void call_dtor(T* pData) {
			GAIA_ASSERT(pData != nullptr);
			if constexpr (!std::is_trivially_destructible_v<T>) {
				pData->~T();
			}
		}

		//! Constructs \param cnt objects of type \tparam T starting at the memory address \param pData.
		template <typename T>
		void call_dtor_n(T* pData, size_t cnt) {
			GAIA_ASSERT(pData != nullptr);
			if constexpr (!std::is_trivially_destructible_v<T>) {
				for (size_t i = 0; i < cnt; ++i)
					pData[i].~T();
			}
		}

		//----------------------------------------------------------------------
		// Element swapping
		//----------------------------------------------------------------------

		template <typename T>
		constexpr void swap(T& left, T& right) {
			T tmp = GAIA_MOV(left);
			left = GAIA_MOV(right);
			right = GAIA_MOV(tmp);
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

		namespace detail {
			template <class Default, class AlwaysVoid, template <class...> class Op, class... Args>
			struct member_func_checker {
				using value_t = std::false_type;
				using type = Default;
			};

			template <class Default, template <class...> class Op, class... Args>
			struct member_func_checker<Default, std::void_t<Op<Args...>>, Op, Args...> {
				using value_t = std::true_type;
				using type = Op<Args...>;
			};

			struct member_func_none {
				~member_func_none() = delete;
				member_func_none(member_func_none const&) = delete;
				void operator=(member_func_none const&) = delete;
			};
		} // namespace detail

		template <template <class...> class Op, typename... Args>
		using has_member_func = typename detail::member_func_checker<detail::member_func_none, void, Op, Args...>::value_t;

		template <typename Func, typename... Args>
		struct has_global_func {
			template <typename F, typename... A>
			static auto test(F&& f, A&&... args) -> decltype(GAIA_FWD(f)(GAIA_FWD(args)...), std::true_type{});

			template <typename...>
			static std::false_type test(...);

			static constexpr bool value = decltype(test(std::declval<Func>(), std::declval<Args>()...))::value;
		};

#define GAIA_DEFINE_HAS_FUNCTION(function_name)                                                                        \
	template <typename T, typename... Args>                                                                              \
	using has_##function_name##_check = decltype(std::declval<T>().function_name(std::declval<Args>()...));              \
                                                                                                                       \
	template <typename T, typename... Args>                                                                              \
	struct has_##function_name {                                                                                         \
		static constexpr bool value = gaia::core::has_member_func<has_##function_name##_check, T, Args...>::value;         \
	};

#define GAIA_HAS_MEMBER_FUNC(function_name, T, ...)                                                                    \
	gaia::core::has_member_func<has_##function_name##_check, T, __VA_ARGS__>::value

		// TODO: Try replacing the above with following:
		//
		//       #define GAIA_HAS_MEMBER_FUNC(function_name, T, ...)
		//         gaia::core::has_member_func<
		//           decltype(std::declval<T>().function_name(std::declval<Args>()...)),
		//					 T, __VA_ARGS__
		//         >::value
		//
		//       This way we could drop GAIA_DEFINE_HAS. However, the issue is that std::declval<Args>
		//       would have to be replaced with a variadic macro that expands into a series
		//       of std::declval<Arg> which is very inconvenient to do and always has a hard limit
		//       on the number of arguments which is super limiting.

#define GAIA_HAS_GLOBAL_FUNC(function_name, ...)                                                                       \
	gaia::core::has_global_func<decltype(&function_name), __VA_ARGS__>::value

		GAIA_DEFINE_HAS_FUNCTION(find)
		GAIA_DEFINE_HAS_FUNCTION(find_if)
		GAIA_DEFINE_HAS_FUNCTION(find_if_not)

		namespace detail {
			template <typename T>
			constexpr auto has_member_equals_check(int)
					-> decltype(std::declval<T>().operator==(std::declval<T>()), std::true_type{});
			template <typename T, typename... Args>
			constexpr std::false_type has_member_equals_check(...);

			template <typename T>
			constexpr auto has_global_equals_check(int)
					-> decltype(operator==(std::declval<T>(), std::declval<T>()), std::true_type{});
			template <typename T, typename... Args>
			constexpr std::false_type has_global_equals_check(...);
		} // namespace detail

		template <typename T>
		struct has_member_equals {
			static constexpr bool value = decltype(detail::has_member_equals_check<T>(0))::value;
		};

		template <typename T>
		struct has_global_equals {
			static constexpr bool value = decltype(detail::has_global_equals_check<T>(0))::value;
		};

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
		// Looping
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
			void each_tuple_impl(Func func, std::integer_sequence<decltype(FirstIdx), Is...> /*no_name*/) {
				if constexpr ((std::is_invocable_v<
													 Func&&, decltype(std::tuple_element_t<FirstIdx + Is, Tuple>{}),
													 std::integral_constant<decltype(FirstIdx), Is>> &&
											 ...))
					// func(Args&& arg, uint32_t idx)
					(func(
							 std::tuple_element_t<FirstIdx + Is, Tuple>{},
							 std::integral_constant<decltype(FirstIdx), FirstIdx + Is>{}),
					 ...);
				else
					// func(Args&& arg)
					(func(std::tuple_element_t<FirstIdx + Is, Tuple>{}), ...);
			}

			template <auto FirstIdx, typename Tuple, typename Func, auto... Is>
			void each_tuple_impl(Tuple&& tuple, Func func, std::integer_sequence<decltype(FirstIdx), Is...> /*no_name*/) {
				if constexpr ((std::is_invocable_v<
													 Func&&, decltype(std::get<FirstIdx + Is>(tuple)),
													 std::integral_constant<decltype(FirstIdx), Is>> &&
											 ...))
					// func(Args&& arg, uint32_t idx)
					(func(std::get<FirstIdx + Is>(tuple), std::integral_constant<decltype(FirstIdx), FirstIdx + Is>{}), ...);
				else
					// func(Args&& arg)
					(func(std::get<FirstIdx + Is>(tuple)), ...);
			}
		} // namespace detail

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
			(func(GAIA_FWD(args)), ...);
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
		//! Output:
		//! 69
		//! likes
		//! 420.0f
		template <typename Tuple, typename Func>
		constexpr void each_tuple(Tuple&& tuple, Func func) {
			using TTSize = uint32_t;
			constexpr auto TSize = (TTSize)std::tuple_size<std::remove_reference_t<Tuple>>::value;
			detail::each_tuple_impl<(TTSize)0>(GAIA_FWD(tuple), func, std::make_integer_sequence<TTSize, TSize>{});
		}

		//! Compile-time for loop over tuples and other objects implementing
		//! tuple_size (sarray, std::pair etc).
		//! \warning This does not use a tuple instance, only the type.
		//!          Use for compile-time operations only.
		//!
		//! Example:
		//! each_tuple(
		//!		std::make_tuple(69, "likes", 420.0f),
		//!		[](const auto& value) {
		//! 		std::cout << value << std::endl;
		//! 	});
		//! Output:
		//! 0
		//! nullptr
		//! 0.0f
		template <typename Tuple, typename Func>
		constexpr void each_tuple(Func func) {
			using TTSize = uint32_t;
			constexpr auto TSize = (TTSize)std::tuple_size<std::remove_reference_t<Tuple>>::value;
			detail::each_tuple_impl<(TTSize)0, Tuple>(func, std::make_integer_sequence<TTSize, TSize>{});
		}

		//! Compile-time for loop over tuples and other objects implementing
		//! tuple_size (sarray, std::pair etc).
		//! Iteration starts at \tparam FirstIdx and ends at \tparam LastIdx (excluding).
		//!
		//! Example:
		//! each_tuple_ext<1, 3>(
		//!		std::make_tuple(69, "likes", 420.0f),
		//!		[](const auto& value) {
		//! 		std::cout << value << std::endl;
		//! 	});
		//! Output:
		//! likes
		//! 420.0f
		template <auto FirstIdx, auto LastIdx, typename Tuple, typename Func>
		constexpr void each_tuple_ext(Tuple&& tuple, Func func) {
			constexpr auto TSize = std::tuple_size<std::remove_reference_t<Tuple>>::value;
			static_assert(LastIdx >= FirstIdx);
			static_assert(LastIdx <= TSize);
			constexpr auto Iters = LastIdx - FirstIdx;
			detail::each_tuple_impl<FirstIdx>(GAIA_FWD(tuple), func, std::make_integer_sequence<decltype(FirstIdx), Iters>{});
		}

		//! Compile-time for loop over tuples and other objects implementing
		//! tuple_size (sarray, std::pair etc).
		//! Iteration starts at \tparam FirstIdx and ends at \tparam LastIdx (excluding).
		//! \warning This does not use a tuple instance, only the type.
		//!          Use for compile-time operations only.
		//!
		//! Example:
		//! each_tuple(
		//!		1, 3, std::make_tuple(69, "likes", 420.0f),
		//!		[](const auto& value) {
		//! 		std::cout << value << std::endl;
		//! 	});
		//! Output:
		//! nullptr
		//! 0.0f
		template <auto FirstIdx, auto LastIdx, typename Tuple, typename Func>
		constexpr void each_tuple_ext(Func func) {
			constexpr auto TSize = std::tuple_size<std::remove_reference_t<Tuple>>::value;
			static_assert(LastIdx >= FirstIdx);
			static_assert(LastIdx <= TSize);
			constexpr auto Iters = LastIdx - FirstIdx;
			detail::each_tuple_impl<FirstIdx, Tuple>(func, std::make_integer_sequence<decltype(FirstIdx), Iters>{});
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

		//! Replaces the item at \param idx in the array \param arr with the last item of the array if possible and
		//! removes its last item. Use when shifting of the entire array is not wanted. \warning If the item order is
		//! important and the size of the array changes after calling this function you need to sort the array.
		//! \warning Does not do bound checks. Undefined behavior when \param idx is out of bounds.
		template <typename C>
		void swap_erase_unsafe(C& arr, typename C::size_type idx) {
			GAIA_ASSERT(idx < arr.size());

			if (idx + 1 != arr.size())
				arr[idx] = arr[arr.size() - 1];

			arr.pop_back();
		}

		//! Replaces the item at \param idx in the array \param arr with the last item of the array if possible and
		//! removes its last item. Use when shifting of the entire array is not wanted. \warning If the item order is
		//! important and the size of the array changes after calling this function you need to sort the array.
		template <typename C>
		void swap_erase(C& arr, typename C::size_type idx) {
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
		struct is_smaller_or_equal {
			constexpr bool operator()(const T& lhs, const T& rhs) const {
				return lhs <= rhs;
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
				const auto& pivot = arr[(uint32_t)high];
				int i = low - 1;
				for (int j = low; j <= high - 1; ++j) {
					if (func(arr[(uint32_t)j], pivot))
						core::swap(arr[(uint32_t)++i], arr[(uint32_t)j]);
				}
				core::swap(arr[(uint32_t)++i], arr[(uint32_t)high]);
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
				GAIA_ASSERT2(false, "sort currently supports at most 32 items in the array");
				// const int n = (int)arr.size();
				// detail::quick_sort(arr, 0, n - 1);
			}
		}
	} // namespace core
} // namespace gaia

/*** End of inlined file: utility.h ***/

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

				const uint32_t mask = ~(MaxValue << idxBit);
				m_data[idxByte] = (uint8_t)(((uint32_t)m_data[idxByte] & mask) | ((uint32_t)value << idxBit));

				const bool overlaps = idxBit + BlockBits > 8;
				if (overlaps) {
					// Value spans over two bytes
					const uint32_t shift2 = 8U - idxBit;
					const uint32_t mask2 = ~(MaxValue >> shift2);
					m_data[idxByte + 1] = (uint8_t)(((uint32_t)m_data[idxByte + 1] & mask2) | ((uint32_t)value >> shift2));
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
					const uint32_t shift2 = uint8_t(8U - idxBit);
					const uint32_t mask2 = MaxValue >> shift2;
					const uint8_t byte2 = uint8_t(((uint32_t)m_data[idxByte + 1] & mask2) << shift2);
					return byte1 | byte2;
				}

				return byte1;
			}
		};

		template <typename T>
		inline auto swap_bits(T& mask, uint32_t left, uint32_t right) {
			// Swap the bits in the read-write mask
			const uint32_t b0 = (mask >> left) & 1U;
			const uint32_t b1 = (mask >> right) & 1U;
			// XOR the two bits
			const uint32_t bxor = b0 ^ b1;
			// Put the XOR bits back to their original positions
			const uint32_t m = (bxor << left) | (bxor << right);
			// XOR mask with the original one effectivelly swapping the bits
			mask = mask ^ (uint8_t)m;
		}
	} // namespace core
} // namespace gaia
/*** End of inlined file: bit_utils.h ***/


/*** Start of inlined file: hashing_policy.h ***/
#pragma once

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
			uint64_t length = 0;
			while (str[length] != '\0')
				++length;

			return detail::murmur2a::hash_murmur2a_64_ct(str, length, detail::murmur2a::seed_64_const);
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

/*** End of inlined file: hashing_policy.h ***/


/*** Start of inlined file: reflection.h ***/
#pragma once

#include <tuple>
#include <type_traits>
#include <utility>

namespace gaia {
	namespace meta {

		GAIA_MSVC_WARNING_PUSH()
		// Fix for MSVC bug:
		// ...
		// template <DataLayout TDataLayout, typename ValueType>
		// struct data_view_policy_soa {
		//		static_assert(std::is_copy_assignable_v<ValueType>);
		//		using TTuple = decltype(meta::struct_to_tuple(std::declval<ValueType>())); << unreachable code
		// ...
		GAIA_MSVC_WARNING_DISABLE(4702) // unreachable code

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
			return {std::get<Is>(GAIA_FWD(tup))...};
		}

		template <typename S, typename Tuple>
		GAIA_NODISCARD S tuple_to_struct(Tuple&& tup) {
			using T = std::remove_reference_t<Tuple>;

			return tuple_to_struct<S>(std::make_index_sequence<std::tuple_size<T>{}>{}, GAIA_FWD(tup));
		}

		//----------------------------------------------------------------------
		// Struct to tuple conversion
		//----------------------------------------------------------------------

		//! The number of bits necessary to fit the maximum supported number of members in a struct
		static constexpr uint32_t StructToTupleMaxTypes_Bits = 4;
		static constexpr uint32_t StructToTupleMaxTypes = (1 << StructToTupleMaxTypes_Bits) - 1;

		//! Converts a struct to a tuple. The struct must support brace-initialization
		template <typename T>
		auto struct_to_tuple(T&& object) noexcept {
			using type = typename core::raw_t<T>;

			if constexpr (std::is_empty_v<type>) {
				return std::make_tuple();
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15] = GAIA_FWD(object);
				return std::make_tuple(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14] = GAIA_FWD(object);
				return std::make_tuple(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13] = GAIA_FWD(object);
				return std::make_tuple(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12] = GAIA_FWD(object);
				return std::make_tuple(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11] = GAIA_FWD(object);
				return std::make_tuple(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9, p10] = GAIA_FWD(object);
				return std::make_tuple(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9] = GAIA_FWD(object);
				return std::make_tuple(p1, p2, p3, p4, p5, p6, p7, p8, p9);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type, detail::any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6, p7, p8] = GAIA_FWD(object);
				return std::make_tuple(p1, p2, p3, p4, p5, p6, p7, p8);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type, detail::any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6, p7] = GAIA_FWD(object);
				return std::make_tuple(p1, p2, p3, p4, p5, p6, p7);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type, detail::any_type>{}) {
				auto&& [p1, p2, p3, p4, p5, p6] = GAIA_FWD(object);
				return std::make_tuple(p1, p2, p3, p4, p5, p6);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type,
															 detail::any_type>{}) {
				auto&& [p1, p2, p3, p4, p5] = GAIA_FWD(object);
				return std::make_tuple(p1, p2, p3, p4, p5);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type, detail::any_type>{}) {
				auto&& [p1, p2, p3, p4] = GAIA_FWD(object);
				return std::make_tuple(p1, p2, p3, p4);
			} else if constexpr (detail::is_braces_constructible_t<
															 type, detail::any_type, detail::any_type, detail::any_type>{}) {
				auto&& [p1, p2, p3] = GAIA_FWD(object);
				return std::make_tuple(p1, p2, p3);
			} else if constexpr (detail::is_braces_constructible_t<type, detail::any_type, detail::any_type>{}) {
				auto&& [p1, p2] = GAIA_FWD(object);
				return std::make_tuple(p1, p2);
			} else if constexpr (detail::is_braces_constructible_t<type, detail::any_type>{}) {
				auto&& [p1] = GAIA_FWD(object);
				return std::make_tuple(p1);
			}

			GAIA_ASSERT2(false, "Unsupported number of members");
		}

		//----------------------------------------------------------------------
		// Struct to tuple conversion
		//----------------------------------------------------------------------

		template <typename T>
		auto struct_member_count() {
			using type = core::raw_t<T>;

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

			GAIA_ASSERT2(false, "Unsupported number of members");
		}

		template <typename T, typename Func>
		auto each_member(T&& object, Func&& visitor) {
			using type = core::raw_t<T>;

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

			GAIA_ASSERT2(false, "Unsupported number of members");
		}

		GAIA_MSVC_WARNING_POP() // C4702
	} // namespace meta
} // namespace gaia

/*** End of inlined file: reflection.h ***/


/*** Start of inlined file: type_info.h ***/
#pragma once

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
						return (size_t)i;
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

/*** End of inlined file: type_info.h ***/


/*** Start of inlined file: data_layout_policy.h ***/
#pragma once

#include <tuple>
#include <type_traits>
#include <utility>


/*** Start of inlined file: mem_alloc.h ***/
#pragma once

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

/*** End of inlined file: mem_alloc.h ***/

namespace gaia {
	namespace mem {
		enum class DataLayout : uint32_t {
			AoS, //< Array Of Structures
			SoA, //< Structure Of Arrays, 4 packed items, good for SSE and similar
			SoA8, //< Structure Of Arrays, 8 packed items, good for AVX and similar
			SoA16, //< Structure Of Arrays, 16 packed items, good for AVX512 and similar

			Count = 4
		};

#ifndef GAIA_LAYOUT
	#define GAIA_LAYOUT(layout_name) static constexpr auto gaia_Data_Layout = ::gaia::mem::DataLayout::layout_name
#endif

		// Helper templates
		namespace detail {
			//! Returns the alignment for a given type \tparam T
			template <typename T>
			inline constexpr uint32_t get_alignment() {
				if constexpr (std::is_empty_v<T>)
					// Always consider 0 for empty types
					return 0;
				else {
					// Use at least 4 (32-bit systems) or 8 (64-bit systems) bytes for alignment
					constexpr auto s = (uint32_t)sizeof(uintptr_t);
					return core::get_min(s, (uint32_t)alignof(T));
				}
			}

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
		} // namespace detail

		template <DataLayout TDataLayout, typename TItem>
		struct data_layout_properties;
		template <typename TItem>
		struct data_layout_properties<DataLayout::AoS, TItem> {
			constexpr static DataLayout Layout = DataLayout::AoS;
			constexpr static size_t PackSize = 1;
			constexpr static size_t Alignment = detail::get_alignment<TItem>();
		};
		template <typename TItem>
		struct data_layout_properties<DataLayout::SoA, TItem> {
			constexpr static DataLayout Layout = DataLayout::SoA;
			constexpr static size_t PackSize = 4;
			constexpr static size_t Alignment = PackSize * 4;
		};
		template <typename TItem>
		struct data_layout_properties<DataLayout::SoA8, TItem> {
			constexpr static DataLayout Layout = DataLayout::SoA8;
			constexpr static size_t PackSize = 8;
			constexpr static size_t Alignment = PackSize * 4;
		};
		template <typename TItem>
		struct data_layout_properties<DataLayout::SoA16, TItem> {
			constexpr static DataLayout Layout = DataLayout::SoA16;
			constexpr static size_t PackSize = 16;
			constexpr static size_t Alignment = PackSize * 4;
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

		//! View policy for accessing and storing data in the AoS way.
		//! Good for random access and when accessing data that needs to be
		//! close together.
		//!
		//! struct Foo {
		//!   int x;
		//!   int y;
		//!   int z;
		//! };
		//! using fooViewPolicy = data_view_policy<DataLayout::AoS, Foo>;
		//!
		//! Memory organized as: xyz xyz xyz xyz
		template <typename ValueType>
		struct data_view_policy_aos {
			using TargetCastType = std::add_pointer_t<ValueType>;

			constexpr static DataLayout Layout = data_layout_properties<DataLayout::AoS, ValueType>::Layout;
			constexpr static size_t Alignment = data_layout_properties<DataLayout::AoS, ValueType>::Alignment;

			GAIA_NODISCARD static constexpr uint32_t get_min_byte_size(uintptr_t addr, size_t cnt) noexcept {
				const auto offset = detail::get_aligned_byte_offset<ValueType, Alignment>(addr, cnt);
				return (uint32_t)(offset - addr);
			}

			template <typename Allocator>
			GAIA_NODISCARD static uint8_t* alloc(size_t cnt) noexcept {
				const auto bytes = get_min_byte_size(0, cnt);
				auto* pData = (ValueType*)mem::AllocHelper::alloc<uint8_t, Allocator>(bytes);
				core::call_ctor_raw_n(pData, cnt);
				return (uint8_t*)pData;
			}

			template <typename Allocator>
			static void free(void* pData, size_t cnt) noexcept {
				if (pData == nullptr)
					return;
				core::call_dtor_n((ValueType*)pData, cnt);
				return mem::AllocHelper::free<Allocator>(pData);
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
				GAIA_ASSERT(idx < m_data.size());
				return ((const ValueType*)m_data.data())[idx];
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
				GAIA_ASSERT(idx < m_data.size());
				return ((ValueType*)m_data.data())[idx];
			}

			GAIA_NODISCARD const ValueType& operator[](size_t idx) const noexcept {
				GAIA_ASSERT(idx < m_data.size());
				return ((const ValueType*)m_data.data())[idx];
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

		//! View policy for accessing and storing data in the SoA way.
		//! Good for SIMD processing.
		//!
		//! struct Foo {
		//!   int x;
		//!   int y;
		//!   int z;
		//! };
		//! using fooViewPolicy = data_view_policy<DataLayout::SoA, Foo>;
		//!
		//! Memory organized as: xxxx yyyy zzzz
		template <DataLayout TDataLayout, typename ValueType>
		struct data_view_policy_soa {
			static_assert(std::is_copy_assignable_v<ValueType>);

			using TTuple = decltype(meta::struct_to_tuple(std::declval<ValueType>()));
			using TargetCastType = uint8_t*;

			constexpr static DataLayout Layout = data_layout_properties<TDataLayout, ValueType>::Layout;
			constexpr static size_t Alignment = data_layout_properties<TDataLayout, ValueType>::Alignment;
			constexpr static size_t TTupleItems = std::tuple_size<TTuple>::value;
			static_assert(Alignment > 0U, "SoA data can't be zero-aligned");
			static_assert(sizeof(ValueType) > 0U, "SoA data can't be zero-size");

			template <size_t Item>
			using value_type = typename std::tuple_element<Item, TTuple>::type;
			template <size_t Item>
			using const_value_type = typename std::add_const<value_type<Item>>::type;

			GAIA_NODISCARD constexpr static uint32_t get_min_byte_size(uintptr_t addr, size_t cnt) noexcept {
				const auto offset = get_aligned_byte_offset<TTupleItems>(addr, cnt);
				return (uint32_t)(offset - addr);
			}

			template <typename Allocator>
			GAIA_NODISCARD static uint8_t* alloc(size_t cnt) noexcept {
				const auto bytes = get_min_byte_size(0, cnt);
				return mem::AllocHelper::alloc_alig<uint8_t, Allocator>(Alignment, bytes);
			}

			template <typename Allocator>
			static void free(void* pData, [[maybe_unused]] size_t cnt) noexcept {
				if (pData == nullptr)
					return;
				return mem::AllocHelper::free_alig<Allocator>(pData);
			}

			GAIA_NODISCARD constexpr static ValueType get(std::span<const uint8_t> s, size_t idx) noexcept {
				return get_inter(meta::struct_to_tuple(ValueType{}), s, idx, std::make_index_sequence<TTupleItems>());
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
				constexpr accessor(std::span<uint8_t> data, size_t idx): m_data(data), m_idx(idx) {}

				constexpr void operator=(const ValueType& val) noexcept {
					set_inter(meta::struct_to_tuple(val), m_data, m_idx, std::make_index_sequence<TTupleItems>());
				}

				constexpr void operator=(ValueType&& val) noexcept {
					set_inter(meta::struct_to_tuple(GAIA_MOV(val)), m_data, m_idx, std::make_index_sequence<TTupleItems>());
				}

				GAIA_NODISCARD constexpr operator ValueType() const noexcept {
					return get_inter(
							meta::struct_to_tuple(ValueType{}), {(const uint8_t*)m_data.data(), m_data.size()}, m_idx,
							std::make_index_sequence<TTupleItems>());
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

			template <typename Tup, size_t... Ids>
			GAIA_NODISCARD constexpr static ValueType
			get_inter(Tup&& t, std::span<const uint8_t> s, size_t idx, std::index_sequence<Ids...> /*no_name*/) noexcept {
				auto address = mem::align<Alignment>((uintptr_t)s.data());
				((
						 // Put the value at the address into our tuple. Data is aligned so we can read directly.
						 std::get<Ids>(t) = get_ref<value_type<Ids>>((const uint8_t*)address, idx),
						 // Skip towards the next element and make sure the address is aligned properly
						 address = mem::align<Alignment>(address + sizeof(value_type<Ids>) * s.size())),
				 ...);
				return meta::tuple_to_struct<ValueType, TTuple>(GAIA_FWD(t));
			}

			template <typename Tup, size_t... Ids>
			constexpr static void
			set_inter(Tup&& t, std::span<uint8_t> s, size_t idx, std::index_sequence<Ids...> /*no_name*/) noexcept {
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

			GAIA_NODISCARD constexpr decltype(auto) operator[](size_t idx) const noexcept {
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
					view_policy::set(m_data, m_idx) = GAIA_FWD(val);
				}
			};

			GAIA_NODISCARD constexpr decltype(auto) operator[](size_t idx) const noexcept {
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
			struct auto_view_policy_inter<T, std::void_t<decltype(T::gaia_Data_Layout)>> {
				static constexpr DataLayout data_layout_type = T::gaia_Data_Layout;
			};

			template <typename, typename = void>
			struct is_soa_layout: std::false_type {};
			template <typename T>
			struct is_soa_layout<T, std::void_t<decltype(T::gaia_Data_Layout)>>:
					std::bool_constant<!std::is_empty_v<T> && (T::gaia_Data_Layout != DataLayout::AoS)> {};
		} // namespace detail

		template <typename T>
		using auto_view_policy = data_view_policy<detail::auto_view_policy_inter<T>::data_layout_type, T>;
		template <typename T>
		using auto_view_policy_get = data_view_policy_get<detail::auto_view_policy_inter<T>::data_layout_type, T>;
		template <typename T>
		using auto_view_policy_set = data_view_policy_set<detail::auto_view_policy_inter<T>::data_layout_type, T>;

		template <typename T>
		inline constexpr bool is_soa_layout_v = detail::is_soa_layout<T>::value;

	} // namespace mem
} // namespace gaia

/*** End of inlined file: data_layout_policy.h ***/


/*** Start of inlined file: mem_sani.h ***/
#pragma once

#ifndef GAIA_USE_MEM_SANI
	#if defined(__has_feature)
		#if __has_feature(address_sanitizer) || defined(USE_SANITIZER) || defined(_SANITIZE_ADDRESS__)
			#define GAIA_USE_MEM_SANI 1
		#else
			#define GAIA_USE_MEM_SANI 0
		#endif
	#else
		#if defined(USE_SANITIZER) || defined(_SANITIZE_ADDRESS__)
			#define GAIA_USE_MEM_SANI 1
		#else
			#define GAIA_USE_MEM_SANI 0
		#endif
	#endif
#endif

#if GAIA_USE_MEM_SANI
	#ifndef __SANITIZE_ADDRESS__
		#define __SANITIZE_ADDRESS__
	#endif
	#include <sanitizer/asan_interface.h>

	// Poison a new contiguous block of memory
	#define GAIA_MEM_SANI_ADD_BLOCK(type, ptr, cap, size)                                                                \
		if (ptr != nullptr)                                                                                                \
		__sanitizer_annotate_contiguous_container(                                                                         \
				ptr, (unsigned char*)(ptr) + (cap) * sizeof(type), (unsigned char*)(ptr) + (cap) * sizeof(type),               \
				(unsigned char*)(ptr) + (size) * sizeof(type))
	// Unpoison an existing contiguous block of buffer
	#define GAIA_MEM_SANI_DEL_BLOCK(type, ptr, cap, size)                                                                \
		if (ptr != nullptr)                                                                                                \
		__sanitizer_annotate_contiguous_container(                                                                         \
				ptr, (unsigned char*)(ptr) + (cap) * sizeof(type), (unsigned char*)(ptr) + (size) * sizeof(type),              \
				(unsigned char*)(ptr) + (cap) * sizeof(type))

	// Unpoison memory for N new elements, use before adding the elements
	#define GAIA_MEM_SANI_PUSH_N(type, ptr, cap, size, diff)                                                             \
		if (ptr != nullptr)                                                                                                \
			__sanitizer_annotate_contiguous_container(                                                                       \
					ptr, (unsigned char*)(ptr) + (cap) * sizeof(type), (unsigned char*)(ptr) + (size) * sizeof(type),            \
					(unsigned char*)(ptr) + ((size) + (diff)) * sizeof(type));
	// Poison memory for last N elements, use after removing the elements
	#define GAIA_MEM_SANI_POP_N(type, ptr, cap, size, diff)                                                              \
		if (ptr != nullptr)                                                                                                \
			__sanitizer_annotate_contiguous_container(                                                                       \
					ptr, (unsigned char*)(ptr) + (cap) * sizeof(type), (unsigned char*)(ptr) + ((size) + (diff)) * sizeof(type), \
					(unsigned char*)(ptr) + (size) * sizeof(type));

	// Unpoison memory for a new element, use before adding it
	#define GAIA_MEM_SANI_PUSH(type, ptr, cap, size) GAIA_MEM_SANI_PUSH_N(type, ptr, cap, size, 1)
	// Poison memory for the last elements, use after removing it
	#define GAIA_MEM_SANI_POP(type, ptr, cap, size) GAIA_MEM_SANI_POP_N(type, ptr, cap, size, 1)

#else
	#define GAIA_MEM_SANI_ADD_BLOCK(type, ptr, cap, size) ((void)(ptr), (void)(cap), (void)(size))
	#define GAIA_MEM_SANI_DEL_BLOCK(type, ptr, cap, size) ((void)(ptr), (void)(cap), (void)(size))
	#define GAIA_MEM_SANI_PUSH_N(type, ptr, cap, size, diff) ((void)(ptr), (void)(cap), (void)(size), (void)(diff))
	#define GAIA_MEM_SANI_POP_N(type, ptr, cap, size, diff) ((void)(ptr), (void)(cap), (void)(size), (void)(diff))
	#define GAIA_MEM_SANI_PUSH(type, ptr, cap, size) ((void)(ptr), (void)(cap), (void)(size))
	#define GAIA_MEM_SANI_POP(type, ptr, cap, size) ((void)(ptr), (void)(cap), (void)(size))
#endif

/*** End of inlined file: mem_sani.h ***/


/*** Start of inlined file: mem_utils.h ***/
#pragma once

#include <cstdint>
#include <type_traits>
#include <utility>

namespace gaia {
	namespace mem {
		template <typename T>
		inline constexpr bool is_copyable() {
			return std::is_trivially_copyable_v<T> || std::is_trivially_assignable_v<T, T> || //
						 std::is_copy_assignable_v<T> || std::is_copy_constructible_v<T>;
		}

		template <typename T>
		inline constexpr bool is_movable() {
			return std::is_trivially_move_assignable_v<T> || std::is_trivially_move_constructible_v<T> || //
						 std::is_move_assignable_v<T> || std::is_move_constructible_v<T>;
		}

		namespace detail {
			template <typename T>
			void copy_ctor_element_aos(T* GAIA_RESTRICT dst, const T* GAIA_RESTRICT src, uint32_t idxDst, uint32_t idxSrc) {
				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(6385)

				static_assert(!mem::is_soa_layout_v<T>);

				if constexpr (std::is_copy_assignable_v<T>) {
					dst[idxDst] = src[idxSrc];
				} else if constexpr (std::is_copy_constructible_v<T>) {
					dst[idxDst] = T(src[idxSrc]);
				} else {
					// Fallback to raw memory copy
					memmove((void*)dst[idxDst], (const void*)dst[idxSrc], sizeof(T));
				}

				GAIA_MSVC_WARNING_POP()
			}

			template <typename T>
			void copy_element_aos(T* GAIA_RESTRICT dst, const T* GAIA_RESTRICT src, uint32_t idxDst, uint32_t idxSrc) {
				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(6385)

				static_assert(!mem::is_soa_layout_v<T>);

				if constexpr (std::is_copy_assignable_v<T>) {
					dst[idxDst] = src[idxSrc];
				} else if constexpr (std::is_copy_constructible_v<T>) {
					dst[idxDst] = T(src[idxSrc]);
				} else {
					// Fallback to raw memory copy
					memmove((void*)dst[idxDst], (const void*)dst[idxSrc], sizeof(T));
				}

				GAIA_MSVC_WARNING_POP()
			}

			template <typename T>
			void copy_elements_aos(T* GAIA_RESTRICT dst, const T* GAIA_RESTRICT src, uint32_t idxDst, uint32_t idxSrc) {
				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(6385)

				static_assert(!mem::is_soa_layout_v<T>);

				GAIA_ASSERT(idxSrc < idxDst);

				if constexpr (std::is_copy_assignable_v<T>) {
					GAIA_FOR2(idxSrc, idxDst) dst[i] = src[i];
				} else if constexpr (std::is_copy_constructible_v<T>) {
					GAIA_FOR2(idxSrc, idxDst) dst[i] = T(src[i]);
				} else {
					// Fallback to raw memory copy
					GAIA_FOR2(idxSrc, idxDst) memmove((void*)dst[i], (const void*)dst[i], sizeof(T));
				}

				GAIA_MSVC_WARNING_POP()
			}

			template <typename T>
			void copy_element_soa(
					uint8_t* GAIA_RESTRICT dst, const uint8_t* GAIA_RESTRICT src, uint32_t idxDst, uint32_t idxSrc,
					uint32_t sizeDst, uint32_t sizeSrc) {
				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(6385)

				static_assert(mem::is_soa_layout_v<T>);

				(data_view_policy_set<T::gaia_Data_Layout, T>({std::span{dst, sizeDst}}))[idxDst] =
						(data_view_policy_get<T::gaia_Data_Layout, T>({std::span{(const uint8_t*)src, sizeSrc}}))[idxSrc];

				GAIA_MSVC_WARNING_POP()
			}

			template <typename T>
			void copy_elements_soa(
					uint8_t* GAIA_RESTRICT dst, const uint8_t* GAIA_RESTRICT src, uint32_t idxDst, uint32_t idxSrc,
					uint32_t sizeDst, uint32_t sizeSrc) {
				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(6385)

				static_assert(mem::is_soa_layout_v<T>);

				GAIA_ASSERT(idxSrc < idxDst);

				GAIA_FOR2(idxSrc, idxDst) {
					(data_view_policy_set<T::gaia_Data_Layout, T>({std::span{dst, sizeDst}}))[i] =
							(data_view_policy_get<T::gaia_Data_Layout, T>({std::span{(const uint8_t*)src, sizeSrc}}))[i];
				}

				GAIA_MSVC_WARNING_POP()
			}

			template <typename T>
			void move_ctor_element_aos(T* GAIA_RESTRICT dst, const T* GAIA_RESTRICT src, uint32_t idxDst, uint32_t idxSrc) {
				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(6385)

				static_assert(!mem::is_soa_layout_v<T>);

				if constexpr (!std::is_trivially_move_assignable_v<T> && std::is_move_assignable_v<T>) {
					core::call_ctor(&dst[idxDst]);
					dst[idxDst] = GAIA_MOV(src[idxSrc]);
				} else if constexpr (!std::is_trivially_move_constructible_v<T> && std::is_move_constructible_v<T>) {
					core::call_ctor(&dst[idxDst], T(GAIA_MOV(src[idxSrc])));
				} else {
					// Fallback to raw memory copy
					memmove((void*)&dst[idxDst], (const void*)&src[idxSrc], sizeof(T));
				}

				GAIA_MSVC_WARNING_POP()
			}

			template <typename T>
			void move_element_aos(T* GAIA_RESTRICT dst, T* GAIA_RESTRICT src, uint32_t idxDst, uint32_t idxSrc) {
				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(6385)

				static_assert(!mem::is_soa_layout_v<T>);

				if constexpr (!std::is_trivially_move_assignable_v<T> && std::is_move_assignable_v<T>) {
					dst[idxDst] = GAIA_MOV(src[idxSrc]);
				} else if constexpr (!std::is_trivially_move_constructible_v<T> && std::is_move_constructible_v<T>) {
					dst[idxDst] = T(GAIA_MOV(src[idxSrc]));
				} else {
					// Fallback to raw memory copy
					memmove((void*)&dst[idxDst], (const void*)&src[idxSrc], sizeof(T));
				}

				GAIA_MSVC_WARNING_POP()
			}

			template <typename T>
			void move_elements_aos(T* GAIA_RESTRICT dst, T* GAIA_RESTRICT src, uint32_t idxDst, uint32_t idxSrc) {
				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(6385)

				static_assert(!mem::is_soa_layout_v<T>);

				GAIA_ASSERT(idxSrc < idxDst);

				if constexpr (!std::is_trivially_move_assignable_v<T> && std::is_move_assignable_v<T>) {
					GAIA_FOR2(idxSrc, idxDst) dst[i] = GAIA_MOV(src[i]);
				} else if constexpr (!std::is_trivially_move_constructible_v<T> && std::is_move_constructible_v<T>) {
					GAIA_FOR2(idxSrc, idxDst) dst[i] = T(GAIA_MOV(src[i]));
				} else {
					// Fallback to raw memory copy
					GAIA_FOR2(idxSrc, idxDst) memmove((void*)&dst[i], (const void*)&src[i], sizeof(T));
				}

				GAIA_MSVC_WARNING_POP()
			}

			//! Shift elements at the address pointed to by \param dst to the left by one
			template <typename T>
			void shift_elements_left_aos(T* dst, uint32_t idxDst, uint32_t idxSrc, uint32_t n) {
				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(6385)

				static_assert(!mem::is_soa_layout_v<T>);

				GAIA_ASSERT(idxSrc < idxDst);

				// Move first if possible
				if constexpr (!std::is_trivially_move_assignable_v<T> && std::is_move_assignable_v<T>) {
					GAIA_FOR2(idxSrc, idxDst) dst[i] = GAIA_MOV(dst[i + n]);
				} else if constexpr (!std::is_trivially_move_constructible_v<T> && std::is_move_constructible_v<T>) {
					GAIA_FOR2(idxSrc, idxDst) dst[i] = T(GAIA_MOV(dst[i + n]));
				}
				// Try to copy if moves are not possible
				else if constexpr (std::is_copy_assignable_v<T>) {
					GAIA_FOR2(idxSrc, idxDst) dst[i] = dst[i + n];
				} else if constexpr (std::is_copy_constructible_v<T>) {
					GAIA_FOR2(idxSrc, idxDst) dst[i] = T(dst[i + n]);
				} else {
					// Fallback to raw memory copy
					GAIA_FOR2(idxSrc, idxDst) memmove((void*)&dst[i], (const void*)&dst[i + n], sizeof(T));
				}

				GAIA_MSVC_WARNING_POP()
			}

			//! Shift elements at the address pointed to by \param dst to the left by one
			template <typename T>
			void shift_elements_left_aos_n(T* dst, uint32_t idxDst, uint32_t idxSrc, uint32_t n) {
				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(6385)

				static_assert(!mem::is_soa_layout_v<T>);

				GAIA_ASSERT(idxSrc < idxDst);

				const auto max = idxDst - idxSrc - n;
				// Move first if possible
				if constexpr (!std::is_trivially_move_assignable_v<T> && std::is_move_assignable_v<T>) {
					GAIA_FOR(max) dst[idxSrc + i] = GAIA_MOV(dst[idxSrc + i + n]);
				} else if constexpr (!std::is_trivially_move_constructible_v<T> && std::is_move_constructible_v<T>) {
					GAIA_FOR(max) dst[idxSrc + i] = T(GAIA_MOV(dst[idxSrc + i + n]));
				}
				// Try to copy if moves are not possible
				else if constexpr (std::is_copy_assignable_v<T>) {
					GAIA_FOR(max) dst[idxSrc + i] = dst[idxSrc + i + n];
				} else if constexpr (std::is_copy_constructible_v<T>) {
					GAIA_FOR(max) dst[idxSrc + i] = T(dst[idxSrc + i + n]);
				} else {
					// Fallback to raw memory copy
					GAIA_FOR(max) memmove((void*)&dst[idxSrc + i], (const void*)&dst[idxSrc + i + n], sizeof(T));
				}

				GAIA_MSVC_WARNING_POP()
			}

			//! Shift elements at the address pointed to by \param dst to the left by one
			template <typename T>
			void shift_elements_left_soa(uint8_t* dst, uint32_t idxDst, uint32_t idxSrc, uint32_t n, uint32_t size) {
				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(6385)

				static_assert(mem::is_soa_layout_v<T>);

				GAIA_ASSERT(idxSrc < idxDst);

				GAIA_FOR2(idxSrc, idxDst) {
					(data_view_policy_set<T::gaia_Data_Layout, T>({std::span<uint8_t>{dst, size}}))[i] =
							(data_view_policy_get<T::gaia_Data_Layout, T>(
									{std::span<const uint8_t>{(const uint8_t*)dst, size}}))[i + n];
				}

				GAIA_MSVC_WARNING_POP()
			}

			//! Shift elements at the address pointed to by \param dst to the right by one
			template <typename T>
			void shift_elements_right_aos(T* dst, uint32_t idxDst, uint32_t idxSrc, uint32_t n) {
				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(6385)

				static_assert(!mem::is_soa_layout_v<T>);

				GAIA_ASSERT(idxSrc < idxDst);

				const auto max = idxDst - idxSrc + 1;
				// Move first if possible
				if constexpr (!std::is_trivially_move_assignable_v<T> && std::is_move_assignable_v<T>) {
					GAIA_FOR(max) dst[idxDst - i + n] = GAIA_MOV(dst[idxDst - i]);
				} else if constexpr (!std::is_trivially_move_constructible_v<T> && std::is_move_constructible_v<T>) {
					GAIA_FOR(max) dst[idxDst - i + n] = T(GAIA_MOV(dst[idxDst - i]));
				}
				// Try to copy if moves are not possible
				else if constexpr (std::is_copy_assignable_v<T>) {
					GAIA_FOR(max) dst[idxDst - i + n] = dst[idxDst - i];
				} else if constexpr (std::is_copy_constructible_v<T>) {
					GAIA_FOR(max) dst[idxDst - i + n] = T(dst[idxDst - i]);
				} else {
					// Fallback to raw memory copy
					GAIA_FOR(max) memmove((void*)&dst[idxDst - i + n], (const void*)&dst[idxDst - i], sizeof(T));
				}

				GAIA_MSVC_WARNING_POP()
			}

			//! Shift elements at the address pointed to by \param dst to the right by one
			template <typename T>
			void shift_elements_right_aos_n(T* dst, uint32_t idxDst, uint32_t idxSrc, uint32_t n) {
				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(6385)

				static_assert(!mem::is_soa_layout_v<T>);

				GAIA_ASSERT(idxSrc < idxDst);

				const auto max = idxDst - idxSrc + 1;
				// Move first if possible
				if constexpr (!std::is_trivially_move_assignable_v<T> && std::is_move_assignable_v<T>) {
					GAIA_FOR(max) dst[idxDst - i + n] = GAIA_MOV(dst[idxDst - i]);
				} else if constexpr (!std::is_trivially_move_constructible_v<T> && std::is_move_constructible_v<T>) {
					GAIA_FOR(max) dst[idxDst - i + n] = T(GAIA_MOV(dst[idxDst - i]));
				}
				// Try to copy if moves are not possible
				else if constexpr (std::is_copy_assignable_v<T>) {
					GAIA_FOR(max) dst[idxDst - i + n] = dst[idxDst - i];
				} else if constexpr (std::is_copy_constructible_v<T>) {
					GAIA_FOR(max) dst[idxDst - i + n] = T(dst[idxDst - i]);
				} else {
					// Fallback to raw memory copy
					GAIA_FOR(max) memmove((void*)&dst[idxDst - i + n], (const void*)&dst[idxDst - i], sizeof(T));
				}

				GAIA_MSVC_WARNING_POP()
			}

			//! Shift elements at the address pointed to by \param dst to the right by one
			template <typename T>
			void shift_elements_right_soa(uint8_t* dst, uint32_t idxDst, uint32_t idxSrc, uint32_t n, uint32_t size) {
				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(6385)

				static_assert(mem::is_soa_layout_v<T>);

				GAIA_ASSERT(idxSrc < idxDst);

				GAIA_FOR2(idxSrc, idxDst) {
					(data_view_policy_set<T::gaia_Data_Layout, T>({std::span<uint8_t>{dst, size}}))[i + n] =
							(data_view_policy_get<T::gaia_Data_Layout, T>({std::span<const uint8_t>{(const uint8_t*)dst, size}}))[i];
				}

				GAIA_MSVC_WARNING_POP()
			}
		} // namespace detail

		GAIA_CLANG_WARNING_PUSH()
		// Memory is aligned so we can silence this warning
		GAIA_CLANG_WARNING_DISABLE("-Wcast-align")

		//! Copy \param size elements of type \tparam T from the address pointer to by \param src to \param dst
		template <typename T>
		void copy_ctor_element(
				uint8_t* GAIA_RESTRICT dst, const uint8_t* GAIA_RESTRICT src, uint32_t idxDst, uint32_t idxSrc,
				[[maybe_unused]] uint32_t sizeDst, [[maybe_unused]] uint32_t sizeSrc) {
			if GAIA_UNLIKELY (src == dst && idxSrc == idxDst)
				return;

			if constexpr (!mem::is_soa_layout_v<T>)
				detail::copy_ctor_element_aos<T>((T*)dst, (const T*)src, idxDst, idxSrc);
			else
				detail::copy_element_soa<T>(dst, src, idxDst, idxSrc, sizeDst, sizeSrc);
		}

		//! Copy \param size elements of type \tparam T from the address pointer to by \param src to \param dst
		template <typename T>
		void copy_element(
				uint8_t* GAIA_RESTRICT dst, const uint8_t* GAIA_RESTRICT src, uint32_t idxDst, uint32_t idxSrc,
				[[maybe_unused]] uint32_t sizeDst, [[maybe_unused]] uint32_t sizeSrc) {
			if GAIA_UNLIKELY (src == dst && idxSrc == idxDst)
				return;

			if constexpr (!mem::is_soa_layout_v<T>)
				detail::copy_element_aos<T>((T*)dst, (const T*)src, idxDst, idxSrc);
			else
				detail::copy_element_soa<T>(dst, src, idxDst, idxSrc, sizeDst, sizeSrc);
		}

		//! Copy \param size elements of type \tparam T from the address pointer to by \param src to \param dst
		template <typename T>
		void copy_elements(
				uint8_t* GAIA_RESTRICT dst, const uint8_t* GAIA_RESTRICT src, uint32_t idxDst, uint32_t idxSrc,
				[[maybe_unused]] uint32_t sizeDst, [[maybe_unused]] uint32_t sizeSrc) {
			GAIA_ASSERT(idxSrc <= idxDst);
			if GAIA_UNLIKELY (idxSrc == idxDst)
				return;

			if constexpr (!mem::is_soa_layout_v<T>)
				detail::copy_elements_aos<T>((T*)dst, (const T*)src, idxDst, idxSrc);
			else
				detail::copy_elements_soa<T>(dst, src, idxDst, idxSrc, sizeDst, sizeSrc);
		}

		//! Move or copy \param cnt elements of type \tparam T from the address pointer to by \param src to \param dst
		template <typename T>
		void move_ctor_element(
				uint8_t* GAIA_RESTRICT dst, const uint8_t* GAIA_RESTRICT src, uint32_t idxDst, uint32_t idxSrc,
				[[maybe_unused]] uint32_t sizeDst, [[maybe_unused]] uint32_t sizeSrc) {
			if GAIA_UNLIKELY (src == dst && idxSrc == idxDst)
				return;

			if constexpr (!mem::is_soa_layout_v<T>) {
				if constexpr (is_movable<T>())
					detail::move_ctor_element_aos<T>((T*)dst, (T*)src, idxDst, idxSrc);
				else
					detail::copy_ctor_element_aos<T>((T*)dst, (const T*)src, idxDst, idxSrc);
			} else
				detail::copy_element_soa<T>(dst, src, idxDst, idxSrc, sizeDst, sizeSrc);
		}

		//! Move or copy \param cnt elements of type \tparam T from the address pointer to by \param src to \param dst
		template <typename T>
		void move_element(
				uint8_t* GAIA_RESTRICT dst, uint8_t* GAIA_RESTRICT src, uint32_t idxDst, uint32_t idxSrc,
				[[maybe_unused]] uint32_t sizeDst, [[maybe_unused]] uint32_t sizeSrc) {
			if GAIA_UNLIKELY (src == dst && idxSrc == idxDst)
				return;

			if constexpr (!mem::is_soa_layout_v<T>) {
				if constexpr (is_movable<T>())
					detail::move_element_aos<T>((T*)dst, (T*)src, idxDst, idxSrc);
				else
					detail::copy_element_aos<T>((T*)dst, (const T*)src, idxDst, idxSrc);
			} else
				detail::copy_element_soa<T>(dst, src, idxDst, idxSrc, sizeDst, sizeSrc);
		}

		//! Move or copy \param cnt elements of type \tparam T from the address pointer to by \param src to \param dst
		template <typename T>
		void move_elements(
				uint8_t* GAIA_RESTRICT dst, uint8_t* GAIA_RESTRICT src, uint32_t idxDst, uint32_t idxSrc,
				[[maybe_unused]] uint32_t sizeDst, [[maybe_unused]] uint32_t sizeSrc) {
			GAIA_ASSERT(idxSrc <= idxDst);
			if GAIA_UNLIKELY (idxSrc == idxDst)
				return;

			if constexpr (!mem::is_soa_layout_v<T>) {
				if constexpr (is_movable<T>())
					detail::move_elements_aos<T>((T*)dst, (T*)src, idxDst, idxSrc);
				else
					detail::copy_elements_aos<T>((T*)dst, (const T*)src, idxDst, idxSrc);
			} else
				detail::copy_elements_soa<T>(dst, src, idxDst, idxSrc, sizeDst, sizeSrc);
		}

		//! Move or copy \param cnt elements of type \tparam T from the address pointer to by \param src to \param dst
		template <typename T>
		void swap_elements(
				uint8_t* GAIA_RESTRICT dst, uint8_t* GAIA_RESTRICT src, uint32_t idxDst, uint32_t idxSrc,
				[[maybe_unused]] uint32_t sizeDst, [[maybe_unused]] uint32_t sizeSrc) {
			if GAIA_UNLIKELY (src == dst && idxSrc == idxDst)
				return;

			if constexpr (!mem::is_soa_layout_v<T>) {
				if constexpr (is_movable<T>()) {
					auto* l = (T*)src;
					auto* r = (T*)dst;
					T tmp;
					detail::move_element_aos<T>(&tmp, l, 0, idxSrc);
					detail::move_element_aos<T>(l, r, idxSrc, idxDst);
					detail::move_element_aos<T>(r, &tmp, idxDst, 0);
				} else {
					auto* l = (T*)src;
					auto* r = (T*)dst;
					T tmp;
					detail::copy_element_aos<T>(&tmp, l, 0, idxSrc);
					detail::copy_element_aos<T>(l, r, idxSrc, idxDst);
					detail::copy_element_aos<T>(r, &tmp, idxDst, 0);
				}
			} else {
				T tmp = mem::data_view_policy_get<T::gaia_Data_Layout, T>{std::span{(const uint8_t*)src, sizeSrc}}[idxSrc];
				detail::copy_element_soa<T>(src, dst, idxSrc, idxDst, sizeSrc, sizeDst);
				mem::data_view_policy_set<T::gaia_Data_Layout, T>{std::span{(const uint8_t*)dst, sizeDst}}[idxDst] = tmp;
			}
		}

		//! Shift elements at the address pointed to by \param dst to the left by one
		template <typename T>
		void shift_elements_left(uint8_t* dst, uint32_t idxDst, uint32_t idxSrc, [[maybe_unused]] uint32_t size) {
			GAIA_ASSERT(idxSrc <= idxDst);
			if GAIA_UNLIKELY (idxSrc == idxDst)
				return;

			if constexpr (mem::is_soa_layout_v<T>)
				detail::shift_elements_left_soa<T>(*dst, idxDst, idxSrc, 1, size);
			else
				detail::shift_elements_left_aos<T>((T*)dst, idxDst, idxSrc, 1);
		}

		//! Shift elements at the address pointed to by \param dst to the left by one
		template <typename T>
		void
		shift_elements_left_n(uint8_t* dst, uint32_t idxDst, uint32_t idxSrc, uint32_t n, [[maybe_unused]] uint32_t size) {
			GAIA_ASSERT(idxSrc <= idxDst);
			if GAIA_UNLIKELY (idxSrc == idxDst)
				return;

			if constexpr (mem::is_soa_layout_v<T>)
				detail::shift_elements_left_soa<T>(*dst, idxDst, idxSrc, n, size);
			else
				detail::shift_elements_left_aos_n<T>((T*)dst, idxDst, idxSrc, n);
		}

		//! Shift elements at the address pointed to by \param dst to the right by one
		template <typename T>
		void shift_elements_right(uint8_t* dst, uint32_t idxDst, uint32_t idxSrc, [[maybe_unused]] uint32_t size) {
			GAIA_ASSERT(idxSrc <= idxDst);
			if GAIA_UNLIKELY (idxSrc == idxDst)
				return;

			if constexpr (mem::is_soa_layout_v<T>)
				detail::shift_elements_right_soa<T>(*dst, idxDst, idxSrc, 1, size);
			else
				detail::shift_elements_right_aos<T>((T*)dst, idxDst, idxSrc, 1);
		}

		//! Shift elements at the address pointed to by \param dst to the right by one
		template <typename T>
		void
		shift_elements_right_n(uint8_t* dst, uint32_t idxDst, uint32_t idxSrc, uint32_t n, [[maybe_unused]] uint32_t size) {
			GAIA_ASSERT(idxSrc <= idxDst);
			if GAIA_UNLIKELY (idxSrc == idxDst)
				return;

			if constexpr (mem::is_soa_layout_v<T>)
				detail::shift_elements_right_soa<T>(*dst, idxDst, idxSrc, n, size);
			else
				detail::shift_elements_right_aos_n<T>((T*)dst, idxDst, idxSrc, n);
		}

		GAIA_CLANG_WARNING_POP()
	} // namespace mem
} // namespace gaia

/*** End of inlined file: mem_utils.h ***/


/*** Start of inlined file: raw_data_holder.h ***/
#pragma once

#include <cinttypes>

namespace gaia {
	namespace mem {
		namespace detail {
			template <uint32_t Size, uint32_t Alignment>
			struct raw_data_holder {
				static_assert(Size > 0);

				GAIA_ALIGNAS(Alignment) uint8_t data[Size];

				constexpr operator uint8_t*() noexcept {
					return &data[0];
				}

				constexpr operator const uint8_t*() const noexcept {
					return &data[0];
				}
			};

			template <uint32_t Size>
			struct raw_data_holder<Size, 0> {
				static_assert(Size > 0);

				uint8_t data[Size];

				constexpr operator uint8_t*() noexcept {
					return &data[0];
				}

				constexpr operator const uint8_t*() const noexcept {
					return &data[0];
				}
			};
		} // namespace detail

		template <typename T, uint32_t N>
		using raw_data_holder = detail::raw_data_holder<N, auto_view_policy<T>::Alignment>;
	} // namespace mem
} // namespace gaia
/*** End of inlined file: raw_data_holder.h ***/


/*** Start of inlined file: stack_allocator.h ***/
#pragma once

#include <cinttypes>

namespace gaia {
	namespace mem {
		namespace detail {
			struct AllocationInfo {
				//! Byte offset of the previous allocation
				uint32_t prev;
				//! Offset of data area from info area in bytes
				uint32_t off : 8;
				//! The number of requested bytes to allocate
				uint32_t cnt : 24;
				void (*dtor)(void*, uint32_t);
			};
		} // namespace detail

		// MSVC might warn about applying additional padding to an instance of StackAllocator.
		// This is perfectly fine, but might make builds with warning-as-error turned on to fail.
		GAIA_MSVC_WARNING_PUSH()
		GAIA_MSVC_WARNING_DISABLE(4324)

		//! Stack allocator capable of instantiating any default-constructible object on stack.
		//! Every allocation comes with a 16-bytes long sentinel object.
		template <uint32_t CapacityInBytes = 1024>
		class StackAllocator {
			using alloc_info = detail::AllocationInfo;

			//! Internal stack buffer aligned to 16B boundary
			detail::raw_data_holder<CapacityInBytes, 16> m_buffer;
			//! Current byte offset
			uint32_t m_pos = 0;
			//! Byte offset of the previous allocation
			uint32_t m_posPrev = 0;
			//! Number of allocations made
			uint32_t m_allocs = 0;

		public:
			StackAllocator() {
				// Aligned used so the sentinel object can be stored properly
				const auto bufferMemAddr = (uintptr_t)((uint8_t*)m_buffer);
				m_posPrev = m_pos = padding<alignof(alloc_info)>(bufferMemAddr);
			}

			~StackAllocator() {
				reset();
			}

			StackAllocator(const StackAllocator&) = delete;
			StackAllocator(StackAllocator&&) = delete;
			StackAllocator& operator=(const StackAllocator&) = delete;
			StackAllocator& operator=(StackAllocator&&) = delete;

			//! Allocates \param cnt objects of type \tparam T inside the buffer.
			//! No default initialization is done so the object is returned in a non-initialized
			//! state unless a custom constructor is provided.
			//! \return Pointer to the first allocated object
			template <typename T>
			GAIA_NODISCARD T* alloc(uint32_t cnt) {
				constexpr auto sizeT = (uint32_t)sizeof(T);
				const auto addrBuff = (uintptr_t)((uint8_t*)m_buffer);
				const auto addrAllocInfo = align<alignof(alloc_info)>(addrBuff + m_pos);
				const auto addrAllocData = align<alignof(T)>(addrAllocInfo + sizeof(alloc_info));
				const auto off = (uint32_t)(addrAllocData - addrAllocInfo);

				// There has to be some space left in the buffer
				const bool isFull = (uint32_t)(addrAllocData - addrBuff) + sizeT * cnt >= CapacityInBytes;
				if GAIA_UNLIKELY (isFull) {
					GAIA_ASSERT(!isFull && "Allocation space exceeded on StackAllocator");
					return nullptr;
				}

				// Memory sentinel
				auto* pInfo = (alloc_info*)addrAllocInfo;
				pInfo->prev = m_posPrev;
				pInfo->off = off;
				pInfo->cnt = cnt;
				pInfo->dtor = [](void* ptr, uint32_t cnt) {
					core::call_dtor_n((T*)ptr, cnt);
				};

				// Constructing the object is necessary
				auto* pData = (T*)addrAllocData;
				core::call_ctor_raw_n(pData, cnt);

				// Allocation start offset
				m_posPrev = (uint32_t)(addrAllocInfo - addrBuff);
				// Point to the next free space (not necessary aligned yet)
				m_pos = m_posPrev + pInfo->off + sizeT * cnt;

				++m_allocs;
				return pData;
			}

			//! Frees the last allocated object from the stack.
			//! \param pData Pointer to the last allocated object on the stack
			//! \param cnt Number of objects that were allocated on the given memory address
			void free([[maybe_unused]] void* pData, [[maybe_unused]] uint32_t cnt) {
				GAIA_ASSERT(pData != nullptr);
				GAIA_ASSERT(cnt > 0);
				GAIA_ASSERT(m_allocs > 0);

				const auto addrBuff = (uintptr_t)((uint8_t*)m_buffer);

				// Destroy the last allocated object
				const auto addrAllocInfo = addrBuff + m_posPrev;
				auto* pInfo = (alloc_info*)addrAllocInfo;
				const auto addrAllocData = addrAllocInfo + pInfo->off;
				void* pInfoData = (void*)addrAllocData;
				GAIA_ASSERT(pData == pInfoData);
				GAIA_ASSERT(pInfo->cnt == cnt);
				pInfo->dtor(pInfoData, pInfo->cnt);

				m_pos = m_posPrev;
				m_posPrev = pInfo->prev;
				--m_allocs;
			}

			//! Frees all allocated objects from the buffer
			void reset() {
				const auto addrBuff = (uintptr_t)((uint8_t*)m_buffer);

				// Destroy allocated objects back-to-front
				auto pos = m_posPrev;
				while (m_allocs > 0) {
					const auto addrAllocInfo = addrBuff + pos;
					auto* pInfo = (alloc_info*)addrAllocInfo;
					const auto addrAllocData = addrAllocInfo + pInfo->off;
					pInfo->dtor((void*)addrAllocData, pInfo->cnt);
					pos = pInfo->prev;

					--m_allocs;
				}

				GAIA_ASSERT(m_allocs == 0);

				m_pos = 0;
				m_posPrev = 0;
				m_allocs = 0;
			}

			GAIA_NODISCARD constexpr uint32_t capacity() {
				return CapacityInBytes;
			}
		};

		GAIA_MSVC_WARNING_POP()
	} // namespace mem
} // namespace gaia
/*** End of inlined file: stack_allocator.h ***/


/*** Start of inlined file: bitset.h ***/
#pragma once

#include <cstdint>
#include <type_traits>


/*** Start of inlined file: bitset_iterator.h ***/
#pragma once

#include <cstdint>
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
/*** End of inlined file: bitset_iterator.h ***/

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

		public:
			static constexpr uint32_t BitsPerItem = (NBits / 64) > 0 ? 64 : 32;
			static constexpr uint32_t Items = (NBits + BitsPerItem - 1) / BitsPerItem;

			using size_type = typename size_type_selector<BitsPerItem == 32>::type;

		private:
			static constexpr bool HasTrailingBits = (NBits % BitsPerItem) != 0;
			static constexpr size_type LastItemMask = ((size_type)1 << (NBits % BitsPerItem)) - 1;

			size_type m_data[Items]{};

			//! Returns the word stored at the index \param wordIdx
			size_type data(uint32_t wordIdx) const {
				return m_data[wordIdx];
			}

		public:
			using const_iterator = bitset_const_iterator<bitset<NBits>, false>;
			friend const_iterator;
			using const_iterator_inverse = bitset_const_iterator<bitset<NBits>, true>;
			friend const_iterator_inverse;

			size_type* data() {
				return &m_data[0];
			}

			const size_type* data() const {
				return &m_data[0];
			}

			//! Returns the number of words used by the bitset internally
			GAIA_NODISCARD constexpr uint32_t items() const {
				return Items;
			}

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
				GAIA_FOR(Items) {
					if (m_data[i] != other.m_data[i])
						return false;
				}
				return true;
			}

			GAIA_NODISCARD constexpr bool operator!=(const bitset& other) const {
				GAIA_FOR(Items) {
					if (m_data[i] == other.m_data[i])
						return false;
				}
				return true;
			}

			//! Sets all bits
			constexpr void set() {
				if constexpr (HasTrailingBits) {
					GAIA_FOR(Items - 1) m_data[i] = (size_type)-1;
					m_data[Items - 1] = LastItemMask;
				} else {
					GAIA_FOR(Items) m_data[i] = (size_type)-1;
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
					GAIA_FOR(Items - 1) m_data[i] = ~m_data[i];
					m_data[Items - 1] = (~m_data[Items - 1]) & LastItemMask;
				} else {
					GAIA_FOR(Items) m_data[i] = ~m_data[i];
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
					GAIA_FOR2(wordIdxFrom + 1, wordIdxTo) m_data[i] = ~m_data[i];
					// Last word
					m_data[wordIdxTo] ^= getMask(0, bitTo % BitsPerItem);
				}

				return *this;
			}

			//! Unsets all bits
			constexpr void reset() {
				GAIA_FOR(Items) m_data[i] = 0;
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
					GAIA_FOR(Items - 1) {
						if (m_data[i] != (size_type)-1)
							return false;
					}
					return (m_data[Items - 1] & LastItemMask) == LastItemMask;
				} else {
					GAIA_FOR(Items) {
						if (m_data[i] != (size_type)-1)
							return false;
					}
					return true;
				}
			}

			//! Checks if any bit is set
			GAIA_NODISCARD constexpr bool any() const {
				GAIA_FOR(Items) {
					if (m_data[i] != 0)
						return true;
				}
				return false;
			}

			//! Checks if all bits are reset
			GAIA_NODISCARD constexpr bool none() const {
				GAIA_FOR(Items) {
					if (m_data[i] != 0)
						return false;
				}
				return true;
			}

			//! Returns the number of set bits
			GAIA_NODISCARD uint32_t count() const {
				uint32_t total = 0;

				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(4244)
				if constexpr (sizeof(size_type) == 4) {
					GAIA_FOR(Items) total += GAIA_POPCNT(m_data[i]);
				} else {
					GAIA_FOR(Items) total += GAIA_POPCNT64(m_data[i]);
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
/*** End of inlined file: bitset.h ***/


/*** Start of inlined file: darray.h ***/
#pragma once


/*** Start of inlined file: darray_impl.h ***/
#pragma once

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
				return mem::data_view_policy<T::gaia_Data_Layout, T>::get({m_ptr, m_cnt}, m_idx);
			}
			T operator->() const {
				return mem::data_view_policy<T::gaia_Data_Layout, T>::get({m_ptr, m_cnt}, m_idx);
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
		template <typename T, typename Allocator = mem::DefaultAllocatorAdaptor>
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
				if GAIA_LIKELY (cap != 0 && cnt < cap)
					return;

				// If no data is allocated go with at least 4 elements
				if GAIA_UNLIKELY (m_pData == nullptr) {
					m_pData = view_policy::template alloc<Allocator>(m_cap = 4);
					GAIA_MEM_SANI_ADD_BLOCK(value_type, m_pData, m_cap, cnt);
					return;
				}

				// We increase the capacity in multiples of 1.5 which is about the golden ratio (1.618).
				// This effectively means we prefer more frequent allocations over memory fragmentation.
				m_cap = (cap * 3 + 1) / 2;

				auto* pDataOld = m_pData;
				m_pData = view_policy::template alloc<Allocator>(m_cap);
				GAIA_MEM_SANI_ADD_BLOCK(value_type, m_pData, m_cap, cnt);
				mem::move_elements<T>(m_pData, pDataOld, cnt, 0, m_cap, cap);
				GAIA_MEM_SANI_DEL_BLOCK(value_type, pDataOld, cap, cnt);
				view_policy::template free<Allocator>(pDataOld, cnt);
			}

		public:
			constexpr darr() noexcept = default;

			darr(size_type count, const_reference value) {
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

			darr(darr&& other) noexcept {
				// This is a newly constructed object.
				// It can't have any memory allocated, yet.
				GAIA_ASSERT(m_pData == nullptr);

				m_pData = other.m_pData;
				m_cnt = other.m_cnt;
				m_cap = other.m_cap;

				other.m_cnt = size_type(0);
				other.m_cap = size_type(0);
				other.m_pData = nullptr;
			}

			darr& operator=(std::initializer_list<T> il) {
				*this = darr(il.begin(), il.end());
				return *this;
			}

			darr& operator=(const darr& other) {
				GAIA_ASSERT(core::addressof(other) != this);

				resize(other.size());
				mem::copy_elements<T>(
						(uint8_t*)m_pData, (const uint8_t*)other.m_pData, other.size(), 0, capacity(), other.capacity());

				return *this;
			}

			darr& operator=(darr&& other) noexcept {
				GAIA_ASSERT(core::addressof(other) != this);

				// Release previously allocated memory if there was anything
				GAIA_MEM_SANI_DEL_BLOCK(value_type, m_pData, m_cap, m_cnt);
				view_policy::template free<Allocator>(m_pData, m_cnt);

				m_pData = other.m_pData;
				m_cnt = other.m_cnt;
				m_cap = other.m_cap;

				other.m_pData = nullptr;
				other.m_cnt = size_type(0);
				other.m_cap = size_type(0);

				return *this;
			}

			~darr() {
				GAIA_MEM_SANI_DEL_BLOCK(value_type, m_pData, m_cap, m_cnt);
				view_policy::template free<Allocator>(m_pData, m_cnt);
			}

			GAIA_CLANG_WARNING_PUSH()
			// Memory is aligned so we can silence this warning
			GAIA_CLANG_WARNING_DISABLE("-Wcast-align")

			GAIA_NODISCARD pointer data() noexcept {
				return (pointer)m_pData;
			}

			GAIA_NODISCARD const_pointer data() const noexcept {
				return (const_pointer)m_pData;
			}

			GAIA_NODISCARD decltype(auto) operator[](size_type pos) noexcept {
				GAIA_ASSERT(pos < size());
				return view_policy::set({(typename view_policy::TargetCastType)m_pData, capacity()}, pos);
			}

			GAIA_NODISCARD decltype(auto) operator[](size_type pos) const noexcept {
				GAIA_ASSERT(pos < size());
				return view_policy::get({(typename view_policy::TargetCastType)m_pData, capacity()}, pos);
			}

			GAIA_CLANG_WARNING_POP()

			void reserve(size_type cap) {
				if (cap <= m_cap)
					return;

				auto* pDataOld = m_pData;
				m_pData = view_policy::template alloc<Allocator>(cap);
				GAIA_MEM_SANI_ADD_BLOCK(value_type, m_pData, cap, m_cnt);

				if (pDataOld != nullptr) {
					mem::move_elements<T>(m_pData, pDataOld, m_cnt, 0, cap, m_cap);
					GAIA_MEM_SANI_DEL_BLOCK(value_type, pDataOld, m_cap, m_cnt);
					view_policy::template free<Allocator>(pDataOld, m_cnt);
				}

				m_cap = cap;
			}

			void resize(size_type count) {
				// Fresh allocation
				if (m_pData == nullptr) {
					if (count > 0) {
						m_pData = view_policy::template alloc<Allocator>(count);
						GAIA_MEM_SANI_ADD_BLOCK(value_type, m_pData, count, count);
						m_cap = count;
						m_cnt = count;
					}
					return;
				}

				// Resizing to a smaller size
				if (count <= m_cnt) {
					// Destroy elements at the end
					if constexpr (!mem::is_soa_layout_v<T>) {
						core::call_dtor_n(&data()[count], m_cnt - count);
						GAIA_MEM_SANI_POP_N(value_type, m_pData, m_cap, m_cnt - count, count);
					}

					m_cnt = count;
					return;
				}

				// Resizing to a bigger size but still within allocated capacity
				if (count <= m_cap) {
					// Construct new elements
					if constexpr (!mem::is_soa_layout_v<T>) {
						GAIA_MEM_SANI_PUSH_N(value_type, m_pData, m_cap, m_cnt, count - m_cnt);
						core::call_ctor_n(&data()[m_cnt], count - m_cnt);
					}

					m_cnt = count;
					return;
				}

				auto* pDataOld = m_pData;
				m_pData = view_policy::template alloc<Allocator>(count);
				GAIA_MEM_SANI_ADD_BLOCK(value_type, m_pData, count, count);
				{
					// Move old data to the new location
					mem::move_elements<T>(m_pData, pDataOld, m_cnt, 0, count, m_cap);
					// Default-construct new items
					if constexpr (!mem::is_soa_layout_v<T>)
						core::call_ctor_n(&data()[m_cnt], count - m_cnt);
				}

				// Release old memory
				GAIA_MEM_SANI_DEL_BLOCK(value_type, pDataOld, m_cap, m_cnt);
				view_policy::template free<Allocator>(pDataOld, m_cnt);

				m_cap = count;
				m_cnt = count;
			}

			void push_back(const T& arg) {
				try_grow();

				if constexpr (mem::is_soa_layout_v<T>) {
					operator[](m_cnt++) = arg;
				} else {
					GAIA_MEM_SANI_PUSH(value_type, m_pData, m_cap, m_cnt);
					auto* ptr = &data()[m_cnt++];
					core::call_ctor(ptr, arg);
				}
			}

			void push_back(T&& arg) {
				try_grow();

				if constexpr (mem::is_soa_layout_v<T>) {
					operator[](m_cnt++) = GAIA_MOV(arg);
				} else {
					GAIA_MEM_SANI_PUSH(value_type, m_pData, m_cap, m_cnt);
					auto* ptr = &data()[m_cnt++];
					core::call_ctor(ptr, GAIA_MOV(arg));
				}
			}

			template <typename... Args>
			decltype(auto) emplace_back(Args&&... args) {
				try_grow();

				if constexpr (mem::is_soa_layout_v<T>) {
					operator[](m_cnt++) = T(GAIA_FWD(args)...);
					return;
				} else {
					GAIA_MEM_SANI_PUSH(value_type, m_pData, m_cap, m_cnt);
					auto* ptr = &data()[m_cnt++];
					core::call_ctor(ptr, GAIA_FWD(args)...);
					return (reference)*ptr;
				}
			}

			void pop_back() noexcept {
				GAIA_ASSERT(!empty());

				if constexpr (!mem::is_soa_layout_v<T>) {
					auto* ptr = &data()[m_cnt];
					core::call_dtor(ptr);
					GAIA_MEM_SANI_POP(value_type, m_pData, m_cap, m_cnt - 1);
				}

				--m_cnt;
			}

			//! Insert the element to the position given by iterator \param pos
			iterator insert(iterator pos, const T& arg) noexcept {
				GAIA_ASSERT(pos >= data());
				GAIA_ASSERT(empty() || (pos < iterator(data() + size())));

				try_grow();

				const auto idxSrc = (size_type)core::distance(begin(), pos);
				const auto idxDst = (size_type)core::distance(begin(), end()) + 1;

				mem::shift_elements_right<T>(m_pData, idxDst, idxSrc, m_cap);

				if constexpr (mem::is_soa_layout_v<T>) {
					operator[](idxSrc) = arg;
				} else {
					auto* ptr = &data()[m_cnt];
					core::call_ctor(ptr, arg);
					GAIA_MEM_SANI_PUSH(value_type, m_pData, m_cap, m_cnt);
				}

				++m_cnt;

				return iterator(&data()[idxSrc]);
			}

			//! Insert the element to the position given by iterator \param pos
			iterator insert(iterator pos, T&& arg) noexcept {
				GAIA_ASSERT(pos >= data());
				GAIA_ASSERT(empty() || (pos < iterator(data() + size())));

				try_grow();

				const auto idxSrc = (size_type)core::distance(begin(), pos);
				const auto idxDst = (size_type)core::distance(begin(), end());

				mem::shift_elements_right<T>(m_pData, idxDst, idxSrc, m_cap);

				if constexpr (mem::is_soa_layout_v<T>) {
					operator[](idxSrc) = GAIA_MOV(arg);
				} else {
					auto* ptr = &data()[idxSrc];
					core::call_ctor(ptr, GAIA_MOV(arg));
					GAIA_MEM_SANI_PUSH(value_type, m_pData, m_cap, m_cnt);
				}

				++m_cnt;

				return iterator(&data()[idxSrc]);
			}

			//! Removes the element at pos
			//! \param pos Iterator to the element to remove
			iterator erase(iterator pos) noexcept {
				GAIA_ASSERT(pos >= data());
				GAIA_ASSERT(empty() || (pos < iterator(data() + size())));

				if (empty())
					return end();

				const auto idxSrc = (size_type)core::distance(begin(), pos);
				const auto idxDst = (size_type)core::distance(begin(), end()) - 1;

				mem::shift_elements_left<T>(m_pData, idxDst, idxSrc, m_cap);
				// Destroy if it's the last element
				if constexpr (!mem::is_soa_layout_v<T>) {
					auto* ptr = &data()[m_cnt - 1];
					core::call_dtor(ptr);
					GAIA_MEM_SANI_POP(value_type, m_pData, m_cap, m_cnt - 1);
				}

				--m_cnt;

				return iterator(&data()[idxSrc]);
			}

			//! Removes the elements in the range [first, last)
			//! \param first Iterator to the element to remove
			//! \param last Iterator to the one beyond the last element to remove
			iterator erase(iterator first, iterator last) noexcept {
				GAIA_ASSERT(first >= data())
				GAIA_ASSERT(empty() || (first < iterator(data() + size())));
				GAIA_ASSERT(last > first);
				GAIA_ASSERT(last <= iterator(data() + size()));

				if (empty())
					return end();

				const auto idxSrc = (size_type)core::distance(begin(), first);
				const auto idxDst = size();
				const auto cnt = last - first;

				mem::shift_elements_left_n<T>(m_pData, idxDst, idxSrc, cnt, m_cap);
				// Destroy if it's the last element
				if constexpr (!mem::is_soa_layout_v<T>) {
					auto* ptr = &data()[m_cnt - cnt];
					core::call_dtor_n(ptr, cnt);
					GAIA_MEM_SANI_POP_N(value_type, data(), m_cap, m_cnt - cnt, cnt);
				}

				m_cnt -= cnt;

				return iterator(&data()[idxSrc]);
			}

			void clear() {
				resize(0);
			}

			void shrink_to_fit() {
				const auto cap = capacity();
				const auto cnt = size();

				if (cap == cnt)
					return;

				auto* pDataOld = m_pData;
				m_pData = view_policy::mem_alloc(m_cap = cnt);
				GAIA_MEM_SANI_ADD_BLOCK(value_type, m_pData, m_cap, m_cnt);
				mem::move_elements<T>(m_pData, pDataOld, cnt, 0);
				GAIA_MEM_SANI_DEL_BLOCK(value_type, pDataOld, cap, cnt);
				view_policy::mem_free(pDataOld);
			}

			//! Removes all elements that fail the predicate.
			//! \param func A lambda or a functor with the bool operator()(Container::value_type&) overload.
			//! \return The new size of the array.
			template <typename Func>
			auto retain(Func&& func) {
				size_type erased = 0;
				size_type idxDst = 0;
				size_type idxSrc = 0;

				while (idxSrc < m_cnt) {
					if (func(operator[](idxSrc))) {
						if (idxDst < idxSrc) {
							mem::move_element<T>(m_pData, m_pData, idxDst, idxSrc, m_cap, m_cap);
							auto* ptr = &data()[idxSrc];
							core::call_dtor(ptr);
						}
						++idxDst;
					} else {
						auto* ptr = &data()[idxSrc];
						core::call_dtor(ptr);
						++erased;
					}

					++idxSrc;
				}

				if constexpr (!mem::is_soa_layout_v<T>) {
					if (erased > 0)
						GAIA_MEM_SANI_POP_N(value_type, data(), m_cap, m_cnt - erased, erased);
				}

				m_cnt -= erased;
				return idxDst;
			}

			GAIA_NODISCARD size_type size() const noexcept {
				return m_cnt;
			}

			GAIA_NODISCARD bool empty() const noexcept {
				return size() == 0;
			}

			GAIA_NODISCARD size_type capacity() const noexcept {
				return m_cap;
			}

			GAIA_NODISCARD size_type max_size() const noexcept {
				return static_cast<size_type>(-1);
			}

			GAIA_NODISCARD decltype(auto) front() noexcept {
				GAIA_ASSERT(!empty());
				if constexpr (mem::is_soa_layout_v<T>)
					return *begin();
				else
					return (reference)*begin();
			}

			GAIA_NODISCARD decltype(auto) front() const noexcept {
				GAIA_ASSERT(!empty());
				if constexpr (mem::is_soa_layout_v<T>)
					return *begin();
				else
					return (const_reference)*begin();
			}

			GAIA_NODISCARD decltype(auto) back() noexcept {
				GAIA_ASSERT(!empty());
				if constexpr (mem::is_soa_layout_v<T>)
					return operator[](m_cnt - 1);
				else
					return (reference)(operator[](m_cnt - 1));
			}

			GAIA_NODISCARD decltype(auto) back() const noexcept {
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
					return iterator(data());
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
					return iterator(data() + size());
			}

			GAIA_NODISCARD auto rend() const noexcept {
				if constexpr (mem::is_soa_layout_v<T>)
					return iterator_soa(m_pData, size(), -1);
				else
					return iterator(data() - 1);
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

			GAIA_NODISCARD constexpr bool operator!=(const darr& other) const {
				return !operator==(other);
			}

			template <size_t Item>
			auto soa_view_mut() noexcept {
				return mem::data_view_policy<T::gaia_Data_Layout, T>::template get<Item>(
						std::span<uint8_t>{(uint8_t*)m_pData, capacity()});
			}

			template <size_t Item>
			auto soa_view() const noexcept {
				return mem::data_view_policy<T::gaia_Data_Layout, T>::template get<Item>(
						std::span<const uint8_t>{(const uint8_t*)m_pData, capacity()});
			}
		};
	} // namespace cnt

} // namespace gaia
/*** End of inlined file: darray_impl.h ***/

namespace gaia {
	namespace cnt {
		template <typename T>
		using darray = cnt::darr<T>;
	} // namespace cnt
} // namespace gaia

/*** End of inlined file: darray.h ***/


/*** Start of inlined file: darray_ext.h ***/
#pragma once


/*** Start of inlined file: darray_ext_impl.h ***/
#pragma once

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
				return mem::data_view_policy<T::gaia_Data_Layout, T>::get({m_ptr, m_cnt}, m_idx);
			}
			T operator->() const {
				return mem::data_view_policy<T::gaia_Data_Layout, T>::get({m_ptr, m_cnt}, m_idx);
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
		template <typename T, darr_ext_detail::size_type N, typename Allocator = mem::DefaultAllocatorAdaptor>
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
			uint8_t* m_pDataHeap = nullptr;
			//! Pointer to the currently used data
			uint8_t* m_pData = m_data;
			//! Number of currently used items ín this container
			size_type m_cnt = size_type(0);
			//! Allocated capacity of m_dataDyn or the extend
			size_type m_cap = extent;

			void try_grow() {
				const auto cnt = size();
				const auto cap = capacity();

				// Unless we reached the capacity don't do anything
				if GAIA_LIKELY (cnt < cap)
					return;

				// We increase the capacity in multiples of 1.5 which is about the golden ratio (1.618).
				// This means we prefer more frequent allocations over memory fragmentation.
				m_cap = (cap * 3 + 1) / 2;

				if GAIA_UNLIKELY (m_pDataHeap == nullptr) {
					// If no heap memory is allocated yet we need to allocate it and move the old stack elements to it
					m_pDataHeap = view_policy::template alloc<Allocator>(m_cap);
					GAIA_MEM_SANI_ADD_BLOCK(value_type, m_pDataHeap, m_cap, cnt);
					mem::move_elements<T>(m_pDataHeap, m_data, cnt, 0, m_cap, cap);
				} else {
					// Move items from the old heap array to the new one. Delete the old
					auto* pDataOld = m_pDataHeap;
					m_pDataHeap = view_policy::template alloc<Allocator>(m_cap);
					GAIA_MEM_SANI_ADD_BLOCK(value_type, m_pDataHeap, m_cap, cnt);
					mem::move_elements<T>(m_pDataHeap, pDataOld, cnt, 0, m_cap, cap);
					GAIA_MEM_SANI_DEL_BLOCK(value_type, pDataOld, cap, cnt);
					view_policy::template free<Allocator>(pDataOld, cnt);
				}

				m_pData = m_pDataHeap;
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

			darr_ext(darr_ext&& other) noexcept {
				GAIA_ASSERT(core::addressof(other) != this);

				if (other.m_pDataHeap != nullptr) {
					GAIA_ASSERT(m_pDataHeap == nullptr);
					m_pDataHeap = other.m_pDataHeap;
					m_pData = m_pDataHeap;
				} else {
					resize(other.size());
					mem::move_elements<T>(m_data, other.m_data, other.size(), 0, extent, other.extent);
					m_pDataHeap = nullptr;
					m_pData = m_data;
				}

				m_cnt = other.m_cnt;
				m_cap = other.m_cap;

				other.m_pDataHeap = nullptr;
				other.m_pData = other.m_data;
				other.m_cnt = size_type(0);
				other.m_cap = extent;
			}

			darr_ext& operator=(std::initializer_list<T> il) {
				*this = darr_ext(il.begin(), il.end());
				return *this;
			}

			darr_ext& operator=(const darr_ext& other) {
				GAIA_ASSERT(core::addressof(other) != this);

				resize(other.size());
				mem::copy_elements<T>(
						(uint8_t*)m_pData, (const uint8_t*)other.m_pData, other.size(), 0, capacity(), other.capacity());

				return *this;
			}

			darr_ext& operator=(darr_ext&& other) noexcept {
				GAIA_ASSERT(core::addressof(other) != this);

				// Moving from heap-allocated source
				if (other.m_pDataHeap != nullptr) {
					// Release current heap memory and replace it with the source
					GAIA_MEM_SANI_DEL_BLOCK(value_type, m_pDataHeap, m_cap, m_cnt);
					view_policy::template free<Allocator>(m_pDataHeap, m_cnt);

					m_pDataHeap = other.m_pDataHeap;
					m_pData = m_pDataHeap;
				} else
				// Moving from stack-allocated source
				{
					resize(other.size());
					mem::move_elements<T>(m_data, other.m_data, other.size(), 0, extent, other.extent);
					m_pDataHeap = nullptr;
					m_pData = m_data;
				}
				m_cnt = other.m_cnt;
				m_cap = other.m_cap;

				other.m_cnt = size_type(0);
				other.m_cap = extent;
				other.m_pDataHeap = nullptr;
				other.m_pData = other.m_data;

				return *this;
			}

			~darr_ext() {
				GAIA_MEM_SANI_DEL_BLOCK(value_type, m_pDataHeap, m_cap, m_cnt);
				view_policy::template free<Allocator>(m_pDataHeap, m_cnt);
			}

			GAIA_CLANG_WARNING_PUSH()
			// Memory is aligned so we can silence this warning
			GAIA_CLANG_WARNING_DISABLE("-Wcast-align")

			GAIA_NODISCARD pointer data() noexcept {
				return (pointer)m_pData;
			}

			GAIA_NODISCARD const_pointer data() const noexcept {
				return (const_pointer)m_pData;
			}

			GAIA_NODISCARD decltype(auto) operator[](size_type pos) noexcept {
				GAIA_ASSERT(pos < size());
				return view_policy::set({(typename view_policy::TargetCastType)m_pData, size()}, pos);
			}

			GAIA_NODISCARD decltype(auto) operator[](size_type pos) const noexcept {
				GAIA_ASSERT(pos < size());
				return view_policy::get({(typename view_policy::TargetCastType)m_pData, size()}, pos);
			}

			GAIA_CLANG_WARNING_POP()

			void reserve(size_type cap) {
				if (cap <= m_cap)
					return;

				if (m_pDataHeap) {
					auto* pDataOld = m_pDataHeap;
					m_pDataHeap = view_policy::template alloc<Allocator>(cap);
					GAIA_MEM_SANI_ADD_BLOCK(value_type, m_pDataHeap, cap, m_cnt);

					mem::move_elements<T>(m_pDataHeap, pDataOld, m_cnt, 0, cap, m_cap);
					GAIA_MEM_SANI_DEL_BLOCK(value_type, pDataOld, m_cap, m_cnt);
					view_policy::template free<Allocator>(pDataOld, m_cnt);
				} else {
					m_pDataHeap = view_policy::template alloc<Allocator>(cap);
					mem::move_elements<T>(m_pDataHeap, m_data, m_cnt, 0, cap, m_cap);
				}

				m_cap = cap;
				m_pData = m_pDataHeap;
			}

			void resize(size_type count) {
				// Resizing to a smaller size
				if (count <= m_cnt) {
					// Destroy elements at the endif (count <= m_cap) {
					if constexpr (!mem::is_soa_layout_v<T>) {
						core::call_dtor_n(&data()[count], m_cnt - count);
						GAIA_MEM_SANI_POP_N(value_type, data(), m_cap, m_cnt - count, count);
					}

					m_cnt = count;
					return;
				}

				// Resizing to a bigger size but still within allocated capacity
				if (count <= m_cap) {
					// Construct new elements
					if constexpr (!mem::is_soa_layout_v<T>) {
						GAIA_MEM_SANI_PUSH_N(value_type, data(), m_cap, m_cnt, count - m_cnt);
						core::call_ctor_n(&data()[m_cnt], count - m_cnt);
					}

					m_cnt = count;
					return;
				}

				auto* pDataOld = m_pDataHeap;
				m_pDataHeap = view_policy::template alloc<Allocator>(count);
				GAIA_MEM_SANI_ADD_BLOCK(value_type, m_pDataHeap, count, count);
				if (pDataOld != nullptr) {
					// Move old data to the new location
					mem::move_elements<T>(m_pDataHeap, pDataOld, m_cnt, 0, count, m_cap);

					// Default-construct new items
					if constexpr (!mem::is_soa_layout_v<T>)
						core::call_ctor_n(&data()[m_cnt], count - m_cnt);

					// Release old memory
					GAIA_MEM_SANI_DEL_BLOCK(value_type, pDataOld, m_cap, m_cnt);
					view_policy::template free<Allocator>(pDataOld, m_cnt);
				} else {
					mem::move_elements<T>(m_pDataHeap, m_data, m_cnt, 0, count, m_cap);
				}

				m_cap = count;
				m_cnt = count;
				m_pData = m_pDataHeap;
			}

			void push_back(const T& arg) {
				try_grow();

				if constexpr (mem::is_soa_layout_v<T>) {
					operator[](m_cnt++) = arg;
				} else {
					GAIA_MEM_SANI_PUSH(value_type, data(), m_cap, m_cnt);
					auto* ptr = &data()[m_cnt++];
					core::call_ctor(ptr, arg);
				}
			}

			void push_back(T&& arg) {
				try_grow();

				if constexpr (mem::is_soa_layout_v<T>) {
					operator[](m_cnt++) = GAIA_MOV(arg);
				} else {
					GAIA_MEM_SANI_PUSH(value_type, data(), m_cap, m_cnt);
					auto* ptr = &data()[m_cnt++];
					core::call_ctor(ptr, GAIA_MOV(arg));
				}
			}

			template <typename... Args>
			decltype(auto) emplace_back(Args&&... args) {
				try_grow();

				if constexpr (mem::is_soa_layout_v<T>) {
					operator[](m_cnt++) = T(GAIA_FWD(args)...);
					return;
				} else {
					GAIA_MEM_SANI_PUSH(value_type, data(), m_cap, m_cnt);
					auto* ptr = &data()[m_cnt++];
					core::call_ctor(ptr, GAIA_FWD(args)...);
					return (reference)*ptr;
				}
			}

			void pop_back() noexcept {
				GAIA_ASSERT(!empty());

				if constexpr (!mem::is_soa_layout_v<T>) {
					auto* ptr = &data()[m_cnt];
					core::call_dtor(ptr);
					GAIA_MEM_SANI_POP(value_type, data(), m_cap, m_cnt - 1);
				}

				--m_cnt;
			}

			//! Insert the element to the position given by iterator \param pos
			iterator insert(iterator pos, const T& arg) noexcept {
				GAIA_ASSERT(pos >= data());
				GAIA_ASSERT(empty() || (pos < iterator(data() + size())));

				try_grow();

				const auto idxSrc = (size_type)core::distance(begin(), pos);
				const auto idxDst = (size_type)core::distance(begin(), end()) + 1;

				mem::shift_elements_right<T>(m_pData, idxDst, idxSrc, m_cap);

				if constexpr (mem::is_soa_layout_v<T>) {
					operator[](idxSrc) = arg;
				} else {
					auto* ptr = &data()[idxSrc];
					core::call_ctor(ptr, arg);
					GAIA_MEM_SANI_PUSH(value_type, data(), m_cap, m_cnt);
				}

				++m_cnt;

				return iterator(&data()[idxSrc]);
			}

			//! Insert the element to the position given by iterator \param pos
			iterator insert(iterator pos, T&& arg) noexcept {
				GAIA_ASSERT(pos >= data());
				GAIA_ASSERT(empty() || (pos < iterator(data() + size())));

				try_grow();

				const auto idxSrc = (size_type)core::distance(begin(), pos);
				const auto idxDst = (size_type)core::distance(begin(), end());

				mem::shift_elements_right<T>(m_pData, idxDst, idxSrc, m_cap);

				if constexpr (mem::is_soa_layout_v<T>) {
					operator[](idxSrc) = GAIA_MOV(arg);
				} else {
					auto* ptr = &data()[idxSrc];
					core::call_ctor(ptr, GAIA_MOV(arg));
					GAIA_MEM_SANI_PUSH(value_type, data(), m_cap, m_cnt);
				}

				++m_cnt;

				return iterator(&data()[idxSrc]);
			}

			//! Removes the element at pos
			//! \param pos Iterator to the element to remove
			iterator erase(iterator pos) noexcept {
				GAIA_ASSERT(pos >= data());
				GAIA_ASSERT(empty() || (pos < iterator(data() + size())));

				if (empty())
					return end();

				const auto idxSrc = (size_type)core::distance(begin(), pos);
				const auto idxDst = (size_type)core::distance(begin(), end()) - 1;

				mem::shift_elements_left<T>(m_pData, idxDst, idxSrc, m_cap);
				// Destroy if it's the last element
				if constexpr (!mem::is_soa_layout_v<T>) {
					auto* ptr = &data()[m_cnt - 1];
					core::call_dtor(ptr);
					GAIA_MEM_SANI_POP(value_type, data(), m_cap, m_cnt - 1);
				}

				--m_cnt;

				return iterator(&data()[idxSrc]);
			}

			//! Removes the elements in the range [first, last)
			//! \param first Iterator to the element to remove
			//! \param last Iterator to the one beyond the last element to remove
			iterator erase(iterator first, iterator last) noexcept {
				GAIA_ASSERT(first >= data())
				GAIA_ASSERT(empty() || (first < iterator(data() + size())));
				GAIA_ASSERT(last > first);
				GAIA_ASSERT(last <= iterator(data() + size()));

				if (empty())
					return end();

				const auto idxSrc = (size_type)core::distance(begin(), first);
				const auto idxDst = size();
				const auto cnt = last - first;

				mem::shift_elements_left_n<T>(m_pData, idxDst, idxSrc, cnt, m_cap);
				// Destroy if it's the last element
				if constexpr (!mem::is_soa_layout_v<T>) {
					core::call_dtor_n(&data()[m_cnt - cnt], cnt);
					GAIA_MEM_SANI_POP_N(value_type, data(), m_cap, m_cnt - cnt, cnt);
				}

				m_cnt -= cnt;

				return iterator(&data()[idxSrc]);
			}

			void clear() {
				resize(0);
			}

			void shrink_to_fit() {
				const auto cap = capacity();
				const auto cnt = size();

				if (cap == cnt)
					return;

				if (m_pDataHeap != nullptr) {
					auto* pDataOld = m_pDataHeap;

					if (cnt < extent) {
						mem::move_elements<T>(m_data, pDataOld, cnt, 0);
						m_pData = m_data;
						m_cap = extent;
					} else {
						m_pDataHeap = view_policy::mem_alloc(m_cap = cnt);
						GAIA_MEM_SANI_ADD_BLOCK(value_type, m_pDataHeap, m_cap, m_cnt);
						mem::move_elements<T>(m_pDataHeap, pDataOld, cnt, 0);
						m_pData = m_pDataHeap;
					}

					GAIA_MEM_SANI_DEL_BLOCK(value_type, pDataOld, cap, cnt);
					view_policy::mem_free(pDataOld);
				} else
					resize(cnt);
			}

			//! Removes all elements that fail the predicate.
			//! \param func A lambda or a functor with the bool operator()(Container::value_type&) overload.
			//! \return The new size of the array.
			template <typename Func>
			auto retain(Func&& func) {
				size_type erased = 0;
				size_type idxDst = 0;
				size_type idxSrc = 0;

				while (idxSrc < m_cnt) {
					if (func(operator[](idxSrc))) {
						if (idxDst < idxSrc) {
							auto* ptr = (uint8_t*)data();
							mem::move_element<T>(ptr, ptr, idxDst, idxSrc, m_cap, m_cap);
							auto* ptr2 = &data()[idxSrc];
							core::call_dtor(ptr2);
						}
						++idxDst;
					} else {
						auto* ptr = &data()[idxSrc];
						core::call_dtor(ptr);
						++erased;
					}

					++idxSrc;
				}

				if constexpr (!mem::is_soa_layout_v<T>) {
					if (erased > 0)
						GAIA_MEM_SANI_POP_N(value_type, data(), m_cap, m_cnt - erased, erased);
				}

				m_cnt -= erased;
				return idxDst;
			}

			GAIA_NODISCARD size_type size() const noexcept {
				return m_cnt;
			}

			GAIA_NODISCARD bool empty() const noexcept {
				return size() == 0;
			}

			GAIA_NODISCARD size_type capacity() const noexcept {
				return m_cap;
			}

			GAIA_NODISCARD size_type max_size() const noexcept {
				return N;
			}

			GAIA_NODISCARD decltype(auto) front() noexcept {
				GAIA_ASSERT(!empty());
				if constexpr (mem::is_soa_layout_v<T>)
					return *begin();
				else
					return (reference)*begin();
			}

			GAIA_NODISCARD decltype(auto) front() const noexcept {
				GAIA_ASSERT(!empty());
				if constexpr (mem::is_soa_layout_v<T>)
					return *begin();
				else
					return (const_reference)*begin();
			}

			GAIA_NODISCARD decltype(auto) back() noexcept {
				GAIA_ASSERT(!empty());
				if constexpr (mem::is_soa_layout_v<T>)
					return operator[](m_cnt - 1);
				else
					return (reference) operator[](m_cnt - 1);
			}

			GAIA_NODISCARD decltype(auto) back() const noexcept {
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
					return iterator(data());
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
					return iterator(data() + size());
			}

			GAIA_NODISCARD auto rend() const noexcept {
				if constexpr (mem::is_soa_layout_v<T>)
					return iterator_soa(m_pData, size(), -1);
				else
					return iterator(data() - 1);
			}

			GAIA_NODISCARD bool operator==(const darr_ext& other) const {
				if (m_cnt != other.m_cnt)
					return false;
				const size_type n = size();
				for (size_type i = 0; i < n; ++i)
					if (!(operator[](i) == other[i]))
						return false;
				return true;
			}

			GAIA_NODISCARD constexpr bool operator!=(const darr_ext& other) const {
				return !operator==(other);
			}

			template <size_t Item>
			auto soa_view_mut() noexcept {
				return mem::data_view_policy<T::gaia_Data_Layout, T>::template get<Item>(
						std::span<uint8_t>{(uint8_t*)m_pData, capacity()});
			}

			template <size_t Item>
			auto soa_view() const noexcept {
				return mem::data_view_policy<T::gaia_Data_Layout, T>::template get<Item>(
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

/*** End of inlined file: darray_ext_impl.h ***/

namespace gaia {
	namespace cnt {
		template <typename T, darr_ext_detail::size_type N>
		using darray_ext = cnt::darr_ext<T, N>;
	} // namespace cnt
} // namespace gaia

/*** End of inlined file: darray_ext.h ***/


/*** Start of inlined file: dbitset.h ***/
#pragma once

#include <cstdint>
#include <type_traits>

namespace gaia {
	namespace cnt {
		template <typename Allocator = mem::DefaultAllocatorAdaptor>
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

			static constexpr uint32_t BitsPerItem = sizeof(typename size_type_selector::type) * 8;

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
				m_pData = mem::AllocHelper::alloc<size_type, Allocator>(itemsNew);

				if (pDataOld == nullptr) {
					// Make sure the new data is set to zeros
					GAIA_FOR2(itemsOld, itemsNew) m_pData[i] = 0;
				} else {
					// Copy the old data over and set the old data to zeros
					mem::copy_elements<size_type>((uint8_t*)m_pData, (const uint8_t*)pDataOld, itemsOld, 0, 0, 0);
					GAIA_FOR2(itemsOld, itemsNew) m_pData[i] = 0;

					// Release the old data
					mem::AllocHelper::free<Allocator>((void*)pDataOld);
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
				mem::AllocHelper::free<Allocator>((void*)m_pData);
			}

			dbitset(const dbitset& other) {
				*this = other;
			}

			dbitset& operator=(const dbitset& other) {
				GAIA_ASSERT(core::addressof(other) != this);

				resize(other.m_cnt);
				mem::copy_elements<size_type>((uint8_t*)m_pData, (const uint8_t*)other.m_pData, other.items(), 0, 0, 0);
				return *this;
			}

			dbitset(dbitset&& other) noexcept {
				*this = GAIA_MOV(other);
			}

			dbitset& operator=(dbitset&& other) noexcept {
				GAIA_ASSERT(core::addressof(other) != this);

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
				m_pData = mem::AllocHelper::alloc<size_type, Allocator>(itemsNew);

				if (pDataOld == nullptr) {
					// Make sure the new data is set to zeros
					GAIA_FOR(itemsNew) m_pData[i] = 0;
				} else {
					const uint32_t itemsOld = items();
					// Copy the old data over and set the old data to zeros
					mem::copy_elements<size_type>((uint8_t*)m_pData, (const uint8_t*)pDataOld, itemsOld, 0, 0, 0);
					GAIA_FOR2(itemsOld, itemsNew) m_pData[i] = 0;

					// Release old data
					mem::AllocHelper::free<Allocator>(pDataOld);
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
				m_pData = mem::AllocHelper::alloc<size_type, Allocator>(itemsNew);

				if (pDataOld == nullptr) {
					// Make sure the new data is set to zeros
					GAIA_FOR(itemsNew) m_pData[i] = 0;
				} else {
					// Copy the old data over and set the old data to zeros
					mem::copy_elements<size_type>((uint8_t*)m_pData, (const uint8_t*)pDataOld, itemsOld, 0, 0, 0);
					GAIA_FOR2(itemsOld, itemsNew) m_pData[i] = 0;

					// Release old data
					mem::AllocHelper::free<Allocator>((void*)pDataOld);
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
				GAIA_FOR(item_count) {
					if (m_pData[i] != other.m_pData[i])
						return false;
				}
				return true;
			}

			GAIA_NODISCARD bool operator!=(const dbitset& other) const {
				const uint32_t item_count = items();
				GAIA_FOR(item_count) {
					if (m_pData[i] == other.m_pData[i])
						return false;
				}
				return true;
			}

			//! Sets all bits
			void set() {
				if GAIA_UNLIKELY (size() == 0)
					return;

				const auto item_count = items();
				const auto lastItemMask = last_item_mask();

				if (lastItemMask != 0) {
					GAIA_FOR(item_count - 1) m_pData[i] = (size_type)-1;
					m_pData[item_count - 1] = lastItemMask;
				} else {
					GAIA_FOR(item_count) m_pData[i] = (size_type)-1;
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
					GAIA_FOR(item_count - 1) m_pData[i] = ~m_pData[i];
					m_pData[item_count - 1] = (~m_pData[item_count - 1]) & lastItemMask;
				} else {
					GAIA_FOR(item_count + 1) m_pData[i] = ~m_pData[i];
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
					GAIA_FOR2(wordIdxFrom + 1, wordIdxTo) m_pData[i] = ~m_pData[i];
					// Last word
					m_pData[wordIdxTo] ^= getMask(0, bitTo % BitsPerItem);
				}

				return *this;
			}

			//! Unsets all bits
			void reset() {
				const auto item_count = items();
				GAIA_FOR(item_count) m_pData[i] = 0;
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

				GAIA_FOR(item_count) {
					if (m_pData[i] != (size_type)-1)
						return false;
				}

				if (has_trailing_bits())
					return (m_pData[item_count] & lastItemMask) == lastItemMask;

				return m_pData[item_count] == (size_type)-1;
			}

			//! Checks if any bit is set
			GAIA_NODISCARD bool any() const {
				const auto item_count = items();
				GAIA_FOR(item_count) {
					if (m_pData[i] != 0)
						return true;
				}
				return false;
			}

			//! Checks if all bits are reset
			GAIA_NODISCARD bool none() const {
				const auto item_count = items();
				GAIA_FOR(item_count) {
					if (m_pData[i] != 0)
						return false;
				}
				return true;
			}

			//! Returns the number of set bits
			GAIA_NODISCARD uint32_t count() const {
				uint32_t total = 0;

				const auto item_count = items();

				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(4244)
				if constexpr (sizeof(size_type) == 4) {
					GAIA_FOR(item_count) total += GAIA_POPCNT(m_pData[i]);
				} else {
					GAIA_FOR(item_count) total += GAIA_POPCNT64(m_pData[i]);
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
/*** End of inlined file: dbitset.h ***/


/*** Start of inlined file: ilist.h ***/
#pragma once

#include <cstdint>
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

			GAIA_NODISCARD bool empty() const noexcept {
				return size() == 0;
			}

			GAIA_NODISCARD size_type capacity() const noexcept {
				return (size_type)m_items.capacity();
			}

			GAIA_NODISCARD iterator begin() const noexcept {
				return {(pointer)m_items.data()};
			}

			GAIA_NODISCARD iterator end() const noexcept {
				return {(pointer)m_items.data() + size()};
			}

			void reserve(size_type cap) {
				m_items.reserve(cap);
			}

			//! Allocates a new item in the list
			//! \return Handle to the new item
			GAIA_NODISCARD TItemHandle alloc(void* ctx) {
				if GAIA_UNLIKELY (m_freeItems == 0U) {
					// We don't want to go out of range for new item
					const auto itemCnt = (size_type)m_items.size();
					GAIA_ASSERT(itemCnt < TItemHandle::IdMask && "Trying to allocate too many items!");

					GAIA_GCC_WARNING_PUSH()
					GAIA_CLANG_WARNING_PUSH()
					GAIA_GCC_WARNING_DISABLE("-Wmissing-field-initializers");
					GAIA_CLANG_WARNING_DISABLE("-Wmissing-field-initializers");
					m_items.push_back(TListItem::create(itemCnt, 0U, ctx));
					return TListItem::handle(m_items.back());
					GAIA_GCC_WARNING_POP()
					GAIA_CLANG_WARNING_POP()
				}

				// Make sure the list is not broken
				GAIA_ASSERT(m_nextFreeIdx < (size_type)m_items.size() && "Item recycle list broken!");

				--m_freeItems;
				const auto index = m_nextFreeIdx;
				auto& j = m_items[m_nextFreeIdx];
				m_nextFreeIdx = j.idx;
				j = TListItem::create(index, j.gen, ctx);
				return TListItem::handle(j);
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
			//! Every time an item is deallocated its generation is increased by one.
			TListItem& free(TItemHandle handle) {
				auto& item = m_items[handle.id()];

				// Update our implicit list
				if GAIA_UNLIKELY (m_freeItems == 0)
					item.idx = TItemHandle::IdMask;
				else
					item.idx = m_nextFreeIdx;
				++item.gen;

				m_nextFreeIdx = handle.id();
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
/*** End of inlined file: ilist.h ***/


/*** Start of inlined file: map.h ***/
#pragma once


/*** Start of inlined file: robin_hood.h ***/
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
			throw E(GAIA_FWD(args)...);
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
				doThrow<E>(GAIA_FWD(args)...);
			}
			return t;
		}

		template <typename T>
		inline T unaligned_load(void const* ptr) noexcept {
			// using memcpy so we don't get into unaligned load problems.
			// compiler should optimize this very well anyways.
			T t;
			memcpy(&t, ptr, sizeof(T));
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
				using gaia::core::swap;
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
	// which means it would not be allowed to be used in memcpy. This struct is copyable, which is
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
				first(o.first), second(o.second) {}

		// pair constructors are explicit so we don't accidentally call this ctor when we don't have to.
		explicit constexpr pair(std::pair<T1, T2>&& o) noexcept(
				noexcept(T1(GAIA_MOV(std::declval<T1&&>()))) && noexcept(T2(GAIA_MOV(std::declval<T2&&>())))):
				first(GAIA_MOV(o.first)), second(GAIA_MOV(o.second)) {}

		constexpr pair(T1&& a, T2&& b) noexcept(
				noexcept(T1(GAIA_MOV(std::declval<T1&&>()))) && noexcept(T2(GAIA_MOV(std::declval<T2&&>())))):
				first(GAIA_MOV(a)), second(GAIA_MOV(b)) {}

		template <typename U1, typename U2>
		constexpr pair(U1&& a, U2&& b) noexcept(
				noexcept(T1(GAIA_FWD(std::declval<U1&&>()))) && noexcept(T2(GAIA_FWD(std::declval<U2&&>())))):
				first(GAIA_FWD(a)), second(GAIA_FWD(b)) {}

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
		pair(std::tuple<U1...>& a, std::tuple<U2...>& b, std::index_sequence<I1...> /*unused*/, std::index_sequence<I2...> /*unused*/) noexcept(
				noexcept(T1(std::forward<U1>(std::get<I1>(std::declval<std::tuple<U1...>&>()))...)) &&
				noexcept(T2(std::forward<U2>(std::get<I2>(std::declval<std::tuple<U2...>&>()))...))):
				first(GAIA_FWD(std::get<I1>(a))...), second(GAIA_FWD(std::get<I2>(b))...) {
			// make visual studio compiler happy about warning about unused a & b.
			// Visual studio's pair implementation disables warning 4100.
			(void)a;
			(void)b;
		}

		void
		swap(pair<T1, T2>& o) noexcept((detail::swappable::nothrow<T1>::value) && (detail::swappable::nothrow<T2>::value)) {
			using gaia::core::swap;
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
	inline constexpr bool operator<(pair<A, B> const& x, pair<A, B> const& y) noexcept(
			noexcept(std::declval<A const&>() < std::declval<A const&>()) &&
			noexcept(std::declval<B const&>() < std::declval<B const&>())) {
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
						noexcept(value_type(GAIA_FWD(args)...))): mData(GAIA_FWD(args)...) {}

				DataNode(M& ROBIN_HOOD_UNUSED(map) /*unused*/, DataNode<M, true>&& n) noexcept(
						std::is_nothrow_move_constructible<value_type>::value): mData(GAIA_MOV(n.mData)) {}

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
					::new (static_cast<void*>(mData)) value_type(GAIA_FWD(args)...);
				}

				DataNode(M& ROBIN_HOOD_UNUSED(map) /*unused*/, DataNode<M, false>&& n) noexcept: mData(GAIA_MOV(n.mData)) {}

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
					using gaia::core::swap;
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
					auto const* const src = reinterpret_cast<uint8_t const*>(source.mKeyVals);
					auto* tgt = reinterpret_cast<uint8_t*>(target.mKeyVals);
					auto const numElementsWithBuffer = target.calcNumElementsWithBuffer(target.mMask + 1);
					gaia::mem::copy_elements<uint8_t>(
							tgt, src, (uint32_t)target.calcNumBytesTotal(numElementsWithBuffer), 0, 0, 0);
				}
			};

			template <typename M>
			struct Cloner<M, false> {
				void operator()(M const& s, M& t) const {
					auto const numElementsWithBuffer = t.calcNumElementsWithBuffer(t.mMask + 1);
					gaia::mem::copy_elements<uint8_t>(
							t.mInfo, s.mInfo, (uint32_t)t.calcNumBytesInfo(numElementsWithBuffer), 0, 0, 0);

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
				::new (static_cast<void*>(mKeyVals + idx)) Node(GAIA_MOV(mKeyVals[idx - 1]));
				while (--idx != insertion_idx) {
					mKeyVals[idx] = GAIA_MOV(mKeyVals[idx - 1]);
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
					mKeyVals[idx] = GAIA_MOV(mKeyVals[idx + 1]);
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
					::new (static_cast<void*>(&l)) Node(GAIA_MOV(keyval));
				} else {
					shiftUp(idx, insertion_idx);
					l = GAIA_MOV(keyval);
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
					WHash(h), WKeyEqual(equal) {
				ROBIN_HOOD_TRACE(this);
				GAIA_ASSERT(gaia::CheckEndianess());
			}

			template <typename Iter>
			Table(
					Iter first, Iter last, size_t ROBIN_HOOD_UNUSED(bucket_count) /*unused*/ = 0, const Hash& h = Hash{},
					const KeyEqual& equal = KeyEqual{}): WHash(h), WKeyEqual(equal) {
				ROBIN_HOOD_TRACE(this);
				GAIA_ASSERT(gaia::CheckEndianess());
				insert(first, last);
			}

			Table(
					std::initializer_list<value_type> initlist, size_t ROBIN_HOOD_UNUSED(bucket_count) /*unused*/ = 0,
					const Hash& h = Hash{}, const KeyEqual& equal = KeyEqual{}): WHash(h), WKeyEqual(equal) {
				ROBIN_HOOD_TRACE(this);
				GAIA_ASSERT(gaia::CheckEndianess());
				insert(initlist.begin(), initlist.end());
			}

			Table(Table&& o) noexcept:
					WHash(GAIA_MOV(static_cast<WHash&>(o))), WKeyEqual(GAIA_MOV(static_cast<WKeyEqual&>(o))),
					DataPool(GAIA_MOV(static_cast<DataPool&>(o))) {
				ROBIN_HOOD_TRACE(this)
				if (o.mMask) {
					mHashMultiplier = GAIA_MOV(o.mHashMultiplier);
					mKeyVals = GAIA_MOV(o.mKeyVals);
					mInfo = GAIA_MOV(o.mInfo);
					mNumElements = GAIA_MOV(o.mNumElements);
					mMask = GAIA_MOV(o.mMask);
					mMaxNumElementsAllowed = GAIA_MOV(o.mMaxNumElementsAllowed);
					mInfoInc = GAIA_MOV(o.mInfoInc);
					mInfoHashShift = GAIA_MOV(o.mInfoHashShift);
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
						mHashMultiplier = GAIA_MOV(o.mHashMultiplier);
						mKeyVals = GAIA_MOV(o.mKeyVals);
						mInfo = GAIA_MOV(o.mInfo);
						mNumElements = GAIA_MOV(o.mNumElements);
						mMask = GAIA_MOV(o.mMask);
						mMaxNumElementsAllowed = GAIA_MOV(o.mMaxNumElementsAllowed);
						mInfoInc = GAIA_MOV(o.mInfoInc);
						mInfoHashShift = GAIA_MOV(o.mInfoHashShift);
						WHash::operator=(GAIA_MOV(static_cast<WHash&>(o)));
						WKeyEqual::operator=(GAIA_MOV(static_cast<WKeyEqual&>(o)));
						DataPool::operator=(GAIA_MOV(static_cast<DataPool&>(o)));

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
				using gaia::core::swap;
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
								Node(*this, std::piecewise_construct, std::forward_as_tuple(GAIA_MOV(key)), std::forward_as_tuple());
						break;

					case InsertionState::overwrite_node:
						mKeyVals[idxAndState.first] =
								Node(*this, std::piecewise_construct, std::forward_as_tuple(GAIA_MOV(key)), std::forward_as_tuple());
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
					insert(GAIA_MOV(vt));
				}
			}

			template <typename... Args>
			std::pair<iterator, bool> emplace(Args&&... args) {
				ROBIN_HOOD_TRACE(this)
				Node n{*this, GAIA_FWD(args)...};
				auto idxAndState = insertKeyPrepareEmptySpot(getFirstConst(n));
				switch (idxAndState.second) {
					case InsertionState::key_found:
						n.destroy(*this);
						break;

					case InsertionState::new_node:
						::new (static_cast<void*>(&mKeyVals[idxAndState.first])) Node(*this, GAIA_MOV(n));
						break;

					case InsertionState::overwrite_node:
						mKeyVals[idxAndState.first] = GAIA_MOV(n);
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
				return emplace(GAIA_FWD(args)...).first;
			}

			template <typename... Args>
			std::pair<iterator, bool> try_emplace(const key_type& key, Args&&... args) {
				return try_emplace_impl(key, GAIA_FWD(args)...);
			}

			template <typename... Args>
			std::pair<iterator, bool> try_emplace(key_type&& key, Args&&... args) {
				return try_emplace_impl(GAIA_MOV(key), GAIA_FWD(args)...);
			}

			template <typename... Args>
			iterator try_emplace(const_iterator hint, const key_type& key, Args&&... args) {
				(void)hint;
				return try_emplace_impl(key, GAIA_FWD(args)...).first;
			}

			template <typename... Args>
			iterator try_emplace(const_iterator hint, key_type&& key, Args&&... args) {
				(void)hint;
				return try_emplace_impl(GAIA_MOV(key), GAIA_FWD(args)...).first;
			}

			template <typename Mapped>
			std::pair<iterator, bool> insert_or_assign(const key_type& key, Mapped&& obj) {
				return insertOrAssignImpl(key, GAIA_FWD(obj));
			}

			template <typename Mapped>
			std::pair<iterator, bool> insert_or_assign(key_type&& key, Mapped&& obj) {
				return insertOrAssignImpl(GAIA_MOV(key), GAIA_FWD(obj));
			}

			template <typename Mapped>
			iterator insert_or_assign(const_iterator hint, const key_type& key, Mapped&& obj) {
				(void)hint;
				return insertOrAssignImpl(key, GAIA_FWD(obj)).first;
			}

			template <typename Mapped>
			iterator insert_or_assign(const_iterator hint, key_type&& key, Mapped&& obj) {
				(void)hint;
				return insertOrAssignImpl(GAIA_MOV(key), GAIA_FWD(obj)).first;
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
				return emplace(GAIA_MOV(keyval));
			}

			iterator insert(const_iterator hint, value_type&& keyval) {
				(void)hint;
				return emplace(GAIA_MOV(keyval)).first;
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
				return MaxLoadFactor100 / 100.0f;
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
							insert_move(GAIA_MOV(oldKeyVals[i]));
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
								*this, std::piecewise_construct, std::forward_as_tuple(GAIA_FWD(key)),
								std::forward_as_tuple(GAIA_FWD(args)...));
						break;

					case InsertionState::overwrite_node:
						mKeyVals[idxAndState.first] = Node(
								*this, std::piecewise_construct, std::forward_as_tuple(GAIA_FWD(key)),
								std::forward_as_tuple(GAIA_FWD(args)...));
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
						mKeyVals[idxAndState.first].getSecond() = GAIA_FWD(obj);
						break;

					case InsertionState::new_node:
						::new (static_cast<void*>(&mKeyVals[idxAndState.first])) Node(
								*this, std::piecewise_construct, std::forward_as_tuple(GAIA_FWD(key)),
								std::forward_as_tuple(GAIA_FWD(obj)));
						break;

					case InsertionState::overwrite_node:
						mKeyVals[idxAndState.first] = Node(
								*this, std::piecewise_construct, std::forward_as_tuple(GAIA_FWD(key)),
								std::forward_as_tuple(GAIA_FWD(obj)));
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
					memcpy(mInfo + i, &val, sizeof(val));
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

/*** End of inlined file: robin_hood.h ***/

namespace gaia {
	namespace cnt {
		template <typename Key, typename Data>
		using map = robin_hood::unordered_flat_map<Key, Data>;
	} // namespace cnt
} // namespace gaia

/*** End of inlined file: map.h ***/


/*** Start of inlined file: sarray.h ***/
#pragma once


/*** Start of inlined file: sarray_impl.h ***/
#pragma once

#include <cstddef>
#include <new>
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
				return mem::data_view_policy<T::gaia_Data_Layout, T>::get({m_ptr, m_cnt}, m_idx);
			}
			T operator->() const {
				return mem::data_view_policy<T::gaia_Data_Layout, T>::get({m_ptr, m_cnt}, m_idx);
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
			using iterator_soa = sarr_iterator_soa<T>;

			static constexpr size_type extent = N;
			static constexpr uint32_t allocated_bytes = view_policy::get_min_byte_size(0, N);

			mem::raw_data_holder<T, allocated_bytes> m_data;

			constexpr sarr() noexcept {
				if constexpr (!mem::is_soa_layout_v<T>)
					core::call_ctor_raw_n(data(), extent);
			}

			~sarr() {
				if constexpr (!mem::is_soa_layout_v<T>)
					core::call_dtor_n(data(), extent);
			}

			template <typename InputIt>
			constexpr sarr(InputIt first, InputIt last) noexcept {
				if constexpr (!mem::is_soa_layout_v<T>)
					core::call_ctor_n(data(), extent);

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
				GAIA_ASSERT(core::addressof(other) != this);

				if constexpr (!mem::is_soa_layout_v<T>)
					core::call_ctor_n(data(), extent);
				mem::move_elements<T>((uint8_t*)m_data, (uint8_t*)other.m_data, other.size(), 0, extent, other.extent);
			}

			sarr& operator=(std::initializer_list<T> il) {
				*this = sarr(il.begin(), il.end());
				return *this;
			}

			constexpr sarr& operator=(const sarr& other) {
				GAIA_ASSERT(core::addressof(other) != this);

				if constexpr (!mem::is_soa_layout_v<T>)
					core::call_ctor_n(data(), extent);
				mem::copy_elements<T>(
						GAIA_ACC((uint8_t*)&m_data[0]), GAIA_ACC((const uint8_t*)&other.m_data[0]), other.size(), 0, extent,
						other.extent);

				return *this;
			}

			constexpr sarr& operator=(sarr&& other) noexcept {
				GAIA_ASSERT(core::addressof(other) != this);

				if constexpr (!mem::is_soa_layout_v<T>)
					core::call_ctor_n(data(), extent);
				mem::move_elements<T>(
						GAIA_ACC((uint8_t*)&m_data[0]), GAIA_ACC((uint8_t*)&other.m_data[0]), other.size(), 0, extent,
						other.extent);

				return *this;
			}

			GAIA_CLANG_WARNING_PUSH()
			// Memory is aligned so we can silence this warning
			GAIA_CLANG_WARNING_DISABLE("-Wcast-align")

			GAIA_NODISCARD constexpr pointer data() noexcept {
				return GAIA_ACC((pointer)&m_data[0]);
			}

			GAIA_NODISCARD constexpr const_pointer data() const noexcept {
				return GAIA_ACC((const_pointer)&m_data[0]);
			}

			GAIA_NODISCARD constexpr decltype(auto) operator[](size_type pos) noexcept {
				GAIA_ASSERT(pos < size());
				return view_policy::set({GAIA_ACC((typename view_policy::TargetCastType) & m_data[0]), extent}, pos);
			}

			GAIA_NODISCARD constexpr decltype(auto) operator[](size_type pos) const noexcept {
				GAIA_ASSERT(pos < size());
				return view_policy::get({GAIA_ACC((typename view_policy::TargetCastType) & m_data[0]), extent}, pos);
			}

			GAIA_CLANG_WARNING_POP()

			GAIA_NODISCARD constexpr size_type size() const noexcept {
				return N;
			}

			GAIA_NODISCARD constexpr bool empty() const noexcept {
				return begin() == end();
			}

			GAIA_NODISCARD constexpr size_type max_size() const noexcept {
				return N;
			}

			GAIA_NODISCARD constexpr decltype(auto) front() noexcept {
				if constexpr (mem::is_soa_layout_v<T>)
					return *begin();
				else
					return (reference)*begin();
			}

			GAIA_NODISCARD constexpr decltype(auto) front() const noexcept {

				if constexpr (mem::is_soa_layout_v<T>)
					return *begin();
				else
					return (const_reference)*begin();
			}

			GAIA_NODISCARD constexpr decltype(auto) back() noexcept {
				if constexpr (mem::is_soa_layout_v<T>)
					return (operator[])(N - 1);
				else
					return (reference) operator[](N - 1);
			}

			GAIA_NODISCARD constexpr decltype(auto) back() const noexcept {
				if constexpr (mem::is_soa_layout_v<T>)
					return operator[](N - 1);
				else
					return (const_reference) operator[](N - 1);
			}

			GAIA_NODISCARD constexpr auto begin() const noexcept {
				if constexpr (mem::is_soa_layout_v<T>)
					return iterator_soa(m_data, size(), 0);
				else
					return iterator(data());
			}

			GAIA_NODISCARD constexpr auto rbegin() const noexcept {
				if constexpr (mem::is_soa_layout_v<T>)
					return iterator_soa(m_data, size(), size() - 1);
				else
					return iterator((pointer)&back());
			}

			GAIA_NODISCARD constexpr auto end() const noexcept {
				if constexpr (mem::is_soa_layout_v<T>)
					return iterator_soa(GAIA_ACC(&m_data[0]), size(), size());
				else
					return iterator(GAIA_ACC((pointer)&m_data[0]) + size());
			}

			GAIA_NODISCARD constexpr auto rend() const noexcept {
				if constexpr (mem::is_soa_layout_v<T>)
					return iterator_soa(GAIA_ACC(&m_data[0]), size(), -1);
				else
					return iterator(GAIA_ACC((pointer)&m_data[0]) - 1);
			}

			GAIA_NODISCARD constexpr bool operator==(const sarr& other) const {
				for (size_type i = 0; i < N; ++i)
					if (!(operator[](i) == other[i]))
						return false;
				return true;
			}

			GAIA_NODISCARD constexpr bool operator!=(const sarr& other) const {
				return !operator==(other);
			}

			template <size_t Item>
			auto soa_view_mut() noexcept {
				return mem::data_view_policy<T::gaia_Data_Layout, T>::template get<Item>(
						std::span<uint8_t>{GAIA_ACC((uint8_t*)&m_data[0]), extent});
			}

			template <size_t Item>
			auto soa_view() const noexcept {
				return mem::data_view_policy<T::gaia_Data_Layout, T>::template get<Item>(
						std::span<const uint8_t>{GAIA_ACC((const uint8_t*)&m_data[0]), extent});
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

/*** End of inlined file: sarray_impl.h ***/

namespace gaia {
	namespace cnt {
		template <typename T, sarr_detail::size_type N>
		using sarray = cnt::sarr<T, N>;
	} // namespace cnt
} // namespace gaia

/*** End of inlined file: sarray.h ***/


/*** Start of inlined file: sarray_ext.h ***/
#pragma once


/*** Start of inlined file: sarray_ext_impl.h ***/
#pragma once

#include <cstddef>
#include <initializer_list>
#include <new>
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
				return mem::data_view_policy<T::gaia_Data_Layout, T>::get({m_ptr, m_cnt}, m_idx);
			}
			T operator->() const {
				return mem::data_view_policy<T::gaia_Data_Layout, T>::get({m_ptr, m_cnt}, m_idx);
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
				if constexpr (!mem::is_soa_layout_v<T>)
					core::call_ctor_raw_n(data(), extent);
			}

			~sarr_ext() {
				if constexpr (!mem::is_soa_layout_v<T>)
					core::call_dtor_n(data(), m_cnt);
			}

			constexpr sarr_ext(size_type count, const_reference value) noexcept {
				resize(count);
				for (auto it: *this)
					*it = value;
			}

			constexpr sarr_ext(size_type count) noexcept {
				resize(count);
			}

			template <typename InputIt>
			constexpr sarr_ext(InputIt first, InputIt last) noexcept {
				if constexpr (!mem::is_soa_layout_v<T>)
					core::call_ctor_n(data(), extent);

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
				GAIA_ASSERT(core::addressof(other) != this);

				if constexpr (!mem::is_soa_layout_v<T>)
					core::call_ctor_n(data(), extent);
				mem::move_elements<T>(m_data, other.m_data, other.size(), 0, extent, other.extent);

				other.m_cnt = size_type(0);
			}

			sarr_ext& operator=(std::initializer_list<T> il) {
				*this = sarr_ext(il.begin(), il.end());
				return *this;
			}

			constexpr sarr_ext& operator=(const sarr_ext& other) {
				GAIA_ASSERT(core::addressof(other) != this);

				if constexpr (!mem::is_soa_layout_v<T>)
					core::call_ctor_n(data(), extent);
				resize(other.size());
				mem::copy_elements<T>(
						GAIA_ACC((uint8_t*)&m_data[0]), GAIA_ACC((const uint8_t*)&other.m_data[0]), other.size(), 0, extent,
						other.extent);

				return *this;
			}

			constexpr sarr_ext& operator=(sarr_ext&& other) noexcept {
				GAIA_ASSERT(core::addressof(other) != this);

				if constexpr (!mem::is_soa_layout_v<T>)
					core::call_ctor_n(data(), extent);
				resize(other.m_cnt);
				mem::move_elements<T>(
						GAIA_ACC((uint8_t*)&m_data[0]), GAIA_ACC((uint8_t*)&other.m_data[0]), other.size(), 0, extent,
						other.extent);

				other.m_cnt = size_type(0);

				return *this;
			}

			GAIA_CLANG_WARNING_PUSH()
			// Memory is aligned so we can silence this warning
			GAIA_CLANG_WARNING_DISABLE("-Wcast-align")

			GAIA_NODISCARD constexpr pointer data() noexcept {
				return GAIA_ACC((pointer)&m_data[0]);
			}

			GAIA_NODISCARD constexpr const_pointer data() const noexcept {
				return GAIA_ACC((const_pointer)&m_data[0]);
			}

			GAIA_NODISCARD constexpr decltype(auto) operator[](size_type pos) noexcept {
				GAIA_ASSERT(pos < size());
				return view_policy::set({GAIA_ACC((typename view_policy::TargetCastType) & m_data[0]), extent}, pos);
			}

			GAIA_NODISCARD constexpr decltype(auto) operator[](size_type pos) const noexcept {
				GAIA_ASSERT(pos < size());
				return view_policy::get({GAIA_ACC((typename view_policy::TargetCastType) & m_data[0]), extent}, pos);
			}

			GAIA_CLANG_WARNING_POP()

			constexpr void push_back(const T& arg) noexcept {
				GAIA_ASSERT(size() < N);

				if constexpr (mem::is_soa_layout_v<T>) {
					operator[](m_cnt++) = arg;
				} else {
					auto* ptr = &data()[m_cnt++];
					core::call_ctor(ptr, arg);
				}
			}

			constexpr void push_back(T&& arg) noexcept {
				GAIA_ASSERT(size() < N);

				if constexpr (mem::is_soa_layout_v<T>) {
					operator[](m_cnt++) = GAIA_MOV(arg);
				} else {
					auto* ptr = &data()[m_cnt++];
					core::call_ctor(ptr, GAIA_MOV(arg));
				}
			}

			template <typename... Args>
			constexpr decltype(auto) emplace_back(Args&&... args) {
				GAIA_ASSERT(size() < N);

				if constexpr (mem::is_soa_layout_v<T>) {
					operator[](m_cnt++) = T(GAIA_FWD(args)...);
					return;
				} else {
					auto* ptr = &data()[m_cnt++];
					core::call_ctor(ptr, GAIA_FWD(args)...);
					return (reference)*ptr;
				}
			}

			constexpr void pop_back() noexcept {
				GAIA_ASSERT(!empty());

				if constexpr (!mem::is_soa_layout_v<T>) {
					auto* ptr = &data()[m_cnt];
					core::call_dtor(ptr);
				}

				--m_cnt;
			}

			//! Insert the element to the position given by iterator \param pos
			iterator insert(iterator pos, const T& arg) noexcept {
				GAIA_ASSERT(size() < N);
				GAIA_ASSERT(pos >= data());
				GAIA_ASSERT(empty() || (pos < iterator(data() + size())));

				const auto idxSrc = (size_type)core::distance(begin(), pos);
				const auto idxDst = (size_type)core::distance(begin(), end());

				mem::shift_elements_right<T>(m_data, idxDst, idxSrc, extent);

				if constexpr (mem::is_soa_layout_v<T>) {
					operator[](idxSrc) = arg;
				} else {
					auto* ptr = &data()[idxSrc];
					core::call_ctor(ptr, arg);
				}

				++m_cnt;

				return iterator(&data()[idxSrc]);
			}

			//! Insert the element to the position given by iterator \param pos
			iterator insert(iterator pos, T&& arg) noexcept {
				GAIA_ASSERT(size() < N);
				GAIA_ASSERT(pos >= data());
				GAIA_ASSERT(empty() || (pos < iterator(data() + size())));

				const auto idxSrc = (size_type)core::distance(begin(), pos);
				const auto idxDst = (size_type)core::distance(begin(), end());

				mem::shift_elements_right<T>(m_data, idxDst, idxSrc, extent);

				if constexpr (mem::is_soa_layout_v<T>) {
					operator[](idxSrc) = GAIA_MOV(arg);
				} else {
					auto* ptr = &data()[idxSrc];
					core::call_ctor(ptr, GAIA_MOV(arg));
				}

				++m_cnt;

				return iterator(&data()[idxSrc]);
			}

			//! Removes the element at pos
			//! \param pos Iterator to the element to remove
			constexpr iterator erase(iterator pos) noexcept {
				GAIA_ASSERT(pos >= data());
				GAIA_ASSERT(empty() || (pos < iterator(data() + size())));

				if (empty())
					return end();

				const auto idxSrc = (size_type)core::distance(begin(), pos);
				const auto idxDst = (size_type)core::distance(begin(), end()) - 1;

				mem::shift_elements_left<T>(m_data, idxDst, idxSrc, extent);
				// Destroy if it's the last element
				if constexpr (!mem::is_soa_layout_v<T>) {
					auto* ptr = &data()[m_cnt - 1];
					core::call_dtor(ptr);
				}

				--m_cnt;

				return iterator(&data()[idxSrc]);
			}

			//! Removes the elements in the range [first, last)
			//! \param first Iterator to the element to remove
			//! \param last Iterator to the one beyond the last element to remove
			iterator erase(iterator first, iterator last) noexcept {
				GAIA_ASSERT(first >= data())
				GAIA_ASSERT(empty() || (first < iterator(data() + size())));
				GAIA_ASSERT(last > first);
				GAIA_ASSERT(last <= (data() + size()));

				if (empty())
					return end();

				const auto idxSrc = (size_type)core::distance(begin(), first);
				const auto idxDst = size();
				const auto cnt = last - first;

				mem::shift_elements_left_n<T>(m_data, idxDst, idxSrc, cnt, extent);
				// Destroy if it's the last element
				if constexpr (!mem::is_soa_layout_v<T>)
					core::call_dtor_n(&data()[m_cnt - cnt], cnt);

				m_cnt -= cnt;

				return iterator(&data()[idxSrc]);
			}

			//! Removes the element at index \param pos
			iterator erase_at(size_type pos) noexcept {
				GAIA_ASSERT(pos < size());

				const auto idxSrc = pos;
				const auto idxDst = (size_type)core::distance(begin(), end()) - 1;

				mem::shift_elements_left<T>(m_data, idxDst, idxSrc, extent);
				// Destroy if it's the last element
				if constexpr (!mem::is_soa_layout_v<T>) {
					auto* ptr = &data()[m_cnt - 1];
					core::call_dtor(ptr);
				}

				--m_cnt;

				return iterator(&data()[idxSrc]);
			}

			constexpr void clear() noexcept {
				resize(0);
			}

			constexpr void resize(size_type count) noexcept {
				GAIA_ASSERT(count <= max_size());

				// Resizing to a smaller size
				if (count <= m_cnt) {
					// Destroy elements at the end
					if constexpr (!mem::is_soa_layout_v<T>)
						core::call_dtor_n(&data()[count], size() - count);
				} else
				// Resizing to a bigger size but still within allocated capacity
				{
					// Construct new elements
					if constexpr (!mem::is_soa_layout_v<T>)
						core::call_ctor_n(&data()[size()], count - size());
				}

				m_cnt = count;
			}

			//! Removes all elements that fail the predicate.
			//! \param func A lambda or a functor with the bool operator()(Container::value_type&) overload.
			//! \return The new size of the array.
			template <typename Func>
			auto retain(Func&& func) {
				size_type erased = 0;
				size_type idxDst = 0;
				size_type idxSrc = 0;

				while (idxSrc < m_cnt) {
					if (func(operator[](idxSrc))) {
						if (idxDst < idxSrc) {
							auto* ptr = (uint8_t*)data();
							mem::move_element<T>(ptr, ptr, idxDst, idxSrc, max_size(), max_size());
							auto* ptr2 = &data()[idxSrc];
							core::call_dtor(ptr2);
						}
						++idxDst;
					} else {
						auto* ptr = &data()[idxSrc];
						core::call_dtor(ptr);
						++erased;
					}

					++idxSrc;
				}

				m_cnt -= erased;
				return idxDst;
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

			GAIA_NODISCARD constexpr decltype(auto) front() noexcept {
				GAIA_ASSERT(!empty());
				if constexpr (mem::is_soa_layout_v<T>)
					return *begin();
				else
					return (reference)*begin();
			}

			GAIA_NODISCARD constexpr decltype(auto) front() const noexcept {
				GAIA_ASSERT(!empty());
				if constexpr (mem::is_soa_layout_v<T>)
					return *begin();
				else
					return (const_reference)*begin();
			}

			GAIA_NODISCARD constexpr decltype(auto) back() noexcept {
				GAIA_ASSERT(!empty());
				if constexpr (mem::is_soa_layout_v<T>)
					return (operator[])(m_cnt - 1);
				else
					return (reference) operator[](m_cnt - 1);
			}

			GAIA_NODISCARD constexpr decltype(auto) back() const noexcept {
				GAIA_ASSERT(!empty());
				if constexpr (mem::is_soa_layout_v<T>)
					return operator[](m_cnt - 1);
				else
					return (const_reference) operator[](m_cnt - 1);
			}

			GAIA_NODISCARD constexpr auto begin() const noexcept {
				if constexpr (mem::is_soa_layout_v<T>)
					return iterator_soa(m_data, size(), 0);
				else
					return iterator(data());
			}

			GAIA_NODISCARD constexpr auto rbegin() const noexcept {
				if constexpr (mem::is_soa_layout_v<T>)
					return iterator_soa(m_data, size(), size() - 1);
				else
					return iterator((pointer)&back());
			}

			GAIA_NODISCARD constexpr auto end() const noexcept {
				if constexpr (mem::is_soa_layout_v<T>)
					return iterator_soa(GAIA_ACC(&m_data[0]), size(), size());
				else
					return iterator(GAIA_ACC((pointer)&m_data[0]) + size());
			}

			GAIA_NODISCARD constexpr auto rend() const noexcept {
				if constexpr (mem::is_soa_layout_v<T>)
					return iterator_soa(GAIA_ACC(&m_data[0]), size(), -1);
				else
					return iterator(GAIA_ACC((pointer)&m_data[0]) - 1);
			}

			GAIA_NODISCARD constexpr bool operator==(const sarr_ext& other) const {
				if (m_cnt != other.m_cnt)
					return false;
				const size_type n = size();
				for (size_type i = 0; i < n; ++i)
					if (!(operator[](i) == other[i]))
						return false;
				return true;
			}

			GAIA_NODISCARD constexpr bool operator!=(const sarr_ext& other) const {
				return !operator==(other);
			}

			template <size_t Item>
			auto soa_view_mut() noexcept {
				return mem::data_view_policy<T::gaia_Data_Layout, T>::template get<Item>(
						std::span<uint8_t>{GAIA_ACC((uint8_t*)&m_data[0]), extent});
			}

			template <size_t Item>
			auto soa_view() const noexcept {
				return mem::data_view_policy<T::gaia_Data_Layout, T>::template get<Item>(
						std::span<const uint8_t>{GAIA_ACC((const uint8_t*)&m_data[0]), extent});
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
/*** End of inlined file: sarray_ext_impl.h ***/

namespace gaia {
	namespace cnt {
		template <typename T, sarr_ext_detail::size_type N>
		using sarray_ext = cnt::sarr_ext<T, N>;
	} // namespace cnt
} // namespace gaia

/*** End of inlined file: sarray_ext.h ***/


/*** Start of inlined file: set.h ***/
#pragma once

namespace gaia {
	namespace cnt {
		template <typename Key>
		using set = robin_hood::unordered_flat_set<Key>;
	} // namespace cnt
} // namespace gaia

/*** End of inlined file: set.h ***/


/*** Start of inlined file: sparse_storage.h ***/
#pragma once

#include <cstddef>
#include <initializer_list>
#include <type_traits>
#include <utility>

namespace gaia {
	namespace cnt {
		namespace detail {
			using difference_type = uint32_t;
			using size_type = uint32_t;

			constexpr static uint32_t InvalidId = BadIndex - 1;

			template <typename T, uint32_t PageCapacity, typename Allocator, typename>
			class sparse_page;
		} // namespace detail

		using dense_id = uint32_t;
		using sparse_id = uint32_t;

		template <typename T>
		struct to_sparse_id {
			static sparse_id get(const T& item) noexcept {
				(void)item;
				static_assert(
						std::is_empty_v<T>,
						"Sparse_storage items require a conversion function to be defined in gaia::cnt namespace");
				return BadIndex;
			}
		};

		template <typename T, uint32_t PageCapacity, typename Allocator, typename = void>
		struct sparse_iterator {
			using iterator_category = core::random_access_iterator_tag;
			using value_type = T;
			using pointer = T*;
			using reference = T&;
			using difference_type = detail::difference_type;
			using size_type = detail::size_type;
			using iterator = sparse_iterator;

		private:
			constexpr static detail::size_type to_page_index = core::count_bits(PageCapacity);
			using page_type = detail::sparse_page<T, PageCapacity, Allocator, void>;

			uint32_t* m_pDense;
			page_type* m_pPages;

		public:
			sparse_iterator(uint32_t* pDense, page_type* pPages): m_pDense(pDense), m_pPages(pPages) {}

			reference operator*() const {
				const auto sid = *m_pDense;
				const auto pid = sid >> to_page_index;
				const auto did = sid & (PageCapacity - 1);
				auto& page = m_pPages[pid];
				return page.set_data(did);
			}
			pointer operator->() const {
				const auto sid = *m_pDense;
				const auto pid = sid >> to_page_index;
				const auto did = sid & (PageCapacity - 1);
				auto& page = m_pPages[pid];
				return &page.set_data(did);
			}
			iterator operator[](size_type offset) const {
				return {m_pDense + offset, m_pPages};
			}

			iterator& operator+=(size_type diff) {
				m_pDense += diff;
				return *this;
			}
			iterator& operator-=(size_type diff) {
				m_pDense -= diff;
				return *this;
			}
			iterator& operator++() {
				++m_pDense;
				return *this;
			}
			iterator operator++(int) {
				iterator temp(*this);
				++*this;
				return temp;
			}
			iterator& operator--() {
				--m_pDense;
				return *this;
			}
			iterator operator--(int) {
				iterator temp(*this);
				--*this;
				return temp;
			}

			iterator operator+(size_type offset) const {
				return {m_pDense + offset, m_pPages};
			}
			iterator operator-(size_type offset) const {
				return {m_pDense - offset, m_pPages};
			}
			difference_type operator-(const iterator& other) const {
				return (difference_type)(m_pDense - other.m_pDense);
			}

			GAIA_NODISCARD bool operator==(const iterator& other) const {
				return m_pDense == other.m_pDense;
			}
			GAIA_NODISCARD bool operator!=(const iterator& other) const {
				return m_pDense != other.m_pDense;
			}
			GAIA_NODISCARD bool operator>(const iterator& other) const {
				return m_pDense > other.m_pDense;
			}
			GAIA_NODISCARD bool operator>=(const iterator& other) const {
				return m_pDense >= other.m_pDense;
			}
			GAIA_NODISCARD bool operator<(const iterator& other) const {
				return m_pDense < other.m_pDense;
			}
			GAIA_NODISCARD bool operator<=(const iterator& other) const {
				return m_pDense <= other.m_pDense;
			}
		};

		template <typename T, uint32_t PageCapacity, typename Allocator>
		struct sparse_iterator<T, PageCapacity, Allocator, std::enable_if_t<std::is_empty_v<T>>> {
			using iterator_category = core::random_access_iterator_tag;
			using value_type = sparse_id;
			// using pointer = sparse_id*; not supported
			// using reference = sparse_id&; not supported
			using difference_type = detail::difference_type;
			using size_type = detail::size_type;
			using iterator = sparse_iterator;

		private:
			constexpr static detail::size_type to_page_index = core::count_bits(PageCapacity);
			using page_type = detail::sparse_page<T, PageCapacity, Allocator, std::enable_if_t<std::is_empty_v<T>>>;

			uint32_t* m_pDense;

		public:
			sparse_iterator(uint32_t* pDense): m_pDense(pDense) {}

			value_type operator*() const {
				const auto sid = *m_pDense;
				return sid;
			}
			value_type operator->() const {
				const auto sid = *m_pDense;
				return sid;
			}
			iterator operator[](size_type offset) const {
				return {m_pDense + offset};
			}

			iterator& operator+=(size_type diff) {
				m_pDense += diff;
				return *this;
			}
			iterator& operator-=(size_type diff) {
				m_pDense -= diff;
				return *this;
			}
			iterator& operator++() {
				++m_pDense;
				return *this;
			}
			iterator operator++(int) {
				iterator temp(*this);
				++*this;
				return temp;
			}
			iterator& operator--() {
				--m_pDense;
				return *this;
			}
			iterator operator--(int) {
				iterator temp(*this);
				--*this;
				return temp;
			}

			iterator operator+(size_type offset) const {
				return {m_pDense + offset};
			}
			iterator operator-(size_type offset) const {
				return {m_pDense - offset};
			}
			difference_type operator-(const iterator& other) const {
				return (difference_type)(m_pDense - other.m_pDense);
			}

			GAIA_NODISCARD bool operator==(const iterator& other) const {
				return m_pDense == other.m_pDense;
			}
			GAIA_NODISCARD bool operator!=(const iterator& other) const {
				return m_pDense != other.m_pDense;
			}
			GAIA_NODISCARD bool operator>(const iterator& other) const {
				return m_pDense > other.m_pDense;
			}
			GAIA_NODISCARD bool operator>=(const iterator& other) const {
				return m_pDense >= other.m_pDense;
			}
			GAIA_NODISCARD bool operator<(const iterator& other) const {
				return m_pDense < other.m_pDense;
			}
			GAIA_NODISCARD bool operator<=(const iterator& other) const {
				return m_pDense <= other.m_pDense;
			}
		};

		template <typename T, uint32_t PageCapacity, typename Allocator>
		struct sparse_iterator_soa {
			using iterator_category = core::random_access_iterator_tag;
			using value_type = T;
			// using pointer = T*; not supported
			// using reference = T&; not supported
			using difference_type = detail::difference_type;
			using size_type = detail::size_type;
			using iterator = sparse_iterator_soa;

		private:
			constexpr static detail::size_type to_page_index = core::count_bits(PageCapacity);
			using page_type = detail::sparse_page<T, PageCapacity, Allocator, void>;

			uint32_t* m_pDense;
			page_type* m_pPages;

		public:
			sparse_iterator_soa(uint32_t* pDense, page_type* pPages): m_pDense(pDense), m_pPages(pPages) {}

			value_type operator*() const {
				const auto sid = *m_pDense;
				const auto pid = sid >> to_page_index;
				const auto did = sid & (PageCapacity - 1);
				const auto& page = m_pPages[pid];
				return page.get_data(did);
			}
			value_type operator->() const {
				const auto sid = *m_pDense;
				const auto pid = sid >> to_page_index;
				const auto did = sid & (PageCapacity - 1);
				const auto& page = m_pPages[pid];
				return page.get_data(did);
			}
			iterator operator[](size_type offset) const {
				return iterator(m_pDense + offset, m_pPages);
			}

			iterator& operator+=(size_type diff) {
				m_pDense += diff;
				return *this;
			}
			iterator& operator-=(size_type diff) {
				m_pDense -= diff;
				return *this;
			}
			iterator& operator++() {
				++m_pDense;
				return *this;
			}
			iterator operator++(int) {
				iterator temp(*this);
				++*this;
				return temp;
			}
			iterator& operator--() {
				--m_pDense;
				return *this;
			}
			iterator operator--(int) {
				iterator temp(*this);
				--*this;
				return temp;
			}

			iterator operator+(size_type offset) const {
				return {m_pDense + offset, m_pPages};
			}
			iterator operator-(size_type offset) const {
				return {m_pDense - offset, m_pPages};
			}
			difference_type operator-(const iterator& other) const {
				return (difference_type)(m_pDense - other.m_pDense);
			}

			GAIA_NODISCARD bool operator==(const iterator& other) const {
				return m_pDense == other.m_pDense;
			}
			GAIA_NODISCARD bool operator!=(const iterator& other) const {
				return m_pDense != other.m_pDense;
			}
			GAIA_NODISCARD bool operator>(const iterator& other) const {
				return m_pDense > other.m_pDense;
			}
			GAIA_NODISCARD bool operator>=(const iterator& other) const {
				return m_pDense >= other.m_pDense;
			}
			GAIA_NODISCARD bool operator<(const iterator& other) const {
				return m_pDense < other.m_pDense;
			}
			GAIA_NODISCARD bool operator<=(const iterator& other) const {
				return m_pDense <= other.m_pDense;
			}
		};

		namespace detail {
			template <typename T, uint32_t PageCapacity, typename Allocator, typename = void>
			class sparse_page {
			public:
				static_assert(
						!std::is_empty_v<T>, "It only makes sense to use sparse storage for data types with non-zero size");

				using value_type = T;
				using reference = T&;
				using const_reference = const T&;
				using pointer = T*;
				using const_pointer = T*;
				using view_policy = mem::auto_view_policy<T>;
				using difference_type = detail::difference_type;
				using size_type = detail::size_type;

				using iterator = sparse_iterator<T, PageCapacity, Allocator>;
				using iterator_soa = sparse_iterator_soa<T, PageCapacity, Allocator>;

			private:
				uint32_t* m_pSparse = nullptr;
				uint8_t* m_pData = nullptr;
				size_type m_cnt = 0;

				void ensure() {
					if (m_pSparse == nullptr) {
						// Allocate memory for sparse->dense index mapping.
						// Make sure initial values are detail::InvalidId.
						m_pSparse = mem::AllocHelper::alloc<uint32_t>("SparsePage", PageCapacity);
						GAIA_FOR(PageCapacity) m_pSparse[i] = detail::InvalidId;

						// Allocate memory for data
						m_pData = view_policy::template alloc<Allocator>(PageCapacity);
					}
				}

				void del_data_inter(uint32_t idx) noexcept {
					GAIA_ASSERT(!empty());

					if constexpr (!mem::is_soa_layout_v<T>) {
						auto* ptr = &data()[idx];
						core::call_dtor(ptr);
					}

					--m_cnt;
				}

				void del_active_data() noexcept {
					GAIA_ASSERT(m_pSparse != nullptr);

					for (uint32_t i = 0; m_cnt != 0 && i != PageCapacity; ++i) {
						if (m_pSparse[i] == detail::InvalidId)
							continue;

						if constexpr (!mem::is_soa_layout_v<T>) {
							auto* ptr = &data()[i];
							core::call_dtor(ptr);
						}
					}

					m_cnt = 0;
				}

				void invalidate() {
					if (m_pSparse == nullptr)
						return;

					// Destruct active items
					del_active_data();

					// Release allocated memory
					mem::AllocHelper::free("SparsePage", m_pSparse);
					view_policy::template free<Allocator>(m_pData, m_cnt);

					m_pSparse = nullptr;
					m_pData = nullptr;
					m_cnt = 0;
				}

			public:
				sparse_page() = default;

				sparse_page(const sparse_page& other) {
					// Copy new items over
					if (other.m_pSparse == nullptr) {
						invalidate();
					} else {
						for (uint32_t i = 0; i < PageCapacity; ++i) {
							// Copy indices
							m_pSparse[i] = other.m_pSparse[i];
							if (m_pSparse[i] == detail::InvalidId)
								continue;

							// Copy data
							set_data(i) = other.set_data(i);
						}

						m_cnt = other.m_cnt;
					}
				}

				sparse_page& operator=(const sparse_page& other) {
					GAIA_ASSERT(core::addressof(other) != this);

					if (m_pData == nullptr && other.m_pData != nullptr)
						ensure();

					// Remove current active items
					if (m_pSparse != nullptr)
						del_active_data();

					GAIA_ASSERT(m_cnt == 0);

					// Copy new items over if there are any
					if (other.m_pSparse == nullptr) {
						invalidate();
					} else {
						for (uint32_t i = 0; i < PageCapacity; ++i) {
							// Copy indices
							m_pSparse[i] = other.m_pSparse[i];
							if (other.m_pSparse[i] == detail::InvalidId)
								continue;

							// Copy data
							mem::copy_ctor_element<T>(m_pData, other.m_pData, i, i, PageCapacity, PageCapacity);
						}

						m_cnt = other.m_cnt;
					}

					return *this;
				}

				sparse_page(sparse_page&& other) noexcept {
					// This is a newly constructed object.
					// It can't have any memory allocated, yet.
					GAIA_ASSERT(m_pData == nullptr);

					m_pSparse = other.m_pSparse;
					m_pData = other.m_pData;
					m_cnt = other.m_cnt;

					other.m_pSparse = nullptr;
					other.m_pData = nullptr;
					other.m_cnt = size_type(0);
				}

				sparse_page& operator=(sparse_page&& other) noexcept {
					GAIA_ASSERT(core::addressof(other) != this);

					invalidate();

					m_pSparse = other.m_pSparse;
					m_pData = other.m_pData;
					m_cnt = other.m_cnt;

					other.m_pSparse = nullptr;
					other.m_pData = nullptr;
					other.m_cnt = size_type(0);

					return *this;
				}

				~sparse_page() {
					invalidate();
				}

				GAIA_CLANG_WARNING_PUSH()
				// Memory is aligned so we can silence this warning
				GAIA_CLANG_WARNING_DISABLE("-Wcast-align")

				GAIA_NODISCARD pointer data() noexcept {
					return (pointer)m_pData;
				}

				GAIA_NODISCARD const_pointer data() const noexcept {
					return (const_pointer)m_pData;
				}

				GAIA_NODISCARD uint32_t& set_id(size_type pos) noexcept {
					return m_pSparse[pos];
				}

				GAIA_NODISCARD uint32_t get_id(size_type pos) const noexcept {
					return m_pSparse[pos];
				}

				GAIA_NODISCARD decltype(auto) set_data(size_type pos) noexcept {
					return view_policy::set({(typename view_policy::TargetCastType)m_pData, PageCapacity}, pos);
				}

				GAIA_NODISCARD decltype(auto) get_data(size_type pos) const noexcept {
					return view_policy::get({(typename view_policy::TargetCastType)m_pData, PageCapacity}, pos);
				}

				GAIA_CLANG_WARNING_POP()

				void add() {
					ensure();
					++m_cnt;
				}

				decltype(auto) add_data(uint32_t idx, const T& arg) {
					if constexpr (mem::is_soa_layout_v<T>) {
						set_data(idx) = arg;
					} else {
						auto* ptr = &set_data(idx);
						core::call_ctor(ptr, arg);
						return (reference)(*ptr);
					}
				}

				decltype(auto) add_data(uint32_t idx, T&& arg) {
					if constexpr (mem::is_soa_layout_v<T>) {
						set_data(idx) = GAIA_MOV(arg);
					} else {
						auto* ptr = &set_data(idx);
						core::call_ctor(ptr, GAIA_MOV(arg));
						return (reference)(*ptr);
					}
				}

				void del_data(uint32_t idx) noexcept {
					del_data_inter(idx);

					// If there is no more data, release the memory allocated by the page
					if (m_cnt == 0)
						invalidate();
				}

				GAIA_NODISCARD size_type size() const noexcept {
					return m_cnt;
				}

				GAIA_NODISCARD bool empty() const noexcept {
					return size() == 0;
				}

				GAIA_NODISCARD decltype(auto) front() noexcept {
					GAIA_ASSERT(!empty());
					if constexpr (mem::is_soa_layout_v<T>)
						return *begin();
					else
						return (reference)*begin();
				}

				GAIA_NODISCARD decltype(auto) front() const noexcept {
					GAIA_ASSERT(!empty());
					if constexpr (mem::is_soa_layout_v<T>)
						return *begin();
					else
						return (const_reference)*begin();
				}

				GAIA_NODISCARD decltype(auto) back() noexcept {
					GAIA_ASSERT(!empty());
					if constexpr (mem::is_soa_layout_v<T>)
						return set_data(m_cnt - 1);
					else
						return (reference)(set_data(m_cnt - 1));
				}

				GAIA_NODISCARD decltype(auto) back() const noexcept {
					GAIA_ASSERT(!empty());
					if constexpr (mem::is_soa_layout_v<T>)
						return set_data(m_cnt - 1);
					else
						return (const_reference)set_data(m_cnt - 1);
				}

				GAIA_NODISCARD auto begin() const noexcept {
					if constexpr (mem::is_soa_layout_v<T>)
						return iterator_soa(m_pData, size(), 0);
					else
						return iterator(data());
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
						return iterator(data() + size());
				}

				GAIA_NODISCARD auto rend() const noexcept {
					if constexpr (mem::is_soa_layout_v<T>)
						return iterator_soa(m_pData, size(), -1);
					else
						return iterator(data() - 1);
				}

				GAIA_NODISCARD bool operator==(const sparse_page& other) const {
					if (m_cnt != other.m_cnt)
						return false;
					const size_type n = size();
					for (size_type i = 0; i < n; ++i)
						if (!(get_data(i) == other[i]))
							return false;
					return true;
				}

				GAIA_NODISCARD constexpr bool operator!=(const sparse_page& other) const {
					return !operator==(other);
				}
			};

			//! Spare page. Specialized for zero-size \tparam T
			template <typename T, uint32_t PageCapacity, typename Allocator>
			class sparse_page<T, PageCapacity, Allocator, std::enable_if_t<std::is_empty_v<T>>> {
			public:
				using value_type = T;
				// using reference = T&; not supported
				// using const_reference = const T&; not supported
				using pointer = T*;
				using const_pointer = T*;
				using view_policy = mem::auto_view_policy<T>;
				using difference_type = detail::difference_type;
				using size_type = detail::size_type;

				using iterator = sparse_iterator<T, PageCapacity, Allocator, std::enable_if_t<std::is_empty_v<T>>>;

			private:
				uint32_t* m_pSparse = nullptr;
				size_type m_cnt = 0;

				void ensure() {
					if (m_pSparse == nullptr) {
						// Allocate memory for sparse->dense index mapping.
						// Make sure initial values are detail::InvalidId.
						m_pSparse = mem::AllocHelper::alloc<uint32_t>("SparsePage", PageCapacity);
						GAIA_FOR(PageCapacity) m_pSparse[i] = detail::InvalidId;
					}
				}

				void del_data_inter(uint32_t idx) noexcept {
					GAIA_ASSERT(!empty());
					--m_cnt;
				}

				void del_active_data() noexcept {
					GAIA_ASSERT(m_pSparse != nullptr);
					m_cnt = 0;
				}

				void invalidate() {
					if (m_pSparse == nullptr)
						return;

					// Release allocated memory
					mem::AllocHelper::free("SparsePage", m_pSparse);

					m_pSparse = nullptr;
					m_cnt = 0;
				}

			public:
				sparse_page() = default;

				sparse_page(const sparse_page& other) {
					// Copy new items over
					if (other.m_pSparse == nullptr) {
						invalidate();
					} else {
						for (uint32_t i = 0; i < PageCapacity; ++i) {
							// Copy indices
							m_pSparse[i] = other.m_pSparse[i];
							if (m_pSparse[i] == detail::InvalidId)
								continue;
						}

						m_cnt = other.m_cnt;
					}
				}

				sparse_page& operator=(const sparse_page& other) {
					GAIA_ASSERT(core::addressof(other) != this);

					if (m_pSparse == nullptr && other.m_pSparse != nullptr)
						ensure();

					// Remove current active items
					if (m_pSparse != nullptr)
						del_active_data();

					GAIA_ASSERT(m_cnt == 0);

					// Copy new items over if there are any
					if (other.m_pSparse == nullptr) {
						invalidate();
					} else {
						// Copy indices
						for (uint32_t i = 0; i < PageCapacity; ++i)
							m_pSparse[i] = other.m_pSparse[i];

						m_cnt = other.m_cnt;
					}

					return *this;
				}

				sparse_page(sparse_page&& other) noexcept {
					// This is a newly constructed object.
					// It can't have any memory allocated, yet.
					GAIA_ASSERT(m_pSparse == nullptr);

					m_pSparse = other.m_pSparse;
					m_cnt = other.m_cnt;

					other.m_pSparse = nullptr;
					other.m_cnt = size_type(0);
				}

				sparse_page& operator=(sparse_page&& other) noexcept {
					GAIA_ASSERT(core::addressof(other) != this);

					invalidate();

					m_pSparse = other.m_pSparse;
					m_cnt = other.m_cnt;

					other.m_pSparse = nullptr;
					other.m_cnt = size_type(0);

					return *this;
				}

				~sparse_page() {
					invalidate();
				}

				GAIA_CLANG_WARNING_PUSH()
				// Memory is aligned so we can silence this warning
				GAIA_CLANG_WARNING_DISABLE("-Wcast-align")

				GAIA_NODISCARD pointer data() noexcept {
					return (pointer)m_pSparse;
				}

				GAIA_NODISCARD const_pointer data() const noexcept {
					return (const_pointer)m_pSparse;
				}

				GAIA_CLANG_WARNING_POP()

				void add() {
					ensure();
					++m_cnt;
				}

				GAIA_NODISCARD uint32_t& set_id(size_type pos) noexcept {
					return m_pSparse[pos];
				}

				GAIA_NODISCARD uint32_t get_id(size_type pos) const noexcept {
					return m_pSparse[pos];
				}

				void del_id(uint32_t idx) noexcept {
					del_data_inter(idx);

					// If there is no more data, release the memory allocated by the page
					if (m_cnt == 0)
						invalidate();
				}

				GAIA_NODISCARD size_type size() const noexcept {
					return m_cnt;
				}

				GAIA_NODISCARD bool empty() const noexcept {
					return size() == 0;
				}

				GAIA_NODISCARD sparse_id front() const noexcept {
					GAIA_ASSERT(!empty());
					return *begin();
				}

				GAIA_NODISCARD sparse_id back() const noexcept {
					GAIA_ASSERT(!empty());
					return get_id(m_cnt - 1);
				}

				GAIA_NODISCARD auto begin() const noexcept {
					return iterator(data());
				}

				GAIA_NODISCARD auto rbegin() const noexcept {
					return iterator((pointer)&back());
				}

				GAIA_NODISCARD auto end() const noexcept {
					return iterator(data() + size());
				}

				GAIA_NODISCARD auto rend() const noexcept {
					return iterator(data() - 1);
				}

				GAIA_NODISCARD bool operator==(const sparse_page& other) const {
					if (m_cnt != other.m_cnt)
						return false;
					const size_type n = size();
					for (size_type i = 0; i < n; ++i)
						if (!(get_id(i) == other.get_id(i)))
							return false;
					return true;
				}

				GAIA_NODISCARD constexpr bool operator!=(const sparse_page& other) const {
					return !operator==(other);
				}
			};
		} // namespace detail

		//! Array with variable size of elements of type \tparam T allocated on heap.
		//! Allocates enough memory to support \tparam PageCapacity elements.
		//! Uses \tparam Allocator to allocate memory.
		template <
				typename T, uint32_t PageCapacity = 4096, typename Allocator = mem::DefaultAllocatorAdaptor, typename = void>
		class sparse_storage {
		public:
			using value_type = T;
			using reference = T&;
			using const_reference = const T&;
			using pointer = T*;
			using const_pointer = T*;
			using view_policy = mem::auto_view_policy<T>;
			using difference_type = detail::difference_type;
			using size_type = detail::size_type;

			using iterator = sparse_iterator<T, PageCapacity, Allocator>;
			using iterator_soa = sparse_iterator_soa<T, PageCapacity, Allocator>;
			using page_type = detail::sparse_page<T, PageCapacity, Allocator>;

		private:
			constexpr static detail::size_type to_page_index = core::count_bits(PageCapacity);

			//! Contains mappings to sparse storage inside pages
			cnt::darray<sparse_id> m_dense;
			//! Contains pages with data and sparse–>dense mapping
			cnt::darray<page_type> m_pages;
			//! Current number of items tracked by the sparse set
			size_type m_cnt = size_type(0);

			void try_grow(uint32_t pid) {
				m_dense.resize(m_cnt + 1);

				// The spare array has to be able to take any sparse index
				if (pid >= m_pages.size())
					m_pages.resize(pid + 1);

				m_pages[pid].add();
			}

		public:
			constexpr sparse_storage() noexcept = default;

			sparse_storage(const sparse_storage& other) {
				GAIA_ASSERT(core::addressof(other) != this);

				m_dense = other.m_dense;
				m_pages = other.m_pages;
				m_cnt = other.m_cnt;
			}

			sparse_storage& operator=(const sparse_storage& other) {
				GAIA_ASSERT(core::addressof(other) != this);

				m_dense = other.m_dense;
				m_pages = other.m_pages;
				m_cnt = other.m_cnt;

				return *this;
			}

			sparse_storage(sparse_storage&& other) noexcept {
				// This is a newly constructed object.
				// It can't have any memory allocated, yet.
				GAIA_ASSERT(m_dense.data() == nullptr);

				m_dense = GAIA_MOV(other.m_dense);
				m_pages = GAIA_MOV(other.m_pages);
				m_cnt = other.m_cnt;

				other.m_dense = {};
				other.m_pages = {};
				other.m_cnt = size_type(0);
			}

			sparse_storage& operator=(sparse_storage&& other) noexcept {
				GAIA_ASSERT(core::addressof(other) != this);

				m_dense = GAIA_MOV(other.m_dense);
				m_pages = GAIA_MOV(other.m_pages);
				m_cnt = other.m_cnt;

				other.m_dense = {};
				other.m_pages = {};
				other.m_cnt = size_type(0);

				return *this;
			}

			~sparse_storage() = default;

			GAIA_CLANG_WARNING_PUSH()
			// Memory is aligned so we can silence this warning
			GAIA_CLANG_WARNING_DISABLE("-Wcast-align")

			GAIA_NODISCARD decltype(auto) operator[](size_type sid) noexcept {
				GAIA_ASSERT(has(sid));
				const auto pid = sid >> to_page_index;
				const auto did = sid & (PageCapacity - 1);

				auto& page = m_pages[pid];
				return view_policy::set({(typename view_policy::TargetCastType)page.data(), PageCapacity}, did);
			}

			GAIA_NODISCARD decltype(auto) operator[](size_type sid) const noexcept {
				GAIA_ASSERT(has(sid));
				const auto pid = sid >> to_page_index;
				const auto did = sid & (PageCapacity - 1);

				auto& page = m_pages[pid];
				return view_policy::get({(typename view_policy::TargetCastType)page.data(), PageCapacity}, did);
			}

			GAIA_CLANG_WARNING_POP()

			//! Checks if an item with a given sparse id \param sid exists
			GAIA_NODISCARD bool has(sparse_id sid) const {
				if (sid == detail::InvalidId)
					return false;

				const auto pid = sid >> to_page_index;
				if (pid >= m_pages.size())
					return false;

				const auto did = sid & (PageCapacity - 1);
				const auto id = m_pages[pid].get_id(did);
				return id != detail::InvalidId;
			}

			//! Checks if an item \param arg exists within the storage
			GAIA_NODISCARD bool has(const T& arg) const {
				const auto sid = to_sparse_id<T>::get(arg);
				GAIA_ASSERT(sid != detail::InvalidId);
				return has(sid);
			}

			//! Inserts the item \param arg into the storage.
			//! \return Reference to the inserted record or nothing in case it is has a SoA layout.
			decltype(auto) add(const T& arg) {
				const auto sid = to_sparse_id<T>::get(arg);
				GAIA_ASSERT(sid != detail::InvalidId);
				if (has(sid)) {
					if constexpr (mem::is_soa_layout_v<T>)
						return;
					else {
						const auto pid = sid >> to_page_index;
						const auto did = sid & (PageCapacity - 1);
						auto& page = m_pages[pid];
						return page.set_data(did);
					}
				}

				const auto pid = sid >> to_page_index;
				const auto did = sid & (PageCapacity - 1);

				try_grow(pid);
				m_dense[m_cnt] = sid;

				auto& page = m_pages[pid];
				page.set_id(did) = m_cnt++;
				if constexpr (mem::is_soa_layout_v<T>)
					page.add_data(did, arg);
				else
					return page.add_data(did, arg);
			}

			//! Inserts the item \param arg into the storage.
			//! \return Reference to the inserted record or nothing in case it is has a SoA layout.
			decltype(auto) add(T&& arg) {
				const auto sid = to_sparse_id<T>::get(arg);
				if (has(sid)) {
					if constexpr (mem::is_soa_layout_v<T>)
						return;
					else {
						const auto pid = sid >> to_page_index;
						const auto did = sid & (PageCapacity - 1);
						auto& page = m_pages[pid];
						return page.set_data(did);
					}
				}

				const auto pid = sid >> to_page_index;
				const auto did = sid & (PageCapacity - 1);

				try_grow(pid);
				m_dense[m_cnt] = sid;

				auto& page = m_pages[pid];
				page.set_id(did) = m_cnt++;
				if constexpr (mem::is_soa_layout_v<T>)
					page.add_data(did, GAIA_MOV(arg));
				else
					return page.add_data(did, GAIA_MOV(arg));
			}

			//! Update the record at the index \param sid.
			//! \return Reference to the inserted record or nothing in case it is has a SoA layout.
			decltype(auto) set(sparse_id sid) {
				GAIA_ASSERT(has(sid));

				const auto pid = sid >> to_page_index;
				const auto did = sid & (PageCapacity - 1);

				auto& page = m_pages[pid];
				return page.set_data(did);
			}

			//! Removes the item at the index \param sid from the storage.
			void del(sparse_id sid) noexcept {
				GAIA_ASSERT(!empty());
				GAIA_ASSERT(sid != detail::InvalidId);

				if (!has(sid))
					return;

				const auto pid = sid >> to_page_index;
				const auto did = sid & (PageCapacity - 1);

				const auto sidLast = m_dense[m_cnt - 1];
				const auto didLast = sidLast & (PageCapacity - 1);

				auto& page = m_pages[pid];
				const auto id = page.get_id(did);
				page.set_id(didLast) = id;
				page.set_id(did) = detail::InvalidId;
				page.del_data(did);
				m_dense[id] = sidLast;
				m_dense.resize(m_cnt - 1);

				--m_cnt;
			}

			//! Removes the item \param arg from the storage.
			void del(const T& arg) noexcept {
				const auto sid = to_sparse_id<T>::get(arg);
				return del(sid);
			}

			//! Clears the storage
			void clear() {
				m_dense.resize(0);
				m_pages.resize(0);
				m_cnt = 0;
			}

			//! Returns the number of items inserted into the storage
			GAIA_NODISCARD size_type size() const noexcept {
				return m_cnt;
			}

			//! Checks if the storage is empty (no items inserted)
			GAIA_NODISCARD bool empty() const noexcept {
				return size() == 0;
			}

			GAIA_NODISCARD decltype(auto) front() noexcept {
				GAIA_ASSERT(!empty());
				if constexpr (mem::is_soa_layout_v<T>)
					return *begin();
				else
					return (reference)*begin();
			}

			GAIA_NODISCARD decltype(auto) front() const noexcept {
				GAIA_ASSERT(!empty());
				if constexpr (mem::is_soa_layout_v<T>)
					return *begin();
				else
					return (const_reference)*begin();
			}

			GAIA_NODISCARD decltype(auto) back() noexcept {
				GAIA_ASSERT(!empty());

				const auto sid = m_dense[m_cnt - 1];
				const auto pid = sid >> to_page_index;
				const auto did = sid & (PageCapacity - 1);

				if constexpr (mem::is_soa_layout_v<T>)
					return m_pages[pid].set_data(did);
				else
					return (reference)m_pages[pid].set_data(did);
			}

			GAIA_NODISCARD decltype(auto) back() const noexcept {
				GAIA_ASSERT(!empty());

				const auto sid = m_dense[m_cnt - 1];
				const auto pid = sid >> to_page_index;
				const auto did = sid & (PageCapacity - 1);

				if constexpr (mem::is_soa_layout_v<T>)
					return m_pages[pid].get_data(did);
				else
					return (const_reference)m_pages[pid].get_data(did);
			}

			GAIA_NODISCARD auto begin() const noexcept {
				GAIA_ASSERT(!empty());

				if constexpr (mem::is_soa_layout_v<T>)
					return iterator_soa(m_dense.data(), size(), 0);
				else
					return iterator(m_dense.data(), m_pages.data());
			}

			GAIA_NODISCARD auto end() const noexcept {
				GAIA_ASSERT(!empty());

				if constexpr (mem::is_soa_layout_v<T>)
					return iterator_soa(m_dense.data() + m_cnt, size(), size());
				else
					return iterator(m_dense.data() + size(), m_pages.data());
			}

			GAIA_NODISCARD bool operator==(const sparse_storage& other) const {
				// The number of items needs to be the same
				if (m_cnt != other.m_cnt)
					return false;

				// Dense indices need to be the same.
				// We don't check m_sparse, because it m_dense doesn't
				// match, m_sparse will be different as well.
				if (m_dense != other.m_dense)
					return false;

				// Check data one-by-one.
				// We don't compare the entire array, only the actually stored values,
				// because their is possible a lot of empty space in the data array (it is sparse).
				const size_type n = size();
				for (size_type i = 0, cnt = 0; i < n && cnt < m_cnt; ++i, ++cnt) {
					const auto sid = m_dense[i];
					const auto pid = sid >> to_page_index;
					const auto did = sid & (PageCapacity - 1);

					const auto& item0 = m_pages[pid].get_data(did);
					const auto& item1 = m_pages[pid].get_data(did);

					if (!(item0 == item1))
						return false;
				}
				return true;
			}

			GAIA_NODISCARD constexpr bool operator!=(const sparse_storage& other) const {
				return !operator==(other);
			}
		};

		//! Array with variable size of elements of type \tparam T allocated on heap.
		//! Allocates enough memory to support \tparam PageCapacity elements.
		//! Uses \tparam Allocator to allocate memory.
		//! This version is optimized for tags (data of zero size).
		template <typename T, uint32_t PageCapacity, typename Allocator>
		class sparse_storage<T, PageCapacity, Allocator, std::enable_if_t<std::is_empty_v<T>>> {
		public:
			using value_type = T;
			using reference = T&;
			using const_reference = const T&;
			using pointer = T*;
			using const_pointer = T*;
			using view_policy = mem::auto_view_policy<T>;
			using difference_type = detail::difference_type;
			using size_type = detail::size_type;

			using iterator = sparse_iterator<T, PageCapacity, Allocator, std::enable_if_t<std::is_empty_v<T>>>;
			using page_type = detail::sparse_page<T, PageCapacity, Allocator, std::enable_if_t<std::is_empty_v<T>>>;

		private:
			constexpr static detail::size_type to_page_index = core::count_bits(PageCapacity);

			//! Contains mappings to sparse storage inside pages
			cnt::darray<sparse_id> m_dense;
			//! Contains pages with data and sparse–>dense mapping
			cnt::darray<page_type> m_pages;
			//! Current number of items tracked by the sparse set
			size_type m_cnt = size_type(0);

			void try_grow(uint32_t pid) {
				m_dense.resize(m_cnt + 1);

				// The spare array has to be able to take any sparse index
				if (pid >= m_pages.size())
					m_pages.resize(pid + 1);

				m_pages[pid].add();
			}

		public:
			constexpr sparse_storage() noexcept = default;

			sparse_storage(const sparse_storage& other) {
				GAIA_ASSERT(core::addressof(other) != this);

				m_dense = other.m_dense;
				m_pages = other.m_pages;
				m_cnt = other.m_cnt;
			}

			sparse_storage& operator=(const sparse_storage& other) {
				GAIA_ASSERT(core::addressof(other) != this);

				m_dense = other.m_dense;
				m_pages = other.m_pages;
				m_cnt = other.m_cnt;

				return *this;
			}

			sparse_storage(sparse_storage&& other) noexcept {
				// This is a newly constructed object.
				// It can't have any memory allocated, yet.
				GAIA_ASSERT(m_dense.data() == nullptr);

				m_dense = GAIA_MOV(other.m_dense);
				m_pages = GAIA_MOV(other.m_pages);
				m_cnt = other.m_cnt;

				other.m_dense = {};
				other.m_pages = {};
				other.m_cnt = size_type(0);
			}

			sparse_storage& operator=(sparse_storage&& other) noexcept {
				GAIA_ASSERT(core::addressof(other) != this);

				m_dense = GAIA_MOV(other.m_dense);
				m_pages = GAIA_MOV(other.m_pages);
				m_cnt = other.m_cnt;

				other.m_dense = {};
				other.m_pages = {};
				other.m_cnt = size_type(0);

				return *this;
			}

			~sparse_storage() = default;

			//! Checks if an item with a given sparse id \param sid exists
			GAIA_NODISCARD bool has(sparse_id sid) const {
				if (sid == detail::InvalidId)
					return false;

				const auto pid = sid >> to_page_index;
				if (pid >= m_pages.size())
					return false;

				const auto did = sid & (PageCapacity - 1);
				const auto id = m_pages[pid].get_id(did);
				return id != detail::InvalidId;
			}

			//! Inserts the item \param arg into the storage.
			//! \return Reference to the inserted record or nothing in case it is has a SoA layout.
			void add(sparse_id sid) {
				if (has(sid))
					return;

				const auto pid = sid >> to_page_index;
				const auto did = sid & (PageCapacity - 1);

				try_grow(pid);
				m_dense[m_cnt] = sid;

				auto& page = m_pages[pid];
				page.set_id(did) = m_cnt++;
			}

			//! Removes the item at the index \param sid from the storage.
			void del(sparse_id sid) noexcept {
				GAIA_ASSERT(!empty());
				GAIA_ASSERT(sid != detail::InvalidId);

				if (!has(sid))
					return;

				const auto pid = sid >> to_page_index;
				const auto did = sid & (PageCapacity - 1);

				const auto sidLast = m_dense[m_cnt - 1];
				const auto didLast = sidLast & (PageCapacity - 1);

				auto& page = m_pages[pid];
				const auto id = page.get_id(did);
				page.set_id(didLast) = id;
				page.set_id(did) = detail::InvalidId;
				m_dense[id] = sidLast;
				m_dense.resize(m_cnt - 1);

				--m_cnt;
			}

			//! Clears the storage
			void clear() {
				m_dense.resize(0);
				m_pages.resize(0);
				m_cnt = 0;
			}

			//! Returns the number of items inserted into the storage
			GAIA_NODISCARD size_type size() const noexcept {
				return m_cnt;
			}

			//! Checks if the storage is empty (no items inserted)
			GAIA_NODISCARD bool empty() const noexcept {
				return size() == 0;
			}

			GAIA_NODISCARD decltype(auto) front() noexcept {
				GAIA_ASSERT(!empty());
				return (reference)*begin();
			}

			GAIA_NODISCARD decltype(auto) front() const noexcept {
				GAIA_ASSERT(!empty());
				return (const_reference)*begin();
			}

			GAIA_NODISCARD decltype(auto) back() noexcept {
				GAIA_ASSERT(!empty());

				const auto sid = m_dense[m_cnt - 1];
				const auto pid = sid >> to_page_index;
				const auto did = sid & (PageCapacity - 1);

				return (reference)m_pages[pid].set_id(did);
			}

			GAIA_NODISCARD decltype(auto) back() const noexcept {
				GAIA_ASSERT(!empty());

				const auto sid = m_dense[m_cnt - 1];
				const auto pid = sid >> to_page_index;
				const auto did = sid & (PageCapacity - 1);

				return (const_reference)m_pages[pid].get_id(did);
			}

			GAIA_NODISCARD auto begin() const noexcept {
				GAIA_ASSERT(!empty());
				return iterator(m_dense.data());
			}

			GAIA_NODISCARD auto end() const noexcept {
				GAIA_ASSERT(!empty());
				return iterator(m_dense.data() + size());
			}

			GAIA_NODISCARD bool operator==(const sparse_storage& other) const {
				// The number of items needs to be the same
				if (m_cnt != other.m_cnt)
					return false;

				// Dense indices need to be the same.
				// We don't check m_sparse, because it m_dense doesn't
				// match, m_sparse will be different as well.
				if (m_dense != other.m_dense)
					return false;

				return true;
			}

			GAIA_NODISCARD constexpr bool operator!=(const sparse_storage& other) const {
				return !operator==(other);
			}
		};
	} // namespace cnt

} // namespace gaia
/*** End of inlined file: sparse_storage.h ***/


/*** Start of inlined file: sringbuffer.h ***/
#pragma once
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
				GAIA_ASSERT(core::addressof(other) != this);

				mem::move_elements<T>(m_data, other.m_data, other.size(), 0, extent, other.extent);

				other.m_tail = size_type(0);
				other.m_size = size_type(0);
			}

			sringbuffer& operator=(std::initializer_list<T> il) {
				*this = sringbuffer(il.begin(), il.end());
				return *this;
			}

			constexpr sringbuffer& operator=(const sringbuffer& other) {
				GAIA_ASSERT(core::addressof(other) != this);

				mem::copy_elements<T>((uint8_t*)&m_data[0], other.m_data, other.size(), 0, extent, other.extent);

				m_tail = other.m_tail;
				m_size = other.m_size;

				return *this;
			}

			constexpr sringbuffer& operator=(sringbuffer&& other) noexcept {
				GAIA_ASSERT(core::addressof(other) != this);

				mem::move_elements<T>(m_data, other.m_data, other.size(), 0, extent, other.extent);

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
				m_data[head] = GAIA_MOV(arg);
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
				out = GAIA_FWD(m_data[m_tail]);
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
				out = GAIA_FWD(m_data[head]);
				--m_size;
			}

			GAIA_NODISCARD constexpr size_type size() const noexcept {
				return m_size;
			}

			GAIA_NODISCARD constexpr bool empty() const noexcept {
				return size() == 0;
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
				for (size_type i = 0; i < N; ++i) {
					if (m_data[i] == other.m_data[i])
						return false;
				}
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

/*** End of inlined file: sringbuffer.h ***/


/*** Start of inlined file: signal.h ***/
#pragma once

// TODO: Needed only for std::invoke. Possibly replace it somehow?
#include <functional>

#include <tuple>
#include <typeinfo>
#include <utility>

namespace gaia {
	namespace util {
		namespace detail {
			template <typename Ret, typename... Args>
			auto func_ptr(Ret (*)(Args...)) -> Ret (*)(Args...);

			template <typename Ret, typename Type, typename... Args, typename Other>
			auto func_ptr(Ret (*)(Type, Args...), Other&&) -> Ret (*)(Args...);

			template <typename Class, typename Ret, typename... Args, typename... Other>
			auto func_ptr(Ret (Class::*)(Args...), Other&&...) -> Ret (*)(Args...);

			template <typename Class, typename Ret, typename... Args, typename... Other>
			auto func_ptr(Ret (Class::*)(Args...) const, Other&&...) -> Ret (*)(Args...);

			template <typename Class, typename Type, typename... Other>
			auto func_ptr(Type Class::*, Other&&...) -> Type (*)();

			template <typename... Type>
			using func_ptr_t = decltype(detail::func_ptr(std::declval<Type>()...));

			template <typename... Class, typename Ret, typename... Args>
			GAIA_NODISCARD constexpr auto index_sequence_for(Ret (*)(Args...)) {
				return std::index_sequence_for<Class..., Args...>{};
			}

			// Wraps a function or a member of a specified type
			template <auto>
			struct connect_arg_t {};
			// Function wrapper
			template <auto Func>
			inline constexpr connect_arg_t<Func> connect_arg{};
		} // namespace detail

		template <typename>
		class delegate;
		template <typename Function>
		class signal;
		template <typename Function>
		class sink;

		//------------------------------------------------------------------------------
		// delegate
		//------------------------------------------------------------------------------

		//! Delegate for function pointers and members.
		//! It can be used as a general-purpose invoker for any free function with no memory overhead.
		//! \warning The delegate isn't responsible for the connected object or its context.
		//!          User is in charge of disconnecting instances before deleting them and
		//!          guarantee the lifetime of the instance is longer than that of the delegate.
		//! \tparam Ret Return type of a function type.
		//! \tparam Args Types of arguments of a function type.
		template <typename Ret, typename... Args>
		class delegate<Ret(Args...)> {
			using func_type = Ret(const void*, Args...);

			func_type* m_fnc = nullptr;
			const void* m_ctx = nullptr;

		public:
			delegate() noexcept = default;

			//! Constructs a delegate by binding a free function or an unbound member to it.
			//! \tparam FuncToBind Function or member to bind to the delegate.
			template <auto FuncToBind>
			delegate(detail::connect_arg_t<FuncToBind>) noexcept {
				bind<FuncToBind>();
			}

			//! Constructs a delegate by binding a free function with context or a bound member to it.
			//! \tparam FuncToBind Function or member to bind to the delegate.
			//! \tparam Type Type of class or type of context.
			//! \param value_or_instance A valid object that fits the purpose.
			template <auto FuncToBind, typename Type>
			delegate(detail::connect_arg_t<FuncToBind>, Type&& value_or_instance) noexcept {
				bind<FuncToBind>(GAIA_FWD(value_or_instance));
			}

			//! Constructs a delegate by binding a function with optional context to it.
			//! \param func Function to bind to the delegate.
			//! \param data User defined arbitrary data.
			delegate(func_type* func, const void* data = nullptr) noexcept {
				bind(func, data);
			}

			//! Binds a free function or an unbound member to a delegate.
			//! \tparam FuncToBind Function or member to bind to the delegate.
			template <auto FuncToBind>
			void bind() noexcept {
				m_ctx = nullptr;

				if constexpr (std::is_invocable_r_v<Ret, decltype(FuncToBind), Args...>) {
					m_fnc = [](const void*, Args... args) {
						return Ret(std::invoke(FuncToBind, GAIA_FWD(args)...));
					};
				} else if constexpr (std::is_member_pointer_v<decltype(FuncToBind)>) {
					m_fnc = wrap<FuncToBind>(detail::index_sequence_for<std::tuple_element_t<0, std::tuple<Args...>>>(
							detail::func_ptr_t<decltype(FuncToBind)>{}));
				} else {
					m_fnc = wrap<FuncToBind>(detail::index_sequence_for(detail::func_ptr_t<decltype(FuncToBind)>{}));
				}
			}

			//! Binds a free function with context or a bound member to a delegate. When used to bind a ree function with
			//! context, its signature must be such that the instance is the first argument before the ones used to define the
			//! delegate itself.
			//! \tparam FuncToBind Function or member to bind to the delegate.
			//! \tparam Type Type of class or type of context.
			//! \param value_or_instance* A valid* reference that fits the purpose.
			template <auto FuncToBind, typename Type>
			void bind(Type& value_or_instance) noexcept {
				m_ctx = &value_or_instance;

				if constexpr (std::is_invocable_r_v<Ret, decltype(FuncToBind), Type&, Args...>) {
					using const_or_not_type = std::conditional_t<std::is_const_v<Type>, const void*, void*>;
					m_fnc = [](const void* ctx, Args... args) {
						auto pType = static_cast<Type*>(const_cast<const_or_not_type>(ctx));
						return Ret(std::invoke(FuncToBind, *pType, GAIA_FWD(args)...));
					};
				} else {
					m_fnc = wrap<FuncToBind>(
							value_or_instance, detail::index_sequence_for(detail::func_ptr_t<decltype(FuncToBind), Type>{}));
				}
			}

			//! Binds a free function with context or a bound member to a delegate.
			//! \tparam FuncToBind Function or member to bind to the delegate.
			//! \tparam Type Type of class or type of context.
			//! \param value_or_instance A valid pointer that fits the purpose.
			template <auto FuncToBind, typename Type>
			void bind(Type* value_or_instance) noexcept {
				m_ctx = value_or_instance;

				if constexpr (std::is_invocable_r_v<Ret, decltype(FuncToBind), Type*, Args...>) {
					using const_or_not_type = std::conditional_t<std::is_const_v<Type>, const void*, void*>;
					m_fnc = [](const void* ctx, Args... args) {
						auto pType = static_cast<Type*>(const_cast<const_or_not_type>(ctx));
						return Ret(std::invoke(FuncToBind, pType, GAIA_FWD(args)...));
					};
				} else {
					m_fnc = wrap<FuncToBind>(
							value_or_instance, detail::index_sequence_for(detail::func_ptr_t<decltype(FuncToBind), Type>{}));
				}
			}

			//! Binds an user defined function with optional context to a delegate.
			//! The context is returned as the first argument to the target function in all cases.
			//! \param function Function to bind* to* the delegate.
			//! \param context User defined arbitrary ctx.
			void bind(func_type* function, const void* context = nullptr) noexcept {
				m_fnc = function;
				m_ctx = context;
			}

			//! Resets a delegate. After a reset, a delegate cannot be invoked anymore.
			void reset() noexcept {
				m_fnc = nullptr;
				m_ctx = nullptr;
			}

			//! Returns the functor pointer linked to a delegate, if any.
			//! \return An opaque pointer to the underlying data.
			GAIA_NODISCARD bool has_func() const noexcept {
				return m_fnc != nullptr;
			}

			//! Returns the instance or the context linked to a delegate, if any.
			//! \return An opaque pointer to the underlying data.
			GAIA_NODISCARD const void* instance() const noexcept {
				return m_ctx;
			}

			//! The delegate invokes the underlying function and returns the result.
			//! \param args* Arguments* to use to invoke the underlying function.
			//! \return The value returned by the underlying * function.
			Ret operator()(Args... args) const {
				GAIA_ASSERT(m_fnc != nullptr && "Trying to call an unbound delegate!");
				return m_fnc(m_ctx, GAIA_FWD(args)...);
			}

			//! Checks whether a delegate actually points to something.
			//! \return False if the delegate is empty, true otherwise.
			GAIA_NODISCARD explicit operator bool() const noexcept {
				// There's no way to set just m_ctx so it's enough to test m_fnc
				return m_fnc != nullptr;
			}

			//! Compares the contents of two delegates.
			//! \param other Delegate with which to compare.
			//! \return True if the two contents are equal, false otherwise.
			GAIA_NODISCARD bool operator==(const delegate<Ret(Args...)>& other) const noexcept {
				return m_fnc == other.m_fnc && m_ctx == other.m_ctx;
			}

			//! Compares the contents of two delegates.
			//! \param other Delegate with which to compare.
			//! \return True if the two contents differ, false otherwise.
			GAIA_NODISCARD bool operator!=(const delegate<Ret(Args...)>& other) const noexcept {
				return !operator==(other);
			}

		private:
			template <auto FuncToBind, std::size_t... Index>
			GAIA_NODISCARD auto wrap(std::index_sequence<Index...>) noexcept {
				return [](const void*, Args... args) {
					[[maybe_unused]] const auto argsFwd = std::forward_as_tuple(GAIA_FWD(args)...);
					return Ret(std::invoke(FuncToBind, GAIA_FWD(std::get<Index>(argsFwd))...));
				};
			}

			template <auto FuncToBind, typename Type, std::size_t... Index>
			GAIA_NODISCARD auto wrap(Type&, std::index_sequence<Index...>) noexcept {
				using const_or_not_type = std::conditional_t<std::is_const_v<Type>, const void*, void*>;
				return [](const void* ctx, Args... args) {
					[[maybe_unused]] const auto argsFwd = std::forward_as_tuple(GAIA_FWD(args)...);
					auto pType = static_cast<Type*>(const_cast<const_or_not_type>(ctx));
					return Ret(std::invoke(FuncToBind, *pType, GAIA_FWD(std::get<Index>(argsFwd))...));
				};
			}

			template <auto FuncToBind, typename Type, std::size_t... Index>
			GAIA_NODISCARD auto wrap(Type*, std::index_sequence<Index...>) noexcept {
				using const_or_not_type = std::conditional_t<std::is_const_v<Type>, const void*, void*>;
				return [](const void* ctx, Args... args) {
					[[maybe_unused]] const auto argsFwd = std::forward_as_tuple(GAIA_FWD(args)...);
					auto pType = static_cast<Type*>(const_cast<const_or_not_type>(ctx));
					return Ret(std::invoke(FuncToBind, pType, GAIA_FWD(std::get<Index>(argsFwd))...));
				};
			}
		};

		//! Compares the contents of two delegates.
		//! \tparam Ret Return type of a function type.
		//! \tparam Args Types of arguments of a function type.
		//! \param lhs A valid delegate object.
		//! \param rhs A valid delegate object.
		//! \return True if the two contents are equal, false otherwise.
		template <typename Ret, typename... Args>
		GAIA_NODISCARD bool operator==(const delegate<Ret(Args...)>& lhs, const delegate<Ret(Args...)>& rhs) noexcept {
			return lhs == rhs;
		}

		//! Compares the contents of two delegates.
		//! \tparam Ret Return type of a function type.
		//! \tparam Args Types of arguments of a function type.
		//! \param lhs A valid delegate object.
		//! \param rhs A valid delegate object.
		//! \return True if the two contents differ, false otherwise.
		template <typename Ret, typename... Args>
		GAIA_NODISCARD bool operator!=(const delegate<Ret(Args...)>& lhs, const delegate<Ret(Args...)>& rhs) noexcept {
			return lhs != rhs;
		}

		template <auto Func>
		delegate(detail::connect_arg_t<Func>) noexcept
				-> delegate<std::remove_pointer_t<detail::func_ptr_t<decltype(Func)>>>;

		template <auto Func, typename Type>
		delegate(detail::connect_arg_t<Func>, Type&&) noexcept
				-> delegate<std::remove_pointer_t<detail::func_ptr_t<decltype(Func), Type>>>;

		template <typename Ret, typename... Args>
		delegate(Ret (*)(const void*, Args...), const void* = nullptr) noexcept -> delegate<Ret(Args...)>;

		//------------------------------------------------------------------------------
		// signal
		//------------------------------------------------------------------------------

		template <typename Ret, typename... Args>
		class signal<Ret(Args...)>;

		namespace detail {
			template <typename Ret, typename... Args>
			using container = cnt::darray<delegate<Ret(Args...)>>;
		} // namespace detail

		//! Signal is a container of listener which it can notify.
		//! It works directly with references to classes and pointers to both free and member functions.
		//! \tparam Ret Return type of a function type.
		//! \tparam* Args Types of arguments of a function type.
		template <typename Ret, typename... Args>
		class signal<Ret(Args...)> {
			friend class sink<Ret(Args...)>;

		private:
			//! Container storing listeners
			detail::container<Ret, Args...> m_listeners;

		public:
			using size_type = typename detail::container<Ret, Args...>::size_type;
			using sink_type = sink<Ret(Args...)>;

			//! Number of listeners connected to the signal.
			//! \return Number of listeners currently connected.
			GAIA_NODISCARD size_type size() const noexcept {
				return m_listeners.size();
			}

			//! Check is there is any listener bound to the signal.
			//! \return True if there is any listener bound to the signal, false otherwise.
			GAIA_NODISCARD bool empty() const noexcept {
				return m_listeners.empty();
			}

			//! Signals all listeners.
			//! \param args Arguments to use to trigger listeners.
			void emit(Args... args) {
				for (auto&& call: std::as_const(m_listeners))
					call(args...);
			}
		};

		//------------------------------------------------------------------------------
		// Sink
		//------------------------------------------------------------------------------

		//! Sink is a helper class used to bind listeners to signals.
		//! The separation between signal and sink makes it possible to store signals as private data members
		//! without exposing their invoke functionality to the users of the class.
		//! It also allows for less header files contention because any binding can (and should) happen in source files.
		//! \warning Lifetime of any sink must not be longer than that of the signal to which it refers.
		//! \tparam Ret Return type of a function type.
		//! \tparam Args Types of arguments of a function type.
		template <typename Ret, typename... Args>
		class sink<Ret(Args...)> {
			using signal_type = signal<Ret(Args...)>;
			using func_type = Ret(const void*, Args...);

			signal_type* m_s;

		public:
			//! Constructs a sink that is allowed to modify a given signal.
			//! \param ref A valid reference to a signal object.
			sink(signal<Ret(Args...)>& ref) noexcept: m_s{&ref} {}

			//! Moves signals from another sink to this one. Result is stored in this object.
			//! The sink we merge from is cleared.
			//! \param other Sink to move signals from
			//! \param compact If true the detail container is reallocated so as little memory as possible is used by it.
			void move_from(sink& other) {
				m_s->m_listeners.reserve(m_s->m_listeners.size() + other.m_s->m_listeners.size());
				for (auto&& listener: other.m_s->m_listeners)
					m_s->m_listeners.push_back(GAIA_MOV(listener));

				other.reset();
			}

			//! Binds a free function or an unbound member to a signal.
			//! The signal handler performs checks to avoid multiple connections for the same function.
			//! \tparam FuncToBind Function or member to bind to the signal.
			template <auto FuncToBind>
			void bind() {
				delegate<Ret(Args...)> call{};
				call.template bind<FuncToBind>();
				bind_internal(call);
			}

			//! Binds a free function with context or a bound member to a signal. When used to bind a free function with
			//! context, its signature must be such that the instance is the first argument before the ones used to define the
			//! signal itself.
			//! \tparam FuncToBind Function or member to bind to the signal.
			//! \tparam Type Type of class or type of context.
			//! \param value_or_instance A valid object that fits the purpose.
			template <auto FuncToBind, typename Type>
			void bind(Type& value_or_instance) {
				delegate<Ret(Args...)> call{};
				call.template bind<FuncToBind>(value_or_instance);
				bind_internal(call);
			}

			//! Binds an user defined function with optional context to a signal.
			//! The context is returned as the first  argument to the target function in all cases.
			//! \param func Function to bind to the signal.
			//! \param data User defined arbitrary ctx.
			void bind(func_type* func, const void* data = nullptr) {
				if (!func && data == nullptr)
					return;

				delegate<Ret(Args...)> call{};
				call.bind(func, data);
				bind_internal(call);
			}

			//! Unbinds a free function or an unbound member from a signal.
			//! \tparam FuncToUnbind* Function* or member to unbind from the signal.
			template <auto FuncToUnbind>
			void unbind() {
				delegate<Ret(Args...)> call{};
				call.template bind<FuncToUnbind>();

				m_s->m_listeners.retain([&](const auto& l) {
					return l != call;
				});
			}

			//! Unbinds a free function with context or a bound member from a signal.
			//! \tparam** FuncToUnbind Function or member to unbind from the signal.
			//! \tparam Type Type of class or type of** context.
			//! \param value_or_instance A valid object that fits the purpose.
			template <auto FuncToUnbind, typename Type>
			void unbind(Type& value_or_instance) {
				delegate<Ret(Args...)> call{};
				call.template bind<FuncToUnbind>(value_or_instance);

				auto& listeners = m_s->m_listeners;
				for (uint32_t i = 0; i < listeners.size();) {
					if (listeners[i] != call) {
						++i;
						continue;
					}

					core::swap_erase_unsafe(listeners, i);
				}
			}

			//! Unbinds a free function with context or bound members from a signal.
			//! \tparam Type Type of class or type of context.
			//! \param value_or_instance A valid object that fits the purpose.
			template <typename Type>
			void unbind(Type* value_or_instance) {
				if (!value_or_instance)
					return;

				auto& listeners = m_s->m_listeners;
				for (uint32_t i = 0; i < listeners.size();) {
					if (listeners[i].instance() != value_or_instance) {
						++i;
						continue;
					}

					core::swap_erase_unsafe(listeners, i);
				}
			}

			//! Unbinds a free function with context or bound members from a signal.
			//! \tparam Type Type of class or type of context.
			//! \param value_or_instance A valid object that fits the purpose.
			template <typename Type>
			void unbind(Type& value_or_instance) {
				unbind(&value_or_instance);
			}

			//! Unbinds all listeners from a signal.
			void reset() {
				m_s->m_listeners.clear();
			}

		private:
			void bind_internal(const delegate<Ret(Args...)>& call) {
				if (!core::has(m_s->m_listeners, call))
					m_s->m_listeners.push_back(GAIA_MOV(call));
			}
		};

		template <typename Ret, typename... Args>
		sink(signal<Ret(Args...)>&) noexcept -> sink<Ret(Args...)>;
	} // namespace util
} // namespace gaia

/*** End of inlined file: signal.h ***/


/*** Start of inlined file: serialization.h ***/
#pragma once

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
			constexpr auto size(C&& c) noexcept -> decltype(c.size()) {
				return c.size();
			}
			template <typename T, auto N>
			constexpr std::size_t size(const T (&)[N]) noexcept {
				return N;
			}

			template <typename C>
			constexpr auto data(C&& c) noexcept -> decltype(c.data()) {
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
			struct is_int_kind_id:
					std::disjunction<
							std::is_same<T, int8_t>, std::is_same<T, uint8_t>, //
							std::is_same<T, int16_t>, std::is_same<T, uint16_t>, //
							std::is_same<T, int32_t>, std::is_same<T, uint32_t>, //
							std::is_same<T, int64_t>, std::is_same<T, uint64_t>, //
							std::is_same<T, bool>> {};

			template <typename T>
			struct is_flt_kind_id:
					std::disjunction<
							// std::is_same<T, float8_t>, //
							// std::is_same<T, float16_t>, //
							std::is_same<T, float>, //
							std::is_same<T, double>, //
							std::is_same<T, long double>> {};

			template <typename T>
			GAIA_NODISCARD constexpr serialization_type_id int_kind_id() {
				static_assert(is_int_kind_id<T>::value, "Unsupported integral type");

				if constexpr (std::is_same_v<int8_t, T>) {
					return serialization_type_id::s8;
				} else if constexpr (std::is_same_v<uint8_t, T>) {
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
				} else { // if constexpr (std::is_same_v<bool, T>) {
					return serialization_type_id::b;
				}
			}

			template <typename T>
			GAIA_NODISCARD constexpr serialization_type_id flt_type_id() {
				static_assert(is_flt_kind_id<T>::value, "Unsupported floating type");

				// if constexpr (std::is_same_v<float8_t, T>) {
				// 	return serialization_type_id::f8;
				// } else if constexpr (std::is_same_v<float16_t, T>) {
				// 	return serialization_type_id::f16;
				// } else
				if constexpr (std::is_same_v<float, T>) {
					return serialization_type_id::f32;
				} else if constexpr (std::is_same_v<double, T>) {
					return serialization_type_id::f64;
				} else { // if constexpr (std::is_same_v<long double, T>) {
					return serialization_type_id::f128;
				}
			}

			template <typename T>
			GAIA_NODISCARD constexpr serialization_type_id type_id() {
				if constexpr (std::is_enum_v<T>)
					return int_kind_id<std::underlying_type_t<T>>();
				else if constexpr (std::is_integral_v<T>)
					return int_kind_id<T>();
				else if constexpr (std::is_floating_point_v<T>)
					return flt_type_id<T>();
				else if constexpr (has_data_and_size<T>::value)
					return serialization_type_id::data_and_size;
				else if constexpr (std::is_class_v<T>)
					return serialization_type_id::trivial_wrapper;

				return serialization_type_id::Last;
			}

			template <typename T>
			GAIA_NODISCARD constexpr uint32_t bytes_one(const T& item) noexcept {
				using U = core::raw_t<T>;

				constexpr auto id = type_id<U>();
				static_assert(id != serialization_type_id::Last);
				uint32_t size_in_bytes{};

				// Custom bytes() has precedence
				if constexpr (has_bytes<U>::value) {
					size_in_bytes = (uint32_t)item.bytes();
				}
				// Trivially serializable types
				else if constexpr (is_trivially_serializable<U>::value) {
					size_in_bytes = (uint32_t)sizeof(U);
				}
				// Types which have data() and size() member functions
				else if constexpr (has_data_and_size<U>::value) {
					size_in_bytes = (uint32_t)item.size();
				}
				// Classes
				else if constexpr (std::is_class_v<U>) {
					meta::each_member(item, [&](auto&&... items) {
						size_in_bytes += (bytes_one(items) + ...);
					});
				} else
					static_assert(!sizeof(U), "Type is not supported for serialization, yet");

				return size_in_bytes;
			}

			template <bool Write, typename Serializer, typename T>
			void ser_data_one(Serializer& s, T&& arg) {
				using U = core::raw_t<T>;

				// Custom save() & load() have precedence
				if constexpr (Write && has_save<U, Serializer&>::value) {
					arg.save(s);
				} else if constexpr (!Write && has_load<U, Serializer&>::value) {
					arg.load(s);
				}
				// Trivially serializable types
				else if constexpr (is_trivially_serializable<U>::value) {
					if constexpr (Write)
						s.save(GAIA_FWD(arg));
					else
						s.load(GAIA_FWD(arg));
				}
				// Types which have data() and size() member functions
				else if constexpr (has_data_and_size<U>::value) {
					if constexpr (Write) {
						const auto size = arg.size();
						s.save(size);

						for (const auto& e: arg)
							ser_data_one<Write>(s, e);
					} else {
						auto size = arg.size();
						s.load(size);

						if constexpr (has_resize<U, size_t>::value) {
							// If resize is present, use it
							arg.resize(size);
							for (auto& e: arg)
								ser_data_one<Write>(s, e);
						} else {
							// With no resize present, write directly into memory
							GAIA_FOR(size) {
								using arg_type = typename std::remove_pointer<decltype(arg.data())>::type;
								auto& e_ref = (arg_type&)arg[i];
								ser_data_one<Write>(s, e_ref);
							}
						}
					}
				}
				// Classes
				else if constexpr (std::is_class_v<U>) {
					meta::each_member(GAIA_FWD(arg), [&s](auto&&... items) {
						// TODO: Handle contiguous blocks of trivially copyable types
						(ser_data_one<Write>(s, items), ...);
					});
				} else
					static_assert(!sizeof(U), "Type is not supported for serialization, yet");
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
/*** End of inlined file: serialization.h ***/


/*** Start of inlined file: threadpool.h ***/
#pragma once

#if GAIA_PLATFORM_WINDOWS
	#include <cstdio>
	#include <windows.h>
	#define GAIA_THREAD std::thread
#elif GAIA_PLATFORM_APPLE
	#include <pthread.h>
	#include <pthread/sched.h>
	#include <sys/qos.h>
	#define GAIA_THREAD pthread_t
#elif GAIA_PLATFORM_LINUX || GAIA_PLATFORM_FREEBSD
	#include <pthread.h>
	#define GAIA_THREAD pthread_t
#endif

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>


/*** Start of inlined file: jobcommon.h ***/
#pragma once

#include <functional>
#include <inttypes.h>

namespace gaia {
	namespace mt {
		enum JobPriority : uint32_t {
			//! High priority job. If available it should target the CPU's performance cores
			High = 0,
			//! Low priority job. If available it should target the CPU's efficiency cores
			Low = 1
		};
		static inline constexpr uint32_t JobPriorityCnt = 2;

		struct JobAllocCtx {
			JobPriority priority;
		};

		struct Job {
			std::function<void()> func;
			JobPriority priority = JobPriority::High;
		};

		struct JobArgs {
			uint32_t idxStart;
			uint32_t idxEnd;
		};

		struct JobParallel {
			std::function<void(const JobArgs&)> func;
			JobPriority priority = JobPriority::High;
		};
	} // namespace mt
} // namespace gaia
/*** End of inlined file: jobcommon.h ***/


/*** Start of inlined file: jobhandle.h ***/
#pragma once

#include <cstdint>
#include <type_traits>

namespace gaia {
	namespace mt {
		using JobInternalType = uint32_t;
		using JobId = JobInternalType;
		using JobGenId = JobInternalType;

		struct JobHandle final {
			static constexpr JobInternalType IdBits = 20;
			static constexpr JobInternalType GenBits = 11;
			static constexpr JobInternalType PrioBits = 1;
			static constexpr JobInternalType AllBits = IdBits + GenBits + PrioBits;
			static constexpr JobInternalType IdMask = (uint32_t)(uint64_t(1) << IdBits) - 1;
			static constexpr JobInternalType GenMask = (uint32_t)(uint64_t(1) << GenBits) - 1;
			static constexpr JobInternalType PrioMask = (uint32_t)(uint64_t(1) << PrioBits) - 1;

			using JobSizeType = std::conditional_t<(AllBits > 32), uint64_t, uint32_t>;

			static_assert(AllBits <= 64, "Job IdBits and GenBits must fit inside 64 bits");
			static_assert(IdBits <= 31, "Job IdBits must be at most 31 bits long");
			static_assert(GenBits > 10, "Job GenBits must be at least 10 bits long");

		private:
			struct JobData {
				//! Index in entity array
				JobInternalType id: IdBits;
				//! Generation index. Incremented every time an item is deleted
				JobInternalType gen: GenBits;
				//! Job priority. 1-priority, 0-background
				JobInternalType prio: PrioBits;
			};

			union {
				JobData data;
				JobSizeType val;
			};

		public:
			JobHandle() noexcept = default;
			JobHandle(JobId id, JobGenId gen, JobGenId prio) {
				data.id = id;
				data.gen = gen;
				data.prio = prio;
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

			GAIA_NODISCARD auto id() const {
				return data.id;
			}
			GAIA_NODISCARD auto gen() const {
				return data.gen;
			}
			GAIA_NODISCARD auto prio() const {
				return data.prio;
			}
			GAIA_NODISCARD auto value() const {
				return val;
			}
		};

		struct JobNull_t {
			GAIA_NODISCARD operator JobHandle() const noexcept {
				return JobHandle(JobHandle::IdMask, JobHandle::GenMask, JobHandle::PrioMask);
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

/*** End of inlined file: jobhandle.h ***/


/*** Start of inlined file: jobmanager.h ***/
#pragma once

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
			JobPriority priority : 1;
			JobInternalState state : 31;
			std::function<void()> func;

			JobContainer() = default;

			GAIA_NODISCARD static JobContainer create(uint32_t index, uint32_t generation, void* pCtx) {
				auto* ctx = (JobAllocCtx*)pCtx;

				JobContainer jc{};
				jc.idx = index;
				jc.gen = generation;
				jc.priority = ctx->priority;
				jc.state = JobInternalState::Idle;
				// The rest of the values are set later on:
				//   jc.dependencyIdx
				//   jc.func
				return jc;
			}

			GAIA_NODISCARD static JobHandle handle(const JobContainer& jc) {
				return JobHandle(jc.idx, jc.gen, (uint32_t)jc.priority);
			}
		};

		using DepHandle = JobHandle;

		struct JobDependency: cnt::ilist_item {
			uint32_t dependencyIdxNext;
			JobHandle dependsOn;

			JobDependency() = default;

			GAIA_NODISCARD static JobDependency create(uint32_t index, uint32_t generation, [[maybe_unused]] void* pCtx) {
				JobDependency jd{};
				jd.idx = index;
				jd.gen = generation;
				// The rest of the values are set later on:
				//   jc.dependencyIdxNext
				//   jc.dependsOn
				return jd;
			}

			GAIA_NODISCARD static DepHandle handle(const JobDependency& jd) {
				return DepHandle(
						jd.idx, jd.gen,
						// It does not matter what value we set for priority on dependencies,
						// it is always going to be ignored.
						1);
			}
		};

		class JobManager {
			GAIA_PROF_MUTEX(std::mutex, m_jobsLock);
			//! Implicit list of jobs
			cnt::ilist<JobContainer, JobHandle> m_jobs;

			GAIA_PROF_MUTEX(std::mutex, m_depsLock);
			//! List of job dependencies
			cnt::ilist<JobDependency, DepHandle> m_deps;

		public:
			//! Cleans up any job allocations and dependencies associated with \param jobHandle
			void wait(JobHandle jobHandle) {
				// We need to release any dependencies related to this job
				auto& job = m_jobs[jobHandle.id()];

				if (job.state == JobInternalState::Released)
					return;

				uint32_t depIdx = job.dependencyIdx;
				while (depIdx != BadIndex) {
					auto& dep = m_deps[depIdx];
					const uint32_t depIdxNext = dep.dependencyIdxNext;
					// const uint32_t depPrio = dep.;
					wait(dep.dependsOn);
					free_dep(DepHandle{depIdx, 0, jobHandle.prio()});
					depIdx = depIdxNext;
				}

				// Deallocate the job itself
				free_job(jobHandle);
			}

			//! Allocates a new job container identified by a unique JobHandle.
			//! \return JobHandle
			//! \warning Must be used from the main thread.
			GAIA_NODISCARD JobHandle alloc_job(const Job& job) {
				JobAllocCtx ctx{job.priority};
				std::scoped_lock lock(m_jobsLock);
				auto handle = m_jobs.alloc(&ctx);
				auto& j = m_jobs[handle.id()];
				GAIA_ASSERT(j.state == JobInternalState::Idle || j.state == JobInternalState::Released);
				j.dependencyIdx = BadIndex;
				j.state = JobInternalState::Idle;
				j.func = job.func;
				return handle;
			}

			//! Invalidates \param jobHandle by resetting its index in the job pool.
			//! Every time a job is deallocated its generation is increased by one.
			//! \warning Must be used from the main thread.
			void free_job(JobHandle jobHandle) {
				// No need to lock. Called from the main thread only when the job has finished already.
				// --> std::scoped_lock lock(m_jobsLock);
				auto& job = m_jobs.free(jobHandle);
				job.state = JobInternalState::Released;
			}

			//! Allocates a new dependency identified by a unique DepHandle.
			//! \return DepHandle
			//! \warning Must be used from the main thread.
			GAIA_NODISCARD DepHandle alloc_dep() {
				JobAllocCtx dummyCtx{};
				return m_deps.alloc(&dummyCtx);
			}

			//! Invalidates \param depHandle by resetting its index in the dependency pool.
			//! Every time a dependency is deallocated its generation is increased by one.
			//! \warning Must be used from the main thread.
			void free_dep(DepHandle depHandle) {
				m_deps.free(depHandle);
			}

			//! Resets the job pool.
			void reset() {
				{
					// No need to lock. Called from the main thread only when all jobs have finished already.
					// --> std::scoped_lock lock(m_jobsLock);
					m_jobs.clear();
				}
				{
					// No need to lock. Called from the main thread only when all jobs must have ended already.
					// --> std::scoped_lock lock(m_depsLock);
					m_deps.clear();
				}
			}

			void run(JobHandle jobHandle) {
				std::function<void()> func;

				{
					std::scoped_lock lock(m_jobsLock);
					auto& job = m_jobs[jobHandle.id()];
					job.state = JobInternalState::Running;
					func = job.func;
				}
				if (func.operator bool())
					func();
				{
					std::scoped_lock lock(m_jobsLock);
					auto& job = m_jobs[jobHandle.id()];
					job.state = JobInternalState::Done;
				}
			}

			//! Evaluates job dependencies.
			//! \return True if job dependencies are met. False otherwise
			GAIA_NODISCARD bool handle_deps(JobHandle jobHandle) {
				GAIA_PROF_SCOPE(JobManager::handle_deps);
				std::scoped_lock lockJobs(m_jobsLock);
				auto& job = m_jobs[jobHandle.id()];
				if (job.dependencyIdx == BadIndex)
					return true;

				uint32_t depsId = job.dependencyIdx;
				{
					std::scoped_lock lockDeps(m_depsLock);

					// Iterate over all dependencies.
					// The first busy dependency breaks the loop. At this point we also update
					// the initial dependency index because we know all previous dependencies
					// have already finished and there's no need to check them.
					do {
						JobDependency dep = m_deps[depsId];
						if (!done(dep.dependsOn)) {
							m_jobs[jobHandle.id()].dependencyIdx = depsId;
							return false;
						}

						depsId = dep.dependencyIdxNext;
					} while (depsId != BadIndex);
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
				std::scoped_lock lockJobs(m_jobsLock);
				auto& job = m_jobs[jobHandle.id()];

#if GAIA_ASSERT_ENABLED
				GAIA_ASSERT(jobHandle != dependsOn);
				GAIA_ASSERT(!busy(jobHandle));
				GAIA_ASSERT(!busy(dependsOn));
#endif

				{
					GAIA_PROF_SCOPE(JobManager::dep);
					std::scoped_lock lockDeps(m_depsLock);

					auto depHandle = alloc_dep();
					auto& dep = m_deps[depHandle.id()];
					dep.dependsOn = dependsOn;

					if (job.dependencyIdx == BadIndex)
						// First time adding a dependency to this job. Point it to the first allocated handle
						dep.dependencyIdxNext = BadIndex;
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

				GAIA_PROF_SCOPE(JobManager::deps);
				std::scoped_lock lockJobs(m_jobsLock);
				{
					std::scoped_lock lockDeps(m_depsLock);

					for (auto dependsOn: dependsOnSpan) {
						auto depHandle = alloc_dep();
						auto& dep = m_deps[depHandle.id()];
						dep.dependsOn = dependsOn;

						if (job.dependencyIdx == BadIndex)
							// First time adding a dependency to this job. Point it to the first allocated handle
							dep.dependencyIdxNext = BadIndex;
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

			GAIA_NODISCARD bool done(JobHandle jobHandle) const {
				const auto& job = m_jobs[jobHandle.id()];
				return ((uint32_t)job.state & (uint32_t)JobInternalState::Done) != 0;
			}
		};
	} // namespace mt
} // namespace gaia
/*** End of inlined file: jobmanager.h ***/


/*** Start of inlined file: jobqueue.h ***/
#pragma once

#define JOB_QUEUE_USE_LOCKS 1
#if JOB_QUEUE_USE_LOCKS

	#include <mutex>
#endif

namespace gaia {
	namespace mt {
		class JobQueue {
			//! The maximum number of jobs fitting in the queue at the same time
			static constexpr uint32_t N = 1 << 12;
#if !JOB_QUEUE_USE_LOCKS
			static constexpr uint32_t MASK = N - 1;
#endif

#if JOB_QUEUE_USE_LOCKS
			GAIA_PROF_MUTEX(std::mutex, m_bufferLock);
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
				GAIA_PROF_SCOPE(JobQueue::try_push);

#if JOB_QUEUE_USE_LOCKS
				std::scoped_lock lock(m_bufferLock);
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

			//! Tries retrieving a job to the queue. FIFO.
			//! \return True if the job was retrieved. False otherwise (e.g. there are no jobs).
			GAIA_NODISCARD bool try_pop(JobHandle& jobHandle) {
				GAIA_PROF_SCOPE(JobQueue::try_pop);

#if JOB_QUEUE_USE_LOCKS
				std::scoped_lock lock(m_bufferLock);
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
				std::scoped_lock lock(m_bufferLock);
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
/*** End of inlined file: jobqueue.h ***/

namespace gaia {
	namespace mt {
#if GAIA_PLATFORM_WINDOWS
		extern "C" typedef HRESULT(WINAPI* TOSApiFunc_SetThreadDescription)(HANDLE, PCWSTR);

	#pragma pack(push, 8)
		typedef struct tagTHREADNAME_INFO {
			DWORD dwType; // Must be 0x1000.
			LPCSTR szName; // Pointer to name (in user addr space).
			DWORD dwThreadID; // Thread ID (-1=caller thread).
			DWORD dwFlags; // Reserved for future use, must be zero.
		} THREADNAME_INFO;
	#pragma pack(pop)
#endif

		class ThreadPool final {
			static constexpr uint32_t MaxWorkers = 31;

			//! ID of the main thread
			std::thread::id m_mainThreadId;
			//! When true the pool is supposed to finish all work and terminate all threads
			bool m_stop{};
			//! Array of worker threads
			cnt::sarr_ext<GAIA_THREAD, MaxWorkers> m_workers;
			//! The number of workers dedicated for a given level of job priority
			uint32_t m_workerCnt[JobPriorityCnt]{};

			//! Manager for internal jobs
			JobManager m_jobManager;

			//! How many jobs are currently being processed
			std::atomic_uint32_t m_jobsPending[JobPriorityCnt]{};
			//! Mutex protecting the access to a given queue
			GAIA_PROF_MUTEX(std::mutex, m_cvLock0);
			GAIA_PROF_MUTEX(std::mutex, m_cvLock1);
			//! Signals for given workers to wake up
			std::condition_variable m_cv[JobPriorityCnt];
			//! Array of pending user jobs
			JobQueue m_jobQueue[JobPriorityCnt];

		private:
			ThreadPool() {
				make_main_thread();

				const auto hwThreads = hw_thread_cnt();
				set_max_workers(hwThreads, hwThreads);
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

			~ThreadPool() {
				reset();
			}

			//! Make the calling thread the effective main thread from the thread pool perspective
			void make_main_thread() {
				m_mainThreadId = std::this_thread::get_id();
			}

			//! Returns the number of worker threads
			GAIA_NODISCARD uint32_t workers() const {
				return m_workers.size();
			}

			void update_workers_cnt(uint32_t count) {
				if (count == 0) {
					// Use one because we still have the main thread
					m_workerCnt[0] = 1;
					m_workerCnt[1] = 1;
				} else {
					m_workerCnt[0] = count;
					m_workerCnt[1] = m_workers.size() - count;
				}
			}

			//! Set the maximum number of works for this system.
			//! \param count Requested number of worker threads to create
			//! \param countHighPrio HighPrio Number of high-priority workers to create.
			//!                      Calculated as Max(count, countHighPrio).
			//! \warning All jobs are finished first before threads are recreated.
			void set_max_workers(uint32_t count, uint32_t countHighPrio) {
				const auto workersCnt = core::get_min(MaxWorkers, count);
				countHighPrio = core::get_max(count, countHighPrio);

				// Stop all threads first
				reset();
				m_workers.resize(workersCnt);
				GAIA_EACH(m_workers) m_workers[i] = {};

				// Create a new set of high and low priority threads (if any)
				set_workers_high_prio(countHighPrio);
			}

			//! Updates the number of worker threads participating at high priority workloads
			//! \warning All jobs are finished first before threads are recreated.
			void set_workers_high_prio(uint32_t count) {
				const uint32_t realCnt = gaia::core::get_min(count, m_workers.size());

				// Stop all threads first
				reset();

				update_workers_cnt(realCnt);

				// Create a new set of high and low priority threads (if any)
				set_workers_high_prio_inter(0, m_workerCnt[0]);
				set_workers_low_prio_inter(m_workerCnt[0], m_workerCnt[1]);
			}

			//! Updates the number of worker threads participating at low priority workloads
			//! \warning All jobs are finished first before threads are recreated.
			void set_workers_low_prio(uint32_t count) {
				const uint32_t realCnt = gaia::core::get_max(count, m_workers.size());

				// Stop all threads first
				reset();

				update_workers_cnt(m_workers.size() - realCnt);

				// Create a new set of high and low priority threads (if any)
				set_workers_high_prio_inter(0, m_workerCnt[0]);
				set_workers_low_prio_inter(m_workerCnt[0], m_workerCnt[1]);
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
			JobHandle add(Job& job) {
				GAIA_ASSERT(main_thread());

				// Don't add new jobs once stop was requested
				if GAIA_UNLIKELY (m_stop)
					return JobNull;

				job.priority = final_prio(job);

				++m_jobsPending[(uint32_t)job.priority];
				return m_jobManager.alloc_job(job);
			}

			//! Pushes \param jobHandle into the internal queue so worker threads
			//! can pick it up and execute it.
			//! If there are more jobs than the queue can handle it puts the calling
			//! thread to sleep until workers consume enough jobs.
			//! \warning Once submited, dependencies can't be modified for this job.
			void submit(JobHandle jobHandle) {
				m_jobManager.submit(jobHandle);

				const auto prio = (JobPriority)jobHandle.prio();
				auto& jobQueue = m_jobQueue[(uint32_t)prio];
				auto& cv = m_cv[(uint32_t)prio];

				if GAIA_UNLIKELY (m_workers.empty()) {
					(void)jobQueue.try_push(jobHandle);
					main_thread_tick(prio);
					return;
				}

				// Try pushing a new job until we succeed.
				// The thread is put to sleep if pushing the jobs fails.
				while (!jobQueue.try_push(jobHandle))
					poll(prio);

				// Wake some worker thread
				cv.notify_one();
			}

		private:
			//! Resubmits \param jobHandle into the internal queue so worker threads
			//! can pick it up and execute it.
			//! If there are more jobs than the queue can handle it puts the calling
			//! thread to sleep until workers consume enough jobs.
			//! \warning Internal usage only. Only worker threads can decide to resubmit.
			void resubmit(JobHandle jobHandle) {
				m_jobManager.resubmit(jobHandle);

				const auto prio = (JobPriority)jobHandle.prio();
				auto& jobQueue = m_jobQueue[(uint32_t)prio];
				auto& cv = m_cv[(uint32_t)prio];

				if GAIA_UNLIKELY (m_workers.empty()) {
					(void)jobQueue.try_push(jobHandle);
					// Let the other parts of the code handle resubmittion (submit, update).
					// Otherwise, we would enter an endless recursion and stack overflow here.
					// -->  main_thread_tick(prio);
					return;
				}

				// Try pushing a new job until we succeed.
				// The thread is put to sleep if pushing the jobs fails.
				while (!jobQueue.try_push(jobHandle))
					poll(prio);

				// Wake some worker thread
				cv.notify_one();
			}

		public:
			//! Schedules a job to run on a worker thread.
			//! \param job Job descriptor
			//! \warning Must be used from the main thread.
			//! \warning Dependencies can't be modified for this job.
			//! \return Job handle of the scheduled job.
			JobHandle sched(Job& job) {
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
			JobHandle sched(Job& job, JobHandle dependsOn) {
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
			JobHandle sched(Job& job, std::span<const JobHandle> dependsOnSpan) {
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
			JobHandle sched_par(JobParallel& job, uint32_t itemsToProcess, uint32_t groupSize) {
				GAIA_ASSERT(main_thread());

				// Empty data set are considered wrong inputs
				GAIA_ASSERT(itemsToProcess != 0);
				if (itemsToProcess == 0)
					return JobNull;

				// Don't add new jobs once stop was requested
				if GAIA_UNLIKELY (m_stop)
					return JobNull;

				// Make sure the right priority is selected
				const auto prio = job.priority = final_prio(job);

				const uint32_t workerCount = m_workerCnt[(uint32_t)prio];

				// No group size was given, make a guess based on the set size
				if (groupSize == 0)
					groupSize = (itemsToProcess + workerCount - 1) / workerCount;

				const auto jobs = (itemsToProcess + groupSize - 1) / groupSize;
				// Internal jobs + 1 for the groupHandle
				m_jobsPending[(uint32_t)prio] += (jobs + 1U);

				JobHandle groupHandle = m_jobManager.alloc_job({{}, prio});

				GAIA_FOR_(jobs, jobIndex) {
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

					JobHandle jobHandle = m_jobManager.alloc_job({groupJobFunc, prio});
					dep(groupHandle, jobHandle);
					submit(jobHandle);
				}

				submit(groupHandle);
				return groupHandle;
			}

			//! Waits for the job to finish.
			//! \param jobHandle Job handle
			//! \warning Must be used from the main thread.
			void wait(JobHandle jobHandle) {
				// When no workers are available, execute all we have.
				if GAIA_UNLIKELY (m_workers.empty()) {
					update();
					m_jobManager.reset();
					return;
				}

				GAIA_ASSERT(main_thread());

				while (m_jobManager.busy(jobHandle))
					poll((JobPriority)jobHandle.prio());

				GAIA_ASSERT(!m_jobManager.busy(jobHandle));
				m_jobManager.wait(jobHandle);
			}

			//! Waits for all jobs to finish.
			//! \warning Must be used from the main thread.
			void wait_all() {
				// When no workers are available, execute all we have.
				if GAIA_UNLIKELY (m_workers.empty()) {
					update();
					m_jobManager.reset();
					return;
				}

				GAIA_ASSERT(main_thread());

				busy_poll_all();

#if GAIA_ASSERT_ENABLED
				// No jobs should be pending at this point
				GAIA_FOR(JobPriorityCnt) {
					GAIA_ASSERT(!busy((JobPriority)i));
				}
#endif

				m_jobManager.reset();
			}

			//! Uses the main thread to help with jobs processing.
			void update() {
				GAIA_ASSERT(main_thread());

				bool moreWork = false;
				do {
					// Participate at processing of high-performance tasks.
					// They have higher priority so execute them first.
					moreWork = main_thread_tick(JobPriority::High);

					// Participate at processing of low-performance tasks.
					moreWork |= main_thread_tick(JobPriority::Low);
				} while (moreWork);
			}

			//! Returns the number of HW threads available on the system minus 1 (the main thread).
			//! \return 0 if failed. Otherwise, the number of hardware threads minus 1 (1 is minimum).
			GAIA_NODISCARD static uint32_t hw_thread_cnt() {
				auto hwThreads = (uint32_t)std::thread::hardware_concurrency();
				if (hwThreads > 0)
					return core::get_max(1U, hwThreads - 1U);
				return hwThreads;
			}

		private:
			struct ThreadFuncCtx {
				ThreadPool* tp;
				uint32_t workerIdx;
				JobPriority prio;
			};

			static void* thread_func(void* pCtx) {
				const auto& ctx = *(const ThreadFuncCtx*)pCtx;

				// Set the worker thread name.
				// Needs to be called from inside the thread because some platforms
				// can change the name only when run from the specific thread.
				ctx.tp->set_thread_name(ctx.workerIdx, ctx.prio);

				// Set the worker thread priority
				ctx.tp->set_thread_priority(ctx.workerIdx, ctx.prio);

				// Process jobs
				ctx.tp->worker_loop(ctx.prio);

#if !GAIA_PLATFORM_WINDOWS
				// Other platforms allocate the context dynamically
				ctx.~ThreadFuncCtx();
				mem::AllocHelper::free(pCtx);
#endif
				return nullptr;
			}

			//! Creates a thread
			//! \param workerIdx Worker index
			//! \param prio Priority used for the thread
			void create_thread(uint32_t workerIdx, JobPriority prio) {
				if GAIA_UNLIKELY (workerIdx >= m_workers.size())
					return;

#if GAIA_PLATFORM_WINDOWS
				ThreadFuncCtx ctx{this, workerIdx, prio};
				m_workers[workerIdx] = std::thread([ctx]() {
					thread_func((void*)&ctx);
				});
#else
				pthread_attr_t attr{};
				int ret = pthread_attr_init(&attr);
				if (ret != 0) {
					GAIA_LOG_W("pthread_attr_init failed for worker thread %u. ErrCode = %d", workerIdx, ret);
					return;
				}

				///////////////////////////////////////////////////////////////////////////
				// Apple's recommendation for Apple Silicon for games / high-perf software
				// ========================================================================
				// Per frame              | Scheduling policy     | QoS class / Priority
				// ========================================================================
				// Main thread            |     SCHED_OTHER       | QOS_CLASS_USER_INTERACTIVE (47)
				// Render/Audio thread    |     SCHED_RR          | 45
				// Workers High Prio      |     SCHED_RR          | 39-41
				// Workers Low Prio       |     SCHED_OTHER       | QOS_CLASS_USER_INTERACTIVE (38)
				// ========================================================================
				// Multiple-frames        |                       |
				// ========================================================================
				// Async Workers High Prio|     SCHED_OTHER       | QOS_CLASS_USER_INITIATED (37)
				// Async Workers Low Prio |     SCHED_OTHER       | QOS_CLASS_DEFAULT (31)
				// Prefetching/Streaming  |     SCHED_OTHER       | QOS_CLASS_UTILITY (20)
				// ========================================================================

				if (prio == JobPriority::Low) {
	#if GAIA_PLATFORM_APPLE
					ret = pthread_attr_set_qos_class_np(&attr, QOS_CLASS_USER_INTERACTIVE, -9); // 47-9=38
					if (ret != 0) {
						GAIA_LOG_W(
								"pthread_attr_set_qos_class_np failed for worker thread %u [prio=%u]. ErrCode = %d", workerIdx,
								(uint32_t)prio, ret);
					}
	#else
					ret = pthread_attr_setschedpolicy(&attr, SCHED_OTHER);
					if (ret != 0) {
						GAIA_LOG_W(
								"pthread_attr_setschedpolicy SCHED_RR failed for worker thread %u [prio=%u]. ErrCode = %d", workerIdx,
								(uint32_t)prio, ret);
					}

					int prioMax = core::get_min(38, sched_get_priority_max(SCHED_OTHER));
					int prioMin = core::get_min(prioMax, sched_get_priority_min(SCHED_OTHER));
					int prioUse = core::get_min(prioMin + 5, prioMax);
					prioUse = core::get_max(prioUse, prioMin);
					sched_param param{};
					param.sched_priority = prioUse;

					ret = pthread_attr_setschedparam(&attr, &param);
					if (ret != 0) {
						GAIA_LOG_W(
								"pthread_attr_setschedparam %d failed for worker thread %u [prio=%u]. ErrCode = %d",
								param.sched_priority, workerIdx, (uint32_t)prio, ret);
					}
	#endif
				} else {
					ret = pthread_attr_setschedpolicy(&attr, SCHED_RR);
					if (ret != 0) {
						GAIA_LOG_W(
								"pthread_attr_setschedpolicy SCHED_RR failed for worker thread %u [prio=%u]. ErrCode = %d", workerIdx,
								(uint32_t)prio, ret);
					}

					int prioMax = core::get_min(41, sched_get_priority_max(SCHED_RR));
					int prioMin = core::get_min(prioMax, sched_get_priority_min(SCHED_RR));
					int prioUse = core::get_max(prioMax - 5, prioMin);
					prioUse = core::get_min(prioUse, prioMax);
					sched_param param{};
					param.sched_priority = prioUse;

					ret = pthread_attr_setschedparam(&attr, &param);
					if (ret != 0) {
						GAIA_LOG_W(
								"pthread_attr_setschedparam %d failed for worker thread %u [prio=%u]. ErrCode = %d",
								param.sched_priority, workerIdx, (uint32_t)prio, ret);
					}
				}

				// Create the thread with given attributes
				auto* ctx = mem::AllocHelper::alloc<ThreadFuncCtx>();
				(void)new (ctx) ThreadFuncCtx{this, workerIdx, prio};
				ret = pthread_create(&m_workers[workerIdx], &attr, thread_func, ctx);
				if (ret != 0) {
					GAIA_LOG_W("pthread_create failed for worker thread %u. ErrCode = %d", workerIdx, ret);
				}

				pthread_attr_destroy(&attr);
#endif

				// Stick each thread to a specific CPU core if possible
				set_thread_affinity(workerIdx);
			}

			//! Joins a worker thread with the main thread (graceful thread termination)
			//! \param workerIdx Worker index
			void join_thread(uint32_t workerIdx) {
				if GAIA_UNLIKELY (workerIdx >= m_workers.size())
					return;

#if GAIA_PLATFORM_WINDOWS
				auto& t = m_workers[workerIdx];
				if (t.joinable())
					t.join();
#else
				auto& t = m_workers[workerIdx];
				pthread_join(t, nullptr);
#endif
			}

			void set_workers_high_prio_inter(uint32_t from, uint32_t count) {
				const uint32_t to = from + count;
				for (uint32_t i = from; i < to; ++i)
					create_thread(i, JobPriority::High);
			}

			void set_workers_low_prio_inter(uint32_t from, uint32_t count) {
				const uint32_t to = from + count;
				for (uint32_t i = from; i < to; ++i)
					create_thread(i, JobPriority::Low);
			}

			void set_thread_priority([[maybe_unused]] uint32_t workerIdx, [[maybe_unused]] JobPriority priority) {
#if GAIA_PLATFORM_WINDOWS
				HANDLE nativeHandle = (HANDLE)m_workers[workerIdx].native_handle();

				THREAD_POWER_THROTTLING_STATE state{};
				state.Version = THREAD_POWER_THROTTLING_CURRENT_VERSION;
				if (priority == JobPriority::High) {
					// HighQoS
					// Turn EXECUTION_SPEED throttling off.
					// ControlMask selects the mechanism and StateMask is set to zero as mechanisms should be turned off.
					state.ControlMask = THREAD_POWER_THROTTLING_EXECUTION_SPEED;
					state.StateMask = 0;
				} else {
					// EcoQoS
					// Turn EXECUTION_SPEED throttling on.
					// ControlMask selects the mechanism and StateMask declares which mechanism should be on or off.
					state.ControlMask = THREAD_POWER_THROTTLING_EXECUTION_SPEED;
					state.StateMask = THREAD_POWER_THROTTLING_EXECUTION_SPEED;
				}

				BOOL ret = SetThreadInformation(nativeHandle, ThreadPowerThrottling, &state, sizeof(state));
				if (ret != TRUE) {
					GAIA_LOG_W("SetThreadInformation failed for thread %u", workerIdx);
					return;
				}
#else
				// Done when the thread is created
#endif
			}

			void set_thread_affinity([[maybe_unused]] uint32_t workerIdx) {
				// NOTE:
				// Some cores might have multiple logic threads, there might be
				// more sockets and some cores might even be physically different
				// form others (performance vs efficiency cores).
				// Because of that, do not handle affinity and let the OS figure it out.
				// All treads created by the pool are setting thread priorities to make
				// it easier for the OS.

				// #if GAIA_PLATFORM_WINDOWS
				// 				HANDLE nativeHandle = (HANDLE)m_workers[workerIdx].native_handle();
				//
				// 				auto mask = SetThreadAffinityMask(nativeHandle, 1ULL << workerIdx);
				// 				if (mask <= 0)
				// 					GAIA_LOG_W("Issue setting thread affinity for worker thread %u!", workerIdx);
				// #elif GAIA_PLATFORM_APPLE
				// 				// Do not do affinity for MacOS. If is not supported for Apple Silicon and
				// 				// Intel MACs are deprecated anyway.
				// 				// TODO: Consider supporting this at least for Intel MAC as there are still
				// 				//       quite of few of them out there.
				// #elif GAIA_PLATFORM_LINUX || GAIA_PLATFORM_FREEBSD
				// 				pthread_t nativeHandle = (pthread_t)m_workers[workerIdx].native_handle();
				//
				// 				cpu_set_t cpuset;
				// 				CPU_ZERO(&cpuset);
				// 				CPU_SET(workerIdx, &cpuset);
				//
				// 				auto ret = pthread_setaffinity_np(nativeHandle, sizeof(cpuset), &cpuset);
				// 				if (ret != 0)
				// 					GAIA_LOG_W("Issue setting thread affinity for worker thread %u!", workerIdx);
				//
				// 				ret = pthread_getaffinity_np(nativeHandle, sizeof(cpuset), &cpuset);
				// 				if (ret != 0)
				// 					GAIA_LOG_W("Thread affinity could not be set for worker thread %u!", workerIdx);
				// #endif
			}

			//! Updates the name of a given thread. It is based on the index and priority of the worker.
			//! \param workerIdx Index of the worker
			//! \param prio Worker priority
			void set_thread_name(uint32_t workerIdx, JobPriority prio) {
#if GAIA_PROF_USE_PROFILER_THREAD_NAME
				char threadName[16]{};
				snprintf(threadName, 16, "worker_%s_%u", prio == JobPriority::High ? "HI" : "LO", workerIdx);
				GAIA_PROF_THREAD_NAME(threadName);
#elif GAIA_PLATFORM_WINDOWS
				auto nativeHandle = (HANDLE)m_workers[workerIdx].native_handle();

				TOSApiFunc_SetThreadDescription pSetThreadDescFunc = nullptr;
				if (auto* pModule = GetModuleHandleA("kernel32.dll"))
					pSetThreadDescFunc = (TOSApiFunc_SetThreadDescription)GetProcAddress(pModule, "SetThreadDescription");
				if (pSetThreadDescFunc != nullptr) {
					wchar_t threadName[16]{};
					swprintf_s(threadName, L"worker_%s_%u", prio == JobPriority::High ? L"HI" : L"LO", workerIdx);

					auto hr = pSetThreadDescFunc(nativeHandle, threadName);
					if (FAILED(hr)) {
						GAIA_LOG_W(
								"Issue setting name for worker %s thread %u!", prio == JobPriority::High ? "HI" : "LO", workerIdx);
					}
				} else {
	#if defined _MSC_VER
					char threadName[16]{};
					snprintf(threadName, 16, "worker_%s_%u", prio == JobPriority::High ? "HI" : "LO", workerIdx);

					THREADNAME_INFO info{};
					info.dwType = 0x1000;
					info.szName = threadName;
					info.dwThreadID = GetThreadId(nativeHandle);

					__try {
						RaiseException(0x406D1388, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
					} __except (EXCEPTION_EXECUTE_HANDLER) {
					}
	#endif
				}
#elif GAIA_PLATFORM_APPLE
				char threadName[16]{};
				snprintf(threadName, 16, "worker_%s_%u", prio == JobPriority::High ? "HI" : "LO", workerIdx);
				auto ret = pthread_setname_np(threadName);
				if (ret != 0)
					GAIA_LOG_W("Issue setting name for worker %s thread %u!", prio == JobPriority::High ? "HI" : "LO", workerIdx);
#elif GAIA_PLATFORM_LINUX || GAIA_PLATFORM_FREEBSD
				auto nativeHandle = m_workers[workerIdx];

				char threadName[16]{};
				snprintf(threadName, 16, "worker_%s_%u", prio == JobPriority::High ? "HI" : "LO", workerIdx);
				GAIA_PROF_THREAD_NAME(threadName);
				auto ret = pthread_setname_np(nativeHandle, threadName);
				if (ret != 0)
					GAIA_LOG_W("Issue setting name for worker %s thread %u!", prio == JobPriority::High ? "HI" : "LO", workerIdx);
#endif
			}

			//! Checks if the calling thread is considered the main thread
			//! \return True if the calling thread is the main thread. False otherwise.
			GAIA_NODISCARD bool main_thread() const {
				return std::this_thread::get_id() == m_mainThreadId;
			}

			//! Loop run by main thread. Pops jobs from the given queue
			//! and executes it.
			//! \param prio Target worker queue defined by job priority
			//! \return True if a job was resubmitted or executed. False otherwise.
			bool main_thread_tick(JobPriority prio) {
				auto& jobQueue = m_jobQueue[(uint32_t)prio];

				JobHandle jobHandle;

				if (!jobQueue.try_pop(jobHandle))
					return false;

				GAIA_ASSERT(busy(prio));

				// Make sure we can execute the job.
				// If it has dependencies which were not completed we need
				// to reschedule and come back to it later.
				if (!m_jobManager.handle_deps(jobHandle)) {
					resubmit(jobHandle);
					return true;
				}

				m_jobManager.run(jobHandle);
				--m_jobsPending[(uint32_t)prio];
				return true;
			}

			//! Loop run by worker threads. Pops jobs from the given queue
			//! and executes it.
			//! \param prio Target worker queue defined by job priority
			void worker_loop(JobPriority prio) {
				auto& jobQueue = m_jobQueue[(uint32_t)prio];
				auto& cv = m_cv[prio];
				auto& cvLock = prio == 0 ? m_cvLock0 : m_cvLock1;

				bool ready = false;
				while (!m_stop) {
					JobHandle jobHandle;
					if (!jobQueue.try_pop(jobHandle)) {
						ready = true;

						auto& mtx = GAIA_PROF_EXTRACT_MUTEX(std::mutex, cvLock);
						std::unique_lock lock(mtx);
						cv.wait(lock, [&] {
							return ready;
						});

						ready = false;
						continue;
					}

					GAIA_ASSERT(busy(prio));

					// Make sure we can execute the job.
					// If it has dependencies which were not completed we need
					// to reschedule and come back to it later.
					if (!m_jobManager.handle_deps(jobHandle)) {
						resubmit(jobHandle);
						continue;
					}

					m_jobManager.run(jobHandle);
					--m_jobsPending[(uint32_t)prio];
				}
			}

			//! Finishes all jobs and stops all worker threads
			void reset() {
				// Request stopping
				m_stop = true;

				// complete all remaining work
				wait_all();

				// Wake up any threads that were put to sleep
				m_cv[0].notify_all();
				m_cv[1].notify_all();

				// Join threads with the main one
				GAIA_FOR(m_workers.size()) join_thread(i);

				// All threads have been stopped. Allow new threads to run if necessary.
				m_stop = false;
			}

			//! Checks whether workers are busy doing work.
			//!	\return True if any workers are busy doing work.
			GAIA_NODISCARD bool busy(JobPriority prio) const {
				return m_jobsPending[(uint32_t)prio] > 0;
			}

			//! Wakes up some worker thread and reschedules the current one.
			void poll(JobPriority prio) {
				// Wake some worker thread
				m_cv[(uint32_t)prio].notify_one();

				// Allow this thread to be rescheduled
				std::this_thread::yield();
			}

			//! Wakes up all worker threads and reschedules the current one.
			void busy_poll_all() {
				bool b0 = busy(JobPriority::High);
				bool b1 = busy(JobPriority::Low);

				while (b1 || b0) {
					// Wake some priority worker thread
					if (b0)
						m_cv[0].notify_all();
					// Wake some background worker thread
					if (b1)
						m_cv[1].notify_all();

					// Allow this thread to be rescheduled
					std::this_thread::yield();

					// Check the status again
					b0 = busy(JobPriority::High);
					b1 = busy(JobPriority::Low);
				}
			}

			//! Makes sure the priority is right for the given set of allocated workers
			template <typename TJob>
			JobPriority final_prio(const TJob& job) {
				const auto cnt = m_workerCnt[job.priority];
				return cnt > 0
									 // If there is enough workers, keep the priority
									 ? job.priority
									 // Not enough workers, use the other priority that has workers
									 : (JobPriority)((job.priority + 1U) % (uint32_t)JobPriorityCnt);
			}
		};
	} // namespace mt
} // namespace gaia
/*** End of inlined file: threadpool.h ***/


/*** Start of inlined file: archetype.h ***/
#pragma once

#include <cinttypes>
#include <cstdint>


/*** Start of inlined file: archetype_common.h ***/
#pragma once

#include <cstdint>

namespace gaia {
	namespace ecs {
		class Archetype;

		using ArchetypeId = uint32_t;
		using ArchetypeDArray = cnt::darray<Archetype*>;
		using ArchetypeIdHash = core::direct_hash_key<uint32_t>;

		struct ArchetypeIdHashPair {
			ArchetypeId id;
			ArchetypeIdHash hash;

			GAIA_NODISCARD bool operator==(ArchetypeIdHashPair other) const {
				return id == other.id;
			}
			GAIA_NODISCARD bool operator!=(ArchetypeIdHashPair other) const {
				return id != other.id;
			}
		};

		static constexpr ArchetypeId ArchetypeIdBad = (ArchetypeId)-1;
		static constexpr ArchetypeIdHashPair ArchetypeIdHashPairBad = {ArchetypeIdBad, {0}};

		class ArchetypeIdLookupKey final {
		public:
			using LookupHash = core::direct_hash_key<uint32_t>;

		private:
			ArchetypeId m_id;
			ArchetypeIdHash m_hash;

		public:
			GAIA_NODISCARD static LookupHash calc(ArchetypeId id) {
				return {static_cast<uint32_t>(core::calculate_hash64(id))};
			}

			static constexpr bool IsDirectHashKey = true;

			ArchetypeIdLookupKey(): m_id(0), m_hash({0}) {}
			ArchetypeIdLookupKey(ArchetypeIdHashPair pair): m_id(pair.id), m_hash(pair.hash) {}
			explicit ArchetypeIdLookupKey(ArchetypeId id, LookupHash hash): m_id(id), m_hash(hash) {}

			GAIA_NODISCARD size_t hash() const {
				return (size_t)m_hash.hash;
			}

			GAIA_NODISCARD bool operator==(const ArchetypeIdLookupKey& other) const {
				// Hash doesn't match we don't have a match.
				// Hash collisions are expected to be very unlikely so optimize for this case.
				if GAIA_LIKELY (m_hash != other.m_hash)
					return false;

				return m_id == other.m_id;
			}
		};

		using ArchetypeMapById = cnt::map<ArchetypeIdLookupKey, Archetype*>;
	} // namespace ecs
} // namespace gaia
/*** End of inlined file: archetype_common.h ***/


/*** Start of inlined file: archetype_graph.h ***/
#pragma once

#include <cstdint>


/*** Start of inlined file: component.h ***/
#pragma once

#include <cstdint>
#include <type_traits>


/*** Start of inlined file: id.h ***/
#pragma once

#include <cstdint>
#include <type_traits>

namespace gaia {
	namespace ecs {
#define GAIA_ID(type) GAIA_ID_##type

		using Identifier = uint64_t;
		inline constexpr Identifier IdentifierBad = (Identifier)-1;
		inline constexpr Identifier EntityCompMask = IdentifierBad << 1;

		using IdentifierId = uint32_t;
		using IdentifierData = uint32_t;

		using EntityId = IdentifierId;
		using ComponentId = IdentifierId;

		inline constexpr IdentifierId IdentifierIdBad = (IdentifierId)-1;

		// ------------------------------------------------------------------------------------
		// Component
		// ------------------------------------------------------------------------------------

		struct Component final {
			static constexpr uint32_t IdMask = IdentifierIdBad;
			static constexpr uint32_t MaxAlignment_Bits = 10;
			static constexpr uint32_t MaxAlignment = (1U << MaxAlignment_Bits) - 1;
			static constexpr uint32_t MaxComponentSize_Bits = 8;
			static constexpr uint32_t MaxComponentSizeInBytes = (1 << MaxComponentSize_Bits) - 1;

			struct InternalData {
				//! Index in the entity array
				// detail::ComponentDescId id;
				uint32_t id;
				//! Component is SoA
				IdentifierData soa: meta::StructToTupleMaxTypes_Bits;
				//! Component size
				IdentifierData size: MaxComponentSize_Bits;
				//! Component alignment
				IdentifierData alig: MaxAlignment_Bits;
				//! Unused part
				IdentifierData unused : 10;
			};
			static_assert(sizeof(InternalData) == sizeof(Identifier));

			union {
				InternalData data;
				Identifier val;
			};

			Component() noexcept = default;

			Component(uint32_t id, uint32_t soa, uint32_t size, uint32_t alig) noexcept {
				data.id = id;
				data.soa = soa;
				data.size = size;
				data.alig = alig;
				data.unused = 0;
			}

			GAIA_NODISCARD constexpr auto id() const noexcept {
				return (uint32_t)data.id;
			}

			GAIA_NODISCARD constexpr auto soa() const noexcept {
				return (uint32_t)data.soa;
			}

			GAIA_NODISCARD constexpr auto size() const noexcept {
				return (uint32_t)data.size;
			}

			GAIA_NODISCARD constexpr auto alig() const noexcept {
				return (uint32_t)data.alig;
			}

			GAIA_NODISCARD constexpr auto value() const noexcept {
				return val;
			}

			GAIA_NODISCARD constexpr bool operator==(Component other) const noexcept {
				return value() == other.value();
			}

			GAIA_NODISCARD constexpr bool operator!=(Component other) const noexcept {
				return value() != other.value();
			}

			GAIA_NODISCARD constexpr bool operator<(Component other) const noexcept {
				return id() < other.id();
			}
		};

		//----------------------------------------------------------------------
		// Entity kind
		//----------------------------------------------------------------------

		enum EntityKind : uint8_t {
			// Generic entity, one per entity
			EK_Gen = 0,
			// Unique entity, one per chunk
			EK_Uni,
			// Number of entity kinds
			EK_Count
		};

		inline constexpr const char* EntityKindString[EntityKind::EK_Count] = {"Gen", "Uni"};

		//----------------------------------------------------------------------
		// Id type deduction
		//----------------------------------------------------------------------

		template <typename T>
		struct uni {
			static_assert(core::is_raw_v<T>);
			static_assert(
					std::is_trivial_v<T> ||
							// For non-trivial T the comparison operator must be implemented because
							// defragmentation needs it to figure out if entities can be moved around.
							(core::has_global_equals<T>::value || core::has_member_equals<T>::value),
					"Non-trivial Uni component must implement operator==");

			//! Component kind
			static constexpr EntityKind Kind = EntityKind::EK_Uni;

			//! Raw type with no additional sugar
			using TType = T;
			//! uni<TType>
			using TTypeFull = uni<TType>;
			//! Original template type
			using TTypeOriginal = T;
		};

		namespace detail {
			template <typename, typename = void>
			struct has_entity_kind: std::false_type {};
			template <typename T>
			struct has_entity_kind<T, std::void_t<decltype(T::Kind)>>: std::true_type {};

			template <typename T>
			struct ExtractComponentType_NoEntityKind {
				//! Component kind
				static constexpr EntityKind Kind = EntityKind::EK_Gen;

				//! Raw type with no additional sugar
				using Type = core::raw_t<T>;
				//! Same as Type
				using TypeFull = Type;
				//! Original template type
				using TypeOriginal = T;
			};

			template <typename T>
			struct ExtractComponentType_WithEntityKind {
				//! Component kind
				static constexpr EntityKind Kind = T::Kind;

				//! Raw type with no additional sugar
				using Type = typename T::TType;
				//! T or uni<T> depending on entity kind specified
				using TypeFull = std::conditional_t<Kind == EntityKind::EK_Gen, Type, uni<Type>>;
				//! Original template type
				using TypeOriginal = typename T::TTypeOriginal;
			};

			template <typename, typename = void>
			struct is_gen_component: std::true_type {};
			template <typename T>
			struct is_gen_component<T, std::void_t<decltype(T::Kind)>>: std::bool_constant<T::Kind == EntityKind::EK_Gen> {};

			template <typename T, typename = void>
			struct component_type {
				using type = typename detail::ExtractComponentType_NoEntityKind<T>;
			};
			template <typename T>
			struct component_type<T, std::void_t<decltype(T::Kind)>> {
				using type = typename detail::ExtractComponentType_WithEntityKind<T>;
			};
		} // namespace detail

		template <typename T>
		using component_type_t = typename detail::component_type<T>::type;

		template <typename T>
		inline constexpr EntityKind entity_kind_v = component_type_t<T>::Kind;

		//----------------------------------------------------------------------
		// Pair helpers
		//----------------------------------------------------------------------

		namespace detail {
			struct pair_base {};
		} // namespace detail

		//! Wrapper for two types forming a relationship pair.
		//! Depending on what types are used to form a pair it can contain a value.
		//! To determine the storage type the following logic is applied:
		//! If \tparam Rel is non-empty, the storage type is Rel.
		//! If \tparam Rel is empty and \tparam Tgt is non-empty, the storage type is Tgt.
		//! \tparam Rel relation part of the relationship
		//! \tparam Tgt target part of the relationship
		template <typename Rel, typename Tgt>
		class pair: public detail::pair_base {
			using rel_comp_type = component_type_t<Rel>;
			using tgt_comp_type = component_type_t<Tgt>;

		public:
			using rel = typename rel_comp_type::TypeFull;
			using tgt = typename tgt_comp_type::TypeFull;
			using rel_type = typename rel_comp_type::Type;
			using tgt_type = typename tgt_comp_type::Type;
			using rel_original = typename rel_comp_type::TypeOriginal;
			using tgt_original = typename tgt_comp_type::TypeOriginal;
			using type = std::conditional_t<!std::is_empty_v<rel_type> || std::is_empty_v<tgt_type>, rel, tgt>;
		};

		template <typename T>
		struct is_pair {
			static constexpr bool value = std::is_base_of<detail::pair_base, core::raw_t<T>>::value;
		};

		// ------------------------------------------------------------------------------------
		// Entity
		// ------------------------------------------------------------------------------------

		struct Entity final {
			static constexpr uint32_t IdMask = IdentifierIdBad;

			struct InternalData {
				//! Index in the entity array
				EntityId id;

				///////////////////////////////////////////////////////////////////
				// Bits in this section need to be 1:1 with EntityContainer data.
				// Note, the order of these bits is important because entities
				// are sorted by their "val" member and many behaviors rely on this.
				///////////////////////////////////////////////////////////////////

				//! Generation index. Incremented every time an entity is deleted
				IdentifierData gen : 28;
				//! 0-component, 1-entity
				IdentifierData ent : 1;
				//! 0-ordinary, 1-pair
				IdentifierData pair : 1;
				//! 0=EntityKind::CT_Gen, 1=EntityKind::CT_Uni
				IdentifierData kind : 1;
				//! Unused
				IdentifierData unused : 1;

				///////////////////////////////////////////////////////////////////
			};
			static_assert(sizeof(InternalData) == sizeof(Identifier));

			union {
				InternalData data;
				Identifier val;
			};

			constexpr Entity() noexcept: val(IdentifierBad) {};

			//! We need the entity to be braces-constructible and at the same type prevent it from
			//! getting constructed accidentally from an int (e.g .Entity::id()). Therefore, only
			//! allow Entity(Identifier) to be used.
			template <typename T, typename = std::enable_if_t<std::is_same_v<T, Identifier>>>
			constexpr Entity(T value) noexcept: val(value) {}

			//! Special constructor for cnt::ilist
			Entity(EntityId id, IdentifierData gen) noexcept {
				val = 0;
				data.id = id;
				data.gen = gen;
			}

			Entity(EntityId id, IdentifierData gen, bool isEntity, bool isPair, EntityKind kind) noexcept {
				data.id = id;
				data.gen = gen;
				data.ent = isEntity;
				data.pair = isPair;
				data.kind = kind;
				data.unused = 0;
			}

			GAIA_NODISCARD constexpr auto id() const noexcept {
				return (uint32_t)data.id;
			}

			GAIA_NODISCARD constexpr auto gen() const noexcept {
				return (uint32_t)data.gen;
			}

			GAIA_NODISCARD constexpr bool entity() const noexcept {
				return data.ent != 0;
			}

			GAIA_NODISCARD constexpr bool pair() const noexcept {
				return data.pair != 0;
			}

			GAIA_NODISCARD constexpr auto kind() const noexcept {
				return (EntityKind)data.kind;
			}

			GAIA_NODISCARD constexpr auto value() const noexcept {
				return val;
			}

			GAIA_NODISCARD constexpr bool operator==(Entity other) const noexcept {
				return value() == other.value();
			}

			GAIA_NODISCARD constexpr bool operator!=(Entity other) const noexcept {
				return value() != other.value();
			}

			GAIA_NODISCARD constexpr bool operator<(Entity other) const noexcept {
				return value() < other.value();
			}
			GAIA_NODISCARD constexpr bool operator<=(Entity other) const noexcept {
				return value() <= other.value();
			}

			GAIA_NODISCARD constexpr bool operator>(Entity other) const noexcept {
				return value() > other.value();
			}
			GAIA_NODISCARD constexpr bool operator>=(Entity other) const noexcept {
				return value() >= other.value();
			}
		};

		inline static const Entity EntityBad = Entity(IdentifierBad);

		//! Hashmap lookup structure used for Entity
		struct EntityLookupKey {
			using LookupHash = core::direct_hash_key<uint64_t>;

		private:
			//! Entity
			Entity m_entity;
			//! Entity hash
			LookupHash m_hash;

			static LookupHash calc(Entity entity) {
				return {core::calculate_hash64(entity.value())};
			}

		public:
			static constexpr bool IsDirectHashKey = true;

			EntityLookupKey() = default;
			explicit EntityLookupKey(Entity entity): m_entity(entity), m_hash(calc(entity)) {}
			~EntityLookupKey() = default;

			EntityLookupKey(const EntityLookupKey&) = default;
			EntityLookupKey(EntityLookupKey&&) = default;
			EntityLookupKey& operator=(const EntityLookupKey&) = default;
			EntityLookupKey& operator=(EntityLookupKey&&) = default;

			Entity entity() const {
				return m_entity;
			}

			size_t hash() const {
				return (size_t)m_hash.hash;
			}

			bool operator==(const EntityLookupKey& other) const {
				if GAIA_LIKELY (m_hash != other.m_hash)
					return false;

				return m_entity == other.m_entity;
			}

			bool operator!=(const EntityLookupKey& other) const {
				return !operator==(other);
			}
		};

		inline static const EntityLookupKey EntityBadLookupKey = EntityLookupKey(EntityBad);

		//! Component used to describe the entity name
		struct EntityDesc {
			const char* name{};
			uint32_t len{};
		};

		//----------------------------------------------------------------------
		// Pair
		//----------------------------------------------------------------------

		//! Wrapper for two Entities forming a relationship pair.
		template <>
		class pair<Entity, Entity>: public detail::pair_base {
			Entity m_first;
			Entity m_second;

		public:
			pair(Entity a, Entity b) noexcept: m_first(a), m_second(b) {}

			operator Entity() const noexcept {
				return Entity(
						m_first.id(), m_second.id(),
						// Pairs have no way of telling gen and uni entities apart.
						// Therefore, for first, we use the entity bit as Gen/Uni...
						(bool)m_first.kind(),
						// Always true for pairs
						true,
						// ... and for second, we use the kind bit.
						m_second.kind());
			}

			Entity first() const noexcept {
				return m_first;
			}

			Entity second() const noexcept {
				return m_second;
			}

			bool operator==(const pair& other) const {
				return m_first == other.m_first && m_second == other.m_second;
			}
			bool operator!=(const pair& other) const {
				return !operator==(other);
			}
		};

		using Pair = pair<Entity, Entity>;

		//----------------------------------------------------------------------
		// Core components
		//----------------------------------------------------------------------

		namespace detail {
			template <typename T, typename U = void>
			struct actual_type {
				using type = typename component_type<T>::type;
			};

			template <typename T>
			struct actual_type<T, std::enable_if_t<is_pair<T>::value>> {
				using storage_type = typename T::type;
				using type = typename component_type<storage_type>::type;
			};
		} // namespace detail

		template <typename T>
		using actual_type_t = typename detail::actual_type<T>::type;

		//----------------------------------------------------------------------
		// Core components
		//----------------------------------------------------------------------

		// Core component. The entity it is attached to is ignored by queries
		struct Core_ {};
		// struct EntityDesc;
		// struct Component;
		struct OnDelete_ {};
		struct OnDeleteTarget_ {};
		struct Remove_ {};
		struct Delete_ {};
		struct Error_ {};
		struct Requires_ {};
		struct CantCombine_ {};
		struct Exclusive_ {};
		struct Acyclic_ {};
		struct All_ {};
		struct ChildOf_ {};
		struct Is_ {};
		struct Traversable_ {};
		// struct System2_;
		struct DependsOn_ {};

		// Query variables
		struct _Var0 {};
		struct _Var1 {};
		struct _Var2 {};
		struct _Var3 {};
		struct _Var4 {};
		struct _Var5 {};
		struct _Var6 {};
		struct _Var7 {};

		//----------------------------------------------------------------------
		// Core component entities
		//----------------------------------------------------------------------

		// Core component. The entity it is attached to is ignored by queries
		inline Entity Core = Entity(0, 0, false, false, EntityKind::EK_Gen);
		inline Entity GAIA_ID(EntityDesc) = Entity(1, 0, false, false, EntityKind::EK_Gen);
		inline Entity GAIA_ID(Component) = Entity(2, 0, false, false, EntityKind::EK_Gen);
		// Cleanup rules
		inline Entity OnDelete = Entity(3, 0, false, false, EntityKind::EK_Gen);
		inline Entity OnDeleteTarget = Entity(4, 0, false, false, EntityKind::EK_Gen);
		inline Entity Remove = Entity(5, 0, false, false, EntityKind::EK_Gen);
		inline Entity Delete = Entity(6, 0, false, false, EntityKind::EK_Gen);
		inline Entity Error = Entity(7, 0, false, false, EntityKind::EK_Gen);
		// Entity dependencies
		inline Entity Requires = Entity(8, 0, false, false, EntityKind::EK_Gen);
		inline Entity CantCombine = Entity(9, 0, false, false, EntityKind::EK_Gen);
		inline Entity Exclusive = Entity(10, 0, false, false, EntityKind::EK_Gen);
		// Graph properties
		inline Entity Acyclic = Entity(11, 0, false, false, EntityKind::EK_Gen);
		inline Entity Traversable = Entity(12, 0, false, false, EntityKind::EK_Gen);
		// Wildcard query entity
		inline Entity All = Entity(13, 0, false, false, EntityKind::EK_Gen);
		// Entity representing a physical hierarchy.
		// When the relationship target is deleted all children are deleted as well.
		inline Entity ChildOf = Entity(14, 0, false, false, EntityKind::EK_Gen);
		// Alias for a base entity/inheritance
		inline Entity Is = Entity(15, 0, false, false, EntityKind::EK_Gen);
		// Systems
		inline Entity System2 = Entity(16, 0, false, false, EntityKind::EK_Gen);
		inline Entity DependsOn = Entity(17, 0, false, false, EntityKind::EK_Gen);
		// Query variables
		inline Entity Var0 = Entity(18, 0, false, false, EntityKind::EK_Gen);
		inline Entity Var1 = Entity(19, 0, false, false, EntityKind::EK_Gen);
		inline Entity Var2 = Entity(20, 0, false, false, EntityKind::EK_Gen);
		inline Entity Var3 = Entity(21, 0, false, false, EntityKind::EK_Gen);
		inline Entity Var4 = Entity(22, 0, false, false, EntityKind::EK_Gen);
		inline Entity Var5 = Entity(23, 0, false, false, EntityKind::EK_Gen);
		inline Entity Var6 = Entity(24, 0, false, false, EntityKind::EK_Gen);
		inline Entity Var7 = Entity(25, 0, false, false, EntityKind::EK_Gen);

		// Always has to match the last internal entity
		inline Entity GAIA_ID(LastCoreComponent) = Var7;

		//----------------------------------------------------------------------
		// Helper functions
		//----------------------------------------------------------------------

		GAIA_NODISCARD inline bool is_wildcard(EntityId entityId) {
			return entityId == All.id();
		}

		GAIA_NODISCARD inline bool is_wildcard(Entity entity) {
			return entity.pair() && (is_wildcard(entity.id()) || is_wildcard(entity.gen()));
		}

		GAIA_NODISCARD inline bool is_wildcard(Pair pair) {
			return pair.first() == All || pair.second() == All;
		}

		GAIA_NODISCARD inline bool is_variable(EntityId entityId) {
			return entityId <= Var7.id() && entityId >= Var0.id();
		}

		GAIA_NODISCARD inline bool is_variable(Entity entity) {
			return entity.id() <= Var7.id() && entity.id() >= Var0.id();
		}

		GAIA_NODISCARD inline bool is_variable(Pair pair) {
			return is_variable(pair.first()) || is_variable(pair.second());
		}

	} // namespace ecs

	namespace cnt {
		template <>
		struct to_sparse_id<ecs::Entity> {
			static sparse_id get(const ecs::Entity& item) noexcept {
				// Cut off the flags
				return item.id();
			}
		};
	} // namespace cnt
} // namespace gaia
/*** End of inlined file: id.h ***/

namespace gaia {
	namespace ecs {
		//----------------------------------------------------------------------
		// Component-related types
		//----------------------------------------------------------------------

		using ComponentVersion = uint32_t;
		using ChunkDataVersionOffset = uint8_t;
		using CompOffsetMappingIndex = uint8_t;
		using ChunkDataOffset = uint16_t;
		using ComponentLookupHash = core::direct_hash_key<uint64_t>;
		using EntitySpan = std::span<const Entity>;
		using EntitySpanMut = std::span<Entity>;
		using ComponentSpan = std::span<const Component>;
		using ChunkDataOffsetSpan = std::span<const ChunkDataOffset>;
		using SortComponentCond = core::is_smaller<Entity>;

		//----------------------------------------------------------------------
		// Component storage
		//----------------------------------------------------------------------

		enum class DataStorageType : uint32_t {
			Table, //< Data stored in a table
			Sparse, //< Data stored in a sparse set

			Count = 2
		};

#ifndef GAIA_STORAGE
	#define GAIA_STORAGE(storage_name)                                                                                   \
		static constexpr auto gaia_Storage_Type = ::gaia::ecs::DataStorageType::storage_name
#endif

		namespace detail {
			template <typename, typename = void>
			struct storage_type {
				static constexpr DataStorageType value = DataStorageType::Table;
			};
			template <typename T>
			struct storage_type<T, std::void_t<decltype(T::gaia_Storage_Type)>> {
				static constexpr DataStorageType value = T::gaia_Storage_Type;
			};
		} // namespace detail

		template <typename T>
		inline constexpr DataStorageType storage_type_v = detail::storage_type<T>::value;

		//----------------------------------------------------------------------
		// Component verification
		//----------------------------------------------------------------------

		namespace detail {
			template <typename T>
			struct is_component_size_valid: std::bool_constant<sizeof(T) < Component::MaxComponentSizeInBytes> {};

			template <typename T>
			struct is_component_type_valid:
					std::bool_constant<
							// SoA types need to be trivial. No restrictions otherwise.
							(!mem::is_soa_layout_v<T> || std::is_trivially_copyable_v<T>)> {};
		} // namespace detail

		//----------------------------------------------------------------------
		// Component verification
		//----------------------------------------------------------------------

		template <typename T>
		constexpr void verify_comp() {
			using U = typename actual_type_t<T>::TypeOriginal;

			// Make sure we only use this for "raw" types
			static_assert(
					core::is_raw_v<U>,
					"Components have to be \"raw\" types - no arrays, no const, reference, pointer or volatile");
		}

		//----------------------------------------------------------------------
		// Component lookup hash
		//----------------------------------------------------------------------

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

		//! Calculates a lookup hash from the provided entities
		//! \param comps Span of entities
		//! \return Lookup hash
		GAIA_NODISCARD inline ComponentLookupHash calc_lookup_hash(EntitySpan comps) noexcept {
			const auto compsSize = comps.size();
			if (compsSize == 0)
				return {0};

			auto hash = core::calculate_hash64(comps[0].value());
			GAIA_FOR2(1, compsSize) {
				hash = core::hash_combine(hash, core::calculate_hash64(comps[i].value()));
			}
			return {hash};
		}

		//! Located the index at which the provided component id is located in the component array
		//! \param pComps Pointer to the start of the component array
		//! \param entity Entity we search for
		//! \return Index of the component id in the array
		//! \warning The component id must be present in the array
		template <uint32_t MAX_COMPONENTS>
		GAIA_NODISCARD inline uint32_t comp_idx(const Entity* pComps, Entity entity) {
			// We let the compiler know the upper iteration bound at compile-time.
			// This way it can optimize better (e.g. loop unrolling, vectorization).
			GAIA_FOR(MAX_COMPONENTS) {
				if (pComps[i] == entity)
					return i;
			}

			GAIA_ASSERT(false);
			return BadIndex;
		}
	} // namespace ecs
} // namespace gaia
/*** End of inlined file: component.h ***/

namespace gaia {
	namespace ecs {
		class World;
		const char* entity_name(const World& world, Entity entity);
		const char* entity_name(const World& world, EntityId entityId);

		using ArchetypeGraphEdge = ArchetypeIdHashPair;

		class ArchetypeGraph {
			using EdgeMap = cnt::map<EntityLookupKey, ArchetypeGraphEdge>;

			//! Map of edges in the archetype graph when adding components
			EdgeMap m_edgesAdd;
			//! Map of edges in the archetype graph when removing components
			EdgeMap m_edgesDel;

		private:
			void add_edge(EdgeMap& edges, Entity entity, ArchetypeId archetypeId, ArchetypeIdHash hash) {
#if !GAIA_DISABLE_ASSERTS
				const auto ret =
#endif
						edges.try_emplace(EntityLookupKey(entity), ArchetypeGraphEdge{archetypeId, hash});
#if !GAIA_DISABLE_ASSERTS
				// If the result already exists make sure the new one is the same
				if (!ret.second) {
					const auto it = edges.find(EntityLookupKey(entity));
					GAIA_ASSERT(it != edges.end());
					GAIA_ASSERT(it->second.id == archetypeId);
					GAIA_ASSERT(it->second.hash == hash);
				}
#endif
			}

			void del_edge(EdgeMap& edges, Entity entity) {
				edges.erase(EntityLookupKey(entity));
			}

			GAIA_NODISCARD ArchetypeGraphEdge find_edge(const EdgeMap& edges, Entity entity) const {
				const auto it = edges.find(EntityLookupKey(entity));
				return it != edges.end() ? it->second : ArchetypeIdHashPairBad;
			}

		public:
			//! Creates an "add" edge in the graph leading to the target archetype.
			//! \param entity Edge entity.
			//! \param archetypeId Target archetype.
			void add_edge_right(Entity entity, ArchetypeId archetypeId, ArchetypeIdHash hash) {
				add_edge(m_edgesAdd, entity, archetypeId, hash);
			}

			//! Creates a "del" edge in the graph leading to the target archetype.
			//! \param entity Edge entity.
			//! \param archetypeId Target archetype.
			void add_edge_left(Entity entity, ArchetypeId archetypeId, ArchetypeIdHash hash) {
				add_edge(m_edgesDel, entity, archetypeId, hash);
			}

			//! Deletes the "add" edge formed by the entity \param entity.
			void del_edge_right(Entity entity) {
				del_edge(m_edgesAdd, entity);
			}

			//! Deletes the "del" edge formed by the entity \param entity.
			void del_edge_left(Entity entity) {
				del_edge(m_edgesDel, entity);
			}

			//! Checks if an archetype graph "add" edge with entity \param entity exists.
			//! \return Archetype id of the target archetype if the edge is found. ArchetypeGraphEdgeBad otherwise.
			GAIA_NODISCARD ArchetypeGraphEdge find_edge_right(Entity entity) const {
				return find_edge(m_edgesAdd, entity);
			}

			//! Checks if an archetype graph "del" edge with entity \param entity exists.
			//! \return Archetype id of the target archetype if the edge is found. ArchetypeGraphEdgeBad otherwise.
			GAIA_NODISCARD ArchetypeGraphEdge find_edge_left(Entity entity) const {
				return find_edge(m_edgesDel, entity);
			}

			GAIA_NODISCARD auto& right_edges() {
				return m_edgesAdd;
			}

			GAIA_NODISCARD const auto& right_edges() const {
				return m_edgesAdd;
			}

			GAIA_NODISCARD auto& left_edges() {
				return m_edgesDel;
			}

			GAIA_NODISCARD const auto& left_edges() const {
				return m_edgesDel;
			}

			void diag(const World& world) const {
				auto diagEdge = [&](const auto& edges) {
					for (const auto& edge: edges) {
						const auto entity = edge.first.entity();
						if (entity.pair()) {
							const auto* name0 = entity_name(world, entity.id());
							const auto* name1 = entity_name(world, entity.gen());
							GAIA_LOG_N(
									"    pair [%u:%u], %s -> %s, aid:%u",
									//
									entity.id(), entity.gen(), name0, name1, edge.second.id);
						} else {
							const auto* name = entity_name(world, entity);
							GAIA_LOG_N(
									"    ent [%u:%u], %s [%s], aid:%u",
									//
									entity.id(), entity.gen(), name, EntityKindString[entity.kind()], edge.second.id);
						}
					}
				};

				// Add edges (movement towards the leafs)
				if (!m_edgesAdd.empty()) {
					GAIA_LOG_N("  Add edges - count:%u", (uint32_t)m_edgesAdd.size());
					diagEdge(m_edgesAdd);
				}

				// Delete edges (movement towards the root)
				if (!m_edgesDel.empty()) {
					GAIA_LOG_N("  Del edges - count:%u", (uint32_t)m_edgesDel.size());
					diagEdge(m_edgesDel);
				}
			}
		};
	} // namespace ecs
} // namespace gaia
/*** End of inlined file: archetype_graph.h ***/


/*** Start of inlined file: chunk.h ***/
#pragma once

#include <cstdint>
#include <cstring>
#include <tuple>
#include <type_traits>
#include <utility>


/*** Start of inlined file: chunk_allocator.h ***/
#pragma once

#include <cstdint>

#if GAIA_ECS_CHUNK_ALLOCATOR
	#include <cinttypes>


/*** Start of inlined file: dyn_singleton.h ***/
#pragma once

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
/*** End of inlined file: dyn_singleton.h ***/


/*** Start of inlined file: common.h ***/
#pragma once

#include <cstdint>

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

/*** End of inlined file: common.h ***/

#endif

namespace gaia {
	namespace ecs {
		//! Size of one allocated block of memory
		static constexpr uint32_t MaxMemoryBlockSize = 16384;
		//! Unusable area at the beginning of the allocated block designated for special purposes
		static constexpr uint32_t MemoryBlockUsableOffset = sizeof(uintptr_t);

		inline constexpr uint16_t mem_block_size(uint32_t sizeType) {
			return sizeType != 0 ? MaxMemoryBlockSize : MaxMemoryBlockSize / 2;
		}

		inline constexpr uint8_t mem_block_size_type(uint32_t sizeBytes) {
			return (uint8_t)(sizeBytes > MaxMemoryBlockSize / 2);
		}

#if GAIA_ECS_CHUNK_ALLOCATOR
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

			//! Allocator for ECS Chunks. Memory is organized in pages of chunks.
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
					//! Index in the list of pages
					uint32_t m_idx;

					//! Block size type, 0=8K, 1=16K blocks
					uint32_t m_sizeType : 1;
					//! Number of blocks in the block array
					uint32_t m_blockCnt: NBlocks_Bits;
					//! Number of used blocks out of NBlocks
					uint32_t m_usedBlocks: NBlocks_Bits;
					//! Index of the next block to recycle
					uint32_t m_nextFreeBlock: NBlocks_Bits;
					//! Number of blocks to recycle
					uint32_t m_freeBlocks: NBlocks_Bits;
					//! Free bits to use in the future
					// uint32_t m_unused : 7;

					//! Implicit list of blocks
					BlockArray m_blocks;

					MemoryPage(void* ptr, uint8_t sizeType):
							m_data(ptr), m_idx(0), m_sizeType(sizeType), m_blockCnt(0), m_usedBlocks(0), m_nextFreeBlock(0),
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

					GAIA_NODISCARD void* alloc_block() {
						auto StoreBlockAddress = [&](uint32_t index) {
							// Encode info about chunk's page in the memory block.
							// The actual pointer returned is offset by MemoryBlockUsableOffset bytes
							uint8_t* pMemoryBlock = (uint8_t*)m_data + index * mem_block_size(m_sizeType);
							GAIA_ASSERT((uintptr_t)pMemoryBlock % 16 == 0);
							mem::unaligned_ref<uintptr_t>{pMemoryBlock} = (uintptr_t)this;
							return (void*)(pMemoryBlock + MemoryBlockUsableOffset);
						};

						// We don't want to go out of range for new blocks
						GAIA_ASSERT(!full() && "Trying to allocate too many blocks!");

						if (m_freeBlocks == 0U) {
							const auto index = m_blockCnt;
							++m_usedBlocks;
							++m_blockCnt;
							write_block_idx(index, index);

							return StoreBlockAddress(index);
						}

						GAIA_ASSERT(m_nextFreeBlock < m_blockCnt && "Block allocator recycle list broken!");

						++m_usedBlocks;
						--m_freeBlocks;

						const auto index = m_nextFreeBlock;
						m_nextFreeBlock = read_block_idx(m_nextFreeBlock);

						return StoreBlockAddress(index);
					}

					void free_block(void* pBlock) {
						GAIA_ASSERT(m_usedBlocks > 0);
						GAIA_ASSERT(m_freeBlocks <= NBlocks);

						// Offset the chunk memory so we get the real block address
						const auto* pMemoryBlock = (uint8_t*)pBlock - MemoryBlockUsableOffset;
						const auto blckAddr = (uintptr_t)pMemoryBlock;
						GAIA_ASSERT(blckAddr % 16 == 0);
						const auto dataAddr = (uintptr_t)m_data;
						const auto blockIdx = (uint32_t)((blckAddr - dataAddr) / mem_block_size(m_sizeType));

						// Update our implicit list
						if (m_freeBlocks == 0U)
							write_block_idx(blockIdx, InvalidBlockId);
						else
							write_block_idx(blockIdx, m_nextFreeBlock);
						m_nextFreeBlock = blockIdx;

						++m_freeBlocks;
						--m_usedBlocks;
					}

					GAIA_NODISCARD uint32_t used_blocks_cnt() const {
						return m_usedBlocks;
					}

					GAIA_NODISCARD bool full() const {
						return used_blocks_cnt() >= NBlocks;
					}

					GAIA_NODISCARD bool empty() const {
						return used_blocks_cnt() == 0;
					}
				};

				struct MemoryPageContainer {
					//! Array of available pages
					cnt::darray<MemoryPage*> pagesFree;
					//! Array of full pages
					cnt::darray<MemoryPage*> pagesFull;

					GAIA_NODISCARD bool empty() const {
						return pagesFree.empty() && pagesFull.empty();
					}
				};

				//! Container for pages storing various-sized chunks
				MemoryPageContainer m_pages[2];

				//! When true, destruction has been requested
				bool m_isDone = false;

			private:
				ChunkAllocatorImpl() = default;

				void on_delete() {
					flush();

					// Make sure there are no leaks
					auto memStats = stats();
					for (const auto& s: memStats.stats) {
						if (s.mem_total != 0) {
							GAIA_ASSERT2(false, "ECS leaking memory");
							GAIA_LOG_W("ECS leaking memory!");
							diag();
						}
					}
				}

			public:
				~ChunkAllocatorImpl() {
					on_delete();
				}

				ChunkAllocatorImpl(ChunkAllocatorImpl&& world) = delete;
				ChunkAllocatorImpl(const ChunkAllocatorImpl& world) = delete;
				ChunkAllocatorImpl& operator=(ChunkAllocatorImpl&&) = delete;
				ChunkAllocatorImpl& operator=(const ChunkAllocatorImpl&) = delete;

				//! Allocates memory
				void* alloc(uint32_t bytesWanted) {
					GAIA_ASSERT(bytesWanted <= MaxMemoryBlockSize);

					void* pBlock = nullptr;
					MemoryPage* pPage = nullptr;

					const auto sizeType = mem_block_size_type(bytesWanted);
					auto& container = m_pages[sizeType];

					// Find first page with available space
					for (auto* p: container.pagesFree) {
						if (p->full())
							continue;
						pPage = p;
						break;
					}
					if (pPage == nullptr) {
						// Allocate a new page if no free page was found
						pPage = alloc_page(sizeType);
						container.pagesFree.push_back(pPage);
					}

					// Allocate a new chunk of memory
					pBlock = pPage->alloc_block();

					// Handle full pages
					if (pPage->full()) {
						// Remove the page from the open list and update the swapped page's pointer
						container.pagesFree.back()->m_idx = 0;
						core::swap_erase(container.pagesFree, 0);

						// Move our page to the full list
						pPage->m_idx = (uint32_t)container.pagesFull.size();
						container.pagesFull.push_back(pPage);
					}

					return pBlock;
				}

				GAIA_CLANG_WARNING_PUSH()
				// Memory is aligned so we can silence this warning
				GAIA_CLANG_WARNING_DISABLE("-Wcast-align")

				//! Releases memory allocated for pointer
				void free(void* pBlock) {
					// Decode the page from the address
					const auto pageAddr = *(uintptr_t*)((uint8_t*)pBlock - MemoryBlockUsableOffset);
					GAIA_ASSERT(pageAddr % 16 == 0);
					auto* pPage = (MemoryPage*)pageAddr;
					const bool wasFull = pPage->full();

					auto& container = m_pages[pPage->m_sizeType];

	#if GAIA_ASSERT_ENABLED
					if (wasFull) {
						const auto res = core::has_if(container.pagesFull, [&](auto* page) {
							return page == pPage;
						});
						GAIA_ASSERT(res && "Memory page couldn't be found among full pages");
					} else {
						const auto res = core::has_if(container.pagesFree, [&](auto* page) {
							return page == pPage;
						});
						GAIA_ASSERT(res && "Memory page couldn't be found among free pages");
					}
	#endif

					// Free the chunk
					pPage->free_block(pBlock);

					// Update lists
					if (wasFull) {
						// Our page is no longer full. Remove it from the full list and update the swapped page's pointer
						container.pagesFull.back()->m_idx = pPage->m_idx;
						core::swap_erase(container.pagesFull, pPage->m_idx);

						// Move our page to the open list
						pPage->m_idx = (uint32_t)container.pagesFree.size();
						container.pagesFree.push_back(pPage);
					}

					// Special handling for the allocator signaled to destroy itself
					if (m_isDone) {
						// Remove the page right away
						if (pPage->empty()) {
							GAIA_ASSERT(!container.pagesFree.empty());
							container.pagesFree.back()->m_idx = pPage->m_idx;
							core::swap_erase(container.pagesFree, pPage->m_idx);
						}

						try_delete_this();
					}
				}

				GAIA_CLANG_WARNING_POP()

				//! Returns allocator statistics
				ChunkAllocatorStats stats() const {
					ChunkAllocatorStats stats;
					stats.stats[0] = page_stats(0);
					stats.stats[1] = page_stats(1);
					return stats;
				}

				//! Flushes unused memory
				void flush() {
					auto flushPages = [](MemoryPageContainer& container) {
						for (uint32_t i = 0; i < container.pagesFree.size();) {
							auto* pPage = container.pagesFree[i];

							// Skip non-empty pages
							if (!pPage->empty()) {
								++i;
								continue;
							}

							GAIA_ASSERT(pPage->m_idx == i);
							container.pagesFree.back()->m_idx = i;
							core::swap_erase(container.pagesFree, i);
							free_page(pPage);
						}
					};

					for (auto& c: m_pages)
						flushPages(c);
				}

				//! Performs diagnostics of the memory used.
				void diag() const {
					auto diagPage = [](const ChunkAllocatorPageStats& stats) {
						GAIA_LOG_N("ChunkAllocator stats");
						GAIA_LOG_N("  Allocated: %" PRIu64 " B", stats.mem_total);
						GAIA_LOG_N("  Used: %" PRIu64 " B", stats.mem_total - stats.mem_used);
						GAIA_LOG_N("  Overhead: %" PRIu64 " B", stats.mem_used);
						GAIA_LOG_N("  Utilization: %.1f%%", 100.0 * ((double)stats.mem_used / (double)stats.mem_total));
						GAIA_LOG_N("  Pages: %u", stats.num_pages);
						GAIA_LOG_N("  Free pages: %u", stats.num_pages_free);
					};

					diagPage(page_stats(0));
					diagPage(page_stats(1));
				}

			private:
				static MemoryPage* alloc_page(uint8_t sizeType) {
					const uint32_t size = mem_block_size(sizeType) * MemoryPage::NBlocks;
					auto* pPageData = mem::AllocHelper::alloc_alig<uint8_t>("Chunk", 16U, size);
					auto* pMemoryPage = mem::AllocHelper::alloc<MemoryPage>("MemoryPage");
					return new (pMemoryPage) MemoryPage(pPageData, sizeType);
				}

				static void free_page(MemoryPage* pMemoryPage) {
					mem::AllocHelper::free_alig("Chunk", pMemoryPage->m_data);
					pMemoryPage->~MemoryPage();
					mem::AllocHelper::free("MemoryPage", pMemoryPage);
				}

				void done() {
					m_isDone = true;
				}

				void try_delete_this() {
					// When there is nothing left, delete the allocator
					bool allEmpty = true;
					for (const auto& c: m_pages)
						allEmpty = allEmpty && c.empty();
					if (allEmpty)
						delete this;
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

#endif

	} // namespace ecs
} // namespace gaia

/*** End of inlined file: chunk_allocator.h ***/


/*** Start of inlined file: chunk_header.h ***/
#pragma once

#include <cstdint>

namespace gaia {
	namespace ecs {
		class ComponentCache;
		struct ComponentCacheItem;

		struct ChunkDataOffsets {
			//! Byte at which the first version number is located
			ChunkDataVersionOffset firstByte_Versions{};
			//! Byte at which the first entity id is located
			ChunkDataOffset firstByte_CompEntities{};
			//! Byte at which the first component id is located
			ChunkDataOffset firstByte_Records{};
			//! Byte at which the first entity is located
			ChunkDataOffset firstByte_EntityData{};
		};

		struct ComponentRecord {
			//! Component id
			Component comp;
			//! Pointer to where the first instance of the component is stored
			uint8_t* pData;
			//! Pointer to component cache record
			const ComponentCacheItem* pItem;
		};

		struct ChunkRecords {
			//! Pointer to where component versions are stored
			ComponentVersion* pVersions{};
			//! Pointer to where (component) entities are stored
			Entity* pCompEntities{};
			//! Pointer to the array of component records
			ComponentRecord* pRecords{};
			//! Pointer to the array of entities
			Entity* pEntities{};
		};

		struct ChunkHeader final {
			static constexpr uint32_t MAX_COMPONENTS_BITS = 5U;
			//! Maximum number of components on archetype
			static constexpr uint32_t MAX_COMPONENTS = 1U << MAX_COMPONENTS_BITS;

			//! Maximum number of entities per chunk.
			//! Defined as sizeof(big_chunk) / sizeof(entity)
			static constexpr uint16_t MAX_CHUNK_ENTITIES = (mem_block_size(1) - 64) / sizeof(Entity);
			static constexpr uint16_t MAX_CHUNK_ENTITIES_BITS = (uint16_t)core::count_bits(MAX_CHUNK_ENTITIES);

			static constexpr uint16_t CHUNK_LIFESPAN_BITS = 4;
			//! Number of ticks before empty chunks are removed
			static constexpr uint16_t MAX_CHUNK_LIFESPAN = (1 << CHUNK_LIFESPAN_BITS) - 1;

			static constexpr uint16_t CHUNK_LOCKS_BITS = 3;
			//! Number of locks the chunk can acquire
			static constexpr uint16_t MAX_CHUNK_LOCKS = (1 << CHUNK_LOCKS_BITS) - 1;

			//! Component cache reference
			const ComponentCache* cc;
			//! Chunk index in its archetype list
			uint32_t index;
			//! Total number of entities in the chunk.
			uint16_t count;
			//! Number of enabled entities in the chunk.
			uint16_t countEnabled;
			//! Capacity (copied from the owner archetype).
			uint16_t capacity;

			//! Index of the first enabled entity in the chunk
			uint16_t rowFirstEnabledEntity: MAX_CHUNK_ENTITIES_BITS;
			//! True if there's any generic component that requires custom construction
			uint16_t hasAnyCustomGenCtor : 1;
			//! True if there's any unique component that requires custom construction
			uint16_t hasAnyCustomUniCtor : 1;
			//! True if there's any generic component that requires custom destruction
			uint16_t hasAnyCustomGenDtor : 1;
			//! True if there's any unique component that requires custom destruction
			uint16_t hasAnyCustomUniDtor : 1;
			//! Chunk size type. This tells whether it's 8K or 16K
			uint16_t sizeType : 1;
			//! When it hits 0 the chunk is scheduled for deletion
			uint16_t lifespanCountdown: CHUNK_LIFESPAN_BITS;
			//! True if deleted, false otherwise
			uint16_t dead : 1;
			//! Updated when chunks are being iterated. Used to inform of structural changes when they shouldn't happen.
			uint16_t structuralChangesLocked: CHUNK_LOCKS_BITS;
			//! Empty space for future use
			uint16_t unused : 8;

			//! Number of generic entities/components
			uint8_t genEntities;
			//! Number of components on the archetype
			uint8_t cntEntities;
			//! Version of the world (stable pointer to parent world's world version)
			uint32_t& worldVersion;

			static inline uint32_t s_worldVersionDummy = 0;
			ChunkHeader(): worldVersion(s_worldVersionDummy) {}

			ChunkHeader(
					const ComponentCache& compCache, uint32_t chunkIndex, uint16_t cap, uint8_t genEntitiesCnt, uint16_t st,
					uint32_t& version):
					cc(&compCache), index(chunkIndex), count(0), countEnabled(0), capacity(cap),
					//
					rowFirstEnabledEntity(0), hasAnyCustomGenCtor(0), hasAnyCustomUniCtor(0), hasAnyCustomGenDtor(0),
					hasAnyCustomUniDtor(0), sizeType(st), lifespanCountdown(0), dead(0), structuralChangesLocked(0), unused(0),
					//
					genEntities(genEntitiesCnt), cntEntities(0), worldVersion(version) {
				// Make sure the alignment is right
				GAIA_ASSERT(uintptr_t(this) % (sizeof(size_t)) == 0);
			}

			bool has_disabled_entities() const {
				return rowFirstEnabledEntity > 0;
			}

			bool has_enabled_entities() const {
				return countEnabled > 0;
			}
		};
	} // namespace ecs
} // namespace gaia

/*** End of inlined file: chunk_header.h ***/


/*** Start of inlined file: component_cache.h ***/
#pragma once

#include <cinttypes>
#include <cstdint>
#include <type_traits>


/*** Start of inlined file: hashing_string.h ***/
#pragma once

#include <cstdint>

namespace gaia {
	namespace core {
		template <uint32_t MaxLen>
		struct StringLookupKey {
			using LookupHash = core::direct_hash_key<uint32_t>;

		private:
			//! Pointer to the string
			const char* m_pStr;
			//! Length of the string
			uint32_t m_len : 31;
			//! 1 - owned (lifetime managed by the framework), 0 - non-owned (lifetime user-managed)
			uint32_t m_owned : 1;
			//! String hash
			LookupHash m_hash;

			static uint32_t len(const char* pStr) {
				GAIA_FOR(MaxLen) {
					if (pStr[i] == 0)
						return i;
				}
				GAIA_ASSERT2(false, "Only null-terminated strings up to MaxLen characters are supported");
				return BadIndex;
			}

			static LookupHash calc(const char* pStr, uint32_t len) {
				return {static_cast<uint32_t>(core::calculate_hash64(pStr, len))};
			}

		public:
			static constexpr bool IsDirectHashKey = true;

			StringLookupKey(): m_pStr(nullptr), m_len(0), m_owned(0), m_hash({0}) {}
			//! Constructor calculating hash and length from the provided string \param pStr
			//! \warning String has to be null-terminated and up to MaxLen characters long.
			//!          Undefined behavior otherwise.
			explicit StringLookupKey(const char* pStr):
					m_pStr(pStr), m_len(len(pStr)), m_owned(0), m_hash(calc(pStr, m_len)) {}
			//! Constructor calculating hash from the provided string \param pStr and \param length
			//! \warning String has to be null-terminated and up to MaxLen characters long.
			//!          Undefined behavior otherwise.
			explicit StringLookupKey(const char* pStr, uint32_t len):
					m_pStr(pStr), m_len(len), m_owned(0), m_hash(calc(pStr, len)) {}
			//! Constructor just for setting values
			explicit StringLookupKey(const char* pStr, uint32_t len, uint32_t owned, LookupHash hash):
					m_pStr(pStr), m_len(len), m_owned(owned), m_hash(hash) {}

			const char* str() const {
				return m_pStr;
			}

			uint32_t len() const {
				return m_len;
			}

			bool owned() const {
				return m_owned == 1;
			}

			uint32_t hash() const {
				return m_hash.hash;
			}

			bool operator==(const StringLookupKey& other) const {
				// Hash doesn't match we don't have a match.
				// Hash collisions are expected to be very unlikely so optimize for this case.
				if GAIA_LIKELY (m_hash != other.m_hash)
					return false;

				// Lengths have to match
				if (m_len != other.m_len)
					return false;

				// Contents have to match
				const auto l = m_len;
				GAIA_ASSUME(l < MaxLen);
				GAIA_FOR(l) {
					if (m_pStr[i] != other.m_pStr[i])
						return false;
				}

				return true;
			}

			bool operator!=(const StringLookupKey& other) const {
				return !operator==(other);
			}
		};
	} // namespace core
} // namespace gaia
/*** End of inlined file: hashing_string.h ***/


/*** Start of inlined file: component_cache_item.h ***/
#pragma once

#include <cstdint>
#include <type_traits>


/*** Start of inlined file: component_desc.h ***/
#pragma once

#include <cstdint>
#include <cstring>
#include <tuple>
#include <type_traits>

namespace gaia {
	namespace ecs {
		namespace detail {
			using ComponentDescId = uint32_t;

			template <typename T>
			struct ComponentDesc final {
				using CT = component_type_t<T>;
				using U = typename component_type_t<T>::Type;
				using DescU = typename CT::TypeFull;

				using FuncCtor = void(void*, uint32_t);
				using FuncDtor = void(void*, uint32_t);
				using FuncCopy = void(void*, void*);
				using FuncMove = void(void*, void*);
				using FuncSwap = void(void*, void*);
				using FuncCmp = bool(const void*, const void*);

				static ComponentDescId id() {
					return meta::type_info::id<DescU>();
				}

				static constexpr ComponentLookupHash hash_lookup() {
					return {meta::type_info::hash<DescU>()};
				}

				static constexpr auto name() {
					return meta::type_info::name<DescU>();
				}

				static constexpr uint32_t size() {
					if constexpr (std::is_empty_v<U>)
						return 0;
					else
						return (uint32_t)sizeof(U);
				}

				static constexpr uint32_t alig() {
					constexpr auto alig = mem::auto_view_policy<U>::Alignment;
					static_assert(alig < Component::MaxAlignment, "Maximum supported alignment for a component is MaxAlignment");
					return alig;
				}

				static uint32_t soa(std::span<uint8_t, meta::StructToTupleMaxTypes> soaSizes) {
					if constexpr (mem::is_soa_layout_v<U>) {
						uint32_t i = 0;
						using TTuple = decltype(meta::struct_to_tuple(std::declval<U>()));
						// is_soa_layout_v is always false for empty types so we know there is at least one element in the tuple
						constexpr auto TTupleSize = std::tuple_size_v<TTuple>;
						static_assert(TTupleSize > 0);
						static_assert(TTupleSize <= meta::StructToTupleMaxTypes);
						core::each_tuple<TTuple>([&](auto&& item) {
							static_assert(sizeof(item) <= 255, "Each member of a SoA component can be at most 255 B long!");
							soaSizes[i] = (uint8_t)sizeof(item);
							++i;
						});
						GAIA_ASSERT(i <= meta::StructToTupleMaxTypes);
						return i;
					} else {
						return 0U;
					}
				}

				static constexpr auto func_ctor() {
					if constexpr (!mem::is_soa_layout_v<U> && !std::is_trivially_constructible_v<U>) {
						return [](void* ptr, uint32_t cnt) {
							core::call_ctor_n((U*)ptr, cnt);
						};
					} else {
						return nullptr;
					}
				}

				static constexpr auto func_dtor() {
					if constexpr (!mem::is_soa_layout_v<U> && !std::is_trivially_destructible_v<U>) {
						return [](void* ptr, uint32_t cnt) {
							core::call_dtor_n((U*)ptr, cnt);
						};
					} else {
						return nullptr;
					}
				}

				static constexpr auto func_copy_ctor() {
					return [](void* GAIA_RESTRICT dst, const void* GAIA_RESTRICT src, uint32_t idxDst, uint32_t idxSrc,
										uint32_t sizeDst, uint32_t sizeSrc) {
						mem::copy_ctor_element<U>((uint8_t*)dst, (const uint8_t*)src, idxDst, idxSrc, sizeDst, sizeSrc);
					};
				}

				static constexpr auto func_move_ctor() {
					return [](void* GAIA_RESTRICT dst, void* GAIA_RESTRICT src, uint32_t idxDst, uint32_t idxSrc,
										uint32_t sizeDst, uint32_t sizeSrc) {
						mem::move_ctor_element<U>((uint8_t*)dst, (uint8_t*)src, idxDst, idxSrc, sizeDst, sizeSrc);
					};
				}

				static constexpr auto func_copy() {
					return [](void* GAIA_RESTRICT dst, const void* GAIA_RESTRICT src, uint32_t idxDst, uint32_t idxSrc,
										uint32_t sizeDst, uint32_t sizeSrc) {
						mem::copy_element<U>((uint8_t*)dst, (const uint8_t*)src, idxDst, idxSrc, sizeDst, sizeSrc);
					};
				}

				static constexpr auto func_move() {
					return [](void* GAIA_RESTRICT dst, void* GAIA_RESTRICT src, uint32_t idxDst, uint32_t idxSrc,
										uint32_t sizeDst, uint32_t sizeSrc) {
						mem::move_element<U>((uint8_t*)dst, (uint8_t*)src, idxDst, idxSrc, sizeDst, sizeSrc);
					};
				}

				static constexpr auto func_swap() {
					return [](void* GAIA_RESTRICT left, void* GAIA_RESTRICT right, uint32_t idxLeft, uint32_t idxRight,
										uint32_t sizeLeft, uint32_t sizeRight) {
						mem::swap_elements<U>((uint8_t*)left, (uint8_t*)right, idxLeft, idxRight, sizeLeft, sizeRight);
					};
				}

				static constexpr auto func_cmp() {
					if constexpr (mem::is_soa_layout_v<U>) {
						return []([[maybe_unused]] const void* left, [[maybe_unused]] const void* right) {
							GAIA_ASSERT(false && "func_cmp for SoA not implemented yet");
							return false;
						};
					} else {
						constexpr bool hasGlobalCmp = core::has_global_equals<U>::value;
						constexpr bool hasMemberCmp = core::has_member_equals<U>::value;
						if constexpr (hasGlobalCmp || hasMemberCmp) {
							return [](const void* left, const void* right) {
								const auto* l = (const U*)left;
								const auto* r = (const U*)right;
								return *l == *r;
							};
						} else {
							// fallback comparison function
							return [](const void* left, const void* right) {
								const auto* l = (const U*)left;
								const auto* r = (const U*)right;
								return memcmp(l, r, sizeof(U)) == 0;
							};
						}
					}
				}
			};
		} // namespace detail
	} // namespace ecs
} // namespace gaia

/*** End of inlined file: component_desc.h ***/

namespace gaia {
	namespace ecs {
		struct ComponentCacheItem final {
			using SymbolLookupKey = core::StringLookupKey<512>;
			using FuncCtor = void(void*, uint32_t);
			using FuncDtor = void(void*, uint32_t);
			using FuncFrom = void(void*, void*, uint32_t, uint32_t, uint32_t, uint32_t);
			using FuncCopy = void(void*, const void*, uint32_t, uint32_t, uint32_t, uint32_t);
			using FuncMove = void(void*, void*, uint32_t, uint32_t, uint32_t, uint32_t);
			using FuncSwap = void(void*, void*, uint32_t, uint32_t, uint32_t, uint32_t);
			using FuncCmp = bool(const void*, const void*);

			//! Component entity
			Entity entity;
			//! Unique component identifier
			Component comp;
			//! Complex hash used for look-ups
			ComponentLookupHash hashLookup;
			//! If component is SoA, this stores how many bytes each of the elements take
			uint8_t soaSizes[meta::StructToTupleMaxTypes];

			//! Component name
			SymbolLookupKey name;
			//! Function to call when the component needs to be constructed
			FuncCtor* func_ctor{};
			//! Function to call when the component needs to be move constructed
			FuncMove* func_move_ctor{};
			//! Function to call when the component needs to be copy constructed
			FuncCopy* func_copy_ctor{};
			//! Function to call when the component needs to be destroyed
			FuncDtor* func_dtor{};
			//! Function to call when the component needs to be copied
			FuncCopy* func_copy{};
			//! Function to call when the component needs to be moved
			FuncMove* func_move{};
			//! Function to call when the component needs to swap
			FuncSwap* func_swap{};
			//! Function to call when comparing two components of the same type
			FuncCmp* func_cmp{};

		private:
			ComponentCacheItem() = default;
			~ComponentCacheItem() = default;

		public:
			ComponentCacheItem(const ComponentCacheItem&) = delete;
			ComponentCacheItem(ComponentCacheItem&&) = delete;
			ComponentCacheItem& operator=(const ComponentCacheItem&) = delete;
			ComponentCacheItem& operator=(ComponentCacheItem&&) = delete;

			void
			ctor_move(void* pDst, void* pSrc, uint32_t idxDst, uint32_t idxSrc, int32_t sizeDst, uint32_t sizeSrc) const {
				GAIA_ASSERT(func_move_ctor != nullptr && (pSrc != pDst || idxSrc != idxDst));
				func_move_ctor(pDst, pSrc, idxDst, idxSrc, sizeDst, sizeSrc);
			}

			void ctor_copy(
					void* pDst, const void* pSrc, uint32_t idxDst, uint32_t idxSrc, uint32_t sizeDst, uint32_t sizeSrc) const {
				GAIA_ASSERT(func_copy_ctor != nullptr && (pSrc != pDst || idxSrc != idxDst));
				func_copy_ctor(pDst, pSrc, idxDst, idxSrc, sizeDst, sizeSrc);
			}

			void dtor(void* pSrc) const {
				if (func_dtor != nullptr)
					func_dtor(pSrc, 1);
			}

			void
			copy(void* pDst, const void* pSrc, uint32_t idxDst, uint32_t idxSrc, uint32_t sizeDst, uint32_t sizeSrc) const {
				GAIA_ASSERT(func_copy != nullptr && (pSrc != pDst || idxSrc != idxDst));
				func_copy(pDst, pSrc, idxDst, idxSrc, sizeDst, sizeSrc);
			}

			void move(void* pDst, void* pSrc, uint32_t idxDst, uint32_t idxSrc, int32_t sizeDst, uint32_t sizeSrc) const {
				GAIA_ASSERT(func_move != nullptr && (pSrc != pDst || idxSrc != idxDst));
				func_move(pDst, pSrc, idxDst, idxSrc, sizeDst, sizeSrc);
			}

			void
			swap(void* pLeft, void* pRight, uint32_t idxLeft, uint32_t idxRight, int32_t sizeDst, uint32_t sizeSrc) const {
				GAIA_ASSERT(func_swap != nullptr);
				func_swap(pLeft, pRight, idxLeft, idxRight, sizeDst, sizeSrc);
			}

			bool cmp(const void* pLeft, const void* pRight) const {
				GAIA_ASSERT(pLeft != pRight);
				GAIA_ASSERT(func_cmp != nullptr);
				return func_cmp(pLeft, pRight);
			}

			GAIA_NODISCARD uint32_t calc_new_mem_offset(uint32_t addr, size_t N) const noexcept {
				if (comp.soa() == 0) {
					addr = (uint32_t)mem::detail::get_aligned_byte_offset(addr, comp.alig(), comp.size(), N);
				} else {
					GAIA_FOR(comp.soa()) {
						addr = (uint32_t)mem::detail::get_aligned_byte_offset(addr, comp.alig(), soaSizes[i], N);
					}
					// TODO: Magic offset. Otherwise, SoA data might leak past the chunk boundary when accessing
					//       the last element. By faking the memory offset we can bypass this is issue for now.
					//       Obviously, this needs fixing at some point.
					addr += comp.soa() * 4;
				}
				return addr;
			}

			template <typename T>
			GAIA_NODISCARD static ComponentCacheItem* create(Entity entity) {
				static_assert(core::is_raw_v<T>);

				auto* cci = mem::AllocHelper::alloc<ComponentCacheItem>("ComponentCacheItem");
				(void)new (cci) ComponentCacheItem();
				cci->entity = entity;
				cci->comp = Component(
						// component id
						detail::ComponentDesc<T>::id(),
						// soa
						detail::ComponentDesc<T>::soa(cci->soaSizes),
						// size in bytes
						detail::ComponentDesc<T>::size(),
						// alignment
						detail::ComponentDesc<T>::alig());
				cci->hashLookup = detail::ComponentDesc<T>::hash_lookup();

				auto ct_name = detail::ComponentDesc<T>::name();

				// Allocate enough memory for the name string + the null-terminating character (
				// the compile time string returned by ComponentDesc<T>::name is not null-terminated).
				// Different compilers will give a bit different strings, e.g.:
				//   Clang/GCC: gaia::ecs::uni<Position>
				//   MSVC     : gaia::ecs::uni<struct Position>
				// Therefore, we first copy the compile-time string and then tweak it so it is
				// the same on all supported compilers.
				char nameTmp[256];
				auto nameTmpLen = (uint32_t)ct_name.size();
				GAIA_ASSERT(nameTmpLen < 256);
				memcpy((void*)nameTmp, (const void*)ct_name.data(), nameTmpLen + 1);
				nameTmp[ct_name.size()] = 0;

				// Remove "class " or "struct " substrings from the string
				const uint32_t NSubstrings = 2;
				const char* to_remove[NSubstrings] = {"class ", "struct "};
				const uint32_t to_remove_len[NSubstrings] = {6, 7};
				GAIA_FOR(NSubstrings) {
					const auto& str = to_remove[i];
					const auto len = to_remove_len[i];

					auto* pos = nameTmp;
					while ((pos = strstr(pos, str)) != nullptr) {
						memmove(pos, pos + len, strlen(pos + len) + 1);
						nameTmpLen -= len;
					}
				}

				// Allocate the final string
				char* name = mem::AllocHelper::alloc<char>(nameTmpLen + 1);
				memcpy((void*)name, (const void*)nameTmp, nameTmpLen + 1);
				name[nameTmpLen] = 0;

				SymbolLookupKey tmp(name, nameTmpLen);
				cci->name = SymbolLookupKey(tmp.str(), tmp.len(), 1, {tmp.hash()});

				cci->func_ctor = detail::ComponentDesc<T>::func_ctor();
				cci->func_move_ctor = detail::ComponentDesc<T>::func_move_ctor();
				cci->func_copy_ctor = detail::ComponentDesc<T>::func_copy_ctor();
				cci->func_dtor = detail::ComponentDesc<T>::func_dtor();
				cci->func_copy = detail::ComponentDesc<T>::func_copy();
				cci->func_move = detail::ComponentDesc<T>::func_move();
				cci->func_swap = detail::ComponentDesc<T>::func_swap();
				cci->func_cmp = detail::ComponentDesc<T>::func_cmp();
				return cci;
			}

			static void destroy(ComponentCacheItem* pItem) {
				if (pItem == nullptr)
					return;

				if (pItem->name.str() != nullptr && pItem->name.owned()) {
					mem::AllocHelper::free((void*)pItem->name.str());
					pItem->name = {};
				}

				pItem->~ComponentCacheItem();
				mem::AllocHelper::free("ComponentCacheItem", pItem);
			}
		};
	} // namespace ecs
} // namespace gaia

/*** End of inlined file: component_cache_item.h ***/

namespace gaia {
	namespace ecs {
		class World;

		//! Cache for compile-time defined components
		class ComponentCache {
			friend class World;

			static constexpr uint32_t FastComponentCacheSize = 512;

			//! Fast-lookup cache for the first FastComponentCacheSize components
			cnt::darray<const ComponentCacheItem*> m_itemArr;
			//! Slower but more memory-friendly lookup cache for components with ids beyond FastComponentCacheSize
			cnt::map<detail::ComponentDescId, const ComponentCacheItem*> m_itemByDescId;

			//! Lookup of component items by their symbol name. Strings are owned by m_itemArr/m_itemByDescId
			cnt::map<ComponentCacheItem::SymbolLookupKey, const ComponentCacheItem*> m_compByString;
			//! Lookup of component items by their entity.
			cnt::map<EntityLookupKey, const ComponentCacheItem*> m_compByEntity;

			//! Clears the contents of the component cache
			//! \warning Should be used only after worlds are cleared because it invalidates all currently
			//!          existing component ids. Any cached content would stop working.
			//! \note Hidden from users because clearing the cache means that all existing component entities
			//!       would lose connection to it and effectively become dangling. This means that a new
			//!       component of type T could be added with a new entity id.
			void clear() {
				for (const auto* pItem: m_itemArr)
					ComponentCacheItem::destroy(const_cast<ComponentCacheItem*>(pItem));
				for (auto [componentId, pItem]: m_itemByDescId)
					ComponentCacheItem::destroy(const_cast<ComponentCacheItem*>(pItem));

				m_itemArr.clear();
				m_itemByDescId.clear();
				m_compByString.clear();
				m_compByEntity.clear();
			}

		public:
			ComponentCache() {
				// Reserve enough storage space for most use-cases
				m_itemArr.reserve(FastComponentCacheSize);
			}

			~ComponentCache() {
				clear();
			}

			ComponentCache(ComponentCache&&) = delete;
			ComponentCache(const ComponentCache&) = delete;
			ComponentCache& operator=(ComponentCache&&) = delete;
			ComponentCache& operator=(const ComponentCache&) = delete;

			//! Registers the component item for \tparam T. If it already exists it is returned.
			//! \return Component info
			template <typename T>
			GAIA_NODISCARD GAIA_FORCEINLINE const ComponentCacheItem& add(Entity entity) {
				static_assert(!is_pair<T>::value);
				GAIA_ASSERT(!entity.pair());

				const auto compDescId = detail::ComponentDesc<T>::id();

				// Fast path for small component ids - use the array storage
				if (compDescId < FastComponentCacheSize) {
					auto createDesc = [&]() -> const ComponentCacheItem& {
						const auto* pItem = ComponentCacheItem::create<T>(entity);
						GAIA_ASSERT(compDescId == pItem->comp.id());
						m_itemArr[compDescId] = pItem;
						m_compByString.emplace(pItem->name, pItem);
						m_compByEntity.emplace(pItem->entity, pItem);
						return *pItem;
					};

					if GAIA_UNLIKELY (compDescId >= m_itemArr.size()) {
						const auto oldSize = m_itemArr.size();
						const auto newSize = compDescId + 1U;

						// Increase the capacity by multiples of CapacityIncreaseSize
						constexpr uint32_t CapacityIncreaseSize = 128;
						const auto newCapacity = (newSize / CapacityIncreaseSize) * CapacityIncreaseSize + CapacityIncreaseSize;
						m_itemArr.reserve(newCapacity);

						// Update the size
						m_itemArr.resize(newSize);

						// Make sure unused memory is initialized to nullptr.
						GAIA_FOR2(oldSize, newSize - 1) m_itemArr[i] = nullptr;

						return createDesc();
					}

					if GAIA_UNLIKELY (m_itemArr[compDescId] == nullptr) {
						return createDesc();
					}

					return *m_itemArr[compDescId];
				}

				// Generic path for large component ids - use the map storage
				{
					auto createDesc = [&]() -> const ComponentCacheItem& {
						const auto* pItem = ComponentCacheItem::create<T>(entity);
						GAIA_ASSERT(compDescId == pItem->comp.id());
						m_itemByDescId.emplace(compDescId, pItem);
						m_compByString.emplace(pItem->name, pItem);
						m_compByEntity.emplace(pItem->entity, pItem);
						return *pItem;
					};

					const auto it = m_itemByDescId.find(compDescId);
					if (it == m_itemByDescId.end())
						return createDesc();

					return *it->second;
				}
			}

			//! Searches for the component cache item given the \param compDescId.
			//! \return Component info or nullptr it not found.
			GAIA_NODISCARD const ComponentCacheItem* find(detail::ComponentDescId compDescId) const noexcept {
				// Fast path - array storage
				if (compDescId < FastComponentCacheSize) {
					if (compDescId >= m_itemArr.size())
						return nullptr;

					return m_itemArr[compDescId];
				}

				// Generic path - map storage
				const auto it = m_itemByDescId.find(compDescId);
				return it != m_itemByDescId.end() ? it->second : nullptr;
			}

			//! Returns the component cache item given the \param compDescId.
			//! \return Component info
			//! \warning It is expected the component item with the given id exists! Undefined behavior otherwise.
			GAIA_NODISCARD const ComponentCacheItem& get(detail::ComponentDescId compDescId) const noexcept {
				// Fast path - array storage
				if (compDescId < FastComponentCacheSize) {
					GAIA_ASSERT(compDescId < m_itemArr.size());
					return *m_itemArr[compDescId];
				}

				// Generic path - map storage
				GAIA_ASSERT(m_itemByDescId.contains(compDescId));
				return *m_itemByDescId.find(compDescId)->second;
			}

			//! Searches for the component cache item.
			//! \param entity Entity associated with the component item.
			//! \param len String length. If zero, the length is calculated.
			//! \return Component cache item if found, nullptr otherwise.
			GAIA_NODISCARD const ComponentCacheItem* find(Entity entity) const noexcept {
				GAIA_ASSERT(!entity.pair());
				const auto it = m_compByEntity.find(EntityLookupKey(entity));
				if (it != m_compByEntity.end())
					return it->second;

				return nullptr;
			}

			//! Returns the component cache item.
			//! \param entity Entity associated with the component item.
			//! \return Component info.
			//! \warning It is expected the component item with the given name/length exists! Undefined behavior otherwise.
			GAIA_NODISCARD const ComponentCacheItem& get(Entity entity) const noexcept {
				GAIA_ASSERT(!entity.pair());
				const auto* pItem = find(entity);
				GAIA_ASSERT(pItem != nullptr);
				return *pItem;
			}

			//! Searches for the component cache item. The provided string is NOT copied internally.
			//! \param name A null-terminated string.
			//! \param len String length. If zero, the length is calculated.
			//! \return Component cache item if found, nullptr otherwise.
			GAIA_NODISCARD const ComponentCacheItem* find(const char* name, uint32_t len = 0) const noexcept {
				const auto it = m_compByString.find(
						len != 0 ? ComponentCacheItem::SymbolLookupKey(name, len) : ComponentCacheItem::SymbolLookupKey(name));
				if (it != m_compByString.end())
					return it->second;

				return nullptr;
			}

			//! Returns the component cache item. The provided string is NOT copied internally.
			//! \param name A null-terminated string
			//! \param len String length. If zero, the length is calculated
			//! \return Component info.
			//! \warning It is expected the component item with the given name/length exists! Undefined behavior otherwise.
			GAIA_NODISCARD const ComponentCacheItem& get(const char* name, uint32_t len = 0) const noexcept {
				const auto* pItem = find(name, len);
				GAIA_ASSERT(pItem != nullptr);
				return *pItem;
			}

			//! Searches for the component item for \tparam T.
			//! \warning It is expected the component already exists! Undefined behavior otherwise.
			//! \return Component info or nullptr if not found.
			template <typename T>
			GAIA_NODISCARD const ComponentCacheItem* find() const noexcept {
				static_assert(!is_pair<T>::value);
				const auto compDescId = detail::ComponentDesc<T>::id();
				return find(compDescId);
			}

			//! Returns the component item for \tparam T.
			//! \warning It is expected the component already exists! Undefined behavior otherwise.
			//! \return Component info
			template <typename T>
			GAIA_NODISCARD const ComponentCacheItem& get() const noexcept {
				static_assert(!is_pair<T>::value);
				const auto compDescId = detail::ComponentDesc<T>::id();
				return get(compDescId);
			}

			void diag() const {
				const auto registeredTypes = m_itemArr.size();
				GAIA_LOG_N("Registered components: %u", registeredTypes);

				auto logDesc = [](const ComponentCacheItem& item) {
					GAIA_LOG_N(
							"    hash:%016" PRIx64 ", size:%3u B, align:%3u B, [%u:%u] %s [%s]", item.hashLookup.hash,
							item.comp.size(), item.comp.alig(), item.entity.id(), item.entity.gen(), item.name.str(),
							EntityKindString[item.entity.kind()]);
				};
				for (const auto* pItem: m_itemArr) {
					if (pItem == nullptr)
						continue;
					logDesc(*pItem);
				}
				for (auto [componentId, pItem]: m_itemByDescId)
					logDesc(*pItem);
			}
		};
	} // namespace ecs
} // namespace gaia

/*** End of inlined file: component_cache.h ***/


/*** Start of inlined file: entity_container.h ***/
#pragma once

#include <cstdint>
#include <type_traits>

namespace gaia {
	namespace ecs {
		class Chunk;
		class Archetype;

		struct EntityContainerCtx {
			bool isEntity;
			bool isPair;
			EntityKind kind;
		};

		using EntityContainerFlagsType = uint16_t;
		enum EntityContainerFlags : EntityContainerFlagsType {
			OnDelete_Remove = 1 << 0,
			OnDelete_Delete = 1 << 1,
			OnDelete_Error = 1 << 2,
			OnDeleteTarget_Remove = 1 << 3,
			OnDeleteTarget_Delete = 1 << 4,
			OnDeleteTarget_Error = 1 << 5,
			HasAcyclic = 1 << 6,
			HasCantCombine = 1 << 7,
			IsExclusive = 1 << 8,
			HasAliasOf = 1 << 9,
			IsSingleton = 1 << 10,
			DeleteRequested = 1 << 11,
		};

		struct EntityContainer: cnt::ilist_item_base {
			//! Allocated items: Index in the list.
			//! Deleted items: Index of the next deleted item in the list.
			uint32_t idx;

			///////////////////////////////////////////////////////////////////
			// Bits in this section need to be 1:1 with Entity internal data
			///////////////////////////////////////////////////////////////////

			//! Generation ID of the record
			uint32_t gen : 27;
			//! 0-component, 1-entity
			uint32_t ent : 1;
			//! 0-ordinary, 1-pair
			uint32_t pair : 1;
			//! Component kind
			uint32_t kind : 1;
			//! Disabled
			//! Entity does not use this bit (always zero) so we steal it
			//! for special purposes.
			uint32_t dis : 1;

			///////////////////////////////////////////////////////////////////

			//! Row at which the entity is stored in the chunk
			uint16_t row;
			//! Flags
			uint16_t flags = 0;

			//! Archetype (stable address)
			Archetype* pArchetype;
			//! Chunk the entity currently resides in (stable address)
			Chunk* pChunk;

			// uint8_t depthDependsOn = 0;

			EntityContainer() = default;

			GAIA_NODISCARD static EntityContainer create(uint32_t index, uint32_t generation, void* pCtx) {
				auto* ctx = (EntityContainerCtx*)pCtx;

				EntityContainer ec{};
				ec.idx = index;
				ec.gen = generation;
				ec.ent = (uint32_t)ctx->isEntity;
				ec.pair = (uint32_t)ctx->isPair;
				ec.kind = (uint32_t)ctx->kind;
				return ec;
			}

			GAIA_NODISCARD static Entity handle(const EntityContainer& ec) {
				return Entity(ec.idx, ec.gen, (bool)ec.ent, (bool)ec.pair, (EntityKind)ec.kind);
			}

			void req_del() {
				flags |= DeleteRequested;
			}
		};

		struct EntityContainers {
			//! Implicit list of entities. Used for look-ups only when searching for
			//! entities in chunks + data validation. Entities only.
			cnt::ilist<EntityContainer, Entity> entities;
			//! Just like m_recs.entities, but stores pairs. Needs to be a map because
			//! pair ids are huge numbers.
			cnt::map<EntityLookupKey, EntityContainer> pairs;

			EntityContainer& operator[](Entity entity) {
				return entity.pair() //
									 ? pairs.find(EntityLookupKey(entity))->second
									 : entities[entity.id()];
			}

			const EntityContainer& operator[](Entity entity) const {
				return entity.pair() //
									 ? pairs.find(EntityLookupKey(entity))->second
									 : entities[entity.id()];
			}
		};
	} // namespace ecs
} // namespace gaia

/*** End of inlined file: entity_container.h ***/

namespace gaia {
	namespace ecs {
		class Chunk final {
		public:
			using EntityArray = cnt::sarray_ext<Entity, ChunkHeader::MAX_COMPONENTS>;
			using ComponentArray = cnt::sarray_ext<Component, ChunkHeader::MAX_COMPONENTS>;
			using ComponentOffsetArray = cnt::sarray_ext<ChunkDataOffset, ChunkHeader::MAX_COMPONENTS>;

			// TODO: Make this private
			//! Chunk header
			ChunkHeader m_header;
			//! Pointers to various parts of data inside chunk
			ChunkRecords m_records;

		private:
			//! Pointer to where the chunk data starts.
			//! Data layed out as following:
			//!			1) ComponentVersions
			//!     2) EntityIds/ComponentIds
			//!			3) ComponentRecords
			//!			4) Entities (identifiers)
			//!			5) Entities (data)
			//! Note, root archetypes store only entities, therefore it is fully occupied with entities.
			uint8_t m_data[1];

			GAIA_MSVC_WARNING_PUSH()
			GAIA_MSVC_WARNING_DISABLE(26495)

			// Hidden default constructor. Only use to calculate the relative offset of m_data
			Chunk() = default;

			Chunk(
					const ComponentCache& cc, uint32_t chunkIndex, uint16_t capacity, uint8_t genEntities, uint16_t st,
					uint32_t& worldVersion): //
					m_header(cc, chunkIndex, capacity, genEntities, st, worldVersion) {
				// Chunk data area consist of memory offsets, entities and component data. Normally. we would need
				// to in-place construct all of it manually.
				// However, the memory offsets and entities are all trivial types and components are initialized via
				// their constructors on-demand (if not trivial) so we do not really need to do any construction here.
			}

			GAIA_MSVC_WARNING_POP()

			GAIA_CLANG_WARNING_PUSH()
			// Memory is aligned so we can silence this warning
			GAIA_CLANG_WARNING_DISABLE("-Wcast-align")

			void init(
					uint32_t cntEntities, const Entity* ids, const Component* comps, const ChunkDataOffsets& headerOffsets,
					const ChunkDataOffset* compOffs) {
				m_header.cntEntities = (uint8_t)cntEntities;

				// Cache pointers to versions
				if (cntEntities > 0) {
					m_records.pVersions = (ComponentVersion*)&data(headerOffsets.firstByte_Versions);
				}

				// Cache entity ids
				if (cntEntities > 0) {
					auto* dst = m_records.pCompEntities = (Entity*)&data(headerOffsets.firstByte_CompEntities);

					// We treat the entity array as if were MAX_COMPONENTS long.
					// Real size can be smaller.
					uint32_t j = 0;
					for (; j < cntEntities; ++j)
						dst[j] = ids[j];
					for (; j < ChunkHeader::MAX_COMPONENTS; ++j)
						dst[j] = EntityBad;
				}

				// Cache component records
				if (cntEntities > 0) {
					auto* dst = m_records.pRecords = (ComponentRecord*)&data(headerOffsets.firstByte_Records);
					GAIA_FOR_(cntEntities, j) {
						dst[j].comp = comps[j];
						dst[j].pData = &data(compOffs[j]);
						dst[j].pItem = m_header.cc->find(comps[j].id());
					}
				}

				m_records.pEntities = (Entity*)&data(headerOffsets.firstByte_EntityData);

				// Now that records are set, we use the cached component descriptors to set ctor/dtor masks.
				{
					auto recs = comp_rec_view();
					GAIA_EACH(recs) {
						const auto& rec = recs[i];
						if (rec.comp.size() == 0)
							continue;

						const auto e = m_records.pCompEntities[i];
						if (e.kind() == EntityKind::EK_Gen) {
							m_header.hasAnyCustomGenCtor |= (rec.pItem->func_ctor != nullptr);
							m_header.hasAnyCustomGenDtor |= (rec.pItem->func_dtor != nullptr);
						} else {
							m_header.hasAnyCustomUniCtor |= (rec.pItem->func_ctor != nullptr);
							m_header.hasAnyCustomUniDtor |= (rec.pItem->func_dtor != nullptr);

							// We construct unique components right away if possible
							call_ctor(0, *rec.pItem);
						}
					}
				}
			}

			GAIA_MSVC_WARNING_POP()

			GAIA_NODISCARD std::span<const ComponentVersion> comp_version_view() const {
				return {(const ComponentVersion*)m_records.pVersions, m_header.cntEntities};
			}

			GAIA_NODISCARD std::span<ComponentVersion> comp_version_view_mut() {
				return {m_records.pVersions, m_header.cntEntities};
			}

			GAIA_NODISCARD std::span<Entity> entity_view_mut() {
				return {m_records.pEntities, m_header.count};
			}

			//! Returns a read-only span of the component data.
			//! \warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			//! \tparam T Component
			//! \return Span of read-only component data.
			template <typename T>
			GAIA_NODISCARD GAIA_FORCEINLINE auto view_inter(uint32_t from, uint32_t to) const //
					-> decltype(std::span<const uint8_t>{}) {

				if constexpr (std::is_same_v<core::raw_t<T>, Entity>) {
					GAIA_ASSERT(to <= m_header.count);
					return {(const uint8_t*)&m_records.pEntities[from], to - from};
				} else if constexpr (is_pair<T>::value) {
					using TT = typename T::type;
					using U = typename component_type_t<TT>::Type;
					static_assert(!std::is_empty_v<U>, "Attempting to get value of an empty component");

					constexpr auto kind = entity_kind_v<TT>;
					const auto rel = m_header.cc->get<typename T::rel>().entity;
					const auto tgt = m_header.cc->get<typename T::tgt>().entity;
					const auto compIdx = comp_idx((Entity)Pair(rel, tgt));

					if constexpr (mem::is_soa_layout_v<U>) {
						GAIA_ASSERT(from == 0);
						GAIA_ASSERT(to == capacity());
						return {comp_ptr(compIdx), to};
					} else if constexpr (kind == EntityKind::EK_Gen) {
						GAIA_ASSERT(to <= m_header.count);
						return {comp_ptr(compIdx, from), to - from};
					} else {
						GAIA_ASSERT(to <= m_header.count);
						// GAIA_ASSERT(count == 1); we don't really care and always consider 1 for unique components
						return {comp_ptr(compIdx), 1};
					}
				} else {
					using U = typename component_type_t<T>::Type;
					static_assert(!std::is_empty_v<U>, "Attempting to get value of an empty component");

					constexpr auto kind = entity_kind_v<T>;
					const auto comp = m_header.cc->get<T>().entity;
					GAIA_ASSERT(comp.kind() == kind);
					const auto compIdx = comp_idx(comp);

					if constexpr (mem::is_soa_layout_v<U>) {
						GAIA_ASSERT(from == 0);
						GAIA_ASSERT(to == capacity());
						return {comp_ptr(compIdx), to};
					} else if constexpr (kind == EntityKind::EK_Gen) {
						GAIA_ASSERT(to <= m_header.count);
						return {comp_ptr(compIdx, from), to - from};
					} else {
						GAIA_ASSERT(to <= m_header.count);
						// GAIA_ASSERT(count == 1); we don't really care and always consider 1 for unique components
						return {comp_ptr(compIdx), 1};
					}
				}
			}

			//! Returns a read-write span of the component data. Also updates the world version for the component.
			//! \warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			//! \tparam T Component
			//! \tparam WorldVersionUpdateWanted If true, the world version is updated as a result of the write access
			//! \return Span of read-write component data.
			template <typename T, bool WorldVersionUpdateWanted>
			GAIA_NODISCARD GAIA_FORCEINLINE auto view_mut_inter(uint32_t from, uint32_t to) //
					-> decltype(std::span<uint8_t>{}) {
				static_assert(!std::is_same_v<core::raw_t<T>, Entity>, "view_mut can't be used to modify Entity");

				if constexpr (is_pair<T>::value) {
					using TT = typename T::type;
					using U = typename component_type_t<TT>::Type;
					static_assert(!std::is_empty_v<U>, "view_mut can't be used to modify tag components");

					constexpr auto kind = entity_kind_v<TT>;
					const auto rel = m_header.cc->get<typename T::rel>().entity;
					const auto tgt = m_header.cc->get<typename T::tgt>().entity;
					const auto compIdx = comp_idx((Entity)Pair(rel, tgt));

					// Update version number if necessary so we know RW access was used on the chunk
					if constexpr (WorldVersionUpdateWanted)
						update_world_version(compIdx);

					if constexpr (mem::is_soa_layout_v<U>) {
						GAIA_ASSERT(from == 0);
						GAIA_ASSERT(to == capacity());
						return {comp_ptr_mut(compIdx), to};
					} else if constexpr (kind == EntityKind::EK_Gen) {
						GAIA_ASSERT(to <= m_header.count);
						return {comp_ptr_mut(compIdx, from), to - from};
					} else {
						GAIA_ASSERT(to <= m_header.count);
						// GAIA_ASSERT(count == 1); we don't really care and always consider 1 for unique components
						return {comp_ptr_mut(compIdx), 1};
					}
				} else {
					using U = typename component_type_t<T>::Type;
					static_assert(!std::is_empty_v<U>, "view_mut can't be used to modify tag components");

					constexpr auto kind = entity_kind_v<T>;
					const auto comp = m_header.cc->get<T>().entity;
					GAIA_ASSERT(comp.kind() == kind);
					const auto compIdx = comp_idx(comp);

					// Update version number if necessary so we know RW access was used on the chunk
					if constexpr (WorldVersionUpdateWanted)
						update_world_version(compIdx);

					if constexpr (mem::is_soa_layout_v<U>) {
						GAIA_ASSERT(from == 0);
						GAIA_ASSERT(to == capacity());
						return {comp_ptr_mut(compIdx), to};
					} else if constexpr (kind == EntityKind::EK_Gen) {
						GAIA_ASSERT(to <= m_header.count);
						return {comp_ptr_mut(compIdx, from), to - from};
					} else {
						GAIA_ASSERT(to <= m_header.count);
						// GAIA_ASSERT(count == 1); we don't really care and always consider 1 for unique components
						return {comp_ptr_mut(compIdx), 1};
					}
				}
			}

			//! Returns the value stored in the component \tparam T on \param row in the chunk.
			//! \warning It is expected the \param row is valid. Undefined behavior otherwise.
			//! \warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			//! \tparam T Component
			//! \param row Row of entity in the chunk
			//! \return Value stored in the component if smaller than 8 bytes. Const reference to the value otherwise.
			template <typename T>
			GAIA_NODISCARD decltype(auto) comp_inter(uint16_t row) const {
				using U = typename actual_type_t<T>::Type;
				using RetValueType = decltype(view<T>()[0]);

				GAIA_ASSERT(row < m_header.count);
				if constexpr (mem::is_soa_layout_v<U>)
					return view<T>(0, capacity())[row];
				else if constexpr (sizeof(RetValueType) <= 8)
					return view<T>()[row];
				else
					return (const U&)view<T>()[row];
			}

		public:
			Chunk(const Chunk& chunk) = delete;
			Chunk(Chunk&& chunk) = delete;
			Chunk& operator=(const Chunk& chunk) = delete;
			Chunk& operator=(Chunk&& chunk) = delete;
			~Chunk() = default;

			static constexpr uint16_t chunk_header_size() {
				const auto dataAreaOffset =
						// ChunkAllocator reserves the first few bytes for internal purposes
						MemoryBlockUsableOffset +
						// Chunk "header" area (before actual entity/component data starts)
						sizeof(ChunkHeader) + sizeof(ChunkRecords);
				static_assert(dataAreaOffset < UINT16_MAX);
				return dataAreaOffset;
			}

			static constexpr uint16_t chunk_total_bytes(uint16_t dataSize) {
				return chunk_header_size() + dataSize;
			}

			static constexpr uint16_t chunk_data_bytes(uint16_t totalSize) {
				return totalSize - chunk_header_size();
			}

			//! Returns the relative offset of m_data in Chunk
			static uintptr_t chunk_data_area_offset() {
				// Note, offsetof is implementation-defined and conditionally-supported since C++17.
				// Therefore, we instantiate the chunk and calculate the relative address ourselves.
				Chunk chunk;
				const auto chunk_offset = (uintptr_t)&chunk;
				const auto data_offset = (uintptr_t)&chunk.m_data[0];
				return data_offset - chunk_offset;
			}

			//! Allocates memory for a new chunk.
			//! \param chunkIndex Index of this chunk within the parent archetype
			//! \return Newly allocated chunk
			static Chunk* create(
					const ComponentCache& cc, uint32_t chunkIndex, uint16_t capacity, uint8_t cntEntities, uint8_t genEntities,
					uint16_t dataBytes, uint32_t& worldVersion,
					// data offsets
					const ChunkDataOffsets& offsets,
					// component entities
					const Entity* ids,
					// component
					const Component* comps,
					// component offsets
					const ChunkDataOffset* compOffs) {
				const auto totalBytes = chunk_total_bytes(dataBytes);
				const auto sizeType = mem_block_size_type(totalBytes);
#if GAIA_ECS_CHUNK_ALLOCATOR
				auto* pChunk = (Chunk*)ChunkAllocator::get().alloc(totalBytes);
				(void)new (pChunk) Chunk(cc, chunkIndex, capacity, genEntities, sizeType, worldVersion);
#else
				GAIA_ASSERT(totalBytes <= MaxMemoryBlockSize);
				const auto allocSize = mem_block_size(sizeType);
				auto* pChunkMem = mem::AllocHelper::alloc<uint8_t>(allocSize);
				std::memset(pChunkMem, 0, allocSize);
				auto* pChunk = new (pChunkMem) Chunk(cc, chunkIndex, capacity, genEntities, sizeType, worldVersion);
#endif

				pChunk->init((uint32_t)cntEntities, ids, comps, offsets, compOffs);
				return pChunk;
			}

			//! Releases all memory allocated by \param pChunk.
			//! \param pChunk Chunk which we want to destroy
			static void free(Chunk* pChunk) {
				GAIA_ASSERT(pChunk != nullptr);
				GAIA_ASSERT(!pChunk->dead());

				// Mark as dead
				pChunk->die();

				// Call destructors for components that need it
				pChunk->call_all_dtors();

				pChunk->~Chunk();
#if GAIA_ECS_CHUNK_ALLOCATOR
				ChunkAllocator::get().free(pChunk);
#else
				mem::AllocHelper::free((uint8_t*)pChunk);
#endif
			}

			//! Remove the last entity from a chunk.
			//! If as a result the chunk becomes empty it is scheduled for deletion.
			//! \param chunksToDelete Container of chunks ready for deletion
			void remove_last_entity() {
				// Should never be called over an empty chunk
				GAIA_ASSERT(!empty());

#if GAIA_ASSERT_ENABLED
				// Invalidate the entity in chunk data
				entity_view_mut()[m_header.count - 1] = EntityBad;
#endif

				--m_header.count;
				--m_header.countEnabled;
			}

			//! Updates the version numbers for this chunk.
			void update_versions() {
				update_version(m_header.worldVersion);
				update_world_version();
			}

			//! Returns a read-only entity or component view.
			//! \warning If \tparam T is a component it is expected it is present. Undefined behavior otherwise.
			//! \tparam T Component or Entity
			//! \param from First valid entity row
			//! \param to Last valid entity row
			//! \return Entity of component view with read-only access
			template <typename T>
			GAIA_NODISCARD decltype(auto) view(uint16_t from, uint16_t to) const {
				using U = typename actual_type_t<T>::Type;

				// Always consider full range for SoA
				if constexpr (mem::is_soa_layout_v<U>)
					return mem::auto_view_policy_get<U>{view_inter<T>(0, capacity())};
				else
					return mem::auto_view_policy_get<U>{view_inter<T>(from, to)};
			}

			template <typename T>
			GAIA_NODISCARD decltype(auto) view() const {
				return view<T>(0, m_header.count);
			}

			template <typename T>
			GAIA_NODISCARD decltype(auto) view_raw(const void* ptr, uint32_t size) const {
				using U = typename actual_type_t<T>::Type;
				return mem::auto_view_policy_get<U>{std::span{(const uint8_t*)ptr, size}};
			}

			//! Returns a mutable entity or component view.
			//! \warning If \tparam T is a component it is expected it is present. Undefined behavior otherwise.
			//! \tparam T Component or Entity
			//! \param from First valid entity row
			//! \param to Last valid entity row
			//! \return Entity or component view with read-write access
			template <typename T>
			GAIA_NODISCARD decltype(auto) view_mut(uint16_t from, uint16_t to) {
				using U = typename actual_type_t<T>::Type;
				static_assert(!std::is_same_v<U, Entity>, "Modifying chunk entities via view_mut is forbidden");

				// Always consider full range for SoA
				if constexpr (mem::is_soa_layout_v<U>)
					return mem::auto_view_policy_set<U>{view_mut_inter<T, true>(0, capacity())};
				else
					return mem::auto_view_policy_set<U>{view_mut_inter<T, true>(from, to)};
			}

			template <typename T>
			GAIA_NODISCARD decltype(auto) view_mut() {
				return view_mut<T>(0, m_header.count);
			}

			template <typename T>
			GAIA_NODISCARD decltype(auto) view_mut_raw(void* ptr, uint32_t size) const {
				using U = typename actual_type_t<T>::Type;
				static_assert(!std::is_same_v<U, Entity>, "Modifying chunk entities via view_mut is forbidden");

				return mem::auto_view_policy_set<U>{std::span{(uint8_t*)ptr, size}};
			}

			//! Returns a mutable component view.
			//! Doesn't update the world version when the access is acquired.
			//! \warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			//! \tparam T Component
			//! \param from First valid entity row
			//! \param to Last valid entity row
			//! \return Component view with read-write access
			template <typename T>
			GAIA_NODISCARD decltype(auto) sview_mut(uint16_t from, uint16_t to) {
				using U = typename actual_type_t<T>::Type;
				static_assert(!std::is_same_v<U, Entity>, "Modifying chunk entities via view_mut is forbidden");

				// Always consider full range for SoA
				if constexpr (mem::is_soa_layout_v<U>)
					return mem::auto_view_policy_set<U>{view_mut_inter<T, false>(0, capacity())};
				else
					return mem::auto_view_policy_set<U>{view_mut_inter<T, false>(from, to)};
			}

			template <typename T>
			GAIA_NODISCARD decltype(auto) sview_mut_raw(void* ptr, uint32_t size) const {
				using U = typename actual_type_t<T>::Type;
				static_assert(!std::is_same_v<U, Entity>, "Modifying chunk entities via view_mut is forbidden");

				return mem::auto_view_policy_set<U>{std::span{(uint8_t*)ptr, size}};
			}

			template <typename T>
			GAIA_NODISCARD decltype(auto) sview_mut() {
				return sview_mut<T>(0, m_header.count);
			}

			//! Returns either a mutable or immutable entity/component view based on the requested type.
			//! Value and const types are considered immutable. Anything else is mutable.
			//! \warning If \tparam T is a component it is expected to be present. Undefined behavior otherwise.
			//! \tparam T Component or Entity
			//! \param from First valid entity row
			//! \param to Last valid entity row
			//! \return Entity or component view
			template <typename T>
			GAIA_NODISCARD decltype(auto) view_auto(uint16_t from, uint16_t to) {
				using UOriginal = typename actual_type_t<T>::TypeOriginal;
				if constexpr (core::is_mut_v<UOriginal>)
					return view_mut<T>(from, to);
				else
					return view<T>(from, to);
			}

			template <typename T>
			GAIA_NODISCARD decltype(auto) view_auto() {
				return view_auto<T>(0, m_header.count);
			}

			//! Returns either a mutable or immutable entity/component view based on the requested type.
			//! Value and const types are considered immutable. Anything else is mutable.
			//! Doesn't update the world version when read-write access is acquired.
			//! \warning If \tparam T is a component it is expected to be present. Undefined behavior otherwise.
			//! \tparam T Component or Entity
			//! \param from First valid entity row
			//! \param to Last valid entity row
			//! \return Entity or component view
			template <typename T>
			GAIA_NODISCARD decltype(auto) sview_auto(uint16_t from, uint16_t to) {
				using UOriginal = typename actual_type_t<T>::TypeOriginal;
				if constexpr (core::is_mut_v<UOriginal>)
					return sview_mut<T>(from, to);
				else
					return view<T>(from, to);
			}

			template <typename T>
			GAIA_NODISCARD decltype(auto) sview_auto() {
				return sview_auto<T>(0, m_header.count);
			}

			GAIA_NODISCARD EntitySpan entity_view() const {
				return {(const Entity*)m_records.pEntities, m_header.count};
			}

			GAIA_NODISCARD EntitySpan ids_view() const {
				return {(const Entity*)m_records.pCompEntities, m_header.cntEntities};
			}

			GAIA_NODISCARD std::span<const ComponentRecord> comp_rec_view() const {
				return {m_records.pRecords, m_header.cntEntities};
			}

			GAIA_NODISCARD uint8_t* comp_ptr_mut(uint32_t compIdx) {
				const auto& rec = m_records.pRecords[compIdx];
				return rec.pData;
			}

			GAIA_NODISCARD uint8_t* comp_ptr_mut(uint32_t compIdx, uint32_t offset) {
				const auto& rec = m_records.pRecords[compIdx];
				return rec.pData + (uintptr_t)rec.comp.size() * offset;
			}

			GAIA_NODISCARD const uint8_t* comp_ptr(uint32_t compIdx) const {
				const auto& rec = m_records.pRecords[compIdx];
				return rec.pData;
			}

			GAIA_NODISCARD const uint8_t* comp_ptr(uint32_t compIdx, uint32_t offset) const {
				const auto& rec = m_records.pRecords[compIdx];
				return rec.pData + (uintptr_t)rec.comp.size() * offset;
			}

			//! Make \param entity a part of the chunk at the version of the world.
			//! \return Row of entity within the chunk.
			GAIA_NODISCARD uint16_t add_entity(Entity entity) {
				const auto row = m_header.count++;

				// Zero after increase of value means an overflow!
				GAIA_ASSERT(m_header.count != 0);

				++m_header.countEnabled;
				entity_view_mut()[row] = entity;

				update_version(m_header.worldVersion);
				update_world_version();

				return row;
			}

			//! Copies all data associated with \param srcEntity into \param dstEntity.
			static void copy_entity_data(Entity srcEntity, Entity dstEntity, EntityContainers& recs) {
				GAIA_PROF_SCOPE(Chunk::copy_entity_data);

				auto& srcEntityContainer = recs[srcEntity];
				auto* pSrcChunk = srcEntityContainer.pChunk;

				auto& dstEntityContainer = recs[dstEntity];
				auto* pDstChunk = dstEntityContainer.pChunk;

				GAIA_ASSERT(srcEntityContainer.pArchetype == dstEntityContainer.pArchetype);

				auto srcRecs = pSrcChunk->comp_rec_view();

				// Copy generic component data from reference entity to our new entity.
				// Unique components do not change place in the chunk so there is no need to move them.
				GAIA_FOR(pSrcChunk->m_header.genEntities) {
					const auto& rec = srcRecs[i];
					if (rec.comp.size() == 0U)
						continue;

					const auto* pSrc = (const void*)pSrcChunk->comp_ptr_mut(i);
					auto* pDst = (void*)pDstChunk->comp_ptr_mut(i);
					rec.pItem->copy(
							pDst, pSrc, dstEntityContainer.row, srcEntityContainer.row, pDstChunk->capacity(), pSrcChunk->capacity());
				}
			}

			//! Moves all data associated with \param entity into the chunk so that it is stored at the row \param row.
			void move_entity_data(Entity entity, uint16_t row, EntityContainers& recs) {
				GAIA_PROF_SCOPE(Chunk::move_entity_data);

				auto& ec = recs[entity];
				auto* pSrcChunk = ec.pChunk;
				auto srcRecs = pSrcChunk->comp_rec_view();

				// Copy generic component data from reference entity to our new entity.
				// Unique components do not change place in the chunk so there is no need to move them.
				GAIA_FOR(pSrcChunk->m_header.genEntities) {
					const auto& rec = srcRecs[i];
					if (rec.comp.size() == 0U)
						continue;

					auto* pSrc = (void*)pSrcChunk->comp_ptr_mut(i);
					auto* pDst = (void*)comp_ptr_mut(i);
					rec.pItem->ctor_move(pDst, pSrc, row, ec.row, capacity(), pSrcChunk->capacity());
				}
			}

			//! Copies all data associated with \param entity into the chunk so that it is stored at the row \param row.
			static void copy_foreign_entity_data(Chunk* pSrcChunk, uint32_t srcRow, Chunk* pDstChunk, uint32_t dstRow) {
				GAIA_PROF_SCOPE(Chunk::copy_foreign_entity_data);

				GAIA_ASSERT(pSrcChunk != nullptr);
				GAIA_ASSERT(pDstChunk != nullptr);
				GAIA_ASSERT(srcRow < pSrcChunk->size());
				GAIA_ASSERT(dstRow < pDstChunk->size());

				auto srcIds = pSrcChunk->ids_view();
				auto dstIds = pDstChunk->ids_view();
				auto dstRecs = pDstChunk->comp_rec_view();

				// Find intersection of the two component lists.
				// Arrays are sorted so we can do linear intersection lookup.
				// Call constructor on each match.
				// Unique components do not change place in the chunk so there is no need to move them.
				{
					uint32_t i = 0;
					uint32_t j = 0;
					while (i < pSrcChunk->m_header.genEntities && j < pDstChunk->m_header.genEntities) {
						const auto oldId = srcIds[i];
						const auto newId = dstIds[j];

						if (oldId == newId) {
							const auto& rec = dstRecs[j];
							if (rec.comp.size() != 0U) {
								auto* pSrc = (void*)pSrcChunk->comp_ptr_mut(i);
								auto* pDst = (void*)pDstChunk->comp_ptr_mut(j);
								rec.pItem->ctor_copy(pDst, pSrc, dstRow, srcRow, pDstChunk->capacity(), pSrcChunk->capacity());
							}

							++i;
							++j;
						} else if (SortComponentCond{}.operator()(oldId, newId)) {
							++i;
						} else {
							// No match with the old chunk. Construct the component
							const auto& rec = dstRecs[j];
							if (rec.pItem != nullptr && rec.pItem->func_ctor != nullptr) {
								auto* pDst = (void*)pDstChunk->comp_ptr_mut(j, dstRow);
								rec.pItem->func_ctor(pDst, 1);
							}

							++j;
						}
					}

					// Initialize the rest of the components if they are generic.
					for (; j < pDstChunk->m_header.genEntities; ++j) {
						const auto& rec = dstRecs[j];
						if (rec.pItem != nullptr && rec.pItem->func_ctor != nullptr) {
							auto* pDst = (void*)pDstChunk->comp_ptr_mut(j, dstRow);
							rec.pItem->func_ctor(pDst, 1);
						}
					}
				}
			}

			//! Moves all data associated with \param entity into the chunk so that it is stored at the row \param row.
			static void move_foreign_entity_data(Chunk* pSrcChunk, uint32_t srcRow, Chunk* pDstChunk, uint32_t dstRow) {
				GAIA_PROF_SCOPE(Chunk::move_foreign_entity_data);

				GAIA_ASSERT(pSrcChunk != nullptr);
				GAIA_ASSERT(pDstChunk != nullptr);
				GAIA_ASSERT(srcRow < pSrcChunk->size());
				GAIA_ASSERT(dstRow < pDstChunk->size());

				auto srcIds = pSrcChunk->ids_view();
				auto dstIds = pDstChunk->ids_view();
				auto dstRecs = pDstChunk->comp_rec_view();

				// Find intersection of the two component lists.
				// Arrays are sorted so we can do linear intersection lookup.
				// Call constructor on each match.
				// Unique components do not change place in the chunk so there is no need to move them.
				{
					uint32_t i = 0;
					uint32_t j = 0;
					while (i < pSrcChunk->m_header.genEntities && j < pDstChunk->m_header.genEntities) {
						const auto oldId = srcIds[i];
						const auto newId = dstIds[j];

						if (oldId == newId) {
							const auto& rec = dstRecs[j];
							if (rec.comp.size() != 0U) {
								auto* pSrc = (void*)pSrcChunk->comp_ptr_mut(i);
								auto* pDst = (void*)pDstChunk->comp_ptr_mut(j);
								rec.pItem->ctor_move(pDst, pSrc, dstRow, srcRow, pDstChunk->capacity(), pSrcChunk->capacity());
							}

							++i;
							++j;
						} else if (SortComponentCond{}.operator()(oldId, newId)) {
							++i;
						} else {
							// No match with the old chunk. Construct the component
							const auto& rec = dstRecs[j];
							if (rec.pItem != nullptr && rec.pItem->func_ctor != nullptr) {
								auto* pDst = (void*)pDstChunk->comp_ptr_mut(j, dstRow);
								rec.pItem->func_ctor(pDst, 1);
							}

							++j;
						}
					}

					// Initialize the rest of the components if they are generic.
					for (; j < pDstChunk->m_header.genEntities; ++j) {
						const auto& rec = dstRecs[j];
						if (rec.pItem != nullptr && rec.pItem->func_ctor != nullptr) {
							auto* pDst = (void*)pDstChunk->comp_ptr_mut(j, dstRow);
							rec.pItem->func_ctor(pDst, 1);
						}
					}
				}
			}

			//! Tries to remove the entity at \param row.
			//! Removal is done via swapping with last entity in chunk.
			//! Upon removal, all associated data is also removed.
			//! If the entity at the given row already is the last chunk entity, it is removed directly.
			void remove_entity_inter(uint16_t row, EntityContainers& recs) {
				GAIA_PROF_SCOPE(Chunk::remove_entity_inter);

				const uint16_t rowA = row;
				const uint16_t rowB = m_header.count - 1;
				// The "rowA" entity is the one we are going to destroy so it needs to precede the "rowB"
				GAIA_ASSERT(rowA <= rowB);

				// To move anything, we need at least 2 entities
				if GAIA_LIKELY (rowA < rowB) {
					GAIA_ASSERT(m_header.count > 1);

					auto ev = entity_view_mut();

					// Update entity data
					const auto entityB = ev[rowB];
					auto& ecB = recs[entityB];
#if GAIA_ASSERT_ENABLED
					const auto entityA = ev[rowA];
					auto& ecA = recs[entityA];

					GAIA_ASSERT(ecA.pArchetype == ecB.pArchetype);
					GAIA_ASSERT(ecA.pChunk == ecB.pChunk);
#endif

					ev[rowA] = entityB;

					// Move component data from entityB to entityA
					auto recView = comp_rec_view();
					GAIA_FOR(m_header.genEntities) {
						const auto& rec = recView[i];
						if (rec.comp.size() == 0U)
							continue;

						auto* pSrc = (void*)comp_ptr_mut(i);
						rec.pItem->move(pSrc, pSrc, rowA, rowB, capacity(), capacity());

						pSrc = (void*)comp_ptr_mut(i, rowB);
						rec.pItem->dtor(pSrc);
					}

					// Entity has been replaced with the last one in our chunk. Update its container record.
					ecB.row = rowA;
				} else if (m_header.hasAnyCustomGenDtor) {
					// This is the last entity in the chunk so simply destroy its data
					auto recView = comp_rec_view();
					GAIA_FOR(m_header.genEntities) {
						const auto& rec = recView[i];
						if (rec.comp.size() == 0U)
							continue;

						auto* pSrc = (void*)comp_ptr_mut(i, rowA);
						rec.pItem->dtor(pSrc);
					}
				}
			}

			//! Tries to remove the entity at row \param row.
			//! Removal is done via swapping with last entity in chunk.
			//! Upon removal, all associated data is also removed.
			//! If the entity at the given row already is the last chunk entity, it is removed directly.
			void remove_entity(uint16_t row, EntityContainers& recs) {
				GAIA_ASSERT(
						!locked() && "Entities can't be removed while their chunk is being iterated "
												 "(structural changes are forbidden during this time!)");

				if GAIA_UNLIKELY (m_header.count == 0)
					return;

				GAIA_PROF_SCOPE(Chunk::remove_entity);

				if (enabled(row)) {
					// Entity was previously enabled. Swap with the last entity
					remove_entity_inter(row, recs);
					// If this was the first enabled entity make sure to update the row
					if (m_header.rowFirstEnabledEntity > 0 && row == m_header.rowFirstEnabledEntity)
						--m_header.rowFirstEnabledEntity;
				} else {
					// Entity was previously disabled. Swap with the last disabled entity
					const uint16_t pivot = size_disabled() - 1;
					swap_chunk_entities(row, pivot, recs);
					// Once swapped, try to swap with the last (enabled) entity in the chunk.
					remove_entity_inter(pivot, recs);
					--m_header.rowFirstEnabledEntity;
				}

				// At this point the last entity is no longer valid so remove it
				remove_last_entity();
			}

			//! Tries to swap the entity at row \param rowA with the one at the row \param rowB.
			//! When swapping, all data associated with the two entities is swapped as well.
			//! If \param rowA equals \param rowB no swapping is performed.
			//! \warning "rowA" must he smaller or equal to "rowB"
			void swap_chunk_entities(uint16_t rowA, uint16_t rowB, EntityContainers& recs) {
				// The "rowA" entity is the one we are going to destroy so it needs to precede the "rowB".
				// Unlike remove_entity_inter, it is not technically necessary but we do it
				// anyway for the sake of consistency.
				GAIA_ASSERT(rowA <= rowB);

				// If there are at least two different entities inside to swap
				if GAIA_UNLIKELY (m_header.count <= 1 || rowA == rowB)
					return;

				GAIA_PROF_SCOPE(Chunk::swap_chunk_entities);

				// Update entity data
				auto ev = entity_view_mut();
				const auto entityA = ev[rowA];
				const auto entityB = ev[rowB];

				auto& ecA = recs[entityA];
				auto& ecB = recs[entityB];
				GAIA_ASSERT(ecA.pArchetype == ecB.pArchetype);
				GAIA_ASSERT(ecA.pChunk == ecB.pChunk);

				ev[rowA] = entityB;
				ev[rowB] = entityA;

				// Swap component data
				auto recView = comp_rec_view();
				GAIA_FOR2(0, m_header.genEntities) {
					const auto& rec = recView[i];
					if (rec.comp.size() == 0U)
						continue;

					GAIA_ASSERT(rec.pData == comp_ptr_mut(i));
					rec.pItem->swap(rec.pData, rec.pData, rowA, rowB, capacity(), capacity());
				}

				// Update indices in entity container.
				ecA.row = (uint16_t)rowB;
				ecB.row = (uint16_t)rowA;
			}

			//! Enables or disables the entity on a given row in the chunk.
			//! \param row Row of the entity within chunk
			//! \param enableEntity Enables or disabled the entity
			//! \param entities Span of entity container records
			void enable_entity(uint16_t row, bool enableEntity, EntityContainers& recs) {
				GAIA_ASSERT(
						!locked() && "Entities can't be enable while their chunk is being iterated "
												 "(structural changes are forbidden during this time!)");
				GAIA_ASSERT(row < m_header.count && "Entity chunk row out of bounds!");

				if (enableEntity) {
					// Nothing to enable if there are no disabled entities
					if (!m_header.has_disabled_entities())
						return;
					// Trying to enable an already enabled entity
					if (enabled(row))
						return;
					// Try swapping our entity with the last disabled one
					const auto entity = entity_view()[row];
					swap_chunk_entities(--m_header.rowFirstEnabledEntity, row, recs);
					recs[entity].dis = 0;
					++m_header.countEnabled;
				} else {
					// Nothing to disable if there are no enabled entities
					if (!m_header.has_enabled_entities())
						return;
					// Trying to disable an already disabled entity
					if (!enabled(row))
						return;
					// Try swapping our entity with the last one in our chunk
					const auto entity = entity_view()[row];
					swap_chunk_entities(m_header.rowFirstEnabledEntity++, row, recs);
					recs[entity].dis = 1;
					--m_header.countEnabled;
				}
			}

			//! Checks if the entity is enabled.
			//! \param row Row of the entity within chunk
			//! \return True if entity is enabled. False otherwise.
			bool enabled(uint16_t row) const {
				GAIA_ASSERT(m_header.count > 0);

				return row >= (uint16_t)m_header.rowFirstEnabledEntity;
			}

			//! Returns a mutable pointer to chunk data.
			//! \param offset Offset into chunk data
			//! \return Pointer to chunk data.
			uint8_t& data(uint32_t offset) {
				return m_data[offset];
			}

			//! Returns an immutable pointer to chunk data.
			//! \param offset Offset into chunk data
			//! \return Pointer to chunk data.
			const uint8_t& data(uint32_t offset) const {
				return m_data[offset];
			}

			//----------------------------------------------------------------------
			// Component handling
			//----------------------------------------------------------------------

			bool has_custom_gen_ctor() const {
				return m_header.hasAnyCustomGenCtor;
			}

			bool has_custom_uni_ctor() const {
				return m_header.hasAnyCustomUniCtor;
			}

			bool has_custom_gen_dtor() const {
				return m_header.hasAnyCustomGenDtor;
			}

			bool has_custom_uni_dtor() const {
				return m_header.hasAnyCustomUniDtor;
			}

			void call_ctor(uint32_t entIdx, const ComponentCacheItem& item) {
				if (item.func_ctor == nullptr)
					return;

				GAIA_PROF_SCOPE(Chunk::call_ctor);

				const auto compIdx = comp_idx(item.entity);
				auto* pSrc = (void*)comp_ptr_mut(compIdx, entIdx);
				item.func_ctor(pSrc, 1);
			}

			void call_gen_ctors(uint32_t entIdx, uint32_t entCnt) {
				if (!m_header.hasAnyCustomGenCtor)
					return;

				GAIA_PROF_SCOPE(Chunk::call_gen_ctors);

				auto recs = comp_rec_view();
				GAIA_FOR2(0, m_header.genEntities) {
					const auto& rec = recs[i];

					const auto* pItem = rec.pItem;
					if (pItem == nullptr || pItem->func_ctor == nullptr)
						continue;

					auto* pSrc = (void*)comp_ptr_mut(i, entIdx);
					pItem->func_ctor(pSrc, entCnt);
				}
			}

			void call_all_dtors() {
				if (!m_header.hasAnyCustomGenDtor && !m_header.hasAnyCustomUniCtor)
					return;

				GAIA_PROF_SCOPE(Chunk::call_all_dtors);

				auto ids = ids_view();
				auto recs = comp_rec_view();
				GAIA_EACH(recs) {
					const auto& rec = recs[i];

					const auto* pItem = rec.pItem;
					if (pItem == nullptr || pItem->func_dtor == nullptr)
						continue;

					auto* pSrc = (void*)comp_ptr_mut(i, 0);
					const auto e = ids[i];
					const auto cnt = (e.kind() == EntityKind::EK_Gen) ? m_header.count : 1;
					pItem->func_dtor(pSrc, cnt);
				}
			};

			//----------------------------------------------------------------------
			// Check component presence
			//----------------------------------------------------------------------

			//! Checks if a component/entity \param entity is present in the chunk.
			//! \param entity Entity
			//! \return True if found. False otherwise.
			GAIA_NODISCARD bool has(Entity entity) const {
				auto ids = ids_view();
				return core::has(ids, entity);
			}

			//! Checks if component \tparam T is present in the chunk.
			//! \tparam T Component or pair
			//! \return True if the component is present. False otherwise.
			template <typename T>
			GAIA_NODISCARD bool has() const {
				if constexpr (is_pair<T>::value) {
					const auto rel = m_header.cc->get<typename T::rel>().entity;
					const auto tgt = m_header.cc->get<typename T::tgt>().entity;
					return has((Entity)Pair(rel, tgt));
				} else {
					const auto* pComp = m_header.cc->find<T>();
					return pComp != nullptr && has(pComp->entity);
				}
			}

			//----------------------------------------------------------------------
			// Set component data
			//----------------------------------------------------------------------

			//! Sets the value of the unique component \tparam T on \param row in the chunk.
			//! \tparam T Component or pair
			//! \param row Row of entity in the chunk
			//! \param value Value to set for the component
			//! \warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			template <typename T>
			decltype(auto) set(uint16_t row) {
				verify_comp<T>();

				GAIA_ASSERT2(
						actual_type_t<T>::Kind == EntityKind::EK_Gen || row == 0,
						"Set providing a row can only be used with generic components");

				// Update the world version
				update_version(m_header.worldVersion);

				GAIA_ASSERT(row < m_header.capacity);
				return view_mut<T>()[row];
			}

			//! Sets the value of a generic entity \param type at the position \param row in the chunk.
			//! \param row Row of entity in the chunk
			//! \param type Component/entity/pair
			//! \param value New component value
			//! \warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			template <typename T>
			decltype(auto) set(uint16_t row, Entity type) {
				verify_comp<T>();

				GAIA_ASSERT2(
						type.kind() == EntityKind::EK_Gen || row == 0,
						"Set providing a row can only be used with generic components");
				GAIA_ASSERT(type.kind() == entity_kind_v<T>);

				// Update the world version
				update_version(m_header.worldVersion);

				GAIA_ASSERT(row < m_header.capacity);

				// TODO: This function works but is useless because it does the same job as
				//       set(uint16_t row, U&& value).
				//       This is because T needs to match U anyway for the component lookup to succeed.
				(void)type;
				// const uint32_t col = comp_idx(type);
				//(void)col;

				return view_mut<T>()[row];
			}

			//! Sets the value of the unique component \tparam T on \param row in the chunk.
			//! \tparam T Component or pair
			//! \param row Row of entity in the chunk
			//! \param value Value to set for the component
			//! \warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			//! \warning World version is not updated so Query filters will not be able to catch this change.
			template <typename T>
			decltype(auto) sset(uint16_t row) {
				GAIA_ASSERT2(
						actual_type_t<T>::Kind == EntityKind::EK_Gen || row == 0,
						"Set providing a row can only be used with generic components");

				GAIA_ASSERT(row < m_header.capacity);
				return view_mut<T>()[row];
			}

			//! Sets the value of a generic entity \param type at the position \param row in the chunk.
			//! \param row Row of entity in the chunk
			//! \param type Component/entity/pair
			//! \param value New component value
			//! \warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			//! \warning World version is not updated so Query filters will not be able to catch this change.
			template <typename T>
			decltype(auto) sset(uint16_t row, Entity type) {
				static_assert(core::is_raw_v<T>);

				GAIA_ASSERT2(
						type.kind() == EntityKind::EK_Gen || row == 0,
						"Set providing a row can only be used with generic components");

				GAIA_ASSERT(row < m_header.capacity);

				// TODO: This function works but is useless because it does the same job as
				//       sset(uint16_t row, U&& value).
				//       This is because T needs to match U anyway for the component lookup to succeed.
				(void)type;
				// const uint32_t col = comp_idx(type);
				//(void)col;

				return view_mut<T>()[row];
			}

			//----------------------------------------------------------------------
			// Read component data
			//----------------------------------------------------------------------

			//! Returns the value stored in the generic component \tparam T on \param row in the chunk.
			//! \warning It is expected the \param row is valid. Undefined behavior otherwise.
			//! \warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			//! \tparam T Component or pair
			//! \param row Row of entity in the chunk
			//! \return Value stored in the component.
			template <typename T>
			GAIA_NODISCARD decltype(auto) get(uint16_t row) const {
				static_assert(
						entity_kind_v<T> == EntityKind::EK_Gen, "Get providing a row can only be used with generic components");

				return comp_inter<T>(row);
			}

			//! Returns the value stored in the unique component \tparam T.
			//! \warning It is expected the unique component \tparam T is present. Undefined behavior otherwise.
			//! \tparam T Component or pair
			//! \return Value stored in the component.
			template <typename T>
			GAIA_NODISCARD decltype(auto) get() const {
				static_assert(
						entity_kind_v<T> != EntityKind::EK_Gen,
						"Get not providing a row can only be used with non-generic components");

				return comp_inter<T>(0);
			}

			//! Returns the internal index of a component based on the provided \param entity.
			//! \param entity Component
			//! \return Component index if the component was found. -1 otherwise.
			GAIA_NODISCARD uint32_t comp_idx(Entity entity) const {
				return ecs::comp_idx<ChunkHeader::MAX_COMPONENTS>(m_records.pCompEntities, entity);
			}

			//----------------------------------------------------------------------

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

			//! Marks the chunk as dead (ready to delete)
			void die() {
				m_header.dead = 1;
			}

			//! Checks is this chunk is dead (ready to delete)
			GAIA_NODISCARD bool dead() const {
				return m_header.dead == 1;
			}

			//! Starts the process of dying (not yet ready to delete, can be revived)
			void start_dying() {
				GAIA_ASSERT(!dead());
				m_header.lifespanCountdown = ChunkHeader::MAX_CHUNK_LIFESPAN;
			}

			//! Makes a dying chunk alive again
			void revive() {
				GAIA_ASSERT(!dead());
				m_header.lifespanCountdown = 0;
			}

			//! Updates internal lifespan
			//! \return True if there is some lifespan rowA, false otherwise.
			bool progress_death() {
				GAIA_ASSERT(dying());
				--m_header.lifespanCountdown;
				return dying();
			}

			//! If true locks the chunk for structural changed.
			//! While locked, no new entities or component can be added or removed.
			//! While locked, no entities can be enabled or disabled.
			void lock(bool value) {
				if (value) {
					GAIA_ASSERT(m_header.structuralChangesLocked < ChunkHeader::MAX_CHUNK_LOCKS);
					++m_header.structuralChangesLocked;
				} else {
					GAIA_ASSERT(m_header.structuralChangesLocked > 0);
					--m_header.structuralChangesLocked;
				}
			}

			//! Checks if the chunk is locked for structural changes.
			bool locked() const {
				return m_header.structuralChangesLocked != 0;
			}

			//! Checks is the full capacity of the has has been reached
			GAIA_NODISCARD bool full() const {
				return m_header.count >= m_header.capacity;
			}

			//! Checks is the chunk is semi-full.
			GAIA_NODISCARD bool is_semi() const {
				// We want the chunk filled to at least 75% before considering it semi-full
				constexpr float Threshold = 0.75f;
				return ((float)m_header.count / (float)m_header.capacity) < Threshold;
			}

			//! Returns the total number of entities in the chunk (both enabled and disabled)
			GAIA_NODISCARD uint16_t size() const {
				return m_header.count;
			}

			//! Checks is there are any entities in the chunk
			GAIA_NODISCARD bool empty() const {
				return m_header.count == 0;
			}

			//! Return the number of entities in the chunk which are enabled
			GAIA_NODISCARD uint16_t size_enabled() const {
				return m_header.countEnabled;
			}

			//! Return the number of entities in the chunk which are enabled
			GAIA_NODISCARD uint16_t size_disabled() const {
				return (uint16_t)m_header.rowFirstEnabledEntity;
			}

			//! Returns the number of entities in the chunk
			GAIA_NODISCARD uint16_t capacity() const {
				return m_header.capacity;
			}

			//! Returns the number of bytes the chunk spans over
			GAIA_NODISCARD uint32_t bytes() const {
				return mem_block_size(m_header.sizeType);
			}

			//! Returns true if the provided version is newer than the one stored internally
			GAIA_NODISCARD bool changed(uint32_t version, uint32_t compIdx) const {
				auto versions = comp_version_view();
				return version_changed(versions[compIdx], version);
			}

			//! Update the version of a component at the index \param compIdx
			GAIA_FORCEINLINE void update_world_version(uint32_t compIdx) {
				auto versions = comp_version_view_mut();
				versions[compIdx] = m_header.worldVersion;
			}

			//! Update the version of all components
			GAIA_FORCEINLINE void update_world_version() {
				auto versions = comp_version_view_mut();
				for (auto& v: versions)
					v = m_header.worldVersion;
			}

			void diag() const {
				GAIA_LOG_N(
						"  Chunk #%04u, entities:%u/%u, lifespanCountdown:%u", m_header.index, m_header.count, m_header.capacity,
						m_header.lifespanCountdown);
			}
		};
	} // namespace ecs
} // namespace gaia

/*** End of inlined file: chunk.h ***/

namespace gaia {
	namespace ecs {
		class World;
		class Archetype;
		struct EntityContainer;

		const ComponentCache& comp_cache(const World& world);
		Entity entity_from_id(const World& world, EntityId id);
		const char* entity_name(const World& world, Entity entity);
		const char* entity_name(const World& world, EntityId entityId);

		namespace detail {
			GAIA_NODISCARD inline bool cmp_comps(EntitySpan comps, EntitySpan compsOther) {
				// Size has to match
				if (comps.size() != compsOther.size())
					return false;

				// Elements have to match
				GAIA_EACH(comps) {
					if (comps[i] != compsOther[i])
						return false;
				}

				return true;
			}
		} // namespace detail

		struct ArchetypeChunkPair {
			Archetype* pArchetype;
			Chunk* pChunk;

			GAIA_NODISCARD bool operator==(const ArchetypeChunkPair& other) const {
				return pArchetype == other.pArchetype && pChunk == other.pChunk;
			}
		};

		class ArchetypeBase {
		protected:
			//! Archetype ID - used to address the archetype directly in the world's list or archetypes
			ArchetypeId m_archetypeId = ArchetypeIdBad;

		public:
			GAIA_NODISCARD ArchetypeId id() const {
				return m_archetypeId;
			}
		};

		class ArchetypeLookupChecker: public ArchetypeBase {
			friend class Archetype;

			//! Span of component indices
			EntitySpan m_comps;

		public:
			ArchetypeLookupChecker(EntitySpan comps): m_comps(comps) {}

			GAIA_NODISCARD bool cmp_comps(const ArchetypeLookupChecker& other) const {
				return detail::cmp_comps(m_comps, other.m_comps);
			}
		};

		class Archetype final: public ArchetypeBase {
		public:
			using LookupHash = core::direct_hash_key<uint64_t>;

			//! Number of bits representing archetype lifespan
			static constexpr uint16_t ARCHETYPE_LIFESPAN_BITS = 7;
			//! Archetype lifespan must be at least as long as chunk lifespan
			static_assert(ARCHETYPE_LIFESPAN_BITS >= ChunkHeader::CHUNK_LIFESPAN_BITS);
			//! Number of ticks before empty chunks are removed
			static constexpr uint16_t MAX_ARCHETYPE_LIFESPAN = (1 << ARCHETYPE_LIFESPAN_BITS) - 1;

			struct Properties {
				//! The number of data entities this archetype can take (e.g 5 = 5 entities with all their components)
				uint16_t capacity;
				//! How many bytes of data is needed for a fully utilized chunk
				ChunkDataOffset chunkDataBytes;
				//! The number of generic entities/components
				uint8_t genEntities;
				//! Total number of entities/components
				uint8_t cntEntities;
			};

		private:
			using AsPairsIndexBuffer = cnt::sarr<uint8_t, ChunkHeader::MAX_COMPONENTS>;

			ArchetypeIdLookupKey::LookupHash m_archetypeIdHash;
			//! Hash of components within this archetype - used for lookups
			LookupHash m_hashLookup = {0};

			Properties m_properties{};
			//! Component cache reference
			const ComponentCache& m_cc;
			//! Stable reference to parent world's world version
			uint32_t& m_worldVersion;

			//! Index of the first chunk with enough space to add at least one entity
			uint32_t m_firstFreeChunkIdx = 0;
			//! Array of chunks allocated by this archetype
			cnt::darray<Chunk*> m_chunks;
			//! Mask of chunks with disabled entities
			// cnt::dbitset m_disabledMask;
			//! Graph of archetypes linked with this one
			ArchetypeGraph m_graph;

			//! Offsets to various parts of data inside chunk
			ChunkDataOffsets m_dataOffsets;
			//! Array of entities used to identify the archetype
			Entity m_ids[ChunkHeader::MAX_COMPONENTS];
			//! Array of indices to Is relationship pairs in m_ids
			uint8_t m_pairs_as_index_buffer[ChunkHeader::MAX_COMPONENTS];
			//! Array of component ids
			Component m_comps[ChunkHeader::MAX_COMPONENTS];
			//! Array of components offset indices
			ChunkDataOffset m_compOffs[ChunkHeader::MAX_COMPONENTS];

			//! Archetype list index
			uint32_t m_listIdx;

			//! Delete requested
			uint32_t m_deleteReq : 1;
			//! If set the archetype is to be deleted
			uint32_t m_dead : 1;
			//! Max lifespan of the archetype
			uint32_t m_lifespanCountdownMax: ARCHETYPE_LIFESPAN_BITS;
			//! Remaining lifespan of the archetype
			uint32_t m_lifespanCountdown: ARCHETYPE_LIFESPAN_BITS;
			//! Number of relationship pairs on the archetype
			uint32_t m_pairCnt: ChunkHeader::MAX_COMPONENTS_BITS;
			//! Number of Is relationship pairs on the archetype
			uint32_t m_pairCnt_is: ChunkHeader::MAX_COMPONENTS_BITS;
			//! Unused bits
			// uint32_t m_unused : 6;

			//! Constructor is hidden. Create archetypes via Archetype::Create
			Archetype(const ComponentCache& cc, uint32_t& worldVersion):
					m_cc(cc), m_worldVersion(worldVersion), m_listIdx(BadIndex), //
					m_deleteReq(0), m_dead(0), //
					m_lifespanCountdownMax(1), m_lifespanCountdown(0), //
					m_pairCnt(0), m_pairCnt_is(0) {}

			~Archetype() {
				// Delete all archetype chunks
				for (auto* pChunk: m_chunks)
					Chunk::free(pChunk);
			}

			//! Calculates offsets in memory at which important chunk data is going to be stored.
			//! These offsets are use to setup the chunk data area layout.
			//! \param memoryAddress Memory address used to calculate offsets
			void update_data_offsets(uintptr_t memoryAddress) {
				uintptr_t offset = 0;

				// Versions
				// We expect versions to fit in the first 256 bytes.
				// With 32 components per archetype this gives us some headroom.
				{
					offset += mem::padding<alignof(ComponentVersion)>(memoryAddress);

					const auto cnt = comps_view().size();
					if (cnt != 0) {
						GAIA_ASSERT(offset < 256);
						m_dataOffsets.firstByte_Versions = (ChunkDataVersionOffset)offset;
						offset += sizeof(ComponentVersion) * cnt;
					}
				}

				// Entity ids
				{
					offset += mem::padding<alignof(Entity)>(offset);

					const auto cnt = comps_view().size();
					if (cnt != 0) {
						m_dataOffsets.firstByte_CompEntities = (ChunkDataOffset)offset;

						// Storage-wise, treat the component array as it it were MAX_COMPONENTS long.
						offset += sizeof(Entity) * ChunkHeader::MAX_COMPONENTS;
					}
				}

				// Component records
				{
					offset += mem::padding<alignof(ComponentRecord)>(offset);

					const auto cnt = comps_view().size();
					if (cnt != 0) {

						m_dataOffsets.firstByte_Records = (ChunkDataOffset)offset;

						// Storage-wise, treat the component array as it it were MAX_COMPONENTS long.
						offset += sizeof(ComponentRecord) * cnt;
					}
				}

				// First entity offset
				{
					offset += mem::padding<alignof(Entity)>(offset);
					m_dataOffsets.firstByte_EntityData = (ChunkDataOffset)offset;
				}
			}

			//! Estimates how many entities can fit into the chunk described by \param comps components.
			static bool est_max_entities_per_archetype(
					const ComponentCache& cc, uint32_t& offs, uint32_t& maxItems, ComponentSpan comps, uint32_t size,
					uint32_t maxDataOffset) {
				for (const auto comp: comps) {
					if (comp.alig() == 0)
						continue;

					const auto& desc = cc.get(comp.id());

					// If we're beyond what the chunk could take, subtract one entity
					const auto nextOffset = desc.calc_new_mem_offset(offs, size);
					if (nextOffset >= maxDataOffset) {
						const auto subtractItems = (nextOffset - maxDataOffset + comp.size()) / comp.size();
						GAIA_ASSERT(subtractItems > 0);
						GAIA_ASSERT(maxItems > subtractItems);
						maxItems -= subtractItems;
						return false;
					}

					offs = nextOffset;
				}

				return true;
			};

			static void reg_components(
					Archetype& arch, EntitySpan ids, ComponentSpan comps, uint8_t from, uint8_t to, uint32_t& currOff,
					uint32_t count) {
				auto& ofs = arch.m_compOffs;

				// Set component ids
				GAIA_FOR2(from, to) arch.m_ids[i] = ids[i];

				// Calculate offsets and assign them indices according to our mappings
				GAIA_FOR2(from, to) {
					const auto comp = comps[i];
					const auto compIdx = i;

					const auto alig = comp.alig();
					if (alig == 0) {
						ofs[compIdx] = {};
					} else {
						currOff = mem::align(currOff, alig);
						ofs[compIdx] = (ChunkDataOffset)currOff;

						// Make sure the following component list is properly aligned
						currOff += comp.size() * count;
					}
				}
			}

		public:
			Archetype(Archetype&&) = delete;
			Archetype(const Archetype&) = delete;
			Archetype& operator=(Archetype&&) = delete;
			Archetype& operator=(const Archetype&) = delete;

			void list_idx(uint32_t idx) {
				m_listIdx = idx;
			}

			uint32_t list_idx() const {
				return m_listIdx;
			}

			GAIA_NODISCARD bool cmp_comps(const ArchetypeLookupChecker& other) const {
				return detail::cmp_comps(ids_view(), other.m_comps);
			}

			GAIA_NODISCARD static Archetype*
			create(const World& world, ArchetypeId archetypeId, uint32_t& worldVersion, EntitySpan ids) {
				const auto& cc = comp_cache(world);

				auto* newArch = mem::AllocHelper::alloc<Archetype>("Archetype");
				(void)new (newArch) Archetype(cc, worldVersion);

				newArch->m_archetypeId = archetypeId;
				newArch->m_archetypeIdHash = ArchetypeIdLookupKey::calc(archetypeId);
				const uint32_t maxEntities = archetypeId == 0 ? ChunkHeader::MAX_CHUNK_ENTITIES : 512;

				const auto cnt = (uint32_t)ids.size();
				newArch->m_properties.cntEntities = (uint8_t)ids.size();

				auto as_comp = [&](Entity entity) {
					const auto* pDesc = cc.find(entity);
					return pDesc == nullptr //
										 ? Component(IdentifierIdBad, 0, 0, 0) //
										 : pDesc->comp;
				};

				// Prepare m_comps array
				auto comps = std::span(&newArch->m_comps[0], cnt);
				GAIA_EACH(ids) {
					if (ids[i].pair()) {
						// When using pairs we need to decode the storage type from them.
						// This is what pair<Rel, Tgt>::type actually does to determine what type to use at compile-time.
						Entity pairEntities[] = {entity_from_id(world, ids[i].id()), entity_from_id(world, ids[i].gen())};
						Component pairComponents[] = {as_comp(pairEntities[0]), as_comp(pairEntities[1])};
						const uint32_t idx = (pairComponents[0].size() != 0U || pairComponents[1].size() == 0U) ? 0 : 1;
						comps[i] = pairComponents[idx];
					} else {
						comps[i] = as_comp(ids[i]);
					}
				}

				// Calculate offsets
				static auto ChunkDataAreaOffset = Chunk::chunk_data_area_offset();
				newArch->update_data_offsets(
						// This is not a real memory address.
						// Chunk memory is organized as header+data. The offsets we calculate here belong to
						// the data area.
						// Every allocated chunk is going to have the same relative offset from the header part
						// which is why providing a fictional relative offset is enough.
						ChunkDataAreaOffset);
				const auto& offs = newArch->m_dataOffsets;

				// Calculate the number of pairs
				GAIA_EACH(ids) {
					if (!ids[i].pair())
						continue;

					++newArch->m_pairCnt;

					// If it is an Is relationship, count it separately
					if (ids[i].id() == Is.id())
						newArch->m_pairs_as_index_buffer[newArch->m_pairCnt_is++] = (uint8_t)i;
				}

				// Find the index of the last generic component in both arrays
				uint32_t entsGeneric = (uint32_t)ids.size();
				if (!ids.empty()) {
					for (int i = (int)ids.size() - 1; i >= 0; --i) {
						if (ids[(uint32_t)i].kind() != EntityKind::EK_Uni)
							break;
						--entsGeneric;
					}
				}

				// Calculate the number of entities per chunks precisely so we can
				// fit as many of them into chunk as possible.

				uint32_t genCompsSize = 0;
				uint32_t uniCompsSize = 0;
				GAIA_FOR(entsGeneric) genCompsSize += newArch->m_comps[i].size();
				GAIA_FOR2(entsGeneric, cnt) uniCompsSize += newArch->m_comps[i].size();

				const uint32_t size0 = Chunk::chunk_data_bytes(mem_block_size(0));
				const uint32_t size1 = Chunk::chunk_data_bytes(mem_block_size(1));
				const auto sizeM = (size0 + size1) / 2;

				uint32_t maxDataOffsetTarget = size1;
				// Theoretical maximum number of components we can fit into one chunk.
				// This can be further reduced due alignment and padding.
				auto maxGenItemsInArchetype = (maxDataOffsetTarget - offs.firstByte_EntityData - uniCompsSize - 1) /
																			(genCompsSize + (uint32_t)sizeof(Entity));

				bool finalCheck = false;
			recalculate:
				auto currOff = offs.firstByte_EntityData + (uint32_t)sizeof(Entity) * maxGenItemsInArchetype;

				// Adjust the maximum number of entities. Recalculation happens at most once when the original guess
				// for entity count is not right (most likely because of padding or usage of SoA components).
				if (!est_max_entities_per_archetype(
								cc, currOff, maxGenItemsInArchetype, comps.subspan(0, entsGeneric), maxGenItemsInArchetype,
								maxDataOffsetTarget))
					goto recalculate;
				if (!est_max_entities_per_archetype(
								cc, currOff, maxGenItemsInArchetype, comps.subspan(entsGeneric), 1, maxDataOffsetTarget))
					goto recalculate;

				// Limit the number of entities to a certain number so we can make use of smaller
				// chunks where it makes sense.
				// TODO: Tweak this so the full remaining capacity is used. So if we occupy 7000 B we still
				//       have 1000 B left to fill.
				if (maxGenItemsInArchetype > maxEntities) {
					maxGenItemsInArchetype = maxEntities;
					goto recalculate;
				}

				// We create chunks of either 8K or 16K but might end up with requested capacity 8.1K. Allocating a 16K chunk
				// in this case would be wasteful. Therefore, let's find the middle ground. Anything 12K or smaller we'll
				// allocate into 8K chunks so we avoid wasting too much memory.
				if (!finalCheck && currOff < sizeM) {
					finalCheck = true;
					maxDataOffsetTarget = size0;

					maxGenItemsInArchetype = (maxDataOffsetTarget - offs.firstByte_EntityData - uniCompsSize - 1) /
																	 (genCompsSize + (uint32_t)sizeof(Entity));
					goto recalculate;
				}

				// Update the offsets according to the recalculated maxGenItemsInArchetype
				currOff = offs.firstByte_EntityData + (uint32_t)sizeof(Entity) * maxGenItemsInArchetype;
				reg_components(*newArch, ids, comps, (uint8_t)0, (uint8_t)entsGeneric, currOff, maxGenItemsInArchetype);
				reg_components(*newArch, ids, comps, (uint8_t)entsGeneric, (uint8_t)ids.size(), currOff, 1);

				GAIA_ASSERT(Chunk::chunk_total_bytes((ChunkDataOffset)currOff) < mem_block_size(currOff));
				newArch->m_properties.capacity = (uint16_t)maxGenItemsInArchetype;
				newArch->m_properties.chunkDataBytes = (ChunkDataOffset)currOff;
				newArch->m_properties.genEntities = (uint8_t)entsGeneric;

				return newArch;
			}

			void static destroy(Archetype* pArchetype) {
				GAIA_ASSERT(pArchetype != nullptr);
				pArchetype->~Archetype();
				mem::AllocHelper::free("Archetype", pArchetype);
			}

			ArchetypeIdLookupKey::LookupHash id_hash() const {
				return m_archetypeIdHash;
			}

			//! Sets hashes for each component type and lookup.
			//! \param hashLookup Hash used for archetype lookup purposes
			void set_hashes(LookupHash hashLookup) {
				m_hashLookup = hashLookup;
			}

			//! Enables or disables the entity on a given row in the chunk.
			//! \param pChunk Chunk the entity belongs to
			//! \param row Row of the entity
			//! \param enableEntity Enables the entity
			void enable_entity(Chunk* pChunk, uint16_t row, bool enableEntity, EntityContainers& recs) {
				pChunk->enable_entity(row, enableEntity, recs);
				// m_disabledMask.set(pChunk->idx(), enableEntity ? true : pChunk->has_disabled_entities());
			}

			//! Removes a chunk from the list of chunks managed by their archetype and deletes its memory.
			//! \param pChunk Chunk to remove from the list of managed archetypes
			void del(Chunk* pChunk) {
				// Make sure there are any chunks to delete
				GAIA_ASSERT(!m_chunks.empty());

				const auto chunkIndex = pChunk->idx();

				// Make sure the chunk is a part of the chunk array
				GAIA_ASSERT(chunkIndex == core::get_index(m_chunks, pChunk));

				// Remove the chunk from the chunk array. We are swapping this chunk's entry
				// with the last one in the array. Therefore, we first update the last item's
				// index with the current chunk's index and then do the swapping.
				m_chunks.back()->set_idx(chunkIndex);
				core::swap_erase(m_chunks, chunkIndex);

				// Delete the chunk now. Otherwise, if the chunk happened to be the last
				// one we would end up overriding released memory.
				Chunk::free(pChunk);
			}

			//! Tries to locate a chunk that has some space left for a new entity.
			//! If not found a new chunk is created.
			//! \warning Always used in tandem with try_update_free_chunk_idx() or remove_entity()
			GAIA_NODISCARD Chunk* foc_free_chunk() {
				const auto chunkCnt = m_chunks.size();

				if (chunkCnt > 0) {
					for (uint32_t i = m_firstFreeChunkIdx; i < m_chunks.size(); ++i) {
						auto* pChunk = m_chunks[i];
						GAIA_ASSERT(pChunk != nullptr);
						const auto entityCnt = pChunk->size();
						if (entityCnt < pChunk->capacity()) {
							m_firstFreeChunkIdx = i;
							return pChunk;
						}
					}
				}

				// Make sure not too many chunks are allocated
				GAIA_ASSERT(chunkCnt < UINT32_MAX);

				// No free space found anywhere. Let's create a new chunk.
				auto* pChunk = Chunk::create(
						m_cc, chunkCnt, //
						m_properties.capacity, m_properties.cntEntities, //
						m_properties.genEntities, m_properties.chunkDataBytes, //
						m_worldVersion, m_dataOffsets, m_ids, m_comps, m_compOffs);

				m_firstFreeChunkIdx = m_chunks.size();
				m_chunks.push_back(pChunk);
				return pChunk;
			}

			//! Tries to update the index of the first chunk that has space left
			//! for at least one entity.
			//! \warning Always use in tandem with foc_free_chunk()
			void try_update_free_chunk_idx() {
				// This is expected to be called only if there are any chunks
				GAIA_ASSERT(!m_chunks.empty());

				auto* pChunk = m_chunks[m_firstFreeChunkIdx];
				if (pChunk->size() >= pChunk->capacity())
					++m_firstFreeChunkIdx;
			}

			//! Tries to update the index of the first chunk that has space left
			//! for at least one entity.
			//! \warning Always use in tandem with foc_free_chunk() and remove_entity()
			void try_update_free_chunk_idx(Chunk& chunkThatRemovedEntity) {
				// This is expected to be called only if there are any chunks
				GAIA_ASSERT(!m_chunks.empty());

				if (chunkThatRemovedEntity.idx() == m_firstFreeChunkIdx)
					return;

				if (chunkThatRemovedEntity.idx() < m_firstFreeChunkIdx) {
					m_firstFreeChunkIdx = chunkThatRemovedEntity.idx();
					return;
				}

				auto* pChunk = m_chunks[m_firstFreeChunkIdx];
				if (pChunk->size() >= pChunk->capacity())
					++m_firstFreeChunkIdx;
			}

			void remove_entity(Chunk& chunk, uint16_t row, EntityContainers& recs) {
				chunk.remove_entity(row, recs);
				chunk.update_versions();

				try_update_free_chunk_idx(chunk);
			}

			GAIA_NODISCARD const Properties& props() const {
				return m_properties;
			}

			GAIA_NODISCARD const cnt::darray<Chunk*>& chunks() const {
				return m_chunks;
			}

			GAIA_NODISCARD LookupHash lookup_hash() const {
				return m_hashLookup;
			}

			GAIA_NODISCARD EntitySpan ids_view() const {
				return {&m_ids[0], m_properties.cntEntities};
			}

			GAIA_NODISCARD ComponentSpan comps_view() const {
				return {&m_comps[0], m_properties.cntEntities};
			}

			GAIA_NODISCARD ChunkDataOffsetSpan comp_offs_view() const {
				return {&m_compOffs[0], m_properties.cntEntities};
			}

			GAIA_NODISCARD uint32_t pairs() const {
				return m_pairCnt;
			}

			GAIA_NODISCARD uint32_t pairs_is() const {
				return m_pairCnt_is;
			}

			GAIA_NODISCARD Entity entity_from_pairs_as_idx(uint32_t idx) const {
				const auto ids_idx = m_pairs_as_index_buffer[idx];
				return m_ids[ids_idx];
			}

			//! Checks if an entity is a part of the archetype.
			//! \param entity Entity
			//! \return True if found. False otherwise.
			GAIA_NODISCARD bool has(Entity entity) const {
				return core::has_if(ids_view(), [&](Entity e) {
					return e == entity;
				});
			}

			//! Checks if component \tparam T is present in the chunk.
			//! \tparam T Component or pair
			//! \return True if the component is present. False otherwise.
			template <typename T>
			GAIA_NODISCARD bool has() const {
				if constexpr (is_pair<T>::value) {
					const auto rel = m_cc.get<typename T::rel>().entity;
					const auto tgt = m_cc.get<typename T::tgt>().entity;
					return has((Entity)Pair(rel, tgt));
				} else {
					const auto* pComp = m_cc.find<T>();
					return pComp != nullptr && has(pComp->entity);
				}
			}

			void build_graph_edges(Archetype* pArchetypeRight, Entity entity) {
				// Loops can't happen
				GAIA_ASSERT(pArchetypeRight != this);

				m_graph.add_edge_right(entity, pArchetypeRight->id(), pArchetypeRight->id_hash());
				pArchetypeRight->build_graph_edges_left(this, entity);
			}

			void build_graph_edges_left(Archetype* pArchetypeLeft, Entity entity) {
				// Loops can't happen
				GAIA_ASSERT(pArchetypeLeft != this);

				m_graph.add_edge_left(entity, pArchetypeLeft->id(), pArchetypeLeft->id_hash());
			}

			void del_graph_edges(Archetype* pArchetypeRight, Entity entity) {
				// Loops can't happen
				GAIA_ASSERT(pArchetypeRight != this);

				m_graph.del_edge_right(entity);
				pArchetypeRight->del_graph_edges_left(this, entity);
			}

			void del_graph_edges_left([[maybe_unused]] Archetype* pArchetypeLeft, Entity entity) {
				// Loops can't happen
				GAIA_ASSERT(pArchetypeLeft != this);

				m_graph.del_edge_left(entity);
			}

			//! Checks if an archetype graph "add" edge with entity \param entity exists.
			//! \return Archetype id of the target archetype if the edge is found. ArchetypeIdBad otherwise.
			GAIA_NODISCARD ArchetypeGraphEdge find_edge_right(Entity entity) const {
				return m_graph.find_edge_right(entity);
			}

			//! Checks if an archetype graph "del" edge with entity \param entity exists.
			//! \return Archetype id of the target archetype if the edge is found. ArchetypeIdBad otherwise.
			GAIA_NODISCARD ArchetypeGraphEdge find_edge_left(Entity entity) const {
				return m_graph.find_edge_left(entity);
			}

			GAIA_NODISCARD auto& right_edges() {
				return m_graph.right_edges();
			}

			GAIA_NODISCARD const auto& right_edges() const {
				return m_graph.right_edges();
			}

			GAIA_NODISCARD auto& left_edges() {
				return m_graph.left_edges();
			}

			GAIA_NODISCARD const auto& left_edges() const {
				return m_graph.left_edges();
			}

			//! Checks is there are no chunk in the archetype.
			GAIA_NODISCARD bool empty() const {
				return m_chunks.empty();
			}

			//! Request deleting the archetype.
			void req_del() {
				m_deleteReq = 1;
			}

			//! Returns true if this archetype is requested to be deleted.
			GAIA_NODISCARD bool is_req_del() const {
				return m_deleteReq;
			}

			//! Sets maximal lifespan of an archetype
			//! \param lifespan How many world updates an empty archetype is kept.
			//!                 If zero, the archetype it kept indefinitely.
			void set_max_lifespan(uint32_t lifespan) {
				GAIA_ASSERT(lifespan > 0);
				GAIA_ASSERT(lifespan <= MAX_ARCHETYPE_LIFESPAN);

				m_lifespanCountdownMax = lifespan;
			}

			//! Checks is this chunk is dying
			GAIA_NODISCARD bool dying() const {
				return m_lifespanCountdown > 0;
			}

			//! Marks the chunk as dead
			void die() {
				m_dead = 1;
			}

			//! Checks is this chunk is dying
			GAIA_NODISCARD bool dead() const {
				return m_dead == 1;
			}

			//! Starts the process of dying
			void start_dying() {
				GAIA_ASSERT(!dead());
				m_lifespanCountdown = m_lifespanCountdownMax;
			}

			//! Makes the archetype alive again
			void revive() {
				GAIA_ASSERT(!dead());
				m_lifespanCountdown = 0;
				m_deleteReq = 0;
			}

			//! Updates internal lifespan
			//! \return True if there is some lifespan left, false otherwise.
			GAIA_NODISCARD bool progress_death() {
				GAIA_ASSERT(dying());
				--m_lifespanCountdown;
				return dying();
			}

			//! Tells whether archetype is ready to be deleted
			GAIA_NODISCARD bool ready_to_die() const {
				return m_lifespanCountdownMax > 0 && !dying() && empty();
			}

			static void diag_entity(const World& world, Entity entity) {
				if (entity.entity()) {
					GAIA_LOG_N(
							"    ent [%u:%u] %s [%s]", entity.id(), entity.gen(), entity_name(world, entity),
							EntityKindString[entity.kind()]);
				} else if (entity.pair()) {
					GAIA_LOG_N(
							"    pair [%u:%u] %s -> %s", entity.id(), entity.gen(), entity_name(world, entity.id()),
							entity_name(world, entity.gen()));
				} else {
					const auto& cc = comp_cache(world);
					const auto& desc = cc.get(entity);
					GAIA_LOG_N(
							"    hash:%016" PRIx64 ", size:%3u B, align:%3u B, [%u:%u] %s [%s]", desc.hashLookup.hash,
							desc.comp.size(), desc.comp.alig(), desc.entity.id(), desc.entity.gen(), desc.name.str(),
							EntityKindString[entity.kind()]);
				}
			}

			static void diag_basic_info(const World& world, const Archetype& archetype) {
				auto ids = archetype.ids_view();
				auto comps = archetype.comps_view();

				// Calculate the number of entities in archetype
				uint32_t entCnt = 0;
				uint32_t entCntDisabled = 0;
				for (const auto* chunk: archetype.m_chunks) {
					entCnt += chunk->size();
					entCntDisabled += chunk->size_disabled();
				}

				// Calculate the number of components
				uint32_t genCompsSize = 0;
				uint32_t uniCompsSize = 0;
				{
					const auto& p = archetype.props();
					GAIA_FOR(p.genEntities) genCompsSize += comps[i].size();
					GAIA_FOR2(p.genEntities, comps.size()) uniCompsSize += comps[i].size();
				}

				GAIA_LOG_N(
						"aid:%u, "
						"hash:%016" PRIx64 ", "
						"chunks:%u (%uK), data:%u/%u/%u B, "
						"entities:%u/%u/%u",
						archetype.id(), archetype.lookup_hash().hash, (uint32_t)archetype.chunks().size(),
						Chunk::chunk_total_bytes(archetype.props().chunkDataBytes) <= 8192 ? 8 : 16, genCompsSize, uniCompsSize,
						archetype.props().chunkDataBytes, entCnt, entCntDisabled, archetype.props().capacity);

				if (!ids.empty()) {
					GAIA_LOG_N("  Components - count:%u", (uint32_t)ids.size());
					for (const auto ent: ids)
						diag_entity(world, ent);
				}
			}

			static void diag_graph_info(const World& world, const Archetype& archetype) {
				archetype.m_graph.diag(world);
			}

			static void diag_chunk_info(const Archetype& archetype) {
				const auto& chunks = archetype.m_chunks;
				if (chunks.empty())
					return;

				GAIA_LOG_N("  Chunks");
				for (const auto* pChunk: chunks)
					pChunk->diag();
			}

			static void diag_entity_info(const World& world, const Archetype& archetype) {
				const auto& chunks = archetype.m_chunks;
				if (chunks.empty())
					return;

				GAIA_LOG_N("  Entities");
				bool noEntities = true;
				for (const auto* pChunk: chunks) {
					if (pChunk->empty())
						continue;
					noEntities = false;

					auto ev = pChunk->entity_view();
					for (auto entity: ev)
						diag_entity(world, entity);
				}
				if (noEntities)
					GAIA_LOG_N("    N/A");
			}

			//! Performs diagnostics on a specific archetype. Prints basic info about it and the chunks it contains.
			//! \param archetype Archetype to run diagnostics on
			static void diag(const World& world, const Archetype& archetype) {
				diag_basic_info(world, archetype);
				diag_graph_info(world, archetype);
				diag_chunk_info(archetype);
				diag_entity_info(world, archetype);
			}
		};

		class ArchetypeLookupKey final {
			Archetype::LookupHash m_hash;
			const ArchetypeBase* m_pArchetypeBase;

		public:
			static constexpr bool IsDirectHashKey = true;

			ArchetypeLookupKey(): m_hash({0}), m_pArchetypeBase(nullptr) {}
			explicit ArchetypeLookupKey(Archetype::LookupHash hash, const ArchetypeBase* pArchetypeBase):
					m_hash(hash), m_pArchetypeBase(pArchetypeBase) {}

			GAIA_NODISCARD size_t hash() const {
				return (size_t)m_hash.hash;
			}

			GAIA_NODISCARD Archetype* archetype() const {
				return (Archetype*)m_pArchetypeBase;
			}

			GAIA_NODISCARD bool operator==(const ArchetypeLookupKey& other) const {
				// Hash doesn't match we don't have a match.
				// Hash collisions are expected to be very unlikely so optimize for this case.
				if GAIA_LIKELY (m_hash != other.m_hash)
					return false;

				const auto id = m_pArchetypeBase->id();
				if (id == ArchetypeIdBad) {
					const auto* pArchetype = (const Archetype*)other.m_pArchetypeBase;
					const auto* pArchetypeLookupChecker = (const ArchetypeLookupChecker*)m_pArchetypeBase;
					return pArchetype->cmp_comps(*pArchetypeLookupChecker);
				}

				// Real ArchetypeID is given. Compare the pointers.
				// Normally we'd compare archetype IDs but because we do not allow archetype copies and all archetypes are
				// unique it's guaranteed that if pointers are the same we have a match.
				// This also saves a pointer indirection because we do not access the memory the pointer points to.
				return m_pArchetypeBase == other.m_pArchetypeBase;
			}
		};

		using ArchetypeMapByHash = cnt::map<ArchetypeLookupKey, Archetype*>;
	} // namespace ecs
} // namespace gaia

/*** End of inlined file: archetype.h ***/


/*** Start of inlined file: chunk_iterator.h ***/
#pragma once

#include <cinttypes>
#include <cstdint>
#include <type_traits>


/*** Start of inlined file: query_common.h ***/
#pragma once

#include <type_traits>


/*** Start of inlined file: data_buffer.h ***/
#pragma once

#include <type_traits>

namespace gaia {
	namespace ecs {
		namespace detail {
			static constexpr uint32_t SerializationBufferCapacityIncreaseSize = 128U;

			template <typename DataContainer>
			class SerializationBufferImpl {
				// Increase the capacity by multiples of CapacityIncreaseSize
				static constexpr uint32_t CapacityIncreaseSize = SerializationBufferCapacityIncreaseSize;

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
					mem::unaligned_ref<T> mem((void*)&m_data[m_dataPos]);
					mem = GAIA_FWD(value);

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
				void save_comp(const ComponentCache& cc, T&& value) {
					const auto& desc = cc.get<T>();
					const bool isManualDestroyNeeded = desc.func_copy_ctor != nullptr || desc.func_move_ctor != nullptr;
					constexpr bool isRValue = std::is_rvalue_reference_v<decltype(value)>;

					reserve(sizeof(isManualDestroyNeeded) + sizeof(T));
					save(isManualDestroyNeeded);
					m_data.resize(m_dataPos + sizeof(T));

					auto* pSrc = (void*)&value; // TODO: GAIA_FWD(value)?
					auto* pDst = (void*)&m_data[m_dataPos];
					if (isRValue && desc.func_move_ctor != nullptr) {
						if constexpr (mem::is_movable<T>())
							mem::detail::move_ctor_element_aos<T>((T*)pDst, (T*)pSrc, 0, 0);
						else
							mem::detail::copy_ctor_element_aos<T>((T*)pDst, (const T*)pSrc, 0, 0);
					} else
						mem::detail::copy_ctor_element_aos<T>((T*)pDst, (const T*)pSrc, 0, 0);

					m_dataPos += sizeof(T);
				}

				//! Loads \param value from the buffer
				template <typename T>
				void load(T& value) {
					GAIA_ASSERT(m_dataPos + sizeof(T) <= bytes());

					const auto& cdata = std::as_const(m_data);
					value = mem::unaligned_ref<T>((void*)&cdata[m_dataPos]);

					m_dataPos += sizeof(T);
				}

				//! Loads \param size bytes of data from the buffer and writes them to the address \param pDst
				void load(void* pDst, uint32_t size) {
					GAIA_ASSERT(m_dataPos + size <= bytes());

					const auto& cdata = std::as_const(m_data);
					memmove(pDst, (const void*)&cdata[m_dataPos], size);

					m_dataPos += size;
				}

				//! Loads \param value from the buffer
				void load_comp(const ComponentCache& cc, void* pDst, Entity entity) {
					bool isManualDestroyNeeded = false;
					load(isManualDestroyNeeded);

					const auto& desc = cc.get(entity);
					GAIA_ASSERT(m_dataPos + desc.comp.size() <= bytes());
					const auto& cdata = std::as_const(m_data);
					auto* pSrc = (void*)&cdata[m_dataPos];
					desc.move(pDst, pSrc, 0, 0, 1, 1);
					if (isManualDestroyNeeded)
						desc.dtor(pSrc);

					m_dataPos += desc.comp.size();
				}
			};
		} // namespace detail

		using SerializationBuffer_DArrExt = cnt::darray_ext<uint8_t, detail::SerializationBufferCapacityIncreaseSize>;
		using SerializationBuffer_DArr = cnt::darray<uint8_t>;

		using SerializationBuffer = detail::SerializationBufferImpl<SerializationBuffer_DArrExt>;
		using SerializationBufferDyn = detail::SerializationBufferImpl<SerializationBuffer_DArr>;
	} // namespace ecs
} // namespace gaia
/*** End of inlined file: data_buffer.h ***/

namespace gaia {
	namespace ecs {
		class World;
		class Archetype;

		//! Number of items that can be a part of Query
		static constexpr uint32_t MAX_ITEMS_IN_QUERY = 8U;

		GAIA_GCC_WARNING_PUSH()
		// GCC is unnecessarily too strict about shadowing.
		// We have a globally defined entity All and thinks QueryOpKind::All shadows it.
		GAIA_GCC_WARNING_DISABLE("-Wshadow")

		//! Operation type
		enum class QueryOpKind : uint8_t { All, Any, Not, Count };
		//! Access type
		enum class QueryAccess : uint8_t { None, Read, Write };
		//! Operation flags
		enum class QueryInputFlags : uint8_t { None, Variable };

		GAIA_GCC_WARNING_POP()

		using QueryId = uint32_t;
		using GroupId = uint32_t;
		using QueryLookupHash = core::direct_hash_key<uint64_t>;
		using QueryEntityArray = cnt::sarray_ext<Entity, MAX_ITEMS_IN_QUERY>;
		using QueryArchetypeCacheIndexMap = cnt::map<EntityLookupKey, uint32_t>;
		using QueryOpArray = cnt::sarray_ext<QueryOpKind, MAX_ITEMS_IN_QUERY>;
		using QuerySerBuffer = SerializationBufferDyn;
		using QuerySerMap = cnt::map<QueryId, QuerySerBuffer>;
		using TGroupByFunc = GroupId (*)(const World&, const Archetype&, Entity);

		static constexpr QueryId QueryIdBad = (QueryId)-1;
		static constexpr GroupId GroupIdMax = ((GroupId)-1) - 1;

		struct QueryHandle {
			static constexpr uint32_t IdMask = QueryIdBad;

		private:
			struct HandleData {
				QueryId id;
				uint32_t gen;
			};

			union {
				HandleData data;
				uint64_t val;
			};

		public:
			constexpr QueryHandle() noexcept: val((uint64_t)-1) {};

			QueryHandle(QueryId id, uint32_t gen) {
				data.id = id;
				data.gen = gen;
			}
			~QueryHandle() = default;

			QueryHandle(QueryHandle&&) noexcept = default;
			QueryHandle(const QueryHandle&) = default;
			QueryHandle& operator=(QueryHandle&&) noexcept = default;
			QueryHandle& operator=(const QueryHandle&) = default;

			GAIA_NODISCARD constexpr bool operator==(const QueryHandle& other) const noexcept {
				return val == other.val;
			}
			GAIA_NODISCARD constexpr bool operator!=(const QueryHandle& other) const noexcept {
				return val != other.val;
			}

			GAIA_NODISCARD auto id() const {
				return data.id;
			}
			GAIA_NODISCARD auto gen() const {
				return data.gen;
			}
			GAIA_NODISCARD auto value() const {
				return val;
			}
		};

		inline static const QueryHandle QueryHandleBad = QueryHandle();

		//! Hashmap lookup structure used for Entity
		struct QueryHandleLookupKey {
			using LookupHash = core::direct_hash_key<uint64_t>;

		private:
			//! Entity
			QueryHandle m_handle;
			//! Entity hash
			LookupHash m_hash;

			static LookupHash calc(QueryHandle handle) {
				return {core::calculate_hash64(handle.value())};
			}

		public:
			static constexpr bool IsDirectHashKey = true;

			QueryHandleLookupKey() = default;
			explicit QueryHandleLookupKey(QueryHandle handle): m_handle(handle), m_hash(calc(handle)) {}
			~QueryHandleLookupKey() = default;

			QueryHandleLookupKey(const QueryHandleLookupKey&) = default;
			QueryHandleLookupKey(QueryHandleLookupKey&&) = default;
			QueryHandleLookupKey& operator=(const QueryHandleLookupKey&) = default;
			QueryHandleLookupKey& operator=(QueryHandleLookupKey&&) = default;

			QueryHandle handle() const {
				return m_handle;
			}

			size_t hash() const {
				return (size_t)m_hash.hash;
			}

			bool operator==(const QueryHandleLookupKey& other) const {
				if GAIA_LIKELY (m_hash != other.m_hash)
					return false;

				return m_handle == other.m_handle;
			}

			bool operator!=(const QueryHandleLookupKey& other) const {
				return !operator==(other);
			}
		};

		inline static const QueryHandleLookupKey QueryHandleBadLookupKey = QueryHandleLookupKey(QueryHandleBad);

		//! User-provided query input
		struct QueryInput {
			//! Operation to perform with the input
			QueryOpKind op = QueryOpKind::All;
			//! Access type
			QueryAccess access = QueryAccess::Read;
			//! Entity/Component/Pair to query
			Entity id;
			//! Source entity to query the id on.
			//! If id==EntityBad the source is fixed.
			//! If id!=src the source is variable.
			Entity src = EntityBad;
		};

		//! Internal representation of QueryInput
		struct QueryTerm {
			//! Queried id
			Entity id;
			//! Source of where the queried id is looked up at
			Entity src;
			//! Archetype of the src entity
			Archetype* srcArchetype;
			//! Operation to perform with the term
			QueryOpKind op;

			bool operator==(const QueryTerm& other) const {
				return id == other.id && src == other.src && op == other.op;
			}
			bool operator!=(const QueryTerm& other) const {
				return !operator==(other);
			}
		};

		using QueryTermArray = cnt::sarray_ext<QueryTerm, MAX_ITEMS_IN_QUERY>;
		using QueryTermSpan = std::span<QueryTerm>;
		using QueryRemappingArray = cnt::sarray_ext<uint8_t, MAX_ITEMS_IN_QUERY>;

		QuerySerBuffer& query_buffer(World& world, QueryId& serId);
		void query_buffer_reset(World& world, QueryId& serId);
		ComponentCache& comp_cache_mut(World& world);

		struct QueryIdentity {
			//! Query id
			QueryHandle handle = {};
			//! Serialization id
			QueryId serId = QueryIdBad;

			GAIA_NODISCARD QuerySerBuffer& ser_buffer(World* world) {
				return query_buffer(*world, serId);
			}
			void ser_buffer_reset(World* world) {
				query_buffer_reset(*world, serId);
			}
		};

		struct QueryCtx {
			// World
			const World* w{};
			//! Component cache
			ComponentCache* cc{};
			//! Lookup hash for this query
			QueryLookupHash hashLookup{};
			//! Query identity
			QueryIdentity q{};

			enum QueryFlags : uint8_t { //
				SortGroups = 0x01
			};

			struct Data {
				//! Array of queried ids
				QueryEntityArray ids;
				//! Array of terms
				QueryTermArray terms;
				//! Index of the last checked archetype in the component-to-archetype map
				QueryArchetypeCacheIndexMap lastMatchedArchetypeIdx_All;
				QueryArchetypeCacheIndexMap lastMatchedArchetypeIdx_Any;
				QueryArchetypeCacheIndexMap lastMatchedArchetypeIdx_Not;
				//! Mapping of the original indices to the new ones after sorting
				QueryRemappingArray remapping;
				//! Array of filtered components
				QueryEntityArray changed;
				//! Entity to group the archetypes by. EntityBad for no grouping.
				Entity groupBy;
				//! Iteration will be restricted only to target Group
				GroupId groupIdSet;
				//! Function to use to perform the grouping
				TGroupByFunc groupByFunc;
				//! Mask for items with Is relationship pair.
				//! If the id is a pair, the first part (id) is written here.
				uint32_t as_mask_0;
				//! Mask for items with Is relationship pair.
				//! If the id is a pair, the second part (gen) is written here.
				uint32_t as_mask_1;
				//! First NOT record in pairs/ids/ops
				uint8_t firstNot;
				//! First ANY record in pairs/ids/ops
				uint8_t firstAny;
				//! Read-write mask. Bit 0 stands for component 0 in component arrays.
				//! A set bit means write access is requested.
				uint8_t readWriteMask;
				//! Query flags
				uint8_t flags;
			} data{};
			// Make sure that MAX_ITEMS_IN_QUERY can fit into data.readWriteMask
			static_assert(MAX_ITEMS_IN_QUERY == 8);

			void init(World* pWorld) {
				w = pWorld;
				cc = &comp_cache_mut(*pWorld);
			}

			GAIA_NODISCARD bool operator==(const QueryCtx& other) const {
				// Comparison expected to be done only the first time the query is set up
				GAIA_ASSERT(q.handle.id() == QueryIdBad);
				// Fast path when cache ids are set
				// if (queryId != QueryIdBad && queryId == other.queryId)
				// 	return true;

				// Lookup hash must match
				if (hashLookup != other.hashLookup)
					return false;

				const auto& left = data;
				const auto& right = other.data;

				// Check array sizes first
				if (left.terms.size() != right.terms.size())
					return false;
				if (left.changed.size() != right.changed.size())
					return false;
				if (left.readWriteMask != right.readWriteMask)
					return false;

				// Components need to be the same
				if (left.terms != right.terms)
					return false;

				// Filters need to be the same
				if (left.changed != right.changed)
					return false;

				// Grouping data need to match
				if (left.groupBy != right.groupBy)
					return false;
				if (left.groupByFunc != right.groupByFunc)
					return false;

				return true;
			}

			GAIA_NODISCARD bool operator!=(const QueryCtx& other) const {
				return !operator==(other);
			};
		};

		//! Functor for sorting terms in a query before compilation
		struct query_sort_cond {
			constexpr bool operator()(const QueryTerm& lhs, const QueryTerm& rhs) const {
				// Smaller ops first.
				if (lhs.op != rhs.op)
					return lhs.op < rhs.op;

				// Smaller ids second.
				if (lhs.id != rhs.id)
					return SortComponentCond()(lhs.id, rhs.id);

				// Sources go last. Note, sources are never a pair.
				// We want to do it this way because it would be expensive to build cache for
				// the entire tree. Rather, we only cache fixed parts of the query without
				// variables.
				// TODO: In theory, there might be a better way to sort sources.
				//       E.g. depending on the number of archetypes we'd have to traverse
				//       it might be beneficial to do a different ordering which is impossible
				//       to do at this point.
				return SortComponentCond()(lhs.src, rhs.src);
			}
		};

		//! Sorts internal component arrays
		inline void sort(QueryCtx& ctx) {
			auto& data = ctx.data;

			auto remappingCopy = data.remapping;

			// Sort data. Necessary for correct hash calculation.
			// Without sorting query.all<XXX, YYY> would be different than query.all<YYY, XXX>.
			// Also makes sure data is in optimal order for query processing.
			core::sort(data.terms, query_sort_cond{}, [&](uint32_t left, uint32_t right) {
				core::swap(data.ids[left], data.ids[right]);
				core::swap(data.terms[left], data.terms[right]);
				core::swap(remappingCopy[left], remappingCopy[right]);

				// Make sure masks remains correct after sorting
				core::swap_bits(data.readWriteMask, left, right);
				core::swap_bits(data.as_mask_0, left, right);
				core::swap_bits(data.as_mask_1, left, right);
			});

			// Update remapping indices.
			// E.g., let us have ids 0, 14, 15, with indices 0, 1, 2.
			// After sorting they become 14, 15, 0, with indices 1, 2, 0.
			// So indices mapping is as follows: 0 -> 1, 1 -> 2, 2 -> 0.
			// After remapping update, indices become 0 -> 2, 1 -> 0, 2 -> 1.
			// Therefore, if we want to see where 15 was located originally (curr index 1), we do look at index 2 and get 1.
			GAIA_EACH(data.terms) {
				const auto idxBeforeRemapping = (uint8_t)core::get_index_unsafe(remappingCopy, (uint8_t)i);
				data.remapping[i] = idxBeforeRemapping;
			}

			const auto& terms = data.terms;
			if (!terms.empty()) {
				uint32_t i = 0;
				while (i < terms.size() && terms[i].op == QueryOpKind::All)
					++i;
				data.firstAny = (uint8_t)i;
				while (i < terms.size() && terms[i].op == QueryOpKind::Any)
					++i;
				data.firstNot = (uint8_t)i;
			}
		}

		inline void calc_lookup_hash(QueryCtx& ctx) {
			GAIA_ASSERT(ctx.cc != nullptr);
			// Make sure we don't calculate the hash twice
			GAIA_ASSERT(ctx.hashLookup.hash == 0);

			QueryLookupHash::Type hashLookup = 0;

			auto& data = ctx.data;

			// Ids & ops
			{
				QueryLookupHash::Type hash = 0;

				const auto& terms = data.terms;
				for (auto pair: terms) {
					hash = core::hash_combine(hash, (QueryLookupHash::Type)pair.op);
					hash = core::hash_combine(hash, (QueryLookupHash::Type)pair.id.value());
				}
				hash = core::hash_combine(hash, (QueryLookupHash::Type)terms.size());
				hash = core::hash_combine(hash, (QueryLookupHash::Type)data.readWriteMask);

				hashLookup = hash;
			}

			// Filters
			{
				QueryLookupHash::Type hash = 0;

				const auto& changed = data.changed;
				for (auto id: changed)
					hash = core::hash_combine(hash, (QueryLookupHash::Type)id.value());
				hash = core::hash_combine(hash, (QueryLookupHash::Type)changed.size());

				hashLookup = core::hash_combine(hashLookup, hash);
			}

			// Grouping
			{
				QueryLookupHash::Type hash = 0;

				hash = core::hash_combine(hash, (QueryLookupHash::Type)data.groupBy.value());
				hash = core::hash_combine(hash, (QueryLookupHash::Type)data.groupByFunc);

				hashLookup = core::hash_combine(hashLookup, hash);
			}

			ctx.hashLookup = {core::calculate_hash64(hashLookup)};
		}

		//! Located the index at which the provided component id is located in the component array
		//! \param pComps Pointer to the start of the component array
		//! \param entity Entity we search for
		//! \return Index of the component id in the array
		//! \warning The component id must be present in the array
		template <uint32_t MAX_COMPONENTS>
		GAIA_NODISCARD inline uint32_t comp_idx(const QueryTerm* pTerms, Entity entity, Entity src) {
			// We let the compiler know the upper iteration bound at compile-time.
			// This way it can optimize better (e.g. loop unrolling, vectorization).
			GAIA_FOR(MAX_COMPONENTS) {
				if (pTerms[i].id == entity && pTerms[i].src == src)
					return i;
			}

			GAIA_ASSERT(false);
			return BadIndex;
		}
	} // namespace ecs
} // namespace gaia
/*** End of inlined file: query_common.h ***/

namespace gaia {
	namespace ecs {
		class World;

		template <typename T>
		const ComponentCacheItem& comp_cache_add(World& world);

		//! QueryImpl constraints
		enum class Constraints : uint8_t { EnabledOnly, DisabledOnly, AcceptAll };

		namespace detail {
			template <Constraints IterConstraint>
			class ChunkIterImpl {
			protected:
				using CompIndicesBitView = core::bit_view<ChunkHeader::MAX_COMPONENTS_BITS>;

				//! Chunk currently associated with the iterator
				Chunk* m_pChunk = nullptr;
				//! ChunkHeader::MAX_COMPONENTS values for component indices mapping for the parent archetype
				const uint8_t* m_pCompIdxMapping = nullptr;
				//! GroupId. 0 if not set.
				GroupId m_groupId = 0;

			public:
				ChunkIterImpl() = default;
				~ChunkIterImpl() = default;
				ChunkIterImpl(ChunkIterImpl&&) noexcept = default;
				ChunkIterImpl& operator=(ChunkIterImpl&&) noexcept = default;
				ChunkIterImpl(const ChunkIterImpl&) = delete;
				ChunkIterImpl& operator=(const ChunkIterImpl&) = delete;

				void set_chunk(Chunk* pChunk) {
					GAIA_ASSERT(pChunk != nullptr);
					m_pChunk = pChunk;
				}

				void set_remapping_indices(const uint8_t* pCompIndicesMapping) {
					m_pCompIdxMapping = pCompIndicesMapping;
				}

				void set_group_id(GroupId groupId) {
					m_groupId = groupId;
				}

				GAIA_NODISCARD GroupId group_id() const {
					return m_groupId;
				}

				//! Returns a read-only entity or component view.
				//! \warning If \tparam T is a component it is expected it is present. Undefined behavior otherwise.
				//! \tparam T Component or Entity
				//! \return Entity of component view with read-only access
				template <typename T>
				GAIA_NODISCARD auto view() const {
					return m_pChunk->view<T>(from(), to());
				}

				template <typename T>
				GAIA_NODISCARD auto view(uint32_t termIdx) {
					using U = typename actual_type_t<T>::Type;

					const auto compIdx = m_pCompIdxMapping[termIdx];
					GAIA_ASSERT(compIdx < m_pChunk->ids_view().size());

					if constexpr (mem::is_soa_layout_v<U>) {
						auto* pData = m_pChunk->comp_ptr_mut(compIdx);
						return m_pChunk->view_raw<T>(pData, m_pChunk->capacity());
					} else {
						auto* pData = m_pChunk->comp_ptr_mut(compIdx, from());
						return m_pChunk->view_raw<T>(pData, to() - from());
					}
				}

				//! Returns a mutable entity or component view.
				//! \warning If \tparam T is a component it is expected it is present. Undefined behavior otherwise.
				//! \tparam T Component or Entity
				//! \return Entity or component view with read-write access
				template <typename T>
				GAIA_NODISCARD auto view_mut() {
					return m_pChunk->view_mut<T>(from(), to());
				}

				template <typename T>
				GAIA_NODISCARD auto view_mut(uint32_t termIdx) {
					using U = typename actual_type_t<T>::Type;

					const auto compIdx = m_pCompIdxMapping[termIdx];
					GAIA_ASSERT(compIdx < m_pChunk->comp_rec_view().size());

					m_pChunk->update_world_version(compIdx);

					if constexpr (mem::is_soa_layout_v<U>) {
						auto* pData = m_pChunk->comp_ptr_mut(compIdx);
						return m_pChunk->view_mut_raw<T>(pData, m_pChunk->capacity());
					} else {
						auto* pData = m_pChunk->comp_ptr_mut(compIdx, from());
						return m_pChunk->view_mut_raw<T>(pData, to() - from());
					}
				}

				//! Returns a mutable component view.
				//! Doesn't update the world version when the access is acquired.
				//! \warning It is expected the component \tparam T is present. Undefined behavior otherwise.
				//! \tparam T Component
				//! \return Component view with read-write access
				template <typename T>
				GAIA_NODISCARD auto sview_mut() {
					return m_pChunk->sview_mut<T>(from(), to());
				}

				template <typename T>
				GAIA_NODISCARD auto sview_mut(uint32_t termIdx) {
					using U = typename actual_type_t<T>::Type;

					const auto compIdx = m_pCompIdxMapping[termIdx];
					GAIA_ASSERT(compIdx < m_pChunk->ids_view().size());

					if constexpr (mem::is_soa_layout_v<U>) {
						auto* pData = m_pChunk->comp_ptr_mut(compIdx);
						return m_pChunk->view_mut_raw<T>(pData, m_pChunk->capacity());
					} else {
						auto* pData = m_pChunk->comp_ptr_mut(compIdx, from());
						return m_pChunk->view_mut_raw<T>(pData, to() - from());
					}
				}

				//! Returns either a mutable or immutable entity/component view based on the requested type.
				//! Value and const types are considered immutable. Anything else is mutable.
				//! \warning If \tparam T is a component it is expected to be present. Undefined behavior otherwise.
				//! \tparam T Component or Entity
				//! \return Entity or component view
				template <typename T>
				GAIA_NODISCARD auto view_auto() {
					return m_pChunk->view_auto<T>(from(), to());
				}

				//! Returns either a mutable or immutable entity/component view based on the requested type.
				//! Value and const types are considered immutable. Anything else is mutable.
				//! Doesn't update the world version when read-write access is acquired.
				//! \warning If \tparam T is a component it is expected to be present. Undefined behavior otherwise.
				//! \tparam T Component or Entity
				//! \return Entity or component view
				template <typename T>
				GAIA_NODISCARD auto sview_auto() {
					return m_pChunk->sview_auto<T>(from(), to());
				}

				//! Checks if the entity at the current iterator index is enabled.
				//! \return True it the entity is enabled. False otherwise.
				GAIA_NODISCARD bool enabled(uint32_t index) const {
					const auto row = (uint16_t)(from() + index);
					return m_pChunk->enabled(row);
				}

				//! Checks if entity \param entity is present in the chunk.
				//! \param entity Entity
				//! \return True if the component is present. False otherwise.
				GAIA_NODISCARD bool has(Entity entity) const {
					return m_pChunk->has(entity);
				}

				//! Checks if relationship pair \param pair is present in the chunk.
				//! \param pair Relationship pair
				//! \return True if the component is present. False otherwise.
				GAIA_NODISCARD bool has(Pair pair) const {
					return m_pChunk->has((Entity)pair);
				}

				//! Checks if component \tparam T is present in the chunk.
				//! \tparam T Component
				//! \return True if the component is present. False otherwise.
				template <typename T>
				GAIA_NODISCARD bool has() const {
					return m_pChunk->has<T>();
				}

				//! Returns the number of entities accessible via the iterator
				GAIA_NODISCARD uint16_t size() const noexcept {
					if constexpr (IterConstraint == Constraints::EnabledOnly)
						return m_pChunk->size_enabled();
					else if constexpr (IterConstraint == Constraints::DisabledOnly)
						return m_pChunk->size_disabled();
					else
						return m_pChunk->size();
				}

				//! Returns the absolute index that should be used to access an item in the chunk.
				//! AoS indices map directly, SoA indices need some adjustments because the view is
				//! always considered {0..ChunkCapacity} instead of {FirstEnabled..ChunkSize}.
				template <typename T>
				uint32_t acc_index(uint32_t idx) const noexcept {
					using U = typename actual_type_t<T>::Type;

					if constexpr (mem::is_soa_layout_v<U>)
						return idx + from();
					else
						return idx;
				}

			protected:
				//! Returns the starting index of the iterator
				GAIA_NODISCARD uint16_t from() const noexcept {
					if constexpr (IterConstraint == Constraints::EnabledOnly)
						return m_pChunk->size_disabled();
					else
						return 0;
				}

				//! Returns the ending index of the iterator (one past the last valid index)
				GAIA_NODISCARD uint16_t to() const noexcept {
					if constexpr (IterConstraint == Constraints::DisabledOnly)
						return m_pChunk->size_disabled();
					else
						return m_pChunk->size();
				}
			};
		} // namespace detail

		//! Iterator for iterating enabled entities
		using Iter = detail::ChunkIterImpl<Constraints::EnabledOnly>;
		//! Iterator for iterating disabled entities
		using IterDisabled = detail::ChunkIterImpl<Constraints::DisabledOnly>;

		//! Iterator for iterating both enabled and disabled entities.
		//! Disabled entities always precede enabled ones.
		class IterAll: public detail::ChunkIterImpl<Constraints::AcceptAll> {
		public:
			//! Returns the number of enabled entities accessible via the iterator.
			GAIA_NODISCARD uint16_t size_enabled() const noexcept {
				return m_pChunk->size_enabled();
			}

			//! Returns the number of disabled entities accessible via the iterator.
			//! Can be read also as "the index of the first enabled entity".
			GAIA_NODISCARD uint16_t size_disabled() const noexcept {
				return m_pChunk->size_disabled();
			}
		};
	} // namespace ecs
} // namespace gaia
/*** End of inlined file: chunk_iterator.h ***/


/*** Start of inlined file: command_buffer.h ***/
#pragma once

#include <cstdint>
#include <type_traits>


/*** Start of inlined file: world.h ***/
#pragma once

#include <cstdint>
#include <type_traits>


/*** Start of inlined file: component_getter.h ***/
#pragma once
#include <cstdint>

namespace gaia {
	namespace ecs {
		struct ComponentGetter {
			const Chunk* m_pChunk;
			uint16_t m_row;

			//! Returns the value stored in the component \tparam T on \param entity.
			//! \tparam T Component
			//! \return Value stored in the component.
			template <typename T>
			GAIA_NODISCARD decltype(auto) get() const {
				verify_comp<T>();

				if constexpr (entity_kind_v<T> == EntityKind::EK_Gen)
					return m_pChunk->template get<T>(m_row);
				else
					return m_pChunk->template get<T>();
			}
		};
	} // namespace ecs
} // namespace gaia
/*** End of inlined file: component_getter.h ***/


/*** Start of inlined file: component_setter.h ***/
#pragma once

#include <cstdint>

namespace gaia {
	namespace ecs {
		struct ComponentSetter: public ComponentGetter {
			//! Returns a mutable reference to component.
			//! \tparam T Component or pair
			//! \return Reference to data for AoS, or mutable accessor for SoA types
			template <typename T>
			decltype(auto) mut() {
				return const_cast<Chunk*>(m_pChunk)->template set<T>(m_row);
			}

			//! Sets the value of the component \tparam T.
			//! \tparam T Component or pair
			//! \param value Value to set for the component
			//! \return ComponentSetter
			template <typename T, typename U = typename actual_type_t<T>::Type>
			ComponentSetter& set(U&& value) {
				mut<T>() = GAIA_FWD(value);
				return *this;
			}

			//! Returns a mutable reference to component.
			//! \tparam T Component or pair
			//! \return Reference to data for AoS, or mutable accessor for SoA types
			template <typename T>
			decltype(auto) mut(Entity type) {
				return const_cast<Chunk*>(m_pChunk)->template set<T>(m_row, type);
			}

			//! Sets the value of the component \param type.
			//! \tparam T Component or pair
			//! \param type Entity associated with the type
			//! \param value Value to set for the component
			//! \return ComponentSetter
			template <typename T>
			ComponentSetter& set(Entity type, T&& value) {
				mut<T>(type) = GAIA_FWD(value);
				return *this;
			}

			//! Returns a mutable reference to component without triggering a world version update.
			//! \tparam T Component or pair
			//! \return Reference to data for AoS, or mutable accessor for SoA types
			template <typename T>
			decltype(auto) smut() {
				return const_cast<Chunk*>(m_pChunk)->template sset<T>(m_row);
			}

			//! Sets the value of the component without triggering a world version update.
			//! \tparam T Component or pair
			//! \param value Value to set for the component
			//! \return ComponentSetter
			template <typename T, typename U = typename actual_type_t<T>::Type>
			ComponentSetter& sset(U&& value) {
				smut<T>() = GAIA_FWD(value);
				return *this;
			}

			//! Returns a mutable reference to component without triggering a world version update.
			//! \tparam T Component or pair
			//! \param type Entity associated with the type
			//! \return Reference to data for AoS, or mutable accessor for SoA types
			template <typename T>
			decltype(auto) smut(Entity type) {
				return const_cast<Chunk*>(m_pChunk)->template sset<T>(type);
			}

			//! Sets the value of the component without triggering a world version update.
			//! \tparam T Component or pair
			//! \param type Entity associated with the type
			//! \param value Value to set for the component
			//! \return ComponentSetter
			template <typename T>
			ComponentSetter& sset(Entity type, T&& value) {
				smut<T>(type) = GAIA_FWD(value);
				return *this;
			}
		};
	} // namespace ecs
} // namespace gaia
/*** End of inlined file: component_setter.h ***/


/*** Start of inlined file: query.h ***/
#pragma once

#include <cstdarg>
#include <type_traits>


/*** Start of inlined file: query_cache.h ***/
#pragma once


/*** Start of inlined file: query_info.h ***/
#pragma once


/*** Start of inlined file: vm.h ***/
#pragma once

#include <cstdint>
#include <type_traits>

namespace gaia {
	namespace ecs {
		extern Archetype* archetype_from_entity(const World& world, Entity entity);
		extern GroupId group_by_func_default(const World& world, const Archetype& archetype, Entity groupBy);
		extern bool is(const World& world, Entity entity, Entity baseEntity);

		template <typename Func>
		void as_relations_trav(const World& world, Entity target, Func func);
		template <typename Func>
		bool as_relations_trav_if(const World& world, Entity target, Func func);

		using EntityToArchetypeMap = cnt::map<EntityLookupKey, ArchetypeDArray>;

		namespace vm {

			struct MatchingCtx {
				// Setup up externally
				//////////////////////////////////

				//! World
				const World* pWorld;
				//! entity -> archetypes mapping
				const EntityToArchetypeMap* pEntityToArchetypeMap;
				//! Array of all archetypes in the world
				const ArchetypeDArray* pAllArchetypes;
				//! Set of already matched archetypes. Reset before each exec().
				cnt::set<Archetype*>* pMatchesSet;
				//! Array of already matches archetypes. Reset before each exec().
				ArchetypeDArray* pMatchesArr;
				//! Idx of the last matched archetype against the ALL opcode
				QueryArchetypeCacheIndexMap* pLastMatchedArchetypeIdx_All;
				//! Idx of the last matched archetype against the ANY opcode
				QueryArchetypeCacheIndexMap* pLastMatchedArchetypeIdx_Any;
				//! Idx of the last matched archetype against the NOT opcode
				QueryArchetypeCacheIndexMap* pLastMatchedArchetypeIdx_Not;
				//! Mask for items with Is relationship pair.
				//! If the id is a pair, the first part (id) is written here.
				uint32_t as_mask_0;
				//! Mask for items with Is relationship pair.
				//! If the id is a pair, the second part (gen) is written here.
				uint32_t as_mask_1;

				// For the opcode compiler to modify
				//////////////////////////////////////

				//! Entity to match
				Entity ent;
				//! List of entity ids in a query to consider
				EntitySpan idsToMatch;
				//! Current stack position (program counter)
				uint32_t pc;
			};

			inline const ArchetypeDArray* fetch_archetypes_for_select(const EntityToArchetypeMap& map, EntityLookupKey key) {
				GAIA_ASSERT(key != EntityBadLookupKey);

				const auto it = map.find(key);
				if (it == map.end() || it->second.empty())
					return nullptr;

				return &it->second;
			}

			inline const ArchetypeDArray* fetch_archetypes_for_select(const EntityToArchetypeMap& map, Entity src) {
				GAIA_ASSERT(src != EntityBad);

				return fetch_archetypes_for_select(map, EntityLookupKey(src));
			}

			namespace detail {
				enum class EOpcode : uint8_t {
					//! X
					All,
					//! ?X
					Any_NoAll,
					Any_WithAll,
					//! !X
					Not
				};

				using VmLabel = uint16_t;

				struct CompiledOp {
					//! Opcode to execute
					EOpcode opcode;
					//! Stack position to go to if the opcode returns true
					VmLabel pc_ok;
					//! Stack position to go to if the opcode returns false
					VmLabel pc_fail;
				};

				struct QueryCompileCtx {
					cnt::darray<CompiledOp> ops;
					//! Array of ops that can be evaluated with a ALL opcode
					cnt::sarr_ext<Entity, MAX_ITEMS_IN_QUERY> ids_all;
					//! Array of ops that can be evaluated with a ANY opcode
					cnt::sarr_ext<Entity, MAX_ITEMS_IN_QUERY> ids_any;
					//! Array of ops that can be evaluated with a NOT opcode
					cnt::sarr_ext<Entity, MAX_ITEMS_IN_QUERY> ids_not;
				};

				inline uint32_t handle_last_archetype_match(
						QueryArchetypeCacheIndexMap* pCont, EntityLookupKey entityKey, const ArchetypeDArray* pSrcArchetype) {
					const auto cache_it = pCont->find(entityKey);
					uint32_t lastMatchedIdx = 0;
					if (cache_it == pCont->end())
						pCont->emplace(entityKey, pSrcArchetype->size());
					else {
						lastMatchedIdx = cache_it->second;
						cache_it->second = pSrcArchetype->size();
					}
					return lastMatchedIdx;
				}

				// Operator ALL (used by query::all)
				struct OpAll {
					static bool can_continue(bool hasMatch) {
						return hasMatch;
					};
					static bool eval(uint32_t expectedMatches, uint32_t totalMatches) {
						return expectedMatches == totalMatches;
					}
					static uint32_t
					handle_last_match(MatchingCtx& ctx, EntityLookupKey entityKey, const ArchetypeDArray* pSrcArchetype) {
						return handle_last_archetype_match(ctx.pLastMatchedArchetypeIdx_All, entityKey, pSrcArchetype);
					}
				};
				// Operator OR (used by query::any)
				struct OpAny {
					static bool can_continue(bool hasMatch) {
						return hasMatch;
					};
					static bool eval(uint32_t expectedMatches, uint32_t totalMatches) {
						(void)expectedMatches;
						return totalMatches > 0;
					}
					static uint32_t
					handle_last_match(MatchingCtx& ctx, EntityLookupKey entityKey, const ArchetypeDArray* pSrcArchetype) {
						return handle_last_archetype_match(ctx.pLastMatchedArchetypeIdx_Any, entityKey, pSrcArchetype);
					}
				};
				// Operator NOT (used by query::no)
				struct OpNo {
					static bool can_continue(bool hasMatch) {
						return !hasMatch;
					};
					static bool eval(uint32_t expectedMatches, uint32_t totalMatches) {
						(void)expectedMatches;
						return totalMatches == 0;
					}
					static uint32_t
					handle_last_match(MatchingCtx& ctx, EntityLookupKey entityKey, const ArchetypeDArray* pSrcArchetype) {
						return handle_last_archetype_match(ctx.pLastMatchedArchetypeIdx_Not, entityKey, pSrcArchetype);
					}
				};

				template <typename Op>
				inline bool match_inter_eval_matches(uint32_t queryIdMarches, uint32_t& outMatches) {
					const bool hadAnyMatches = queryIdMarches > 0;

					// We finished checking matches with an id from query.
					// We need to check if we have sufficient amount of results in the run.
					if (!Op::can_continue(hadAnyMatches))
						return false;

					// No matter the amount of matches we only care if at least one
					// match happened with the id from query.
					outMatches += (uint32_t)hadAnyMatches;
					return true;
				}

				//! Tries to match ids in \param queryIds with ids in \param archetypeIds given
				//! the comparison function \param func bool(Entity queryId, Entity archetypeId).
				//! \return True if there is a match, false otherwise.
				template <typename Op, typename CmpFunc>
				inline GAIA_NODISCARD bool match_inter(EntitySpan queryIds, EntitySpan archetypeIds, CmpFunc func) {
					const auto archetypeIdsCnt = (uint32_t)archetypeIds.size();
					const auto queryIdsCnt = (uint32_t)queryIds.size();

					// Arrays are sorted so we can do linear intersection lookup
					uint32_t indices[2]{}; // 0 for query ids, 1 for archetype ids
					uint32_t matches = 0;

					// Ids in query and archetype are sorted.
					// Therefore, to match any two ids we perform a linear intersection forward loop.
					// The only exception are transitive ids in which case we need to start searching
					// form the start.
					// Finding just one match for any id in the query is enough to start checking
					// the next it. We only have 3 different operations - ALL, OR, NOT.
					//
					// Example:
					// - query #1 ------------------------
					// queryIds    : 5, 10
					// archetypeIds: 1,  3,  5,  6,  7, 10
					// - query #2 ------------------------
					// queryIds    : 1, 10, 11
					// archetypeIds: 3,  5,  6,  7, 10, 15
					// -----------------------------------
					// indices[0]  : 0,  1,  2
					// indices[1]  : 0,  1,  2,  3,  4,  5
					//
					// For query #1:
					// We start matching 5 in the query with 1 in the archetype. They do not match.
					// We continue with 3 in the archetype. No match.
					// We continue with 5 in the archetype. Match.
					// We try to match 10 in the query with 6 in the archetype. No match.
					// ... etc.

					while (indices[0] < queryIdsCnt) {
						const auto idInQuery = queryIds[indices[0]];

						// For * and transitive ids we have to search from the start.
						if (idInQuery == All || idInQuery.id() == Is.id())
							indices[1] = 0;

						uint32_t queryIdMatches = 0;
						while (indices[1] < archetypeIdsCnt) {
							const auto idInArchetype = archetypeIds[indices[1]];

							// See if we have a match
							const auto res = func(idInQuery, idInArchetype);

							// Once a match is found we start matching with the next id in query.
							if (res.matched) {
								++indices[0];
								++indices[1];
								++queryIdMatches;

								// Only continue with the next iteration unless the given Op determines it is
								// no longer needed.
								if (!match_inter_eval_matches<Op>(queryIdMatches, matches))
									return false;

								goto next_query_id;
							} else {
								++indices[1];
							}
						}

						if (!match_inter_eval_matches<Op>(queryIdMatches, matches))
							return false;

						++indices[0];

					next_query_id:
						continue;
					}

					return Op::eval(queryIdsCnt, matches);
				}

				struct IdCmpResult {
					bool matched;
				};

				inline GAIA_NODISCARD IdCmpResult cmp_ids(Entity idInQuery, Entity idInArchetype) {
					return {idInQuery == idInArchetype};
				}

				inline GAIA_NODISCARD IdCmpResult cmp_ids_pairs(Entity idInQuery, Entity idInArchetype) {
					if (idInQuery.pair()) {
						// all(Pair<All, All>) aka "any pair"
						if (idInQuery == Pair(All, All))
							return {true};

						// all(Pair<X, All>):
						//   X, AAA
						//   X, BBB
						//   ...
						//   X, ZZZ
						if (idInQuery.gen() == All.id())
							return {idInQuery.id() == idInArchetype.id()};

						// all(Pair<All, X>):
						//   AAA, X
						//   BBB, X
						//   ...
						//   ZZZ, X
						if (idInQuery.id() == All.id())
							return {idInQuery.gen() == idInArchetype.gen()};
					}

					// 1:1 match needed for non-pairs
					return cmp_ids(idInQuery, idInArchetype);
				}

				inline GAIA_NODISCARD IdCmpResult
				cmp_ids_is(const World& w, const Archetype& archetype, Entity idInQuery, Entity idInArchetype) {
					// all(Pair<Is, X>)
					if (idInQuery.pair() && idInQuery.id() == Is.id()) {
						auto archetypeIds = archetype.ids_view();
						return {
								idInQuery.gen() == idInArchetype.id() || // X vs Id
								as_relations_trav_if(w, idInQuery, [&](Entity relation) {
									const auto idx = core::get_index(archetypeIds, relation);
									// Stop at the first match
									return idx != BadIndex;
								})};
					}

					// 1:1 match needed for non-pairs
					return cmp_ids(idInQuery, idInArchetype);
				}

				inline GAIA_NODISCARD IdCmpResult
				cmp_ids_is_pairs(const World& w, const Archetype& archetype, Entity idInQuery, Entity idInArchetype) {
					if (idInQuery.pair()) {
						// all(Pair<All, All>) aka "any pair"
						if (idInQuery == Pair(All, All))
							return {true};

						// all(Pair<Is, X>)
						if (idInQuery.id() == Is.id()) {
							// (Is, X) in archetype == (Is, X) in query
							if (idInArchetype == idInQuery)
								return {true};

							auto archetypeIds = archetype.ids_view();
							const auto eQ = entity_from_id(w, idInQuery.gen());
							if (eQ == idInArchetype)
								return {true};

							// If the archetype entity is an (Is, X) pair treat Is as X and try matching it with
							// entities inheriting from e.
							if (idInArchetype.id() == Is.id()) {
								const auto eA = entity_from_id(w, idInArchetype.gen());
								if (eA == eQ)
									return {true};

								return {as_relations_trav_if(w, eQ, [eA](Entity relation) {
									return eA == relation;
								})};
							}

							// Archetype entity is generic, try matching it with entities inheriting from e.
							return {as_relations_trav_if(w, eQ, [&archetypeIds](Entity relation) {
								// Relation does not necessary match the sorted order of components in the archetype
								// so we need to search through all of its ids.
								const auto idx = core::get_index(archetypeIds, relation);
								// Stop at the first match
								return idx != BadIndex;
							})};
						}

						// all(Pair<All, X>):
						//   AAA, X
						//   BBB, X
						//   ...
						//   ZZZ, X
						if (idInQuery.id() == All.id()) {
							if (idInQuery.gen() == idInArchetype.gen())
								return {true};

							// If there are any Is pairs on the archetype we need to check if we match them
							if (archetype.pairs_is() > 0) {
								auto archetypeIds = archetype.ids_view();

								const auto e = entity_from_id(w, idInQuery.gen());
								return {as_relations_trav_if(w, e, [&](Entity relation) {
									// Relation does not necessary match the sorted order of components in the archetype
									// so we need to search through all of its ids.
									const auto idx = core::get_index(archetypeIds, relation);
									// Stop at the first match
									return idx != BadIndex;
								})};
							}

							// No match found
							return {false};
						}

						// all(Pair<X, All>):
						//   X, AAA
						//   X, BBB
						//   ...
						//   X, ZZZ
						if (idInQuery.gen() == All.id()) {
							return {idInQuery.id() == idInArchetype.id()};
						}
					}

					// 1:1 match needed for non-pairs
					return cmp_ids(idInQuery, idInArchetype);
				}

				//! Tries to match entity ids in \param queryIds with those in \param archetype given
				//! the comparison function \param func. Does not consider Is relationships.
				//! \return True on the first match, false otherwise.
				template <typename Op>
				inline GAIA_NODISCARD bool match_res(const Archetype& archetype, EntitySpan queryIds) {
					// Archetype has no pairs we can compare ids directly.
					// This has better performance.
					if (archetype.pairs() == 0) {
						return match_inter<Op>(
								queryIds, archetype.ids_view(),
								// Cmp func
								[](Entity idInQuery, Entity idInArchetype) {
									return cmp_ids(idInQuery, idInArchetype);
								});
					}

					// Pairs are present, we have to evaluate.
					return match_inter<Op>(
							queryIds, archetype.ids_view(),
							// Cmp func
							[](Entity idInQuery, Entity idInArchetype) {
								return cmp_ids_pairs(idInQuery, idInArchetype);
							});
				}

				//! Tries to match entity ids in \param queryIds with those in \param archetype given
				//! the comparison function \param func. Considers Is relationships.
				//! \return True on the first match, false otherwise.
				template <typename Op>
				inline GAIA_NODISCARD bool match_res_as(const World& w, const Archetype& archetype, EntitySpan queryIds) {
					// Archetype has no pairs we can compare ids directly
					if (archetype.pairs() == 0) {
						return match_inter<Op>(
								queryIds, archetype.ids_view(),
								// cmp func
								[&](Entity idInQuery, Entity idInArchetype) {
									return cmp_ids_is(w, archetype, idInQuery, idInArchetype);
								});
					}

					return match_inter<Op>(
							queryIds, archetype.ids_view(),
							// cmp func
							[&](Entity idInQuery, Entity idInArchetype) {
								return cmp_ids_is_pairs(w, archetype, idInQuery, idInArchetype);
							});
				}

				template <typename OpKind, bool CheckAs>
				inline void
				match_archetype_inter(MatchingCtx& ctx, EntityLookupKey entityKey, const ArchetypeDArray* pSrcArchetypes) {
					const auto& archetypes = *pSrcArchetypes;
					auto& matchesArr = *ctx.pMatchesArr;
					auto& matchesSet = *ctx.pMatchesSet;

					uint32_t lastMatchedIdx = OpKind::handle_last_match(ctx, entityKey, pSrcArchetypes);
					if (lastMatchedIdx >= archetypes.size())
						return;

					auto archetypeSpan = std::span(&archetypes[lastMatchedIdx], archetypes.size() - lastMatchedIdx);
					for (auto* pArchetype: archetypeSpan) {
						if (matchesSet.contains(pArchetype))
							continue;

						if constexpr (CheckAs) {
							if (!match_res_as<OpKind>(*ctx.pWorld, *pArchetype, ctx.idsToMatch))
								continue;
						} else {
							if (!match_res<OpKind>(*pArchetype, ctx.idsToMatch))
								continue;
						};

						matchesSet.emplace(pArchetype);
						matchesArr.emplace_back(pArchetype);
					}
				}

				inline void match_archetype_all(MatchingCtx& ctx) {
					EntityLookupKey entityKey(ctx.ent);

					// For ALL we need all the archetypes to match. We start by checking
					// if the first one is registered in the world at all.
					const auto* pArchetypes = fetch_archetypes_for_select(*ctx.pEntityToArchetypeMap, entityKey);
					if (pArchetypes == nullptr)
						return;

					match_archetype_inter<OpAll, false>(ctx, entityKey, pArchetypes);
				}

				inline void match_archetype_all_as(MatchingCtx& ctx) {
					const auto& allArchetypes = *ctx.pAllArchetypes;

					// For ALL we need all the archetypes to match. We start by checking
					// if the first one is registered in the world at all.
					const ArchetypeDArray* pSrcArchetypes = nullptr;

					EntityLookupKey entityKey = EntityBadLookupKey;

					if (ctx.ent.id() == Is.id()) {
						ctx.ent = EntityBad;
						pSrcArchetypes = &allArchetypes;
					} else {
						entityKey = EntityLookupKey(ctx.ent);

						pSrcArchetypes = fetch_archetypes_for_select(*ctx.pEntityToArchetypeMap, entityKey);
						if (pSrcArchetypes == nullptr)
							return;
					}

					match_archetype_inter<OpAll, true>(ctx, entityKey, pSrcArchetypes);
				}

				inline void match_archetype_one(MatchingCtx& ctx) {
					EntityLookupKey entityKey(ctx.ent);

					// For ANY we need at least one archetype to match.
					// However, because any of them can match, we need to check them all.
					// Iterating all of them is caller's responsibility.
					const auto* pArchetypes = fetch_archetypes_for_select(*ctx.pEntityToArchetypeMap, entityKey);
					if (pArchetypes == nullptr)
						return;

					match_archetype_inter<OpAny, false>(ctx, entityKey, pArchetypes);
				}

				inline void match_archetype_one_as(MatchingCtx& ctx) {
					const auto& allArchetypes = *ctx.pAllArchetypes;

					// For ANY we need at least one archetype to match.
					// However, because any of them can match, we need to check them all.
					// Iterating all of them is caller's responsibility.

					const ArchetypeDArray* pSrcArchetypes = nullptr;

					EntityLookupKey entityKey = EntityBadLookupKey;

					if (ctx.ent.id() == Is.id()) {
						ctx.ent = EntityBad;
						pSrcArchetypes = &allArchetypes;
					} else {
						entityKey = EntityLookupKey(ctx.ent);

						pSrcArchetypes = fetch_archetypes_for_select(*ctx.pEntityToArchetypeMap, entityKey);
						if (pSrcArchetypes == nullptr)
							return;
					}

					match_archetype_inter<OpAny, true>(ctx, entityKey, pSrcArchetypes);
				}

				inline void match_archetype_no(MatchingCtx& ctx) {
					ctx.pMatchesSet->clear();

					match_archetype_inter<OpNo, false>(ctx, EntityBadLookupKey, ctx.pAllArchetypes);
				}

				inline void match_archetype_no_as(MatchingCtx& ctx) {
					ctx.pMatchesSet->clear();

					match_archetype_inter<OpNo, true>(ctx, EntityBadLookupKey, ctx.pAllArchetypes);
				}

				inline void match_archetype_no_2(MatchingCtx& ctx) {
					// We had some matches already (with ALL or ANY). We need to remove those
					// that match with the NO list.
					for (uint32_t i = 0; i < ctx.pMatchesArr->size();) {
						auto* pArchetype = (*ctx.pMatchesArr)[i];
						if (match_res<OpNo>(*pArchetype, ctx.idsToMatch)) {
							++i;
							continue;
						}

						core::swap_erase(*ctx.pMatchesArr, i);
					}
				}

				inline void match_archetype_no_as_2(MatchingCtx& ctx) {
					// We had some matches already (with ALL or ANY). We need to remove those
					// that match with the NO list.
					for (uint32_t i = 0; i < ctx.pMatchesArr->size();) {
						auto* pArchetype = (*ctx.pMatchesArr)[i];
						if (match_res_as<OpNo>(*ctx.pWorld, *pArchetype, ctx.idsToMatch)) {
							++i;
							continue;
						}

						core::swap_erase(*ctx.pMatchesArr, i);
					}
				}

				struct OpcodeBaseData {
					EOpcode id;
				};

				struct OpcodeAll {
					static constexpr EOpcode Id = EOpcode::All;

					bool exec(const QueryCompileCtx& comp, MatchingCtx& ctx) {
						GAIA_PROF_SCOPE(vm::op_and);

						ctx.ent = comp.ids_all[0];
						ctx.idsToMatch = std::span{comp.ids_all.data(), comp.ids_all.size()};

						// First viable item is not related to an Is relationship
						if (ctx.as_mask_0 + ctx.as_mask_1 == 0U) {
							match_archetype_all(ctx);
						}
						// First viable item is related to an Is relationship.
						// In this case we need to gather all related archetypes.
						else {
							match_archetype_all_as(ctx);
						}

						// If no ALL matches were found, we can quit right away.
						return !ctx.pMatchesArr->empty();
					}
				};

				struct OpcodeAny_NoAll {
					static constexpr EOpcode Id = EOpcode::Any_NoAll;

					bool exec(const QueryCompileCtx& comp, MatchingCtx& ctx) {
						GAIA_PROF_SCOPE(vm::op_any);

						ctx.idsToMatch = std::span{comp.ids_any.data(), comp.ids_any.size()};

						// Try find matches with optional components.
						GAIA_EACH(comp.ids_any) {
							ctx.ent = comp.ids_any[i];

							// First viable item is not related to an Is relationship
							if (ctx.as_mask_0 + ctx.as_mask_1 == 0U) {
								match_archetype_one(ctx);
							}
							// First viable item is related to an Is relationship.
							// In this case we need to gather all related archetypes.
							else {
								match_archetype_one_as(ctx);
							}
						}

						return true;
					}
				};

				struct OpcodeAny_WithAll {
					static constexpr EOpcode Id = EOpcode::Any_WithAll;

					bool exec(const QueryCompileCtx& comp, MatchingCtx& ctx) {
						GAIA_PROF_SCOPE(vm::op_any);

						ctx.idsToMatch = std::span{comp.ids_any.data(), comp.ids_any.size()};

						// We tried to match ALL items. Only search among archetypes we already found.
						// No last matched idx update is necessary because all ids were already checked
						// during the ALL pass.
						for (uint32_t i = 0; i < ctx.pMatchesArr->size();) {
							auto* pArchetype = (*ctx.pMatchesArr)[i];

							if (ctx.as_mask_0 + ctx.as_mask_1 == 0U) {
								GAIA_FOR_((uint32_t)comp.ids_any.size(), j) {
									// First viable item is not related to an Is relationship
									if (match_res<OpAny>(*pArchetype, ctx.idsToMatch))
										goto checkNextArchetype;

									// First viable item is related to an Is relationship.
									// In this case we need to gather all related archetypes.
									if (match_res_as<OpAny>(*ctx.pWorld, *pArchetype, ctx.idsToMatch))
										goto checkNextArchetype;
								}
							} else {
								GAIA_FOR_((uint32_t)comp.ids_any.size(), j) {
									// First viable item is related to an Is relationship.
									// In this case we need to gather all related archetypes.
									if (match_res_as<OpAny>(*ctx.pWorld, *pArchetype, ctx.idsToMatch))
										goto checkNextArchetype;
								}
							}

							// No match found among ANY. Remove the archetype from the matching ones
							core::swap_erase(*ctx.pMatchesArr, i);
							continue;

						checkNextArchetype:
							++i;
						}

						return true;
					}
				};

				struct OpcodeNot {
					static constexpr EOpcode Id = EOpcode::Not;

					bool exec(const QueryCompileCtx& comp, MatchingCtx& ctx) {
						GAIA_PROF_SCOPE(vm::op_not);

						ctx.idsToMatch = std::span{comp.ids_not.data(), comp.ids_not.size()};

						// We searched for nothing more than NOT matches
						if (ctx.pMatchesArr->empty()) {
							// If there are no previous matches (no ALL or ANY matches),
							// we need to search among all archetypes.
							if (ctx.as_mask_0 + ctx.as_mask_1 == 0U)
								match_archetype_no(ctx);
							else
								match_archetype_no_as(ctx);
						} else {
							if (ctx.as_mask_0 + ctx.as_mask_1 == 0U)
								match_archetype_no_2(ctx);
							else
								match_archetype_no_as_2(ctx);
						}

						return true;
					}
				};
			} // namespace detail

			class VirtualMachine {
				using OpcodeFunc = bool (*)(const detail::QueryCompileCtx& comp, MatchingCtx& ctx);
				struct Opcodes {
					OpcodeFunc exec;
				};

				static constexpr OpcodeFunc OpcodeFuncs[] = {
						// OpcodeAll
						[](const detail::QueryCompileCtx& comp, MatchingCtx& ctx) {
							detail::OpcodeAll op;
							return op.exec(comp, ctx);
						},
						// OpcodeAny_NoAll
						[](const detail::QueryCompileCtx& comp, MatchingCtx& ctx) {
							detail::OpcodeAny_NoAll op;
							return op.exec(comp, ctx);
						},
						// OpcodeAny_WithAll
						[](const detail::QueryCompileCtx& comp, MatchingCtx& ctx) {
							detail::OpcodeAny_WithAll op;
							return op.exec(comp, ctx);
						},
						// OpcodeNot
						[](const detail::QueryCompileCtx& comp, MatchingCtx& ctx) {
							detail::OpcodeNot op;
							return op.exec(comp, ctx);
						}};

				detail::QueryCompileCtx m_compCtx;

			private:
				GAIA_NODISCARD detail::VmLabel add_op(detail::CompiledOp&& op) {
					const auto cnt = (detail::VmLabel)m_compCtx.ops.size();
					op.pc_ok = cnt + 1;
					op.pc_fail = cnt - 1;
					m_compCtx.ops.push_back(GAIA_MOV(op));
					return cnt;
				}

			public:
				//! Transforms inputs into virtual machine opcodes.
				void compile(const EntityToArchetypeMap& entityToArchetypeMap, const QueryCtx& queryCtx) {
					GAIA_PROF_SCOPE(vm::compile);

					const auto& data = queryCtx.data;

					QueryTermSpan terms{data.terms.data(), data.terms.size()};
					QueryTermSpan terms_all = terms.subspan(0, data.firstAny);
					QueryTermSpan terms_any = terms.subspan(data.firstAny, data.firstNot - data.firstAny);
					QueryTermSpan terms_not = terms.subspan(data.firstNot);

					// ALL
					if (!terms_all.empty()) {
						GAIA_PROF_SCOPE(vm::compile_all);

						GAIA_EACH(terms_all) {
							auto& p = terms_all[i];
							if (p.src == EntityBad) {
								m_compCtx.ids_all.push_back(p.id);
								continue;
							}

							// Fixed source
							{
								p.srcArchetype = archetype_from_entity(*queryCtx.w, p.src);

								// Archetype needs to exist. If it does not we have nothing to do here.
								if (p.srcArchetype == nullptr) {
									m_compCtx.ops.clear();
									return;
								}
							}
						}
					}

					// ANY
					if (!terms_any.empty()) {
						GAIA_PROF_SCOPE(vm::compile_any);

						cnt::sarr_ext<const ArchetypeDArray*, MAX_ITEMS_IN_QUERY> archetypesWithId;
						GAIA_EACH(terms_any) {
							auto& p = terms_any[i];
							if (p.src != EntityBad) {
								p.srcArchetype = archetype_from_entity(*queryCtx.w, p.src);
								if (p.srcArchetype == nullptr)
									continue;
							}

							// Check if any archetype is associated with the entity id.
							// All ids must be registered in the world.
							const auto* pArchetypes = fetch_archetypes_for_select(entityToArchetypeMap, EntityLookupKey(p.id));
							if (pArchetypes == nullptr)
								continue;

							archetypesWithId.push_back(pArchetypes);

							m_compCtx.ids_any.push_back(p.id);
						}

						// No archetypes with "any" entities exist. We can quit right away.
						if (archetypesWithId.empty()) {
							m_compCtx.ops.clear();
							return;
						}
					}

					// NOT
					if (!terms_not.empty()) {
						GAIA_PROF_SCOPE(vm::compile_not);

						GAIA_EACH(terms_not) {
							auto& p = terms_not[i];
							if (p.src != EntityBad)
								continue;

							m_compCtx.ids_not.push_back(p.id);
						}
					}

					if (!m_compCtx.ids_all.empty()) {
						detail::CompiledOp op{};
						op.opcode = detail::EOpcode::All;
						add_op(GAIA_MOV(op));
					}
					if (!m_compCtx.ids_any.empty()) {
						detail::CompiledOp op{};
						op.opcode = m_compCtx.ids_all.empty() ? detail::EOpcode::Any_NoAll : detail::EOpcode::Any_WithAll;
						add_op(GAIA_MOV(op));
					}
					if (!m_compCtx.ids_not.empty()) {
						detail::CompiledOp op{};
						op.opcode = detail::EOpcode::Not;
						add_op(GAIA_MOV(op));
					}
				}

				GAIA_NODISCARD bool is_compiled() const {
					return !m_compCtx.ops.empty();
				}

				//! Executes compiled opcodes
				void exec(MatchingCtx& ctx) {
					GAIA_PROF_SCOPE(vm::exec);

					ctx.pc = 0;

					// Extract data from the buffer
					do {
						auto& stackItem = m_compCtx.ops[ctx.pc];
						const bool ret = OpcodeFuncs[(uint32_t)stackItem.opcode](m_compCtx, ctx);
						ctx.pc = ret ? stackItem.pc_ok : stackItem.pc_fail;
					} while (ctx.pc < m_compCtx.ops.size()); // (uint32_t)-1 falls in this category as well
				}
			};

		} // namespace vm
	} // namespace ecs
} // namespace gaia
/*** End of inlined file: vm.h ***/

namespace gaia {
	namespace ecs {
		struct Entity;
		class World;

		bool is_base(const World& world, Entity entity);

		using EntityToArchetypeMap = cnt::map<EntityLookupKey, ArchetypeDArray>;
		struct ArchetypeCacheData {
			GroupId groupId = 0;
			uint8_t indices[ChunkHeader::MAX_COMPONENTS];
		};

		struct QueryInfoCreationCtx {
			QueryCtx* pQueryCtx;
			const EntityToArchetypeMap* pEntityToArchetypeMap;
		};

		class QueryInfo: public cnt::ilist_item {
		public:
			//! Query matching result
			enum class MatchArchetypeQueryRet : uint8_t { Fail, Ok, Skip };

		private:
			struct Instruction {
				Entity id;
				QueryOpKind op;
			};

			struct GroupData {
				GroupId groupId;
				uint32_t idxFirst;
				uint32_t idxLast;
				bool needsSorting;
			};

			uint32_t m_refs = 0;

			//! Query context
			QueryCtx m_ctx;
			//! Virtual machine
			vm::VirtualMachine m_vm;

			//! Use to make sure only unique archetypes are inserted into the cache
			//! TODO: Get rid of the set by changing the way the caching works.
			cnt::set<Archetype*> m_archetypeSet;
			//! Cached array of archetypes matching the query
			ArchetypeDArray m_archetypeCache;
			//! Cached array of query-specific data
			cnt::darray<ArchetypeCacheData> m_archetypeCacheData;
			//! Group data used by cache
			cnt::darray<GroupData> m_archetypeGroupData;

			//! Id of the last archetype in the world we checked
			ArchetypeId m_lastArchetypeId{};
			//! Version of the world for which the query has been called most recently
			uint32_t m_worldVersion{};

			enum QueryCmdType : uint8_t { ALL, ANY, NOT };

			template <typename TType>
			GAIA_NODISCARD bool has_inter([[maybe_unused]] QueryOpKind op, bool isReadWrite) const {
				using T = core::raw_t<TType>;

				if constexpr (std::is_same_v<T, Entity>) {
					// Entities are read-only.
					GAIA_ASSERT(!isReadWrite);
					// Skip Entity input args. Entities are always there.
					return true;
				} else {
					Entity id;

					if constexpr (is_pair<T>::value) {
						const auto rel = m_ctx.cc->get<typename T::rel>().entity;
						const auto tgt = m_ctx.cc->get<typename T::tgt>().entity;
						id = (Entity)Pair(rel, tgt);
					} else {
						id = m_ctx.cc->get<T>().entity;
					}

					const auto& data = m_ctx.data;
					const auto& terms = data.terms;
					const auto compIdx = comp_idx<MAX_ITEMS_IN_QUERY>(terms.data(), id, EntityBad);

					if (op != data.terms[compIdx].op)
						return false;

					// Read-write mask must match
					const uint32_t maskRW = (uint32_t)data.readWriteMask & (1U << compIdx);
					const uint32_t maskXX = (uint32_t)isReadWrite << compIdx;
					return maskRW == maskXX;
				}
			}

			template <typename T>
			GAIA_NODISCARD bool has_inter(QueryOpKind op) const {
				// static_assert(is_raw_v<<T>, "has() must be used with raw types");
				constexpr bool isReadWrite = core::is_mut_v<T>;
				return has_inter<T>(op, isReadWrite);
			}

		public:
			void add_ref() {
				++m_refs;
				GAIA_ASSERT(m_refs != 0);
			}

			void dec_ref() {
				GAIA_ASSERT(m_refs > 0);
				--m_refs;
			}

			uint32_t refs() const {
				return m_refs;
			}

			void init(World* world) {
				m_ctx.w = world;
			}

			void reset() {
				m_archetypeSet = {};
				m_archetypeCache = {};
				m_archetypeCacheData = {};
				m_archetypeCacheData = {};
				m_lastArchetypeId = 0;

				m_ctx.data.lastMatchedArchetypeIdx_All = {};
				m_ctx.data.lastMatchedArchetypeIdx_Any = {};
				m_ctx.data.lastMatchedArchetypeIdx_Not = {};
			}

			GAIA_NODISCARD static QueryInfo
			create(QueryId id, QueryCtx&& ctx, const EntityToArchetypeMap& entityToArchetypeMap) {
				// Make sure query items are sorted
				sort(ctx);

				QueryInfo info;
				info.idx = id;
				info.gen = 0;

				info.m_ctx = GAIA_MOV(ctx);
				info.m_ctx.q.handle = {id, 0};

				// Compile the query
				info.compile(entityToArchetypeMap);

				return info;
			}

			GAIA_NODISCARD static QueryInfo create(uint32_t idx, uint32_t gen, void* pCtx) {
				auto* pCreationCtx = (QueryInfoCreationCtx*)pCtx;
				auto& queryCtx = (QueryCtx&)*pCreationCtx->pQueryCtx;
				auto& entityToArchetypeMap = (EntityToArchetypeMap&)*pCreationCtx->pEntityToArchetypeMap;

				// Make sure query items are sorted
				sort(queryCtx);

				QueryInfo info;
				info.idx = idx;
				info.gen = gen;

				info.m_ctx = GAIA_MOV(queryCtx);
				info.m_ctx.q.handle = {idx, gen};

				// Compile the query
				info.compile(entityToArchetypeMap);

				return info;
			}

			GAIA_NODISCARD static QueryHandle handle(const QueryInfo& info) {
				return QueryHandle(info.idx, info.gen);
			}

			//! Compile the query terms into a form we can easily process
			void compile(const EntityToArchetypeMap& entityToArchetypeMap) {
				GAIA_PROF_SCOPE(queryinfo::compile);

				// Compile the opcodes
				m_vm.compile(entityToArchetypeMap, m_ctx);
			}

			void set_world_version(uint32_t version) {
				m_worldVersion = version;
			}

			GAIA_NODISCARD uint32_t world_version() const {
				return m_worldVersion;
			}

			GAIA_NODISCARD bool operator==(const QueryCtx& other) const {
				return m_ctx == other;
			}

			GAIA_NODISCARD bool operator!=(const QueryCtx& other) const {
				return m_ctx != other;
			}

			void refresh_ctx() {
				auto& data = m_ctx.data;

				// Update masks
				{
					uint32_t as_mask_0 = 0;
					uint32_t as_mask_1 = 0;

					const auto& ids = data.ids;
					GAIA_EACH(ids) {
						const auto id = ids[i];

						// Build the Is mask.
						// We will use it to identify entities with an Is relationship quickly.
						if (!id.pair()) {
							const auto j = i; // data.remapping[i];
							const auto has_as = (uint8_t)is_base(*m_ctx.w, id);
							as_mask_0 |= (has_as << (uint8_t)j);
						} else {
							if (!is_wildcard(id.id())) {
								const auto j = i; // data.remapping[i];
								const auto e = entity_from_id(*m_ctx.w, id.id());
								const auto has_as = (uint8_t)is_base(*m_ctx.w, e);
								as_mask_0 |= (has_as << (uint8_t)j);
							}

							if (!is_wildcard(id.gen())) {
								const auto j = i; // data.remapping[i];
								const auto e = entity_from_id(*m_ctx.w, id.gen());
								const auto has_as = (uint8_t)is_base(*m_ctx.w, e);
								as_mask_1 |= (has_as << (uint8_t)j);
							}
						}
					}

					// Update the mask
					data.as_mask_0 = as_mask_0;
					data.as_mask_1 = as_mask_1;
				}
			}

			//! Tries to match the query against archetypes in \param entityToArchetypeMap.
			//! This is necessary so we do not iterate all chunks over and over again when running queries.
			//! \warning Not thread safe. No two threads can call this at the same time.
			void match(
					// entity -> archetypes mapping
					const EntityToArchetypeMap& entityToArchetypeMap,
					// all archetypes in the world
					const ArchetypeDArray& allArchetypes,
					// last matched archetype id
					ArchetypeId archetypeLastId) {

				// Global temporary buffers for collecting archetypes that match a query.
				static cnt::set<Archetype*> s_tmpArchetypeMatchesSet;
				static ArchetypeDArray s_tmpArchetypeMatchesArr;

				struct CleanUpTmpArchetypeMatches {
					CleanUpTmpArchetypeMatches() = default;
					CleanUpTmpArchetypeMatches(const CleanUpTmpArchetypeMatches&) = delete;
					CleanUpTmpArchetypeMatches(CleanUpTmpArchetypeMatches&&) = delete;
					CleanUpTmpArchetypeMatches& operator=(const CleanUpTmpArchetypeMatches&) = delete;
					CleanUpTmpArchetypeMatches& operator=(CleanUpTmpArchetypeMatches&&) = delete;

					~CleanUpTmpArchetypeMatches() {
						// When the scope ends, we clear the arrays.
						// Note, no memory is released. Allocated capacity remains unchanged
						// because we do not want to kill time with allocating memory all the time.
						s_tmpArchetypeMatchesSet.clear();
						s_tmpArchetypeMatchesArr.clear();
					}
				} autoCleanup;

				// Skip if nothing has been compiled.
				if (!m_vm.is_compiled())
					return;

				// Skip if no new archetype appeared
				GAIA_ASSERT(archetypeLastId >= m_lastArchetypeId);
				if (m_lastArchetypeId == archetypeLastId)
					return;
				m_lastArchetypeId = archetypeLastId;

				GAIA_PROF_SCOPE(queryinfo::match);

				auto& data = m_ctx.data;

				// Prepare the context
				vm::MatchingCtx ctx{};
				ctx.pWorld = world();
				ctx.pAllArchetypes = &allArchetypes;
				ctx.pEntityToArchetypeMap = &entityToArchetypeMap;
				ctx.pMatchesArr = &s_tmpArchetypeMatchesArr;
				ctx.pMatchesSet = &s_tmpArchetypeMatchesSet;
				ctx.pLastMatchedArchetypeIdx_All = &data.lastMatchedArchetypeIdx_All;
				ctx.pLastMatchedArchetypeIdx_Any = &data.lastMatchedArchetypeIdx_Any;
				ctx.pLastMatchedArchetypeIdx_Not = &data.lastMatchedArchetypeIdx_Not;
				ctx.as_mask_0 = data.as_mask_0;
				ctx.as_mask_1 = data.as_mask_1;

				// Run the virtual machine
				m_vm.exec(ctx);

				// Write found matches to cache
				for (auto* pArchetype: *ctx.pMatchesArr)
					add_archetype_to_cache(pArchetype);

				// Sort cache groups if necessary
				sort_cache_groups();
			}

			void sort_cache_groups() {
				if ((m_ctx.data.flags & QueryCtx::QueryFlags::SortGroups) == 0)
					return;
				m_ctx.data.flags ^= QueryCtx::QueryFlags::SortGroups;

				struct sort_cond {
					bool operator()(const ArchetypeCacheData& a, const ArchetypeCacheData& b) const {
						return a.groupId <= b.groupId;
					}
				};

				// Archetypes in cache are ordered by groupId. Adding a new archetype
				// possibly means rearranging the existing ones.
				// 2 2 3 3 3 3 4 4 4 [2]
				// -->
				// 2 2 [2] 3 3 3 3 4 4 4
				core::sort(m_archetypeCacheData, sort_cond{}, [&](uint32_t left, uint32_t right) {
					auto* pTmpArchetype = m_archetypeCache[left];
					m_archetypeCache[left] = m_archetypeCache[right];
					m_archetypeCache[right] = pTmpArchetype;

					auto tmp = m_archetypeCacheData[left];
					m_archetypeCacheData[left] = m_archetypeCacheData[right];
					m_archetypeCacheData[right] = tmp;
				});
			}

			ArchetypeCacheData create_cache_data(Archetype* pArchetype) {
				ArchetypeCacheData cacheData;
				const auto& queryIds = ids();
				GAIA_EACH(queryIds) {
					const auto idxBeforeRemapping = m_ctx.data.remapping[i];
					const auto queryId = queryIds[idxBeforeRemapping];
					// compIdx can be -1. We are fine with it because the user should never ask for something
					// that is not present on the archetype. If they do, they made a mistake.
					const auto compIdx = core::get_index_unsafe(pArchetype->ids_view(), queryId);
					GAIA_ASSERT(compIdx != BadIndex);

					cacheData.indices[i] = (uint8_t)compIdx;
				}
				return cacheData;
			}

			void add_archetype_to_cache_no_grouping(Archetype* pArchetype) {
				GAIA_PROF_SCOPE(add_cache_ng);

				if (m_archetypeSet.contains(pArchetype))
					return;

				m_archetypeSet.emplace(pArchetype);
				m_archetypeCache.push_back(pArchetype);
				m_archetypeCacheData.push_back(create_cache_data(pArchetype));
			}

			void add_archetype_to_cache_w_grouping(Archetype* pArchetype) {
				GAIA_PROF_SCOPE(add_cache_wg);

				if (m_archetypeSet.contains(pArchetype))
					return;

				const GroupId groupId = m_ctx.data.groupByFunc(*m_ctx.w, *pArchetype, m_ctx.data.groupBy);

				ArchetypeCacheData cacheData = create_cache_data(pArchetype);
				cacheData.groupId = groupId;

				if (m_archetypeGroupData.empty()) {
					m_archetypeGroupData.push_back({groupId, 0, 0, false});
				} else {
					GAIA_EACH(m_archetypeGroupData) {
						if (groupId < m_archetypeGroupData[i].groupId) {
							// Insert the new group before one with a lower groupId.
							// 2 3 5 10 20 25 [7]<-new group
							// -->
							// 2 3 5 [7] 10 20 25
							m_archetypeGroupData.insert(
									m_archetypeGroupData.begin() + i,
									{groupId, m_archetypeGroupData[i].idxFirst, m_archetypeGroupData[i].idxFirst, false});
							const auto lastGrpIdx = m_archetypeGroupData.size();

							// Update ranges
							for (uint32_t j = i + 1; j < lastGrpIdx; ++j) {
								++m_archetypeGroupData[j].idxFirst;
								++m_archetypeGroupData[j].idxLast;
							}

							// Resort groups
							m_ctx.data.flags |= QueryCtx::QueryFlags::SortGroups;
							goto groupWasFound;
						} else if (m_archetypeGroupData[i].groupId == groupId) {
							const auto lastGrpIdx = m_archetypeGroupData.size();
							++m_archetypeGroupData[i].idxLast;

							// Update ranges
							for (uint32_t j = i + 1; j < lastGrpIdx; ++j) {
								++m_archetypeGroupData[j].idxFirst;
								++m_archetypeGroupData[j].idxLast;
								m_ctx.data.flags |= QueryCtx::QueryFlags::SortGroups;
							}

							goto groupWasFound;
						}
					}

					{
						// We have a new group
						const auto groupsCnt = m_archetypeGroupData.size();
						if (groupsCnt == 0) {
							// No groups exist yet, the range is {0 .. 0}
							m_archetypeGroupData.push_back( //
									{groupId, 0, 0, false});
						} else {
							const auto& groupPrev = m_archetypeGroupData[groupsCnt - 1];
							GAIA_ASSERT(groupPrev.idxLast + 1 == m_archetypeCache.size());
							// The new group starts where the old one ends
							m_archetypeGroupData.push_back(
									{groupId, //
									 groupPrev.idxLast + 1, //
									 groupPrev.idxLast + 1, //
									 false});
						}
					}

				groupWasFound:;
				}

				m_archetypeSet.emplace(pArchetype);
				m_archetypeCache.push_back(pArchetype);
				m_archetypeCacheData.push_back(GAIA_MOV(cacheData));
			}

			void add_archetype_to_cache(Archetype* pArchetype) {
				if (m_ctx.data.groupBy != EntityBad)
					add_archetype_to_cache_w_grouping(pArchetype);
				else
					add_archetype_to_cache_no_grouping(pArchetype);
			}

			bool del_archetype_from_cache(Archetype* pArchetype) {
				const auto it = m_archetypeSet.find(pArchetype);
				if (it == m_archetypeSet.end())
					return false;
				m_archetypeSet.erase(it);

				const auto archetypeIdx = core::get_index_unsafe(m_archetypeCache, pArchetype);
				GAIA_ASSERT(archetypeIdx != BadIndex);

				core::swap_erase(m_archetypeCache, archetypeIdx);
				core::swap_erase(m_archetypeCacheData, archetypeIdx);

				// Update the group data if possible
				if (m_ctx.data.groupBy != EntityBad) {
					const auto groupId = m_ctx.data.groupByFunc(*m_ctx.w, *pArchetype, m_ctx.data.groupBy);
					const auto grpIdx = core::get_index_if_unsafe(m_archetypeGroupData, [&](const GroupData& group) {
						return group.groupId == groupId;
					});
					GAIA_ASSERT(grpIdx != BadIndex);

					auto& currGrp = m_archetypeGroupData[archetypeIdx];

					// Update ranges
					const auto lastGrpIdx = m_archetypeGroupData.size();
					for (uint32_t j = grpIdx + 1; j < lastGrpIdx; ++j) {
						--m_archetypeGroupData[j].idxFirst;
						--m_archetypeGroupData[j].idxLast;
					}

					// Handle the current group. If it's about to be left empty, delete it.
					if (currGrp.idxLast - currGrp.idxFirst > 0)
						--currGrp.idxLast;
					else
						m_archetypeGroupData.erase(m_archetypeGroupData.begin() + grpIdx);
				}

				return true;
			}

			GAIA_NODISCARD World* world() {
				return const_cast<World*>(m_ctx.w);
			}
			GAIA_NODISCARD const World* world() const {
				return m_ctx.w;
			}

			GAIA_NODISCARD QuerySerBuffer& ser_buffer() {
				return m_ctx.q.ser_buffer(world());
			}
			void ser_buffer_reset() {
				m_ctx.q.ser_buffer_reset(world());
			}

			GAIA_NODISCARD QueryCtx& ctx() {
				return m_ctx;
			}
			GAIA_NODISCARD const QueryCtx& ctx() const {
				return m_ctx;
			}

			GAIA_NODISCARD const QueryCtx::Data& data() const {
				return m_ctx.data;
			}

			GAIA_NODISCARD const QueryEntityArray& ids() const {
				return m_ctx.data.ids;
			}

			GAIA_NODISCARD const QueryEntityArray& filters() const {
				return m_ctx.data.changed;
			}

			GAIA_NODISCARD bool has_filters() const {
				return !m_ctx.data.changed.empty();
			}

			template <typename... T>
			bool has_any() const {
				return (has_inter<T>(QueryOpKind::Any) || ...);
			}

			template <typename... T>
			bool has_all() const {
				return (has_inter<T>(QueryOpKind::All) && ...);
			}

			template <typename... T>
			bool has_no() const {
				return (!has_inter<T>(QueryOpKind::Not) && ...);
			}

			//! Removes an archetype from cache
			//! \param pArchetype Archetype to remove
			void remove(Archetype* pArchetype) {
				GAIA_PROF_SCOPE(queryinfo::remove);

				if (!del_archetype_from_cache(pArchetype))
					return;

				// An archetype was removed from the world so the last matching archetype index needs to be
				// lowered by one for every component context.
				auto clearMatches = [](QueryArchetypeCacheIndexMap& matches) {
					for (auto& pair: matches) {
						auto& lastMatchedArchetypeIdx = pair.second;
						if (lastMatchedArchetypeIdx > 0)
							--lastMatchedArchetypeIdx;
					}
				};
				clearMatches(m_ctx.data.lastMatchedArchetypeIdx_All);
				clearMatches(m_ctx.data.lastMatchedArchetypeIdx_Any);
			}

			//! Returns a view of indices mapping for component entities in a given archetype
			std::span<const uint8_t> indices_mapping_view(uint32_t archetypeIdx) const {
				const auto& data = m_archetypeCacheData[archetypeIdx];
				return {(const uint8_t*)&data.indices[0], ChunkHeader::MAX_COMPONENTS};
			}

			GAIA_NODISCARD ArchetypeDArray::iterator begin() {
				return m_archetypeCache.begin();
			}

			GAIA_NODISCARD ArchetypeDArray::iterator begin() const {
				return m_archetypeCache.begin();
			}

			GAIA_NODISCARD ArchetypeDArray::iterator end() {
				return m_archetypeCache.end();
			}

			GAIA_NODISCARD ArchetypeDArray::iterator end() const {
				return m_archetypeCache.end();
			}

			GAIA_NODISCARD std::span<Archetype*> cache_archetype_view() const {
				return std::span{m_archetypeCache.data(), m_archetypeCache.size()};
			}
			GAIA_NODISCARD std::span<const ArchetypeCacheData> cache_data_view() const {
				return std::span{m_archetypeCacheData.data(), m_archetypeCacheData.size()};
			}
			GAIA_NODISCARD std::span<const GroupData> group_data_view() const {
				return std::span{m_archetypeGroupData.data(), m_archetypeGroupData.size()};
			}
		};
	} // namespace ecs
} // namespace gaia

/*** End of inlined file: query_info.h ***/

namespace gaia {
	namespace ecs {
		class QueryLookupKey {
			QueryLookupHash m_hash;
			const QueryCtx* m_pCtx;

		public:
			static constexpr bool IsDirectHashKey = true;

			QueryLookupKey(): m_hash({0}), m_pCtx(nullptr) {}
			explicit QueryLookupKey(QueryLookupHash hash, const QueryCtx* pCtx): m_hash(hash), m_pCtx(pCtx) {}

			size_t hash() const {
				return (size_t)m_hash.hash;
			}

			bool operator==(const QueryLookupKey& other) const {
				// Hash doesn't match we don't have a match.
				// Hash collisions are expected to be very unlikely so optimize for this case.
				if GAIA_LIKELY (m_hash != other.m_hash)
					return false;

				const auto id = m_pCtx->q.handle.id();

				// Temporary key is given. Do full context comparison.
				if (id == QueryIdBad)
					return *m_pCtx == *other.m_pCtx;

				// Real key is given. Compare context pointer.
				// Normally we'd compare query IDs but because we do not allow query copies and all queries are
				// unique it's guaranteed that if pointers are the same we have a match.
				return m_pCtx == other.m_pCtx;
			}
		};

		class QueryCache {
			cnt::map<QueryLookupKey, uint32_t> m_queryCache;
			// TODO: Make m_queryArr allocate data in pages.
			//       Currently ilist always uses a darr internally which keeps growing following
			//       logic not suitable for this particular use case.
			//       QueryInfo is quite big and we do not want to copying a lot of data every time
			//       resizing is necessary.
			cnt::ilist<QueryInfo, QueryHandle> m_queryArr;

			//! entity -> query mapping
			cnt::map<EntityLookupKey, cnt::darray<QueryHandle>> m_entityToQuery;

		public:
			QueryCache() {
				m_queryArr.reserve(256);
			}

			~QueryCache() = default;

			QueryCache(QueryCache&&) = delete;
			QueryCache(const QueryCache&) = delete;
			QueryCache& operator=(QueryCache&&) = delete;
			QueryCache& operator=(const QueryCache&) = delete;

			GAIA_NODISCARD bool valid(QueryHandle handle) const {
				if (handle.id() == QueryIdBad)
					return false;

				// Entity ID has to fit inside the entity array
				if (handle.id() >= m_queryArr.size())
					return false;

				const auto& h = m_queryArr[handle.id()];
				return h.idx == handle.id() && h.gen == handle.gen();
			}

			void clear() {
				m_queryCache.clear();
				m_queryArr.clear();
				m_entityToQuery.clear();
			}

			//! Returns a QueryInfo object stored at the index \param idx.
			//! \param idx Index of the QueryInfo we try to retrieve
			//! \return Query info
			QueryInfo* try_get(QueryHandle handle) {
				if (!valid(handle))
					return nullptr;

				auto& info = m_queryArr[handle.id()];
				GAIA_ASSERT(info.idx == handle.id());
				GAIA_ASSERT(info.gen == handle.gen());
				return &info;
			};

			//! Returns a QueryInfo object stored at the index \param idx.
			//! \param idx Index of the QueryInfo we try to retrieve
			//! \return Query info
			QueryInfo& get(QueryHandle handle) {
				GAIA_ASSERT(valid(handle));

				auto& info = m_queryArr[handle.id()];
				GAIA_ASSERT(info.idx == handle.id());
				GAIA_ASSERT(info.gen == handle.gen());
				return info;
			};

			//! Registers the provided query lookup context \param ctx. If it already exists it is returned.
			//! \return Reference a newly created or an already existing QueryInfo object.
			QueryInfo& add(QueryCtx&& ctx, const EntityToArchetypeMap& entityToArchetypeMap) {
				GAIA_ASSERT(ctx.hashLookup.hash != 0);

				// First check if the query cache record exists
				auto ret = m_queryCache.try_emplace(QueryLookupKey(ctx.hashLookup, &ctx));
				if (!ret.second) {
					const auto idx = ret.first->second;
					auto& info = m_queryArr[idx];
					GAIA_ASSERT(idx == info.idx);
					info.add_ref();
					return info;
				}

				// No record exists, let us create a new one
				QueryInfoCreationCtx creationCtx;
				creationCtx.pQueryCtx = &ctx;
				creationCtx.pEntityToArchetypeMap = &entityToArchetypeMap;
				auto handle = m_queryArr.alloc(&creationCtx);

				// We are moving the rvalue to "ctx". As a result, the pointer stored in m_queryCache.emplace above is no longer
				// going to be valid. Therefore we swap the map key with a one with a valid pointer.
				auto& info = get(handle);
				info.add_ref();
				auto new_p = robin_hood::pair(std::make_pair(QueryLookupKey(ctx.hashLookup, &info.ctx()), info.idx));
				ret.first->swap(new_p);

				// Add the entity->query pair
				add_entity_to_query_pairs({info.ids().data(), info.ids().size()}, handle);

				return info;
			}

			//! Deletes an existing QueryInfo object given the provided query lookup context \param ctx.
			bool del(QueryHandle handle) {
				auto* pInfo = try_get(handle);
				if (pInfo == nullptr)
					return false;

				pInfo->dec_ref();
				if (pInfo->refs() != 0)
					return false;

				// If this was the last reference to the query, we can safely remove it
				auto it = m_queryCache.find(QueryLookupKey(pInfo->ctx().hashLookup, &pInfo->ctx()));
				GAIA_ASSERT(it != m_queryCache.end());
				m_queryCache.erase(it);
				m_queryArr.free(handle);

				// Remove the entity->query pair
				del_entity_to_query_pairs({pInfo->ids().data(), pInfo->ids().size()}, handle);

				return true;
			}

			cnt::darray<QueryInfo>::iterator begin() {
				return m_queryArr.begin();
			}

			cnt::darray<QueryInfo>::iterator end() {
				return m_queryArr.end();
			}

			//! Invalidates all cached queries that work with the given entity
			//! This covers the following kinds of query terms:
			//! 1) X
			//! 2) (*, X)
			//! 3) (X, *)
			void invalidate_queries_for_entity(EntityLookupKey entityKey) {
				auto it = m_entityToQuery.find(entityKey);
				if (it == m_entityToQuery.end())
					return;

				const auto& handles = it->second;
				for (auto& handle: handles) {
					auto& info = get(handle);
					info.reset();
					info.refresh_ctx();
				}
			}

		private:
			//! Adds an entity to the <entity, query> map
			//! \param entity Entity getting added
			//! \param handle Query handle
			void add_entity_query_pair(Entity entity, QueryHandle handle) {
				EntityLookupKey entityKey(entity);
				const auto it = m_entityToQuery.find(entityKey);
				if (it == m_entityToQuery.end()) {
					m_entityToQuery.try_emplace(entityKey, cnt::darray<QueryHandle>{handle});
					return;
				}

				auto& handles = it->second;
				if (!core::has(handles, handle))
					handles.push_back(handle);
			}

			//! Deletes an entity from the <entity, query> map
			//! \param entity Entity getting removed
			//! \param handle Query handle
			void del_entity_archetype_pair(Entity entity, QueryHandle handle) {
				auto it = m_entityToQuery.find(EntityLookupKey(entity));
				if (it == m_entityToQuery.end())
					return;

				auto& handles = it->second;
				const auto idx = core::get_index_unsafe(handles, handle);
				core::swap_erase_unsafe(handles, idx);

				// Remove the mapping if there are no more matches
				if (handles.empty())
					m_entityToQuery.erase(it);
			}

			//! Adds an entity to the <entity, query> map
			//! \param entities Entities getting added
			void add_entity_to_query_pairs(EntitySpan entities, QueryHandle handle) {
				for (auto entity: entities) {
					add_entity_query_pair(entity, handle);
				}
			}

			//! Deletes an entity from the <entity, query> map
			//! \param entities Entities getting deleted
			void del_entity_to_query_pairs(EntitySpan entities, QueryHandle handle) {
				for (auto entity: entities) {
					add_entity_query_pair(entity, handle);
				}
			}
		};
	} // namespace ecs
} // namespace gaia

/*** End of inlined file: query_cache.h ***/

namespace gaia {
	namespace ecs {
		class World;

		QuerySerBuffer& query_buffer(World& world);
		const ComponentCache& comp_cache(const World& world);
		ComponentCache& comp_cache_mut(World& world);
		template <typename T>
		const ComponentCacheItem& comp_cache_add(World& world);
		bool is_base(const World& world, Entity entity);

		namespace detail {
			//! Query command types
			enum QueryCmdType : uint8_t { ADD_ITEM, ADD_FILTER, GROUP_BY, SET_GROUP, FILTER_GROUP };

			struct QueryCmd_AddItem {
				static constexpr QueryCmdType Id = QueryCmdType::ADD_ITEM;
				static constexpr bool InvalidatesHash = true;

				QueryInput item;

				void exec(QueryCtx& ctx) const {
					auto& data = ctx.data;
					auto& ids = data.ids;
					auto& terms = data.terms;

					// Unique component ids only
					GAIA_ASSERT(!core::has(ids, item.id));

#if GAIA_DEBUG
					// There's a limit to the amount of query items which we can store
					if (ids.size() >= MAX_ITEMS_IN_QUERY) {
						GAIA_ASSERT2(false, "Trying to create a query with too many components!");

						const auto* name = ctx.cc->get(item.id).name.str();
						GAIA_LOG_E("Trying to add component '%s' to an already full ECS query!", name);
						return;
					}
#endif

					// Build the read-write mask.
					// This will be used to determine what kind of access the user wants for a given component.
					const uint8_t isReadWrite = uint8_t(item.access == QueryAccess::Write);
					data.readWriteMask |= (isReadWrite << (uint8_t)ids.size());

					// Build the Is mask.
					// We will use it to identify entities with an Is relationship quickly.
					if (!item.id.pair()) {
						const auto has_as = (uint8_t)is_base(*ctx.w, item.id);
						data.as_mask_0 |= (has_as << (uint8_t)ids.size());
					} else {
						if (!is_wildcard(item.id.id())) {
							const auto e = entity_from_id(*ctx.w, item.id.id());
							const auto has_as = (uint8_t)is_base(*ctx.w, e);
							data.as_mask_0 |= (has_as << (uint8_t)ids.size());
						}

						if (!is_wildcard(item.id.gen())) {
							const auto e = entity_from_id(*ctx.w, item.id.gen());
							const auto has_as = (uint8_t)is_base(*ctx.w, e);
							data.as_mask_1 |= (has_as << (uint8_t)ids.size());
						}
					}

					// The query engine is going to reorder the query items as necessary.
					// Remapping is used so the user can still identify the items according the order in which
					// they defined them when building the query.
					data.remapping.push_back((uint8_t)data.remapping.size());

					ids.push_back(item.id);
					terms.push_back({item.id, item.src, nullptr, item.op});
				}
			};

			struct QueryCmd_AddFilter {
				static constexpr QueryCmdType Id = QueryCmdType::ADD_FILTER;
				static constexpr bool InvalidatesHash = true;

				Entity comp;

				void exec(QueryCtx& ctx) const {
					auto& data = ctx.data;
					auto& ids = data.ids;
					auto& changed = data.changed;
					const auto& terms = data.terms;

					GAIA_ASSERT(core::has(ids, comp));
					GAIA_ASSERT(!core::has(changed, comp));

#if GAIA_DEBUG
					// There's a limit to the amount of components which we can store
					if (changed.size() >= MAX_ITEMS_IN_QUERY) {
						GAIA_ASSERT2(false, "Trying to create an filter query with too many components!");

						auto compName = ctx.cc->get(comp).name.str();
						GAIA_LOG_E("Trying to add component %s to an already full filter query!", compName);
						return;
					}
#endif

					uint32_t compIdx = 0;
					for (; compIdx < ids.size(); ++compIdx)
						if (ids[compIdx] == comp)
							break;
					// NOTE: This code bellow does technically the same as above.
					//       However, compilers can't quite optimize it as well because it does some more
					//       calculations. This is used often so go with the custom code.
					// const auto compIdx = core::get_index_unsafe(ids, comp);

					// Component has to be present in anyList or allList.
					// NoneList makes no sense because we skip those in query processing anyway.
					if (terms[compIdx].op != QueryOpKind::Not) {
						changed.push_back(comp);
						return;
					}

					GAIA_ASSERT2(false, "SetChangeFilter trying to filter component which is not a part of the query");
#if GAIA_DEBUG
					auto compName = ctx.cc->get(comp).name.str();
					GAIA_LOG_E("SetChangeFilter trying to filter component %s but it's not a part of the query!", compName);
#endif
				}
			};

			struct QueryCmd_GroupBy {
				static constexpr QueryCmdType Id = QueryCmdType::GROUP_BY;
				static constexpr bool InvalidatesHash = true;

				Entity groupBy;
				TGroupByFunc func;

				void exec(QueryCtx& ctx) {
					auto& data = ctx.data;
					data.groupBy = groupBy;
					GAIA_ASSERT(func != nullptr);
					data.groupByFunc = func; // group_by_func_default;
				}
			};

			struct QueryCmd_SetGroupId {
				static constexpr QueryCmdType Id = QueryCmdType::SET_GROUP;
				static constexpr bool InvalidatesHash = false;

				GroupId groupId;

				void exec(QueryCtx& ctx) {
					auto& data = ctx.data;
					data.groupIdSet = groupId;
				}
			};

			template <bool Cached>
			struct QueryImplStorage {
				World* m_world{};
				//! QueryImpl cache (stable pointer to parent world's query cache)
				QueryCache* m_queryCache{};
				//! Query identity
				QueryIdentity m_q{};
				bool m_destroyed = false;

			public:
				QueryImplStorage() = default;
				~QueryImplStorage() {
					if (try_del_from_cache())
						ser_buffer_reset();
				}

				QueryImplStorage(QueryImplStorage&& other) {
					m_world = other.m_world;
					m_queryCache = other.m_queryCache;
					m_q = other.m_q;
					m_destroyed = other.m_destroyed;

					// Make sure old instance is invalidated
					other.m_q = {};
					other.m_destroyed = false;
				}
				QueryImplStorage& operator=(QueryImplStorage&& other) {
					GAIA_ASSERT(core::addressof(other) != this);

					m_world = other.m_world;
					m_queryCache = other.m_queryCache;
					m_q = other.m_q;
					m_destroyed = other.m_destroyed;

					// Make sure old instance is invalidated
					other.m_q = {};
					other.m_destroyed = false;

					return *this;
				}

				QueryImplStorage(const QueryImplStorage& other) {
					m_world = other.m_world;
					m_queryCache = other.m_queryCache;
					m_q = other.m_q;
					m_destroyed = other.m_destroyed;

					// Make sure to update the ref count of the cached query so
					// it doesn't get deleted by accident.
					if (!m_destroyed) {
						auto* pInfo = m_queryCache->try_get(m_q.handle);
						if (pInfo != nullptr)
							pInfo->add_ref();
					}
				}
				QueryImplStorage& operator=(const QueryImplStorage& other) {
					GAIA_ASSERT(core::addressof(other) != this);

					m_world = other.m_world;
					m_queryCache = other.m_queryCache;
					m_q = other.m_q;
					m_destroyed = other.m_destroyed;

					// Make sure to update the ref count of the cached query so
					// it doesn't get deleted by accident.
					if (!m_destroyed) {
						auto* pInfo = m_queryCache->try_get(m_q.handle);
						if (pInfo != nullptr)
							pInfo->add_ref();
					}

					return *this;
				}

				GAIA_NODISCARD World* world() {
					return m_world;
				}

				GAIA_NODISCARD QuerySerBuffer& ser_buffer() {
					return m_q.ser_buffer(m_world);
				}
				void ser_buffer_reset() {
					return m_q.ser_buffer_reset(m_world);
				}

				void init(World* world, QueryCache* queryCache) {
					m_world = world;
					m_queryCache = queryCache;
				}

				//! Release any data allocated by the query
				void reset() {
					auto& info = m_queryCache->get(m_q.handle);
					info.reset();
				}

				void allow_to_destroy_again() {
					m_destroyed = false;
				}

				//! Try delete the query from query cache
				GAIA_NODISCARD bool try_del_from_cache() {
					if (!m_destroyed)
						m_queryCache->del(m_q.handle);

					// Don't allow multiple calls to destroy to break the reference counter.
					// One object is only allowed to destroy once.
					m_destroyed = true;
					return false;
				}

				//! Invalidates the query handle
				void invalidate() {
					m_q.handle = {};
				}

				//! Returns true if the query is found in the query cache.
				GAIA_NODISCARD bool is_cached() const {
					auto* pInfo = m_queryCache->try_get(m_q.handle);
					return pInfo != nullptr;
				}

				//! Returns true if the query is ready to be used.
				GAIA_NODISCARD bool is_initialized() const {
					return m_world != nullptr && m_queryCache != nullptr;
				}
			};

			template <>
			struct QueryImplStorage<false> {
				QueryInfo m_queryInfo;

				QueryImplStorage() {
					m_queryInfo.idx = QueryIdBad;
					m_queryInfo.gen = QueryIdBad;
				}

				GAIA_NODISCARD World* world() {
					return m_queryInfo.world();
				}

				GAIA_NODISCARD QuerySerBuffer& ser_buffer() {
					return m_queryInfo.ser_buffer();
				}
				void ser_buffer_reset() {
					return m_queryInfo.ser_buffer_reset();
				}

				void init(World* world) {
					m_queryInfo.init(world);
				}

				//! Release any data allocated by the query
				void reset() {
					m_queryInfo.reset();
				}

				//! Does nothing for uncached queries.
				GAIA_NODISCARD bool try_del_from_cache() {
					return false;
				}

				//! Does nothing for uncached queries.
				void invalidate() {}

				//! Does nothing for uncached queries.
				GAIA_NODISCARD bool is_cached() const {
					return false;
				}

				//! Returns true. Uncached queries are always considered initialized.
				GAIA_NODISCARD bool is_initialized() const {
					return true;
				}
			};

			template <bool UseCaching = true>
			class QueryImpl {
				static constexpr uint32_t ChunkBatchSize = 32;

				struct ChunkBatch {
					Chunk* pChunk;
					const uint8_t* pIndicesMapping;
					GroupId groupId;
				};

				using CmdBuffer = SerializationBufferDyn;
				using ChunkSpan = std::span<const Chunk*>;
				using ChunkSpanMut = std::span<Chunk*>;
				using ChunkBatchArray = cnt::sarray_ext<ChunkBatch, ChunkBatchSize>;
				using CmdFunc = void (*)(CmdBuffer& buffer, QueryCtx& ctx);

			private:
				static constexpr CmdFunc CommandBufferRead[] = {
						// Add item
						[](CmdBuffer& buffer, QueryCtx& ctx) {
							QueryCmd_AddItem cmd;
							ser::load(buffer, cmd);
							cmd.exec(ctx);
						},
						// Add filter
						[](CmdBuffer& buffer, QueryCtx& ctx) {
							QueryCmd_AddFilter cmd;
							ser::load(buffer, cmd);
							cmd.exec(ctx);
						},
						// GroupBy
						[](CmdBuffer& buffer, QueryCtx& ctx) {
							QueryCmd_GroupBy cmd;
							ser::load(buffer, cmd);
							cmd.exec(ctx);
						},
						// SetGroupId
						[](CmdBuffer& buffer, QueryCtx& ctx) {
							QueryCmd_SetGroupId cmd;
							ser::load(buffer, cmd);
							cmd.exec(ctx);
						}};

				//! Storage for data based on whether Caching is used or not
				QueryImplStorage<UseCaching> m_storage;
				//! World version (stable pointer to parent world's m_nextArchetypeId)
				ArchetypeId* m_nextArchetypeId{};
				//! World version (stable pointer to parent world's world version)
				uint32_t* m_worldVersion{};
				//! Map of archetypes (stable pointer to parent world's archetype array)
				const ArchetypeMapById* m_archetypes{};
				//! Map of component ids to archetypes (stable pointer to parent world's archetype component-to-archetype map)
				const EntityToArchetypeMap* m_entityToArchetypeMap{};
				//! All world archetypes
				const ArchetypeDArray* m_allArchetypes{};

				//--------------------------------------------------------------------------------
			public:
				//! Fetches the QueryInfo object.
				//! \return QueryInfo object
				QueryInfo& fetch() {
					if constexpr (UseCaching) {
						GAIA_PROF_SCOPE(query::fetch);

						// Make sure the query was created by World::query()
						GAIA_ASSERT(m_storage.is_initialized());

						// If queryId is set it means QueryInfo was already created.
						// This is the common case for cached queries.
						if GAIA_LIKELY (m_storage.m_q.handle.id() != QueryIdBad) {
							auto* pQueryInfo = m_storage.m_queryCache->try_get(m_storage.m_q.handle);

							// The only time when this can be nullptr is just once after Query::destroy is called.
							if GAIA_LIKELY (pQueryInfo != nullptr) {
								recommit(pQueryInfo->ctx());
								pQueryInfo->match(*m_entityToArchetypeMap, *m_allArchetypes, last_archetype_id());
								return *pQueryInfo;
							}

							m_storage.invalidate();
						}

						// No queryId is set which means QueryInfo needs to be created
						QueryCtx ctx;
						ctx.init(m_storage.world());
						commit(ctx);
						auto& queryInfo = m_storage.m_queryCache->add(GAIA_MOV(ctx), *m_entityToArchetypeMap);
						m_storage.m_q.handle = queryInfo.handle(queryInfo);
						m_storage.allow_to_destroy_again();
						queryInfo.match(*m_entityToArchetypeMap, *m_allArchetypes, last_archetype_id());
						return queryInfo;
					} else {
						GAIA_PROF_SCOPE(query::fetchu);

						if GAIA_UNLIKELY (m_storage.m_queryInfo.ctx().q.handle.id() == QueryIdBad) {
							QueryCtx ctx;
							ctx.init(m_storage.world());
							commit(ctx);
							m_storage.m_queryInfo = QueryInfo::create(QueryId{}, GAIA_MOV(ctx), *m_entityToArchetypeMap);
						}
						m_storage.m_queryInfo.match(*m_entityToArchetypeMap, *m_allArchetypes, last_archetype_id());
						return m_storage.m_queryInfo;
					}
				}

				//--------------------------------------------------------------------------------
			private:
				ArchetypeId last_archetype_id() const {
					return *m_nextArchetypeId - 1;
				}

				template <typename T>
				void add_cmd(T& cmd) {
					// Make sure to invalidate if necessary.
					if constexpr (T::InvalidatesHash)
						m_storage.invalidate();

					auto& serBuffer = m_storage.ser_buffer();
					ser::save(serBuffer, T::Id);
					ser::save(serBuffer, T::InvalidatesHash);
					ser::save(serBuffer, cmd);
				}

				void add_inter(QueryInput item) {
					// When excluding make sure the access type is None.
					GAIA_ASSERT(item.op != QueryOpKind::Not || item.access == QueryAccess::None);

					QueryCmd_AddItem cmd{item};
					add_cmd(cmd);
				}

				template <typename T>
				void add_inter(QueryOpKind op) {
					Entity e;

					if constexpr (is_pair<T>::value) {
						// Make sure the components are always registered
						const auto& desc_rel = comp_cache_add<typename T::rel_type>(*m_storage.world());
						const auto& desc_tgt = comp_cache_add<typename T::tgt_type>(*m_storage.world());

						e = Pair(desc_rel.entity, desc_tgt.entity);
					} else {
						// Make sure the component is always registered
						const auto& desc = comp_cache_add<T>(*m_storage.world());
						e = desc.entity;
					}

					// Determine the access type
					QueryAccess access = QueryAccess::None;
					if (op != QueryOpKind::Not) {
						constexpr auto isReadWrite = core::is_mut_v<T>;
						access = isReadWrite ? QueryAccess::Write : QueryAccess::Read;
					}

					add_inter({op, access, e});
				}

				template <typename Rel, typename Tgt>
				void add_inter(QueryOpKind op) {
					using UO_Rel = typename component_type_t<Rel>::TypeOriginal;
					using UO_Tgt = typename component_type_t<Tgt>::TypeOriginal;
					static_assert(core::is_raw_v<UO_Rel>, "Use add() with raw types only");
					static_assert(core::is_raw_v<UO_Tgt>, "Use add() with raw types only");

					// Make sure the component is always registered
					const auto& descRel = comp_cache_add<Rel>(*m_storage.world());
					const auto& descTgt = comp_cache_add<Tgt>(*m_storage.world());

					// Determine the access type
					QueryAccess access = QueryAccess::None;
					if (op != QueryOpKind::Not) {
						constexpr auto isReadWrite = core::is_mut_v<UO_Rel> || core::is_mut_v<UO_Tgt>;
						access = isReadWrite ? QueryAccess::Write : QueryAccess::Read;
					}

					add_inter({op, access, {descRel.entity, descTgt.entity}});
				}

				//--------------------------------------------------------------------------------

				void changed_inter(Entity entity) {
					QueryCmd_AddFilter cmd{entity};
					add_cmd(cmd);
				}

				template <typename T>
				void changed_inter() {
					using UO = typename component_type_t<T>::TypeOriginal;
					static_assert(core::is_raw_v<UO>, "Use changed() with raw types only");

					// Make sure the component is always registered
					const auto& desc = comp_cache_add<T>(*m_storage.world());
					changed_inter(desc.entity);
				}

				template <typename Rel, typename Tgt>
				void changed_inter() {
					using UO_Rel = typename component_type_t<Rel>::TypeOriginal;
					using UO_Tgt = typename component_type_t<Tgt>::TypeOriginal;
					static_assert(core::is_raw_v<UO_Rel>, "Use changed() with raw types only");
					static_assert(core::is_raw_v<UO_Tgt>, "Use changed() with raw types only");

					// Make sure the component is always registered
					const auto& descRel = comp_cache_add<Rel>(*m_storage.world());
					const auto& descTgt = comp_cache_add<Tgt>(*m_storage.world());
					changed_inter({descRel.entity, descTgt.entity});
				}

				//--------------------------------------------------------------------------------

				void group_by_inter(Entity entity, TGroupByFunc func) {
					QueryCmd_GroupBy cmd{entity, func};
					add_cmd(cmd);
				}

				template <typename T>
				void group_by_inter(Entity entity, TGroupByFunc func) {
					using UO = typename component_type_t<T>::TypeOriginal;
					static_assert(core::is_raw_v<UO>, "Use changed() with raw types only");

					group_by_inter(entity, func);
				}

				template <typename Rel, typename Tgt>
				void group_by_inter(TGroupByFunc func) {
					using UO_Rel = typename component_type_t<Rel>::TypeOriginal;
					using UO_Tgt = typename component_type_t<Tgt>::TypeOriginal;
					static_assert(core::is_raw_v<UO_Rel>, "Use group_by() with raw types only");
					static_assert(core::is_raw_v<UO_Tgt>, "Use group_by() with raw types only");

					// Make sure the component is always registered
					const auto& descRel = comp_cache_add<Rel>(*m_storage.world());
					const auto& descTgt = comp_cache_add<Tgt>(*m_storage.world());

					group_by_inter({descRel.entity, descTgt.entity}, func);
				}

				//--------------------------------------------------------------------------------

				void set_group_id_inter(GroupId groupId) {
					// Dummy usage of GroupIdMax to avoid warning about unused constant
					(void)GroupIdMax;

					QueryCmd_SetGroupId cmd{groupId};
					add_cmd(cmd);
				}

				void set_group_id_inter(Entity groupId) {
					set_group_id_inter(groupId.value());
				}

				template <typename T>
				void set_group_id_inter() {
					using UO = typename component_type_t<T>::TypeOriginal;
					static_assert(core::is_raw_v<UO>, "Use group_id() with raw types only");

					// Make sure the component is always registered
					const auto& desc = comp_cache_add<T>(*m_storage.world());
					set_group_inter(desc.entity);
				}

				//--------------------------------------------------------------------------------

				void commit(QueryCtx& ctx) {
					GAIA_PROF_SCOPE(commit);

#if GAIA_ASSERT_ENABLED
					if constexpr (UseCaching) {
						GAIA_ASSERT(m_storage.m_q.handle.id() == QueryIdBad);
					} else {
						GAIA_ASSERT(m_storage.m_queryInfo.idx == QueryIdBad);
					}
#endif

					auto& serBuffer = m_storage.ser_buffer();

					// Read data from buffer and execute the command stored in it
					serBuffer.seek(0);
					while (serBuffer.tell() < serBuffer.bytes()) {
						QueryCmdType id{};
						bool invalidatesHash = false;
						ser::load(serBuffer, id);
						ser::load(serBuffer, invalidatesHash);
						(void)invalidatesHash; // We don't care about this during commit
						CommandBufferRead[id](serBuffer, ctx);
					}

					// Calculate the lookup hash from the provided context
					if constexpr (UseCaching) {
						calc_lookup_hash(ctx);
					}

					// We can free all temporary data now
					m_storage.ser_buffer_reset();
				}

				void recommit(QueryCtx& ctx) {
					GAIA_PROF_SCOPE(recommit);

					auto& serBuffer = m_storage.ser_buffer();

					// Read data from buffer and execute the command stored in it
					serBuffer.seek(0);
					while (serBuffer.tell() < serBuffer.bytes()) {
						QueryCmdType id{};
						bool invalidatesHash = false;
						ser::load(serBuffer, id);
						ser::load(serBuffer, invalidatesHash);
						// Hash recalculation is not accepted here
						GAIA_ASSERT(!invalidatesHash);
						if (invalidatesHash)
							return;
						CommandBufferRead[id](serBuffer, ctx);
					}

					// We can free all temporary data now
					m_storage.ser_buffer_reset();
				}

				//--------------------------------------------------------------------------------
			public:
#if GAIA_ASSERT_ENABLED
				//! Unpacks the parameter list \param types into query \param query and performs has_all for each of them
				template <typename... T>
				GAIA_NODISCARD bool unpack_args_into_query_has_all(
						const QueryInfo& queryInfo, [[maybe_unused]] core::func_type_list<T...> types) const {
					if constexpr (sizeof...(T) > 0)
						return queryInfo.template has_all<T...>();
					else
						return true;
				}
#endif

				//--------------------------------------------------------------------------------

				GAIA_NODISCARD static bool match_filters(const Chunk& chunk, const QueryInfo& queryInfo) {
					GAIA_ASSERT(!chunk.empty() && "match_filters called on an empty chunk");

					const auto queryVersion = queryInfo.world_version();

					// See if any component has changed
					const auto& filtered = queryInfo.filters();
					for (const auto comp: filtered) {
						// TODO: Components are sorted. Therefore, we don't need to search from 0
						//       all the time. We can search from the last found index.
						const auto compIdx = chunk.comp_idx(comp);
						if (chunk.changed(queryVersion, compIdx))
							return true;
					}

					// Skip unchanged chunks.
					return false;
				}

				//! Execute functors in batches
				template <typename Func, typename TIter>
				static void run_query_func(Func func, TIter& it, ChunkBatchArray& chunks) {
					GAIA_PROF_SCOPE(query::run_query_func);

					const auto chunkCnt = chunks.size();
					GAIA_ASSERT(chunkCnt > 0);

					auto runFunc = [&](ChunkBatch& batch) {
						auto* pChunk = batch.pChunk;

#if GAIA_ASSERT_ENABLED
						pChunk->lock(true);
#endif
						it.set_group_id(batch.groupId);
						it.set_remapping_indices(batch.pIndicesMapping);
						it.set_chunk(pChunk);
						func(it);
#if GAIA_ASSERT_ENABLED
						pChunk->lock(false);
#endif
					};

					// This is what the function is doing:
					// for (auto *pChunk: chunks) {
					//  pChunk->lock(true);
					//	runFunc(pChunk);
					//  pChunk->lock(false);
					// }
					// chunks.clear();

					// We only have one chunk to process
					if GAIA_UNLIKELY (chunkCnt == 1) {
						runFunc(chunks[0]);
						chunks.clear();
						return;
					}

					// We have many chunks to process.
					// Chunks might be located at different memory locations. Not even in the same memory page.
					// Therefore, to make it easier for the CPU we give it a hint that we want to prefetch data
					// for the next chunk explicitly so we do not end up stalling later.
					// Note, this is a micro optimization and on average it brings no performance benefit. It only
					// helps with edge cases.
					// Let us be conservative for now and go with T2. That means we will try to keep our data at
					// least in L3 cache or higher.
					gaia::prefetch(&chunks[1].pChunk, PrefetchHint::PREFETCH_HINT_T2);
					runFunc(chunks[0]);

					uint32_t chunkIdx = 1;
					for (; chunkIdx < chunkCnt - 1; ++chunkIdx) {
						gaia::prefetch(&chunks[chunkIdx + 1].pChunk, PrefetchHint::PREFETCH_HINT_T2);
						runFunc(chunks[chunkIdx]);
					}

					runFunc(chunks[chunkIdx]);

					chunks.clear();
				}

				GAIA_NODISCARD bool can_process_archetype(const Archetype& archetype) const {
					// Archetypes requested for deletion are skipped for processing
					return !archetype.is_req_del();
				}

				template <bool HasFilters, typename TIter, typename Func>
				void run_query(const QueryInfo& queryInfo, Func func) {
					ChunkBatchArray chunkBatch;
					TIter it;

					GAIA_PROF_SCOPE(query::run_query); // batch preparation + chunk processing

					// TODO: Have archetype cache as double-linked list with pointers only.
					//       Have chunk cache as double-linked list with pointers only.
					//       Make it so only valid pointers are linked together.
					//       This means one less indirection + we won't need to call can_process_archetype()
					//       and pChunk.size()==0 here.
					auto cache_view = queryInfo.cache_archetype_view();
					auto cache_data_view = queryInfo.cache_data_view();

					// Determine the range of archetypes we will iterate.
					// Use the entire range by default.
					uint32_t idx_from = 0;
					uint32_t idx_to = (uint32_t)cache_view.size();

					const bool isGroupBy = queryInfo.data().groupBy != EntityBad;
					const bool isGroupSet = queryInfo.data().groupIdSet != 0;
					if (isGroupBy && isGroupSet) {
						// We wish to iterate only a certain group
						auto group_data_view = queryInfo.group_data_view();
						GAIA_EACH(group_data_view) {
							if (group_data_view[i].groupId == queryInfo.data().groupIdSet) {
								idx_from = group_data_view[i].idxFirst;
								idx_to = group_data_view[i].idxLast + 1;
								goto groupSetFound;
							}
						}
						return;
					}

				groupSetFound:
					ArchetypeCacheData dummyCacheData{};

					for (uint32_t i = idx_from; i < idx_to; ++i) {
						auto* pArchetype = cache_view[i];

						if GAIA_UNLIKELY (!can_process_archetype(*pArchetype))
							continue;

						const auto& cacheData = isGroupBy ? cache_data_view[i] : dummyCacheData;
						GAIA_ASSERT(
								// Either no grouping is used...
								!isGroupBy ||
								// ... or no groupId is set...
								queryInfo.data().groupIdSet == 0 ||
								// ... or the groupId must match the requested one
								cache_data_view[i].groupId == queryInfo.data().groupIdSet);

						auto indices_view = queryInfo.indices_mapping_view(i);
						const auto& chunks = pArchetype->chunks();

						uint32_t chunkOffset = 0;
						uint32_t itemsLeft = chunks.size();
						while (itemsLeft > 0) {
							const auto maxBatchSize = chunkBatch.max_size() - chunkBatch.size();
							const auto batchSize = itemsLeft > maxBatchSize ? maxBatchSize : itemsLeft;

							ChunkSpanMut chunkSpan((Chunk**)&chunks[chunkOffset], batchSize);
							for (auto* pChunk: chunkSpan) {
								it.set_chunk(pChunk);
								if GAIA_UNLIKELY (it.size() == 0)
									continue;

								if constexpr (HasFilters) {
									if (!match_filters(*pChunk, queryInfo))
										continue;
								}

								chunkBatch.push_back({pChunk, indices_view.data(), cacheData.groupId});
							}

							if GAIA_UNLIKELY (chunkBatch.size() == chunkBatch.max_size())
								run_query_func(func, it, chunkBatch);

							itemsLeft -= batchSize;
							chunkOffset += batchSize;
						}
					}

					// Take care of any leftovers not processed during run_query
					if (!chunkBatch.empty())
						run_query_func(func, it, chunkBatch);
				}

				template <typename TIter, typename Func>
				void run_query_on_chunks(QueryInfo& queryInfo, Func func) {
					// Update the world version
					update_version(*m_worldVersion);

					const bool hasFilters = queryInfo.has_filters();
					if (hasFilters)
						run_query<true, TIter>(queryInfo, func);
					else
						run_query<false, TIter>(queryInfo, func);

					// Update the query version with the current world's version
					queryInfo.set_world_version(*m_worldVersion);
				}

				template <typename TIter, typename Func, typename... T>
				GAIA_FORCEINLINE void
				run_query_on_chunk(TIter& it, Func func, [[maybe_unused]] core::func_type_list<T...> types) {
					if constexpr (sizeof...(T) > 0) {
						// Pointers to the respective component types in the chunk, e.g
						// 		q.each([&](Position& p, const Velocity& v) {...}
						// Translates to:
						//  	auto p = it.view_mut_inter<Position, true>();
						//		auto v = it.view_inter<Velocity>();
						auto dataPointerTuple = std::make_tuple(it.template view_auto<T>()...);

						// Iterate over each entity in the chunk.
						// Translates to:
						//		GAIA_EACH(it) func(p[i], v[i]);

						GAIA_EACH(it) {
							func(std::get<decltype(it.template view_auto<T>())>(dataPointerTuple)[it.template acc_index<T>(i)]...);
						}
					} else {
						// No functor parameters. Do an empty loop.
						GAIA_EACH(it) func();
					}
				}

				template <bool UseFilters, typename TIter>
				GAIA_NODISCARD bool empty_inter(const QueryInfo& queryInfo) const {
					for (const auto* pArchetype: queryInfo) {
						if GAIA_UNLIKELY (!can_process_archetype(*pArchetype))
							continue;

						GAIA_PROF_SCOPE(query::empty);

						const auto& chunks = pArchetype->chunks();
						TIter it;

						const bool isNotEmpty = core::has_if(chunks, [&](Chunk* pChunk) {
							it.set_chunk(pChunk);
							if constexpr (UseFilters)
								return it.size() > 0 && match_filters(*pChunk, queryInfo);
							else
								return it.size() > 0;
						});

						if (isNotEmpty)
							return false;
					}

					return true;
				}

				template <bool UseFilters, typename TIter>
				GAIA_NODISCARD uint32_t count_inter(const QueryInfo& queryInfo) const {
					uint32_t cnt = 0;
					TIter it;

					for (auto* pArchetype: queryInfo) {
						if GAIA_UNLIKELY (!can_process_archetype(*pArchetype))
							continue;

						GAIA_PROF_SCOPE(query::count);

						// No mapping for count(). It doesn't need to access data cache.
						// auto indices_view = queryInfo.indices_mapping_view(aid);

						const auto& chunks = pArchetype->chunks();
						for (auto* pChunk: chunks) {
							it.set_chunk(pChunk);

							const auto entityCnt = it.size();
							if (entityCnt == 0)
								continue;

							// Filters
							if constexpr (UseFilters) {
								if (!match_filters(*pChunk, queryInfo))
									continue;
							}

							// Entity count
							cnt += entityCnt;
						}
					}

					return cnt;
				}

				template <bool UseFilters, typename TIter, typename ContainerOut>
				void arr_inter(QueryInfo& queryInfo, ContainerOut& outArray) {
					using ContainerItemType = typename ContainerOut::value_type;
					TIter it;

					for (auto* pArchetype: queryInfo) {
						if GAIA_UNLIKELY (!can_process_archetype(*pArchetype))
							continue;

						GAIA_PROF_SCOPE(query::arr);

						// No mapping for arr(). It doesn't need to access data cache.
						// auto indices_view = queryInfo.indices_mapping_view(aid);

						const auto& chunks = pArchetype->chunks();
						for (auto* pChunk: chunks) {
							it.set_chunk(pChunk);
							if (it.size() == 0)
								continue;

							// Filters
							if constexpr (UseFilters) {
								if (!match_filters(*pChunk, queryInfo))
									continue;
							}

							const auto dataView = it.template view<ContainerItemType>();
							GAIA_EACH(it) {
								outArray.push_back(dataView[it.template acc_index<ContainerItemType>(i)]);
							}
						}
					}
				}

				static auto trim(std::span<const char> expr) {
					uint32_t beg = 0;
					while (expr[beg] == ' ')
						++beg;
					uint32_t end = (uint32_t)expr.size() - 1;
					while (end > beg && expr[end] == ' ')
						--end;
					return expr.subspan(beg, end - beg + 1);
				};

				Entity expr_to_entity(va_list& args, std::span<const char> exprRaw) {
					auto expr = trim(exprRaw);

					if (expr[0] == '%') {
						if (expr[1] != 'e') {
							GAIA_ASSERT2(false, "Expression '%' not terminated");
							return EntityBad;
						}

						auto id = (Identifier)va_arg(args, unsigned long long);
						return Entity(id);
					}

					if (expr[0] == '(') {
						if (expr.back() != ')') {
							GAIA_ASSERT2(false, "Expression '(' not terminated");
							return EntityBad;
						}

						auto idStr = expr.subspan(1, expr.size() - 2);
						const auto commaIdx = core::get_index(idStr, ',');

						const auto first = expr_to_entity(args, idStr.subspan(0, commaIdx));
						if (first == EntityBad)
							return EntityBad;
						const auto second = expr_to_entity(args, idStr.subspan(commaIdx + 1));
						if (second == EntityBad)
							return EntityBad;

						return ecs::Pair(first, second);
					}

					{
						auto idStr = trim(expr);

						// Wildcard character
						if (idStr.size() == 1 && idStr[0] == '*')
							return All;

						// Anything else is a component name
						const auto& cc = comp_cache(*m_storage.world());
						const auto* pItem = cc.find(idStr.data(), (uint32_t)idStr.size());
						if (pItem == nullptr) {
							GAIA_ASSERT2(false, "Type not found");
							GAIA_LOG_W("Type '%.*s' not found", (uint32_t)idStr.size(), idStr.data());
							return EntityBad;
						}

						return pItem->entity;
					}
				};

			public:
				QueryImpl() = default;
				~QueryImpl() = default;

				template <bool FuncEnabled = UseCaching>
				QueryImpl(
						World& world, QueryCache& queryCache, ArchetypeId& nextArchetypeId, uint32_t& worldVersion,
						const ArchetypeMapById& archetypes, const EntityToArchetypeMap& entityToArchetypeMap,
						const ArchetypeDArray& allArchetypes):
						m_nextArchetypeId(&nextArchetypeId), m_worldVersion(&worldVersion), m_archetypes(&archetypes),
						m_entityToArchetypeMap(&entityToArchetypeMap), m_allArchetypes(&allArchetypes) {
					m_storage.init(&world, &queryCache);
				}

				template <bool FuncEnabled = !UseCaching>
				QueryImpl(
						World& world, ArchetypeId& nextArchetypeId, uint32_t& worldVersion, const ArchetypeMapById& archetypes,
						const EntityToArchetypeMap& entityToArchetypeMap, const ArchetypeDArray& allArchetypes):
						m_nextArchetypeId(&nextArchetypeId), m_worldVersion(&worldVersion), m_archetypes(&archetypes),
						m_entityToArchetypeMap(&entityToArchetypeMap), m_allArchetypes(&allArchetypes) {
					m_storage.init(&world);
				}

				GAIA_NODISCARD QueryId id() const {
					static_assert(UseCaching, "id() can be used only with cached queries");
					return m_storage.m_q.handle.id();
				}

				GAIA_NODISCARD uint32_t gen() const {
					static_assert(UseCaching, "gen() can be used only with cached queries");
					return m_storage.m_q.handle.gen();
				}

				//------------------------------------------------

				//! Release any data allocated by the query
				void reset() {
					m_storage.reset();
				}

				void destroy() {
					m_storage.try_del_from_cache();
				}

				//! Returns true if the query is stored in the query cache
				GAIA_NODISCARD bool is_cached() const {
					return m_storage.is_cached();
				}

				//------------------------------------------------

				//! Creates a query from a null-terminated expression string.
				//!
				//! Expresion is a string between two semicolons.
				//! Spaces are trimmed automatically.
				//!
				//! Supported modifiers:
				//!   ";" - separates expressions
				//!   "?" - query::any
				//!   "!" - query::none
				//!   "&" - read-write access
				//!   "%e" - entity value
				//!   "(rel,tgt)" - relationship pair, a wildcard character in either rel or tgt is translated into All
				//!
				//! Example:
				//!   struct Position {...};
				//!   struct Velocity {...};
				//!   struct RigidBody {...};
				//!   struct Fuel {...};
				//!   auto player = w.add();
				//!   w.query().add("&Position; !Velocity; ?RigidBody; (Fuel,*); %e", player.value());
				//! Translates into:
				//!   w.query()
				//!      .all<Position&>()
				//!      .no<Velocity>()
				//!      .any<RigidBody>()
				//!      .all(Pair(w.add<Fuel>().entity, All)>()
				//!      .all(Player);
				//!
				//! \param str Null-terminated string with the query expression
				QueryImpl& add(const char* str, ...) {
					GAIA_ASSERT(str != nullptr);
					if (str == nullptr)
						return *this;

					va_list args{};
					va_start(args, str);

					uint32_t pos = 0;
					uint32_t exp0 = 0;

					auto process = [&]() {
						std::span<const char> exprRaw(&str[exp0], pos - exp0);
						exp0 = ++pos;

						auto expr = trim(exprRaw);
						if (expr.empty())
							return true;

						if (expr[0] == '+') {
							uint32_t off = 1;
							if (expr[1] == '&')
								off = 2;

							auto var = expr.subspan(off);
							auto entity = expr_to_entity(args, var);
							if (entity == EntityBad)
								return false;

							any(entity, off != 0);
						} else if (expr[0] == '!') {
							auto var = expr.subspan(1);
							auto entity = expr_to_entity(args, var);
							if (entity == EntityBad)
								return false;

							no(entity);
						} else {
							auto entity = expr_to_entity(args, expr);
							if (entity == EntityBad)
								return false;

							all(entity);
						}

						return true;
					};

					for (; str[pos] != 0; ++pos) {
						if (str[pos] == ';' && !process())
							goto add_end;
					}
					process();

				add_end:
					va_end(args);
					return *this;
				}

				QueryImpl& add(QueryInput item) {
					// Add commands to the command buffer
					add_inter(item);
					return *this;
				}

				//------------------------------------------------

				QueryImpl& all(Entity entity, bool isReadWrite = false) {
					if (entity.pair())
						add({QueryOpKind::All, QueryAccess::None, entity});
					else
						add({QueryOpKind::All, isReadWrite ? QueryAccess::Write : QueryAccess::Read, entity});
					return *this;
				}

				QueryImpl& all(Entity entity, Entity src, bool isReadWrite = false) {
					if (entity.pair())
						add({QueryOpKind::All, QueryAccess::None, entity, src});
					else
						add({QueryOpKind::All, isReadWrite ? QueryAccess::Write : QueryAccess::Read, entity, src});
					return *this;
				}

				template <typename... T>
				QueryImpl& all() {
					// Add commands to the command buffer
					(add_inter<T>(QueryOpKind::All), ...);
					return *this;
				}

				//------------------------------------------------

				QueryImpl& any(Entity entity, bool isReadWrite = false) {
					if (entity.pair())
						add({QueryOpKind::Any, QueryAccess::None, entity});
					else
						add({QueryOpKind::Any, isReadWrite ? QueryAccess::Write : QueryAccess::Read, entity});
					return *this;
				}

				template <typename... T>
				QueryImpl& any() {
					// Add commands to the command buffer
					(add_inter<T>(QueryOpKind::Any), ...);
					return *this;
				}

				//------------------------------------------------

				QueryImpl& no(Entity entity) {
					add({QueryOpKind::Not, QueryAccess::None, entity});
					return *this;
				}

				template <typename... T>
				QueryImpl& no() {
					// Add commands to the command buffer
					(add_inter<T>(QueryOpKind::Not), ...);
					return *this;
				}

				//------------------------------------------------

				QueryImpl& changed(Entity entity) {
					changed_inter(entity);
					return *this;
				}

				template <typename... T>
				QueryImpl& changed() {
					// Add commands to the command buffer
					(changed_inter<T>(), ...);
					return *this;
				}

				//------------------------------------------------

				QueryImpl& group_by(Entity entity, TGroupByFunc func = group_by_func_default) {
					group_by_inter(entity, func);
					return *this;
				}

				template <typename T>
				QueryImpl& group_by(TGroupByFunc func = group_by_func_default) {
					group_by_inter<T>(func);
					return *this;
				}

				template <typename Rel, typename Tgt>
				QueryImpl& group_by(TGroupByFunc func = group_by_func_default) {
					group_by_inter<Rel, Tgt>(func);
					return *this;
				}

				//------------------------------------------------

				QueryImpl& group_id(GroupId groupId) {
					set_group_id_inter(groupId);
					return *this;
				}

				QueryImpl& group_id(Entity entity) {
					GAIA_ASSERT(!entity.pair());
					set_group_id_inter(entity.id());
					return *this;
				}

				template <typename T>
				QueryImpl& group_id() {
					set_group_id_inter<T>();
					return *this;
				}

				//------------------------------------------------

				template <typename Func>
				void each(QueryInfo& queryInfo, Func func) {
					using InputArgs = decltype(core::func_args(&Func::operator()));

#if GAIA_ASSERT_ENABLED
					// Make sure we only use components specified in the query.
					// Constness is respected. Therefore, if a type is const when registered to query,
					// it has to be const (or immutable) also in each().
					// in query.
					// Example 1:
					//   auto q = w.query().all<MyType>(); // immutable access requested
					//   q.each([](MyType val)) {}); // okay
					//   q.each([](const MyType& val)) {}); // okay
					//   q.each([](MyType& val)) {}); // error
					// Example 2:
					//   auto q = w.query().all<MyType&>(); // mutable access requested
					//   q.each([](MyType val)) {}); // error
					//   q.each([](const MyType& val)) {}); // error
					//   q.each([](MyType& val)) {}); // okay
					GAIA_ASSERT(unpack_args_into_query_has_all(queryInfo, InputArgs{}));
#endif

					run_query_on_chunks<Iter>(queryInfo, [&](Iter& it) {
						GAIA_PROF_SCOPE(query_func);
						run_query_on_chunk(it, func, InputArgs{});
					});
				}

				template <typename Func>
				void each(Func func) {
					auto& queryInfo = fetch();

					if constexpr (std::is_invocable_v<Func, IterAll&>) {
						run_query_on_chunks<IterAll>(queryInfo, [&](IterAll& it) {
							GAIA_PROF_SCOPE(query_func);
							func(it);
						});
					} else if constexpr (std::is_invocable_v<Func, Iter&>) {
						run_query_on_chunks<Iter>(queryInfo, [&](Iter& it) {
							GAIA_PROF_SCOPE(query_func);
							func(it);
						});
					} else if constexpr (std::is_invocable_v<Func, IterDisabled&>) {
						run_query_on_chunks<IterDisabled>(queryInfo, [&](IterDisabled& it) {
							GAIA_PROF_SCOPE(query_func);
							func(it);
						});
					} else
						each(queryInfo, func);
				}

				template <typename Func, bool FuncEnabled = UseCaching, typename std::enable_if<FuncEnabled>::type* = nullptr>
				void each(QueryId queryId, Func func) {
					// Make sure the query was created by World.query()
					GAIA_ASSERT(m_storage.m_queryCache != nullptr);
					GAIA_ASSERT(queryId != QueryIdBad);

					auto& queryInfo = m_storage.m_queryCache->get(queryId);
					each(queryInfo, func);
				}

				//!	Returns true or false depending on whether there are any entities matching the query.
				//!	\warning Only use if you only care if there are any entities matching the query.
				//!					 The result is not cached and repeated calls to the function might be slow.
				//!					 If you already called arr(), checking if it is empty is preferred.
				//!					 Use empty() instead of calling count()==0.
				//!	\return True if there are any entities matching the query. False otherwise.
				bool empty(Constraints constraints = Constraints::EnabledOnly) {
					auto& queryInfo = fetch();
					const bool hasFilters = queryInfo.has_filters();

					if (hasFilters) {
						switch (constraints) {
							case Constraints::EnabledOnly:
								return empty_inter<true, Iter>(queryInfo);
							case Constraints::DisabledOnly:
								return empty_inter<true, IterDisabled>(queryInfo);
							case Constraints::AcceptAll:
								return empty_inter<true, IterAll>(queryInfo);
						}
					} else {
						switch (constraints) {
							case Constraints::EnabledOnly:
								return empty_inter<false, Iter>(queryInfo);
							case Constraints::DisabledOnly:
								return empty_inter<false, IterDisabled>(queryInfo);
							case Constraints::AcceptAll:
								return empty_inter<false, IterAll>(queryInfo);
						}
					}

					return true;
				}

				//! Calculates the number of entities matching the query
				//! \warning Only use if you only care about the number of entities matching the query.
				//!          The result is not cached and repeated calls to the function might be slow.If you already called
				//!          arr(), use the size provided by the array.Use empty() instead of calling count() == 0.
				//! \return The number of matching entities
				uint32_t count(Constraints constraints = Constraints::EnabledOnly) {
					auto& queryInfo = fetch();
					uint32_t entCnt = 0;

					const bool hasFilters = queryInfo.has_filters();
					if (hasFilters) {
						switch (constraints) {
							case Constraints::EnabledOnly: {
								entCnt += count_inter<true, Iter>(queryInfo);
							} break;
							case Constraints::DisabledOnly: {
								entCnt += count_inter<true, IterDisabled>(queryInfo);
							} break;
							case Constraints::AcceptAll: {
								entCnt += count_inter<true, IterAll>(queryInfo);
							} break;
						}
					} else {
						switch (constraints) {
							case Constraints::EnabledOnly: {
								entCnt += count_inter<false, Iter>(queryInfo);
							} break;
							case Constraints::DisabledOnly: {
								entCnt += count_inter<false, IterDisabled>(queryInfo);
							} break;
							case Constraints::AcceptAll: {
								entCnt += count_inter<false, IterAll>(queryInfo);
							} break;
						}
					}

					return entCnt;
				}

				//! Appends all components or entities matching the query to the output array
				//! \tparam outArray Container storing entities or components
				//! \param constraints QueryImpl constraints
				//! \return Array with entities or components
				template <typename Container>
				void arr(Container& outArray, Constraints constraints = Constraints::EnabledOnly) {
					const auto entCnt = count();
					if (entCnt == 0)
						return;

					outArray.reserve(entCnt);
					auto& queryInfo = fetch();

					const bool hasFilters = queryInfo.has_filters();
					if (hasFilters) {
						switch (constraints) {
							case Constraints::EnabledOnly:
								arr_inter<true, Iter>(queryInfo, outArray);
								break;
							case Constraints::DisabledOnly:
								arr_inter<true, IterDisabled>(queryInfo, outArray);
								break;
							case Constraints::AcceptAll:
								arr_inter<true, IterAll>(queryInfo, outArray);
								break;
						}
					} else {
						switch (constraints) {
							case Constraints::EnabledOnly:
								arr_inter<false, Iter>(queryInfo, outArray);
								break;
							case Constraints::DisabledOnly:
								arr_inter<false, IterDisabled>(queryInfo, outArray);
								break;
							case Constraints::AcceptAll:
								arr_inter<false, IterAll>(queryInfo, outArray);
								break;
						}
					}
				}

				//!
				void diag() {
					// Make sure matching happened
					auto& info = fetch();
					GAIA_LOG_N("DIAG Query %u.%u [%c]", id(), gen(), UseCaching ? 'C' : 'U');
					for (const auto* pArchetype: info)
						Archetype::diag_basic_info(*m_storage.world(), *pArchetype);
					GAIA_LOG_N("END DIAG Query");
				}
			};
		} // namespace detail

		using Query = typename detail::QueryImpl<true>;
		using QueryUncached = typename detail::QueryImpl<false>;
	} // namespace ecs
} // namespace gaia

/*** End of inlined file: query.h ***/

namespace gaia {
	namespace ecs {
		class SystemBuilder;

		class GAIA_API World final {
			friend class ECSSystem;
			friend class ECSSystemManager;
			friend class CommandBuffer;
			friend void* AllocateChunkMemory(World& world);
			friend void ReleaseChunkMemory(World& world, void* mem);

			using TFunc_Void_With_Entity = void(Entity);
			static void func_void_with_entity([[maybe_unused]] Entity entity) {}

			using EntityNameLookupKey = core::StringLookupKey<256>;
			using PairMap = cnt::map<EntityLookupKey, cnt::set<EntityLookupKey>>;

			//! Cache of components
			ComponentCache m_compCache;
			//! Cache of queries
			QueryCache m_queryCache;
			//! A map of [Query*, Buffer].
			//! Contains serialization buffers used by queries during their initialization.
			//! Kept here because it's only necessary for query initialization and would just
			//! take space on a query almost 100% of the time with no purpose at all.
			//! Records removed as soon as the query is compiled.
			QuerySerMap m_querySerMap;
			uint32_t m_nextQuerySerId = 0;

			//! Map of entity -> archetypes
			EntityToArchetypeMap m_entityToArchetypeMap;
			//! Map of [entity; Is relationship targets].
			//!   w.as(herbivore, animal);
			//!   w.as(rabbit, herbivore);
			//!   w.as(hare, herbivore);
			//! -->
			//!   herbivore -> {animal}
			//!   rabbit -> {herbivore}
			//!   hare -> {herbivore}
			PairMap m_entityToAsTargets;
			//! Map of [entity; Is relationship relations]
			//!   w.as(herbivore, animal);
			//!   w.as(rabbit, herbivore);
			//!   w.as(hare, herbivore);
			//!-->
			//!   animal -> {herbivore}
			//!   herbivore -> {rabbit, hare}
			PairMap m_entityToAsRelations;
			//! Map of relation -> targets
			PairMap m_relationsToTargets;
			//! Map of target -> relations
			PairMap m_targetsToRelations;

			//! Array of all archetypes
			ArchetypeDArray m_archetypes;
			//! Map of archetypes identified by their component hash code
			ArchetypeMapByHash m_archetypesByHash;
			//! Map of archetypes identified by their ID
			ArchetypeMapById m_archetypesById;

			//! Pointer to the root archetype
			Archetype* m_pRootArchetype = nullptr;
			//! Entity archetype
			Archetype* m_pEntityArchetype = nullptr;
			//! Pointer to the component archetype
			Archetype* m_pCompArchetype = nullptr;
			//! Id assigned to the next created archetype
			ArchetypeId m_nextArchetypeId = 0;

			//! Entity records
			EntityContainers m_recs;

			//! Name to entity mapping
			cnt::map<EntityNameLookupKey, Entity> m_nameToEntity;

			//! Archetypes requested to be deleted
			cnt::set<ArchetypeLookupKey> m_reqArchetypesToDel;
			//! Entities requested to be deleted
			cnt::set<EntityLookupKey> m_reqEntitiesToDel;

			//! Query used to iterate systems
			ecs::Query m_systemsQuery;

			//! Local set of entities to delete
			cnt::set<EntityLookupKey> m_entitiesToDel;
			//! Array of chunks to delete
			cnt::darray<ArchetypeChunkPair> m_chunksToDel;
			//! Array of archetypes to delete
			ArchetypeDArray m_archetypesToDel;
			//! Index of the last defragmented archetype in the archetype list
			uint32_t m_defragLastArchetypeIdx = 0;
			//! Maximum number of entities to defragment per world tick
			uint32_t m_defragEntitiesPerTick = 100;

			//! With every structural change world version changes
			uint32_t m_worldVersion = 0;

		public:
			//! Provides a query set up to work with the parent world.
			//! \tparam UseCache If true, results of the query are cached
			//! \return Valid query object
			template <bool UseCache = true>
			auto query() {
				if constexpr (UseCache) {
					Query q(
							*const_cast<World*>(this), m_queryCache,
							//
							m_nextArchetypeId, m_worldVersion, m_archetypesById, m_entityToArchetypeMap, m_archetypes);
					return q;
				} else {
					QueryUncached q(
							*const_cast<World*>(this),
							//
							m_nextArchetypeId, m_worldVersion, m_archetypesById, m_entityToArchetypeMap, m_archetypes);
					return q;
				}
			}

			//----------------------------------------------------------------------

			GAIA_NODISCARD EntityContainer& fetch(Entity entity) {
				// Valid entity
				GAIA_ASSERT(valid(entity));
				// Wildcard pairs are not a real entity so we can't accept them
				GAIA_ASSERT(!entity.pair() || !is_wildcard(entity));
				return m_recs[entity];
			}

			GAIA_NODISCARD const EntityContainer& fetch(Entity entity) const {
				// Valid entity
				GAIA_ASSERT(valid(entity));
				// Wildcard pairs are not a real entity so we can't accept them
				GAIA_ASSERT(!entity.pair() || !is_wildcard(entity));
				return m_recs[entity];
			}

			GAIA_NODISCARD static bool is_req_del(const EntityContainer& ec) {
				if ((ec.flags & EntityContainerFlags::DeleteRequested) != 0)
					return true;
				if (ec.pArchetype != nullptr && ec.pArchetype->is_req_del())
					return true;

				return false;
			}

			struct EntityBuilder final {
				friend class World;

				World& m_world;
				Archetype* m_pArchetype;
				Entity m_entity;

				EntityBuilder(World& world, Entity entity):
						m_world(world), m_pArchetype(world.fetch(entity).pArchetype), m_entity(entity) {}

				EntityBuilder(const EntityBuilder&) = default;
				EntityBuilder(EntityBuilder&&) = delete;
				EntityBuilder& operator=(const EntityBuilder&) = delete;
				EntityBuilder& operator=(EntityBuilder&&) = delete;

				~EntityBuilder() {
					commit();
				}

				//! Commits all gathered changes and performs an archetype movement.
				//! \warning Once called, the object is returned to its default state (as if no add/remove was ever called).
				void commit() {
					if (m_pArchetype == nullptr)
						return;

					// Now that we have the final archetype move the entity to it
					m_world.move_entity(m_entity, *m_pArchetype);

					// Finalize the builder by reseting the archetype pointer
					m_pArchetype = nullptr;
				}

				//! Prepares an archetype movement by following the "add" edge of the current archetype.
				//! \param entity Added entity
				EntityBuilder& add(Entity entity) {
					GAIA_PROF_SCOPE(EntityBuilder::add);
					GAIA_ASSERT(m_world.valid(m_entity));
					GAIA_ASSERT(m_world.valid(entity));

					add_inter(entity);
					return *this;
				}

				//! Prepares an archetype movement by following the "add" edge of the current archetype.
				//! \param pair Relationship pair
				EntityBuilder& add(Pair pair) {
					GAIA_PROF_SCOPE(EntityBuilder::add);
					GAIA_ASSERT(m_world.valid(m_entity));
					GAIA_ASSERT(m_world.valid(pair.first()));
					GAIA_ASSERT(m_world.valid(pair.second()));

					add_inter(pair);
					return *this;
				}

				//! Shortcut for add(Pair(Is, entityBase)).
				//! Effectively makes an entity inherit from \param entityBase
				EntityBuilder& as(Entity entityBase) {
					return add(Pair(Is, entityBase));
				}

				//! Check if \param entity inherits from \param entityBase
				//! \return True if entity inherits from entityBase
				GAIA_NODISCARD bool as(Entity entity, Entity entityBase) const {
					return static_cast<const World&>(m_world).is(entity, entityBase);
				}

				//! Shortcut for add(Pair(ChildOf, parent))
				EntityBuilder& child(Entity parent) {
					return add(Pair(ChildOf, parent));
				}

				//! Takes care of registering the component \tparam T
				template <typename T>
				Entity register_component() {
					if constexpr (is_pair<T>::value) {
						const auto rel = m_world.add<typename T::rel>().entity;
						const auto tgt = m_world.add<typename T::tgt>().entity;
						const Entity ent = Pair(rel, tgt);
						add_inter(ent);
						return ent;
					} else {
						return m_world.add<T>().entity;
					}
				}

				template <typename... T>
				EntityBuilder& add() {
					(verify_comp<T>(), ...);
					(add(register_component<T>()), ...);
					return *this;
				}

				//! Prepares an archetype movement by following the "del" edge of the current archetype.
				//! \param entity Removed entity
				EntityBuilder& del(Entity entity) {
					GAIA_PROF_SCOPE(EntityBuilder::del);
					GAIA_ASSERT(m_world.valid(m_entity));
					GAIA_ASSERT(m_world.valid(entity));
					del_inter(entity);
					return *this;
				}

				//! Prepares an archetype movement by following the "del" edge of the current archetype.
				//! \param pair Relationship pair
				EntityBuilder& del(Pair pair) {
					GAIA_PROF_SCOPE(EntityBuilder::add);
					GAIA_ASSERT(m_world.valid(m_entity));
					GAIA_ASSERT(m_world.valid(pair.first()));
					GAIA_ASSERT(m_world.valid(pair.second()));
					del_inter(pair);
					return *this;
				}

				template <typename... T>
				EntityBuilder& del() {
					(verify_comp<T>(), ...);
					(del(register_component<T>()), ...);
					return *this;
				}

			private:
				bool handle_add_entity(Entity entity) {
					cnt::sarray_ext<Entity, ChunkHeader::MAX_COMPONENTS> targets;

					const auto& ecMain = m_world.fetch(entity);

					// Handle entity combinations that can't be together
					if ((ecMain.flags & EntityContainerFlags::HasCantCombine) != 0) {
						m_world.targets(entity, CantCombine, [&targets](Entity target) {
							targets.push_back(target);
						});
						for (auto e: targets) {
							if (m_pArchetype->has(e)) {
#if GAIA_ASSERT_ENABLED
								GAIA_ASSERT2(false, "Trying to add an entity which can't be combined with the source");
								print_archetype_entities(m_world, *m_pArchetype, entity, true);
#endif
								return false;
							}
						}
					}

					// Handle exclusivity
					if (entity.pair()) {
						// Check if (rel, tgt)'s rel part is exclusive
						const auto& ecRel = m_world.m_recs.entities[entity.id()];
						if ((ecRel.flags & EntityContainerFlags::IsExclusive) != 0) {
							auto rel = Entity(entity.id(), ecRel.gen, (bool)ecRel.ent, (bool)ecRel.pair, (EntityKind)ecRel.kind);
							auto tgt = m_world.get(entity.gen());

							// Make sure to remove the (rel, tgt0) so only the new (rel, tgt1) remains.
							// However, before that we need to make sure there only exists one target at most.
							targets.clear();
							m_world.targets_if(m_entity, rel, [&targets](Entity target) {
								targets.push_back(target);
								// Stop the moment we have more than 1 target. This kind of scenario is not supported
								// and can happen only if Exclusive is added after multiple relationships already exist.
								return targets.size() < 2;
							});

							const auto sz = targets.size();
							if (sz > 1) {
#if GAIA_ASSERT_ENABLED
								GAIA_ASSERT2(
										false, "Trying to add a pair with exclusive relationship but there are multiple targets present. "
													 "Make sure to add the Exclusive property before any relationships with it are created.");
								print_archetype_entities(m_world, *m_pArchetype, entity, true);
#endif
								return false;
							}

							// Remove the previous relationship if possible.
							// We avoid self-removal.
							const auto tgtNew = *targets.begin();
							if (sz == 1 && tgt != tgtNew) {
								// Exclusive relationship replaces the previous one.
								// We need to check if the old one can be removed.
								// This is what del_inter does on the inside.
								// It first checks if entity can be deleted and calls handle_del afterwards.
								if (!can_del(entity)) {
#if GAIA_ASSERT_ENABLED
									GAIA_ASSERT2(
											false, "Trying to replace an exclusive relationship but the entity which is getting removed has "
														 "dependencies.");
									print_archetype_entities(m_world, *m_pArchetype, entity, true);
#endif
									return false;
								}

								handle_del(ecs::Pair(rel, tgtNew));
							}
						}
					}

					// Handle requirements
					{
						targets.clear();
						m_world.targets(entity, Requires, [&targets](Entity target) {
							targets.push_back(target);
						});

						for (auto e: targets) {
							auto* pArchetype = m_pArchetype;
							handle_add(e);
							if (m_pArchetype != pArchetype)
								handle_add_entity(e);
						}
					}

					return true;
				}

				bool has_Requires_tgt(Entity entity) const {
					// Don't allow to delete entity if something in the archetype requires it
					auto ids = m_pArchetype->ids_view();
					for (auto e: ids) {
						if (m_world.has(e, Pair(Requires, entity)))
							return true;
					}

					return false;
				}

				static void set_flag(EntityContainerFlagsType& flags, EntityContainerFlags flag, bool enable) {
					if (enable)
						flags |= flag;
					else
						flags &= ~flag;
				};

				void set_flag(Entity entity, EntityContainerFlags flag, bool enable) {
					auto& ec = m_world.fetch(entity);
					set_flag(ec.flags, flag, enable);
				};

				void try_set_flags(Entity entity, bool enable) {
					auto& ecMain = m_world.fetch(m_entity);
					try_set_CantCombine(ecMain, entity, enable);

					auto& ec = m_world.fetch(entity);
					try_set_Is(ec, entity, enable);
					try_set_IsExclusive(ecMain, entity, enable);
					try_set_IsSingleton(ecMain, entity, enable);
					try_set_OnDelete(ecMain, entity, enable);
					try_set_OnDeleteTarget(entity, enable);
				}

				void try_set_Is(EntityContainer& ec, Entity entity, bool enable) {
					if (!entity.pair() || entity.id() != Is.id())
						return;

					set_flag(ec.flags, EntityContainerFlags::HasAliasOf, enable);
				}

				void try_set_CantCombine(EntityContainer& ec, Entity entity, bool enable) {
					if (!entity.pair() || entity.id() != CantCombine.id())
						return;

					GAIA_ASSERT(entity != m_entity);

					// Setting the flag can be done right away.
					// One bit can only contain information about one pair but there
					// can be any amount of CanCombine pairs formed with an entity.
					// Therefore, when resetting the flag, we first need to check if there
					// are any other targets with this flag set and only reset the flag
					// if there is only one present.
					if (enable)
						set_flag(ec.flags, EntityContainerFlags::HasCantCombine, true);
					else if ((ec.flags & EntityContainerFlags::HasCantCombine) != 0) {
						uint32_t targets = 0;
						m_world.targets(m_entity, CantCombine, [&targets]() {
							++targets;
						});
						if (targets == 1)
							set_flag(ec.flags, EntityContainerFlags::HasCantCombine, false);
					}
				}

				void try_set_IsExclusive(EntityContainer& ec, Entity entity, bool enable) {
					if (entity.pair() || entity.id() != Exclusive.id())
						return;

					set_flag(ec.flags, EntityContainerFlags::IsExclusive, enable);
				}

				void try_set_OnDeleteTarget(Entity entity, bool enable) {
					if (!entity.pair())
						return;

					const auto rel = m_world.get(entity.id());
					const auto tgt = m_world.get(entity.gen());

					// Adding a pair to an entity with OnDeleteTarget relationship.
					// We need to update the target entity's flags.
					if (m_world.has(rel, Pair(OnDeleteTarget, Delete)))
						set_flag(tgt, EntityContainerFlags::OnDeleteTarget_Delete, enable);
					else if (m_world.has(rel, Pair(OnDeleteTarget, Remove)))
						set_flag(tgt, EntityContainerFlags::OnDeleteTarget_Remove, enable);
					else if (m_world.has(rel, Pair(OnDeleteTarget, Error)))
						set_flag(tgt, EntityContainerFlags::OnDeleteTarget_Error, enable);
				}

				void try_set_OnDelete(EntityContainer& ec, Entity entity, bool enable) {
					if (entity == Pair(OnDelete, Delete))
						set_flag(ec.flags, EntityContainerFlags::OnDelete_Delete, enable);
					else if (entity == Pair(OnDelete, Remove))
						set_flag(ec.flags, EntityContainerFlags::OnDelete_Remove, enable);
					else if (entity == Pair(OnDelete, Error))
						set_flag(ec.flags, EntityContainerFlags::OnDelete_Error, enable);
				}

				void try_set_IsSingleton(EntityContainer& ec, Entity entity, bool enable) {
					const bool isSingleton = enable && m_entity == entity;
					set_flag(ec.flags, EntityContainerFlags::IsSingleton, isSingleton);
				}

				void handle_DependsOn(Entity entity, bool enable) {
					(void)entity;
					(void)enable;
					// auto& ec = m_world.fetch(entity);
					// if (enable) {
					// 	// Calculate the depth in the dependency tree
					// 	uint32_t depth = 1;

					// 	auto e = entity;
					// 	if (m_world.valid(e)) {
					// 		while (true) {
					// 			auto tgt = m_world.target(e, DependsOn);
					// 			if (tgt == EntityBad)
					// 				break;

					// 			++depth;
					// 			e = tgt;
					// 		}
					// 	}
					// 	ec.depthDependsOn = (uint8_t)depth;

					// 	// Update depth for all entities depending on this one
					// 	auto q = m_world.query<false>();
					// 	q.all(ecs::Pair(DependsOn, m_entity)) //
					// 			.each([&](Entity dependingEntity) {
					// 				auto& ecDependingEntity = m_world.fetch(dependingEntity);
					// 				ecDependingEntity.depthDependsOn += (uint8_t)depth;
					// 			});
					// } else {
					// 	// Update depth for all entities depending on this one
					// 	auto q = m_world.query<false>();
					// 	q.all(ecs::Pair(DependsOn, m_entity)) //
					// 			.each([&](Entity dependingEntity) {
					// 				auto& ecDependingEntity = m_world.fetch(dependingEntity);
					// 				ecDependingEntity.depthDependsOn -= ec.depthDependsOn;
					// 			});

					// 	// Reset the depth
					// 	ec.depthDependsOn = 0;
					// }
				}

				template <bool IsBootstrap = false>
				bool handle_add(Entity entity) {
#if GAIA_ASSERT_ENABLED
					World::verify_add(m_world, *m_pArchetype, m_entity, entity);
#endif

					// Don't add the same entity twice
					if (m_pArchetype->has(entity))
						return false;

					try_set_flags(entity, true);

					// Update the Is relationship base counter if necessary
					if (entity.pair() && entity.id() == Is.id()) {
						auto e = m_world.get(entity.gen());

						EntityLookupKey entityKey(m_entity);
						EntityLookupKey eKey(e);

						// m_entity -> {..., e}
						auto& entity_to_e = m_world.m_entityToAsTargets[entityKey];
						entity_to_e.insert(eKey);
						// e -> {..., m_entity}
						auto& e_to_entity = m_world.m_entityToAsRelations[eKey];
						e_to_entity.insert(entityKey);

						// Make sure the relation entity is registered as archetype so queries can find it
						// auto& ec = m_world.fetch(tgt);
						// m_world.add_entity_archetype_pair(m_entity, ec.pArchetype);

						// Cached queries might need to be invalidated.
						m_world.invalidate_queries_for_entity({Is, e});
					}

					m_pArchetype = m_world.foc_archetype_add(m_pArchetype, entity);

					if constexpr (!IsBootstrap) {
						handle_DependsOn(entity, true);
					}

					return true;
				}

				void handle_del(Entity entity) {
#if GAIA_ASSERT_ENABLED
					World::verify_del(m_world, *m_pArchetype, m_entity, entity);
#endif

					// Don't delete what has not beed added
					if (!m_pArchetype->has(entity))
						return;

					try_set_flags(entity, false);
					handle_DependsOn(entity, false);

					// Update the Is relationship base counter if necessary
					if (entity.pair() && entity.id() == Is.id()) {
						auto e = m_world.get(entity.gen());

						EntityLookupKey entityKey(m_entity);
						EntityLookupKey eKey(e);

						// Cached queries might need to be invalidated.
						m_world.invalidate_queries_for_entity({Is, e});

						// m_entity -> {..., e}
						{
							const auto it = m_world.m_entityToAsTargets.find(entityKey);
							GAIA_ASSERT(it != m_world.m_entityToAsTargets.end());
							auto& set = it->second;
							GAIA_ASSERT(!set.empty());
							set.erase(eKey);

							// Remove the record if it is not referenced anymore
							if (set.empty())
								m_world.m_entityToAsTargets.erase(it);
						}

						// e -> {..., m_entity}
						{
							const auto it = m_world.m_entityToAsRelations.find(eKey);
							GAIA_ASSERT(it != m_world.m_entityToAsRelations.end());
							auto& set = it->second;
							GAIA_ASSERT(!set.empty());
							set.erase(entityKey);

							// Remove the record if it is not referenced anymore
							if (set.empty())
								m_world.m_entityToAsRelations.erase(it);
						}
					}

					m_pArchetype = m_world.foc_archetype_del(m_pArchetype, entity);
				}

				void add_inter(Entity entity) {
					GAIA_ASSERT(!is_wildcard(entity));

					if (entity.pair()) {
						// Make sure the entity container record exists if it is a pair
						m_world.assign_pair(entity, *m_world.m_pEntityArchetype);
					}

					if (!handle_add_entity(entity))
						return;

					handle_add(entity);
				}

				void add_inter_init(Entity entity) {
					GAIA_ASSERT(!is_wildcard(entity));

					if (entity.pair()) {
						// Make sure the entity container record exists if it is a pair
						m_world.assign_pair(entity, *m_world.m_pEntityArchetype);
					}

					if (!handle_add_entity(entity))
						return;

					handle_add<true>(entity);
				}

				GAIA_NODISCARD bool can_del(Entity entity) const {
					if (has_Requires_tgt(entity))
						return false;

					return true;
				}

				bool del_inter(Entity entity) {
					if (!can_del(entity))
						return false;

					handle_del(entity);
					return true;
				}
			};

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

			//----------------------------------------------------------------------

			GAIA_NODISCARD ComponentCache& comp_cache_mut() {
				return m_compCache;
			}
			GAIA_NODISCARD const ComponentCache& comp_cache() const {
				return m_compCache;
			}

			GAIA_NODISCARD QuerySerMap& query_ser_map() {
				return m_querySerMap;
			}

			//----------------------------------------------------------------------

			//! Checks if \param entity is valid.
			//! \return True if the entity is valid. False otherwise.
			GAIA_NODISCARD bool valid(Entity entity) const {
				return entity.pair() //
									 ? valid_pair(entity)
									 : valid_entity(entity);
			}

			//----------------------------------------------------------------------

			//! Returns the entity located at the index \param id
			//! \return Entity
			GAIA_NODISCARD Entity get(EntityId id) const {
				GAIA_ASSERT(valid_entity_id(id));

				const auto& ec = m_recs.entities[id];
				return Entity(id, ec.gen, (bool)ec.ent, (bool)ec.pair, (EntityKind)ec.kind);
			}

			template <typename T>
			GAIA_NODISCARD Entity get() const {
				static_assert(!is_pair<T>::value, "Pairs can't be registered as components");

				using CT = component_type_t<T>;
				using FT = typename CT::TypeFull;

				const auto* pItem = comp_cache().find<FT>();
				GAIA_ASSERT(pItem != nullptr);
				return pItem->entity;
			}

			//----------------------------------------------------------------------

			//! Starts a bulk add/remove operation on \param entity.
			//! \param entity Entity
			//! \return EntityBuilder
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			EntityBuilder build(Entity entity) {
				return EntityBuilder(*this, entity);
			}

			//! Creates a new empty entity
			//! \param kind Entity kind
			//! \return New entity
			GAIA_NODISCARD Entity add(EntityKind kind = EntityKind::EK_Gen) {
				return add(*m_pEntityArchetype, true, false, kind);
			}

			//! Creates \param count new empty entities
			//! \param func Functor to execute every time an entity is added
			template <typename Func = TFunc_Void_With_Entity>
			void add_n(uint32_t count, Func func = func_void_with_entity) {
				add_entity_n(*m_pEntityArchetype, count, func);
			}

			//! Creates \param count of entities of the same archetype as \param entity.
			//! \param func Functor to execute every time an entity is added
			//! \note Similar to copy_n, but keeps component values uninitialized or default-initialized
			//!       if they provide a constructor
			template <typename Func = TFunc_Void_With_Entity>
			void add_n(Entity entity, uint32_t count, Func func = func_void_with_entity) {
				auto& ec = m_recs.entities[entity.id()];

				GAIA_ASSERT(ec.pChunk != nullptr);
				GAIA_ASSERT(ec.pArchetype != nullptr);

				add_entity_n(*ec.pArchetype, count, func);
			}

			//! Creates a new component if not found already.
			//! \param kind Component kind
			//! \return Component cache item of the component
			template <typename T>
			GAIA_NODISCARD const ComponentCacheItem& add() {
				static_assert(!is_pair<T>::value, "Pairs can't be registered as components");

				using CT = component_type_t<T>;
				using FT = typename CT::TypeFull;
				constexpr auto kind = CT::Kind;

				const auto* pItem = comp_cache().find<FT>();
				if (pItem != nullptr)
					return *pItem;

				const auto entity = add(*m_pCompArchetype, false, false, kind);

				const auto& item = comp_cache_mut().add<FT>(entity);
				sset<Component>(item.entity) = item.comp;

				// Make sure the default component entity name points to the cache item name.
				// The name is deleted when the component cache item is deleted.
				name_raw(item.entity, item.name.str(), item.name.len());

				return item;
			}

			//! Attaches entity \param object to entity \param entity.
			//! \param entity Source entity
			//! \param object Added entity
			//! \warning It is expected both \param entity and \param object are valid. Undefined behavior otherwise.
			void add(Entity entity, Entity object) {
				EntityBuilder(*this, entity).add(object);
			}

			//! Creates a new entity relationship pair
			//! \param entity Source entity
			//! \param pair Pair
			//! \warning It is expected both \param entity and the entities forming the relationship are valid.
			//!          Undefined behavior otherwise.
			void add(Entity entity, Pair pair) {
				EntityBuilder(*this, entity).add(pair);
			}

			//! Attaches a new component \tparam T to \param entity.
			//! \tparam T Component
			//! \param entity Entity
			//! \warning It is expected the component is not present on \param entity yet. Undefined behavior otherwise.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			template <typename T>
			void add(Entity entity) {
				EntityBuilder(*this, entity).add<T>();
			}

			//! Attaches \param object to \param entity. Also sets its value.
			//! \param object Object
			//! \param entity Entity
			//! \param value Value to set for the object
			//! \warning It is expected the component is not present on \param entity yet. Undefined behavior otherwise.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			//! \warning It is expected \param object is valid. Undefined behavior otherwise.
			template <typename T>
			void add(Entity entity, Entity object, T&& value) {
				static_assert(core::is_raw_v<T>);

				EntityBuilder(*this, entity).add(object);

				const auto& ec = fetch(entity);
				// Make sure the idx is 0 for unique entities
				const auto idx = uint16_t(ec.row * (1U - (uint32_t)object.kind()));
				ComponentSetter{{ec.pChunk, idx}}.set(object, GAIA_FWD(value));
			}

			//! Attaches a new component \tparam T to \param entity. Also sets its value.
			//! \tparam T Component
			//! \param entity Entity
			//! \param value Value to set for the component
			//! \warning It is expected the component is not present on \param entity yet. Undefined behavior otherwise.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			template <typename T, typename U = typename actual_type_t<T>::Type>
			void add(Entity entity, U&& value) {
				EntityBuilder(*this, entity).add<T>();

				const auto& ec = m_recs.entities[entity.id()];
				// Make sure the idx is 0 for unique entities
				constexpr auto kind = (uint32_t)actual_type_t<T>::Kind;
				const auto idx = uint16_t(ec.row * (1U - (uint32_t)kind));
				ComponentSetter{{ec.pChunk, idx}}.set<T>(GAIA_FWD(value));
			}

			//----------------------------------------------------------------------

			//! Creates a new entity by cloning an already existing one.
			//! \param srcEntity Entity to clone
			//! \return New entity
			//! \warning It is expected \param srcEntity is valid. Undefined behavior otherwise.
			//! \warning If EntityDesc is present on \param srcEntity, it is not copied because names are
			//!          expected to be unique. Instead, the copied entity will be a part of an archetype
			//!          without EntityDesc and any calls to World::name(copiedEntity) will return nullptr.
			GAIA_NODISCARD Entity copy(Entity srcEntity) {
				GAIA_ASSERT(!srcEntity.pair());
				GAIA_ASSERT(valid(srcEntity));

				auto& ec = m_recs.entities[srcEntity.id()];
				GAIA_ASSERT(ec.pChunk != nullptr);
				GAIA_ASSERT(ec.pArchetype != nullptr);

				auto* pDstArchetype = ec.pArchetype;

				// Names have to be unique so if we see that EntityDesc is present during copy
				// we navigate towards a version of the archetype without the EntityDesc.
				if (pDstArchetype->has<EntityDesc>()) {
					pDstArchetype = foc_archetype_del(pDstArchetype, GAIA_ID(EntityDesc));

					const auto dstEntity = add(*pDstArchetype, srcEntity.entity(), srcEntity.pair(), srcEntity.kind());
					auto& ecDst = m_recs.entities[dstEntity.id()];

					Chunk::copy_foreign_entity_data(ec.pChunk, ec.row, ecDst.pChunk, ecDst.row);
					return dstEntity;
				}

				// No description associated with the entity, direct copy is possible
				const auto dstEntity = add(*pDstArchetype, srcEntity.entity(), srcEntity.pair(), srcEntity.kind());
				Chunk::copy_entity_data(srcEntity, dstEntity, m_recs);
				return dstEntity;
			}

			//! Creates \param count new entities by cloning an already existing one.
			//! \param entity Entity to clone
			//! \param count Number of clones to make
			//! \param func void(Entity copy) functor executed every time a copy is created
			//! \warning It is expected \param entity is valid generic entity. Undefined behavior otherwise.
			//! \warning If EntityDesc is present on \param srcEntity, it is not copied because names are
			//!          expected to be unique. Instead, the copied entity will be a part of an archetype
			//!          without EntityDesc and any calls to World::name(copiedEntity) will return nullptr.
			template <typename Func = TFunc_Void_With_Entity>
			void copy_n(Entity entity, uint32_t count, Func func = func_void_with_entity) {
				GAIA_ASSERT(!entity.pair());
				GAIA_ASSERT(valid(entity));

				auto& ec = m_recs.entities[entity.id()];

				GAIA_ASSERT(ec.pChunk != nullptr);
				GAIA_ASSERT(ec.pArchetype != nullptr);

				auto* pSrcChunk = ec.pChunk;

				auto* pDstArchetype = ec.pArchetype;
				if (pDstArchetype->has<EntityDesc>()) {
					pDstArchetype = foc_archetype_del(pDstArchetype, GAIA_ID(EntityDesc));

					// Entities array might get reallocated after m_recs.entities.alloc
					// so instead of fetching the container again we simply cache the row
					// of our source entity.
					const auto srcRow = ec.row;

					EntityContainerCtx ctx{true, false, EntityKind::EK_Gen};

					uint32_t left = count;
					do {
						auto* pDstChunk = pDstArchetype->foc_free_chunk();
						const uint32_t originalChunkSize = pDstChunk->size();
						const uint32_t freeSlotsInChunk = pDstChunk->capacity() - originalChunkSize;
						const uint32_t toCreate = core::get_min(freeSlotsInChunk, left);

						GAIA_FOR(toCreate) {
							const auto entityNew = m_recs.entities.alloc(&ctx);
							auto& ecNew = m_recs.entities[entityNew.id()];
							store_entity(ecNew, entityNew, pDstArchetype, pDstChunk);

#if GAIA_ASSERT_ENABLED
							GAIA_ASSERT(ecNew.pChunk == pDstChunk);
							auto entityExpected = pDstChunk->entity_view()[ecNew.row];
							GAIA_ASSERT(entityExpected == entityNew);
#endif

							Chunk::copy_foreign_entity_data(pSrcChunk, srcRow, pDstChunk, ecNew.row);
						}

						// Call functors
						{
							auto entities = pDstChunk->entity_view();
							GAIA_FOR2(originalChunkSize, pDstChunk->size()) func(entities[i]);
						}

						left -= toCreate;
					} while (left > 0);
				} else {
					pDstArchetype = ec.pArchetype;

					// Entities array might get reallocated after m_recs.entities.alloc
					// so instead of fetching the container again we simply cache the row
					// of our source entity.
					const auto srcRow = ec.row;

					EntityContainerCtx ctx{true, false, EntityKind::EK_Gen};

					uint32_t left = count;
					do {
						auto* pDstChunk = pDstArchetype->foc_free_chunk();
						const uint32_t originalChunkSize = pDstChunk->size();
						const uint32_t freeSlotsInChunk = pDstChunk->capacity() - originalChunkSize;
						const uint32_t toCreate = core::get_min(freeSlotsInChunk, left);

						GAIA_FOR(toCreate) {
							const auto entityNew = m_recs.entities.alloc(&ctx);
							auto& ecNew = m_recs.entities[entityNew.id()];
							store_entity(ecNew, entityNew, pDstArchetype, pDstChunk);

#if GAIA_ASSERT_ENABLED
							GAIA_ASSERT(ecNew.pChunk == pDstChunk);
							auto entityExpected = pDstChunk->entity_view()[ecNew.row];
							GAIA_ASSERT(entityExpected == entityNew);
#endif
						}

						// New entities were added, try updating the free chunk index
						pDstArchetype->try_update_free_chunk_idx();

						// Call constructors for the generic components on the newly added entity if necessary
						pDstChunk->call_gen_ctors(originalChunkSize, toCreate);

						// Copy data
						{
							GAIA_PROF_SCOPE(copy_n::copy_entity_data);

							auto srcRecs = pSrcChunk->comp_rec_view();

							// Copy generic component data from reference entity to our new entity
							GAIA_FOR(pSrcChunk->m_header.genEntities) {
								const auto& rec = srcRecs[i];
								if (rec.comp.size() == 0U)
									continue;

								const auto* pSrc = (const void*)pSrcChunk->comp_ptr(i);
								GAIA_FOR_(toCreate, rowOffset) {
									auto* pDst = (void*)pDstChunk->comp_ptr_mut(i);
									rec.pItem->copy(
											pDst, pSrc, originalChunkSize + rowOffset, srcRow, pDstChunk->capacity(), pSrcChunk->capacity());
								}
							}
						}

						// Call functors
						{
							auto entities = pDstChunk->entity_view();
							GAIA_FOR2(originalChunkSize, pDstChunk->size()) func(entities[i]);
						}

						left -= toCreate;
					} while (left > 0);
				}
			}

			//----------------------------------------------------------------------

			//! Removes an entity along with all data associated with it.
			//! \param entity Entity to delete
			void del(Entity entity) {
				if (!entity.pair()) {
					// Delete all relationships associated with this entity (if any)
					del_inter(Pair(entity, All));
					del_inter(Pair(All, entity));
				}

				del_inter(entity);
			}

			//! Removes an \param object from \param entity if possible.
			//! \param entity Entity to delete from
			//! \param object Entity to delete
			//! \warning It is expected both \param entity and \param object are valid. Undefined behavior otherwise.
			void del(Entity entity, Entity object) {
				EntityBuilder(*this, entity).del(object);
			}

			//! Removes an existing entity relationship pair
			//! \param entity Source entity
			//! \param pair Pair
			//! \warning It is expected both \param entity and the entities forming the relationship are valid.
			//!          Undefined behavior otherwise.
			void del(Entity entity, Pair pair) {
				EntityBuilder(*this, entity).del(pair);
			}

			//! Removes a component \tparam T from \param entity.
			//! \tparam T Component
			//! \param entity Entity
			//! \warning It is expected the component is present on \param entity. Undefined behavior otherwise.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			template <typename T>
			void del(Entity entity) {
				using CT = component_type_t<T>;
				using FT = typename CT::TypeFull;
				EntityBuilder(*this, entity).del<FT>();
			}

			//----------------------------------------------------------------------

			//! Shortcut for add(entity, Pair(Is, entityBase)
			void as(Entity entity, Entity entityBase) {
				// Make sure entityBase has an archetype of its own
				add(entityBase, entityBase);
				// Form the relationship
				add(entity, Pair(Is, entityBase));
			}

			//! Checks if \param entity inherits from \param entityBase.
			//! True if entity inherits from entityBase. False otherwise.
			GAIA_NODISCARD bool is(Entity entity, Entity entityBase) const {
				return is_inter<false>(entity, entityBase);
			}

			//! Checks if \param entity is located in \param entityBase.
			//! This is almost the same as "is" with the exception that false is returned
			//! if \param entity matches \param entityBase
			//! True if entity is located in entityBase. False otherwise.
			GAIA_NODISCARD bool in(Entity entity, Entity entityBase) const {
				return is_inter<true>(entity, entityBase);
			}

			GAIA_NODISCARD bool is_base(Entity target) const {
				GAIA_ASSERT(valid_entity(target));

				// Pairs are not supported
				if (target.pair())
					return false;

				const auto it = m_entityToAsRelations.find(EntityLookupKey(target));
				return it != m_entityToAsRelations.end();
			}

			//----------------------------------------------------------------------

			//! Shortcut for add(entity, Pair(ChildOf, parent)
			void child(Entity entity, Entity parent) {
				add(entity, Pair(ChildOf, parent));
			}

			//! Checks if \param entity is a child of \param parent
			//! True if entity is a child of parent. False otherwise.
			GAIA_NODISCARD bool child(Entity entity, Entity parent) const {
				return has(entity, Pair(ChildOf, parent));
			}

			//----------------------------------------------------------------------

			//! Starts a bulk set operation on \param entity.
			//! \param entity Entity
			//! \return ComponentSetter
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			//! \warning Undefined behavior if \param entity changes archetype after ComponentSetter is created.
			GAIA_NODISCARD ComponentSetter acc_mut(Entity entity) {
				GAIA_ASSERT(valid(entity));

				const auto& ec = m_recs.entities[entity.id()];
				return ComponentSetter{{ec.pChunk, ec.row}};
			}

			//! Sets the value of the component \tparam T on \param entity.
			//! \tparam T Component
			//! \param entity Entity
			//! \warning It is expected the component is present on \param entity. Undefined behavior otherwise.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			//! \warning Undefined behavior if \param entity changes archetype after ComponentSetter is created.
			template <typename T>
			GAIA_NODISCARD decltype(auto) set(Entity entity) {
				static_assert(!is_pair<T>::value);
				return acc_mut(entity).mut<T>();
			}

			//! Sets the value of the component \tparam T on \param entity without triggering a world version update.
			//! \tparam T Component
			//! \param entity Entity
			//! \warning It is expected the component is present on \param entity. Undefined behavior otherwise.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			//! \warning Undefined behavior if \param entity changes archetype after ComponentSetter is created.
			template <typename T>
			GAIA_NODISCARD decltype(auto) sset(Entity entity) {
				static_assert(!is_pair<T>::value);
				return acc_mut(entity).smut<T>();
			}

			//----------------------------------------------------------------------

			//! Starts a bulk get operation on \param entity.
			//! \param entity Entity
			//! \return ComponentGetter
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			//! \warning Undefined behavior if \param entity changes archetype after ComponentGetter is created.
			ComponentGetter acc(Entity entity) const {
				GAIA_ASSERT(valid(entity));

				const auto& ec = m_recs.entities[entity.id()];
				return ComponentGetter{ec.pChunk, ec.row};
			}

			//! Returns the value stored in the component \tparam T on \param entity.
			//! \tparam T Component
			//! \param entity Entity
			//! \return Value stored in the component.
			//! \warning It is expected the component is present on \param entity. Undefined behavior otherwise.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			//! \warning Undefined behavior if \param entity changes archetype after ComponentGetter is created.
			template <typename T>
			GAIA_NODISCARD decltype(auto) get(Entity entity) const {
				return acc(entity).get<T>();
			}

			//----------------------------------------------------------------------

			//! Checks if \param entity is currently used by the world.
			//! \return True if the entity is used. False otherwise.
			GAIA_NODISCARD bool has(Entity entity) const {
				// Pair
				if (entity.pair()) {
					if (is_wildcard(entity)) {
						if (!m_entityToArchetypeMap.contains(EntityLookupKey(entity)))
							return false;

						// If the pair is found, both entities forming it need to be found as well
						GAIA_ASSERT(has(get(entity.id())) && has(get(entity.gen())));

						return true;
					}

					const auto it = m_recs.pairs.find(EntityLookupKey(entity));
					if (it == m_recs.pairs.end())
						return false;

					const auto& ec = it->second;
					if (is_req_del(ec))
						return false;

#if GAIA_ASSERT_ENABLED
					// If the pair is found, both entities forming it need to be found as well
					GAIA_ASSERT(has(get(entity.id())) && has(get(entity.gen())));

					// Index of the entity must fit inside the chunk
					auto* pChunk = ec.pChunk;
					GAIA_ASSERT(pChunk != nullptr && ec.row < pChunk->size());
#endif

					return true;
				}

				// Regular entity
				{
					// Entity ID has to fit inside the entity array
					if (entity.id() >= m_recs.entities.size())
						return false;

					// Index of the entity must fit inside the chunk
					const auto& ec = m_recs.entities[entity.id()];
					if (is_req_del(ec))
						return false;

					auto* pChunk = ec.pChunk;
					return pChunk != nullptr && ec.row < pChunk->size();
				}
			}

			//! Checks if \param pair is currently used by the world.
			//! \return True if the entity is used. False otherwise.
			GAIA_NODISCARD bool has(Pair pair) const {
				return has((Entity)pair);
			}

			//! Tells if \param entity contains the entity \param object.
			//! \param entity Entity
			//! \param object Tested entity
			//! \return True if object is present on entity. False otherwise or if any of the entities is not valid.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			//! \warning Undefined behavior if \param entity changes archetype after ComponentSetter is created.
			GAIA_NODISCARD bool has(Entity entity, Entity object) const {
				const auto& ec = fetch(entity);
				if (is_req_del(ec))
					return false;

				const auto* pArchetype = ec.pArchetype;

				if (object.pair()) {
					// Early exit if there are no pairs on the archetype
					if (pArchetype->pairs() == 0)
						return false;

					EntityId rel = object.id();
					EntityId tgt = object.gen();

					// (*,*)
					if (rel == All.id() && tgt == All.id())
						return true;

					// (X,*)
					if (rel != All.id() && tgt == All.id()) {
						auto ids = pArchetype->ids_view();
						for (auto id: ids) {
							if (!id.pair())
								continue;
							if (id.id() == rel)
								return true;
						}

						return false;
					}

					// (*,X)
					if (rel == All.id() && tgt != All.id()) {
						auto ids = pArchetype->ids_view();
						for (auto id: ids) {
							if (!id.pair())
								continue;
							if (id.gen() == tgt)
								return true;
						}

						return false;
					}
				}

				return pArchetype->has(object);
			}

			//! Tells if \param entity contains \param pair.
			//! \param entity Entity
			//! \param pair Tested pair
			//! \return True if object is present on entity. False otherwise or if any of the entities is not valid.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			//! \warning Undefined behavior if \param entity changes archetype after ComponentSetter is created.
			GAIA_NODISCARD bool has(Entity entity, Pair pair) const {
				return has(entity, (Entity)pair);
			}

			//! Tells if \param entity contains the component \tparam T.
			//! \tparam T Component
			//! \param entity Entity
			//! \return True if the component is present on entity.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			//! \warning Undefined behavior if \param entity changes archetype after ComponentSetter is created.
			template <typename T>
			GAIA_NODISCARD bool has(Entity entity) const {
				GAIA_ASSERT(valid(entity));

				const auto& ec = m_recs.entities[entity.id()];
				if (is_req_del(ec))
					return false;

				return ec.pArchetype->has<T>();
			}

			//----------------------------------------------------------------------

			//! Assigns a \param name to \param entity. Ignored if used with pair.
			//! The string is copied and kept internally.
			//! \param entity Entity
			//! \param name A null-terminated string
			//! \param len String length. If zero, the length is calculated
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			//! \warning Name is expected to be unique. If it is not this function does nothing.
			void name(Entity entity, const char* name, uint32_t len = 0) {
				name_inter<true>(entity, name, len);
			}

			//! Assigns a \param name to \param entity. Ignored if used with pair.
			//! The string is NOT copied. Your are responsible for its lifetime.
			//! \param entity Entity
			//! \param name Pointer to a stable null-terminated string
			//! \param len String length. If zero, the length is calculated
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			//! \warning Name is expected to be unique. If it is not this function does nothing.
			//! \warning In this case the string is NOT copied and NOT stored internally. You are responsible for its
			//!          lifetime. The pointer also needs to be stable. Otherwise, any time your storage tries to move
			//!          the string to a different place you have to unset the name before it happens and set it anew
			//!          after the move is done.
			void name_raw(Entity entity, const char* name, uint32_t len = 0) {
				name_inter<false>(entity, name, len);
			}

			//! Returns the name assigned to \param entity.
			//! \param entity Entity
			//! \return Name assigned to entity.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			GAIA_NODISCARD const char* name(Entity entity) const {
				if (entity.pair())
					return nullptr;

				const auto& ec = m_recs.entities[entity.id()];
				if (!ec.pArchetype->has<EntityDesc>()) {
					// If no EntityDesc is assigned it is still possible to extract a name from
					// the entity. Components always come with a compile-time string associated
					// with them.
					if (!entity.entity())
						return m_compCache.get(entity).name.str();

					return nullptr;
				}

				const auto& desc = ComponentGetter{ec.pChunk, ec.row}.get<EntityDesc>();
				return desc.name;
			}

			//! Returns the name assigned to \param entityId.
			//! \param entityId EntityId
			//! \return Name assigned to entity.
			//! \warning It is expected \param entityId is valid. Undefined behavior otherwise.
			GAIA_NODISCARD const char* name(EntityId entityId) const {
				auto entity = get(entityId);
				return name(entity);
			}

			//! Returns the entity that is assigned with the \param name.
			//! \param name Name
			//! \return Entity assigned the given name. EntityBad if there is nothing to return.
			GAIA_NODISCARD Entity get(const char* name) const {
				if (name == nullptr)
					return EntityBad;

				const auto it = m_nameToEntity.find(EntityNameLookupKey(name));
				if (it == m_nameToEntity.end())
					return EntityBad;

				return it->second;
			}

			//----------------------------------------------------------------------

			//! Returns relations for \param target.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			GAIA_NODISCARD const cnt::set<EntityLookupKey>* relations(Entity target) const {
				const auto it = m_targetsToRelations.find(EntityLookupKey(target));
				if (it == m_targetsToRelations.end())
					return nullptr;

				return &it->second;
			}

			//! Returns the first relationship relation for the \param target entity on \param entity.
			//! \return Relationship target. EntityBad if there is nothing to return.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			GAIA_NODISCARD Entity relation(Entity entity, Entity target) const {
				GAIA_ASSERT(valid(entity));
				if (!valid(target))
					return EntityBad;

				const auto& ec = fetch(entity);
				const auto* pArchetype = ec.pArchetype;

				// Early exit if there are no pairs on the archetype
				if (pArchetype->pairs() == 0)
					return EntityBad;

				auto ids = pArchetype->ids_view();
				for (auto e: ids) {
					if (!e.pair())
						continue;
					if (e.gen() != target.id())
						continue;

					const auto& ecRel = m_recs.entities[e.id()];
					auto relation = ecRel.pChunk->entity_view()[ecRel.row];
					return relation;
				}

				return EntityBad;
			}

			//! Returns the relationship relations for the \param target entity on \param entity.
			//! \param func void(Entity relation) functor executed for relationship relation found.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			template <typename Func>
			void relations(Entity entity, Entity target, Func func) const {
				GAIA_ASSERT(valid(entity));
				if (!valid(target))
					return;

				const auto& ec = fetch(entity);
				const auto* pArchetype = ec.pArchetype;

				// Early exit if there are no pairs on the archetype
				if (pArchetype->pairs() == 0)
					return;

				auto ids = pArchetype->ids_view();
				for (auto e: ids) {
					if (!e.pair())
						continue;
					if (e.gen() != target.id())
						continue;

					const auto& ecRel = m_recs.entities[e.id()];
					auto relation = ecRel.pChunk->entity_view()[ecRel.row];
					func(relation);
				}
			}

			//! Returns the relationship relations for the \param target entity on \param entity.
			//! \param func bool(Entity relation) functor executed for relationship relation found.
			//!             Stops if false is returned.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			template <typename Func>
			void relations_if(Entity entity, Entity target, Func func) const {
				GAIA_ASSERT(valid(entity));
				if (!valid(target))
					return;

				const auto& ec = fetch(entity);
				const auto* pArchetype = ec.pArchetype;

				// Early exit if there are no pairs on the archetype
				if (pArchetype->pairs() == 0)
					return;

				auto ids = pArchetype->ids_view();
				for (auto e: ids) {
					if (!e.pair())
						continue;
					if (e.gen() != target.id())
						continue;

					const auto& ecRel = m_recs.entities[e.id()];
					auto relation = ecRel.pChunk->entity_view()[ecRel.row];
					if (!func(relation))
						return;
				}
			}

			template <typename Func>
			void as_relations_trav(Entity target, Func func) const {
				GAIA_ASSERT(valid(target));
				if (!valid(target))
					return;

				const auto it = m_entityToAsRelations.find(EntityLookupKey(target));
				if (it == m_entityToAsRelations.end())
					return;

				const auto& set = it->second;
				for (auto relation: set) {
					func(relation.entity());
					as_relations_trav(relation.entity(), func);
				}
			}

			template <typename Func>
			bool as_relations_trav_if(Entity target, Func func) const {
				GAIA_ASSERT(valid(target));
				if (!valid(target))
					return false;

				const auto it = m_entityToAsRelations.find(EntityLookupKey(target));
				if (it == m_entityToAsRelations.end())
					return false;

				const auto& set = it->second;
				for (auto relation: set) {
					if (func(relation.entity()))
						return true;
					if (as_relations_trav_if(relation.entity(), func))
						return true;
				}

				return false;
			}

			//----------------------------------------------------------------------

			//! Returns targets for \param relation.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			GAIA_NODISCARD const cnt::set<EntityLookupKey>* targets(Entity relation) const {
				const auto it = m_relationsToTargets.find(EntityLookupKey(relation));
				if (it == m_relationsToTargets.end())
					return nullptr;

				return &it->second;
			}

			//! Returns the first relationship target for the \param relation entity on \param entity.
			//! \return Relationship target. EntityBad if there is nothing to return.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			GAIA_NODISCARD Entity target(Entity entity, Entity relation) const {
				GAIA_ASSERT(valid(entity));
				if (!valid(relation))
					return EntityBad;

				const auto& ec = fetch(entity);
				const auto* pArchetype = ec.pArchetype;

				// Early exit if there are no pairs on the archetype
				if (pArchetype->pairs() == 0)
					return EntityBad;

				auto ids = pArchetype->ids_view();
				for (auto e: ids) {
					if (!e.pair())
						continue;
					if (e.id() != relation.id())
						continue;

					const auto& ecTarget = m_recs.entities[e.gen()];
					auto target = ecTarget.pChunk->entity_view()[ecTarget.row];
					return target;
				}

				return EntityBad;
			}

			//! Returns the relationship targets for the \param relation entity on \param entity.
			//! \param func void(Entity target) functor executed for relationship target found.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			template <typename Func>
			void targets(Entity entity, Entity relation, Func func) const {
				GAIA_ASSERT(valid(entity));
				if (!valid(relation))
					return;

				const auto& ec = fetch(entity);
				const auto* pArchetype = ec.pArchetype;

				// Early exit if there are no pairs on the archetype
				if (pArchetype->pairs() == 0)
					return;

				auto ids = pArchetype->ids_view();
				for (auto e: ids) {
					if (!e.pair())
						continue;
					if (e.id() != relation.id())
						continue;

					// We accept void(Entity) and void()
					if constexpr (std::is_invocable_v<Func, Entity>) {
						const auto& ecTarget = m_recs.entities[e.gen()];
						auto target = ecTarget.pChunk->entity_view()[ecTarget.row];
						func(target);
					} else {
						func();
					}
				}
			}

			//! Returns the relationship targets for the \param relation entity on \param entity.
			//! \param func bool(Entity target) functor executed for relationship target found.
			//!             Stops if false is returned.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			template <typename Func>
			void targets_if(Entity entity, Entity relation, Func func) const {
				GAIA_ASSERT(valid(entity));
				if (!valid(relation))
					return;

				const auto& ec = fetch(entity);
				const auto* pArchetype = ec.pArchetype;

				// Early exit if there are no pairs on the archetype
				if (pArchetype->pairs() == 0)
					return;

				auto ids = pArchetype->ids_view();
				for (auto e: ids) {
					if (!e.pair())
						continue;
					if (e.id() != relation.id())
						continue;

					// We accept void(Entity) and void()
					if constexpr (std::is_invocable_v<Func, Entity>) {
						const auto& ecTarget = m_recs.entities[e.gen()];
						auto target = ecTarget.pChunk->entity_view()[ecTarget.row];
						func(target);
					} else {
						func();
					}
				}
			}

			template <typename Func>
			void as_targets_trav(Entity relation, Func func) const {
				GAIA_ASSERT(valid(relation));
				if (!valid(relation))
					return;

				const auto it = m_entityToAsTargets.find(EntityLookupKey(relation));
				if (it == m_entityToAsTargets.end())
					return;

				const auto& set = it->second;
				for (auto target: set) {
					func(target.entity());
					as_targets_trav(target.entity(), func);
				}
			}

			template <typename Func>
			bool as_targets_trav_if(Entity relation, Func func) const {
				GAIA_ASSERT(valid(relation));
				if (!valid(relation))
					return false;

				const auto it = m_entityToAsTargets.find(EntityLookupKey(relation));
				if (it == m_entityToAsTargets.end())
					return false;

				const auto& set = it->second;
				for (auto target: set) {
					if (func(target.entity()))
						return true;
					if (as_targets_trav(target.entity(), func))
						return true;
				}

				return false;
			}

			//----------------------------------------------------------------------

#if GAIA_SYSTEMS_ENABLED

			//! Makes sure the world can work with systems.
			void systems_init();

			//! Executes all registered systems once.
			void systems_run();

			//! Provides a system set up to work with the parent world.
			//! \return Entity holding the system.
			SystemBuilder system();

#endif

			//----------------------------------------------------------------------

			//! Enables or disables an entire entity.
			//! \param entity Entity
			//! \param enable Enable or disable the entity
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			void enable(Entity entity, bool enable) {
				GAIA_ASSERT(valid(entity));

				auto& ec = m_recs.entities[entity.id()];

				GAIA_ASSERT(
						(ec.pChunk && !ec.pChunk->locked()) &&
						"Entities can't be enabled/disabled while their chunk is being iterated "
						"(structural changes are forbidden during this time!)");

				auto& archetype = *ec.pArchetype;
				archetype.enable_entity(ec.pChunk, ec.row, enable, m_recs);
			}

			//! Checks if an entity is enabled.
			//! \param entity Entity
			//! \return True it the entity is enabled. False otherwise.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			bool enabled(Entity entity) const {
				GAIA_ASSERT(valid(entity));

				const auto& ec = m_recs.entities[entity.id()];
				const bool entityStateInContainer = !ec.dis;
#if GAIA_ASSERT_ENABLED
				const bool entityStateInChunk = ec.pChunk->enabled(ec.row);
				GAIA_ASSERT(entityStateInChunk == entityStateInContainer);
#endif
				return entityStateInContainer;
			}

			//----------------------------------------------------------------------

			//! Returns a chunk containing the \param entity.
			//! \return Chunk or nullptr if not found.
			GAIA_NODISCARD Chunk* get_chunk(Entity entity) const {
				GAIA_ASSERT(entity.id() < m_recs.entities.size());
				const auto& ec = m_recs.entities[entity.id()];
				return ec.pChunk;
			}

			//! Returns a chunk containing the \param entity.
			//! Index of the entity is stored in \param indexInChunk
			//! \return Chunk or nullptr if not found
			GAIA_NODISCARD Chunk* get_chunk(Entity entity, uint32_t& indexInChunk) const {
				GAIA_ASSERT(entity.id() < m_recs.entities.size());
				const auto& ec = m_recs.entities[entity.id()];
				indexInChunk = ec.row;
				return ec.pChunk;
			}

			//! Returns the number of active entities
			//! \return Entity
			GAIA_NODISCARD uint32_t size() const {
				return m_recs.entities.item_count();
			}

			//! Returns the current version of the world.
			//! \return World version number.
			GAIA_NODISCARD uint32_t& world_version() {
				return m_worldVersion;
			}

			//! Sets maximal lifespan of an archetype \param entity belongs to.
			//! \param lifespan How many world updates an empty archetype is kept.
			//!                 If zero, the archetype it kept indefinitely.
			void set_max_lifespan(Entity entity, uint32_t lifespan = Archetype::MAX_ARCHETYPE_LIFESPAN) {
				if (!valid(entity))
					return;

				auto& ec = fetch(entity);
				ec.pArchetype->set_max_lifespan(lifespan);
			}

			//----------------------------------------------------------------------

			//! Performs various internal operations related to the end of the frame such as
			//! memory cleanup and other management operations which keep the system healthy.
			void update() {
				systems_run();

				// Finish deleting entities
				del_finalize();

				// Run garbage collector
				gc();

				// Signal the end of the frame
				GAIA_PROF_FRAME();
			}

			//! Clears the world so that all its entities and components are released
			void cleanup() {
				cleanup_inter();

				// Reinit
				m_pRootArchetype = nullptr;
				m_pEntityArchetype = nullptr;
				m_pCompArchetype = nullptr;
				m_nextArchetypeId = 0;
				m_defragLastArchetypeIdx = 0;
				m_worldVersion = 0;
				init();
			}

			//! Sets the maximum number of entities defragmented per world tick
			//! \param value Number of entities to defragment
			void defrag_entities_per_tick(uint32_t value) {
				m_defragEntitiesPerTick = value;
			}

			//--------------------------------------------------------------------------------

			//! Performs diagnostics on archetypes. Prints basic info about them and the chunks they contain.
			void diag_archetypes() const {
				GAIA_LOG_N("Archetypes:%u", (uint32_t)m_archetypes.size());
				for (auto* pArchetype: m_archetypes)
					Archetype::diag(*this, *pArchetype);
			}

			//! Performs diagnostics on registered components.
			//! Prints basic info about them and reports and detected issues.
			void diag_components() const {
				comp_cache().diag();
			}

			//! Performs diagnostics on entities of the world.
			//! Also performs validation of internal structures which hold the entities.
			void diag_entities() const {
				validate_entities();

				GAIA_LOG_N("Deleted entities: %u", (uint32_t)m_recs.entities.get_free_items());
				if (m_recs.entities.get_free_items() != 0U) {
					GAIA_LOG_N("  --> %u", (uint32_t)m_recs.entities.get_next_free_item());

					uint32_t iters = 0;
					auto fe = m_recs.entities[m_recs.entities.get_next_free_item()].idx;
					while (fe != IdentifierIdBad) {
						GAIA_LOG_N("  --> %u", m_recs.entities[fe].idx);
						fe = m_recs.entities[fe].idx;
						++iters;
						if (iters > m_recs.entities.get_free_items())
							break;
					}

					if ((iters == 0U) || iters > m_recs.entities.get_free_items())
						GAIA_LOG_E("  Entities recycle list contains inconsistent data!");
				}
			}

			//! Performs all diagnostics.
			void diag() const {
				diag_archetypes();
				diag_components();
				diag_entities();
			}

		private:
			//! Clears the world so that all its entities and components are released
			void cleanup_inter() {
				GAIA_PROF_SCOPE(World::cleanup_inter);

				// Clear entities
				m_recs.entities = {};
				m_recs.pairs = {};

				// Clear archetypes
				{
					// Delete all allocated chunks and their parent archetypes
					for (auto* pArchetype: m_archetypes)
						Archetype::destroy(pArchetype);

					m_entityToAsRelations = {};
					m_entityToAsTargets = {};
					m_targetsToRelations = {};
					m_relationsToTargets = {};

					m_archetypes = {};
					m_archetypesById = {};
					m_archetypesByHash = {};

					m_reqArchetypesToDel = {};
					m_reqEntitiesToDel = {};

					m_entitiesToDel = {};
					m_chunksToDel = {};
					m_archetypesToDel = {};
				}

				// Clear caches
				{
					m_entityToArchetypeMap = {};
					m_queryCache.clear();
				}

				// Clear entity names
				{
					for (auto& pair: m_nameToEntity) {
						if (!pair.first.owned())
							continue;
						// Release any memory allocated for owned names
						mem::mem_free((void*)pair.first.str());
					}
					m_nameToEntity = {};
				}

				// Clear component cache
				m_compCache.clear();
			}

			GAIA_NODISCARD bool valid(const EntityContainer& ec, [[maybe_unused]] Entity entityExpected) const {
				if (is_req_del(ec))
					return false;

				// The entity in the chunk must match the index in the entity container
				auto* pChunk = ec.pChunk;
				if (pChunk == nullptr || ec.row >= pChunk->size())
					return false;

#if GAIA_ASSERT_ENABLED
				const auto entityPresent = ec.pChunk->entity_view()[ec.row];
				GAIA_ASSERT(entityExpected == entityPresent);
				if (entityExpected != entityPresent)
					return false;
#endif

				return true;
			}

			//! Checks if the pair \param entity is valid.
			//! \return True if the entity is valid. False otherwise.
			GAIA_NODISCARD bool valid_pair(Entity entity) const {
				if (entity == EntityBad)
					return false;

				GAIA_ASSERT(entity.pair());
				if (!entity.pair())
					return false;

				// Ignore wildcards because they can't be attached to entities
				if (is_wildcard(entity))
					return true;

				const auto it = m_recs.pairs.find(EntityLookupKey(entity));
				if (it == m_recs.pairs.end())
					return false;

				const auto& ec = it->second;
				return valid(ec, entity);
			}

			//! Checks if the entity \param entity is valid.
			//! \return True if the entity is valid. False otherwise.
			GAIA_NODISCARD bool valid_entity(Entity entity) const {
				if (entity == EntityBad)
					return false;

				GAIA_ASSERT(!entity.pair());
				if (entity.pair())
					return false;

				// Entity ID has to fit inside the entity array
				if (entity.id() >= m_recs.entities.size())
					return false;

				const auto& ec = m_recs.entities[entity.id()];
				return valid(ec, entity);
			}

			//! Checks if the entity with id \param entityId is valid.
			//! Pairs are considered invalid.
			//! \return True if entityId is valid. False otherwise.
			GAIA_NODISCARD bool valid_entity_id(EntityId entityId) const {
				if (entityId == EntityBad.id())
					return false;

				// Entity ID has to fit inside the entity array
				if (entityId >= m_recs.entities.size())
					return false;

				const auto& ec = m_recs.entities[entityId];
				if (ec.pair != 0)
					return false;

				return valid(ec, Entity(entityId, ec.gen, (bool)ec.ent, (bool)ec.pair, (EntityKind)ec.kind));
			}

			//! Sorts archetypes in the archetype list with their ids in ascending order
			void sort_archetypes() {
				struct sort_cond {
					bool operator()(const Archetype* a, const Archetype* b) const {
						return a->id() < b->id();
					}
				};

				core::sort(m_archetypes, sort_cond{}, [&](uint32_t left, uint32_t right) {
					Archetype* tmp = m_archetypes[left];

					m_archetypes[right]->list_idx(left);
					m_archetypes[left]->list_idx(right);

					m_archetypes.data()[left] = (Archetype*)m_archetypes[right];
					m_archetypes.data()[right] = tmp;
				});
			}

			//! Remove a chunk from its archetype.
			//! \param archetype Archetype we remove the chunk from
			//! \param chunk Chunk we are removing
			void remove_chunk(Archetype& archetype, Chunk& chunk) {
				archetype.del(&chunk);
				try_enqueue_archetype_for_deletion(archetype);
			}

			//! Remove an entity from its chunk.
			//! \param archetype Archetype we remove the entity from
			//! \param chunk Chunk we remove the entity from
			//! \param row Index of entity within its chunk
			void remove_entity(Archetype& archetype, Chunk& chunk, uint16_t row) {
				archetype.remove_entity(chunk, row, m_recs);
				try_enqueue_chunk_for_deletion(archetype, chunk);
			}

			//! Delete all chunks which are empty (have no entities) and have not been used in a while
			void del_empty_chunks() {
				GAIA_PROF_SCOPE(World::del_empty_chunks);

				for (uint32_t i = 0; i < m_chunksToDel.size();) {
					auto* pArchetype = m_chunksToDel[i].pArchetype;
					auto* pChunk = m_chunksToDel[i].pChunk;

					// Revive reclaimed chunks
					if (!pChunk->empty()) {
						pChunk->revive();
						revive_archetype(*pArchetype);
						core::swap_erase(m_chunksToDel, i);
						continue;
					}

					// Skip chunks which still have some lifespan left
					if (pChunk->progress_death()) {
						++i;
						continue;
					}

					// Delete unused chunks that are past their lifespan
					remove_chunk(*pArchetype, *pChunk);
					core::swap_erase(m_chunksToDel, i);
				}
			}

			//! Delete an archetype from the world
			void del_empty_archetype(Archetype* pArchetype) {
				GAIA_PROF_SCOPE(World::del_empty_archetype);

				GAIA_ASSERT(pArchetype != nullptr);
				GAIA_ASSERT(pArchetype->empty() || pArchetype->is_req_del());
				GAIA_ASSERT(!pArchetype->dying() || pArchetype->is_req_del());

				unreg_archetype(pArchetype);
				Archetype::destroy(pArchetype);
			}

			//! Delete all archetypes which are empty (have no used chunks) and have not been used in a while
			void del_empty_archetypes() {
				GAIA_PROF_SCOPE(World::del_empty_archetypes);

				cnt::sarr_ext<Archetype*, 512> tmp;

				// Remove all dead archetypes from query caches.
				// Because the number of cached queries is way higher than the number of archetypes
				// we want to remove, we flip the logic around and iterate over all query caches
				// and match against our lists.
				// Note, all archetype pointers in the tmp array are invalid at this point and can
				// be used only for comparison. They can't be dereferenced.
				auto remove_from_queries = [&]() {
					if (tmp.empty())
						return;

					// TODO: How to speed this up? If there are 1k cached queries is it still going to
					//       be fast enough or do we get spikes? Probably a linked list for query cache
					//       would be a way to go.
					for (auto& info: m_queryCache) {
						for (auto* pArchetype: tmp)
							info.remove(pArchetype);
					}

					for (auto* pArchetype: tmp)
						del_empty_archetype(pArchetype);
					tmp.clear();
				};

				for (uint32_t i = 0; i < m_archetypesToDel.size();) {
					auto* pArchetype = m_archetypesToDel[i];

					// Skip reclaimed archetypes
					if (!pArchetype->empty()) {
						revive_archetype(*pArchetype);
						core::swap_erase(m_archetypesToDel, i);
						continue;
					}

					// Skip archetypes which still have some lifespan left unless
					// they are force-deleted.
					if (!pArchetype->is_req_del() && pArchetype->progress_death()) {
						++i;
						continue;
					}

					tmp.push_back(pArchetype);

					// Remove the unused archetypes
					core::swap_erase(m_archetypesToDel, i);

					// Clear what we have once the capacity is reached
					if (tmp.size() == tmp.max_size())
						remove_from_queries();
				}

				remove_from_queries();
			}

			void revive_archetype(Archetype& archetype) {
				archetype.revive();
				m_reqArchetypesToDel.erase(ArchetypeLookupKey(archetype.lookup_hash(), &archetype));
			}

			void try_enqueue_chunk_for_deletion(Archetype& archetype, Chunk& chunk) {
				if (chunk.dying() || !chunk.empty())
					return;

				// When the chunk is emptied we want it to be removed. We can't do it
				// rowB away and need to wait for world::gc() to be called.
				//
				// However, we need to prevent the following:
				//    1) chunk is emptied, add it to some removal list
				//    2) chunk is reclaimed
				//    3) chunk is emptied, add it to some removal list again
				//
				// Therefore, we have a flag telling us the chunk is already waiting to
				// be removed. The chunk might be reclaimed before garbage collection happens
				// but it simply ignores such requests. This way we always have at most one
				// record for removal for any given chunk.
				chunk.start_dying();

				m_chunksToDel.push_back({&archetype, &chunk});
			}

			void try_enqueue_archetype_for_deletion(Archetype& archetype) {
				if (archetype.dying() || !archetype.empty())
					return;

				// When the chunk is emptied we want it to be removed. We can't do it
				// rowB away and need to wait for world::gc() to be called.
				//
				// However, we need to prevent the following:
				//    1) archetype is emptied, add it to some removal list
				//    2) archetype is reclaimed
				//    3) archetype is emptied, add it to some removal list again
				//
				// Therefore, we have a flag telling us the chunk is already waiting to
				// be removed. The archetype might be reclaimed before garbage collection happens
				// but it simply ignores such requests. This way we always have at most one
				// record for removal for any given chunk.
				archetype.start_dying();

				m_archetypesToDel.push_back(&archetype);
			}

			//! Defragments chunks.
			//! \param maxEntities Maximum number of entities moved per call
			void defrag_chunks(uint32_t maxEntities) {
				GAIA_PROF_SCOPE(World::defrag_chunks);

				const auto maxIters = m_archetypes.size();
				// There has to be at least the root archetype present
				GAIA_ASSERT(maxIters > 0);

				GAIA_FOR(maxIters) {
					const auto idx = (m_defragLastArchetypeIdx + 1) % maxIters;
					auto* pArchetype = m_archetypes[idx];
					defrag_archetype(*pArchetype, maxEntities);
					if (maxEntities == 0)
						return;

					m_defragLastArchetypeIdx = idx;
				}
			}

			//! Defragments the chunk.
			//! \param maxEntities Maximum number of entities moved per call
			//! \param chunksToDelete Container of chunks ready for removal
			//! \param entities Container with entities
			void defrag_archetype(Archetype& archetype, uint32_t& maxEntities) {
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
				// TODO: Implement mask of semi-full chunks so we can pick one easily when searching
				//       for a chunk to fill with a new entity and when defragmenting.
				// NOTE 1:
				// Even though entity movement might be present during defragmentation, we do
				// not update the world version here because no real structural changes happen.
				// All entities and components remain intact, they just move to a different place.
				// NOTE 2:
				// Entities belonging to chunks with uni components are locked to their chunk.
				// Therefore, we won't defragment them unless their uni components contain matching
				// values.

				if (maxEntities == 0)
					return;

				auto& chunks = archetype.chunks();
				if (chunks.size() < 2)
					return;

				uint32_t front = 0;
				uint32_t back = chunks.size() - 1;

				auto* pDstChunk = chunks[front];
				auto* pSrcChunk = chunks[back];

				// Find the first semi-full chunk in the front
				while (front < back && (pDstChunk->full() || !pDstChunk->is_semi()))
					pDstChunk = chunks[++front];
				// Find the last semi-full chunk in the back
				while (front < back && (pSrcChunk->empty() || !pSrcChunk->is_semi()))
					pSrcChunk = chunks[--back];

				const auto& props = archetype.props();
				const bool hasUniEnts =
						props.cntEntities > 0 && archetype.ids_view()[props.cntEntities - 1].kind() == EntityKind::EK_Uni;

				// Find the first semi-empty chunk in the back
				while (front < back) {
					pDstChunk = chunks[front];
					pSrcChunk = chunks[back];

					const uint32_t entitiesInSrcChunk = pSrcChunk->size();
					const uint32_t spaceInDstChunk = pDstChunk->m_header.capacity - pDstChunk->size();
					const uint32_t entitiesToMoveSrc = core::get_min(entitiesInSrcChunk, maxEntities);
					const uint32_t entitiesToMove = core::get_min(entitiesToMoveSrc, spaceInDstChunk);

					// Make sure uni components have matching values
					if (hasUniEnts) {
						auto rec = pSrcChunk->comp_rec_view();
						bool res = true;
						GAIA_FOR2(props.genEntities, props.cntEntities) {
							const auto* pSrcVal = (const void*)pSrcChunk->comp_ptr(i, 0);
							const auto* pDstVal = (const void*)pDstChunk->comp_ptr(i, 0);
							if (rec[i].pItem->cmp(pSrcVal, pDstVal)) {
								res = false;
								break;
							}
						}

						// When there is not a match we move to the next chunk
						if (!res) {
							pDstChunk = chunks[++front];
							goto next_iteration;
						}
					}

					GAIA_FOR(entitiesToMove) {
						const auto lastSrcEntityIdx = entitiesInSrcChunk - i - 1;
						const auto entity = pSrcChunk->entity_view()[lastSrcEntityIdx];

						auto& ec = m_recs[entity];

						const auto srcRow = ec.row;
						const auto dstRow = pDstChunk->add_entity(entity);
						const bool wasEnabled = !ec.dis;

						// Make sure the old entity becomes enabled now
						archetype.enable_entity(pSrcChunk, srcRow, true, m_recs);
						// We go back-to-front in the chunk so enabling the entity is not expected to change its row
						GAIA_ASSERT(srcRow == ec.row);

						// Move data from the old chunk to the new one
						pDstChunk->move_entity_data(entity, dstRow, m_recs);

						// Remove the entity record from the old chunk
						remove_entity(archetype, *pSrcChunk, srcRow);

						// Bring the entity container record up-to-date
						ec.pChunk = pDstChunk;
						ec.row = (uint16_t)dstRow;

						// Transfer the original enabled state to the new chunk
						archetype.enable_entity(pDstChunk, dstRow, wasEnabled, m_recs);
					}

					maxEntities -= entitiesToMove;
					if (maxEntities == 0)
						return;

					// The source is empty, find another semi-empty source
					if (pSrcChunk->empty()) {
						while (front < back && !chunks[--back]->is_semi())
							;
					}

				next_iteration:
					// The destination chunk is full, we need to move to the next one.
					// The idea is to fill the destination as much as possible.
					while (front < back && pDstChunk->full())
						pDstChunk = chunks[++front];
				}
			}

			//! Searches for archetype with a given set of components
			//! \param hashLookup Archetype lookup hash
			//! \param ids Archetype entities/components
			//! \return Pointer to archetype or nullptr.
			GAIA_NODISCARD Archetype* find_archetype(Archetype::LookupHash hashLookup, EntitySpan ids) {
				auto tmpArchetype = ArchetypeLookupChecker(ids);
				ArchetypeLookupKey key(hashLookup, &tmpArchetype);

				// Search for the archetype in the map
				const auto it = m_archetypesByHash.find(key);
				if (it == m_archetypesByHash.end())
					return nullptr;

				auto* pArchetype = it->second;
				return pArchetype;
			}

			//! Adds the archetype to <entity, archetype> map for quick lookups of archetypes by comp/tag/pair
			//! \param entity Entity getting added
			//! \param pArchetype Linked archetype
			void add_entity_archetype_pair(Entity entity, Archetype* pArchetype) {
				EntityLookupKey entityKey(entity);
				const auto it = m_entityToArchetypeMap.find(entityKey);
				if (it == m_entityToArchetypeMap.end()) {
					m_entityToArchetypeMap.try_emplace(entityKey, ArchetypeDArray{pArchetype});
					return;
				}

				auto& archetypes = it->second;
				if (!core::has(archetypes, pArchetype))
					archetypes.push_back(pArchetype);
			}

			//! Deletes an archetype from the <pairEntity, archetype> map
			//! \param pair Pair entity used as a key in the map
			//! \param entityToRemove Entity used to identify archetypes we are removing from the archetype array
			void del_entity_archetype_pair(Pair pair, Entity entityToRemove) {
				auto it = m_entityToArchetypeMap.find(EntityLookupKey(pair));
				auto& archetypes = it->second;

				// Remove any reference to the found archetype from the array.
				// We don't know the archetype so we remove any archetype that contains our entity.
				for (int i = (int)archetypes.size() - 1; i >= 0; --i) {
					const auto* pArchetype = archetypes[(uint32_t)i];
					if (!pArchetype->has(entityToRemove))
						continue;

					core::swap_erase_unsafe(archetypes, i);
				}

				// NOTE: No need to delete keys with empty archetype arrays.
				//       There are only 3 such keys: (*, tgt), (src, *), (*, *)
				// If no more items are present in the array, remove the map key.
				// if (archetypes.empty())
				// 	m_entityToArchetypeMap.erase(it); DON'T
			}

			//! Deletes an archetype from the <entity, archetype> map
			//! \param entity Entity getting deleted
			void del_entity_archetype_pairs(Entity entity) {
				// TODO: Optimize. Either switch to an array or add an index to the map value.
				//       Otherwise all these lookups make deleting entities slow.

				m_entityToArchetypeMap.erase(EntityLookupKey(entity));

				if (entity.pair()) {
					const auto first = get(entity.id());
					const auto second = get(entity.gen());

					// (*, tgt)
					del_entity_archetype_pair(Pair(All, second), entity);
					// (src, *)
					del_entity_archetype_pair(Pair(first, All), entity);
					// (*, *)
					del_entity_archetype_pair(Pair(All, All), entity);
				}
			}

			//! Creates a new archetype from a given set of entities
			//! \param entities Archetype entities/components
			//! \return Pointer to the new archetype.
			GAIA_NODISCARD Archetype* create_archetype(EntitySpan entities) {
				GAIA_ASSERT(m_nextArchetypeId < (decltype(m_nextArchetypeId))-1);
				auto* pArchetype = Archetype::create(*this, m_nextArchetypeId++, m_worldVersion, entities);

				for (auto entity: entities) {
					add_entity_archetype_pair(entity, pArchetype);

					// If the entity is a pair, make sure to create special wildcard records for it
					// as well so wildcard queries can find the archetype.
					if (entity.pair()) {
						const auto first = get(entity.id());
						const auto second = get(entity.gen());

						// (*, tgt)
						add_entity_archetype_pair(Pair(All, second), pArchetype);
						// (src, *)
						add_entity_archetype_pair(Pair(first, All), pArchetype);
						// (*, *)
						add_entity_archetype_pair(Pair(All, All), pArchetype);
					}
				}

				return pArchetype;
			}

			//! Registers the archetype in the world.
			//! \param pArchetype Archetype to register.
			void reg_archetype(Archetype* pArchetype) {
				GAIA_ASSERT(pArchetype != nullptr);

				// // Make sure hashes were set already
				// GAIA_ASSERT(
				// 		(m_archetypesById.empty() || pArchetype == m_pRootArchetype) || (pArchetype->lookup_hash().hash != 0));

				// Make sure the archetype is not registered yet
				GAIA_ASSERT(pArchetype->list_idx() == BadIndex);

				// Register the archetype
				[[maybe_unused]] const auto it0 =
						m_archetypesById.emplace(ArchetypeIdLookupKey(pArchetype->id(), pArchetype->id_hash()), pArchetype);
				[[maybe_unused]] const auto it1 =
						m_archetypesByHash.emplace(ArchetypeLookupKey(pArchetype->lookup_hash(), pArchetype), pArchetype);
				GAIA_ASSERT(it0.second);
				GAIA_ASSERT(it1.second);

				pArchetype->list_idx(m_archetypes.size());
				m_archetypes.emplace_back(pArchetype);
			}

			//! Unregisters the archetype in the world.
			//! \param pArchetype Archetype to register.
			void unreg_archetype(Archetype* pArchetype) {
				GAIA_ASSERT(pArchetype != nullptr);

				// Make sure hashes were set already
				GAIA_ASSERT(
						(m_archetypesById.empty() || pArchetype == m_pRootArchetype) || (pArchetype->lookup_hash().hash != 0));

				// Make sure the archetype was registered already
				GAIA_ASSERT(pArchetype->list_idx() != BadIndex);

				// Break graph connections
				{
					auto& edgeLefts = pArchetype->left_edges();
					for (auto& itLeft: edgeLefts)
						remove_edge_from_archetype(pArchetype, itLeft.second, itLeft.first.entity());
				}

				auto tmpArchetype = ArchetypeLookupChecker(pArchetype->ids_view());
				[[maybe_unused]] const auto res0 =
						m_archetypesById.erase(ArchetypeIdLookupKey(pArchetype->id(), pArchetype->id_hash()));
				[[maybe_unused]] const auto res1 =
						m_archetypesByHash.erase(ArchetypeLookupKey(pArchetype->lookup_hash(), &tmpArchetype));
				GAIA_ASSERT(res0 != 0);
				GAIA_ASSERT(res1 != 0);

				const auto idx = pArchetype->list_idx();
				GAIA_ASSERT(idx == core::get_index(m_archetypes, pArchetype));
				core::swap_erase(m_archetypes, idx);
				if (!m_archetypes.empty() && idx != m_archetypes.size())
					m_archetypes[idx]->list_idx(idx);
			}

#if GAIA_ASSERT_ENABLED
			static void print_archetype_entities(const World& world, const Archetype& archetype, Entity entity, bool adding) {
				auto ids = archetype.ids_view();

				GAIA_LOG_W("Currently present:");
				GAIA_EACH(ids) {
					GAIA_LOG_W("> [%u] %s [%s]", i, entity_name(world, ids[i]), EntityKindString[(uint32_t)ids[i].kind()]);
				}

				GAIA_LOG_W("Trying to %s:", adding ? "add" : "del");
				GAIA_LOG_W("> %s [%s]", entity_name(world, entity), EntityKindString[(uint32_t)entity.kind()]);
			}

			static void verify_add(const World& world, Archetype& archetype, Entity entity, Entity addEntity) {
				// Makes sure no wildcard entities are added
				if (is_wildcard(addEntity)) {
					GAIA_ASSERT2(false, "Adding wildcard pairs is not supported");
					print_archetype_entities(world, archetype, addEntity, true);
					return;
				}
				// Make sure not to add too many entities/components
				auto ids = archetype.ids_view();
				if GAIA_UNLIKELY (ids.size() + 1 >= ChunkHeader::MAX_COMPONENTS) {
					GAIA_ASSERT2(false, "Trying to add too many entities to entity!");
					GAIA_LOG_W("Trying to add an entity to entity [%u:%u] but there's no space left!", entity.id(), entity.gen());
					print_archetype_entities(world, archetype, addEntity, true);
					return;
				}
			}

			static void verify_del(const World& world, Archetype& archetype, Entity entity, Entity delEntity) {
				if GAIA_UNLIKELY (!archetype.has(delEntity)) {
					GAIA_ASSERT2(false, "Trying to remove an entity which wasn't added");
					GAIA_LOG_W("Trying to del an entity from entity [%u:%u] but it was never added", entity.id(), entity.gen());
					print_archetype_entities(world, archetype, delEntity, false);
				}
			}
#endif

			//! Searches for an archetype which is formed by adding entity \param entity of \param pArchetypeLeft.
			//! If no such archetype is found a new one is created.
			//! \param pArchetypeLeft Archetype we originate from.
			//! \param entity Entity we want to add.
			//! \return Archetype pointer.
			GAIA_NODISCARD Archetype* foc_archetype_add(Archetype* pArchetypeLeft, Entity entity) {
				// Check if the component is found when following the "add" edges
				{
					const auto edge = pArchetypeLeft->find_edge_right(entity);
					if (edge != ArchetypeIdHashPairBad) {
						auto it = m_archetypesById.find(ArchetypeIdLookupKey(edge.id, edge.hash));
						// The edge must exist at this point
						GAIA_ASSERT(it != m_archetypesById.end());

						auto* pArchetypeRight = it->second;
						GAIA_ASSERT(pArchetypeRight != nullptr);
						return pArchetypeRight;
					}
				}

				// Prepare a joint array of components of old + the newly added component
				cnt::sarray_ext<Entity, ChunkHeader::MAX_COMPONENTS> entsNew;
				{
					auto entsOld = pArchetypeLeft->ids_view();
					entsNew.resize((uint32_t)entsOld.size() + 1);
					GAIA_EACH(entsOld) entsNew[i] = entsOld[i];
					entsNew[(uint32_t)entsOld.size()] = entity;
				}

				// Make sure to sort the components so we receive the same hash no matter the order in which components
				// are provided Bubble sort is okay. We're dealing with at most ChunkHeader::MAX_COMPONENTS items.
				sort(entsNew, SortComponentCond{});

				// Once sorted we can calculate the hashes
				const auto hashLookup = calc_lookup_hash({entsNew.data(), entsNew.size()}).hash;
				auto* pArchetypeRight = find_archetype({hashLookup}, {entsNew.data(), entsNew.size()});
				if (pArchetypeRight == nullptr) {
					pArchetypeRight = create_archetype({entsNew.data(), entsNew.size()});
					pArchetypeRight->set_hashes({hashLookup});
					pArchetypeLeft->build_graph_edges(pArchetypeRight, entity);
					reg_archetype(pArchetypeRight);
				}

				return pArchetypeRight;
			}

			//! Searches for an archetype which is formed by removing entity \param entity from \param pArchetypeRight.
			//! If no such archetype is found a new one is created.
			//! \param pArchetypeRight Archetype we originate from.
			//! \param entity Component we want to remove.
			//! \return Pointer to archetype.
			GAIA_NODISCARD Archetype* foc_archetype_del(Archetype* pArchetypeRight, Entity entity) {
				// Check if the component is found when following the "del" edges
				{
					const auto edge = pArchetypeRight->find_edge_left(entity);
					if (edge != ArchetypeIdHashPairBad)
						return m_archetypesById[edge];
				}

				cnt::sarray_ext<Entity, ChunkHeader::MAX_COMPONENTS> entsNew;
				auto entsOld = pArchetypeRight->ids_view();

				// Find the intersection
				for (const auto e: entsOld) {
					if (e == entity)
						continue;

					entsNew.push_back(e);
				}

				// Verify there was a change
				GAIA_ASSERT(entsNew.size() != entsOld.size());

				// Calculate the hashes
				const auto hashLookup = calc_lookup_hash({entsNew.data(), entsNew.size()}).hash;
				auto* pArchetype = find_archetype({hashLookup}, {entsNew.data(), entsNew.size()});
				if (pArchetype == nullptr) {
					pArchetype = create_archetype({entsNew.data(), entsNew.size()});
					pArchetype->set_hashes({hashLookup});
					pArchetype->build_graph_edges(pArchetypeRight, entity);
					reg_archetype(pArchetype);
				}

				return pArchetype;
			}

			//! Returns an array of archetypes registered in the world
			//! \return Array or archetypes.
			GAIA_NODISCARD const auto& archetypes() const {
				return m_archetypesById;
			}

			//! Returns the archetype the entity belongs to.
			//! \param entity Entity
			//! \return Reference to the archetype.
			GAIA_NODISCARD Archetype& archetype(Entity entity) {
				const auto& ec = fetch(entity);
				return *ec.pArchetype;
			}

			//! Removes any name associated with the entity
			//! \param entity Entity the name of which we want to delete
			void del_name(EntityContainer& ec, Entity entity) {
				if (entity.pair())
					return;

				del_name_inter(ec);
			}

			//! Removes any name associated with the entity
			//! \param entity Entity the name of which we want to delete
			void del_name(Entity entity) {
				if (entity.pair())
					return;

				auto& ec = fetch(entity);
				del_name_inter(ec);
			}

			//! Removes any name associated with the entity
			//! \param entity Entity the name of which we want to delete
			void del_name_inter(EntityContainer& ec) {
				if (!ec.pArchetype->has<EntityDesc>())
					return;

				auto& entityDesc = ec.pChunk->sview_mut<EntityDesc>()[ec.row];
				if (entityDesc.name == nullptr)
					return;

				const auto it = m_nameToEntity.find(EntityNameLookupKey(entityDesc.name, entityDesc.len));
				// If the assert is hit it means the pointer to the name string was invalidated or became dangling.
				// That should not be possible for strings managed internally so the only other option is user-managed
				// strings are broken.
				GAIA_ASSERT(it != m_nameToEntity.end());
				if (it != m_nameToEntity.end()) {
					// Release memory allocated for the string if we own it
					if (it->first.owned())
						mem::mem_free((void*)entityDesc.name);

					m_nameToEntity.erase(it);
				}
				entityDesc.name = nullptr;
			}

			//! Deletes an entity along with all data associated with it.
			//! \param entity Entity to delete
			void del_entity(Entity entity, bool invalidate) {
				if (entity.pair() || entity == EntityBad)
					return;

				auto& ec = fetch(entity);
				del_entity_inter(ec, entity, invalidate);
			}

			//! Deletes an entity along with all data associated with it.
			//! \param entity Entity to delete
			void del_entity(EntityContainer& ec, Entity entity, bool invalidate) {
				if (entity.pair() || entity == EntityBad)
					return;

				del_entity_inter(ec, entity, invalidate);
			}

			//! Deletes an entity along with all data associated with it.
			//! \param entity Entity to delete
			void del_entity_inter(EntityContainer& ec, Entity entity, bool invalidate) {
				GAIA_ASSERT(entity.id() > GAIA_ID(LastCoreComponent).id());

				// if (!is_req_del(ec))
				{
					if (m_recs.entities.item_count() == 0)
						return;

#if GAIA_ASSERT_ENABLED
					auto* pChunk = ec.pChunk;
					GAIA_ASSERT(pChunk != nullptr);
#endif

					// Remove the entity from its chunk.
					// We call del_name first because remove_entity calls component destructors.
					// If the call was made inside invalidate_entity we would access a memory location
					// which has already been destructed which is not nice.
					del_name(ec, entity);
					remove_entity(*ec.pArchetype, *ec.pChunk, ec.row);
				}

				// Invalidate on-demand.
				// We delete as a separate step in the delayed deletion.
				if (invalidate)
					invalidate_entity(entity);
			}

			//! Deletes all entities (and in turn chunks) from \param archetype.
			//! If an archetype forming entity is present, the chunk is treated as if it were empty
			//! and normal dying procedure is applied to it. At the last dying tick the entity is
			//! deleted so the chunk can be removed.
			void del_entities(Archetype& archetype) {
				for (auto* pChunk: archetype.chunks()) {
					auto ids = pChunk->entity_view();
					for (auto e: ids) {
						if (!valid(e))
							continue;

#if GAIA_ASSERT_ENABLED
						const auto& ec = fetch(e);

						// We should never end up trying to delete a forbidden-to-delete entity
						GAIA_ASSERT((ec.flags & EntityContainerFlags::OnDeleteTarget_Error) == 0);
#endif

						del_entity(e, true);
					}

					validate_chunk(pChunk);

					// If the chunk was already dying we need to remove it from the delete list
					// because we can delete it right away.
					// TODO: Instead of searching for it we could store a delete index in the chunk
					//       header. This way the lookup is O(1) instead of O(N) and it will help
					//       with edge-cases (tons of chunks removed at the same time).
					if (pChunk->dying()) {
						const auto idx = core::get_index(m_chunksToDel, {&archetype, pChunk});
						if (idx != BadIndex)
							core::swap_erase(m_chunksToDel, idx);
					}

					remove_chunk(archetype, *pChunk);
				}

				validate_entities();
			}

			//! Deletes the entity
			void del_inter(Entity entity) {
				auto on_delete = [this](Entity entityToDel) {
					auto& ec = fetch(entityToDel);
					handle_del_entity(ec, entityToDel);
					req_del(ec, entityToDel);
				};

				if (is_wildcard(entity)) {
					const auto rel = get(entity.id());
					const auto tgt = get(entity.gen());

					// (*,*)
					if (rel == All && tgt == All) {
						GAIA_ASSERT2(false, "Not supported yet");
					}
					// (*,X)
					else if (rel == All) {
						if (const auto* pTargets = relations(tgt)) {
							// handle_del might invalidate the targets map so we need to make a copy
							// TODO: this is suboptimal at best, needs to be optimized
							cnt::darray_ext<Entity, 64> tmp;
							for (auto key: *pTargets)
								tmp.push_back(key.entity());
							for (auto e: tmp)
								on_delete(Pair(e, tgt));
						}
					}
					// (X,*)
					else if (tgt == All) {
						if (const auto* pRelations = targets(rel)) {
							// handle_del might invalidate the targets map so we need to make a copy
							// TODO: this is suboptimal at best, needs to be optimized
							cnt::darray_ext<Entity, 64> tmp;
							for (auto key: *pRelations)
								tmp.push_back(key.entity());
							for (auto e: tmp)
								on_delete(Pair(rel, e));
						}
					}
				} else {
					on_delete(entity);
				}
			}

			// Force-delete all entities from the requested archetypes along with the archetype itself
			void del_finalize_archetypes() {
				GAIA_PROF_SCOPE(del_finalize_archetypes);

				for (auto& key: m_reqArchetypesToDel) {
					auto* pArchetype = key.archetype();
					if (pArchetype == nullptr)
						continue;

					del_entities(*pArchetype);

					// Now that all entities are deleted, all their chunks are requested to get deleted
					// and in turn the archetype itself as well. Therefore, it is added to the archetype
					// delete list and picked up by del_empty_archetypes. No need to call deletion from here.
					// > del_empty_archetype(pArchetype);
				}
				m_reqArchetypesToDel.clear();
			}

			//! Try to delete all requested entities
			void del_finalize_entities() {
				GAIA_PROF_SCOPE(del_finalize_entities);

				for (auto it = m_reqEntitiesToDel.begin(); it != m_reqEntitiesToDel.end();) {
					const auto e = it->entity();

					// Entities that form archetypes need to stay until the archetype itself is gone
					if (m_entityToArchetypeMap.contains(*it)) {
						++it;
						continue;
					}

					// Requested entities are partially deleted. We only need to invalidate them.
					invalidate_entity(e);

					it = m_reqEntitiesToDel.erase(it);
				}
			}

			//! Finalize all queued delete operations
			void del_finalize() {
				GAIA_PROF_SCOPE(del_finalize);

				del_finalize_archetypes();
				del_finalize_entities();
			}

			GAIA_NODISCARD bool archetype_cond_match(Archetype& archetype, Pair cond, Entity target) const {
				// E.g.:
				//   target = (All, entity)
				//   cond = (OnDeleteTarget, delete)
				// Delete the entity if it matches the cond
				auto ids = archetype.ids_view();

				if (target.pair()) {
					for (auto e: ids) {
						// Find the pair which matches (All, entity)
						if (!e.pair())
							continue;
						if (e.gen() != target.gen())
							continue;

						const auto& ec = m_recs.entities[e.id()];
						const auto entity = ec.pChunk->entity_view()[ec.row];
						if (!has(entity, cond))
							continue;

						return true;
					}
				} else {
					for (auto e: ids) {
						if (e.pair())
							continue;
						if (!has(e, cond))
							continue;

						return true;
					}
				}

				return false;
			}

			//! Updates all chunks and entities of archetype \param srcArchetype so they are a part of \param dstArchetype
			void move_to_archetype(Archetype& srcArchetype, Archetype& dstArchetype) {
				GAIA_ASSERT(&srcArchetype != &dstArchetype);

				for (auto* pSrcChunk: srcArchetype.chunks()) {
					auto srcEnts = pSrcChunk->entity_view();
					if (srcEnts.empty())
						continue;

					// Copy entities back-to-front to avoid unnecessary data movements.
					// TODO: Handle disabled entities efficiently.
					//       If there are disabled entities, we still do data movements if there already
					//       are enabled entities in the chunk.
					// TODO: If the header was of some fixed size, e.g. if we always acted as if we had
					//       ChunkHeader::MAX_COMPONENTS, certain data movements could be done pretty much instantly.
					//       E.g. when removing tags or pairs, we would simply replace the chunk pointer
					//       with a pointer to another one. The same goes for archetypes. Component data
					//       would not have to move at all internal chunk header pointers would remain unchanged.

					int i = (int)(srcEnts.size() - 1);
					while (i >= 0) {
						auto* pDstChunk = dstArchetype.foc_free_chunk();
						move_entity(srcEnts[(uint32_t)i], dstArchetype, *pDstChunk);

						--i;
					}
				}
			}

			//! Find the destination archetype \param pArchetype as if removing \param entity.
			//! \return Destination archetype of nullptr if no match is found
			GAIA_NODISCARD Archetype* calc_dst_archetype_ent(Archetype* pArchetype, Entity entity) {
				GAIA_ASSERT(!is_wildcard(entity));

				auto ids = pArchetype->ids_view();
				for (auto id: ids) {
					if (id != entity)
						continue;

					return foc_archetype_del(pArchetype, id);
				}

				return nullptr;
			}

			//! Find the destination archetype \param pArchetype as if removing all entities
			//! matching (All, entity) from the archetype.
			//! \return Destination archetype of nullptr if no match is found
			GAIA_NODISCARD Archetype* calc_dst_archetype_all_ent(Archetype* pArchetype, Entity entity) {
				GAIA_ASSERT(is_wildcard(entity));

				Archetype* pDstArchetype = pArchetype;

				auto ids = pArchetype->ids_view();
				for (auto id: ids) {
					if (!id.pair() || id.gen() != entity.gen())
						continue;

					pDstArchetype = foc_archetype_del(pDstArchetype, id);
				}

				return pArchetype != pDstArchetype ? pDstArchetype : nullptr;
			}

			//! Find the destination archetype \param pArchetype as if removing all entities
			//! matching (entity, All) from the archetype.
			//! \return Destination archetype of nullptr if no match is found
			GAIA_NODISCARD Archetype* calc_dst_archetype_ent_all(Archetype* pArchetype, Entity entity) {
				GAIA_ASSERT(is_wildcard(entity));

				Archetype* pDstArchetype = pArchetype;

				auto ids = pArchetype->ids_view();
				for (auto id: ids) {
					if (!id.pair() || id.id() != entity.id())
						continue;

					pDstArchetype = foc_archetype_del(pDstArchetype, id);
				}

				return pArchetype != pDstArchetype ? pDstArchetype : nullptr;
			}

			//! Find the destination archetype \param pArchetype as if removing all entities
			//! matching (All, All) from the archetype.
			//! \return Destination archetype of nullptr if no match is found
			GAIA_NODISCARD Archetype* calc_dst_archetype_all_all(Archetype* pArchetype, [[maybe_unused]] Entity entity) {
				GAIA_ASSERT(is_wildcard(entity));

				Archetype* pDstArchetype = pArchetype;
				bool found = false;

				auto ids = pArchetype->ids_view();
				for (auto id: ids) {
					if (!id.pair())
						continue;

					pDstArchetype = foc_archetype_del(pDstArchetype, id);
					found = true;
				}

				return found ? pDstArchetype : nullptr;
			}

			//! Find the destination archetype \param pArchetype as if removing \param entity.
			//! Wildcard pair entities are supported as well.
			//! \return Destination archetype of nullptr if no match is found
			GAIA_NODISCARD Archetype* calc_dst_archetype(Archetype* pArchetype, Entity entity) {
				if (entity.pair()) {
					auto rel = entity.id();
					auto tgt = entity.gen();

					// Removing a wildcard pair. We need to find all pairs matching it.
					if (rel == All.id() || tgt == All.id()) {
						// (first, All) means we need to match (first, A), (first, B), ...
						if (rel != All.id() && tgt == All.id())
							return calc_dst_archetype_ent_all(pArchetype, entity);

						// (All, second) means we need to match (A, second), (B, second), ...
						if (rel == All.id() && tgt != All.id())
							return calc_dst_archetype_all_ent(pArchetype, entity);

						// (All, All) means we need to match all relationships
						return calc_dst_archetype_all_all(pArchetype, EntityBad);
					}
				}

				// Non-wildcard pair or entity
				return calc_dst_archetype_ent(pArchetype, entity);
			}

			void req_del(Archetype& archetype) {
				if (archetype.is_req_del())
					return;

				archetype.req_del();
				m_reqArchetypesToDel.insert(ArchetypeLookupKey(archetype.lookup_hash(), &archetype));
			}

			void req_del(EntityContainer& ec, Entity entity) {
				if (is_req_del(ec))
					return;

				del_entity(ec, entity, false);

				ec.req_del();
				m_reqEntitiesToDel.insert(EntityLookupKey(entity));
			}

			//! Requests deleting anything that references the \param entity. No questions asked.
			void req_del_entities_with(Entity entity) {
				GAIA_PROF_SCOPE(World::req_del_entities_with);

				const auto it = m_entityToArchetypeMap.find(EntityLookupKey(entity));
				if (it == m_entityToArchetypeMap.end())
					return;

				const auto& archetypes = it->second;
				for (auto* pArchetype: archetypes)
					req_del(*pArchetype);
			}

			//! Requests deleting anything that references the \param entity. No questions asked.
			//! Takes \param cond into account.
			void req_del_entities_with(Entity entity, Pair cond) {
				GAIA_PROF_SCOPE(World::req_del_entities_with);

				const auto it = m_entityToArchetypeMap.find(EntityLookupKey(entity));
				if (it == m_entityToArchetypeMap.end())
					return;

				const auto& archetypes = it->second;
				for (auto* pArchetype: archetypes) {
					// Evaluate the condition if a valid pair is given
					if (!archetype_cond_match(*pArchetype, cond, entity))
						continue;

					req_del(*pArchetype);
				}
			}

			//! Removes the entity \param entity from anything referencing it.
			void rem_from_entities(Entity entity) {
				GAIA_PROF_SCOPE(World::rem_from_entities);

				const auto it = m_entityToArchetypeMap.find(EntityLookupKey(entity));
				if (it == m_entityToArchetypeMap.end())
					return;

				// Invalidate the singleton status if necessary
				if (!entity.pair()) {
					auto& ec = fetch(entity);
					if ((ec.flags & EntityContainerFlags::IsSingleton) != 0) {
						auto ids = ec.pArchetype->ids_view();
						const auto idx = core::get_index(ids, entity);
						if (idx != BadIndex)
							EntityBuilder::set_flag(ec.flags, EntityContainerFlags::IsSingleton, false);
					}
				}

				// Update archetypes of all affected entities
				const auto& archetypes = it->second;
				for (auto* pArchetype: archetypes) {
					if (pArchetype->is_req_del())
						continue;

					auto* pDstArchetype = calc_dst_archetype(pArchetype, entity);
					if (pDstArchetype != nullptr)
						move_to_archetype(*pArchetype, *pDstArchetype);
				}
			}

			//! Removes the entity \param entity from anything referencing it.
			//! Takes \param cond into account.
			void rem_from_entities(Entity entity, Pair cond) {
				GAIA_PROF_SCOPE(World::rem_from_entities);

				const auto it = m_entityToArchetypeMap.find(EntityLookupKey(entity));
				if (it == m_entityToArchetypeMap.end())
					return;

				// Invalidate the singleton status if necessary
				if (!entity.pair()) {
					auto& ec = fetch(entity);
					if ((ec.flags & EntityContainerFlags::IsSingleton) != 0) {
						auto ids = ec.pArchetype->ids_view();
						const auto idx = core::get_index(ids, entity);
						if (idx != BadIndex)
							EntityBuilder::set_flag(ec.flags, EntityContainerFlags::IsSingleton, false);
					}
				}

				const auto& archetypes = it->second;
				for (auto* pArchetype: archetypes) {
					if (pArchetype->is_req_del())
						continue;

					// Evaluate the condition if a valid pair is given
					if (!archetype_cond_match(*pArchetype, cond, entity))
						continue;

					auto* pDstArchetype = calc_dst_archetype(pArchetype, entity);
					if (pDstArchetype != nullptr)
						move_to_archetype(*pArchetype, *pDstArchetype);
				}
			}

			//! Handles specific conditions that might arise from deleting \param entity.
			//! Conditions are:
			//!   OnDelete - deleting an entity/pair
			//!   OnDeleteTarget - deleting a pair's target
			//! Reactions are:
			//!   Remove - removes the entity/pair from anything referencing it
			//!   Delete - delete everything referencing the entity
			//!   Error - error out when deleted
			//! These rules can be set up as:
			//!   e.add(Pair(OnDelete, Remove));
			void handle_del_entity(const EntityContainer& ec, Entity entity) {
				GAIA_PROF_SCOPE(World::handle_del_entity);

				GAIA_ASSERT(!is_wildcard(entity));

				if (entity.pair()) {
					if ((ec.flags & EntityContainerFlags::OnDelete_Error) != 0) {
						GAIA_ASSERT2(false, "Trying to delete entity that is forbidden to be deleted");
						GAIA_LOG_E(
								"Trying to delete pair [%u.%u] %s [%s] that is forbidden to be deleted", entity.id(), entity.gen(),
								name(entity), EntityKindString[entity.kind()]);
						return;
					}

					const auto tgt = get(entity.gen());
					const auto& ecTgt = fetch(tgt);
					if ((ecTgt.flags & EntityContainerFlags::OnDeleteTarget_Error) != 0) {
						GAIA_ASSERT2(false, "Trying to delete entity that is forbidden to be deleted (target restriction)");
						GAIA_LOG_E(
								"Trying to delete pair [%u.%u] %s [%s] that is forbidden to be deleted (target restriction)",
								entity.id(), entity.gen(), name(entity), EntityKindString[entity.kind()]);
						return;
					}

					if ((ecTgt.flags & EntityContainerFlags::OnDeleteTarget_Delete) != 0) {
						// Delete all entities referencing this one as a relationship pair's target
						req_del_entities_with(Pair(All, tgt), Pair(OnDeleteTarget, Delete));
					} else {
						// Remove from all entities referencing this one as a relationship pair's target
						rem_from_entities(Pair(All, tgt));
					}

					// This entity is supposed to be deleted. Nothing more for us to do here
					if (is_req_del(ec))
						return;

					if ((ec.flags & EntityContainerFlags::OnDelete_Delete) != 0) {
						// Delete all references to the entity
						req_del_entities_with(entity);
					} else {
						// Entities are only removed by default
						rem_from_entities(entity);
					}
				} else {
					if ((ec.flags & EntityContainerFlags::OnDelete_Error) != 0) {
						GAIA_ASSERT2(false, "Trying to delete entity that is forbidden to be deleted");
						GAIA_LOG_E(
								"Trying to delete entity [%u.%u] %s [%s] that is forbidden to be deleted", entity.id(), entity.gen(),
								name(entity), EntityKindString[entity.kind()]);
						return;
					}

					if ((ec.flags & EntityContainerFlags::OnDeleteTarget_Error) != 0) {
						GAIA_ASSERT2(false, "Trying to delete entity that is forbidden to be deleted (a pair's target)");
						GAIA_LOG_E(
								"Trying to delete entity [%u.%u] %s [%s] that is forbidden to be deleted (a pair's target)",
								entity.id(), entity.gen(), name(entity), EntityKindString[entity.kind()]);
						return;
					}

					if ((ec.flags & EntityContainerFlags::OnDeleteTarget_Delete) != 0) {
						// Delete all entities referencing this one as a relationship pair's target
						req_del_entities_with(Pair(All, entity), Pair(OnDeleteTarget, Delete));
					} else {
						// Remove from all entities referencing this one as a relationship pair's target
						rem_from_entities(Pair(All, entity));
					}

					// This entity is supposed to be deleted. Nothing more for us to do here
					if (is_req_del(ec))
						return;

					if ((ec.flags & EntityContainerFlags::OnDelete_Delete) != 0) {
						// Delete all references to the entity
						req_del_entities_with(entity);
					} else {
						// Entities are only removed by default
						rem_from_entities(entity);
					}
				}
			}

			//! Removes a graph connection with the surrounding archetypes.
			//! \param pArchetype Archetype we are removing an edge from
			//! \param edgeLeft An edge pointing towards the archetype on the left
			//! \param edgeEntity An entity which when followed from the left edge we reach the archetype
			void remove_edge_from_archetype(Archetype* pArchetype, ArchetypeGraphEdge edgeLeft, Entity edgeEntity) {
				GAIA_ASSERT(pArchetype != nullptr);

				const auto edgeLeftIt = m_archetypesById.find(ArchetypeIdLookupKey(edgeLeft.id, edgeLeft.hash));
				if (edgeLeftIt == m_archetypesById.end())
					return;

				auto* pArchetypeLeft = edgeLeftIt->second;
				GAIA_ASSERT(pArchetypeLeft != nullptr);

				// Remove the connection with the current archetype
				pArchetypeLeft->del_graph_edges(pArchetype, edgeEntity);

				// Traverse all archetypes on the right
				auto& archetypesRight = pArchetype->right_edges();
				for (auto& it: archetypesRight) {
					const auto& edgeRight = it.second;
					const auto edgeRightIt = m_archetypesById.find(ArchetypeIdLookupKey(edgeRight.id, edgeRight.hash));
					if (edgeRightIt == m_archetypesById.end())
						continue;

					auto* pArchetypeRight = edgeRightIt->second;

					// Remove the connection with the current archetype
					pArchetype->del_graph_edges(pArchetypeRight, it.first.entity());
				}
			}

			void remove_edges(Entity entityToRemove) {
				const auto it = m_entityToArchetypeMap.find(EntityLookupKey(entityToRemove));
				if (it == m_entityToArchetypeMap.end())
					return;

				const auto& archetypes = it->second;
				for (auto* pArchetype: archetypes)
					remove_edge_from_archetype(pArchetype, pArchetype->find_edge_left(entityToRemove), entityToRemove);
			}

			void remove_edges_from_pairs(Entity entity) {
				if (entity.pair())
					return;

				// Make sure to remove all pairs containing the entity
				// (X, something)
				const auto* tgts = targets(entity);
				if (tgts != nullptr) {
					for (auto target: *tgts)
						remove_edges(Pair(entity, target.entity()));
				}
				// (something, X)
				const auto* rels = relations(entity);
				if (rels != nullptr) {
					for (auto relation: *rels)
						remove_edges(Pair(relation.entity(), entity));
				}
			}

			//! Deletes any edges containing the entity from the archetype graph.
			//! \param entity Entity to delete
			void del_graph_edges(Entity entity) {
				remove_edges(entity);
				remove_edges_from_pairs(entity);
			}

			void del_reltgt_tgtrel_pairs(Entity entity) {
				auto delPair = [](PairMap& map, Entity source, Entity remove) {
					auto itTargets = map.find(EntityLookupKey(source));
					if (itTargets != map.end()) {
						auto& targets = itTargets->second;
						targets.erase(EntityLookupKey(remove));
					}
				};

				if (entity.pair()) {
					const auto it = m_recs.pairs.find(EntityLookupKey(entity));
					if (it != m_recs.pairs.end()) {
						// Delete the container record
						m_recs.pairs.erase(it);

						// Update pairs
						auto rel = get(entity.id());
						auto tgt = get(entity.gen());

						delPair(m_relationsToTargets, rel, tgt);
						delPair(m_relationsToTargets, All, tgt);
						delPair(m_targetsToRelations, tgt, rel);
						delPair(m_targetsToRelations, All, rel);
					}
				} else {
					// Update the container record
					auto& ec = m_recs.entities.free(entity);

					// If this is a singleton entity its archetype needs to be deleted
					if ((ec.flags & EntityContainerFlags::IsSingleton) != 0)
						req_del(*ec.pArchetype);

					ec.pArchetype = nullptr;
					ec.pChunk = nullptr;
					EntityBuilder::set_flag(ec.flags, EntityContainerFlags::DeleteRequested, false);

					// Update pairs
					delPair(m_relationsToTargets, All, entity);
					delPair(m_targetsToRelations, All, entity);
					m_relationsToTargets.erase(EntityLookupKey(entity));
					m_targetsToRelations.erase(EntityLookupKey(entity));
				}
			}

			//! Invalidates the entity record, effectively deleting it.
			//! \param entity Entity to delete
			void invalidate_entity(Entity entity) {
				del_graph_edges(entity);
				del_reltgt_tgtrel_pairs(entity);
				del_entity_archetype_pairs(entity);
			}

			//! Associates an entity with a chunk.
			//! \param entity Entity to associate with a chunk
			//! \param pArchetype Target archetype
			//! \param pChunk Target chunk (with the archetype \param pArchetype)
			void store_entity(EntityContainer& ec, Entity entity, Archetype* pArchetype, Chunk* pChunk) {
				GAIA_ASSERT(pArchetype != nullptr);
				GAIA_ASSERT(pChunk != nullptr);
				GAIA_ASSERT(
						!pChunk->locked() && "Entities can't be added while their chunk is being iterated "
																 "(structural changes are forbidden during this time!)");

				ec.pArchetype = pArchetype;
				ec.pChunk = pChunk;
				ec.row = pChunk->add_entity(entity);
				GAIA_ASSERT(entity.pair() || ec.gen == entity.gen());
				ec.dis = 0;
			}

			//! Moves an entity along with all its generic components from its current chunk to another one.
			//! \param entity Entity to move
			//! \param dstArchetype Destination archetype
			//! \param dstChunk Destination chunk
			void move_entity(Entity entity, Archetype& dstArchetype, Chunk& dstChunk) {
				GAIA_PROF_SCOPE(World::move_entity);

				auto* pDstChunk = &dstChunk;

				auto& ec = fetch(entity);
				auto* pSrcChunk = ec.pChunk;

				GAIA_ASSERT(pDstChunk != pSrcChunk);

				const auto srcRow0 = ec.row;
				const auto dstRow = pDstChunk->add_entity(entity);
				const bool wasEnabled = !ec.dis;

				auto& srcArchetype = *ec.pArchetype;

				// Make sure the old entity becomes enabled now
				srcArchetype.enable_entity(pSrcChunk, srcRow0, true, m_recs);
				// Enabling the entity might have changed its chunk index so fetch it again
				const auto srcRow = ec.row;

				// Move data from the old chunk to the new one
				if (dstArchetype.id() == srcArchetype.id()) {
					pDstChunk->move_entity_data(entity, dstRow, m_recs);
				} else {
					pDstChunk->move_foreign_entity_data(pSrcChunk, srcRow, pDstChunk, dstRow);
				}

				// Remove the entity record from the old chunk
				remove_entity(srcArchetype, *pSrcChunk, srcRow);

				// An entity might have moved, try updating the free chunk index
				dstArchetype.try_update_free_chunk_idx();

				// Bring the entity container record up-to-date
				ec.pArchetype = &dstArchetype;
				ec.pChunk = pDstChunk;
				ec.row = (uint16_t)dstRow;

				// Make the enabled state in the new chunk match the original state
				dstArchetype.enable_entity(pDstChunk, dstRow, wasEnabled, m_recs);

				// End-state validation
				GAIA_ASSERT(valid(entity));
				validate_chunk(pSrcChunk);
				validate_chunk(pDstChunk);
				validate_entities();
			}

			//! Moves an entity along with all its generic components from its current chunk to another one in a new
			//! archetype. \param entity Entity to move \param dstArchetype Target archetype
			void move_entity(Entity entity, Archetype& dstArchetype) {
				// Archetypes need to be different
				const auto& ec = fetch(entity);
				if (ec.pArchetype == &dstArchetype)
					return;

				auto* pDstChunk = dstArchetype.foc_free_chunk();
				move_entity(entity, dstArchetype, *pDstChunk);
			}

			void validate_archetype_edges([[maybe_unused]] const Archetype* pArchetype) const {
#if GAIA_ECS_VALIDATE_ARCHETYPE_GRAPH && GAIA_ASSERT_ENABLED
				GAIA_ASSERT(pArchetype != nullptr);

				// Validate left edges
				const auto& archetypesLeft = pArchetype->left_edges();
				for (const auto& it: archetypesLeft) {
					const auto& edge = it.second;
					const auto edgeIt = m_archetypesById.find(ArchetypeIdLookupKey(edge.id, edge.hash));
					if (edgeIt == m_archetypesById.end())
						continue;

					const auto entity = it.first.entity();
					const auto* pArchetypeRight = edgeIt->second;

					// Edge must be found
					const auto edgeRight = pArchetypeRight->find_edge_right(entity);
					GAIA_ASSERT(edgeRight != ArchetypeIdHashPairBad);

					// The edge must point to pArchetype
					const auto it2 = m_archetypesById.find(ArchetypeIdLookupKey(edgeRight.id, edgeRight.hash));
					GAIA_ASSERT(it2 != m_archetypesById.end());
					const auto* pArchetype2 = it2->second;
					GAIA_ASSERT(pArchetype2 == pArchetype);
				}

				// Validate right edges
				const auto& archetypesRight = pArchetype->right_edges();
				for (const auto& it: archetypesRight) {
					const auto& edge = it.second;
					const auto edgeIt = m_archetypesById.find(ArchetypeIdLookupKey(edge.id, edge.hash));
					if (edgeIt == m_archetypesById.end())
						continue;

					const auto entity = it.first.entity();
					const auto* pArchetypeRight = edgeIt->second;

					// Edge must be found
					const auto edgeLeft = pArchetypeRight->find_edge_left(entity);
					GAIA_ASSERT(edgeLeft != ArchetypeIdHashPairBad);

					// The edge must point to pArchetype
					const auto it2 = m_archetypesById.find(ArchetypeIdLookupKey(edgeLeft.id, edgeLeft.hash));
					GAIA_ASSERT(it2 != m_archetypesById.end());
					const auto* pArchetype2 = it2->second;
					GAIA_ASSERT(pArchetype2 == pArchetype);
				}
#endif
			}

			//! Verifies than the implicit linked list of entities is valid
			void validate_entities() const {
#if GAIA_ECS_VALIDATE_ENTITY_LIST
				m_recs.entities.validate();
#endif
			}

			//! Verifies that the chunk is valid
			void validate_chunk([[maybe_unused]] Chunk* pChunk) const {
#if GAIA_ECS_VALIDATE_CHUNKS && GAIA_ASSERT_ENABLED
				GAIA_ASSERT(pChunk != nullptr);

				if (!pChunk->empty()) {
					// Make sure a proper amount of entities reference the chunk
					uint32_t cnt = 0;
					for (const auto& ec: m_recs.entities) {
						if (ec.pChunk != pChunk)
							continue;
						++cnt;
					}
					for (const auto& pair: m_recs.pairs) {
						if (pair.second.pChunk != pChunk)
							continue;
						++cnt;
					}
					GAIA_ASSERT(cnt == pChunk->size());
				} else {
					// Make sure no entities reference the chunk
					for (const auto& ec: m_recs.entities) {
						GAIA_ASSERT(ec.pChunk != pChunk);
					}
					for (const auto& pair: m_recs.pairs) {
						GAIA_ASSERT(pair.second.pChunk != pChunk);
					}
				}
#endif
			}

			template <bool IsOwned>
			void name_inter(Entity entity, const char* name, uint32_t len) {
				GAIA_ASSERT(!entity.pair());
				if (entity.pair())
					return;

				GAIA_ASSERT(valid(entity));

				// When nullptr is passed for the name it means the user wants to delete the current name
				if (name == nullptr) {
					GAIA_ASSERT(len == 0);
					del_name(entity);
					return;
				}

				// Make sure EntityDesc is added
				add<EntityDesc>(entity);

				auto res =
						m_nameToEntity.try_emplace(len == 0 ? EntityNameLookupKey(name) : EntityNameLookupKey(name, len), entity);
				// Make sure the name is unique. Ignore setting the same name twice on the same entity
				GAIA_ASSERT(res.second || res.first->second == entity);

				// Not a unique name, nothing to do
				if (!res.second)
					return;

				auto& key = res.first->first;

				if constexpr (IsOwned) {
					// Allocate enough storage for the name
					char* entityStr = (char*)mem::mem_alloc(key.len() + 1);
					memcpy((void*)entityStr, (const void*)name, key.len() + 1);
					entityStr[key.len()] = 0;

					// Update the map so it points to the newly allocated string.
					// We replace the pointer we provided in try_emplace with an internally allocated string.
					auto p = robin_hood::pair(std::make_pair(EntityNameLookupKey(entityStr, key.len(), 1, {key.hash()}), entity));
					res.first->swap(p);

					// Update the entity string pointer
					sset<EntityDesc>(entity) = {entityStr, key.len()};
				} else {
					// We tell the map the string is non-owned.
					auto p = robin_hood::pair(std::make_pair(EntityNameLookupKey(key.str(), key.len(), 0, {key.hash()}), entity));
					res.first->swap(p);

					// Update the entity string pointer
					sset<EntityDesc>(entity) = {name, key.len()};
				}
			}

			//! If \tparam CheckIn is true, checks if \param entity is located in \param entityBase.
			//! If \tparam CheckIn is false, checks if \param entity inherits from \param entityBase.
			//! True if \param entity inherits from/is located in \param entityBase. False otherwise.
			template <bool CheckIn>
			GAIA_NODISCARD bool is_inter(Entity entity, Entity entityBase) const {
				GAIA_ASSERT(valid_entity(entity));
				GAIA_ASSERT(valid_entity(entityBase));

				// Pairs are not supported
				if (entity.pair() || entityBase.pair())
					return false;

				if constexpr (!CheckIn) {
					if (entity == entityBase)
						return true;
				}

				const auto& ec = m_recs.entities[entity.id()];
				const auto* pArchetype = ec.pArchetype;

				// Early exit if there are no Is relationship pairs on the archetype
				if (pArchetype->pairs_is() == 0)
					return false;

				for (uint32_t i = 0; i < pArchetype->pairs_is(); ++i) {
					auto e = pArchetype->entity_from_pairs_as_idx(i);
					const auto& ecTarget = m_recs.entities[e.gen()];
					auto target = ecTarget.pChunk->entity_view()[ecTarget.row];
					if (target == entityBase)
						return true;

					if (is_inter<CheckIn>(target, entityBase))
						return true;
				}

				return false;
			}

			//! Traverse the (Is, X) relationships all the way to their source
			template <bool CheckIn, typename Func>
			GAIA_NODISCARD void as_up_trav(Entity entity, Func func) {
				GAIA_ASSERT(valid_entity(entity));

				// Pairs are not supported
				if (entity.pair())
					return;

				if constexpr (!CheckIn) {
					func(entity);
				}

				const auto& ec = m_recs.entities[entity.id()];
				const auto* pArchetype = ec.pArchetype;

				// Early exit if there are no Is relationship pairs on the archetype
				if (pArchetype->pairs_is() == 0)
					return;

				for (uint32_t i = 0; i < pArchetype->pairs_is(); ++i) {
					auto e = pArchetype->entity_from_pairs_as_idx(i);
					const auto& ecTarget = m_recs.entities[e.gen()];
					auto target = ecTarget.pChunk->entity_view()[ecTarget.row];
					func(target);

					as_up_trav<CheckIn>(target, func);
				}
			}

			template <typename T>
			const ComponentCacheItem& reg_core_entity(Entity id, Archetype* pArchetype) {
				auto comp = add(*pArchetype, id.entity(), id.pair(), id.kind());
				const auto& ci = comp_cache_mut().add<T>(id);
				GAIA_ASSERT(ci.entity == id);
				GAIA_ASSERT(comp == id);
				(void)comp;
				return ci;
			}

			template <typename T>
			const ComponentCacheItem& reg_core_entity(Entity id) {
				return reg_core_entity<T>(id, m_pRootArchetype);
			}

			void init();

			void done() {
				cleanup_inter();

#if GAIA_ECS_CHUNK_ALLOCATOR
				ChunkAllocator::get().flush();
#endif
			}

			//! Assigns an entity to a given archetype.
			//! \param archetype Archetype the entity should inherit
			//! \param entity Entity
			void assign_entity(Entity entity, Archetype& archetype) {
				GAIA_ASSERT(!entity.pair());

				auto* pChunk = archetype.foc_free_chunk();
				store_entity(m_recs.entities[entity.id()], entity, &archetype, pChunk);
				archetype.try_update_free_chunk_idx();

				// Call constructors for the generic components on the newly added entity if necessary
				pChunk->call_gen_ctors(pChunk->size() - 1, 1);

#if GAIA_ASSERT_ENABLED
				const auto& ec = m_recs.entities[entity.id()];
				GAIA_ASSERT(ec.pChunk == pChunk);
				auto entityExpected = pChunk->entity_view()[ec.row];
				GAIA_ASSERT(entityExpected == entity);
#endif
			}

			//! Assigns a pair entity to a given archetype.
			//! \param archetype Archetype the pair should inherit
			//! \param entity Pair entity
			void assign_pair(Entity entity, Archetype& archetype) {
				GAIA_ASSERT(entity.pair());

				// Pairs are always added to m_pEntityArchetype initially and this can't change.
				GAIA_ASSERT(&archetype == m_pEntityArchetype);

				const auto it = m_recs.pairs.find(EntityLookupKey(entity));
				if (it != m_recs.pairs.end())
					return;

				// Update the container record
				EntityContainer ec{};
				ec.idx = entity.id();
				ec.gen = entity.gen();
				ec.pair = 1;
				ec.ent = 1;
				ec.kind = EntityKind::EK_Gen;

				auto* pChunk = archetype.foc_free_chunk();
				store_entity(ec, entity, &archetype, pChunk);
				archetype.try_update_free_chunk_idx();

				m_recs.pairs.emplace(EntityLookupKey(entity), GAIA_MOV(ec));

				// Update pair mappings
				const auto rel = get(entity.id());
				const auto tgt = get(entity.gen());

				auto addPair = [](PairMap& map, Entity source, Entity add) {
					auto& ents = map[EntityLookupKey(source)];
					ents.insert(EntityLookupKey(add));
				};

				addPair(m_relationsToTargets, rel, tgt);
				addPair(m_relationsToTargets, All, tgt);
				addPair(m_targetsToRelations, tgt, rel);
				addPair(m_targetsToRelations, All, rel);
			}

			//! Creates a new entity of a given archetype.
			//! \param archetype Archetype the entity should inherit.
			//! \param isEntity True if entity, false otherwise.
			//! \param isPair True if pair, false otherwise.
			//! \param kind Component kind.
			//! \return New entity.
			GAIA_NODISCARD Entity add(Archetype& archetype, bool isEntity, bool isPair, EntityKind kind) {
				EntityContainerCtx ctx{isEntity, isPair, kind};
				const auto entity = m_recs.entities.alloc(&ctx);
				assign_entity(entity, archetype);
				return entity;
			}

			//! Creates multiple entity of a given archetype at once.
			//! More efficient than creating entities individually.
			//! \param archetype Archetype the entity should inherit.
			//! \param count Number of entities to create.
			//! \param func void(Entity) functor executed for each added entity.
			template <typename Func>
			void add_entity_n(Archetype& archetype, uint32_t count, Func func) {
				EntityContainerCtx ctx{true, false, EntityKind::EK_Gen};

				uint32_t left = count;
				do {
					auto* pChunk = archetype.foc_free_chunk();
					const uint32_t originalChunkSize = pChunk->size();
					const uint32_t freeSlotsInChunk = pChunk->capacity() - originalChunkSize;
					const uint32_t toCreate = core::get_min(freeSlotsInChunk, left);

					GAIA_FOR(toCreate) {
						const auto entityNew = m_recs.entities.alloc(&ctx);
						auto& ecNew = m_recs.entities[entityNew.id()];
						store_entity(ecNew, entityNew, &archetype, pChunk);

#if GAIA_ASSERT_ENABLED
						GAIA_ASSERT(ecNew.pChunk == pChunk);
						auto entityExpected = pChunk->entity_view()[ecNew.row];
						GAIA_ASSERT(entityExpected == entityNew);
#endif
					}

					// New entities were added, try updating the free chunk index
					archetype.try_update_free_chunk_idx();

					// Call constructors for the generic components on the newly added entity if necessary
					pChunk->call_gen_ctors(originalChunkSize, toCreate);

					// Call functors
					{
						auto entities = pChunk->entity_view();
						GAIA_FOR2(originalChunkSize, pChunk->size()) func(entities[i]);
					}

					left -= toCreate;
				} while (left > 0);
			}

			//! Garbage collection
			void gc() {
				GAIA_PROF_SCOPE(World::gc);

				del_empty_chunks();
				defrag_chunks(m_defragEntitiesPerTick);
				del_empty_archetypes();
			}

		public:
			QuerySerBuffer& query_buffer(QueryId& serId) {
				// No serialization id set on the query, try creating a new record
				if GAIA_UNLIKELY (serId == QueryIdBad) {
#if GAIA_ASSERT_ENABLED
					uint32_t safetyCounter = 0;
#endif

					while (true) {
#if GAIA_ASSERT_ENABLED
						// Make sure we don't cross some safety threshold
						++safetyCounter;
						GAIA_ASSERT(safetyCounter < 100000);
#endif

						serId = ++m_nextQuerySerId;
						// Make sure we do not overflow
						GAIA_ASSERT(serId != 0);

						// If the id is already found, try again.
						// Note, this is essentially never going to repeat. We would have to prepare millions if
						// not billions of queries for which we only added inputs but never queried them.
						auto ret = m_querySerMap.try_emplace(serId);
						if (!ret.second)
							continue;

						return ret.first->second;
					};
				}

				return m_querySerMap[serId];
			}

			void query_buffer_reset(QueryId& serId) {
				auto it = m_querySerMap.find(serId);
				if (it == m_querySerMap.end())
					return;

				m_querySerMap.erase(it);
				serId = QueryIdBad;
			}

			void invalidate_queries_for_entity(Pair is_pair) {
				GAIA_ASSERT(is_pair.first() == Is);

				// We still need to handle invalidation "down-the-tree".
				// E.g. following setup:
				// q = w.query().all({Is,animal});
				// w.as(wolf, carnivore);
				// w.as(carnivore, animal);
				// q.each() ...; // animal, carnivore, wolf
				// w.del(wolf, {Is,carnivore}) // wolf no longer a carnivore and thus no longer an animal
				// After this deletion, we need to invalidate "q" because wolf is no longer an animal
				// and we don't want q to include it.
				// q.each() ...; // animal

				auto e = is_pair.second();
				as_up_trav<false>(e, [&](Entity target) {
					// Invalidate all queries that contain  everything in our path.
					m_queryCache.invalidate_queries_for_entity(EntityLookupKey(Pair{Is, target}));
				});
			}
		};

		GAIA_NODISCARD inline QuerySerBuffer& query_buffer(World& world, QueryId& serId) {
			return world.query_buffer(serId);
		}

		inline void query_buffer_reset(World& world, QueryId& serId) {
			world.query_buffer_reset(serId);
		}

		GAIA_NODISCARD inline const ComponentCache& comp_cache(const World& world) {
			return world.comp_cache();
		}

		GAIA_NODISCARD inline ComponentCache& comp_cache_mut(World& world) {
			return world.comp_cache_mut();
		}

		template <typename T>
		GAIA_NODISCARD inline const ComponentCacheItem& comp_cache_add(World& world) {
			return world.add<T>();
		}

		GAIA_NODISCARD inline Entity entity_from_id(const World& world, EntityId id) {
			return world.get(id);
		}

		GAIA_NODISCARD inline bool valid(const World& world, Entity entity) {
			return world.valid(entity);
		}

		GAIA_NODISCARD inline bool is(const World& world, Entity entity, Entity baseEntity) {
			return world.is(entity, baseEntity);
		}

		GAIA_NODISCARD inline bool is_base(const World& world, Entity entity) {
			return world.is_base(entity);
		}

		GAIA_NODISCARD inline Archetype* archetype_from_entity(const World& world, Entity entity) {
			const auto& ec = world.fetch(entity);
			if (World::is_req_del(ec))
				return nullptr;

			return ec.pArchetype;
		}

		GAIA_NODISCARD inline const char* entity_name(const World& world, Entity entity) {
			return world.name(entity);
		}

		GAIA_NODISCARD inline const char* entity_name(const World& world, EntityId entityId) {
			return world.name(entityId);
		}

		template <typename Func>
		void as_relations_trav(const World& world, Entity target, Func func) {
			world.as_relations_trav(target, func);
		}

		template <typename Func>
		bool as_relations_trav_if(const World& world, Entity target, Func func) {
			return world.as_relations_trav_if(target, func);
		}

		template <typename Func>
		void as_targets_trav(const World& world, Entity relation, Func func) {
			world.as_targets_trav(relation, func);
		}

		template <typename Func>
		void as_targets_trav_if(const World& world, Entity relation, Func func) {
			return world.as_targets_trav_if(relation, func);
		}
	} // namespace ecs
} // namespace gaia

../include/gaia/ecs/system.inl"

namespace gaia {
	namespace ecs {
		inline void World::init() {
			// Register the root archetype
			{
				m_pRootArchetype = create_archetype({});
				m_pRootArchetype->set_hashes({calc_lookup_hash({})});
				reg_archetype(m_pRootArchetype);
			}

			(void)reg_core_entity<Core_>(Core);

			// Entity archetype matches the root archetype for now
			m_pEntityArchetype = m_pRootArchetype;

			// Register the component archetype (entity + EntityDesc + Component)
			{
				Archetype* pCompArchetype{};
				{
					const auto id = GAIA_ID(EntityDesc);
					const auto& ci = reg_core_entity<EntityDesc>(id);
					EntityBuilder(*this, id).add_inter_init(ci.entity);
					sset<EntityDesc>(id) = {ci.name.str(), ci.name.len()};
					pCompArchetype = m_recs.entities[id.id()].pArchetype;
				}
				{
					const auto id = GAIA_ID(Component);
					const auto& ci = reg_core_entity<Component>(id, pCompArchetype);
					EntityBuilder(*this, id).add_inter_init(ci.entity);
					acc_mut(id)
							// Entity descriptor
							.sset<EntityDesc>({ci.name.str(), ci.name.len()})
							// Component
							.sset<Component>(ci.comp);
					m_pCompArchetype = m_recs.entities[id.id()].pArchetype;
				}
			}

			// Core components.
			// Their order must correspond to the value sequence in id.h.
			{
				(void)reg_core_entity<OnDelete_>(OnDelete);
				(void)reg_core_entity<OnDeleteTarget_>(OnDeleteTarget);
				(void)reg_core_entity<Remove_>(Remove);
				(void)reg_core_entity<Delete_>(Delete);
				(void)reg_core_entity<Error_>(Error);
				(void)reg_core_entity<Requires_>(Requires);
				(void)reg_core_entity<CantCombine_>(CantCombine);
				(void)reg_core_entity<Exclusive_>(Exclusive);
				(void)reg_core_entity<Acyclic_>(Acyclic);
				(void)reg_core_entity<Traversable_>(Traversable);
				(void)reg_core_entity<All_>(All);
				(void)reg_core_entity<ChildOf_>(ChildOf);
				(void)reg_core_entity<Is_>(Is);
				(void)reg_core_entity<System2_>(System2);
				(void)reg_core_entity<DependsOn_>(DependsOn);

				(void)reg_core_entity<_Var0>(Var0);
				(void)reg_core_entity<_Var1>(Var1);
				(void)reg_core_entity<_Var2>(Var2);
				(void)reg_core_entity<_Var3>(Var3);
				(void)reg_core_entity<_Var4>(Var4);
				(void)reg_core_entity<_Var5>(Var5);
				(void)reg_core_entity<_Var6>(Var6);
				(void)reg_core_entity<_Var7>(Var7);
			}

			// Add special properties for core components.
			// Their order must correspond to the value sequence in id.h.
			{
				EntityBuilder(*this, Core) //
						.add(Core)
						.add(Pair(OnDelete, Error));
				EntityBuilder(*this, GAIA_ID(EntityDesc)) //
						.add(Core)
						.add(Pair(OnDelete, Error));
				EntityBuilder(*this, GAIA_ID(Component)) //
						.add(Core)
						.add(Pair(OnDelete, Error));
				EntityBuilder(*this, OnDelete) //
						.add(Core)
						.add(Exclusive)
						.add(Pair(OnDelete, Error));
				EntityBuilder(*this, OnDeleteTarget) //
						.add(Core)
						.add(Exclusive)
						.add(Pair(OnDelete, Error));
				EntityBuilder(*this, Remove) //
						.add(Core)
						.add(Pair(OnDelete, Error));
				EntityBuilder(*this, Delete) //
						.add(Core)
						.add(Pair(OnDelete, Error));
				EntityBuilder(*this, Error) //
						.add(Core)
						.add(Pair(OnDelete, Error));
				EntityBuilder(*this, All) //
						.add(Core)
						.add(Pair(OnDelete, Error));
				EntityBuilder(*this, Requires) //
						.add(Core)
						.add(Acyclic)
						.add(Pair(OnDelete, Error));
				EntityBuilder(*this, CantCombine) //
						.add(Core)
						.add(Acyclic)
						.add(Pair(OnDelete, Error));
				EntityBuilder(*this, Exclusive) //
						.add(Core)
						.add(Pair(OnDelete, Error))
						.add(Acyclic);
				EntityBuilder(*this, Acyclic) //
						.add(Core)
						.add(Pair(OnDelete, Error));
				EntityBuilder(*this, Traversable) //
						.add(Core)
						.add(Pair(OnDelete, Error));

				EntityBuilder(*this, ChildOf) //
						.add(Core)
						.add(Acyclic)
						.add(Exclusive)
						.add(Traversable)
						.add(Pair(OnDelete, Error))
						.add(Pair(OnDeleteTarget, Delete));
				EntityBuilder(*this, Is) //
						.add(Core)
						.add(Acyclic)
						.add(Pair(OnDelete, Error));

				EntityBuilder(*this, System2) //
						.add(Core)
						.add(Acyclic)
						.add(Pair(OnDelete, Error));
				EntityBuilder(*this, DependsOn) //
						.add(Core)
						.add(Acyclic)
						.add(Pair(OnDelete, Error));

				EntityBuilder(*this, Var0) //
						.add(Core)
						.add(Pair(OnDelete, Error));
				EntityBuilder(*this, Var1) //
						.add(Core)
						.add(Pair(OnDelete, Error));
				EntityBuilder(*this, Var2) //
						.add(Core)
						.add(Pair(OnDelete, Error));
				EntityBuilder(*this, Var3) //
						.add(Core)
						.add(Pair(OnDelete, Error));
				EntityBuilder(*this, Var4) //
						.add(Core)
						.add(Pair(OnDelete, Error));
				EntityBuilder(*this, Var5) //
						.add(Core)
						.add(Pair(OnDelete, Error));
				EntityBuilder(*this, Var6) //
						.add(Core)
						.add(Pair(OnDelete, Error));
				EntityBuilder(*this, Var7) //
						.add(Core)
						.add(Pair(OnDelete, Error));
			}

			// Remove all archetypes with no chunks. We don't want any leftovers after
			// archetype movements.
			{
				for (uint32_t i = 1; i < m_archetypes.size(); ++i) {
					auto* pArchetype = m_archetypes[i];
					if (!pArchetype->chunks().empty())
						continue;

					// Request deletion the standard way.
					// We could simply add archetypes into m_archetypesToDel but this way
					// we can actually replicate what the system really does on the inside
					// and it will require more work at the cost of easier maintenance.
					// The amount of archetypes cleanup is very small after init and the code
					// only runs after the world is created so this is not a big deal.
					req_del(*pArchetype);
				}

				// Cleanup
				{
					del_finalize();
					while (!m_chunksToDel.empty() || !m_archetypesToDel.empty())
						gc();

					// Make sure everything has been cleared
					GAIA_ASSERT(m_reqArchetypesToDel.empty());
					GAIA_ASSERT(m_chunksToDel.empty());
					GAIA_ASSERT(m_archetypesToDel.empty());
				}

				sort_archetypes();

				// Make sure archetypes have valid graphs after the cleanup
				for (const auto* pArchetype: m_archetypes)
					validate_archetype_edges(pArchetype);
			}

			// Make sure archetype pointers are up-to-date
			m_pCompArchetype = m_recs.entities[GAIA_ID(Component).id()].pArchetype;

#if GAIA_SYSTEMS_ENABLED
			// Initialize the systems query
			systems_init();
#endif
		}

		inline GroupId
		group_by_func_default([[maybe_unused]] const World& world, const Archetype& archetype, Entity groupBy) {
			if (archetype.pairs() > 0) {
				auto ids = archetype.ids_view();
				for (auto id: ids) {
					if (!id.pair() || id.id() != groupBy.id())
						continue;

					// Consider the pair's target the groupId
					return id.gen();
				}
			}

			// No group
			return 0;
		}
	} // namespace ecs
} // namespace gaia

#if GAIA_SYSTEMS_ENABLED
namespace gaia {
	namespace ecs {
		inline void World::systems_init() {
			m_systemsQuery = query().all<System2_&>();
		}

		inline void World::systems_run() {
			m_systemsQuery.each([](ecs::Iter& it) {
				auto se_view = it.sview_mut<ecs::System2_>(0);
				GAIA_EACH(it) {
					auto& sys = se_view[i];
					sys.exec();
				}
			});
		}

		inline SystemBuilder World::system() {
			// Create the system
			auto sysEntity = add();
			EntityBuilder(*this, sysEntity) //
					.add<System2_>();

			auto ss = acc_mut(sysEntity);
			auto& sys = ss.smut<System2_>();
			{
				sys.entity = sysEntity;
				sys.query = query();
			}
			return SystemBuilder(*this, sysEntity);
		}
	} // namespace ecs
} // namespace gaia
#endif

/*** End of inlined file: world.h ***/

namespace gaia {
	namespace ecs {
		struct TempEntity final {
			uint32_t id;
		};

		//! Buffer for deferred execution of some operations on entities.
		//!
		//! Adding and removing components and entities inside World::each or can result
		//! in changes of archetypes or chunk structure. This would lead to undefined behavior.
		//! Therefore, such operations have to be executed after the loop is done.
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

			enum CommandBufferCmdType : uint8_t {
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

			struct CommandBufferCmd {
				CommandBufferCmdType id;
			};

			struct CreateEntityCmd: CommandBufferCmd {};
			struct CreateEntityFromArchetypeCmd: CommandBufferCmd {
				uint64_t archetypePtr;

				void commit(CommandBufferCtx& ctx) const {
					auto* pArchetype = (Archetype*)archetypePtr;
					[[maybe_unused]] const auto res =
							ctx.entityMap.try_emplace(ctx.entities++, ctx.world.add(*pArchetype, true, false, EntityKind::EK_Gen));
					GAIA_ASSERT(res.second);
				}
			};
			struct CreateEntityFromEntityCmd: CommandBufferCmd {
				Entity entity;

				void commit(CommandBufferCtx& ctx) const {
					[[maybe_unused]] const auto res = ctx.entityMap.try_emplace(ctx.entities++, ctx.world.copy(entity));
					GAIA_ASSERT(res.second);
				}
			};
			struct DeleteEntityCmd: CommandBufferCmd {
				Entity entity;

				void commit(CommandBufferCtx& ctx) const {
					ctx.world.del(entity);
				}
			};
			struct AddComponentCmd: CommandBufferCmd {
				Entity entity;
				Entity object;

				void commit(CommandBufferCtx& ctx) const {
					World::EntityBuilder(ctx.world, entity).add(object);

#if GAIA_ASSERT_ENABLED
					[[maybe_unused]] uint32_t indexInChunk{};
					[[maybe_unused]] auto* pChunk = ctx.world.get_chunk(entity, indexInChunk);
					GAIA_ASSERT(pChunk != nullptr);
#endif
				}
			};
			struct AddComponentWithDataCmd: CommandBufferCmd {
				Entity entity;
				Entity object;

				void commit(CommandBufferCtx& ctx) const {
					World::EntityBuilder(ctx.world, entity).add(object);

					uint32_t indexInChunk{};
					auto* pChunk = ctx.world.get_chunk(entity, indexInChunk);
					GAIA_ASSERT(pChunk != nullptr);

					if (object.kind() == EntityKind::EK_Uni)
						indexInChunk = 0;

					// Component data
					const auto compIdx = pChunk->comp_idx(object);
					auto* pComponentData = (void*)pChunk->comp_ptr_mut(compIdx, indexInChunk);
					ctx.load_comp(ctx.world.comp_cache(), pComponentData, object);
				}
			};
			struct AddComponentToTempEntityCmd: CommandBufferCmd {
				TempEntity tempEntity;
				Entity object;

				void commit(CommandBufferCtx& ctx) const {
					// For delayed entities we have to do a look in our map
					// of temporaries and find a link there
					const auto it = ctx.entityMap.find(tempEntity.id);
					// Link has to exist!
					GAIA_ASSERT(it != ctx.entityMap.end());

					Entity entity = it->second;
					World::EntityBuilder(ctx.world, entity).add(object);

#if GAIA_ASSERT_ENABLED
					[[maybe_unused]] uint32_t indexInChunk{};
					[[maybe_unused]] auto* pChunk = ctx.world.get_chunk(entity, indexInChunk);
					GAIA_ASSERT(pChunk != nullptr);
#endif
				}
			};
			struct AddComponentWithDataToTempEntityCmd: CommandBufferCmd {
				TempEntity tempEntity;
				Entity object;

				void commit(CommandBufferCtx& ctx) const {
					// For delayed entities we have to do a look in our map
					// of temporaries and find a link there
					const auto it = ctx.entityMap.find(tempEntity.id);
					// Link has to exist!
					GAIA_ASSERT(it != ctx.entityMap.end());

					Entity entity = it->second;
					World::EntityBuilder(ctx.world, entity).add(object);

					uint32_t indexInChunk{};
					auto* pChunk = ctx.world.get_chunk(entity, indexInChunk);
					GAIA_ASSERT(pChunk != nullptr);

					if (object.kind() == EntityKind::EK_Uni)
						indexInChunk = 0;

					// Component data
					const auto compIdx = pChunk->comp_idx(object);
					auto* pComponentData = (void*)pChunk->comp_ptr_mut(compIdx, indexInChunk);
					ctx.load_comp(ctx.world.comp_cache(), pComponentData, object);
				}
			};
			struct SetComponentCmd: CommandBufferCmd {
				Entity entity;
				Entity object;

				void commit(CommandBufferCtx& ctx) const {
					const auto& ec = ctx.world.m_recs.entities[entity.id()];
					const auto row = object.kind() == EntityKind::EK_Uni ? 0U : ec.row;

					// Component data
					const auto compIdx = ec.pChunk->comp_idx(object);
					auto* pComponentData = (void*)ec.pChunk->comp_ptr_mut(compIdx, row);
					ctx.load_comp(ctx.world.comp_cache(), pComponentData, object);
				}
			};
			struct SetComponentOnTempEntityCmd: CommandBufferCmd {
				TempEntity tempEntity;
				Entity object;

				void commit(CommandBufferCtx& ctx) const {
					// For delayed entities we have to do a look in our map
					// of temporaries and find a link there
					const auto it = ctx.entityMap.find(tempEntity.id);
					// Link has to exist!
					GAIA_ASSERT(it != ctx.entityMap.end());

					Entity entity = it->second;

					const auto& ec = ctx.world.m_recs.entities[entity.id()];
					const auto row = object.kind() == EntityKind::EK_Uni ? 0U : ec.row;

					// Component data
					const auto compIdx = ec.pChunk->comp_idx(object);
					auto* pComponentData = (void*)ec.pChunk->comp_ptr_mut(compIdx, row);
					ctx.load_comp(ctx.world.comp_cache(), pComponentData, object);
				}
			};
			struct RemoveComponentCmd: CommandBufferCmd {
				Entity entity;
				Entity object;

				void commit(CommandBufferCtx& ctx) const {
					World::EntityBuilder(ctx.world, entity).del(object);
				}
			};

			friend class World;

			CommandBufferCtx m_ctx;
			uint32_t m_entities;

			//! Requests a new entity to be created from archetype
			//! \return Entity that will be created. The id is not usable right away. It
			//!         will be filled with proper data after commit(),
			GAIA_NODISCARD TempEntity add(Archetype& archetype) {
				m_ctx.save(CREATE_ENTITY_FROM_ARCHETYPE);

				CreateEntityFromArchetypeCmd cmd;
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

			//! Requests a new entity to be created
			//! \return Entity that will be created. The id is not usable right away. It
			//!         will be filled with proper data after commit().
			GAIA_NODISCARD TempEntity add() {
				m_ctx.save(CREATE_ENTITY);

				return {m_entities++};
			}

			//! Requests a new entity to be created by cloning an already existing entity
			//! \return Entity that will be created. The id is not usable right away. It
			//!         will be filled with proper data after commit()
			GAIA_NODISCARD TempEntity copy(Entity entityFrom) {
				m_ctx.save(CREATE_ENTITY_FROM_ENTITY);

				CreateEntityFromEntityCmd cmd;
				cmd.entity = entityFrom;
				ser::save(m_ctx, cmd);

				return {m_entities++};
			}

			//! Requests an existing \param entity to be removed.
			void del(Entity entity) {
				m_ctx.save(DELETE_ENTITY);

				DeleteEntityCmd cmd;
				cmd.entity = entity;
				ser::save(m_ctx, cmd);
			}

			//! Requests a component \tparam T to be added to \param entity.
			template <typename T>
			void add(Entity entity) {
				verify_comp<T>();

				// Make sure the component is registered
				const auto& desc = comp_cache_add<T>(m_ctx.world);

				m_ctx.save(ADD_COMPONENT);

				AddComponentCmd cmd;
				cmd.entity = entity;
				cmd.object = desc.entity;
				ser::save(m_ctx, cmd);
			}

			//! Requests a component \tparam T to be added to a temporary entity.
			template <typename T>
			void add(TempEntity entity) {
				verify_comp<T>();

				// Make sure the component is registered
				const auto& desc = comp_cache_add<T>(m_ctx.world);

				m_ctx.save(ADD_COMPONENT_TO_TEMPENTITY);

				AddComponentToTempEntityCmd cmd;
				cmd.tempEntity = entity;
				cmd.object = desc.entity;
				ser::save(m_ctx, cmd);
			}

			//! Requests a component \tparam T to be added to entity. Also sets its value.
			template <typename T>
			void add(Entity entity, T&& value) {
				verify_comp<T>();

				// Make sure the component is registered
				const auto& desc = comp_cache_add<T>(m_ctx.world);

				m_ctx.save(ADD_COMPONENT_DATA);

				AddComponentWithDataCmd cmd;
				cmd.entity = entity;
				cmd.object = desc.entity;
				ser::save(m_ctx, cmd);
				m_ctx.save_comp(m_ctx.world.comp_cache(), GAIA_FWD(value));
			}

			//! Requests a component \tparam T to be added to a temporary entity. Also sets its value.
			template <typename T>
			void add(TempEntity entity, T&& value) {
				verify_comp<T>();

				// Make sure the component is registered
				const auto& desc = comp_cache_add<T>(m_ctx.world);

				m_ctx.save(ADD_COMPONENT_TO_TEMPENTITY_DATA);

				AddComponentToTempEntityCmd cmd;
				cmd.tempEntity = entity;
				cmd.object = desc.entity;
				ser::save(m_ctx, cmd);
				m_ctx.save_comp(m_ctx.world.comp_cache(), GAIA_FWD(value));
			}

			//! Requests component data to be set to given values for a given entity.
			template <typename T>
			void set(Entity entity, T&& value) {
				verify_comp<T>();

				// Make sure the component is registered
				const auto& desc = comp_cache_add<T>(m_ctx.world);

				m_ctx.save(SET_COMPONENT);

				SetComponentCmd cmd;
				cmd.entity = entity;
				cmd.object = desc.entity;
				ser::save(m_ctx, cmd);
				m_ctx.save_comp(m_ctx.world.comp_cache(), GAIA_FWD(value));
			}

			//! Requests component data to be set to given values for a given temp entity.
			template <typename T>
			void set(TempEntity entity, T&& value) {
				verify_comp<T>();

				// Make sure the component is registered
				const auto& desc = comp_cache_add<T>(m_ctx.world);

				m_ctx.save(SET_COMPONENT_FOR_TEMPENTITY);

				SetComponentOnTempEntityCmd cmd;
				cmd.tempEntity = entity;
				cmd.object = desc.entity;
				ser::save(m_ctx, cmd);
				m_ctx.save_comp(m_ctx.world.comp_cache(), GAIA_FWD(value));
			}

			//! Requests removal of component \tparam T from \param entity
			template <typename T>
			void del(Entity entity) {
				verify_comp<T>();

				// Make sure the component is registered
				const auto& desc = comp_cache_add<T>(m_ctx.world);

				m_ctx.save(REMOVE_COMPONENT);

				RemoveComponentCmd cmd;
				cmd.entity = entity;
				cmd.object = desc.entity;
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
						CreateEntityFromArchetypeCmd cmd;
						ser::load(ctx, cmd);
						cmd.commit(ctx);
					},
					// CREATE_ENTITY_FROM_ENTITY
					[](CommandBufferCtx& ctx) {
						CreateEntityFromEntityCmd cmd;
						ser::load(ctx, cmd);
						cmd.commit(ctx);
					},
					// DELETE_ENTITY
					[](CommandBufferCtx& ctx) {
						DeleteEntityCmd cmd;
						ser::load(ctx, cmd);
						cmd.commit(ctx);
					},
					// ADD_COMPONENT
					[](CommandBufferCtx& ctx) {
						AddComponentCmd cmd;
						ser::load(ctx, cmd);
						cmd.commit(ctx);
					},
					// ADD_COMPONENT_DATA
					[](CommandBufferCtx& ctx) {
						AddComponentWithDataCmd cmd;
						ser::load(ctx, cmd);
						cmd.commit(ctx);
					},
					// ADD_COMPONENT_TO_TEMPENTITY
					[](CommandBufferCtx& ctx) {
						AddComponentToTempEntityCmd cmd;
						ser::load(ctx, cmd);
						cmd.commit(ctx);
					},
					// ADD_COMPONENT_TO_TEMPENTITY_DATA
					[](CommandBufferCtx& ctx) {
						AddComponentWithDataToTempEntityCmd cmd;
						ser::load(ctx, cmd);
						cmd.commit(ctx);
					},
					// SET_COMPONENT
					[](CommandBufferCtx& ctx) {
						SetComponentCmd cmd;
						ser::load(ctx, cmd);
						cmd.commit(ctx);
					},
					// SET_COMPONENT_FOR_TEMPENTITY
					[](CommandBufferCtx& ctx) {
						SetComponentOnTempEntityCmd cmd;
						ser::load(ctx, cmd);
						cmd.commit(ctx);
					},
					// REMOVE_COMPONENT
					[](CommandBufferCtx& ctx) {
						RemoveComponentCmd cmd;
						ser::load(ctx, cmd);
						cmd.commit(ctx);
					}};

		public:
			//! Commits all queued changes.
			void commit() {
				if (m_ctx.empty())
					return;

				// Extract data from the buffer
				m_ctx.seek(0);
				while (m_ctx.tell() < m_ctx.bytes()) {
					CommandBufferCmdType id{};
					m_ctx.load(id);
					CommandBufferRead[id](m_ctx);
				}

				m_entities = 0;
				m_ctx.reset();
			} // namespace ecs
		};
	} // namespace ecs
} // namespace gaia

/*** End of inlined file: command_buffer.h ***/


/*** Start of inlined file: system.h ***/
#pragma once
#include <cstdint>


/*** Start of inlined file: system_base.h ***/
#pragma once

#include <cstdint>
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
			//! Array of systems - used for iteration
			cnt::darray<BaseSystem*> m_systems;
			//! Array of new systems which need to be initialised
			cnt::darray<BaseSystem*> m_systemsToCreate;
			//! Array of systems which need to be deleted
			cnt::darray<BaseSystem*> m_systemsToDelete;

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
				for (auto* pSystem: m_systems) {
					pSystem->~BaseSystem();
					mem::AllocHelper::free("System", pSystem);
				}

				m_systems.clear();
				m_systemsMap.clear();

				m_systemsToCreate.clear();
				m_systemsToDelete.clear();
			}

			void cleanup() {
				for (auto& s: m_systems)
					s->OnCleanup();
			}

			void update() {
				// Remove all systems queued to be destroyed
				for (auto* pSystem: m_systemsToDelete)
					pSystem->enable(false);
				for (auto* pSystem: m_systemsToDelete)
					pSystem->OnCleanup();
				for (auto* pSystem: m_systemsToDelete)
					pSystem->OnDestroyed();
				for (auto* pSystem: m_systemsToDelete)
					m_systemsMap.erase({pSystem->m_hash});
				for (auto* pSystem: m_systemsToDelete) {
					m_systems.erase(core::find(m_systems, pSystem));
				}
				for (auto* pSystem: m_systemsToDelete) {
					pSystem->~BaseSystem();
					mem::AllocHelper::free("System", pSystem);
				}
				m_systemsToDelete.clear();

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

				auto* pSystem = mem::AllocHelper::alloc<T>("System");
				(void)new (pSystem) T();
				pSystem->m_world = &m_world;

#if GAIA_PROFILER_CPU
				if (name == nullptr) {
					constexpr auto ct_name = meta::type_info::name<T>();
					const size_t len = ct_name.size() >= MaxSystemNameLength ? MaxSystemNameLength : ct_name.size() + 1;
					GAIA_STRCPY(pSystem->m_name, len, ct_name.data());
				} else {
					GAIA_STRCPY(pSystem->m_name, MaxSystemNameLength, name);
				}
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
				m_systemsToDelete.push_back(pSystem);
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
				GAIA_FOR_(m_systems.size() - 1, l) {
					auto min = l;
					GAIA_FOR2_(l + 1, m_systems.size(), p) {
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
				GAIA_FOR2_(1U, m_systems.size(), j) {
					if (!m_systems[j - 1]->DependsOn(m_systems[j]))
						continue;
					GAIA_ASSERT2(false, "Wrong systems dependencies!");
					GAIA_LOG_E("Wrong systems dependencies!");
				}
#endif
			}
		};

	} // namespace ecs
} // namespace gaia

/*** End of inlined file: system_base.h ***/

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

			CommandBuffer& BeforeUpdateCmdBuffer() {
				return m_beforeUpdateCmdBuffer;
			}
			CommandBuffer& AfterUpdateCmdBuffer() {
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

/*** End of inlined file: system.h ***/

