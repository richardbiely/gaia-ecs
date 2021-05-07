#pragma once
#include <cassert>
#include <inttypes.h>
#include <unordered_map>
#include <vector>

#include "../config/config.h"
#include "../external/span.hpp"
#include "entity.h"
#include "entity_query.h"
#include "fwd.h"

namespace gaia {
namespace ecs {
//-----------------------------------------------------------------------------------

#define GAIA_ECS_WORLD_H_INIT int gaia::ecs::World::WorldCount = 0;

//-----------------------------------------------------------------------------------

namespace {
template <typename... Type> struct FuncTypeList {};
template <typename Class, typename Ret, typename... Args>
FuncTypeList<Args...> FuncArgs(Ret (Class::*)(Args...) const);
} // namespace

class GAIA_API World final {
  friend class ECSSystem;
  friend class ECSSystemManager;
  friend class EntityCommandBuffer;

  static int WorldCount;

  //! Map or archetypes mapping to the same hash - used for lookups
  std::unordered_map<uint64_t, std::vector<Archetype *>> m_archetypeMap;
  //! List of archetypes - used for iteration
  std::vector<Archetype *> m_archetypeList;
  //! List of unique archetype hashes - used for iteration
  std::vector<uint64_t> m_archetypeHashList;

  //! List of entities. Used for look-ups only when searching for entities in
  //! chunks + data validation
  std::vector<EntityContainer> m_entities;
  //! Identified of the next entity to recycle
  uint32_t m_nextFreeEntity = 0;
  //! Number of entites to recycle
  uint32_t m_freeEntities = 0;

  //! List of chunks to delete
  std::vector<Chunk *> m_chunksToRemove;

  //! With every structural change world version changes
  uint32_t m_worldVersion = 0;

  void UpdateWorldVersion() { UpdateVersion(m_worldVersion); }

  [[nodiscard]] Archetype *
  FindArchetype(tcb::span<const ComponentMetaData *> genericTypes,
                tcb::span<const ComponentMetaData *> chunkTypes,
                const uint64_t componentsHash) {
    // Search for the archetype in the map
    const auto it = m_archetypeMap.find(componentsHash);
    if (it == m_archetypeMap.end())
      return nullptr;

    const auto &archetypeArray = it->second;

    auto checkTypes = [&](const ChunkComponentList &list,
                          const tcb::span<const ComponentMetaData *> &types) {
      for (uint32_t j = 0; j < types.size(); j++) {
        // Different components. We need to search further
        if (list[j].type != types[j])
          return false;
      }
      return true;
    };

    // Iterate over the list of archetypes and find the exact match
    for (const auto archetype : archetypeArray) {
      const auto &genericComponentList =
          archetype->componentList[ComponentType::CT_Generic];
      if (genericComponentList.size() != genericTypes.size())
        continue;
      const auto &chunkComponentList =
          archetype->componentList[ComponentType::CT_Chunk];
      if (chunkComponentList.size() != chunkTypes.size())
        continue;

      if (checkTypes(genericComponentList, genericTypes) &&
          checkTypes(chunkComponentList, chunkTypes))
        return archetype;
    }

    return nullptr;
  }

  Archetype *CreateArchetype(tcb::span<const ComponentMetaData *> genericTypes,
                             tcb::span<const ComponentMetaData *> chunkTypes,
                             const uint64_t componentsHash) {
    auto newArch = Archetype::Create(*this, genericTypes, chunkTypes);
    // TODO: Probably move this to Archetype::Create so we don't forget to set
    // the hashes if we ever decide to call it
    newArch->componentsHash = componentsHash;

    m_archetypeList.push_back(newArch);
    m_archetypeHashList.push_back(componentsHash);

    auto it = m_archetypeMap.find(componentsHash);
    if (it == m_archetypeMap.end()) {
      m_archetypeMap.insert({componentsHash, {newArch}});
    } else {
      auto &archetypes = it->second;
      if (!utils::has(archetypes, newArch))
        archetypes.push_back(newArch);
    }

    return newArch;
  }

  Archetype *
  FindOrCreateArchetype(tcb::span<const ComponentMetaData *> genericTypes,
                        tcb::span<const ComponentMetaData *> chunkTypes) {
    // Make sure to sort the meta-types so we receive the same hash no matter
    // the order in which components are provided
    // Bubble sort is okay. We're dealing with at most
    // MAX_COMPONENTS_PER_ARCHETYPE items.
    // TODO: Replace with a sorting network
    std::sort(genericTypes.begin(), genericTypes.end(),
              std::less<const ComponentMetaData *>());
    std::sort(chunkTypes.begin(), chunkTypes.end());

    // Calculate hash for our combination of components
    const auto componentsHash = CalculateComponentsHash(genericTypes) |
                                CalculateComponentsHash(chunkTypes);

    if (auto archetype =
            FindArchetype(genericTypes, chunkTypes, componentsHash))
      return archetype;

    // Archetype wasn't found so we have to create a new one
    return CreateArchetype(genericTypes, chunkTypes, componentsHash);
  }

  void RemoveArchetype(Archetype *pArchetype) {
    const auto idx = utils::get_index(m_archetypeList, pArchetype);
    if (idx == utils::BadIndex)
      return;

    utils::erase_fast(m_archetypeList, idx);
    utils::erase_fast(m_archetypeHashList, idx);

    const auto it = m_archetypeMap.find(pArchetype->componentsHash);
    if (it != m_archetypeMap.end())
      utils::erase_fast(it->second, utils::get_index(it->second, pArchetype));

    delete pArchetype;
  }

  [[nodiscard]] Entity AllocateEntity() {
    if (!m_freeEntities) {
      m_entities.push_back({nullptr, {Entity::IdInvalid, 0}});
      return {(EntityId)m_entities.size() - 1, 0};
    } else {
      --m_freeEntities;
      assert(m_freeEntities != Entity::IdInvalid &&
             "ECS entity list underflow when recycling");
      assert(m_nextFreeEntity < m_entities.size() &&
             "ECS recycle list broken!");

      const auto index = m_nextFreeEntity;
      m_nextFreeEntity = m_entities[m_nextFreeEntity].entity.id();
      return {index, m_entities[index].entity.gen()};
    }
  }

  void DeallocateEntity(Entity entityToDelete) {
    auto &entityContainer = m_entities[entityToDelete.id()];
    entityContainer.chunk = nullptr;

    // New generation. Skip -1 because it has a special meaning
    auto gen = entityContainer.entity.gen() + 1;
    if (gen == Entity::GenInvalid)
      gen = 0;

    // Update our implicit list
    if (!m_freeEntities) {
      m_nextFreeEntity = entityToDelete.id();
      entityContainer.entity = {Entity::IdInvalid, gen};
    } else {
      entityContainer.entity = {m_nextFreeEntity, gen};
      m_nextFreeEntity = entityToDelete.id();
    }
    ++m_freeEntities;
  }

  void StoreEntity(Entity entity, Chunk *chunk) {
    auto &entityContainer = m_entities[entity.id()];
    entityContainer.chunk = chunk;

    const auto idx = chunk->AddEntity(entity);
    const auto gen = entityContainer.entity.gen();
    entityContainer.entity = {idx, gen};
  }

