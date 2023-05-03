#pragma once
#include "../containers/sarray_ext.h"
#include "../utils/hashing_policy.h"
#include "component.h"
#include "component_utils.h"

namespace gaia {
	namespace ecs {
		namespace query {
			//! Number of components that can be a part of EntityQuery
			static constexpr uint32_t MAX_COMPONENTS_IN_QUERY = 8U;

			using LookupHash = utils::direct_hash_key<uint64_t>;
			//! Array of component ids
			using ComponentIdArray = containers::sarray_ext<ComponentId, MAX_COMPONENTS_IN_QUERY>;
			//! Array of component ids reserved for filtering
			using ChangeFilterArray = containers::sarray_ext<ComponentId, MAX_COMPONENTS_IN_QUERY>;

			//! List type
			enum ListType : uint8_t { LT_None, LT_Any, LT_All, LT_Count };

			struct ComponentListData {
				//! List of component ids
				ComponentIdArray componentIds[ListType::LT_Count]{};
				//! List of component matcher hashes
				ComponentMatcherHash hash[ListType::LT_Count]{};
			};

			struct LookupCtx {
				//! Lookup hash for this query
				LookupHash hashLookup{};
				//! Cache id
				uint32_t cacheId = (uint32_t)-1;
				//! List of querried components
				ComponentListData list[ComponentType::CT_Count]{};
				//! List of filtered components
				ChangeFilterArray listChangeFiltered[ComponentType::CT_Count]{};
				//! Read-write mask. Bit 0 stands for component 0 in component arrays.
				//! A set bit means write access is requested.
				uint8_t rw[ComponentType::CT_Count]{};
				static_assert(MAX_COMPONENTS_IN_QUERY == 8); // Make sure that MAX_COMPONENTS_IN_QUERY can fit into m_rw

				GAIA_NODISCARD bool operator==(const LookupCtx& other) const {
					// Fast path when cache ids are set
					if (cacheId != (uint32_t)-1 && cacheId == other.cacheId)
						return true;

					// Lookup hash must match
					if (hashLookup != other.hashLookup)
						return false;

					for (size_t j = 0; j < ComponentType::CT_Count; ++j) {
						// Read-write access has to be the same
						if (rw[j] != other.rw[j])
							return false;

						// Filter count needs to be the same
						if (listChangeFiltered[j].size() != other.listChangeFiltered[j].size())
							return false;

						const auto& queryList = list[j];
						const auto& otherList = other.list[j];

						// Component count needes to be the same
						for (size_t i = 0; i < ListType::LT_Count; ++i) {
							if (queryList.componentIds[i].size() != otherList.componentIds[i].size())
								return false;
						}

						// Matches hashes need to be the same
						for (size_t i = 0; i < ListType::LT_Count; ++i) {
							if (queryList.hash[i] != otherList.hash[i])
								return false;
						}

						// Components need to be the same
						for (size_t i = 0; i < ListType::LT_Count; ++i) {
							const auto ret = std::memcmp(
									(const void*)&queryList.componentIds[i], (const void*)&otherList.componentIds[i],
									queryList.componentIds[i].size() * sizeof(queryList.componentIds[0]));
							if (ret != 0)
								return false;
						}

						// Filters need to be the same
						{
							const auto ret = std::memcmp(
									(const void*)&listChangeFiltered[j], (const void*)&other.listChangeFiltered[j],
									listChangeFiltered[j].size() * sizeof(listChangeFiltered[0]));
							if (ret != 0)
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
			inline void SortComponentArrays(query::LookupCtx& ctx) {
				for (size_t i = 0; i < ComponentType::CT_Count; ++i) {
					auto& l = ctx.list[i];
					for (auto& componentIds: l.componentIds) {
						// Make sure the read-write mask remains correct after sorting
						utils::sort(componentIds, SortComponentCond{}, [&](size_t left, size_t right) {
							// Swap component ids
							utils::swap(componentIds[left], componentIds[right]);

							// Swap the bits in the read-write mask
							const uint32_t b0 = ctx.rw[i] & (1U << left);
							const uint32_t b1 = ctx.rw[i] & (1U << right);
							// XOR the two bits
							const uint32_t bxor = (b0 ^ b1);
							// Put the XOR bits back to their original positions
							const uint32_t mask = (bxor << left) | (bxor << right);
							// XOR mask with the original one effectivelly swapping the bits
							ctx.rw[i] = ctx.rw[i] ^ (uint8_t)mask;
						});
					}
				}
			}

			inline void CalculateMatcherHashes(query::LookupCtx& ctx) {
				// Sort the arrays if necessary
				SortComponentArrays(ctx);

				// Calculate the matcher hash
				for (auto& l: ctx.list) {
					for (size_t i = 0; i < query::ListType::LT_Count; ++i)
						l.hash[i] = CalculateMatcherHash(l.componentIds[i]);
				}
			}

			inline void CalculateLookupHash(query::LookupCtx& ctx) {
				// Make sure we don't calculate the hash twice
				GAIA_ASSERT(ctx.hashLookup.hash == 0);

				query::LookupHash::Type hashLookup = 0;

				// Filters
				for (size_t i = 0; i < ComponentType::CT_Count; ++i) {
					query::LookupHash::Type hash = 0;

					const auto& l = ctx.listChangeFiltered[i];
					for (auto componentId: l)
						hash = utils::hash_combine(hash, (query::LookupHash::Type)componentId);
					hash = utils::hash_combine(hash, (query::LookupHash::Type)l.size());

					hashLookup = utils::hash_combine(hashLookup, hash);
				}

				// Components
				for (size_t i = 0; i < ComponentType::CT_Count; ++i) {
					query::LookupHash::Type hash = 0;

					const auto& l = ctx.list[i];
					for (const auto& componentIds: l.componentIds) {
						for (const auto componentId: componentIds) {
							hash = utils::hash_combine(hash, (query::LookupHash::Type)componentId);
						}
						hash = utils::hash_combine(hash, (query::LookupHash::Type)componentIds.size());
					}

					hash = utils::hash_combine(hash, (query::LookupHash::Type)ctx.rw[i]);
					hashLookup = utils::hash_combine(hashLookup, hash);
				}

				ctx.hashLookup = {utils::calculate_hash64(hashLookup)};
			}
		} // namespace query
	} // namespace ecs
} // namespace gaia