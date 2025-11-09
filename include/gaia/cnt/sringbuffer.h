#pragma once
#include <cstddef>
#include <type_traits>
#include <utility>

#include "gaia/core/iterator.h"
#include "gaia/mem/data_layout_policy.h"
#include "gaia/mem/mem_utils.h"

namespace gaia {
	namespace cnt {
		namespace sringbuffer_detail {
			using difference_type = uint32_t;
			using size_type = uint32_t;
		} // namespace sringbuffer_detail

		template <typename T, sringbuffer_detail::size_type N>
		struct sringbuffer_iterator {
			using value_type = T;
			using pointer = T*;
			using reference = T&;
			using difference_type = sringbuffer_detail::difference_type;
			using size_type = sringbuffer_detail::size_type;

			using iterator = sringbuffer_iterator;
			using iterator_category = core::random_access_iterator_tag;

		private:
			pointer m_ptr;
			sringbuffer_detail::size_type m_tail;
			sringbuffer_detail::size_type m_size;
			sringbuffer_detail::size_type m_index;

		public:
			sringbuffer_iterator(
					// Buffer address
					pointer ptr,
					// Ringbuffer tail
					sringbuffer_detail::size_type tail,
					// Ringbuffer size
					sringbuffer_detail::size_type size,
					// Current index
					sringbuffer_detail::size_type index): m_ptr(ptr), m_tail(tail), m_size(size), m_index(index) {}

			T& operator*() const {
				return m_ptr[(m_tail + m_index) % N];
			}
			T* operator->() const {
				return &m_ptr[(m_tail + m_index) % N];
			}
			iterator operator[](size_type offset) const {
				return m_ptr[(m_tail + m_index + offset) % N];
			}

			iterator& operator+=(size_type diff) {
				m_index += diff;
				return *this;
			}
			iterator& operator-=(size_type diff) {
				m_index -= diff;
				return *this;
			}
			iterator& operator++() {
				++m_index;
				return *this;
			}
			iterator operator++(int) {
				iterator temp(*this);
				++*this;
				return temp;
			}
			iterator& operator--() {
				--m_index;
				return *this;
			}
			iterator operator--(int) {
				iterator temp(*this);
				--*this;
				return temp;
			}
			iterator operator+(size_type offset) const {
				return {m_index + offset};
			}
			iterator operator-(size_type offset) const {
				return {m_index - offset};
			}
			difference_type operator-(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return (difference_type)(m_index - other.m_index);
			}
			GAIA_NODISCARD bool operator==(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return m_index == other.m_index;
			}
			GAIA_NODISCARD bool operator!=(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return m_index != other.m_index;
			}
			GAIA_NODISCARD bool operator>(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return m_index > other.m_index;
			}
			GAIA_NODISCARD bool operator>=(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return m_index >= other.m_index;
			}
			GAIA_NODISCARD bool operator<(const iterator& other) const {
				GAIA_ASSERT(m_ptr == other.m_ptr);
				return m_index < other.m_index;
			}
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
			static_assert(!mem::is_soa_layout_v<T>);

			using value_type = T;
			using reference = T&;
			using const_reference = const T&;
			using pointer = T*;
			using const_pointer = T*;
			using difference_type = sringbuffer_detail::size_type;
			using size_type = sringbuffer_detail::size_type;

			using iterator = sringbuffer_iterator<T, N>;
			using iterator_category = core::random_access_iterator_tag;

			static constexpr size_type extent = N;

			size_type m_tail{};
			size_type m_size{};
			T m_data[N];

			constexpr sringbuffer() noexcept = default;

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

			constexpr sringbuffer(std::initializer_list<T> il) noexcept: sringbuffer(il.begin(), il.end()) {}

			constexpr sringbuffer(const sringbuffer& other) noexcept: m_tail(other.m_tail), m_size(other.m_size) {
				mem::copy_elements<T>(m_data, other.m_data, other.size(), 0, extent, other.extent);
			}

			constexpr sringbuffer(sringbuffer&& other) noexcept: m_tail(other.m_tail), m_size(other.m_size) {
				mem::move_elements<T>(m_data, other.m_data, other.size(), 0, extent, other.extent);

				other.m_tail = size_type(0);
				other.m_size = size_type(0);
			}

			constexpr sringbuffer& operator=(std::initializer_list<T> il) noexcept {
				*this = sringbuffer(il.begin(), il.end());
				return *this;
			}

