#pragma once
#include "../config/config.h"

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
#include "entity_container.h"
#include "id.h"

namespace gaia {
	namespace ecs {
		class Chunk final {
		public:
			static constexpr uint32_t MAX_COMPONENTS_BITS = 5U;
			//! Maximum number of components on archetype
			static constexpr uint32_t MAX_COMPONENTS = 1U << MAX_COMPONENTS_BITS;

			using EntityArray = cnt::sarray_ext<Entity, MAX_COMPONENTS>;
			using ComponentArray = cnt::sarray_ext<Component, MAX_COMPONENTS>;
			using ComponentOffsetArray = cnt::sarray_ext<ChunkDataOffset, MAX_COMPONENTS>;

			// TODO: Make this private
			//! Chunk header
			ChunkHeader m_header;
			//! Pointers to various parts of data inside chunk
			ChunkRecords m_records;

		private:
			//! Pointer to where the chunk data starts.
			//! Data layed out as following:
			//!			1) ComponentVersions
			//!     2) EntityIds
			//!     3) ComponentIds
			//!			4) ComponentRecords
			//!			5) Entities (identifiers)
			//!			6) Entities (data)
			//! Note, root archetypes store only entites, therefore it is fully occupied with entities.
			uint8_t m_data[1];

			GAIA_MSVC_WARNING_PUSH()
			GAIA_MSVC_WARNING_DISABLE(26495)

			// Hidden default constructor. Only use to calculate the relative offset of m_data
			Chunk() = default;

			Chunk(
					const ComponentCache& cc, uint32_t chunkIndex, uint16_t capacity, uint8_t genEntities, uint16_t st,
					uint32_t& worldVersion):
					m_header(cc, chunkIndex, capacity, genEntities, st, worldVersion) {
				// Chunk data area consist of memory offsets, entities and component data. Normally. we would need
				// to in-place construct all of it manually.
				// However, the memory offsets and entities are all trivial types and components are initialized via
				// their constructors on-demand (if not trivial) so we do not really need to do any construction here.
			}

			GAIA_MSVC_WARNING_POP()

			GAIA_CLANG_WARNING_PUSH()
			// Memory is aligned so we can silence this warning
			GAIA_CLANG_WARNING_DISABLE("-Wcast-align")

			void init(
					const EntityArray& ids, const ComponentArray& comps, const ChunkDataOffsets& headerOffsets,
					const ComponentOffsetArray& compOffs) {
				m_header.componentCount = (uint8_t)ids.size();

				// Cache pointers to versions
				if (!ids.empty()) {
					m_records.pVersions = (ComponentVersion*)&data(headerOffsets.firstByte_Versions);
				}

				// Cache entity ids
				if (!ids.empty()) {
					auto* dst = m_records.pCompEntities = (Entity*)&data(headerOffsets.firstByte_CompEntities);

					// We treat the entity array as if were MAX_COMPONENTS long.
					// Real size can be smaller.
					uint32_t j = 0;
					for (; j < ids.size(); ++j)
						dst[j] = ids[j];
					for (; j < MAX_COMPONENTS; ++j)
						dst[j] = EntityBad;
				}

				// Cache component records
				if (!ids.empty()) {
					auto* dst = m_records.pRecords = (ComponentRecord*)&data(headerOffsets.firstByte_Records);
					GAIA_EACH_(comps, j) {
						dst[j].entity = ids[j];
						dst[j].comp = comps[j];
						dst[j].pData = &data(compOffs[j]);
						dst[j].pDesc = m_header.cc->find(comps[j].id());
					}
				}

				m_records.pEntities = (Entity*)&data(headerOffsets.firstByte_EntityData);

				// Now that records are set, we use the cached component descriptors to set ctor/dtor masks.
				{
					auto recs = comp_rec_view();
					for (const auto& rec: recs) {
						if (rec.comp.size() == 0)
							continue;

						if (rec.entity.kind() == EntityKind::EK_Gen) {
							m_header.hasAnyCustomGenCtor |= (rec.pDesc->func_ctor != nullptr);
							m_header.hasAnyCustomGenDtor |= (rec.pDesc->func_dtor != nullptr);
						} else {
							m_header.hasAnyCustomUniCtor |= (rec.pDesc->func_ctor != nullptr);
							m_header.hasAnyCustomUniDtor |= (rec.pDesc->func_dtor != nullptr);

							// We construct unique components rowB away if possible
							call_ctor(0, *rec.pDesc);
						}
					}
				}
			}

			GAIA_MSVC_WARNING_POP()

			GAIA_NODISCARD std::span<const ComponentVersion> comp_version_view() const {
				return {(const ComponentVersion*)m_records.pVersions, m_header.componentCount};
			}

