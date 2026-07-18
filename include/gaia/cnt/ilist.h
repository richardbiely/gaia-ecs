#pragma once
#include "gaia/config/config.h"

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "gaia/cnt/bitset.h"
#include "gaia/cnt/darray.h"
#include "gaia/core/utility.h"
#include "gaia/mem/mem_alloc.h"
#include "gaia/mem/raw_data_holder.h"

namespace gaia {
	namespace cnt {
		//! Access policy for implicit-list slot index and generation metadata.
		//! \tparam TListItem Implicit-list item type.
		template <typename TListItem>
		struct ilist_item_traits;

		//! \cond INTERNAL
		namespace detail {
			template <typename T, typename = void>
			struct ilist_has_idx_member: std::false_type {};
			template <typename T>
			struct ilist_has_idx_member<T, std::void_t<decltype(std::declval<T&>().idx)>>: std::true_type {};

			template <typename T, typename = void>
			struct ilist_has_gen_member: std::false_type {};
			template <typename T>
			struct ilist_has_gen_member<T, std::void_t<decltype(std::declval<T&>().gen)>>: std::true_type {};

			template <typename T, typename = void>
			struct ilist_has_data_gen_member: std::false_type {};
			template <typename T>
			struct ilist_has_data_gen_member<T, std::void_t<decltype(std::declval<T&>().data.gen)>>: std::true_type {};

			template <typename T, typename = void>
			struct ilist_has_id_mask: std::false_type {};
			template <typename T>
			struct ilist_has_id_mask<T, std::void_t<decltype(T::IdMask)>>: std::true_type {};

			template <typename T, typename = void>
			struct ilist_has_handle_id: std::false_type {};
			template <typename T>
			struct ilist_has_handle_id<T, std::void_t<decltype(std::declval<const T&>().id())>>: std::true_type {};

			template <typename T, typename = void>
			struct ilist_has_handle_gen: std::false_type {};
			template <typename T>
			struct ilist_has_handle_gen<T, std::void_t<decltype(std::declval<const T&>().gen())>>: std::true_type {};

			template <typename T, typename = void>
			struct ilist_has_create: std::false_type {};
			template <typename T>
			struct ilist_has_create<
					T, std::void_t<decltype(T::create(std::declval<uint32_t>(), std::declval<uint32_t>(), (void*)nullptr))>>:
					std::true_type {};

			template <typename T, typename THandle, typename = void>
			struct ilist_has_handle: std::false_type {};
			template <typename T, typename THandle>
			struct ilist_has_handle<T, THandle, std::void_t<decltype(T::handle(std::declval<const T&>()))>>:
					std::bool_constant<std::is_convertible_v<decltype(T::handle(std::declval<const T&>())), THandle>> {};

			template <typename T, typename = void>
			struct ilist_traits_has_idx: std::false_type {};
			template <typename T>
			struct ilist_traits_has_idx<T, std::void_t<decltype(ilist_item_traits<T>::idx(std::declval<const T&>()))>>:
					std::true_type {};

			template <typename T, typename = void>
			struct ilist_traits_has_set_idx: std::false_type {};
			template <typename T>
			struct ilist_traits_has_set_idx<
					T, std::void_t<decltype(ilist_item_traits<T>::set_idx(std::declval<T&>(), std::declval<uint32_t>()))>>:
					std::true_type {};

			template <typename T, typename = void>
			struct ilist_traits_has_gen: std::false_type {};
			template <typename T>
			struct ilist_traits_has_gen<T, std::void_t<decltype(ilist_item_traits<T>::gen(std::declval<const T&>()))>>:
					std::true_type {};

			template <typename T, typename = void>
			struct ilist_traits_has_set_gen: std::false_type {};
			template <typename T>
			struct ilist_traits_has_set_gen<
					T, std::void_t<decltype(ilist_item_traits<T>::set_gen(std::declval<T&>(), std::declval<uint32_t>()))>>:
					std::true_type {};
		} // namespace detail
		//! \endcond

		//! Default access policy for item types exposing idx and gen or data.gen members.
		//! \tparam TListItem Implicit-list item type.
		template <typename TListItem>
		struct ilist_item_traits {
			static_assert(
					detail::ilist_has_idx_member<TListItem>::value,
					"ilist item type must expose idx or specialize ilist_item_traits");
			static_assert(
					detail::ilist_has_gen_member<TListItem>::value || detail::ilist_has_data_gen_member<TListItem>::value,
					"ilist item type must expose gen/data.gen or specialize ilist_item_traits");

			//! Returns an item's slot index or free-list link.
			//! \param item Item whose index is read.
			//! \return Slot index for a live item or next free slot for a recycled item.
			GAIA_NODISCARD static uint32_t idx(const TListItem& item) noexcept {
				return (uint32_t)item.idx;
			}

			//! Sets an item's slot index or free-list link.
			//! \param item Item whose index is updated.
			//! \param value Slot index or next free slot.
			static void set_idx(TListItem& item, uint32_t value) noexcept {
				item.idx = value;
			}

			//! Returns an item's generation.
			//! \param item Item whose generation is read.
			//! \return Current generation value.
			GAIA_NODISCARD static uint32_t gen(const TListItem& item) noexcept {
				if constexpr (detail::ilist_has_gen_member<TListItem>::value)
					return (uint32_t)item.gen;
				else
					return (uint32_t)item.data.gen;
			}

			//! Sets an item's generation.
			//! \param item Item whose generation is updated.
			//! \param value New generation value.
			static void set_gen(TListItem& item, uint32_t value) noexcept {
				if constexpr (detail::ilist_has_gen_member<TListItem>::value)
					item.gen = value;
				else
					item.data.gen = value;
			}
		};

		//! Basic item metadata supported by ilist.
		struct ilist_item {
			//! Generation metadata associated with an item slot.
			struct ItemData {
				//! Generation ID
				uint32_t gen;
			};

			//! Allocated items: Index in the list.
			//! Deleted items: Index of the next deleted item in the list.
			uint32_t idx;
			//! Item data
			ItemData data;

			ilist_item() = default;
			//! Constructs item metadata for a slot.
			//! \param index Slot index.
			//! \param generation Slot generation.
			ilist_item(uint32_t index, uint32_t generation): idx(index) {
				data.gen = generation;
			}

			//! Copy-constructs item metadata.
			//! \param other Metadata to copy.
			ilist_item(const ilist_item& other) {
				idx = other.idx;
				data.gen = other.data.gen;
			}
			//! Copy-assigns item metadata.
			//! \param other Metadata to copy.
			//! \return This item metadata.
			ilist_item& operator=(const ilist_item& other) {
				GAIA_ASSERT(core::addressof(other) != this);
				idx = other.idx;
				data.gen = other.data.gen;
				return *this;
			}

