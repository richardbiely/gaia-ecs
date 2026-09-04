#pragma once
#include "gaia/config/config.h"

#include <cstdint>
#include <type_traits>

#include "gaia/cnt/darray_ext.h"
#include "gaia/cnt/dbitset.h"
#include "gaia/ecs/archetype.h"
#include "gaia/ecs/command_buffer_fwd.h"
#include "gaia/ecs/common.h"
#include "gaia/ecs/component.h"
#include "gaia/ecs/component_cache.h"
#include "gaia/ecs/component_cache_item.h"
#include "gaia/ecs/id.h"
#include "gaia/ecs/world.h"
#include "gaia/ser/ser_buffer_binary.h"

namespace gaia {
	namespace ecs {
		//! Single-threaded command-buffer access guard. Locking is a no-op.
		struct AccessContextST {
			//! Acquires the access guard.
			void lock() {}
			//! Releases the access guard.
			void unlock() {}
		};

		//! Multi-threaded command-buffer access guard backed by a spin lock.
		struct AccessContextMT {
			//! Spin lock serializing command-buffer access across worker threads.
			mt::SpinLock m_lock;

			//! Acquires the access guard.
			void lock() {
				m_lock.lock();
			}

			//! Releases the access guard.
			void unlock() {
				m_lock.unlock();
			}
		};

		namespace detail {
			//! Buffer for deferred execution of some operations on entities.
			//!
			//! Adding, removing, copying, or instantiating entities inside World::each can result
			//! in changes of archetypes or chunk structure. This would lead to an undefined behavior.
			//! Therefore, such operations have to be executed after the loop is done.
			//! \tparam AccessContext Access guard. AccessContextST for single-threaded recording,
			//!                       AccessContextMT when multiple threads record into the same buffer.
			template <typename AccessContext>
			class CommandBuffer final {
				//! \cond INTERNAL
				enum class OpType : uint8_t {
					NONE = 0,
					ADD_ENTITY,
					CPY_ENTITY,
					INSTANTIATE_ENTITY,
					DEL_ENTITY,
					ADD_COMPONENT,
					ADD_COMPONENT_DATA,
					SET_COMPONENT,
					DEL_COMPONENT,
				};

				struct Op {
					//! Operation type.
					OpType type;
					//! Payload offset if any.
					uint32_t off;
					//! Entity being modified (may be temp before commit).
					Entity target;
					//! For ADD_COMPONENT/SET_COMPONENT/DEL_COMPONENT (other entity), CPY_ENTITY source,
					//! or INSTANTIATE_ENTITY prefab.
					Entity other;
					//! Pair target kept separately so temporary pair endpoints survive until commit.
					//! For INSTANTIATE_ENTITY this is the optional parent instance, or EntityBad.
					Entity pairTarget = EntityBad;
					//! Original insertion order, used to keep equal-key operations deterministic after sorting.
					uint32_t order = 0;
				};

				//! Parent world.
				ecs::World& m_world;
				//! Buffer with op codes.
				cnt::darray_ext<Op, 128> m_ops;
				//! Array to hold temporary->real entity mapping [0..next_temp).
				cnt::darray_ext<Entity, 128> m_temp2real;
				//! Bit layout for each temporary entity (a few bits per temporary entity in m_temp2real).
				cnt::dbitset<mem::DefaultAllocatorAdaptor> m_tmpFlags;
				//! Id of the next temporary entity to create.
				uint32_t m_nextTemp = 0;
				//! True if op sorting is necessary.
				bool m_needsSort = false;

				//! Buffer holding component data.
				ser::bin_stream m_data;
				//! Accessor object.
				AccessContext m_acc;
				//! \endcond

			public:
				//! Creates a command buffer bound to \a world.
				//! \param world World that receives the recorded operations on commit.
				explicit CommandBuffer(World& world): m_world(world) {}
				~CommandBuffer() = default;

				CommandBuffer(CommandBuffer&&) = delete;
				CommandBuffer(const CommandBuffer&) = delete;
				CommandBuffer& operator=(CommandBuffer&&) = delete;
				CommandBuffer& operator=(const CommandBuffer&) = delete;

