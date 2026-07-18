#pragma once
#include "gaia/config/config.h"

#include <cstddef>
#include <initializer_list>
#include <type_traits>
#include <utility>

#include "gaia/cnt/darray.h"
#include "gaia/core/iterator.h"
#include "gaia/core/utility.h"
#include "gaia/mem/data_layout_policy.h"
#include "gaia/mem/mem_utils.h"

namespace gaia {
	namespace cnt {
		//! Identifier used to address an element in sparse storage.
		using sparse_id = uint64_t;

		namespace detail {
			//! \cond INTERNAL
			using difference_type = uint32_t;
			using size_type = uint32_t;

			constexpr static sparse_id InvalidSparseId = (sparse_id)-1;
			constexpr static size_type InvalidDenseId = BadIndex - 1;
			inline constexpr sparse_id EmptyDenseId = InvalidSparseId;
			//! \endcond

			//! Returns dense data or a valid shared sentinel for an unallocated empty array.
			//! \tparam Dense Dense sparse-id array type.
			//! \param dense Dense sparse-id array.
			//! \return Pointer suitable for constructing an empty or populated iterator range.
			template <typename Dense>
			GAIA_NODISCARD auto sparse_dense_data(Dense& dense) noexcept {
				auto* pData = dense.data();
				return pData != nullptr ? pData : &EmptyDenseId;
			}

			//! \cond INTERNAL
			template <typename T, uint32_t PageCapacity, typename Allocator, typename>
			class sparse_page;
			//! \endcond
		} // namespace detail

		template <typename T>
		//! Converts an item to the sparse identifier used by sparse_storage.
		//! \tparam T Item type. Non-empty types must specialize this conversion.
		struct to_sparse_id {
			//! Returns the sparse identifier for an item.
			//! \param item Item to convert.
			//! \return Sparse identifier supplied by a specialization.
			static sparse_id get(const T& item) noexcept {
				(void)item;
				static_assert(
						std::is_empty_v<T>,
						"Sparse_storage items require a conversion function to be defined in gaia::cnt namespace");
				return detail::InvalidSparseId;
			}
		};

		template <typename T, uint32_t PageCapacity, typename Allocator, typename = void>
		//! Mutable random-access iterator over sparse-storage values.
		//! \tparam T Stored value type.
		//! \tparam PageCapacity Number of sparse entries represented by each page.
		//! \tparam Allocator Allocator used by the sparse pages.
		struct sparse_iterator {
			using iterator_category = core::random_access_iterator_tag; //!< Iterator category.
			using value_type = T; //!< Stored value type.
			using pointer = T*; //!< Pointer to a stored value.
			using reference = T&; //!< Reference to a stored value.
			using difference_type = detail::difference_type; //!< Type used for iterator distances.
			using size_type = detail::size_type; //!< Type used for iterator offsets.
			using iterator = sparse_iterator; //!< Iterator type.

		private:
			static_assert((PageCapacity & (PageCapacity - 1)) == 0, "PageCapacity of sparse_iterator must be a power of 2");
			constexpr static sparse_id page_mask = PageCapacity - 1;
			constexpr static sparse_id to_page_index = core::count_bits(page_mask);

			using page_type = detail::sparse_page<T, PageCapacity, Allocator, void>;

			const sparse_id* m_pDense;
			page_type* m_pPages;

		public:
			//! Constructs an iterator for a dense position and sparse-page array.
			//! \param pDense Dense sparse-id position.
			//! \param pPages Sparse-page array containing the values.
			sparse_iterator(const sparse_id* pDense, page_type* pPages): m_pDense(pDense), m_pPages(pPages) {}

			//! Returns the value at the current position.
			//! \return Mutable reference to the current value.
			reference operator*() const {
				const auto sid = *m_pDense;
				const auto pid = uint32_t(sid >> to_page_index);
				const auto did = uint32_t(sid & page_mask);
				auto& page = m_pPages[pid];
				return page.set_data(did);
			}
			//! Returns a pointer to the value at the current position.
			//! \return Pointer to the current value.
			pointer operator->() const {
				const auto sid = *m_pDense;
				const auto pid = uint32_t(sid >> to_page_index);
				const auto did = uint32_t(sid & page_mask);
				auto& page = m_pPages[pid];
				return &page.set_data(did);
			}
			//! Returns an iterator at an offset from the current position.
			//! \param offset Number of positions to advance.
			//! \return Offset iterator.
			iterator operator[](size_type offset) const {
				return {m_pDense + offset, m_pPages};
			}

			//! Advances the iterator.
			//! \param diff Number of positions to advance.
			//! \return This iterator after advancing.
			iterator& operator+=(size_type diff) {
				m_pDense += diff;
				return *this;
			}
			//! Moves the iterator backward.
			//! \param diff Number of positions to retreat.
			//! \return This iterator after retreating.
			iterator& operator-=(size_type diff) {
				m_pDense -= diff;
				return *this;
			}
			//! Advances to the next value.
			//! \return This iterator after advancing.
			iterator& operator++() {
				++m_pDense;
				return *this;
			}
			//! Advances to the next value.
			//! \return Iterator value before advancing.
			iterator operator++(int) {
				iterator temp(*this);
				++*this;
				return temp;
			}
			//! Moves to the previous value.
			//! \return This iterator after retreating.
			iterator& operator--() {
				--m_pDense;
				return *this;
			}
			//! Moves to the previous value.
			//! \return Iterator value before retreating.
			iterator operator--(int) {
				iterator temp(*this);
				--*this;
				return temp;
			}

			//! Returns an iterator advanced by an offset.
			//! \param offset Number of positions to advance.
			//! \return Advanced iterator.
			iterator operator+(size_type offset) const {
				return {m_pDense + offset, m_pPages};
			}
			//! Returns an iterator moved backward by an offset.
			//! \param offset Number of positions to retreat.
			//! \return Retreated iterator.
			iterator operator-(size_type offset) const {
				return {m_pDense - offset, m_pPages};
			}
			//! Returns the distance from another iterator.
			//! \param other Iterator to subtract.
			//! \return Number of positions between the iterators.
			difference_type operator-(const iterator& other) const {
				return (difference_type)(m_pDense - other.m_pDense);
			}

