#pragma once
#include "gaia/config/config.h"

#include <tuple>
#include <type_traits>
#include <utility>

#include "gaia/core/span.h"
#include "gaia/core/utility.h"
#include "gaia/mem/mem_alloc.h"
#include "gaia/mem/mem_sani.h"
#include "gaia/meta/reflection.h"

namespace gaia {
	namespace mem {
		//! Supported physical layouts for component values.
		enum class DataLayout : uint8_t {
			AoS = 0, //!< Array of structures.
			SoA = 1, //!< Structure of arrays with four-item packing, suitable for SSE-class SIMD.
			SoA8 = 2, //!< Structure of arrays with eight-item packing, suitable for AVX-class SIMD.
			SoA16 = 3, //!< Structure of arrays with sixteen-item packing, suitable for AVX-512-class SIMD.

			Count = 4 //!< Number of supported layouts.
		};

#define GAIA_LAYOUT(layout_name) static constexpr auto gaia_Data_Layout = ::gaia::mem::DataLayout::layout_name

		// Helper templates
		//! \cond INTERNAL
		namespace detail {
			template <typename T>
			constexpr uint32_t get_alignment() {
				if constexpr (std::is_empty_v<T>)
					// Always consider 0 for empty types
					return 0U;
				else {
					// Use at least 4 (32-bit systems) or 8 (64-bit systems) bytes for alignment
					return (uint32_t)core::get_min(sizeof(uintptr_t), alignof(T));
				}
			}

			//! Advances across one field array in SoA storage.
			//! \param address Current field-array address.
			//! \param alig Alignment shared by the field arrays.
			//! \param itemSize Byte size of one field value.
			//! \param cnt Number of values reserved in the field array.
			//! \return Address immediately after the aligned field array.
			constexpr size_t get_aligned_byte_offset(uintptr_t address, size_t alig, size_t itemSize, size_t cnt) {
				const auto padding = mem::padding(address, alig);
				address += padding + itemSize * cnt;
				return address;
			}

			//! Advances across one typed field array in SoA storage.
			//! \tparam T Field value type.
			//! \tparam Alignment Alignment shared by the field arrays.
			//! \param address Current field-array address.
			//! \param cnt Number of values reserved in the field array.
			//! \return Address immediately after the aligned field array.
			template <typename T, size_t Alignment>
			constexpr size_t get_aligned_byte_offset(uintptr_t address, size_t cnt) {
				const auto padding = mem::padding<Alignment>(address);
				address += padding + sizeof(T) * cnt;
				return address;
			}
		} // namespace detail
		//! \endcond

		//! Compile-time properties of a data layout.
		//! \tparam TDataLayout Physical data layout.
		//! \tparam TItem Stored item type.
		template <DataLayout TDataLayout, typename TItem>
		struct data_layout_properties;
		//! Properties of array-of-structures storage.
		//! \tparam TItem Stored item type.
		template <typename TItem>
		struct data_layout_properties<DataLayout::AoS, TItem> {
			//! Physical layout.
			constexpr static DataLayout Layout = DataLayout::AoS;
			//! Number of values in one packing group.
			constexpr static size_t PackSize = 1;
			//! Required byte alignment.
			constexpr static size_t Alignment = detail::get_alignment<TItem>();
		};
		//! Properties of four-wide structure-of-arrays storage.
		//! \tparam TItem Stored item type.
		template <typename TItem>
		struct data_layout_properties<DataLayout::SoA, TItem> {
			//! Physical layout.
			constexpr static DataLayout Layout = DataLayout::SoA;
			//! Number of values in one packing group.
			constexpr static size_t PackSize = 4;
			//! Required byte alignment.
			constexpr static size_t Alignment = PackSize * 4;
		};
		//! Properties of eight-wide structure-of-arrays storage.
		//! \tparam TItem Stored item type.
		template <typename TItem>
		struct data_layout_properties<DataLayout::SoA8, TItem> {
			//! Physical layout.
			constexpr static DataLayout Layout = DataLayout::SoA8;
			//! Number of values in one packing group.
			constexpr static size_t PackSize = 8;
			//! Required byte alignment.
			constexpr static size_t Alignment = PackSize * 4;
		};
		//! Properties of sixteen-wide structure-of-arrays storage.
		//! \tparam TItem Stored item type.
		template <typename TItem>
		struct data_layout_properties<DataLayout::SoA16, TItem> {
			//! Physical layout.
			constexpr static DataLayout Layout = DataLayout::SoA16;
			//! Number of values in one packing group.
			constexpr static size_t PackSize = 16;
			//! Required byte alignment.
			constexpr static size_t Alignment = PackSize * 4;
		};

