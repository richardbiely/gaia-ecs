#pragma once
#include "gaia/config/config.h"

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <type_traits>
#include <utility>

#include "gaia/cnt/bitset.h"
#include "gaia/cnt/darray.h"
#include "gaia/core/iterator.h"
#include "gaia/core/utility.h"
#include "gaia/mem/data_layout_policy.h"
#include "gaia/mem/mem_alloc.h"
#include "gaia/mem/mem_utils.h"
#include "gaia/mem/paged_allocator.h"
#include "gaia/mem/raw_data_holder.h"

namespace gaia {
	namespace cnt {
		//! Identifier used to address an element in paged storage.
		using page_storage_id = uint32_t;

		//! \cond INTERNAL
		namespace detail {
			using difference_type = uint32_t;
			using size_type = uint32_t;

			constexpr static page_storage_id InvalidPageStorageId = (page_storage_id)-1;

			template <typename T>
			struct mem_page_data;
			template <typename T, typename Allocator>
			class mem_page;
		} // namespace detail
		//! \endcond

		//! Customization point for converting a value to its paged-storage identifier.
		//! The default implementation supports only empty types and returns an invalid identifier.
		//! \tparam T Value type to convert.
		template <typename T>
		struct to_page_storage_id {
			//! Applies the default conversion for an item.
			//! \param item Item to convert.
			//! \return The invalid paged-storage identifier.
			static page_storage_id get(const T& item) noexcept {
				(void)item;
				static_assert(
						std::is_empty_v<T>,
						"Sparse_storage items require a conversion function to be defined in gaia::cnt namespace");
				return detail::InvalidPageStorageId;
			}
		};

		//! \cond INTERNAL
		namespace detail {
			template <typename T, typename Allocator, bool IsFwd>
			struct mem_page_iterator {
				using value_type = T;
				using pointer = T*;
				using reference = T&;
				using difference_type = detail::difference_type;
				using size_type = detail::size_type;
				using iterator = mem_page_iterator;
				using iterator_category = core::bidirectional_iterator_tag;

			private:
				using page_data_type = detail::mem_page_data<T>;
				using page_type = detail::mem_page<T, Allocator>;
				using bit_set_iter_type = std::conditional_t<
						IsFwd, typename page_data_type::bit_set::iter, typename page_data_type::bit_set::iter_rev>;

				page_type* m_pPage = nullptr;
				bit_set_iter_type m_it;

			public:
				mem_page_iterator() = default;
				mem_page_iterator(page_type* pPage): m_pPage(pPage) {}
				mem_page_iterator(page_type* pPage, bit_set_iter_type it): m_pPage(pPage), m_it(it) {}

				reference operator*() const {
					return m_pPage->set_data(*m_it);
				}
				pointer operator->() const {
					return &m_pPage->set_data(*m_it);
				}

				iterator& operator++() {
					++m_it;
					return *this;
				}
				iterator operator++(int) {
					iterator temp(*this);
					++*this;
					return temp;
				}

				GAIA_NODISCARD bool operator==(const iterator& other) const {
					return m_pPage == other.m_pPage && m_it == other.m_it;
				}
				GAIA_NODISCARD bool operator!=(const iterator& other) const {
					return m_pPage != other.m_pPage && m_it != other.m_it;
				}
			};

			template <typename T, typename Allocator, bool IsFwd>
			struct mem_page_iterator_soa {
				using value_type = T;
				// using pointer = T*; not supported
				// using reference = T&; not supported
				using difference_type = detail::difference_type;
				using size_type = detail::size_type;
				using iterator = mem_page_iterator_soa;
				using iterator_category = core::bidirectional_iterator_tag;

			private:
				using page_data_type = detail::mem_page_data<T>;
				using page_type = detail::mem_page<T, Allocator>;
				using bit_set_iter_type = std::conditional_t<
						IsFwd, typename page_data_type::bit_set::iter, typename page_data_type::bit_set::iter_rev>;

				page_type* m_pPage;
				bit_set_iter_type m_it;

			public:
				mem_page_iterator_soa(page_type* pPage, bit_set_iter_type it): m_pPage(pPage), m_it(it) {}

				value_type operator*() const {
					return m_pPage->set_data(*m_it);
				}
				value_type operator->() const {
					return &m_pPage->set_data(*m_it);
				}

				iterator& operator++() {
					++m_it;
					return *this;
				}
				iterator operator++(int) {
					iterator temp(*this);
					++*this;
					return temp;
				}
				iterator& operator--() {
					--m_it;
					return *this;
				}
				iterator operator--(int) {
					iterator temp(*this);
					--*this;
					return temp;
				}

				GAIA_NODISCARD bool operator==(const iterator& other) const {
					return m_pPage == other.m_pPage && m_it == other.m_it;
				}
				GAIA_NODISCARD bool operator!=(const iterator& other) const {
					return m_pPage != other.m_pPage && m_it != other.m_it;
				}
			};

			template <typename T>
			struct mem_page_data {
				static constexpr uint32_t PageCapacity = 4096;

				using view_policy = mem::auto_view_policy<T>;
				using bit_set = cnt::bitset<PageCapacity>;
				static constexpr uint32_t AllocatedBytes = view_policy::get_min_byte_size(0, PageCapacity);

				struct PageHeader {
					//! How many items are allocated inside the page
					size_type cnt = size_type(0);
					//! Mask for used data
					bit_set mask;
				};

				//! Page header
				PageHeader header;
				//! Page data
				mem::raw_data_holder<T, AllocatedBytes> data;
			};

