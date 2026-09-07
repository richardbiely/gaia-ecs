#pragma once
#include "gaia/config/config.h"

#include "gaia/core/utility.h"
#include "gaia/mem/mem_utils.h"

namespace gaia {
	namespace cnt {
		//! \cond INTERNAL
		namespace detail {
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
