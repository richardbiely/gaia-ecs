#pragma once
#include "gaia/config/config.h"

#include <cstdint>
#include <type_traits>

#include "gaia/cnt/bitset_iterator.h"
#include "gaia/core/utility.h"
#include "gaia/mem/mem_alloc.h"
#include "gaia/mem/mem_utils.h"

namespace gaia {
	namespace cnt {
		//! Dynamically sized bit set.
		//! \tparam Allocator Allocator adaptor used for backing-word storage.
		template <typename Allocator = mem::DefaultAllocatorAdaptor>
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
			using const_pointer = const size_type*;

			static constexpr uint32_t BitsPerItem = sizeof(typename size_type_selector::type) * 8;

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
				m_pData = mem::AllocHelper::alloc<size_type, Allocator>(itemsNew);

				if (pDataOld == nullptr) {
					// Make sure the new data is set to zeros
					GAIA_FOR(itemsNew) m_pData[i] = 0;
				} else {
					// Copy the old data over and set the old data to zeros
					mem::copy_elements<size_type, false>((uint8_t*)m_pData, (const uint8_t*)pDataOld, itemsOld, 0, 0, 0);
					GAIA_FOR2(itemsOld, itemsNew) m_pData[i] = 0;

					// Release the old data
					mem::AllocHelper::free<Allocator>((void*)pDataOld);
				}
			}

			//! Returns the word stored at the index \param wordIdx
			size_type data(uint32_t wordIdx) const {
				return m_pData[wordIdx];
			}

		public:
			//! Forward iterator over set bit indices.
			using iter = const_iterator<dbitset>;
			//! Forward iterator over unset bit indices.
			using iter_inv = const_iterator_inverse<dbitset>;
			//! Reverse iterator over set bit indices.
			using iter_rev = const_reverse_iterator<dbitset>;
			//! Reverse iterator over unset bit indices.
			using iter_rev_inv = const_reverse_inverse_iterator<dbitset>;

			//! \cond INTERNAL
			friend iter;
			friend iter_inv;
			friend iter_rev;
			friend iter_rev_inv;
			//! \endcond

			//! Constructs a bit set with capacity for at least 128 bits and an initial size of one bit.
			dbitset(): m_cnt(1) {
				// Allocate at least 128 bits
				reserve(128);
			}

			//! Constructs a bit set with requested initial capacity and a size of one bit.
			//! \param reserveBits Minimum requested capacity in bits.
			dbitset(uint32_t reserveBits): m_cnt(1) {
				reserve(reserveBits);
			}

			~dbitset() {
				mem::AllocHelper::free<Allocator>((void*)m_pData);
			}

			//! Copy-constructs a bit set.
			//! \param other Bit set to copy.
			dbitset(const dbitset& other) {
				resize(other.m_cnt);
				mem::copy_elements<size_type, false>((uint8_t*)m_pData, (const uint8_t*)other.m_pData, other.items(), 0, 0, 0);
			}

			//! Copy-assigns a bit set.
			//! \param other Bit set to copy.
			//! \return This bit set.
			dbitset& operator=(const dbitset& other) {
				GAIA_ASSERT(core::addressof(other) != this);

				resize(other.m_cnt);
				mem::copy_elements<size_type, false>((uint8_t*)m_pData, (const uint8_t*)other.m_pData, other.items(), 0, 0, 0);
				return *this;
			}

			//! Move-constructs a bit set and leaves the source empty.
			//! \param other Bit set whose storage is transferred.
			dbitset(dbitset&& other) noexcept {
				m_pData = other.m_pData;
				m_cnt = other.m_cnt;
				m_cap = other.m_cap;

				other.m_pData = nullptr;
				other.m_cnt = 0;
				other.m_cap = 0;
			}

			//! Move-assigns a bit set and leaves the source empty.
			//! \param other Bit set whose storage is transferred.
			//! \return This bit set.
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

			//! Reserves storage without changing the current bit count.
			//! \param bitsWanted Minimum requested capacity in bits.
			void reserve(uint32_t bitsWanted) {
				// Make sure at least one bit is requested
				if (bitsWanted < 1)
					bitsWanted = 1;

				// Nothing to do if the capacity is already bigger than requested
				if (bitsWanted <= capacity())
					return;

				const uint32_t itemsOld = m_cap / BitsPerItem;
				const uint32_t itemsNew = (bitsWanted + BitsPerItem - 1) / BitsPerItem;
				if (itemsOld != itemsNew) {
					auto* pDataOld = m_pData;
					m_pData = mem::AllocHelper::alloc<size_type, Allocator>(itemsNew);

					if (pDataOld == nullptr) {
						// Make sure the new data is set to zeros
						GAIA_FOR(itemsNew) m_pData[i] = 0;
					} else {
						const uint32_t itemsOld2 = items();
						// Copy the old data over and set the old data to zeros
						mem::copy_elements<size_type, false>((uint8_t*)m_pData, (const uint8_t*)pDataOld, itemsOld2, 0, 0, 0);
						GAIA_FOR2(itemsOld2, itemsNew) m_pData[i] = 0;

						// Release old data
						mem::AllocHelper::free<Allocator>(pDataOld);
					}
				}

				m_cap = itemsNew * BitsPerItem;
			}

