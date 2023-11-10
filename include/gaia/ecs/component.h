#pragma once
#include "../config/config.h"

#include <cinttypes>
#include <type_traits>

#include "../core/hashing_policy.h"
#include "../core/utility.h"
#include "../mem/data_layout_policy.h"
#include "../meta/type_info.h"

namespace gaia {
	namespace ecs {
		enum ComponentKind : uint8_t {
			// Generic component, one per entity
			CK_Gen = 0,
			// Unique component, one per chunk
			CK_Uni,
			// Number of component kinds
			CK_Count
		};

		inline const char* const ComponentKindString[ComponentKind::CK_Count] = {"Gen", "Uni"};

		//----------------------------------------------------------------------
		// Component
		//----------------------------------------------------------------------

		using ComponentId = uint32_t;
		static constexpr ComponentId ComponentIdBad = (ComponentId)-1;

		struct Component {
			using InternalType = uint64_t;

			static constexpr uint32_t MaxAlignment_Bits = 10;
			static constexpr uint32_t MaxAlignment = (1U << MaxAlignment_Bits) - 1;
			static constexpr uint32_t MaxComponentSize_Bits = 8;
			static constexpr uint32_t MaxComponentSizeInBytes = (1 << MaxComponentSize_Bits) - 1;
			static constexpr InternalType Bad = (InternalType)-1;

		private:
			struct ComponentData {
				//! Component identifier
				InternalType id : 32;
				//! Component is SoA
				InternalType soa: meta::StructToTupleMaxTypes_Bits;
				//! Component size
				InternalType size: MaxComponentSize_Bits;
				//! Component alignment
				InternalType alig: MaxAlignment_Bits;
			};
			static_assert(sizeof(ComponentData) == sizeof(InternalType));

			union {
				ComponentData data;
				InternalType val;
			};

		public:
			Component() noexcept = default;
			explicit Component(bool bad) noexcept {
				(void)bad;
				val = (InternalType)-1;
			}
			explicit Component(uint32_t id, uint32_t soa, uint32_t size, uint32_t alig) {
				data.id = id;
				data.soa = soa;
				data.size = size;
				data.alig = alig;
			}
			~Component() = default;

			Component(Component&&) noexcept = default;
			Component(const Component&) = default;
			Component& operator=(Component&&) noexcept = default;
			Component& operator=(const Component&) = default;

			GAIA_NODISCARD constexpr bool operator==(const Component& other) const noexcept {
				return val == other.val;
			}
			GAIA_NODISCARD constexpr bool operator!=(const Component& other) const noexcept {
				return val != other.val;
			}

			GAIA_NODISCARD auto id() const {
				return (uint32_t)data.id;
			}
			GAIA_NODISCARD auto soa() const {
				return (uint32_t)data.soa;
			}
			GAIA_NODISCARD auto size() const {
				return (uint32_t)data.size;
			}
			GAIA_NODISCARD auto alig() const {
				return (uint32_t)data.alig;
			}

			// We don't want automatic conversion from Component to ComponentId
			// operator ComponentId() const {
			// 	return id();
			// }

			bool operator<(Component other) const {
				return id() < other.id();
			}
		};

		//----------------------------------------------------------------------
		// ComponentBad
		//----------------------------------------------------------------------

		struct ComponentNull_t {
			GAIA_NODISCARD operator Component() const noexcept {
				return Component(false);
			}

			GAIA_NODISCARD constexpr bool operator==([[maybe_unused]] const ComponentNull_t& null) const noexcept {
				return true;
			}
			GAIA_NODISCARD constexpr bool operator!=([[maybe_unused]] const ComponentNull_t& null) const noexcept {
				return false;
			}
		};

		GAIA_NODISCARD inline bool operator==(const ComponentNull_t& null, const Component& comp) noexcept {
			return static_cast<Component>(null).id() == comp.id();
		}

		GAIA_NODISCARD inline bool operator!=(const ComponentNull_t& null, const Component& comp) noexcept {
			return static_cast<Component>(null).id() != comp.id();
		}

		GAIA_NODISCARD inline bool operator==(const Component& comp, const ComponentNull_t& null) noexcept {
			return null == comp;
		}

		GAIA_NODISCARD inline bool operator!=(const Component& comp, const ComponentNull_t& null) noexcept {
			return null != comp;
		}

		inline constexpr ComponentNull_t ComponentBad{};

		//----------------------------------------------------------------------
		// Component-related types
		//----------------------------------------------------------------------

		using ComponentVersion = uint32_t;
		using ChunkDataVersionOffset = uint8_t;
		using CompOffsetMappingIndex = uint8_t;
		using ChunkDataOffset = uint16_t;
		using ComponentLookupHash = core::direct_hash_key<uint64_t>;
		using ComponentMatcherHash = core::direct_hash_key<uint64_t>;
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

		namespace detail {
			template <typename, typename = void>
			struct has_component_kind: std::false_type {};
			template <typename T>
			struct has_component_kind<T, std::void_t<decltype(T::Kind)>>: std::true_type {};

