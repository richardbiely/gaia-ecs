#include <cstdint>

#include <gaia.h>

int main() {
	gaia::ecs::World world;

	const auto root = world.add();
	const auto childOfNode = world.add();
	const auto parentNode = world.add();

	world.child(childOfNode, root);
	world.parent(parentNode, childOfNode);
	world.child(root, parentNode);

	world.del(root);
	world.update();

	if (world.has(root))
		return 1;
	if (world.has(childOfNode))
		return 2;
	if (world.has(parentNode))
		return 3;
	return 0;
}
