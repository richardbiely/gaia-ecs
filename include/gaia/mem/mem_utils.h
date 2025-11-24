#pragma once
#include "gaia/config/config.h"

#include <cstdint>
#include <type_traits>

#include "gaia/core/utility.h"
#include "gaia/mem/data_layout_policy.h"

namespace gaia {
	namespace mem {
		template <typename T>
		constexpr bool is_copyable() {
			return std::is_trivially_copyable_v<T> || std::is_trivially_assignable_v<T, T> || //
						 std::is_copy_assignable_v<T> || std::is_copy_constructible_v<T>;
		}

		template <typename T>
		constexpr bool is_movable() {
			return std::is_trivially_move_assignable_v<T> || std::is_trivially_move_constructible_v<T> || //
						 std::is_move_assignable_v<T> || std::is_move_constructible_v<T>;
		}

		namespace detail {
			template <typename T>
			void copy_ctor_element_aos(T* GAIA_RESTRICT dst, const T* GAIA_RESTRICT src, uint32_t idxDst, uint32_t idxSrc) {
				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(6385)

				if constexpr (std::is_trivially_copyable_v<T>) {
					memcpy(dst + idxDst, src + idxSrc, sizeof(T));
				} else if constexpr (std::is_copy_assignable_v<T>) {
					// Prefer assignment path — it can be faster for types with data reuse
					core::call_ctor(&dst[idxDst]);
					dst[idxDst] = src[idxSrc];
				} else {
					static_assert(std::is_copy_constructible_v<T>);
					core::call_ctor(&dst[idxDst], T(src[idxSrc]));
				}

				GAIA_MSVC_WARNING_POP()
			}

			template <typename T>
			void copy_element_aos(T* GAIA_RESTRICT dst, const T* GAIA_RESTRICT src, uint32_t idxDst, uint32_t idxSrc) {
				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(6385)

				if constexpr (std::is_trivially_copyable_v<T>) {
					memcpy(dst + idxDst, src + idxSrc, sizeof(T));
				} else if constexpr (std::is_copy_assignable_v<T>) {
					dst[idxDst] = src[idxSrc];
				} else {
					static_assert(std::is_copy_constructible_v<T>);
					dst[idxDst] = T(src[idxSrc]);
				}

				GAIA_MSVC_WARNING_POP()
			}

			template <typename T>
			void copy_elements_aos(T* GAIA_RESTRICT dst, const T* GAIA_RESTRICT src, uint32_t idxDst, uint32_t idxSrc) {
				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(6385)

				GAIA_ASSERT(idxSrc < idxDst);

				const auto cnt = idxDst - idxSrc;

				if constexpr (std::is_trivially_copyable_v<T>) {
					memcpy(dst + idxSrc, src + idxSrc, sizeof(T) * cnt);
				} else if constexpr (std::is_copy_assignable_v<T>) {
					const T* s = src + idxSrc;
					T* d = dst + idxSrc;
					GAIA_FOR(cnt) d[i] = s[i];
				} else {
					static_assert(std::is_copy_constructible_v<T>);
					const T* s = src + idxSrc;
					T* d = dst + idxSrc;
					GAIA_FOR(cnt) d[i] = T(s[i]);
				}

				GAIA_MSVC_WARNING_POP()
			}

			template <typename T>
			void copy_element_soa(
					uint8_t* GAIA_RESTRICT dst, const uint8_t* GAIA_RESTRICT src, uint32_t idxDst, uint32_t idxSrc,
					uint32_t sizeDst, uint32_t sizeSrc) {
				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(6385)

				static_assert(mem::is_soa_layout_v<T>);

				(data_view_policy_soa_set<T::gaia_Data_Layout, T>({std::span{dst, sizeDst}}))[idxDst] =
						(data_view_policy_soa_get<T::gaia_Data_Layout, T>({std::span{(const uint8_t*)src, sizeSrc}}))[idxSrc];

				GAIA_MSVC_WARNING_POP()
			}

