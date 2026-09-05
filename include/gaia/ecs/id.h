#pragma once
#include "gaia/config/config.h"

#include <cstdint>
#include <type_traits>

#include "gaia/cnt/sparse_storage.h"
#include "gaia/core/hashing_policy.h"
#include "gaia/core/utility.h"
#include "gaia/ecs/id_fwd.h"
#include "gaia/ser/ser_common.h"

namespace gaia {
	namespace ecs {
#define GAIA_ID(type) GAIA_ID_##type

		using Identifier = uint64_t;
		inline constexpr Identifier IdentifierBad = (Identifier)-1;
		inline constexpr Identifier EntityCompMask = IdentifierBad << 1;
		inline constexpr IdentifierId IdentifierIdBad = (IdentifierId)-1;

		enum class DataStorageType : uint32_t {
			//! Data stored in a table
			Table,
			//! Data stored in sparse storage
			Sparse,
			//! Non-fragmenting membership. Empty tags stay table-sized with no payload.
			//! AoS payloads use sparse storage and keep the id outside archetype identity.
			DontFragment,

			//! Number of packed payload storage modes. `DontFragment` maps to `Table` or `Sparse`.
			Count = 2
		};

//! Declares the storage mode used when registering a typed C++ component.
//! \param storage_name `DataStorageType` enumerator name such as `Table`, `Sparse`, or `DontFragment`.
#define GAIA_STORAGE(storage_name) static constexpr auto gaia_Data_Storage = ::gaia::ecs::DataStorageType::storage_name

		// ------------------------------------------------------------------------------------
		// Component
		// ------------------------------------------------------------------------------------

		//! Identifier of a registered component type.
		//! Packs the component id, size, alignment, storage mode and SoA layout into one 64-bit value.
		struct GAIA_API Component final {
			//! Bit mask covering all valid component ids.
			static constexpr uint32_t IdMask = IdentifierIdBad;
			//! Number of bits used to store the component size.
			static constexpr uint32_t MaxComponentSize_Bits = 13;
			//! Largest component size storable, in bytes.
			static constexpr uint32_t MaxComponentSizeInBytes = (1 << MaxComponentSize_Bits) - 1;
			//! Number of bits used to store the component alignment.
			static constexpr uint32_t MaxAlignment_Bits = MaxComponentSize_Bits;
			//! Largest component alignment storable.
			static constexpr uint32_t MaxAlignment = MaxComponentSizeInBytes;

			//! Bit-packed storage layout of a component id.
			struct InternalData {
				//! Component entity index
				uint32_t id;
				//! Component size
				IdentifierData size : MaxComponentSize_Bits;
				//! Component alignment
				IdentifierData alig : MaxAlignment_Bits;
				//! Component storage kind. 0 = table, 1 = sparse.
				IdentifierData storage : 1;
				//! Component is SoA
				IdentifierData soa : meta::StructToTupleMaxTypes_Bits;
				//! Unused part
				IdentifierData unused : 1;
			};
			static_assert(sizeof(InternalData) == sizeof(Identifier));

			union {
				//! Structured view of the packed value.
				InternalData data;
				//! Raw 64-bit value.
				Identifier val;
			};

			Component() noexcept = default;

			Component(uint32_t id, uint32_t soa, uint32_t size, uint32_t alig, DataStorageType storage) noexcept {
				data.id = id;
				data.soa = soa;
				data.size = size;
				data.alig = alig;
				data.storage = (IdentifierData)storage;
				data.unused = 0;
			}

			//! Component id.
			//! \return Component id.
			GAIA_NODISCARD constexpr auto id() const noexcept {
				return (uint32_t)data.id;
			}

			//! Whether the component uses SoA storage.
			//! \return Non-zero when SoA storage is used.
			GAIA_NODISCARD constexpr auto soa() const noexcept {
				return (uint32_t)data.soa;
			}

			//! Component size in bytes.
			//! \return Component size in bytes.
			GAIA_NODISCARD constexpr auto size() const noexcept {
				return (uint32_t)data.size;
			}

			//! Component alignment in bytes.
			//! \return Component alignment in bytes.
			GAIA_NODISCARD constexpr auto alig() const noexcept {
				return (uint32_t)data.alig;
			}

			//! Storage mode of the component.
			//! \return Storage mode of the component.
			GAIA_NODISCARD constexpr DataStorageType storage_type() const noexcept {
				return (DataStorageType)data.storage;
			}

