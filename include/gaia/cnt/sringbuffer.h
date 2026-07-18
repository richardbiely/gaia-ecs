#pragma once
#include <cstddef>
#include <type_traits>
#include <utility>

#include "gaia/core/iterator.h"
#include "gaia/mem/data_layout_policy.h"
#include "gaia/mem/mem_utils.h"

namespace gaia {
	namespace cnt {
		//! \cond INTERNAL
		namespace sringbuffer_detail {
			using difference_type = uint32_t;
			using size_type = uint32_t;
		} // namespace sringbuffer_detail
		//! \endcond

		//! Random-access iterator over the logical sequence stored in a sringbuffer.
		//! \tparam T Element type, optionally const-qualified.
		//! \tparam N Ring-buffer capacity.
		template <typename T, sringbuffer_detail::size_type N>
		struct sringbuffer_iterator {
			//! Element value type.
			using value_type = T;
			//! Pointer to an element.
			using pointer = T*;
			//! Reference to an element.
			using reference = T&;
			//! Type used for iterator distances.
			using difference_type = sringbuffer_detail::difference_type;
			//! Type used for indices and offsets.
			using size_type = sringbuffer_detail::size_type;

			//! Iterator type.
			using iterator = sringbuffer_iterator;
			//! Iterator category tag.
			using iterator_category = core::random_access_iterator_tag;

		private:
			pointer m_ptr;
			sringbuffer_detail::size_type m_tail;
			sringbuffer_detail::size_type m_size;
			sringbuffer_detail::size_type m_index;

		public:
			//! Constructs an iterator over a ring-buffer sequence.
			//! \param ptr Address of the physical buffer.
			//! \param tail Physical index of the logical front element.
			//! \param size Number of live elements in the buffer.
			//! \param index Logical iterator index in the range [0, size].
			sringbuffer_iterator(
					// Buffer address
					pointer ptr,
					// Ringbuffer tail
					sringbuffer_detail::size_type tail,
					// Ringbuffer size
					sringbuffer_detail::size_type size,
					// Current index
					sringbuffer_detail::size_type index): m_ptr(ptr), m_tail(tail), m_size(size), m_index(index) {}

			//! Dereferences the current logical element.
			//! \return Reference to the current element.
			T& operator*() const {
				return m_ptr[(m_tail + m_index) % N];
			}
			//! Accesses the current logical element.
			//! \return Pointer to the current element.
			T* operator->() const {
				return &m_ptr[(m_tail + m_index) % N];
			}
			//! Accesses storage at a logical offset using the declared iterator result type.
			//! \param offset Logical offset from the current index.
			//! \return Value read from the corresponding physical storage position.
			iterator operator[](size_type offset) const {
				return m_ptr[(m_tail + m_index + offset) % N];
			}

