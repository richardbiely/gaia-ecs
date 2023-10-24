#pragma once

#include <type_traits>

#include "../cnt/darray.h"
#include "../cnt/sarray_ext.h"
#include "../core/hashing_policy.h"
#include "component.h"
#include "component_utils.h"

namespace gaia {
	namespace ecs {
		namespace query {
			//! Number of components that can be a part of Query
			static constexpr uint32_t MAX_COMPONENTS_IN_QUERY = 8U;

			//! List type
			enum ListType : uint8_t { LT_None, LT_Any, LT_All, LT_Count };

			//! Exection mode
			enum class ExecutionMode : uint8_t {
				//! Run on the main thread
				Run,
				//! Run on a single worker thread
				Single,
				//! Run on as many worker threads as possible
				Parallel
			};

			using QueryId = uint32_t;
			using LookupHash = core::direct_hash_key<uint64_t>;
			using ComponentIdArray = cnt::sarray_ext<component::ComponentId, MAX_COMPONENTS_IN_QUERY>;
			using ListTypeArray = cnt::sarray_ext<ListType, MAX_COMPONENTS_IN_QUERY>;
			using ChangeFilterArray = cnt::sarray_ext<component::ComponentId, MAX_COMPONENTS_IN_QUERY>;

			static constexpr QueryId QueryIdBad = (QueryId)-1;

			struct LookupCtx {
				//! Lookup hash for this query
				LookupHash hashLookup{};
				//! Query id
				QueryId queryId = QueryIdBad;

				struct Data {
					//! List of querried components
					ComponentIdArray compIds;
					//! Filtering rules for the components
					ListTypeArray rules;
					//! List of component matcher hashes
					component::ComponentMatcherHash hash[ListType::LT_Count];
					//! Array of indiices to the last checked archetype in the component-to-archetype map
					cnt::darray<uint32_t> lastMatchedArchetypeIndex;
					//! List of filtered components
					ChangeFilterArray withChanged;
					//! Read-write mask. Bit 0 stands for component 0 in component arrays.
					//! A set bit means write access is requested.
					uint8_t readWriteMask;
					//! The number of components which are required for the query to match
					uint8_t rulesAllCount;
				} data[component::ComponentType::CT_Count]{};
				static_assert(MAX_COMPONENTS_IN_QUERY == 8); // Make sure that MAX_COMPONENTS_IN_QUERY can fit into m_rw

				GAIA_NODISCARD bool operator==(const LookupCtx& other) const {
					// Fast path when cache ids are set
					if (queryId != QueryIdBad && queryId == other.queryId)
						return true;

					// Lookup hash must match
					if (hashLookup != other.hashLookup)
						return false;

					for (uint32_t i = 0; i < component::ComponentType::CT_Count; ++i) {
						const auto& left = data[i];
						const auto& right = other.data[i];

						// Check array sizes first
						if (left.compIds.size() != right.compIds.size())
							return false;
						if (left.rules.size() != right.rules.size())
							return false;
						if (left.withChanged.size() != right.withChanged.size())
							return false;
						if (left.readWriteMask != right.readWriteMask)
							return false;

						// Matches hashes need to be the same
						for (uint32_t j = 0; j < ListType::LT_Count; ++j) {
							if (left.hash[j] != right.hash[j])
								return false;
						}

						// Components need to be the same
						for (uint32_t j = 0; j < left.compIds.size(); ++j) {
							if (left.compIds[j] != right.compIds[j])
								return false;
						}

						// Rules need to be the same
						for (uint32_t j = 0; j < left.rules.size(); ++j) {
							if (left.rules[j] != right.rules[j])
								return false;
						}

						// Filters need to be the same
						for (uint32_t j = 0; j < left.withChanged.size(); ++j) {
							if (left.withChanged[j] != right.withChanged[j])
								return false;
						}
					}

					return true;
				}

				GAIA_NODISCARD bool operator!=(const LookupCtx& other) const {
					return !operator==(other);
				};
			};

			//! Sorts internal component arrays
			inline void sort(LookupCtx& ctx) {
				for (uint32_t i = 0; i < component::ComponentType::CT_Count; ++i) {
					auto& data = ctx.data[i];
					// Make sure the read-write mask remains correct after sorting
					core::sort(data.compIds, component::SortComponentCond{}, [&](uint32_t left, uint32_t right) {
						core::swap(data.compIds[left], data.compIds[right]);
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

			inline void matcher_hashes(LookupCtx& ctx) {
				// Sort the arrays if necessary
				sort(ctx);

				// Calculate the matcher hash
				for (auto& data: ctx.data) {
					for (uint32_t i = 0; i < data.rules.size(); ++i)
						component::matcher_hash(data.hash[data.rules[i]], data.compIds[i]);
				}
			}

			inline void calc_lookup_hash(LookupCtx& ctx) {
				// Make sure we don't calculate the hash twice
				GAIA_ASSERT(ctx.hashLookup.hash == 0);

				LookupHash::Type hashLookup = 0;

				for (uint32_t i = 0; i < component::ComponentType::CT_Count; ++i) {
					auto& data = ctx.data[i];

					// Components
					{
						LookupHash::Type hash = 0;

						const auto& compIds = data.compIds;
						for (const auto componentId: compIds)
							hash = core::hash_combine(hash, (LookupHash::Type)componentId);
						hash = core::hash_combine(hash, (LookupHash::Type)compIds.size());

						hash = core::hash_combine(hash, (LookupHash::Type)data.readWriteMask);
						hashLookup = core::hash_combine(hashLookup, hash);
					}

					// Rules
					{
						LookupHash::Type hash = 0;

						const auto& rules = data.withChanged;
						for (auto listType: rules)
							hash = core::hash_combine(hash, (LookupHash::Type)listType);
						hash = core::hash_combine(hash, (LookupHash::Type)rules.size());

						hashLookup = core::hash_combine(hashLookup, hash);
					}

					// Filters
					{
						LookupHash::Type hash = 0;

						const auto& withChanged = data.withChanged;
						for (auto componentId: withChanged)
							hash = core::hash_combine(hash, (LookupHash::Type)componentId);
						hash = core::hash_combine(hash, (LookupHash::Type)withChanged.size());

						hashLookup = core::hash_combine(hashLookup, hash);
					}
				}

				ctx.hashLookup = {core::calculate_hash64(hashLookup)};
			}
		} // namespace query
	} // namespace ecs
} // namespace gaia