				//! Requests a new entity to be created.
				//! \return Entity that will be created. The id is not usable right away. It
				//!         will be filled with proper data after commit().
				GAIA_NODISCARD Entity add() {
					core::lock_scope lock(m_acc);

					Entity temp = add_temp();
					push_op({OpType::ADD_ENTITY, 0, temp, EntityBad});
					return temp;
				}

				//! Requests a new entity to be created by cloning an already existing entity.
				//! \param entityFrom Entity to clone. Temporary entities from this buffer are resolved on commit.
				//! \return Entity that will be created. The id is not usable right away. It
				//!         will be filled with proper data after commit().
				GAIA_NODISCARD Entity copy(Entity entityFrom) {
					core::lock_scope lock(m_acc);

					Entity temp = add_temp();
					push_op({OpType::CPY_ENTITY, 0, temp, entityFrom});
					return temp;
				}

				//! Requests a prefab to be instantiated as a normal entity.
				//! Commit replays through World::instantiate, including Prefab removal, the direct
				//! Pair(Is, prefab) edge, skipped names, OnInstantiate policies, and recursive prefab children.
				//! Non-prefab sources fall back to copy.
				//! \param prefabEntity Prefab entity to instantiate.
				//! \return Temporary entity filled with the spawned root instance after commit().
				//! \warning The returned entity is not usable until commit(). Prefab children are not
				//!          exposed as temporaries; look them up with World::find_prefab_instance after commit.
				GAIA_NODISCARD Entity instantiate(Entity prefabEntity) {
					return instantiate(prefabEntity, EntityBad);
				}

				//! Requests a prefab to be instantiated as a normal entity parented under \a parentInstance.
				//! Commit replays through World::instantiate. Pair(Parent, parentInstance) is attached to the
				//! spawned root. Non-prefab sources fall back to a parented copy.
				//! \param prefabEntity Prefab entity to instantiate.
				//! \param parentInstance Entity receiving the spawned root through Parent, or EntityBad for an
				//!                      unparented root.
				//! \return Temporary entity filled with the spawned root instance after commit().
				//! \warning The returned entity is not usable until commit(). Prefab children are not
				//!          exposed as temporaries; look them up with World::find_prefab_instance after commit.
				GAIA_NODISCARD Entity instantiate(Entity prefabEntity, Entity parentInstance) {
					GAIA_ASSERT(!prefabEntity.pair());
					core::lock_scope lock(m_acc);

					Entity temp = add_temp();
					push_op({OpType::INSTANTIATE_ENTITY, 0, temp, prefabEntity, parentInstance});
					return temp;
				}

				//! Requests \a count prefab instantiations.
				//! Commit groups matching records and replays them through World::instantiate_n.
				//! \param prefabEntity Prefab entity to instantiate.
				//! \param count Number of root instances to spawn. Zero is a no-op.
				void instantiate_n(Entity prefabEntity, uint32_t count) {
					instantiate_n(prefabEntity, EntityBad, count, []([[maybe_unused]] Entity) {});
				}

				//! Requests \a count prefab instantiations parented under \a parentInstance.
				//! \param prefabEntity Prefab entity to instantiate.
				//! \param parentInstance Entity receiving each spawned root through Parent.
				//! \param count Number of root instances to spawn. Zero is a no-op.
				void instantiate_n(Entity prefabEntity, Entity parentInstance, uint32_t count) {
					instantiate_n(prefabEntity, parentInstance, count, []([[maybe_unused]] Entity) {});
				}

				//! Requests \a count prefab instantiations and records per-instance commands.
				//! The callback runs immediately while recording and receives temporary entity handles,
				//! matching add and copy. CopyIter callbacks are World-only.
				//! \tparam Func Callback type. Must be invocable as void(Entity).
				//! \param prefabEntity Prefab entity to instantiate.
				//! \param count Number of root instances to spawn. Zero is a no-op.
				//! \param func Functor invoked with each temporary root handle while recording.
				template <typename Func>
				void instantiate_n(Entity prefabEntity, uint32_t count, Func func) {
					instantiate_n(prefabEntity, EntityBad, count, func);
				}