			//! Move-constructs item metadata and invalidates the source metadata.
			//! \param other Metadata to move.
			ilist_item(ilist_item&& other) {
				idx = other.idx;
				data.gen = other.data.gen;

				other.idx = (uint32_t)-1;
				other.data.gen = (uint32_t)-1;
			}
			//! Move-assigns item metadata and invalidates the source metadata.
			//! \param other Metadata to move.
			//! \return This item metadata.
			ilist_item& operator=(ilist_item&& other) {
				GAIA_ASSERT(core::addressof(other) != this);
				idx = other.idx;
				data.gen = other.data.gen;

				other.idx = (uint32_t)-1;
				other.data.gen = (uint32_t)-1;
				return *this;
			}
		};

		//! Contiguous storage adapter used by ilist by default.
		//! \tparam TListItem Implicit-list item type.
		template <typename TListItem>
		struct darray_ilist_storage: public cnt::darray<TListItem> {
			//! Appends a live item.
			//! \param container Item to move into storage.
			void add_item(TListItem&& container) {
				this->push_back(GAIA_MOV(container));
			}

			//! Notifies the storage adapter that an item is being recycled.
			//! \param container Item being recycled. Contiguous storage keeps the slot in place.
			void del_item([[maybe_unused]] TListItem& container) {}
		};

		//! Implicit list. Rather than with pointers, items \tparam TListItem are linked
		//! together through an internal indexing mechanism. To the outside world they are
		//! presented as \tparam TItemHandle. All items are stored in a container instance
		//! of the type \tparam TInternalStorage.
		//! \tparam TListItem needs to expose slot metadata through ilist_item_traits<TListItem>
		//!         and expose a constructor that initializes the slot index and generation.
		template <typename TListItem, typename TItemHandle, typename TInternalStorage = darray_ilist_storage<TListItem>>
		struct ilist {
			//! Underlying slot storage type.
			using internal_storage = TInternalStorage;

			//! Stored item type.
			using value_type = TListItem;
			//! Mutable item reference.
			using reference = TListItem&;
			//! Immutable item reference.
			using const_reference = const TListItem&;
			//! Mutable item pointer.
			using pointer = TListItem*;
			//! Immutable item pointer.
			using const_pointer = const TListItem*;
			//! Underlying iterator distance type.
			using difference_type = typename internal_storage::difference_type;
			//! Underlying size and index type.
			using size_type = typename internal_storage::size_type;

			// TODO: replace this iterator with a real list iterator
			//! Mutable storage iterator.
			using iterator = typename internal_storage::iterator;
			//! Immutable storage iterator.
			using const_iterator = typename internal_storage::const_iterator;
			//! Underlying iterator category tag.
			using iterator_category = typename internal_storage::iterator_category;

			static_assert(detail::ilist_traits_has_idx<TListItem>::value, "ilist_item_traits<T> must expose idx(const T&)");
			static_assert(
					detail::ilist_traits_has_set_idx<TListItem>::value, "ilist_item_traits<T> must expose set_idx(T&, uint32_t)");
			static_assert(detail::ilist_traits_has_gen<TListItem>::value, "ilist_item_traits<T> must expose gen(const T&)");
			static_assert(
					detail::ilist_traits_has_set_gen<TListItem>::value, "ilist_item_traits<T> must expose set_gen(T&, uint32_t)");
			static_assert(
					detail::ilist_has_create<TListItem>::value,
					"ilist item type must expose static create(index, generation, ctx)");
			static_assert(
					detail::ilist_has_handle<TListItem, TItemHandle>::value,
					"ilist item type must expose static handle(const item) returning the handle type");
			static_assert(detail::ilist_has_id_mask<TItemHandle>::value, "ilist handle type must expose IdMask");
			static_assert(detail::ilist_has_handle_id<TItemHandle>::value, "ilist handle type must expose id()");
			static_assert(detail::ilist_has_handle_gen<TItemHandle>::value, "ilist handle type must expose gen()");
			//! Implicit list items
			internal_storage m_items;
			//! Index of the next item to recycle
			size_type m_nextFreeIdx = (size_type)-1;
			//! Number of items to recycle
			size_type m_freeItems = 0;

			//! Returns contiguous item storage.
			//! \return Pointer to the first slot.
			GAIA_NODISCARD pointer data() noexcept {
				return reinterpret_cast<pointer>(m_items.data());
			}

			//! Returns contiguous item storage.
			//! \return Const pointer to the first slot.
			GAIA_NODISCARD const_pointer data() const noexcept {
				return reinterpret_cast<const_pointer>(m_items.data());
			}

			//! Returns an item slot by index.
			//! \param index Slot index.
			//! \return Mutable reference to the slot.
			GAIA_NODISCARD reference operator[](size_type index) {
				return m_items[index];
			}
			//! Returns an item slot by index.
			//! \param index Slot index.
			//! \return Immutable reference to the slot.
			GAIA_NODISCARD const_reference operator[](size_type index) const {
				return m_items[index];
			}

			//! Removes all slots and resets the free list.
			void clear() {
				m_items.clear();
				m_nextFreeIdx = (size_type)-1;
				m_freeItems = 0;
			}

			//! Returns the free-list head.
			//! \return Index of the next recyclable slot, or the handle type's invalid id.
			GAIA_NODISCARD size_type get_next_free_item() const noexcept {
				return m_nextFreeIdx;
			}

			//! Returns the number of recyclable slots.
			//! \return Number of slots linked through the free list.
			GAIA_NODISCARD size_type get_free_items() const noexcept {
				return m_freeItems;
			}

			//! Returns the number of live items.
			//! \return Total slot count minus recyclable slot count.
			GAIA_NODISCARD size_type item_count() const noexcept {
				return size() - m_freeItems;
			}

			//! Returns the total number of allocated slots.
			//! \return Live and recyclable slot count.
			GAIA_NODISCARD size_type size() const noexcept {
				return (size_type)m_items.size();
			}

			//! Checks whether no slots have been allocated.
			//! \return True when size() is zero. False otherwise.
			GAIA_NODISCARD bool empty() const noexcept {
				return size() == 0;
			}

			//! Returns slot capacity.
			//! \return Number of slots available without growing storage.
			GAIA_NODISCARD size_type capacity() const noexcept {
				return (size_type)m_items.capacity();
			}

			//! Returns a mutable iterator to the first storage slot.
			//! \return Mutable iterator to the first slot.
			GAIA_NODISCARD iterator begin() noexcept {
				return m_items.begin();
			}

