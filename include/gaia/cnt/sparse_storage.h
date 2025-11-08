#pragma once
#include "../config/config.h"

#include <cstddef>
#include <initializer_list>
#include <type_traits>
#include <utility>

#include "../core/iterator.h"
#include "../core/utility.h"
#include "../mem/data_layout_policy.h"
#include "../mem/mem_utils.h"
#include "darray.h"

namespace gaia {
	namespace cnt {
		using sparse_id = uint64_t;

		namespace detail {
			using difference_type = uint32_t;
			using size_type = uint32_t;

			constexpr static sparse_id InvalidSparseId = (sparse_id)-1;
			constexpr static size_type InvalidDenseId = BadIndex - 1;

			template <typename T, uint32_t PageCapacity, typename Allocator, typename>
			class sparse_page;
		} // namespace detail

		template <typename T>
		struct to_sparse_id {
			static sparse_id get(const T& item) noexcept {
				(void)item;
				static_assert(
						std::is_empty_v<T>,
						"Sparse_storage items require a conversion function to be defined in gaia::cnt namespace");
				return detail::InvalidSparseId;
			}
		};

		template <typename T, uint32_t PageCapacity, typename Allocator, typename = void>
		struct sparse_iterator {
			using iterator_category = core::random_access_iterator_tag;
			using value_type = T;
			using pointer = T*;
			using reference = T&;
			using difference_type = detail::difference_type;
			using size_type = detail::size_type;
			using iterator = sparse_iterator;

		private:
			static_assert((PageCapacity & (PageCapacity - 1)) == 0, "PageCapacity of sparse_iterator must be a power of 2");
			constexpr static sparse_id page_mask = PageCapacity - 1;
			constexpr static sparse_id to_page_index = core::count_bits(page_mask);

			using page_type = detail::sparse_page<T, PageCapacity, Allocator, void>;

			sparse_id* m_pDense;
			page_type* m_pPages;

		public:
			sparse_iterator(sparse_id* pDense, page_type* pPages): m_pDense(pDense), m_pPages(pPages) {}

			reference operator*() const {
				const auto sid = *m_pDense;
				const auto pid = uint32_t(sid >> to_page_index);
				const auto did = uint32_t(sid & page_mask);
				auto& page = m_pPages[pid];
				return page.set_data(did);
			}
			pointer operator->() const {
				const auto sid = *m_pDense;
				const auto pid = uint32_t(sid >> to_page_index);
				const auto did = uint32_t(sid & page_mask);
				auto& page = m_pPages[pid];
				return &page.set_data(did);
			}
			iterator operator[](size_type offset) const {
				return {m_pDense + offset, m_pPages};
			}

			iterator& operator+=(size_type diff) {
				m_pDense += diff;
				return *this;
			}
			iterator& operator-=(size_type diff) {
				m_pDense -= diff;
				return *this;
			}
			iterator& operator++() {
				++m_pDense;
				return *this;
			}
			iterator operator++(int) {
				iterator temp(*this);
				++*this;
				return temp;
			}
			iterator& operator--() {
				--m_pDense;
				return *this;
			}
			iterator operator--(int) {
				iterator temp(*this);
				--*this;
				return temp;
			}

			iterator operator+(size_type offset) const {
				return {m_pDense + offset, m_pPages};
			}
			iterator operator-(size_type offset) const {
				return {m_pDense - offset, m_pPages};
			}
			difference_type operator-(const iterator& other) const {
				return (difference_type)(m_pDense - other.m_pDense);
			}

			GAIA_NODISCARD bool operator==(const iterator& other) const {
				return m_pDense == other.m_pDense;
			}
			GAIA_NODISCARD bool operator!=(const iterator& other) const {
				return m_pDense != other.m_pDense;
			}
			GAIA_NODISCARD bool operator>(const iterator& other) const {
				return m_pDense > other.m_pDense;
			}
			GAIA_NODISCARD bool operator>=(const iterator& other) const {
				return m_pDense >= other.m_pDense;
			}
			GAIA_NODISCARD bool operator<(const iterator& other) const {
				return m_pDense < other.m_pDense;
			}
			GAIA_NODISCARD bool operator<=(const iterator& other) const {
				return m_pDense <= other.m_pDense;
			}
		};

		template <typename T, uint32_t PageCapacity, typename Allocator>
		struct sparse_iterator<T, PageCapacity, Allocator, std::enable_if_t<std::is_empty_v<T>>> {
			using iterator_category = core::random_access_iterator_tag;
			using value_type = sparse_id;
			// using pointer = sparse_id*; not supported
			// using reference = sparse_id&; not supported
			using difference_type = detail::difference_type;
			using size_type = detail::size_type;
			using iterator = sparse_iterator;