		//! Storage policy for a selected layout and item type.
		//! \tparam TDataLayout Physical data layout.
		//! \tparam TItem Stored item type.
		template <DataLayout TDataLayout, typename TItem>
		struct data_view_policy;

		//! Read-only view for a selected layout and item type.
		//! \tparam TDataLayout Physical data layout.
		//! \tparam TItem Stored item type.
		template <DataLayout TDataLayout, typename TItem>
		struct data_view_policy_get;
		//! Mutable view for a selected layout and item type.
		//! \tparam TDataLayout Physical data layout.
		//! \tparam TItem Stored item type.
		template <DataLayout TDataLayout, typename TItem>
		struct data_view_policy_set;

		//! Indexed read-only view metadata.
		//! \tparam TDataLayout Physical data layout.
		//! \tparam TItem Stored item type.
		//! \tparam Ids Selected field index.
		template <DataLayout TDataLayout, typename TItem, size_t Ids>
		struct data_view_policy_get_idx;
		//! Indexed mutable view metadata.
		//! \tparam TDataLayout Physical data layout.
		//! \tparam TItem Stored item type.
		//! \tparam Ids Selected field index.
		template <DataLayout TDataLayout, typename TItem, size_t Ids>
		struct data_view_policy_set_idx;

		//! View policy for accessing and storing data in the AoS way.
		//! Good for random access and when accessing data that needs to be
		//! close together.
		//!
		//! struct Foo {
		//!   int x;
		//!   int y;
		//!   int z;
		//! };
		//! using fooViewPolicy = data_view_policy<DataLayout::AoS, Foo>;
		//!
		//! Memory organized as: xyz xyz xyz xyz
		template <typename ValueType>
		struct data_view_policy_aos {
			//! Pointer type used to address stored values.
			using TargetCastType = std::add_pointer_t<ValueType>;

			//! Physical layout.
			constexpr static DataLayout Layout = data_layout_properties<DataLayout::AoS, ValueType>::Layout;
			//! Required byte alignment.
			constexpr static size_t Alignment = data_layout_properties<DataLayout::AoS, ValueType>::Alignment;

			//! Calculates the bytes required for a value range.
			//! \param addr Starting address used for alignment calculations.
			//! \param cnt Number of values.
			//! \return Minimum required byte count.
			GAIA_NODISCARD static constexpr uint32_t get_min_byte_size(uintptr_t addr, size_t cnt) noexcept {
				const auto offset = detail::get_aligned_byte_offset<ValueType, Alignment>(addr, cnt);
				return (uint32_t)(offset - addr);
			}

			//! Allocates and default-constructs an AoS value range.
			//! \tparam Allocator Allocator adaptor type.
			//! \param cnt Number of values.
			//! \return Allocated byte buffer.
			template <typename Allocator>
			GAIA_NODISCARD static uint8_t* alloc(size_t cnt) noexcept {
				const auto bytes = get_min_byte_size(0, cnt);
				auto* pData = (ValueType*)mem::AllocHelper::alloc<uint8_t, Allocator>(bytes);
				core::call_ctor_raw_n(pData, cnt);
				return (uint8_t*)pData;
			}

			//! Destroys and releases an instrumented AoS value range.
			//! \tparam Allocator Allocator adaptor type.
			//! \param pData Allocated buffer, or null.
			//! \param cap Buffer capacity in values.
			//! \param cnt Number of live values.
			template <typename Allocator>
			static void free(void* pData, size_t cap, size_t cnt) noexcept {
				if (pData == nullptr)
					return;
				core::call_dtor_n((ValueType*)pData, cnt);
				GAIA_MEM_SANI_DEL_BLOCK(sizeof(ValueType), pData, cap, cnt);
				return mem::AllocHelper::free<Allocator>(pData);
			}

			//! Destroys and releases an AoS value range.
			//! \tparam Allocator Allocator adaptor type.
			//! \param pData Allocated buffer, or null.
			//! \param cnt Number of live values.
			template <typename Allocator>
			static void free(void* pData, size_t cnt) noexcept {
				if (pData == nullptr)
					return;
				core::call_dtor_n((ValueType*)pData, cnt);
				return mem::AllocHelper::free<Allocator>(pData);
			}

			//! Copies a value from an AoS span.
			//! \param s Source values.
			//! \param idx Value index.
			//! \return Copied value.
			GAIA_NODISCARD constexpr static ValueType get_value(std::span<const ValueType> s, size_t idx) noexcept {
				return s[idx];
			}

