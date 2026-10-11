#pragma once
#include "gaia/config/config.h"

#include "gaia/core/utility.h"
#include "gaia/mem/mem_utils.h"

namespace gaia {
	namespace cnt {
		//! How a dynamic array's resize() grows its capacity once the new size outgrows it.
		enum class ResizePolicy : uint8_t {
			//! Allocate exactly the new size.
			Exact,
			//! Grow capacity by half again, as appending does, or to the new size when that is larger,
			//! so growing an array a few elements at a time relocates it only a logarithmic number of
			//! times.
			Scale,
		};

		//! \cond INTERNAL
		namespace detail {
			//! Capacity a dynamic array's resize() allocates for \a count values once they outgrow
			//! \a cap.
			//! \tparam SizeType Size type of the array.
			//! \param policy How capacity grows: exactly to \a count, or by half again.
			//! \param cap Current capacity, smaller than \a count.
			//! \param count Number of values the array holds after the resize.
			//! \return \a count for ResizePolicy::Exact; the larger of \a count and one and a half
			//! times \a cap for ResizePolicy::Scale.
			template <typename SizeType>
			GAIA_NODISCARD constexpr SizeType resize_capacity(ResizePolicy policy, SizeType cap, SizeType count) noexcept {
				return policy == ResizePolicy::Scale ? core::get_max(count, (SizeType)((cap * 3 + 1) / 2)) : count;
			}

			//! Stably compacts an array using assignment, then destroys the discarded AoS tail.
			//! SoA moves fields through the existing memory helper without destroying AoS objects.
			//! The caller adjusts sanitizer annotations after this function returns.
			//! \tparam SOA Whether the array uses SoA field storage.
			//! \tparam Array Gaia array implementation.
			//! \tparam Func Predicate callable type.
			//! \param arr Array whose live elements are compacted.
			//! \param count Reference to the array's live size, updated after compaction.
			//! \param func Predicate called once per element in order. May modify the element,
			//! but must not change the array's size or storage.
			//! \return Number of retained elements.
			template <bool SOA, typename Array, typename Func>
			uint32_t retain_array(Array& arr, uint32_t& count, Func&& func) {
				using T = typename Array::value_type;
				uint32_t erased = 0;
				uint32_t idxDst = 0;
				uint32_t idxSrc = 0;

				while (idxSrc < count) {
					if (func(arr[idxSrc])) {
						if (idxDst < idxSrc) {
							auto* ptr = (uint8_t*)arr.data();
							mem::move_element<T, SOA>(ptr, ptr, idxDst, idxSrc, arr.capacity(), arr.capacity());
						}
						++idxDst;
					} else {
						++erased;
					}
					++idxSrc;
				}

				//! Assignment destinations must remain alive throughout the scan.
				if constexpr (!SOA) {
					if (erased != 0)
						core::call_dtor_n(arr.data() + idxDst, erased);
				}
				count -= erased;
				return idxDst;
			}
		} // namespace detail
		//! \endcond
	} // namespace cnt
} // namespace gaia
