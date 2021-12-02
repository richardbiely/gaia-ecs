#pragma once
#include "version.h"

//------------------------------------------------------------------------------
// General settings
//------------------------------------------------------------------------------

#define GAIA_DEBUG _DEBUG
#define GAIA_DISABLE_ASSERTS 0
#define GAIA_PROFILER 0
#define GAIA_ECS_DIAGS 1
#define GAIA_ECS_CHUNK_ALLOCATOR 1
#define GAIA_USE_STL_CONTAINERS 0

//------------------------------------------------------------------------------
// Debug features
//------------------------------------------------------------------------------

#define GAIA_ECS_CHUNK_ALLOCATOR_CLEAN_MEMORY_WITH_GARBAGE GAIA_DEBUG
#define GAIA_ECS_VALIDATE_CHUNKS GAIA_DEBUG
#define GAIA_ECS_VALIDATE_ENTITY_LIST GAIA_DEBUG

//------------------------------------------------------------------------------
// Compiler
//------------------------------------------------------------------------------
#define GAIA_COMPILER_CLANG 0
#define GAIA_COMPILER_GCC 0
#define GAIA_COMPILER_MSVC 0

#if defined(__clang__)
// Clang check is performed first as it might pretend to be MSVC or GCC by
// defining their predefined macros.
	#undef GAIA_COMPILER_CLANG
	#define GAIA_COMPILER_CLANG 1
#elif defined(__SNC__) || defined(__GNUC__)
	#undef GAIA_COMPILER_GCC
	#define GAIA_COMPILER_GCC 1
#elif defined(_MSC_VER)
	#undef GAIA_COMPILER_MSVC
	#define GAIA_COMPILER_MSVC 1
#else
	#error "Unrecognized compiler"
#endif

//------------------------------------------------------------------------------
// Architecture and architecture features
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
#elif defined(__arm__) || defined(__aarch64__)
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

//------------------------------------------------------------------------------

#if (GAIA_COMPILER_MSVC && _MSC_VER >= 1400) || GAIA_COMPILER_GCC || GAIA_COMPILER_CLANG
	#define GAIA_RESTRICT __restrict
#else
	#define GAIA_RESTRICT
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
// Features
//------------------------------------------------------------------------------
#if defined(GAIA_PROFILER)
// ...
#else
// ...
#endif

#if !defined(GAIA_DISABLE_ASSERTS)
	#undef GAIA_ASSERT
	#define GAIA_ASSERT(condition) (void(0))
#elif !defined(GAIA_ASSERT)
	#include <cassert>
	#define GAIA_ASSERT(condition) assert(condition)
#endif

#if defined(GAIA_ECS_DIAGS)
	#undef GAIA_ECS_DIAG_ARCHETYPES
	#undef GAIA_ECS_DIAG_REGISTERED_TYPES
	#undef GAIA_ECS_DIAG_DELETED_ENTITIES

	#define GAIA_ECS_DIAG_ARCHETYPES 1
	#define GAIA_ECS_DIAG_REGISTERED_TYPES 1
	#define GAIA_ECS_DIAG_DELETED_ENTITIES 1
#else
	#if !defined(GAIA_ECS_DIAG_ARCHETYPES)
		#define GAIA_ECS_DIAG_ARCHETYPES 0
	#endif
	#if !defined(GAIA_ECS_DIAG_REGISTERED_TYPES)
		#define GAIA_ECS_DIAG_REGISTERED_TYPES 0
	#endif
	#if !defined(GAIA_ECS_DIAG_DELETED_ENTITIES)
		#define GAIA_ECS_DIAG_DELETED_ENTITIES 0
	#endif
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
	#define GAIA_CLANG_WARNING_DISABLE(warningId) _Pragma(GAIA_STRINGIZE_MACRO(clang diagnostic ignored #warningId))
	#define GAIA_CLANG_WARNING_ERROR(warningId) _Pragma(GAIA_STRINGIZE_MACRO(clang diagnostic error #warningId))
	#define GAIA_CLANG_WARNING_ALLOW(warningId) _Pragma(GAIA_STRINGIZE_MACRO(clang diagnostic warning #warningId))
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
	#define GAIA_GCC_WARNING_ERROR(warningId) _Pragma(GAIA_STRINGIZE_MACRO(GCC diagnostic error warningId))
	#define GAIA_GCC_WARNING_DISABLE(warningId) DO_PRAGMA(GCC diagnostic ignored #warningId)
#else
	#define GAIA_GCC_WARNING_PUSH()
	#define GAIA_GCC_WARNING_POP()
	#define GAIA_GCC_WARNING_DISABLE(warningId)
#endif