			//! Returns an immutable iterator to the first storage slot.
			//! \return Immutable iterator to the first slot.
			GAIA_NODISCARD const_iterator begin() const noexcept {
				return m_items.begin();
			}

			//! Returns an immutable iterator to the first storage slot.
			//! \return Immutable iterator to the first slot.
			GAIA_NODISCARD const_iterator cbegin() const noexcept {
				return m_items.begin();
			}

			//! Returns the mutable storage end sentinel.
			//! \return Mutable iterator following the last slot.
			GAIA_NODISCARD iterator end() noexcept {
				return m_items.end();
			}

			//! Returns the immutable storage end sentinel.
			//! \return Immutable iterator following the last slot.
			GAIA_NODISCARD const_iterator end() const noexcept {
				return m_items.end();
			}

			//! Returns the immutable storage end sentinel.
			//! \return Immutable iterator following the last slot.
			GAIA_NODISCARD const_iterator cend() const noexcept {
				return m_items.end();
			}

			//! Reserves storage for slots.
			//! \param cap Minimum requested slot capacity.
			void reserve(size_type cap) {
				m_items.reserve(cap);
			}

			//! Allocates a new item in the list
			//! \param ctx Context forwarded to TListItem::create().
			//! \return Handle to the new item
			GAIA_NODISCARD TItemHandle alloc(void* ctx) {
				if GAIA_UNLIKELY (m_freeItems == 0U) {
					// We don't want to go out of range for new item
					const auto itemCnt = (size_type)m_items.size();
					GAIA_ASSERT(itemCnt < TItemHandle::IdMask && "Trying to allocate too many items!");

					GAIA_GCC_WARNING_PUSH()
					GAIA_CLANG_WARNING_PUSH()
					GAIA_GCC_WARNING_DISABLE("-Wstringop-overflow");
					GAIA_GCC_WARNING_DISABLE("-Wmissing-field-initializers");
					GAIA_CLANG_WARNING_DISABLE("-Wmissing-field-initializers");
					m_items.add_item(TListItem::create(itemCnt, 0U, ctx));
					return TListItem::handle(m_items.back());
					GAIA_GCC_WARNING_POP()
					GAIA_CLANG_WARNING_POP()
				}

				// Make sure the list is not broken
				GAIA_ASSERT(m_nextFreeIdx < (size_type)m_items.size() && "Item recycle list broken!");

				--m_freeItems;
				const auto index = m_nextFreeIdx;
				auto& j = m_items[m_nextFreeIdx];
				m_nextFreeIdx = ilist_item_traits<TListItem>::idx(j);
				j = TListItem::create(index, ilist_item_traits<TListItem>::gen(j), ctx);
				return TListItem::handle(j);
			}

			//! Allocates a new item in the list
			//! \return Handle to the new item
			GAIA_NODISCARD TItemHandle alloc() {
				if GAIA_UNLIKELY (m_freeItems == 0U) {
					// We don't want to go out of range for new item
					const auto itemCnt = (size_type)m_items.size();
					GAIA_ASSERT(itemCnt < TItemHandle::IdMask && "Trying to allocate too many items!");

					GAIA_GCC_WARNING_PUSH()
					GAIA_CLANG_WARNING_PUSH()
					GAIA_GCC_WARNING_DISABLE("-Wstringop-overflow");
					GAIA_GCC_WARNING_DISABLE("-Wmissing-field-initializers");
					GAIA_CLANG_WARNING_DISABLE("-Wmissing-field-initializers");
					m_items.add_item(TListItem(itemCnt, 0U));
					return {itemCnt, 0U};
					GAIA_GCC_WARNING_POP()
					GAIA_CLANG_WARNING_POP()
				}

				// Make sure the list is not broken
				GAIA_ASSERT(m_nextFreeIdx < (size_type)m_items.size() && "Item recycle list broken!");

				--m_freeItems;
				const auto index = m_nextFreeIdx;
				auto& j = m_items[m_nextFreeIdx];
				m_nextFreeIdx = ilist_item_traits<TListItem>::idx(j);
				return {index, ilist_item_traits<TListItem>::gen(m_items[index])};
			}

			//! Invalidates \a handle.
			//! Every time an item is deallocated its generation is increased by one.
			//! \param handle Handle
			//! \return Reference to the recycled item slot.
			TListItem& free(TItemHandle handle) {
				auto& item = m_items[handle.id()];
				m_items.del_item(item);

				// Update our implicit list
				if GAIA_UNLIKELY (m_freeItems == 0)
					ilist_item_traits<TListItem>::set_idx(item, TItemHandle::IdMask);
				else
					ilist_item_traits<TListItem>::set_idx(item, m_nextFreeIdx);
				ilist_item_traits<TListItem>::set_gen(item, ilist_item_traits<TListItem>::gen(item) + 1);

				m_nextFreeIdx = handle.id();
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

				auto freeItems = m_freeItems;
				auto nextFreeItem = m_nextFreeIdx;
				while (freeItems > 0) {
					GAIA_ASSERT(nextFreeItem < m_items.size() && "Item recycle list broken!");

					nextFreeItem = ilist_item_traits<TListItem>::idx(m_items[nextFreeItem]);
					--freeItems;
				}

				// At this point the index of the last item in list should
				// point to -1 because that's the tail of our implicit list.
				GAIA_ASSERT(nextFreeItem == TItemHandle::IdMask);
			}
		};

		//! Rebuild policy for implicit-list handles after generation changes.
		//! \tparam TItemHandle External handle type.
		//! The second template argument is reserved for specialization detection.
		template <typename TItemHandle, typename = void>
		struct ilist_handle_traits {
			//! Rebuilds a handle after its generation changes.
			//! \param id Slot index stored in the handle.
			//! \param gen New slot generation.
			//! \param prev Previous handle value. Unused for simple id/generation handles.
			//! \return Rebuilt handle preserving the handle type's supported metadata.
			static TItemHandle make(uint32_t id, uint32_t gen, const TItemHandle& prev) {
				(void)prev;
				return TItemHandle(id, gen);
			}
		};

		//! Handle rebuild policy preserving priority metadata exposed by prio().
		//! \tparam TItemHandle External handle type with priority metadata.
		template <typename TItemHandle>
		struct ilist_handle_traits<TItemHandle, std::void_t<decltype(std::declval<const TItemHandle&>().prio())>> {
			//! Rebuilds a handle after its generation changes while preserving packed priority bits.
			//! \param id Slot index stored in the handle.
			//! \param gen New slot generation.
			//! \param prev Previous handle value whose priority bit must be preserved.
			//! \return Rebuilt handle with the same priority metadata as \a prev.
			static TItemHandle make(uint32_t id, uint32_t gen, const TItemHandle& prev) {
				return TItemHandle(id, gen, prev.prio());
			}
		};

