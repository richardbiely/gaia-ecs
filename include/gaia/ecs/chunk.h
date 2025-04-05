#pragma once
#include "../config/config.h"

#include <cstdint>
#include <cstring>
#include <tuple>
#include <type_traits>
#include <utility>

#include "../cnt/sarray_ext.h"
#include "../config/profiler.h"
#include "../core/utility.h"
#include "../mem/data_layout_policy.h"
#include "../mem/mem_alloc.h"
#include "archetype_common.h"
#include "chunk_allocator.h"
#include "chunk_header.h"
#include "common.h"
#include "component.h"
#include "component_cache.h"
#include "component_desc.h"
#include "entity_container.h"
#include "id.h"

namespace gaia {
	namespace ecs {
		class Chunk final {
		public:
			using EntityArray = cnt::sarray_ext<Entity, ChunkHeader::MAX_COMPONENTS>;
			using ComponentArray = cnt::sarray_ext<Component, ChunkHeader::MAX_COMPONENTS>;
			using ComponentOffsetArray = cnt::sarray_ext<ChunkDataOffset, ChunkHeader::MAX_COMPONENTS>;

		private:
			//! Chunk header
			ChunkHeader m_header;
			//! Pointers to various parts of data inside chunk
			ChunkRecords m_records;

			//! Pointer to where the chunk data starts.
			//! Data laid out as following:
			//!			1) ComponentVersions
			//!     2) EntityIds/ComponentIds
			//!			3) ComponentRecords
			//!			4) Entities (identifiers)
			//!			5) Entities (data)
			//! Note, root archetypes store only entities, therefore it is fully occupied with entities.
			uint8_t m_data[1];

			GAIA_MSVC_WARNING_PUSH()
			GAIA_MSVC_WARNING_DISABLE(26495)

			// Hidden default constructor. Only use to calculate the relative offset of m_data
			Chunk() = default;

			Chunk(
					const World& wld, const ComponentCache& cc, //
					uint32_t chunkIndex, uint16_t capacity, uint8_t genEntities, uint16_t st, //
					uint32_t& worldVersion): //
					m_header(wld, cc, chunkIndex, capacity, genEntities, st, worldVersion) {
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
					uint32_t cntEntities, const Entity* ids, const Component* comps, const ChunkDataOffsets& headerOffsets,
					const ChunkDataOffset* compOffs) {
				m_header.cntEntities = (uint8_t)cntEntities;

				// Cache pointers to versions
				if (cntEntities > 0) {
					m_records.pVersions = (ComponentVersion*)&data(headerOffsets.firstByte_Versions);
				}

				// Cache entity ids
				if (cntEntities > 0) {
					auto* dst = m_records.pCompEntities = (Entity*)&data(headerOffsets.firstByte_CompEntities);

					// We treat the entity array as if were MAX_COMPONENTS long.
					// Real size can be smaller.
					uint32_t j = 0;
					for (; j < cntEntities; ++j)
						dst[j] = ids[j];
					for (; j < ChunkHeader::MAX_COMPONENTS; ++j)
						dst[j] = EntityBad;
				}

				// Cache component records
				if (cntEntities > 0) {
					auto* dst = m_records.pRecords = (ComponentRecord*)&data(headerOffsets.firstByte_Records);
					GAIA_FOR_(cntEntities, j) {
						dst[j].comp = comps[j];
						dst[j].pData = &data(compOffs[j]);
						dst[j].pItem = m_header.cc->find(comps[j].id());
					}
				}

				m_records.pEntities = (Entity*)&data(headerOffsets.firstByte_EntityData);

				// Now that records are set, we use the cached component descriptors to set ctor/dtor masks.
				{
					auto recs = comp_rec_view();
					const auto recs_cnt = recs.size();
					GAIA_FOR(recs_cnt) {
						const auto& rec = recs[i];
						if (rec.comp.size() == 0)
							continue;

						const auto e = m_records.pCompEntities[i];
						if (e.kind() == EntityKind::EK_Gen) {
							m_header.hasAnyCustomGenCtor |= (rec.pItem->func_ctor != nullptr);
							m_header.hasAnyCustomGenDtor |= (rec.pItem->func_dtor != nullptr);
						} else {
							m_header.hasAnyCustomUniCtor |= (rec.pItem->func_ctor != nullptr);
							m_header.hasAnyCustomUniDtor |= (rec.pItem->func_dtor != nullptr);

							// We construct unique components right away if possible
							call_ctor(0, *rec.pItem);
						}
					}
				}
			}

			GAIA_CLANG_WARNING_POP()

			GAIA_NODISCARD std::span<const ComponentVersion> comp_version_view() const {
				return {(const ComponentVersion*)m_records.pVersions, m_header.cntEntities};
			}

			GAIA_NODISCARD std::span<ComponentVersion> comp_version_view_mut() {
				return {m_records.pVersions, m_header.cntEntities};
			}

			GAIA_NODISCARD std::span<Entity> entity_view_mut() {
				return {m_records.pEntities, m_header.count};
			}

