#pragma once
#include <cstdint>

#include "../cnt/darray.h"

namespace gaia {
	namespace ecs {
		class Archetype;

		using ArchetypeId = uint32_t;
		using ArchetypeList = cnt::darray<Archetype*>;

		static constexpr ArchetypeId ArchetypeIdBad = (ArchetypeId)-1;
	} // namespace ecs
} // namespace gaia