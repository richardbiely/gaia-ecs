

#include <cinttypes>

//------------------------------------------------------------------------------
// DO NOT MODIFY THIS FILE
//------------------------------------------------------------------------------

#if __cplusplus >= 201703L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L)
#else
	// We use some C++17+ features such as folding expressions, compile-time ifs
	// and similar which makes it impossible to use Gaia-ECS with old compilers.
	#error "To build Gaia-ECS a compiler capable of at least C++17 is necesary"
#endif

#define GAIA_SAFE_CONSTEXPR constexpr

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
		return isLittleEndian == GAIA_LITTLE_ENDIAN;
	}
} // namespace gaia

//------------------------------------------------------------------------------

#if (GAIA_COMPILER_MSVC && _MSC_VER >= 1400) || GAIA_COMPILER_GCC || GAIA_COMPILER_CLANG
	#define GAIA_RESTRICT __restrict
#else
	#define GAIA_RESTRICT
#endif

#if __cplusplus >= 202002L
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
	#if GAIA_ARCH_ARM
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
			return _BitScanForward(&index, value) ? (uint32_t)index : (uint32_t)32;                                  \
		}(x))
	#pragma intrinsic(_BitScanForward64)
	//! Returns the number of leading zeros of \param x or 64 if \param x is 0.
	//! \warning Little-endian format.
	#define GAIA_CLZ64(x)                                                                                                \
		([](uint64_t value) noexcept {                                                                                     \
			unsigned long index;                                                                                             \
			return _BitScanForward64(&index, value) ? (uint32_t)index : (uint32_t)64;                                \
		}(x))

	#pragma intrinsic(_BitScanReverse)
	//! Returns the number of trailing zeros of \param x or 32 if \param x is 0.
	//! \warning Little-endian format.
	#define GAIA_CTZ(x)                                                                                                  \
		([](uint32_t value) noexcept {                                                                                     \
			unsigned long index;                                                                                             \
			return _BitScanReverse(&index, value) ? (uint32_t)index : (uint32_t)32;                                          \
		}(x))
	#pragma intrinsic(_BitScanReverse64)
	//! Returns the number of trailing zeros of \param x or 64 if \param x is 0.
	//! \warning Little-endian format.
	#define GAIA_CTZ64(x)                                                                                                \
		([](uint64_t value) noexcept {                                                                                     \
			unsigned long index;                                                                                             \
			return _BitScanReverse64(&index, value) ? (uint32_t)index : (uint32_t)64;                                        \
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
	#define GAIA_CLANG_WARNING_DISABLE(warningId) _Pragma(GAIA_STRINGIZE(clang diagnostic ignored #warningId))
	#define GAIA_CLANG_WARNING_ERROR(warningId) _Pragma(GAIA_STRINGIZE(clang diagnostic error #warningId))
	#define GAIA_CLANG_WARNING_ALLOW(warningId) _Pragma(GAIA_STRINGIZE(clang diagnostic warning #warningId))
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
	#define GAIA_GCC_WARNING_ERROR(warningId) _Pragma(GAIA_STRINGIZE(GCC diagnostic error warningId))
	#define GAIA_GCC_WARNING_DISABLE(warningId) DO_PRAGMA(GCC diagnostic ignored #warningId)
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

// The DoNotOptimize(...) function can be used to prevent a value or
// expression from being optimized away by the compiler. This function is
// intended to add little to no overhead.
// See: https://youtu.be/nXaxk27zwlk?t=2441
#if !GAIA_HAS_NO_INLINE_ASSEMBLY
template <class T>
inline void DoNotOptimize(T const& value) {
	asm volatile("" : : "r,m"(value) : "memory");
}

template <class T>
inline void DoNotOptimize(T& value) {
	#if defined(__clang__)
	asm volatile("" : "+r,m"(value) : : "memory");
	#else
	asm volatile("" : "+m,r"(value) : : "memory");
	#endif
}
#else
namespace internal {
	inline void UseCharPointer(char const volatile* var) {
		(void)var;
	}
} // namespace internal

	#if defined(_MSC_VER)
template <class T>
inline void DoNotOptimize(T const& value) {
	internal::UseCharPointer(&reinterpret_cast<char const volatile&>(value));
	::_ReadWriteBarrier();
}
	#else
template <class T>
inline void DoNotOptimize(T const& value) {
	internal::UseCharPointer(&reinterpret_cast<char const volatile&>(value));
}
	#endif
#endif

// Breaking changes and big features
#define GAIA_VERSION_MAJOR 0
// Smaller changes and features
#define GAIA_VERSION_MINOR 7
// Fixes and tweaks
#define GAIA_VERSION_PATCH 0

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

//! If enabled, STL containers are going to be used by the framework.
#if !defined(GAIA_USE_STL_CONTAINERS)
	#define GAIA_USE_STL_CONTAINERS 0
#endif
//! If enabled, internal containers stay compatible with STL by sticking to STL iterators.
#if !defined(GAIA_USE_STL_COMPATIBLE_CONTAINERS)
	#define GAIA_USE_STL_COMPATIBLE_CONTAINERS (GAIA_USE_STL_CONTAINERS || 0)
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

//! When searching for empty chunk we have two options:
//! 1) pick any non-empty one first
//!    - fragments memory, progresivelly slower peformance over time possible
//!    - fast removals because entity swaps happen within the same chunk
//! 2) always take from the last chunk
//!    - avoids memory fragmentation altogether
//!    - makes entity removal and component movement slower because we always swap with the last chunk
//!
//! We stick with (1) as a default because in general it is better to take a small performance hit
//! for defragmeneting every once in a while than having consistently slower performance all the time.
#ifndef GAIA_AVOID_CHUNK_FRAGMENTATION
	#define GAIA_AVOID_CHUNK_FRAGMENTATION 0
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
// General settings
//------------------------------------------------------------------------------

#if GAIA_USE_STL_CONTAINERS && !GAIA_USE_STL_COMPATIBLE_CONTAINERS
	#undef GAIA_USE_STL_COMPATIBLE_CONTAINERS
	#define GAIA_USE_STL_COMPATIBLE_CONTAINERS 1
#endif
#if GAIA_USE_STL_CONTAINERS || GAIA_USE_STL_COMPATIBLE_CONTAINERS
	#define GAIA_UTIL std
#else
	#define GAIA_UTIL gaia::utils
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
			(GAIA_LIKELY(cond) ? void(0) : [] {                                                                              \
				assert(!#cond);                                                                                                \
			}())
	#else
		// For non-Debug builds simulate asserts
		#if GAIA_DEBUG
			#define GAIA_ASSERT_ENABLED 1
			#define GAIA_ASSERT(cond)                                                                                        \
				(GAIA_LIKELY(cond) ? void(0) : [] {                                                                            \
					GAIA_LOG_E("%s:%d: Assertion failed: '%s'.", __FILE__, __LINE__, #cond);                                     \
				}())
		#else
			#define GAIA_ASSERT_ENABLED 0
			#define GAIA_ASSERT(cond) (void(0))
		#endif
	#endif
#endif

#if !defined(GAIA_ECS_CHUNK_ALLOCATOR_CLEAN_MEMORY_WITH_GARBAGE)
	#define GAIA_ECS_CHUNK_ALLOCATOR_CLEAN_MEMORY_WITH_GARBAGE (GAIA_DEBUG)
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

#include <tuple>
#include <type_traits>
#include <utility>

#define USE_SPAN GAIA_USE_STL_CONTAINERS

#if USE_SPAN && __cpp_lib_span
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

#ifndef TCB_SPAN_HPP_INCLUDED
#define TCB_SPAN_HPP_INCLUDED

#include <cstddef>
#include <cstdint>
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

#define USE_ARRAY GAIA_USE_STL_CONTAINERS

#if USE_ARRAY == 1
	#include <array>
namespace gaia {
	namespace containers {
		template <typename T, size_t N>
		using sarray = std::array<T, N>;
	} // namespace containers
} // namespace gaia
#elif USE_VECTOR == 0
	
#include <cstddef>
#include <type_traits>
#include <utility>

#if GAIA_USE_STL_COMPATIBLE_CONTAINERS
	#include <iterator>
#else
	#include <cstddef>
	#include <type_traits>

namespace gaia {
	namespace utils {
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
				using difference_type = ptrdiff_t;
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
			constexpr bool is_iterator_v = false;

			template <typename T>
			constexpr bool is_iterator_v<T, std::void_t<iterator_cat_t<T>>> = true;

			template <typename T>
			struct is_iterator: std::bool_constant<is_iterator_v<T>> {};

			template <typename It>
			constexpr bool is_input_iter_v = std::is_convertible_v<iterator_cat_t<It>, input_iterator_tag>;

			template <typename It>
			constexpr bool is_fwd_iter_v = std::is_convertible_v<iterator_cat_t<It>, forward_iterator_tag>;

			template <typename It>
			constexpr bool is_rev_iter_v = std::is_convertible_v<iterator_cat_t<It>, reverse_iterator_tag>;

			template <typename It>
			constexpr bool is_bidi_iter_v = std::is_convertible_v<iterator_cat_t<It>, bidirectional_iterator_tag>;

			template <typename It>
			constexpr bool is_random_iter_v = std::is_convertible_v<iterator_cat_t<It>, random_access_iterator_tag>;
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
	} // namespace utils
} // namespace gaia
#endif

namespace gaia {
	namespace containers {
		//! Array of elements of type \tparam T with fixed size and capacity \tparam N allocated on stack.
		//! Interface compatiblity with std::array where it matters.
		template <typename T, size_t N>
		class sarr {
		public:
			using value_type = T;
			using reference = T&;
			using const_reference = const T&;
			using pointer = T*;
			using const_pointer = T*;
			using difference_type = std::ptrdiff_t;
			using size_type = decltype(N);

			static constexpr size_type extent = N;

			T m_data[N != 0U ? N : 1]; // support zero-size arrays

			class iterator {
			public:
				using iterator_category = GAIA_UTIL::random_access_iterator_tag;
				using value_type = T;
				using difference_type = std::ptrdiff_t;
				using pointer = T*;
				using reference = T&;

				using size_type = decltype(N);

			private:
				pointer m_ptr;

			public:
				constexpr iterator(pointer ptr): m_ptr(ptr) {}

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
					return m_ptr - other.m_ptr;
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

			class const_iterator {
			public:
				using iterator_category = GAIA_UTIL::random_access_iterator_tag;
				using value_type = const T;
				using difference_type = std::ptrdiff_t;
				using pointer = const T*;
				using reference = const T&;

				using size_type = decltype(N);

			private:
				pointer m_ptr;

			public:
				constexpr const_iterator(pointer ptr): m_ptr(ptr) {}

				constexpr reference operator*() const {
					return *m_ptr;
				}
				constexpr pointer operator->() const {
					return m_ptr;
				}
				constexpr const_iterator operator[](size_type offset) const {
					return {m_ptr + offset};
				}

				constexpr const_iterator& operator+=(size_type diff) {
					m_ptr += diff;
					return *this;
				}
				constexpr const_iterator& operator-=(size_type diff) {
					m_ptr -= diff;
					return *this;
				}
				constexpr const_iterator& operator++() {
					++m_ptr;
					return *this;
				}
				constexpr const_iterator operator++(int) {
					const_iterator temp(*this);
					++*this;
					return temp;
				}
				constexpr const_iterator& operator--() {
					--m_ptr;
					return *this;
				}
				constexpr const_iterator operator--(int) {
					const_iterator temp(*this);
					--*this;
					return temp;
				}

				constexpr const_iterator operator+(size_type offset) const {
					return {m_ptr + offset};
				}
				constexpr const_iterator operator-(size_type offset) const {
					return {m_ptr - offset};
				}
				constexpr difference_type operator-(const const_iterator& other) const {
					return m_ptr - other.m_ptr;
				}

				constexpr bool operator==(const const_iterator& other) const {
					return m_ptr == other.m_ptr;
				}
				constexpr bool operator!=(const const_iterator& other) const {
					return m_ptr != other.m_ptr;
				}
				constexpr bool operator>(const const_iterator& other) const {
					return m_ptr > other.m_ptr;
				}
				constexpr bool operator>=(const const_iterator& other) const {
					return m_ptr >= other.m_ptr;
				}
				constexpr bool operator<(const const_iterator& other) const {
					return m_ptr < other.m_ptr;
				}
				constexpr bool operator<=(const const_iterator& other) const {
					return m_ptr <= other.m_ptr;
				}
			};

			GAIA_NODISCARD constexpr pointer data() noexcept {
				return (pointer)m_data;
			}

			GAIA_NODISCARD constexpr const_pointer data() const noexcept {
				return (const_pointer)m_data;
			}

			GAIA_NODISCARD constexpr reference operator[](size_type pos) noexcept {
				GAIA_ASSERT(pos < size());
				return (reference)m_data[pos];
			}

			GAIA_NODISCARD constexpr const_reference operator[](size_type pos) const noexcept {
				GAIA_ASSERT(pos < size());
				return (const_reference)m_data[pos];
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

			GAIA_NODISCARD constexpr reference front() noexcept {
				return *begin();
			}

			GAIA_NODISCARD constexpr const_reference front() const noexcept {
				return *begin();
			}

			GAIA_NODISCARD constexpr reference back() noexcept {
				return N != 0U ? *(end() - 1) : *end();
			}

			GAIA_NODISCARD constexpr const_reference back() const noexcept {
				return N != 0U ? *(end() - 1) : *end();
			}

			GAIA_NODISCARD constexpr iterator begin() const noexcept {
				return {(T*)m_data};
			}

			GAIA_NODISCARD constexpr const_iterator cbegin() const noexcept {
				return {(const T*)m_data};
			}

			GAIA_NODISCARD constexpr iterator end() const noexcept {
				return {(T*)m_data + N};
			}

			GAIA_NODISCARD constexpr const_iterator cend() const noexcept {
				return {(const T*)m_data + N};
			}

			GAIA_NODISCARD bool operator==(const sarr& other) const {
				for (size_type i = 0; i < N; ++i)
					if (!(m_data[i] == other.m_data[i]))
						return false;
				return true;
			}
		};

		namespace detail {
			template <typename T, std::size_t N, std::size_t... I>
			constexpr sarr<std::remove_cv_t<T>, N> to_array_impl(T (&a)[N], std::index_sequence<I...> /*no_name*/) {
				return {{a[I]...}};
			}
		} // namespace detail

		template <typename T, std::size_t N>
		constexpr sarr<std::remove_cv_t<T>, N> to_array(T (&a)[N]) {
			return detail::to_array_impl(a, std::make_index_sequence<N>{});
		}

		template <typename T, typename... U>
		sarr(T, U...) -> sarr<T, 1 + sizeof...(U)>;

	} // namespace containers

} // namespace gaia

namespace std {
	template <typename T, size_t N>
	struct tuple_size<gaia::containers::sarr<T, N>>: std::integral_constant<std::size_t, N> {};

	template <size_t I, typename T, size_t N>
	struct tuple_element<I, gaia::containers::sarr<T, N>> {
		using type = T;
	};
} // namespace std

namespace gaia {
	namespace containers {
		template <typename T, size_t N>
		using sarray = containers::sarr<T, N>;
	} // namespace containers
} // namespace gaia
#else

	// You can add your custom container here
	#error Unsupported value used for USE_ARRAY

#endif

// Various feature test macros

#ifndef TCB_SPAN_NAMESPACE_NAME
	#define TCB_SPAN_NAMESPACE_NAME tcb
#endif

#if __cplusplus >= 201703L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L)
	#define TCB_SPAN_HAVE_CPP17
#endif

#if __cplusplus >= 201402L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201402L)
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

	template <typename ElementType, std::size_t Extent = dynamic_extent>
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
		struct is_std_array<gaia::containers::sarray<T, N>>: std::true_type {};

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
		struct is_container_element_type_compatible: std::false_type {};

		template <typename T, typename E>
		struct is_container_element_type_compatible<
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

	template <typename ElementType, std::size_t Extent>
	class span {
		static_assert(
				std::is_object<ElementType>::value, "A span's ElementType must be an object type (not a "
																						"reference type or void)");
		static_assert(
				detail::is_complete<ElementType>::value, "A span's ElementType must be a complete type (not a forward "
																								 "declaration)");
		static_assert(!std::is_abstract<ElementType>::value, "A span's ElementType cannot be an abstract class type");

		using storage_type = detail::span_storage<ElementType, Extent>;

	public:
		// constants and types
		using element_type = ElementType;
		using value_type = typename std::remove_cv<ElementType>::type;
		using size_type = std::size_t;
		using difference_type = std::ptrdiff_t;
		using pointer = element_type*;
		using const_pointer = const element_type*;
		using reference = element_type&;
		using const_reference = const element_type&;
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
								detail::is_container_element_type_compatible<element_type (&)[N], ElementType>::value,
						int>::type = 0>
		constexpr span(element_type (&arr)[N]) noexcept: storage_(arr, N) {}

		template <
				typename T, std::size_t N, std::size_t E = Extent,
				typename std::enable_if<
						(E == dynamic_extent || N == E) &&
								detail::is_container_element_type_compatible<gaia::containers::sarray<T, N>&, ElementType>::value,
						int>::type = 0>
		TCB_SPAN_ARRAY_CONSTEXPR span(gaia::containers::sarray<T, N>& arr) noexcept: storage_(arr.data(), N) {}

		template <
				typename T, std::size_t N, std::size_t E = Extent,
				typename std::enable_if<
						(E == dynamic_extent || N == E) &&
								detail::is_container_element_type_compatible<const gaia::containers::sarray<T, N>&, ElementType>::value,
						int>::type = 0>
		TCB_SPAN_ARRAY_CONSTEXPR span(const gaia::containers::sarray<T, N>& arr) noexcept: storage_(arr.data(), N) {}

		template <
				typename Container, std::size_t E = Extent,
				typename std::enable_if<
						E == dynamic_extent && detail::is_container<Container>::value &&
								detail::is_container_element_type_compatible<Container&, ElementType>::value,
						int>::type = 0>
		constexpr span(Container& cont): storage_(detail::data(cont), detail::size(cont)) {}

		template <
				typename Container, std::size_t E = Extent,
				typename std::enable_if<
						E == dynamic_extent && detail::is_container<Container>::value &&
								detail::is_container_element_type_compatible<const Container&, ElementType>::value,
						int>::type = 0>
		constexpr span(const Container& cont): storage_(detail::data(cont), detail::size(cont)) {}

		constexpr span(const span& other) noexcept = default;

		template <
				typename OtherElementType, std::size_t OtherExtent,
				typename std::enable_if<
						(Extent == dynamic_extent || OtherExtent == dynamic_extent || Extent == OtherExtent) &&
								std::is_convertible<OtherElementType (*)[], ElementType (*)[]>::value,
						int>::type = 0>
		constexpr span(const span<OtherElementType, OtherExtent>& other) noexcept: storage_(other.data(), other.size()) {}

		~span() noexcept = default;

		TCB_SPAN_CONSTEXPR_ASSIGN span& operator=(const span& other) noexcept = default;

		// [span.sub], span subviews
		template <std::size_t Count>
		TCB_SPAN_CONSTEXPR11 span<element_type, Count> first() const {
			TCB_SPAN_EXPECT(Count <= size());
			return {data(), Count};
		}

		template <std::size_t Count>
		TCB_SPAN_CONSTEXPR11 span<element_type, Count> last() const {
			TCB_SPAN_EXPECT(Count <= size());
			return {data() + (size() - Count), Count};
		}

		template <std::size_t Offset, std::size_t Count = dynamic_extent>
		using subspan_return_t = span<
				ElementType, Count != dynamic_extent ? Count : (Extent != dynamic_extent ? Extent - Offset : dynamic_extent)>;

		template <std::size_t Offset, std::size_t Count = dynamic_extent>
		TCB_SPAN_CONSTEXPR11 subspan_return_t<Offset, Count> subspan() const {
			TCB_SPAN_EXPECT(Offset <= size() && (Count == dynamic_extent || Offset + Count <= size()));
			return {data() + Offset, Count != dynamic_extent ? Count : size() - Offset};
		}

		TCB_SPAN_CONSTEXPR11 span<element_type, dynamic_extent> first(size_type count) const {
			TCB_SPAN_EXPECT(count <= size());
			return {data(), count};
		}

		TCB_SPAN_CONSTEXPR11 span<element_type, dynamic_extent> last(size_type count) const {
			TCB_SPAN_EXPECT(count <= size());
			return {data() + (size() - count), count};
		}

		TCB_SPAN_CONSTEXPR11 span<element_type, dynamic_extent>
		subspan(size_type offset, size_type count = dynamic_extent) const {
			TCB_SPAN_EXPECT(offset <= size() && (count == dynamic_extent || offset + count <= size()));
			return {data() + offset, count == dynamic_extent ? size() - offset : count};
		}

		// [span.obs], span observers
		constexpr size_type size() const noexcept {
			return storage_.size;
		}

		constexpr size_type size_bytes() const noexcept {
			return size() * sizeof(element_type);
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
	span(gaia::containers::sarray<T, N>&) -> span<T, N>;

	template <typename T, size_t N>
	span(const gaia::containers::sarray<T, N>&) -> span<const T, N>;

	template <typename Container>
	span(Container&) -> span<typename std::remove_reference<decltype(*detail::data(std::declval<Container&>()))>::type>;

	template <typename Container>
	span(const Container&) -> span<const typename Container::value_type>;

#endif // TCB_HAVE_DEDUCTION_GUIDES

	template <typename ElementType, std::size_t Extent>
	constexpr span<ElementType, Extent> make_span(span<ElementType, Extent> s) noexcept {
		return s;
	}

	template <typename T, std::size_t N>
	constexpr span<T, N> make_span(T (&arr)[N]) noexcept {
		return {arr};
	}

	template <typename T, std::size_t N>
	TCB_SPAN_ARRAY_CONSTEXPR span<T, N> make_span(gaia::containers::sarray<T, N>& arr) noexcept {
		return {arr};
	}

	template <typename T, std::size_t N>
	TCB_SPAN_ARRAY_CONSTEXPR span<const T, N> make_span(const gaia::containers::sarray<T, N>& arr) noexcept {
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

	template <typename ElementType, std::size_t Extent>
	span<const byte, ((Extent == dynamic_extent) ? dynamic_extent : sizeof(ElementType) * Extent)>
	as_bytes(span<ElementType, Extent> s) noexcept {
		return {reinterpret_cast<const byte*>(s.data()), s.size_bytes()};
	}

	template <
			class ElementType, size_t Extent, typename std::enable_if<!std::is_const<ElementType>::value, int>::type = 0>
	span<byte, ((Extent == dynamic_extent) ? dynamic_extent : sizeof(ElementType) * Extent)>
	as_writable_bytes(span<ElementType, Extent> s) noexcept {
		return {reinterpret_cast<byte*>(s.data()), s.size_bytes()};
	}

	template <std::size_t N, typename E, std::size_t S>
	constexpr auto get(span<E, S> s) -> decltype(s[N]) {
		return s[N];
	}

} // namespace TCB_SPAN_NAMESPACE_NAME

namespace std {

	template <typename ElementType, size_t Extent>
	struct tuple_size<TCB_SPAN_NAMESPACE_NAME::span<ElementType, Extent>>: public integral_constant<size_t, Extent> {};

	template <typename ElementType>
	struct tuple_size<TCB_SPAN_NAMESPACE_NAME::span<ElementType, TCB_SPAN_NAMESPACE_NAME::dynamic_extent>>; // not defined

	template <size_t I, typename ElementType, size_t Extent>
	struct tuple_element<I, TCB_SPAN_NAMESPACE_NAME::span<ElementType, Extent>> {
		static_assert(Extent != TCB_SPAN_NAMESPACE_NAME::dynamic_extent && I < Extent, "");
		using type = ElementType;
	};

} // end namespace std

#endif // TCB_SPAN_HPP_INCLUDED

namespace std {
	using tcb::span;
}
#endif

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
	namespace utils {
		inline void* mem_alloc(size_t size) {
			void* ptr = GAIA_MEM_ALLC(size);
			GAIA_PROF_ALLOC(ptr, size);
			return ptr;
		}

		inline void* mem_alloc_alig(size_t size, size_t alig) {
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

		//! Move or copy \param size elements of type \tparam T from the address pointer to by \param src to \param dst
		template <typename T>
		void move_elements(T* GAIA_RESTRICT dst, const T* GAIA_RESTRICT src, size_t size) {
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

#include <tuple>
#include <type_traits>
#include <utility>

namespace gaia {
	namespace utils {

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
		static constexpr uint32_t StructToTupleMaxTypesBits = 4;
		// static constexpr uint32_t StructToTupleMaxTypes = 1 << StructToTupleMaxTypesBits;

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
		auto for_each_member(T&& object, Func&& visitor) {
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

	} // namespace utils
} // namespace gaia

namespace gaia {
	namespace utils {
		enum class DataLayout {
			AoS, //< Array Of Structures
			SoA, //< Structure Of Arrays, 4 packed items, good for SSE and similar
			SoA8, //< Structure Of Arrays, 8 packed items, good for AVX and similar
			SoA16 //< Structure Of Arrays, 16 packed items, good for AVX512 and similar
		};

		// Helper templates
		namespace detail {

			//----------------------------------------------------------------------
			// Byte offset of a member of SoA-organized data
			//----------------------------------------------------------------------

			template <size_t N, size_t Alignment, typename Tuple>
			constexpr static size_t soa_byte_offset(const uintptr_t address, [[maybe_unused]] const size_t size) {
				const auto addressAligned = utils::align<Alignment>(address) - address;
				if constexpr (N == 0) {
					return addressAligned;
				} else {
					using tt = typename std::tuple_element<N - 1, Tuple>::type;
					return addressAligned + sizeof(tt) * size + soa_byte_offset<N - 1, Alignment, Tuple>(address, size);
				}
			}

		} // namespace detail

		template <DataLayout TDataLayout, typename TItem>
		struct data_layout_properties;
		template <typename TItem>
		struct data_layout_properties<DataLayout::AoS, TItem> {
			constexpr static DataLayout Layout = DataLayout::AoS;
			constexpr static size_t PackSize = 1;
			constexpr static size_t Alignment = alignof(TItem);
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
			constexpr static DataLayout Layout = data_layout_properties<DataLayout::AoS, ValueType>::Layout;
			constexpr static size_t Alignment = data_layout_properties<DataLayout::AoS, ValueType>::Alignment;

			GAIA_NODISCARD constexpr static ValueType getc(std::span<const ValueType> s, size_t idx) {
				return s[idx];
			}

			GAIA_NODISCARD constexpr static ValueType get(std::span<ValueType> s, size_t idx) {
				return s[idx];
			}

			GAIA_NODISCARD constexpr static const ValueType& getc_constref(std::span<const ValueType> s, size_t idx) {
				return (const ValueType&)s[idx];
			}

			GAIA_NODISCARD constexpr static const ValueType& get_constref(std::span<ValueType> s, size_t idx) {
				return (const ValueType&)s[idx];
			}

			GAIA_NODISCARD constexpr static ValueType& get_ref(std::span<ValueType> s, size_t idx) {
				return s[idx];
			}

			constexpr static void set(std::span<ValueType> s, size_t idx, ValueType&& val) {
				s[idx] = std::forward<ValueType>(val);
			}
		};

		template <typename ValueType>
		struct data_view_policy<DataLayout::AoS, ValueType>: data_view_policy_aos<ValueType> {};

		template <typename ValueType>
		struct data_view_policy_aos_get {
			using view_policy = data_view_policy_aos<ValueType>;

			//! Raw data pointed to by the view policy
			std::span<const ValueType> m_data;

			GAIA_NODISCARD const ValueType& operator[](size_t idx) const {
				return view_policy::getc_constref(m_data, idx);
			}

			GAIA_NODISCARD auto data() const {
				return m_data.data();
			}

			GAIA_NODISCARD auto size() const {
				return m_data.size();
			}
		};

		template <typename ValueType>
		struct data_view_policy_get<DataLayout::AoS, ValueType>: data_view_policy_aos_get<ValueType> {};

		template <typename ValueType>
		struct data_view_policy_aos_set {
			using view_policy = data_view_policy_aos<ValueType>;

			//! Raw data pointed to by the view policy
			std::span<ValueType> m_data;

			GAIA_NODISCARD ValueType& operator[](size_t idx) {
				return view_policy::get_ref(m_data, idx);
			}

			GAIA_NODISCARD const ValueType& operator[](size_t idx) const {
				return view_policy::getc_constref(m_data, idx);
			}

			GAIA_NODISCARD auto data() const {
				return m_data.data();
			}

			GAIA_NODISCARD auto size() const {
				return m_data.size();
			}
		};

		template <typename ValueType>
		struct data_view_policy_set<DataLayout::AoS, ValueType>: data_view_policy_aos_set<ValueType> {};

		template <typename ValueType>
		using aos_view_policy = data_view_policy_aos<ValueType>;
		template <typename ValueType>
		using aos_view_policy_get = data_view_policy_aos_get<ValueType>;
		template <typename ValueType>
		using aos_view_policy_set = data_view_policy_aos_set<ValueType>;

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
			constexpr static DataLayout Layout = data_layout_properties<TDataLayout, ValueType>::Layout;
			constexpr static size_t Alignment = data_layout_properties<TDataLayout, ValueType>::Alignment;

			template <size_t Ids>
			using value_type = typename std::tuple_element<Ids, decltype(struct_to_tuple(ValueType{}))>::type;
			template <size_t Ids>
			using const_value_type = typename std::add_const<value_type<Ids>>::type;

			GAIA_NODISCARD constexpr static ValueType get(std::span<const ValueType> s, const size_t idx) {
				auto t = struct_to_tuple(ValueType{});
				return get_internal(t, s, idx, std::make_integer_sequence<size_t, std::tuple_size<decltype(t)>::value>());
			}

			template <size_t Ids>
			GAIA_NODISCARD constexpr static auto get(std::span<const ValueType> s, const size_t idx = 0) {
				using Tuple = decltype(struct_to_tuple(ValueType{}));
				using MemberType = typename std::tuple_element<Ids, Tuple>::type;
				const auto* ret = (const uint8_t*)s.data() + idx * sizeof(MemberType) +
													detail::soa_byte_offset<Ids, Alignment, Tuple>((uintptr_t)s.data(), s.size());
				return std::span{(const MemberType*)ret, s.size() - idx};
			}

			constexpr static void set(std::span<ValueType> s, const size_t idx, ValueType&& val) {
				auto t = struct_to_tuple(std::forward<ValueType>(val));
				set_internal(t, s, idx, std::make_integer_sequence<size_t, std::tuple_size<decltype(t)>::value>());
			}

			template <size_t Ids>
			constexpr static auto set(std::span<ValueType> s, const size_t idx = 0) {
				using Tuple = decltype(struct_to_tuple(ValueType{}));
				using MemberType = typename std::tuple_element<Ids, Tuple>::type;
				auto* ret = (uint8_t*)s.data() + idx * sizeof(MemberType) +
										detail::soa_byte_offset<Ids, Alignment, Tuple>((uintptr_t)s.data(), s.size());
				return std::span{(MemberType*)ret, s.size() - idx};
			}

		private:
			template <typename Tuple, size_t... Ids>
			GAIA_NODISCARD constexpr static ValueType get_internal(
					Tuple& t, std::span<const ValueType> s, const size_t idx, std::integer_sequence<size_t, Ids...> /*no_name*/) {
				(get_internal<Tuple, Ids, typename std::tuple_element<Ids, Tuple>::type>(
						 t, (const uint8_t*)s.data(),
						 idx * sizeof(typename std::tuple_element<Ids, Tuple>::type) +
								 detail::soa_byte_offset<Ids, Alignment, Tuple>((uintptr_t)s.data(), s.size())),
				 ...);
				return tuple_to_struct<ValueType, Tuple>(std::forward<Tuple>(t));
			}

			template <typename Tuple, size_t Ids, typename TMemberType>
			constexpr static void get_internal(Tuple& t, const uint8_t* data, const size_t idx) {
				unaligned_ref<TMemberType> reader((void*)&data[idx]);
				std::get<Ids>(t) = reader;
			}

			template <typename Tuple, typename TValue, size_t... Ids>
			constexpr static void
			set_internal(Tuple& t, std::span<TValue> s, const size_t idx, std::integer_sequence<size_t, Ids...> /*no_name*/) {
				(set_internal(
						 (uint8_t*)s.data(),
						 idx * sizeof(typename std::tuple_element<Ids, Tuple>::type) +
								 detail::soa_byte_offset<Ids, Alignment, Tuple>((uintptr_t)s.data(), s.size()),
						 std::get<Ids>(t)),
				 ...);
			}

			template <typename MemberType>
			constexpr static void set_internal(uint8_t* data, const size_t idx, MemberType val) {
				unaligned_ref<MemberType> writer((void*)&data[idx]);
				writer = val;
			}
		};

		template <typename ValueType>
		struct data_view_policy<DataLayout::SoA, ValueType>: data_view_policy_soa<DataLayout::SoA, ValueType> {};
		template <typename ValueType>
		struct data_view_policy<DataLayout::SoA8, ValueType>: data_view_policy_soa<DataLayout::SoA8, ValueType> {};
		template <typename ValueType>
		struct data_view_policy<DataLayout::SoA16, ValueType>: data_view_policy_soa<DataLayout::SoA16, ValueType> {};

		template <typename ValueType>
		using soa_view_policy = data_view_policy<DataLayout::SoA, ValueType>;
		template <typename ValueType>
		using soa8_view_policy = data_view_policy<DataLayout::SoA8, ValueType>;
		template <typename ValueType>
		using soa16_view_policy = data_view_policy<DataLayout::SoA16, ValueType>;

		template <DataLayout TDataLayout, typename ValueType>
		struct data_view_policy_soa_get {
			using view_policy = data_view_policy_soa<TDataLayout, ValueType>;

			template <size_t Ids>
			struct data_view_policy_idx_info {
				using const_value_type = typename view_policy::template const_value_type<Ids>;
			};

			//! Raw data pointed to by the view policy
			std::span<const ValueType> m_data;

			GAIA_NODISCARD constexpr auto operator[](size_t idx) const {
				return view_policy::get(m_data, idx);
			}

			template <size_t Ids>
			GAIA_NODISCARD constexpr auto get() const {
				return std::span<typename data_view_policy_idx_info<Ids>::const_value_type>(
						view_policy::template get<Ids>(m_data).data(), view_policy::template get<Ids>(m_data).size());
			}

			GAIA_NODISCARD auto data() const {
				return m_data.data();
			}

			GAIA_NODISCARD auto size() const {
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

		template <typename ValueType>
		using soa_view_policy_get = data_view_policy_get<DataLayout::SoA, ValueType>;
		template <typename ValueType>
		using soa8_view_policy_get = data_view_policy_get<DataLayout::SoA8, ValueType>;
		template <typename ValueType>
		using soa16_view_policy_get = data_view_policy_get<DataLayout::SoA16, ValueType>;

		template <DataLayout TDataLayout, typename ValueType>
		struct data_view_policy_soa_set {
			using view_policy = data_view_policy_soa<TDataLayout, ValueType>;

			template <size_t Ids>
			struct data_view_policy_idx_info {
				using value_type = typename view_policy::template value_type<Ids>;
				using const_value_type = typename view_policy::template const_value_type<Ids>;
			};

			//! Raw data pointed to by the view policy
			std::span<ValueType> m_data;

			struct setter {
				const std::span<ValueType>& m_data;
				const size_t m_idx;

				constexpr setter(const std::span<ValueType>& data, const size_t idx): m_data(data), m_idx(idx) {}
				constexpr void operator=(ValueType&& val) {
					view_policy::set(m_data, m_idx, std::forward<ValueType>(val));
				}
			};

			GAIA_NODISCARD constexpr auto operator[](size_t idx) const {
				return view_policy::get(m_data, idx);
			}
			GAIA_NODISCARD constexpr auto operator[](size_t idx) {
				return setter(m_data, idx);
			}

			template <size_t Ids>
			GAIA_NODISCARD constexpr auto get() const {
				using value_type = typename data_view_policy_idx_info<Ids>::const_value_type;
				const std::span<const ValueType> data((const ValueType*)m_data.data(), m_data.size());
				return std::span<value_type>(
						view_policy::template get<Ids>(data).data(), view_policy::template get<Ids>(data).size());
			}

			template <size_t Ids>
			GAIA_NODISCARD constexpr auto set() {
				return std::span<typename data_view_policy_idx_info<Ids>::value_type>(
						view_policy::template set<Ids>(m_data).data(), view_policy::template set<Ids>(m_data).size());
			}

			GAIA_NODISCARD auto data() const {
				return m_data.data();
			}

			GAIA_NODISCARD auto size() const {
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

		template <typename ValueType>
		using soa_view_policy_set = data_view_policy_set<DataLayout::SoA, ValueType>;
		template <typename ValueType>
		using soa8_view_policy_set = data_view_policy_set<DataLayout::SoA8, ValueType>;
		template <typename ValueType>
		using soa16_view_policy_set = data_view_policy_set<DataLayout::SoA16, ValueType>;

		//----------------------------------------------------------------------
		// Helpers
		//----------------------------------------------------------------------

		namespace detail {
			template <typename T, typename = void>
			struct auto_view_policy_internal {
				static constexpr DataLayout data_layout_type = DataLayout::AoS;
			};
			template <typename T>
			struct auto_view_policy_internal<T, std::void_t<decltype(T::Layout)>> {
				static constexpr DataLayout data_layout_type = T::Layout;
			};

			template <typename, typename = void>
			struct is_soa_layout: std::false_type {};
			template <typename T>
			struct is_soa_layout<T, typename std::enable_if_t<T::Layout != DataLayout::AoS>>: std::true_type {};
		} // namespace detail

		template <typename T>
		using auto_view_policy = data_view_policy<detail::auto_view_policy_internal<T>::data_layout_type, T>;
		template <typename T>
		using auto_view_policy_get = data_view_policy_get<detail::auto_view_policy_internal<T>::data_layout_type, T>;
		template <typename T>
		using auto_view_policy_set = data_view_policy_set<detail::auto_view_policy_internal<T>::data_layout_type, T>;

		template <typename T>
		inline constexpr bool is_soa_layout_v = detail::is_soa_layout<T>::value;

	} // namespace utils
} // namespace gaia

#include <cstdint>
#include <type_traits>
#if GAIA_USE_STL_CONTAINERS
	#include <functional>
#endif

namespace gaia {
	namespace utils {

		namespace detail {
			template <typename, typename = void>
			struct is_direct_hash_key: std::false_type {};
			template <typename T>
			struct is_direct_hash_key<T, typename std::enable_if_t<T::IsDirectHashKey>>: std::true_type {};

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

			size_t i = 0;
			while (str[i] != '\0') {
				hash = (hash ^ uint64_t(str[i])) * detail::fnv1a::prime_64_const;
				++i;
			}

			return hash;
		}

		constexpr uint64_t calculate_hash64(const char* const str, const size_t length) noexcept {
			uint64_t hash = detail::fnv1a::val_64_const;

			for (size_t i = 0; i < length; i++)
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

				constexpr uint64_t StaticHashValueRest64(uint64_t h, size_t len, const char* data) {
					return ((len & 7) == 7)		? StaticHashValue64Tail7(h, data)
								 : ((len & 7) == 6) ? StaticHashValue64Tail6(h, data)
								 : ((len & 7) == 5) ? StaticHashValue64Tail5(h, data)
								 : ((len & 7) == 4) ? StaticHashValue64Tail4(h, data)
								 : ((len & 7) == 3) ? StaticHashValue64Tail3(h, data)
								 : ((len & 7) == 2) ? StaticHashValue64Tail2(h, data)
								 : ((len & 7) == 1) ? StaticHashValue64Tail1(h, data)
																		: StaticHashValueLast64_(h);
				}

				constexpr uint64_t StaticHashValueLoop64(size_t i, uint64_t h, size_t len, const char* data) {
					return (
							i == 0 ? StaticHashValueRest64(h, len, data)
										 : StaticHashValueLoop64(
													 i - 1, (h ^ (((Load8(data) * m) ^ ((Load8(data) * m) >> r)) * m)) * m, len, data + 8));
				}

				constexpr uint64_t hash_murmur2a_64_ct(const char* key, size_t len, uint64_t seed) {
					return StaticHashValueLoop64(len / 8, seed ^ (uint64_t(len) * m), (len), key);
				}
			} // namespace murmur2a
		} // namespace detail

		constexpr uint64_t calculate_hash64(uint64_t value) {
			value ^= value >> 33U;
			value *= 0xff51afd7ed558ccdULL;
			value ^= value >> 33U;

			value *= 0xc4ceb9fe1a85ec53ULL;
			value ^= value >> 33U;
			return static_cast<size_t>(value);
		}

		constexpr uint64_t calculate_hash64(const char* str) {
			size_t size = 0;
			while (str[size] != '\0')
				++size;

			return detail::murmur2a::hash_murmur2a_64_ct(str, size, detail::murmur2a::seed_64_const);
		}

		constexpr uint64_t calculate_hash64(const char* str, size_t length) {
			return detail::murmur2a::hash_murmur2a_64_ct(str, length, detail::murmur2a::seed_64_const);
		}

		GAIA_MSVC_WARNING_POP()

#else
	#error "Unknown hashing type defined"
#endif

	} // namespace utils
} // namespace gaia

#if GAIA_USE_STL_CONTAINERS

	#define REGISTER_HASH_TYPE_IMPL(type)                                                                                \
		template <>                                                                                                        \
		struct std::hash<type> {                                                                                           \
			size_t operator()(type obj) const noexcept { return obj.hash; }                                                  \
		};

REGISTER_HASH_TYPE_IMPL(gaia::utils::direct_hash_key<uint64_t>)
REGISTER_HASH_TYPE_IMPL(gaia::utils::direct_hash_key<uint32_t>)

	// Keeping this empty for now. Instead we register the types using the above.
	// The thing is any version of direct_hash_key<T> is going to be treated the same
	// way and because we are a header-only library there would be duplicates.
	#define REGISTER_HASH_TYPE(type)
#else
	#define REGISTER_HASH_TYPE(type)
#endif

#include <tuple>
#include <type_traits>
#include <utility>

#if GAIA_USE_STL_COMPATIBLE_CONTAINERS
	#include <algorithm>
#endif
#include <tuple>
#include <type_traits>

namespace gaia {
	constexpr size_t BadIndex = size_t(-1);

	namespace utils {
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
		// Element swapping
		//----------------------------------------------------------------------

		template <typename T>
		constexpr void swap(T& left, T& right) {
#if GAIA_USE_STL_COMPATIBLE_CONTAINERS
			return std::swap(left, right);
#else
			T tmp = std::move(left);
			left = std::move(right);
			right = std::move(tmp);
#endif
		}

		template <typename T, typename TCmpFunc>
		constexpr void swap_if(T& lhs, T& rhs, TCmpFunc cmpFunc) noexcept {
			if (!cmpFunc(lhs, rhs))
				utils::swap(lhs, rhs);
		}

		template <typename T, typename TCmpFunc>
		constexpr void swap_if_not(T& lhs, T& rhs, TCmpFunc cmpFunc) noexcept {
			if (cmpFunc(lhs, rhs))
				utils::swap(lhs, rhs);
		}

		template <typename C, typename TCmpFunc, typename TSortFunc>
		constexpr void try_swap_if(C& c, size_t lhs, size_t rhs, TCmpFunc cmpFunc, TSortFunc sortFunc) noexcept {
			if (!cmpFunc(c[lhs], c[rhs]))
				sortFunc(lhs, rhs);
		}

		template <typename C, typename TCmpFunc, typename TSortFunc>
		constexpr void try_swap_if_not(C& c, size_t lhs, size_t rhs, TCmpFunc cmpFunc, TSortFunc sortFunc) noexcept {
			if (cmpFunc(c[lhs], c[rhs]))
				sortFunc(lhs, rhs);
		}

		//----------------------------------------------------------------------
		// Value filling
		//----------------------------------------------------------------------

		template <class ForwardIt, class T>
		constexpr void fill(ForwardIt first, ForwardIt last, const T& value) {
#if GAIA_USE_STL_COMPATIBLE_CONTAINERS
			std::fill(first, last, value);
#else
			for (; first != last; ++first) {
				*first = value;
			}
#endif
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
		// Function helpers
		//----------------------------------------------------------------------

		template <typename... Type>
		struct func_type_list {};

		template <typename Class, typename Ret, typename... Args>
		func_type_list<Args...> func_args(Ret (Class::*)(Args...) const);

#define DEFINE_HAS_FUNCTION(function_name)                                                                             \
	template <typename T, typename... TArgs>                                                                             \
	auto has_##function_name(TArgs&&... args)                                                                            \
			->decltype(std::declval<T>().function_name(std::forward<TArgs>(args)...), std::true_type{}) {                    \
		return std::true_type{};                                                                                           \
	}                                                                                                                    \
	template <typename T>                                                                                                \
	auto has_##function_name(...)->std::false_type {                                                                     \
		return std::false_type{};                                                                                          \
	}

		DEFINE_HAS_FUNCTION(find)
		DEFINE_HAS_FUNCTION(find_if)
		DEFINE_HAS_FUNCTION(find_if_not)

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
			template <typename Func, auto... Is>
			constexpr void for_each_impl(Func func, std::index_sequence<Is...> /*no_name*/) {
				if constexpr ((std::is_invocable_v<Func&&, std::integral_constant<decltype(Is), Is>> && ...))
					(func(std::integral_constant<decltype(Is), Is>{}), ...);
				else
					(((void)Is, func()), ...);
			}

			template <typename Tuple, typename Func, auto... Is>
			void for_each_tuple_impl(Tuple&& tuple, Func func, std::index_sequence<Is...> /*no_name*/) {
				(func(std::get<Is>(tuple)), ...);
			}
		} // namespace detail

		//----------------------------------------------------------------------
		// Looping
		//----------------------------------------------------------------------

		//! Compile-time for loop. Performs \tparam Iters iterations.
		//!
		//! Example 1 (index argument):
		//! sarray<int, 10> arr = { ... };
		//! for_each<arr.size()>([&arr](auto i) {
		//!    GAIA_LOG_N("%d", i);
		//! });
		//!
		//! Example 2 (no index argument):
		//! uint32_t cnt = 0;
		//! for_each<10>([&cnt]() {
		//!    GAIA_LOG_N("Invocation number: %u", cnt++);
		//! });
		template <auto Iters, typename Func>
		constexpr void for_each(Func func) {
			detail::for_each_impl(func, std::make_index_sequence<Iters>());
		}

		//! Compile-time for loop with adjustable range and iteration size.
		//! Iteration starts at \tparam FirstIdx and end at \tparam LastIdx
		//! (excluding) in increments of \tparam Inc.
		//!
		//! Example 1 (index argument):
		//! sarray<int, 10> arr;
		//! for_each_ext<0, 10, 2>([&arr](auto i) {
		//!    GAIA_LOG_N("%d", i);
		//! });
		//!
		//! Example 2 (no argument):
		//! uint32_t cnt = 0;
		//! for_each_ext<0, 10, 2>([&cnt]() {
		//!    GAIA_LOG_N("Invocation number: %u", cnt++);
		//! });
		template <auto FirstIdx, auto LastIdx, auto Inc, typename Func>
		constexpr void for_each_ext(Func func) {
			if constexpr (FirstIdx < LastIdx) {
				if constexpr (std::is_invocable_v<Func&&, std::integral_constant<decltype(FirstIdx), FirstIdx>>)
					func(std::integral_constant<decltype(FirstIdx), FirstIdx>());
				else
					func();

				for_each_ext<FirstIdx + Inc, LastIdx, Inc>(func);
			}
		}

		//! Compile-time for loop over parameter packs.
		//!
		//! Example:
		//! template<typename... Args>
		//! void print(const Args&... args) {
		//!  for_each_pack([](const auto& value) {
		//!    std::cout << value << std::endl;
		//!  });
		//! }
		//! print(69, "likes", 420.0f);
		template <typename Func, typename... Args>
		constexpr void for_each_pack(Func func, Args&&... args) {
			(func(std::forward<Args>(args)), ...);
		}

		//! Compile-time for loop over tuples and other objects implementing
		//! tuple_size (sarray, std::pair etc).
		//!
		//! Example:
		//! for_each_tuple(
		//!		std::make_tuple(69, "likes", 420.0f),
		//!		[](const auto& value) {
		//! 		std::cout << value << std::endl;
		//! 	});
		template <typename Tuple, typename Func>
		constexpr void for_each_tuple(Tuple&& tuple, Func func) {
			detail::for_each_tuple_impl(
					std::forward<Tuple>(tuple), func,
					std::make_index_sequence<std::tuple_size<std::remove_reference_t<Tuple>>::value>{});
		}

		template <typename InputIt, typename Func>
		constexpr Func for_each(InputIt first, InputIt last, Func func) {
#if GAIA_USE_STL_COMPATIBLE_CONTAINERS
			return std::for_each(first, last, func);
#else
			for (; first != last; ++first)
				func(*first);
			return func;
#endif
		}

		template <typename C, typename Func>
		constexpr auto for_each(const C& arr, Func func) {
			return for_each(arr.begin(), arr.end(), func);
		}

		//----------------------------------------------------------------------
		// Lookups
		//----------------------------------------------------------------------

		template <typename InputIt, typename T>
		constexpr InputIt find(InputIt first, InputIt last, const T& value) {
#if GAIA_USE_STL_COMPATIBLE_CONTAINERS
			return std::find(first, last, value);
#else
			if constexpr (std::is_pointer_v<InputIt>) {
				const auto size = (size_t)GAIA_UTIL::distance(first, last);
				for (size_t i = 0; i < size; ++i) {
					if (first[i] == value)
						return &first[i];
				}
			} else if constexpr (std::is_same_v<typename InputIt::iterator_category, GAIA_UTIL::random_access_iterator_tag>) {
				const auto size = (size_t)GAIA_UTIL::distance(first, last);
				for (size_t i = 0; i < size; ++i) {
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
#endif
		}

		template <typename C, typename V>
		constexpr auto find(const C& arr, const V& item) {
			if constexpr (decltype(has_find<C>(item))::value)
				return arr.find(item);
			else
				return gaia::utils::find(arr.begin(), arr.end(), item);
		}

		template <typename InputIt, typename Func>
		constexpr InputIt find_if(InputIt first, InputIt last, Func func) {
#if GAIA_USE_STL_COMPATIBLE_CONTAINERS
			return std::find_if(first, last, func);
#else
			if constexpr (std::is_pointer_v<InputIt>) {
				const auto size = (size_t)GAIA_UTIL::distance(first, last);
				for (size_t i = 0; i < size; ++i) {
					if (func(first[i]))
						return &first[i];
				}
			} else if constexpr (std::is_same_v<typename InputIt::iterator_category, GAIA_UTIL::random_access_iterator_tag>) {
				const auto size = (size_t)GAIA_UTIL::distance(first, last);
				for (size_t i = 0; i < size; ++i) {
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
#endif
		}

		template <typename UnaryPredicate, typename C>
		constexpr auto find_if(const C& arr, UnaryPredicate predicate) {
			if constexpr (decltype(has_find_if<C>(predicate))::value)
				return arr.find_id(predicate);
			else
				return gaia::utils::find_if(arr.begin(), arr.end(), predicate);
		}

		template <typename InputIt, typename Func>
		constexpr InputIt find_if_not(InputIt first, InputIt last, Func func) {
#if GAIA_USE_STL_COMPATIBLE_CONTAINERS
			return std::find_if_not(first, last, func);
#else
			if constexpr (std::is_pointer_v<InputIt>) {
				const auto size = (size_t)GAIA_UTIL::distance(first, last);
				for (size_t i = 0; i < size; ++i) {
					if (!func(first[i]))
						return &first[i];
				}
			} else if constexpr (std::is_same_v<typename InputIt::iterator_category, GAIA_UTIL::random_access_iterator_tag>) {
				const auto size = (size_t)GAIA_UTIL::distance(first, last);
				for (size_t i = 0; i < size; ++i) {
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
#endif
		}

		template <typename UnaryPredicate, typename C>
		constexpr auto find_if_not(const C& arr, UnaryPredicate predicate) {
			if constexpr (decltype(has_find_if_not<C>(predicate))::value)
				return arr.find_if_not(predicate);
			else
				return gaia::utils::find_if_not(arr.begin(), arr.end(), predicate);
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

			return (decltype(BadIndex))GAIA_UTIL::distance(arr.begin(), it);
		}

		template <typename C>
		constexpr auto get_index_unsafe(const C& arr, typename C::const_reference item) {
			return (decltype(BadIndex))GAIA_UTIL::distance(arr.begin(), find(arr, item));
		}

		template <typename UnaryPredicate, typename C>
		constexpr auto get_index_if(const C& arr, UnaryPredicate predicate) {
			const auto it = find_if(arr, predicate);
			if (it == arr.end())
				return BadIndex;

			return (decltype(BadIndex))GAIA_UTIL::distance(arr.begin(), it);
		}

		template <typename UnaryPredicate, typename C>
		constexpr auto get_index_if_unsafe(const C& arr, UnaryPredicate predicate) {
			return (decltype(BadIndex))GAIA_UTIL::distance(arr.begin(), find_if(arr, predicate));
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
		void erase_fast(C& arr, size_t idx) {
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
				for (int j = low; j <= high - 1; j++) {
					if (func(arr[j], pivot))
						utils::swap(arr[++i], arr[j]);
				}
				utils::swap(arr[++i], arr[high]);
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
#if GAIA_USE_STL_COMPATIBLE_CONTAINERS
				//! TODO: replace with std::sort for c++20
				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(4244)
				detail::comb_sort_impl(arr, func);
				GAIA_MSVC_WARNING_POP()
#else
				detail::comb_sort_impl(arr, func);
#endif
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
				const size_t n = arr.size();
				for (size_t i = 0; i < n - 1; i++) {
					for (size_t j = 0; j < n - i - 1; j++)
						swap_if(arr[j], arr[j + 1], func);
				}
			} else {
#if GAIA_USE_STL_COMPATIBLE_CONTAINERS
				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(4244)
				std::sort(arr.begin(), arr.end(), func);
				GAIA_MSVC_WARNING_POP()
#else
				const int n = (int)arr.size();
				detail::quick_sort(arr, func, 0, n - 1);

#endif
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
				const size_t n = arr.size();
				for (size_t i = 0; i < n - 1; i++)
					for (size_t j = 0; j < n - i - 1; j++)
						try_swap_if(arr, j, j + 1, func, sortFunc);
			} else {
				GAIA_ASSERT(false && "sort currently supports at most 32 items in the array");
				// const int n = (int)arr.size();
				// detail::quick_sort(arr, 0, n - 1);
			}
		}
	} // namespace utils
} // namespace gaia

namespace gaia {
	namespace serialization {
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

			template <typename, typename = void>
			struct has_data_and_size: std::false_type {};
			template <typename T>
			struct has_data_and_size<T, std::void_t<decltype(data(std::declval<T>())), decltype(size(std::declval<T>()))>>:
					std::true_type {};

			DEFINE_HAS_FUNCTION(resize);
			// DEFINE_HAS_FUNCTION(serialize);
			DEFINE_HAS_FUNCTION(save);
			DEFINE_HAS_FUNCTION(load);

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
			GAIA_NODISCARD constexpr serialization_type_id get_integral_type() {
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
			GAIA_NODISCARD constexpr serialization_type_id get_floating_point_type() {
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
			GAIA_NODISCARD constexpr serialization_type_id get_type_id() {
				if constexpr (std::is_enum_v<T>)
					return get_integral_type<std::underlying_type_t<T>>();
				else if constexpr (std::is_integral_v<T>)
					return get_integral_type<T>();
				else if constexpr (std::is_floating_point_v<T>)
					return get_floating_point_type<T>();
				else if constexpr (detail::has_data_and_size<T>::value)
					return serialization_type_id::data_and_size;
				else if constexpr (std::is_class_v<T>)
					return serialization_type_id::trivial_wrapper;

				static_assert("Unsupported serialization type");
				return serialization_type_id::Last;
			}

			template <typename T>
			GAIA_NODISCARD constexpr uint32_t calculate_size_one(const T& item) {
				using type = typename std::decay_t<typename std::remove_pointer_t<T>>;

				constexpr auto id = detail::get_type_id<type>();
				static_assert(id != detail::serialization_type_id::Last);
				uint32_t size_in_bytes{};

				if constexpr (is_trivially_serializable<type>::value)
					size_in_bytes = sizeof(type);
				else if constexpr (detail::has_data_and_size<type>::value) {
					size_in_bytes += item.size();
				} else if constexpr (std::is_class_v<type>) {
					utils::for_each_member(item, [&](auto&&... items) {
						size_in_bytes += (calculate_size_one(items) + ...);
					});
				} else
					static_assert(!sizeof(type), "Type is not supported for serialization, yet");

				return size_in_bytes;
			}

			template <bool Write, typename Serializer, typename T>
			void serialize_data_one(Serializer& s, T&& arg) {
				using type = typename std::decay_t<typename std::remove_pointer_t<T>>;

				// TODO: Consider supporting custom save/load functions
				// if constexpr (decltype(has_serialize<type>(s, std::forward<T>(arg)))::value) {
				// 	arg.serialize<Write>(s, std::forward<T>(arg));
				// } else
				if constexpr (is_trivially_serializable<type>::value) {
					if constexpr (Write)
						s.save(std::forward<T>(arg));
					else
						s.load(std::forward<T>(arg));
				} else if constexpr (detail::has_data_and_size<type>::value) {
					if constexpr (Write) {
						if constexpr (decltype(has_resize<type>(0))::value) {
							const auto size = arg.size();
							s.save(size);
						}
						for (const auto& e: arg)
							serialize_data_one<Write>(s, e);
					} else {
						if constexpr (decltype(has_resize<type>(0))::value) {
							auto size = arg.size();
							s.load(size);
							arg.resize(size);
						}
						for (auto& e: arg)
							serialize_data_one<Write>(s, e);
					}
				} else if constexpr (std::is_class_v<type>) {
					utils::for_each_member(std::forward<T>(arg), [&s](auto&&... items) {
						// TODO: Handle contiguous blocks of trivially copiable types
						(serialize_data_one<Write>(s, items), ...);
					});
				} else
					static_assert(!sizeof(type), "Type is not supported for serialization, yet");
			}
		} // namespace detail

		//! Calculates the number of bytes necessary to serialize data using the "save" function.
		//! \warning Compile-time.
		template <typename T>
		GAIA_NODISCARD constexpr uint32_t calculate_size(const T& data) {
			return detail::calculate_size_one(data);
		}

		//! Write \param data using \tparam Writer at compile-time.
		//!
		//! \warning Writer has to implement a save function as follows:
		//! 					template <typename T> void save(const T& arg);
		template <typename Writer, typename T>
		void save(Writer& writer, const T& data) {
			detail::serialize_data_one<true>(writer, data);
		}

		//! Read \param data using \tparam Reader at compile-time.
		//!
		//! \warning Reader has to implement a save function as follows:
		//! 					template <typename T> void load(T& arg);
		template <typename Reader, typename T>
		void load(Reader& reader, T& data) {
			detail::serialize_data_one<false>(reader, data);
		}
	} // namespace serialization
} // namespace gaia

namespace gaia {
	namespace utils {

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
			constexpr static size_t GetMin(size_t a, size_t b) {
				return b < a ? b : a;
			}

			constexpr static size_t FindFirstOf(const char* data, size_t len, char toFind, size_t startPos = 0) {
				for (size_t i = startPos; i < len; ++i) {
					if (data[i] == toFind)
						return i;
				}
				return size_t(-1);
			}

			constexpr static size_t FindLastOf(const char* data, size_t len, char c, size_t startPos = size_t(-1)) {
				for (int64_t i = (int64_t)GetMin(len - 1, startPos); i >= 0; --i) {
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
				//		As a workaround FindFirstOf and FindLastOf were implemented

				size_t strLen = 0;
				while (GAIA_PRETTY_FUNCTION[strLen] != '\0')
					++strLen;

				std::span<const char> name{GAIA_PRETTY_FUNCTION, strLen};
				const auto prefixPos = FindFirstOf(name.data(), name.size(), GAIA_PRETTY_FUNCTION_PREFIX);
				const auto start = FindFirstOf(name.data(), name.size(), ' ', prefixPos + 1);
				const auto end = FindLastOf(name.data(), name.size(), GAIA_PRETTY_FUNCTION_SUFFIX);
				return name.subspan(start + 1, end - start - 1);
			}

			template <typename T>
			GAIA_NODISCARD static constexpr auto hash() noexcept {
#if GAIA_COMPILER_MSVC && _MSV_VER <= 1916
				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(4307)
#endif

				auto n = name<T>();
				return calculate_hash64(n.data(), n.size());

#if GAIA_COMPILER_MSVC && _MSV_VER <= 1916
				GAIA_MSVC_WARNING_PUSH()
#endif
			}
		};

	} // namespace utils
} // namespace gaia

#include <cinttypes>
#include <type_traits>

namespace gaia {
	namespace containers {
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

		public:
			class const_iterator {
			public:
				using iterator_category = GAIA_UTIL::random_access_iterator_tag;
				using value_type = uint32_t;
				using size_type = bitset<NBits>::size_type;

			private:
				const bitset<NBits>& m_bitset;
				value_type m_pos;

				uint32_t find_next_set_bit(uint32_t pos) const {
					value_type wordIndex = pos / bitset::BitsPerItem;
					GAIA_ASSERT(wordIndex < Items);
					size_type word = 0;

					const size_type posInWord = pos % bitset::BitsPerItem;
					if (posInWord < bitset::BitsPerItem - 1) {
						const size_type mask = (size_type(1) << (posInWord + 1)) - 1;
						const size_type maskInv = ~mask;
						word = m_bitset.m_data[wordIndex] & maskInv;
					}

					// No set bit in the current word, move to the next one
					while (word == 0) {
						if (wordIndex >= bitset::Items - 1)
							return pos;

						word = m_bitset.m_data[++wordIndex];
					}

					// Process the word
					uint32_t fwd = 0;

					GAIA_MSVC_WARNING_PUSH()
					GAIA_MSVC_WARNING_DISABLE(4244)
					if constexpr (bitset::BitsPerItem == 32)
						fwd = GAIA_FFS(word) - 1;
					else
						fwd = GAIA_FFS64(word) - 1;
					GAIA_MSVC_WARNING_POP()

					return wordIndex * bitset::BitsPerItem + fwd;
				}

				uint32_t find_prev_set_bit(uint32_t pos) const {
					value_type wordIndex = pos / bitset::BitsPerItem;
					GAIA_ASSERT(wordIndex < Items);
					size_type word = m_bitset.m_data[wordIndex];

					// No set bit in the current word, move to the previous word
					while (word == 0) {
						if (wordIndex == 0)
							return pos;
						word = m_bitset.m_data[--wordIndex];
					}

					// Process the word
					uint32_t ctz = 0;

					GAIA_MSVC_WARNING_PUSH()
					GAIA_MSVC_WARNING_DISABLE(4244)
					if constexpr (bitset::BitsPerItem == 32)
						ctz = GAIA_CTZ(word);
					else
						ctz = GAIA_CTZ64(word);
					GAIA_MSVC_WARNING_POP()

					return bitset::BitsPerItem * (wordIndex + 1) - ctz - 1;
				}

			public:
				const_iterator(const bitset<NBits>& bitset, value_type pos, bool fwd): m_bitset(bitset), m_pos(pos) {
					if (fwd) {
						// Find the first set bit)
						if (pos != 0 || !bitset.test(0)) {
							pos = find_next_set_bit(m_pos);
							// Point beyond the last item if no set bit was found
							if (pos == m_pos)
								pos = bitset.size();
						}
						m_pos = pos;
					} else {
						const auto bitsetSize = bitset.size();
						const auto lastBit = bitsetSize - 1;

						// Stay inside bounds
						if (pos >= bitsetSize)
							pos = bitsetSize - 1;

						// Find the last set bit
						if (pos != lastBit || !bitset.test(lastBit)) {
							const uint32_t newPos = find_prev_set_bit(pos);
							// Point one beyond the last found bit
							pos = (newPos == pos) ? bitsetSize : newPos + 1;
						}
						// Point one beyond the last found bit
						else
							++pos;

						m_pos = pos;
					}
				}

				GAIA_NODISCARD value_type operator*() const {
					return m_pos;
				}

				const_iterator& operator++() {
					uint32_t newPos = find_next_set_bit(m_pos);
					// Point one past the last item if no new bit was found
					if (newPos == m_pos)
						++newPos;
					m_pos = newPos;
					return *this;
				}

				GAIA_NODISCARD const_iterator operator++(int) {
					const_iterator temp(*this);
					++*this;
					return temp;
				}

				GAIA_NODISCARD bool operator==(const const_iterator& other) const {
					return m_pos == other.m_pos;
				}

				GAIA_NODISCARD bool operator!=(const const_iterator& other) const {
					return m_pos != other.m_pos;
				}
			};

			const_iterator begin() const {
				return const_iterator(*this, 0, true);
			}

			const_iterator end() const {
				return const_iterator(*this, NBits, false);
			}

			const_iterator cbegin() const {
				return const_iterator(*this, 0, true);
			}

			const_iterator cend() const {
				return const_iterator(*this, NBits, false);
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
				m_data[pos / BitsPerItem] ^= ((size_type)1 << (pos % BitsPerItem));
			}

			//! Flips all bits from \param bitFrom to \param bitTo (including)
			constexpr bitset& flip(uint32_t bitFrom, uint32_t bitTo) {
				GAIA_ASSERT(bitFrom <= bitTo);
				GAIA_ASSERT(bitFrom < size());

				if GAIA_UNLIKELY (size() == 0)
					return *this;

				for (uint32_t i = bitFrom; i <= bitTo; i++) {
					uint32_t wordIdx = i / BitsPerItem;
					uint32_t bitOffset = i % BitsPerItem;
					m_data[wordIdx] ^= ((size_type)1 << bitOffset);
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
	} // namespace containers
} // namespace gaia

#define USE_VECTOR GAIA_USE_STL_CONTAINERS

#if USE_VECTOR == 1

	#include <vector>

namespace gaia {
	namespace containers {
		template <typename T>
		using darray = std::vector<T>;
	} // namespace containers
} // namespace gaia
#elif USE_VECTOR == 0

	

#include <cstddef>
#include <initializer_list>
#include <type_traits>
#include <utility>

namespace gaia {
	namespace containers {
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
			using difference_type = std::ptrdiff_t;
			using size_type = size_t;

		private:
			pointer m_pData = nullptr;
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
					m_pData = new T[m_cap = 4];
					return;
				}

				// Increase the size of an existing array.
				// We increase the capacity in multiples of 1.5 which is about the golden ratio (1.618).
				// This means we prefer more frequent allocations over memory fragmentation.
				pointer pDataOld = m_pData;
				m_pData = new T[m_cap = (cap * 3) / 2 + 1];
				utils::move_elements(m_pData, pDataOld, cnt);
				delete[] pDataOld;
			}

		public:
			class iterator {
				friend class darr;

			public:
				using iterator_category = GAIA_UTIL::random_access_iterator_tag;
				using value_type = T;
				using difference_type = std::ptrdiff_t;
				using pointer = T*;
				using reference = T&;
				using size_type = darr::size_type;

			private:
				pointer m_ptr;

			public:
				iterator(T* ptr): m_ptr(ptr) {}

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
					return m_ptr - other.m_ptr;
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

			class const_iterator {
				friend class darr;

			public:
				using iterator_category = GAIA_UTIL::random_access_iterator_tag;
				using value_type = const T;
				using difference_type = std::ptrdiff_t;
				using pointer = const T*;
				using reference = const T&;
				using size_type = darr::size_type;

			private:
				pointer m_ptr;

			public:
				const_iterator(pointer ptr): m_ptr(ptr) {}

				reference operator*() const {
					return *m_ptr;
				}
				pointer operator->() const {
					return m_ptr;
				}
				const_iterator operator[](size_type offset) const {
					return {m_ptr + offset};
				}

				const_iterator& operator+=(size_type diff) {
					m_ptr += diff;
					return *this;
				}
				const_iterator& operator-=(size_type diff) {
					m_ptr -= diff;
					return *this;
				}
				const_iterator& operator++() {
					++m_ptr;
					return *this;
				}
				const_iterator operator++(int) {
					const_iterator temp(*this);
					++*this;
					return temp;
				}
				const_iterator& operator--() {
					--m_ptr;
					return *this;
				}
				const_iterator operator--(int) {
					const_iterator temp(*this);
					--*this;
					return temp;
				}

				const_iterator operator+(size_type offset) const {
					return {m_ptr + offset};
				}
				const_iterator operator-(size_type offset) const {
					return {m_ptr - offset};
				}
				difference_type operator-(const const_iterator& other) const {
					return m_ptr - other.m_ptr;
				}

				GAIA_NODISCARD bool operator==(const const_iterator& other) const {
					return m_ptr == other.m_ptr;
				}
				GAIA_NODISCARD bool operator!=(const const_iterator& other) const {
					return m_ptr != other.m_ptr;
				}
				GAIA_NODISCARD bool operator>(const const_iterator& other) const {
					return m_ptr > other.m_ptr;
				}
				GAIA_NODISCARD bool operator>=(const const_iterator& other) const {
					return m_ptr >= other.m_ptr;
				}
				GAIA_NODISCARD bool operator<(const const_iterator& other) const {
					return m_ptr < other.m_ptr;
				}
				GAIA_NODISCARD bool operator<=(const const_iterator& other) const {
					return m_ptr <= other.m_ptr;
				}
			};

			darr() noexcept = default;

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
				const auto count = (size_type)GAIA_UTIL::distance(first, last);
				resize(count);

				if constexpr (std::is_pointer_v<InputIt>) {
					for (size_type i = 0; i < count; ++i)
						m_pData[i] = first[i];
				} else if constexpr (std::is_same_v<
																 typename InputIt::iterator_category, GAIA_UTIL::random_access_iterator_tag>) {
					for (size_type i = 0; i < count; ++i)
						m_pData[i] = *(first[i]);
				} else {
					size_type i = 0;
					for (auto it = first; it != last; ++it)
						m_pData[i++] = *it;
				}
			}

			darr(std::initializer_list<T> il): darr(il.begin(), il.end()) {}

			darr(const darr& other): darr(other.begin(), other.end()) {}

			darr(darr&& other) noexcept: m_pData(other.m_pData), m_cnt(other.m_cnt), m_cap(other.m_cap) {
				other.m_cnt = size_type(0);
				other.m_cap = size_type(0);
				other.m_pData = nullptr;
			}

			GAIA_NODISCARD darr& operator=(std::initializer_list<T> il) {
				*this = darr(il.begin(), il.end());
				return *this;
			}

			GAIA_NODISCARD darr& operator=(const darr& other) {
				GAIA_ASSERT(GAIA_UTIL::addressof(other) != this);

				resize(other.size());
				utils::copy_elements(m_pData, other.m_pData, other.size());

				return *this;
			}

			GAIA_NODISCARD darr& operator=(darr&& other) noexcept {
				GAIA_ASSERT(GAIA_UTIL::addressof(other) != this);

				m_cnt = other.m_cnt;
				m_cap = other.m_cap;
				m_pData = other.m_pData;

				other.m_cnt = size_type(0);
				other.m_cap = size_type(0);
				other.m_pData = nullptr;

				return *this;
			}

			~darr() {
				delete[] m_pData;
			}

			GAIA_NODISCARD pointer data() noexcept {
				return (pointer)m_pData;
			}

			GAIA_NODISCARD const_pointer data() const noexcept {
				return (const_pointer)m_pData;
			}

			GAIA_NODISCARD reference operator[](size_type pos) noexcept {
				GAIA_ASSERT(pos < size());
				return (reference)m_pData[pos];
			}

			GAIA_NODISCARD const_reference operator[](size_type pos) const noexcept {
				GAIA_ASSERT(pos < size());
				return (const_reference)m_pData[pos];
			}

			void reserve(size_type count) {
				if (count <= m_cap)
					return;

				pointer pDataOld = m_pData;
				m_pData = new T[count];
				if (pDataOld != nullptr) {
					utils::move_elements(m_pData, pDataOld, size());
					delete[] pDataOld;
				}

				m_cap = count;
			}

			void resize(size_type count) {
				if (count <= m_cap) {
					m_cnt = count;
					return;
				}

				pointer pDataOld = m_pData;
				m_pData = new T[count];
				if GAIA_LIKELY (pDataOld != nullptr) {
					utils::move_elements(m_pData, pDataOld, size());
					delete[] pDataOld;
				}

				m_cap = count;
				m_cnt = count;
			}

			void push_back(const T& arg) {
				try_grow();
				m_pData[m_cnt++] = arg;
			}

			void push_back(T&& arg) {
				try_grow();
				m_pData[m_cnt++] = std::forward<T>(arg);
			}

			template <typename... Args>
			reference emplace_back(Args&&... args) {
				try_grow();

				reference ref = m_pData[m_cnt++];
				::new (&ref) T(std::forward<Args>(args)...);
				return ref;
			}

			void pop_back() noexcept {
				GAIA_ASSERT(!empty());
				reference ref = m_pData[--m_cnt];
				ref.~T();
			}

			GAIA_NODISCARD iterator erase(iterator pos) noexcept {
				GAIA_ASSERT(pos.m_ptr >= &m_pData[0] && pos.m_ptr < &m_pData[m_cap - 1]);

				const auto idxSrc = (size_type)GAIA_UTIL::distance(pos, begin());
				const auto idxDst = size() - 1;

				utils::shift_elements_left(&m_pData[idxSrc], idxDst - idxSrc);
				--m_cnt;

				return iterator((T*)m_pData + idxSrc);
			}

			GAIA_NODISCARD const_iterator erase(const_iterator pos) noexcept {
				GAIA_ASSERT(pos.m_ptr >= &m_pData[0] && pos.m_ptr < &m_pData[m_cap - 1]);

				const auto idxSrc = (size_type)GAIA_UTIL::distance(pos, begin());
				const auto idxDst = size() - 1;

				utils::shift_elements_left(&m_pData[idxSrc], idxDst - idxSrc);
				--m_cnt;

				return iterator((const T*)m_pData + idxSrc);
			}

			GAIA_NODISCARD iterator erase(iterator first, iterator last) noexcept {
				GAIA_ASSERT(first.m_cnt >= 0 && first.m_cnt < size());
				GAIA_ASSERT(last.m_cnt >= 0 && last.m_cnt < size());
				GAIA_ASSERT(last.m_cnt >= first.m_cnt);

				const auto size = last.m_cnt - first.m_cnt;
				utils::shift_elements_left(&m_pData[first.cnt], size);
				m_cnt -= size;

				return {(T*)m_pData + size_type(last.m_cnt)};
			}

			void clear() {
				resize(0);
			}

			void shirk_to_fit() {
				if (capacity() == size())
					return;
				T* old = m_pData;
				m_pData = new T[m_cap = size()];
				move_elements(m_pData, old, size());
				delete[] old;
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

			GAIA_NODISCARD reference front() noexcept {
				GAIA_ASSERT(!empty());
				return *begin();
			}

			GAIA_NODISCARD const_reference front() const noexcept {
				GAIA_ASSERT(!empty());
				return *begin();
			}

			GAIA_NODISCARD reference back() noexcept {
				GAIA_ASSERT(!empty());
				return m_pData[m_cnt - 1];
			}

			GAIA_NODISCARD const_reference back() const noexcept {
				GAIA_ASSERT(!empty());
				return m_pData[m_cnt - 1];
			}

			GAIA_NODISCARD iterator begin() const noexcept {
				return {(T*)m_pData};
			}

			GAIA_NODISCARD const_iterator cbegin() const noexcept {
				return {(const T*)m_pData};
			}

			GAIA_NODISCARD iterator rbegin() const noexcept {
				return {(T*)&back()};
			}

			GAIA_NODISCARD const_iterator crbegin() const noexcept {
				return {(T*)&back()};
			}

			GAIA_NODISCARD iterator end() const noexcept {
				return {(T*)m_pData + size()};
			}

			GAIA_NODISCARD const_iterator cend() const noexcept {
				return {(const T*)m_pData + size()};
			}

			GAIA_NODISCARD iterator rend() const noexcept {
				return {(T*)m_pData - 1};
			}

			GAIA_NODISCARD const_iterator crend() const noexcept {
				return {(const T*)m_pData - 1};
			}

			GAIA_NODISCARD bool operator==(const darr& other) const {
				if (m_cnt != other.m_cnt)
					return false;
				const size_type n = size();
				for (size_type i = 0; i < n; ++i)
					if (!(m_pData[i] == other.m_pData[i]))
						return false;
				return true;
			}
		};
	} // namespace containers

} // namespace gaia

namespace gaia {
	namespace containers {
		template <typename T>
		using darray = containers::darr<T>;
	} // namespace containers
} // namespace gaia
#else

	// You can add your custom container here
	#error Unsupported value used for USE_VECTOR

#endif

#include <cinttypes>
#include <type_traits>

namespace gaia {
	namespace containers {
		class dbitset {
		private:
			struct size_type_selector {
				static constexpr bool Use32Bit = sizeof(size_t) == 4;
				using type = std::conditional_t<Use32Bit, uint32_t, uint64_t>;
			};

			using difference_type = std::ptrdiff_t;
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

			uint32_t Items() const {
				return (m_cnt + BitsPerItem - 1) / BitsPerItem;
			}

			bool HasTrailingBits() const {
				return (m_cnt % BitsPerItem) != 0;
			}

			size_type LastItemMask() const {
				return ((size_type)1 << (m_cnt % BitsPerItem)) - 1;
			}

			void try_grow(uint32_t bitsWanted) {
				uint32_t itemsOld = Items();
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
					utils::move_elements(m_pData, pDataOld, itemsNew - itemsOld);
					for (uint32_t i = itemsOld; i < itemsNew; ++i)
						m_pData[i] = 0;

					// Release the old data
					delete[] pDataOld;
				}
			}

		public:
			class const_iterator {
			public:
				using iterator_category = GAIA_UTIL::random_access_iterator_tag;
				using value_type = uint32_t;
				using size_type = dbitset::size_type;

			private:
				const dbitset& m_bitset;
				value_type m_pos;

				uint32_t find_next_set_bit(uint32_t pos) const {
					value_type wordIndex = pos / dbitset::BitsPerItem;
					GAIA_ASSERT(wordIndex < m_bitset.Items());
					size_type word = 0;

					const size_type posInWord = pos % dbitset::BitsPerItem;
					if (posInWord < dbitset::BitsPerItem - 1) {
						const size_type mask = (size_type(1) << (posInWord + 1)) - 1;
						const size_type maskInv = ~mask;
						word = m_bitset.m_pData[wordIndex] & maskInv;
					}

					// No set bit in the current word, move to the next one
					while (word == 0) {
						if (wordIndex >= m_bitset.Items() - 1)
							return pos;

						word = m_bitset.m_pData[++wordIndex];
					}

					// Process the word
					uint32_t fwd = 0;

					GAIA_MSVC_WARNING_PUSH()
					GAIA_MSVC_WARNING_DISABLE(4244)
					if constexpr (dbitset::BitsPerItem == 32)
						fwd = GAIA_FFS(word) - 1;
					else
						fwd = GAIA_FFS64(word) - 1;
					GAIA_MSVC_WARNING_POP()

					return wordIndex * dbitset::BitsPerItem + fwd;
				}

				uint32_t find_prev_set_bit(uint32_t pos) const {
					value_type wordIndex = pos / dbitset::BitsPerItem;
					GAIA_ASSERT(wordIndex < m_bitset.Items());
					size_type word = m_bitset.m_pData[wordIndex];

					// No set bit in the current word, move to the previous word
					while (word == 0) {
						if (wordIndex == 0)
							return pos;
						word = m_bitset.m_pData[--wordIndex];
					}

					// Process the word
					uint32_t ctz = 0;

					GAIA_MSVC_WARNING_PUSH()
					GAIA_MSVC_WARNING_DISABLE(4244)
					if constexpr (dbitset::BitsPerItem == 32)
						ctz = GAIA_CTZ(word);
					else
						ctz = GAIA_CTZ64(word);
					GAIA_MSVC_WARNING_POP()

					return dbitset::BitsPerItem * (wordIndex + 1) - ctz - 1;
				}

			public:
				const_iterator(const dbitset& bitset, value_type pos, bool fwd): m_bitset(bitset), m_pos(pos) {
					const auto bitsetSize = bitset.size();
					if (bitsetSize == 0) {
						// Point beyond the last item if no set bit was found
						m_pos = bitsetSize;
						return;
					}

					// Stay inside bounds
					const auto lastBit = bitsetSize - 1;
					if (pos >= bitsetSize)
						pos = lastBit;

					if (fwd) {
						// Find the first set bit)
						if (pos != 0 || !bitset.test(0)) {
							pos = find_next_set_bit(m_pos);
							// Point beyond the last item if no set bit was found
							if (pos == m_pos)
								pos = bitsetSize;
						}
					} else {
						// Find the last set bit
						if (pos != lastBit || !bitset.test(lastBit)) {
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

				GAIA_NODISCARD value_type operator*() const {
					return m_pos;
				}

				const_iterator& operator++() {
					uint32_t newPos = find_next_set_bit(m_pos);
					// Point one past the last item if no new bit was found
					if (newPos == m_pos)
						++newPos;
					m_pos = newPos;
					return *this;
				}

				GAIA_NODISCARD const_iterator operator++(int) {
					const_iterator temp(*this);
					++*this;
					return temp;
				}

				GAIA_NODISCARD bool operator==(const const_iterator& other) const {
					return m_pos == other.m_pos;
				}

				GAIA_NODISCARD bool operator!=(const const_iterator& other) const {
					return m_pos != other.m_pos;
				}
			};

			dbitset() {
				// Allocate at least 128 bits
				reserve(128);
			}

			dbitset(uint32_t reserveBits) {
				reserve(reserveBits);
			}

			~dbitset() {
				delete[] m_pData;
			}

			dbitset(const dbitset& other) {
				*this = other;
			}
			dbitset& operator=(const dbitset& other) {
				GAIA_ASSERT(GAIA_UTIL::addressof(other) != this);

				resize(other.m_cnt);
				utils::copy_elements(m_pData, other.m_pData, other.Items());
				return *this;
			}

			dbitset(dbitset&& other) noexcept {
				*this = std::move(other);
			}
			dbitset& operator=(dbitset&& other) noexcept {
				GAIA_ASSERT(GAIA_UTIL::addressof(other) != this);

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
					const uint32_t itemsOld = Items();
					// Copy the old data over and set the old data to zeros
					utils::move_elements(m_pData, pDataOld, size());
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

				const uint32_t itemsOld = Items();

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
					utils::move_elements(m_pData, pDataOld, size());
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

			const_iterator cbegin() const {
				return const_iterator(*this, 0, true);
			}

			const_iterator cend() const {
				return const_iterator(*this, size(), false);
			}

			GAIA_NODISCARD bool operator[](uint32_t pos) const {
				return test(pos);
			}

			GAIA_NODISCARD bool operator==(const dbitset& other) const {
				const uint32_t items = Items();
				for (uint32_t i = 0; i < items; ++i)
					if (m_pData[i] != other.m_pData[i])
						return false;
				return true;
			}

			GAIA_NODISCARD bool operator!=(const dbitset& other) const {
				const uint32_t items = Items();
				for (uint32_t i = 0; i < items; ++i)
					if (m_pData[i] == other.m_pData[i])
						return false;
				return true;
			}

			//! Sets all bits
			void set() {
				if GAIA_UNLIKELY (size() == 0)
					return;

				const auto items = Items();
				const auto lastItemMask = LastItemMask();

				if (lastItemMask != 0) {
					uint32_t i = 0;
					for (; i < items - 1; ++i)
						m_pData[i] = (size_type)-1;
					m_pData[i] = lastItemMask;
				} else {
					for (uint32_t i = 0; i < items; ++i)
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

				const auto items = Items();
				const auto lastItemMask = LastItemMask();

				if (lastItemMask != 0) {
					uint32_t i = 0;
					for (; i < items - 1; ++i)
						m_pData[i] = ~m_pData[i];
					m_pData[i] = (~m_pData[i]) & lastItemMask;
				} else {
					for (uint32_t i = 0; i <= items; ++i)
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
				GAIA_ASSERT(bitFrom < size());

				if GAIA_UNLIKELY (size() == 0)
					return *this;

				for (uint32_t i = bitFrom; i <= bitTo; i++) {
					uint32_t wordIdx = i / BitsPerItem;
					uint32_t bitOffset = i % BitsPerItem;
					m_pData[wordIdx] ^= ((size_type)1 << bitOffset);
				}

				return *this;
			}

			//! Unsets all bits
			void reset() {
				const auto items = Items();
				for (uint32_t i = 0; i < items; ++i)
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
				const auto items = Items() - 1;
				const auto lastItemMask = LastItemMask();

				for (uint32_t i = 0; i < items; ++i)
					if (m_pData[i] != (size_type)-1)
						return false;

				if (HasTrailingBits())
					return (m_pData[items] & lastItemMask) == lastItemMask;
				else
					return m_pData[items] == (size_type)-1;
			}

			//! Checks if any bit is set
			GAIA_NODISCARD bool any() const {
				const auto items = Items();
				for (uint32_t i = 0; i < items; ++i)
					if (m_pData[i] != 0)
						return true;
				return false;
			}

			//! Checks if all bits are reset
			GAIA_NODISCARD bool none() const {
				const auto items = Items();
				for (uint32_t i = 0; i < items; ++i)
					if (m_pData[i] != 0)
						return false;
				return true;
			}

			//! Returns the number of set bits
			GAIA_NODISCARD uint32_t count() const {
				uint32_t total = 0;

				const auto items = Items();

				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(4244)
				if constexpr (sizeof(size_type) == 4) {
					for (uint32_t i = 0; i < items; ++i)
						total += GAIA_POPCNT(m_pData[i]);
				} else {
					for (uint32_t i = 0; i < items; ++i)
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
	} // namespace containers
} // namespace gaia

#define USE_HASHMAP GAIA_USE_STL_CONTAINERS

#if USE_HASHMAP == 1
	#include <unordered_map>
namespace gaia {
	namespace containers {
		template <typename Key, typename Data>
		using map = std::unordered_map<Key, Data>;
	} // namespace containers
} // namespace gaia
#elif USE_HASHMAP == 0
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
#if GAIA_USE_STL_CONTAINERS
	#include <functional>
#endif
#include <initializer_list>
#include <new>
#include <tuple>
#include <type_traits>
#include <utility>

// #define ROBIN_HOOD_STD_SMARTPOINTERS
#if defined(ROBIN_HOOD_STD_SMARTPOINTERS) || GAIA_USE_STL_CONTAINERS
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
			operator=(const BulkPoolAllocator& ROBIN_HOOD_UNUSED(o) /*unused*/) noexcept {
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
					gaia::utils::mem_free(mListForFree);
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
					gaia::utils::mem_free(ptr);
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
						"gaia::utils::mem_alloc " << bytes << " = " << ALIGNMENT << " + " << ALIGNED_SIZE << " * "
																			<< numElementsToAlloc)
				add(assertNotNull<std::bad_alloc>(gaia::utils::mem_alloc(bytes)), bytes);
				return mHead;
			}

			// enforce byte alignment of the T's
			static constexpr size_t ALIGNMENT =
					gaia::utils::get_max(std::alignment_of<T>::value, std::alignment_of<T*>::value);

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
				gaia::utils::mem_free(ptr);
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

#if GAIA_USE_STL_CONTAINERS
	// A thin wrapper around std::hash, performing an additional simple mixing step of the result.
	template <typename T, typename Enable = void>
	struct hash: public std::hash<T> {
		size_t operator()(T const& obj) const
				noexcept(noexcept(std::declval<std::hash<T>>().operator()(std::declval<T const&>()))) {
			// call base hash
			auto result = std::hash<T>::operator()(obj);
			// return mixed of that, to be save against identity has
			return hash_int(static_cast<detail::SizeT>(result));
		}
	};
#else
	template <typename T, typename Enable = void>
	struct hash {
		size_t operator()(T const& obj) const noexcept {
			return hash_bytes(&obj, sizeof(T));
		}
	};
#endif

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
	struct hash<T, typename std::enable_if<gaia::utils::is_direct_hash_key_v<T>>::type> {
		size_t operator()(const T& obj) const noexcept {
			return obj.hash;
		}
	};

#define ROBIN_HOOD_HASH_INT(T)                                                                                         \
	template <>                                                                                                          \
	struct hash<T> {                                                                                                     \
		size_t operator()(T const& obj) const noexcept { return hash_int(static_cast<uint64_t>(obj)); }                    \
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

		template <typename T>
		struct void_type {
			using type = void;
		};

		template <typename T, typename = void>
		struct has_is_transparent: public std::false_type {};

		template <typename T>
		struct has_is_transparent<T, typename void_type<typename T::is_transparent>::type>: public std::true_type {};

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
				using iterator_category = GAIA_UTIL::forward_iterator_tag;

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
					mInfo++;
					mKeyVals++;
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
				if constexpr (!gaia::utils::is_direct_hash_key_v<HashKeyRaw>) {
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
																GAIA_UTIL::distance(mKeyVals, reinterpret_cast_no_cast_align_warning<Node*>(mInfo)));
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
							"gaia::utils::mem_alloc " << numBytesTotal << " = calcNumBytesTotal(" << numElementsWithBuffer << ")")
					mHashMultiplier = o.mHashMultiplier;
					mKeyVals = static_cast<Node*>(detail::assertNotNull<std::bad_alloc>(gaia::utils::mem_alloc(numBytesTotal)));
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
						gaia::utils::mem_free(mKeyVals);
					}

					auto const numElementsWithBuffer = calcNumElementsWithBuffer(o.mMask + 1);
					auto const numBytesTotal = calcNumBytesTotal(numElementsWithBuffer);
					ROBIN_HOOD_LOG(
							"gaia::utils::mem_alloc " << numBytesTotal << " = calcNumBytesTotal(" << numElementsWithBuffer << ")")
					mKeyVals = static_cast<Node*>(detail::assertNotNull<std::bad_alloc>(gaia::utils::mem_alloc(numBytesTotal)));

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
				gaia::utils::fill(mInfo, mInfo + calcNumBytesInfo(numElementsWithBuffer), z);
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
				return numElements + gaia::utils::get_min(maxNumElementsAllowed, (static_cast<size_t>(0xFF)));
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
				auto const minElementsAllowed = gaia::utils::get_max(c, mNumElements);
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
							gaia::utils::mem_free(oldKeyVals);
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
				mKeyVals =
						reinterpret_cast<Node*>(detail::assertNotNull<std::bad_alloc>(gaia::utils::mem_alloc(numBytesTotal)));
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
					gaia::utils::mem_free(mKeyVals);
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
			typename Key, typename T, typename Hash = hash<Key>, typename KeyEqual = GAIA_UTIL::equal_to<Key>,
			size_t MaxLoadFactor100 = 80>
	using unordered_flat_map = detail::Table<true, MaxLoadFactor100, Key, T, Hash, KeyEqual>;

	template <
			typename Key, typename T, typename Hash = hash<Key>, typename KeyEqual = GAIA_UTIL::equal_to<Key>,
			size_t MaxLoadFactor100 = 80>
	using unordered_node_map = detail::Table<false, MaxLoadFactor100, Key, T, Hash, KeyEqual>;

	template <
			typename Key, typename T, typename Hash = hash<Key>, typename KeyEqual = GAIA_UTIL::equal_to<Key>,
			size_t MaxLoadFactor100 = 80>
	using unordered_map = detail::Table<
			sizeof(robin_hood::pair<Key, T>) <= sizeof(size_t) * 6 &&
					std::is_nothrow_move_constructible<robin_hood::pair<Key, T>>::value &&
					std::is_nothrow_move_assignable<robin_hood::pair<Key, T>>::value,
			MaxLoadFactor100, Key, T, Hash, KeyEqual>;

	// set

	template <
			typename Key, typename Hash = hash<Key>, typename KeyEqual = GAIA_UTIL::equal_to<Key>,
			size_t MaxLoadFactor100 = 80>
	using unordered_flat_set = detail::Table<true, MaxLoadFactor100, Key, void, Hash, KeyEqual>;

	template <
			typename Key, typename Hash = hash<Key>, typename KeyEqual = GAIA_UTIL::equal_to<Key>,
			size_t MaxLoadFactor100 = 80>
	using unordered_node_set = detail::Table<false, MaxLoadFactor100, Key, void, Hash, KeyEqual>;

	template <
			typename Key, typename Hash = hash<Key>, typename KeyEqual = GAIA_UTIL::equal_to<Key>,
			size_t MaxLoadFactor100 = 80>
	using unordered_set = detail::Table<
			sizeof(Key) <= sizeof(size_t) * 6 && std::is_nothrow_move_constructible<Key>::value &&
					std::is_nothrow_move_assignable<Key>::value,
			MaxLoadFactor100, Key, void, Hash, KeyEqual>;

} // namespace robin_hood

#endif

namespace gaia {
	namespace containers {
		template <typename Key, typename Data>
		using map = robin_hood::unordered_flat_map<Key, Data>;
	} // namespace containers
} // namespace gaia
#else

	// You can add your custom container here
	#error Unsupported value used for USE_HASHMAP

#endif

// TODO: There is no quickly achievable std alternative so go with gaia container

#include <cstddef>
#include <type_traits>
#include <utility>

namespace gaia {
	namespace containers {
		//! Array of elements of type \tparam T with fixed capacity \tparam N and variable size allocated on stack.
		//! Interface compatiblity with std::array where it matters.
		template <typename T, size_t N>
		class sarr_ext {
		public:
			using iterator_category = GAIA_UTIL::random_access_iterator_tag;
			using value_type = T;
			using reference = T&;
			using const_reference = const T&;
			using pointer = T*;
			using const_pointer = T*;
			using difference_type = std::ptrdiff_t;
			using size_type = decltype(N);

			static constexpr size_type extent = N;

		private:
			T m_data[N != 0U ? N : 1]; // support zero-size arrays
			size_type m_cnt = size_type(0);

		public:
			class iterator {
			public:
				using iterator_category = GAIA_UTIL::random_access_iterator_tag;
				using value_type = T;
				using difference_type = std::ptrdiff_t;
				using pointer = T*;
				using reference = T&;
				using size_type = decltype(N);

			private:
				pointer m_ptr;

			public:
				constexpr iterator(T* ptr): m_ptr(ptr) {}

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
					return m_ptr - other.m_ptr;
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

			class const_iterator {
			public:
				using iterator_category = GAIA_UTIL::random_access_iterator_tag;
				using value_type = const T;
				using difference_type = std::ptrdiff_t;
				using pointer = const T*;
				using reference = const T&;
				using size_type = decltype(N);

			private:
				pointer m_ptr;

			public:
				constexpr const_iterator(pointer ptr): m_ptr(ptr) {}

				constexpr reference operator*() const {
					return *m_ptr;
				}
				constexpr pointer operator->() const {
					return m_ptr;
				}
				constexpr const_iterator operator[](size_type offset) const {
					return {m_ptr + offset};
				}

				constexpr const_iterator& operator+=(size_type diff) {
					m_ptr += diff;
					return *this;
				}
				constexpr const_iterator& operator-=(size_type diff) {
					m_ptr -= diff;
					return *this;
				}
				constexpr const_iterator& operator++() {
					++m_ptr;
					return *this;
				}
				constexpr const_iterator operator++(int) {
					const_iterator temp(*this);
					++*this;
					return temp;
				}
				constexpr const_iterator& operator--() {
					--m_ptr;
					return *this;
				}
				constexpr const_iterator operator--(int) {
					const_iterator temp(*this);
					--*this;
					return temp;
				}

				constexpr const_iterator operator+(size_type offset) const {
					return {m_ptr + offset};
				}
				constexpr const_iterator operator-(size_type offset) const {
					return {m_ptr - offset};
				}
				constexpr difference_type operator-(const const_iterator& other) const {
					return m_ptr - other.m_ptr;
				}

				GAIA_NODISCARD constexpr bool operator==(const const_iterator& other) const {
					return m_ptr == other.m_ptr;
				}
				GAIA_NODISCARD constexpr bool operator!=(const const_iterator& other) const {
					return m_ptr != other.m_ptr;
				}
				GAIA_NODISCARD constexpr bool operator>(const const_iterator& other) const {
					return m_ptr > other.m_ptr;
				}
				GAIA_NODISCARD constexpr bool operator>=(const const_iterator& other) const {
					return m_ptr >= other.m_ptr;
				}
				GAIA_NODISCARD constexpr bool operator<(const const_iterator& other) const {
					return m_ptr < other.m_ptr;
				}
				GAIA_NODISCARD constexpr bool operator<=(const const_iterator& other) const {
					return m_ptr <= other.m_ptr;
				}
			};

			sarr_ext() noexcept = default;
			~sarr_ext() = default;

			constexpr sarr_ext(size_type count, const T& value) noexcept {
				resize(count);

				for (auto it: *this)
					*it = value;
			}

			constexpr sarr_ext(size_type count) noexcept {
				resize(count);
			}

			template <typename InputIt>
			constexpr sarr_ext(InputIt first, InputIt last) noexcept {
				const auto count = (size_type)GAIA_UTIL::distance(first, last);
				resize(count);

				if constexpr (std::is_pointer_v<InputIt>) {
					for (size_type i = 0; i < count; ++i)
						m_data[i] = first[i];
				} else if constexpr (std::is_same_v<
																 typename InputIt::iterator_category, GAIA_UTIL::random_access_iterator_tag>) {
					for (size_type i = 0; i < count; ++i)
						m_data[i] = *(first[i]);
				} else {
					size_type i = 0;
					for (auto it = first; it != last; ++it)
						m_data[i++] = *it;
				}
			}

			constexpr sarr_ext(std::initializer_list<T> il): sarr_ext(il.begin(), il.end()) {}

			constexpr sarr_ext(const sarr_ext& other): sarr_ext(other.begin(), other.end()) {}

			constexpr sarr_ext(sarr_ext&& other) noexcept: m_cnt(other.m_cnt) {
				GAIA_ASSERT(GAIA_UTIL::addressof(other) != this);

				utils::move_elements(m_data, other.m_data, other.size());

				other.m_cnt = size_type(0);
			}

			GAIA_NODISCARD sarr_ext& operator=(std::initializer_list<T> il) {
				*this = sarr_ext(il.begin(), il.end());
				return *this;
			}

			GAIA_NODISCARD constexpr sarr_ext& operator=(const sarr_ext& other) {
				GAIA_ASSERT(GAIA_UTIL::addressof(other) != this);

				resize(other.size());
				utils::copy_elements(m_data, other.m_data, other.size());

				return *this;
			}

			GAIA_NODISCARD constexpr sarr_ext& operator=(sarr_ext&& other) noexcept {
				GAIA_ASSERT(GAIA_UTIL::addressof(other) != this);

				resize(other.m_cnt);
				utils::move_elements(m_data, other.m_data, other.size());

				other.m_cnt = size_type(0);

				return *this;
			}

			GAIA_NODISCARD constexpr pointer data() noexcept {
				return (pointer)m_data;
			}

			GAIA_NODISCARD constexpr const_pointer data() const noexcept {
				return (const_pointer)m_data;
			}

			GAIA_NODISCARD constexpr reference operator[](size_type pos) noexcept {
				GAIA_ASSERT(pos < size());
				return (reference)m_data[pos];
			}

			GAIA_NODISCARD constexpr const_reference operator[](size_type pos) const noexcept {
				GAIA_ASSERT(pos < size());
				return (const_reference)m_data[pos];
			}

			constexpr void push_back(const T& arg) noexcept {
				GAIA_ASSERT(size() < N);
				m_data[m_cnt++] = arg;
			}

			constexpr void push_back(T&& arg) noexcept {
				GAIA_ASSERT(size() < N);
				m_data[m_cnt++] = std::forward<T>(arg);
			}

			template <typename... Args>
			constexpr reference emplace_back(Args&&... args) {
				GAIA_ASSERT(size() < N);
				reference ref = m_data[m_cnt++];
				::new (&ref) T(std::forward<Args>(args)...);
				return ref;
			}

			constexpr void pop_back() noexcept {
				GAIA_ASSERT(!empty());
				reference ref = m_data[--m_cnt];
				ref.~T();
			}

			GAIA_NODISCARD constexpr iterator erase(iterator pos) noexcept {
				GAIA_ASSERT(pos.m_ptr >= &m_data[0] && pos.m_ptr < &m_data[N - 1]);

				const auto idxSrc = (size_type)GAIA_UTIL::distance(pos, begin());
				const auto idxDst = size() - 1;

				utils::shift_elements_left(&m_data[idxSrc], idxDst - idxSrc);
				--m_cnt;

				return iterator((T*)m_data + idxSrc);
			}

			GAIA_NODISCARD constexpr const_iterator erase(const_iterator pos) noexcept {
				GAIA_ASSERT(pos.m_ptr >= &m_data[0] && pos.m_ptr < &m_data[N - 1]);

				const auto idxSrc = (size_type)GAIA_UTIL::distance(pos, begin());
				const auto idxDst = size() - 1;

				utils::shift_elements_left(&m_data[idxSrc], idxDst - idxSrc);
				--m_cnt;

				return iterator((const T*)m_data + idxSrc);
			}

			GAIA_NODISCARD constexpr iterator erase(iterator first, iterator last) noexcept {
				GAIA_ASSERT(first.m_cnt >= 0 && first.m_cnt < size());
				GAIA_ASSERT(last.m_cnt >= 0 && last.m_cnt < size());
				GAIA_ASSERT(last.m_cnt >= first.m_cnt);

				const auto size = last.m_cnt - first.m_cnt;
				utils::shift_elements_left(&m_data[first.cnt], size);
				m_cnt -= size;

				return {(T*)m_data + size_type(last.m_cnt)};
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

			GAIA_NODISCARD constexpr reference front() noexcept {
				return *begin();
			}

			GAIA_NODISCARD constexpr const_reference front() const noexcept {
				return *begin();
			}

			GAIA_NODISCARD constexpr reference back() noexcept {
				return N != 0U ? *(end() - 1) : *end();
			}

			GAIA_NODISCARD constexpr const_reference back() const noexcept {
				return N != 0U ? *(end() - 1) : *end();
			}

			GAIA_NODISCARD constexpr iterator begin() const noexcept {
				return {(T*)m_data};
			}

			GAIA_NODISCARD constexpr const_iterator cbegin() const noexcept {
				return {(const T*)m_data};
			}

			GAIA_NODISCARD iterator end() const noexcept {
				return {(T*)m_data + size()};
			}

			GAIA_NODISCARD const_iterator cend() const noexcept {
				return {(const T*)m_data + size()};
			}

			GAIA_NODISCARD bool operator==(const sarr_ext& other) const {
				if (m_cnt != other.m_cnt)
					return false;
				const size_type n = size();
				for (size_type i = 0; i < n; ++i)
					if (!(m_data[i] == other.m_data[i]))
						return false;
				return true;
			}
		};

		namespace detail {
			template <typename T, std::size_t N, std::size_t... I>
			constexpr sarr_ext<std::remove_cv_t<T>, N> to_sarray_impl(T (&a)[N], std::index_sequence<I...> /*no_name*/) {
				return {{a[I]...}};
			}
		} // namespace detail

		template <typename T, std::size_t N>
		constexpr sarr_ext<std::remove_cv_t<T>, N> to_sarray(T (&a)[N]) {
			return detail::to_sarray_impl(a, std::make_index_sequence<N>{});
		}

	} // namespace containers

} // namespace gaia

namespace std {
	template <typename T, size_t N>
	struct tuple_size<gaia::containers::sarr_ext<T, N>>: std::integral_constant<std::size_t, N> {};

	template <size_t I, typename T, size_t N>
	struct tuple_element<I, gaia::containers::sarr_ext<T, N>> {
		using type = T;
	};
} // namespace std

namespace gaia {
	namespace containers {
		template <typename T, auto N>
		using sarray_ext = containers::sarr_ext<T, N>;
	} // namespace containers
} // namespace gaia

#include <cstddef>
#include <type_traits>
#include <utility>

namespace gaia {
	namespace containers {
		//! Array of elements of type \tparam T with fixed size and capacity \tparam N allocated on stack.
		//! Interface compatiblity with std::array where it matters.
		template <typename T, uint32_t N>
		class sringbuffer {
		public:
			static_assert(N > 1);

			using iterator_category = GAIA_UTIL::random_access_iterator_tag;
			using value_type = T;
			using reference = T&;
			using const_reference = const T&;
			using pointer = T*;
			using const_pointer = T*;
			using difference_type = std::ptrdiff_t;
			using size_type = decltype(N);

			static constexpr size_type extent = N;

			size_type m_tail{};
			size_type m_size{};
			T m_data[N];

			sringbuffer() noexcept = default;

			template <typename InputIt>
			sringbuffer(InputIt first, InputIt last) {
				const auto count = (size_type)GAIA_UTIL::distance(first, last);
				GAIA_ASSERT(count <= max_size());
				if (count < 1)
					return;

				m_size = count;
				m_tail = 0;

				if constexpr (std::is_pointer_v<InputIt>) {
					for (size_type i = 0; i < count; ++i)
						m_data[i] = first[i];
				} else if constexpr (std::is_same_v<
																 typename InputIt::iterator_category, GAIA_UTIL::random_access_iterator_tag>) {
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
				GAIA_ASSERT(GAIA_UTIL::addressof(other) != this);

				utils::move_elements(m_data, other.m_data, other.size());

				other.m_tail = size_type(0);
				other.m_size = size_type(0);
			}

			GAIA_NODISCARD sringbuffer& operator=(std::initializer_list<T> il) {
				*this = sringbuffer(il.begin(), il.end());
				return *this;
			}

			GAIA_NODISCARD constexpr sringbuffer& operator=(const sringbuffer& other) {
				GAIA_ASSERT(GAIA_UTIL::addressof(other) != this);

				utils::copy_elements(m_data, other.m_data, other.size());

				m_tail = other.m_tail;
				m_size = other.m_size;

				return *this;
			}

			GAIA_NODISCARD constexpr sringbuffer& operator=(sringbuffer&& other) noexcept {
				GAIA_ASSERT(GAIA_UTIL::addressof(other) != this);

				utils::move_elements(m_data, other.m_data, other.size());

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
			template <typename T, std::size_t N, std::size_t... I>
			constexpr sringbuffer<std::remove_cv_t<T>, N>
			to_sringbuffer_impl(T (&a)[N], std::index_sequence<I...> /*no_name*/) {
				return {{a[I]...}};
			}
		} // namespace detail

		template <typename T, std::size_t N>
		constexpr sringbuffer<std::remove_cv_t<T>, N> to_sringbuffer(T (&a)[N]) {
			return detail::to_sringbuffer_impl(a, std::make_index_sequence<N>{});
		}

		template <typename T, typename... U>
		sringbuffer(T, U...) -> sringbuffer<T, 1 + sizeof...(U)>;

	} // namespace containers

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

#include <cinttypes>
#include <type_traits>

namespace gaia {
	namespace containers {
		struct ImplicitListItem {
			//! Allocated items: Index in the list.
			//! Deleted items: Index of the next deleted item in the list.
			uint32_t idx;
			//! Generation ID
			uint32_t gen;
		};

		template <typename TListItem, typename TItemHandle>
		struct ImplicitList {
			using internal_storage = containers::darr<TListItem>;
			using iterator = typename internal_storage::iterator;
			using const_iterator = typename internal_storage::const_iterator;

			using iterator_category = typename internal_storage::iterator::iterator_category;
			using value_type = TListItem;
			using reference = TListItem&;
			using const_reference = const TListItem&;
			using pointer = TListItem*;
			using const_pointer = TListItem*;
			using difference_type = std::ptrdiff_t;
			using size_type = uint32_t;

			static_assert(std::is_base_of<ImplicitListItem, TListItem>::value);
			//! Implicit list items
			darr<TListItem> m_items;
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

			GAIA_NODISCARD const_iterator cbegin() const noexcept {
				return {(const_pointer)m_items.data()};
			}

			GAIA_NODISCARD iterator end() const noexcept {
				return {(pointer)m_items.data() + size()};
			}

			GAIA_NODISCARD const_iterator cend() const noexcept {
				return {(const_pointer)m_items.data() + size()};
			}

			//! Allocates a new item in the list
			//! \return Handle to the new item
			GAIA_NODISCARD TItemHandle allocate() {
				if GAIA_UNLIKELY (m_freeItems == 0U) {
					// We don't want to go out of range for new item
					const auto itemCnt = (size_type)m_items.size();
					GAIA_ASSERT(itemCnt < TItemHandle::IdMask && "Trying to allocate too many items!");

					m_items.push_back({{itemCnt, 0U}});
					return {itemCnt, 0U};
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
			TListItem& release(TItemHandle handle) {
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

				auto freeEntities = m_freeItems;
				auto nextFreeEntity = m_nextFreeIdx;
				while (freeEntities > 0) {
					GAIA_ASSERT(nextFreeEntity < m_items.size() && "Item recycle list broken!");

					nextFreeEntity = m_items[nextFreeEntity].idx;
					--freeEntities;
				}

				// At this point the index of the last item in list should
				// point to -1 because that's the tail of our implicit list.
				GAIA_ASSERT(nextFreeEntity == TItemHandle::IdMask);
			}
		};
	} // namespace containers
} // namespace gaia

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

		struct JobContainer: containers::ImplicitListItem {
			uint32_t dependencyIdx;
			JobInternalState state;
			std::function<void()> func;
		};

		struct JobDependency: containers::ImplicitListItem {
			uint32_t dependencyIdxNext;
			JobHandle dependsOn;
		};

		using DepHandle = JobHandle;

		class JobManager {
			std::mutex m_jobsLock;
			//! Implicit list of jobs
			containers::ImplicitList<JobContainer, JobHandle> m_jobs;

			std::mutex m_depsLock;
			//! List of job dependencies
			containers::ImplicitList<JobDependency, DepHandle> m_deps;

		public:
			//! Cleans up any job allocations and dependicies associated with \param jobHandle
			void Complete(JobHandle jobHandle) {
				// We need to release any dependencies related to this job
				auto& job = m_jobs[jobHandle.id()];

				if (job.state == JobInternalState::Released)
					return;

				uint32_t depIdx = job.dependencyIdx;
				while (depIdx != (uint32_t)-1) {
					auto& dep = m_deps[depIdx];
					const uint32_t depIdxNext = dep.dependencyIdxNext;
					Complete(dep.dependsOn);
					DeallocateDependency(DepHandle{depIdx, 0});
					depIdx = depIdxNext;
				}

				// Deallocate the job itself
				DeallocateJob(jobHandle);
			}

			//! Allocates a new job container identified by a unique JobHandle.
			//! \return JobHandle
			//! \warning Must be used from the main thread.
			GAIA_NODISCARD JobHandle AllocateJob(const Job& job) {
				std::scoped_lock<std::mutex> lock(m_jobsLock);
				auto handle = m_jobs.allocate();
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
			void DeallocateJob(JobHandle jobHandle) {
				// No need to lock. Called from the main thread only when the job has finished already.
				// --> std::scoped_lock<std::mutex> lock(m_jobsLock);
				auto& job = m_jobs.release(jobHandle);
				job.state = JobInternalState::Released;
			}

			//! Allocates a new dependency identified by a unique DepHandle.
			//! \return DepHandle
			//! \warning Must be used from the main thread.
			GAIA_NODISCARD DepHandle AllocateDependency() {
				return m_deps.allocate();
			}

			//! Invalidates \param depHandle by resetting its index in the dependency pool.
			//! Everytime a dependency is deallocated its generation is increased by one.
			//! \warning Must be used from the main thread.
			void DeallocateDependency(DepHandle depHandle) {
				m_deps.release(depHandle);
			}

			//! Resets the job pool.
			void Reset() {
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

			void Run(JobHandle jobHandle) {
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
			GAIA_NODISCARD bool HandleDependencies(JobHandle jobHandle) {
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
						if (!IsDone(dep.dependsOn)) {
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
			void AddDependency(JobHandle jobHandle, JobHandle dependsOn) {
				std::scoped_lock<std::mutex> lockJobs(m_jobsLock);
				auto& job = m_jobs[jobHandle.id()];

#if GAIA_ASSERT_ENABLED
				GAIA_ASSERT(jobHandle != dependsOn);
				GAIA_ASSERT(!IsBusy(jobHandle));
				GAIA_ASSERT(!IsBusy(dependsOn));
#endif

				{
					GAIA_PROF_SCOPE(JobManager::AddDep);
					std::scoped_lock<std::mutex> lockDeps(m_depsLock);

					auto depHandle = AllocateDependency();
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
			void AddDependencies(JobHandle jobHandle, std::span<const JobHandle> dependsOnSpan) {
				if (dependsOnSpan.empty())
					return;

				auto& job = m_jobs[jobHandle.id()];

#if GAIA_ASSERT_ENABLED
				GAIA_ASSERT(!IsBusy(jobHandle));
				for (auto dependsOn: dependsOnSpan) {
					GAIA_ASSERT(jobHandle != dependsOn);
					GAIA_ASSERT(!IsBusy(dependsOn));
				}
#endif

				GAIA_PROF_SCOPE(JobManager::AddDeps);
				std::scoped_lock<std::mutex> lockJobs(m_jobsLock);
				{
					std::scoped_lock<std::mutex> lockDeps(m_depsLock);

					for (uint32_t i = 0; i < dependsOnSpan.size(); ++i) {
						auto depHandle = AllocateDependency();
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

			void Submit(JobHandle jobHandle) {
				auto& job = m_jobs[jobHandle.id()];
				GAIA_ASSERT(job.state < JobInternalState::Submitted);
				job.state = JobInternalState::Submitted;
			}

			void ReSubmit(JobHandle jobHandle) {
				auto& job = m_jobs[jobHandle.id()];
				GAIA_ASSERT(job.state <= JobInternalState::Submitted);
				job.state = JobInternalState::Submitted;
			}

			GAIA_NODISCARD bool IsBusy(JobHandle jobHandle) const {
				const auto& job = m_jobs[jobHandle.id()];
				return ((uint32_t)job.state & (uint32_t)JobInternalState::Busy) != 0;
			}

			GAIA_NODISCARD bool IsDone(JobHandle jobHandle) const {
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
			containers::sringbuffer<JobHandle, N> m_buffer;
#else
			containers::sarray<JobHandle, N> m_buffer;
			std::atomic_uint32_t m_bottom{};
			std::atomic_uint32_t m_top{};
#endif

		public:
			//! Tries adding a job to the queue. FIFO.
			//! \return True if the job was added. False otherwise (e.g. maximum capacity has been reached).
			GAIA_NODISCARD bool TryPush(JobHandle jobHandle) {
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
			GAIA_NODISCARD bool TryPop(JobHandle& jobHandle) {
				GAIA_PROF_SCOPE(JobQueue::TryPop);

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
			GAIA_NODISCARD bool TrySteal(JobHandle& jobHandle) {
				GAIA_PROF_SCOPE(JobQueue::TrySteal);

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
			std::thread::id m_mainThreadId;

			//! List of worker threads
			containers::sarr_ext<std::thread, 64> m_workers;
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
				uint32_t workerCount = CalculateThreadCount(0);
				if (workerCount > m_workers.max_size())
					workerCount = (uint32_t)m_workers.max_size();

				m_workers.resize(workerCount);

				m_mainThreadId = std::this_thread::get_id();

				for (uint32_t i = 0; i < workerCount; ++i) {
					m_workers[i] = std::thread([this, i]() {
						// Set the worker thread name.
						// Needs to be called from inside the thread because some platforms
						// can change the name only when run from the specific thread.
						SetThreadName(i);

						// Process jobs
						ThreadLoop();
					});

					// Stick each thread to a specific CPU core
					SetThreadAffinity(i);
				}
			}

			ThreadPool(ThreadPool&&) = delete;
			ThreadPool(const ThreadPool&) = delete;
			ThreadPool& operator=(ThreadPool&&) = delete;
			ThreadPool& operator=(const ThreadPool&) = delete;

		public:
			static ThreadPool& Get() {
				static ThreadPool threadPool;
				return threadPool;
			}

			GAIA_NODISCARD uint32_t GetWorkersCount() const {
				return (uint32_t)m_workers.size();
			}

			~ThreadPool() {
				Reset();
			}

			//! Makes \param jobHandle depend on \param dependsOn.
			//! This means \param jobHandle will run only after \param dependsOn finishes.
			//! \warning Must be used from the main thread.
			//! \warning Needs to be called before any of the listed jobs are scheduled.
			void AddDependency(JobHandle jobHandle, JobHandle dependsOn) {
				m_jobManager.AddDependency(jobHandle, dependsOn);
			}

			//! Makes \param jobHandle depend on the jobs listed in \param dependsOnSpan.
			//! This means \param jobHandle will run only after all \param dependsOnSpan jobs finish.
			//! \warning Must be used from the main thread.
			//! \warning Needs to be called before any of the listed jobs are scheduled.
			void AddDependencies(JobHandle jobHandle, std::span<const JobHandle> dependsOnSpan) {
				m_jobManager.AddDependencies(jobHandle, dependsOnSpan);
			}

			//! Creates a job system job from \param job.
			//! \warning Must be used from the main thread.
			//! \return Job handle of the scheduled job.
			JobHandle CreateJob(const Job& job) {
				GAIA_ASSERT(IsMainThread());

				// Don't add new jobs once stop was requested
				if GAIA_UNLIKELY (m_stop)
					return JobNull;

				++m_jobsPending;

				return m_jobManager.AllocateJob(job);
			}

			//! Pushes \param jobHandle into the internal queue so worker threads
			//! can pick it up and execute it.
			//! If there are more jobs than the queue can handle it puts the calling
			//! thread to sleep until workers consume enough jobs.
			//! \warning Once submited, dependencies can't be modified for this job.
			void Submit(JobHandle jobHandle) {
				m_jobManager.Submit(jobHandle);

				// Try pushing a new job until it we succeed.
				// The thread is put to sleep if pushing the jobs fails.
				while (!m_jobQueue.TryPush(jobHandle))
					Poll();

				// Wake some worker thread
				m_cv.notify_one();
			}

		private:
			//! Resubmits \param jobHandle into the internal queue so worker threads
			//! can pick it up and execute it.
			//! If there are more jobs than the queue can handle it puts the calling
			//! thread to sleep until workers consume enough jobs.
			//! \warning Internal usage only. Only worker theads can decide to resubmit.
			void ReSubmit(JobHandle jobHandle) {
				m_jobManager.ReSubmit(jobHandle);

				// Try pushing a new job until it we succeed.
				// The thread is put to sleep if pushing the jobs fails.
				while (!m_jobQueue.TryPush(jobHandle))
					Poll();

				// Wake some worker thread
				m_cv.notify_one();
			}

		public:
			//! Schedules a job to run on a worker thread.
			//! \param job Job descriptor
			//! \warning Must be used from the main thread.
			//! \warning Dependencies can't be modified for this job.
			//! \return Job handle of the scheduled job.
			JobHandle Schedule(const Job& job) {
				JobHandle jobHandle = CreateJob(job);
				Submit(jobHandle);
				return jobHandle;
			}

			//! Schedules a job to run on a worker thread.
			//! \param job Job descriptor
			//! \param dependsOn Job we depend on
			//! \warning Must be used from the main thread.
			//! \warning Dependencies can't be modified for this job.
			//! \return Job handle of the scheduled job.
			JobHandle Schedule(const Job& job, JobHandle dependsOn) {
				JobHandle jobHandle = CreateJob(job);
				AddDependency(jobHandle, dependsOn);
				Submit(jobHandle);
				return jobHandle;
			}

			//! Schedules a job to run on a worker thread.
			//! \param job Job descriptor
			//! \param dependsOnSpan Jobs we depend on
			//! \warning Must be used from the main thread.
			//! \warning Dependencies can't be modified for this job.
			//! \return Job handle of the scheduled job.
			JobHandle Schedule(const Job& job, std::span<const JobHandle> dependsOnSpan) {
				JobHandle jobHandle = CreateJob(job);
				AddDependencies(jobHandle, dependsOnSpan);
				Submit(jobHandle);
				return jobHandle;
			}

			//! Schedules a job to run on worker threads in parallel.
			//! \param job Job descriptor
			//! \param itemsToProcess Total number of work items
			//! \param groupSize Group size per created job. If zero the job system decides the group size.
			//! \warning Must be used from the main thread.
			//! \warning Dependencies can't be modified for this job.
			//! \return Job handle of the scheduled batch of jobs.
			JobHandle ScheduleParallel(const JobParallel& job, uint32_t itemsToProcess, uint32_t groupSize) {
				GAIA_ASSERT(IsMainThread());

				// Empty data set are considered wrong inputs
				GAIA_ASSERT(itemsToProcess != 0);
				if (itemsToProcess == 0)
					return JobNull;

				// Don't add new jobs once stop was requested
				if GAIA_UNLIKELY (m_stop)
					return JobNull;

				const uint32_t workerCount = (uint32_t)m_workers.size();

				// No group size was given, make a guess based on the set size
				if (groupSize == 0)
					groupSize = (itemsToProcess + workerCount - 1) / workerCount;

				const auto jobs = (itemsToProcess + groupSize - 1) / groupSize;
				// Internal jobs + 1 for the groupHandle
				m_jobsPending += (jobs + 1U);

				JobHandle groupHandle = m_jobManager.AllocateJob({});

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

					JobHandle jobHandle = m_jobManager.AllocateJob({groupJobFunc});
					AddDependency(groupHandle, jobHandle);
					Submit(jobHandle);
				}

				Submit(groupHandle);
				return groupHandle;
			}

			//! Wait for the job to finish.
			//! \param jobHandle Job handle
			//! \warning Must be used from the main thread.
			void Complete(JobHandle jobHandle) {
				GAIA_ASSERT(IsMainThread());

				while (m_jobManager.IsBusy(jobHandle))
					Poll();

				GAIA_ASSERT(!m_jobManager.IsBusy(jobHandle));
				m_jobManager.Complete(jobHandle);
			}

			//! Wait for all jobs to finish.
			//! \warning Must be used from the main thread.
			void CompleteAll() {
				GAIA_ASSERT(IsMainThread());

				while (IsBusy())
					PollAll();

				GAIA_ASSERT(m_jobsPending == 0);
				m_jobManager.Reset();
			}

		private:
			void SetThreadAffinity(uint32_t threadID) {
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
				GAIA_ASSERT(mask > 0);
				if (mask <= 0)
					GAIA_LOG_W("Issue setting thread affinity for worker thread %u!", threadID);
#elif GAIA_PLATFORM_APPLE
				auto nativeHandle = (pthread_t)m_workers[threadID].native_handle();

				mach_port_t mach_thread = pthread_mach_thread_np(nativeHandle);
				thread_affinity_policy_data_t policy_data = {(int)threadID};
				auto ret = thread_policy_set(
						mach_thread, THREAD_AFFINITY_POLICY, (thread_policy_t)&policy_data, THREAD_AFFINITY_POLICY_COUNT);
				GAIA_ASSERT(ret != 0);
				if (ret == 0)
					GAIA_LOG_W("Issue setting thread affinity for worker thread %u!", threadID);
#elif GAIA_PLATFORM_LINUX || GAIA_PLATFORM_FREEBSD
				auto nativeHandle = (pthread_t)m_workers[threadID].native_handle();

				cpu_set_t cpuset;
				CPU_ZERO(&cpuset);
				CPU_SET(threadID, &cpuset);

				auto ret = pthread_setaffinity_np(nativeHandle, sizeof(cpuset), &cpuset);
				GAIA_ASSERT(ret == 0);
				if (ret != 0)
					GAIA_LOG_W("Issue setting thread affinity for worker thread %u!", threadID);

				ret = pthread_getaffinity_np(nativeHandle, sizeof(cpuset), &cpuset);
				GAIA_ASSERT(ret == 0);
				if (ret != 0)
					GAIA_LOG_W("Thread affinity could not be set for worker thread %u!", threadID);
#endif
			}

			void SetThreadName(uint32_t threadID) {
#if GAIA_PLATFORM_WINDOWS
				auto nativeHandle = (HANDLE)m_workers[threadID].native_handle();

				wchar_t threadName[10]{};
				swprintf_s(threadName, L"worker_%u", threadID);
				auto hr = SetThreadDescription(nativeHandle, threadName);
				GAIA_ASSERT(SUCCEEDED(hr));
				if (FAILED(hr))
					GAIA_LOG_W("Issue setting worker thread name!");
#elif GAIA_PLATFORM_APPLE
				char threadName[10]{};
				snprintf(threadName, 10, "worker_%u", threadID);
				auto ret = pthread_setname_np(threadName);
				GAIA_ASSERT(ret == 0);
				if (ret != 0)
					GAIA_LOG_W("Issue setting name for worker thread %u!", threadID);
#elif GAIA_PLATFORM_LINUX || GAIA_PLATFORM_FREEBSD
				auto nativeHandle = (pthread_t)m_workers[threadID].native_handle();

				char threadName[10]{};
				snprintf(threadName, 10, "worker_%u", threadID);
				auto ret = pthread_setname_np(nativeHandle, threadName);
				GAIA_ASSERT(ret == 0);
				if (ret != 0)
					GAIA_LOG_W("Issue setting name for worker thread %u!", threadID);
#endif
			}

			GAIA_NODISCARD bool IsMainThread() const {
				return std::this_thread::get_id() == m_mainThreadId;
			}

			GAIA_NODISCARD static uint32_t CalculateThreadCount(uint32_t threadsWanted) {
				// Make sure a reasonable amount of threads is used
				if (threadsWanted == 0) {
					// Subtract one (the main thread)
					threadsWanted = std::thread::hardware_concurrency() - 1;
					if (threadsWanted <= 0)
						threadsWanted = 1;
				}
				return threadsWanted;
			}

			void ThreadLoop() {
				while (!m_stop) {
					JobHandle jobHandle;

					if (!m_jobQueue.TryPop(jobHandle)) {
						std::unique_lock<std::mutex> lock(m_cvLock);
						m_cv.wait(lock);
						continue;
					}

					GAIA_ASSERT(m_jobsPending > 0);

					// Make sure we can execute the job.
					// If it has dependencies which were not completed we need to reschedule
					// and come back to it later.
					if (!m_jobManager.HandleDependencies(jobHandle)) {
						ReSubmit(jobHandle);
						continue;
					}

					m_jobManager.Run(jobHandle);
					--m_jobsPending;
				}
			}

			void Reset() {
				// Request stopping
				m_stop = true;
				// Complete all remaining work
				CompleteAll();
				// Wake up any threads that were put to sleep
				m_cv.notify_all();
				// Join threads with the main one
				for (auto& w: m_workers)
					w.join();
			}

			//! Checks whether workers are busy doing work.
			//!	\return True if any workers are busy doing work.
			GAIA_NODISCARD bool IsBusy() const {
				return m_jobsPending > 0;
			}

			//! Wakes up some worker thread and reschedules the current one.
			void Poll() {
				// Wake some worker thread
				m_cv.notify_one();

				// Allow this thread to be rescheduled
				std::this_thread::yield();
			}

			//! Wakes up all worker threads and reschedules the current one.
			void PollAll() {
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

#include <cinttypes>

namespace gaia {
	namespace ecs {
		namespace component {

			enum ComponentType : uint8_t {
				// General purpose component
				CT_Generic = 0,
				// Chunk component
				CT_Chunk,
				// Number of component types
				CT_Count
			};

			inline const char* const ComponentTypeString[component::ComponentType::CT_Count] = {"Generic", "Chunk"};

			using ComponentId = uint32_t;
			using ComponentLookupHash = utils::direct_hash_key<uint64_t>;
			using ComponentMatcherHash = utils::direct_hash_key<uint64_t>;
			using ComponentIdSpan = std::span<const ComponentId>;

			static constexpr ComponentId ComponentIdBad = (ComponentId)-1;
			static constexpr uint32_t MAX_COMPONENTS_SIZE_BITS = 8;
			static constexpr uint32_t MAX_COMPONENTS_SIZE_IN_BYTES = (1 << MAX_COMPONENTS_SIZE_BITS) - 1;

			//----------------------------------------------------------------------
			// Component type deduction
			//----------------------------------------------------------------------

			namespace detail {
				template <typename T>
				struct ExtractComponentType_Generic {
					using Type = typename std::decay_t<typename std::remove_pointer_t<T>>;
					using TypeOriginal = T;
				};
				template <typename T>
				struct ExtractComponentType_NonGeneric {
					using Type = typename T::TType;
					using TypeOriginal = typename T::TTypeOriginal;
				};

				template <typename T, typename = void>
				struct IsGenericComponent_Internal: std::true_type {};
				template <typename T>
				struct IsGenericComponent_Internal<T, decltype((void)T::TComponentType, void())>: std::false_type {};

				template <typename T>
				struct IsComponentSizeValid_Internal: std::bool_constant<sizeof(T) < MAX_COMPONENTS_SIZE_IN_BYTES> {};

				template <typename T>
				struct IsComponentTypeValid_Internal:
						std::bool_constant<
								// SoA types need to be trivial. No restrictions otherwise.
								(!utils::is_soa_layout_v<T> || std::is_trivially_copyable_v<T>)> {};
			} // namespace detail

			template <typename T>
			inline constexpr bool IsGenericComponent = detail::IsGenericComponent_Internal<T>::value;
			template <typename T>
			inline constexpr bool IsComponentSizeValid = detail::IsComponentSizeValid_Internal<T>::value;
			template <typename T>
			inline constexpr bool IsComponentTypeValid = detail::IsComponentTypeValid_Internal<T>::value;

			template <typename T>
			using DeduceComponent = std::conditional_t<
					IsGenericComponent<T>, typename detail::ExtractComponentType_Generic<T>,
					typename detail::ExtractComponentType_NonGeneric<T>>;

			//! Returns the component id for \tparam T
			//! \return Component id
			template <typename T>
			GAIA_NODISCARD inline ComponentId GetComponentId() {
				using U = typename DeduceComponent<T>::Type;
				return utils::type_info::id<U>();
			}

			//! Returns the component id for \tparam T
			//! \return Component id
			template <typename T>
			GAIA_NODISCARD inline constexpr ComponentType GetComponentType() {
				if constexpr (IsGenericComponent<T>)
					return ComponentType::CT_Generic;
				else
					return ComponentType::CT_Chunk;
			}

			template <typename T>
			struct IsReadOnlyType:
					std::bool_constant<
							std::is_const_v<std::remove_reference_t<std::remove_pointer_t<T>>> ||
							(!std::is_pointer<T>::value && !std::is_reference<T>::value)> {};

			//----------------------------------------------------------------------
			// Component verification
			//----------------------------------------------------------------------

			template <typename T>
			constexpr void VerifyComponent() {
				using U = typename DeduceComponent<T>::Type;
				// Make sure we only use this for "raw" types
				static_assert(!std::is_const_v<U>);
				static_assert(!std::is_pointer_v<U>);
				static_assert(!std::is_reference_v<U>);
				static_assert(!std::is_volatile_v<U>);
				static_assert(IsComponentSizeValid<U>, "MAX_COMPONENTS_SIZE_IN_BYTES in bytes is exceeded");
				static_assert(IsComponentTypeValid<U>, "Component type restrictions not met");
			}

			//----------------------------------------------------------------------
			// Component hash operations
			//----------------------------------------------------------------------

			namespace detail {
				template <typename T>
				constexpr uint64_t CalculateMatcherHash() noexcept {
					return (uint64_t(1) << (utils::type_info::hash<T>() % uint64_t(63)));
				}
			} // namespace detail

			template <typename = void, typename...>
			constexpr ComponentMatcherHash CalculateMatcherHash() noexcept;

			template <typename T, typename... Rest>
			GAIA_NODISCARD constexpr ComponentMatcherHash CalculateMatcherHash() noexcept {
				if constexpr (sizeof...(Rest) == 0)
					return {detail::CalculateMatcherHash<T>()};
				else
					return {utils::combine_or(detail::CalculateMatcherHash<T>(), detail::CalculateMatcherHash<Rest>()...)};
			}

			template <>
			GAIA_NODISCARD constexpr ComponentMatcherHash CalculateMatcherHash() noexcept {
				return {0};
			}

			//-----------------------------------------------------------------------------------

			template <typename Container>
			GAIA_NODISCARD constexpr ComponentLookupHash CalculateLookupHash(Container arr) noexcept {
				constexpr auto arrSize = arr.size();
				if constexpr (arrSize == 0) {
					return {0};
				} else {
					ComponentLookupHash::Type hash = arr[0];
					utils::for_each<arrSize - 1>([&hash, &arr](auto i) {
						hash = utils::hash_combine(hash, arr[i + 1]);
					});
					return {hash};
				}
			}

			template <typename = void, typename...>
			constexpr ComponentLookupHash CalculateLookupHash() noexcept;

			template <typename T, typename... Rest>
			GAIA_NODISCARD constexpr ComponentLookupHash CalculateLookupHash() noexcept {
				if constexpr (sizeof...(Rest) == 0)
					return {utils::type_info::hash<T>()};
				else
					return {utils::hash_combine(utils::type_info::hash<T>(), utils::type_info::hash<Rest>()...)};
			}

			template <>
			GAIA_NODISCARD constexpr ComponentLookupHash CalculateLookupHash() noexcept {
				return {0};
			}
		} // namespace component

		template <typename T>
		struct AsChunk {
			using TType = typename std::decay_t<typename std::remove_pointer_t<T>>;
			using TTypeOriginal = T;
			static constexpr component::ComponentType TComponentType = component::ComponentType::CT_Chunk;
		};
	} // namespace ecs
} // namespace gaia

namespace gaia {
	namespace ecs {
		namespace archetype {
			constexpr uint32_t MAX_COMPONENTS_PER_ARCHETYPE_BITS = 5U;
			//! Maximum number of components on archetype
			constexpr uint32_t MAX_COMPONENTS_PER_ARCHETYPE = 1U << MAX_COMPONENTS_PER_ARCHETYPE_BITS;

			class Archetype;

			using ArchetypeId = uint32_t;
			using LookupHash = utils::direct_hash_key<uint64_t>;
			using ArchetypeList = containers::darray<Archetype*>;
			using ComponentIdArray = containers::sarray_ext<component::ComponentId, MAX_COMPONENTS_PER_ARCHETYPE>;
			// uint16_t can fit at most 65535 items therefore MemoryBlockSize can't be set to a value biggen than that
			using ChunkComponentOffset = uint16_t;
			using ComponentOffsetArray = containers::sarray_ext<ChunkComponentOffset, MAX_COMPONENTS_PER_ARCHETYPE>;

			static constexpr ArchetypeId ArchetypeIdBad = (ArchetypeId)-1;

			GAIA_NODISCARD inline constexpr bool VerifyArchetypeComponentCount(uint32_t count) {
				return count <= MAX_COMPONENTS_PER_ARCHETYPE;
			}
		} // namespace archetype
	} // namespace ecs
} // namespace gaia

#include <cinttypes>

#include <cinttypes>
#include <type_traits>

#include <cinttypes>
#include <tuple>
#include <type_traits>

namespace gaia {
	namespace ecs {
		namespace component {
			struct ComponentDesc final {
				using FuncCtor = void(void*, size_t);
				using FuncDtor = void(void*, size_t);
				using FuncCopy = void(void*, void*);
				using FuncMove = void(void*, void*);

				//! Component name
				std::span<const char> name;
				//! Function to call when the component needs to be constructed
				FuncCtor* ctor = nullptr;
				//! Function to call when the component needs to be move constructed
				FuncMove* ctor_move = nullptr;
				//! Function to call when the component needs to be copy constructed
				FuncCopy* ctor_copy = nullptr;
				//! Function to call when the component needs to be destroyed
				FuncDtor* dtor = nullptr;
				//! Function to call when the component needs to be copied
				FuncCopy* copy = nullptr;
				//! Fucntion to call when the component needs to be moved
				FuncMove* move = nullptr;
				//! Unique component identifier
				ComponentId componentId = ComponentIdBad;

				//! Various component properties
				struct {
					//! Component alignment
					uint32_t alig: MAX_COMPONENTS_SIZE_BITS;
					//! Component size
					uint32_t size: MAX_COMPONENTS_SIZE_BITS;
					//! SOA variables. If > 0 the component is laid out in SoA style
					uint32_t soa: utils::StructToTupleMaxTypesBits;
				} properties{};

				void CtorFrom(void* pSrc, void* pDst) const {
					if (ctor_move != nullptr)
						ctor_move(pSrc, pDst);
					else if (ctor_copy != nullptr)
						ctor_copy(pSrc, pDst);
					else
						memmove(pDst, (const void*)pSrc, properties.size);
				}

				void Move(void* pSrc, void* pDst) const {
					if (move != nullptr)
						move(pSrc, pDst);
					else
						Copy(pSrc, pDst);
				}

				void Copy(void* pSrc, void* pDst) const {
					if (copy != nullptr)
						copy(pSrc, pDst);
					else
						memmove(pDst, (const void*)pSrc, properties.size);
				}

				void Dtor(void* pSrc) const {
					if (dtor != nullptr)
						dtor(pSrc, 1);
				}

				template <typename T>
				GAIA_NODISCARD static constexpr ComponentDesc Calculate() {
					using U = typename DeduceComponent<T>::Type;

					ComponentDesc info{};
					info.name = utils::type_info::name<U>();
					info.componentId = GetComponentId<T>();

					if constexpr (!std::is_empty_v<U>) {
						info.properties.alig = utils::auto_view_policy<U>::Alignment;
						info.properties.size = (uint32_t)sizeof(U);

						if constexpr (utils::is_soa_layout_v<U>) {
							using TTuple = decltype(utils::struct_to_tuple(T{}));
							info.properties.soa = (uint32_t)std::tuple_size<TTuple>::value;
						} else {
							info.properties.soa = 0U;

							// Custom construction
							if constexpr (!std::is_trivially_constructible_v<U>) {
								info.ctor = [](void* ptr, size_t cnt) {
									auto* first = (U*)ptr;
									auto* last = (U*)ptr + cnt;
									for (; first != last; ++first)
										(void)new (first) U();
								};
							}

							// Custom destruction
							if constexpr (!std::is_trivially_destructible_v<U>) {
								info.dtor = [](void* ptr, size_t cnt) {
									auto first = (U*)ptr;
									auto last = (U*)ptr + cnt;
									for (; first != last; ++first)
										first->~U();
								};
							}

							// Copyability
							if (!std::is_trivially_copyable_v<U>) {
								if constexpr (std::is_copy_assignable_v<U>) {
									info.copy = [](void* from, void* to) {
										auto* src = (U*)from;
										auto* dst = (U*)to;
										*dst = *src;
									};
									info.ctor_copy = [](void* from, void* to) {
										auto* src = (U*)from;
										auto* dst = (U*)to;
										new (dst) U();
										*dst = *src;
									};
								} else if constexpr (std::is_copy_constructible_v<U>) {
									info.copy = [](void* from, void* to) {
										auto* src = (U*)from;
										auto* dst = (U*)to;
										*dst = U(*src);
									};
									info.ctor_copy = [](void* from, void* to) {
										auto* src = (U*)from;
										auto* dst = (U*)to;
										(void)new (dst) U(std::move(*src));
									};
								}
							}

							// Movability
							if constexpr (!std::is_trivially_move_assignable_v<U> && std::is_move_assignable_v<U>) {
								info.move = [](void* from, void* to) {
									auto* src = (U*)from;
									auto* dst = (U*)to;
									*dst = std::move(*src);
								};
								info.ctor_move = [](void* from, void* to) {
									auto* src = (U*)from;
									auto* dst = (U*)to;
									new (dst) U();
									*dst = std::move(*src);
								};
							} else if constexpr (!std::is_trivially_move_constructible_v<U> && std::is_move_constructible_v<U>) {
								info.move = [](void* from, void* to) {
									auto* src = (U*)from;
									auto* dst = (U*)to;
									*dst = U(std::move(*src));
								};
								info.ctor_move = [](void* from, void* to) {
									auto* src = (U*)from;
									auto* dst = (U*)to;
									(void)new (dst) U(std::move(*src));
								};
							}
						}
					}

					return info;
				}

				template <typename T>
				GAIA_NODISCARD static ComponentDesc Create() {
					using U = std::decay_t<T>;
					return ComponentDesc::Calculate<U>();
				}
			};
		} // namespace component
	} // namespace ecs
} // namespace gaia

REGISTER_HASH_TYPE(gaia::ecs::component::ComponentLookupHash)
REGISTER_HASH_TYPE(gaia::ecs::component::ComponentMatcherHash)

#include <type_traits>

namespace gaia {
	namespace ecs {
		namespace component {
			struct ComponentInfo final {
				//! Complex hash used for look-ups
				ComponentLookupHash lookupHash;
				//! Simple hash used for matching component
				ComponentMatcherHash matcherHash;
				//! Unique component identifier
				ComponentId componentId;

				GAIA_NODISCARD bool operator==(const ComponentInfo& other) const {
					return lookupHash == other.lookupHash && componentId == other.componentId;
				}
				GAIA_NODISCARD bool operator!=(const ComponentInfo& other) const {
					return lookupHash != other.lookupHash || componentId != other.componentId;
				}
				GAIA_NODISCARD bool operator<(const ComponentInfo& other) const {
					return componentId < other.componentId;
				}

				template <typename T>
				GAIA_NODISCARD static constexpr ComponentInfo Calculate() {
					using U = typename DeduceComponent<T>::Type;

					ComponentInfo info{};
					info.lookupHash = {utils::type_info::hash<U>()};
					info.matcherHash = CalculateMatcherHash<U>();
					info.componentId = GetComponentId<T>();

					return info;
				}

				template <typename T>
				static const ComponentInfo* Create() {
					using U = std::decay_t<T>;
					return new ComponentInfo{Calculate<U>()};
				}
			};
		} // namespace component
	} // namespace ecs
} // namespace gaia

namespace gaia {
	namespace ecs {
		class ComponentCache {
			containers::darray<const component::ComponentInfo*> m_infoByIndex;
			containers::darray<component::ComponentDesc> m_descByIndex;

			ComponentCache() {
				// Reserve enough storage space for most use-cases
				constexpr uint32_t DefaultComponentCacheSize = 2048;
				m_infoByIndex.reserve(DefaultComponentCacheSize);
				m_descByIndex.reserve(DefaultComponentCacheSize);
			}

		public:
			static ComponentCache& Get() {
				static ComponentCache cache;
				return cache;
			}

			~ComponentCache() {
				ClearRegisteredInfoCache();
			}

			ComponentCache(ComponentCache&&) = delete;
			ComponentCache(const ComponentCache&) = delete;
			ComponentCache& operator=(ComponentCache&&) = delete;
			ComponentCache& operator=(const ComponentCache&) = delete;

			//! Registers the component info for \tparam T. If it already exists it is returned.
			//! \return Component info
			template <typename T>
			GAIA_NODISCARD const component::ComponentInfo& GetOrCreateComponentInfo() {
				using U = typename component::DeduceComponent<T>::Type;
				const auto componentId = component::GetComponentId<T>();

				auto createInfo = [&]() -> const component::ComponentInfo& {
					const auto* pInfo = component::ComponentInfo::Create<U>();
					m_infoByIndex[componentId] = pInfo;
					m_descByIndex[componentId] = component::ComponentDesc::Create<U>();
					return *pInfo;
				};

				if GAIA_UNLIKELY (componentId >= m_infoByIndex.size()) {
					const auto oldSize = m_infoByIndex.size();
					const auto newSize = componentId + 1U;

					// Increase the capacity by multiples of CapacityIncreaseSize
					constexpr uint32_t CapacityIncreaseSize = 128;
					const auto newCapacity = (newSize / CapacityIncreaseSize) * CapacityIncreaseSize + CapacityIncreaseSize;
					m_infoByIndex.reserve(newCapacity);

					// Update the size
					m_infoByIndex.resize(newSize);
					m_descByIndex.resize(newSize);

					// Make sure that unused memory is initialized to nullptr
					for (size_t i = oldSize; i < newSize; ++i)
						m_infoByIndex[i] = nullptr;

					return createInfo();
				}

				if GAIA_UNLIKELY (m_infoByIndex[componentId] == nullptr) {
					return createInfo();
				}

				return *m_infoByIndex[componentId];
			}

			//! Returns the component info given the \param componentId.
			//! \warning It is expected the component info with a given component id exists! Undefined behavior otherwise.
			//! \return Component info
			GAIA_NODISCARD const component::ComponentInfo&
			GetComponentInfo(component::ComponentId componentId) const noexcept {
				GAIA_ASSERT(componentId < m_infoByIndex.size());
				const auto* pInfo = m_infoByIndex[componentId];
				GAIA_ASSERT(pInfo != nullptr);
				return *pInfo;
			}

			//! Returns the component creation info given the \param componentId.
			//! \warning It is expected the component info with a given component id exists! Undefined behavior otherwise.
			//! \return Component info
			GAIA_NODISCARD const component::ComponentDesc&
			GetComponentDesc(component::ComponentId componentId) const noexcept {
				GAIA_ASSERT(componentId < m_descByIndex.size());
				return m_descByIndex[componentId];
			}

			//! Returns the component info for \tparam T.
			//! \warning It is expected the component already exists! Undefined behavior otherwise.
			//! \return Component info
			template <typename T>
			GAIA_NODISCARD const component::ComponentInfo& GetComponentInfo() const noexcept {
				const auto componentId = component::GetComponentId<T>();
				return GetComponentInfo(componentId);
			}

			void Diag() const {
				const auto registeredTypes = (uint32_t)m_descByIndex.size();
				GAIA_LOG_N("Registered infos: %u", registeredTypes);

				for (const auto& desc: m_descByIndex)
					GAIA_LOG_N("  id:%010u, %.*s", desc.componentId, (uint32_t)desc.name.size(), desc.name.data());
			}

		private:
			void ClearRegisteredInfoCache() {
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
		namespace archetype {
			class ArchetypeGraph {
				struct ArchetypeGraphEdge {
					ArchetypeId archetypeId;
				};

				//! Map of edges in the archetype graph when adding components
				containers::map<component::ComponentId, ArchetypeGraphEdge> m_edgesAdd[component::ComponentType::CT_Count];
				//! Map of edges in the archetype graph when removing components
				containers::map<component::ComponentId, ArchetypeGraphEdge> m_edgesDel[component::ComponentType::CT_Count];

			public:
				//! Creates an edge in the graph leading to the target archetype \param archetypeId via component \param
				//! componentId of type \param componentType.
				void AddGraphEdgeRight(
						component::ComponentType componentType, component::ComponentId componentId, ArchetypeId archetypeId) {
					[[maybe_unused]] const auto ret =
							m_edgesAdd[componentType].try_emplace({componentId}, ArchetypeGraphEdge{archetypeId});
					GAIA_ASSERT(ret.second);
				}

				//! Creates an edge in the graph leading to the target archetype \param archetypeId via component \param
				//! componentId of type \param componentType.
				void AddGraphEdgeLeft(
						component::ComponentType componentType, component::ComponentId componentId, ArchetypeId archetypeId) {
					[[maybe_unused]] const auto ret =
							m_edgesDel[componentType].try_emplace({componentId}, ArchetypeGraphEdge{archetypeId});
					GAIA_ASSERT(ret.second);
				}

				//! Checks if the graph edge for component type \param componentType contains the component \param componentId.
				//! \return Archetype id of the target archetype if the edge is found. ArchetypeIdBad otherwise.
				GAIA_NODISCARD ArchetypeId
				FindGraphEdgeRight(component::ComponentType componentType, const component::ComponentId componentId) const {
					const auto& edges = m_edgesAdd[componentType];
					const auto it = edges.find({componentId});
					return it != edges.end() ? it->second.archetypeId : ArchetypeIdBad;
				}

				//! Checks if the graph edge for component type \param componentType contains the component \param componentId.
				//! \return Archetype id of the target archetype if the edge is found. ArchetypeIdBad otherwise.
				GAIA_NODISCARD ArchetypeId
				FindGraphEdgeLeft(component::ComponentType componentType, const component::ComponentId componentId) const {
					const auto& edges = m_edgesDel[componentType];
					const auto it = edges.find({componentId});
					return it != edges.end() ? it->second.archetypeId : ArchetypeIdBad;
				}

				void Diag() const {
					const auto& cc = ComponentCache::Get();

					// Add edges (movement towards the leafs)
					{
						const auto& edgesG = m_edgesAdd[component::ComponentType::CT_Generic];
						const auto& edgesC = m_edgesAdd[component::ComponentType::CT_Chunk];
						const auto edgeCount = (uint32_t)(edgesG.size() + edgesC.size());
						if (edgeCount > 0) {
							GAIA_LOG_N("  Add edges - count:%u", edgeCount);

							if (!edgesG.empty()) {
								GAIA_LOG_N("    Generic - count:%u", (uint32_t)edgesG.size());
								for (const auto& edge: edgesG) {
									const auto& info = cc.GetComponentInfo(edge.first);
									const auto& infoCreate = cc.GetComponentDesc(info.componentId);
									GAIA_LOG_N(
											"      %.*s (--> Archetype ID:%u)", (uint32_t)infoCreate.name.size(), infoCreate.name.data(),
											edge.second.archetypeId);
								}
							}

							if (!edgesC.empty()) {
								GAIA_LOG_N("    Chunk - count:%u", (uint32_t)edgesC.size());
								for (const auto& edge: edgesC) {
									const auto& info = cc.GetComponentInfo(edge.first);
									const auto& infoCreate = cc.GetComponentDesc(info.componentId);
									GAIA_LOG_N(
											"      %.*s (--> Archetype ID:%u)", (uint32_t)infoCreate.name.size(), infoCreate.name.data(),
											edge.second.archetypeId);
								}
							}
						}
					}

					// Delete edges (movement towards the root)
					{
						const auto& edgesG = m_edgesDel[component::ComponentType::CT_Generic];
						const auto& edgesC = m_edgesDel[component::ComponentType::CT_Chunk];
						const auto edgeCount = (uint32_t)(edgesG.size() + edgesC.size());
						if (edgeCount > 0) {
							GAIA_LOG_N("  Del edges - count:%u", edgeCount);

							if (!edgesG.empty()) {
								GAIA_LOG_N("    Generic - count:%u", (uint32_t)edgesG.size());
								for (const auto& edge: edgesG) {
									const auto& info = cc.GetComponentInfo(edge.first);
									const auto& infoCreate = cc.GetComponentDesc(info.componentId);
									GAIA_LOG_N(
											"      %.*s (--> Archetype ID:%u)", (uint32_t)infoCreate.name.size(), infoCreate.name.data(),
											edge.second.archetypeId);
								}
							}

							if (!edgesC.empty()) {
								GAIA_LOG_N("    Chunk - count:%u", (uint32_t)edgesC.size());
								for (const auto& edge: edgesC) {
									const auto& info = cc.GetComponentInfo(edge.first);
									const auto& infoCreate = cc.GetComponentDesc(info.componentId);
									GAIA_LOG_N(
											"      %.*s (--> Archetype ID:%u)", (uint32_t)infoCreate.name.size(), infoCreate.name.data(),
											edge.second.archetypeId);
								}
							}
						}
					}
				}
			};
		} // namespace archetype
	} // namespace ecs
} // namespace gaia

#include <cinttypes>
#include <cstdint>
#include <type_traits>
#include <utility>

#include <cstdint>

#include <cinttypes>

namespace gaia {
	namespace ecs {
		GAIA_NODISCARD inline bool DidVersionChange(uint32_t changeVersion, uint32_t requiredVersion) {
			// When a system runs for the first time, everything is considered changed.
			if GAIA_UNLIKELY (requiredVersion == 0U)
				return true;

			// Supporting wrap-around for version numbers. ChangeVersion must be
			// bigger than requiredVersion (never detect change of something the
			// system itself changed).
			return (int)(changeVersion - requiredVersion) > 0;
		}

		inline void UpdateVersion(uint32_t& version) {
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
		static constexpr uint32_t MemoryBlockSize = 16384;
		//! Unusable area at the beggining of the allocated block designated for special pruposes
		static constexpr uint32_t MemoryBlockUsableOffset = sizeof(size_t);
		//! Effectively usable size of the allocated block of memory
		static constexpr uint32_t ChunkMemorySize = MemoryBlockSize - MemoryBlockUsableOffset;

		struct ChunkAllocatorStats final {
			//! Total allocated memory
			uint64_t AllocatedMemory;
			//! Memory actively used
			uint64_t UsedMemory;
			//! Number of allocated pages
			uint32_t NumPages;
			//! Number of free pages
			uint32_t NumFreePages;
		};

		class ChunkAllocator;

		namespace detail {

			/*!
			Allocator for ECS Chunks. Memory is organized in pages of chunks.
			*/
			class ChunkAllocatorImpl {
				friend class gaia::ecs::ChunkAllocator;

				struct MemoryBlock {
					using MemoryBlockType = uint8_t;

					//! Active block : Index of the block within the page.
					//! Passive block: Index of the next free block in the implicit list.
					MemoryBlockType idx;
				};

				struct MemoryPage {
					static constexpr uint16_t NBlocks = 64;
					static constexpr uint32_t Size = NBlocks * MemoryBlockSize;
					static constexpr MemoryBlock::MemoryBlockType InvalidBlockId = (MemoryBlock::MemoryBlockType)-1;
					static constexpr uint32_t MemoryLockTypeSizeInBits =
							(uint32_t)utils::as_bits(sizeof(MemoryBlock::MemoryBlockType));
					static_assert((uint32_t)NBlocks < (1 << MemoryLockTypeSizeInBits));
					using iterator = containers::darray<MemoryPage*>::iterator;

					//! Pointer to data managed by page
					void* m_data;
					//! Implicit list of blocks
					MemoryBlock m_blocks[NBlocks]{};
					//! Index in the list of pages
					uint32_t m_pageIdx;
					//! Number of blocks in the block array
					MemoryBlock::MemoryBlockType m_blockCnt;
					//! Number of used blocks out of NBlocks
					MemoryBlock::MemoryBlockType m_usedBlocks;
					//! Index of the next block to recycle
					MemoryBlock::MemoryBlockType m_nextFreeBlock;
					//! Number of blocks to recycle
					MemoryBlock::MemoryBlockType m_freeBlocks;

					MemoryPage(void* ptr):
							m_data(ptr), m_pageIdx(0), m_blockCnt(0), m_usedBlocks(0), m_nextFreeBlock(0), m_freeBlocks(0) {}

					GAIA_NODISCARD void* AllocChunk() {
						auto StoreChunkAddress = [&](size_t index) {
							// Encode info about chunk's page in the memory block.
							// The actual pointer returned is offset by UsableOffset bytes
							uint8_t* pMemoryBlock = (uint8_t*)m_data + index * MemoryBlockSize;
							*utils::unaligned_pointer<uintptr_t>{pMemoryBlock} = (uintptr_t)this;
							return (void*)(pMemoryBlock + MemoryBlockUsableOffset);
						};

						if (m_freeBlocks == 0U) {
							// We don't want to go out of range for new blocks
							GAIA_ASSERT(!IsFull() && "Trying to allocate too many blocks!");

							++m_usedBlocks;

							const size_t index = m_blockCnt;
							GAIA_ASSERT(index < NBlocks);
							m_blocks[index].idx = (MemoryBlock::MemoryBlockType)index;
							++m_blockCnt;

							return StoreChunkAddress(index);
						}

						GAIA_ASSERT(m_nextFreeBlock < m_blockCnt && "Block allocator recycle containers::list broken!");

						++m_usedBlocks;
						--m_freeBlocks;

						const size_t index = m_nextFreeBlock;
						m_nextFreeBlock = m_blocks[m_nextFreeBlock].idx;

						return StoreChunkAddress(index);
					}

					void FreeChunk(void* pChunk) {
						GAIA_ASSERT(m_freeBlocks <= NBlocks);

						// Offset the chunk memory so we get the real block address
						const auto* pMemoryBlock = (uint8_t*)pChunk - MemoryBlockUsableOffset;

						const auto blckAddr = (uintptr_t)pMemoryBlock;
						const auto dataAddr = (uintptr_t)m_data;
						GAIA_ASSERT(blckAddr >= dataAddr && blckAddr < dataAddr + MemoryPage::Size);
						MemoryBlock block = {MemoryBlock::MemoryBlockType((blckAddr - dataAddr) / MemoryBlockSize)};

						auto& blockContainer = m_blocks[block.idx];

						// Update our implicit containers::list
						if (m_freeBlocks == 0U) {
							blockContainer.idx = InvalidBlockId;
							m_nextFreeBlock = block.idx;
						} else {
							blockContainer.idx = m_nextFreeBlock;
							m_nextFreeBlock = block.idx;
						}

						++m_freeBlocks;
						--m_usedBlocks;
					}

					GAIA_NODISCARD uint32_t GetUsedBlocks() const {
						return m_usedBlocks;
					}
					GAIA_NODISCARD bool IsFull() const {
						return m_usedBlocks == NBlocks;
					}
					GAIA_NODISCARD bool IsEmpty() const {
						return m_usedBlocks == 0;
					}
				};

				//! List of available pages
				//! Note, this currently only contains at most 1 item
				containers::darray<MemoryPage*> m_pagesFree;
				//! List of full pages
				containers::darray<MemoryPage*> m_pagesFull;
				//! When true, destruction has been requested
				bool m_isDone = false;

				ChunkAllocatorImpl() noexcept = default;

			public:
				~ChunkAllocatorImpl() = default;

				ChunkAllocatorImpl(ChunkAllocatorImpl&& world) = delete;
				ChunkAllocatorImpl(const ChunkAllocatorImpl& world) = delete;
				ChunkAllocatorImpl& operator=(ChunkAllocatorImpl&&) = delete;
				ChunkAllocatorImpl& operator=(const ChunkAllocatorImpl&) = delete;

				/*!
				Allocates memory
				*/
				void* Allocate() {
					void* pChunk = nullptr;

					if (m_pagesFree.empty()) {
						// Initial allocation
						auto* pPage = AllocPage();
						pPage->m_pageIdx = 0;
						pChunk = pPage->AllocChunk();
						m_pagesFree.push_back(pPage);
					} else {
						auto* pPage = m_pagesFree[0];
						GAIA_ASSERT(!pPage->IsFull());
						// Allocate a new chunk of memory
						pChunk = pPage->AllocChunk();

						// Handle full pages
						if (pPage->IsFull()) {
							// Remove the page from the open list and update the swapped page's pointer
							utils::erase_fast(m_pagesFree, 0);
							if (!m_pagesFree.empty())
								m_pagesFree[0]->m_pageIdx = 0;

							// Move our page to the full list
							pPage->m_pageIdx = (uint32_t)m_pagesFull.size();
							m_pagesFull.push_back(pPage);
						}
					}

#if GAIA_ECS_CHUNK_ALLOCATOR_CLEAN_MEMORY_WITH_GARBAGE
					// Fill allocated memory with garbage.
					// This way we always know if we treat the memory correctly.
					constexpr uint32_t AllocMemDefValue = 0x7fcdf00dU;
					constexpr uint32_t AllocSpanSize = (uint32_t)((ChunkMemorySize + 3) / sizeof(uint32_t));
					std::span<uint32_t, AllocSpanSize> s((uint32_t*)pChunk, AllocSpanSize);
					for (auto& val: s)
						val = AllocMemDefValue;
#endif

					return pChunk;
				}

				/*!
				Releases memory allocated for pointer
				*/
				void Release(void* pChunk) {
					// Decode the page from the address
					uintptr_t pageAddr = *(uintptr_t*)((uint8_t*)pChunk - MemoryBlockUsableOffset);
					auto* pPage = (MemoryPage*)pageAddr;

#if GAIA_ECS_CHUNK_ALLOCATOR_CLEAN_MEMORY_WITH_GARBAGE
					// Fill freed memory with garbage.
					// This way we always know if we treat the memory correctly.
					constexpr uint32_t FreedMemDefValue = 0xfeeefeeeU;
					constexpr uint32_t FreedSpanSize = (uint32_t)((ChunkMemorySize + 3) / sizeof(uint32_t));
					std::span<uint32_t, FreedSpanSize> s((uint32_t*)pChunk, FreedSpanSize);
					for (auto& val: s)
						val = FreedMemDefValue;
#endif

					const bool pageFull = pPage->IsFull();

#if GAIA_DEBUG
					if (pageFull) {
						[[maybe_unused]] auto it = utils::find_if(m_pagesFull.begin(), m_pagesFull.end(), [&](auto page) {
							return page == pPage;
						});
						GAIA_ASSERT(
								it != m_pagesFull.end() && "ChunkAllocator delete couldn't find the memory page expected "
																					 "in the full pages containers::list");
					} else {
						[[maybe_unused]] auto it = utils::find_if(m_pagesFree.begin(), m_pagesFree.end(), [&](auto page) {
							return page == pPage;
						});
						GAIA_ASSERT(
								it != m_pagesFree.end() && "ChunkAllocator delete couldn't find memory page expected in "
																					 "the free pages containers::list");
					}
#endif

					// Update lists
					if (pageFull) {
						// Our page is no longer full. Remove it from the list and update the swapped page's pointer
						m_pagesFull.back()->m_pageIdx = pPage->m_pageIdx;
						utils::erase_fast(m_pagesFull, pPage->m_pageIdx);

						// Move our page to the open list
						pPage->m_pageIdx = (uint32_t)m_pagesFree.size();
						m_pagesFree.push_back(pPage);
					}

					// Free the chunk
					pPage->FreeChunk(pChunk);

					if (m_isDone) {
						// Remove the page right away
						if (pPage->IsEmpty()) {
							GAIA_ASSERT(!m_pagesFree.empty());
							m_pagesFree.back()->m_pageIdx = pPage->m_pageIdx;
							utils::erase_fast(m_pagesFree, pPage->m_pageIdx);
						}

						// When there is nothing left, delete the allocator
						if (m_pagesFree.empty() && m_pagesFull.empty())
							delete this;
					}
				}

				/*!
				Returns allocator statistics
				*/
				ChunkAllocatorStats GetStats() const {
					ChunkAllocatorStats stats{};
					stats.NumPages = (uint32_t)m_pagesFree.size() + (uint32_t)m_pagesFull.size();
					stats.NumFreePages = (uint32_t)m_pagesFree.size();
					stats.AllocatedMemory = stats.NumPages * (size_t)MemoryPage::Size;
					stats.UsedMemory = m_pagesFull.size() * (size_t)MemoryPage::Size;
					for (auto* page: m_pagesFree)
						stats.UsedMemory += page->GetUsedBlocks() * (size_t)MemoryBlockSize;
					return stats;
				}

				/*!
				Flushes unused memory
				*/
				void Flush() {
					for (size_t i = 0; i < m_pagesFree.size();) {
						auto* pPage = m_pagesFree[i];

						// Skip non-empty pages
						if (!pPage->IsEmpty()) {
							++i;
							continue;
						}

						utils::erase_fast(m_pagesFree, i);
						FreePage(pPage);
						if (!m_pagesFree.empty())
							m_pagesFree[i]->m_pageIdx = (uint32_t)i;
					}
				}

				/*!
				Performs diagnostics of the memory used.
				*/
				void Diag() const {
					ChunkAllocatorStats memstats = GetStats();
					GAIA_LOG_N("ChunkAllocator stats");
					GAIA_LOG_N("  Allocated: %" PRIu64 " B", memstats.AllocatedMemory);
					GAIA_LOG_N("  Used: %" PRIu64 " B", memstats.AllocatedMemory - memstats.UsedMemory);
					GAIA_LOG_N("  Overhead: %" PRIu64 " B", memstats.UsedMemory);
					GAIA_LOG_N("  Utilization: %.1f%%", 100.0 * ((double)memstats.UsedMemory / (double)memstats.AllocatedMemory));
					GAIA_LOG_N("  Pages: %u", memstats.NumPages);
					GAIA_LOG_N("  Free pages: %u", memstats.NumFreePages);
				}

			private:
				static MemoryPage* AllocPage() {
					auto* pPageData = utils::mem_alloc_alig(MemoryPage::Size, 16);
					return new MemoryPage(pPageData);
				}

				static void FreePage(MemoryPage* page) {
					utils::mem_free_alig(page->m_data);
					delete page;
				}

				void Done() {
					m_isDone = true;
				}
			};
		} // namespace detail

		//! Manager of ECS memory for Chunks.
		//! IMPORTANT:
		//! Gaia-ECS is a header-only library which means we want to avoid using global
		//! static variables because they would get copied to each translation units.
		//! At the same time the goal is for user not to see any memory allocator used
		//! by the library. Therefore, the only solution is a static variable with local
		//! scope.
		//!
		//! Being a static variable with local scope which means the allocator is guaranteed
		//! to be younger than its caller. Because static variables are released in the reverse
		//! order in which they are created, if used with a static World it would mean we first
		//! release the allocator memory and only then proceed with the world itself. As a result,
		//! in its destructor the world would try to access memory which has already been released.
		//!
		//! Instead, we let this object alocate the real allocator on the heap and once
		//! ChunkAllocator's destructor is called we tell the real one it should destroy
		//! itself. This way there are no memory leaks or access-after-freed issues on app
		//! exit reported.
		class ChunkAllocator final {
			detail::ChunkAllocatorImpl* m_allocator = new detail::ChunkAllocatorImpl();

			ChunkAllocator() = default;

		public:
			static detail::ChunkAllocatorImpl& Get() noexcept {
				static ChunkAllocator staticAllocator;
				return *staticAllocator.m_allocator;
			}

			ChunkAllocator(ChunkAllocator&& world) = delete;
			ChunkAllocator(const ChunkAllocator& world) = delete;
			ChunkAllocator& operator=(ChunkAllocator&&) = delete;
			ChunkAllocator& operator=(const ChunkAllocator&) = delete;

			~ChunkAllocator() {
				Get().Done();
			}
		};

	} // namespace ecs
} // namespace gaia

#include <cinttypes>
#include <cstdint>

namespace gaia {
	namespace ecs {
		namespace archetype {
			struct ChunkHeaderOffsets {
				//! Byte at which the first component id is located
				ChunkComponentOffset firstByte_Versions[component::ComponentType::CT_Count]{};
				//! Byte at which the first component id is located
				ChunkComponentOffset firstByte_ComponentIds[component::ComponentType::CT_Count]{};
				//! Byte at which the first component offset is located
				ChunkComponentOffset firstByte_ComponentOffsets[component::ComponentType::CT_Count]{};
				//! Byte at which the first entity is located
				ChunkComponentOffset firstByte_EntityData{};
			};

			struct ChunkHeader final {
			public:
				static constexpr uint16_t CHUNK_LIFESPAN_BITS = 4;
				//! Number of ticks before empty chunks are removed
				static constexpr uint16_t MAX_CHUNK_LIFESPAN = (1 << CHUNK_LIFESPAN_BITS) - 1;
				using DisabledEntityMask = containers::bitset<512>;

				//! Archetype the chunk belongs to
				ArchetypeId archetypeId{};
				//! Number of items enabled in the chunk.
				uint16_t count{};
				//! Capacity (copied from the owner archetype).
				uint16_t capacity{};
				//! Chunk index in its archetype list
				uint16_t index{};
				//! Once removal is requested and it hits 0 the chunk is removed.
				uint16_t lifespanCountdown: CHUNK_LIFESPAN_BITS;
				//! If true this chunk stores disabled entities
				uint16_t hasDisabledEntities : 1;
				//! Updated when chunks are being iterated. Used to inform of structural changes when they shouldn't happen.
				uint16_t structuralChangesLocked : 4;
				//! True if there's a component that requires custom construction
				uint16_t hasAnyCustomGenericCtor : 1;
				//! True if there's a component that requires custom construction
				uint16_t hasAnyCustomChunkCtor : 1;
				//! True if there's a component that requires custom destruction
				uint16_t hasAnyCustomGenericDtor : 1;
				//! True if there's a component that requires custom destruction
				uint16_t hasAnyCustomChunkDtor : 1;
				//! Version of the world (stable pointer to parent world's world version)
				uint32_t& worldVersion;
				//! Number of components on the archetype
				uint8_t componentCount[component::ComponentType::CT_Count]{};
				//! Offsets to various parts of data inside chunk
				ChunkHeaderOffsets offsets;
				//! Mask of disabled entities
				DisabledEntityMask disabledEntityMask;

				ChunkHeader(const ChunkHeaderOffsets& offs, uint32_t& version):
						lifespanCountdown(0), hasDisabledEntities(0), structuralChangesLocked(0), hasAnyCustomGenericCtor(0),
						hasAnyCustomChunkCtor(0), hasAnyCustomGenericDtor(0), hasAnyCustomChunkDtor(0),
						worldVersion(version), offsets(offs) {
					// Make sure the alignment is right
					GAIA_ASSERT(uintptr_t(this) % (sizeof(size_t)) == 0);
				}
			};
		} // namespace archetype
	} // namespace ecs
} // namespace gaia

namespace gaia {
	namespace ecs {
		namespace component {
			//! Updates the provided component matcher hash based on the provided component id
			//! \param matcherHash Initial matcher hash
			//! \param componentId Component id
			GAIA_NODISCARD inline void
			CalculateMatcherHash(ComponentMatcherHash& matcherHash, component::ComponentId componentId) noexcept {
				const auto& cc = ComponentCache::Get();
				const auto componentHash = cc.GetComponentInfo(componentId).matcherHash.hash;
				matcherHash.hash = utils::combine_or(matcherHash.hash, componentHash);
			}

			//! Calculates a component matcher hash from the provided component ids
			//! \param componentIds Span of component ids
			//! \return Component matcher hash
			GAIA_NODISCARD inline ComponentMatcherHash CalculateMatcherHash(ComponentIdSpan componentIds) noexcept {
				const auto infosSize = componentIds.size();
				if (infosSize == 0)
					return {0};

				const auto& cc = ComponentCache::Get();
				ComponentMatcherHash::Type hash = cc.GetComponentInfo(componentIds[0]).matcherHash.hash;
				for (size_t i = 1; i < infosSize; ++i)
					hash = utils::combine_or(hash, cc.GetComponentInfo(componentIds[i]).matcherHash.hash);
				return {hash};
			}

			//! Calculates a component lookup hash from the provided component ids
			//! \param componentIds Span of component ids
			//! \return Component lookup hash
			GAIA_NODISCARD inline ComponentLookupHash CalculateLookupHash(ComponentIdSpan componentIds) noexcept {
				const auto infosSize = componentIds.size();
				if (infosSize == 0)
					return {0};

				const auto& cc = ComponentCache::Get();
				ComponentLookupHash::Type hash = cc.GetComponentInfo(componentIds[0]).lookupHash.hash;
				for (size_t i = 1; i < infosSize; ++i)
					hash = utils::hash_combine(hash, cc.GetComponentInfo(componentIds[i]).lookupHash.hash);
				return {hash};
			}

			using SortComponentCond = utils::is_smaller<ComponentId>;

			//! Sorts component ids
			template <typename Container>
			inline void SortComponents(Container& c) {
				utils::sort(c, SortComponentCond{});
			}
		} // namespace component
	} // namespace ecs
} // namespace gaia

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

		namespace archetype {
			class Chunk;
		}

		struct EntityContainer: containers::ImplicitListItem {
			//! Chunk the entity currently resides in
			archetype::Chunk* pChunk;
#if !GAIA_64
			uint32_t pChunk_padding;
#endif
		};
	} // namespace ecs
} // namespace gaia

namespace gaia {
	namespace ecs {
		namespace archetype {
			class Chunk final {
			public:
				//! Size of one chunk's data part with components
				static constexpr size_t DATA_SIZE = ChunkMemorySize - sizeof(ChunkHeader);

			private:
				//! Chunk header
				ChunkHeader m_header;
				//! Chunk data layed out as:
				//!			1) ComponentVersions[component::ComponentType::CT_Generic]
				//!			2) ComponentVersions[component::ComponentType::CT_Chunk]
				//!     3) ComponentIds[component::ComponentType::CT_Generic]
				//!			4) ComponentIds[component::ComponentType::CT_Chunk]
				//!			5) ComponentOffsets[component::ComponentType::CT_Generic]
				//!			6) ComponentOffsets[component::ComponentType::CT_Chunk]
				//!			7) Entities
				//!			8) Components
				uint8_t m_data[DATA_SIZE];

				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(26495)

				Chunk(
						uint32_t archetypeId, uint16_t chunkIndex, uint16_t capacity, uint32_t& worldVersion,
						const ChunkHeaderOffsets& headerOffsets):
						m_header(headerOffsets, worldVersion) {
					m_header.archetypeId = archetypeId;
					m_header.index = chunkIndex;
					m_header.capacity = capacity;
				}

				GAIA_MSVC_WARNING_POP()

				void Init(
						const containers::sarray<ComponentIdArray, component::ComponentType::CT_Count>& componentIds,
						const containers::sarray<ComponentOffsetArray, component::ComponentType::CT_Count>& componentOffsets) {
					m_header.componentCount[component::ComponentType::CT_Generic] =
							(uint8_t)componentIds[component::ComponentType::CT_Generic].size();
					m_header.componentCount[component::ComponentType::CT_Chunk] =
							(uint8_t)componentIds[component::ComponentType::CT_Chunk].size();

					const auto& cc = ComponentCache::Get();

					const auto& componentIdsGeneric = componentIds[component::ComponentType::CT_Generic];
					for (const auto componentId: componentIdsGeneric) {
						const auto& desc = cc.GetComponentDesc(componentId);
						m_header.hasAnyCustomGenericCtor |= (desc.ctor != nullptr);
						m_header.hasAnyCustomGenericDtor |= (desc.dtor != nullptr);
					}
					const auto& componentIdsChunk = componentIds[component::ComponentType::CT_Chunk];
					for (const auto componentId: componentIdsChunk) {
						const auto& desc = cc.GetComponentDesc(componentId);
						m_header.hasAnyCustomChunkCtor |= (desc.ctor != nullptr);
						m_header.hasAnyCustomChunkDtor |= (desc.dtor != nullptr);
					}

					// Copy provided component id data to this chunk's data area
					{
						for (size_t i = 0; i < component::ComponentType::CT_Count; ++i) {
							size_t offset = m_header.offsets.firstByte_ComponentIds[i];
							for (const auto componentId: componentIds[i]) {
								utils::unaligned_ref<component::ComponentId>{(void*)&m_data[offset]} = componentId;
								offset += sizeof(component::ComponentId);
							}
						}
					}

					// Copy provided component offset data to this chunk's data area
					{
						for (size_t i = 0; i < component::ComponentType::CT_Count; ++i) {
							size_t offset = m_header.offsets.firstByte_ComponentOffsets[i];
							for (const auto componentOffset: componentOffsets[i]) {
								utils::unaligned_ref<archetype::ChunkComponentOffset>{(void*)&m_data[offset]} = componentOffset;
								offset += sizeof(archetype::ChunkComponentOffset);
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
				GAIA_NODISCARD GAIA_FORCEINLINE auto View_Internal() const {
					using U = typename component::DeduceComponent<T>::Type;
					using UConst = typename std::add_const_t<U>;

					if constexpr (std::is_same_v<U, Entity>) {
						return std::span<UConst>{(UConst*)&GetData(m_header.offsets.firstByte_EntityData), GetEntityCount()};
					} else {
						static_assert(!std::is_empty_v<U>, "Attempting to get value of an empty component");

						const auto componentId = component::GetComponentId<T>();

						if constexpr (component::IsGenericComponent<T>) {
							const auto offset = FindDataOffset(component::ComponentType::CT_Generic, componentId);

							[[maybe_unused]] const auto capacity = GetEntityCapacity();
							[[maybe_unused]] const auto maxOffset = offset + capacity * sizeof(U);
							GAIA_ASSERT(maxOffset <= Chunk::DATA_SIZE);

							return std::span<UConst>{(UConst*)&GetData(offset), GetEntityCount()};
						} else {
							const auto offset = FindDataOffset(component::ComponentType::CT_Chunk, componentId);

							[[maybe_unused]] const auto maxOffset = offset + sizeof(U);
							GAIA_ASSERT(maxOffset <= Chunk::DATA_SIZE);

							return std::span<UConst>{(UConst*)&GetData(offset), 1};
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
				GAIA_NODISCARD GAIA_FORCEINLINE auto ViewRW_Internal() {
					using U = typename component::DeduceComponent<T>::Type;
#if GAIA_COMPILER_MSVC && _MSC_VER <= 1916
					// Workaround for MSVC 2017 bug where it incorrectly evaluates the static assert
					// even in context where it shouldn't.
					// Unfortunatelly, even runtime assert can't be used...
					// GAIA_ASSERT(!std::is_same_v<U, Entity>::value);
#else
					static_assert(!std::is_same_v<U, Entity>);
#endif
					static_assert(!std::is_empty_v<U>, "Attempting to set value of an empty component");

					const auto componentId = component::GetComponentId<T>();
					uint32_t componentIdx = 0;

					if constexpr (component::IsGenericComponent<T>) {
						const auto offset = FindDataOffset(component::ComponentType::CT_Generic, componentId, componentIdx);

						[[maybe_unused]] const auto capacity = GetEntityCapacity();
						[[maybe_unused]] const auto maxOffset = offset + capacity * sizeof(U);
						GAIA_ASSERT(maxOffset <= Chunk::DATA_SIZE);

						if constexpr (WorldVersionUpdateWanted) {
							// Update version number so we know RW access was used on chunk
							this->UpdateWorldVersion(component::ComponentType::CT_Generic, componentIdx);
						}

						return std::span<U>{(U*)&GetData(offset), GetEntityCount()};
					} else {
						const auto offset = FindDataOffset(component::ComponentType::CT_Chunk, componentId, componentIdx);

						[[maybe_unused]] const auto maxOffset = offset + sizeof(U);
						GAIA_ASSERT(maxOffset <= Chunk::DATA_SIZE);

						if constexpr (WorldVersionUpdateWanted) {
							// Update version number so we know RW access was used on chunk
							this->UpdateWorldVersion(component::ComponentType::CT_Chunk, componentIdx);
						}

						return std::span<U>{(U*)&GetData(offset), 1};
					}
				}

				/*!
				Returns the value stored in the component \tparam T on \param index in the chunk.
				\warning It is expected the \param index is valid. Undefined behavior otherwise.
				\warning It is expected the component \tparam T is present. Undefined behavior otherwise.
				\tparam T Component
				\param index Index of entity in the chunk
				\return Value stored in the component.
				*/
				template <typename T>
				GAIA_NODISCARD auto GetComponent_Internal(uint32_t index) const {
					using U = typename component::DeduceComponent<T>::Type;
					using RetValue = decltype(View<T>()[0]);

					GAIA_ASSERT(index < m_header.count);
					if constexpr (sizeof(RetValue) > 8)
						return (const U&)View<T>()[index];
					else
						return View<T>()[index];
				}

				/*!
				Removes the entity at from the chunk and updates the world versions
				*/
				void RemoveLastEntity_Internal() {
					// Should never be called over an empty chunk
					GAIA_ASSERT(HasEntities());
					--m_header.count;
					m_header.disabledEntityMask.set(m_header.count, false);
				}

			public:
				/*!
				Allocates memory for a new chunk.
				\param chunkIndex Index of this chunk within the parent archetype
				\return Newly allocated chunk
				*/
				static Chunk* Create(
						uint32_t archetypeId, uint16_t chunkIndex, uint16_t capacity, uint32_t& worldVersion,
						const ChunkHeaderOffsets& offsets,
						const containers::sarray<ComponentIdArray, component::ComponentType::CT_Count>& componentIds,
						const containers::sarray<ComponentOffsetArray, component::ComponentType::CT_Count>& componentOffsets) {
#if GAIA_ECS_CHUNK_ALLOCATOR
					auto* pChunk = (Chunk*)ChunkAllocator::Get().Allocate();
					new (pChunk) Chunk(archetypeId, chunkIndex, capacity, worldVersion, offsets);
#else
					auto pChunk = new Chunk(archetypeId, chunkIndex, capacity, worldVersion, offsets);
#endif

					pChunk->Init(componentIds, componentOffsets);

					return pChunk;
				}

				/*!
				Releases all memory allocated by \param pChunk.
				\param pChunk Chunk which we want to destroy
				*/
				static void Release(Chunk* pChunk) {
					GAIA_ASSERT(pChunk != nullptr);

					// Call destructors for components that need it
					if (pChunk->HasAnyCustomGenericDestructor())
						pChunk->CallDestructors(component::ComponentType::CT_Generic, 0, pChunk->GetEntityCount());
					if (pChunk->HasAnyCustomChunkDestructor())
						pChunk->CallDestructors(component::ComponentType::CT_Chunk, 0, 1);

#if GAIA_ECS_CHUNK_ALLOCATOR
					pChunk->~Chunk();
					ChunkAllocator::Get().Release(pChunk);
#else
					delete pChunk;
#endif
				}

				/*!
				Remove the last entity from chunk.
				\param chunksToRemove Container of chunks ready for removal
				*/
				void RemoveLastEntity(containers::darray<archetype::Chunk*>& chunksToRemove) {
					GAIA_ASSERT(
							!IsStructuralChangesLocked() && "Entities can't be removed while their chunk is being iterated "
																							"(structural changes are forbidden during this time!)");

					RemoveLastEntity_Internal();

					if (!IsDying() && !HasEntities()) {
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
						PrepareToDie();

						chunksToRemove.push_back(this);
					}
				}

				//! Updates the version numbers for this chunk.
				void UpdateVersions() {
					UpdateVersion(m_header.worldVersion);
					UpdateWorldVersion(component::ComponentType::CT_Generic);
					UpdateWorldVersion(component::ComponentType::CT_Chunk);
				}

				/*!
				Returns a read-only entity or component view.
				\warning If \tparam T is a component it is expected it is present. Undefined behavior otherwise.
				\tparam T Component or Entity
				\return Entity of component view with read-only access
				*/
				template <typename T>
				GAIA_NODISCARD auto View() const {
					using U = typename component::DeduceComponent<T>::Type;

					return utils::auto_view_policy_get<std::add_const_t<U>>{{View_Internal<T>()}};
				}

				/*!
				Returns a mutable entity or component view.
				\warning If \tparam T is a component it is expected it is present. Undefined behavior otherwise.
				\tparam T Component or Entity
				\return Entity or component view with read-write access
				*/
				template <typename T>
				GAIA_NODISCARD auto ViewRW() {
					using U = typename component::DeduceComponent<T>::Type;
					static_assert(!std::is_same_v<U, Entity>);

					return utils::auto_view_policy_set<U>{{ViewRW_Internal<T, true>()}};
				}

				/*!
				Returns a mutable component view.
				Doesn't update the world version when the access is aquired.
				\warning It is expected the component \tparam T is present. Undefined behavior otherwise.
				\tparam T Component
				\return Component view with read-write access
				*/
				template <typename T>
				GAIA_NODISCARD auto ViewRWSilent() {
					using U = typename component::DeduceComponent<T>::Type;
					static_assert(!std::is_same_v<U, Entity>);

					return utils::auto_view_policy_set<U>{{ViewRW_Internal<T, false>()}};
				}

				/*!
				Make \param entity a part of the chunk at the version of the world
				\return Index of the entity within the chunk.
				*/
				GAIA_NODISCARD uint32_t AddEntity(Entity entity) {
					const auto index = m_header.count++;
					SetEntity(index, entity);

					UpdateVersion(m_header.worldVersion);
					UpdateWorldVersion(component::ComponentType::CT_Generic);
					UpdateWorldVersion(component::ComponentType::CT_Chunk);

					return index;
				}

				/*!
				Copies all data associated with \param oldEntity into \param newEntity.
				*/
				static void CopyEntityData(Entity oldEntity, Entity newEntity, std::span<EntityContainer> entities) {
					GAIA_PROF_SCOPE(CopyEntityData);

					auto& oldEntityContainer = entities[oldEntity.id()];
					auto* pOldChunk = oldEntityContainer.pChunk;

					auto& newEntityContainer = entities[newEntity.id()];
					auto* pNewChunk = newEntityContainer.pChunk;

					GAIA_ASSERT(pOldChunk->GetArchetypeId() == pNewChunk->GetArchetypeId());

					const auto& cc = ComponentCache::Get();
					const auto& oldInfos = pOldChunk->GetComponentIdArray(component::ComponentType::CT_Generic);
					const auto& oldOffs = pOldChunk->GetComponentOffsetArray(component::ComponentType::CT_Generic);

					// Copy generic component data from reference entity to our new entity
					for (size_t i = 0; i < oldInfos.size(); i++) {
						const auto& desc = cc.GetComponentDesc(oldInfos[i]);
						if (desc.properties.size == 0U)
							continue;

						const auto offset = oldOffs[i];
						const auto idxSrc = offset + desc.properties.size * (uint32_t)oldEntityContainer.idx;
						const auto idxDst = offset + desc.properties.size * (uint32_t)newEntityContainer.idx;

						GAIA_ASSERT(idxSrc < Chunk::DATA_SIZE);
						GAIA_ASSERT(idxDst < Chunk::DATA_SIZE);

						auto* pSrc = (void*)&pOldChunk->GetData(idxSrc);
						auto* pDst = (void*)&pNewChunk->GetData(idxDst);
						desc.Copy(pSrc, pDst);
					}
				}

				/*!
				Moves all data associated with \param entity into the chunk so that it is stored at \param newEntityIdx.
				*/
				void MoveEntityData(Entity entity, uint32_t newEntityIdx, std::span<EntityContainer> entities) {
					GAIA_PROF_SCOPE(CopyEntityFrom);

					auto& oldEntityContainer = entities[entity.id()];
					auto* pOldChunk = oldEntityContainer.pChunk;

					GAIA_ASSERT(pOldChunk->GetArchetypeId() == GetArchetypeId());

					const auto& cc = ComponentCache::Get();
					const auto& oldInfos = pOldChunk->GetComponentIdArray(component::ComponentType::CT_Generic);
					const auto& oldOffs = pOldChunk->GetComponentOffsetArray(component::ComponentType::CT_Generic);

					// Copy generic component data from reference entity to our new entity
					for (size_t i = 0; i < oldInfos.size(); i++) {
						const auto& desc = cc.GetComponentDesc(oldInfos[i]);
						if (desc.properties.size == 0U)
							continue;

						const auto offset = oldOffs[i];
						const auto idxSrc = offset + desc.properties.size * (uint32_t)oldEntityContainer.idx;
						const auto idxDst = offset + desc.properties.size * newEntityIdx;

						GAIA_ASSERT(idxSrc < Chunk::DATA_SIZE);
						GAIA_ASSERT(idxDst < Chunk::DATA_SIZE);

						auto* pSrc = (void*)&pOldChunk->GetData(idxSrc);
						auto* pDst = (void*)&GetData(idxDst);
						desc.CtorFrom(pSrc, pDst);
					}
				}

				/*!
				Moves all data associated with \param entity into the chunk so that it is stored at index \param newEntityIdx.
				*/
				void MoveForeignEntityData(Entity entity, uint32_t newEntityIdx, std::span<EntityContainer> entities) {
					GAIA_PROF_SCOPE(MoveForeignEntityData);

					auto& oldEntityContainer = entities[entity.id()];
					auto* pOldChunk = oldEntityContainer.pChunk;

					const auto& cc = ComponentCache::Get();

					// Find intersection of the two component lists.
					// We ignore chunk components here because they should't be influenced
					// by entities moving around.
					const auto& oldInfos = pOldChunk->GetComponentIdArray(component::ComponentType::CT_Generic);
					const auto& oldOffs = pOldChunk->GetComponentOffsetArray(component::ComponentType::CT_Generic);
					const auto& newInfos = GetComponentIdArray(component::ComponentType::CT_Generic);
					const auto& newOffs = GetComponentOffsetArray(component::ComponentType::CT_Generic);

					// Arrays are sorted so we can do linear intersection lookup
					{
						size_t i = 0;
						size_t j = 0;

						auto moveData = [&](const component::ComponentDesc& desc) {
							if (desc.properties.size == 0U)
								return;

							// Let's move all type data from oldEntity to newEntity
							const auto idxSrc = oldOffs[i] + desc.properties.size * (uint32_t)oldEntityContainer.idx;
							const auto idxDst = newOffs[j] + desc.properties.size * newEntityIdx;

							GAIA_ASSERT(idxSrc < Chunk::DATA_SIZE);
							GAIA_ASSERT(idxDst < Chunk::DATA_SIZE);

							auto* pSrc = (void*)&pOldChunk->GetData(idxSrc);
							auto* pDst = (void*)&GetData(idxDst);
							desc.CtorFrom(pSrc, pDst);
						};

						while (i < oldInfos.size() && j < newInfos.size()) {
							const auto& descOld = cc.GetComponentDesc(oldInfos[i]);
							const auto& descNew = cc.GetComponentDesc(newInfos[j]);

							if (&descOld == &descNew) {
								moveData(descOld);
								++i;
								++j;
							} else if (component::SortComponentCond{}.operator()(descOld.componentId, descNew.componentId))
								++i;
							else
								++j;
						}
					}
				}

				void SwapEntitiesInsideChunkAndDeleteOld(uint32_t index, std::span<EntityContainer> entities) {
					// If there are at least two entities inside and it's not already the
					// last one let's swap our entity with the last one in the chunk.
					if GAIA_LIKELY (m_header.count > 1 && m_header.count != index + 1) {
						GAIA_PROF_SCOPE(SwapEntitiesInsideChunkAndDeleteOld);

						// Swap data at index with the last one
						const auto entity = GetEntity(m_header.count - 1);
						SetEntity(index, entity);

						const auto& cc = ComponentCache::Get();
						const auto& componentIds = GetComponentIdArray(component::ComponentType::CT_Generic);
						const auto& componentOffsets = GetComponentOffsetArray(component::ComponentType::CT_Generic);

						for (size_t i = 0; i < componentIds.size(); i++) {
							const auto& desc = cc.GetComponentDesc(componentIds[i]);
							if (desc.properties.size == 0U)
								continue;

							const auto offset = componentOffsets[i];
							const auto idxSrc = offset + index * desc.properties.size;
							const auto idxDst = offset + (m_header.count - 1U) * desc.properties.size;

							GAIA_ASSERT(idxSrc < Chunk::DATA_SIZE);
							GAIA_ASSERT(idxDst < Chunk::DATA_SIZE);
							GAIA_ASSERT(idxSrc != idxDst);

							auto* pSrc = (void*)&m_data[idxSrc];
							auto* pDst = (void*)&m_data[idxDst];
							desc.Move(pSrc, pDst);
							desc.Dtor(pSrc);
						}

						// Entity has been replaced with the last one in chunk.
						// Update its index so look ups can find it.
						auto& entityContainer = entities[entity.id()];
						entityContainer.idx = index;
						entityContainer.gen = entity.gen();
					}
				}

				/*!
				Makes the entity a part of a chunk on a given index.
				\param index Index of the entity
				\param entity Entity to store in the chunk
				*/
				void SetEntity(uint32_t index, Entity entity) {
					GAIA_ASSERT(index < m_header.count && "Entity chunk index out of bounds!");

					const auto offset = sizeof(Entity) * index + m_header.offsets.firstByte_EntityData;
					utils::unaligned_ref<Entity> mem((void*)&m_data[offset]);
					mem = entity;
				}

				/*!
				Returns the entity on a given index in the chunk.
				\param index Index of the entity
				\return Entity on a given index within the chunk.
				*/
				GAIA_NODISCARD Entity GetEntity(uint32_t index) const {
					GAIA_ASSERT(index < m_header.count && "Entity chunk index out of bounds!");

					const auto offset = sizeof(Entity) * index + m_header.offsets.firstByte_EntityData;
					utils::unaligned_ref<Entity> mem((void*)&m_data[offset]);
					return mem;
				}

				/*!
				Enables or disables the entity on a given index in the chunk.
				\param index Index of the entity
				\param enableEntity Enables or disabled the entity
				*/
				void EnableEntity(uint32_t index, bool enableEntity) {
					GAIA_ASSERT(index < m_header.count && "Entity chunk index out of bounds!");

					if (enableEntity) {
						m_header.disabledEntityMask.set(index, false);
						SetDisabled(m_header.disabledEntityMask.any());
					} else {
						m_header.disabledEntityMask.set(index, true);
						SetDisabled(true);
					}
				}

				/*!
				Returns a pointer to chunk data.
				\param offset Offset into chunk data
				\return Pointer to chunk data.
				*/
				uint8_t& GetData(uint32_t offset) {
					return m_data[offset];
				}

				/*!
				Returns a pointer to chunk data.
				\param offset Offset into chunk data
				\return Pointer to chunk data.
				*/
				const uint8_t& GetData(uint32_t offset) const {
					return m_data[offset];
				}

				/*!
				Returns an offset to chunk data at which the component is stored.
				\warning It is expected the component with \param componentId is present. Undefined behavior otherwise.
				\param componentType Component type
				\param componentId Component id
				\param componentIdx Index of the component in this chunk's component array
				\return Offset from the start of chunk data area.
				*/
				GAIA_NODISCARD GAIA_FORCEINLINE ChunkComponentOffset FindDataOffset(
						component::ComponentType componentType, component::ComponentId componentId, uint32_t& componentIdx) const {
					// Searching for a component that's not there! Programmer mistake.
					GAIA_ASSERT(HasComponent(componentType, componentId));
					// Don't use this with empty components. It's impossible to write to them anyway.
					GAIA_ASSERT(ComponentCache::Get().GetComponentDesc(componentId).properties.size != 0);

					const auto& componentIds = GetComponentIdArray(componentType);
					const auto& componentOffsets = GetComponentOffsetArray(componentType);

					componentIdx = (uint32_t)utils::get_index_unsafe(componentIds, componentId);
					const auto offset = componentOffsets[componentIdx];
					GAIA_ASSERT(offset >= m_header.offsets.firstByte_EntityData);
					return offset;
				}

				/*!
				Returns an offset to chunk data at which the component is stored.
				\warning It is expected the component with \param componentId is present. Undefined behavior otherwise.
				\param componentType Component type
				\param componentId Component id
				\return Offset from the start of chunk data area.
				*/
				GAIA_NODISCARD GAIA_FORCEINLINE ChunkComponentOffset
				FindDataOffset(component::ComponentType componentType, component::ComponentId componentId) const {
					[[maybe_unused]] uint32_t componentIdx = 0;
					return FindDataOffset(componentType, componentId, componentIdx);
				}

				//----------------------------------------------------------------------
				// Component handling
				//----------------------------------------------------------------------

				bool HasAnyCustomGenericConstructor() const {
					return m_header.hasAnyCustomGenericCtor;
				}

				bool HasAnyCustomChunkConstructor() const {
					return m_header.hasAnyCustomChunkCtor;
				}

				bool HasAnyCustomGenericDestructor() const {
					return m_header.hasAnyCustomGenericDtor;
				}

				bool HasAnyCustomChunkDestructor() const {
					return m_header.hasAnyCustomChunkDtor;
				}

				void CallConstructor(
						component::ComponentType componentType, component::ComponentId componentId, uint32_t entityIndex) {
					GAIA_PROF_SCOPE(CallConstructor);

					// Make sure only generic types are used with indices
					GAIA_ASSERT(componentType == component::ComponentType::CT_Generic || entityIndex == 0);

					const auto& cc = ComponentCache::Get();
					const auto& desc = cc.GetComponentDesc(componentId);
					if (desc.ctor == nullptr)
						return;

					const auto& componentIds = GetComponentIdArray(componentType);
					const auto& componentOffsets = GetComponentOffsetArray(componentType);

					const auto idx = utils::get_index_unsafe(componentIds, componentId);
					const auto offset = componentOffsets[idx];
					const auto idxSrc = offset + entityIndex * desc.properties.size;
					GAIA_ASSERT(idxSrc < Chunk::DATA_SIZE);

					auto* pSrc = (void*)&m_data[idxSrc];
					desc.ctor(pSrc, 1);
				}

				void CallConstructors(component::ComponentType componentType, uint32_t entityIndex, uint32_t entityCount) {
					GAIA_PROF_SCOPE(CallConstructors);

					GAIA_ASSERT(
							componentType == component::ComponentType::CT_Generic && HasAnyCustomGenericConstructor() ||
							componentType == component::ComponentType::CT_Chunk && HasAnyCustomChunkConstructor());

					// Make sure only generic types are used with indices
					GAIA_ASSERT(componentType == component::ComponentType::CT_Generic || (entityIndex == 0 && entityCount == 1));

					const auto& cc = ComponentCache::Get();
					const auto& componentIds = GetComponentIdArray(componentType);
					const auto& componentOffsets = GetComponentOffsetArray(componentType);

					for (size_t i = 0; i < componentIds.size(); i++) {
						const auto& desc = cc.GetComponentDesc(componentIds[i]);
						if (desc.ctor == nullptr)
							continue;

						const auto offset = componentOffsets[i];
						const auto idxSrc = offset + entityIndex * desc.properties.size;
						GAIA_ASSERT(idxSrc < Chunk::DATA_SIZE);

						auto* pSrc = (void*)&m_data[idxSrc];
						desc.ctor(pSrc, entityCount);
					}
				}

				void CallDestructors(component::ComponentType componentType, uint32_t entityIndex, uint32_t entityCount) {
					GAIA_PROF_SCOPE(CallDestructors);

					GAIA_ASSERT(
							componentType == component::ComponentType::CT_Generic && HasAnyCustomGenericDestructor() ||
							componentType == component::ComponentType::CT_Chunk && HasAnyCustomChunkDestructor());

					// Make sure only generic types are used with indices
					GAIA_ASSERT(componentType == component::ComponentType::CT_Generic || (entityIndex == 0 && entityCount == 1));

					const auto& cc = ComponentCache::Get();
					const auto& componentIds = GetComponentIdArray(componentType);
					const auto& componentOffsets = GetComponentOffsetArray(componentType);

					for (size_t i = 0; i < componentIds.size(); ++i) {
						const auto& desc = cc.GetComponentDesc(componentIds[i]);
						if (desc.dtor == nullptr)
							continue;

						const auto offset = componentOffsets[i];
						const auto idxSrc = offset + entityIndex * desc.properties.size;
						GAIA_ASSERT(idxSrc < Chunk::DATA_SIZE);

						auto* pSrc = (void*)&m_data[idxSrc];
						desc.dtor(pSrc, entityCount);
					}
				};

				//----------------------------------------------------------------------
				// Check component presence
				//----------------------------------------------------------------------

				/*!
				Checks if a component with \param componentId and type \param componentType is present in the chunk.
				\param componentId Component id
				\param componentType Component type
				\return True if found. False otherwise.
				*/
				GAIA_NODISCARD bool
				HasComponent(component::ComponentType componentType, component::ComponentId componentId) const {
					const auto& componentIds = GetComponentIdArray(componentType);
					return utils::has(componentIds, componentId);
				}

				/*!
				Checks if component \tparam T is present in the chunk.
				\tparam T Component
				\return True if the component is present. False otherwise.
				*/
				template <typename T>
				GAIA_NODISCARD bool HasComponent() const {
					const auto componentId = component::GetComponentId<T>();

					if constexpr (component::IsGenericComponent<T>)
						return HasComponent(component::ComponentType::CT_Generic, componentId);
					else
						return HasComponent(component::ComponentType::CT_Chunk, componentId);
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
				template <typename T, typename U = typename component::DeduceComponent<T>::Type>
				void SetComponent(uint32_t index, U&& value) {
					static_assert(
							component::IsGenericComponent<T>,
							"SetComponent providing an index can only be used with generic components");

					// Update the world version
					UpdateVersion(m_header.worldVersion);

					GAIA_ASSERT(index < m_header.capacity);
					ViewRW<T>()[index] = std::forward<U>(value);
				}

				/*!
				Sets the value of the chunk component \tparam T in the chunk.
				\warning It is expected the component \tparam T is present. Undefined behavior otherwise.
				\tparam T Component
				\param value Value to set for the component
				*/
				template <typename T, typename U = typename component::DeduceComponent<T>::Type>
				void SetComponent(U&& value) {
					static_assert(
							!component::IsGenericComponent<T>,
							"SetComponent not providing an index can only be used with chunk components");

					// Update the world version
					UpdateVersion(m_header.worldVersion);

					GAIA_ASSERT(0 < m_header.capacity);
					ViewRW<T>()[0] = std::forward<U>(value);
				}

				/*!
				Sets the value of the chunk component \tparam T on \param index in the chunk.
				\warning World version is not updated so Query filters will not be able to catch this change.
				\warning It is expected the component \tparam T is present. Undefined behavior otherwise.
				\tparam T Component
				\param index Index of entity in the chunk
				\param value Value to set for the component
				*/
				template <typename T, typename U = typename component::DeduceComponent<T>::Type>
				void SetComponentSilent(uint32_t index, U&& value) {
					static_assert(
							component::IsGenericComponent<T>,
							"SetComponentSilent providing an index can only be used with generic components");

					GAIA_ASSERT(index < m_header.capacity);
					ViewRWSilent<T>()[index] = std::forward<U>(value);
				}

				/*!
				Sets the value of the chunk component \tparam T in the chunk.
				\warning World version is not updated so Query filters will not be able to catch this change.
				\warning It is expected the component \tparam T is present. Undefined behavior otherwise.
				\tparam T Component
				\param value Value to set for the component
				*/
				template <typename T, typename U = typename component::DeduceComponent<T>::Type>
				void SetComponentSilent(U&& value) {
					static_assert(
							!component::IsGenericComponent<T>,
							"SetComponentSilent not providing an index can only be used with chunk components");

					GAIA_ASSERT(0 < m_header.capacity);
					ViewRWSilent<T>()[0] = std::forward<U>(value);
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
				GAIA_NODISCARD auto GetComponent(uint32_t index) const {
					static_assert(
							component::IsGenericComponent<T>,
							"GetComponent providing an index can only be used with generic components");
					return GetComponent_Internal<T>(index);
				}

				/*!
				Returns the value stored in the chunk component \tparam T.
				\warning It is expected the chunk component \tparam T is present. Undefined behavior otherwise.
				\tparam T Component
				\return Value stored in the component.
				*/
				template <typename T>
				GAIA_NODISCARD auto GetComponent() const {
					static_assert(
							!component::IsGenericComponent<T>,
							"GetComponent not providing an index can only be used with chunk components");
					return GetComponent_Internal<T>(0);
				}

				/*!
				Returns the internal index of a component based on the provided \param componentId.
				\param componentType Component type
				\return Component index if the component was found. -1 otherwise.
				*/
				GAIA_NODISCARD uint32_t
				GetComponentIdx(component::ComponentType componentType, component::ComponentId componentId) const {
					const auto& componentIds = GetComponentIdArray(componentType);
					const auto idx = utils::get_index_unsafe(componentIds, componentId);
					GAIA_ASSERT(idx != BadIndex);
					return (uint32_t)idx;
				}

				//----------------------------------------------------------------------
				// Iteration
				//----------------------------------------------------------------------

				template <typename T>
				GAIA_NODISCARD constexpr GAIA_FORCEINLINE auto GetComponentView() {
					using U = typename component::DeduceComponent<T>::Type;
					using UOriginal = typename component::DeduceComponent<T>::TypeOriginal;
					if constexpr (component::IsReadOnlyType<UOriginal>::value)
						return View_Internal<U>();
					else
						return ViewRW_Internal<U, true>();
				}

				template <typename... T, typename Func>
				GAIA_FORCEINLINE void ForEach([[maybe_unused]] utils::func_type_list<T...> types, Func func) {
					const size_t size = GetEntityCount();
					GAIA_ASSERT(size > 0);

					if constexpr (sizeof...(T) > 0) {
						// Pointers to the respective component types in the chunk, e.g
						// 		q.ForEach([&](Position& p, const Velocity& v) {...}
						// Translates to:
						//  	auto p = iter.ViewRW_Internal<Position, true>();
						//		auto v = iter.View_Internal<Velocity>();
						auto dataPointerTuple = std::make_tuple(GetComponentView<T>()...);

						// Iterate over each entity in the chunk.
						// Translates to:
						//		for (uint32_t i: iter)
						//			func(p[i], v[i]);

						for (size_t i = 0; i < size; ++i)
							func(std::get<decltype(GetComponentView<T>())>(dataPointerTuple)[i]...);
					} else {
						// No functor parameters. Do an empty loop.
						for (size_t i = 0; i < size; ++i)
							func();
					}
				}

				//----------------------------------------------------------------------

				GAIA_NODISCARD ArchetypeId GetArchetypeId() const {
					return m_header.archetypeId;
				}

				void SetChunkIndex(uint16_t value) {
					m_header.index = value;
				}

				GAIA_NODISCARD uint16_t GetChunkIndex() const {
					return m_header.index;
				}

				void SetDisabled(bool value) {
					m_header.hasDisabledEntities = value;
				}

				//! Checks is this chunk is disabled
				GAIA_NODISCARD bool HasDisabledEntities() const {
					return m_header.hasDisabledEntities != 0;
				}

				//! Checks is this chunk is dying
				GAIA_NODISCARD bool IsDying() const {
					return m_header.lifespanCountdown > 0;
				}

				void PrepareToDie() {
					m_header.lifespanCountdown = ChunkHeader::MAX_CHUNK_LIFESPAN;
				}

				bool ProgressDeath() {
					GAIA_ASSERT(IsDying());
					--m_header.lifespanCountdown;
					return IsDying();
				}

				void SetStructuralChanges(bool value) {
					if (value) {
						GAIA_ASSERT(m_header.structuralChangesLocked < 16);
						++m_header.structuralChangesLocked;
					} else {
						GAIA_ASSERT(m_header.structuralChangesLocked > 0);
						--m_header.structuralChangesLocked;
					}
				}

				bool IsStructuralChangesLocked() const {
					return m_header.structuralChangesLocked != 0;
				}

				//! Checks is the full capacity of the has has been reached
				GAIA_NODISCARD bool IsFull() const {
					return m_header.count >= m_header.capacity;
				}

				//! Checks is the chunk is semi-full.
				GAIA_NODISCARD bool IsSemiFull() const {
					constexpr float Threshold = 0.7f;
					return ((float)m_header.count / (float)m_header.capacity) < Threshold;
				}

				//! Checks is there are any entities in the chunk
				GAIA_NODISCARD bool HasEntities() const {
					return m_header.count > 0;
				}

				//! Returns the number of entities in the chunk
				GAIA_NODISCARD uint32_t GetEntityCount() const {
					return m_header.count;
				}

				//! Returns the number of entities in the chunk
				GAIA_NODISCARD uint32_t GetEntityCapacity() const {
					return m_header.capacity;
				}

				//! Returns the mask of disabled entities
				GAIA_NODISCARD ChunkHeader::DisabledEntityMask& GetDisabledEntityMask() {
					return m_header.disabledEntityMask;
				}

				//! Returns the mask of disabled entities
				GAIA_NODISCARD const ChunkHeader::DisabledEntityMask& GetDisabledEntityMask() const {
					return m_header.disabledEntityMask;
				}

				GAIA_NODISCARD std::span<uint32_t> GetComponentVersionArray(component::ComponentType componentType) const {
					const auto offset = m_header.offsets.firstByte_Versions[componentType];
					return {(uint32_t*)(&m_data[offset]), m_header.componentCount[componentType]};
				}

				GAIA_NODISCARD std::span<const component::ComponentId>
				GetComponentIdArray(component::ComponentType componentType) const {
					using RetType = std::add_pointer_t<const component::ComponentId>;
					const auto offset = m_header.offsets.firstByte_ComponentIds[componentType];
					return {(RetType)&m_data[offset], m_header.componentCount[componentType]};
				}

				GAIA_NODISCARD std::span<const ChunkComponentOffset>
				GetComponentOffsetArray(component::ComponentType componentType) const {
					using RetType = std::add_pointer_t<const ChunkComponentOffset>;
					const auto offset = m_header.offsets.firstByte_ComponentOffsets[componentType];
					return {(RetType)&m_data[offset], m_header.componentCount[componentType]};
				}

				//! Returns true if the provided version is newer than the one stored internally
				GAIA_NODISCARD bool
				DidChange(component::ComponentType componentType, uint32_t version, uint32_t componentIdx) const {
					auto versions = GetComponentVersionArray(componentType);
					return DidVersionChange(versions[componentIdx], version);
				}

				GAIA_FORCEINLINE void UpdateWorldVersion(component::ComponentType componentType, uint32_t componentIdx) const {
					// Make sure only proper input is provided
					GAIA_ASSERT(componentIdx >= 0 && componentIdx < archetype::MAX_COMPONENTS_PER_ARCHETYPE);

					auto versions = GetComponentVersionArray(componentType);

					// Update the version of the component
					versions[componentIdx] = m_header.worldVersion;
				}

				GAIA_FORCEINLINE void UpdateWorldVersion(component::ComponentType componentType) const {
					auto versions = GetComponentVersionArray(componentType);

					// Update the version of all components
					for (size_t i = 0; i < versions.size(); i++)
						versions[i] = m_header.worldVersion;
				}

				void Diag(uint32_t index) const {
					GAIA_LOG_N(
							"  Chunk #%04u, entities:%u/%u, lifespanCountdown:%u", index, m_header.count, m_header.capacity,
							m_header.lifespanCountdown);
				}
			};
			static_assert(sizeof(Chunk) <= ChunkMemorySize, "Chunk size must match ChunkMemorySize!");
		} // namespace archetype
	} // namespace ecs
} // namespace gaia

namespace gaia {
	namespace ecs {
		namespace archetype {
			class Archetype final {
			public:
				using LookupHash = utils::direct_hash_key<uint64_t>;
				using GenericComponentHash = utils::direct_hash_key<uint64_t>;
				using ChunkComponentHash = utils::direct_hash_key<uint64_t>;

			private:
				//! List of chunks allocated by this archetype
				containers::darray<Chunk*> m_chunks;
				//! Mask of chunk with disabled entities
				containers::dbitset m_disabledMask;
				//! Graph of archetypes linked with this one
				ArchetypeGraph m_graph;

				//! Offsets to various parts of data inside chunk
				ChunkHeaderOffsets m_dataOffsets;
				//! Description of components within this archetype
				containers::sarray<ComponentIdArray, component::ComponentType::CT_Count> m_componentIds;
				//! Lookup hashes of components within this archetype
				containers::sarray<ComponentOffsetArray, component::ComponentType::CT_Count> m_componentOffsets;

				//! Hash of generic components
				GenericComponentHash m_genericHash = {0};
				//! Hash of chunk components
				ChunkComponentHash m_chunkHash = {0};
				//! Hash of components within this archetype - used for lookups
				component::ComponentLookupHash m_lookupHash = {0};
				//! Hash of components within this archetype - used for matching
				component::ComponentMatcherHash m_matcherHash[component::ComponentType::CT_Count]{};

				//! Archetype ID - used to address the archetype directly in the world's list or archetypes
				ArchetypeId m_archetypeId = ArchetypeIdBad;
				//! Stable reference to parent world's world version
				uint32_t& m_worldVersion;
				struct {
					//! The number of entities this archetype can take (e.g 5 = 5 entities with all their components)
					uint32_t capacity : 16;
				} m_properties{};

				// Constructor is hidden. Create archetypes via Create
				Archetype(uint32_t& worldVersion): m_worldVersion(worldVersion) {}

				void UpdateDataOffsets(uintptr_t memoryAddress) {
					size_t offset = 0;

					// Versions
					{
						const size_t padding = utils::align(memoryAddress, alignof(uint32_t)) - memoryAddress;
						offset += padding;

						if (!m_componentIds[component::ComponentType::CT_Generic].empty()) {
							m_dataOffsets.firstByte_Versions[component::ComponentType::CT_Generic] = (ChunkComponentOffset)offset;
							offset += sizeof(uint32_t) * m_componentIds[component::ComponentType::CT_Generic].size();
						}
						if (!m_componentIds[component::ComponentType::CT_Chunk].empty()) {
							m_dataOffsets.firstByte_Versions[component::ComponentType::CT_Chunk] = (ChunkComponentOffset)offset;
							offset += sizeof(uint32_t) * m_componentIds[component::ComponentType::CT_Chunk].size();
						}
					}

					// Component ids
					{
						const size_t padding = utils::align(offset, alignof(component::ComponentId)) - offset;
						offset += padding;

						if (!m_componentIds[component::ComponentType::CT_Generic].empty()) {
							m_dataOffsets.firstByte_ComponentIds[component::ComponentType::CT_Generic] = (ChunkComponentOffset)offset;
							offset += sizeof(component::ComponentId) * m_componentIds[component::ComponentType::CT_Generic].size();
						}
						if (!m_componentIds[component::ComponentType::CT_Chunk].empty()) {
							m_dataOffsets.firstByte_ComponentIds[component::ComponentType::CT_Chunk] = (ChunkComponentOffset)offset;
							offset += sizeof(component::ComponentId) * m_componentIds[component::ComponentType::CT_Chunk].size();
						}
					}

					// Component offsets
					{
						const size_t padding = utils::align(offset, alignof(ChunkComponentOffset)) - offset;
						offset += padding;

						if (!m_componentIds[component::ComponentType::CT_Generic].empty()) {
							m_dataOffsets.firstByte_ComponentOffsets[component::ComponentType::CT_Generic] =
									(ChunkComponentOffset)offset;
							offset += sizeof(ChunkComponentOffset) * m_componentIds[component::ComponentType::CT_Generic].size();
						}
						if (!m_componentIds[component::ComponentType::CT_Chunk].empty()) {
							m_dataOffsets.firstByte_ComponentOffsets[component::ComponentType::CT_Chunk] =
									(ChunkComponentOffset)offset;
							offset += sizeof(ChunkComponentOffset) * m_componentIds[component::ComponentType::CT_Chunk].size();
						}
					}

					// First entity offset
					{
						const size_t padding = utils::align(offset, alignof(Entity)) - offset;
						offset += padding;
						m_dataOffsets.firstByte_EntityData = (ChunkComponentOffset)offset;
					}
				}

#if GAIA_AVOID_CHUNK_FRAGMENTATION
				static void VerifyChunksFragmentation_Internal(std::span<Chunk*> chunkArray) {
					if (chunkArray.size() <= 1)
						return;

					uint32_t i = 1;

					if (chunkArray[0]->IsFull()) {
						// Make sure all chunks before the first non-full one are full
						for (; i < chunkArray.size(); ++i) {
							auto* pChunk = chunkArray[i];
							if (pChunk->IsFull())
								GAIA_ASSERT(chunkArray[i - 1]->IsFull());
							else
								break;
						}
					}

					// Make sure all chunks after the full non-full are empty
					++i;
					for (; i < chunkArray.size(); ++i) {
						GAIA_ASSERT(!chunkArray[i]->HasEntities());
					}
				}

				//! Returns the first non-full chunk or nullptr if there is no such chunk.
				GAIA_NODISCARD static Chunk* FindFirstNonFullChunk_Internal(std::span<Chunk*> chunkArray) {
					if GAIA_UNLIKELY (chunkArray.empty())
						return nullptr;

					Chunk* pFirstNonFullChunk = nullptr;
					uint32_t i = (uint32_t)chunkArray.size() - 1;
					do {
						auto* pChunk = chunkArray[i];
						if (pChunk->IsFull())
							break;

						pFirstNonFullChunk = pChunk;
					} while (i-- > 0);

					return pFirstNonFullChunk;
				}

				//! Returns the first non-empty chunk or nullptr if there are no chunks.
				GAIA_NODISCARD static Chunk* FindFirstNonEmptyChunk_Internal(std::span<Chunk*> chunkArray) {
					if GAIA_UNLIKELY (chunkArray.empty())
						return nullptr;
					if (chunkArray.size() == 1)
						return chunkArray[0];

					Chunk* pFirstNonEmptyChunk = nullptr;
					uint32_t i = (uint32_t)chunkArray.size() - 1;
					do {
						auto* pChunk = chunkArray[i];
						if (pChunk->IsFull()) {
							if (pFirstNonEmptyChunk == nullptr)
								return pChunk;
							break;
						}

						if (pChunk->HasEntities())
							pFirstNonEmptyChunk = pChunk;
					} while (i-- > 0);

					return pFirstNonEmptyChunk;
				}
#endif

				GAIA_NODISCARD Chunk* FindOrCreateFreeChunk_Internal(containers::darray<Chunk*>& chunkArray) const {
					const auto chunkCnt = chunkArray.size();

					if (chunkCnt > 0) {
#if GAIA_AVOID_CHUNK_FRAGMENTATION
						// In order to avoid memory fragmentation we always take from the back.
						// This means all previous chunks are always going to be fully utilized
						// and it is safe for as to peek at the last one to make descisions.
						auto* pChunk = FindFirstNonFullChunk_Internal(chunkArray);
						if (pChunk != nullptr)
							return pChunk;
#else
						// Find first semi-empty chunk.
						// Picking the first non-full would only support fragmentation.
						Chunk* pEmptyChunk = nullptr;
						for (auto* pChunk: chunkArray) {
							GAIA_ASSERT(pChunk != nullptr);
							const auto entityCnt = pChunk->GetEntityCount();
							if GAIA_UNLIKELY (entityCnt == 0)
								pEmptyChunk = pChunk;
							else if (entityCnt + 1 < pChunk->GetEntityCapacity())
								return pChunk;
						}
						if (pEmptyChunk != nullptr)
							return pEmptyChunk;
#endif
					}

					// Make sure not too many chunks are allocated
					GAIA_ASSERT(chunkCnt < UINT16_MAX);

					// No free space found anywhere. Let's create a new chunk.
					auto* pChunk = Chunk::Create(
							m_archetypeId, (uint16_t)chunkCnt, m_properties.capacity, m_worldVersion, m_dataOffsets, m_componentIds,
							m_componentOffsets);

					chunkArray.push_back(pChunk);
					return pChunk;
				}

				/*!
				Checks if a component with \param componentId and type \param componentType is present in the archetype.
				\param componentId Component id
				\param componentType Component type
				\return True if found. False otherwise.
				*/
				GAIA_NODISCARD bool
				HasComponent_Internal(component::ComponentType componentType, component::ComponentId componentId) const {
					const auto& componentIds = GetComponentIdArray(componentType);
					return utils::has(componentIds, componentId);
				}

				/*!
				Checks if a component of type \tparam T is present in the archetype.
				\return True if found. False otherwise.
				*/
				template <typename T>
				GAIA_NODISCARD bool HasComponent_Internal() const {
					const auto componentId = component::GetComponentId<T>();

					if constexpr (component::IsGenericComponent<T>) {
						return HasComponent_Internal(component::ComponentType::CT_Generic, componentId);
					} else {
						return HasComponent_Internal(component::ComponentType::CT_Chunk, componentId);
					}
				}

			public:
				/*!
				Checks if the archetype id is valid.
				\return True if the id is valid, false otherwise.
				*/
				static bool IsIdValid(ArchetypeId id) {
					return id != ArchetypeIdBad;
				}

				Archetype(Archetype&& world) = delete;
				Archetype(const Archetype& world) = delete;
				Archetype& operator=(Archetype&&) = delete;
				Archetype& operator=(const Archetype&) = delete;

				~Archetype() {
					// Delete all archetype chunks
					for (auto* pChunk: m_chunks)
						Chunk::Release(pChunk);
				}

				GAIA_NODISCARD static LookupHash
				CalculateLookupHash(GenericComponentHash genericHash, ChunkComponentHash chunkHash) noexcept {
					return {utils::hash_combine(genericHash.hash, chunkHash.hash)};
				}

				GAIA_NODISCARD static Archetype* Create(
						ArchetypeId archetypeId, uint32_t& worldVersion, component::ComponentIdSpan componentIdsGeneric,
						component::ComponentIdSpan componentIdsChunk) {
					auto* newArch = new Archetype(worldVersion);
					newArch->m_archetypeId = archetypeId;
					newArch->m_componentIds[component::ComponentType::CT_Generic].resize(componentIdsGeneric.size());
					newArch->m_componentIds[component::ComponentType::CT_Chunk].resize(componentIdsChunk.size());
					newArch->m_componentOffsets[component::ComponentType::CT_Generic].resize(componentIdsGeneric.size());
					newArch->m_componentOffsets[component::ComponentType::CT_Chunk].resize(componentIdsChunk.size());
					newArch->UpdateDataOffsets(sizeof(ChunkHeader));

					const auto& cc = ComponentCache::Get();
					const auto& dataOffset = newArch->m_dataOffsets;

					// Calculate the number of entities per chunks precisely so we can
					// fit as many of them into chunk as possible.

					// Total size of generic components
					size_t genericComponentListSize = 0;
					for (const auto componentId: componentIdsGeneric) {
						const auto& desc = cc.GetComponentDesc(componentId);
						genericComponentListSize += desc.properties.size;
					}

					// Total size of chunk components
					size_t chunkComponentListSize = 0;
					for (const auto componentId: componentIdsChunk) {
						const auto& desc = cc.GetComponentDesc(componentId);
						chunkComponentListSize += desc.properties.size;
					}

					// Theoretical maximum number of components we can fit into one chunk.
					// This can be further reduced due alignment and padding.
					auto maxGenericItemsInArchetype =
							(Chunk::DATA_SIZE - dataOffset.firstByte_EntityData - chunkComponentListSize - 1) /
							(genericComponentListSize + sizeof(Entity));

				recalculate:
					auto componentOffsets = dataOffset.firstByte_EntityData + sizeof(Entity) * maxGenericItemsInArchetype;

					auto adjustMaxGenericItemsInAchetype = [&](component::ComponentIdSpan componentIds, size_t size) {
						for (const auto componentId: componentIds) {
							const auto& desc = cc.GetComponentDesc(componentId);
							const auto alignment = desc.properties.alig;
							if (alignment == 0)
								continue;

							const auto padding = utils::align(componentOffsets, alignment) - componentOffsets;

							// For SoA types we shall assume there is a padding of the entire size of the array.
							// Of course this is a bit wasteful but it's a bit of work to calculate how much area exactly we need.
							// We might have:
							// 	struct foo { float x; float y; bool a; float z; };
							// Each of the variables of the foo struct might need separate padding when converted to SoA.
							// TODO: Introduce a function that can calculate this.
							const auto componentDataSize =
									padding + ((uint32_t)desc.properties.soa * desc.properties.size) + desc.properties.size * size;
							const auto nextOffset = componentOffsets + componentDataSize;

							// If we're beyond what the chunk could take, subtract one entity
							if (nextOffset >= Chunk::DATA_SIZE) {
								--maxGenericItemsInArchetype;
								return false;
							}

							componentOffsets += componentDataSize;
						}

						return true;
					};

					// Adjust the maximum number of entities. Recalculation happens at most once when the original guess
					// for entity count is not right (most likely because of padding or usage of SoA components).
					if (!adjustMaxGenericItemsInAchetype(componentIdsGeneric, maxGenericItemsInArchetype))
						goto recalculate;
					if (!adjustMaxGenericItemsInAchetype(componentIdsChunk, 1))
						goto recalculate;

					// TODO: Make it possible for chunks to be not restricted by ChunkHeader::DisabledEntityMask::BitCount
					if (maxGenericItemsInArchetype > ChunkHeader::DisabledEntityMask::BitCount)
						maxGenericItemsInArchetype = ChunkHeader::DisabledEntityMask::BitCount;

					// Update the offsets according to the recalculated maxGenericItemsInArchetype
					componentOffsets = dataOffset.firstByte_EntityData + sizeof(Entity) * maxGenericItemsInArchetype;

					auto registerComponents = [&](component::ComponentIdSpan componentIds, component::ComponentType componentType,
																				const size_t count) {
						auto& ids = newArch->m_componentIds[componentType];
						auto& ofs = newArch->m_componentOffsets[componentType];

						for (size_t i = 0; i < componentIds.size(); ++i) {
							const auto componentId = componentIds[i];
							const auto& desc = cc.GetComponentDesc(componentId);
							const auto alignment = desc.properties.alig;
							if (alignment == 0) {
								GAIA_ASSERT(desc.properties.size == 0);

								// Register the component info
								ids[i] = componentId;
								ofs[i] = {};
							} else {
								const size_t padding = utils::align(componentOffsets, alignment) - componentOffsets;
								componentOffsets += padding;

								// Register the component info
								ids[i] = componentId;
								ofs[i] = (ChunkComponentOffset)componentOffsets;

								// Make sure the following component list is properly aligned
								componentOffsets += desc.properties.size * count;
							}
						}
					};
					registerComponents(componentIdsGeneric, component::ComponentType::CT_Generic, maxGenericItemsInArchetype);
					registerComponents(componentIdsChunk, component::ComponentType::CT_Chunk, 1);

					newArch->m_properties.capacity = (uint32_t)maxGenericItemsInArchetype;
					newArch->m_matcherHash[component::ComponentType::CT_Generic] =
							component::CalculateMatcherHash(componentIdsGeneric);
					newArch->m_matcherHash[component::ComponentType::CT_Chunk] =
							component::CalculateMatcherHash(componentIdsChunk);

					return newArch;
				}

				/*!
				Initializes the archetype with hash values for each kind of component types.
				\param hashGeneric Generic components hash
				\param hashChunk Chunk components hash
				\param hashLookup Hash used for archetype lookup purposes
				*/
				void Init(
						GenericComponentHash hashGeneric, ChunkComponentHash hashChunk, component::ComponentLookupHash hashLookup) {
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
				void EnableEntity(Chunk* pChunk, uint32_t entityIdx, bool enableEntity) {
					pChunk->EnableEntity(entityIdx, enableEntity);
					m_disabledMask.set(pChunk->GetChunkIndex(), pChunk->HasDisabledEntities());
				}

				/*!
				Removes a chunk from the list of chunks managed by their achetype.
				\param pChunk Chunk to remove from the list of managed archetypes
				*/
				void RemoveChunk(Chunk* pChunk) {
					const auto chunkIndex = pChunk->GetChunkIndex();

					Chunk::Release(pChunk);

					auto remove = [&](auto& chunkArray) {
						if (chunkArray.size() > 1)
							chunkArray.back()->SetChunkIndex(chunkIndex);
						GAIA_ASSERT(chunkIndex == utils::get_index(chunkArray, pChunk));
						utils::erase_fast(chunkArray, chunkIndex);
					};

					remove(m_chunks);
				}

#if GAIA_AVOID_CHUNK_FRAGMENTATION
				void VerifyChunksFramentation() const {
					VerifyChunksFragmentation_Internal(m_chunks);
				}

				//! Returns the first non-empty chunk or nullptr if none is found.
				GAIA_NODISCARD Chunk* FindFirstNonEmptyChunk() const {
					auto* pChunk = FindFirstNonEmptyChunk_Internal(m_chunks);
					GAIA_ASSERT(pChunk == nullptr || !pChunk->HasDisabledEntities());
					return pChunk;
				}
#else
				//! Defragments the chunk.
				//! \param maxEntites Maximum number of entities moved per call
				//! \param chunksToRemove Container of chunks ready for removal
				//! \param entities Container with entities
				void Defragment(
						uint32_t& maxEntities, containers::darray<Chunk*>& chunksToRemove, std::span<EntityContainer> entities) {
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

					uint32_t front = 0;
					uint32_t back = (uint32_t)m_chunks.size();

					// Find the first semi-empty chunk in the front
					while (front < m_chunks.size() && !m_chunks[front++]->IsSemiFull())
						;
					auto* pDstChunk = m_chunks[front];
					uint32_t firstFreeIdxInDstChunk = pDstChunk->GetEntityCount();

					// Find the first semi-empty chunk in the back
					bool updateDstChunkVersions = false;
					while (front < back && m_chunks[--back]->IsSemiFull()) {
						updateDstChunkVersions = true;

						auto* pSrcChunk = m_chunks[back];
						pSrcChunk->UpdateVersions();

						const uint32_t entitiesInChunk = pSrcChunk->GetEntityCount();
						const uint32_t entitiesToMove = entitiesInChunk > maxEntities ? maxEntities : entitiesInChunk;
						for (uint32_t i = 0; i < entitiesToMove; ++i) {
							const auto lastEntityIdx = entitiesInChunk - i - 1;
							auto entity = pSrcChunk->GetEntity(lastEntityIdx);
							pDstChunk->SetEntity(firstFreeIdxInDstChunk, entity);
							pDstChunk->MoveEntityData(entity, firstFreeIdxInDstChunk++, entities);
							pSrcChunk->RemoveLastEntity(chunksToRemove);

							auto& lastEntityContainer = entities[entity.id()];
							lastEntityContainer.pChunk = pDstChunk;
							lastEntityContainer.idx = firstFreeIdxInDstChunk;
							lastEntityContainer.gen = entity.gen();

							// The destination chunk is full, we need to move to the next one
							if (firstFreeIdxInDstChunk == m_properties.capacity) {
								pDstChunk->UpdateVersions();
								updateDstChunkVersions = false;
								++front;

								// We reached the source chunk which means this archetype has been deframented
								if (front >= back) {
									maxEntities -= i + 1;
									return;
								}
							}
						}

						maxEntities -= entitiesToMove;
					}

					if (updateDstChunkVersions)
						pDstChunk->UpdateVersions();
				}
#endif

				//! Tries to locate a chunk that has some space left for a new entity.
				//! If not found a new chunk is created.
				GAIA_NODISCARD Chunk* FindOrCreateFreeChunk() {
					auto* pChunk = FindOrCreateFreeChunk_Internal(m_chunks);
					GAIA_ASSERT(!pChunk->HasDisabledEntities());
					return pChunk;
				}

				GAIA_NODISCARD ArchetypeId GetArchetypeId() const {
					return m_archetypeId;
				}

				GAIA_NODISCARD uint32_t GetCapacity() const {
					return m_properties.capacity;
				}

				GAIA_NODISCARD const containers::darray<Chunk*>& GetChunks() const {
					return m_chunks;
				}

				GAIA_NODISCARD GenericComponentHash GetGenericHash() const {
					return m_genericHash;
				}

				GAIA_NODISCARD ChunkComponentHash GetChunkHash() const {
					return m_chunkHash;
				}

				GAIA_NODISCARD component::ComponentLookupHash GetLookupHash() const {
					return m_lookupHash;
				}

				GAIA_NODISCARD component::ComponentMatcherHash GetMatcherHash(component::ComponentType componentType) const {
					return m_matcherHash[componentType];
				}

				GAIA_NODISCARD const ComponentIdArray& GetComponentIdArray(component::ComponentType componentType) const {
					return m_componentIds[componentType];
				}

				GAIA_NODISCARD const ComponentOffsetArray&
				GetComponentOffsetArray(component::ComponentType componentType) const {
					return m_componentOffsets[componentType];
				}

				/*!
				Checks if a given component is present on the archetype.
				\return True if the component is present. False otherwise.
				*/
				template <typename T>
				GAIA_NODISCARD bool HasComponent() const {
					return HasComponent_Internal<T>();
				}

				/*!
				Returns the internal index of a component based on the provided \param componentId.
				\param componentType Component type
				\return Component index if the component was found. -1 otherwise.
				*/
				GAIA_NODISCARD uint32_t
				GetComponentIdx(component::ComponentType componentType, component::ComponentId componentId) const {
					const auto idx = utils::get_index_unsafe(m_componentIds[componentType], componentId);
					GAIA_ASSERT(idx != BadIndex);
					return (uint32_t)idx;
				}

				void BuildGraphEdges(
						Archetype* pArchetypeRight, component::ComponentType componentType, component::ComponentId componentId) {
					GAIA_ASSERT(pArchetypeRight != this);
					m_graph.AddGraphEdgeRight(componentType, componentId, pArchetypeRight->GetArchetypeId());
					pArchetypeRight->BuildGraphEdgesLeft(this, componentType, componentId);
				}

				void BuildGraphEdgesLeft(
						Archetype* pArchetypeLeft, component::ComponentType componentType, component::ComponentId componentId) {
					GAIA_ASSERT(pArchetypeLeft != this);
					m_graph.AddGraphEdgeLeft(componentType, componentId, pArchetypeLeft->GetArchetypeId());
				}

				//! Checks if the graph edge for component type \param componentType contains the component \param componentId.
				//! \return Archetype id of the target archetype if the edge is found. ArchetypeIdBad otherwise.
				GAIA_NODISCARD ArchetypeId
				FindGraphEdgeRight(component::ComponentType componentType, const component::ComponentId componentId) const {
					return m_graph.FindGraphEdgeRight(componentType, componentId);
				}

				//! Checks if the graph edge for component type \param componentType contains the component \param componentId.
				//! \return Archetype id of the target archetype if the edge is found. ArchetypeIdBad otherwise.
				GAIA_NODISCARD ArchetypeId
				FindGraphEdgeLeft(component::ComponentType componentType, const component::ComponentId componentId) const {
					return m_graph.FindGraphEdgeLeft(componentType, componentId);
				}

				static void DiagArchetype_PrintBasicInfo(const Archetype& archetype) {
					const auto& cc = ComponentCache::Get();
					const auto& genericComponents = archetype.m_componentIds[component::ComponentType::CT_Generic];
					const auto& chunkComponents = archetype.m_componentIds[component::ComponentType::CT_Chunk];

					// Caclulate the number of entites in archetype
					uint32_t entityCount = 0;
					uint32_t entityCountDisabled = 0;
					for (const auto* chunk: archetype.m_chunks) {
						entityCount += chunk->GetEntityCount();
						entityCountDisabled += chunk->GetDisabledEntityMask().count();
					}

					// Calculate the number of components
					uint32_t genericComponentsSize = 0;
					uint32_t chunkComponentsSize = 0;
					for (const auto componentId: genericComponents) {
						const auto& desc = cc.GetComponentDesc(componentId);
						genericComponentsSize += desc.properties.size;
					}
					for (const auto componentId: chunkComponents) {
						const auto& desc = cc.GetComponentDesc(componentId);
						chunkComponentsSize += desc.properties.size;
					}

					GAIA_LOG_N(
							"Archetype ID:%u, "
							"lookupHash:%016" PRIx64 ", "
							"mask:%016" PRIx64 "/%016" PRIx64 ", "
							"chunks:%u, data size:%3u B (%u/%u), "
							"entities:%u/%u (disabled:%u)",
							archetype.m_archetypeId, archetype.m_lookupHash.hash,
							archetype.m_matcherHash[component::ComponentType::CT_Generic].hash,
							archetype.m_matcherHash[component::ComponentType::CT_Chunk].hash, (uint32_t)archetype.m_chunks.size(),
							genericComponentsSize + chunkComponentsSize, genericComponentsSize, chunkComponentsSize, entityCount,
							archetype.m_properties.capacity, entityCountDisabled);

					auto logComponentInfo = [](const component::ComponentInfo& info, const component::ComponentDesc& desc) {
						GAIA_LOG_N(
								"    lookupHash:%016" PRIx64 ", mask:%016" PRIx64 ", size:%3u B, align:%3u B, %.*s",
								info.lookupHash.hash, info.matcherHash.hash, desc.properties.size, desc.properties.alig,
								(uint32_t)desc.name.size(), desc.name.data());
					};

					if (!genericComponents.empty()) {
						GAIA_LOG_N("  Generic components - count:%u", (uint32_t)genericComponents.size());
						for (const auto componentId: genericComponents) {
							const auto& info = cc.GetComponentInfo(componentId);
							logComponentInfo(info, cc.GetComponentDesc(componentId));
						}
						if (!chunkComponents.empty()) {
							GAIA_LOG_N("  Chunk components - count:%u", (uint32_t)chunkComponents.size());
							for (const auto componentId: chunkComponents) {
								const auto& info = cc.GetComponentInfo(componentId);
								logComponentInfo(info, cc.GetComponentDesc(componentId));
							}
						}
					}
				}

				static void DiagArchetype_PrintGraphInfo(const Archetype& archetype) {
					archetype.m_graph.Diag();
				}

				static void DiagArchetype_PrintChunkInfo(const Archetype& archetype) {
					auto logChunks = [](const auto& chunks) {
						for (size_t i = 0; i < chunks.size(); ++i) {
							const auto* pChunk = chunks[i];
							pChunk->Diag((uint32_t)i);
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
				static void DiagArchetype(const Archetype& archetype) {
					DiagArchetype_PrintBasicInfo(archetype);
					DiagArchetype_PrintGraphInfo(archetype);
					DiagArchetype_PrintChunkInfo(archetype);
				}
			};
		} // namespace archetype
	} // namespace ecs
} // namespace gaia

REGISTER_HASH_TYPE(gaia::ecs::Archetype::LookupHash)
REGISTER_HASH_TYPE(gaia::ecs::Archetype::GenericComponentHash)
REGISTER_HASH_TYPE(gaia::ecs::Archetype::ChunkComponentHash)

#include <cinttypes>
#include <type_traits>

#include <type_traits>

// TODO: There is no quickly achievable std alternative so go with gaia container

#include <cstddef>
#include <type_traits>
#include <utility>

namespace gaia {
	namespace containers {
		//! Array of elements of type \tparam T allocated on heap or stack. Stack capacity is \tparam N elements.
		//! If the number of elements is bellow \tparam N the stack storage is used.
		//! If the number of elements is above \tparam N the heap storage is used.
		//! Interface compatiblity with std::vector and std::array where it matters.
		template <typename T, uint32_t N>
		class darr_ext {
		public:
			using iterator_category = GAIA_UTIL::random_access_iterator_tag;
			using value_type = T;
			using reference = T&;
			using const_reference = const T&;
			using pointer = T*;
			using const_pointer = T*;
			using difference_type = std::ptrdiff_t;
			using size_type = decltype(N);

			static constexpr size_type extent = N;

		private:
			//! Data allocated on the stack
			T m_data[N != 0U ? N : 1]; // support zero-size arrays
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
					m_pDataHeap = new T[m_cap = 4];
					return;
				}

				// Increase the size of an existing array.
				// We increase the capacity in multiples of 1.5 which is about the golden ratio (1.618).
				// This means we prefer more frequent allocations over memory fragmentation.
				T* old = m_pDataHeap;
				m_pDataHeap = new T[m_cap = (cap * 3) / 2 + 1];
				utils::move_elements(m_pDataHeap, old, cnt);
				delete[] old;
			}

		public:
			class iterator {
			public:
				using iterator_category = GAIA_UTIL::random_access_iterator_tag;
				using value_type = T;
				using difference_type = std::ptrdiff_t;
				using pointer = T*;
				using reference = T&;
				using size_type = decltype(N);

			private:
				pointer m_ptr;

			public:
				iterator(T* ptr): m_ptr(ptr) {}

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
					return m_ptr - other.m_ptr;
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

			class const_iterator {
			public:
				using iterator_category = GAIA_UTIL::random_access_iterator_tag;
				using value_type = const T;
				using difference_type = std::ptrdiff_t;
				using pointer = const T*;
				using reference = const T&;
				using size_type = decltype(N);

			private:
				pointer m_ptr;

			public:
				const_iterator(pointer ptr): m_ptr(ptr) {}

				reference operator*() const {
					return *m_ptr;
				}
				pointer operator->() const {
					return m_ptr;
				}
				const_iterator operator[](size_type offset) const {
					return {m_ptr + offset};
				}

				const_iterator& operator+=(size_type diff) {
					m_ptr += diff;
					return *this;
				}
				const_iterator& operator-=(size_type diff) {
					m_ptr -= diff;
					return *this;
				}
				const_iterator& operator++() {
					++m_ptr;
					return *this;
				}
				const_iterator operator++(int) {
					const_iterator temp(*this);
					++*this;
					return temp;
				}
				const_iterator& operator--() {
					--m_ptr;
					return *this;
				}
				const_iterator operator--(int) {
					const_iterator temp(*this);
					--*this;
					return temp;
				}

				const_iterator operator+(size_type offset) const {
					return {m_ptr + offset};
				}
				const_iterator operator-(size_type offset) const {
					return {m_ptr - offset};
				}
				difference_type operator-(const const_iterator& other) const {
					return m_ptr - other.m_ptr;
				}

				GAIA_NODISCARD bool operator==(const const_iterator& other) const {
					return m_ptr == other.m_ptr;
				}
				GAIA_NODISCARD bool operator!=(const const_iterator& other) const {
					return m_ptr != other.m_ptr;
				}
				GAIA_NODISCARD bool operator>(const const_iterator& other) const {
					return m_ptr > other.m_ptr;
				}
				GAIA_NODISCARD bool operator>=(const const_iterator& other) const {
					return m_ptr >= other.m_ptr;
				}
				GAIA_NODISCARD bool operator<(const const_iterator& other) const {
					return m_ptr < other.m_ptr;
				}
				GAIA_NODISCARD bool operator<=(const const_iterator& other) const {
					return m_ptr <= other.m_ptr;
				}
			};

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
				const auto count = (size_type)GAIA_UTIL::distance(first, last);
				resize(count);

				if constexpr (std::is_pointer_v<InputIt>) {
					for (size_type i = 0; i < count; ++i)
						m_pData[i] = first[i];
				} else if constexpr (std::is_same_v<
																 typename InputIt::iterator_category, GAIA_UTIL::random_access_iterator_tag>) {
					for (size_type i = 0; i < count; ++i)
						m_pData[i] = *(first[i]);
				} else {
					size_type i = 0;
					for (auto it = first; it != last; ++it)
						m_pData[i++] = *it;
				}
			}

			darr_ext(std::initializer_list<T> il): darr_ext(il.begin(), il.end()) {}

			darr_ext(const darr_ext& other): darr_ext(other.begin(), other.end()) {}

			darr_ext(darr_ext&& other) noexcept: m_cnt(other.m_cnt), m_cap(other.m_cap) {
				GAIA_ASSERT(GAIA_UTIL::addressof(other) != this);

				if (other.m_pData == other.m_pDataHeap) {
					m_pData = m_pDataHeap;
					m_pDataHeap = other.m_pDataHeap;
				} else {
					m_pData = m_data;
					utils::move_elements(m_data, other.m_data, other.size());
					m_pDataHeap = nullptr;
				}

				other.m_cnt = size_type(0);
				other.m_cap = extent;
				other.m_pDataHeap = nullptr;
				other.m_pData = m_data;
			}

			GAIA_NODISCARD darr_ext& operator=(std::initializer_list<T> il) {
				*this = darr_ext(il.begin(), il.end());
				return *this;
			}

			GAIA_NODISCARD darr_ext& operator=(const darr_ext& other) {
				GAIA_ASSERT(GAIA_UTIL::addressof(other) != this);

				resize(other.size());
				utils::copy_elements(m_pData, other.m_pData, other.size());

				return *this;
			}

			GAIA_NODISCARD darr_ext& operator=(darr_ext&& other) noexcept {
				GAIA_ASSERT(GAIA_UTIL::addressof(other) != this);

				m_cnt = other.m_cnt;
				m_cap = other.m_cap;
				if (other.m_pData == other.m_pDataHeap) {
					m_pData = m_pDataHeap;
					m_pDataHeap = other.m_pDataHeap;
				} else {
					m_pData = m_data;
					utils::move_elements(m_data, other.m_data, other.m_data.size());
					m_pDataHeap = nullptr;
				}

				other.m_cnt = size_type(0);
				other.m_cap = extent;
				other.m_pDataHeap = nullptr;
				other.m_pData = m_data;

				return *this;
			}

			~darr_ext() {
				delete[] m_pDataHeap;
			}

			GAIA_NODISCARD pointer data() noexcept {
				return (pointer)m_pData;
			}

			GAIA_NODISCARD const_pointer data() const noexcept {
				return (const_pointer)m_pData;
			}

			GAIA_NODISCARD reference operator[](size_type pos) noexcept {
				return (reference)m_pData[pos];
			}

			GAIA_NODISCARD const_reference operator[](size_type pos) const noexcept {
				return (const_reference)m_pData[pos];
			}

			void reserve(size_type count) {
				if (count <= m_cap)
					return;

				if (m_pDataHeap) {
					T* old = m_pDataHeap;
					m_pDataHeap = new T[count];
					utils::move_elements(m_pDataHeap, old, size());
					delete[] old;
				} else {
					m_pDataHeap = new T[count];
					utils::move_elements(m_pDataHeap, m_data, size());
				}

				m_cap = count;
				m_pData = m_pDataHeap;
			}

			void resize(size_type count) {
				if (count <= extent || count <= m_cap) {
					m_cnt = count;
					return;
				}

				pointer pOldData = m_pDataHeap;
				m_pDataHeap = new T[count];
				if (pOldData != nullptr) {
					utils::move_elements(m_pDataHeap, pOldData, size());
					delete[] pOldData;
				} else
					utils::move_elements(m_pDataHeap, m_data, size());

				m_cap = count;
				m_cnt = count;
				m_pData = m_pDataHeap;
			}

			void push_back(const T& arg) {
				try_grow();
				m_pData[m_cnt++] = arg;
			}

			void push_back(T&& arg) {
				try_grow();
				m_pData[m_cnt++] = std::forward<T>(arg);
			}

			template <typename... Args>
			reference emplace_back(Args&&... args) {
				try_grow();

				reference ref = m_pData[m_cnt++];
				::new (&ref) T(std::forward<Args>(args)...);
				return ref;
			}

			void pop_back() noexcept {
				GAIA_ASSERT(!empty());
				reference ref = m_pData[--m_cnt];
				ref.~T();
			}

			GAIA_NODISCARD iterator erase(iterator pos) noexcept {
				GAIA_ASSERT(pos.m_ptr >= &m_pData[0] && pos.m_ptr < &m_pData[m_cap - 1]);

				const auto idxSrc = (size_type)GAIA_UTIL::distance(pos, begin());
				const auto idxDst = size() - 1;

				utils::shift_elements_left(&m_pData[idxSrc], idxDst - idxSrc);
				--m_cnt;

				return iterator((T*)m_pData + idxSrc);
			}

			GAIA_NODISCARD const_iterator erase(const_iterator pos) noexcept {
				GAIA_ASSERT(pos.m_ptr >= &m_pData[0] && pos.m_ptr < &m_pData[m_cap - 1]);

				const auto idxSrc = (size_type)GAIA_UTIL::distance(pos, begin());
				const auto idxDst = size() - 1;

				utils::shift_elements_left(&m_pData[idxSrc], idxDst - idxSrc);
				--m_cnt;

				return iterator((const T*)m_pData + idxSrc);
			}

			GAIA_NODISCARD iterator erase(iterator first, iterator last) noexcept {
				GAIA_ASSERT(first.m_cnt >= 0 && first.m_cnt < size());
				GAIA_ASSERT(last.m_cnt >= 0 && last.m_cnt < size());
				GAIA_ASSERT(last.m_cnt >= first.m_cnt);

				const auto size = last.m_cnt - first.m_cnt;
				utils::shift_elements_left(&m_pData[first.cnt], size);
				m_cnt -= size;

				return {(T*)m_pData + size_type(last.m_cnt)};
			}

			void clear() {
				resize(0);
			}

			void shirk_to_fit() {
				if (capacity() == size())
					return;

				if (m_pData == m_pDataHeap) {
					T* old = m_pDataHeap;

					if (size() < extent) {
						utils::move_elements(m_data, old, size());
						m_pData = m_data;
					} else {
						m_pDataHeap = new T[m_cap = size()];
						utils::move_elements(m_pDataHeap, old, size());
						m_pData = m_pDataHeap;
					}

					delete[] old;
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

			GAIA_NODISCARD reference front() noexcept {
				return *begin();
			}

			GAIA_NODISCARD const_reference front() const noexcept {
				return *begin();
			}

			GAIA_NODISCARD reference back() noexcept {
				return N != 0U ? *(end() - 1) : *end();
			}

			GAIA_NODISCARD const_reference back() const noexcept {
				return N != 0U ? *(end() - 1) : *end();
			}

			GAIA_NODISCARD iterator begin() const noexcept {
				return {(T*)m_pData};
			}

			GAIA_NODISCARD const_iterator cbegin() const noexcept {
				return {(const T*)m_pData};
			}

			GAIA_NODISCARD iterator end() const noexcept {
				return {(T*)m_pData + size()};
			}

			GAIA_NODISCARD const_iterator cend() const noexcept {
				return {(const T*)m_pData + size()};
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
		};

		namespace detail {
			template <typename T, std::size_t N, std::size_t... I>
			darr_ext<std::remove_cv_t<T>, N> to_sarray_impl(T (&a)[N], std::index_sequence<I...> /*no_name*/) {
				return {{a[I]...}};
			}
		} // namespace detail

		template <typename T, std::size_t N>
		darr_ext<std::remove_cv_t<T>, N> to_sarray(T (&a)[N]) {
			return detail::to_sarray_impl(a, std::make_index_sequence<N>{});
		}

	} // namespace containers

} // namespace gaia

namespace std {
	template <typename T, size_t N>
	struct tuple_size<gaia::containers::darr_ext<T, N>>: std::integral_constant<std::size_t, N> {};

	template <size_t I, typename T, size_t N>
	struct tuple_element<I, gaia::containers::darr_ext<T, N>> {
		using type = T;
	};
} // namespace std

namespace gaia {
	namespace containers {
		template <typename T, auto N>
		using darray_ext = containers::darr_ext<T, N>;
	} // namespace containers
} // namespace gaia

namespace gaia {
	namespace ecs {
		class DataBuffer {
			// Increase the capacity by multiples of CapacityIncreaseSize
			static constexpr uint32_t CapacityIncreaseSize = 128U;
			// TODO: Replace with some memory allocator
			using DataContainer = containers::darray_ext<uint8_t, CapacityIncreaseSize>;

			//! Buffer holding raw data
			DataContainer m_data;
			//! Current position in the buffer
			uint32_t m_dataPos = 0;

		public:
			DataBuffer() {}

			void Reset() {
				m_dataPos = 0;
				m_data.clear();
			}

			//! Returns the number of bytes written in the buffer
			GAIA_NODISCARD uint32_t Size() const {
				return (uint32_t)m_data.size();
			}

			//! Makes sure there is enough capacity in our data container to hold another \param size bytes of data
			void EnsureCapacity(uint32_t size) {
				const auto nextSize = m_dataPos + size;
				if (nextSize <= (uint32_t)m_data.size())
					return;

				// Make sure there is enough capacity to hold our data
				const auto newSize = m_data.size() + size;
				const auto newCapacity = (newSize / CapacityIncreaseSize) * CapacityIncreaseSize + CapacityIncreaseSize;
				m_data.reserve(newCapacity);
			}

			//! Changes the current position in the buffer
			void Seek(uint32_t pos) {
				m_dataPos = pos;
			}

			//! Returns the current position in the buffer
			GAIA_NODISCARD uint32_t GetPos() const {
				return m_dataPos;
			}

			//! Writes \param value to the buffer
			template <typename T>
			void Save(T&& value) {
				EnsureCapacity(sizeof(T));

				m_data.resize(m_dataPos + sizeof(T));
				utils::unaligned_ref<T> mem(&m_data[m_dataPos]);
				mem = std::forward<T>(value);

				m_dataPos += sizeof(T);
			}

			//! Writes \param value to the buffer
			template <typename T>
			void SaveComponent(T&& value) {
				const auto componentId = component::GetComponentId<T>();
				const auto& desc = ComponentCache::Get().GetComponentDesc(componentId);
				const bool isManualDestroyNeeded = desc.ctor_copy != nullptr || desc.ctor_move != nullptr;
				constexpr bool isRValue = std::is_rvalue_reference_v<decltype(value)>;

				EnsureCapacity(sizeof(isManualDestroyNeeded) + sizeof(T));
				Save(isManualDestroyNeeded);
				m_data.resize(m_dataPos + sizeof(T));

				auto* pSrc = (void*)&value;
				auto* pDst = (void*)&m_data[m_dataPos];
				if (isRValue && desc.ctor_move != nullptr)
					desc.ctor_move(pSrc, pDst);
				else if (desc.ctor_copy != nullptr)
					desc.ctor_copy(pSrc, pDst);
				else
					memmove(pDst, (const void*)pSrc, sizeof(T));

				m_dataPos += sizeof(T);
			}

			//! Writes \param size bytes of data starting at the address \param pSrc to the buffer
			void Save(const void* pSrc, uint32_t size) {
				EnsureCapacity(size);

				// Copy "size" bytes of raw data starting at pSrc
				m_data.resize(m_dataPos + size);
				memcpy((void*)&m_data[m_dataPos], pSrc, size);

				m_dataPos += size;
			}

			//! Loads \param value from the buffer
			void LoadComponent(void* pDst, component::ComponentId componentId) {
				bool isManualDestroyNeeded = false;
				Load(isManualDestroyNeeded);

				const auto& desc = ComponentCache::Get().GetComponentDesc(componentId);
				GAIA_ASSERT(m_dataPos + desc.properties.size <= m_data.size());

				auto* pSrc = (void*)&m_data[m_dataPos];
				desc.Move(pSrc, pDst);
				if (isManualDestroyNeeded)
					desc.Dtor(pSrc);

				m_dataPos += desc.properties.size;
			}

			//! Loads \param value from the buffer
			template <typename T>
			void Load(T& value) {
				GAIA_ASSERT(m_dataPos + sizeof(T) <= m_data.size());

				value = utils::unaligned_ref<T>((void*)&m_data[m_dataPos]);

				m_dataPos += sizeof(T);
			}

			//! Loads \param size bytes of data from the buffer and writes them to the address \param pDst
			void Load(void* pDst, uint32_t size) {
				GAIA_ASSERT(m_dataPos + size <= m_data.size());

				memcpy(pDst, (void*)&m_data[m_dataPos], size);

				m_dataPos += size;
			}
		};

		class DataBuffer_SerializationWrapper {
			ecs::DataBuffer& m_buffer;

		public:
			DataBuffer_SerializationWrapper(ecs::DataBuffer& buffer): m_buffer(buffer) {}

			ecs::DataBuffer& buffer() {
				return m_buffer;
			}

			void reserve(uint32_t size) {
				m_buffer.EnsureCapacity(size);
			}

			void seek(uint32_t pos) {
				m_buffer.Seek(pos);
			}

			template <typename T>
			void save(T&& arg) {
				m_buffer.Save(std::forward<T>(arg));
			}

			void save(const void* pSrc, uint32_t size) {
				m_buffer.Save(pSrc, size);
			}

			template <typename T>
			void load(T& arg) {
				m_buffer.Load(arg);
			}

			void load(void* pDst, uint32_t size) {
				m_buffer.Load(pDst, size);
			}
		};
	} // namespace ecs
} // namespace gaia

#include <cinttypes>
#include <type_traits>

#define USE_HASHSET GAIA_USE_STL_CONTAINERS

#if USE_HASHSET == 1
	#include <unordered_set>
namespace gaia {
	namespace containers {
		template <typename Key>
		using set = std::unordered_set<Key>;
	} // namespace containers
} // namespace gaia
#elif USE_HASHSET == 0
	
namespace gaia {
	namespace containers {
		template <typename Key>
		using set = robin_hood::unordered_flat_set<Key>;
	} // namespace containers
} // namespace gaia
#else

	// You can add your custom container here
	#error Unsupported value used for USE_HASHSET

#endif

#include <cstdint>

namespace gaia {
	namespace ecs {
		struct ComponentGetter {
			const archetype::Chunk* m_pChunk;
			uint32_t m_idx;

			//! Returns the value stored in the component \tparam T on \param entity.
			//! \tparam T Component
			//! \return Value stored in the component.
			template <typename T>
			GAIA_NODISCARD auto GetComponent() const {
				component::VerifyComponent<T>();

				if constexpr (component::IsGenericComponent<T>)
					return m_pChunk->template GetComponent<T>(m_idx);
				else
					return m_pChunk->template GetComponent<T>();
			}

			//! Tells if \param entity contains the component \tparam T.
			//! \tparam T Component
			//! \return True if the component is present on entity.
			template <typename T>
			GAIA_NODISCARD bool HasComponent() const {
				component::VerifyComponent<T>();

				return m_pChunk->template HasComponent<T>();
			}
		};
	} // namespace ecs
} // namespace gaia

#include <cstdint>

namespace gaia {
	namespace ecs {
		struct ComponentSetter {
			archetype::Chunk* m_pChunk;
			uint32_t m_idx;

			//! Sets the value of the component \tparam T on \param entity.
			//! \tparam T Component
			//! \param value Value to set for the component
			//! \return ComponentSetter
			template <typename T, typename U = typename component::DeduceComponent<T>::Type>
			ComponentSetter& SetComponent(U&& data) {
				component::VerifyComponent<T>();

				if constexpr (component::IsGenericComponent<T>)
					m_pChunk->template SetComponent<T>(m_idx, std::forward<U>(data));
				else
					m_pChunk->template SetComponent<T>(std::forward<U>(data));
				return *this;
			}

			//! Sets the value of the component \tparam T on \param entity without trigger a world version update.
			//! \tparam T Component
			//! \param value Value to set for the component
			//! \return ComponentSetter
			template <typename T, typename U = typename component::DeduceComponent<T>::Type>
			ComponentSetter& SetComponentSilent(U&& data) {
				component::VerifyComponent<T>();

				if constexpr (component::IsGenericComponent<T>)
					m_pChunk->template SetComponentSilent<T>(m_idx, std::forward<U>(data));
				else
					m_pChunk->template SetComponentSilent<T>(std::forward<U>(data));
				return *this;
			}
		};
	} // namespace ecs
} // namespace gaia

#include <tuple>
#include <type_traits>

#include <cinttypes>
#include <type_traits>

namespace gaia {
	namespace ecs {
		namespace query {
			//! Number of components that can be a part of Query
			static constexpr uint32_t MAX_COMPONENTS_IN_QUERY = 8U;

			//! List type
			enum ListType : uint8_t { LT_None, LT_Any, LT_All, LT_Count };

			using QueryId = uint32_t;
			using LookupHash = utils::direct_hash_key<uint64_t>;
			using ComponentIdArray = containers::sarray_ext<component::ComponentId, MAX_COMPONENTS_IN_QUERY>;
			using ListTypeArray = containers::sarray_ext<ListType, MAX_COMPONENTS_IN_QUERY>;
			using ChangeFilterArray = containers::sarray_ext<component::ComponentId, MAX_COMPONENTS_IN_QUERY>;

			static constexpr QueryId QueryIdBad = (QueryId)-1;

			struct LookupCtx {
				//! Lookup hash for this query
				LookupHash hashLookup{};
				//! Query id
				QueryId queryId = QueryIdBad;

				struct Data {
					//! List of querried components
					ComponentIdArray componentIds;
					//! Filtering rules for the components
					ListTypeArray rules;
					//! List of component matcher hashes
					component::ComponentMatcherHash hash[ListType::LT_Count];
					//! Array of indiices to the last checked archetype in the component-to-archetype map
					containers::darray<uint32_t> lastMatchedArchetypeIndex;
					//! List of filtered components
					ChangeFilterArray withChanged;
					//! Read-write mask. Bit 0 stands for component 0 in component arrays.
					//! A set bit means write access is requested.
					uint8_t readWriteMask;
					//! The number of components which are required for the query to match
					uint8_t rulesAllCount;
				} data[component::ComponentType::CT_Count]{};
				static_assert(MAX_COMPONENTS_IN_QUERY == 8); // Make sure that MAX_COMPONENTS_IN_QUERY can fit into m_rw

				GAIA_NODISCARD bool operator==(const LookupCtx& other) const {
					// Fast path when cache ids are set
					if (queryId != QueryIdBad && queryId == other.queryId)
						return true;

					// Lookup hash must match
					if (hashLookup != other.hashLookup)
						return false;

					for (size_t i = 0; i < component::ComponentType::CT_Count; ++i) {
						const auto& left = data[i];
						const auto& right = other.data[i];

						// Check array sizes first
						if (left.componentIds.size() != right.componentIds.size())
							return false;
						if (left.rules.size() != right.rules.size())
							return false;
						if (left.withChanged.size() != right.withChanged.size())
							return false;
						if (left.readWriteMask != right.readWriteMask)
							return false;

						// Matches hashes need to be the same
						for (size_t j = 0; j < ListType::LT_Count; ++j) {
							if (left.hash[j] != right.hash[j])
								return false;
						}

						// Components need to be the same
						for (size_t j = 0; j < left.componentIds.size(); ++j) {
							if (left.componentIds[j] != right.componentIds[j])
								return false;
						}

						// Rules need to be the same
						for (size_t j = 0; j < left.rules.size(); ++j) {
							if (left.rules[j] != right.rules[j])
								return false;
						}

						// Filters need to be the same
						for (size_t j = 0; j < left.withChanged.size(); ++j) {
							if (left.withChanged[j] != right.withChanged[j])
								return false;
						}
					}

					return true;
				}

				GAIA_NODISCARD bool operator!=(const LookupCtx& other) const {
					return !operator==(other);
				};
			};

			//! Sorts internal component arrays
			inline void SortComponentArrays(LookupCtx& ctx) {
				for (size_t i = 0; i < component::ComponentType::CT_Count; ++i) {
					auto& data = ctx.data[i];
					// Make sure the read-write mask remains correct after sorting
					utils::sort(data.componentIds, component::SortComponentCond{}, [&](size_t left, size_t right) {
						utils::swap(data.componentIds[left], data.componentIds[right]);
						utils::swap(data.rules[left], data.rules[right]);

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

			inline void CalculateMatcherHashes(LookupCtx& ctx) {
				// Sort the arrays if necessary
				SortComponentArrays(ctx);

				// Calculate the matcher hash
				for (auto& data: ctx.data) {
					for (size_t i = 0; i < data.rules.size(); ++i)
						component::CalculateMatcherHash(data.hash[data.rules[i]], data.componentIds[i]);
				}
			}

			inline void CalculateLookupHash(LookupCtx& ctx) {
				// Make sure we don't calculate the hash twice
				GAIA_ASSERT(ctx.hashLookup.hash == 0);

				LookupHash::Type hashLookup = 0;

				for (size_t i = 0; i < component::ComponentType::CT_Count; ++i) {
					auto& data = ctx.data[i];

					// Components
					{
						LookupHash::Type hash = 0;

						const auto& componentIds = data.componentIds;
						for (const auto componentId: componentIds)
							hash = utils::hash_combine(hash, (LookupHash::Type)componentId);
						hash = utils::hash_combine(hash, (LookupHash::Type)componentIds.size());

						hash = utils::hash_combine(hash, (LookupHash::Type)data.readWriteMask);
						hashLookup = utils::hash_combine(hashLookup, hash);
					}

					// Rules
					{
						LookupHash::Type hash = 0;

						const auto& rules = data.withChanged;
						for (auto listType: rules)
							hash = utils::hash_combine(hash, (LookupHash::Type)listType);
						hash = utils::hash_combine(hash, (LookupHash::Type)rules.size());

						hashLookup = utils::hash_combine(hashLookup, hash);
					}

					// Filters
					{
						LookupHash::Type hash = 0;

						const auto& withChanged = data.withChanged;
						for (auto componentId: withChanged)
							hash = utils::hash_combine(hash, (LookupHash::Type)componentId);
						hash = utils::hash_combine(hash, (LookupHash::Type)withChanged.size());

						hashLookup = utils::hash_combine(hashLookup, hash);
					}
				}

				ctx.hashLookup = {utils::calculate_hash64(hashLookup)};
			}
		} // namespace query
	} // namespace ecs
} // namespace gaia

namespace gaia {
	namespace ecs {
		struct Entity;

		namespace query {
			using ComponentToArchetypeMap = containers::map<component::ComponentId, archetype::ArchetypeList>;

			class QueryInfo {
			public:
				//! Query matching result
				enum class MatchArchetypeQueryRet : uint8_t { Fail, Ok, Skip };

			private:
				//! Lookup context
				query::LookupCtx m_lookupCtx;
				//! List of archetypes matching the query
				archetype::ArchetypeList m_archetypeCache;
				//! Index of the last archetype in the world we checked
				uint32_t m_lastArchetypeIdx = 1; // skip the root archetype
				//! Version of the world for which the query has been called most recently
				uint32_t m_worldVersion = 0;

				template <typename T>
				bool HasComponent_Internal(
						[[maybe_unused]] query::ListType listType, [[maybe_unused]] component::ComponentType componentType,
						bool isReadWrite) const {
					if constexpr (std::is_same_v<T, Entity>) {
						// Skip Entity input args
						return true;
					} else {
						const auto& data = m_lookupCtx.data[componentType];
						const auto& componentIds = data.componentIds;

						// Component id has to be present
						const auto componentId = component::GetComponentId<T>();
						GAIA_ASSERT(utils::has(componentIds, componentId));
						const auto idx = utils::get_index_unsafe(componentIds, componentId);
						if (listType != query::ListType::LT_Count && listType != data.rules[idx])
							return false;

						// Read-write mask must match
						const uint32_t maskRW = (uint32_t)data.readWriteMask & (1U << (uint32_t)idx);
						const uint32_t maskXX = (uint32_t)isReadWrite << idx;
						return maskRW == maskXX;
					}
				}

				template <typename T>
				bool HasComponent_Internal(query::ListType listType) const {
					using U = typename component::DeduceComponent<T>::Type;
					using UOriginal = typename component::DeduceComponent<T>::TypeOriginal;
					using UOriginalPR = std::remove_reference_t<std::remove_pointer_t<UOriginal>>;
					constexpr bool isReadWrite =
							std::is_same_v<U, UOriginal> || (!std::is_const_v<UOriginalPR> && !std::is_empty_v<U>);

					if constexpr (component::IsGenericComponent<T>)
						return HasComponent_Internal<U>(listType, component::ComponentType::CT_Generic, isReadWrite);
					else
						return HasComponent_Internal<U>(listType, component::ComponentType::CT_Chunk, isReadWrite);
				}

				template <typename Func>
				GAIA_NODISCARD bool CheckMatch_Internal(
						component::ComponentType componentType, const archetype::ComponentIdArray& archetypeComponentIds,
						ListType listType, Func func) const {
					const auto& data = m_lookupCtx.data[componentType];

					// Arrays are sorted so we can do linear intersection lookup
					size_t i = 0;
					size_t j = 0;
					while (i < archetypeComponentIds.size() && j < data.componentIds.size()) {
						if (data.rules[j] == listType) {
							const auto componentIdArchetype = archetypeComponentIds[i];
							const auto componentIdQuery = data.componentIds[j];

							if (componentIdArchetype == componentIdQuery && func(componentIdArchetype, componentIdQuery))
								return true;

							if (component::SortComponentCond{}.operator()(componentIdArchetype, componentIdQuery)) {
								++i;
								continue;
							}
						}
						++j;
					}

					return false;
				}

				//! Tries to match component ids in \param componentIdsQuery with those in \param componentIds.
				//! \return True if there is a match, false otherwise.
				GAIA_NODISCARD bool CheckMatchOne(
						component::ComponentType componentType, const archetype::ComponentIdArray& archetypeComponentIds,
						ListType listType) const {
					return CheckMatch_Internal(
							componentType, archetypeComponentIds, listType,
							[](component::ComponentId componentId, component::ComponentId componentIdQuery) {
								return componentId == componentIdQuery;
							});
				}

				//! Tries to match all component ids in \param componentIdsQuery with those in \param componentIds.
				//! \return True if there is a match, false otherwise.
				GAIA_NODISCARD bool CheckMatchAll(
						component::ComponentType componentType, const archetype::ComponentIdArray& archetypeComponentIds) const {
					size_t matches = 0;
					const auto& data = m_lookupCtx.data[componentType];
					return CheckMatch_Internal(
							componentType, archetypeComponentIds, ListType::LT_All,
							[&](component::ComponentId componentId, component::ComponentId componentIdQuery) {
								return componentId == componentIdQuery && (++matches == data.rulesAllCount);
							});
				}

				//! Tries to match component with component type \param componentType from the archetype \param archetype with
				//! the query. \return MatchArchetypeQueryRet::Fail if there is no match, MatchArchetypeQueryRet::Ok for match
				//! or MatchArchetypeQueryRet::Skip is not relevant.
				GAIA_NODISCARD MatchArchetypeQueryRet
				Match(const archetype::Archetype& archetype, component::ComponentType componentType) const {
					const auto& matcherHash = archetype.GetMatcherHash(componentType);
					const auto& data = GetData(componentType);

					const auto withNoneTest = matcherHash.hash & data.hash[query::ListType::LT_None].hash;
					const auto withAnyTest = matcherHash.hash & data.hash[query::ListType::LT_Any].hash;
					const auto withAllTest = matcherHash.hash & data.hash[query::ListType::LT_All].hash;

					// If withAnyTest is empty but we wanted something
					if (withAnyTest == 0 && data.hash[query::ListType::LT_Any].hash != 0)
						return MatchArchetypeQueryRet::Fail;

					// If withAllTest is empty but we wanted something
					if (withAllTest == 0 && data.hash[query::ListType::LT_All].hash != 0)
						return MatchArchetypeQueryRet::Fail;

					const auto& archetypeComponentIds = archetype.GetComponentIdArray(componentType);

					// If there is any match with withNoneList we quit
					if (withNoneTest != 0) {
						if (CheckMatchOne(componentType, archetypeComponentIds, query::ListType::LT_None))
							return MatchArchetypeQueryRet::Fail;
					}

					// If there is any match with withAnyTest
					if (withAnyTest != 0) {
						if (CheckMatchOne(componentType, archetypeComponentIds, query::ListType::LT_Any))
							goto checkWithAllMatches;

						// At least one match necessary to continue
						return MatchArchetypeQueryRet::Fail;
					}

				checkWithAllMatches:
					// If withAllList is not empty there has to be an exact match
					if (withAllTest != 0) {
						if (
								// We can't search for more components than there are no the archetype itself
								data.rulesAllCount <= archetypeComponentIds.size() &&
								// Match everything with LT_ALL
								CheckMatchAll(componentType, archetypeComponentIds))
							return MatchArchetypeQueryRet::Ok;

						// No match found. We're done
						return MatchArchetypeQueryRet::Fail;
					}

					return (withAnyTest != 0) ? MatchArchetypeQueryRet::Ok : MatchArchetypeQueryRet::Skip;
				}

			public:
				GAIA_NODISCARD static QueryInfo Create(QueryId id, query::LookupCtx&& ctx) {
					QueryInfo info;
					query::CalculateMatcherHashes(ctx);
					info.m_lookupCtx = std::move(ctx);
					info.m_lookupCtx.queryId = id;
					return info;
				}

				void SetWorldVersion(uint32_t version) {
					m_worldVersion = version;
				}

				GAIA_NODISCARD uint32_t GetWorldVersion() const {
					return m_worldVersion;
				}

				query::LookupHash GetLookupHash() const {
					return m_lookupCtx.hashLookup;
				}

				GAIA_NODISCARD bool operator==(const query::LookupCtx& other) const {
					return m_lookupCtx == other;
				}

				GAIA_NODISCARD bool operator!=(const query::LookupCtx& other) const {
					return !operator==(other);
				}

				//! Tries to match the query against archetypes in \param componentToArchetypeMap. For each matched archetype
				//! the archetype is cached. This is necessary so we do not iterate all chunks over and over again when running
				//! queries.
				void Match(const ComponentToArchetypeMap& componentToArchetypeMap, uint32_t archetypeCount) {
					static containers::set<archetype::Archetype*> s_tmpArchetypeMatches;

					// Skip if no new archetype appeared
					if (m_lastArchetypeIdx == archetypeCount)
						return;
					m_lastArchetypeIdx = archetypeCount;

					// Match against generic types
					{
						auto& data = m_lookupCtx.data[component::ComponentType::CT_Generic];
						for (size_t i = 0; i < data.componentIds.size(); ++i) {
							const auto componentId = data.componentIds[i];

							const auto it = componentToArchetypeMap.find(componentId);
							if (it == componentToArchetypeMap.end())
								continue;

							for (size_t j = data.lastMatchedArchetypeIndex[i]; j < it->second.size(); ++j) {
								auto* pArchetype = it->second[j];
								// Early exit if generic query doesn't match
								const auto retGeneric = Match(*pArchetype, component::ComponentType::CT_Generic);
								if (retGeneric == MatchArchetypeQueryRet::Fail)
									continue;

								(void)s_tmpArchetypeMatches.emplace(pArchetype);
							}
							data.lastMatchedArchetypeIndex[i] = (uint32_t)it->second.size();
						}
					}

					// Match against chunk types
					{
						auto& data = m_lookupCtx.data[component::ComponentType::CT_Chunk];
						for (size_t i = 0; i < data.componentIds.size(); ++i) {
							const auto componentId = data.componentIds[i];

							const auto it = componentToArchetypeMap.find(componentId);
							if (it == componentToArchetypeMap.end())
								continue;

							for (size_t j = data.lastMatchedArchetypeIndex[i]; j < it->second.size(); ++j) {
								auto* pArchetype = it->second[j];
								// Early exit if generic query doesn't match
								const auto retGeneric = Match(*pArchetype, component::ComponentType::CT_Chunk);
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

				GAIA_NODISCARD const query::LookupCtx::Data& GetData(component::ComponentType componentType) const {
					return m_lookupCtx.data[componentType];
				}

				GAIA_NODISCARD const query::ComponentIdArray& GetComponentIds(component::ComponentType componentType) const {
					return m_lookupCtx.data[componentType].componentIds;
				}

				GAIA_NODISCARD const query::ChangeFilterArray& GetFiltered(component::ComponentType componentType) const {
					return m_lookupCtx.data[componentType].withChanged;
				}

				GAIA_NODISCARD bool HasFilters() const {
					return !m_lookupCtx.data[component::ComponentType::CT_Generic].withChanged.empty() ||
								 !m_lookupCtx.data[component::ComponentType::CT_Chunk].withChanged.empty();
				}

				template <typename... T>
				bool Has() const {
					return (HasComponent_Internal<T>(query::ListType::LT_Count) || ...);
				}

				template <typename... T>
				bool HasAny() const {
					return (HasComponent_Internal<T>(query::ListType::LT_Any) || ...);
				}

				template <typename... T>
				bool HasAll() const {
					return (HasComponent_Internal<T>(query::ListType::LT_All) && ...);
				}

				template <typename... T>
				bool HasNone() const {
					return (!HasComponent_Internal<T>(query::ListType::LT_None) && ...);
				}

				GAIA_NODISCARD archetype::ArchetypeList::iterator begin() {
					return m_archetypeCache.begin();
				}

				GAIA_NODISCARD archetype::ArchetypeList::const_iterator begin() const {
					return m_archetypeCache.cbegin();
				}

				GAIA_NODISCARD archetype::ArchetypeList::iterator end() {
					return m_archetypeCache.end();
				}

				GAIA_NODISCARD archetype::ArchetypeList::const_iterator end() const {
					return m_archetypeCache.cend();
				}
			};
		} // namespace query
	} // namespace ecs
} // namespace gaia

namespace gaia {
	namespace ecs {
		struct Iterator {
		private:
			query::QueryInfo& m_info;
			archetype::Chunk& m_chunk;
			uint32_t m_pos;

		public:
			Iterator(query::QueryInfo& info, archetype::Chunk& chunk): m_info(info), m_chunk(chunk), m_pos(0) {}
			Iterator(query::QueryInfo& info, archetype::Chunk& chunk, uint32_t pos):
					m_info(info), m_chunk(chunk), m_pos(pos) {}

			uint32_t operator*() const {
				return m_pos;
			}

			uint32_t operator->() const {
				return m_pos;
			}

			Iterator& operator++() {
				++m_pos;
				return *this;
			}

			GAIA_NODISCARD Iterator operator++(int) {
				Iterator temp(*this);
				++*this;
				return temp;
			}

			GAIA_NODISCARD bool operator==(const Iterator& other) const {
				return m_pos == other.m_pos;
			}

			GAIA_NODISCARD bool operator!=(const Iterator& other) const {
				return m_pos != other.m_pos;
			}

			GAIA_NODISCARD Iterator begin() const {
				return *this;
			}

			GAIA_NODISCARD Iterator end() const {
				return {m_info, m_chunk, m_chunk.GetEntityCount()};
			}

			GAIA_NODISCARD uint32_t size() const {
				return m_chunk.GetEntityCount();
			}

			//! Checks if the entity at the current iterator index is enabled.
			//! \return True it the entity is enabled. False otherwise.
			bool IsEntityEnabled() const {
				return !m_chunk.GetDisabledEntityMask().test(m_pos);
			}

			//! Checks if component \tparam T is present in the chunk.
			//! \tparam T Component
			//! \return True if the component is present. False otherwise.
			template <typename T>
			GAIA_NODISCARD bool HasComponent() const {
				return m_chunk.HasComponent<T>();
			}

			//! Returns a read-only entity or component view.
			//! \warning If \tparam T is a component it is expected it is present. Undefined behavior otherwise.
			//! \tparam T Component or Entity
			//! \return Entity of component view with read-only access
			template <typename T>
			GAIA_NODISCARD auto View() const {
#if GAIA_DEBUG
				if constexpr (component::IsGenericComponent<T>) {
					using U = typename component::DeduceComponent<T>::Type;
					using UConst = std::add_const_t<U>;
					GAIA_ASSERT(m_info.Has<UConst>());
				} else {
					using U = typename component::DeduceComponent<T>::Type;
					using UConst = std::add_const_t<U>;
					using UChunk = AsChunk<UConst>;
					GAIA_ASSERT(m_info.Has<UChunk>());
				}
#endif

				return m_chunk.View<T>();
			}

			//! Returns a mutable entity or component view.
			//! \warning If \tparam T is a component it is expected it is present. Undefined behavior otherwise.
			//! \tparam T Component or Entity
			//! \return Entity or component view with read-write access
			template <typename T>
			GAIA_NODISCARD auto ViewRW() {
				GAIA_ASSERT(m_info.Has<T>());
				return m_chunk.ViewRW<T>();
			}

			//! Returns a mutable component view.
			//! Doesn't update the world version when the access is aquired.
			//! \warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			//! \tparam T Component
			//! \return Component view with read-write access
			template <typename T>
			GAIA_NODISCARD auto ViewRWSilent() {
				GAIA_ASSERT(m_info.Has<T>());
				return m_chunk.ViewRWSilent<T>();
			}
		};
	} // namespace ecs
} // namespace gaia

#include <cinttypes>
#include <type_traits>

namespace gaia {
	namespace ecs {
		struct IteratorDisabled {
		private:
			using Mask = archetype::ChunkHeader::DisabledEntityMask;
			using Iter = Mask::const_iterator;

			query::QueryInfo& m_info;
			archetype::Chunk& m_chunk;
			Mask m_mask;
			Iter m_iter;

			IteratorDisabled(query::QueryInfo& info, archetype::Chunk& chunk, uint32_t pos):
					m_info(info), m_chunk(chunk), m_mask(chunk.GetDisabledEntityMask()), m_iter(m_mask, pos, false) {}

		public:
			IteratorDisabled(query::QueryInfo& info, archetype::Chunk& chunk):
					m_info(info), m_chunk(chunk), m_mask(chunk.GetDisabledEntityMask()), m_iter(m_mask, 0, true) {}

			uint32_t operator*() const {
				return *m_iter;
			}

			uint32_t operator->() const {
				return *m_iter;
			}

			IteratorDisabled& operator++() {
				++m_iter;
				return *this;
			}

			GAIA_NODISCARD IteratorDisabled operator++(int) {
				IteratorDisabled temp(*this);
				++*this;
				return temp;
			}

			GAIA_NODISCARD bool operator==(const IteratorDisabled& other) const {
				return m_iter == other.m_iter;
			}

			GAIA_NODISCARD bool operator!=(const IteratorDisabled& other) const {
				return m_iter != other.m_iter;
			}

			GAIA_NODISCARD IteratorDisabled begin() const {
				return *this;
			}

			GAIA_NODISCARD IteratorDisabled end() const {
				return {m_info, m_chunk, m_chunk.GetEntityCount()};
			}

			GAIA_NODISCARD uint32_t size() const {
				return m_mask.count();
			}

			//! Checks if component \tparam T is present in the chunk.
			//! \tparam T Component
			//! \return True if the component is present. False otherwise.
			template <typename T>
			GAIA_NODISCARD bool HasComponent() const {
				return m_chunk.HasComponent<T>();
			}

			//! Sets the value of the component \tparam T on \param entity.
			//! \tparam T Component
			//! \param value Value to set for the component
			//! \return ComponentSetter
			template <typename T, typename U = typename component::DeduceComponent<T>::Type>
			ComponentSetter& SetComponent(U&& data) {
				ComponentSetter setter{&m_chunk, *m_iter};
				return setter.SetComponent<T, U>(std::forward<U>(data));
			}

			//! Sets the value of the component \tparam T on \param entity without trigger a world version update.
			//! \tparam T Component
			//! \param value Value to set for the component
			//! \return ComponentSetter
			template <typename T, typename U = typename component::DeduceComponent<T>::Type>
			ComponentSetter& SetComponentSilent(U&& data) {
				ComponentSetter setter{&m_chunk, *m_iter};
				return setter.SetComponentSilent<T, U>(std::forward<U>(data));
			}
		};
	} // namespace ecs
} // namespace gaia

#include <cinttypes>
#include <type_traits>

namespace gaia {
	namespace ecs {
		struct IteratorEnabled {
		private:
			using Mask = archetype::ChunkHeader::DisabledEntityMask;
			using Iter = Mask::const_iterator;

			query::QueryInfo& m_info;
			archetype::Chunk& m_chunk;
			Mask m_mask;
			Iter m_iter;

			IteratorEnabled(query::QueryInfo& info, archetype::Chunk& chunk, uint32_t pos):
					m_info(info), m_chunk(chunk), m_mask(chunk.GetDisabledEntityMask()),
					m_iter(m_mask.flip(0, chunk.HasEntities() ? chunk.GetEntityCount() - 1 : 0), pos, false) {}

		public:
			IteratorEnabled(query::QueryInfo& info, archetype::Chunk& chunk):
					m_info(info), m_chunk(chunk), m_mask(chunk.GetDisabledEntityMask()),
					m_iter(m_mask.flip(0, chunk.HasEntities() ? chunk.GetEntityCount() - 1 : 0), 0, true) {}

			uint32_t operator*() const {
				return *m_iter;
			}

			uint32_t operator->() const {
				return *m_iter;
			}

			IteratorEnabled& operator++() {
				++m_iter;
				return *this;
			}

			GAIA_NODISCARD IteratorEnabled operator++(int) {
				IteratorEnabled temp(*this);
				++*this;
				return temp;
			}

			GAIA_NODISCARD bool operator==(const IteratorEnabled& other) const {
				return m_iter == other.m_iter;
			}

			GAIA_NODISCARD bool operator!=(const IteratorEnabled& other) const {
				return m_iter != other.m_iter;
			}

			GAIA_NODISCARD IteratorEnabled begin() const {
				return *this;
			}

			GAIA_NODISCARD IteratorEnabled end() const {
				return {m_info, m_chunk, m_chunk.GetEntityCount()};
			}

			GAIA_NODISCARD uint32_t size() const {
				return m_mask.count();
			}

			//! Checks if component \tparam T is present in the chunk.
			//! \tparam T Component
			//! \return True if the component is present. False otherwise.
			template <typename T>
			GAIA_NODISCARD bool HasComponent() const {
				return ComponentGetter{&m_chunk, *m_iter}.HasComponent<T>();
			}

			//! Returns the value stored in the component \tparam T on \param entity.
			//! \tparam T Component
			//! \return Value stored in the component.
			template <typename T>
			GAIA_NODISCARD auto GetComponent() const {
				return ComponentGetter{&m_chunk, *m_iter}.GetComponent<T>();
			}

			//! Sets the value of the component \tparam T on \param entity.
			//! \tparam T Component
			//! \param value Value to set for the component
			//! \return ComponentSetter
			template <typename T, typename U = typename component::DeduceComponent<T>::Type>
			ComponentSetter& SetComponent(U&& data) {
				return ComponentSetter{&m_chunk, *m_iter}.SetComponent<T, U>(std::forward<U>(data));
			}

			//! Sets the value of the component \tparam T on \param entity without trigger a world version update.
			//! \tparam T Component
			//! \param value Value to set for the component
			//! \return ComponentSetter
			template <typename T, typename U = typename component::DeduceComponent<T>::Type>
			ComponentSetter& SetComponentSilent(U&& data) {
				return ComponentSetter{&m_chunk, *m_iter}.SetComponentSilent<T, U>(std::forward<U>(data));
			}
		};
	} // namespace ecs
} // namespace gaia

namespace gaia {
	namespace ecs {
		class QueryCache {
			using QueryCacheLookupArray = containers::darray<uint32_t>;

			containers::map<query::LookupHash, QueryCacheLookupArray> m_queryCache;
			containers::darray<query::QueryInfo> m_queryArr;

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
			query::QueryInfo& Get(query::QueryId queryId) {
				return m_queryArr[queryId];
			};

			//! Registers the provided query lookup context \param ctx. If it already exists it is returned.
			//! \return Query id
			uint32_t GetOrCreate(query::LookupCtx&& ctx) {
				GAIA_ASSERT(ctx.hashLookup.hash != 0);

				// Check if the query info exists first
				auto ret = m_queryCache.try_emplace(ctx.hashLookup, QueryCacheLookupArray{});
				if (!ret.second) {
					const auto& queryIds = ret.first->second;

					// Record with the query info lookup hash exists but we need to check if the query itself is a part of it.
					if GAIA_LIKELY (ctx.queryId != query::QueryIdBad) {
						// Make sure the same hash gets us to the proper query
						for (const auto queryId: queryIds) {
							const auto& queryInfo = m_queryArr[queryId];
							if (queryInfo != ctx)
								continue;

							return queryId;
						}

						GAIA_ASSERT(false && "QueryInfo not found despite having its lookupHash and cacheId set!");
						return query::QueryIdBad;
					}
				}

				const auto queryId = (query::QueryId)m_queryArr.size();
				m_queryArr.push_back(query::QueryInfo::Create(queryId, std::move(ctx)));
				ret.first->second.push_back(queryId);
				return queryId;
			};
		};
	} // namespace ecs
} // namespace gaia

namespace gaia {
	namespace ecs {
		enum class ExecutionMode : uint8_t {
			//! Run on the main thread
			Run,
			//! Run on a single worker thread
			Single,
			//! Run on as many worker threads as possible
			Parallel
		};

		class Query final {
			static constexpr uint32_t ChunkBatchSize = 16;
			using CChunkSpan = std::span<const archetype::Chunk*>;
			using ChunkBatchedList = containers::sarray_ext<archetype::Chunk*, ChunkBatchSize>;
			using CmdBufferCmdFunc = void (*)(DataBuffer& buffer, query::LookupCtx& ctx);

		public:
			//! Query constraints
			enum class Constraints : uint8_t { EnabledOnly, DisabledOnly, AcceptAll };

		private:
			//! Command buffer command type
			enum CommandBufferCmd : uint8_t { ADD_COMPONENT, ADD_FILTER };

			struct Command_AddComponent {
				static constexpr CommandBufferCmd Id = CommandBufferCmd::ADD_COMPONENT;

				component::ComponentId componentId;
				component::ComponentType componentType;
				query::ListType listType;
				bool isReadWrite;

				void Exec(query::LookupCtx& ctx) const {
					auto& data = ctx.data[componentType];
					auto& componentIds = data.componentIds;
					auto& lastMatchedArchetypeIndex = data.lastMatchedArchetypeIndex;
					auto& rules = data.rules;

					// Unique component ids only
					GAIA_ASSERT(!utils::has(componentIds, componentId));

#if GAIA_DEBUG
					// There's a limit to the amount of components which we can store
					if (componentIds.size() >= query::MAX_COMPONENTS_IN_QUERY) {
						GAIA_ASSERT(false && "Trying to create an ECS query with too many components!");

						const auto& cc = ComponentCache::Get();
						auto componentName = cc.GetComponentDesc(componentId).name;
						GAIA_LOG_E(
								"Trying to add ECS component '%.*s' to an already full ECS query!", (uint32_t)componentName.size(),
								componentName.data());

						return;
					}
#endif

					data.readWriteMask |= (uint8_t)isReadWrite << (uint8_t)componentIds.size();
					componentIds.push_back(componentId);
					lastMatchedArchetypeIndex.push_back(0);
					rules.push_back(listType);

					if (listType == query::ListType::LT_All)
						++data.rulesAllCount;
				}
			};

			struct Command_Filter {
				static constexpr CommandBufferCmd Id = CommandBufferCmd::ADD_FILTER;

				component::ComponentId componentId;
				component::ComponentType componentType;

				void Exec(query::LookupCtx& ctx) const {
					auto& data = ctx.data[componentType];
					auto& componentIds = data.componentIds;
					auto& withChanged = data.withChanged;
					const auto& rules = data.rules;

					GAIA_ASSERT(utils::has(componentIds, componentId));
					GAIA_ASSERT(!utils::has(withChanged, componentId));

#if GAIA_DEBUG
					// There's a limit to the amount of components which we can store
					if (withChanged.size() >= query::MAX_COMPONENTS_IN_QUERY) {
						GAIA_ASSERT(false && "Trying to create an ECS filter query with too many components!");

						const auto& cc = ComponentCache::Get();
						auto componentName = cc.GetComponentDesc(componentId).name;
						GAIA_LOG_E(
								"Trying to add ECS component %.*s to an already full filter query!", (uint32_t)componentName.size(),
								componentName.data());
						return;
					}
#endif

					const auto componentIdx = utils::get_index_unsafe(componentIds, componentId);

					// Component has to be present in anyList or allList.
					// NoneList makes no sense because we skip those in query processing anyway.
					if (rules[componentIdx] != query::ListType::LT_None) {
						withChanged.push_back(componentId);
						return;
					}

					GAIA_ASSERT(false && "SetChangeFilter trying to filter ECS component which is not a part of the query");
#if GAIA_DEBUG
					const auto& cc = ComponentCache::Get();
					auto componentName = cc.GetComponentDesc(componentId).name;
					GAIA_LOG_E(
							"SetChangeFilter trying to filter ECS component %.*s but "
							"it's not a part of the query!",
							(uint32_t)componentName.size(), componentName.data());
#endif
				}
			};

			static constexpr CmdBufferCmdFunc CommandBufferRead[] = {
					// Add component
					[](DataBuffer& buffer, query::LookupCtx& ctx) {
						Command_AddComponent cmd;
						DataBuffer_SerializationWrapper s(buffer);
						serialization::load(s, cmd);
						cmd.Exec(ctx);
					},
					// Add filter
					[](DataBuffer& buffer, query::LookupCtx& ctx) {
						Command_Filter cmd;
						DataBuffer_SerializationWrapper s(buffer);
						serialization::load(s, cmd);
						cmd.Exec(ctx);
					}};

			DataBuffer m_cmdBuffer;
			//! Query cache id
			query::QueryId m_queryId = query::QueryIdBad;

			//! Query cache (stable pointer to parent world's query cache)
			QueryCache* m_entityQueryCache{};
			//! World version (stable pointer to parent world's world version)
			uint32_t* m_worldVersion{};
			//! List of achetypes (stable pointer to parent world's archetype array)
			const archetype::ArchetypeList* m_archetypes{};
			//! Map of component ids to archetypes (stable pointer to parent world's archetype component-to-archetype map)
			const query::ComponentToArchetypeMap* m_componentToArchetypeMap{};
			//! Execution mode
			ExecutionMode m_executionMode = ExecutionMode::Run;

			//--------------------------------------------------------------------------------
		public:
			query::QueryInfo& FetchQueryInfo() {
				// Lookup hash is present which means QueryInfo was already found
				if GAIA_LIKELY (m_queryId != query::QueryIdBad) {
					auto& queryInfo = m_entityQueryCache->Get(m_queryId);
					queryInfo.Match(*m_componentToArchetypeMap, (uint32_t)m_archetypes->size());
					return queryInfo;
				}

				// No lookup hash is present which means QueryInfo needes to fetched or created
				query::LookupCtx ctx;
				Commit(ctx);
				m_queryId = m_entityQueryCache->GetOrCreate(std::move(ctx));
				auto& queryInfo = m_entityQueryCache->Get(m_queryId);
				queryInfo.Match(*m_componentToArchetypeMap, (uint32_t)m_archetypes->size());
				return queryInfo;
			}

			//--------------------------------------------------------------------------------
		private:
			template <typename T>
			void AddComponent_Internal(query::ListType listType) {
				using U = typename component::DeduceComponent<T>::Type;
				using UOriginal = typename component::DeduceComponent<T>::TypeOriginal;
				using UOriginalPR = std::remove_reference_t<std::remove_pointer_t<UOriginal>>;

				const auto componentId = component::GetComponentId<T>();
				constexpr auto componentType = component::IsGenericComponent<T> ? component::ComponentType::CT_Generic
																																				: component::ComponentType::CT_Chunk;
				constexpr bool isReadWrite =
						std::is_same_v<U, UOriginal> || (!std::is_const_v<UOriginalPR> && !std::is_empty_v<U>);

				// Make sure the component is always registered
				auto& cc = ComponentCache::Get();
				(void)cc.GetOrCreateComponentInfo<T>();

				Command_AddComponent cmd{componentId, componentType, listType, isReadWrite};
				DataBuffer_SerializationWrapper s(m_cmdBuffer);
				serialization::save(s, Command_AddComponent::Id);
				serialization::save(s, cmd);
			}

			template <typename T>
			void WithChanged_Internal() {
				const auto componentId = component::GetComponentId<T>();
				constexpr auto componentType = component::IsGenericComponent<T> ? component::ComponentType::CT_Generic
																																				: component::ComponentType::CT_Chunk;

				Command_Filter cmd{componentId, componentType};
				DataBuffer_SerializationWrapper s(m_cmdBuffer);
				serialization::save(s, Command_Filter::Id);
				serialization::save(s, cmd);
			}

			//--------------------------------------------------------------------------------

			void Commit(query::LookupCtx& ctx) {
				GAIA_ASSERT(m_queryId == query::QueryIdBad);
				DataBuffer_SerializationWrapper s(m_cmdBuffer);

				// Read data from buffer and execute the command stored in it
				m_cmdBuffer.Seek(0);
				while (m_cmdBuffer.GetPos() < m_cmdBuffer.Size()) {
					CommandBufferCmd id{};
					serialization::load(s, id);
					CommandBufferRead[id](m_cmdBuffer, ctx);
				}

				// Calculate the lookup hash from the provided context
				query::CalculateLookupHash(ctx);

				// We can free all temporary data now
				m_cmdBuffer.Reset();
			}

			//--------------------------------------------------------------------------------

			//! Unpacks the parameter list \param types into query \param query and performs All for each of them
			template <typename... T>
			void UnpackArgsIntoQuery_All(Query& query, [[maybe_unused]] utils::func_type_list<T...> types) const {
				static_assert(sizeof...(T) > 0, "Inputs-less functors can not be unpacked to query");
				query.All<T...>();
			}

			//! Unpacks the parameter list \param types into query \param query and performs HasAll for each of them
			template <typename... T>
			GAIA_NODISCARD bool UnpackArgsIntoQuery_HasAll(
					const query::QueryInfo& queryInfo, [[maybe_unused]] utils::func_type_list<T...> types) const {
				if constexpr (sizeof...(T) > 0)
					return queryInfo.HasAll<T...>();
				else
					return true;
			}

			//--------------------------------------------------------------------------------

			GAIA_NODISCARD static bool CheckFilters(const archetype::Chunk& chunk, const query::QueryInfo& queryInfo) {
				GAIA_ASSERT(chunk.HasEntities() && "CheckFilters called on an empty chunk");

				const auto queryVersion = queryInfo.GetWorldVersion();

				// See if any generic component has changed
				{
					const auto& filtered = queryInfo.GetFiltered(component::ComponentType::CT_Generic);
					for (const auto componentId: filtered) {
						const auto componentIdx = chunk.GetComponentIdx(component::ComponentType::CT_Generic, componentId);
						if (chunk.DidChange(component::ComponentType::CT_Generic, queryVersion, componentIdx))
							return true;
					}
				}

				// See if any chunk component has changed
				{
					const auto& filtered = queryInfo.GetFiltered(component::ComponentType::CT_Chunk);
					for (const auto componentId: filtered) {
						const uint32_t componentIdx = chunk.GetComponentIdx(component::ComponentType::CT_Chunk, componentId);
						if (chunk.DidChange(component::ComponentType::CT_Chunk, queryVersion, componentIdx))
							return true;
					}
				}

				// Skip unchanged chunks.
				return false;
			}

			template <bool HasFilters>
			GAIA_NODISCARD bool
			CanAcceptChunkForProcessing(const archetype::Chunk& chunk, const query::QueryInfo& queryInfo) const {
				if GAIA_UNLIKELY (!chunk.HasEntities())
					return false;

				if constexpr (HasFilters) {
					if (!CheckFilters(chunk, queryInfo))
						return false;
				}

				return true;
			}

			//! Execute functors in batches
			template <typename Func>
			static void ChunkBatch_Perform(Func func, ChunkBatchedList& chunks) {
				GAIA_ASSERT(!chunks.empty());

				// This is what the function is doing:
				// for (auto *pChunk: chunks)
				//	func(*pChunk);
				// chunks.clear();

				GAIA_PROF_SCOPE(ChunkBatch_Perform);

				// We only have one chunk to process
				if GAIA_UNLIKELY (chunks.size() == 1) {
					chunks[0]->SetStructuralChanges(true);
					func(*chunks[0]);
					chunks[0]->SetStructuralChanges(false);
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
				chunks[0]->SetStructuralChanges(true);
				func(*chunks[0]);
				chunks[0]->SetStructuralChanges(false);

				size_t chunkIdx = 1;
				for (; chunkIdx < chunks.size() - 1; ++chunkIdx) {
					gaia::prefetch(&chunks[chunkIdx + 1], PrefetchHint::PREFETCH_HINT_T2);
					chunks[chunkIdx]->SetStructuralChanges(true);
					func(*chunks[chunkIdx]);
					chunks[chunkIdx]->SetStructuralChanges(false);
				}

				chunks[chunkIdx]->SetStructuralChanges(true);
				func(*chunks[chunkIdx]);
				chunks[chunkIdx]->SetStructuralChanges(false);

				chunks.clear();
			}

			template <bool HasFilters, typename Func>
			void ProcessQueryOnChunks_NoConstraints(
					Func func, ChunkBatchedList& chunkBatch, const containers::darray<archetype::Chunk*>& chunks,
					const query::QueryInfo& queryInfo) {
				size_t chunkOffset = 0;
				size_t itemsLeft = chunks.size();
				while (itemsLeft > 0) {
					const size_t maxBatchSize = chunkBatch.max_size() - chunkBatch.size();
					const size_t batchSize = itemsLeft > maxBatchSize ? maxBatchSize : itemsLeft;

					CChunkSpan chunkSpan((const archetype::Chunk**)&chunks[chunkOffset], batchSize);
					for (const auto* pChunk: chunkSpan) {
						if (!pChunk->HasEntities())
							continue;
						if constexpr (HasFilters) {
							if (!CheckFilters(*pChunk, queryInfo))
								continue;
						}

						chunkBatch.push_back(const_cast<archetype::Chunk*>(pChunk));
					}

					if GAIA_UNLIKELY (chunkBatch.size() == chunkBatch.max_size())
						ChunkBatch_Perform(func, chunkBatch);

					itemsLeft -= batchSize;
					chunkOffset += batchSize;
				}
			}

			template <bool HasFilters, typename Func>
			void ProcessQueryOnChunks(
					Func func, ChunkBatchedList& chunkBatch, const containers::darray<archetype::Chunk*>& chunks,
					const query::QueryInfo& queryInfo, bool enabledOnly) {
				size_t chunkOffset = 0;
				size_t itemsLeft = chunks.size();
				while (itemsLeft > 0) {
					const size_t maxBatchSize = chunkBatch.max_size() - chunkBatch.size();
					const size_t batchSize = itemsLeft > maxBatchSize ? maxBatchSize : itemsLeft;

					CChunkSpan chunkSpan((const archetype::Chunk**)&chunks[chunkOffset], batchSize);
					for (const auto* pChunk: chunkSpan) {
						if (!pChunk->HasEntities())
							continue;
						if (enabledOnly == pChunk->HasDisabledEntities()) {
							const auto maskCnt = pChunk->GetDisabledEntityMask().count();

							// We want enabled entities but there are only disables ones on the chunk
							if (enabledOnly && maskCnt == pChunk->GetEntityCount())
								continue;
							// We want disabled entities but there are only enabled ones on the chunk
							if (!enabledOnly && maskCnt == 0)
								continue;
						}
						if constexpr (HasFilters) {
							if (!CheckFilters(*pChunk, queryInfo))
								continue;
						}

						chunkBatch.push_back(const_cast<archetype::Chunk*>(pChunk));
					}

					if GAIA_UNLIKELY (chunkBatch.size() == chunkBatch.max_size())
						ChunkBatch_Perform(func, chunkBatch);

					itemsLeft -= batchSize;
					chunkOffset += batchSize;
				}
			}

			template <typename Func>
			void ForEach_RunQueryOnChunks(query::QueryInfo& queryInfo, Query::Constraints constraints, Func func) {
				// Update the world version
				UpdateVersion(*m_worldVersion);

				ChunkBatchedList chunkBatch;

				const bool hasFilters = queryInfo.HasFilters();
				if (hasFilters) {
					// Evaluation defaults to EnabledOnly changes. AcceptAll is something that has to be asked for explicitely
					if GAIA_UNLIKELY (constraints == Query::Constraints::AcceptAll) {
						for (auto* pArchetype: queryInfo)
							ProcessQueryOnChunks_NoConstraints<true>(func, chunkBatch, pArchetype->GetChunks(), queryInfo);
					} else {
						const bool enabledOnly = constraints == Query::Constraints::EnabledOnly;
						for (auto* pArchetype: queryInfo)
							ProcessQueryOnChunks<true>(func, chunkBatch, pArchetype->GetChunks(), queryInfo, enabledOnly);
					}
				} else {
					if GAIA_UNLIKELY (constraints == Query::Constraints::AcceptAll) {
						for (auto* pArchetype: queryInfo)
							ProcessQueryOnChunks_NoConstraints<false>(func, chunkBatch, pArchetype->GetChunks(), queryInfo);
					} else {
						const bool enabledOnly = constraints == Query::Constraints::EnabledOnly;
						for (auto* pArchetype: queryInfo)
							ProcessQueryOnChunks<false>(func, chunkBatch, pArchetype->GetChunks(), queryInfo, enabledOnly);
					}
				}

				if (!chunkBatch.empty())
					ChunkBatch_Perform(func, chunkBatch);

				// Update the query version with the current world's version
				queryInfo.SetWorldVersion(*m_worldVersion);
			}

			template <typename Func>
			void ForEach_Internal(query::QueryInfo& queryInfo, Func func) {
				using InputArgs = decltype(utils::func_args(&Func::operator()));

#if GAIA_DEBUG
				// Make sure we only use components specified in the query
				GAIA_ASSERT(UnpackArgsIntoQuery_HasAll(queryInfo, InputArgs{}));
#endif

				ForEach_RunQueryOnChunks(queryInfo, Query::Constraints::EnabledOnly, [&](archetype::Chunk& chunk) {
					chunk.ForEach(InputArgs{}, func);
				});
			}

			void InvalidateQuery() {
				m_queryId = query::QueryIdBad;
			}

		public:
			Query() = default;
			Query(
					QueryCache& queryCache, uint32_t& worldVersion, const containers::darray<archetype::Archetype*>& archetypes,
					const query::ComponentToArchetypeMap& componentToArchetypeMap):
					m_entityQueryCache(&queryCache),
					m_worldVersion(&worldVersion), m_archetypes(&archetypes),
					m_componentToArchetypeMap(&componentToArchetypeMap) {}

			GAIA_NODISCARD uint32_t GetQueryId() const {
				return m_queryId;
			}

			template <typename... T>
			Query& All() {
				// Adding new rules invalides the query
				InvalidateQuery();
				// Add commands to the command buffer
				(AddComponent_Internal<T>(query::ListType::LT_All), ...);
				return *this;
			}

			template <typename... T>
			Query& Any() {
				// Adding new rules invalides the query
				InvalidateQuery();
				// Add commands to the command buffer
				(AddComponent_Internal<T>(query::ListType::LT_Any), ...);
				return *this;
			}

			template <typename... T>
			Query& None() {
				// Adding new rules invalides the query
				InvalidateQuery();
				// Add commands to the command buffer
				(AddComponent_Internal<T>(query::ListType::LT_None), ...);
				return *this;
			}

			template <typename... T>
			Query& WithChanged() {
				// Adding new rules invalides the query
				InvalidateQuery();
				// Add commands to the command buffer
				(WithChanged_Internal<T>(), ...);
				return *this;
			}

			GAIA_NODISCARD static bool
			CheckConstraintsEnabledDisabledOnly(ecs::Query::Constraints constraints, bool filterEnabledOnly) {
				const bool arr[2] = {// EnabledOnly
														 true,
														 // DisabledOnly
														 false};
				return filterEnabledOnly == arr[(int)constraints];
			}

			GAIA_NODISCARD static bool CheckConstraints(ecs::Query::Constraints constraints, bool filterEnabledOnly) {
				const bool arr[2] = {// EnabledOnly
														 true,
														 // DisabledOnly
														 false};
				return constraints == Query::Constraints::AcceptAll || filterEnabledOnly == arr[(int)constraints];
			}

			Query& Schedule() {
				m_executionMode = ExecutionMode::Single;
				return *this;
			}

			Query& ScheduleParallel() {
				m_executionMode = ExecutionMode::Parallel;
				return *this;
			}

			template <typename Func>
			void ForEach(Func func) {
				// Make sure the query was created by World.CreateQuery()
				GAIA_ASSERT(m_entityQueryCache != nullptr);

				auto& queryInfo = FetchQueryInfo();

				if constexpr (std::is_invocable<Func, Iterator>::value)
					ForEach_RunQueryOnChunks(queryInfo, Query::Constraints::AcceptAll, [&](archetype::Chunk& chunk) {
						func(Iterator(queryInfo, chunk));
					});
				else if constexpr (std::is_invocable<Func, IteratorEnabled>::value)
					ForEach_RunQueryOnChunks(queryInfo, Query::Constraints::EnabledOnly, [&](archetype::Chunk& chunk) {
						func(IteratorEnabled(queryInfo, chunk));
					});
				else if constexpr (std::is_invocable<Func, IteratorDisabled>::value)
					ForEach_RunQueryOnChunks(queryInfo, Query::Constraints::DisabledOnly, [&](archetype::Chunk& chunk) {
						func(IteratorDisabled(queryInfo, chunk));
					});
				else
					ForEach_Internal(queryInfo, func);
			}

			template <typename Func>
			void ForEach(query::QueryId queryId, Func func) {
				// Make sure the query was created by World.CreateQuery()
				GAIA_ASSERT(m_entityQueryCache != nullptr);

				auto& queryInfo = m_entityQueryCache->Get(queryId);
				ForEach_Internal(queryInfo, func);
			}

			/*!
				Returns true or false depending on whether there are entities matching the query.
				\warning Only use if you only care if there are any entities matching the query.
								 The result is not cached and repeated calls to the function might be slow.
								 If you already called ToArray, checking if it is empty is preferred.
				\return True if there are any entites matchine the query. False otherwise.
				*/
			bool HasEntities(ecs::Query::Constraints constraints = ecs::Query::Constraints::EnabledOnly) {
				// Make sure the query was created by World.CreateQuery()
				GAIA_ASSERT(m_entityQueryCache != nullptr);

				auto& queryInfo = FetchQueryInfo();
				const bool hasFilters = queryInfo.HasFilters();

				if (hasFilters) {
					auto execWithFiltersON = [&](const auto& chunks) {
						return utils::has_if(chunks, [&](archetype::Chunk* pChunk) {
							if (!pChunk->HasEntities())
								return false;
							return CheckFilters(*pChunk, queryInfo);
						});
					};

					auto execWithFiltersON_EnabledDisabled = [&](const auto& chunks, bool enabledOnly) {
						return utils::has_if(chunks, [&](archetype::Chunk* pChunk) {
							const auto hasEntities = enabledOnly
																					 ? pChunk->GetEntityCount() != pChunk->GetDisabledEntityMask().count()
																					 : pChunk->GetDisabledEntityMask().count() > 0;
							if (!hasEntities)
								return false;

							return CheckFilters(*pChunk, queryInfo);
						});
					};

					if (constraints == Query::Constraints::AcceptAll) {
						for (auto* pArchetype: queryInfo) {
							if (execWithFiltersON(pArchetype->GetChunks()))
								return true;
						}
					} else {
						for (auto* pArchetype: queryInfo) {
							if (execWithFiltersON_EnabledDisabled(
											pArchetype->GetChunks(), constraints == Query::Constraints::EnabledOnly))
								return true;
						}
					}
				} else {
					auto execWithFiltersOFF = [&](const auto& chunks) {
						return utils::has_if(chunks, [&](archetype::Chunk* pChunk) {
							return pChunk->HasEntities();
						});
					};

					auto execWithFiltersOFF_EnabledDisabled = [&](const auto& chunks, bool enabledOnly) {
						return utils::has_if(chunks, [&](archetype::Chunk* pChunk) {
							return enabledOnly ? pChunk->GetEntityCount() != pChunk->GetDisabledEntityMask().count()
																 : pChunk->GetDisabledEntityMask().count() > 0;
						});
					};

					if (constraints == Query::Constraints::AcceptAll) {
						for (auto* pArchetype: queryInfo) {
							if (execWithFiltersOFF(pArchetype->GetChunks()))
								return true;
						}
					} else {
						for (auto* pArchetype: queryInfo) {
							if (execWithFiltersOFF_EnabledDisabled(
											pArchetype->GetChunks(), constraints == Query::Constraints::EnabledOnly))
								return true;
						}
					}
				}

				return false;
			}

			/*!
			Returns the number of entities matching the query
			\warning Only use if you only care about the number of entities matching the query.
							 The result is not cached and repeated calls to the function might be slow.
							 If you already called ToArray, use the size provided by the array.
			\return The number of matching entities
			*/
			size_t CalculateEntityCount(ecs::Query::Constraints constraints = ecs::Query::Constraints::EnabledOnly) {
				// Make sure the query was created by World.CreateQuery()
				GAIA_ASSERT(m_entityQueryCache != nullptr);

				auto& queryInfo = FetchQueryInfo();
				const bool hasFilters = queryInfo.HasFilters();
				size_t entityCount = 0;

				if (hasFilters) {
					auto execWithFiltersON = [&](const auto& chunks) {
						size_t cnt = 0;
						for (auto* pChunk: chunks) {
							if (!pChunk->HasEntities())
								continue;
							if (!CheckFilters(*pChunk, queryInfo))
								continue;
							cnt += pChunk->GetEntityCount();
						};
						return cnt;
					};

					auto execWithFiltersON_EnabledDisabled = [&](const auto& chunks, bool enabledOnly) {
						size_t cnt = 0;
						for (auto* pChunk: chunks) {
							if (!pChunk->HasEntities())
								continue;
							if (!CheckFilters(*pChunk, queryInfo))
								continue;
							if (enabledOnly)
								cnt += pChunk->GetEntityCount() - pChunk->GetDisabledEntityMask().count();
							else
								cnt += pChunk->GetDisabledEntityMask().count();
						};
						return cnt;
					};

					if (constraints == Query::Constraints::AcceptAll) {
						for (auto* pArchetype: queryInfo)
							entityCount += execWithFiltersON(pArchetype->GetChunks());
					} else {
						for (auto* pArchetype: queryInfo)
							entityCount += execWithFiltersON_EnabledDisabled(
									pArchetype->GetChunks(), constraints == Query::Constraints::EnabledOnly);
					}
				} else {
					auto execWithFiltersOFF = [&](const auto& chunks) {
						size_t cnt = 0;
						for (auto* pChunk: chunks) {
							if (!pChunk->HasEntities())
								continue;
							cnt += pChunk->GetEntityCount();
						};
						return cnt;
					};

					auto execWithFiltersOFF_EnabledDisabled = [&](const auto& chunks, bool enabledOnly) {
						size_t cnt = 0;
						for (auto* pChunk: chunks) {
							if (!pChunk->HasEntities())
								continue;
							if (enabledOnly)
								cnt += pChunk->GetEntityCount() - pChunk->GetDisabledEntityMask().count();
							else
								cnt += pChunk->GetDisabledEntityMask().count();
						};
						return cnt;
					};

					if (constraints == Query::Constraints::AcceptAll) {
						for (auto* pArchetype: queryInfo)
							entityCount += execWithFiltersOFF(pArchetype->GetChunks());
					} else {
						for (auto* pArchetype: queryInfo)
							entityCount += execWithFiltersOFF_EnabledDisabled(
									pArchetype->GetChunks(), constraints == Query::Constraints::EnabledOnly);
					}
				}

				return entityCount;
			}

			/*!
			Appends all components or entities matching the query to the array
			\tparam outArray Container storing entities or components
			\param constraints Query constraints
			\return Array with entities or components
			*/
			template <typename Container>
			void ToArray(Container& outArray, ecs::Query::Constraints constraints = ecs::Query::Constraints::EnabledOnly) {
				using ContainerItemType = typename Container::value_type;

				// Make sure the query was created by World.CreateQuery()
				GAIA_ASSERT(m_entityQueryCache != nullptr);

				const auto entityCount = CalculateEntityCount();
				if (entityCount == 0)
					return;

				outArray.reserve(entityCount);
				auto& queryInfo = FetchQueryInfo();
				const bool hasFilters = queryInfo.HasFilters();

				auto addChunk = [&](archetype::Chunk* pChunk) {
					const auto componentView = pChunk->template View<ContainerItemType>();
					for (size_t i = 0; i < pChunk->GetEntityCount(); ++i)
						outArray.push_back(componentView[i]);
				};

				if (hasFilters) {
					auto execWithFiltersON = [&](const auto& chunks) {
						for (auto* pChunk: chunks) {
							if (!pChunk->HasEntities())
								continue;
							if (!CheckFilters(*pChunk, queryInfo))
								continue;
							addChunk(pChunk);
						};
					};

					auto execWithFiltersON_EnabledDisabled = [&](const auto& chunks, bool enabledOnly) {
						for (auto* pChunk: chunks) {
							if (!pChunk->HasEntities())
								continue;
							if (CheckConstraintsEnabledDisabledOnly(constraints, enabledOnly) == pChunk->HasDisabledEntities())
								continue;
							if (!CheckFilters(*pChunk, queryInfo))
								continue;
							addChunk(pChunk);
						};
					};

					if (constraints == Query::Constraints::AcceptAll) {
						for (auto* pArchetype: queryInfo)
							execWithFiltersON(pArchetype->GetChunks());
					} else {
						for (auto* pArchetype: queryInfo)
							execWithFiltersON_EnabledDisabled(
									pArchetype->GetChunks(), constraints == Query::Constraints::EnabledOnly);
					}
				} else {
					auto execWithFiltersOFF = [&](const auto& chunks) {
						for (auto* pChunk: chunks) {
							if (!pChunk->HasEntities())
								continue;
							addChunk(pChunk);
						};
					};

					auto execWithFiltersOFF_EnabledDisabled = [&](const auto& chunks, bool enabledOnly) {
						for (auto* pChunk: chunks) {
							if (!pChunk->HasEntities())
								continue;
							if (CheckConstraintsEnabledDisabledOnly(constraints, enabledOnly) == pChunk->HasDisabledEntities())
								continue;
							addChunk(pChunk);
						};
					};

					if (constraints == Query::Constraints::AcceptAll) {
						for (auto* pArchetype: queryInfo)
							execWithFiltersOFF(pArchetype->GetChunks());
					} else {
						for (auto* pArchetype: queryInfo)
							execWithFiltersOFF_EnabledDisabled(
									pArchetype->GetChunks(), constraints == Query::Constraints::EnabledOnly);
					}
				}
			}
		};
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
			//! Cache of query ids to speed up ForEach
			containers::map<component::ComponentLookupHash, query::QueryId> m_uniqueFuncQueryPairs;
			//! Map of componentId -> archetype matches.
			query::ComponentToArchetypeMap m_componentToArchetypeMap;

			//! Map of archetypes mapping to the same hash - used for lookups
			containers::map<archetype::LookupHash, archetype::ArchetypeList> m_archetypeMap;
			//! List of archetypes - used for iteration
			archetype::ArchetypeList m_archetypes;

			//! Implicit list of entities. Used for look-ups only when searching for
			//! entities in chunks + data validation
			containers::ImplicitList<EntityContainer, Entity> m_entities;

			//! List of chunks to delete
			containers::darray<archetype::Chunk*> m_chunksToRemove;
#if !GAIA_AVOID_CHUNK_FRAGMENTATION
			//! ID of the last defragmented archetype
			uint32_t m_defragLastArchetypeID = 0;
#endif

			//! With every structural change world version changes
			uint32_t m_worldVersion = 0;

		private:
			//! Remove an entity from chunk.
			//! \param pChunk Chunk we remove the entity from
			//! \param entityChunkIndex Index of entity within its chunk
			//! \tparam IsEntityReleaseWanted True if entity is to be released as well. False otherwise.
			template <bool IsEntityReleaseWanted>
			void RemoveEntity(archetype::Chunk* pChunk, uint32_t entityChunkIndex) {
				GAIA_ASSERT(
						!pChunk->IsStructuralChangesLocked() && "Entities can't be removed while their chunk is being iterated "
																										"(structural changes are forbidden during this time!)");

				const auto chunkEntityCount = pChunk->GetEntityCount();
				if GAIA_UNLIKELY (chunkEntityCount == 0)
					return;

				GAIA_PROF_SCOPE(RemoveEntity);

				auto entity = pChunk->GetEntity(entityChunkIndex);
				auto& archetype = *m_archetypes[pChunk->GetArchetypeId()];

#if GAIA_AVOID_CHUNK_FRAGMENTATION
				// Swap the last entity in the last chunk with the entity spot we just created by moving
				// the entity to somewhere else.
				auto* pOldChunk = archetype.FindFirstNonEmptyChunk();
				if (pOldChunk == pChunk) {
					const uint32_t lastEntityIdx = chunkEntityCount - 1;
					const bool wasDisabled = pChunk->GetDisabledEntityMask().test(lastEntityIdx);

					// Transfer data form the last entity to the new one
					pChunk->SwapEntitiesInsideChunkAndDeleteOld(entityChunkIndex, m_entities);
					pChunk->RemoveLastEntity(m_chunksToRemove);

					// Transfer the disabled state
					if GAIA_LIKELY (chunkEntityCount > 1)
						archetype.EnableEntity(pChunk, entityChunkIndex, !wasDisabled);
				} else if (pOldChunk != nullptr && pOldChunk->HasEntities()) {
					const uint32_t lastEntityIdx = pOldChunk->GetEntityCount() - 1;
					const bool wasDisabled = pOldChunk->GetDisabledEntityMask().test(lastEntityIdx);

					// Transfer data form the old chunk to the new one
					auto lastEntity = pOldChunk->GetEntity(lastEntityIdx);
					pChunk->SetEntity(entityChunkIndex, lastEntity);
					pChunk->MoveEntityData(lastEntity, entityChunkIndex, m_entities);
					pOldChunk->RemoveLastEntity(m_chunksToRemove);
					pOldChunk->UpdateVersions();

					// Transfer the disabled state
					archetype.EnableEntity(pChunk, entityChunkIndex, !wasDisabled);

					auto& lastEntityContainer = m_entities[lastEntity.id()];
					lastEntityContainer.pChunk = pChunk;
					lastEntityContainer.idx = entityChunkIndex;
					lastEntityContainer.gen = lastEntity.gen();
				}

				pChunk->UpdateVersions();
				if constexpr (IsEntityReleaseWanted)
					ReleaseEntity(entity);

				archetype.VerifyChunksFramentation();
#else
				const uint32_t lastEntityIdx = chunkEntityCount - 1;
				const bool wasDisabled = pChunk->GetDisabledEntityMask().test(lastEntityIdx);

				if constexpr (IsEntityReleaseWanted) {
					pChunk->SwapEntitiesInsideChunkAndDeleteOld(entityChunkIndex, m_entities);

					// Transfer the disabled state is possible
					if GAIA_LIKELY (chunkEntityCount > 1)
						archetype.EnableEntity(pChunk, entityChunkIndex, !wasDisabled);

					pChunk->RemoveLastEntity(m_chunksToRemove);
					ReleaseEntity(entity);
				} else {
					pChunk->SwapEntitiesInsideChunkAndDeleteOld(entityChunkIndex, m_entities);

					// Transfer the disabled state is possible
					if GAIA_LIKELY (chunkEntityCount > 1)
						archetype.EnableEntity(pChunk, entityChunkIndex, !wasDisabled);

					pChunk->RemoveLastEntity(m_chunksToRemove);
				}

				pChunk->UpdateVersions();
#endif
			}

#if !GAIA_AVOID_CHUNK_FRAGMENTATION
			//! Defragments chunks.
			//! \param maxEntites Maximum number of entities moved per call
			void DefragmentChunks(uint32_t maxEntities) {
				const auto maxIters = (uint32_t)m_archetypes.size();
				for (uint32_t i = 0; i < maxIters; ++i) {
					m_defragLastArchetypeID = (m_defragLastArchetypeID + i) % maxIters;

					auto* pArchetype = m_archetypes[m_defragLastArchetypeID];
					pArchetype->Defragment(maxEntities, m_chunksToRemove, m_entities);
					if (maxEntities == 0)
						return;
				}
			}
#endif

			//! Searches for archetype with a given set of components
			//! \param lookupHash Archetype lookup hash
			//! \param componentIdsGeneric Span of generic component ids
			//! \param componentIdsChunk Span of chunk component ids
			//! \return Pointer to archetype or nullptr.
			GAIA_NODISCARD archetype::Archetype* FindArchetype(
					archetype::LookupHash lookupHash, component::ComponentIdSpan componentIdsGeneric,
					component::ComponentIdSpan componentIdsChunk) {
				// Search for the archetype in the map
				const auto it = m_archetypeMap.find(lookupHash);
				if (it == m_archetypeMap.end())
					return nullptr;

				const auto& archetypeArray = it->second;
				GAIA_ASSERT(!archetypeArray.empty());

				// More than one archetype can have the same lookup key. However, this should be extermely
				// rare (basically it should never happen). For this reason, only search for the exact match
				// if we happen to have more archetypes under the same hash.
				if GAIA_LIKELY (archetypeArray.size() == 1)
					return archetypeArray[0];

				auto checkComponentIds = [&](const archetype::ComponentIdArray& componentIdsArchetype,
																		 component::ComponentIdSpan componentIds) {
					for (uint32_t j = 0; j < componentIds.size(); j++) {
						// Different components. We need to search further
						if (componentIdsArchetype[j] != componentIds[j])
							return false;
					}
					return true;
				};

				// Iterate over the list of archetypes and find the exact match
				for (auto* pArchetype: archetypeArray) {
					const auto& genericComponentList = pArchetype->GetComponentIdArray(component::ComponentType::CT_Generic);
					if (genericComponentList.size() != componentIdsGeneric.size())
						continue;
					const auto& chunkComponentList = pArchetype->GetComponentIdArray(component::ComponentType::CT_Chunk);
					if (chunkComponentList.size() != componentIdsChunk.size())
						continue;

					if (checkComponentIds(genericComponentList, componentIdsGeneric) &&
							checkComponentIds(chunkComponentList, componentIdsChunk))
						return pArchetype;
				}

				return nullptr;
			}

			//! Creates a new archetype from a given set of components
			//! \param componentIdsGeneric Span of generic component infos
			//! \param componentIdsChunk Span of chunk component infos
			//! \return Pointer to the new archetype.
			GAIA_NODISCARD archetype::Archetype*
			CreateArchetype(component::ComponentIdSpan componentIdsGeneric, component::ComponentIdSpan componentIdsChunk) {
				auto* pArchetype = archetype::Archetype::Create(
						(archetype::ArchetypeId)m_archetypes.size(), m_worldVersion, componentIdsGeneric, componentIdsChunk);

				auto registerComponentToArchetypePair = [&](component::ComponentId componentId) {
					const auto it = m_componentToArchetypeMap.find(componentId);
					if (it == m_componentToArchetypeMap.end())
						m_componentToArchetypeMap.try_emplace(componentId, archetype::ArchetypeList{pArchetype});
					else if (!utils::has(it->second, pArchetype))
						it->second.push_back(pArchetype);
				};

				for (const auto componentId: componentIdsGeneric)
					registerComponentToArchetypePair(componentId);
				for (const auto componentId: componentIdsChunk)
					registerComponentToArchetypePair(componentId);

				return pArchetype;
			}

			//! Registers the archetype in the world.
			//! \param pArchetype Archetype to register.
			void RegisterArchetype(archetype::Archetype* pArchetype) {
				// Make sure hashes were set already
				GAIA_ASSERT(
						(m_archetypes.empty() || pArchetype == m_archetypes[0]) ||
						(pArchetype->GetGenericHash().hash != 0 || pArchetype->GetChunkHash().hash != 0));
				GAIA_ASSERT((m_archetypes.empty() || pArchetype == m_archetypes[0]) || pArchetype->GetLookupHash().hash != 0);

				// Make sure the archetype is not registered yet
				GAIA_ASSERT(!utils::has(m_archetypes, pArchetype));

				// Register the archetype
				m_archetypes.push_back(pArchetype);

				auto it = m_archetypeMap.find(pArchetype->GetLookupHash());
				if (it == m_archetypeMap.end()) {
					m_archetypeMap[pArchetype->GetLookupHash()] = {pArchetype};
				} else {
					auto& archetypes = it->second;
					GAIA_ASSERT(!utils::has(archetypes, pArchetype));
					archetypes.push_back(pArchetype);
				}
			}

#if GAIA_DEBUG
			static void VerifyAddComponent(
					archetype::Archetype& archetype, Entity entity, component::ComponentType componentType,
					const component::ComponentInfo& infoToAdd) {
				const auto& componentIds = archetype.GetComponentIdArray(componentType);
				const auto& cc = ComponentCache::Get();

				// Make sure not to add too many infos
				if GAIA_UNLIKELY (!archetype::VerifyArchetypeComponentCount(1)) {
					GAIA_ASSERT(false && "Trying to add too many components to entity!");
					GAIA_LOG_W(
							"Trying to add a component to entity [%u.%u] but there's no space left!", entity.id(), entity.gen());
					GAIA_LOG_W("Already present:");
					const size_t oldInfosCount = componentIds.size();
					for (size_t i = 0; i < oldInfosCount; i++) {
						const auto& info = cc.GetComponentDesc(componentIds[i]);
						GAIA_LOG_W("> [%u] %.*s", (uint32_t)i, (uint32_t)info.name.size(), info.name.data());
					}
					GAIA_LOG_W("Trying to add:");
					{
						const auto& info = cc.GetComponentDesc(infoToAdd.componentId);
						GAIA_LOG_W("> %.*s", (uint32_t)info.name.size(), info.name.data());
					}
				}

				// Don't add the same component twice
				for (size_t i = 0; i < componentIds.size(); ++i) {
					const auto& info = cc.GetComponentDesc(componentIds[i]);
					if (info.componentId == infoToAdd.componentId) {
						GAIA_ASSERT(false && "Trying to add a duplicate component");

						GAIA_LOG_W(
								"Trying to add a duplicate of component %s to entity [%u.%u]",
								component::ComponentTypeString[componentType], entity.id(), entity.gen());
						GAIA_LOG_W("> %.*s", (uint32_t)info.name.size(), info.name.data());
					}
				}
			}

			static void VerifyRemoveComponent(
					archetype::Archetype& archetype, Entity entity, component::ComponentType componentType,
					const component::ComponentInfo& infoToRemove) {
				const auto& componentIds = archetype.GetComponentIdArray(componentType);
				if GAIA_UNLIKELY (!utils::has(componentIds, infoToRemove.componentId)) {
					GAIA_ASSERT(false && "Trying to remove a component which wasn't added");
					GAIA_LOG_W(
							"Trying to remove a component from entity [%u.%u] but it was never added", entity.id(), entity.gen());
					GAIA_LOG_W("Currently present:");

					const auto& cc = ComponentCache::Get();

					for (size_t k = 0; k < componentIds.size(); k++) {
						const auto& info = cc.GetComponentDesc(componentIds[k]);
						GAIA_LOG_W("> [%u] %.*s", (uint32_t)k, (uint32_t)info.name.size(), info.name.data());
					}

					{
						GAIA_LOG_W("Trying to remove:");
						const auto& info = cc.GetComponentDesc(infoToRemove.componentId);
						GAIA_LOG_W("> %.*s", (uint32_t)info.name.size(), info.name.data());
					}
				}
			}
#endif

			//! Searches for an archetype which is formed by adding \param componentType to \param pArchetypeLeft.
			//! If no such archetype is found a new one is created.
			//! \param pArchetypeLeft Archetype we originate from.
			//! \param componentType Component infos.
			//! \param infoToAdd Component we want to add.
			//! \return Pointer to archetype.
			GAIA_NODISCARD archetype::Archetype* FindOrCreateArchetype_AddComponent(
					archetype::Archetype* pArchetypeLeft, component::ComponentType componentType,
					const component::ComponentInfo& infoToAdd) {
				// We don't want to store edges for the root archetype because the more components there are the longer
				// it would take to find anything. Therefore, for the root archetype we always make a lookup.
				// Compared to an ordinary lookup this path is stripped as much as possible.
				if (pArchetypeLeft == m_archetypes[0]) {
					archetype::Archetype* pArchetypeRight = nullptr;

					if (componentType == component::ComponentType::CT_Generic) {
						const auto genericHash = infoToAdd.lookupHash;
						const auto lookupHash = archetype::Archetype::CalculateLookupHash(genericHash, {0});
						pArchetypeRight = FindArchetype(lookupHash, component::ComponentIdSpan(&infoToAdd.componentId, 1), {});
						if (pArchetypeRight == nullptr) {
							pArchetypeRight = CreateArchetype(component::ComponentIdSpan(&infoToAdd.componentId, 1), {});
							pArchetypeRight->Init({genericHash}, {0}, lookupHash);
							pArchetypeRight->BuildGraphEdgesLeft(pArchetypeLeft, componentType, infoToAdd.componentId);
							RegisterArchetype(pArchetypeRight);
						}
					} else {
						const auto chunkHash = infoToAdd.lookupHash;
						const auto lookupHash = archetype::Archetype::CalculateLookupHash({0}, chunkHash);
						pArchetypeRight = FindArchetype(lookupHash, {}, component::ComponentIdSpan(&infoToAdd.componentId, 1));
						if (pArchetypeRight == nullptr) {
							pArchetypeRight = CreateArchetype({}, component::ComponentIdSpan(&infoToAdd.componentId, 1));
							pArchetypeRight->Init({0}, {chunkHash}, lookupHash);
							pArchetypeRight->BuildGraphEdgesLeft(pArchetypeLeft, componentType, infoToAdd.componentId);
							RegisterArchetype(pArchetypeRight);
						}
					}

					return pArchetypeRight;
				}

				// Check if the component is found when following the "add" edges
				{
					const auto archetypeId = pArchetypeLeft->FindGraphEdgeRight(componentType, infoToAdd.componentId);
					if (archetypeId != archetype::ArchetypeIdBad)
						return m_archetypes[archetypeId];
				}

				const uint32_t a = componentType;
				const uint32_t b = (componentType + 1) & 1;
				const containers::sarray_ext<uint32_t, archetype::MAX_COMPONENTS_PER_ARCHETYPE>* infos[2];

				containers::sarray_ext<uint32_t, archetype::MAX_COMPONENTS_PER_ARCHETYPE> infosNew;
				infos[a] = &infosNew;
				infos[b] = &pArchetypeLeft->GetComponentIdArray((component::ComponentType)b);

				// Prepare a joint array of component infos of old + the newly added component
				{
					const auto& componentIds = pArchetypeLeft->GetComponentIdArray((component::ComponentType)a);
					const size_t componentInfosSize = componentIds.size();
					infosNew.resize(componentInfosSize + 1);

					for (size_t j = 0; j < componentInfosSize; ++j)
						infosNew[j] = componentIds[j];
					infosNew[componentInfosSize] = infoToAdd.componentId;
				}

				// Make sure to sort the component infos so we receive the same hash no matter the order in which components
				// are provided Bubble sort is okay. We're dealing with at most MAX_COMPONENTS_PER_ARCHETYPE items.
				component::SortComponents(infosNew);

				// Once sorted we can calculate the hashes
				const archetype::Archetype::GenericComponentHash genericHash = {
						component::CalculateLookupHash({*infos[0]}).hash};
				const archetype::Archetype::ChunkComponentHash chunkHash = {component::CalculateLookupHash({*infos[1]}).hash};
				const auto lookupHash = archetype::Archetype::CalculateLookupHash(genericHash, chunkHash);

				auto* pArchetypeRight =
						FindArchetype(lookupHash, {infos[0]->data(), infos[0]->size()}, {infos[1]->data(), infos[1]->size()});
				if (pArchetypeRight == nullptr) {
					pArchetypeRight = CreateArchetype({infos[0]->data(), infos[0]->size()}, {infos[1]->data(), infos[1]->size()});
					pArchetypeRight->Init(genericHash, chunkHash, lookupHash);
					pArchetypeLeft->BuildGraphEdges(pArchetypeRight, componentType, infoToAdd.componentId);
					RegisterArchetype(pArchetypeRight);
				}

				return pArchetypeRight;
			}

			//! Searches for an archetype which is formed by removing \param componentType from \param pArchetypeRight.
			//! If no such archetype is found a new one is created.
			//! \param pArchetypeRight Archetype we originate from.
			//! \param componentType Component infos.
			//! \param infoToRemove Component we want to remove.
			//! \return Pointer to archetype.
			GAIA_NODISCARD archetype::Archetype* FindOrCreateArchetype_RemoveComponent(
					archetype::Archetype* pArchetypeRight, component::ComponentType componentType,
					const component::ComponentInfo& infoToRemove) {
				// Check if the component is found when following the "del" edges
				{
					const auto archetypeId = pArchetypeRight->FindGraphEdgeLeft(componentType, infoToRemove.componentId);
					if (archetypeId != archetype::ArchetypeIdBad)
						return m_archetypes[archetypeId];
				}

				const uint32_t a = componentType;
				const uint32_t b = (componentType + 1) & 1;
				const containers::sarray_ext<uint32_t, archetype::MAX_COMPONENTS_PER_ARCHETYPE>* infos[2];

				containers::sarray_ext<uint32_t, archetype::MAX_COMPONENTS_PER_ARCHETYPE> infosNew;
				infos[a] = &infosNew;
				infos[b] = &pArchetypeRight->GetComponentIdArray((component::ComponentType)b);

				// Find the intersection
				for (const auto componentId: pArchetypeRight->GetComponentIdArray((component::ComponentType)a)) {
					if (componentId == infoToRemove.componentId)
						continue;

					infosNew.push_back(componentId);
				}

				// Return if there's no change
				if (infosNew.size() == pArchetypeRight->GetComponentIdArray((component::ComponentType)a).size())
					return nullptr;

				// Calculate the hashes
				const archetype::Archetype::GenericComponentHash genericHash = {
						component::CalculateLookupHash({*infos[0]}).hash};
				const archetype::Archetype::ChunkComponentHash chunkHash = {component::CalculateLookupHash({*infos[1]}).hash};
				const auto lookupHash = archetype::Archetype::CalculateLookupHash(genericHash, chunkHash);

				auto* pArchetype = FindArchetype(lookupHash, {*infos[0]}, {*infos[1]});
				if (pArchetype == nullptr) {
					pArchetype = CreateArchetype({infos[0]->data(), infos[0]->size()}, {infos[1]->data(), infos[1]->size()});
					pArchetype->Init(genericHash, lookupHash, lookupHash);
					pArchetype->BuildGraphEdges(pArchetypeRight, componentType, infoToRemove.componentId);
					RegisterArchetype(pArchetype);
				}

				return pArchetype;
			}

			//! Returns an array of archetypes registered in the world
			//! \return Array or archetypes.
			const auto& GetArchetypes() const {
				return m_archetypes;
			}

			//! Returns the archetype the entity belongs to.
			//! \param entity Entity
			//! \return Reference to the archetype.
			GAIA_NODISCARD archetype::Archetype& GetArchetype(Entity entity) const {
				GAIA_ASSERT(IsEntityValid(entity));

				const auto& entityContainer = m_entities[entity.id()];
				auto* pChunk = entityContainer.pChunk;
				return *m_archetypes[pChunk == nullptr ? archetype::ArchetypeId(0) : pChunk->GetArchetypeId()];
			}

			//! Invalidates \param entityToDelete
			void ReleaseEntity(Entity entityToDelete) {
				auto& entityContainer = m_entities.release(entityToDelete);
				entityContainer.pChunk = nullptr;
			}

			//! Associates an entity with a chunk.
			//! \param entity Entity to associate with a chunk
			//! \param pChunk Chunk the entity is to become a part of
			void StoreEntity(Entity entity, archetype::Chunk* pChunk) {
				GAIA_ASSERT(pChunk != nullptr);
				GAIA_ASSERT(
						!pChunk->IsStructuralChangesLocked() && "Entities can't be added while their chunk is being iterated "
																										"(structural changes are forbidden during this time!)");

				auto& entityContainer = m_entities[entity.id()];
				entityContainer.pChunk = pChunk;
				entityContainer.idx = pChunk->AddEntity(entity);
				entityContainer.gen = entity.gen();
			}

			/*!
			Moves an entity along with all its generic components from its current chunk to another one.
			\param oldEntity Entity to move
			\param targetChunk Target chunk
			*/
			void MoveEntity(Entity oldEntity, archetype::Chunk& targetChunk) {
				GAIA_PROF_SCOPE(MoveEntity);

				auto* pNewChunk = &targetChunk;

				auto& entityContainer = m_entities[oldEntity.id()];
				auto* pOldChunk = entityContainer.pChunk;

				const auto oldIndex = entityContainer.idx;
				const auto newIndex = pNewChunk->AddEntity(oldEntity);

				// Transfer the disabled state
				const bool wasDisabled = pOldChunk->GetDisabledEntityMask().test(oldIndex);
				auto& newArchetype = *m_archetypes[pNewChunk->GetArchetypeId()];
				newArchetype.EnableEntity(pNewChunk, newIndex, !wasDisabled);

				// No data movement necessary when dealing with the root archetype
				if GAIA_LIKELY (pNewChunk->GetArchetypeId() + pOldChunk->GetArchetypeId() != 0) {
					// Move data from the old chunk to the new one
					if (pOldChunk->GetArchetypeId() == pNewChunk->GetArchetypeId())
						pNewChunk->MoveEntityData(oldEntity, newIndex, m_entities);
					else
						pNewChunk->MoveForeignEntityData(oldEntity, newIndex, m_entities);
				}

				// Remove the entity record from the old chunk
				RemoveEntity<false>(pOldChunk, oldIndex);

				// Make the entity point to the new chunk
				entityContainer.pChunk = pNewChunk;
				entityContainer.idx = newIndex;
				entityContainer.gen = oldEntity.gen();

				// End-state validation
				ValidateChunk(pOldChunk);
				ValidateChunk(pNewChunk);
				ValidateEntityList();
			}

			/*!
			Moves an entity along with all its generic components from its current chunk to another one in a new archetype.
			\param oldEntity Entity to move
			\param newArchetype Target archetype
			*/
			void MoveEntity(Entity oldEntity, archetype::Archetype& newArchetype) {
				auto* pNewChunk = newArchetype.FindOrCreateFreeChunk();
				return MoveEntity(oldEntity, *pNewChunk);
			}

			//! Verifies than the implicit linked list of entities is valid
			void ValidateEntityList() const {
#if GAIA_ECS_VALIDATE_ENTITY_LIST
				m_entities.validate();
#endif
			}

			//! Verifies that the chunk is valid
			void ValidateChunk([[maybe_unused]] archetype::Chunk* pChunk) const {
#if GAIA_ECS_VALIDATE_CHUNKS
				// Note: Normally we'd go [[maybe_unused]] instead of "(void)" but MSVC
				// 2017 suffers an internal compiler error in that case...
				(void)pChunk;
				GAIA_ASSERT(pChunk != nullptr);

				if (pChunk->HasEntities()) {
					// Make sure a proper amount of entities reference the chunk
					size_t cnt = 0;
					for (const auto& e: m_entities) {
						if (e.pChunk != pChunk)
							continue;
						++cnt;
					}
					GAIA_ASSERT(cnt == pChunk->GetEntityCount());
				} else {
					// Make sure no entites reference the chunk
					for (const auto& e: m_entities) {
						(void)e;
						GAIA_ASSERT(e.pChunk != pChunk);
					}
				}
#endif
			}

			EntityContainer& AddComponent_Internal(
					component::ComponentType componentType, Entity entity, const component::ComponentInfo& infoToAdd) {
				GAIA_PROF_SCOPE(AddComponent);

				auto& entityContainer = m_entities[entity.id()];

				auto* pChunk = entityContainer.pChunk;

				GAIA_ASSERT(pChunk != nullptr);
				GAIA_ASSERT(
						!pChunk->IsStructuralChangesLocked() && "New components can't be added while their chunk is being iterated "
																										"(structural changes are forbidden during this time!)");

				// Adding a component to an entity which already is a part of some chunk
				{
					auto& archetype = *m_archetypes[pChunk->GetArchetypeId()];

#if GAIA_DEBUG
					VerifyAddComponent(archetype, entity, componentType, infoToAdd);
#endif

					auto* pTargetArchetype = FindOrCreateArchetype_AddComponent(&archetype, componentType, infoToAdd);
					MoveEntity(entity, *pTargetArchetype);
					pChunk = entityContainer.pChunk;
				}

				// Call the constructor for the newly added component if necessary
				if (componentType == component::ComponentType::CT_Generic)
					pChunk->CallConstructor(componentType, infoToAdd.componentId, entityContainer.idx);
				else if (componentType == component::ComponentType::CT_Chunk)
					pChunk->CallConstructor(componentType, infoToAdd.componentId, 0);

				return entityContainer;
			}

			ComponentSetter RemoveComponent_Internal(
					component::ComponentType componentType, Entity entity, const component::ComponentInfo& infoToRemove) {
				GAIA_PROF_SCOPE(RemoveComponent);

				auto& entityContainer = m_entities[entity.id()];
				auto* pChunk = entityContainer.pChunk;

				GAIA_ASSERT(pChunk != nullptr);
				GAIA_ASSERT(
						!pChunk->IsStructuralChangesLocked() && "Components can't be removed while their chunk is being iterated "
																										"(structural changes are forbidden during this time!)");

				auto& archetype = *m_archetypes[pChunk->GetArchetypeId()];

#if GAIA_DEBUG
				VerifyRemoveComponent(archetype, entity, componentType, infoToRemove);
#endif

				auto* pNewArchetype = FindOrCreateArchetype_RemoveComponent(&archetype, componentType, infoToRemove);
				GAIA_ASSERT(pNewArchetype != nullptr);
				MoveEntity(entity, *pNewArchetype);

				return ComponentSetter{pChunk, entityContainer.idx};
			}

			void Init() {
				auto* pRootArchetype = CreateArchetype({}, {});
				pRootArchetype->Init({0}, {0}, archetype::Archetype::CalculateLookupHash({0}, {0}));
				RegisterArchetype(pRootArchetype);
			}

			void Done() {
				Cleanup();

				ChunkAllocator::Get().Flush();

#if GAIA_DEBUG && GAIA_ECS_CHUNK_ALLOCATOR
				// Make sure there are no leaks
				ChunkAllocatorStats memstats = ChunkAllocator::Get().GetStats();
				if (memstats.AllocatedMemory != 0) {
					GAIA_ASSERT(false && "ECS leaking memory");
					GAIA_LOG_W("ECS leaking memory!");
					ChunkAllocator::Get().Diag();
				}
#endif
			}

			//! Creates a new entity from archetype
			//! \return Entity
			GAIA_NODISCARD Entity CreateEntity(archetype::Archetype& archetype) {
				const auto entity = m_entities.allocate();

				const auto& entityContainer = m_entities[entity.id()];
				auto* pChunk = archetype.FindOrCreateFreeChunk();

				StoreEntity(entity, pChunk);

				// Call constructors for the generic components on the newly added entity if necessary
				if (pChunk->HasAnyCustomGenericConstructor())
					pChunk->CallConstructors(component::ComponentType::CT_Generic, entityContainer.idx, 1);

				return entity;
			}

			//! Garbage collection. Checks all chunks and archetypes which are empty and have not been
			//! used for a while and tries to delete them and release memory allocated by them.
			void GC() {
				// Handle chunks
				for (size_t i = 0; i < m_chunksToRemove.size();) {
					auto* pChunk = m_chunksToRemove[i];

					// Skip reclaimed chunks
					if (pChunk->HasEntities()) {
						pChunk->PrepareToDie();
						utils::erase_fast(m_chunksToRemove, i);
						continue;
					}

					if (pChunk->ProgressDeath()) {
						++i;
						continue;
					}
				}

				// Remove all dead chunks
				for (auto* pChunk: m_chunksToRemove) {
					auto& archetype = *m_archetypes[pChunk->GetArchetypeId()];
					archetype.RemoveChunk(pChunk);
				}
				m_chunksToRemove.clear();

#if !GAIA_AVOID_CHUNK_FRAGMENTATION
				// Defragment chunks only now. If we did this at the begging of the function
				// we would needlessly iterate chunks which have no way of being collected because
				// it would be their first frame dying.
				DefragmentChunks(100);
#endif
			}

		public:
			World() {
				Init();
			}

			~World() {
				Done();
			}

			World(World&&) = delete;
			World(const World&) = delete;
			World& operator=(World&&) = delete;
			World& operator=(const World&) = delete;

			//! Checks if \param entity is valid.
			//! \return True is the entity is valid. False otherwise.
			GAIA_NODISCARD bool IsEntityValid(Entity entity) const {
				// Entity ID has to fit inside the entity array
				if (entity.id() >= m_entities.size())
					return false;

				const auto& entityContainer = m_entities[entity.id()];

				// Generation ID has to match the one in the array
				if (entityContainer.gen != entity.gen())
					return false;

				// The entity in the chunk must match the index in the entity container
				auto* pChunk = entityContainer.pChunk;
				return pChunk != nullptr && pChunk->GetEntity(entityContainer.idx) == entity;
			}

			//! Checks if \param entity is currently used by the world.
			//! \return True is the entity is used. False otherwise.
			GAIA_NODISCARD bool IsEntityUsed(Entity entity) const {
				// Entity ID has to fit inside the entity array
				if (entity.id() >= m_entities.size())
					return false;

				// Index of the entity must fit inside the chunk
				const auto& entityContainer = m_entities[entity.id()];
				auto* pChunk = entityContainer.pChunk;
				return pChunk != nullptr && entityContainer.idx < pChunk->GetEntityCount();
			}

			//! Clears the world so that all its entities and components are released
			void Cleanup() {
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
			GAIA_NODISCARD uint32_t& GetWorldVersion() {
				return m_worldVersion;
			}

			//----------------------------------------------------------------------

			//! Creates a new empty entity
			//! \return New entity
			GAIA_NODISCARD Entity CreateEntity() {
				const auto entity = m_entities.allocate();
				auto* pChunk = m_archetypes[0]->FindOrCreateFreeChunk();
				StoreEntity(entity, pChunk);
				return entity;
			}

			//! Creates a new entity by cloning an already existing one.
			//! \param entity Entity to clone
			//! \return New entity
			GAIA_NODISCARD Entity CreateEntity(Entity entity) {
				auto& entityContainer = m_entities[entity.id()];

				auto* pChunk = entityContainer.pChunk;
				GAIA_ASSERT(pChunk != nullptr);

				auto& archetype = *m_archetypes[pChunk->GetArchetypeId()];
				const auto newEntity = CreateEntity(archetype);

				archetype::Chunk::CopyEntityData(entity, newEntity, m_entities);

				return newEntity;
			}

			//! Removes an entity along with all data associated with it.
			//! \param entity Entity to delete
			void DeleteEntity(Entity entity) {
				if (m_entities.item_count() == 0 || entity == EntityNull)
					return;

				GAIA_ASSERT(IsEntityValid(entity));

				const auto& entityContainer = m_entities[entity.id()];

				// Remove entity from chunk
				if (auto* pChunk = entityContainer.pChunk) {
					RemoveEntity<true>(pChunk, entityContainer.idx);
					ValidateChunk(pChunk);
					ValidateEntityList();
				} else {
					ReleaseEntity(entity);
				}
			}

			//! Enables or disables an entire entity.
			//! \param entity Entity
			//! \param enable Enable or disable the entity
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			void EnableEntity(Entity entity, bool enable) {
				auto& entityContainer = m_entities[entity.id()];

				GAIA_ASSERT(
						(!entityContainer.pChunk || !entityContainer.pChunk->IsStructuralChangesLocked()) &&
						"Entities can't be enabled/disabled while their chunk is being iterated "
						"(structural changes are forbidden during this time!)");

				if (auto* pChunk = entityContainer.pChunk) {
					auto& archetype = *m_archetypes[pChunk->GetArchetypeId()];
					archetype.EnableEntity(pChunk, entityContainer.idx, enable);
				}
			}

			//! Checks if an entity is valid.
			//! \param entity Entity
			//! \return True it the entity is valid. False otherwise.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			bool IsEnabled(Entity entity) const {
				GAIA_ASSERT(IsEntityValid(entity));

				const auto& entityContainer = m_entities[entity.id()];
				const auto* pChunk = entityContainer.pChunk;

				GAIA_ASSERT(pChunk != nullptr);

				return !pChunk->GetDisabledEntityMask().test(entityContainer.idx);
			}

			//! Returns the number of active entities
			//! \return Entity
			GAIA_NODISCARD GAIA_FORCEINLINE uint32_t GetEntityCount() const {
				return m_entities.item_count();
			}

			//! Returns an entity at the index \param idx
			//! \return Entity
			GAIA_NODISCARD Entity GetEntity(uint32_t idx) const {
				GAIA_ASSERT(idx < m_entities.size());
				const auto& entityContainer = m_entities[idx];
				return {idx, entityContainer.gen};
			}

			//! Returns a chunk containing the \param entity.
			//! \return Chunk or nullptr if not found.
			GAIA_NODISCARD archetype::Chunk* GetChunk(Entity entity) const {
				GAIA_ASSERT(entity.id() < m_entities.size());
				const auto& entityContainer = m_entities[entity.id()];
				return entityContainer.pChunk;
			}

			//! Returns a chunk containing the \param entity.
			//! Index of the entity is stored in \param indexInChunk
			//! \return Chunk or nullptr if not found
			GAIA_NODISCARD archetype::Chunk* GetChunk(Entity entity, uint32_t& indexInChunk) const {
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
			ComponentSetter AddComponent(Entity entity) {
				component::VerifyComponent<T>();
				GAIA_ASSERT(IsEntityValid(entity));

				using U = typename component::DeduceComponent<T>::Type;
				const auto& info = ComponentCache::Get().GetOrCreateComponentInfo<U>();

				if constexpr (component::IsGenericComponent<T>) {
					auto& entityContainer = AddComponent_Internal(component::ComponentType::CT_Generic, entity, info);
					return ComponentSetter{entityContainer.pChunk, entityContainer.idx};
				} else {
					auto& entityContainer = AddComponent_Internal(component::ComponentType::CT_Chunk, entity, info);
					return ComponentSetter{entityContainer.pChunk, entityContainer.idx};
				}
			}

			//! Attaches a new component \tparam T to \param entity. Also sets its value.
			//! \tparam T Component
			//! \param entity Entity
			//! \param value Value to set for the component
			//! \return ComponentSetter object.
			//! \warning It is expected the component is not present on \param entity yet. Undefined behavior otherwise.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			template <typename T, typename U = typename component::DeduceComponent<T>::Type>
			ComponentSetter AddComponent(Entity entity, U&& value) {
				component::VerifyComponent<T>();
				GAIA_ASSERT(IsEntityValid(entity));

				const auto& info = ComponentCache::Get().GetOrCreateComponentInfo<U>();

				if constexpr (component::IsGenericComponent<T>) {
					auto& entityContainer = AddComponent_Internal(component::ComponentType::CT_Generic, entity, info);
					auto* pChunk = entityContainer.pChunk;
					pChunk->template SetComponent<T>(entityContainer.idx, std::forward<U>(value));
					return ComponentSetter{entityContainer.pChunk, entityContainer.idx};
				} else {
					auto& entityContainer = AddComponent_Internal(component::ComponentType::CT_Chunk, entity, info);
					auto* pChunk = entityContainer.pChunk;
					pChunk->template SetComponent<T>(std::forward<U>(value));
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
			ComponentSetter RemoveComponent(Entity entity) {
				component::VerifyComponent<T>();
				GAIA_ASSERT(IsEntityValid(entity));

				using U = typename component::DeduceComponent<T>::Type;
				const auto& info = ComponentCache::Get().GetOrCreateComponentInfo<U>();

				if constexpr (component::IsGenericComponent<T>)
					return RemoveComponent_Internal(component::ComponentType::CT_Generic, entity, info);
				else
					return RemoveComponent_Internal(component::ComponentType::CT_Chunk, entity, info);
			}

			//----------------------------------------------------------------------

			//! Sets the value of the component \tparam T on \param entity.
			//! \tparam T Component
			//! \param entity Entity
			//! \param value Value to set for the component
			//! \return ComponentSetter
			//! \warning It is expected the component is present on \param entity. Undefined behavior otherwise.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			template <typename T, typename U = typename component::DeduceComponent<T>::Type>
			ComponentSetter SetComponent(Entity entity, U&& value) {
				GAIA_ASSERT(IsEntityValid(entity));

				const auto& entityContainer = m_entities[entity.id()];
				return ComponentSetter{entityContainer.pChunk, entityContainer.idx}.SetComponent<T>(std::forward<U>(value));
			}

			//! Sets the value of the component \tparam T on \param entity without trigger a world version update.
			//! \tparam T Component
			//! \param entity Entity
			//! \param value Value to set for the component
			//! \return ComponentSetter
			//! \warning It is expected the component is present on \param entity. Undefined behavior otherwise.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			template <typename T, typename U = typename component::DeduceComponent<T>::Type>
			ComponentSetter SetComponentSilent(Entity entity, U&& value) {
				GAIA_ASSERT(IsEntityValid(entity));

				const auto& entityContainer = m_entities[entity.id()];
				return ComponentSetter{entityContainer.pChunk, entityContainer.idx}.SetComponentSilent<T>(
						std::forward<U>(value));
			}

			//----------------------------------------------------------------------

			//! Returns the value stored in the component \tparam T on \param entity.
			//! \tparam T Component
			//! \param entity Entity
			//! \return Value stored in the component.
			//! \warning It is expected the component is present on \param entity. Undefined behavior otherwise.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			template <typename T>
			GAIA_NODISCARD auto GetComponent(Entity entity) const {
				GAIA_ASSERT(IsEntityValid(entity));

				const auto& entityContainer = m_entities[entity.id()];
				return ComponentGetter{entityContainer.pChunk, entityContainer.idx}.GetComponent<T>();
			}

			//! Tells if \param entity contains the component \tparam T.
			//! \tparam T Component
			//! \param entity Entity
			//! \return True if the component is present on entity.
			//! \warning It is expected \param entity is valid. Undefined behavior otherwise.
			template <typename T>
			GAIA_NODISCARD bool HasComponent(Entity entity) const {
				GAIA_ASSERT(IsEntityValid(entity));

				const auto& entityContainer = m_entities[entity.id()];
				return ComponentGetter{entityContainer.pChunk, entityContainer.idx}.HasComponent<T>();
			}

			//----------------------------------------------------------------------

		private:
			template <typename T>
			GAIA_NODISCARD constexpr GAIA_FORCEINLINE auto GetComponentView(archetype::Chunk& chunk) const {
				using U = typename component::DeduceComponent<T>::Type;
				using UOriginal = typename component::DeduceComponent<T>::TypeOriginal;
				if constexpr (component::IsReadOnlyType<UOriginal>::value)
					return chunk.View_Internal<U>();
				else
					return chunk.ViewRW_Internal<U, true>();
			}

			//--------------------------------------------------------------------------------

			template <typename... T>
			void UnpackArgsIntoQuery(Query& query, [[maybe_unused]] utils::func_type_list<T...> types) const {
				static_assert(sizeof...(T) > 0, "Inputs-less functors can not be unpacked to query");
				query.All<T...>();
			}

			template <typename... T>
			static void RegisterComponents_Internal([[maybe_unused]] utils::func_type_list<T...> types) {
				static_assert(sizeof...(T) > 0, "Empty Query is not supported in this context");
				auto& cc = ComponentCache::Get();
				((void)cc.GetOrCreateComponentInfo<T>(), ...);
			}

			template <typename Func>
			static void RegisterComponents() {
				using InputArgs = decltype(utils::func_args(&Func::operator()));
				RegisterComponents_Internal(InputArgs{});
			}

			template <typename... T>
			static constexpr component::ComponentLookupHash
			CalculateQueryIdLookupHash([[maybe_unused]] utils::func_type_list<T...> types) {
				return component::CalculateLookupHash<T...>();
			}

		public:
			Query CreateQuery() {
				return Query(m_queryCache, m_worldVersion, m_archetypes, m_componentToArchetypeMap);
			}

			//! Iterates over all chunks satisfying conditions set by \param func and calls \param func for all of them.
			//! Query instance is generated internally from the input arguments of \param func.
			//! \warning Performance-wise it has less potential than iterating using ecs::Chunk or a comparable ForEach
			//!          passing in a query because it needs to do cached query lookups on each invocation. However, it is
			//!          easier to use and for non-critical code paths it is the most elegant way of iterating your data.
			template <typename Func>
			void ForEach(Func func) {
				using InputArgs = decltype(utils::func_args(&Func::operator()));

				RegisterComponents<Func>();

				constexpr auto lookupHash = CalculateQueryIdLookupHash(InputArgs{});
				if (m_uniqueFuncQueryPairs.count(lookupHash) == 0) {
					Query query = CreateQuery();
					UnpackArgsIntoQuery(query, InputArgs{});
					(void)query.FetchQueryInfo();
					m_uniqueFuncQueryPairs.try_emplace(lookupHash, query.GetQueryId());
					CreateQuery().ForEach(query.GetQueryId(), func);
				} else {
					const auto queryId = m_uniqueFuncQueryPairs[lookupHash];
					CreateQuery().ForEach(queryId, func);
				}
			}

			//! Performs various internal operations related to the end of the frame such as
			//! memory cleanup and other various managment operations which keep the system healthy.
			void Update() {
				GC();

				// Signal the end of the frame
				GAIA_PROF_FRAME();
			}

			//--------------------------------------------------------------------------------

			//! Performs diagnostics on archetypes. Prints basic info about them and the chunks they contain.
			void DiagArchetypes() const {
				GAIA_LOG_N("Archetypes:%u", (uint32_t)m_archetypes.size());
				for (const auto* archetype: m_archetypes)
					archetype::Archetype::DiagArchetype(*archetype);
			}

			//! Performs diagnostics on registered components.
			//! Prints basic info about them and reports and detected issues.
			static void DiagRegisteredTypes() {
				ComponentCache::Get().Diag();
			}

			//! Performs diagnostics on entites of the world.
			//! Also performs validation of internal structures which hold the entities.
			void DiagEntities() const {
				ValidateEntityList();

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
			void Diag() const {
				DiagArchetypes();
				DiagRegisteredTypes();
				DiagEntities();
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

		Adding and removing components and entities inside World::ForEach or can result
		in changes of archetypes or chunk structure. This would lead to undefined behavior.
		Therefore, such operations have to be executed after the loop is done.
		*/
		class CommandBuffer final {
			struct CommandBufferCtx: DataBuffer_SerializationWrapper {
				ecs::World& world;
				uint32_t entities;
				containers::map<uint32_t, Entity> entityMap;
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

				void Commit(CommandBufferCtx& ctx) const {
					auto* pArchetype = (archetype::Archetype*)archetypePtr;
					[[maybe_unused]] const auto res =
							ctx.entityMap.try_emplace(ctx.entities++, ctx.world.CreateEntity(*pArchetype));
					GAIA_ASSERT(res.second);
				}
			};
			struct CREATE_ENTITY_FROM_ENTITY_t: CommandBufferCmd_t {
				Entity entity;

				void Commit(CommandBufferCtx& ctx) const {
					[[maybe_unused]] const auto res = ctx.entityMap.try_emplace(ctx.entities++, ctx.world.CreateEntity(entity));
					GAIA_ASSERT(res.second);
				}
			};
			struct DELETE_ENTITY_t: CommandBufferCmd_t {
				Entity entity;

				void Commit(CommandBufferCtx& ctx) const {
					ctx.world.DeleteEntity(entity);
				}
			};
			struct ADD_COMPONENT_t: CommandBufferCmd_t {
				Entity entity;
				component::ComponentId componentId;
				component::ComponentType componentType;

				void Commit(CommandBufferCtx& ctx) const {
					const auto& info = ComponentCache::Get().GetComponentInfo(componentId);
					ctx.world.AddComponent_Internal(componentType, entity, info);

#if GAIA_ASSERT_ENABLED
					[[maybe_unused]] uint32_t indexInChunk{};
					[[maybe_unused]] auto* pChunk = ctx.world.GetChunk(entity, indexInChunk);
					GAIA_ASSERT(pChunk != nullptr);
#endif
				}
			};
			struct ADD_COMPONENT_DATA_t: CommandBufferCmd_t {
				Entity entity;
				component::ComponentId componentId;
				component::ComponentType componentType;

				void Commit(CommandBufferCtx& ctx) const {
					const auto& info = ComponentCache::Get().GetComponentInfo(componentId);
					ctx.world.AddComponent_Internal(componentType, entity, info);

					uint32_t indexInChunk{};
					auto* pChunk = ctx.world.GetChunk(entity, indexInChunk);
					GAIA_ASSERT(pChunk != nullptr);

					if (componentType == component::ComponentType::CT_Chunk)
						indexInChunk = 0;

					// Component data
					const auto& desc = ComponentCache::Get().GetComponentDesc(componentId);
					const auto offset = pChunk->FindDataOffset(componentType, info.componentId);
					auto* pComponentData = (void*)&pChunk->GetData(offset + (uint32_t)indexInChunk * desc.properties.size);
					ctx.buffer().LoadComponent(pComponentData, componentId);
				}
			};
			struct ADD_COMPONENT_TO_TEMPENTITY_t: CommandBufferCmd_t {
				TempEntity tempEntity;
				component::ComponentId componentId;
				component::ComponentType componentType;

				void Commit(CommandBufferCtx& ctx) const {
					// For delayed entities we have to do a look in our map
					// of temporaries and find a link there
					const auto it = ctx.entityMap.find(tempEntity.id);
					// Link has to exist!
					GAIA_ASSERT(it != ctx.entityMap.end());

					Entity entity = it->second;

					const auto& info = ComponentCache::Get().GetComponentInfo(componentId);
					ctx.world.AddComponent_Internal(componentType, entity, info);

#if GAIA_ASSERT_ENABLED
					[[maybe_unused]] uint32_t indexInChunk{};
					[[maybe_unused]] auto* pChunk = ctx.world.GetChunk(entity, indexInChunk);
					GAIA_ASSERT(pChunk != nullptr);
#endif
				}
			};
			struct ADD_COMPONENT_TO_TEMPENTITY_DATA_t: CommandBufferCmd_t {
				TempEntity tempEntity;
				component::ComponentId componentId;
				component::ComponentType componentType;

				void Commit(CommandBufferCtx& ctx) const {
					// For delayed entities we have to do a look in our map
					// of temporaries and find a link there
					const auto it = ctx.entityMap.find(tempEntity.id);
					// Link has to exist!
					GAIA_ASSERT(it != ctx.entityMap.end());

					Entity entity = it->second;

					// Components
					const auto& info = ComponentCache::Get().GetComponentInfo(componentId);
					ctx.world.AddComponent_Internal(componentType, entity, info);

					uint32_t indexInChunk{};
					auto* pChunk = ctx.world.GetChunk(entity, indexInChunk);
					GAIA_ASSERT(pChunk != nullptr);

					if (componentType == component::ComponentType::CT_Chunk)
						indexInChunk = 0;

					// Component data
					const auto& desc = ComponentCache::Get().GetComponentDesc(componentId);
					const auto offset = pChunk->FindDataOffset(componentType, desc.componentId);
					auto* pComponentData = (void*)&pChunk->GetData(offset + (uint32_t)indexInChunk * desc.properties.size);
					ctx.buffer().LoadComponent(pComponentData, componentId);
				}
			};
			struct SET_COMPONENT_t: CommandBufferCmd_t {
				Entity entity;
				component::ComponentId componentId;
				component::ComponentType componentType;

				void Commit(CommandBufferCtx& ctx) const {
					const auto& entityContainer = ctx.world.m_entities[entity.id()];
					auto* pChunk = entityContainer.pChunk;
					const auto indexInChunk = componentType == component::ComponentType::CT_Chunk ? 0U : entityContainer.idx;

					// Component data
					const auto& desc = ComponentCache::Get().GetComponentDesc(componentId);
					const auto offset = pChunk->FindDataOffset(componentType, componentId);
					auto* pComponentData = (void*)&pChunk->GetData(offset + (uint32_t)indexInChunk * desc.properties.size);
					ctx.buffer().LoadComponent(pComponentData, componentId);
				}
			};
			struct SET_COMPONENT_FOR_TEMPENTITY_t: CommandBufferCmd_t {
				TempEntity tempEntity;
				component::ComponentId componentId;
				component::ComponentType componentType;

				void Commit(CommandBufferCtx& ctx) const {
					// For delayed entities we have to do a look in our map
					// of temporaries and find a link there
					const auto it = ctx.entityMap.find(tempEntity.id);
					// Link has to exist!
					GAIA_ASSERT(it != ctx.entityMap.end());

					Entity entity = it->second;

					const auto& entityContainer = ctx.world.m_entities[entity.id()];
					auto* pChunk = entityContainer.pChunk;
					const auto indexInChunk = componentType == component::ComponentType::CT_Chunk ? 0U : entityContainer.idx;

					// Component data
					const auto& desc = ComponentCache::Get().GetComponentDesc(componentId);
					const auto offset = pChunk->FindDataOffset(componentType, componentId);
					auto* pComponentData = (void*)&pChunk->GetData(offset + (uint32_t)indexInChunk * desc.properties.size);
					ctx.buffer().LoadComponent(pComponentData, componentId);
				}
			};
			struct REMOVE_COMPONENT_t: CommandBufferCmd_t {
				Entity entity;
				component::ComponentId componentId;
				component::ComponentType componentType;

				void Commit(CommandBufferCtx& ctx) const {
					const auto& info = ComponentCache::Get().GetComponentInfo(componentId);
					ctx.world.RemoveComponent_Internal(componentType, entity, info);
				}
			};

			friend class World;

			World& m_world;
			DataBuffer m_data;
			uint32_t m_entities;

			/*!
			Requests a new entity to be created from archetype
			\return Entity that will be created. The id is not usable right away. It
			will be filled with proper data after Commit()
			*/
			GAIA_NODISCARD TempEntity CreateEntity(archetype::Archetype& archetype) {
				DataBuffer_SerializationWrapper s(m_data);
				s.save(CREATE_ENTITY_FROM_ARCHETYPE);

				CREATE_ENTITY_FROM_ARCHETYPE_t cmd;
				cmd.archetypePtr = (uintptr_t)&archetype;
				serialization::save(s, cmd);

				return {m_entities++};
			}

		public:
			CommandBuffer(World& world): m_world(world), m_entities(0) {}
			~CommandBuffer() = default;

			CommandBuffer(CommandBuffer&&) = delete;
			CommandBuffer(const CommandBuffer&) = delete;
			CommandBuffer& operator=(CommandBuffer&&) = delete;
			CommandBuffer& operator=(const CommandBuffer&) = delete;

			/*!
			Requests a new entity to be created
			\return Entity that will be created. The id is not usable right away. It
			will be filled with proper data after Commit()
			*/
			GAIA_NODISCARD TempEntity CreateEntity() {
				DataBuffer_SerializationWrapper s(m_data);
				s.save(CREATE_ENTITY);

				return {m_entities++};
			}

			/*!
			Requests a new entity to be created by cloning an already existing
			entity \return Entity that will be created. The id is not usable right
			away. It will be filled with proper data after Commit()
			*/
			GAIA_NODISCARD TempEntity CreateEntity(Entity entityFrom) {
				DataBuffer_SerializationWrapper s(m_data);
				s.save(CREATE_ENTITY_FROM_ENTITY);

				CREATE_ENTITY_FROM_ENTITY_t cmd;
				cmd.entity = entityFrom;
				serialization::save(s, cmd);

				return {m_entities++};
			}

			/*!
			Requests an existing \param entity to be removed.
			*/
			void DeleteEntity(Entity entity) {
				DataBuffer_SerializationWrapper s(m_data);
				s.save(DELETE_ENTITY);

				DELETE_ENTITY_t cmd;
				cmd.entity = entity;
				serialization::save(s, cmd);
			}

			/*!
			Requests a component to be added to entity.
			*/
			template <typename T>
			void AddComponent(Entity entity) {
				// Make sure the component is registered
				const auto& info = ComponentCache::Get().GetOrCreateComponentInfo<T>();

				using U = typename component::DeduceComponent<T>::Type;
				component::VerifyComponent<U>();

				DataBuffer_SerializationWrapper s(m_data);
				s.save(ADD_COMPONENT);

				ADD_COMPONENT_t cmd;
				cmd.entity = entity;
				cmd.componentType = component::GetComponentType<T>();
				cmd.componentId = info.componentId;
				serialization::save(s, cmd);
			}

			/*!
			Requests a component to be added to temporary entity.
			*/
			template <typename T>
			void AddComponent(TempEntity entity) {
				// Make sure the component is registered
				const auto& info = ComponentCache::Get().GetOrCreateComponentInfo<T>();

				using U = typename component::DeduceComponent<T>::Type;
				component::VerifyComponent<U>();

				DataBuffer_SerializationWrapper s(m_data);
				s.save(ADD_COMPONENT_TO_TEMPENTITY);

				ADD_COMPONENT_TO_TEMPENTITY_t cmd;
				cmd.tempEntity = entity;
				cmd.componentType = component::GetComponentType<T>();
				cmd.componentId = component::GetComponentId<T>();
				serialization::save(s, cmd);
			}

			/*!
			Requests a component to be added to entity.
			*/
			template <typename T>
			void AddComponent(Entity entity, T&& value) {
				// Make sure the component is registered
				const auto& info = ComponentCache::Get().GetOrCreateComponentInfo<T>();

				using U = typename component::DeduceComponent<T>::Type;
				component::VerifyComponent<U>();

				DataBuffer_SerializationWrapper s(m_data);
				s.save(ADD_COMPONENT_DATA);

				ADD_COMPONENT_DATA_t cmd;
				cmd.entity = entity;
				cmd.componentType = component::GetComponentType<T>();
				cmd.componentId = info.componentId;
				serialization::save(s, cmd);
				s.buffer().SaveComponent(std::forward<U>(value));
			}

			/*!
			Requests a component to be added to temporary entity.
			*/
			template <typename T>
			void AddComponent(TempEntity entity, T&& value) {
				// Make sure the component is registered
				const auto& info = ComponentCache::Get().GetOrCreateComponentInfo<T>();

				using U = typename component::DeduceComponent<T>::Type;
				component::VerifyComponent<U>();

				DataBuffer_SerializationWrapper s(m_data);
				s.save(ADD_COMPONENT_TO_TEMPENTITY_DATA);

				ADD_COMPONENT_TO_TEMPENTITY_t cmd;
				cmd.tempEntity = entity;
				cmd.componentType = component::GetComponentType<T>();
				cmd.componentId = info.componentId;
				serialization::save(s, cmd);
				s.buffer().SaveComponent(std::forward<U>(value));
			}

			/*!
			Requests component data to be set to given values for a given entity.
			*/
			template <typename T>
			void SetComponent(Entity entity, T&& value) {
				// No need to check if the component is registered.
				// If we want to set the value of a component we must have created it already.
				// (void)ComponentCache::Get().GetComponentInfo<T>();

				using U = typename component::DeduceComponent<T>::Type;
				component::VerifyComponent<U>();

				DataBuffer_SerializationWrapper s(m_data);
				s.save(SET_COMPONENT);

				SET_COMPONENT_t cmd;
				cmd.entity = entity;
				cmd.componentType = component::GetComponentType<T>();
				cmd.componentId = component::GetComponentId<T>();
				serialization::save(s, cmd);
				s.buffer().SaveComponent(std::forward<U>(value));
			}

			/*!
			Requests component data to be set to given values for a given temp
			entity.
			*/
			template <typename T>
			void SetComponent(TempEntity entity, T&& value) {
				// No need to check if the component is registered.
				// If we want to set the value of a component we must have created it already.
				// (void)ComponentCache::Get().GetOrCreateComponentInfo<T>();

				using U = typename component::DeduceComponent<T>::Type;
				component::VerifyComponent<U>();

				DataBuffer_SerializationWrapper s(m_data);
				s.save(SET_COMPONENT_FOR_TEMPENTITY);

				SET_COMPONENT_FOR_TEMPENTITY_t cmd;
				cmd.tempEntity = entity;
				cmd.componentType = component::GetComponentType<T>();
				cmd.componentId = component::GetComponentId<T>();
				serialization::save(s, cmd);
				s.buffer().SaveComponent(std::forward<U>(value));
			}

			/*!
			Requests removal of a component from entity
			*/
			template <typename T>
			void RemoveComponent(Entity entity) {
				// No need to check if the component is registered.
				// If we want to remove a component we must have created it already.
				// (void)ComponentCache::Get().GetOrCreateComponentInfo<T>();

				using U = typename component::DeduceComponent<T>::Type;
				component::VerifyComponent<U>();

				DataBuffer_SerializationWrapper s(m_data);
				s.save(REMOVE_COMPONENT);

				REMOVE_COMPONENT_t cmd;
				cmd.entity = entity;
				cmd.componentType = component::GetComponentType<T>();
				cmd.componentId = component::GetComponentId<T>();
				serialization::save(s, cmd);
			}

		private:
			using CommandBufferReadFunc = void (*)(CommandBufferCtx& ctx);
			static constexpr CommandBufferReadFunc CommandBufferRead[] = {
					// CREATE_ENTITY
					[](CommandBufferCtx& ctx) {
						[[maybe_unused]] const auto res = ctx.entityMap.try_emplace(ctx.entities++, ctx.world.CreateEntity());
						GAIA_ASSERT(res.second);
					},
					// CREATE_ENTITY_FROM_ARCHETYPE
					[](CommandBufferCtx& ctx) {
						CREATE_ENTITY_FROM_ARCHETYPE_t cmd;
						serialization::load(ctx, cmd);
						cmd.Commit(ctx);
					},
					// CREATE_ENTITY_FROM_ENTITY
					[](CommandBufferCtx& ctx) {
						CREATE_ENTITY_FROM_ENTITY_t cmd;
						serialization::load(ctx, cmd);
						cmd.Commit(ctx);
					},
					// DELETE_ENTITY
					[](CommandBufferCtx& ctx) {
						DELETE_ENTITY_t cmd;
						serialization::load(ctx, cmd);
						cmd.Commit(ctx);
					},
					// ADD_COMPONENT
					[](CommandBufferCtx& ctx) {
						ADD_COMPONENT_t cmd;
						serialization::load(ctx, cmd);
						cmd.Commit(ctx);
					},
					// ADD_COMPONENT_DATA
					[](CommandBufferCtx& ctx) {
						ADD_COMPONENT_DATA_t cmd;
						serialization::load(ctx, cmd);
						cmd.Commit(ctx);
					},
					// ADD_COMPONENT_TO_TEMPENTITY
					[](CommandBufferCtx& ctx) {
						ADD_COMPONENT_TO_TEMPENTITY_t cmd;
						serialization::load(ctx, cmd);
						cmd.Commit(ctx);
					},
					// ADD_COMPONENT_TO_TEMPENTITY_DATA
					[](CommandBufferCtx& ctx) {
						ADD_COMPONENT_TO_TEMPENTITY_DATA_t cmd;
						serialization::load(ctx, cmd);
						cmd.Commit(ctx);
					},
					// SET_COMPONENT
					[](CommandBufferCtx& ctx) {
						SET_COMPONENT_t cmd;
						serialization::load(ctx, cmd);
						cmd.Commit(ctx);
					},
					// SET_COMPONENT_FOR_TEMPENTITY
					[](CommandBufferCtx& ctx) {
						SET_COMPONENT_FOR_TEMPENTITY_t cmd;
						serialization::load(ctx, cmd);
						cmd.Commit(ctx);
					},
					// REMOVE_COMPONENT
					[](CommandBufferCtx& ctx) {
						REMOVE_COMPONENT_t cmd;
						serialization::load(ctx, cmd);
						cmd.Commit(ctx);
					}};

		public:
			/*!
			Commits all queued changes.
			*/
			void Commit() {
				CommandBufferCtx ctx{m_data, m_world, 0, {}};

				// Extract data from the buffer
				m_data.Seek(0);
				while (m_data.GetPos() < m_data.Size()) {
					CommandBufferCmd id{};
					ctx.load(id);
					CommandBufferRead[id](ctx);
				}

				m_entities = 0;
				m_data.Reset();
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
		constexpr size_t MaxSystemNameLength = 64;
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
			GAIA_NODISCARD World& GetWorld() {
				return *m_world;
			}
			GAIA_NODISCARD const World& GetWorld() const {
				return *m_world;
			}

			//! Enable/disable system
			void Enable(bool enable) {
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
			GAIA_NODISCARD bool IsEnabled() const {
				return m_enabled;
			}

		protected:
			//! Called when system is first created
			virtual void OnCreated() {}
			//! Called every time system is started (before the first run and after
			//! Enable(true) is called
			virtual void OnStarted() {}

			//! Called right before every OnUpdate()
			virtual void BeforeOnUpdate() {}
			//! Called every time system is allowed to tick
			virtual void OnUpdate() {}
			//! Called aright after every OnUpdate()
			virtual void AfterOnUpdate() {}

			//! Called every time system is stopped (after Enable(false) is called and
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
			void SetDestroyed(bool destroy) {
				m_destroy = destroy;
			}
			bool IsDestroyed() const {
				return m_destroy;
			}
		};

		class BaseSystemManager {
		protected:
			using SystemHash = utils::direct_hash_key<uint64_t>;

			World& m_world;
			//! Map of all systems - used for look-ups only
			containers::map<SystemHash, BaseSystem*> m_systemsMap;
			//! List of system - used for iteration
			containers::darray<BaseSystem*> m_systems;
			//! List of new systems which need to be initialised
			containers::darray<BaseSystem*> m_systemsToCreate;
			//! List of systems which need to be deleted
			containers::darray<BaseSystem*> m_systemsToRemove;

		public:
			BaseSystemManager(World& world): m_world(world) {}
			~BaseSystemManager() {
				Clear();
			}

			BaseSystemManager(BaseSystemManager&& world) = delete;
			BaseSystemManager(const BaseSystemManager& world) = delete;
			BaseSystemManager& operator=(BaseSystemManager&&) = delete;
			BaseSystemManager& operator=(const BaseSystemManager&) = delete;

			void Clear() {
				for (auto* pSystem: m_systems)
					pSystem->Enable(false);
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

			void Cleanup() {
				for (auto& s: m_systems)
					s->OnCleanup();
			}

			void Update() {
				// Remove all systems queued to be destroyed
				for (auto* pSystem: m_systemsToRemove)
					pSystem->Enable(false);
				for (auto* pSystem: m_systemsToRemove)
					pSystem->OnCleanup();
				for (auto* pSystem: m_systemsToRemove)
					pSystem->OnDestroyed();
				for (auto* pSystem: m_systemsToRemove)
					m_systemsMap.erase({pSystem->m_hash});
				for (auto* pSystem: m_systemsToRemove) {
					m_systems.erase(utils::find(m_systems, pSystem));
				}
				for (auto* pSystem: m_systemsToRemove)
					delete pSystem;
				m_systemsToRemove.clear();

				if GAIA_UNLIKELY (!m_systemsToCreate.empty()) {
					// Sort systems if necessary
					SortSystems();

					// Create all new systems
					for (auto* pSystem: m_systemsToCreate) {
						pSystem->OnCreated();
						if (pSystem->IsEnabled())
							pSystem->OnStarted();
					}
					m_systemsToCreate.clear();
				}

				OnBeforeUpdate();

				for (auto* pSystem: m_systems) {
					if (!pSystem->IsEnabled())
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
			T* CreateSystem([[maybe_unused]] const char* name = nullptr) {
				GAIA_SAFE_CONSTEXPR auto hash = utils::type_info::hash<std::decay_t<T>>();

				const auto res = m_systemsMap.try_emplace({hash}, nullptr);
				if GAIA_UNLIKELY (!res.second)
					return (T*)res.first->second;

				BaseSystem* pSystem = new T();
				pSystem->m_world = &m_world;

#if GAIA_PROFILER_CPU
				if (name == nullptr) {
					constexpr auto ct_name = utils::type_info::name<T>();
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
			void RemoveSystem() {
				auto pSystem = FindSystem<T>();
				if (pSystem == nullptr || pSystem->IsDestroyed())
					return;

				pSystem->SetDestroyed(true);

				// Request removal of the system
				m_systemsToRemove.push_back(pSystem);
			}

			template <typename T>
			GAIA_NODISCARD T* FindSystem() {
				GAIA_SAFE_CONSTEXPR auto hash = utils::type_info::hash<std::decay_t<T>>();

				const auto it = m_systemsMap.find({hash});
				if (it != m_systemsMap.end())
					return (T*)it->second;

				return (T*)nullptr;
			}

		protected:
			virtual void OnBeforeUpdate() {}
			virtual void OnAfterUpdate() {}

		private:
			void SortSystems() {
				for (size_t l = 0; l < m_systems.size() - 1; l++) {
					auto min = l;
					for (size_t p = l + 1; p < m_systems.size(); p++) {
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
				for (auto j = 1U; j < m_systems.size(); j++) {
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
				m_beforeUpdateCmdBuffer.Commit();
			}

			void OnAfterUpdate() final {
				m_afterUpdateCmdBuffer.Commit();
			}
		};
	} // namespace ecs
} // namespace gaia