			template <typename T, typename Allocator>
			class mem_page {
				static_assert(!std::is_empty_v<T>, "It only makes sense to use page storage for data types with non-zero size");

			public:
				using value_type = T;
				using reference = T&;
				using const_reference = const T&;
				using pointer = T*;
				using const_pointer = const T*;
				using view_policy = mem::auto_view_policy<T>;
				using difference_type = detail::difference_type;
				using size_type = detail::size_type;

				template <bool IsFwd>
				using iterator_base = mem_page_iterator<T, Allocator, IsFwd>;
				using iterator = iterator_base<true>;
				using iterator_reverse = iterator_base<false>;

				template <bool IsFwd>
				using iterator_soa_base = mem_page_iterator_soa<T, Allocator, IsFwd>;
				using iterator_soa = iterator_soa_base<true>;
				using iterator_soa_reverse = iterator_soa_base<false>;

				using PageData = mem_page_data<T>;
				static constexpr uint32_t PageCapacity = PageData::PageCapacity;

			private:
				//! Pointer to page data
				PageData* m_pData = nullptr;

				void ensure() {
					if GAIA_LIKELY (m_pData != nullptr)
						return;

					// Allocate memory for data
					m_pData = mem::AllocHelper::alloc<PageData, Allocator>(1);
					(void)new (m_pData) PageData{};
					// Prepare the item data region
					core::call_ctor_raw_n(data(), PageCapacity);
				}

				void dtr_data_inter(uint32_t idx) noexcept {
					GAIA_ASSERT(!empty());

					if constexpr (!mem::is_soa_layout_v<T>) {
						auto* ptr = &data()[idx];
						core::call_dtor(ptr);
					}

					m_pData->header.mask.set(idx, false);
					--m_pData->header.cnt;
				}

				void dtr_active_data() noexcept {
					if constexpr (!mem::is_soa_layout_v<T>) {
						for (auto i: m_pData->header.mask) {
							auto* ptr = &data()[i];
							core::call_dtor(ptr);
						}
					}
				}

				void invalidate() {
					if (m_pData == nullptr)
						return;

					// Destruct active items
					dtr_active_data();

					// Release allocated memory
					m_pData->~PageData();
					mem::AllocHelper::free<Allocator>(m_pData);
					m_pData = nullptr;
				}

			public:
				mem_page() = default;

				mem_page(const mem_page& other) {
					// Copy new items over
					if (other.m_pData == nullptr) {
						invalidate();
					} else {
						ensure();

						m_pData->header.mask = other.m_pData->header.mask;
						m_pData->header.cnt = other.m_pData->header.cnt;

						// Copy construct data
						for (auto i: other.m_pData->header.mask)
							add_data(i, other.get_data(i));
					}
				}

				mem_page& operator=(const mem_page& other) {
					GAIA_ASSERT(core::addressof(other) != this);

					// Copy new items over if there are any
					if (other.m_pData == nullptr) {
						invalidate();
					} else {
						ensure();

						// Remove current active items
						if (m_pData != nullptr)
							dtr_active_data();

						m_pData->header.mask = other.m_pData->header.mask;
						m_pData->header.cnt = other.m_pData->header.cnt;

						// Copy data
						for (auto i: other.m_pData->header.mask)
							add_data(i, other.get_data(i));
					}

					return *this;
				}

				mem_page(mem_page&& other) noexcept {
					m_pData = other.m_pData;
					other.m_pData = nullptr;
				}

				mem_page& operator=(mem_page&& other) noexcept {
					GAIA_ASSERT(core::addressof(other) != this);

					invalidate();

					m_pData = other.m_pData;
					other.m_pData = nullptr;

					return *this;
				}

				~mem_page() {
					invalidate();
				}

				GAIA_CLANG_WARNING_PUSH()
				// Memory is aligned so we can silence this warning
				GAIA_CLANG_WARNING_DISABLE("-Wcast-align")

				GAIA_NODISCARD pointer data() noexcept {
					return GAIA_ACC((pointer)&m_pData->data[0]);
				}

				GAIA_NODISCARD const_pointer data() const noexcept {
					return GAIA_ACC((const_pointer)&m_pData->data[0]);
				}

				GAIA_NODISCARD decltype(auto) set_data(size_type pos) noexcept {
					GAIA_ASSERT(m_pData->header.mask.test(pos));
					return view_policy::set(
							{GAIA_ACC((typename view_policy::TargetCastType) & m_pData->data[0]), PageCapacity}, pos);
				}

				GAIA_NODISCARD decltype(auto) operator[](size_type pos) noexcept {
					GAIA_ASSERT(m_pData->header.mask.test(pos));
					return view_policy::set(
							{GAIA_ACC((typename view_policy::TargetCastType) & m_pData->data[0]), PageCapacity}, pos);
				}

				GAIA_NODISCARD decltype(auto) get_data(size_type pos) const noexcept {
					GAIA_ASSERT(m_pData->header.mask.test(pos));
					return view_policy::get(
							{GAIA_ACC((typename view_policy::TargetCastType) & m_pData->data[0]), PageCapacity}, pos);
				}

				GAIA_NODISCARD decltype(auto) operator[](size_type pos) const noexcept {
					GAIA_ASSERT(m_pData->header.mask.test(pos));
					return view_policy::get(
							{GAIA_ACC((typename view_policy::TargetCastType) & m_pData->data[0]), PageCapacity}, pos);
				}

				GAIA_CLANG_WARNING_POP()

