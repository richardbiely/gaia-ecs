#pragma once
#include <inttypes.h>
#include <vector>

#include "archetype.h"
#include "component.h"
#include "entity.h"
#include "fwd.h"
#include "world.h"

namespace gaia {
	namespace ecs {

		struct TempEntity final {
			uint32_t id;
		};

		/*!
		Buffer for deferred execution of some operations on entities.

		Adding and removing components and entities inside World::ForEach or
		World::ForEachChunk can result in changes of archetypes or chunk structure.
		This would lead to undefined behavior. Therefore, such operations have to be
		executed after the loop is done.
		*/
		class CommandBuffer final {
			enum CommandBufferCmd : uint8_t {
				CREATE_ENTITY,
				CREATE_ENTITY_FROM_ARCHETYPE,
				CREATE_ENTITY_FROM_QUERY,
				CREATE_ENTITY_FROM_ENTITY,
				ADD_COMPONENT,
				ADD_COMPONENT_TO_TEMPENTITY,
				SET_COMPONENT,
				SET_COMPONENT_FOR_TEMPENTITY,
				REMOVE_COMPONENT
			};

			friend class World;

			std::vector<uint8_t> m_data;
			uint32_t m_entities;

			template <typename TEntity, typename... TComponent>
			void AddComponent_Internal(TEntity entity) {
				// Entity
				{
					const auto lastIndex = m_data.size();
					m_data.resize(m_data.size() + sizeof(TEntity));

					utils::unaligned_ref<TEntity> to(&m_data[lastIndex]);
					to = entity;
				}
				// Components
				{
					const ComponentMetaData* typesToAdd[] = {
							g_ComponentCache.GetOrCreateComponentMetaType<TComponent>()...};

					// Component count
					constexpr auto componentCount = (uint8_t)sizeof...(TComponent);
					m_data.push_back(componentCount);

					// Component meta data
					const auto lastIndex = m_data.size();
					m_data.resize(
							m_data.size() + sizeof(ComponentMetaData*) * componentCount);

					for (uint8_t i = 0; i < componentCount; i++) {
						utils::unaligned_ref<ComponentMetaData*> to(&m_data[lastIndex]);
						to = typesToAdd[i];
					}
				}
			}

			template <typename T>
			void SetComponent_Internal(uint32_t& index, T&& data) {
				using TComponent = std::decay_t<T>;

				// Meta type
				*(uint32_t*)&m_data[index] = utils::type_info::index<TComponent>();

				// Component data
				*(TComponent*)&m_data[index + sizeof(uint32_t)] =
						std::forward<TComponent>(data);

				index += sizeof(uint32_t) + sizeof(TComponent);
			}

			template <typename TEntity, typename... TComponent>
			void SetComponent_Internal(TEntity entity, TComponent&&... data) {
				// Register components
				[[maybe_unused]] const ComponentMetaData* typesToAdd[] = {
						g_ComponentCache.GetComponentMetaType<TComponent>()...};

				// Entity
				{
					const auto lastIndex = m_data.size();
					m_data.resize(m_data.size() + sizeof(TEntity));

					utils::unaligned_ref<TEntity> to(&m_data[lastIndex]);
					to = entity;
				}
				// Components
				{
					// Component count
					constexpr auto NComponents = (uint8_t)sizeof...(TComponent);
					m_data.push_back(NComponents);

					// Data size
					auto lastIndex = (uint32_t)m_data.size();

					constexpr auto ComponentsSize = (sizeof(TComponent) + ...);
					constexpr auto ComponentTypeIdxSize = sizeof(uint32_t);
					m_data.resize(m_data.size() + ComponentsSize * ComponentTypeIdxSize);

					// Component data
					(SetComponent_Internal<TComponent>(
							 lastIndex, std::forward<TComponent>(data)),
					 ...);
				}
			}

