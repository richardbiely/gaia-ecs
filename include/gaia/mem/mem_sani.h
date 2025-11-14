#pragma once

#ifndef GAIA_USE_MEM_SANI
	#if defined(__has_feature)
		#if __has_feature(address_sanitizer)
			#define GAIA_HAS_SANI_FEATURE 1
		#else
			#define GAIA_HAS_SANI_FEATURE 0
		#endif
		#if (GAIA_HAS_SANI_FEATURE && GAIA_USE_SANITIZER) || defined(__SANITIZE_ADDRESS__)
			#define GAIA_USE_MEM_SANI 1
		#else
			#define GAIA_USE_MEM_SANI 0
		#endif
	#else
		#if GAIA_USE_SANITIZER || defined(__SANITIZE_ADDRESS__)
			#define GAIA_USE_MEM_SANI 1
		#else
			#define GAIA_USE_MEM_SANI 0
		#endif
	#endif
#endif

#if GAIA_USE_MEM_SANI
	#include <sanitizer/asan_interface.h>

	// Poison a new contiguous block of memory
	#define GAIA_MEM_SANI_ADD_BLOCK(type_size, ptr, cap, size)                                                           \
		{                                                                                                                  \
			if (ptr != nullptr)                                                                                              \
				__sanitizer_annotate_contiguous_container(                                                                     \
						ptr, /**/                                                                                                  \
						(unsigned char*)(ptr) + ((cap) * type_size), /**/                                                          \
						ptr, /**/                                                                                                  \
						(unsigned char*)(ptr) + ((size) * type_size))                                                              \
		}
	// Unpoison an existing contiguous block of buffer
	#define GAIA_MEM_SANI_DEL_BLOCK(type_size, ptr, cap, size)                                                           \
		{                                                                                                                  \
			if (ptr != nullptr)                                                                                              \
				__sanitizer_annotate_contiguous_container(                                                                     \
						ptr, /**/                                                                                                  \
						(unsigned char*)(ptr) + ((cap) * type_size), /**/                                                          \
						(unsigned char*)(ptr) + ((size) * type_size), /**/                                                         \
						ptr)                                                                                                       \
		}

	// Unpoison memory for N new elements, use before adding the elements
	#define GAIA_MEM_SANI_PUSH_N(type_size, ptr, cap, size, diff)                                                        \
		{                                                                                                                  \
			if (ptr != nullptr)                                                                                              \
				__sanitizer_annotate_contiguous_container(                                                                     \
						ptr, /**/                                                                                                  \
						(unsigned char*)(ptr) + ((cap) * type_size), /**/                                                          \
						(unsigned char*)(ptr) + ((size) * type_size), /**/                                                         \
						(unsigned char*)(ptr) + (((size) + (diff)) * type_size))                                                   \
		}
	// Poison memory for last N elements, use after removing the elements
	#define GAIA_MEM_SANI_POP_N(type_size, ptr, cap, size, diff)                                                         \
		{                                                                                                                  \
			if (ptr != nullptr)                                                                                              \
				__sanitizer_annotate_contiguous_container(                                                                     \
						ptr, /**/                                                                                                  \
						(unsigned char*)(ptr) + ((cap) * type_size), /**/                                                          \
						(unsigned char*)(ptr) + ((size) * type_size), /**/                                                         \
						(unsigned char*)(ptr) + (((size) - (diff)) * type_size))                                                   \
		}

	// Unpoison memory for a new element, use before adding it
	#define GAIA_MEM_SANI_PUSH(type_size, ptr, cap, size) GAIA_MEM_SANI_PUSH_N(type_size, ptr, cap, size, 1)
	// Poison memory for the last elements, use after removing it
	#define GAIA_MEM_SANI_POP(type_size, ptr, cap, size) GAIA_MEM_SANI_POP_N(type_size, ptr, cap, size, 1)

#else
	#define GAIA_MEM_SANI_ADD_BLOCK(type_size, ptr, cap, size) ((void)type_size, (void)(ptr), (void)(cap), (void)(size))
	#define GAIA_MEM_SANI_DEL_BLOCK(type_size, ptr, cap, size) ((void)type_size, (void)(ptr), (void)(cap), (void)(size))
	#define GAIA_MEM_SANI_PUSH_N(type_size, ptr, cap, size, diff)                                                        \
		((void)type_size, (void)(ptr), (void)(cap), (void)(size), (void)(diff))
	#define GAIA_MEM_SANI_POP_N(type_size, ptr, cap, size, diff)                                                         \
		((void)type_size, (void)(ptr), (void)(cap), (void)(size), (void)(diff))
	#define GAIA_MEM_SANI_PUSH(type_size, ptr, cap, size) ((void)type_size, (void)(ptr), (void)(cap), (void)(size))
	#define GAIA_MEM_SANI_POP(type_size, ptr, cap, size) ((void)type_size, (void)(ptr), (void)(cap), (void)(size))
#endif
