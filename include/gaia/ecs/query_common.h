#pragma once
#include "gaia/config/config.h"

#include <type_traits>

#include "gaia/cnt/darray.h"
#include "gaia/cnt/map.h"
#include "gaia/cnt/sarray.h"
#include "gaia/cnt/sarray_ext.h"
#include "gaia/core/bit_utils.h"
#include "gaia/core/hashing_policy.h"
#include "gaia/core/utility.h"
#include "gaia/ecs/api.h"
#include "gaia/ecs/component.h"
#include "gaia/ecs/id.h"
#include "gaia/ecs/query_fwd.h"
#include "gaia/ecs/query_mask.h"

namespace gaia {
	namespace ecs {
		class World;
		class Archetype;
		GAIA_NODISCARD uint32_t world_rel_version(const World& world, Entity relation);
		GAIA_NODISCARD bool world_has_entity_term(const World& world, Entity entity, Entity term);
		GAIA_NODISCARD bool world_has_entity_term_direct(const World& world, Entity entity, Entity term);
		GAIA_NODISCARD bool world_is_exclusive_dont_fragment_relation(const World& world, Entity relation);
		GAIA_NODISCARD bool world_is_sparse_dont_fragment_component(const World& world, Entity component);
		GAIA_NODISCARD uint32_t world_count_direct_term_entities(const World& world, Entity term);
		GAIA_NODISCARD uint32_t world_count_direct_term_entities_direct(const World& world, Entity term);
		void world_collect_direct_term_entities(const World& world, Entity term, cnt::darray<Entity>& out);
		void world_collect_direct_term_entities_direct(const World& world, Entity term, cnt::darray<Entity>& out);
		GAIA_NODISCARD bool
		world_for_each_direct_term_entity(const World& world, Entity term, void* ctx, bool (*func)(void*, Entity));
		GAIA_NODISCARD bool
		world_for_each_direct_term_entity_direct(const World& world, Entity term, void* ctx, bool (*func)(void*, Entity));
		GAIA_NODISCARD bool world_entity_enabled(const World& world, Entity entity);
		GAIA_NODISCARD bool world_entity_prefab(const World& world, Entity entity);
		GAIA_NODISCARD const Archetype* world_entity_archetype(const World& world, Entity entity);
		template <typename T>
		GAIA_NODISCARD decltype(auto) world_direct_entity_arg(World& world, Entity entity);
		//! Returns the per-entity archetype version used for targeted source-query freshness checks.
		GAIA_NODISCARD uint32_t world_entity_archetype_version(const World& world, Entity entity);

		//! Number of items that can be a part of Query
		static constexpr uint32_t MAX_ITEMS_IN_QUERY = 12U;

		GAIA_GCC_WARNING_PUSH()
		// GCC is unnecessarily too strict about shadowing.
		// We have a globally defined entity All and thinks QueryOpKind::All shadows it.
		GAIA_GCC_WARNING_DISABLE("-Wshadow")

		//! Operation type
		enum class QueryOpKind : uint8_t { All, Or, Not, Any, Count };
		//! Access type
		enum class QueryAccess : uint8_t { None, Read, Write };
		//! Term match semantics.
		enum class QueryMatchKind : uint8_t { Semantic, Direct };
		//! Operation flags
		enum class QueryInputFlags : uint8_t { None, Variable };
		//! Source traversal filter used for source lookups.
		enum class QueryTravKind : uint8_t { None = 0x00, Self = 0x01, Up = 0x02, Down = 0x04 };

		GAIA_GCC_WARNING_POP()

		GAIA_NODISCARD constexpr QueryTravKind operator|(QueryTravKind lhs, QueryTravKind rhs) {
			return (QueryTravKind)((uint8_t)lhs | (uint8_t)rhs);
		}

		GAIA_NODISCARD constexpr bool query_trav_has(QueryTravKind value, QueryTravKind bit) {
			return (((uint8_t)value) & ((uint8_t)bit)) != 0;
		}

		using QueryLookupHash = core::direct_hash_key<uint64_t>;
		using QueryEntityArray = cnt::sarray<Entity, MAX_ITEMS_IN_QUERY>;
		using QueryArchetypeCacheIndexMap = cnt::map<EntityLookupKey, uint32_t>;
		using QuerySerMap = cnt::map<QueryId, QuerySerBuffer>;

		static constexpr QueryId QueryIdBad = (QueryId)-1;
		static constexpr GroupId GroupIdMax = ((GroupId)-1) - 1;

		struct QueryHandle {
			static constexpr uint32_t IdMask = QueryIdBad;

		private:
			struct HandleData {
				QueryId id;
				uint32_t gen;
			};

			union {
				HandleData data;
				uint64_t val;
			};

		public:
			constexpr QueryHandle() noexcept: val((uint64_t)-1) {};

			QueryHandle(QueryId id, uint32_t gen) {
				data.id = id;
				data.gen = gen;
			}
			~QueryHandle() = default;

			QueryHandle(QueryHandle&&) noexcept = default;
			QueryHandle(const QueryHandle&) = default;
			QueryHandle& operator=(QueryHandle&&) noexcept = default;
			QueryHandle& operator=(const QueryHandle&) = default;

