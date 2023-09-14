#pragma once
#include "../config/config.h"

#include <cinttypes>
#include <type_traits>

#include "../utils/iterator.h"
#include "../utils/mem.h"

namespace gaia {
	namespace containers {
		class dbitset {
		private:
			struct size_type_selector {
				static constexpr bool Use32Bit = sizeof(size_t) == 4;
				using type = std::conditional_t<Use32Bit, uint32_t, uint64_t>;
			};

			using difference_type = std::ptrdiff_t;
			using size_type = typename size_type_selector::type;
			using value_type = size_type;
			using reference = size_type&;
			using const_reference = const size_type&;
			using pointer = size_type*;
			using const_pointer = size_type*;

			static constexpr uint32_t BitsPerItem = sizeof(size_type_selector::type) * 8;

			pointer m_pData = nullptr;
			uint32_t m_cnt = uint32_t(0);
			uint32_t m_cap = uint32_t(0);

			uint32_t Items() const {
				return (m_cnt + BitsPerItem - 1) / BitsPerItem;
			}

			bool HasTrailingBits() const {
				return (m_cnt % BitsPerItem) != 0;
			}

			size_type LastItemMask() const {
				return ((size_type)1 << (m_cnt % BitsPerItem)) - 1;
			}

			void try_grow(uint32_t bitsWanted) {
				uint32_t itemsOld = Items();
				if GAIA_UNLIKELY (bitsWanted > size())
					m_cnt = bitsWanted;
				if GAIA_LIKELY (m_cnt <= capacity())
					return;

				// Increase the size of an existing array.
				// We are pessimistic with our allocations and only allocate as much as we need.
				// If we know the expected size ahead of the time a manual call to reserve is necessary.
				const uint32_t itemsNew = (m_cnt + BitsPerItem - 1) / BitsPerItem;
				m_cap = itemsNew * BitsPerItem;

				pointer pDataOld = m_pData;
				m_pData = new size_type[itemsNew];
				if (pDataOld == nullptr) {
					// Make sure the new data is set to zeros
					for (uint32_t i = itemsOld; i < itemsNew; ++i)
						m_pData[i] = 0;
				} else {
					// Copy the old data over and set the old data to zeros
					utils::move_elements(m_pData, pDataOld, itemsNew - itemsOld);
					for (uint32_t i = itemsOld; i < itemsNew; ++i)
						m_pData[i] = 0;

					// Release the old data
					delete[] pDataOld;
				}
			}

		public:
			class const_iterator {
			public:
				using iterator_category = GAIA_UTIL::random_access_iterator_tag;
				using value_type = uint32_t;
				using size_type = dbitset::size_type;

			private:
				const dbitset& m_bitset;
				value_type m_pos;

				uint32_t find_next_set_bit(uint32_t pos) const {
					value_type wordIndex = pos / dbitset::BitsPerItem;
					GAIA_ASSERT(wordIndex < m_bitset.Items());
					size_type word = 0;

					const size_type posInWord = pos % dbitset::BitsPerItem;
					if (posInWord < dbitset::BitsPerItem - 1) {
						const size_type mask = (size_type(1) << (posInWord + 1)) - 1;
						const size_type maskInv = ~mask;
						word = m_bitset.m_pData[wordIndex] & maskInv;
					}

					// No set bit in the current word, move to the next one
					while (word == 0) {
						if (wordIndex >= m_bitset.Items() - 1)
							return pos;

						word = m_bitset.m_pData[++wordIndex];
					}

					// Process the word
					uint32_t fwd = 0;

					GAIA_MSVC_WARNING_PUSH()
					GAIA_MSVC_WARNING_DISABLE(4244)
					if constexpr (dbitset::BitsPerItem == 32)
						fwd = GAIA_FFS(word) - 1;
					else
						fwd = GAIA_FFS64(word) - 1;
					GAIA_MSVC_WARNING_POP()

					return wordIndex * dbitset::BitsPerItem + fwd;
				}

				uint32_t find_prev_set_bit(uint32_t pos) const {
					value_type wordIndex = pos / dbitset::BitsPerItem;
					GAIA_ASSERT(wordIndex < m_bitset.Items());
					size_type word = m_bitset.m_pData[wordIndex];

					// No set bit in the current word, move to the previous word
					while (word == 0) {
						if (wordIndex == 0)
							return pos;
						word = m_bitset.m_pData[--wordIndex];
					}

					// Process the word
					uint32_t ctz = 0;

					GAIA_MSVC_WARNING_PUSH()
					GAIA_MSVC_WARNING_DISABLE(4244)
					if constexpr (dbitset::BitsPerItem == 32)
						ctz = GAIA_CTZ(word);
					else
						ctz = GAIA_CTZ64(word);
					GAIA_MSVC_WARNING_POP()

					return dbitset::BitsPerItem * (wordIndex + 1) - ctz - 1;
				}

