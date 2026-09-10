#pragma once
#include "gaia/config/config.h"
#include "gaia/config/profiler.h"

#include <cstdint>
#include <cstring>
#include <tuple>
#include <type_traits>
#include <utility>

#include "gaia/cnt/sarray_ext.h"
#include "gaia/core/utility.h"
#include "gaia/ecs/archetype_common.h"
#include "gaia/ecs/chunk_allocator.h"
#include "gaia/ecs/chunk_header.h"
#include "gaia/ecs/common.h"
#include "gaia/ecs/component.h"
#include "gaia/ecs/component_cache.h"
#include "gaia/ecs/component_desc.h"
#include "gaia/ecs/entity_container.h"
#include "gaia/ecs/id.h"
#include "gaia/mem/data_layout_policy.h"
#include "gaia/mem/mem_alloc.h"
#include "gaia/ser/ser_binary.h"
#include "gaia/ser/ser_rt.h"

namespace gaia {
	namespace ecs {
		class World;
		class Chunk;
		void world_invalidate_sorted_queries_for_entity(World& world, Entity entity);
		void world_invalidate_sorted_queries(World& world);
		void world_notify_on_set(World& world, Entity term, Chunk& chunk, uint16_t from, uint16_t to);

		//! Fixed-capacity archetype storage unit holding entities and their component columns.
		class GAIA_API Chunk final {
		public:
			//! Fixed-size array of entity identifiers held by the chunk.
			using EntityArray = cnt::sarray_ext<Entity, ChunkHeader::MAX_COMPONENTS>;
			//! Fixed-size array of component descriptors for the chunk columns.
			using ComponentArray = cnt::sarray_ext<Component, ChunkHeader::MAX_COMPONENTS>;
			//! Fixed-size array of byte offsets into the chunk data area, one per column.
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
			//! Use m_data[1], not something bigger, to avoid triggering sanitizers. They will understand this
			//! is a flexible array.
			uint8_t m_data[1];

			GAIA_MSVC_WARNING_PUSH()
			GAIA_MSVC_WARNING_DISABLE(26495)

			// Hidden default constructor. Only use to calculate the relative offset of m_data
			Chunk() = default;

			Chunk(
					const World& wld, const ComponentCache& cc, //
					uint32_t chunkIndex, uint16_t capacity, //
					uint32_t& worldVersion): //
					m_header(wld, cc, chunkIndex, capacity, worldVersion) {
				// Chunk data area consist of memory offsets, entities, and component data. Normally,  we would need
				// to in-place construct all of it manually.
				// However, the memory offsets and entities are all trivial types and components are initialized via
				// their constructors on-demand (if not trivial) so we do not really need to do any construction here.
			}

			GAIA_MSVC_WARNING_POP()

			GAIA_CLANG_WARNING_PUSH()
			// Memory is aligned so we can silence this warning
			GAIA_CLANG_WARNING_DISABLE("-Wcast-align")

			void init(
					uint32_t cntEntities, const Entity* ids, const ComponentCacheItem* const* pItems,
					const ChunkDataOffsets& headerOffsets, const ChunkDataOffset* compOffs) {
				m_header.cntEntities = (uint8_t)cntEntities;

				// Cache pointers to versions
				m_records.pVersions = (ComponentVersion*)&data(headerOffsets.firstByte_Versions);

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
						dst[j].comp = pItems[j] == nullptr //
															? Component(IdentifierIdBad, 0, 0, 0, DataStorageType::Table)
															: archetype_component(ids[j], pItems[j]->comp);
						dst[j].pData = &data(compOffs[j]);
						dst[j].pItem = pItems[j];
					}
				}

				m_records.pEntities = (Entity*)&data(headerOffsets.firstByte_EntityData);

				// Now that records are set, we use the cached component descriptors to set ctor/dtor masks.
				{
					auto recs = comp_rec_view();
					const auto recs_cnt = recs.size();
					GAIA_FOR(recs_cnt) {
						const auto& rec = recs[i];
						if (!component_uses_table_storage(rec.comp))
							continue;

						m_header.hasAnyCustomCtor |= (rec.pItem->func_ctor != nullptr);
						m_header.hasAnyCustomDtor |= (rec.pItem->func_dtor != nullptr);
					}
				}

				// Make sure world versions are set initially.
				update_world_version_init();
			}

			GAIA_CLANG_WARNING_POP()

			//! Returns a read-only view of data version numbers.
			//! The first index belongs to the entity itself. Following indices belong to data attached to the entity.
			GAIA_NODISCARD std::span<const ComponentVersion> comp_version_view() const {
				return {(const ComponentVersion*)m_records.pVersions, (size_t)m_header.cntEntities + 1};
			}

			//! Returns a mutable view of data version numbers.
			//! The first index belongs to the entity itself. Following indices belong to data attached to the entity.
			GAIA_NODISCARD std::span<ComponentVersion> comp_version_view_mut() {
				return {m_records.pVersions, (size_t)m_header.cntEntities + 1};
			}

			GAIA_NODISCARD std::span<Entity> entity_view_mut() {
				return {m_records.pEntities, m_header.count};
			}

			//! Returns a read-only span of the component data.
			//! \tparam T Component
			//! \param compIdx Index of component column
			//! \param from Starting row
			//! \param to Ending row
			//! \return Span of read-only component data.
			//! \warning It is expected the component column \a compIdx is valid for \a T.
			template <typename T>
			GAIA_NODISCARD GAIA_FORCEINLINE auto view_inter_idx(uint32_t compIdx, uint32_t from, uint32_t to) const //
					-> decltype(std::span<const uint8_t>{}) {

				if constexpr (std::is_same_v<core::raw_t<T>, Entity>) {
					GAIA_ASSERT(to <= m_header.count);
					return {(const uint8_t*)&m_records.pEntities[from], to - from};
				} else if constexpr (is_pair<T>::value) {
					using TT = typename T::type;
					using U = typename component_type_t<TT>::Type;
					static_assert(!std::is_empty_v<U>, "Attempting to get value of an empty component");

					if constexpr (mem::is_soa_layout_v<U>) {
						GAIA_ASSERT(from == 0);
						GAIA_ASSERT(to == capacity());
						return {comp_ptr(compIdx), to};
					} else {
						GAIA_ASSERT(to <= m_header.count);
						return {comp_ptr(compIdx, from), to - from};
					}
				} else {
					using U = typename component_type_t<T>::Type;
					static_assert(!std::is_empty_v<U>, "Attempting to get value of an empty component");

					if constexpr (mem::is_soa_layout_v<U>) {
						GAIA_ASSERT(from == 0);
						GAIA_ASSERT(to == capacity());
						return {comp_ptr(compIdx), to};
					} else {
						GAIA_ASSERT(to <= m_header.count);
						return {comp_ptr(compIdx, from), to - from};
					}
				}
			}

			//! Returns a read-only span of the component data.
			//! \warning It is expected the component \a T is present. Undefined behavior otherwise.
			//! \tparam T Component
			//! \param from Starting row
			//! \param to Ending row
			//! \return Span of read-only component data.
			template <typename T>
			GAIA_NODISCARD GAIA_FORCEINLINE auto view_inter(uint32_t from, uint32_t to) const //
					-> decltype(std::span<const uint8_t>{}) {
				if constexpr (std::is_same_v<core::raw_t<T>, Entity>)
					return view_inter_idx<T>(BadIndex, from, to);
				else if constexpr (is_pair<T>::value) {
					const auto rel = m_header.cc->get<typename T::rel>().entity;
					const auto tgt = m_header.cc->get<typename T::tgt>().entity;
					return view_inter_idx<T>(comp_idx((Entity)Pair(rel, tgt)), from, to);
				} else {
					const auto comp = m_header.cc->get<T>().entity;
					return view_inter_idx<T>(comp_idx(comp), from, to);
				}
			}