				//! Requests \a count prefab instantiations parented under \a parentInstance and records
				//! per-instance commands.
				//! The callback runs immediately while recording and receives temporary entity handles,
				//! matching add and copy. CopyIter callbacks are World-only.
				//! \tparam Func Callback type. Must be invocable as void(Entity).
				//! \param prefabEntity Prefab entity to instantiate.
				//! \param parentInstance Entity receiving each spawned root through Parent, or EntityBad.
				//! \param count Number of root instances to spawn. Zero is a no-op.
				//! \param func Functor invoked with each temporary root handle while recording.
				template <typename Func>
				void instantiate_n(Entity prefabEntity, Entity parentInstance, uint32_t count, Func func) {
					static_assert(
							std::is_invocable_v<Func, Entity>,
							"Command-buffer instantiate_n callbacks receive temporary Entity handles");
					if (count == 0U)
						return;

					GAIA_FOR(count) {
						const Entity temp = instantiate(prefabEntity, parentInstance);
						func(temp);
					}
				}

				//! Requests a component \a T to be added to \a entity.
				//! \tparam T Component type.
				//! \param entity Destination entity.
				//! \warning Component \a T should be registered in the world before calling this function while
				//!          the world is locked for iteration. Registering a new component type is a structural change.
				template <typename T>
				void add(Entity entity) {
					verify_comp<T>();
					core::lock_scope lock(m_acc);

					// Make sure the component is registered
					const auto& item = comp_cache_add<T>(m_world);

					push_op({OpType::ADD_COMPONENT, 0, entity, item.entity});
				}

				//! Requests an entity \a other to be added to entity \a entity.
				//! \param entity Destination entity.
				//! \param other Entity to add to \a entity.
				void add(Entity entity, Entity other) {
					core::lock_scope lock(m_acc);

					if (other.pair()) {
						push_op({OpType::ADD_COMPONENT, 0, entity, m_world.get(other.id()), m_world.get(other.gen())});
						return;
					}

					push_op({OpType::ADD_COMPONENT, 0, entity, other});
				}

				//! Requests a relationship pair to be added to entity \a entity.
				//! \param entity Destination entity.
				//! \param pair Relationship pair to add to \a entity.
				void add(Entity entity, const Pair& pair) {
					core::lock_scope lock(m_acc);

					push_op({OpType::ADD_COMPONENT, 0, entity, pair.first(), pair.second()});
				}

				//! Requests a component \a T to be added to entity. Also sets its value.
				//! \tparam T Component type.
				//! \param entity Destination entity.
				//! \param value Component value.
				//! \warning Component \a T should be registered in the world before calling this function while
				//!          the world is locked for iteration. Registering a new component type is a structural change.
				//!          If used in concurrent environment, race conditions may occur otherwise.
				template <typename T, std::enable_if_t<!is_pair<std::remove_cv_t<std::remove_reference_t<T>>>::value, int> = 0>
				void add(Entity entity, T&& value) {
					verify_comp<T>();
					core::lock_scope lock(m_acc);

					// Make sure the component is registered
					const auto& item = comp_cache_add<T>(m_world);

					const auto pos = m_data.tell();
					auto serializer = ser::make_serializer(m_data);
					item.save(serializer, &value, 0, 1, 1);
					push_op({OpType::ADD_COMPONENT_DATA, pos, entity, item.entity});
				}

				//! Requests component data to be set to given values for a given entity.
				//! \tparam T Component type.
				//! \param entity Destination entity.
				//! \param value Component value.
				//! \warning Component \a T must be registered in the world before calling this function.
				//!          Calling set without a previous add of the component doesn't make sense.
				template <typename T>
				void set(Entity entity, T&& value) {
					verify_comp<T>();
					core::lock_scope lock(m_acc);

					// Make sure the component is registered
					const auto& item = comp_cache(m_world).template get<T>();

					const auto pos = m_data.tell();
					auto serializer = ser::make_serializer(m_data);
					item.save(serializer, &value, 0, 1, 1);
					push_op({OpType::SET_COMPONENT, pos, entity, item.entity});
				}

				//! Requests an existing \a entity to be removed.
				//! \param entity Entity to remove.
				void del(Entity entity) {
					core::lock_scope lock(m_acc);

					push_op({OpType::DEL_ENTITY, 0, entity, EntityBad});
				}

