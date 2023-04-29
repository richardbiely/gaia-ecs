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
			using DataBuffer = containers::darray<uint8_t>;

			World& m_world;
			DataBuffer m_data;
			uint32_t m_entities;

			template <typename T>
			void AddData(T&& value) {
				// When adding data, if what we add is exactly the same as the buffer type, we can simply push_back
				if constexpr (
						sizeof(T) == sizeof(DataBuffer::value_type) ||
						(std::is_enum_v<T> && std::is_same_v<std::underlying_type<T>, DataBuffer::value_type>)) {
					m_data.push_back(std::forward<T>(value));
				}
				// When the data type does not match the buffer type, we perform a memory safe operation
				else {
					const auto lastIndex = m_data.size();
					m_data.resize(lastIndex + sizeof(T));

					utils::unaligned_ref<T> mem(&m_data[lastIndex]);
					mem = std::forward<T>(value);
				}
			}

			template <typename T>
			void SetData(T&& value, uint32_t& lastIndex) {
				utils::unaligned_ref<T> mem(&m_data[lastIndex]);
				mem = std::forward<T>(value);

				lastIndex += sizeof(T);
			}

			template <typename TEntity, typename T>
			void AddComponent_Internal(TEntity entity) {
				// Entity
				AddData(entity);

				// Components
				const auto& infoToAdd = GetComponentCacheRW().GetOrCreateComponentInfo<T>();
				AddData(infoToAdd.componentId);
			}

			template <typename T>
			void SetComponentFinal_Internal(uint32_t& index, T&& data) {
				using U = std::decay_t<T>;

				// Component info
				SetData(utils::type_info::id<U>(), index);

				// Component data
				SetData(std::forward<U>(data), index);
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
				AddData(entity);

				// Components
				SetComponentNoEntity_Internal(std::forward<T>(data));
			}

			template <typename T>
			void RemoveComponent_Internal(Entity entity) {
				// Entity
				AddData(entity);

				// Components
				{
					const auto& typeToRemove = GetComponentCache().GetComponentInfo<T>();

					// Component info
					AddData(typeToRemove.componentId);
				}
			}

			/*!
			Requests a new entity to be created from archetype
			\return Entity that will be created. The id is not usable right away. It
			will be filled with proper data after Commit()
			*/
			GAIA_NODISCARD TempEntity CreateEntity(Archetype& archetype) {
				AddData(CREATE_ENTITY_FROM_ARCHETYPE);
				AddData((uintptr_t)&archetype);

				return {m_entities++};
			}

		public:
			CommandBuffer(World& world): m_world(world), m_entities(0) {
				m_data.reserve(256);
			}
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
				AddData(CREATE_ENTITY);
				return {m_entities++};
			}

			/*!
			Requests a new entity to be created by cloning an already existing
			entity \return Entity that will be created. The id is not usable right
			away. It will be filled with proper data after Commit()
			*/
			GAIA_NODISCARD TempEntity CreateEntity(Entity entityFrom) {
				AddData(CREATE_ENTITY_FROM_ENTITY);
				AddData(entityFrom);

				return {m_entities++};
			}

			/*!
			Requests an existing \param entity to be removed.
			*/
			void DeleteEntity(Entity entity) {
				AddData(DELETE_ENTITY);
				AddData(entity);
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

				AddData(ADD_COMPONENT);
				if constexpr (IsGenericComponent<T>)
					AddData(ComponentType::CT_Generic);
				else
					AddData(ComponentType::CT_Chunk);
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

				AddData(ADD_COMPONENT_TO_TEMPENTITY);
				if constexpr (IsGenericComponent<T>)
					AddData(ComponentType::CT_Generic);
				else
					AddData(ComponentType::CT_Chunk);
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

				AddData(ADD_COMPONENT_DATA);
				if constexpr (IsGenericComponent<T>)
					AddData(ComponentType::CT_Generic);
				else
					AddData(ComponentType::CT_Chunk);
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

				AddData(ADD_COMPONENT_TO_TEMPENTITY_DATA);
				if constexpr (IsGenericComponent<T>)
					AddData(ComponentType::CT_Generic);
				else
					AddData(ComponentType::CT_Chunk);
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

				AddData(SET_COMPONENT);
				if constexpr (IsGenericComponent<T>)
					AddData(ComponentType::CT_Generic);
				else
					AddData(ComponentType::CT_Chunk);
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

				AddData(SET_COMPONENT_FOR_TEMPENTITY);
				if constexpr (IsGenericComponent<T>)
					AddData(ComponentType::CT_Generic);
				else
					AddData(ComponentType::CT_Chunk);
				SetComponent_Internal(entity, std::forward<U>(data));
			}

			/*!
			Requests removal of a component from entity
			*/
			template <typename T>
			void RemoveComponent(Entity entity) {
				using U = typename DeduceComponent<T>::Type;
				VerifyComponent<U>();

				AddData(REMOVE_COMPONENT);
				if constexpr (IsGenericComponent<T>)
					AddData(ComponentType::CT_Generic);
				else
					AddData(ComponentType::CT_Chunk);
				RemoveComponent_Internal<U>(entity);
			}

		private:
			struct CommandBufferCtx {
				ecs::World& world;
				DataBuffer& data;
				uint32_t dataOffset;
				uint32_t entities;
				containers::map<uint32_t, Entity> entityMap;
			};

			using CmdBufferCmdFunc = void (*)(CommandBufferCtx& ctx);
			static constexpr CmdBufferCmdFunc CommandBufferCmd[] = {
					// CREATE_ENTITY
					[](CommandBufferCtx& ctx) {
						[[maybe_unused]] const auto res = ctx.entityMap.try_emplace(ctx.entities++, ctx.world.CreateEntity());
						GAIA_ASSERT(res.second);
					},
					// CREATE_ENTITY_FROM_ARCHETYPE
					[](CommandBufferCtx& ctx) {
						uintptr_t ptr = utils::unaligned_ref<uintptr_t>((void*)&ctx.data[ctx.dataOffset]);
						auto* pArchetype = (Archetype*)ptr;
						ctx.dataOffset += sizeof(void*);
						[[maybe_unused]] const auto res =
								ctx.entityMap.try_emplace(ctx.entities++, ctx.world.CreateEntity(*pArchetype));
						GAIA_ASSERT(res.second);
					},
					// CREATE_ENTITY_FROM_ENTITY
					[](CommandBufferCtx& ctx) {
						Entity entityFrom = utils::unaligned_ref<Entity>((void*)&ctx.data[ctx.dataOffset]);
						ctx.dataOffset += sizeof(Entity);
						[[maybe_unused]] const auto res =
								ctx.entityMap.try_emplace(ctx.entities++, ctx.world.CreateEntity(entityFrom));
						GAIA_ASSERT(res.second);
					},
					// DELETE_ENTITY
					[](CommandBufferCtx& ctx) {
						Entity entity = utils::unaligned_ref<Entity>((void*)&ctx.data[ctx.dataOffset]);
						ctx.dataOffset += sizeof(Entity);
						ctx.world.DeleteEntity(entity);
					},
					// ADD_COMPONENT
					[](CommandBufferCtx& ctx) {
						// Type
						ComponentType componentType = (ComponentType)ctx.data[ctx.dataOffset];
						ctx.dataOffset += sizeof(ComponentType);
						// Entity
						Entity entity = utils::unaligned_ref<Entity>((void*)&ctx.data[ctx.dataOffset]);
						ctx.dataOffset += sizeof(Entity);

						// Components
						uint32_t componentId = utils::unaligned_ref<uint32_t>((void*)&ctx.data[ctx.dataOffset]);
						const auto& newInfo = GetComponentCache().GetComponentInfo(componentId);
						ctx.dataOffset += sizeof(uint32_t);
						ctx.world.AddComponent_Internal(componentType, entity, newInfo);

						uint32_t indexInChunk{};
						[[maybe_unused]] auto* pChunk = ctx.world.GetChunk(entity, indexInChunk);
						GAIA_ASSERT(pChunk != nullptr);
					},
					// ADD_COMPONENT_DATA
					[](CommandBufferCtx& ctx) {
						// Type
						ComponentType componentType = (ComponentType)ctx.data[ctx.dataOffset];
						ctx.dataOffset += sizeof(ComponentType);
						// Entity
						Entity entity = utils::unaligned_ref<Entity>((void*)&ctx.data[ctx.dataOffset]);
						ctx.dataOffset += sizeof(Entity);

						// Components
						uint32_t componentId = utils::unaligned_ref<uint32_t>((void*)&ctx.data[ctx.dataOffset]);
						const auto& newInfo = GetComponentCache().GetComponentInfo(componentId);
						ctx.dataOffset += sizeof(uint32_t);
						ctx.world.AddComponent_Internal(componentType, entity, newInfo);

						uint32_t indexInChunk{};
						auto* pChunk = ctx.world.GetChunk(entity, indexInChunk);
						GAIA_ASSERT(pChunk != nullptr);

						if (componentType == ComponentType::CT_Chunk)
							indexInChunk = 0;

						// Skip the component index
						// TODO: Don't include the component index here
						uint32_t componentId2 = utils::unaligned_ref<uint32_t>((void*)&ctx.data[ctx.dataOffset]);
						(void)componentId2;
						ctx.dataOffset += sizeof(uint32_t);

						auto* pComponentDataStart = pChunk->GetDataPtrRW<false>(componentType, newInfo.componentId);
						auto* pComponentData = (void*)&pComponentDataStart[(size_t)indexInChunk * newInfo.properties.size];
						memcpy(pComponentData, (const void*)&ctx.data[ctx.dataOffset], newInfo.properties.size);
						ctx.dataOffset += newInfo.properties.size;
					},
					// ADD_COMPONENT_TO_TEMPENTITY
					[](CommandBufferCtx& ctx) {
						// Type
						ComponentType componentType = (ComponentType)ctx.data[ctx.dataOffset];
						ctx.dataOffset += sizeof(ComponentType);
						// Entity
						Entity e = utils::unaligned_ref<Entity>((void*)&ctx.data[ctx.dataOffset]);
						ctx.dataOffset += sizeof(Entity);

						// For delayed entities we have to do a look in our map
						// of temporaries and find a link there
						const auto it = ctx.entityMap.find(e.id());
						// Link has to exist!
						GAIA_ASSERT(it != ctx.entityMap.end());

						Entity entity = it->second;

						// Components
						uint32_t componentId = utils::unaligned_ref<uint32_t>((void*)&ctx.data[ctx.dataOffset]);
						const auto& newInfo = GetComponentCache().GetComponentInfo(componentId);
						ctx.dataOffset += sizeof(uint32_t);
						ctx.world.AddComponent_Internal(componentType, entity, newInfo);

						uint32_t indexInChunk{};
						[[maybe_unused]] auto* pChunk = ctx.world.GetChunk(entity, indexInChunk);
						GAIA_ASSERT(pChunk != nullptr);
					},
					// ADD_COMPONENT_TO_TEMPENTITY_DATA
					[](CommandBufferCtx& ctx) {
						// Type
						ComponentType componentType = (ComponentType)ctx.data[ctx.dataOffset];
						ctx.dataOffset += sizeof(ComponentType);
						// Entity
						Entity e = utils::unaligned_ref<Entity>((void*)&ctx.data[ctx.dataOffset]);
						ctx.dataOffset += sizeof(Entity);

						// For delayed entities we have to do a look in our map
						// of temporaries and find a link there
						const auto it = ctx.entityMap.find(e.id());
						// Link has to exist!
						GAIA_ASSERT(it != ctx.entityMap.end());

						Entity entity = it->second;

						// Components
						uint32_t componentId = utils::unaligned_ref<uint32_t>((void*)&ctx.data[ctx.dataOffset]);
						const auto& newInfo = GetComponentCache().GetComponentInfo(componentId);
						ctx.dataOffset += sizeof(uint32_t);
						ctx.world.AddComponent_Internal(componentType, entity, newInfo);

						uint32_t indexInChunk{};
						auto* pChunk = ctx.world.GetChunk(entity, indexInChunk);
						GAIA_ASSERT(pChunk != nullptr);

						if (componentType == ComponentType::CT_Chunk)
							indexInChunk = 0;

						// Skip the type index
						// TODO: Don't include the type index here
						uint32_t componentId2 = utils::unaligned_ref<uint32_t>((void*)&ctx.data[ctx.dataOffset]);
						(void)componentId2;
						ctx.dataOffset += sizeof(uint32_t);

						auto* pComponentDataStart = pChunk->GetDataPtrRW<false>(componentType, newInfo.componentId);
						auto* pComponentData = (void*)&pComponentDataStart[(size_t)indexInChunk * newInfo.properties.size];
						memcpy(pComponentData, (const void*)&ctx.data[ctx.dataOffset], newInfo.properties.size);
						ctx.dataOffset += newInfo.properties.size;
					},
					// SET_COMPONENT
					[](CommandBufferCtx& ctx) {
						// Type
						ComponentType componentType = (ComponentType)ctx.data[ctx.dataOffset];
						ctx.dataOffset += sizeof(ComponentType);

						// Entity
						Entity entity = utils::unaligned_ref<Entity>((void*)&ctx.data[ctx.dataOffset]);
						ctx.dataOffset += sizeof(Entity);

						const auto& entityContainer = ctx.world.m_entities[entity.id()];
						auto* pChunk = entityContainer.pChunk;
						const auto indexInChunk = componentType == ComponentType::CT_Chunk ? 0U : entityContainer.idx;

						// Components
						{
							const auto componentId = utils::unaligned_ref<uint32_t>((void*)&ctx.data[ctx.dataOffset]);
							const auto& info = GetComponentCache().GetComponentInfo(componentId);
							ctx.dataOffset += sizeof(uint32_t);

							auto* pComponentDataStart = pChunk->GetDataPtrRW<false>(componentType, info.componentId);
							auto* pComponentData = (void*)&pComponentDataStart[(size_t)indexInChunk * info.properties.size];
							memcpy(pComponentData, (const void*)&ctx.data[ctx.dataOffset], info.properties.size);
							ctx.dataOffset += info.properties.size;
						}
					},
					// SET_COMPONENT_FOR_TEMPENTITY
					[](CommandBufferCtx& ctx) {
						// Type
						ComponentType componentType = (ComponentType)ctx.data[ctx.dataOffset];
						ctx.dataOffset += sizeof(ComponentType);

						// Entity
						Entity e = utils::unaligned_ref<Entity>((void*)&ctx.data[ctx.dataOffset]);
						ctx.dataOffset += sizeof(Entity);

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
							const auto componentId = utils::unaligned_ref<uint32_t>((void*)&ctx.data[ctx.dataOffset]);
							const auto& info = GetComponentCache().GetComponentInfo(componentId);
							ctx.dataOffset += sizeof(uint32_t);

							auto* pComponentDataStart = pChunk->GetDataPtrRW<false>(componentType, info.componentId);
							auto* pComponentData = (void*)&pComponentDataStart[(size_t)indexInChunk * info.properties.size];
							memcpy(pComponentData, (const void*)&ctx.data[ctx.dataOffset], info.properties.size);
							ctx.dataOffset += info.properties.size;
						}
					},
					// REMOVE_COMPONENT
					[](CommandBufferCtx& ctx) {
						// Type
						ComponentType componentType = utils::unaligned_ref<ComponentType>((void*)&ctx.data[ctx.dataOffset]);
						ctx.dataOffset += sizeof(ComponentType);

						// Entity
						Entity e = utils::unaligned_ref<Entity>((void*)&ctx.data[ctx.dataOffset]);
						ctx.dataOffset += sizeof(Entity);

						// Components
						uint32_t componentId = utils::unaligned_ref<uint32_t>((void*)&ctx.data[ctx.dataOffset]);
						const auto& newInfo = GetComponentCache().GetComponentInfo(componentId);
						ctx.dataOffset += sizeof(uint32_t);

						ctx.world.RemoveComponent_Internal(componentType, e, newInfo);
					}};

		public:
			/*!
			Commits all queued changes.
			*/
			void Commit() {
				// Extract data from the buffer
				CommandBufferCtx ctx{m_world, m_data, 0, 0, {}};
				while (ctx.dataOffset < m_data.size())
					CommandBufferCmd[m_data[ctx.dataOffset++]](ctx);

				m_entities = 0;
				m_data.clear();
			} // namespace ecs
		}; // namespace gaia
	} // namespace ecs
} // namespace gaia
