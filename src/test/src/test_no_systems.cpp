#include <gaia.h>

int main() {
	gaia::ecs::World world;

	const auto entity = world.add();
	world.del(entity);
	world.update();

	return world.has(entity) ? 1 : 0;
}
