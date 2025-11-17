#pragma once
#include "gaia/config/config.h"

#include <cinttypes>
#include <cstdint>
#include <type_traits>

#include "gaia/cnt/darray.h"
#include "gaia/cnt/map.h"
#include "gaia/core/hashing_string.h"
#include "gaia/ecs/component.h"
#include "gaia/ecs/component_cache_item.h"
#include "gaia/ecs/component_desc.h"
#include "gaia/ecs/id.h"
#include "gaia/meta/type_info.h"
#include "gaia/util/logging.h"

namespace gaia {
	namespace ecs {
		class World;

		//! Cache for compile-time defined components
		class GAIA_API ComponentCache final {
			friend class World;

			static constexpr uint32_t FastComponentCacheSize = 512;

			//! Fast-lookup cache for the first FastComponentCacheSize components
			cnt::darray<const ComponentCacheItem*> m_itemArr;
			//! Slower but more memory-friendly lookup cache for components with ids beyond FastComponentCacheSize
			cnt::map<detail::ComponentDescId, const ComponentCacheItem*> m_itemByDescId;

			//! Lookup of component items by their symbol name. Strings are owned by m_itemArr/m_itemByDescId
			cnt::map<ComponentCacheItem::SymbolLookupKey, const ComponentCacheItem*> m_compByString;
			//! Lookup of component items by their entity.
			cnt::map<EntityLookupKey, const ComponentCacheItem*> m_compByEntity;

			//! Clears the contents of the component cache
			//! \warning Should be used only after worlds are cleared because it invalidates all currently
			//!          existing component ids. Any cached content would stop working.
			//! \note Hidden from users because clearing the cache means that all existing component entities
			//!       would lose connection to it and effectively become dangling. This means that a new
			//!       component of type T could be added with a new entity id.
			void clear() {
				for (const auto* pItem: m_itemArr)
					ComponentCacheItem::destroy(const_cast<ComponentCacheItem*>(pItem));
				for (auto [componentId, pItem]: m_itemByDescId)
					ComponentCacheItem::destroy(const_cast<ComponentCacheItem*>(pItem));

				m_itemArr.clear();
				m_itemByDescId.clear();
				m_compByString.clear();
				m_compByEntity.clear();
			}

		public:
			ComponentCache() {
				// Reserve enough storage space for most use-cases
				m_itemArr.reserve(FastComponentCacheSize);
			}

			~ComponentCache() {
				clear();
			}

			ComponentCache(ComponentCache&&) = delete;
			ComponentCache(const ComponentCache&) = delete;
			ComponentCache& operator=(ComponentCache&&) = delete;
			ComponentCache& operator=(const ComponentCache&) = delete;

			//! Registers the component item for \tparam T. If it already exists it is returned.
			//! \return Component info
			template <typename T>
			GAIA_NODISCARD GAIA_FORCEINLINE const ComponentCacheItem& add(Entity entity) {
				static_assert(!is_pair<T>::value);
				GAIA_ASSERT(!entity.pair());

				const auto compDescId = detail::ComponentDesc<T>::id();

				// Fast path for small component ids - use the array storage
				if (compDescId < FastComponentCacheSize) {
					auto createDesc = [&]() -> const ComponentCacheItem& {
						const auto* pItem = ComponentCacheItem::create<T>(entity);
						GAIA_ASSERT(compDescId == pItem->comp.id());
						m_itemArr[compDescId] = pItem;
						m_compByString.emplace(pItem->name, pItem);
						m_compByEntity.emplace(pItem->entity, pItem);
						return *pItem;
					};

					if GAIA_UNLIKELY (compDescId >= m_itemArr.size()) {
						const auto oldSize = m_itemArr.size();
						const auto newSize = compDescId + 1U;

						// Increase the capacity by multiples of CapacityIncreaseSize
						constexpr uint32_t CapacityIncreaseSize = 128;
						const auto newCapacity = ((newSize / CapacityIncreaseSize) * CapacityIncreaseSize) + CapacityIncreaseSize;
						m_itemArr.reserve(newCapacity);

						// Update the size
						m_itemArr.resize(newSize);

						// Make sure unused memory is initialized to nullptr.
						GAIA_FOR2(oldSize, newSize - 1) m_itemArr[i] = nullptr;

						return createDesc();
					}

					if GAIA_UNLIKELY (m_itemArr[compDescId] == nullptr) {
						return createDesc();
					}

					return *m_itemArr[compDescId];
				}

				// Generic path for large component ids - use the map storage
				{
					auto createDesc = [&]() -> const ComponentCacheItem& {
						const auto* pItem = ComponentCacheItem::create<T>(entity);
						GAIA_ASSERT(compDescId == pItem->comp.id());
						m_itemByDescId.emplace(compDescId, pItem);
						m_compByString.emplace(pItem->name, pItem);
						m_compByEntity.emplace(pItem->entity, pItem);
						return *pItem;
					};

					const auto it = m_itemByDescId.find(compDescId);
					if (it == m_itemByDescId.end())
						return createDesc();

					return *it->second;
				}
			}

