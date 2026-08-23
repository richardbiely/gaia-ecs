#if !defined(GAIA_ALLOC_ARENA_LOCK)
	#error GAIA_ALLOC_ARENA_LOCK must be defined by the build
#endif
#if !GAIA_ALLOC_ARENA_LOCK
	#error This target verifies a lock-enabled build
#endif

#include <gaia.h>

#include <atomic>
#include <thread>
#include <vector>

struct Position {
	float x, y, z;
};

struct PositionSparse {
	GAIA_STORAGE(Sparse);
	float x, y, z;
};

int main() {
	constexpr int kThreads = 3;
	constexpr int kEntities = 256;
	std::atomic<int> ok{0};

	std::vector<std::thread> threads;
	threads.reserve(kThreads);
	for (int t = 0; t < kThreads; ++t) {
		threads.emplace_back([&, t] {
			gaia::ecs::World world;
			world.add<Position>();
			world.add<PositionSparse>();

			float sum = 0.f;
			for (int i = 0; i < kEntities; ++i) {
				auto e = world.add();
				world.add<Position>(e, {(float)i, (float)t, 0.f});
				world.add<PositionSparse>(e, {(float)i, 1.f, 2.f});
				sum += world.get<Position>(e).x;
			}

			if (world.query().all<Position>().count() != (uint32_t)kEntities)
				return;
			if (world.query().all<PositionSparse>().count() != (uint32_t)kEntities)
				return;
			if (!(sum > 0.f))
				return;
			ok.fetch_add(1, std::memory_order_relaxed);
		});
	}

	for (auto& th: threads)
		th.join();

	return ok.load() == kThreads ? 0 : 1;
}