				//! Requests removal of component \a T from \a entity.
				//! \tparam T Component type.
				//! \param entity Source entity.
				//! \warning Component \a T must be registered in the world before calling this function.
				//!          Calling del without a previous add of the component doesn't make sense.
				template <typename T>
				void del(Entity entity) {
					verify_comp<T>();
					core::lock_scope lock(m_acc);

					// Make sure the component is registered
					const auto& item = comp_cache(m_world).template get<T>();

					push_op({OpType::DEL_COMPONENT, 0, entity, item.entity});
				}

				//! Requests removal of entity \a object from entity \a entity.
				//! \param entity Source entity.
				//! \param object Entity to remove.
				void del(Entity entity, Entity object) {
					core::lock_scope lock(m_acc);

					if (object.pair()) {
						push_op({OpType::DEL_COMPONENT, 0, entity, m_world.get(object.id()), m_world.get(object.gen())});
						return;
					}

					push_op({OpType::DEL_COMPONENT, 0, entity, object});
				}

				//! Requests removal of a relationship pair from entity \a entity.
				//! \param entity Source entity.
				//! \param pair Relationship pair to remove from \a entity.
				void del(Entity entity, const Pair& pair) {
					core::lock_scope lock(m_acc);

					push_op({OpType::DEL_COMPONENT, 0, entity, pair.first(), pair.second()});
				}

			private:
				//! \cond INTERNAL
				//! Returns true if the op modifies a relationship between entities (e.g. adds or removes a component).
				//! \param t Operation type.
				//! \return True when \a t is a component add, set, or delete.
				GAIA_NODISCARD bool is_rel(OpType t) const {
					return (uint32_t)t >= (uint32_t)OpType::ADD_COMPONENT;
				}

				//! Returns true if an entity is a temporary one.
				//! \param e Entity to test.
				//! \return True when \a e was allocated by this command buffer.
				GAIA_NODISCARD bool is_tmp(Entity e) const {
					return e.data.tmp != 0;
				}

				//! Maps a temporary entity to a real one, or returns the real one immediately.
				//! \param e Entity to resolve.
				//! \return Real entity, or EntityBad when a temporary cannot be mapped.
				GAIA_NODISCARD Entity resolve(Entity e) const {
					if (!is_tmp(e))
						return e;

					const auto ti = e.id();
					if (ti < m_temp2real.size())
						return m_temp2real[ti];

					return EntityBad;
				}

				//! Resolves an operation's component or pair identifier.
				//! \param op Operation whose component or pair endpoints are resolved.
				//! \return Resolved component or pair identifier, or EntityBad when a temporary endpoint cannot be resolved.
				GAIA_NODISCARD Entity resolve_object(const Op& op) const {
					if (op.pairTarget == EntityBad)
						return resolve(op.other);

					const auto relation = resolve(op.other);
					const auto target = resolve(op.pairTarget);
					if (relation == EntityBad || target == EntityBad)
						return EntityBad;

					return (Entity)Pair(relation, target);
				}

				//! Allocates real entities for one instantiate record and any later matching records.
				//! Matching means the same prefab and parent keys. Results are replayed through
				//! World::instantiate / instantiate_n so Prefab, Is, and child-hierarchy rules stay identical.
				//! \param firstIdx Index of the first INSTANTIATE_ENTITY operation to allocate.
				void allocate_instantiate_group(uint32_t firstIdx) {
					const Op& first = m_ops[firstIdx];
					const Entity prefab = resolve(first.other);
					if (prefab == EntityBad)
						return;

					Entity parent = EntityBad;
					if (first.pairTarget != EntityBad) {
						parent = resolve(first.pairTarget);
						if (parent == EntityBad)
							return;
					}

					cnt::darray_ext<uint32_t, 16> tempIds;
					for (uint32_t j = firstIdx; j < m_ops.size(); ++j) {
						const Op& op = m_ops[j];
						if (op.type != OpType::INSTANTIATE_ENTITY || op.other != first.other || op.pairTarget != first.pairTarget)
							continue;
						if (!is_tmp(op.target))
							continue;

						const uint32_t ti = op.target.id();
						if (ti >= m_temp2real.size() || m_temp2real[ti] != EntityBad || is_canceled_temp(ti))
							continue;

						tempIds.push_back(ti);
					}

					if (tempIds.empty())
						return;

					if (tempIds.size() == 1) {
						m_temp2real[tempIds[0]] =
								parent == EntityBad ? m_world.instantiate(prefab) : m_world.instantiate(prefab, parent);
						return;
					}

					uint32_t mapped = 0;
					auto mapRoot = [&](Entity instance) {
						GAIA_ASSERT(mapped < tempIds.size());
						m_temp2real[tempIds[mapped++]] = instance;
					};
					if (parent == EntityBad)
						m_world.instantiate_n(prefab, tempIds.size(), mapRoot);
					else
						m_world.instantiate_n(prefab, parent, tempIds.size(), mapRoot);
					GAIA_ASSERT(mapped == tempIds.size());
				}

