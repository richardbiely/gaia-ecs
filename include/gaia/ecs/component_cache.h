#pragma once
#include <cinttypes>
#include <type_traits>

#include "../config/config.h"
#include "../config/logging.h"
#include "../containers/map.h"
#include "../ecs/component.h"
#include "../utils/type_info.h"

namespace gaia {
	namespace ecs {
		class ComponentCache {
			containers::map<uint32_t, const ComponentInfo*> m_infoByIndex;
			containers::map<uint32_t, ComponentInfoCreate> m_infoCreateByIndex;

		public:
			ComponentCache() {
				// Reserve enough storage space for most use-cases
				m_infoByIndex.reserve(2048);
				m_infoCreateByIndex.reserve(2048);
			}

			ComponentCache(ComponentCache&&) = delete;
			ComponentCache(const ComponentCache&) = delete;
			ComponentCache& operator=(ComponentCache&&) = delete;
			ComponentCache& operator=(const ComponentCache&) = delete;

			~ComponentCache() {
				ClearRegisteredInfoCache();
			}

			template <typename T>
			[[nodiscard]] const ComponentInfo* GetOrCreateComponentInfo() {
				using U = typename DeduceComponent<T>::Type;
				const auto index = utils::type_info::index<U>();

				if (m_infoCreateByIndex.find(index) == m_infoCreateByIndex.end())
					m_infoCreateByIndex.emplace(index, ComponentInfoCreate::Create<U>());

				{
					const auto res = m_infoByIndex.emplace(index, nullptr);
					if (res.second)
						res.first->second = ComponentInfo::Create<U>();

					return res.first->second;
				}
			}

			template <typename T>
			[[nodiscard]] const ComponentInfo* FindComponentInfo() const {
				using U = typename DeduceComponent<T>::Type;

				const auto index = utils::type_info::hash<U>();
				const auto it = m_infoByIndex.find(index);
				return it != m_infoByIndex.end() ? it->second : (const ComponentInfo*)nullptr;
			}

			template <typename T>
			[[nodiscard]] const ComponentInfo* GetComponentInfo() const {
				using U = typename DeduceComponent<T>::Type;
				const auto index = utils::type_info::index<U>();
				return GetComponentInfoFromIdx(index);
			}

			[[nodiscard]] const ComponentInfo* GetComponentInfoFromIdx(uint32_t componentIndex) const {
				// Let's assume the component has been registered via AddComponent already!
				GAIA_ASSERT(m_infoByIndex.find(componentIndex) != m_infoByIndex.end());
				return m_infoByIndex.at(componentIndex);
			}

			[[nodiscard]] const ComponentInfoCreate& GetComponentCreateInfoFromIdx(uint32_t componentIndex) const {
				// Let's assume the component has been registered via AddComponent already!
				GAIA_ASSERT(m_infoCreateByIndex.find(componentIndex) != m_infoCreateByIndex.end());
				return m_infoCreateByIndex.at(componentIndex);
			}

			template <typename T>
			[[nodiscard]] bool HasComponentInfo() const {
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
