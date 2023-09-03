#pragma once
#include "../config/config.h"

#include <cinttypes>
#include <type_traits>

#include "../containers/map.h"
#include "../containers/sarray_ext.h"
#include "../utils/mem.h"
#include "../utils/serialization.h"
#include "archetype.h"
#include "common.h"
#include "component.h"
#include "component_cache.h"
#include "data_buffer.h"
#include "entity.h"
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
			struct CommandBufferCtx: DataBuffer_SerializationWrapper {
				ecs::World& world;
				uint32_t entities;
				containers::map<uint32_t, Entity> entityMap;
			};

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

			struct CommandBufferCmd_t {
				CommandBufferCmd id;
			};

			struct CREATE_ENTITY_t: CommandBufferCmd_t {};
			struct CREATE_ENTITY_FROM_ARCHETYPE_t: CommandBufferCmd_t {
				uint64_t archetypePtr;

				void Commit(CommandBufferCtx& ctx) {
					auto* pArchetype = (archetype::Archetype*)archetypePtr;
					[[maybe_unused]] const auto res =
							ctx.entityMap.try_emplace(ctx.entities++, ctx.world.CreateEntity(*pArchetype));
					GAIA_ASSERT(res.second);
				}
			};
			struct CREATE_ENTITY_FROM_ENTITY_t: CommandBufferCmd_t {
				Entity entity;

				void Commit(CommandBufferCtx& ctx) {
					[[maybe_unused]] const auto res = ctx.entityMap.try_emplace(ctx.entities++, ctx.world.CreateEntity(entity));
					GAIA_ASSERT(res.second);
				}
			};
			struct DELETE_ENTITY_t: CommandBufferCmd_t {
				Entity entity;

				void Commit(CommandBufferCtx& ctx) {
					ctx.world.DeleteEntity(entity);
				}
			};
			struct ADD_COMPONENT_t: CommandBufferCmd_t {
				Entity entity;
				component::ComponentId componentId;
				component::ComponentType componentType;

				void Commit(CommandBufferCtx& ctx) {
					const auto& info = ComponentCache::Get().GetComponentInfo(componentId);
					ctx.world.AddComponent_Internal(componentType, entity, info);

#if GAIA_ASSERT_ENABLED
					[[maybe_unused]] uint32_t indexInChunk{};
					[[maybe_unused]] auto* pChunk = ctx.world.GetChunk(entity, indexInChunk);
					GAIA_ASSERT(pChunk != nullptr);
#endif
				}
			};
			struct ADD_COMPONENT_DATA_t: CommandBufferCmd_t {
				Entity entity;
				component::ComponentId componentId;
				component::ComponentType componentType;

				void Commit(CommandBufferCtx& ctx) {
					const auto& info = ComponentCache::Get().GetComponentInfo(componentId);
					ctx.world.AddComponent_Internal(componentType, entity, info);

					uint32_t indexInChunk{};
					auto* pChunk = ctx.world.GetChunk(entity, indexInChunk);
					GAIA_ASSERT(pChunk != nullptr);

					if (componentType == component::ComponentType::CT_Chunk)
						indexInChunk = 0;

					// Component data
					const auto& desc = ComponentCache::Get().GetComponentDesc(componentId);
					const auto offset = pChunk->FindDataOffset(componentType, info.componentId);
					auto* pComponentData = (void*)&pChunk->GetData(offset + (uint32_t)indexInChunk * desc.properties.size);
					ctx.buffer().LoadComponent(pComponentData, componentId);
				}
			};
			struct ADD_COMPONENT_TO_TEMPENTITY_t: CommandBufferCmd_t {
				TempEntity tempEntity;
				component::ComponentId componentId;
				component::ComponentType componentType;

				void Commit(CommandBufferCtx& ctx) {
					// For delayed entities we have to do a look in our map
					// of temporaries and find a link there
					const auto it = ctx.entityMap.find(tempEntity.id);
					// Link has to exist!
					GAIA_ASSERT(it != ctx.entityMap.end());

					Entity entity = it->second;

					const auto& info = ComponentCache::Get().GetComponentInfo(componentId);
					ctx.world.AddComponent_Internal(componentType, entity, info);

#if GAIA_ASSERT_ENABLED
					[[maybe_unused]] uint32_t indexInChunk{};
					[[maybe_unused]] auto* pChunk = ctx.world.GetChunk(entity, indexInChunk);
					GAIA_ASSERT(pChunk != nullptr);
#endif
				}
			};
			struct ADD_COMPONENT_TO_TEMPENTITY_DATA_t: CommandBufferCmd_t {
				TempEntity tempEntity;
				component::ComponentId componentId;
				component::ComponentType componentType;

				void Commit(CommandBufferCtx& ctx) {
					// For delayed entities we have to do a look in our map
					// of temporaries and find a link there
					const auto it = ctx.entityMap.find(tempEntity.id);
					// Link has to exist!
					GAIA_ASSERT(it != ctx.entityMap.end());

					Entity entity = it->second;

					// Components
					const auto& info = ComponentCache::Get().GetComponentInfo(componentId);
					ctx.world.AddComponent_Internal(componentType, entity, info);

					uint32_t indexInChunk{};
					auto* pChunk = ctx.world.GetChunk(entity, indexInChunk);
					GAIA_ASSERT(pChunk != nullptr);

					if (componentType == component::ComponentType::CT_Chunk)
						indexInChunk = 0;

					// Component data
					const auto& desc = ComponentCache::Get().GetComponentDesc(componentId);
					const auto offset = pChunk->FindDataOffset(componentType, desc.componentId);
					auto* pComponentData = (void*)&pChunk->GetData(offset + (uint32_t)indexInChunk * desc.properties.size);
					ctx.buffer().LoadComponent(pComponentData, componentId);
				}
			};
			struct SET_COMPONENT_t: CommandBufferCmd_t {
				Entity entity;
				component::ComponentId componentId;
				component::ComponentType componentType;

				void Commit(CommandBufferCtx& ctx) {
					const auto& entityContainer = ctx.world.m_entities[entity.id()];
					auto* pChunk = entityContainer.pChunk;
					const auto indexInChunk = componentType == component::ComponentType::CT_Chunk ? 0U : entityContainer.idx;

					// Component data
					const auto& desc = ComponentCache::Get().GetComponentDesc(componentId);
					const auto offset = pChunk->FindDataOffset(componentType, componentId);
					auto* pComponentData = (void*)&pChunk->GetData(offset + (uint32_t)indexInChunk * desc.properties.size);
					ctx.buffer().LoadComponent(pComponentData, componentId);
				}
			};
			struct SET_COMPONENT_FOR_TEMPENTITY_t: CommandBufferCmd_t {
				TempEntity tempEntity;
				component::ComponentId componentId;
				component::ComponentType componentType;

				void Commit(CommandBufferCtx& ctx) {
					// For delayed entities we have to do a look in our map
					// of temporaries and find a link there
					const auto it = ctx.entityMap.find(tempEntity.id);
					// Link has to exist!
					GAIA_ASSERT(it != ctx.entityMap.end());

					Entity entity = it->second;

					const auto& entityContainer = ctx.world.m_entities[entity.id()];
					auto* pChunk = entityContainer.pChunk;
					const auto indexInChunk = componentType == component::ComponentType::CT_Chunk ? 0U : entityContainer.idx;

					// Component data
					const auto& desc = ComponentCache::Get().GetComponentDesc(componentId);
					const auto offset = pChunk->FindDataOffset(componentType, componentId);
					auto* pComponentData = (void*)&pChunk->GetData(offset + (uint32_t)indexInChunk * desc.properties.size);
					ctx.buffer().LoadComponent(pComponentData, componentId);
				}
			};
			struct REMOVE_COMPONENT_t: CommandBufferCmd_t {
				Entity entity;
				component::ComponentId componentId;
				component::ComponentType componentType;

				void Commit(CommandBufferCtx& ctx) {
					const auto& info = ComponentCache::Get().GetComponentInfo(componentId);
					ctx.world.RemoveComponent_Internal(componentType, entity, info);
				}
			};

			friend class World;

			World& m_world;
			DataBuffer m_data;
			uint32_t m_entities;

			/*!
			Requests a new entity to be created from archetype
			\return Entity that will be created. The id is not usable right away. It
			will be filled with proper data after Commit()
			*/
			GAIA_NODISCARD TempEntity CreateEntity(archetype::Archetype& archetype) {
				DataBuffer_SerializationWrapper s(m_data);
				s.save(CREATE_ENTITY_FROM_ARCHETYPE);

				CREATE_ENTITY_FROM_ARCHETYPE_t cmd;
				cmd.archetypePtr = (uintptr_t)&archetype;
				serialization::save(s, cmd);

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
				DataBuffer_SerializationWrapper s(m_data);
				s.save(CREATE_ENTITY);

				return {m_entities++};
			}

			/*!
			Requests a new entity to be created by cloning an already existing
			entity \return Entity that will be created. The id is not usable right
			away. It will be filled with proper data after Commit()
			*/
			GAIA_NODISCARD TempEntity CreateEntity(Entity entityFrom) {
				DataBuffer_SerializationWrapper s(m_data);
				s.save(CREATE_ENTITY_FROM_ENTITY);

				CREATE_ENTITY_FROM_ENTITY_t cmd;
				cmd.entity = entityFrom;
				serialization::save(s, cmd);

				return {m_entities++};
			}

			/*!
			Requests an existing \param entity to be removed.
			*/
			void DeleteEntity(Entity entity) {
				DataBuffer_SerializationWrapper s(m_data);
				s.save(DELETE_ENTITY);

				DELETE_ENTITY_t cmd;
				cmd.entity = entity;
				serialization::save(s, cmd);
			}

			/*!
			Requests a component to be added to entity.
			*/
			template <typename T>
			void AddComponent(Entity entity) {
				// Make sure the component is registered
				const auto& info = ComponentCache::Get().GetOrCreateComponentInfo<T>();

				using U = typename component::DeduceComponent<T>::Type;
				component::VerifyComponent<U>();

				DataBuffer_SerializationWrapper s(m_data);
				s.save(ADD_COMPONENT);

				ADD_COMPONENT_t cmd;
				cmd.entity = entity;
				cmd.componentType = component::GetComponentType<T>();
				cmd.componentId = info.componentId;
				serialization::save(s, cmd);
			}

			/*!
			Requests a component to be added to temporary entity.
			*/
			template <typename T>
			void AddComponent(TempEntity entity) {
				// Make sure the component is registered
				const auto& info = ComponentCache::Get().GetOrCreateComponentInfo<T>();

				using U = typename component::DeduceComponent<T>::Type;
				component::VerifyComponent<U>();

				DataBuffer_SerializationWrapper s(m_data);
				s.save(ADD_COMPONENT_TO_TEMPENTITY);

				ADD_COMPONENT_TO_TEMPENTITY_t cmd;
				cmd.tempEntity = entity;
				cmd.componentType = component::GetComponentType<T>();
				cmd.componentId = component::GetComponentId<T>();
				serialization::save(s, cmd);
			}

			/*!
			Requests a component to be added to entity.
			*/
			template <typename T>
			void AddComponent(Entity entity, T&& value) {
				// Make sure the component is registered
				const auto& info = ComponentCache::Get().GetOrCreateComponentInfo<T>();

				using U = typename component::DeduceComponent<T>::Type;
				component::VerifyComponent<U>();

				DataBuffer_SerializationWrapper s(m_data);
				s.save(ADD_COMPONENT_DATA);

				ADD_COMPONENT_DATA_t cmd;
				cmd.entity = entity;
				cmd.componentType = component::GetComponentType<T>();
				cmd.componentId = info.componentId;
				serialization::save(s, cmd);

				const auto& desc = ComponentCache::Get().GetComponentDesc(cmd.componentId);
				s.buffer().SaveComponent(std::forward<U>(value));
			}

			/*!
			Requests a component to be added to temporary entity.
			*/
			template <typename T>
			void AddComponent(TempEntity entity, T&& value) {
				// Make sure the component is registered
				const auto& info = ComponentCache::Get().GetOrCreateComponentInfo<T>();

				using U = typename component::DeduceComponent<T>::Type;
				component::VerifyComponent<U>();

				DataBuffer_SerializationWrapper s(m_data);
				s.save(ADD_COMPONENT_TO_TEMPENTITY_DATA);

				ADD_COMPONENT_TO_TEMPENTITY_t cmd;
				cmd.tempEntity = entity;
				cmd.componentType = component::GetComponentType<T>();
				cmd.componentId = info.componentId;
				serialization::save(s, cmd);

				const auto& desc = ComponentCache::Get().GetComponentDesc(cmd.componentId);
				s.buffer().SaveComponent(std::forward<U>(value));
			}

			/*!
			Requests component data to be set to given values for a given entity.
			*/
			template <typename T>
			void SetComponent(Entity entity, T&& value) {
				// No need to check if the component is registered.
				// If we want to set the value of a component we must have created it already.
				// (void)ComponentCache::Get().GetComponentInfo<T>();

				using U = typename component::DeduceComponent<T>::Type;
				component::VerifyComponent<U>();

				DataBuffer_SerializationWrapper s(m_data);
				s.save(SET_COMPONENT);

				SET_COMPONENT_t cmd;
				cmd.entity = entity;
				cmd.componentType = component::GetComponentType<T>();
				cmd.componentId = component::GetComponentId<T>();
				serialization::save(s, cmd);

				const auto& desc = ComponentCache::Get().GetComponentDesc(cmd.componentId);
				s.buffer().SaveComponent(std::forward<U>(value));
			}

			/*!
			Requests component data to be set to given values for a given temp
			entity.
			*/
			template <typename T>
			void SetComponent(TempEntity entity, T&& value) {
				// No need to check if the component is registered.
				// If we want to set the value of a component we must have created it already.
				// (void)ComponentCache::Get().GetOrCreateComponentInfo<T>();

				using U = typename component::DeduceComponent<T>::Type;
				component::VerifyComponent<U>();

				DataBuffer_SerializationWrapper s(m_data);
				s.save(SET_COMPONENT_FOR_TEMPENTITY);

				SET_COMPONENT_FOR_TEMPENTITY_t cmd;
				cmd.tempEntity = entity;
				cmd.componentType = component::GetComponentType<T>();
				cmd.componentId = component::GetComponentId<T>();
				serialization::save(s, cmd);

				const auto& desc = ComponentCache::Get().GetComponentDesc(cmd.componentId);
				s.buffer().SaveComponent(std::forward<U>(value));
			}

			/*!
			Requests removal of a component from entity
			*/
			template <typename T>
			void RemoveComponent(Entity entity) {
				// No need to check if the component is registered.
				// If we want to remove a component we must have created it already.
				// (void)ComponentCache::Get().GetOrCreateComponentInfo<T>();

				using U = typename component::DeduceComponent<T>::Type;
				component::VerifyComponent<U>();

				DataBuffer_SerializationWrapper s(m_data);
				s.save(REMOVE_COMPONENT);

				REMOVE_COMPONENT_t cmd;
				cmd.entity = entity;
				cmd.componentType = component::GetComponentType<T>();
				cmd.componentId = component::GetComponentId<T>();
				serialization::save(s, cmd);
			}

		private:
			using CommandBufferReadFunc = void (*)(CommandBufferCtx& ctx);
			static constexpr CommandBufferReadFunc CommandBufferRead[] = {
					// CREATE_ENTITY
					[](CommandBufferCtx& ctx) {
						[[maybe_unused]] const auto res = ctx.entityMap.try_emplace(ctx.entities++, ctx.world.CreateEntity());
						GAIA_ASSERT(res.second);
					},
					// CREATE_ENTITY_FROM_ARCHETYPE
					[](CommandBufferCtx& ctx) {
						CREATE_ENTITY_FROM_ARCHETYPE_t cmd;
						serialization::load(ctx, cmd);
						cmd.Commit(ctx);
					},
					// CREATE_ENTITY_FROM_ENTITY
					[](CommandBufferCtx& ctx) {
						CREATE_ENTITY_FROM_ENTITY_t cmd;
						serialization::load(ctx, cmd);
						cmd.Commit(ctx);
					},
					// DELETE_ENTITY
					[](CommandBufferCtx& ctx) {
						DELETE_ENTITY_t cmd;
						serialization::load(ctx, cmd);
						cmd.Commit(ctx);
					},
					// ADD_COMPONENT
					[](CommandBufferCtx& ctx) {
						ADD_COMPONENT_t cmd;
						serialization::load(ctx, cmd);
						cmd.Commit(ctx);
					},
					// ADD_COMPONENT_DATA
					[](CommandBufferCtx& ctx) {
						ADD_COMPONENT_DATA_t cmd;
						serialization::load(ctx, cmd);
						cmd.Commit(ctx);
					},
					// ADD_COMPONENT_TO_TEMPENTITY
					[](CommandBufferCtx& ctx) {
						ADD_COMPONENT_TO_TEMPENTITY_t cmd;
						serialization::load(ctx, cmd);
						cmd.Commit(ctx);
					},
					// ADD_COMPONENT_TO_TEMPENTITY_DATA
					[](CommandBufferCtx& ctx) {
						ADD_COMPONENT_TO_TEMPENTITY_DATA_t cmd;
						serialization::load(ctx, cmd);
						cmd.Commit(ctx);
					},
					// SET_COMPONENT
					[](CommandBufferCtx& ctx) {
						SET_COMPONENT_t cmd;
						serialization::load(ctx, cmd);
						cmd.Commit(ctx);
					},
					// SET_COMPONENT_FOR_TEMPENTITY
					[](CommandBufferCtx& ctx) {
						SET_COMPONENT_FOR_TEMPENTITY_t cmd;
						serialization::load(ctx, cmd);
						cmd.Commit(ctx);
					},
					// REMOVE_COMPONENT
					[](CommandBufferCtx& ctx) {
						REMOVE_COMPONENT_t cmd;
						serialization::load(ctx, cmd);
						cmd.Commit(ctx);
					}};

		public:
			/*!
			Commits all queued changes.
			*/
			void Commit() {
				CommandBufferCtx ctx{m_data, m_world, 0, {}};

				// Extract data from the buffer
				m_data.Seek(0);
				while (m_data.GetPos() < m_data.Size()) {
					CommandBufferCmd id{};
					ctx.load(id);
					CommandBufferRead[id](ctx);
				}

				m_entities = 0;
				m_data.Reset();
			} // namespace ecs
		}; // namespace gaia
	} // namespace ecs
} // namespace gaia