				void add() {
					ensure();
					++m_pData->header.cnt;
				}

				decltype(auto) add_data(uint32_t idx, const T& arg) {
					m_pData->header.mask.set(idx);

					if constexpr (mem::is_soa_layout_v<T>) {
						set_data(idx) = arg;
					} else {
						auto* ptr = &set_data(idx);
						core::call_ctor(ptr, arg);
						return (reference)(*ptr);
					}
				}

				decltype(auto) add_data(uint32_t idx, T&& arg) {
					m_pData->header.mask.set(idx);

					if constexpr (mem::is_soa_layout_v<T>) {
						set_data(idx) = GAIA_MOV(arg);
					} else {
						auto* ptr = &set_data(idx);
						core::call_ctor(ptr, GAIA_MOV(arg));
						return (reference)(*ptr);
					}
				}

				template <typename... Args>
				decltype(auto) emplace_data(uint32_t idx, Args&&... args) {
					m_pData->header.used.set(idx);

					if constexpr (mem::is_soa_layout_v<T>) {
						set_data(idx) = T(GAIA_FWD(args)...);
					} else {
						auto* ptr = &set_data(idx);
						core::call_ctor(ptr, GAIA_FWD(args)...);
						return (reference)(*ptr);
					}
				}

				void del_data(uint32_t idx) noexcept {
					dtr_data_inter(idx);

					// If there is no more data, release the memory allocated by the page
					if (m_pData->header.cnt == 0)
						invalidate();
				}

				GAIA_NODISCARD bool has_data(uint32_t idx) const noexcept {
					return m_pData ? m_pData->header.mask.test(idx) : false;
				}

				GAIA_NODISCARD size_type size() const noexcept {
					return m_pData ? m_pData->header.cnt : 0;
				}

				GAIA_NODISCARD bool empty() const noexcept {
					return size() == 0;
				}

				GAIA_NODISCARD decltype(auto) front() noexcept {
					GAIA_ASSERT(!empty());
					if constexpr (mem::is_soa_layout_v<T>)
						return *begin();
					else
						return (reference)*begin();
				}

				GAIA_NODISCARD decltype(auto) front() const noexcept {
					GAIA_ASSERT(!empty());
					if constexpr (mem::is_soa_layout_v<T>)
						return *begin();
					else
						return (const_reference)*begin();
				}

				GAIA_NODISCARD decltype(auto) back() noexcept {
					GAIA_ASSERT(!empty());
					const auto idx = *m_pData->header.mask.rbegin();
					if constexpr (mem::is_soa_layout_v<T>)
						return set_data(idx);
					else
						return (reference)(set_data(idx));
				}

				GAIA_NODISCARD decltype(auto) back() const noexcept {
					GAIA_ASSERT(!empty());
					const auto idx = *m_pData->header.mask.rbegin();
					if constexpr (mem::is_soa_layout_v<T>)
						return set_data(idx);
					else
						return (const_reference)set_data(idx);
				}

				static constexpr typename PageData::bit_set s_dummyBitSet{};

				GAIA_NODISCARD auto begin() const noexcept {
					if constexpr (mem::is_soa_layout_v<T>)
						return iterator_soa((mem_page*)this, m_pData ? m_pData->header.mask.begin() : s_dummyBitSet.begin());
					else
						return iterator((mem_page*)this, m_pData ? m_pData->header.mask.begin() : s_dummyBitSet.begin());
				}

				GAIA_NODISCARD auto end() const noexcept {
					if constexpr (mem::is_soa_layout_v<T>)
						return iterator_soa((mem_page*)this, m_pData ? m_pData->header.mask.end() : s_dummyBitSet.end());
					else
						return iterator((mem_page*)this, m_pData ? m_pData->header.mask.end() : s_dummyBitSet.end());
				}

				GAIA_NODISCARD auto rbegin() const noexcept {
					if constexpr (mem::is_soa_layout_v<T>)
						return iterator_soa_reverse(
								(mem_page*)this, m_pData ? m_pData->header.mask.rbegin() : s_dummyBitSet.rbegin());
					else
						return iterator_reverse((mem_page*)this, m_pData ? m_pData->header.mask.rbegin() : s_dummyBitSet.rbegin());
				}

				GAIA_NODISCARD auto rend() const noexcept {
					if constexpr (mem::is_soa_layout_v<T>)
						return iterator_soa_reverse((mem_page*)this, m_pData ? m_pData->header.mask.rend() : s_dummyBitSet.rend());
					else
						return iterator_reverse((mem_page*)this, m_pData ? m_pData->header.mask.rend() : s_dummyBitSet.rend());
				}

				GAIA_NODISCARD bool operator==(const mem_page& other) const noexcept {
					// We expect to compare only valid pages
					GAIA_ASSERT(m_pData != nullptr);
					GAIA_ASSERT(other.m_pData != nullptr);

					if (m_pData->header.cnt != other.m_pData->header.cnt)
						return false;
					if (m_pData->header.mask != other.m_pData->header.mask)
						return false;
					for (auto i: m_pData->header.mask)
						if (!(get_data(i) == other[i]))
							return false;
					return true;
				}

				GAIA_NODISCARD bool operator!=(const mem_page& other) const noexcept {
					return !operator==(other);
				}
			};
		} // namespace detail
		//! \endcond