			//! Checks whether two iterators refer to the same position.
			//! \param other Iterator to compare.
			//! \return True if the positions are equal.
			GAIA_NODISCARD bool operator==(const iterator& other) const {
				return m_pDense == other.m_pDense;
			}
			//! Checks whether two iterators refer to different positions.
			//! \param other Iterator to compare.
			//! \return True if the positions differ.
			GAIA_NODISCARD bool operator!=(const iterator& other) const {
				return m_pDense != other.m_pDense;
			}
			//! Checks whether this iterator follows another iterator.
			//! \param other Iterator to compare.
			//! \return True if this position follows the other position.
			GAIA_NODISCARD bool operator>(const iterator& other) const {
				return m_pDense > other.m_pDense;
			}
			//! Checks whether this iterator does not precede another iterator.
			//! \param other Iterator to compare.
			//! \return True if this position is at or after the other position.
			GAIA_NODISCARD bool operator>=(const iterator& other) const {
				return m_pDense >= other.m_pDense;
			}
			//! Checks whether this iterator precedes another iterator.
			//! \param other Iterator to compare.
			//! \return True if this position precedes the other position.
			GAIA_NODISCARD bool operator<(const iterator& other) const {
				return m_pDense < other.m_pDense;
			}
			//! Checks whether this iterator does not follow another iterator.
			//! \param other Iterator to compare.
			//! \return True if this position is at or before the other position.
			GAIA_NODISCARD bool operator<=(const iterator& other) const {
				return m_pDense <= other.m_pDense;
			}
		};

		template <typename T, uint32_t PageCapacity, typename Allocator, typename = void>
		//! Constant random-access iterator over sparse-storage values.
		//! \tparam T Stored value type.
		//! \tparam PageCapacity Number of sparse entries represented by each page.
		//! \tparam Allocator Allocator used by the sparse pages.
		struct const_sparse_iterator {
			using iterator_category = core::random_access_iterator_tag; //!< Iterator category.
			using value_type = T; //!< Stored value type.
			using pointer = const T*; //!< Pointer to a stored value.
			using reference = const T&; //!< Reference to a stored value.
			using difference_type = detail::difference_type; //!< Type used for iterator distances.
			using size_type = detail::size_type; //!< Type used for iterator offsets.
			using iterator = const_sparse_iterator; //!< Iterator type.

		private:
			static_assert((PageCapacity & (PageCapacity - 1)) == 0, "PageCapacity of sparse_iterator must be a power of 2");
			constexpr static sparse_id page_mask = PageCapacity - 1;
			constexpr static sparse_id to_page_index = core::count_bits(page_mask);

			using page_type = detail::sparse_page<T, PageCapacity, Allocator, void>;

			const sparse_id* m_pDense;
			const page_type* m_pPages;

		public:
			//! Constructs a constant iterator for a dense position and sparse-page array.
			//! \param pDense Dense sparse-id position.
			//! \param pPages Sparse-page array containing the values.
			const_sparse_iterator(const sparse_id* pDense, const page_type* pPages): m_pDense(pDense), m_pPages(pPages) {}

			//! Returns the value at the current position.
			//! \return Constant reference to the current value.
			reference operator*() const {
				const auto sid = *m_pDense;
				const auto pid = uint32_t(sid >> to_page_index);
				const auto did = uint32_t(sid & page_mask);
				auto& page = m_pPages[pid];
				return page.get_data(did);
			}
			//! Returns a pointer to the value at the current position.
			//! \return Pointer to the current value.
			pointer operator->() const {
				const auto sid = *m_pDense;
				const auto pid = uint32_t(sid >> to_page_index);
				const auto did = uint32_t(sid & page_mask);
				auto& page = m_pPages[pid];
				return &page.get_data(did);
			}
			//! Returns an iterator at an offset from the current position.
			//! \param offset Number of positions to advance.
			//! \return Offset iterator.
			iterator operator[](size_type offset) const {
				return {m_pDense + offset, m_pPages};
			}

			//! Advances the iterator.
			//! \param diff Number of positions to advance.
			//! \return This iterator after advancing.
			iterator& operator+=(size_type diff) {
				m_pDense += diff;
				return *this;
			}
			//! Moves the iterator backward.
			//! \param diff Number of positions to retreat.
			//! \return This iterator after retreating.
			iterator& operator-=(size_type diff) {
				m_pDense -= diff;
				return *this;
			}
			//! Advances to the next value.
			//! \return This iterator after advancing.
			iterator& operator++() {
				++m_pDense;
				return *this;
			}
			//! Advances to the next value.
			//! \return Iterator value before advancing.
			iterator operator++(int) {
				iterator temp(*this);
				++*this;
				return temp;
			}
			//! Moves to the previous value.
			//! \return This iterator after retreating.
			iterator& operator--() {
				--m_pDense;
				return *this;
			}
			//! Moves to the previous value.
			//! \return Iterator value before retreating.
			iterator operator--(int) {
				iterator temp(*this);
				--*this;
				return temp;
			}

			//! Returns an iterator advanced by an offset.
			//! \param offset Number of positions to advance.
			//! \return Advanced iterator.
			iterator operator+(size_type offset) const {
				return {m_pDense + offset, m_pPages};
			}
			//! Returns an iterator moved backward by an offset.
			//! \param offset Number of positions to retreat.
			//! \return Retreated iterator.
			iterator operator-(size_type offset) const {
				return {m_pDense - offset, m_pPages};
			}
			//! Returns the distance from another iterator.
			//! \param other Iterator to subtract.
			//! \return Number of positions between the iterators.
			difference_type operator-(const iterator& other) const {
				return (difference_type)(m_pDense - other.m_pDense);
			}