			//! Searches for the component cache item given the \param compDescId.
			//! \return Component info or nullptr it not found.
			GAIA_NODISCARD const ComponentCacheItem* find(detail::ComponentDescId compDescId) const noexcept {
				// Fast path - array storage
				if (compDescId < FastComponentCacheSize) {
					if (compDescId >= m_itemArr.size())
						return nullptr;

					return m_itemArr[compDescId];
				}

				// Generic path - map storage
				const auto it = m_itemByDescId.find(compDescId);
				return it != m_itemByDescId.end() ? it->second : nullptr;
			}

			//! Returns the component cache item given the \param compDescId.
			//! \return Component info
			//! \warning It is expected the component item with the given id exists! Undefined behavior otherwise.
			GAIA_NODISCARD const ComponentCacheItem& get(detail::ComponentDescId compDescId) const noexcept {
				// Fast path - array storage
				if (compDescId < FastComponentCacheSize) {
					GAIA_ASSERT(compDescId < m_itemArr.size());
					return *m_itemArr[compDescId];
				}

				// Generic path - map storage
				GAIA_ASSERT(m_itemByDescId.contains(compDescId));
				return *m_itemByDescId.find(compDescId)->second;
			}

			//! Searches for the component cache item.
			//! \param entity Entity associated with the component item.
			//! \param len String length. If zero, the length is calculated.
			//! \return Component cache item if found, nullptr otherwise.
			GAIA_NODISCARD const ComponentCacheItem* find(Entity entity) const noexcept {
				GAIA_ASSERT(!entity.pair());
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
				GAIA_ASSERT(!entity.pair());
				const auto* pItem = find(entity);
				GAIA_ASSERT(pItem != nullptr);
				return *pItem;
			}

			//! Searches for the component cache item. The provided string is NOT copied internally.
			//! \param name A null-terminated string.
			//! \param len String length. If zero, the length is calculated.
			//! \return Component cache item if found, nullptr otherwise.
			GAIA_NODISCARD const ComponentCacheItem* find(const char* name, uint32_t len = 0) const noexcept {
				const auto l = len == 0 ? (uint32_t)strlen(name) : len;
				const auto it = m_compByString.find(ComponentCacheItem::SymbolLookupKey(name, l, 0));
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
				static_assert(!is_pair<T>::value);
				const auto compDescId = detail::ComponentDesc<T>::id();
				return find(compDescId);
			}

			//! Returns the component item for \tparam T.
			//! \warning It is expected the component already exists! Undefined behavior otherwise.
			//! \return Component info
			template <typename T>
			GAIA_NODISCARD const ComponentCacheItem& get() const noexcept {
				static_assert(!is_pair<T>::value);
				const auto compDescId = detail::ComponentDesc<T>::id();
				return get(compDescId);
			}

#if GAIA_ENABLE_HOOKS

			//! Gives access to hooks that can be defined for a given component.
			//! \param cacheItem Cache item of a component.
			//! \return Reference to component hooks.
			static ComponentCacheItem::Hooks& hooks(const ComponentCacheItem& cacheItem) noexcept {
				return const_cast<ComponentCacheItem&>(cacheItem).hooks();
			}

#endif

			void diag() const {
				const auto registeredTypes = m_itemArr.size();
				GAIA_LOG_N("Registered components: %u", registeredTypes);

				auto logDesc = [](const ComponentCacheItem& item) {
					GAIA_LOG_N(
							"    hash:%016" PRIx64 ", size:%3u B, align:%3u B, [%u:%u] %s [%s]", item.hashLookup.hash,
							item.comp.size(), item.comp.alig(), item.entity.id(), item.entity.gen(), item.name.str(),
							EntityKindString[item.entity.kind()]);
				};
				for (const auto* pItem: m_itemArr) {
					if (pItem == nullptr)
						continue;
					logDesc(*pItem);
				}
				for (auto [componentId, pItem]: m_itemByDescId)
					logDesc(*pItem);
			}
		};
	} // namespace ecs
} // namespace gaia
