#pragma once
#include "../config/config.h"

#include <cstdint>
#include <type_traits>

#include "bitset_iterator.h"

namespace gaia {
	namespace cnt {
		template <uint32_t NBits>
		class bitset {
		public:
			static constexpr uint32_t BitCount = NBits;
			static_assert(NBits > 0);

		private:
			template <bool Use32Bit>
			struct size_type_selector {
				using type = std::conditional_t<Use32Bit, uint32_t, uint64_t>;
			};

		public:
			static constexpr uint32_t BitsPerItem = (NBits / 64) > 0 ? 64 : 32;
			static constexpr uint32_t Items = (NBits + BitsPerItem - 1) / BitsPerItem;

			using size_type = typename size_type_selector<BitsPerItem == 32>::type;

		private:
			static constexpr bool HasTrailingBits = (NBits % BitsPerItem) != 0;
			static constexpr size_type LastItemMask = ((size_type)1 << (NBits % BitsPerItem)) - 1;

			size_type m_data[Items]{};

			//! Returns the word stored at the index \param wordIdx
			size_type data(uint32_t wordIdx) const {
				return m_data[wordIdx];
			}

		public:
			using this_bitset = bitset<NBits>;
			using iter = const_iterator<this_bitset>;
			using iter_inv = const_iterator_inverse<this_bitset>;
			using iter_rev = const_reverse_iterator<this_bitset>;
			using iter_rev_inv = const_reverse_inverse_iterator<this_bitset>;
			friend iter;
			friend iter_inv;
			friend iter_rev;
			friend iter_rev_inv;

			constexpr size_type* data() {
				return &m_data[0];
			}

			constexpr const size_type* data() const {
				return &m_data[0];
			}

			//! Returns the number of words used by the bitset internally
			GAIA_NODISCARD constexpr uint32_t items() const {
				return Items;
			}

			constexpr iter begin() const {
				return iter(*this, 0, true);
			}

			constexpr iter end() const {
				return iter(*this, NBits, false);
			}

			constexpr iter_rev rbegin() const {
				return iter_rev(*this, NBits, false);
			}

			constexpr iter_rev rend() const {
				return iter_rev(*this, 0, true);
			}

			constexpr iter_inv ibegin() const {
				return iter_inv(*this, 0, true);
			}

			constexpr iter_inv iend() const {
				return iter_inv(*this, NBits, false);
			}

			constexpr iter_rev_inv ribegin() const {
				return iter_rev_inv(*this, NBits, false);
			}

			constexpr iter_rev_inv riend() const {
				return iter_rev_inv(*this, 0, true);
			}

			GAIA_NODISCARD constexpr bool operator[](uint32_t pos) const {
				return test(pos);
			}

			GAIA_NODISCARD constexpr bool operator==(const bitset& other) const {
				GAIA_FOR(Items) {
					if (m_data[i] != other.m_data[i])
						return false;
				}
				return true;
			}

			GAIA_NODISCARD constexpr bool operator!=(const bitset& other) const {
				GAIA_FOR(Items) {
					if (m_data[i] == other.m_data[i])
						return false;
				}
				return true;
			}

			//! Sets all bits
			constexpr void set() {
				if constexpr (HasTrailingBits) {
					GAIA_FOR(Items - 1) m_data[i] = (size_type)-1;
					m_data[Items - 1] = LastItemMask;
				} else {
					GAIA_FOR(Items) m_data[i] = (size_type)-1;
				}
			}

			//! Sets the bit at the postion \param pos to value \param value
			constexpr void set(uint32_t pos, bool value = true) {
				GAIA_ASSERT(pos < NBits);
				if (value)
					m_data[pos / BitsPerItem] |= ((size_type)1 << (pos % BitsPerItem));
				else
					m_data[pos / BitsPerItem] &= ~((size_type)1 << (pos % BitsPerItem));
			}

			//! Flips all bits
			constexpr bitset& flip() {
				if constexpr (HasTrailingBits) {
					GAIA_FOR(Items - 1) m_data[i] = ~m_data[i];
					m_data[Items - 1] = (~m_data[Items - 1]) & LastItemMask;
				} else {
					GAIA_FOR(Items) m_data[i] = ~m_data[i];
				}
				return *this;
			}

