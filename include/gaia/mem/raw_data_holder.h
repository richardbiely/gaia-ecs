#pragma once
#include "gaia/config/config.h"

#include <cinttypes>

#include "gaia/mem/data_layout_policy.h"

namespace gaia {
	namespace mem {
		namespace detail {
			template <uint32_t Size, uint32_t Alignment>
			struct raw_data_holder {
				static_assert(Size > 0);

				GAIA_ALIGNAS(Alignment) uint8_t data[Size];

				constexpr operator uint8_t*() noexcept {
					return &data[0];
				}

				constexpr operator const uint8_t*() const noexcept {
					return &data[0];
				}
			};

			template <uint32_t Size>
			struct raw_data_holder<Size, 0> {
				static_assert(Size > 0);

				uint8_t data[Size];

				constexpr operator uint8_t*() noexcept {
					return &data[0];
				}

				constexpr operator const uint8_t*() const noexcept {
					return &data[0];
				}
			};
		} // namespace detail

		template <typename T, uint32_t N>
		using raw_data_holder = detail::raw_data_holder<N, auto_view_policy<T>::Alignment>;
	} // namespace mem
} // namespace gaia