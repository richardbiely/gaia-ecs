#pragma once
#include "../config/config.h"

#include "../cnt/darray.h"
#include "../cnt/ilist.h"
#include "../cnt/set.h"
#include "../config/profiler.h"
#include "../core/hashing_policy.h"
#include "../core/utility.h"
#include "../mem/mem_utils.h"
#include "archetype.h"
#include "archetype_common.h"
#include "component.h"
#include "component_cache.h"
#include "id.h"
#include "query_common.h"
#include "vm.h"

namespace gaia {
	namespace ecs {
		struct Entity;
		class World;

		using EntityToArchetypeMap = cnt::map<EntityLookupKey, ArchetypeDArray>;
		struct ArchetypeCacheData {
			GroupId groupId = 0;
			uint8_t indices[ChunkHeader::MAX_COMPONENTS];
		};

		struct QueryInfoCreationCtx {
			QueryCtx* pQueryCtx;
			const EntityToArchetypeMap* pEntityToArchetypeMap;
		};

		class QueryInfo: public cnt::ilist_item {
		public:
			//! Query matching result
			enum class MatchArchetypeQueryRet : uint8_t { Fail, Ok, Skip };

		private:
			struct Instruction {
				Entity id;
				QueryOpKind op;
			};

			struct GroupData {
				GroupId groupId;
				uint32_t idxFirst;
				uint32_t idxLast;
				bool needsSorting;
			};

			uint32_t m_refs = 0;

			//! Query context
			QueryCtx m_ctx;
			//! Virtual machine
			vm::VirtualMachine m_vm;

			//! Use to make sure only unique archetypes are inserted into the cache
			//! TODO: Get rid of the set by changing the way the caching works.
			cnt::set<Archetype*> m_archetypeSet;
			//! Cached array of archetypes matching the query
			ArchetypeDArray m_archetypeCache;
			//! Cached array of query-specific data
			cnt::darray<ArchetypeCacheData> m_archetypeCacheData;
			//! Group data used by cache
			cnt::darray<GroupData> m_archetypeGroupData;

			//! Id of the last archetype in the world we checked
			ArchetypeId m_lastArchetypeId{};
			//! Version of the world for which the query has been called most recently
			uint32_t m_worldVersion{};

			enum QueryCmdType : uint8_t { ALL, ANY, NOT };

			template <typename TType>
			GAIA_NODISCARD bool has_inter([[maybe_unused]] QueryOpKind op, bool isReadWrite) const {
				using T = core::raw_t<TType>;

				if constexpr (std::is_same_v<T, Entity>) {
					// Entities are read-only.
					GAIA_ASSERT(!isReadWrite);
					// Skip Entity input args. Entities are always there.
					return true;
				} else {
					Entity id;

					if constexpr (is_pair<T>::value) {
						const auto rel = m_ctx.cc->get<typename T::rel>().entity;
						const auto tgt = m_ctx.cc->get<typename T::tgt>().entity;
						id = (Entity)Pair(rel, tgt);
					} else {
						id = m_ctx.cc->get<T>().entity;
					}

					const auto& data = m_ctx.data;
					const auto& terms = data.terms;
					const auto compIdx = comp_idx<MAX_ITEMS_IN_QUERY>(terms.data(), id, EntityBad);

					if (op != data.terms[compIdx].op)
						return false;

					// Read-write mask must match
					const uint32_t maskRW = (uint32_t)data.readWriteMask & (1U << compIdx);
					const uint32_t maskXX = (uint32_t)isReadWrite << compIdx;
					return maskRW == maskXX;
				}
			}

			template <typename T>
			GAIA_NODISCARD bool has_inter(QueryOpKind op) const {
				// static_assert(is_raw_v<<T>, "has() must be used with raw types");
				constexpr bool isReadWrite = core::is_mut_v<T>;
				return has_inter<T>(op, isReadWrite);
			}

		public:
			void add_ref() {
				++m_refs;
				GAIA_ASSERT(m_refs != 0);
			}

			void dec_ref() {
				GAIA_ASSERT(m_refs > 0);
				--m_refs;
			}