		//! Iterator over elements in paged storage.
		//! \tparam T Element type.
		//! \tparam Allocator Page allocator type.
		//! \tparam IsFwd Whether iteration proceeds forward.
		template <typename T, typename Allocator, bool IsFwd>
		struct page_iterator {
			//! Element type.
			using value_type = T;
			//! Pointer to an element.
			using pointer = T*;
			//! Reference to an element.
			using reference = T&;
			//! Type used for iterator distances.
			using difference_type = detail::difference_type;
			//! Type used for sizes and indices.
			using size_type = detail::size_type;
			//! Iterator type.
			using iterator = page_iterator;
			//! Iterator category tag.
			using iterator_category = core::bidirectional_iterator_tag;

		private:
			using page_type = detail::mem_page<T, Allocator>;

			page_type* m_pPage;
			page_type* m_pPageLast;
			typename page_type::template iterator_base<IsFwd> m_it;

		public:
			//! Constructs an end iterator at a page boundary.
			//! \param pPage Boundary page.
			page_iterator(page_type* pPage): m_pPage(pPage), m_pPageLast(pPage) {}

			//! Constructs an iterator over a page range.
			//! \param pPage First page to inspect.
			//! \param pPageLast Page marking the range boundary.
			page_iterator(page_type* pPage, page_type* pPageLast): m_pPage(pPage), m_pPageLast(pPageLast) {
				// Find first page with data
				if constexpr (!IsFwd) {
					m_it = m_pPage->rbegin();
					while (m_it == m_pPage->rend()) {
						--m_pPage;
						if (m_pPage == m_pPageLast) {
							m_it = {};
							break;
						}
						m_it = m_pPage->rbegin();
					}
				} else {
					m_it = m_pPage->begin();
					while (m_it == m_pPage->end()) {
						++m_pPage;
						if (m_pPage == m_pPageLast) {
							m_it = {};
							break;
						}
						m_it = m_pPage->begin();
					}
				}
			}

			//! Accesses the current element.
			//! \return Reference to the current element.
			reference operator*() const {
				return m_it.operator*();
			}
			//! Accesses the current element through a pointer.
			//! \return Pointer to the current element.
			pointer operator->() const {
				return m_it.operator->();
			}

			//! Advances to the next element.
			//! \return Reference to this advanced iterator.
			iterator& operator++() {
				if constexpr (!IsFwd) {
					++m_it;
					if (m_it == m_pPage->rend()) {
						--m_pPage;
						if (m_pPage == m_pPageLast) {
							m_it = {};
							return *this;
						}
						m_it = m_pPage->rbegin();
					}
				} else {
					++m_it;
					if (m_it == m_pPage->end()) {
						++m_pPage;
						if (m_pPage == m_pPageLast) {
							m_it = {};
							return *this;
						}
						m_it = m_pPage->begin();
					}
				}
				return *this;
			}
			//! Advances to the next element.
			//! \return Iterator value before advancement.
			iterator operator++(int) {
				iterator temp(*this);
				++*this;
				return temp;
			}

			//! Checks whether two iterators have the same position.
			//! \param other Iterator to compare with.
			//! \return True if both iterators have the same position.
			GAIA_NODISCARD bool operator==(const iterator& other) const {
				return m_pPage == other.m_pPage && m_it == other.m_it;
			}
			//! Checks whether two iterators have different positions.
			//! \param other Iterator to compare with.
			//! \return True if the iterators have different positions.
			GAIA_NODISCARD bool operator!=(const iterator& other) const {
				return m_pPage != other.m_pPage || m_it != other.m_it;
			}
		};

		//! Read-only iterator over elements in paged storage.
		//! \tparam T Element type.
		//! \tparam Allocator Page allocator type.
		//! \tparam IsFwd Whether iteration proceeds forward.
		template <typename T, typename Allocator, bool IsFwd>
		struct const_page_iterator {
			//! Element type.
			using value_type = T;
			//! Pointer to a read-only element.
			using pointer = const T*;
			//! Reference to a read-only element.
			using reference = const T&;
			//! Type used for iterator distances.
			using difference_type = detail::difference_type;
			//! Type used for sizes and indices.
			using size_type = detail::size_type;
			//! Iterator type.
			using iterator = const_page_iterator;
			//! Iterator category tag.
			using iterator_category = core::bidirectional_iterator_tag;

		private:
			using page_type = detail::mem_page<T, Allocator>;

			const page_type* m_pPage;
			const page_type* m_pPageLast;
			typename page_type::template iterator_base<IsFwd> m_it;

		public:
			//! Constructs an end iterator at a page boundary.
			//! \param pPage Boundary page.
			const_page_iterator(const page_type* pPage): m_pPage(pPage), m_pPageLast(pPage) {}

			//! Constructs a read-only iterator over a page range.
			//! \param pPage First page to inspect.
			//! \param pPageLast Page marking the range boundary.
			const_page_iterator(const page_type* pPage, const page_type* pPageLast): m_pPage(pPage), m_pPageLast(pPageLast) {
				// Find first page with data
				if constexpr (!IsFwd) {
					m_it = m_pPage->rbegin();
					while (m_it == m_pPage->rend()) {
						--m_pPage;
						if (m_pPage == m_pPageLast) {
							m_it = {};
							break;
						}
						m_it = m_pPage->rbegin();
					}
				} else {
					m_it = m_pPage->begin();
					while (m_it == m_pPage->end()) {
						++m_pPage;
						if (m_pPage == m_pPageLast) {
							m_it = {};
							break;
						}
						m_it = m_pPage->begin();
					}
				}
			}

