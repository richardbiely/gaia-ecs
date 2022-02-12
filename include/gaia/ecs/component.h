#pragma once
#include <inttypes.h>
#include <string_view>
#include <type_traits>

#include "../config/config.h"
#include "../config/logging.h"
#include "../containers/darray.h"
#include "../containers/map.h"
#include "../containers/sarray_ext.h"
#include "../utils/data_layout_policy.h"
#include "../utils/hashing_policy.h"
#include "../utils/span.h"
#include "../utils/type_info.h"
#include "../utils/utility.h"
#include "common.h"
#include "fwd.h"

namespace gaia {
	namespace ecs {
		//----------------------------------------------------------------------
		// Helpers
		//----------------------------------------------------------------------

		static constexpr uint32_t MAX_COMPONENTS_SIZE_BITS = 8;
		//! Maximum size of components in bytes
		static constexpr uint32_t MAX_COMPONENTS_SIZE = (1 << MAX_COMPONENTS_SIZE_BITS) - 1;

		template <typename T>
		struct ComponentSizeValid: std::bool_constant<sizeof(T) < MAX_COMPONENTS_SIZE> {};

		template <typename T>
		struct ComponentTypeValid:
				std::bool_constant<std::is_trivially_copyable<T>::value && std::is_default_constructible<T>::value> {};

		template <typename... T>
		constexpr void VerifyComponents() {
			static_assert(utils::is_unique<std::decay_t<T>...>, "Only unique inputs are enabled");
			static_assert(
					std::conjunction_v<ComponentSizeValid<std::decay_t<T>>...>, "MAX_COMPONENTS_SIZE in bytes exceeded");
			static_assert(
					std::conjunction_v<ComponentTypeValid<std::decay_t<T>>...>, "Only components of trivial type are allowed");
		}

		enum ComponentType : uint8_t {
			// General purpose component
			CT_Generic = 0,
			// Chunk component
			CT_Chunk,
			// Number of component types
			CT_Count
		};

		inline const char* ComponentTypeString[ComponentType::CT_Count] = {"Generic", "Chunk"};

		struct ComponentMetaData;

		//----------------------------------------------------------------------
		// Component hash operations
		//----------------------------------------------------------------------

		namespace detail {
			template <typename T>
			constexpr uint64_t CalculateMatcherHash() noexcept {
				return (uint64_t(1) << (utils::type_info::hash<T>() % uint64_t(63)));
			}
		} // namespace detail

		template <typename = void, typename...>
		constexpr uint64_t CalculateMatcherHash() noexcept;

		template <typename T, typename... Rest>
		[[nodiscard]] constexpr uint64_t CalculateMatcherHash() noexcept {
			if constexpr (sizeof...(Rest) == 0)
				return detail::CalculateMatcherHash<T>();
			else
				return utils::combine_or(detail::CalculateMatcherHash<T>(), detail::CalculateMatcherHash<Rest>()...);
		}

		template <>
		[[nodiscard]] constexpr uint64_t CalculateMatcherHash() noexcept {
			return 0;
		}

		//-----------------------------------------------------------------------------------

		template <typename Container>
		[[nodiscard]] constexpr uint64_t CalculateLookupHash(Container arr) noexcept {
			constexpr auto arrSize = arr.size();
			if constexpr (arrSize == 0) {
				return 0;
			} else {
				uint64_t hash = arr[0];
				utils::for_each<arrSize - 1>([&hash, &arr](auto i) {
					hash = utils::hash_combine(hash, arr[i + 1]);
				});
				return hash;
			}
		}

		template <typename = void, typename...>
		constexpr uint64_t CalculateLookupHash() noexcept;

		template <typename T, typename... Rest>
		[[nodiscard]] constexpr uint64_t CalculateLookupHash() noexcept {
			if constexpr (sizeof...(Rest) == 0)
				return utils::type_info::hash<T>();
			else
				return utils::hash_combine(utils::type_info::hash<T>(), utils::type_info::hash<Rest>()...);
		}

		template <>
		[[nodiscard]] constexpr uint64_t CalculateLookupHash() noexcept {
			return 0;
		}

		//----------------------------------------------------------------------
		// ComponentMetaData
		//----------------------------------------------------------------------

		struct ComponentMetaData final {
			using FuncConstructor = void(void*);
			using FuncDestructor = void(void*);

			// TODO: Organize this in SaO way. Consider keeping commonly used data together.

			//! [ 0-15] Component name
			std::string_view name;
			//! [16-23] Complex hash used for look-ups
			uint64_t lookupHash;
			//! [24-31] Simple hash used for matching component
			uint64_t matcherHash;
			//! [32-39] Constructor to call when the type is being constructed
			FuncConstructor* constructor;
			//! [40-47] Destructor to call when the type is being destructed
			FuncDestructor* destructor;

