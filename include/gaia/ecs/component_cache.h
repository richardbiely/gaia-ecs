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
			containers::map<uint64_t, const ComponentMetaData*> m_types;
			containers::map<uint32_t, const ComponentMetaData*> m_typesByIndex;

		public:
			ComponentCache() {
				// Reserve enough storage space for most use-cases
				m_types.reserve(2048);
				m_typesByIndex.reserve(2048);
			}

			ComponentCache(ComponentCache&&) = delete;
			ComponentCache(const ComponentCache&) = delete;
			ComponentCache& operator=(ComponentCache&&) = delete;
			ComponentCache& operator=(const ComponentCache&) = delete;

			~ComponentCache() {
				ClearRegisteredTypeCache();
			}

			template <typename T>
			[[nodiscard]] const ComponentMetaData* GetOrCreateComponentMetaType() {
				using TComponent = std::decay_t<T>;
				const auto lookupHash = utils::type_info::hash<TComponent>();

				const auto res = m_types.emplace(lookupHash, nullptr);
				if (res.second) {
					res.first->second = ComponentMetaData::Create<TComponent>();

					const auto index = utils::type_info::index<TComponent>();
					m_typesByIndex.emplace(index, res.first->second);
				}

				const ComponentMetaData* pMetaData = res.first->second;
				return pMetaData;
			}

			template <typename T>
			[[nodiscard]] const ComponentMetaData* FindComponentMetaType() const {
				using TComponent = std::decay_t<T>;
				const auto lookupHash = utils::type_info::hash<TComponent>();

				const auto it = m_types.find(lookupHash);
				return it != m_types.end() ? it->second : (const ComponentMetaData*)nullptr;
			}

			template <typename T>
			[[nodiscard]] const ComponentMetaData* GetComponentMetaType() const {
				using TComponent = std::decay_t<T>;
				const auto lookupHash = utils::type_info::hash<TComponent>();

				// Let's assume the component has been registered via AddComponent already!
				GAIA_ASSERT(m_types.find(lookupHash) != m_types.end());
				return m_types.at(lookupHash);
			}

			[[nodiscard]] const ComponentMetaData* GetComponentMetaTypeFromIdx(uint32_t componentIndex) const {
				// Let's assume the component has been registered via AddComponent already!
				GAIA_ASSERT(m_typesByIndex.find(componentIndex) != m_typesByIndex.end());
				return m_typesByIndex.at(componentIndex);
			}

			template <typename T>
			[[nodiscard]] bool HasComponentMetaType() const {
				using TComponent = std::decay_t<T>;
				const auto lookupHash = utils::type_info::hash<TComponent>();

				return m_types.find(lookupHash) != m_types.end();
			}

			template <typename... T>
			[[nodiscard]] bool HasComponentMetaTypes() const {
				return (HasComponentMetaType<T>() && ...);
			}

			template <typename... T>
			[[nodiscard]] bool HasAnyComponentMetaTypes() const {
				return (HasComponentMetaType<T>() || ...);
			}

			template <typename... T>
			[[nodiscard]] bool HasNoneComponentMetaTypes() const {
				return (!HasComponentMetaType<T>() && ...);
			}

			void Diag() const {
				const auto registeredTypes = (uint32_t)m_types.size();
				LOG_N("Registered types: %u", registeredTypes);

				for (const auto& pair: m_types) {
					const auto* type = pair.second;
					LOG_N(
							"  %-16.*s (%p) --> index:%010u, lookupHash:%016" PRIx64 ", mask:%016" PRIx64 "",
							(uint32_t)type->name.length(), type->name.data(), (void*)type, type->typeIndex, type->lookupHash,
							type->matcherHash);
				}

				using DuplicateMap = containers::map<uint64_t, containers::darray<const ComponentMetaData*>>;

				auto checkDuplicity = [](const DuplicateMap& map, bool errIfDuplicate) {
					for (const auto& pair: map) {
						if (pair.second.size() <= 1)
							continue;

						if (errIfDuplicate) {
							LOG_E("Duplicity detected for key %016" PRIx64 "", pair.first);
						} else {
							LOG_N("Duplicity detected for key %016" PRIx64 "", pair.first);
						}

						for (const auto* type: pair.second) {
							if (type == nullptr)
								continue;

							LOG_N(
									"--> (%p) lookupHash:%016" PRIx64 ", mask:%016" PRIx64 ", index:%010u, %.*s", (void*)type,
									type->lookupHash, type->matcherHash, type->typeIndex, (uint32_t)type->name.length(),
									type->name.data());
						}
					}
				};

				// Lookup hash duplicity. These must be unique.
				{
					bool hasDuplicates = false;
					DuplicateMap m;
					for (const auto& pair: m_types) {
						const auto* type = pair.second;

						const auto it = m.find(type->lookupHash);
						if (it == m.end())
							m[type->lookupHash] = {type};
						else {
							it->second.push_back(type);
							hasDuplicates = true;
						}
					}

					if (hasDuplicates)
						checkDuplicity(m, true);
				}

				// Component matcher hash duplicity. These are fine if not unique.
				// However, the more unique the lower the probability of us having
				// to check multiple archetype headers when matching queries.
				{
					bool hasDuplicates = false;
					DuplicateMap m;
					for (const auto& pair: m_types) {
						const auto* type = pair.second;

						const auto ret = m.emplace(type->matcherHash, containers::darray<const ComponentMetaData*>{type});
						if (!ret.second) {
							ret.first->second.push_back(type);
							hasDuplicates = true;
						}
					}

					if (hasDuplicates)
						checkDuplicity(m, false);
				}
			}

		private:
			void ClearRegisteredTypeCache() {
				for (auto& pair: m_types)
					delete pair.second;
				m_types.clear();
				m_typesByIndex.clear();
			}
		};
	} // namespace ecs
} // namespace gaia
