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
#include "../ser/serialization.h"
#include "component.h"

namespace gaia {
	namespace ecs {
		class SerializationBufferDyn;

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
					return [](void* pSerializer, const void* pSrc, uint32_t cnt) {
						auto* pSer = (SerializationBufferDyn*)pSerializer;
						const auto* pComponent = (const U*)pSrc;
						GAIA_FOR(cnt) {
							// TODO: Add support for SoA types. They are not stored in the chunk contiguously.
							//       Therefore, we first need to load them into AoS form and then store them.
							ser::save(*pSer, *pComponent);
							++pComponent;
						}
					};
				}

				static constexpr auto func_load() {
					return [](void* pSerializer, void* pDst, uint32_t cnt) {
						auto* pSer = (SerializationBufferDyn*)pSerializer;
						auto* pComponent = (U*)pDst;
						GAIA_FOR(cnt) {
							// TODO: Add support for SoA types. They are not stored in the chunk contiguously.
							//       Therefore, after we read them form the buffer in their AoS form, we need to store them SoA style.
							ser::load(*pSer, *pComponent);
							++pComponent;
						}
					};
				}
			};
		} // namespace detail
	} // namespace ecs
} // namespace gaia
