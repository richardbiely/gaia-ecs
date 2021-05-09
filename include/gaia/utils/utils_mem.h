#pragma once

namespace gaia {
	namespace utils {
		/*
		Align the number
		\param num		number to align
		\param alignment	where to align
		\return			aligned number
		*/
		template <class T, class V> constexpr T align(T num, V alignment) {
			return alignment == 0 ? num
														: ((num + (alignment - 1)) / alignment) * alignment;
		}

		/*
		Align the number to 'alignment' bytes
		\param num		number to align
		\return			aligned number
		*/
		template <size_t alignment, class T> constexpr T align(T num) {
			return ((num + (alignment - 1)) & ~(alignment - 1));
		}
	} // namespace utils
} // namespace gaia
