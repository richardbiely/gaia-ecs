#pragma once
#include <inttypes.h>

namespace gaia {
	namespace ecs {
		using EntityId = uint32_t;
		using EntityGenId = uint32_t;

		struct Entity final {
			using EntityInternalType = uint32_t;
			static constexpr EntityInternalType IdBits = 20; // A million entities
			static constexpr EntityInternalType GenBits = 12; // 4096 generations
			static constexpr EntityInternalType IdMask = (1u << IdBits) - 1;
			static constexpr EntityInternalType GenMask = (1u << GenBits) - 1;
			static_assert(
					IdBits + GenBits <= sizeof(EntityInternalType) * 8,
					"Entity IdBits and GenBits must fit inside EntityInternalType");

		private:
			struct EntityData {
				//! Index in entity array
				EntityInternalType id: IdBits;
				//! Generation index. Incremented every time an entity is deleted
				EntityInternalType gen: GenBits;
			};

			union {
				EntityData data;
				EntityInternalType val;
			};

		public:
			Entity() = default;
			Entity(EntityId id, EntityGenId gen) {
				data.id = id;
				data.gen = gen;
			}

			Entity(Entity&&) = default;
			Entity& operator=(Entity&&) = default;
			Entity(const Entity&) = default;
			Entity& operator=(const Entity&) = default;

			[[nodiscard]] constexpr bool operator==(const Entity& other) const noexcept {
				return val == other.val;
			}
			[[nodiscard]] constexpr bool operator!=(const Entity& other) const noexcept {
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
			[[nodiscard]] operator Entity() const noexcept {
				return Entity(Entity::IdMask, Entity::GenMask);
			}

			[[nodiscard]] constexpr bool operator==(const EntityNull_t&) const noexcept {
				return true;
			}
			[[nodiscard]] constexpr bool operator!=(const EntityNull_t&) const noexcept {
				return false;
			}
		};

		[[nodiscard]] inline bool operator==(const EntityNull_t& null, const Entity& entity) noexcept {
			return static_cast<Entity>(null).id() == entity.id();
		}

		[[nodiscard]] inline bool operator!=(const EntityNull_t& null, const Entity& entity) noexcept {
			return static_cast<Entity>(null).id() != entity.id();
		}

		[[nodiscard]] inline bool operator==(const Entity& entity, const EntityNull_t& null) noexcept {
			return null == entity;
		}

		[[nodiscard]] inline bool operator!=(const Entity& entity, const EntityNull_t& null) noexcept {
			return null != entity;
		}

		inline constexpr EntityNull_t EntityNull{};

		class Chunk;
		struct EntityContainer {
			//! Chunk the entity currently resides in
			Chunk* pChunk;
			//! For allocated entity: Index of entity within chunk.
			//! For deleted entity: Index of the next entity in the implicit list.
			uint32_t idx : 31;
			//! Tells if the entity is disabled. Borrows one bit from idx because it's unlikely to cause issues there
			uint32_t disabled : 1;
			//! Generation ID
			EntityGenId gen;
		};
	} // namespace ecs
} // namespace gaia