			uint32_t refs() const {
				return m_refs;
			}

			void init(World* world) {
				m_ctx.w = world;
			}

			void reset() {
				m_archetypeSet = {};
				m_archetypeCache = {};
				m_archetypeCacheData = {};
				m_archetypeCacheData = {};
				m_lastArchetypeId = 0;

				m_ctx.data.lastMatchedArchetypeIdx_All = {};
				m_ctx.data.lastMatchedArchetypeIdx_Any = {};
				m_ctx.data.lastMatchedArchetypeIdx_Not = {};
			}

			GAIA_NODISCARD static QueryInfo
			create(QueryId id, QueryCtx&& ctx, const EntityToArchetypeMap& entityToArchetypeMap) {
				// Make sure query items are sorted
				sort(ctx);

				QueryInfo info;
				info.idx = id;
				info.gen = 0;

				info.m_ctx = GAIA_MOV(ctx);
				info.m_ctx.q.handle = {id, 0};

				// Compile the query
				info.compile(entityToArchetypeMap);

				return info;
			}

			GAIA_NODISCARD static QueryInfo create(uint32_t idx, uint32_t gen, void* pCtx) {
				auto* pCreationCtx = (QueryInfoCreationCtx*)pCtx;
				auto& queryCtx = (QueryCtx&)*pCreationCtx->pQueryCtx;
				auto& entityToArchetypeMap = (EntityToArchetypeMap&)*pCreationCtx->pEntityToArchetypeMap;

				// Make sure query items are sorted
				sort(queryCtx);

				QueryInfo info;
				info.idx = idx;
				info.gen = gen;

				info.m_ctx = GAIA_MOV(queryCtx);
				info.m_ctx.q.handle = {idx, gen};

				// Compile the query
				info.compile(entityToArchetypeMap);

				return info;
			}

			GAIA_NODISCARD static QueryHandle handle(const QueryInfo& info) {
				return QueryHandle(info.idx, info.gen);
			}

			//! Compile the query terms into a form we can easily process
			void compile(const EntityToArchetypeMap& entityToArchetypeMap) {
				GAIA_PROF_SCOPE(queryinfo::compile);

				// Compile the opcodes
				m_vm.compile(entityToArchetypeMap, m_ctx);
			}

			void set_world_version(uint32_t version) {
				m_worldVersion = version;
			}

			GAIA_NODISCARD uint32_t world_version() const {
				return m_worldVersion;
			}

			GAIA_NODISCARD bool operator==(const QueryCtx& other) const {
				return m_ctx == other;
			}

			GAIA_NODISCARD bool operator!=(const QueryCtx& other) const {
				return m_ctx != other;
			}

