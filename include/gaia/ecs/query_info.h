#pragma once
#include "../config/config.h"

#include "../cnt/darray.h"
#include "../cnt/sarray_ext.h"
#include "../cnt/set.h"
#include "../config/profiler.h"
#include "../core/hashing_policy.h"
#include "../core/utility.h"
#include "archetype.h"
#include "archetype_common.h"
#include "component.h"
#include "component_cache.h"
#include "component_utils.h"
#include "id.h"
#include "query_common.h"

namespace gaia {
	namespace ecs {
		struct Entity;

		using ComponentIdToArchetypeMap = cnt::map<EntityLookupKey, ArchetypeList>;

		class QueryInfo {
		public:
			//! Query matching result
			enum class MatchArchetypeQueryRet : uint8_t { Fail, Ok, Skip };

		private:
			//! Lookup context
			QueryCtx m_lookupCtx;
			//! List of archetypes matching the query
			ArchetypeList m_archetypeCache;
			//! Id of the last archetype in the world we checked
			ArchetypeId m_lastArchetypeId{};
			//! Version of the world for which the query has been called most recently
			uint32_t m_worldVersion{};

			template <typename T>
			bool has_inter([[maybe_unused]] QueryOp op, bool isReadWrite) const {
				if constexpr (std::is_same_v<T, Entity>) {
					// Entities are read-only.
					GAIA_ASSERT(!isReadWrite);
					// Skip Entity input args. Entities are always there.
					return true;
				} else {
					const auto& data = m_lookupCtx.data;
					const auto& ids = data.ids;

					const auto id = m_lookupCtx.cc->get<T>().entity;
					const auto compIdx = ecs::comp_idx<MAX_ITEMS_IN_QUERY>(ids.data(), id);

					if (op != data.pairs[compIdx].op)
						return false;

					// Read-write mask must match
					const uint32_t maskRW = (uint32_t)data.readWriteMask & (1U << compIdx);
					const uint32_t maskXX = (uint32_t)isReadWrite << compIdx;
					return maskRW == maskXX;
				}
			}

			template <typename T>
			bool has_inter(QueryOp op) const {
				// static_assert(is_raw_v<<T>, "has() must be used with raw types");

				using CT = component_type_t<T>;
				using FT = typename CT::TypeFull;
				constexpr bool isReadWrite = core::is_mut_v<T>;

				return has_inter<FT>(op, isReadWrite);
			}

			//! Tries to match entity ids in \param queryIds with those in \param archetypeIds given
			//! the comparison function \param func.
			//! \return True if there is a match, false otherwise.
			template <typename Func>
			GAIA_NODISCARD bool match_inter(const Chunk::EntityArray& archetypeIds, EntitySpan queryIds, Func func) const {
				// Arrays are sorted so we can do linear intersection lookup
				uint32_t i = 0;
				uint32_t j = 0;
				while (i < archetypeIds.size() && j < queryIds.size()) {
					const auto idInArchetype = archetypeIds[i];
					const auto idInQuery = queryIds[j];

					if (func(idInArchetype, idInQuery))
						return true;

					if (SortComponentCond{}.operator()(idInArchetype, idInQuery)) {
						++i;
						continue;
					}

					++j;
				}

				return false;
			}

			//! Tries to match entity ids in \param queryIds with those in \param archetypeIds given
			//! the comparison function \param func.
			//! \return True on the first match, false otherwise.
			GAIA_NODISCARD bool match_one(const Chunk::EntityArray& archetypeIds, EntitySpan queryIds) const {
				return match_inter(archetypeIds, queryIds, [&](Entity idArchetype, Entity idQuery) {
					// TODO: Comparison inside match_inter is slow. Do something about it.
					//       Ideally we want to do "idQuery == idArchetype" here.
					if (idQuery.pair()) {
						// all(Pair<X, All>):
						//   X, AAA
						//   X, BBB
						//   ...
						//   X, ZZZ
						if (idQuery.gen() == GAIA_ID(All).id())
							return idQuery.id() == idArchetype.id();

						// all(Pair<All, X>):
						//   AAA, X
						//   BBB, X
						//   ...
						//   ZZZ, X
						if (idQuery.id() == GAIA_ID(All).id())
							return idQuery.gen() == idArchetype.gen();

						// all(Pair<All, All>) aka "any pair"
						if (idQuery == Pair(GAIA_ID(All), GAIA_ID(All)))
							return true;
					}

					return idQuery == idArchetype;
				});
			}