			template <typename T>
			void copy_elements_soa(
					uint8_t* GAIA_RESTRICT dst, const uint8_t* GAIA_RESTRICT src, uint32_t idxDst, uint32_t idxSrc,
					uint32_t sizeDst, uint32_t sizeSrc) {
				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(6385)

				static_assert(mem::is_soa_layout_v<T>);

				GAIA_ASSERT(idxSrc < idxDst);

				GAIA_FOR2(idxSrc, idxDst) {
					(data_view_policy_soa_set<T::gaia_Data_Layout, T>({std::span{dst, sizeDst}}))[i] =
							(data_view_policy_soa_get<T::gaia_Data_Layout, T>({std::span{(const uint8_t*)src, sizeSrc}}))[i];
				}

				GAIA_MSVC_WARNING_POP()
			}

			template <typename T>
			void move_ctor_element_aos(T* GAIA_RESTRICT dst, T* GAIA_RESTRICT src, uint32_t idxDst, uint32_t idxSrc) {
				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(6385)

				if constexpr (std::is_trivially_move_constructible_v<T> && std::is_trivially_destructible_v<T>) {
					memcpy(dst + idxDst, src + idxSrc, sizeof(T));
				} else if constexpr (std::is_move_assignable_v<T>) {
					core::call_ctor(&dst[idxDst]);
					dst[idxDst] = GAIA_MOV(src[idxSrc]);
				} else {
					static_assert(std::is_move_constructible_v<T>);
					core::call_ctor(&dst[idxDst], T(GAIA_MOV(src[idxSrc])));
				}

				GAIA_MSVC_WARNING_POP()
			}

			template <typename T>
			void move_element_aos(T* GAIA_RESTRICT dst, T* GAIA_RESTRICT src, uint32_t idxDst, uint32_t idxSrc) {
				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(6385)

				if constexpr (std::is_trivially_move_assignable_v<T>) {
					memcpy(dst + idxDst, src + idxSrc, sizeof(T));
				} else if constexpr (std::is_move_assignable_v<T>) {
					dst[idxDst] = GAIA_MOV(src[idxSrc]);
				} else {
					static_assert(std::is_move_constructible_v<T>);
					dst[idxDst] = T(GAIA_MOV(src[idxSrc]));
				}

				GAIA_MSVC_WARNING_POP()
			}

			template <typename T>
			void move_elements_aos(T* GAIA_RESTRICT dst, T* GAIA_RESTRICT src, uint32_t idxDst, uint32_t idxSrc) {
				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(6385)

				GAIA_ASSERT(idxSrc < idxDst);

				if constexpr (std::is_trivially_move_assignable_v<T>) {
					memcpy((void*)&dst[idxSrc], (const void*)&src[idxSrc], sizeof(T) * (idxDst - idxSrc));
				} else if constexpr (std::is_move_assignable_v<T>) {
					GAIA_FOR2(idxSrc, idxDst) dst[i] = GAIA_MOV(src[i]);
				} else {
					static_assert(std::is_move_constructible_v<T>);
					GAIA_FOR2(idxSrc, idxDst) dst[i] = T(GAIA_MOV(src[i]));
				}

				GAIA_MSVC_WARNING_POP()
			}

			//! Shift elements at the address pointed to by \param dst to the left by \param n elements.
			template <typename T>
			void shift_elements_left_aos(T* dst, uint32_t idxDst, uint32_t idxSrc, uint32_t n) {
				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(6385)

				GAIA_ASSERT(idxSrc < idxDst);

				if constexpr (std::is_trivially_copy_assignable_v<T> || std::is_trivially_move_assignable_v<T>) {
					memmove((void*)&dst[idxSrc], (const void*)&dst[idxSrc + n], sizeof(T) * (idxDst - idxSrc));
				}
				// Move first if possible
				else if constexpr (std::is_move_assignable_v<T>) {
					GAIA_FOR2(idxSrc, idxDst) dst[i] = GAIA_MOV(dst[i + n]);
				} else if constexpr (std::is_move_constructible_v<T>) {
					GAIA_FOR2(idxSrc, idxDst) dst[i] = T(GAIA_MOV(dst[i + n]));
				}
				// Try to copy if moves are not possible
				else if constexpr (std::is_copy_assignable_v<T>) {
					GAIA_FOR2(idxSrc, idxDst) dst[i] = dst[i + n];
				} else if constexpr (std::is_copy_constructible_v<T>) {
					GAIA_FOR2(idxSrc, idxDst) dst[i] = T(dst[i + n]);
				} else {
					GAIA_ASSERT(false && "Not implemented");
				}

				GAIA_MSVC_WARNING_POP()
			}

