#pragma once
#include "gaia/config/config.h"

#include <cstdint>
#include <cstring>
#include <tuple>
#include <type_traits>

#include "gaia/core/span.h"
#include "gaia/core/utility.h"
#include "gaia/ecs/component.h"
#include "gaia/mem/data_layout_policy.h"
#include "gaia/mem/mem_utils.h"
#include "gaia/meta/reflection.h"
#include "gaia/meta/type_info.h"
#include "gaia/ser/ser_rt.h"
#include "gaia/util/str.h"

namespace gaia {
	namespace ecs {
		//! Runtime reflection kind associated with a component/type entity.
		enum class RuntimeTypeKind : uint8_t {
			//! No runtime reflection metadata is associated with the entity.
			None,
			//! Built-in primitive runtime type such as Bool, S32, F32, or Char8.
			Primitive,
			//! Struct-like runtime type described by named byte-offset fields.
			Struct,
			//! Enum runtime type described by named constants and a primitive underlying type.
			Enum,
			//! Bitmask runtime type described by named flag constants and a primitive underlying type.
			Bitmask,
			//! Fixed-array runtime type described by an element type and element count.
			Array,
			//! Dynamic vector/list runtime type described by an element type and adapter-owned storage.
			Vector,
			//! Opaque runtime type whose physical payload layout is exposed through a semantic runtime type.
			Opaque,
		};

		//! User-authored runtime field descriptor.
		//! A count of 0 means scalar; positive values describe a fixed inline array.
		struct RuntimeFieldDesc final {
			//! Field symbol.
			util::str_view name{};
			//! Entity describing the field type.
			Entity type = EntityBad;
			//! Byte offset from the start of the component payload.
			uint32_t offset = 0;
			//! Inline array element count. 0 means scalar.
			uint32_t count = 0;
		};

		//! Stored runtime field metadata.
		struct RuntimeField final {
			//! Null-terminated copied field symbol.
			char name[256]{};
			//! Entity describing the field type.
			Entity type = EntityBad;
			//! Byte offset from the start of the component payload.
			uint32_t offset = 0;
			//! Inline array element count. 0 means scalar.
			uint32_t count = 0;
		};

		//! User-authored symbolic runtime constant descriptor for enum and bitmask type entities.
		struct RuntimeConstantDesc final {
			//! Constant symbol.
			util::str_view name{};
			//! Constant value.
			int64_t value = 0;
		};

		//! Stored symbolic runtime constant metadata.
		struct RuntimeConstant final {
			//! Null-terminated copied constant symbol.
			char name[256]{};
			//! Constant value stored in the enum/bitmask underlying integer domain.
			int64_t value = 0;
		};

		//! Runtime sequence adapter input scope. The byte layout is owned by the adapter.
		struct RuntimeSequenceScope final {
			//! Runtime type entity for the selected sequence value.
			Entity type = EntityBad;
			//! Read-only sequence payload or handle bytes.
			const void* data = nullptr;
			//! Mutable sequence payload or handle bytes. Null for read-only traversal.
			void* mutData = nullptr;
			//! Size of the selected sequence payload or handle bytes.
			uint32_t size = 0;
		};

		//! Runtime sequence element returned by an adapter.
		struct RuntimeSequenceElement final {
			//! Runtime type entity for the selected element value.
			Entity type = EntityBad;
			//! Read-only element bytes.
			const void* data = nullptr;
			//! Mutable element bytes. Null for read-only traversal.
			void* mutData = nullptr;
			//! Size of the selected element bytes.
			uint32_t size = 0;
			//! Adapter-owned token for projected elements.
			void* token = nullptr;
		};

		//! Callback table for adapter-owned dynamic sequence values.
		struct RuntimeSequenceAdapter final {
			//! User context passed to all callbacks.
			void* ctx = nullptr;
			//! Reads the dynamic element count. Required for traversal.
			bool (*count)(void*, const RuntimeSequenceScope&, uint32_t&) = nullptr;
			//! Selects one element by index. Required for traversal.
			bool (*element)(void*, const RuntimeSequenceScope&, uint32_t, RuntimeSequenceElement&) = nullptr;
			//! Resizes the sequence. Optional; required for JSON load into dynamic sequences.
			bool (*resize)(void*, RuntimeSequenceScope&, uint32_t) = nullptr;
			//! Commits a projected element after cursor writes. Optional for direct element pointers.
			bool (*commitElement)(void*, RuntimeSequenceScope&, RuntimeSequenceElement&) = nullptr;
		};

