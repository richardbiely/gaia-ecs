#pragma once
#include <cinttypes>

#include "../core/hashing_policy.h"
#include "../core/utility.h"
#include "../mem/data_layout_policy.h"
#include "../meta/type_info.h"

namespace gaia {
	namespace ecs {
		enum ComponentKind : uint8_t {
			// General purpose component
			CK_Generic = 0,
			// Chunk component
			CK_Chunk,
			// Number of component types
			CK_Count
		};

		inline const char* const ComponentKindString[ComponentKind::CK_Count] = {"Generic", "Chunk"};

		using ComponentId = uint32_t;
		using ComponentLookupHash = core::direct_hash_key<uint64_t>;
		using ComponentMatcherHash = core::direct_hash_key<uint64_t>;
		using ComponentIdSpan = std::span<const ComponentId>;

		static constexpr ComponentId ComponentIdBad = (ComponentId)-1;
		static constexpr uint32_t MAX_COMPONENTS_SIZE_BITS = 8;
		static constexpr uint32_t MAX_COMPONENTS_SIZE_IN_BYTES = (1 << MAX_COMPONENTS_SIZE_BITS) - 1;

		//----------------------------------------------------------------------
		// Component type deduction
		//----------------------------------------------------------------------

		namespace detail {
			template <typename, typename = void>
			struct has_component_kind: std::false_type {};
			template <typename T>
			struct has_component_kind<T, std::void_t<decltype(T::Kind)>>: std::true_type {};

			template <typename T>
			struct ExtractComponentType_NoComponentKind {
				using Type = typename std::decay_t<typename std::remove_pointer_t<T>>;
				using TypeOriginal = T;
				static constexpr ComponentKind Kind = ComponentKind::CK_Generic;
			};
			template <typename T>
			struct ExtractComponentType_WithComponentKind {
				using Type = typename T::TKind;
				using TypeOriginal = typename T::TTypeOriginal;
				static constexpr ComponentKind Kind = T::Kind;
			};

			template <typename, typename = void>
			struct is_generic_component: std::true_type {};
			template <typename T>
			struct is_generic_component<T, std::void_t<decltype(T::Kind)>>:
					std::bool_constant<T::Kind == ComponentKind::CK_Generic> {};

			template <typename T>
			struct is_component_size_valid: std::bool_constant<sizeof(T) < MAX_COMPONENTS_SIZE_IN_BYTES> {};

			template <typename T>
			struct is_component_type_valid:
					std::bool_constant<
							// SoA types need to be trivial. No restrictions otherwise.
							(!mem::is_soa_layout_v<T> || std::is_trivially_copyable_v<T>)> {};

			template <typename T, typename = void>
			struct component_type {
				using type = typename detail::ExtractComponentType_NoComponentKind<T>;
			};
			template <typename T>
			struct component_type<T, std::void_t<decltype(T::Kind)>> {
				using type = typename detail::ExtractComponentType_WithComponentKind<T>;
			};

			template <typename T>
			struct is_component_mut:
					std::bool_constant<
							!std::is_const_v<std::remove_reference_t<std::remove_pointer_t<T>>> &&
							(std::is_pointer<T>::value || std::is_reference<T>::value)> {};
		} // namespace detail

		template <typename T>
		inline constexpr bool is_component_size_valid_v = detail::is_component_size_valid<T>::value;
		template <typename T>
		inline constexpr bool is_component_type_valid_v = detail::is_component_type_valid<T>::value;

		template <typename T>
		using component_type_t = typename detail::component_type<T>::type;
		template <typename T>
		inline constexpr ComponentKind component_kind_v = component_type_t<T>::Kind;

		//! Returns the component id for \tparam T
		//! \return Component id
		template <typename T>
		GAIA_NODISCARD inline ComponentId comp_id() {
			using U = typename component_type_t<T>::Type;
			return meta::type_info::id<U>();
		}

		template <typename T>
		inline constexpr bool is_component_mut_v = detail::is_component_mut<T>::value;

		//----------------------------------------------------------------------
		// Component verification
		//----------------------------------------------------------------------

		template <typename T>
		constexpr void verify_comp() {
			using U = typename component_type_t<T>::Type;
			// Make sure we only use this for "raw" types
			static_assert(!std::is_const_v<U>);
			static_assert(!std::is_pointer_v<U>);
			static_assert(!std::is_reference_v<U>);
			static_assert(!std::is_volatile_v<U>);
			static_assert(is_component_size_valid_v<U>, "MAX_COMPONENTS_SIZE_IN_BYTES in bytes is exceeded");
			static_assert(is_component_type_valid_v<U>, "Component type restrictions not met");
		}

		//----------------------------------------------------------------------
		// Component hash operations
		//----------------------------------------------------------------------

		namespace detail {
			template <typename T>
			constexpr uint64_t calc_matcher_hash() noexcept {
				return (uint64_t(1) << (meta::type_info::hash<T>() % uint64_t(63)));
			}
		} // namespace detail

		template <typename = void, typename...>
		constexpr ComponentMatcherHash calc_matcher_hash() noexcept;

		template <typename T, typename... Rest>
		GAIA_NODISCARD constexpr ComponentMatcherHash calc_matcher_hash() noexcept {
			if constexpr (sizeof...(Rest) == 0)
				return {detail::calc_matcher_hash<T>()};
			else
				return {core::combine_or(detail::calc_matcher_hash<T>(), detail::calc_matcher_hash<Rest>()...)};
		}

		template <>
		GAIA_NODISCARD constexpr ComponentMatcherHash calc_matcher_hash() noexcept {
			return {0};
		}

		//-----------------------------------------------------------------------------------

		template <typename Container>
		GAIA_NODISCARD constexpr ComponentLookupHash calc_lookup_hash(Container arr) noexcept {
			constexpr auto arrSize = arr.size();
			if constexpr (arrSize == 0) {
				return {0};
			} else {
				ComponentLookupHash::Type hash = arr[0];
				core::each<arrSize - 1>([&hash, &arr](auto i) {
					hash = core::hash_combine(hash, arr[i + 1]);
				});
				return {hash};
			}
		}

		template <typename = void, typename...>
		constexpr ComponentLookupHash calc_lookup_hash() noexcept;

		template <typename T, typename... Rest>
		GAIA_NODISCARD constexpr ComponentLookupHash calc_lookup_hash() noexcept {
			if constexpr (sizeof...(Rest) == 0)
				return {meta::type_info::hash<T>()};
			else
				return {core::hash_combine(meta::type_info::hash<T>(), meta::type_info::hash<Rest>()...)};
		}

		template <>
		GAIA_NODISCARD constexpr ComponentLookupHash calc_lookup_hash() noexcept {
			return {0};
		}

		template <typename T>
		struct AsChunk {
			using TKind = typename std::decay_t<typename std::remove_pointer_t<T>>;
			using TTypeOriginal = T;
			static constexpr ComponentKind Kind = ComponentKind::CK_Chunk;
		};
	} // namespace ecs
} // namespace gaia