		private:
			static_assert((PageCapacity & (PageCapacity - 1)) == 0, "PageCapacity of sparse_iterator must be a power of 2");
			constexpr static sparse_id page_mask = PageCapacity - 1;
			constexpr static sparse_id to_page_index = core::count_bits(page_mask);

			using page_type = detail::sparse_page<T, PageCapacity, Allocator, std::enable_if_t<std::is_empty_v<T>>>;

			value_type* m_pDense;

		public:
			sparse_iterator(value_type* pDense): m_pDense(pDense) {}

			value_type operator*() const {
				const auto sid = *m_pDense;
				return sid;
			}
			value_type operator->() const {
				const auto sid = *m_pDense;
				return sid;
			}
			iterator operator[](size_type offset) const {
				return {m_pDense + offset};
			}

			iterator& operator+=(size_type diff) {
				m_pDense += diff;
				return *this;
			}
			iterator& operator-=(size_type diff) {
				m_pDense -= diff;
				return *this;
			}
			iterator& operator++() {
				++m_pDense;
				return *this;
			}
			iterator operator++(int) {
				iterator temp(*this);
				++*this;
				return temp;
			}
			iterator& operator--() {
				--m_pDense;
				return *this;
			}
			iterator operator--(int) {
				iterator temp(*this);
				--*this;
				return temp;
			}

			iterator operator+(size_type offset) const {
				return {m_pDense + offset};
			}
			iterator operator-(size_type offset) const {
				return {m_pDense - offset};
			}
			difference_type operator-(const iterator& other) const {
				return (difference_type)(m_pDense - other.m_pDense);
			}

			GAIA_NODISCARD bool operator==(const iterator& other) const {
				return m_pDense == other.m_pDense;
			}
			GAIA_NODISCARD bool operator!=(const iterator& other) const {
				return m_pDense != other.m_pDense;
			}
			GAIA_NODISCARD bool operator>(const iterator& other) const {
				return m_pDense > other.m_pDense;
			}
			GAIA_NODISCARD bool operator>=(const iterator& other) const {
				return m_pDense >= other.m_pDense;
			}
			GAIA_NODISCARD bool operator<(const iterator& other) const {
				return m_pDense < other.m_pDense;
			}
			GAIA_NODISCARD bool operator<=(const iterator& other) const {
				return m_pDense <= other.m_pDense;
			}
		};

		namespace detail {
			template <typename T, uint32_t PageCapacity, typename Allocator, typename = void>
			class sparse_page {
			public:
				using value_type = T;
				using reference = T&;
				using const_reference = const T&;
				using pointer = T*;
				using const_pointer = T*;
				using view_policy = mem::data_view_policy_aos<T>;
				using difference_type = detail::difference_type;
				using size_type = detail::size_type;

				using iterator = sparse_iterator<T, PageCapacity, Allocator>;

			private:
				size_type* m_pSparse = nullptr;
				uint8_t* m_pData = nullptr;
				size_type m_cnt = 0;

				void ensure() {
					if (m_pSparse != nullptr)
						return;

					// Allocate memory for sparse->dense index mapping.
					// Make sure initial values are detail::InvalidDenseId.
					m_pSparse = mem::AllocHelper::alloc<size_type>("SparsePage", PageCapacity);
					GAIA_FOR(PageCapacity) m_pSparse[i] = detail::InvalidDenseId;

					// Allocate memory for data
					m_pData = view_policy::template alloc<Allocator>(PageCapacity);
				}

				void dtr_data_inter(uint32_t idx) noexcept {
					GAIA_ASSERT(!empty());

					auto* ptr = &data()[idx];
					core::call_dtor(ptr);
				}

				void dtr_active_data() noexcept {
					GAIA_ASSERT(m_pSparse != nullptr);

					for (uint32_t i = 0; m_cnt != 0 && i != PageCapacity; ++i) {
						if (m_pSparse[i] == detail::InvalidDenseId)
							continue;

						auto* ptr = &data()[i];
						core::call_dtor(ptr);
					}
				}

				void invalidate() {
					if (m_pSparse == nullptr)
						return;

					GAIA_ASSERT(m_cnt > 0);

					// Destruct active items
					dtr_active_data();

					// Release allocated memory
					mem::AllocHelper::free("SparsePage", m_pSparse);
					view_policy::template free<Allocator>(m_pData, m_cnt);

					m_pSparse = nullptr;
					m_pData = nullptr;
					m_cnt = 0;
				}

