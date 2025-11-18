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
		using page_storage_id = uint32_t;

		namespace detail {
			using difference_type = uint32_t;
			using size_type = uint32_t;

			constexpr static page_storage_id InvalidPageStorageId = (page_storage_id)-1;

			template <typename T>
			struct mem_page_data;
			template <typename T, typename Allocator>
			class mem_page;
		} // namespace detail

		template <typename T>
		struct to_page_storage_id {
			static page_storage_id get(const T& item) noexcept {
				(void)item;
				static_assert(
						std::is_empty_v<T>,
						"Sparse_storage items require a conversion function to be defined in gaia::cnt namespace");
				return detail::InvalidPageStorageId;
			}
		};

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

		template <typename T, typename Allocator, bool IsFwd>
		struct page_iterator {
			using value_type = T;
			using pointer = T*;
			using reference = T&;
			using difference_type = detail::difference_type;
			using size_type = detail::size_type;
			using iterator = page_iterator;
			using iterator_category = core::bidirectional_iterator_tag;

		private:
			using page_type = detail::mem_page<T, Allocator>;

			page_type* m_pPage;
			page_type* m_pPageLast;
			typename page_type::template iterator_base<IsFwd> m_it;

		public:
			page_iterator(page_type* pPage): m_pPage(pPage), m_pPageLast(pPage) {}

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

			reference operator*() const {
				return m_it.operator*();
			}
			pointer operator->() const {
				return m_it.operator->();
			}

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
			iterator operator++(int) {
				iterator temp(*this);
				++*this;
				return temp;
			}

			GAIA_NODISCARD bool operator==(const iterator& other) const {
				return m_pPage == other.m_pPage && m_it == other.m_it;
			}
			GAIA_NODISCARD bool operator!=(const iterator& other) const {
				return m_pPage != other.m_pPage || m_it != other.m_it;
			}
		};

		template <typename T, typename Allocator, bool IsFwd>
		struct const_page_iterator {
			using value_type = T;
			using pointer = const T*;
			using reference = const T&;
			using difference_type = detail::difference_type;
			using size_type = detail::size_type;
			using iterator = const_page_iterator;
			using iterator_category = core::bidirectional_iterator_tag;

		private:
			using page_type = detail::mem_page<T, Allocator>;

			const page_type* m_pPage;
			const page_type* m_pPageLast;
			typename page_type::template iterator_base<IsFwd> m_it;

		public:
			const_page_iterator(const page_type* pPage): m_pPage(pPage), m_pPageLast(pPage) {}

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

			reference operator*() const {
				return m_it.operator*();
			}
			pointer operator->() const {
				return m_it.operator->();
			}

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
			iterator operator++(int) {
				iterator temp(*this);
				++*this;
				return temp;
			}

			GAIA_NODISCARD bool operator==(const iterator& other) const {
				return m_pPage == other.m_pPage && m_it == other.m_it;
			}
			GAIA_NODISCARD bool operator!=(const iterator& other) const {
				return m_pPage != other.m_pPage || m_it != other.m_it;
			}
		};

		template <typename T, typename Allocator, bool IsFwd>
		struct page_iterator_soa {
			using value_type = T;
			// using pointer = T*;
			// using reference = T&;
			using difference_type = detail::difference_type;
			using size_type = detail::size_type;
			using iterator = page_iterator_soa;
			using iterator_category = core::bidirectional_iterator_tag;

		private:
			using page_type = detail::mem_page<T, Allocator>;

			page_type* m_pPage;
			page_type* m_pPageLast;
			typename page_type::template iterator_soa_base<IsFwd> m_it;

		public:
			page_iterator_soa(page_type* pPage): m_pPage(pPage), m_pPageLast(pPage) {}

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

			value_type operator*() const {
				return m_it.operator*();
			}
			value_type operator->() const {
				return m_it.operator->();
			}

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
			iterator operator++(int) {
				iterator temp(*this);
				++*this;
				return temp;
			}

			GAIA_NODISCARD bool operator==(const iterator& other) const {
				return m_pPage == other.m_pPage && m_it == other.m_it;
			}
			GAIA_NODISCARD bool operator!=(const iterator& other) const {
				return m_pPage != other.m_pPage || m_it != other.m_it;
			}
		};

		template <typename T, typename Allocator, bool IsFwd>
		struct const_page_iterator_soa {
			using value_type = T;
			// using pointer = T*;
			// using reference = T&;
			using difference_type = detail::difference_type;
			using size_type = detail::size_type;
			using iterator = const_page_iterator_soa;
			using iterator_category = core::bidirectional_iterator_tag;

		private:
			using page_type = detail::mem_page<T, Allocator>;

			const page_type* m_pPage;
			const page_type* m_pPageLast;
			typename page_type::template iterator_soa_base<IsFwd> m_it;

		public:
			const_page_iterator_soa(const page_type* pPage): m_pPage(pPage), m_pPageLast(pPage) {}

			const_page_iterator_soa(const page_type* pPage, const page_type* pPageLast): m_pPage(pPage), m_pPageLast(pPageLast) {
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

			value_type operator*() const {
				return m_it.operator*();
			}
			value_type operator->() const {
				return m_it.operator->();
			}

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
			iterator operator++(int) {
				iterator temp(*this);
				++*this;
				return temp;
			}

			GAIA_NODISCARD bool operator==(const iterator& other) const {
				return m_pPage == other.m_pPage && m_it == other.m_it;
			}
			GAIA_NODISCARD bool operator!=(const iterator& other) const {
				return m_pPage != other.m_pPage || m_it != other.m_it;
			}
		};

		//! Array with variable size of elements of type \tparam T allocated on heap.
		//! Allocates enough memory to support \tparam PageCapacity elements.
		//! Uses \tparam Allocator to allocate memory.
		template <typename T>
		class page_storage {
		public:
			using value_type = T;
			using reference = T&;
			using const_reference = const T&;
			using pointer = T*;
			using const_pointer = const T*;
			using view_policy = mem::auto_view_policy<T>;
			using difference_type = detail::difference_type;
			using size_type = detail::size_type;

			static constexpr uint32_t AllocatorBlockSize = (uint32_t)sizeof(detail::mem_page_data<T>);
			using Allocator = mem::PagedAllocator<T, AllocatorBlockSize>;

			using page_data_type = detail::mem_page_data<T>;
			using page_type = detail::mem_page<T, Allocator>;
			static constexpr uint32_t PageCapacity = page_type::PageCapacity;

			using iterator = page_iterator<T, Allocator, true>;
			using iterator_reverse = page_iterator<T, Allocator, false>;
			using iterator_soa = page_iterator_soa<T, Allocator, true>;
			using iterator_soa_reverse = page_iterator_soa<T, Allocator, false>;
			using const_iterator = const_page_iterator<T, Allocator, true>;
			using const_iterator_reverse = const_page_iterator<T, Allocator, false>;
			using const_iterator_soa = const_page_iterator_soa<T, Allocator, true>;
			using const_iterator_soa_reverse = const_page_iterator_soa<T, Allocator, false>;
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

			page_storage(const page_storage& other) {
				m_pages = other.m_pages;
				m_itemCnt = other.m_itemCnt;
			}

			page_storage& operator=(const page_storage& other) {
				GAIA_ASSERT(core::addressof(other) != this);

				m_pages = other.m_pages;
				m_itemCnt = other.m_itemCnt;
				return *this;
			}

			page_storage(page_storage&& other) noexcept {
				m_pages = GAIA_MOV(other.m_pages);
				m_itemCnt = other.m_itemCnt;

				other.m_pages = {};
			}

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

			GAIA_NODISCARD decltype(auto) operator[](page_storage_id id) noexcept {
				GAIA_ASSERT(has(id));
				const auto pid = size_type(id >> ToPageIndex);
				const auto did = size_type(id & PageMask);
				auto& page = m_pages[pid];
				return view_policy::set({(typename view_policy::TargetCastType)page.data(), PageCapacity}, did);
			}

			GAIA_NODISCARD decltype(auto) operator[](page_storage_id id) const noexcept {
				GAIA_ASSERT(has(id));
				const auto pid = size_type(id >> ToPageIndex);
				const auto did = size_type(id & PageMask);
				auto& page = m_pages[pid];
				return view_policy::get({(typename view_policy::TargetCastType)page.data(), PageCapacity}, did);
			}

			GAIA_CLANG_WARNING_POP()

			//! Checks if an item with a given page id \param sid exists
			GAIA_NODISCARD bool has(page_storage_id id) const noexcept {
				const auto pid = size_type(id >> ToPageIndex);
				if (pid >= m_pages.size())
					return false;

				const auto did = size_type(id & PageMask);
				const auto val = page_data_type::bit_set::BitCount;
				return did < val && m_pages[pid].has_data(did);
			}

			//! Checks if an item \param arg exists within the storage
			GAIA_NODISCARD bool has(const T& arg) const noexcept {
				const auto id = to_page_storage_id<T>::get(arg);
				GAIA_ASSERT(id != detail::InvalidPageStorageId);
				return has(id);
			}

			//! Inserts the item \param arg into the storage.
			//! \return Reference to the inserted record or nothing in case it is has a SoA layout.
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

			//! Update the record at the index \param id.
			//! \return Reference to the inserted record or nothing in case it is has a SoA layout.
			decltype(auto) set(page_storage_id id) {
				GAIA_ASSERT(has(id));

				const auto pid = uint32_t(id >> ToPageIndex);
				const auto did = uint32_t(id & PageMask);

				auto& page = m_pages[pid];
				return page.set_data(did);
			}

			//! Removes the item at the index \param id from the storage.
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

			//! Removes the item \param arg from the storage.
			void del(const T& arg) noexcept {
				const auto id = to_page_storage_id<T>::get(arg);
				return del(id);
			}

			//! Clears the storage
			void clear() {
				m_pages.resize(0);
				m_itemCnt = 0;
			}

			//! Returns the number of items inserted into the storage
			GAIA_NODISCARD size_type size() const noexcept {
				return m_itemCnt;
			}

			//! Checks if the storage is empty (no items inserted)
			GAIA_NODISCARD bool empty() const noexcept {
				return m_itemCnt == 0;
			}

			GAIA_NODISCARD decltype(auto) front() noexcept {
				GAIA_ASSERT(!empty());
				return (reference)*begin();
			}

			GAIA_NODISCARD decltype(auto) front() const noexcept {
				GAIA_ASSERT(!empty());
				return (const_reference)*begin();
			}

			GAIA_NODISCARD decltype(auto) back() noexcept {
				GAIA_ASSERT(!empty());
				return (reference)*rbegin();
			}

			GAIA_NODISCARD decltype(auto) back() const noexcept {
				GAIA_ASSERT(!empty());
				return (const_reference)*rbegin();
			}

			GAIA_NODISCARD auto begin() noexcept {
				GAIA_ASSERT(!empty());
				return iterator(m_pages.data(), m_pages.data() + m_pages.size());
			}

			GAIA_NODISCARD auto begin() const noexcept {
				GAIA_ASSERT(!empty());
				return const_iterator(m_pages.data(), m_pages.data() + m_pages.size());
			}

			GAIA_NODISCARD auto cbegin() const noexcept {
				GAIA_ASSERT(!empty());
				return const_iterator(m_pages.data(), m_pages.data() + m_pages.size());
			}

			GAIA_NODISCARD auto end() noexcept {
				GAIA_ASSERT(!empty());
				return iterator(m_pages.data() + m_pages.size());
			}

			GAIA_NODISCARD auto end() const noexcept {
				GAIA_ASSERT(!empty());
				return const_iterator(m_pages.data() + m_pages.size());
			}

			GAIA_NODISCARD auto cend() const noexcept {
				GAIA_ASSERT(!empty());
				return const_iterator(m_pages.data() + m_pages.size());
			}

			GAIA_NODISCARD auto rbegin() noexcept {
				GAIA_ASSERT(!empty());
				return iterator_reverse(m_pages.data() + m_pages.size() - 1, m_pages.data() - 1);
			}

			GAIA_NODISCARD auto rbegin() const noexcept {
				GAIA_ASSERT(!empty());
				return const_iterator_reverse(m_pages.data() + m_pages.size() - 1, m_pages.data() - 1);
			}

			GAIA_NODISCARD auto crbegin() const noexcept {
				GAIA_ASSERT(!empty());
				return const_iterator_reverse(m_pages.data() + m_pages.size() - 1, m_pages.data() - 1);
			}

			GAIA_NODISCARD auto rend() noexcept {
				GAIA_ASSERT(!empty());
				return iterator_reverse(m_pages.data() - 1);
			}

			GAIA_NODISCARD auto rend() const noexcept {
				GAIA_ASSERT(!empty());
				return const_iterator_reverse(m_pages.data() - 1);
			}

			GAIA_NODISCARD auto crend() const noexcept {
				GAIA_ASSERT(!empty());
				return const_iterator_reverse(m_pages.data() - 1);
			}

			GAIA_NODISCARD bool operator==(const page_storage& other) const noexcept {
				return m_pages == other.m_pages;
			}

			GAIA_NODISCARD bool operator!=(const page_storage& other) const noexcept {
				return !operator==(other);
			}
		};
	} // namespace cnt

} // namespace gaia