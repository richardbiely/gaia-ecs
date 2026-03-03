#pragma once
#include "gaia/config/config.h"

#include <cstdint>
#include <type_traits>

#include "gaia/cnt/darray.h"
#include "gaia/cnt/sarray.h"
#include "gaia/cnt/set.h"
#include "gaia/config/profiler.h"
#include "gaia/core/utility.h"
#include "gaia/ecs/api.h"
#include "gaia/ecs/archetype.h"
#include "gaia/ecs/archetype_common.h"
#include "gaia/ecs/id.h"
#include "gaia/ecs/query_common.h"
#include "gaia/ecs/query_mask.h"
#include "gaia/ser/ser_binary.h"

namespace gaia {
	namespace ecs {
		using EntityToArchetypeMap = cnt::map<EntityLookupKey, ArchetypeDArray>;

		namespace vm {

			enum class MatchingStyle { Simple, Wildcard, Complex };

			struct MatchingCtx {
				// Setup up externally
				//////////////////////////////////

				//! World
				const World* pWorld;
				//! Entities for which we run the VM. If empty, we run against all of them.
				EntitySpan targetEntities;
				//! entity -> archetypes mapping
				const EntityToArchetypeMap* pEntityToArchetypeMap;
				//! Array of all archetypes in the world
				std::span<const Archetype*> allArchetypes;
				//! Set of already matched archetypes. Reset before each exec().
				cnt::set<const Archetype*>* pMatchesSet;
				//! Array of already matches archetypes. Reset before each exec().
				cnt::darr<const Archetype*>* pMatchesArr;
				//! Idx of the last matched archetype against the ALL opcode
				QueryArchetypeCacheIndexMap* pLastMatchedArchetypeIdx_All;
				//! Idx of the last matched archetype against the ANY opcode
				QueryArchetypeCacheIndexMap* pLastMatchedArchetypeIdx_Any;
				//! Idx of the last matched archetype against the NOT opcode
				QueryArchetypeCacheIndexMap* pLastMatchedArchetypeIdx_Not;
				//! Mask for speeding up simple query matching
				QueryMask queryMask;
				//! Mask for items with Is relationship pair.
				//! If the id is a pair, the first part (id) is written here.
				uint32_t as_mask_0;
				//! Mask for items with Is relationship pair.
				//! If the id is a pair, the second part (gen) is written here.
				uint32_t as_mask_1;
				//! Flags copied over from QueryCtx::Data
				uint8_t flags;
				//! ANY group was already satisfied by source terms.
				bool skipAny = false;

				// For the opcode compiler to modify
				//////////////////////////////////////

				//! Entity to match
				Entity ent;
				//! List of entity ids in a query to consider
				EntitySpan idsToMatch;
				//! Current stack position (program counter)
				uint32_t pc;
			};

			inline std::span<const Archetype*> fetch_archetypes_for_select(
					const EntityToArchetypeMap& map, std::span<const Archetype*> arr, Entity ent, const EntityLookupKey& key) {
				GAIA_ASSERT(key != EntityBadLookupKey);

				if (ent == Pair(All, All))
					return arr;

				const auto it = map.find(key);
				if (it == map.end() || it->second.empty())
					return {};

				return std::span((const Archetype**)it->second.data(), it->second.size());
			}

			inline std::span<const Archetype*> fetch_archetypes_for_select(
					const EntityToArchetypeMap& map, std::span<const Archetype*> arr, Entity ent, Entity src) {
				GAIA_ASSERT(src != EntityBad);

				return fetch_archetypes_for_select(map, arr, ent, EntityLookupKey(src));
			}

			namespace detail {
				enum class EOpcode : uint8_t {
					//! X
					All_Simple,
					All_Wildcard,
					All_Complex,
					//! ?X
					Any_NoAll,
					Any_WithAll,
					//! !X
					Not_Simple,
					Not_Wildcard,
					Not_Complex
				};

				using VmLabel = uint16_t;

				struct CompiledOp {
					//! Opcode to execute
					EOpcode opcode;
					//! Stack position to go to if the opcode returns true
					VmLabel pc_ok;
					//! Stack position to go to if the opcode returns false
					VmLabel pc_fail;
				};

				struct QueryCompileCtx {
					cnt::darray<CompiledOp> ops;
					//! Array of ops that can be evaluated with a ALL opcode
					cnt::sarray_ext<Entity, MAX_ITEMS_IN_QUERY> ids_all;
					//! Array of ops that can be evaluated with a ANY opcode
					cnt::sarray_ext<Entity, MAX_ITEMS_IN_QUERY> ids_any;
					//! Array of ops that can be evaluated with a NOT opcode
					cnt::sarray_ext<Entity, MAX_ITEMS_IN_QUERY> ids_not;
					//! Source lookup terms for ALL
					QueryTermArray terms_all_src;
					//! Source lookup terms for ANY
					QueryTermArray terms_any_src;
					//! Source lookup terms for NOT
					QueryTermArray terms_not_src;
					//! Variable terms for ALL
					QueryTermArray terms_all_var;
					//! Variable terms for ANY
					QueryTermArray terms_any_var;
					//! Variable terms for NOT
					QueryTermArray terms_not_var;

					GAIA_NODISCARD bool has_source_terms() const {
						return !terms_all_src.empty() || !terms_any_src.empty() || !terms_not_src.empty();
					}

					GAIA_NODISCARD bool has_variable_terms() const {
						return !terms_all_var.empty() || !terms_any_var.empty() || !terms_not_var.empty();
					}
				};

				inline uint32_t handle_last_archetype_match(
						QueryArchetypeCacheIndexMap* pCont, EntityLookupKey entityKey, uint32_t srcArchetypeCnt) {
					const auto cache_it = pCont->find(entityKey);
					uint32_t lastMatchedIdx = 0;
					if (cache_it == pCont->end())
						pCont->emplace(entityKey, srcArchetypeCnt);
					else {
						lastMatchedIdx = cache_it->second;
						cache_it->second = srcArchetypeCnt;
					}
					return lastMatchedIdx;
				}