			//! Tries to match the query against archetypes in \param entityToArchetypeMap.
			//! This is necessary so we do not iterate all chunks over and over again when running queries.
			//! \warning Not thread safe. No two threads can call this at the same time.
			void match(
					// entity -> archetypes mapping
					const EntityToArchetypeMap& entityToArchetypeMap,
					// all archetypes in the world
					const ArchetypeDArray& allArchetypes,
					// last matched archetype id
					ArchetypeId archetypeLastId) {

				// Global temporary buffers for collecting archetypes that match a query.
				static cnt::set<Archetype*> s_tmpArchetypeMatchesSet;
				static ArchetypeDArray s_tmpArchetypeMatchesArr;

				struct CleanUpTmpArchetypeMatches {
					CleanUpTmpArchetypeMatches() = default;
					CleanUpTmpArchetypeMatches(const CleanUpTmpArchetypeMatches&) = delete;
					CleanUpTmpArchetypeMatches(CleanUpTmpArchetypeMatches&&) = delete;
					CleanUpTmpArchetypeMatches& operator=(const CleanUpTmpArchetypeMatches&) = delete;
					CleanUpTmpArchetypeMatches& operator=(CleanUpTmpArchetypeMatches&&) = delete;

					~CleanUpTmpArchetypeMatches() {
						// When the scope ends, we clear the arrays.
						// Note, no memory is released. Allocated capacity remains unchanged
						// because we do not want to kill time with allocating memory all the time.
						s_tmpArchetypeMatchesSet.clear();
						s_tmpArchetypeMatchesArr.clear();
					}
				} autoCleanup;

				// Skip if nothing has been compiled.
				if (!m_vm.is_compiled())
					return;

				// Skip if no new archetype appeared
				GAIA_ASSERT(archetypeLastId >= m_lastArchetypeId);
				if (m_lastArchetypeId == archetypeLastId)
					return;
				m_lastArchetypeId = archetypeLastId;

				GAIA_PROF_SCOPE(queryinfo::match);

				auto& data = m_ctx.data;

				vm::MatchingCtx ctx{};
				ctx.pWorld = world();
				ctx.pAllArchetypes = &allArchetypes;
				ctx.pEntityToArchetypeMap = &entityToArchetypeMap;
				ctx.pMatchesArr = &s_tmpArchetypeMatchesArr;
				ctx.pMatchesSet = &s_tmpArchetypeMatchesSet;
				ctx.pLastMatchedArchetypeIdx_All = &data.lastMatchedArchetypeIdx_All;
				ctx.pLastMatchedArchetypeIdx_Any = &data.lastMatchedArchetypeIdx_Any;
				ctx.pLastMatchedArchetypeIdx_Not = &data.lastMatchedArchetypeIdx_Not;
				ctx.as_mask_0 = data.as_mask_0;
				ctx.as_mask_1 = data.as_mask_1;
				m_vm.exec(ctx);

				// Write found matches to cache
				for (auto* pArchetype: *ctx.pMatchesArr)
					add_archetype_to_cache(pArchetype);

				// Sort cache groups if necessary
				sort_cache_groups();
			}

			void sort_cache_groups() {
				if ((m_ctx.data.flags & QueryCtx::QueryFlags::SortGroups) == 0)
					return;
				m_ctx.data.flags ^= QueryCtx::QueryFlags::SortGroups;

				struct sort_cond {
					bool operator()(const ArchetypeCacheData& a, const ArchetypeCacheData& b) const {
						return a.groupId <= b.groupId;
					}
				};

				// Archetypes in cache are ordered by groupId. Adding a new archetype
				// possibly means rearranging the existing ones.
				// 2 2 3 3 3 3 4 4 4 [2]
				// -->
				// 2 2 [2] 3 3 3 3 4 4 4
				core::sort(m_archetypeCacheData, sort_cond{}, [&](uint32_t left, uint32_t right) {
					auto* pTmpArchetype = m_archetypeCache[left];
					m_archetypeCache[left] = m_archetypeCache[right];
					m_archetypeCache[right] = pTmpArchetype;

					auto tmp = m_archetypeCacheData[left];
					m_archetypeCacheData[left] = m_archetypeCacheData[right];
					m_archetypeCacheData[right] = tmp;
				});
			}

			ArchetypeCacheData create_cache_data(Archetype* pArchetype) {
				ArchetypeCacheData cacheData;
				const auto& queryIds = ids();
				GAIA_EACH(queryIds) {
					const auto idxBeforeRemapping = m_ctx.data.remapping[i];
					const auto queryId = queryIds[idxBeforeRemapping];
					// compIdx can be -1. We are fine with it because the user should never ask for something
					// that is not present on the archetype. If they do, they made a mistake.
					const auto compIdx = core::get_index_unsafe(pArchetype->ids_view(), queryId);
					GAIA_ASSERT(compIdx != BadIndex);

					cacheData.indices[i] = (uint8_t)compIdx;
				}
				return cacheData;
			}

			void add_archetype_to_cache_no_grouping(Archetype* pArchetype) {
				GAIA_PROF_SCOPE(add_cache_ng);

				if (m_archetypeSet.contains(pArchetype))
					return;

				m_archetypeSet.emplace(pArchetype);
				m_archetypeCache.push_back(pArchetype);
				m_archetypeCacheData.push_back(create_cache_data(pArchetype));
			}

