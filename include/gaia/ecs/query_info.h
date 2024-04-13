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

		using EntityToArchetypeMap = cnt::map<EntityLookupKey, ArchetypeDArray>;
		struct ArchetypeCacheData {
			uint8_t indices[Chunk::MAX_COMPONENTS];
		};

		Archetype* archetype_from_entity(const World& world, Entity entity);
		bool is(const World& world, Entity entity, Entity baseEntity);
		template <typename Func>
		void as_relations_trav(const World& world, Entity target, Func func);
		template <typename Func>
		bool as_relations_trav_if(const World& world, Entity target, Func func);

		class QueryInfo {
		public:
			//! Query matching result
			enum class MatchArchetypeQueryRet : uint8_t { Fail, Ok, Skip };

		private:
			struct Instruction {
				Entity id;
				QueryOp op;
			};

			//! Query context
			QueryCtx m_ctx;
			//! Compiled instructions for the query engine
			cnt::darray<Instruction> m_instructions;
			//! Cached array of archetypes matching the query
			ArchetypeDArray m_archetypeCache;
			//! Cached array of query-specific data
			cnt::darray<ArchetypeCacheData> m_archetypeCacheData;
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
			bool has_inter(QueryOp op) const {
				// static_assert(is_raw_v<<T>, "has() must be used with raw types");
				constexpr bool isReadWrite = core::is_mut_v<T>;
				return has_inter<T>(op, isReadWrite);
			}

			//! Tries to match entity ids in \param queryIds with those that form \param archetype given
			//! the comparison function \param func.
			//! \return True if there is a match, false otherwise.
			template <typename Func>
			GAIA_NODISCARD bool match_inter(const Archetype& archetype, EntitySpan queryIds, Func func) const {
				auto archetypeIds = archetype.ids_view();
				const auto archetypeIdsCnt = (uint32_t)archetypeIds.size();
				const auto queryIdsCnt = queryIds.size();

				// Arrays are sorted so we can do linear intersection lookup
				uint32_t i = 0;
				uint32_t j = 0;
				while (i < archetypeIdsCnt && j < queryIdsCnt) {
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

			GAIA_NODISCARD static bool match_idQueryId_with_idArchetype(
					const World& w, uint32_t mask, uint32_t idQueryIdx, EntityId idQuery, EntityId idArchetype) {
				if ((mask & (1U << idQueryIdx)) == 0U)
					return idQuery == idArchetype;

				const auto qe = entity_from_id(w, idQuery);
				const auto ae = entity_from_id(w, idArchetype);
				return is(w, ae, qe);
			};

			GAIA_NODISCARD static bool cmp_ids(Entity idInArchetype, Entity idInQuery) {
				return idInQuery == idInArchetype;
			}

			GAIA_NODISCARD static bool cmp_ids_pairs(Entity idInArchetype, Entity idInQuery) {
				if (idInQuery.pair()) {
					// all(Pair<All, All>) aka "any pair"
					if (idInQuery == Pair(All, All))
						return true;

					// all(Pair<X, All>):
					//   X, AAA
					//   X, BBB
					//   ...
					//   X, ZZZ
					if (idInQuery.gen() == All.id())
						return idInQuery.id() == idInArchetype.id();

					// all(Pair<All, X>):
					//   AAA, X
					//   BBB, X
					//   ...
					//   ZZZ, X
					if (idInQuery.id() == All.id())
						return idInQuery.gen() == idInArchetype.gen();
				}

				return cmp_ids(idInArchetype, idInQuery);
			}

			GAIA_NODISCARD bool cmp_ids_is(const Archetype& archetype, Entity idInArchetype, Entity idInQuery) const {
				auto archetypeIds = archetype.ids_view();

				// all(Pair<Is, X>)
				if (idInQuery.pair() && idInQuery.id() == Is.id()) {
					return as_relations_trav_if(*m_ctx.w, idInQuery, [&](Entity relation) {
						const auto idx = core::get_index(archetypeIds, relation);
						// Stop at the first match
						return idx != BadIndex;
					});
				}

				return cmp_ids(idInArchetype, idInQuery);
			}

			GAIA_NODISCARD bool cmp_ids_is_pairs(const Archetype& archetype, Entity idInArchetype, Entity idInQuery) const {
				auto archetypeIds = archetype.ids_view();

				if (idInQuery.pair()) {
					// all(Pair<All, All>) aka "any pair"
					if (idInQuery == Pair(All, All))
						return true;

					// all(Pair<Is, X>)
					if (idInQuery.id() == Is.id()) {
						// (Is, X) in archetype == (Is, X) in query
						if (idInArchetype == idInQuery)
							return true;

						const auto e = entity_from_id(*m_ctx.w, idInQuery.gen());

						// If the archetype entity is an (Is, X) pair treat is as X and try matching it with
						// entities inheriting from e.
						if (idInArchetype.id() == Is.id()) {
							const auto e2 = entity_from_id(*m_ctx.w, idInArchetype.gen());
							return as_relations_trav_if(*m_ctx.w, e, [&](Entity relation) {
								return e2 == relation;
							});
						}

						// Archetype entity is generic, try matching it with entites inheriting from e.
						return as_relations_trav_if(*m_ctx.w, e, [&](Entity relation) {
							// Relation does not necessary match the sorted order of components in the archetype
							// so we need to search through all of its ids.
							const auto idx = core::get_index(archetypeIds, relation);
							// Stop at the first match
							return idx != BadIndex;
						});
					}

					// all(Pair<All, X>):
					//   AAA, X
					//   BBB, X
					//   ...
					//   ZZZ, X
					if (idInQuery.id() == All.id()) {
						if (idInQuery.gen() == idInArchetype.gen())
							return true;

						// If there are any Is pairs on the archetype we need to check if we match them
						if (archetype.pairs_is() > 0) {
							const auto e = entity_from_id(*m_ctx.w, idInQuery.gen());
							return as_relations_trav_if(*m_ctx.w, e, [&](Entity relation) {
								// Relation does not necessary match the sorted order of components in the archetype
								// so we need to search through all of its ids.
								const auto idx = core::get_index(archetypeIds, relation);
								// Stop at the first match
								return idx != BadIndex;
							});
						}

						// No match found
						return false;
					}

					// all(Pair<X, All>):
					//   X, AAA
					//   X, BBB
					//   ...
					//   X, ZZZ
					if (idInQuery.gen() == All.id()) {
						return idInQuery.id() == idInArchetype.id();
					}
				}

				return cmp_ids(idInArchetype, idInQuery);
			}

			//! Tries to match entity ids in \param queryIds with those in \param archetype given
			//! the comparison function \param func.
			//! \return True on the first match, false otherwise.
			template <typename Func>
			GAIA_NODISCARD bool match_res(const Archetype& archetype, EntitySpan queryIds, Func func) const {
				// Archetype has no pairs we can compare ids directly
				if (archetype.pairs() == 0) {
					return match_inter(archetype, queryIds, [&](Entity idInArchetype, Entity idInQuery) {
						return cmp_ids(idInArchetype, idInQuery) && func();
					});
				}

				return match_inter(archetype, queryIds, [&](Entity idInArchetype, Entity idInQuery) {
					return cmp_ids_pairs(idInArchetype, idInQuery) && func();
				});
			}

			//! Tries to match entity ids in \param queryIds with those in \param archetype given
			//! the comparison function \param func.
			//! \return True on the first match, false otherwise.
			GAIA_NODISCARD bool match_one(const Archetype& archetype, EntitySpan queryIds) const {
				return match_res(archetype, queryIds, []() {
					return true;
				});
			}

			//! Tries to match entity ids in \param queryIds with those in \param archetypeIds given
			//! the comparison function \param func.
			//! \return True if all ids match, false otherwise.
			GAIA_NODISCARD
			bool match_all(const Archetype& archetype, EntitySpan queryIds) const {
				uint32_t matches = 0;
				uint32_t expected = (uint32_t)queryIds.size();
				return match_res(archetype, queryIds, [&matches, expected]() {
					return (++matches) == expected;
				});
			}

			template <typename Func>
			GAIA_NODISCARD bool match_res_backtrack(const Archetype& archetype, EntitySpan queryIds, Func func) const {
				// Archetype has no pairs we can compare ids directly
				if (archetype.pairs() == 0) {
					return match_inter(archetype, queryIds, [&](Entity idInArchetype, Entity idInQuery) {
						return cmp_ids_is(archetype, idInArchetype, idInQuery) && func();
					});
				}

				return match_inter(archetype, queryIds, [&](Entity idInArchetype, Entity idInQuery) {
					return cmp_ids_is_pairs(archetype, idInArchetype, idInQuery) && func();
				});
			}

			GAIA_NODISCARD
			bool match_one_backtrack(const Archetype& archetype, EntitySpan queryIds) const {
				return match_res_backtrack(archetype, queryIds, []() {
					return true;
				});
			}

			GAIA_NODISCARD
			bool match_all_backtrack(const Archetype& archetype, EntitySpan queryIds) const {
				uint32_t matches = 0;
				uint32_t expected = (uint32_t)queryIds.size();
				return match_res_backtrack(archetype, queryIds, [&matches, expected]() {
					return (++matches) == expected;
				});
			}

		public:
			void init(World* world) {
				m_ctx.w = world;
			}

			GAIA_NODISCARD static QueryInfo
			create(QueryId id, QueryCtx&& ctx, const EntityToArchetypeMap& entityToArchetypeMap) {
				// Make sure query items are sorted
				sort(ctx);

				QueryInfo info;
				info.m_ctx = GAIA_MOV(ctx);
				info.m_ctx.q.queryId = id;

				// Compile the query
				info.compile(entityToArchetypeMap);

				return info;
			}

			//! Compile the query terms into a form we can easily process
			void compile(const EntityToArchetypeMap& entityToArchetypeMap) {
				GAIA_PROF_SCOPE(queryinfo::compile);

				auto& data = m_ctx.data;

				QueryTermSpan terms{data.terms.data(), data.terms.size()};
				QueryTermSpan terms_all = terms.subspan(0, data.firstAny);
				QueryTermSpan terms_any = terms.subspan(data.firstAny, data.firstNot - data.firstAny);
				QueryTermSpan terms_not = terms.subspan(data.firstNot);

				// ALL
				if (!terms_all.empty()) {
					GAIA_PROF_SCOPE(queryinfo::compile_all);

					GAIA_EACH(terms_all) {
						auto& p = terms_all[i];
						if (p.src == EntityBad) {
							m_instructions.emplace_back(p.id, QueryOp::All);
							continue;
						}

						// Match static fixed sources
						p.srcArchetype = archetype_from_entity(*m_ctx.w, p.src);

						// Archetype needs to exist. If it does not we have nothing to do here.
						if (p.srcArchetype == nullptr) {
							m_instructions.clear();
							return;
						}
					}
				}

				// ANY
				if (!terms_any.empty()) {
					GAIA_PROF_SCOPE(queryinfo::compile_any);

					cnt::sarr_ext<const ArchetypeDArray*, MAX_ITEMS_IN_QUERY> archetypesWithId;
					GAIA_EACH(terms_any) {
						auto& p = terms_any[i];
						if (p.src != EntityBad) {
							p.srcArchetype = archetype_from_entity(*m_ctx.w, p.src);
							if (p.srcArchetype == nullptr)
								continue;
						}

						// Check if any archetype is associated with the entity id.
						// All ids must be registered in the world.
						const auto it = entityToArchetypeMap.find(EntityLookupKey(p.id));
						if (it == entityToArchetypeMap.end() || it->second.empty())
							continue;

						archetypesWithId.push_back(&it->second);
						m_instructions.emplace_back(p.id, QueryOp::Any);
					}

					// No archetypes with "any" entities exist. We can quit right away.
					if (archetypesWithId.empty()) {
						m_instructions.clear();
						return;
					}
				}

				// NOT
				if (!terms_not.empty()) {
					GAIA_PROF_SCOPE(queryinfo::compile_not);

					GAIA_EACH(terms_not) {
						auto& p = terms_not[i];
						if (p.src != EntityBad)
							continue;

						m_instructions.emplace_back(p.id, QueryOp::Not);
					}
				}
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

			void match_archetype_all(
					const EntityToArchetypeMap& entityToArchetypeMap, cnt::set<Archetype*>& matchesSet,
					ArchetypeDArray& matchesArr, Entity ent, EntitySpan idsToMatch) {
				// For ALL we need all the archetypes to match. We start by checking
				// if the first one is registered in the world at all.
				const auto it = entityToArchetypeMap.find(EntityLookupKey(ent));
				if (it == entityToArchetypeMap.end() || it->second.empty())
					return;

				auto& data = m_ctx.data;

				const auto& archetypes = it->second;
				const auto cache_it = data.lastMatchedArchetypeIdx_All.find(EntityLookupKey(ent));
				uint32_t lastMatchedIdx = 0;
				if (cache_it == data.lastMatchedArchetypeIdx_All.end())
					data.lastMatchedArchetypeIdx_All.emplace(EntityLookupKey(ent), archetypes.size());
				else {
					lastMatchedIdx = cache_it->second;
					cache_it->second = archetypes.size();
				}

				// For simple cases it is enough to add archetypes to cache right away
				if (idsToMatch.size() == 1) {
					for (uint32_t a = lastMatchedIdx; a < archetypes.size(); ++a) {
						auto* pArchetype = archetypes[a];
						matchesArr.emplace_back(pArchetype);
					}
				} else {
					for (uint32_t a = lastMatchedIdx; a < archetypes.size(); ++a) {
						auto* pArchetype = archetypes[a];

						if (matchesSet.contains(pArchetype))
							continue;
						if (!match_all(*pArchetype, idsToMatch))
							continue;

						matchesSet.emplace(pArchetype);
						matchesArr.emplace_back(pArchetype);
					}
				}
			}

			void match_archetype_all_as(
					const EntityToArchetypeMap& entityToArchetypeMap, const ArchetypeDArray& allArchetypes,
					cnt::set<Archetype*>& matchesSet, ArchetypeDArray& matchesArr, Entity ent, EntitySpan idsToMatch) {
				// For ALL we need all the archetypes to match. We start by checking
				// if the first one is registered in the world at all.

				const ArchetypeDArray* pSrcArchetypes = nullptr;

				if (ent.id() == Is.id()) {
					ent = EntityBad;
					pSrcArchetypes = &allArchetypes;
				} else {
					const auto it = entityToArchetypeMap.find(EntityLookupKey(ent));
					if (it == entityToArchetypeMap.end() || it->second.empty())
						return;
					pSrcArchetypes = &it->second;
				}

				auto& data = m_ctx.data;

				const auto& archetypes = *pSrcArchetypes;
				const auto cache_it = data.lastMatchedArchetypeIdx_All.find(EntityLookupKey(ent));
				uint32_t lastMatchedIdx = 0;
				if (cache_it == data.lastMatchedArchetypeIdx_All.end())
					data.lastMatchedArchetypeIdx_All.emplace(EntityLookupKey(ent), archetypes.size());
				else {
					lastMatchedIdx = cache_it->second;
					cache_it->second = archetypes.size();
				}

				// For simple cases it is enough to add archetypes to cache right away
				// if (idsToMatch.size() == 1) {
				// 	for (uint32_t a = lastMatchedIdx; a < archetypes.size(); ++a) {
				// 		auto* pArchetype = archetypes[a];

				// 		auto res = matchesSet.emplace(pArchetype);
				// 		if (res.second)
				// 			matchesArr.emplace_back(pArchetype);
				// 	}
				// } else {
				for (uint32_t a = lastMatchedIdx; a < archetypes.size(); ++a) {
					auto* pArchetype = archetypes[a];

					if (matchesSet.contains(pArchetype))
						continue;
					if (!match_all_backtrack(*pArchetype, idsToMatch))
						continue;

					matchesSet.emplace(pArchetype);
					matchesArr.emplace_back(pArchetype);
				}
				//}
			}

			void match_archetype_one(
					const EntityToArchetypeMap& entityToArchetypeMap, cnt::set<Archetype*>& matchesSet,
					ArchetypeDArray& matchesArr, Entity ent, EntitySpan idsToMatch) {
				// For ANY we need at least one archetypes to match.
				// However, because any of them can match, we need to check them all.
				// Iterating all of them is caller's responsibility.
				const auto it = entityToArchetypeMap.find(EntityLookupKey(ent));
				if (it == entityToArchetypeMap.end() || it->second.empty())
					return;

				auto& data = m_ctx.data;

				const auto& archetypes = it->second;
				const auto cache_it = data.lastMatchedArchetypeIdx_Any.find(EntityLookupKey(ent));
				uint32_t lastMatchedIdx = 0;
				if (cache_it == data.lastMatchedArchetypeIdx_Any.end())
					data.lastMatchedArchetypeIdx_Any.emplace(EntityLookupKey(ent), archetypes.size());
				else {
					lastMatchedIdx = cache_it->second;
					cache_it->second = archetypes.size();
				}

				// For simple cases it is enough to add archetypes to cache right away
				if (idsToMatch.size() == 1) {
					for (uint32_t a = lastMatchedIdx; a < archetypes.size(); ++a) {
						auto* pArchetype = archetypes[a];

						auto res = matchesSet.emplace(pArchetype);
						if (res.second)
							matchesArr.emplace_back(pArchetype);
					}
				} else {
					for (uint32_t a = lastMatchedIdx; a < archetypes.size(); ++a) {
						auto* pArchetype = archetypes[a];

						if (matchesSet.contains(pArchetype))
							continue;
						if (!match_one(*pArchetype, idsToMatch))
							continue;

						matchesSet.emplace(pArchetype);
						matchesArr.emplace_back(pArchetype);
					}
				}
			}

			void match_archetype_one_as(
					const EntityToArchetypeMap& entityToArchetypeMap, const ArchetypeDArray& allArchetypes,
					cnt::set<Archetype*>& matchesSet, ArchetypeDArray& matchesArr, Entity ent, EntitySpan idsToMatch) {
				// For ANY we need at least one archetypes to match.
				// However, because any of them can match, we need to check them all.
				// Iterating all of them is caller's responsibility.

				const ArchetypeDArray* pSrcArchetypes = nullptr;

				if (ent.id() == Is.id()) {
					ent = EntityBad;
					pSrcArchetypes = &allArchetypes;
				} else {
					const auto it = entityToArchetypeMap.find(EntityLookupKey(ent));
					if (it == entityToArchetypeMap.end() || it->second.empty())
						return;
					pSrcArchetypes = &it->second;
				}

				auto& data = m_ctx.data;

				const auto& archetypes = *pSrcArchetypes;
				const auto cache_it = data.lastMatchedArchetypeIdx_Any.find(EntityLookupKey(ent));
				uint32_t lastMatchedIdx = 0;
				if (cache_it == data.lastMatchedArchetypeIdx_Any.end())
					data.lastMatchedArchetypeIdx_Any.emplace(EntityLookupKey(ent), archetypes.size());
				else {
					lastMatchedIdx = cache_it->second;
					cache_it->second = archetypes.size();
				}

				// For simple cases it is enough to add archetypes to cache right away
				// if (idsToMatch.size() == 1) {
				// 	for (uint32_t a = lastMatchedIdx; a < archetypes.size(); ++a) {
				// 		auto* pArchetype = archetypes[a];

				// 		auto res = matchesSet.emplace(pArchetype);
				// 		if (res.second)
				// 			matchesArr.emplace_back(pArchetype);
				// 	}
				// } else {
				for (uint32_t a = lastMatchedIdx; a < archetypes.size(); ++a) {
					auto* pArchetype = archetypes[a];

					if (matchesSet.contains(pArchetype))
						continue;
					if (!match_one_backtrack(*pArchetype, idsToMatch))
						continue;

					matchesSet.emplace(pArchetype);
					matchesArr.emplace_back(pArchetype);
				}
				//}
			}

			void match_archetype_no(
					const ArchetypeDArray& archetypes, cnt::set<Archetype*>& matchesSet, ArchetypeDArray& matchesArr,
					EntitySpan idsToMatch) {
				// For NO we need to search among all archetypes.
				const EntityLookupKey key(EntityBad);
				auto& data = m_ctx.data;

				const auto cache_it = data.lastMatchedArchetypeIdx_All.find(key);
				uint32_t lastMatchedIdx = 0;
				if (cache_it == data.lastMatchedArchetypeIdx_All.end())
					data.lastMatchedArchetypeIdx_All.emplace(key, 0U);
				else
					lastMatchedIdx = cache_it->second;
				cache_it->second = archetypes.size();

				for (uint32_t a = lastMatchedIdx; a < archetypes.size(); ++a) {
					auto* pArchetype = archetypes[a];
					if (matchesSet.contains(pArchetype))
						continue;
					if (match_one(*pArchetype, idsToMatch))
						continue;

					matchesSet.emplace(pArchetype);
					matchesArr.emplace_back(pArchetype);
				}
			}

			void match_archetype_no_as(
					const ArchetypeDArray& archetypes, cnt::set<Archetype*>& matchesSet, ArchetypeDArray& matchesArr,
					EntitySpan idsToMatch) {
				// For NO we need to search among all archetypes.
				const EntityLookupKey key(EntityBad);
				auto& data = m_ctx.data;

				const auto cache_it = data.lastMatchedArchetypeIdx_All.find(key);
				uint32_t lastMatchedIdx = 0;
				if (cache_it == data.lastMatchedArchetypeIdx_All.end())
					data.lastMatchedArchetypeIdx_All.emplace(key, 0U);
				else
					lastMatchedIdx = cache_it->second;
				cache_it->second = archetypes.size();

				for (uint32_t a = lastMatchedIdx; a < archetypes.size(); ++a) {
					auto* pArchetype = archetypes[a];

					if (matchesSet.contains(pArchetype))
						continue;
					if (match_one_backtrack(*pArchetype, idsToMatch))
						continue;

					matchesSet.emplace(pArchetype);
					matchesArr.emplace_back(pArchetype);
				}
			}

			void do_match_all(
					const EntityToArchetypeMap& entityToArchetypeMap, const ArchetypeDArray& allArchetypes,
					cnt::set<Archetype*>& matchesSet, ArchetypeDArray& matchesArr, Entity ent, EntitySpan idsToMatch,
					uint32_t as_mask_0, uint32_t as_mask_1) {
				// First viable item is not related to an Is relationship
				if (as_mask_0 + as_mask_1 == 0U) {
					match_archetype_all(entityToArchetypeMap, matchesSet, matchesArr, ent, idsToMatch);
				} else
				// First viable item is related to an Is relationship.
				// In this case we need to gather all related archetypes and evaluate one-by-one (backtracking).
				{
					match_archetype_all_as(entityToArchetypeMap, allArchetypes, matchesSet, matchesArr, ent, idsToMatch);
				}
			}

			void do_match_one(
					const EntityToArchetypeMap& entityToArchetypeMap, const ArchetypeDArray& allArchetypes,
					cnt::set<Archetype*>& matchesSet, ArchetypeDArray& matchesArr, Entity ent, EntitySpan idsToMatch,
					uint32_t as_mask_0, uint32_t as_mask_1) {
				// First viable item is not related to an Is relationship
				if (as_mask_0 + as_mask_1 == 0U) {
					match_archetype_one(entityToArchetypeMap, matchesSet, matchesArr, ent, idsToMatch);
				}
				// First viable item is related to an Is relationship.
				// In this case we need to gather all related archetypes.
				else {
					match_archetype_one_as(entityToArchetypeMap, allArchetypes, matchesSet, matchesArr, ent, idsToMatch);
				}
			}

			bool do_match_one(const Archetype& archetype, EntitySpan idsToMatch, uint32_t as_mask_0, uint32_t as_mask_1) {
				// First viable item is not related to an Is relationship
				if (as_mask_0 + as_mask_1 == 0U) {
					return match_one(archetype, idsToMatch);
				}
				// First viable item is related to an Is relationship.
				// In this case we need to gather all related archetypes.
				else {
					return match_one_backtrack(archetype, idsToMatch);
				}
			}

			void do_match_no(
					const ArchetypeDArray& allArchetypes, cnt::set<Archetype*>& matchesSet, ArchetypeDArray& matchesArr,
					EntitySpan idsToMatch, uint32_t as_mask_0, uint32_t as_mask_1) {
				matchesSet.clear();

				if (as_mask_0 + as_mask_1 == 0U)
					match_archetype_no(allArchetypes, matchesSet, matchesArr, idsToMatch);
				else
					match_archetype_no_as(allArchetypes, matchesSet, matchesArr, idsToMatch);
			}

			//! Tries to match the query against archetypes in \param entityToArchetypeMap.
			//! This is necessary so we do not iterate all chunks over and over again when running queries.
			void match(
					// entity -> archetypes mapping
					const EntityToArchetypeMap& entityToArchetypeMap,
					// all archetypes in the world
					const ArchetypeDArray& allArchetypes,
					// last matched archetype id
					ArchetypeId archetypeLastId) {
				static cnt::set<Archetype*> s_tmpArchetypeMatches;
				static ArchetypeDArray s_tmpArchetypeMatchesArr;

				struct CleanUpTmpArchetypeMatches {
					~CleanUpTmpArchetypeMatches() {
						s_tmpArchetypeMatches.clear();
						s_tmpArchetypeMatchesArr.clear();
					}
				} autoCleanup;

				// Skip if no new archetype appeared
				GAIA_ASSERT(archetypeLastId >= m_lastArchetypeId);
				if (m_lastArchetypeId == archetypeLastId || m_instructions.empty())
					return;
				m_lastArchetypeId = archetypeLastId;

				GAIA_PROF_SCOPE(queryinfo::match);

				auto& data = m_ctx.data;

				// Array of archetypes containing the given entity/component/pair
				cnt::sarr_ext<const ArchetypeDArray*, MAX_ITEMS_IN_QUERY> archetypesWithId;
				cnt::sarr_ext<Entity, MAX_ITEMS_IN_QUERY> ids_all;
				cnt::sarr_ext<Entity, MAX_ITEMS_IN_QUERY> ids_any;
				cnt::sarr_ext<Entity, MAX_ITEMS_IN_QUERY> ids_not;

				QueryTermSpan terms{data.terms.data(), data.terms.size()};
				// QueryTermSpan terms_all = terms.subspan(0, data.firstAny);
				QueryTermSpan terms_any = terms.subspan(data.firstAny, data.firstNot - data.firstAny);
				QueryTermSpan terms_not = terms.subspan(data.firstNot);

				for (const auto& inst: m_instructions) {
					switch (inst.op) {
						case QueryOp::All:
							ids_all.push_back(inst.id);
							break;
						case QueryOp::Any:
							ids_any.push_back(inst.id);
							break;
						default:
							ids_not.push_back(inst.id);
							break;
					}
				}

				if (!ids_all.empty()) {
					do_match_all(
							entityToArchetypeMap, allArchetypes, s_tmpArchetypeMatches, s_tmpArchetypeMatchesArr, //
							ids_all[0], std::span{ids_all.data(), ids_all.size()}, //
							data.as_mask, data.as_mask_2);

					// No ALL matches were found. We can quit right away.
					if (s_tmpArchetypeMatchesArr.empty())
						return;
				}

				if (!terms_any.empty()) {
					if (ids_all.empty()) {
						// We didn't try to match any ALL items.
						// We need to search among all archetypes.

						// Try find matches with optional components.
						GAIA_EACH(ids_any) {
							do_match_one(
									entityToArchetypeMap, allArchetypes, s_tmpArchetypeMatches, s_tmpArchetypeMatchesArr, //
									ids_any[i], std::span{ids_any.data(), ids_any.size()}, //
									data.as_mask, data.as_mask_2);
						}
					} else {
						// We tried to match ALL items. Only search among those we already found.
						// No last matched idx update is necessary because all ids were already checked
						// during the ALL pass.
						for (uint32_t i = 0; i < s_tmpArchetypeMatchesArr.size();) {
							auto* pArchetype = s_tmpArchetypeMatchesArr[i];

							GAIA_FOR_((uint32_t)ids_any.size(), j) {
								if (do_match_one(
												*pArchetype, //
												std::span{ids_any.data(), ids_any.size()}, //
												data.as_mask, data.as_mask_2))
									goto checkNextArchetype;
							}

							// No match found among ANY. Remove the archetype from the matching ones
							core::erase_fast(s_tmpArchetypeMatchesArr, i);
							continue;

						checkNextArchetype:
							++i;
						}
					}
				}

				// Make sure there is no match with NOT items.
				if (!terms_not.empty()) {
					// We searched for nothing more than NOT matches
					if (s_tmpArchetypeMatchesArr.empty()) {
						do_match_no(
								allArchetypes, s_tmpArchetypeMatches, m_archetypeCache, //
								std::span{ids_not.data(), ids_not.size()}, //
								data.as_mask, data.as_mask_2);
					} else {
						// Write the temporary matches to cache if no match with NO is found
						for (auto* pArchetype: s_tmpArchetypeMatchesArr) {
							if (match_one(
											*pArchetype, //
											std::span{ids_not.data(), ids_not.size()}))
								continue;

							add_archetype_to_cache(pArchetype);
						}
					}
				} else {
					// Write the temporary matches to cache
					for (auto* pArchetype: s_tmpArchetypeMatchesArr)
						add_archetype_to_cache(pArchetype);
				}
			}

			void add_archetype_to_cache(Archetype* pArchetype) {
				// Add archetype to cache
				m_archetypeCache.push_back(pArchetype);

				// Update id mappings
				ArchetypeCacheData cacheData;
				const auto& queryIds = ids();
				GAIA_EACH(queryIds) {
					const auto idxBeforeRemapping = m_ctx.data.remapping[i];
					const auto queryId = queryIds[idxBeforeRemapping];
					// compIdx can be -1. We are fine with it because the user should never ask for something
					// that is not present on the archetype. If they do, they made a mistake.
					const auto compIdx = core::get_index_unsafe(pArchetype->ids_view(), queryId);

					cacheData.indices[i] = (uint8_t)compIdx;
				}
				m_archetypeCacheData.push_back(GAIA_MOV(cacheData));
			}

			void del_archetype_from_cache(uint32_t idx) {
				core::erase_fast(m_archetypeCache, idx);
				core::erase_fast(m_archetypeCacheData, idx);
			}

			GAIA_NODISCARD World* world() {
				return const_cast<World*>(m_ctx.w);
			}
			GAIA_NODISCARD const World* world() const {
				return m_ctx.w;
			}

			GAIA_NODISCARD QueryId id() const {
				return m_ctx.q.queryId;
			}

			GAIA_NODISCARD QuerySerBuffer& ser_buffer() {
				return m_ctx.q.ser_buffer(world());
			}
			void ser_buffer_reset() {
				m_ctx.q.ser_buffer_reset(world());
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
				return (has_inter<T>(QueryOp::Any) || ...);
			}

			template <typename... T>
			bool has_all() const {
				return (has_inter<T>(QueryOp::All) && ...);
			}

			template <typename... T>
			bool has_no() const {
				return (!has_inter<T>(QueryOp::Not) && ...);
			}

			//! Removes an archetype from cache
			//! \param pArchetype Archetype to remove
			void remove(Archetype* pArchetype) {
				GAIA_PROF_SCOPE(queryinfo::remove);

				const auto idx = core::get_index(m_archetypeCache, pArchetype);
				if (idx == BadIndex)
					return;

				del_archetype_from_cache(idx);

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
				return {(const uint8_t*)&data.indices[0], Chunk::MAX_COMPONENTS};
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
		};
	} // namespace ecs
} // namespace gaia
