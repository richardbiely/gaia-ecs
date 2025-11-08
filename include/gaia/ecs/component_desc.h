#pragma once
#include "gaia/config/config.h"

#include <cstdint>
#include <cstring>
#include <tuple>
#include <type_traits>

#include "component.h"
#include "gaia/core/span.h"
#include "gaia/core/utility.h"
#include "gaia/mem/data_layout_policy.h"
#include "gaia/mem/mem_utils.h"
#include "gaia/meta/reflection.h"
#include "gaia/meta/type_info.h"
#include "gaia/ser/ser_rt.h"

namespace gaia {
	namespace ecs {
		namespace detail {
			using ComponentDescId = uint32_t;

			template <typename T>
			struct ComponentDesc final {
				using CT = component_type_t<T>;
				using U = typename component_type_t<T>::Type;
				using DescU = typename CT::TypeFull;

				static ComponentDescId id() {
					return meta::type_info::id<DescU>();
				}

				static constexpr ComponentLookupHash hash_lookup() {
					return {meta::type_info::hash<DescU>()};
				}

				static constexpr auto name() {
					return meta::type_info::name<DescU>();
				}

				static constexpr uint32_t size() {
					if constexpr (std::is_empty_v<U>)
						return 0;
					else
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
						using TTuple = decltype(meta::struct_to_tuple(std::declval<U>()));
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
					return [](void* GAIA_RESTRICT dst, const void* GAIA_RESTRICT src, uint32_t idxDst, uint32_t idxSrc,
										uint32_t sizeDst, uint32_t sizeSrc) {
						mem::copy_ctor_element<U>((uint8_t*)dst, (const uint8_t*)src, idxDst, idxSrc, sizeDst, sizeSrc);
					};
				}

				static constexpr auto func_move_ctor() {
					return [](void* GAIA_RESTRICT dst, void* GAIA_RESTRICT src, uint32_t idxDst, uint32_t idxSrc,
										uint32_t sizeDst, uint32_t sizeSrc) {
						mem::move_ctor_element<U>((uint8_t*)dst, (uint8_t*)src, idxDst, idxSrc, sizeDst, sizeSrc);
					};
				}

				static constexpr auto func_copy() {
					return [](void* GAIA_RESTRICT dst, const void* GAIA_RESTRICT src, uint32_t idxDst, uint32_t idxSrc,
										uint32_t sizeDst, uint32_t sizeSrc) {
						mem::copy_element<U>((uint8_t*)dst, (const uint8_t*)src, idxDst, idxSrc, sizeDst, sizeSrc);
					};
				}

				static constexpr auto func_move() {
					return [](void* GAIA_RESTRICT dst, void* GAIA_RESTRICT src, uint32_t idxDst, uint32_t idxSrc,
										uint32_t sizeDst, uint32_t sizeSrc) {
						mem::move_element<U>((uint8_t*)dst, (uint8_t*)src, idxDst, idxSrc, sizeDst, sizeSrc);
					};
				}

				static constexpr auto func_swap() {
					return [](void* GAIA_RESTRICT left, void* GAIA_RESTRICT right, uint32_t idxLeft, uint32_t idxRight,
										uint32_t sizeLeft, uint32_t sizeRight) {
						mem::swap_elements<U>((uint8_t*)left, (uint8_t*)right, idxLeft, idxRight, sizeLeft, sizeRight);
					};
				}

				static constexpr auto func_cmp() {
					if constexpr (mem::is_soa_layout_v<U>) {
						return []([[maybe_unused]] const void* left, [[maybe_unused]] const void* right) {
							GAIA_ASSERT(false && "func_cmp for SoA not implemented yet");
							return false;
						};
					} else {
						constexpr bool hasGlobalCmp = core::has_ffunc_equals<U>::value;
						constexpr bool hasMemberCmp = core::has_func_equals<U>::value;
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
								return memcmp(l, r, sizeof(U)) == 0;
							};
						}
					}
				}

				static constexpr auto func_save() {
					return [](ser::ISerializer* pSer, const void* pSrc, uint32_t from, uint32_t to, uint32_t cap) {
						const auto* pComponent = (const U*)pSrc;

#if GAIA_ASSERT_ENABLED
						// Check if save and load match. Testing with one item is enough.
						pSer->check(*pComponent);
#endif

						if constexpr (mem::is_soa_layout_v<U>) {
							auto view = mem::auto_view_policy_get<U>{std::span{(const uint8_t*)pSrc, cap}};
							GAIA_FOR2(from, to) {
								auto val = view[i];
								pSer->save(val);
							}
						} else {
							pComponent += from;
							GAIA_FOR2(from, to) {
								pSer->save(*pComponent);
								++pComponent;
							}
						}
					};
				}

				static constexpr auto func_load() {
					return [](ser::ISerializer* pSer, void* pDst, uint32_t from, uint32_t to, uint32_t cap) {
						if constexpr (mem::is_soa_layout_v<U>) {
							auto view = mem::auto_view_policy_set<U>{std::span{(uint8_t*)pDst, cap}};
							GAIA_FOR2(from, to) {
								U val;
								pSer->load(val);
								view[i] = val;
							}
						} else {
							auto* pComponent = (U*)pDst + from;
							GAIA_FOR2(from, to) {
								pSer->load(*pComponent);
								++pComponent;
							}
						}
					};
				}
			};
		} // namespace detail
	} // namespace ecs
} // namespace gaia
