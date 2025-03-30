#pragma once
#include <cstdint>

namespace gaia {
	namespace ecs {
		using IdentifierId = uint32_t;
		using IdentifierData = uint32_t;

		using EntityId = IdentifierId;
		using ComponentId = IdentifierId;
	} // namespace ecs
} // namespace gaia