			//! Returns a read-only span of the component data.
			//! \warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			//! \tparam T Component
			//! \return Span of read-only component data.
			template <typename T>
			GAIA_NODISCARD GAIA_FORCEINLINE auto view_inter(uint32_t from, uint32_t to) const //
					-> decltype(std::span<const uint8_t>{}) {

				if constexpr (std::is_same_v<core::raw_t<T>, Entity>) {
					GAIA_ASSERT(to <= m_header.count);
					return {(const uint8_t*)&m_records.pEntities[from], to - from};
				} else if constexpr (is_pair<T>::value) {
					using TT = typename T::type;
					using U = typename component_type_t<TT>::Type;
					static_assert(!std::is_empty_v<U>, "Attempting to get value of an empty component");

					constexpr auto kind = entity_kind_v<TT>;
					const auto rel = m_header.cc->get<typename T::rel>().entity;
					const auto tgt = m_header.cc->get<typename T::tgt>().entity;
					const auto compIdx = comp_idx((Entity)Pair(rel, tgt));

					if constexpr (mem::is_soa_layout_v<U>) {
						GAIA_ASSERT(from == 0);
						GAIA_ASSERT(to == capacity());
						return {comp_ptr(compIdx), to};
					} else if constexpr (kind == EntityKind::EK_Gen) {
						GAIA_ASSERT(to <= m_header.count);
						return {comp_ptr(compIdx, from), to - from};
					} else {
						GAIA_ASSERT(to <= m_header.count);
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

					if constexpr (mem::is_soa_layout_v<U>) {
						GAIA_ASSERT(from == 0);
						GAIA_ASSERT(to == capacity());
						return {comp_ptr(compIdx), to};
					} else if constexpr (kind == EntityKind::EK_Gen) {
						GAIA_ASSERT(to <= m_header.count);
						return {comp_ptr(compIdx, from), to - from};
					} else {
						GAIA_ASSERT(to <= m_header.count);
						// GAIA_ASSERT(count == 1); we don't really care and always consider 1 for unique components
						return {comp_ptr(compIdx), 1};
					}
				}
			}

			//! Returns a read-write span of the component data. Also updates the world version for the component.
			//! \warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			//! \tparam T Component
			//! \tparam WorldVersionUpdateWanted If true, the world version is updated as a result of the write access
			//! \return Span of read-write component data.
			template <typename T, bool WorldVersionUpdateWanted>
			GAIA_NODISCARD GAIA_FORCEINLINE auto view_mut_inter(uint32_t from, uint32_t to) //
					-> decltype(std::span<uint8_t>{}) {
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
					if constexpr (WorldVersionUpdateWanted) {
						update_world_version(compIdx);

#if GAIA_ENABLE_SET_HOOKS
						const auto& rec = m_records.pRecords[compIdx];
						if GAIA_UNLIKELY (rec.pItem->comp_hooks.func_set != nullptr)
							rec.pItem->comp_hooks.func_set(*m_header.world, rec, *this);
#endif
					}

					if constexpr (mem::is_soa_layout_v<U>) {
						GAIA_ASSERT(from == 0);
						GAIA_ASSERT(to == capacity());
						return {comp_ptr_mut(compIdx), to};
					} else if constexpr (kind == EntityKind::EK_Gen) {
						GAIA_ASSERT(to <= m_header.count);
						return {comp_ptr_mut(compIdx, from), to - from};
					} else {
						GAIA_ASSERT(to <= m_header.count);
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
					if constexpr (WorldVersionUpdateWanted) {
						update_world_version(compIdx);

#if GAIA_ENABLE_SET_HOOKS
						const auto& rec = m_records.pRecords[compIdx];
						if GAIA_UNLIKELY (rec.pItem->comp_hooks.func_set != nullptr)
							rec.pItem->comp_hooks.func_set(*m_header.world, rec, *this);
#endif
					}

					if constexpr (mem::is_soa_layout_v<U>) {
						GAIA_ASSERT(from == 0);
						GAIA_ASSERT(to == capacity());
						return {comp_ptr_mut(compIdx), to};
					} else if constexpr (kind == EntityKind::EK_Gen) {
						GAIA_ASSERT(to <= m_header.count);
						return {comp_ptr_mut(compIdx, from), to - from};
					} else {
						GAIA_ASSERT(to <= m_header.count);
						// GAIA_ASSERT(count == 1); we don't really care and always consider 1 for unique components
						return {comp_ptr_mut(compIdx), 1};
					}
				}
			}

			//! Returns the value stored in the component \tparam T on \param row in the chunk.
			//! \warning It is expected the \param row is valid. Undefined behavior otherwise.
			//! \warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			//! \tparam T Component
			//! \param row Row of entity in the chunk
			//! \return Value stored in the component if smaller than 8 bytes. Const reference to the value otherwise.
			template <typename T>
			GAIA_NODISCARD decltype(auto) comp_inter(uint16_t row) const {
				using U = typename actual_type_t<T>::Type;
				using RetValueType = decltype(view<T>()[0]);

				GAIA_ASSERT(row < m_header.count);
				if constexpr (mem::is_soa_layout_v<U>)
					return view<T>(0, capacity())[row];
				else if constexpr (sizeof(RetValueType) <= 8)
					return view<T>()[row];
				else
					return (const U&)view<T>()[row];
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
						// Chunk "header" area (before actual entity/component data starts)
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

			//! Allocates memory for a new chunk.
			//! \param chunkIndex Index of this chunk within the parent archetype
			//! \return Newly allocated chunk
			static Chunk* create(
					const World& wld, const ComponentCache& cc, //
					uint32_t chunkIndex, uint16_t capacity, uint8_t cntEntities, uint8_t genEntities, //
					uint16_t dataBytes, uint32_t& worldVersion,
					// data offsets
					const ChunkDataOffsets& offsets,
					// component entities
					const Entity* ids,
					// component
					const Component* comps,
					// component offsets
					const ChunkDataOffset* compOffs) {
				const auto totalBytes = chunk_total_bytes(dataBytes);
				const auto sizeType = mem_block_size_type(totalBytes);
#if GAIA_ECS_CHUNK_ALLOCATOR
				auto* pChunk = (Chunk*)ChunkAllocator::get().alloc(totalBytes);
				(void)new (pChunk) Chunk(wld, cc, chunkIndex, capacity, genEntities, sizeType, worldVersion);
#else
				GAIA_ASSERT(totalBytes <= MaxMemoryBlockSize);
				const auto allocSize = mem_block_size(sizeType);
				auto* pChunkMem = mem::AllocHelper::alloc<uint8_t>(allocSize);
				std::memset(pChunkMem, 0, allocSize);
				auto* pChunk = new (pChunkMem) Chunk(wld, cc, chunkIndex, capacity, genEntities, sizeType, worldVersion);
#endif

				pChunk->init((uint32_t)cntEntities, ids, comps, offsets, compOffs);
				return pChunk;
			}

			//! Releases all memory allocated by \param pChunk.
			//! \param pChunk Chunk which we want to destroy
			static void free(Chunk* pChunk) {
				GAIA_ASSERT(pChunk != nullptr);
				GAIA_ASSERT(!pChunk->dead());

				// Mark as dead
				pChunk->die();

				// Call destructors for components that need it
				pChunk->call_all_dtors();

				pChunk->~Chunk();
#if GAIA_ECS_CHUNK_ALLOCATOR
				ChunkAllocator::get().free(pChunk);
#else
				mem::AllocHelper::free((uint8_t*)pChunk);
#endif
			}

			//! Remove the last entity from a chunk.
			//! If as a result the chunk becomes empty it is scheduled for deletion.
			//! \param chunksToDelete Container of chunks ready for deletion
			void remove_last_entity() {
				// Should never be called over an empty chunk
				GAIA_ASSERT(!empty());

#if GAIA_ASSERT_ENABLED
				// Invalidate the entity in chunk data
				entity_view_mut()[m_header.count - 1] = EntityBad;
#endif

				--m_header.count;
				--m_header.countEnabled;
			}

			//! Updates the version numbers for this chunk.
			void update_versions() {
				::gaia::ecs::update_version(m_header.worldVersion);
				update_world_version();
			}

			//! Returns a read-only entity or component view.
			//! \warning If \tparam T is a component it is expected it is present. Undefined behavior otherwise.
			//! \tparam T Component or Entity
			//! \param from First valid entity row
			//! \param to Last valid entity row
			//! \return Entity of component view with read-only access
			template <typename T>
			GAIA_NODISCARD decltype(auto) view(uint16_t from, uint16_t to) const {
				using U = typename actual_type_t<T>::Type;

				// Always consider full range for SoA
				if constexpr (mem::is_soa_layout_v<U>)
					return mem::auto_view_policy_get<U>{view_inter<T>(0, capacity())};
				else
					return mem::auto_view_policy_get<U>{view_inter<T>(from, to)};
			}

			template <typename T>
			GAIA_NODISCARD decltype(auto) view() const {
				return view<T>(0, m_header.count);
			}

			template <typename T>
			GAIA_NODISCARD decltype(auto) view_raw(const void* ptr, uint32_t size) const {
				using U = typename actual_type_t<T>::Type;
				return mem::auto_view_policy_get<U>{std::span{(const uint8_t*)ptr, size}};
			}

			//! Returns a mutable entity or component view.
			//! \warning If \tparam T is a component it is expected it is present. Undefined behavior otherwise.
			//! \tparam T Component or Entity
			//! \param from First valid entity row
			//! \param to Last valid entity row
			//! \return Entity or component view with read-write access
			template <typename T>
			GAIA_NODISCARD decltype(auto) view_mut(uint16_t from, uint16_t to) {
				using U = typename actual_type_t<T>::Type;
				static_assert(!std::is_same_v<U, Entity>, "Modifying chunk entities via view_mut is forbidden");

				// Always consider full range for SoA
				if constexpr (mem::is_soa_layout_v<U>)
					return mem::auto_view_policy_set<U>{view_mut_inter<T, true>(0, capacity())};
				else
					return mem::auto_view_policy_set<U>{view_mut_inter<T, true>(from, to)};
			}

			template <typename T>
			GAIA_NODISCARD decltype(auto) view_mut() {
				return view_mut<T>(0, m_header.count);
			}

			template <typename T>
			GAIA_NODISCARD decltype(auto) view_mut_raw(void* ptr, uint32_t size) const {
				using U = typename actual_type_t<T>::Type;
				static_assert(!std::is_same_v<U, Entity>, "Modifying chunk entities via view_mut is forbidden");

				return mem::auto_view_policy_set<U>{std::span{(uint8_t*)ptr, size}};
			}

			//! Returns a mutable component view.
			//! Doesn't update the world version when the access is acquired.
			//! \warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			//! \tparam T Component
			//! \param from First valid entity row
			//! \param to Last valid entity row
			//! \return Component view with read-write access
			template <typename T>
			GAIA_NODISCARD decltype(auto) sview_mut(uint16_t from, uint16_t to) {
				using U = typename actual_type_t<T>::Type;
				static_assert(!std::is_same_v<U, Entity>, "Modifying chunk entities via sview_mut is forbidden");

				// Always consider full range for SoA
				if constexpr (mem::is_soa_layout_v<U>)
					return mem::auto_view_policy_set<U>{view_mut_inter<T, false>(0, capacity())};
				else
					return mem::auto_view_policy_set<U>{view_mut_inter<T, false>(from, to)};
			}

			template <typename T>
			GAIA_NODISCARD decltype(auto) sview_mut_raw(void* ptr, uint32_t size) const {
				using U = typename actual_type_t<T>::Type;
				static_assert(!std::is_same_v<U, Entity>, "Modifying chunk entities via sview_mut is forbidden");

				return mem::auto_view_policy_set<U>{std::span{(uint8_t*)ptr, size}};
			}

			template <typename T>
			GAIA_NODISCARD decltype(auto) sview_mut() {
				return sview_mut<T>(0, m_header.count);
			}

			//! Marks the component \tparam T as modified. Best used with sview to manually trigger
			//! an update at user's whim.
			//! If \tparam TriggerHooks is true, also triggers the component's set hooks.
			template <
					typename T
#if GAIA_ENABLE_HOOKS
					,
					bool TriggerHooks
#endif
					>
			GAIA_FORCEINLINE void modify() {
				static_assert(!std::is_same_v<core::raw_t<T>, Entity>, "mod can't be used to modify Entity");

				if constexpr (is_pair<T>::value) {
					using TT = typename T::type;
					using U = typename component_type_t<TT>::Type;
					static_assert(!std::is_empty_v<U>, "mut can't be used to modify tag components");

#if GAIA_ASSERT_ENABLED
					// constexpr auto kind = entity_kind_v<TT>;
#endif
					const auto rel = m_header.cc->get<typename T::rel>().entity;
					const auto tgt = m_header.cc->get<typename T::tgt>().entity;
					const auto compIdx = comp_idx((Entity)Pair(rel, tgt));

					// Update version number if necessary so we know RW access was used on the chunk
					update_world_version(compIdx);

#if GAIA_ENABLE_SET_HOOKS
					if constexpr (TriggerHooks) {
						const auto& rec = m_records.pRecords[compIdx];
						if GAIA_UNLIKELY (rec.pItem->comp_hooks.func_set != nullptr)
							rec.pItem->comp_hooks.func_set(*m_header.world, rec, *this);
					}
#endif
				} else {
					using U = typename component_type_t<T>::Type;
					static_assert(!std::is_empty_v<U>, "mut can't be used to modify tag components");

#if GAIA_ASSERT_ENABLED
					constexpr auto kind = entity_kind_v<T>;
#endif
					const auto comp = m_header.cc->get<T>().entity;
					GAIA_ASSERT(comp.kind() == kind);
					const auto compIdx = comp_idx(comp);

					// Update version number if necessary so we know RW access was used on the chunk
					update_world_version(compIdx);

#if GAIA_ENABLE_SET_HOOKS
					if constexpr (TriggerHooks) {
						const auto& rec = m_records.pRecords[compIdx];
						if GAIA_UNLIKELY (rec.pItem->comp_hooks.func_set != nullptr)
							rec.pItem->comp_hooks.func_set(*m_header.world, rec, *this);
					}
#endif
				}
			}

			//! Returns either a mutable or immutable entity/component view based on the requested type.
			//! Value and const types are considered immutable. Anything else is mutable.
			//! \warning If \tparam T is a component it is expected to be present. Undefined behavior otherwise.
			//! \tparam T Component or Entity
			//! \param from First valid entity row
			//! \param to Last valid entity row
			//! \return Entity or component view
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
				return view_auto<T>(0, m_header.count);
			}

			//! Returns either a mutable or immutable entity/component view based on the requested type.
			//! Value and const types are considered immutable. Anything else is mutable.
			//! Doesn't update the world version when read-write access is acquired.
			//! \warning If \tparam T is a component it is expected to be present. Undefined behavior otherwise.
			//! \tparam T Component or Entity
			//! \param from First valid entity row
			//! \param to Last valid entity row
			//! \return Entity or component view
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
				return sview_auto<T>(0, m_header.count);
			}