			public:
				sparse_page() = default;

				sparse_page(const sparse_page& other) {
					// Copy new items over
					if (other.m_pSparse == nullptr) {
						invalidate();
					} else {
						ensure();

						for (uint32_t i = 0; i < PageCapacity; ++i) {
							// Copy indices
							m_pSparse[i] = other.m_pSparse[i];
							if (other.m_pSparse[i] == detail::InvalidDenseId)
								continue;

							// Copy construct data
							add_data(i, other.set_data(i));
						}

						m_cnt = other.m_cnt;
					}
				}

				sparse_page& operator=(const sparse_page& other) {
					GAIA_ASSERT(core::addressof(other) != this);

					if (other.m_pSparse == nullptr) {
						// If the other array is empty, let's just invalidate this one
						invalidate();
					} else {
						ensure();

						// Remove current active items
						if (m_pSparse != nullptr)
							dtr_active_data();

						// Copy new items over if there are any
						for (uint32_t i = 0; i < PageCapacity; ++i) {
							// Copy indices
							m_pSparse[i] = other.m_pSparse[i];
							if (other.m_pSparse[i] == detail::InvalidDenseId)
								continue;

							// Copy construct data
							add_data(i, other.get_data(i));
						}

						m_cnt = other.m_cnt;
					}

					return *this;
				}

				sparse_page(sparse_page&& other) noexcept {
					m_pSparse = other.m_pSparse;
					m_pData = other.m_pData;
					m_cnt = other.m_cnt;

					other.m_pSparse = nullptr;
					other.m_pData = nullptr;
					other.m_cnt = size_type(0);
				}

				sparse_page& operator=(sparse_page&& other) noexcept {
					GAIA_ASSERT(core::addressof(other) != this);

					invalidate();

					m_pSparse = other.m_pSparse;
					m_pData = other.m_pData;
					m_cnt = other.m_cnt;

					other.m_pSparse = nullptr;
					other.m_pData = nullptr;
					other.m_cnt = size_type(0);

					return *this;
				}

				~sparse_page() {
					invalidate();
				}

				GAIA_CLANG_WARNING_PUSH()
				// Memory is aligned so we can silence this warning
				GAIA_CLANG_WARNING_DISABLE("-Wcast-align")

				GAIA_NODISCARD pointer data() noexcept {
					return (pointer)m_pData;
				}

				GAIA_NODISCARD const_pointer data() const noexcept {
					return (const_pointer)m_pData;
				}

				GAIA_NODISCARD auto& set_id(size_type pos) noexcept {
					return m_pSparse[pos];
				}

				GAIA_NODISCARD auto get_id(size_type pos) const noexcept {
					return m_pSparse[pos];
				}

				GAIA_NODISCARD decltype(auto) set_data(size_type pos) noexcept {
					return view_policy::set({(typename view_policy::TargetCastType)m_pData, PageCapacity}, pos);
				}

				GAIA_NODISCARD decltype(auto) get_data(size_type pos) const noexcept {
					return view_policy::get({(typename view_policy::TargetCastType)m_pData, PageCapacity}, pos);
				}

				GAIA_CLANG_WARNING_POP()

				void add() {
					ensure();
					++m_cnt;
				}

				decltype(auto) add_data(uint32_t idx, const T& arg) {
					auto* ptr = &set_data(idx);
					core::call_ctor(ptr, arg);
					return (reference)(*ptr);
				}

				decltype(auto) add_data(uint32_t idx, T&& arg) {
					auto* ptr = &set_data(idx);
					core::call_ctor(ptr, GAIA_MOV(arg));
					return (reference)(*ptr);
				}

				void del_data(uint32_t idx) noexcept {
					dtr_data_inter(idx);

					GAIA_ASSERT(m_cnt > 0);
					--m_cnt;

					// If there is no more data, release the memory allocated by the page
					if (m_cnt == 0)
						invalidate();
				}

				GAIA_NODISCARD size_type size() const noexcept {
					return m_cnt;
				}

				GAIA_NODISCARD bool empty() const noexcept {
					return size() == 0;
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
					return (reference)(set_data(m_cnt - 1));
				}

				GAIA_NODISCARD decltype(auto) back() const noexcept {
					GAIA_ASSERT(!empty());
					return (const_reference)set_data(m_cnt - 1);
				}

				GAIA_NODISCARD auto begin() const noexcept {
					return iterator(data());
				}

				GAIA_NODISCARD auto rbegin() const noexcept {
					return iterator((pointer)&back());
				}

				GAIA_NODISCARD auto end() const noexcept {
					return iterator(data() + size());
				}

