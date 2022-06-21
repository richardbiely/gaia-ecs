#pragma once
#include <cinttypes>

namespace gaia {
	namespace ecs {
		using EntityId = uint32_t;
		using EntityGenId = uint32_t;
		struct Entity;

		class Chunk;
		class Archetype;
		class World;

		class EntityQuery;

		struct ComponentInfo;
		class CommandBuffer;
	} // namespace ecs
} // namespace gaia