			GAIA_NODISCARD constexpr bool operator==(const QueryHandle& other) const noexcept {
				return val == other.val;
			}
			GAIA_NODISCARD constexpr bool operator!=(const QueryHandle& other) const noexcept {
				return val != other.val;
			}

			GAIA_NODISCARD auto id() const {
				return data.id;
			}
			GAIA_NODISCARD auto gen() const {
				return data.gen;
			}
			GAIA_NODISCARD auto value() const {
				return val;
			}
		};

		inline static const QueryHandle QueryHandleBad = QueryHandle();

		//! Hashmap lookup structure used for Entity
		struct QueryHandleLookupKey {
			using LookupHash = core::direct_hash_key<uint64_t>;

		private:
			//! Entity
			QueryHandle m_handle;
			//! Entity hash
			LookupHash m_hash;

			static LookupHash calc(QueryHandle handle) {
				return {core::calculate_hash64(handle.value())};
			}

		public:
			static constexpr bool IsDirectHashKey = true;

			QueryHandleLookupKey() = default;
			explicit QueryHandleLookupKey(QueryHandle handle): m_handle(handle), m_hash(calc(handle)) {}
			~QueryHandleLookupKey() = default;

			QueryHandleLookupKey(const QueryHandleLookupKey&) = default;
			QueryHandleLookupKey(QueryHandleLookupKey&&) = default;
			QueryHandleLookupKey& operator=(const QueryHandleLookupKey&) = default;
			QueryHandleLookupKey& operator=(QueryHandleLookupKey&&) = default;

			QueryHandle handle() const {
				return m_handle;
			}

			size_t hash() const {
				return (size_t)m_hash.hash;
			}

			bool operator==(const QueryHandleLookupKey& other) const {
				if GAIA_LIKELY (m_hash != other.m_hash)
					return false;

				return m_handle == other.m_handle;
			}

			bool operator!=(const QueryHandleLookupKey& other) const {
				return !operator==(other);
			}
		};

		inline static const QueryHandleLookupKey QueryHandleBadLookupKey = QueryHandleLookupKey(QueryHandleBad);

		//! User-provided query input
		struct QueryInput {
			static constexpr uint8_t TravDepthUnlimited = 0;

			//! Operation to perform with the input
			QueryOpKind op = QueryOpKind::All;
			//! Access type
			QueryAccess access = QueryAccess::Read;
			//! Entity/Component/Pair to query
			Entity id;
			//! Source entity to query the id on.
			//! If id==EntityBad the source is fixed.
			//! If id!=src the source is variable.
			Entity entSrc = EntityBad;
			//! Optional traversal relation for source lookups.
			//! When set, the lookup starts at src and then walks relation targets upwards and/or downwards.
			Entity entTrav = EntityBad;
			//! Source traversal filter.
			//! `Self` means checking the source itself, `Up` means checking traversed ancestors,
			//! `Down` means checking traversed descendants.
			QueryTravKind travKind = QueryTravKind::Self | QueryTravKind::Up;
			//! Maximum number of traversal steps.
			//! 0 means unlimited traversal depth (bounded internally).
			uint8_t travDepth = TravDepthUnlimited;
			//! Match semantics for terms with special meaning, such as `Pair(Is, X)`.
			QueryMatchKind matchKind = QueryMatchKind::Semantic;
		};

		//! Additional options for query terms.
		//! This can be used to configure source lookup, traversal and access mode
		//! without relying on many positional overloads.
		struct QueryTermOptions {
			static constexpr uint8_t TravDepthUnlimited = QueryInput::TravDepthUnlimited;

			//! Source entity to query from.
			Entity entSrc = EntityBad;
			//! Optional traversal relation used for source lookup.
			Entity entTrav = EntityBad;
			//! Source traversal filter.
			QueryTravKind travKind = QueryTravKind::Self | QueryTravKind::Up;
			//! Maximum number of traversal steps.
			//! 0 means unlimited traversal depth (bounded internally).
			uint8_t travDepth = TravDepthUnlimited;
			//! Access mode for the term.
			//! When None, typed query terms infer read/write access from template mutability.
			QueryAccess access = QueryAccess::None;
			//! Match semantics for terms with special meaning, such as `Pair(Is, X)`.
			QueryMatchKind matchKind = QueryMatchKind::Semantic;

			QueryTermOptions& src(Entity source) {
				entSrc = source;
				return *this;
			}

			QueryTermOptions& trav(Entity relation = ChildOf) {
				entTrav = relation;
				travKind = QueryTravKind::Self | QueryTravKind::Up;
				travDepth = TravDepthUnlimited;
				return *this;
			}

			QueryTermOptions& trav_up(Entity relation = ChildOf) {
				entTrav = relation;
				travKind = QueryTravKind::Up;
				travDepth = TravDepthUnlimited;
				return *this;
			}

			QueryTermOptions& trav_parent(Entity relation = ChildOf) {
				entTrav = relation;
				travKind = QueryTravKind::Up;
				travDepth = 1;
				return *this;
			}

			QueryTermOptions& trav_self_parent(Entity relation = ChildOf) {
				entTrav = relation;
				travKind = QueryTravKind::Self | QueryTravKind::Up;
				travDepth = 1;
				return *this;
			}

			QueryTermOptions& trav_down(Entity relation = ChildOf) {
				entTrav = relation;
				travKind = QueryTravKind::Down;
				travDepth = TravDepthUnlimited;
				return *this;
			}

