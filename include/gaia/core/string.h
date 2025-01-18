#pragma once
#include "../config/config.h"

#include "span.h"

namespace gaia {
	namespace core {
		inline auto trim(std::span<const char> expr) {
			uint32_t beg = 0;
			while (expr[beg] == ' ')
				++beg;
			uint32_t end = (uint32_t)expr.size() - 1;
			while (end > beg && expr[end] == ' ')
				--end;
			return expr.subspan(beg, end - beg + 1);
		}
	} // namespace core
} // namespace gaia