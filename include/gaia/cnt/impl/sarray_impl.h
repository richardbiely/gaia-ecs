#pragma once
#include "gaia/config/config.h"

#include <cstddef>
#include <new>
#include <tuple>
#include <type_traits>
#include <utility>

#include "gaia/core/iterator.h"
#include "gaia/core/utility.h"
#include "gaia/mem/data_layout_policy.h"
#include "gaia/mem/mem_utils.h"
#include "gaia/mem/raw_data_holder.h"

namespace gaia {
	namespace cnt {
		namespace sarr_detail {
			using difference_type = uint32_t;
			using size_type = uint32_t;
		} // namespace sarr_detail

		//! Array of elements of type \tparam T with fixed size and capacity \tparam N allocated on stack.
		//! Interface compatiblity with std::array where it matters.
		template <typename T, sarr_detail::size_type N>
		class sarr {
		public:
			static_assert(N > 0);

			using value_type = T;
			using reference = T&;
			using const_reference = const T&;
			using pointer = T*;
			using const_pointer = T*;
			using view_policy = mem::data_view_policy_aos<T>;
			using difference_type = sarr_detail::difference_type;
			using size_type = sarr_detail::size_type;

			using iterator = pointer;
			using const_iterator = const_pointer;
			using iterator_category = core::random_access_iterator_tag;

			static constexpr size_type extent = N;
			static constexpr uint32_t allocated_bytes = view_policy::get_min_byte_size(0, N);

			mem::raw_data_holder<T, allocated_bytes> m_data;

			constexpr sarr() noexcept {
				core::call_ctor_raw_n(data(), extent);
			}

			//! Zero-initialization constructor. Because sarr is not aggretate type, doing: sarr<int,10> tmp{} does not
			//! zero-initialize its internals. We need to be explicit about our intent and use a special constructor.
			constexpr sarr(core::zero_t) noexcept {
				core::call_ctor_raw_n(data(), extent);

				// explicit zeroing
				for (auto i = (size_type)0; i < extent; ++i)
					operator[](i) = {};
			}

			~sarr() {
				core::call_dtor_n(data(), extent);
			}

			template <typename InputIt>
			constexpr sarr(InputIt first, InputIt last) noexcept {
				core::call_ctor_raw_n(data(), extent);

				const auto count = (size_type)core::distance(first, last);

				if constexpr (std::is_pointer_v<InputIt>) {
					for (size_type i = 0; i < count; ++i)
						operator[](i) = first[i];
				} else if constexpr (std::is_same_v<typename InputIt::iterator_category, core::random_access_iterator_tag>) {
					for (size_type i = 0; i < count; ++i)
						operator[](i) = *(first[i]);
				} else {
					size_type i = 0;
					for (auto it = first; it != last; ++it)
						operator[](++i) = *it;
				}
			}

			constexpr sarr(std::initializer_list<T> il): sarr(il.begin(), il.end()) {}

			constexpr sarr(const sarr& other): sarr(other.begin(), other.end()) {}

			constexpr sarr(sarr&& other) noexcept {
				GAIA_ASSERT(core::addressof(other) != this);

				core::call_ctor_raw_n(data(), extent);
				mem::move_elements<T>((uint8_t*)m_data, (uint8_t*)other.m_data, other.size(), 0, extent, other.extent);
			}

			sarr& operator=(std::initializer_list<T> il) {
				*this = sarr(il.begin(), il.end());
				return *this;
			}

			constexpr sarr& operator=(const sarr& other) {
				GAIA_ASSERT(core::addressof(other) != this);

				core::call_ctor_raw_n(data(), extent);
				mem::copy_elements<T>(
						GAIA_ACC((uint8_t*)&m_data[0]), GAIA_ACC((const uint8_t*)&other.m_data[0]), other.size(), 0, extent,
						other.extent);

				return *this;
			}

			constexpr sarr& operator=(sarr&& other) noexcept {
				GAIA_ASSERT(core::addressof(other) != this);

				core::call_ctor_raw_n(data(), extent);
				mem::move_elements<T>(
						GAIA_ACC((uint8_t*)&m_data[0]), GAIA_ACC((uint8_t*)&other.m_data[0]), other.size(), 0, extent,
						other.extent);

				return *this;
			}

			GAIA_CLANG_WARNING_PUSH()
			// Memory is aligned so we can silence this warning
			GAIA_CLANG_WARNING_DISABLE("-Wcast-align")

			GAIA_NODISCARD constexpr pointer data() noexcept {
				return GAIA_ACC((pointer)&m_data[0]);
			}

