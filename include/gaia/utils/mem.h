#pragma once
#include <cstring>
#include <type_traits>

namespace gaia {
	namespace utils {
		/*!
		Align a number to the requested byte alignment
		\param num Number to align
		\param alignment Requested alignment
		\return Aligned number
		*/
		template <typename T, typename V>
		constexpr T align(T num, V alignment) {
			return alignment == 0 ? num : ((num + (alignment - 1)) / alignment) * alignment;
		}

		/*!
		Align a number to the requested byte alignment
		\tparam alignment Requested alignment in bytes
		\param num Number to align
		\return Aligned number
		*/
		template <size_t alignment, typename T>
		constexpr T align(T num) {
			return ((num + (alignment - 1)) & ~(alignment - 1));
		}

		/*!
		Fill memory with 32 bit integer value
		\param dest pointer to destination
		\param num number of items to be filled (not data size!)
		\param data 32bit data to be filled
		*/
		template <typename T>
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
						(sizeof(To) == sizeof(From)) && std::is_trivially_copyable_v<To> && std::is_trivially_copyable_v<From>>>
		To bit_cast(const From& from) {
			To to;
			memmove(&to, &from, sizeof(To));
			return to;
		}

		/*!
		Pointer wrapper for reading memory in defined way (not causing undefined
		behavior).
		*/
		template <typename T>
		class const_unaligned_pointer {
			const char* from;

		public:
			const_unaligned_pointer(): from(nullptr) {}
			const_unaligned_pointer(const void* p): from((const char*)p) {}

			T operator*() const {
				T to;
				memmove(&to, from, sizeof(T));
				return to;
			}

			T operator[](ptrdiff_t d) const {
				return *(*this + d);
			}

			const_unaligned_pointer operator+(ptrdiff_t d) const {
				return const_unaligned_pointer(from + d * sizeof(T));
			}
			const_unaligned_pointer operator-(ptrdiff_t d) const {
				return const_unaligned_pointer(from - d * sizeof(T));
			}
		};

		/*!
		Pointer wrapper for writing memory in defined way (not causing undefined
		behavior).
		*/
		template <typename T>
		class unaligned_ref {
			void* m_p;

		public:
			unaligned_ref(void* p): m_p(p) {}

			T operator=(const T& rvalue) {
				memmove(m_p, &rvalue, sizeof(T));
				return rvalue;
			}

			operator T() const {
				T tmp;
				memmove(&tmp, m_p, sizeof(T));
				return tmp;
			}
		};

		/*!
		Pointer wrapper for writing memory in defined way (not causing undefined
		behavior).
		*/
		template <typename T>
		class unaligned_pointer {
			char* m_p;

		public:
			unaligned_pointer(): m_p(nullptr) {}
			unaligned_pointer(void* p): m_p((char*)p) {}

			unaligned_ref<T> operator*() const {
				return unaligned_ref<T>(m_p);
			}

			unaligned_ref<T> operator[](ptrdiff_t d) const {
				return *(*this + d);
			}

			unaligned_pointer operator+(ptrdiff_t d) const {
				return unaligned_pointer(m_p + d * sizeof(T));
			}
			unaligned_pointer operator-(ptrdiff_t d) const {
				return unaligned_pointer(m_p - d * sizeof(T));
			}
		};
	} // namespace utils
} // namespace gaia
