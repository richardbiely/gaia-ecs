#pragma once
#include <cinttypes>
#include <type_traits>

#include "../config/config.h"
#include "../containers/map.h"
#include "../containers/sarray_ext.h"
#include "../utils/mem.h"
#include "archetype.h"
#include "common.h"
#include "component.h"
#include "component_cache.h"
#include "data_buffer.h"
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
			DataBuffer m_data;
			uint32_t m_entities;

			template <typename TEntity, typename T>
			void AddComponent_Internal(TEntity entity) {
				// Entity
				m_data.Save(entity);

				// Components
				const auto& infoToAdd = GetComponentCacheRW().GetOrCreateComponentInfo<T>();
				m_data.Save(infoToAdd.componentId);
			}

			template <typename T>
			void SetComponentFinal_Internal(T&& data) {
				using U = std::decay_t<T>;

				m_data.Save(GetComponentId<U>());
				m_data.Save(std::forward<U>(data));
			}

			template <typename T>
			void SetComponentNoEntityNoSize_Internal(T&& data) {
				this->SetComponentFinal_Internal<T>(std::forward<T>(data));
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
				m_data.Save(entity);

				// Components
				SetComponentNoEntity_Internal(std::forward<T>(data));
			}

			template <typename T>
			void RemoveComponent_Internal(Entity entity) {
				// Entity
				m_data.Save(entity);

				// Components
				const auto& typeToRemove = GetComponentCache().GetComponentInfo<T>();
				m_data.Save(typeToRemove.componentId);
			}

			/*!
			Requests a new entity to be created from archetype
			\return Entity that will be created. The id is not usable right away. It
			will be filled with proper data after Commit()
			*/
			GAIA_NODISCARD TempEntity CreateEntity(Archetype& archetype) {
				m_data.Save(CREATE_ENTITY_FROM_ARCHETYPE);
				m_data.Save((uintptr_t)&archetype);

				return {m_entities++};
			}

		public:
			CommandBuffer(World& world): m_world(world), m_entities(0) {}
			~CommandBuffer() = default;

			CommandBuffer(CommandBuffer&&) = delete;
			CommandBuffer(const CommandBuffer&) = delete;
			CommandBuffer& operator=(CommandBuffer&&) = delete;
			CommandBuffer& operator=(const CommandBuffer&) = delete;

			/*!
			Requests a new entity to be created
			\return Entity that will be created. The id is not usable right away. It
			will be filled with proper data after Commit()
			*/
			GAIA_NODISCARD TempEntity CreateEntity() {
				m_data.Save(CREATE_ENTITY);
				return {m_entities++};
			}

			/*!
			Requests a new entity to be created by cloning an already existing
			entity \return Entity that will be created. The id is not usable right
			away. It will be filled with proper data after Commit()
			*/
			GAIA_NODISCARD TempEntity CreateEntity(Entity entityFrom) {
				m_data.Save(CREATE_ENTITY_FROM_ENTITY);
				m_data.Save(entityFrom);

				return {m_entities++};
			}

			/*!
			Requests an existing \param entity to be removed.
			*/
			void DeleteEntity(Entity entity) {
				m_data.Save(DELETE_ENTITY);
				m_data.Save(entity);
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

				m_data.Save(ADD_COMPONENT);
				if constexpr (IsGenericComponent<T>)
					m_data.Save(ComponentType::CT_Generic);
				else
					m_data.Save(ComponentType::CT_Chunk);
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

				m_data.Save(ADD_COMPONENT_TO_TEMPENTITY);
				if constexpr (IsGenericComponent<T>)
					m_data.Save(ComponentType::CT_Generic);
				else
					m_data.Save(ComponentType::CT_Chunk);
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

				m_data.Save(ADD_COMPONENT_DATA);
				if constexpr (IsGenericComponent<T>)
					m_data.Save(ComponentType::CT_Generic);
				else
					m_data.Save(ComponentType::CT_Chunk);
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
			bool AddComponent(TempEntity entity, T&& value) {
				using U = typename DeduceComponent<T>::Type;
				VerifyComponent<U>();

				m_data.Save(ADD_COMPONENT_TO_TEMPENTITY_DATA);
				if constexpr (IsGenericComponent<T>)
					m_data.Save(ComponentType::CT_Generic);
				else
					m_data.Save(ComponentType::CT_Chunk);

				AddComponent_Internal<TempEntity, U>(entity);
				SetComponentNoEntityNoSize_Internal(std::forward<U>(value));
				return true;
			}

			/*!
			Requests component data to be set to given values for a given entity.

			\warning Just like World::SetComponent, this function expects the
			given component infos to exist. Undefined behavior otherwise.
			*/
			template <typename T>
			void SetComponent(Entity entity, T&& value) {
				using U = typename DeduceComponent<T>::Type;
				VerifyComponent<U>();

				m_data.Save(SET_COMPONENT);
				if constexpr (IsGenericComponent<T>)
					m_data.Save(ComponentType::CT_Generic);
				else
					m_data.Save(ComponentType::CT_Chunk);

				SetComponent_Internal(entity, std::forward<U>(value));
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

				m_data.Save(SET_COMPONENT_FOR_TEMPENTITY);
				if constexpr (IsGenericComponent<T>)
					m_data.Save(ComponentType::CT_Generic);
				else
					m_data.Save(ComponentType::CT_Chunk);
				SetComponent_Internal(entity, std::forward<U>(data));
			}

			/*!
			Requests removal of a component from entity
			*/
			template <typename T>
			void RemoveComponent(Entity entity) {
				using U = typename DeduceComponent<T>::Type;
				VerifyComponent<U>();

				m_data.Save(REMOVE_COMPONENT);
				if constexpr (IsGenericComponent<T>)
					m_data.Save(ComponentType::CT_Generic);
				else
					m_data.Save(ComponentType::CT_Chunk);
				RemoveComponent_Internal<U>(entity);
			}

		private:
			struct CommandBufferCtx {
				ecs::World& world;
				DataBuffer& data;
				uint32_t entities;
				containers::map<uint32_t, Entity> entityMap;
			};

			using CommandBufferReadFunc = void (*)(CommandBufferCtx& ctx);
			static constexpr CommandBufferReadFunc CommandBufferRead[] = {
					// CREATE_ENTITY
					[](CommandBufferCtx& ctx) {
						[[maybe_unused]] const auto res = ctx.entityMap.try_emplace(ctx.entities++, ctx.world.CreateEntity());
						GAIA_ASSERT(res.second);
					},
					// CREATE_ENTITY_FROM_ARCHETYPE
					[](CommandBufferCtx& ctx) {
						uintptr_t ptr{};
						ctx.data.Load(ptr);

						auto* pArchetype = (Archetype*)ptr;
						[[maybe_unused]] const auto res =
								ctx.entityMap.try_emplace(ctx.entities++, ctx.world.CreateEntity(*pArchetype));
						GAIA_ASSERT(res.second);
					},
					// CREATE_ENTITY_FROM_ENTITY
					[](CommandBufferCtx& ctx) {
						Entity entityFrom{};
						ctx.data.Load(entityFrom);

						[[maybe_unused]] const auto res =
								ctx.entityMap.try_emplace(ctx.entities++, ctx.world.CreateEntity(entityFrom));
						GAIA_ASSERT(res.second);
					},
					// DELETE_ENTITY
					[](CommandBufferCtx& ctx) {
						Entity entity{};
						ctx.data.Load(entity);

						ctx.world.DeleteEntity(entity);
					},
					// ADD_COMPONENT
					[](CommandBufferCtx& ctx) {
						ComponentType componentType{};
						ctx.data.Load(componentType);
						Entity entity{};
						ctx.data.Load(entity);
						ComponentId componentId{};
						ctx.data.Load(componentId);

						const auto& newInfo = GetComponentCache().GetComponentInfo(componentId);
						ctx.world.AddComponent_Internal(componentType, entity, newInfo);

						uint32_t indexInChunk{};
						[[maybe_unused]] auto* pChunk = ctx.world.GetChunk(entity, indexInChunk);
						GAIA_ASSERT(pChunk != nullptr);
					},
					// ADD_COMPONENT_DATA
					[](CommandBufferCtx& ctx) {
						ComponentType componentType{};
						ctx.data.Load(componentType);
						Entity entity{};
						ctx.data.Load(entity);
						ComponentId componentId{};
						ctx.data.Load(componentId);
						ComponentId componentId2{};
						ctx.data.Load(componentId);
						// TODO: Don't include the component index here
						(void)componentId2;

						// Components
						const auto& newInfo = GetComponentCache().GetComponentInfo(componentId);
						const auto& newDesc = GetComponentCache().GetComponentDesc(componentId);
						ctx.world.AddComponent_Internal(componentType, entity, newInfo);

						uint32_t indexInChunk{};
						auto* pChunk = ctx.world.GetChunk(entity, indexInChunk);
						GAIA_ASSERT(pChunk != nullptr);

						if (componentType == ComponentType::CT_Chunk)
							indexInChunk = 0;

						auto* pComponentDataStart = pChunk->GetDataPtrRW<false>(componentType, newInfo.componentId);
						auto* pComponentData = (void*)&pComponentDataStart[(size_t)indexInChunk * newDesc.properties.size];
						ctx.data.Load(pComponentData, newDesc.properties.size);
					},
					// ADD_COMPONENT_TO_TEMPENTITY
					[](CommandBufferCtx& ctx) {
						ComponentType componentType{};
						ctx.data.Load(componentType);
						Entity e{};
						ctx.data.Load(e);
						ComponentId componentId{};
						ctx.data.Load(componentId);

						// For delayed entities we have to peek into our map of temporaries and find a link there
						const auto it = ctx.entityMap.find(e.id());
						// The link has to exist!
						GAIA_ASSERT(it != ctx.entityMap.end());

						Entity entity = it->second;

						// Components
						const auto& newInfo = GetComponentCache().GetComponentInfo(componentId);
						ctx.world.AddComponent_Internal(componentType, entity, newInfo);

						uint32_t indexInChunk{};
						[[maybe_unused]] auto* pChunk = ctx.world.GetChunk(entity, indexInChunk);
						GAIA_ASSERT(pChunk != nullptr);
					},
					// ADD_COMPONENT_TO_TEMPENTITY_DATA
					[](CommandBufferCtx& ctx) {
						ComponentType componentType{};
						ctx.data.Load(componentType);
						Entity e{};
						ctx.data.Load(e);
						ComponentId componentId{};
						ctx.data.Load(componentId);
						ComponentId componentId2{};
						ctx.data.Load(componentId);
						// TODO: Don't include the component index here
						(void)componentId2;

						// For delayed entities we have to do a look in our map
						// of temporaries and find a link there
						const auto it = ctx.entityMap.find(e.id());
						// Link has to exist!
						GAIA_ASSERT(it != ctx.entityMap.end());

						Entity entity = it->second;

						// Components
						const auto& newInfo = GetComponentCache().GetComponentInfo(componentId);
						const auto& newDesc = GetComponentCache().GetComponentDesc(componentId);
						ctx.world.AddComponent_Internal(componentType, entity, newInfo);

						uint32_t indexInChunk{};
						auto* pChunk = ctx.world.GetChunk(entity, indexInChunk);
						GAIA_ASSERT(pChunk != nullptr);

						if (componentType == ComponentType::CT_Chunk)
							indexInChunk = 0;

						auto* pComponentDataStart = pChunk->GetDataPtrRW<false>(componentType, newDesc.componentId);
						auto* pComponentData = (void*)&pComponentDataStart[(size_t)indexInChunk * newDesc.properties.size];
						ctx.data.Load(pComponentData, newDesc.properties.size);
					},
					// SET_COMPONENT
					[](CommandBufferCtx& ctx) {
						ComponentType componentType{};
						ctx.data.Load(componentType);
						Entity entity{};
						ctx.data.Load(entity);
						ComponentId componentId{};
						ctx.data.Load(componentId);

						const auto& entityContainer = ctx.world.m_entities[entity.id()];
						auto* pChunk = entityContainer.pChunk;
						const auto indexInChunk = componentType == ComponentType::CT_Chunk ? 0U : entityContainer.idx;

						// Components
						{
							const auto& desc = GetComponentCache().GetComponentDesc(componentId);

							auto* pComponentDataStart = pChunk->GetDataPtrRW<false>(componentType, componentId);
							auto* pComponentData = (void*)&pComponentDataStart[(size_t)indexInChunk * desc.properties.size];
							ctx.data.Load(pComponentData, desc.properties.size);
						}
					},
					// SET_COMPONENT_FOR_TEMPENTITY
					[](CommandBufferCtx& ctx) {
						ComponentType componentType{};
						ctx.data.Load(componentType);
						Entity e{};
						ctx.data.Load(e);
						ComponentId componentId{};
						ctx.data.Load(componentId);

						// For delayed entities we have to do a look in our map
						// of temporaries and find a link there
						const auto it = ctx.entityMap.find(e.id());
						// Link has to exist!
						GAIA_ASSERT(it != ctx.entityMap.end());

						Entity entity = it->second;

						const auto& entityContainer = ctx.world.m_entities[entity.id()];
						auto* pChunk = entityContainer.pChunk;
						const auto indexInChunk = componentType == ComponentType::CT_Chunk ? 0U : entityContainer.idx;

						// Components
						{
							const auto& desc = GetComponentCache().GetComponentDesc(componentId);

							auto* pComponentDataStart = pChunk->GetDataPtrRW<false>(componentType, componentId);
							auto* pComponentData = (void*)&pComponentDataStart[(size_t)indexInChunk * desc.properties.size];
							ctx.data.Load(pComponentData, desc.properties.size);
						}
					},
					// REMOVE_COMPONENT
					[](CommandBufferCtx& ctx) {
						ComponentType componentType{};
						ctx.data.Load(componentType);
						Entity entity{};
						ctx.data.Load(entity);
						ComponentId componentId{};
						ctx.data.Load(componentId);

						// Components
						const auto& newInfo = GetComponentCache().GetComponentInfo(componentId);
						ctx.world.RemoveComponent_Internal(componentType, entity, newInfo);
					}};

		public:
			/*!
			Commits all queued changes.
			*/
			void Commit() {
				CommandBufferCtx ctx{m_world, m_data, 0, {}};

				// Extract data from the buffer
				m_data.Seek(0);
				while (m_data.GetPos() < m_data.Size()) {
					CommandBufferCmd id{};
					m_data.Load(id);
					CommandBufferRead[id](ctx);
				}

				m_entities = 0;
				m_data.Reset();
			} // namespace ecs
		}; // namespace gaia
	} // namespace ecs
} // namespace gaia