			//! Returns a read-only value reference from an AoS span.
			//! \param s Source values.
			//! \param idx Value index.
			//! \return Reference to the selected value.
			GAIA_NODISCARD constexpr static const ValueType& get(std::span<const ValueType> s, size_t idx) noexcept {
				return s[idx];
			}

			//! Returns a mutable value reference from an AoS span.
			//! \param s Destination values.
			//! \param idx Value index.
			//! \return Reference to the selected value.
			GAIA_NODISCARD constexpr static ValueType& set(std::span<ValueType> s, size_t idx) noexcept {
				return s[idx];
			}
		};

		template <typename ValueType>
		struct data_view_policy<DataLayout::AoS, ValueType>: data_view_policy_aos<ValueType> {};

		//! Read-only byte view over AoS values.
		//! \tparam ValueType Stored value type.
		template <typename ValueType>
		struct data_view_policy_aos_get {
			//! Underlying storage policy.
			using view_policy = data_view_policy_aos<ValueType>;

			//! Raw data pointed to by the view policy
			std::span<const uint8_t> m_data;

			//! Creates a read-only view over mutable bytes.
			//! \param data Backing byte span.
			data_view_policy_aos_get(std::span<uint8_t> data): m_data({(const uint8_t*)data.data(), data.size()}) {}
			//! Creates a read-only view over bytes.
			//! \param data Backing byte span.
			data_view_policy_aos_get(std::span<const uint8_t> data): m_data({(const uint8_t*)data.data(), data.size()}) {}
			//! Creates a read-only view over a contiguous container.
			//! \tparam C Contiguous container type.
			//! \param c Backing container.
			template <typename C>
			data_view_policy_aos_get(const C& c): m_data({(const uint8_t*)c.data(), c.size()}) {
				static_assert(!std::is_same_v<C, data_view_policy_aos_get>);
			}

			//! Returns a value by index.
			//! \param idx Value index.
			//! \return Read-only reference to the value.
			GAIA_NODISCARD const ValueType& operator[](size_t idx) const noexcept {
				GAIA_ASSERT(idx < m_data.size());
				return ((const ValueType*)m_data.data())[idx];
			}

			//! Returns the backing byte address.
			//! \return Read-only byte pointer.
			GAIA_NODISCARD decltype(auto) data() const noexcept {
				return (const uint8_t*)m_data.data();
			}

			//! Returns the backing span size.
			//! \return Span size in bytes.
			GAIA_NODISCARD auto size() const noexcept {
				return m_data.size();
			}
		};

		template <typename ValueType>
		struct data_view_policy_get<DataLayout::AoS, ValueType>: data_view_policy_aos_get<ValueType> {};

		//! Mutable byte view over AoS values.
		//! \tparam ValueType Stored value type.
		template <typename ValueType>
		struct data_view_policy_aos_set {
			//! Underlying storage policy.
			using view_policy = data_view_policy_aos<ValueType>;

			//! Raw data pointed to by the view policy
			std::span<uint8_t> m_data;

			//! Creates a mutable view over bytes.
			//! \param data Backing byte span.
			data_view_policy_aos_set(std::span<uint8_t> data): m_data({(uint8_t*)data.data(), data.size()}) {}
			//! Creates a mutable view from a byte span.
			//! \param data Backing byte span whose constness is intentionally erased.
			data_view_policy_aos_set(std::span<const uint8_t> data): m_data({(uint8_t*)data.data(), data.size()}) {}
			//! Creates a mutable view over a contiguous container.
			//! \tparam C Contiguous container type.
			//! \param c Backing container.
			template <typename C>
			data_view_policy_aos_set(const C& c): m_data({(uint8_t*)c.data(), c.size()}) {
				static_assert(!std::is_same_v<C, data_view_policy_aos_set>);
			}

			//! Returns a mutable value by index.
			//! \param idx Value index.
			//! \return Mutable reference to the value.
			GAIA_NODISCARD ValueType& operator[](size_t idx) noexcept {
				GAIA_ASSERT(idx < m_data.size());
				return ((ValueType*)m_data.data())[idx];
			}

			//! Returns a read-only value by index.
			//! \param idx Value index.
			//! \return Read-only reference to the value.
			GAIA_NODISCARD const ValueType& operator[](size_t idx) const noexcept {
				GAIA_ASSERT(idx < m_data.size());
				return ((const ValueType*)m_data.data())[idx];
			}

			//! Returns the backing byte address.
			//! \return Mutable byte pointer.
			GAIA_NODISCARD auto data() const noexcept {
				return m_data.data();
			}

			//! Returns the backing span size.
			//! \return Span size in bytes.
			GAIA_NODISCARD auto size() const noexcept {
				return m_data.size();
			}
		};

