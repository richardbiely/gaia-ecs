#pragma once
#include "gaia/config/config.h"

#include <cinttypes>
#include <cstdint>
#include <cstring>
#include <type_traits>

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

			struct ResolvedLookupEntry {
				const ComponentCacheItem* pItem = nullptr;
				uint32_t matches = 0;
			};

			//! Lookup of component items by component entity index.
			cnt::map<uint32_t, const ComponentCacheItem*> m_compByEntityId;
			//! World-local component lookup keyed by compile-time metadata hash.
			cnt::map<ComponentLookupHash, const ComponentCacheItem*> m_compByTypeHash;

			//! Lookup of component items by their exact registered symbol name.
			cnt::map<ComponentCacheItem::SymbolLookupKey, const ComponentCacheItem*> m_compBySymbol;
			//! Lookup of component items by their exact path name.
			//! Ambiguous paths keep a tracked representative but remain lookup misses.
			cnt::map<ComponentCacheItem::SymbolLookupKey, ResolvedLookupEntry> m_compByPath;
			//! Lookup of component items by their unique short symbol name (leaf after the last `::`).
			//! Ambiguous short names keep a tracked representative but remain lookup misses.
			cnt::map<ComponentCacheItem::SymbolLookupKey, ResolvedLookupEntry> m_compByShortSymbol;

			//! Clears the contents of the component cache
			//! \warning Should be used only after worlds are cleared because it invalidates all currently
			//!          existing component ids. Any cached content would stop working.
			//! \note Hidden from users because clearing the cache means that all existing component entities
			//!       would lose connection to it and effectively become dangling. This means that a new
			//!       component of type T could be added with a new entity id.
			void clear() {
				for (auto [entityId, pItem]: m_compByEntityId) {
					(void)entityId;
					ComponentCacheItem::destroy(const_cast<ComponentCacheItem*>(pItem));
				}

				m_compByEntityId.clear();
				m_compByTypeHash.clear();
				m_compBySymbol.clear();
				m_compByPath.clear();
				m_compByShortSymbol.clear();
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

			GAIA_NODISCARD static util::str_view short_symbol_view(const ComponentCacheItem& item) noexcept {
				const auto symbol = util::str_view{item.name.str(), item.name.len()};
				if (is_internal_symbol(symbol))
					return {};

				const auto shortName = short_name_key(item);
				if (shortName.str() == nullptr || shortName.len() == 0)
					return {};

				return util::str_view(shortName.str(), shortName.len());
			}

			GAIA_NODISCARD static ComponentCacheItem::SymbolLookupKey lookup_key(util::str_view value) noexcept {
				return value.empty() ? ComponentCacheItem::SymbolLookupKey()
														 : ComponentCacheItem::SymbolLookupKey(value.data(), value.size(), 0);
			}

			GAIA_NODISCARD static util::str_view normalize_name_view(const char* name, uint32_t len = 0) noexcept {
				if (name == nullptr)
					return {};

				const auto l = len == 0 ? (uint32_t)GAIA_STRLEN(name, ComponentCacheItem::MaxNameLength) : len;
				GAIA_ASSERT(l < ComponentCacheItem::MaxNameLength);
				return util::str_view(name, l);
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

			static void add_lookup_mapping(
					cnt::map<ComponentCacheItem::SymbolLookupKey, ResolvedLookupEntry>& map, util::str_view view,
					const ComponentCacheItem& item) {
				if (view.empty())
					return;

				const auto key = lookup_key(view);
				const auto it = map.find(key);
				if (it == map.end()) {
					map.emplace(key, ResolvedLookupEntry{&item, 1});
					return;
				}

				auto& entry = it->second;
				GAIA_ASSERT(entry.matches != 0);
				++entry.matches;
			}

			void add_path_mapping(const ComponentCacheItem& item) {
				add_lookup_mapping(m_compByPath, item.path.view(), item);
			}

			static void push_unique_entity(cnt::darray<Entity>& out, Entity entity) {
				if (entity == EntityBad)
					return;

				for (const auto existing: out) {
					if (existing == entity)
						return;
				}

				out.push_back(entity);
			}

			void remove_path_mapping(const ComponentCacheItem& item) {
				const auto pathValue = item.path.view();
				if (pathValue.empty())
					return;

				const auto key = lookup_key(pathValue);
				const auto it = m_compByPath.find(key);
				if (it == m_compByPath.end())
					return;

				auto& entry = it->second;
				GAIA_ASSERT(entry.matches != 0);

				if (entry.matches == 1) {
					GAIA_ASSERT(entry.pItem == &item);
					m_compByPath.erase(it);
					return;
				}

				--entry.matches;
				if (entry.pItem != &item)
					return;

				entry.pItem = nullptr;
				for (const auto& [entityId, pItem]: m_compByEntityId) {
					(void)entityId;
					GAIA_ASSERT(pItem != nullptr);
					if (pItem == &item)
						continue;

					if (pItem->path.view() != pathValue)
						continue;

					entry.pItem = pItem;
					return;
				}

				GAIA_ASSERT(false);
			}

			//! Adds all name-based lookup mappings for a component item.
			//! \param item Component cache item to register.
			//! \param scopePath Optional world-provided scope prefix used for default path generation.
			void add_name_mappings(ComponentCacheItem& item, util::str_view scopePath) {
				m_compBySymbol.emplace(item.name, &item);
				initialize_names(item, scopePath);
				add_path_mapping(item);
				add_lookup_mapping(m_compByShortSymbol, short_symbol_view(item), item);
			}

			//! Stores a newly created component item and updates all lookup tables.
			//! \param pItem Component cache item to store.
			//! \param scopePath Optional world-provided scope prefix used for default path generation.
			//! \return Stored component item.
			const ComponentCacheItem& add_item(const ComponentCacheItem* pItem, util::str_view scopePath) {
				GAIA_ASSERT(pItem != nullptr);
				GAIA_ASSERT(pItem->entity.id() == pItem->comp.id());
				m_compByEntityId.emplace(pItem->entity.id(), pItem);

				add_name_mappings(*const_cast<ComponentCacheItem*>(pItem), scopePath);
				return *pItem;
			}

			//! Searches for the component cache item given the compile-time metadata hash.
			//! \param hash Compile-time metadata hash.
			//! \return Component info or nullptr when not found.
			GAIA_NODISCARD const ComponentCacheItem* find_item(ComponentLookupHash hash) const noexcept {
				const auto it = m_compByTypeHash.find(hash);
				return it != m_compByTypeHash.end() ? it->second : nullptr;
			}

			//! Registers the compile-time metadata-hash mapping for a component item.
			//! \tparam T Component type.
			//! \param item Registered component cache item.
			template <typename T>
			void add_hash_mapping(const ComponentCacheItem& item) {
				const auto hash = detail::ComponentDesc<T>::hash_lookup();
				const auto it = m_compByTypeHash.find(hash);
				if (it == m_compByTypeHash.end()) {
					m_compByTypeHash.emplace(hash, &item);
					return;
				}

				GAIA_ASSERT(it->second == &item);
			}

		public:
			ComponentCache() = default;

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

				const auto* pItem = find_item(detail::ComponentDesc<T>::hash_lookup());
				if (pItem != nullptr)
					return *pItem;

				const auto* pNewItem = ComponentCacheItem::create<T>(entity);
				GAIA_ASSERT(entity.id() == pNewItem->comp.id());
				const auto& item = add_item(pNewItem, scopePath);
				add_hash_mapping<T>(item);
				return item;
			}

			//! Registers a runtime-defined component.
			//! \param entity Component entity to bind the cache record to.
			//! \param desc Plain component registration descriptor.
			//! \param scopePath Optional scoped path prefix used to derive the default component path.
			//! \return Component info.
			GAIA_NODISCARD const ComponentCacheItem&
			add(Entity entity, const ecs::ComponentDesc& desc, util::str_view scopePath = {}) {
				GAIA_ASSERT(entity != EntityBad);
				GAIA_ASSERT(!entity.pair());
				GAIA_ASSERT(!desc.name.empty());
				GAIA_ASSERT(desc.name.size() < ComponentCacheItem::MaxNameLength);

				{
					const auto* pExisting = symbol(desc.name);
					if (pExisting != nullptr)
						return *pExisting;
				}

				const auto* pItem = ComponentCacheItem::create(entity, desc);
				GAIA_ASSERT(entity.id() == pItem->comp.id());
				return add_item(pItem, scopePath);
			}

			//! Registers a runtime-defined component.
			//! \param entity Component entity to bind the cache record to.
			//! \param item Component item registration context.
			//! \param scopePath Optional scoped path prefix used to derive the default component path.
			//! \return Component info.
			GAIA_NODISCARD const ComponentCacheItem&
			add(Entity entity, const ComponentCacheItem::ComponentCacheItemCtx& item, util::str_view scopePath = {}) {
				ecs::ComponentDesc desc{};
				desc.name = item.name;
				desc.size = item.size;
				desc.alig = item.alig;
				desc.storageType = item.storageType;
				desc.soa = item.soa;
				desc.pSoaSizes = item.pSoaSizes;
				desc.hashLookup = item.hashLookup;
				desc.funcCtor = item.funcCtor;
				desc.funcMoveCtor = item.funcMoveCtor;
				desc.funcCopyCtor = item.funcCopyCtor;
				desc.funcDtor = item.funcDtor;
				desc.funcCopy = item.funcCopy;
				desc.funcMove = item.funcMove;
				desc.funcSwap = item.funcSwap;
				desc.funcCmp = item.funcCmp;
				desc.funcSave = item.funcSave;
				desc.funcLoad = item.funcLoad;
				return add(entity, desc, scopePath);
			}

			//! Searches for the component cache item.
			//! \param entity Entity associated with the component item.
			//! \return Component cache item if found, nullptr otherwise.
			GAIA_NODISCARD const ComponentCacheItem* find(Entity entity) const noexcept {
				if (entity.pair())
					return nullptr;

				const auto it = m_compByEntityId.find(entity.id());
				return it != m_compByEntityId.end() ? it->second : nullptr;
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

			//! Assigns a component path name used by path lookup and display_name().
			//! Passing an empty string view clears the path.
			//! \param item Component cache item to modify.
			//! \param name Path name.
			//! \return True when the path metadata was updated, false if validation failed.
			bool path(ComponentCacheItem& item, util::str_view name) noexcept {
				if (path_name(item) == name)
					return true;

				if (name.empty()) {
					remove_path_mapping(item);
					item.path.clear();
					return true;
				}

				if (name.size() >= ComponentCacheItem::MaxNameLength)
					return false;

				remove_path_mapping(item);
				item.path.assign(name);
				add_path_mapping(item);
				return true;
			}

			//! Assigns a component path name used by path lookup and display_name().
			//! Passing nullptr or an empty string clears the path.
			//! \param item Component cache item to modify.
			//! \param name Path name.
			//! \param len Path length. If zero, the length is calculated.
			//! \return True when the path metadata was updated, false if validation failed.
			bool path(ComponentCacheItem& item, const char* name, uint32_t len = 0) noexcept {
				return path(item, normalize_name_view(name, len));
			}

			//! Searches for the component cache item by its exact registered symbol name.
			//! \param name Symbol name.
			//! \return Component cache item if found, nullptr otherwise.
			GAIA_NODISCARD const ComponentCacheItem* symbol(util::str_view name) const noexcept {
				GAIA_ASSERT(name.size() < ComponentCacheItem::MaxNameLength);
				const auto it = m_compBySymbol.find(lookup_key(name));
				return it != m_compBySymbol.end() ? it->second : nullptr;
			}

			//! Searches for the component cache item by its exact registered symbol name.
			//! \param name A null-terminated string.
			//! \param len String length. If zero, the length is calculated.
			//! \return Component cache item if found, nullptr otherwise.
			GAIA_NODISCARD const ComponentCacheItem* symbol(const char* name, uint32_t len = 0) const noexcept {
				GAIA_ASSERT(name != nullptr);
				return symbol(normalize_name_view(name, len));
			}

			//! Searches for the component cache item by its exact path name.
			//! \param name Path name.
			//! \return Component cache item if found, nullptr otherwise.
			GAIA_NODISCARD const ComponentCacheItem* path(util::str_view name) const noexcept {
				GAIA_ASSERT(name.size() < ComponentCacheItem::MaxNameLength);
				const auto it = m_compByPath.find(lookup_key(name));
				if (it == m_compByPath.end())
					return nullptr;

				return it->second.matches == 1 ? it->second.pItem : nullptr;
			}

			//! Searches for the component cache item by its exact path name.
			//! \param name A null-terminated string.
			//! \param len String length. If zero, the length is calculated.
			//! \return Component cache item if found, nullptr otherwise.
			GAIA_NODISCARD const ComponentCacheItem* path(const char* name, uint32_t len = 0) const noexcept {
				GAIA_ASSERT(name != nullptr);
				return path(normalize_name_view(name, len));
			}

			//! Searches for the component cache item by its unique short symbol name.
			//! The short name is the leaf segment after the last `::` in the registered symbol.
			//! \param name Short symbol name.
			//! \return Component cache item if found and unique, nullptr otherwise.
			GAIA_NODISCARD const ComponentCacheItem* short_symbol(util::str_view name) const noexcept {
				GAIA_ASSERT(name.size() < ComponentCacheItem::MaxNameLength);
				const auto it = m_compByShortSymbol.find(lookup_key(name));
				if (it == m_compByShortSymbol.end())
					return nullptr;

				return it->second.matches == 1 ? it->second.pItem : nullptr;
			}

			//! Searches for the component cache item by its unique short symbol name.
			//! The short name is the leaf segment after the last `::` in the registered symbol.
			//! \param name A null-terminated string.
			//! \param len String length. If zero, the length is calculated.
			//! \return Component cache item if found and unique, nullptr otherwise.
			GAIA_NODISCARD const ComponentCacheItem* short_symbol(const char* name, uint32_t len = 0) const noexcept {
				GAIA_ASSERT(name != nullptr);
				return short_symbol(normalize_name_view(name, len));
			}

			//! Appends every component entity whose scoped path exactly matches @a name.
			//! Unique matches append one entity. Ambiguous matches append all matching component entities.
			//! \param out Output array.
			//! \param name Exact scoped path to inspect.
			void add_path_matches(cnt::darray<Entity>& out, util::str_view name) const {
				if (name.empty())
					return;

				const auto it = m_compByPath.find(lookup_key(name));
				if (it == m_compByPath.end())
					return;

				const auto& entry = it->second;
				GAIA_ASSERT(entry.matches != 0);

				if (entry.matches == 1) {
					GAIA_ASSERT(entry.pItem != nullptr);
					push_unique_entity(out, entry.pItem->entity);
					return;
				}

				for (const auto& [entityId, pItem]: m_compByEntityId) {
					(void)entityId;
					GAIA_ASSERT(pItem != nullptr);
					if (pItem->path.view() == name)
						push_unique_entity(out, pItem->entity);
				}
			}

		public:
			//! Searches for the component item for \tparam T.
			//! \warning It is expected the component already exists! Undefined behavior otherwise.
			//! \return Component info or nullptr if not found.
			template <typename T>
			GAIA_NODISCARD const ComponentCacheItem* find() const noexcept {
				static_assert(!is_pair<T>::value);
				return find_item(detail::ComponentDesc<T>::hash_lookup());
			}

			//! Returns the component item for \tparam T.
			//! \warning It is expected the component already exists! Undefined behavior otherwise.
			//! \return Component info
			template <typename T>
			GAIA_NODISCARD const ComponentCacheItem& get() const noexcept {
				static_assert(!is_pair<T>::value);
				const auto* pItem = find<T>();
				GAIA_ASSERT(pItem != nullptr);
				return *pItem;
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
				const auto registeredTypes = m_compByEntityId.size();
				GAIA_LOG_N("Registered components: %u", registeredTypes);

				auto logDesc = [](const ComponentCacheItem& item) {
					GAIA_LOG_N(
							"    hash:%016" PRIx64 ", size:%3u B, align:%3u B, [%u:%u] %s [%s]", item.hashLookup.hash,
							item.comp.size(), item.comp.alig(), item.entity.id(), item.entity.gen(), item.name.str(),
							EntityKindString[item.entity.kind()]);
				};
				for (const auto& [entityId, pItem]: m_compByEntityId) {
					(void)entityId;
					GAIA_ASSERT(pItem != nullptr);
					logDesc(*pItem);
				}
			}
		};
	} // namespace ecs
} // namespace gaia
