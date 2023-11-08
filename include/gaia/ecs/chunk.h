#pragma once
#include "../config/config.h"

#include <cinttypes>
#include <cstdint>
#include <tuple>
#include <type_traits>
#include <utility>

#include "../cnt/sarray_ext.h"
#include "../config/profiler.h"
#include "../core/utility.h"
#include "archetype_common.h"
#include "chunk_allocator.h"
#include "chunk_header.h"
#include "common.h"
#include "component.h"
#include "component_cache.h"
#include "component_desc.h"
#include "component_utils.h"
#include "entity.h"
#include "entity_container.h"

namespace gaia {
	namespace ecs {
		class Chunk final {
		public:
			static constexpr uint32_t MAX_COMPONENTS_BITS = 5U;
			//! Maximum number of components on archetype
			static constexpr uint32_t MAX_COMPONENTS = 1U << MAX_COMPONENTS_BITS;

			using ComponentArray = cnt::sarray_ext<Component, MAX_COMPONENTS>;
#if GAIA_COMP_ID_PROBING
			using ComponentIdInterMap = cnt::sarray_ext<ComponentId, MAX_COMPONENTS>;
#endif
			using ComponentOffsetArray = cnt::sarray_ext<ChunkDataOffset, MAX_COMPONENTS>;

			// TODO: Make this private
			//! Chunk header
			ChunkHeader m_header;
			//! Pointers to various parts of data inside chunk
			ChunkRecords m_records;

		private:
			//! Pointer to where the chunk data starts.
			//! Data layed out as following:
			//!			1) ComponentVersions[ComponentKind::CK_Gen]
			//!			2) ComponentVersions[ComponentKind::CK_Uni]
			//!     3) ComponentIds[ComponentKind::CK_Gen]
			//!			4) ComponentIds[ComponentKind::CK_Uni]
			//!			5) ComponentRecords[ComponentKind::CK_Gen]
			//!			6) ComponentRecords[ComponentKind::CK_Uni]
			//!			7) Entities
			//!			8) Components
			//! Note, root archetypes store only entites, therefore it is fully occupied with entities.
			uint8_t m_data[1];

			GAIA_MSVC_WARNING_PUSH()
			GAIA_MSVC_WARNING_DISABLE(26495)

			// Hidden default constructor. Only use to calculate the relative offset of m_data
			Chunk() = default;

			Chunk(uint32_t chunkIndex, uint16_t capacity, uint16_t st, uint32_t& worldVersion):
					m_header(chunkIndex, capacity, st, worldVersion) {
				// Chunk data area consist of memory offsets, entities and component data. Normally. we would need
				// to in-place construct all of it manually.
				// However, the memory offsets and entities are all trivial types and components are initialized via
				// their constructors on-demand (if not trivial) so we do not really need to do any construction here.
			}

			GAIA_MSVC_WARNING_POP()

			void init(
					const cnt::sarray<ComponentArray, ComponentKind::CK_Count>& comps,
#if GAIA_COMP_ID_PROBING
					const cnt::sarray<ComponentIdInterMap, ComponentKind::CK_Count>& compMap,
#endif
					const ChunkDataOffsets& headerOffsets,
					const cnt::sarray<ComponentOffsetArray, ComponentKind::CK_Count>& compOffs) {
				m_header.componentCount[ComponentKind::CK_Gen] = (uint8_t)comps[ComponentKind::CK_Gen].size();
				m_header.componentCount[ComponentKind::CK_Uni] = (uint8_t)comps[ComponentKind::CK_Uni].size();

				const auto& cc = ComponentCache::get();

				// Cache pointers to versions
				GAIA_FOR(ComponentKind::CK_Count) {
					if (comps[i].empty())
						continue;

					m_records.pVersions[i] = (ComponentVersion*)&data(headerOffsets.firstByte_Versions[i]);
				}

				// Cache component ids
				GAIA_FOR(ComponentKind::CK_Count) {
					if (comps[i].empty())
						continue;

					auto* dst = m_records.pComponentIds[i] = (ComponentId*)&data(headerOffsets.firstByte_ComponentIds[i]);
					const auto& cids = comps[i];

					// We treat the component array as if were MAX_COMPONENTS long.
					// Real size can be smaller.
					uint32_t j = 0;
					for (; j < cids.size(); ++j)
						dst[j] = cids[j].id();
					for (; j < MAX_COMPONENTS; ++j)
						dst[j] = ComponentIdBad;
				}

				// Cache component records
				GAIA_FOR(ComponentKind::CK_Count) {
					if (comps[i].empty())
						continue;

					auto* dst = m_records.pRecords[i] = (ComponentRecord*)&data(headerOffsets.firstByte_Records[i]);
					const auto& offs = compOffs[i];
					const auto& cids = comps[i];
					GAIA_EACH_(cids, j) {
						dst[j].comp = cids[j];
						const auto off = offs[j];
						dst[j].pData = &data(off);
						dst[j].pDesc = &cc.comp_desc(cids[j].id());
					}
				}

#if GAIA_COMP_ID_PROBING
				// Copy provided component id hash map
				GAIA_FOR(ComponentKind::CK_Count) {
					if (comps[i].empty())
						continue;

					const auto& map = compMap[i];
					auto* dst = comp_id_mapping_ptr_mut((ComponentKind)i);
					GAIA_EACH_(map, j) dst[j] = map[j];
				}
#endif

				m_records.pEntities = (Entity*)&data(headerOffsets.firstByte_EntityData);

				// Now that records are set, we use the cached component descriptors to set ctor/dtor masks.
				{
					auto recs = comp_rec_view(ComponentKind::CK_Gen);
					for (const auto& rec: recs) {
						m_header.hasAnyCustomGenCtor |= (rec.pDesc->func_ctor != nullptr);
						m_header.hasAnyCustomGenDtor |= (rec.pDesc->func_dtor != nullptr);
					}
				}
				{
					auto recs = comp_rec_view(ComponentKind::CK_Uni);
					for (const auto& rec: recs) {
						m_header.hasAnyCustomUniCtor |= (rec.pDesc->func_ctor != nullptr);
						m_header.hasAnyCustomUniDtor |= (rec.pDesc->func_dtor != nullptr);
					}

					// Also construct unique components if possible
					if (has_custom_uni_ctor())
						call_ctors(ComponentKind::CK_Uni, 0, 1);
				}
			}

