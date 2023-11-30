#pragma once
#include "../config/config.h"

#include <cstdint>

#include "../core/hashing_policy.h"
#include "../mem/data_layout_policy.h"

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

		enum EntityKind : uint8_t {
			// Generic entity, one per entity
			EK_Gen = 0,
			// Unique entity, one per chunk
			EK_Uni,
			// Number of entity kinds
			EK_Count
		};

		inline constexpr const char* EntityKindString[EntityKind::EK_Count] = {"Gen", "Uni"};

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

			constexpr Entity() noexcept: val(IdentifierBad){};
			constexpr Entity(Identifier value) noexcept: val(value) {}

			// Special constructor for cnt::ilist
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
		};

		struct EntityLookupKey {
			using LookupHash = core::direct_hash_key<uint32_t>;

		private:
			//! Entity
			Entity m_entity;
			//! Entity hash
			LookupHash m_hash;

			static LookupHash calc(Entity entity) {
				return {static_cast<uint32_t>(core::calculate_hash64(entity.value()))};
			}

		public:
			static constexpr bool IsDirectHashKey = true;

			EntityLookupKey() = default;
			EntityLookupKey(Entity entity): m_entity(entity), m_hash(calc(entity)) {}

			Entity entity() const {
				return m_entity;
			}

			auto hash() const {
				return m_hash.hash;
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

		struct EntityDesc {
			const char* name{};
			uint32_t len{};
		};

		class Pair {
			Entity m_first;
			Entity m_second;

		public:
			Pair(Entity a, Entity b) noexcept: m_first(a), m_second(b) {}

			operator Entity() const noexcept {
				return Entity(m_first.id(), m_second.id(), false, true, EntityKind::EK_Gen);
			}

			Entity first() const noexcept {
				return m_first;
			}

			Entity second() const noexcept {
				return m_second;
			}
		};

		struct Core {};
		struct OnDelete_ {};
		struct OnDeleteTarget_ {};
		struct Remove_ {};
		struct Delete_ {};
		struct All_ {};

		inline Entity GAIA_ID(Core) = Entity(0, 0, false, false, EntityKind::EK_Gen);
		inline Entity GAIA_ID(EntityDesc) = Entity(1, 0, false, false, EntityKind::EK_Gen);
		inline Entity GAIA_ID(Component) = Entity(2, 0, false, false, EntityKind::EK_Gen);
		inline Entity OnDelete = Entity(3, false, false, false, EntityKind::EK_Gen);
		inline Entity OnDeleteTarget = Entity(4, false, false, false, EntityKind::EK_Gen);
		inline Entity Remove = Entity(5, false, false, false, EntityKind::EK_Gen);
		inline Entity Delete = Entity(6, false, false, false, EntityKind::EK_Gen);
		inline Entity All = Entity(7, 0, false, false, EntityKind::EK_Gen);
		inline Entity GAIA_ID(LastCoreComponent) = All;
	} // namespace ecs
} // namespace gaia