			//! Flips the bit at the postion \param pos
			constexpr void flip(uint32_t pos) {
				GAIA_ASSERT(pos < NBits);
				const auto wordIdx = pos / BitsPerItem;
				const auto bitIdx = pos % BitsPerItem;
				m_data[wordIdx] ^= ((size_type)1 << bitIdx);
			}

			//! Flips all bits from \param bitFrom to \param bitTo (including)
			constexpr bitset& flip(uint32_t bitFrom, uint32_t bitTo) {
				GAIA_ASSERT(bitFrom <= bitTo);
				GAIA_ASSERT(bitTo < size());

				// The following can't happen because we always have at least 1 bit
				// if GAIA_UNLIKELY (size() == 0)
				// 	return *this;

				const uint32_t wordIdxFrom = bitFrom / BitsPerItem;
				const uint32_t wordIdxTo = bitTo / BitsPerItem;

				auto getMask = [](uint32_t from, uint32_t to) -> size_type {
					const auto diff = to - from;
					// Set all bits when asking for the full range
					if (diff == BitsPerItem - 1)
						return (size_type)-1;

					return ((size_type(1) << (diff + 1)) - 1) << from;
				};

				if (wordIdxFrom == wordIdxTo) {
					m_data[wordIdxTo] ^= getMask(bitFrom % BitsPerItem, bitTo % BitsPerItem);
				} else {
					// First word
					m_data[wordIdxFrom] ^= getMask(bitFrom % BitsPerItem, BitsPerItem - 1);
					// Middle
					GAIA_FOR2(wordIdxFrom + 1, wordIdxTo) m_data[i] = ~m_data[i];
					// Last word
					m_data[wordIdxTo] ^= getMask(0, bitTo % BitsPerItem);
				}

				return *this;
			}

			//! Unsets all bits
			constexpr void reset() {
				GAIA_FOR(Items) m_data[i] = 0;
			}

			//! Unsets the bit at the postion \param pos
			constexpr void reset(uint32_t pos) {
				GAIA_ASSERT(pos < NBits);
				m_data[pos / BitsPerItem] &= ~((size_type)1 << (pos % BitsPerItem));
			}

			//! Returns the value of the bit at the position \param pos
			GAIA_NODISCARD constexpr bool test(uint32_t pos) const {
				GAIA_ASSERT(pos < NBits);
				return (m_data[pos / BitsPerItem] & ((size_type)1 << (pos % BitsPerItem))) != 0;
			}

			//! Checks if all bits are set
			GAIA_NODISCARD constexpr bool all() const {
				if constexpr (HasTrailingBits) {
					GAIA_FOR(Items - 1) {
						if (m_data[i] != (size_type)-1)
							return false;
					}
					return (m_data[Items - 1] & LastItemMask) == LastItemMask;
				} else {
					GAIA_FOR(Items) {
						if (m_data[i] != (size_type)-1)
							return false;
					}
					return true;
				}
			}

			//! Checks if any bit is set
			GAIA_NODISCARD constexpr bool any() const {
				GAIA_FOR(Items) {
					if (m_data[i] != 0)
						return true;
				}
				return false;
			}

			//! Checks if all bits are reset
			GAIA_NODISCARD constexpr bool none() const {
				GAIA_FOR(Items) {
					if (m_data[i] != 0)
						return false;
				}
				return true;
			}

			//! Returns the number of set bits
			GAIA_NODISCARD uint32_t count() const {
				uint32_t total = 0;

				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(4244)
				if constexpr (sizeof(size_type) == 4) {
					GAIA_FOR(Items) total += GAIA_POPCNT(m_data[i]);
				} else {
					GAIA_FOR(Items) total += GAIA_POPCNT64(m_data[i]);
				}
				GAIA_MSVC_WARNING_POP()

				return total;
			}

			//! Returns the number of bits the bitset can hold
			GAIA_NODISCARD constexpr uint32_t size() const {
				return NBits;
			}
		};
	} // namespace cnt
} // namespace gaia