			//! Raw identifier value.
			//! \return Raw 64-bit value.
			GAIA_NODISCARD constexpr auto value() const noexcept {
				return val;
			}

			GAIA_NODISCARD constexpr bool operator==(Component other) const noexcept {
				return value() == other.value();
			}

			GAIA_NODISCARD constexpr bool operator!=(Component other) const noexcept {
				return value() != other.value();
			}

			GAIA_NODISCARD constexpr bool operator<(Component other) const noexcept {
				return id() < other.id();
			}

			//! Serializes the component id.
			//! \tparam Serializer serializer type.
			//! \param s serializer to write to.
			template <typename Serializer>
			void save(Serializer& s) const;

			//! Deserializes the component id.
			//! \tparam Serializer serializer type.
			//! \param s serializer to read from.
			template <typename Serializer>
			void load(Serializer& s);
		};

		//----------------------------------------------------------------------
		// Id type deduction
		//----------------------------------------------------------------------

		//! \cond INTERNAL
		namespace detail {
			template <typename T>
			struct ExtractComponentType {
				//! Raw type with no additional sugar
				using Type = core::raw_t<T>;
				//! Same as Type
				using TypeFull = Type;
				//! Original template type
				using TypeOriginal = T;
			};

			template <typename T>
			struct component_type {
				using type = ExtractComponentType<T>;
			};
		} // namespace detail
		//! \endcond

		template <typename T>
		using component_type_t = typename detail::component_type<T>::type;

		//----------------------------------------------------------------------
		// Pair helpers
		//----------------------------------------------------------------------

		//! \cond INTERNAL
		namespace detail {
			struct pair_base {};
		} // namespace detail
		//! \endcond

		//! Wrapper for two types forming a relationship pair.
		//! Depending on what types are used to form a pair it can contain a value.
		//! To determine the storage type the following logic is applied:
		//! If \a Rel is non-empty, the storage type is Rel.
		//! If \a Rel is empty and \a Tgt is non-empty, the storage type is Tgt.
		//! \tparam Rel relation part of the relationship
		//! \tparam Tgt target part of the relationship
		template <typename Rel, typename Tgt>
		class pair: public detail::pair_base {
			using rel_comp_type = component_type_t<Rel>;
			using tgt_comp_type = component_type_t<Tgt>;

		public:
			//! Full relation type.
			using rel = typename rel_comp_type::TypeFull;
			//! Full target type.
			using tgt = typename tgt_comp_type::TypeFull;
			//! Raw relation type.
			using rel_type = typename rel_comp_type::Type;
			//! Raw target type.
			using tgt_type = typename tgt_comp_type::Type;
			//! Original relation template type.
			using rel_original = typename rel_comp_type::TypeOriginal;
			//! Original target template type.
			using tgt_original = typename tgt_comp_type::TypeOriginal;
			//! Storage type for the pair value.
			using type = std::conditional_t<!std::is_empty_v<rel_type> || std::is_empty_v<tgt_type>, rel, tgt>;
		};

		//! Detects whether a type is a relationship pair.
		template <typename T>
		struct is_pair {
			//! True when the type derives from the pair base.
			static constexpr bool value = std::is_base_of<detail::pair_base, core::raw_t<T>>::value;
		};

		// ------------------------------------------------------------------------------------
		// Entity
		// ------------------------------------------------------------------------------------

		//! Identifier of an entity or component instance in the world.
		//! Packs the entity index, generation, reserved bit and flags into one 64-bit value.
		struct GAIA_API Entity final {
			//! Bit mask covering all valid entity indices.
			static constexpr uint32_t IdMask = IdentifierIdBad;

			//! Bit-packed storage layout of an entity identifier.
			struct InternalData {
				//! Index in the entity array
				EntityId id;

				///////////////////////////////////////////////////////////////////
				// Bits in this section need to be 1:1 with EntityContainer data.
				// Note, the order of these bits is important because entities
				// are sorted by their "val" member and many behaviors rely on this.
				///////////////////////////////////////////////////////////////////

				//! Generation index. Incremented every time an entity is deleted
				IdentifierData gen : 28;
				//! 0-component, 1-entity
				IdentifierData ent : 1;
				//! 0-ordinary, 1-pair
				IdentifierData pair : 1;
				//! Reserved for future use. Always 0.
				IdentifierData reserved : 1;
				//! 0-real entity, 1-temporary entity
				IdentifierData tmp : 1;

				///////////////////////////////////////////////////////////////////
			};
			static_assert(sizeof(InternalData) == sizeof(Identifier));