			public:
				const_iterator(const dbitset& bitset, value_type pos, bool fwd): m_bitset(bitset), m_pos(pos) {
					const auto bitsetSize = bitset.size();
					if (bitsetSize == 0) {
						// Point beyond the last item if no set bit was found
						m_pos = bitsetSize;
						return;
					}

					// Stay inside bounds
					const auto lastBit = bitsetSize - 1;
					if (pos >= bitsetSize)
						pos = lastBit;

					if (fwd) {
						// Find the first set bit)
						if (pos != 0 || !bitset.test(0)) {
							pos = find_next_set_bit(m_pos);
							// Point beyond the last item if no set bit was found
							if (pos == m_pos)
								pos = bitsetSize;
						}
					} else {
						// Find the last set bit
						if (pos != lastBit || !bitset.test(lastBit)) {
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

				GAIA_NODISCARD value_type operator*() const {
					return m_pos;
				}

				const_iterator& operator++() {
					uint32_t newPos = find_next_set_bit(m_pos);
					// Point one past the last item if no new bit was found
					if (newPos == m_pos)
						++newPos;
					m_pos = newPos;
					return *this;
				}

				GAIA_NODISCARD const_iterator operator++(int) {
					const_iterator temp(*this);
					++*this;
					return temp;
				}

				GAIA_NODISCARD bool operator==(const const_iterator& other) const {
					return m_pos == other.m_pos;
				}

				GAIA_NODISCARD bool operator!=(const const_iterator& other) const {
					return m_pos != other.m_pos;
				}
			};

			dbitset() {
				// Allocate at least 128 bits
				reserve(128);
			}

			dbitset(uint32_t reserveBits) {
				reserve(reserveBits);
			}

			~dbitset() {
				delete[] m_pData;
			}

			dbitset(const dbitset& other) {
				*this = other;
			}
			dbitset& operator=(const dbitset& other) {
				GAIA_ASSERT(GAIA_UTIL::addressof(other) != this);

				resize(other.m_cnt);
				utils::copy_elements(m_pData, other.m_pData, other.Items());
				return *this;
			}

			dbitset(dbitset&& other) noexcept {
				*this = std::move(other);
			}
			dbitset& operator=(dbitset&& other) noexcept {
				GAIA_ASSERT(GAIA_UTIL::addressof(other) != this);

				m_pData = other.m_pData;
				m_cnt = other.m_cnt;
				m_cap = other.m_cap;

				other.m_pData = nullptr;
				other.m_cnt = 0;
				other.m_cap = 0;
				return *this;
			}

			void reserve(uint32_t bitsWanted) {
				// Make sure at least one bit is requested
				GAIA_ASSERT(bitsWanted > 0);
				if (bitsWanted < 1)
					bitsWanted = 1;

				// Nothing to do if the capacity is already bigger than requested
				if (bitsWanted <= capacity())
					return;

				const uint32_t itemsNew = (bitsWanted + BitsPerItem - 1) / BitsPerItem;
				auto* pDataOld = m_pData;
				m_pData = new size_type[itemsNew];
				if (pDataOld == nullptr) {
					// Make sure the new data is set to zeros
					for (uint32_t i = 0; i < itemsNew; ++i)
						m_pData[i] = 0;
				} else {
					const uint32_t itemsOld = Items();
					// Copy the old data over and set the old data to zeros
					utils::move_elements(m_pData, pDataOld, size());
					for (uint32_t i = itemsOld; i < itemsNew; ++i)
						m_pData[i] = 0;

					// Release old data
					delete[] pDataOld;
				}

				m_cap = itemsNew * BitsPerItem;
			}

			void resize(uint32_t bitsWanted) {
				// Make sure at least one bit is requested
				GAIA_ASSERT(bitsWanted > 0);
				if (bitsWanted < 1)
					bitsWanted = 1;

				const uint32_t itemsOld = Items();

				// Nothing to do if the capacity is already bigger than requested
				if (bitsWanted <= capacity()) {
					m_cnt = bitsWanted;
					return;
				}

				const uint32_t itemsNew = (bitsWanted + BitsPerItem - 1) / BitsPerItem;
				auto* pDataOld = m_pData;
				m_pData = new size_type[itemsNew];
				if (pDataOld == nullptr) {
					// Make sure the new data is set to zeros
					for (uint32_t i = 0; i < itemsNew; ++i)
						m_pData[i] = 0;
				} else {
					// Copy the old data over and set the old data to zeros
					utils::move_elements(m_pData, pDataOld, size());
					for (uint32_t i = itemsOld; i < itemsNew; ++i)
						m_pData[i] = 0;

					// Release old data
					delete[] pDataOld;
				}

				m_cnt = bitsWanted;
				m_cap = itemsNew * BitsPerItem;
			}

			const_iterator begin() const {
				return const_iterator(*this, 0, true);
			}

			const_iterator end() const {
				return const_iterator(*this, size(), false);
			}

			const_iterator cbegin() const {
				return const_iterator(*this, 0, true);
			}

			const_iterator cend() const {
				return const_iterator(*this, size(), false);
			}

			GAIA_NODISCARD bool operator[](uint32_t pos) const {
				return test(pos);
			}

			GAIA_NODISCARD bool operator==(const dbitset& other) const {
				const uint32_t items = Items();
				for (uint32_t i = 0; i < items; ++i)
					if (m_pData[i] != other.m_pData[i])
						return false;
				return true;
			}

			GAIA_NODISCARD bool operator!=(const dbitset& other) const {
				const uint32_t items = Items();
				for (uint32_t i = 0; i < items; ++i)
					if (m_pData[i] == other.m_pData[i])
						return false;
				return true;
			}

			//! Sets all bits
			void set() {
				if GAIA_UNLIKELY (size() == 0)
					return;

				const auto items = Items();
				const auto lastItemMask = LastItemMask();

				if (lastItemMask != 0) {
					uint32_t i = 0;
					for (; i < items - 1; ++i)
						m_pData[i] = (size_type)-1;
					m_pData[i] = lastItemMask;
				} else {
					for (uint32_t i = 0; i < items; ++i)
						m_pData[i] = (size_type)-1;
				}
			}

			//! Sets the bit at the postion \param pos to value \param value
			void set(uint32_t pos, bool value = true) {
				try_grow(pos + 1);

				if (value)
					m_pData[pos / BitsPerItem] |= ((size_type)1 << (pos % BitsPerItem));
				else
					m_pData[pos / BitsPerItem] &= ~((size_type)1 << (pos % BitsPerItem));
			}

			//! Flips all bits
			void flip() {
				if GAIA_UNLIKELY (size() == 0)
					return;

				const auto items = Items();
				const auto lastItemMask = LastItemMask();

				if (lastItemMask != 0) {
					uint32_t i = 0;
					for (; i < items - 1; ++i)
						m_pData[i] = ~m_pData[i];
					m_pData[i] = (~m_pData[i]) & lastItemMask;
				} else {
					for (uint32_t i = 0; i <= items; ++i)
						m_pData[i] = ~m_pData[i];
				}
			}

			//! Flips the bit at the postion \param pos
			void flip(uint32_t pos) {
				GAIA_ASSERT(pos < size());
				m_pData[pos / BitsPerItem] ^= ((size_type)1 << (pos % BitsPerItem));
			}

			//! Flips all bits from \param bitFrom to \param bitTo (including)
			dbitset& flip(uint32_t bitFrom, uint32_t bitTo) {
				GAIA_ASSERT(bitFrom <= bitTo);
				GAIA_ASSERT(bitFrom < size());

				if GAIA_UNLIKELY (size() == 0)
					return *this;

				for (uint32_t i = bitFrom; i <= bitTo; i++) {
					uint32_t wordIdx = i / BitsPerItem;
					uint32_t bitOffset = i % BitsPerItem;
					m_pData[wordIdx] ^= ((size_type)1 << bitOffset);
				}

				return *this;
			}

			//! Unsets all bits
			void reset() {
				const auto items = Items();
				for (uint32_t i = 0; i < items; ++i)
					m_pData[i] = 0;
			}

			//! Unsets the bit at the postion \param pos
			void reset(uint32_t pos) {
				GAIA_ASSERT(pos < size());
				m_pData[pos / BitsPerItem] &= ~((size_type)1 << (pos % BitsPerItem));
			}

			//! Returns the value of the bit at the position \param pos
			GAIA_NODISCARD bool test(uint32_t pos) const {
				GAIA_ASSERT(pos < size());
				return (m_pData[pos / BitsPerItem] & ((size_type)1 << (pos % BitsPerItem))) != 0;
			}

			//! Checks if all bits are set
			GAIA_NODISCARD bool all() const {
				const auto items = Items() - 1;
				const auto lastItemMask = LastItemMask();

				for (uint32_t i = 0; i < items; ++i)
					if (m_pData[i] != (size_type)-1)
						return false;

				if (HasTrailingBits())
					return (m_pData[items] & lastItemMask) == lastItemMask;
				else
					return m_pData[items] == (size_type)-1;
			}

			//! Checks if any bit is set
			GAIA_NODISCARD bool any() const {
				const auto items = Items();
				for (uint32_t i = 0; i < items; ++i)
					if (m_pData[i] != 0)
						return true;
				return false;
			}

			//! Checks if all bits are reset
			GAIA_NODISCARD bool none() const {
				const auto items = Items();
				for (uint32_t i = 0; i < items; ++i)
					if (m_pData[i] != 0)
						return false;
				return true;
			}

			//! Returns the number of set bits
			GAIA_NODISCARD uint32_t count() const {
				uint32_t total = 0;

				const auto items = Items();

				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(4244)
				if constexpr (sizeof(size_type) == 4) {
					for (uint32_t i = 0; i < items; ++i)
						total += GAIA_POPCNT(m_pData[i]);
				} else {
					for (uint32_t i = 0; i < items; ++i)
						total += GAIA_POPCNT64(m_pData[i]);
				}
				GAIA_MSVC_WARNING_POP()

				return total;
			}

			//! Returns the number of bits the dbitset holds
			GAIA_NODISCARD constexpr uint32_t size() const {
				return m_cnt;
			}

			//! Returns the number of bits the dbitset can hold
			GAIA_NODISCARD constexpr uint32_t capacity() const {
				return m_cap;
			}
		};
	} // namespace containers
} // namespace gaia