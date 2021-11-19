#pragma once
#include <inttypes.h>
#include <string_view>
#include <type_traits>
#include <vector>

#include "../config/config.h"
#include "../utils/data_layout_policy.h"
#include "../utils/hashing_policy.h"
#include "../utils/sarray.h"
#include "../utils/map.h"
#include "../utils/span.h"
#include "../utils/type_info.h"
#include "../utils/utility.h"
#include "common.h"
#include "fwd.h"

namespace gaia {
	namespace ecs {

#pragma region Helpers
		//! Maximum size of components in bytes
		static constexpr uint32_t MAX_COMPONENTS_SIZE = 255u;

		template <typename T>
		struct ComponentSizeValid:
				std::bool_constant<sizeof(T) < MAX_COMPONENTS_SIZE> {};

		template <typename T>
		struct ComponentTypeValid:
				std::bool_constant<
						std::is_trivially_copyable<T>::value &&
						std::is_default_constructible<T>::value> {};

		template <typename... T>
		constexpr void VerifyComponents() {
			static_assert(
					utils::is_unique<std::decay_t<T>...>,
					"Only unique inputs are enabled");
			static_assert(
					std::conjunction_v<ComponentSizeValid<std::decay_t<T>>...>,
					"Only component with size of at most MAX_COMPONENTS_SIZE are "
					"allowed");
			static_assert(
					std::conjunction_v<ComponentTypeValid<std::decay_t<T>>...>,
					"Only components of trivial type are allowed");
		}

		enum ComponentType : uint8_t {
			// General purpose component
			CT_Generic = 0,
			// Chunk component
			CT_Chunk,
			// Number of component types
			CT_Count
		};

		inline const char* ComponentTypeString[ComponentType::CT_Count] = {
				"Generic", "Chunk"};

#pragma endregion

		struct ComponentMetaData;

#pragma region Component hash operations

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
				return utils::combine_or(
						detail::CalculateMatcherHash<T>(),
						detail::CalculateMatcherHash<Rest>()...);
		};

		template <>
		[[nodiscard]] constexpr uint64_t CalculateMatcherHash() noexcept {
			return 0;
		};

		//-----------------------------------------------------------------------------------

		template <typename Container>
		[[nodiscard]] constexpr uint64_t
		CalculateLookupHash(Container arr) noexcept {
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
				return utils::hash_combine(
						utils::type_info::hash<T>(), utils::type_info::hash<Rest>()...);
		};

		template <>
		[[nodiscard]] constexpr uint64_t CalculateLookupHash() noexcept {
			return 0;
		};

#pragma endregion

#pragma region ComponentMetaData

		struct ComponentMetaData final {
			using FuncConstructor = void(void*);
			using FuncDestructor = void(void*);

			// TODO: Organize this in SaO way. Consider keeping commonly used data
			// together.

			//! [ 0-15] Component name
			std::string_view name;
			//! [16-23] Complex hash used for look-ups
			uint64_t lookupHash;
			//! [24-31] Simple hash used for matching component
			uint64_t matcherHash;
			//! [32-35] Unique component identifier
			uint32_t typeIndex;
			//! [36-39] Component alignment
			uint32_t alig;
			//! [40-43] Component size
			uint32_t size;
			//! [44] Tells if the component is laid out in SoA style
			bool soa;
			//! [48-63] Constructor to call when the type is being constructed
			FuncConstructor* constructor;
			//! [64-79] Destructor to call when the type is being destructed
			FuncDestructor* destructor;

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

				ComponentMetaData mth;
				mth.name = utils::type_info::name<TComponent>();
				mth.lookupHash = utils::type_info::hash<TComponent>();
				mth.matcherHash = CalculateMatcherHash<TComponent>();
				mth.typeIndex = utils::type_info::index<TComponent>();
				mth.alig = 0;
				mth.size = 0;
				mth.soa = false;
				mth.constructor = nullptr;
				mth.destructor = nullptr;

				if constexpr (!std::is_empty<TComponent>::value) {
					mth.alig = utils::auto_view_policy<TComponent>::Alignment;
					mth.size = (uint32_t)sizeof(TComponent);
					if constexpr (utils::is_soa_layout<TComponent>::value) {
						mth.soa = true;
					}
					else if constexpr (!std::is_trivial<T>::value) {
						mth.constructor = [](void* ptr) { new (ptr) T{}; };
						mth.destructor = [](void* ptr) { ((T*)ptr)->~T(); };
					}
				}

				return mth;
			};

