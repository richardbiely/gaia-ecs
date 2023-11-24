#pragma once
#include "../config/config.h"

#include <cstdint>
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
#include "gaia/ecs/id.h"

namespace gaia {
	namespace ecs {
		namespace detail {
			using ComponentDescId = uint32_t;

			template <typename T>
			struct ComponentDesc final {
				using CT = component_type_t<T>;
				using U = typename component_type_t<T>::Type;
				using DescU = typename CT::TypeFull;

				using FuncCtor = void(void*, uint32_t);
				using FuncDtor = void(void*, uint32_t);
				using FuncCopy = void(void*, void*);
				using FuncMove = void(void*, void*);
				using FuncSwap = void(void*, void*);
				using FuncCmp = bool(const void*, const void*);

				static ComponentDescId id() {
					return meta::type_info::id<DescU>();
				}

				static constexpr ComponentLookupHash hash_lookup() {
					return {meta::type_info::hash<DescU>()};
				}

				static constexpr ComponentMatcherHash hash_matcher() {
					return {calc_matcher_hash<DescU>()};
				}

				static constexpr auto name() {
					return meta::type_info::name<DescU>();
				}

				static constexpr uint32_t size() {
					return (uint32_t)sizeof(U);
				}

				static constexpr uint32_t alig() {
					constexpr auto alig = mem::auto_view_policy<U>::Alignment;
					static_assert(alig < Component::MaxAlignment, "Maximum supported alignment for a component is MaxAlignment");
					return alig;
				}

				static uint32_t soa(std::span<uint8_t, meta::StructToTupleMaxTypes> soaSizes) {
					if constexpr (mem::is_soa_layout_v<U>) {
						uint32_t i = 0;
						using TTuple = decltype(meta::struct_to_tuple(U{}));
						// is_soa_layout_v is always false for empty types so we know there is at least one element in the tuple
						constexpr auto TTupleSize = std::tuple_size_v<TTuple>;
						static_assert(TTupleSize > 0);
						static_assert(TTupleSize <= meta::StructToTupleMaxTypes);
						core::each_tuple<TTuple>([&](auto&& item) {
							static_assert(sizeof(item) <= 255, "Each member of a SoA component can be at most 255 B long!");
							soaSizes[i] = (uint8_t)sizeof(item);
							++i;
						});
						GAIA_ASSERT(i <= meta::StructToTupleMaxTypes);
						return i;
					} else {
						return 0U;
					}
				}

				static constexpr auto func_ctor() {
					if constexpr (!mem::is_soa_layout_v<U> && !std::is_trivially_constructible_v<U>) {
						return [](void* ptr, uint32_t cnt) {
							core::call_ctor_n((U*)ptr, cnt);
						};
					} else {
						return nullptr;
					}
				}

				static constexpr auto func_dtor() {
					if constexpr (!mem::is_soa_layout_v<U> && !std::is_trivially_destructible_v<U>) {
						return [](void* ptr, uint32_t cnt) {
							core::call_dtor_n((U*)ptr, cnt);
						};
					} else {
						return nullptr;
					}
				}

				static constexpr auto func_copy_ctor() {
					if constexpr (mem::is_soa_layout_v<U>) {
						return nullptr;
					} else if constexpr (std::is_copy_assignable_v<U>) {
						return [](void* from, void* to) {
							auto* src = (U*)from;
							auto* dst = (U*)to;
							core::call_ctor(dst);
							*dst = *src;
						};
					} else if constexpr (std::is_copy_constructible_v<U>) {
						return [](void* from, void* to) {
							auto* src = (U*)from;
							auto* dst = (U*)to;
							core::call_ctor(dst, U(GAIA_MOV(*src)));
						};
					} else {
						return nullptr;
					}
				}

				static constexpr auto func_copy() {
					if constexpr (mem::is_soa_layout_v<U>) {
						return nullptr;
					} else if constexpr (std::is_copy_assignable_v<U>) {
						return [](void* from, void* to) {
							auto* src = (U*)from;
							auto* dst = (U*)to;
							*dst = *src;
						};
					} else if constexpr (std::is_copy_constructible_v<U>) {
						return [](void* from, void* to) {
							auto* src = (U*)from;
							auto* dst = (U*)to;
							*dst = U(*src);
						};
					} else {
						return nullptr;
					}
				}

				static constexpr auto func_move_ctor() {
					if constexpr (mem::is_soa_layout_v<U>) {
						return nullptr;
					} else if constexpr (!std::is_trivially_move_assignable_v<U> && std::is_move_assignable_v<U>) {
						return [](void* from, void* to) {
							auto* src = (U*)from;
							auto* dst = (U*)to;
							core::call_ctor(dst);
							*dst = GAIA_MOV(*src);
						};
					} else if constexpr (!std::is_trivially_move_constructible_v<U> && std::is_move_constructible_v<U>) {
						return [](void* from, void* to) {
							auto* src = (U*)from;
							auto* dst = (U*)to;
							core::call_ctor(dst, GAIA_MOV(*src));
						};
					} else {
						return nullptr;
					}
				}

				static constexpr auto func_move() {
					if constexpr (mem::is_soa_layout_v<U>) {
						return nullptr;
					} else if constexpr (!std::is_trivially_move_assignable_v<U> && std::is_move_assignable_v<U>) {
						return [](void* from, void* to) {
							auto* src = (U*)from;
							auto* dst = (U*)to;
							*dst = GAIA_MOV(*src);
						};
					} else if constexpr (!std::is_trivially_move_constructible_v<U> && std::is_move_constructible_v<U>) {
						return [](void* from, void* to) {
							auto* src = (U*)from;
							auto* dst = (U*)to;
							*dst = U(GAIA_MOV(*src));
						};
					} else {
						return nullptr;
					}
				}

				static constexpr auto func_swap() {
					if constexpr (mem::is_soa_layout_v<U>) {
						return nullptr;
					} else if constexpr (std::is_move_constructible_v<U> && std::is_move_assignable_v<U>) {
						return [](void* left, void* right) {
							auto* l = (U*)left;
							auto* r = (U*)right;
							U tmp = GAIA_MOV(*r);
							*r = GAIA_MOV(*l);
							*l = GAIA_MOV(tmp);
						};
					} else {
						return [](void* left, void* right) {
							auto* l = (U*)left;
							auto* r = (U*)right;
							U tmp = *r;
							*r = *l;
							*l = tmp;
						};
					}
				}

				static constexpr auto func_cmp() {
					if constexpr (mem::is_soa_layout_v<U>) {
						return nullptr;
					} else {
						constexpr bool hasGlobalCmp = core::has_global_equals<U>::value;
						constexpr bool hasMemberCmp = core::has_member_equals<U>::value;
						if constexpr (hasGlobalCmp || hasMemberCmp) {
							return [](const void* left, const void* right) {
								const auto* l = (const U*)left;
								const auto* r = (const U*)right;
								return *l == *r;
							};
						} else {
							// fallback comparison function
							return [](const void* left, const void* right) {
								const auto* l = (const U*)left;
								const auto* r = (const U*)right;
								return memcmp(l, r, 1) == 0;
							};
						}
					}
				}
			};
		} // namespace detail
	} // namespace ecs
} // namespace gaia
