#pragma once
#include <cinttypes>
#include <string_view>
#include <type_traits>

#include "../containers/darray.h"
#include "../containers/map.h"
#include "../containers/sarray_ext.h"
#include "../ecs/common.h"
#include "../utils/data_layout_policy.h"
#include "../utils/hashing_policy.h"
#include "../utils/span.h"
#include "../utils/type_info.h"
#include "../utils/utility.h"

namespace gaia {
	namespace ecs {

		static constexpr uint32_t MAX_COMPONENTS_SIZE_BITS = 8;
		//! Maximum size of components in bytes
		static constexpr uint32_t MAX_COMPONENTS_SIZE = (1 << MAX_COMPONENTS_SIZE_BITS) - 1;

		enum ComponentType : uint8_t {
			// General purpose component
			CT_Generic = 0,
			// Chunk component
			CT_Chunk,
			// Number of component types
			CT_Count
		};

		inline const char* ComponentTypeString[ComponentType::CT_Count] = {"Generic", "Chunk"};

		struct ComponentInfo;

		//----------------------------------------------------------------------
		// Component type deduction
		//----------------------------------------------------------------------

		template <typename T>
		struct AsChunk {
			using __Type = typename std::decay_t<typename std::remove_pointer_t<T>>;
			using __TypeOriginal = T;
			static constexpr ComponentType __ComponentType = ComponentType::CT_Chunk;
		};

		namespace detail {
			template <typename T>
			struct ExtractComponentType_Generic {
				using Type = typename std::decay_t<typename std::remove_pointer_t<T>>;
				using TypeOriginal = T;
			};
			template <typename T>
			struct ExtractComponentType_NonGeneric {
				using Type = typename T::__Type;
				using TypeOriginal = typename T::__TypeOriginal;
			};
		} // namespace detail

		template <typename T, typename = void>
		struct IsGenericComponent: std::true_type {};
		template <typename T>
		struct IsGenericComponent<T, decltype((void)T::__ComponentType, void())>: std::false_type {};

		template <typename T>
		using DeduceComponent = std::conditional_t<
				IsGenericComponent<T>::value, typename detail::ExtractComponentType_Generic<T>,
				typename detail::ExtractComponentType_NonGeneric<T>>;

		template <typename T>
		struct IsReadOnlyType:
				std::bool_constant<
						std::is_const<std::remove_reference_t<std::remove_pointer_t<T>>>::value ||
						(!std::is_pointer<T>::value && !std::is_reference<T>::value)> {};

		//----------------------------------------------------------------------
		// Component verification
		//----------------------------------------------------------------------

		template <typename T>
		struct ComponentSizeValid: std::bool_constant<sizeof(T) < MAX_COMPONENTS_SIZE> {};

		template <typename T>
		struct ComponentTypeValid:
				std::bool_constant<std::is_trivially_copyable<T>::value && std::is_default_constructible<T>::value> {};

		template <typename T>
		constexpr void VerifyComponent() {
			using U = typename DeduceComponent<T>::Type;
			// Make sure we only use this for "raw" types
			static_assert(!std::is_const<U>::value);
			static_assert(!std::is_pointer<U>::value);
			static_assert(!std::is_reference<U>::value);
			static_assert(!std::is_volatile<U>::value);
			static_assert(ComponentSizeValid<U>::value, "MAX_COMPONENTS_SIZE in bytes is exceeded");
			static_assert(ComponentTypeValid<U>::value, "Only components of trivial type are allowed");
		}

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
		// ComponentInfo
		//----------------------------------------------------------------------

		struct ComponentInfo final {
			using FuncConstructor = void(void*);
			using FuncDestructor = void(void*);

			// TODO: Organize this in SaO way. Consider keeping commonly used data together.

