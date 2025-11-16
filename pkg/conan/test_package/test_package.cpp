#include <gaia.h>

int main() {
	gaia::ecs::World world;
	gaia::ecs::Entity e = world.add();
	(void)e;

	return 0;
}