			union {
				//! Structured view of the packed value.
				InternalData data;
				//! Raw 64-bit value.
				Identifier val;
			};

			constexpr Entity() noexcept: val(IdentifierBad) {};

			//! We need the entity to be braces-constructible and at the same type prevent it from
			//! getting constructed accidentally from an int (e.g .Entity::id()). Therefore, only
			//! allow Entity(Identifier) to be used.
			template <typename T, typename = std::enable_if_t<std::is_same_v<T, Identifier>>>
			constexpr Entity(T value) noexcept: val(value) {}

			//! Special constructor for cnt::ilist
			Entity(EntityId id, IdentifierData gen) noexcept {
				val = 0;
				data.id = id;
				data.gen = gen;
			}

			Entity(EntityId id, IdentifierData gen, bool isEntity, bool isPair) noexcept {
				val = 0;
				data.id = id;
				data.gen = gen;
				data.ent = isEntity;
				data.pair = isPair;
				data.reserved = 0;
				data.tmp = 0;
			}

			//! Entity index in the entity array.
			//! \return Entity index.
			GAIA_NODISCARD constexpr auto id() const noexcept {
				return (uint32_t)data.id;
			}

			//! Generation index of the entity.
			//! \return Generation index.
			GAIA_NODISCARD constexpr auto gen() const noexcept {
				return (uint32_t)data.gen;
			}

			//! Whether this id refers to an entity.
			//! \return True for an entity, false for a component.
			GAIA_NODISCARD constexpr bool entity() const noexcept {
				return data.ent != 0;
			}

			//! Whether this id refers to a relationship pair.
			//! \return True when a pair.
			GAIA_NODISCARD constexpr bool pair() const noexcept {
				return data.pair != 0;
			}

			//! Whether this id refers to a component.
			//! \return True when a component.
			GAIA_NODISCARD constexpr bool comp() const noexcept {
				return (data.pair | data.ent) == 0;
			}

			//! Raw identifier value.
			//! \return Raw 64-bit value.
			GAIA_NODISCARD constexpr auto value() const noexcept {
				return val;
			}

			GAIA_NODISCARD constexpr bool operator==(Entity other) const noexcept {
				return value() == other.value();
			}

			GAIA_NODISCARD constexpr bool operator!=(Entity other) const noexcept {
				return value() != other.value();
			}

			GAIA_NODISCARD constexpr bool operator<(Entity other) const noexcept {
				return value() < other.value();
			}
			GAIA_NODISCARD constexpr bool operator<=(Entity other) const noexcept {
				return value() <= other.value();
			}

			GAIA_NODISCARD constexpr bool operator>(Entity other) const noexcept {
				return value() > other.value();
			}
			GAIA_NODISCARD constexpr bool operator>=(Entity other) const noexcept {
				return value() >= other.value();
			}

			//! Serializes the entity id.
			//! \tparam Serializer serializer type.
			//! \param s serializer to write to.
			template <typename Serializer>
			void save(Serializer& s) const;

			//! Deserializes the entity id, remapping it to the current world.
			//! \tparam Serializer serializer type.
			//! \param s serializer to read from.
			template <typename Serializer>
			void load(Serializer& s);
		};

		inline static const Entity EntityBad = Entity(IdentifierBad);

		//! \cond INTERNAL
		namespace detail {
			struct EntityLoadRemapState {
				uint32_t savedLastCoreComponentId = 0;
				uint32_t currLastCoreComponentId = 0;
				bool remapComponentIds = false;
				bool active = false;
			};

			// NOTE: Entity::load() only receives a serializer, not World state.
			//       Therefore the load-time core-id remap currently uses scoped ambient context.
			//       thread_local keeps concurrent world loads on different threads independent.
			//       Tradeoff: this is less explicit than carrying the remap state on the serializer.
			inline thread_local EntityLoadRemapState g_entityLoadRemapState{};

			struct EntityLoadRemapGuard {
				EntityLoadRemapState prev;

				EntityLoadRemapGuard(
						uint32_t savedLastCoreComponentId, uint32_t currLastCoreComponentId, bool remapComponentIds) noexcept:
						prev(g_entityLoadRemapState) {
					g_entityLoadRemapState.savedLastCoreComponentId = savedLastCoreComponentId;
					g_entityLoadRemapState.currLastCoreComponentId = currLastCoreComponentId;
					g_entityLoadRemapState.remapComponentIds = remapComponentIds;
					g_entityLoadRemapState.active = true;
				}

