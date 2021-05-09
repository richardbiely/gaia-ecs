#pragma once
#include <any>
#include <inttypes.h>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "../config/config.h"
#include "../external/span.hpp"
#include "../external/stack_allocator.h"
#include "../utils/hashing_policy.h"
#include "../utils/type_info.h"
#include "../utils/utility.h"
#include "common.h"
#include "fwd.h"

namespace gaia {
  namespace ecs {
    //! Maximum size of components in bytes
    static constexpr uint32_t MAX_COMPONENTS_SIZE = 255u;

    template <typename T> constexpr void VerifyComponent() {
      using TT = std::decay_t<T>;
      static_assert(
          (uint32_t)sizeof(TT) < MAX_COMPONENTS_SIZE,
          "Components can be max MAX_COMPONENTS_SIZE bytes long");
      static_assert(
          std::is_trivial<TT>::value,
          "Only trivial types are allowed to be used as components!");
    }

    template <typename... T> constexpr void VerifyComponents() {
      static_assert(utils::is_unique<T...>);
      (VerifyComponent<T>(), ...);
    }

#if 1 // GAIA_COMPILER_MSVC && _MSC_VER < 1920 // Turned off for VS2017 because
      // "fatal error C1001: An internal error has occurred in the compiler.".
      // Good job, Microsoft :)
  #define ECS_SMART_ARG(T) T&&
#else
    template <typename T>
    using SmartArg = std::conditional_t<sizeof(T) <= size_t(8), T, T&&>;
  #define ECS_SMART_ARG(T) SmartArg<T>
#endif

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
      uint64_t nameHash;
      uint64_t componentHash;
      uint32_t typeIndex;
      uint32_t alig;
      uint32_t size;

      bool operator==(const ComponentMetaData& other) const {
        return nameHash == other.nameHash && typeIndex == other.typeIndex;
      }
      bool operator!=(const ComponentMetaData& other) const {
        return nameHash != other.nameHash || typeIndex != other.typeIndex;
      }
      bool operator<(const ComponentMetaData& other) const {
        return nameHash < other.nameHash;
      }

      template <typename T> static constexpr ComponentMetaData Calculate() {
        using TComponent = std::decay_t<T>;

        ComponentMetaData mth;
#if GAIA_DEBUG
        mth.name = utils::type_info::name<TComponent>();
#endif
        mth.nameHash = utils::type_info::hash<TComponent>();
        // Map the name hash into 64 bits
        // TODO: Something better should be used probably so we don't have to
        // check too much archetype headers when matching then in queries.
        //       However, even using this the number of collisions in general
        //       use-case is very low (at least from what can be seen from
        //       diags).
        mth.componentHash |= (0x1ULL << (mth.nameHash % 63ULL));
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

      template <typename T> static const ComponentMetaData* Create() {
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
        for (auto* p : g_ComponentMetaTypeCache) {
          if (p)
            delete p;
        }
      g_ComponentMetaTypeCache.clear();
    }

    template <typename T>
    inline const ComponentMetaData* GetOrCreateComponentMetaType() {
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
    inline const ComponentMetaData* GetComponentMetaType() {
      using TComponent = std::decay_t<T>;
      const auto idx = utils::type_info::index<TComponent>();
      // Let's assume somebody has registered the component already via
      // AddComponent!
      assert(idx < g_ComponentMetaTypeCache.size());
      return g_ComponentMetaTypeCache[idx];
    }

    inline const ComponentMetaData* GetComponentMetaTypeFromIdx(uint32_t idx) {
      const auto it =
          utils::find_if(g_ComponentMetaTypeCache, [idx](const auto& info) {
            return info->typeIndex == idx;
          });
      // Let's assume somebody has registered the component already via
      // AddComponent!
      assert(it != g_ComponentMetaTypeCache.end());
      return *it;
    }

    template <typename T> inline bool HasComponentMetaType() {
      using TComponent = std::decay_t<T>;
      const auto idx = utils::type_info::index<TComponent>();
      return idx < g_ComponentMetaTypeCache.size();
    }

    inline uint64_t
    CalculateComponentsHash(tcb::span<const ComponentMetaData*> types) {
      uint64_t hash = 0;
      for (const auto type : types)
        hash |= type->componentHash;
      return hash;
    }
  } // namespace ecs
} // namespace gaia