			QueryTermOptions& trav_self_down(Entity relation = ChildOf) {
				entTrav = relation;
				travKind = QueryTravKind::Self | QueryTravKind::Down;
				travDepth = TravDepthUnlimited;
				return *this;
			}

			QueryTermOptions& trav_child(Entity relation = ChildOf) {
				entTrav = relation;
				travKind = QueryTravKind::Down;
				travDepth = 1;
				return *this;
			}

			QueryTermOptions& trav_self_child(Entity relation = ChildOf) {
				entTrav = relation;
				travKind = QueryTravKind::Self | QueryTravKind::Down;
				travDepth = 1;
				return *this;
			}

			QueryTermOptions& trav_kind(QueryTravKind kind) {
				travKind = kind;
				return *this;
			}

			QueryTermOptions& trav_depth(uint8_t maxDepth) {
				travDepth = maxDepth;
				return *this;
			}

			QueryTermOptions& read() {
				access = QueryAccess::Read;
				return *this;
			}

			QueryTermOptions& write() {
				access = QueryAccess::Write;
				return *this;
			}

			QueryTermOptions& direct() {
				matchKind = QueryMatchKind::Direct;
				return *this;
			}
		};

		//! Internal representation of QueryInput
		struct QueryTerm {
			//! Queried id
			Entity id;
			//! Source of where the queried id is looked up at
			Entity src;
			//! Optional traversal relation for source lookups
			Entity entTrav;
			//! Source traversal filter.
			QueryTravKind travKind;
			//! Maximum number of traversal steps.
			uint8_t travDepth;
			//! Match semantics for this term.
			QueryMatchKind matchKind;
			//! Archetype of the src entity
			Archetype* srcArchetype;
			//! Operation to perform with the term
			QueryOpKind op;

			bool operator==(const QueryTerm& other) const {
				return id == other.id && src == other.src && entTrav == other.entTrav && travKind == other.travKind &&
							 travDepth == other.travDepth && matchKind == other.matchKind && op == other.op;
			}
			bool operator!=(const QueryTerm& other) const {
				return !operator==(other);
			}
		};

		GAIA_NODISCARD inline bool term_has_variables(const QueryTerm& term) {
			if (is_variable(term.src))
				return true;

			if (term.id.pair())
				return is_variable(EntityId(term.id.id())) || is_variable(EntityId(term.id.gen()));

			return is_variable(EntityId(term.id.id()));
		}

		using QueryTermArray = cnt::sarray_ext<QueryTerm, MAX_ITEMS_IN_QUERY>;
		using QueryTermSpan = std::span<QueryTerm>;
		using QueryRemappingArray = cnt::sarray_ext<uint8_t, MAX_ITEMS_IN_QUERY>;

		struct QueryIdentity {
			//! Query id
			QueryHandle handle = {};
			//! Serialization id
			QueryId serId = QueryIdBad;

			GAIA_NODISCARD QuerySerBuffer& ser_buffer(World* world) {
				return query_buffer(*world, serId);
			}
			void ser_buffer_reset(World* world) {
				query_buffer_reset(*world, serId);
			}
		};

		struct QueryCtx {
			// World
			const World* w{};
			//! Component cache
			ComponentCache* cc{};
			//! Lookup hash for this query
			QueryLookupHash hashLookup{};
			//! Query identity
			QueryIdentity q{};

			enum QueryFlags : uint8_t {
				Empty = 0x00,
				// Entities need sorting
				SortEntities = 0x01,
				// Groups need sorting
				SortGroups = 0x02,
				// Complex query
				Complex = 0x04,
				// Recompilation requested
				Recompile = 0x08,
				// Query contains source-based lookup terms
				HasSourceTerms = 0x10,
				// Query contains variable-based lookup terms
				HasVariableTerms = 0x20,
				// Include entities tagged with Prefab even when the query does not mention Prefab explicitly.
				MatchPrefab = 0x40,
				// Query mentions Prefab explicitly and therefore must not auto-exclude it.
				HasPrefabTerms = 0x80,
			};

			enum class CachePolicy : uint8_t {
				// Structural query with a positive selector term. Safe to update immediately on archetype creation.
				Immediate,
				// Structural query that stays cached but refreshes lazily on the next read.
				Lazy,
				// Query with source or variable terms. Cached state is repaired on demand.
				Dynamic,
			};

			enum class CreateArchetypeMatchKind : uint8_t {
				// Use the normal one-archetype VM path.
				Vm,
				// Match a small immediate ALL-term structural query directly on the archetype.
				DirectAllTerms,
			};

			enum DependencyFlags : uint16_t {
				DependencyNone = 0x00,
				DependencyHasSourceTerms = 0x01,
				DependencyHasVariableTerms = 0x02,
				DependencyHasPositiveTerms = 0x04,
				DependencyHasNegativeTerms = 0x08,
				DependencyHasAnyTerms = 0x10,
				DependencyHasWildcardTerms = 0x20,
				DependencyHasSort = 0x40,
				DependencyHasGroup = 0x80,
				DependencyHasTraversalTerms = 0x100,
				DependencyHasAdjunctTerms = 0x200,
			};

