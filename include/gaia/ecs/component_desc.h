#pragma once
#include "../config/config.h"

#include <cinttypes>
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

			//! Unique component identifier
			Component comp = ComponentBad;
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
				func_swap(pLeft, pRight);
			}

			GAIA_NODISCARD uint32_t calc_new_mem_offset(uint32_t addr, size_t N) const noexcept {
				if (comp.soa() == 0) {
					addr = (uint32_t)mem::detail::get_aligned_byte_offset(addr, comp.alig(), comp.size(), N);
				} else {
					GAIA_FOR(comp.soa()) {
						addr = (uint32_t)mem::detail::get_aligned_byte_offset(addr, comp.alig(), soaSizes[i], N);
					}
				}
				return addr;
			}

			template <typename T>
			GAIA_NODISCARD static constexpr ComponentDesc build() {
				using U = typename component_type_t<T>::Type;

				uint32_t id = comp_id<T>();
				uint32_t size = 0, alig = 0, soa = 0;

				ComponentDesc desc{};
				desc.hashLookup = {meta::type_info::hash<U>()};
				desc.matcherHash = calc_matcher_hash<U>();
				desc.name = meta::type_info::name<U>();

				if constexpr (!std::is_empty_v<U>) {
					size = (uint32_t)sizeof(U);

					static_assert(Component::MaxAlignment_Bits, "Maximum supported alignemnt for a component is MaxAlignment");
					alig = (uint32_t)mem::auto_view_policy<U>::Alignment;

					if constexpr (mem::is_soa_layout_v<U>) {
						uint32_t i = 0;
						using TTuple = decltype(meta::struct_to_tuple(U{}));
						core::each_tuple<TTuple>([&](auto&& item) {
							static_assert(sizeof(item) <= 255, "Each member of a SoA component can be at most 255 B long!");
							desc.soaSizes[i] = (uint8_t)sizeof(item);
							++i;
						});
						soa = i;
						GAIA_ASSERT(i <= meta::StructToTupleMaxTypes);
					} else {
						// Custom construction
						if constexpr (!std::is_trivially_constructible_v<U>) {
							desc.func_ctor = [](void* ptr, uint32_t cnt) {
								core::call_ctor_n((U*)ptr, cnt);
							};
						}

						// Custom destruction
						if constexpr (!std::is_trivially_destructible_v<U>) {
							desc.func_dtor = [](void* ptr, uint32_t cnt) {
								core::call_dtor_n((U*)ptr, cnt);
							};
						}

						// Copyability
						if (!std::is_trivially_copyable_v<U>) {
							if constexpr (std::is_copy_assignable_v<U>) {
								desc.func_copy = [](void* from, void* to) {
									auto* src = (U*)from;
									auto* dst = (U*)to;
									*dst = *src;
								};
								desc.func_ctor_copy = [](void* from, void* to) {
									auto* src = (U*)from;
									auto* dst = (U*)to;
									new (dst) U();
									*dst = *src;
								};
							} else if constexpr (std::is_copy_constructible_v<U>) {
								desc.func_copy = [](void* from, void* to) {
									auto* src = (U*)from;
									auto* dst = (U*)to;
									*dst = U(*src);
								};
								desc.func_ctor_copy = [](void* from, void* to) {
									auto* src = (U*)from;
									auto* dst = (U*)to;
									(void)new (dst) U(GAIA_MOV(*src));
								};
							}
						}

						// Movability
						if constexpr (!std::is_trivially_move_assignable_v<U> && std::is_move_assignable_v<U>) {
							desc.func_move = [](void* from, void* to) {
								auto* src = (U*)from;
								auto* dst = (U*)to;
								*dst = GAIA_MOV(*src);
							};
							desc.func_ctor_move = [](void* from, void* to) {
								auto* src = (U*)from;
								auto* dst = (U*)to;
								new (dst) U();
								*dst = GAIA_MOV(*src);
							};
						} else if constexpr (!std::is_trivially_move_constructible_v<U> && std::is_move_constructible_v<U>) {
							desc.func_move = [](void* from, void* to) {
								auto* src = (U*)from;
								auto* dst = (U*)to;
								*dst = U(GAIA_MOV(*src));
							};
							desc.func_ctor_move = [](void* from, void* to) {
								auto* src = (U*)from;
								auto* dst = (U*)to;
								(void)new (dst) U(GAIA_MOV(*src));
							};
						}
					}

					// Value swap
					if constexpr (std::is_move_constructible_v<U> && std::is_move_assignable_v<U>) {
						desc.func_swap = [](void* left, void* right) {
							auto* l = (U*)left;
							auto* r = (U*)right;
							U tmp = GAIA_MOV(*l);
							*r = GAIA_MOV(*l);
							*r = GAIA_MOV(tmp);
						};
					} else {
						desc.func_swap = [](void* left, void* right) {
							auto* l = (U*)left;
							auto* r = (U*)right;
							U tmp = *l;
							*r = *l;
							*r = tmp;
						};
					}

					desc.comp = Component(id, soa, size, alig);
				}

				desc.comp = Component(id, soa, size, alig);
				return desc;
			}

			template <typename T>
			GAIA_NODISCARD static ComponentDesc* create() {
				using U = std::decay_t<T>;
				return new ComponentDesc{ComponentDesc::build<U>()};
			}
		};
	} // namespace ecs
} // namespace gaia
