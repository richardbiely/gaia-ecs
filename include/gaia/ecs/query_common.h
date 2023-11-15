#pragma once

#include <type_traits>

#include "../cnt/darray.h"
#include "../cnt/sarray_ext.h"
#include "../core/hashing_policy.h"
#include "component.h"
#include "component_utils.h"

namespace gaia {
	namespace ecs {
		//! Number of components that can be a part of Query
		static constexpr uint32_t MAX_COMPONENTS_IN_QUERY = 8U;

		//! List type
		enum QueryListType : uint8_t { LT_None, LT_Any, LT_All, LT_Count };

		using QueryId = uint32_t;
		using QueryLookupHash = core::direct_hash_key<uint64_t>;
		using QueryComponentArray = cnt::sarray_ext<Component, MAX_COMPONENTS_IN_QUERY>;
		using QueryListTypeArray = cnt::sarray_ext<QueryListType, MAX_COMPONENTS_IN_QUERY>;
		using QueryChangeArray = cnt::sarray_ext<Component, MAX_COMPONENTS_IN_QUERY>;

		static constexpr QueryId QueryIdBad = (QueryId)-1;

		struct QueryCtx {
			//! Lookup hash for this query
			QueryLookupHash hashLookup{};
			//! Query id
			QueryId queryId = QueryIdBad;

			struct Data {
				//! List of querried components
				QueryComponentArray comps;
				//! Filtering rules for the components
				QueryListTypeArray rules;
				//! List of component matcher hashes
				ComponentMatcherHash hash[QueryListType::LT_Count];
				//! Array of indices to the last checked archetype in the component-to-archetype map
				cnt::darray<uint32_t> lastMatchedArchetypeIdx;
				//! List of filtered components
				QueryChangeArray withChanged;
				//! Read-write mask. Bit 0 stands for component 0 in component arrays.
				//! A set bit means write access is requested.
				uint8_t readWriteMask;
				//! The number of components which are required for the query to match
				uint8_t rulesAllCount;
			} data[ComponentKind::CK_Count]{};
			static_assert(MAX_COMPONENTS_IN_QUERY == 8); // Make sure that MAX_COMPONENTS_IN_QUERY can fit into m_rw

			GAIA_NODISCARD bool operator==(const QueryCtx& other) const {
				// Fast path when cache ids are set
				if (queryId != QueryIdBad && queryId == other.queryId)
					return true;

				// Lookup hash must match
				if (hashLookup != other.hashLookup)
					return false;

				GAIA_FOR(ComponentKind::CK_Count) {
					const auto& left = data[i];
					const auto& right = other.data[i];

					// Check array sizes first
					if (left.comps.size() != right.comps.size())
						return false;
					if (left.rules.size() != right.rules.size())
						return false;
					if (left.withChanged.size() != right.withChanged.size())
						return false;
					if (left.readWriteMask != right.readWriteMask)
						return false;

					// Matches hashes need to be the same
					GAIA_FOR_(QueryListType::LT_Count, j) {
						if (left.hash[j] != right.hash[j])
							return false;
					}

					// Components need to be the same
					GAIA_EACH_(left.comps, j) {
						if (left.comps[j] != right.comps[j])
							return false;
					}

					// Rules need to be the same
					GAIA_EACH_(left.rules, j) {
						if (left.rules[j] != right.rules[j])
							return false;
					}

					// Filters need to be the same
					GAIA_EACH_(left.withChanged, j) {
						if (left.withChanged[j] != right.withChanged[j])
							return false;
					}
				}

				return true;
			}

			GAIA_NODISCARD bool operator!=(const QueryCtx& other) const {
				return !operator==(other);
			};
		};

		//! Sorts internal component arrays
		inline void sort(QueryCtx& ctx) {
			GAIA_FOR(ComponentKind::CK_Count) {
				auto& data = ctx.data[i];
				// Make sure the read-write mask remains correct after sorting
				core::sort(data.comps, SortComponentCond{}, [&](uint32_t left, uint32_t right) {
					core::swap(data.comps[left], data.comps[right]);
					core::swap(data.rules[left], data.rules[right]);

					{
						// Swap the bits in the read-write mask
						const uint32_t b0 = (data.readWriteMask >> left) & 1U;
						const uint32_t b1 = (data.readWriteMask >> right) & 1U;
						// XOR the two bits
						const uint32_t bxor = b0 ^ b1;
						// Put the XOR bits back to their original positions
						const uint32_t mask = (bxor << left) | (bxor << right);
						// XOR mask with the original one effectivelly swapping the bits
						data.readWriteMask = data.readWriteMask ^ (uint8_t)mask;
					}
				});
			}
		}

		inline void matcher_hashes(QueryCtx& ctx) {
			// Sort the arrays if necessary
			sort(ctx);

			// Calculate the matcher hash
			for (auto& data: ctx.data) {
				GAIA_EACH(data.rules) matcher_hash(data.hash[data.rules[i]], data.comps[i]);
			}
		}

		inline void calc_lookup_hash(QueryCtx& ctx) {
			// Make sure we don't calculate the hash twice
			GAIA_ASSERT(ctx.hashLookup.hash == 0);

			QueryLookupHash::Type hashLookup = 0;

			GAIA_FOR(ComponentKind::CK_Count) {
				auto& data = ctx.data[i];

				// Components
				{
					QueryLookupHash::Type hash = 0;

					const auto& comps = data.comps;
					for (const auto comp: comps)
						hash = core::hash_combine(hash, (QueryLookupHash::Type)comp.id());
					hash = core::hash_combine(hash, (QueryLookupHash::Type)comps.size());

					hash = core::hash_combine(hash, (QueryLookupHash::Type)data.readWriteMask);
					hashLookup = core::hash_combine(hashLookup, hash);
				}

				// Rules
				{
					QueryLookupHash::Type hash = 0;

					const auto& rules = data.rules;
					for (auto listType: rules)
						hash = core::hash_combine(hash, (QueryLookupHash::Type)listType);
					hash = core::hash_combine(hash, (QueryLookupHash::Type)rules.size());

					hashLookup = core::hash_combine(hashLookup, hash);
				}

				// Filters
				{
					QueryLookupHash::Type hash = 0;

					const auto& withChanged = data.withChanged;
					for (auto comp: withChanged)
						hash = core::hash_combine(hash, (QueryLookupHash::Type)comp.id());
					hash = core::hash_combine(hash, (QueryLookupHash::Type)withChanged.size());

					hashLookup = core::hash_combine(hashLookup, hash);
				}
			}

			ctx.hashLookup = {core::calculate_hash64(hashLookup)};
		}
	} // namespace ecs
} // namespace gaia