			template <typename T>
			static const ComponentMetaData* Create() {
				using TComponent = std::decay_t<T>;
				return new ComponentMetaData(Calculate<TComponent>());
			};
		};

		[[nodiscard]] inline uint64_t
		CalculateMatcherHash(uint64_t hashA, uint64_t hashB) noexcept {
			return utils::combine_or(hashA, hashB);
		}

		[[nodiscard]] inline uint64_t
		CalculateMatcherHash(std::span<const ComponentMetaData*> types) noexcept {
			uint64_t hash = types.empty() ? 0 : types[0]->matcherHash;
			for (uint32_t i = 1; i < types.size(); ++i)
				hash = utils::combine_or(hash, types[i]->matcherHash);
			return hash;
		}

		[[nodiscard]] inline uint64_t
		CalculateLookupHash(std::span<const ComponentMetaData*> types) noexcept {
			uint64_t hash = types.empty() ? 0 : types[0]->lookupHash;
			for (uint32_t i = 1; i < types.size(); ++i)
				hash = utils::hash_combine(hash, types[i]->lookupHash);
			return hash;
		}

#pragma endregion

		struct ChunkComponentInfo final {
			//! Pointer to the associated meta type
			const ComponentMetaData* type;
			//! Distance in bytes from the archetype's chunk data segment
			uint32_t offset;
		};

		using ChunkComponentList =
				utils::sarray<ChunkComponentInfo, MAX_COMPONENTS_PER_ARCHETYPE>;

		//-----------------------------------------------------------------------------------

		class ComponentCache {
			utils::map<uint32_t, const ComponentMetaData*> m_types;

		public:
			ComponentCache() {
				// Reserve enough storage space for most use-cases
				m_types.reserve(2048);
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
				const auto componentIndex = utils::type_info::index<TComponent>();
				auto it = m_types.find(componentIndex);
				if (it != m_types.end())
					return it->second;

				const auto pMetaData = ComponentMetaData::Create<TComponent>();
				m_types.insert({componentIndex, pMetaData});
				return pMetaData;
			}

			template <typename T>
			[[nodiscard]] const ComponentMetaData* FindComponentMetaType() const {
				using TComponent = std::decay_t<T>;
				const auto componentIndex = utils::type_info::index<TComponent>();
				const auto it = m_types.find(componentIndex);
				return it != m_types.end() ? it->second
																	 : (const ComponentMetaData*)nullptr;
			}

			template <typename T>
			[[nodiscard]] const ComponentMetaData* GetComponentMetaType() const {
				using TComponent = std::decay_t<T>;
				const auto componentIndex = utils::type_info::index<TComponent>();
				// Let's assume the component has been registered via AddComponent
				// already!
				GAIA_ASSERT(m_types.find(componentIndex) != m_types.end());
				return m_types.at(componentIndex);
			}

			[[nodiscard]] const ComponentMetaData*
			GetComponentMetaTypeFromIdx(uint32_t componentIndex) const {
				// Let's assume the component has been registered via AddComponent
				// already!
				GAIA_ASSERT(m_types.find(componentIndex) != m_types.end());
				return m_types.at(componentIndex);
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
							"  %-16.*s (%p) --> index:%010u, lookupHash:%016llx, "
							"matcherHash:%016llx",
							(uint32_t)type->name.length(), type->name.data(), (void*)type,
							type->typeIndex, type->lookupHash, type->matcherHash);
				}

				using DuplicateMap =
						utils::map<uint64_t, std::vector<const ComponentMetaData*>>;

				auto checkDuplicity = [](const DuplicateMap& map, bool errIfDuplicate) {
					for (const auto& pair: map) {
						if (pair.second.size() <= 1)
							continue;

						if (errIfDuplicate) {
							LOG_E("Duplicity detected for key %016llx", pair.first);
						} else {
							LOG_N("Duplicity detected for key %016llx", pair.first);
						}

						for (const auto* type: pair.second) {
							if (type == nullptr)
								continue;

							LOG_N(
									"--> (%p) lookupHash:%016llx, matcherHash:%016llx, "
									"index:%010u, %.*s",
									(void*)type, type->lookupHash, type->matcherHash,
									type->typeIndex, (uint32_t)type->name.length(),
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

				// Component matcher hash duplicity. These are fine if not unique
				// However, the more unique the lower the probability of us having
				// to check multiple archetype headers when matching queries.
				{
					bool hasDuplicates = false;
					DuplicateMap m;
					for (const auto& pair: m_types) {
						const auto* type = pair.second;

						const auto it = m.find(type->matcherHash);
						if (it == m.end())
							m.insert({type->matcherHash, {type}});
						else {
							it->second.push_back(type);
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
			}
		};

		//-----------------------------------------------------------------------------------

#define GAIA_ECS_COMPONENT_CACHE_H_INIT                                        \
	gaia::ecs::ComponentCache gaia::ecs::g_ComponentCache;

		extern ComponentCache g_ComponentCache;

		//-----------------------------------------------------------------------------------

	} // namespace ecs
} // namespace gaia