				GAIA_NODISCARD auto rend() const noexcept {
					return iterator(data() - 1);
				}

				GAIA_NODISCARD bool operator==(const sparse_page& other) const {
					if (m_cnt != other.m_cnt)
						return false;
					const size_type n = size();
					for (size_type i = 0; i < n; ++i)
						if (!(get_data(i) == other[i]))
							return false;
					return true;
				}

				GAIA_NODISCARD constexpr bool operator!=(const sparse_page& other) const {
					return !operator==(other);
				}
			};

			//! Sparse page. Specialized for zero-size \tparam T
			template <typename T, uint32_t PageCapacity, typename Allocator>
			class sparse_page<T, PageCapacity, Allocator, std::enable_if_t<std::is_empty_v<T>>> {
			public:
				using value_type = T;
				// using reference = T&; not supported
				// using const_reference = const T&; not supported
				using pointer = T*;
				using const_pointer = T*;
				using view_policy = mem::data_view_policy_aos<T>;
				using difference_type = detail::difference_type;
				using size_type = detail::size_type;

				using iterator = sparse_iterator<T, PageCapacity, Allocator, std::enable_if_t<std::is_empty_v<T>>>;

			private:
				size_type* m_pSparse = nullptr;
				size_type m_cnt = 0;

				void ensure() {
					if (m_pSparse == nullptr) {
						// Allocate memory for sparse->dense index mapping.
						// Make sure initial values are detail::InvalidId.
						m_pSparse = mem::AllocHelper::alloc<size_type>("SparsePage", PageCapacity);
						GAIA_FOR(PageCapacity) m_pSparse[i] = detail::InvalidDenseId;
					}
				}

				void dtr_data_inter([[maybe_unused]] uint32_t idx) noexcept {
					GAIA_ASSERT(!empty());
				}

				void dtr_active_data() noexcept {
					GAIA_ASSERT(m_pSparse != nullptr);
				}

				void invalidate() {
					if (m_pSparse == nullptr)
						return;

					// Release allocated memory
					mem::AllocHelper::free("SparsePage", m_pSparse);

					m_pSparse = nullptr;
					m_cnt = 0;
				}

			public:
				sparse_page() = default;

				sparse_page(const sparse_page& other) {
					// Copy new items over
					if (other.m_pSparse == nullptr) {
						invalidate();
					} else {
						for (uint32_t i = 0; i < PageCapacity; ++i) {
							// Copy indices
							m_pSparse[i] = other.m_pSparse[i];
							if (m_pSparse[i] == detail::InvalidDenseId)
								continue;
						}

						m_cnt = other.m_cnt;
					}
				}

				sparse_page& operator=(const sparse_page& other) {
					GAIA_ASSERT(core::addressof(other) != this);

					if (m_pSparse == nullptr && other.m_pSparse != nullptr)
						ensure();

					// Copy new items over if there are any
					if (other.m_pSparse == nullptr) {
						invalidate();
					} else {
						// Remove current active items
						if (m_pSparse != nullptr)
							dtr_active_data();

						// Copy indices
						for (uint32_t i = 0; i < PageCapacity; ++i)
							m_pSparse[i] = other.m_pSparse[i];

						m_cnt = other.m_cnt;
					}

					return *this;
				}

				sparse_page(sparse_page&& other) noexcept {
					// This is a newly constructed object.
					// It can't have any memory allocated, yet.
					GAIA_ASSERT(m_pSparse == nullptr);

					m_pSparse = other.m_pSparse;
					m_cnt = other.m_cnt;

					other.m_pSparse = nullptr;
					other.m_cnt = size_type(0);
				}

				sparse_page& operator=(sparse_page&& other) noexcept {
					GAIA_ASSERT(core::addressof(other) != this);

					invalidate();

					m_pSparse = other.m_pSparse;
					m_cnt = other.m_cnt;

					other.m_pSparse = nullptr;
					other.m_cnt = size_type(0);

					return *this;
				}

				~sparse_page() {
					invalidate();
				}

				GAIA_CLANG_WARNING_PUSH()
				// Memory is aligned so we can silence this warning
				GAIA_CLANG_WARNING_DISABLE("-Wcast-align")

				GAIA_NODISCARD pointer data() noexcept {
					return (pointer)m_pSparse;
				}

				GAIA_NODISCARD const_pointer data() const noexcept {
					return (const_pointer)m_pSparse;
				}

				GAIA_CLANG_WARNING_POP()

				void add() {
					ensure();
					++m_cnt;
				}

				GAIA_NODISCARD auto& set_id(size_type pos) noexcept {
					return m_pSparse[pos];
				}

