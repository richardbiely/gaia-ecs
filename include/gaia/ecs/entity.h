#pragma once
#include <inttypes.h>

namespace gaia {
	namespace ecs {

		using EntityId = uint32_t;
		using EntityGenId = uint32_t;

		struct Entity final {
			static constexpr uint32_t IdBits = 20;	// A million entities
			static constexpr uint32_t GenBits = 12; // 4096 generations
			static constexpr uint32_t IdMask = (1u << IdBits) - 1;
			static constexpr uint32_t GenMask = (1u << GenBits) - 1;
			static_assert(
					IdBits + GenBits <= 32,
					"Entity Id and Gen must fit inside 32 bits"); // Let's fit within 4
																												// bytes. If needed we
																												// can expand it to 8
																												// bytes in the future

		private:
			struct EntityData {
				//! Index in entity array
				uint32_t id : IdBits;
				//! Generation index. Incremented every time an entity is deleted. -1 is
				//! reserved and means it is an entity used for delayed creation
				uint32_t gen : GenBits;
			};

			union {
				EntityData data;
				uint32_t val;
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

			[[nodiscard]] constexpr bool
			operator==(const Entity& other) const noexcept {
				return val == other.val;
			}
			[[nodiscard]] constexpr bool
			operator!=(const Entity& other) const noexcept {
				return val != other.val;
			}

			auto id() const { return data.id; }
			auto gen() const { return data.gen; }
			auto value() const { return val; }
		};

		struct EntityNull_t {
			[[nodiscard]] operator Entity() const noexcept {
				return Entity(Entity::IdMask, Entity::GenMask);
			}

			[[nodiscard]] constexpr bool
			operator==(const EntityNull_t&) const noexcept {
				return true;
			}
			[[nodiscard]] constexpr bool
			operator!=(const EntityNull_t&) const noexcept {
				return false;
			}
		};

		[[nodiscard]] inline bool
		operator==(const EntityNull_t& null, const Entity& entity) noexcept {
			return static_cast<Entity>(null).id() == entity.id();
		}

		[[nodiscard]] inline bool
		operator!=(const EntityNull_t& null, const Entity& entity) noexcept {
			return static_cast<Entity>(null).id() != entity.id();
		}

		[[nodiscard]] inline bool
		operator==(const Entity& entity, const EntityNull_t& null) noexcept {
			return null == entity;
		}

		[[nodiscard]] inline bool
		operator!=(const Entity& entity, const EntityNull_t& null) noexcept {
			return null != entity;
		}

		inline constexpr EntityNull_t EntityNull{};

		class Chunk;
		struct EntityContainer {
			//! Chunk the entity currently resides in
			Chunk* chunk;
			//! For allocated entity: Index of entity within chunk + generation ID.
			//! For deleted entity: Index of the next entity in the implicit list +
			//! generation ID.
			EntityId idx;
			//! Generation ID
			EntityGenId gen;
		};

	} // namespace ecs
} // namespace gaia
