#pragma once
#include <inttypes.h>
#include <vector>

#include "archetype.h"
#include "component.h"
#include "fwd.h"
#include "world.h"

namespace gaia {
	namespace ecs {
		/*!
		Buffer for deferred execution of some operations on entities.

		Adding and removing components and entities inside World::ForEach or
		World::ForEachChunk can result in changes of archetypes or chunk structure.
		This would lead to undefined behavior. Therefore, such operations have to be
		executed after the loop is done.
		*/
		class EntityCommandBuffer final {
			enum EntityCommandBufferCmd : uint8_t {
				CREATE_ENTITY,
				CREATE_ENTITY_FROM_ARCHETYPE,
				CREATE_ENTITY_FROM_QUERY,
				CREATE_ENTITY_FROM_ENTITY,
				ADD_COMPONENT,
				SET_COMPONENT,
				REMOVE_COMPONENT,
			};

			friend class World;

			std::vector<uint8_t> m_data = std::vector<uint8_t>(256);
			uint32_t m_entities = 0;

			template <typename... TComponent>
			void AddComponent_Internal(Entity entity) {
				// Entity
				{
					const auto lastIndex = m_data.size();
					m_data.resize(m_data.size() + sizeof(Entity));
					*(Entity*)&m_data[lastIndex] = entity;
				}
				// Components
				{
					const ComponentMetaData* typesToAdd[] = {
							GetOrCreateComponentMetaType<TComponent>()...};

					// Component count
					constexpr auto componentCount = (uint8_t)sizeof...(TComponent);
					m_data.push_back(componentCount);

					// Component meta data
					const auto lastIndex = m_data.size();
					m_data.resize(
							m_data.size() + sizeof(ComponentMetaData*) * componentCount);
					for (uint8_t i = 0; i < componentCount; i++)
						*((ComponentMetaData**)&m_data[lastIndex] + i) =
								(ComponentMetaData*)typesToAdd[i];
				}
			}

			template <typename T>
			void SetComponentData_Internal(uint32_t& index, T&& data) {
				using TComponent = std::decay_t<T>;

				auto* metaData = GetComponentMetaType<TComponent>();
				if (metaData == nullptr)
					return;

				// Meta type
				*(ComponentMetaData**)&m_data[index] = (ComponentMetaData*)metaData;

				// Component data
				*(TComponent*)&m_data[index + sizeof(void*)] =
						std::forward<TComponent>(data);

				index += sizeof(void*) + metaData->size;
			}

