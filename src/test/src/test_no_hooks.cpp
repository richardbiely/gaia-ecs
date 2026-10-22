#include <cstdint>

#include <gaia.h>

namespace {
	struct Position {
		float x;
	};

	struct PositionSparse {
		GAIA_STORAGE(Sparse);
		float x;
	};
} // namespace

int main() {
	gaia::ecs::World world;

	uint32_t tableSets = 0;
	uint32_t sparseSets = 0;
	(void)world.observer()
			.event(gaia::ecs::ObserverEvent::OnSet)
			.all<Position>()
			.on_each([&](const Position&) {
				++tableSets;
			})
			.entity();
	(void)world.observer()
			.event(gaia::ecs::ObserverEvent::OnSet)
			.all<PositionSparse>()
			.on_each([&](const PositionSparse&) {
				++sparseSets;
			})
			.entity();

	const auto e = world.add();
	world.add<Position>(e, {1.0f});
	world.add<PositionSparse>(e, {1.0f});

	// Set side effects are not hooks. OnSet observers must still see them with hooks compiled out.
	tableSets = 0;
	sparseSets = 0;
	world.modify<Position, false>(e);
	world.modify<PositionSparse, false>(e);
	if (tableSets != 0 || sparseSets != 0)
		return 1;
	world.modify<Position, true>(e);
	world.modify<PositionSparse, true>(e);
	if (tableSets != 1 || sparseSets != 1)
		return 2;

	world.query().all<Position&>().each([](Position& p) {
		p.x += 1.0f;
	});
	if (world.get<Position>(e).x != 2.0f)
		return 3;

	tableSets = 0;
	gaia::ecs::CommandBufferST cb(world);
	cb.set<Position>(e, {5.0f});
	cb.commit();
	if (tableSets != 1 || world.get<Position>(e).x != 5.0f)
		return 4;

	world.del(e);
	world.update();
	return world.has(e) ? 5 : 0;
}
