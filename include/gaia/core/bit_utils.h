#pragma once
#include "../config/config.h"

#include "span.h"
#include "utility.h"

namespace gaia {
	namespace core {
		template <uint32_t BlockBits>
		struct bit_view {
			static constexpr uint32_t MaxValue = (1 << BlockBits) - 1;

			std::span<uint8_t> m_data;

			void set(uint32_t bitPosition, uint8_t value) noexcept {
				GAIA_ASSERT(bitPosition < (m_data.size() * 8));
				GAIA_ASSERT(value <= MaxValue);

				const uint32_t idxByte = bitPosition / 8;
				const uint32_t idxBit = bitPosition % 8;

				const uint32_t mask = ~(MaxValue << idxBit);
				m_data[idxByte] = (uint8_t)(((uint32_t)m_data[idxByte] & mask) | ((uint32_t)value << idxBit));

				const bool overlaps = idxBit + BlockBits > 8;
				if (overlaps) {
					// Value spans over two bytes
					const uint32_t shift2 = 8U - idxBit;
					const uint32_t mask2 = ~(MaxValue >> shift2);
					m_data[idxByte + 1] = (uint8_t)(((uint32_t)m_data[idxByte + 1] & mask2) | ((uint32_t)value >> shift2));
				}
			}

			uint8_t get(uint32_t bitPosition) const noexcept {
				GAIA_ASSERT(bitPosition < (m_data.size() * 8));

				const uint32_t idxByte = bitPosition / 8;
				const uint32_t idxBit = bitPosition % 8;

				const uint8_t byte1 = (m_data[idxByte] >> idxBit) & MaxValue;

				const bool overlaps = idxBit + BlockBits > 8;
				if (overlaps) {
					// Value spans over two bytes
					const uint32_t shift2 = uint8_t(8U - idxBit);
					const uint32_t mask2 = MaxValue >> shift2;
					const uint8_t byte2 = uint8_t(((uint32_t)m_data[idxByte + 1] & mask2) << shift2);
					return byte1 | byte2;
				}

				return byte1;
			}
		};
	} // namespace core
} // namespace gaia