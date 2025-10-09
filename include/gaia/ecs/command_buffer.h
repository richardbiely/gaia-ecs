#pragma once
#include "../config/config.h"

#include <cstdint>
#include <type_traits>

#include "../cnt/darray_ext.h"
#include "../cnt/dbitset.h"
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
			//! in changes of archetypes or chunk structure. This would lead to an undefined behavior.
			//! Therefore, such operations have to be executed after the loop is done.
			template <typename AccessContext>
			class CommandBuffer final {
				enum class OpType : uint8_t {
					NONE = 0,
					ADD_ENTITY,
					CPY_ENTITY,
					DEL_ENTITY,
					ADD_COMPONENT,
					ADD_COMPONENT_DATA,
					SET_COMPONENT,
					DEL_COMPONENT,
				};

				struct Op {
					//! Operation type
					OpType type;
					//! Payload offset if any
					uint32_t off;
					//! Entity being modified (may be temp before commit)
					Entity target;
					//! For ADD_COMPONENT/SET_COMPONENT/DEL_COMPONENT (other entity) or CPY_ENTITY source
					Entity other;
				};

				//! Parent world
				ecs::World& m_world;
				//! Buffer with op codes
				cnt::darray_ext<Op, 128> m_ops;
				//! Array to hold temporary->real entity mapping [0..next_temp)
				cnt::darray_ext<Entity, 128> m_temp2real;
				//! Bit layout for each temporary entity (a few bits per temporary entity in m_temp2real)
				cnt::dbitset<mem::DefaultAllocatorAdaptor> m_tmpFlags;
				//! Id of the next temporary entity to create
				uint32_t m_nextTemp = 0;
				//! True if op sorting is necessary
				bool m_needsSort = false;

				bool m_haveReal = false;
				bool m_haveTemp = false;
				Entity m_lastRealTarget = EntityBad;
				Entity m_lastTempTarget = EntityBad;

				//! Buffer holding component data
				SerializationBuffer m_data;
				//! Accessor object
				AccessContext m_acc;

			public:
				explicit CommandBuffer(World& world): m_world(world) {}
				~CommandBuffer() = default;

				CommandBuffer(CommandBuffer&&) = delete;
				CommandBuffer(const CommandBuffer&) = delete;
				CommandBuffer& operator=(CommandBuffer&&) = delete;
				CommandBuffer& operator=(const CommandBuffer&) = delete;

				//! Requests a new entity to be created
				//! \param kind Entity kind. Generic entity by default.
				//! \return Entity that will be created. The id is not usable right away. It
				//!         will be filled with proper data after commit().
				GAIA_NODISCARD Entity add(EntityKind kind = EntityKind::EK_Gen) {
					core::lock_scope lock(m_acc);

					Entity temp = add_temp(kind);
					push_op({OpType::ADD_ENTITY, 0, temp, EntityBad});
					return temp;
				}

				//! Requests a new entity to be created by cloning an already existing entity
				//! \return Entity that will be created. The id is not usable right away. It
				//!         will be filled with proper data after commit()
				GAIA_NODISCARD Entity copy(Entity entityFrom) {
					core::lock_scope lock(m_acc);

					Entity temp = add_temp(entityFrom.kind());
					push_op({OpType::CPY_ENTITY, 0, temp, entityFrom});
					return temp;
				}

				//! Requests a component \tparam T to be added to \param entity.
				template <typename T>
				void add(Entity entity) {
					verify_comp<T>();
					core::lock_scope lock(m_acc);

					// Make sure the component is registered
					const auto& item = comp_cache_add<T>(m_world);

					push_op({OpType::ADD_COMPONENT, 0, entity, item.entity});
				}

				//! Requests an entity \param other to be added to entity \param entity.
				void add(Entity entity, Entity other) {
					core::lock_scope lock(m_acc);

					push_op({OpType::ADD_COMPONENT, 0, entity, other});
				}

				//! Requests a component \tparam T to be added to entity. Also sets its value.
				//! \warning Component \tparam T should be registered in the world before calling this function.
				//!          If used in concurrent environment, race conditions may occur otherwise.
				template <typename T>
				void add(Entity entity, T&& value) {
					verify_comp<T>();
					core::lock_scope lock(m_acc);

					// Make sure the component is registered
					const auto& item = comp_cache_add<T>(m_world);

					const auto pos = m_data.tell();
					m_data.save_comp(item, GAIA_FWD(value));
					push_op({OpType::ADD_COMPONENT_DATA, pos, entity, item.entity});
				}

				//! Requests component data to be set to given values for a given entity.
				//! \warning Component \tparam T must be registered in the world before calling this function.
				//!          Calling set without a previous add of the component doesn't make sense.
				template <typename T>
				void set(Entity entity, T&& value) {
					verify_comp<T>();
					core::lock_scope lock(m_acc);

					// Make sure the component is registered
					const auto& item = comp_cache(m_world).template get<T>();

					const auto pos = m_data.tell();
					m_data.save_comp(item, GAIA_FWD(value));
					push_op({OpType::SET_COMPONENT, pos, entity, item.entity});
				}

				//! Requests an existing \param entity to be removed.
				void del(Entity entity) {
					core::lock_scope lock(m_acc);

					push_op({OpType::DEL_ENTITY, 0, entity, EntityBad});
				}

				//! Requests removal of component \tparam T from \param entity.
				//! \warning Component \tparam T must be registered in the world before calling this function.
				//!          Calling del without a previous add of the component doesn't make sense.
				template <typename T>
				void del(Entity entity) {
					verify_comp<T>();
					core::lock_scope lock(m_acc);

					// Make sure the component is registered
					const auto& item = comp_cache(m_world).template get<T>();

					push_op({OpType::DEL_COMPONENT, 0, entity, item.entity});
				}

				//! Requests removal of entity \param object from entity \param entity.
				void del(Entity entity, Entity object) {
					core::lock_scope lock(m_acc);

					push_op({OpType::DEL_COMPONENT, 0, entity, object});
				}

			private:
				//! Returns true if the op modifies a relationship between entities (e.g. adds or removes a component).
				GAIA_NODISCARD bool is_rel(OpType t) const {
					return (uint32_t)t >= (uint32_t)OpType::ADD_COMPONENT;
				}

				//! Returns true if an entity is a temporary one
				GAIA_NODISCARD bool is_tmp(Entity e) const {
					return e.data.tmp != 0;
				}

				//! Maps a temporary entity to a real one, or returns the real one immediately
				GAIA_NODISCARD Entity resolve(Entity e) const {
					if (!is_tmp(e))
						return e;

					const auto ti = e.id();
					if (ti < m_temp2real.size())
						return m_temp2real[ti];

					return EntityBad;
				}

				//! Returns true if a temporary entity was created and then destroyed within the same command buffer (
				//! meaning all its operations cancel out and it should be completely ignored during commit).
				GAIA_NODISCARD bool is_canceled_temp(uint32_t idx) const {
					const uint32_t base = idx * 3;
					if (base + 2 >= m_tmpFlags.size())
						return false;

					const bool destroy = m_tmpFlags.test(base);
					const bool usedOther = m_tmpFlags.test(base + 2);

					// If deleted in this batch and not referenced by others, cancel entirely
					return destroy && !usedOther;
				}

				//! Create a temporary entity.
				GAIA_NODISCARD Entity add_temp(EntityKind kind) {
					m_temp2real.push_back(EntityBad);
					Entity tmp(m_nextTemp++, 0, true, false, kind);
					// Use the unused flag to mark temporary entities
					tmp.data.tmp = 1;
					return tmp;
				}

				GAIA_NODISCARD bool less_target(Entity a, Entity b) const {
					const bool ta = is_tmp(a);
					const bool tb = is_tmp(b);

					// Real entities always come first in order. Temps come last.
					if (ta != tb)
						return !ta && tb;

					// Within same domain, normal numeric order.
					return a.id() < b.id();
				}

				//! Verifies if sorting is necessary after a given op is added
				void check_sort(const Op& op) {
					if (is_tmp(op.target)) {
						if (m_haveTemp) {
							// Only compare temp indices against previous temp target
							const uint32_t prev = m_lastTempTarget.id();
							const uint32_t curr = op.target.id();
							if (curr < prev)
								m_needsSort = true;
						}
						m_lastTempTarget = op.target;
						m_haveTemp = true;
					} else {
						if (m_haveReal) {
							// Only compare real IDs against previous real target
							if (op.target.id() < m_lastRealTarget.id())
								m_needsSort = true;
						}
						m_lastRealTarget = op.target;
						m_haveReal = true;
					}
				}

				//! Pushes the op to the op buffer
				void push_op(Op&& op) {
					check_sort(op);
					m_ops.push_back(GAIA_MOV(op));
				}

				//! Clears the internal buffers and effectively resets the object to its default state
				void clear() {
					m_ops.clear();
					m_temp2real.clear();
					m_tmpFlags.reset();
					m_nextTemp = 0;
					m_data.reset();

					m_needsSort = false;
					m_haveReal = false;
					m_haveTemp = false;
					m_lastRealTarget = EntityBad;
					m_lastTempTarget = EntityBad;
				}

			public:
				//! Commits all queued changes.
				void commit() {
					core::lock_scope lock(m_acc);

					if (m_ops.empty())
						return;

					GAIA_PROF_SCOPE(cmdbuf::commit);

					// Build flags + allocate entities
					if (m_nextTemp > 0) {
						GAIA_PROF_SCOPE(cmdbuf::alloc);

						// Bit layout for each temporary entity:
						// bit 0 -> marked for destruction (DEL_ENTITY recorded)
						// bit 1 -> used as a relation target (appeared as target in component ops)
						// bit 2 -> used as a relation source / dependency (appeared as other in component ops)
						m_tmpFlags.resize(m_nextTemp * 3);

						// Pre-map surviving temps for ADD/CPY (avoid allocating canceled temps)
						if (m_temp2real.size() < m_nextTemp) {
							const auto from = m_temp2real.size();
							m_temp2real.resize(m_nextTemp);
							GAIA_FOR2(from, m_nextTemp) m_temp2real[i] = EntityBad;
						}

						// Build all flags
						for (const Op& o: m_ops) {
							// Set flag bits
							if (is_tmp(o.target)) {
								const uint32_t ti = o.target.id();

								if (o.type == OpType::DEL_ENTITY)
									m_tmpFlags.set((ti * 3) + 0, true);
								else if (is_rel(o.type))
									m_tmpFlags.set((ti * 3) + 1, true);
							}

							if (is_tmp(o.other) && o.other.id() < m_tmpFlags.size())
								m_tmpFlags.set((o.other.id() * 3) + 2, true);
						}

						// Allocate real entities for the surviving temporaries
						for (const Op& o: m_ops) {
							if (!is_tmp(o.target))
								continue;

							const uint32_t ti = o.target.id();
							if (is_canceled_temp(ti))
								continue;

							if (o.type == OpType::ADD_ENTITY) {
								if (m_temp2real[ti] == EntityBad)
									m_temp2real[ti] = m_world.add(o.target.kind());
							} else if (o.type == OpType::CPY_ENTITY) {
								if (m_temp2real[ti] == EntityBad) {
									const Entity src = resolve(o.other);
									if (src != EntityBad)
										m_temp2real[ti] = m_world.copy(src);
								}
							}
						}
					}

					// Sort by (target, other), reduce last-wins, apply relations.
					// DEL_COMPONENT last per target.
					if (m_needsSort) {
						GAIA_PROF_SCOPE(cmdbuf::sort);

						m_needsSort = false;
						core::sort(m_ops.begin(), m_ops.end(), [](const Op& a, const Op& b) {
							if (a.target != b.target)
								return a.target < b.target;
							if (a.other != b.other)
								return a.other < b.other;
							return false;
						});
					}

					// Replay batched operations
					Entity lastKey = EntityBad;
					Entity lastResolved = EntityBad;
					auto resolve_cached = [&](Entity e) {
						if (e == lastKey)
							return lastResolved;
						lastKey = e;
						return lastResolved = resolve(e);
					};
					for (uint32_t p = 0; p < m_ops.size();) {
						GAIA_PROF_SCOPE(cmdbuf::merge);

						const Entity tgtKey = m_ops[p].target;

						const bool tgtIsTemp = is_tmp(tgtKey);
						const uint32_t ti = tgtIsTemp ? tgtKey.id() : 0u;
						const Entity tgtReal =
								tgtIsTemp ? (ti < m_temp2real.size() ? m_temp2real[ti] : EntityBad) : resolve_cached(tgtKey);

						// Range for this target
						uint32_t q = p;
						bool hasDelEntity = false;
						while (q < m_ops.size() && m_ops[q].target == tgtKey) {
							if (m_ops[q].type == OpType::DEL_ENTITY)
								hasDelEntity = true;
							++q;
						}

						// Skip canceled or non-existent temporary entities
						if (tgtReal == EntityBad) {
							p = q;
							continue;
						}
						if (tgtIsTemp && is_canceled_temp(ti)) {
							p = q;
							continue;
						}

						enum : uint8_t { F_ADD = 1 << 0, F_ADD_DATA = 1 << 1, F_SET = 1 << 2, F_DEL = 1 << 3 };

						// Emit relation groups.
						// Inside [p..q) range (same target), process groups by 'other'.
						// We perform per-component reduction.
						for (uint32_t i = p; i < q;) {
							const Entity othKey = m_ops[i].other;
							const Entity othReal = resolve_cached(othKey);

							// Group ops with same (target, other)
							uint32_t j = i + 1;
							while (j < q && m_ops[j].other == othKey)
								++j;

							if (tgtReal != EntityBad) {
								const uint32_t groupSize = j - i;
								// Fast path - single op
								if (groupSize == 1) {
									const Op& op = m_ops[i];
									switch (op.type) {
										case OpType::DEL_COMPONENT:
											World::EntityBuilder(m_world, tgtReal).del(othReal);
											break;
										case OpType::ADD_COMPONENT:
											World::EntityBuilder(m_world, tgtReal).add(othReal);
											break;
										case OpType::ADD_COMPONENT_DATA: {
											World::EntityBuilder(m_world, tgtReal).add(othReal);

											const auto& ec = m_world.m_recs.entities[tgtReal.id()];
											const auto row = tgtReal.kind() == EntityKind::EK_Uni ? 0U : ec.row;

											// Component data
											const auto compIdx = ec.pChunk->comp_idx(othReal);
											auto* pComponentData = (void*)ec.pChunk->comp_ptr_mut(compIdx, row);
											m_data.seek(op.off);
											m_data.load_comp(m_world.comp_cache().get((othReal)), pComponentData);
										} break;
										case OpType::SET_COMPONENT: {
											const auto& ec = m_world.m_recs.entities[tgtReal.id()];
											const auto row = tgtReal.kind() == EntityKind::EK_Uni ? 0U : ec.row;

											// Component data
											const auto compIdx = ec.pChunk->comp_idx(othReal);
											auto* pComponentData = (void*)ec.pChunk->comp_ptr_mut(compIdx, row);
											m_data.seek(op.off);
											m_data.load_comp(m_world.comp_cache().get((othReal)), pComponentData);
										} break;
										default:
											break;
									}
								}
								// Slow path: merge multiple ops
								else {
									uint8_t mask = 0;
									uint32_t dataPos = 0;

									for (uint32_t k = i; k < j; ++k) {
										const Op& op = m_ops[k];
										switch (op.type) {
											case OpType::ADD_COMPONENT:
												mask |= F_ADD;
												break;
											case OpType::ADD_COMPONENT_DATA:
												mask |= F_ADD_DATA;
												dataPos = op.off;
												break;
											case OpType::SET_COMPONENT:
												mask |= F_SET;
												dataPos = op.off;
												break;
											case OpType::DEL_COMPONENT:
												mask |= F_DEL;
												break;
											default:
												break;
										}
									}

									const bool hasAdd = mask & F_ADD;
									const bool hasAddData = mask & F_ADD_DATA;
									const bool hasSet = mask & F_SET;
									const bool hasDel = mask & F_DEL;

									// 1) ADD(+DATA) + DEL = no-op
									if (hasDel && (hasAdd || hasAddData)) {
									}
									// 2) DEL only
									else if (hasDel) {
										World::EntityBuilder(m_world, tgtReal).del(othReal);
									}
									// 3) ADD_WITH_DATA or ADD+SET = ADD_WITH_DATA
									else if (hasAddData || (hasAdd && hasSet)) {
										World::EntityBuilder(m_world, tgtReal).add(othReal);

										const auto& ec = m_world.m_recs.entities[tgtReal.id()];
										const auto row = tgtReal.kind() == EntityKind::EK_Uni ? 0U : ec.row;

										// Component data
										const auto compIdx = ec.pChunk->comp_idx(othReal);
										auto* pComponentData = (void*)ec.pChunk->comp_ptr_mut(compIdx, row);
										m_data.seek(dataPos);
										m_data.load_comp(m_world.comp_cache().get((othReal)), pComponentData);
									}
									// 4) ADD only
									else if (hasAdd) {
										World::EntityBuilder(m_world, tgtReal).add(othReal);
									}
									// 5) SET only
									else if (hasSet) {
										const auto& ec = m_world.m_recs.entities[tgtReal.id()];
										const auto row = tgtReal.kind() == EntityKind::EK_Uni ? 0U : ec.row;

										// Component data
										const auto compIdx = ec.pChunk->comp_idx(othReal);
										auto* pComponentData = (void*)ec.pChunk->comp_ptr_mut(compIdx, row);
										m_data.seek(dataPos);
										m_data.load_comp(m_world.comp_cache().get((othReal)), pComponentData);
									}
								}
							}

							// Advance to next component group
							i = j;
						}

						// Safely delete entity only if it was actually created
						if (hasDelEntity)
							m_world.del(tgtReal);

						// Advance to next target group
						p = q;
					}

					clear();
				}
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
