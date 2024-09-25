#pragma once
#include "../config/config.h"

#include <cstdint>
#include <type_traits>

#include "../cnt/darray.h"
#include "../cnt/set.h"
#include "../config/profiler.h"
#include "../core/utility.h"
#include "archetype.h"
#include "archetype_common.h"
#include "data_buffer.h"
#include "id.h"
#include "query_common.h"

namespace gaia {
	namespace ecs {
		extern Archetype* archetype_from_entity(const World& world, Entity entity);
		extern GroupId group_by_func_default(const World& world, const Archetype& archetype, Entity groupBy);
		extern bool is(const World& world, Entity entity, Entity baseEntity);

		template <typename Func>
		void as_relations_trav(const World& world, Entity target, Func func);
		template <typename Func>
		bool as_relations_trav_if(const World& world, Entity target, Func func);

		using EntityToArchetypeMap = cnt::map<EntityLookupKey, ArchetypeDArray>;

		namespace vm {

			struct MatchingCtx {
				// Setup up externally
				//////////////////////////////////

				//! World
				const World* pWorld;
				//! entity -> archetypes mapping
				const EntityToArchetypeMap* pEntityToArchetypeMap;
				//! Array of all archetypes in the world
				const ArchetypeDArray* pAllArchetypes;
				//! Set of already matched archetypes. Reset before each exec().
				cnt::set<Archetype*>* pMatchesSet;
				//! Array of already matches archetypes. Reset before each exec().
				ArchetypeDArray* pMatchesArr;
				//! Idx of the last matched archetype against the ALL opcode
				QueryArchetypeCacheIndexMap* pLastMatchedArchetypeIdx_All;
				//! Idx of the last matched archetype against the ANY opcode
				QueryArchetypeCacheIndexMap* pLastMatchedArchetypeIdx_Any;
				//! Mask for items with Is relationship pair.
				//! If the id is a pair, the first part (id) is written here.
				uint32_t as_mask_0;
				//! Mask for items with Is relationship pair.
				//! If the id is a pair, the second part (gen) is written here.
				uint32_t as_mask_1;

				// For the opcode compiler to modify
				//////////////////////////////////////

				//! Entity to match
				Entity ent;
				//! List of entity ids in a query to consider
				EntitySpan idsToMatch;
				//! Current stack position (program counter)
				uint32_t pc;
			};

			inline const ArchetypeDArray* fetch_archetypes_for_select(const EntityToArchetypeMap& map, EntityLookupKey key) {
				GAIA_ASSERT(key != EntityBadLookupKey);

				const auto it = map.find(key);
				if (it == map.end() || it->second.empty())
					return nullptr;

				return &it->second;
			}

			inline const ArchetypeDArray* fetch_archetypes_for_select(const EntityToArchetypeMap& map, Entity src) {
				GAIA_ASSERT(src != EntityBad);

				return fetch_archetypes_for_select(map, EntityLookupKey(src));
			}

			namespace detail {
				enum class EOpcode : uint8_t {
					//! X
					All,
					//! ?X
					Any_NoAll,
					Any_WithAll,
					//! !X
					Not
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
					cnt::sarr_ext<Entity, MAX_ITEMS_IN_QUERY> ids_all;
					//! Array of ops that can be evaluated with a ANY opcode
					cnt::sarr_ext<Entity, MAX_ITEMS_IN_QUERY> ids_any;
					//! Array of ops that can be evaluated with a NOT opcode
					cnt::sarr_ext<Entity, MAX_ITEMS_IN_QUERY> ids_not;
				};

