#pragma once
#include "../config/config.h"

#include <cstdint>
#include <type_traits>

#include "../cnt/map.h"
#include "../cnt/sarray_ext.h"
#include "../mt/spinlock.h"
#include "../ser/serialization.h"
#include "archetype.h"
#include "command_buffer_fwd.h"
#include "common.h"
#include "component.h"
#include "component_cache.h"
#include "data_buffer.h"
#include "id.h"
#include "world.h"

namespace gaia {
	namespace ecs {
		struct TempEntity final {
			uint32_t id;
		};

		struct AccessContextST {
			void lock() {}
			void unlock() {}
		};

		struct AccessContextMT {
			mt::SpinLock m_lock;

			void lock() {
				m_lock.lock();
			}

			void unlock() {
				m_lock.unlock();
			}
		};

		namespace detail {
			//! Buffer for deferred execution of some operations on entities.
			//!
			//! Adding and removing components and entities inside World::each or can result
			//! in changes of archetypes or chunk structure. This would lead to undefined behavior.
			//! Therefore, such operations have to be executed after the loop is done.
			template <typename AccessContext>
			class CommandBuffer final {
				struct CommandBufferCtx: SerializationBuffer {
					uint32_t entities = 0;
					ecs::World& world;
					cnt::map<uint32_t, Entity> entityMap;

					CommandBufferCtx(ecs::World& w): world(w) {}

					using SerializationBuffer::reset;
					void reset() {
						SerializationBuffer::reset();
						entities = 0;
						entityMap.clear();
					}
				};

				enum CommandBufferCmdType : uint8_t {
					CREATE_ENTITY,
					CREATE_ENTITY_FROM_ARCHETYPE,
					CREATE_ENTITY_FROM_ENTITY,
					DELETE_ENTITY,
					ADD_COMPONENT,
					ADD_COMPONENT_10,
					ADD_COMPONENT_01,
					ADD_COMPONENT_00,
					ADD_COMPONENT_DATA,
					ADD_COMPONENT_TO_TEMPENTITY_DATA,
					SET_COMPONENT,
					SET_COMPONENT_FOR_TEMPENTITY,
					REMOVE_COMPONENT,
					REMOVE_COMPONENT_10,
					REMOVE_COMPONENT_01,
					REMOVE_COMPONENT_00
				};

				struct CommandBufferCmd {
					CommandBufferCmdType id;
				};

				struct CreateEntityCmd: CommandBufferCmd {};
				struct CreateEntityFromArchetypeCmd: CommandBufferCmd {
					uint64_t archetypePtr;

					void commit(CommandBufferCtx& ctx) const {
						auto* pArchetype = (Archetype*)archetypePtr;
						[[maybe_unused]] const auto res =
								ctx.entityMap.try_emplace(ctx.entities++, ctx.world.add(*pArchetype, true, false, EntityKind::EK_Gen));
						GAIA_ASSERT(res.second);
					}
				};
				struct CreateEntityFromEntityCmd: CommandBufferCmd {
					Entity entity;

					void commit(CommandBufferCtx& ctx) const {
						[[maybe_unused]] const auto res = ctx.entityMap.try_emplace(ctx.entities++, ctx.world.copy(entity));
						GAIA_ASSERT(res.second);
					}
				};
				struct DeleteEntityCmd: CommandBufferCmd {
					Entity entity;

					void commit(CommandBufferCtx& ctx) const {
						ctx.world.del(entity);
					}
				};
				struct AddComponentCmd: CommandBufferCmd {
					Entity entity;
					Entity object;

					void commit(CommandBufferCtx& ctx) const {
						World::EntityBuilder(ctx.world, entity).add(object);

#if GAIA_ASSERT_ENABLED
						[[maybe_unused]] uint32_t indexInChunk{};
						[[maybe_unused]] auto* pChunk = ctx.world.get_chunk(entity, indexInChunk);
						GAIA_ASSERT(pChunk != nullptr);
#endif
					}
				};
				struct AddComponent10Cmd: CommandBufferCmd {
					Entity entity;
					TempEntity tempObject;

