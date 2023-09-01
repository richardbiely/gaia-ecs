#pragma once
#include <cinttypes>

#include "../utils/data_layout_policy.h"
#include "../utils/hashing_policy.h"
#include "../utils/type_info.h"
#include "../utils/utility.h"

namespace gaia {
	namespace ecs {
		namespace component {

			enum ComponentType : uint8_t {
				// General purpose component
				CT_Generic = 0,
				// Chunk component
				CT_Chunk,
				// Number of component types
				CT_Count
			};

			inline const char* const ComponentTypeString[component::ComponentType::CT_Count] = {"Generic", "Chunk"};

			using ComponentId = uint32_t;
			using ComponentLookupHash = utils::direct_hash_key<uint64_t>;
			using ComponentMatcherHash = utils::direct_hash_key<uint64_t>;
			using ComponentIdSpan = std::span<const ComponentId>;

			static constexpr ComponentId ComponentIdBad = (ComponentId)-1;
			static constexpr uint32_t MAX_COMPONENTS_SIZE_BITS = 8;
			static constexpr uint32_t MAX_COMPONENTS_SIZE_IN_BYTES = (1 << MAX_COMPONENTS_SIZE_BITS) - 1;

			//----------------------------------------------------------------------
			// Component type deduction
			//----------------------------------------------------------------------

			namespace detail {
				template <typename T>
				struct ExtractComponentType_Generic {
					using Type = typename std::decay_t<typename std::remove_pointer_t<T>>;
					using TypeOriginal = T;
				};
				template <typename T>
				struct ExtractComponentType_NonGeneric {
					using Type = typename T::TType;
					using TypeOriginal = typename T::TTypeOriginal;
				};

				template <typename T, typename = void>
				struct IsGenericComponent_Internal: std::true_type {};
				template <typename T>
				struct IsGenericComponent_Internal<T, decltype((void)T::TComponentType, void())>: std::false_type {};

				template <typename T>
				struct IsComponentSizeValid_Internal: std::bool_constant<sizeof(T) < MAX_COMPONENTS_SIZE_IN_BYTES> {};

				template <typename T>
				struct IsComponentTypeValid_Internal:
						std::bool_constant<
								// SoA types need to be trivial. No restrictions otherwise.
								(!utils::is_soa_layout_v<T> || std::is_trivially_copyable_v<T>)> {};
			} // namespace detail

			template <typename T>
			inline constexpr bool IsGenericComponent = detail::IsGenericComponent_Internal<T>::value;
			template <typename T>
			inline constexpr bool IsComponentSizeValid = detail::IsComponentSizeValid_Internal<T>::value;
			template <typename T>
			inline constexpr bool IsComponentTypeValid = detail::IsComponentTypeValid_Internal<T>::value;

			template <typename T>
			using DeduceComponent = std::conditional_t<
					IsGenericComponent<T>, typename detail::ExtractComponentType_Generic<T>,
					typename detail::ExtractComponentType_NonGeneric<T>>;

			//! Returns the component id for \tparam T
			//! \return Component id
			template <typename T>
			GAIA_NODISCARD inline ComponentId GetComponentId() {
				using U = typename DeduceComponent<T>::Type;
				return utils::type_info::id<U>();
			}

			//! Returns the component id for \tparam T
			//! \return Component id
			template <typename T>
			GAIA_NODISCARD inline constexpr ComponentType GetComponentType() {
				if constexpr (IsGenericComponent<T>)
					return ComponentType::CT_Generic;
				else
					return ComponentType::CT_Chunk;
			}

			template <typename T>
			struct IsReadOnlyType:
					std::bool_constant<
							std::is_const_v<std::remove_reference_t<std::remove_pointer_t<T>>> ||
							(!std::is_pointer<T>::value && !std::is_reference<T>::value)> {};

			//----------------------------------------------------------------------
			// Component verification
			//----------------------------------------------------------------------

			template <typename T>
			constexpr void VerifyComponent() {
				using U = typename DeduceComponent<T>::Type;
				// Make sure we only use this for "raw" types
				static_assert(!std::is_const_v<U>);
				static_assert(!std::is_pointer_v<U>);
				static_assert(!std::is_reference_v<U>);
				static_assert(!std::is_volatile_v<U>);
				static_assert(IsComponentSizeValid<U>, "MAX_COMPONENTS_SIZE_IN_BYTES in bytes is exceeded");
				static_assert(IsComponentTypeValid<U>, "Component type restrictions not met");
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
			constexpr ComponentMatcherHash CalculateMatcherHash() noexcept;

			template <typename T, typename... Rest>
			constexpr GAIA_NODISCARD ComponentMatcherHash CalculateMatcherHash() noexcept {
				if constexpr (sizeof...(Rest) == 0)
					return {detail::CalculateMatcherHash<T>()};
				else
					return {utils::combine_or(detail::CalculateMatcherHash<T>(), detail::CalculateMatcherHash<Rest>()...)};
			}

			template <>
			constexpr GAIA_NODISCARD ComponentMatcherHash CalculateMatcherHash() noexcept {
				return {0};
			}

			//-----------------------------------------------------------------------------------

			template <typename Container>
			constexpr GAIA_NODISCARD ComponentLookupHash CalculateLookupHash(Container arr) noexcept {
				constexpr auto arrSize = arr.size();
				if constexpr (arrSize == 0) {
					return {0};
				} else {
					ComponentLookupHash::Type hash = arr[0];
					utils::for_each<arrSize - 1>([&hash, &arr](auto i) {
						hash = utils::hash_combine(hash, arr[i + 1]);
					});
					return {hash};
				}
			}

			template <typename = void, typename...>
			constexpr ComponentLookupHash CalculateLookupHash() noexcept;

			template <typename T, typename... Rest>
			constexpr GAIA_NODISCARD ComponentLookupHash CalculateLookupHash() noexcept {
				if constexpr (sizeof...(Rest) == 0)
					return {utils::type_info::hash<T>()};
				else
					return {utils::hash_combine(utils::type_info::hash<T>(), utils::type_info::hash<Rest>()...)};
			}

			template <>
			constexpr GAIA_NODISCARD ComponentLookupHash CalculateLookupHash() noexcept {
				return {0};
			}
		} // namespace component

		template <typename T>
		struct AsChunk {
			using TType = typename std::decay_t<typename std::remove_pointer_t<T>>;
			using TTypeOriginal = T;
			static constexpr component::ComponentType TComponentType = component::ComponentType::CT_Chunk;
		};
	} // namespace ecs
} // namespace gaia