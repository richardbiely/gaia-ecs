#pragma once

#include <cstdio> // vsnprintf, sscanf, printf

//! Log - debug
#define LOG_D(...)                                                             \
	{                                                                            \
		fprintf(stdout, "D: ");                                                    \
		fprintf(stdout, __VA_ARGS__);                                              \
		fprintf(stdout, "\n");                                                     \
	}
//! Log - normal/informational
#define LOG_N(...)                                                             \
	{                                                                            \
		fprintf(stdout, "I: ");                                                    \
		fprintf(stdout, __VA_ARGS__);                                              \
		fprintf(stdout, "\n");                                                     \
	}
//! Log - warning
#define LOG_W(...)                                                             \
	{                                                                            \
		fprintf(stderr, "W: ");                                                    \
		fprintf(stderr, __VA_ARGS__);                                              \
		fprintf(stderr, "\n");                                                     \
	}
//! Log - error
#define LOG_E(...)                                                             \
	{                                                                            \
		fprintf(stderr, "E: ");                                                    \
		fprintf(stderr, __VA_ARGS__);                                              \
		fprintf(stderr, "\n");                                                     \
	}
