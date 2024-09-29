#pragma once

#include <type_traits>

#include "../cnt/darray.h"
#include "../cnt/map.h"
#include "../cnt/sarray.h"
#include "../cnt/sarray_ext.h"
#include "../core/bit_utils.h"
#include "../core/hashing_policy.h"
#include "../core/utility.h"
#include "component.h"
#include "data_buffer.h"
#include "id.h"

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

		using QueryId = uint32_t;
		using GroupId = uint32_t;
		using QueryLookupHash = core::direct_hash_key<uint64_t>;
		using QueryEntityArray = cnt::sarray_ext<Entity, MAX_ITEMS_IN_QUERY>;
		using QueryArchetypeCacheIndexMap = cnt::map<EntityLookupKey, uint32_t>;
		using QueryOpArray = cnt::sarray_ext<QueryOpKind, MAX_ITEMS_IN_QUERY>;
		using QuerySerBuffer = SerializationBufferDyn;
		using QuerySerMap = cnt::map<QueryId, QuerySerBuffer>;
		using TGroupByFunc = GroupId (*)(const World&, const Archetype&, Entity);

		static constexpr QueryId QueryIdBad = (QueryId)-1;
		static constexpr GroupId GroupIdMax = ((GroupId)-1) - 1;

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

		QuerySerBuffer& query_buffer(World& world, QueryId& serId);
		void query_buffer_reset(World& world, QueryId& serId);
		ComponentCache& comp_cache_mut(World& world);

		struct QueryIdentity {
			//! Query id
			QueryId queryId = QueryIdBad;
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
			QueryIdentity q;

			enum QueryFlags : uint8_t { //
				SortGroups = 0x01
			};

			struct Data {
				//! Array of queried ids
				QueryEntityArray ids;
				//! Array of terms
				QueryTermArray terms;
				//! Index of the last checked archetype in the component-to-archetype map
				QueryArchetypeCacheIndexMap lastMatchedArchetypeIdx_All;
				QueryArchetypeCacheIndexMap lastMatchedArchetypeIdx_Any;
				//! Mapping of the original indices to the new ones after sorting
				QueryRemappingArray remapping;
				//! Array of filtered components
				QueryEntityArray changed;
				//! Entity to group the archetypes by. EntityBad for no grouping.
				Entity groupBy;
				//! Iteration will be restricted only to target Group
				GroupId groupIdSet;
				//! Function to use to perform the grouping
				TGroupByFunc groupByFunc;
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
			} data{};
			// Make sure that MAX_ITEMS_IN_QUERY can fit into data.readWriteMask
			static_assert(MAX_ITEMS_IN_QUERY == 8);

			void init(World* pWorld) {
				w = pWorld;
				cc = &comp_cache_mut(*pWorld);
			}

			GAIA_NODISCARD bool operator==(const QueryCtx& other) const {
				// Comparison expected to be done only the first time the query is set up
				GAIA_ASSERT(q.queryId == QueryIdBad);
				// Fast path when cache ids are set
				// if (queryId != QueryIdBad && queryId == other.queryId)
				// 	return true;

				// Lookup hash must match
				if (hashLookup != other.hashLookup)
					return false;

				const auto& left = data;
				const auto& right = other.data;

				// Check array sizes first
				if (left.terms.size() != right.terms.size())
					return false;
				if (left.changed.size() != right.changed.size())
					return false;
				if (left.readWriteMask != right.readWriteMask)
					return false;

				// Components need to be the same
				if (left.terms != right.terms)
					return false;

				// Filters need to be the same
				if (left.changed != right.changed)
					return false;

				// Grouping data need to match
				if (left.groupBy != right.groupBy)
					return false;
				if (left.groupByFunc != right.groupByFunc)
					return false;

				return true;
			}

			GAIA_NODISCARD bool operator!=(const QueryCtx& other) const {
				return !operator==(other);
			};
		};

		//! Functor for sorting terms in a query before compilation
		struct query_sort_cond {
			constexpr bool operator()(const QueryTerm& lhs, const QueryTerm& rhs) const {
				// Smaller ops first.
				if (lhs.op != rhs.op)
					return lhs.op < rhs.op;

				// Smaller ids second.
				if (lhs.id != rhs.id)
					return lhs.id < rhs.id;

				// Sources go last. Note, sources are never a pair.
				// We want to do it this way because it would be expensive to build cache for
				// the entire tree. Rather, we only cache fixed parts of the query without
				// variables.
				// TODO: In theory, there might be a better way to sort sources.
				//       E.g. depending on the number of archetypes we'd have to traverse
				//       it might be beneficial to do a different ordering which is impossible
				//       to do at this point.
				return lhs.src.id() < rhs.src.id();
			}
		};

		//! Sorts internal component arrays
		inline void sort(QueryCtx& ctx) {
			auto& data = ctx.data;

			auto remappingCopy = data.remapping;

			// Sort data. Necessary for correct hash calculation.
			// Without sorting query.all<XXX, YYY> would be different than query.all<YYY, XXX>.
			// Also makes sure data is in optimal order for query processing.
			core::sort(data.terms, query_sort_cond{}, [&](uint32_t left, uint32_t right) {
				core::swap(data.ids[left], data.ids[right]);
				core::swap(data.terms[left], data.terms[right]);
				core::swap(remappingCopy[left], remappingCopy[right]);

				// Make sure masks remains correct after sorting
				core::swap_bits(data.readWriteMask, left, right);
				core::swap_bits(data.as_mask_0, left, right);
				core::swap_bits(data.as_mask_1, left, right);
			});

			// Update remapping indices.
			// E.g., let us have ids 0, 14, 15, with indices 0, 1, 2.
			// After sorting they become 14, 15, 0, with indices 1, 2, 0.
			// So indices mapping is as follows: 0 -> 1, 1 -> 2, 2 -> 0.
			// After remapping update, indices become 0 -> 2, 1 -> 0, 2 -> 1.
			// Therefore, if we want to see where 15 was located originally (curr index 1), we do look at index 2 and get 1.
			GAIA_EACH(data.terms) {
				const auto idxBeforeRemapping = (uint8_t)core::get_index_unsafe(remappingCopy, (uint8_t)i);
				data.remapping[i] = idxBeforeRemapping;
			}

			const auto& terms = data.terms;
			if (!terms.empty()) {
				uint32_t i = 0;
				while (i < terms.size() && terms[i].op == QueryOpKind::All)
					++i;
				data.firstAny = (uint8_t)i;
				while (i < terms.size() && terms[i].op == QueryOpKind::Any)
					++i;
				data.firstNot = (uint8_t)i;
			}
		}

		inline void calc_lookup_hash(QueryCtx& ctx) {
			GAIA_ASSERT(ctx.cc != nullptr);
			// Make sure we don't calculate the hash twice
			GAIA_ASSERT(ctx.hashLookup.hash == 0);

			QueryLookupHash::Type hashLookup = 0;

			auto& data = ctx.data;

			// Ids & ops
			{
				QueryLookupHash::Type hash = 0;

				const auto& terms = data.terms;
				for (auto pair: terms) {
					hash = core::hash_combine(hash, (QueryLookupHash::Type)pair.op);
					hash = core::hash_combine(hash, (QueryLookupHash::Type)pair.id.value());
				}
				hash = core::hash_combine(hash, (QueryLookupHash::Type)terms.size());
				hash = core::hash_combine(hash, (QueryLookupHash::Type)data.readWriteMask);

				hashLookup = hash;
			}

			// Filters
			{
				QueryLookupHash::Type hash = 0;

				const auto& changed = data.changed;
				for (auto id: changed)
					hash = core::hash_combine(hash, (QueryLookupHash::Type)id.value());
				hash = core::hash_combine(hash, (QueryLookupHash::Type)changed.size());

				hashLookup = core::hash_combine(hashLookup, hash);
			}

			// Grouping
			{
				QueryLookupHash::Type hash = 0;

				hash = core::hash_combine(hash, (QueryLookupHash::Type)data.groupBy.value());
				hash = core::hash_combine(hash, (QueryLookupHash::Type)data.groupByFunc);

				hashLookup = core::hash_combine(hashLookup, hash);
			}

			ctx.hashLookup = {core::calculate_hash64(hashLookup)};
		}

		//! Located the index at which the provided component id is located in the component array
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