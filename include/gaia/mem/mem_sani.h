#pragma once

#if __has_feature(address_sanitizer) || defined(USE_SANITIZER)
	// #ifndef __SANITIZE_ADDRESS__
	// 	#define __SANITIZE_ADDRESS__
	// #endif
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