					void commit(CommandBufferCtx& ctx) const {
						// For delayed entities we have to do a look in our map
						// of temporaries and find a link there
						const auto it = ctx.entityMap.find(tempObject.id);
						// Link has to exist!
						GAIA_ASSERT(it != ctx.entityMap.end());

						Entity object = it->second;
						World::EntityBuilder(ctx.world, entity).add(object);

#if GAIA_ASSERT_ENABLED
						[[maybe_unused]] uint32_t indexInChunk{};
						[[maybe_unused]] auto* pChunk = ctx.world.get_chunk(entity, indexInChunk);
						GAIA_ASSERT(pChunk != nullptr);
#endif
					}
				};
				struct AddComponent01Cmd: CommandBufferCmd {
					TempEntity tempEntity;
					Entity object;

					void commit(CommandBufferCtx& ctx) const {
						// For delayed entities we have to do a look in our map
						// of temporaries and find a link there
						const auto it = ctx.entityMap.find(tempEntity.id);
						// Link has to exist!
						GAIA_ASSERT(it != ctx.entityMap.end());

						Entity entity = it->second;
						World::EntityBuilder(ctx.world, entity).add(object);

#if GAIA_ASSERT_ENABLED
						[[maybe_unused]] uint32_t indexInChunk{};
						[[maybe_unused]] auto* pChunk = ctx.world.get_chunk(entity, indexInChunk);
						GAIA_ASSERT(pChunk != nullptr);
#endif
					}
				};
				struct AddComponent00Cmd: CommandBufferCmd {
					TempEntity tempEntity;
					TempEntity tempObject;

					void commit(CommandBufferCtx& ctx) const {
						Entity entity, object;
						{
							// For delayed entities we have to do a look in our map
							// of temporaries and find a link there
							const auto it = ctx.entityMap.find(tempEntity.id);
							// Link has to exist!
							GAIA_ASSERT(it != ctx.entityMap.end());
							entity = it->second;
						}
						{
							// For delayed entities we have to do a look in our map
							// of temporaries and find a link there
							const auto it = ctx.entityMap.find(tempObject.id);
							// Link has to exist!
							GAIA_ASSERT(it != ctx.entityMap.end());
							object = it->second;
						}

						World::EntityBuilder(ctx.world, entity).add(object);

#if GAIA_ASSERT_ENABLED
						[[maybe_unused]] uint32_t indexInChunk{};
						[[maybe_unused]] auto* pChunk = ctx.world.get_chunk(entity, indexInChunk);
						GAIA_ASSERT(pChunk != nullptr);
#endif
					}
				};

				struct AddComponentWithDataCmd: CommandBufferCmd {
					Entity entity;
					Entity object;

					void commit(CommandBufferCtx& ctx) const {
						World::EntityBuilder(ctx.world, entity).add(object);

						uint32_t indexInChunk{};
						auto* pChunk = ctx.world.get_chunk(entity, indexInChunk);
						GAIA_ASSERT(pChunk != nullptr);

						if (object.kind() == EntityKind::EK_Uni)
							indexInChunk = 0;

						// Component data
						const auto compIdx = pChunk->comp_idx(object);
						auto* pComponentData = (void*)pChunk->comp_ptr_mut(compIdx, indexInChunk);
						ctx.load_comp(ctx.world.comp_cache(), pComponentData, object);
					}
				};
				struct AddComponentWithDataToTempEntityCmd: CommandBufferCmd {
					TempEntity tempEntity;
					Entity object;

					void commit(CommandBufferCtx& ctx) const {
						// For delayed entities we have to do a look in our map
						// of temporaries and find a link there
						const auto it = ctx.entityMap.find(tempEntity.id);
						// Link has to exist!
						GAIA_ASSERT(it != ctx.entityMap.end());

						Entity entity = it->second;
						World::EntityBuilder(ctx.world, entity).add(object);

						uint32_t indexInChunk{};
						auto* pChunk = ctx.world.get_chunk(entity, indexInChunk);
						GAIA_ASSERT(pChunk != nullptr);

						if (object.kind() == EntityKind::EK_Uni)
							indexInChunk = 0;

						// Component data
						const auto compIdx = pChunk->comp_idx(object);
						auto* pComponentData = (void*)pChunk->comp_ptr_mut(compIdx, indexInChunk);
						ctx.load_comp(ctx.world.comp_cache(), pComponentData, object);
					}
				};
				struct SetComponentCmd: CommandBufferCmd {
					Entity entity;
					Entity object;