			template <typename... TComponent>
			void RemoveComponent_Internal(Entity entity) {
				// Entity
				{
					const auto lastIndex = m_data.size();
					m_data.resize(m_data.size() + sizeof(Entity));

					utils::unaligned_ref<Entity> to(&m_data[lastIndex]);
					to = entity;
				}
				// Components
				{
					const ComponentMetaData* typesToRemove[] = {
							g_ComponentCache.GetComponentMetaType<TComponent>()...};

					// Component count
					constexpr auto NComponents = (uint8_t)sizeof...(TComponent);
					m_data.push_back(NComponents);

					// Component meta data
					const auto lastIndex = m_data.size();
					m_data.resize(
							m_data.size() + sizeof(ComponentMetaData*) * NComponents);
					for (uint8_t i = 0; i < NComponents; i++) {
						utils::unaligned_ref<ComponentMetaData*> to(&m_data[lastIndex]);
						to = typesToRemove[i];
					}
				}
			}

			/*!
			Requests a new entity to be created from archetype
			\return Entity that will be created. The id is not usable right away. It
			will be filled with proper data after Commit()
			*/
			[[nodiscard]] TempEntity CreateEntity(Archetype& archetype) {
				m_data.push_back(CREATE_ENTITY_FROM_ARCHETYPE);
				const auto archetypeSize =
						sizeof(void*); // we'll serialize just the pointer
				const auto lastIndex = m_data.size();
				m_data.resize(m_data.size() + archetypeSize);

				utils::unaligned_ref<uintptr_t> to(&m_data[lastIndex]);
				to = (uintptr_t)&archetype;

				return {m_entities++};
			}

		public:
			CommandBuffer() {
				m_data.reserve(256);
				m_entities = 0;
			}

			CommandBuffer(CommandBuffer&&) = delete;
			CommandBuffer(const CommandBuffer&) = delete;
			CommandBuffer& operator=(CommandBuffer&&) = delete;
			CommandBuffer& operator=(const CommandBuffer&) = delete;

			/*!
			Requests a new entity to be created
			\return Entity that will be created. The id is not usable right away. It
			will be filled with proper data after Commit()
			*/
			[[nodiscard]] TempEntity CreateEntity() {
				m_data.push_back(CREATE_ENTITY);
				return {m_entities++};
			}

			/*!
			Requests a new entity to be created from query
			\return Entity that will be created. The id is not usable right away. It
			will be filled with proper data after Commit()
			*/
			[[nodiscard]] TempEntity CreateEntity(CreationQuery& query) {
				m_data.push_back(CREATE_ENTITY_FROM_QUERY);
				const auto querySize =
						sizeof(query); // we'll serialize the query by making a copy of it
				const auto lastIndex = m_data.size();
				m_data.resize(m_data.size() + querySize);

				utils::unaligned_ref<CreationQuery> to(&m_data[lastIndex]);
				to = query;

				return {m_entities++};
			}

			/*!
			Requests a new entity to be created by cloning an already existing
			entity \return Entity that will be created. The id is not usable right
			away. It will be filled with proper data after Commit()
			*/
			[[nodiscard]] TempEntity CreateEntity(Entity entityFrom) {
				m_data.push_back(CREATE_ENTITY_FROM_ENTITY);
				const auto entitySize = sizeof(entityFrom);
				const auto lastIndex = m_data.size();
				m_data.resize(m_data.size() + entitySize);

				utils::unaligned_ref<Entity> to(&m_data[lastIndex]);
				to = entityFrom;

				return {m_entities++};
			}

			/*!
			Requests a component to be added to entity.

			\return True if component could be added (e.g. maximum component count
			on the archetype not exceeded). False otherwise.
			*/
			template <typename... TComponent>
			bool AddComponent(Entity entity) {
				VerifyComponents<TComponent...>();
				if (!VerityArchetypeComponentCount((uint32_t)sizeof...(TComponent)))
					return false;

				m_data.push_back(ADD_COMPONENT);
				m_data.push_back(ComponentType::CT_Generic);
				AddComponent_Internal<Entity, TComponent...>(entity);
				return true;
			}

