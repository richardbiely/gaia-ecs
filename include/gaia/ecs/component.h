#pragma once
#include <inttypes.h>
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
			static_assert(utils::is_unique<std::decay_t<T>...>, "Unique components must be provided");
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

	} // namespace ecs
} // namespace gaia
