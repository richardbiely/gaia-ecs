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
#include "id.h"

namespace gaia {
	namespace ecs {
		//! Cache for compile-time defined components
		class ComponentCache {
			static constexpr uint32_t FastComponentCacheSize = 512;

			//! Fast-lookup cache for the first FastComponentCacheSize components
			cnt::darray<const ComponentCacheItem*> m_descIdArr;
			//! Slower but more memory-friendly lookup cache for components with ids beyond FastComponentCacheSize
			cnt::map<detail::ComponentDescId, const ComponentCacheItem*> m_descByDescId;

			//! Lookup of component items by their symbol name. Strings are owned by m_descIdArr/m_descByDescId
			cnt::map<ComponentCacheItem::SymbolLookupKey, const ComponentCacheItem*> m_compByString;
			//! Lookup of component items by their entity.
			cnt::map<EntityLookupKey, const ComponentCacheItem*> m_compByEntity;

		public:
			ComponentCache() {
				// Reserve enough storage space for most use-cases
				m_descIdArr.reserve(FastComponentCacheSize);
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
				for (const auto* pDesc: m_descIdArr)
					ComponentCacheItem::destroy(const_cast<ComponentCacheItem*>(pDesc));
				for (auto [componentId, pDesc]: m_descByDescId)
					ComponentCacheItem::destroy(const_cast<ComponentCacheItem*>(pDesc));

				m_descIdArr.clear();
				m_descByDescId.clear();
				m_compByString.clear();
				m_compByEntity.clear();
			}

			//! Registers the component item for \tparam T. If it already exists it is returned.
			//! \return Component info
			template <typename T>
			GAIA_NODISCARD GAIA_FORCEINLINE const ComponentCacheItem& add(Entity entity) {
				using U = typename component_type_t<T>::Type;
				const auto compDescId = detail::ComponentDesc<T>::id();

				// Fast path for small component ids - use the array storage
				if (compDescId < FastComponentCacheSize) {
					auto createDesc = [&]() -> const ComponentCacheItem& {
						const auto* pDesc = ComponentCacheItem::create<U>(entity);
						m_descIdArr[compDescId] = pDesc;
						m_compByString.emplace(pDesc->name, pDesc);
						m_compByEntity.emplace(pDesc->entity, pDesc);
						return *pDesc;
					};

					if GAIA_UNLIKELY (compDescId >= m_descIdArr.size()) {
						const auto oldSize = m_descIdArr.size();
						const auto newSize = compDescId + 1U;

						// Increase the capacity by multiples of CapacityIncreaseSize
						constexpr uint32_t CapacityIncreaseSize = 128;
						const auto newCapacity = (newSize / CapacityIncreaseSize) * CapacityIncreaseSize + CapacityIncreaseSize;
						m_descIdArr.reserve(newCapacity);

						// Update the size
						m_descIdArr.resize(newSize);

						// Make sure unused memory is initialized to nullptr.
						GAIA_FOR2(oldSize, newSize - 1) m_descIdArr[i] = nullptr;

						return createDesc();
					}

					if GAIA_UNLIKELY (m_descIdArr[compDescId] == nullptr) {
						return createDesc();
					}

					return *m_descIdArr[compDescId];
				}

				// Generic path for large component ids - use the map storage
				{
					auto createDesc = [&]() -> const ComponentCacheItem& {
						const auto* pDesc = ComponentCacheItem::create<U>(entity);
						m_descByDescId.emplace(compDescId, pDesc);
						m_compByString.emplace(pDesc->name, pDesc);
						m_compByEntity.emplace(pDesc->entity, pDesc);
						return *pDesc;
					};

					const auto it = m_descByDescId.find(compDescId);
					if (it == m_descByDescId.end())
						return createDesc();

					return *it->second;
				}
			}

			//! Searches for the component cache item given the \param compDescId.
			//! \return Component info or nullptr it not found.
			GAIA_NODISCARD const ComponentCacheItem* find(detail::ComponentDescId compDescId) const noexcept {
				// Fast path - array storage
				if (compDescId < FastComponentCacheSize) {
					if (compDescId >= m_descIdArr.size())
						return nullptr;

					return m_descIdArr[compDescId];
				}

				// Generic path - map storage
				const auto it = m_descByDescId.find(compDescId);
				return it != m_descByDescId.end() ? it->second : nullptr;
			}