			template <typename... TComponent>
			void
			SetComponent_Internal(Entity entity, ECS_SMART_ARG(TComponent)... data) {
				// Verify components
				const ComponentMetaData* typesToAdd[] = {
						GetComponentMetaType<TComponent>()...};
				uint32_t validComponents = 0;
				uint32_t validSize = 0;
					for (const auto type : typesToAdd) {
						if (type == nullptr)
							continue;
						++validComponents;
						validSize +=
								type->size + sizeof(void*); // meta data pointer + data size
					}

				// Entity
				{
					const auto lastIndex = m_data.size();
					m_data.resize(m_data.size() + sizeof(Entity));
					*(Entity*)&m_data[lastIndex] = entity;
				}
				// Components
				{
					// Component count
					m_data.push_back(validComponents);

					// Data size
					uint32_t lastIndex = m_data.size();
					m_data.resize(m_data.size() + validSize);

					// Component data
					(SetComponentData_Internal<TComponent>(
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
					*(Entity*)&m_data[lastIndex] = entity;
				}
				// Components
				{
					const ComponentMetaData* typesToRemove[] = {
							GetComponentMetaType<TComponent>()...};

					// Component count
					constexpr auto componentCount = (uint8_t)sizeof...(TComponent);
					uint8_t validComponentCount = 0;
						for (uint8_t i = 0; i < componentCount; i++) {
							if (typesToRemove[i] != nullptr)
								++validComponentCount;
						}
					m_data.push_back(validComponentCount);

					// Component meta data
					const auto lastIndex = m_data.size();
					m_data.resize(
							m_data.size() + sizeof(ComponentMetaData*) * validComponentCount);
						for (uint8_t i = 0; i < validComponentCount; i++) {
							if (typesToRemove[i] != nullptr)
								*((ComponentMetaData**)&m_data[lastIndex] + i) =
										(ComponentMetaData*)typesToRemove[i];
						}
				}
			}

		public:
			/*!
			Requests a new entity to be created
			\return Entity that will be created. The id is not usable right away. It
			will be filled with proper data after Commit()
			*/
			[[nodiscard]] Entity CreateEntity() {
				m_data.push_back(CREATE_ENTITY);
				return {m_entities++, Entity::GenMask};
			}

			/*!
			Requests a new entity to be created from archetype
			\return Entity that will be created. The id is not usable right away. It
			will be filled with proper data after Commit()
			*/
			[[nodiscard]] Entity CreateEntity(Archetype& archetype) {
				m_data.push_back(CREATE_ENTITY_FROM_ARCHETYPE);
				const auto archetypeSize =
						sizeof(void*); // we'll serialize just the pointer
				const auto lastIndex = m_data.size();
				m_data.resize(m_data.size() + archetypeSize);
				*((uintptr_t*)&m_data[lastIndex]) = (uintptr_t)&archetype;
				return {m_entities++, Entity::GenMask};
			}

			/*!
			Requests a new entity to be created from query
			\return Entity that will be created. The id is not usable right away. It
			will be filled with proper data after Commit()
			*/
			[[nodiscard]] Entity CreateEntity(CreationQuery& query) {
				m_data.push_back(CREATE_ENTITY_FROM_QUERY);
				const auto querySize =
						sizeof(query); // we'll serialize the query by making a copy of it
				const auto lastIndex = m_data.size();
				m_data.resize(m_data.size() + querySize);
				*((CreationQuery*)&m_data[lastIndex]) = query;
				return {m_entities++, Entity::GenMask};
			}

			/*!
			Requests a new entity to be created by cloning an already existing entity
			\return Entity that will be created. The id is not usable right away. It
			will be filled with proper data after Commit()
			*/
			[[nodiscard]] Entity CreateEntity(Entity entityFrom) {
				m_data.push_back(CREATE_ENTITY_FROM_ENTITY);
				const auto entitySize = sizeof(entityFrom);
				const auto lastIndex = m_data.size();
				m_data.resize(m_data.size() + entitySize);
				*((Entity*)&m_data[lastIndex]) = entityFrom;
				return {m_entities++, Entity::GenMask};
			}

			/*!
			Requests a component to be added to entity
			*/
			template <typename... TComponent> void AddComponent(Entity entity) {
				VerifyComponents<TComponent...>();
				if (!VerifyMaxComponentCountPerArchetype(
								(uint32_t)sizeof...(TComponent)))
					return;

				m_data.push_back(ADD_COMPONENT);
				m_data.push_back(ComponentType::CT_Generic);
				AddComponent_Internal<TComponent...>(entity);
			}

			/*!
			Requests a chunk component to be added to entity
			*/
			template <typename... TComponent> void AddChunkComponent(Entity entity) {
				VerifyComponents<TComponent...>();
				if (!VerifyMaxComponentCountPerArchetype(
								(uint32_t)sizeof...(TComponent)))
					return;

				m_data.push_back(ADD_COMPONENT);
				m_data.push_back(ComponentType::CT_Chunk);
				AddComponent_Internal<TComponent...>(entity);
			}

			/*!
			Requests component data to be set to given values
			*/
			template <typename... TComponent>
			void SetComponentData(Entity entity, ECS_SMART_ARG(TComponent)... data) {
				VerifyComponents<TComponent...>();

				m_data.push_back(SET_COMPONENT);
				m_data.push_back(ComponentType::CT_Generic);
				SetComponent_Internal<TComponent...>(
						entity, std::forward<TComponent>(data)...);
			}

			/*!
			Requests chunk component data to be set to given values
			*/
			template <typename... TComponent>
			void
			SetChunkComponentData(Entity entity, ECS_SMART_ARG(TComponent)... data) {
				VerifyComponents<TComponent...>();

				m_data.push_back(SET_COMPONENT);
				m_data.push_back(ComponentType::CT_Chunk);
				SetComponent_Internal<TComponent...>(
						entity, std::forward<TComponent>(data)...);
			}

			/*!
			Requests removal of a component from entity
			*/
			template <typename... TComponent> void RemoveComponent(Entity entity) {
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
					for (auto i = 0; i < m_data.size();) {
						const auto type = m_data[i++];
							switch (type) {
									case CREATE_ENTITY: {
										entityMap.insert({entities++, world->CreateEntity()});
									} break;
									case CREATE_ENTITY_FROM_ARCHETYPE: {
										uintptr_t ptr = (uintptr_t&)m_data[i];
										Archetype* archetype = (Archetype*)ptr;
										i += sizeof(void*);
										entityMap.insert(
												{entities++, world->CreateEntity(*archetype)});
									} break;
									case CREATE_ENTITY_FROM_QUERY: {
										auto& query = (CreationQuery&)m_data[i];
										i += sizeof(CreationQuery);
										entityMap.insert({entities++, world->CreateEntity(query)});
									} break;
									case CREATE_ENTITY_FROM_ENTITY: {
										Entity entityFrom = (Entity&)m_data[i];
										i += sizeof(Entity);
										entityMap.insert(
												{entities++, world->CreateEntity(entityFrom)});
									} break;
									case ADD_COMPONENT: {
										// Type
										ComponentType type = (ComponentType)m_data[i];
										i += sizeof(ComponentType);
										// Entity
										Entity e = (Entity&)m_data[i];
										i += sizeof(Entity);

										Entity* entity = &e;
											if (e.gen() == Entity::GenMask) {
												// For delayed entities we have to do a look in our map
												// of temporaries and find a link there
												const auto it = entityMap.find(e.id());
												// Link has to exist!
												assert(it != entityMap.end());

												entity = &it->second;
										}

										// Component count
										uint8_t componentCount = m_data[i++];

										// Components
										auto newMetatypes = (const ComponentMetaData**)alloca(
												sizeof(ComponentMetaData) * componentCount);
											for (uint8_t j = 0; j < componentCount; ++j) {
												auto type = *(const ComponentMetaData**)&m_data[i];
												newMetatypes[j] = type;
												i += sizeof(const ComponentMetaData*);
											}
										world->AddComponent_Internal(
												type, *entity,
												tcb::span(newMetatypes, (uintptr_t)componentCount));
									} break;
									case SET_COMPONENT: {
										// Type
										ComponentType componentType = (ComponentType)m_data[i];
										i += sizeof(ComponentType);
										// Entity
										Entity e = (Entity&)m_data[i];
										i += sizeof(Entity);

										Entity* entity = &e;
											if (e.gen() == Entity::GenMask) {
												// For delayed entities we have to do a look in our map
												// of temporaries and find a link there
												const auto it = entityMap.find(e.id());
												// Link has to exist!
												assert(it != entityMap.end());

												entity = &it->second;
										}

										const auto& entityContainer =
												world->m_entities[entity->id()];
										auto chunk = entityContainer.chunk;
										const auto& archetype = chunk->header.owner;

										// Component count
										uint8_t componentCount = m_data[i++];

											// Components
											for (uint8_t j = 0; j < componentCount; ++j) {
												const auto type =
														*(const ComponentMetaData**)&m_data[i];
												uint32_t index = 0;
													for (const auto& component :
															 archetype.componentList[componentType]) {
														if (component.type != type)
															continue;
														index = component.offset +
																		entityContainer.idx * type->size;
													}
												assert(index > 0);

												i += sizeof(const ComponentMetaData*);
												memcpy(
														chunk->GetComponent_Internal(
																componentType, index, type),
														(const void*)&m_data[i], type->size);
												i += type->size;
											}
									} break;
									case REMOVE_COMPONENT: {
										// Type
										ComponentType type = (ComponentType&)m_data[i];
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
												auto type = *(ComponentMetaData**)&m_data[i];
												newMetatypes[j] = type;
												i += sizeof(const ComponentMetaData*);
											}
										world->RemoveComponent_Internal(
												type, e,
												tcb::span(newMetatypes, (uintptr_t)componentCount));
									} break;
							}
					}

				m_entities = 0;
				m_data.clear();
			}
		};
	} // namespace ecs
} // namespace gaia