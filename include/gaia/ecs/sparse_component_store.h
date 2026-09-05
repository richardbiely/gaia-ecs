#pragma once
#include "gaia/config/config.h"

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "gaia/cnt/darray.h"
#include "gaia/cnt/map.h"
#include "gaia/cnt/sparse_storage.h"
#include "gaia/core/utility.h"
#include "gaia/ecs/component_cache_item.h"
#include "gaia/ecs/id.h"
#include "gaia/mem/mem_alloc.h"
#include "gaia/mem/smallblock_allocator.h"

//! \cond INTERNAL
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

		//! Runtime sparse component record owning one aligned payload allocation.
		struct RuntimeSparseComponentRecord {
			//! Entity owning the sparse value.
			Entity entity;
			//! Aligned runtime payload bytes. Null for tags.
			void* pData = nullptr;
		};
	} // namespace ecs

	namespace cnt {
		template <typename T>
		struct to_sparse_id<ecs::SparseComponentRecord<T>> {
			static sparse_id get(const ecs::SparseComponentRecord<T>& item) noexcept {
				return (sparse_id)item.entity.id();
			}
		};

		template <>
		struct to_sparse_id<ecs::RuntimeSparseComponentRecord> {
			static sparse_id get(const ecs::RuntimeSparseComponentRecord& item) noexcept {
				return (sparse_id)item.entity.id();
			}
		};
	} // namespace cnt

	namespace ecs {
		namespace detail {
			//! Type-erased sparse component store callbacks.
			struct SparseComponentStoreErased {
				//! Concrete sparse store instance.
				void* pStore = nullptr;
				//! Adds or returns one entity payload.
				void* (*func_add)(void*, Entity) = nullptr;
				//! Returns one mutable entity payload.
				void* (*func_mut)(void*, Entity) = nullptr;
				//! Returns one read-only entity payload.
				const void* (*func_get)(const void*, Entity) = nullptr;
				//! Returns one read-only ordinary entity payload without pair dispatch.
				const void* (*func_get_entity)(const void*, Entity) = nullptr;
				void (*func_del)(void*, Entity) = nullptr;
				bool (*func_has)(const void*, Entity) = nullptr;
				bool (*func_copy_entity)(void*, Entity, Entity) = nullptr;
				uint32_t (*func_count)(const void*) = nullptr;
				void (*func_collect_entities)(const void*, cnt::darray<Entity>&) = nullptr;
				bool (*func_each_entity)(const void*, void*, bool (*)(void*, Entity)) = nullptr;
				void (*func_clear_store)(void*) = nullptr;
				void (*func_del_store)(void*) = nullptr;
			};

			//! Runtime-sized sparse component values stored outside archetype rows.
			struct RuntimeSparseComponentStore final {
				GAIA_USE_SMALLBLOCK(RuntimeSparseComponentStore)
				//! Target upper bound for one payload page.
				static constexpr uint32_t PayloadPageBytes = 16U * 1024U;
				//! Maximum number of payload slots reserved by one page.
				static constexpr uint32_t MaxPayloadsPerPage = 256U;

				//! Entity-to-payload sparse index.
				cnt::sparse_storage<RuntimeSparseComponentRecord> data;
				//! Full-identity payload index for exact pair records.
				cnt::map<EntityLookupKey, RuntimeSparseComponentRecord> pairData;
				//! Aligned pages owning runtime payload slots.
				cnt::darray<void*> payloadPages;
				//! Registered runtime component metadata.
				const ComponentCacheItem* pItem = nullptr;
				//! Intrusive free-list head stored in released payload slots.
				void* pFreePayload = nullptr;
				//! Aligned byte stride between payload slots.
				uint32_t payloadStride = 0;
				//! Alignment used for payload pages and slots.
				uint32_t payloadAlignment = 0;
				//! Number of slots in each payload page.
				uint32_t payloadsPerPage = 0;
				//! Next unused slot in the current payload page.
				uint32_t nextPayload = 0;

				explicit RuntimeSparseComponentStore(const ComponentCacheItem& item): pItem(&item) {
					const auto size = item.comp.size();
					if (size == 0)
						return;

					const auto itemAlignment = item.comp.alig();
					payloadAlignment = itemAlignment < alignof(void*) ? (uint32_t)alignof(void*) : itemAlignment;
					const auto payloadSize = size < sizeof(void*) ? (uint32_t)sizeof(void*) : size;
					payloadStride = mem::align(payloadSize, payloadAlignment);
					payloadsPerPage = PayloadPageBytes / payloadStride;
					if (payloadsPerPage == 0)
						payloadsPerPage = 1;
					else if (payloadsPerPage > MaxPayloadsPerPage)
						payloadsPerPage = MaxPayloadsPerPage;
					nextPayload = payloadsPerPage;
				}

				static cnt::sparse_id sid(Entity entity) {
					GAIA_ASSERT(!entity.pair());
					return (cnt::sparse_id)entity.id();
				}

				//! Allocates and constructs one runtime payload.
				//! \return Payload pointer, or nullptr for a tag component.
				GAIA_NODISCARD void* create_payload() {
					const auto size = pItem->comp.size();
					if (size == 0)
						return nullptr;

					auto* pData = alloc_payload();
					GAIA_ASSERT(pData != nullptr);
					if (pItem->func_ctor != nullptr)
						pItem->func_ctor(pData, 1);
					return pData;
				}

				//! Destroys and releases one runtime payload.
				//! \param pData Payload pointer, or nullptr for a tag component.
				void destroy_payload(void* pData) {
					if (pData == nullptr)
						return;

					pItem->dtor(pData);
					free_payload(pData);
				}

				//! Allocates one stable payload slot from the free list or current page.
				GAIA_NODISCARD void* alloc_payload() {
					if (pFreePayload != nullptr) {
						auto* pData = pFreePayload;
						memcpy(&pFreePayload, pData, sizeof(pFreePayload));
						return pData;
					}

					if (nextPayload == payloadsPerPage) {
						const auto pageBytes = payloadStride * payloadsPerPage;
						auto* pPage = payloadAlignment <= alignof(std::max_align_t)
															? mem::mem_alloc("Runtime sparse payload page", pageBytes)
															: mem::mem_alloc_alig("Runtime sparse payload page", pageBytes, payloadAlignment);
						GAIA_ASSERT(pPage != nullptr);
						payloadPages.push_back(pPage);
						nextPayload = 0;
					}

					auto* pData = (uint8_t*)payloadPages.back() + ((uintptr_t)payloadStride * nextPayload);
					++nextPayload;
					return pData;
				}

				//! Returns one destroyed payload slot to the intrusive free list.
				void free_payload(void* pData) {
					memcpy(pData, &pFreePayload, sizeof(pFreePayload));
					pFreePayload = pData;
				}

				//! Releases every owned payload page after all live values are destroyed.
				void free_payload_pages() {
					for (auto* pPage: payloadPages) {
						if (payloadAlignment <= alignof(std::max_align_t))
							mem::mem_free("Runtime sparse payload page", pPage);
						else
							mem::mem_free_alig("Runtime sparse payload page", pPage);
					}
					payloadPages.clear();
					pFreePayload = nullptr;
					nextPayload = payloadsPerPage;
				}

				void* add(Entity entity) {
					if GAIA_UNLIKELY (entity.pair()) {
						const auto key = EntityLookupKey(entity);
						const auto it = pairData.find(key);
						if (it != pairData.end())
							return it->second.pData;

						auto* pData = create_payload();
						return pairData.emplace(key, RuntimeSparseComponentRecord{entity, pData}).first->second.pData;
					}

					const auto sparseId = sid(entity);
					if (data.has(sparseId))
						return data[sparseId].pData;

					auto* pData = create_payload();
					data.add(RuntimeSparseComponentRecord{entity, pData});
					return pData;
				}

				void* mut(Entity entity) {
					if GAIA_UNLIKELY (entity.pair()) {
						const auto it = pairData.find(EntityLookupKey(entity));
						GAIA_ASSERT(it != pairData.end());
						return it != pairData.end() ? it->second.pData : nullptr;
					}

					GAIA_ASSERT(data.has(sid(entity)));
					return data[sid(entity)].pData;
				}

				const void* get(Entity entity) const {
					if GAIA_UNLIKELY (entity.pair()) {
						const auto it = pairData.find(EntityLookupKey(entity));
						GAIA_ASSERT(it != pairData.end());
						return it != pairData.end() ? it->second.pData : nullptr;
					}

					GAIA_ASSERT(data.has(sid(entity)));
					return data[sid(entity)].pData;
				}

				//! Returns an ordinary entity payload without testing for pair identity.
				//! \param entity Ordinary entity owning the payload.
				//! \return Read-only runtime payload pointer.
				GAIA_NODISCARD const void* get_entity(Entity entity) const {
					GAIA_ASSERT(!entity.pair());
					GAIA_ASSERT(data.has(sid(entity)));
					return data[sid(entity)].pData;
				}

				void del_entity(Entity entity) {
					if GAIA_UNLIKELY (entity.pair()) {
						const auto it = pairData.find(EntityLookupKey(entity));
						if (it == pairData.end())
							return;

						destroy_payload(it->second.pData);
						pairData.erase(it);
						return;
					}

					const auto sparseId = sid(entity);
					if (!data.has(sparseId))
						return;

					destroy_payload(data[sparseId].pData);
					data.del(sparseId);
				}

				bool has(Entity entity) const {
					if GAIA_UNLIKELY (entity.pair())
						return pairData.contains(EntityLookupKey(entity));

					return data.has(sid(entity));
				}

				bool copy_entity(Entity dstEntity, Entity srcEntity) {
					if (!has(srcEntity))
						return false;

					auto* pDst = add(dstEntity);
					const auto* pSrc = get(srcEntity);
					if (pDst != nullptr)
						pItem->copy(pDst, pSrc, 0, 0, pItem->comp.size(), pItem->comp.size());
					return true;
				}

				uint32_t count() const {
					return (uint32_t)(data.size() + pairData.size());
				}

				void collect_entities(cnt::darray<Entity>& out) const {
					out.reserve(out.size() + count());
					for (const auto& item: data)
						out.push_back(item.entity);

					const auto firstPair = out.size();
					for (const auto& item: pairData)
						out.push_back(item.first.entity());
					core::sort(out.begin() + firstPair, out.end(), [](Entity left, Entity right) {
						return left.value() < right.value();
					});
				}

				//! Visits every sparse payload owner in deterministic order.
				//! \param ctx Opaque callback context.
				//! \param func Callback returning false to stop iteration.
				//! \return False when iteration was stopped by the callback, true otherwise.
				GAIA_NODISCARD bool each_entity(void* ctx, bool (*func)(void*, Entity)) const {
					for (const auto& item: data) {
						if (!func(ctx, item.entity))
							return false;
					}

					if (pairData.empty())
						return true;

					cnt::darray<Entity> pairEntities;
					pairEntities.reserve((uint32_t)pairData.size());
					for (const auto& item: pairData)
						pairEntities.push_back(item.first.entity());
					core::sort(pairEntities.begin(), pairEntities.end(), [](Entity left, Entity right) {
						return left.value() < right.value();
					});
					for (auto entity: pairEntities) {
						if (!func(ctx, entity))
							return false;
					}
					return true;
				}

				void clear_store() {
					while (!data.empty())
						del_entity(data.begin()->entity);
					while (!pairData.empty())
						del_entity(pairData.begin()->first.entity());
					free_payload_pages();
				}
			};

			//! Typed sparse component values stored outside archetype rows.
			template <typename T>
			struct SparseComponentStore final {
				GAIA_USE_SMALLBLOCK(SparseComponentStore)

				cnt::sparse_storage<SparseComponentRecord<T>> data;
				//! Full-identity payload index for exact pair records.
				cnt::map<EntityLookupKey, SparseComponentRecord<T>> pairData;

				static cnt::sparse_id sid(Entity entity) {
					GAIA_ASSERT(!entity.pair());
					return (cnt::sparse_id)entity.id();
				}

				T& add(Entity entity) {
					if GAIA_UNLIKELY (entity.pair()) {
						const auto key = EntityLookupKey(entity);
						const auto it = pairData.find(key);
						if (it != pairData.end())
							return it->second.value;

						return pairData.emplace(key, SparseComponentRecord<T>{entity}).first->second.value;
					}

					const auto sparseId = sid(entity);
					if (data.has(sparseId))
						return data[sparseId].value;

					auto& item = data.add(SparseComponentRecord<T>{entity});
					return item.value;
				}

				T& mut(Entity entity) {
					if GAIA_UNLIKELY (entity.pair()) {
						const auto it = pairData.find(EntityLookupKey(entity));
						GAIA_ASSERT(it != pairData.end());
						return it->second.value;
					}

					GAIA_ASSERT(data.has(sid(entity)));
					return data[sid(entity)].value;
				}

				const T& get(Entity entity) const {
					if GAIA_UNLIKELY (entity.pair()) {
						return get_pair(entity);
					}

					return get_entity(entity);
				}

				//! Returns an ordinary entity payload without testing for pair identity.
				//! \param entity Ordinary entity owning the payload.
				//! \return Read-only sparse component value.
				GAIA_NODISCARD const T& get_entity(Entity entity) const {
					GAIA_ASSERT(!entity.pair());
					GAIA_ASSERT(data.has(sid(entity)));
					return data[sid(entity)].value;
				}

				//! Returns an exact pair record payload from the full-identity index.
				//! \param entity Exact pair record owning the payload.
				//! \return Read-only sparse component value.
				GAIA_NODISCARD const T& get_pair(Entity entity) const {
					GAIA_ASSERT(entity.pair());
					const auto it = pairData.find(EntityLookupKey(entity));
					GAIA_ASSERT(it != pairData.end());
					return it->second.value;
				}

				//! Finds a mutable payload using ordinary or exact-pair identity.
				//! \param entity Entity or exact pair record to find.
				//! \return Payload pointer, or nullptr when absent.
				GAIA_NODISCARD T* try_mut(Entity entity) {
					if GAIA_UNLIKELY (entity.pair()) {
						const auto it = pairData.find(EntityLookupKey(entity));
						return it != pairData.end() ? &it->second.value : nullptr;
					}

					const auto sparseId = sid(entity);
					return data.has(sparseId) ? &data[sparseId].value : nullptr;
				}

				//! Finds a read-only payload using ordinary or exact-pair identity.
				//! \param entity Entity or exact pair record to find.
				//! \return Payload pointer, or nullptr when absent.
				GAIA_NODISCARD const T* try_get(Entity entity) const {
					if GAIA_UNLIKELY (entity.pair()) {
						const auto it = pairData.find(EntityLookupKey(entity));
						return it != pairData.end() ? &it->second.value : nullptr;
					}

					const auto sparseId = sid(entity);
					return data.has(sparseId) ? &data[sparseId].value : nullptr;
				}

				void del_entity(Entity entity) {
					if GAIA_UNLIKELY (entity.pair()) {
						pairData.erase(EntityLookupKey(entity));
						return;
					}

					const auto sparseId = sid(entity);
					if (data.has(sparseId))
						data.del(sparseId);
				}

				bool has(Entity entity) const {
					if GAIA_UNLIKELY (entity.pair())
						return pairData.contains(EntityLookupKey(entity));

					return data.has(sid(entity));
				}

				bool copy_entity(Entity dstEntity, Entity srcEntity) {
					if (!has(srcEntity))
						return false;
					add(dstEntity) = get(srcEntity);
					return true;
				}

				uint32_t count() const {
					return (uint32_t)(data.size() + pairData.size());
				}

				void collect_entities(cnt::darray<Entity>& out) const {
					out.reserve(out.size() + count());
					for (const auto& item: data)
						out.push_back(item.entity);

					const auto firstPair = out.size();
					for (const auto& item: pairData)
						out.push_back(item.first.entity());
					core::sort(out.begin() + firstPair, out.end(), [](Entity left, Entity right) {
						return left.value() < right.value();
					});
				}

				//! Visits every sparse payload owner in deterministic order.
				//! \param ctx Opaque callback context.
				//! \param func Callback returning false to stop iteration.
				//! \return False when iteration was stopped by the callback, true otherwise.
				GAIA_NODISCARD bool each_entity(void* ctx, bool (*func)(void*, Entity)) const {
					for (const auto& item: data) {
						if (!func(ctx, item.entity))
							return false;
					}

					if (pairData.empty())
						return true;

					cnt::darray<Entity> pairEntities;
					pairEntities.reserve((uint32_t)pairData.size());
					for (const auto& item: pairData)
						pairEntities.push_back(item.first.entity());
					core::sort(pairEntities.begin(), pairEntities.end(), [](Entity left, Entity right) {
						return left.value() < right.value();
					});
					for (auto entity: pairEntities) {
						if (!func(ctx, entity))
							return false;
					}
					return true;
				}

				void clear_store() {
					data.clear();
					pairData.clear();
				}
			};

			template <typename Store>
			static SparseComponentStoreErased make_sparse_component_store_erased(Store* pStore) {
				SparseComponentStoreErased store{};
				store.pStore = pStore;
				store.func_add = [](void* pStoreRaw, Entity entity) {
					if constexpr (std::is_pointer_v<decltype(static_cast<Store*>(pStoreRaw)->add(entity))>)
						return static_cast<Store*>(pStoreRaw)->add(entity);
					else
						return (void*)&static_cast<Store*>(pStoreRaw)->add(entity);
				};
				store.func_mut = [](void* pStoreRaw, Entity entity) {
					if constexpr (std::is_pointer_v<decltype(static_cast<Store*>(pStoreRaw)->mut(entity))>)
						return static_cast<Store*>(pStoreRaw)->mut(entity);
					else
						return (void*)&static_cast<Store*>(pStoreRaw)->mut(entity);
				};
				store.func_get = [](const void* pStoreRaw, Entity entity) {
					if constexpr (std::is_pointer_v<decltype(static_cast<const Store*>(pStoreRaw)->get(entity))>)
						return static_cast<const Store*>(pStoreRaw)->get(entity);
					else
						return (const void*)&static_cast<const Store*>(pStoreRaw)->get(entity);
				};
				store.func_get_entity = [](const void* pStoreRaw, Entity entity) {
					if constexpr (std::is_pointer_v<decltype(static_cast<const Store*>(pStoreRaw)->get_entity(entity))>)
						return static_cast<const Store*>(pStoreRaw)->get_entity(entity);
					else
						return (const void*)&static_cast<const Store*>(pStoreRaw)->get_entity(entity);
				};
				store.func_del = [](void* pStoreRaw, Entity entity) {
					static_cast<Store*>(pStoreRaw)->del_entity(entity);
				};
				store.func_has = [](const void* pStoreRaw, Entity entity) {
					return static_cast<const Store*>(pStoreRaw)->has(entity);
				};
				store.func_copy_entity = [](void* pStoreRaw, Entity dstEntity, Entity srcEntity) {
					return static_cast<Store*>(pStoreRaw)->copy_entity(dstEntity, srcEntity);
				};
				store.func_count = [](const void* pStoreRaw) {
					return static_cast<const Store*>(pStoreRaw)->count();
				};
				store.func_collect_entities = [](const void* pStoreRaw, cnt::darray<Entity>& out) {
					static_cast<const Store*>(pStoreRaw)->collect_entities(out);
				};
				store.func_each_entity = [](const void* pStoreRaw, void* pCtx, bool (*func)(void*, Entity)) {
					return static_cast<const Store*>(pStoreRaw)->each_entity(pCtx, func);
				};
				store.func_clear_store = [](void* pStoreRaw) {
					static_cast<Store*>(pStoreRaw)->clear_store();
				};
				store.func_del_store = [](void* pStoreRaw) {
					delete static_cast<Store*>(pStoreRaw);
				};
				return store;
			}
		} // namespace detail
	} // namespace ecs
} // namespace gaia
//! \endcond