					void commit(CommandBufferCtx& ctx) const {
						const auto& ec = ctx.world.m_recs.entities[entity.id()];
						const auto row = object.kind() == EntityKind::EK_Uni ? 0U : ec.row;

						// Component data
						const auto compIdx = ec.pChunk->comp_idx(object);
						auto* pComponentData = (void*)ec.pChunk->comp_ptr_mut(compIdx, row);
						ctx.load_comp(ctx.world.comp_cache(), pComponentData, object);
					}
				};
				struct SetComponentOnTempEntityCmd: CommandBufferCmd {
					TempEntity tempEntity;
					Entity object;

					void commit(CommandBufferCtx& ctx) const {
						// For delayed entities we have to do a look in our map
						// of temporaries and find a link there
						const auto it = ctx.entityMap.find(tempEntity.id);
						// Link has to exist!
						GAIA_ASSERT(it != ctx.entityMap.end());

						Entity entity = it->second;

						const auto& ec = ctx.world.m_recs.entities[entity.id()];
						const auto row = object.kind() == EntityKind::EK_Uni ? 0U : ec.row;

						// Component data
						const auto compIdx = ec.pChunk->comp_idx(object);
						auto* pComponentData = (void*)ec.pChunk->comp_ptr_mut(compIdx, row);
						ctx.load_comp(ctx.world.comp_cache(), pComponentData, object);
					}
				};
				struct RemoveComponentCmd: CommandBufferCmd {
					Entity entity;
					Entity object;

					void commit(CommandBufferCtx& ctx) const {
						World::EntityBuilder(ctx.world, entity).del(object);
					}
				};
				struct RemoveComponent10Cmd: CommandBufferCmd {
					Entity entity;
					TempEntity tempObject;

					void commit(CommandBufferCtx& ctx) const {
						// For delayed entities we have to do a look in our map
						// of temporaries and find a link there
						const auto it = ctx.entityMap.find(tempObject.id);
						// Link has to exist!
						GAIA_ASSERT(it != ctx.entityMap.end());
						Entity object = it->second;

						World::EntityBuilder(ctx.world, entity).del(object);
					}
				};
				struct RemoveComponent01Cmd: CommandBufferCmd {
					TempEntity tempEntity;
					Entity object;

					void commit(CommandBufferCtx& ctx) const {
						// For delayed entities we have to do a look in our map
						// of temporaries and find a link there
						const auto it = ctx.entityMap.find(tempEntity.id);
						// Link has to exist!
						GAIA_ASSERT(it != ctx.entityMap.end());
						Entity entity = it->second;

						World::EntityBuilder(ctx.world, entity).del(object);
					}
				};
				struct RemoveComponent00Cmd: CommandBufferCmd {
					TempEntity tempEntity;
					TempEntity tempObject;

					void commit(CommandBufferCtx& ctx) const {
						Entity entity, object;
						{
							// For delayed entities we have to do a look in our map
							// of temporaries and find a link there
							const auto it = ctx.entityMap.find(tempObject.id);
							// Link has to exist!
							GAIA_ASSERT(it != ctx.entityMap.end());
							entity = it->second;
						}
						{
							// For delayed entities we have to do a look in our map
							// of temporaries and find a link there
							const auto it = ctx.entityMap.find(tempObject.id);
							// Link has to exist!
							GAIA_ASSERT(it != ctx.entityMap.end());
							object = it->second;
						}
						World::EntityBuilder(ctx.world, entity).del(object);
					}
				};

				friend class World;

				AccessContext m_acc;
				CommandBufferCtx m_ctx;
				uint32_t m_entities = 0;

				//! Requests a new entity to be created from archetype
				//! \return Entity that will be created. The id is not usable right away. It
				//!         will be filled with proper data after commit(),
				GAIA_NODISCARD TempEntity add(Archetype& archetype) {
					m_ctx.save(CREATE_ENTITY_FROM_ARCHETYPE);

					CreateEntityFromArchetypeCmd cmd;
					cmd.archetypePtr = (uintptr_t)&archetype;
					ser::save(m_ctx, cmd);

					return {m_entities++};
				}