		//! Paged implicit list declaration.
		//! \tparam TListItem Payload type stored in the list.
		//! \tparam TItemHandle External handle type used to address slots.
		//! \tparam MaxPages Maximum number of addressable pages. A value of 0 keeps the
		//!         page table dynamic. A non-zero value embeds a fixed page table in the
		//!         container so page-table storage never reallocates after construction.
		template <typename TListItem, typename TItemHandle, uint32_t MaxPages = 0>
		struct paged_ilist;

		//! Forward iterator used by paged_ilist.
		//! Kept outside the container template so the container body stays smaller to compile.
		//! \tparam TPagedIList Owning paged_ilist type.
		//! \tparam IsConst Whether the iterator yields const references.
		template <typename TPagedIList, bool IsConst>
		class paged_ilist_iterator {
			using owner_type = std::conditional_t<IsConst, const TPagedIList, TPagedIList>;
			using owner_pointer = owner_type*;

			owner_pointer m_pOwner = nullptr;
			typename TPagedIList::size_type m_index = 0;

			void skip_dead() {
				while (m_pOwner != nullptr && m_index < m_pOwner->size() && !m_pOwner->has(m_index))
					++m_index;
			}

		public:
			//! Stored payload type.
			using value_type = typename TPagedIList::value_type;
			//! Reference type selected from iterator constness.
			using reference =
					std::conditional_t<IsConst, typename TPagedIList::const_reference, typename TPagedIList::reference>;
			//! Pointer type selected from iterator constness.
			using pointer = std::conditional_t<IsConst, typename TPagedIList::const_pointer, typename TPagedIList::pointer>;
			//! Type used for iterator distances.
			using difference_type = typename TPagedIList::difference_type;
			//! Iterator category tag.
			using iterator_category = core::forward_iterator_tag;

			paged_ilist_iterator() = default;

			//! \param pOwner Owning paged list.
			//! \param index Initial slot index.
			paged_ilist_iterator(owner_pointer pOwner, typename TPagedIList::size_type index):
					m_pOwner(pOwner), m_index(index) {
				skip_dead();
			}

			//! Dereferences the current live payload.
			//! \return Reference to the current payload.
			GAIA_NODISCARD reference operator*() const {
				return (*m_pOwner)[m_index];
			}

			//! Accesses the current live payload.
			//! \return Pointer to the current payload.
			GAIA_NODISCARD pointer operator->() const {
				return &(*m_pOwner)[m_index];
			}

			//! Advances to the next live payload.
			//! \return This iterator after advancement.
			paged_ilist_iterator& operator++() {
				++m_index;
				skip_dead();
				return *this;
			}

			//! Advances to the next live payload.
			//! \return Iterator value before advancement.
			paged_ilist_iterator operator++(int) {
				auto tmp = *this;
				++(*this);
				return tmp;
			}

			//! Compares iterator positions and owners.
			//! \param other Iterator to compare with.
			//! \return True when both iterators have the same owner and slot index.
			GAIA_NODISCARD bool operator==(const paged_ilist_iterator& other) const {
				return m_pOwner == other.m_pOwner && m_index == other.m_index;
			}

			//! Compares iterator positions and owners.
			//! \param other Iterator to compare with.
			//! \return True when the iterators differ.
			GAIA_NODISCARD bool operator!=(const paged_ilist_iterator& other) const {
				return !(*this == other);
			}
		};

		//! Paged implicit list with page-local slot metadata and lazily allocated page payloads.
		//! Live slots own payload objects. Dead slots keep only handle and free-list metadata, which
		//! allows payload storage for fully empty pages to be released.
		//! \tparam TListItem Payload type stored in the list. Must expose slot metadata
		//!         through ilist_item_traits<TListItem> and ilist-compatible create()/handle() helpers.
		//! \tparam TItemHandle External handle type exposing id(), gen(), and IdMask.
		//! \tparam MaxPages Maximum number of page pointers kept by the container. Use 0 for
		//!         dynamic growth through darray. Use a non-zero value when the maximum slot
		//!         count is known and pointer-table relocation must be impossible.
		template <typename TListItem, typename TItemHandle, uint32_t MaxPages>
		struct paged_ilist {
			//! Stored payload type.
			using value_type = TListItem;
			//! Mutable payload reference.
			using reference = TListItem&;
			//! Immutable payload reference.
			using const_reference = const TListItem&;
			//! Mutable payload pointer.
			using pointer = TListItem*;
			//! Immutable payload pointer.
			using const_pointer = const TListItem*;
			//! Type used for iterator distances.
			using difference_type = std::ptrdiff_t;
			//! Type used for slot indices and sizes.
			using size_type = uint32_t;
			//! Iterator category tag.
			using iterator_category = core::forward_iterator_tag;

			static_assert(detail::ilist_traits_has_idx<TListItem>::value, "ilist_item_traits<T> must expose idx(const T&)");
			static_assert(
					detail::ilist_traits_has_set_idx<TListItem>::value, "ilist_item_traits<T> must expose set_idx(T&, uint32_t)");
			static_assert(detail::ilist_traits_has_gen<TListItem>::value, "ilist_item_traits<T> must expose gen(const T&)");
			static_assert(
					detail::ilist_traits_has_set_gen<TListItem>::value, "ilist_item_traits<T> must expose set_gen(T&, uint32_t)");
			static_assert(
					detail::ilist_has_create<TListItem>::value,
					"paged_ilist item type must expose static create(index, generation, ctx)");
			static_assert(
					detail::ilist_has_handle<TListItem, TItemHandle>::value,
					"paged_ilist item type must expose static handle(const item) returning the handle type");
			static_assert(detail::ilist_has_id_mask<TItemHandle>::value, "paged_ilist handle type must expose IdMask");
			static_assert(detail::ilist_has_handle_id<TItemHandle>::value, "paged_ilist handle type must expose id()");
			static_assert(detail::ilist_has_handle_gen<TItemHandle>::value, "paged_ilist handle type must expose gen()");

		private:
			static constexpr size_type target_payload_bytes() noexcept {
				return 16384;
			}

			static constexpr size_type min_page_capacity() noexcept {
				return 8;
			}

			static constexpr size_type max_page_capacity() noexcept {
				return 256;
			}

			static constexpr size_type calc_page_capacity() noexcept {
				const size_type payloadItemBytes = sizeof(TListItem) == 0 ? 1U : (size_type)sizeof(TListItem);
				const size_type desired = target_payload_bytes() / payloadItemBytes;
				if (desired < min_page_capacity())
					return min_page_capacity();
				if (desired > max_page_capacity())
					return max_page_capacity();
				return desired;
			}

