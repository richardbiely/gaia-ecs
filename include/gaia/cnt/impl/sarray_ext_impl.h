#pragma once
#include "../../config/config.h"

#include <cstddef>
#include <initializer_list>
#include <tuple>
#include <type_traits>
#include <utility>

#include "../../core/iterator.h"
#include "../../mem/data_layout_policy.h"
#include "../../mem/mem_utils.h"

namespace gaia {
	namespace cnt {
		//! Array of elements of type \tparam T with fixed capacity \tparam N and variable size allocated on stack.
		//! Interface compatiblity with std::array where it matters.
		template <typename T, uint32_t N>
		class sarr_ext {
		public:
			static_assert(N > 0);

			using iterator_category = core::random_access_iterator_tag;
			using value_type = T;
			using reference = T&;
			using const_reference = const T&;
			using pointer = T*;
			using const_pointer = T*;
			using difference_type = decltype(N);
			using size_type = decltype(N);
			using view_policy = mem::auto_view_policy<T>;

			static constexpr size_type extent = N;
			static constexpr uint32_t allocated_bytes = view_policy::get_min_byte_size(0, N);

		private:
			mem::raw_data_holder<T, allocated_bytes> m_data;
			size_type m_cnt = size_type(0);

		public:
			class iterator {
			public:
				using iterator_category = core::random_access_iterator_tag;
				using value_type = T;
				using difference_type = sarr_ext::size_type;
				using pointer = T*;
				using reference = T&;
				using size_type = sarr_ext::size_type;

			private:
				pointer m_ptr;

			public:
				constexpr iterator(T* ptr): m_ptr(ptr) {}

				constexpr T& operator*() const {
					return *m_ptr;
				}
				constexpr T* operator->() const {
					return m_ptr;
				}
				constexpr iterator operator[](size_type offset) const {
					return {m_ptr + offset};
				}

				constexpr iterator& operator+=(size_type diff) {
					m_ptr += diff;
					return *this;
				}
				constexpr iterator& operator-=(size_type diff) {
					m_ptr -= diff;
					return *this;
				}
				constexpr iterator& operator++() {
					++m_ptr;
					return *this;
				}
				constexpr iterator operator++(int) {
					iterator temp(*this);
					++*this;
					return temp;
				}
				constexpr iterator& operator--() {
					--m_ptr;
					return *this;
				}
				constexpr iterator operator--(int) {
					iterator temp(*this);
					--*this;
					return temp;
				}

				constexpr iterator operator+(size_type offset) const {
					return {m_ptr + offset};
				}
				constexpr iterator operator-(size_type offset) const {
					return {m_ptr - offset};
				}
				constexpr difference_type operator-(const iterator& other) const {
					return (difference_type)(m_ptr - other.m_ptr);
				}

				GAIA_NODISCARD constexpr bool operator==(const iterator& other) const {
					return m_ptr == other.m_ptr;
				}
				GAIA_NODISCARD constexpr bool operator!=(const iterator& other) const {
					return m_ptr != other.m_ptr;
				}
				GAIA_NODISCARD constexpr bool operator>(const iterator& other) const {
					return m_ptr > other.m_ptr;
				}
				GAIA_NODISCARD constexpr bool operator>=(const iterator& other) const {
					return m_ptr >= other.m_ptr;
				}
				GAIA_NODISCARD constexpr bool operator<(const iterator& other) const {
					return m_ptr < other.m_ptr;
				}
				GAIA_NODISCARD constexpr bool operator<=(const iterator& other) const {
					return m_ptr <= other.m_ptr;
				}
			};

			class iterator_soa {
				friend class sarr_ext;

			public:
				using iterator_category = core::random_access_iterator_tag;
				using value_type = T;
				using difference_type = sarr_ext::size_type;
				// using pointer = T*; not supported
				// using reference = T&; not supported
				using size_type = sarr_ext::size_type;

			private:
				uint8_t* m_ptr;
				uint32_t m_cnt;
				uint32_t m_idx;

			public:
				iterator_soa(uint8_t* ptr, uint32_t cnt, uint32_t idx): m_ptr(ptr), m_cnt(cnt), m_idx(idx) {}

				T operator*() const {
					return mem::data_view_policy<T::Layout, T>::get({m_ptr, m_cnt}, m_idx);
				}

				iterator_soa operator[](size_type offset) const {
					return iterator_soa(m_ptr, m_cnt, m_idx + offset);
				}