				GAIA_NODISCARD auto get_id(size_type pos) const noexcept {
					return m_pSparse[pos];
				}

				void del_id(uint32_t idx) noexcept {
					dtr_data_inter(idx);

					GAIA_ASSERT(m_cnt > 0);
					--m_cnt;

					// If there is no more data, release the memory allocated by the page
					if (m_cnt == 0)
						invalidate();
				}

				GAIA_NODISCARD size_type size() const noexcept {
					return m_cnt;
				}

				GAIA_NODISCARD bool empty() const noexcept {
					return size() == 0;
				}

				GAIA_NODISCARD auto front() const noexcept {
					GAIA_ASSERT(!empty());
					return *begin();
				}

				GAIA_NODISCARD auto back() const noexcept {
					GAIA_ASSERT(!empty());
					return get_id(m_cnt - 1);
				}

				GAIA_NODISCARD auto begin() const noexcept {
					return iterator(data());
				}

				GAIA_NODISCARD auto rbegin() const noexcept {
					return iterator((pointer)&back());
				}

				GAIA_NODISCARD auto end() const noexcept {
					return iterator(data() + size());
				}

				GAIA_NODISCARD auto rend() const noexcept {
					return iterator(data() - 1);
				}

				GAIA_NODISCARD bool operator==(const sparse_page& other) const {
					if (m_cnt != other.m_cnt)
						return false;
					const size_type n = size();
					for (size_type i = 0; i < n; ++i)
						if (!(get_id(i) == other.get_id(i)))
							return false;
					return true;
				}

				GAIA_NODISCARD constexpr bool operator!=(const sparse_page& other) const {
					return !operator==(other);
				}
			};
		} // namespace detail

		//! Array with variable size of elements of type \tparam T allocated on heap.
		//! Allocates enough memory to support \tparam PageCapacity elements.
		//! Uses \tparam Allocator to allocate memory.
		template <
				typename T, uint32_t PageCapacity = 4096, typename Allocator = mem::DefaultAllocatorAdaptor, typename = void>
		class sparse_storage {
		public:
			using value_type = T;
			using reference = T&;
			using const_reference = const T&;
			using pointer = T*;
			using const_pointer = T*;
			using view_policy = mem::data_view_policy_aos<T>;
			using difference_type = detail::difference_type;
			using size_type = detail::size_type;

			using iterator = sparse_iterator<T, PageCapacity, Allocator>;
			using page_type = detail::sparse_page<T, PageCapacity, Allocator>;

		private:
			static_assert((PageCapacity & (PageCapacity - 1)) == 0, "PageCapacity of sparse_storage must be a power of 2");
			constexpr static sparse_id page_mask = PageCapacity - 1;
			constexpr static sparse_id to_page_index = core::count_bits(page_mask);

			//! Contains mappings to sparse storage inside pages
			cnt::darray<sparse_id> m_dense;
			//! Contains pages with data and sparse–>dense mapping
			cnt::darray<page_type> m_pages;
			//! Current number of items tracked by the sparse set
			size_type m_cnt = size_type(0);

			void try_grow(uint32_t pid) {
				m_dense.resize(m_cnt + 1);

				// The sparse array has to be able to take any sparse index
				if (pid >= m_pages.size())
					m_pages.resize(pid + 1);

				m_pages[pid].add();
			}

		public:
			constexpr sparse_storage() noexcept = default;

			sparse_storage(const sparse_storage& other) {
				GAIA_ASSERT(core::addressof(other) != this);

				m_dense = other.m_dense;
				m_pages = other.m_pages;
				m_cnt = other.m_cnt;
			}

			sparse_storage& operator=(const sparse_storage& other) {
				GAIA_ASSERT(core::addressof(other) != this);

				m_dense = other.m_dense;
				m_pages = other.m_pages;
				m_cnt = other.m_cnt;

				return *this;
			}

			sparse_storage(sparse_storage&& other) noexcept {
				// This is a newly constructed object.
				// It can't have any memory allocated, yet.
				GAIA_ASSERT(m_dense.data() == nullptr);

				m_dense = GAIA_MOV(other.m_dense);
				m_pages = GAIA_MOV(other.m_pages);
				m_cnt = other.m_cnt;

				other.m_dense = {};
				other.m_pages = {};
				other.m_cnt = size_type(0);
			}

			sparse_storage& operator=(sparse_storage&& other) noexcept {
				GAIA_ASSERT(core::addressof(other) != this);

				m_dense = GAIA_MOV(other.m_dense);
				m_pages = GAIA_MOV(other.m_pages);
				m_cnt = other.m_cnt;

				other.m_dense = {};
				other.m_pages = {};
				other.m_cnt = size_type(0);

				return *this;
			}