			static constexpr size_type PageCapacity = calc_page_capacity();
			static constexpr bool FixedPageTable = MaxPages != 0;
			static constexpr size_type StaticPageCount = MaxPages == 0 ? 1U : MaxPages;

			struct page_type {
				using alive_mask_type = cnt::bitset<PageCapacity>;
				using storage_type = mem::raw_data_holder<TListItem, sizeof(TListItem) * PageCapacity>;

				alive_mask_type aliveMask;
				TItemHandle handles[PageCapacity]{};
				uint32_t nextFree[PageCapacity];
				uint32_t liveCount = 0;
				storage_type* pStorage = nullptr;

				page_type() {
					GAIA_FOR(PageCapacity)
					nextFree[i] = TItemHandle::IdMask;
				}

				~page_type() {
					clear();
				}

				page_type(const page_type&) = delete;
				page_type& operator=(const page_type&) = delete;

				page_type(page_type&& other) noexcept:
						aliveMask(other.aliveMask), liveCount(other.liveCount), pStorage(other.pStorage) {
					GAIA_FOR(PageCapacity) {
						handles[i] = other.handles[i];
						nextFree[i] = other.nextFree[i];
					}

					other.aliveMask.reset();
					other.liveCount = 0;
					other.pStorage = nullptr;
				}

				page_type& operator=(page_type&& other) noexcept {
					GAIA_ASSERT(core::addressof(other) != this);

					clear();
					aliveMask = other.aliveMask;
					liveCount = other.liveCount;
					pStorage = other.pStorage;
					GAIA_FOR(PageCapacity) {
						handles[i] = other.handles[i];
						nextFree[i] = other.nextFree[i];
					}

					other.aliveMask.reset();
					other.liveCount = 0;
					other.pStorage = nullptr;
					return *this;
				}

				GAIA_NODISCARD pointer data() noexcept {
					return pStorage == nullptr ? nullptr : reinterpret_cast<pointer>((uint8_t*)*pStorage);
				}

				GAIA_NODISCARD const_pointer data() const noexcept {
					return pStorage == nullptr ? nullptr : reinterpret_cast<const_pointer>((const uint8_t*)*pStorage);
				}

				GAIA_NODISCARD pointer ptr(size_type slot) noexcept {
					GAIA_ASSERT(slot < PageCapacity);
					GAIA_ASSERT(pStorage != nullptr);
					return data() + slot;
				}

				GAIA_NODISCARD const_pointer ptr(size_type slot) const noexcept {
					GAIA_ASSERT(slot < PageCapacity);
					GAIA_ASSERT(pStorage != nullptr);
					return data() + slot;
				}

				void ensure_storage() {
					if GAIA_LIKELY (pStorage != nullptr)
						return;

					constexpr auto StorageAlignment =
							alignof(storage_type) < sizeof(void*) ? sizeof(void*) : alignof(storage_type);
					pStorage = mem::AllocHelper::alloc_alig<storage_type>("PagedIListPage", StorageAlignment);
				}

				void maybe_release_storage() {
					if (liveCount != 0 || pStorage == nullptr)
						return;

					mem::AllocHelper::free_alig("PagedIListPage", pStorage);
					pStorage = nullptr;
				}

				void clear() {
					if (pStorage != nullptr) {
						for (auto slot: aliveMask)
							core::call_dtor(ptr(slot));
						mem::AllocHelper::free_alig("PagedIListPage", pStorage);
						pStorage = nullptr;
					}

					aliveMask.reset();
					liveCount = 0;
					GAIA_FOR(PageCapacity)
					nextFree[i] = TItemHandle::IdMask;
				}

				template <typename... Args>
				void construct(size_type slot, Args&&... args) {
					GAIA_ASSERT(slot < PageCapacity);
					ensure_storage();
					core::call_ctor(ptr(slot), GAIA_FWD(args)...);
					aliveMask.set(slot);
					++liveCount;
				}

				void destroy(size_type slot) {
					GAIA_ASSERT(slot < PageCapacity);
					GAIA_ASSERT(aliveMask.test(slot));
					core::call_dtor(ptr(slot));
					aliveMask.reset(slot);
					GAIA_ASSERT(liveCount > 0);
					--liveCount;
					maybe_release_storage();
				}
			};

			//! Dynamic page pointer table used when MaxPages is 0.
			cnt::darray<page_type*> m_pages;
			//! Fixed page pointer table used when MaxPages is non-zero.
			//! Page payloads are still allocated lazily. Only the pointer table is fixed.
			page_type* m_staticPages[StaticPageCount]{};
			size_type m_size = 0;

		public:
			//! Head of the implicit free-list, or TItemHandle::IdMask when no slots are free.
			size_type m_nextFreeIdx = (size_type)-1;
			//! Number of slots currently linked through the implicit free-list.
			size_type m_freeItems = 0;

		private:
			GAIA_NODISCARD static constexpr size_type page_index(size_type index) noexcept {
				return index / PageCapacity;
			}

			GAIA_NODISCARD static constexpr size_type slot_index(size_type index) noexcept {
				return index % PageCapacity;
			}

			GAIA_NODISCARD static constexpr size_type page_count_for_slots(size_type slotCnt) noexcept {
				return slotCnt == 0 ? 0U : (slotCnt + PageCapacity - 1) / PageCapacity;
			}

		public:
			//! Returns the compile-time number of payload slots stored in one page.
			//! \return Number of slots per page.
			GAIA_NODISCARD static constexpr size_type page_capacity() noexcept {
				return PageCapacity;
			}

			//! Calculates how many pages are needed to address \a slotCnt slots.
			//! \param slotCnt Number of slots that must be addressable.
			//! \return Number of pages required for \a slotCnt.
			GAIA_NODISCARD static constexpr size_type page_count_for_capacity(size_type slotCnt) noexcept {
				return page_count_for_slots(slotCnt);
			}

		private:
			GAIA_NODISCARD size_type page_table_size() const noexcept {
				if constexpr (FixedPageTable)
					return MaxPages;
				else
					return (size_type)m_pages.size();
			}

			GAIA_NODISCARD page_type*& page_slot(size_type pageIdx) noexcept {
				if constexpr (FixedPageTable) {
					GAIA_ASSERT(pageIdx < MaxPages);
					return m_staticPages[pageIdx];
				} else {
					GAIA_ASSERT(pageIdx < (size_type)m_pages.size());
					return m_pages[pageIdx];
				}
			}