				iterator_soa& operator+=(size_type diff) {
					m_idx += diff;
					return *this;
				}
				iterator& operator-=(size_type diff) {
					m_idx -= diff;
					return *this;
				}
				iterator_soa& operator++() {
					++m_idx;
					return *this;
				}
				iterator_soa operator++(int) {
					iterator_soa temp(*this);
					++*this;
					return temp;
				}
				iterator_soa& operator--() {
					--m_idx;
					return *this;
				}
				iterator_soa operator--(int) {
					iterator_soa temp(*this);
					--*this;
					return temp;
				}

				iterator_soa operator+(size_type offset) const {
					return iterator_soa(m_ptr, m_cnt, m_idx + offset);
				}
				iterator_soa operator-(size_type offset) const {
					return iterator_soa(m_ptr, m_cnt, m_idx + offset);
				}
				difference_type operator-(const iterator_soa& other) const {
					GAIA_ASSERT(m_ptr == other.m_ptr);
					return (difference_type)(m_idx - other.m_idx);
				}

				GAIA_NODISCARD bool operator==(const iterator_soa& other) const {
					GAIA_ASSERT(m_ptr == other.m_ptr);
					return m_idx == other.m_idx;
				}
				GAIA_NODISCARD bool operator!=(const iterator_soa& other) const {
					GAIA_ASSERT(m_ptr == other.m_ptr);
					return m_idx != other.m_idx;
				}
				GAIA_NODISCARD bool operator>(const iterator_soa& other) const {
					GAIA_ASSERT(m_ptr == other.m_ptr);
					return m_idx > other.m_idx;
				}
				GAIA_NODISCARD bool operator>=(const iterator_soa& other) const {
					GAIA_ASSERT(m_ptr == other.m_ptr);
					return m_idx >= other.m_idx;
				}
				GAIA_NODISCARD bool operator<(const iterator_soa& other) const {
					GAIA_ASSERT(m_ptr == other.m_ptr);
					return m_idx < other.m_idx;
				}
				GAIA_NODISCARD bool operator<=(const iterator_soa& other) const {
					GAIA_ASSERT(m_ptr == other.m_ptr);
					return m_idx <= other.m_idx;
				}
			};

			constexpr sarr_ext() noexcept {
				mem::construct_elements((pointer)&m_data[0], extent);
			}

			~sarr_ext() {
				mem::destruct_elements((pointer)&m_data[0], extent);
			}

			constexpr sarr_ext(size_type count, reference value) noexcept {
				resize(count);

				for (auto it: *this)
					*it = value;
			}

			constexpr sarr_ext(size_type count) noexcept {
				resize(count);
			}

			template <typename InputIt>
			constexpr sarr_ext(InputIt first, InputIt last) noexcept {
				mem::construct_elements((pointer)&m_data[0], extent);

				const auto count = (size_type)core::distance(first, last);
				resize(count);

				if constexpr (std::is_pointer_v<InputIt>) {
					for (size_type i = 0; i < count; ++i)
						operator[](i) = first[i];
				} else if constexpr (std::is_same_v<typename InputIt::iterator_category, core::random_access_iterator_tag>) {
					for (size_type i = 0; i < count; ++i)
						operator[](i) = *(first[i]);
				} else {
					size_type i = 0;
					for (auto it = first; it != last; ++it)
						operator[](i++) = *it;
				}
			}

			constexpr sarr_ext(std::initializer_list<T> il): sarr_ext(il.begin(), il.end()) {}

			constexpr sarr_ext(const sarr_ext& other): sarr_ext(other.begin(), other.end()) {}

			constexpr sarr_ext(sarr_ext&& other) noexcept: m_cnt(other.m_cnt) {
				GAIA_ASSERT(gaia::mem::addressof(other) != this);

				mem::construct_elements((pointer)&m_data[0], extent);
				mem::move_elements<T>(m_data, other.m_data, 0, other.size(), extent, other.extent);

				other.m_cnt = size_type(0);
			}

			sarr_ext& operator=(std::initializer_list<T> il) {
				*this = sarr_ext(il.begin(), il.end());
				return *this;
			}

			constexpr sarr_ext& operator=(const sarr_ext& other) {
				GAIA_ASSERT(gaia::mem::addressof(other) != this);

				mem::construct_elements((pointer)&m_data[0], extent);
				resize(other.size());
				mem::copy_elements<T>(
						(uint8_t*)&m_data[0], (const uint8_t*)&other.m_data[0], 0, other.size(), extent, other.extent);

				return *this;
			}