			//! Shift elements at the address pointed to by \param dst to the left by \param n elements.
			//! Handles only the non-overlapping part.
			template <typename T>
			void shift_elements_left_aos_fast(T* dst, uint32_t idxDst, uint32_t idxSrc, uint32_t n) {
				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(6385)

				GAIA_ASSERT(idxSrc < idxDst);

				const auto max = idxDst - idxSrc - n;

				if constexpr (std::is_trivially_copy_assignable_v<T> || std::is_trivially_move_assignable_v<T>) {
					memcpy((void*)&dst[idxSrc], (const void*)&dst[idxSrc + n], sizeof(T) * max);
				}
				// Move first if possible
				else if constexpr (std::is_move_assignable_v<T>) {
					GAIA_FOR(max) dst[idxSrc + i] = GAIA_MOV(dst[idxSrc + i + n]);
				} else if constexpr (std::is_move_constructible_v<T>) {
					GAIA_FOR(max) dst[idxSrc + i] = T(GAIA_MOV(dst[idxSrc + i + n]));
				}
				// Try to copy if moves are not possible
				else if constexpr (std::is_copy_assignable_v<T>) {
					GAIA_FOR(max) dst[idxSrc + i] = dst[idxSrc + i + n];
				} else if constexpr (std::is_copy_constructible_v<T>) {
					GAIA_FOR(max) dst[idxSrc + i] = T(dst[idxSrc + i + n]);
				} else {
					GAIA_ASSERT(false && "Not implemented");
				}

				GAIA_MSVC_WARNING_POP()
			}

			//! Shift elements at the address pointed to by \param dst to the left by \param n elements.
			template <typename T>
			void shift_elements_left_soa(uint8_t* dst, uint32_t idxDst, uint32_t idxSrc, uint32_t n, uint32_t size) {
				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(6385)

				static_assert(mem::is_soa_layout_v<T>);

				GAIA_ASSERT(idxSrc < idxDst);

				GAIA_FOR2(idxSrc, idxDst) {
					(data_view_policy_soa_set<T::gaia_Data_Layout, T>({std::span<uint8_t>{dst, size}}))[i] =
							(data_view_policy_soa_get<T::gaia_Data_Layout, T>(
									{std::span<const uint8_t>{(const uint8_t*)dst, size}}))[i + n];
				}

				GAIA_MSVC_WARNING_POP()
			}

			//! Shift elements at the address pointed to by \param dst to the right by \param n elements.
			template <typename T>
			void shift_elements_right_aos(T* dst, uint32_t idxDst, uint32_t idxSrc, uint32_t n) {
				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(6385)

				GAIA_ASSERT(idxSrc < idxDst);

				const auto max = idxDst - idxSrc;
				const auto idx = idxDst - 1;

				if constexpr (std::is_trivially_copy_assignable_v<T> || std::is_trivially_move_assignable_v<T>) {
					memmove(dst + idxSrc + n, dst + idxSrc, sizeof(T) * max);
				}
				// Move first if possible
				else if constexpr (std::is_move_assignable_v<T>) {
					GAIA_FOR(max) dst[idx - i + n] = GAIA_MOV(dst[idx - i]);
				} else if constexpr (std::is_move_constructible_v<T>) {
					GAIA_FOR(max) dst[idx - i + n] = T(GAIA_MOV(dst[idx - i]));
				}
				// Try to copy if moves are not possible
				else if constexpr (std::is_copy_assignable_v<T>) {
					GAIA_FOR(max) dst[idx - i + n] = dst[idx - i];
				} else if constexpr (std::is_copy_constructible_v<T>) {
					GAIA_FOR(max) dst[idx - i + n] = T(dst[idx - i]);
				} else {
					GAIA_ASSERT(false && "Not implemented");
				}

				GAIA_MSVC_WARNING_POP()
			}