			//! Returns the component cache item given the \param compDescId.
			//! \return Component info
			//! \warning It is expected the component item with the given id exists! Undefined behavior otherwise.
			GAIA_NODISCARD const ComponentCacheItem& get(detail::ComponentDescId compDescId) const noexcept {
				// Fast path - array storage
				if (compDescId < FastComponentCacheSize) {
					GAIA_ASSERT(compDescId < m_descIdArr.size());
					return *m_descIdArr[compDescId];
				}

				// Generic path - map storage
				GAIA_ASSERT(m_descByDescId.contains(compDescId));
				return *m_descByDescId.find(compDescId)->second;
			}

			//! Searches for the component cache item.
			//! \param entity Entity associated with the component item.
			//! \param len String length. If zero, the length is calculated.
			//! \return Component cache item if found, nullptr otherwise.
			GAIA_NODISCARD const ComponentCacheItem* find(Entity entity) const noexcept {
				const auto it = m_compByEntity.find(EntityLookupKey(entity));
				if (it != m_compByEntity.end())
					return it->second;

				return nullptr;
			}

			//! Returns the component cache item.
			//! \param entity Entity associated with the component item.
			//! \return Component info.
			//! \warning It is expected the component item with the given name/length exists! Undefined behavior otherwise.
			GAIA_NODISCARD const ComponentCacheItem& get(Entity entity) const noexcept {
				const auto* pItem = find(entity);
				GAIA_ASSERT(pItem != nullptr);
				return *pItem;
			}

			//! Searches for the component cache item. The provided string is NOT copied internally.
			//! \param name A null-terminated string.
			//! \param len String length. If zero, the length is calculated.
			//! \return Component cache item if found, nullptr otherwise.
			GAIA_NODISCARD const ComponentCacheItem* find(const char* name, uint32_t len = 0) const noexcept {
				const auto it = m_compByString.find(
						len != 0 ? ComponentCacheItem::SymbolLookupKey(name, len) : ComponentCacheItem::SymbolLookupKey(name));
				if (it != m_compByString.end())
					return it->second;

				return nullptr;
			}

			//! Returns the component cache item. The provided string is NOT copied internally.
			//! \param name A null-terminated string
			//! \param len String length. If zero, the length is calculated
			//! \return Component info.
			//! \warning It is expected the component item with the given name/length exists! Undefined behavior otherwise.
			GAIA_NODISCARD const ComponentCacheItem& get(const char* name, uint32_t len = 0) const noexcept {
				const auto* pItem = find(name, len);
				GAIA_ASSERT(pItem != nullptr);
				return *pItem;
			}

			//! Searches for the component item for \tparam T.
			//! \warning It is expected the component already exists! Undefined behavior otherwise.
			//! \return Component info or nullptr if not found.
			template <typename T>
			GAIA_NODISCARD const ComponentCacheItem* find() const noexcept {
				const auto compDescId = detail::ComponentDesc<T>::id();
				return find(compDescId);
			}

			//! Returns the component item for \tparam T.
			//! \warning It is expected the component already exists! Undefined behavior otherwise.
			//! \return Component info
			template <typename T>
			GAIA_NODISCARD const ComponentCacheItem& get() const noexcept {
				const auto compDescId = detail::ComponentDesc<T>::id();
				return get(compDescId);
			}

			void diag() const {
				const auto registeredTypes = m_descIdArr.size();
				GAIA_LOG_N("Registered components: %u", registeredTypes);

				auto logDesc = [](const ComponentCacheItem* pDesc) {
					GAIA_LOG_N(
							"  [%u.%u], id:%010u, %s", pDesc->entity.id(), pDesc->entity.gen(), pDesc->comp.id(), pDesc->name.str());
				};
				for (const auto* pDesc: m_descIdArr)
					logDesc(pDesc);
				for (auto [componentId, pDesc]: m_descByDescId)
					logDesc(pDesc);
			}
		};
	} // namespace ecs
} // namespace gaia