  EntityContainer *
  AddComponent_Internal(ComponentType TYPE, Entity entity,
                        tcb::span<const ComponentMetaData *> typesToAdd) {
    const auto newTypesCount = (uint32_t)typesToAdd.size();
    auto &entityContainer = m_entities[entity.id()];

    // Adding a component to an entity which already is a part of some chunk
    if (auto chunk = entityContainer.chunk) {
      if (chunk->GetEntity(entityContainer.entity.id()) != entity)
        return nullptr;
      assert(entityContainer.entity.gen() == entity.gen());

      const auto &archetype = chunk->header.owner;
      const auto &componentList = archetype.componentList[TYPE];

      const auto metatypesCount =
          (uint32_t)componentList.size() + (uint32_t)typesToAdd.size();
      if (!VerifyMaxComponentCountPerArchetype(metatypesCount)) {
        assert(false && "Trying to add too many ECS components to ECS entity!");
#if GAIA_DEBUG
        LOG_W("Trying to add %u ECS %s components to ECS entity [%u.%u] but "
              "there's only enough room for %u more!",
              newTypesCount, ComponentTypeString[TYPE], entity.id(),
              entity.gen(),
              MAX_COMPONENTS_PER_ARCHETYPE -
                  (uint32_t)archetype.componentList[TYPE].size());
        LOG_W("Already present:");
        for (uint32_t i = 0u; i < componentList.size(); i++)
          LOG_W("> [%u] %s", i, componentList[i].type->name.data());
        LOG_W("Trying to add:");
        for (uint32_t i = 0u; i < newTypesCount; i++)
          LOG_W("> [%u] %s", i, typesToAdd[i]->name.data());
#endif
        return nullptr;
      }

      auto newMetatypes = (const ComponentMetaData **)alloca(
          sizeof(ComponentMetaData) * metatypesCount);

      uint32_t j = 0;
      for (uint32_t i = 0; i < componentList.size(); i++) {
        const auto &info = componentList[i];
        // Don't add the same component twice
        for (uint32_t k = 0; k < newTypesCount; k++) {
          if (info.type == typesToAdd[k]) {
            assert(false &&
                   "Trying to add a duplicate ECS component to ECS entity");
#if GAIA_DEBUG
            LOG_W("Trying to add a duplicate ECS %s component to ECS entity "
                  "[%u.%u]",
                  ComponentTypeString[TYPE], entity.id(), entity.gen());
            LOG_W("> %s", info.type->name.data());
#endif
            return nullptr;
          }
        }
        // Keep the types offset by the number of new input types
        newMetatypes[newTypesCount + j++] = info.type;
      }
      // Fill in the gap with new input types
      for (uint32_t i = 0; i < newTypesCount; i++)
        newMetatypes[i] = typesToAdd[i];

      const auto &secondList = archetype.componentList[(TYPE + 1) & 1];
      auto secondMetaTypes = (const ComponentMetaData **)alloca(
          sizeof(ComponentMetaData) * secondList.size());
      for (uint32_t i = 0; i < secondList.size(); i++)
        secondMetaTypes[i] = secondList[i].type;

      auto newArchetype =
          TYPE == ComponentType::CT_Generic
              ? FindOrCreateArchetype(
                    tcb::span(newMetatypes, metatypesCount),
                    tcb::span(secondMetaTypes, secondList.size()))
              : FindOrCreateArchetype(
                    tcb::span(secondMetaTypes, secondList.size()),
                    tcb::span(newMetatypes, metatypesCount));

      MoveEntity(entity, *newArchetype);
    }
    // Adding a component to an empty entity
    else {
      if (!VerifyMaxComponentCountPerArchetype(newTypesCount)) {
        assert(false && "Trying to add too many ECS components to ECS entity!");
#if GAIA_DEBUG
        LOG_W("Trying to add %u ECS %s components to ECS entity [%u.%u] but "
              "maximum of only %u is supported!",
              newTypesCount, ComponentTypeString[TYPE], entity.id(),
              entity.gen(), MAX_COMPONENTS_PER_ARCHETYPE);
        for (uint32_t i = 0; i < newTypesCount; i++)
          LOG_W("> [%u] %s", i, typesToAdd[i]->name.data());
#endif
        return nullptr;
      }

      auto newArchetype = TYPE == ComponentType::CT_Generic
                              ? FindOrCreateArchetype(typesToAdd, {})
                              : FindOrCreateArchetype({}, typesToAdd);

      StoreEntity(entity, newArchetype->FindOrCreateChunk());
    }

    return &entityContainer;
  }

  template <typename... TComponent>
  EntityContainer *AddComponent_Internal(ComponentType TYPE, Entity entity) {
    VerifyComponents<TComponent...>();

    constexpr auto newTypesCount = (uint32_t)sizeof...(TComponent);
    auto &entityContainer = m_entities[entity.id()];

    // Adding a component to an entity which already is a part of some chunk
    if (auto chunk = entityContainer.chunk) {
      if (chunk->GetEntity(entityContainer.entity.id()) != entity)
        return nullptr;
      assert(entityContainer.entity.gen() == entity.gen());

      const auto &archetype = chunk->header.owner;
      const auto &componentList = archetype.componentList[TYPE];

      const auto metatypesCount =
          (uint32_t)componentList.size() + newTypesCount;
      if (!VerifyMaxComponentCountPerArchetype(metatypesCount)) {
        assert(false && "Trying to add too many ECS components to ECS entity!");
#if GAIA_DEBUG
        LOG_W("Trying to add %u ECS %s components to ECS entity [%u.%u] but "
              "there's only enough room for %u more!",
              newTypesCount, ComponentTypeString[TYPE], entity.id(),
              entity.gen(),
              MAX_COMPONENTS_PER_ARCHETYPE -
                  (uint32_t)archetype.componentList[TYPE].size());
        LOG_W("Already present:");
        for (uint32_t i = 0; i < componentList.size(); i++)
          LOG_W("> [%u] %s", i, componentList[i].type->name.data());
        const char *newNames[] = {
            ComponentMetaData::GetMetaName<TComponent>()...};
        LOG_W("Trying to add:");
        for (uint32_t i = 0; i < newTypesCount; i++)
          LOG_W("> [%u] %s", i, newNames[i]);
#endif
        return nullptr;
      }

      const ComponentMetaData *typesToAdd[] = {
          GetOrCreateComponentMetaType<TComponent>()...};

      auto newMetatypes = (const ComponentMetaData **)alloca(
          sizeof(ComponentMetaData) * metatypesCount);

      uint32_t j = 0;
      for (uint32_t i = 0; i < componentList.size(); i++) {
        const auto &info = componentList[i];

        // Don't add the same component twice
        for (uint32_t k = 0; k < newTypesCount; k++) {
          if (info.type == typesToAdd[k]) {
            assert(false &&
                   "Trying to add a duplicate ECS component to ECS entity");
#if GAIA_DEBUG
            LOG_W("Trying to add a duplicate ECS %s component to ECS entity "
                  "[%u.%u]",
                  ComponentTypeString[TYPE], entity.id(), entity.gen());
            LOG_W("> %s", info.type->name.data());
#endif
            return nullptr;
          }
        }

        // Keep the types offset by the number of new input types
        newMetatypes[newTypesCount + j++] = info.type;
      }
      // Fill in the gap with new input types
      for (uint32_t i = 0; i < newTypesCount; i++)
        newMetatypes[i] = typesToAdd[i];

      const auto &secondList = archetype.componentList[(TYPE + 1) & 1];
      auto secondMetaTypes = (const ComponentMetaData **)alloca(
          sizeof(ComponentMetaData) * secondList.size());
      for (uint32_t i = 0; i < secondList.size(); i++)
        secondMetaTypes[i] = secondList[i].type;

      auto newArchetype =
          TYPE == ComponentType::CT_Generic
              ? FindOrCreateArchetype(
                    tcb::span(newMetatypes, metatypesCount),
                    tcb::span(secondMetaTypes, secondList.size()))
              : FindOrCreateArchetype(
                    tcb::span(secondMetaTypes, secondList.size()),
                    tcb::span(newMetatypes, metatypesCount));

      MoveEntity(entity, *newArchetype);
    }
    // Adding a component to an empty entity
    else {
      if (!VerifyMaxComponentCountPerArchetype(newTypesCount)) {
        assert(false && "Trying to add too many ECS components to ECS entity!");
#if GAIA_DEBUG
        LOG_W("Trying to add %u ECS %s components to ECS entity [%u.%u] but "
              "maximum of only %u is supported!",
              newTypesCount, ComponentTypeString[TYPE], entity.id(),
              entity.gen(), MAX_COMPONENTS_PER_ARCHETYPE);
        const char *newNames[] = {
            ComponentMetaData::GetMetaName<TComponent>()...};
        for (auto i = 0; i < newTypesCount; i++)
          LOG_W("> [%u] %s", i, newNames[i]);
#endif
        return nullptr;
      }

      const ComponentMetaData *newMetatypes[] = {
          GetOrCreateComponentMetaType<TComponent>()...};

      auto newArchetype = TYPE == ComponentType::CT_Generic
                              ? FindOrCreateArchetype(
                                    tcb::span(newMetatypes, newTypesCount), {})
                              : FindOrCreateArchetype(
                                    {}, tcb::span(newMetatypes, newTypesCount));

      StoreEntity(entity, newArchetype->FindOrCreateChunk());
    }

    return &entityContainer;
  }