			public:
				explicit CommandBuffer(World& world): m_ctx(world) {}
				~CommandBuffer() = default;

				CommandBuffer(CommandBuffer&&) = delete;
				CommandBuffer(const CommandBuffer&) = delete;
				CommandBuffer& operator=(CommandBuffer&&) = delete;
				CommandBuffer& operator=(const CommandBuffer&) = delete;

				//! Requests a new entity to be created
				//! \return Entity that will be created. The id is not usable right away. It
				//!         will be filled with proper data after commit().
				GAIA_NODISCARD TempEntity add() {
					core::lock_scope lock(m_acc);

					m_ctx.save(CREATE_ENTITY);

					return {m_entities++};
				}

				//! Requests a new entity to be created by cloning an already existing entity
				//! \return Entity that will be created. The id is not usable right away. It
				//!         will be filled with proper data after commit()
				GAIA_NODISCARD TempEntity copy(Entity entityFrom) {
					core::lock_scope lock(m_acc);

					m_ctx.save(CREATE_ENTITY_FROM_ENTITY);

					CreateEntityFromEntityCmd cmd;
					cmd.entity = entityFrom;
					ser::save(m_ctx, cmd);

					return {m_entities++};
				}

				//! Requests an existing \param entity to be removed.
				void del(Entity entity) {
					core::lock_scope lock(m_acc);

					m_ctx.save(DELETE_ENTITY);

					DeleteEntityCmd cmd;
					cmd.entity = entity;
					ser::save(m_ctx, cmd);
				}

				//! Requests a component \tparam T to be added to \param entity.
				template <typename T>
				void add(Entity entity) {
					verify_comp<T>();
					core::lock_scope lock(m_acc);

					// Make sure the component is registered
					const auto& item = comp_cache_add<T>(m_ctx.world);

					add(entity, item.entity);
				}

				//! Requests a component \tparam T to be added to a temporary entity.
				template <typename T>
				void add(TempEntity entity) {
					verify_comp<T>();
					core::lock_scope lock(m_acc);

					// Make sure the component is registered
					const auto& item = comp_cache_add<T>(m_ctx.world);

					add(entity, item.entity);
				}

				//! Requests an entity \param other to be added to entity \param entity.
				void add(Entity entity, Entity other) {
					core::lock_scope lock(m_acc);

					m_ctx.save(ADD_COMPONENT);

					AddComponentCmd cmd;
					cmd.entity = entity;
					cmd.object = other;
					ser::save(m_ctx, cmd);
				}

				//! Requests a temporary entity \param other to be added to entity \param entity.
				void add(Entity entity, TempEntity other) {
					core::lock_scope lock(m_acc);

					m_ctx.save(ADD_COMPONENT_10);

					AddComponent10Cmd cmd;
					cmd.entity = entity;
					cmd.tempObject = other;
					ser::save(m_ctx, cmd);
				}

				//! Requests a temporary entity \param entity to be added to entity \param other.
				void add(TempEntity entity, Entity other) {
					core::lock_scope lock(m_acc);

					m_ctx.save(ADD_COMPONENT_01);

					AddComponent01Cmd cmd;
					cmd.tempEntity = entity;
					cmd.object = other;
					ser::save(m_ctx, cmd);
				}

				//! Requests a temporary entity \param entity to be added to a temporary entity \param other.
				void add(TempEntity entity, TempEntity other) {
					core::lock_scope lock(m_acc);

					m_ctx.save(ADD_COMPONENT_00);

					AddComponent00Cmd cmd;
					cmd.tempEntity = entity;
					cmd.tempObject = other;
					ser::save(m_ctx, cmd);
				}

