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
			containers::map<ComponentHash, const ComponentInfo*> m_infoByHash;

		public:
			ComponentCache() {
				// Reserve enough storage space for most use-cases
				constexpr uint32_t DefaultComponentCacheSize = 2048;
				m_infoByIndex.reserve(DefaultComponentCacheSize);
				m_infoCreateByIndex.reserve(DefaultComponentCacheSize);
				m_infoByHash.reserve(DefaultComponentCacheSize);
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
			GAIA_NODISCARD const ComponentInfo* GetOrCreateComponentInfo() {
				using U = typename DeduceComponent<T>::Type;

				const auto index = utils::type_info::index<U>();

				const auto res1 = m_infoCreateByIndex.try_emplace(index, ComponentInfoCreate{});
				if GAIA_UNLIKELY (res1.second)
					res1.first->second = ComponentInfoCreate::Create<U>();

				const auto res2 = m_infoByIndex.try_emplace(index, nullptr);
				if GAIA_UNLIKELY (res2.second) {
					const auto* pInfo = ComponentInfo::Create<U>();
					res2.first->second = pInfo;
					GAIA_SAFE_CONSTEXPR auto hash = utils::type_info::hash<U>();
					(void)m_infoByHash.try_emplace({hash}, pInfo);
				}
				return res2.first->second;
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
			GAIA_NODISCARD const ComponentInfo* GetComponentInfo() const {
				using U = typename DeduceComponent<T>::Type;
				GAIA_SAFE_CONSTEXPR auto hash = utils::type_info::hash<U>();

				return GetComponentInfoFromHash({hash});
			}

			//! Returns the component info given the \param index.
			//! \warning It is expected the component info with a given index exists! Undefined behavior otherwise.
			//! \return Component info
			GAIA_NODISCARD const ComponentInfo* GetComponentInfoFromIdx(uint32_t index) const {
				const auto it = m_infoByIndex.find(index);
				GAIA_ASSERT(it != m_infoByIndex.end());
				return it->second;
			}

			//! Returns the component creation info given the \param index.
			//! \warning It is expected the component info with a given index exists! Undefined behavior otherwise.
			//! \return Component info
			GAIA_NODISCARD const ComponentInfoCreate& GetComponentCreateInfoFromIdx(uint32_t index) const {
				const auto it = m_infoCreateByIndex.find(index);
				GAIA_ASSERT(it != m_infoCreateByIndex.end());
				return it->second;
			}

			//! Returns the component info given the \param hash.
			//! \warning It is expected the component info with a given index exists! Undefined behavior otherwise.
			//! \return Component info
			GAIA_NODISCARD const ComponentInfo* GetComponentInfoFromHash(ComponentHash hash) const {
				const auto it = m_infoByHash.find(hash);
				GAIA_ASSERT(it != m_infoByHash.end());
				return it->second;
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
				m_infoByHash.clear();
			}
		};
	} // namespace ecs
} // namespace gaia
