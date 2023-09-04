#pragma once
#include "../config/config.h"

#include <cinttypes>
#include <type_traits>

#include "../utils/iterator.h"

namespace gaia {
	namespace containers {
		template <uint32_t NBits>
		class bitset {
		private:
			template <bool Use32Bit>
			struct size_type_selector {
				using type = std::conditional_t<Use32Bit, uint32_t, uint64_t>;
			};

			static constexpr uint32_t BitsPerItem = (NBits / 64) > 0 ? 64 : 32;
			static constexpr uint32_t Items = (NBits + BitsPerItem - 1) / BitsPerItem;
			using size_type = typename size_type_selector<BitsPerItem == 32>::type;
			static constexpr bool HasTrailingBits = (NBits % BitsPerItem) != 0;
			static constexpr size_type LastItemMask = ((size_type)1 << (NBits % BitsPerItem)) - 1;

			size_type m_data[Items]{};

		public:
			class const_iterator {
			public:
				using iterator_category = GAIA_UTIL::random_access_iterator_tag;
				using value_type = uint32_t;
				using size_type = bitset<NBits>::size_type;

			private:
				const bitset<NBits>& m_bitset;
				value_type m_pos;

				void find_next_set_bit() {
					// Don't iterate beyond the range
					if GAIA_UNLIKELY (m_pos + 1 >= NBits) {
						m_pos = NBits;
						return;
					}

					// Keep moving forward
					++m_pos;

					size_type word = m_bitset.m_data[m_pos / bitset::BitsPerItem] >> (m_pos % bitset::BitsPerItem);
					if (word != 0) {
						if constexpr (bitset::BitsPerItem == 32)
							m_pos += GAIA_FFS(word) - 1;
						else
							m_pos += GAIA_FFS64(word) - 1;
					} else {
						// No set bit in the current word, move to the next word
						value_type wordIndex = m_pos / bitset::BitsPerItem + 1;
						while (wordIndex < m_bitset.Items && m_bitset.m_data[wordIndex] == 0)
							++wordIndex;

						if (wordIndex < m_bitset.Items) {
							word = m_bitset.m_data[wordIndex];
							if constexpr (bitset::BitsPerItem == 32)
								m_pos = wordIndex * 32 + GAIA_FFS(word) - 1;
							else
								m_pos = wordIndex * 64 + GAIA_FFS64(word) - 1;
						}
					}
				}

				void find_prev_set_bit() {
					// Don't iterate beyond the range
					GAIA_ASSERT(m_pos > 0);

					value_type wordIndex = (m_pos - 1) / bitset::BitsPerItem;
					size_type word = m_bitset.m_data[wordIndex];
					if (word != 0) {
						m_pos = ((m_pos - 1) / bitset::BitsPerItem) * bitset::BitsPerItem + (bitset::BitsPerItem - 1);
						if constexpr (bitset::BitsPerItem == 32)
							m_pos -= GAIA_CTZ(word);
						else
							m_pos -= GAIA_CTZ64(word);
					} else {
						// No set bit in the current word, move to the previous word
						do {
							--wordIndex;
						} while (wordIndex > 0 && m_bitset.m_data[wordIndex] == 0);

						word = m_bitset.m_data[wordIndex];
						if constexpr (bitset::BitsPerItem == 32)
							m_pos = wordIndex * 32 + 31 - GAIA_CTZ(word);
						else {
							m_pos = wordIndex * 64 + 63 - GAIA_CTZ64(word);
						}
					}
				}

			public:
				const_iterator(const bitset<NBits>& bitset, value_type pos, bool fwd): m_bitset(bitset), m_pos(pos) {
					if (fwd) {
						--m_pos;
						find_next_set_bit();
					} else {
						find_prev_set_bit();
						++m_pos; // point 1 item past the end
					}
				}

				GAIA_NODISCARD value_type operator*() const {
					return m_pos;
				}

				const_iterator& operator++() {
					find_next_set_bit();
					return *this;
				}
				const_iterator operator++(int) {
					const_iterator temp(*this);
					++*this;
					return temp;
				}
				const_iterator& operator--() {
					find_prev_set_bit();
					return *this;
				}
				const_iterator operator--(int) {
					const_iterator temp(*this);
					--*this;
					return temp;
				}

