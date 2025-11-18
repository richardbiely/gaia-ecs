#pragma once
#include "gaia/config/config.h"

#include <cstddef>
#include <initializer_list>
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
		namespace sarr_ext_detail {
			using difference_type = uint32_t;
			using size_type = uint32_t;
		} // namespace sarr_ext_detail

		//! Array of elements of type \tparam T with fixed capacity \tparam N and variable size allocated on stack.
		//! Interface compatiblity with std::array where it matters.
		template <typename T, sarr_ext_detail::size_type N>
		class sarr_ext {
		public:
			static_assert(N > 0);

			using value_type = T;
			using reference = T&;
			using const_reference = const T&;
			using pointer = T*;
			using const_pointer = const T*;
			using view_policy = mem::data_view_policy_aos<T>;
			using difference_type = sarr_ext_detail::difference_type;
			using size_type = sarr_ext_detail::size_type;

			using iterator = pointer;
			using const_iterator = const_pointer;
			using iterator_category = core::random_access_iterator_tag;

			static constexpr size_t value_size = sizeof(T);
			static constexpr size_type extent = N;
			static constexpr uint32_t allocated_bytes = view_policy::get_min_byte_size(0, N);

		private:
			mem::raw_data_holder<T, allocated_bytes> m_data;
			size_type m_cnt = size_type(0);

		public:
			constexpr sarr_ext() noexcept = default;
			constexpr sarr_ext(core::zero_t) noexcept {}

			~sarr_ext() {
				core::call_dtor_n(data(), m_cnt);
			}

			constexpr sarr_ext(size_type count, const_reference value) noexcept {
				resize(count);
				for (auto it: *this)
					*it = value;
			}

			constexpr sarr_ext(size_type count) noexcept {
				resize(count);
			}

			template <typename InputIt>
			constexpr sarr_ext(InputIt first, InputIt last) noexcept {
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
						operator[](++i) = *it;
				}
			}

			constexpr sarr_ext(std::initializer_list<T> il): sarr_ext(il.begin(), il.end()) {}

			constexpr sarr_ext(const sarr_ext& other): sarr_ext(other.begin(), other.end()) {}

			constexpr sarr_ext(sarr_ext&& other) noexcept: m_cnt(other.m_cnt) {
				GAIA_ASSERT(core::addressof(other) != this);

				core::call_ctor_raw_n(data(), extent);
				mem::move_elements<T>(m_data, other.m_data, other.size(), 0, extent, other.extent);

				other.m_cnt = size_type(0);
			}

			sarr_ext& operator=(std::initializer_list<T> il) {
				*this = sarr_ext(il.begin(), il.end());
				return *this;
			}

			constexpr sarr_ext& operator=(const sarr_ext& other) {
				GAIA_ASSERT(core::addressof(other) != this);

				resize(other.size());
				mem::copy_elements<T>(
						GAIA_ACC((uint8_t*)&m_data[0]), GAIA_ACC((const uint8_t*)&other.m_data[0]), other.size(), 0, extent,
						other.extent);

				return *this;
			}

			constexpr sarr_ext& operator=(sarr_ext&& other) noexcept {
				GAIA_ASSERT(core::addressof(other) != this);

				resize(other.m_cnt);
				mem::move_elements<T>(
						GAIA_ACC((uint8_t*)&m_data[0]), GAIA_ACC((uint8_t*)&other.m_data[0]), other.size(), 0, extent,
						other.extent);

				other.m_cnt = size_type(0);

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

			constexpr void push_back(const T& arg) noexcept {
				GAIA_ASSERT(size() < N);

				auto* ptr = &data()[m_cnt++];
				core::call_ctor(ptr, arg);
			}

			constexpr void push_back(T&& arg) noexcept {
				GAIA_ASSERT(size() < N);

				auto* ptr = &data()[m_cnt++];
				core::call_ctor(ptr, GAIA_MOV(arg));
			}

			template <typename... Args>
			constexpr decltype(auto) emplace_back(Args&&... args) {
				GAIA_ASSERT(size() < N);

				auto* ptr = &data()[m_cnt++];
				core::call_ctor(ptr, GAIA_FWD(args)...);
				return (reference)*ptr;
			}

			constexpr void pop_back() noexcept {
				GAIA_ASSERT(!empty());

				auto* ptr = &data()[m_cnt];
				core::call_dtor(ptr);

				--m_cnt;
			}

			//! Insert the element to the position given by iterator \param pos
			iterator insert(iterator pos, const T& arg) noexcept {
				GAIA_ASSERT(size() < N);
				GAIA_ASSERT(pos >= data());
				GAIA_ASSERT(empty() || (pos < iterator(data() + size())));

				const auto idxSrc = (size_type)core::distance(begin(), pos);
				const auto idxDst = (size_type)core::distance(begin(), end());

				mem::shift_elements_right<T>(m_data, idxDst, idxSrc, extent);

				auto* ptr = &data()[idxSrc];
				core::call_ctor(ptr, arg);

				++m_cnt;

				return iterator(&data()[idxSrc]);
			}

			//! Insert the element to the position given by iterator \param pos
			iterator insert(iterator pos, T&& arg) noexcept {
				GAIA_ASSERT(size() < N);
				GAIA_ASSERT(pos >= data());
				GAIA_ASSERT(empty() || (pos < iterator(data() + size())));

				const auto idxSrc = (size_type)core::distance(begin(), pos);
				const auto idxDst = (size_type)core::distance(begin(), end());

				mem::shift_elements_right<T>(m_data, idxDst, idxSrc, extent);

				auto* ptr = &data()[idxSrc];
				core::call_ctor(ptr, GAIA_MOV(arg));

				++m_cnt;

				return iterator(&data()[idxSrc]);
			}

			//! Removes the element at pos
			//! \param pos Iterator to the element to remove
			constexpr iterator erase(iterator pos) noexcept {
				GAIA_ASSERT(pos >= data());
				GAIA_ASSERT(empty() || (pos < iterator(data() + size())));

				if (empty())
					return end();

				const auto idxSrc = (size_type)core::distance(begin(), pos);
				const auto idxDst = (size_type)core::distance(begin(), end()) - 1;

				mem::shift_elements_left<T>(m_data, idxDst, idxSrc, extent);
				// Destroy if it's the last element
				auto* ptr = &data()[m_cnt - 1];
				core::call_dtor(ptr);

				--m_cnt;

				return iterator(&data()[idxSrc]);
			}

			//! Removes the elements in the range [first, last)
			//! \param first Iterator to the element to remove
			//! \param last Iterator to the one beyond the last element to remove
			iterator erase(iterator first, iterator last) noexcept {
				GAIA_ASSERT(first >= data())
				GAIA_ASSERT(empty() || (first < iterator(data() + size())));
				GAIA_ASSERT(last > first);
				GAIA_ASSERT(last <= (data() + size()));

				if (empty())
					return end();

				const auto idxSrc = (size_type)core::distance(begin(), first);
				const auto idxDst = size();
				const auto cnt = (size_type)(last - first);

				mem::shift_elements_left_n<T>(m_data, idxDst, idxSrc, cnt, extent);
				// Destroy if it's the last element
				core::call_dtor_n(&data()[m_cnt - cnt], cnt);

				m_cnt -= cnt;

				return iterator(&data()[idxSrc]);
			}

			//! Removes the element at index \param pos
			iterator erase_at(size_type pos) noexcept {
				GAIA_ASSERT(pos < size());

				const auto idxSrc = pos;
				const auto idxDst = (size_type)core::distance(begin(), end()) - 1;

				mem::shift_elements_left<T>(m_data, idxDst, idxSrc, extent);
				// Destroy if it's the last element
				auto* ptr = &data()[m_cnt - 1];
				core::call_dtor(ptr);

				--m_cnt;

				return iterator(&data()[idxSrc]);
			}

			constexpr void clear() noexcept {
				resize(0);
			}

			constexpr void resize(size_type count) noexcept {
				GAIA_ASSERT(count <= max_size());

				// Resizing to a smaller size
				if (count <= m_cnt) {
					// Destroy elements at the end
					core::call_dtor_n(&data()[count], size() - count);
				} else
				// Resizing to a bigger size but still within allocated capacity
				{
					// Construct new elements
					core::call_ctor_n(&data()[size()], count - size());
				}

				m_cnt = count;
			}

			//! Removes all elements that fail the predicate.
			//! \param func A lambda or a functor with the bool operator()(Container::value_type&) overload.
			//! \return The new size of the array.
			template <typename Func>
			auto retain(Func&& func) {
				size_type erased = 0;
				size_type idxDst = 0;
				size_type idxSrc = 0;

				while (idxSrc < m_cnt) {
					if (func(operator[](idxSrc))) {
						if (idxDst < idxSrc) {
							auto* ptr = (uint8_t*)data();
							mem::move_element<T>(ptr, ptr, idxDst, idxSrc, max_size(), max_size());
							auto* ptr2 = &data()[idxSrc];
							core::call_dtor(ptr2);
						}
						++idxDst;
					} else {
						auto* ptr = &data()[idxSrc];
						core::call_dtor(ptr);
						++erased;
					}

					++idxSrc;
				}

				m_cnt -= erased;
				return idxDst;
			}

			GAIA_NODISCARD constexpr size_type size() const noexcept {
				return m_cnt;
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

			GAIA_NODISCARD constexpr decltype(auto) front() noexcept {
				GAIA_ASSERT(!empty());
				return (reference)*begin();
			}

			GAIA_NODISCARD constexpr decltype(auto) front() const noexcept {
				GAIA_ASSERT(!empty());
				return (const_reference)*begin();
			}

			GAIA_NODISCARD constexpr decltype(auto) back() noexcept {
				GAIA_ASSERT(!empty());
				return (reference) operator[](m_cnt - 1);
			}

			GAIA_NODISCARD constexpr decltype(auto) back() const noexcept {
				GAIA_ASSERT(!empty());
				return (const_reference) operator[](m_cnt - 1);
			}

			GAIA_NODISCARD constexpr auto begin() noexcept {
				return iterator(data());
			}

			GAIA_NODISCARD constexpr auto begin() const noexcept {
				return const_iterator(data());
			}

			GAIA_NODISCARD constexpr auto cbegin() const noexcept {
				return const_iterator(data());
			}

			GAIA_NODISCARD constexpr auto rbegin() noexcept {
				return iterator((pointer)&back());
			}

			GAIA_NODISCARD constexpr auto rbegin() const noexcept {
				return const_iterator((pointer)&back());
			}

			GAIA_NODISCARD constexpr auto crbegin() const noexcept {
				return const_iterator((pointer)&back());
			}

			GAIA_NODISCARD constexpr auto end() noexcept {
				return iterator(GAIA_ACC((pointer)&m_data[0]) + size());
			}

			GAIA_NODISCARD constexpr auto end() const noexcept {
				return const_iterator(GAIA_ACC((const_pointer)&m_data[0]) + size());
			}

			GAIA_NODISCARD constexpr auto cend() const noexcept {
				return const_iterator(GAIA_ACC((const_pointer)&m_data[0]) + size());
			}

			GAIA_NODISCARD constexpr auto rend() noexcept {
				return iterator(GAIA_ACC((pointer)&m_data[0]) - 1);
			}

			GAIA_NODISCARD constexpr auto rend() const noexcept {
				return const_iterator(GAIA_ACC((const_pointer)&m_data[0]) - 1);
			}

			GAIA_NODISCARD constexpr auto crend() const noexcept {
				return const_iterator(GAIA_ACC((const_pointer)&m_data[0]) - 1);
			}

			GAIA_NODISCARD constexpr bool operator==(const sarr_ext& other) const {
				if (m_cnt != other.m_cnt)
					return false;
				const size_type n = size();
				for (size_type i = 0; i < n; ++i)
					if (!(operator[](i) == other[i]))
						return false;
				return true;
			}

			GAIA_NODISCARD constexpr bool operator!=(const sarr_ext& other) const {
				return !operator==(other);
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