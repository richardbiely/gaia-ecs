#pragma once
#include <cstdint>

namespace gaia {
	namespace ser {
		class ser_buffer_binary;
	}

	namespace ecs {
		class World;
		class Archetype;
		struct Entity;

		using QueryId = uint32_t;
		using GroupId = uint32_t;
		using QuerySerBuffer = ser::ser_buffer_binary;

		using TSortByFunc = int (*)(const World&, const void*, const void*);
		using TGroupByFunc = GroupId (*)(const World&, const Archetype&, Entity);
	} // namespace ecs
} // namespace gaia