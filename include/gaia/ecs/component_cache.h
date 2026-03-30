#pragma once
#include "gaia/config/config.h"

#include <cinttypes>
#include <cstdint>
#include <cstring>
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
#include "gaia/util/str.h"

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

			//! Lookup of component items by their exact registered symbol name.
			cnt::map<ComponentCacheItem::SymbolLookupKey, const ComponentCacheItem*> m_compBySymbol;
			//! Lookup of component items by their exact path name.
			//! Ambiguous paths are stored with nullptr and treated as lookup misses.
			cnt::map<ComponentCacheItem::SymbolLookupKey, const ComponentCacheItem*> m_compByPath;
			//! Lookup of component items by their unique short symbol name (leaf after the last `::`).
			//! Ambiguous short names are stored with nullptr and treated as lookup misses.
			cnt::map<ComponentCacheItem::SymbolLookupKey, const ComponentCacheItem*> m_compByShortSymbol;
			//! Lookup of component items by their entity.
			cnt::map<EntityLookupKey, const ComponentCacheItem*> m_compByEntity;
			//! Runtime component descriptor id generator.
			detail::ComponentDescId m_nextRuntimeCompDescId = 0x80000000u;

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
				m_compBySymbol.clear();
				m_compByPath.clear();
				m_compByShortSymbol.clear();
				m_compByEntity.clear();
			}

			template <typename Func>
			void for_each_item(Func&& func) {
				for (auto* pItem: m_itemArr) {
					if (pItem != nullptr)
						func(*const_cast<ComponentCacheItem*>(pItem));
				}

				for (auto& [componentId, pItem]: m_itemByDescId) {
					(void)componentId;
					func(*const_cast<ComponentCacheItem*>(pItem));
				}
			}

			template <typename Func>
			void for_each_item(Func&& func) const {
				for (const auto* pItem: m_itemArr) {
					if (pItem != nullptr)
						func(*pItem);
				}

				for (const auto& [componentId, pItem]: m_itemByDescId) {
					(void)componentId;
					func(*pItem);
				}
			}

			GAIA_NODISCARD static bool is_internal_symbol(util::str_view symbol) noexcept {
				constexpr char InternalPrefix[] = "gaia::ecs::";
				constexpr uint32_t InternalPrefixLen = (uint32_t)(sizeof(InternalPrefix) - 1);
				return symbol.size() >= InternalPrefixLen && memcmp(symbol.data(), InternalPrefix, InternalPrefixLen) == 0;
			}

			static ComponentCacheItem::SymbolLookupKey short_name_key(const ComponentCacheItem& item) noexcept {
				const auto* pName = item.name.str();
				const auto len = item.name.len();
				for (uint32_t i = len; i > 1; --i) {
					const auto idx = i - 1;
					if (pName[idx] != ':' || pName[idx - 1] != ':')
						continue;
					return ComponentCacheItem::SymbolLookupKey(pName + idx + 1, len - idx - 1, 0);
				}

				return ComponentCacheItem::SymbolLookupKey();
			}

			GAIA_NODISCARD static ComponentCacheItem::SymbolLookupKey lookup_key(util::str_view value) noexcept {
				return value.empty() ? ComponentCacheItem::SymbolLookupKey()
														 : ComponentCacheItem::SymbolLookupKey(value.data(), value.size(), 0);
			}

			static bool build_default_path(util::str& out, util::str_view symbol) {
				out.clear();
				bool changed = false;
				out.reserve(symbol.size());
				for (uint32_t i = 0; i < symbol.size(); ++i) {
					if (i + 1 < symbol.size() && symbol.data()[i] == ':' && symbol.data()[i + 1] == ':') {
						out.append('.');
						++i;
						changed = true;
						continue;
					}

					out.append(symbol.data()[i]);
				}

				if (!changed)
					out.clear();

				return changed;
			}

			//! Builds a scoped component path from a scope prefix and a leaf component name.
			//! \param out Receives the combined dotted path when successful.
			//! \param scopePath Dotted scope prefix.
			//! \param leaf Final component name segment.
			//! \return True when a scoped path was produced, false otherwise.
			static bool build_scoped_path(util::str& out, util::str_view scopePath, util::str_view leaf) {
				out.clear();
				if (scopePath.empty() || leaf.empty())
					return false;

				out.reserve(scopePath.size() + 1 + leaf.size());
				out.append(scopePath);
				out.append('.');
				out.append(leaf);
				return true;
			}

			//! Initializes alias and path metadata for a component item.
			//! \param item Component cache item to update.
			//! \param scopePath Optional world-provided scope prefix used for default path generation.
			static void initialize_names(ComponentCacheItem& item, util::str_view scopePath = {}) {
				item.path.clear();
				const auto symbol = util::str_view{item.name.str(), item.name.len()};
				if (is_internal_symbol(symbol))
					return;

				const auto shortName = short_name_key(item);
				const bool hasShortName =
						shortName.str() != nullptr && shortName.len() != 0 && shortName.len() != item.name.len();
				const auto leaf = hasShortName ? util::str_view{shortName.str(), shortName.len()} : symbol;

				if (!build_scoped_path(item.path, scopePath, leaf))
					(void)build_default_path(item.path, symbol);
			}

			template <typename ViewFunc>
			void rebuild_lookup_map(
					cnt::map<ComponentCacheItem::SymbolLookupKey, const ComponentCacheItem*>& map, ViewFunc&& getView) {
				map.clear();
				for_each_item([&](const ComponentCacheItem& item) {
					const auto view = getView(item);
					if (view.empty())
						return;

					const auto key = lookup_key(view);
					const auto it = map.find(key);
					if (it == map.end()) {
						map.emplace(key, &item);
						return;
					}

					if (it->second != &item)
						it->second = nullptr;
				});
			}

			void rebuild_resolved_name_maps() {
				rebuild_lookup_map(m_compByPath, [](const ComponentCacheItem& item) {
					return item.path.view();
				});
				rebuild_lookup_map(m_compByShortSymbol, [&](const ComponentCacheItem& item) {
					if (is_internal_symbol(symbol_name(item)))
						return util::str_view{};

					const auto shortName = short_name_key(item);
					return shortName.str() != nullptr && shortName.len() != 0 ? util::str_view(shortName.str(), shortName.len())
																																		: util::str_view{};
				});
			}

			//! Adds all name-based lookup mappings for a component item.
			//! \param item Component cache item to register.
			//! \param scopePath Optional world-provided scope prefix used for default path generation.
			void add_name_mappings(ComponentCacheItem& item, util::str_view scopePath) {
				m_compBySymbol.emplace(item.name, &item);
				initialize_names(item, scopePath);
				rebuild_resolved_name_maps();
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
			GAIA_NODISCARD GAIA_FORCEINLINE const ComponentCacheItem& add(Entity entity, util::str_view scopePath = {}) {
				static_assert(!is_pair<T>::value);
				GAIA_ASSERT(!entity.pair());

				const auto compDescId = detail::ComponentDesc<T>::id();

				// Fast path for small component ids - use the array storage
				if (compDescId < FastComponentCacheSize) {
					auto createDesc = [&]() -> const ComponentCacheItem& {
						const auto* pItem = ComponentCacheItem::create<T>(entity);
						GAIA_ASSERT(compDescId == pItem->comp.id());
						m_itemArr[compDescId] = pItem;
						add_name_mappings(*const_cast<ComponentCacheItem*>(pItem), scopePath);
						m_compByEntity.emplace(pItem->entity, pItem);
						return *pItem;
					};

					if GAIA_UNLIKELY (compDescId >= m_itemArr.size()) {
						const auto newSize = compDescId + 1U;

						// Increase the capacity by multiples of CapacityIncreaseSize
						constexpr uint32_t CapacityIncreaseSize = 128;
						const auto newCapacity = ((newSize / CapacityIncreaseSize) * CapacityIncreaseSize) + CapacityIncreaseSize;
						m_itemArr.reserve(newCapacity);

						// Update the size
						m_itemArr.resize(newSize, nullptr);

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
						add_name_mappings(*const_cast<ComponentCacheItem*>(pItem), scopePath);
						m_compByEntity.emplace(pItem->entity, pItem);
						return *pItem;
					};

					const auto it = m_itemByDescId.find(compDescId);
					if (it == m_itemByDescId.end())
						return createDesc();

					return *it->second;
				}
			}

			//! Registers a runtime-defined component.
			//! \param entity Entity associated with the component.
			//! \param name Component name.
			//! \param len Name length. If zero, the length is calculated.
			//! \param size Component size in bytes.
			//! \param alig Component alignment in bytes.
			//! \param soa Number of SoA items (0 for AoS).
			//! \param dataStorage Data storage type. DataStorageType::Table by default.
			//! \param pSoaSizes SoA item sizes, must contain at least @a soa values when @a soa > 0.
			//! \param hashLookup Optional lookup hash. If zero, hash(name) is used.
			//! \param scopePath Optional world-provided scoped path prefix used when assigning the default path/alias.
			//! \return Component info.
			GAIA_NODISCARD const ComponentCacheItem&
			add(Entity entity, const char* name, uint32_t len, uint32_t size, DataStorageType storageType, uint32_t alig = 1,
					uint32_t soa = 0, const uint8_t* pSoaSizes = nullptr, ComponentLookupHash hashLookup = {},
					util::str_view scopePath = {}) {
				GAIA_ASSERT(!entity.pair());
				GAIA_ASSERT(name != nullptr);

				const auto l = len == 0 ? (uint32_t)GAIA_STRLEN(name, ComponentCacheItem::MaxNameLength) : len;
				GAIA_ASSERT(l > 0 && l < ComponentCacheItem::MaxNameLength);

				{
					const auto* pExisting = symbol(name, l);
					if (pExisting != nullptr)
						return *pExisting;
				}

				detail::ComponentDescId compDescId = m_nextRuntimeCompDescId;
				while (find(compDescId) != nullptr) {
					++compDescId;
				}
				m_nextRuntimeCompDescId = compDescId + 1;

				ComponentCacheItem::ComponentCacheItemCtx ctx{};
				ctx.compDescId = compDescId;
				ctx.nameStr = name;
				ctx.nameLen = l;
				ctx.size = size;
				ctx.alig = alig;
				ctx.storageType = storageType;
				ctx.soa = soa;
				ctx.pSoaSizes = pSoaSizes;
				ctx.hashLookup = hashLookup;

				const auto* pItem = ComponentCacheItem::create(entity, ctx);
				if (compDescId < FastComponentCacheSize) {
					if (compDescId >= m_itemArr.size())
						m_itemArr.resize(compDescId + 1U);
					m_itemArr[compDescId] = pItem;
				} else {
					m_itemByDescId.emplace(compDescId, pItem);
				}

				add_name_mappings(*const_cast<ComponentCacheItem*>(pItem), scopePath);
				m_compByEntity.emplace(pItem->entity, pItem);
				return *pItem;
			}

			//! Searches for the component cache item given the @a compDescId.
			//! \param compDescId Component descriptor id
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

			//! Returns the component cache item given the @a compDescId.
			//! \param compDescId Component descriptor id
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
			//! \return Component cache item if found, nullptr otherwise.
			GAIA_NODISCARD const ComponentCacheItem* find(Entity entity) const noexcept {
				if (entity.pair())
					return nullptr;

				const auto it = m_compByEntity.find(EntityLookupKey(entity));
				if (it != m_compByEntity.end())
					return it->second;

				return nullptr;
			}

			//! Searches for the component cache item.
			//! \param entity Entity associated with the component item.
			//! \return Component cache item if found, nullptr otherwise.
			GAIA_NODISCARD ComponentCacheItem* find(Entity entity) noexcept {
				return const_cast<ComponentCacheItem*>(const_cast<const ComponentCache*>(this)->find(entity));
			}

			//! Returns the component cache item.
			//! \param entity Entity associated with the component item.
			//! \return Component info.
			//! \warning It is expected the component item exists! Undefined behavior otherwise.
			GAIA_NODISCARD const ComponentCacheItem& get(Entity entity) const noexcept {
				GAIA_ASSERT(!entity.pair());
				const auto* pItem = find(entity);
				GAIA_ASSERT(pItem != nullptr);
				return *pItem;
			}

			//! Returns the component cache item.
			//! \param entity Entity associated with the component item.
			//! \return Component info.
			//! \warning It is expected the component item exists! Undefined behavior otherwise.
			GAIA_NODISCARD ComponentCacheItem& get(Entity entity) noexcept {
				auto* pItem = find(entity);
				GAIA_ASSERT(pItem != nullptr);
				return *pItem;
			}

		private:
			//! Returns the registered symbol name used as the component identity.
			//! \param item Component cache item to inspect.
			//! \return Registered symbol name as a non-owning string view.
			GAIA_NODISCARD util::str_view symbol_name(const ComponentCacheItem& item) const noexcept {
				return {item.name.str(), item.name.len()};
			}

			//! Returns the path name used for scoped lookup.
			//! \param item Component cache item to inspect.
			//! \return Path name as a non-owning string view. Empty view when no path is registered.
			GAIA_NODISCARD util::str_view path_name(const ComponentCacheItem& item) const noexcept {
				return item.path.view();
			}

			//! Returns the preferred user-facing component name for diagnostics, logs and other pretty output.
			//! Path name is preferred when unique and not shadowed by a registered symbol.
			//! Otherwise the registered symbol name is returned.
			//! \param item Component cache item to inspect.
			//! \return Display name as a non-owning string view.
			//! \warning This is not a stable identity key.
			GAIA_NODISCARD util::str_view display_name(const ComponentCacheItem& item) const noexcept {
				const auto symbol = symbol_name(item);
				if (is_internal_symbol(symbol))
					return symbol;

				const auto path = path_name(item);
				if (!path.empty()) {
					const auto pathIt = m_compByPath.find(lookup_key(path));
					const auto symbolIt = m_compBySymbol.find(lookup_key(path));
					const bool pathIsUnique = pathIt != m_compByPath.end() && pathIt->second == &item;
					const bool pathShadowed = symbolIt != m_compBySymbol.end() && symbolIt->second != &item;
					if (pathIsUnique && !pathShadowed)
						return path;
				}

				return symbol;
			}

			//! Assigns a component path name used by path lookup and display_name().
			//! Passing nullptr or an empty string clears the path.
			//! \param item Component cache item to modify.
			//! \param name Path name.
			//! \param len Path length. If zero, the length is calculated.
			//! \return True when the path metadata was updated, false if validation failed.
			bool path(ComponentCacheItem& item, const char* name, uint32_t len = 0) noexcept {
				if (name == nullptr || name[0] == 0) {
					item.path.clear();
					rebuild_resolved_name_maps();
					return true;
				}

				const auto l = len == 0 ? (uint32_t)GAIA_STRLEN(name, ComponentCacheItem::MaxNameLength) : len;
				if (l == 0 || l >= ComponentCacheItem::MaxNameLength)
					return false;

				item.path.assign(name, l);
				rebuild_resolved_name_maps();
				return true;
			}

			//! Searches for the component cache item by its exact registered symbol name.
			//! \param name A null-terminated string.
			//! \param len String length. If zero, the length is calculated.
			//! \return Component cache item if found, nullptr otherwise.
			GAIA_NODISCARD const ComponentCacheItem* symbol(const char* name, uint32_t len = 0) const noexcept {
				GAIA_ASSERT(name != nullptr);

				const auto l = len == 0 ? (uint32_t)GAIA_STRLEN(name, ComponentCacheItem::MaxNameLength) : len;
				GAIA_ASSERT(l < ComponentCacheItem::MaxNameLength);

				const auto it = m_compBySymbol.find(ComponentCacheItem::SymbolLookupKey(name, l, 0));
				return it != m_compBySymbol.end() ? it->second : nullptr;
			}

			//! Searches for the component cache item by its exact path name.
			//! \param name A null-terminated string.
			//! \param len String length. If zero, the length is calculated.
			//! \return Component cache item if found, nullptr otherwise.
			GAIA_NODISCARD const ComponentCacheItem* path(const char* name, uint32_t len = 0) const noexcept {
				GAIA_ASSERT(name != nullptr);

				const auto l = len == 0 ? (uint32_t)GAIA_STRLEN(name, ComponentCacheItem::MaxNameLength) : len;
				GAIA_ASSERT(l < ComponentCacheItem::MaxNameLength);

				const auto it = m_compByPath.find(ComponentCacheItem::SymbolLookupKey(name, l, 0));
				return it != m_compByPath.end() ? it->second : nullptr;
			}

			//! Searches for the component cache item by its unique short symbol name.
			//! The short name is the leaf segment after the last `::` in the registered symbol.
			//! \param name A null-terminated string.
			//! \param len String length. If zero, the length is calculated.
			//! \return Component cache item if found and unique, nullptr otherwise.
			GAIA_NODISCARD const ComponentCacheItem* short_symbol(const char* name, uint32_t len = 0) const noexcept {
				GAIA_ASSERT(name != nullptr);

				const auto l = len == 0 ? (uint32_t)GAIA_STRLEN(name, ComponentCacheItem::MaxNameLength) : len;
				GAIA_ASSERT(l < ComponentCacheItem::MaxNameLength);

				const auto it = m_compByShortSymbol.find(ComponentCacheItem::SymbolLookupKey(name, l, 0));
				return it != m_compByShortSymbol.end() ? it->second : nullptr;
			}

			GAIA_NODISCARD ComponentCacheItem* symbol(const char* name, uint32_t len = 0) noexcept {
				return const_cast<ComponentCacheItem*>(const_cast<const ComponentCache*>(this)->symbol(name, len));
			}

			GAIA_NODISCARD ComponentCacheItem* path(const char* name, uint32_t len = 0) noexcept {
				return const_cast<ComponentCacheItem*>(const_cast<const ComponentCache*>(this)->path(name, len));
			}

			GAIA_NODISCARD ComponentCacheItem* short_symbol(const char* name, uint32_t len = 0) noexcept {
				return const_cast<ComponentCacheItem*>(const_cast<const ComponentCache*>(this)->short_symbol(name, len));
			}

			//! Resolves a component name within component metadata lookup only.
			//! Exact registered symbol lookup is attempted first, then exact path lookup,
			//! then unique short-symbol lookup.
			//! \param name A null-terminated string.
			//! \param len String length. If zero, the length is calculated.
			//! \return Component cache item if found, nullptr otherwise.
			//! \warning World entity-name lookup and active world scope rules are not considered here.
			GAIA_NODISCARD const ComponentCacheItem* resolve(const char* name, uint32_t len = 0) const noexcept {
				if (const auto* pItem = symbol(name, len); pItem != nullptr)
					return pItem;
				if (const auto* pItem = path(name, len); pItem != nullptr)
					return pItem;
				if (const auto* pItem = short_symbol(name, len); pItem != nullptr)
					return pItem;
				return nullptr;
			}

			GAIA_NODISCARD ComponentCacheItem* resolve(const char* name, uint32_t len = 0) noexcept {
				return const_cast<ComponentCacheItem*>(const_cast<const ComponentCache*>(this)->resolve(name, len));
			}

			//! Collects all component items that match @a name as an exact symbol, exact path or short symbol.
			//! This is primarily useful for low-level component metadata diagnostics.
			//! \param name Lookup string.
			//! \param[out] out Output array cleared and then filled with unique matching component items.
			//! \param len String length. If zero, the length is calculated.
			//! \warning World entity-name lookup and active world scope rules are not considered here.
			void resolve(cnt::darray<const ComponentCacheItem*>& out, const char* name, uint32_t len = 0) const {
				GAIA_ASSERT(name != nullptr);
				out.clear();

				const auto l = len == 0 ? (uint32_t)GAIA_STRLEN(name, ComponentCacheItem::MaxNameLength) : len;
				GAIA_ASSERT(l < ComponentCacheItem::MaxNameLength);
				const auto needle = util::str_view(name, l);

				auto push_unique = [&](const ComponentCacheItem* pItem) {
					if (pItem == nullptr)
						return;
					for (const auto* pExisting: out) {
						if (pExisting == pItem)
							return;
					}
					out.push_back(pItem);
				};

				push_unique(symbol(name, l));
				push_unique(short_symbol(name, l));

				if (const auto* pItem = path(name, l); pItem != nullptr) {
					push_unique(pItem);
				} else {
					for_each_item([&](const ComponentCacheItem& item) {
						if (item.path.view() == needle)
							push_unique(&item);
					});
				}
			}

			//! Returns the component cache item using resolved lookup.
			//! Exact registered symbol lookup is attempted first, followed by path lookup and alias lookup.
			//! \param name A null-terminated string
			//! \param len String length. If zero, the length is calculated
			//! \return Component info.
			//! \warning It is expected the component item with the given name/length exists! Undefined behavior otherwise.
			GAIA_NODISCARD const ComponentCacheItem& get(const char* name, uint32_t len = 0) const noexcept {
				const auto* pItem = resolve(name, len);
				GAIA_ASSERT(pItem != nullptr);
				return *pItem;
			}

			//! Returns the component cache item using resolved lookup.
			//! Exact registered symbol lookup is attempted first, followed by path lookup and alias lookup.
			//! \param name A null-terminated string
			//! \param len String length. If zero, the length is calculated
			//! \return Component info.
			//! \warning It is expected the component item with the given name/length exists! Undefined behavior otherwise.
			GAIA_NODISCARD ComponentCacheItem& get(const char* name, uint32_t len = 0) noexcept {
				auto* pItem = resolve(name, len);
				GAIA_ASSERT(pItem != nullptr);
				return *pItem;
			}

		public:
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
