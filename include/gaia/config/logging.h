#pragma once

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