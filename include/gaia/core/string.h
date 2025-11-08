#pragma once
#include "gaia/config/config.h"

#include "gaia/core/span.h"

namespace gaia {
	namespace core {
		inline bool is_whitespace(char c) {
			return c == ' ' || (c >= '\t' && c <= '\r');
		}

		inline auto trim(std::span<const char> expr) {
			if (expr.empty())
				return std::span<const char>{};

			uint32_t beg = 0;
			while (is_whitespace(expr[beg]))
				++beg;
			uint32_t end = (uint32_t)expr.size() - 1;
			while (end > beg && is_whitespace(expr[end]))
				--end;
			return expr.subspan(beg, end - beg + 1);
		}
	} // namespace core
} // namespace gaia