			/*!
			Requests a component to be added to temporary entity.

			\return True if component could be added (e.g. maximum component count
			on the archetype not exceeded). False otherwise.
			*/
			template <typename... TComponent>
			bool AddComponent(TempEntity entity) {
				VerifyComponents<TComponent...>();
				if (!VerityArchetypeComponentCount((uint32_t)sizeof...(TComponent)))
					return false;

				m_data.push_back(ADD_COMPONENT_TO_TEMPENTITY);
				m_data.push_back(ComponentType::CT_Generic);
				AddComponent_Internal<TempEntity, TComponent...>(entity);
				return true;
			}

			/*!
			Requests a chunk component to be added to entity.

			\return True if component could be added (e.g. maximum component count
			on the archetype not exceeded). False otherwise.
			*/
			template <typename... TComponent>
			bool AddChunkComponent(Entity entity) {
				VerifyComponents<TComponent...>();
				if (!VerityArchetypeComponentCount((uint32_t)sizeof...(TComponent)))
					return false;

				m_data.push_back(ADD_COMPONENT);
				m_data.push_back(ComponentType::CT_Chunk);
				AddComponent_Internal<Entity, TComponent...>(entity);
				return true;
			}

			/*!
			Requests a chunk component to be added to temp entity.

			\return True if component could be added (e.g. maximum component count
			on the archetype not exceeded). False otherwise.
			*/
			template <typename... TComponent>
			bool AddChunkComponent(TempEntity entity) {
				VerifyComponents<TComponent...>();
				if (!VerityArchetypeComponentCount((uint32_t)sizeof...(TComponent)))
					return false;

				m_data.push_back(ADD_COMPONENT);
				m_data.push_back(ComponentType::CT_Chunk);
				AddComponent_Internal<TempEntity, TComponent...>(entity);
				return true;
			}

			/*!
			Requests component data to be set to given values for a given entity.

			\warning Just like World::SetComponent, this method expects the
			given component types to exist. Undefined behavior otherwise.
			*/
			template <typename... TComponent>
			void SetComponent(Entity entity, TComponent&&... data) {
				VerifyComponents<TComponent...>();

				m_data.push_back(SET_COMPONENT);
				m_data.push_back(ComponentType::CT_Generic);
				SetComponent_Internal(entity, std::forward<TComponent>(data)...);
			}

			/*!
			Requests component data to be set to given values for a given temp
			entity.

			\warning Just like World::SetComponent, this method expects the
			given component types to exist. Undefined behavior otherwise.
			*/
			template <typename... TComponent>
			void SetComponent(TempEntity entity, TComponent&&... data) {
				VerifyComponents<TComponent...>();

				m_data.push_back(SET_COMPONENT_FOR_TEMPENTITY);
				m_data.push_back(ComponentType::CT_Generic);
				SetComponent_Internal(entity, std::forward<TComponent>(data)...);
			}

			/*!
			Requests chunk component data to be set to given values for a given
			entity's chunk.

			\warning Just like World::SetChunkComponent, this method expects the
			given component types to exist. Undefined behavior otherwise.
			*/
			template <typename... TComponent>
			void SetChunkComponent(Entity entity, TComponent&&... data) {
				VerifyComponents<TComponent...>();

				m_data.push_back(SET_COMPONENT);
				m_data.push_back(ComponentType::CT_Chunk);
				SetComponent_Internal(entity, std::forward<TComponent>(data)...);
			}

			/*!
			Requests chunk component data to be set to given values for a given
			entity's chunk.

			\warning Just like World::SetChunkComponent, this method expects the
			given component types to exist. Undefined behavior otherwise.
			*/
			template <typename... TComponent>
			void SetChunkComponent(TempEntity entity, TComponent&&... data) {
				VerifyComponents<TComponent...>();

				m_data.push_back(SET_COMPONENT_FOR_TEMPENTITY);
				m_data.push_back(ComponentType::CT_Chunk);
				SetComponent_Internal(entity, std::forward<TComponent>(data)...);
			}

			/*!
			Requests removal of a component from entity
			*/
			template <typename... TComponent>
			void RemoveComponent(Entity entity) {
				m_data.push_back(REMOVE_COMPONENT);
				m_data.push_back(ComponentType::CT_Generic);
				RemoveComponent_Internal<TComponent...>(entity);
			}