			GAIA_NODISCARD std::span<ComponentVersion> comp_version_view_mut() {
				return {m_records.pVersions, m_header.componentCount};
			}

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
				GAIA_ASSERT(to <= size());

				if constexpr (std::is_same_v<core::raw_t<T>, Entity>) {
					return {(const uint8_t*)&m_records.pEntities[from], to - from};
				} else if constexpr (is_pair<T>::value) {
					using TT = typename T::type;
					using U = typename component_type_t<TT>::Type;
					static_assert(!std::is_empty_v<U>, "Attempting to get value of an empty component");

					constexpr auto kind = entity_kind_v<TT>;
					const auto rel = m_header.cc->get<typename T::rel>().entity;
					const auto tgt = m_header.cc->get<typename T::tgt>().entity;
					const auto compIdx = comp_idx((Entity)Pair(rel, tgt));

					if constexpr (kind == EntityKind::EK_Gen) {
						return {comp_ptr(compIdx, from), to - from};
					} else {
						// GAIA_ASSERT(count == 1); we don't really care and always consider 1 for unique components
						return {comp_ptr(compIdx), 1};
					}
				} else {
					using U = typename component_type_t<T>::Type;
					static_assert(!std::is_empty_v<U>, "Attempting to get value of an empty component");

					constexpr auto kind = entity_kind_v<T>;
					const auto comp = m_header.cc->get<T>().entity;
					GAIA_ASSERT(comp.kind() == kind);
					const auto compIdx = comp_idx(comp);

					if constexpr (kind == EntityKind::EK_Gen) {
						return {comp_ptr(compIdx, from), to - from};
					} else {
						// GAIA_ASSERT(count == 1); we don't really care and always consider 1 for unique components
						return {comp_ptr(compIdx), 1};
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
				GAIA_ASSERT(to <= size());

				static_assert(!std::is_same_v<core::raw_t<T>, Entity>, "view_mut can't be used to modify Entity");

				if constexpr (is_pair<T>::value) {
					using TT = typename T::type;
					using U = typename component_type_t<TT>::Type;
					static_assert(!std::is_empty_v<U>, "view_mut can't be used to modify tag components");

					constexpr auto kind = entity_kind_v<TT>;
					const auto rel = m_header.cc->get<typename T::rel>().entity;
					const auto tgt = m_header.cc->get<typename T::tgt>().entity;
					const auto compIdx = comp_idx((Entity)Pair(rel, tgt));

					// Update version number if necessary so we know RW access was used on the chunk
					if constexpr (WorldVersionUpdateWanted)
						update_world_version(compIdx);

					if constexpr (kind == EntityKind::EK_Gen) {
						return {comp_ptr_mut(compIdx, from), to - from};
					} else {
						// GAIA_ASSERT(count == 1); we don't really care and always consider 1 for unique components
						return {comp_ptr_mut(compIdx), 1};
					}
				} else {
					using U = typename component_type_t<T>::Type;
					static_assert(!std::is_empty_v<U>, "view_mut can't be used to modify tag components");

					constexpr auto kind = entity_kind_v<T>;
					const auto comp = m_header.cc->get<T>().entity;
					GAIA_ASSERT(comp.kind() == kind);
					const auto compIdx = comp_idx(comp);

					// Update version number if necessary so we know RW access was used on the chunk
					if constexpr (WorldVersionUpdateWanted)
						update_world_version(compIdx);

					if constexpr (kind == EntityKind::EK_Gen) {
						return {comp_ptr_mut(compIdx, from), to - from};
					} else {
						// GAIA_ASSERT(count == 1); we don't really care and always consider 1 for unique components
						return {comp_ptr_mut(compIdx), 1};
					}
				}
			}

			/*!
			Returns the value stored in the component \tparam T on \param row in the chunk.
			\warning It is expected the \param row is valid. Undefined behavior otherwise.
			\warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			\tparam T Component
			\param row Row of entity in the chunk
			\return Value stored in the component if smaller than 8 bytes. Const reference to the value otherwise.
			*/
			template <typename T>
			GAIA_NODISCARD decltype(auto) comp_inter(uint16_t row) const {
				using U = typename actual_type_t<T>::Type;
				using RetValueType = decltype(view<T>()[0]);

				GAIA_ASSERT(row < m_header.count);
				if constexpr (sizeof(RetValueType) > 8)
					return (const U&)view<T>()[row];
				else
					return view<T>()[row];
			}

			/*!
			Removes the entity at from the chunk and updates the world versions
			*/
			void remove_last_entity_inter() {
				// Should never be called over an empty chunk
				GAIA_ASSERT(!empty());

#if GAIA_ASSERT_ENABLED
				// Invalidate the entity in chunk data
				entity_view_mut()[m_header.count - 1] = EntityBad;
#endif

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
					const ComponentCache& cc, uint32_t chunkIndex, uint16_t capacity, uint8_t genEntities, uint16_t dataBytes,
					uint32_t& worldVersion,
					// data offsets
					const ChunkDataOffsets& offsets,
					// component entities
					const EntityArray& ids,
					// component
					const ComponentArray& comps,
					// component offsets
					const ComponentOffsetArray& compOffs) {
				const auto totalBytes = chunk_total_bytes(dataBytes);
				const auto sizeType = mem_block_size_type(totalBytes);
#if GAIA_ECS_CHUNK_ALLOCATOR
				auto* pChunk = (Chunk*)ChunkAllocator::get().alloc(totalBytes);
				new (pChunk) Chunk(cc, chunkIndex, capacity, genEntities, sizeType, worldVersion);
#else
				GAIA_ASSERT(totalBytes <= MaxMemoryBlockSize);
				const auto allocSize = mem_block_size(sizeType);
				auto* pChunkMem = new uint8_t[allocSize];
				auto* pChunk = new (pChunkMem) Chunk(cc, chunkIndex, capacity, genEntities, sizeType, worldVersion);
#endif

				pChunk->init(ids, comps, offsets, compOffs);
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
				if (pChunk->has_custom_gen_dtor() || pChunk->has_custom_uni_dtor())
					pChunk->call_all_dtors();

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
			Remove the last entity from a chunk.
			If as a result the chunk becomes empty it is scheduled for deletion.
			\param chunksToDelete Container of chunks ready for deletion
			*/
			void remove_last_entity(cnt::darray<Chunk*>& chunksToDelete) {
				remove_last_entity_inter();

				// TODO: This needs cleaning up.
				//       Chunk should have no idea of the world and also should not store
				//       any states realted to its lifetime.
				if (!dying() && empty()) {
					// When the chunk is emptied we want it to be removed. We can't do it
					// rowB away and need to wait for world::gc() to be called.
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

					chunksToDelete.push_back(this);
				}
			}

			//! Updates the version numbers for this chunk.
			void update_versions() {
				update_version(m_header.worldVersion);
				update_world_version();
			}

			/*!
			Returns a read-only entity or component view.
			\warning If \tparam T is a component it is expected it is present. Undefined behavior otherwise.
			\tparam T Component or Entity
			\param from First valid entity row
			\param to Last valid entity row
			\return Entity of component view with read-only access
			*/
			template <typename T>
			GAIA_NODISCARD decltype(auto) view(uint16_t from, uint16_t to) const {
				using U = typename actual_type_t<T>::Type;
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
			\param from First valid entity row
			\param to Last valid entity row
			\return Entity or component view with read-write access
			*/
			template <typename T>
			GAIA_NODISCARD decltype(auto) view_mut(uint16_t from, uint16_t to) {
				using U = typename actual_type_t<T>::Type;
				static_assert(!std::is_same_v<U, Entity>, "Modifying chunk entities via view_mut is forbidden");

				return mem::auto_view_policy_set<U>{view_mut_inter<T, true>(from, to)};
			}

			template <typename T>
			GAIA_NODISCARD decltype(auto) view_mut() {
				using TT = core::raw_t<T>;
				return view_mut<TT>(0, size());
			}

			/*!
			Returns a mutable component view.
			Doesn't update the world version when the access is aquired.
			\warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			\tparam T Component
			\param from First valid entity row
			\param to Last valid entity row
			\return Component view with read-write access
			*/
			template <typename T>
			GAIA_NODISCARD decltype(auto) sview_mut(uint16_t from, uint16_t to) {
				using U = typename actual_type_t<T>::Type;
				static_assert(!std::is_same_v<U, Entity>, "Modifying chunk entities via view_mut is forbidden");

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
			\param from First valid entity row
			\param to Last valid entity row
			\return Entity or component view
			*/
			template <typename T>
			GAIA_NODISCARD decltype(auto) view_auto(uint16_t from, uint16_t to) {
				using UOriginal = typename actual_type_t<T>::TypeOriginal;
				if constexpr (core::is_mut_v<UOriginal>)
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
			\param from First valid entity row
			\param to Last valid entity row
			\return Entity or component view
			*/
			template <typename T>
			GAIA_NODISCARD decltype(auto) sview_auto(uint16_t from, uint16_t to) {
				using UOriginal = typename actual_type_t<T>::TypeOriginal;
				if constexpr (core::is_mut_v<UOriginal>)
					return sview_mut<T>(from, to);
				else
					return view<T>(from, to);
			}

			template <typename T>
			GAIA_NODISCARD decltype(auto) sview_auto() {
				return sview_auto<T>(0, size());
			}

			GAIA_NODISCARD EntitySpan entity_view() const {
				return {(const Entity*)m_records.pEntities, size()};
			}

			GAIA_NODISCARD EntitySpan ents_id_view() const {
				return {(const Entity*)m_records.pCompEntities, m_header.componentCount};
			}

			GAIA_NODISCARD std::span<const ComponentRecord> comp_rec_view() const {
				return {m_records.pRecords, m_header.componentCount};
			}

			GAIA_NODISCARD uint8_t* comp_ptr_mut(uint32_t compIdx) {
				const auto& rec = m_records.pRecords[compIdx];
				return rec.pData;
			}

			GAIA_NODISCARD uint8_t* comp_ptr_mut(uint32_t compIdx, uint32_t offset) {
				const auto& rec = m_records.pRecords[compIdx];
				return rec.pData + (uintptr_t)rec.comp.size() * offset;
			}

			GAIA_NODISCARD const uint8_t* comp_ptr(uint32_t compIdx) const {
				const auto& rec = m_records.pRecords[compIdx];
				return rec.pData;
			}

			GAIA_NODISCARD const uint8_t* comp_ptr(uint32_t compIdx, uint32_t offset) const {
				const auto& rec = m_records.pRecords[compIdx];
				return rec.pData + (uintptr_t)rec.comp.size() * offset;
			}

			/*!
			Make \param entity a part of the chunk at the version of the world
			\return Row of entity in the chunk,
			*/
			GAIA_NODISCARD uint16_t add_entity(Entity entity) {
				const auto row = m_header.count++;

				// Zero after increase of value means an overflow!
				GAIA_ASSERT(m_header.count != 0);

				++m_header.countEnabled;
				entity_view_mut()[row] = entity;

				update_version(m_header.worldVersion);
				update_world_version();

				return row;
			}

			/*!
			Copies all data associated with \param oldEntity into \param newEntity.
			*/
			static void copy_entity_data(Entity oldEntity, Entity newEntity, EntityContainers& recs) {
				GAIA_PROF_SCOPE(Chunk::copy_entity_data);

				auto& oldEntityContainer = recs[oldEntity];
				auto* pOldChunk = oldEntityContainer.pChunk;

				auto& newEntityContainer = recs[newEntity];
				auto* pNewChunk = newEntityContainer.pChunk;

				GAIA_ASSERT(oldEntityContainer.pArchetype == newEntityContainer.pArchetype);

				auto oldRecs = pOldChunk->comp_rec_view();

				// Copy generic component data from reference entity to our new entity
				GAIA_FOR(pOldChunk->m_header.genEntities) {
					const auto& rec = oldRecs[i];
					if (rec.comp.size() == 0U)
						continue;

					auto* pSrc = (void*)pOldChunk->comp_ptr_mut(i, oldEntityContainer.row);
					auto* pDst = (void*)pNewChunk->comp_ptr_mut(i, newEntityContainer.row);
					rec.pDesc->copy(pSrc, pDst);
				}
			}

			/*!
			Moves all data associated with \param entity into the chunk so that it is stored at the row \param row.
			*/
			void move_entity_data(Entity entity, uint16_t row, EntityContainers& recs) {
				GAIA_PROF_SCOPE(Chunk::move_entity_data);

				auto& ec = recs[entity];
				auto* pOldChunk = ec.pChunk;
				auto oldRecs = pOldChunk->comp_rec_view();

				// Copy generic component data from reference entity to our new entity
				GAIA_FOR(pOldChunk->m_header.genEntities) {
					const auto& rec = oldRecs[i];
					if (rec.comp.size() == 0U)
						continue;

					auto* pSrc = (void*)pOldChunk->comp_ptr_mut(i, ec.row);
					auto* pDst = (void*)comp_ptr_mut(i, row);
					rec.pDesc->ctor_from(pSrc, pDst);
				}
			}

			static void move_foreign_entity_data(Chunk* pOldChunk, uint32_t oldRow, Chunk* pNewChunk, uint32_t newRow) {
				GAIA_PROF_SCOPE(Chunk::move_foreign_entity_data);

				GAIA_ASSERT(pOldChunk != nullptr);
				GAIA_ASSERT(pNewChunk != nullptr);
				GAIA_ASSERT(oldRow < pOldChunk->size());
				GAIA_ASSERT(newRow < pNewChunk->size());

				auto oldIds = pOldChunk->ents_id_view();
				auto newIds = pNewChunk->ents_id_view();
				auto newRecs = pNewChunk->comp_rec_view();

				// Find intersection of the two component lists.
				// Arrays are sorted so we can do linear intersection lookup.
				// Call constructor on each match.
				{
					uint32_t i = 0;
					uint32_t j = 0;
					while (i < pOldChunk->m_header.genEntities && j < pNewChunk->m_header.genEntities) {
						const auto oldId = oldIds[i];
						const auto newId = newIds[j];

						if (oldId == newId) {
							const auto& rec = newRecs[j];
							GAIA_ASSERT(rec.entity == newId);
							if (rec.comp.size() != 0U) {
								auto* pSrc = (void*)pOldChunk->comp_ptr_mut(i, oldRow);
								auto* pDst = (void*)pNewChunk->comp_ptr_mut(j, newRow);
								rec.pDesc->ctor_from(pSrc, pDst);
							}

							++i;
							++j;
						} else if (SortComponentCond{}.operator()(oldId, newId)) {
							++i;
						} else {
							// No match with the old chunk. Construct the component
							const auto& rec = newRecs[j];
							GAIA_ASSERT(rec.entity == newId);
							if (rec.pDesc != nullptr && rec.pDesc->func_ctor != nullptr) {
								auto* pDst = (void*)pNewChunk->comp_ptr_mut(j, newRow);
								rec.pDesc->func_ctor(pDst, 1);
							}

							++j;
						}
					}

					// Initialize the rest of the components if they are generic.
					for (; j < pNewChunk->m_header.genEntities; ++j) {
						const auto& rec = newRecs[j];
						if (rec.pDesc != nullptr && rec.pDesc->func_ctor != nullptr) {
							auto* pDst = (void*)pNewChunk->comp_ptr_mut(j, newRow);
							rec.pDesc->func_ctor(pDst, 1);
						}
					}
				}
			}

			/*!
			Moves all data associated with \param entity into the chunk so that it is stored at the row \param row.
			*/
			void move_foreign_entity_data(Entity entity, uint16_t row, EntityContainers& recs) {
				GAIA_PROF_SCOPE(Chunk::move_foreign_entity_data);

				auto& ec = recs[entity];
				move_foreign_entity_data(ec.pChunk, ec.row, this, row);
			}

			/*!
			Tries to remove the entity at \param row.
			Removal is done via swapping with last entity in chunk.
			Upon removal, all associated data is also removed.
			If the entity at the given row already is the last chunk entity, it is removed directly.
			*/
			void remove_entity_inter(uint16_t row, EntityContainers& recs) {
				GAIA_PROF_SCOPE(Chunk::remove_entity_inter);

				const uint16_t rowA = row;
				const uint16_t rowB = m_header.count - 1;
				// The "rowA" entity is the one we are going to destroy so it needs to preceed the "rowB"
				GAIA_ASSERT(rowA <= rowB);

				// There must be at least 2 entities inside to swap
				if GAIA_LIKELY (rowA < rowB) {
					GAIA_ASSERT(m_header.count > 1);

					auto ev = entity_view_mut();

					// Update entity data
					const auto entityB = ev[rowB];
					auto& ecB = recs[entityB];
#if GAIA_ASSERT_ENABLED
					const auto entityA = ev[rowA];
					auto& ecA = recs[entityA];

					GAIA_ASSERT(ecA.pArchetype == ecB.pArchetype);
					GAIA_ASSERT(ecA.pChunk == ecB.pChunk);
#endif

					ev[rowA] = entityB;

					// Move component data from entityB to entityA
					auto recView = comp_rec_view();
					GAIA_FOR(m_header.genEntities) {
						const auto& rec = recView[i];
						if (rec.comp.size() == 0U)
							continue;

						auto* pSrc = (void*)comp_ptr_mut(i, rowB);
						auto* pDst = (void*)comp_ptr_mut(i, rowA);
						rec.pDesc->move(pSrc, pDst);
						rec.pDesc->dtor(pSrc);
					}

					// Entity has been replaced with the last one in our chunk. Update its container record.
					ecB.row = rowA;
				} else {
					// This is the last entity in chunk so simply destroy its data
					auto recView = comp_rec_view();
					GAIA_FOR(m_header.genEntities) {
						const auto& rec = recView[i];
						if (rec.comp.size() == 0U)
							continue;

						auto* pSrc = (void*)comp_ptr_mut(i, rowA);
						rec.pDesc->dtor(pSrc);
					}
				}
			}

			/*!
			Tries to remove the entity at row \param row.
			Removal is done via swapping with last entity in chunk.
			Upon removal, all associated data is also removed.
			If the entity at the given row already is the last chunk entity, it is removed directly.
			*/
			void remove_entity(uint16_t row, EntityContainers& recs, cnt::darray<Chunk*>& chunksToDelete) {
				GAIA_ASSERT(
						!locked() && "Entities can't be removed while their chunk is being iterated "
												 "(structural changes are forbidden during this time!)");

				const auto chunkEntityCount = size();
				if GAIA_UNLIKELY (chunkEntityCount == 0)
					return;

				GAIA_PROF_SCOPE(Chunk::remove_entity);

				if (enabled(row)) {
					// Entity was previously enabled. Swap with the last entity
					remove_entity_inter(row, recs);
					// If this was the first enabled entity make sure to update the row
					if (m_header.rowFirstEnabledEntity > 0 && row == m_header.rowFirstEnabledEntity)
						--m_header.rowFirstEnabledEntity;
				} else {
					// Entity was previously disabled. Swap with the last disabled entity
					const uint16_t pivot = size_disabled() - 1;
					swap_chunk_entities(row, pivot, recs);
					// Once swapped, try to swap with the last (enabled) entity in the chunk.
					remove_entity_inter(pivot, recs);
					--m_header.rowFirstEnabledEntity;
				}

				// At this point the last entity is no longer valid so remove it
				remove_last_entity(chunksToDelete);
			}

			/*!
			Tries to swap the entity at row \param rowA with the one at the row \param rowB.
			When swapping, all data associated with the two entities is swapped as well.
			If \param rowA equals \param rowB no swapping is performed.
			\warning "rowA" must he smaller or equal to "rowB"
			*/
			void swap_chunk_entities(uint16_t rowA, uint16_t rowB, EntityContainers& recs) {
				// The "rowA" entity is the one we are going to destroy so it needs to preceed the "rowB".
				// Unlike remove_entity_inter, it is not technically necessary but we do it
				// anyway for the sake of consistency.
				GAIA_ASSERT(rowA <= rowB);

				// If there are at least two different entities inside to swap
				if GAIA_UNLIKELY (m_header.count <= 1 || rowA == rowB)
					return;

				GAIA_PROF_SCOPE(Chunk::swap_chunk_entities);

				// Update entity data
				auto ev = entity_view_mut();
				const auto entityA = ev[rowA];
				const auto entityB = ev[rowB];

				auto& ecA = recs[entityA];
				auto& ecB = recs[entityB];
				GAIA_ASSERT(ecA.pArchetype == ecB.pArchetype);
				GAIA_ASSERT(ecA.pChunk == ecB.pChunk);

				ev[rowA] = entityB;
				ev[rowB] = entityA;

				// Swap component data
				auto recView = comp_rec_view();
				GAIA_FOR2(0, m_header.genEntities) {
					const auto& rec = recView[i];
					if (rec.comp.size() == 0U)
						continue;

					auto* pSrc = (void*)comp_ptr_mut(i, rowA);
					auto* pDst = (void*)comp_ptr_mut(i, rowB);
					rec.pDesc->swap(pSrc, pDst);
				}

				// Update indices in entity container.
				ecA.row = (uint16_t)rowB;
				ecB.row = (uint16_t)rowA;
			}

			/*!
			Enables or disables the entity on a given row in the chunk.
			\param row Row of the entity within chunk
			\param enableEntity Enables or disabled the entity
			\param entities Span of entity container records
			*/
			void enable_entity(uint16_t row, bool enableEntity, EntityContainers& recs) {
				GAIA_ASSERT(
						!locked() && "Entities can't be enable while their chunk is being iterated "
												 "(structural changes are forbidden during this time!)");
				GAIA_ASSERT(row < m_header.count && "Entity chunk row out of bounds!");

				if (enableEntity) {
					// Nothing to enable if there are no disabled entities
					if (!m_header.has_disabled_entities())
						return;
					// Trying to enable an already enabled entity
					if (enabled(row))
						return;
					// Try swapping our entity with the last disabled one
					const auto entity = entity_view()[row];
					swap_chunk_entities(--m_header.rowFirstEnabledEntity, row, recs);
					recs[entity].dis = 0;
					++m_header.countEnabled;
				} else {
					// Nothing to disable if there are no enabled entities
					if (!m_header.has_enabled_entities())
						return;
					// Trying to disable an already disabled entity
					if (!enabled(row))
						return;
					// Try swapping our entity with the last one in our chunk
					const auto entity = entity_view()[row];
					swap_chunk_entities(m_header.rowFirstEnabledEntity++, row, recs);
					recs[entity].dis = 1;
					--m_header.countEnabled;
				}
			}

			/*!
			Checks if the entity is enabled.
			\param row Row of the entity within chunk
			\return True if entity is enabled. False otherwise.
			*/
			bool enabled(uint16_t row) const {
				GAIA_ASSERT(m_header.count > 0);

				return row >= (uint16_t)m_header.rowFirstEnabledEntity;
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

			void call_ctor(uint32_t entIdx, const ComponentCacheItem& desc) {
				GAIA_PROF_SCOPE(Chunk::call_ctor);

				if (desc.func_ctor == nullptr)
					return;

				const auto compIdx = comp_idx(desc.entity);
				auto* pSrc = (void*)comp_ptr_mut(compIdx, entIdx);
				desc.func_ctor(pSrc, 1);
			}

			void call_gen_ctors(uint32_t entIdx, uint32_t entCnt) {
				GAIA_PROF_SCOPE(Chunk::call_gen_ctors);

				auto recs = comp_rec_view();
				GAIA_FOR2(0, m_header.genEntities) {
					const auto& rec = recs[i];

					const auto* pDesc = rec.pDesc;
					if (pDesc == nullptr || pDesc->func_ctor == nullptr)
						continue;

					auto* pSrc = (void*)comp_ptr_mut(i, entIdx);
					pDesc->func_ctor(pSrc, entCnt);
				}
			}

			void call_all_dtors() {
				GAIA_PROF_SCOPE(Chunk::call_all_dtors);

				auto recs = comp_rec_view();
				GAIA_EACH(recs) {
					const auto& rec = recs[i];

					const auto* pDesc = rec.pDesc;
					if (pDesc == nullptr || pDesc->func_dtor == nullptr)
						continue;

					auto* pSrc = (void*)comp_ptr_mut(i, 0);
					const auto cnt = (rec.entity.kind() == EntityKind::EK_Gen) ? m_header.count : 1;
					pDesc->func_dtor(pSrc, cnt);
				}
			};

			//----------------------------------------------------------------------
			// Check component presence
			//----------------------------------------------------------------------

			/*!
			Checks if a component/entity \param entity is present in the chunk.
			\param entity Entity
			\return True if found. False otherwise.
			*/
			GAIA_NODISCARD bool has(Entity entity) const {
				auto ids = ents_id_view();
				return core::has(ids, entity);
			}

			/*!
			Checks if component \tparam T is present in the chunk.
			\tparam T Component or pair
			\return True if the component is present. False otherwise.
			*/
			template <typename T>
			GAIA_NODISCARD bool has() const {
				if constexpr (is_pair<T>::value) {
					const auto rel = m_header.cc->get<typename T::rel>().entity;
					const auto tgt = m_header.cc->get<typename T::tgt>().entity;
					return has((Entity)Pair(rel, tgt));
				} else {
					const auto* pComp = m_header.cc->find<T>();
					return pComp != nullptr && has(pComp->entity);
				}
			}

			//----------------------------------------------------------------------
			// Set component data
			//----------------------------------------------------------------------

			/*!
			Sets the value of the unique component \tparam T on \param row in the chunk.
			\tparam T Component or pair
			\param row Row of entity in the chunk
			\param value Value to set for the component
			\warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			*/
			template <typename T, typename U = typename actual_type_t<T>::Type>
			void set(uint16_t row, U&& value) {
				verify_comp<T>();

				GAIA_ASSERT2(
						actual_type_t<T>::Kind == EntityKind::EK_Gen || row == 0,
						"Set providing a row can only be used with generic components");

				// Update the world version
				update_version(m_header.worldVersion);

				GAIA_ASSERT(row < m_header.capacity);
				view_mut<T>()[row] = GAIA_FWD(value);
			}

			/*!
			Sets the value of a generic entity \param type at the position \param row in the chunk.
			\param row Row of entity in the chunk
			\param type Component/entity/pair
			\param value New component value\warning It is expected the component \tparam T is present. Undefined behavior
			otherwise.
			*/
			template <typename T>
			void set(uint16_t row, Entity type, T&& value) {
				verify_comp<T>();

				GAIA_ASSERT2(
						type.kind() == EntityKind::EK_Gen || row == 0,
						"Set providing a row can only be used with generic components");
				GAIA_ASSERT(type.kind() == entity_kind_v<T>);

				// Update the world version
				update_version(m_header.worldVersion);

				GAIA_ASSERT(row < m_header.capacity);

				// TODO: This function work but is useless because it does the same job as
				//       set(uint16_t row, U&& value).
				//       This is because T needs to match U anyway so the component lookup can succeed.
				(void)type;
				// const uint32_t col = comp_idx(type);
				//(void)col;

				view_mut<T>()[row] = GAIA_FWD(value);
			}

			/*!
			Sets the value of the unique component \tparam T on \param row in the chunk.
			\tparam T Component or pair
			\param row Row of entity in the chunk
			\param value Value to set for the component
			\warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			\warning World version is not updated so Query filters will not be able to catch this change.
			*/
			template <typename T, typename U = typename actual_type_t<T>::Type>
			void sset(uint16_t row, U&& value) {
				GAIA_ASSERT2(
						actual_type_t<T>::Kind == EntityKind::EK_Gen || row == 0,
						"Set providing a row can only be used with generic components");

				GAIA_ASSERT(row < m_header.capacity);
				view_mut<T>()[row] = GAIA_FWD(value);
			}

			/*!
			Sets the value of a generic entity \param object at the position \param row in the chunk.
			\param row Row of entity in the chunk
			\param object Component/entity/pair
			\param value New component value\warning It is expected the component \tparam T is present. Undefined behavior
			otherwise.
			\warning World version is not updated so Query filters will not be able to catch this change.
			*/
			template <typename T>
			void sset(uint16_t row, [[maybe_unused]] Entity object, T&& value) {
				static_assert(core::is_raw_v<T>);

				GAIA_ASSERT2(
						object.kind() == EntityKind::EK_Gen || row == 0,
						"Set providing a row can only be used with generic components");

				GAIA_ASSERT(row < m_header.capacity);
				view_mut<T>()[row] = GAIA_FWD(value);
			}

			//----------------------------------------------------------------------
			// Read component data
			//----------------------------------------------------------------------

			/*!
			Returns the value stored in the generic component \tparam T on \param row in the chunk.
			\warning It is expected the \param row is valid. Undefined behavior otherwise.
			\warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			\tparam T Component or pair
			\param row Row of entity in the chunk
			\return Value stored in the component.
			*/
			template <typename T>
			GAIA_NODISCARD decltype(auto) get(uint16_t row) const {
				static_assert(
						entity_kind_v<T> == EntityKind::EK_Gen, "Get providing a row can only be used with generic components");

				return comp_inter<T>(row);
			}

			/*!
			Returns the value stored in the unique component \tparam T.
			\warning It is expected the unique component \tparam T is present. Undefined behavior otherwise.
			\tparam T Component or pair
			\return Value stored in the component.
			*/
			template <typename T>
			GAIA_NODISCARD decltype(auto) get() const {
				static_assert(
						entity_kind_v<T> != EntityKind::EK_Gen,
						"Get not providing a row can only be used with non-generic components");

				return comp_inter<T>(0);
			}

			/*!
			 Returns the internal index of a component based on the provided \param entity.
			 \param entity Component
			 \return Component index if the component was found. -1 otherwise.
			 */
			GAIA_NODISCARD uint32_t comp_idx(Entity entity) const {
				return ecs::comp_idx<MAX_COMPONENTS>(m_records.pCompEntities, entity);
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
			//! \return True if there is some lifespan rowA, false otherwise.
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
			GAIA_NODISCARD uint16_t size() const {
				return m_header.count;
			}

			//! Checks is there are any entities in the chunk
			GAIA_NODISCARD bool empty() const {
				return size() == 0;
			}

			//! Return the number of entities in the chunk which are enabled
			GAIA_NODISCARD uint16_t size_enabled() const {
				return m_header.countEnabled;
			}

			//! Return the number of entities in the chunk which are enabled
			GAIA_NODISCARD uint16_t size_disabled() const {
				return (uint16_t)m_header.rowFirstEnabledEntity;
			}

			//! Returns the number of entities in the chunk
			GAIA_NODISCARD uint16_t capacity() const {
				return m_header.capacity;
			}

			//! Returns the number of bytes the chunk spans over
			GAIA_NODISCARD uint32_t bytes() const {
				return mem_block_size(m_header.sizeType);
			}

			//! Returns true if the provided version is newer than the one stored internally
			GAIA_NODISCARD bool changed(uint32_t version, uint32_t compIdx) const {
				auto versions = comp_version_view();
				return version_changed(versions[compIdx], version);
			}

			//! Update the version of a component at the index \param compIdx
			GAIA_FORCEINLINE void update_world_version(uint32_t compIdx) {
				auto versions = comp_version_view_mut();
				versions[compIdx] = m_header.worldVersion;
			}

			//! Update the version of all components
			GAIA_FORCEINLINE void update_world_version() {
				auto versions = comp_version_view_mut();
				for (auto& v: versions)
					v = m_header.worldVersion;
			}

			void diag() const {
				GAIA_LOG_N(
						"  Chunk #%04u, entities:%u/%u, lifespanCountdown:%u", m_header.index, m_header.count, m_header.capacity,
						m_header.lifespanCountdown);
			}
		};
	} // namespace ecs
} // namespace gaia
