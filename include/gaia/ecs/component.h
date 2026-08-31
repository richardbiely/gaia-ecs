#pragma once
#include "gaia/config/config.h"

#include <cstdint>
#include <type_traits>

#include "gaia/core/hashing_policy.h"
#include "gaia/core/utility.h"
#include "gaia/ecs/id.h"
#include "gaia/mem/data_layout_policy.h"
#include "gaia/meta/type_info.h"

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
		using EntitySpan = std::span<const Entity>;
		using EntitySpanMut = std::span<Entity>;
		using ComponentSpan = std::span<const Component>;
		using ChunkDataOffsetSpan = std::span<const ChunkDataOffset>;
		using SortComponentCond = core::is_smaller<Entity>;

		//! True when the component payload uses table storage inside archetype chunks.
		//! Sparse AoS components are the notable false case: their id may still fragment,
		//! but the payload itself uses sparse storage.
		GAIA_NODISCARD constexpr bool component_uses_table_storage(Component component) noexcept {
			return component.size() != 0U && (component.storage_type() != DataStorageType::Sparse || component.soa() != 0U);
		}

		//! True when the component payload is stored in sparse storage.
		//! Today this is used by sparse AoS components, regardless of whether the id
		//! itself remains fragmenting or is marked DontFragment.
		GAIA_NODISCARD constexpr bool component_uses_sparse_storage(Component component) noexcept {
			return component.size() != 0U && component.storage_type() == DataStorageType::Sparse && component.soa() == 0U;
		}

		//! Returns the effective payload descriptor for an archetype term.
		//! Pairs always keep their payload in the archetype table, even when the component type
		//! supplying their payload requests sparse storage.
		//! \param term Archetype term using the payload descriptor.
		//! \param component Registered payload descriptor.
		//! \return Descriptor with the storage mode used by the archetype.
		GAIA_NODISCARD constexpr Component archetype_component(Entity term, Component component) noexcept {
			if (component.storage_type() != DataStorageType::Sparse || !term.pair())
				return component;

			return Component(component.id(), component.soa(), component.size(), component.alig(), DataStorageType::Table);
		}

		//! \cond INTERNAL
		namespace detail {
			template <typename, typename = void>
			struct auto_storage_policy_inter {
				static constexpr DataStorageType data_storage_type = DataStorageType::Table;
			};
			template <typename T>
			struct auto_storage_policy_inter<T, std::void_t<decltype(T::gaia_Data_Storage)>> {
				static constexpr DataStorageType data_storage_type = T::gaia_Data_Storage;
			};
		} // namespace detail
		//! \endcond

		//! Returns the storage mode requested by a C++ component type.
		//! \tparam T Component payload type.
		template <typename T>
		inline constexpr DataStorageType auto_storage_policy_v = detail::auto_storage_policy_inter<T>::data_storage_type;

		//! \cond INTERNAL
		namespace detail {
			template <typename T, bool IsEntity = std::is_same_v<std::remove_cv_t<std::remove_reference_t<T>>, Entity>>
			struct uses_compile_time_sparse_storage: std::false_type {};

			template <typename T>
			struct uses_compile_time_sparse_storage<T, false> {
				using Arg = std::remove_cv_t<std::remove_reference_t<T>>;
				using FT = typename component_type_t<Arg>::TypeFull;
				using U = typename actual_type_t<Arg>::Type;

				static constexpr bool value = !is_pair<FT>::value && entity_kind_v<Arg> == EntityKind::EK_Gen &&
															 !mem::is_soa_layout_v<U> && auto_storage_policy_v<U> == DataStorageType::Sparse;
			};
		} // namespace detail
		//! \endcond

		//! True when a typed component uses Gaia's compile-time sparse payload path.
		//! Pair, unique, and SoA component forms remain table-backed even when their payload type requests sparse storage.
		//! \tparam T Component API type.
		template <typename T>
		inline constexpr bool uses_compile_time_sparse_storage_v = detail::uses_compile_time_sparse_storage<T>::value;

		//----------------------------------------------------------------------
		// Component verification
		//----------------------------------------------------------------------

		//! \cond INTERNAL
		namespace detail {
			template <typename T>
			struct is_component_size_valid: std::bool_constant<sizeof(T) < Component::MaxComponentSizeInBytes> {};

			template <typename T>
			struct is_component_type_valid:
					std::bool_constant<
							// SoA types need to be trivial. No restrictions otherwise.
							(!mem::is_soa_layout_v<T> || std::is_trivially_copyable_v<T>)> {};

		} // namespace detail
		//! \endcond

		//----------------------------------------------------------------------
		// Component verification
		//----------------------------------------------------------------------

		template <typename T>
		constexpr void verify_comp() {
			using U = typename actual_type_t<T>::TypeOriginal;

			// Make sure we only use this for "raw" types
			static_assert(
					core::is_raw_v<U>,
					"Components have to be \"raw\" types - no arrays, no const, reference, pointer or volatile");
			static_assert(detail::is_component_type_valid<U>::value, "SoA components must be trivially copyable");
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

		//! Calculates a lookup hash from the provided entities
		//! \param comps Span of entities
		//! \return Lookup hash
		GAIA_NODISCARD inline ComponentLookupHash calc_lookup_hash(EntitySpan comps) noexcept {
			const auto compsSize = comps.size();
			if (compsSize == 0)
				return {0};

			auto hash = core::calculate_hash64(comps[0].value());
			GAIA_FOR2(1, compsSize) {
				hash = core::hash_combine(hash, core::calculate_hash64(comps[i].value()));
			}
			return {hash};
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

		//! Located the index at which the provided component id is located in the component array
		//! \param comps Component view to search in
		//! \param entity Entity we search for
		//! \warning The component id must be present in the array
		GAIA_NODISCARD inline uint32_t comp_idx(std::span<const Entity> comps, Entity entity) {
			// We let the compiler know the upper iteration bound at compile-time.
			// This way it can optimize better (e.g. loop unrolling, vectorization).
			const auto cnt = (uint32_t)comps.size();
			GAIA_FOR(cnt) {
				if (comps[i] == entity)
					return i;
			}

			GAIA_ASSERT(false);
			return BadIndex;
		}
	} // namespace ecs
} // namespace gaia
