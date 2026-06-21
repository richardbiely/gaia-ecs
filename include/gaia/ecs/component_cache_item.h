#pragma once
#include "gaia/config/config.h"

#include <cstdint>
#include <cstring>
#include <type_traits>

#include "gaia/cnt/darray.h"
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

namespace gaia {
	namespace ecs {
		class World;
		class Chunk;
		struct ComponentRecord;

		//! Runtime cache metadata for one registered Gaia component entity.
		//!
		//! A cache item is the authoritative metadata record used by chunk storage, runtime component registration,
		//! field reflection, lifecycle callbacks, hooks, symbol lookup, and serialization. Instances are created through
		//! the static create helpers and released with destroy().
		struct GAIA_API ComponentCacheItem final {
			GAIA_USE_SMALLBLOCK(ComponentCacheItem);

			//! Maximum stored component and runtime-field symbol length, including the null terminator.
			static constexpr uint32_t MaxNameLength = 256;

			//! Interned lookup key type used for component symbols.
			using SymbolLookupKey = core::StringLookupKey<512>;

			//! Constructs @a cnt component values in raw storage.
			using FuncCtor = void(void*, uint32_t);
			//! Destroys @a cnt component values in raw storage.
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

			//! Stored runtime field metadata type.
			using RuntimeField = ecs::RuntimeField;

			//! Component item registration context.
			struct ComponentCacheItemCtx {
				//! Registered component symbol.
				util::str_view name{};
				//! Component payload size in bytes.
				uint32_t size = 0;
				//! Component payload alignment in bytes.
				uint32_t alig = 0;
				//! Component storage mode.
				DataStorageType storageType = DataStorageType::Table;
				//! Number of SoA elements, 0 means AoS.
				uint32_t soa = 0;
				//! Per-element SoA sizes when \ref soa is non-zero.
				const uint8_t* pSoaSizes = nullptr;
				//! Optional explicit lookup hash. When empty, the symbol hash is used.
				ComponentLookupHash hashLookup{};
				//! Runtime reflection type kind.
				RuntimeTypeKind typeKind = RuntimeTypeKind::Struct;
				//! Runtime primitive kind. Only valid when typeKind is Primitive.
				RuntimePrimitiveKind primitiveKind = RuntimePrimitiveKind::None;
				//! Optional construction callback.
				FuncCtor* funcCtor = nullptr;
				//! Optional move-construction callback.
				FuncMove* funcMoveCtor = nullptr;
				//! Optional copy-construction callback.
				FuncCopy* funcCopyCtor = nullptr;
				//! Optional destruction callback.
				FuncDtor* funcDtor = nullptr;
				//! Optional copy-assignment callback.
				FuncCopy* funcCopy = nullptr;
				//! Optional move-assignment callback.
				FuncMove* funcMove = nullptr;
				//! Optional swap callback.
				FuncSwap* funcSwap = nullptr;
				//! Optional equality comparison callback.
				FuncCmp* funcCmp = nullptr;
				//! Optional serialization save callback.
				FuncSave* funcSave = nullptr;
				//! Optional serialization load callback.
				FuncLoad* funcLoad = nullptr;
			};

			//! Component entity bound to this cache item.
			Entity entity;
			//! Compact component descriptor stored in archetype/chunk metadata.
			Component comp;
			//! Hash used for component lookup by registered symbol.
			ComponentLookupHash hashLookup;
			//! Per-element byte sizes for SoA components; unused for AoS components.
			uint8_t soaSizes[meta::StructToTupleMaxTypes];

			//! Registered component symbol.
			SymbolLookupKey name;
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
			//! Serialization callback for saving component values.
			FuncSave* func_save{};
			//! Serialization callback for loading component values.
			FuncLoad* func_load{};
			//! Runtime reflection type kind.
			RuntimeTypeKind typeKind = RuntimeTypeKind::Struct;
			//! Runtime primitive kind. Only valid when typeKind is Primitive.
			RuntimePrimitiveKind primitiveKind = RuntimePrimitiveKind::None;

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
			//! Runtime field metadata registered for this component.
			cnt::darray<RuntimeField> fields;

		private:
			//! Creates an empty cache item. Use create() to populate metadata.
			ComponentCacheItem() = default;
			//! Destroys the cache item shell. Use destroy() so owned symbol memory is released first.
			~ComponentCacheItem() = default;