			void add_archetype_to_cache_w_grouping(Archetype* pArchetype) {
				GAIA_PROF_SCOPE(add_cache_wg);

				if (m_archetypeSet.contains(pArchetype))
					return;

				const GroupId groupId = m_ctx.data.groupByFunc(*m_ctx.w, *pArchetype, m_ctx.data.groupBy);

				ArchetypeCacheData cacheData = create_cache_data(pArchetype);
				cacheData.groupId = groupId;

				if (m_archetypeGroupData.empty()) {
					m_archetypeGroupData.push_back({groupId, 0, 0, false});
				} else {
					GAIA_EACH(m_archetypeGroupData) {
						if (groupId < m_archetypeGroupData[i].groupId) {
							// Insert the new group before one with a lower groupId.
							// 2 3 5 10 20 25 [7]<-new group
							// -->
							// 2 3 5 [7] 10 20 25
							m_archetypeGroupData.insert(
									m_archetypeGroupData.begin() + i,
									{groupId, m_archetypeGroupData[i].idxFirst, m_archetypeGroupData[i].idxFirst, false});
							const auto lastGrpIdx = m_archetypeGroupData.size();

							// Update ranges
							for (uint32_t j = i + 1; j < lastGrpIdx; ++j) {
								++m_archetypeGroupData[j].idxFirst;
								++m_archetypeGroupData[j].idxLast;
							}

							// Resort groups
							m_ctx.data.flags |= QueryCtx::QueryFlags::SortGroups;
							goto groupWasFound;
						} else if (m_archetypeGroupData[i].groupId == groupId) {
							const auto lastGrpIdx = m_archetypeGroupData.size();
							++m_archetypeGroupData[i].idxLast;

							// Update ranges
							for (uint32_t j = i + 1; j < lastGrpIdx; ++j) {
								++m_archetypeGroupData[j].idxFirst;
								++m_archetypeGroupData[j].idxLast;
								m_ctx.data.flags |= QueryCtx::QueryFlags::SortGroups;
							}

							goto groupWasFound;
						}
					}

					{
						// We have a new group
						const auto groupsCnt = m_archetypeGroupData.size();
						if (groupsCnt == 0) {
							// No groups exist yet, the range is {0 .. 0}
							m_archetypeGroupData.push_back( //
									{groupId, 0, 0, false});
						} else {
							const auto& groupPrev = m_archetypeGroupData[groupsCnt - 1];
							GAIA_ASSERT(groupPrev.idxLast + 1 == m_archetypeCache.size());
							// The new group starts where the old one ends
							m_archetypeGroupData.push_back(
									{groupId, //
									 groupPrev.idxLast + 1, //
									 groupPrev.idxLast + 1, //
									 false});
						}
					}

				groupWasFound:;
				}

				m_archetypeSet.emplace(pArchetype);
				m_archetypeCache.push_back(pArchetype);
				m_archetypeCacheData.push_back(GAIA_MOV(cacheData));
			}

			void add_archetype_to_cache(Archetype* pArchetype) {
				if (m_ctx.data.groupBy != EntityBad)
					add_archetype_to_cache_w_grouping(pArchetype);
				else
					add_archetype_to_cache_no_grouping(pArchetype);
			}

			bool del_archetype_from_cache(Archetype* pArchetype) {
				const auto it = m_archetypeSet.find(pArchetype);
				if (it == m_archetypeSet.end())
					return false;
				m_archetypeSet.erase(it);

				const auto idx = core::get_index_unsafe(m_archetypeCache, pArchetype);
				GAIA_ASSERT(idx != BadIndex);

				core::swap_erase(m_archetypeCache, idx);
				core::swap_erase(m_archetypeCacheData, idx);

				// Update the group data if possible
				if (m_ctx.data.groupBy != EntityBad) {
					const auto groupId = m_ctx.data.groupByFunc(*m_ctx.w, *pArchetype, m_ctx.data.groupBy);
					const auto grpIdx = core::get_index_if_unsafe(m_archetypeGroupData, [&](const GroupData& group) {
						return group.groupId == groupId;
					});
					GAIA_ASSERT(grpIdx != BadIndex);

					auto& currGrp = m_archetypeGroupData[idx];

					// Update ranges
					const auto lastGrpIdx = m_archetypeGroupData.size();
					for (uint32_t j = grpIdx + 1; j < lastGrpIdx; ++j) {
						--m_archetypeGroupData[j].idxFirst;
						--m_archetypeGroupData[j].idxLast;
					}

					// Handle the current group. If it's about to be left empty, delete it.
					if (currGrp.idxLast - currGrp.idxFirst > 0)
						--currGrp.idxLast;
					else
						m_archetypeGroupData.erase(m_archetypeGroupData.begin() + grpIdx);
				}

				return true;
			}