				//! Requests a component \tparam T to be added to entity. Also sets its value.
				//! \warning Component \tparam T should be registered in the world before calling this function.
				//!          If used in concurrent environment, race conditions may occur otherwise.
				template <typename T>
				void add(Entity entity, T&& value) {
					verify_comp<T>();
					core::lock_scope lock(m_acc);

					// Make sure the component is registered
					const auto& item = comp_cache_add<T>(m_ctx.world);

					m_ctx.save(ADD_COMPONENT_DATA);

					AddComponentWithDataCmd cmd;
					cmd.entity = entity;
					cmd.object = item.entity;
					ser::save(m_ctx, cmd);
					m_ctx.save_comp(item, GAIA_FWD(value));
				}

				//! Requests a component \tparam T to be added to a temporary entity. Also sets its value.
				//! \warning Component \tparam T should be registered in the world before calling this function.
				//!          If used in concurrent environment, race conditions may occur otherwise.
				template <typename T>
				void add(TempEntity entity, T&& value) {
					verify_comp<T>();
					core::lock_scope lock(m_acc);

					// Make sure the component is registered
					const auto& item = comp_cache_add<T>(m_ctx.world);

					m_ctx.save(ADD_COMPONENT_TO_TEMPENTITY_DATA);

					AddComponent01Cmd cmd;
					cmd.tempEntity = entity;
					cmd.object = item.entity;
					ser::save(m_ctx, cmd);
					m_ctx.save_comp(item, GAIA_FWD(value));
				}

				//! Requests component data to be set to given values for a given entity.
				//! \warning Component \tparam T must be registered in the world before calling this function.
				//!          Calling set without a previous add of the component doesn't make sense.
				template <typename T>
				void set(Entity entity, T&& value) {
					verify_comp<T>();
					core::lock_scope lock(m_acc);

					// Make sure the component is registered
					const auto& item = comp_cache(m_ctx.world).template get<T>();

					m_ctx.save(SET_COMPONENT);

					SetComponentCmd cmd;
					cmd.entity = entity;
					cmd.object = item.entity;
					ser::save(m_ctx, cmd);
					m_ctx.save_comp(item, GAIA_FWD(value));
				}

				//! Requests component data to be set to given values for a given temp entity.
				//! \warning Component \tparam T must be registered in the world before calling this function.
				//!          Calling set without a previous add of the component doesn't make sense.
				template <typename T>
				void set(TempEntity entity, T&& value) {
					verify_comp<T>();
					core::lock_scope lock(m_acc);

					// Make sure the component is registered
					const auto& item = comp_cache(m_ctx.world).template get<T>();

					m_ctx.save(SET_COMPONENT_FOR_TEMPENTITY);

					SetComponentOnTempEntityCmd cmd;
					cmd.tempEntity = entity;
					cmd.object = item.entity;
					ser::save(m_ctx, cmd);
					m_ctx.save_comp(item, GAIA_FWD(value));
				}

				//! Requests removal of component \tparam T from \param entity.
				void del(Entity entity, Entity object) {
					m_ctx.save(REMOVE_COMPONENT);

					RemoveComponentCmd cmd;
					cmd.entity = entity;
					cmd.object = object;
					ser::save(m_ctx, cmd);
				}

				//! Requests removal of component \tparam T from \param entity.
				void del(Entity entity, TempEntity object) {
					m_ctx.save(REMOVE_COMPONENT_10);

					RemoveComponent10Cmd cmd;
					cmd.entity = entity;
					cmd.tempObject = object;
					ser::save(m_ctx, cmd);
				}

				//! Requests removal of component \tparam T from \param entity.
				void del(TempEntity entity, Entity object) {
					m_ctx.save(REMOVE_COMPONENT_01);

					RemoveComponent01Cmd cmd;
					cmd.tempEntity = entity;
					cmd.object = object;
					ser::save(m_ctx, cmd);
				}

				//! Requests removal of component \tparam T from \param entity.
				void del(TempEntity entity, TempEntity object) {
					m_ctx.save(REMOVE_COMPONENT_00);

					RemoveComponent00Cmd cmd;
					cmd.tempEntity = entity;
					cmd.tempObject = object;
					ser::save(m_ctx, cmd);
				}