			//! Accesses the current element.
			//! \return Read-only reference to the current element.
			reference operator*() const {
				return m_it.operator*();
			}
			//! Accesses the current element through a pointer.
			//! \return Read-only pointer to the current element.
			pointer operator->() const {
				return m_it.operator->();
			}

			//! Advances to the next element.
			//! \return Reference to this advanced iterator.
			iterator& operator++() {
				if constexpr (!IsFwd) {
					++m_it;
					if (m_it == m_pPage->rend()) {
						--m_pPage;
						if (m_pPage == m_pPageLast) {
							m_it = {};
							return *this;
						}
						m_it = m_pPage->rbegin();
					}
				} else {
					++m_it;
					if (m_it == m_pPage->end()) {
						++m_pPage;
						if (m_pPage == m_pPageLast) {
							m_it = {};
							return *this;
						}
						m_it = m_pPage->begin();
					}
				}
				return *this;
			}
			//! Advances to the next element.
			//! \return Iterator value before advancement.
			iterator operator++(int) {
				iterator temp(*this);
				++*this;
				return temp;
			}

			//! Checks whether two iterators have the same position.
			//! \param other Iterator to compare with.
			//! \return True if both iterators have the same position.
			GAIA_NODISCARD bool operator==(const iterator& other) const {
				return m_pPage == other.m_pPage && m_it == other.m_it;
			}
			//! Checks whether two iterators have different positions.
			//! \param other Iterator to compare with.
			//! \return True if the iterators have different positions.
			GAIA_NODISCARD bool operator!=(const iterator& other) const {
				return m_pPage != other.m_pPage || m_it != other.m_it;
			}
		};

		//! Iterator over structure-of-arrays elements in paged storage.
		//! \tparam T Element type.
		//! \tparam Allocator Page allocator type.
		//! \tparam IsFwd Whether iteration proceeds forward.
		template <typename T, typename Allocator, bool IsFwd>
		struct page_iterator_soa {
			//! Element view type returned by value.
			using value_type = T;
			// using pointer = T*;
			// using reference = T&;
			//! Type used for iterator distances.
			using difference_type = detail::difference_type;
			//! Type used for sizes and indices.
			using size_type = detail::size_type;
			//! Iterator type.
			using iterator = page_iterator_soa;
			//! Iterator category tag.
			using iterator_category = core::bidirectional_iterator_tag;

		private:
			using page_type = detail::mem_page<T, Allocator>;

			page_type* m_pPage;
			page_type* m_pPageLast;
			typename page_type::template iterator_soa_base<IsFwd> m_it;

		public:
			//! Constructs an end iterator at a page boundary.
			//! \param pPage Boundary page.
			page_iterator_soa(page_type* pPage): m_pPage(pPage), m_pPageLast(pPage) {}

			//! Constructs an iterator over a page range.
			//! \param pPage First page to inspect.
			//! \param pPageLast Page marking the range boundary.
			page_iterator_soa(page_type* pPage, page_type* pPageLast): m_pPage(pPage), m_pPageLast(pPageLast) {
				// Find first page with data
				if constexpr (!IsFwd) {
					m_it = m_pPage->rbegin();
					while (m_it == m_pPage->rend()) {
						if (m_pPage == m_pPageLast)
							break;
						--m_pPage;
						m_it = m_pPage->rbegin();
					}
				} else {
					m_it = m_pPage->begin();
					while (m_it == m_pPage->end()) {
						if (m_pPage == m_pPageLast)
							break;
						++m_pPage;
						m_it = m_pPage->begin();
					}
				}
			}

			//! Accesses the current element view.
			//! \return View of the current element.
			value_type operator*() const {
				return m_it.operator*();
			}
			//! Accesses the current element view through arrow syntax.
			//! \return View of the current element.
			value_type operator->() const {
				return m_it.operator->();
			}

			//! Advances to the next element.
			//! \return Reference to this advanced iterator.
			iterator& operator++() {
				if constexpr (!IsFwd) {
					++m_it;
					while (m_it == m_pPage->rend()) {
						--m_pPage;
						m_it = m_pPage->rbegin();
					}
				} else {
					++m_it;
					while (m_it == m_pPage->end()) {
						++m_pPage;
						m_it = m_pPage->begin();
					}
				}
				return *this;
			}
			//! Advances to the next element.
			//! \return Iterator value before advancement.
			iterator operator++(int) {
				iterator temp(*this);
				++*this;
				return temp;
			}

			//! Checks whether two iterators have the same position.
			//! \param other Iterator to compare with.
			//! \return True if both iterators have the same position.
			GAIA_NODISCARD bool operator==(const iterator& other) const {
				return m_pPage == other.m_pPage && m_it == other.m_it;
			}
			//! Checks whether two iterators have different positions.
			//! \param other Iterator to compare with.
			//! \return True if the iterators have different positions.
			GAIA_NODISCARD bool operator!=(const iterator& other) const {
				return m_pPage != other.m_pPage || m_it != other.m_it;
			}
		};

		//! Read-only iterator over structure-of-arrays elements in paged storage.
		//! \tparam T Element type.
		//! \tparam Allocator Page allocator type.
		//! \tparam IsFwd Whether iteration proceeds forward.
		template <typename T, typename Allocator, bool IsFwd>
		struct const_page_iterator_soa {
			//! Element view type returned by value.
			using value_type = T;
			// using pointer = T*;
			// using reference = T&;
			//! Type used for iterator distances.
			using difference_type = detail::difference_type;
			//! Type used for sizes and indices.
			using size_type = detail::size_type;
			//! Iterator type.
			using iterator = const_page_iterator_soa;
			//! Iterator category tag.
			using iterator_category = core::bidirectional_iterator_tag;

