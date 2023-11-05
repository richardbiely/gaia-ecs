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
		using ComponentVersion = uint32_t;
		using ChunkDataVersionOffset = uint8_t;
		using CompOffsetMappingIndex = uint8_t;
		using ChunkDataOffset = uint16_t;
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
				using Type = typename T::TType;
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

		template <typename T>
		struct AsChunk {
			using TType = typename std::decay_t<typename std::remove_pointer_t<T>>;
			using TTypeOriginal = T;
			static constexpr ComponentKind Kind = ComponentKind::CK_Chunk;
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
			static_assert(is_component_size_valid_v<U>, "MAX_COMPONENTS_SIZE_IN_BYTES in bytes is exceeded");
			static_assert(is_component_type_valid_v<U>, "Component type restrictions not met");
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
		//! \param pCompIds Pointer to the start of the component array
		//! \param compId Component id we search for
		//! \return Index of the component id in the array
		//! \warning The component id must be present in the array
		template <uint32_t MAX_COMPONENTS>
		GAIA_NODISCARD inline uint32_t comp_idx(const ComponentId* pCompIds, ComponentId compId) {
#if GAIA_USE_SIMD_COMP_IDX && GAIA_ARCH == GAIA_ARCH_ARM
			// Set the search value in a Neon register
			uint32x4_t searchValue = vdupq_n_u32(compId);

			// auto _mm_movemask_ps = [](uint32x4_t v) {
			// 	static const uint32x4_t mask = {1, 2, 4, 8};
			// 	const uint32x4_t av = vandq_u32(v, mask), xv = vextq_u32(av, av, 2), ov = vorrq_u32(av, xv);
			// 	return vgetq_lane_u32(vorrq_u32(ov, vextq_u32(ov, ov, 3)), 0);
			// };
			// // This is slower
			// // auto _mm_movemask_ps = [](uint32x4_t CR) {
			// // 	static const uint32_t elementIndex[4]{1, 2, 4, 8};
			// // 	static const uint32x4_t mask = vld1q_u32(elementIndex); // extract element Index bitmask from compare
			// result
			// // 	uint32x4_t vtemp = vandq_u32(CR, mask);
			// // 	uint32x2_t VL = vget_low_u32(vtemp); // get low 2 uint32
			// // 	uint32x2_t VH = vget_high_u32(vtemp); // get high 2 uint32
			// // 	VL = vorr_u32(VL, VH);
			// // 	VL = vpadd_u32(VL, VL);
			// // 	return vget_lane_u32(VL, 0);
			// // };

			// for (uint32_t j = 0; j < MAX_COMPONENTS; j += 4) {
			// 	uint32x4_t values[4] = {
			// 			vld1q_u32(&pCompIds[j + 0]),
			// 			vld1q_u32(&pCompIds[j + 1]),
			// 			vld1q_u32(&pCompIds[j + 2]),
			// 			vld1q_u32(&pCompIds[j + 3]),
			// 	};
			// 	uint32x4_t cmp[4] = {
			// 			vceqq_u32(values[0], searchValue), vceqq_u32(values[1], searchValue), vceqq_u32(values[2], searchValue),
			// 			vceqq_u32(values[3], searchValue)};
			// 	uint32_t res[4] = {
			// 			_mm_movemask_ps(cmp[0]), _mm_movemask_ps(cmp[1]), _mm_movemask_ps(cmp[2]), _mm_movemask_ps(cmp[3])};

			// 	// This is way slower than searching in non-simd way
			// 	// static const uint32x4_t s = vdupq_n_u32(1);
			// 	// auto v = vld1q_u32(res);
			// 	// auto c = vceqq_u32(v, s);
			// 	// auto r = _mm_movemask_ps(c);
			// 	// if (r != 0)
			// 	// 	return __builtin_ctz(r);
			// 	for (uint32_t i = 0; i < 4; ++i) {
			// 		if (res[i] != 0)
			// 			return __builtin_ctz(res[i]);
			// 	}
			// }

			// GAIA_ASSERT(false);
			// return 0;

			uint32_t i = 0;
			uint64_t res = 0;
			do {
				// Load the elements into a Neon register
				uint32x4_t values = vld1q_u32(&pCompIds[i]);
				// Compare values with searchValue
				uint32x4_t cmp = vceqq_u32(values, searchValue);
				// Convert to uint16x4_t
				uint16x4_t cmp2 = vshrn_n_u32(cmp, 16);
				// Convert to uint64x1_t
				uint64x1_t mask = vreinterpret_u64_u16(cmp2);
				// Convert to scalar uint64_t
				res = vget_lane_u64(mask, 0);

				i += 4;
			} while (res == 0);
			// Find the first set bit and divide by 16 because the numbers
			// are stored in 16-bit pairs. This will return 0..3.
			uint32_t iii = __builtin_clzll(res);
			uint32_t idx = (63 - iii) / 16;
			return idx + i;
#else
			// We let the compiler know the upper iteration bound at compile-time.
			// This way it can optimize better (e.g. loop unrolling, vectorization).
			for (uint32_t idx = 0; idx < MAX_COMPONENTS; ++idx)
				if (pCompIds[idx] == compId)
					return idx;

			GAIA_ASSERT(false);
			return BadIndex;

			// NOTE: This code does technically the same as the above.
			//       However, compilers can't quite optimize it as well because it does some more
			//       calculations.
			//			 Component ID to component index conversion might be used often to it deserves
			//       optimizing as much as possible.
			// const auto idx = core::get_index_unsafe({pCompIds, MAX_COMPONENTS}, compId);
			// GAIA_ASSERT(idx != BadIndex);
			// return idx;
#endif
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