  void
  RemoveComponent_Internal(ComponentType TYPE, Entity entity,
                           tcb::span<const ComponentMetaData *> typesToRemove) {
    auto &entityContainer = m_entities[entity.id()];
    auto chunk = entityContainer.chunk;
    if (!chunk || chunk->GetEntity(entityContainer.entity.id()) != entity)
      return;
    assert(entityContainer.entity.gen() == entity.gen());

    const auto &archetype = chunk->header.owner;
    const auto &componentList = archetype.componentList[TYPE];

    // find intersection
    const auto metatypesCount = componentList.size();
    auto newMetatypes = (const ComponentMetaData **)alloca(
        sizeof(ComponentMetaData) * metatypesCount);

    size_t typesAfter = 0;
    // TODO: Arrays are sorted so we can make this in O(n+m) instead of O(N^2)
    for (auto i = 0; i < componentList.size(); i++) {
      const auto &info = componentList[i];

      for (auto k = 0; k < typesToRemove.size(); k++) {
        if (info.type == typesToRemove[k])
          goto nextIter;
      }

      newMetatypes[typesAfter++] = info.type;

    nextIter:
      continue;
    }

    // Nothing has changed. Return
    if (typesAfter == metatypesCount)
      return;

    const auto &secondList = archetype.componentList[(TYPE + 1) & 1];
    auto secondMetaTypes = (const ComponentMetaData **)alloca(
        sizeof(ComponentMetaData) * secondList.size());
    for (auto i = 0; i < secondList.size(); i++)
      secondMetaTypes[i] = secondList[i].type;

    auto newArchetype = TYPE == ComponentType::CT_Generic
                            ? FindOrCreateArchetype(
                                  tcb::span(newMetatypes, typesAfter),
                                  tcb::span(secondMetaTypes, secondList.size()))
                            : FindOrCreateArchetype(
                                  tcb::span(secondMetaTypes, secondList.size()),
                                  tcb::span(newMetatypes, typesAfter));

    MoveEntity(entity, *newArchetype);
  }

  template <typename... TComponent>
  void RemoveComponent_Internal(ComponentType TYPE, Entity entity) {
    VerifyComponents<TComponent...>();

    auto &entityContainer = m_entities[entity.id()];
    auto chunk = entityContainer.chunk;
    if (!chunk || chunk->GetEntity(entityContainer.entity.id()) != entity)
      return;
    assert(entityContainer.entity.gen() == entity.gen());

    const ComponentMetaData *typesToRemove[] = {
        GetOrCreateComponentMetaType<TComponent>()...};

    const auto &archetype = chunk->header.owner;
    const auto &componentList = archetype.componentList[TYPE];

    // find intersection
    const auto metatypesCount = componentList.size();
    auto newMetatypes = (const ComponentMetaData **)alloca(
        sizeof(ComponentMetaData) * metatypesCount);

    size_t typesAfter = 0;
    // TODO: Arrays are sorted so we can make this in O(n+m) instead of O(N^2)
    for (auto i = 0; i < componentList.size(); i++) {
      const auto &info = componentList[i];

      for (auto k = 0; k < sizeof...(TComponent); k++) {
        if (info.type == typesToRemove[k])
          goto nextIter;
      }

      newMetatypes[typesAfter++] = info.type;

    nextIter:
      continue;
    }

    // Nothing has changed. Return
    if (typesAfter == metatypesCount)
      return;

    // TODO: Remove the archetype after the last component is removed?
    /*if (typesAfter == 0)
    {
    ...
    }*/

    const auto &secondList = archetype.componentList[(TYPE + 1) & 1];
    auto secondMetaTypes = (const ComponentMetaData **)alloca(
        sizeof(ComponentMetaData) * secondList.size());
    for (uint32_t i = 0; i < secondList.size(); i++)
      secondMetaTypes[i] = secondList[i].type;

    auto newArchetype = TYPE == ComponentType::CT_Generic
                            ? FindOrCreateArchetype(
                                  tcb::span(newMetatypes, typesAfter),
                                  tcb::span(secondMetaTypes, secondList.size()))
                            : FindOrCreateArchetype(
                                  tcb::span(secondMetaTypes, secondList.size()),
                                  tcb::span(newMetatypes, typesAfter));
    MoveEntity(entity, *newArchetype);
  }

