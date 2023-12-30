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
		class World;

		using EntityToArchetypeMap = cnt::map<EntityLookupKey, ArchetypeList>;

		inline Archetype* archetype_from_entity(const World& world, Entity entity);

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

			enum QueryCmdType : uint8_t { ALL, ANY, NOT };

			template <typename TType>
			bool has_inter([[maybe_unused]] QueryOp op, bool isReadWrite) const {
				using T = core::raw_t<TType>;

				if constexpr (std::is_same_v<T, Entity>) {
					// Entities are read-only.
					GAIA_ASSERT(!isReadWrite);
					// Skip Entity input args. Entities are always there.
					return true;
				} else {
					Entity id;

					if constexpr (is_pair<T>::value) {
						const auto rel = m_lookupCtx.cc->get<typename T::rel>().entity;
						const auto tgt = m_lookupCtx.cc->get<typename T::tgt>().entity;
						id = (Entity)Pair(rel, tgt);
					} else {
						id = m_lookupCtx.cc->get<T>().entity;
					}

					const auto& data = m_lookupCtx.data;
					const auto& pairs = data.pairs;
					const auto compIdx = comp_idx<MAX_ITEMS_IN_QUERY>(pairs.data(), id, EntityBad);

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
				constexpr bool isReadWrite = core::is_mut_v<T>;
				return has_inter<T>(op, isReadWrite);
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
					if (idQuery.pair()) {
						// all(Pair<All, All>) aka "any pair"
						if (idQuery == Pair(All, All))
							return true;

						// all(Pair<X, All>):
						//   X, AAA
						//   X, BBB
						//   ...
						//   X, ZZZ
						if (idQuery.gen() == All.id())
							return idQuery.id() == idArchetype.id();

						// all(Pair<All, X>):
						//   AAA, X
						//   BBB, X
						//   ...
						//   ZZZ, X
						if (idQuery.id() == All.id())
							return idQuery.gen() == idArchetype.gen();
					}

					return idQuery == idArchetype;
				});
			}

			//! Tries to match entity ids in \param queryIds with those in \param archetypeIds given
			//! the comparison function \param func.
			//! A fast version of match_one which does not consider wildcards.
			//! \return True on the first match, false otherwise.
			GAIA_NODISCARD bool match_one_nopair(const Chunk::EntityArray& archetypeIds, EntitySpan queryIds) const {
				return match_inter(archetypeIds, queryIds, [&](Entity idArchetype, Entity idQuery) {
					return idQuery == idArchetype;
				});
			}

			//! Tries to match entity ids in \param queryIds with those in \param archetypeIds given
			//! the comparison function \param func.
			//! \return True if all ids match, false otherwise.
			GAIA_NODISCARD bool match_all(const Chunk::EntityArray& archetypeIds, EntitySpan queryIds) const {
				uint32_t matches = 0;
				return match_inter(archetypeIds, queryIds, [&](Entity idArchetype, Entity idQuery) {
					if (idQuery.pair()) {
						// all(Pair<All, All>) aka "any pair"
						if (idQuery == Pair(All, All))
							return ++matches == (uint32_t)queryIds.size();

						// all(Pair<X, All>):
						//   X, AAA
						//   X, BBB
						//   ...
						//   X, ZZZ
						if (idQuery.gen() == All.id())
							return idQuery.id() == idArchetype.id() && (++matches == (uint32_t)queryIds.size());

						// all(Pair<All, X>):
						//   AAA, X
						//   BBB, X
						//   ...
						//   ZZZ, X
						if (idQuery.id() == All.id())
							return idQuery.gen() == idArchetype.gen() && (++matches == (uint32_t)queryIds.size());
					}

					return idQuery == idArchetype && (++matches == (uint32_t)queryIds.size());
				});
			}

			//! Tries to match entity ids in \param queryIds with those in \param archetypeIds given
			//! the comparison function \param func.
			//! A fast version of match_one which does not consider wildcards.
			//! \return True if all ids match, false otherwise.
			GAIA_NODISCARD bool match_all_nopair(const Chunk::EntityArray& archetypeIds, EntitySpan queryIds) const {
				uint32_t matches = 0;
				return match_inter(archetypeIds, queryIds, [&](Entity idArchetype, Entity idQuery) {
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

			//! Tries to match the query against archetypes in \param entityToArchetypeMap.
			//! This is necessary so we do not iterate all chunks over and over again when running queries.
			void match(
					// entity -> archetypes mapping
					const EntityToArchetypeMap& entityToArchetypeMap,
					// last matched archetype id
					ArchetypeId archetypeLastId) {
				static cnt::set<Archetype*> s_tmpArchetypeMatches;
				static cnt::darray<Archetype*> s_tmpArchetypeMatchesArr;

				struct CleanUpTmpArchetypeMatches {
					~CleanUpTmpArchetypeMatches() {
						s_tmpArchetypeMatches.clear();
						s_tmpArchetypeMatchesArr.clear();
					}
				} autoCleanup;

				// Skip if no new archetype appeared
				GAIA_ASSERT(archetypeLastId >= m_lastArchetypeId);
				if (m_lastArchetypeId == archetypeLastId)
					return;
				m_lastArchetypeId = archetypeLastId;

				GAIA_PROF_SCOPE(queryinfo::match);

				auto& data = m_lookupCtx.data;
				const auto& pairs = data.pairs;
				if (pairs.empty())
					return;

				// Array of archetypes containing the given entity/component/pair
				cnt::sarr_ext<const ArchetypeList*, MAX_ITEMS_IN_QUERY> archetypesWithId;
				cnt::sarr_ext<Entity, MAX_ITEMS_IN_QUERY> ids_all;
				cnt::sarr_ext<Entity, MAX_ITEMS_IN_QUERY> ids_any;
				cnt::sarr_ext<Entity, MAX_ITEMS_IN_QUERY> ids_none;
				uint32_t jj = 0;

				QueryEntityOpPairSpan ops_ids{pairs.data(), pairs.size()};
				QueryEntityOpPairSpan ops_ids_all = ops_ids.subspan(0, data.firstAny);
				QueryEntityOpPairSpan ops_ids_any = ops_ids.subspan(data.firstAny, data.firstNot - data.firstAny);
				QueryEntityOpPairSpan ops_ids_not = ops_ids.subspan(data.firstNot);

				if (!ops_ids_all.empty()) {
					GAIA_EACH(ops_ids_all) {
						const auto& p = ops_ids_all[i];
						if (p.src != EntityBad)
							continue;

						// For ALL we need all the archetypes to match. We start by checking
						// if the first one is registered in the world at all.
						const auto it = entityToArchetypeMap.find(EntityLookupKey(ops_ids_all[0].id));
						if (it == entityToArchetypeMap.end() || it->second.empty())
							return;

						const auto& arr = it->second;
						archetypesWithId.push_back(&arr);
						break;
					}

					const auto& archetypes_all = *archetypesWithId[0];
					const auto lastMatchedIdx = data.lastMatchedArchetypeIdx[jj];
					data.lastMatchedArchetypeIdx[jj++] = archetypes_all.size();

					// Match static fixed sources
					GAIA_EACH(ops_ids_all) {
						auto& p = ops_ids_all[i];
						if (p.src == EntityBad) {
							ids_all.push_back(p.id);
							continue;
						}

						if (p.srcArchetype == nullptr) {
							p.srcArchetype = archetype_from_entity(*m_lookupCtx.w, p.src);

							// Archetype needs to exist. If it does not we have nothing to do here.
							if (p.srcArchetype == nullptr)
								return;
						}
					}

					// Match variable fixed sources
					{
						for (uint32_t a = lastMatchedIdx; a < archetypes_all.size(); ++a) {
							auto* pArchetype = archetypes_all[a];

							if (s_tmpArchetypeMatches.contains(pArchetype))
								continue;
							if (!match_all(pArchetype->ids(), ids_all))
								continue;

							auto res = s_tmpArchetypeMatches.emplace(pArchetype);
							if (res.second)
								s_tmpArchetypeMatchesArr.emplace_back(pArchetype);
						}
					}

					// No ALL matches were found. We can quit right away.
					if (s_tmpArchetypeMatchesArr.empty())
						return;
				}

				if (!ops_ids_any.empty()) {
					archetypesWithId.clear();
					GAIA_EACH(ops_ids_any) {
						auto& p = ops_ids_any[i];
						if (p.src != EntityBad) {
							if (p.srcArchetype == nullptr)
								p.srcArchetype = archetype_from_entity(*m_lookupCtx.w, p.src);
							if (p.srcArchetype == nullptr)
								continue;
						}

						// Check if any archetype is associated with the entity id.
						// All ids must be registered in the world.
						const auto it = entityToArchetypeMap.find(EntityLookupKey(ops_ids_any[i].id));
						if (it == entityToArchetypeMap.end() || it->second.empty())
							continue;

						archetypesWithId.push_back(&it->second);
						ids_any.push_back(p.id);
					}

					// No archetypes with "any" entities exist. We can quit right away.
					if (archetypesWithId.empty())
						return;
				}

				if (!ids_any.empty()) {
					if (ops_ids_all.empty()) {
						// We didn't try to match any ALL items.
						// We need to search among all archetypes.

						// Try find matches with optional components.
						GAIA_EACH(ids_any) {
							const auto& archetypes = *archetypesWithId[i];
							const auto lastMatchedIdx = data.lastMatchedArchetypeIdx[jj];
							data.lastMatchedArchetypeIdx[jj++] = archetypes.size();

							for (uint32_t a = lastMatchedIdx; a < archetypes.size(); ++a) {
								auto* pArchetype = archetypes[a];

								if (s_tmpArchetypeMatches.contains(pArchetype))
									continue;
								if (!match_one(pArchetype->ids(), ids_any))
									continue;

								auto res = s_tmpArchetypeMatches.emplace(pArchetype);
								if (res.second)
									s_tmpArchetypeMatchesArr.emplace_back(pArchetype);
							}
						}
					} else {
						// We tried to match ALL items. Only search among those we already found.
						// No last matched idx update is necessary because all ids were already checked
						// during the ALL pass.
						for (uint32_t i = 0; i < s_tmpArchetypeMatchesArr.size();) {
							auto* pArchetype = s_tmpArchetypeMatchesArr[i];

							if (match_one(pArchetype->ids(), ids_any)) {
								++i;
								continue;
							}

							core::erase_fast(s_tmpArchetypeMatchesArr, i);
						}
					}
				}

				// Make sure there is no match with NOT items.
				if (!ops_ids_not.empty()) {
					GAIA_EACH(ops_ids_not) {
						const auto& p = ops_ids_not[i];
						if (p.src == EntityBad)
							ids_none.push_back(p.id);
					}

					// Write the temporary matches to local cache
					for (auto* pArchetype: s_tmpArchetypeMatchesArr) {
						if (match_one(pArchetype->ids(), ids_none))
							continue;

						m_archetypeCache.push_back(pArchetype);
					}
				} else {
					// Write the temporary matches to local cache
					for (auto* pArchetype: s_tmpArchetypeMatchesArr)
						m_archetypeCache.push_back(pArchetype);
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
