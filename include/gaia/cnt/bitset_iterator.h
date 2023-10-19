#pragma once
#include "../config/config.h"

#include <cinttypes>
#include <type_traits>

namespace gaia {
	namespace cnt {
		template <typename TBitset, bool IsInverse>
		class bitset_const_iterator {
		public:
			using value_type = uint32_t;
			using size_type = typename TBitset::size_type;

		private:
			const TBitset& m_bitset;
			value_type m_pos;

			size_type item(uint32_t wordIdx) const {
				if constexpr (IsInverse)
					return ~m_bitset.data(wordIdx);
				else
					return m_bitset.data(wordIdx);
			}

			uint32_t find_next_set_bit(uint32_t pos) const {
				value_type wordIndex = pos / TBitset::BitsPerItem;
				const auto item_count = m_bitset.items();
				GAIA_ASSERT(wordIndex < item_count);
				size_type word = 0;

				const size_type posInWord = pos % TBitset::BitsPerItem + 1;
				if GAIA_LIKELY (posInWord < TBitset::BitsPerItem) {
					const size_type mask = (size_type(1) << posInWord) - 1;
					word = item(wordIndex) & (~mask);
				}

				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(4244)
				while (true) {
					if (word != 0) {
						if constexpr (TBitset::BitsPerItem == 32)
							return wordIndex * TBitset::BitsPerItem + GAIA_FFS(word) - 1;
						else
							return wordIndex * TBitset::BitsPerItem + GAIA_FFS64(word) - 1;
					}

					// No set bit in the current word, move to the next one
					if (++wordIndex >= item_count)
						return pos;

					word = item(wordIndex);
				}
				GAIA_MSVC_WARNING_POP()
			}

			uint32_t find_prev_set_bit(uint32_t pos) const {
				value_type wordIndex = pos / TBitset::BitsPerItem;
				GAIA_ASSERT(wordIndex < m_bitset.items());

				const size_type posInWord = pos % TBitset::BitsPerItem;
				const size_type mask = (size_type(1) << posInWord) - 1;
				size_type word = item(wordIndex) & mask;

				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(4244)
				while (true) {
					if (word != 0) {
						if constexpr (TBitset::BitsPerItem == 32)
							return TBitset::BitsPerItem * (wordIndex + 1) - GAIA_CTZ(word) - 1;
						else
							return TBitset::BitsPerItem * (wordIndex + 1) - GAIA_CTZ64(word) - 1;
					}

					// No set bit in the current word, move to the previous one
					if (wordIndex == 0)
						return pos;

					word = item(--wordIndex);
				}
				GAIA_MSVC_WARNING_POP()
			}

		public:
			bitset_const_iterator(const TBitset& bitset, value_type pos, bool fwd): m_bitset(bitset), m_pos(pos) {
				if (fwd) {
					if constexpr (IsInverse) {
						// Find the first set bit
						if (pos != 0 || bitset.test(0)) {
							pos = find_next_set_bit(m_pos);
							// Point beyond the last item if no set bit was found
							if (pos == m_pos)
								pos = bitset.size();
						}
					} else {
						// Find the first set bit
						if (pos != 0 || !bitset.test(0)) {
							pos = find_next_set_bit(m_pos);
							// Point beyond the last item if no set bit was found
							if (pos == m_pos)
								pos = bitset.size();
						}
					}
					m_pos = pos;
				} else {
					const auto bitsetSize = bitset.size();
					const auto lastBit = bitsetSize - 1;

					// Stay inside bounds
					if (pos >= bitsetSize)
						pos = bitsetSize - 1;

					if constexpr (IsInverse) {
						// Find the last set bit
						if (pos != lastBit || bitset.test(pos)) {
							const uint32_t newPos = find_prev_set_bit(pos);
							// Point one beyond the last found bit
							pos = (newPos == pos) ? bitsetSize : newPos + 1;
						}
						// Point one beyond the last found bit
						else
							++pos;
					} else {
						// Find the last set bit
						if (pos != lastBit || !bitset.test(pos)) {
							const uint32_t newPos = find_prev_set_bit(pos);
							// Point one beyond the last found bit
							pos = (newPos == pos) ? bitsetSize : newPos + 1;
						}
						// Point one beyond the last found bit
						else
							++pos;
					}

					m_pos = pos;
				}
			}

			GAIA_NODISCARD value_type operator*() const {
				return m_pos;
			}

			GAIA_NODISCARD value_type operator->() const {
				return m_pos;
			}

			GAIA_NODISCARD value_type index() const {
				return m_pos;
			}

			bitset_const_iterator& operator++() {
				uint32_t newPos = find_next_set_bit(m_pos);
				// Point one past the last item if no new bit was found
				if (newPos == m_pos)
					++newPos;
				m_pos = newPos;
				return *this;
			}

			GAIA_NODISCARD bitset_const_iterator operator++(int) {
				bitset_const_iterator temp(*this);
				++*this;
				return temp;
			}

			GAIA_NODISCARD bool operator==(const bitset_const_iterator& other) const {
				return m_pos == other.m_pos;
			}

			GAIA_NODISCARD bool operator!=(const bitset_const_iterator& other) const {
				return m_pos != other.m_pos;
			}
		};
	} // namespace cnt
} // namespace gaia