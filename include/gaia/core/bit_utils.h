#pragma once
#include "gaia/config/config.h"

#include <cstdint>

#include "gaia/core/span.h"

namespace gaia {
	namespace core {
		//! Provides packed access to fixed-width unsigned values stored in a byte span.
		//! \tparam BlockBits Number of bits occupied by each value.
		template <uint32_t BlockBits>
		struct bit_view {
			//! Largest value representable by one packed block.
			static constexpr uint32_t MaxValue = (1 << BlockBits) - 1;

			//! Bytes containing the packed values.
			std::span<uint8_t> m_data;

			//! Stores a packed value at the specified bit position.
			//! \param bitPosition Bit offset of the value within m_data.
			//! \param value Value to store. It must not exceed MaxValue.
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

			//! Reads a packed value from the specified bit position.
			//! \param bitPosition Bit offset of the value within m_data.
			//! \return The decoded value.
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

		//! Swaps two individual bits in an integer-like mask.
		//! \tparam T Mask type supporting shifts and bitwise operators.
		//! \param mask Mask to modify.
		//! \param left Position of the first bit.
		//! \param right Position of the second bit.
		//! \return No value. Mask is modified in place.
		template <typename T>
		inline auto swap_bits(T& mask, uint32_t left, uint32_t right) {
			// Swap the bits in the read-write mask
			const uint32_t b0 = (mask >> left) & 1U;
			const uint32_t b1 = (mask >> right) & 1U;
			// XOR the two bits
			const uint32_t bxor = b0 ^ b1;
			// Put the XOR bits back to their original positions
			const uint32_t m = (bxor << left) | (bxor << right);
			// XOR mask with the original one effectively swapping the bits
			mask = mask ^ (uint8_t)m;
		}
	} // namespace core
} // namespace gaia