				~EntityLoadRemapGuard() {
					g_entityLoadRemapState = prev;
				}

				EntityLoadRemapGuard(const EntityLoadRemapGuard&) = delete;
				EntityLoadRemapGuard& operator=(const EntityLoadRemapGuard&) = delete;
				EntityLoadRemapGuard(EntityLoadRemapGuard&&) = delete;
				EntityLoadRemapGuard& operator=(EntityLoadRemapGuard&&) = delete;
			};

			GAIA_NODISCARD inline uint32_t remap_loaded_entity_id(
					uint32_t id, uint32_t savedLastCoreComponentId, uint32_t currLastCoreComponentId) noexcept {
				if (id == Entity::IdMask || //
						id <= savedLastCoreComponentId || //
						currLastCoreComponentId <= savedLastCoreComponentId //
				)
					return id;

				return id + (currLastCoreComponentId - savedLastCoreComponentId);
			}

			GAIA_NODISCARD inline uint32_t remap_loaded_entity_id(uint32_t id) noexcept {
				const auto& state = g_entityLoadRemapState;
				if (!state.active)
					return id;

				return remap_loaded_entity_id(id, state.savedLastCoreComponentId, state.currLastCoreComponentId);
			}

			GAIA_NODISCARD inline Entity
			remap_loaded_entity(Entity entity, uint32_t savedLastCoreComponentId, uint32_t currLastCoreComponentId) noexcept {
				if (entity == EntityBad)
					return entity;

				if (!entity.pair()) {
					return Entity(
							(EntityId)remap_loaded_entity_id(entity.id(), savedLastCoreComponentId, currLastCoreComponentId),
							entity.gen(), entity.entity(), false);
				}

				return Entity(
						(EntityId)remap_loaded_entity_id(entity.id(), savedLastCoreComponentId, currLastCoreComponentId),
						(IdentifierData)remap_loaded_entity_id(entity.gen(), savedLastCoreComponentId, currLastCoreComponentId),
						false, true);
			}

			GAIA_NODISCARD inline Entity remap_loaded_entity(Entity entity) noexcept {
				const auto& state = g_entityLoadRemapState;
				if (!state.active)
					return entity;

				return remap_loaded_entity(entity, state.savedLastCoreComponentId, state.currLastCoreComponentId);
			}
		} // namespace detail
		//! \endcond

		template <typename Serializer>
		inline void Entity::save(Serializer& s) const {
			s.save(val);
		}

		template <typename Serializer>
		inline void Entity::load(Serializer& s) {
			s.load(val);
			data.reserved = 0;
			*this = detail::remap_loaded_entity(*this);
		}

		template <typename Serializer>
		inline void Component::save(Serializer& s) const {
			s.save(val);
		}

		template <typename Serializer>
		inline void Component::load(Serializer& s) {
			s.load(val);
			if (detail::g_entityLoadRemapState.active && detail::g_entityLoadRemapState.remapComponentIds)
				data.id = detail::remap_loaded_entity_id(data.id);
		}

		//! Hashmap lookup structure used for Entity
		struct GAIA_API EntityLookupKey {
			//! Direct hash type used for entity lookups.
			using LookupHash = core::direct_hash_key<uint64_t>;

		private:
			//! Entity
			Entity m_entity;
			//! Entity hash
			LookupHash m_hash;

			static LookupHash calc(Entity entity) {
				return {core::calculate_hash64(entity.value())};
			}

		public:
			//! Marks this key as a direct hash key.
			static constexpr bool IsDirectHashKey = true;

			EntityLookupKey() = default;
			explicit EntityLookupKey(Entity entity): m_entity(entity), m_hash(calc(entity)) {}
			~EntityLookupKey() = default;

			EntityLookupKey(const EntityLookupKey&) = default;
			EntityLookupKey(EntityLookupKey&&) noexcept = default;
			EntityLookupKey& operator=(const EntityLookupKey&) = default;
			EntityLookupKey& operator=(EntityLookupKey&&) noexcept = default;

			//! Entity held by this lookup key.
			//! \return Entity held by this lookup key.
			Entity entity() const {
				return m_entity;
			}

			//! Hash of the entity value.
			//! \return Hash of the entity value.
			size_t hash() const {
				return (size_t)m_hash.hash;
			}

			bool operator==(const EntityLookupKey& other) const {
				if GAIA_LIKELY (m_hash != other.m_hash)
					return false;

				return m_entity == other.m_entity;
			}