			struct Data {
				struct Dependencies {
					QueryEntityArray createSelectors;
					QueryEntityArray exclusions;
					QueryEntityArray relations;
					QueryEntityArray sourceEntities;
					uint8_t createSelectorCnt = 0;
					uint8_t exclusionCnt = 0;
					uint8_t relationCnt = 0;
					uint8_t sourceEntityCnt = 0;
					uint8_t sourceTermCnt = 0;
					DependencyFlags flags = DependencyNone;

					void clear() {
						createSelectorCnt = 0;
						exclusionCnt = 0;
						relationCnt = 0;
						sourceEntityCnt = 0;
						sourceTermCnt = 0;
						flags = DependencyNone;
					}

					GAIA_NODISCARD std::span<const Entity> create_selectors_view() const {
						return {createSelectors.data(), createSelectorCnt};
					}

					GAIA_NODISCARD std::span<const Entity> exclusions_view() const {
						return {exclusions.data(), exclusionCnt};
					}

					GAIA_NODISCARD std::span<const Entity> relations_view() const {
						return {relations.data(), relationCnt};
					}

					GAIA_NODISCARD std::span<const Entity> src_entities_view() const {
						return {sourceEntities.data(), sourceEntityCnt};
					}

					void add(DependencyFlags dependency) {
						flags = (DependencyFlags)(flags | dependency);
					}

					void add_rel(Entity relation) {
						if (relation == EntityBad || core::has(relations_view(), relation))
							return;

						GAIA_ASSERT(relationCnt < MAX_ITEMS_IN_QUERY);
						relations[relationCnt++] = relation;
					}

					void add_src_entity(Entity entity) {
						if (entity == EntityBad || core::has(src_entities_view(), entity))
							return;

						GAIA_ASSERT(sourceEntityCnt < MAX_ITEMS_IN_QUERY);
						sourceEntities[sourceEntityCnt++] = entity;
					}

					GAIA_NODISCARD bool can_reuse_src_cache() const {
						return sourceTermCnt > 0 && sourceTermCnt == sourceEntityCnt;
					}

					GAIA_NODISCARD bool has(DependencyFlags dependency) const {
						return (flags & dependency) != 0;
					}
				};

				//! Array of queried ids
				QueryEntityArray _ids;
				//! Array of terms
				cnt::sarray<QueryTerm, MAX_ITEMS_IN_QUERY> _terms;
				//! Mapping of the original indices to the new ones after sorting
				cnt::sarray<uint8_t, MAX_ITEMS_IN_QUERY> _remapping;
				//! Index of the last checked archetype in the component-to-archetype map
				QueryArchetypeCacheIndexMap lastMatchedArchetypeIdx_All;
				QueryArchetypeCacheIndexMap lastMatchedArchetypeIdx_Or;
				QueryArchetypeCacheIndexMap lastMatchedArchetypeIdx_Not;
				uint8_t idsCnt = 0;
				uint8_t changedCnt = 0;
				//! Iteration will be restricted only to target Group
				GroupId groupIdSet;
				//! Array of filtered components
				QueryEntityArray _changed;
				//! Entity to sort the archetypes by. EntityBad for no sorting.
				Entity sortBy;
				//! Function to use to perform sorting
				TSortByFunc sortByFunc;
				//! Entity to group the archetypes by. EntityBad for no grouping.
				Entity groupBy;
				//! Function to use to perform the grouping
				TGroupByFunc groupByFunc;
				//! Component mask used for faster matching of simple queries
				QueryMask queryMask;
				//! Mask for items with Is relationship pair.
				//! If the id is a pair, the first part (id) is written here.
				uint32_t as_mask_0;
				//! Mask for items with Is relationship pair.
				//! If the id is a pair, the second part (gen) is written here.
				uint32_t as_mask_1;
				//! First NOT record in pairs/ids/ops
				uint8_t firstNot;
				//! First ANY record in pairs/ids/ops
				uint8_t firstAny;
				//! First OR record in pairs/ids/ops
				uint8_t firstOr;
				//! Read-write mask. Bit 0 stands for component 0 in component arrays.
				//! A set bit means write access is requested.
				uint16_t readWriteMask;
				//! Query flags
				uint8_t flags;
				//! Runtime bindings for Var0..Var7.
				cnt::sarray<Entity, 8> varBindings;
				//! Bitmask of runtime variable bindings.
				uint8_t varBindingMask = 0;
				//! Maximum allowed size of an explicitly cached traversed-source lookup closure.
				uint16_t cacheSrcTrav = 0;
				//! Explicit dependency metadata derived from query shape.
				Dependencies deps;
				//! Cache maintenance policy derived from query shape.
				CachePolicy cachePolicy = CachePolicy::Lazy;
				//! Create-time archetype matcher derived from query shape.
				CreateArchetypeMatchKind createArchetypeMatchKind = CreateArchetypeMatchKind::Vm;

				GAIA_NODISCARD std::span<const Entity> ids_view() const {
					return {_ids.data(), idsCnt};
				}

				GAIA_NODISCARD std::span<const Entity> changed_view() const {
					return {_changed.data(), changedCnt};
				}

				GAIA_NODISCARD std::span<QueryTerm> terms_view_mut() {
					return {_terms.data(), idsCnt};
				}
				GAIA_NODISCARD std::span<const QueryTerm> terms_view() const {
					return {_terms.data(), idsCnt};
				}
			} data{};
			// Make sure that MAX_ITEMS_IN_QUERY can fit into data.readWriteMask
			static_assert(MAX_ITEMS_IN_QUERY < 16);