			GAIA_NODISCARD std::span<const ComponentVersion> comp_version_view(ComponentKind compKind) const {
				return {(const ComponentVersion*)m_records.pVersions[compKind], m_header.componentCount[compKind]};
			}

			GAIA_NODISCARD std::span<ComponentVersion> comp_version_view_mut(ComponentKind compKind) {
				return {m_records.pVersions[compKind], m_header.componentCount[compKind]};
			}

#if GAIA_COMP_ID_PROBING
			GAIA_NODISCARD const ComponentId* comp_id_mapping_ptr(ComponentKind compKind) const {
				return (const ComponentId*)m_records.compMap[compKind];
			}

			GAIA_NODISCARD ComponentId* comp_id_mapping_ptr_mut(ComponentKind compKind) {
				return m_records.compMap[compKind];
			}
#endif

			GAIA_NODISCARD std::span<Entity> entity_view_mut() {
				return {m_records.pEntities, size()};
			}

			/*!
			Returns a read-only span of the component data.
			\warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			\tparam T Component
			\return Span of read-only component data.
			*/
			template <typename T>
			GAIA_NODISCARD GAIA_FORCEINLINE auto view_inter(uint32_t from, uint32_t to) const
					-> decltype(std::span<const uint8_t>{}) {
				using U = typename component_type_t<T>::Type;

				GAIA_ASSERT(to <= size());

				if constexpr (std::is_same_v<U, Entity>) {
					return {(const uint8_t*)&m_records.pEntities[from], to - from};
				} else {
					static_assert(!std::is_empty_v<U>, "Attempting to get value of an empty component");

					constexpr auto compKind = component_kind_v<T>;
					const auto compId = comp_id<T>();
					const auto compIdx = comp_idx(compKind, compId);

					if constexpr (compKind == ComponentKind::CK_Gen) {
						return {comp_ptr(compKind, compIdx, from), to - from};
					} else {
						// GAIA_ASSERT(count == 1); we don't really care and always consider 1 for unique components
						return {comp_ptr(compKind, compIdx), 1};
					}
				}
			}

			/*!
			Returns a read-write span of the component data. Also updates the world version for the component.
			\warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			\tparam T Component
			\tparam WorldVersionUpdateWanted If true, the world version is updated as a result of the write access
			\return Span of read-write component data.
			*/
			template <typename T, bool WorldVersionUpdateWanted>
			GAIA_NODISCARD GAIA_FORCEINLINE auto view_mut_inter(uint32_t from, uint32_t to)
					-> decltype(std::span<uint8_t>{}) {
				using U = typename component_type_t<T>::Type;

				GAIA_ASSERT(to <= size());

				if constexpr (std::is_same_v<U, Entity>) {
					static_assert(!WorldVersionUpdateWanted);

					return {(uint8_t*)&m_records.pEntities[from], to - from};
				} else {
					static_assert(!std::is_empty_v<U>, "Attempting to set value of an empty component");

					constexpr auto compKind = component_kind_v<T>;
					const auto compId = comp_id<T>();
					const auto compIdx = comp_idx(compKind, compId);

					// Update version number if necessary so we know RW access was used on the chunk
					if constexpr (WorldVersionUpdateWanted)
						update_world_version(compKind, compIdx);

					if constexpr (compKind == ComponentKind::CK_Gen) {
						return {comp_ptr_mut(compKind, compIdx, from), to - from};
					} else {
						// GAIA_ASSERT(count == 1); we don't really care and always consider 1 for unique components
						return {comp_ptr_mut(compKind, compIdx), 1};
					}
				}
			}

			/*!
			Returns the value stored in the component \tparam T on \param index in the chunk.
			\warning It is expected the \param index is valid. Undefined behavior otherwise.
			\warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			\tparam T Component
			\param index Index of entity in the chunk
			\return Value stored in the component if smaller than 8 bytes. Const reference to the value otherwise.
			*/
			template <typename T>
			GAIA_NODISCARD decltype(auto) comp_inter(uint32_t index) const {
				using U = typename component_type_t<T>::Type;
				using RetValueType = decltype(view<T>()[0]);

				GAIA_ASSERT(index < m_header.count);
				if constexpr (sizeof(RetValueType) > 8)
					return (const U&)view<T>()[index];
				else
					return view<T>()[index];
			}

			/*!
			Removes the entity at from the chunk and updates the world versions
			*/
			void remove_last_entity_inter() {
				// Should never be called over an empty chunk
				GAIA_ASSERT(!empty());
				--m_header.count;
				--m_header.countEnabled;
			}

		public:
			Chunk(const Chunk& chunk) = delete;
			Chunk(Chunk&& chunk) = delete;
			Chunk& operator=(const Chunk& chunk) = delete;
			Chunk& operator=(Chunk&& chunk) = delete;
			~Chunk() = default;

			static constexpr uint16_t chunk_header_size() {
				const auto dataAreaOffset =
						// ChunkAllocator reserves the first few bytes for internal purposes
						MemoryBlockUsableOffset +
						// Chunk "header" area (before actuall entity/component data starts)
						sizeof(ChunkHeader) + sizeof(ChunkRecords);
				static_assert(dataAreaOffset < UINT16_MAX);
				return dataAreaOffset;
			}

			static constexpr uint16_t chunk_total_bytes(uint16_t dataSize) {
				return chunk_header_size() + dataSize;
			}

			static constexpr uint16_t chunk_data_bytes(uint16_t totalSize) {
				return totalSize - chunk_header_size();
			}