			GAIA_NODISCARD EntitySpan entity_view() const {
				return {(const Entity*)m_records.pEntities, m_header.count};
			}

			GAIA_NODISCARD EntitySpan ids_view() const {
				return {(const Entity*)m_records.pCompEntities, m_header.cntEntities};
			}

			GAIA_NODISCARD std::span<const ComponentRecord> comp_rec_view() const {
				return {m_records.pRecords, m_header.cntEntities};
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

			//! Make \param entity a part of the chunk at the version of the world.
			//! \return Row of entity within the chunk.
			GAIA_NODISCARD uint16_t add_entity(Entity entity) {
				const auto row = m_header.count++;

				// Zero after increase of value means an overflow!
				GAIA_ASSERT(m_header.count != 0);

				++m_header.countEnabled;
				entity_view_mut()[row] = entity;

				return row;
			}

			//! Copies all data associated with \param srcEntity into \param dstEntity.
			static void copy_entity_data(Entity srcEntity, Entity dstEntity, EntityContainers& recs) {
				GAIA_PROF_SCOPE(Chunk::copy_entity_data);

				auto& srcEntityContainer = recs[srcEntity];
				auto* pSrcChunk = srcEntityContainer.pChunk;

				auto& dstEntityContainer = recs[dstEntity];
				auto* pDstChunk = dstEntityContainer.pChunk;

				GAIA_ASSERT(srcEntityContainer.pArchetype == dstEntityContainer.pArchetype);

				auto srcRecs = pSrcChunk->comp_rec_view();

				// Copy generic component data from reference entity to our new entity.
				// Unique components do not change place in the chunk so there is no need to move them.
				GAIA_FOR(pSrcChunk->m_header.genEntities) {
					const auto& rec = srcRecs[i];
					if (rec.comp.size() == 0U)
						continue;

					const auto* pSrc = (const void*)pSrcChunk->comp_ptr_mut(i);
					auto* pDst = (void*)pDstChunk->comp_ptr_mut(i);
					rec.pItem->copy(
							pDst, pSrc, dstEntityContainer.row, srcEntityContainer.row, pDstChunk->capacity(), pSrcChunk->capacity());
				}
			}

			//! Moves all data associated with \param entity into the chunk so that it is stored at the row \param row.
			void move_entity_data(Entity entity, uint16_t row, EntityContainers& recs) {
				GAIA_PROF_SCOPE(Chunk::move_entity_data);

				auto& ec = recs[entity];
				auto* pSrcChunk = ec.pChunk;
				auto srcRecs = pSrcChunk->comp_rec_view();

				// Copy generic component data from reference entity to our new entity.
				// Unique components do not change place in the chunk so there is no need to move them.
				GAIA_FOR(pSrcChunk->m_header.genEntities) {
					const auto& rec = srcRecs[i];
					if (rec.comp.size() == 0U)
						continue;

					auto* pSrc = (void*)pSrcChunk->comp_ptr_mut(i);
					auto* pDst = (void*)comp_ptr_mut(i);
					rec.pItem->ctor_move(pDst, pSrc, row, ec.row, capacity(), pSrcChunk->capacity());
				}
			}

			//! Copies all data associated with \param entity into the chunk so that it is stored at the row \param row.
			static void copy_foreign_entity_data(Chunk* pSrcChunk, uint32_t srcRow, Chunk* pDstChunk, uint32_t dstRow) {
				GAIA_PROF_SCOPE(Chunk::copy_foreign_entity_data);

				GAIA_ASSERT(pSrcChunk != nullptr);
				GAIA_ASSERT(pDstChunk != nullptr);
				GAIA_ASSERT(srcRow < pSrcChunk->size());
				GAIA_ASSERT(dstRow < pDstChunk->size());

				auto srcIds = pSrcChunk->ids_view();
				auto dstIds = pDstChunk->ids_view();
				auto dstRecs = pDstChunk->comp_rec_view();

				// Find intersection of the two component lists.
				// Arrays are sorted so we can do linear intersection lookup.
				// Call constructor on each match.
				// Unique components do not change place in the chunk so there is no need to move them.
				{
					uint32_t i = 0;
					uint32_t j = 0;
					while (i < pSrcChunk->m_header.genEntities && j < pDstChunk->m_header.genEntities) {
						const auto oldId = srcIds[i];
						const auto newId = dstIds[j];

						if (oldId == newId) {
							const auto& rec = dstRecs[j];
							if (rec.comp.size() != 0U) {
								auto* pSrc = (void*)pSrcChunk->comp_ptr_mut(i);
								auto* pDst = (void*)pDstChunk->comp_ptr_mut(j);
								rec.pItem->ctor_copy(pDst, pSrc, dstRow, srcRow, pDstChunk->capacity(), pSrcChunk->capacity());
							}

							++i;
							++j;
						} else if (SortComponentCond{}.operator()(oldId, newId)) {
							++i;
						} else {
							// No match with the old chunk. Construct the component
							const auto& rec = dstRecs[j];
							if (rec.pItem != nullptr && rec.pItem->func_ctor != nullptr) {
								auto* pDst = (void*)pDstChunk->comp_ptr_mut(j, dstRow);
								rec.pItem->func_ctor(pDst, 1);
							}

							++j;
						}
					}

					// Initialize the rest of the components if they are generic.
					for (; j < pDstChunk->m_header.genEntities; ++j) {
						const auto& rec = dstRecs[j];
						if (rec.pItem != nullptr && rec.pItem->func_ctor != nullptr) {
							auto* pDst = (void*)pDstChunk->comp_ptr_mut(j, dstRow);
							rec.pItem->func_ctor(pDst, 1);
						}
					}
				}
			}

			//! Moves all data associated with \param entity into the chunk so that it is stored at the row \param row.
			static void move_foreign_entity_data(Chunk* pSrcChunk, uint32_t srcRow, Chunk* pDstChunk, uint32_t dstRow) {
				GAIA_PROF_SCOPE(Chunk::move_foreign_entity_data);

				GAIA_ASSERT(pSrcChunk != nullptr);
				GAIA_ASSERT(pDstChunk != nullptr);
				GAIA_ASSERT(srcRow < pSrcChunk->size());
				GAIA_ASSERT(dstRow < pDstChunk->size());

				auto srcIds = pSrcChunk->ids_view();
				auto dstIds = pDstChunk->ids_view();
				auto dstRecs = pDstChunk->comp_rec_view();

				// Find intersection of the two component lists.
				// Arrays are sorted so we can do linear intersection lookup.
				// Call constructor on each match.
				// Unique components do not change place in the chunk so there is no need to move them.
				{
					uint32_t i = 0;
					uint32_t j = 0;
					while (i < pSrcChunk->m_header.genEntities && j < pDstChunk->m_header.genEntities) {
						const auto oldId = srcIds[i];
						const auto newId = dstIds[j];

						if (oldId == newId) {
							const auto& rec = dstRecs[j];
							if (rec.comp.size() != 0U) {
								auto* pSrc = (void*)pSrcChunk->comp_ptr_mut(i);
								auto* pDst = (void*)pDstChunk->comp_ptr_mut(j);
								rec.pItem->ctor_move(pDst, pSrc, dstRow, srcRow, pDstChunk->capacity(), pSrcChunk->capacity());
							}

							++i;
							++j;
						} else if (SortComponentCond{}.operator()(oldId, newId)) {
							++i;
						} else {
							// No match with the old chunk. Construct the component
							const auto& rec = dstRecs[j];
							if (rec.pItem != nullptr && rec.pItem->func_ctor != nullptr) {
								auto* pDst = (void*)pDstChunk->comp_ptr_mut(j, dstRow);
								rec.pItem->func_ctor(pDst, 1);
							}

							++j;
						}
					}

					// Initialize the rest of the components if they are generic.
					for (; j < pDstChunk->m_header.genEntities; ++j) {
						const auto& rec = dstRecs[j];
						if (rec.pItem != nullptr && rec.pItem->func_ctor != nullptr) {
							auto* pDst = (void*)pDstChunk->comp_ptr_mut(j, dstRow);
							rec.pItem->func_ctor(pDst, 1);
						}
					}
				}
			}

			//! Tries to remove the entity at \param row.
			//! Removal is done via swapping with last entity in chunk.
			//! Upon removal, all associated data is also removed.
			//! If the entity at the given row already is the last chunk entity, it is removed directly.
			void remove_entity_inter(uint16_t row, EntityContainers& recs) {
				GAIA_PROF_SCOPE(Chunk::remove_entity_inter);

				const uint16_t rowA = row;
				const uint16_t rowB = m_header.count - 1;
				// The "rowA" entity is the one we are going to destroy so it needs to precede the "rowB"
				GAIA_ASSERT(rowA <= rowB);

				// To move anything, we need at least 2 entities
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

						auto* pSrc = (void*)comp_ptr_mut(i);
						rec.pItem->move(pSrc, pSrc, rowA, rowB, capacity(), capacity());

						pSrc = (void*)comp_ptr_mut(i, rowB);
						rec.pItem->dtor(pSrc);
					}

					// Entity has been replaced with the last one in our chunk. Update its container record.
					ecB.row = rowA;
				} else if (m_header.hasAnyCustomGenDtor) {
					// This is the last entity in the chunk so simply destroy its data
					auto recView = comp_rec_view();
					GAIA_FOR(m_header.genEntities) {
						const auto& rec = recView[i];
						if (rec.comp.size() == 0U)
							continue;

						auto* pSrc = (void*)comp_ptr_mut(i, rowA);
						rec.pItem->dtor(pSrc);
					}
				}
			}

