#pragma once

#define USE_LIST 0

#if USE_LIST == 0

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