			bool operator!=(const EntityLookupKey& other) const {
				return !operator==(other);
			}
		};

		inline static const EntityLookupKey EntityBadLookupKey = EntityLookupKey(EntityBad);

		//! Component used to describe the entity name
		struct GAIA_API EntityDesc {
			//! Entity name text.
			const char* name{};
			//! Length of the entity name.
			uint32_t name_len{};
			//! Entity alias text.
			const char* alias{};
			//! Length of the entity alias.
			uint32_t alias_len{};
		};

		//----------------------------------------------------------------------
		// Pair
		//----------------------------------------------------------------------

		//! Wrapper for two Entities forming a relationship pair.
		//! Conversion to `Entity` stores only `id()` of each endpoint, so nested pairs are not
		//! representable as pair ids.
		template <>
		class pair<Entity, Entity>: public detail::pair_base {
			Entity m_first;
			Entity m_second;

		public:
			pair(Entity a, Entity b) noexcept: m_first(a), m_second(b) {}

			operator Entity() const noexcept {
				return Entity(
						m_first.id(), m_second.id(), false,
						// Always true for pairs
						true);
			}

			//! First entity of the pair.
			//! \return First entity of the pair.
			Entity first() const noexcept {
				return m_first;
			}

			//! Second entity of the pair.
			//! \return Second entity of the pair.
			Entity second() const noexcept {
				return m_second;
			}

			bool operator==(const pair& other) const {
				return m_first == other.m_first && m_second == other.m_second;
			}
			bool operator!=(const pair& other) const {
				return !operator==(other);
			}
		};

		using Pair = pair<Entity, Entity>;

		//----------------------------------------------------------------------
		// Core components
		//----------------------------------------------------------------------

		//! \cond INTERNAL
		namespace detail {
			template <typename T, typename U = void>
			struct actual_type {
				using type = typename component_type<T>::type;
			};

			template <typename T>
			struct actual_type<T, std::enable_if_t<is_pair<T>::value>> {
				using storage_type = typename T::type;
				using type = typename component_type<storage_type>::type;
			};
		} // namespace detail
		//! \endcond

		template <typename T>
		using actual_type_t = typename detail::actual_type<T>::type;

		//----------------------------------------------------------------------
		// Core components
		//----------------------------------------------------------------------

		// Core component. The entity it is attached to is ignored by queries
		struct GAIA_API Core_ {};
		// struct EntityDesc;
		// struct Component;
		struct GAIA_API OnDelete_ {};
		struct GAIA_API OnDeleteTarget_ {};
		struct GAIA_API Remove_ {};
		struct GAIA_API Delete_ {};
		struct GAIA_API Error_ {};
		//! Marks an id as required.
		//! Used directly on a non-pair id, it prevents removing that id from its owners. Used as the relation in
		//! `(Requires, target)`, it adds the target with the requiring id and prevents removing the target while the
		//! requiring id remains present.
		struct GAIA_API Requires_ {};
		struct GAIA_API CantCombine_ {};
		struct GAIA_API Exclusive_ {};
		struct GAIA_API DontFragment_ {};
		struct GAIA_API Sparse_ {};
		struct GAIA_API Acyclic_ {};
		struct GAIA_API All_ {};
		struct GAIA_API ChildOf_ {};
		struct GAIA_API Parent_ {};
		struct GAIA_API Is_ {};
		struct GAIA_API Prefab_ {};
		struct GAIA_API OnInstantiate_ {};
		struct GAIA_API Override_ {};
		struct GAIA_API Inherit_ {};
		struct GAIA_API DontInherit_ {};
		struct GAIA_API Traversable_ {};
		struct GAIA_API System_;
		struct GAIA_API DependsOn_ {};
		struct GAIA_API Observer_;

		// Query variables
		struct GAIA_API _Var0 {};
		struct GAIA_API _Var1 {};
		struct GAIA_API _Var2 {};
		struct GAIA_API _Var3 {};
		struct GAIA_API _Var4 {};
		struct GAIA_API _Var5 {};
		struct GAIA_API _Var6 {};
		struct GAIA_API _Var7 {};

		//----------------------------------------------------------------------
		// Core component entities
		//----------------------------------------------------------------------