		//! Runtime opaque adapter input scope. The physical byte layout is owned by the adapter.
		struct RuntimeOpaqueScope final {
			//! Opaque runtime type entity for the selected value.
			Entity type = EntityBad;
			//! Read-only opaque payload or handle bytes.
			const void* data = nullptr;
			//! Mutable opaque payload or handle bytes. Null for read-only traversal.
			void* mutData = nullptr;
			//! Size of the opaque payload or handle bytes.
			uint32_t size = 0;
		};

		//! Semantic value projected from opaque runtime storage.
		struct RuntimeOpaqueValue final {
			//! Semantic runtime type entity for projected bytes. EntityBad uses opaqueAsType.
			Entity type = EntityBad;
			//! Read-only projected semantic bytes.
			const void* data = nullptr;
			//! Mutable projected semantic bytes. Null for read-only traversal.
			void* mutData = nullptr;
			//! Size of projected semantic bytes.
			uint32_t size = 0;
			//! Adapter-owned projection token.
			void* token = nullptr;
		};

		//! Callback table for opaque runtime values with adapter-owned physical storage.
		struct RuntimeOpaqueAdapter final {
			//! User context passed to all callbacks.
			void* ctx = nullptr;
			//! Projects an opaque value to its semantic runtime type. Required for traversal.
			bool (*project)(void*, const RuntimeOpaqueScope&, RuntimeOpaqueValue&) = nullptr;
			//! Commits a projected semantic value after cursor writes. Optional for direct projections.
			bool (*commit)(void*, RuntimeOpaqueScope&, RuntimeOpaqueValue&) = nullptr;
		};

		//! Plain component registration descriptor shared by typed and runtime component paths.
		//! Typed registration produces this descriptor from detail::ComponentDesc, while runtime
		//! registration can fill it from data loaded at runtime.
		struct ComponentDesc final {
			//! Component constructor callback for a contiguous range of payload slots.
			using FuncCtor = void(void*, uint32_t);
			//! Component destructor callback for a contiguous range of payload slots.
			using FuncDtor = void(void*, uint32_t);
			//! Component migration callback used when copying or moving from one layout stride to another.
			using FuncFrom = void(void*, void*, uint32_t, uint32_t, uint32_t, uint32_t);
			//! Component copy callback used by typed registration and storage moves.
			using FuncCopy = void(void*, const void*, uint32_t, uint32_t, uint32_t, uint32_t);
			//! Component move callback used by typed registration and storage moves.
			using FuncMove = void(void*, void*, uint32_t, uint32_t, uint32_t, uint32_t);
			//! Component swap callback used by storage operations that exchange payload ranges.
			using FuncSwap = void(void*, void*, uint32_t, uint32_t, uint32_t, uint32_t);
			//! Component equality callback for comparing two payload values.
			using FuncCmp = bool(const void*, const void*);
			//! Component serializer callback for typed component save operations.
			using FuncSave = void(ser::serializer&, const void*, uint32_t, uint32_t, uint32_t);
			//! Component serializer callback for typed component load operations.
			using FuncLoad = void(ser::serializer&, void*, uint32_t, uint32_t, uint32_t);

			//! Registered component symbol.
			util::str_view name;
			//! Component payload size in bytes.
			uint32_t size = 0;
			//! Component payload alignment in bytes.
			uint32_t alig = 0;
			//! Component storage mode.
			DataStorageType storageType = DataStorageType::Table;
			//! Number of SoA elements, 0 means AoS.
			uint32_t soa = 0;
			//! Per-element SoA sizes when @a soa is non-zero. The array must contain @a soa non-zero entries and
			//! their sum must not exceed @a size.
			const uint8_t* pSoaSizes = nullptr;
			//! Optional explicit lookup hash. When empty, the symbol hash is used.
			ComponentLookupHash hashLookup{};
			//! Runtime reflection type kind.
			RuntimeTypeKind typeKind = RuntimeTypeKind::Struct;
			//! Primitive storage type for enum/bitmask metadata. EntityBad otherwise.
			Entity underlyingType = EntityBad;
			//! Runtime field descriptors copied into component metadata during registration.
			const RuntimeFieldDesc* fields = nullptr;
			//! Number of field descriptors.
			uint32_t fieldCount = 0;
			//! Runtime constant descriptors copied into enum/bitmask metadata during registration.
			const RuntimeConstantDesc* constants = nullptr;
			//! Number of constant descriptors.
			uint32_t constantCount = 0;
			//! Optional constructor callback. Runtime byte-only components leave this null.
			FuncCtor* funcCtor = nullptr;
			//! Optional move-constructor callback. Runtime byte-only components leave this null.
			FuncMove* funcMoveCtor = nullptr;
			//! Optional copy-constructor callback. Runtime byte-only components leave this null.
			FuncCopy* funcCopyCtor = nullptr;
			//! Optional destructor callback. Runtime byte-only components leave this null.
			FuncDtor* funcDtor = nullptr;
			//! Optional copy-assignment callback. Runtime byte-only components leave this null.
			FuncCopy* funcCopy = nullptr;
			//! Optional move-assignment callback. Runtime byte-only components leave this null.
			FuncMove* funcMove = nullptr;
			//! Optional swap callback. Runtime byte-only components leave this null.
			FuncSwap* funcSwap = nullptr;
			//! Optional equality callback. Runtime byte-only components leave this null.
			FuncCmp* funcCmp = nullptr;
			//! Optional typed serialization save callback. Semantic runtime JSON uses field metadata instead.
			FuncSave* funcSave = nullptr;
			//! Optional typed serialization load callback. Semantic runtime JSON uses field metadata instead.
			FuncLoad* funcLoad = nullptr;
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
		};