			GAIA_NODISCARD World* world() {
				return const_cast<World*>(m_ctx.w);
			}
			GAIA_NODISCARD const World* world() const {
				return m_ctx.w;
			}

			GAIA_NODISCARD QuerySerBuffer& ser_buffer() {
				return m_ctx.q.ser_buffer(world());
			}
			void ser_buffer_reset() {
				m_ctx.q.ser_buffer_reset(world());
			}

			GAIA_NODISCARD QueryCtx& ctx() {
				return m_ctx;
			}
			GAIA_NODISCARD const QueryCtx& ctx() const {
				return m_ctx;
			}

			GAIA_NODISCARD const QueryCtx::Data& data() const {
				return m_ctx.data;
			}

			GAIA_NODISCARD const QueryEntityArray& ids() const {
				return m_ctx.data.ids;
			}

			GAIA_NODISCARD const QueryEntityArray& filters() const {
				return m_ctx.data.changed;
			}

			GAIA_NODISCARD bool has_filters() const {
				return !m_ctx.data.changed.empty();
			}

			template <typename... T>
			bool has_any() const {
				return (has_inter<T>(QueryOpKind::Any) || ...);
			}

			template <typename... T>
			bool has_all() const {
				return (has_inter<T>(QueryOpKind::All) && ...);
			}

			template <typename... T>
			bool has_no() const {
				return (!has_inter<T>(QueryOpKind::Not) && ...);
			}

			//! Removes an archetype from cache
			//! \param pArchetype Archetype to remove
			void remove(Archetype* pArchetype) {
				GAIA_PROF_SCOPE(queryinfo::remove);

				if (!del_archetype_from_cache(pArchetype))
					return;

				// An archetype was removed from the world so the last matching archetype index needs to be
				// lowered by one for every component context.
				auto clearMatches = [](QueryArchetypeCacheIndexMap& matches) {
					for (auto& pair: matches) {
						auto& lastMatchedArchetypeIdx = pair.second;
						if (lastMatchedArchetypeIdx > 0)
							--lastMatchedArchetypeIdx;
					}
				};
				clearMatches(m_ctx.data.lastMatchedArchetypeIdx_All);
				clearMatches(m_ctx.data.lastMatchedArchetypeIdx_Any);
			}

			//! Returns a view of indices mapping for component entities in a given archetype
			std::span<const uint8_t> indices_mapping_view(uint32_t idx) const {
				const auto& data = m_archetypeCacheData[idx];
				return {(const uint8_t*)&data.indices[0], ChunkHeader::MAX_COMPONENTS};
			}

			GAIA_NODISCARD ArchetypeDArray::iterator begin() {
				return m_archetypeCache.begin();
			}

			GAIA_NODISCARD ArchetypeDArray::iterator begin() const {
				return m_archetypeCache.begin();
			}

			GAIA_NODISCARD ArchetypeDArray::iterator end() {
				return m_archetypeCache.end();
			}

			GAIA_NODISCARD ArchetypeDArray::iterator end() const {
				return m_archetypeCache.end();
			}

			GAIA_NODISCARD std::span<Archetype*> cache_archetype_view() const {
				return std::span{m_archetypeCache.data(), m_archetypeCache.size()};
			}
			GAIA_NODISCARD std::span<const ArchetypeCacheData> cache_data_view() const {
				return std::span{m_archetypeCacheData.data(), m_archetypeCacheData.size()};
			}
			GAIA_NODISCARD std::span<const GroupData> group_data_view() const {
				return std::span{m_archetypeGroupData.data(), m_archetypeGroupData.size()};
			}
		};
	} // namespace ecs
} // namespace gaia
