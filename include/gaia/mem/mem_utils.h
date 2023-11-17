#pragma once
#include "../config/config.h"

#include <cstdint>
#include <type_traits>
#include <utility>

#include "../core/utility.h"
#include "data_layout_policy.h"

namespace gaia {
	namespace mem {
		namespace detail {
			template <typename T>
			void copy_elements_aos(T* GAIA_RESTRICT dst, const T* GAIA_RESTRICT src, uint32_t idxSrc, uint32_t idxDst) {
				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(6385)

				static_assert(std::is_copy_assignable_v<T>);
				static_assert(!mem::is_soa_layout_v<T>);

				GAIA_FOR2(idxSrc, idxDst) dst[i] = src[i];

				GAIA_MSVC_WARNING_POP()
			}

			template <typename T>
			void copy_elements_soa(
					uint8_t* GAIA_RESTRICT dst, const uint8_t* GAIA_RESTRICT src, uint32_t idxSrc, uint32_t idxDst,
					uint32_t sizeDst, uint32_t sizeSrc) {
				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(6385)

				GAIA_FOR2(idxSrc, idxDst) {
					(data_view_policy_set<T::Layout, T>({std::span<uint8_t>{dst, sizeDst}}))[i] =
							(data_view_policy_set<T::Layout, T>({std::span<const uint8_t>{(const uint8_t*)src, sizeSrc}}))[i];
				}

				GAIA_MSVC_WARNING_POP()
			}

			template <typename T>
			void move_elements_aos(T* GAIA_RESTRICT dst, const T* GAIA_RESTRICT src, uint32_t idxSrc, uint32_t idxDst) {
				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(6385)

				static_assert(std::is_move_assignable_v<T>);
				static_assert(!mem::is_soa_layout_v<T>);

				GAIA_FOR2(idxSrc, idxDst) dst[i] = GAIA_MOV(src[i]);

				GAIA_MSVC_WARNING_POP()
			}

			//! Shift elements at the address pointed to by \param dst to the left by one
			template <typename T>
			void shift_elements_left_aos(T* dst, uint32_t idxSrc, uint32_t idxDst, uint32_t n) {
				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(6385)

				static_assert(!mem::is_soa_layout_v<T>);

				if constexpr (std::is_move_assignable_v<T>)
					GAIA_FOR2(idxSrc, idxDst) dst[i] = GAIA_MOV(dst[i + n]);
				else
					GAIA_FOR2(idxSrc, idxDst) dst[i] = dst[i + n];

				GAIA_MSVC_WARNING_POP()
			}

			//! Shift elements at the address pointed to by \param dst to the left by one
			template <typename T>
			void shift_elements_left_aos_n(T* dst, uint32_t idxSrc, uint32_t idxDst, uint32_t n) {
				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(6385)

				static_assert(!mem::is_soa_layout_v<T>);

				const auto max = idxDst - idxSrc - n;
				if constexpr (std::is_move_assignable_v<T>)
					GAIA_FOR(max) dst[idxSrc + i] = GAIA_MOV(dst[idxSrc + i + n]);
				else
					GAIA_FOR(max) dst[idxSrc + i] = dst[idxSrc + i + n];

				GAIA_MSVC_WARNING_POP()
			}

			//! Shift elements at the address pointed to by \param dst to the left by one
			template <typename T>
			void shift_elements_left_soa(uint8_t* dst, uint32_t idxSrc, uint32_t idxDst, uint32_t n, uint32_t size) {
				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(6385)

				GAIA_FOR2(idxSrc, idxDst) {
					(data_view_policy_set<T::Layout, T>({std::span<uint8_t>{dst, size}}))[i] =
							(data_view_policy_get<T::Layout, T>({std::span<const uint8_t>{(const uint8_t*)dst, size}}))[i + n];
				}

				GAIA_MSVC_WARNING_POP()
			}
		} // namespace detail

		//! Copy \param size elements of type \tparam T from the address pointer to by \param src to \param dst
		template <typename T>
		void copy_elements(
				uint8_t* GAIA_RESTRICT dst, const uint8_t* GAIA_RESTRICT src, uint32_t idxSrc, uint32_t idxDst,
				[[maybe_unused]] uint32_t sizeDst, [[maybe_unused]] uint32_t sizeSrc) {
			GAIA_ASSERT(idxSrc <= idxDst);
			if (idxSrc == idxDst)
				return;

			if constexpr (mem::is_soa_layout_v<T>)
				detail::copy_elements_soa<T>(dst, src, sizeDst, sizeSrc, idxSrc, idxDst);
			else
				detail::copy_elements_aos<T>((T*)dst, (const T*)src, idxSrc, idxDst);
		}

		//! Move or copy \param cnt elements of type \tparam T from the address pointer to by \param src to \param dst
		template <typename T>
		void move_elements(
				uint8_t* GAIA_RESTRICT dst, const uint8_t* GAIA_RESTRICT src, uint32_t idxSrc, uint32_t idxDst,
				[[maybe_unused]] uint32_t sizeDst, [[maybe_unused]] uint32_t sizeSrc) {
			GAIA_ASSERT(idxSrc <= idxDst);
			if (idxSrc == idxDst)
				return;

			if constexpr (std::is_move_assignable_v<T> && !mem::is_soa_layout_v<T>)
				detail::move_elements_aos<T>((T*)dst, (const T*)src, idxSrc, idxDst);
			else
				copy_elements<T>(dst, src, idxSrc, idxDst, sizeDst, sizeSrc);
		}

		//! Shift elements at the address pointed to by \param dst to the left by one
		template <typename T>
		void shift_elements_left(uint8_t* dst, uint32_t idxSrc, uint32_t idxDst, [[maybe_unused]] uint32_t size) {
			GAIA_ASSERT(idxSrc <= idxDst);
			if (idxSrc == idxDst)
				return;

			if constexpr (mem::is_soa_layout_v<T>)
				detail::shift_elements_left_soa<T>(*dst, idxSrc, idxDst, 1, size);
			else
				detail::shift_elements_left_aos<T>((T*)dst, idxSrc, idxDst, 1);
		}

		//! Shift elements at the address pointed to by \param dst to the left by one
		template <typename T>
		void
		shift_elements_left_n(uint8_t* dst, uint32_t idxSrc, uint32_t idxDst, uint32_t n, [[maybe_unused]] uint32_t size) {
			GAIA_ASSERT(idxSrc <= idxDst);
			if (idxSrc == idxDst)
				return;

			if constexpr (mem::is_soa_layout_v<T>)
				detail::shift_elements_left_soa<T>(*dst, idxSrc, idxDst, n, size);
			else
				detail::shift_elements_left_aos_n<T>((T*)dst, idxSrc, idxDst, n);
		}
	} // namespace mem
} // namespace gaia
