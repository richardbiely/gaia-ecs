#pragma once
#include "../config/config.h"

#include <cinttypes>
#include <type_traits>

#include "../config/logging.h"
#include "../containers/darray.h"
#include "../containers/map.h"
#include "../utils/type_info.h"
#include "component.h"
#include "component_desc.h"
#include "component_info.h"

namespace gaia {
	namespace ecs {
		class ComponentCache {
			containers::darray<const component::ComponentInfo*> m_infoByIndex;
			containers::darray<component::ComponentDesc> m_descByIndex;

			ComponentCache() {
				// Reserve enough storage space for most use-cases
				constexpr uint32_t DefaultComponentCacheSize = 2048;
				m_infoByIndex.reserve(DefaultComponentCacheSize);
				m_descByIndex.reserve(DefaultComponentCacheSize);
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
			GAIA_NODISCARD const component::ComponentInfo& GetOrCreateComponentInfo() {
				using U = typename component::DeduceComponent<T>::Type;
				const auto componentId = component::GetComponentId<T>();

				auto createInfo = [&]() -> const component::ComponentInfo& {
					const auto* pInfo = component::ComponentInfo::Create<U>();
					m_infoByIndex[componentId] = pInfo;
					m_descByIndex[componentId] = component::ComponentDesc::Create<U>();
					return *pInfo;
				};

				if GAIA_UNLIKELY (componentId >= m_infoByIndex.size()) {
					const auto oldSize = m_infoByIndex.size();
					const auto newSize = componentId + 1U;

					// Increase the capacity by multiples of CapacityIncreaseSize
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

			//! Returns the component info given the \param componentId.
			//! \warning It is expected the component info with a given component id exists! Undefined behavior otherwise.
			//! \return Component info
			GAIA_NODISCARD const component::ComponentInfo&
			GetComponentInfo(component::ComponentId componentId) const noexcept {
				GAIA_ASSERT(componentId < m_infoByIndex.size());
				const auto* pInfo = m_infoByIndex[componentId];
				GAIA_ASSERT(pInfo != nullptr);
				return *pInfo;
			}

			//! Returns the component creation info given the \param componentId.
			//! \warning It is expected the component info with a given component id exists! Undefined behavior otherwise.
			//! \return Component info
			GAIA_NODISCARD const component::ComponentDesc&
			GetComponentDesc(component::ComponentId componentId) const noexcept {
				GAIA_ASSERT(componentId < m_descByIndex.size());
				return m_descByIndex[componentId];
			}

			//! Returns the component info for \tparam T.
			//! \warning It is expected the component already exists! Undefined behavior otherwise.
			//! \return Component info
			template <typename T>
			GAIA_NODISCARD const component::ComponentInfo& GetComponentInfo() const noexcept {
				const auto componentId = component::GetComponentId<T>();
				return GetComponentInfo(componentId);
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
			}
		};
	} // namespace ecs
} // namespace gaia
