#pragma once
#include <tuple>
#include <type_traits>

#include "../config/profiler.h"
#include "../containers/darray.h"
#include "../containers/sarray_ext.h"
#include "../utils/hashing_policy.h"
#include "../utils/utility.h"
#include "common.h"
#include "component.h"
#include "component_cache.h"
#include "component_utils.h"
#include "data_buffer.h"
#include "entity_query_info.h"

namespace gaia {
	namespace ecs {
		class EntityQuery final {
		public:
			//! Query constraints
			enum class Constraints : uint8_t { EnabledOnly, DisabledOnly, AcceptAll };

		private:
			//! Command buffer command type
			enum CommandBufferCmd : uint8_t { ADD_COMPONENT, ADD_FILTER };

			struct Command_AddComponent {
				static constexpr CommandBufferCmd Id = CommandBufferCmd::ADD_COMPONENT;

				ComponentId componentId;
				ComponentType componentType;
				EntityQueryInfo::ListType listType;
				bool isReadWrite;

				void Save(DataBuffer& buffer) {
					buffer.Save(componentId);
					buffer.Save(componentType);
					buffer.Save(listType);
					buffer.Save(isReadWrite);
				}
				void Load(DataBuffer& buffer) {
					buffer.Load(componentId);
					buffer.Load(componentType);
					buffer.Load(listType);
					buffer.Load(isReadWrite);
				}

				void Exec(EntityQueryInfo::LookupCtx& ctx) {
					auto& list = ctx.list[componentType];
					auto& componentIds = list.componentIds[listType];

					// Unique component ids only
					GAIA_ASSERT(!utils::has(componentIds, componentId));

#if GAIA_DEBUG
					// There's a limit to the amount of components which we can store
					if (componentIds.size() >= EntityQueryInfo::MAX_COMPONENTS_IN_QUERY) {
						GAIA_ASSERT(false && "Trying to create an ECS query with too many components!");

						const auto& cc = ComponentCache::Get();
						auto componentName = cc.GetComponentDesc(componentId).name;
						GAIA_LOG_E(
								"Trying to add ECS component '%.*s' to an already full ECS query!", (uint32_t)componentName.size(),
								componentName.data());

						return;
					}
#endif

					ctx.rw[componentType] |= (uint8_t)isReadWrite << (uint8_t)componentIds.size();
					componentIds.push_back(componentId);
				}
			};

			struct Command_Filter {
				static constexpr CommandBufferCmd Id = CommandBufferCmd::ADD_FILTER;

				ComponentId componentId;
				ComponentType componentType;

				void Save(DataBuffer& buffer) {
					buffer.Save(componentId);
					buffer.Save(componentType);
				}
				void Load(DataBuffer& buffer) {
					buffer.Load(componentId);
					buffer.Load(componentType);
				}

				void Exec(EntityQueryInfo::LookupCtx& ctx) {
					auto& list = ctx.list[componentType];
					auto& arrFilter = ctx.listChangeFiltered[componentType];

					// Unique component ids only
					GAIA_ASSERT(!utils::has(arrFilter, componentId));

#if GAIA_DEBUG
					// There's a limit to the amount of components which we can store
					if (arrFilter.size() >= EntityQueryInfo::MAX_COMPONENTS_IN_QUERY) {
						GAIA_ASSERT(false && "Trying to create an ECS filter query with too many components!");

						const auto& cc = ComponentCache::Get();
						auto componentName = cc.GetComponentDesc(componentId).name;
						GAIA_LOG_E(
								"Trying to add ECS component %.*s to an already full filter query!", (uint32_t)componentName.size(),
								componentName.data());
						return;
					}
#endif

					// Component has to be present in anyList or allList.
					// NoneList makes no sense because we skip those in query processing anyway.
					if (utils::has(list.componentIds[EntityQueryInfo::ListType::LT_Any], componentId)) {
						arrFilter.push_back(componentId);
						return;
					}
					if (utils::has(list.componentIds[EntityQueryInfo::ListType::LT_All], componentId)) {
						arrFilter.push_back(componentId);
						return;
					}

					GAIA_ASSERT(false && "SetChangeFilter trying to filter ECS component which is not a part of the query");
#if GAIA_DEBUG
					const auto& cc = ComponentCache::Get();
					auto componentName = cc.GetComponentDesc(componentId).name;
					GAIA_LOG_E(
							"SetChangeFilter trying to filter ECS component %.*s but "
							"it's not a part of the query!",
							(uint32_t)componentName.size(), componentName.data());
#endif
				}
			};

			using CmdBufferCmdFunc = void (*)(DataBuffer& buffer, EntityQueryInfo::LookupCtx& ctx);
			static constexpr CmdBufferCmdFunc CommandBufferRead[] = {
					// Add component
					[](DataBuffer& buffer, EntityQueryInfo::LookupCtx& ctx) {
						Command_AddComponent cmd;
						cmd.Load(buffer);
						cmd.Exec(ctx);
					},
					// Add filter
					[](DataBuffer& buffer, EntityQueryInfo::LookupCtx& ctx) {
						Command_Filter cmd;
						cmd.Load(buffer);
						cmd.Exec(ctx);
					}};

