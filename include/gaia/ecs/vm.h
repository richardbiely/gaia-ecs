#pragma once
#include "gaia/config/config.h"

#include <cstdint>
#include <cstdio>
#include <type_traits>

#include "gaia/cnt/darray.h"
#include "gaia/cnt/sarray.h"
#include "gaia/cnt/set.h"
#include "gaia/cnt/sparse_storage.h"
#include "gaia/config/profiler.h"
#include "gaia/core/utility.h"
#include "gaia/ecs/api.h"
#include "gaia/ecs/archetype.h"
#include "gaia/ecs/archetype_common.h"
#include "gaia/ecs/id.h"
#include "gaia/ecs/query_common.h"
#include "gaia/ecs/query_mask.h"
#include "gaia/ser/ser_binary.h"
#include "gaia/util/str.h"

namespace gaia {
	namespace ecs {
		using EntityToArchetypeMap = cnt::map<EntityLookupKey, ArchetypeDArray>;

		struct ArchetypeMatchStamp {
			cnt::sparse_id id = 0;
			uint32_t epoch = 0;
		};
	} // namespace ecs

	namespace cnt {
		template <>
		struct to_sparse_id<ecs::ArchetypeMatchStamp> {
			static sparse_id get(const ecs::ArchetypeMatchStamp& item) noexcept {
				return item.id;
			}
		};
	} // namespace cnt

	namespace ecs {
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
				//! Optional per-archetype stamp table for O(1) dedup in hot loops.
				//! If null, matching falls back to pMatchesSet-based dedup.
				cnt::sparse_storage<ecs::ArchetypeMatchStamp>* pMatchesStampByArchetypeId;
				//! Current dedup epoch used with pMatchesStampByArchetypeId.
				uint32_t matchesEpoch;
				//! Idx of the last matched archetype against the ALL opcode
				QueryArchetypeCacheIndexMap* pLastMatchedArchetypeIdx_All;
				//! Idx of the last matched archetype against the OR opcode
				QueryArchetypeCacheIndexMap* pLastMatchedArchetypeIdx_Or;
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
				//! Runtime variable bindings (Var0..Var7) provided by the query.
				cnt::sarray<Entity, 8> varBindings;
				//! Bitmask of bindings set in varBindings.
				uint8_t varBindingMask = 0;
				//! OR group was already satisfied by source terms.
				bool skipOr = false;

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
				enum class EOpcode : uint8_t { //
					//! X
					All_Simple,
					All_Wildcard,
					All_Complex,
					//! ?X
					Or_NoAll_Simple,
					Or_NoAll_Wildcard,
					Or_NoAll_Complex,
					Or_WithAll_Simple,
					Or_WithAll_Wildcard,
					Or_WithAll_Complex,
					//! !X
					Not_Simple,
					Not_Wildcard,
					Not_Complex,
					//! Seed current result set with all archetypes
					Seed_All,
					//! Checks if all variable terms are fully bound by runtime bindings
					Var_CheckBound,
					//! Filter current result set using variable terms
					Var_Filter,
					//! Filter current result set using variable terms (all variables pre-bound)
					Var_Filter_Bound,
					//! Filter current result set using a single-variable pair mixed matcher (bound)
					Var_Filter_Bound_1VarPairMixed,
					//! Filter current result set using ALL-only variable terms
					Var_Filter_AllOnly,
					//! Filter current result set using grouped ALL-only variable terms
					Var_Filter_AllOnlyGrouped,
					//! Filter current result set using a single-variable specialized matcher
					Var_Filter_1Var,
					//! Filter current result set using a single-variable source-gated OR matcher
					Var_Filter_1VarOrSource,
					//! Filter current result set using a single-variable pair mixed matcher
					Var_Filter_1VarPairMixed,
					//! Filter current result set using single-variable pair-intersection matcher
					Var_Filter_1VarPairAll,
					//! Filter current result set using two-variable pair-intersection matcher
					Var_Filter_2VarPairAll,
					//! Source term gates
					Src_AllTerm,
					Src_NotTerm,
					Src_OrTerm,
					//! Source traversal sub-opcodes (used by source terms)
					Src_Never,
					Src_Self,
					Src_Up,
					Src_Down,
					Src_UpDown
				};

				using VmLabel = uint16_t;

				struct CompiledOp {
					//! Opcode to execute
					EOpcode opcode;
					//! Stack position to go to if the opcode returns true
					VmLabel pc_ok;
					//! Stack position to go to if the opcode returns false
					VmLabel pc_fail;
					//! Optional opcode argument (e.g. index to a source term array).
					uint8_t arg = 0;
				};

				enum class EVarUnboundStrategy : uint8_t {
					Generic,
					AllOnly,
					AllOnlyGrouped,
					OneVar,
					OneVarOrSource,
					OneVarPairMixed,
					OneVarPairAll,
					TwoVarPairAll
				};

				struct QueryCompileCtx {
					struct SourceTermOp {
						EOpcode opcode = EOpcode::Src_Never;
						QueryTerm term{};
					};
					struct VarTermOp {
						EOpcode sourceOpcode = EOpcode::Src_Never;
						QueryTerm term{};
						uint8_t varMask = 0;
					};
					struct SingleVarTermRef {
						uint8_t termIdx = 0;
						uint8_t cost = 0;
					};
					struct SingleVarCheckOp {
						uint8_t termIdx = 0;
						uint8_t cost = 0;
						bool negate = false;
					};
					struct SingleVarPlan {
						cnt::sarray_ext<uint8_t, MAX_ITEMS_IN_QUERY> allAnchorTerms;
						cnt::sarray_ext<uint8_t, MAX_ITEMS_IN_QUERY> orAnchorTerms;
						cnt::sarray_ext<SingleVarCheckOp, MAX_ITEMS_IN_QUERY> requiredChecks;
						cnt::sarray_ext<SingleVarTermRef, MAX_ITEMS_IN_QUERY> orChecks;

						void clear() {
							allAnchorTerms.clear();
							orAnchorTerms.clear();
							requiredChecks.clear();
							orChecks.clear();
						}
					};
					struct SingleVarPairMixedPlan {
						cnt::sarray_ext<uint32_t, MAX_ITEMS_IN_QUERY> allRelIds;
						cnt::sarray_ext<uint32_t, MAX_ITEMS_IN_QUERY> orRelIds;
						cnt::sarray_ext<uint32_t, MAX_ITEMS_IN_QUERY> notRelIds;

						void clear() {
							allRelIds.clear();
							orRelIds.clear();
							notRelIds.clear();
						}
					};

					cnt::darray<CompiledOp> ops;
					//! Array of ops that can be evaluated with a ALL opcode
					cnt::sarray_ext<Entity, MAX_ITEMS_IN_QUERY> ids_all;
					//! Array of ops that can be evaluated with a OR opcode
					cnt::sarray_ext<Entity, MAX_ITEMS_IN_QUERY> ids_or;
					//! Array of ops that can be evaluated with a NOT opcode
					cnt::sarray_ext<Entity, MAX_ITEMS_IN_QUERY> ids_not;
					//! Source lookup terms for ALL
					cnt::sarray_ext<SourceTermOp, MAX_ITEMS_IN_QUERY> terms_all_src;
					//! Source lookup terms for OR
					cnt::sarray_ext<SourceTermOp, MAX_ITEMS_IN_QUERY> terms_or_src;
					//! Source lookup terms for NOT
					cnt::sarray_ext<SourceTermOp, MAX_ITEMS_IN_QUERY> terms_not_src;
					//! Variable terms for ALL
					cnt::sarray_ext<VarTermOp, MAX_ITEMS_IN_QUERY> terms_all_var;
					//! Variable terms for OR
					cnt::sarray_ext<VarTermOp, MAX_ITEMS_IN_QUERY> terms_or_var;
					//! Variable terms for NOT
					cnt::sarray_ext<VarTermOp, MAX_ITEMS_IN_QUERY> terms_not_var;
					//! Variable terms for ANY
					cnt::sarray_ext<VarTermOp, MAX_ITEMS_IN_QUERY> terms_any_var;
					//! Variable masks (Var0..Var7) used by variable terms.
					uint8_t varMaskAll = 0;
					uint8_t varMaskOr = 0;
					uint8_t varMaskNot = 0;
					uint8_t varMaskAny = 0;
					//! Selected unbound variable execution strategy.
					EVarUnboundStrategy varUnboundStrategy = EVarUnboundStrategy::Generic;
					//! Variables used by specialized strategies.
					uint8_t varIdx0 = 0;
					uint8_t varIdx1 = 0;
					uint8_t varAnchorTermIdx = (uint8_t)-1;
					SingleVarPlan var1Plan{};
					SingleVarPairMixedPlan var1PairMixedPlan{};
					uint8_t varGroupMask = 0;
					uint8_t varGroupSourceMask = 0;
					uint8_t varGroupAnchorTermIdx[8] = {};
					cnt::sarray_ext<uint8_t, MAX_ITEMS_IN_QUERY> varGroupTerms[8];
					cnt::sarray_ext<uint8_t, MAX_ITEMS_IN_QUERY> varGroupPairTerms[8];

					GAIA_NODISCARD bool has_source_terms() const {
						return !terms_all_src.empty() || !terms_or_src.empty() || !terms_not_src.empty();
					}

					GAIA_NODISCARD bool has_variable_terms() const {
						return !terms_all_var.empty() || !terms_or_var.empty() || !terms_not_var.empty() || !terms_any_var.empty();
					}

					GAIA_NODISCARD bool has_id_terms() const {
						return !ids_all.empty() || !ids_or.empty() || !ids_not.empty();
					}
				};

				GAIA_NODISCARD inline uint8_t source_term_cost(const QueryCompileCtx::SourceTermOp& termOp) {
					const bool depth1 = termOp.term.travDepth == 1;
					switch (termOp.opcode) {
						case EOpcode::Src_Never:
							return 0;
						case EOpcode::Src_Self:
							return 1;
						case EOpcode::Src_Up:
						case EOpcode::Src_Down:
							return depth1 ? 2 : 4;
						case EOpcode::Src_UpDown:
							return depth1 ? 3 : 5;
						default:
							return 6;
					}
				}

				GAIA_NODISCARD inline uint8_t bound_match_id_cost(Entity queryId) {
					if (!queryId.pair())
						return (!is_variable(queryId) && queryId.id() != All.id()) ? 1u : 3u;

					uint8_t cost = 0;
					cost += (!is_variable((EntityId)queryId.id()) && queryId.id() != All.id()) ? 1u : 3u;
					cost += (!is_variable((EntityId)queryId.gen()) && queryId.gen() != All.id()) ? 1u : 3u;
					return cost;
				}

				GAIA_NODISCARD inline uint8_t bound_term_cost(const QueryCompileCtx::VarTermOp& termOp) {
					uint8_t cost = bound_match_id_cost(termOp.term.id);
					if (termOp.term.src != EntityBad)
						cost = (uint8_t)(cost + source_term_cost({termOp.sourceOpcode, termOp.term}));
					return cost;
				}

				template <typename TermRefsArray>
				inline void sort_single_var_term_refs_by_cost(TermRefsArray& terms) {
					const auto cnt = (uint32_t)terms.size();
					if (cnt < 2)
						return;

					for (uint32_t i = 1; i < cnt; ++i) {
						const auto key = terms[i];

						uint32_t j = i;
						while (j > 0) {
							const auto prev = terms[j - 1];
							if (prev.cost < key.cost)
								break;
							if (prev.cost == key.cost && prev.termIdx <= key.termIdx)
								break;
							terms[j] = prev;
							--j;
						}

						terms[j] = key;
					}
				}

				template <typename ChecksArray>
				inline void sort_single_var_checks_by_cost(ChecksArray& checks) {
					const auto cnt = (uint32_t)checks.size();
					if (cnt < 2)
						return;

					for (uint32_t i = 1; i < cnt; ++i) {
						const auto key = checks[i];

						uint32_t j = i;
						while (j > 0) {
							const auto prev = checks[j - 1];
							if (prev.cost < key.cost)
								break;
							if (prev.cost == key.cost && prev.negate && !key.negate)
								break;
							if (prev.cost == key.cost && prev.negate == key.negate && prev.termIdx <= key.termIdx)
								break;
							checks[j] = prev;
							--j;
						}

						checks[j] = key;
					}
				}

