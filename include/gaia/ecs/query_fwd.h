#pragma once
#include <cstdint>

namespace gaia {
	namespace ser {
		class ser_buffer_binary_dyn;
	}

	namespace ecs {
		class World;
		class Archetype;
		struct Entity;

		//! Stable numeric identifier assigned to a cached query.
		using QueryId = uint32_t;
		//! Numeric key returned by query grouping callbacks.
		using GroupId = uint32_t;
		//! Dynamic binary buffer used to serialize query definitions.
		using QuerySerBuffer = ser::ser_buffer_binary_dyn;

		//! Comparator callback used to order query rows by component values.
		using TSortByFunc = int (*)(const World&, const void*, const void*);
		//! Callback used to assign an archetype entity to a query group.
		using TGroupByFunc = GroupId (*)(const World&, const Archetype&, Entity);
	} // namespace ecs
} // namespace gaia