				GAIA_NODISCARD bool operator==(const const_iterator& other) const {
					return m_pos == other.m_pos;
				}
				GAIA_NODISCARD bool operator!=(const const_iterator& other) const {
					return m_pos != other.m_pos;
				}
				GAIA_NODISCARD bool operator>(const const_iterator& other) const {
					return m_pos > other.m_pos;
				}
				GAIA_NODISCARD bool operator>=(const const_iterator& other) const {
					return m_pos >= other.m_pos;
				}
				GAIA_NODISCARD bool operator<(const const_iterator& other) const {
					return m_pos < other.m_pos;
				}
				GAIA_NODISCARD bool operator<=(const const_iterator& other) const {
					return m_pos <= other.m_pos;
				}
			};

			const_iterator begin() const {
				return const_iterator(*this, 0, true);
			}

			const_iterator end() const {
				return const_iterator(*this, NBits, false);
			}

			const_iterator cbegin() const {
				return const_iterator(*this, 0, true);
			}

			const_iterator cend() const {
				return const_iterator(*this, NBits, false);
			}

			GAIA_NODISCARD constexpr bool operator[](uint32_t pos) const {
				return test(pos);
			}

			GAIA_NODISCARD constexpr bool operator==(const bitset& other) const {
				for (uint32_t i = 0; i < Items; ++i)
					if (m_data[i] != other[i])
						return false;
				return true;
			}

			GAIA_NODISCARD constexpr bool operator!=(const bitset& other) const {
				for (uint32_t i = 0; i < Items; ++i)
					if (m_data[i] == other[i])
						return false;
				return true;
			}

			//! Sets all bits
			constexpr void set() {
				for (uint32_t i = 0; i < Items - 1; ++i)
					m_data[i] = (size_type)-1;
				if constexpr (HasTrailingBits)
					m_data[Items - 1] = LastItemMask;
				else
					m_data[Items - 1] = (size_type)-1;
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
			constexpr void flip() {
				for (uint32_t i = 0; i < Items - 1; ++i)
					m_data[i] = ~m_data[i];
				if constexpr (HasTrailingBits)
					m_data[Items - 1] = (~m_data[Items - 1]) & LastItemMask;
				else
					m_data[Items - 1] = ~m_data[Items - 1];
			}

			//! Flips the bit at the postion \param pos
			constexpr void flip(uint32_t pos) {
				GAIA_ASSERT(pos < NBits);
				m_data[pos / BitsPerItem] ^= ((size_type)1 << (pos % BitsPerItem));
			}

			//! Unsets all bits
			constexpr void reset() {
				for (uint32_t i = 0; i < Items; ++i)
					m_data[i] = 0;
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
				for (uint32_t i = 0; i < Items - 1; ++i)
					if (m_data[i] != (size_type)-1)
						return false;
				if constexpr (HasTrailingBits)
					return (m_data[Items - 1] & LastItemMask) == LastItemMask;
				else
					return m_data[Items - 1] == (size_type)-1;
			}

			//! Checks if any bit is set
			GAIA_NODISCARD constexpr bool any() const {
				for (uint32_t i = 0; i < Items; ++i)
					if (m_data[i] != 0)
						return true;
				return false;
			}

			//! Checks if all bits are reset
			GAIA_NODISCARD constexpr bool none() const {
				for (uint32_t i = 0; i < Items; ++i)
					if (m_data[i] != 0)
						return false;
				return true;
			}

			//! Returns the number of set bits
			GAIA_NODISCARD uint32_t count() const {
				uint32_t total = 0;

				if constexpr (sizeof(size_type) == 4) {
					for (uint32_t i = 0; i < Items; ++i)
						total += GAIA_POPCNT(m_data[i]);
				} else {
					for (uint32_t i = 0; i < Items; ++i)
						total += GAIA_POPCNT64(m_data[i]);
				}

				return total;
			}

			//! Returns the number of bits the bitset can hold
			GAIA_NODISCARD constexpr uint32_t size() const {
				return NBits;
			}
		};
	} // namespace containers
} // namespace gaia