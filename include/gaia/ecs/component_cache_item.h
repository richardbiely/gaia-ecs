#pragma once
#include "gaia/config/config.h"

#include <cstdint>
#include <cstring>
#include <type_traits>

#include "gaia/cnt/darray.h"
#include "gaia/cnt/map.h"
#include "gaia/core/hashing_string.h"
#include "gaia/ecs/component.h"
#include "gaia/ecs/component_desc.h"
#include "gaia/ecs/id.h"
#include "gaia/mem/mem_alloc.h"
#include "gaia/mem/mem_utils.h"
#include "gaia/mem/smallblock_allocator.h"
#include "gaia/ser/ser_common.h"
#include "gaia/ser/ser_rt.h"
#include "gaia/util/str.h"

//! \cond INTERNAL
namespace gaia {
	namespace ecs {
		class World;
		class Chunk;
		class ComponentCache;
		struct ComponentRecord;

		//! Intern table for immutable component-cache symbols.
		class SymbolTable final {
			using Key = core::StringLookupKey<256>;

			cnt::darray<Key> m_values;
			cnt::map<Key, SymbolId> m_ids;

		public:
			SymbolTable() = default;
			SymbolTable(const SymbolTable&) = delete;
			SymbolTable(SymbolTable&&) = delete;
			SymbolTable& operator=(const SymbolTable&) = delete;
			SymbolTable& operator=(SymbolTable&&) = delete;

			~SymbolTable() {
				clear();
			}

			//! Interns component-cache symbol text.
			//! \param value Symbol text to intern. Empty values map to an invalid identifier.
			//! \return Stable identifier for \a value.
			GAIA_NODISCARD SymbolId intern(util::str_view value) {
				if (value.empty())
					return {};

				const Key lookup(value.data(), value.size(), 0);
				const auto it = m_ids.find(lookup);
				if (it != m_ids.end())
					return it->second;

				auto* data = mem::AllocHelper::alloc<char>(value.size() + 1);
				memcpy(data, value.data(), value.size());
				data[value.size()] = 0;
				const Key owned(data, value.size(), 1);
				const SymbolId id{(uint32_t)m_values.size()};
				m_values.push_back(owned);
				m_ids.emplace(owned, id);
				return id;
			}

			//! Resolves an interned component-cache symbol.
			//! \param id Identifier returned by intern().
			//! \return Interned string view, or an empty view for an invalid identifier.
			GAIA_NODISCARD util::str_view view(SymbolId id) const noexcept {
				if (!id.valid() || id.value >= m_values.size())
					return {};
				const auto& value = m_values[id.value];
				return {value.str(), value.len()};
			}

			//! Releases all interned symbols.
			void clear() {
				m_ids.clear();
				for (const auto& value: m_values) {
					if (value.str() != nullptr && value.owned())
						mem::AllocHelper::free((void*)value.str());
				}
				m_values.clear();
			}
		};

		//! Runtime cache metadata for one registered Gaia component entity.
		//!
		//! A cache item is the authoritative metadata record used by chunk storage, runtime component registration,
		//! field reflection, lifecycle callbacks, hooks, symbol lookup, and serialization. Instances are created through
		//! the static create helpers and released with destroy().
		struct GAIA_API ComponentCacheItem final {
			GAIA_USE_SMALLBLOCK(ComponentCacheItem);
			friend class ComponentCache;

			//! Maximum stored component and runtime-field symbol length, including the null terminator.
			static constexpr uint32_t MaxNameLength = 256;

			//! Interned lookup key type used for component symbols.
			using SymbolLookupKey = core::StringLookupKey<512>;

			//! Constructs \a cnt component values in raw storage.
			using FuncCtor = void(void*, uint32_t);
			//! Destroys \a cnt component values in raw storage.
			using FuncDtor = void(void*, uint32_t);
			//! Moves or copies component values between different storage layouts.
			using FuncFrom = void(void*, void*, uint32_t, uint32_t, uint32_t, uint32_t);
			//! Copies component values from source storage to destination storage.
			using FuncCopy = void(void*, const void*, uint32_t, uint32_t, uint32_t, uint32_t);
			//! Moves component values from source storage to destination storage.
			using FuncMove = void(void*, void*, uint32_t, uint32_t, uint32_t, uint32_t);
			//! Swaps component values between two storage locations.
			using FuncSwap = void(void*, void*, uint32_t, uint32_t, uint32_t, uint32_t);
			//! Compares two component values for equality.
			using FuncCmp = bool(const void*, const void*);
			//! Creates the typed sparse store selected by compile-time component registration.
			using FuncCreateSparseStore = void(World&, Entity);

			//! Saves a contiguous range of component values through a serializer.
			using FuncSave = void(ser::serializer&, const void*, uint32_t, uint32_t, uint32_t);
			//! Loads a contiguous range of component values through a serializer.
			using FuncLoad = void(ser::serializer&, void*, uint32_t, uint32_t, uint32_t);

			//! Hook called after a component is added to an entity.
			using FuncOnAdd = void(const World& world, const ComponentCacheItem&, Entity);
			//! Hook called before or while a component is deleted from an entity.
			using FuncOnDel = void(const World& world, const ComponentCacheItem&, Entity);
			//! Hook called when a component value is written in a chunk.
			using FuncOnSet = void(const World& world, const ComponentRecord&, Chunk& chunk);

