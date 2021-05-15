#pragma once
#include <inttypes.h>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "../config/config.h"
#include "../external/stack_allocator.h"
#include "../utils/hashing_policy.h"
#include "../utils/span.h"
#include "../utils/type_info.h"
#include "../utils/utility.h"
#include "common.h"
#include "fwd.h"

namespace gaia {
	namespace ecs {
		//! Maximum size of components in bytes
		static constexpr uint32_t MAX_COMPONENTS_SIZE = 255u;

		template <typename T>
		struct ComponentSizeValid:
				std::bool_constant<sizeof(T) < MAX_COMPONENTS_SIZE> {};

		template <typename T>
		struct ComponentTypeValid: std::bool_constant<std::is_trivial<T>::value> {};

		template <typename... T>
		constexpr void VerifyComponents() {
			static_assert(
					utils::is_unique<std::decay_t<T>...>,
					"Only unique inputs are enabled");
			static_assert(
					std::conjunction_v<ComponentSizeValid<std::decay_t<T>>...>,
					"Only component with size of at most MAX_COMPONENTS_SIZE are "
					"allowed");
			static_assert(
					std::conjunction_v<ComponentTypeValid<std::decay_t<T>>...>,
					"Only components of trivial type are allowed");
		}

#pragma endregion

		enum ComponentType : uint8_t {
			// General purpose component
			CT_Generic = 0,
			// Chunk component
			CT_Chunk,
			// Number of component types
			CT_Count
		};

		inline const char* ComponentTypeString[ComponentType::CT_Count] = {
				"Generic", "Chunk"};

#pragma region ComponentMetaData

		struct ComponentMetaData final {
#if GAIA_DEBUG
			std::string_view name;
#endif
			uint64_t componentHash;
			uint32_t typeIndex;
			uint32_t alig;
			uint32_t size;

			[[nodiscard]] bool operator==(const ComponentMetaData& other) const {
				return componentHash == other.componentHash && typeIndex == other.typeIndex;
			}
			[[nodiscard]] bool operator!=(const ComponentMetaData& other) const {
				return componentHash != other.componentHash || typeIndex != other.typeIndex;
			}
			[[nodiscard]] bool operator<(const ComponentMetaData& other) const {
				return componentHash < other.componentHash;
			}

			template <typename T>
			[[nodiscard]] static constexpr ComponentMetaData Calculate() {
				using TComponent = std::decay_t<T>;

				ComponentMetaData mth;
#if GAIA_DEBUG
				mth.name = utils::type_info::name<TComponent>();
#endif
				mth.componentHash = utils::type_info::hash<TComponent>();
				mth.typeIndex = utils::type_info::index<TComponent>();

					if constexpr (!std::is_empty<TComponent>::value) {
						mth.alig = (uint32_t)alignof(TComponent);
						mth.size = (uint32_t)sizeof(TComponent);
					} else {
						mth.alig = 0;
						mth.size = 0;
					}

				return mth;
			};

			template <typename T>
			static const ComponentMetaData* Create() {
				using TComponent = std::decay_t<T>;
				return new ComponentMetaData(Calculate<TComponent>());
			};
		};

#pragma endregion

		struct ChunkComponentInfo final {
			//! Pointer to the associated meta type
			const ComponentMetaData* type;
			//! Distance in bytes from the archetype's chunk data segment
			uint32_t offset;
		};

		using ChunkComponentListAllocator =
				stack_allocator<ChunkComponentInfo, MAX_COMPONENTS_PER_ARCHETYPE>;
		using ChunkComponentList =
				std::vector<ChunkComponentInfo, ChunkComponentListAllocator>;

		//-----------------------------------------------------------------------------------

#define GAIA_ECS_META_TYPES_H_INIT                                             \
	std::vector<const gaia::ecs::ComponentMetaData*>                             \
			gaia::ecs::g_ComponentMetaTypeCache;

		extern std::vector<const ComponentMetaData*> g_ComponentMetaTypeCache;

		//-----------------------------------------------------------------------------------

		void ClearRegisteredTypeCache() {
				for (auto* p: g_ComponentMetaTypeCache) {
					if (p)
						delete p;
				}
			g_ComponentMetaTypeCache.clear();
		}

		template <typename T>
		[[nodiscard]] inline const ComponentMetaData*
		GetOrCreateComponentMetaType() {
			using TComponent = std::decay_t<T>;
			const auto idx = utils::type_info::index<TComponent>();
			if (idx < g_ComponentMetaTypeCache.size() &&
					g_ComponentMetaTypeCache[idx] != nullptr)
				return g_ComponentMetaTypeCache[idx];

			const auto pMetaData = ComponentMetaData::Create<TComponent>();

			// Make sure there's enough space. Initialize new memory to zero
			const auto startIdx = g_ComponentMetaTypeCache.size();
			if (startIdx < idx + 1)
				g_ComponentMetaTypeCache.resize(idx + 1);
			const auto endIdx = g_ComponentMetaTypeCache.size() - 1;
			for (auto i = startIdx; i < endIdx; i++)
				g_ComponentMetaTypeCache[i] = nullptr;

			g_ComponentMetaTypeCache[idx] = pMetaData;
			return pMetaData;
		}

		template <typename T>
		[[nodiscard]] inline const ComponentMetaData* GetComponentMetaType() {
			using TComponent = std::decay_t<T>;
			const auto idx = utils::type_info::index<TComponent>();
			// Let's assume somebody has registered the component already via
			// AddComponent!
			assert(idx < g_ComponentMetaTypeCache.size());
			return g_ComponentMetaTypeCache[idx];
		}

		[[nodiscard]] inline const ComponentMetaData*
		GetComponentMetaTypeFromIdx(uint32_t idx) {
			const auto it =
					utils::find_if(g_ComponentMetaTypeCache, [idx](const auto& info) {
						return info->typeIndex == idx;
					});
			// Let's assume somebody has registered the component already via
			// AddComponent!
			assert(it != g_ComponentMetaTypeCache.end());
			return *it;
		}

		template <typename T>
		[[nodiscard]] inline bool HasComponentMetaType() {
			using TComponent = std::decay_t<T>;
			const auto idx = utils::type_info::index<TComponent>();
			return idx < g_ComponentMetaTypeCache.size();
		}

		[[nodiscard]] inline uint64_t
		CombineComponentHashes(std::span<const ComponentMetaData*> types) {
			uint64_t hash = 0;
			for (const auto type: types)
				// Multiplied by some large prime. Test the results later.
				// So long there are no collisions, this one is okay.
				hash |= type->componentHash * 2,147,483,647ULL;
			return hash;
		}

		template <class Tuple>
		[[nodiscard]] constexpr uint64_t
		CalculateComponentsHash2(Tuple const& tuple) noexcept {
			auto combine_hashes = [](auto const&... e) {
				if constexpr (sizeof...(e) == 0)
					return 0;
				else
					return (utils::type_info::hash<decltype(e)>() | ...);
			};
			return std::apply(combine_hashes, tuple);
		};

		template <typename... T>
		[[nodiscard]] constexpr uint64_t CalculateComponentsHash3() noexcept {
			if constexpr (sizeof...(T) == 0)
				return 0;
			else
				return (utils::type_info::hash<T>() | ...);
		};
	} // namespace ecs
} // namespace gaia