  void MoveEntity(Entity oldEntity, Archetype &newArchetype) {
    auto &entityContainer = m_entities[oldEntity.id()];
    auto oldChunk = entityContainer.chunk;
    const auto oldIndex = entityContainer.entity.id();
    const auto &oldArchetype = oldChunk->header.owner;

    // find a new chunk for the entity and move it inside.
    // Old entity ID needs to remain valid or lookups would break.
    auto newChunk = newArchetype.FindOrCreateChunk();
    const auto newIndex = newChunk->AddEntity(oldEntity);

    // find intersection of the two component lists
    const auto &oldInfo = oldArchetype.componentList[ComponentType::CT_Generic];
    const auto &newInfo = newArchetype.componentList[ComponentType::CT_Generic];

    // TODO: Handle chunk components!

    struct Intersection {
      uint32_t size;
      uint32_t oldIndex;
      uint32_t newIndex;
    };

    const auto maxIntersectionCount =
        oldInfo.size() > newInfo.size() ? newInfo.size() : oldInfo.size();
    auto intersections =
        (Intersection *)alloca(maxIntersectionCount * sizeof(Intersection));
    uint32_t intersectionCount = 0;

    // TODO: Arrays are sorted so we can do this in O(n+_) instead of O(N^2)
    for (uint32_t i = 0; i < oldInfo.size(); i++) {
      const auto typeOld = oldInfo[i].type;
      if (!typeOld->size)
        continue;

      for (uint32_t j = 0; j < newInfo.size(); j++) {
        const auto typeNew = newInfo[j].type;
        if (typeNew == typeOld) {
          intersections[intersectionCount] = {typeOld->size, i, j};
          ++intersectionCount;
        }
      }
    }

    // Let's move all data from oldEntity to newEntity
    for (uint32_t i = 0; i < intersectionCount; i++) {
      const uint32_t idxFrom = newInfo[intersections[i].newIndex].offset +
                               intersections[i].size * newIndex;
      const uint32_t idxTo = oldInfo[intersections[i].oldIndex].offset +
                             intersections[i].size * oldIndex;

      assert(idxFrom < Chunk::DATA_SIZE);
      assert(idxTo < Chunk::DATA_SIZE);

      memcpy(&newChunk->data[idxFrom], &oldChunk->data[idxTo],
             intersections[i].size);
    }

    // Remove entity from the previous chunk
    RemoveEntity(oldChunk, oldIndex);

    // Update entity's chunk and index so look-ups can find it
    entityContainer.chunk = newChunk;
    entityContainer.entity = {newIndex, oldEntity.gen()};

#if ECS_VALIDATE_CHUNKS
    // Make sure no entites reference the deleted chunk
    for (int i = 0; i < m_entities.size(); i++) {
      const auto &e = m_entities[i];
      assert(oldChunk->HasEntities() || e.chunk != oldChunk);
      assert((e.chunk == nullptr && e.entity.id() == Entity::IdInvalid) ||
             (e.chunk != nullptr && e.entity.id() != Entity::IdInvalid));
    }
#endif
  }

  void Init() { ++WorldCount; }

  void Done() {
    Cleanup();

    // Make sure Init/Done is called the correct amount of times
    assert(WorldCount > 0);
    --WorldCount;

    if (!WorldCount) {
      ClearRegisteredTypeCache();
      g_ChunkAllocator.Flush();

#if GAIA_DEBUG
      // Make sure there are no leaks
      SmallBlockAllocatorStats memstats;
      ecs::g_ChunkAllocator.GetStats(memstats);
      if (memstats.NumAllocations != 0) {
        LOG_W("ECS leaking memory: %u of unreleased allocations (blocks:%u, "
              "allocated:%" PRIu64 "B, overhead:%" PRIu64 "B)",
              (uint32_t)memstats.NumAllocations, (uint32_t)memstats.NumBlocks,
              memstats.Allocated, memstats.Overhead);
      }
#endif
    }
  }

public:
  World() { Init(); }

  ~World() { Done(); }

  World(World &&) = delete;
  World(const World &) = delete;
  World &operator=(World &&) = delete;
  World &operator=(const World &) = delete;

  void Cleanup() {
    // Clear entities
    {
      m_entities = {};
      m_nextFreeEntity = 0;
      m_freeEntities = 0;
    }

    // Clear archetypes
    {
      m_chunksToRemove = {};

      // Delete all allocated chunks and their parent archetypes
      for (auto archetype : m_archetypeList)
        delete archetype;

      m_archetypeList = {};
      m_archetypeHashList = {};
      m_archetypeMap = {};
    }
  }

  void Diag() {
    DiagArchetypes();
    DiagRegisteredTypes();
    DiagEntities();
  }

  [[nodiscard]] uint32_t GetWorldVersion() const { return m_worldVersion; }

  [[nodiscard]] Archetype *FindOrCreateArchetype(CreationQuery &query) {
    // TODO: Verify if return FindOrCreateArchetype(tcb::span<const
    // ComponentMetaData*>(types)) works the same way
    return FindOrCreateArchetype(
        tcb::span(query.list[ComponentType::CT_Generic]),
        tcb::span(query.list[ComponentType::CT_Chunk]));
  }

  [[nodiscard]] Archetype *GetArchetype(Entity entity) const {
    auto &entityContainer = m_entities[entity.id()];
    auto chunk = entityContainer.chunk;
    if (!chunk || chunk->GetEntity(entityContainer.entity.id()) != entity)
      return nullptr;
    assert(entityContainer.entity.gen() == entity.gen());

    return (Archetype *)&entityContainer.chunk->header.owner;
  }

  /*!
  Creates a new empty entity
  \return Entity
  */
  [[nodiscard]] Entity CreateEntity() { return AllocateEntity(); }

  /*!
  Creates a new entity from archetype
  \return Entity
  */
  Entity CreateEntity(Archetype &archetype) {
    Entity entity = AllocateEntity();

    auto chunk = m_entities[entity.id()].chunk;
    if (chunk == nullptr)
      chunk = archetype.FindOrCreateChunk();

    StoreEntity(entity, chunk);

    return entity;
  }

  /*!
  Creates a new entity from query
  \return Entity
  */
  Entity CreateEntity(CreationQuery &query) {
    auto archetype = FindOrCreateArchetype(query);
    return CreateEntity(*archetype);
  }

  /*!
  Creates a new entity by cloning an already existing one
  \return Entity
  */
  Entity CreateEntity(Entity entity) {
    auto &entityContainer = m_entities[entity.id()];
    auto chunk = entityContainer.chunk;
    auto &archetype = const_cast<Archetype &>(chunk->header.owner);

    Entity newEntity = CreateEntity(archetype);
    auto &newEntityContainer = m_entities[newEntity.id()];
    auto newChunk = newEntityContainer.chunk;

    // By adding a new entity m_entities array might have been reallocated.
    // We need to get the new address.
    auto &oldEntityContainer = m_entities[entity.id()];
    auto oldChunk = oldEntityContainer.chunk;

    // Copy generic component data from reference entity to our new entity
    const auto &info = archetype.componentList[ComponentType::CT_Generic];
    for (uint32_t i = 0; i < info.size(); i++) {
      const auto type = info[i].type;
      if (!type->size)
        continue;

      const uint32_t idxFrom =
          info[i].offset + type->size * oldEntityContainer.entity.id();
      const uint32_t idxTo =
          info[i].offset + type->size * newEntityContainer.entity.id();

      assert(idxFrom < Chunk::DATA_SIZE);
      assert(idxTo < Chunk::DATA_SIZE);

      memcpy(&newChunk->data[idxTo], &oldChunk->data[idxFrom], type->size);
    }

    return newEntity;
  }