			void init(World* pWorld) {
				w = pWorld;
				cc = &comp_cache_mut(*pWorld);
			}

			void refresh() {
				const auto mask0_old = data.as_mask_0;
				const auto mask1_old = data.as_mask_1;
				const auto isComplex_old = data.flags & QueryFlags::Complex;
				const auto hasSourceTerms_old = data.flags & QueryFlags::HasSourceTerms;
				const auto hasVariableTerms_old = data.flags & QueryFlags::HasVariableTerms;
				const auto cachePolicy_old = data.cachePolicy;
				const auto createArchetypeMatchKind_old = data.createArchetypeMatchKind;
				const auto dependencyFlags_old = data.deps.flags;
				const auto createSelectorCnt_old = data.deps.createSelectorCnt;
				const auto exclusionCnt_old = data.deps.exclusionCnt;
				const auto relationCnt_old = data.deps.relationCnt;
				const auto sourceEntityCnt_old = data.deps.sourceEntityCnt;
				const auto sourceTermCnt_old = data.deps.sourceTermCnt;
				auto createSelectors_old = data.deps.createSelectors;
				auto exclusions_old = data.deps.exclusions;
				auto relations_old = data.deps.relations;
				auto sourceEntities_old = data.deps.sourceEntities;

				// Update masks
				{
					uint32_t as_mask_0 = 0;
					uint32_t as_mask_1 = 0;
					bool isComplex = false;
					bool hasSourceTerms = false;
					bool hasVariableTerms = false;
					bool hasPrefabTerms = false;
					bool hasCreateSelector = false;
					bool canDirectCreateArchetypeMatch = true;
					bool hasAdjunctTerms = false;
					QueryEntityArray idsNoSrc;
					uint32_t idsNoSrcCnt = 0;
					data.deps.clear();
					if (data.sortByFunc != nullptr)
						data.deps.add(DependencyHasSort);
					if (data.groupBy != EntityBad)
						data.deps.add(DependencyHasGroup);

					auto terms = data.terms_view();
					const auto cnt = (uint32_t)terms.size();
					GAIA_FOR(cnt) {
						const auto& term = terms[i];
						const auto id = term.id;
						hasPrefabTerms |= id == Prefab;
						const bool isDirectIsTerm = term.src == EntityBad && term.entTrav == EntityBad &&
																				!term_has_variables(term) && term.matchKind == QueryMatchKind::Semantic &&
																				id.pair() && id.id() == Is.id() && !is_wildcard(id.gen()) &&
																				!is_variable((EntityId)id.gen());
						const bool isAdjunctTerm =
								term.src == EntityBad && term.entTrav == EntityBad && !term_has_variables(term) &&
								((id.pair() && world_is_exclusive_dont_fragment_relation(*w, entity_from_id(*w, id.id()))) ||
								 (!id.pair() && world_is_sparse_dont_fragment_component(*w, id)));
						hasAdjunctTerms |= isAdjunctTerm || isDirectIsTerm;
					}

					GAIA_FOR(cnt) {
						const auto& term = terms[i];
						const auto id = term.id;
						const bool isDirectIsTerm = term.src == EntityBad && term.entTrav == EntityBad &&
																				!term_has_variables(term) && term.matchKind == QueryMatchKind::Semantic &&
																				id.pair() && id.id() == Is.id() && !is_wildcard(id.gen()) &&
																				!is_variable((EntityId)id.gen());
						const bool isAdjunctTerm =
								term.src == EntityBad && term.entTrav == EntityBad && !term_has_variables(term) &&
								((id.pair() && world_is_exclusive_dont_fragment_relation(*w, entity_from_id(*w, id.id()))) ||
								 (!id.pair() && world_is_sparse_dont_fragment_component(*w, id)));
						canDirectCreateArchetypeMatch &= term.op == QueryOpKind::All && term.src == EntityBad;
						if (id.pair() && (is_wildcard(id.id()) || is_wildcard(id.gen())))
							data.deps.add(DependencyHasWildcardTerms);
						const bool hasDynamicRelationUsage =
								term.entTrav != EntityBad || term.src != EntityBad || term_has_variables(term);
						if (id.pair() && hasDynamicRelationUsage && !is_wildcard(id.id()) && !is_variable((EntityId)id.id()))
							data.deps.add_rel(entity_from_id(*w, id.id()));
						if (term.entTrav != EntityBad) {
							data.deps.add_rel(term.entTrav);
							data.deps.add(DependencyHasTraversalTerms);
						}
						if (term.src != EntityBad) {
							hasSourceTerms = true;
							data.deps.add(DependencyHasSourceTerms);
							++data.deps.sourceTermCnt;
							if (!is_variable(term.src))
								data.deps.add_src_entity(term.src);
						}

						if (term_has_variables(term)) {
							hasVariableTerms = true;
							data.deps.add(DependencyHasVariableTerms);
							isComplex = true;
							continue;
						}

						if (isAdjunctTerm || isDirectIsTerm) {
							data.deps.add(DependencyHasAdjunctTerms);
							if (id.pair() && !is_wildcard(id.id()) && !is_variable((EntityId)id.id()))
								data.deps.add_rel(entity_from_id(*w, id.id()));
							continue;
						}

						if (hasAdjunctTerms && term.op == QueryOpKind::Or) {
							isComplex = true;
							continue;
						}

						// Source terms are evaluated separately by the VM.
						// They should not affect archetype-level query masks.
						if (term.src != EntityBad) {
							continue;
						}

						// ANY terms are not hard requirements and must not affect archetype prefilter masks.
						if (term.op != QueryOpKind::Any)
							idsNoSrc[idsNoSrcCnt++] = id;

						if (term.op == QueryOpKind::All || term.op == QueryOpKind::Or) {
							hasCreateSelector = true;
							data.deps.add(DependencyHasPositiveTerms);
							data.deps.createSelectors[data.deps.createSelectorCnt++] = id;
						} else if (term.op == QueryOpKind::Not) {
							data.deps.add(DependencyHasNegativeTerms);
							data.deps.exclusions[data.deps.exclusionCnt++] = id;
						} else if (term.op == QueryOpKind::Any) {
							data.deps.add(DependencyHasAnyTerms);
						}

						// Build the Is mask.
						// We will use it to identify entities with an Is relationship quickly.
						const bool allowSemanticIs = !(
								term.matchKind == QueryMatchKind::Direct && id.pair() && id.id() == Is.id() && !is_wildcard(id.gen()));
						if (!id.pair()) {
							const auto j = (uint32_t)i; // data.remapping[i];
							const auto has_as = allowSemanticIs ? (uint32_t)is_base(*w, id) : 0U;
							as_mask_0 |= (has_as << j);
						} else {
							const bool idIsWildcard = is_wildcard(id.id());
							const bool isGenWildcard = is_wildcard(id.gen());
							isComplex |= (idIsWildcard || isGenWildcard);

							if (!idIsWildcard) {
								const auto j = (uint32_t)i; // data.remapping[i];
								const auto e = entity_from_id(*w, id.id());
								const auto has_as = allowSemanticIs ? (uint32_t)is_base(*w, e) : 0U;
								as_mask_0 |= (has_as << j);
							}

							if (!isGenWildcard) {
								const auto j = (uint32_t)i; // data.remapping[i];
								const auto e = entity_from_id(*w, id.gen());
								const auto has_as = allowSemanticIs ? (uint32_t)is_base(*w, e) : 0U;
								as_mask_1 |= (has_as << j);
							}
						}
					}

					// Update the mask
					data.as_mask_0 = as_mask_0;
					data.as_mask_1 = as_mask_1;
					if (hasPrefabTerms)
						data.flags |= QueryCtx::QueryFlags::HasPrefabTerms;
					else
						data.flags &= ~QueryCtx::QueryFlags::HasPrefabTerms;

					if (hasSourceTerms)
						data.flags |= QueryCtx::QueryFlags::HasSourceTerms;
					else
						data.flags &= ~QueryCtx::QueryFlags::HasSourceTerms;

					if (hasVariableTerms)
						data.flags |= QueryCtx::QueryFlags::HasVariableTerms;
					else
						data.flags &= ~QueryCtx::QueryFlags::HasVariableTerms;

					if (hasSourceTerms || hasVariableTerms)
						data.cachePolicy = CachePolicy::Dynamic;
					else if (!hasAdjunctTerms && data.sortByFunc == nullptr && data.groupBy == EntityBad && hasCreateSelector)
						data.cachePolicy = CachePolicy::Immediate;
					else
						data.cachePolicy = CachePolicy::Lazy;

					data.createArchetypeMatchKind = data.cachePolicy == CachePolicy::Immediate && canDirectCreateArchetypeMatch
																							? CreateArchetypeMatchKind::DirectAllTerms
																							: CreateArchetypeMatchKind::Vm;

					// Traversed-source snapshot caching is only effective for traversed source terms.
					if (!data.deps.has(DependencyHasSourceTerms) || !data.deps.has(DependencyHasTraversalTerms))
						data.cacheSrcTrav = 0;

					// Calculate the component mask for simple queries
					isComplex |= ((data.as_mask_0 + data.as_mask_1) != 0);
					if (isComplex) {
						data.queryMask = {};
						data.flags |= QueryCtx::QueryFlags::Complex;
					} else {
						data.queryMask = build_entity_mask(EntitySpan{idsNoSrc.data(), idsNoSrcCnt});
						data.flags &= ~QueryCtx::QueryFlags::Complex;
					}
				}

				// Request recompilation of the query if the mask has changed
				if (mask0_old != data.as_mask_0 || mask1_old != data.as_mask_1 ||
						isComplex_old != (data.flags & QueryFlags::Complex) ||
						hasSourceTerms_old != (data.flags & QueryFlags::HasSourceTerms) ||
						hasVariableTerms_old != (data.flags & QueryFlags::HasVariableTerms) ||
						cachePolicy_old != data.cachePolicy || createArchetypeMatchKind_old != data.createArchetypeMatchKind ||
						dependencyFlags_old != data.deps.flags || createSelectorCnt_old != data.deps.createSelectorCnt ||
						exclusionCnt_old != data.deps.exclusionCnt || relationCnt_old != data.deps.relationCnt ||
						sourceEntityCnt_old != data.deps.sourceEntityCnt || sourceTermCnt_old != data.deps.sourceTermCnt ||
						createSelectors_old != data.deps.createSelectors || exclusions_old != data.deps.exclusions ||
						relations_old != data.deps.relations || sourceEntities_old != data.deps.sourceEntities)
					data.flags |= QueryCtx::QueryFlags::Recompile;
			}