		namespace detail {
			//! Compile-time adapter that converts typed component metadata into a plain ComponentDesc.
			//! \tparam T Component type or component wrapper being registered.
			template <typename T>
			struct ComponentDesc final {
				//! Reflected Gaia component type wrapper.
				using CT = component_type_t<T>;
				//! Stored C++ payload type.
				using U = typename component_type_t<T>::Type;
				//! Fully-qualified component type used for stable symbol/hash generation.
				using DescU = typename CT::TypeFull;

				//! Returns the stable lookup hash for the component type.
				//! \return Component lookup hash derived from reflected type metadata.
				static constexpr ComponentLookupHash hash_lookup() {
					return {meta::type_info::hash<DescU>()};
				}

				//! Returns the reflected component symbol name.
				//! \return Reflected type name for descriptor registration.
				static constexpr auto name() {
					return meta::type_info::name<DescU>();
				}

				//! Returns the payload size stored in chunks.
				//! \return 0 for empty marker components, otherwise sizeof(payload type).
				static constexpr uint32_t size() {
					if constexpr (std::is_empty_v<U>)
						return 0;
					else
						return (uint32_t)sizeof(U);
				}

				//! Returns the payload alignment required by the component storage policy.
				//! \return Component payload alignment in bytes.
				static constexpr uint32_t alig() {
					constexpr auto alig = mem::auto_view_policy<U>::Alignment;
					static_assert(alig < Component::MaxAlignment, "Maximum supported alignment for a component is MaxAlignment");
					return alig;
				}

				//! Populates SoA element sizes for SoA payloads.
				//! \param soaSizes Scratch/output storage for per-element byte sizes.
				//! \return Number of SoA elements, or 0 for AoS payloads.
				static uint32_t soa(std::span<uint8_t, meta::StructToTupleMaxTypes> soaSizes) {
					if constexpr (mem::is_soa_layout_v<U>) {
						uint32_t i = 0;
						using TTuple = decltype(meta::struct_to_tuple(std::declval<U>()));
						// is_soa_layout_v is always false for empty types so we know there is at least one element in the tuple
						constexpr auto TTupleSize = std::tuple_size_v<TTuple>;
						static_assert(TTupleSize > 0);
						static_assert(TTupleSize <= meta::StructToTupleMaxTypes);
						core::each_tuple<TTuple>([&](auto&& item) {
							static_assert(sizeof(item) <= 255, "Each member of a SoA component can be at most 255 B long!");
							soaSizes[i] = (uint8_t)sizeof(item);
							++i;
						});
						GAIA_ASSERT(i <= meta::StructToTupleMaxTypes);
						return i;
					} else {
						return 0U;
					}
				}

				//! Builds the optional constructor callback for typed AoS payloads.
				//! \return Constructor callback, or nullptr when construction is trivial or SoA-managed.
				static constexpr auto func_ctor() {
					if constexpr (!mem::is_soa_layout_v<U> && !std::is_trivially_constructible_v<U>) {
						return [](void* ptr, uint32_t cnt) {
							core::call_ctor_n((U*)ptr, cnt);
						};
					} else {
						return nullptr;
					}
				}

				//! Builds the optional destructor callback for typed AoS payloads.
				//! \return Destructor callback, or nullptr when destruction is trivial or SoA-managed.
				static constexpr auto func_dtor() {
					if constexpr (!mem::is_soa_layout_v<U> && !std::is_trivially_destructible_v<U>) {
						return [](void* ptr, uint32_t cnt) {
							core::call_dtor_n((U*)ptr, cnt);
						};
					} else {
						return nullptr;
					}
				}