		template <typename ValueType>
		struct data_view_policy_set<DataLayout::AoS, ValueType>: data_view_policy_aos_set<ValueType> {};

		//! Type-erased policy for addressing one field value in SoA storage.
		//! The field arrays use the same alignment and capacity rules as data_view_policy_soa.
		struct data_view_policy_soa_erased {
			//! Returns one read-only field value from type-erased SoA storage.
			//! \param pData Base address of the SoA storage.
			//! \param alignment Alignment shared by all field arrays.
			//! \param fieldSizes Byte size of each field value in storage order.
			//! \param fieldIdx Field array index.
			//! \param row Value index inside the selected field array.
			//! \param capacity Number of values reserved in every field array.
			//! \return Pointer to the selected field value.
			GAIA_NODISCARD static const uint8_t*
			get(const void* pData, uint32_t alignment, std::span<const uint8_t> fieldSizes, uint32_t fieldIdx, uint32_t row,
					uint32_t capacity) noexcept {
				GAIA_ASSERT(pData != nullptr);
				GAIA_ASSERT(alignment != 0 && fieldIdx < fieldSizes.size() && row < capacity);

				auto address = (uintptr_t)pData;
				GAIA_FOR(fieldIdx) {
					address = detail::get_aligned_byte_offset(address, alignment, fieldSizes[i], capacity);
				}
				address += mem::padding(address, alignment);
				return (const uint8_t*)address + ((uintptr_t)fieldSizes[fieldIdx] * row);
			}

			//! Returns one mutable field value from type-erased SoA storage.
			//! \param pData Base address of the SoA storage.
			//! \param alignment Alignment shared by all field arrays.
			//! \param fieldSizes Byte size of each field value in storage order.
			//! \param fieldIdx Field array index.
			//! \param row Value index inside the selected field array.
			//! \param capacity Number of values reserved in every field array.
			//! \return Pointer to the selected field value.
			GAIA_NODISCARD static uint8_t*
			set(void* pData, uint32_t alignment, std::span<const uint8_t> fieldSizes, uint32_t fieldIdx, uint32_t row,
					uint32_t capacity) noexcept {
				return const_cast<uint8_t*>(get(pData, alignment, fieldSizes, fieldIdx, row, capacity));
			}
		};

		//! View policy for accessing and storing data in the SoA way.
		//! Good for SIMD processing.
		//!
		//! struct Foo {
		//!   int x;
		//!   int y;
		//!   int z;
		//! };
		//! using fooViewPolicy = data_view_policy<DataLayout::SoA, Foo>;
		//!
		//! Memory organized as: xxxx yyyy zzzz
		template <DataLayout TDataLayout, typename ValueType>
		struct data_view_policy_soa {
			static_assert(std::is_copy_assignable_v<ValueType>);

			//! Tuple representation of the reflected value fields.
			using TTuple = decltype(meta::struct_to_tuple(std::declval<ValueType>()));
			//! Pointer type used to address SoA storage.
			using TargetCastType = uint8_t*;

			//! Physical layout.
			constexpr static DataLayout Layout = data_layout_properties<TDataLayout, ValueType>::Layout;
			//! Required byte alignment.
			constexpr static size_t Alignment = data_layout_properties<TDataLayout, ValueType>::Alignment;
			//! Number of reflected fields.
			constexpr static size_t TTupleItems = std::tuple_size<TTuple>::value;
			static_assert(Alignment > 0U, "SoA data can't be zero-aligned");

			//! Type of a reflected field.
			//! \tparam Item Field index.
			template <size_t Item>
			using value_type = typename std::tuple_element<Item, TTuple>::type;
			//! Const-qualified type of a reflected field.
			//! \tparam Item Field index.
			template <size_t Item>
			using const_value_type = typename std::add_const<value_type<Item>>::type;

			//! Calculates the bytes required for an SoA value range.
			//! \param addr Starting address used for alignment calculations.
			//! \param cnt Number of values.
			//! \return Minimum required byte count.
			GAIA_NODISCARD constexpr static uint32_t get_min_byte_size(uintptr_t addr, size_t cnt) noexcept {
				const auto offset = get_aligned_byte_offset<TTupleItems>(addr, cnt);
				return (uint32_t)(offset - addr);
			}

			//! Allocates an SoA value range.
			//! \tparam Allocator Allocator adaptor type.
			//! \param cnt Number of values.
			//! \return Allocated byte buffer.
			template <typename Allocator>
			GAIA_NODISCARD static uint8_t* alloc(size_t cnt) noexcept {
				const auto bytes = get_min_byte_size(0, cnt);
				return mem::AllocHelper::alloc_alig<uint8_t, Allocator>(Alignment, bytes);
			}

