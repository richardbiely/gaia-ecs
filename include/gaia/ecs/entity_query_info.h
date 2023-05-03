#pragma once
#include "../containers/darray.h"
#include "../containers/sarray_ext.h"
#include "../utils/hashing_policy.h"
#include "../utils/utility.h"
#include "component.h"
#include "component_utils.h"
#include "gaia/ecs/fwd.h"

namespace gaia {
	namespace ecs {
		struct Entity;
		class Archetype;

		const ComponentIdList& GetArchetypeComponentInfoList(const Archetype& archetype, ComponentType componentType);
		ComponentMatcherHash GetArchetypeMatcherHash(const Archetype& archetype, ComponentType componentType);

		class EntityQueryInfo {
			friend class World;

		public:
			//! Number of components that can be a part of EntityQuery
			static constexpr uint32_t MAX_COMPONENTS_IN_QUERY = 8U;

			using LookupHash = utils::direct_hash_key<uint64_t>;
			//! Array of component ids
			using ComponentIdArray = containers::sarray_ext<ComponentId, MAX_COMPONENTS_IN_QUERY>;
			//! Array of component ids reserved for filtering
			using ChangeFilterArray = containers::sarray_ext<ComponentId, MAX_COMPONENTS_IN_QUERY>;

			//! List type
			enum ListType : uint8_t { LT_None, LT_Any, LT_All, LT_Count };
			//! Query matching result
			enum class MatchArchetypeQueryRet : uint8_t { Fail, Ok, Skip };

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
				static_assert(
						EntityQueryInfo::MAX_COMPONENTS_IN_QUERY == 8); // Make sure that MAX_COMPONENTS_IN_QUERY can fit into m_rw

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

		private:
			//! Lookup context
			LookupCtx m_lookupCtx;
			//! List of archetypes matching the query
			containers::darray<Archetype*> m_archetypeCache;
			//! Entity of the last added archetype in the world this query remembers
			uint32_t m_lastArchetypeId = 1; // skip the root archetype
			//! Version of the world for which the query has been called most recently
			uint32_t m_worldVersion = 0;

			void SetWorldVersion(uint32_t version) {
				m_worldVersion = version;
			}

			GAIA_NODISCARD uint32_t GetWorldVersion() const {
				return m_worldVersion;
			}

			template <typename T>
			bool HasComponent_Internal(
					[[maybe_unused]] ListType listType, [[maybe_unused]] ComponentType componentType, bool isReadWrite) const {
				if constexpr (std::is_same_v<T, Entity>) {
					// Skip Entity input args
					return true;
				} else {
					const ComponentIdArray& componentIds = m_lookupCtx.list[componentType].componentIds[listType];

					// Component id has to be present
					const auto componentId = GetComponentId<T>();
					const auto idx = utils::get_index(componentIds, componentId);
					if (idx == BadIndex)
						return false;

					// Read-write mask must match
					const uint8_t maskRW = (uint32_t)m_lookupCtx.rw[componentType] & (1U << (uint32_t)idx);
					const uint8_t maskXX = (uint32_t)isReadWrite << idx;
					return maskRW == maskXX;
				}
			}

		public:
			static GAIA_NODISCARD EntityQueryInfo Create(LookupCtx&& ctx) {
				EntityQueryInfo info;
				EntityQueryInfo::CalculateMatcherHashes(ctx);
				info.m_lookupCtx = std::move(ctx);
				return info;
			}

			void Init(uint32_t id) {
				GAIA_ASSERT(m_lookupCtx.cacheId == (uint32_t)-1);
				m_lookupCtx.cacheId = id;
			}

			EntityQueryInfo::LookupHash GetLookupHash() const {
				return m_lookupCtx.hashLookup;
			}

			uint32_t GetCacheId() const {
				return m_lookupCtx.cacheId;
			}