		// Core component. The entity it is attached to is ignored by queries
		inline Entity Core = Entity(0, 0, false, false);
		inline Entity GAIA_ID(EntityDesc) = Entity(1, 0, false, false);
		inline Entity GAIA_ID(Component) = Entity(2, 0, false, false);
		// Cleanup rules
		inline Entity OnDelete = Entity(3, 0, false, false);
		inline Entity OnDeleteTarget = Entity(4, 0, false, false);
		inline Entity Remove = Entity(5, 0, false, false);
		inline Entity Delete = Entity(6, 0, false, false);
		inline Entity Error = Entity(7, 0, false, false);
		// Entity dependencies
		//! Runtime entity for `Requires_`.
		//! \see Requires_
		inline Entity Requires = Entity(8, 0, false, false);
		inline Entity CantCombine = Entity(9, 0, false, false);
		inline Entity Exclusive = Entity(10, 0, false, false);
		// Entity storage
		inline Entity DontFragment = Entity(11, 0, false, false);
		inline Entity Sparse = Entity(12, 0, false, false);
		// Graph properties
		inline Entity Acyclic = Entity(13, 0, false, false);
		inline Entity Traversable = Entity(14, 0, false, false);
		// Wildcard query entity
		inline Entity All = Entity(15, 0, false, false);
		//! Fragmenting physical hierarchy relation.
		//! The target participates in archetype identity and is deleted with its children.
		inline Entity ChildOf = Entity(16, 0, false, false);
		//! Non-fragmenting logical hierarchy relation.
		//! Parent changes do not move the source entity to another archetype.
		inline Entity Parent = Entity(17, 0, false, false);
		// Alias for a base entity/inheritance
		inline Entity Is = Entity(18, 0, false, false);
		// Template entity excluded from queries by default unless explicitly requested.
		inline Entity Prefab = Entity(19, 0, false, false);
		// Prefab instantiation policy relation and values.
		inline Entity OnInstantiate = Entity(20, 0, false, false);
		inline Entity Override = Entity(21, 0, false, false);
		inline Entity Inherit = Entity(22, 0, false, false);
		inline Entity DontInherit = Entity(23, 0, false, false);
		// Systems
		inline Entity System = Entity(24, 0, false, false);
		inline Entity DependsOn = Entity(25, 0, false, false);
		// Observers
		inline Entity Observer = Entity(26, 0, false, false);
		// Query variables
		inline Entity Var0 = Entity(27, 0, false, false);
		inline Entity Var1 = Entity(28, 0, false, false);
		inline Entity Var2 = Entity(29, 0, false, false);
		inline Entity Var3 = Entity(30, 0, false, false);
		inline Entity Var4 = Entity(31, 0, false, false);
		inline Entity Var5 = Entity(32, 0, false, false);
		inline Entity Var6 = Entity(33, 0, false, false);
		inline Entity Var7 = Entity(34, 0, false, false);
		// Runtime primitive type entities. Supported entity ids are aligned with ser::serialization_type_id:
		// runtime_primitive_type_entity(t).id() == RuntimePrimitiveTypeBaseId + (uint32_t)t.
		inline constexpr uint32_t RuntimePrimitiveTypeBaseId = 34;

		GAIA_NODISCARD inline bool is_runtime_primitive_serialization_type_id(uint32_t typeId) noexcept {
			return typeId >= (uint32_t)ser::serialization_type_id::s8 && typeId <= (uint32_t)ser::serialization_type_id::f64;
		}

		GAIA_NODISCARD inline Entity runtime_primitive_type_entity(ser::serialization_type_id type) noexcept {
			const auto typeId = (uint32_t)type;
			if (!is_runtime_primitive_serialization_type_id(typeId))
				return EntityBad;
			return Entity((EntityId)(RuntimePrimitiveTypeBaseId + typeId), 0, false, false);
		}

		GAIA_NODISCARD inline bool
		runtime_primitive_serialization_type(Entity type, ser::serialization_type_id& out) noexcept {
			if (type.pair())
				return false;

			const auto typeId = type.id() - RuntimePrimitiveTypeBaseId;
			if (!is_runtime_primitive_serialization_type_id(typeId))
				return false;

			out = (ser::serialization_type_id)typeId;
			return true;
		}

