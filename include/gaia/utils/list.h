#pragma once

#define USE_LIST 1 // GAIA_USE_STL_CONTAINERS
// TODO: Implement a custom list container

#if USE_LIST == 1

	#include <list>

namespace gaia {
	namespace utils {
		template <typename T>
		using list = std::list<T>;
	}
} // namespace gaia

#else

	#error Unsupported value used for USE_LIST

#endif