		private:
			using page_type = detail::mem_page<T, Allocator>;

			const page_type* m_pPage;
			const page_type* m_pPageLast;
			typename page_type::template iterator_soa_base<IsFwd> m_it;

		public:
			//! Constructs an end iterator at a page boundary.
			//! \param pPage Boundary page.
			const_page_iterator_soa(const page_type* pPage): m_pPage(pPage), m_pPageLast(pPage) {}

			//! Constructs a read-only iterator over a page range.
			//! \param pPage First page to inspect.
			//! \param pPageLast Page marking the range boundary.
			const_page_iterator_soa(const page_type* pPage, const page_type* pPageLast):
					m_pPage(pPage), m_pPageLast(pPageLast) {
				// Find first page with data
				if constexpr (!IsFwd) {
					m_it = m_pPage->rbegin();
					while (m_it == m_pPage->rend()) {
						if (m_pPage == m_pPageLast)
							break;
						--m_pPage;
						m_it = m_pPage->rbegin();
					}
				} else {
					m_it = m_pPage->begin();
					while (m_it == m_pPage->end()) {
						if (m_pPage == m_pPageLast)
							break;
						++m_pPage;
						m_it = m_pPage->begin();
					}
				}
			}

			//! Accesses the current element view.
			//! \return Read-only view of the current element.
			value_type operator*() const {
				return m_it.operator*();
			}
			//! Accesses the current element view through arrow syntax.
			//! \return Read-only view of the current element.
			value_type operator->() const {
				return m_it.operator->();
			}

			//! Advances to the next element.
			//! \return Reference to this advanced iterator.
			iterator& operator++() {
				if constexpr (!IsFwd) {
					++m_it;
					while (m_it == m_pPage->rend()) {
						--m_pPage;
						m_it = m_pPage->rbegin();
					}
				} else {
					++m_it;
					while (m_it == m_pPage->end()) {
						++m_pPage;
						m_it = m_pPage->begin();
					}
				}
				return *this;
			}
			//! Advances to the next element.
			//! \return Iterator value before advancement.
			iterator operator++(int) {
				iterator temp(*this);
				++*this;
				return temp;
			}

			//! Checks whether two iterators have the same position.
			//! \param other Iterator to compare with.
			//! \return True if both iterators have the same position.
			GAIA_NODISCARD bool operator==(const iterator& other) const {
				return m_pPage == other.m_pPage && m_it == other.m_it;
			}
			//! Checks whether two iterators have different positions.
			//! \param other Iterator to compare with.
			//! \return True if the iterators have different positions.
			GAIA_NODISCARD bool operator!=(const iterator& other) const {
				return m_pPage != other.m_pPage || m_it != other.m_it;
			}
		};

		//! Heap-allocated paged storage for elements of type \a T.
		//! \tparam T Element type.
		template <typename T>
		class page_storage {
		public:
			//! Element type.
			using value_type = T;
			//! Reference to an element.
			using reference = T&;
			//! Reference to a read-only element.
			using const_reference = const T&;
			//! Pointer to an element.
			using pointer = T*;
			//! Pointer to a read-only element.
			using const_pointer = const T*;
			//! Data-layout access policy for the element type.
			using view_policy = mem::auto_view_policy<T>;
			//! Type used for iterator distances.
			using difference_type = detail::difference_type;
			//! Type used for sizes and indices.
			using size_type = detail::size_type;

			//! Size in bytes of one allocator block.
			static constexpr uint32_t AllocatorBlockSize = (uint32_t)sizeof(detail::mem_page_data<T>);
			//! Allocator used for page data.
			using Allocator = mem::PagedAllocator<T, AllocatorBlockSize>;

			//! Raw page-data type.
			using page_data_type = detail::mem_page_data<T>;
			//! Page type used by the storage.
			using page_type = detail::mem_page<T, Allocator>;
			//! Maximum number of elements addressable in one page.
			static constexpr uint32_t PageCapacity = page_type::PageCapacity;

			//! Forward mutable iterator.
			using iterator = page_iterator<T, Allocator, true>;
			//! Reverse mutable iterator.
			using iterator_reverse = page_iterator<T, Allocator, false>;
			//! Forward mutable iterator for structure-of-arrays elements.
			using iterator_soa = page_iterator_soa<T, Allocator, true>;
			//! Reverse mutable iterator for structure-of-arrays elements.
			using iterator_soa_reverse = page_iterator_soa<T, Allocator, false>;
			//! Forward read-only iterator.
			using const_iterator = const_page_iterator<T, Allocator, true>;
			//! Reverse read-only iterator.
			using const_iterator_reverse = const_page_iterator<T, Allocator, false>;
			//! Forward read-only iterator for structure-of-arrays elements.
			using const_iterator_soa = const_page_iterator_soa<T, Allocator, true>;
			//! Reverse read-only iterator for structure-of-arrays elements.
			using const_iterator_soa_reverse = const_page_iterator_soa<T, Allocator, false>;
			//! Iterator category tag.
			using iterator_category = core::bidirectional_iterator_tag;

		private:
			constexpr static uint32_t PageMask = PageCapacity - 1;
			constexpr static uint32_t ToPageIndex = core::count_bits(PageMask);

			//! Contains pages with data
			cnt::darray<page_type> m_pages;
			//! Total number of items stored in all pages
			uint32_t m_itemCnt = 0;