			//! Sorts internal component arrays
			static void SortComponentArrays(LookupCtx& ctx) {
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

			static void CalculateMatcherHashes(LookupCtx& ctx) {
				// Sort the arrays if necessary
				SortComponentArrays(ctx);

				// Calculate the matcher hash
				for (auto& l: ctx.list) {
					for (size_t i = 0; i < ListType::LT_Count; ++i)
						l.hash[i] = CalculateMatcherHash(l.componentIds[i]);
				}
			}

			static void CalculateLookupHash(LookupCtx& ctx) {
				// Make sure we don't calculate the hash twice
				GAIA_ASSERT(ctx.hashLookup.hash == 0);

				LookupHash::Type hashLookup = 0;

				// Filters
				for (size_t i = 0; i < ComponentType::CT_Count; ++i) {
					LookupHash::Type hash = 0;

					const auto& l = ctx.listChangeFiltered[i];
					for (auto componentId: l)
						hash = utils::hash_combine(hash, (LookupHash::Type)componentId);
					hash = utils::hash_combine(hash, (LookupHash::Type)l.size());

					hashLookup = utils::hash_combine(hashLookup, hash);
				}

				// Components
				for (size_t i = 0; i < ComponentType::CT_Count; ++i) {
					LookupHash::Type hash = 0;

					const auto& l = ctx.list[i];
					for (const auto& componentIds: l.componentIds) {
						for (const auto componentId: componentIds) {
							hash = utils::hash_combine(hash, (LookupHash::Type)componentId);
						}
						hash = utils::hash_combine(hash, (LookupHash::Type)componentIds.size());
					}

					hash = utils::hash_combine(hash, (LookupHash::Type)ctx.rw[i]);
					hashLookup = utils::hash_combine(hashLookup, hash);
				}

				ctx.hashLookup = {utils::calculate_hash64(hashLookup)};
			}

			//! Tries to match component ids in \param componentIdsQuery with those in \param componentIds.
			//! \return True if there is a match, false otherwise.
			static GAIA_NODISCARD bool
			CheckMatchOne(const ComponentIdList& componentIds, const ComponentIdArray& componentIdsQuery) {
				for (const auto componentId: componentIdsQuery) {
					if (utils::has(componentIds, componentId))
						return true;
				}

				return false;
			}

			//! Tries to match all component ids in \param componentIdsQuery with those in \param componentIds.
			//! \return True if there is a match, false otherwise.
			static GAIA_NODISCARD bool
			CheckMatchMany(const ComponentIdList& componentIds, const ComponentIdArray& componentIdsQuery) {
				size_t matches = 0;

				for (const auto componentIdQuery: componentIdsQuery) {
					for (const auto componentId: componentIds) {
						if (componentId == componentIdQuery) {
							if (++matches == componentIdsQuery.size())
								return true;

							break;
						}
					}
				}

				return false;
			}

			//! Tries to match \param componentIds with a given \param matcherHash.
			//! \return MatchArchetypeQueryRet::Fail if there is no match, MatchArchetypeQueryRet::Ok for match or
			//! MatchArchetypeQueryRet::Skip is not relevant.
			template <ComponentType TComponentType>
			GAIA_NODISCARD MatchArchetypeQueryRet
			Match(const ComponentIdList& componentIds, ComponentMatcherHash matcherHash) const {
				const auto& queryList = GetData(TComponentType);
				const auto withNoneTest = matcherHash.hash & queryList.hash[ListType::LT_None].hash;
				const auto withAnyTest = matcherHash.hash & queryList.hash[ListType::LT_Any].hash;
				const auto withAllTest = matcherHash.hash & queryList.hash[ListType::LT_All].hash;

				// If withAllTest is empty but we wanted something
				if (!withAllTest && queryList.hash[ListType::LT_All].hash != 0)
					return MatchArchetypeQueryRet::Fail;

				// If withAnyTest is empty but we wanted something
				if (!withAnyTest && queryList.hash[ListType::LT_Any].hash != 0)
					return MatchArchetypeQueryRet::Fail;

				// If there is any match with withNoneList we quit
				if (withNoneTest != 0) {
					if (CheckMatchOne(componentIds, queryList.componentIds[ListType::LT_None]))
						return MatchArchetypeQueryRet::Fail;
				}

				// If there is any match with withAnyTest
				if (withAnyTest != 0) {
					if (CheckMatchOne(componentIds, queryList.componentIds[ListType::LT_Any]))
						goto checkWithAllMatches;

					// At least one match necessary to continue
					return MatchArchetypeQueryRet::Fail;
				}

			checkWithAllMatches:
				// If withAllList is not empty there has to be an exact match
				if (withAllTest != 0) {
					// If the number of queried components is greater than the
					// number of components in archetype there's no need to search
					if (queryList.componentIds[ListType::LT_All].size() <= componentIds.size()) {
						// m_list[ListType::LT_All] first because we usually request for less
						// components than there are components in archetype
						if (CheckMatchMany(componentIds, queryList.componentIds[ListType::LT_All]))
							return MatchArchetypeQueryRet::Ok;
					}

					// No match found. We're done
					return MatchArchetypeQueryRet::Fail;
				}

				return (withAnyTest != 0) ? MatchArchetypeQueryRet::Ok : MatchArchetypeQueryRet::Skip;
			}

			GAIA_NODISCARD bool operator==(const LookupCtx& other) const {
				return m_lookupCtx == other;
			}

			GAIA_NODISCARD bool operator!=(const LookupCtx& other) const {
				return !operator==(other);
			}

			//! Tries to match the query against \param archetypes. For each matched archetype the archetype is cached.
			//! This is necessary so we do not iterate all chunks over and over again when running queries.
			void Match(const containers::darray<Archetype*>& archetypes) {
				for (size_t i = m_lastArchetypeId; i < archetypes.size(); i++) {
					auto* pArchetype = archetypes[i];
#if GAIA_DEBUG
					auto& archetype = *pArchetype;
#else
					const auto& archetype = *pArchetype;
#endif

					// Early exit if generic query doesn't match
					const auto retGeneric = Match<ComponentType::CT_Generic>(
							GetArchetypeComponentInfoList(archetype, ComponentType::CT_Generic),
							GetArchetypeMatcherHash(archetype, ComponentType::CT_Generic));
					if (retGeneric == MatchArchetypeQueryRet::Fail)
						continue;

					// Early exit if chunk query doesn't match
					const auto retChunk = Match<ComponentType::CT_Chunk>(
							GetArchetypeComponentInfoList(archetype, ComponentType::CT_Chunk),
							GetArchetypeMatcherHash(archetype, ComponentType::CT_Chunk));
					if (retChunk == MatchArchetypeQueryRet::Fail)
						continue;

					// If at least one query succeeded run our logic
					if (retGeneric == MatchArchetypeQueryRet::Ok || retChunk == MatchArchetypeQueryRet::Ok)
						m_archetypeCache.push_back(pArchetype);
				}

				m_lastArchetypeId = (uint32_t)archetypes.size();
			}

			template <typename T>
			bool HasComponent_Internal(ListType listType) const {
				using U = typename DeduceComponent<T>::Type;
				using UOriginal = typename DeduceComponent<T>::TypeOriginal;
				using UOriginalPR = std::remove_reference_t<std::remove_pointer_t<UOriginal>>;
				constexpr bool isReadWrite =
						std::is_same_v<U, UOriginal> || !std::is_const_v<UOriginalPR> && !std::is_empty_v<U>;

				if constexpr (IsGenericComponent<T>)
					return HasComponent_Internal<U>(listType, ComponentType::CT_Generic, isReadWrite);
				else
					return HasComponent_Internal<U>(listType, ComponentType::CT_Chunk, isReadWrite);
			}

		private:
			GAIA_NODISCARD const ComponentListData& GetData(ComponentType componentType) const {
				return m_lookupCtx.list[componentType];
			}

			GAIA_NODISCARD const ChangeFilterArray& GetFiltered(ComponentType componentType) const {
				return m_lookupCtx.listChangeFiltered[componentType];
			}

			GAIA_NODISCARD bool HasFilters() const {
				return !m_lookupCtx.listChangeFiltered[ComponentType::CT_Generic].empty() ||
							 !m_lookupCtx.listChangeFiltered[ComponentType::CT_Chunk].empty();
			}

			template <typename... T>
			bool HasAny() const {
				return (HasComponent_Internal<T>(ListType::LT_Any) || ...);
			}

			template <typename... T>
			bool HasAll() const {
				return (HasComponent_Internal<T>(ListType::LT_All) && ...);
			}

			template <typename... T>
			bool HasNone() const {
				return (!HasComponent_Internal<T>(ListType::LT_None) && ...);
			}

			GAIA_NODISCARD containers::darray<Archetype*>::iterator begin() {
				return m_archetypeCache.begin();
			}

			GAIA_NODISCARD containers::darray<Archetype*>::const_iterator begin() const {
				return m_archetypeCache.cbegin();
			}

			GAIA_NODISCARD containers::darray<Archetype*>::iterator end() {
				return m_archetypeCache.end();
			}

			GAIA_NODISCARD containers::darray<Archetype*>::const_iterator end() const {
				return m_archetypeCache.cend();
			}
		};
	} // namespace ecs
} // namespace gaia