			//! Returns the relative offset of m_data in Chunk
			static uintptr_t chunk_data_area_offset() {
				// Note, offsetof is implementation-defined and conditionally-supported since C++17.
				// Therefore, we instantiate the chunk and calculate the relative address ourselves.
				Chunk chunk;
				const auto chunk_offset = (uintptr_t)&chunk;
				const auto data_offset = (uintptr_t)&chunk.m_data[0];
				return data_offset - chunk_offset;
			}

			/*!
			Allocates memory for a new chunk.
			\param chunkIndex Index of this chunk within the parent archetype
			\return Newly allocated chunk
			*/
			static Chunk* create(
					uint32_t chunkIndex, uint16_t capacity, uint16_t dataBytes, uint32_t& worldVersion,
					const ChunkDataOffsets& offsets, const cnt::sarray<ComponentArray, ComponentKind::CK_Count>& comps,
#if GAIA_COMP_ID_PROBING
					const cnt::sarray<ComponentIdInterMap, ComponentKind::CK_Count>& compMap,
#endif
					const cnt::sarray<ComponentOffsetArray, ComponentKind::CK_Count>& compOffs) {
				const auto totalBytes = chunk_total_bytes(dataBytes);
				const auto sizeType = detail::ChunkAllocatorImpl::mem_block_size_type(totalBytes);
#if GAIA_ECS_CHUNK_ALLOCATOR
				auto* pChunk = (Chunk*)ChunkAllocator::get().alloc(totalBytes);
				new (pChunk) Chunk(chunkIndex, capacity, sizeType, worldVersion);
#else
				GAIA_ASSERT(totalBytes <= MaxMemoryBlockSize);
				const auto allocSize = detail::ChunkAllocatorImpl::mem_block_size(sizeType);
				auto* pChunkMem = new uint8_t[allocSize];
				auto* pChunk = new (pChunkMem) Chunk(chunkIndex, capacity, sizeType, worldVersion);
#endif

#if GAIA_COMP_ID_PROBING
				pChunk->init(comps, compMap, offsets, compOffs);
#else
				pChunk->init(comps, offsets, compOffs);
#endif

				return pChunk;
			}

			/*!
			Releases all memory allocated by \param pChunk.
			\param pChunk Chunk which we want to destroy
			*/
			static void free(Chunk* pChunk) {
				GAIA_ASSERT(pChunk != nullptr);
				GAIA_ASSERT(!pChunk->dead());

				// Mark as dead
				pChunk->die();

				// Call destructors for components that need it
				if (pChunk->has_custom_gen_dtor())
					pChunk->call_dtors(ComponentKind::CK_Gen, 0, pChunk->size());
				if (pChunk->has_custom_uni_dtor())
					pChunk->call_dtors(ComponentKind::CK_Uni, 0, 1);

#if GAIA_ECS_CHUNK_ALLOCATOR
				pChunk->~Chunk();
				ChunkAllocator::get().free(pChunk);
#else
				pChunk->~Chunk();
				auto* pChunkMem = (uint8_t*)pChunk;
				delete pChunkMem;
#endif
			}

			/*!
			Remove the last entity from chunk.
			\param chunksToRemove Container of chunks ready for removal
			*/
			void remove_last_entity(cnt::darray<Chunk*>& chunksToRemove) {
				remove_last_entity_inter();

				// TODO: This needs cleaning up.
				//       Chunk should have no idea of the world and also should not store
				//       any states realted to its lifetime.
				if (!dying() && empty()) {
					// When the chunk is emptied we want it to be removed. We can't do it
					// right away and need to wait for world::gc() to be called.
					//
					// However, we need to prevent the following:
					//    1) chunk is emptied, add it to some removal list
					//    2) chunk is reclaimed
					//    3) chunk is emptied, add it to some removal list again
					//
					// Therefore, we have a flag telling us the chunk is already waiting to
					// be removed. The chunk might be reclaimed before garbage collection happens
					// but it simply ignores such requests. This way we always have at most one
					// record for removal for any given chunk.
					start_dying();

					chunksToRemove.push_back(this);
				}
			}

			//! Updates the version numbers for this chunk.
			void update_versions() {
				update_version(m_header.worldVersion);
				update_world_version(ComponentKind::CK_Gen);
				update_world_version(ComponentKind::CK_Uni);
			}

			/*!
			Returns a read-only entity or component view.
			\warning If \tparam T is a component it is expected it is present. Undefined behavior otherwise.
			\tparam T Component or Entity
			\param from First valid entity index
			\param to Last valid entity index
			\return Entity of component view with read-only access
			*/
			template <typename T>
			GAIA_NODISCARD decltype(auto) view(uint32_t from, uint32_t to) const {
				using U = typename component_type_t<T>::Type;

				return mem::auto_view_policy_get<U>{view_inter<T>(from, to)};
			}

			template <typename T>
			GAIA_NODISCARD decltype(auto) view() const {
				return view<T>(0, size());
			}

			/*!
			Returns a mutable entity or component view.
			\warning If \tparam T is a component it is expected it is present. Undefined behavior otherwise.
			\tparam T Component or Entity
			\param from First valid entity index
			\param to Last valid entity index
			\return Entity or component view with read-write access
			*/
			template <typename T>
			GAIA_NODISCARD decltype(auto) view_mut(uint32_t from, uint32_t to) {
				using U = typename component_type_t<T>::Type;
				static_assert(!std::is_same_v<U, Entity>);

				return mem::auto_view_policy_set<U>{view_mut_inter<T, true>(from, to)};
			}

			template <typename T>
			GAIA_NODISCARD decltype(auto) view_mut() {
				return view_mut<T>(0, size());
			}