			//! Component entity bound to this cache item.
			Entity entity;
			//! Compact component descriptor stored in archetype/chunk metadata.
			Component comp;
			//! Hash used for component lookup by registered symbol.
			ComponentLookupHash hashLookup;
			//! Per-element byte sizes for SoA components. Unused for AoS components.
			uint8_t soaSizes[meta::StructToTupleMaxTypes];

			//! Interned registered component symbol.
			SymbolId name;
			//! User-facing scoped component path, e.g. "Gameplay.Device".
			util::str path;
			//! Construction callback for this component type.
			FuncCtor* func_ctor{};
			//! Move-construction callback for this component type.
			FuncMove* func_move_ctor{};
			//! Copy-construction callback for this component type.
			FuncCopy* func_copy_ctor{};
			//! Destruction callback for this component type.
			FuncDtor* func_dtor{};
			//! Copy callback for this component type.
			FuncCopy* func_copy{};
			//! Move callback for this component type.
			FuncMove* func_move{};
			//! Swap callback for this component type.
			FuncSwap* func_swap{};
			//! Equality callback for this component type.
			FuncCmp* func_cmp{};
			//! Typed sparse-store factory. Null for runtime-created components.
			mutable FuncCreateSparseStore* func_create_sparse_store{};
			//! Serialization callback for saving component values.
			FuncSave* func_save{};
			//! Serialization callback for loading component values.
			FuncLoad* func_load{};
			//! Runtime reflection type kind.
			RuntimeTypeKind typeKind = RuntimeTypeKind::Struct;
			//! Optional named entity identifying the authored semantic.
			Entity semantic = EntityBad;
#if GAIA_JSON_ENABLED
			//! JSON representation applied to this value.
			RuntimeJsonEncoding jsonEncoding = RuntimeJsonEncoding::Default;
#endif
			//! Primitive storage type for enum/bitmask metadata. EntityBad otherwise.
			Entity underlyingType = EntityBad;
			//! Element type for fixed array or dynamic vector metadata. May reference another array/vector type.
			Entity elementType = EntityBad;
			//! Fixed element count for reflected array metadata at this array dimension. 0 otherwise.
			uint32_t elementCount = 0;
			//! Semantic runtime type exposed by opaque metadata. EntityBad otherwise.
			Entity opaqueAsType = EntityBad;
			//! Optional adapter for dynamic sequence metadata.
			const RuntimeSequenceAdapter* sequenceAdapter = nullptr;
			//! Optional adapter for opaque semantic projections.
			const RuntimeOpaqueAdapter* opaqueAdapter = nullptr;

#if GAIA_ENABLE_HOOKS
			//! Component hook callbacks associated with this cache item.
			struct Hooks {
	#if GAIA_ENABLE_ADD_DEL_HOOKS
				//! Callback fired when this component is added to an entity.
				FuncOnAdd* func_add{};
				//! Callback fired when this component is deleted from an entity.
				FuncOnDel* func_del{};
	#endif
	#if GAIA_ENABLE_SET_HOOKS
				//! Callback fired when this component is written.
				FuncOnSet* func_set{};
	#endif
			};
			//! Hook callback storage for this component.
			Hooks comp_hooks;
#endif

		private:
			//! Owning component cache used to resolve reflected runtime field type entities.
			const ComponentCache* m_ownerCache = nullptr;
			//! Non-owning symbol table shared by this item's component and runtime metadata.
			SymbolTable* m_symbols = nullptr;
			//! Intern table owned only by cache items created outside a ComponentCache.
			SymbolTable* m_ownedSymbols = nullptr;
			//! Runtime field metadata registered for this component.
			cnt::darray<RuntimeFieldDesc> m_fields;
			//! Runtime symbolic constant metadata registered for this enum/bitmask type.
			cnt::darray<RuntimeConstantDesc> m_constants;

			//! Returns the physical SoA field-array cardinality for this component.
			//! \param capacity Capacity of the containing chunk.
			//! \return One for unique components, otherwise \a capacity.
			GAIA_NODISCARD uint32_t soa_capacity(uint32_t capacity) const noexcept {
				return entity.kind() == EntityKind::EK_Uni ? 1U : capacity;
			}

			//! Moves the bytes of one type-erased SoA value between storage blocks.
			//! \param pDst Destination SoA storage base.
			//! \param pSrc Source SoA storage base.
			//! \param idxDst Destination value index.
			//! \param idxSrc Source value index.
			//! \param capDst Number of values reserved in each destination field array.
			//! \param capSrc Number of values reserved in each source field array.
			void move_soa_element(
					void* pDst, const void* pSrc, uint32_t idxDst, uint32_t idxSrc, uint32_t capDst,
					uint32_t capSrc) const noexcept {
				capDst = soa_capacity(capDst);
				capSrc = soa_capacity(capSrc);
				const std::span<const uint8_t> fieldSizes{soaSizes, comp.soa()};
				GAIA_FOR(comp.soa()) {
					auto* pD = mem::data_view_policy_soa_erased::set(pDst, comp.alig(), fieldSizes, i, idxDst, capDst);
					const auto* pS = mem::data_view_policy_soa_erased::get(pSrc, comp.alig(), fieldSizes, i, idxSrc, capSrc);
					memmove(pD, pS, soaSizes[i]);
				}
			}

