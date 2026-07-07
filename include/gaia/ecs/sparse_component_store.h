#pragma once
#include "gaia/config/config.h"

#include <cstdint>

#include "gaia/cnt/darray.h"
#include "gaia/cnt/sparse_storage.h"
#include "gaia/ecs/id.h"
#include "gaia/mem/smallblock_allocator.h"

namespace gaia {
	namespace ecs {
		//! Sparse component record keyed by entity id.
		template <typename T>
		struct SparseComponentRecord {
			//! Entity owning the sparse value.
			Entity entity;
			//! Sparse component value.
			T value{};
		};
	} // namespace ecs

	namespace cnt {
		template <typename T>
		struct to_sparse_id<ecs::SparseComponentRecord<T>> {
			static sparse_id get(const ecs::SparseComponentRecord<T>& item) noexcept {
				return (sparse_id)item.entity.id();
			}
		};
	} // namespace cnt

	namespace ecs {
		namespace detail {
			//! Type-erased sparse component store callbacks.
			struct SparseComponentStoreErased {
				void* pStore = nullptr;
				void (*func_del)(void*, Entity) = nullptr;
				bool (*func_has)(const void*, Entity) = nullptr;
				bool (*func_copy_entity)(void*, Entity, Entity) = nullptr;
				uint32_t (*func_count)(const void*) = nullptr;
				void (*func_collect_entities)(const void*, cnt::darray<Entity>&) = nullptr;
				bool (*func_for_each_entity)(const void*, void*, bool (*)(void*, Entity)) = nullptr;
				void (*func_clear_store)(void*) = nullptr;
				void (*func_del_store)(void*) = nullptr;
			};

			//! Typed sparse component values stored outside archetype rows.
			template <typename T>
			struct SparseComponentStore final {
				GAIA_USE_SMALLBLOCK(SparseComponentStore)

				cnt::sparse_storage<SparseComponentRecord<T>> data;

				static cnt::sparse_id sid(Entity entity) {
					return (cnt::sparse_id)entity.id();
				}

				T& add(Entity entity) {
					const auto sparseId = sid(entity);
					if (data.has(sparseId))
						return data[sparseId].value;

					auto& item = data.add(SparseComponentRecord<T>{entity});
					return item.value;
				}

				T& mut(Entity entity) {
					GAIA_ASSERT(data.has(sid(entity)));
					return data[sid(entity)].value;
				}

				const T& get(Entity entity) const {
					GAIA_ASSERT(data.has(sid(entity)));
					return data[sid(entity)].value;
				}

				void del_entity(Entity entity) {
					const auto sparseId = sid(entity);
					if (!data.empty() && data.has(sparseId))
						data.del(sparseId);
				}

				bool has(Entity entity) const {
					if (data.empty())
						return false;
					return data.has(sid(entity));
				}

				uint32_t count() const {
					return (uint32_t)data.size();
				}

				void collect_entities(cnt::darray<Entity>& out) const {
					out.reserve(out.size() + (uint32_t)data.size());
					for (const auto& item: data)
						out.push_back(item.entity);
				}

				void clear_store() {
					data.clear();
				}
			};

			template <typename T>
			static SparseComponentStoreErased make_sparse_component_store_erased(SparseComponentStore<T>* pStore) {
				SparseComponentStoreErased store{};
				store.pStore = pStore;
				store.func_del = [](void* pStoreRaw, Entity entity) {
					static_cast<SparseComponentStore<T>*>(pStoreRaw)->del_entity(entity);
				};
				store.func_has = [](const void* pStoreRaw, Entity entity) {
					return static_cast<const SparseComponentStore<T>*>(pStoreRaw)->has(entity);
				};
				store.func_copy_entity = [](void* pStoreRaw, Entity dstEntity, Entity srcEntity) {
					auto* pStore = static_cast<SparseComponentStore<T>*>(pStoreRaw);
					if (!pStore->has(srcEntity))
						return false;

					auto& dst = pStore->add(dstEntity);
					dst = pStore->get(srcEntity);
					return true;
				};
				store.func_count = [](const void* pStoreRaw) {
					return static_cast<const SparseComponentStore<T>*>(pStoreRaw)->count();
				};
				store.func_collect_entities = [](const void* pStoreRaw, cnt::darray<Entity>& out) {
					static_cast<const SparseComponentStore<T>*>(pStoreRaw)->collect_entities(out);
				};
				store.func_for_each_entity = [](const void* pStoreRaw, void* pCtx, bool (*func)(void*, Entity)) {
					for (const auto& item: static_cast<const SparseComponentStore<T>*>(pStoreRaw)->data) {
						if (!func(pCtx, item.entity))
							return false;
					}
					return true;
				};
				store.func_clear_store = [](void* pStoreRaw) {
					static_cast<SparseComponentStore<T>*>(pStoreRaw)->clear_store();
				};
				store.func_del_store = [](void* pStoreRaw) {
					delete static_cast<SparseComponentStore<T>*>(pStoreRaw);
				};
				return store;
			}
		} // namespace detail
	} // namespace ecs
} // namespace gaia
