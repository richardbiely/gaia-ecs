#pragma once
#include <inttypes.h>

namespace gaia {
	namespace ecs {
		using EntityId = uint32_t;
		using EntityGenId = uint32_t;
		struct Entity;

		class Chunk;
		class Archetype;
		class World;

		class CreationQuery;
		class EntityQuery;

		struct ComponentMetaData;
		class EntityCommandBuffer;
	} // namespace ecs
} // namespace gaia