			/*!
			Returns a mutable component view.
			Doesn't update the world version when the access is aquired.
			\warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			\tparam T Component
			\param from First valid entity index
			\param to Last valid entity index
			\return Component view with read-write access
			*/
			template <typename T>
			GAIA_NODISCARD decltype(auto) sview_mut(uint32_t from, uint32_t to) {
				using U = typename component_type_t<T>::Type;
				static_assert(!std::is_same_v<U, Entity>);

				return mem::auto_view_policy_set<U>{view_mut_inter<T, false>(from, to)};
			}

			template <typename T>
			GAIA_NODISCARD decltype(auto) sview_mut() {
				return sview_mut<T>(0, size());
			}

			/*!
			Returns either a mutable or immutable entity/component view based on the requested type.
			Value and const types are considered immutable. Anything else is mutable.
			\warning If \tparam T is a component it is expected to be present. Undefined behavior otherwise.
			\tparam T Component or Entity
			\param from First valid entity index
			\param to Last valid entity index
			\return Entity or component view
			*/
			template <typename T>
			GAIA_NODISCARD decltype(auto) view_auto(uint32_t from, uint32_t to) {
				using UOriginal = typename component_type_t<T>::TypeOriginal;
				if constexpr (is_component_mut_v<UOriginal>)
					return view_mut<T>(from, to);
				else
					return view<T>(from, to);
			}

			template <typename T>
			GAIA_NODISCARD decltype(auto) view_auto() {
				return view_auto<T>(0, size());
			}

			/*!
			Returns either a mutable or immutable entity/component view based on the requested type.
			Value and const types are considered immutable. Anything else is mutable.
			Doesn't update the world version when read-write access is aquired.
			\warning If \tparam T is a component it is expected to be present. Undefined behavior otherwise.
			\tparam T Component or Entity
			\param from First valid entity index
			\param to Last valid entity index
			\return Entity or component view
			*/
			template <typename T>
			GAIA_NODISCARD decltype(auto) sview_auto(uint32_t from, uint32_t to) {
				using UOriginal = typename component_type_t<T>::TypeOriginal;
				if constexpr (is_component_mut_v<UOriginal>)
					return sview_mut<T>(from, to);
				else
					return view<T>(from, to);
			}

			template <typename T>
			GAIA_NODISCARD decltype(auto) sview_auto() {
				return sview_auto<T>(0, size());
			}

			GAIA_NODISCARD std::span<const Entity> entity_view() const {
				return {(const Entity*)m_records.pEntities, size()};
			}

			GAIA_NODISCARD std::span<const ComponentId> comp_id_view(ComponentKind compKind) const {
				return {(const ComponentId*)m_records.pComponentIds[compKind], m_header.componentCount[compKind]};
			}

			GAIA_NODISCARD std::span<const ComponentRecord> comp_rec_view(ComponentKind compKind) const {
				return {m_records.pRecords[compKind], m_header.componentCount[compKind]};
			}

			GAIA_NODISCARD uint8_t* comp_ptr_mut(ComponentKind compKind, uint32_t compIdx) {
				const auto& rec = m_records.pRecords[compKind][compIdx];
				return rec.pData;
			}

			GAIA_NODISCARD uint8_t* comp_ptr_mut(ComponentKind compKind, uint32_t compIdx, uint32_t offset) {
				const auto& rec = m_records.pRecords[compKind][compIdx];
				return rec.pData + (uintptr_t)rec.comp.size() * offset;
			}

			GAIA_NODISCARD const uint8_t* comp_ptr(ComponentKind compKind, uint32_t compIdx) const {
				const auto& rec = m_records.pRecords[compKind][compIdx];
				return rec.pData;
			}

			GAIA_NODISCARD const uint8_t* comp_ptr(ComponentKind compKind, uint32_t compIdx, uint32_t offset) const {
				const auto& rec = m_records.pRecords[compKind][compIdx];
				return rec.pData + (uintptr_t)rec.comp.size() * offset;
			}

			/*!
			Make \param entity a part of the chunk at the version of the world
			\return Index of the entity within the chunk.
			*/
			GAIA_NODISCARD uint32_t add_entity(Entity entity) {
				const auto index = m_header.count++;
				++m_header.countEnabled;
				entity_view_mut()[index] = entity;

				update_version(m_header.worldVersion);
				update_world_version(ComponentKind::CK_Gen);
				update_world_version(ComponentKind::CK_Uni);

				return index;
			}

			/*!
			Copies all data associated with \param oldEntity into \param newEntity.
			*/
			static void copy_entity_data(Entity oldEntity, Entity newEntity, std::span<EntityContainer> entities) {
				GAIA_PROF_SCOPE(copy_entity_data);

				auto& oldEntityContainer = entities[oldEntity.id()];
				auto* pOldChunk = oldEntityContainer.pChunk;

				auto& newEntityContainer = entities[newEntity.id()];
				auto* pNewChunk = newEntityContainer.pChunk;

				GAIA_ASSERT(oldEntityContainer.pArchetype == newEntityContainer.pArchetype);

				auto oldRecs = pOldChunk->comp_rec_view(ComponentKind::CK_Gen);

				// Copy generic component data from reference entity to our new entity
				GAIA_EACH(oldRecs) {
					const auto& rec = oldRecs[i];
					if (rec.comp.size() == 0U)
						continue;

					auto* pSrc = (void*)pOldChunk->comp_ptr_mut(ComponentKind::CK_Gen, i, oldEntityContainer.idx);
					auto* pDst = (void*)pNewChunk->comp_ptr_mut(ComponentKind::CK_Gen, i, newEntityContainer.idx);
					rec.pDesc->copy(pSrc, pDst);
				}
			}