				//! Replays a component or relationship pair addition.
				//! \param target Entity receiving the component or pair.
				//! \param object Resolved component or pair identifier.
				void replay_add(Entity target, Entity object) {
					if (object.pair()) {
						World::EntityBuilder(m_world, target).add(Pair(m_world.get(object.id()), m_world.get(object.gen())));
						return;
					}

					const auto* pItem = m_world.comp_cache().find(object);
					if (pItem != nullptr && pItem->comp.storage_type() == DataStorageType::Sparse) {
						const auto mode = m_world.sparse_storage_mode(object);
						GAIA_ASSERT(mode != World::SparseStorageMode::None);
						auto& store = m_world.sparse_component_store_erased_mut(object, *pItem);
						(void)store.func_add(store.pStore, target);
						m_world.finish_sparse_component_add_inter(target, object, mode);
						return;
					}

					World::EntityBuilder(m_world, target).add(object);
				}

				//! Replays a component or relationship pair removal.
				//! \param target Entity losing the component or pair.
				//! \param object Resolved component or pair identifier.
				void replay_del(Entity target, Entity object) {
					if (object.pair())
						m_world.del(target, Pair(m_world.get(object.id()), m_world.get(object.gen())));
					else
						m_world.del(target, object);
				}

				//! Replays serialized component data into table or sparse storage.
				//! \param target Entity receiving the payload.
				//! \param object Component id whose payload is restored.
				//! \param dataPos Serialized payload offset in the command-buffer data stream.
				//! \param finishWrite Whether to publish changed state and `OnSet` after loading.
				void replay_data(Entity target, Entity object, uint32_t dataPos, bool finishWrite = true) {
					auto serializer = ser::make_serializer(m_data);
					serializer.seek(dataPos);
					const auto* pItem =
							object.pair() ? m_world.comp_cache().find_pair_payload(object) : m_world.comp_cache().find(object);
					GAIA_ASSERT(pItem != nullptr);
					if (pItem == nullptr)
						return;
					const auto& item = *pItem;

					if (m_world.component_uses_sparse_storage(object)) {
						const auto payload = m_world.mut_raw(target, object);
						GAIA_ASSERT(payload.valid());
						if (payload.valid())
							item.load(serializer, payload.data, 0, 1, 1);
						if (finishWrite)
							m_world.finish_write(target, object);
						return;
					}

					const auto& ec = target.pair() ? m_world.fetch(target) : m_world.m_recs.entities[target.id()];
					const auto row = ec.row;
					const auto compIdx = ec.pChunk->comp_idx(object);
					auto* pComponentData = (void*)ec.pChunk->comp_ptr_mut(compIdx, 0);
					item.load(serializer, pComponentData, row, row + 1, ec.pChunk->capacity());
					if (finishWrite)
						m_world.finish_write(target, object);
				}

				//! Replays a component add whose serialized payload must be initialized before add notification.
				//! \param target Entity receiving the component.
				//! \param object Component id being added.
				//! \param dataPos Serialized payload offset in the command-buffer data stream.
				void replay_add_data(Entity target, Entity object, uint32_t dataPos) {
					const auto* pItem =
							object.pair() ? m_world.comp_cache().find_pair_payload(object) : m_world.comp_cache().find(object);
					GAIA_ASSERT(pItem != nullptr);
					if (pItem == nullptr)
						return;
					const auto& item = *pItem;
					if (m_world.component_uses_sparse_storage(object)) {
						const auto mode = m_world.sparse_storage_mode(object);
						GAIA_ASSERT(mode != World::SparseStorageMode::None);
						auto& store = m_world.sparse_component_store_erased_mut(object, item);
						auto* pPayload = store.func_add(store.pStore, target);

						auto serializer = ser::make_serializer(m_data);
						serializer.seek(dataPos);
						item.load(serializer, pPayload, 0, 1, 1);
						m_world.finish_sparse_component_add_inter(target, object, mode);
						return;
					}

					World::EntityBuilder builder(m_world, target);
#if GAIA_OBSERVERS_ENABLED
					auto addDiffCtx = m_world.m_observers.prepare_diff(
							m_world, ObserverEvent::OnAdd, EntitySpan{&object, 1}, EntitySpan{&target, 1});
#endif
					builder.add_inter_init(object);
					builder.commit();
					replay_data(target, object, dataPos, false);
					m_world.notify_add_single(target, object);
#if GAIA_OBSERVERS_ENABLED
					m_world.m_observers.finish_diff(m_world, GAIA_MOV(addDiffCtx));
#endif
				}