			//! Tries to match entity ids in \param queryIds with those in \param archetypeIds given
			//! the comparison function \param func.
			//! \return True if all ids match, false otherwise.
			GAIA_NODISCARD bool match_all(const Chunk::EntityArray& archetypeIds, EntitySpan queryIds) const {
				uint32_t matches = 0;
				return match_inter(archetypeIds, queryIds, [&](Entity idArchetype, Entity idQuery) {
					// TODO: Comparison inside match_inter is slow. Do something about it.
					//       Ideally we want to do "idQuery == idArchetype" here.
					if (idQuery.pair()) {
						// all(Pair<X, All>):
						//   X, AAA
						//   X, BBB
						//   ...
						//   X, ZZZ
						if (idQuery.gen() == GAIA_ID(All).id())
							return idQuery.id() == idArchetype.id() && (++matches == (uint32_t)queryIds.size());

						// all(Pair<All, X>):
						//   AAA, X
						//   BBB, X
						//   ...
						//   ZZZ, X
						if (idQuery.id() == GAIA_ID(All).id())
							return idQuery.gen() == idArchetype.gen() && (++matches == (uint32_t)queryIds.size());

						// all(Pair<All, All>) aka "any pair"
						if (idQuery == Pair(GAIA_ID(All), GAIA_ID(All)))
							return ++matches == (uint32_t)queryIds.size();
					}

					return idQuery == idArchetype && (++matches == (uint32_t)queryIds.size());
				});
			}

		public:
			GAIA_NODISCARD static QueryInfo create(QueryId id, QueryCtx&& ctx) {
				// Make sure query items are sorted
				sort(ctx);

				QueryInfo info;
				info.m_lookupCtx = GAIA_MOV(ctx);
				info.m_lookupCtx.queryId = id;
				return info;
			}

			void set_world_version(uint32_t version) {
				m_worldVersion = version;
			}

			GAIA_NODISCARD uint32_t world_version() const {
				return m_worldVersion;
			}

			GAIA_NODISCARD bool operator==(const QueryCtx& other) const {
				return m_lookupCtx == other;
			}

			GAIA_NODISCARD bool operator!=(const QueryCtx& other) const {
				return !operator==(other);
			}

			void match(
					uint32_t firstArchetypeIdx,
					// matched archetypes
					cnt::set<Archetype*>& matchedArchetypes,
					// list of archetypes associated with a given id/entity
					const ArchetypeList& archetypes, EntitySpan ops_ids_not, EntitySpan ops_ids_any, EntitySpan ops_ids_all) {
				for (uint32_t j = firstArchetypeIdx; j < archetypes.size(); ++j) {
					auto* pArchetype = archetypes[j];
					if (pArchetype == nullptr || matchedArchetypes.contains(pArchetype))
						continue;

					// Eliminate archetypes not matching the NOT rule
					if (!ops_ids_not.empty() && match_one(pArchetype->entities(), ops_ids_not))
						continue;
					// Eliminate archetypes not matching the ANY rule
					if (!ops_ids_any.empty() && !match_one(pArchetype->entities(), ops_ids_any))
						continue;
					// Eliminate archetypes not matching the ALL rule
					if (!ops_ids_all.empty() && !match_all(pArchetype->entities(), ops_ids_all))
						continue;

					matchedArchetypes.emplace(archetypes[j]);
				}
			}