			//! Swaps the bytes of two type-erased SoA values between storage blocks.
			//! \param pLeft Left SoA storage base.
			//! \param pRight Right SoA storage base.
			//! \param idxLeft Left value index.
			//! \param idxRight Right value index.
			//! \param capLeft Number of values reserved in each left field array.
			//! \param capRight Number of values reserved in each right field array.
			void swap_soa_elements(
					void* pLeft, void* pRight, uint32_t idxLeft, uint32_t idxRight, uint32_t capLeft,
					uint32_t capRight) const noexcept {
				capLeft = soa_capacity(capLeft);
				capRight = soa_capacity(capRight);
				const std::span<const uint8_t> fieldSizes{soaSizes, comp.soa()};
				GAIA_FOR(comp.soa()) {
					auto* pL = mem::data_view_policy_soa_erased::set(pLeft, comp.alig(), fieldSizes, i, idxLeft, capLeft);
					auto* pR = mem::data_view_policy_soa_erased::set(pRight, comp.alig(), fieldSizes, i, idxRight, capRight);
					GAIA_FOR_(soaSizes[i], j) {
						const auto value = pL[j];
						pL[j] = pR[j];
						pR[j] = value;
					}
				}
			}

			ComponentCacheItem() = default;
			~ComponentCacheItem() = default;

		public:
			//! Cache items retain symbol-table references and are not copyable.
			ComponentCacheItem(const ComponentCacheItem&) = delete;
			//! Cache items retain symbol-table references and are not movable.
			ComponentCacheItem(ComponentCacheItem&&) = delete;
			//! Cache items retain symbol-table references and are not copy-assignable.
			ComponentCacheItem& operator=(const ComponentCacheItem&) = delete;
			//! Cache items retain symbol-table references and are not move-assignable.
			ComponentCacheItem& operator=(ComponentCacheItem&&) = delete;

			//! Resolves the registered component symbol.
			//! \return Interned component symbol, or an empty view when this item is not initialized.
			GAIA_NODISCARD util::str_view symbol_name() const noexcept {
				return m_symbols != nullptr ? m_symbols->view(name) : util::str_view{};
			}

			//! Move-constructs one component value from another value.
			//! \param pDst Destination component storage base pointer.
			//! \param pSrc Source component storage base pointer.
			//! \param idxDst Destination value index.
			//! \param idxSrc Source value index.
			//! \param sizeDst Destination storage capacity.
			//! \param sizeSrc Source storage capacity.
			void
			ctor_move(void* pDst, void* pSrc, uint32_t idxDst, uint32_t idxSrc, uint32_t sizeDst, uint32_t sizeSrc) const {
				GAIA_ASSERT(pSrc != nullptr && pDst != nullptr);
				GAIA_ASSERT(pSrc != pDst || idxSrc != idxDst);
				if (func_move_ctor != nullptr) {
					func_move_ctor(pDst, pSrc, idxDst, idxSrc, sizeDst, sizeSrc);
					return;
				}

				if (comp.soa() != 0) {
					move_soa_element(pDst, pSrc, idxDst, idxSrc, sizeDst, sizeSrc);
					return;
				}
				if (comp.size() == 0)
					return;

				auto* pD = (uint8_t*)pDst + ((uintptr_t)comp.size() * idxDst);
				auto* pS = (uint8_t*)pSrc + ((uintptr_t)comp.size() * idxSrc);
				memmove((void*)pD, (const void*)pS, comp.size());
			}

			//! Copy-constructs one component value from another value.
			//! \param pDst Destination component storage base pointer.
			//! \param pSrc Source component storage base pointer.
			//! \param idxDst Destination value index.
			//! \param idxSrc Source value index.
			//! \param sizeDst Destination storage capacity.
			//! \param sizeSrc Source storage capacity.
			void ctor_copy(
					void* pDst, const void* pSrc, uint32_t idxDst, uint32_t idxSrc, uint32_t sizeDst, uint32_t sizeSrc) const {
				GAIA_ASSERT(pSrc != nullptr && pDst != nullptr);
				GAIA_ASSERT(pSrc != pDst || idxSrc != idxDst);
				if (func_copy_ctor != nullptr) {
					func_copy_ctor(pDst, pSrc, idxDst, idxSrc, sizeDst, sizeSrc);
					return;
				}

				if (comp.soa() != 0) {
					move_soa_element(pDst, pSrc, idxDst, idxSrc, sizeDst, sizeSrc);
					return;
				}
				if (comp.size() == 0)
					return;

				auto* pD = (uint8_t*)pDst + ((uintptr_t)comp.size() * idxDst);
				auto* pS = (const uint8_t*)pSrc + ((uintptr_t)comp.size() * idxSrc);
				memcpy((void*)pD, (const void*)pS, comp.size());
			}

			//! Destroys one component value when a destructor callback is registered.
			//! \param pSrc Component value pointer.
			void dtor(void* pSrc) const {
				if (func_dtor != nullptr)
					func_dtor(pSrc, 1);
			}

