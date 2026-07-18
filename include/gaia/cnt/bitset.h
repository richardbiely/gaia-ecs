#pragma once
#include "gaia/config/config.h"

#include <cstdint>
#include <type_traits>

#include "gaia/cnt/bitset_iterator.h"

namespace gaia {
	namespace cnt {
		//! Fixed-size bit set.
		//! \tparam NBits Number of addressable bits.
		template <uint32_t NBits>
		class bitset {
		public:
			//! Number of addressable bits.
			static constexpr uint32_t BitCount = NBits;
			static_assert(NBits > 0);

		private:
			template <bool Use32Bit>
			struct size_type_selector {
				using type = std::conditional_t<Use32Bit, uint32_t, uint64_t>;
			};

		public:
			//! Number of bits stored in each backing word.
			static constexpr uint32_t BitsPerItem = (NBits / 64) > 0 ? 64 : 32;
			//! Number of backing words.
			static constexpr uint32_t Items = (NBits + BitsPerItem - 1) / BitsPerItem;

			//! Unsigned backing-word type.
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
			//! Forward iterator over set bit indices.
			using iter = const_iterator<bitset>;
			//! Forward iterator over unset bit indices.
			using iter_inv = const_iterator_inverse<bitset>;
			//! Reverse iterator over set bit indices.
			using iter_rev = const_reverse_iterator<bitset>;
			//! Reverse iterator over unset bit indices.
			using iter_rev_inv = const_reverse_inverse_iterator<bitset>;
			//! \cond INTERNAL
			friend iter;
			friend iter_inv;
			friend iter_rev;
			friend iter_rev_inv;
			//! \endcond

			//! Returns the mutable backing-word storage.
			//! \return Pointer to the first backing word.
			constexpr size_type* data() {
				return &m_data[0];
			}

			//! Returns the immutable backing-word storage.
			//! \return Pointer to the first backing word.
			constexpr const size_type* data() const {
				return &m_data[0];
			}

			//! Returns the number of words used by the bitset internally
			//! \return Number of backing words.
			GAIA_NODISCARD constexpr uint32_t items() const {
				return Items;
			}

			//! Returns an iterator to the first set bit.
			//! \return Forward iterator to the first set bit, or end() when none is set.
			constexpr iter begin() const {
				return iter(*this, 0, true);
			}

			//! Returns the forward set-bit sentinel.
			//! \return Sentinel following the last set bit.
			constexpr iter end() const {
				return iter(*this, NBits, false);
			}

			//! Returns an iterator to the last set bit.
			//! \return Reverse iterator to the last set bit, or rend() when none is set.
			constexpr iter_rev rbegin() const {
				return iter_rev(*this, NBits, false);
			}

			//! Returns the reverse set-bit sentinel.
			//! \return Sentinel preceding the first set bit.
			constexpr iter_rev rend() const {
				return iter_rev(*this, 0, true);
			}

			//! Returns an iterator to the first unset bit.
			//! \return Forward iterator to the first unset bit, or iend() when all bits are set.
			constexpr iter_inv ibegin() const {
				return iter_inv(*this, 0, true);
			}

			//! Returns the forward unset-bit sentinel.
			//! \return Sentinel following the last unset bit.
			constexpr iter_inv iend() const {
				return iter_inv(*this, NBits, false);
			}

			//! Returns an iterator to the last unset bit.
			//! \return Reverse iterator to the last unset bit, or riend() when all bits are set.
			constexpr iter_rev_inv ribegin() const {
				return iter_rev_inv(*this, NBits, false);
			}

			//! Returns the reverse unset-bit sentinel.
			//! \return Sentinel preceding the first unset bit.
			constexpr iter_rev_inv riend() const {
				return iter_rev_inv(*this, 0, true);
			}

			//! Tests a bit.
			//! \param pos Zero-based bit position, which must be less than NBits.
			//! \return True when the bit is set. False otherwise.
			GAIA_NODISCARD constexpr bool operator[](uint32_t pos) const {
				return test(pos);
			}

			//! Compares two bit sets for equality.
			//! \param other Bit set to compare with.
			//! \return True when all backing words are equal. False otherwise.
			GAIA_NODISCARD constexpr bool operator==(const bitset& other) const {
				GAIA_FOR(Items) {
					if (m_data[i] != other.m_data[i])
						return false;
				}
				return true;
			}

			//! Compares two bit sets for inequality.
			//! \param other Bit set to compare with.
			//! \return True when every compared backing word differs. False otherwise.
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

			//! Sets the bit at the given position.
			//! \param pos Bit position to set (0-based, must be < NBits)
			//! \param value Value to set the bit to. Defaults to true.
			constexpr void set(uint32_t pos, bool value = true) {
				GAIA_ASSERT(pos < NBits);
				if (value)
					m_data[pos / BitsPerItem] |= ((size_type)1 << (pos % BitsPerItem));
				else
					m_data[pos / BitsPerItem] &= ~((size_type)1 << (pos % BitsPerItem));
			}

			//! Flips all bits
			//! \return This bit set.
			constexpr bitset& flip() {
				if constexpr (HasTrailingBits) {
					GAIA_FOR(Items - 1) m_data[i] = ~m_data[i];
					m_data[Items - 1] = (~m_data[Items - 1]) & LastItemMask;
				} else {
					GAIA_FOR(Items) m_data[i] = ~m_data[i];
				}
				return *this;
			}

			//! Flips one bit.
			//! \param pos Zero-based bit position, which must be less than NBits.
			constexpr void flip(uint32_t pos) {
				GAIA_ASSERT(pos < NBits);
				const auto wordIdx = pos / BitsPerItem;
				const auto bitIdx = pos % BitsPerItem;
				m_data[wordIdx] ^= ((size_type)1 << bitIdx);
			}

			//! Flips an inclusive range of bits.
			//! \param bitFrom First bit position to flip.
			//! \param bitTo Last bit position to flip, inclusive.
			//! \return This bit set.
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

			//! Unsets one bit.
			//! \param pos Zero-based bit position, which must be less than NBits.
			constexpr void reset(uint32_t pos) {
				GAIA_ASSERT(pos < NBits);
				m_data[pos / BitsPerItem] &= ~((size_type)1 << (pos % BitsPerItem));
			}

			//! Returns the value of one bit.
			//! \param pos Zero-based bit position, which must be less than NBits.
			//! \return True when the bit is set. False otherwise.
			GAIA_NODISCARD constexpr bool test(uint32_t pos) const {
				GAIA_ASSERT(pos < NBits);
				return (m_data[pos / BitsPerItem] & ((size_type)1 << (pos % BitsPerItem))) != 0;
			}

			//! Checks if all bits are set
			//! \return True when every bit is set. False otherwise.
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
			//! \return True when at least one bit is set. False otherwise.
			GAIA_NODISCARD constexpr bool any() const {
				GAIA_FOR(Items) {
					if (m_data[i] != 0)
						return true;
				}
				return false;
			}

			//! Checks if all bits are reset
			//! \return True when no bit is set. False otherwise.
			GAIA_NODISCARD constexpr bool none() const {
				GAIA_FOR(Items) {
					if (m_data[i] != 0)
						return false;
				}
				return true;
			}

			//! Returns the number of set bits
			//! \return Number of set bits.
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
			//! \return NBits.
			GAIA_NODISCARD constexpr uint32_t size() const {
				return NBits;
			}
		};
	} // namespace cnt
} // namespace gaia
