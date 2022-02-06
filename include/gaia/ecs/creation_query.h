#pragma once
#include <type_traits>

#include "../containers/sarray_ext.h"
#include "common.h"
#include "component.h"
#include "fwd.h"

namespace gaia {
	namespace ecs {
		class CreationQuery final {
		private:
			friend class World;

			using ComponentMetaDataArray = containers::sarray_ext<const ComponentMetaData*, MAX_COMPONENTS_PER_ARCHETYPE>;

			ComponentMetaDataArray list[ComponentType::CT_Count];

			template <typename T>
			void AddToList(const ComponentType componentType) {
				using TComponent = std::decay_t<T>;
				list[componentType].push_back(g_ComponentCache.GetOrCreateComponentMetaType<TComponent>());
			}

		public:
			CreationQuery() = default;

			template <typename... TComponent>
			CreationQuery& AddComponent() {
				static_assert(VerityArchetypeComponentCount(sizeof...(TComponent)), "Maximum number of components exceeded");
				VerifyComponents<TComponent...>();
				(AddToList<TComponent>(ComponentType::CT_Generic), ...);
				return *this;
			}

			template <typename... TComponent>
			CreationQuery& AddChunkComponent() {
				static_assert(VerityArchetypeComponentCount(sizeof...(TComponent)), "Maximum number of components exceeded");
				VerifyComponents<TComponent...>();
				(AddToList<TComponent>(ComponentType::CT_Chunk), ...);
				return *this;
			}
		};
	} // namespace ecs
} // namespace gaia