			//! Copies one existing component value into another value.
			//! \param pDst Destination component storage base pointer.
			//! \param pSrc Source component storage base pointer.
			//! \param idxDst Destination value index.
			//! \param idxSrc Source value index.
			//! \param sizeDst Destination storage capacity.
			//! \param sizeSrc Source storage capacity.
			void
			copy(void* pDst, const void* pSrc, uint32_t idxDst, uint32_t idxSrc, uint32_t sizeDst, uint32_t sizeSrc) const {
				GAIA_ASSERT(pSrc != nullptr && pDst != nullptr);
				GAIA_ASSERT(pSrc != pDst || idxSrc != idxDst);
				if (func_copy != nullptr) {
					func_copy(pDst, pSrc, idxDst, idxSrc, sizeDst, sizeSrc);
					return;
				}

				if (comp.soa() != 0) {
					move_soa_element(pDst, pSrc, idxDst, idxSrc, sizeDst, sizeSrc);
					return;
				}
				if (comp.size() == 0)
					return;

				auto* pD = (uint8_t*)pDst + ((uintptr_t)comp.size() * idxDst);
				auto* pS = (const uint8_t*)pSrc + ((uintptr_t)comp.size() * idxSrc);
				memcpy((void*)pD, (const void*)pS, comp.size());
			}

			//! Moves one existing component value into another value.
			//! \param pDst Destination component storage base pointer.
			//! \param pSrc Source component storage base pointer.
			//! \param idxDst Destination value index.
			//! \param idxSrc Source value index.
			//! \param sizeDst Destination storage capacity.
			//! \param sizeSrc Source storage capacity.
			void move(void* pDst, void* pSrc, uint32_t idxDst, uint32_t idxSrc, uint32_t sizeDst, uint32_t sizeSrc) const {
				GAIA_ASSERT(pSrc != nullptr && pDst != nullptr);
				GAIA_ASSERT(pSrc != pDst || idxSrc != idxDst);
				if (func_move != nullptr) {
					func_move(pDst, pSrc, idxDst, idxSrc, sizeDst, sizeSrc);
					return;
				}

				if (comp.soa() != 0) {
					move_soa_element(pDst, pSrc, idxDst, idxSrc, sizeDst, sizeSrc);
					return;
				}
				if (comp.size() == 0)
					return;

				auto* pD = (uint8_t*)pDst + ((uintptr_t)comp.size() * idxDst);
				auto* pS = (uint8_t*)pSrc + ((uintptr_t)comp.size() * idxSrc);
				memmove((void*)pD, (const void*)pS, comp.size());
			}

			//! Swaps two component values.
			//! \param pLeft Left component storage base pointer.
			//! \param pRight Right component storage base pointer.
			//! \param idxLeft Left value index.
			//! \param idxRight Right value index.
			//! \param sizeDst Left storage capacity.
			//! \param sizeSrc Right storage capacity.
			void
			swap(void* pLeft, void* pRight, uint32_t idxLeft, uint32_t idxRight, uint32_t sizeDst, uint32_t sizeSrc) const {
				GAIA_ASSERT(pLeft != nullptr && pRight != nullptr);
				if (func_swap != nullptr) {
					func_swap(pLeft, pRight, idxLeft, idxRight, sizeDst, sizeSrc);
					return;
				}

				if (comp.soa() != 0) {
					swap_soa_elements(pLeft, pRight, idxLeft, idxRight, sizeDst, sizeSrc);
					return;
				}
				if (comp.size() == 0)
					return;

				auto* l = (uint8_t*)pLeft + ((uintptr_t)comp.size() * idxLeft);
				auto* r = (uint8_t*)pRight + ((uintptr_t)comp.size() * idxRight);
				GAIA_FOR(comp.size()) {
					const auto tmp = l[i];
					l[i] = r[i];
					r[i] = tmp;
				}
			}

			//! Compares two component values of this type.
			//! \param pLeft Left component value pointer.
			//! \param pRight Right component value pointer.
			//! \return True when the values compare equal.
			bool cmp(const void* pLeft, const void* pRight) const {
				GAIA_ASSERT(pLeft != pRight);
				if (func_cmp != nullptr)
					return func_cmp(pLeft, pRight);

				if (comp.soa() != 0 || comp.size() == 0)
					return true;

				return memcmp(pLeft, pRight, comp.size()) == 0;
			}

			//! Saves a contiguous range of component values.
			//! \param serializer Destination serializer.
			//! \param pSrc Source component storage base pointer.
			//! \param from First value index to save.
			//! \param to One-past-last value index to save.
			//! \param cap Value capacity of the source storage.
			void save(ser::serializer& serializer, const void* pSrc, uint32_t from, uint32_t to, uint32_t cap) const {
				GAIA_ASSERT(serializer.valid() && pSrc != nullptr && from < to && to <= cap);
				if (func_save != nullptr) {
					func_save(serializer, pSrc, from, to, cap);
					return;
				}

				if (comp.soa() != 0 || comp.size() == 0)
					return;

				const auto* pBase = (const uint8_t*)pSrc;
				GAIA_FOR2(from, to) {
					const auto* p = pBase + ((uintptr_t)comp.size() * i);
					serializer.save_raw((const void*)p, comp.size(), ser::serialization_type_id::trivial_wrapper);
				}
			}

			//! Loads a contiguous range of component values.
			//! \param serializer Source serializer.
			//! \param pDst Destination component storage base pointer.
			//! \param from First value index to load.
			//! \param to One-past-last value index to load.
			//! \param cap Value capacity of the destination storage.
			void load(ser::serializer& serializer, void* pDst, uint32_t from, uint32_t to, uint32_t cap) const {
				GAIA_ASSERT(serializer.valid() && pDst != nullptr && from < to && to <= cap);
				if (func_load != nullptr) {
					func_load(serializer, pDst, from, to, cap);
					return;
				}

				if (comp.soa() != 0 || comp.size() == 0)
					return;

				auto* pBase = (uint8_t*)pDst;
				GAIA_FOR2(from, to) {
					auto* p = pBase + ((uintptr_t)comp.size() * i);
					serializer.load_raw((void*)p, comp.size(), ser::serialization_type_id::trivial_wrapper);
				}
			}

