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

	const auto relation = world.add();
	const auto entityA = world.add();
	const auto entityB = world.add();
	world.add(relation, gaia::ecs::Exclusive);
	world.add(relation, gaia::ecs::DontFragment);
	world.add(relation, gaia::ecs::Pair(gaia::ecs::OnDeleteTarget, gaia::ecs::Delete));
	world.add(entityA, gaia::ecs::Pair(relation, entityB));
	world.add(entityB, gaia::ecs::Pair(relation, entityA));

	world.del(entityA);
	world.update();

	if (world.has(entityA))
		return 4;
	if (world.has(entityB))
		return 5;
	return 0;
}
