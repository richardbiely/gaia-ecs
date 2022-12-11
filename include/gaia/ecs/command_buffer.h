#pragma once
#include <cinttypes>

#include "../config/config.h"
#include "../containers/map.h"
#include "../containers/sarray_ext.h"
#include "../utils/mem.h"
#include "archetype.h"
#include "common.h"
#include "component.h"
#include "component_cache.h"
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

		Adding and removing components and entities inside World::ForEach or can result
		in changes of archetypes or chunk structure. This would lead to undefined behavior.
		Therefore, such operations have to be executed after the loop is done.
		*/
		class CommandBuffer final {
			enum CommandBufferCmd : uint8_t {
				CREATE_ENTITY,
				CREATE_ENTITY_FROM_ARCHETYPE,
				CREATE_ENTITY_FROM_ENTITY,
				DELETE_ENTITY,
				ADD_COMPONENT,
				ADD_COMPONENT_DATA,
				ADD_COMPONENT_TO_TEMPENTITY,
				ADD_COMPONENT_TO_TEMPENTITY_DATA,
				SET_COMPONENT,
				SET_COMPONENT_FOR_TEMPENTITY,
				REMOVE_COMPONENT
			};

			friend class World;

			World& m_world;
			containers::darray<uint8_t> m_data;
			uint32_t m_entities;

			template <typename TEntity, typename T>
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
					const auto* infoToAdd = GetComponentCacheRW().GetOrCreateComponentInfo<T>();

					// Component info
					auto lastIndex = m_data.size();
					m_data.resize(m_data.size() + sizeof(uint32_t));

					utils::unaligned_ref<uint32_t> to(&m_data[lastIndex]);
					to = infoToAdd->infoIndex;

					lastIndex += sizeof(uint32_t);
				}
			}

			template <typename T>
			void SetComponentFinal_Internal(uint32_t& index, T&& data) {
				using U = std::decay_t<T>;

				// Component info
				{
					utils::unaligned_ref<uint32_t> mem((void*)&m_data[index]);
					mem = utils::type_info::index<U>();
				}

				// Component data
				{
					utils::unaligned_ref<U> mem((void*)&m_data[index + sizeof(uint32_t)]);
					mem = std::forward<U>(data);
				}

				index += (uint32_t)(sizeof(uint32_t) + sizeof(U));
			}

			template <typename T>
			void SetComponentNoEntityNoSize_Internal(T&& data) {
				// Data size
				auto lastIndex = (uint32_t)m_data.size();

				constexpr auto ComponentsSize = sizeof(T);
				constexpr auto ComponentTypeIdxSize = sizeof(uint32_t);
				constexpr auto AddSize = ComponentsSize + ComponentTypeIdxSize;
				m_data.resize(m_data.size() + AddSize);

				// Component data
				this->SetComponentFinal_Internal<T>(lastIndex, std::forward<T>(data));
			}

			template <typename T>
			void SetComponentNoEntity_Internal(T&& data) {
				// Register components
				(void)GetComponentCacheRW().GetOrCreateComponentInfo<T>();

				// Data
				SetComponentNoEntityNoSize_Internal(std::forward<T>(data));
			}

			template <typename TEntity, typename T>
			void SetComponent_Internal(TEntity entity, T&& data) {
				// Entity
				{
					const auto lastIndex = m_data.size();
					m_data.resize(m_data.size() + sizeof(TEntity));

					utils::unaligned_ref<TEntity> to(&m_data[lastIndex]);
					to = entity;
				}

				// Components
				SetComponentNoEntity_Internal(std::forward<T>(data));
			}

			template <typename T>
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
					const auto* typeToRemove = GetComponentCache().GetComponentInfo<T>();
					GAIA_ASSERT(typeToRemove != nullptr);

					// Component info
					auto lastIndex = m_data.size();
					m_data.resize(m_data.size() + sizeof(uint32_t));

					utils::unaligned_ref<uint32_t> to(&m_data[lastIndex]);
					to = typeToRemove->infoIndex;

					lastIndex += sizeof(uint32_t);
				}
			}

			/*!
			Requests a new entity to be created from archetype
			\return Entity that will be created. The id is not usable right away. It
			will be filled with proper data after Commit()
			*/
			[[nodiscard]] TempEntity CreateEntity(Archetype& archetype) {
				m_data.push_back(CREATE_ENTITY_FROM_ARCHETYPE);
				const auto archetypeSize = sizeof(void*); // we'll serialize just the pointer
				const auto lastIndex = m_data.size();
				m_data.resize(m_data.size() + archetypeSize);

				utils::unaligned_ref<uintptr_t> to(&m_data[lastIndex]);
				to = (uintptr_t)&archetype;

				return {m_entities++};
			}

		public:
			CommandBuffer(World& world): m_world(world), m_entities(0) {
				m_data.reserve(256);
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
			Requests an existing \param entity to be removed.
			*/
			void DeleteEntity(Entity entity) {
				m_data.push_back(DELETE_ENTITY);
				const auto entitySize = sizeof(entity);
				const auto lastIndex = m_data.size();
				m_data.resize(m_data.size() + entitySize);

				utils::unaligned_ref<Entity> to(&m_data[lastIndex]);
				to = entity;
			}

			/*!
			Requests a component to be added to entity.

			\return True if component could be added (e.g. maximum component count
			on the archetype not exceeded). False otherwise.
			*/
			template <typename T>
			bool AddComponent(Entity entity) {
				using U = typename DeduceComponent<T>::Type;
				VerifyComponent<U>();

				m_data.push_back(ADD_COMPONENT);
				if constexpr (IsGenericComponent<T>::value)
					m_data.push_back(ComponentType::CT_Generic);
				else
					m_data.push_back(ComponentType::CT_Chunk);
				AddComponent_Internal<Entity, U>(entity);
				return true;
			}

			/*!
			Requests a component to be added to temporary entity.

			\return True if component could be added (e.g. maximum component count
			on the archetype not exceeded). False otherwise.
			*/
			template <typename T>
			bool AddComponent(TempEntity entity) {
				using U = typename DeduceComponent<T>::Type;
				VerifyComponent<U>();

				m_data.push_back(ADD_COMPONENT_TO_TEMPENTITY);
				if constexpr (IsGenericComponent<T>::value)
					m_data.push_back(ComponentType::CT_Generic);
				else
					m_data.push_back(ComponentType::CT_Chunk);
				AddComponent_Internal<TempEntity, U>(entity);
				return true;
			}

			/*!
			Requests a component to be added to entity.

			\return True if component could be added (e.g. maximum component count
			on the archetype not exceeded). False otherwise.
			*/
			template <typename T>
			bool AddComponent(Entity entity, T&& data) {
				using U = typename DeduceComponent<T>::Type;
				VerifyComponent<U>();

				m_data.push_back(ADD_COMPONENT_DATA);
				if constexpr (IsGenericComponent<T>::value)
					m_data.push_back(ComponentType::CT_Generic);
				else
					m_data.push_back(ComponentType::CT_Chunk);
				AddComponent_Internal<Entity, U>(entity);
				SetComponentNoEntityNoSize_Internal(std::forward<U>(data));
				return true;
			}

			/*!
			Requests a component to be added to temporary entity.

			\return True if component could be added (e.g. maximum component count
			on the archetype not exceeded). False otherwise.
			*/
			template <typename T>
			bool AddComponent(TempEntity entity, T&& data) {
				using U = typename DeduceComponent<T>::Type;
				VerifyComponent<U>();

				m_data.push_back(ADD_COMPONENT_TO_TEMPENTITY_DATA);
				if constexpr (IsGenericComponent<T>::value)
					m_data.push_back(ComponentType::CT_Generic);
				else
					m_data.push_back(ComponentType::CT_Chunk);
				AddComponent_Internal<TempEntity, U>(entity);
				SetComponentNoEntityNoSize_Internal(std::forward<U>(data));
				return true;
			}

			/*!
			Requests component data to be set to given values for a given entity.

			\warning Just like World::SetComponent, this function expects the
			given component infos to exist. Undefined behavior otherwise.
			*/
			template <typename T>
			void SetComponent(Entity entity, T&& data) {
				using U = typename DeduceComponent<T>::Type;
				VerifyComponent<U>();

				m_data.push_back(SET_COMPONENT);
				if constexpr (IsGenericComponent<T>::value)
					m_data.push_back(ComponentType::CT_Generic);
				else
					m_data.push_back(ComponentType::CT_Chunk);
				SetComponent_Internal(entity, std::forward<U>(data));
			}

			/*!
			Requests component data to be set to given values for a given temp
			entity.

			\warning Just like World::SetComponent, this function expects the
			given component infos to exist. Undefined behavior otherwise.
			*/
			template <typename T>
			void SetComponent(TempEntity entity, T&& data) {
				using U = typename DeduceComponent<T>::Type;
				VerifyComponent<U>();

				m_data.push_back(SET_COMPONENT_FOR_TEMPENTITY);
				if constexpr (IsGenericComponent<T>::value)
					m_data.push_back(ComponentType::CT_Generic);
				else
					m_data.push_back(ComponentType::CT_Chunk);
				SetComponent_Internal(entity, std::forward<U>(data));
			}

			/*!
			Requests removal of a component from entity
			*/
			template <typename T>
			void RemoveComponent(Entity entity) {
				using U = typename DeduceComponent<T>::Type;
				VerifyComponent<U>();

				m_data.push_back(REMOVE_COMPONENT);
				if constexpr (IsGenericComponent<T>::value)
					m_data.push_back(ComponentType::CT_Generic);
				else
					m_data.push_back(ComponentType::CT_Chunk);
				RemoveComponent_Internal<U>(entity);
			}

			/*!
			Commits all queued changes.
			*/
			void Commit() {
				containers::map<uint32_t, Entity> entityMap;
				uint32_t entities = 0;

				// Extract data from the buffer
				for (size_t i = 0; i < m_data.size();) {
					const auto cmd = m_data[i++];
					switch (cmd) {
						case CREATE_ENTITY: {
							[[maybe_unused]] const auto res = entityMap.emplace(entities++, m_world.CreateEntity());
							GAIA_ASSERT(res.second);
						} break;
						case CREATE_ENTITY_FROM_ARCHETYPE: {
							uintptr_t ptr = utils::unaligned_ref<uintptr_t>((void*)&m_data[i]);
							Archetype* archetype = (Archetype*)ptr;
							i += sizeof(void*);
							[[maybe_unused]] const auto res = entityMap.emplace(entities++, m_world.CreateEntity(*archetype));
							GAIA_ASSERT(res.second);
						} break;
						case CREATE_ENTITY_FROM_ENTITY: {
							Entity entityFrom = utils::unaligned_ref<Entity>((void*)&m_data[i]);
							i += sizeof(Entity);
							[[maybe_unused]] const auto res = entityMap.emplace(entities++, m_world.CreateEntity(entityFrom));
							GAIA_ASSERT(res.second);
						} break;
						case DELETE_ENTITY: {
							Entity entity = utils::unaligned_ref<Entity>((void*)&m_data[i]);
							i += sizeof(Entity);
							m_world.DeleteEntity(entity);
						} break;
						case ADD_COMPONENT:
						case ADD_COMPONENT_DATA: {
							// Type
							ComponentType type = (ComponentType)m_data[i];
							i += sizeof(ComponentType);
							// Entity
							Entity entity = utils::unaligned_ref<Entity>((void*)&m_data[i]);
							i += sizeof(Entity);

							// Components
							uint32_t infoIndex = utils::unaligned_ref<uint32_t>((void*)&m_data[i]);
							const auto* newInfo = GetComponentCache().GetComponentInfoFromIdx(infoIndex);
							i += sizeof(uint32_t);
							m_world.AddComponent_Internal(type, entity, newInfo);

							uint32_t indexInChunk;
							auto* pChunk = m_world.GetChunk(entity, indexInChunk);
							GAIA_ASSERT(pChunk != nullptr);

							if (type == ComponentType::CT_Chunk)
								indexInChunk = 0;

							if (cmd == ADD_COMPONENT_DATA) {
								// Skip the component index
								// TODO: Don't include the component index here
								uint32_t infoIndex2 = utils::unaligned_ref<uint32_t>((void*)&m_data[i]);
								(void)infoIndex2;
								i += sizeof(uint32_t);

								auto* pComponentDataStart = pChunk->GetDataPtrRW<false>(type, newInfo->infoIndex);
								auto* pComponentData = (void*)&pComponentDataStart[indexInChunk * newInfo->properties.size];
								memcpy(pComponentData, (const void*)&m_data[i], newInfo->properties.size);
								i += newInfo->properties.size;
							}
						} break;
						case ADD_COMPONENT_TO_TEMPENTITY:
						case ADD_COMPONENT_TO_TEMPENTITY_DATA: {
							// Type
							ComponentType type = (ComponentType)m_data[i];
							i += sizeof(ComponentType);
							// Entity
							Entity e = utils::unaligned_ref<Entity>((void*)&m_data[i]);
							i += sizeof(Entity);

							// For delayed entities we have to do a look in our map
							// of temporaries and find a link there
							const auto it = entityMap.find(e.id());
							// Link has to exist!
							GAIA_ASSERT(it != entityMap.end());

							Entity entity = it->second;

							// Components
							uint32_t infoIndex = utils::unaligned_ref<uint32_t>((void*)&m_data[i]);
							const auto* newInfo = GetComponentCache().GetComponentInfoFromIdx(infoIndex);
							i += sizeof(uint32_t);
							m_world.AddComponent_Internal(type, entity, newInfo);

							uint32_t indexInChunk;
							auto* pChunk = m_world.GetChunk(entity, indexInChunk);
							GAIA_ASSERT(pChunk != nullptr);

							if (type == ComponentType::CT_Chunk)
								indexInChunk = 0;

							if (cmd == ADD_COMPONENT_TO_TEMPENTITY_DATA) {
								// Skip the type index
								// TODO: Don't include the type index here
								uint32_t infoIndex2 = utils::unaligned_ref<uint32_t>((void*)&m_data[i]);
								(void)infoIndex2;
								i += sizeof(uint32_t);

								auto* pComponentDataStart = pChunk->GetDataPtrRW<false>(type, newInfo->infoIndex);
								auto* pComponentData = (void*)&pComponentDataStart[indexInChunk * newInfo->properties.size];
								memcpy(pComponentData, (const void*)&m_data[i], newInfo->properties.size);
								i += newInfo->properties.size;
							}
						} break;
						case SET_COMPONENT: {
							// Type
							ComponentType type = (ComponentType)m_data[i];
							i += sizeof(ComponentType);

							// Entity
							Entity entity = utils::unaligned_ref<Entity>((void*)&m_data[i]);
							i += sizeof(Entity);

							const auto& entityContainer = m_world.m_entities[entity.id()];
							auto* pChunk = entityContainer.pChunk;
							const auto indexInChunk = type == ComponentType::CT_Chunk ? 0 : entityContainer.idx;

							// Components
							{
								const auto infoIndex = utils::unaligned_ref<uint32_t>((void*)&m_data[i]);
								const auto* info = GetComponentCache().GetComponentInfoFromIdx(infoIndex);
								i += sizeof(uint32_t);

								auto* pComponentDataStart = pChunk->GetDataPtrRW<false>(type, info->infoIndex);
								auto* pComponentData = (void*)&pComponentDataStart[indexInChunk * info->properties.size];
								memcpy(pComponentData, (const void*)&m_data[i], info->properties.size);
								i += info->properties.size;
							}
						} break;
						case SET_COMPONENT_FOR_TEMPENTITY: {
							// Type
							ComponentType type = (ComponentType)m_data[i];
							i += sizeof(ComponentType);

							// Entity
							Entity e = utils::unaligned_ref<Entity>((void*)&m_data[i]);
							i += sizeof(Entity);

							// For delayed entities we have to do a look in our map
							// of temporaries and find a link there
							const auto it = entityMap.find(e.id());
							// Link has to exist!
							GAIA_ASSERT(it != entityMap.end());

							Entity entity = it->second;

							const auto& entityContainer = m_world.m_entities[entity.id()];
							auto* pChunk = entityContainer.pChunk;
							const auto indexInChunk = type == ComponentType::CT_Chunk ? 0 : entityContainer.idx;

							// Components
							{
								const auto infoIndex = utils::unaligned_ref<uint32_t>((void*)&m_data[i]);
								const auto* info = GetComponentCache().GetComponentInfoFromIdx(infoIndex);
								i += sizeof(uint32_t);

								auto* pComponentDataStart = pChunk->GetDataPtrRW<false>(type, info->infoIndex);
								auto* pComponentData = (void*)&pComponentDataStart[indexInChunk * info->properties.size];
								memcpy(pComponentData, (const void*)&m_data[i], info->properties.size);
								i += info->properties.size;
							}
						} break;
						case REMOVE_COMPONENT: {
							// Type
							ComponentType type = utils::unaligned_ref<ComponentType>((void*)&m_data[i]);
							i += sizeof(ComponentType);

							// Entity
							Entity e = utils::unaligned_ref<Entity>((void*)&m_data[i]);
							i += sizeof(Entity);

							// Components
							uint32_t infoIndex = utils::unaligned_ref<uint32_t>((void*)&m_data[i]);
							const auto* newInfo = GetComponentCache().GetComponentInfoFromIdx(infoIndex);
							i += sizeof(uint32_t);

							m_world.RemoveComponent_Internal(type, e, newInfo);
						} break;
					}
				}

				m_entities = 0;
				m_data.clear();
			}
		};
	} // namespace ecs
} // namespace gaia