			//! Shift elements at the address pointed to by \param dst to the right by \param n elements.
			//! Handles only the non-overlapping part.
			template <typename T>
			void shift_elements_right_aos_fast(T* dst, uint32_t idxDst, uint32_t idxSrc, uint32_t n) {
				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(6385)

				GAIA_ASSERT(idxSrc + n < idxDst);

				const auto max = idxDst - idxSrc - n;

				if constexpr (std::is_trivially_copy_assignable_v<T> || std::is_trivially_move_assignable_v<T>) {
					memcpy(dst + idxSrc + n, dst + idxSrc, sizeof(T) * max);
				}
				// Move/copy from the end to avoid overwriting data
				else if constexpr (std::is_move_assignable_v<T>) {
					GAIA_FOR(max) dst[idxSrc + i + n] = GAIA_MOV(dst[idxSrc + i]);
				} else if constexpr (std::is_move_constructible_v<T>) {
					GAIA_FOR(max) dst[idxSrc + i + n] = T(GAIA_MOV(dst[idxSrc + i]));
				}
				// Try to copy if moves are not possible
				else if constexpr (std::is_copy_assignable_v<T>) {
					GAIA_FOR(max) dst[idxSrc + i + n] = dst[idxSrc + i];
				} else if constexpr (std::is_copy_constructible_v<T>) {
					GAIA_FOR(max) dst[idxSrc + i + n] = T(dst[idxSrc + i]);
				} else {
					GAIA_ASSERT(false && "Not implemented");
				}

				GAIA_MSVC_WARNING_POP()
			}

			//! Shift elements at the address pointed to by \param dst to the right by one
			template <typename T>
			void shift_elements_right_soa(uint8_t* dst, uint32_t idxDst, uint32_t idxSrc, uint32_t n, uint32_t size) {
				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(6385)

				static_assert(mem::is_soa_layout_v<T>);

				GAIA_ASSERT(idxSrc < idxDst);

				GAIA_FOR2(idxSrc, idxDst) {
					(data_view_policy_soa_set<T::gaia_Data_Layout, T>({std::span<uint8_t>{dst, size}}))[i + n] =
							(data_view_policy_soa_get<T::gaia_Data_Layout, T>(
									{std::span<const uint8_t>{(const uint8_t*)dst, size}}))[i];
				}

				GAIA_MSVC_WARNING_POP()
			}
		} // namespace detail

		GAIA_CLANG_WARNING_PUSH()
		// Memory is aligned so we can silence this warning
		GAIA_CLANG_WARNING_DISABLE("-Wcast-align")

		//! Copy \param size elements of type \tparam T from the address pointed to by \param src to \param dst
		template <typename T, bool SOA = mem::is_soa_layout_v<T>>
		void copy_ctor_element(
				uint8_t* GAIA_RESTRICT dst, const uint8_t* GAIA_RESTRICT src, uint32_t idxDst, uint32_t idxSrc,
				[[maybe_unused]] uint32_t sizeDst, [[maybe_unused]] uint32_t sizeSrc) {
			if GAIA_UNLIKELY (src == dst && idxSrc == idxDst)
				return;

			if constexpr (!SOA)
				detail::copy_ctor_element_aos<T>((T*)dst, (const T*)src, idxDst, idxSrc);
			else
				detail::copy_element_soa<T>(dst, src, idxDst, idxSrc, sizeDst, sizeSrc);
		}

		//! Copy one element of type \tparam T from the address pointed to by \param src to \param dst
		//! at relative offsets \param idxSrc and \param idxDst.
		template <typename T, bool SOA = mem::is_soa_layout_v<T>>
		void copy_element(
				uint8_t* GAIA_RESTRICT dst, const uint8_t* GAIA_RESTRICT src, uint32_t idxDst, uint32_t idxSrc,
				[[maybe_unused]] uint32_t sizeDst, [[maybe_unused]] uint32_t sizeSrc) {
			if GAIA_UNLIKELY (src == dst && idxSrc == idxDst)
				return;

			if constexpr (!SOA)
				detail::copy_element_aos<T>((T*)dst, (const T*)src, idxDst, idxSrc);
			else
				detail::copy_element_soa<T>(dst, src, idxDst, idxSrc, sizeDst, sizeSrc);
		}

