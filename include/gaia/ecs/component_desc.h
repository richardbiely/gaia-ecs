#pragma once
#include "../config/config.h"

#include <__tuple_dir/tuple_size.h>
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

namespace gaia {
	namespace ecs {
		template <typename T>
		struct ComponentDesc final {
			static_assert(core::is_raw_v<T>);

			using FuncCtor = void(void*, uint32_t);
			using FuncDtor = void(void*, uint32_t);
			using FuncCopy = void(void*, void*);
			using FuncMove = void(void*, void*);
			using FuncSwap = void(void*, void*);
			using FuncCmp = bool(const void*, const void*);

			static uint32_t id() {
				return comp_id<T>();
			}

			static constexpr ComponentLookupHash hash_lookup() {
				return {meta::type_info::hash<T>()};
			}

			static constexpr ComponentMatcherHash hash_matcher() {
				return calc_matcher_hash<T>();
			}

			static constexpr auto name() {
				return meta::type_info::name<T>();
			}

			static constexpr uint32_t size() {
				return (uint32_t)sizeof(T);
			}

			static constexpr uint32_t alig() {
				if constexpr (!std::is_empty_v<T>) {
					constexpr auto alig = mem::auto_view_policy<T>::Alignment;
					static_assert(alig < Component::MaxAlignment, "Maximum supported alignemnt for a component is MaxAlignment");
					return alig;
				} else {
					return 0;
				}
			}

			static uint32_t soa(std::span<uint8_t, meta::StructToTupleMaxTypes> soaSizes) {
				if constexpr (mem::is_soa_layout_v<T>) {
					uint32_t i = 0;
					using TTuple = decltype(meta::struct_to_tuple(T{}));
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
				if constexpr (!mem::is_soa_layout_v<T> && !std::is_trivially_constructible_v<T>) {
					return [](void* ptr, uint32_t cnt) {
						core::call_ctor_n((T*)ptr, cnt);
					};
				} else {
					return nullptr;
				}
			}

			static constexpr auto func_dtor() {
				if constexpr (!mem::is_soa_layout_v<T> && !std::is_trivially_destructible_v<T>) {
					return [](void* ptr, uint32_t cnt) {
						core::call_dtor_n((T*)ptr, cnt);
					};
				} else {
					return nullptr;
				}
			}

			static constexpr auto func_copy_ctor() {
				if constexpr (mem::is_soa_layout_v<T>) {
					return nullptr;
				} else if constexpr (std::is_copy_assignable_v<T>) {
					return [](void* from, void* to) {
						auto* src = (T*)from;
						auto* dst = (T*)to;
						new (dst) T();
						*dst = *src;
					};
				} else if constexpr (std::is_copy_constructible_v<T>) {
					return [](void* from, void* to) {
						auto* src = (T*)from;
						auto* dst = (T*)to;
						(void)new (dst) T(GAIA_MOV(*src));
					};
				} else {
					return nullptr;
				}
			}

			static constexpr auto func_copy() {
				if constexpr (mem::is_soa_layout_v<T>) {
					return nullptr;
				} else if constexpr (std::is_copy_assignable_v<T>) {
					return [](void* from, void* to) {
						auto* src = (T*)from;
						auto* dst = (T*)to;
						*dst = *src;
					};
				} else if constexpr (std::is_copy_constructible_v<T>) {
					return [](void* from, void* to) {
						auto* src = (T*)from;
						auto* dst = (T*)to;
						*dst = T(*src);
					};
				} else {
					return nullptr;
				}
			}

			static constexpr auto func_move_ctor() {
				if constexpr (mem::is_soa_layout_v<T>) {
					return nullptr;
				} else if constexpr (!std::is_trivially_move_assignable_v<T> && std::is_move_assignable_v<T>) {
					return [](void* from, void* to) {
						auto* src = (T*)from;
						auto* dst = (T*)to;
						new (dst) T();
						*dst = GAIA_MOV(*src);
					};
				} else if constexpr (!std::is_trivially_move_constructible_v<T> && std::is_move_constructible_v<T>) {
					return [](void* from, void* to) {
						auto* src = (T*)from;
						auto* dst = (T*)to;
						(void)new (dst) T(GAIA_MOV(*src));
					};
				} else {
					return nullptr;
				}
			}

			static constexpr auto func_move() {
				if constexpr (mem::is_soa_layout_v<T>) {
					return nullptr;
				} else if constexpr (!std::is_trivially_move_assignable_v<T> && std::is_move_assignable_v<T>) {
					return [](void* from, void* to) {
						auto* src = (T*)from;
						auto* dst = (T*)to;
						*dst = GAIA_MOV(*src);
					};
				} else if constexpr (!std::is_trivially_move_constructible_v<T> && std::is_move_constructible_v<T>) {
					return [](void* from, void* to) {
						auto* src = (T*)from;
						auto* dst = (T*)to;
						*dst = T(GAIA_MOV(*src));
					};
				} else {
					return nullptr;
				}
			}

			static constexpr auto func_swap() {
				if constexpr (mem::is_soa_layout_v<T>) {
					return nullptr;
				} else if constexpr (std::is_move_constructible_v<T> && std::is_move_assignable_v<T>) {
					return [](void* left, void* right) {
						auto* l = (T*)left;
						auto* r = (T*)right;
						T tmp = GAIA_MOV(*l);
						*r = GAIA_MOV(*l);
						*r = GAIA_MOV(tmp);
					};
				} else {
					return [](void* left, void* right) {
						auto* l = (T*)left;
						auto* r = (T*)right;
						T tmp = *l;
						*r = *l;
						*r = tmp;
					};
				}
			}

			static constexpr auto func_cmp() {
				if constexpr (mem::is_soa_layout_v<T>) {
					return nullptr;
				} else {
					constexpr bool hasGlobalCmp = core::has_global_equals<T>::value;
					constexpr bool hasMemberCmp = core::has_member_equals<T>::value;
					if constexpr (hasGlobalCmp || hasMemberCmp) {
						return [](const void* left, const void* right) {
							const auto* l = (const T*)left;
							const auto* r = (const T*)right;
							return *l == *r;
						};
					} else {
						// fallback comparison function
						return [](const void* left, const void* right) {
							const auto* l = (const T*)left;
							const auto* r = (const T*)right;
							return memcmp(l, r, 1) == 0;
						};
					}
				}
			}
		};
	} // namespace ecs
} // namespace gaia