			//! Advances by a logical offset.
			//! \param diff Number of positions to advance.
			//! \return This iterator after advancement.
			iterator& operator+=(size_type diff) {
				m_index += diff;
				return *this;
			}
			//! Moves backward by a logical offset.
			//! \param diff Number of positions to move backward.
			//! \return This iterator after movement.
			iterator& operator-=(size_type diff) {
				m_index -= diff;
				return *this;
			}
			//! Advances by one logical position.
			//! \return This iterator after advancement.
			iterator& operator++() {
				++m_index;
				return *this;
			}
			//! Advances by one logical position.
			//! \return Iterator value before advancement.
			iterator operator++(int) {
				iterator temp(*this);
				++*this;
				return temp;
			}
			//! Moves backward by one logical position.
			//! \return This iterator after movement.
			iterator& operator--() {
				--m_index;
				return *this;
			}
			//! Moves backward by one logical position.
			//! \return Iterator value before movement.
			iterator operator--(int) {
				iterator temp(*this);
				--*this;
				return temp;
			}
			//! Returns an iterator advanced by an offset.
			//! \param offset Number of logical positions to advance.
			//! \return Offset iterator.
			iterator operator+(size_type offset) const {
				return {m_index + offset};
			}
			//! Returns an iterator moved backward by an offset.
			//! \param offset Number of logical positions to move backward.
			//! \return Offset iterator.
			iterator operator-(size_type offset) const {
				return {m_index - offset};
			}
			//! Calculates the logical distance between iterators from the same buffer.
			//! \param other Iterator to subtract.
			//! \return Signed logical index difference.
			difference_type operator-(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return (difference_type)(m_index - other.m_index);
			}
			//! Compares logical iterator positions.
			//! \param other Iterator from the same buffer.
			//! \return True when both iterators have the same logical index.
			GAIA_NODISCARD bool operator==(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return m_index == other.m_index;
			}
			//! Compares logical iterator positions.
			//! \param other Iterator from the same buffer.
			//! \return True when the iterators have different logical indices.
			GAIA_NODISCARD bool operator!=(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return m_index != other.m_index;
			}
			//! Compares logical iterator positions.
			//! \param other Iterator from the same buffer.
			//! \return True when this iterator follows other.
			GAIA_NODISCARD bool operator>(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return m_index > other.m_index;
			}
			//! Compares logical iterator positions.
			//! \param other Iterator from the same buffer.
			//! \return True when this iterator does not precede other.
			GAIA_NODISCARD bool operator>=(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return m_index >= other.m_index;
			}
			//! Compares logical iterator positions.
			//! \param other Iterator from the same buffer.
			//! \return True when this iterator precedes other.
			GAIA_NODISCARD bool operator<(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return m_index < other.m_index;
			}
			//! Compares logical iterator positions.
			//! \param other Iterator from the same buffer.
			//! \return True when this iterator does not follow other.
			GAIA_NODISCARD bool operator<=(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return m_index <= other.m_index;
			}
		};

		//! Array of elements of type \tparam T with fixed size and capacity \tparam N allocated on stack
		//! working as a ring buffer. That means the element at position N-1 is followed by the element
		//! at the position 0.
		//! Interface compatiblity with std::array where it matters.
		template <typename T, sringbuffer_detail::size_type N>
		class sringbuffer {
		public:
			static_assert(N > 1);

			//! Element value type.
			using value_type = T;
			//! Mutable element reference.
			using reference = T&;
			//! Immutable element reference.
			using const_reference = const T&;
			//! Mutable element pointer.
			using pointer = T*;
			//! Immutable element pointer.
			using const_pointer = const T*;
			//! Type used for iterator distances.
			using difference_type = sringbuffer_detail::size_type;
			//! Type used for sizes and indices.
			using size_type = sringbuffer_detail::size_type;

			//! Mutable random-access iterator.
			using iterator = sringbuffer_iterator<T, N>;
			//! Immutable random-access iterator.
			using const_iterator = sringbuffer_iterator<const T, N>;
			//! Iterator category tag.
			using iterator_category = core::random_access_iterator_tag;

			//! Compile-time buffer capacity.
			static constexpr size_type extent = N;

			//! Physical index of the logical front element.
			size_type m_tail{};
			//! Number of live elements.
			size_type m_size{};
			//! Physical element storage.
			T m_data[N];

			constexpr sringbuffer() noexcept = default;

			//! Constructs a ring buffer from an iterator range.
			//! \tparam InputIt Input iterator type.
			//! \param first First element to copy.
			//! \param last Sentinel following the last element to copy.
			template <typename InputIt>
			constexpr sringbuffer(InputIt first, InputIt last) noexcept {
				const auto count = (size_type)core::distance(first, last);
				GAIA_ASSERT(count <= max_size());
				if (count < 1)
					return;

				m_size = count;
				m_tail = 0;

				if constexpr (std::is_pointer_v<InputIt>) {
					for (size_type i = 0; i < count; ++i)
						m_data[i] = first[i];
				} else if constexpr (std::is_same_v<typename InputIt::iterator_category, core::random_access_iterator_tag>) {
					for (size_type i = 0; i < count; ++i)
						m_data[i] = *(first[i]);
				} else {
					size_type i = 0;
					for (auto it = first; it != last; ++it)
						m_data[i++] = *it;
				}
			}

			//! Constructs a ring buffer from an initializer list.
			//! \param il Elements to copy in logical order.
			constexpr sringbuffer(std::initializer_list<T> il) noexcept: sringbuffer(il.begin(), il.end()) {}

