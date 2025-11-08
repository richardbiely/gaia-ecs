#pragma once
#include "gaia/config/config.h"

#include <type_traits>

#include "api.h"
#include "component.h"
#include "gaia/cnt/darray.h"
#include "gaia/cnt/map.h"
#include "gaia/cnt/sarray.h"
#include "gaia/cnt/sarray_ext.h"
#include "gaia/core/bit_utils.h"
#include "gaia/core/hashing_policy.h"
#include "gaia/core/utility.h"
#include "id.h"
#include "query_fwd.h"
#include "query_mask.h"
#include "ser_binary.h"

namespace gaia {
	namespace ecs {
		class World;
		class Archetype;

		//! Number of items that can be a part of Query
		static constexpr uint32_t MAX_ITEMS_IN_QUERY = 8U;

		GAIA_GCC_WARNING_PUSH()
		// GCC is unnecessarily too strict about shadowing.
		// We have a globally defined entity All and thinks QueryOpKind::All shadows it.
		GAIA_GCC_WARNING_DISABLE("-Wshadow")

		//! Operation type
		enum class QueryOpKind : uint8_t { All, Any, Not, Count };
		//! Access type
		enum class QueryAccess : uint8_t { None, Read, Write };
		//! Operation flags
		enum class QueryInputFlags : uint8_t { None, Variable };

		GAIA_GCC_WARNING_POP()

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
			//! Operation to perform with the input
			QueryOpKind op = QueryOpKind::All;
			//! Access type
			QueryAccess access = QueryAccess::Read;
			//! Entity/Component/Pair to query
			Entity id;
			//! Source entity to query the id on.
			//! If id==EntityBad the source is fixed.
			//! If id!=src the source is variable.
			Entity src = EntityBad;
		};

		//! Internal representation of QueryInput
		struct QueryTerm {
			//! Queried id
			Entity id;
			//! Source of where the queried id is looked up at
			Entity src;
			//! Archetype of the src entity
			Archetype* srcArchetype;
			//! Operation to perform with the term
			QueryOpKind op;

			bool operator==(const QueryTerm& other) const {
				return id == other.id && src == other.src && op == other.op;
			}
			bool operator!=(const QueryTerm& other) const {
				return !operator==(other);
			}
		};

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
			};

			struct Data {
				//! Array of queried ids
				QueryEntityArray _ids;
				//! Array of terms
				cnt::sarray<QueryTerm, MAX_ITEMS_IN_QUERY> _terms;
				//! Mapping of the original indices to the new ones after sorting
				cnt::sarray<uint8_t, MAX_ITEMS_IN_QUERY> _remapping;
				//! Index of the last checked archetype in the component-to-archetype map
				QueryArchetypeCacheIndexMap lastMatchedArchetypeIdx_All;
				QueryArchetypeCacheIndexMap lastMatchedArchetypeIdx_Any;
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
				//! Read-write mask. Bit 0 stands for component 0 in component arrays.
				//! A set bit means write access is requested.
				uint8_t readWriteMask;
				//! Query flags
				uint8_t flags;

				GAIA_NODISCARD std::span<Entity> ids_view() const {
					return {_ids.data(), idsCnt};
				}

				GAIA_NODISCARD std::span<QueryTerm> terms_view() const {
					return {_terms.data(), idsCnt};
				}

				GAIA_NODISCARD std::span<Entity> changed_view() const {
					return {_changed.data(), changedCnt};
				}
			} data{};
			// Make sure that MAX_ITEMS_IN_QUERY can fit into data.readWriteMask
			static_assert(MAX_ITEMS_IN_QUERY == 8);

			void init(World* pWorld) {
				w = pWorld;
				cc = &comp_cache_mut(*pWorld);
			}

			void refresh() {
				const auto mask0_old = data.as_mask_0;
				const auto mask1_old = data.as_mask_1;
				const auto isComplex_old = data.flags & QueryFlags::Complex;

				// Update masks
				{
					uint32_t as_mask_0 = 0;
					uint32_t as_mask_1 = 0;
					bool isComplex = false;

					auto ids = data.ids_view();
					const auto cnt = (uint32_t)ids.size();
					GAIA_FOR(cnt) {
						const auto id = ids[i];

						// Build the Is mask.
						// We will use it to identify entities with an Is relationship quickly.
						if (!id.pair()) {
							const auto j = (uint32_t)i; // data.remapping[i];
							const auto has_as = (uint32_t)is_base(*w, id);
							as_mask_0 |= (has_as << j);
						} else {
							const bool idIsWildcard = is_wildcard(id.id());
							const bool isGenWildcard = is_wildcard(id.gen());
							isComplex |= (idIsWildcard || isGenWildcard);

							if (!idIsWildcard) {
								const auto j = (uint32_t)i; // data.remapping[i];
								const auto e = entity_from_id(*w, id.id());
								const auto has_as = (uint32_t)is_base(*w, e);
								as_mask_0 |= (has_as << j);
							}

							if (!isGenWildcard) {
								const auto j = (uint32_t)i; // data.remapping[i];
								const auto e = entity_from_id(*w, id.gen());
								const auto has_as = (uint32_t)is_base(*w, e);
								as_mask_1 |= (has_as << j);
							}
						}
					}

					// Update the mask
					data.as_mask_0 = as_mask_0;
					data.as_mask_1 = as_mask_1;

					// Calculate the component mask for simple queries
					isComplex |= ((data.as_mask_0 + data.as_mask_1) != 0);
					if (isComplex) {
						data.queryMask = {};
						data.flags |= QueryCtx::QueryFlags::Complex;
					} else {
						data.queryMask = build_entity_mask(ids);
						data.flags &= ~QueryCtx::QueryFlags::Complex;
					}
				}

				// Request recompilation of the query if the mask has changed
				if (mask0_old != data.as_mask_0 || mask1_old != data.as_mask_1 ||
						isComplex_old != (data.flags & QueryFlags::Complex))
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
				return SortComponentCond()(lhs.src, rhs.src);
			}
		};

		//! Sorts internal component arrays
		inline void sort(QueryCtx& ctx) {
			const uint32_t idsCnt = ctx.data.idsCnt;

			auto& ctxData = ctx.data;
			auto remappingCopy = ctxData._remapping;

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
				ctxData.firstAny = (uint8_t)i;
				while (i < idsCnt && ctxData._terms[i].op == QueryOpKind::Any)
					++i;
				ctxData.firstNot = (uint8_t)i;
			}
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
				}
				hash = core::hash_combine(hash, (QueryLookupHash::Type)terms.size());
				hash = core::hash_combine(hash, (QueryLookupHash::Type)ctxData.readWriteMask);

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
		//! \param pComps Pointer to the start of the component array
		//! \param entity Entity we search for
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