				// Operator ALL (used by query::all)
				struct OpAll {
					static bool check_mask(const QueryMask& maskArchetype, const QueryMask& maskQuery) {
						return match_entity_mask(maskArchetype, maskQuery);
					}
					static void restart([[maybe_unused]] uint32_t& idx) {}
					static bool can_continue(bool hasMatch) {
						return hasMatch;
					}
					static bool eval(uint32_t expectedMatches, uint32_t totalMatches) {
						return expectedMatches == totalMatches;
					}
					static uint32_t handle_last_match(MatchingCtx& ctx, EntityLookupKey entityKey, uint32_t srcArchetypeCnt) {
						return handle_last_archetype_match(ctx.pLastMatchedArchetypeIdx_All, entityKey, srcArchetypeCnt);
					}
				};
				// Operator OR (used by query::any)
				struct OpAny {
					static bool check_mask(const QueryMask& maskArchetype, const QueryMask& maskQuery) {
						return match_entity_mask(maskArchetype, maskQuery);
					}
					static void restart([[maybe_unused]] uint32_t& idx) {}
					static bool can_continue(bool hasMatch) {
						return hasMatch;
					}
					static bool eval(uint32_t expectedMatches, uint32_t totalMatches) {
						(void)expectedMatches;
						return totalMatches > 0;
					}
					static uint32_t handle_last_match(MatchingCtx& ctx, EntityLookupKey entityKey, uint32_t srcArchetypeCnt) {
						return handle_last_archetype_match(ctx.pLastMatchedArchetypeIdx_Any, entityKey, srcArchetypeCnt);
					}
				};
				// Operator NOT (used by query::no)
				struct OpNo {
					static bool check_mask(const QueryMask& maskArchetype, const QueryMask& maskQuery) {
						return !match_entity_mask(maskArchetype, maskQuery);
					}
					static void restart(uint32_t& idx) {
						idx = 0;
					}
					static bool can_continue(bool hasMatch) {
						return !hasMatch;
					}
					static bool eval(uint32_t expectedMatches, uint32_t totalMatches) {
						(void)expectedMatches;
						return totalMatches == 0;
					}
					static uint32_t handle_last_match(MatchingCtx& ctx, EntityLookupKey entityKey, uint32_t srcArchetypeCnt) {
						return handle_last_archetype_match(ctx.pLastMatchedArchetypeIdx_Not, entityKey, srcArchetypeCnt);
					}
				};

				inline void add_all_archetypes(MatchingCtx& ctx) {
					for (const auto* pArchetype: ctx.allArchetypes) {
						if (ctx.pMatchesSet->contains(pArchetype))
							continue;

						ctx.pMatchesSet->emplace(pArchetype);
						ctx.pMatchesArr->emplace_back(pArchetype);
					}
				}

				template <typename OpKind>
				inline bool match_inter_eval_matches(uint32_t queryIdMarches, uint32_t& outMatches) {
					const bool hadAnyMatches = queryIdMarches > 0;

					// We finished checking matches with an id from query.
					// We need to check if we have sufficient amount of results in the run.
					if (!OpKind::can_continue(hadAnyMatches))
						return false;

					// No matter the amount of matches we only care if at least one
					// match happened with the id from query.
					outMatches += (uint32_t)hadAnyMatches;
					return true;
				}

				//! Tries to match ids in @a queryIds with ids in @a archetypeIds given
				//! the comparison function @a func bool(Entity queryId, Entity archetypeId).
				//! \tparam OpKind Operation kind
				//! \tparam CmpFunc Comparison function
				//! \param queryIds Entity ids inside archetype
				//! \param archetypeIds Entity ids inside archetype
				//! \param func Comparison function
				//! \return True if there is a match, false otherwise.
				template <typename OpKind, typename CmpFunc>
				GAIA_NODISCARD inline bool match_inter(EntitySpan queryIds, EntitySpan archetypeIds, CmpFunc func) {
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
					// the next it. We only have 3 different operations - ALL, OR, NOT.
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
								if (!match_inter_eval_matches<OpKind>(queryIdMatches, matches))
									return false;

								goto next_query_id;
							} else {
								++indices[1];
							}
						}

						if (!match_inter_eval_matches<OpKind>(queryIdMatches, matches))
							return false;

						++indices[0];
						// Make sure to continue from the right index on the archetype array.
						// Some operators can keep moving forward (AND, ANY), but NOT needs to start
						// matching from the beginning again if the previous query operator didn't find a match.
						OpKind::restart(indices[1]);

					next_query_id:
						continue;
					}

					return OpKind::eval(queryIdsCnt, matches);
				}

				struct IdCmpResult {
					bool matched;
				};

				GAIA_NODISCARD inline IdCmpResult cmp_ids(Entity idInQuery, Entity idInArchetype) {
					return {idInQuery == idInArchetype};
				}

				GAIA_NODISCARD inline IdCmpResult cmp_ids_pairs(Entity idInQuery, Entity idInArchetype) {
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

					// 1:1 match needed for non-pairs
					return cmp_ids(idInQuery, idInArchetype);
				}

				GAIA_NODISCARD inline IdCmpResult
				cmp_ids_is(const World& w, const Archetype& archetype, Entity idInQuery, Entity idInArchetype) {
					// all(Pair<Is, X>)
					if (idInQuery.pair() && idInQuery.id() == Is.id()) {
						auto archetypeIds = archetype.ids_view();
						return {
								idInQuery.gen() == idInArchetype.id() || // X vs Id
								as_relations_trav_if(w, idInQuery, [&](Entity relation) {
									const auto idx = core::get_index(archetypeIds, relation);
									// Stop at the first match
									return idx != BadIndex;
								})};
					}

					// 1:1 match needed for non-pairs
					return cmp_ids(idInQuery, idInArchetype);
				}

