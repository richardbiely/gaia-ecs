#pragma once
#include <inttypes.h>

namespace gaia {
namespace ecs {
class Chunk;

using EntityId = uint32_t;
using EntityGenId = uint32_t;

struct Entity final {
  static constexpr uint32_t IdBits = 20;  // A million entities
  static constexpr uint32_t GenBits = 12; // 4096 generations
  static constexpr uint32_t IdInvalid = (1u << IdBits) - 1;
  static constexpr uint32_t GenInvalid = (1u << GenBits) - 1;
  static_assert(
      IdBits + GenBits <= 32,
      "Entity Id and Gen must fit inside 32 bits"); // Let's fit within 4 bytes.
                                                    // If needed we can expand
                                                    // it to 8 bytes in the
                                                    // future

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
  Entity() : val(-1) {}
  Entity(EntityId id, EntityGenId gen) {
    data.id = id;
    data.gen = gen;
  }

  Entity(Entity &&) = default;
  Entity &operator=(Entity &&) = default;
  Entity(const Entity &) = default;
  Entity &operator=(const Entity &) = default;

  bool operator==(const Entity &other) const { return val == other.val; }
  bool operator!=(const Entity &other) const { return !operator==(other); }

  EntityId id() const { return data.id; }
  EntityGenId gen() const { return data.gen; }

  [[nodiscard]] bool IsValid() const {
    return data.id != IdInvalid && data.gen != GenInvalid;
  }
};

struct EntityContainer {
  //! Chunk the entity currently resides in
  Chunk *chunk;
  //! For allocated entity: Index of entity within chunk + generation ID of
  //! entity For   deleted entity: Index of the next entity in the implicit list
  //! + generation ID of entity
  Entity entity;
};
} // namespace ecs
} // namespace gaia
