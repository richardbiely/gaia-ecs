#pragma once
#include <cstddef>
#include <type_traits>
#include <utility>

#include "../utils/iterator.h"
#include "../utils/mem.h"

namespace gaia {
	namespace containers {
		//! Array of elements of type \tparam T with fixed size and capacity \tparam N allocated on stack.
		//! Interface compatiblity with std::array where it matters.
		template <typename T, uint32_t N>
		class sringbuffer {
		public:
			static_assert(N > 1);

			using iterator_category = GAIA_UTIL::random_access_iterator_tag;
			using value_type = T;
			using reference = T&;
			using const_reference = const T&;
			using pointer = T*;
			using const_pointer = T*;
			using difference_type = std::ptrdiff_t;
			using size_type = decltype(N);

			static constexpr size_type extent = N;

			size_type m_tail{};
			size_type m_size{};
			T m_data[N];

			sringbuffer() = default;

			template <typename InputIt>
			sringbuffer(InputIt first, InputIt last) {
				const auto count = (size_type)GAIA_UTIL::distance(first, last);
				GAIA_ASSERT(count <= max_size());
				if (count < 1)
					return;

				m_size = count;
				m_tail = 0;

				if constexpr (std::is_pointer_v<InputIt>) {
					for (size_type i = 0; i < count; ++i)
						m_data[i] = first[i];
				} else if constexpr (std::is_same_v<
																 typename InputIt::iterator_category, GAIA_UTIL::random_access_iterator_tag>) {
					for (size_type i = 0; i < count; ++i)
						m_data[i] = *(first[i]);
				} else {
					size_type i = 0;
					for (auto it = first; it != last; ++it)
						m_data[i++] = *it;
				}
			}

			sringbuffer(std::initializer_list<T> il): sringbuffer(il.begin(), il.end()) {}

			sringbuffer(const sringbuffer& other): sringbuffer(other.begin(), other.end()) {}

			sringbuffer(sringbuffer&& other) noexcept: m_tail(other.m_tail), m_size(other.m_size) {
				GAIA_ASSERT(GAIA_UTIL::addressof(other) != this);

				utils::transfer_elements(m_data, other.m_data, other.size());

				other.m_tail = size_type(0);
				other.m_size = size_type(0);
			}

			GAIA_NODISCARD sringbuffer& operator=(std::initializer_list<T> il) {
				*this = sringbuffer(il.begin(), il.end());
				return *this;
			}

			GAIA_NODISCARD constexpr sringbuffer& operator=(const sringbuffer& other) noexcept {
				GAIA_ASSERT(GAIA_UTIL::addressof(other) != this);

				utils::copy_elements(m_data, other.m_data, other.size());

				m_tail = other.m_tail;
				m_size = other.m_size;

				return *this;
			}

			GAIA_NODISCARD constexpr sringbuffer& operator=(sringbuffer&& other) noexcept {
				GAIA_ASSERT(GAIA_UTIL::addressof(other) != this);

				utils::transfer_elements(m_data, other.m_data, other.size());

				m_tail = other.m_tail;
				m_size = other.m_size;

				other.m_tail = size_type(0);
				other.m_size = size_type(0);

				return *this;
			}

			~sringbuffer() = default;

			void push_back(const T& arg) {
				GAIA_ASSERT(m_size < N);
				const auto head = (m_tail + m_size) % N;
				m_data[head] = arg;
				++m_size;
			}

			void push_back(T&& arg) {
				GAIA_ASSERT(m_size < N);
				const auto head = (m_tail + m_size) % N;
				m_data[head] = std::forward<T>(arg);
				++m_size;
			}

			template <typename... Args>
			void emplace_back(Args&&... args) {
				GAIA_ASSERT(m_size < N);
				const auto head = (m_tail + m_size) % N;
				m_data[head] = {std::forward<Args>(args)...};
				++m_size;
			}

			void pop_front(T& out) {
				GAIA_ASSERT(!empty());
				out = m_data[m_tail];
				m_tail = (m_tail + 1) % N;
				--m_size;
			}

			void pop_front(T&& out) {
				GAIA_ASSERT(!empty());
				out = std::forward<T>(m_data[m_tail]);
				m_tail = (m_tail + 1) % N;
				--m_size;
			}

			void pop_back(T& out) {
				GAIA_ASSERT(m_size < N);
				const auto head = (m_tail + m_size - 1) % N;
				out = m_data[head];
				--m_size;
			}

			void pop_back(T&& out) {
				GAIA_ASSERT(m_size < N);
				const auto head = (m_tail + m_size - 1) % N;
				out = std::forward<T>(m_data[head]);
				--m_size;
			}

			GAIA_NODISCARD constexpr size_type size() const noexcept {
				return m_size;
			}

			GAIA_NODISCARD constexpr bool empty() const noexcept {
				return !m_size;
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

			GAIA_NODISCARD bool operator==(const sringbuffer& other) const {
				for (size_type i = 0; i < N; ++i)
					if (!(m_data[i] == other.m_data[i]))
						return false;
				return true;
			}
		};

		namespace detail {
			template <typename T, std::size_t N, std::size_t... I>
			constexpr sringbuffer<std::remove_cv_t<T>, N>
			to_sringbuffer_impl(T (&a)[N], std::index_sequence<I...> /*no_name*/) {
				return {{a[I]...}};
			}
		} // namespace detail

		template <typename T, std::size_t N>
		constexpr sringbuffer<std::remove_cv_t<T>, N> to_sringbuffer(T (&a)[N]) {
			return detail::to_sringbuffer_impl(a, std::make_index_sequence<N>{});
		}

		template <typename T, typename... U>
		sringbuffer(T, U...) -> sringbuffer<T, 1 + sizeof...(U)>;

	} // namespace containers

} // namespace gaia