			//! Tries to remove the entity at row \param row.
			//! Removal is done via swapping with last entity in chunk.
			//! Upon removal, all associated data is also removed.
			//! If the entity at the given row already is the last chunk entity, it is removed directly.
			void remove_entity(uint16_t row, EntityContainers& recs) {
				if GAIA_UNLIKELY (m_header.count == 0)
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
				remove_last_entity();
			}

			//! Tries to swap the entity at row \param rowA with the one at the row \param rowB.
			//! When swapping, all data associated with the two entities is swapped as well.
			//! If \param rowA equals \param rowB no swapping is performed.
			//! \warning "rowA" must he smaller or equal to "rowB"
			void swap_chunk_entities(uint16_t rowA, uint16_t rowB, EntityContainers& recs) {
				// The "rowA" entity is the one we are going to destroy so it needs to precede the "rowB".
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
				GAIA_FOR(m_header.genEntities) {
					const auto& rec = recView[i];
					if (rec.comp.size() == 0U)
						continue;

					GAIA_ASSERT(rec.pData == comp_ptr_mut(i));
					rec.pItem->swap(rec.pData, rec.pData, rowA, rowB, capacity(), capacity());
				}

				// Update indices in entity container.
				ecA.row = (uint16_t)rowB;
				ecB.row = (uint16_t)rowA;
			}