		inline Entity S8 = runtime_primitive_type_entity(ser::serialization_type_id::s8);
		inline Entity U8 = runtime_primitive_type_entity(ser::serialization_type_id::u8);
		inline Entity S16 = runtime_primitive_type_entity(ser::serialization_type_id::s16);
		inline Entity U16 = runtime_primitive_type_entity(ser::serialization_type_id::u16);
		inline Entity S32 = runtime_primitive_type_entity(ser::serialization_type_id::s32);
		inline Entity U32 = runtime_primitive_type_entity(ser::serialization_type_id::u32);
		inline Entity S64 = runtime_primitive_type_entity(ser::serialization_type_id::s64);
		inline Entity U64 = runtime_primitive_type_entity(ser::serialization_type_id::u64);
		inline Entity Bool = runtime_primitive_type_entity(ser::serialization_type_id::b);
		inline Entity Char8 = runtime_primitive_type_entity(ser::serialization_type_id::c8);
		inline Entity Char16 = runtime_primitive_type_entity(ser::serialization_type_id::c16);
		inline Entity Char32 = runtime_primitive_type_entity(ser::serialization_type_id::c32);
		inline Entity F8 = runtime_primitive_type_entity(ser::serialization_type_id::f8);
		inline Entity F16 = runtime_primitive_type_entity(ser::serialization_type_id::f16);
		inline Entity F32 = runtime_primitive_type_entity(ser::serialization_type_id::f32);
		inline Entity F64 = runtime_primitive_type_entity(ser::serialization_type_id::f64);
		inline static constexpr uint32_t MaxVarCnt = 8;

		// Core component ids are append-only.
		// Existing core ids must remain stable; new core components may only be added
		// after LastCoreComponent.
		//
		// Because of that, old snapshots stay loadable without bumping the serializer
		// version: any serialized entity id greater than the saved last core id is
		// remapped by the current core-id delta during load.
		//
		// Reordering or removing core components is not supported by this compatibility path.
		// Always has to match the last internal entity.
		inline Entity GAIA_ID(LastCoreComponent) = F64;

		//----------------------------------------------------------------------
		// Helper functions
		//----------------------------------------------------------------------

		GAIA_NODISCARD inline bool is_wildcard(EntityId entityId) {
			return entityId == All.id();
		}

		GAIA_NODISCARD inline bool is_wildcard(Entity entity) {
			return entity.pair() && (is_wildcard(entity.id()) || is_wildcard(entity.gen()));
		}

		GAIA_NODISCARD inline bool is_wildcard(Pair pair) {
			return pair.first() == All || pair.second() == All;
		}

		GAIA_NODISCARD inline bool is_variable(EntityId entityId) {
			return entityId <= Var7.id() && entityId >= Var0.id();
		}

		GAIA_NODISCARD inline bool is_variable(Entity entity) {
			return entity.id() <= Var7.id() && entity.id() >= Var0.id();
		}

		GAIA_NODISCARD inline bool is_variable(Pair pair) {
			return is_variable(pair.first()) || is_variable(pair.second());
		}

		//----------------------------------------------------------------------
		// Entity id enumeration
		//----------------------------------------------------------------------

		//! Storage sources visited by `World::ids` / `World::ids_if`.
		enum class EntityIdStorage : uint8_t {
			//! Visit no storage source.
			None = 0x00,
			//! Archetype-resident ids, matching `Archetype::ids_view()`.
			Archetype = 0x01,
			//! DontFragment component membership stored outside the archetype.
			DontFragment = 0x02,
			//! Exclusive non-fragmenting pairs such as `Parent`.
			Relation = 0x04,
			//! Every storage source.
			All = 0x07
		};

		//! Combines entity-id storage sources.
		//! \param lhs First storage mask.
		//! \param rhs Second storage mask.
		//! \return Bitwise union of \a lhs and \a rhs.
		GAIA_NODISCARD constexpr EntityIdStorage operator|(EntityIdStorage lhs, EntityIdStorage rhs) {
			return (EntityIdStorage)((uint8_t)lhs | (uint8_t)rhs);
		}

		//! Tests whether a storage mask contains a requested source.
		//! \param value Combined storage mask.
		//! \param bit Storage source to test.
		//! \return True when \a bit is set in \a value.
		GAIA_NODISCARD constexpr bool entity_id_storage_has(EntityIdStorage value, EntityIdStorage bit) {
			return (((uint8_t)value) & ((uint8_t)bit)) != 0;
		}

		//! Id kinds visited by `World::ids` / `World::ids_if`.
		enum class EntityIdKind : uint8_t {
			//! Visit no id kind.
			None = 0x00,
			//! Non-pair ids: components, tags, and ordinary entities used as ids.
			Component = 0x01,
			//! Relationship pair ids.
			Pair = 0x02,
			//! Components and pairs.
			All = 0x03
		};