			GAIA_NODISCARD bool operator==(const QueryCtx& other) const noexcept {
				// Comparison expected to be done only the first time the query is set up
				GAIA_ASSERT(q.handle.id() == QueryIdBad);
				// Fast path when cache ids are set
				// if (queryId != QueryIdBad && queryId == other.queryId)
				// 	return true;

				// Lookup hash must match
				if (hashLookup != other.hashLookup)
					return false;

				const auto& left = data;
				const auto& right = other.data;

				// Check array sizes first
				if (left.idsCnt != right.idsCnt)
					return false;
				if (left.changedCnt != right.changedCnt)
					return false;
				if (left.readWriteMask != right.readWriteMask)
					return false;
				if (left.cacheSrcTrav != right.cacheSrcTrav)
					return false;

				// Components need to be the same
				{
					const auto cnt = left.idsCnt;
					GAIA_FOR(cnt) {
						if (left._terms[i] != right._terms[i])
							return false;
					}
				}

				// Filters need to be the same
				{
					const auto cnt = left.changedCnt;
					GAIA_FOR(cnt) {
						if (left._changed[i] != right._changed[i])
							return false;
					}
				}

				// SortBy data need to match
				if (left.sortBy != right.sortBy)
					return false;
				if (left.sortByFunc != right.sortByFunc)
					return false;

				// Grouping data need to match
				if (left.groupBy != right.groupBy)
					return false;
				if (left.groupByFunc != right.groupByFunc)
					return false;

				return true;
			}