			constexpr sarr_ext& operator=(sarr_ext&& other) noexcept {
				GAIA_ASSERT(gaia::mem::addressof(other) != this);

				mem::construct_elements((pointer)&m_data[0], extent);
				resize(other.m_cnt);
				mem::move_elements<T>(
						(uint8_t*)&m_data[0], (const uint8_t*)&other.m_data[0], 0, other.size(), extent, other.extent);

				other.m_cnt = size_type(0);

				return *this;
			}

			GAIA_NODISCARD constexpr pointer data() noexcept {
				return (pointer)&m_data[0];
			}

			GAIA_NODISCARD constexpr const_pointer data() const noexcept {
				return (const_pointer)&m_data[0];
			}

			GAIA_NODISCARD constexpr auto operator[](size_type pos) noexcept -> decltype(view_policy::set({}, 0)) {
				GAIA_ASSERT(pos < size());
				return view_policy::set({(typename view_policy::TargetCastType) & m_data[0], extent}, pos);
			}

			GAIA_NODISCARD constexpr auto operator[](size_type pos) const noexcept -> decltype(view_policy::get({}, 0)) {
				GAIA_ASSERT(pos < size());
				return view_policy::get({(typename view_policy::TargetCastType) & m_data[0], extent}, pos);
			}

			constexpr void push_back(const T& arg) noexcept {
				GAIA_ASSERT(size() < N);

				if constexpr (mem::is_soa_layout_v<T>) {
					operator[](m_cnt++) = arg;
				} else {
					auto* ptr = m_data + sizeof(T) * (m_cnt++);
					::new (ptr) T(arg);
				}
			}

			constexpr void push_back(T&& arg) noexcept {
				GAIA_ASSERT(size() < N);

				if constexpr (mem::is_soa_layout_v<T>) {
					operator[](m_cnt++) = std::forward<T>(arg);
				} else {
					auto* ptr = m_data + sizeof(T) * (m_cnt++);
					::new (ptr) T(std::forward<T>(arg));
				}
			}

			template <typename... Args>
			constexpr auto emplace_back(Args&&... args) {
				GAIA_ASSERT(size() < N);

				if constexpr (mem::is_soa_layout_v<T>) {
					operator[](m_cnt++) = T(std::forward<Args>(args)...);
					return;
				} else {
					auto* ptr = m_data + sizeof(T) * (m_cnt++);
					::new (ptr) T(std::forward<Args>(args)...);
					return (reference)*ptr;
				}
			}

			constexpr void pop_back() noexcept {
				GAIA_ASSERT(!empty());
				if constexpr (mem::is_soa_layout_v<T>) {
					--m_cnt;
				} else {
					auto* ptr = m_data + sizeof(T) * (--m_cnt);
					((pointer)ptr)->~T();
				}
			}

			constexpr iterator erase(iterator pos) noexcept {
				GAIA_ASSERT(pos.m_ptr >= data() && pos.m_ptr < (data() + extent - 1));

				const auto idxSrc = (size_type)core::distance(pos, begin());
				const auto idxDst = size() - 1;

				mem::shift_elements_left<T>(&m_data[0], idxSrc, idxDst);
				--m_cnt;

				return iterator((pointer)m_data + idxSrc);
			}

			iterator_soa erase(iterator_soa pos) noexcept {
				const auto idxSrc = pos.m_idx;
				const auto idxDst = size() - 1;

				mem::shift_elements_left<T>(m_data, idxSrc, idxDst);
				--m_cnt;

				return iterator_soa(m_data, m_cnt, idxSrc);
			}

			constexpr iterator erase(iterator first, iterator last) noexcept {
				GAIA_ASSERT(first.m_cnt >= 0 && first.m_cnt < size());
				GAIA_ASSERT(last.m_cnt >= 0 && last.m_cnt < size());
				GAIA_ASSERT(last.m_cnt >= first.m_cnt);

				const auto cnt = last.m_cnt - first.m_cnt;
				mem::shift_elements_left<T>(&m_data[0], first.cnt, last.cnt);
				m_cnt -= cnt;

				return {(pointer)m_data + size_type(last.m_cnt)};
			}

			iterator_soa erase(iterator_soa first, iterator_soa last) noexcept {
				static_assert(!mem::is_soa_layout_v<T>);
				GAIA_ASSERT(first.m_idx >= 0 && first.m_idx < size());
				GAIA_ASSERT(last.m_idx >= 0 && last.m_idx < size());
				GAIA_ASSERT(last.m_idx >= first.m_idx);

				const auto cnt = last.m_idx - first.m_idx;
				mem::shift_elements_left<T>(m_data, first.cnt, last.cnt);
				m_cnt -= cnt;

				return iterator_soa(m_data, m_cnt, last.m_cnt);
			}