			void try_grow(uint32_t pid) {
				// The page array has to be able to take any page index
				if (pid >= m_pages.size())
					m_pages.resize(pid + 1);

				m_pages[pid].add();
				++m_itemCnt;
			}

		public:
			constexpr page_storage() noexcept = default;

			//! Copy-constructs a storage.
			//! \param other Storage to copy.
			page_storage(const page_storage& other) {
				m_pages = other.m_pages;
				m_itemCnt = other.m_itemCnt;
			}

			//! Copy-assigns a storage.
			//! \param other Storage to copy.
			//! \return Reference to this storage.
			page_storage& operator=(const page_storage& other) {
				GAIA_ASSERT(core::addressof(other) != this);

				m_pages = other.m_pages;
				m_itemCnt = other.m_itemCnt;
				return *this;
			}

			//! Move-constructs a storage.
			//! \param other Storage to move from.
			page_storage(page_storage&& other) noexcept {
				m_pages = GAIA_MOV(other.m_pages);
				m_itemCnt = other.m_itemCnt;

				other.m_pages = {};
			}

			//! Move-assigns a storage.
			//! \param other Storage to move from.
			//! \return Reference to this storage.
			page_storage& operator=(page_storage&& other) noexcept {
				GAIA_ASSERT(core::addressof(other) != this);

				m_pages = GAIA_MOV(other.m_pages);
				m_itemCnt = other.m_itemCnt;

				other.m_pages = {};

				return *this;
			}

			~page_storage() = default;

			GAIA_CLANG_WARNING_PUSH()
			// Memory is aligned so we can silence this warning
			GAIA_CLANG_WARNING_DISABLE("-Wcast-align")

			//! Accesses an element by identifier.
			//! \param id Identifier of an existing element.
			//! \return Mutable element reference or structure-of-arrays view.
			GAIA_NODISCARD decltype(auto) operator[](page_storage_id id) noexcept {
				GAIA_ASSERT(has(id));
				const auto pid = size_type(id >> ToPageIndex);
				const auto did = size_type(id & PageMask);
				auto& page = m_pages[pid];
				return view_policy::set({(typename view_policy::TargetCastType)page.data(), PageCapacity}, did);
			}

			//! Accesses an element by identifier.
			//! \param id Identifier of an existing element.
			//! \return Read-only element reference or structure-of-arrays view.
			GAIA_NODISCARD decltype(auto) operator[](page_storage_id id) const noexcept {
				GAIA_ASSERT(has(id));
				const auto pid = size_type(id >> ToPageIndex);
				const auto did = size_type(id & PageMask);
				auto& page = m_pages[pid];
				return view_policy::get({(typename view_policy::TargetCastType)page.data(), PageCapacity}, did);
			}

			GAIA_CLANG_WARNING_POP()

			//! Checks if an item with a given page \a id exists
			//! \param id Page id
			//! \return True if an item with \p id exists.
			GAIA_NODISCARD bool has(page_storage_id id) const noexcept {
				const auto pid = size_type(id >> ToPageIndex);
				if (pid >= m_pages.size())
					return false;

				const auto did = size_type(id & PageMask);
				const auto val = page_data_type::bit_set::BitCount;
				return did < val && m_pages[pid].has_data(did);
			}

			//! Checks if an item \a arg exists within the storage
			//! \param arg Data
			//! \return True if \p arg exists in the storage.
			GAIA_NODISCARD bool has(const T& arg) const noexcept {
				const auto id = to_page_storage_id<T>::get(arg);
				GAIA_ASSERT(id != detail::InvalidPageStorageId);
				return has(id);
			}

			//! Inserts the item \a arg into the storage.
			//! \tparam TType Type of the forwarded item.
			//! \param arg Data
			//! \return Reference to the inserted record, or nothing for a structure-of-arrays layout.
			template <typename TType>
			decltype(auto) add(TType&& arg) {
				const auto id = to_page_storage_id<T>::get(arg);
				if (has(id)) {
					if constexpr (mem::is_soa_layout_v<TType>)
						return;
					else {
						const auto pid = size_type(id >> ToPageIndex);
						const auto did = size_type(id & PageMask);
						auto& page = m_pages[pid];
						return page.set_data(did);
					}
				}

				const auto pid = size_type(id >> ToPageIndex);
				const auto did = size_type(id & PageMask);

				try_grow(pid);

				auto& page = m_pages[pid];
				if constexpr (mem::is_soa_layout_v<TType>)
					page.add_data(did, GAIA_FWD(arg));
				else
					return page.add_data(did, GAIA_FWD(arg));
			}

			//! Accesses the record at the index \a id for update.
			//! \param id Page id
			//! \return Mutable record reference or structure-of-arrays view.
			decltype(auto) set(page_storage_id id) {
				GAIA_ASSERT(has(id));

				const auto pid = uint32_t(id >> ToPageIndex);
				const auto did = uint32_t(id & PageMask);

				auto& page = m_pages[pid];
				return page.set_data(did);
			}

			//! Removes the item at the index \a id from the storage.
			//! \param id Page id
			void del(page_storage_id id) noexcept {
				GAIA_ASSERT(!empty());
				GAIA_ASSERT(id != detail::InvalidPageStorageId);

				if (!has(id))
					return;

				const auto pid = uint32_t(id >> ToPageIndex);
				const auto did = uint32_t(id & PageMask);

				auto& page = m_pages[pid];
				page.del_data(did);
				--m_itemCnt;
			}

