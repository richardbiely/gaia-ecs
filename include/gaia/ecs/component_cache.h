#pragma once
#include <cinttypes>
#include <type_traits>

#include "../config/config.h"
#include "../config/logging.h"
#include "../containers/darray.h"
#include "../containers/map.h"
#include "../utils/type_info.h"
#include "component.h"

namespace gaia {
	namespace ecs {
		class ComponentCache {
			containers::darray<const ComponentInfo*> m_infoByIndex;
			containers::darray<ComponentDesc> m_descByIndex;
			containers::map<ComponentLookupHash, const ComponentInfo*> m_infoByHash;

			ComponentCache() {
				// Reserve enough storage space for most use-cases
				constexpr uint32_t DefaultComponentCacheSize = 2048;
				m_infoByIndex.reserve(DefaultComponentCacheSize);
				m_descByIndex.reserve(DefaultComponentCacheSize);
				m_infoByHash.reserve(DefaultComponentCacheSize);
			}

		public:
			static ComponentCache& Get() {
				static ComponentCache cache;
				return cache;
			}

			~ComponentCache() {
				ClearRegisteredInfoCache();
			}

			ComponentCache(ComponentCache&&) = delete;
			ComponentCache(const ComponentCache&) = delete;
			ComponentCache& operator=(ComponentCache&&) = delete;
			ComponentCache& operator=(const ComponentCache&) = delete;

			//! Registers the component info for \tparam T. If it already exists it is returned.
			//! \return Component info
			template <typename T>
			GAIA_NODISCARD const ComponentInfo& GetOrCreateComponentInfo() {
				using U = typename DeduceComponent<T>::Type;
				const auto componentId = GetComponentIdUnsafe<U>();

				auto createInfo = [&]() -> const ComponentInfo& {
					const auto* pInfo = ComponentInfo::Create<U>();
					m_infoByIndex[componentId] = pInfo;
					m_descByIndex[componentId] = ComponentDesc::Create<U>();
					GAIA_SAFE_CONSTEXPR auto hash = utils::type_info::hash<U>();
					[[maybe_unused]] const auto res = m_infoByHash.try_emplace({hash}, pInfo);
					// This has to be the first time this has has been added!
					GAIA_ASSERT(res.second);
					return *pInfo;
				};

				if GAIA_UNLIKELY (componentId >= m_infoByIndex.size()) {
					const auto oldSize = m_infoByIndex.size();
					const auto newSize = componentId + 1U;

					// Increase the capacity by multiples of 128
					constexpr uint32_t CapacityIncreaseSize = 128;
					const auto newCapacity = (newSize / CapacityIncreaseSize) * CapacityIncreaseSize + CapacityIncreaseSize;
					m_infoByIndex.reserve(newCapacity);

					// Update the size
					m_infoByIndex.resize(newSize);
					m_descByIndex.resize(newSize);

					// Make sure that unused memory is initialized to nullptr
					for (size_t i = oldSize; i < newSize; ++i)
						m_infoByIndex[i] = nullptr;

					return createInfo();
				}

				if GAIA_UNLIKELY (m_infoByIndex[componentId] == nullptr) {
					return createInfo();
				}

				return *m_infoByIndex[componentId];
			}

			//! Returns the component info for \tparam T.
			//! \return Component info if it exists, nullptr otherwise.
			template <typename T>
			GAIA_NODISCARD const ComponentInfo* FindComponentInfo() const {
				using U = typename DeduceComponent<T>::Type;

				GAIA_SAFE_CONSTEXPR auto hash = utils::type_info::hash<U>();
				const auto it = m_infoByHash.find({hash});
				return it != m_infoByHash.end() ? it->second : (const ComponentInfo*)nullptr;
			}

			//! Returns the component info for \tparam T.
			//! \warning It is expected the component already exists! Undefined behavior otherwise.
			//! \return Component info
			template <typename T>
			GAIA_NODISCARD const ComponentInfo& GetComponentInfo() const {
				using U = typename DeduceComponent<T>::Type;

				GAIA_SAFE_CONSTEXPR auto hash = utils::type_info::hash<U>();
				return GetComponentInfoFromHash({hash});
			}

			//! Returns the component info given the \param componentId.
			//! \warning It is expected the component info with a given component id exists! Undefined behavior otherwise.
			//! \return Component info
			GAIA_NODISCARD const ComponentInfo& GetComponentInfo(ComponentId componentId) const {
				GAIA_ASSERT(componentId < m_infoByIndex.size());
				const auto* pInfo = m_infoByIndex[componentId];
				GAIA_ASSERT(pInfo != nullptr);
				return *pInfo;
			}

			//! Returns the component creation info given the \param componentId.
			//! \warning It is expected the component info with a given component id exists! Undefined behavior otherwise.
			//! \return Component info
			GAIA_NODISCARD const ComponentDesc& GetComponentDesc(ComponentId componentId) const {
				GAIA_ASSERT(componentId < m_descByIndex.size());
				return m_descByIndex[componentId];
			}

			//! Returns the component info given the \param hash.
			//! \warning It is expected the component info with a given component id exists! Undefined behavior otherwise.
			//! \return Component info
			GAIA_NODISCARD const ComponentInfo& GetComponentInfoFromHash(ComponentLookupHash hash) const {
				const auto it = m_infoByHash.find(hash);
				GAIA_ASSERT(it != m_infoByHash.end());
				GAIA_ASSERT(it->second != nullptr);
				return *it->second;
			}

			void Diag() const {
				const auto registeredTypes = (uint32_t)m_descByIndex.size();
				GAIA_LOG_N("Registered infos: %u", registeredTypes);

				for (const auto& desc: m_descByIndex)
					GAIA_LOG_N("  id:%010u, %.*s", desc.componentId, (uint32_t)desc.name.size(), desc.name.data());
			}

		private:
			void ClearRegisteredInfoCache() {
				for (const auto* pInfo: m_infoByIndex)
					delete pInfo;
				m_infoByIndex.clear();
				m_descByIndex.clear();
				m_infoByHash.clear();
			}
		};

		GAIA_NODISCARD inline const ComponentCache& GetComponentCache() {
			return (const ComponentCache&)ComponentCache::Get();
		}

		GAIA_NODISCARD inline ComponentCache& GetComponentCacheRW() {
			return ComponentCache::Get();
		}
	} // namespace ecs
} // namespace gaia