				//! Builds the copy-constructor callback used when typed payload storage grows or migrates.
				//! \return Copy-constructor callback for the component payload type.
				static constexpr auto func_copy_ctor() {
					return [](void* GAIA_RESTRICT dst, const void* GAIA_RESTRICT src, uint32_t idxDst, uint32_t idxSrc,
										uint32_t sizeDst, uint32_t sizeSrc) {
						mem::copy_ctor_element<U>((uint8_t*)dst, (const uint8_t*)src, idxDst, idxSrc, sizeDst, sizeSrc);
					};
				}

				//! Builds the move-constructor callback used when typed payload storage grows or migrates.
				//! \return Move-constructor callback for the component payload type.
				static constexpr auto func_move_ctor() {
					return [](void* GAIA_RESTRICT dst, void* GAIA_RESTRICT src, uint32_t idxDst, uint32_t idxSrc,
										uint32_t sizeDst, uint32_t sizeSrc) {
						mem::move_ctor_element<U>((uint8_t*)dst, (uint8_t*)src, idxDst, idxSrc, sizeDst, sizeSrc);
					};
				}

				//! Builds the copy-assignment callback used for typed payload writes.
				//! \return Copy-assignment callback for the component payload type.
				static constexpr auto func_copy() {
					return [](void* GAIA_RESTRICT dst, const void* GAIA_RESTRICT src, uint32_t idxDst, uint32_t idxSrc,
										uint32_t sizeDst, uint32_t sizeSrc) {
						mem::copy_element<U>((uint8_t*)dst, (const uint8_t*)src, idxDst, idxSrc, sizeDst, sizeSrc);
					};
				}

				//! Builds the move-assignment callback used for typed payload writes.
				//! \return Move-assignment callback for the component payload type.
				static constexpr auto func_move() {
					return [](void* GAIA_RESTRICT dst, void* GAIA_RESTRICT src, uint32_t idxDst, uint32_t idxSrc,
										uint32_t sizeDst, uint32_t sizeSrc) {
						mem::move_element<U>((uint8_t*)dst, (uint8_t*)src, idxDst, idxSrc, sizeDst, sizeSrc);
					};
				}

				//! Builds the swap callback used by storage operations.
				//! \return Swap callback for the component payload type.
				static constexpr auto func_swap() {
					return [](void* GAIA_RESTRICT left, void* GAIA_RESTRICT right, uint32_t idxLeft, uint32_t idxRight,
										uint32_t sizeLeft, uint32_t sizeRight) {
						mem::swap_elements<U>((uint8_t*)left, (uint8_t*)right, idxLeft, idxRight, sizeLeft, sizeRight);
					};
				}

				//! Builds the equality comparison callback for typed payload values.
				//! \return Equality callback using operator== when available, otherwise byte comparison.
				static constexpr auto func_cmp() {
					if constexpr (mem::is_soa_layout_v<U>) {
						return []([[maybe_unused]] const void* left, [[maybe_unused]] const void* right) {
							GAIA_ASSERT(false && "func_cmp for SoA not implemented yet");
							return false;
						};
					} else {
						constexpr bool hasGlobalCmp = core::has_ffunc_equals<U>::value;
						constexpr bool hasMemberCmp = core::has_func_equals<U>::value;
						if constexpr (hasGlobalCmp || hasMemberCmp) {
							return [](const void* left, const void* right) {
								const auto* l = (const U*)left;
								const auto* r = (const U*)right;
								return *l == *r;
							};
						} else {
							// fallback comparison function
							return [](const void* left, const void* right) {
								const auto* l = (const U*)left;
								const auto* r = (const U*)right;
								return memcmp(l, r, sizeof(U)) == 0;
							};
						}
					}
				}

				//! Builds the serializer save callback for typed payload values.
				//! \return Save callback for the component payload type.
				static constexpr auto func_save() {
					return [](ser::serializer& s, const void* pSrc, uint32_t from, uint32_t to, uint32_t cap) {
						const auto* pComponent = (const U*)pSrc;

#if GAIA_ASSERT_ENABLED
						// Check if save and load match. Testing with one item is enough.
						s.check(*pComponent);
#endif

						if constexpr (mem::is_soa_layout_v<U>) {
							auto view = mem::auto_view_policy_get<U>{std::span{(const uint8_t*)pSrc, cap}};
							GAIA_FOR2(from, to) {
								auto val = view[i];
								s.save(val);
							}
						} else {
							pComponent += from;
							GAIA_FOR2(from, to) {
								s.save(*pComponent);
								++pComponent;
							}
						}
					};
				}

