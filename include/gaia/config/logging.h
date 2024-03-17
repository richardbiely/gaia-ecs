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