			//! Releases an instrumented SoA value range.
			//! \tparam Allocator Allocator adaptor type.
			//! \param pData Allocated buffer, or null.
			//! \param cap Buffer capacity in values.
			//! \param cnt Number of live values.
			template <typename Allocator>
			static void free(void* pData, size_t cap, size_t cnt) noexcept {
				if (pData == nullptr)
					return;

				mem_del_block(pData, cap, cnt);
				return mem::AllocHelper::free_alig<Allocator>(pData);
			}

			//! Registers a newly allocated SoA range with the memory sanitizer.
			//! \param pData Backing buffer.
			//! \param cap Buffer capacity in values.
			//! \param count Number of live values.
			static void mem_add_block(void* pData, size_t cap, size_t count) {
				meta::each_member(ValueType{}, [&](auto&&... item) {
					auto address = mem::align<Alignment>((uintptr_t)pData);
					((
							 //
							 GAIA_MEM_SANI_ADD_BLOCK(sizeof(item), (void*)address, cap, count),
							 // Skip towards the next element and make sure the address is aligned properly
							 address = mem::align<Alignment>(address + (sizeof(item) * cap))),
					 ...);
				});
			}

			//! Unregisters an SoA range from the memory sanitizer.
			//! \param pData Backing buffer.
			//! \param cap Buffer capacity in values.
			//! \param count Number of live values.
			static void mem_del_block(void* pData, size_t cap, size_t count) {
				meta::each_member(ValueType{}, [&](auto&&... item) {
					auto address = mem::align<Alignment>((uintptr_t)pData);
					((
							 //
							 GAIA_MEM_SANI_DEL_BLOCK(sizeof(item), (void*)address, cap, count),
							 // Skip towards the next element and make sure the address is aligned properly
							 address = mem::align<Alignment>(address + (sizeof(item) * cap))),
					 ...);
				});
			}

			//! Makes newly appended SoA values addressable by the memory sanitizer.
			//! \param pData Backing buffer.
			//! \param cap Buffer capacity in values.
			//! \param count Current number of live values.
			//! \param n Number of values being appended.
			static void mem_push_block(void* pData, size_t cap, size_t count, size_t n) {
				meta::each_member(ValueType{}, [&](auto&&... item) {
					auto address = mem::align<Alignment>((uintptr_t)pData);
					((
							 //
							 GAIA_MEM_SANI_PUSH_N(sizeof(item), (void*)address, cap, count, n),
							 // Skip towards the next element and make sure the address is aligned properly
							 address = mem::align<Alignment>(address + (sizeof(item) * cap))),
					 ...);
				});
			}

			//! Poisons removed SoA values for the memory sanitizer.
			//! \param pData Backing buffer.
			//! \param cap Buffer capacity in values.
			//! \param count Current number of live values.
			//! \param n Number of values being removed.
			static void mem_pop_block(void* pData, size_t cap, size_t count, size_t n) {
				meta::each_member(ValueType{}, [&](auto&&... item) {
					auto address = mem::align<Alignment>((uintptr_t)pData);
					((
							 //
							 GAIA_MEM_SANI_POP_N(sizeof(item), (void*)address, cap, count, n),
							 // Skip towards the next element and make sure the address is aligned properly
							 address = mem::align<Alignment>(address + (sizeof(item) * cap))),
					 ...);
				});
			}

			//! Reconstructs a value from its SoA fields.
			//! \param s Backing byte span. Its size is the value capacity.
			//! \param idx Value index.
			//! \return Reconstructed value.
			GAIA_NODISCARD constexpr static ValueType get(std::span<const uint8_t> s, size_t idx) noexcept {
				return get_inter(meta::struct_to_tuple(ValueType{}), s, idx, std::make_index_sequence<TTupleItems>());
			}

			//! Returns a read-only span over one SoA field.
			//! \tparam Item Field index.
			//! \param s Backing byte span. Its size is the value capacity.
			//! \param idx First value index.
			//! \return Field span beginning at the requested index.
			template <size_t Item>
			GAIA_NODISCARD constexpr static auto get(std::span<const uint8_t> s, size_t idx = 0) noexcept {
				const auto offset = get_aligned_byte_offset<Item>((uintptr_t)s.data(), s.size());
				const auto& ref = get_ref<const value_type<Item>>(reinterpret_cast<const uint8_t*>(offset), idx);
				return std::span{&ref, s.size() - idx};
			}

			//! Proxy used to read or assign one complete SoA value.
			class accessor {
				std::span<uint8_t> m_data;
				size_t m_idx;

			public:
				//! Creates a value proxy.
				//! \param data Backing byte span.
				//! \param idx Value index.
				constexpr accessor(std::span<uint8_t> data, size_t idx): m_data(data), m_idx(idx) {}