				// Operator ALL (used by query::all)
				struct OpAll {
					static bool can_continue(bool hasMatch) {
						return hasMatch;
					};
					static bool eval(uint32_t expectedMatches, uint32_t totalMatches) {
						return expectedMatches == totalMatches;
					}
				};
				// Operator OR (used by query::any)
				struct OpAny {
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
				inline bool match_inter_eval_matches(uint32_t queryIdMarches, uint32_t& outMatches) {
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
				inline GAIA_NODISCARD bool match_inter(EntitySpan queryIds, EntitySpan archetypeIds, CmpFunc func) {
					const auto archetypeIdsCnt = (uint32_t)archetypeIds.size();
					const auto queryIdsCnt = (uint32_t)queryIds.size();

					// Arrays are sorted so we can do linear intersection lookup
					uint32_t indices[2]{}; // 0 for query ids, 1 for archetype ids
					uint32_t matches = 0;
					Entity entityToMatch = EntityBad;
					Entity varEntity = EntityBad;

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

				struct IdCmpResult {
					bool matched;
				};

				inline GAIA_NODISCARD IdCmpResult cmp_ids(Entity idInQuery, Entity idInArchetype) {
					return {idInQuery == idInArchetype};
				}

				inline GAIA_NODISCARD IdCmpResult cmp_ids_pairs(Entity idInQuery, Entity idInArchetype) {
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

				inline GAIA_NODISCARD IdCmpResult
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

				inline GAIA_NODISCARD IdCmpResult
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

				//! Tries to match entity ids in \param queryIds with those in \param archetype given
				//! the comparison function \param func. Does not consider Is relationships.
				//! \return True on the first match, false otherwise.
				template <typename Op>
				inline GAIA_NODISCARD bool match_res(const Archetype& archetype, EntitySpan queryIds) {
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

				//! Tries to match entity ids in \param queryIds with those in \param archetype given
				//! the comparison function \param func. Considers Is relationships.
				//! \return True on the first match, false otherwise.
				template <typename Op>
				inline GAIA_NODISCARD bool match_res_as(const World& w, const Archetype& archetype, EntitySpan queryIds) {
					// Archetype has no pairs we can compare ids directly
					if (archetype.pairs() == 0) {
						return match_inter<Op>(
								queryIds, archetype.ids_view(),
								// cmp func
								[&](Entity idInQuery, Entity idInArchetype) {
									return cmp_ids_is(w, archetype, idInQuery, idInArchetype);
								});
					}

					return match_inter<Op>(
							queryIds, archetype.ids_view(),
							// cmp func
							[&](Entity idInQuery, Entity idInArchetype) {
								return cmp_ids_is_pairs(w, archetype, idInQuery, idInArchetype);
							});
				}

				template <typename OpKind, bool CheckAs>
				inline void
				match_archetype_internal(MatchingCtx& ctx, EntityLookupKey entityKey, const ArchetypeDArray* pSrcArchetypes) {
					const auto& archetypes = *pSrcArchetypes;

					auto& matchesArr = *ctx.pMatchesArr;
					auto& matchesSet = *ctx.pMatchesSet;

					const auto cache_it = ctx.pLastMatchedArchetypeIdx_All->find(entityKey);
					uint32_t lastMatchedIdx = 0;
					if (cache_it == ctx.pLastMatchedArchetypeIdx_All->end())
						ctx.pLastMatchedArchetypeIdx_All->emplace(entityKey, archetypes.size());
					else {
						lastMatchedIdx = cache_it->second;
						cache_it->second = archetypes.size();
					}

					if (lastMatchedIdx >= archetypes.size())
						return;

					auto archetypeSpan = std::span(&archetypes[lastMatchedIdx], archetypes.size() - lastMatchedIdx);
					for (auto* pArchetype: archetypeSpan) {
						if (matchesSet.contains(pArchetype))
							continue;

						if constexpr (CheckAs) {
							if (!match_res_as<OpKind>(*ctx.pWorld, *pArchetype, ctx.idsToMatch))
								continue;
						} else {
							if (!match_res<OpKind>(*pArchetype, ctx.idsToMatch))
								continue;
						};

						matchesSet.emplace(pArchetype);
						matchesArr.emplace_back(pArchetype);
					}
				}

				inline void match_archetype_all(MatchingCtx& ctx) {
					EntityLookupKey entityKey(ctx.ent);

					// For ALL we need all the archetypes to match. We start by checking
					// if the first one is registered in the world at all.
					const auto* pArchetypes = fetch_archetypes_for_select(*ctx.pEntityToArchetypeMap, entityKey);
					if (pArchetypes == nullptr)
						return;

					match_archetype_internal<OpAll, false>(ctx, entityKey, pArchetypes);
				}

				inline void match_archetype_all_as(MatchingCtx& ctx) {
					const auto& allArchetypes = *ctx.pAllArchetypes;

					// For ALL we need all the archetypes to match. We start by checking
					// if the first one is registered in the world at all.
					const ArchetypeDArray* pSrcArchetypes = nullptr;

					EntityLookupKey entityKey = EntityBadLookupKey;

					if (ctx.ent.id() == Is.id()) {
						ctx.ent = EntityBad;
						pSrcArchetypes = &allArchetypes;
					} else {
						entityKey = EntityLookupKey(ctx.ent);

						pSrcArchetypes = fetch_archetypes_for_select(*ctx.pEntityToArchetypeMap, entityKey);
						if (pSrcArchetypes == nullptr)
							return;
					}

					match_archetype_internal<OpAll, true>(ctx, entityKey, pSrcArchetypes);
				}

				inline void match_archetype_one(MatchingCtx& ctx) {
					EntityLookupKey entityKey(ctx.ent);

					// For ANY we need at least one archetypes to match.
					// However, because any of them can match, we need to check them all.
					// Iterating all of them is caller's responsibility.
					const auto* pArchetypes = fetch_archetypes_for_select(*ctx.pEntityToArchetypeMap, entityKey);
					if (pArchetypes == nullptr)
						return;

					match_archetype_internal<OpAny, false>(ctx, entityKey, pArchetypes);
				}

				inline void match_archetype_one_as(MatchingCtx& ctx) {
					const auto& allArchetypes = *ctx.pAllArchetypes;

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

						pSrcArchetypes = fetch_archetypes_for_select(*ctx.pEntityToArchetypeMap, entityKey);
						if (pSrcArchetypes == nullptr)
							return;
					}

					match_archetype_internal<OpAny, true>(ctx, entityKey, pSrcArchetypes);
				}

				inline void match_archetype_no(MatchingCtx& ctx) {
					ctx.pMatchesSet->clear();

					match_archetype_internal<OpNo, false>(ctx, EntityBadLookupKey, ctx.pAllArchetypes);
				}

				inline void match_archetype_no_as(MatchingCtx& ctx) {
					ctx.pMatchesSet->clear();

					match_archetype_internal<OpNo, true>(ctx, EntityBadLookupKey, ctx.pAllArchetypes);
				}

				inline void match_archetype_no_2(MatchingCtx& ctx) {
					// We had some matches already (with ALL or ANY). We need to remove those
					// that match with the NO list. Remove them back-to-front.
					for (uint32_t i = 0; i < ctx.pMatchesArr->size();) {
						auto* pArchetype = (*ctx.pMatchesArr)[i];
						if (match_res<OpNo>(*pArchetype, ctx.idsToMatch)) {
							++i;
							continue;
						}

						core::erase_fast(*ctx.pMatchesArr, i);
					}
				}

				inline void match_archetype_no_as_2(MatchingCtx& ctx) {
					// We had some matches already (with ALL or ANY). We need to remove those
					// that match with the NO list. Remove them back-to-front.
					for (uint32_t i = 0; i < ctx.pMatchesArr->size();) {
						auto* pArchetype = (*ctx.pMatchesArr)[i];
						if (match_res_as<OpNo>(*ctx.pWorld, *pArchetype, ctx.idsToMatch)) {
							++i;
							continue;
						}

						core::erase_fast(*ctx.pMatchesArr, i);
					}
				}

				struct OpcodeBaseData {
					EOpcode id;
				};

				struct OpcodeAll {
					static constexpr EOpcode Id = EOpcode::All;

					bool exec(const QueryCompileCtx& comp, MatchingCtx& ctx) {
						GAIA_PROF_SCOPE(vm::op_and);

						ctx.ent = comp.ids_all[0];
						ctx.idsToMatch = std::span{comp.ids_all.data(), comp.ids_all.size()};

						// First viable item is not related to an Is relationship
						if (ctx.as_mask_0 + ctx.as_mask_1 == 0U) {
							match_archetype_all(ctx);
						}
						// First viable item is related to an Is relationship.
						// In this case we need to gather all related archetypes.
						else {
							match_archetype_all_as(ctx);
						}

						// If no ALL matches were found, we can quit right away.
						return !ctx.pMatchesArr->empty();
					}
				};

				struct OpcodeAny_NoAll {
					static constexpr EOpcode Id = EOpcode::Any_NoAll;

					bool exec(const QueryCompileCtx& comp, MatchingCtx& ctx) {
						GAIA_PROF_SCOPE(vm::op_any);

						ctx.idsToMatch = std::span{comp.ids_any.data(), comp.ids_any.size()};

						// Try find matches with optional components.
						GAIA_EACH(comp.ids_any) {
							ctx.ent = comp.ids_any[i];

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

						return true;
					}
				};

				struct OpcodeAny_WithAll {
					static constexpr EOpcode Id = EOpcode::Any_WithAll;

					bool exec(const QueryCompileCtx& comp, MatchingCtx& ctx) {
						GAIA_PROF_SCOPE(vm::op_any);

						ctx.idsToMatch = std::span{comp.ids_any.data(), comp.ids_any.size()};

						// We tried to match ALL items. Only search among archetypes we already found.
						// No last matched idx update is necessary because all ids were already checked
						// during the ALL pass.
						for (uint32_t i = 0; i < ctx.pMatchesArr->size();) {
							auto* pArchetype = (*ctx.pMatchesArr)[i];

							if (ctx.as_mask_0 + ctx.as_mask_1 == 0U) {
								GAIA_FOR_((uint32_t)comp.ids_any.size(), j) {
									// First viable item is not related to an Is relationship
									if (match_res<OpAny>(*pArchetype, ctx.idsToMatch))
										goto checkNextArchetype;

									// First viable item is related to an Is relationship.
									// In this case we need to gather all related archetypes.
									if (match_res_as<OpAny>(*ctx.pWorld, *pArchetype, ctx.idsToMatch))
										goto checkNextArchetype;
								}
							} else {
								GAIA_FOR_((uint32_t)comp.ids_any.size(), j) {
									// First viable item is related to an Is relationship.
									// In this case we need to gather all related archetypes.
									if (match_res_as<OpAny>(*ctx.pWorld, *pArchetype, ctx.idsToMatch))
										goto checkNextArchetype;
								}
							}

							// No match found among ANY. Remove the archetype from the matching ones
							core::erase_fast(*ctx.pMatchesArr, i);
							continue;

						checkNextArchetype:
							++i;
						}

						return true;
					}
				};

				struct OpcodeNot {
					static constexpr EOpcode Id = EOpcode::Not;

					bool exec(const QueryCompileCtx& comp, MatchingCtx& ctx) {
						GAIA_PROF_SCOPE(vm::op_not);

						ctx.idsToMatch = std::span{comp.ids_not.data(), comp.ids_not.size()};

						// We searched for nothing more than NOT matches
						if (ctx.pMatchesArr->empty()) {
							// If there are no previous matches (no ALL or ANY matches),
							// we need to search among all archetypes.
							if (ctx.as_mask_0 + ctx.as_mask_1 == 0U)
								match_archetype_no(ctx);
							else
								match_archetype_no_as(ctx);
						} else {
							if (ctx.as_mask_0 + ctx.as_mask_1 == 0U)
								match_archetype_no_2(ctx);
							else
								match_archetype_no_as_2(ctx);
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
						// OpcodeAll
						[](const detail::QueryCompileCtx& comp, MatchingCtx& ctx) {
							detail::OpcodeAll op;
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
						// OpcodeNot
						[](const detail::QueryCompileCtx& comp, MatchingCtx& ctx) {
							detail::OpcodeNot op;
							return op.exec(comp, ctx);
						}};

				detail::QueryCompileCtx m_compCtx;

			private:
				GAIA_NODISCARD detail::VmLabel add_op(detail::CompiledOp&& op) {
					const auto cnt = (detail::VmLabel)m_compCtx.ops.size();
					op.pc_ok = cnt + 1;
					op.pc_fail = cnt - 1;
					m_compCtx.ops.push_back(GAIA_MOV(op));
					return cnt;
				}

			public:
				//! Transforms inputs into virtual machine opcodes.
				void compile(const EntityToArchetypeMap& entityToArchetypeMap, const QueryCtx& queryCtx) {
					GAIA_PROF_SCOPE(vm::compile);

					const auto& data = queryCtx.data;

					QueryTermSpan terms{data.terms.data(), data.terms.size()};
					QueryTermSpan terms_all = terms.subspan(0, data.firstAny);
					QueryTermSpan terms_any = terms.subspan(data.firstAny, data.firstNot - data.firstAny);
					QueryTermSpan terms_not = terms.subspan(data.firstNot);

					// ALL
					if (!terms_all.empty()) {
						GAIA_PROF_SCOPE(vm::compile_all);

						GAIA_EACH(terms_all) {
							auto& p = terms_all[i];
							if (p.src == EntityBad) {
								m_compCtx.ids_all.push_back(p.id);
								continue;
							}

							// Fixed source
							{
								p.srcArchetype = archetype_from_entity(*queryCtx.w, p.src);

								// Archetype needs to exist. If it does not we have nothing to do here.
								if (p.srcArchetype == nullptr) {
									m_compCtx.ops.clear();
									return;
								}
							}
						}
					}

					// ANY
					if (!terms_any.empty()) {
						GAIA_PROF_SCOPE(vm::compile_any);

						cnt::sarr_ext<const ArchetypeDArray*, MAX_ITEMS_IN_QUERY> archetypesWithId;
						GAIA_EACH(terms_any) {
							auto& p = terms_any[i];
							if (p.src != EntityBad) {
								p.srcArchetype = archetype_from_entity(*queryCtx.w, p.src);
								if (p.srcArchetype == nullptr)
									continue;
							}

							// Check if any archetype is associated with the entity id.
							// All ids must be registered in the world.
							const auto* pArchetypes = fetch_archetypes_for_select(entityToArchetypeMap, EntityLookupKey(p.id));
							if (pArchetypes == nullptr)
								continue;

							archetypesWithId.push_back(pArchetypes);

							m_compCtx.ids_any.push_back(p.id);
						}

						// No archetypes with "any" entities exist. We can quit right away.
						if (archetypesWithId.empty()) {
							m_compCtx.ops.clear();
							return;
						}
					}

					// NOT
					if (!terms_not.empty()) {
						GAIA_PROF_SCOPE(vm::compile_not);

						GAIA_EACH(terms_not) {
							auto& p = terms_not[i];
							if (p.src != EntityBad)
								continue;

							m_compCtx.ids_not.push_back(p.id);
						}
					}

					if (!m_compCtx.ids_all.empty()) {
						detail::CompiledOp op{};
						op.opcode = detail::EOpcode::All;
						add_op(GAIA_MOV(op));
					}
					if (!m_compCtx.ids_any.empty()) {
						detail::CompiledOp op{};
						op.opcode = m_compCtx.ids_all.empty() ? detail::EOpcode::Any_NoAll : detail::EOpcode::Any_WithAll;
						add_op(GAIA_MOV(op));
					}
					if (!m_compCtx.ids_not.empty()) {
						detail::CompiledOp op{};
						op.opcode = detail::EOpcode::Not;
						add_op(GAIA_MOV(op));
					}
				}

				GAIA_NODISCARD bool is_compiled() const {
					return !m_compCtx.ops.empty();
				}

				//! Executes compiled opcodes
				void exec(MatchingCtx& ctx) {
					GAIA_PROF_SCOPE(vm::exec);

					ctx.pc = 0;

					// Extract data from the buffer
					do {
						auto& stackItem = m_compCtx.ops[ctx.pc];
						const bool ret = OpcodeFuncs[(uint32_t)stackItem.opcode](m_compCtx, ctx);
						ctx.pc = ret ? stackItem.pc_ok : stackItem.pc_fail;
					} while (ctx.pc < m_compCtx.ops.size()); // (uint32_t)-1 falls in this category as well
				}
			};

		} // namespace vm
	} // namespace ecs
} // namespace gaia