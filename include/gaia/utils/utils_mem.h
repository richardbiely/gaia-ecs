#pragma once
#include <type_traits>

namespace gaia {
	namespace utils {
		/*
		Align the number
		\param num		number to align
		\param alignment	where to align
		\return			aligned number
		*/
		template <class T, class V>
		constexpr T align(T num, V alignment) {
			return alignment == 0 ? num
														: ((num + (alignment - 1)) / alignment) * alignment;
		}

		/*
		Align the number to 'alignment' bytes
		\param num		number to align
		\return			aligned number
		*/
		template <size_t alignment, class T>
		constexpr T align(T num) {
			return ((num + (alignment - 1)) & ~(alignment - 1));
		}

		/*!
		Fill memory with 32 bit integer value
		\param dest pointer to destination
		\param num number of items to be filled (not data size!)
		\param data 32bit data to be filled
		*/
		template <class T>
		void fill_array(T* dest, int num, const T& data) {
			for (int n = 0; n < num; n++)
				((T*)dest)[n] = data;
		}

		/*!
		Convert form type \tparam From to type \tparam To without causing an
		undefined behavior.

		E.g.:
		int i = {};
		float f = *(*float)&i; // undefined behavior
		memcpy(&f, &i, sizeof(float)); // okay
		 */
		template <
				typename To, typename From,
				typename = std::enable_if_t<
						(sizeof(To) == sizeof(From)) && std::is_trivially_copyable_v<To> &&
						std::is_trivially_copyable_v<From>>>
		To bit_cast(const From& from) {
			To to;
			std::memcpy(&to, &from, sizeof(To));
			return to;
		}
	} // namespace utils
} // namespace gaia