				//! Assigns a copied value to the selected SoA fields.
				//! \param val Source value.
				constexpr void operator=(const ValueType& val) noexcept {
					set_inter(meta::struct_to_tuple(val), m_data, m_idx, std::make_index_sequence<TTupleItems>());
				}

				//! Assigns a moved value to the selected SoA fields.
				//! \param val Source value.
				constexpr void operator=(ValueType&& val) noexcept {
					set_inter(meta::struct_to_tuple(GAIA_MOV(val)), m_data, m_idx, std::make_index_sequence<TTupleItems>());
				}

				//! Reconstructs the selected value.
				//! \return Reconstructed value.
				GAIA_NODISCARD constexpr operator ValueType() const noexcept {
					return get_inter(
							meta::struct_to_tuple(ValueType{}), {(const uint8_t*)m_data.data(), m_data.size()}, m_idx,
							std::make_index_sequence<TTupleItems>());
				}
			};

			//! Returns a mutable proxy for one complete value.
			//! \param s Backing byte span. Its size is the value capacity.
			//! \param idx Value index.
			//! \return Mutable value proxy.
			GAIA_NODISCARD constexpr static auto set(std::span<uint8_t> s, size_t idx) noexcept {
				return accessor(s, idx);
			}

			//! Returns a mutable span over one SoA field.
			//! \tparam Item Field index.
			//! \param s Backing byte span. Its size is the value capacity.
			//! \param idx First value index.
			//! \return Field span beginning at the requested index.
			template <size_t Item>
			GAIA_NODISCARD constexpr static auto set(std::span<uint8_t> s, size_t idx = 0) noexcept {
				const auto offset = get_aligned_byte_offset<Item>((uintptr_t)s.data(), s.size());
				auto& ref = get_ref<value_type<Item>>((const uint8_t*)offset, idx);
				return std::span{&ref, s.size() - idx};
			}

		private:
			template <size_t... Ids>
			GAIA_NODISCARD constexpr static size_t
			get_aligned_byte_offset_seq(uintptr_t address, size_t cnt, std::index_sequence<Ids...> /*no_name*/) {
				((address = detail::get_aligned_byte_offset(address, Alignment, sizeof(value_type<Ids>), cnt)), ...);
				address += mem::padding(address, Alignment);
				return address;
			}

			template <uint32_t N>
			GAIA_NODISCARD constexpr static size_t get_aligned_byte_offset(uintptr_t address, size_t cnt) {
				return get_aligned_byte_offset_seq(address, cnt, std::make_index_sequence<N>());
			}

			template <typename TMemberType>
			GAIA_NODISCARD constexpr static TMemberType& get_ref(const uint8_t* data, size_t idx) noexcept {
				// Write the value directly to the memory address.
				// Usage of unaligned_ref is not necessary because the memory is aligned.
				auto* pCastData = (TMemberType*)data;
				return pCastData[idx];
			}

			template <typename Tup, size_t... Ids>
			GAIA_NODISCARD constexpr static ValueType
			get_inter(Tup&& t, std::span<const uint8_t> s, size_t idx, std::index_sequence<Ids...> /*no_name*/) noexcept {
				auto address = mem::align<Alignment>((uintptr_t)s.data());
				((
						 // Put the value at the address into our tuple. Data is aligned so we can read directly.
						 std::get<Ids>(t) = get_ref<value_type<Ids>>((const uint8_t*)address, idx),
						 // Skip towards the next element and make sure the address is aligned properly
						 address = mem::align<Alignment>(address + (sizeof(value_type<Ids>) * s.size()))),
				 ...);
				return meta::tuple_to_struct<ValueType, TTuple>(GAIA_FWD(t));
			}

			template <typename Tup, size_t... Ids>
			constexpr static void
			set_inter(Tup&& t, std::span<uint8_t> s, size_t idx, std::index_sequence<Ids...> /*no_name*/) noexcept {
				auto address = mem::align<Alignment>((uintptr_t)s.data());
				((
						 // Set the tuple value. Data is aligned so we can write directly.
						 get_ref<value_type<Ids>>((uint8_t*)address, idx) = std::get<Ids>(t),
						 // Skip towards the next element and make sure the address is aligned properly
						 address = mem::align<Alignment>(address + (sizeof(value_type<Ids>) * s.size()))),
				 ...);
			}
		};