			/*!
			Requests removal of a chunk component from entity
			*/
			template <typename... TComponent>
			void RemoveChunkComponent(Entity entity) {
				m_data.push_back(REMOVE_COMPONENT);
				m_data.push_back(ComponentType::CT_Chunk);
				RemoveComponent_Internal<TComponent...>(entity);
			}

			/*!
			Commits all queued changes.
			*/
			void Commit(World* world) {
				std::unordered_map<uint32_t, Entity> entityMap;
				uint32_t entities = 0;

				// Extract data from the buffer
				for (auto i = 0U; i < m_data.size();) {
					const auto cmd = m_data[i++];
					switch (cmd) {
						case CREATE_ENTITY: {
							entityMap.insert({entities++, world->CreateEntity()});
						} break;
						case CREATE_ENTITY_FROM_ARCHETYPE: {
							uintptr_t ptr = (uintptr_t&)m_data[i];
							Archetype* archetype = (Archetype*)ptr;
							i += sizeof(void*);
							entityMap.insert({entities++, world->CreateEntity(*archetype)});
						} break;
						case CREATE_ENTITY_FROM_QUERY: {
							auto& query = (CreationQuery&)m_data[i];
							i += sizeof(CreationQuery);
							entityMap.insert({entities++, world->CreateEntity(query)});
						} break;
						case CREATE_ENTITY_FROM_ENTITY: {
							Entity entityFrom = (Entity&)m_data[i];
							i += sizeof(Entity);
							entityMap.insert({entities++, world->CreateEntity(entityFrom)});
						} break;
						case ADD_COMPONENT: {
							// // Type
							// ComponentType componentType = (ComponentType)m_data[i];
							// i += sizeof(ComponentType);
							// // Entity
							// Entity entity = (Entity&)m_data[i];
							// i += sizeof(Entity);

							// // Component count
							// uint8_t componentCount = m_data[i++];

							// // Components
							// auto newMetatypes = (const ComponentMetaData**)alloca(
							// 		sizeof(ComponentMetaData) * componentCount);
							// for (uint8_t j = 0; j < componentCount; ++j) {
							// 	const auto metaType = *(const ComponentMetaData**)&m_data[i];
							// 	newMetatypes[j] = metaType;
							// 	i += sizeof(const ComponentMetaData*);
							// }
							// world->AddComponent_Internal(
							// 		componentType, entity,
							// 		{newMetatypes, (uintptr_t)componentCount});

							// uint32_t indexInChunk;
							// auto chunk = world->GetEntityChunk(entity, indexInChunk);
							// GAIA_ASSERT(chunk != nullptr);

							// if (componentType == ComponentType::CT_Chunk)
							// 	indexInChunk = 0;

							// for (uint8_t j = 0; j < componentCount; ++j) {
							// 	const auto metaType = newMetatypes[j];
							// 	auto pComponentData = chunk->SetComponent_Internal(
							// 			componentType, indexInChunk, metaType);
							// 	memset(pComponentData, 0, metaType->size);
							// }
						} break;
						case ADD_COMPONENT_TO_TEMPENTITY: {
							// // Type
							// ComponentType componentType = (ComponentType)m_data[i];
							// i += sizeof(ComponentType);
							// // Entity
							// Entity e = (Entity&)m_data[i];
							// i += sizeof(Entity);

							// // For delayed entities we have to do a look in our map
							// // of temporaries and find a link there
							// const auto it = entityMap.find(e.id());
							// // Link has to exist!
							// GAIA_ASSERT(it != entityMap.end());

							// Entity entity = it->second;

							// // Component count
							// uint8_t componentCount = m_data[i++];

							// // Components
							// auto newMetatypes = (const ComponentMetaData**)alloca(
							// 		sizeof(ComponentMetaData) * componentCount);
							// for (uint8_t j = 0; j < componentCount; ++j) {
							// 	const auto metaType = *(const ComponentMetaData**)&m_data[i];
							// 	newMetatypes[j] = metaType;
							// 	i += sizeof(const ComponentMetaData*);
							// }
							// world->AddComponent_Internal(
							// 		componentType, entity,
							// 		{newMetatypes, (uintptr_t)componentCount});

							// uint32_t indexInChunk;
							// auto chunk = world->GetEntityChunk(entity, indexInChunk);
							// GAIA_ASSERT(chunk != nullptr);

							// if (componentType == ComponentType::CT_Chunk)
							// 	indexInChunk = 0;

							// for (uint8_t j = 0; j < componentCount; ++j) {
							// 	const auto metaType = newMetatypes[j];
							// 	auto pComponentData = chunk->SetComponent_Internal(
							// 			componentType, indexInChunk, metaType);
							// 	memset(pComponentData, 0, metaType->size);
							// }
						} break;
						case SET_COMPONENT: {
							// // Type
							// ComponentType componentType = (ComponentType)m_data[i];
							// i += sizeof(ComponentType);
							// // Entity
							// Entity entity = (Entity&)m_data[i];
							// i += sizeof(Entity);

							// const auto& entityContainer = world->m_entities[entity.id()];
							// auto chunk = entityContainer.chunk;
							// const auto indexInChunk = componentType ==
							// ComponentType::CT_Chunk 															? 0
							// : entityContainer.idx;

							// // Component count
							// const uint8_t componentCount = m_data[i++];

							// // Components
							// for (uint8_t j = 0; j < componentCount; ++j) {
							// 	const uint32_t typeIdx = *(uint32_t*)&m_data[i];
							// 	const auto* metaType =
							// 			g_ComponentCache.GetComponentMetaTypeFromIdx(typeIdx);
							// 	i += sizeof(typeIdx);

							// 	memcpy(
							// 			chunk->SetComponent_Internal(
							// 					componentType, indexInChunk, metaType),
							// 			(const void*)&m_data[i], metaType->size);
							// 	i += metaType->size;
							// }
						} break;
						case SET_COMPONENT_FOR_TEMPENTITY: {
							// // Type
							// ComponentType componentType = (ComponentType)m_data[i];
							// i += sizeof(ComponentType);
							// // Entity
							// Entity e = (Entity&)m_data[i];
							// i += sizeof(Entity);

							// // For delayed entities we have to do a look in our map
							// // of temporaries and find a link there
							// const auto it = entityMap.find(e.id());
							// // Link has to exist!
							// GAIA_ASSERT(it != entityMap.end());

							// Entity entity = it->second;

							// const auto& entityContainer = world->m_entities[entity.id()];
							// auto chunk = entityContainer.chunk;
							// const auto indexInChunk = componentType ==
							// ComponentType::CT_Chunk 															? 0
							// : entityContainer.idx;

							// // Component count
							// const uint8_t componentCount = m_data[i++];

							// // Components
							// for (uint8_t j = 0; j < componentCount; ++j) {
							// 	const uint32_t typeIdx = *(uint32_t*)&m_data[i];
							// 	const auto* metaType =
							// 			g_ComponentCache.GetComponentMetaTypeFromIdx(typeIdx);
							// 	i += sizeof(typeIdx);

							// 	memcpy(
							// 			chunk->SetComponent_Internal(
							// 					componentType, indexInChunk, metaType),
							// 			(const void*)&m_data[i], metaType->size);
							// 	i += metaType->size;
							// }
						} break;
						case REMOVE_COMPONENT: {
							// Type
							ComponentType componentType = (ComponentType&)m_data[i];
							i += sizeof(ComponentType);
							// Entity
							Entity e = (Entity&)m_data[i];
							i += sizeof(Entity);

							// Component count
							uint8_t componentCount = m_data[i++];

							// Components
							auto newMetatypes = (const ComponentMetaData**)alloca(
									sizeof(ComponentMetaData) * componentCount);
							for (uint8_t j = 0; j < componentCount; ++j) {
								auto metaType = *(ComponentMetaData**)&m_data[i];
								newMetatypes[j] = metaType;
								i += sizeof(const ComponentMetaData*);
							}
							world->RemoveComponent_Internal(
									componentType, e, {newMetatypes, (uintptr_t)componentCount});
						} break;
					}
				}

				m_entities = 0;
				m_data.clear();
			}
		};
	} // namespace ecs
} // namespace gaia