			//! Copy-constructs a ring buffer.
			//! \param other Ring buffer to copy.
			constexpr sringbuffer(const sringbuffer& other) noexcept: m_tail(other.m_tail), m_size(other.m_size) {
				mem::copy_elements<T, false>(m_data, other.m_data, other.size(), 0, extent, other.extent);
			}

			//! Move-constructs a ring buffer and leaves the source empty.
			//! \param other Ring buffer whose elements are transferred.
			constexpr sringbuffer(sringbuffer&& other) noexcept: m_tail(other.m_tail), m_size(other.m_size) {
				mem::move_elements<T, false>(m_data, other.m_data, other.size(), 0, extent, other.extent);

				other.m_tail = size_type(0);
				other.m_size = size_type(0);
			}

			//! Assigns elements from an initializer list.
			//! \param il Elements to copy in logical order.
			//! \return This ring buffer.
			constexpr sringbuffer& operator=(std::initializer_list<T> il) noexcept {
				*this = sringbuffer(il.begin(), il.end());
				return *this;
			}

			//! Copy-assigns a ring buffer.
			//! \param other Ring buffer to copy.
			//! \return This ring buffer.
			constexpr sringbuffer& operator=(const sringbuffer& other) {
				GAIA_ASSERT(core::addressof(other) != this);

				mem::copy_elements<T, false>(&m_data[0], other.m_data, other.size(), 0, extent, other.extent);

				m_tail = other.m_tail;
				m_size = other.m_size;

				return *this;
			}

			//! Move-assigns a ring buffer and leaves the source empty.
			//! \param other Ring buffer whose elements are transferred.
			//! \return This ring buffer.
			constexpr sringbuffer& operator=(sringbuffer&& other) noexcept {
				GAIA_ASSERT(core::addressof(other) != this);

				mem::move_elements<T, false>(m_data, other.m_data, other.size(), 0, extent, other.extent);

				m_tail = other.m_tail;
				m_size = other.m_size;

				other.m_tail = size_type(0);
				other.m_size = size_type(0);

				return *this;
			}

			~sringbuffer() noexcept = default;

			//! Appends a copied element.
			//! \param arg Element to append. The buffer must not be full.
			constexpr void push_back(const T& arg) {
				GAIA_ASSERT(m_size < N);
				const auto head = (m_tail + m_size) % N;
				m_data[head] = arg;
				++m_size;
			}

			//! Appends a moved element.
			//! \param arg Element to move into the buffer. The buffer must not be full.
			constexpr void push_back(T&& arg) {
				GAIA_ASSERT(m_size < N);
				const auto head = (m_tail + m_size) % N;
				m_data[head] = GAIA_MOV(arg);
				++m_size;
			}

			//! Removes and copies the front element.
			//! \param out Destination receiving the removed element.
			constexpr void pop_front(T& out) {
				GAIA_ASSERT(!empty());
				out = m_data[m_tail];
				m_tail = (m_tail + 1) % N;
				--m_size;
			}

			//! Removes and moves the front element.
			//! \param out Destination receiving the removed element.
			constexpr void pop_front(T&& out) {
				GAIA_ASSERT(!empty());
				out = GAIA_MOV(m_data[m_tail]);
				m_tail = (m_tail + 1) % N;
				--m_size;
			}

			//! Removes and copies the back element.
			//! \param out Destination receiving the removed element.
			constexpr void pop_back(T& out) {
				GAIA_ASSERT(m_size < N);
				const auto head = (m_tail + m_size - 1) % N;
				out = m_data[head];
				--m_size;
			}

			//! Removes and moves the back element.
			//! \param out Destination receiving the removed element.
			constexpr void pop_back(T&& out) {
				GAIA_ASSERT(m_size < N);
				const auto head = (m_tail + m_size - 1) % N;
				out = GAIA_MOV(m_data[head]);
				--m_size;
			}

			//! Returns the number of live elements.
			//! \return Current element count.
			GAIA_NODISCARD constexpr size_type size() const noexcept {
				return m_size;
			}

			//! Checks whether the buffer is empty.
			//! \return True when size() is zero. False otherwise.
			GAIA_NODISCARD constexpr bool empty() const noexcept {
				return size() == 0;
			}

			//! Returns the fixed capacity.
			//! \return N.
			GAIA_NODISCARD constexpr size_type capacity() const noexcept {
				return N;
			}