		template <typename ValueType>
		struct data_view_policy<DataLayout::SoA, ValueType>: //
			data_view_policy_soa<DataLayout::SoA, ValueType> {};
		template <typename ValueType>
		struct data_view_policy<DataLayout::SoA8, ValueType>: //
			data_view_policy_soa<DataLayout::SoA8, ValueType> {};
		template <typename ValueType>
		struct data_view_policy<DataLayout::SoA16, ValueType>: //
			data_view_policy_soa<DataLayout::SoA16, ValueType> {};

		//! Read-only byte view over SoA values.
		//! \tparam TDataLayout SoA packing layout.
		//! \tparam ValueType Stored value type.
		template <DataLayout TDataLayout, typename ValueType>
		struct data_view_policy_soa_get {
			static_assert(std::is_copy_assignable_v<ValueType>);

			//! Underlying storage policy.
			using view_policy = data_view_policy_soa<TDataLayout, ValueType>;

			//! Metadata for a selected field.
			//! \tparam Item Field index.
			template <size_t Item>
			struct data_view_policy_idx_info {
				//! Const-qualified field type.
				using const_value_type = typename view_policy::template const_value_type<Item>;
			};

			//! Raw data pointed to by the view policy
			std::span<const uint8_t> m_data;

			//! Creates a read-only view over mutable bytes.
			//! \param data Backing byte span.
			data_view_policy_soa_get(std::span<uint8_t> data): m_data({(const uint8_t*)data.data(), data.size()}) {}
			//! Creates a read-only view over bytes.
			//! \param data Backing byte span.
			data_view_policy_soa_get(std::span<const uint8_t> data): m_data({(const uint8_t*)data.data(), data.size()}) {}
			//! Creates a read-only view over a contiguous container.
			//! \tparam C Contiguous container type.
			//! \param c Backing container.
			template <typename C>
			data_view_policy_soa_get(const C& c): m_data({(const uint8_t*)c.data(), c.size()}) {
				static_assert(!std::is_same_v<C, data_view_policy_soa_get>);
			}

			//! Reconstructs a value by index.
			//! \param idx Value index.
			//! \return Reconstructed value.
			GAIA_NODISCARD constexpr decltype(auto) operator[](size_t idx) const noexcept {
				return view_policy::get(m_data, idx);
			}

			//! Returns a read-only span over one field.
			//! \tparam Item Field index.
			//! \return Field span.
			template <size_t Item>
			GAIA_NODISCARD constexpr auto get() const noexcept {
				auto s = view_policy::template get<Item>(m_data);
				return std::span(s.data(), s.size());
			}

			//! Returns the backing byte address.
			//! \return Read-only byte pointer.
			GAIA_NODISCARD decltype(auto) data() const noexcept {
				return (const uint8_t*)m_data.data();
			}

			//! Returns the backing span size.
			//! \return Value capacity encoded by the span.
			GAIA_NODISCARD auto size() const noexcept {
				return m_data.size();
			}
		};

		template <typename ValueType>
		struct data_view_policy_get<DataLayout::SoA, ValueType>: //
			data_view_policy_soa_get<DataLayout::SoA, ValueType> {};
		template <typename ValueType>
		struct data_view_policy_get<DataLayout::SoA8, ValueType>: //
			data_view_policy_soa_get<DataLayout::SoA8, ValueType> {};
		template <typename ValueType>
		struct data_view_policy_get<DataLayout::SoA16, ValueType>: //
				data_view_policy_soa_get<DataLayout::SoA16, ValueType> {};

		//! Mutable byte view over SoA values.
		//! \tparam TDataLayout SoA packing layout.
		//! \tparam ValueType Stored value type.
		template <DataLayout TDataLayout, typename ValueType>
		struct data_view_policy_soa_set {
			static_assert(std::is_copy_assignable_v<ValueType>);

			//! Underlying storage policy.
			using view_policy = data_view_policy_soa<TDataLayout, ValueType>;

			//! Metadata for a selected field.
			//! \tparam Item Field index.
			template <size_t Item>
			struct data_view_policy_idx_info {
				//! Mutable field type.
				using value_type = typename view_policy::template value_type<Item>;
				//! Const-qualified field type.
				using const_value_type = typename view_policy::template const_value_type<Item>;
			};

			//! Raw data pointed to by the view policy
			std::span<uint8_t> m_data;

			//! Creates a mutable view over bytes.
			//! \param data Backing byte span.
			data_view_policy_soa_set(std::span<uint8_t> data): m_data({(uint8_t*)data.data(), data.size()}) {}
			//! Creates a mutable view from a byte span.
			//! \param data Backing byte span whose constness is intentionally erased.
			data_view_policy_soa_set(std::span<const uint8_t> data): m_data({(uint8_t*)data.data(), data.size()}) {}
			//! Creates a mutable view over a contiguous container.
			//! \tparam C Contiguous container type.
			//! \param c Backing container.
			template <typename C>
			data_view_policy_soa_set(const C& c): m_data({(uint8_t*)c.data(), c.size()}) {
				static_assert(!std::is_same_v<C, data_view_policy_soa_set>);
			}