				//! Builds the serializer load callback for typed payload values.
				//! \return Load callback for the component payload type.
				static constexpr auto func_load() {
					return [](ser::serializer& s, void* pDst, uint32_t from, uint32_t to, uint32_t cap) {
						if constexpr (mem::is_soa_layout_v<U>) {
							auto view = mem::auto_view_policy_set<U>{std::span{(uint8_t*)pDst, cap}};
							GAIA_FOR2(from, to) {
								U val;
								s.load(val);
								view[i] = val;
							}
						} else {
							auto* pComponent = (U*)pDst + from;
							GAIA_FOR2(from, to) {
								s.load(*pComponent);
								++pComponent;
							}
						}
					};
				}

#if GAIA_ECS_AUTO_COMPONENT_FIELDS

				//! Gets reflected primitive entity for a supported C++ field type.
				//! \tparam Field C++ field type to map to a runtime primitive entity.
				//! \return Primitive type entity, or EntityBad when unsupported.
				template <typename Field>
				static Entity auto_field_type() noexcept {
					using FU = core::raw_t<Field>;
					if constexpr (std::is_same_v<FU, bool>)
						return Bool;
					else if constexpr (std::is_same_v<FU, char>)
						return Char8;
					else if constexpr (std::is_arithmetic_v<FU>)
						return runtime_primitive_type_entity(ser::type_id<FU>());
					else
						return EntityBad;
				}

				//! Populates compile-time reflected fields for descriptor-time metadata registration.
				//! \param fields Scratch/output field descriptor storage.
				//! \return Number of valid field descriptors written.
				static uint32_t auto_fields(std::span<RuntimeFieldDesc, meta::StructToTupleMaxTypes> fields) {
					using Raw = core::raw_t<T>;
					if constexpr (std::is_empty_v<Raw> || mem::is_soa_layout_v<Raw>) {
						return 0;
					} else if constexpr (!std::is_class_v<Raw>) {
						const auto fieldType = auto_field_type<Raw>();
						if (fieldType == EntityBad)
							return 0;
						fields[0] = {util::str_view("value", 5), fieldType, 0, 0};
						return 1;
					} else if constexpr (!std::is_aggregate_v<Raw>) {
						return 0;
					} else {
						Raw tmp{};
						const auto* pBase = reinterpret_cast<const uint8_t*>(&tmp);
						static constexpr const char* FieldNames[meta::StructToTupleMaxTypes] = {
								"f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7", "f8", "f9", "f10", "f11", "f12", "f13", "f14"};
						uint32_t fieldCount = 0;
						meta::each_member(tmp, [&](auto&... members) {
							auto add_field = [&](auto& member) {
								using Field = core::raw_t<decltype(member)>;
								const auto fieldType = auto_field_type<Field>();
								if (fieldType == EntityBad)
									return;
								GAIA_ASSERT(fieldCount < meta::StructToTupleMaxTypes);
								const auto* pField = reinterpret_cast<const uint8_t*>(&member);
								const auto offset = (uint32_t)(pField - pBase);
								const auto* fieldName = FieldNames[fieldCount];
								const auto fieldNameLen = (uint32_t)GAIA_STRLEN(fieldName, 4);
								fields[fieldCount] = {util::str_view(fieldName, fieldNameLen), fieldType, offset, 0};
								++fieldCount;
							};
							(add_field(members), ...);
						});
						return fieldCount;
					}
				}

#endif

				//! Builds the plain descriptor used by component cache registration.
				//! \param descName Normalized component symbol name.
				//! \param soaSizes Scratch/output storage for SoA element sizes. The returned descriptor keeps a
				//! non-owning pointer to this buffer and must be consumed before the buffer expires.
				//! \return Component descriptor for the current component type.
				static ecs::ComponentDesc
				make(util::str_view descName, std::span<uint8_t, meta::StructToTupleMaxTypes> soaSizes) {
					ecs::ComponentDesc desc{};
					desc.name = descName;
					desc.size = size();
					desc.alig = alig();
					desc.storageType = DataStorageType::Table;
					desc.soa = soa(soaSizes);
					desc.pSoaSizes = soaSizes.data();
					desc.hashLookup = hash_lookup();
					desc.funcCtor = func_ctor();
					desc.funcMoveCtor = func_move_ctor();
					desc.funcCopyCtor = func_copy_ctor();
					desc.funcDtor = func_dtor();
					desc.funcCopy = func_copy();
					desc.funcMove = func_move();
					desc.funcSwap = func_swap();
					desc.funcCmp = func_cmp();
					desc.funcSave = func_save();
					desc.funcLoad = func_load();
					return desc;
				}
			};
		} // namespace detail
	} // namespace ecs
} // namespace gaia