			/*!
			Moves all data associated with \param entity into the chunk so that it is stored at \param newEntityIdx.
			*/
			void move_entity_data(Entity entity, uint32_t newEntityIdx, std::span<EntityContainer> entities) {
				GAIA_PROF_SCOPE(CopyEntityFrom);

				auto& oldEntityContainer = entities[entity.id()];
				auto* pOldChunk = oldEntityContainer.pChunk;

				auto oldRecs = pOldChunk->comp_rec_view(ComponentKind::CK_Gen);

				// Copy generic component data from reference entity to our new entity
				GAIA_EACH(oldRecs) {
					const auto& rec = oldRecs[i];
					if (rec.comp.size() == 0U)
						continue;

					auto* pSrc = (void*)pOldChunk->comp_ptr_mut(ComponentKind::CK_Gen, i, oldEntityContainer.idx);
					auto* pDst = (void*)comp_ptr_mut(ComponentKind::CK_Gen, i, newEntityIdx);
					rec.pDesc->ctor_from(pSrc, pDst);
				}
			}

			/*!
			Moves all data associated with \param entity into the chunk so that it is stored at index \param newEntityIdx.
			*/
			void move_foreign_entity_data(Entity entity, uint32_t newEntityIdx, std::span<EntityContainer> entities) {
				GAIA_PROF_SCOPE(move_foreign_entity_data);

				auto& oldEntityContainer = entities[entity.id()];
				auto* pOldChunk = oldEntityContainer.pChunk;

				// Find intersection of the two component lists.
				// We ignore unique components here because they are not influenced by entities moving around.
				auto oldIds = pOldChunk->comp_id_view(ComponentKind::CK_Gen);
				auto newIds = comp_id_view(ComponentKind::CK_Gen);

				// Arrays are sorted so we can do linear intersection lookup
				{
					uint32_t i = 0;
					uint32_t j = 0;
					while (i < oldIds.size() && j < newIds.size()) {
						const auto oldId = oldIds[i];
						const auto newId = newIds[j];

						if (oldId == newId) {
							auto recs = comp_rec_view(ComponentKind::CK_Gen);
							const auto& rec = recs[j];
							GAIA_ASSERT(rec.comp.id() == oldId);
							if (rec.comp.size() != 0U) {
								auto* pSrc = (void*)pOldChunk->comp_ptr_mut(ComponentKind::CK_Gen, i, oldEntityContainer.idx);
								auto* pDst = (void*)comp_ptr_mut(ComponentKind::CK_Gen, j, newEntityIdx);
								rec.pDesc->ctor_from(pSrc, pDst);
							}

							++i;
							++j;
						} else if (SortComponentIdCond{}.operator()(oldId, newId))
							++i;
						else
							++j;
					}
				}
			}

			/*!
			Tries to remove the entity at index \param index.
			Removal is done via swapping with last entity in chunk.
			Upon removal, all associated data is also removed.
			If the entity at the given index already is the last chunk entity, it is removed directly.
			*/
			void remove_entity_inter(uint32_t index, std::span<EntityContainer> entities) {
				GAIA_PROF_SCOPE(remove_entity_inter);

				const auto left = index;
				const auto right = (uint32_t)m_header.count - 1;
				// The "left" entity is the one we are going to destroy so it needs to preceed the "right"
				GAIA_ASSERT(left <= right);

				// There must be at least 2 entities inside to swap
				if GAIA_LIKELY (left < right) {
					GAIA_ASSERT(m_header.count > 1);

					const auto entity = entity_view()[right];
					auto& entityContainer = entities[entity.id()];
					const auto wasDisabled = entityContainer.dis;

					// Update entity index inside chunk
					entity_view_mut()[left] = entity;

					auto recs = comp_rec_view(ComponentKind::CK_Gen);

					GAIA_EACH(recs) {
						const auto& rec = recs[i];
						if (rec.comp.size() == 0U)
							continue;

						auto* pSrc = (void*)comp_ptr_mut(ComponentKind::CK_Gen, i, left);
						auto* pDst = (void*)comp_ptr_mut(ComponentKind::CK_Gen, i, right);
						rec.pDesc->move(pSrc, pDst);
						rec.pDesc->dtor(pSrc);
					}

					// Entity has been replaced with the last one in our chunk.
					// Update its container record.
					entityContainer.idx = left;
					entityContainer.gen = entity.gen();
					entityContainer.dis = wasDisabled;
				} else {
					auto recs = comp_rec_view(ComponentKind::CK_Gen);

					GAIA_EACH(recs) {
						const auto& rec = recs[i];
						if (rec.comp.size() == 0U)
							continue;

						auto* pSrc = (void*)comp_ptr_mut(ComponentKind::CK_Gen, i, left);
						rec.pDesc->dtor(pSrc);
					}
				}
			}

			/*!
			Tries to remove the entity at index \param index.
			Removal is done via swapping with last entity in chunk.
			Upon removal, all associated data is also removed.
			If the entity at the given index already is the last chunk entity, it is removed directly.
			*/
			void remove_entity(uint32_t index, std::span<EntityContainer> entities, cnt::darray<Chunk*>& chunksToRemove) {
				GAIA_ASSERT(
						!locked() && "Entities can't be removed while their chunk is being iterated "
												 "(structural changes are forbidden during this time!)");

				const auto chunkEntityCount = size();
				if GAIA_UNLIKELY (chunkEntityCount == 0)
					return;

				GAIA_PROF_SCOPE(remove_entity);

				if (enabled(index)) {
					// Entity was previously enabled. Swap with the last entity
					remove_entity_inter(index, entities);
					// If this was the first enabled entity make sure to update the index
					if (m_header.firstEnabledEntityIndex > 0 && index == m_header.firstEnabledEntityIndex)
						--m_header.firstEnabledEntityIndex;
				} else {
					// Entity was previously disabled. Swap with the last disabled entity
					const auto pivot = size_disabled() - 1;
					swap_chunk_entities(index, pivot, entities);
					// Once swapped, try to swap with the last (enabled) entity in the chunk.
					remove_entity_inter(pivot, entities);
					--m_header.firstEnabledEntityIndex;
				}

				// At this point the last entity is no longer valid so remove it
				remove_last_entity(chunksToRemove);
			}