			//! Changes the number of addressable bits.
			//! \param bitsWanted Requested size in bits. Values below one are clamped to one.
			void resize(uint32_t bitsWanted) {
				// Make sure at least one bit is requested
				if (bitsWanted < 1)
					bitsWanted = 1;

				// Nothing to do if the capacity is already bigger than requested
				if (bitsWanted == size())
					return;

				const uint32_t itemsOld = m_cap / BitsPerItem;
				const uint32_t itemsNew = (bitsWanted + BitsPerItem - 1) / BitsPerItem;
				if (itemsOld != itemsNew) {
					auto* pDataOld = m_pData;
					m_pData = mem::AllocHelper::alloc<size_type, Allocator>(itemsNew);

					if (pDataOld == nullptr) {
						// Make sure the new data is set to zeros
						GAIA_FOR(itemsNew) m_pData[i] = 0;
					} else {
						// Copy only the storage that exists in both allocations. resize() can shrink the
						// backing store when the requested bit count is smaller than the current size.
						const uint32_t itemsToCopy = itemsOld < itemsNew ? itemsOld : itemsNew;
						mem::copy_elements<size_type, false>((uint8_t*)m_pData, (const uint8_t*)pDataOld, itemsToCopy, 0, 0, 0);
						// Set the old data to zeros.
						// If resizing to a smaller size this will do nothing
						GAIA_FOR2(itemsToCopy, itemsNew) m_pData[i] = 0;

						// Release old data
						mem::AllocHelper::free<Allocator>((void*)pDataOld);
					}

					m_cap = itemsNew * BitsPerItem;
				}

				m_cnt = bitsWanted;
			}

			//! Returns an iterator to the first set bit.
			//! \return Forward iterator to the first set bit, or end() when none is set.
			iter begin() const {
				return iter(*this, 0, true);
			}

			//! Returns the forward set-bit sentinel.
			//! \return Sentinel following the last set bit.
			iter end() const {
				return iter(*this, size(), false);
			}

			//! Returns an iterator to the last set bit.
			//! \return Reverse iterator to the last set bit, or rend() when none is set.
			iter_rev rbegin() const {
				return iter_rev(*this, size(), false);
			}

			//! Returns the reverse set-bit sentinel.
			//! \return Sentinel preceding the first set bit.
			iter_rev rend() const {
				return iter_rev(*this, 0, true);
			}

			//! Returns an iterator to the first unset bit.
			//! \return Forward iterator to the first unset bit, or iend() when all bits are set.
			iter_inv ibegin() const {
				return iter_inv(*this, 0, true);
			}

			//! Returns the forward unset-bit sentinel.
			//! \return Sentinel following the last unset bit.
			iter_inv iend() const {
				return iter_inv(*this, size(), false);
			}

			//! Returns an iterator to the last unset bit.
			//! \return Reverse iterator to the last unset bit, or riend() when all bits are set.
			iter_rev_inv ribegin() const {
				return iter_rev_inv(*this, size(), false);
			}

			//! Returns the reverse unset-bit sentinel.
			//! \return Sentinel preceding the first unset bit.
			iter_rev_inv riend() const {
				return iter_rev_inv(*this, 0, true);
			}

			//! Tests a bit.
			//! \param pos Zero-based bit position, which must be less than size().
			//! \return True when the bit is set. False otherwise.
			GAIA_NODISCARD bool operator[](uint32_t pos) const {
				return test(pos);
			}

			//! Compares two bit sets for equality.
			//! \param other Bit set to compare with.
			//! \return True when all backing words are equal. False otherwise.
			GAIA_NODISCARD bool operator==(const dbitset& other) const {
				const uint32_t item_count = items();
				GAIA_FOR(item_count) {
					if (m_pData[i] != other.m_pData[i])
						return false;
				}
				return true;
			}

			//! Compares two bit sets for inequality.
			//! \param other Bit set to compare with.
			//! \return True when every compared backing word differs. False otherwise.
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

			//! Sets one bit, growing the bit set when needed.
			//! \param pos Zero-based bit position.
			//! \param value Value assigned to the bit.
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

			//! Flips one bit.
			//! \param pos Zero-based bit position, which must be less than size().
			void flip(uint32_t pos) {
				GAIA_ASSERT(pos < size());
				m_pData[pos / BitsPerItem] ^= ((size_type)1 << (pos % BitsPerItem));
			}

			//! Flips an inclusive range of bits.
			//! \param bitFrom First bit position to flip.
			//! \param bitTo Last bit position to flip, inclusive.
			//! \return This bit set.
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

			//! Unsets one bit.
			//! \param pos Zero-based bit position, which must be less than size().
			void reset(uint32_t pos) {
				GAIA_ASSERT(pos < size());
				m_pData[pos / BitsPerItem] &= ~((size_type)1 << (pos % BitsPerItem));
			}

			//! Returns the value of one bit.
			//! \param pos Zero-based bit position, which must be less than size().
			//! \return True when the bit is set. False otherwise.
			GAIA_NODISCARD bool test(uint32_t pos) const {
				GAIA_ASSERT(pos < size());
				return (m_pData[pos / BitsPerItem] & ((size_type)1 << (pos % BitsPerItem))) != 0;
			}

			//! Checks if all bits are set
			//! \return True when every bit is set. False otherwise.
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
			//! \return True when at least one bit is set. False otherwise.
			GAIA_NODISCARD bool any() const {
				const auto item_count = items();
				GAIA_FOR(item_count) {
					if (m_pData[i] != 0)
						return true;
				}
				return false;
			}

			//! Checks if all bits are reset
			//! \return True when no bit is set. False otherwise.
			GAIA_NODISCARD bool none() const {
				const auto item_count = items();
				GAIA_FOR(item_count) {
					if (m_pData[i] != 0)
						return false;
				}
				return true;
			}

			//! Returns the number of set bits
			//! \return Number of set bits.
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
			//! \return Current number of addressable bits.
			GAIA_NODISCARD constexpr uint32_t size() const {
				return m_cnt;
			}

			//! Returns the number of bits the dbitset can hold
			//! \return Current capacity in bits.
			GAIA_NODISCARD constexpr uint32_t capacity() const {
				return m_cap;
			}
		};
	} // namespace cnt
} // namespace gaia
