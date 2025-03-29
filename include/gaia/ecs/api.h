#pragma once
#include "../config/config.h"
#include "id.h"

namespace gaia {
	namespace ecs {
		class World;
		struct EntityContainer;

		GAIA_NODISCARD const EntityContainer& fetch(const World& world, Entity entity);
		GAIA_NODISCARD EntityContainer& fetch_mut(World& world, Entity entity);

		GAIA_NODISCARD bool valid(const World& world, Entity entity);

		void del(World& world, Entity entity);
	} // namespace ecs
} // namespace gaia