				GAIA_NODISCARD inline IdCmpResult
				cmp_ids_is_pairs(const World& w, const Archetype& archetype, Entity idInQuery, Entity idInArchetype) {
					if (idInQuery.pair()) {
						// all(Pair<All, All>) aka "any pair"
						if (idInQuery == Pair(All, All))
							return {true};

						// all(Pair<Is, X>)
						if (idInQuery.id() == Is.id()) {
							// (Is, X) in archetype == (Is, X) in query
							if (idInArchetype == idInQuery)
								return {true};

							auto archetypeIds = archetype.ids_view();
							const auto eQ = entity_from_id(w, idInQuery.gen());
							if (eQ == idInArchetype)
								return {true};

							// If the archetype entity is an (Is, X) pair treat Is as X and try matching it with
							// entities inheriting from e.
							if (idInArchetype.id() == Is.id()) {
								const auto eA = entity_from_id(w, idInArchetype.gen());
								if (eA == eQ)
									return {true};

								return {as_relations_trav_if(w, eQ, [eA](Entity relation) {
									return eA == relation;
								})};
							}

							// Archetype entity is generic, try matching it with entities inheriting from e.
							return {as_relations_trav_if(w, eQ, [&archetypeIds](Entity relation) {
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
								auto archetypeIds = archetype.ids_view();

								const auto e = entity_from_id(w, idInQuery.gen());
								return {as_relations_trav_if(w, e, [&](Entity relation) {
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

					// 1:1 match needed for non-pairs
					return cmp_ids(idInQuery, idInArchetype);
				}

				//! Tries to match entity ids in @a queryIds with those in @a archetype.
				//! Does not consider Is relationships.
				//! \tparam OpKind Operation kind
				//! \param archetype Archetype checked against
				//! \param queryIds Entity ids to match
				//! \return True on the first match, false otherwise.
				template <typename OpKind>
				GAIA_NODISCARD inline bool match_res(const Archetype& archetype, EntitySpan queryIds) {
					// Archetype has no pairs we can compare ids directly.
					// This has better performance.
					if (archetype.pairs() == 0) {
						return match_inter<OpKind>(
								queryIds, archetype.ids_view(),
								// Cmp func
								[](Entity idInQuery, Entity idInArchetype) {
									return cmp_ids(idInQuery, idInArchetype);
								});
					}

					// Pairs are present, we have to evaluate.
					return match_inter<OpKind>(
							queryIds, archetype.ids_view(),
							// Cmp func
							[](Entity idInQuery, Entity idInArchetype) {
								return cmp_ids_pairs(idInQuery, idInArchetype);
							});
				}

				//! Tries to match entity ids in @a queryIds with those in @a archetype.
				//! \tparam OpKind Kind of VM operation
				//! \param w Parent world
				//! \param archetype Archetype checked against
				//! \param queryIds Entity ids to match
				//! \return True on the first match, false otherwise.
				template <typename OpKind>
				GAIA_NODISCARD inline bool match_res_as(const World& w, const Archetype& archetype, EntitySpan queryIds) {
					// Archetype has no pairs we can compare ids directly
					if (archetype.pairs() == 0) {
						return match_inter<OpKind>(
								queryIds, archetype.ids_view(),
								// cmp func
								[&](Entity idInQuery, Entity idInArchetype) {
									return cmp_ids_is(w, archetype, idInQuery, idInArchetype);
								});
					}

					return match_inter<OpKind>(
							queryIds, archetype.ids_view(),
							// cmp func
							[&](Entity idInQuery, Entity idInArchetype) {
								return cmp_ids_is_pairs(w, archetype, idInQuery, idInArchetype);
							});
				}

				GAIA_NODISCARD inline bool match_single_id_on_archetype(const World& w, const Archetype& archetype, Entity id) {
					const Entity ids[1] = {id};
					return match_res_as<OpAny>(w, archetype, EntitySpan{ids, 1});
				}

				template <typename Func>
				GAIA_NODISCARD inline bool each_lookup_source(const World& w, const QueryTerm& term, Entity sourceEntity, Func&& func) {
					if (!valid(w, sourceEntity))
						return false;

					const bool includeSelf = query_trav_has(term.travKind, QueryTravKind::Self);
					const bool includeUp = query_trav_has(term.travKind, QueryTravKind::Up) && term.entTrav != EntityBad &&
																 term.travDepth != 0;

					if (includeSelf && func(sourceEntity))
						return true;

					if (!includeUp)
						return false;

					constexpr uint32_t MaxTraversalDepth = 2048;
					const uint32_t maxDepth =
							term.travDepth == QueryTermOptions::TravDepthUnlimited ? MaxTraversalDepth : (uint32_t)term.travDepth;

					Entity source = sourceEntity;
					for (uint32_t depth = 0; depth < maxDepth; ++depth) {
						const auto next = target(w, source, term.entTrav);
						if (next == EntityBad || next == source)
							break;

						source = next;
						if (func(source))
							return true;
					}

					return false;
				}

				GAIA_NODISCARD inline bool match_source_term(const World& w, const QueryTerm& term) {
					auto match_source_entity = [&](Entity source) {
						if (!valid(w, source))
							return false;

						auto* pArchetype = archetype_from_entity(w, source);
						if (pArchetype == nullptr)
							return false;

						return match_single_id_on_archetype(w, *pArchetype, term.id);
					};

					return each_lookup_source(w, term, term.src, match_source_entity);
				}

				struct VarBindings {
					static constexpr uint32_t VarCnt = 8;
					cnt::sarray<Entity, VarCnt> values{};
					uint8_t mask = 0;
				};

				GAIA_NODISCARD inline bool is_var_entity(Entity entity) {
					return is_variable(EntityId(entity.id()));
				}

				GAIA_NODISCARD inline uint32_t var_index(Entity varEntity) {
					GAIA_ASSERT(is_var_entity(varEntity));
					return (uint32_t)(varEntity.id() - Var0.id());
				}

				GAIA_NODISCARD inline bool var_is_bound(const VarBindings& vars, Entity varEntity) {
					const auto idx = var_index(varEntity);
					return (vars.mask & (uint8_t(1) << idx)) != 0;
				}

				GAIA_NODISCARD inline bool bind_var(VarBindings& vars, Entity varEntity, Entity value) {
					const auto idx = var_index(varEntity);
					const auto bit = (uint8_t(1) << idx);
					if ((vars.mask & bit) != 0)
						return vars.values[idx].id() == value.id();

					vars.values[idx] = value;
					vars.mask |= bit;
					return true;
				}

				GAIA_NODISCARD inline bool match_token(VarBindings& vars, Entity token, Entity value, bool pairSide) {
					if (pairSide && token.id() == All.id())
						return true;

					if (!is_var_entity(token))
						return token.id() == value.id();

					return bind_var(vars, token, value);
				}

				inline void collect_id_matches(
						const World& w, const Archetype& archetype, Entity queryId, const VarBindings& varsIn,
						cnt::sarray_ext<VarBindings, VarBindings::VarCnt>& outStates) {
					auto archetypeIds = archetype.ids_view();
					const auto cnt = (uint32_t)archetypeIds.size();

					if (!queryId.pair()) {
						GAIA_FOR(cnt) {
							const auto idInArchetype = archetypeIds[i];
							if (idInArchetype.pair())
								continue;

							const auto value = entity_from_id(w, idInArchetype.id());
							if (value == EntityBad)
								continue;

							auto vars = varsIn;
							if (!match_token(vars, queryId, value, false))
								continue;

							outStates.emplace_back(vars);
						}
						return;
					}

					GAIA_FOR(cnt) {
						const auto idInArchetype = archetypeIds[i];
						if (!idInArchetype.pair())
							continue;

						const auto rel = entity_from_id(w, idInArchetype.id());
						const auto tgt = entity_from_id(w, idInArchetype.gen());
						const auto queryRel = entity_from_id(w, queryId.id());
						const auto queryTgt = entity_from_id(w, queryId.gen());
						if (rel == EntityBad || tgt == EntityBad)
							continue;
						if (queryRel == EntityBad || queryTgt == EntityBad)
							continue;

						auto vars = varsIn;
						if (!match_token(vars, queryRel, rel, true))
							continue;
						if (!match_token(vars, queryTgt, tgt, true))
							continue;

						outStates.emplace_back(vars);
					}
				}

				inline void collect_term_matches(
						const World& w, const Archetype& candidateArchetype, const QueryTerm& term, const VarBindings& varsIn,
						cnt::sarray_ext<VarBindings, VarBindings::VarCnt>& outStates) {
					auto collect_on_source = [&](Entity sourceEntity, const VarBindings& vars) {
						each_lookup_source(w, term, sourceEntity, [&](Entity source) {
							auto* pSrcArchetype = archetype_from_entity(w, source);
							if (pSrcArchetype != nullptr)
								collect_id_matches(w, *pSrcArchetype, term.id, vars, outStates);

							return false;
						});
					};

					if (term.src == EntityBad) {
						collect_id_matches(w, candidateArchetype, term.id, varsIn, outStates);
						return;
					}

					if (is_var_entity(term.src)) {
						if (!var_is_bound(varsIn, term.src))
							return;

						const auto source = varsIn.values[var_index(term.src)];
						collect_on_source(source, varsIn);
						return;
					}

					collect_on_source(term.src, varsIn);
				}

				GAIA_NODISCARD inline bool
				term_has_match(const World& w, const Archetype& archetype, const QueryTerm& term, const VarBindings& varsIn) {
					cnt::sarray_ext<VarBindings, VarBindings::VarCnt> states;
					collect_term_matches(w, archetype, term, varsIn, states);
					return !states.empty();
				}

				template <typename OpKind, MatchingStyle Style>
				inline void match_archetype_inter(MatchingCtx& ctx, std::span<const Archetype*> archetypes) {
					auto& matchesArr = *ctx.pMatchesArr;
					auto& matchesSet = *ctx.pMatchesSet;

					if constexpr (Style == MatchingStyle::Complex) {
						for (const auto* pArchetype: archetypes) {
							if (matchesSet.contains(pArchetype))
								continue;

							if (!match_res_as<OpKind>(*ctx.pWorld, *pArchetype, ctx.idsToMatch))
								continue;

							matchesSet.emplace(pArchetype);
							matchesArr.emplace_back(pArchetype);
						}
					}
#if GAIA_USE_PARTITIONED_BLOOM_FILTER >= 0
					else if constexpr (Style == MatchingStyle::Simple) {
						for (const auto* pArchetype: archetypes) {
							if (matchesSet.contains(pArchetype))
								continue;

							// Try early exit
							if (!OpKind::check_mask(pArchetype->queryMask(), ctx.queryMask))
								continue;

							if (!match_res<OpKind>(*pArchetype, ctx.idsToMatch))
								continue;

							matchesSet.emplace(pArchetype);
							matchesArr.emplace_back(pArchetype);
						}
					}
#endif
					else {
						for (const auto* pArchetype: archetypes) {
							if (matchesSet.contains(pArchetype))
								continue;

							if (!match_res<OpKind>(*pArchetype, ctx.idsToMatch))
								continue;

							matchesSet.emplace(pArchetype);
							matchesArr.emplace_back(pArchetype);
						}
					}
				}

				template <typename OpKind, MatchingStyle Style>
				inline void
				match_archetype_inter(MatchingCtx& ctx, EntityLookupKey entityKey, std::span<const Archetype*> archetypes) {
					uint32_t lastMatchedIdx = OpKind::handle_last_match(ctx, entityKey, (uint32_t)archetypes.size());
					if (lastMatchedIdx >= archetypes.size())
						return;

					auto archetypesNew = std::span(&archetypes[lastMatchedIdx], archetypes.size() - lastMatchedIdx);
					match_archetype_inter<OpKind, Style>(ctx, archetypesNew);
				}

				template <MatchingStyle Style>
				inline void match_archetype_all(MatchingCtx& ctx) {
					if constexpr (Style == MatchingStyle::Complex) {
						// For ALL we need all the archetypes to match. We start by checking
						// if the first one is registered in the world at all.
						if (ctx.ent.id() == Is.id()) {
							ctx.ent = EntityBad;
							match_archetype_inter<OpAll, Style>(ctx, EntityBadLookupKey, ctx.allArchetypes);
						} else {
							auto entityKey = EntityLookupKey(ctx.ent);

							auto archetypes =
									fetch_archetypes_for_select(*ctx.pEntityToArchetypeMap, ctx.allArchetypes, ctx.ent, entityKey);
							if (archetypes.empty())
								return;

							match_archetype_inter<OpAll, Style>(ctx, entityKey, archetypes);
						}
					} else {
						auto entityKey = EntityLookupKey(ctx.ent);

						// For ALL we need all the archetypes to match. We start by checking
						// if the first one is registered in the world at all.
						auto archetypes =
								fetch_archetypes_for_select(*ctx.pEntityToArchetypeMap, ctx.allArchetypes, ctx.ent, entityKey);
						if (archetypes.empty())
							return;

						match_archetype_inter<OpAll, Style>(ctx, entityKey, archetypes);
					}
				}

				inline void match_archetype_any(MatchingCtx& ctx) {
					EntityLookupKey entityKey(ctx.ent);

					// For ANY we need at least one archetype to match.
					// However, because any of them can match, we need to check them all.
					// Iterating all of them is caller's responsibility.
					auto archetypes =
							fetch_archetypes_for_select(*ctx.pEntityToArchetypeMap, ctx.allArchetypes, ctx.ent, entityKey);
					if (archetypes.empty())
						return;

					// TODO: Support MatchingStyle::Simple too
					match_archetype_inter<OpAny, MatchingStyle::Wildcard>(ctx, entityKey, archetypes);
				}

				inline void match_archetype_any_as(MatchingCtx& ctx) {
					EntityLookupKey entityKey = EntityBadLookupKey;

					// For ANY we need at least one archetype to match.
					// However, because any of them can match, we need to check them all.
					// Iterating all of them is caller's responsibility.
					if (ctx.ent.id() == Is.id()) {
						ctx.ent = EntityBad;
						match_archetype_inter<OpAny, MatchingStyle::Complex>(ctx, entityKey, ctx.allArchetypes);
					} else {
						entityKey = EntityLookupKey(ctx.ent);

						auto archetypes =
								fetch_archetypes_for_select(*ctx.pEntityToArchetypeMap, ctx.allArchetypes, ctx.ent, entityKey);
						if (archetypes.empty())
							return;

						match_archetype_inter<OpAny, MatchingStyle::Complex>(ctx, entityKey, archetypes);
					}
				}

				template <MatchingStyle Style>
				inline void match_archetype_no_2(MatchingCtx& ctx) {
					// We had some matches already (with ALL or ANY). We need to remove those
					// that match with the NO list.

					if constexpr (Style == MatchingStyle::Complex) {
						for (uint32_t i = 0; i < ctx.pMatchesArr->size();) {
							const auto* pArchetype = (*ctx.pMatchesArr)[i];

							if (match_res_as<OpNo>(*ctx.pWorld, *pArchetype, ctx.idsToMatch)) {
								++i;
								continue;
							}

							core::swap_erase(*ctx.pMatchesArr, i);
						}
					}
#if GAIA_USE_PARTITIONED_BLOOM_FILTER >= 0
					else if constexpr (Style == MatchingStyle::Simple) {
						for (uint32_t i = 0; i < ctx.pMatchesArr->size();) {
							const auto* pArchetype = (*ctx.pMatchesArr)[i];

							// Try early exit
							if (OpNo::check_mask(pArchetype->queryMask(), ctx.queryMask))
								continue;

							if (match_res<OpNo>(*pArchetype, ctx.idsToMatch)) {
								++i;
								continue;
							}

							core::swap_erase(*ctx.pMatchesArr, i);
						}
					}
#endif
					else {
						for (uint32_t i = 0; i < ctx.pMatchesArr->size();) {
							const auto* pArchetype = (*ctx.pMatchesArr)[i];

							if (match_res<OpNo>(*pArchetype, ctx.idsToMatch)) {
								++i;
								continue;
							}

							core::swap_erase(*ctx.pMatchesArr, i);
						}
					}
				}

				struct OpcodeAll_Simple {
					static constexpr EOpcode Id = EOpcode::All_Simple;

					bool exec(const QueryCompileCtx& comp, MatchingCtx& ctx) {
						GAIA_PROF_SCOPE(vm::op_and_simple);

						ctx.ent = comp.ids_all[0];
						ctx.idsToMatch = std::span{comp.ids_all.data(), comp.ids_all.size()};

						if (ctx.targetEntities.empty())
							match_archetype_all<MatchingStyle::Simple>(ctx);
						else
							match_archetype_inter<OpAll, MatchingStyle::Simple>(ctx, ctx.allArchetypes);

						// If no ALL matches were found, we can quit right away.
						return !ctx.pMatchesArr->empty();
					}
				};

				struct OpcodeAll_Wildcard {
					static constexpr EOpcode Id = EOpcode::All_Wildcard;

					bool exec(const QueryCompileCtx& comp, MatchingCtx& ctx) {
						GAIA_PROF_SCOPE(vm::op_and_wildcard);

						ctx.ent = comp.ids_all[0];
						ctx.idsToMatch = std::span{comp.ids_all.data(), comp.ids_all.size()};

						if (ctx.targetEntities.empty())
							match_archetype_all<MatchingStyle::Wildcard>(ctx);
						else
							match_archetype_inter<OpAll, MatchingStyle::Wildcard>(ctx, ctx.allArchetypes);

						// If no ALL matches were found, we can quit right away.
						return !ctx.pMatchesArr->empty();
					}
				};

				struct OpcodeAll_Complex {
					static constexpr EOpcode Id = EOpcode::All_Complex;

					bool exec(const QueryCompileCtx& comp, MatchingCtx& ctx) {
						GAIA_PROF_SCOPE(vm::op_and_complex);

						ctx.ent = comp.ids_all[0];
						ctx.idsToMatch = std::span{comp.ids_all.data(), comp.ids_all.size()};

						if (ctx.targetEntities.empty())
							match_archetype_all<MatchingStyle::Complex>(ctx);
						else
							match_archetype_inter<OpAll, MatchingStyle::Complex>(ctx, ctx.allArchetypes);

						// If no ALL matches were found, we can quit right away.
						return !ctx.pMatchesArr->empty();
					}
				};

				struct OpcodeAny_NoAll {
					static constexpr EOpcode Id = EOpcode::Any_NoAll;

					bool exec(const QueryCompileCtx& comp, MatchingCtx& ctx) {
						GAIA_PROF_SCOPE(vm::op_any);

						if (ctx.skipAny)
							return true;

						const auto cnt = comp.ids_any.size();
						ctx.idsToMatch = std::span{comp.ids_any.data(), cnt};

						// Try find matches with optional components.
						GAIA_FOR(cnt) {
							ctx.ent = comp.ids_any[i];

							// First viable item is not related to an Is relationship
							if (ctx.as_mask_0 + ctx.as_mask_1 == 0U) {
								match_archetype_any(ctx);
							}
							// First viable item is related to an Is relationship.
							// In this case we need to gather all related archetypes.
							else {
								match_archetype_any_as(ctx);
							}
						}

						return true;
					}
				};

				struct OpcodeAny_WithAll {
					static constexpr EOpcode Id = EOpcode::Any_WithAll;

					bool exec(const QueryCompileCtx& comp, MatchingCtx& ctx) {
						GAIA_PROF_SCOPE(vm::op_any);

						if (ctx.skipAny)
							return true;

						ctx.idsToMatch = std::span{comp.ids_any.data(), comp.ids_any.size()};

						const bool hasNoAsRelationships = ctx.as_mask_0 + ctx.as_mask_1 == 0U;
#if GAIA_USE_PARTITIONED_BLOOM_FILTER >= 0
						const bool isSimple = (ctx.flags & QueryCtx::QueryFlags::Complex) == 0U;
#endif

						// We tried to match ALL items. Only search among archetypes we already found.
						// No last matched idx update is necessary because all ids were already checked
						// during the ALL pass.
						if (hasNoAsRelationships) {
#if GAIA_USE_PARTITIONED_BLOOM_FILTER >= 0
							if (isSimple) {
								for (uint32_t i = 0; i < ctx.pMatchesArr->size();) {
									const auto* pArchetype = (*ctx.pMatchesArr)[i];

									GAIA_FOR_((uint32_t)comp.ids_any.size(), j) {
										// Try early exit
										if (!OpAny::check_mask(pArchetype->queryMask(), ctx.queryMask))
											goto checkNextArchetype3;

										// First viable item is not related to an Is relationship
										if (match_res<OpAny>(*pArchetype, ctx.idsToMatch))
											goto checkNextArchetype3;
									}

									// No match found among ANY. Remove the archetype from the matching ones
									core::swap_erase(*ctx.pMatchesArr, i);
									continue;

								checkNextArchetype3:
									++i;
								}
							} else
#endif
							{
								for (uint32_t i = 0; i < ctx.pMatchesArr->size();) {
									const auto* pArchetype = (*ctx.pMatchesArr)[i];

									GAIA_FOR_((uint32_t)comp.ids_any.size(), j) {
										// First viable item is not related to an Is relationship
										if (match_res<OpAny>(*pArchetype, ctx.idsToMatch))
											goto checkNextArchetype1;

										// First viable item is related to an Is relationship.
										// In this case we need to gather all related archetypes.
										if (match_res_as<OpAny>(*ctx.pWorld, *pArchetype, ctx.idsToMatch))
											goto checkNextArchetype1;
									}

									// No match found among ANY. Remove the archetype from the matching ones
									core::swap_erase(*ctx.pMatchesArr, i);
									continue;

								checkNextArchetype1:
									++i;
								}
							}
						} else {
							for (uint32_t i = 0; i < ctx.pMatchesArr->size();) {
								const auto* pArchetype = (*ctx.pMatchesArr)[i];

								GAIA_FOR_((uint32_t)comp.ids_any.size(), j) {
									// First viable item is related to an Is relationship.
									// In this case we need to gather all related archetypes.
									if (match_res_as<OpAny>(*ctx.pWorld, *pArchetype, ctx.idsToMatch))
										goto checkNextArchetype2;
								}

								// No match found among ANY. Remove the archetype from the matching ones
								core::swap_erase(*ctx.pMatchesArr, i);
								continue;

							checkNextArchetype2:
								++i;
							}
						}

						return true;
					}
				};

				struct OpcodeNot_Simple {
					static constexpr EOpcode Id = EOpcode::Not_Simple;

					bool exec(const QueryCompileCtx& comp, MatchingCtx& ctx) {
						GAIA_PROF_SCOPE(vm::op_not);

						ctx.idsToMatch = std::span{comp.ids_not.data(), comp.ids_not.size()};
						ctx.pMatchesSet->clear();

						if (ctx.targetEntities.empty()) {
							// We searched for nothing more than NOT matches
							if (ctx.pMatchesArr->empty()) {
								// If there are no previous matches (no ALL or ANY matches),
								// we need to search among all archetypes.
								match_archetype_inter<detail::OpNo, MatchingStyle::Simple>(ctx, EntityBadLookupKey, ctx.allArchetypes);
							} else {
								match_archetype_no_2<MatchingStyle::Simple>(ctx);
							}
						} else {
							// We searched for nothing more than NOT matches
							if (ctx.pMatchesArr->empty())
								match_archetype_inter<detail::OpNo, MatchingStyle::Simple>(ctx, ctx.allArchetypes);
							else
								match_archetype_no_2<MatchingStyle::Simple>(ctx);
						}

						return true;
					}
				};

				struct OpcodeNot_Wildcard {
					static constexpr EOpcode Id = EOpcode::Not_Wildcard;

					bool exec(const QueryCompileCtx& comp, MatchingCtx& ctx) {
						GAIA_PROF_SCOPE(vm::op_not);

						ctx.idsToMatch = std::span{comp.ids_not.data(), comp.ids_not.size()};
						ctx.pMatchesSet->clear();

						if (ctx.targetEntities.empty()) {
							// We searched for nothing more than NOT matches
							if (ctx.pMatchesArr->empty()) {
								// If there are no previous matches (no ALL or ANY matches),
								// we need to search among all archetypes.
								match_archetype_inter<detail::OpNo, MatchingStyle::Wildcard>(
										ctx, EntityBadLookupKey, ctx.allArchetypes);
							} else {
								match_archetype_no_2<MatchingStyle::Wildcard>(ctx);
							}
						} else {
							// We searched for nothing more than NOT matches
							if (ctx.pMatchesArr->empty())
								match_archetype_inter<detail::OpNo, MatchingStyle::Wildcard>(ctx, ctx.allArchetypes);
							else
								match_archetype_no_2<MatchingStyle::Wildcard>(ctx);
						}

						return true;
					}
				};

				struct OpcodeNot_Complex {
					static constexpr EOpcode Id = EOpcode::Not_Complex;

					bool exec(const QueryCompileCtx& comp, MatchingCtx& ctx) {
						GAIA_PROF_SCOPE(vm::op_not);

						ctx.idsToMatch = std::span{comp.ids_not.data(), comp.ids_not.size()};
						ctx.pMatchesSet->clear();

						if (ctx.targetEntities.empty()) {
							// We searched for nothing more than NOT matches
							if (ctx.pMatchesArr->empty()) {
								// If there are no previous matches (no ALL or ANY matches),
								// we need to search among all archetypes.
								match_archetype_inter<detail::OpNo, MatchingStyle::Complex>(ctx, EntityBadLookupKey, ctx.allArchetypes);
							} else {
								match_archetype_no_2<MatchingStyle::Complex>(ctx);
							}
						} else {
							// We searched for nothing more than NOT matches
							if (ctx.pMatchesArr->empty())
								match_archetype_inter<detail::OpNo, MatchingStyle::Complex>(ctx, ctx.allArchetypes);
							else
								match_archetype_no_2<MatchingStyle::Complex>(ctx);
						}

						return true;
					}
				};
			} // namespace detail

			class VirtualMachine {
				using OpcodeFunc = bool (*)(const detail::QueryCompileCtx& comp, MatchingCtx& ctx);
				struct Opcodes {
					OpcodeFunc exec;
				};

				static constexpr OpcodeFunc OpcodeFuncs[] = {
						// OpcodeAll_Simple
						[](const detail::QueryCompileCtx& comp, MatchingCtx& ctx) {
							detail::OpcodeAll_Simple op;
							return op.exec(comp, ctx);
						},
						// OpcodeAll_Wildcard
						[](const detail::QueryCompileCtx& comp, MatchingCtx& ctx) {
							detail::OpcodeAll_Wildcard op;
							return op.exec(comp, ctx);
						},
						// OpcodeAll_Complex
						[](const detail::QueryCompileCtx& comp, MatchingCtx& ctx) {
							detail::OpcodeAll_Complex op;
							return op.exec(comp, ctx);
						},
						// OpcodeAny_NoAll
						[](const detail::QueryCompileCtx& comp, MatchingCtx& ctx) {
							detail::OpcodeAny_NoAll op;
							return op.exec(comp, ctx);
						},
						// OpcodeAny_WithAll
						[](const detail::QueryCompileCtx& comp, MatchingCtx& ctx) {
							detail::OpcodeAny_WithAll op;
							return op.exec(comp, ctx);
						},
						// OpcodeNot_Simple
						[](const detail::QueryCompileCtx& comp, MatchingCtx& ctx) {
							detail::OpcodeNot_Simple op;
							return op.exec(comp, ctx);
						},
						// OpcodeNot_Wildcard
						[](const detail::QueryCompileCtx& comp, MatchingCtx& ctx) {
							detail::OpcodeNot_Wildcard op;
							return op.exec(comp, ctx);
						},
						// OpcodeNot_Complex
						[](const detail::QueryCompileCtx& comp, MatchingCtx& ctx) {
							detail::OpcodeNot_Complex op;
							return op.exec(comp, ctx);
						},
				};

				detail::QueryCompileCtx m_compCtx;

			private:
				GAIA_NODISCARD bool eval_source_terms(MatchingCtx& ctx) const {
					ctx.skipAny = false;

					// ALL source terms must all match.
					for (const auto& term: m_compCtx.terms_all_src) {
						if (!detail::match_source_term(*ctx.pWorld, term))
							return false;
					}

					// NOT source terms must all be absent.
					for (const auto& term: m_compCtx.terms_not_src) {
						if (detail::match_source_term(*ctx.pWorld, term))
							return false;
					}

					// ANY source terms short-circuit the ANY group if one matches.
					bool anySourceMatched = false;
					for (const auto& term: m_compCtx.terms_any_src) {
						if (!detail::match_source_term(*ctx.pWorld, term))
							continue;

						anySourceMatched = true;
						break;
					}

					if (!m_compCtx.terms_any_src.empty() && //
							!anySourceMatched && //
							m_compCtx.ids_any.empty() && //
							m_compCtx.terms_any_var.empty())
						return false;

					if (anySourceMatched) {
						ctx.skipAny = true;
						if (m_compCtx.ids_all.empty())
							detail::add_all_archetypes(ctx);
					}

					return true;
				}

				GAIA_NODISCARD bool eval_variable_terms_on_archetype(
						const MatchingCtx& ctx, const Archetype& archetype, bool anyAlreadySatisfied) const {
					using namespace detail;

					auto finalize = [&](const VarBindings& vars, bool anyMatched) -> bool {
						// NOT variable terms must not match.
						for (const auto& term: m_compCtx.terms_not_var) {
							if (term_has_match(*ctx.pWorld, archetype, term, vars))
								return false;
						}

						bool anySatisfied = anyAlreadySatisfied || anyMatched;
						if (!anySatisfied && !m_compCtx.terms_any_var.empty()) {
							for (const auto& term: m_compCtx.terms_any_var) {
								if (!term_has_match(*ctx.pWorld, archetype, term, vars))
									continue;
								anySatisfied = true;
								break;
							}
						}

						if (!anySatisfied && !m_compCtx.terms_any_var.empty())
							return false;

						return true;
					};

					auto solve = [&](auto&& self, uint16_t pendingAllMask, uint16_t pendingAnyMask, const VarBindings& vars,
													 bool anyMatched) -> bool {
						if (pendingAllMask == 0)
							return finalize(vars, anyMatched);

						// Try to resolve one ready ALL term.
						const auto allCnt = (uint32_t)m_compCtx.terms_all_var.size();
						GAIA_FOR(allCnt) {
							const auto bit = (uint16_t(1) << i);
							if ((pendingAllMask & bit) == 0)
								continue;

							const auto& term = m_compCtx.terms_all_var[i];
							if (is_var_entity(term.src) && !var_is_bound(vars, term.src))
								continue;

							cnt::sarray_ext<VarBindings, VarBindings::VarCnt> states;
							collect_term_matches(*ctx.pWorld, archetype, term, vars, states);
							if (states.empty())
								return false;

							const auto nextAllMask = (uint16_t)(pendingAllMask & ~bit);
							for (const auto& state: states) {
								if (self(self, nextAllMask, pendingAnyMask, state, anyMatched))
									return true;
							}

							return false;
						}

						// No ALL term was ready. Use ANY terms to discover missing bindings.
						const auto anyCnt = (uint32_t)m_compCtx.terms_any_var.size();
						GAIA_FOR(anyCnt) {
							const auto bit = (uint16_t(1) << i);
							if ((pendingAnyMask & bit) == 0)
								continue;

							const auto& term = m_compCtx.terms_any_var[i];
							if (is_var_entity(term.src) && !var_is_bound(vars, term.src))
								continue;

							cnt::sarray_ext<VarBindings, VarBindings::VarCnt> states;
							collect_term_matches(*ctx.pWorld, archetype, term, vars, states);
							if (states.empty())
								continue;

							const auto nextAnyMask = (uint16_t)(pendingAnyMask & ~bit);
							for (const auto& state: states) {
								if (self(self, pendingAllMask, nextAnyMask, state, true))
									return true;
							}
						}

						return false;
					};

					const auto pendingAllMask = (uint16_t)((uint16_t(1) << m_compCtx.terms_all_var.size()) - 1u);
					const auto pendingAnyMask = (uint16_t)((uint16_t(1) << m_compCtx.terms_any_var.size()) - 1u);
					VarBindings vars{};
					return solve(solve, pendingAllMask, pendingAnyMask, vars, false);
				}

				void filter_variable_terms(MatchingCtx& ctx, bool fallbackToAllArchetypes) const {
					if (!m_compCtx.has_variable_terms())
						return;

					const bool anyAlreadySatisfied = !m_compCtx.ids_any.empty() || !m_compCtx.terms_any_src.empty();

					cnt::darr<const Archetype*> filtered;

					if (!ctx.pMatchesArr->empty()) {
						filtered.reserve(ctx.pMatchesArr->size());
						for (const auto* pArchetype: *ctx.pMatchesArr) {
							if (!eval_variable_terms_on_archetype(ctx, *pArchetype, anyAlreadySatisfied))
								continue;
							filtered.push_back(pArchetype);
						}
					} else if (fallbackToAllArchetypes) {
						filtered.reserve((uint32_t)ctx.allArchetypes.size());
						for (const auto* pArchetype: ctx.allArchetypes) {
							if (!eval_variable_terms_on_archetype(ctx, *pArchetype, anyAlreadySatisfied))
								continue;
							filtered.push_back(pArchetype);
						}
					}

					ctx.pMatchesArr->clear();
					ctx.pMatchesSet->clear();
					for (const auto* pArchetype: filtered) {
						ctx.pMatchesArr->push_back(pArchetype);
						ctx.pMatchesSet->emplace(pArchetype);
					}
				}

				GAIA_NODISCARD detail::VmLabel add_op(detail::CompiledOp&& op) {
					const auto cnt = (detail::VmLabel)m_compCtx.ops.size();
					op.pc_ok = cnt + 1;
					op.pc_fail = cnt - 1;
					m_compCtx.ops.push_back(GAIA_MOV(op));
					return cnt;
				}

			public:
				//! Transforms inputs into virtual machine opcodes.
				void compile(
						const EntityToArchetypeMap& entityToArchetypeMap, std::span<const Archetype*> allArchetypes,
						QueryCtx& queryCtx) {
					GAIA_PROF_SCOPE(vm::compile);
					(void)entityToArchetypeMap;
					(void)allArchetypes;

					m_compCtx.ids_all.clear();
					m_compCtx.ids_any.clear();
					m_compCtx.ids_not.clear();
					m_compCtx.terms_all_src.clear();
					m_compCtx.terms_any_src.clear();
					m_compCtx.terms_not_src.clear();
					m_compCtx.terms_all_var.clear();
					m_compCtx.terms_any_var.clear();
					m_compCtx.terms_not_var.clear();
					m_compCtx.ops.clear();

					auto& data = queryCtx.data;

					QueryTermSpan terms = data.terms_view_mut();
					QueryTermSpan terms_all = terms.subspan(0, data.firstAny);
					QueryTermSpan terms_any = terms.subspan(data.firstAny, data.firstNot - data.firstAny);
					QueryTermSpan terms_not = terms.subspan(data.firstNot);

					// ALL
					if (!terms_all.empty()) {
						GAIA_PROF_SCOPE(vm::compile_all);

						const auto cnt = terms_all.size();
						GAIA_FOR(cnt) {
							auto& p = terms_all[i];
							if (term_has_variables(p)) {
								m_compCtx.terms_all_var.push_back(p);
								continue;
							}

							if (p.src == EntityBad) {
								m_compCtx.ids_all.push_back(p.id);
								continue;
							}
							m_compCtx.terms_all_src.push_back(p);
						}
					}

					// ANY
					if (!terms_any.empty()) {
						GAIA_PROF_SCOPE(vm::compile_any);

						const auto cnt = terms_any.size();
						GAIA_FOR(cnt) {
							auto& p = terms_any[i];
							if (term_has_variables(p)) {
								m_compCtx.terms_any_var.push_back(p);
								continue;
							}

							if (p.src == EntityBad)
								m_compCtx.ids_any.push_back(p.id);
							else
								m_compCtx.terms_any_src.push_back(p);
						}
					}

					// NOT
					if (!terms_not.empty()) {
						GAIA_PROF_SCOPE(vm::compile_not);

						const auto cnt = terms_not.size();
						GAIA_FOR(cnt) {
							auto& p = terms_not[i];
							if (term_has_variables(p)) {
								m_compCtx.terms_not_var.push_back(p);
								continue;
							}

							if (p.src == EntityBad)
								m_compCtx.ids_not.push_back(p.id);
							else
								m_compCtx.terms_not_src.push_back(p);
						}
					}

					create_opcodes(queryCtx);
				}

				void create_opcodes(QueryCtx& queryCtx) {
					const bool isSimple = (queryCtx.data.flags & QueryCtx::QueryFlags::Complex) == 0U;
					const bool isAs = (queryCtx.data.as_mask_0 + queryCtx.data.as_mask_1) != 0U;

					// Reset the list of opcodes
					m_compCtx.ops.clear();

					// ALL
					if (!m_compCtx.ids_all.empty()) {
						detail::CompiledOp op{};
						if (isSimple)
							op.opcode = detail::EOpcode::All_Simple;
						else if (isAs)
							op.opcode = detail::EOpcode::All_Complex;
						else
							op.opcode = detail::EOpcode::All_Wildcard;
						(void)add_op(GAIA_MOV(op));
					}

					// ANY
					if (!m_compCtx.ids_any.empty()) {
						detail::CompiledOp op{};
						op.opcode = m_compCtx.ids_all.empty() ? detail::EOpcode::Any_NoAll : detail::EOpcode::Any_WithAll;
						(void)add_op(GAIA_MOV(op));
					}

					// NOT
					if (!m_compCtx.ids_not.empty()) {
						detail::CompiledOp op{};
						if (isSimple)
							op.opcode = detail::EOpcode::Not_Simple;
						else if (isAs)
							op.opcode = detail::EOpcode::Not_Complex;
						else
							op.opcode = detail::EOpcode::Not_Wildcard;
						(void)add_op(GAIA_MOV(op));
					}

					// Mark as compiled
					queryCtx.data.flags &= ~QueryCtx::QueryFlags::Recompile;
				}

				GAIA_NODISCARD bool is_compiled() const {
					return !m_compCtx.ops.empty() || m_compCtx.has_source_terms() || m_compCtx.has_variable_terms();
				}

				//! Executes compiled opcodes
				void exec(MatchingCtx& ctx) {
					GAIA_PROF_SCOPE(vm::exec);

					if (!eval_source_terms(ctx))
						return;

					// Query contains only source terms.
					if (m_compCtx.ops.empty()) {
						if (ctx.pMatchesArr->empty())
							detail::add_all_archetypes(ctx);

						filter_variable_terms(ctx, true);
						return;
					}

					ctx.pc = 0;

					// Extract data from the buffer
					do {
						auto& stackItem = m_compCtx.ops[ctx.pc];
						const bool ret = OpcodeFuncs[(uint32_t)stackItem.opcode](m_compCtx, ctx);
						ctx.pc = ret ? stackItem.pc_ok : stackItem.pc_fail;
					} while (ctx.pc < m_compCtx.ops.size()); // (uint32_t)-1 falls in this category as well

					filter_variable_terms(ctx, false);
				}
			};

		} // namespace vm
	} // namespace ecs
} // namespace gaia
