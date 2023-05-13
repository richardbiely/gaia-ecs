#pragma once
#include "../config/config_core.h"

#include <cinttypes>
#include <type_traits>

#include "../utils/data_layout_policy.h"
#include "../utils/span.h"
#include "../utils/type_info.h"
#include "component.h"

namespace gaia {
	namespace ecs {
		namespace component {
			struct ComponentDesc final {
				using FuncCtor = void(void*, size_t);
				using FuncDtor = void(void*, size_t);
				using FuncCopy = void(void*, void*);
				using FuncMove = void(void*, void*);

				//! Component name
				std::span<const char> name;
				//! Function to call when the component needs to be constructed
				FuncCtor* ctor = nullptr;
				//! Function to call when the component needs to be move constructed
				FuncMove* ctor_move = nullptr;
				//! Function to call when the component needs to be copy constructed
				FuncCopy* ctor_copy = nullptr;
				//! Function to call when the component needs to be destroyed
				FuncDtor* dtor = nullptr;
				//! Function to call when the component needs to be copied
				FuncCopy* copy = nullptr;
				//! Fucntion to call when the component needs to be moved
				FuncMove* move = nullptr;
				//! Unique component identifier
				ComponentId componentId = ComponentIdBad;

				//! Various component properties
				struct {
					//! Component alignment
					uint32_t alig: MAX_COMPONENTS_SIZE_BITS;
					//! Component size
					uint32_t size: MAX_COMPONENTS_SIZE_BITS;
					//! Tells if the component is laid out in SoA style
					uint32_t soa : 1;
				} properties{};

				template <typename T>
				GAIA_NODISCARD static constexpr ComponentDesc Calculate() {
					using U = typename DeduceComponent<T>::Type;

					ComponentDesc info{};
					info.name = utils::type_info::name<U>();
					info.componentId = GetComponentId<T>();

					if constexpr (!std::is_empty_v<U>) {
						info.properties.alig = utils::auto_view_policy<U>::Alignment;
						info.properties.size = (uint32_t)sizeof(U);

						if constexpr (utils::is_soa_layout_v<U>) {
							info.properties.soa = true;
						} else {
							info.properties.soa = false;

							// Custom construction
							if constexpr (!std::is_trivially_constructible_v<U>) {
								info.ctor = [](void* ptr, size_t cnt) {
									auto* first = (U*)ptr;
									auto* last = (U*)ptr + cnt;
									for (; first != last; ++first)
										(void)new (first) U();
								};
							}

							// Custom destruction
							if constexpr (!std::is_trivially_destructible_v<U>) {
								info.dtor = [](void* ptr, size_t cnt) {
									auto first = (U*)ptr;
									auto last = (U*)ptr + cnt;
									for (; first != last; ++first)
										first->~U();
								};
							}

							// Copyability
							if (!std::is_trivially_copyable_v<U>) {
								if constexpr (std::is_copy_constructible_v<U>) {
									info.copy = [](void* from, void* to) {
										auto* src = (U*)from;
										auto* dst = (U*)to;
										*dst = U(*src);
									};
									info.ctor_copy = [](void* from, void* to) {
										auto* src = (U*)from;
										auto* dst = (U*)to;
										(void)new (dst) U(std::move(*src));
									};
								} else if constexpr (std::is_copy_assignable_v<U>) {
									info.copy = [](void* from, void* to) {
										auto* src = (U*)from;
										auto* dst = (U*)to;
										*dst = *src;
									};
									info.ctor_copy = [](void* from, void* to) {
										auto* src = (U*)from;
										auto* dst = (U*)to;
										new (dst) U();
										*dst = *src;
									};
								}
							}

							// Movability
							if constexpr (!std::is_trivially_move_constructible_v<U> && std::is_move_constructible_v<U>) {
								info.move = [](void* from, void* to) {
									auto* src = (U*)from;
									auto* dst = (U*)to;
									*dst = U(std::move(*src));
								};
								info.ctor_move = [](void* from, void* to) {
									auto* src = (U*)from;
									auto* dst = (U*)to;
									(void)new (dst) U(std::move(*src));
								};
							} else if constexpr (!std::is_trivially_move_assignable_v<U> && std::is_move_assignable_v<U>) {
								info.move = [](void* from, void* to) {
									auto* src = (U*)from;
									auto* dst = (U*)to;
									*dst = std::move(*src);
								};
								info.ctor_move = [](void* from, void* to) {
									auto* src = (U*)from;
									auto* dst = (U*)to;
									new (dst) U();
									*dst = std::move(*src);
								};
							}
						}
					}

					return info;
				}

				template <typename T>
				GAIA_NODISCARD static ComponentDesc Create() {
					using U = std::decay_t<T>;
					return ComponentDesc::Calculate<U>();
				}
			};
		} // namespace component
	} // namespace ecs
} // namespace gaia

REGISTER_HASH_TYPE(gaia::ecs::component::ComponentLookupHash)
REGISTER_HASH_TYPE(gaia::ecs::component::ComponentMatcherHash)