		//! Copy elements of type \tparam T from the address pointed to by \param src to \param dst
		//! at relative offsets \param idxSrc and \param idxDst. The number of moved elements is idxDst-idxSrc.
		template <typename T, bool SOA = mem::is_soa_layout_v<T>>
		void copy_elements(
				uint8_t* GAIA_RESTRICT dst, const uint8_t* GAIA_RESTRICT src, uint32_t idxDst, uint32_t idxSrc,
				[[maybe_unused]] uint32_t sizeDst, [[maybe_unused]] uint32_t sizeSrc) {
			GAIA_ASSERT(idxSrc <= idxDst);
			if GAIA_UNLIKELY (idxSrc == idxDst)
				return;

			if constexpr (!SOA)
				detail::copy_elements_aos<T>((T*)dst, (const T*)src, idxDst, idxSrc);
			else
				detail::copy_elements_soa<T>(dst, src, idxDst, idxSrc, sizeDst, sizeSrc);
		}

		//! Move or copy \param cnt elements of type \tparam T from the address pointed to by \param src to \param dst
		template <typename T, bool SOA = mem::is_soa_layout_v<T>>
		void move_ctor_element(
				uint8_t* GAIA_RESTRICT dst, uint8_t* GAIA_RESTRICT src, uint32_t idxDst, uint32_t idxSrc,
				[[maybe_unused]] uint32_t sizeDst, [[maybe_unused]] uint32_t sizeSrc) {
			if GAIA_UNLIKELY (src == dst && idxSrc == idxDst)
				return;

			if constexpr (!SOA) {
				if constexpr (is_movable<T>())
					detail::move_ctor_element_aos<T>((T*)dst, (T*)src, idxDst, idxSrc);
				else
					detail::copy_ctor_element_aos<T>((T*)dst, (const T*)src, idxDst, idxSrc);
			} else
				detail::copy_element_soa<T>(dst, src, idxDst, idxSrc, sizeDst, sizeSrc);
		}

		//! Move or copy one elements of type \tparam T from the address pointed to by \param src to \param dst
		//! at relative offsets \param idxSrc and \param idxDst.
		template <typename T, bool SOA = mem::is_soa_layout_v<T>>
		void move_element(
				uint8_t* GAIA_RESTRICT dst, uint8_t* GAIA_RESTRICT src, uint32_t idxDst, uint32_t idxSrc,
				[[maybe_unused]] uint32_t sizeDst, [[maybe_unused]] uint32_t sizeSrc) {
			if GAIA_UNLIKELY (src == dst && idxSrc == idxDst)
				return;

			if constexpr (!SOA) {
				if constexpr (is_movable<T>())
					detail::move_element_aos<T>((T*)dst, (T*)src, idxDst, idxSrc);
				else
					detail::copy_element_aos<T>((T*)dst, (const T*)src, idxDst, idxSrc);
			} else
				detail::copy_element_soa<T>(dst, src, idxDst, idxSrc, sizeDst, sizeSrc);
		}

		//! Move or copy elements of type \tparam T from the address pointed to by \param src to \param dst
		//! at relative offsets \param idxSrc and \param idxDst. The number of moved elements is idxDst-idxSrc.
		template <typename T, bool SOA = mem::is_soa_layout_v<T>>
		void move_elements(
				uint8_t* GAIA_RESTRICT dst, uint8_t* GAIA_RESTRICT src, uint32_t idxDst, uint32_t idxSrc,
				[[maybe_unused]] uint32_t sizeDst, [[maybe_unused]] uint32_t sizeSrc) {
			GAIA_ASSERT(idxSrc <= idxDst);
			if GAIA_UNLIKELY (idxSrc == idxDst)
				return;

			if constexpr (!SOA) {
				if constexpr (is_movable<T>())
					detail::move_elements_aos<T>((T*)dst, (T*)src, idxDst, idxSrc);
				else
					detail::copy_elements_aos<T>((T*)dst, (const T*)src, idxDst, idxSrc);
			} else
				detail::copy_elements_soa<T>(dst, src, idxDst, idxSrc, sizeDst, sizeSrc);
		}