			//! Enables or disables the entity on a given row in the chunk.
			//! \param row Row of the entity within chunk
			//! \param enableEntity Enables or disabled the entity
			//! \param entities Span of entity container records
			void enable_entity(uint16_t row, bool enableEntity, EntityContainers& recs) {
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

			//! Checks if the entity is enabled.
			//! \param row Row of the entity within chunk
			//! \return True if entity is enabled. False otherwise.
			bool enabled(uint16_t row) const {
				GAIA_ASSERT(m_header.count > 0);

				return row >= (uint16_t)m_header.rowFirstEnabledEntity;
			}

			//! Returns a mutable pointer to chunk data.
			//! \param offset Offset into chunk data
			//! \return Pointer to chunk data.
			uint8_t& data(uint32_t offset) {
				return m_data[offset];
			}

			//! Returns an immutable pointer to chunk data.
			//! \param offset Offset into chunk data
			//! \return Pointer to chunk data.
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

			void call_ctor(uint32_t entIdx, const ComponentCacheItem& item) {
				if (item.func_ctor == nullptr)
					return;

				GAIA_PROF_SCOPE(Chunk::call_ctor);

				const auto compIdx = comp_idx(item.entity);
				auto* pSrc = (void*)comp_ptr_mut(compIdx, entIdx);
				item.func_ctor(pSrc, 1);
			}

			void call_gen_ctors(uint32_t entIdx, uint32_t entCnt) {
				if (!m_header.hasAnyCustomGenCtor)
					return;

				GAIA_PROF_SCOPE(Chunk::call_gen_ctors);

				auto recs = comp_rec_view();
				GAIA_FOR(m_header.genEntities) {
					const auto& rec = recs[i];

					const auto* pItem = rec.pItem;
					if (pItem == nullptr || pItem->func_ctor == nullptr)
						continue;

					auto* pSrc = (void*)comp_ptr_mut(i, entIdx);
					pItem->func_ctor(pSrc, entCnt);
				}
			}

			void call_all_dtors() {
				if (!m_header.hasAnyCustomGenDtor && !m_header.hasAnyCustomUniCtor)
					return;

				GAIA_PROF_SCOPE(Chunk::call_all_dtors);

				auto ids = ids_view();
				auto recs = comp_rec_view();
				const auto recs_cnt = recs.size();
				GAIA_FOR(recs_cnt) {
					const auto& rec = recs[i];

					const auto* pItem = rec.pItem;
					if (pItem == nullptr || pItem->func_dtor == nullptr)
						continue;

					auto* pSrc = (void*)comp_ptr_mut(i, 0);
					const auto e = ids[i];
					const auto cnt = (e.kind() == EntityKind::EK_Gen) ? m_header.count : (uint16_t)1;
					pItem->func_dtor(pSrc, cnt);
				}
			};

			//----------------------------------------------------------------------
			// Check component presence
			//----------------------------------------------------------------------

			//! Checks if a component/entity \param entity is present in the chunk.
			//! \param entity Entity
			//! \return True if found. False otherwise.
			GAIA_NODISCARD bool has(Entity entity) const {
				auto ids = ids_view();
				return core::has(ids, entity);
			}

			//! Checks if component \tparam T is present in the chunk.
			//! \tparam T Component or pair
			//! \return True if the component is present. False otherwise.
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

			//! Sets the value of the unique component \tparam T on \param row in the chunk.
			//! \tparam T Component or pair
			//! \param row Row of entity in the chunk
			//! \param value Value to set for the component
			//! \warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			template <typename T>
			decltype(auto) set(uint16_t row) {
				verify_comp<T>();

				GAIA_ASSERT2(
						actual_type_t<T>::Kind == EntityKind::EK_Gen || row == 0,
						"Set providing a row can only be used with generic components");

				// Update the world version
				::gaia::ecs::update_version(m_header.worldVersion);

				GAIA_ASSERT(row < m_header.capacity);
				return view_mut<T>()[row];
			}

			//! Sets the value of a generic entity \param type at the position \param row in the chunk.
			//! \param row Row of entity in the chunk
			//! \param type Component/entity/pair
			//! \param value New component value
			//! \warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			template <typename T>
			decltype(auto) set(uint16_t row, Entity type) {
				verify_comp<T>();

				GAIA_ASSERT2(
						type.kind() == EntityKind::EK_Gen || row == 0,
						"Set providing a row can only be used with generic components");
				GAIA_ASSERT(type.kind() == entity_kind_v<T>);

				// Update the world version
				::gaia::ecs::update_version(m_header.worldVersion);

				GAIA_ASSERT(row < m_header.capacity);

				// TODO: This function works but is useless because it does the same job as
				//       set(uint16_t row, U&& value).
				//       This is because T needs to match U anyway for the component lookup to succeed.
				(void)type;
				// const uint32_t col = comp_idx(type);
				//(void)col;

				return view_mut<T>()[row];
			}

			//! Sets the value of the unique component \tparam T on \param row in the chunk.
			//! \tparam T Component or pair
			//! \param row Row of entity in the chunk
			//! \param value Value to set for the component
			//! \warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			//! \warning World version is not updated so Query filters will not be able to catch this change.
			template <typename T>
			decltype(auto) sset(uint16_t row) {
				GAIA_ASSERT2(
						actual_type_t<T>::Kind == EntityKind::EK_Gen || row == 0,
						"Set providing a row can only be used with generic components");

				GAIA_ASSERT(row < m_header.capacity);
				return sview_mut<T>()[row];
			}

			//! Sets the value of a generic entity \param type at the position \param row in the chunk.
			//! \param row Row of entity in the chunk
			//! \param type Component/entity/pair
			//! \param value New component value
			//! \warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			//! \warning World version is not updated so Query filters will not be able to catch this change.
			template <typename T>
			decltype(auto) sset(uint16_t row, Entity type) {
				static_assert(core::is_raw_v<T>);

				GAIA_ASSERT2(
						type.kind() == EntityKind::EK_Gen || row == 0,
						"Set providing a row can only be used with generic components");

				GAIA_ASSERT(row < m_header.capacity);

				// TODO: This function works but is useless because it does the same job as
				//       sset(uint16_t row, U&& value).
				//       This is because T needs to match U anyway for the component lookup to succeed.
				(void)type;
				// const uint32_t col = comp_idx(type);
				//(void)col;

				return sview_mut<T>()[row];
			}

			//----------------------------------------------------------------------
			// Read component data
			//----------------------------------------------------------------------

			//! Returns the value stored in the generic component \tparam T on \param row in the chunk.
			//! \warning It is expected the \param row is valid. Undefined behavior otherwise.
			//! \warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			//! \tparam T Component or pair
			//! \param row Row of entity in the chunk
			//! \return Value stored in the component.
			template <typename T>
			GAIA_NODISCARD decltype(auto) get(uint16_t row) const {
				static_assert(
						entity_kind_v<T> == EntityKind::EK_Gen, "Get providing a row can only be used with generic components");

				return comp_inter<T>(row);
			}

			//! Returns the value stored in the unique component \tparam T.
			//! \warning It is expected the unique component \tparam T is present. Undefined behavior otherwise.
			//! \tparam T Component or pair
			//! \return Value stored in the component.
			template <typename T>
			GAIA_NODISCARD decltype(auto) get() const {
				static_assert(
						entity_kind_v<T> != EntityKind::EK_Gen,
						"Get not providing a row can only be used with non-generic components");

				return comp_inter<T>(0);
			}

			//! Returns the internal index of a component based on the provided \param entity.
			//! \param entity Component
			//! \return Component index if the component was found. -1 otherwise.
			GAIA_NODISCARD uint32_t comp_idx(Entity entity) const {
				return ecs::comp_idx<ChunkHeader::MAX_COMPONENTS>(m_records.pCompEntities, entity);
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

			//! Marks the chunk as dead (ready to delete)
			void die() {
				m_header.dead = 1;
			}

			//! Checks is this chunk is dead (ready to delete)
			GAIA_NODISCARD bool dead() const {
				return m_header.dead == 1;
			}

			//! Starts the process of dying (not yet ready to delete, can be revived)
			void start_dying() {
				GAIA_ASSERT(!dead());
				m_header.lifespanCountdown = ChunkHeader::MAX_CHUNK_LIFESPAN;
			}

			//! Makes a dying chunk alive again
			void revive() {
				GAIA_ASSERT(!dead());
				m_header.lifespanCountdown = 0;
			}

			//! Updates internal lifespan
			//! \return True if there is some lifespan rowA, false otherwise.
			bool progress_death() {
				GAIA_ASSERT(dying());
				--m_header.lifespanCountdown;
				return dying();
			}

			//! Checks is the full capacity of the has has been reached
			GAIA_NODISCARD bool full() const {
				return m_header.count >= m_header.capacity;
			}

			//! Checks is the chunk is semi-full.
			GAIA_NODISCARD bool is_semi() const {
				// We want the chunk filled to at least 75% before considering it semi-full
				constexpr float Threshold = 0.75f;
				return ((float)m_header.count / (float)m_header.capacity) < Threshold;
			}

			//! Returns the total number of entities in the chunk (both enabled and disabled)
			GAIA_NODISCARD uint16_t size() const {
				return m_header.count;
			}

			//! Checks is there are any entities in the chunk
			GAIA_NODISCARD bool empty() const {
				return m_header.count == 0;
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

			//! Returns the total number of generic entities/components in the chunk
			GAIA_NODISCARD uint8_t size_generic() const {
				return m_header.genEntities;
			}

			//! Returns the number of bytes the chunk spans over
			GAIA_NODISCARD uint32_t bytes() const {
				return mem_block_size(m_header.sizeType);
			}

			//! Returns true if the provided version is newer than the one stored internally
			GAIA_NODISCARD bool changed(uint32_t version, uint32_t compIdx) const {
				auto versions = comp_version_view();
				return ::gaia::ecs::version_changed(versions[compIdx], version);
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