				//! Requests removal of component \tparam T from \param entity.
				//! \warning Component \tparam T must be registered in the world before calling this function.
				//!          Calling del without a previous add of the component doesn't make sense.
				template <typename T>
				void del(Entity entity) {
					verify_comp<T>();
					core::lock_scope lock(m_acc);

					// Make sure the component is registered
					const auto& item = comp_cache(m_ctx.world).template get<T>();

					del(entity, item.entity);
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
							CreateEntityFromArchetypeCmd cmd;
							ser::load(ctx, cmd);
							cmd.commit(ctx);
						},
						// CREATE_ENTITY_FROM_ENTITY
						[](CommandBufferCtx& ctx) {
							CreateEntityFromEntityCmd cmd;
							ser::load(ctx, cmd);
							cmd.commit(ctx);
						},
						// DELETE_ENTITY
						[](CommandBufferCtx& ctx) {
							DeleteEntityCmd cmd;
							ser::load(ctx, cmd);
							cmd.commit(ctx);
						},
						// ADD_COMPONENT
						[](CommandBufferCtx& ctx) {
							AddComponentCmd cmd;
							ser::load(ctx, cmd);
							cmd.commit(ctx);
						},
						// ADD_COMPONENT_10
						[](CommandBufferCtx& ctx) {
							AddComponent10Cmd cmd;
							ser::load(ctx, cmd);
							cmd.commit(ctx);
						},
						// ADD_COMPONENT_01
						[](CommandBufferCtx& ctx) {
							AddComponent01Cmd cmd;
							ser::load(ctx, cmd);
							cmd.commit(ctx);
						},
						// ADD_COMPONENT_00
						[](CommandBufferCtx& ctx) {
							AddComponent00Cmd cmd;
							ser::load(ctx, cmd);
							cmd.commit(ctx);
						},
						// ADD_COMPONENT_DATA
						[](CommandBufferCtx& ctx) {
							AddComponentWithDataCmd cmd;
							ser::load(ctx, cmd);
							cmd.commit(ctx);
						},
						// ADD_COMPONENT_TO_TEMPENTITY_DATA
						[](CommandBufferCtx& ctx) {
							AddComponentWithDataToTempEntityCmd cmd;
							ser::load(ctx, cmd);
							cmd.commit(ctx);
						},
						// SET_COMPONENT
						[](CommandBufferCtx& ctx) {
							SetComponentCmd cmd;
							ser::load(ctx, cmd);
							cmd.commit(ctx);
						},
						// SET_COMPONENT_FOR_TEMPENTITY
						[](CommandBufferCtx& ctx) {
							SetComponentOnTempEntityCmd cmd;
							ser::load(ctx, cmd);
							cmd.commit(ctx);
						},
						// REMOVE_COMPONENT
						[](CommandBufferCtx& ctx) {
							RemoveComponentCmd cmd;
							ser::load(ctx, cmd);
							cmd.commit(ctx);
						}};

			public:
				//! Commits all queued changes.
				void commit() {
					core::lock_scope lock(m_acc);

					if (m_ctx.empty())
						return;

					// Extract data from the buffer
					m_ctx.seek(0);
					while (m_ctx.tell() < m_ctx.bytes()) {
						CommandBufferCmdType id{};
						m_ctx.load(id);
						CommandBufferRead[id](m_ctx);
					}

					m_entities = 0;
					m_ctx.reset();
				} // namespace ecs
			};
		} // namespace detail

		using CommandBufferST = detail::CommandBuffer<AccessContextST>;
		using CommandBufferMT = detail::CommandBuffer<AccessContextMT>;

		inline CommandBufferST* cmd_buffer_st_create(World& world) {
			return new CommandBufferST(world);
		}
		inline void cmd_buffer_destroy(CommandBufferST& cmdBuffer) {
			delete &cmdBuffer;
		}
		inline void cmd_buffer_commit(CommandBufferST& cmdBuffer) {
			cmdBuffer.commit();
		}

		inline CommandBufferMT* cmd_buffer_mt_create(World& world) {
			return new CommandBufferMT(world);
		}
		inline void cmd_buffer_destroy(CommandBufferMT& cmdBuffer) {
			delete &cmdBuffer;
		}
		inline void cmd_buffer_commit(CommandBufferMT& cmdBuffer) {
			cmdBuffer.commit();
		}
	} // namespace ecs
} // namespace gaia