			constexpr void clear() noexcept {
				resize(0);
			}

			constexpr void resize(size_type size) noexcept {
				GAIA_ASSERT(size <= N);
				m_cnt = size;
			}

			GAIA_NODISCARD constexpr size_type size() const noexcept {
				return m_cnt;
			}

			GAIA_NODISCARD constexpr bool empty() const noexcept {
				return size() == 0;
			}

			GAIA_NODISCARD constexpr size_type max_size() const noexcept {
				return N;
			}

			GAIA_NODISCARD constexpr auto front() noexcept {
				GAIA_ASSERT(!empty());
				if constexpr (mem::is_soa_layout_v<T>)
					return *begin();
				else
					return (reference)*begin();
			}

			GAIA_NODISCARD constexpr auto front() const noexcept {
				GAIA_ASSERT(!empty());
				if constexpr (mem::is_soa_layout_v<T>)
					return *begin();
				else
					return (const_reference)*begin();
			}

			GAIA_NODISCARD constexpr auto back() noexcept {
				GAIA_ASSERT(!empty());
				if constexpr (mem::is_soa_layout_v<T>)
					return (operator[])(N - 1);
				else
					return (reference) operator[](N - 1);
			}

			GAIA_NODISCARD constexpr auto back() const noexcept {
				GAIA_ASSERT(!empty());
				if constexpr (mem::is_soa_layout_v<T>)
					return operator[](N - 1);
				else
					return (const_reference) operator[](N - 1);
			}

			GAIA_NODISCARD constexpr auto begin() const noexcept {
				if constexpr (mem::is_soa_layout_v<T>)
					return iterator_soa(m_data, size(), 0);
				else
					return iterator((pointer)&m_data[0]);
			}

			GAIA_NODISCARD constexpr auto rbegin() const noexcept {
				if constexpr (mem::is_soa_layout_v<T>)
					return iterator_soa(m_data, size(), size() - 1);
				else
					return iterator((pointer)&back());
			}

			GAIA_NODISCARD constexpr auto end() const noexcept {
				if constexpr (mem::is_soa_layout_v<T>)
					return iterator_soa(m_data, size(), size());
				else
					return iterator((pointer)&m_data[0] + size());
			}

			GAIA_NODISCARD constexpr auto rend() const noexcept {
				if constexpr (mem::is_soa_layout_v<T>)
					return iterator_soa(m_data, size(), -1);
				else
					return iterator((pointer)m_data - 1);
			}

			GAIA_NODISCARD constexpr bool operator==(const sarr_ext& other) const {
				if (m_cnt != other.m_cnt)
					return false;
				const size_type n = size();
				for (size_type i = 0; i < n; ++i)
					if (!(m_data[i] == other.m_data[i]))
						return false;
				return true;
			}

			template <size_t Item>
			auto soa_view_mut() noexcept {
				return mem::data_view_policy<T::Layout, T>::template get<Item>(
						std::span<uint8_t>{(uint8_t*)&m_data[0], extent});
			}

			template <size_t Item>
			auto soa_view() const noexcept {
				return mem::data_view_policy<T::Layout, T>::template get<Item>(
						std::span<const uint8_t>{(const uint8_t*)&m_data[0], extent});
			}
		};

		namespace detail {
			template <typename T, uint32_t N, uint32_t... I>
			constexpr sarr_ext<std::remove_cv_t<T>, N> to_sarray_impl(T (&a)[N], std::index_sequence<I...> /*no_name*/) {
				return {{a[I]...}};
			}
		} // namespace detail

		template <typename T, uint32_t N>
		constexpr sarr_ext<std::remove_cv_t<T>, N> to_sarray(T (&a)[N]) {
			return detail::to_sarray_impl(a, std::make_index_sequence<N>{});
		}

	} // namespace cnt

} // namespace gaia

namespace std {
	template <typename T, uint32_t N>
	struct tuple_size<gaia::cnt::sarr_ext<T, N>>: std::integral_constant<uint32_t, N> {};

	template <size_t I, typename T, uint32_t N>
	struct tuple_element<I, gaia::cnt::sarr_ext<T, N>> {
		using type = T;
	};
} // namespace std