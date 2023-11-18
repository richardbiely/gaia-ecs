#pragma once
#include "../config/config.h"

#include <cstdint>
#include <type_traits>

#include "../cnt/darray.h"
#include "../cnt/map.h"
#include "../config/logging.h"
#include "../core/hashing_string.h"
#include "../meta/type_info.h"
#include "component.h"
#include "component_cache_item.h"
#include "component_desc.h"

namespace gaia {
	namespace ecs {
		class ComponentCache {
			static constexpr uint32_t FastComponentCacheSize = 1024;
			using SymbolLookupKey = core::StringLookupKey<512>;

			//! Fast-lookup cache for the first FastComponentCacheSize components
			cnt::darray<const ComponentCacheItem*> m_descByIndex;
			//! Slower but more memory-friendly lookup cache for components with ids beyond FastComponentCacheSize
			cnt::map<ComponentId, const ComponentCacheItem*> m_descByIndexMap;
			//! Lookup of component items by their symbol name. Strings are owned by m_descByIndex/m_descByIndexMap
			cnt::map<SymbolLookupKey, const ComponentCacheItem*> m_descByString;

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
					ComponentCacheItem::destroy(const_cast<ComponentCacheItem*>(pDesc));
				for (auto [componentId, pDesc]: m_descByIndexMap)
					ComponentCacheItem::destroy(const_cast<ComponentCacheItem*>(pDesc));

				m_descByIndex.clear();
				m_descByIndexMap.clear();
				m_descByString.clear();
			}

			//! Registers the component info for \tparam T. If it already exists it is returned.
			//! \return Component info
			template <typename T>
			GAIA_NODISCARD GAIA_FORCEINLINE const ComponentCacheItem& goc_comp_desc() {
				using U = typename component_type_t<T>::Type;
				const auto compDescId = detail::ComponentDesc<T>::id();

				// Fast path for small component ids - use the array storage
				if (compDescId < FastComponentCacheSize) {
					auto createDesc = [&]() -> const ComponentCacheItem& {
						const auto* pDesc = ComponentCacheItem::create<U>();
						m_descByIndex[compDescId] = pDesc;
						m_descByString[pDesc->name] = pDesc;
						return *pDesc;
					};

					if GAIA_UNLIKELY (compDescId >= m_descByIndex.size()) {
						const auto oldSize = m_descByIndex.size();
						const auto newSize = compDescId + 1U;

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

					if GAIA_UNLIKELY (m_descByIndex[compDescId] == nullptr) {
						return createDesc();
					}

					return *m_descByIndex[compDescId];
				}

				// Generic path for large component ids - use the map storage
				{
					auto createDesc = [&]() -> const ComponentCacheItem& {
						const auto* pDesc = ComponentCacheItem::create<U>();
						m_descByIndexMap.emplace(compDescId, pDesc);
						return *pDesc;
					};

					const auto it = m_descByIndexMap.find(compDescId);
					if (it == m_descByIndexMap.end())
						return createDesc();

					return *it->second;
				}
			}

			//! Searches for the cached component info given the \param compDescId.
			//! \warning It is expected the component info with a given component id exists! Undefined behavior otherwise.
			//! \return Component info or nullptr it not found.
			GAIA_NODISCARD const ComponentCacheItem* find_comp_desc(detail::ComponentDescId compDescId) const noexcept {
				// Fast path - array storage
				if (compDescId < FastComponentCacheSize) {
					if (compDescId >= m_descByIndex.size())
						return nullptr;

					return m_descByIndex[compDescId];
				}

				// Generic path - map storage
				const auto it = m_descByIndexMap.find(compDescId);
				return it != m_descByIndexMap.end() ? it->second : nullptr;
			}

			//! Returns the cached component info given the \param compDescId.
			//! \warning It is expected the component info with a given component id exists! Undefined behavior otherwise.
			//! \return Component info
			GAIA_NODISCARD const ComponentCacheItem& comp_desc(detail::ComponentDescId compDescId) const noexcept {
				// Fast path - array storage
				if (compDescId < FastComponentCacheSize) {
					GAIA_ASSERT(compDescId < m_descByIndex.size());
					return *m_descByIndex[compDescId];
				}

				// Generic path - map storage
				GAIA_ASSERT(m_descByIndexMap.contains(compDescId));
				return *m_descByIndexMap.find(compDescId)->second;
			}

			//! Searches for the cached component info. The provided string is NOT copied internally.
			//! \param name A null-terminated string.
			//! \param len String length. If zero, the length is calculated.
			//! \warning It is expected the component exists! Undefined behavior otherwise.
			//! \return Component info if found, otherwise nullptr.
			GAIA_NODISCARD const ComponentCacheItem* find_comp_desc(const char* name, uint32_t len = 0) const noexcept {
				const auto it = m_descByString.find(len != 0 ? SymbolLookupKey(name, len) : SymbolLookupKey(name));
				if (it != m_descByString.end())
					return it->second;

				return nullptr;
			}

			//! Returns the cached component info. The provided string is NOT copied internally.
			//! \param name A null-terminated string
			//! \param len String length. If zero, the length is calculated
			//! \return Component info.
			GAIA_NODISCARD const ComponentCacheItem& comp_desc(const char* name, uint32_t len = 0) const noexcept {
				const auto* pItem = find_comp_desc(name, len);
				GAIA_ASSERT(pItem != nullptr);
				return *pItem;
			}

			//! Searches for the component info for \tparam T.
			//! \warning It is expected the component already exists! Undefined behavior otherwise.
			//! \return Component info or nullptr if not found.
			template <typename T>
			GAIA_NODISCARD const ComponentCacheItem* find_comp_desc() const noexcept {
				const auto compDescId = detail::ComponentDesc<T>::id();
				return find_comp_desc(compDescId);
			}

			//! Returns the component info for \tparam T.
			//! \warning It is expected the component already exists! Undefined behavior otherwise.
			//! \return Component info
			template <typename T>
			GAIA_NODISCARD const ComponentCacheItem& comp_desc() const noexcept {
				const auto compDescId = detail::ComponentDesc<T>::id();
				return comp_desc(compDescId);
			}

			void diag() const {
				const auto registeredTypes = m_descByIndex.size();
				GAIA_LOG_N("Registered comps: %u", registeredTypes);

				auto logDesc = [](const ComponentCacheItem* pDesc) {
					GAIA_LOG_N("  id:%010u, %s", pDesc->comp.id(), pDesc->name.str());
				};
				for (const auto* pDesc: m_descByIndex)
					logDesc(pDesc);
				for (auto [componentId, pDesc]: m_descByIndexMap)
					logDesc(pDesc);
			}
		};
	} // namespace ecs
} // namespace gaia
