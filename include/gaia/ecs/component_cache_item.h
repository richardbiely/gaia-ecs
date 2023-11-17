#pragma once
#include "../config/config.h"

#include <cstdint>
#include <type_traits>

#include "../cnt/darray.h"
#include "../cnt/map.h"
#include "../config/logging.h"
#include "../meta/type_info.h"
#include "component.h"
#include "component_desc.h"
#include "id.h"

namespace gaia {
	namespace ecs {
		struct ComponentCacheItem final {
			using FuncCtor = void(void*, uint32_t);
			using FuncDtor = void(void*, uint32_t);
			using FuncCopy = void(void*, void*);
			using FuncMove = void(void*, void*);
			using FuncSwap = void(void*, void*);
			using FuncCmp = bool(const void*, const void*);

			//! Unique component identifier
			Component comp = {IdentifierBad};
			//! Complex hash used for look-ups
			ComponentLookupHash hashLookup;
			//! Simple hash used for matching component
			ComponentMatcherHash matcherHash;
			//! If component is SoA, this stores how many bytes each of the elemenets take
			uint8_t soaSizes[meta::StructToTupleMaxTypes];

			//! Component name
			std::span<const char> name;
			//! Function to call when the component needs to be constructed
			FuncCtor* func_ctor = nullptr;
			//! Function to call when the component needs to be move constructed
			FuncMove* func_move_ctor = nullptr;
			//! Function to call when the component needs to be copy constructed
			FuncCopy* func_copy_ctor = nullptr;
			//! Function to call when the component needs to be destroyed
			FuncDtor* func_dtor = nullptr;
			//! Function to call when the component needs to be copied
			FuncCopy* func_copy = nullptr;
			//! Fucntion to call when the component needs to be moved
			FuncMove* func_move = nullptr;
			//! Function to call when the component needs to swap
			FuncSwap* func_swap = nullptr;
			//! Function to call when comparing two components of the same type
			FuncCmp* func_cmp = nullptr;

			void ctor_from(void* pSrc, void* pDst) const {
				if (func_move_ctor != nullptr)
					func_move_ctor(pSrc, pDst);
				else if (func_copy_ctor != nullptr)
					func_copy_ctor(pSrc, pDst);
				else
					memmove(pDst, (const void*)pSrc, comp.size());
			}

			void move(void* pSrc, void* pDst) const {
				if (func_move != nullptr)
					func_move(pSrc, pDst);
				else
					copy(pSrc, pDst);
			}

			void copy(void* pSrc, void* pDst) const {
				if (func_copy != nullptr)
					func_copy(pSrc, pDst);
				else
					memmove(pDst, (const void*)pSrc, comp.size());
			}

			void dtor(void* pSrc) const {
				if (func_dtor != nullptr)
					func_dtor(pSrc, 1);
			}

			void swap(void* pLeft, void* pRight) const {
				// Function pointer is not provided only in 2 cases:
				// 1) SoA component
				GAIA_ASSERT(func_swap != nullptr);
				func_swap(pLeft, pRight);
			}

			bool cmp(const void* pLeft, const void* pRight) const {
				// We only ever compare components during defragmentation when they are uni components.
				// For those cases the comparison operator must be present.
				// Correct setup should be ensured by a compile time check when adding a new component.
				GAIA_ASSERT(func_cmp != nullptr);
				return func_cmp(pLeft, pRight);
			}

			GAIA_NODISCARD uint32_t calc_new_mem_offset(uint32_t addr, size_t N) const noexcept {
				if (comp.soa() == 0) {
					addr = (uint32_t)mem::detail::get_aligned_byte_offset(addr, comp.alig(), comp.size(), N);
				} else {
					GAIA_FOR(comp.soa()) {
						addr = (uint32_t)mem::detail::get_aligned_byte_offset(addr, comp.alig(), soaSizes[i], N);
					}
					// TODO: Magic offset. Otherwise, SoA data might leak past the chunk boundary when accessing
					//       the last element. By faking the memory offset we can bypass this is issue for now.
					//       Obviously, this needs fixing at some point.
					addr += comp.soa() * 4;
				}
				return addr;
			}

			template <typename T>
			GAIA_NODISCARD static constexpr ComponentCacheItem* create() {
				static_assert(core::is_raw_v<T>);

				auto* cci = new ComponentCacheItem();
				cci->comp = Component(
						// component id
						ComponentDesc<T>::id(),
						// soa
						ComponentDesc<T>::soa(cci->soaSizes),
						// size in bytes
						ComponentDesc<T>::size(),
						// alignment
						ComponentDesc<T>::alig());
				cci->hashLookup = ComponentDesc<T>::hash_lookup();
				cci->matcherHash = ComponentDesc<T>::hash_matcher();
				cci->name = ComponentDesc<T>::name();
				cci->func_ctor = ComponentDesc<T>::func_ctor();
				cci->func_move_ctor = ComponentDesc<T>::func_move_ctor();
				cci->func_copy_ctor = ComponentDesc<T>::func_copy_ctor();
				cci->func_dtor = ComponentDesc<T>::func_dtor();
				cci->func_copy = ComponentDesc<T>::func_copy();
				cci->func_move = ComponentDesc<T>::func_move();
				cci->func_swap = ComponentDesc<T>::func_swap();
				cci->func_cmp = ComponentDesc<T>::func_cmp();
				return cci;
			}
		};
	} // namespace ecs
} // namespace gaia