			//! \return True when this component has runtime field metadata.
			GAIA_NODISCARD bool has_fields() const {
				return !m_fields.empty();
			}

			//! Gets the element count represented by runtime field metadata.
			//! StringType selects registration or cache-owned field metadata.
			//! \param field Field metadata to inspect.
			//! Returns 1 for scalar fields, otherwise the fixed inline array count.
			template <typename StringType>
			GAIA_NODISCARD static uint32_t field_element_count(const RuntimeFieldMeta<StringType>& field) noexcept {
				return field.count == 0 ? 1U : field.count;
			}

			//! Gets the byte size of a reflected primitive runtime type entity.
			//! \param type Primitive type entity.
			//! \return Primitive byte size, or 0 when \a type is not a reflected primitive type.
			GAIA_NODISCARD static uint32_t primitive_type_size(Entity type) noexcept {
				ser::serialization_type_id id = ser::serialization_type_id::ignore;
				if (!runtime_primitive_serialization_type(type, id))
					return 0;
				return ser::serialization_type_size(id, 0);
			}

			//! Looks up runtime field metadata by index.
			//! \param index Field index.
			//! \return Field metadata pointer when found, nullptr otherwise.
			GAIA_NODISCARD const RuntimeFieldDesc* field(uint32_t index) const noexcept {
				return index < m_fields.size() ? &m_fields[index] : nullptr;
			}

			//! Looks up runtime field metadata by name.
			//! \param fieldName Field name.
			//! \return Field metadata pointer when found, nullptr otherwise.
			GAIA_NODISCARD const RuntimeFieldDesc* field(util::str_view fieldName) const noexcept {
				if (fieldName.empty() || fieldName.size() >= MaxNameLength)
					return nullptr;

				for (const auto& field: m_fields) {
					if (field_name(field) == fieldName)
						return &field;
				}
				return nullptr;
			}

			//! Resolves a stored runtime field name.
			//! \param field Stored field metadata.
			//! Returns the interned field name.
			GAIA_NODISCARD util::str_view field_name(const RuntimeFieldDesc& field) const noexcept {
				return m_symbols != nullptr ? m_symbols->view(field.name) : util::str_view{};
			}

			//! Resolves a stored runtime field unit.
			//! \param field Stored field metadata.
			//! Returns the interned unit label, or an empty view when no unit was authored.
			GAIA_NODISCARD util::str_view field_unit(const RuntimeFieldDesc& field) const noexcept {
				return m_symbols != nullptr ? m_symbols->view(field.unit) : util::str_view{};
			}

			//! \return Number of runtime fields registered on this component.
			GAIA_NODISCARD uint32_t field_count() const noexcept {
				return (uint32_t)m_fields.size();
			}

			//! \return Owning component cache, or nullptr before the item is registered in a cache.
			GAIA_NODISCARD const ComponentCache* owner_cache() const noexcept {
				return m_ownerCache;
			}

			//! \return Primitive type entity represented by this metadata, or EntityBad for non-primitive metadata.
			GAIA_NODISCARD Entity primitive_type() const noexcept {
				if (typeKind == RuntimeTypeKind::Primitive)
					return entity;
				if (typeKind == RuntimeTypeKind::Enum || typeKind == RuntimeTypeKind::Bitmask)
					return underlyingType;
				return EntityBad;
			}

			//! \return Element type entity for reflected sequence metadata, or EntityBad otherwise.
			GAIA_NODISCARD Entity element_type() const noexcept {
				return typeKind == RuntimeTypeKind::Array || typeKind == RuntimeTypeKind::Vector ? elementType : EntityBad;
			}

			//! \return Fixed element count for arrays, or 0 for non-sequences and dynamic vector/list metadata.
			GAIA_NODISCARD uint32_t element_count() const noexcept {
				return typeKind == RuntimeTypeKind::Array ? elementCount : 0;
			}

			//! \return Semantic runtime type exposed by opaque metadata, or EntityBad otherwise.
			GAIA_NODISCARD Entity opaque_as_type() const noexcept {
				return typeKind == RuntimeTypeKind::Opaque ? opaqueAsType : EntityBad;
			}

			//! \return Adapter for reflected dynamic sequence metadata, or nullptr otherwise.
			GAIA_NODISCARD const RuntimeSequenceAdapter* sequence_adapter() const noexcept {
				return typeKind == RuntimeTypeKind::Vector ? sequenceAdapter : nullptr;
			}

			//! \return Adapter for opaque semantic projections, or nullptr otherwise.
			GAIA_NODISCARD const RuntimeOpaqueAdapter* opaque_adapter() const noexcept {
				return typeKind == RuntimeTypeKind::Opaque ? opaqueAdapter : nullptr;
			}

			//! \return True when this component has a custom serializer callback.
			GAIA_NODISCARD bool has_custom_serializer() const noexcept {
				return func_save != nullptr;
			}

			//! \return True when this component has a custom deserializer callback.
			GAIA_NODISCARD bool has_custom_deserializer() const noexcept {
				return func_load != nullptr;
			}