			//! Proxy used to assign one complete value.
			struct accessor {
				//! Backing byte span.
				std::span<uint8_t> m_data;
				//! Value index.
				size_t m_idx;

				//! Assigns a copied value.
				//! \param val Source value.
				constexpr void operator=(const ValueType& val) noexcept {
					view_policy::set(m_data, m_idx) = val;
				}
				//! Assigns a moved value.
				//! \param val Source value.
				constexpr void operator=(ValueType&& val) noexcept {
					view_policy::set(m_data, m_idx) = GAIA_FWD(val);
				}
			};

			//! Reconstructs a read-only value by index.
			//! \param idx Value index.
			//! \return Reconstructed value.
			GAIA_NODISCARD constexpr decltype(auto) operator[](size_t idx) const noexcept {
				return view_policy::get({(const uint8_t*)m_data.data(), m_data.size()}, idx);
			}
			//! Returns a mutable proxy by index.
			//! \param idx Value index.
			//! \return Mutable value proxy.
			GAIA_NODISCARD constexpr auto operator[](size_t idx) noexcept {
				return accessor{m_data, idx};
			}

			//! Returns a read-only span over one field.
			//! \tparam Item Field index.
			//! \return Field span.
			template <size_t Item>
			GAIA_NODISCARD constexpr auto get() const noexcept {
				auto s = view_policy::template get<Item>(m_data);
				return std::span(s.data(), s.size());
			}

			//! Returns a mutable span over one field.
			//! \tparam Item Field index.
			//! \return Field span.
			template <size_t Item>
			GAIA_NODISCARD constexpr auto set() noexcept {
				auto s = view_policy::template set<Item>(m_data);
				return std::span(s.data(), s.size());
			}

			//! Returns the backing byte address.
			//! \return Mutable byte pointer.
			GAIA_NODISCARD auto data() const noexcept {
				return m_data.data();
			}

			//! Returns the backing span size.
			//! \return Value capacity encoded by the span.
			GAIA_NODISCARD auto size() const noexcept {
				return m_data.size();
			}
		};

		template <typename ValueType>
		struct data_view_policy_set<DataLayout::SoA, ValueType>: //
			data_view_policy_soa_set<DataLayout::SoA, ValueType> {};
		template <typename ValueType>
		struct data_view_policy_set<DataLayout::SoA8, ValueType>: //
			data_view_policy_soa_set<DataLayout::SoA8, ValueType> {};
		template <typename ValueType>
		struct data_view_policy_set<DataLayout::SoA16, ValueType>: //
				data_view_policy_soa_set<DataLayout::SoA16, ValueType> {};

		//----------------------------------------------------------------------
		// Helpers
		//----------------------------------------------------------------------

		//! \cond INTERNAL
		namespace detail {
			template <typename, typename = void>
			struct auto_view_policy_inter {
				static constexpr DataLayout data_layout_type = DataLayout::AoS;
			};
			template <typename T>
			struct auto_view_policy_inter<T, std::void_t<decltype(T::gaia_Data_Layout)>> {
				static constexpr DataLayout data_layout_type = T::gaia_Data_Layout;
			};

			template <typename, typename = void>
			struct is_soa_layout: std::false_type {};
			template <typename T>
			struct is_soa_layout<T, std::void_t<decltype(T::gaia_Data_Layout)>>:
					std::bool_constant<!std::is_empty_v<T> && (T::gaia_Data_Layout != DataLayout::AoS)> {};
		} // namespace detail
		//! \endcond

		//! Automatically selected storage policy for a type.
		//! \tparam T Stored type.
		template <typename T>
		using auto_view_policy = data_view_policy<detail::auto_view_policy_inter<T>::data_layout_type, T>;
		//! Automatically selected read-only view for a type.
		//! \tparam T Stored type.
		template <typename T>
		using auto_view_policy_get = data_view_policy_get<detail::auto_view_policy_inter<T>::data_layout_type, T>;
		//! Automatically selected mutable view for a type.
		//! \tparam T Stored type.
		template <typename T>
		using auto_view_policy_set = data_view_policy_set<detail::auto_view_policy_inter<T>::data_layout_type, T>;

		//! Whether a type selects a non-AoS layout.
		//! \tparam T Type to inspect.
		template <typename T>
		inline constexpr bool is_soa_layout_v = detail::is_soa_layout<T>::value;

	} // namespace mem
} // namespace gaia