			constexpr sringbuffer& operator=(const sringbuffer& other) {
				GAIA_ASSERT(core::addressof(other) != this);

				mem::copy_elements<T>(&m_data[0], other.m_data, other.size(), 0, extent, other.extent);

				m_tail = other.m_tail;
				m_size = other.m_size;

				return *this;
			}

			constexpr sringbuffer& operator=(sringbuffer&& other) noexcept {
				GAIA_ASSERT(core::addressof(other) != this);

				mem::move_elements<T>(m_data, other.m_data, other.size(), 0, extent, other.extent);

				m_tail = other.m_tail;
				m_size = other.m_size;

				other.m_tail = size_type(0);
				other.m_size = size_type(0);

				return *this;
			}

			~sringbuffer() noexcept = default;

			constexpr void push_back(const T& arg) {
				GAIA_ASSERT(m_size < N);
				const auto head = (m_tail + m_size) % N;
				m_data[head] = arg;
				++m_size;
			}

			constexpr void push_back(T&& arg) {
				GAIA_ASSERT(m_size < N);
				const auto head = (m_tail + m_size) % N;
				m_data[head] = GAIA_MOV(arg);
				++m_size;
			}

			constexpr void pop_front(T& out) {
				GAIA_ASSERT(!empty());
				out = m_data[m_tail];
				m_tail = (m_tail + 1) % N;
				--m_size;
			}

			constexpr void pop_front(T&& out) {
				GAIA_ASSERT(!empty());
				out = GAIA_MOV(m_data[m_tail]);
				m_tail = (m_tail + 1) % N;
				--m_size;
			}

			constexpr void pop_back(T& out) {
				GAIA_ASSERT(m_size < N);
				const auto head = (m_tail + m_size - 1) % N;
				out = m_data[head];
				--m_size;
			}

			constexpr void pop_back(T&& out) {
				GAIA_ASSERT(m_size < N);
				const auto head = (m_tail + m_size - 1) % N;
				out = GAIA_MOV(m_data[head]);
				--m_size;
			}

			GAIA_NODISCARD constexpr size_type size() const noexcept {
				return m_size;
			}

			GAIA_NODISCARD constexpr bool empty() const noexcept {
				return size() == 0;
			}

			GAIA_NODISCARD constexpr size_type capacity() const noexcept {
				return N;
			}

			GAIA_NODISCARD constexpr size_type max_size() const noexcept {
				return N;
			}

			GAIA_NODISCARD constexpr reference front() noexcept {
				GAIA_ASSERT(!empty());
				return m_data[m_tail];
			}

			GAIA_NODISCARD constexpr const_reference front() const noexcept {
				GAIA_ASSERT(!empty());
				return m_data[m_tail];
			}

			GAIA_NODISCARD constexpr reference back() noexcept {
				GAIA_ASSERT(!empty());
				const auto head = (m_tail + m_size - 1) % N;
				return m_data[head];
			}

			GAIA_NODISCARD constexpr const_reference back() const noexcept {
				GAIA_ASSERT(!empty());
				const auto head = (m_tail + m_size - 1) % N;
				return m_data[head];
			}

			GAIA_NODISCARD constexpr iterator begin() const noexcept {
				return iterator((T*)&m_data[0], m_tail, m_size, 0);
			}

			GAIA_NODISCARD constexpr iterator end() const noexcept {
				return iterator((T*)&m_data[0], m_tail, m_size, m_size);
			}

			GAIA_NODISCARD constexpr bool operator==(const sringbuffer& other) const {
				for (size_type i = 0; i < N; ++i) {
					if (m_data[i] == other.m_data[i])
						return false;
				}
				return true;
			}
		};

		namespace detail {
			template <typename T, uint32_t N, uint32_t... I>
			constexpr sringbuffer<std::remove_cv_t<T>, N>
			to_sringbuffer_impl(T (&a)[N], std::index_sequence<I...> /*no_name*/) {
				return {{a[I]...}};
			}
		} // namespace detail

		template <typename T, uint32_t N>
		constexpr sringbuffer<std::remove_cv_t<T>, N> to_sringbuffer(T (&a)[N]) {
			return detail::to_sringbuffer_impl(a, std::make_index_sequence<N>{});
		}

		template <typename T, typename... U>
		sringbuffer(T, U...) -> sringbuffer<T, 1 + sizeof...(U)>;

	} // namespace cnt

} // namespace gaia