			~sparse_storage() = default;

			GAIA_CLANG_WARNING_PUSH()
			// Memory is aligned so we can silence this warning
			GAIA_CLANG_WARNING_DISABLE("-Wcast-align")

			GAIA_NODISCARD decltype(auto) operator[](sparse_id sid) noexcept {
				GAIA_ASSERT(has(sid));
				const auto pid = uint32_t(sid >> to_page_index);
				const auto did = uint32_t(sid & page_mask);

				auto& page = m_pages[pid];
				return view_policy::set({(typename view_policy::TargetCastType)page.data(), PageCapacity}, did);
			}

			GAIA_NODISCARD decltype(auto) operator[](sparse_id sid) const noexcept {
				GAIA_ASSERT(has(sid));
				const auto pid = uint32_t(sid >> to_page_index);
				const auto did = uint32_t(sid & page_mask);

				auto& page = m_pages[pid];
				return view_policy::get({(typename view_policy::TargetCastType)page.data(), PageCapacity}, did);
			}

			GAIA_CLANG_WARNING_POP()

			//! Checks if an item with a given sparse id \param sid exists
			GAIA_NODISCARD bool has(sparse_id sid) const {
				if (sid == detail::InvalidSparseId)
					return false;

				const auto pid = uint32_t(sid >> to_page_index);
				if (pid >= m_pages.size())
					return false;

				const auto did = uint32_t(sid & page_mask);
				const auto id = m_pages[pid].get_id(did);
				return id != detail::InvalidDenseId;
			}

			//! Checks if an item \param arg exists within the storage
			GAIA_NODISCARD bool has(const T& arg) const {
				const auto sid = to_sparse_id<T>::get(arg);
				GAIA_ASSERT(sid != detail::InvalidSparseId);
				return has(sid);
			}

			//! Inserts the item \param arg into the storage.
			//! \return Reference to the inserted record or nothing in case it is has a SoA layout.
			template <typename TType>
			decltype(auto) add(TType&& arg) {
				const auto sid = to_sparse_id<T>::get(arg);
				if (has(sid)) {
					const auto pid = uint32_t(sid >> to_page_index);
					const auto did = uint32_t(sid & page_mask);
					auto& page = m_pages[pid];
					return page.set_data(did);
				}

				const auto pid = uint32_t(sid >> to_page_index);
				const auto did = uint32_t(sid & page_mask);

				try_grow(pid);
				m_dense[m_cnt] = sid;

				auto& page = m_pages[pid];
				page.set_id(did) = m_cnt++;
				return page.add_data(did, GAIA_FWD(arg));
			}

			//! Update the record at the index \param sid.
			//! \return Reference to the inserted record or nothing in case it is has a SoA layout.
			decltype(auto) set(sparse_id sid) {
				GAIA_ASSERT(has(sid));

				const auto pid = uint32_t(sid >> to_page_index);
				const auto did = uint32_t(sid & page_mask);

				auto& page = m_pages[pid];
				return page.set_data(did);
			}

			//! Removes the item at the index \param sid from the storage.
			void del(sparse_id sid) noexcept {
				GAIA_ASSERT(!empty());
				GAIA_ASSERT(sid != detail::InvalidSparseId);

				if (!has(sid))
					return;

				const auto pid = uint32_t(sid >> to_page_index);
				const auto did = uint32_t(sid & page_mask);

				const auto sidPrev = m_dense[m_cnt - 1];
				const auto didPrev = uint32_t(sidPrev & page_mask);

				auto& page = m_pages[pid];
				const auto id = page.get_id(did);
				page.set_id(didPrev) = id;
				page.set_id(did) = detail::InvalidDenseId;
				page.del_data(did);
				m_dense[id] = sidPrev;
				m_dense.resize(m_cnt - 1);

				GAIA_ASSERT(m_cnt > 0);
				--m_cnt;
			}

			//! Removes the item \param arg from the storage.
			void del(const T& arg) noexcept {
				const auto sid = to_sparse_id<T>::get(arg);
				return del(sid);
			}

			//! Clears the storage
			void clear() {
				m_dense.resize(0);
				m_pages.resize(0);
				m_cnt = 0;
			}

			//! Returns the number of items inserted into the storage
			GAIA_NODISCARD size_type size() const noexcept {
				return m_cnt;
			}