			//! [48-51] Unique component identifier
			uint32_t typeIndex;

			//! [52-55]
			struct {
				//! Component alignment
				uint32_t alig: MAX_COMPONENTS_SIZE_BITS;
				//! Component size
				uint32_t size: MAX_COMPONENTS_SIZE_BITS;
				//! Tells if the component is laid out in SoA style
				uint32_t soa : 1;
			} info{};

			[[nodiscard]] bool operator==(const ComponentMetaData& other) const {
				return lookupHash == other.lookupHash && typeIndex == other.typeIndex;
			}
			[[nodiscard]] bool operator!=(const ComponentMetaData& other) const {
				return lookupHash != other.lookupHash || typeIndex != other.typeIndex;
			}
			[[nodiscard]] bool operator<(const ComponentMetaData& other) const {
				return lookupHash < other.lookupHash;
			}

			template <typename T>
			[[nodiscard]] static constexpr ComponentMetaData Calculate() {
				using TComponent = std::decay_t<T>;

				ComponentMetaData mth{};
				mth.name = utils::type_info::name<TComponent>();
				mth.lookupHash = utils::type_info::hash<TComponent>();
				mth.matcherHash = CalculateMatcherHash<TComponent>();
				mth.typeIndex = utils::type_info::index<TComponent>();

				if constexpr (!std::is_empty<TComponent>::value) {
					mth.info.alig = utils::auto_view_policy<TComponent>::Alignment;
					mth.info.size = (uint32_t)sizeof(TComponent);
					if constexpr (utils::is_soa_layout<TComponent>::value) {
						mth.info.soa = 1;
					} else if constexpr (!std::is_trivial<T>::value) {
						mth.constructor = [](void* ptr) {
							new (ptr) T{};
						};
						mth.destructor = [](void* ptr) {
							((T*)ptr)->~T();
						};
					}
				}

				return mth;
			}

			template <typename T>
			static const ComponentMetaData* Create() {
				using TComponent = std::decay_t<T>;
				return new ComponentMetaData{Calculate<TComponent>()};
			}
		};

		[[nodiscard]] inline uint64_t CalculateMatcherHash(uint64_t hashA, uint64_t hashB) noexcept {
			return utils::combine_or(hashA, hashB);
		}

		[[nodiscard]] inline uint64_t CalculateMatcherHash(std::span<const ComponentMetaData*> types) noexcept {
			uint64_t hash = types.empty() ? 0 : types[0]->matcherHash;
			for (uint32_t i = 1U; i < types.size(); ++i)
				hash = utils::combine_or(hash, types[i]->matcherHash);
			return hash;
		}

		[[nodiscard]] inline uint64_t CalculateLookupHash(std::span<const ComponentMetaData*> types) noexcept {
			uint64_t hash = types.empty() ? 0 : types[0]->lookupHash;
			for (uint32_t i = 1U; i < types.size(); ++i)
				hash = utils::hash_combine(hash, types[i]->lookupHash);
			return hash;
		}

		//----------------------------------------------------------------------

		struct ChunkComponentTypeInfo final {
			//! Pointer to the associated meta type
			const ComponentMetaData* type;
		};

		struct ChunkComponentLookupInfo final {
			//! Component lookup hash. A copy of the value in meta data
			uint32_t typeIndex;
			//! Distance in bytes from the archetype's chunk data segment
			uint32_t offset;
		};

		using ChunkComponentTypeList = containers::sarray_ext<ChunkComponentTypeInfo, MAX_COMPONENTS_PER_ARCHETYPE>;
		using ChunkComponentLookupList = containers::sarray_ext<ChunkComponentLookupInfo, MAX_COMPONENTS_PER_ARCHETYPE>;

		//-----------------------------------------------------------------------------------

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
				const auto componentIndex = utils::type_info::index<TComponent>();
				return m_types.find(componentIndex) != m_types.end();
			}

			void Diag() const {
				const auto registeredTypes = (uint32_t)m_types.size();
				LOG_N("Registered types: %u", registeredTypes);

				for (const auto& pair: m_types) {
					const auto* type = pair.second;
					LOG_N(
							"  %-16.*s (%p) --> index:%010u, lookupHash:%016" PRIx64 ", matcherHash:%016" PRIx64 "",
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
									"--> (%p) lookupHash:%016" PRIx64 ", matcherHash:%016" PRIx64 ", index:%010u, %.*s", (void*)type,
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

		//-----------------------------------------------------------------------------------

#define GAIA_ECS_COMPONENT_CACHE_H_INIT gaia::ecs::ComponentCache gaia::ecs::g_ComponentCache;

		extern ComponentCache g_ComponentCache;

		//-----------------------------------------------------------------------------------

	} // namespace ecs
} // namespace gaia