			GAIA_NODISCARD constexpr const_pointer data() const noexcept {
				return GAIA_ACC((const_pointer)&m_data[0]);
			}

			GAIA_NODISCARD constexpr decltype(auto) operator[](size_type pos) noexcept {
				GAIA_ASSERT(pos < size());
				return view_policy::set({GAIA_ACC((typename view_policy::TargetCastType) & m_data[0]), extent}, pos);
			}

			GAIA_NODISCARD constexpr decltype(auto) operator[](size_type pos) const noexcept {
				GAIA_ASSERT(pos < size());
				return view_policy::get({GAIA_ACC((typename view_policy::TargetCastType) & m_data[0]), extent}, pos);
			}

			GAIA_CLANG_WARNING_POP()

			GAIA_NODISCARD constexpr size_type size() const noexcept {
				return N;
			}

			GAIA_NODISCARD constexpr bool empty() const noexcept {
				return begin() == end();
			}

			GAIA_NODISCARD constexpr size_type max_size() const noexcept {
				return N;
			}

			GAIA_NODISCARD constexpr decltype(auto) front() noexcept {
				return (reference)*begin();
			}

			GAIA_NODISCARD constexpr decltype(auto) front() const noexcept {
				return (const_reference)*begin();
			}

			GAIA_NODISCARD constexpr decltype(auto) back() noexcept {
				return (reference) operator[](N - 1);
			}

			GAIA_NODISCARD constexpr decltype(auto) back() const noexcept {
				return (const_reference) operator[](N - 1);
			}

			GAIA_NODISCARD constexpr auto begin() noexcept {
				return iterator(GAIA_ACC(&m_data[0]));
			}

			GAIA_NODISCARD constexpr decltype(auto) begin() const noexcept {
				return const_iterator(GAIA_ACC(&m_data[0]));
			}

			GAIA_NODISCARD constexpr auto cbegin() const noexcept {
				return const_iterator(GAIA_ACC(&m_data[0]));
			}

			GAIA_NODISCARD constexpr auto rbegin() noexcept {
				return iterator((pointer)&back());
			}

			GAIA_NODISCARD constexpr decltype(auto) rbegin() const noexcept {
				return const_iterator((const_pointer)&back());
			}

			GAIA_NODISCARD constexpr auto crbegin() const noexcept {
				return const_iterator((const_pointer)&back());
			}

			GAIA_NODISCARD constexpr auto end() noexcept {
				return iterator(GAIA_ACC((pointer)&m_data[0]) + size());
			}

			GAIA_NODISCARD constexpr decltype(auto) end() const noexcept {
				return const_iterator(GAIA_ACC((const_pointer)&m_data[0]) + size());
			}

			GAIA_NODISCARD constexpr auto cend() const noexcept {
				return const_iterator(GAIA_ACC((const_pointer)&m_data[0]) + size());
			}

			GAIA_NODISCARD constexpr auto rend() noexcept {
				return iterator(GAIA_ACC((pointer)&m_data[0]) - 1);
			}

			GAIA_NODISCARD constexpr decltype(auto) rend() const noexcept {
				return const_iterator(GAIA_ACC((const_pointer)&m_data[0]) - 1);
			}

			GAIA_NODISCARD constexpr auto crend() const noexcept {
				return const_iterator(GAIA_ACC((const_pointer)&m_data[0]) - 1);
			}

			GAIA_NODISCARD constexpr bool operator==(const sarr& other) const {
				for (size_type i = 0; i < N; ++i)
					if (!(operator[](i) == other[i]))
						return false;
				return true;
			}

			GAIA_NODISCARD constexpr bool operator!=(const sarr& other) const {
				return !operator==(other);
			}
		};

		namespace detail {
			template <typename T, uint32_t N, uint32_t... I>
			constexpr sarr<std::remove_cv_t<T>, N> to_array_impl(T (&a)[N], std::index_sequence<I...> /*no_name*/) {
				return {{a[I]...}};
			}
		} // namespace detail

		template <typename T, uint32_t N>
		constexpr sarr<std::remove_cv_t<T>, N> to_array(T (&a)[N]) {
			return detail::to_array_impl(a, std::make_index_sequence<N>{});
		}

		template <typename T, typename... U>
		sarr(T, U...) -> sarr<T, 1 + (uint32_t)sizeof...(U)>;

	} // namespace cnt
} // namespace gaia

namespace std {
	template <typename T, uint32_t N>
	struct tuple_size<gaia::cnt::sarr<T, N>>: std::integral_constant<uint32_t, N> {};

	template <size_t I, typename T, uint32_t N>
	struct tuple_element<I, gaia::cnt::sarr<T, N>> {
		using type = T;
	};
} // namespace std
