#pragma once
#include <cinttypes>
#include <type_traits>

#include "../config/config.h"
#include "../config/logging.h"
#include "../containers/map.h"
#include "../utils/type_info.h"
#include "component.h"

namespace gaia {
	namespace ecs {
		class ComponentCache {
			containers::map<uint32_t, const ComponentInfo*> m_infoByIndex;
			containers::map<uint32_t, ComponentInfoCreate> m_infoCreateByIndex;

		public:
			ComponentCache() {
				// Reserve enough storage space for most use-cases
				constexpr uint32_t DefaultComponentCacheSize = 2048;
				m_infoByIndex.reserve(DefaultComponentCacheSize);
				m_infoCreateByIndex.reserve(DefaultComponentCacheSize);
			}
			~ComponentCache() {
				ClearRegisteredInfoCache();
			}

			ComponentCache(ComponentCache&&) = delete;
			ComponentCache(const ComponentCache&) = delete;
			ComponentCache& operator=(ComponentCache&&) = delete;
			ComponentCache& operator=(const ComponentCache&) = delete;

			template <typename T>
			GAIA_NODISCARD const ComponentInfo* GetOrCreateComponentInfo() {
				using U = typename DeduceComponent<T>::Type;
				const auto index = utils::type_info::index<U>();

				if (m_infoCreateByIndex.find(index) == m_infoCreateByIndex.end())
					(void)m_infoCreateByIndex.try_emplace(index, ComponentInfoCreate::Create<U>());

				{
					const auto res = m_infoByIndex.try_emplace(index, ComponentInfo::Create<U>());
					return res.first->second;
				}
			}

			template <typename T>
			GAIA_NODISCARD const ComponentInfo* FindComponentInfo() const {
				using U = typename DeduceComponent<T>::Type;

				const auto index = utils::type_info::hash<U>();
				const auto it = m_infoByIndex.find(index);
				return it != m_infoByIndex.end() ? it->second : (const ComponentInfo*)nullptr;
			}

			template <typename T>
			GAIA_NODISCARD const ComponentInfo* GetComponentInfo() const {
				using U = typename DeduceComponent<T>::Type;
				const auto index = utils::type_info::index<U>();
				return GetComponentInfoFromIdx(index);
			}

			GAIA_NODISCARD const ComponentInfo* GetComponentInfoFromIdx(uint32_t componentIndex) const {
				// Let's assume the component has been registered via AddComponent already!
				GAIA_ASSERT(m_infoByIndex.find(componentIndex) != m_infoByIndex.end());
				return m_infoByIndex.at(componentIndex);
			}

			GAIA_NODISCARD const ComponentInfoCreate& GetComponentCreateInfoFromIdx(uint32_t componentIndex) const {
				// Let's assume the component has been registered via AddComponent already!
				GAIA_ASSERT(m_infoCreateByIndex.find(componentIndex) != m_infoCreateByIndex.end());
				return m_infoCreateByIndex.at(componentIndex);
			}

			template <typename T>
			GAIA_NODISCARD bool HasComponentInfo() const {
				using U = typename DeduceComponent<T>::Type;
				const auto index = utils::type_info::index<U>();
				return m_infoCreateByIndex.find(index) != m_infoCreateByIndex.end();
			}

			void Diag() const {
				const auto registeredTypes = (uint32_t)m_infoCreateByIndex.size();
				LOG_N("Registered infos: %u", registeredTypes);

				for (const auto& pair: m_infoCreateByIndex) {
					const auto& info = pair.second;
					LOG_N("  (%p) index:%010u, %.*s", (void*)&info, info.infoIndex, (uint32_t)info.name.size(), info.name.data());
				}
			}

		private:
			void ClearRegisteredInfoCache() {
				for (auto& pair: m_infoByIndex)
					delete pair.second;
				m_infoByIndex.clear();
				m_infoCreateByIndex.clear();
			}
		};
	} // namespace ecs
} // namespace gaia