			//! [ 0-15] Component name
			std::string_view name;
			//! [16-23] Complex hash used for look-ups
			uint64_t lookupHash;
			//! [24-31] Simple hash used for matching component
			uint64_t matcherHash;
			//! [32-39] Constructor to call when the component is being constructed
			FuncConstructor* constructor;
			//! [40-47] Destructor to call when the component is being destroyed
			FuncDestructor* destructor;

			//! [48-51] Unique component identifier
			uint32_t infoIndex;

			//! [52-55]
			struct {
				//! Component alignment
				uint32_t alig: MAX_COMPONENTS_SIZE_BITS;
				//! Component size
				uint32_t size: MAX_COMPONENTS_SIZE_BITS;
				//! Tells if the component is laid out in SoA style
				uint32_t soa : 1;
			} properties{};

			[[nodiscard]] bool operator==(const ComponentInfo& other) const {
				return lookupHash == other.lookupHash && infoIndex == other.infoIndex;
			}
			[[nodiscard]] bool operator!=(const ComponentInfo& other) const {
				return lookupHash != other.lookupHash || infoIndex != other.infoIndex;
			}
			[[nodiscard]] bool operator<(const ComponentInfo& other) const {
				return lookupHash < other.lookupHash;
			}

			template <typename T>
			[[nodiscard]] static constexpr ComponentInfo Calculate() {
				using U = typename DeduceComponent<T>::Type;

				ComponentInfo info{};
				info.name = utils::type_info::name<U>();
				info.lookupHash = utils::type_info::hash<U>();
				info.matcherHash = CalculateMatcherHash<U>();
				info.infoIndex = utils::type_info::index<U>();

				if constexpr (!std::is_empty<U>::value) {
					info.properties.alig = utils::auto_view_policy<U>::Alignment;
					info.properties.size = (uint32_t)sizeof(U);
					if constexpr (utils::is_soa_layout<U>::value) {
						info.properties.soa = 1;
					} else if constexpr (!std::is_trivial<T>::value) {
						info.constructor = [](void* ptr) {
							new (ptr) T{};
						};
						info.destructor = [](void* ptr) {
							((T*)ptr)->~T();
						};
					}
				}

				return info;
			}

			template <typename T>
			static const ComponentInfo* Create() {
				using U = std::decay_t<T>;
				return new ComponentInfo{Calculate<U>()};
			}
		};

		[[nodiscard]] inline uint64_t CalculateMatcherHash(uint64_t hashA, uint64_t hashB) noexcept {
			return utils::combine_or(hashA, hashB);
		}

		[[nodiscard]] inline uint64_t CalculateMatcherHash(std::span<const ComponentInfo*> infos) noexcept {
			uint64_t hash = infos.empty() ? 0 : infos[0]->matcherHash;
			for (uint32_t i = 1U; i < (uint32_t)infos.size(); ++i)
				hash = utils::combine_or(hash, infos[i]->matcherHash);
			return hash;
		}

		[[nodiscard]] inline uint64_t CalculateLookupHash(std::span<const ComponentInfo*> infos) noexcept {
			uint64_t hash = infos.empty() ? 0 : infos[0]->lookupHash;
			for (uint32_t i = 1U; i < (uint32_t)infos.size(); ++i)
				hash = utils::hash_combine(hash, infos[i]->lookupHash);
			return hash;
		}

		//----------------------------------------------------------------------

		struct ComponentInfoData final {
			//! Pointer to the associated component info
			const ComponentInfo* info;
		};

		struct ComponentLookupData final {
			//! Component info index. A copy of the value in ComponentInfo
			uint32_t infoIndex;
			//! Distance in bytes from the archetype's chunk data segment
			uint32_t offset;
		};

		using ComponentInfoList = containers::sarray_ext<ComponentInfoData, MAX_COMPONENTS_PER_ARCHETYPE>;
		using ComponentLookupList = containers::sarray_ext<ComponentLookupData, MAX_COMPONENTS_PER_ARCHETYPE>;

	} // namespace ecs
} // namespace gaia
