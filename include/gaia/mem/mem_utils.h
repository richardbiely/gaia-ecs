#pragma once
#include "../config/config.h"

#include <cinttypes>
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

				for (uint32_t i = idxSrc; i < idxDst; ++i)
					dst[i] = src[i];

				GAIA_MSVC_WARNING_POP()
			}

			template <typename T>
			void copy_elements_soa(
					uint8_t* GAIA_RESTRICT dst, const uint8_t* GAIA_RESTRICT src, uint32_t idxSrc, uint32_t idxDst,
					uint32_t sizeDst, uint32_t sizeSrc) {
				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(6385)

				for (uint32_t i = idxSrc; i < idxDst; ++i)
					(data_view_policy_set<T::Layout, T>({std::span<uint8_t>{dst, sizeDst}}))[i] =
							(data_view_policy_set<T::Layout, T>({std::span<const uint8_t>{(const uint8_t*)src, sizeSrc}}))[i];

				GAIA_MSVC_WARNING_POP()
			}

			template <typename T>
			void move_elements_aos(T* GAIA_RESTRICT dst, const T* GAIA_RESTRICT src, uint32_t idxSrc, uint32_t idxDst) {
				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(6385)

				static_assert(std::is_move_assignable_v<T>);
				static_assert(!mem::is_soa_layout_v<T>);

				for (uint32_t i = idxSrc; i < idxDst; ++i)
					dst[i] = std::move(src[i]);

				GAIA_MSVC_WARNING_POP()
			}

			//! Shift elements at the address pointed to by \param dst to the left by one
			template <typename T>
			void shift_elements_left_aos(T* GAIA_RESTRICT dst, uint32_t idxSrc, uint32_t idxDst) {
				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(6385)

				static_assert(!mem::is_soa_layout_v<T>);

				if constexpr (std::is_move_assignable_v<T>) {
					for (uint32_t i = idxSrc; i < idxDst; ++i)
						dst[i] = std::move(dst[i + 1]);
				} else {
					for (uint32_t i = idxSrc; i < idxDst; ++i)
						dst[i] = dst[i + 1];
				}

				GAIA_MSVC_WARNING_POP()
			}
			//! Shift elements at the address pointed to by \param dst to the left by one
			template <typename T>
			void shift_elements_left_soa(uint8_t* GAIA_RESTRICT dst, uint32_t size, uint32_t idxSrc, uint32_t idxDst) {
				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(6385)

				for (uint32_t i = idxSrc; i < idxDst; ++i)
					(data_view_policy_set<T::Layout, T>({std::span<uint8_t>{dst, size}}))[i] =
							(data_view_policy_get<T::Layout, T>({std::span<const uint8_t>{(const uint8_t*)dst, size}}))[i + 1];

				GAIA_MSVC_WARNING_POP()
			}
		} // namespace detail

		template <typename T>
		void construct_elements(T* pData, size_t cnt) {
			if constexpr (!mem::is_soa_layout_v<T>)
				core::call_ctor(pData, cnt);
		}

		template <typename T>
		void destruct_elements(T* pData, size_t cnt) {
			if constexpr (!mem::is_soa_layout_v<T>)
				core::call_ctor(pData, cnt);
		}

		//! Copy \param size elements of type \tparam T from the address pointer to by \param src to \param dst
		template <typename T>
		void copy_elements(
				uint8_t* GAIA_RESTRICT dst, const uint8_t* GAIA_RESTRICT src, uint32_t idxSrc, uint32_t idxDst,
				[[maybe_unused]] uint32_t sizeDst, [[maybe_unused]] uint32_t sizeSrc) {
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
			if constexpr (std::is_move_assignable_v<T> && !mem::is_soa_layout_v<T>)
				detail::move_elements_aos<T>((T*)dst, (const T*)src, idxSrc, idxDst);
			else
				copy_elements<T>(dst, src, idxSrc, idxDst, sizeDst, sizeSrc);
		}

		//! Shift elements at the address pointed to by \param dst to the left by one
		template <typename T>
		void
		shift_elements_left(uint8_t* GAIA_RESTRICT dst, uint32_t idxSrc, uint32_t idxDst, [[maybe_unused]] uint32_t size) {
			if constexpr (mem::is_soa_layout_v<T>)
				detail::shift_elements_left_soa<T>(*dst, idxSrc, idxDst, size);
			else
				detail::shift_elements_left_aos<T>((T*)dst, idxSrc, idxDst);
		}
	} // namespace mem
} // namespace gaia