		//! Move or copy \param cnt elements of type \tparam T from the address pointed to by \param src to \param dst
		template <typename T, bool SOA = mem::is_soa_layout_v<T>>
		void swap_elements(
				uint8_t* GAIA_RESTRICT dst, uint8_t* GAIA_RESTRICT src, uint32_t idxDst, uint32_t idxSrc,
				[[maybe_unused]] uint32_t sizeDst, [[maybe_unused]] uint32_t sizeSrc) {
			if GAIA_UNLIKELY (src == dst && idxSrc == idxDst)
				return;

			if constexpr (!SOA) {
				if constexpr (is_movable<T>()) {
					auto* l = (T*)src;
					auto* r = (T*)dst;
					T tmp;
					detail::move_element_aos<T>(&tmp, l, 0, idxSrc);
					detail::move_element_aos<T>(l, r, idxSrc, idxDst);
					detail::move_element_aos<T>(r, &tmp, idxDst, 0);
				} else {
					auto* l = (T*)src;
					auto* r = (T*)dst;
					T tmp;
					detail::copy_element_aos<T>(&tmp, l, 0, idxSrc);
					detail::copy_element_aos<T>(l, r, idxSrc, idxDst);
					detail::copy_element_aos<T>(r, &tmp, idxDst, 0);
				}
			} else {
				T tmp = mem::data_view_policy_soa_get<T::gaia_Data_Layout, T>{std::span{(const uint8_t*)src, sizeSrc}}[idxSrc];
				detail::copy_element_soa<T>(src, dst, idxSrc, idxDst, sizeSrc, sizeDst);
				mem::data_view_policy_soa_set<T::gaia_Data_Layout, T>{std::span{(const uint8_t*)dst, sizeDst}}[idxDst] = tmp;
			}
		}

		//! Shift elements at the address pointed to by \param dst to the left by one element
		template <typename T, bool SOA = mem::is_soa_layout_v<T>>
		void shift_elements_left(uint8_t* dst, uint32_t idxDst, uint32_t idxSrc, [[maybe_unused]] uint32_t size) {
			GAIA_ASSERT(idxSrc <= idxDst);
			if GAIA_UNLIKELY (idxSrc == idxDst)
				return;

			if constexpr (SOA)
				detail::shift_elements_left_soa<T>(*dst, idxDst, idxSrc, 1, size);
			else
				detail::shift_elements_left_aos<T>((T*)dst, idxDst, idxSrc, 1);
		}

		//! Shift elements at the address pointed to by \param dst to the left by one \param n elements
		template <typename T, bool SOA = mem::is_soa_layout_v<T>>
		void shift_elements_left_fast(
				uint8_t* dst, uint32_t idxDst, uint32_t idxSrc, uint32_t n, [[maybe_unused]] uint32_t size) {
			GAIA_ASSERT(idxSrc <= idxDst);
			if GAIA_UNLIKELY (idxSrc == idxDst)
				return;

			if constexpr (SOA)
				detail::shift_elements_left_soa<T>(*dst, idxDst, idxSrc, n, size);
			else
				detail::shift_elements_left_aos_fast<T>((T*)dst, idxDst, idxSrc, n);
		}

		//! Shift elements at the address pointed to by \param dst to the right by one element
		template <typename T, bool SOA = mem::is_soa_layout_v<T>>
		void shift_elements_right(uint8_t* dst, uint32_t idxDst, uint32_t idxSrc, [[maybe_unused]] uint32_t size) {
			GAIA_ASSERT(idxSrc <= idxDst);
			if GAIA_UNLIKELY (idxSrc == idxDst)
				return;

			if constexpr (SOA)
				detail::shift_elements_right_soa<T>(*dst, idxDst, idxSrc, 1, size);
			else
				detail::shift_elements_right_aos<T>((T*)dst, idxDst, idxSrc, 1);
		}

		//! Shift elements at the address pointed to by \param dst to the right by \param n elements
		template <typename T, bool SOA = mem::is_soa_layout_v<T>>
		void shift_elements_right_fast(
				uint8_t* dst, uint32_t idxDst, uint32_t idxSrc, uint32_t n, [[maybe_unused]] uint32_t size) {
			GAIA_ASSERT(idxSrc <= idxDst);
			if GAIA_UNLIKELY (idxSrc == idxDst)
				return;

			if constexpr (SOA)
				detail::shift_elements_right_soa<T>(*dst, idxDst, idxSrc, n, size);
			else
				detail::shift_elements_right_aos_fast<T>((T*)dst, idxDst, idxSrc, n);
		}

		GAIA_CLANG_WARNING_POP()
	} // namespace mem
} // namespace gaia
