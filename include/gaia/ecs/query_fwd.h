#pragma once
#include <cstdint>

#include "data_buffer_fwd.h"

namespace gaia {
	namespace ecs {
		class World;
		class Archetype;
		struct Entity;

		using QueryId = uint32_t;
		using GroupId = uint32_t;
		using QuerySerBuffer = SerializationBufferDyn;

		using TSortByFunc = int (*)(const World&, const void*, const void*);
		using TGroupByFunc = GroupId (*)(const World&, const Archetype&, Entity);
	} // namespace ecs
} // namespace gaia