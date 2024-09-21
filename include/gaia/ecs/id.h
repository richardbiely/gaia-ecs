#pragma once
#include "../config/config.h"

#include <cstdint>
#include <type_traits>

#include "../core/hashing_policy.h"
#include "../core/utility.h"
#include "../mem/data_layout_policy.h"
#include "../cnt/sparse_storage.h"

namespace gaia {
	namespace ecs {
#define GAIA_ID(type) GAIA_ID_##type

		using Identifier = uint64_t;
		inline constexpr Identifier IdentifierBad = (Identifier)-1;
		inline constexpr Identifier EntityCompMask = IdentifierBad << 1;

		using IdentifierId = uint32_t;
		using IdentifierData = uint32_t;

		using EntityId = IdentifierId;
		using ComponentId = IdentifierId;

		inline constexpr IdentifierId IdentifierIdBad = (IdentifierId)-1;

		// ------------------------------------------------------------------------------------
		// Component
		// ------------------------------------------------------------------------------------

		struct Component final {
			static constexpr uint32_t IdMask = IdentifierIdBad;
			static constexpr uint32_t MaxAlignment_Bits = 10;
			static constexpr uint32_t MaxAlignment = (1U << MaxAlignment_Bits) - 1;
			static constexpr uint32_t MaxComponentSize_Bits = 8;
			static constexpr uint32_t MaxComponentSizeInBytes = (1 << MaxComponentSize_Bits) - 1;

			struct InternalData {
				//! Index in the entity array
				// detail::ComponentDescId id;
				uint32_t id;
				//! Component is SoA
				IdentifierData soa: meta::StructToTupleMaxTypes_Bits;
				//! Component size
				IdentifierData size: MaxComponentSize_Bits;
				//! Component alignment
				IdentifierData alig: MaxAlignment_Bits;
				//! Unused part
				IdentifierData unused : 10;
			};
			static_assert(sizeof(InternalData) == sizeof(Identifier));

			union {
				InternalData data;
				Identifier val;
			};

			Component() noexcept = default;

			Component(uint32_t id, uint32_t soa, uint32_t size, uint32_t alig) noexcept {
				data.id = id;
				data.soa = soa;
				data.size = size;
				data.alig = alig;
				data.unused = 0;
			}

			GAIA_NODISCARD constexpr auto id() const noexcept {
				return (uint32_t)data.id;
			}

			GAIA_NODISCARD constexpr auto soa() const noexcept {
				return (uint32_t)data.soa;
			}

			GAIA_NODISCARD constexpr auto size() const noexcept {
				return (uint32_t)data.size;
			}

			GAIA_NODISCARD constexpr auto alig() const noexcept {
				return (uint32_t)data.alig;
			}

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
		};

		//----------------------------------------------------------------------
		// Entity kind
		//----------------------------------------------------------------------

		enum EntityKind : uint8_t {
			// Generic entity, one per entity
			EK_Gen = 0,
			// Unique entity, one per chunk
			EK_Uni,
			// Number of entity kinds
			EK_Count
		};

		inline constexpr const char* EntityKindString[EntityKind::EK_Count] = {"Gen", "Uni"};

		//----------------------------------------------------------------------
		// Id type deduction
		//----------------------------------------------------------------------

