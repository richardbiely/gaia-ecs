#pragma once
#include "../containers/sarray_ext.h"
#include "../utils/hashing_policy.h"
#include "component.h"
#include "component_utils.h"

namespace gaia {
	namespace ecs {
		namespace query {
			//! Number of components that can be a part of Query
			static constexpr uint32_t MAX_COMPONENTS_IN_QUERY = 8U;

			//! List type
			enum ListType : uint8_t { LT_None, LT_Any, LT_All, LT_Count };

			using QueryId = uint32_t;
			using LookupHash = utils::direct_hash_key<uint64_t>;
			using ComponentIdArray = containers::sarray_ext<component::ComponentId, MAX_COMPONENTS_IN_QUERY>;
			using ListTypeArray = containers::sarray_ext<ListType, MAX_COMPONENTS_IN_QUERY>;
			using ChangeFilterArray = containers::sarray_ext<component::ComponentId, MAX_COMPONENTS_IN_QUERY>;

			static constexpr QueryId QueryIdBad = (QueryId)-1;

			struct LookupCtx {
				//! Lookup hash for this query
				LookupHash hashLookup{};
				//! Query id
				QueryId queryId = QueryIdBad;

				struct Data {
					//! List of querried components
					ComponentIdArray componentIds;
					//! Filtering rules for the components
					ListTypeArray rules;
					//! List of component matcher hashes
					component::ComponentMatcherHash hash[ListType::LT_Count];
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

					for (size_t i = 0; i < component::ComponentType::CT_Count; ++i) {
						const auto& left = data[i];
						const auto& right = other.data[i];

						// Check array sizes first
						if (left.componentIds.size() != right.componentIds.size())
							return false;
						if (left.rules.size() != right.rules.size())
							return false;
						if (left.withChanged.size() != right.withChanged.size())
							return false;
						if (left.readWriteMask != right.readWriteMask)
							return false;

						// Matches hashes need to be the same
						for (size_t j = 0; j < ListType::LT_Count; ++j) {
							if (left.hash[j] != right.hash[j])
								return false;
						}

						// Components need to be the same
						for (size_t j = 0; j < left.componentIds.size(); ++j) {
							if (left.componentIds[j] != right.componentIds[j])
								return false;
						}

						// Rules need to be the same
						for (size_t j = 0; j < left.rules.size(); ++j) {
							if (left.rules[j] != right.rules[j])
								return false;
						}

						// Filters need to be the same
						for (size_t j = 0; j < left.withChanged.size(); ++j) {
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
			inline void SortComponentArrays(LookupCtx& ctx) {
				for (size_t i = 0; i < component::ComponentType::CT_Count; ++i) {
					auto& data = ctx.data[i];
					// Make sure the read-write mask remains correct after sorting
					utils::sort(data.componentIds, component::SortComponentCond{}, [&](size_t left, size_t right) {
						utils::swap(data.componentIds[left], data.componentIds[right]);
						utils::swap(data.rules[left], data.rules[right]);

						{
							// Swap the bits in the read-write mask
							const uint32_t b0 = data.readWriteMask & (1U << left);
							const uint32_t b1 = data.readWriteMask & (1U << right);
							// XOR the two bits
							const uint32_t bxor = (b0 ^ b1);
							// Put the XOR bits back to their original positions
							const uint32_t mask = (bxor << left) | (bxor << right);
							// XOR mask with the original one effectivelly swapping the bits
							data.readWriteMask = data.readWriteMask ^ (uint8_t)mask;
						}
					});
				}
			}

			inline void CalculateMatcherHashes(LookupCtx& ctx) {
				// Sort the arrays if necessary
				SortComponentArrays(ctx);

				// Calculate the matcher hash
				for (auto& data: ctx.data) {
					for (size_t i = 0; i < data.rules.size(); ++i)
						component::CalculateMatcherHash(data.hash[data.rules[i]], data.componentIds[i]);
				}
			}

			inline void CalculateLookupHash(LookupCtx& ctx) {
				// Make sure we don't calculate the hash twice
				GAIA_ASSERT(ctx.hashLookup.hash == 0);

				LookupHash::Type hashLookup = 0;

				for (size_t i = 0; i < component::ComponentType::CT_Count; ++i) {
					auto& data = ctx.data[i];

					// Components
					{
						LookupHash::Type hash = 0;

						const auto& componentIds = data.componentIds;
						for (const auto componentId: componentIds)
							hash = utils::hash_combine(hash, (LookupHash::Type)componentId);
						hash = utils::hash_combine(hash, (LookupHash::Type)componentIds.size());

						hash = utils::hash_combine(hash, (LookupHash::Type)data.readWriteMask);
						hashLookup = utils::hash_combine(hashLookup, hash);
					}

					// Rules
					{
						LookupHash::Type hash = 0;

						const auto& rules = data.withChanged;
						for (auto listType: rules)
							hash = utils::hash_combine(hash, (LookupHash::Type)listType);
						hash = utils::hash_combine(hash, (LookupHash::Type)rules.size());

						hashLookup = utils::hash_combine(hashLookup, hash);
					}

					// Filters
					{
						LookupHash::Type hash = 0;

						const auto& withChanged = data.withChanged;
						for (auto componentId: withChanged)
							hash = utils::hash_combine(hash, (LookupHash::Type)componentId);
						hash = utils::hash_combine(hash, (LookupHash::Type)withChanged.size());

						hashLookup = utils::hash_combine(hashLookup, hash);
					}
				}

				ctx.hashLookup = {utils::calculate_hash64(hashLookup)};
			}
		} // namespace query
	} // namespace ecs
} // namespace gaia