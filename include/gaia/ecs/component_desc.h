#pragma once
#include "../config/config.h"

#include <cinttypes>
#include <cstring>
#include <tuple>
#include <type_traits>

#include "../core/span.h"
#include "../core/utility.h"
#include "../mem/data_layout_policy.h"
#include "../mem/mem_utils.h"
#include "../meta/reflection.h"
#include "../meta/type_info.h"
#include "component.h"

namespace gaia {
	namespace ecs {
		struct ComponentDesc final {
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
			FuncMove* func_ctor_move = nullptr;
			//! Function to call when the component needs to be copy constructed
			FuncCopy* func_ctor_copy = nullptr;
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
				if (func_ctor_move != nullptr)
					func_ctor_move(pSrc, pDst);
				else if (func_ctor_copy != nullptr)
					func_ctor_copy(pSrc, pDst);
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
			GAIA_NODISCARD static constexpr ComponentDesc build() {
				static_assert(core::is_raw_v<T>);

				uint32_t id = comp_id<T>();
				uint32_t size = 0, alig = 0, soa = 0;

				ComponentDesc desc{};
				desc.hashLookup = {meta::type_info::hash<T>()};
				desc.matcherHash = calc_matcher_hash<T>();
				desc.name = meta::type_info::name<T>();

				if constexpr (!std::is_empty_v<T>) {
					size = (uint32_t)sizeof(T);

					static_assert(Component::MaxAlignment_Bits, "Maximum supported alignemnt for a component is MaxAlignment");
					alig = (uint32_t)mem::auto_view_policy<T>::Alignment;

					if constexpr (mem::is_soa_layout_v<T>) {
						uint32_t i = 0;
						using TTuple = decltype(meta::struct_to_tuple(T{}));
						core::each_tuple<TTuple>([&](auto&& item) {
							static_assert(sizeof(item) <= 255, "Each member of a SoA component can be at most 255 B long!");
							desc.soaSizes[i] = (uint8_t)sizeof(item);
							++i;
						});
						soa = i;
						GAIA_ASSERT(i <= meta::StructToTupleMaxTypes);
					} else {
						// Custom construction
						if constexpr (!std::is_trivially_constructible_v<T>) {
							desc.func_ctor = [](void* ptr, uint32_t cnt) {
								core::call_ctor_n((T*)ptr, cnt);
							};
						}

						// Custom destruction
						if constexpr (!std::is_trivially_destructible_v<T>) {
							desc.func_dtor = [](void* ptr, uint32_t cnt) {
								core::call_dtor_n((T*)ptr, cnt);
							};
						}

						// Copyability
						if (!std::is_trivially_copyable_v<T>) {
							if constexpr (std::is_copy_assignable_v<T>) {
								desc.func_copy = [](void* from, void* to) {
									auto* src = (T*)from;
									auto* dst = (T*)to;
									*dst = *src;
								};
								desc.func_ctor_copy = [](void* from, void* to) {
									auto* src = (T*)from;
									auto* dst = (T*)to;
									new (dst) T();
									*dst = *src;
								};
							} else if constexpr (std::is_copy_constructible_v<T>) {
								desc.func_copy = [](void* from, void* to) {
									auto* src = (T*)from;
									auto* dst = (T*)to;
									*dst = T(*src);
								};
								desc.func_ctor_copy = [](void* from, void* to) {
									auto* src = (T*)from;
									auto* dst = (T*)to;
									(void)new (dst) T(GAIA_MOV(*src));
								};
							}
						}

						// Movability
						if constexpr (!std::is_trivially_move_assignable_v<T> && std::is_move_assignable_v<T>) {
							desc.func_move = [](void* from, void* to) {
								auto* src = (T*)from;
								auto* dst = (T*)to;
								*dst = GAIA_MOV(*src);
							};
							desc.func_ctor_move = [](void* from, void* to) {
								auto* src = (T*)from;
								auto* dst = (T*)to;
								new (dst) T();
								*dst = GAIA_MOV(*src);
							};
						} else if constexpr (!std::is_trivially_move_constructible_v<T> && std::is_move_constructible_v<T>) {
							desc.func_move = [](void* from, void* to) {
								auto* src = (T*)from;
								auto* dst = (T*)to;
								*dst = T(GAIA_MOV(*src));
							};
							desc.func_ctor_move = [](void* from, void* to) {
								auto* src = (T*)from;
								auto* dst = (T*)to;
								(void)new (dst) T(GAIA_MOV(*src));
							};
						}
					}

					// Value swap
					if constexpr (std::is_move_constructible_v<T> && std::is_move_assignable_v<T>) {
						desc.func_swap = [](void* left, void* right) {
							auto* l = (T*)left;
							auto* r = (T*)right;
							T tmp = GAIA_MOV(*l);
							*r = GAIA_MOV(*l);
							*r = GAIA_MOV(tmp);
						};
					} else {
						desc.func_swap = [](void* left, void* right) {
							auto* l = (T*)left;
							auto* r = (T*)right;
							T tmp = *l;
							*r = *l;
							*r = tmp;
						};
					}

					// Value comparison
					constexpr bool hasGlobalCmp = core::has_global_equals<T>::value;
					constexpr bool hasMemberCmp = core::has_member_equals<T>::value;
					if constexpr (hasGlobalCmp || hasMemberCmp) {
						desc.func_cmp = [](const void* left, const void* right) {
							const auto* l = (const T*)left;
							const auto* r = (const T*)right;
							return *l == *r;
						};
					} else {
						// fallback comparison function
						desc.func_cmp = [](const void* left, const void* right) {
							const auto* l = (const T*)left;
							const auto* r = (const T*)right;
							return memcmp(l, r, 1) == 0;
						};
					}

					desc.comp = Component(id, soa, size, alig);
				}

				desc.comp = Component(id, soa, size, alig);
				return desc;
			}

			template <typename T>
			GAIA_NODISCARD static ComponentDesc* create() {
				return new ComponentDesc{ComponentDesc::build<T>()};
			}
		};
	} // namespace ecs
} // namespace gaia
