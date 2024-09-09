#pragma once
#include "../config/config.h"

#include "../cnt/darray.h"
#include "../cnt/sarray_ext.h"
#include "../cnt/set.h"
#include "../config/profiler.h"
#include "../core/hashing_policy.h"
#include "../core/utility.h"
#include "../mem/mem_utils.h"
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
			GroupId groupId = 0;
			uint8_t indices[Chunk::MAX_COMPONENTS];
		};

		Archetype* archetype_from_entity(const World& world, Entity entity);
		bool is(const World& world, Entity entity, Entity baseEntity);
		template <typename Func>
		void as_relations_trav(const World& world, Entity target, Func func);
		template <typename Func>
		bool as_relations_trav_if(const World& world, Entity target, Func func);

		extern GroupId group_by_func_default(const World& world, const Archetype& archetype, Entity groupBy);

		class QueryInfo {
		public:
			//! Query matching result
			enum class MatchArchetypeQueryRet : uint8_t { Fail, Ok, Skip };

		private:
			struct Instruction {
				Entity id;
				QueryOp op;
			};

			struct GroupData {
				GroupId groupId;
				uint32_t idxFirst;
				uint32_t idxLast;
				bool needsSorting;
			};

			struct MatchingCtx {
				const EntityToArchetypeMap* pEntityToArchetypeMap;
				const ArchetypeDArray* pAllArchetypes;
				cnt::set<Archetype*>* pMatchesSet;
				ArchetypeDArray* pMatchesArr;

				Entity ent;
				EntitySpan idsToMatch;
				uint32_t as_mask_0;
				uint32_t as_mask_1;
			};

			//! Query context
			QueryCtx m_ctx;
			//! Compiled instructions for the query engine
			cnt::darray<Instruction> m_instructions;

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
			GAIA_NODISCARD bool has_inter([[maybe_unused]] QueryOp op, bool isReadWrite) const {
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
			GAIA_NODISCARD bool has_inter(QueryOp op) const {
				// static_assert(is_raw_v<<T>, "has() must be used with raw types");
				constexpr bool isReadWrite = core::is_mut_v<T>;
				return has_inter<T>(op, isReadWrite);
			}

			// Operator AND (used by query::all)
			struct OpAnd {
				static bool can_continue(bool hasMatch) {
					return hasMatch;
				};
				static bool eval(uint32_t expectedMatches, uint32_t totalMatches) {
					return expectedMatches == totalMatches;
				}
			};
			// Operator OR (used by query::any)
			struct OpOr {
				static bool can_continue(bool hasMatch) {
					return hasMatch;
				};
				static bool eval(uint32_t expectedMatches, uint32_t totalMatches) {
					(void)expectedMatches;
					return totalMatches > 0;
				}
			};
			// Operator NOT (used by query::no)
			struct OpNo {
				static bool can_continue(bool hasMatch) {
					return !hasMatch;
				};
				static bool eval(uint32_t expectedMatches, uint32_t totalMatches) {
					(void)expectedMatches;
					return totalMatches == 0;
				}
			};

			template <typename Op>
			static bool match_inter_eval_matches(uint32_t queryIdMarches, uint32_t& outMatches) {
				const bool hadAnyMatches = queryIdMarches > 0;

				// We finished checking matches with an id from query.
				// We need to check if we have sufficient amount of results in the run.
				if (!Op::can_continue(hadAnyMatches))
					return false;

				// No matter the amount of matches we only care if at least one
				// match happened with the id from query.
				outMatches += (uint32_t)hadAnyMatches;
				return true;
			}

			//! Tries to match ids in \param queryIds with ids in \param archetypeIds given
			//! the comparison function \param func bool(Entity queryId, Entity archetypeId).
			//! \return True if there is a match, false otherwise.
			template <typename Op, typename CmpFunc>
			GAIA_NODISCARD bool match_inter(EntitySpan queryIds, EntitySpan archetypeIds, CmpFunc func) const {
				const auto archetypeIdsCnt = (uint32_t)archetypeIds.size();
				const auto queryIdsCnt = (uint32_t)queryIds.size();

				// Arrays are sorted so we can do linear intersection lookup
				uint32_t indices[2]{}; // 0 for query ids, 1 for archetype ids
				uint32_t matches = 0;

				// Ids in query and archetype are sorted.
				// Therefore, to match any two ids we perform a linear intersection forward loop.
				// The only exception are transitive ids in which case we need to start searching
				// form the start.
				// Finding just one match for any id in the query is enough to start checking
				// the next it. We only have 3 different operations - AND, OR, NOT.
				//
				// Example:
				// - query #1 ------------------------
				// queryIds    : 5, 10
				// archetypeIds: 1,  3,  5,  6,  7, 10
				// - query #2 ------------------------
				// queryIds    : 1, 10, 11
				// archetypeIds: 3,  5,  6,  7, 10, 15
				// -----------------------------------
				// indices[0]  : 0,  1,  2
				// indices[1]  : 0,  1,  2,  3,  4,  5
				//
				// For query #1:
				// We start matching 5 in the query with 1 in the archetype. They do not match.
				// We continue with 3 in the archetype. No match.
				// We continue with 5 in the archetype. Match.
				// We try to match 10 in the query with 6 in the archetype. No match.
				// ... etc.

				while (indices[0] < queryIdsCnt) {
					const auto idInQuery = queryIds[indices[0]];

					// For * and transitive ids we have to search from the start.
					if (idInQuery == All || idInQuery.id() == Is.id())
						indices[1] = 0;

					uint32_t queryIdMatches = 0;
					while (indices[1] < archetypeIdsCnt) {
						const auto idInArchetype = archetypeIds[indices[1]];

						// See if we have a match
						const auto res = func(idInQuery, idInArchetype);

						// Once a match is found we start matching with the next id in query.
						if (res.matched) {
							++indices[0];
							++indices[1];
							++queryIdMatches;

							// Only continue with the next iteration unless the given Op determines it is
							// no longer needed.
							if (!match_inter_eval_matches<Op>(queryIdMatches, matches))
								return false;

							goto next_query_id;
						} else {
							++indices[1];
						}
					}

					if (!match_inter_eval_matches<Op>(queryIdMatches, matches))
						return false;

					++indices[0];

				next_query_id:
					continue;
				}

				return Op::eval(queryIdsCnt, matches);
			}

			GAIA_NODISCARD static bool match_idQueryId_with_idArchetype(
					const World& w, uint32_t mask, uint32_t idQueryIdx, EntityId idQuery, EntityId idArchetype) {
				if ((mask & (1U << idQueryIdx)) == 0U)
					return idQuery == idArchetype;

				const auto qe = entity_from_id(w, idQuery);
				const auto ae = entity_from_id(w, idArchetype);
				return is(w, ae, qe);
			};

			struct IdCmpResult {
				bool matched;
			};

			GAIA_NODISCARD static IdCmpResult cmp_ids(Entity idInQuery, Entity idInArchetype) {
				return {idInQuery == idInArchetype};
			}

			GAIA_NODISCARD static IdCmpResult cmp_ids_pairs(Entity idInQuery, Entity idInArchetype) {
				if (idInQuery.pair()) {
					// all(Pair<All, All>) aka "any pair"
					if (idInQuery == Pair(All, All))
						return {true};

					// all(Pair<X, All>):
					//   X, AAA
					//   X, BBB
					//   ...
					//   X, ZZZ
					if (idInQuery.gen() == All.id())
						return {idInQuery.id() == idInArchetype.id()};

					// all(Pair<All, X>):
					//   AAA, X
					//   BBB, X
					//   ...
					//   ZZZ, X
					if (idInQuery.id() == All.id())
						return {idInQuery.gen() == idInArchetype.gen()};
				}

				return cmp_ids(idInQuery, idInArchetype);
			}

			GAIA_NODISCARD IdCmpResult cmp_ids_is(const Archetype& archetype, Entity idInQuery, Entity idInArchetype) const {
				auto archetypeIds = archetype.ids_view();

				// all(Pair<Is, X>)
				if (idInQuery.pair() && idInQuery.id() == Is.id()) {
					return {as_relations_trav_if(*m_ctx.w, idInQuery, [&](Entity relation) {
						const auto idx = core::get_index(archetypeIds, relation);
						// Stop at the first match
						return idx != BadIndex;
					})};
				}

				return cmp_ids(idInQuery, idInArchetype);
			}

			GAIA_NODISCARD IdCmpResult
			cmp_ids_is_pairs(const Archetype& archetype, Entity idInQuery, Entity idInArchetype) const {
				auto archetypeIds = archetype.ids_view();

				if (idInQuery.pair()) {
					// all(Pair<All, All>) aka "any pair"
					if (idInQuery == Pair(All, All))
						return {true};

					// all(Pair<Is, X>)
					if (idInQuery.id() == Is.id()) {
						// (Is, X) in archetype == (Is, X) in query
						if (idInArchetype == idInQuery)
							return {true};

						const auto e = entity_from_id(*m_ctx.w, idInQuery.gen());

						// If the archetype entity is an (Is, X) pair treat Is as X and try matching it with
						// entities inheriting from e.
						if (idInArchetype.id() == Is.id()) {
							const auto e2 = entity_from_id(*m_ctx.w, idInArchetype.gen());
							return {as_relations_trav_if(*m_ctx.w, e, [&](Entity relation) {
								return e2 == relation;
							})};
						}

						// Archetype entity is generic, try matching it with entities inheriting from e.
						return {as_relations_trav_if(*m_ctx.w, e, [&](Entity relation) {
							// Relation does not necessary match the sorted order of components in the archetype
							// so we need to search through all of its ids.
							const auto idx = core::get_index(archetypeIds, relation);
							// Stop at the first match
							return idx != BadIndex;
						})};
					}

					// all(Pair<All, X>):
					//   AAA, X
					//   BBB, X
					//   ...
					//   ZZZ, X
					if (idInQuery.id() == All.id()) {
						if (idInQuery.gen() == idInArchetype.gen())
							return {true};

						// If there are any Is pairs on the archetype we need to check if we match them
						if (archetype.pairs_is() > 0) {
							const auto e = entity_from_id(*m_ctx.w, idInQuery.gen());
							return {as_relations_trav_if(*m_ctx.w, e, [&](Entity relation) {
								// Relation does not necessary match the sorted order of components in the archetype
								// so we need to search through all of its ids.
								const auto idx = core::get_index(archetypeIds, relation);
								// Stop at the first match
								return idx != BadIndex;
							})};
						}

						// No match found
						return {false};
					}

					// all(Pair<X, All>):
					//   X, AAA
					//   X, BBB
					//   ...
					//   X, ZZZ
					if (idInQuery.gen() == All.id()) {
						return {idInQuery.id() == idInArchetype.id()};
					}
				}

				return cmp_ids(idInQuery, idInArchetype);
			}

			//! Tries to match entity ids in \param queryIds with those in \param archetype given
			//! the comparison function \param func.
			//! \return True on the first match, false otherwise.
			template <typename Op>
			GAIA_NODISCARD bool match_res(const Archetype& archetype, EntitySpan queryIds) const {
				// Archetype has no pairs we can compare ids directly.
				// This has better performance.
				if (archetype.pairs() == 0) {
					return match_inter<Op>(
							queryIds, archetype.ids_view(),
							// Cmp func
							[](Entity idInQuery, Entity idInArchetype) {
								return cmp_ids(idInQuery, idInArchetype);
							});
				}

				// Pairs are present, we have to evaluate.
				return match_inter<Op>(
						queryIds, archetype.ids_view(),
						// Cmp func
						[](Entity idInQuery, Entity idInArchetype) {
							return cmp_ids_pairs(idInQuery, idInArchetype);
						});
			}

			template <typename Op>
			GAIA_NODISCARD bool match_res_backtrack(const Archetype& archetype, EntitySpan queryIds) const {
				// Archetype has no pairs we can compare ids directly
				if (archetype.pairs() == 0) {
					return match_inter<Op>(
							queryIds, archetype.ids_view(),
							// cmp func
							[&](Entity idInQuery, Entity idInArchetype) {
								return cmp_ids_is(archetype, idInQuery, idInArchetype);
							});
				}

				return match_inter<Op>(
						queryIds, archetype.ids_view(),
						// cmp func
						[&](Entity idInQuery, Entity idInArchetype) {
							return cmp_ids_is_pairs(archetype, idInQuery, idInArchetype);
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

						// // Dynamic source
						// if (is_variable(p.src)) {
						// 	m_instructions.emplace_back(p.id, QueryOp::All);
						// 	continue;
						// }

						// Fixed source
						{
							p.srcArchetype = archetype_from_entity(*m_ctx.w, p.src);

							// Archetype needs to exist. If it does not we have nothing to do here.
							if (p.srcArchetype == nullptr) {
								m_instructions.clear();
								return;
							}
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

			void match_archetype_all(MatchingCtx& ctx) {
				const auto& entityToArchetypeMap = *ctx.pEntityToArchetypeMap;
				auto& matchesArr = *ctx.pMatchesArr;
				auto& matchesSet = *ctx.pMatchesSet;

				EntityLookupKey entityKey(ctx.ent);

				// For ALL we need all the archetypes to match. We start by checking
				// if the first one is registered in the world at all.
				const auto it = entityToArchetypeMap.find(entityKey);
				if (it == entityToArchetypeMap.end() || it->second.empty())
					return;

				auto& data = m_ctx.data;

				const auto& archetypes = it->second;
				const auto cache_it = data.lastMatchedArchetypeIdx_All.find(entityKey);
				uint32_t lastMatchedIdx = 0;
				if (cache_it == data.lastMatchedArchetypeIdx_All.end())
					data.lastMatchedArchetypeIdx_All.emplace(entityKey, archetypes.size());
				else {
					lastMatchedIdx = cache_it->second;
					cache_it->second = archetypes.size();
				}

				// For simple cases it is enough to add archetypes to cache right away
				if (ctx.idsToMatch.size() == 1) {
					for (uint32_t a = lastMatchedIdx; a < archetypes.size(); ++a) {
						auto* pArchetype = archetypes[a];
						matchesArr.emplace_back(pArchetype);
					}
				} else {
					for (uint32_t a = lastMatchedIdx; a < archetypes.size(); ++a) {
						auto* pArchetype = archetypes[a];

						if (matchesSet.contains(pArchetype))
							continue;
						if (!match_res<OpAnd>(*pArchetype, ctx.idsToMatch))
							continue;

						matchesSet.emplace(pArchetype);
						matchesArr.emplace_back(pArchetype);
					}
				}
			}

			void match_archetype_all_as(MatchingCtx& ctx) {
				const auto& entityToArchetypeMap = *ctx.pEntityToArchetypeMap;
				const auto& allArchetypes = *ctx.pAllArchetypes;
				auto& matchesArr = *ctx.pMatchesArr;
				auto& matchesSet = *ctx.pMatchesSet;

				// For ALL we need all the archetypes to match. We start by checking
				// if the first one is registered in the world at all.
				const ArchetypeDArray* pSrcArchetypes = nullptr;

				EntityLookupKey entityKey = EntityBadLookupKey;

				if (ctx.ent.id() == Is.id()) {
					ctx.ent = EntityBad;
					pSrcArchetypes = &allArchetypes;
				} else {
					entityKey = EntityLookupKey(ctx.ent);
					const auto it = entityToArchetypeMap.find(entityKey);
					if (it == entityToArchetypeMap.end() || it->second.empty())
						return;
					pSrcArchetypes = &it->second;
				}

				auto& data = m_ctx.data;

				const auto& archetypes = *pSrcArchetypes;
				const auto cache_it = data.lastMatchedArchetypeIdx_All.find(entityKey);
				uint32_t lastMatchedIdx = 0;
				if (cache_it == data.lastMatchedArchetypeIdx_All.end())
					data.lastMatchedArchetypeIdx_All.emplace(entityKey, archetypes.size());
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
					if (!match_res_backtrack<OpAnd>(*pArchetype, ctx.idsToMatch))
						continue;

					matchesSet.emplace(pArchetype);
					matchesArr.emplace_back(pArchetype);
				}
				//}
			}

			void match_archetype_one(MatchingCtx& ctx) {
				const auto& entityToArchetypeMap = *ctx.pEntityToArchetypeMap;
				auto& matchesArr = *ctx.pMatchesArr;
				auto& matchesSet = *ctx.pMatchesSet;

				EntityLookupKey entityKey(ctx.ent);

				// For ANY we need at least one archetypes to match.
				// However, because any of them can match, we need to check them all.
				// Iterating all of them is caller's responsibility.
				const auto it = entityToArchetypeMap.find(entityKey);
				if (it == entityToArchetypeMap.end() || it->second.empty())
					return;

				auto& data = m_ctx.data;

				const auto& archetypes = it->second;
				const auto cache_it = data.lastMatchedArchetypeIdx_Any.find(entityKey);
				uint32_t lastMatchedIdx = 0;
				if (cache_it == data.lastMatchedArchetypeIdx_Any.end())
					data.lastMatchedArchetypeIdx_Any.emplace(entityKey, archetypes.size());
				else {
					lastMatchedIdx = cache_it->second;
					cache_it->second = archetypes.size();
				}

				// For simple cases it is enough to add archetypes to cache right away
				if (ctx.idsToMatch.size() == 1) {
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
						if (!match_res<OpOr>(*pArchetype, ctx.idsToMatch))
							continue;

						matchesSet.emplace(pArchetype);
						matchesArr.emplace_back(pArchetype);
					}
				}
			}

			void match_archetype_one_as(MatchingCtx& ctx) {
				const auto& entityToArchetypeMap = *ctx.pEntityToArchetypeMap;
				const auto& allArchetypes = *ctx.pAllArchetypes;
				auto& matchesArr = *ctx.pMatchesArr;
				auto& matchesSet = *ctx.pMatchesSet;

				// For ANY we need at least one archetypes to match.
				// However, because any of them can match, we need to check them all.
				// Iterating all of them is caller's responsibility.

				const ArchetypeDArray* pSrcArchetypes = nullptr;

				EntityLookupKey entityKey = EntityBadLookupKey;

				if (ctx.ent.id() == Is.id()) {
					ctx.ent = EntityBad;
					pSrcArchetypes = &allArchetypes;
				} else {
					entityKey = EntityLookupKey(ctx.ent);
					const auto it = entityToArchetypeMap.find(entityKey);
					if (it == entityToArchetypeMap.end() || it->second.empty())
						return;
					pSrcArchetypes = &it->second;
				}

				auto& data = m_ctx.data;

				const auto& archetypes = *pSrcArchetypes;
				const auto cache_it = data.lastMatchedArchetypeIdx_Any.find(entityKey);
				uint32_t lastMatchedIdx = 0;
				if (cache_it == data.lastMatchedArchetypeIdx_Any.end())
					data.lastMatchedArchetypeIdx_Any.emplace(entityKey, archetypes.size());
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
					if (!match_res_backtrack<OpOr>(*pArchetype, ctx.idsToMatch))
						continue;

					matchesSet.emplace(pArchetype);
					matchesArr.emplace_back(pArchetype);
				}
				//}
			}

			void match_archetype_no(MatchingCtx& ctx) {
				const auto& archetypes = *ctx.pAllArchetypes;
				auto& matchesArr = *ctx.pMatchesArr;
				auto& matchesSet = *ctx.pMatchesSet;

				// For NO we need to search among all archetypes.
				auto& data = m_ctx.data;

				const auto cache_it = data.lastMatchedArchetypeIdx_All.find(EntityBadLookupKey);
				uint32_t lastMatchedIdx = 0;
				if (cache_it == data.lastMatchedArchetypeIdx_All.end())
					data.lastMatchedArchetypeIdx_All.emplace(EntityBadLookupKey, 0U);
				else
					lastMatchedIdx = cache_it->second;
				cache_it->second = archetypes.size();

				for (uint32_t a = lastMatchedIdx; a < archetypes.size(); ++a) {
					auto* pArchetype = archetypes[a];
					if (matchesSet.contains(pArchetype))
						continue;
					if (!match_res<OpNo>(*pArchetype, ctx.idsToMatch))
						continue;

					matchesSet.emplace(pArchetype);
					matchesArr.emplace_back(pArchetype);
				}
			}

			void match_archetype_no_as(MatchingCtx& ctx) {
				const auto& archetypes = *ctx.pAllArchetypes;
				auto& matchesArr = *ctx.pMatchesArr;
				auto& matchesSet = *ctx.pMatchesSet;

				// For NO we need to search among all archetypes.
				auto& data = m_ctx.data;

				const auto cache_it = data.lastMatchedArchetypeIdx_All.find(EntityBadLookupKey);
				uint32_t lastMatchedIdx = 0;
				if (cache_it == data.lastMatchedArchetypeIdx_All.end())
					data.lastMatchedArchetypeIdx_All.emplace(EntityBadLookupKey, 0U);
				else
					lastMatchedIdx = cache_it->second;
				cache_it->second = archetypes.size();

				for (uint32_t a = lastMatchedIdx; a < archetypes.size(); ++a) {
					auto* pArchetype = archetypes[a];

					if (matchesSet.contains(pArchetype))
						continue;
					if (!match_res_backtrack<OpNo>(*pArchetype, ctx.idsToMatch))
						continue;

					matchesSet.emplace(pArchetype);
					matchesArr.emplace_back(pArchetype);
				}
			}

			void do_match_all(MatchingCtx& ctx) {
				// First viable item is not related to an Is relationship
				if (ctx.as_mask_0 + ctx.as_mask_1 == 0U) {
					match_archetype_all(ctx);
				} else
				// First viable item is related to an Is relationship.
				// In this case we need to gather all related archetypes and evaluate one-by-one (backtracking).
				{
					match_archetype_all_as(ctx);
				}
			}

			void do_match_one(MatchingCtx& ctx) {
				// First viable item is not related to an Is relationship
				if (ctx.as_mask_0 + ctx.as_mask_1 == 0U) {
					match_archetype_one(ctx);
				}
				// First viable item is related to an Is relationship.
				// In this case we need to gather all related archetypes.
				else {
					match_archetype_one_as(ctx);
				}
			}

			bool do_match_one(MatchingCtx& ctx, const Archetype& archetype) {
				// First viable item is not related to an Is relationship
				if (ctx.as_mask_0 + ctx.as_mask_1 == 0U)
					return match_res<OpOr>(archetype, ctx.idsToMatch);

				// First viable item is related to an Is relationship.
				// In this case we need to gather all related archetypes.
				return match_res_backtrack<OpOr>(archetype, ctx.idsToMatch);
			}

			void do_match_no(MatchingCtx& ctx) {
				ctx.pMatchesSet->clear();

				if (ctx.as_mask_0 + ctx.as_mask_1 == 0U)
					match_archetype_no(ctx);
				else
					match_archetype_no_as(ctx);
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
				static cnt::set<Archetype*> s_tmpArchetypeMatchesSet;
				static ArchetypeDArray s_tmpArchetypeMatchesArr;

				struct CleanUpTmpArchetypeMatches {
					~CleanUpTmpArchetypeMatches() {
						s_tmpArchetypeMatchesSet.clear();
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

				MatchingCtx ctx;
				ctx.pAllArchetypes = &allArchetypes;
				ctx.pEntityToArchetypeMap = &entityToArchetypeMap;
				ctx.pMatchesArr = &s_tmpArchetypeMatchesArr;
				ctx.pMatchesSet = &s_tmpArchetypeMatchesSet;
				ctx.as_mask_0 = data.as_mask_0;
				ctx.as_mask_1 = data.as_mask_1;

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
					ctx.ent = ids_all[0];
					ctx.idsToMatch = std::span{ids_all.data(), ids_all.size()};
					do_match_all(ctx);

					// No ALL matches were found. We can quit right away.
					if (s_tmpArchetypeMatchesArr.empty())
						return;
				}

				if (!terms_any.empty()) {
					ctx.idsToMatch = std::span{ids_any.data(), ids_any.size()};

					if (ids_all.empty()) {
						// We didn't try to match any ALL items.
						// We need to search among all archetypes.

						// Try find matches with optional components.
						GAIA_EACH(ids_any) {
							ctx.ent = ids_any[i];
							do_match_one(ctx);
						}
					} else {
						// We tried to match ALL items. Only search among those we already found.
						// No last matched idx update is necessary because all ids were already checked
						// during the ALL pass.
						for (uint32_t i = 0; i < s_tmpArchetypeMatchesArr.size();) {
							auto* pArchetype = s_tmpArchetypeMatchesArr[i];

							GAIA_FOR_((uint32_t)ids_any.size(), j) {
								if (do_match_one(ctx, *pArchetype))
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

				auto addTmpArchetypesToCache = [&]() {
					// Write the temporary matches to cache
					for (auto* pArchetype: s_tmpArchetypeMatchesArr)
						add_archetype_to_cache(pArchetype);
				};
				auto addTmpArchetypesToCacheIfNoMatch = [&]() {
					// Write the temporary matches to cache if no match with NO is found
					for (auto* pArchetype: s_tmpArchetypeMatchesArr) {
						if (!match_res<OpNo>(*pArchetype, ctx.idsToMatch))
							continue;
						add_archetype_to_cache(pArchetype);
					}
				};

				// Make sure there is no match with NOT items.
				if (!terms_not.empty()) {
					ctx.idsToMatch = std::span{ids_not.data(), ids_not.size()};

					// We searched for nothing more than NOT matches
					if (s_tmpArchetypeMatchesArr.empty()) {
						do_match_no(ctx);
						addTmpArchetypesToCache();
					} else {
						addTmpArchetypesToCacheIfNoMatch();
					}
				} else {
					addTmpArchetypesToCache();
				}

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

					cacheData.indices[i] = (uint8_t)compIdx;
				}
				return cacheData;
			}

			void add_archetype_to_cache_no_grouping(Archetype* pArchetype) {
				m_archetypeCache.push_back(pArchetype);
				m_archetypeCacheData.push_back(create_cache_data(pArchetype));
			}

			void add_archetype_to_cache_w_grouping(Archetype* pArchetype) {
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

				m_archetypeCache.push_back(pArchetype);
				m_archetypeCacheData.push_back(GAIA_MOV(cacheData));
			}

			void add_archetype_to_cache(Archetype* pArchetype) {
				if (m_ctx.data.groupBy != EntityBad)
					add_archetype_to_cache_w_grouping(pArchetype);
				else
					add_archetype_to_cache_no_grouping(pArchetype);
			}

			void del_archetype_from_cache(uint32_t idx) {
				auto* pArchetype = m_archetypeCache[idx];
				core::erase_fast(m_archetypeCache, idx);
				core::erase_fast(m_archetypeCacheData, idx);

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