			//! Looks up runtime enum/bitmask constant metadata by index.
			//! \param index Constant index.
			//! \return Constant metadata pointer when found, nullptr otherwise.
			GAIA_NODISCARD const RuntimeConstantDesc* constant(uint32_t index) const noexcept {
				return index < m_constants.size() ? &m_constants[index] : nullptr;
			}

			//! Looks up runtime enum/bitmask constant metadata by name.
			//! \param constantName Constant name.
			//! \return Constant metadata pointer when found, nullptr otherwise.
			GAIA_NODISCARD const RuntimeConstantDesc* constant(util::str_view constantName) const noexcept {
				if (constantName.empty() || constantName.size() >= MaxNameLength)
					return nullptr;

				for (const auto& constant: m_constants) {
					if (constant_name(constant) == constantName)
						return &constant;
				}
				return nullptr;
			}

			//! Resolves a stored runtime constant name.
			//! \param constant Stored constant metadata.
			//! \return Interned constant name.
			GAIA_NODISCARD util::str_view constant_name(const RuntimeConstantDesc& constant) const noexcept {
				return m_symbols != nullptr ? m_symbols->view(constant.name) : util::str_view{};
			}

			//! Looks up runtime enum/bitmask constant metadata by exact value.
			//! \param value Constant value.
			//! \return Constant metadata pointer when found, nullptr otherwise.
			GAIA_NODISCARD const RuntimeConstantDesc* constant_by_value(int64_t value) const noexcept {
				for (const auto& constant: m_constants) {
					if (constant.value == value)
						return &constant;
				}
				return nullptr;
			}

			//! \return Number of runtime enum/bitmask constants registered on this type.
			GAIA_NODISCARD uint32_t constant_count() const noexcept {
				return (uint32_t)m_constants.size();
			}

#if GAIA_ENABLE_HOOKS
			//! \return Mutable hook callback storage for this component.
			Hooks& hooks() {
				return comp_hooks;
			}

			//! \return Read-only hook callback storage for this component.
			const Hooks& hooks() const {
				return comp_hooks;
			}

#endif

			//! Calculates the next aligned memory offset after storing \a cnt values of this component.
			//! \param addr Starting byte offset.
			//! \param cnt Number of component values to reserve.
			//! \return Byte offset after the component storage block.
			GAIA_NODISCARD uint32_t calc_new_mem_offset(uint32_t addr, size_t cnt) const noexcept {
				if (comp.soa() == 0) {
					addr = (uint32_t)mem::detail::get_aligned_byte_offset(addr, comp.alig(), comp.size(), cnt);
				} else {
					GAIA_FOR(comp.soa()) {
						addr = (uint32_t)mem::detail::get_aligned_byte_offset(addr, comp.alig(), soaSizes[i], cnt);
					}
					// TODO: Magic offset. Otherwise, SoA data might leak past the chunk boundary when accessing
					//       the last element. By faking the memory offset we can bypass this is issue for now.
					//       Obviously, this needs fixing at some point.
					addr += comp.soa() * 12;
				}
				return addr;
			}

		private:
			//! Builds a stable component symbol from compiler type-info text.
			//! \tparam T Component type to register.
			//! \param nameTmp Output buffer receiving the normalized null-terminated name.
			//! \return Length of the normalized component name, excluding the null terminator.
			template <typename T>
			GAIA_NODISCARD static uint32_t init_type_name(char (&nameTmp)[MaxNameLength]) {
				auto ct_name = detail::ComponentDesc<T>::name();

				// Allocate enough memory for the name string + the null-terminating character (
				// the compile time string returned by ComponentDesc<T>::name is not null-terminated).
				// Different compilers will give a bit different strings, e.g.:
				//   Clang/GCC: gaia::ecs::uni<Position>
				//   MSVC     : gaia::ecs::uni<struct Position>
				// Therefore, we first copy the compile-time string and then tweak it so it is
				// the same on all supported compilers.
				auto nameTmpLen = (uint32_t)ct_name.size();
				GAIA_ASSERT(nameTmpLen < MaxNameLength);
				memcpy((void*)nameTmp, (const void*)ct_name.data(), nameTmpLen);
				nameTmp[ct_name.size()] = 0;

				auto strip_prefix = [&](const char* prefix, uint32_t prefixLen) {
					if (nameTmpLen <= prefixLen || strncmp(nameTmp, prefix, prefixLen) != 0)
						return;

					memmove(nameTmp, nameTmp + prefixLen, nameTmpLen - prefixLen + 1);
					nameTmpLen -= prefixLen;
				};

				strip_prefix("const ", 6);

				const uint32_t NSubstrings = 3;
				const char* to_remove[NSubstrings] = {"class ", "struct ", "enum "};
				const uint32_t to_remove_len[NSubstrings] = {6, 7, 5};
				GAIA_FOR(NSubstrings) {
					strip_prefix(to_remove[i], to_remove_len[i]);
				}

				while (nameTmpLen > 0) {
					const auto ch = nameTmp[nameTmpLen - 1];
					if (ch != ' ' && ch != '&' && ch != '*')
						break;

					nameTmp[--nameTmpLen] = 0;
				}

				if (nameTmpLen > 6 && strncmp(nameTmp + nameTmpLen - 6, " const", 6) == 0) {
					nameTmpLen -= 6;
					nameTmp[nameTmpLen] = 0;
				}

				// Normalization template names by removing keywords when they appear as template argument
				// prefixes instead of as part of a longer identifier.
				GAIA_FOR(NSubstrings) {
					const auto* str = to_remove[i];
					const auto len = to_remove_len[i];

					auto* pos = nameTmp;
					while ((pos = strstr(pos, str)) != nullptr) {
						const bool isBoundary = pos == nameTmp || pos[-1] == '<' || pos[-1] == ',' || pos[-1] == ' ';
						if (!isBoundary) {
							++pos;
							continue;
						}

						const auto tailMaxLen = (size_t)(MaxNameLength - (uint32_t)(pos + len - nameTmp));
						memmove(pos, pos + len, GAIA_STRLEN(pos + len, tailMaxLen) + 1);
						nameTmpLen -= len;
					}
				}

				return nameTmpLen;
			}

