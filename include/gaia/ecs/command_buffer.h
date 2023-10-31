#pragma once
#include "../config/config.h"

#include <cinttypes>
#include <type_traits>

#include "../cnt/map.h"
#include "../cnt/sarray_ext.h"
#include "../ser/serialization.h"
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

		Adding and removing components and entities inside World::each or can result
		in changes of archetypes or chunk structure. This would lead to undefined behavior.
		Therefore, such operations have to be executed after the loop is done.
		*/
		class CommandBuffer final {
			struct CommandBufferCtx: SerializationBuffer {
				ecs::World& world;
				uint32_t entities;
				cnt::map<uint32_t, Entity> entityMap;

				CommandBufferCtx(ecs::World& w): world(w), entities(0) {}

				using SerializationBuffer::reset;
				void reset() {
					SerializationBuffer::reset();
					entities = 0;
					entityMap.clear();
				}
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

				void commit(CommandBufferCtx& ctx) const {
					auto* pArchetype = (Archetype*)archetypePtr;
					[[maybe_unused]] const auto res = ctx.entityMap.try_emplace(ctx.entities++, ctx.world.add(*pArchetype));
					GAIA_ASSERT(res.second);
				}
			};
			struct CREATE_ENTITY_FROM_ENTITY_t: CommandBufferCmd_t {
				Entity entity;

				void commit(CommandBufferCtx& ctx) const {
					[[maybe_unused]] const auto res = ctx.entityMap.try_emplace(ctx.entities++, ctx.world.add(entity));
					GAIA_ASSERT(res.second);
				}
			};
			struct DELETE_ENTITY_t: CommandBufferCmd_t {
				Entity entity;

				void commit(CommandBufferCtx& ctx) const {
					ctx.world.del(entity);
				}
			};
			struct ADD_COMPONENT_t: CommandBufferCmd_t {
				Entity entity;
				ComponentId compId;
				ComponentKind compKind;

				void commit(CommandBufferCtx& ctx) const {
					const auto& info = ComponentCache::get().comp_info(compId);
					ctx.world.add_inter(compKind, entity, info);

#if GAIA_ASSERT_ENABLED
					[[maybe_unused]] uint32_t indexInChunk{};
					[[maybe_unused]] auto* pChunk = ctx.world.get_chunk(entity, indexInChunk);
					GAIA_ASSERT(pChunk != nullptr);
#endif
				}
			};
			struct ADD_COMPONENT_DATA_t: CommandBufferCmd_t {
				Entity entity;
				ComponentId compId;
				ComponentKind compKind;

				void commit(CommandBufferCtx& ctx) const {
					const auto& info = ComponentCache::get().comp_info(compId);
					ctx.world.add_inter(compKind, entity, info);

					uint32_t indexInChunk{};
					auto* pChunk = ctx.world.get_chunk(entity, indexInChunk);
					GAIA_ASSERT(pChunk != nullptr);

					if (compKind == ComponentKind::CK_Chunk)
						indexInChunk = 0;

					// Component data
					const auto& desc = ComponentCache::get().comp_desc(compId);
					const auto compIdx = pChunk->comp_idx(compKind, compId);
					const auto offset = pChunk->data_offset(compKind, compIdx);
					auto* pComponentData = (void*)&pChunk->data(offset + (uint32_t)indexInChunk * desc.properties.size);
					ctx.load_comp(pComponentData, compId);
				}
			};
			struct ADD_COMPONENT_TO_TEMPENTITY_t: CommandBufferCmd_t {
				TempEntity tempEntity;
				ComponentId compId;
				ComponentKind compKind;

				void commit(CommandBufferCtx& ctx) const {
					// For delayed entities we have to do a look in our map
					// of temporaries and find a link there
					const auto it = ctx.entityMap.find(tempEntity.id);
					// Link has to exist!
					GAIA_ASSERT(it != ctx.entityMap.end());

					Entity entity = it->second;

					const auto& info = ComponentCache::get().comp_info(compId);
					ctx.world.add_inter(compKind, entity, info);

#if GAIA_ASSERT_ENABLED
					[[maybe_unused]] uint32_t indexInChunk{};
					[[maybe_unused]] auto* pChunk = ctx.world.get_chunk(entity, indexInChunk);
					GAIA_ASSERT(pChunk != nullptr);
#endif
				}
			};
			struct ADD_COMPONENT_TO_TEMPENTITY_DATA_t: CommandBufferCmd_t {
				TempEntity tempEntity;
				ComponentId compId;
				ComponentKind compKind;

				void commit(CommandBufferCtx& ctx) const {
					// For delayed entities we have to do a look in our map
					// of temporaries and find a link there
					const auto it = ctx.entityMap.find(tempEntity.id);
					// Link has to exist!
					GAIA_ASSERT(it != ctx.entityMap.end());

					Entity entity = it->second;

					// Components
					const auto& info = ComponentCache::get().comp_info(compId);
					ctx.world.add_inter(compKind, entity, info);

					uint32_t indexInChunk{};
					auto* pChunk = ctx.world.get_chunk(entity, indexInChunk);
					GAIA_ASSERT(pChunk != nullptr);

					if (compKind == ComponentKind::CK_Chunk)
						indexInChunk = 0;

					// Component data
					const auto& desc = ComponentCache::get().comp_desc(compId);
					const auto compIdx = pChunk->comp_idx(compKind, compId);
					const auto offset = pChunk->data_offset(compKind, compIdx);
					auto* pComponentData = (void*)&pChunk->data(offset + (uint32_t)indexInChunk * desc.properties.size);
					ctx.load_comp(pComponentData, compId);
				}
			};
			struct SET_COMPONENT_t: CommandBufferCmd_t {
				Entity entity;
				ComponentId compId;
				ComponentKind compKind;

				void commit(CommandBufferCtx& ctx) const {
					const auto& entityContainer = ctx.world.m_entities[entity.id()];
					auto* pChunk = entityContainer.pChunk;
					const auto indexInChunk = compKind == ComponentKind::CK_Chunk ? 0U : entityContainer.idx;

					// Component data
					const auto& desc = ComponentCache::get().comp_desc(compId);
					const auto compIdx = pChunk->comp_idx(compKind, compId);
					const auto offset = pChunk->data_offset(compKind, compIdx);
					auto* pComponentData = (void*)&pChunk->data(offset + (uint32_t)indexInChunk * desc.properties.size);
					ctx.load_comp(pComponentData, compId);
				}
			};
			struct SET_COMPONENT_FOR_TEMPENTITY_t: CommandBufferCmd_t {
				TempEntity tempEntity;
				ComponentId compId;
				ComponentKind compKind;

				void commit(CommandBufferCtx& ctx) const {
					// For delayed entities we have to do a look in our map
					// of temporaries and find a link there
					const auto it = ctx.entityMap.find(tempEntity.id);
					// Link has to exist!
					GAIA_ASSERT(it != ctx.entityMap.end());

					Entity entity = it->second;

					const auto& entityContainer = ctx.world.m_entities[entity.id()];
					auto* pChunk = entityContainer.pChunk;
					const auto indexInChunk = compKind == ComponentKind::CK_Chunk ? 0U : entityContainer.idx;

					// Component data
					const auto& desc = ComponentCache::get().comp_desc(compId);
					const auto compIdx = pChunk->comp_idx(compKind, compId);
					const auto offset = pChunk->data_offset(compKind, compIdx);
					auto* pComponentData = (void*)&pChunk->data(offset + (uint32_t)indexInChunk * desc.properties.size);
					ctx.load_comp(pComponentData, compId);
				}
			};
			struct REMOVE_COMPONENT_t: CommandBufferCmd_t {
				Entity entity;
				ComponentId compId;
				ComponentKind compKind;

				void commit(CommandBufferCtx& ctx) const {
					const auto& info = ComponentCache::get().comp_info(compId);
					ctx.world.del_inter(compKind, entity, info);
				}
			};

			friend class World;

			CommandBufferCtx m_ctx;
			uint32_t m_entities;

			/*!
			Requests a new entity to be created from archetype
			\return Entity that will be created. The id is not usable right away. It
			will be filled with proper data after commit()
			*/
			GAIA_NODISCARD TempEntity add(Archetype& archetype) {
				m_ctx.save(CREATE_ENTITY_FROM_ARCHETYPE);

				CREATE_ENTITY_FROM_ARCHETYPE_t cmd;
				cmd.archetypePtr = (uintptr_t)&archetype;
				ser::save(m_ctx, cmd);

				return {m_entities++};
			}

		public:
			CommandBuffer(World& world): m_ctx(world), m_entities(0) {}
			~CommandBuffer() = default;

			CommandBuffer(CommandBuffer&&) = delete;
			CommandBuffer(const CommandBuffer&) = delete;
			CommandBuffer& operator=(CommandBuffer&&) = delete;
			CommandBuffer& operator=(const CommandBuffer&) = delete;

			/*!
			Requests a new entity to be created
			\return Entity that will be created. The id is not usable right away. It
			will be filled with proper data after commit()
			*/
			GAIA_NODISCARD TempEntity add() {
				m_ctx.save(CREATE_ENTITY);

				return {m_entities++};
			}

			/*!
			Requests a new entity to be created by cloning an already existing
			entity \return Entity that will be created. The id is not usable right
			away. It will be filled with proper data after commit()
			*/
			GAIA_NODISCARD TempEntity add(Entity entityFrom) {
				m_ctx.save(CREATE_ENTITY_FROM_ENTITY);

				CREATE_ENTITY_FROM_ENTITY_t cmd;
				cmd.entity = entityFrom;
				ser::save(m_ctx, cmd);

				return {m_entities++};
			}

			/*!
			Requests an existing \param entity to be removed.
			*/
			void del(Entity entity) {
				m_ctx.save(DELETE_ENTITY);

				DELETE_ENTITY_t cmd;
				cmd.entity = entity;
				ser::save(m_ctx, cmd);
			}

			/*!
			Requests a component to be added to entity.
			*/
			template <typename T>
			void add(Entity entity) {
				// Make sure the component is registered
				const auto& info = ComponentCache::get().goc_comp_info<T>();

				using U = typename component_type_t<T>::Type;
				verify_comp<U>();

				m_ctx.save(ADD_COMPONENT);

				ADD_COMPONENT_t cmd;
				cmd.entity = entity;
				cmd.compKind = component_kind_v<T>;
				cmd.compId = info.compId;
				ser::save(m_ctx, cmd);
			}

			/*!
			Requests a component to be added to temporary entity.
			*/
			template <typename T>
			void add(TempEntity entity) {
				// Make sure the component is registered
				const auto& info = ComponentCache::get().goc_comp_info<T>();

				using U = typename component_type_t<T>::Type;
				verify_comp<U>();

				m_ctx.save(ADD_COMPONENT_TO_TEMPENTITY);

				ADD_COMPONENT_TO_TEMPENTITY_t cmd;
				cmd.tempEntity = entity;
				cmd.compKind = component_kind_v<T>;
				cmd.compId = info.compId;
				ser::save(m_ctx, cmd);
			}

			/*!
			Requests a component to be added to entity.
			*/
			template <typename T>
			void add(Entity entity, T&& value) {
				// Make sure the component is registered
				const auto& info = ComponentCache::get().goc_comp_info<T>();

				using U = typename component_type_t<T>::Type;
				verify_comp<U>();

				m_ctx.save(ADD_COMPONENT_DATA);

				ADD_COMPONENT_DATA_t cmd;
				cmd.entity = entity;
				cmd.compKind = component_kind_v<T>;
				cmd.compId = info.compId;
				ser::save(m_ctx, cmd);
				m_ctx.save_comp(GAIA_FWD(value));
			}

			/*!
			Requests a component to be added to temporary entity.
			*/
			template <typename T>
			void add(TempEntity entity, T&& value) {
				// Make sure the component is registered
				const auto& info = ComponentCache::get().goc_comp_info<T>();

				using U = typename component_type_t<T>::Type;
				verify_comp<U>();

				m_ctx.save(ADD_COMPONENT_TO_TEMPENTITY_DATA);

				ADD_COMPONENT_TO_TEMPENTITY_t cmd;
				cmd.tempEntity = entity;
				cmd.compKind = component_kind_v<T>;
				cmd.compId = info.compId;
				ser::save(m_ctx, cmd);
				m_ctx.save_comp(GAIA_FWD(value));
			}

			/*!
			Requests component data to be set to given values for a given entity.
			*/
			template <typename T>
			void set(Entity entity, T&& value) {
				// No need to check if the component is registered.
				// If we want to set the value of a component we must have created it already.
				// (void)ComponentCache::get().comp_info<T>();

				using U = typename component_type_t<T>::Type;
				verify_comp<U>();

				m_ctx.save(SET_COMPONENT);

				SET_COMPONENT_t cmd;
				cmd.entity = entity;
				cmd.compKind = component_kind_v<T>;
				cmd.compId = comp_id<T>();
				ser::save(m_ctx, cmd);
				m_ctx.save_comp(GAIA_FWD(value));
			}

			/*!
			Requests component data to be set to given values for a given temp
			entity.
			*/
			template <typename T>
			void set(TempEntity entity, T&& value) {
				// No need to check if the component is registered.
				// If we want to set the value of a component we must have created it already.
				// (void)ComponentCache::get().goc_comp_info<T>();

				using U = typename component_type_t<T>::Type;
				verify_comp<U>();

				m_ctx.save(SET_COMPONENT_FOR_TEMPENTITY);

				SET_COMPONENT_FOR_TEMPENTITY_t cmd;
				cmd.tempEntity = entity;
				cmd.compKind = component_kind_v<T>;
				cmd.compId = comp_id<T>();
				ser::save(m_ctx, cmd);
				m_ctx.save_comp(GAIA_FWD(value));
			}

			/*!
			Requests removal of a component from entity
			*/
			template <typename T>
			void del(Entity entity) {
				// No need to check if the component is registered.
				// If we want to remove a component we must have created it already.
				// (void)ComponentCache::get().goc_comp_info<T>();

				using U = typename component_type_t<T>::Type;
				verify_comp<U>();

				m_ctx.save(REMOVE_COMPONENT);

				REMOVE_COMPONENT_t cmd;
				cmd.entity = entity;
				cmd.compKind = component_kind_v<T>;
				cmd.compId = comp_id<T>();
				ser::save(m_ctx, cmd);
			}

		private:
			using CommandBufferReadFunc = void (*)(CommandBufferCtx& ctx);
			static constexpr CommandBufferReadFunc CommandBufferRead[] = {
					// CREATE_ENTITY
					[](CommandBufferCtx& ctx) {
						[[maybe_unused]] const auto res = ctx.entityMap.try_emplace(ctx.entities++, ctx.world.add());
						GAIA_ASSERT(res.second);
					},
					// CREATE_ENTITY_FROM_ARCHETYPE
					[](CommandBufferCtx& ctx) {
						CREATE_ENTITY_FROM_ARCHETYPE_t cmd;
						ser::load(ctx, cmd);
						cmd.commit(ctx);
					},
					// CREATE_ENTITY_FROM_ENTITY
					[](CommandBufferCtx& ctx) {
						CREATE_ENTITY_FROM_ENTITY_t cmd;
						ser::load(ctx, cmd);
						cmd.commit(ctx);
					},
					// DELETE_ENTITY
					[](CommandBufferCtx& ctx) {
						DELETE_ENTITY_t cmd;
						ser::load(ctx, cmd);
						cmd.commit(ctx);
					},
					// ADD_COMPONENT
					[](CommandBufferCtx& ctx) {
						ADD_COMPONENT_t cmd;
						ser::load(ctx, cmd);
						cmd.commit(ctx);
					},
					// ADD_COMPONENT_DATA
					[](CommandBufferCtx& ctx) {
						ADD_COMPONENT_DATA_t cmd;
						ser::load(ctx, cmd);
						cmd.commit(ctx);
					},
					// ADD_COMPONENT_TO_TEMPENTITY
					[](CommandBufferCtx& ctx) {
						ADD_COMPONENT_TO_TEMPENTITY_t cmd;
						ser::load(ctx, cmd);
						cmd.commit(ctx);
					},
					// ADD_COMPONENT_TO_TEMPENTITY_DATA
					[](CommandBufferCtx& ctx) {
						ADD_COMPONENT_TO_TEMPENTITY_DATA_t cmd;
						ser::load(ctx, cmd);
						cmd.commit(ctx);
					},
					// SET_COMPONENT
					[](CommandBufferCtx& ctx) {
						SET_COMPONENT_t cmd;
						ser::load(ctx, cmd);
						cmd.commit(ctx);
					},
					// SET_COMPONENT_FOR_TEMPENTITY
					[](CommandBufferCtx& ctx) {
						SET_COMPONENT_FOR_TEMPENTITY_t cmd;
						ser::load(ctx, cmd);
						cmd.commit(ctx);
					},
					// REMOVE_COMPONENT
					[](CommandBufferCtx& ctx) {
						REMOVE_COMPONENT_t cmd;
						ser::load(ctx, cmd);
						cmd.commit(ctx);
					}};

		public:
			/*!
			Commits all queued changes.
			*/
			void commit() {
				if (m_ctx.empty())
					return;

				// Extract data from the buffer
				m_ctx.seek(0);
				while (m_ctx.tell() < m_ctx.bytes()) {
					CommandBufferCmd id{};
					m_ctx.load(id);
					CommandBufferRead[id](m_ctx);
				}

				m_entities = 0;
				m_ctx.reset();
			} // namespace ecs
		}; // namespace gaia
	} // namespace ecs
} // namespace gaia