			//! Removes the item \a arg from the storage.
			//! \param arg Data
			void del(const T& arg) noexcept {
				const auto id = to_page_storage_id<T>::get(arg);
				return del(id);
			}

			//! Clears the storage.
			void clear() {
				m_pages.resize(0);
				m_itemCnt = 0;
			}

			//! Returns the number of items inserted into the storage.
			//! \return Number of stored items.
			GAIA_NODISCARD size_type size() const noexcept {
				return m_itemCnt;
			}

			//! Checks if the storage is empty (no items inserted).
			//! \return True if the storage contains no items.
			GAIA_NODISCARD bool empty() const noexcept {
				return m_itemCnt == 0;
			}

			//! Accesses the first stored element.
			//! \return Mutable reference to the first element.
			GAIA_NODISCARD decltype(auto) front() noexcept {
				GAIA_ASSERT(!empty());
				return (reference)*begin();
			}

			//! Accesses the first stored element.
			//! \return Read-only reference to the first element.
			GAIA_NODISCARD decltype(auto) front() const noexcept {
				GAIA_ASSERT(!empty());
				return (const_reference)*begin();
			}

			//! Accesses the last stored element.
			//! \return Mutable reference to the last element.
			GAIA_NODISCARD decltype(auto) back() noexcept {
				GAIA_ASSERT(!empty());
				return (reference)*rbegin();
			}

			//! Accesses the last stored element.
			//! \return Read-only reference to the last element.
			GAIA_NODISCARD decltype(auto) back() const noexcept {
				GAIA_ASSERT(!empty());
				return (const_reference)*rbegin();
			}

			//! Returns an iterator to the first stored element.
			//! \return Mutable iterator to the first element.
			GAIA_NODISCARD auto begin() noexcept {
				GAIA_ASSERT(!empty());
				return iterator(m_pages.data(), m_pages.data() + m_pages.size());
			}

			//! Returns an iterator to the first stored element.
			//! \return Read-only iterator to the first element.
			GAIA_NODISCARD auto begin() const noexcept {
				GAIA_ASSERT(!empty());
				return const_iterator(m_pages.data(), m_pages.data() + m_pages.size());
			}

			//! Returns a read-only iterator to the first stored element.
			//! \return Read-only iterator to the first element.
			GAIA_NODISCARD auto cbegin() const noexcept {
				GAIA_ASSERT(!empty());
				return const_iterator(m_pages.data(), m_pages.data() + m_pages.size());
			}

			//! Returns an iterator past the last stored element.
			//! \return Mutable end iterator.
			GAIA_NODISCARD auto end() noexcept {
				GAIA_ASSERT(!empty());
				return iterator(m_pages.data() + m_pages.size());
			}

			//! Returns an iterator past the last stored element.
			//! \return Read-only end iterator.
			GAIA_NODISCARD auto end() const noexcept {
				GAIA_ASSERT(!empty());
				return const_iterator(m_pages.data() + m_pages.size());
			}

			//! Returns a read-only iterator past the last stored element.
			//! \return Read-only end iterator.
			GAIA_NODISCARD auto cend() const noexcept {
				GAIA_ASSERT(!empty());
				return const_iterator(m_pages.data() + m_pages.size());
			}

			//! Returns a reverse iterator to the last stored element.
			//! \return Mutable reverse iterator to the last element.
			GAIA_NODISCARD auto rbegin() noexcept {
				GAIA_ASSERT(!empty());
				return iterator_reverse(m_pages.data() + m_pages.size() - 1, m_pages.data() - 1);
			}

			//! Returns a reverse iterator to the last stored element.
			//! \return Read-only reverse iterator to the last element.
			GAIA_NODISCARD auto rbegin() const noexcept {
				GAIA_ASSERT(!empty());
				return const_iterator_reverse(m_pages.data() + m_pages.size() - 1, m_pages.data() - 1);
			}

			//! Returns a read-only reverse iterator to the last stored element.
			//! \return Read-only reverse iterator to the last element.
			GAIA_NODISCARD auto crbegin() const noexcept {
				GAIA_ASSERT(!empty());
				return const_iterator_reverse(m_pages.data() + m_pages.size() - 1, m_pages.data() - 1);
			}

			//! Returns a reverse iterator before the first stored element.
			//! \return Mutable reverse end iterator.
			GAIA_NODISCARD auto rend() noexcept {
				GAIA_ASSERT(!empty());
				return iterator_reverse(m_pages.data() - 1);
			}

			//! Returns a reverse iterator before the first stored element.
			//! \return Read-only reverse end iterator.
			GAIA_NODISCARD auto rend() const noexcept {
				GAIA_ASSERT(!empty());
				return const_iterator_reverse(m_pages.data() - 1);
			}

			//! Returns a read-only reverse iterator before the first stored element.
			//! \return Read-only reverse end iterator.
			GAIA_NODISCARD auto crend() const noexcept {
				GAIA_ASSERT(!empty());
				return const_iterator_reverse(m_pages.data() - 1);
			}

			//! Checks whether two storages contain equal elements.
			//! \param other Storage to compare with.
			//! \return True if both storages contain equal elements.
			GAIA_NODISCARD bool operator==(const page_storage& other) const noexcept {
				return m_pages == other.m_pages;
			}

			//! Checks whether two storages differ.
			//! \param other Storage to compare with.
			//! \return True if the storages differ.
			GAIA_NODISCARD bool operator!=(const page_storage& other) const noexcept {
				return !operator==(other);
			}
		};
	} // namespace cnt

} // namespace gaia
