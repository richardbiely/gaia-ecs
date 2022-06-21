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
			containers::map<uint64_t, const ComponentInfo*> m_info;
			containers::map<uint32_t, const ComponentInfo*> m_infoByIndex;

		public:
			ComponentCache() {
				// Reserve enough storage space for most use-cases
				m_info.reserve(2048);
				m_infoByIndex.reserve(2048);
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
				GAIA_SAFE_CONSTEXPR auto lookupHash = utils::type_info::hash<U>();

				const auto res = m_info.emplace(lookupHash, nullptr);
				if (res.second) {
					res.first->second = ComponentInfo::Create<U>();

					const auto index = utils::type_info::index<U>();
					m_infoByIndex.emplace(index, res.first->second);
				}

				const ComponentInfo* pMetaData = res.first->second;
				return pMetaData;
			}

			template <typename T>
			[[nodiscard]] const ComponentInfo* FindComponentInfo() const {
				using U = typename DeduceComponent<T>::Type;
				GAIA_SAFE_CONSTEXPR auto lookupHash = utils::type_info::hash<U>();

				const auto it = m_info.find(lookupHash);
				return it != m_info.end() ? it->second : (const ComponentInfo*)nullptr;
			}

			template <typename T>
			[[nodiscard]] const ComponentInfo* GetComponentInfo() const {
				using U = typename DeduceComponent<T>::Type;
				GAIA_SAFE_CONSTEXPR auto lookupHash = utils::type_info::hash<U>();

				// Let's assume the component has been registered via AddComponent already!
				GAIA_ASSERT(m_info.find(lookupHash) != m_info.end());
				return m_info.at(lookupHash);
			}

			[[nodiscard]] const ComponentInfo* GetComponentInfoFromIdx(uint32_t componentIndex) const {
				// Let's assume the component has been registered via AddComponent already!
				GAIA_ASSERT(m_infoByIndex.find(componentIndex) != m_infoByIndex.end());
				return m_infoByIndex.at(componentIndex);
			}

			template <typename T>
			[[nodiscard]] bool HasComponentInfo() const {
				using U = typename DeduceComponent<T>::Type;
				GAIA_SAFE_CONSTEXPR auto lookupHash = utils::type_info::hash<U>();

				return m_info.find(lookupHash) != m_info.end();
			}

			template <typename... T>
			[[nodiscard]] bool HasComponentMetaTypes() const {
				return (HasComponentInfo<T>() && ...);
			}

			template <typename... T>
			[[nodiscard]] bool HasAnyComponentMetaTypes() const {
				return (HasComponentInfo<T>() || ...);
			}

			template <typename... T>
			[[nodiscard]] bool HasNoneComponentMetaTypes() const {
				return (!HasComponentInfo<T>() && ...);
			}

			void Diag() const {
				const auto registeredTypes = (uint32_t)m_info.size();
				LOG_N("Registered infos: %u", registeredTypes);

				for (const auto& pair: m_info) {
					const auto* info = pair.second;
					LOG_N(
							"  (%p) index:%010u, lookupHash:%016" PRIx64 ", mask:%016" PRIx64 ", %.*s", (void*)info, info->infoIndex,
							info->lookupHash, info->matcherHash, (uint32_t)info->name.length(), info->name.data());
				}

				using DuplicateMap = containers::map<uint64_t, containers::darray<const ComponentInfo*>>;

				auto checkForDuplicates = [](const DuplicateMap& map, bool errIfDuplicate) {
					for (const auto& pair: map) {
						if (pair.second.size() <= 1)
							continue;

						if (errIfDuplicate) {
							LOG_E("Duplicity detected for key %016" PRIx64 "", pair.first);
						} else {
							LOG_N("Duplicity detected for key %016" PRIx64 "", pair.first);
						}

						for (const auto* info: pair.second) {
							if (info == nullptr)
								continue;

							LOG_N(
									"--> (%p) lookupHash:%016" PRIx64 ", mask:%016" PRIx64 ", index:%010u, %.*s", (void*)info,
									info->lookupHash, info->matcherHash, info->infoIndex, (uint32_t)info->name.length(),
									info->name.data());
						}
					}
				};

				// Lookup hash duplicity. These must be unique.
				{
					bool hasDuplicates = false;
					DuplicateMap m;
					for (const auto& pair: m_info) {
						const auto* info = pair.second;

						const auto it = m.find(info->lookupHash);
						if (it == m.end())
							m[info->lookupHash] = {info};
						else {
							it->second.push_back(info);
							hasDuplicates = true;
						}
					}

					if (hasDuplicates)
						checkForDuplicates(m, true);
				}

				// Component matcher hash duplicity. These are fine if not unique.
				// However, the more unique the lower the probability of us having
				// to check multiple archetype headers when matching queries.
				{
					bool hasDuplicates = false;
					DuplicateMap m;
					for (const auto& pair: m_info) {
						const auto* info = pair.second;

						const auto ret = m.emplace(info->matcherHash, containers::darray<const ComponentInfo*>{info});
						if (!ret.second) {
							ret.first->second.push_back(info);
							hasDuplicates = true;
						}
					}

					if (hasDuplicates)
						checkForDuplicates(m, false);
				}
			}

		private:
			void ClearRegisteredInfoCache() {
				for (auto& pair: m_info)
					delete pair.second;
				m_info.clear();
				m_infoByIndex.clear();
			}
		};
	} // namespace ecs
} // namespace gaia