				//! Returns true if a temporary entity was created and then destroyed within the same command buffer,
				//! meaning all its operations cancel out and it should be completely ignored during commit.
				//! \param idx Temporary entity index.
				//! \return True when the temporary can be discarded.
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
				//! \return Temporary entity handle valid until commit.
				GAIA_NODISCARD Entity add_temp() {
					m_temp2real.push_back(EntityBad);
					Entity tmp(m_nextTemp++, 0, true, false);
					// Use the unused flag to mark temporary entities
					tmp.data.tmp = 1;
					return tmp;
				}

				//! Compares operation grouping buckets without considering insertion order.
				//! \param a Left operation.
				//! \param b Right operation.
				//! \return True when the grouping bucket of \p a sorts before the grouping bucket of \p b.
				GAIA_NODISCARD static bool less_op_key(const Op& a, const Op& b) {
					if (a.target != b.target)
						return a.target < b.target;

					const bool aIsPair = a.pairTarget != EntityBad;
					const bool bIsPair = b.pairTarget != EntityBad;
					if (aIsPair != bIsPair)
						return !aIsPair;
					if (aIsPair)
						return false;

					if (a.other != b.other)
						return a.other < b.other;
					return false;
				}

				//! Verifies if sorting is necessary after a given operation is added.
				//! \param op Operation about to be appended.
				void check_sort(const Op& op) {
					if (!m_ops.empty() && less_op_key(op, m_ops.back()))
						m_needsSort = true;
				}

				//! Pushes an operation to the operation buffer.
				//! \param op Operation to append.
				void push_op(Op&& op) {
					op.order = m_ops.size();
					check_sort(op);
					m_ops.push_back(GAIA_MOV(op));
				}

				//! Clears the internal buffers and effectively resets the object to its default state.
				void clear() {
					m_ops.clear();
					m_temp2real.clear();
					m_tmpFlags.reset();
					m_nextTemp = 0;
					m_data.reset();

					m_needsSort = false;
				}
				//! \endcond

			public:
				//! Commits all queued changes.
				//! Entity create, copy, and instantiate records are allocated first, then component
				//! operations are merged and replayed, then entity deletions are applied.
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

						// Pre-map surviving temps for ADD/CPY/INSTANTIATE (avoid allocating canceled temps)
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
							if (is_tmp(o.pairTarget) && o.pairTarget.id() < m_tmpFlags.size())
								m_tmpFlags.set((o.pairTarget.id() * 3) + 2, true);
						}

						// Allocate real entities for the surviving temporaries
						for (uint32_t i = 0; i < m_ops.size(); ++i) {
							const Op& o = m_ops[i];
							if (!is_tmp(o.target))
								continue;

							const uint32_t ti = o.target.id();
							if (is_canceled_temp(ti) || m_temp2real[ti] != EntityBad)
								continue;

							if (o.type == OpType::ADD_ENTITY) {
								m_temp2real[ti] = m_world.add();
							} else if (o.type == OpType::CPY_ENTITY) {
								const Entity src = resolve(o.other);
								if (src != EntityBad)
									m_temp2real[ti] = m_world.copy(src);
							} else if (o.type == OpType::INSTANTIATE_ENTITY) {
								allocate_instantiate_group(i);
							}
						}
					}