		template <typename T>
		struct uni {
			static_assert(core::is_raw_v<T>);
			static_assert(
					std::is_trivial_v<T> ||
							// For non-trivial T the comparison operator must be implemented because
							// defragmentation needs it to figure out if entities can be moved around.
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
				//! Same as Type
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
				//! T or uni<T> depending on entity kind specified
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
		// Pair helpers
		//----------------------------------------------------------------------

		namespace detail {
			struct pair_base {};
		} // namespace detail

		//! Wrapper for two types forming a relationship pair.
		//! Depending on what types are used to form a pair it can contain a value.
		//! To determine the storage type the following logic is applied:
		//! If \tparam Rel is non-empty, the storage type is Rel.
		//! If \tparam Rel is empty and \tparam Tgt is non-empty, the storage type is Tgt.
		//! \tparam Rel relation part of the relationship
		//! \tparam Tgt target part of the relationship
		template <typename Rel, typename Tgt>
		class pair: public detail::pair_base {
			using rel_comp_type = component_type_t<Rel>;
			using tgt_comp_type = component_type_t<Tgt>;

		public:
			using rel = typename rel_comp_type::TypeFull;
			using tgt = typename tgt_comp_type::TypeFull;
			using rel_type = typename rel_comp_type::Type;
			using tgt_type = typename tgt_comp_type::Type;
			using rel_original = typename rel_comp_type::TypeOriginal;
			using tgt_original = typename tgt_comp_type::TypeOriginal;
			using type = std::conditional_t<!std::is_empty_v<rel_type> || std::is_empty_v<tgt_type>, rel, tgt>;
		};

		template <typename T>
		struct is_pair {
			static constexpr bool value = std::is_base_of<detail::pair_base, core::raw_t<T>>::value;
		};

		// ------------------------------------------------------------------------------------
		// Entity
		// ------------------------------------------------------------------------------------

		struct Entity final {
			static constexpr uint32_t IdMask = IdentifierIdBad;

			struct InternalData {
				//! Index in the entity array
				EntityId id;

				///////////////////////////////////////////////////////////////////
				// Bits in this section need to be 1:1 with EntityContainer data
				///////////////////////////////////////////////////////////////////

				//! Generation index. Incremented every time an entity is deleted
				IdentifierData gen : 28;
				//! 0-component, 1-entity
				IdentifierData ent : 1;
				//! 0-ordinary, 1-pair
				IdentifierData pair : 1;
				//! 0=EntityKind::CT_Gen, 1=EntityKind::CT_Uni
				IdentifierData kind : 1;
				//! Unused
				IdentifierData unused : 1;

				///////////////////////////////////////////////////////////////////
			};
			static_assert(sizeof(InternalData) == sizeof(Identifier));

			union {
				InternalData data;
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
			Entity(EntityId id, IdentifierData gen, bool isEntity, bool isPair, EntityKind kind) noexcept {
				data.id = id;
				data.gen = gen;
				data.ent = isEntity;
				data.pair = isPair;
				data.kind = kind;
				data.unused = 0;
			}

			GAIA_NODISCARD constexpr auto id() const noexcept {
				return (uint32_t)data.id;
			}

			GAIA_NODISCARD constexpr auto gen() const noexcept {
				return (uint32_t)data.gen;
			}

			GAIA_NODISCARD constexpr bool entity() const noexcept {
				return data.ent != 0;
			}

			GAIA_NODISCARD constexpr bool pair() const noexcept {
				return data.pair != 0;
			}

			GAIA_NODISCARD constexpr auto kind() const noexcept {
				return (EntityKind)data.kind;
			}

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
		};

		inline static const Entity EntityBad = Entity(IdentifierBad);

		//! Hashmap lookup structure used for Entity
		struct EntityLookupKey {
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
			static constexpr bool IsDirectHashKey = true;

			EntityLookupKey() = default;
			explicit EntityLookupKey(Entity entity): m_entity(entity), m_hash(calc(entity)) {}
			~EntityLookupKey() = default;

			EntityLookupKey(const EntityLookupKey&) = default;
			EntityLookupKey(EntityLookupKey&&) = default;
			EntityLookupKey& operator=(const EntityLookupKey&) = default;
			EntityLookupKey& operator=(EntityLookupKey&&) = default;

			Entity entity() const {
				return m_entity;
			}

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
		struct EntityDesc {
			const char* name{};
			uint32_t len{};
		};

		//----------------------------------------------------------------------
		// Pair
		//----------------------------------------------------------------------

		//! Wrapper for two Entities forming a relationship pair.
		template <>
		class pair<Entity, Entity>: public detail::pair_base {
			Entity m_first;
			Entity m_second;

		public:
			pair(Entity a, Entity b) noexcept: m_first(a), m_second(b) {}

			operator Entity() const noexcept {
				return Entity(
						m_first.id(), m_second.id(),
						// Pairs have no way of telling gen and uni entities apart.
						// Therefore, for first, we use the entity bit as Gen/Uni...
						(bool)m_first.kind(),
						// Always true for pairs
						true,
						// ... and for second, we use the kind bit.
						m_second.kind());
			}

			Entity first() const noexcept {
				return m_first;
			}

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

		template <typename T>
		using actual_type_t = typename detail::actual_type<T>::type;

		//----------------------------------------------------------------------
		// Core components
		//----------------------------------------------------------------------

		// Core component. The entity it is attached to is ignored by queries
		struct Core_ {};
		// struct EntityDesc;
		// struct Component;
		struct OnDelete_ {};
		struct OnDeleteTarget_ {};
		struct Remove_ {};
		struct Delete_ {};
		struct Error_ {};
		struct Requires_ {};
		struct CantCombine_ {};
		struct Exclusive_ {};
		struct Acyclic_ {};
		struct All_ {};
		struct ChildOf_ {};
		struct Is_ {};
		struct Traversable_ {};
		// struct System2_;
		struct DependsOn_ {};

		// Query variables
		struct _Var0 {};
		struct _Var1 {};
		struct _Var2 {};
		struct _Var3 {};
		struct _Var4 {};
		struct _Var5 {};
		struct _Var6 {};
		struct _Var7 {};

		//----------------------------------------------------------------------
		// Core component entities
		//----------------------------------------------------------------------

		// Core component. The entity it is attached to is ignored by queries
		inline Entity Core = Entity(0, 0, false, false, EntityKind::EK_Gen);
		inline Entity GAIA_ID(EntityDesc) = Entity(1, 0, false, false, EntityKind::EK_Gen);
		inline Entity GAIA_ID(Component) = Entity(2, 0, false, false, EntityKind::EK_Gen);
		// Cleanup rules
		inline Entity OnDelete = Entity(3, 0, false, false, EntityKind::EK_Gen);
		inline Entity OnDeleteTarget = Entity(4, 0, false, false, EntityKind::EK_Gen);
		inline Entity Remove = Entity(5, 0, false, false, EntityKind::EK_Gen);
		inline Entity Delete = Entity(6, 0, false, false, EntityKind::EK_Gen);
		inline Entity Error = Entity(7, 0, false, false, EntityKind::EK_Gen);
		// Entity dependencies
		inline Entity Requires = Entity(8, 0, false, false, EntityKind::EK_Gen);
		inline Entity CantCombine = Entity(9, 0, false, false, EntityKind::EK_Gen);
		inline Entity Exclusive = Entity(10, 0, false, false, EntityKind::EK_Gen);
		// Graph properties
		inline Entity Acyclic = Entity(11, 0, false, false, EntityKind::EK_Gen);
		inline Entity Traversable = Entity(12, 0, false, false, EntityKind::EK_Gen);
		// Wildcard query entity
		inline Entity All = Entity(13, 0, false, false, EntityKind::EK_Gen);
		// Entity representing a physical hierarchy.
		// When the relationship target is deleted all children are deleted as well.
		inline Entity ChildOf = Entity(14, 0, false, false, EntityKind::EK_Gen);
		// Alias for a base entity/inheritance
		inline Entity Is = Entity(15, 0, false, false, EntityKind::EK_Gen);
		// Systems
		inline Entity System2 = Entity(16, 0, false, false, EntityKind::EK_Gen);
		inline Entity DependsOn = Entity(17, 0, false, false, EntityKind::EK_Gen);
		// Query variables
		inline Entity Var0 = Entity(18, 0, false, false, EntityKind::EK_Gen);
		inline Entity Var1 = Entity(19, 0, false, false, EntityKind::EK_Gen);
		inline Entity Var2 = Entity(20, 0, false, false, EntityKind::EK_Gen);
		inline Entity Var3 = Entity(21, 0, false, false, EntityKind::EK_Gen);
		inline Entity Var4 = Entity(22, 0, false, false, EntityKind::EK_Gen);
		inline Entity Var5 = Entity(23, 0, false, false, EntityKind::EK_Gen);
		inline Entity Var6 = Entity(24, 0, false, false, EntityKind::EK_Gen);
		inline Entity Var7 = Entity(25, 0, false, false, EntityKind::EK_Gen);

		// Always has to match the last internal entity
		inline Entity GAIA_ID(LastCoreComponent) = Var7;

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

	} // namespace ecs

	namespace cnt {
		template <>
		struct to_sparse_id<ecs::Entity> {
			static sparse_id get(const ecs::Entity& item) noexcept {
				// Cut off the flags
				return item.value() >> 4;
			}
		};
	}
} // namespace gaia