			//! Checks whether two iterators refer to the same position.
			//! \param other Iterator to compare.
			//! \return True if the positions are equal.
			GAIA_NODISCARD bool operator==(const iterator& other) const {
				return m_pDense == other.m_pDense;
			}
			//! Checks whether two iterators refer to different positions.
			//! \param other Iterator to compare.
			//! \return True if the positions differ.
			GAIA_NODISCARD bool operator!=(const iterator& other) const {
				return m_pDense != other.m_pDense;
			}
			//! Checks whether this iterator follows another iterator.
			//! \param other Iterator to compare.
			//! \return True if this position follows the other position.
			GAIA_NODISCARD bool operator>(const iterator& other) const {
				return m_pDense > other.m_pDense;
			}
			//! Checks whether this iterator does not precede another iterator.
			//! \param other Iterator to compare.
			//! \return True if this position is at or after the other position.
			GAIA_NODISCARD bool operator>=(const iterator& other) const {
				return m_pDense >= other.m_pDense;
			}
			//! Checks whether this iterator precedes another iterator.
			//! \param other Iterator to compare.
			//! \return True if this position precedes the other position.
			GAIA_NODISCARD bool operator<(const iterator& other) const {
				return m_pDense < other.m_pDense;
			}
			//! Checks whether this iterator does not follow another iterator.
			//! \param other Iterator to compare.
			//! \return True if this position is at or before the other position.
			GAIA_NODISCARD bool operator<=(const iterator& other) const {
				return m_pDense <= other.m_pDense;
			}
		};

		template <typename T, uint32_t PageCapacity, typename Allocator>
		//! Mutable random-access iterator over sparse identifiers for empty stored types.
		//! \tparam T Empty stored value type.
		//! \tparam PageCapacity Number of sparse entries represented by each page.
		//! \tparam Allocator Allocator used by the sparse pages.
		struct sparse_iterator<T, PageCapacity, Allocator, std::enable_if_t<std::is_empty_v<T>>> {
			using iterator_category = core::random_access_iterator_tag; //!< Iterator category.
			using value_type = sparse_id; //!< Sparse identifier value type.
			// using pointer = sparse_id*; not supported
			// using reference = sparse_id&; not supported
			using difference_type = detail::difference_type; //!< Type used for iterator distances.
			using size_type = detail::size_type; //!< Type used for iterator offsets.
			using iterator = sparse_iterator; //!< Iterator type.

		private:
			static_assert((PageCapacity & (PageCapacity - 1)) == 0, "PageCapacity of sparse_iterator must be a power of 2");
			constexpr static sparse_id page_mask = PageCapacity - 1;
			constexpr static sparse_id to_page_index = core::count_bits(page_mask);

			using page_type = detail::sparse_page<T, PageCapacity, Allocator, std::enable_if_t<std::is_empty_v<T>>>;

			const value_type* m_pDense;

		public:
			//! Constructs an iterator for a dense sparse-id position.
			//! \param pDense Dense sparse-id position.
			sparse_iterator(const value_type* pDense): m_pDense(pDense) {}

			//! Returns the sparse identifier at the current position.
			//! \return Current sparse identifier.
			value_type operator*() const {
				const auto sid = *m_pDense;
				return sid;
			}
			//! Returns the sparse identifier at the current position.
			//! \return Current sparse identifier.
			value_type operator->() const {
				const auto sid = *m_pDense;
				return sid;
			}
			//! Returns an iterator at an offset from the current position.
			//! \param offset Number of positions to advance.
			//! \return Offset iterator.
			iterator operator[](size_type offset) const {
				return {m_pDense + offset};
			}

			//! Advances the iterator.
			//! \param diff Number of positions to advance.
			//! \return This iterator after advancing.
			iterator& operator+=(size_type diff) {
				m_pDense += diff;
				return *this;
			}
			//! Moves the iterator backward.
			//! \param diff Number of positions to retreat.
			//! \return This iterator after retreating.
			iterator& operator-=(size_type diff) {
				m_pDense -= diff;
				return *this;
			}
			//! Advances to the next sparse identifier.
			//! \return This iterator after advancing.
			iterator& operator++() {
				++m_pDense;
				return *this;
			}
			//! Advances to the next sparse identifier.
			//! \return Iterator value before advancing.
			iterator operator++(int) {
				iterator temp(*this);
				++*this;
				return temp;
			}
			//! Moves to the previous sparse identifier.
			//! \return This iterator after retreating.
			iterator& operator--() {
				--m_pDense;
				return *this;
			}
			//! Moves to the previous sparse identifier.
			//! \return Iterator value before retreating.
			iterator operator--(int) {
				iterator temp(*this);
				--*this;
				return temp;
			}

			//! Returns an iterator advanced by an offset.
			//! \param offset Number of positions to advance.
			//! \return Advanced iterator.
			iterator operator+(size_type offset) const {
				return {m_pDense + offset};
			}
			//! Returns an iterator moved backward by an offset.
			//! \param offset Number of positions to retreat.
			//! \return Retreated iterator.
			iterator operator-(size_type offset) const {
				return {m_pDense - offset};
			}
			//! Returns the distance from another iterator.
			//! \param other Iterator to subtract.
			//! \return Number of positions between the iterators.
			difference_type operator-(const iterator& other) const {
				return (difference_type)(m_pDense - other.m_pDense);
			}

