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
					//! Filter current result set using variable terms
					Var_Filter,
					//! Source term gates
					Src_AllTerm,
					Src_NotTerm,
					Src_OrTerm,
					//! Source traversal sub-opcodes (used by source terms)
					Src_Never,
					Src_Self,
					Src_Up,
					Src_Down,
					Src_UpDown,
					//! Variable search micro-ops
					Var_Term_All,
					Var_Term_Or_Check,
					Var_Term_Or_Bind,
					Var_Term_Any_Check,
					Var_Term_Any_Bind,
					Var_Term_Not,
					Var_Search_Enter,
					Var_Search_SelectOtherOr,
					Var_Search_SelectAny,
					Var_Search_Finalize,
					Var_Final_Not_Check,
					Var_Final_Require_Or,
					Var_Final_Or_Check,
					Var_Final_Success
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
					//! Optional planner-side cost used for sorting compiled micro-op plans.
					uint8_t cost = 0;
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
					struct VarProgram {
						uint16_t begin = 0;
						uint16_t count = 0;

						void clear() {
							begin = 0;
							count = 0;
						}

						GAIA_NODISCARD bool empty() const {
							return count == 0;
						}
					};
					struct VarSearchMeta {
						uint16_t enterPc = (uint16_t)-1;
						uint16_t selectOtherOrPc = (uint16_t)-1;
						uint16_t selectAnyPc = (uint16_t)-1;
						uint16_t finalizePc = (uint16_t)-1;
						uint16_t initialAllMask = 0;
						uint16_t initialOrMask = 0;
						uint16_t initialAnyMask = 0;
						uint16_t allBegin = 0;
						uint16_t allCount = 0;
						uint16_t orBegin = 0;
						uint16_t orCheckBegin = 0;
						uint16_t orCount = 0;
						uint16_t anyBegin = 0;
						uint16_t anyCheckBegin = 0;
						uint16_t anyCount = 0;
						uint16_t notBegin = 0;
						uint16_t notCount = 0;
					};
					struct VarProgramStep {
						VarProgram program{};
						VarSearchMeta search{};
					};

					cnt::darray<CompiledOp> ops;
					uint16_t mainOpsCount = 0;
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
					cnt::sarray_ext<VarProgramStep, 8> varPrograms;

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

				enum class EVarProgramTermSet : uint8_t { None, All, Or, Any, Not };

				struct VarProgramOpcodeMeta {
					EVarProgramTermSet termSet;
				};

				static constexpr auto VarProgramOpcodeFirst = EOpcode::Var_Term_All;
				static constexpr auto VarProgramOpcodeLast = EOpcode::Var_Final_Success;
				static constexpr VarProgramOpcodeMeta VarProgramOpcodeMetaTable[] = {
						{EVarProgramTermSet::All}, //
						{EVarProgramTermSet::Or}, //
						{EVarProgramTermSet::Or}, //
						{EVarProgramTermSet::Any}, //
						{EVarProgramTermSet::Any}, //
						{EVarProgramTermSet::Not}, //
						{EVarProgramTermSet::None}, //
						{EVarProgramTermSet::None}, //
						{EVarProgramTermSet::None}, //
						{EVarProgramTermSet::None}, //
						{EVarProgramTermSet::Not}, //
						{EVarProgramTermSet::None}, //
						{EVarProgramTermSet::Or}, //
						{EVarProgramTermSet::None}, //
				};

				static_assert(
						sizeof(VarProgramOpcodeMetaTable) / sizeof(VarProgramOpcodeMetaTable[0]) ==
								(uint32_t)VarProgramOpcodeLast - (uint32_t)VarProgramOpcodeFirst + 1u,
						"VarProgramOpcodeMetaTable out of sync with EOpcode variable micro-op range.");

				GAIA_NODISCARD inline const VarProgramOpcodeMeta& var_program_opcode_meta(EOpcode opcode) {
					GAIA_ASSERT((uint32_t)opcode >= (uint32_t)VarProgramOpcodeFirst);
					GAIA_ASSERT((uint32_t)opcode <= (uint32_t)VarProgramOpcodeLast);
					return VarProgramOpcodeMetaTable[(uint32_t)opcode - (uint32_t)VarProgramOpcodeFirst];
				}

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

				GAIA_NODISCARD inline uint8_t search_term_cost(const QueryCompileCtx::VarTermOp& termOp) {
					uint8_t cost = bound_term_cost(termOp);
					if (termOp.term.src != EntityBad) {
						const bool srcIsVar = is_variable(EntityId(termOp.term.src.id()));
						cost = (uint8_t)(cost + (srcIsVar ? 32u : 8u));
					}
					return cost;
				}

				template <typename ProgramOpsArray>
				inline void sort_program_ops_by_cost(ProgramOpsArray& ops) {
					const auto cnt = (uint32_t)ops.size();
					if (cnt < 2)
						return;

					for (uint32_t i = 1; i < cnt; ++i) {
						const auto key = ops[i];

						uint32_t j = i;
						while (j > 0) {
							const auto prev = ops[j - 1];
							if (prev.cost < key.cost)
								break;
							if (prev.cost == key.cost && (uint8_t)prev.opcode < (uint8_t)key.opcode)
								break;
							if (prev.cost == key.cost && prev.opcode == key.opcode && prev.arg <= key.arg)
								break;
							ops[j] = prev;
							--j;
						}

						ops[j] = key;
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

				GAIA_NODISCARD inline std::span<const CompiledOp>
				program_ops(const QueryCompileCtx& comp, const QueryCompileCtx::VarProgram& program) {
					return {comp.ops.data() + program.begin, program.count};
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

				struct SourceLookupCursor {
					uint32_t queueIdx = 0;
					uint32_t childIdx = 0;
					uint32_t childLevel = 0;
					uint32_t upDepth = 0;
					uint8_t phase = 0;
					bool initialized = false;
					bool selfEmitted = false;
					Entity upSource = EntityBad;
					cnt::darray<Entity> queue;
					cnt::darray<uint32_t> levels;
					cnt::darray<Entity> children;
					cnt::set<EntityLookupKey> visited;

					void reset_runtime_state() {
						queueIdx = 0;
						childIdx = 0;
						childLevel = 0;
						upDepth = 0;
						initialized = false;
						selfEmitted = false;
						upSource = EntityBad;
						queue.clear();
						levels.clear();
						children.clear();
						visited.clear();
					}
				};

				GAIA_NODISCARD inline bool next_lookup_source_cursor(
						const World& w, EOpcode opcode, const QueryTerm& term, Entity sourceEntity, SourceLookupCursor& cursor,
						Entity& outSource);

				template <typename Func>
				GAIA_NODISCARD inline bool
				each_lookup_source(const World& w, EOpcode opcode, const QueryTerm& term, Entity sourceEntity, Func&& func) {
					SourceLookupCursor cursor{};
					Entity source = EntityBad;
					while (next_lookup_source_cursor(w, opcode, term, sourceEntity, cursor, source)) {
						if (func(source))
							return true;
					}

					return false;
				}

				template <typename Func>
				GAIA_NODISCARD inline bool
				each_lookup_source(const World& w, const QueryTerm& term, Entity sourceEntity, Func&& func) {
					return each_lookup_source(w, source_opcode_from_term(term), term, sourceEntity, GAIA_FWD(func));
				}

				GAIA_NODISCARD inline bool next_lookup_source_cursor_up(
						const World& w, const QueryTerm& term, Entity sourceEntity, SourceLookupCursor& cursor, Entity& outSource,
						bool includeSelf) {
					if (!valid(w, sourceEntity))
						return false;

					constexpr uint32_t MaxTraversalDepth = 2048;
					const uint32_t maxDepth =
							term.travDepth == QueryTermOptions::TravDepthUnlimited ? MaxTraversalDepth : (uint32_t)term.travDepth;

					if (!cursor.initialized) {
						cursor.initialized = true;
						cursor.upSource = sourceEntity;
					}

					if (includeSelf && !cursor.selfEmitted) {
						cursor.selfEmitted = true;
						outSource = sourceEntity;
						return true;
					}

					while (cursor.upDepth < maxDepth) {
						const auto next = target(w, cursor.upSource, term.entTrav);
						if (next == EntityBad || next == cursor.upSource)
							return false;

						cursor.upSource = next;
						++cursor.upDepth;
						outSource = next;
						return true;
					}

					return false;
				}

				GAIA_NODISCARD inline bool next_lookup_source_cursor_down(
						const World& w, const QueryTerm& term, Entity sourceEntity, SourceLookupCursor& cursor, Entity& outSource,
						bool includeSelf) {
					if (!valid(w, sourceEntity))
						return false;

					constexpr uint32_t MaxTraversalDepth = 2048;
					const uint32_t maxDepth =
							term.travDepth == QueryTermOptions::TravDepthUnlimited ? MaxTraversalDepth : (uint32_t)term.travDepth;

					if (!cursor.initialized) {
						cursor.initialized = true;
						cursor.queue.push_back(sourceEntity);
						cursor.levels.push_back(0);
						cursor.visited.insert(EntityLookupKey(sourceEntity));
					}

					if (includeSelf && !cursor.selfEmitted) {
						cursor.selfEmitted = true;
						outSource = sourceEntity;
						return true;
					}

					for (;;) {
						if (cursor.childIdx < cursor.children.size()) {
							const auto child = cursor.children[cursor.childIdx++];
							cursor.queue.push_back(child);
							cursor.levels.push_back(cursor.childLevel);
							outSource = child;
							return true;
						}

						bool loadedChildren = false;
						while (cursor.queueIdx < cursor.queue.size()) {
							const auto source = cursor.queue[cursor.queueIdx];
							const auto level = cursor.levels[cursor.queueIdx];
							++cursor.queueIdx;
							if (level >= maxDepth)
								continue;

							cursor.children.clear();
							cursor.childIdx = 0;
							cursor.childLevel = level + 1;
							sources(w, term.entTrav, source, [&](Entity next) {
								const auto key = EntityLookupKey(next);
								const auto ins = cursor.visited.insert(key);
								if (!ins.second)
									return;

								cursor.children.push_back(next);
							});

							core::sort(cursor.children, [](Entity left, Entity right) {
								return left.id() < right.id();
							});

							if (!cursor.children.empty()) {
								loadedChildren = true;
								break;
							}
						}

						if (!loadedChildren)
							return false;
					}
				}

				GAIA_NODISCARD inline bool next_lookup_source_cursor(
						const World& w, EOpcode opcode, const QueryTerm& term, Entity sourceEntity, SourceLookupCursor& cursor,
						Entity& outSource) {
					const bool includeSelf = query_trav_has(term.travKind, QueryTravKind::Self);
					switch (opcode) {
						case EOpcode::Src_Never:
							return false;
						case EOpcode::Src_Self:
							if (cursor.initialized || !valid(w, sourceEntity))
								return false;
							cursor.initialized = true;
							outSource = sourceEntity;
							return true;
						case EOpcode::Src_Up:
							return next_lookup_source_cursor_up(w, term, sourceEntity, cursor, outSource, includeSelf);
						case EOpcode::Src_Down:
							return next_lookup_source_cursor_down(w, term, sourceEntity, cursor, outSource, includeSelf);
						case EOpcode::Src_UpDown:
							if (cursor.phase == 0) {
								if (next_lookup_source_cursor_up(w, term, sourceEntity, cursor, outSource, includeSelf))
									return true;
								cursor.reset_runtime_state();
								cursor.phase = 1;
							}
							return next_lookup_source_cursor_down(w, term, sourceEntity, cursor, outSource, false);
						default:
							GAIA_ASSERT(false);
							return false;
					}
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

				struct VarTermMatchCursor {
					uint32_t idIdx = 0;
					Entity source = EntityBad;
					SourceLookupCursor sourceCursor{};
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

				struct ResolvedPairToken {
					Entity token = EntityBad;
					Entity matchValue = EntityBad;
					bool concrete = false;
					bool needsBind = false;
				};

				GAIA_NODISCARD inline ResolvedPairToken resolve_pair_query_token(Entity queryToken, const VarBindings& vars) {
					ResolvedPairToken out{};
					out.token = queryToken;

					if (queryToken == EntityBad)
						return out;

					if (is_var_entity(queryToken)) {
						if (!var_is_bound(vars, queryToken)) {
							out.needsBind = true;
							return out;
						}

						out.matchValue = vars.values[var_index(queryToken)];
						out.concrete = out.matchValue.id() != All.id();
						return out;
					}

					if (queryToken.id() == All.id())
						return out;

					out.matchValue = queryToken;
					out.concrete = true;
					return out;
				}

				template <typename Func>
				GAIA_NODISCARD inline bool each_term_match(
						const World& w, const Archetype& candidateArchetype, const QueryCompileCtx::VarTermOp& termOp,
						const VarBindings& varsIn, Func&& func);

				GAIA_NODISCARD inline bool next_id_match_cursor(
						const World& w, const Archetype& archetype, Entity queryId, const VarBindings& varsIn, uint32_t& idIdx,
						VarBindings& outVars) {
					auto archetypeIds = archetype.ids_view();
					const auto cnt = (uint32_t)archetypeIds.size();

					if (!queryId.pair()) {
						for (uint32_t i = idIdx; i < cnt; ++i) {
							const auto idInArchetype = archetypeIds[i];
							if (idInArchetype.pair())
								continue;

							const auto value = entity_from_id(w, idInArchetype.id());
							if (value == EntityBad)
								continue;

							auto vars = varsIn;
							if (!match_token(vars, queryId, value, false))
								continue;

							outVars = vars;
							idIdx = i + 1;
							return true;
						}

						return false;
					}

					const auto queryRel = entity_from_id(w, queryId.id());
					const auto queryTgt = entity_from_id(w, queryId.gen());
					if (queryRel == EntityBad || queryTgt == EntityBad)
						return false;
					const auto rel = resolve_pair_query_token(queryRel, varsIn);
					const auto tgt = resolve_pair_query_token(queryTgt, varsIn);

					for (uint32_t i = idIdx; i < cnt; ++i) {
						const auto idInArchetype = archetypeIds[i];
						if (!idInArchetype.pair())
							continue;

						if (rel.concrete && idInArchetype.id() != rel.matchValue.id())
							continue;
						if (tgt.concrete && idInArchetype.gen() != tgt.matchValue.id())
							continue;

						auto vars = varsIn;
						if (rel.needsBind) {
							const auto relValue = entity_from_id(w, idInArchetype.id());
							if (relValue == EntityBad)
								continue;
							if (!match_token(vars, rel.token, relValue, true))
								continue;
						}
						if (tgt.needsBind) {
							const auto tgtValue = entity_from_id(w, idInArchetype.gen());
							if (tgtValue == EntityBad)
								continue;
							if (!match_token(vars, tgt.token, tgtValue, true))
								continue;
						}

						outVars = vars;
						idIdx = i + 1;
						return true;
					}

					return false;
				}

				GAIA_NODISCARD inline bool next_term_match_cursor(
						const World& w, const Archetype& archetype, const QueryCompileCtx::VarTermOp& termOp,
						const VarBindings& varsIn, VarTermMatchCursor& cursor, VarBindings& outVars) {
					const auto& term = termOp.term;
					if (term.src == EntityBad)
						return next_id_match_cursor(w, archetype, term.id, varsIn, cursor.idIdx, outVars);

					auto sourceEntity = term.src;
					if (is_var_entity(sourceEntity)) {
						if (!var_is_bound(varsIn, sourceEntity))
							return false;
						sourceEntity = varsIn.values[var_index(sourceEntity)];
					}

					for (;;) {
						if (cursor.source != EntityBad) {
							auto* pSrcArchetype = archetype_from_entity(w, cursor.source);
							if (pSrcArchetype != nullptr &&
									next_id_match_cursor(w, *pSrcArchetype, term.id, varsIn, cursor.idIdx, outVars))
								return true;

							cursor.idIdx = 0;
							cursor.source = EntityBad;
						}

						Entity nextSource = EntityBad;
						if (!next_lookup_source_cursor(w, termOp.sourceOpcode, term, sourceEntity, cursor.sourceCursor, nextSource))
							return false;

						cursor.source = nextSource;
					}
				}

				GAIA_NODISCARD inline bool term_has_match_bound(
						const World& w, const Archetype& candidateArchetype, const QueryCompileCtx::VarTermOp& termOp,
						const VarBindings& vars);

				GAIA_NODISCARD inline bool term_has_match(
						const World& w, const Archetype& archetype, const QueryCompileCtx::VarTermOp& termOp,
						const VarBindings& varsIn) {
					if ((uint8_t)(termOp.varMask & ~varsIn.mask) == 0)
						return term_has_match_bound(w, archetype, termOp, varsIn);

					return each_term_match(w, archetype, termOp, varsIn, [&](const VarBindings&) {
						return true;
					});
				}

				GAIA_NODISCARD inline uint32_t count_term_matches_limited(
						const World& w, const Archetype& archetype, const QueryCompileCtx::VarTermOp& termOp,
						const VarBindings& varsIn, uint32_t limit) {
					GAIA_ASSERT(limit > 0);

					if ((uint8_t)(termOp.varMask & ~varsIn.mask) == 0)
						return term_has_match_bound(w, archetype, termOp, varsIn) ? 1u : 0u;

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
					const auto rel = resolve_pair_query_token(queryRel, varsIn);
					const auto tgt = resolve_pair_query_token(queryTgt, varsIn);

					GAIA_FOR(cnt) {
						const auto idInArchetype = archetypeIds[i];
						if (!idInArchetype.pair())
							continue;

						if (rel.concrete && idInArchetype.id() != rel.matchValue.id())
							continue;
						if (tgt.concrete && idInArchetype.gen() != tgt.matchValue.id())
							continue;

						if (!rel.needsBind && !tgt.needsBind) {
							if (func(varsIn))
								return true;
							continue;
						}

						auto vars = varsIn;
						if (rel.needsBind) {
							const auto relValue = entity_from_id(w, idInArchetype.id());
							if (relValue == EntityBad)
								continue;
							if (!match_token(vars, rel.token, relValue, true))
								continue;
						}
						if (tgt.needsBind) {
							const auto tgtValue = entity_from_id(w, idInArchetype.gen());
							if (tgtValue == EntityBad)
								continue;
							if (!match_token(vars, tgt.token, tgtValue, true))
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
							"varf", //
							"src_all_t", //
							"src_not_t", //
							"src_or_t", //
							"nev", //
							"self", //
							"up", //
							"down", //
							"updown", //
							"term_all", //
							"term_or_check", //
							"term_or_bind", //
							"term_any_check", //
							"term_any_bind", //
							"term_not", //
							"search_enter", //
							"search_other_or", //
							"search_any", //
							"search_finalize", //
							"final_not_check", //
							"final_require_or", //
							"final_or_check", //
							"final_success", //
					};
					static_assert(
							sizeof(s_names) / sizeof(s_names[0]) == (uint32_t)detail::EOpcode::Var_Final_Success + 1u,
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

				GAIA_NODISCARD static const QueryTerm&
				var_program_op_term(const detail::QueryCompileCtx& comp, const detail::CompiledOp& op) {
					switch (detail::var_program_opcode_meta(op.opcode).termSet) {
						case detail::EVarProgramTermSet::None:
							GAIA_ASSERT(false);
							return comp.terms_all_var[0].term;
						case detail::EVarProgramTermSet::Or:
							return comp.terms_or_var[(uint32_t)op.arg].term;
						case detail::EVarProgramTermSet::Any:
							return comp.terms_any_var[(uint32_t)op.arg].term;
						case detail::EVarProgramTermSet::Not:
							return comp.terms_not_var[(uint32_t)op.arg].term;
						case detail::EVarProgramTermSet::All:
						default:
							return comp.terms_all_var[(uint32_t)op.arg].term;
					}
				}

				static void append_var_program_ops_section(
						util::str& out, const char* title, std::span<const detail::CompiledOp> ops,
						const detail::QueryCompileCtx& comp, const World& world) {
					if (ops.empty())
						return;

					append_cstr(out, title);
					out.append(": ");
					append_uint(out, (uint32_t)ops.size());
					out.append('\n');

					const auto cnt = (uint32_t)ops.size();
					GAIA_FOR(cnt) {
						const auto& op = ops[i];
						out.append("  [");
						append_uint(out, i);
						out.append("] ");
						append_cstr(out, opcode_name(op.opcode));
						if (detail::var_program_opcode_meta(op.opcode).termSet != detail::EVarProgramTermSet::None) {
							out.append(" term=");
							append_uint(out, (uint32_t)op.arg);
							out.append(" cost=");
							append_uint(out, (uint32_t)op.cost);
							out.append(" id=");
							append_term_expr(out, world, var_program_op_term(comp, op));
						}
						out.append(" ok=");
						append_uint(out, (uint32_t)op.pc_ok);
						out.append(" fail=");
						append_uint(out, (uint32_t)op.pc_fail);
						out.append('\n');
					}
				}

				static void append_var_program_exec_section(util::str& out, const detail::QueryCompileCtx& comp) {
					if (comp.varPrograms.empty())
						return;

					out.append("var_exec: ");
					append_uint(out, (uint32_t)comp.varPrograms.size());
					out.append('\n');

					const auto cnt = (uint32_t)comp.varPrograms.size();
					GAIA_FOR(cnt) {
						out.append("  [");
						append_uint(out, i);
						out.append("] search");
						out.append('\n');
					}
				}

				static void
				append_var_program_sections(util::str& out, const detail::QueryCompileCtx& comp, const World& world) {
					const auto cnt = (uint32_t)comp.varPrograms.size();
					GAIA_FOR(cnt) {
						const auto& step = comp.varPrograms[i];
						char title[32];
						[[maybe_unused]] const auto len = GAIA_STRFMT(title, sizeof(title), "varp%u", i);
						GAIA_ASSERT(len > 0);
						append_var_program_ops_section(out, title, detail::program_ops(comp, step.program), comp, world);
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

				GAIA_NODISCARD bool eval_variable_terms_program_on_archetype(
						const MatchingCtx& ctx, const Archetype& archetype, bool orAlreadySatisfied) const {
					GAIA_ASSERT(m_compCtx.varPrograms.size() == 1);
					const auto& programStep = m_compCtx.varPrograms[0];
					return match_search_program_on_archetype(ctx, archetype, programStep, orAlreadySatisfied);
				}

				GAIA_NODISCARD const detail::QueryCompileCtx::VarTermOp&
				search_program_term_op(const detail::CompiledOp& op) const {
					switch (op.opcode) {
						case detail::EOpcode::Var_Term_Or_Check:
						case detail::EOpcode::Var_Term_Or_Bind:
						case detail::EOpcode::Var_Final_Or_Check:
							return m_compCtx.terms_or_var[(uint32_t)op.arg];
						case detail::EOpcode::Var_Term_Any_Check:
						case detail::EOpcode::Var_Term_Any_Bind:
							return m_compCtx.terms_any_var[(uint32_t)op.arg];
						case detail::EOpcode::Var_Term_Not:
						case detail::EOpcode::Var_Final_Not_Check:
							return m_compCtx.terms_not_var[(uint32_t)op.arg];
						case detail::EOpcode::Var_Term_All:
							return m_compCtx.terms_all_var[(uint32_t)op.arg];
						default:
							GAIA_ASSERT(false);
							return m_compCtx.terms_all_var[0];
					}
				}

				GAIA_NODISCARD uint32_t select_next_pending_search_term(
						std::span<const detail::CompiledOp> programOps, uint16_t begin, uint16_t count, uint16_t pendingMask,
						const detail::VarBindings& vars, bool preferBoundTerms = true, bool requireNewBindings = false) const {
					uint32_t firstReadyIdx = (uint32_t)-1;

					for (uint32_t localIdx = 0; localIdx < count; ++localIdx) {
						const auto i = (uint32_t)begin + localIdx;
						const auto bit = (uint16_t)(uint16_t(1) << i);
						if ((pendingMask & bit) == 0)
							continue;

						const auto& termOp = search_program_term_op(programOps[i]);
						if (detail::is_var_entity(termOp.term.src) && !detail::var_is_bound(vars, termOp.term.src))
							continue;

						const bool bindsNewVars = (uint8_t)(termOp.varMask & ~vars.mask) != 0;
						if (requireNewBindings) {
							if (bindsNewVars)
								return i;
							continue;
						}

						if (preferBoundTerms && !bindsNewVars)
							return i;

						if (firstReadyIdx == (uint32_t)-1)
							firstReadyIdx = i;
					}

					return firstReadyIdx;
				}

				GAIA_NODISCARD bool select_next_pending_search_or_term(
						std::span<const detail::CompiledOp> programOps, const detail::QueryCompileCtx::VarSearchMeta& search,
						uint16_t pendingMask, uint16_t pendingCheckMask, const detail::VarBindings& vars, bool preferBoundTerms,
						bool requireNewBindings, uint32_t& outLocalIdx, uint32_t& outPc) const {
					outLocalIdx = (uint32_t)-1;
					outPc = (uint32_t)-1;
					uint32_t firstReadyLocalIdx = (uint32_t)-1;
					uint32_t firstReadyPc = (uint32_t)-1;

					for (uint32_t localIdx = 0; localIdx < search.orCount; ++localIdx) {
						const auto bit = (uint16_t)(uint16_t(1) << localIdx);
						if ((pendingMask & bit) == 0)
							continue;

						const auto bindPc = (uint32_t)search.orBegin + localIdx;
						const auto& bindOp = programOps[bindPc];
						const auto& termOp = search_program_term_op(bindOp);
						if (detail::is_var_entity(termOp.term.src) && !detail::var_is_bound(vars, termOp.term.src))
							continue;

						const bool bindsNewVars = (uint8_t)(termOp.varMask & ~vars.mask) != 0;
						if (requireNewBindings) {
							if (!bindsNewVars)
								continue;
							outLocalIdx = localIdx;
							outPc = bindPc;
							return true;
						}

						if (!bindsNewVars && (pendingCheckMask & bit) == 0)
							continue;

						const auto pc = bindsNewVars ? bindPc : (uint32_t)search.orCheckBegin + localIdx;
						if (preferBoundTerms && !bindsNewVars) {
							outLocalIdx = localIdx;
							outPc = pc;
							return true;
						}

						if (firstReadyLocalIdx == (uint32_t)-1) {
							firstReadyLocalIdx = localIdx;
							firstReadyPc = pc;
						}
					}

					if (firstReadyLocalIdx == (uint32_t)-1)
						return false;

					outLocalIdx = firstReadyLocalIdx;
					outPc = firstReadyPc;
					return true;
				}

				GAIA_NODISCARD bool select_next_pending_search_any_term(
						std::span<const detail::CompiledOp> programOps, const detail::QueryCompileCtx::VarSearchMeta& search,
						uint16_t pendingMask, const detail::VarBindings& vars, uint32_t& outLocalIdx, uint32_t& outPc) const {
					outLocalIdx = (uint32_t)-1;
					outPc = (uint32_t)-1;
					uint32_t firstReadyBindingLocalIdx = (uint32_t)-1;
					uint32_t firstReadyBindingPc = (uint32_t)-1;

					for (uint32_t localIdx = 0; localIdx < search.anyCount; ++localIdx) {
						const auto bit = (uint16_t)(uint16_t(1) << localIdx);
						if ((pendingMask & bit) == 0)
							continue;

						const auto bindPc = (uint32_t)search.anyBegin + localIdx;
						const auto& bindOp = programOps[bindPc];
						const auto& termOp = search_program_term_op(bindOp);
						if (detail::is_var_entity(termOp.term.src) && !detail::var_is_bound(vars, termOp.term.src))
							continue;

						const bool bindsNewVars = (uint8_t)(termOp.varMask & ~vars.mask) != 0;
						if (!bindsNewVars) {
							outLocalIdx = localIdx;
							outPc = (uint32_t)search.anyCheckBegin + localIdx;
							return true;
						}

						if (firstReadyBindingLocalIdx == (uint32_t)-1) {
							firstReadyBindingLocalIdx = localIdx;
							firstReadyBindingPc = bindPc;
						}
					}

					if (firstReadyBindingLocalIdx == (uint32_t)-1)
						return false;

					outLocalIdx = firstReadyBindingLocalIdx;
					outPc = firstReadyBindingPc;
					return true;
				}

				GAIA_NODISCARD int32_t select_best_pending_search_term(
						const MatchingCtx& ctx, const Archetype& archetype, std::span<const detail::CompiledOp> programOps,
						uint16_t begin, uint16_t count, uint16_t pendingMask, const detail::VarBindings& vars,
						uint32_t& outBestIdx) const {
					constexpr uint32_t MatchProbeLimit = 64;
					outBestIdx = (uint32_t)-1;
					uint32_t bestMatchCnt = MatchProbeLimit + 1;

					for (uint32_t localIdx = 0; localIdx < count; ++localIdx) {
						const auto i = (uint32_t)begin + localIdx;
						const auto bit = (uint16_t)(uint16_t(1) << i);
						if ((pendingMask & bit) == 0)
							continue;

						const auto& termOp = search_program_term_op(programOps[i]);
						if (detail::is_var_entity(termOp.term.src) && !detail::var_is_bound(vars, termOp.term.src))
							continue;

						const auto matchCnt =
								detail::count_term_matches_limited(*ctx.pWorld, archetype, termOp, vars, bestMatchCnt);
						if (matchCnt == 0)
							return -1;

						if (outBestIdx == (uint32_t)-1 || matchCnt < bestMatchCnt) {
							outBestIdx = i;
							bestMatchCnt = matchCnt;
							if (bestMatchCnt == 1)
								break;
						}
					}

					return outBestIdx == (uint32_t)-1 ? 0 : 1;
				}

				GAIA_NODISCARD bool can_skip_pending_search_all(
						std::span<const detail::CompiledOp> programOps, const detail::QueryCompileCtx::VarSearchMeta& search,
						uint16_t pendingAllMask, const detail::VarBindings& vars) const {
					const auto anyVarMask = m_compCtx.varMaskAny;
					for (uint32_t localIdx = 0; localIdx < search.allCount; ++localIdx) {
						const auto i = (uint32_t)search.allBegin + localIdx;
						const auto bit = (uint16_t(1) << i);
						if ((pendingAllMask & bit) == 0)
							continue;

						const auto& termOp = search_program_term_op(programOps[i]);
						const auto missingMask = (uint8_t)(termOp.varMask & ~vars.mask);
						if (missingMask == 0)
							return false;
						if ((missingMask & ~anyVarMask) != 0)
							return false;
					}

					return true;
				}

				GAIA_NODISCARD bool match_search_program_on_archetype(
						const MatchingCtx& ctx, const Archetype& archetype,
						const detail::QueryCompileCtx::VarProgramStep& programStep, bool orAlreadySatisfied) const {
					using namespace detail;

					constexpr uint16_t BacktrackPc = (uint16_t)-1;

					struct SearchProgramState {
						VarBindings vars{};
						uint16_t pendingAllMask = 0;
						uint16_t pendingOrMask = 0;
						uint16_t pendingFinalOrMask = 0;
						uint16_t pendingAnyMask = 0;
						uint16_t pc = BacktrackPc;
						uint8_t termOpIdx = 0xff;
						uint8_t bestOrIdx = 0xff;
						uint8_t scanIdx = 0;
						bool orMatched = false;
						bool anyMatched = false;
						bool currentAnyMatched = false;
					};

					struct SearchBacktrackFrame {
						SearchProgramState state{};
						VarBindings varsBase{};
						VarTermMatchCursor cursor{};
					};

					const auto& program = programStep.program;
					const auto& search = programStep.search;
					const auto programOps = detail::program_ops(m_compCtx, program);
					if (programOps.empty())
						return true;
					GAIA_ASSERT(search.enterPc != BacktrackPc);
					GAIA_ASSERT(search.selectOtherOrPc != BacktrackPc);
					GAIA_ASSERT(search.selectAnyPc != BacktrackPc);
					GAIA_ASSERT(search.finalizePc != BacktrackPc);

					const auto is_term_ready = [&](const detail::CompiledOp& op, const VarBindings& vars) {
						const auto& termOp = search_program_term_op(op);
						return !is_var_entity(termOp.term.src) || var_is_bound(vars, termOp.term.src);
					};
					const auto advance_after_search_term_success = [&](SearchProgramState& state, const detail::CompiledOp& op,
																														 VarBindings nextVars) {
						const auto bit = (uint16_t)(uint16_t(1) << state.termOpIdx);
						state.vars = nextVars;
						switch (op.opcode) {
							case EOpcode::Var_Term_All:
								state.pendingAllMask = (uint16_t)(state.pendingAllMask & ~bit);
								state.pc = op.pc_ok;
								break;
							case EOpcode::Var_Term_Or_Check:
							case EOpcode::Var_Term_Or_Bind:
								state.pendingOrMask = (uint16_t)(state.pendingOrMask & ~(uint16_t(1) << state.termOpIdx));
								state.pendingFinalOrMask = (uint16_t)(state.pendingFinalOrMask & ~(uint16_t(1) << state.termOpIdx));
								state.orMatched = true;
								state.pc = op.pc_ok;
								break;
							case EOpcode::Var_Term_Any_Check:
							case EOpcode::Var_Term_Any_Bind:
								state.pendingAnyMask = (uint16_t)(state.pendingAnyMask & ~(uint16_t(1) << state.termOpIdx));
								state.anyMatched = true;
								state.currentAnyMatched = true;
								state.pc = op.pc_ok;
								break;
							default:
								GAIA_ASSERT(false);
								state.pc = BacktrackPc;
								break;
						}
					};
					const auto handle_search_term_exhausted = [&](SearchProgramState& state, const detail::CompiledOp& op) {
						if ((op.opcode == EOpcode::Var_Term_Any_Check || op.opcode == EOpcode::Var_Term_Any_Bind) &&
								!state.currentAnyMatched) {
							state.pendingAnyMask = (uint16_t)(state.pendingAnyMask & ~(uint16_t(1) << state.termOpIdx));
						}
						state.pc = op.pc_fail;
					};
					const auto try_enter_search_term = [&](SearchProgramState& state,
																								 cnt::sarray_ext<SearchBacktrackFrame, MAX_ITEMS_IN_QUERY>& stack) {
						const auto& op = programOps[state.pc];
						const auto& termOp = search_program_term_op(op);
						VarBindings nextVars{};
						SearchBacktrackFrame frame{};
						frame.state = state;
						frame.varsBase = state.vars;

						if (!detail::next_term_match_cursor(*ctx.pWorld, archetype, termOp, frame.varsBase, frame.cursor, nextVars))
							return false;

						if (op.opcode == EOpcode::Var_Term_Any_Check || op.opcode == EOpcode::Var_Term_Any_Bind) {
							frame.state.anyMatched = true;
							frame.state.currentAnyMatched = true;
						}

						stack.push_back(GAIA_MOV(frame));
						advance_after_search_term_success(state, op, nextVars);
						return true;
					};
					const auto backtrack = [&](SearchProgramState& state,
																		 cnt::sarray_ext<SearchBacktrackFrame, MAX_ITEMS_IN_QUERY>& stack) {
						while (!stack.empty()) {
							auto& frame = stack.back();
							const auto resumeState = frame.state;
							const auto& op = programOps[resumeState.pc];
							const auto& termOp = search_program_term_op(op);
							VarBindings nextVars{};

							if (detail::next_term_match_cursor(
											*ctx.pWorld, archetype, termOp, frame.varsBase, frame.cursor, nextVars)) {
								if (op.opcode == EOpcode::Var_Term_Any_Check || op.opcode == EOpcode::Var_Term_Any_Bind) {
									frame.state.anyMatched = true;
									frame.state.currentAnyMatched = true;
								}

								state = frame.state;
								advance_after_search_term_success(state, op, nextVars);
								return true;
							}

							stack.pop_back();
							state = resumeState;
							handle_search_term_exhausted(state, op);
							if (state.pc != BacktrackPc)
								return true;
						}

						return false;
					};

					cnt::sarray_ext<SearchBacktrackFrame, MAX_ITEMS_IN_QUERY> stack;
					SearchProgramState state{};
					state.vars = make_initial_var_bindings(ctx);
					state.pendingAllMask = search.initialAllMask;
					state.pendingOrMask = search.initialOrMask;
					state.pendingFinalOrMask = search.initialOrMask;
					state.pendingAnyMask = search.initialAnyMask;
					state.pc = search.enterPc;

					for (;;) {
						const auto& op = programOps[state.pc];
						switch (op.opcode) {
							case EOpcode::Var_Search_Enter: {
								if (state.pendingAllMask == 0) {
									state.pc = search.finalizePc;
									break;
								}

								if (search.orCount == 0 && search.anyCount == 0) {
									uint32_t bestAllIdx = (uint32_t)-1;
									const auto allSel = select_best_pending_search_term(
											ctx, archetype, programOps, search.allBegin, search.allCount, state.pendingAllMask, state.vars,
											bestAllIdx);
									if (allSel < 0) {
										if (!backtrack(state, stack))
											return false;
										break;
									}

									if (allSel > 0) {
										state.termOpIdx = (uint8_t)bestAllIdx;
										state.pc = (uint16_t)bestAllIdx;
										break;
									}
								} else {
									const auto nextAllIdx = select_next_pending_search_term(
											programOps, search.allBegin, search.allCount, state.pendingAllMask, state.vars);
									if (nextAllIdx != (uint32_t)-1) {
										state.termOpIdx = (uint8_t)nextAllIdx;
										state.pc = (uint16_t)nextAllIdx;
										break;
									}
								}

								uint32_t nextOrLocalIdx = (uint32_t)-1;
								uint32_t nextOrPc = (uint32_t)-1;
								if (select_next_pending_search_or_term(
												programOps, search, state.pendingOrMask, state.pendingFinalOrMask, state.vars, !state.orMatched,
												state.orMatched, nextOrLocalIdx, nextOrPc)) {
									state.bestOrIdx = (uint8_t)nextOrLocalIdx;
									state.termOpIdx = state.bestOrIdx;
									state.pc = (uint16_t)nextOrPc;
									break;
								}

								state.bestOrIdx = 0xff;
								state.scanIdx = 0;
								state.pc = search.selectOtherOrPc;
								break;
							}
							case EOpcode::Var_Search_SelectOtherOr: {
								uint32_t firstReadyBindingLocalIdx = (uint32_t)-1;
								bool found = false;
								while (state.scanIdx < search.orCount) {
									const auto localIdx = (uint32_t)state.scanIdx++;
									if (localIdx == state.bestOrIdx)
										continue;

									const auto bit = (uint16_t)(uint16_t(1) << localIdx);
									if ((state.pendingOrMask & bit) == 0)
										continue;
									const auto bindPc = (uint32_t)search.orBegin + localIdx;
									if (!is_term_ready(programOps[bindPc], state.vars))
										continue;

									const bool bindsNewVars =
											(uint8_t)(search_program_term_op(programOps[bindPc]).varMask & ~state.vars.mask) != 0;
									if (state.orMatched) {
										if (!bindsNewVars)
											continue;
									} else {
										if (!bindsNewVars && (state.pendingFinalOrMask & bit) == 0)
											continue;
										if (bindsNewVars) {
											if (firstReadyBindingLocalIdx == (uint32_t)-1)
												firstReadyBindingLocalIdx = localIdx;
											continue;
										}
									}

									state.termOpIdx = (uint8_t)localIdx;
									state.pc = (uint16_t)((uint32_t)search.orCheckBegin + localIdx);
									found = true;
									break;
								}

								if (!found && !state.orMatched && firstReadyBindingLocalIdx != (uint32_t)-1) {
									state.termOpIdx = (uint8_t)firstReadyBindingLocalIdx;
									state.pc = (uint16_t)((uint32_t)search.orBegin + firstReadyBindingLocalIdx);
									found = true;
								}

								if (!found) {
									state.anyMatched = false;
									state.scanIdx = 0;
									state.pc = search.selectAnyPc;
								}
								break;
							}
							case EOpcode::Var_Search_SelectAny: {
								uint32_t nextAnyLocalIdx = (uint32_t)-1;
								uint32_t nextAnyPc = (uint32_t)-1;
								const bool found = select_next_pending_search_any_term(
										programOps, search, state.pendingAnyMask, state.vars, nextAnyLocalIdx, nextAnyPc);
								if (found) {
									state.termOpIdx = (uint8_t)nextAnyLocalIdx;
									state.currentAnyMatched = false;
									state.pc = (uint16_t)nextAnyPc;
								}

								if (found)
									break;

								if (!state.anyMatched &&
										can_skip_pending_search_all(programOps, search, state.pendingAllMask, state.vars)) {
									state.pc = search.finalizePc;
									break;
								}

								if (!backtrack(state, stack))
									return false;
								break;
							}
							case EOpcode::Var_Search_Finalize:
								state.pc = op.pc_ok;
								break;
							case EOpcode::Var_Term_All:
							case EOpcode::Var_Term_Or_Check:
							case EOpcode::Var_Term_Or_Bind:
							case EOpcode::Var_Term_Any_Check:
							case EOpcode::Var_Term_Any_Bind: {
								const auto& termOp = search_program_term_op(op);
								const bool bindsNewVars = (uint8_t)(termOp.varMask & ~state.vars.mask) != 0;
								if (!bindsNewVars) {
									if (term_has_match(*ctx.pWorld, archetype, termOp, state.vars))
										advance_after_search_term_success(state, op, state.vars);
									else {
										if (op.opcode == EOpcode::Var_Term_Or_Check || op.opcode == EOpcode::Var_Term_Or_Bind)
											state.pendingFinalOrMask =
													(uint16_t)(state.pendingFinalOrMask & ~(uint16_t(1) << state.termOpIdx));
										handle_search_term_exhausted(state, op);
										if (state.pc == BacktrackPc && !backtrack(state, stack))
											return false;
									}
									break;
								}

								if (!try_enter_search_term(state, stack)) {
									handle_search_term_exhausted(state, op);
									if (state.pc == BacktrackPc && !backtrack(state, stack))
										return false;
								}
								break;
							}
							case EOpcode::Var_Final_Not_Check:
								if (term_has_match(*ctx.pWorld, archetype, search_program_term_op(op), state.vars)) {
									if (!backtrack(state, stack))
										return false;
								} else
									state.pc = op.pc_ok;
								break;
							case EOpcode::Var_Final_Require_Or:
								if (orAlreadySatisfied || state.orMatched || search.orCount == 0)
									state.pc = op.pc_ok;
								else if (op.pc_fail != BacktrackPc)
									state.pc = op.pc_fail;
								else if (!backtrack(state, stack))
									return false;
								break;
							case EOpcode::Var_Final_Or_Check:
								if ((state.pendingFinalOrMask & (uint16_t(1) << op.arg)) == 0)
									state.pc = op.pc_fail;
								else if (term_has_match(*ctx.pWorld, archetype, search_program_term_op(op), state.vars))
									state.pc = op.pc_ok;
								else {
									state.pendingFinalOrMask = (uint16_t)(state.pendingFinalOrMask & ~(uint16_t(1) << op.arg));
									if (op.pc_fail != BacktrackPc)
										state.pc = op.pc_fail;
									else if (!backtrack(state, stack))
										return false;
								}
								break;
							case EOpcode::Var_Final_Success:
								return true;
							default:
								GAIA_ASSERT(false);
								return false;
						}
					}
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

				GAIA_NODISCARD bool op_var_filter(MatchingCtx& ctx) const {
					GAIA_PROF_SCOPE(vm::op_var_filter);
					GAIA_ASSERT(!m_compCtx.varPrograms.empty());
					filter_variable_terms(ctx, &VirtualMachine::eval_variable_terms_program_on_archetype);
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
						&VirtualMachine::op_var_filter, //
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
					append_uint(out, (uint32_t)m_compCtx.mainOpsCount);
					out.append('\n');

					const auto opsCnt = (uint32_t)m_compCtx.mainOpsCount;
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
					append_var_program_exec_section(out, m_compCtx);
					append_var_program_sections(out, m_compCtx, world);

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
					m_compCtx.varPrograms.clear();
					m_compCtx.mainOpsCount = 0;
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

					constexpr uint32_t VarSearchProgramOpCapacity = MAX_ITEMS_IN_QUERY * 3u + 8u;
					cnt::sarray_ext<detail::CompiledOp, VarSearchProgramOpCapacity> varSearchProgramOps;
					detail::QueryCompileCtx::VarSearchMeta varSearchMeta{};

					auto init_var_search_program = [&]() {
						varSearchProgramOps.clear();
						varSearchMeta = {};
						cnt::sarray_ext<detail::CompiledOp, MAX_ITEMS_IN_QUERY> searchAllOps;
						cnt::sarray_ext<detail::CompiledOp, MAX_ITEMS_IN_QUERY> searchOrBindOps;
						cnt::sarray_ext<detail::CompiledOp, MAX_ITEMS_IN_QUERY> searchOrCheckOps;
						cnt::sarray_ext<detail::CompiledOp, MAX_ITEMS_IN_QUERY> searchAnyBindOps;
						cnt::sarray_ext<detail::CompiledOp, MAX_ITEMS_IN_QUERY> searchAnyCheckOps;
						cnt::sarray_ext<detail::CompiledOp, MAX_ITEMS_IN_QUERY> finalNotOps;
						cnt::sarray_ext<detail::CompiledOp, MAX_ITEMS_IN_QUERY> finalOrCheckOps;

						const auto allVarCnt = (uint32_t)m_compCtx.terms_all_var.size();
						GAIA_FOR(allVarCnt) {
							searchAllOps.push_back(
									{detail::EOpcode::Var_Term_All, 0, 0, (uint8_t)i,
									 detail::search_term_cost(m_compCtx.terms_all_var[i])});
						}
						detail::sort_program_ops_by_cost(searchAllOps);

						const auto orVarCnt = (uint32_t)m_compCtx.terms_or_var.size();
						GAIA_FOR(orVarCnt) {
							const auto cost = detail::search_term_cost(m_compCtx.terms_or_var[i]);
							searchOrBindOps.push_back({detail::EOpcode::Var_Term_Or_Bind, 0, 0, (uint8_t)i, cost});
							searchOrCheckOps.push_back({detail::EOpcode::Var_Term_Or_Check, 0, 0, (uint8_t)i, cost});
							finalOrCheckOps.push_back({detail::EOpcode::Var_Final_Or_Check, 0, 0, (uint8_t)i, cost});
						}
						detail::sort_program_ops_by_cost(searchOrBindOps);
						detail::sort_program_ops_by_cost(searchOrCheckOps);
						detail::sort_program_ops_by_cost(finalOrCheckOps);

						const auto anyVarCnt = (uint32_t)m_compCtx.terms_any_var.size();
						GAIA_FOR(anyVarCnt) {
							const auto cost = detail::search_term_cost(m_compCtx.terms_any_var[i]);
							searchAnyBindOps.push_back({detail::EOpcode::Var_Term_Any_Bind, 0, 0, (uint8_t)i, cost});
							searchAnyCheckOps.push_back({detail::EOpcode::Var_Term_Any_Check, 0, 0, (uint8_t)i, cost});
						}
						detail::sort_program_ops_by_cost(searchAnyBindOps);
						detail::sort_program_ops_by_cost(searchAnyCheckOps);

						const auto notVarCnt = (uint32_t)m_compCtx.terms_not_var.size();
						GAIA_FOR(notVarCnt) {
							finalNotOps.push_back(
									{detail::EOpcode::Var_Final_Not_Check, 0, 0, (uint8_t)i,
									 detail::search_term_cost(m_compCtx.terms_not_var[i])});
						}
						detail::sort_program_ops_by_cost(finalNotOps);

						for (const auto& op: searchAllOps)
							varSearchProgramOps.push_back(op);
						for (const auto& op: searchOrBindOps)
							varSearchProgramOps.push_back(op);
						for (const auto& op: searchAnyBindOps)
							varSearchProgramOps.push_back(op);

						varSearchMeta.allBegin = 0;
						varSearchMeta.allCount = (uint16_t)searchAllOps.size();
						varSearchMeta.orBegin = varSearchMeta.allCount;
						varSearchMeta.orCount = (uint16_t)searchOrBindOps.size();
						varSearchMeta.anyBegin = (uint16_t)(varSearchMeta.orBegin + varSearchMeta.orCount);
						varSearchMeta.anyCount = (uint16_t)searchAnyBindOps.size();
						varSearchMeta.notBegin = 0;
						varSearchMeta.notCount = 0;

						const auto termOpsCnt = (uint16_t)varSearchProgramOps.size();
						const auto enterPc = termOpsCnt;
						const auto selectOtherOrPc = (uint16_t)(termOpsCnt + 1u);
						const auto selectAnyPc = (uint16_t)(termOpsCnt + 2u);
						const auto finalizePc = (uint16_t)(termOpsCnt + 3u);
						const auto orCheckBegin = (uint16_t)(termOpsCnt + 4u);
						const auto anyCheckBegin = (uint16_t)(orCheckBegin + searchOrCheckOps.size());
						const auto finalNotBegin = (uint16_t)(anyCheckBegin + searchAnyCheckOps.size());
						const auto finalRequireOrPc = (uint16_t)(finalNotBegin + finalNotOps.size());
						const auto finalOrCheckBegin = (uint16_t)(finalRequireOrPc + 1u);
						const auto finalSuccessPc = (uint16_t)(finalOrCheckBegin + finalOrCheckOps.size());
						const auto finalBegin = !finalNotOps.empty() ? finalNotBegin : finalRequireOrPc;
						const auto backtrackPc = (detail::VmLabel)-1;

						for (auto& op: varSearchProgramOps) {
							switch (op.opcode) {
								case detail::EOpcode::Var_Term_All:
									op.pc_ok = enterPc;
									op.pc_fail = backtrackPc;
									break;
								case detail::EOpcode::Var_Term_Or_Bind:
									op.pc_ok = enterPc;
									op.pc_fail = selectOtherOrPc;
									break;
								case detail::EOpcode::Var_Term_Any_Bind:
									op.pc_ok = enterPc;
									op.pc_fail = selectAnyPc;
									break;
								default:
									break;
							}
						}

						varSearchProgramOps.push_back({detail::EOpcode::Var_Search_Enter, enterPc, backtrackPc, 0, 0});
						varSearchProgramOps.push_back(
								{detail::EOpcode::Var_Search_SelectOtherOr, selectOtherOrPc, selectAnyPc, 0, 0});
						varSearchProgramOps.push_back({detail::EOpcode::Var_Search_SelectAny, selectAnyPc, finalizePc, 0, 0});
						varSearchProgramOps.push_back({detail::EOpcode::Var_Search_Finalize, finalBegin, backtrackPc, 0, 0});
						for (auto op: searchOrCheckOps) {
							op.pc_ok = enterPc;
							op.pc_fail = selectOtherOrPc;
							varSearchProgramOps.push_back(op);
						}
						for (auto op: searchAnyCheckOps) {
							op.pc_ok = enterPc;
							op.pc_fail = selectAnyPc;
							varSearchProgramOps.push_back(op);
						}
						for (uint32_t i = 0; i < finalNotOps.size(); ++i) {
							auto op = finalNotOps[i];
							op.pc_ok = (i + 1u < finalNotOps.size()) ? (uint16_t)(finalNotBegin + i + 1u) : finalRequireOrPc;
							op.pc_fail = backtrackPc;
							varSearchProgramOps.push_back(op);
						}
						varSearchProgramOps.push_back(
								{detail::EOpcode::Var_Final_Require_Or, finalSuccessPc,
								 searchOrCheckOps.empty() ? backtrackPc : finalOrCheckBegin, 0, 0});
						for (uint32_t i = 0; i < finalOrCheckOps.size(); ++i) {
							auto op = finalOrCheckOps[i];
							op.pc_ok = finalSuccessPc;
							op.pc_fail = (i + 1u < finalOrCheckOps.size()) ? (uint16_t)(finalOrCheckBegin + i + 1u) : backtrackPc;
							varSearchProgramOps.push_back(op);
						}
						varSearchProgramOps.push_back({detail::EOpcode::Var_Final_Success, finalSuccessPc, backtrackPc, 0, 0});

						varSearchMeta.enterPc = enterPc;
						varSearchMeta.selectOtherOrPc = selectOtherOrPc;
						varSearchMeta.selectAnyPc = selectAnyPc;
						varSearchMeta.finalizePc = finalizePc;
						varSearchMeta.orCheckBegin = orCheckBegin;
						varSearchMeta.anyCheckBegin = anyCheckBegin;
						varSearchMeta.notBegin = finalNotBegin;
						varSearchMeta.notCount = (uint16_t)finalNotOps.size();

						auto init_mask = [](uint16_t begin, uint16_t count) {
							uint16_t mask = 0;
							for (uint16_t i = 0; i < count; ++i)
								mask = (uint16_t)(mask | (uint16_t(1) << (begin + i)));
							return mask;
						};

						varSearchMeta.initialAllMask = init_mask(varSearchMeta.allBegin, varSearchMeta.allCount);
						varSearchMeta.initialOrMask = init_mask(0, varSearchMeta.orCount);
						varSearchMeta.initialAnyMask = init_mask(0, varSearchMeta.anyCount);
					};

					create_opcodes(queryCtx);

					init_var_search_program();

					auto emit_flat_program = [&](std::span<const detail::CompiledOp> ops) {
						detail::QueryCompileCtx::VarProgram program{};
						program.clear();
						if (ops.empty())
							return program;

						GAIA_ASSERT(m_compCtx.ops.size() <= UINT16_MAX);
						program.begin = (uint16_t)m_compCtx.ops.size();
						program.count = (uint16_t)ops.size();
						for (const auto& op: ops)
							m_compCtx.ops.push_back(op);
						return program;
					};

					m_compCtx.varPrograms.clear();
					if (m_compCtx.has_variable_terms()) {
						const auto program = emit_flat_program(
								std::span<const detail::CompiledOp>{varSearchProgramOps.data(), varSearchProgramOps.size()});
						if (!program.empty())
							m_compCtx.varPrograms.push_back({program, varSearchMeta});
					}
				}

				void create_opcodes(QueryCtx& queryCtx) {
					const bool isSimple = (queryCtx.data.flags & QueryCtx::QueryFlags::Complex) == 0U;
					const bool isAs = (queryCtx.data.as_mask_0 + queryCtx.data.as_mask_1) != 0U;
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
						detail::CompiledOp op{};
						op.opcode = detail::EOpcode::Var_Filter;
						(void)add_gate_op(GAIA_MOV(op));
					}

					m_compCtx.mainOpsCount = (uint16_t)m_compCtx.ops.size();

					// Mark as compiled
					queryCtx.data.flags &= ~QueryCtx::QueryFlags::Recompile;
				}

				GAIA_NODISCARD bool is_compiled() const {
					return !m_compCtx.ops.empty();
				}

				GAIA_NODISCARD uint32_t op_count() const {
					return (uint32_t)m_compCtx.ops.size();
				}

				GAIA_NODISCARD uint64_t op_signature() const {
					uint64_t hash = 1469598103934665603ull;
					for (const auto& op: m_compCtx.ops) {
						const uint64_t packed = //
								(uint64_t)(uint8_t)op.opcode | //
								((uint64_t)op.pc_ok << 8u) | //
								((uint64_t)op.pc_fail << 24u) | //
								((uint64_t)op.arg << 40u) | //
								((uint64_t)op.cost << 48u);
						hash ^= packed;
						hash *= 1099511628211ull;
					}
					return hash;
				}

				//! Executes compiled opcodes
				void exec(MatchingCtx& ctx) {
					GAIA_PROF_SCOPE(vm::exec);
					ctx.skipOr = false;
					if (m_compCtx.mainOpsCount == 0)
						return;

					ctx.pc = 0;

					// Extract data from the buffer
					do {
						auto& stackItem = m_compCtx.ops[ctx.pc];
						GAIA_ASSERT((uint32_t)stackItem.opcode < (uint32_t)detail::EOpcode::Src_Never);
						const bool ret = exec_opcode(stackItem, ctx);
						ctx.pc = ret ? stackItem.pc_ok : stackItem.pc_fail;
					} while (ctx.pc < m_compCtx.mainOpsCount); // (uint32_t)-1 falls in this category as well
				}
			};

		} // namespace vm
	} // namespace ecs

} // namespace gaia