					// Sort by (target, other), reduce last-wins, apply relations.
					// DEL_COMPONENT last per target.
					if (m_needsSort) {
						GAIA_PROF_SCOPE(cmdbuf::sort);

						m_needsSort = false;
						core::sort(m_ops.begin(), m_ops.end(), [](const Op& a, const Op& b) {
							if (less_op_key(a, b))
								return true;
							if (less_op_key(b, a))
								return false;
							return a.order < b.order;
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

					cnt::darray_ext<Entity, 16> deleteTargets;
					{
						GAIA_PROF_SCOPE(cmdbuf::merges);
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
								const Entity pairTargetKey = m_ops[i].pairTarget;
								const Entity othReal = resolve_object(m_ops[i]);

								// Group ops with same (target, other)
								uint32_t j = i + 1;
								while (j < q && m_ops[j].other == othKey && m_ops[j].pairTarget == pairTargetKey)
									++j;

								if (tgtReal != EntityBad) {
									const uint32_t groupSize = j - i;
									// Fast path - single op
									if (groupSize == 1) {
										const Op& op = m_ops[i];
										switch (op.type) {
											case OpType::DEL_COMPONENT:
												replay_del(tgtReal, othReal);
												break;
											case OpType::ADD_COMPONENT:
												replay_add(tgtReal, othReal);
												break;
											case OpType::ADD_COMPONENT_DATA:
												replay_add_data(tgtReal, othReal, op.off);
												break;
											case OpType::SET_COMPONENT:
												replay_data(tgtReal, othReal, op.off);
												break;
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
											replay_del(tgtReal, othReal);
										}
										// 3) ADD_WITH_DATA or ADD+SET = ADD_WITH_DATA
										else if (hasAddData || (hasAdd && hasSet)) {
											replay_add_data(tgtReal, othReal, dataPos);
										}
										// 4) ADD only
										else if (hasAdd) {
											replay_add(tgtReal, othReal);
										}
										// 5) SET only
										else if (hasSet) {
											replay_data(tgtReal, othReal, dataPos);
										}
									}
								}

								// Advance to next component group
								i = j;
							}

							// Apply deletions after every relationship operation so sorting target groups cannot
							// invalidate an endpoint that a later group still needs to resolve.
							if (hasDelEntity)
								deleteTargets.push_back(tgtReal);

							// Advance to next target group
							p = q;
						}
					}
					for (auto entity: deleteTargets) {
						if (m_world.valid(entity))
							m_world.del(entity);
					}

					clear();
				}
			};
		} // namespace detail

		//! Single-threaded command buffer.
		using CommandBufferST = detail::CommandBuffer<AccessContextST>;
		//! Multi-threaded command buffer serialized by a spin lock.
		using CommandBufferMT = detail::CommandBuffer<AccessContextMT>;

		//! Creates a heap-allocated single-threaded command buffer.
		//! \param world World that receives the recorded operations on commit.
		//! \return Pointer to the new command buffer. Destroy it with cmd_buffer_destroy.
		inline CommandBufferST* cmd_buffer_st_create(World& world) {
			return new CommandBufferST(world);
		}
		//! Destroys a heap-allocated single-threaded command buffer.
		//! \param cmdBuffer Command buffer previously returned by cmd_buffer_st_create.
		inline void cmd_buffer_destroy(CommandBufferST& cmdBuffer) {
			delete &cmdBuffer;
		}
		//! Commits a single-threaded command buffer.
		//! \param cmdBuffer Command buffer to replay into the world.
		inline void cmd_buffer_commit(CommandBufferST& cmdBuffer) {
			cmdBuffer.commit();
		}

		//! Creates a heap-allocated multi-threaded command buffer.
		//! \param world World that receives the recorded operations on commit.
		//! \return Pointer to the new command buffer. Destroy it with cmd_buffer_destroy.
		inline CommandBufferMT* cmd_buffer_mt_create(World& world) {
			return new CommandBufferMT(world);
		}
		//! Destroys a heap-allocated multi-threaded command buffer.
		//! \param cmdBuffer Command buffer previously returned by cmd_buffer_mt_create.
		inline void cmd_buffer_destroy(CommandBufferMT& cmdBuffer) {
			delete &cmdBuffer;
		}
		//! Commits a multi-threaded command buffer.
		//! \param cmdBuffer Command buffer to replay into the world.
		inline void cmd_buffer_commit(CommandBufferMT& cmdBuffer) {
			cmdBuffer.commit();
		}
	} // namespace ecs
} // namespace gaia