				template <typename SourceTermsArray>
				inline void sort_source_terms_by_cost(SourceTermsArray& terms) {
					const auto cnt = (uint32_t)terms.size();
					if (cnt < 2)
						return;

					for (uint32_t i = 1; i < cnt; ++i) {
						auto key = terms[i];
						const auto keyCost = source_term_cost(key);

						uint32_t j = i;
						while (j > 0 && source_term_cost(terms[j - 1]) > keyCost) {
							terms[j] = terms[j - 1];
							--j;
						}

						terms[j] = key;
					}
				}

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
				// Operator OR (used by query::or_)
				struct OpOr {
					static bool check_mask(const QueryMask& maskArchetype, const QueryMask& maskQuery) {
						return match_entity_mask(maskArchetype, maskQuery);
					}
					static void restart(uint32_t& idx) {
						// OR terms are evaluated independently.
						idx = 0;
					}
					static bool can_continue([[maybe_unused]] bool hasMatch) {
						return true;
					}
					static bool eval(uint32_t expectedMatches, uint32_t totalMatches) {
						(void)expectedMatches;
						return totalMatches > 0;
					}
					static uint32_t handle_last_match(MatchingCtx& ctx, EntityLookupKey entityKey, uint32_t srcArchetypeCnt) {
						return handle_last_archetype_match(ctx.pLastMatchedArchetypeIdx_Or, entityKey, srcArchetypeCnt);
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

				GAIA_NODISCARD inline bool is_archetype_marked(const MatchingCtx& ctx, const Archetype* pArchetype) {
					if (ctx.pMatchesStampByArchetypeId == nullptr)
						return ctx.pMatchesSet->contains(pArchetype);

					const auto& stamps = *ctx.pMatchesStampByArchetypeId;
					const auto sid = (cnt::sparse_id)pArchetype->id();
					if (!stamps.has(sid))
						return false;

					return stamps[sid].epoch == ctx.matchesEpoch;
				}

				inline void mark_archetype_match(MatchingCtx& ctx, const Archetype* pArchetype) {
					if (ctx.pMatchesStampByArchetypeId == nullptr) {
						ctx.pMatchesSet->emplace(pArchetype);
						ctx.pMatchesArr->emplace_back(pArchetype);
						return;
					}

					auto& stamps = *ctx.pMatchesStampByArchetypeId;
					const auto sid = (cnt::sparse_id)pArchetype->id();
					if (stamps.has(sid))
						stamps.set(sid).epoch = ctx.matchesEpoch;
					else {
						ecs::ArchetypeMatchStamp stamp{};
						stamp.id = sid;
						stamp.epoch = ctx.matchesEpoch;
						stamps.add(stamp);
					}

					ctx.pMatchesSet->emplace(pArchetype);
					ctx.pMatchesArr->emplace_back(pArchetype);
				}

				inline void add_all_archetypes(MatchingCtx& ctx) {
					for (const auto* pArchetype: ctx.allArchetypes) {
						if (is_archetype_marked(ctx, pArchetype))
							continue;

						mark_archetype_match(ctx, pArchetype);
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
						// Some operators can keep moving forward (AND, OR), but NOT needs to start
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
					return match_res_as<OpOr>(w, archetype, EntitySpan{ids, 1});
				}

				GAIA_NODISCARD inline EOpcode source_opcode_from_term(const QueryTerm& term) {
					const bool includeSelf = query_trav_has(term.travKind, QueryTravKind::Self);
					const bool includeUp = query_trav_has(term.travKind, QueryTravKind::Up) && term.entTrav != EntityBad;
					const bool includeDown = query_trav_has(term.travKind, QueryTravKind::Down) && term.entTrav != EntityBad;
					if (!includeSelf && !includeUp && !includeDown)
						return EOpcode::Src_Never;
					if (includeSelf && !includeUp && !includeDown)
						return EOpcode::Src_Self;
					if (includeUp && includeDown)
						return EOpcode::Src_UpDown;
					if (includeUp)
						return EOpcode::Src_Up;
					return EOpcode::Src_Down;
				}

				template <typename Func>
				GAIA_NODISCARD inline bool
				each_lookup_source(const World& w, EOpcode opcode, const QueryTerm& term, Entity sourceEntity, Func&& func) {
					if (!valid(w, sourceEntity))
						return false;

					constexpr uint32_t MaxTraversalDepth = 2048;
					const uint32_t maxDepth =
							term.travDepth == QueryTermOptions::TravDepthUnlimited ? MaxTraversalDepth : (uint32_t)term.travDepth;
					const bool includeSelf = query_trav_has(term.travKind, QueryTravKind::Self);

					auto trav_up_1 = [&]() {
						const auto next = target(w, sourceEntity, term.entTrav);
						return next != EntityBad && next != sourceEntity && func(next);
					};

					auto trav_up_n = [&](bool includeSelf) {
						if (includeSelf && func(sourceEntity))
							return true;

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
					};

					auto trav_down_1 = [&]() {
						bool matched = false;
						sources_if(w, term.entTrav, sourceEntity, [&](Entity next) {
							if (func(next)) {
								matched = true;
								return false;
							}

							return true;
						});
						return matched;
					};

					auto trav_down_n = [&](bool includeSelf) {
						if (includeSelf && func(sourceEntity))
							return true;

						cnt::darray<Entity> queue;
						queue.push_back(sourceEntity);

						cnt::darray<uint32_t> levels;
						levels.push_back(0);

						cnt::set<EntityLookupKey> visited;
						visited.insert(EntityLookupKey(sourceEntity));

						for (uint32_t i = 0; i < queue.size(); ++i) {
							const auto source = queue[i];
							const auto level = levels[i];
							if (level >= maxDepth)
								continue;

							cnt::darray<Entity> children;
							sources(w, term.entTrav, source, [&](Entity next) {
								const auto key = EntityLookupKey(next);
								const auto ins = visited.insert(key);
								if (!ins.second)
									return;

								children.push_back(next);
							});

							core::sort(children, [](Entity left, Entity right) {
								return left.id() < right.id();
							});

							for (auto child: children) {
								if (func(child))
									return true;

								queue.push_back(child);
								levels.push_back(level + 1);
							}
						}

						return false;
					};

					switch (opcode) {
						case EOpcode::Src_Never:
							return false;
						case EOpcode::Src_Self:
							return func(sourceEntity);
						case EOpcode::Src_Down:
							if (maxDepth == 1)
								return (includeSelf && func(sourceEntity)) || trav_down_1();
							return trav_down_n(includeSelf);
						case EOpcode::Src_Up:
							if (maxDepth == 1)
								return (includeSelf && func(sourceEntity)) || trav_up_1();
							return trav_up_n(includeSelf);
						case EOpcode::Src_UpDown:
							if (includeSelf && func(sourceEntity))
								return true;
							if (maxDepth == 1)
								return trav_up_1() || trav_down_1();
							return trav_up_n(false) || trav_down_n(false);
						default:
							GAIA_ASSERT(false);
							return true;
					}
				}

				template <typename Func>
				GAIA_NODISCARD inline bool
				each_lookup_source(const World& w, const QueryTerm& term, Entity sourceEntity, Func&& func) {
					return each_lookup_source(w, source_opcode_from_term(term), term, sourceEntity, GAIA_FWD(func));
				}

				GAIA_NODISCARD inline bool match_source_term(const World& w, const QueryTerm& term, EOpcode opcode) {
					auto match_source_entity = [&](Entity source) {
						if (!valid(w, source))
							return false;

						auto* pArchetype = archetype_from_entity(w, source);
						if (pArchetype == nullptr)
							return false;

						return match_single_id_on_archetype(w, *pArchetype, term.id);
					};

					return each_lookup_source(w, opcode, term, term.src, match_source_entity);
				}

				GAIA_NODISCARD inline bool match_source_term(const World& w, const QueryTerm& term) {
					return match_source_term(w, term, source_opcode_from_term(term));
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

				template <typename Func>
				GAIA_NODISCARD inline bool each_term_match(
						const World& w, const Archetype& candidateArchetype, const QueryCompileCtx::VarTermOp& termOp,
						const VarBindings& varsIn, Func&& func);

				GAIA_NODISCARD inline bool term_has_match(
						const World& w, const Archetype& archetype, const QueryCompileCtx::VarTermOp& termOp,
						const VarBindings& varsIn) {
					return each_term_match(w, archetype, termOp, varsIn, [&](const VarBindings&) {
						return true;
					});
				}

				GAIA_NODISCARD inline uint32_t count_term_matches_limited(
						const World& w, const Archetype& archetype, const QueryCompileCtx::VarTermOp& termOp,
						const VarBindings& varsIn, uint32_t limit) {
					GAIA_ASSERT(limit > 0);

					uint32_t count = 0;
					each_term_match(w, archetype, termOp, varsIn, [&](const VarBindings&) {
						++count;
						return count >= limit;
					});
					return count;
				}

				GAIA_NODISCARD inline bool has_concrete_match_id(Entity queryId) {
					if (!queryId.pair())
						return !is_variable(queryId) && queryId.id() != All.id();

					return !is_variable((EntityId)queryId.id()) && queryId.id() != All.id() &&
								 !is_variable((EntityId)queryId.gen()) && queryId.gen() != All.id();
				}

				GAIA_NODISCARD inline bool
				match_id_bound(const World& w, const Archetype& archetype, Entity queryId, const VarBindings& vars) {
					auto archetypeIds = archetype.ids_view();
					const auto cnt = (uint32_t)archetypeIds.size();

					if (!queryId.pair()) {
						Entity queryToken = queryId;
						if (is_var_entity(queryToken)) {
							if (!var_is_bound(vars, queryToken))
								return false;
							queryToken = vars.values[var_index(queryToken)];
						}

						GAIA_FOR(cnt) {
							const auto idInArchetype = archetypeIds[i];
							if (idInArchetype.pair())
								continue;

							const auto value = entity_from_id(w, idInArchetype.id());
							if (value == EntityBad)
								continue;
							if (queryToken.id() != value.id())
								continue;

							return true;
						}

						return false;
					}

					auto queryRel = entity_from_id(w, queryId.id());
					auto queryTgt = entity_from_id(w, queryId.gen());
					if (queryRel == EntityBad || queryTgt == EntityBad)
						return false;

					if (is_var_entity(queryRel)) {
						if (!var_is_bound(vars, queryRel))
							return false;
						queryRel = vars.values[var_index(queryRel)];
					}

					if (is_var_entity(queryTgt)) {
						if (!var_is_bound(vars, queryTgt))
							return false;
						queryTgt = vars.values[var_index(queryTgt)];
					}

					const bool relIsConcrete = queryRel.id() != All.id();
					const bool tgtIsConcrete = queryTgt.id() != All.id();

					GAIA_FOR(cnt) {
						const auto idInArchetype = archetypeIds[i];
						if (!idInArchetype.pair())
							continue;

						if (relIsConcrete && idInArchetype.id() != queryRel.id())
							continue;
						if (tgtIsConcrete && idInArchetype.gen() != queryTgt.id())
							continue;

						if (!relIsConcrete) {
							const auto rel = entity_from_id(w, idInArchetype.id());
							if (rel == EntityBad)
								continue;
						}
						if (!tgtIsConcrete) {
							const auto tgt = entity_from_id(w, idInArchetype.gen());
							if (tgt == EntityBad)
								continue;
						}

						return true;
					}

					return false;
				}

				GAIA_NODISCARD inline bool term_has_match_bound(
						const World& w, const Archetype& candidateArchetype, const QueryCompileCtx::VarTermOp& termOp,
						const VarBindings& vars) {
					const auto& term = termOp.term;
					auto match_on_archetype = [&](const Archetype& archetype) {
						return match_id_bound(w, archetype, term.id, vars);
					};

					if (term.src == EntityBad)
						return match_on_archetype(candidateArchetype);

					auto sourceEntity = term.src;
					if (is_var_entity(sourceEntity)) {
						if (!var_is_bound(vars, sourceEntity))
							return false;
						sourceEntity = vars.values[var_index(sourceEntity)];
					}

					bool matched = false;
					each_lookup_source(w, termOp.sourceOpcode, term, sourceEntity, [&](Entity source) {
						auto* pSrcArchetype = archetype_from_entity(w, source);
						if (pSrcArchetype == nullptr)
							return false;
						if (!match_on_archetype(*pSrcArchetype))
							return false;
						matched = true;
						return true;
					});

					return matched;
				}

				template <typename Func>
				GAIA_NODISCARD inline bool each_id_match(
						const World& w, const Archetype& archetype, Entity queryId, const VarBindings& varsIn, Func&& func) {
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

							if (func(vars))
								return true;
						}
						return false;
					}

					const auto queryRel = entity_from_id(w, queryId.id());
					const auto queryTgt = entity_from_id(w, queryId.gen());
					if (queryRel == EntityBad || queryTgt == EntityBad)
						return false;
					const bool relIsConcrete = !is_var_entity(queryRel) && queryRel.id() != All.id();
					const bool tgtIsConcrete = !is_var_entity(queryTgt) && queryTgt.id() != All.id();

					GAIA_FOR(cnt) {
						const auto idInArchetype = archetypeIds[i];
						if (!idInArchetype.pair())
							continue;

						if (relIsConcrete && idInArchetype.id() != queryRel.id())
							continue;
						if (tgtIsConcrete && idInArchetype.gen() != queryTgt.id())
							continue;

						if (relIsConcrete && tgtIsConcrete) {
							if (func(varsIn))
								return true;
							continue;
						}

						auto vars = varsIn;
						if (!relIsConcrete) {
							const auto rel = entity_from_id(w, idInArchetype.id());
							if (rel == EntityBad)
								continue;
							if (!match_token(vars, queryRel, rel, true))
								continue;
						}
						if (!tgtIsConcrete) {
							const auto tgt = entity_from_id(w, idInArchetype.gen());
							if (tgt == EntityBad)
								continue;
							if (!match_token(vars, queryTgt, tgt, true))
								continue;
						}

						if (func(vars))
							return true;
					}

					return false;
				}

				template <typename Func>
				GAIA_NODISCARD inline bool each_term_match(
						const World& w, const Archetype& candidateArchetype, const QueryCompileCtx::VarTermOp& termOp,
						const VarBindings& varsIn, Func&& func) {
					const auto& term = termOp.term;
					auto&& matchFunc = GAIA_FWD(func);
					auto each_on_source = [&](Entity sourceEntity, const VarBindings& vars) {
						return each_lookup_source(w, termOp.sourceOpcode, term, sourceEntity, [&](Entity source) {
							auto* pSrcArchetype = archetype_from_entity(w, source);
							if (pSrcArchetype == nullptr)
								return false;

							return each_id_match(w, *pSrcArchetype, term.id, vars, matchFunc);
						});
					};

					if (term.src == EntityBad)
						return each_id_match(w, candidateArchetype, term.id, varsIn, matchFunc);

					if (is_var_entity(term.src)) {
						if (!var_is_bound(varsIn, term.src))
							return false;

						const auto source = varsIn.values[var_index(term.src)];
						return each_on_source(source, varsIn);
					}

					return each_on_source(term.src, varsIn);
				}

				template <typename OpKind, MatchingStyle Style>
				inline void match_archetype_inter(MatchingCtx& ctx, std::span<const Archetype*> archetypes) {
					if constexpr (Style == MatchingStyle::Complex) {
						for (const auto* pArchetype: archetypes) {
							if (is_archetype_marked(ctx, pArchetype))
								continue;

							if (!match_res_as<OpKind>(*ctx.pWorld, *pArchetype, ctx.idsToMatch))
								continue;

							mark_archetype_match(ctx, pArchetype);
						}
					}
#if GAIA_USE_PARTITIONED_BLOOM_FILTER >= 0
					else if constexpr (Style == MatchingStyle::Simple) {
						for (const auto* pArchetype: archetypes) {
							if (is_archetype_marked(ctx, pArchetype))
								continue;

							// Try early exit
							if (!OpKind::check_mask(pArchetype->queryMask(), ctx.queryMask))
								continue;

							if (!match_res<OpKind>(*pArchetype, ctx.idsToMatch))
								continue;

							mark_archetype_match(ctx, pArchetype);
						}
					}
#endif
					else {
						for (const auto* pArchetype: archetypes) {
							if (is_archetype_marked(ctx, pArchetype))
								continue;

							if (!match_res<OpKind>(*pArchetype, ctx.idsToMatch))
								continue;

							mark_archetype_match(ctx, pArchetype);
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

				template <MatchingStyle Style>
				inline void match_archetype_or(MatchingCtx& ctx) {
					EntityLookupKey entityKey(ctx.ent);

					// For OR we need at least one archetype to match.
					// However, because any of them can match, we need to check them all.
					// Iterating all of them is caller's responsibility.
					auto archetypes =
							fetch_archetypes_for_select(*ctx.pEntityToArchetypeMap, ctx.allArchetypes, ctx.ent, entityKey);
					if (archetypes.empty())
						return;

					match_archetype_inter<OpOr, Style>(ctx, entityKey, archetypes);
				}

				inline void match_archetype_or_as(MatchingCtx& ctx) {
					EntityLookupKey entityKey = EntityBadLookupKey;

					// For OR we need at least one archetype to match.
					// However, because any of them can match, we need to check them all.
					// Iterating all of them is caller's responsibility.
					if (ctx.ent.id() == Is.id()) {
						ctx.ent = EntityBad;
						match_archetype_inter<OpOr, MatchingStyle::Complex>(ctx, entityKey, ctx.allArchetypes);
					} else {
						entityKey = EntityLookupKey(ctx.ent);

						auto archetypes =
								fetch_archetypes_for_select(*ctx.pEntityToArchetypeMap, ctx.allArchetypes, ctx.ent, entityKey);
						if (archetypes.empty())
							return;

						match_archetype_inter<OpOr, MatchingStyle::Complex>(ctx, entityKey, archetypes);
					}
				}

				template <MatchingStyle Style>
				inline void match_archetype_no_2(MatchingCtx& ctx) {
					// We had some matches already (with ALL or OR). We need to remove those
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

				template <typename OpKind, MatchingStyle Style, bool WildcardWithAsFallback = false>
				inline void filter_current_matches(MatchingCtx& ctx, EntitySpan idsToMatch) {
					if constexpr (Style == MatchingStyle::Complex) {
						for (uint32_t i = 0; i < ctx.pMatchesArr->size();) {
							const auto* pArchetype = (*ctx.pMatchesArr)[i];
							if (match_res_as<OpKind>(*ctx.pWorld, *pArchetype, idsToMatch)) {
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
							if (OpKind::check_mask(pArchetype->queryMask(), ctx.queryMask) &&
									match_res<OpKind>(*pArchetype, idsToMatch)) {
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
							if (match_res<OpKind>(*pArchetype, idsToMatch) ||
									(WildcardWithAsFallback && match_res_as<OpKind>(*ctx.pWorld, *pArchetype, idsToMatch))) {
								++i;
								continue;
							}

							core::swap_erase(*ctx.pMatchesArr, i);
						}
					}
				}

				template <MatchingStyle Style>
				GAIA_NODISCARD inline bool exec_not_impl(const QueryCompileCtx& comp, MatchingCtx& ctx) {
					ctx.idsToMatch = std::span{comp.ids_not.data(), comp.ids_not.size()};
					ctx.pMatchesSet->clear();

					if (ctx.targetEntities.empty()) {
						// We searched for nothing more than NOT matches
						if (ctx.pMatchesArr->empty()) {
							// If there are no previous matches (no ALL or OR matches),
							// we need to search among all archetypes.
							match_archetype_inter<detail::OpNo, Style>(ctx, EntityBadLookupKey, ctx.allArchetypes);
						} else {
							match_archetype_no_2<Style>(ctx);
						}
					} else {
						// We searched for nothing more than NOT matches
						if (ctx.pMatchesArr->empty())
							match_archetype_inter<detail::OpNo, Style>(ctx, ctx.allArchetypes);
						else
							match_archetype_no_2<Style>(ctx);
					}

					return true;
				}

				template <MatchingStyle Style>
				GAIA_NODISCARD inline bool exec_all_impl(const QueryCompileCtx& comp, MatchingCtx& ctx) {
					ctx.ent = comp.ids_all[0];
					ctx.idsToMatch = std::span{comp.ids_all.data(), comp.ids_all.size()};

					if (ctx.targetEntities.empty())
						match_archetype_all<Style>(ctx);
					else
						match_archetype_inter<OpAll, Style>(ctx, ctx.allArchetypes);

					// If no ALL matches were found, we can quit right away.
					return !ctx.pMatchesArr->empty();
				}

				template <MatchingStyle Style>
				GAIA_NODISCARD inline bool exec_or_noall_impl(const QueryCompileCtx& comp, MatchingCtx& ctx) {
					if (ctx.skipOr)
						return true;

					const auto cnt = comp.ids_or.size();
					// Try find matches with OR components.
					GAIA_FOR(cnt) {
						ctx.ent = comp.ids_or[i];
						const Entity idsToMatchData[1] = {ctx.ent};
						ctx.idsToMatch = EntitySpan{idsToMatchData, 1};

						if constexpr (Style == MatchingStyle::Complex)
							match_archetype_or_as(ctx);
						else
							match_archetype_or<Style>(ctx);
					}

					return true;
				}

				template <MatchingStyle Style>
				GAIA_NODISCARD inline bool exec_or_withall_impl(const QueryCompileCtx& comp, MatchingCtx& ctx) {
					if (ctx.skipOr)
						return true;

					ctx.idsToMatch = std::span{comp.ids_or.data(), comp.ids_or.size()};

					if constexpr (Style == MatchingStyle::Complex)
						filter_current_matches<OpOr, MatchingStyle::Complex>(ctx, ctx.idsToMatch);
					else if constexpr (Style == MatchingStyle::Simple)
						filter_current_matches<OpOr, MatchingStyle::Simple>(ctx, ctx.idsToMatch);
					else
						filter_current_matches<OpOr, MatchingStyle::Wildcard, true>(ctx, ctx.idsToMatch);

					return true;
				}

				template <typename SourceTermsArray>
				GAIA_NODISCARD inline const QueryCompileCtx::SourceTermOp&
				get_source_term_op(const QueryCompileCtx& comp, const MatchingCtx& ctx, const SourceTermsArray& terms) {
					const auto& stackItem = comp.ops[ctx.pc];
					GAIA_ASSERT(stackItem.arg < terms.size());
					return terms[stackItem.arg];
				}
			} // namespace detail

			class VirtualMachine {
				static constexpr uint32_t OpcodeArgLimit = 256u;
				static_assert(
						MAX_ITEMS_IN_QUERY <= OpcodeArgLimit,
						"CompiledOp::arg is uint8_t. Increase arg width if query term capacity grows above 256.");

				detail::QueryCompileCtx m_compCtx;

			private:
				static const char* opcode_name(detail::EOpcode opcode) {
					static const char* s_names[] = {
							"all", //
							"allw", //
							"allc", //
							"or", //
							"orw", //
							"orc", //
							"ora", //
							"oraw", //
							"orac", //
							"not", //
							"notw", //
							"notc", //
							"seed", //
							"varcb", //
							"varf", //
							"varfb", //
							"varfb1pm", //
							"varfa", //
							"varfag", //
							"varf1", //
							"varf1os", //
							"varf1pm", //
							"varf1p", //
							"varf2p", //
							"src_all_t", //
							"src_not_t", //
							"src_or_t", //
							"nev", //
							"self", //
							"up", //
							"down", //
							"updown", //
					};
					static_assert(
							sizeof(s_names) / sizeof(s_names[0]) == (uint32_t)detail::EOpcode::Src_UpDown + 1u,
							"Opcode name table out of sync with EOpcode.");
					return s_names[(uint32_t)opcode];
				}

				GAIA_NODISCARD static bool opcode_has_arg(detail::EOpcode opcode) {
					return opcode == detail::EOpcode::Src_AllTerm || //
								 opcode == detail::EOpcode::Src_NotTerm || //
								 opcode == detail::EOpcode::Src_OrTerm;
				}

				static void append_uint(util::str& out, uint32_t value) {
					char buffer[32];
					const auto len = GAIA_STRFMT(buffer, sizeof(buffer), "%u", value);
					GAIA_ASSERT(len >= 0);
					out.append(buffer, (uint32_t)len);
				}

				static void append_cstr(util::str& out, const char* value) {
					GAIA_ASSERT(value != nullptr);
					out.append(value, (uint32_t)GAIA_STRLEN(value, 16));
				}

				static void append_id_expr(util::str& out, const World& world, EntityId id) {
					if (is_variable(id)) {
						out.append('$');
						append_uint(out, (uint32_t)(id - Var0.id()));
						return;
					}

					if (id == All.id()) {
						out.append('*');
						return;
					}

					const auto entity = entity_from_id(world, id);
					if (entity != EntityBad)
						append_entity_expr(out, world, entity);
					else {
						out.append('#');
						append_uint(out, (uint32_t)id);
					}
				}

				static void append_entity_expr(util::str& out, const World& world, Entity entity) {
					if (entity == EntityBad) {
						out.append("EntityBad");
						return;
					}

					if (entity.pair()) {
						out.append('(');
						append_id_expr(out, world, (EntityId)entity.id());
						out.append(',');
						append_id_expr(out, world, (EntityId)entity.gen());
						out.append(')');
						return;
					}

					if (is_variable(EntityId(entity.id()))) {
						out.append('$');
						append_uint(out, (uint32_t)(entity.id() - Var0.id()));
						return;
					}

					if (entity.id() == All.id()) {
						out.append('*');
						return;
					}

					const auto* name = entity_name(world, entity);
					if (name != nullptr) {
						out.append(name, (uint32_t)GAIA_STRLEN(name, 256));
						return;
					}

					append_uint(out, entity.id());
					out.append('.');
					append_uint(out, entity.gen());
				}

				static void append_term_expr(util::str& out, const World& world, const QueryTerm& term) {
					append_entity_expr(out, world, term.id);
					out.append('(');
					if (term.src == EntityBad)
						out.append("$this");
					else
						append_entity_expr(out, world, term.src);
					out.append(')');

					if (term.entTrav != EntityBad) {
						out.append(" trav=");
						append_entity_expr(out, world, term.entTrav);
						out.append(" depth=");
						if (term.travDepth == QueryTermOptions::TravDepthUnlimited)
							out.append('*');
						else
							append_uint(out, (uint32_t)term.travDepth);
					}
				}

				static void
				append_ids_section(util::str& out, const char* title, std::span<const Entity> ids, const World& world) {
					if (ids.empty())
						return;

					append_cstr(out, title);
					out.append(": ");
					append_uint(out, (uint32_t)ids.size());
					out.append('\n');

					const auto cnt = (uint32_t)ids.size();
					GAIA_FOR(cnt) {
						out.append("  [");
						append_uint(out, i);
						out.append("] ");
						append_entity_expr(out, world, ids[i]);
						out.append('\n');
					}
				}

				static void append_source_terms_section(
						util::str& out, const char* title,
						const cnt::sarray_ext<detail::QueryCompileCtx::SourceTermOp, MAX_ITEMS_IN_QUERY>& terms,
						const World& world) {
					if (terms.empty())
						return;

					append_cstr(out, title);
					out.append(": ");
					append_uint(out, (uint32_t)terms.size());
					out.append('\n');

					const auto cnt = (uint32_t)terms.size();
					GAIA_FOR(cnt) {
						out.append("  [");
						append_uint(out, i);
						out.append("] ");
						append_cstr(out, opcode_name(terms[i].opcode));
						out.append(" id=");
						append_term_expr(out, world, terms[i].term);
						out.append('\n');
					}
				}

				static void append_var_terms_section(
						util::str& out, const char* title,
						const cnt::sarray_ext<detail::QueryCompileCtx::VarTermOp, MAX_ITEMS_IN_QUERY>& terms, const World& world) {
					if (terms.empty())
						return;

					append_cstr(out, title);
					out.append(": ");
					append_uint(out, (uint32_t)terms.size());
					out.append('\n');

					const auto cnt = (uint32_t)terms.size();
					GAIA_FOR(cnt) {
						out.append("  [");
						append_uint(out, i);
						out.append("] ");
						append_cstr(out, opcode_name(terms[i].sourceOpcode));
						out.append(" id=");
						append_term_expr(out, world, terms[i].term);
						out.append('\n');
					}
				}

			private:
				GAIA_NODISCARD static detail::VarBindings make_initial_var_bindings(const MatchingCtx& ctx) {
					detail::VarBindings vars{};
					vars.mask = ctx.varBindingMask;
					GAIA_FOR(detail::VarBindings::VarCnt) {
						const auto bit = (uint8_t(1) << i);
						if ((vars.mask & bit) == 0)
							continue;
						vars.values[i] = ctx.varBindings[i];
					}
					return vars;
				}

				GAIA_NODISCARD static uint8_t
				term_unbound_var_mask(const World& world, const QueryTerm& term, const detail::VarBindings& vars) {
					uint8_t mask = 0;

					if (detail::is_var_entity(term.src) && !detail::var_is_bound(vars, term.src))
						mask |= (uint8_t(1) << detail::var_index(term.src));

					if (!term.id.pair()) {
						const auto idEnt = entity_from_id(world, term.id.id());
						if (detail::is_var_entity(idEnt) && !detail::var_is_bound(vars, idEnt))
							mask |= (uint8_t(1) << detail::var_index(idEnt));
						return mask;
					}

					const auto relEnt = entity_from_id(world, term.id.id());
					if (detail::is_var_entity(relEnt) && !detail::var_is_bound(vars, relEnt))
						mask |= (uint8_t(1) << detail::var_index(relEnt));

					const auto tgtEnt = entity_from_id(world, term.id.gen());
					if (detail::is_var_entity(tgtEnt) && !detail::var_is_bound(vars, tgtEnt))
						mask |= (uint8_t(1) << detail::var_index(tgtEnt));

					return mask;
				}

				GAIA_NODISCARD bool all_variable_terms_bound(const MatchingCtx& ctx) const {
					uint8_t requiredMask = (uint8_t)(m_compCtx.varMaskAll | m_compCtx.varMaskNot);
					const bool orAlreadySatisfied = !m_compCtx.ids_or.empty() || ctx.skipOr;
					if (!orAlreadySatisfied)
						requiredMask = (uint8_t)(requiredMask | m_compCtx.varMaskOr);
					return (ctx.varBindingMask & requiredMask) == requiredMask;
				}

				GAIA_NODISCARD bool eval_variable_terms_bound_on_archetype(
						const MatchingCtx& ctx, const Archetype& archetype, bool orAlreadySatisfied) const {
					using namespace detail;

					const auto vars = make_initial_var_bindings(ctx);

					for (const auto& term: m_compCtx.terms_all_var) {
						if (!term_has_match_bound(*ctx.pWorld, archetype, term, vars))
							return false;
					}

					if (!orAlreadySatisfied && !m_compCtx.terms_or_var.empty()) {
						bool orMatched = false;
						for (const auto& term: m_compCtx.terms_or_var) {
							if (!term_has_match_bound(*ctx.pWorld, archetype, term, vars))
								continue;
							orMatched = true;
							break;
						}
						if (!orMatched)
							return false;
					}

					for (const auto& term: m_compCtx.terms_not_var) {
						if (term_has_match_bound(*ctx.pWorld, archetype, term, vars))
							return false;
					}

					return true;
				}

				GAIA_NODISCARD bool eval_variable_terms_bound_1var_pairmixed_on_archetype(
						const MatchingCtx& ctx, const Archetype& archetype, bool orAlreadySatisfied) const {
					using namespace detail;

					GAIA_ASSERT(m_compCtx.varUnboundStrategy == detail::EVarUnboundStrategy::OneVarPairMixed);
					const auto vars = make_initial_var_bindings(ctx);
					const auto varEntity = entity_from_id(*ctx.pWorld, (EntityId)(Var0.id() + m_compCtx.varIdx0));
					GAIA_ASSERT(varEntity != EntityBad);
					if (!var_is_bound(vars, varEntity))
						return false;

					return match_1var_pairmixed_candidate_on_archetype(
							archetype, vars.values[var_index(varEntity)], orAlreadySatisfied, false);
				}

				GAIA_NODISCARD bool init_pairall_group_anchor_on_archetype(
						const Archetype& archetype, Entity varEntity, uint32_t& anchorTermIdx, uint32_t& anchorMatchCnt) const {
					using namespace detail;

					const auto varIdx = (uint8_t)var_index(varEntity);
					const auto archetypeIds = archetype.ids_view();
					const auto idsCnt = (uint32_t)archetypeIds.size();
					const auto& groupPairTerms = m_compCtx.varGroupPairTerms[varIdx];

					anchorTermIdx = (uint32_t)-1;
					anchorMatchCnt = (uint32_t)-1;
					for (const auto termIdx8: groupPairTerms) {
						const auto termIdx = (uint32_t)termIdx8;
						const auto& termOp = m_compCtx.terms_all_var[termIdx];

						uint32_t cnt = 0;
						const auto relId = termOp.term.id.id();
						for (uint32_t idIdx = 0; idIdx < idsCnt; ++idIdx) {
							const auto idInArchetype = archetypeIds[idIdx];
							if (!idInArchetype.pair())
								continue;
							if (idInArchetype.id() != relId)
								continue;
							++cnt;
						}

						if (cnt == 0)
							return false;

						if (anchorTermIdx == (uint32_t)-1 || cnt < anchorMatchCnt) {
							anchorTermIdx = termIdx;
							anchorMatchCnt = cnt;
							if (anchorMatchCnt == 1)
								break;
						}
					}

					return true;
				}

				GAIA_NODISCARD uint32_t count_group_source_anchor_candidates(const MatchingCtx& ctx, uint8_t varIdx) const {
					const auto bit = (uint8_t)(uint8_t(1) << varIdx);
					if ((m_compCtx.varGroupSourceMask & bit) == 0 || ctx.pEntityToArchetypeMap == nullptr)
						return (uint32_t)-1;

					const auto anchorTermIdx = (uint32_t)m_compCtx.varGroupAnchorTermIdx[varIdx];
					GAIA_ASSERT(anchorTermIdx < m_compCtx.terms_all_var.size());
					const auto& anchorTerm = m_compCtx.terms_all_var[anchorTermIdx];
					const auto sourceArchetypes = fetch_archetypes_for_select(
							*ctx.pEntityToArchetypeMap, ctx.allArchetypes, anchorTerm.term.id, anchorTerm.term.id);

					uint32_t count = 0;
					for (const auto* pSourceArchetype: sourceArchetypes) {
						for (const auto* pChunk: pSourceArchetype->chunks())
							count += pChunk->size();
					}

					return count;
				}

				GAIA_NODISCARD bool match_pairall_group_on_archetype(
						const MatchingCtx& ctx, const Archetype& archetype, const detail::VarBindings& varsBase,
						uint8_t varIdx) const {
					using namespace detail;

					const auto varEntity = entity_from_id(*ctx.pWorld, (EntityId)(Var0.id() + varIdx));
					GAIA_ASSERT(varEntity != EntityBad);
					const auto& groupTerms = m_compCtx.varGroupTerms[varIdx];

					if (var_is_bound(varsBase, varEntity)) {
						bool hasTerms = false;
						for (const auto termIdx8: groupTerms) {
							const auto& termOp = m_compCtx.terms_all_var[(uint32_t)termIdx8];
							hasTerms = true;
							if (!term_has_match_bound(*ctx.pWorld, archetype, termOp, varsBase))
								return false;
						}
						return hasTerms;
					}

					uint32_t anchorTermIdx = (uint32_t)-1;
					uint32_t anchorMatchCnt = (uint32_t)-1;
					if (!init_pairall_group_anchor_on_archetype(archetype, varEntity, anchorTermIdx, anchorMatchCnt))
						return false;

					if (anchorTermIdx == (uint32_t)-1)
						return false;

					const auto archetypeIds = archetype.ids_view();
					const auto idsCnt = (uint32_t)archetypeIds.size();
					const auto anchorRelId = m_compCtx.terms_all_var[anchorTermIdx].term.id.id();
					for (uint32_t idIdx = 0; idIdx < idsCnt; ++idIdx) {
						const auto idInArchetype = archetypeIds[idIdx];
						if (!idInArchetype.pair())
							continue;
						if (idInArchetype.id() != anchorRelId)
							continue;

						const auto target = entity_from_id(*ctx.pWorld, idInArchetype.gen());
						if (target == EntityBad)
							continue;

						auto vars = varsBase;
						if (!bind_var(vars, varEntity, target))
							continue;

						bool matched = true;
						for (const auto termIdx8: groupTerms) {
							const auto termIdx = (uint32_t)termIdx8;
							const auto& termOp = m_compCtx.terms_all_var[termIdx];
							if (termIdx == anchorTermIdx)
								continue;
							if (term_has_match_bound(*ctx.pWorld, archetype, termOp, vars))
								continue;
							matched = false;
							break;
						}

						if (matched)
							return true;
					}

					return false;
				}

				GAIA_NODISCARD bool match_variable_terms_1var_bound_state(
						const MatchingCtx& ctx, const Archetype& archetype, Entity varEntity, const detail::VarBindings& vars,
						bool orAlreadySatisfied, bool orMatched, uint32_t skipAllTermIdx = (uint32_t)-1) const {
					using namespace detail;

					if (!var_is_bound(vars, varEntity))
						return false;

					const auto run_required_checks = [&]() {
						for (const auto& check: m_compCtx.var1Plan.requiredChecks) {
							if (!check.negate) {
								if ((uint32_t)check.termIdx == skipAllTermIdx)
									continue;

								const auto& term = m_compCtx.terms_all_var[(uint32_t)check.termIdx];
								if (!term_has_match_bound(*ctx.pWorld, archetype, term, vars))
									return false;
								continue;
							}

							const auto& term = m_compCtx.terms_not_var[(uint32_t)check.termIdx];
							if (term_has_match_bound(*ctx.pWorld, archetype, term, vars))
								return false;
						}

						return true;
					};

					const auto run_or_checks = [&]() {
						for (const auto& orCheck: m_compCtx.var1Plan.orChecks) {
							const auto& term = m_compCtx.terms_or_var[(uint32_t)orCheck.termIdx];
							if (!term_has_match_bound(*ctx.pWorld, archetype, term, vars))
								continue;
							return true;
						}

						return false;
					};

					bool orSatisfied = orAlreadySatisfied || orMatched;
					const auto requiredCost =
							m_compCtx.var1Plan.requiredChecks.empty() ? uint8_t(0xff) : m_compCtx.var1Plan.requiredChecks[0].cost;
					const auto orCost = (!orSatisfied && !m_compCtx.var1Plan.orChecks.empty())
																	? m_compCtx.var1Plan.orChecks[0].cost
																	: uint8_t(0xff);

					if (!orSatisfied && orCost < requiredCost) {
						if (!run_or_checks())
							return false;
						orSatisfied = true;
					}

					if (!run_required_checks())
						return false;

					if (!orSatisfied && !m_compCtx.var1Plan.orChecks.empty() && !run_or_checks())
						return false;

					return true;
				}

				GAIA_NODISCARD bool match_allonly_group_bound_state(
						const MatchingCtx& ctx, const Archetype& archetype, Entity varEntity, const detail::VarBindings& vars,
						uint32_t skipAllTermIdx = (uint32_t)-1) const {
					using namespace detail;

					if (!var_is_bound(vars, varEntity))
						return false;

					bool hasTerms = false;
					const auto& groupTerms = m_compCtx.varGroupTerms[var_index(varEntity)];
					for (const auto termIdx8: groupTerms) {
						const auto termIdx = (uint32_t)termIdx8;
						if (termIdx == skipAllTermIdx)
							continue;

						const auto& term = m_compCtx.terms_all_var[termIdx];
						hasTerms = true;
						if (!term_has_match_bound(*ctx.pWorld, archetype, term, vars))
							return false;
					}

					return hasTerms;
				}

				GAIA_NODISCARD bool match_allonly_group_on_archetype(
						const MatchingCtx& ctx, const Archetype& archetype, const detail::VarBindings& varsBase,
						uint8_t varIdx) const {
					using namespace detail;

					const auto varEntity = entity_from_id(*ctx.pWorld, (EntityId)(Var0.id() + varIdx));
					GAIA_ASSERT(varEntity != EntityBad);

					if (var_is_bound(varsBase, varEntity))
						return match_allonly_group_bound_state(ctx, archetype, varEntity, varsBase);

					const auto anchorTermIdx = (uint32_t)m_compCtx.varGroupAnchorTermIdx[varIdx];
					const auto groupBit = (uint8_t)(uint8_t(1) << varIdx);
					const bool hasSourceAnchor = (m_compCtx.varGroupSourceMask & groupBit) != 0;
					if (!hasSourceAnchor)
						return match_pairall_group_on_archetype(ctx, archetype, varsBase, varIdx);

					if (ctx.pEntityToArchetypeMap == nullptr)
						return eval_variable_terms_allonly_on_archetype(ctx, archetype, false);

					uint32_t pairAnchorTermIdx = (uint32_t)-1;
					uint32_t pairAnchorMatchCnt = (uint32_t)-1;
					if (!init_pairall_group_anchor_on_archetype(archetype, varEntity, pairAnchorTermIdx, pairAnchorMatchCnt))
						return false;

					const uint32_t sourceCandidateCnt = count_group_source_anchor_candidates(ctx, varIdx);
					if (pairAnchorTermIdx != (uint32_t)-1 && pairAnchorMatchCnt <= sourceCandidateCnt) {
						const auto archetypeIds = archetype.ids_view();
						const auto idsCnt = (uint32_t)archetypeIds.size();
						const auto anchorRelId = m_compCtx.terms_all_var[pairAnchorTermIdx].term.id.id();
						for (uint32_t idIdx = 0; idIdx < idsCnt; ++idIdx) {
							const auto idInArchetype = archetypeIds[idIdx];
							if (!idInArchetype.pair())
								continue;
							if (idInArchetype.id() != anchorRelId)
								continue;

							const auto target = entity_from_id(*ctx.pWorld, idInArchetype.gen());
							if (target == EntityBad)
								continue;

							auto vars = varsBase;
							if (!bind_var(vars, varEntity, target))
								continue;

							if (match_allonly_group_bound_state(ctx, archetype, varEntity, vars))
								return true;
						}

						return false;
					}

					GAIA_ASSERT(anchorTermIdx < m_compCtx.terms_all_var.size());
					const auto& anchorTerm = m_compCtx.terms_all_var[anchorTermIdx];
					GAIA_ASSERT(anchorTerm.term.src == varEntity);
					GAIA_ASSERT(anchorTerm.sourceOpcode == detail::EOpcode::Src_Self);

					const auto sourceArchetypes = fetch_archetypes_for_select(
							*ctx.pEntityToArchetypeMap, ctx.allArchetypes, anchorTerm.term.id, anchorTerm.term.id);
					for (const auto* pSourceArchetype: sourceArchetypes) {
						for (const auto* pChunk: pSourceArchetype->chunks()) {
							auto sourceEntities = pChunk->entity_view();
							const auto sourceCnt = (uint32_t)sourceEntities.size();
							GAIA_FOR(sourceCnt) {
								auto vars = varsBase;
								if (!bind_var(vars, varEntity, sourceEntities[i]))
									continue;

								if (match_allonly_group_bound_state(ctx, archetype, varEntity, vars, anchorTermIdx))
									return true;
							}
						}
					}

					return false;
				}

				GAIA_NODISCARD bool eval_variable_terms_1var_on_archetype(
						const MatchingCtx& ctx, const Archetype& archetype, [[maybe_unused]] bool orAlreadySatisfied) const {
					using namespace detail;

					GAIA_ASSERT(
							m_compCtx.varUnboundStrategy == detail::EVarUnboundStrategy::OneVar ||
							m_compCtx.varUnboundStrategy == detail::EVarUnboundStrategy::OneVarOrSource);
					GAIA_ASSERT(m_compCtx.terms_any_var.empty());
					GAIA_ASSERT(!m_compCtx.terms_all_var.empty() || !m_compCtx.terms_or_var.empty());

					const auto varEntity = entity_from_id(*ctx.pWorld, (EntityId)(Var0.id() + m_compCtx.varIdx0));
					GAIA_ASSERT(varEntity != EntityBad);
					const auto varsBase = make_initial_var_bindings(ctx);

					enum class EVar1AnchorKind : uint8_t { None, All, Or };

					constexpr uint32_t MatchProbeLimit = 64;
					EVar1AnchorKind bestKind = EVar1AnchorKind::None;
					uint32_t bestTermIdx = (uint32_t)-1;
					uint32_t bestMatchCnt = MatchProbeLimit + 1;
					for (const auto termIdx8: m_compCtx.var1Plan.allAnchorTerms) {
						const auto i = (uint32_t)termIdx8;
						const auto& termOp = m_compCtx.terms_all_var[i];
						const auto matchCnt = count_term_matches_limited(*ctx.pWorld, archetype, termOp, varsBase, bestMatchCnt);
						if (matchCnt == 0)
							return false;

						if (bestKind == EVar1AnchorKind::None || matchCnt < bestMatchCnt) {
							bestKind = EVar1AnchorKind::All;
							bestTermIdx = i;
							bestMatchCnt = matchCnt;
						}
					}

					for (const auto termIdx8: m_compCtx.var1Plan.orAnchorTerms) {
						const auto i = (uint32_t)termIdx8;
						const auto& termOp = m_compCtx.terms_or_var[i];
						const auto matchCnt = count_term_matches_limited(*ctx.pWorld, archetype, termOp, varsBase, bestMatchCnt);
						if (matchCnt == 0)
							continue;

						if (bestKind == EVar1AnchorKind::None || matchCnt < bestMatchCnt) {
							bestKind = EVar1AnchorKind::Or;
							bestTermIdx = i;
							bestMatchCnt = matchCnt;
							if (bestMatchCnt == 1)
								break;
						}
					}

					if (bestKind == EVar1AnchorKind::All) {
						const auto& anchorTerm = m_compCtx.terms_all_var[bestTermIdx];
						return each_term_match(*ctx.pWorld, archetype, anchorTerm, varsBase, [&](const VarBindings& vars) {
							return match_variable_terms_1var_bound_state(
									ctx, archetype, varEntity, vars, orAlreadySatisfied, false, bestTermIdx);
						});
					}

					if (bestKind == EVar1AnchorKind::Or) {
						const auto& termOp = m_compCtx.terms_or_var[bestTermIdx];
						if (each_term_match(*ctx.pWorld, archetype, termOp, varsBase, [&](const VarBindings& vars) {
									return match_variable_terms_1var_bound_state(
											ctx, archetype, varEntity, vars, orAlreadySatisfied, true);
								}))
							return true;
					}

					return false;
				}

				GAIA_NODISCARD bool eval_variable_terms_1var_orsource_on_archetype(
						const MatchingCtx& ctx, const Archetype& archetype, [[maybe_unused]] bool orAlreadySatisfied) const {
					using namespace detail;

					GAIA_ASSERT(m_compCtx.varUnboundStrategy == detail::EVarUnboundStrategy::OneVarOrSource);
					GAIA_ASSERT(m_compCtx.terms_any_var.empty());
					GAIA_ASSERT(!m_compCtx.terms_or_var.empty());
					GAIA_ASSERT(m_compCtx.varAnchorTermIdx < m_compCtx.terms_all_var.size());

					const auto varEntity = entity_from_id(*ctx.pWorld, (EntityId)(Var0.id() + m_compCtx.varIdx0));
					GAIA_ASSERT(varEntity != EntityBad);

					const auto varsBase = make_initial_var_bindings(ctx);
					if (var_is_bound(varsBase, varEntity))
						return match_variable_terms_1var_bound_state(
								ctx, archetype, varEntity, varsBase, orAlreadySatisfied, false);

					if (ctx.pEntityToArchetypeMap == nullptr)
						return eval_variable_terms_1var_on_archetype(ctx, archetype, orAlreadySatisfied);

					const auto& anchorTerm = m_compCtx.terms_all_var[m_compCtx.varAnchorTermIdx];
					GAIA_ASSERT(anchorTerm.term.src == varEntity);
					GAIA_ASSERT(anchorTerm.sourceOpcode == detail::EOpcode::Src_Self);

					const auto sourceArchetypes = fetch_archetypes_for_select(
							*ctx.pEntityToArchetypeMap, ctx.allArchetypes, anchorTerm.term.id, anchorTerm.term.id);
					for (const auto* pSourceArchetype: sourceArchetypes) {
						for (const auto* pChunk: pSourceArchetype->chunks()) {
							auto sourceEntities = pChunk->entity_view();
							const auto sourceCnt = (uint32_t)sourceEntities.size();
							GAIA_FOR(sourceCnt) {
								auto vars = varsBase;
								if (!bind_var(vars, varEntity, sourceEntities[i]))
									continue;

								if (match_variable_terms_1var_bound_state(
												ctx, archetype, varEntity, vars, orAlreadySatisfied, false, m_compCtx.varAnchorTermIdx))
									return true;
							}
						}
					}

					return false;
				}

				GAIA_NODISCARD bool eval_variable_terms_1var_pairall_on_archetype(
						const MatchingCtx& ctx, const Archetype& archetype, [[maybe_unused]] bool orAlreadySatisfied) const {
					using namespace detail;

					GAIA_ASSERT(m_compCtx.varUnboundStrategy == detail::EVarUnboundStrategy::OneVarPairAll);
					GAIA_ASSERT(m_compCtx.terms_or_var.empty());
					GAIA_ASSERT(m_compCtx.terms_not_var.empty());
					GAIA_ASSERT(m_compCtx.terms_any_var.empty());

					const auto varsBase = make_initial_var_bindings(ctx);
					return match_pairall_group_on_archetype(ctx, archetype, varsBase, m_compCtx.varIdx0);
				}

				GAIA_NODISCARD bool init_1var_pairmixed_anchor_on_archetype(
						const Archetype& archetype, bool orAlreadySatisfied, uint32_t& anchorRelId, bool& anchorFromOr) const {
					const auto archetypeIds = archetype.ids_view();
					const auto idsCnt = (uint32_t)archetypeIds.size();
					const auto& plan = m_compCtx.var1PairMixedPlan;

					anchorRelId = (uint32_t)-1;
					anchorFromOr = false;
					uint32_t anchorMatchCnt = (uint32_t)-1;

					auto count_rel_matches = [&](uint32_t relId) {
						uint32_t cnt = 0;
						for (uint32_t idIdx = 0; idIdx < idsCnt; ++idIdx) {
							const auto idInArchetype = archetypeIds[idIdx];
							if (!idInArchetype.pair())
								continue;
							if (idInArchetype.id() != relId)
								continue;
							++cnt;
						}
						return cnt;
					};

					for (const auto relId: plan.allRelIds) {
						const auto cnt = count_rel_matches(relId);
						if (cnt == 0)
							return false;

						if (anchorRelId == (uint32_t)-1 || cnt < anchorMatchCnt) {
							anchorRelId = relId;
							anchorFromOr = false;
							anchorMatchCnt = cnt;
						}
					}

					if (!orAlreadySatisfied) {
						bool anyOrCandidate = false;
						for (const auto relId: plan.orRelIds) {
							const auto cnt = count_rel_matches(relId);
							if (cnt == 0)
								continue;

							anyOrCandidate = true;
							if (anchorRelId == (uint32_t)-1 || cnt < anchorMatchCnt) {
								anchorRelId = relId;
								anchorFromOr = true;
								anchorMatchCnt = cnt;
							}
						}

						if (!anyOrCandidate)
							return false;
					}

					return anchorRelId != (uint32_t)-1;
				}

				GAIA_NODISCARD bool match_1var_pairmixed_candidate_on_archetype(
						const Archetype& archetype, Entity target, bool orAlreadySatisfied, bool orMatched) const {
					const auto& plan = m_compCtx.var1PairMixedPlan;
					const auto archetypeIds = archetype.ids_view();
					const auto idsCnt = (uint32_t)archetypeIds.size();

					const auto fullAllMask =
							plan.allRelIds.empty() ? uint16_t(0) : (uint16_t)((uint16_t(1) << plan.allRelIds.size()) - 1u);

					uint16_t allMask = 0;
					bool anyOrMatch = orMatched;
					for (uint32_t idIdx = 0; idIdx < idsCnt; ++idIdx) {
						const auto idInArchetype = archetypeIds[idIdx];
						if (!idInArchetype.pair())
							continue;
						if (idInArchetype.gen() != target.id())
							continue;

						const auto relId = idInArchetype.id();
						for (uint32_t i = 0; i < plan.notRelIds.size(); ++i) {
							if (plan.notRelIds[i] != relId)
								continue;
							return false;
						}

						for (uint32_t i = 0; i < plan.allRelIds.size(); ++i) {
							if (plan.allRelIds[i] != relId)
								continue;
							allMask |= (uint16_t(1) << i);
						}

						if (!orAlreadySatisfied && !anyOrMatch) {
							for (uint32_t i = 0; i < plan.orRelIds.size(); ++i) {
								if (plan.orRelIds[i] != relId)
									continue;
								anyOrMatch = true;
								break;
							}
						}
					}

					if (allMask != fullAllMask)
						return false;
					if (!orAlreadySatisfied && !anyOrMatch)
						return false;

					return true;
				}

				GAIA_NODISCARD bool eval_variable_terms_1var_pairmixed_on_archetype(
						const MatchingCtx& ctx, const Archetype& archetype, bool orAlreadySatisfied) const {
					using namespace detail;

					GAIA_ASSERT(m_compCtx.varUnboundStrategy == detail::EVarUnboundStrategy::OneVarPairMixed);
					GAIA_ASSERT(!m_compCtx.terms_all_var.empty());
					GAIA_ASSERT(!m_compCtx.terms_or_var.empty());
					GAIA_ASSERT(m_compCtx.terms_any_var.empty());

					const auto varEntity = entity_from_id(*ctx.pWorld, (EntityId)(Var0.id() + m_compCtx.varIdx0));
					GAIA_ASSERT(varEntity != EntityBad);

					const auto varsBase = make_initial_var_bindings(ctx);
					if (var_is_bound(varsBase, varEntity))
						return match_1var_pairmixed_candidate_on_archetype(
								archetype, varsBase.values[var_index(varEntity)], orAlreadySatisfied, false);

					uint32_t anchorRelId = (uint32_t)-1;
					bool anchorFromOr = false;
					if (!init_1var_pairmixed_anchor_on_archetype(archetype, orAlreadySatisfied, anchorRelId, anchorFromOr))
						return false;

					const auto archetypeIds = archetype.ids_view();
					const auto idsCnt = (uint32_t)archetypeIds.size();
					for (uint32_t idIdx = 0; idIdx < idsCnt; ++idIdx) {
						const auto idInArchetype = archetypeIds[idIdx];
						if (!idInArchetype.pair())
							continue;
						if (idInArchetype.id() != anchorRelId)
							continue;

						const auto target = entity_from_id(*ctx.pWorld, idInArchetype.gen());
						if (target == EntityBad)
							continue;

						if (match_1var_pairmixed_candidate_on_archetype(archetype, target, orAlreadySatisfied, anchorFromOr))
							return true;
					}

					return false;
				}

				GAIA_NODISCARD bool eval_variable_terms_2var_pairall_on_archetype(
						const MatchingCtx& ctx, const Archetype& archetype, [[maybe_unused]] bool orAlreadySatisfied) const {
					using namespace detail;

					GAIA_ASSERT(m_compCtx.varUnboundStrategy == detail::EVarUnboundStrategy::TwoVarPairAll);
					GAIA_ASSERT(m_compCtx.terms_or_var.empty());
					GAIA_ASSERT(m_compCtx.terms_not_var.empty());
					GAIA_ASSERT(m_compCtx.terms_any_var.empty());

					const auto varsBase = make_initial_var_bindings(ctx);
					if (!match_pairall_group_on_archetype(ctx, archetype, varsBase, m_compCtx.varIdx0))
						return false;

					return match_pairall_group_on_archetype(ctx, archetype, varsBase, m_compCtx.varIdx1);
				}

				GAIA_NODISCARD bool eval_variable_terms_allonly_grouped_on_archetype(
						const MatchingCtx& ctx, const Archetype& archetype, [[maybe_unused]] bool orAlreadySatisfied) const {
					using namespace detail;

					GAIA_ASSERT(m_compCtx.varUnboundStrategy == detail::EVarUnboundStrategy::AllOnlyGrouped);
					GAIA_ASSERT(!m_compCtx.terms_all_var.empty());
					GAIA_ASSERT(m_compCtx.terms_or_var.empty());
					GAIA_ASSERT(m_compCtx.terms_not_var.empty());
					GAIA_ASSERT(m_compCtx.terms_any_var.empty());
					GAIA_ASSERT(m_compCtx.varGroupMask != 0);

					const auto varsBase = make_initial_var_bindings(ctx);
					uint8_t groupMask = m_compCtx.varGroupMask;
					while (groupMask != 0) {
						const auto varIdx = (uint8_t)(GAIA_FFS(groupMask) - 1u);
						if (!match_allonly_group_on_archetype(ctx, archetype, varsBase, varIdx))
							return false;
						groupMask &= (uint8_t)(groupMask - 1u);
					}

					return true;
				}

				GAIA_NODISCARD bool eval_variable_terms_allonly_on_archetype(
						const MatchingCtx& ctx, const Archetype& archetype, [[maybe_unused]] bool orAlreadySatisfied) const {
					using namespace detail;

					GAIA_ASSERT(m_compCtx.varUnboundStrategy == detail::EVarUnboundStrategy::AllOnly);
					GAIA_ASSERT(!m_compCtx.terms_all_var.empty());
					GAIA_ASSERT(m_compCtx.terms_or_var.empty());
					GAIA_ASSERT(m_compCtx.terms_not_var.empty());
					GAIA_ASSERT(m_compCtx.terms_any_var.empty());

					constexpr uint32_t MatchProbeLimit = 64;
					auto solve = [&](auto&& self, uint16_t pendingAllMask, const VarBindings& vars) -> bool {
						if (pendingAllMask == 0)
							return true;

						const auto allCnt = (uint32_t)m_compCtx.terms_all_var.size();
						uint32_t bestIdx = (uint32_t)-1;
						uint32_t bestMatchCnt = MatchProbeLimit + 1;
						GAIA_FOR(allCnt) {
							const auto bit = (uint16_t(1) << i);
							if ((pendingAllMask & bit) == 0)
								continue;

							const auto& termOp = m_compCtx.terms_all_var[i];
							if (is_var_entity(termOp.term.src) && !var_is_bound(vars, termOp.term.src))
								continue;

							const uint32_t probeLimit = bestMatchCnt;
							const uint32_t matchCnt = count_term_matches_limited(*ctx.pWorld, archetype, termOp, vars, probeLimit);
							if (matchCnt == 0)
								return false;

							if (bestIdx == (uint32_t)-1 || matchCnt < bestMatchCnt) {
								bestIdx = i;
								bestMatchCnt = matchCnt;
								if (bestMatchCnt == 1)
									break;
							}
						}

						if (bestIdx == (uint32_t)-1)
							return false;

						const auto nextAllMask = (uint16_t)(pendingAllMask & ~(uint16_t(1) << bestIdx));
						const auto& bestTerm = m_compCtx.terms_all_var[bestIdx];
						return each_term_match(*ctx.pWorld, archetype, bestTerm, vars, [&](const VarBindings& state) {
							return self(self, nextAllMask, state);
						});
					};

					const auto pendingAllMask = (uint16_t)((uint16_t(1) << m_compCtx.terms_all_var.size()) - 1u);
					VarBindings vars = make_initial_var_bindings(ctx);
					return solve(solve, pendingAllMask, vars);
				}

				GAIA_NODISCARD bool eval_variable_terms_on_archetype(
						const MatchingCtx& ctx, const Archetype& archetype, bool orAlreadySatisfied) const {
					using namespace detail;

					const auto anyVarMask = m_compCtx.varMaskAny;

					auto can_skip_pending_all = [&](uint16_t pendingAllMask, const VarBindings& vars) -> bool {
						const auto allCnt = (uint32_t)m_compCtx.terms_all_var.size();
						GAIA_FOR(allCnt) {
							const auto bit = (uint16_t(1) << i);
							if ((pendingAllMask & bit) == 0)
								continue;

							const auto& termOp = m_compCtx.terms_all_var[i];
							const auto missingMask = (uint8_t)(termOp.varMask & ~vars.mask);
							if (missingMask == 0)
								return false;
							if ((missingMask & ~anyVarMask) != 0)
								return false;
						}

						return true;
					};

					auto finalize = [&](const VarBindings& vars, bool orMatched) -> bool {
						// NOT variable terms must not match.
						for (const auto& term: m_compCtx.terms_not_var) {
							if (term_has_match(*ctx.pWorld, archetype, term, vars))
								return false;
						}

						bool orSatisfied = orAlreadySatisfied || orMatched;
						if (!orSatisfied && !m_compCtx.terms_or_var.empty()) {
							for (const auto& term: m_compCtx.terms_or_var) {
								if (!term_has_match(*ctx.pWorld, archetype, term, vars))
									continue;
								orSatisfied = true;
								break;
							}
						}

						if (!orSatisfied && !m_compCtx.terms_or_var.empty())
							return false;

						return true;
					};

					auto solve = [&](auto&& self, uint16_t pendingAllMask, uint16_t pendingOrMask, uint16_t pendingAnyMask,
													 const VarBindings& vars, bool orMatched) -> bool {
						if (pendingAllMask == 0)
							return finalize(vars, orMatched);

						// Resolve the ready ALL term with the smallest match domain first (fail-fast).
						const auto allCnt = (uint32_t)m_compCtx.terms_all_var.size();
						uint32_t bestIdx = (uint32_t)-1;
						constexpr uint32_t MatchProbeLimit = 64;
						uint32_t bestMatchCnt = MatchProbeLimit + 1;
						GAIA_FOR(allCnt) {
							const auto bit = (uint16_t(1) << i);
							if ((pendingAllMask & bit) == 0)
								continue;

							const auto& termOp = m_compCtx.terms_all_var[i];
							if (is_var_entity(termOp.term.src) && !var_is_bound(vars, termOp.term.src))
								continue;

							const uint32_t probeLimit = bestMatchCnt;
							const uint32_t matchCnt = count_term_matches_limited(*ctx.pWorld, archetype, termOp, vars, probeLimit);
							if (matchCnt == 0)
								return false;

							if (bestIdx == (uint32_t)-1 || matchCnt < bestMatchCnt) {
								bestIdx = i;
								bestMatchCnt = matchCnt;

								// Can't do better than one branch.
								if (bestMatchCnt == 1)
									break;
							}
						}

						if (bestIdx != (uint32_t)-1) {
							const auto bit = (uint16_t(1) << bestIdx);
							const auto nextAllMask = (uint16_t)(pendingAllMask & ~bit);
							const auto& bestTerm = m_compCtx.terms_all_var[bestIdx];
							return each_term_match(*ctx.pWorld, archetype, bestTerm, vars, [&](const VarBindings& state) {
								return self(self, nextAllMask, pendingOrMask, pendingAnyMask, state, orMatched);
							});
						}

						// No ALL term was ready. Use OR terms to discover missing bindings.
						const auto orCnt = (uint32_t)m_compCtx.terms_or_var.size();
						uint32_t bestOrIdx = (uint32_t)-1;
						uint32_t bestOrMatchCnt = MatchProbeLimit + 1;
						GAIA_FOR(orCnt) {
							const auto bit = (uint16_t(1) << i);
							if ((pendingOrMask & bit) == 0)
								continue;

							const auto& termOp = m_compCtx.terms_or_var[i];
							if (is_var_entity(termOp.term.src) && !var_is_bound(vars, termOp.term.src))
								continue;

							const uint32_t probeLimit = bestOrMatchCnt;
							const uint32_t matchCnt = count_term_matches_limited(*ctx.pWorld, archetype, termOp, vars, probeLimit);
							if (matchCnt == 0)
								continue;

							if (bestOrIdx == (uint32_t)-1 || matchCnt < bestOrMatchCnt) {
								bestOrIdx = i;
								bestOrMatchCnt = matchCnt;

								// Can't do better than one branch.
								if (bestOrMatchCnt == 1)
									break;
							}
						}

						if (bestOrIdx != (uint32_t)-1) {
							const auto bit = (uint16_t(1) << bestOrIdx);
							const auto nextOrMask = (uint16_t)(pendingOrMask & ~bit);
							const auto& bestOrTerm = m_compCtx.terms_or_var[bestOrIdx];
							if (each_term_match(*ctx.pWorld, archetype, bestOrTerm, vars, [&](const VarBindings& state) {
										return self(self, pendingAllMask, nextOrMask, pendingAnyMask, state, true);
									}))
								return true;
						}

						GAIA_FOR(orCnt) {
							if (i == bestOrIdx)
								continue;

							const auto bit = (uint16_t(1) << i);
							if ((pendingOrMask & bit) == 0)
								continue;

							const auto& termOp = m_compCtx.terms_or_var[i];
							if (is_var_entity(termOp.term.src) && !var_is_bound(vars, termOp.term.src))
								continue;

							const auto nextOrMask = (uint16_t)(pendingOrMask & ~bit);
							if (each_term_match(*ctx.pWorld, archetype, termOp, vars, [&](const VarBindings& state) {
										return self(self, pendingAllMask, nextOrMask, pendingAnyMask, state, true);
									}))
								return true;
						}

						// ANY terms can bind variables when matched; if unmatched they are ignored.
						const auto anyCnt = (uint32_t)m_compCtx.terms_any_var.size();
						bool anyMatched = false;
						GAIA_FOR(anyCnt) {
							const auto bit = (uint16_t(1) << i);
							if ((pendingAnyMask & bit) == 0)
								continue;

							const auto& termOp = m_compCtx.terms_any_var[i];
							if (is_var_entity(termOp.term.src) && !var_is_bound(vars, termOp.term.src))
								continue;

							const auto nextAnyMask = (uint16_t)(pendingAnyMask & ~bit);
							bool matched = false;
							if (each_term_match(*ctx.pWorld, archetype, termOp, vars, [&](const VarBindings& state) {
										matched = true;
										anyMatched = true;
										return self(self, pendingAllMask, pendingOrMask, nextAnyMask, state, orMatched);
									}))
								return true;

							if (!matched) {
								if (self(self, pendingAllMask, pendingOrMask, nextAnyMask, vars, orMatched))
									return true;
							}
						}

						// No OR/ANY term was ready. Remaining ALL terms depend only on variables produced by ANY terms.
						// If those variables are still unresolved, we can skip those terms.
						if (!anyMatched && can_skip_pending_all(pendingAllMask, vars))
							return finalize(vars, orMatched);

						return false;
					};

					const auto pendingAllMask = (uint16_t)((uint16_t(1) << m_compCtx.terms_all_var.size()) - 1u);
					const auto pendingOrMask = (uint16_t)((uint16_t(1) << m_compCtx.terms_or_var.size()) - 1u);
					const auto pendingAnyMask = (uint16_t)((uint16_t(1) << m_compCtx.terms_any_var.size()) - 1u);
					VarBindings vars = make_initial_var_bindings(ctx);
					return solve(solve, pendingAllMask, pendingOrMask, pendingAnyMask, vars, false);
				}

				using VarEvalFunc = bool (VirtualMachine::*)(const MatchingCtx&, const Archetype&, bool) const;

				void filter_variable_terms(MatchingCtx& ctx, VarEvalFunc evalFunc) const {
					if (!m_compCtx.has_variable_terms())
						return;

					const bool orAlreadySatisfied = !m_compCtx.ids_or.empty() || ctx.skipOr;
					const auto sourceCnt = ctx.pMatchesArr->size();
					constexpr uint32_t FilterChunkSize = 64;
					cnt::sarray_ext<const Archetype*, FilterChunkSize> filtered;
					uint32_t writeIdx = 0;

					const auto flush_filtered = [&]() {
						for (const auto* pFiltered: filtered) {
							(*ctx.pMatchesArr)[writeIdx++] = pFiltered;
						}
						filtered.clear();
					};

					GAIA_FOR(sourceCnt) {
						const auto* pArchetype = (*ctx.pMatchesArr)[i];
						const bool matched = (this->*evalFunc)(ctx, *pArchetype, orAlreadySatisfied);
						if (!matched)
							continue;

						filtered.push_back(pArchetype);
						if (filtered.size() != FilterChunkSize)
							continue;

						flush_filtered();
					}

					if (!filtered.empty())
						flush_filtered();

					ctx.pMatchesArr->resize(writeIdx);
				}

				void filter_variable_terms(MatchingCtx& ctx) const {
					filter_variable_terms(ctx, &VirtualMachine::eval_variable_terms_on_archetype);
				}

				void filter_variable_terms_bound(MatchingCtx& ctx) const {
					filter_variable_terms(ctx, &VirtualMachine::eval_variable_terms_bound_on_archetype);
				}

				void filter_variable_terms_bound_1var_pairmixed(MatchingCtx& ctx) const {
					filter_variable_terms(ctx, &VirtualMachine::eval_variable_terms_bound_1var_pairmixed_on_archetype);
				}

				void filter_variable_terms_allonly(MatchingCtx& ctx) const {
					filter_variable_terms(ctx, &VirtualMachine::eval_variable_terms_allonly_on_archetype);
				}

				void filter_variable_terms_allonly_grouped(MatchingCtx& ctx) const {
					filter_variable_terms(ctx, &VirtualMachine::eval_variable_terms_allonly_grouped_on_archetype);
				}

				void filter_variable_terms_1var(MatchingCtx& ctx) const {
					filter_variable_terms(ctx, &VirtualMachine::eval_variable_terms_1var_on_archetype);
				}

				void filter_variable_terms_1var_orsource(MatchingCtx& ctx) const {
					filter_variable_terms(ctx, &VirtualMachine::eval_variable_terms_1var_orsource_on_archetype);
				}

				void filter_variable_terms_1var_pairall(MatchingCtx& ctx) const {
					filter_variable_terms(ctx, &VirtualMachine::eval_variable_terms_1var_pairall_on_archetype);
				}

				void filter_variable_terms_2var_pairall(MatchingCtx& ctx) const {
					filter_variable_terms(ctx, &VirtualMachine::eval_variable_terms_2var_pairall_on_archetype);
				}

				GAIA_NODISCARD detail::VmLabel add_op(detail::CompiledOp&& op) {
					const auto cnt = (detail::VmLabel)m_compCtx.ops.size();
					op.pc_ok = cnt + 1;
					op.pc_fail = cnt - 1;
					m_compCtx.ops.push_back(GAIA_MOV(op));
					return cnt;
				}

				GAIA_NODISCARD detail::VmLabel add_gate_op(detail::CompiledOp&& op) {
					const auto cnt = add_op(GAIA_MOV(op));
					m_compCtx.ops[cnt].pc_fail = (detail::VmLabel)-1;
					return cnt;
				}

				GAIA_NODISCARD static uint8_t opcode_arg(uint32_t idx) {
					GAIA_ASSERT(idx < OpcodeArgLimit);
					return (uint8_t)idx;
				}

				template <typename SourceTermsArray>
				void emit_source_gate_terms(const SourceTermsArray& terms, detail::EOpcode opcode) {
					const auto cnt = (uint32_t)terms.size();
					GAIA_FOR(cnt) {
						detail::CompiledOp op{};
						op.opcode = opcode;
						op.arg = opcode_arg(i);
						(void)add_gate_op(GAIA_MOV(op));
					}
				}

				void emit_source_or_terms(bool hasOrFallback) {
					cnt::sarray_ext<detail::VmLabel, MAX_ITEMS_IN_QUERY> orSrcOpLabels;

					const auto srcOrCnt = (uint32_t)m_compCtx.terms_or_src.size();
					GAIA_FOR(srcOrCnt) {
						detail::CompiledOp op{};
						op.opcode = detail::EOpcode::Src_OrTerm;
						op.arg = opcode_arg(i);
						orSrcOpLabels.push_back(add_op(GAIA_MOV(op)));
					}

					const auto orExitPc = (detail::VmLabel)m_compCtx.ops.size();
					for (const auto opLabel: orSrcOpLabels)
						m_compCtx.ops[opLabel].pc_ok = orExitPc;

					const auto lastIdx = (uint32_t)orSrcOpLabels.size() - 1u;
					GAIA_FOR(lastIdx) {
						m_compCtx.ops[orSrcOpLabels[i]].pc_fail = orSrcOpLabels[i + 1];
					}

					m_compCtx.ops[orSrcOpLabels[lastIdx]].pc_fail = hasOrFallback ? orExitPc : (detail::VmLabel)-1;
				}

				GAIA_NODISCARD static detail::EOpcode pick_all_opcode(bool isSimple, bool isAs) {
					if (isSimple)
						return detail::EOpcode::All_Simple;
					if (isAs)
						return detail::EOpcode::All_Complex;
					return detail::EOpcode::All_Wildcard;
				}

				GAIA_NODISCARD static detail::EOpcode pick_or_opcode(bool hasAllTerms, bool isSimple, bool isAs) {
					if (!hasAllTerms) {
						if (isSimple)
							return detail::EOpcode::Or_NoAll_Simple;
						if (isAs)
							return detail::EOpcode::Or_NoAll_Complex;
						return detail::EOpcode::Or_NoAll_Wildcard;
					}

					if (isSimple)
						return detail::EOpcode::Or_WithAll_Simple;
					if (isAs)
						return detail::EOpcode::Or_WithAll_Complex;
					return detail::EOpcode::Or_WithAll_Wildcard;
				}

				GAIA_NODISCARD static detail::EOpcode pick_not_opcode(bool isSimple, bool isAs) {
					if (isSimple)
						return detail::EOpcode::Not_Simple;
					if (isAs)
						return detail::EOpcode::Not_Complex;
					return detail::EOpcode::Not_Wildcard;
				}

				using OpcodeFunc = bool (VirtualMachine::*)(MatchingCtx&) const;

				GAIA_NODISCARD bool op_all_simple(MatchingCtx& ctx) const {
					GAIA_PROF_SCOPE(vm::op_and_simple);
					return detail::exec_all_impl<MatchingStyle::Simple>(m_compCtx, ctx);
				}

				GAIA_NODISCARD bool op_all_wildcard(MatchingCtx& ctx) const {
					GAIA_PROF_SCOPE(vm::op_and_wildcard);
					return detail::exec_all_impl<MatchingStyle::Wildcard>(m_compCtx, ctx);
				}

				GAIA_NODISCARD bool op_all_complex(MatchingCtx& ctx) const {
					GAIA_PROF_SCOPE(vm::op_and_complex);
					return detail::exec_all_impl<MatchingStyle::Complex>(m_compCtx, ctx);
				}

				GAIA_NODISCARD bool op_or_noall_simple(MatchingCtx& ctx) const {
					GAIA_PROF_SCOPE(vm::op_or);
					return detail::exec_or_noall_impl<MatchingStyle::Simple>(m_compCtx, ctx);
				}

				GAIA_NODISCARD bool op_or_noall_wildcard(MatchingCtx& ctx) const {
					GAIA_PROF_SCOPE(vm::op_or);
					return detail::exec_or_noall_impl<MatchingStyle::Wildcard>(m_compCtx, ctx);
				}

				GAIA_NODISCARD bool op_or_noall_complex(MatchingCtx& ctx) const {
					GAIA_PROF_SCOPE(vm::op_or);
					return detail::exec_or_noall_impl<MatchingStyle::Complex>(m_compCtx, ctx);
				}

				GAIA_NODISCARD bool op_or_withall_simple(MatchingCtx& ctx) const {
					GAIA_PROF_SCOPE(vm::op_or);
					return detail::exec_or_withall_impl<MatchingStyle::Simple>(m_compCtx, ctx);
				}

				GAIA_NODISCARD bool op_or_withall_wildcard(MatchingCtx& ctx) const {
					GAIA_PROF_SCOPE(vm::op_or);
					return detail::exec_or_withall_impl<MatchingStyle::Wildcard>(m_compCtx, ctx);
				}

				GAIA_NODISCARD bool op_or_withall_complex(MatchingCtx& ctx) const {
					GAIA_PROF_SCOPE(vm::op_or);
					return detail::exec_or_withall_impl<MatchingStyle::Complex>(m_compCtx, ctx);
				}

				GAIA_NODISCARD bool op_not_simple(MatchingCtx& ctx) const {
					GAIA_PROF_SCOPE(vm::op_not);
					return detail::exec_not_impl<MatchingStyle::Simple>(m_compCtx, ctx);
				}

				GAIA_NODISCARD bool op_not_wildcard(MatchingCtx& ctx) const {
					GAIA_PROF_SCOPE(vm::op_not);
					return detail::exec_not_impl<MatchingStyle::Wildcard>(m_compCtx, ctx);
				}

				GAIA_NODISCARD bool op_not_complex(MatchingCtx& ctx) const {
					GAIA_PROF_SCOPE(vm::op_not);
					return detail::exec_not_impl<MatchingStyle::Complex>(m_compCtx, ctx);
				}

				GAIA_NODISCARD bool op_seed_all(MatchingCtx& ctx) const {
					GAIA_PROF_SCOPE(vm::op_seed_all);
					detail::add_all_archetypes(ctx);
					return true;
				}

				GAIA_NODISCARD bool op_var_check_bound(MatchingCtx& ctx) const {
					GAIA_PROF_SCOPE(vm::op_var_check_bound);
					return all_variable_terms_bound(ctx);
				}

				GAIA_NODISCARD bool op_var_filter(MatchingCtx& ctx) const {
					GAIA_PROF_SCOPE(vm::op_var_filter);
					filter_variable_terms(ctx);
					return true;
				}

				GAIA_NODISCARD bool op_var_filter_bound(MatchingCtx& ctx) const {
					GAIA_PROF_SCOPE(vm::op_var_filter_bound);
					filter_variable_terms_bound(ctx);
					return true;
				}

				GAIA_NODISCARD bool op_var_filter_bound_1var_pairmixed(MatchingCtx& ctx) const {
					GAIA_PROF_SCOPE(vm::op_var_filter_bound_1var_pairmixed);
					filter_variable_terms_bound_1var_pairmixed(ctx);
					return true;
				}

				GAIA_NODISCARD bool op_var_filter_allonly(MatchingCtx& ctx) const {
					GAIA_PROF_SCOPE(vm::op_var_filter_allonly);
					filter_variable_terms_allonly(ctx);
					return true;
				}

				GAIA_NODISCARD bool op_var_filter_allonly_grouped(MatchingCtx& ctx) const {
					GAIA_PROF_SCOPE(vm::op_var_filter_allonly_grouped);
					filter_variable_terms_allonly_grouped(ctx);
					return true;
				}

				GAIA_NODISCARD bool op_var_filter_1var(MatchingCtx& ctx) const {
					GAIA_PROF_SCOPE(vm::op_var_filter_1var);
					filter_variable_terms_1var(ctx);
					return true;
				}

				GAIA_NODISCARD bool op_var_filter_1var_orsource(MatchingCtx& ctx) const {
					GAIA_PROF_SCOPE(vm::op_var_filter_1var_orsource);
					filter_variable_terms_1var_orsource(ctx);
					return true;
				}

				GAIA_NODISCARD bool op_var_filter_1var_pairmixed(MatchingCtx& ctx) const {
					GAIA_PROF_SCOPE(vm::op_var_filter_1var_pairmixed);
					filter_variable_terms(ctx, &VirtualMachine::eval_variable_terms_1var_pairmixed_on_archetype);
					return true;
				}

				GAIA_NODISCARD bool op_var_filter_1var_pairall(MatchingCtx& ctx) const {
					GAIA_PROF_SCOPE(vm::op_var_filter_1var_pairall);
					filter_variable_terms_1var_pairall(ctx);
					return true;
				}

				GAIA_NODISCARD bool op_var_filter_2var_pairall(MatchingCtx& ctx) const {
					GAIA_PROF_SCOPE(vm::op_var_filter_2var_pairall);
					filter_variable_terms_2var_pairall(ctx);
					return true;
				}

				GAIA_NODISCARD bool op_src_all_term(MatchingCtx& ctx) const {
					GAIA_PROF_SCOPE(vm::op_src_all);
					const auto& termOp = detail::get_source_term_op(m_compCtx, ctx, m_compCtx.terms_all_src);
					return detail::match_source_term(*ctx.pWorld, termOp.term, termOp.opcode);
				}

				GAIA_NODISCARD bool op_src_not_term(MatchingCtx& ctx) const {
					GAIA_PROF_SCOPE(vm::op_src_not);
					const auto& termOp = detail::get_source_term_op(m_compCtx, ctx, m_compCtx.terms_not_src);
					return !detail::match_source_term(*ctx.pWorld, termOp.term, termOp.opcode);
				}

				GAIA_NODISCARD bool op_src_or_term(MatchingCtx& ctx) const {
					GAIA_PROF_SCOPE(vm::op_src_or);
					const auto& termOp = detail::get_source_term_op(m_compCtx, ctx, m_compCtx.terms_or_src);
					const bool matched = detail::match_source_term(*ctx.pWorld, termOp.term, termOp.opcode);
					if (!matched)
						return false;

					ctx.skipOr = true;
					if (m_compCtx.ids_all.empty())
						detail::add_all_archetypes(ctx);
					return true;
				}

				static constexpr OpcodeFunc OpcodeFuncs[] = {
						&VirtualMachine::op_all_simple, //
						&VirtualMachine::op_all_wildcard, //
						&VirtualMachine::op_all_complex, //
						&VirtualMachine::op_or_noall_simple, //
						&VirtualMachine::op_or_noall_wildcard, //
						&VirtualMachine::op_or_noall_complex, //
						&VirtualMachine::op_or_withall_simple, //
						&VirtualMachine::op_or_withall_wildcard, //
						&VirtualMachine::op_or_withall_complex, //
						&VirtualMachine::op_not_simple, //
						&VirtualMachine::op_not_wildcard, //
						&VirtualMachine::op_not_complex, //
						&VirtualMachine::op_seed_all, //
						&VirtualMachine::op_var_check_bound, //
						&VirtualMachine::op_var_filter, //
						&VirtualMachine::op_var_filter_bound, //
						&VirtualMachine::op_var_filter_bound_1var_pairmixed, //
						&VirtualMachine::op_var_filter_allonly, //
						&VirtualMachine::op_var_filter_allonly_grouped, //
						&VirtualMachine::op_var_filter_1var, //
						&VirtualMachine::op_var_filter_1var_orsource, //
						&VirtualMachine::op_var_filter_1var_pairmixed, //
						&VirtualMachine::op_var_filter_1var_pairall, //
						&VirtualMachine::op_var_filter_2var_pairall, //
						&VirtualMachine::op_src_all_term, //
						&VirtualMachine::op_src_not_term, //
						&VirtualMachine::op_src_or_term //
				};
				static_assert(
						sizeof(OpcodeFuncs) / sizeof(OpcodeFuncs[0]) == (uint32_t)detail::EOpcode::Src_Never,
						"OpcodeFuncs must contain all executable opcodes.");

				GAIA_NODISCARD bool exec_opcode(const detail::CompiledOp& stackItem, MatchingCtx& ctx) const {
					const auto opcodeIdx = (uint32_t)stackItem.opcode;
					GAIA_ASSERT(opcodeIdx < (uint32_t)detail::EOpcode::Src_Never);
					return (this->*OpcodeFuncs[opcodeIdx])(ctx);
				}

			public:
				GAIA_NODISCARD util::str bytecode(const World& world) const {
					util::str out;
					out.reserve(2048);

					out.append("main_ops: ");
					append_uint(out, (uint32_t)m_compCtx.ops.size());
					out.append('\n');

					const auto opsCnt = (uint32_t)m_compCtx.ops.size();
					GAIA_FOR(opsCnt) {
						const auto& op = m_compCtx.ops[i];
						out.append("  [");
						append_uint(out, i);
						out.append("] ");
						append_cstr(out, opcode_name(op.opcode));
						if (opcode_has_arg(op.opcode)) {
							out.append(" arg=");
							append_uint(out, op.arg);
						}
						out.append(" ok=");
						append_uint(out, op.pc_ok);
						out.append(" fail=");
						append_uint(out, op.pc_fail);
						out.append('\n');
					}

					append_ids_section(
							out, "ids_all", std::span<const Entity>{m_compCtx.ids_all.data(), m_compCtx.ids_all.size()}, world);
					append_ids_section(
							out, "ids_or", std::span<const Entity>{m_compCtx.ids_or.data(), m_compCtx.ids_or.size()}, world);
					append_ids_section(
							out, "ids_not", std::span<const Entity>{m_compCtx.ids_not.data(), m_compCtx.ids_not.size()}, world);

					append_source_terms_section(out, "src_all", m_compCtx.terms_all_src, world);
					append_source_terms_section(out, "src_or", m_compCtx.terms_or_src, world);
					append_source_terms_section(out, "src_not", m_compCtx.terms_not_src, world);

					append_var_terms_section(out, "var_all", m_compCtx.terms_all_var, world);
					append_var_terms_section(out, "var_or", m_compCtx.terms_or_var, world);
					append_var_terms_section(out, "var_not", m_compCtx.terms_not_var, world);
					append_var_terms_section(out, "var_any", m_compCtx.terms_any_var, world);

					return out;
				}

				//! Transforms inputs into virtual machine opcodes.
				void compile(
						const EntityToArchetypeMap& entityToArchetypeMap, std::span<const Archetype*> allArchetypes,
						QueryCtx& queryCtx) {
					GAIA_PROF_SCOPE(vm::compile);
					(void)entityToArchetypeMap;
					(void)allArchetypes;

					m_compCtx.ids_all.clear();
					m_compCtx.ids_or.clear();
					m_compCtx.ids_not.clear();
					m_compCtx.terms_all_src.clear();
					m_compCtx.terms_or_src.clear();
					m_compCtx.terms_not_src.clear();
					m_compCtx.terms_all_var.clear();
					m_compCtx.terms_or_var.clear();
					m_compCtx.terms_not_var.clear();
					m_compCtx.terms_any_var.clear();
					m_compCtx.varMaskAll = 0;
					m_compCtx.varMaskOr = 0;
					m_compCtx.varMaskNot = 0;
					m_compCtx.varMaskAny = 0;
					m_compCtx.varUnboundStrategy = detail::EVarUnboundStrategy::Generic;
					m_compCtx.varIdx0 = 0;
					m_compCtx.varIdx1 = 0;
					m_compCtx.varAnchorTermIdx = (uint8_t)-1;
					m_compCtx.var1Plan.clear();
					m_compCtx.var1PairMixedPlan.clear();
					m_compCtx.varGroupMask = 0;
					m_compCtx.varGroupSourceMask = 0;
					GAIA_FOR(8) {
						m_compCtx.varGroupAnchorTermIdx[i] = (uint8_t)-1;
						m_compCtx.varGroupTerms[i].clear();
						m_compCtx.varGroupPairTerms[i].clear();
					}
					m_compCtx.ops.clear();

					auto& data = queryCtx.data;
					GAIA_ASSERT(queryCtx.w != nullptr);
					const auto& world = *queryCtx.w;

					QueryTermSpan terms = data.terms_view_mut();
					QueryTermSpan terms_all = terms.subspan(0, data.firstOr);
					QueryTermSpan terms_or = terms.subspan(data.firstOr, data.firstNot - data.firstOr);
					QueryTermSpan terms_not = terms.subspan(data.firstNot, data.firstAny - data.firstNot);
					QueryTermSpan terms_any = terms.subspan(data.firstAny);

					// ALL
					if (!terms_all.empty()) {
						GAIA_PROF_SCOPE(vm::compile_all);

						const auto cnt = terms_all.size();
						GAIA_FOR(cnt) {
							auto& p = terms_all[i];
							if (term_has_variables(p)) {
								const auto varMask = term_unbound_var_mask(world, p, detail::VarBindings{});
								m_compCtx.terms_all_var.push_back({detail::source_opcode_from_term(p), p, varMask});
								m_compCtx.varMaskAll |= varMask;
								continue;
							}

							if (p.src == EntityBad) {
								m_compCtx.ids_all.push_back(p.id);
								continue;
							}
							m_compCtx.terms_all_src.push_back({detail::source_opcode_from_term(p), p});
						}
					}

					// OR
					if (!terms_or.empty()) {
						GAIA_PROF_SCOPE(vm::compile_or);

						const auto cnt = terms_or.size();
						GAIA_FOR(cnt) {
							auto& p = terms_or[i];
							if (term_has_variables(p)) {
								const auto varMask = term_unbound_var_mask(world, p, detail::VarBindings{});
								m_compCtx.terms_or_var.push_back({detail::source_opcode_from_term(p), p, varMask});
								m_compCtx.varMaskOr |= varMask;
								continue;
							}

							if (p.src == EntityBad)
								m_compCtx.ids_or.push_back(p.id);
							else
								m_compCtx.terms_or_src.push_back({detail::source_opcode_from_term(p), p});
						}
					}

					// NOT
					if (!terms_not.empty()) {
						GAIA_PROF_SCOPE(vm::compile_not);

						const auto cnt = terms_not.size();
						GAIA_FOR(cnt) {
							auto& p = terms_not[i];
							if (term_has_variables(p)) {
								const auto varMask = term_unbound_var_mask(world, p, detail::VarBindings{});
								m_compCtx.terms_not_var.push_back({detail::source_opcode_from_term(p), p, varMask});
								m_compCtx.varMaskNot |= varMask;
								continue;
							}

							if (p.src == EntityBad)
								m_compCtx.ids_not.push_back(p.id);
							else
								m_compCtx.terms_not_src.push_back({detail::source_opcode_from_term(p), p});
						}
					}

					// ANY
					if (!terms_any.empty()) {
						GAIA_PROF_SCOPE(vm::compile_any);

						const auto cnt = terms_any.size();
						GAIA_FOR(cnt) {
							auto& p = terms_any[i];
							if (!term_has_variables(p))
								continue;
							const auto varMask = term_unbound_var_mask(world, p, detail::VarBindings{});
							m_compCtx.terms_any_var.push_back({detail::source_opcode_from_term(p), p, varMask});
							m_compCtx.varMaskAny |= varMask;
						}
					}

					detail::sort_source_terms_by_cost(m_compCtx.terms_all_src);
					detail::sort_source_terms_by_cost(m_compCtx.terms_or_src);
					detail::sort_source_terms_by_cost(m_compCtx.terms_not_src);

					const auto allVarCnt = (uint32_t)m_compCtx.terms_all_var.size();
					GAIA_FOR(allVarCnt) {
						const auto& termOp = m_compCtx.terms_all_var[i];
						if (termOp.varMask == 0 || GAIA_POPCNT(termOp.varMask) != 1)
							continue;

						const auto varIdx = (uint8_t)(GAIA_FFS(termOp.varMask) - 1u);
						const auto varEntity = entity_from_id(world, (EntityId)(Var0.id() + varIdx));
						GAIA_ASSERT(varEntity != EntityBad);
						m_compCtx.varGroupTerms[varIdx].push_back((uint8_t)i);

						if (detail::is_var_entity(termOp.term.src) && termOp.term.src == varEntity &&
								termOp.sourceOpcode == detail::EOpcode::Src_Self && detail::has_concrete_match_id(termOp.term.id) &&
								m_compCtx.varGroupAnchorTermIdx[varIdx] == (uint8_t)-1)
							m_compCtx.varGroupAnchorTermIdx[varIdx] = (uint8_t)i;

						if (termOp.term.src == EntityBad && termOp.term.id.pair() && termOp.term.id.gen() == varEntity.id() &&
								!is_variable(termOp.term.id.id()) && termOp.term.id.id() != All.id())
							m_compCtx.varGroupPairTerms[varIdx].push_back((uint8_t)i);
					}

					const auto varMask =
							(uint8_t)(m_compCtx.varMaskAll | m_compCtx.varMaskOr | m_compCtx.varMaskNot | m_compCtx.varMaskAny);
					const bool allOnlyVars = !m_compCtx.terms_all_var.empty() && m_compCtx.terms_or_var.empty() &&
																	 m_compCtx.terms_not_var.empty() && m_compCtx.terms_any_var.empty();
					const bool oneVarEligible = varMask != 0 && GAIA_POPCNT(varMask) == 1 && m_compCtx.terms_any_var.empty() &&
																			(!m_compCtx.terms_all_var.empty() || !m_compCtx.terms_or_var.empty());

					auto try_init_pairall_groups = [&](uint32_t expectedVarCnt) -> bool {
						uint8_t groupVarIdx0 = (uint8_t)-1;
						uint8_t groupVarIdx1 = (uint8_t)-1;
						uint32_t groupCnt0 = 0;
						uint32_t groupCnt1 = 0;

						const auto allVarCnt = (uint32_t)m_compCtx.terms_all_var.size();
						GAIA_FOR(allVarCnt) {
							const auto& term = m_compCtx.terms_all_var[i].term;
							if (term.src != EntityBad)
								return false;
							if (!term.id.pair())
								return false;
							if (is_variable(term.id.id()) || term.id.id() == All.id())
								return false;
							if (!is_variable(term.id.gen()))
								return false;

							const auto varIdx = (uint8_t)(term.id.gen() - Var0.id());
							if (groupVarIdx0 == (uint8_t)-1 || varIdx == groupVarIdx0) {
								groupVarIdx0 = varIdx;
								++groupCnt0;
								continue;
							}

							if (expectedVarCnt == 1)
								return false;

							if (groupVarIdx1 == (uint8_t)-1 || varIdx == groupVarIdx1) {
								groupVarIdx1 = varIdx;
								++groupCnt1;
								continue;
							}

							return false;
						}

						if (groupVarIdx0 == (uint8_t)-1 || groupCnt0 == 0)
							return false;
						if (expectedVarCnt == 2 && (groupVarIdx1 == (uint8_t)-1 || groupCnt1 == 0))
							return false;
						if (expectedVarCnt == 1 && groupVarIdx1 != (uint8_t)-1)
							return false;

						m_compCtx.varIdx0 = groupVarIdx0;
						m_compCtx.varIdx1 = groupVarIdx1 == (uint8_t)-1 ? 0 : groupVarIdx1;
						return true;
					};

					auto try_init_1var_or_source = [&](uint8_t varIdx) -> bool {
						if (m_compCtx.terms_or_var.empty())
							return false;

						const auto varEntity = entity_from_id(world, (EntityId)(Var0.id() + varIdx));
						GAIA_ASSERT(varEntity != EntityBad);

						const auto allVarCnt = (uint32_t)m_compCtx.terms_all_var.size();
						GAIA_FOR(allVarCnt) {
							const auto& termOp = m_compCtx.terms_all_var[i];
							if (termOp.term.src != varEntity)
								continue;
							if (termOp.sourceOpcode != detail::EOpcode::Src_Self)
								continue;
							if (termOp.varMask != (uint8_t(1) << varIdx))
								continue;
							if (!detail::has_concrete_match_id(termOp.term.id))
								continue;

							m_compCtx.varIdx0 = varIdx;
							m_compCtx.varAnchorTermIdx = (uint8_t)i;
							return true;
						}

						return false;
					};

					auto try_init_1var_pairmixed = [&](uint8_t varIdx) -> bool {
						if (m_compCtx.terms_all_var.empty() || m_compCtx.terms_or_var.empty())
							return false;

						const auto varEntity = entity_from_id(world, (EntityId)(Var0.id() + varIdx));
						GAIA_ASSERT(varEntity != EntityBad);
						const auto varBit = (uint8_t)(uint8_t(1) << varIdx);

						auto validate_terms = [&](const auto& terms) {
							for (const auto& termOp: terms) {
								if (termOp.varMask != 0 && termOp.varMask != varBit)
									return false;
								if (termOp.term.src != EntityBad)
									return false;
								if (!termOp.term.id.pair())
									return false;
								if (termOp.term.id.gen() != varEntity.id())
									return false;
								if (is_variable((EntityId)termOp.term.id.id()) || termOp.term.id.id() == All.id())
									return false;
							}
							return true;
						};

						if (!validate_terms(m_compCtx.terms_all_var) || !validate_terms(m_compCtx.terms_or_var) ||
								!validate_terms(m_compCtx.terms_not_var))
							return false;

						m_compCtx.var1PairMixedPlan.clear();
						for (const auto& termOp: m_compCtx.terms_all_var)
							m_compCtx.var1PairMixedPlan.allRelIds.push_back(termOp.term.id.id());
						for (const auto& termOp: m_compCtx.terms_or_var)
							m_compCtx.var1PairMixedPlan.orRelIds.push_back(termOp.term.id.id());
						for (const auto& termOp: m_compCtx.terms_not_var)
							m_compCtx.var1PairMixedPlan.notRelIds.push_back(termOp.term.id.id());

						m_compCtx.varIdx0 = varIdx;
						return true;
					};

					auto init_1var_plan = [&](uint8_t varIdx) {
						m_compCtx.var1Plan.clear();

						const auto varEntity = entity_from_id(world, (EntityId)(Var0.id() + varIdx));
						GAIA_ASSERT(varEntity != EntityBad);

						const auto allVarCnt = (uint32_t)m_compCtx.terms_all_var.size();
						GAIA_FOR(allVarCnt) {
							const auto& termOp = m_compCtx.terms_all_var[i];
							GAIA_ASSERT(termOp.varMask == 0 || termOp.varMask == (uint8_t(1) << varIdx));

							if (termOp.term.src != varEntity)
								m_compCtx.var1Plan.allAnchorTerms.push_back((uint8_t)i);

							m_compCtx.var1Plan.requiredChecks.push_back({(uint8_t)i, detail::bound_term_cost(termOp), false});
						}

						const auto orVarCnt = (uint32_t)m_compCtx.terms_or_var.size();
						GAIA_FOR(orVarCnt) {
							const auto& termOp = m_compCtx.terms_or_var[i];
							GAIA_ASSERT(termOp.varMask == 0 || termOp.varMask == (uint8_t(1) << varIdx));

							if (termOp.term.src != varEntity)
								m_compCtx.var1Plan.orAnchorTerms.push_back((uint8_t)i);

							m_compCtx.var1Plan.orChecks.push_back({(uint8_t)i, detail::bound_term_cost(termOp)});
						}

						const auto notVarCnt = (uint32_t)m_compCtx.terms_not_var.size();
						GAIA_FOR(notVarCnt) {
							const auto& termOp = m_compCtx.terms_not_var[i];
							GAIA_ASSERT(termOp.varMask == 0 || termOp.varMask == (uint8_t(1) << varIdx));

							m_compCtx.var1Plan.requiredChecks.push_back({(uint8_t)i, detail::bound_term_cost(termOp), true});
						}

						detail::sort_single_var_checks_by_cost(m_compCtx.var1Plan.requiredChecks);
						detail::sort_single_var_term_refs_by_cost(m_compCtx.var1Plan.orChecks);
					};

					auto try_init_allonly_grouped = [&]() -> bool {
						// Group 0
						// .all(Pair(relConnected, Var0))
						// .all(Pair(relMirror, Var0))
						// .all<SourceType0>(src(Var0))
						// Group 1
						// .all(Pair(relPowered, Var1))
						// .all<SourceType1>(src(Var1))
						//
						// These groups are independent. There is no term tying Var0 to Var1.
						// We do not need full backtracking across both variables.
						// We only need:
						// 1) “can I find some Var0 that satisfies all Var0-terms?”
						// 2) “can I find some Var1 that satisfies all Var1-terms?”

						uint8_t groupMask = 0;
						uint8_t sourceMask = 0;

						GAIA_FOR(allVarCnt) {
							const auto& termOp = m_compCtx.terms_all_var[i];
							if (termOp.varMask == 0 || GAIA_POPCNT(termOp.varMask) != 1)
								return false;

							const auto varIdx = (uint8_t)(GAIA_FFS(termOp.varMask) - 1u);
							const auto varEntity = entity_from_id(world, (EntityId)(Var0.id() + varIdx));
							GAIA_ASSERT(varEntity != EntityBad);
							const auto bit = (uint8_t)(uint8_t(1) << varIdx);

							if (detail::is_var_entity(termOp.term.src)) {
								if (termOp.term.src != varEntity)
									return false;
								if (termOp.sourceOpcode != detail::EOpcode::Src_Self)
									return false;
								if (!detail::has_concrete_match_id(termOp.term.id))
									return false;

								GAIA_ASSERT(m_compCtx.varGroupAnchorTermIdx[varIdx] != (uint8_t)-1);
								sourceMask |= bit;
								groupMask |= bit;
								continue;
							}

							if (termOp.term.src != EntityBad)
								return false;
							if (!termOp.term.id.pair())
								return false;
							if (termOp.term.id.gen() != varEntity.id())
								return false;
							if (is_variable(termOp.term.id.id()) || termOp.term.id.id() == All.id())
								return false;

							groupMask |= bit;
						}

						if (groupMask != m_compCtx.varMaskAll)
							return false;
						if (sourceMask == 0)
							return false;

						m_compCtx.varGroupMask = groupMask;
						m_compCtx.varGroupSourceMask = sourceMask;
						return true;
					};

					if (allOnlyVars)
						m_compCtx.varUnboundStrategy = detail::EVarUnboundStrategy::AllOnly;

					if (oneVarEligible) {
						m_compCtx.varIdx0 = (uint8_t)(GAIA_FFS(varMask) - 1u);
						m_compCtx.varUnboundStrategy = detail::EVarUnboundStrategy::OneVar;
						if (try_init_1var_or_source(m_compCtx.varIdx0))
							m_compCtx.varUnboundStrategy = detail::EVarUnboundStrategy::OneVarOrSource;
						else if (try_init_1var_pairmixed(m_compCtx.varIdx0))
							m_compCtx.varUnboundStrategy = detail::EVarUnboundStrategy::OneVarPairMixed;
						init_1var_plan(m_compCtx.varIdx0);
					}

					if (allOnlyVars && varMask != 0) {
						const auto varCnt = GAIA_POPCNT(varMask);
						if (varCnt == 1 && try_init_pairall_groups(1))
							m_compCtx.varUnboundStrategy = detail::EVarUnboundStrategy::OneVarPairAll;
						else if (varCnt == 2 && try_init_pairall_groups(2))
							m_compCtx.varUnboundStrategy = detail::EVarUnboundStrategy::TwoVarPairAll;
					}

					if (allOnlyVars && try_init_allonly_grouped())
						m_compCtx.varUnboundStrategy = detail::EVarUnboundStrategy::AllOnlyGrouped;

					create_opcodes(queryCtx);
				}

				void create_opcodes(QueryCtx& queryCtx) {
					const bool isSimple = (queryCtx.data.flags & QueryCtx::QueryFlags::Complex) == 0U;
					const bool isAs = (queryCtx.data.as_mask_0 + queryCtx.data.as_mask_1) != 0U;

					// Reset the list of opcodes
					m_compCtx.ops.clear();

					// Source ALL terms: all must match, each is a dedicated gate opcode.
					if (!m_compCtx.terms_all_src.empty())
						emit_source_gate_terms(m_compCtx.terms_all_src, detail::EOpcode::Src_AllTerm);

					// Source NOT terms: none can match, each is a dedicated gate opcode.
					if (!m_compCtx.terms_not_src.empty())
						emit_source_gate_terms(m_compCtx.terms_not_src, detail::EOpcode::Src_NotTerm);

					// Source OR terms: emit a fallback chain that backtracks across alternatives.
					if (!m_compCtx.terms_or_src.empty()) {
						const bool hasOrFallback = !m_compCtx.ids_or.empty() || !m_compCtx.terms_or_var.empty();
						emit_source_or_terms(hasOrFallback);
					}

					// Queries without direct id terms seed from all archetypes via explicit bytecode.
					if (!m_compCtx.has_id_terms() && (m_compCtx.has_source_terms() || m_compCtx.has_variable_terms())) {
						detail::CompiledOp op{};
						op.opcode = detail::EOpcode::Seed_All;
						(void)add_op(GAIA_MOV(op));
					}

					// ALL
					if (!m_compCtx.ids_all.empty()) {
						detail::CompiledOp op{};
						op.opcode = pick_all_opcode(isSimple, isAs);
						(void)add_gate_op(GAIA_MOV(op));
					}

					// OR
					if (!m_compCtx.ids_or.empty()) {
						detail::CompiledOp op{};
						op.opcode = pick_or_opcode(!m_compCtx.ids_all.empty(), isSimple, isAs);
						(void)add_op(GAIA_MOV(op));
					}

					// NOT
					if (!m_compCtx.ids_not.empty()) {
						detail::CompiledOp op{};
						op.opcode = pick_not_opcode(isSimple, isAs);
						(void)add_op(GAIA_MOV(op));
					}

					// Variable term evaluation is part of the VM stream.
					if (m_compCtx.has_variable_terms()) {
						detail::CompiledOp opCheck{};
						opCheck.opcode = detail::EOpcode::Var_CheckBound;
						const auto opCheckLabel = add_op(GAIA_MOV(opCheck));

						detail::CompiledOp opBound{};
						if (m_compCtx.varUnboundStrategy == detail::EVarUnboundStrategy::OneVarPairMixed)
							opBound.opcode = detail::EOpcode::Var_Filter_Bound_1VarPairMixed;
						else
							opBound.opcode = detail::EOpcode::Var_Filter_Bound;
						const auto opBoundLabel = add_gate_op(GAIA_MOV(opBound));

						detail::CompiledOp opUnbound{};
						if (m_compCtx.varUnboundStrategy == detail::EVarUnboundStrategy::OneVarPairAll)
							opUnbound.opcode = detail::EOpcode::Var_Filter_1VarPairAll;
						else if (m_compCtx.varUnboundStrategy == detail::EVarUnboundStrategy::TwoVarPairAll)
							opUnbound.opcode = detail::EOpcode::Var_Filter_2VarPairAll;
						else if (m_compCtx.varUnboundStrategy == detail::EVarUnboundStrategy::AllOnlyGrouped)
							opUnbound.opcode = detail::EOpcode::Var_Filter_AllOnlyGrouped;
						else if (m_compCtx.varUnboundStrategy == detail::EVarUnboundStrategy::OneVarOrSource)
							opUnbound.opcode = detail::EOpcode::Var_Filter_1VarOrSource;
						else if (m_compCtx.varUnboundStrategy == detail::EVarUnboundStrategy::OneVarPairMixed)
							opUnbound.opcode = detail::EOpcode::Var_Filter_1VarPairMixed;
						else if (m_compCtx.varUnboundStrategy == detail::EVarUnboundStrategy::OneVar)
							opUnbound.opcode = detail::EOpcode::Var_Filter_1Var;
						else if (m_compCtx.varUnboundStrategy == detail::EVarUnboundStrategy::AllOnly)
							opUnbound.opcode = detail::EOpcode::Var_Filter_AllOnly;
						else
							opUnbound.opcode = detail::EOpcode::Var_Filter;
						const auto opUnboundLabel = add_gate_op(GAIA_MOV(opUnbound));

						m_compCtx.ops[opCheckLabel].pc_ok = opBoundLabel;
						m_compCtx.ops[opCheckLabel].pc_fail = opUnboundLabel;

						const auto opNext = (detail::VmLabel)m_compCtx.ops.size();
						m_compCtx.ops[opBoundLabel].pc_ok = opNext;
						m_compCtx.ops[opUnboundLabel].pc_ok = opNext;
					}

					// Mark as compiled
					queryCtx.data.flags &= ~QueryCtx::QueryFlags::Recompile;
				}

				GAIA_NODISCARD bool is_compiled() const {
					return !m_compCtx.ops.empty();
				}

				//! Executes compiled opcodes
				void exec(MatchingCtx& ctx) {
					GAIA_PROF_SCOPE(vm::exec);
					ctx.skipOr = false;
					if (m_compCtx.ops.empty())
						return;

					ctx.pc = 0;

					// Extract data from the buffer
					do {
						auto& stackItem = m_compCtx.ops[ctx.pc];
						GAIA_ASSERT((uint32_t)stackItem.opcode < (uint32_t)detail::EOpcode::Src_Never);
						const bool ret = exec_opcode(stackItem, ctx);
						ctx.pc = ret ? stackItem.pc_ok : stackItem.pc_fail;
					} while (ctx.pc < m_compCtx.ops.size()); // (uint32_t)-1 falls in this category as well
				}
			};

		} // namespace vm
	} // namespace ecs

} // namespace gaia