			template <typename T>
			struct ExtractComponentType_NoComponentKind {
				//! Raw type with no additional sugar
				using Type = typename std::decay_t<typename std::remove_pointer_t<T>>;
				//! Original template type
				using TypeOriginal = T;
				//! Component kind
				static constexpr ComponentKind Kind = ComponentKind::CK_Gen;
			};
			template <typename T>
			struct ExtractComponentType_WithComponentKind {
				//! Raw type with no additional sugar
				using Type = typename T::TType;
				//! Original template type
				using TypeOriginal = typename T::TTypeOriginal;
				//! Component kind
				static constexpr ComponentKind Kind = T::Kind;
			};

			template <typename, typename = void>
			struct is_gen_component: std::true_type {};
			template <typename T>
			struct is_gen_component<T, std::void_t<decltype(T::Kind)>>:
					std::bool_constant<T::Kind == ComponentKind::CK_Gen> {};

			template <typename T, typename = void>
			struct component_type {
				using type = typename detail::ExtractComponentType_NoComponentKind<T>;
			};
			template <typename T>
			struct component_type<T, std::void_t<decltype(T::Kind)>> {
				using type = typename detail::ExtractComponentType_WithComponentKind<T>;
			};

			template <typename T>
			struct is_arg_mut:
					std::bool_constant<
							!std::is_const_v<core::rem_rp_t<T>> && (std::is_pointer<T>::value || std::is_reference<T>::value)> {};
		} // namespace detail

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
		inline constexpr bool is_arg_mut_v = detail::is_arg_mut<T>::value;

		template <typename T>
		inline constexpr bool is_raw_v = std::is_same_v<T, std::decay_t<std::remove_pointer_t<T>>>;

		template <typename T>
		struct uni {
			//! Raw type with no additional sugar
			using TType = typename std::decay_t<typename std::remove_pointer_t<T>>;
			//! Original template type
			using TTypeOriginal = T;
			//! Component kind
			static constexpr ComponentKind Kind = ComponentKind::CK_Uni;
		};

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
		}

		//----------------------------------------------------------------------
		// Component matcher hash
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
		//! \param compID Component id we search for
		//! \return Index of the component id in the array
		//! \warning The component id must be present in the array
		template <uint32_t MAX_COMPONENTS>
		GAIA_NODISCARD inline uint32_t comp_idx(const Component* pComps, ComponentId compId) {
			// We let the compiler know the upper iteration bound at compile-time.
			// This way it can optimize better (e.g. loop unrolling, vectorization).
			GAIA_FOR(MAX_COMPONENTS) {
				if (pComps[i].id() == compId)
					return i;
			}

			GAIA_ASSERT(false);
			return BadIndex;
		}

		//! Located the index at which the provided component id is located in the component array
		//! \param pCompIds Pointer to the start of the component id array
		//! \param compId Component id we search for
		//! \return Index of the component id in the array
		//! \warning The component id must be present in the array
		template <uint32_t MAX_COMPONENTS>
		GAIA_NODISCARD inline uint32_t comp_idx(const ComponentId* pCompIds, ComponentId compId) {
			// We let the compiler know the upper iteration bound at compile-time.
			// This way it can optimize better (e.g. loop unrolling, vectorization).
			GAIA_FOR(MAX_COMPONENTS) {
				if (pCompIds[i] == compId)
					return i;
			}

			GAIA_ASSERT(false);
			return BadIndex;

			// NOTE: This code does technically the same as the above.
			//       However, compilers can't quite optimize it as well because it does some more
			//       calculations.
			//			 Component ID to component index conversion might be used often to it deserves
			//       optimizing as much as possible.
			// const auto i = core::get_index_unsafe({pCompIds, MAX_COMPONENTS}, compId);
			// GAIA_ASSERT(i != BadIndex);
			// return i;
		}

#if GAIA_COMP_ID_PROBING
		//----------------------------------------------------------------------
		// Inline component id hash map
		//----------------------------------------------------------------------

		inline uint32_t comp_idx_hash(ComponentId compId) {
			return compId * 2654435761;
		}

		inline void set_comp_idx(std::span<ComponentId> data, ComponentId compId) {
			GAIA_ASSERT(!data.empty());

			// Calculate the index / hash key
			auto idx = (CompOffsetMappingIndex)(comp_idx_hash(compId) % data.size());

			// Linear probing for collision handling
			while (data[idx] != ComponentIdBad && data[idx] != compId)
				idx = (idx + 1) % data.size();

			// If slot is empty, insert the index
			GAIA_ASSERT(data[idx] == ComponentIdBad);
			data[idx] = compId;
		}

		inline CompOffsetMappingIndex get_comp_idx(std::span<const ComponentId> data, ComponentId compId) {
			GAIA_ASSERT(!data.empty());

			// Calculate the index / hash key
			auto idx = (CompOffsetMappingIndex)(comp_idx_hash(compId) % data.size());

			// Linear probing for collision handling
			while (data[idx] != ComponentIdBad && data[idx] != compId)
				idx = (idx + 1) % data.size();

			return idx;
		}

		inline bool has_comp_idx(std::span<const ComponentId> data, ComponentId compId) {
			if (data.empty())
				return false;

			auto idx = (CompOffsetMappingIndex)(comp_idx_hash(compId) % data.size());
			const auto idxOrig = idx;

			// Linear probing for collision handling
			while (data[idx] != ComponentIdBad && data[idx] != compId) {
				idx = (idx + 1) % data.size();
				if (idx == idxOrig)
					return false;
			}

			return data[idx] == compId;
		}
#endif
	} // namespace ecs
} // namespace gaia