			//! Checks whether two iterators refer to the same position.
			//! \param other Iterator to compare.
			//! \return True if the positions are equal.
			GAIA_NODISCARD bool operator==(const iterator& other) const {
				return m_pDense == other.m_pDense;
			}
			//! Checks whether two iterators refer to different positions.
			//! \param other Iterator to compare.
			//! \return True if the positions differ.
			GAIA_NODISCARD bool operator!=(const iterator& other) const {
				return m_pDense != other.m_pDense;
			}
			//! Checks whether this iterator follows another iterator.
			//! \param other Iterator to compare.
			//! \return True if this position follows the other position.
			GAIA_NODISCARD bool operator>(const iterator& other) const {
				return m_pDense > other.m_pDense;
			}
			//! Checks whether this iterator does not precede another iterator.
			//! \param other Iterator to compare.
			//! \return True if this position is at or after the other position.
			GAIA_NODISCARD bool operator>=(const iterator& other) const {
				return m_pDense >= other.m_pDense;
			}
			//! Checks whether this iterator precedes another iterator.
			//! \param other Iterator to compare.
			//! \return True if this position precedes the other position.
			GAIA_NODISCARD bool operator<(const iterator& other) const {
				return m_pDense < other.m_pDense;
			}
			//! Checks whether this iterator does not follow another iterator.
			//! \param other Iterator to compare.
			//! \return True if this position is at or before the other position.
			GAIA_NODISCARD bool operator<=(const iterator& other) const {
				return m_pDense <= other.m_pDense;
			}
		};

		template <typename T, uint32_t PageCapacity, typename Allocator>
		//! Constant random-access iterator over sparse identifiers for empty stored types.
		//! \tparam T Empty stored value type.
		//! \tparam PageCapacity Number of sparse entries represented by each page.
		//! \tparam Allocator Allocator used by the sparse pages.
		struct const_sparse_iterator<T, PageCapacity, Allocator, std::enable_if_t<std::is_empty_v<T>>> {
			using iterator_category = core::random_access_iterator_tag; //!< Iterator category.
			using value_type = sparse_id; //!< Sparse identifier value type.
			// using pointer = sparse_id*; not supported
			// using reference = sparse_id&; not supported
			using difference_type = detail::difference_type; //!< Type used for iterator distances.
			using size_type = detail::size_type; //!< Type used for iterator offsets.
			using iterator = const_sparse_iterator; //!< Iterator type.

		private:
			static_assert((PageCapacity & (PageCapacity - 1)) == 0, "PageCapacity of sparse_iterator must be a power of 2");
			constexpr static sparse_id page_mask = PageCapacity - 1;
			constexpr static sparse_id to_page_index = core::count_bits(page_mask);

			using page_type = detail::sparse_page<T, PageCapacity, Allocator, std::enable_if_t<std::is_empty_v<T>>>;

			const value_type* m_pDense;

		public:
			//! Constructs a constant iterator for a dense sparse-id position.
			//! \param pDense Dense sparse-id position.
			const_sparse_iterator(const value_type* pDense): m_pDense(pDense) {}

			//! Returns the sparse identifier at the current position.
			//! \return Current sparse identifier.
			value_type operator*() const {
				const auto sid = *m_pDense;
				return sid;
			}
			//! Returns the sparse identifier at the current position.
			//! \return Current sparse identifier.
			value_type operator->() const {
				const auto sid = *m_pDense;
				return sid;
			}
			//! Returns an iterator at an offset from the current position.
			//! \param offset Number of positions to advance.
			//! \return Offset iterator.
			iterator operator[](size_type offset) const {
				return {m_pDense + offset};
			}

			//! Advances the iterator.
			//! \param diff Number of positions to advance.
			//! \return This iterator after advancing.
			iterator& operator+=(size_type diff) {
				m_pDense += diff;
				return *this;
			}
			//! Moves the iterator backward.
			//! \param diff Number of positions to retreat.
			//! \return This iterator after retreating.
			iterator& operator-=(size_type diff) {
				m_pDense -= diff;
				return *this;
			}
			//! Advances to the next sparse identifier.
			//! \return This iterator after advancing.
			iterator& operator++() {
				++m_pDense;
				return *this;
			}
			//! Advances to the next sparse identifier.
			//! \return Iterator value before advancing.
			iterator operator++(int) {
				iterator temp(*this);
				++*this;
				return temp;
			}
			//! Moves to the previous sparse identifier.
			//! \return This iterator after retreating.
			iterator& operator--() {
				--m_pDense;
				return *this;
			}
			//! Moves to the previous sparse identifier.
			//! \return Iterator value before retreating.
			iterator operator--(int) {
				iterator temp(*this);
				--*this;
				return temp;
			}

			//! Returns an iterator advanced by an offset.
			//! \param offset Number of positions to advance.
			//! \return Advanced iterator.
			iterator operator+(size_type offset) const {
				return {m_pDense + offset};
			}
			//! Returns an iterator moved backward by an offset.
			//! \param offset Number of positions to retreat.
			//! \return Retreated iterator.
			iterator operator-(size_type offset) const {
				return {m_pDense - offset};
			}
			//! Returns the distance from another iterator.
			//! \param other Iterator to subtract.
			//! \return Number of positions between the iterators.
			difference_type operator-(const iterator& other) const {
				return (difference_type)(m_pDense - other.m_pDense);
			}

			//! Checks whether two iterators refer to the same position.
			//! \param other Iterator to compare.
			//! \return True if the positions are equal.
			GAIA_NODISCARD bool operator==(const iterator& other) const {
				return m_pDense == other.m_pDense;
			}
			//! Checks whether two iterators refer to different positions.
			//! \param other Iterator to compare.
			//! \return True if the positions differ.
			GAIA_NODISCARD bool operator!=(const iterator& other) const {
				return m_pDense != other.m_pDense;
			}
			//! Checks whether this iterator follows another iterator.
			//! \param other Iterator to compare.
			//! \return True if this position follows the other position.
			GAIA_NODISCARD bool operator>(const iterator& other) const {
				return m_pDense > other.m_pDense;
			}
			//! Checks whether this iterator does not precede another iterator.
			//! \param other Iterator to compare.
			//! \return True if this position is at or after the other position.
			GAIA_NODISCARD bool operator>=(const iterator& other) const {
				return m_pDense >= other.m_pDense;
			}
			//! Checks whether this iterator precedes another iterator.
			//! \param other Iterator to compare.
			//! \return True if this position precedes the other position.
			GAIA_NODISCARD bool operator<(const iterator& other) const {
				return m_pDense < other.m_pDense;
			}
			//! Checks whether this iterator does not follow another iterator.
			//! \param other Iterator to compare.
			//! \return True if this position is at or before the other position.
			GAIA_NODISCARD bool operator<=(const iterator& other) const {
				return m_pDense <= other.m_pDense;
			}
		};