			/*!
			Tries to swap the entity at index \param left with the one at the index \param right.
			When swapping, all data associated with the two entities is swapped as well.
			If \param left equals \param right no swapping is performed.
			\warning "Left" must he smaller or equal to "right"
			*/
			void swap_chunk_entities(uint32_t left, uint32_t right, std::span<EntityContainer> entities) {
				// The "left" entity is the one we are going to destroy so it needs to preceed the "right".
				// Unlike remove_entity_inter, it is not technically necessary but we do it
				// anyway for the sake of consistency.
				GAIA_ASSERT(left <= right);

				// If there are at least two entities inside to swap
				if GAIA_UNLIKELY (m_header.count <= 1)
					return;
				if (left == right)
					return;

				GAIA_PROF_SCOPE(SwapEntitiesInsideChunk);

				// Update entity indices inside chunk
				const auto entityLeft = entity_view()[left];
				const auto entityRight = entity_view()[right];
				entity_view_mut()[left] = entityRight;
				entity_view_mut()[right] = entityLeft;

				auto recs = comp_rec_view(ComponentKind::CK_Gen);
				GAIA_EACH(recs) {
					const auto& rec = recs[i];
					if (rec.comp.size() == 0U)
						continue;

					auto* pSrc = (void*)comp_ptr_mut(ComponentKind::CK_Gen, i, left);
					auto* pDst = (void*)comp_ptr_mut(ComponentKind::CK_Gen, i, right);
					rec.pDesc->swap(pSrc, pDst);
				}

				// Entities were swapped. Update their entity container records.
				auto& ecLeft = entities[entityLeft.id()];
				bool ecLeftWasDisabled = ecLeft.dis;
				auto& ecRight = entities[entityRight.id()];
				ecLeft.idx = right;
				ecLeft.gen = entityRight.gen();
				ecLeft.dis = ecRight.dis;
				ecRight.idx = left;
				ecRight.gen = entityLeft.gen();
				ecRight.dis = ecLeftWasDisabled;
			}

			/*!
			Enables or disables the entity on a given index in the chunk.
			\param index Index of the entity
			\param enableEntity Enables or disabled the entity
			*/
			void enable_entity(uint32_t index, bool enableEntity, std::span<EntityContainer> entities) {
				GAIA_ASSERT(
						!locked() && "Entities can't be enable while their chunk is being iterated "
												 "(structural changes are forbidden during this time!)");
				GAIA_ASSERT(index < m_header.count && "Entity chunk index out of bounds!");

				if (enableEntity) {
					// Nothing to enable if there are no disabled entities
					if (!m_header.has_disabled_entities())
						return;
					// Trying to enable an already enabled entity
					if (enabled(index))
						return;
					// Try swapping our entity with the last disabled one
					swap_chunk_entities(--m_header.firstEnabledEntityIndex, index, entities);
					entities[entity_view()[index].id()].dis = 0;
					++m_header.countEnabled;
				} else {
					// Nothing to disable if there are no enabled entities
					if (!m_header.has_enabled_entities())
						return;
					// Trying to disable an already disabled entity
					if (!enabled(index))
						return;
					// Try swapping our entity with the last one in our chunk
					swap_chunk_entities(m_header.firstEnabledEntityIndex++, index, entities);
					entities[entity_view()[index].id()].dis = 1;
					--m_header.countEnabled;
				}
			}

			/*!
			Checks if the entity is enabled.
			\param index Index of the entity
			\return True if entity is enabled. False otherwise.
			*/
			bool enabled(uint32_t index) const {
				GAIA_ASSERT(m_header.count > 0);

				return index >= (uint32_t)m_header.firstEnabledEntityIndex;
			}

			/*!
			Returns a mutable pointer to chunk data.
			\param offset Offset into chunk data
			\return Pointer to chunk data.
			*/
			uint8_t& data(uint32_t offset) {
				return m_data[offset];
			}

			/*!
			Returns an immutable pointer to chunk data.
			\param offset Offset into chunk data
			\return Pointer to chunk data.
			*/
			const uint8_t& data(uint32_t offset) const {
				return m_data[offset];
			}

			//----------------------------------------------------------------------
			// Component handling
			//----------------------------------------------------------------------

			bool has_custom_gen_ctor() const {
				return m_header.hasAnyCustomGenCtor;
			}

			bool has_custom_uni_ctor() const {
				return m_header.hasAnyCustomUniCtor;
			}

			bool has_custom_gen_dtor() const {
				return m_header.hasAnyCustomGenDtor;
			}

			bool has_custom_uni_dtor() const {
				return m_header.hasAnyCustomUniDtor;
			}

			void call_ctor(ComponentKind compKind, uint32_t entIdx, const ComponentDesc& desc) {
				GAIA_PROF_SCOPE(call_ctor);

				// Make sure only generic components are used with this function.
				// Unique components are automatically constructed with chunks.
				GAIA_ASSERT(compKind == ComponentKind::CK_Gen);

				if (desc.func_ctor == nullptr)
					return;

				const auto compIdx = comp_idx(compKind, desc.comp.id());
				auto* pSrc = (void*)comp_ptr_mut(compKind, compIdx, entIdx);
				desc.func_ctor(pSrc, 1);
			}

			void call_ctors(ComponentKind compKind, uint32_t entIdx, uint32_t entCnt) {
				GAIA_PROF_SCOPE(call_ctors);

				GAIA_ASSERT(
						compKind == ComponentKind::CK_Gen && has_custom_gen_ctor() ||
						compKind == ComponentKind::CK_Uni && has_custom_uni_ctor());

				// Make sure only generic types are used with indices
				GAIA_ASSERT(compKind == ComponentKind::CK_Gen || (entIdx == 0 && entCnt == 1));

				auto recs = comp_rec_view(compKind);
				GAIA_EACH(recs) {
					const auto* pDesc = recs[i].pDesc;
					if (pDesc->func_ctor == nullptr)
						continue;

					auto* pSrc = (void*)comp_ptr_mut(compKind, i, entIdx);
					pDesc->func_ctor(pSrc, entCnt);
				}
			}