			GAIA_NODISCARD bool operator!=(const QueryCtx& other) const noexcept {
				return !operator==(other);
			}
		};

		//! Functor for sorting terms in a query before compilation
		struct query_sort_cond {
			constexpr bool operator()(const QueryTerm& lhs, const QueryTerm& rhs) const {
				// Smaller ops first.
				if (lhs.op != rhs.op)
					return lhs.op < rhs.op;

				// Smaller ids second.
				if (lhs.id != rhs.id)
					return SortComponentCond()(lhs.id, rhs.id);

				// Sources go last. Note, sources are never a pair.
				// We want to do it this way because it would be expensive to build cache for
				// the entire tree. Rather, we only cache fixed parts of the query without
				// variables.
				// TODO: In theory, there might be a better way to sort sources.
				//       E.g. depending on the number of archetypes we'd have to traverse
				//       it might be beneficial to do a different ordering which is impossible
				//       to do at this point.
				if (lhs.src != rhs.src)
					return SortComponentCond()(lhs.src, rhs.src);

				if (lhs.entTrav != rhs.entTrav)
					return SortComponentCond()(lhs.entTrav, rhs.entTrav);

				if (lhs.travKind != rhs.travKind)
					return (uint8_t)lhs.travKind < (uint8_t)rhs.travKind;

				if (lhs.travDepth != rhs.travDepth)
					return lhs.travDepth < rhs.travDepth;

				return (uint8_t)lhs.matchKind < (uint8_t)rhs.matchKind;
			}
		};

		//! Sorts internal component arrays
		inline void sort(QueryCtx& ctx) {
			const uint32_t idsCnt = ctx.data.idsCnt;
			const uint32_t changedCnt = ctx.data.changedCnt;

			auto& ctxData = ctx.data;
			auto remappingCopy = ctxData._remapping;

			// Canonicalize degenerate OR queries: a single OR term has AND semantics.
			// Rewriting it here keeps ordering/hash behavior identical to an explicit ALL term.
			if (idsCnt > 0) {
				uint32_t orCnt = 0;
				uint32_t orIdx = BadIndex;
				GAIA_FOR(idsCnt) {
					if (ctxData._terms[i].op != QueryOpKind::Or)
						continue;
					++orCnt;
					orIdx = i;
					if (orCnt > 1)
						break;
				}

				if (orCnt == 1)
					ctxData._terms[orIdx].op = QueryOpKind::All;
			}

			// Sort data. Necessary for correct hash calculation.
			// Without sorting query.all<XXX, YYY> would be different than query.all<YYY, XXX>.
			// Also makes sure data is in optimal order for query processing.
			core::sort(
					ctxData._terms.data(), ctxData._terms.data() + ctxData.idsCnt, query_sort_cond{}, //
					[&](uint32_t left, uint32_t right) {
						core::swap(ctxData._ids[left], ctxData._ids[right]);
						core::swap(ctxData._terms[left], ctxData._terms[right]);
						core::swap(remappingCopy[left], remappingCopy[right]);

						// Make sure masks remains correct after sorting
						core::swap_bits(ctxData.readWriteMask, left, right);
						core::swap_bits(ctxData.as_mask_0, left, right);
						core::swap_bits(ctxData.as_mask_1, left, right);
					});

			// Update remapping indices.
			// E.g., let us have ids 0, 14, 15, with indices 0, 1, 2.
			// After sorting they become 14, 15, 0, with indices 1, 2, 0.
			// So indices mapping is as follows: 0 -> 1, 1 -> 2, 2 -> 0.
			// After remapping update, indices become 0 -> 2, 1 -> 0, 2 -> 1.
			// Therefore, if we want to see where 15 was located originally (curr index 1), we do look at index 2 and get 1.

			GAIA_FOR(idsCnt) {
				const auto idxBeforeRemapping = (uint8_t)core::get_index_unsafe(remappingCopy, (uint8_t)i);
				ctxData._remapping[i] = idxBeforeRemapping;
			}

			if (idsCnt > 0) {
				uint32_t i = 0;
				while (i < idsCnt && ctxData._terms[i].op == QueryOpKind::All)
					++i;
				ctxData.firstOr = (uint8_t)i;
				while (i < idsCnt && ctxData._terms[i].op == QueryOpKind::Or)
					++i;
				ctxData.firstNot = (uint8_t)i;
				while (i < idsCnt && ctxData._terms[i].op == QueryOpKind::Not)
					++i;
				ctxData.firstAny = (uint8_t)i;
			} else
				ctxData.firstOr = ctxData.firstNot = ctxData.firstAny = 0;

			// Canonicalize filter order. This enables monotonic component lookup in filter matching
			// and keeps cache keys stable regardless of changed() call order.
			if (changedCnt > 1) {
				core::sort(ctxData._changed.data(), ctxData._changed.data() + changedCnt, SortComponentCond{});
			}
		}