			//! Tries to match the query against archetypes in \param componentToArchetypeMap.
			//! This is necessary so we do not iterate all chunks over and over again when running queries.
			void match(
					// component -> archetypes mapping
					const ComponentIdToArchetypeMap& componentToArchetypeMap,
					// last matched archetype id
					ArchetypeId archetypeLastId) {
				static cnt::set<Archetype*> s_tmpArchetypeMatches;

				struct CleanUpTmpArchetypeMatches {
					~CleanUpTmpArchetypeMatches() {
						s_tmpArchetypeMatches.clear();
					}
				} autoCleanup;

				// Skip if no new archetype appeared
				GAIA_ASSERT(archetypeLastId >= m_lastArchetypeId);
				if (m_lastArchetypeId == archetypeLastId)
					return;
				m_lastArchetypeId = archetypeLastId;

				GAIA_PROF_SCOPE(queryinfo::match);

				auto& data = m_lookupCtx.data;
				const auto& ids = data.ids;
				if (ids.empty())
					return;

				cnt::sarr_ext<const ArchetypeList*, MAX_ITEMS_IN_QUERY> cache;
				uint32_t jj = 0;

				EntitySpan ops_ids{ids.data(), ids.size()};
				EntitySpan ops_ids_all = ops_ids.subspan(0, data.firstAny);
				EntitySpan ops_ids_any = ops_ids.subspan(data.firstAny, data.firstNot - data.firstAny);
				EntitySpan ops_ids_not = ops_ids.subspan(data.firstNot);

				if (!ops_ids_all.empty()) {
					GAIA_EACH(ops_ids_all) {
						// Check if any archetype is associated with the entity id.
						// All ids must be registered in the world.
						const auto it = componentToArchetypeMap.find(EntityLookupKey(ops_ids_all[i]));
						if (it == componentToArchetypeMap.end() || it->second.empty())
							return;

						cache.push_back(&it->second);
					}
					GAIA_EACH(ops_ids_all) {
						const auto lastMatchedIdx = data.lastMatchedArchetypeIdx[jj];
						const auto& archetypes = *cache[i];
						data.lastMatchedArchetypeIdx[jj++] = archetypes.size();
						match(lastMatchedIdx, s_tmpArchetypeMatches, archetypes, ops_ids_not, ops_ids_any, ops_ids_all);
					}

					if (!ops_ids_any.empty()) {
						cache.clear();
						GAIA_EACH(ops_ids_any) {
							// Check if any archetype is associated with the entity id.
							// All ids must be registered in the world.
							const auto it = componentToArchetypeMap.find(EntityLookupKey(ops_ids_any[i]));
							if (it == componentToArchetypeMap.end() || it->second.empty()) {
								cache.push_back(nullptr);
								continue;
							}

							cache.push_back(&it->second);
						}
						if (cache.empty())
							return;
					}
					GAIA_EACH(ops_ids_any) {
						const auto lastMatchedIdx = data.lastMatchedArchetypeIdx[jj];
						const auto& archetypes = *cache[i];
						data.lastMatchedArchetypeIdx[jj++] = archetypes.size();
						match(lastMatchedIdx, s_tmpArchetypeMatches, archetypes, ops_ids_not, ops_ids_any, ops_ids_all);
					}

					// Make sure there is no match with NOT items.
					// It is possible they were not a part of previous archetypes.
					if (ops_ids_not.empty()) {
						for (auto* pArchetype: s_tmpArchetypeMatches) {
							if (match_one(pArchetype->entities(), ops_ids_not))
								continue;
							m_archetypeCache.push_back(pArchetype);
						}
					} else {
						for (auto* pArchetype: s_tmpArchetypeMatches) {
							m_archetypeCache.push_back(pArchetype);
						}
					}
				} else if (!ops_ids_any.empty()) {
					GAIA_EACH(ops_ids_any) {
						// Check if any archetype is associated with the entity id.
						// All ids must be registered in the world.
						const auto it = componentToArchetypeMap.find(EntityLookupKey(ops_ids_any[i]));
						if (it == componentToArchetypeMap.end() || it->second.empty()) {
							cache.push_back(nullptr);
							continue;
						}

						cache.push_back(&it->second);
					}
					if (cache.empty())
						return;
					GAIA_EACH(ops_ids_any) {
						const auto lastMatchedIdx = data.lastMatchedArchetypeIdx[jj];
						const auto& archetypes = *cache[i];
						data.lastMatchedArchetypeIdx[jj++] = archetypes.size();
						match(lastMatchedIdx, s_tmpArchetypeMatches, archetypes, ops_ids_not, ops_ids_any, ops_ids_all);
					}

					// Make sure there is no match with NOT items.
					// It is possible they were not a part of previous archetypes.
					if (ops_ids_not.empty()) {
						for (auto* pArchetype: s_tmpArchetypeMatches) {
							if (match_one(pArchetype->entities(), ops_ids_not))
								continue;
							m_archetypeCache.push_back(pArchetype);
						}
					} else {
						for (auto* pArchetype: s_tmpArchetypeMatches) {
							m_archetypeCache.push_back(pArchetype);
						}
					}
				} else {
					GAIA_ASSERT2(false, "Querying exclusively for NOT items is not supported yet");
				}
			}

			GAIA_NODISCARD QueryId id() const {
				return m_lookupCtx.queryId;
			}

			GAIA_NODISCARD const QueryCtx::Data& data() const {
				return m_lookupCtx.data;
			}

			GAIA_NODISCARD const QueryEntityArray& ids() const {
				return m_lookupCtx.data.ids;
			}

			GAIA_NODISCARD const QueryEntityArray& filters() const {
				return m_lookupCtx.data.withChanged;
			}

			GAIA_NODISCARD bool has_filters() const {
				return !m_lookupCtx.data.withChanged.empty();
			}

			template <typename... T>
			bool has_any() const {
				return (has_inter<T>(QueryOp::Any) || ...);
			}

			template <typename... T>
			bool has_all() const {
				return (has_inter<T>(QueryOp::All) && ...);
			}

			template <typename... T>
			bool has_none() const {
				return (!has_inter<T>(QueryOp::Not) && ...);
			}

			//! Removes an archetype from cache
			//! \param pArchetype Archetype to remove
			void remove(Archetype* pArchetype) {
				GAIA_PROF_SCOPE(queryinfo::remove);

				const auto idx = core::get_index(m_archetypeCache, pArchetype);
				if (idx == BadIndex)
					return;
				core::erase_fast(m_archetypeCache, idx);

				// An archetype was removed from the world so the last matching archetype index needs to be
				// lowered by one for every component context.
				for (auto& lastMatchedArchetypeIdx: m_lookupCtx.data.lastMatchedArchetypeIdx) {
					if (lastMatchedArchetypeIdx > 0)
						--lastMatchedArchetypeIdx;
				}
			}

			GAIA_NODISCARD ArchetypeList::iterator begin() {
				return m_archetypeCache.begin();
			}

			GAIA_NODISCARD ArchetypeList::iterator end() {
				return m_archetypeCache.end();
			}
		};
	} // namespace ecs
} // namespace gaia