  /*!
  Removes an entity
  */
  void DeleteEntity(Entity entityToDelete) {
    if (m_entities.empty() || !entityToDelete.IsValid())
      return;

    auto &entityContainer = m_entities[entityToDelete.id()];
    auto chunk = entityContainer.chunk;
    if (!chunk ||
        chunk->GetEntity(entityContainer.entity.id()) != entityToDelete)
      return;
    assert(entityContainer.entity.gen() == entityToDelete.gen());

    // Remove entity from chunk
    RemoveEntity(chunk, entityContainer.entity.id());

    // Return entity to pool
    DeallocateEntity(entityToDelete);

#if ECS_VALIDATE_CHUNKS
    // Make sure no entites reference the deleted chunk
    for (int i = 0; i < m_entities.size(); i++) {
      const auto &e = m_entities[i];
      assert(chunk->HasEntities() || e.chunk != chunk);
      assert((e.chunk == nullptr && e.entity.id() == Entity::IdInvalid) ||
             (e.chunk != nullptr && e.entity.id() != Entity::IdInvalid));
    }
#endif
  }

  /*!
  Returns a entity at a given position
  \return Entity
  */
  [[nodiscard]] Entity GetEntity(uint32_t idx) const {
    assert(idx < m_entities.size());
    auto &entityContainer = m_entities[idx];
    return {idx, entityContainer.entity.gen()};
  }

  /*!
  Returns a chunk containing the given entity.
  \return Chunk or nullptr if not found
  */
  [[nodiscard]] Chunk *GetEntityChunk(Entity entity) const {
    assert(entity.id() < m_entities.size());
    auto &entityContainer = m_entities[entity.id()];
    return entityContainer.chunk;
  }

  /*!
  Returns a chunk containing the given entity.
  Index of the entity is stored in \param indexInChunk
  \return Chunk or nullptr if not found
  */
  [[nodiscard]] Chunk *GetEntityChunk(Entity entity,
                                      uint32_t &indexInChunk) const {
    assert(entity.id() < m_entities.size());
    auto &entityContainer = m_entities[entity.id()];
    indexInChunk = entityContainer.entity.id();
    return entityContainer.chunk;
  }

  template <typename... TComponent> void AddComponent(Entity entity) {
    AddComponent_Internal<TComponent...>(ComponentType::CT_Generic, entity);
  }

  template <typename... TComponent>
  void AddComponent(Entity entity, ECS_SMART_ARG(TComponent)... data) {
    if (auto entityContainer = AddComponent_Internal<TComponent...>(
            ComponentType::CT_Generic, entity))
      SetComponentsDataInternal<TComponent...>(
          ComponentType::CT_Generic, entityContainer->chunk,
          entityContainer->entity.id(), std::forward<TComponent>(data)...);
  }

  template <typename... TComponent> void AddChunkComponent(Entity entity) {
    AddComponent_Internal<TComponent...>(ComponentType::CT_Chunk, entity);
  }

  template <typename... TComponent>
  void AddChunkComponent(Entity entity, ECS_SMART_ARG(TComponent)... data) {
    if (auto entityContainer = AddComponent_Internal<TComponent...>(
            ComponentType::CT_Chunk, entity))
      SetComponentsDataInternal<TComponent...>(
          ComponentType::CT_Chunk, entityContainer->chunk,
          entityContainer->entity.id(), std::forward<TComponent>(data)...);
  }

  template <typename... TComponent> void RemoveComponent(Entity entity) {
    RemoveComponent_Internal<TComponent...>(ComponentType::CT_Generic, entity);
  }

  template <typename... TComponent> void RemoveChunkComponent(Entity entity) {
    RemoveComponent_Internal<TComponent...>(ComponentType::CT_Chunk, entity);
  }

  template <typename... TComponent>
  void SetComponentData(Entity entity, ECS_SMART_ARG(TComponent)... data) {
    VerifyComponents<TComponent...>();

    auto &entityContainer = m_entities[entity.id()];
    auto chunk = entityContainer.chunk;
    if (!chunk || chunk->GetEntity(entityContainer.entity.id()) != entity)
      return;
    assert(entityContainer.entity.gen() == entity.gen());

    SetComponentsDataInternal<TComponent...>(ComponentType::CT_Generic, chunk,
                                             entityContainer.entity.id(),
                                             std::forward<TComponent>(data)...);
  }

  template <typename... TComponent>
  void SetChunkComponentData(Entity entity, ECS_SMART_ARG(TComponent)... data) {
    VerifyComponents<TComponent...>();

    auto &entityContainer = m_entities[entity.id()];
    auto chunk = entityContainer.chunk;
    if (!chunk || chunk->GetEntity(entityContainer.entity.id()) != entity)
      return;
    assert(entityContainer.entity.gen() == entity.gen());

    SetComponentsDataInternal<TComponent...>(ComponentType::CT_Chunk, chunk,
                                             entityContainer.entity.id(),
                                             std::forward<TComponent>(data)...);
  }

  template <typename... TComponent>
  void GetComponentData(Entity entity, TComponent &...data) {
    VerifyComponents<TComponent...>();

    auto &entityContainer = m_entities[entity.id()];
    auto chunk = entityContainer.chunk;
    if (!chunk || chunk->GetEntity(entityContainer.entity.id()) != entity)
      return;
    assert(entityContainer.entity.gen() == entity.gen());

    GetComponentsDataInternal<TComponent...>(
        ComponentType::CT_Generic, chunk, entityContainer.entity.id(), data...);
  }

  template <typename... TComponent>
  void GetChunkComponentData(Entity entity, TComponent &...data) {
    VerifyComponents<TComponent...>();

    auto &entityContainer = m_entities[entity.id()];
    auto chunk = entityContainer.chunk;
    if (!chunk || chunk->GetEntity(entityContainer.entity.id()) != entity)
      return;
    assert(entityContainer.entity.gen() == entity.gen());

    GetComponentsDataInternal<TComponent...>(
        ComponentType::CT_Chunk, chunk, entityContainer.entity.id(), data...);
  }

private:
  template <typename TComponent>
  void SetComponentDataInternal(ComponentType TYPE, Chunk *chunk,
                                uint32_t index,
                                ECS_SMART_ARG(TComponent) data) {
    if constexpr (!std::is_empty<TComponent>::value)
      chunk->SetComponent_Internal<TComponent>(TYPE, index) =
          std::forward<TComponent>(data);
  }

  template <typename... TComponent>
  void SetComponentsDataInternal(ComponentType TYPE, Chunk *chunk,
                                 uint32_t index,
                                 ECS_SMART_ARG(TComponent)... data) {
    (SetComponentDataInternal<TComponent>(TYPE, chunk, index,
                                          std::forward<TComponent>(data)),
     ...);
  }

  template <typename TComponent>
  void GetComponentDataInternal(ComponentType TYPE, Chunk *chunk,
                                uint32_t index, TComponent &data) const {
    if constexpr (!std::is_empty<TComponent>::value)
      data = chunk->GetComponent_Internal<TComponent>(TYPE, index);
    else
      assert(false && "Getting value of an empty component is most likely not "
                      "what you want...");
  }