			DataBuffer m_cmdBuffer;
			//! Lookup hash for this query
			EntityQueryInfo::LookupHash m_hashLookup{};
			//! Query cache id
			uint32_t m_cacheId = (uint32_t)-1;
			//! Tell what kinds of chunks are going to be accepted by the query
			EntityQuery::Constraints m_constraints = EntityQuery::Constraints::EnabledOnly;

			template <typename T>
			void AddComponent_Internal(EntityQueryInfo::ListType listType) {
				using U = typename DeduceComponent<T>::Type;
				using UOriginal = typename DeduceComponent<T>::TypeOriginal;
				using UOriginalPR = std::remove_reference_t<std::remove_pointer_t<UOriginal>>;

				const auto componentId = GetComponentIdUnsafe<U>();
				constexpr auto componentType = IsGenericComponent<T> ? ComponentType::CT_Generic : ComponentType::CT_Chunk;
				constexpr bool isReadWrite =
						std::is_same_v<U, UOriginal> || !std::is_const_v<UOriginalPR> && !std::is_empty_v<U>;

				// Make sure the component is always registered
				auto& cc = ComponentCache::Get();
				(void)cc.GetOrCreateComponentInfo<T>();

				m_cmdBuffer.Save(Command_AddComponent::Id);
				Command_AddComponent{componentId, componentType, listType, isReadWrite}.Save(m_cmdBuffer);
			}

			template <typename T>
			void WithChanged_Internal() {
				using U = typename DeduceComponent<T>::Type;
				using UOriginal = typename DeduceComponent<T>::TypeOriginal;
				using UOriginalPR = std::remove_reference_t<std::remove_pointer_t<UOriginal>>;

				const auto componentId = GetComponentIdUnsafe<U>();
				constexpr auto componentType = IsGenericComponent<T> ? ComponentType::CT_Generic : ComponentType::CT_Chunk;

				m_cmdBuffer.Save(Command_Filter::Id);
				Command_Filter{componentId, componentType}.Save(m_cmdBuffer);
			}

		public:
			void Init(EntityQueryInfo::LookupHash hash, uint32_t id) {
				GAIA_ASSERT(m_cacheId == (uint32_t)-1 || m_hashLookup == hash && m_cacheId == id);
				m_hashLookup = hash;
				m_cacheId = id;
			}

			EntityQueryInfo::LookupHash GetLookupHash() const {
				return m_hashLookup;
			}

			uint32_t GetCacheId() const {
				return m_cacheId;
			}

			template <typename... T>
			EntityQuery& All() {
				// Invalidte the query
				m_hashLookup = {0};
				// Add commands to the command buffer
				(AddComponent_Internal<T>(EntityQueryInfo::ListType::LT_All), ...);
				return *this;
			}

			template <typename... T>
			EntityQuery& Any() {
				// Invalidte the query
				m_hashLookup = {0};
				// Add commands to the command buffer
				(AddComponent_Internal<T>(EntityQueryInfo::ListType::LT_Any), ...);
				return *this;
			}

			template <typename... T>
			EntityQuery& None() {
				// Invalidte the query
				m_hashLookup = {0};
				// Add commands to the command buffer
				(AddComponent_Internal<T>(EntityQueryInfo::ListType::LT_None), ...);
				return *this;
			}

			template <typename... T>
			EntityQuery& WithChanged() {
				// Invalidte the query
				m_hashLookup = {0};
				// Add commands to the command buffer
				(WithChanged_Internal<T>(), ...);
				return *this;
			}

			EntityQuery& SetConstraints(EntityQuery::Constraints value) {
				m_constraints = value;
				return *this;
			}

			GAIA_NODISCARD EntityQuery::Constraints GetConstraints() const {
				return m_constraints;
			}

			template <bool Enabled>
			GAIA_NODISCARD bool CheckConstraints() const {
				// By default we only evaluate EnabledOnly changes. AcceptAll is something that has to be asked for
				// explicitely.
				if GAIA_UNLIKELY (m_constraints == EntityQuery::Constraints::AcceptAll)
					return true;

				if constexpr (Enabled)
					return m_constraints == EntityQuery::Constraints::EnabledOnly;
				else
					return m_constraints == EntityQuery::Constraints::DisabledOnly;
			}

			void Commit(EntityQueryInfo::LookupCtx& ctx) {
				// No need to commit anything if we already have the lookup hash
				if (m_hashLookup.hash != 0) {
					ctx.hashLookup = m_hashLookup;
					ctx.cacheId = m_cacheId;
					return;
				}

				// Read data from buffer and execute the command stored in it
				m_cmdBuffer.Seek(0);
				while (m_cmdBuffer.GetPos() < m_cmdBuffer.Size()) {
					CommandBufferCmd id{};
					m_cmdBuffer.Load(id);
					CommandBufferRead[id](m_cmdBuffer, ctx);
				}

				// Calculate the lookup hash from the provided context
				EntityQueryInfo::CalculateLookupHash(ctx);

				// We can free all temporary data now
				m_cmdBuffer.Reset();
			}
		};
	} // namespace ecs
} // namespace gaia