			//! Copies one runtime field initializer into immutable component metadata.
			//! \param desc Field initializer to copy.
			//! \return True when copied, false when the field name is invalid.
			GAIA_NODISCARD bool copy_runtime_field(const RuntimeFieldInit& desc) {
				if (m_symbols == nullptr || desc.name.empty() || desc.name.size() >= MaxNameLength ||
						desc.unit.size() >= RuntimeFieldDesc::MaxUnitLength)
					return false;

				RuntimeFieldDesc field{};
				field.name = m_symbols->intern(desc.name);
				field.type = desc.type;
				field.offset = desc.offset;
				field.count = desc.count;
				field.semantic = desc.semantic;
#if GAIA_JSON_ENABLED
				field.jsonEncoding = desc.jsonEncoding;
#endif
				field.flags = desc.flags;
				field.unit = m_symbols->intern(desc.unit);
				field.minimum = desc.minimum;
				field.maximum = desc.maximum;
				field.step = desc.step;
				m_fields.push_back(field);
				return true;
			}

			//! Copies one runtime constant initializer into immutable component metadata.
			//! \param desc Constant initializer to copy.
			//! \return True when copied, false when the constant name is invalid.
			GAIA_NODISCARD bool copy_runtime_constant(const RuntimeConstantInit& desc) {
				if (m_symbols == nullptr || desc.name.empty() || desc.name.size() >= MaxNameLength)
					return false;

				RuntimeConstantDesc constant{};
				constant.name = m_symbols->intern(desc.name);
				constant.value = desc.value;
				m_constants.push_back(constant);
				return true;
			}

			template <typename T>
			GAIA_NODISCARD static ComponentCacheItem*
			create_typed(Entity entity, SymbolTable& symbols, const RuntimeTypeDesc* pRuntimeType) {
				static_assert(core::is_raw_v<T>);

				constexpr auto componentSize = detail::ComponentDesc<T>::size();
				static_assert(
						componentSize < Component::MaxComponentSizeInBytes,
						"Trying to register a component larger than the maximum allowed component size! In the future this "
						"restriction won't apply to components not stored inside archetype chunks.");

				char nameTmp[MaxNameLength];
				const auto nameTmpLen = init_type_name<T>(nameTmp);

				uint8_t soaSizes[meta::StructToTupleMaxTypes]{};
				RuntimeFieldInit fields[meta::StructToTupleMaxTypes]{};
				auto desc = detail::ComponentDesc<T>::make(
						util::str_view(nameTmp, nameTmpLen), std::span<uint8_t, meta::StructToTupleMaxTypes>{soaSizes});
				if (pRuntimeType != nullptr) {
					desc.runtimeType = *pRuntimeType;
				}
#if GAIA_ECS_AUTO_COMPONENT_FIELDS
				else {
					desc.runtimeType.fields = fields;
					desc.runtimeType.fieldCount =
							detail::ComponentDesc<T>::auto_fields(std::span<RuntimeFieldInit, meta::StructToTupleMaxTypes>{fields});
				}
#endif
				return create(entity, symbols, desc);
			}

		public:
			//! Creates standalone metadata for a compile-time C++ component type.
			//! Template parameter T is the component type to register.
			//! \param entity Component entity that owns the resulting metadata.
			//! Returns a newly allocated component cache item. Release with destroy().
			template <typename T>
			GAIA_NODISCARD static ComponentCacheItem* create(Entity entity) {
				auto* symbols = new SymbolTable();
				auto* item = create<T>(entity, *symbols);
				item->m_ownedSymbols = symbols;
				return item;
			}

		private:
			//! Creates metadata for a compile-time C++ component type.
			//! Template parameter T is the component type to register.
			//! \param entity Component entity that owns the resulting metadata.
			//! \return Newly allocated component cache item. Release with destroy().
			template <typename T>
			GAIA_NODISCARD static ComponentCacheItem* create(Entity entity, SymbolTable& symbols) {
				return create_typed<T>(entity, symbols, nullptr);
			}

		public:
			//! Creates standalone metadata for a compile-time C++ component type with explicit runtime metadata.
			//! Template parameter T is the component type to register.
			//! \param entity Component entity that owns the resulting metadata.
			//! \param runtimeType Reflection-only metadata attached to the typed component.
			//! Returns a newly allocated component cache item. Release with destroy().
			template <typename T>
			GAIA_NODISCARD static ComponentCacheItem* create(Entity entity, const RuntimeTypeDesc& runtimeType) {
				auto* symbols = new SymbolTable();
				auto* item = create<T>(entity, *symbols, runtimeType);
				item->m_ownedSymbols = symbols;
				return item;
			}