			GAIA_NODISCARD const page_type* page_slot(size_type pageIdx) const noexcept {
				if constexpr (FixedPageTable) {
					GAIA_ASSERT(pageIdx < MaxPages);
					return m_staticPages[pageIdx];
				} else {
					GAIA_ASSERT(pageIdx < (size_type)m_pages.size());
					return m_pages[pageIdx];
				}
			}

			void clear_pages() {
				const auto pageCnt = page_table_size();
				GAIA_FOR(pageCnt) {
					auto*& pPage = page_slot(i);
					if (pPage == nullptr)
						continue;
					delete pPage;
					pPage = nullptr;
				}

				if constexpr (!FixedPageTable)
					m_pages.clear();
			}

			void ensure_page_count(size_type slotCnt) {
				const auto pageCnt = page_count_for_slots(slotCnt);
				if constexpr (FixedPageTable) {
					GAIA_ASSERT(pageCnt <= MaxPages && "Trying to allocate too many paged_ilist pages!");
				} else {
					if (pageCnt > (size_type)m_pages.size())
						m_pages.resize(pageCnt, nullptr);
				}
			}

			GAIA_NODISCARD page_type& ensure_page(size_type index) {
				ensure_page_count(index + 1);
				auto*& pPage = page_slot(page_index(index));
				if (pPage == nullptr)
					pPage = new page_type();
				return *pPage;
			}

			GAIA_NODISCARD page_type* try_page(size_type index) noexcept {
				const auto pageIdx = page_index(index);
				if (pageIdx >= page_table_size())
					return nullptr;
				return page_slot(pageIdx);
			}

			GAIA_NODISCARD const page_type* try_page(size_type index) const noexcept {
				const auto pageIdx = page_index(index);
				if (pageIdx >= page_table_size())
					return nullptr;
				return page_slot(pageIdx);
			}

			GAIA_NODISCARD reference slot_ref(size_type index) {
				auto* pPage = try_page(index);
				GAIA_ASSERT(pPage != nullptr);
				return *pPage->ptr(slot_index(index));
			}

			GAIA_NODISCARD const_reference slot_ref(size_type index) const {
				auto* pPage = try_page(index);
				GAIA_ASSERT(pPage != nullptr);
				return *pPage->ptr(slot_index(index));
			}

		public:
			//! Mutable forward iterator over live payloads.
			using iterator = paged_ilist_iterator<paged_ilist, false>;
			//! Immutable forward iterator over live payloads.
			using const_iterator = paged_ilist_iterator<paged_ilist, true>;

			~paged_ilist() {
				clear_pages();
			}

			paged_ilist() = default;
			paged_ilist(const paged_ilist&) = delete;
			paged_ilist& operator=(const paged_ilist&) = delete;
			//! Move-constructs a paged list and leaves the source empty.
			//! \param other Paged list whose pages are transferred.
			paged_ilist(paged_ilist&& other) noexcept:
					m_size(other.m_size), m_nextFreeIdx(other.m_nextFreeIdx), m_freeItems(other.m_freeItems) {
				if constexpr (FixedPageTable) {
					GAIA_FOR(MaxPages) {
						m_staticPages[i] = other.m_staticPages[i];
						other.m_staticPages[i] = nullptr;
					}
				} else {
					m_pages = GAIA_MOV(other.m_pages);
				}

				other.m_size = 0;
				other.m_nextFreeIdx = (size_type)-1;
				other.m_freeItems = 0;
			}
			//! Move-assigns a paged list and leaves the source empty.
			//! \param other Paged list whose pages are transferred.
			//! \return This paged list.
			paged_ilist& operator=(paged_ilist&& other) noexcept {
				GAIA_ASSERT(core::addressof(other) != this);
				clear_pages();

				if constexpr (FixedPageTable) {
					GAIA_FOR(MaxPages) {
						m_staticPages[i] = other.m_staticPages[i];
						other.m_staticPages[i] = nullptr;
					}
				} else {
					m_pages = GAIA_MOV(other.m_pages);
				}

				m_size = other.m_size;
				m_nextFreeIdx = other.m_nextFreeIdx;
				m_freeItems = other.m_freeItems;

				other.m_size = 0;
				other.m_nextFreeIdx = (size_type)-1;
				other.m_freeItems = 0;
				return *this;
			}

			//! Reports that paged storage is not globally contiguous.
			//! \return Always nullptr. Access payloads by slot or iterator instead.
			GAIA_NODISCARD pointer data() noexcept {
				return nullptr;
			}

			//! Reports that paged storage is not globally contiguous.
			//! \return Always nullptr. Access payloads by slot or iterator instead.
			GAIA_NODISCARD const_pointer data() const noexcept {
				return nullptr;
			}

			//! Checks whether a slot contains a live payload.
			//! \param index Slot index to inspect.
			//! \return True when index identifies a live payload. False otherwise.
			GAIA_NODISCARD bool has(size_type index) const noexcept {
				if (index >= m_size)
					return false;

				const auto* pPage = try_page(index);
				return pPage != nullptr && pPage->aliveMask.test(slot_index(index));
			}

			//! Checks whether a handle identifies its current live payload.
			//! \param handle Handle to validate.
			//! \return True when the slot is live and its stored handle equals handle.
			GAIA_NODISCARD bool has(TItemHandle handle) const noexcept {
				return has(handle.id()) && this->handle(handle.id()) == handle;
			}

			//! Returns the handle metadata stored for a slot.
			//! \param index Valid slot index.
			//! \return Stored handle.
			GAIA_NODISCARD TItemHandle handle(size_type index) const noexcept {
				GAIA_ASSERT(index < m_size);
				const auto* pPage = try_page(index);
				GAIA_ASSERT(pPage != nullptr);
				return pPage->handles[slot_index(index)];
			}

			//! Returns a slot's generation.
			//! \param index Valid slot index.
			//! \return Generation encoded in the stored handle.
			GAIA_NODISCARD uint32_t generation(size_type index) const noexcept {
				return handle(index).gen();
			}

			//! Returns the free-list link stored for a slot.
			//! \param index Valid slot index.
			//! \return Next free slot index or the handle type's invalid id.
			GAIA_NODISCARD uint32_t next_free(size_type index) const noexcept {
				GAIA_ASSERT(index < m_size);
				const auto* pPage = try_page(index);
				GAIA_ASSERT(pPage != nullptr);
				return pPage->nextFree[slot_index(index)];
			}

			//! Returns a live payload by slot index.
			//! \param index Live slot index.
			//! \return Mutable reference to the payload.
			GAIA_NODISCARD reference operator[](size_type index) {
				GAIA_ASSERT(has(index));
				return slot_ref(index);
			}