		//! Traversed-source snapshot caching only has meaning for terms that use both a source and traversal.
		//! All other query shapes normalize the effective cap to 0 so shared cache identity does not diverge.
		inline void normalize_cache_src_trav(QueryCtx& ctx) {
			auto& ctxData = ctx.data;
			if (ctxData.cacheSrcTrav == 0)
				return;

			bool hasTraversedSourceTerm = false;
			for (const auto& term: ctxData.terms_view()) {
				if (term.src == EntityBad || term.entTrav == EntityBad)
					continue;

				hasTraversedSourceTerm = true;
				break;
			}

			if (!hasTraversedSourceTerm)
				ctxData.cacheSrcTrav = 0;
		}

		inline void calc_lookup_hash(QueryCtx& ctx) {
			GAIA_ASSERT(ctx.cc != nullptr);
			// Make sure we don't calculate the hash twice
			GAIA_ASSERT(ctx.hashLookup.hash == 0);

			QueryLookupHash::Type hashLookup = 0;

			const auto& ctxData = ctx.data;

			// Ids & ops
			{
				QueryLookupHash::Type hash = 0;

				auto terms = ctxData.terms_view();
				for (const auto& pair: terms) {
					hash = core::hash_combine(hash, (QueryLookupHash::Type)pair.op);
					hash = core::hash_combine(hash, (QueryLookupHash::Type)pair.id.value());
					hash = core::hash_combine(hash, (QueryLookupHash::Type)pair.src.value());
					hash = core::hash_combine(hash, (QueryLookupHash::Type)pair.entTrav.value());
					hash = core::hash_combine(hash, (QueryLookupHash::Type)(uint8_t)pair.travKind);
					hash = core::hash_combine(hash, (QueryLookupHash::Type)pair.travDepth);
					hash = core::hash_combine(hash, (QueryLookupHash::Type)(uint8_t)pair.matchKind);
				}
				hash = core::hash_combine(hash, (QueryLookupHash::Type)terms.size());
				hash = core::hash_combine(hash, (QueryLookupHash::Type)ctxData.readWriteMask);
				hash = core::hash_combine(hash, (QueryLookupHash::Type)ctxData.cacheSrcTrav);

				const bool matchPrefab = (ctxData.flags & QueryCtx::QueryFlags::MatchPrefab) != 0;
				hash = core::hash_combine(hash, (QueryLookupHash::Type)matchPrefab);

				hashLookup = hash;
			}

			// Filters
			{
				QueryLookupHash::Type hash = 0;

				auto changed = ctxData.changed_view();
				for (auto id: changed)
					hash = core::hash_combine(hash, (QueryLookupHash::Type)id.value());
				hash = core::hash_combine(hash, (QueryLookupHash::Type)changed.size());

				hashLookup = core::hash_combine(hashLookup, hash);
			}

			// Grouping
			{
				QueryLookupHash::Type hash = 0;

				hash = core::hash_combine(hash, (QueryLookupHash::Type)ctxData.groupBy.value());
				hash = core::hash_combine(hash, (QueryLookupHash::Type)ctxData.groupByFunc);

				hashLookup = core::hash_combine(hashLookup, hash);
			}

			ctx.hashLookup = {core::calculate_hash64(hashLookup)};
		}

		//! Finds the index at which the provided component id is located in the component array
		//! \param pTerms Pointer to the start of the terms array
		//! \param entity Entity we search for
		//! \param src Source entity
		//! \return Index of the component id in the array
		//! \warning The component id must be present in the array
		template <uint32_t MAX_COMPONENTS>
		GAIA_NODISCARD inline uint32_t comp_idx(const QueryTerm* pTerms, Entity entity, Entity src) {
			// We let the compiler know the upper iteration bound at compile-time.
			// This way it can optimize better (e.g. loop unrolling, vectorization).
			GAIA_FOR(MAX_COMPONENTS) {
				if (pTerms[i].id == entity && pTerms[i].src == src)
					return i;
			}

			GAIA_ASSERT(false);
			return BadIndex;
		}
	} // namespace ecs
} // namespace gaia