			void call_dtors(ComponentKind compKind, uint32_t entIdx, uint32_t entCnt) {
				GAIA_PROF_SCOPE(call_dtors);

				GAIA_ASSERT(
						compKind == ComponentKind::CK_Gen && has_custom_gen_dtor() ||
						compKind == ComponentKind::CK_Uni && has_custom_uni_dtor());

				// Make sure only generic types are used with indices
				GAIA_ASSERT(compKind == ComponentKind::CK_Gen || (entIdx == 0 && entCnt == 1));

				auto recs = comp_rec_view(compKind);
				GAIA_EACH(recs) {
					const auto* pDesc = recs[i].pDesc;
					if (pDesc->func_dtor == nullptr)
						continue;

					auto* pSrc = (void*)comp_ptr_mut(compKind, i, entIdx);
					pDesc->func_dtor(pSrc, entCnt);
				}
			};

			//----------------------------------------------------------------------
			// Check component presence
			//----------------------------------------------------------------------

			/*!
			Checks if a component with \param compId and type \param compKind is present in the chunk.
			\param compId Component id
			\param compKind Component type
			\return True if found. False otherwise.
			*/
			GAIA_NODISCARD bool has(ComponentKind compKind, ComponentId compId) const {
#if GAIA_COMP_ID_PROBING
				const ComponentId* pSrc = comp_id_mapping_ptr(compKind);
				return ecs::has_comp_idx({pSrc, m_header.componentCount[compKind]}, compId);
#else
				auto compIds = comp_id_view(compKind);
				return core::has(compIds, compId);
#endif
			}

			/*!
			Checks if component \tparam T is present in the chunk.
			\tparam T Component
			\return True if the component is present. False otherwise.
			*/
			template <typename T>
			GAIA_NODISCARD bool has() const {
				const auto compId = comp_id<T>();

				constexpr auto compKind = component_kind_v<T>;
				return has(compKind, compId);
			}

			//----------------------------------------------------------------------
			// Set component data
			//----------------------------------------------------------------------

			/*!
			Sets the value of the unique component \tparam T on \param index in the chunk.
			\warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			\tparam T Component
			\param index Index of entity in the chunk
			\param value Value to set for the component
			*/
			template <typename T, typename U = typename component_type_t<T>::Type>
			U& set(uint32_t index) {
				static_assert(
						component_kind_v<T> == ComponentKind::CK_Gen,
						"Set providing an index can only be used with generic components");

				// Update the world version
				update_version(m_header.worldVersion);

				GAIA_ASSERT(index < m_header.capacity);
				return view_mut<T>()[index];
			}

			/*!
			Sets the value of the unique component \tparam T on \param index in the chunk.
			\warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			\tparam T Component
			\param index Index of entity in the chunk
			\param value Value to set for the component
			*/
			template <typename T, typename U = typename component_type_t<T>::Type>
			U& set() {
				// Update the world version
				update_version(m_header.worldVersion);

				GAIA_ASSERT(0 < m_header.capacity);
				return view_mut<T>()[0];
			}

			/*!
			Sets the value of a generic component \tparam T on \param index in the chunk.
			\warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			\tparam T Component
			\param index Index of entity in the chunk
			\param value New component value
			*/
			template <typename T, typename U = typename component_type_t<T>::Type>
			void set(uint32_t index, U&& value) {
				static_assert(
						component_kind_v<T> == ComponentKind::CK_Gen,
						"Set providing an index can only be used with generic components");

				// Update the world version
				update_version(m_header.worldVersion);

				GAIA_ASSERT(index < m_header.capacity);
				view_mut<T>()[index] = GAIA_FWD(value);
			}

			/*!
			Sets the value of a unique component \tparam T in the chunk.
			\warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			\tparam T Component
			\param value New component value
			*/
			template <typename T, typename U = typename component_type_t<T>::Type>
			void set(U&& value) {
				static_assert(
						component_kind_v<T> != ComponentKind::CK_Gen,
						"Set not providing an index can only be used with non-generic components");

				// Update the world version
				update_version(m_header.worldVersion);

				GAIA_ASSERT(0 < m_header.capacity);
				view_mut<T>()[0] = GAIA_FWD(value);
			}

			/*!
			Sets the value of a generic component \tparam T on \param index in the chunk.
			\warning World version is not updated so Query filters will not be able to catch this change.
			\warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			\tparam T Component
			\param index Index of entity in the chunk
			\param value New component value
			*/
			template <typename T, typename U = typename component_type_t<T>::Type>
			void sset(uint32_t index, U&& value) {
				static_assert(
						component_kind_v<T> == ComponentKind::CK_Gen,
						"SetSilent providing an index can only be used with generic components");

				GAIA_ASSERT(index < m_header.capacity);
				sview_mut<T>()[index] = GAIA_FWD(value);
			}

			/*!
			Sets the value of a unique component \tparam T in the chunk.
			\warning World version is not updated so Query filters will not be able to catch this change.
			\warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			\tparam T Component
			\param value Newcomponent value
			*/
			template <typename T, typename U = typename component_type_t<T>::Type>
			void sset(U&& value) {
				static_assert(
						component_kind_v<T> != ComponentKind::CK_Gen,
						"SetSilent not providing an index can only be used with non-generic components");

				GAIA_ASSERT(0 < m_header.capacity);
				sview_mut<T>()[0] = GAIA_FWD(value);
			}

			//----------------------------------------------------------------------
			// Read component data
			//----------------------------------------------------------------------

			/*!
			Returns the value stored in the generic component \tparam T on \param index in the chunk.
			\warning It is expected the \param index is valid. Undefined behavior otherwise.
			\warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			\tparam T Component
			\param index Index of entity in the chunk
			\return Value stored in the component.
			*/
			template <typename T>
			GAIA_NODISCARD decltype(auto) get(uint32_t index) const {
				static_assert(
						component_kind_v<T> == ComponentKind::CK_Gen,
						"Get providing an index can only be used with generic components");

				return comp_inter<T>(index);
			}