		private:
			//! Creates metadata for a compile-time C++ component type with explicit runtime type metadata.
			//! Template parameter T is the component type to register.
			//! \param entity Component entity that owns the resulting metadata.
			//! \param runtimeType Reflection-only metadata attached to the typed component.
			//! \return Newly allocated component cache item. Release with destroy().
			template <typename T>
			GAIA_NODISCARD static ComponentCacheItem*
			create(Entity entity, SymbolTable& symbols, const RuntimeTypeDesc& runtimeType) {
				return create_typed<T>(entity, symbols, &runtimeType);
			}

			//! Creates metadata from a plain component descriptor.
			//! \param entity Component entity that owns the resulting metadata.
			//! \param desc Component descriptor describing storage, lifecycle, and runtime type metadata.
			//! \return Newly allocated component cache item. Release with destroy().
			GAIA_NODISCARD static ComponentCacheItem*
			create(Entity entity, SymbolTable& symbols, const ecs::ComponentDesc& desc) {
				GAIA_ASSERT(!desc.name.empty());
				GAIA_ASSERT(desc.name.size() < MaxNameLength);
				GAIA_ASSERT(desc.size < Component::MaxComponentSizeInBytes);
				GAIA_ASSERT((desc.size == 0 && desc.alig == 0) || (desc.alig > 0 && desc.alig < Component::MaxAlignment));
				GAIA_ASSERT(desc.alig == 0 || (desc.alig & (desc.alig - 1)) == 0);
				GAIA_ASSERT(desc.soa <= meta::StructToTupleMaxTypes);
				GAIA_ASSERT(desc.soa == 0 || desc.pSoaSizes != nullptr);
#if GAIA_ASSERT_ENABLED
				if (desc.soa > 0) {
					uint32_t soaSize = 0;
					GAIA_FOR(desc.soa) {
						GAIA_ASSERT(desc.pSoaSizes[i] != 0);
						soaSize += desc.pSoaSizes[i];
					}
					GAIA_ASSERT(soaSize <= desc.size);
				}
#endif

				auto* cci = new ComponentCacheItem();
				cci->m_symbols = &symbols;
				cci->entity = entity;
				cci->comp = Component(entity.id(), desc.soa, desc.size, desc.alig, desc.storageType);
				cci->hashLookup = desc.hashLookup.hash != 0
															? desc.hashLookup
															: ComponentLookupHash{core::calculate_hash64(desc.name.data(), desc.name.size())};

				if (desc.soa > 0) {
					GAIA_FOR(desc.soa) cci->soaSizes[i] = desc.pSoaSizes[i];
				}

				cci->name = symbols.intern(desc.name);
				GAIA_ASSERT(cci->name.valid());

				cci->func_ctor = desc.funcCtor;
				cci->func_move_ctor = desc.funcMoveCtor;
				cci->func_copy_ctor = desc.funcCopyCtor;
				cci->func_dtor = desc.funcDtor;
				cci->func_copy = desc.funcCopy;
				cci->func_move = desc.funcMove;
				cci->func_swap = desc.funcSwap;
				cci->func_cmp = desc.funcCmp;
				cci->func_save = desc.funcSave;
				cci->func_load = desc.funcLoad;

				const auto& runtimeType = desc.runtimeType;
				cci->typeKind = runtimeType.typeKind;
				cci->semantic = runtimeType.semantic;
#if GAIA_JSON_ENABLED
				cci->jsonEncoding = runtimeType.jsonEncoding;
#endif
				cci->underlyingType = runtimeType.underlyingType;
				cci->elementType = runtimeType.elementType;
				cci->elementCount = runtimeType.elementCount;
				cci->opaqueAsType = runtimeType.opaqueAsType;
				cci->sequenceAdapter = runtimeType.sequenceAdapter;
				cci->opaqueAdapter = runtimeType.opaqueAdapter;

				if (runtimeType.fieldCount > 0) {
					GAIA_FOR(runtimeType.fieldCount) {
						const bool copied = cci->copy_runtime_field(runtimeType.fields[i]);
						GAIA_ASSERT(copied);
					}
				}

				if (runtimeType.constantCount > 0) {
					GAIA_FOR(runtimeType.constantCount) {
						const bool copied = cci->copy_runtime_constant(runtimeType.constants[i]);
						GAIA_ASSERT(copied);
					}
				}

				return cci;
			}

		public:
			//! Creates standalone metadata from a plain component descriptor.
			//! \param entity Component entity that owns the resulting metadata.
			//! \param desc Component descriptor describing storage, lifecycle, and runtime type metadata.
			//! Returns a newly allocated component cache item. Release with destroy().
			GAIA_NODISCARD static ComponentCacheItem* create(Entity entity, const ecs::ComponentDesc& desc) {
				auto* symbols = new SymbolTable();
				auto* item = create(entity, *symbols, desc);
				item->m_ownedSymbols = symbols;
				return item;
			}

			//! Releases a cache item and any owned symbol storage.
			//! \param pItem Cache item created by create(). Null is accepted.
			static void destroy(ComponentCacheItem* pItem) {
				if (pItem == nullptr)
					return;

				auto* ownedSymbols = pItem->m_ownedSymbols;
				delete pItem;
				delete ownedSymbols;
			}
		};
	} // namespace ecs
} // namespace gaia
//! \endcond
