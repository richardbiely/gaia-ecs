#pragma once
#include "../config/config.h"

#include <cstdint>

#include "../mem/data_layout_policy.h"

namespace gaia {
	namespace ecs {
		using Identifier = uint64_t;
		inline constexpr Identifier IdentifierBad = (Identifier)-1;
		inline constexpr Identifier EntityCompMask = IdentifierBad << 1;

		using IdentifierId = uint32_t;
		using IdentifierData = uint32_t;

		using EntityId = IdentifierId;
		using ComponentId = IdentifierId;

		inline constexpr IdentifierId IdentifierIdBad = (IdentifierId)-1;

		struct Entity final {
			static constexpr uint32_t IdMask = IdentifierIdBad;

			struct InternalData {
				//! Index in the entity array
				EntityId id;
				//! Generation index. Incremented every time an entity is deleted
				IdentifierData gen : 31;
				//! Special flag indicating that this is an entity (used by IsEntity). Always 1
				IdentifierData isEntity : 1;
			};
			static_assert(sizeof(InternalData) == sizeof(Identifier));

			union {
				InternalData data;
				Identifier val;
			};

			Entity() noexcept = default;
			Entity(Identifier value) noexcept {
				val = value;
				GAIA_ASSERT(value == IdentifierBad || is_entity());
				data.isEntity = 1;
			}
			Entity(EntityId id, IdentifierData gen) noexcept {
				data.id = id;
				data.gen = gen;
				data.isEntity = 1;
			}

			GAIA_NODISCARD constexpr auto id() const noexcept {
				return (uint32_t)data.id;
			}
			GAIA_NODISCARD constexpr auto gen() const noexcept {
				return (uint32_t)data.gen;
			}
			GAIA_NODISCARD constexpr bool is_entity() const noexcept {
				const auto isEntity = (uint32_t)data.isEntity;
				GAIA_ASSERT(isEntity == 1);
				return isEntity == 1;
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
				return id() < other.id();
			}
		};

		struct Component final {
			static constexpr uint32_t IdMask = IdentifierIdBad;
			static constexpr uint32_t MaxAlignment_Bits = 10;
			static constexpr uint32_t MaxAlignment = (1U << MaxAlignment_Bits) - 1;
			static constexpr uint32_t MaxComponentSize_Bits = 8;
			static constexpr uint32_t MaxComponentSizeInBytes = (1 << MaxComponentSize_Bits) - 1;

			struct InternalData {
				//! Index in the entity array
				//detail::ComponentDescId id;
				uint32_t id;
				//! Component is SoA
				IdentifierData soa: meta::StructToTupleMaxTypes_Bits;
				//! Component size
				IdentifierData size: MaxComponentSize_Bits;
				//! Component alignment
				IdentifierData alig: MaxAlignment_Bits;
				//! Unused part
				IdentifierData unused : 9;
				//! Special flag indicating that this is an entity (used by IsEntity). Always 0
				IdentifierData isEntity : 1;
			};
			static_assert(sizeof(InternalData) == sizeof(Identifier));

			union {
				InternalData data;
				Identifier val;
			};

			Component() noexcept = default;
			Component(Identifier value) noexcept {
				val = value;
				GAIA_ASSERT(value == IdentifierBad || !is_entity());
				data.unused = 0;
				data.isEntity = 0;
			}
			Component(uint32_t id, uint32_t soa, uint32_t size, uint32_t alig) noexcept {
				data.id = id;
				data.soa = soa;
				data.size = size;
				data.alig = alig;
				data.unused = 0;
				data.isEntity = 0;
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
			GAIA_NODISCARD constexpr bool is_entity() const noexcept {
				const auto isEntity = (uint32_t)data.isEntity;
				GAIA_ASSERT(isEntity == 0);
				return isEntity == 1;
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

		template <typename T>
		inline bool is_entity(T id) {
			return id.is_entity();
		}
		inline bool is_entity(Identifier id) {
			return static_cast<Entity>(id).is_entity();
		}

	} // namespace ecs
} // namespace gaia