			//! Returns the maximum element count.
			//! \return N.
			GAIA_NODISCARD constexpr size_type max_size() const noexcept {
				return N;
			}

			//! Returns the front element.
			//! \return Mutable reference to the logical front element.
			GAIA_NODISCARD constexpr reference front() noexcept {
				GAIA_ASSERT(!empty());
				return m_data[m_tail];
			}

			//! Returns the front element.
			//! \return Immutable reference to the logical front element.
			GAIA_NODISCARD constexpr const_reference front() const noexcept {
				GAIA_ASSERT(!empty());
				return m_data[m_tail];
			}

			//! Returns the back element.
			//! \return Mutable reference to the logical back element.
			GAIA_NODISCARD constexpr reference back() noexcept {
				GAIA_ASSERT(!empty());
				const auto head = (m_tail + m_size - 1) % N;
				return m_data[head];
			}

			//! Returns the back element.
			//! \return Immutable reference to the logical back element.
			GAIA_NODISCARD constexpr const_reference back() const noexcept {
				GAIA_ASSERT(!empty());
				const auto head = (m_tail + m_size - 1) % N;
				return m_data[head];
			}

			//! Returns an iterator to the logical front.
			//! \return Mutable iterator to the first element.
			GAIA_NODISCARD constexpr auto begin() noexcept {
				return iterator((T*)&m_data[0], m_tail, m_size, 0);
			}

			//! Returns an iterator to the logical front.
			//! \return Immutable iterator to the first element.
			GAIA_NODISCARD constexpr auto begin() const noexcept {
				return const_iterator((T*)&m_data[0], m_tail, m_size, 0);
			}

			//! Returns an iterator to the logical front.
			//! \return Immutable iterator to the first element.
			GAIA_NODISCARD constexpr auto cbegin() const noexcept {
				return const_iterator((T*)&m_data[0], m_tail, m_size, 0);
			}

			//! Returns the mutable end sentinel.
			//! \return Iterator following the last logical element.
			GAIA_NODISCARD constexpr auto end() noexcept {
				return iterator((T*)&m_data[0], m_tail, m_size, m_size);
			}

			//! Returns the immutable end sentinel.
			//! \return Iterator following the last logical element.
			GAIA_NODISCARD constexpr auto end() const noexcept {
				return const_iterator((T*)&m_data[0], m_tail, m_size, m_size);
			}

			//! Returns the immutable end sentinel.
			//! \return Iterator following the last logical element.
			GAIA_NODISCARD constexpr auto cend() const noexcept {
				return const_iterator((T*)&m_data[0], m_tail, m_size, m_size);
			}

			//! Compares corresponding physical storage positions.
			//! \param other Ring buffer to compare with.
			//! \return True when every corresponding physical element differs. False otherwise.
			GAIA_NODISCARD constexpr bool operator==(const sringbuffer& other) const {
				for (size_type i = 0; i < N; ++i) {
					if (m_data[i] == other.m_data[i])
						return false;
				}
				return true;
			}
		};

		//! \cond INTERNAL
		namespace detail {
			template <typename T, uint32_t N, uint32_t... I>
			constexpr sringbuffer<std::remove_cv_t<T>, N>
			to_sringbuffer_impl(T (&a)[N], std::index_sequence<I...> /*no_name*/) {
				return {{a[I]...}};
			}
		} // namespace detail
		//! \endcond

		//! Creates a ring buffer containing every element of a built-in array.
		//! \tparam T Array element type.
		//! \tparam N Array extent and resulting ring-buffer capacity.
		//! \param a Source array.
		//! \return Ring buffer containing the array elements in order.
		template <typename T, uint32_t N>
		constexpr sringbuffer<std::remove_cv_t<T>, N> to_sringbuffer(T (&a)[N]) {
			return detail::to_sringbuffer_impl(a, std::make_index_sequence<N>{});
		}

		//! Deduces ring-buffer element type and capacity from constructor arguments.
		//! \tparam T First argument type and resulting element type.
		//! \tparam U Remaining argument types.
		template <typename T, typename... U>
		sringbuffer(T, U...) -> sringbuffer<T, 1 + sizeof...(U)>;

	} // namespace cnt

} // namespace gaia
