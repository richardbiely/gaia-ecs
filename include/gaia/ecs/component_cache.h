#pragma once
#include "../config/config.h"

#include <cstdint>
#include <type_traits>

#include "../cnt/darray.h"
#include "../cnt/map.h"
#include "../config/logging.h"
#include "../meta/type_info.h"
#include "component.h"
#include "component_cache_item.h"

namespace gaia {
	namespace ecs {
		class ComponentCache {
			static constexpr uint32_t FastComponentCacheSize = 1024;

			//! Fast-lookup cache for the first FastComponentCacheSize components
			cnt::darray<const ComponentCacheItem*> m_descByIndex;
			//! Slower but more memory-friendly lookup cache for components with ids beyond FastComponentCacheSize
			cnt::map<ComponentId, const ComponentCacheItem*> m_descByIndexMap;

		public:
			ComponentCache() {
				// Reserve enough storage space for most use-cases
				m_descByIndex.reserve(FastComponentCacheSize);
			}

			~ComponentCache() {
				clear();
			}

			ComponentCache(ComponentCache&&) = delete;
			ComponentCache(const ComponentCache&) = delete;
			ComponentCache& operator=(ComponentCache&&) = delete;
			ComponentCache& operator=(const ComponentCache&) = delete;

			//! Clears the contents of the component cache
			//! \warning Should be used only after worlds are cleared because it invalidates all currently
			//!          existing component ids. Any cached content would stop working.
			void clear() {
				for (const auto* pDesc: m_descByIndex)
					delete pDesc;
				for (auto [componentId, pDesc]: m_descByIndexMap)
					delete pDesc;
				m_descByIndex.clear();
				m_descByIndexMap.clear();
			}

			//! Registers the component info for \tparam T. If it already exists it is returned.
			//! \return Component info
			template <typename T>
			GAIA_NODISCARD GAIA_FORCEINLINE const ComponentCacheItem& goc_comp_desc() {
				using U = typename component_type_t<T>::Type;
				const auto compId = comp_id<T>();

				// Fast path for small component ids - use the array storage
				if (compId < FastComponentCacheSize) {
					auto createDesc = [&]() -> const ComponentCacheItem& {
						const auto* pDesc = ComponentCacheItem::create<U>();
						m_descByIndex[compId] = pDesc;
						return *pDesc;
					};

					if GAIA_UNLIKELY (compId >= m_descByIndex.size()) {
						const auto oldSize = m_descByIndex.size();
						const auto newSize = compId + 1U;

						// Increase the capacity by multiples of CapacityIncreaseSize
						constexpr uint32_t CapacityIncreaseSize = 128;
						const auto newCapacity = (newSize / CapacityIncreaseSize) * CapacityIncreaseSize + CapacityIncreaseSize;
						m_descByIndex.reserve(newCapacity);

						// Update the size
						m_descByIndex.resize(newSize);

						// Make sure unused memory is initialized to nullptr.
						GAIA_FOR2(oldSize, newSize - 1) m_descByIndex[i] = nullptr;

						return createDesc();
					}

					if GAIA_UNLIKELY (m_descByIndex[compId] == nullptr) {
						return createDesc();
					}

					return *m_descByIndex[compId];
				}

				// Generic path for large component ids - use the map storage
				{
					auto createDesc = [&]() -> const ComponentCacheItem& {
						const auto* pDesc = ComponentCacheItem::create<U>();
						m_descByIndexMap.emplace(compId, pDesc);
						return *pDesc;
					};

					const auto it = m_descByIndexMap.find(compId);
					if (it == m_descByIndexMap.end())
						return createDesc();

					return *it->second;
				}
			}

			//! Returns the component creation info given the \param compId.
			//! \warning It is expected the component info with a given component id exists! Undefined behavior otherwise.
			//! \return Component info
			GAIA_NODISCARD const ComponentCacheItem& comp_desc(ComponentId compId) const noexcept {
				// Fast path - array storage
				if (compId < FastComponentCacheSize) {
					GAIA_ASSERT(compId < m_descByIndex.size());
					return *m_descByIndex[compId];
				}

				// Generic path - map storage
				GAIA_ASSERT(m_descByIndexMap.contains(compId));
				return *m_descByIndexMap.find(compId)->second;
			}

			//! Returns the component info for \tparam T.
			//! \warning It is expected the component already exists! Undefined behavior otherwise.
			//! \return Component info
			template <typename T>
			GAIA_NODISCARD const ComponentCacheItem& comp_desc() const noexcept {
				const auto compId = comp_id<T>();
				return comp_desc(compId);
			}

			void diag() const {
				const auto registeredTypes = m_descByIndex.size();
				GAIA_LOG_N("Registered comps: %u", registeredTypes);

				auto logDesc = [](const ComponentCacheItem* pDesc) {
					GAIA_LOG_N("  id:%010u, %.*s", pDesc->comp.id(), (uint32_t)pDesc->name.size(), pDesc->name.data());
				};
				for (const auto* pDesc: m_descByIndex)
					logDesc(pDesc);
				for (auto [componentId, pDesc]: m_descByIndexMap)
					logDesc(pDesc);
			}
		};
	} // namespace ecs
} // namespace gaia