			//! Returns a live payload by slot index.
			//! \param index Live slot index.
			//! \return Immutable reference to the payload.
			GAIA_NODISCARD const_reference operator[](size_type index) const {
				GAIA_ASSERT(has(index));
				return slot_ref(index);
			}

			//! Destroys all live payloads, releases all pages, and resets slot metadata.
			void clear() {
				clear_pages();
				m_size = 0;
				m_nextFreeIdx = (size_type)-1;
				m_freeItems = 0;
			}

			//! Returns the free-list head.
			//! \return Index of the next recyclable slot, or the handle type's invalid id.
			GAIA_NODISCARD size_type get_next_free_item() const noexcept {
				return m_nextFreeIdx;
			}

			//! Returns the number of recyclable slots.
			//! \return Number of slots linked through the free list.
			GAIA_NODISCARD size_type get_free_items() const noexcept {
				return m_freeItems;
			}

			//! Returns the number of live payloads.
			//! \return Total slot count minus recyclable slot count.
			GAIA_NODISCARD size_type item_count() const noexcept {
				return m_size - m_freeItems;
			}

			//! Returns the total number of addressable slots in use.
			//! \return Live and recyclable slot count.
			GAIA_NODISCARD size_type size() const noexcept {
				return m_size;
			}

			//! Checks whether no slots are in use.
			//! \return True when size() is zero. False otherwise.
			GAIA_NODISCARD bool empty() const noexcept {
				return m_size == 0;
			}

			//! Returns the slot capacity represented by the page table.
			//! \return Maximum addressable slots without growing the page table.
			GAIA_NODISCARD size_type capacity() const noexcept {
				if constexpr (FixedPageTable)
					return MaxPages * PageCapacity;
				else
					return (size_type)m_pages.capacity() * PageCapacity;
			}

			//! Returns an iterator over live payload objects only.
			//! \return Mutable iterator to the first live payload.
			GAIA_NODISCARD iterator begin() noexcept {
				return iterator(this, 0);
			}

			//! Returns an iterator over live payload objects only.
			//! \return Immutable iterator to the first live payload.
			GAIA_NODISCARD const_iterator begin() const noexcept {
				return const_iterator(this, 0);
			}

			//! Returns an iterator over live payload objects only.
			//! \return Immutable iterator to the first live payload.
			GAIA_NODISCARD const_iterator cbegin() const noexcept {
				return const_iterator(this, 0);
			}

			//! Returns the mutable end sentinel.
			//! \return Iterator following the last slot.
			GAIA_NODISCARD iterator end() noexcept {
				return iterator(this, m_size);
			}

			//! Returns the immutable end sentinel.
			//! \return Iterator following the last slot.
			GAIA_NODISCARD const_iterator end() const noexcept {
				return const_iterator(this, m_size);
			}

			//! Returns the immutable end sentinel.
			//! \return Iterator following the last slot.
			GAIA_NODISCARD const_iterator cend() const noexcept {
				return const_iterator(this, m_size);
			}

			//! Reserves page-table capacity for at least \a cap slots.
			//! \param cap Number of slots that should be addressable without growing the page table.
			//! \note In fixed-page-table mode this only verifies that \a cap fits into MaxPages.
			//!       Payload pages remain lazily allocated in both modes.
			void reserve(size_type cap) {
				const auto pageCnt = page_count_for_slots(cap);
				if constexpr (FixedPageTable) {
					GAIA_ASSERT(pageCnt <= MaxPages);
				} else {
					m_pages.reserve(pageCnt);
				}
			}

			//! Ensures the page pointer table can address \a cap slots without resizing later.
			//! \param cap Number of slots that must be addressable.
			//! \note This is stronger than reserve() in dynamic mode because it resizes the pointer
			//!       table to contain null page entries. It does not allocate payload pages.
			//! \note In fixed-page-table mode this only verifies that \a cap fits into MaxPages.
			void reserve_slot_table(size_type cap) {
				const auto pageCnt = page_count_for_slots(cap);
				if constexpr (FixedPageTable) {
					GAIA_ASSERT(pageCnt <= MaxPages);
				} else {
					ensure_page_count(cap);
				}
			}

			//! Returns a live payload slot without consulting list-wide size metadata.
			//! \param index Slot index to access.
			//! \return Mutable reference to the live payload at \a index.
			//! \warning This bypasses index < size() checks. Use only when the caller already
			//!          validated the handle/index through stronger external synchronization.
			GAIA_NODISCARD reference live_unsafe(size_type index) {
				auto* pPage = try_page(index);
				GAIA_ASSERT(pPage != nullptr);
				const auto slot = slot_index(index);
				GAIA_ASSERT(pPage->aliveMask.test(slot));
				return *pPage->ptr(slot);
			}

			//! Returns a live payload slot without consulting list-wide size metadata.
			//! \param index Slot index to access.
			//! \return Immutable reference to the live payload at \a index.
			//! \warning This bypasses index < size() checks. Use only when the caller already
			//!          validated the handle/index through stronger external synchronization.
			GAIA_NODISCARD const_reference live_unsafe(size_type index) const {
				const auto* pPage = try_page(index);
				GAIA_ASSERT(pPage != nullptr);
				const auto slot = slot_index(index);
				GAIA_ASSERT(pPage->aliveMask.test(slot));
				return *pPage->ptr(slot);
			}

			//! Attempts to access a live payload.
			//! \param index Slot index to inspect.
			//! \return Pointer to the live payload, or nullptr when the slot is not live.
			GAIA_NODISCARD pointer try_get(size_type index) noexcept {
				if (!has(index))
					return nullptr;
				return &slot_ref(index);
			}

			//! Attempts to access a live payload.
			//! \param index Slot index to inspect.
			//! \return Const pointer to the live payload, or nullptr when the slot is not live.
			GAIA_NODISCARD const_pointer try_get(size_type index) const noexcept {
				if (!has(index))
					return nullptr;
				return &slot_ref(index);
			}

			//! Restores a live slot with a preassigned id/generation.
			//! \param item Payload carrying the slot index and generation to restore.
			//! \note Existing live payload at the same slot is destroyed first. Existing free-list
			//!       metadata for that slot is cleared because the restored slot becomes live.
			void add_live(TListItem&& item) {
				const auto index = (size_type)ilist_item_traits<TListItem>::idx(item);
				auto& page = ensure_page(index);
				const auto slot = slot_index(index);
				const bool existed = index < m_size;
				const bool wasAlive = existed && page.aliveMask.test(slot);
				const bool wasFree = existed && !wasAlive;

				if (index >= m_size)
					m_size = index + 1;
				else if (wasAlive)
					page.destroy(slot);
				else if (wasFree && m_freeItems > 0)
					--m_freeItems;

				page.construct(slot, GAIA_MOV(item));
				page.handles[slot] = TListItem::handle(*page.ptr(slot));
				page.nextFree[slot] = TItemHandle::IdMask;
			}