			//! Checks if the storage is empty (no items inserted)
			GAIA_NODISCARD bool empty() const noexcept {
				return size() == 0;
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

				const auto sid = m_dense[m_cnt - 1];
				const auto pid = uint32_t(sid >> to_page_index);
				const auto did = uint32_t(sid & page_mask);

				return (reference)m_pages[pid].set_data(did);
			}

			GAIA_NODISCARD decltype(auto) back() const noexcept {
				GAIA_ASSERT(!empty());

				const auto sid = m_dense[m_cnt - 1];
				const auto pid = uint32_t(sid >> to_page_index);
				const auto did = uint32_t(sid & page_mask);

				return (const_reference)m_pages[pid].get_data(did);
			}

			GAIA_NODISCARD auto begin() const noexcept {
				GAIA_ASSERT(!empty());

				return iterator(m_dense.data(), m_pages.data());
			}

			GAIA_NODISCARD auto end() const noexcept {
				GAIA_ASSERT(!empty());

				return iterator(m_dense.data() + size(), m_pages.data());
			}

			GAIA_NODISCARD bool operator==(const sparse_storage& other) const {
				// The number of items needs to be the same
				if (m_cnt != other.m_cnt)
					return false;

				// Dense indices need to be the same.
				// We don't check m_sparse, because it m_dense doesn't
				// match, m_sparse will be different as well.
				if (m_dense != other.m_dense)
					return false;

				// Check data one-by-one.
				// We don't compare the entire array, only the actually stored values,
				// because their is possible a lot of empty space in the data array (it is sparse).
				const size_type n = size();
				for (size_type i = 0, cnt = 0; i < n && cnt < m_cnt; ++i, ++cnt) {
					const auto sid = m_dense[i];
					const auto pid = uint32_t(sid >> to_page_index);
					const auto did = uint32_t(sid & page_mask);

					const auto& item0 = m_pages[pid].get_data(did);
					const auto& item1 = m_pages[pid].get_data(did);

					if (!(item0 == item1))
						return false;
				}
				return true;
			}

			GAIA_NODISCARD constexpr bool operator!=(const sparse_storage& other) const {
				return !operator==(other);
			}
		};

		//! Array with variable size of elements of type \tparam T allocated on heap.
		//! Allocates enough memory to support \tparam PageCapacity elements.
		//! Uses \tparam Allocator to allocate memory.
		//! This version is optimized for tags (data of zero size).
		template <typename T, uint32_t PageCapacity, typename Allocator>
		class sparse_storage<T, PageCapacity, Allocator, std::enable_if_t<std::is_empty_v<T>>> {
		public:
			using value_type = T;
			using reference = T&;
			using const_reference = const T&;
			using pointer = T*;
			using const_pointer = T*;
			using view_policy = mem::data_view_policy_aos<T>;
			using difference_type = detail::difference_type;
			using size_type = detail::size_type;

			using iterator = sparse_iterator<T, PageCapacity, Allocator, std::enable_if_t<std::is_empty_v<T>>>;
			using page_type = detail::sparse_page<T, PageCapacity, Allocator, std::enable_if_t<std::is_empty_v<T>>>;

		private:
			static_assert((PageCapacity & (PageCapacity - 1)) == 0, "PageCapacity of sparse_storage must be a power of 2");
			constexpr static sparse_id page_mask = PageCapacity - 1;
			constexpr static sparse_id to_page_index = core::count_bits(page_mask);

			//! Contains mappings to sparse storage inside pages
			cnt::darray<sparse_id> m_dense;
			//! Contains pages with data and sparse–>dense mapping
			cnt::darray<page_type> m_pages;
			//! Current number of items tracked by the sparse set
			size_type m_cnt = size_type(0);

			void try_grow(uint32_t pid) {
				m_dense.resize(m_cnt + 1);

				// The sparse array has to be able to take any sparse index
				if (pid >= m_pages.size())
					m_pages.resize(pid + 1);

				m_pages[pid].add();
			}

		public:
			constexpr sparse_storage() noexcept = default;

			sparse_storage(const sparse_storage& other) {
				GAIA_ASSERT(core::addressof(other) != this);

				m_dense = other.m_dense;
				m_pages = other.m_pages;
				m_cnt = other.m_cnt;
			}

			sparse_storage& operator=(const sparse_storage& other) {
				GAIA_ASSERT(core::addressof(other) != this);

				m_dense = other.m_dense;
				m_pages = other.m_pages;
				m_cnt = other.m_cnt;

				return *this;
			}

