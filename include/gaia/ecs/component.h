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

#pragma region Helpers
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

#pragma endregion

		struct ComponentMetaData;

#pragma region Component hash operations

		namespace detail {
			template <typename T>
			constexpr uint64_t CalculateMatcherHash() noexcept {
				return (uint64_t(1) << (utils::type_info::hash<T>() % uint64_t(63)));
			}
		} // namespace detail

		template <typename = void, typename...>
		constexpr uint64_t CalculateMatcherHash() noexcept;

		template <typename T, typename... Rest>
		[[nodiscard]] constexpr uint64_t CalculateMatcherHash() noexcept {
			if constexpr (sizeof...(Rest) == 0)
				return detail::CalculateMatcherHash<T>();
			else
				return utils::combine_or(
						detail::CalculateMatcherHash<T>(),
						detail::CalculateMatcherHash<Rest>()...);
		};

		template <>
		[[nodiscard]] constexpr uint64_t CalculateMatcherHash() noexcept {
			return 0;
		};

		//-----------------------------------------------------------------------------------

		template <typename Container>
		[[nodiscard]] constexpr uint64_t
		CalculateLookupHash(Container arr) noexcept {
			constexpr auto N = arr.size();
				if constexpr (N == 0) {
					return 0;
				} else {
					uint64_t hash = arr[0];
					utils::for_each<N - 1>([&hash, &arr](auto i) {
						hash = utils::hash_combine(hash, arr[i + 1]);
					});
					return hash;
				}
		}

		template <typename = void, typename...>
		constexpr uint64_t CalculateLookupHash() noexcept;

		template <typename T, typename... Rest>
		[[nodiscard]] constexpr uint64_t CalculateLookupHash() noexcept {
			if constexpr (sizeof...(Rest) == 0)
				return utils::type_info::hash<T>();
			else
				return utils::hash_combine(
						utils::type_info::hash<T>(), utils::type_info::hash<Rest>()...);
		};

		template <>
		[[nodiscard]] constexpr uint64_t CalculateLookupHash() noexcept {
			return 0;
		};

#pragma endregion

#pragma region ComponentMetaData

		struct ComponentMetaData final {
			std::string_view name;
			uint64_t lookupHash;
			uint64_t matcherHash;
			uint32_t typeIndex;
			uint32_t alig;
			uint32_t size;

			[[nodiscard]] bool operator==(const ComponentMetaData& other) const {
				return lookupHash == other.lookupHash && typeIndex == other.typeIndex;
			}
			[[nodiscard]] bool operator!=(const ComponentMetaData& other) const {
				return lookupHash != other.lookupHash || typeIndex != other.typeIndex;
			}
			[[nodiscard]] bool operator<(const ComponentMetaData& other) const {
				return lookupHash < other.lookupHash;
			}

			template <typename T>
			[[nodiscard]] static constexpr ComponentMetaData Calculate() {
				using TComponent = std::decay_t<T>;

				ComponentMetaData mth;
				mth.name = utils::type_info::name<TComponent>();
				mth.lookupHash = utils::type_info::hash<TComponent>();
				mth.matcherHash = CalculateMatcherHash<TComponent>();
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

		[[nodiscard]] inline uint64_t
		CalculateMatcherHash(std::span<const ComponentMetaData*> types) noexcept {
			uint64_t hash = types.empty() ? 0 : types[0]->matcherHash;
			for (uint32_t i = 1; i < types.size(); ++i)
				hash = utils::combine_or(hash, types[i]->matcherHash);
			return hash;
		}

		[[nodiscard]] inline uint64_t
		CalculateLookupHash(std::span<const ComponentMetaData*> types) noexcept {
			uint64_t hash = types.empty() ? 0 : types[0]->lookupHash;
			for (uint32_t i = 1; i < types.size(); ++i)
				hash = utils::hash_combine(hash, types[i]->lookupHash);
			return hash;
		}

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
			GAIA_ASSERT(idx < g_ComponentMetaTypeCache.size());
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
			GAIA_ASSERT(it != g_ComponentMetaTypeCache.end());
			return *it;
		}

		template <typename T>
		[[nodiscard]] inline bool HasComponentMetaType() {
			using TComponent = std::decay_t<T>;
			const auto idx = utils::type_info::index<TComponent>();
			return idx < g_ComponentMetaTypeCache.size();
		}

	} // namespace ecs
} // namespace gaia
