#pragma once
#include "../config/config.h"

#include <cstdint>
#include <type_traits>

#include "../core/hashing_policy.h"
#include "../core/utility.h"
#include "../mem/data_layout_policy.h"
#include "../meta/type_info.h"
#include "id.h"

namespace gaia {
	namespace ecs {
		//----------------------------------------------------------------------
		// Component-related types
		//----------------------------------------------------------------------

		using ComponentVersion = uint32_t;
		using ChunkDataVersionOffset = uint8_t;
		using CompOffsetMappingIndex = uint8_t;
		using ChunkDataOffset = uint16_t;
		using ComponentLookupHash = core::direct_hash_key<uint64_t>;
		using ComponentMatcherHash = core::direct_hash_key<uint64_t>;
		using EntitySpan = std::span<const Entity>;
		using ComponentSpan = std::span<const Component>;

		//----------------------------------------------------------------------
		// Component verification
		//----------------------------------------------------------------------

		namespace detail {
			template <typename T>
			struct is_component_size_valid: std::bool_constant<sizeof(T) < Component::MaxComponentSizeInBytes> {};

			template <typename T>
			struct is_component_type_valid:
					std::bool_constant<
							// SoA types need to be trivial. No restrictions otherwise.
							(!mem::is_soa_layout_v<T> || std::is_trivially_copyable_v<T>)> {};
		} // namespace detail

		//----------------------------------------------------------------------
		// Component type deduction
		//----------------------------------------------------------------------

		template <typename T>
		struct uni {
			static_assert(core::is_raw_v<T>);
			static_assert(
					std::is_trivial_v<T> ||
							// For non-trivial T the comparison operator must be implemented because
							// defragmentation needs it to figure out is entities can be moved around.
							(core::has_global_equals<T>::value || core::has_member_equals<T>::value),
					"Non-trivial Uni component must implement operator==");

			//! Component kind
			static constexpr EntityKind Kind = EntityKind::EK_Uni;

			//! Raw type with no additional sugar
			using TType = T;
			//! uni<TType>
			using TTypeFull = uni<TType>;
			//! Original template type
			using TTypeOriginal = T;
		};

		namespace detail {
			template <typename, typename = void>
			struct has_entity_kind: std::false_type {};
			template <typename T>
			struct has_entity_kind<T, std::void_t<decltype(T::Kind)>>: std::true_type {};

			template <typename T>
			struct ExtractComponentType_NoEntityKind {
				//! Component kind
				static constexpr EntityKind Kind = EntityKind::EK_Gen;

				//! Raw type with no additional sugar
				using Type = core::raw_t<T>;
				//!
				using TypeFull = Type;
				//! Original template type
				using TypeOriginal = T;
			};
			template <typename T>
			struct ExtractComponentType_WithEntityKind {
				//! Component kind
				static constexpr EntityKind Kind = T::Kind;

				//! Raw type with no additional sugar
				using Type = typename T::TType;
				//!
				using TypeFull = std::conditional_t<Kind == EntityKind::EK_Gen, Type, uni<Type>>;
				//! Original template type
				using TypeOriginal = typename T::TTypeOriginal;
			};

			template <typename, typename = void>
			struct is_gen_component: std::true_type {};
			template <typename T>
			struct is_gen_component<T, std::void_t<decltype(T::Kind)>>: std::bool_constant<T::Kind == EntityKind::EK_Gen> {};

			template <typename T, typename = void>
			struct component_type {
				using type = typename detail::ExtractComponentType_NoEntityKind<T>;
			};
			template <typename T>
			struct component_type<T, std::void_t<decltype(T::Kind)>> {
				using type = typename detail::ExtractComponentType_WithEntityKind<T>;
			};
		} // namespace detail

		template <typename T>
		using component_type_t = typename detail::component_type<T>::type;
		template <typename T>
		inline constexpr EntityKind entity_kind_v = component_type_t<T>::Kind;

		//----------------------------------------------------------------------
		// Component verification
		//----------------------------------------------------------------------

		template <typename T>
		constexpr void verify_comp() {
			using U = typename component_type_t<T>::TypeOriginal;

			// Make sure we only use this for "raw" types
			static_assert(
					core::is_raw_v<U>,
					"Components have to be \"raw\" types - no arrays, no const, reference, pointer or volatile");
		}

		//----------------------------------------------------------------------
		// Component matcher hash
		//----------------------------------------------------------------------

		namespace detail {
			inline constexpr uint64_t calc_matcher_hash(uint64_t type_hash) noexcept {
				return (uint64_t(1) << (type_hash % uint64_t(63)));
			}

			template <typename T>
			constexpr uint64_t calc_matcher_hash() noexcept {
				constexpr uint64_t type_hash = meta::type_info::hash<T>();
				return (uint64_t(1) << (type_hash % uint64_t(63)));
			}
		} // namespace detail

		inline constexpr ComponentMatcherHash calc_matcher_hash(uint64_t type_hash) noexcept {
			return {detail::calc_matcher_hash(type_hash)};
		}

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

		//----------------------------------------------------------------------
		// Component lookup hash
		//----------------------------------------------------------------------

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

		//! Located the index at which the provided component id is located in the component array
		//! \param pComps Pointer to the start of the component array
		//! \param entity Entity we search for
		//! \return Index of the component id in the array
		//! \warning The component id must be present in the array
		template <uint32_t MAX_COMPONENTS>
		GAIA_NODISCARD inline uint32_t comp_idx(const Entity* pComps, Entity entity) {
			// We let the compiler know the upper iteration bound at compile-time.
			// This way it can optimize better (e.g. loop unrolling, vectorization).
			GAIA_FOR(MAX_COMPONENTS) {
				if (pComps[i] == entity)
					return i;
			}

			GAIA_ASSERT(false);
			return BadIndex;
		}
	} // namespace ecs
} // namespace gaia