  template <typename... TComponent>
  void GetComponentsDataInternal(ComponentType TYPE, Chunk *chunk,
                                 uint32_t index, TComponent &...data) const {
    (GetComponentDataInternal<TComponent>(TYPE, chunk, index, data), ...);
  }

  template <class T>
  struct IsReadOnlyType
      : std::bool_constant<
            std::is_const<
                std::remove_reference_t<std::remove_pointer_t<T>>>::value ||
            (!std::is_pointer<T>::value && !std::is_reference<T>::value)> {};

  template <class T, std::enable_if_t<IsReadOnlyType<T>::value> * = nullptr>
  constexpr const std::decay_t<T> *expandTuple(Chunk &chunk) const {
    return chunk.ViewRO<T>();
  }

  template <class T, std::enable_if_t<!IsReadOnlyType<T>::value> * = nullptr>
  constexpr std::decay_t<T> *expandTuple(Chunk &chunk) {
    return chunk.ViewRW<T>();
  }

  template <typename... TFuncArgs, typename TFunc>
  void ForEachEntityInChunk(Chunk &chunk, TFunc &&func) {
    auto tup = std::make_tuple(expandTuple<TFuncArgs>(chunk)...);
    for (uint16_t i = 0; i < chunk.GetItemCount(); i++)
      func(std::get<decltype(expandTuple<TFuncArgs>(chunk))>(tup)[i]...);
  }

  enum class MatchArchetypeQueryRet { Fail, Ok, Skip };

  template <ComponentType TYPE>
  [[nodiscard]] MatchArchetypeQueryRet
  MatchArchetypeQuery(const Archetype &archetype, const EntityQuery &query) {
    const auto &componentList = archetype.componentList[TYPE];

    const uint64_t withNoneTest =
        archetype.componentsHash & query.list[TYPE].hashNone;
    const uint64_t withAnyTest =
        archetype.componentsHash & query.list[TYPE].hashAny;
    const uint64_t withAllTest =
        archetype.componentsHash & query.list[TYPE].hashAll;

    // If there is any match with the withNoneList we continue with the next
    // archetype
    if (withNoneTest != 0) {
      // withNoneList first because we usually request for less components that
      // there are components in archetype
      for (const auto type : query.list[TYPE].listNone) {
        for (const auto &component : componentList) {
          if (component.type == type) {
            DiagNotMatching("withNone", componentList,
                            query.list[TYPE].listNone);
            return MatchArchetypeQueryRet::Fail;
          }
        }
      }
    }

    // We need to have at least one match inside withAnyList
    if (withAnyTest != 0) {
      uint32_t matches = 0;
      // withAnyList first because we usually request for less components that
      // there are components in archetype
      for (const auto type : query.list[TYPE].listAny) {
        for (const auto &component : componentList) {
          if (component.type == type) {
            ++matches;
            goto checkIncludeAnyMatches;
          }
        }
      }

    checkIncludeAnyMatches:
      // At least one match has been found to continue with evaluation
      if (!matches) {
        DiagNotMatching("withAny", componentList, query.list[TYPE].listAny);
        return MatchArchetypeQueryRet::Fail;
      }
    }

    // If withAllList is not empty there has to be an exact match
    if (withAllTest != 0) {
      // If the number of queried components is greater than the number of
      // components in archetype there's no need to search
      if (query.list[TYPE].listAll.size() <= componentList.size()) {
        uint32_t matches = 0;

        // withAllList first because we usually request for less components than
        // there are components in archetype
        for (const auto type : query.list[TYPE].listAll) {
          for (const auto &component : componentList) {
            if (component.type != type)
              continue;

            // All requirements are fulfilled. Let's iterate over all chunks in
            // archetype
            if (++matches == query.list[TYPE].listAll.size())
              return MatchArchetypeQueryRet::Ok;

            break;
          }
        }
      }

      // No match found. We're done
      DiagNotMatching("withAll", componentList, query.list[TYPE].listAll);
      return MatchArchetypeQueryRet::Fail;
    }

    return MatchArchetypeQueryRet::Skip;
  }

  template <typename TFunc>
  void ForEachArchetype(const EntityQuery &query, TFunc &&func) {
    for (auto a : m_archetypeList) {
      const auto &archetype = *a;

      // Early exit if generic query doesn't match
      const auto retGeneric =
          MatchArchetypeQuery<ComponentType::CT_Generic>(archetype, query);
      if (retGeneric == MatchArchetypeQueryRet::Fail)
        continue;
      // Early exit if chunk query doesn't match
      const auto retChunk =
          MatchArchetypeQuery<ComponentType::CT_Chunk>(archetype, query);
      if (retChunk == MatchArchetypeQueryRet::Fail)
        continue;

      // If at least one query succeeded run our logic
      if (retGeneric == MatchArchetypeQueryRet::Ok ||
          retChunk == MatchArchetypeQueryRet::Ok)
        func(archetype);
    }
  }

  template <typename... TComponents, typename TFunc>
  void Unpack_ForEachEntityInChunk(FuncTypeList<TComponents...> types,
                                   Chunk &chunk, TFunc &&func) {
    ForEachEntityInChunk<TComponents...>(chunk, func);
  }

  template <typename... TComponents>
  [[nodiscard]] EntityQuery &
  Unpack_ForEachQuery(FuncTypeList<TComponents...> types, EntityQuery &query) {
    if constexpr (!sizeof...(TComponents))
      return query;
    else
      return query.WithAll<TComponents...>();
  }

  [[nodiscard]] static bool CanProcessChunk(EntityQuery &query, Chunk &chunk,
                                            uint32_t lastSystemVersion) {
    // Skip empty chunks
    if (!chunk.HasEntities())
      return false;

    // Skip unchanged chunks
    const bool checkGenericComponents =
        !query.listChangeFiltered[ComponentType::CT_Generic].empty();
    const bool checkChunkComponents =
        !query.listChangeFiltered[ComponentType::CT_Chunk].empty();
    if (checkGenericComponents || checkChunkComponents) {
      bool genericChanged = false;
      bool chunkChanged = false;

      for (auto typeIdx : query.listChangeFiltered[ComponentType::CT_Generic]) {
        const uint32_t componentIdx = chunk.GetComponentIdx(typeIdx);
        assert(componentIdx !=
               (uint32_t)-1); // the component must exist at this point!

        if (!chunk.DidChange(ComponentType::CT_Generic, lastSystemVersion,
                             componentIdx))
          continue;

        genericChanged = true;
        break;
      }

      for (auto typeIdx : query.listChangeFiltered[ComponentType::CT_Chunk]) {
        const uint32_t componentIdx = chunk.GetChunkComponentIdx(typeIdx);
        assert(componentIdx !=
               (uint32_t)-1); // the component must exist at this point!

        if (!chunk.DidChange(ComponentType::CT_Chunk, lastSystemVersion,
                             componentIdx))
          continue;

        chunkChanged = true;
        break;
      }

      if (!genericChanged && !chunkChanged)
        return false;
    }

    return true;
  }