			//! Returns a read-write span of the component data. Also updates the world version for the component.
			//! \warning It is expected the component \a T is present. Undefined behavior otherwise.
			//! \tparam T Component
			//! \tparam WorldVersionUpdateWanted If true, the world version is updated as a result of the write access
			//! \param compIdx Index of component column
			//! \param from Starting row
			//! \param to Ending row
			//! \return Span of read-write component data.
			template <typename T, bool WorldVersionUpdateWanted>
			GAIA_NODISCARD GAIA_FORCEINLINE auto view_mut_inter_idx(uint32_t compIdx, uint32_t from, uint32_t to) //
					-> decltype(std::span<uint8_t>{}) {
				static_assert(!std::is_same_v<core::raw_t<T>, Entity>, "view_mut can't be used to modify Entity");

				if constexpr (is_pair<T>::value) {
					using TT = typename T::type;
					using U = typename component_type_t<TT>::Type;
					static_assert(!std::is_empty_v<U>, "view_mut can't be used to modify tag components");

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
					} else {
						GAIA_ASSERT(to <= m_header.count);
						return {comp_ptr_mut(compIdx, from), to - from};
					}
				} else {
					using U = typename component_type_t<T>::Type;
					static_assert(!std::is_empty_v<U>, "view_mut can't be used to modify tag components");

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
					} else {
						GAIA_ASSERT(to <= m_header.count);
						return {comp_ptr_mut(compIdx, from), to - from};
					}
				}
			}

			template <typename T, bool WorldVersionUpdateWanted>
			GAIA_NODISCARD GAIA_FORCEINLINE auto view_mut_inter(uint32_t from, uint32_t to) //
					-> decltype(std::span<uint8_t>{}) {
				if constexpr (is_pair<T>::value) {
					const auto rel = m_header.cc->get<typename T::rel>().entity;
					const auto tgt = m_header.cc->get<typename T::tgt>().entity;
					return view_mut_inter_idx<T, WorldVersionUpdateWanted>(comp_idx((Entity)Pair(rel, tgt)), from, to);
				} else {
					const auto comp = m_header.cc->get<T>().entity;
					return view_mut_inter_idx<T, WorldVersionUpdateWanted>(comp_idx(comp), from, to);
				}
			}

		public:
			//! Returns a read-write span of the component data. Also updates the world version for the component.
			//! \warning It is expected the component with \a compIdx is present. Undefined behavior otherwise.
			//! \param compIdx Component index
			//! \param row Row of entity in the chunk
			//! \tparam WorldVersionUpdateWanted If true, the world version is updated as a result of the write access
			//! \return Pointer to component data.
			template <bool WorldVersionUpdateWanted>
			GAIA_NODISCARD GAIA_FORCEINLINE auto comp_ptr_mut_gen(uint32_t compIdx, uint32_t row) {
				// Update version number if necessary so we know RW access was used on the chunk
				if constexpr (WorldVersionUpdateWanted) {
					update_world_version(compIdx);

#if GAIA_ENABLE_SET_HOOKS
					const auto& rec = m_records.pRecords[compIdx];
					if GAIA_UNLIKELY (rec.pItem->comp_hooks.func_set != nullptr)
						rec.pItem->comp_hooks.func_set(*m_header.world, rec, *this);
#endif
				}

				return comp_ptr_mut(compIdx, row);
			}

			//! Finishes a raw write over a chunk range by updating versions, running set hooks once,
			//! and notifying `OnSet` observers after the callback completed.
			void finish_write(uint32_t compIdx, uint16_t from, uint16_t to) {
				GAIA_ASSERT(compIdx < m_header.cntEntities);
				if (from >= to)
					return;

				update_world_version(compIdx);

#if GAIA_ENABLE_SET_HOOKS
				const auto& rec = m_records.pRecords[compIdx];
				if GAIA_UNLIKELY (rec.pItem->comp_hooks.func_set != nullptr)
					rec.pItem->comp_hooks.func_set(*m_header.world, rec, *this);
#endif

				world_notify_on_set(*const_cast<World*>(m_header.world), m_records.pCompEntities[compIdx], *this, from, to);
			}

		private:
			//! Returns the value stored in the component \a T on \a row in the chunk.
			//! \warning It is expected the \a row is valid. Undefined behavior otherwise.
			//! \warning It is expected the component \a T is present. Undefined behavior otherwise.
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

			template <typename T>
			GAIA_NODISCARD decltype(auto) comp_inter_idx(uint16_t row, uint32_t compIdx) const {
				using U = typename actual_type_t<T>::Type;
				using RetValueType = decltype(view_raw<T>((const void*)nullptr, 1)[0]);

				GAIA_ASSERT(row < m_header.count);
				if constexpr (mem::is_soa_layout_v<U>)
					return view_raw<T>(comp_ptr(compIdx), capacity())[row];
				else if constexpr (sizeof(RetValueType) <= 8)
					return view_raw<T>(comp_ptr(compIdx, row), 1)[0];
				else
					return (const U&)view_raw<T>(comp_ptr(compIdx, row), 1)[0];
			}

			template <typename T, bool WorldVersionUpdateWanted>
			GAIA_NODISCARD decltype(auto) comp_mut_idx(uint16_t row, uint32_t compIdx) {
				using U = typename actual_type_t<T>::Type;

				GAIA_ASSERT(row < m_header.capacity);
				if constexpr (mem::is_soa_layout_v<U>)
					return view_mut_raw<T>(comp_ptr_mut_gen<WorldVersionUpdateWanted>(compIdx, 0), capacity())[row];
				else
					return view_mut_raw<T>(comp_ptr_mut_gen<WorldVersionUpdateWanted>(compIdx, row), 1)[0];
			}

		public:
			Chunk(const Chunk& chunk) = delete;
			Chunk(Chunk&& chunk) = delete;
			Chunk& operator=(const Chunk& chunk) = delete;
			Chunk& operator=(Chunk&& chunk) = delete;
			~Chunk() = default;

			//! Size in bytes of the chunk header area reserved before entity and component data.
			//! \return Header size in bytes.
			static constexpr uint16_t chunk_header_size() {
				const auto dataAreaOffset =
						// ChunkAllocator reserves the first few bytes for internal purposes
						MemoryBlockUsableOffset +
						// Chunk "header" area (before actual entity/component data starts)
						sizeof(ChunkHeader) + sizeof(ChunkRecords);
				static_assert(dataAreaOffset % MemoryBlockAlignment == 0);
				static_assert(dataAreaOffset < UINT16_MAX);
				return dataAreaOffset;
			}

			//! Total chunk allocation size for a given usable data size.
			//! \param dataSize Usable data area size in bytes.
			//! \return Header plus data size in bytes.
			static constexpr uint16_t chunk_total_bytes(uint16_t dataSize) {
				return chunk_header_size() + dataSize;
			}

			//! Usable data area size for a given total chunk allocation size.
			//! \param totalSize Total allocation size in bytes.
			//! \return Data area size in bytes.
			static constexpr uint16_t chunk_data_bytes(uint16_t totalSize) {
				return totalSize - chunk_header_size();
			}

			//! Returns the relative offset of m_data in Chunk
			//! \return Byte offset of the chunk data area relative to the Chunk start.
			static uintptr_t chunk_data_area_offset() {
				// Note, offsetof is implementation-defined and conditionally-supported since C++17.
				// Therefore, we instantiate the chunk and calculate the relative address ourselves.
				Chunk chunk;
				const auto chunk_offset = (uintptr_t)&chunk;
				const auto data_offset = (uintptr_t)&chunk.m_data[0];
				return data_offset - chunk_offset;
			}

			//! Allocates memory for a new chunk.
			//! \return Newly allocated chunk
			static Chunk* create(
					const World& wld, const ComponentCache& cc, //
					uint32_t chunkIndex, uint16_t capacity, uint8_t cntEntities, //
					uint16_t dataBytes, uint32_t& worldVersion,
					// data offsets
					const ChunkDataOffsets& offsets,
					// component entities
					const Entity* ids,
					// resolved component storage items
					const ComponentCacheItem* const* pItems,
					// component offsets
					const ChunkDataOffset* compOffs) {
				const auto totalBytes = chunk_total_bytes(dataBytes);
#if GAIA_ECS_CHUNK_ALLOCATOR
				auto* pChunk = (Chunk*)ChunkAllocator::get().alloc(totalBytes);
				(void)new (pChunk) Chunk(wld, cc, chunkIndex, capacity, worldVersion);
#else
				GAIA_ASSERT(totalBytes <= MaxMemoryBlockSize);
				const auto sizeType = mem_block_size_type(totalBytes);
				const auto allocSize = mem_block_size(sizeType);
				auto* pChunkMem = mem::AllocHelper::alloc<uint8_t>(allocSize);
				std::memset(pChunkMem, 0, allocSize);
				auto* pChunk = new (pChunkMem) Chunk(wld, cc, chunkIndex, capacity, worldVersion);
#endif

				pChunk->init((uint32_t)cntEntities, ids, pItems, offsets, compOffs);
				return pChunk;
			}

			//! Releases all memory allocated by \a pChunk.
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

			//! Serializes chunk contents: entity counts, lifespan state, entity ids and component data.
			//! \param s serializer to write to.
			void save(ser::serializer& s) const {
				s.save(m_header.count);
				if (m_header.count == 0)
					return;

				s.save(m_header.countEnabled);

				const uint16_t dead = m_header.dead;
				const uint16_t lifespanCountdown = m_header.lifespanCountdown;
				s.save(dead);
				s.save(lifespanCountdown);

				const auto cnt = (uint32_t)m_header.count;
				const auto cap = (uint32_t)m_header.capacity;

				// Store entity data
				{
					const auto* pData = m_records.pEntities;
					GAIA_FOR(cnt) s.save(pData[i]);
				}

				// Store component data
				{
					for (const auto& rec: comp_rec_view()) {
						// Skip the component if there's no size associated with it
						if (!component_uses_table_storage(rec.comp))
							continue;

						rec.pItem->save(s, rec.pData, 0, cnt, cap);
					}
				}
			}

			//! Deserializes chunk contents, restoring entity ids and component data.
			//! \param s serializer to read from.
			void load(ser::serializer& s) {
				uint16_t prevCount = m_header.count;
				s.load(m_header.count);
				if (m_header.count == 0)
					return;

				s.load(m_header.countEnabled);

				uint16_t dead = 0;
				uint16_t lifespanCountdown = 0;
				s.load(dead);
				s.load(lifespanCountdown);
				m_header.dead = dead != 0;
				m_header.lifespanCountdown = lifespanCountdown;

				const auto cnt = (uint32_t)m_header.count;
				const auto cap = (uint32_t)m_header.capacity;

				// Load entity data
				{
					GAIA_FOR(cnt) {
						Entity e;
						s.load(e);
						entity_view_mut()[i] = e;
					}
				}

				// Load component data. Call constructors first as necessary.
				call_ctors(prevCount, cnt);
				{
					for (const auto& rec: comp_rec_view()) {
						// Skip the component if there's no size associated with it
						if (!component_uses_table_storage(rec.comp))
							continue;

						rec.pItem->load(s, rec.pData, 0, cnt, cap);
					}
				}
			}

			//! Remove the last entity from a chunk.
			//! If as a result the chunk becomes empty it is scheduled for deletion.
			void remove_last_entity() {
				// Should never be called over an empty chunk
				GAIA_ASSERT(!empty());

#if GAIA_ASSERT_ENABLED
				// Invalidate the entity in chunk data
				entity_view_mut()[m_header.count - 1] = EntityBad;
#endif

				--m_header.count;
			}

			//! Updates the version numbers for this chunk.
			void update_versions() {
				::gaia::ecs::update_version(m_header.worldVersion);
				update_world_version();
				update_entity_order_version();
			}

			//! Returns a read-only entity or component view.
			//! \warning If \a T is a component it is expected it is present. Undefined behavior otherwise.
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

			//! Returns a read-only entity or component view.
			//! \tparam T Component or Entity.
			//! \return Read-only view over all chunk entities or the component column.
			template <typename T>
			GAIA_NODISCARD decltype(auto) view() const {
				return view<T>(0, m_header.count);
			}

			//! Returns a read-only view over raw bytes as typed data.
			//! \tparam T Component or Entity.
			//! \param ptr Raw data start.
			//! \param size Raw data size in bytes.
			//! \return Read-only view over the raw bytes.
			template <typename T>
			GAIA_NODISCARD decltype(auto) view_raw(const void* ptr, uint32_t size) const {
				using U = typename actual_type_t<T>::Type;
				return mem::auto_view_policy_get<U>{std::span{(const uint8_t*)ptr, size}};
			}

			//! Returns a mutable entity or component view.
			//! \warning If \a T is a component it is expected it is present. Undefined behavior otherwise.
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

			//! Returns a mutable entity or component view.
			//! \tparam T Component or Entity.
			//! \return Mutable view over all chunk entities or the component column.
			template <typename T>
			GAIA_NODISCARD decltype(auto) view_mut() {
				return view_mut<T>(0, m_header.count);
			}

			//! Returns a mutable view over raw bytes as typed data.
			//! \tparam T Component or Entity.
			//! \param ptr Raw data start.
			//! \param size Raw data size in bytes.
			//! \return Mutable view over the raw bytes.
			template <typename T>
			GAIA_NODISCARD decltype(auto) view_mut_raw(void* ptr, uint32_t size) const {
				using U = typename actual_type_t<T>::Type;
				static_assert(!std::is_same_v<U, Entity>, "Modifying chunk entities via view_mut is forbidden");

				return mem::auto_view_policy_set<U>{std::span{(uint8_t*)ptr, size}};
			}

			//! Returns a mutable component view.
			//! Doesn't update the world version when the access is acquired.
			//! \warning It is expected the component \a T is present. Undefined behavior otherwise.
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

			//! Returns a mutable view over raw bytes without query-version updates.
			//! \tparam T Component or Entity.
			//! \param ptr Raw data start.
			//! \param size Raw data size in bytes.
			//! \return Mutable view over the raw bytes.
			template <typename T>
			GAIA_NODISCARD decltype(auto) sview_mut_raw(void* ptr, uint32_t size) const {
				using U = typename actual_type_t<T>::Type;
				static_assert(!std::is_same_v<U, Entity>, "Modifying chunk entities via sview_mut is forbidden");

				return mem::auto_view_policy_set<U>{std::span{(uint8_t*)ptr, size}};
			}

			//! Returns a mutable entity or component view without query-version updates.
			//! \tparam T Component or Entity.
			//! \return Mutable view without query-version updates.
			template <typename T>
			GAIA_NODISCARD decltype(auto) sview_mut() {
				return sview_mut<T>(0, m_header.count);
			}

			//! Marks the component \a T as modified. Best used with sview to manually trigger
			//! an update at user's whim.
			//! If \a TriggerSetHooks is true, also triggers the component's set hooks.
			template <
					typename T