		public:
			//! Cache items own symbol memory and are not copyable.
			ComponentCacheItem(const ComponentCacheItem&) = delete;
			//! Cache items own symbol memory and are not movable.
			ComponentCacheItem(ComponentCacheItem&&) = delete;
			//! Cache items own symbol memory and are not copy-assignable.
			ComponentCacheItem& operator=(const ComponentCacheItem&) = delete;
			//! Cache items own symbol memory and are not move-assignable.
			ComponentCacheItem& operator=(ComponentCacheItem&&) = delete;

			//! Move-constructs one component value from another value.
			//! @param pDst Destination component storage base pointer.
			//! @param pSrc Source component storage base pointer.
			//! @param idxDst Destination value index.
			//! @param idxSrc Source value index.
			//! @param sizeDst Destination value stride in bytes.
			//! @param sizeSrc Source value stride in bytes.
			void
			ctor_move(void* pDst, void* pSrc, uint32_t idxDst, uint32_t idxSrc, uint32_t sizeDst, uint32_t sizeSrc) const {
				GAIA_ASSERT(pSrc != nullptr && pDst != nullptr);
				GAIA_ASSERT(pSrc != pDst || idxSrc != idxDst);
				if (func_move_ctor != nullptr) {
					func_move_ctor(pDst, pSrc, idxDst, idxSrc, sizeDst, sizeSrc);
					return;
				}

				if (comp.soa() != 0 || comp.size() == 0)
					return;

				auto* pD = (uint8_t*)pDst + ((uintptr_t)comp.size() * idxDst);
				auto* pS = (uint8_t*)pSrc + ((uintptr_t)comp.size() * idxSrc);
				memmove((void*)pD, (const void*)pS, comp.size());
			}

			//! Copy-constructs one component value from another value.
			//! @param pDst Destination component storage base pointer.
			//! @param pSrc Source component storage base pointer.
			//! @param idxDst Destination value index.
			//! @param idxSrc Source value index.
			//! @param sizeDst Destination value stride in bytes.
			//! @param sizeSrc Source value stride in bytes.
			void ctor_copy(
					void* pDst, const void* pSrc, uint32_t idxDst, uint32_t idxSrc, uint32_t sizeDst, uint32_t sizeSrc) const {
				GAIA_ASSERT(pSrc != nullptr && pDst != nullptr);
				GAIA_ASSERT(pSrc != pDst || idxSrc != idxDst);
				if (func_copy_ctor != nullptr) {
					func_copy_ctor(pDst, pSrc, idxDst, idxSrc, sizeDst, sizeSrc);
					return;
				}

				if (comp.soa() != 0 || comp.size() == 0)
					return;

				auto* pD = (uint8_t*)pDst + ((uintptr_t)comp.size() * idxDst);
				auto* pS = (const uint8_t*)pSrc + ((uintptr_t)comp.size() * idxSrc);
				memcpy((void*)pD, (const void*)pS, comp.size());
			}

			//! Destroys one component value when a destructor callback is registered.
			//! @param pSrc Component value pointer.
			void dtor(void* pSrc) const {
				if (func_dtor != nullptr)
					func_dtor(pSrc, 1);
			}

			//! Copies one existing component value into another value.
			//! @param pDst Destination component storage base pointer.
			//! @param pSrc Source component storage base pointer.
			//! @param idxDst Destination value index.
			//! @param idxSrc Source value index.
			//! @param sizeDst Destination value stride in bytes.
			//! @param sizeSrc Source value stride in bytes.
			void
			copy(void* pDst, const void* pSrc, uint32_t idxDst, uint32_t idxSrc, uint32_t sizeDst, uint32_t sizeSrc) const {
				GAIA_ASSERT(pSrc != nullptr && pDst != nullptr);
				GAIA_ASSERT(pSrc != pDst || idxSrc != idxDst);
				if (func_copy != nullptr) {
					func_copy(pDst, pSrc, idxDst, idxSrc, sizeDst, sizeSrc);
					return;
				}

				if (comp.soa() != 0 || comp.size() == 0)
					return;

				auto* pD = (uint8_t*)pDst + ((uintptr_t)comp.size() * idxDst);
				auto* pS = (const uint8_t*)pSrc + ((uintptr_t)comp.size() * idxSrc);
				memcpy((void*)pD, (const void*)pS, comp.size());
			}

