#pragma once
#include <type_traits>
#include <vector>

#include "../external/stack_allocator.h"
#include "component.h"
#include "fwd.h"

namespace gaia {
	namespace ecs {
		class CreationQuery final {
		private:
			//! Number of components that can be a part of CreationQuery
			static constexpr uint32_t MAX_COMPONENTS_IN_QUERY = 8u;
			friend class World;

			// We don't want to allocate these things on heap. Instead, we take a
			// small amount of space on stack for each query object
			using ComponentMetaDataArrayAllocator =
					stack_allocator<const ComponentMetaData*, MAX_COMPONENTS_IN_QUERY>;
			using ComponentMetaDataArray = std::vector<
					const ComponentMetaData*, ComponentMetaDataArrayAllocator>;

			ComponentMetaDataArray list[ComponentType::CT_Count];

			template <typename T>
			void AddToList(const ComponentType t) {
				using TComponent = std::decay_t<T>;
				list[(uint32_t)t].push_back(
						g_ComponentCache.GetOrCreateComponentMetaType<TComponent>());
			}

		public:
			CreationQuery() = default;

			template <typename... TComponent>
			CreationQuery& AddComponent() {
				static_assert(
						VerityArchetypeComponentCount(sizeof...(TComponent)),
						"Maximum number of components exceeded");
				VerifyComponents<TComponent...>();
				(AddToList<TComponent>(ComponentType::CT_Generic), ...);
				return *this;
			}

			template <typename... TComponent>
			CreationQuery& AddChunkComponent() {
				static_assert(
						VerityArchetypeComponentCount(sizeof...(TComponent)),
						"Maximum number of components exceeded");
				VerifyComponents<TComponent...>();
				(AddToList<TComponent>(ComponentType::CT_Chunk), ...);
				return *this;
			}
		};
	} // namespace ecs
} // namespace gaia