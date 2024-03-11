#pragma once
#include "../config/config.h"

#include <cstdint>
#include <type_traits>

#include "../mem/mem_utils.h"
#include "bitset_iterator.h"

namespace gaia {
	namespace cnt {
		class dbitset {
		private:
			struct size_type_selector {
				static constexpr bool Use32Bit = sizeof(size_t) == 4;
				using type = std::conditional_t<Use32Bit, uint32_t, uint64_t>;
			};

			using difference_type = typename size_type_selector::type;
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

			uint32_t items() const {
				return (m_cnt + BitsPerItem - 1) / BitsPerItem;
			}

			bool has_trailing_bits() const {
				return (m_cnt % BitsPerItem) != 0;
			}

			size_type last_item_mask() const {
				return ((size_type)1 << (m_cnt % BitsPerItem)) - 1;
			}

			void try_grow(uint32_t bitsWanted) {
				uint32_t itemsOld = items();
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
					GAIA_FOR2(itemsOld, itemsNew) m_pData[i] = 0;
				} else {
					// Copy the old data over and set the old data to zeros
					mem::copy_elements<size_type>((uint8_t*)m_pData, (const uint8_t*)pDataOld, itemsOld, 0, 0, 0);
					GAIA_FOR2(itemsOld, itemsNew) m_pData[i] = 0;

					// Release the old data
					delete[] pDataOld;
				}
			}

			//! Returns the word stored at the index \param wordIdx
			size_type data(uint32_t wordIdx) const {
				return m_pData[wordIdx];
			}

		public:
			using const_iterator = bitset_const_iterator<dbitset, false>;
			friend const_iterator;
			using const_iterator_inverse = bitset_const_iterator<dbitset, true>;
			friend const_iterator_inverse;

			dbitset(): m_cnt(1) {
				// Allocate at least 128 bits
				reserve(128);
			}

			dbitset(uint32_t reserveBits): m_cnt(1) {
				reserve(reserveBits);
			}

			~dbitset() {
				delete[] m_pData;
			}

			dbitset(const dbitset& other) {
				*this = other;
			}

			dbitset& operator=(const dbitset& other) {
				GAIA_ASSERT(core::addressof(other) != this);

				resize(other.m_cnt);
				mem::copy_elements<size_type>((uint8_t*)m_pData, (const uint8_t*)other.m_pData, other.items(), 0, 0, 0);
				return *this;
			}

			dbitset(dbitset&& other) noexcept {
				*this = GAIA_MOV(other);
			}

			dbitset& operator=(dbitset&& other) noexcept {
				GAIA_ASSERT(core::addressof(other) != this);

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
					GAIA_FOR(itemsNew) m_pData[i] = 0;
				} else {
					const uint32_t itemsOld = items();
					// Copy the old data over and set the old data to zeros
					mem::copy_elements<size_type>((uint8_t*)m_pData, (const uint8_t*)pDataOld, itemsOld, 0, 0, 0);
					GAIA_FOR2(itemsOld, itemsNew) m_pData[i] = 0;

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

				const uint32_t itemsOld = items();

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
					GAIA_FOR(itemsNew) m_pData[i] = 0;
				} else {
					// Copy the old data over and set the old data to zeros
					mem::copy_elements<size_type>((uint8_t*)m_pData, (const uint8_t*)pDataOld, itemsOld, 0, 0, 0);
					GAIA_FOR2(itemsOld, itemsNew) m_pData[i] = 0;

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

			const_iterator_inverse begin_inverse() const {
				return const_iterator_inverse(*this, 0, true);
			}

			const_iterator_inverse end_inverse() const {
				return const_iterator_inverse(*this, size(), false);
			}

			GAIA_NODISCARD bool operator[](uint32_t pos) const {
				return test(pos);
			}

			GAIA_NODISCARD bool operator==(const dbitset& other) const {
				const uint32_t item_count = items();
				GAIA_FOR(item_count) {
					if (m_pData[i] != other.m_pData[i])
						return false;
				}
				return true;
			}

			GAIA_NODISCARD bool operator!=(const dbitset& other) const {
				const uint32_t item_count = items();
				GAIA_FOR(item_count) {
					if (m_pData[i] == other.m_pData[i])
						return false;
				}
				return true;
			}

			//! Sets all bits
			void set() {
				if GAIA_UNLIKELY (size() == 0)
					return;

				const auto item_count = items();
				const auto lastItemMask = last_item_mask();

				if (lastItemMask != 0) {
					GAIA_FOR(item_count - 1) m_pData[i] = (size_type)-1;
					m_pData[item_count - 1] = lastItemMask;
				} else {
					GAIA_FOR(item_count) m_pData[i] = (size_type)-1;
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

				const auto item_count = items();
				const auto lastItemMask = last_item_mask();

				if (lastItemMask != 0) {
					GAIA_FOR(item_count - 1) m_pData[i] = ~m_pData[i];
					m_pData[item_count - 1] = (~m_pData[item_count - 1]) & lastItemMask;
				} else {
					GAIA_FOR(item_count + 1) m_pData[i] = ~m_pData[i];
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
				GAIA_ASSERT(bitTo < size());

				if GAIA_UNLIKELY (size() == 0)
					return *this;

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
					m_pData[wordIdxTo] ^= getMask(bitFrom % BitsPerItem, bitTo % BitsPerItem);
				} else {
					// First word
					m_pData[wordIdxFrom] ^= getMask(bitFrom % BitsPerItem, BitsPerItem - 1);
					// Middle
					GAIA_FOR2(wordIdxFrom + 1, wordIdxTo) m_pData[i] = ~m_pData[i];
					// Last word
					m_pData[wordIdxTo] ^= getMask(0, bitTo % BitsPerItem);
				}

				return *this;
			}

			//! Unsets all bits
			void reset() {
				const auto item_count = items();
				GAIA_FOR(item_count) m_pData[i] = 0;
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
				const auto item_count = items() - 1;
				const auto lastItemMask = last_item_mask();

				GAIA_FOR(item_count) {
					if (m_pData[i] != (size_type)-1)
						return false;
				}

				if (has_trailing_bits())
					return (m_pData[item_count] & lastItemMask) == lastItemMask;

				return m_pData[item_count] == (size_type)-1;
			}

			//! Checks if any bit is set
			GAIA_NODISCARD bool any() const {
				const auto item_count = items();
				GAIA_FOR(item_count) {
					if (m_pData[i] != 0)
						return true;
				}
				return false;
			}

			//! Checks if all bits are reset
			GAIA_NODISCARD bool none() const {
				const auto item_count = items();
				GAIA_FOR(item_count) {
					if (m_pData[i] != 0)
						return false;
				}
				return true;
			}

			//! Returns the number of set bits
			GAIA_NODISCARD uint32_t count() const {
				uint32_t total = 0;

				const auto item_count = items();

				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(4244)
				if constexpr (sizeof(size_type) == 4) {
					GAIA_FOR(item_count) total += GAIA_POPCNT(m_pData[i]);
				} else {
					GAIA_FOR(item_count) total += GAIA_POPCNT64(m_pData[i]);
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
	} // namespace cnt
} // namespace gaia