		//! Combines entity-id kinds.
		//! \param lhs First kind mask.
		//! \param rhs Second kind mask.
		//! \return Bitwise union of \a lhs and \a rhs.
		GAIA_NODISCARD constexpr EntityIdKind operator|(EntityIdKind lhs, EntityIdKind rhs) {
			return (EntityIdKind)((uint8_t)lhs | (uint8_t)rhs);
		}

		//! Tests whether a kind mask contains a requested kind.
		//! \param value Combined kind mask.
		//! \param bit Kind to test.
		//! \return True when \a bit is set in \a value.
		GAIA_NODISCARD constexpr bool entity_id_kind_has(EntityIdKind value, EntityIdKind bit) {
			return (((uint8_t)value) & ((uint8_t)bit)) != 0;
		}

		//! Match semantics for entity ids.
		//! Used by query terms and by `World::ids` / `World::ids_if`.
		enum class QueryMatchKind : uint8_t {
			//! Applies Gaia-ECS semantic matching, including inherited Is targets.
			Semantic,
			//! Matches entities containing the term directly or through inheritance.
			In,
			//! Matches only directly stored terms.
			Direct
		};

		//! Options for `World::ids` / `World::ids_if`.
		//! Defaults visit the complete direct type: every storage source and both id kinds.
		//! Chain helpers to narrow or combine: the first storage or kind helper restricts,
		//! later ones accumulate.
		//! Match helpers use the same names as `QueryTermOptions`: `direct()` and `in()`.
		//! Unlike queries, the default is `QueryMatchKind::Direct` because this API lists
		//! assigned ids rather than testing presence of a known term.
		struct EntityIdOptions {
			//! Selects archetype-resident ids.
			//! \return This options object.
			EntityIdOptions& archetype() {
				storage = select_storage(storage, EntityIdStorage::Archetype);
				return *this;
			}

			//! Selects DontFragment component membership.
			//! \return This options object.
			EntityIdOptions& dont_fragment() {
				storage = select_storage(storage, EntityIdStorage::DontFragment);
				return *this;
			}

			//! Selects exclusive non-fragmenting pairs.
			//! \return This options object.
			EntityIdOptions& relation() {
				storage = select_storage(storage, EntityIdStorage::Relation);
				return *this;
			}

			//! Selects non-pair ids.
			//! \return This options object.
			EntityIdOptions& components() {
				kind = select_kind(kind, EntityIdKind::Component);
				return *this;
			}

			//! Selects relationship pair ids.
			//! \return This options object.
			EntityIdOptions& pairs() {
				kind = select_kind(kind, EntityIdKind::Pair);
				return *this;
			}

			//! Restricts visitation to ids stored on the entity.
			//! \return This options object.
			EntityIdOptions& direct() {
				matchKind = QueryMatchKind::Direct;
				return *this;
			}

			//! Allows direct and inherited matches.
			//! After the direct type, appends `OnInstantiate(Inherit)` ids from `Is` bases
			//! that are not already stored on the entity. Does not enumerate inferred
			//! semantic `Pair(Is, ...)` ids; use `World::has` / `World::is` for those.
			//! \return This options object.
			EntityIdOptions& in() {
				matchKind = QueryMatchKind::In;
				return *this;
			}

		private:
			friend class World;

			EntityIdStorage storage = EntityIdStorage::All;
			EntityIdKind kind = EntityIdKind::All;
			QueryMatchKind matchKind = QueryMatchKind::Direct;

			//! Adds a storage source. The first selection after `All` replaces it.
			//! \param current Current storage mask.
			//! \param bit Storage source to select.
			//! \return Updated storage mask.
			GAIA_NODISCARD static constexpr EntityIdStorage select_storage(EntityIdStorage current, EntityIdStorage bit) {
				return current == EntityIdStorage::All ? bit : (current | bit);
			}

			//! Adds an id kind. The first selection after `All` replaces it.
			//! \param current Current kind mask.
			//! \param bit Kind to select.
			//! \return Updated kind mask.
			GAIA_NODISCARD static constexpr EntityIdKind select_kind(EntityIdKind current, EntityIdKind bit) {
				return current == EntityIdKind::All ? bit : (current | bit);
			}
		};

	} // namespace ecs

	namespace cnt {
		//! Converts an \a Entity to a sparse id.
		template <>
		struct to_sparse_id<ecs::Entity> {
			//! Returns the entity index as a sparse id.
			//! \param item entity to convert.
			//! \return Sparse id matching the entity index.
			static sparse_id get(const ecs::Entity& item) noexcept {
				// Cut off the flags
				return item.id();
			}
		};
	} // namespace cnt
} // namespace gaia
