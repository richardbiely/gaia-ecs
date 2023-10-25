#pragma once
#include "../cnt/ilist.h"
#include "../config/config.h"

#include <cinttypes>
#include <type_traits>

namespace gaia {
	namespace ecs {
		using EntityInternalType = uint32_t;
		using EntityId = EntityInternalType;
		using EntityGenId = EntityInternalType;

		struct Entity final {
			static constexpr EntityInternalType IdBits = GAIA_ENTITY_IDBITS;
			static constexpr EntityInternalType GenBits = GAIA_ENTITY_GENBITS;
			static constexpr EntityInternalType IdMask = (uint32_t)(uint64_t(1) << IdBits) - 1;
			static constexpr EntityInternalType GenMask = (uint32_t)(uint64_t(1) << GenBits) - 1;

			static constexpr uint32_t EntityBitsTotal = IdBits + GenBits;
			using EntitySizeType = std::conditional_t<(EntityBitsTotal > 32), uint64_t, uint32_t>;
			static_assert(EntityBitsTotal <= 64, "EntityBitsTotal must fit inside 64 bits");
			static_assert(IdBits <= 31, "Entity IdBits must be at most 31 bits long");
			static_assert(GenBits > 10, "Entity GenBits is recommended to be at least 10 bits long");

		private:
			struct EntityData {
				//! Index in entity array
				EntityInternalType id: IdBits;
				//! Generation index. Incremented every time an entity is deleted
				EntityInternalType gen: GenBits;
			};

			union {
				EntityData data;
				EntitySizeType val;
			};

		public:
			Entity() noexcept = default;
			Entity(EntityId id, EntityGenId gen) {
				data.id = id;
				data.gen = gen;
			}
			~Entity() = default;

			Entity(Entity&&) noexcept = default;
			Entity(const Entity&) = default;
			Entity& operator=(Entity&&) noexcept = default;
			Entity& operator=(const Entity&) = default;

			GAIA_NODISCARD constexpr bool operator==(const Entity& other) const noexcept {
				return val == other.val;
			}
			GAIA_NODISCARD constexpr bool operator!=(const Entity& other) const noexcept {
				return val != other.val;
			}

			auto id() const {
				return data.id;
			}
			auto gen() const {
				return data.gen;
			}
			auto value() const {
				return val;
			}
		};

		struct EntityNull_t {
			GAIA_NODISCARD operator Entity() const noexcept {
				return Entity(Entity::IdMask, Entity::GenMask);
			}

			GAIA_NODISCARD constexpr bool operator==([[maybe_unused]] const EntityNull_t& null) const noexcept {
				return true;
			}
			GAIA_NODISCARD constexpr bool operator!=([[maybe_unused]] const EntityNull_t& null) const noexcept {
				return false;
			}
		};

		GAIA_NODISCARD inline bool operator==(const EntityNull_t& null, const Entity& entity) noexcept {
			return static_cast<Entity>(null).id() == entity.id();
		}

		GAIA_NODISCARD inline bool operator!=(const EntityNull_t& null, const Entity& entity) noexcept {
			return static_cast<Entity>(null).id() != entity.id();
		}

		GAIA_NODISCARD inline bool operator==(const Entity& entity, const EntityNull_t& null) noexcept {
			return null == entity;
		}

		GAIA_NODISCARD inline bool operator!=(const Entity& entity, const EntityNull_t& null) noexcept {
			return null != entity;
		}

		inline constexpr EntityNull_t EntityNull{};

		class Chunk;

		struct EntityContainer: cnt::ilist_item_base {
			//! Allocated items: Index in the list.
			//! Deleted items: Index of the next deleted item in the list.
			uint32_t idx;
			//! Generation ID
			uint32_t gen : 31;
			//! Disabled
			uint32_t dis : 1;
			//! Chunk the entity currently resides in
			Chunk* pChunk;
#if !GAIA_64
			uint32_t pChunk_padding;
#endif

			EntityContainer() = default;
			EntityContainer(uint32_t index, uint32_t generation): idx(index), gen(generation), dis(0), pChunk(nullptr) {}
		};
	} // namespace ecs
} // namespace gaia