#if GAIA_ENABLE_HOOKS
					,
					bool TriggerSetHooks
#endif
					>
			GAIA_FORCEINLINE void modify() {
				static_assert(!std::is_same_v<core::raw_t<T>, Entity>, "mod can't be used to modify Entity");

				if constexpr (is_pair<T>::value) {
					using TT = typename T::type;
					using U = typename component_type_t<TT>::Type;
					static_assert(!std::is_empty_v<U>, "mut can't be used to modify tag components");

					const auto rel = m_header.cc->get<typename T::rel>().entity;
					const auto tgt = m_header.cc->get<typename T::tgt>().entity;
					const auto compIdx = comp_idx((Entity)Pair(rel, tgt));

					// Update version number if necessary so we know RW access was used on the chunk
					update_world_version(compIdx);

#if GAIA_ENABLE_SET_HOOKS
					if constexpr (TriggerSetHooks) {
						const auto& rec = m_records.pRecords[compIdx];
						if GAIA_UNLIKELY (rec.pItem->comp_hooks.func_set != nullptr)
							rec.pItem->comp_hooks.func_set(*m_header.world, rec, *this);
					}
#endif
				} else {
					using U = typename component_type_t<T>::Type;
					static_assert(!std::is_empty_v<U>, "mut can't be used to modify tag components");

					const auto comp = m_header.cc->get<T>().entity;
					const auto compIdx = comp_idx(comp);

					// Update version number if necessary so we know RW access was used on the chunk
					update_world_version(compIdx);

#if GAIA_ENABLE_SET_HOOKS
					if constexpr (TriggerSetHooks) {
						const auto& rec = m_records.pRecords[compIdx];
						if GAIA_UNLIKELY (rec.pItem->comp_hooks.func_set != nullptr)
							rec.pItem->comp_hooks.func_set(*m_header.world, rec, *this);
					}
#endif
				}
			}

			//! Returns either a mutable or immutable entity/component view based on the requested type.
			//! Value and const types are considered immutable. Anything else is mutable.
			//! \warning If \a T is a component it is expected to be present. Undefined behavior otherwise.
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

			//! Returns an automatically-typed mutable view over the chunk.
			//! \tparam T Component or Entity.
			//! \return Automatically-typed mutable view over the chunk.
			template <typename T>
			GAIA_NODISCARD decltype(auto) view_auto() {
				return view_auto<T>(0, m_header.count);
			}

			//! Returns either a mutable or immutable entity/component view based on the requested type.
			//! Value and const types are considered immutable. Anything else is mutable.
			//! Doesn't update the world version when read-write access is acquired.
			//! \warning If \a T is a component it is expected to be present. Undefined behavior otherwise.
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

			//! Returns an automatically-typed mutable view without query-version updates.
			//! \tparam T Component or Entity.
			//! \return Automatically-typed mutable view without query-version updates.
			template <typename T>
			GAIA_NODISCARD decltype(auto) sview_auto() {
				return sview_auto<T>(0, m_header.count);
			}

			//! Span over the entities stored in this chunk.
			//! \return Entity span covering all stored entities.
			GAIA_NODISCARD EntitySpan entity_view() const {
				return {(const Entity*)m_records.pEntities, m_header.count};
			}

			//! Owning world mutable reference.
			//! \return Owning world.
			GAIA_NODISCARD World& world() {
				return *const_cast<World*>(m_header.world);
			}

			//! Owning world const reference.
			//! \return Owning world.
			GAIA_NODISCARD const World& world() const {
				return *m_header.world;
			}

			//! Span over the component and entity identifiers held by this chunk.
			//! \return Identifier span.
			GAIA_NODISCARD EntitySpan ids_view() const {
				return {(const Entity*)m_records.pCompEntities, m_header.cntEntities};
			}

			//! Span over the component records describing each chunk column.
			//! \return Component record span.
			GAIA_NODISCARD std::span<const ComponentRecord> comp_rec_view() const {
				return {m_records.pRecords, m_header.cntEntities};
			}

			//! Mutable pointer to the start of a component column.
			//! \param compIdx Component column index.
			//! \return Pointer to the column data start.
			GAIA_NODISCARD uint8_t* comp_ptr_mut(uint32_t compIdx) {
				const auto& rec = m_records.pRecords[compIdx];
				return rec.pData;
			}

			//! Mutable pointer to a component element within a column.
			//! \param compIdx Component column index.
			//! \param offset Entity row offset.
			//! \return Pointer to the element at \a offset.
			GAIA_NODISCARD uint8_t* comp_ptr_mut(uint32_t compIdx, uint32_t offset) {
				const auto& rec = m_records.pRecords[compIdx];
				return rec.pData + ((uintptr_t)rec.comp.size() * offset);
			}

			//! Const pointer to the start of a component column.
			//! \param compIdx Component column index.
			//! \return Pointer to the column data start.
			GAIA_NODISCARD const uint8_t* comp_ptr(uint32_t compIdx) const {
				const auto& rec = m_records.pRecords[compIdx];
				return rec.pData;
			}

			//! Const pointer to a component element within a column.
			//! \param compIdx Component column index.
			//! \param offset Entity row offset.
			//! \return Pointer to the element at \a offset.
			GAIA_NODISCARD const uint8_t* comp_ptr(uint32_t compIdx, uint32_t offset) const {
				const auto& rec = m_records.pRecords[compIdx];
				return rec.pData + ((uintptr_t)rec.comp.size() * offset);
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

			//! Copies all data associated with \a srcEntity into \a dstEntity.
			//! \param srcEntity Source entity
			//! \param dstEntity Destination entity
			//! \param recs Entity containers
			static void copy_entity_data(Entity srcEntity, Entity dstEntity, EntityContainers& recs) {
				GAIA_PROF_SCOPE(Chunk::copy_entity_data);

				auto& srcEntityContainer = recs[srcEntity];
				auto* pSrcChunk = srcEntityContainer.pChunk;

				auto& dstEntityContainer = recs[dstEntity];
				auto* pDstChunk = dstEntityContainer.pChunk;

				GAIA_ASSERT(srcEntityContainer.pArchetype == dstEntityContainer.pArchetype);

				auto srcRecs = pSrcChunk->comp_rec_view();

				// Copy component data from reference entity to our new entity.
				GAIA_FOR(pSrcChunk->m_header.cntEntities) {
					const auto& rec = srcRecs[i];
					if (!component_uses_table_storage(rec.comp))
						continue;

					const auto* pSrc = (const void*)pSrcChunk->comp_ptr_mut(i);
					auto* pDst = (void*)pDstChunk->comp_ptr_mut(i);
					rec.pItem->copy(
							pDst, pSrc, dstEntityContainer.row, srcEntityContainer.row, pDstChunk->capacity(), pSrcChunk->capacity());
				}
			}

			//! Copies all data associated with \a srcRow into \a dstCount consecutive rows in the same-archetype chunk.
			//! \param pSrcChunk Source chunk
			//! \param srcRow Row in source chunk
			//! \param pDstChunk Destination chunk
			//! \param dstRow First destination row in destination chunk
			//! \param dstCount Number of destination rows to copy into
			static void copy_entity_data_n_same_chunk(
					Chunk* pSrcChunk, uint32_t srcRow, Chunk* pDstChunk, uint32_t dstRow, uint32_t dstCount) {
				GAIA_PROF_SCOPE(Chunk::copy_entity_data_n_same_chunk);

				GAIA_ASSERT(pSrcChunk != nullptr);
				GAIA_ASSERT(pDstChunk != nullptr);
				GAIA_ASSERT(srcRow < pSrcChunk->size());
				GAIA_ASSERT(dstRow + dstCount <= pDstChunk->size());
				GAIA_ASSERT(pSrcChunk->ids_view().size() == pDstChunk->ids_view().size());

				auto srcRecs = pSrcChunk->comp_rec_view();

				// Copy component data from the reference entity to all newly allocated rows.
				GAIA_FOR(pSrcChunk->m_header.cntEntities) {
					const auto& rec = srcRecs[i];
					if (!component_uses_table_storage(rec.comp))
						continue;

					const auto* pSrc = (const void*)pSrcChunk->comp_ptr(i);
					GAIA_FOR_(dstCount, rowOffset) {
						auto* pDst = (void*)pDstChunk->comp_ptr_mut(i);
						rec.pItem->copy(pDst, pSrc, dstRow + rowOffset, srcRow, pDstChunk->capacity(), pSrcChunk->capacity());
					}
				}
			}

			//! Copies all data associated with \a srcRow into \a dstCount consecutive rows in a foreign chunk.
			//! \param pSrcChunk Source chunk
			//! \param srcRow Row in source chunk
			//! \param pDstChunk Destination chunk
			//! \param dstRow First destination row in destination chunk
			//! \param dstCount Number of destination rows to copy into
			static void copy_foreign_entity_data_n(
					Chunk* pSrcChunk, uint32_t srcRow, Chunk* pDstChunk, uint32_t dstRow, uint32_t dstCount) {
				GAIA_PROF_SCOPE(Chunk::copy_foreign_entity_data_n);

				GAIA_ASSERT(pSrcChunk != nullptr);
				GAIA_ASSERT(pDstChunk != nullptr);
				GAIA_ASSERT(srcRow < pSrcChunk->size());
				GAIA_ASSERT(dstRow + dstCount <= pDstChunk->size());

				auto srcIds = pSrcChunk->ids_view();
				auto dstIds = pDstChunk->ids_view();
				auto dstRecs = pDstChunk->comp_rec_view();

				uint32_t i = 0;
				uint32_t j = 0;
				while (i < pSrcChunk->m_header.cntEntities && j < pDstChunk->m_header.cntEntities) {
					const auto oldId = srcIds[i];
					const auto newId = dstIds[j];

					if (oldId == newId) {
						const auto& rec = dstRecs[j];
						if (component_uses_table_storage(rec.comp)) {
							auto* pSrc = (void*)pSrcChunk->comp_ptr_mut(i);
							auto* pDst = (void*)pDstChunk->comp_ptr_mut(j);
							GAIA_FOR_(dstCount, rowOffset) {
								rec.pItem->ctor_copy(
										pDst, pSrc, dstRow + rowOffset, srcRow, pDstChunk->capacity(), pSrcChunk->capacity());
							}
						}

						++i;
						++j;
					} else if (SortComponentCond{}.operator()(oldId, newId)) {
						++i;
					} else {
						const auto& rec = dstRecs[j];
						if (rec.pItem != nullptr && rec.pItem->func_ctor != nullptr && //
								component_uses_table_storage(rec.comp)) {
							auto* pDst = (void*)pDstChunk->comp_ptr_mut(j, dstRow);
							rec.pItem->func_ctor(pDst, dstCount);
						}

						++j;
					}
				}

				for (; j < pDstChunk->m_header.cntEntities; ++j) {
					const auto& rec = dstRecs[j];
					if (rec.pItem != nullptr && rec.pItem->func_ctor != nullptr && //
							component_uses_table_storage(rec.comp)) {
						auto* pDst = (void*)pDstChunk->comp_ptr_mut(j, dstRow);
						rec.pItem->func_ctor(pDst, dstCount);
					}
				}
			}

			//! Moves all data associated with \a entity into the chunk so that it is stored at the row \a row.
			//! \param entity Entity to move
			//! \param row Entity's row within its chunk
			//! \param recs Entity containers
			void move_entity_data(Entity entity, uint16_t row, EntityContainers& recs) {
				GAIA_PROF_SCOPE(Chunk::move_entity_data);

				auto& ec = recs[entity];
				auto* pSrcChunk = ec.pChunk;
				auto srcRecs = pSrcChunk->comp_rec_view();

				// Copy component data from reference entity to our new entity.
				GAIA_FOR(pSrcChunk->m_header.cntEntities) {
					const auto& rec = srcRecs[i];
					if (!component_uses_table_storage(rec.comp))
						continue;

					auto* pSrc = (void*)pSrcChunk->comp_ptr_mut(i);
					auto* pDst = (void*)comp_ptr_mut(i);
					rec.pItem->ctor_move(pDst, pSrc, row, ec.row, capacity(), pSrcChunk->capacity());
				}
			}

			//! Copies all data associated with \a entity into the chunk so that it is stored at the row \a row.
			//! \param pSrcChunk Source chunk
			//! \param srcRow Row in source chunk
			//! \param pDstChunk Destination chunk
			//! \param dstRow Row in destination chunk
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
				{
					uint32_t i = 0;
					uint32_t j = 0;
					while (i < pSrcChunk->m_header.cntEntities && j < pDstChunk->m_header.cntEntities) {
						const auto oldId = srcIds[i];
						const auto newId = dstIds[j];

						if (oldId == newId) {
							const auto& rec = dstRecs[j];
							if (component_uses_table_storage(rec.comp)) {
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
							if (rec.pItem != nullptr && rec.pItem->func_ctor != nullptr && //
									component_uses_table_storage(rec.comp)) {
								auto* pDst = (void*)pDstChunk->comp_ptr_mut(j, dstRow);
								rec.pItem->func_ctor(pDst, 1);
							}

							++j;
						}
					}

					// Initialize remaining destination columns.
					for (; j < pDstChunk->m_header.cntEntities; ++j) {
						const auto& rec = dstRecs[j];
						if (rec.pItem != nullptr && rec.pItem->func_ctor != nullptr && //
								component_uses_table_storage(rec.comp)) {
							auto* pDst = (void*)pDstChunk->comp_ptr_mut(j, dstRow);
							rec.pItem->func_ctor(pDst, 1);
						}
					}
				}
			}

			//! Moves all data associated with \a entity into the chunk so that it is stored at the row \a row.
			//! \param pSrcChunk Source chunk
			//! \param srcRow Row in source chunk
			//! \param pDstChunk Destination chunk
			//! \param dstRow Row in destination chunk
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
				{
					uint32_t i = 0;
					uint32_t j = 0;
					while (i < pSrcChunk->m_header.cntEntities && j < pDstChunk->m_header.cntEntities) {
						const auto oldId = srcIds[i];
						const auto newId = dstIds[j];

						if (oldId == newId) {
							const auto& rec = dstRecs[j];
							if (component_uses_table_storage(rec.comp)) {
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
							if (rec.pItem != nullptr && rec.pItem->func_ctor != nullptr && //
									component_uses_table_storage(rec.comp)) {
								auto* pDst = (void*)pDstChunk->comp_ptr_mut(j, dstRow);
								rec.pItem->func_ctor(pDst, 1);
							}

							++j;
						}
					}

					// Initialize remaining destination columns.
					for (; j < pDstChunk->m_header.cntEntities; ++j) {
						const auto& rec = dstRecs[j];
						if (rec.pItem != nullptr && rec.pItem->func_ctor != nullptr && //
								component_uses_table_storage(rec.comp)) {
							auto* pDst = (void*)pDstChunk->comp_ptr_mut(j, dstRow);
							rec.pItem->func_ctor(pDst, 1);
						}
					}
				}
			}

			//! Tries to remove the entity at \a row.
			//! Removal is done via swapping with last entity in chunk.
			//! Upon removal, all associated data is also removed.
			//! If the entity at the given row already is the last chunk entity, it is removed directly.
			//! \param row Row within a chunk
			//! \param recs Entity containers
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
					GAIA_FOR(m_header.cntEntities) {
						const auto& rec = recView[i];
						if (!component_uses_table_storage(rec.comp))
							continue;

						auto* pSrc = (void*)comp_ptr_mut(i);
						rec.pItem->move(pSrc, pSrc, rowA, rowB, capacity(), capacity());

						pSrc = (void*)comp_ptr_mut(i, rowB);
						rec.pItem->dtor(pSrc);
					}

					// Entity has been replaced with the last one in our chunk. Update its container record.
					ecB.row = rowA;
					ecB.pEntity = &ev[rowA];
				} else if (m_header.hasAnyCustomDtor) {
					// This is the last entity in the chunk so simply destroy its data
					auto recView = comp_rec_view();
					GAIA_FOR(m_header.cntEntities) {
						const auto& rec = recView[i];
						if (!component_uses_table_storage(rec.comp))
							continue;

						auto* pSrc = (void*)comp_ptr_mut(i, rowA);
						rec.pItem->dtor(pSrc);
					}
				}
			}

			//! Tries to remove the entity at row \a row.
			//! Removal is done via swapping with last entity in chunk.
			//! Upon removal, all associated data is also removed.
			//! If the entity at the given row already is the last chunk entity, it is removed directly.
			//! \param row Row within a chunk
			//! \param recs Entity containers
			void remove_entity(uint16_t row, EntityContainers& recs) {
				if GAIA_UNLIKELY (m_header.count == 0)
					return;

				GAIA_PROF_SCOPE(Chunk::remove_entity);

				if (enabled(row)) {
					// Entity was previously enabled. Swap with the last entity
					remove_entity_inter(row, recs);
					// At this point the last entity is no longer valid so remove it
					remove_last_entity();
					--m_header.countEnabled;
				} else {
					// Entity was previously disabled. Swap with the last disabled entity
					const uint16_t pivot = size_disabled() - 1;
					swap_chunk_entities(row, pivot, recs);
					// Once swapped, try to swap with the last (enabled) entity in the chunk.
					remove_entity_inter(pivot, recs);
					--m_header.rowFirstEnabledEntity;
					// At this point the last entity is no longer valid so remove it
					remove_last_entity();
				}
			}

			//! Tries to swap the entity at row \a rowA with the one at the row \a rowB.
			//! When swapping, all data associated with the two entities is swapped as well.
			//! If \a rowA equals \a rowB no swapping is performed.
			//! \param rowA Row of the entityA within chunk
			//! \param rowB Row of the entityB within chunk
			//! \param[out] recs Entity container records
			//! \warning "rowA" must he smaller or equal to "rowB"
			void swap_chunk_entities(uint16_t rowA, uint16_t rowB, EntityContainers& recs) {
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
				GAIA_FOR(m_header.cntEntities) {
					const auto& rec = recView[i];
					if (!component_uses_table_storage(rec.comp))
						continue;

					GAIA_ASSERT(rec.pData == comp_ptr_mut(i));
					rec.pItem->swap(rec.pData, rec.pData, rowA, rowB, capacity(), capacity());
				}

				// Update indices in entity container.
				ecA.row = rowB;
				ecB.row = rowA;
				ecA.pEntity = &ev[rowB];
				ecB.pEntity = &ev[rowA];
			}

			//! Tries to swap \a entityA with \a entityB.
			//! When swapping, all data associated with the two entities is swapped as well.
			//! If \a entityA and \a entityB are the same entity no swapping is performed.
			//! \param world Parent world
			//! \param entityA First entity
			//! \param entityB Second entity
			static void swap_chunk_entities(World& world, Entity entityA, Entity entityB) {
				// Don't swap if the two entities are the same
				if GAIA_UNLIKELY (entityA == entityB)
					return;

				GAIA_PROF_SCOPE(Chunk::swap_chunk_entities);

				auto& ecA = fetch_mut(world, entityA);
				auto& ecB = fetch_mut(world, entityB);

				// Make sure the two entities are in the same archetype
				GAIA_ASSERT(ecA.pArchetype == ecB.pArchetype);
				GAIA_ASSERT(ecA.pArchetype == ecB.pArchetype);

				auto* pChunkA = ecA.pChunk;
				auto* pChunkB = ecB.pChunk;

				// Swap entities in the entity data part
				pChunkA->entity_view_mut()[ecA.row] = entityB;
				pChunkB->entity_view_mut()[ecB.row] = entityA;

				// Swap component data
				auto recViewA = pChunkA->comp_rec_view();
				GAIA_FOR(pChunkA->m_header.cntEntities) {
					const auto& recA = recViewA[i];
					if (!component_uses_table_storage(recA.comp))
						continue;

					auto* pDataA = pChunkA->comp_rec_view()[i].pData;
					auto* pDataB = pChunkB->comp_rec_view()[i].pData;
					recA.pItem->swap(
							// Data pointers
							pDataA, pDataB,
							// Rows
							ecA.row, ecB.row,
							// Chunk capacities
							pChunkA->capacity(), pChunkA->capacity() //
					);
				}

				// Update indices and chunks in entity container.
				core::swap(ecA.row, ecB.row);
				core::swap(ecA.pChunk, ecB.pChunk);
				ecA.pEntity = &ecA.pChunk->entity_view()[ecA.row];
				ecB.pEntity = &ecB.pChunk->entity_view()[ecB.row];
			}

			//! Enables or disables the entity on a given row in the chunk.
			//! \param row Row of the entity within chunk
			//! \param enableEntity Enables or disables the entity
			//! \param recs Entity container records
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
					recs[entity].data.dis = 0;
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
					recs[entity].data.dis = 1;
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

			//! Invokes the registered constructor for one component instance.
			//! \param entIdx Entity row to construct.
			//! \param compIdx Component column index.
			//! \param item Component cache item carrying the constructor.
			void call_ctor(uint32_t entIdx, uint32_t compIdx, const ComponentCacheItem& item) {
				if (item.func_ctor == nullptr || !component_uses_table_storage(item.comp))
					return;

				GAIA_PROF_SCOPE(Chunk::call_ctor);

				auto* pSrc = (void*)comp_ptr_mut(compIdx, entIdx);
				item.func_ctor(pSrc, 1);
			}

			//! Invokes registered constructors for component columns at a row range.
			//! \param entIdx First entity row to construct.
			//! \param entCnt Number of entity rows to construct.
			void call_ctors(uint32_t entIdx, uint32_t entCnt) {
				if (!m_header.hasAnyCustomCtor)
					return;

				GAIA_PROF_SCOPE(Chunk::call_ctors);

				auto recs = comp_rec_view();
				GAIA_FOR(m_header.cntEntities) {
					const auto& rec = recs[i];
					if (!component_uses_table_storage(rec.comp))
						continue;

					const auto* pItem = rec.pItem;
					if (pItem == nullptr || pItem->func_ctor == nullptr)
						continue;

					auto* pSrc = (void*)comp_ptr_mut(i, entIdx);
					pItem->func_ctor(pSrc, entCnt);
				}
			}

			//! Invokes registered destructors for all custom component instances before release.
			void call_all_dtors() {
				if (!m_header.hasAnyCustomDtor)
					return;

				GAIA_PROF_SCOPE(Chunk::call_all_dtors);

				auto recs = comp_rec_view();
				const auto recs_cnt = recs.size();
				GAIA_FOR(recs_cnt) {
					const auto& rec = recs[i];
					if (!component_uses_table_storage(rec.comp))
						continue;

					const auto* pItem = rec.pItem;
					if (pItem == nullptr || pItem->func_dtor == nullptr)
						continue;

					auto* pSrc = (void*)comp_ptr_mut(i, 0);
					const auto cnt = m_header.count;
					pItem->func_dtor(pSrc, cnt);
				}
			}

			//----------------------------------------------------------------------
			// Check component presence
			//----------------------------------------------------------------------

			//! Checks if a component/entity \a entity is present in the chunk.
			//! \param entity Entity
			//! \return True if found. False otherwise.
			GAIA_NODISCARD bool has(Entity entity) const {
				auto ids = ids_view();
				return core::has(ids, entity);
			}

			//! Checks if component \a T is present in the chunk.
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

			//! Sets the value of component \a T on \a row in the chunk.
			//! \tparam T Component or pair
			//! \param row Row of entity in the chunk
			//! \warning It is expected the component \a T is present. Undefined behavior otherwise.
			//! \return Mutable reference to the component value.
			template <typename T>
			decltype(auto) set(uint16_t row) {
				verify_comp<T>();

				// Update the world version
				::gaia::ecs::update_version(m_header.worldVersion);

				GAIA_ASSERT(row < m_header.capacity);
				world_notify_on_set(*const_cast<World*>(m_header.world), comp_entity<T>(), *this, row, (uint16_t)(row + 1));
				return view_mut<T>()[row];
			}

			//! Sets the value of a component using a pre-resolved component column.
			//! \tparam T Component or pair
			//! \param row Row of entity in the chunk
			//! \param compIdx Pre-resolved component column index
			//! \return Mutable reference to the component value.
			template <typename T>
			decltype(auto) set_idx(uint16_t row, uint32_t compIdx) {
				verify_comp<T>();

				// Update the world version
				::gaia::ecs::update_version(m_header.worldVersion);

				world_notify_on_set(
						*const_cast<World*>(m_header.world), m_records.pCompEntities[compIdx], *this, row, (uint16_t)(row + 1));
				return comp_mut_idx<T, true>(row, compIdx);
			}

			//! Sets the value of component \a type at the position \a row in the chunk.
			//! \param row Row of entity in the chunk
			//! \param type Component/entity/pair
			//! \warning It is expected the component \a T is present. Undefined behavior otherwise.
			//! \return Mutable reference to the component value.
			template <typename T>
			decltype(auto) set(uint16_t row, Entity type) {
				const uint32_t compIdx = comp_idx(type);
				GAIA_ASSERT(m_records.pRecords[compIdx].pItem != nullptr);

				// Update the world version
				::gaia::ecs::update_version(m_header.worldVersion);

				GAIA_ASSERT(row < m_header.capacity);
				world_notify_on_set(*const_cast<World*>(m_header.world), type, *this, row, (uint16_t)(row + 1));
				return comp_mut_idx<T, true>(row, compIdx);
			}

			//! Sets the value of component \a T on \a row in the chunk.
			//! \tparam T Component or pair
			//! \param row Row of entity in the chunk
			//! \warning It is expected the component \a T is present. Undefined behavior otherwise.
			//! \warning World version is not updated so Query filters will not be able to catch this change.
			//! \return Mutable reference to the component value.
			template <typename T>
			decltype(auto) sset(uint16_t row) {
				GAIA_ASSERT(row < m_header.capacity);
				return sview_mut<T>()[row];
			}

			//! Sets the value of a component using a pre-resolved component column.
			//! \warning World version is not updated so Query filters will not be able to catch this change.
			//! \return Mutable reference to the component value.
			template <typename T>
			decltype(auto) sset_idx(uint16_t row, uint32_t compIdx) {
				verify_comp<T>();

				return comp_mut_idx<T, false>(row, compIdx);
			}

			//! Sets the value of component \a type at the position \a row in the chunk.
			//! \tparam T Component or pair
			//! \param row Row of entity in the chunk
			//! \param type Component/entity/pair
			//! \warning It is expected the component \a T is present. Undefined behavior otherwise.
			//! \warning World version is not updated so Query filters will not be able to catch this change.
			//! \return Mutable reference to the component value.
			template <typename T>
			decltype(auto) sset(uint16_t row, Entity type) {
				static_assert(core::is_raw_v<T>);

				const uint32_t compIdx = comp_idx(type);
				GAIA_ASSERT(m_records.pRecords[compIdx].pItem != nullptr);

				GAIA_ASSERT(row < m_header.capacity);
				return comp_mut_idx<T, false>(row, compIdx);
			}

			//----------------------------------------------------------------------
			// Read component data
			//----------------------------------------------------------------------

			//! Returns the value stored in component \a T on \a row in the chunk.
			//! \tparam T Component or pair
			//! \param row Row of entity in the chunk
			//! \warning It is expected the \a row is valid. Undefined behavior otherwise.
			//! \warning It is expected the component \a T is present. Undefined behavior otherwise.
			//! \return Value stored in the component.
			template <typename T>
			GAIA_NODISCARD decltype(auto) get(uint16_t row) const {
				return comp_inter<T>(row);
			}

			//! Returns the value stored in component \a T using a pre-resolved component column.
			//! \tparam T Component or pair
			//! \param row Row of entity in the chunk
			//! \param compIdx Pre-resolved component column index
			//! \return Value stored in the component.
			template <typename T>
			GAIA_NODISCARD decltype(auto) get_idx(uint16_t row, uint32_t compIdx) const {
				return comp_inter_idx<T>(row, compIdx);
			}

			//! Returns the value stored in component \a type on \a row in the chunk.
			//! \tparam T Component or pair
			//! \param row Row of entity in the chunk
			//! \param type Component/entity/pair
			//! \warning It is expected the component is present. Undefined behavior otherwise.
			//! \return Value stored in the component.
			template <typename T>
			GAIA_NODISCARD decltype(auto) get(uint16_t row, Entity type) const {
				GAIA_ASSERT(row < m_header.count);
				const uint32_t compIdx = comp_idx(type);
				GAIA_ASSERT(m_records.pRecords[compIdx].pItem != nullptr);
				return comp_inter_idx<T>(row, compIdx);
			}

			//! Component entity for the chunk archetype contents.
			//! \tparam T Component or pair.
			//! \return Component identifier of the archetype term.
			template <typename T>
			GAIA_NODISCARD Entity comp_entity() const {
				if constexpr (is_pair<T>::value) {
					const auto rel = m_header.cc->get<typename T::rel>().entity;
					const auto tgt = m_header.cc->get<typename T::tgt>().entity;
					return (Entity)Pair(rel, tgt);
				} else {
					return m_header.cc->get<T>().entity;
				}
			}

			//! Returns the internal index of a component based on the provided \a entity.
			//! \param entity Component
			//! \return Component index if the component was found. -1 otherwise.
			//! \warning The component id must be present in the array.
			GAIA_NODISCARD uint32_t comp_idx(Entity entity) const {
				return ecs::comp_idx<ChunkHeader::MAX_COMPONENTS>(m_records.pCompEntities, entity);
			}

			//! Returns the internal index of a component based on the provided \a entity.
			//! \param entity Component
			//! \param offset Component offset
			//! \return Component index if the component was found. -1 otherwise.
			//! \warning The component id must be present in the array.
			GAIA_NODISCARD uint32_t comp_idx(Entity entity, uint32_t offset) const {
				return ecs::comp_idx({m_records.pCompEntities + offset, m_header.count - offset}, entity);
			}

			//----------------------------------------------------------------------

			//! Sets the index of this chunk in its archetype's storage
			void set_idx(uint32_t value) {
				m_header.index = value;
			}

			//! Returns the index of this chunk in its archetype's storage.
			//! \return Index of this chunk in its archetype's storage.
			GAIA_NODISCARD uint32_t idx() const {
				return m_header.index;
			}

			//! Checks is this chunk has any enabled entities
			//! \return True when at least one entity is enabled.
			GAIA_NODISCARD bool has_enabled_entities() const {
				return m_header.has_enabled_entities();
			}

			//! Checks is this chunk has any disabled entities
			//! \return True when at least one entity is disabled.
			GAIA_NODISCARD bool has_disabled_entities() const {
				return m_header.has_disabled_entities();
			}

			//! Checks is this chunk is dying
			//! \return True when the chunk is in its dying lifespan countdown.
			GAIA_NODISCARD bool dying() const {
				return m_header.lifespanCountdown > 0;
			}

			//! Returns true when the chunk is currently queued for deferred deletion.
			//! \return True when the chunk is queued for deferred deletion.
			GAIA_NODISCARD bool queued_for_deletion() const {
				return m_header.deleteQueueIndex != BadIndex;
			}

			//! Returns the index inside World's deferred chunk-delete queue.
			//! \return Deferred-delete queue index, BadIndex when not queued.
			GAIA_NODISCARD uint32_t delete_queue_index() const {
				return m_header.deleteQueueIndex;
			}

			//! Stores the index inside World's deferred chunk-delete queue.
			void delete_queue_index(uint32_t idx) {
				m_header.deleteQueueIndex = idx;
			}

			//! Clears the deferred chunk-delete queue index.
			void clear_delete_queue_index() {
				m_header.deleteQueueIndex = BadIndex;
			}

			//! Marks the chunk as dead (ready to delete)
			void die() {
				m_header.dead = 1;
			}

			//! Checks is this chunk is dead (ready to delete)
			//! \return True when the chunk is dead and ready to be deleted.
			GAIA_NODISCARD bool dead() const {
				return m_header.dead == 1;
			}

			//! Starts the process of dying (not yet ready to delete, can be revived)
			void start_dying() {
				GAIA_ASSERT(!dead());
				GAIA_ASSERT(!queued_for_deletion());
				m_header.lifespanCountdown = ChunkHeader::MAX_CHUNK_LIFESPAN;
			}

			//! Makes a dying chunk alive again
			void revive() {
				GAIA_ASSERT(!dead());
				m_header.lifespanCountdown = 0;
				clear_delete_queue_index();
			}

			//! Updates internal lifespan
			//! \return True if there is some lifespan rowA, false otherwise.
			bool progress_death() {
				GAIA_ASSERT(dying());
				--m_header.lifespanCountdown;
				return dying();
			}

			//! Checks is the full capacity of the has has been reached
			//! \return True when the chunk capacity is exhausted.
			GAIA_NODISCARD bool full() const {
				return m_header.count >= m_header.capacity;
			}

			//! Checks is the chunk is semi-full.
			//! \return True when the chunk is below the semi-full threshold.
			GAIA_NODISCARD bool is_semi() const {
				// We want the chunk filled to at least 75% before considering it semi-full
				constexpr float Threshold = 0.75f;
				return ((float)m_header.count / (float)m_header.capacity) < Threshold;
			}

			//! Returns the total number of entities in the chunk (both enabled and disabled)
			//! \return Total number of entities, enabled and disabled.
			GAIA_NODISCARD uint16_t size() const {
				return m_header.count;
			}

			//! Checks is there are any entities in the chunk
			//! \return True when the chunk holds no entities.
			GAIA_NODISCARD bool empty() const {
				return m_header.count == 0;
			}

			//! Return the number of entities in the chunk which are enabled
			//! \return Number of enabled entities.
			GAIA_NODISCARD uint16_t size_enabled() const {
				return m_header.countEnabled;
			}

			//! Return the number of entities in the chunk which are enabled
			//! \return Number of disabled entities.
			GAIA_NODISCARD uint16_t size_disabled() const {
				return (uint16_t)m_header.rowFirstEnabledEntity;
			}

			//! Returns the number of entities in the chunk
			//! \return Maximum number of entities the chunk can hold.
			GAIA_NODISCARD uint16_t capacity() const {
				return m_header.capacity;
			}

			//! Returns true if the provided version is newer than the one stored internally.
			//! Use when checking if there was a movement in data in the world. E.g. if an entity
			//! was added, removed or moved in its archetype.
			//! \param requiredVersion Version to compare against.
			//! \return True when the stored chunk version is newer.
			GAIA_NODISCARD bool changed(uint32_t requiredVersion) const {
				const auto* versions = m_records.pVersions;
				const auto changeVersion = versions[0];
				return ::gaia::ecs::version_changed(changeVersion, requiredVersion);
			}

			//! Returns true if the provided version is newer than the one stored internally
			//! \param requiredVersion Version to compare against.
			//! \param compIdx Component column index.
			//! \return True when the stored component version is newer.
			GAIA_NODISCARD bool changed(uint32_t requiredVersion, uint32_t compIdx) const {
				const auto* versions = m_records.pVersions;
				// Do +1 because index 0 is reserved for the entity version number.
				const auto changeVersion = versions[compIdx + 1];
				return ::gaia::ecs::version_changed(changeVersion, requiredVersion);
			}

			//! Returns true if entity order changed since \a requiredVersion.
			//! This is narrower than changed(requiredVersion): unrelated component writes do not affect it.
			//! \param requiredVersion Version to compare against.
			//! \return True when the entity order changed at or after \a requiredVersion.
			GAIA_NODISCARD bool entity_order_changed(uint32_t requiredVersion) const {
				return ::gaia::ecs::version_changed(m_header.entityOrderVersion, requiredVersion);
			}

			//! Update the version of a component at the index \param compIdx
			GAIA_FORCEINLINE void update_world_version(uint32_t compIdx) {
				auto versions = comp_version_view_mut();
				// Automatically treat the entity as changed.
				versions[0] = m_header.worldVersion;
				// Do +1 because index 0 is reserved for the entity version number.
				versions[compIdx + 1] = m_header.worldVersion;
				// Sorted queries keyed by this component can invalidate their cached order immediately.
				world_invalidate_sorted_queries_for_entity(
						*const_cast<World*>(m_header.world), m_records.pCompEntities[compIdx]);
			}

			//! Updates the entity-order version after rows were added, removed, or reordered.
			GAIA_FORCEINLINE void update_entity_order_version() {
				m_header.entityOrderVersion = m_header.worldVersion;
				// Row-order changes invalidate cached sorted slices regardless of sort key.
				world_invalidate_sorted_queries(*const_cast<World*>(m_header.world));
			}

			//! Update the version of all components
			GAIA_FORCEINLINE void update_world_version() {
				// Edit the version pointer directly. The first elements is always the entity version.
				// This area of memory is always present.
				auto* versions = m_records.pVersions;
				// We update the version of the entity only. If this one changes,
				// all other components are considered changed as well.
				versions[0] = m_header.worldVersion;
			}

			//! Update the version of all components on chunk init
			GAIA_FORCEINLINE void update_world_version_init() {
				auto* versions = m_records.pVersions;
				// We update the version of the entity and all components to match the world version.
				versions[0] = m_header.worldVersion;
				GAIA_FOR(m_header.cntEntities) versions[1 + i] = m_header.worldVersion;
				m_header.entityOrderVersion = m_header.worldVersion;
			}

			//! Logs a diagnostic line describing the chunk capacity and lifespan state.
			void diag() const {
				GAIA_LOG_N(
						"  Chunk #%04u, entities:%u/%u, lifespanCountdown:%u", m_header.index, m_header.count, m_header.capacity,
						m_header.lifespanCountdown);
			}
		};
	} // namespace ecs
} // namespace gaia