		namespace detail {
			//! \cond INTERNAL
			template <typename T, uint32_t PageCapacity, typename Allocator, typename = void>
			class sparse_page {
			public:
				using value_type = T;
				using reference = T&;
				using const_reference = const T&;
				using pointer = T*;
				using const_pointer = const T*;
				using view_policy = mem::data_view_policy_aos<T>;
				using difference_type = detail::difference_type;
				using size_type = detail::size_type;

				using iterator = sparse_iterator<T, PageCapacity, Allocator>;
				using const_iterator = const_sparse_iterator<T, PageCapacity, Allocator>;

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

					// Destruct active items
					if (m_cnt != 0)
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
					return reinterpret_cast<pointer>(m_pData);
				}

				GAIA_NODISCARD const_pointer data() const noexcept {
					return reinterpret_cast<const_pointer>(m_pData);
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

				GAIA_NODISCARD bool allocated() const noexcept {
					return m_pSparse != nullptr;
				}

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

				GAIA_NODISCARD auto begin() noexcept {
					return iterator(data());
				}

				GAIA_NODISCARD auto begin() const noexcept {
					return const_iterator(data());
				}

				GAIA_NODISCARD auto cbegin() const noexcept {
					return const_iterator(data());
				}

				GAIA_NODISCARD auto rbegin() noexcept {
					return iterator((pointer)&back());
				}

				GAIA_NODISCARD auto rbegin() const noexcept {
					return const_iterator((pointer)&back());
				}

				GAIA_NODISCARD auto crbegin() const noexcept {
					return const_iterator((pointer)&back());
				}

				GAIA_NODISCARD auto end() noexcept {
					return iterator(data() + size());
				}

				GAIA_NODISCARD auto end() const noexcept {
					return const_iterator(data() + size());
				}

				GAIA_NODISCARD auto cend() const noexcept {
					return const_iterator(data() + size());
				}

				GAIA_NODISCARD auto rend() noexcept {
					return iterator(data() - 1);
				}

				GAIA_NODISCARD auto rend() const noexcept {
					return const_iterator(data() - 1);
				}

				GAIA_NODISCARD auto crend() const noexcept {
					return const_iterator(data() - 1);
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

			//! Sparse page. Specialized for zero-size \a T
			template <typename T, uint32_t PageCapacity, typename Allocator>
			class sparse_page<T, PageCapacity, Allocator, std::enable_if_t<std::is_empty_v<T>>> {
			public:
				using value_type = T;
				// using reference = T&; not supported
				// using const_reference = const T&; not supported
				using pointer = T*;
				using const_pointer = const T*;
				using view_policy = mem::data_view_policy_aos<T>;
				using difference_type = detail::difference_type;
				using size_type = detail::size_type;

				using iterator = sparse_iterator<T, PageCapacity, Allocator, std::enable_if_t<std::is_empty_v<T>>>;
				using const_iterator = const_sparse_iterator<T, PageCapacity, Allocator, std::enable_if_t<std::is_empty_v<T>>>;

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
					return reinterpret_cast<pointer>(m_pSparse);
				}

				GAIA_NODISCARD const_pointer data() const noexcept {
					return reinterpret_cast<const_pointer>(m_pSparse);
				}

				GAIA_CLANG_WARNING_POP()

				GAIA_NODISCARD bool allocated() const noexcept {
					return m_pSparse != nullptr;
				}

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

				GAIA_NODISCARD auto begin() noexcept {
					return iterator(data());
				}

				GAIA_NODISCARD auto begin() const noexcept {
					return const_iterator(data());
				}

				GAIA_NODISCARD auto cbegin() const noexcept {
					return const_iterator(data());
				}

				GAIA_NODISCARD auto rbegin() noexcept {
					return iterator((pointer)&back());
				}

				GAIA_NODISCARD auto rbegin() const noexcept {
					return const_iterator((pointer)&back());
				}

				GAIA_NODISCARD auto crbegin() const noexcept {
					return const_iterator((pointer)&back());
				}

				GAIA_NODISCARD auto end() noexcept {
					return iterator(data() + size());
				}

				GAIA_NODISCARD auto end() const noexcept {
					return const_iterator(data() + size());
				}

				GAIA_NODISCARD auto cend() const noexcept {
					return const_iterator(data() + size());
				}

				GAIA_NODISCARD auto rend() noexcept {
					return iterator(data() - 1);
				}

				GAIA_NODISCARD auto rend() const noexcept {
					return const_iterator(data() - 1);
				}

				GAIA_NODISCARD auto crend() const noexcept {
					return const_iterator(data() - 1);
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
			//! \endcond
		} // namespace detail

		//! Array with variable size of elements of type \a T allocated on heap.
		//! Allocates enough memory to support \a PageCapacity elements.
		//! Uses \a Allocator to allocate memory.
		//! \tparam T Stored value type.
		//! 	param PageCapacity Number of sparse entries represented by each page. Must be a power of two.
		//! \tparam Allocator Allocator used by the storage pages.
		template <
				typename T, uint32_t PageCapacity = 4096, typename Allocator = mem::DefaultAllocatorAdaptor, typename = void>
		class sparse_storage {
		public:
			using value_type = T; //!< Stored value type.
			using reference = T&; //!< Mutable value reference.
			using const_reference = const T&; //!< Constant value reference.
			using pointer = T*; //!< Mutable value pointer.
			using const_pointer = const T*; //!< Constant value pointer.
			using view_policy = mem::data_view_policy_aos<T>; //!< Data access policy.
			using difference_type = detail::difference_type; //!< Type used for iterator distances.
			using size_type = detail::size_type; //!< Type used for sizes and offsets.

			using iterator = sparse_iterator<T, PageCapacity, Allocator>; //!< Mutable iterator type.
			using const_iterator = const_sparse_iterator<T, PageCapacity, Allocator>; //!< Constant iterator type.
			using page_type = detail::sparse_page<T, PageCapacity, Allocator>; //!< Internal sparse-page type.

		private:
			static_assert((PageCapacity & (PageCapacity - 1)) == 0, "PageCapacity of sparse_storage must be a power of 2");
			constexpr static sparse_id page_mask = PageCapacity - 1;
			constexpr static sparse_id to_page_index = core::count_bits(page_mask);

			//! Contains mappings to sparse storage inside pages
			cnt::darray<sparse_id> m_dense;
			//! Contains pages with data and sparse–>dense mapping
			cnt::darray<page_type> m_pages;
			//! Current number of items tracked by sparse storage
			size_type m_cnt = size_type(0);

			void try_grow(uint32_t pid) {
				const auto required = m_cnt + 1;
				if (required > m_dense.capacity()) {
					auto cap = m_dense.capacity() == 0 ? size_type(16) : m_dense.capacity();
					while (cap < required)
						cap *= 2;
					m_dense.reserve(cap);
				}
				m_dense.resize(required);

				// The sparse array has to be able to take any sparse index
				if (pid >= m_pages.size())
					m_pages.resize(pid + 1);

				m_pages[pid].add();
			}

		public:
			constexpr sparse_storage() noexcept = default;

			//! Copy-constructs the storage.
			//! \param other Storage to copy.
			sparse_storage(const sparse_storage& other) {
				GAIA_ASSERT(core::addressof(other) != this);

				m_dense = other.m_dense;
				m_pages = other.m_pages;
				m_cnt = other.m_cnt;
			}

			//! Copy-assigns the storage.
			//! \param other Storage to copy.
			//! \return This storage.
			sparse_storage& operator=(const sparse_storage& other) {
				GAIA_ASSERT(core::addressof(other) != this);

				m_dense = other.m_dense;
				m_pages = other.m_pages;
				m_cnt = other.m_cnt;

				return *this;
			}

			//! Move-constructs the storage.
			//! \param other Storage to move from.
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

			//! Move-assigns the storage.
			//! \param other Storage to move from.
			//! \return This storage.
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

			//! Returns the value associated with a sparse identifier.
			//! \param sid Sparse identifier to access. It must exist.
			//! \return Mutable reference to the stored value.
			GAIA_NODISCARD decltype(auto) operator[](sparse_id sid) noexcept {
				GAIA_ASSERT(has(sid));
				const auto pid = uint32_t(sid >> to_page_index);
				const auto did = uint32_t(sid & page_mask);

				auto& page = m_pages[pid];
				return view_policy::set({(typename view_policy::TargetCastType)page.data(), PageCapacity}, did);
			}

			//! Returns the value associated with a sparse identifier.
			//! \param sid Sparse identifier to access. It must exist.
			//! \return Constant reference to the stored value.
			GAIA_NODISCARD decltype(auto) operator[](sparse_id sid) const noexcept {
				GAIA_ASSERT(has(sid));
				const auto pid = uint32_t(sid >> to_page_index);
				const auto did = uint32_t(sid & page_mask);

				auto& page = m_pages[pid];
				return view_policy::get({(typename view_policy::TargetCastType)page.data(), PageCapacity}, did);
			}

			GAIA_CLANG_WARNING_POP()

			//! Checks whether an item with a sparse identifier exists.
			//! \param sid Sparse identifier to find.
			//! \return True if the identifier is present.
			GAIA_NODISCARD bool has(sparse_id sid) const {
				if (sid == detail::InvalidSparseId)
					return false;

				const auto pid = uint32_t(sid >> to_page_index);
				if (pid >= m_pages.size())
					return false;

				const auto did = uint32_t(sid & page_mask);
				const auto& page = m_pages[pid];
				// Empty pages release their internal buffers but remain in m_pages until the
				// outer page array is compacted. Treat such slots as missing.
				if (!page.allocated())
					return false;

				const auto id = page.get_id(did);
				return id != detail::InvalidDenseId;
			}

			//! Checks if an item \a arg exists within the storage
			//! \param arg Data
			//! \return True if the item is present.
			GAIA_NODISCARD bool has(const T& arg) const {
				const auto sid = to_sparse_id<T>::get(arg);
				GAIA_ASSERT(sid != detail::InvalidSparseId);
				return has(sid);
			}

			//! Inserts the item \a arg into the storage.
			//! \param arg Data
			//! \return Reference to the inserted record or nothing in case it is has a SoA layout.
			template <typename TType>
			//! \tparam TType Inserted value type.
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

			//! Update the record at the index \a sid.
			//! \param sid Sparse id
			//! \return Reference to the inserted record or nothing in case it is has a SoA layout.
			decltype(auto) set(sparse_id sid) {
				GAIA_ASSERT(has(sid));

				const auto pid = uint32_t(sid >> to_page_index);
				const auto did = uint32_t(sid & page_mask);

				auto& page = m_pages[pid];
				return page.set_data(did);
			}

			//! Removes the item at the index \a sid from the storage.
			//! \param sid Sparse id
			void del(sparse_id sid) noexcept {
				GAIA_ASSERT(!empty());
				GAIA_ASSERT(sid != detail::InvalidSparseId);

				if (!has(sid))
					return;

				const auto pid = uint32_t(sid >> to_page_index);
				const auto did = uint32_t(sid & page_mask);

				const auto sidPrev = std::as_const(m_dense)[m_cnt - 1];
				const auto pidPrev = uint32_t(sidPrev >> to_page_index);
				const auto didPrev = uint32_t(sidPrev & page_mask);

				auto& page = m_pages[pid];
				const auto id = page.get_id(did);
				// The swapped-in dense item may live on a different sparse page.
				auto& pagePrev = m_pages[pidPrev];
				pagePrev.set_id(didPrev) = id;
				page.set_id(did) = detail::InvalidDenseId;
				page.del_data(did);
				m_dense[id] = sidPrev;
				m_dense.resize(m_cnt - 1);

				GAIA_ASSERT(m_cnt > 0);
				--m_cnt;
			}

			//! Removes the item \a arg from the storage.
			//! \param arg Data
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

			//! Returns the number of items inserted into the storage.
			//! \return Number of stored items.
			GAIA_NODISCARD size_type size() const noexcept {
				return m_cnt;
			}

			//! Checks if the storage is empty (no items inserted).
			//! \return True if the storage contains no items.
			GAIA_NODISCARD bool empty() const noexcept {
				return size() == 0;
			}

			//! Returns the first stored value.
			//! \return Mutable reference to the first value.
			GAIA_NODISCARD decltype(auto) front() noexcept {
				GAIA_ASSERT(!empty());
				return (reference)*begin();
			}

			//! Returns the first stored value.
			//! \return Constant reference to the first value.
			GAIA_NODISCARD decltype(auto) front() const noexcept {
				GAIA_ASSERT(!empty());
				return (const_reference)*begin();
			}

			//! Returns the last stored value.
			//! \return Mutable reference to the last value.
			GAIA_NODISCARD decltype(auto) back() noexcept {
				GAIA_ASSERT(!empty());

				const auto sid = m_dense[m_cnt - 1];
				const auto pid = uint32_t(sid >> to_page_index);
				const auto did = uint32_t(sid & page_mask);

				return (reference)m_pages[pid].set_data(did);
			}

			//! Returns the last stored value.
			//! \return Constant reference to the last value.
			GAIA_NODISCARD decltype(auto) back() const noexcept {
				GAIA_ASSERT(!empty());

				const auto sid = m_dense[m_cnt - 1];
				const auto pid = uint32_t(sid >> to_page_index);
				const auto did = uint32_t(sid & page_mask);

				return (const_reference)m_pages[pid].get_data(did);
			}

			//! Returns an iterator to the first value.
			//! \return Mutable begin iterator.
			GAIA_NODISCARD auto begin() noexcept {
				return iterator(detail::sparse_dense_data(m_dense), m_pages.data());
			}

			//! Returns an iterator to the first value.
			//! \return Constant begin iterator.
			GAIA_NODISCARD auto begin() const noexcept {
				return const_iterator(detail::sparse_dense_data(m_dense), m_pages.data());
			}

			//! Returns a constant iterator to the first value.
			//! \return Constant begin iterator.
			GAIA_NODISCARD auto cbegin() const noexcept {
				return const_iterator(detail::sparse_dense_data(m_dense), m_pages.data());
			}

			//! Returns an iterator past the last value.
			//! \return Mutable end iterator.
			GAIA_NODISCARD auto end() noexcept {
				return iterator(detail::sparse_dense_data(m_dense) + size(), m_pages.data());
			}

			//! Returns an iterator past the last value.
			//! \return Constant end iterator.
			GAIA_NODISCARD auto end() const noexcept {
				return const_iterator(detail::sparse_dense_data(m_dense) + size(), m_pages.data());
			}

			//! Returns a constant iterator past the last value.
			//! \return Constant end iterator.
			GAIA_NODISCARD auto cend() const noexcept {
				return const_iterator(detail::sparse_dense_data(m_dense) + size(), m_pages.data());
			}

			//! Checks whether two storages contain equal values at equal sparse identifiers.
			//! \param other Storage to compare.
			//! \return True if the storages are equal.
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

			//! Checks whether two storages differ.
			//! \param other Storage to compare.
			//! \return True if the storages are not equal.
			GAIA_NODISCARD constexpr bool operator!=(const sparse_storage& other) const {
				return !operator==(other);
			}
		};

		//! Array with variable size of elements of type \a T allocated on heap.
		//! Allocates enough memory to support \a PageCapacity elements.
		//! Uses \a Allocator to allocate memory.
		//! This version is optimized for tags (data of zero size).
		//! \tparam T Empty stored value type.
		//! 	param PageCapacity Number of sparse entries represented by each page. Must be a power of two.
		//! \tparam Allocator Allocator used by the storage pages.
		template <typename T, uint32_t PageCapacity, typename Allocator>
		class sparse_storage<T, PageCapacity, Allocator, std::enable_if_t<std::is_empty_v<T>>> {
		public:
			using value_type = T; //!< Stored empty value type.
			using reference = T&; //!< Mutable value reference.
			using const_reference = const T&; //!< Constant value reference.
			using pointer = T*; //!< Mutable value pointer.
			using const_pointer = const T*; //!< Constant value pointer.
			using view_policy = mem::data_view_policy_aos<T>; //!< Data access policy.
			using difference_type = detail::difference_type; //!< Type used for iterator distances.
			using size_type = detail::size_type; //!< Type used for sizes and offsets.

			using iterator =
					sparse_iterator<T, PageCapacity, Allocator, std::enable_if_t<std::is_empty_v<T>>>; //!< Mutable iterator type.
			using const_iterator =
					sparse_iterator<const T, PageCapacity, Allocator, std::enable_if_t<std::is_empty_v<T>>>; //!< Constant
																																																	 //!< iterator type.
			using page_type =
					detail::sparse_page<T, PageCapacity, Allocator, std::enable_if_t<std::is_empty_v<T>>>; //!< Internal
																																																 //!< sparse-page type.

		private:
			static_assert((PageCapacity & (PageCapacity - 1)) == 0, "PageCapacity of sparse_storage must be a power of 2");
			constexpr static sparse_id page_mask = PageCapacity - 1;
			constexpr static sparse_id to_page_index = core::count_bits(page_mask);

			//! Contains mappings to sparse storage inside pages
			cnt::darray<sparse_id> m_dense;
			//! Contains pages with data and sparse–>dense mapping
			cnt::darray<page_type> m_pages;
			//! Current number of items tracked by sparse storage
			size_type m_cnt = size_type(0);

			void try_grow(uint32_t pid) {
				const auto required = m_cnt + 1;
				if (required > m_dense.capacity()) {
					auto cap = m_dense.capacity() == 0 ? size_type(16) : m_dense.capacity();
					while (cap < required)
						cap *= 2;
					m_dense.reserve(cap);
				}
				m_dense.resize(required);

				// The sparse array has to be able to take any sparse index
				if (pid >= m_pages.size())
					m_pages.resize(pid + 1);

				m_pages[pid].add();
			}

		public:
			constexpr sparse_storage() noexcept = default;

			//! Copy-constructs the storage.
			//! \param other Storage to copy.
			sparse_storage(const sparse_storage& other) {
				GAIA_ASSERT(core::addressof(other) != this);

				m_dense = other.m_dense;
				m_pages = other.m_pages;
				m_cnt = other.m_cnt;
			}

			//! Copy-assigns the storage.
			//! \param other Storage to copy.
			//! \return This storage.
			sparse_storage& operator=(const sparse_storage& other) {
				GAIA_ASSERT(core::addressof(other) != this);

				m_dense = other.m_dense;
				m_pages = other.m_pages;
				m_cnt = other.m_cnt;

				return *this;
			}

			//! Move-constructs the storage.
			//! \param other Storage to move from.
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

			//! Move-assigns the storage.
			//! \param other Storage to move from.
			//! \return This storage.
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

			//! Checks whether a sparse identifier is registered.
			//! \param sid Sparse identifier to find.
			//! \return True if the identifier is present.
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

				const auto& page = m_pages[pid];
				if (!page.allocated())
					return false;

				const auto id = page.get_id(did);
				return id != detail::InvalidDenseId;
			}

		public:
			//! Registers a new sparse id.
			//! \param sid Sparse id
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

			//! Removes a sparse id from storage.
			//! \param sid Sparse id
			void del(sparse_id sid) noexcept {
				GAIA_ASSERT(!empty());
				GAIA_ASSERT(sid != detail::InvalidSparseId);

				const auto pid = uint32_t(sid >> to_page_index);
				const auto did = uint32_t(sid & page_mask);

				if (!has_internal(pid, did))
					return;

				const auto sidPrev = std::as_const(m_dense)[m_cnt - 1];
				const auto pidPrev = uint32_t(sidPrev >> to_page_index);
				const auto didPrev = uint32_t(sidPrev & page_mask);

				auto& page = m_pages[pid];
				const auto id = page.get_id(did);
				// The swapped-in dense item may live on a different sparse page.
				auto& pagePrev = m_pages[pidPrev];
				pagePrev.set_id(didPrev) = id;
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

			//! Returns the number of identifiers registered in the storage.
			//! \return Number of registered identifiers.
			GAIA_NODISCARD size_type size() const noexcept {
				return m_cnt;
			}

			//! Checks if the storage is empty (no items inserted).
			//! \return True if the storage contains no identifiers.
			GAIA_NODISCARD bool empty() const noexcept {
				return size() == 0;
			}

			//! Returns the first registered sparse identifier.
			//! \return Reference representing the first identifier.
			GAIA_NODISCARD decltype(auto) front() noexcept {
				GAIA_ASSERT(!empty());
				return (reference)*begin();
			}

			//! Returns the first registered sparse identifier.
			//! \return Constant reference representing the first identifier.
			GAIA_NODISCARD decltype(auto) front() const noexcept {
				GAIA_ASSERT(!empty());
				return (const_reference)*begin();
			}

			//! Returns the last registered sparse identifier.
			//! \return Reference representing the last identifier.
			GAIA_NODISCARD decltype(auto) back() noexcept {
				GAIA_ASSERT(!empty());

				const auto sid = m_dense[m_cnt - 1];
				const auto pid = uint32_t(sid >> to_page_index);
				const auto did = uint32_t(sid & page_mask);

				return (reference)m_pages[pid].set_id(did);
			}

			//! Returns the last registered sparse identifier.
			//! \return Constant reference representing the last identifier.
			GAIA_NODISCARD decltype(auto) back() const noexcept {
				GAIA_ASSERT(!empty());

				const auto sid = m_dense[m_cnt - 1];
				const auto pid = uint32_t(sid >> to_page_index);
				const auto did = uint32_t(sid & page_mask);

				return (const_reference)m_pages[pid].get_id(did);
			}

			//! Returns an iterator to the first sparse identifier.
			//! \return Mutable begin iterator.
			GAIA_NODISCARD auto begin() noexcept {
				return iterator(detail::sparse_dense_data(m_dense));
			}

			//! Returns an iterator to the first sparse identifier.
			//! \return Constant begin iterator.
			GAIA_NODISCARD auto begin() const noexcept {
				return const_iterator(detail::sparse_dense_data(m_dense));
			}

			//! Returns a constant iterator to the first sparse identifier.
			//! \return Constant begin iterator.
			GAIA_NODISCARD auto cbegin() const noexcept {
				return const_iterator(detail::sparse_dense_data(m_dense));
			}

			//! Returns an iterator past the last sparse identifier.
			//! \return Mutable end iterator.
			GAIA_NODISCARD auto end() noexcept {
				return iterator(detail::sparse_dense_data(m_dense) + size());
			}

			//! Returns an iterator past the last sparse identifier.
			//! \return Constant end iterator.
			GAIA_NODISCARD auto end() const noexcept {
				return const_iterator(detail::sparse_dense_data(m_dense) + size());
			}

			//! Returns a constant iterator past the last sparse identifier.
			//! \return Constant end iterator.
			GAIA_NODISCARD auto cend() const noexcept {
				return const_iterator(detail::sparse_dense_data(m_dense) + size());
			}

			//! Checks whether two storages contain the same sparse identifiers.
			//! \param other Storage to compare.
			//! \return True if the storages are equal.
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

			//! Checks whether two storages differ.
			//! \param other Storage to compare.
			//! \return True if the storages are not equal.
			GAIA_NODISCARD constexpr bool operator!=(const sparse_storage& other) const {
				return !operator==(other);
			}
		};
	} // namespace cnt

} // namespace gaia
