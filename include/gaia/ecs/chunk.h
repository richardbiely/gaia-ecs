#pragma once
#include "../config/config.h"

#include <cstdint>
#include <cstring>
#include <tuple>
#include <type_traits>
#include <utility>

#include "../cnt/sarray_ext.h"
#include "../core/utility.h"
#include "../mem/mem_alloc.h"
#include "chunk_allocator.h"
#include "chunk_header.h"
#include "common.h"
#include "component.h"
#include "component_cache.h"
#include "entity_container.h"
#include "id.h"

namespace gaia {
	namespace ecs {
		class Chunk final {
		public:
			// TODO: Make this private
			//! Chunk header
			ChunkHeader m_header;

		private:
			//! Pointer to where the chunk data starts.
			//! Data layed out as following:
			//!			1) Entities (identifiers)
			//!			2) Entities (data)
			//! Note, root archetypes store only entities, therefore it is fully occupied with entities.
			uint8_t m_data[1];

			GAIA_MSVC_WARNING_PUSH()
			GAIA_MSVC_WARNING_DISABLE(26495)

			// Hidden default constructor. Only use to calculate the relative offset of m_data
			Chunk() = default;

			Chunk(
					const ComponentCache& cc, uint32_t chunkIndex, uint16_t capacity, uint8_t genEntities, uint16_t st,
					uint32_t& worldVersion): //
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
					uint32_t cntEntities, const Component* comps, const ChunkDataOffsets& headerOffsets,
					const ChunkDataOffset* compOffs) {
				m_header.cntEntities = (uint8_t)cntEntities;

				// Cache component records
				GAIA_FOR(cntEntities) {
					m_header.recs[i].comp = comps[i];
					m_header.recs[i].pData = &data(compOffs[i]);
				}

				m_header.pEntities = (Entity*)&data(headerOffsets.firstByte_EntityData);
			}

			GAIA_MSVC_WARNING_POP()

		public:
			Chunk(const Chunk& chunk) = delete;
			Chunk(Chunk&& chunk) = delete;
			Chunk& operator=(const Chunk& chunk) = delete;
			Chunk& operator=(Chunk&& chunk) = delete;
			~Chunk() = default;

			//! Allocates memory for a new chunk.
			//! \param chunkIndex Index of this chunk within the parent archetype
			//! \return Newly allocated chunk
			static Chunk* create(
					const ComponentCache& cc, uint32_t chunkIndex, uint16_t capacity, uint8_t cntEntities, uint8_t genEntities,
					uint16_t dataBytes, uint32_t& worldVersion,
					// data offsets
					const ChunkDataOffsets& offsets,
					// component
					const Component* comps,
					// component offsets
					const ChunkDataOffset* compOffs) {
				const auto totalBytes = Chunk::chunk_total_bytes(dataBytes);
				const auto sizeType = mem_block_size_type(totalBytes);
#if GAIA_ECS_CHUNK_ALLOCATOR
				auto* pChunk = (Chunk*)ChunkAllocator::get().alloc(totalBytes);
				(void)new (pChunk) Chunk(cc, chunkIndex, capacity, genEntities, sizeType, worldVersion);
#else
				GAIA_ASSERT(totalBytes <= MaxMemoryBlockSize);
				const auto allocSize = mem_block_size(sizeType);
				auto* pChunkMem = mem::AllocHelper::alloc<uint8_t>(allocSize);
				std::memset(pChunkMem, 0, allocSize);
				auto* pChunk = new (pChunkMem) Chunk(cc, chunkIndex, capacity, genEntities, sizeType, worldVersion);
#endif

				pChunk->init((uint32_t)cntEntities, comps, offsets, compOffs);

				return pChunk;
			}

			//! Releases all memory allocated by \param pChunk.
			//! \param pChunk Chunk which we want to destroy
			static void free(Chunk* pChunk) {
				GAIA_ASSERT(pChunk != nullptr);

				pChunk->~Chunk();
#if GAIA_ECS_CHUNK_ALLOCATOR
				ChunkAllocator::get().free(pChunk);
#else
				mem::AllocHelper::free((uint8_t*)pChunk);
#endif
			}

			static constexpr uint16_t chunk_header_size() {
				const auto dataAreaOffset =
						// ChunkAllocator reserves the first few bytes for internal purposes
						MemoryBlockUsableOffset +
						// Chunk "header" area (before actual entity/component data starts)
						sizeof(ChunkHeader);
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
				--m_header.countEnabled;
			}

			//! Updates the version numbers for this chunk.
			void update_versions() {
				update_version(m_header.worldVersion);
				update_world_version();
			}

			GAIA_NODISCARD static std::span<const ComponentVersion> comp_version_view(const ChunkHeader& header) {
				return {(const ComponentVersion*)header.versions, header.cntEntities};
			}

			GAIA_NODISCARD static std::span<ComponentVersion> comp_version_view_mut(const ChunkHeader& header) {
				return {(ComponentVersion*)header.versions, header.cntEntities};
			}

			GAIA_NODISCARD std::span<const ComponentVersion> comp_version_view() const {
				return comp_version_view(m_header);
			}

			GAIA_NODISCARD std::span<ComponentVersion> comp_version_view_mut() {
				return comp_version_view_mut(m_header);
			}

			GAIA_NODISCARD std::span<Entity> entity_view_mut() {
				return {m_header.pEntities, m_header.count};
			}

			GAIA_NODISCARD EntitySpan entity_view() const {
				return {(const Entity*)m_header.pEntities, m_header.count};
			}

			GAIA_NODISCARD std::span<const ComponentRecord> comp_rec_view() const {
				return {m_header.recs, m_header.cntEntities};
			}

			//! Make \param entity a part of the chunk at the version of the world
			//! \return Row of entity in the chunk,
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

			//! Updates internal lifespan
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