  template <typename TFunc>
  static void RunQueryOnChunks_Direct(World &world, EntityQuery &query,
                                      TFunc &func, uint32_t lastSystemVersion) {
    // Add WithAll filter for components listed as input arguments of func
    query.Commit(world.GetWorldVersion());

    // Iterate over all archetypes
    world.ForEachArchetype(query, [&](const Archetype &archetype) {
      for (auto chunk : archetype.chunks) {
        if (!CanProcessChunk(query, *chunk, lastSystemVersion))
          continue;

        func(*chunk);
      }
    });
  }

  template <typename TFunc>
  static void RunQueryOnChunks_Indirect(World &world, EntityQuery &query,
                                        TFunc &func,
                                        uint32_t lastSystemVersion) {
    using InputArgs = decltype(FuncArgs(&TFunc::operator()));

    // Add WithAll filter for components listed as input arguments of func
    world.Unpack_ForEachQuery(InputArgs{}, query)
        .Commit(world.GetWorldVersion());

    // Iterate over all archetypes
    world.ForEachArchetype(query, [&](const Archetype &archetype) {
      for (auto chunk : archetype.chunks) {
        if (!CanProcessChunk(query, *chunk, lastSystemVersion))
          continue;

        world.Unpack_ForEachEntityInChunk(
            InputArgs{}, *chunk, func); // Don't move func here. We need it for
                                        // every further function calls
      }
    });
  }

  //--------------------------------------------------------------------------------

  template <typename TFunc> struct ForEachChunkExecutionContext {
    World &world;
    EntityQuery &query;
    TFunc func;

  public:
    ForEachChunkExecutionContext(World &w, EntityQuery &q, TFunc &&f)
        : world(w), query(q), func(std::forward<TFunc>(f)) {}
    void Run(uint32_t lastSystemVersion) {
      World::RunQueryOnChunks_Direct(this->world, this->query, this->func,
                                     lastSystemVersion);
    }
  };

  //--------------------------------------------------------------------------------

  template <typename TFunc, bool InternalQuery> struct ForEachExecutionContext {
    using TQuery =
        std::conditional_t<InternalQuery, EntityQuery, EntityQuery &>;

    World &world;
    TQuery query;
    TFunc func;

  public:
    ForEachExecutionContext(World &w, EntityQuery &q, TFunc &&f)
        : world(w), query(q), func(std::forward<TFunc>(f)) {
      static_assert(!InternalQuery,
                    "lvalue/prvalue can be used only with external queries");
    }
    ForEachExecutionContext(World &w, EntityQuery &&q, TFunc &&f)
        : world(w), query(std::move(q)), func(std::forward<TFunc>(f)) {
      static_assert(InternalQuery,
                    "rvalue can be used only with internal queries");
    }
    void Run(uint32_t lastSystemVersion) {
      World::RunQueryOnChunks_Indirect(this->world, this->query, this->func,
                                       lastSystemVersion);
    }
  };

  //--------------------------------------------------------------------------------

public:
  template <typename TFunc>
  [[nodiscard]] ForEachChunkExecutionContext<TFunc>
  ForEachChunk(EntityQuery &query, TFunc &&func) {
    return {(World &)*this, query, std::forward<TFunc>(func)};
  }

  template <typename TFunc>
  [[nodiscard]] ForEachExecutionContext<TFunc, false>
  ForEach(EntityQuery &query, TFunc &&func) {
    return {(World &)*this, query, std::forward<TFunc>(func)};
  }

  template <typename TFunc>
  [[nodiscard]] ForEachExecutionContext<TFunc, true>
  ForEach(EntityQuery &&query, TFunc &&func) {
    return {(World &)*this, std::move(query), std::forward<TFunc>(func)};
  }

  template <typename TFunc>
  [[nodiscard]] ForEachExecutionContext<TFunc, true> ForEach(TFunc &&func) {
    return {(World &)*this, EntityQuery(), std::forward<TFunc>(func)};
  }

private:
  //! Remove entity from a chunk. If the chunk is empty it's queued for removal
  //! as well.
  void RemoveEntity(Chunk *pChunk, uint16_t entityChunkIndex) {
    pChunk->RemoveEntity(entityChunkIndex, m_entities);

    if (
        // Skip non-empty chunks
        pChunk->HasEntities() ||
        // Skip chunks which already requested removal
        pChunk->header.remove)
      return;

    // When the chunk is emptied we want it to be removed. We can't do it right
    // away and need to wait for world's GC to be called.
    //
    // However, we need to prevent the following:
    //    1) chunk is emptied + add to some removal list
    //    2) chunk is reclaimed
    //    3) chunk is emptied again + add to some removal list again
    //
    // Therefore, we have a flag telling us the chunk is already waiting to be
    // removed. The chunk might be reclaimed before GC happens but it simply
    // ignores such requests. This way GC always has at most one record for
    // removal for any given chunk.
    pChunk->header.remove = true;

    m_chunksToRemove.push_back(pChunk);
  }

  //! Collect garbage
  void GC() {
    // Handle memory released by chunks and archetypes
    for (auto chunk : m_chunksToRemove) {
      // Skip chunks which were reused in the meantime
      if (chunk->HasEntities()) {
        // Reset the flag
        chunk->header.remove = false;
        continue;
      }

      assert(chunk->header.remove);
      auto &archetype = const_cast<Archetype &>(chunk->header.owner);

      // Remove the chunk if it's no longer needed
      archetype.RemoveChunk(chunk);

      // Remove the archetype if it's no longer used
      if (archetype.chunks.empty())
        RemoveArchetype(&archetype);
    }

    m_chunksToRemove.clear();
  }

  void DiagArchetypes() {
    static bool DiagArchetypes = GAIA_ECS_DIAG_ARCHETYPES;
    if (DiagArchetypes) {
      DiagArchetypes = false;
      std::unordered_map<uint64_t, uint32_t> archetypeEntityCountMap;
      for (const auto *archetype : m_archetypeList)
        archetypeEntityCountMap.insert({archetype->componentsHash, 0});

      // Calculate the number of entities using a given archetype
      for (const auto &e : m_entities) {
        if (!e.chunk)
          continue;

        auto it =
            archetypeEntityCountMap.find(e.chunk->header.owner.componentsHash);
        if (it != archetypeEntityCountMap.end())
          ++it->second;
      }

      // Print archetype info
      LOG_N("Archetypes:%u", (uint32_t)m_archetypeList.size());
      for (const auto *archetype : m_archetypeList) {
        const auto &genericComponents =
            archetype->componentList[ComponentType::CT_Generic];
        const auto &chunkComponents =
            archetype->componentList[ComponentType::CT_Chunk];
        uint32_t genericComponentsSize = 0;
        uint32_t chunkComponentsSize = 0;
        for (const auto &component : genericComponents)
          genericComponentsSize += component.type->size;
        for (const auto &component : chunkComponents)
          chunkComponentsSize += component.type->size;

        const auto it = archetypeEntityCountMap.find(archetype->componentsHash);
        LOG_N("Archetype ID:%016" PRIx64
              ", chunks:%u, data size:%u B (%u + %u), entities:%u/%u",
              archetype->componentsHash, (uint32_t)archetype->chunks.size(),
              genericComponentsSize + chunkComponentsSize,
              genericComponentsSize, chunkComponentsSize, it->second,
              archetype->capacity);

        auto logInfo = [](const ChunkComponentList &components) {
          for (const auto &component : components) {
            const auto type = component.type;
#if GAIA_DEBUG
            LOG_N("--> (%p) nameHash:%016" PRIx64 ", compHash:%016" PRIx64
                  ", size:%3u B, align:%3u B, %.*s",
                  type, type->nameHash, type->componentHash, type->size,
                  type->alig, (uint32_t)type->name.length(), type->name.data());
#else
            LOG_N("--> (%p) nameHash:%016" PRIx64 ", compHash:%016" PRIx64
                  ", size:%3u B, align:%3u B",
                  type, type->nameHash, type->componentHash, type->size,
                  type->alig);
#endif
          }
        };

        if (!genericComponents.empty()) {
          LOG_N("Generic components - count:%u",
                (uint32_t)genericComponents.size());
          logInfo(genericComponents);
        }
        if (!chunkComponents.empty()) {
          LOG_N("Chunk components - count:%u",
                (uint32_t)chunkComponents.size());
          logInfo(chunkComponents);
        }
      }
    }
  }