			/*!
			Returns the value stored in the unique component \tparam T.
			\warning It is expected the unique component \tparam T is present. Undefined behavior otherwise.
			\tparam T Component
			\return Value stored in the component.
			*/
			template <typename T>
			GAIA_NODISCARD decltype(auto) get() const {
				static_assert(
						component_kind_v<T> != ComponentKind::CK_Gen,
						"Get not providing an index can only be used with non-generic components");

				return comp_inter<T>(0);
			}

			/*!
			Returns the internal index of a component based on the provided \param compId.
			\param compKind Component type
			\return Component index if the component was found. -1 otherwise.
			*/
			GAIA_NODISCARD uint32_t comp_idx(ComponentKind compKind, ComponentId compId) const {
#if GAIA_COMP_ID_PROBING
				const ComponentId* pSrc = comp_id_mapping_ptr(compKind);
				GAIA_ASSERT(ecs::has_comp_idx({pSrc, m_header.componentCount[compKind]}, compId));
				return ecs::get_comp_idx({pSrc, m_header.componentCount[compKind]}, compId);
#else
				return ecs::comp_idx<MAX_COMPONENTS>(m_records.pComponentIds[compKind], compId);
#endif
			}

			//----------------------------------------------------------------------

			//! Sets the index of this chunk in its archetype's storage
			void set_idx(uint32_t value) {
				m_header.index = value;
			}

			//! Returns the index of this chunk in its archetype's storage
			GAIA_NODISCARD uint32_t idx() const {
				return m_header.index;
			}

			//! Checks is this chunk has any enabled entities
			GAIA_NODISCARD bool has_enabled_entities() const {
				return m_header.has_enabled_entities();
			}

			//! Checks is this chunk has any disabled entities
			GAIA_NODISCARD bool has_disabled_entities() const {
				return m_header.has_disabled_entities();
			}

			//! Checks is this chunk is dying
			GAIA_NODISCARD bool dying() const {
				return m_header.lifespanCountdown > 0;
			}

			//! Marks the chunk as dead
			void die() {
				m_header.dead = 1;
			}

			//! Checks is this chunk is dying
			GAIA_NODISCARD bool dead() const {
				return m_header.dead == 1;
			}

			//! Starts the process of dying
			void start_dying() {
				GAIA_ASSERT(!dead());
				m_header.lifespanCountdown = ChunkHeader::MAX_CHUNK_LIFESPAN;
			}

			//! Makes the chunk alive again
			void revive() {
				GAIA_ASSERT(!dead());
				m_header.lifespanCountdown = 0;
			}

			//! Updates internal lifetime
			//! \return True if there is some lifespan left, false otherwise.
			bool progress_death() {
				GAIA_ASSERT(dying());
				--m_header.lifespanCountdown;
				return dying();
			}

			//! If true locks the chunk for structural changed.
			//! While locked, no new entities or component can be added or removed.
			//! While locked, no entities can be enabled or disabled.
			void lock(bool value) {
				if (value) {
					GAIA_ASSERT(m_header.structuralChangesLocked < ChunkHeader::MAX_CHUNK_LOCKS);
					++m_header.structuralChangesLocked;
				} else {
					GAIA_ASSERT(m_header.structuralChangesLocked > 0);
					--m_header.structuralChangesLocked;
				}
			}

			//! Checks if the chunk is locked for structural changes.
			bool locked() const {
				return m_header.structuralChangesLocked != 0;
			}

			//! Checks is the full capacity of the has has been reached
			GAIA_NODISCARD bool full() const {
				return m_header.count >= m_header.capacity;
			}

			//! Checks is the chunk is semi-full.
			GAIA_NODISCARD bool is_semi() const {
				constexpr float Threshold = 0.7f;
				return ((float)m_header.count / (float)m_header.capacity) < Threshold;
			}

			//! Returns the total number of entities in the chunk (both enabled and disabled)
			GAIA_NODISCARD uint32_t size() const {
				return m_header.count;
			}

			//! Checks is there are any entities in the chunk
			GAIA_NODISCARD bool empty() const {
				return size() == 0;
			}

			//! Return the number of entities in the chunk which are enabled
			GAIA_NODISCARD uint32_t size_enabled() const {
				return m_header.countEnabled;
			}

			//! Return the number of entities in the chunk which are enabled
			GAIA_NODISCARD uint32_t size_disabled() const {
				return m_header.firstEnabledEntityIndex;
			}

			//! Returns the number of entities in the chunk
			GAIA_NODISCARD uint32_t capacity() const {
				return m_header.capacity;
			}

			//! Returns the number of bytes the chunk spans over
			GAIA_NODISCARD uint32_t bytes() const {
				return detail::ChunkAllocatorImpl::mem_block_size(m_header.sizeType);
			}

			//! Returns true if the provided version is newer than the one stored internally
			GAIA_NODISCARD bool changed(ComponentKind compKind, uint32_t version, uint32_t compIdx) const {
				auto versions = comp_version_view(compKind);
				return version_changed(versions[compIdx], version);
			}

			//! Update version of a component at the index \param compIdx of a given \param compKind
			GAIA_FORCEINLINE void update_world_version(ComponentKind compKind, uint32_t compIdx) {
				auto versions = comp_version_view_mut(compKind);
				versions[compIdx] = m_header.worldVersion;
			}

			//! Update version of all components of a given \param compKind
			GAIA_FORCEINLINE void update_world_version(ComponentKind compKind) {
				auto versions = comp_version_view_mut(compKind);
				for (auto& v: versions)
					v = m_header.worldVersion;
			}

			void diag(uint32_t index) const {
				GAIA_LOG_N(
						"  Chunk #%04u, entities:%u/%u, lifespanCountdown:%u", index, m_header.count, m_header.capacity,
						m_header.lifespanCountdown);
			}
		};
	} // namespace ecs
} // namespace gaia