			sparse_storage(sparse_storage&& other) noexcept {
				// This is a newly constructed object.
				// It can't have any memory allocated, yet.
				GAIA_ASSERT(m_dense.data() == nullptr);

				m_dense = GAIA_MOV(other.m_dense);
				m_pages = GAIA_MOV(other.m_pages);
				m_cnt = other.m_cnt;

				other.m_dense = {};
				other.m_pages = {};
				other.m_cnt = size_type(0);
			}

			sparse_storage& operator=(sparse_storage&& other) noexcept {
				GAIA_ASSERT(core::addressof(other) != this);

				m_dense = GAIA_MOV(other.m_dense);
				m_pages = GAIA_MOV(other.m_pages);
				m_cnt = other.m_cnt;

				other.m_dense = {};
				other.m_pages = {};
				other.m_cnt = size_type(0);

				return *this;
			}

			~sparse_storage() = default;

			//! Checks if an item with a given sparse id \param sid exists
			GAIA_NODISCARD bool has(sparse_id sid) const {
				GAIA_ASSERT(sid != detail::InvalidSparseId);

				const auto pid = uint32_t(sid >> to_page_index);
				const auto did = uint32_t(sid & page_mask);
				return has_internal(pid, did);
			}

		private:
			GAIA_NODISCARD bool has_internal(uint32_t pid, uint32_t did) const {
				if (pid >= m_pages.size())
					return false;

				const auto id = m_pages[pid].get_id(did);
				return id != detail::InvalidDenseId;
			}

		public:
			//! Inserts the item \param arg into the storage.
			//! \return Reference to the inserted record or nothing in case it is has a SoA layout.
			void add(sparse_id sid) {
				GAIA_ASSERT(sid != detail::InvalidSparseId);

				const auto pid = uint32_t(sid >> to_page_index);
				const auto did = uint32_t(sid & page_mask);

				if (has_internal(pid, did))
					return;

				try_grow(pid);
				m_dense[m_cnt] = sid;

				auto& page = m_pages[pid];
				page.set_id(did) = m_cnt++;
			}

			//! Removes the item at the index \param sid from the storage.
			void del(sparse_id sid) noexcept {
				GAIA_ASSERT(!empty());
				GAIA_ASSERT(sid != detail::InvalidSparseId);

				const auto pid = uint32_t(sid >> to_page_index);
				const auto did = uint32_t(sid & page_mask);

				if (!has_internal(pid, did))
					return;

				const auto sidPrev = m_dense[m_cnt - 1];
				const auto didPrev = uint32_t(sidPrev & page_mask);

				auto& page = m_pages[pid];
				const auto id = page.get_id(did);
				page.set_id(didPrev) = id;
				page.set_id(did) = detail::InvalidDenseId;
				m_dense[id] = sidPrev;
				m_dense.resize(m_cnt - 1);

				GAIA_ASSERT(m_cnt > 0);
				--m_cnt;
			}

			//! Clears the storage
			void clear() {
				m_dense.resize(0);
				m_pages.resize(0);
				m_cnt = 0;
			}

			//! Returns the number of items inserted into the storage
			GAIA_NODISCARD size_type size() const noexcept {
				return m_cnt;
			}

			//! Checks if the storage is empty (no items inserted)
			GAIA_NODISCARD bool empty() const noexcept {
				return size() == 0;
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

				const auto sid = m_dense[m_cnt - 1];
				const auto pid = uint32_t(sid >> to_page_index);
				const auto did = uint32_t(sid & page_mask);

				return (reference)m_pages[pid].set_id(did);
			}

			GAIA_NODISCARD decltype(auto) back() const noexcept {
				GAIA_ASSERT(!empty());

				const auto sid = m_dense[m_cnt - 1];
				const auto pid = uint32_t(sid >> to_page_index);
				const auto did = uint32_t(sid & page_mask);

				return (const_reference)m_pages[pid].get_id(did);
			}

			GAIA_NODISCARD auto begin() const noexcept {
				GAIA_ASSERT(!empty());
				return iterator(m_dense.data());
			}

			GAIA_NODISCARD auto end() const noexcept {
				GAIA_ASSERT(!empty());
				return iterator(m_dense.data() + size());
			}

			GAIA_NODISCARD bool operator==(const sparse_storage& other) const {
				// The number of items needs to be the same
				if (m_cnt != other.m_cnt)
					return false;

				// Dense indices need to be the same.
				// We don't check m_sparse, because it m_dense doesn't
				// match, m_sparse will be different as well.
				if (m_dense != other.m_dense)
					return false;

				return true;
			}

			GAIA_NODISCARD constexpr bool operator!=(const sparse_storage& other) const {
				return !operator==(other);
			}
		};
	} // namespace cnt

} // namespace gaia