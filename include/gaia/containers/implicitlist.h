#pragma once
#include "../config/config.h"

#include <cinttypes>
#include <type_traits>

#include "darray.h"
#include "gaia/config/config_core.h"

namespace gaia {
	namespace containers {
		struct ImplicitListItem {
			//! For allocated entity: Index of entity within chunk.
			//! For deleted entity: Index of the next entity in the implicit list.
			uint32_t idx;
			//! Generation ID
			uint32_t gen;
		};

		template <typename TListItem, typename TItemHandle>
		struct ImplicitList {
			using internal_storage = darray<TListItem>;
			using iterator = typename internal_storage::iterator;
			using const_iterator = typename internal_storage::const_iterator;

			using iterator_category = typename internal_storage::iterator_category;
			using value_type = TListItem;
			using reference = TListItem&;
			using const_reference = const TListItem&;
			using pointer = TListItem*;
			using const_pointer = TListItem*;
			using difference_type = std::ptrdiff_t;
			using size_type = uint32_t;

			static_assert(std::is_base_of<ImplicitListItem, TListItem>::value);
			//! Implicit list items
			darray<TListItem> m_items;
			//! Index of the next item to recycle
			size_type m_nextFreeItem = (size_type)-1;
			//! Number of items to recycle
			size_type m_freeItems = 0;

			GAIA_NODISCARD pointer data() noexcept {
				return (pointer)m_items.data();
			}

			GAIA_NODISCARD const_pointer data() const noexcept {
				return (const_pointer)m_items.data();
			}

			GAIA_NODISCARD reference operator[](size_type index) {
				return m_items[index];
			}
			GAIA_NODISCARD const_reference operator[](size_type index) const {
				return m_items[index];
			}

			void clear() {
				m_items.clear();
				m_nextFreeItem = (size_type)-1;
				m_freeItems = 0;
			}

			GAIA_NODISCARD size_type get_next_free_item() const noexcept {
				return m_nextFreeItem;
			}

			GAIA_NODISCARD size_type get_free_items() const noexcept {
				return m_freeItems;
			}

			GAIA_NODISCARD size_type item_count() const noexcept {
				return size() - m_freeItems;
			}

			GAIA_NODISCARD size_type size() const noexcept {
				return (size_type)m_items.size();
			}

			GAIA_NODISCARD size_type capacity() const noexcept {
				return (size_type)m_items.capacity();
			}

			GAIA_NODISCARD bool empty() const noexcept {
				return size() == 0;
			}

			GAIA_NODISCARD iterator begin() const noexcept {
				return {(pointer)m_items.data()};
			}

			GAIA_NODISCARD const_iterator cbegin() const noexcept {
				return {(const_pointer)m_items.data()};
			}

			GAIA_NODISCARD iterator end() const noexcept {
				return {(pointer)m_items.data() + size()};
			}

			GAIA_NODISCARD const_iterator cend() const noexcept {
				return {(const_pointer)m_items.data() + size()};
			}

			//! Allocates a new item in the list
			//! \return Handle to the new item
			GAIA_NODISCARD TItemHandle allocate() {
				if GAIA_UNLIKELY (!m_freeItems) {
					// We don't want to go out of range for new item
					const auto itemCnt = (size_type)m_items.size();
					GAIA_ASSERT(itemCnt < TItemHandle::IdMask && "Trying to allocate too many items!");

					m_items.emplace_back(itemCnt, 0U);
					return {itemCnt, 0U};
				}

				// Make sure the list is not broken
				GAIA_ASSERT(m_nextFreeItem < (size_type)m_items.size() && "Item recycle list broken!");

				--m_freeItems;
				const auto index = m_nextFreeItem;
				auto& j = m_items[m_nextFreeItem];
				m_nextFreeItem = j.idx;
				return {index, m_items[index].gen};
			}

			//! Invalidates \param handle.
			//! Everytime an item is deallocated its generation is increased by one.
			TListItem& release(TItemHandle handle) {
				auto& item = m_items[handle.id()];

				// New generation
				const auto gen = ++item.gen;

				// Update our implicit list
				if GAIA_UNLIKELY (m_freeItems == 0) {
					m_nextFreeItem = handle.id();
					item.idx = TItemHandle::IdMask;
					item.gen = gen;
				} else {
					item.idx = m_nextFreeItem;
					item.gen = gen;
					m_nextFreeItem = handle.id();
				}
				++m_freeItems;

				return item;
			}

			//! Verifies that the implicit linked list is valid
			void validate() const {
				bool hasThingsToRemove = m_freeItems > 0;
				if (!hasThingsToRemove)
					return;

				// If there's something to remove there has to be at least one entity left
				GAIA_ASSERT(!m_items.empty());

				auto freeEntities = m_freeItems;
				auto nextFreeEntity = m_nextFreeItem;
				while (freeEntities > 0) {
					GAIA_ASSERT(nextFreeEntity < m_items.size() && "Item recycle list broken!");

					nextFreeEntity = m_items[nextFreeEntity].idx;
					--freeEntities;
				}

				// At this point the index of the last item in list should
				// point to -1 because that's the tail of our implicit list.
				GAIA_ASSERT(nextFreeEntity == TItemHandle::IdMask);
			}
		};
	} // namespace containers
} // namespace gaia