			//! Moves one existing component value into another value.
			//! @param pDst Destination component storage base pointer.
			//! @param pSrc Source component storage base pointer.
			//! @param idxDst Destination value index.
			//! @param idxSrc Source value index.
			//! @param sizeDst Destination value stride in bytes.
			//! @param sizeSrc Source value stride in bytes.
			void move(void* pDst, void* pSrc, uint32_t idxDst, uint32_t idxSrc, uint32_t sizeDst, uint32_t sizeSrc) const {
				GAIA_ASSERT(pSrc != nullptr && pDst != nullptr);
				GAIA_ASSERT(pSrc != pDst || idxSrc != idxDst);
				if (func_move != nullptr) {
					func_move(pDst, pSrc, idxDst, idxSrc, sizeDst, sizeSrc);
					return;
				}

				if (comp.soa() != 0 || comp.size() == 0)
					return;

				auto* pD = (uint8_t*)pDst + ((uintptr_t)comp.size() * idxDst);
				auto* pS = (uint8_t*)pSrc + ((uintptr_t)comp.size() * idxSrc);
				memmove((void*)pD, (const void*)pS, comp.size());
			}

			//! Swaps two component values.
			//! @param pLeft Left component storage base pointer.
			//! @param pRight Right component storage base pointer.
			//! @param idxLeft Left value index.
			//! @param idxRight Right value index.
			//! @param sizeDst Left value stride in bytes.
			//! @param sizeSrc Right value stride in bytes.
			void
			swap(void* pLeft, void* pRight, uint32_t idxLeft, uint32_t idxRight, uint32_t sizeDst, uint32_t sizeSrc) const {
				GAIA_ASSERT(pLeft != nullptr && pRight != nullptr);
				if (func_swap != nullptr) {
					func_swap(pLeft, pRight, idxLeft, idxRight, sizeDst, sizeSrc);
					return;
				}

				if (comp.soa() != 0 || comp.size() == 0)
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
			//! @param pLeft Left component value pointer.
			//! @param pRight Right component value pointer.
			//! @return True when the values compare equal.
			bool cmp(const void* pLeft, const void* pRight) const {
				GAIA_ASSERT(pLeft != pRight);
				if (func_cmp != nullptr)
					return func_cmp(pLeft, pRight);

				if (comp.soa() != 0 || comp.size() == 0)
					return true;

				return memcmp(pLeft, pRight, comp.size()) == 0;
			}

			//! Saves a contiguous range of component values.
			//! @param serializer Destination serializer.
			//! @param pSrc Source component storage base pointer.
			//! @param from First value index to save.
			//! @param to One-past-last value index to save.
			//! @param cap Value capacity of the source storage.
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
			//! @param serializer Source serializer.
			//! @param pDst Destination component storage base pointer.
			//! @param from First value index to load.
			//! @param to One-past-last value index to load.
			//! @param cap Value capacity of the destination storage.
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

			//! Clears all runtime field metadata registered on this component.
			void clear_fields() {
				fields.clear();
			}

			//! @return True when this component has runtime field metadata.
			GAIA_NODISCARD bool has_fields() const {
				return !fields.empty();
			}

			//! Gets the element count represented by a runtime field descriptor.
			//! @param field Field descriptor to inspect.
			//! @return 1 for scalar fields, otherwise the fixed inline array count.
			GAIA_NODISCARD static uint32_t field_element_count(const RuntimeFieldDesc& field) noexcept {
				return field.count == 0 ? 1U : field.count;
			}

			//! Gets the element count represented by stored runtime field metadata.
			//! @param field Stored field metadata to inspect.
			//! @return 1 for scalar fields, otherwise the fixed inline array count.
			GAIA_NODISCARD static uint32_t field_element_count(const RuntimeField& field) noexcept {
				return field.count == 0 ? 1U : field.count;
			}

			//! Gets the byte size of a reflected primitive runtime type entity.
			//! @param type Primitive type entity.
			//! @return Primitive byte size, or 0 when @a type is not a reflected primitive type.
			GAIA_NODISCARD static uint32_t primitive_type_size(Entity type) noexcept {
				ser::serialization_type_id id = ser::serialization_type_id::ignore;
				if (!runtime_primitive_serialization_type(type, id))
					return 0;
				return ser::serialization_type_size(id, 0);
			}

			//! Adds or updates runtime field metadata.
			//! @param desc Field descriptor. A count of 0 means one scalar value.
			//! @param typeSize Optional pre-resolved field type size. When 0, primitive type metadata is used.
			//! @return True when the field metadata was added or updated; false when validation failed.
			GAIA_NODISCARD bool add_field(const RuntimeFieldDesc& desc, uint32_t typeSize = 0) {
				if (desc.name.empty() || desc.name.size() >= MaxNameLength)
					return false;
				if (desc.type == EntityBad)
					return false;

				const auto elemSize = typeSize != 0 ? typeSize : primitive_type_size(desc.type);
				if (elemSize == 0)
					return false;

				const auto elemCount = field_element_count(desc);
				const auto end = (uint64_t)desc.offset + (uint64_t)elemSize * (uint64_t)elemCount;
				if (end > comp.size())
					return false;

				for (auto& field: fields) {
					if (strncmp(field.name, desc.name.data(), desc.name.size()) == 0 && field.name[desc.name.size()] == 0) {
						field.type = desc.type;
						field.offset = desc.offset;
						field.count = desc.count;
						return true;
					}
				}

				RuntimeField field{};
				memcpy((void*)field.name, (const void*)desc.name.data(), desc.name.size());
				field.name[desc.name.size()] = 0;
				field.type = desc.type;
				field.offset = desc.offset;
				field.count = desc.count;
				fields.push_back(field);
				return true;
			}

			//! Looks up runtime field metadata by index.
			//! \param index Field index.
			//! \return Field metadata pointer when found, nullptr otherwise.
			GAIA_NODISCARD const RuntimeField* field(uint32_t index) const noexcept {
				return index < fields.size() ? &fields[index] : nullptr;
			}

			//! Looks up runtime field metadata by name.
			//! \param fieldName Field name.
			//! \return Field metadata pointer when found, nullptr otherwise.
			GAIA_NODISCARD const RuntimeField* field(util::str_view fieldName) const noexcept {
				if (fieldName.empty() || fieldName.size() >= MaxNameLength)
					return nullptr;

				for (const auto& field: fields) {
					if (strncmp(field.name, fieldName.data(), fieldName.size()) == 0 && field.name[fieldName.size()] == 0)
						return &field;
				}
				return nullptr;
			}

			//! @return Number of runtime fields registered on this component.
			GAIA_NODISCARD uint32_t field_count() const noexcept {
				return (uint32_t)fields.size();
			}

			//! Clears all runtime field metadata registered on this component.
			void clear_runtime_fields() {
				fields.clear();
			}

#if GAIA_ENABLE_HOOKS
			//! @return Mutable hook callback storage for this component.
			Hooks& hooks() {
				return comp_hooks;
			}

			//! @return Read-only hook callback storage for this component.
			const Hooks& hooks() const {
				return comp_hooks;
			}

#endif

			//! Calculates the next aligned memory offset after storing @a cnt values of this component.
			//! @param addr Starting byte offset.
			//! @param cnt Number of component values to reserve.
			//! @return Byte offset after the component storage block.
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
			//! @tparam T Component type being registered.
			//! @param nameTmp Output buffer receiving the normalized null-terminated name.
			//! @return Length of the normalized component name, excluding the null terminator.
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

			//! Initializes an owned symbol lookup key from a name view.
			//! @param nameOut Destination lookup key receiving owned storage.
			//! @param nameView Component name to copy.
			static void init_name(SymbolLookupKey& nameOut, util::str_view nameView) {
				char* name = mem::AllocHelper::alloc<char>(nameView.size() + 1);
				memcpy((void*)name, (const void*)nameView.data(), nameView.size());
				name[nameView.size()] = 0;
				nameOut = SymbolLookupKey(name, nameView.size(), 1);
			}

		public:
			//! Creates metadata for a compile-time C++ component type.
			//! @tparam T Component type to register.
			//! @param entity Component entity that owns the resulting metadata.
			//! @return Newly allocated component cache item; release with destroy().
			template <typename T>
			GAIA_NODISCARD static ComponentCacheItem* create(Entity entity) {
				static_assert(core::is_raw_v<T>);

				constexpr auto componentSize = detail::ComponentDesc<T>::size();
				static_assert(
						componentSize < Component::MaxComponentSizeInBytes,
						"Trying to register a component larger than the maximum allowed component size! In the future this "
						"restriction won't apply to components not stored inside archetype chunks.");

				char nameTmp[MaxNameLength];
				const auto nameTmpLen = init_type_name<T>(nameTmp);

				uint8_t soaSizes[meta::StructToTupleMaxTypes]{};
				const auto desc = detail::ComponentDesc<T>::make(
						util::str_view(nameTmp, nameTmpLen), std::span<uint8_t, meta::StructToTupleMaxTypes>{soaSizes});
				return create(entity, desc);
			}

			//! Creates metadata from a plain component descriptor.
			//! @param entity Component entity that owns the resulting metadata.
			//! @param desc Component descriptor describing storage, lifecycle, and runtime type metadata.
			//! @return Newly allocated component cache item; release with destroy().
			GAIA_NODISCARD static ComponentCacheItem* create(Entity entity, const ecs::ComponentDesc& desc) {
				GAIA_ASSERT(!desc.name.empty());
				GAIA_ASSERT(desc.name.size() < MaxNameLength);
				GAIA_ASSERT(desc.size < Component::MaxComponentSizeInBytes);
				GAIA_ASSERT((desc.size == 0 && desc.alig == 0) || (desc.alig > 0 && desc.alig < Component::MaxAlignment));
				GAIA_ASSERT(desc.soa <= meta::StructToTupleMaxTypes);

				auto* cci = new ComponentCacheItem();
				cci->entity = entity;
				cci->comp = Component(entity.id(), desc.soa, desc.size, desc.alig, desc.storageType);
				cci->hashLookup = desc.hashLookup.hash != 0
															? desc.hashLookup
															: ComponentLookupHash{core::calculate_hash64(desc.name.data(), desc.name.size())};

				if (desc.soa > 0 && desc.pSoaSizes != nullptr) {
					GAIA_FOR(desc.soa) cci->soaSizes[i] = desc.pSoaSizes[i];
				}

				init_name(cci->name, desc.name);

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
				cci->typeKind = desc.typeKind;
				cci->primitiveKind = desc.primitiveKind;

				return cci;
			}

			//! Creates metadata from the legacy component-cache registration context.
			//! @param entity Component entity that owns the resulting metadata.
			//! @param ctx Component registration context to adapt to ComponentDesc.
			//! @return Newly allocated component cache item; release with destroy().
			GAIA_NODISCARD static ComponentCacheItem* create(Entity entity, const ComponentCacheItemCtx& ctx) {
				ecs::ComponentDesc desc{};
				desc.name = ctx.name;
				desc.size = ctx.size;
				desc.alig = ctx.alig;
				desc.storageType = ctx.storageType;
				desc.soa = ctx.soa;
				desc.pSoaSizes = ctx.pSoaSizes;
				desc.hashLookup = ctx.hashLookup;
				desc.typeKind = ctx.typeKind;
				desc.primitiveKind = ctx.primitiveKind;
				desc.funcCtor = ctx.funcCtor;
				desc.funcMoveCtor = ctx.funcMoveCtor;
				desc.funcCopyCtor = ctx.funcCopyCtor;
				desc.funcDtor = ctx.funcDtor;
				desc.funcCopy = ctx.funcCopy;
				desc.funcMove = ctx.funcMove;
				desc.funcSwap = ctx.funcSwap;
				desc.funcCmp = ctx.funcCmp;
				desc.funcSave = ctx.funcSave;
				desc.funcLoad = ctx.funcLoad;
				return create(entity, desc);
			}

			//! Releases a cache item and any owned symbol storage.
			//! @param pItem Cache item created by create(); null is accepted.
			static void destroy(ComponentCacheItem* pItem) {
				if (pItem == nullptr)
					return;

				if (pItem->name.str() != nullptr && pItem->name.owned()) {
					mem::AllocHelper::free((void*)pItem->name.str());
					pItem->name = {};
				}

				delete pItem;
			}
		};
	} // namespace ecs
} // namespace gaia