  void DiagRegisteredTypes() {
    static bool DiagRegisteredTypes = GAIA_ECS_DIAG_REGISTERED_TYPES;
    if (DiagRegisteredTypes) {
      DiagRegisteredTypes = false;

      uint32_t registeredTypes = 0;
      for (const auto *type : g_ComponentMetaTypeCache) {
        if (type == nullptr)
          continue;

        ++registeredTypes;
      }
      LOG_N("Registered types: %u", registeredTypes);

      for (const auto *type : g_ComponentMetaTypeCache) {
        if (type == nullptr)
          continue;

#if GAIA_DEBUG
        LOG_N("--> (%p) nameHash:%016" PRIx64 ", compHash:%016" PRIx64 ", %.*s",
              type, type->nameHash, type->componentHash,
              (uint32_t)type->name.length(), type->name.data());
#else
        LOG_N("--> (%p) nameHash:%016" PRIx64 ", compHash:%016" PRIx64, type,
              type->nameHash, type->componentHash);
#endif
      }

      using DuplicateMap =
          std::unordered_map<uint64_t, std::vector<const ComponentMetaData *>>;

      auto checkDuplicity = [](const DuplicateMap &map, bool errIfDuplicate) {
        for (const auto &pair : map) {
          if (pair.second.size() <= 1)
            continue;

          if (errIfDuplicate) {
            LOG_E("Duplicity detected for key %016" PRIx64 "", pair.first);
          } else {
            LOG_N("Duplicity detected for key %016" PRIx64 "", pair.first);
          }

          for (const auto *type : pair.second) {
            if (type == nullptr)
              continue;

#if GAIA_DEBUG
            LOG_N("--> (%p) nameHash:%016" PRIx64 ", compHash:%016" PRIx64
                  ", %.*s",
                  type, type->nameHash, type->componentHash,
                  (uint32_t)type->name.length(), type->name.data());
#else
            LOG_N("--> (%p) nameHash:%016" PRIx64 ", compHash:%016" PRIx64,
                  type, type->nameHash, type->componentHash);
#endif
          }
        }
      };

      // Name hash duplicity. These must be unique
      {
        bool hasDuplicates = false;
        DuplicateMap m;
        for (const auto *type : g_ComponentMetaTypeCache) {
          if (type == nullptr)
            continue;

          const auto it = m.find(type->nameHash);
          if (it == m.end())
            m.insert({type->nameHash, {type}});
          else {
            it->second.push_back(type);
            hasDuplicates = true;
          }
        }

        if (hasDuplicates)
          checkDuplicity(m, true);
      }

      // Component matcher hash duplicity. These are fine if not unique
      // However, the more unique the lower the probability of us having to
      // check multiple archetype headers when matching queries.
      {
        bool hasDuplicates = false;
        DuplicateMap m;
        for (const auto *type : g_ComponentMetaTypeCache) {
          if (type == nullptr)
            continue;

          const auto it = m.find(type->componentHash);
          if (it == m.end())
            m.insert({type->componentHash, {type}});
          else {
            it->second.push_back(type);
            hasDuplicates = true;
          }
        }

        if (hasDuplicates)
          checkDuplicity(m, false);
      }
    }
  }

  void
  DiagNotMatching(const char *text, const ChunkComponentList &componentList,
                  const EntityQuery::ComponentMetaDataArray &listToCompare) {
    static bool DiagTypeMatching = GAIA_ECS_DIAG_TYPEMATCHING;
    if (DiagTypeMatching) {
      DiagTypeMatching = false;

      LOG_N("MatchArchetypeQuery - %s, components to compare: %u", text,
            (uint32_t)componentList.size());
      for (const auto &component : componentList) {
        const auto type = component.type;
#if GAIA_DEBUG
        LOG_N("--> (%p) nameHash:%016" PRIx64 ", compHash:%016" PRIx64 ", %.*s",
              type, type->nameHash, type->componentHash,
              (uint32_t)type->name.length(), type->name.data());
#else
        LOG_N("--> (%p) nameHash:%016" PRIx64 ", compHash:%016" PRIx64, type,
              type->nameHash, type->componentHash);
#endif
      }

      LOG_N("types to match: %u", (uint32_t)listToCompare.size());
      for (const auto type : listToCompare) {
#if GAIA_DEBUG
        LOG_N("--> (%p) nameHash:%016" PRIx64 ", compHash:%016" PRIx64 ", %.*s",
              type, type->nameHash, type->componentHash,
              (uint32_t)type->name.length(), type->name.data());
#else
        LOG_N("--> (%p) nameHash:%016" PRIx64 ", compHash:%016" PRIx64, type,
              type->nameHash, type->componentHash);
#endif
      }
    }
  }

  void DiagEntities() {
    static bool DiagDeletedEntities = GAIA_ECS_DIAG_DELETED_ENTITIES;
    if (DiagDeletedEntities) {
      DiagDeletedEntities = false;

      LOG_N("Deleted entities: %u", m_freeEntities);
      if (m_freeEntities) {
        LOG_N("--> %u", m_nextFreeEntity);

        uint32_t iters = 0;
        auto fe = m_entities[m_nextFreeEntity].entity.id();
        while (fe != Entity::IdInvalid) {
          LOG_N("--> %u", m_entities[fe].entity.id());
          fe = m_entities[fe].entity.id();
          ++iters;
          if (!iters || iters > m_freeEntities) {
            LOG_E("  Entities recycle list contains inconsistent data!");
            break;
          }
        }

        // At this point the index of the last index in list should point to -1
        // because that's the tail of our implicit list with m_freeEntities > 0.
        assert(fe == Entity::IdInvalid);
      }
    }
  }
};

uint32_t GetWorldVersionFromWorld(const World &world) {
  return world.GetWorldVersion();
}
} // namespace ecs
} // namespace gaia