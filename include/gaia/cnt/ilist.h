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
		template <typename TListItem>
		struct ilist_item_traits;

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

		template <typename TListItem>
		struct ilist_item_traits {
			static_assert(
					detail::ilist_has_idx_member<TListItem>::value,
					"ilist item type must expose idx or specialize ilist_item_traits");
			static_assert(
					detail::ilist_has_gen_member<TListItem>::value || detail::ilist_has_data_gen_member<TListItem>::value,
					"ilist item type must expose gen/data.gen or specialize ilist_item_traits");

			GAIA_NODISCARD static uint32_t idx(const TListItem& item) noexcept {
				return (uint32_t)item.idx;
			}

			static void set_idx(TListItem& item, uint32_t value) noexcept {
				item.idx = value;
			}

			GAIA_NODISCARD static uint32_t gen(const TListItem& item) noexcept {
				if constexpr (detail::ilist_has_gen_member<TListItem>::value)
					return (uint32_t)item.gen;
				else
					return (uint32_t)item.data.gen;
			}

			static void set_gen(TListItem& item, uint32_t value) noexcept {
				if constexpr (detail::ilist_has_gen_member<TListItem>::value)
					item.gen = value;
				else
					item.data.gen = value;
			}
		};

		struct ilist_item {
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
			ilist_item(uint32_t index, uint32_t generation): idx(index) {
				data.gen = generation;
			}

			ilist_item(const ilist_item& other) {
				idx = other.idx;
				data.gen = other.data.gen;
			}
			ilist_item& operator=(const ilist_item& other) {
				GAIA_ASSERT(core::addressof(other) != this);
				idx = other.idx;
				data.gen = other.data.gen;
				return *this;
			}

			ilist_item(ilist_item&& other) {
				idx = other.idx;
				data.gen = other.data.gen;

				other.idx = (uint32_t)-1;
				other.data.gen = (uint32_t)-1;
			}
			ilist_item& operator=(ilist_item&& other) {
				GAIA_ASSERT(core::addressof(other) != this);
				idx = other.idx;
				data.gen = other.data.gen;

				other.idx = (uint32_t)-1;
				other.data.gen = (uint32_t)-1;
				return *this;
			}
		};

		template <typename TListItem>
		struct darray_ilist_storage: public cnt::darray<TListItem> {
			void add_item(TListItem&& container) {
				this->push_back(GAIA_MOV(container));
			}

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
			using internal_storage = TInternalStorage;

			using value_type = TListItem;
			using reference = TListItem&;
			using const_reference = const TListItem&;
			using pointer = TListItem*;
			using const_pointer = const TListItem*;
			using difference_type = typename internal_storage::difference_type;
			using size_type = typename internal_storage::size_type;

			// TODO: replace this iterator with a real list iterator
			using iterator = typename internal_storage::iterator;
			using const_iterator = typename internal_storage::const_iterator;
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

			GAIA_NODISCARD pointer data() noexcept {
				return reinterpret_cast<pointer>(m_items.data());
			}

			GAIA_NODISCARD const_pointer data() const noexcept {
				return reinterpret_cast<const_pointer>(m_items.data());
			}

			GAIA_NODISCARD reference operator[](size_type index) {
				return m_items[index];
			}
			GAIA_NODISCARD const_reference operator[](size_type index) const {
				return m_items[index];
			}

			void clear() {
				m_items.clear();
				m_nextFreeIdx = (size_type)-1;
				m_freeItems = 0;
			}

			GAIA_NODISCARD size_type get_next_free_item() const noexcept {
				return m_nextFreeIdx;
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

			GAIA_NODISCARD bool empty() const noexcept {
				return size() == 0;
			}

			GAIA_NODISCARD size_type capacity() const noexcept {
				return (size_type)m_items.capacity();
			}

			GAIA_NODISCARD iterator begin() noexcept {
				return m_items.begin();
			}

			GAIA_NODISCARD const_iterator begin() const noexcept {
				return m_items.begin();
			}

			GAIA_NODISCARD const_iterator cbegin() const noexcept {
				return m_items.begin();
			}

			GAIA_NODISCARD iterator end() noexcept {
				return m_items.end();
			}

			GAIA_NODISCARD const_iterator end() const noexcept {
				return m_items.end();
			}

			GAIA_NODISCARD const_iterator cend() const noexcept {
				return m_items.end();
			}

			void reserve(size_type cap) {
				m_items.reserve(cap);
			}

			//! Allocates a new item in the list
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

			//! Invalidates @a handle.
			//! Every time an item is deallocated its generation is increased by one.
			//! \param handle Handle
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

		template <typename TItemHandle, typename = void>
		struct ilist_handle_traits {
			static TItemHandle make(uint32_t id, uint32_t gen, const TItemHandle&) {
				return TItemHandle(id, gen);
			}
		};

		template <typename TListItem, typename TItemHandle>
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
			using value_type = typename TPagedIList::value_type;
			using reference =
					std::conditional_t<IsConst, typename TPagedIList::const_reference, typename TPagedIList::reference>;
			using pointer = std::conditional_t<IsConst, typename TPagedIList::const_pointer, typename TPagedIList::pointer>;
			using difference_type = typename TPagedIList::difference_type;
			using iterator_category = core::forward_iterator_tag;

			paged_ilist_iterator() = default;
			paged_ilist_iterator(owner_pointer pOwner, typename TPagedIList::size_type index):
					m_pOwner(pOwner), m_index(index) {
				skip_dead();
			}

			GAIA_NODISCARD reference operator*() const {
				return (*m_pOwner)[m_index];
			}

			GAIA_NODISCARD pointer operator->() const {
				return &(*m_pOwner)[m_index];
			}

			paged_ilist_iterator& operator++() {
				++m_index;
				skip_dead();
				return *this;
			}

			paged_ilist_iterator operator++(int) {
				auto tmp = *this;
				++(*this);
				return tmp;
			}

			GAIA_NODISCARD bool operator==(const paged_ilist_iterator& other) const {
				return m_pOwner == other.m_pOwner && m_index == other.m_index;
			}

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
		template <typename TListItem, typename TItemHandle>
		struct paged_ilist {
			using value_type = TListItem;
			using reference = TListItem&;
			using const_reference = const TListItem&;
			using pointer = TListItem*;
			using const_pointer = const TListItem*;
			using difference_type = std::ptrdiff_t;
			using size_type = uint32_t;
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

			cnt::darray<page_type*> m_pages;
			size_type m_size = 0;

		public:
			size_type m_nextFreeIdx = (size_type)-1;
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

			void clear_pages() {
				for (auto* pPage: m_pages) {
					if (pPage == nullptr)
						continue;
					delete pPage;
				}
				m_pages.clear();
			}

			void ensure_page_count(size_type slotCnt) {
				const auto pageCnt = page_count_for_slots(slotCnt);
				if (pageCnt > (size_type)m_pages.size())
					m_pages.resize(pageCnt, nullptr);
			}

			GAIA_NODISCARD page_type& ensure_page(size_type index) {
				ensure_page_count(index + 1);
				auto*& pPage = m_pages[page_index(index)];
				if (pPage == nullptr)
					pPage = new page_type();
				return *pPage;
			}

			GAIA_NODISCARD page_type* try_page(size_type index) noexcept {
				const auto pageIdx = page_index(index);
				if (pageIdx >= (size_type)m_pages.size())
					return nullptr;
				return m_pages[pageIdx];
			}

			GAIA_NODISCARD const page_type* try_page(size_type index) const noexcept {
				const auto pageIdx = page_index(index);
				if (pageIdx >= (size_type)m_pages.size())
					return nullptr;
				return m_pages[pageIdx];
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
			using iterator = paged_ilist_iterator<paged_ilist, false>;
			using const_iterator = paged_ilist_iterator<paged_ilist, true>;

			~paged_ilist() {
				clear_pages();
			}

			paged_ilist() = default;
			paged_ilist(const paged_ilist&) = delete;
			paged_ilist& operator=(const paged_ilist&) = delete;
			paged_ilist(paged_ilist&& other) noexcept:
					m_pages(GAIA_MOV(other.m_pages)), m_size(other.m_size), m_nextFreeIdx(other.m_nextFreeIdx),
					m_freeItems(other.m_freeItems) {
				other.m_size = 0;
				other.m_nextFreeIdx = (size_type)-1;
				other.m_freeItems = 0;
			}
			paged_ilist& operator=(paged_ilist&& other) noexcept {
				GAIA_ASSERT(core::addressof(other) != this);
				clear_pages();
				m_pages = GAIA_MOV(other.m_pages);
				m_size = other.m_size;
				m_nextFreeIdx = other.m_nextFreeIdx;
				m_freeItems = other.m_freeItems;

				other.m_size = 0;
				other.m_nextFreeIdx = (size_type)-1;
				other.m_freeItems = 0;
				return *this;
			}

			GAIA_NODISCARD pointer data() noexcept {
				return nullptr;
			}

			GAIA_NODISCARD const_pointer data() const noexcept {
				return nullptr;
			}

			GAIA_NODISCARD bool has(size_type index) const noexcept {
				if (index >= m_size)
					return false;

				const auto* pPage = try_page(index);
				return pPage != nullptr && pPage->aliveMask.test(slot_index(index));
			}

			GAIA_NODISCARD bool has(TItemHandle handle) const noexcept {
				return has(handle.id()) && this->handle(handle.id()) == handle;
			}

			GAIA_NODISCARD TItemHandle handle(size_type index) const noexcept {
				GAIA_ASSERT(index < m_size);
				const auto* pPage = try_page(index);
				GAIA_ASSERT(pPage != nullptr);
				return pPage->handles[slot_index(index)];
			}

			GAIA_NODISCARD uint32_t generation(size_type index) const noexcept {
				return handle(index).gen();
			}

			GAIA_NODISCARD uint32_t next_free(size_type index) const noexcept {
				GAIA_ASSERT(index < m_size);
				const auto* pPage = try_page(index);
				GAIA_ASSERT(pPage != nullptr);
				return pPage->nextFree[slot_index(index)];
			}

			GAIA_NODISCARD reference operator[](size_type index) {
				GAIA_ASSERT(has(index));
				return slot_ref(index);
			}

			GAIA_NODISCARD const_reference operator[](size_type index) const {
				GAIA_ASSERT(has(index));
				return slot_ref(index);
			}

			void clear() {
				clear_pages();
				m_size = 0;
				m_nextFreeIdx = (size_type)-1;
				m_freeItems = 0;
			}

			GAIA_NODISCARD size_type get_next_free_item() const noexcept {
				return m_nextFreeIdx;
			}

			GAIA_NODISCARD size_type get_free_items() const noexcept {
				return m_freeItems;
			}

			GAIA_NODISCARD size_type item_count() const noexcept {
				return m_size - m_freeItems;
			}

			GAIA_NODISCARD size_type size() const noexcept {
				return m_size;
			}

			GAIA_NODISCARD bool empty() const noexcept {
				return m_size == 0;
			}

			GAIA_NODISCARD size_type capacity() const noexcept {
				return (size_type)m_pages.capacity() * PageCapacity;
			}

			//! Returns an iterator over live payload objects only.
			GAIA_NODISCARD iterator begin() noexcept {
				return iterator(this, 0);
			}

			GAIA_NODISCARD const_iterator begin() const noexcept {
				return const_iterator(this, 0);
			}

			GAIA_NODISCARD const_iterator cbegin() const noexcept {
				return const_iterator(this, 0);
			}

			GAIA_NODISCARD iterator end() noexcept {
				return iterator(this, m_size);
			}

			GAIA_NODISCARD const_iterator end() const noexcept {
				return const_iterator(this, m_size);
			}

			GAIA_NODISCARD const_iterator cend() const noexcept {
				return const_iterator(this, m_size);
			}

			void reserve(size_type cap) {
				m_pages.reserve(page_count_for_slots(cap));
			}

			GAIA_NODISCARD pointer try_get(size_type index) noexcept {
				if (!has(index))
					return nullptr;
				return &slot_ref(index);
			}

			GAIA_NODISCARD const_pointer try_get(size_type index) const noexcept {
				if (!has(index))
					return nullptr;
				return &slot_ref(index);
			}

			//! Restores a live slot with a preassigned id/generation.
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

			//! Restores a free slot with a preassigned id/generation and freelist link.
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

			//! Restores a free slot with a preassigned id/generation and freelist link.
			void add_free(size_type index, uint32_t generation, uint32_t nextFreeIdx) {
				add_free(ilist_handle_traits<TItemHandle>::make(index, generation, TItemHandle{}), nextFreeIdx);
			}

			//! Allocates a new item in the list
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
				}

				auto& page = ensure_page(index);
				const auto slot = slot_index(index);
				page.construct(slot, TListItem::create(index, generation, ctx));
				page.handles[slot] = TListItem::handle(*page.ptr(slot));
				page.nextFree[slot] = TItemHandle::IdMask;
				return page.handles[slot];
			}

			//! Allocates a new item in the list
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
				}

				auto& page = ensure_page(index);
				const auto slot = slot_index(index);
				page.construct(slot, TListItem(index, generation));
				page.handles[slot] = ilist_handle_traits<TItemHandle>::make(index, generation, TItemHandle{});
				page.nextFree[slot] = TItemHandle::IdMask;
				return page.handles[slot];
			}

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

			void validate() const {
				if (m_freeItems == 0)
					return;

				auto freeItems = m_freeItems;
				auto nextFreeItem = m_nextFreeIdx;
				while (freeItems > 0) {
					GAIA_ASSERT(nextFreeItem < m_size && "Item recycle list broken!");
					GAIA_ASSERT(!has(nextFreeItem));

					nextFreeItem = next_free(nextFreeItem);
					--freeItems;
				}

				GAIA_ASSERT(nextFreeItem == TItemHandle::IdMask);
			}
		};
	} // namespace cnt
} // namespace gaia