			//! Restores a free slot with a preassigned id/generation and free-list link.
			//! \param handle Handle metadata to restore for the free slot.
			//! \param nextFreeIdx Next slot in the implicit free-list, or TItemHandle::IdMask.
			void add_free(TItemHandle handle, uint32_t nextFreeIdx) {
				const auto index = (size_type)handle.id();
				auto& page = ensure_page(index);
				const auto slot = slot_index(index);
				const bool existed = index < m_size;
				const bool wasAlive = existed && page.aliveMask.test(slot);
				const bool wasFree = existed && !wasAlive;

				if (index >= m_size)
					m_size = index + 1;
				else if (wasAlive)
					page.destroy(slot);

				page.handles[slot] = handle;
				page.nextFree[slot] = nextFreeIdx;
				if (!wasFree)
					++m_freeItems;
				if (m_nextFreeIdx == (size_type)-1)
					m_nextFreeIdx = index;
			}

			//! Restores a free slot with a preassigned id/generation and free-list link.
			//! \param index Slot index to restore.
			//! \param generation Generation to store in the restored handle.
			//! \param nextFreeIdx Next slot in the implicit free-list, or TItemHandle::IdMask.
			void add_free(size_type index, uint32_t generation, uint32_t nextFreeIdx) {
				add_free(ilist_handle_traits<TItemHandle>::make(index, generation, TItemHandle{}), nextFreeIdx);
			}

			//! Allocates a new item in the list.
			//! \param ctx Creation context forwarded to TListItem::create().
			//! \return Handle of the allocated item.
			//! \note Reused slots keep their generation and clear any keep-live payload before construction.
			GAIA_NODISCARD TItemHandle alloc(void* ctx) {
				size_type index = 0;
				uint32_t generation = 0;

				if GAIA_UNLIKELY (m_freeItems == 0U) {
					index = m_size;
					GAIA_ASSERT(index < TItemHandle::IdMask && "Trying to allocate too many items!");
					++m_size;
				} else {
					GAIA_ASSERT(m_nextFreeIdx < m_size && "Item recycle list broken!");
					index = m_nextFreeIdx;
					auto& page = ensure_page(index);
					const auto slot = slot_index(index);
					m_nextFreeIdx = page.nextFree[slot];
					page.nextFree[slot] = TItemHandle::IdMask;
					generation = page.handles[slot].gen();
					--m_freeItems;
					if (page.aliveMask.test(slot))
						page.destroy(slot);
				}

				auto& page = ensure_page(index);
				const auto slot = slot_index(index);
				page.construct(slot, TListItem::create(index, generation, ctx));
				page.handles[slot] = TListItem::handle(*page.ptr(slot));
				page.nextFree[slot] = TItemHandle::IdMask;
				return page.handles[slot];
			}

			//! Allocates a new item in the list.
			//! \return Handle of the allocated item.
			//! \note Reused slots keep their generation and clear any keep-live payload before construction.
			GAIA_NODISCARD TItemHandle alloc() {
				size_type index = 0;
				uint32_t generation = 0;

				if GAIA_UNLIKELY (m_freeItems == 0U) {
					index = m_size;
					GAIA_ASSERT(index < TItemHandle::IdMask && "Trying to allocate too many items!");
					++m_size;
				} else {
					GAIA_ASSERT(m_nextFreeIdx < m_size && "Item recycle list broken!");
					index = m_nextFreeIdx;
					auto& page = ensure_page(index);
					const auto slot = slot_index(index);
					m_nextFreeIdx = page.nextFree[slot];
					page.nextFree[slot] = TItemHandle::IdMask;
					generation = page.handles[slot].gen();
					--m_freeItems;
					if (page.aliveMask.test(slot))
						page.destroy(slot);
				}

				auto& page = ensure_page(index);
				const auto slot = slot_index(index);
				page.construct(slot, TListItem(index, generation));
				page.handles[slot] = ilist_handle_traits<TItemHandle>::make(index, generation, TItemHandle{});
				page.nextFree[slot] = TItemHandle::IdMask;
				return page.handles[slot];
			}

			//! Frees a live item and destroys its payload immediately.
			//! \param handle Handle identifying the item to release.
			//! \note The slot generation is incremented and the slot is linked into the implicit free-list.
			void free(TItemHandle handle) {
				GAIA_ASSERT(has(handle));

				auto& page = ensure_page(handle.id());
				const auto slot = slot_index(handle.id());
				page.destroy(slot);
				page.handles[slot] = ilist_handle_traits<TItemHandle>::make(handle.id(), handle.gen() + 1, page.handles[slot]);
				page.nextFree[slot] = m_freeItems == 0 ? TItemHandle::IdMask : m_nextFreeIdx;
				m_nextFreeIdx = handle.id();
				++m_freeItems;
			}

			//! Frees a handle while keeping the payload alive until slot reuse or clear().
			//! \param handle Handle identifying the item to release.
			//! \warning The slot becomes part of the free-list even though its payload remains alive.
			//!          Iteration and has(handle) treat it as released because the generation changes.
			//!          Callers that inspect the payload afterward must use live_unsafe() and must
			//!          guarantee the slot has not been reused.
			//! \note This is intended for systems that need released-state inspectability without
			//!       moving page storage while other background work may still observe job data.
			void free_keep_live(TItemHandle handle) {
				GAIA_ASSERT(has(handle));

				auto& page = ensure_page(handle.id());
				const auto slot = slot_index(handle.id());
				page.handles[slot] = ilist_handle_traits<TItemHandle>::make(handle.id(), handle.gen() + 1, page.handles[slot]);
				page.nextFree[slot] = m_freeItems == 0 ? TItemHandle::IdMask : m_nextFreeIdx;
				m_nextFreeIdx = handle.id();
				++m_freeItems;
			}

			//! Verifies that the implicit free-list links are well formed.
			void validate() const {
				if (m_freeItems == 0)
					return;

				auto freeItems = m_freeItems;
				auto nextFreeItem = m_nextFreeIdx;
				while (freeItems > 0) {
					GAIA_ASSERT(nextFreeItem < m_size && "Item recycle list broken!");

					nextFreeItem = next_free(nextFreeItem);
					--freeItems;
				}

				GAIA_ASSERT(nextFreeItem == TItemHandle::IdMask);
			}
		};
	} // namespace cnt
} // namespace gaia
