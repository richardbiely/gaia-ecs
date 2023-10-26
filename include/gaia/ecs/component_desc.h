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
			static constexpr uint32_t MaxAlignment_Bits = 10;
			static constexpr uint32_t MaxAlignment = (1U << MaxAlignment_Bits) - 1;

			using FuncCtor = void(void*, uint32_t);
			using FuncDtor = void(void*, uint32_t);
			using FuncCopy = void(void*, void*);
			using FuncMove = void(void*, void*);
			using FuncSwap = void(void*, void*);

			//! Unique component identifier
			ComponentId compId = ComponentIdBad;

			//! Various component properties
			struct {
				//! Component alignment
				uint32_t alig: MaxAlignment_Bits;
				//! Component size
				uint32_t size: MAX_COMPONENTS_SIZE_BITS;
				//! SOA variables. If > 0 the component is laid out in SoA style
				uint32_t soa: meta::StructToTupleMaxTypes_Bits;
			} properties{};

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
					memmove(pDst, (const void*)pSrc, properties.size);
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
					memmove(pDst, (const void*)pSrc, properties.size);
			}

			void dtor(void* pSrc) const {
				if (func_dtor != nullptr)
					func_dtor(pSrc, 1);
			}

			void swap(void* pLeft, void* pRight) const {
				func_swap(pLeft, pRight);
			}

			GAIA_NODISCARD uint32_t calc_new_mem_offset(uint32_t addr, size_t N) const noexcept {
				if (properties.soa == 0) {
					addr = (uint32_t)mem::detail::get_aligned_byte_offset(addr, properties.alig, properties.size, N);
				} else {
					for (uint32_t i = 0; i < (uint32_t)properties.soa; ++i)
						addr = (uint32_t)mem::detail::get_aligned_byte_offset(addr, properties.alig, soaSizes[i], N);
				}
				return addr;
			}

			template <typename T>
			GAIA_NODISCARD static constexpr ComponentDesc build() {
				using U = typename component_kind_t<T>::Kind;

				ComponentDesc info{};
				info.compId = comp_id<T>();
				info.name = meta::type_info::name<U>();

				if constexpr (!std::is_empty_v<U>) {
					info.properties.size = (uint32_t)sizeof(U);

					static_assert(MaxAlignment_Bits, "Maximum supported alignemnt for a component is MaxAlignment");
					info.properties.alig = (uint32_t)mem::auto_view_policy<U>::Alignment;

					if constexpr (mem::is_soa_layout_v<U>) {
						uint32_t i = 0;
						using TTuple = decltype(meta::struct_to_tuple(U{}));
						core::each_tuple(TTuple{}, [&](auto&& item) {
							static_assert(sizeof(item) <= 255, "Each member of a SoA component can be at most 255 B long!");
							info.soaSizes[i] = (uint8_t)sizeof(item);
							++i;
						});
						info.properties.soa = i;
						GAIA_ASSERT(i <= meta::StructToTupleMaxTypes);
					} else {
						info.properties.soa = 0U;

						// Custom construction
						if constexpr (!std::is_trivially_constructible_v<U>) {
							info.func_ctor = [](void* ptr, uint32_t cnt) {
								core::call_ctor((U*)ptr, cnt);
							};
						}

						// Custom destruction
						if constexpr (!std::is_trivially_destructible_v<U>) {
							info.func_dtor = [](void* ptr, uint32_t cnt) {
								core::call_dtor((U*)ptr, cnt);
							};
						}

						// Copyability
						if (!std::is_trivially_copyable_v<U>) {
							if constexpr (std::is_copy_assignable_v<U>) {
								info.func_copy = [](void* from, void* to) {
									auto* src = (U*)from;
									auto* dst = (U*)to;
									*dst = *src;
								};
								info.func_ctor_copy = [](void* from, void* to) {
									auto* src = (U*)from;
									auto* dst = (U*)to;
									new (dst) U();
									*dst = *src;
								};
							} else if constexpr (std::is_copy_constructible_v<U>) {
								info.func_copy = [](void* from, void* to) {
									auto* src = (U*)from;
									auto* dst = (U*)to;
									*dst = U(*src);
								};
								info.func_ctor_copy = [](void* from, void* to) {
									auto* src = (U*)from;
									auto* dst = (U*)to;
									(void)new (dst) U(std::move(*src));
								};
							}
						}

						// Movability
						if constexpr (!std::is_trivially_move_assignable_v<U> && std::is_move_assignable_v<U>) {
							info.func_move = [](void* from, void* to) {
								auto* src = (U*)from;
								auto* dst = (U*)to;
								*dst = std::move(*src);
							};
							info.func_ctor_move = [](void* from, void* to) {
								auto* src = (U*)from;
								auto* dst = (U*)to;
								new (dst) U();
								*dst = std::move(*src);
							};
						} else if constexpr (!std::is_trivially_move_constructible_v<U> && std::is_move_constructible_v<U>) {
							info.func_move = [](void* from, void* to) {
								auto* src = (U*)from;
								auto* dst = (U*)to;
								*dst = U(std::move(*src));
							};
							info.func_ctor_move = [](void* from, void* to) {
								auto* src = (U*)from;
								auto* dst = (U*)to;
								(void)new (dst) U(std::move(*src));
							};
						}
					}

					// Value swap
					if constexpr (std::is_move_constructible_v<U> && std::is_move_assignable_v<U>) {
						info.func_swap = [](void* left, void* right) {
							auto* l = (U*)left;
							auto* r = (U*)right;
							U tmp = std::move(*l);
							*r = std::move(*l);
							*r = std::move(tmp);
						};
					} else {
						info.func_swap = [](void* left, void* right) {
							auto* l = (U*)left;
							auto* r = (U*)right;
							U tmp = *l;
							*r = *l;
							*r = tmp;
						};
					}
				}

				return info;
			}

			template <typename T>
			GAIA_NODISCARD static ComponentDesc create() {
				using U = std::decay_t<T>;
				return ComponentDesc::build<U>();
			}
		};
	} // namespace ecs
} // namespace gaia
