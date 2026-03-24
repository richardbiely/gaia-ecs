#include "test_common.h"

TEST_CASE("add_n") {
	TestWorld twld;

	auto qa = wld.query().all<Acceleration>();
	auto qp = wld.query().all<Position>();

	auto e = wld.add();
	wld.add<Acceleration>(e, {1.f, 1.f, 1.f});
	wld.add<Position>(e, {2.f, 2.f, 2.f});

	constexpr uint32_t N = 1000;

	wld.add_n(e, N);

	{
		uint32_t cnt = 0;
		qa.each([&](ecs::Entity ent, const Acceleration& a) {
			++cnt;

			if (ent == e) {
				CHECK(a.x == 1.f);
				CHECK(a.y == 1.f);
				CHECK(a.z == 1.f);
			} else {
				// CHECK(a.x == 0.f);
				// CHECK(a.y == 0.f);
				// CHECK(a.z == 0.f);
			}
		});
		CHECK(cnt == N + 1);
	}
	{
		uint32_t cnt = 0;
		qp.each([&](ecs::Entity ent, const Position& p) {
			++cnt;

			if (ent == e) {
				CHECK(p.x == 2.f);
				CHECK(p.y == 2.f);
				CHECK(p.z == 2.f);
			} else {
				// CHECK(p.x == 0.f);
				// CHECK(p.y == 0.f);
				// CHECK(p.z == 0.f);
			}
		});
		CHECK(cnt == N + 1);
	}
}

TEST_CASE("copy_n") {
	TestWorld twld;

	auto qa = wld.query().all<Acceleration>();
	auto qp = wld.query().all<Position>();

	auto e = wld.add();
	wld.add<Acceleration>(e, {1.f, 1.f, 1.f});
	wld.add<Position>(e, {2.f, 2.f, 2.f});

	{
		const auto& a = wld.get<Acceleration>(e);
		CHECK(a.x == 1.f);
		CHECK(a.y == 1.f);
		CHECK(a.z == 1.f);
	}
	{
		const auto& p = wld.get<Position>(e);
		CHECK(p.x == 2.f);
		CHECK(p.y == 2.f);
		CHECK(p.z == 2.f);
	}

	constexpr uint32_t N = 1000;

	wld.copy_n(e, N);

	{
		uint32_t cnt = 0;
		qa.each([&](const Acceleration& a) {
			++cnt;

			CHECK(a.x == 1.f);
			CHECK(a.y == 1.f);
			CHECK(a.z == 1.f);
		});
		CHECK(cnt == N + 1);
	}
	{
		uint32_t cnt = 0;
		qp.each([&](const Position& p) {
			++cnt;

			CHECK(p.x == 2.f);
			CHECK(p.y == 2.f);
			CHECK(p.z == 2.f);
		});
		CHECK(cnt == N + 1);
	}
}

TEST_CASE("Set - generic") {
	TestWorld twld;

	constexpr uint32_t N = 100;
	constexpr uint32_t NE = 10;
	cnt::darr<ecs::Entity> arr;
	arr.reserve(N);
	cnt::darr<ecs::Entity> arre;
	arr.reserve(NE);

	GAIA_FOR(NE) {
		auto e = wld.add();
		arre.push_back(e);
	}

	GAIA_FOR(N) {
		const auto ent = wld.add();
		arr.push_back(ent);
		wld.add<Position>(ent, {});
		wld.add<Rotation>(ent, {});
		wld.add<Scale>(ent, {});
		wld.add<Acceleration>(ent, {});
		wld.add<Else>(ent, {});
		GAIA_FOR_(NE, j) {
			auto e = arre[j];
			wld.add(ent, e);
		}
	}

	// Default values
	for (const auto ent: arr) {
		auto r = wld.get<Rotation>(ent);
		CHECK(r.x == 0.f);
		CHECK(r.y == 0.f);
		CHECK(r.z == 0.f);
		CHECK(r.w == 0.f);

		auto s = wld.get<Scale>(ent);
		CHECK(s.x == 0.f);
		CHECK(s.y == 0.f);
		CHECK(s.z == 0.f);

		auto e = wld.get<Else>(ent);
		CHECK(e.value == false);
	}

	// Modify values
	{
		ecs::Query q = wld.query().all<Rotation&>().all<Scale&>().all<Else&>();

		q.each([&](ecs::Iter& it) {
			auto rotationView = it.view_mut<Rotation>();
			auto scaleView = it.view_mut<Scale>();
			auto elseView = it.view_mut<Else>();

			GAIA_EACH(it) {
				rotationView[i] = {1, 2, 3, 4};
				scaleView[i] = {11, 22, 33};
				elseView[i] = {true};
			}
		});

		q.each([&](ecs::Iter& it) {
			auto rotationView = it.view<Rotation>();
			auto scaleView = it.view<Scale>();
			auto elseView = it.view<Else>();

			GAIA_EACH(it) {
				auto r = rotationView[i];
				CHECK(r.x == 1.f);
				CHECK(r.y == 2.f);
				CHECK(r.z == 3.f);
				CHECK(r.w == 4.f);

				auto s = scaleView[i];
				CHECK(s.x == 11.f);
				CHECK(s.y == 22.f);
				CHECK(s.z == 33.f);

				auto e = elseView[i];
				CHECK(e.value == true);
			}
		});

		{
			uint32_t entIdx = 0;
			q.each([&](ecs::Entity ent) {
				CHECK(ent == arr[entIdx++]);
			});
			entIdx = 0;
			q.each([&](ecs::Iter& it) {
				auto entityView = it.view<ecs::Entity>();
				GAIA_EACH(it) {
					CHECK(entityView[i] == arr[entIdx++]);
				}
			});
		}

		for (const auto ent: arr) {
			auto r = wld.get<Rotation>(ent);
			CHECK(r.x == 1.f);
			CHECK(r.y == 2.f);
			CHECK(r.z == 3.f);
			CHECK(r.w == 4.f);

			auto s = wld.get<Scale>(ent);
			CHECK(s.x == 11.f);
			CHECK(s.y == 22.f);
			CHECK(s.z == 33.f);

			auto e = wld.get<Else>(ent);
			CHECK(e.value == true);
		}
	}

	// Modify values + view idx
	{
		ecs::Query q = wld.query().all<Rotation&>().all<Scale&>().all<Else&>();

		q.each([&](ecs::Iter& it) {
			auto rotationView = it.view_mut<Rotation>(0);
			auto scaleView = it.view_mut<Scale>(1);
			auto elseView = it.view_mut<Else>(2);

			GAIA_EACH(it) {
				rotationView[i] = {1, 2, 3, 4};
				scaleView[i] = {11, 22, 33};
				elseView[i] = {true};
			}
		});

		q.each([&](ecs::Iter& it) {
			auto rotationView = it.view<Rotation>(0);
			auto scaleView = it.view<Scale>(1);
			auto elseView = it.view<Else>(2);

			GAIA_EACH(it) {
				auto r = rotationView[i];
				CHECK(r.x == 1.f);
				CHECK(r.y == 2.f);
				CHECK(r.z == 3.f);
				CHECK(r.w == 4.f);

				auto s = scaleView[i];
				CHECK(s.x == 11.f);
				CHECK(s.y == 22.f);
				CHECK(s.z == 33.f);

				auto e = elseView[i];
				CHECK(e.value == true);
			}
		});

		for (const auto ent: arr) {
			auto r = wld.get<Rotation>(ent);
			CHECK(r.x == 1.f);
			CHECK(r.y == 2.f);
			CHECK(r.z == 3.f);
			CHECK(r.w == 4.f);

			auto s = wld.get<Scale>(ent);
			CHECK(s.x == 11.f);
			CHECK(s.y == 22.f);
			CHECK(s.z == 33.f);

			auto e = wld.get<Else>(ent);
			CHECK(e.value == true);
		}
	}

	// Add one more component and check if the values are still fine after creating a new archetype
	{
		auto ent = wld.copy(arr[0]);
		wld.add<Position>(ent, {5, 6, 7});

		auto r = wld.get<Rotation>(ent);
		CHECK(r.x == 1.f);
		CHECK(r.y == 2.f);
		CHECK(r.z == 3.f);
		CHECK(r.w == 4.f);

		auto s = wld.get<Scale>(ent);
		CHECK(s.x == 11.f);
		CHECK(s.y == 22.f);
		CHECK(s.z == 33.f);

		auto e = wld.get<Else>(ent);
		CHECK(e.value == true);
	}
}

TEST_CASE("Set - generic & unique") {
	TestWorld twld;

	constexpr uint32_t N = 100;
	cnt::darr<ecs::Entity> arr;
	arr.reserve(N);

	GAIA_FOR(N) {
		arr.push_back(wld.add());
		wld.add<Rotation>(arr.back(), {});
		wld.add<Scale>(arr.back(), {});
		wld.add<Else>(arr.back(), {});
		wld.add<ecs::uni<Position>>(arr.back(), {});
	}

	// Default values
	for (const auto ent: arr) {
		auto r = wld.get<Rotation>(ent);
		CHECK(r.x == 0.f);
		CHECK(r.y == 0.f);
		CHECK(r.z == 0.f);
		CHECK(r.w == 0.f);

		auto s = wld.get<Scale>(ent);
		CHECK(s.x == 0.f);
		CHECK(s.y == 0.f);
		CHECK(s.z == 0.f);

		auto e = wld.get<Else>(ent);
		CHECK(e.value == false);

		auto p = wld.get<ecs::uni<Position>>(ent);
		CHECK(p.x == 0.f);
		CHECK(p.y == 0.f);
		CHECK(p.z == 0.f);
	}

	// Modify values
	{
		ecs::Query q = wld.query().all<Rotation&>().all<Scale&>().all<Else&>();

		q.each([&](ecs::Iter& it) {
			auto rotationView = it.view_mut<Rotation>();
			auto scaleView = it.view_mut<Scale>();
			auto elseView = it.view_mut<Else>();

			GAIA_EACH(it) {
				rotationView[i] = {1, 2, 3, 4};
				scaleView[i] = {11, 22, 33};
				elseView[i] = {true};
			}
		});

		wld.set<ecs::uni<Position>>(arr[0]) = {111, 222, 333};

		{
			Position p = wld.get<ecs::uni<Position>>(arr[0]);
			CHECK(p.x == 111.f);
			CHECK(p.y == 222.f);
			CHECK(p.z == 333.f);
		}
		{
			for (const auto ent: arr) {
				auto r = wld.get<Rotation>(ent);
				CHECK(r.x == 1.f);
				CHECK(r.y == 2.f);
				CHECK(r.z == 3.f);
				CHECK(r.w == 4.f);

				auto s = wld.get<Scale>(ent);
				CHECK(s.x == 11.f);
				CHECK(s.y == 22.f);
				CHECK(s.z == 33.f);

				auto e = wld.get<Else>(ent);
				CHECK(e.value == true);
			}
		}
		{
			auto p = wld.get<ecs::uni<Position>>(arr[0]);
			CHECK(p.x == 111.f);
			CHECK(p.y == 222.f);
			CHECK(p.z == 333.f);
		}
	}
}

TEST_CASE("Components - non trivial") {
	TestWorld twld;

	constexpr uint32_t N = 100;
	cnt::darr<ecs::Entity> arr;
	arr.reserve(N);

	GAIA_FOR(N) {
		arr.push_back(wld.add());
		wld.add<StringComponent>(arr.back(), {});
		wld.add<StringComponent2>(arr.back(), {});
		wld.add<PositionNonTrivial>(arr.back(), {});
	}

	// Default values
	for (const auto ent: arr) {
		const auto& s1 = wld.get<StringComponent>(ent);
		CHECK(s1.value.empty());

		{
			auto s2 = wld.get<StringComponent2>(ent);
			CHECK(s2.value == StringComponent2DefaultValue);
		}
		{
			const auto& s2 = wld.get<StringComponent2>(ent);
			CHECK(s2.value == StringComponent2DefaultValue);
		}

		const auto& p = wld.get<PositionNonTrivial>(ent);
		CHECK(p.x == 1.f);
		CHECK(p.y == 2.f);
		CHECK(p.z == 3.f);
	}

	// Modify values
	{
		ecs::Query q = wld.query().all<StringComponent&>().all<StringComponent2&>().all<PositionNonTrivial&>();

		q.each([&](ecs::Iter& it) {
			auto strView = it.view_mut<StringComponent>();
			auto str2View = it.view_mut<StringComponent2>();
			auto posView = it.view_mut<PositionNonTrivial>();

			GAIA_EACH(it) {
				strView[i] = {StringComponentDefaultValue};
				str2View[i].value = StringComponent2DefaultValue_2;
				posView[i] = {111, 222, 333};
			}
		});

		q.each([&](ecs::Iter& it) {
			auto strView = it.view_mut<StringComponent>(0);
			auto str2View = it.view_mut<StringComponent2>(1);
			auto posView = it.view_mut<PositionNonTrivial>(2);

			GAIA_EACH(it) {
				strView[i] = {StringComponentDefaultValue};
				str2View[i].value = StringComponent2DefaultValue_2;
				posView[i] = {111, 222, 333};
			}
		});

		for (const auto ent: arr) {
			const auto& s1 = wld.get<StringComponent>(ent);
			CHECK(s1.value == StringComponentDefaultValue);

			const auto& s2 = wld.get<StringComponent2>(ent);
			CHECK(s2.value == StringComponent2DefaultValue_2);

			const auto& p = wld.get<PositionNonTrivial>(ent);
			CHECK(p.x == 111.f);
			CHECK(p.y == 222.f);
			CHECK(p.z == 333.f);
		}
	}

	// Add one more component and check if the values are still fine after creating a new archetype
	{
		auto ent = wld.copy(arr[0]);
		wld.add<Position>(ent, {5, 6, 7});

		const auto& s1 = wld.get<StringComponent>(ent);
		CHECK(s1.value == StringComponentDefaultValue);

		const auto& s2 = wld.get<StringComponent2>(ent);
		CHECK(s2.value == StringComponent2DefaultValue_2);

		const auto& p = wld.get<PositionNonTrivial>(ent);
		CHECK(p.x == 111.f);
		CHECK(p.y == 222.f);
		CHECK(p.z == 333.f);
	}
}

#if GAIA_ECS_CHUNK_ALLOCATOR
TEST_CASE("ChunkAllocator") {
	SUBCASE("size class thresholds") {
		CHECK(ecs::mem_block_size_type(1) == 0);
		CHECK(ecs::mem_block_size_type(ecs::MinMemoryBlockSize) == 0);
		CHECK(ecs::mem_block_size_type(ecs::MinMemoryBlockSize + 1) == 1);
		CHECK(ecs::mem_block_size_type(ecs::MinMemoryBlockSize * 2) == 1);
		CHECK(ecs::mem_block_size_type(ecs::MinMemoryBlockSize * 2 + 1) == 2);
		CHECK(ecs::mem_block_size_type(ecs::MaxMemoryBlockSize) == 2);
	}

	SUBCASE("stats report used memory per size class") {
		auto& alloc = ecs::ChunkAllocator::get();
		alloc.flush();

		constexpr auto NBlocks = ecs::detail::MemoryPage::NBlocks;

		void* p8k = alloc.alloc(ecs::MinMemoryBlockSize);
		void* p16k = alloc.alloc(ecs::MinMemoryBlockSize * 2);
		void* p32k = alloc.alloc(ecs::MaxMemoryBlockSize);

		{
			const auto stats = alloc.stats();
			CHECK(stats.stats[0].num_pages == 1);
			CHECK(stats.stats[1].num_pages == 1);
			CHECK(stats.stats[2].num_pages == 1);
			CHECK(stats.stats[0].num_pages_free == 1);
			CHECK(stats.stats[1].num_pages_free == 1);
			CHECK(stats.stats[2].num_pages_free == 1);

			CHECK(stats.stats[0].mem_total == (uint64_t)ecs::mem_block_size(0) * NBlocks);
			CHECK(stats.stats[1].mem_total == (uint64_t)ecs::mem_block_size(1) * NBlocks);
			CHECK(stats.stats[2].mem_total == (uint64_t)ecs::mem_block_size(2) * NBlocks);

			CHECK(stats.stats[0].mem_used == ecs::mem_block_size(0));
			CHECK(stats.stats[1].mem_used == ecs::mem_block_size(1));
			CHECK(stats.stats[2].mem_used == ecs::mem_block_size(2));
		}

		alloc.free(p8k);
		alloc.free(p16k);
		alloc.free(p32k);
		alloc.flush();

		{
			const auto stats = alloc.stats();
			CHECK(stats.stats[0].num_pages == 1);
			CHECK(stats.stats[1].num_pages == 1);
			CHECK(stats.stats[2].num_pages == 1);
			CHECK(stats.stats[0].num_pages_free == 1);
			CHECK(stats.stats[1].num_pages_free == 1);
			CHECK(stats.stats[2].num_pages_free == 1);
			CHECK(stats.stats[0].mem_total == (uint64_t)ecs::mem_block_size(0) * NBlocks);
			CHECK(stats.stats[1].mem_total == (uint64_t)ecs::mem_block_size(1) * NBlocks);
			CHECK(stats.stats[2].mem_total == (uint64_t)ecs::mem_block_size(2) * NBlocks);
			CHECK(stats.stats[0].mem_used == 0);
			CHECK(stats.stats[1].mem_used == 0);
			CHECK(stats.stats[2].mem_used == 0);
		}

		alloc.flush(true);
		{
			const auto stats = alloc.stats();
			CHECK(stats.stats[0].num_pages == 0);
			CHECK(stats.stats[1].num_pages == 0);
			CHECK(stats.stats[2].num_pages == 0);
			CHECK(stats.stats[0].num_pages_free == 0);
			CHECK(stats.stats[1].num_pages_free == 0);
			CHECK(stats.stats[2].num_pages_free == 0);
			CHECK(stats.stats[0].mem_total == 0);
			CHECK(stats.stats[1].mem_total == 0);
			CHECK(stats.stats[2].mem_total == 0);
			CHECK(stats.stats[0].mem_used == 0);
			CHECK(stats.stats[1].mem_used == 0);
			CHECK(stats.stats[2].mem_used == 0);
		}
	}

	SUBCASE("stats track full and spill pages") {
		auto& alloc = ecs::ChunkAllocator::get();
		alloc.flush();

		constexpr auto NBlocks = ecs::detail::MemoryPage::NBlocks;

		void* blocks[NBlocks + 1]{};
		GAIA_FOR(NBlocks) {
			blocks[i] = alloc.alloc(ecs::MaxMemoryBlockSize);
		}

		{
			const auto stats = alloc.stats();
			CHECK(stats.stats[2].num_pages == 1);
			CHECK(stats.stats[2].num_pages_free == 0);
			CHECK(stats.stats[2].mem_total == (uint64_t)ecs::mem_block_size(2) * NBlocks);
			CHECK(stats.stats[2].mem_used == (uint64_t)ecs::mem_block_size(2) * NBlocks);
		}

		blocks[NBlocks] = alloc.alloc(ecs::MaxMemoryBlockSize);
		{
			const auto stats = alloc.stats();
			CHECK(stats.stats[2].num_pages == 2);
			CHECK(stats.stats[2].num_pages_free == 1);
			CHECK(stats.stats[2].mem_total == (uint64_t)ecs::mem_block_size(2) * NBlocks * 2);
			CHECK(stats.stats[2].mem_used == (uint64_t)ecs::mem_block_size(2) * (NBlocks + 1));
		}

		// Freeing a block from a full page should move it back to the free list.
		alloc.free(blocks[0]);
		blocks[0] = nullptr;
		{
			const auto stats = alloc.stats();
			CHECK(stats.stats[2].num_pages == 2);
			CHECK(stats.stats[2].num_pages_free == 2);
			CHECK(stats.stats[2].mem_used == (uint64_t)ecs::mem_block_size(2) * NBlocks);
		}

		for (void* p: blocks) {
			if (p != nullptr)
				alloc.free(p);
		}
		alloc.flush();

		{
			const auto stats = alloc.stats();
			CHECK(stats.stats[2].num_pages == 1);
			CHECK(stats.stats[2].num_pages_free == 1);
			CHECK(stats.stats[2].mem_total == (uint64_t)ecs::mem_block_size(2) * NBlocks);
			CHECK(stats.stats[2].mem_used == 0);
		}

		alloc.flush(true);
		{
			const auto stats = alloc.stats();
			CHECK(stats.stats[2].num_pages == 0);
			CHECK(stats.stats[2].num_pages_free == 0);
			CHECK(stats.stats[2].mem_total == 0);
			CHECK(stats.stats[2].mem_used == 0);
		}
	}

	// We do this mostly for code coverage
	{
		TestWorld twld;
		ecs::CommandBufferST cb(wld);
		auto mainEntity = wld.add();

		wld.add<Position>(mainEntity, {1, 2, 3});

		constexpr uint32_t M = 100000;
		(void)wld.copy_n(mainEntity, M);

		// delete all created entities
		auto q = wld.query().all<Position>();
		CHECK(q.count() == M + 1);
		q.each([&](ecs::Entity e) {
			cb.del(e);
		});

		cb.commit();
		wld.update();
		ecs::ChunkAllocator::get().flush();
	}

	// We do this just for code coverage.
	// Hide logging so it does not spam the results of unit testing.
	const auto logLevelBackup = util::g_logLevelMask;
	util::g_logLevelMask = 0;
	ecs::ChunkAllocator::get().diag();
	util::g_logLevelMask = logLevelBackup;
}
#endif

TEST_CASE("CommandBuffer") {
	SUBCASE("Entity creation") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		const uint32_t N = 100;
		GAIA_FOR(N) {
			[[maybe_unused]] auto tmp = cb.add();
		}

		cb.commit();

		CHECK(wld.size() == ecs::GAIA_ID(LastCoreComponent).id() + 1 + N);
	}

	SUBCASE("Entity creation from a query") {
		TestWorld twld;
		auto mainEntity = wld.add();
		wld.add<Position>(mainEntity, {1, 2, 3});

		uint32_t cnt = 0;
		auto q = wld.query().all<Position>();
		q.each([&](ecs::Iter& it) {
			auto& cb = it.cmd_buffer_st();
			auto e = cb.add();
			cb.add<Position>(e, {4, 5, 6});
			++cnt;
		});
		CHECK(cnt == 1);

		cnt = 0;
		q.each([&](ecs::Iter& it) {
			auto ev = it.view<ecs::Entity>();
			auto pv = it.view<Position>();
			GAIA_EACH(it) {
				const auto& p = pv[i];
				if (ev[i] == mainEntity) {
					CHECK(p.x == 1.f);
					CHECK(p.y == 2.f);
					CHECK(p.z == 3.f);
				} else {
					CHECK(p.x == 4.f);
					CHECK(p.y == 5.f);
					CHECK(p.z == 6.f);
				}
				++cnt;
			}
		});
		CHECK(cnt == 2);
	}

	SUBCASE("Entity creation from another entity") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		auto mainEntity = wld.add();

		const uint32_t N = 100;
		GAIA_FOR(N) {
			[[maybe_unused]] auto tmp = cb.copy(mainEntity);
		}

		cb.commit();

		CHECK(wld.size() == ecs::GAIA_ID(LastCoreComponent).id() + 1 + 1 + N); // core + mainEntity + N others
	}

	SUBCASE("Entity creation from a to-be-created entity") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		auto mainEntity = cb.add();

		const uint32_t N = 100;
		GAIA_FOR(N) {
			[[maybe_unused]] auto tmp = cb.copy(mainEntity);
		}

		cb.commit();

		CHECK(wld.size() == ecs::GAIA_ID(LastCoreComponent).id() + 1 + 1 + N); // core + mainEntity + N others
	}

	SUBCASE("Entity creation from another entity with a component") {
		{
			TestWorld twld;
			ecs::CommandBufferST cb(wld);
			auto mainEntity = wld.add();

			wld.add<Position>(mainEntity, {1, 2, 3});

			auto q = wld.query().all<Position>();
			CHECK(q.count() == 1);
			uint32_t i = 0;
			q.each([&](const Position& p) {
				CHECK(p.x == 1.f);
				CHECK(p.y == 2.f);
				CHECK(p.z == 3.f);
				++i;
			});
			CHECK(i == 1);

			(void)wld.copy(mainEntity);
			CHECK(q.count() == 2);
			i = 0;
			q.each([&](const Position& p) {
				CHECK(p.x == 1.f);
				CHECK(p.y == 2.f);
				CHECK(p.z == 3.f);
				++i;
			});
			CHECK(i == 2);
		}

		{
			TestWorld twld;
			ecs::CommandBufferST cb(wld);
			auto mainEntity = wld.add();

			wld.add<Position>(mainEntity, {1, 2, 3});

			[[maybe_unused]] auto tmp = cb.copy(mainEntity);
			cb.commit();

			auto q = wld.query().all<Position>();
			CHECK(q.count() == 2);
			uint32_t i = 0;
			q.each([&](const Position& p) {
				CHECK(p.x == 1.f);
				CHECK(p.y == 2.f);
				CHECK(p.z == 3.f);
				++i;
			});
			CHECK(i == 2);
		}
	}

	SUBCASE("Entity creation from another entity with a SoA component") {
		{
			TestWorld twld;
			ecs::CommandBufferST cb(wld);
			auto mainEntity = wld.add();

			wld.add<PositionSoA>(mainEntity, {1, 2, 3});

			auto q = wld.query().all<PositionSoA>();
			CHECK(q.count() == 1);
			uint32_t i = 0;
			q.each([&](const PositionSoA& p) {
				CHECK(p.x == 1.f);
				CHECK(p.y == 2.f);
				CHECK(p.z == 3.f);
				++i;
			});
			CHECK(i == 1);

			(void)wld.copy(mainEntity);
			CHECK(q.count() == 2);
			i = 0;
			q.each([&](const PositionSoA& p) {
				CHECK(p.x == 1.f);
				CHECK(p.y == 2.f);
				CHECK(p.z == 3.f);
				++i;
			});
			CHECK(i == 2);
		}

		{
			TestWorld twld;
			ecs::CommandBufferST cb(wld);
			auto mainEntity = wld.add();

			wld.add<PositionSoA>(mainEntity, {1, 2, 3});

			(void)cb.copy(mainEntity);
			cb.commit();

			auto q = wld.query().all<PositionSoA>();
			CHECK(q.count() == 2);
			uint32_t i = 0;
			q.each([&](const PositionSoA& p) {
				CHECK(p.x == 1.f);
				CHECK(p.y == 2.f);
				CHECK(p.z == 3.f);
				++i;
			});
			CHECK(i == 2);
		}
	}

	SUBCASE("Delayed component addition to an existing entity") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		auto e = wld.add();
		cb.add<Position>(e);
		CHECK_FALSE(wld.has<Position>(e));
		cb.commit();
		CHECK(wld.has<Position>(e));
	}

	SUBCASE("Delayed component addition (via entity) to an existing entity") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		auto p = wld.add<Position>().entity;

		auto e = wld.add();
		cb.add(e, p);
		CHECK_FALSE(wld.has(e, p));
		cb.commit();
		CHECK(wld.has(e, p));
	}

	SUBCASE("Delayed entity addition to an existing entity") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		auto e = wld.add(); // core + 1
		const auto s = wld.size();
		auto tmp = cb.add();
		CHECK(wld.size() == s); // no new added entity
		cb.add(e, tmp);
		cb.commit();
		CHECK(wld.size() == s + 1); // new entity added

		auto e2 = wld.get(s); // core + e + new entity
		CHECK(wld.has(e, e2));
	}

	SUBCASE("Delayed component addition to a to-be-created entity") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		(void)wld.add<Position>();

		const auto s = wld.size();

		auto tmp = cb.add(); // no new entity created yet
		CHECK(wld.size() == s);
		cb.add<Position>(tmp); // component entity created
		CHECK(wld.size() == s);
		cb.commit();
		CHECK(wld.size() == s + 1);

		auto e = wld.get(s); // + new entity
		CHECK(wld.has<Position>(e));
	}

	SUBCASE("Delayed component addition (via entity) to a to-be-created entity") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		(void)wld.add<Position>();

		const auto s = wld.size();
		auto p = wld.add<Position>().entity;
		CHECK(wld.size() == s);

		auto tmp = cb.add(); // no new entity created yet
		CHECK(wld.size() == s);
		cb.add(tmp, p);
		CHECK(wld.size() == s);
		cb.commit();
		CHECK(wld.size() == s + 1);

		auto e = wld.get(s); // + new entity
		CHECK(wld.has(e, p));
	}

	SUBCASE("Delayed entity addition to a to-be-created entity") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		const auto s = wld.size();
		auto tmpA = cb.add();
		auto tmpB = cb.add(); // core + 0 (no new entity created yet)
		CHECK(wld.size() == s);
		cb.add(tmpA, tmpB);
		CHECK(wld.size() == s);
		cb.commit();
		CHECK(wld.size() == s + 2);

		auto e1 = wld.get(s);
		auto e2 = wld.get(s + 1);
		CHECK(wld.has(e1, e2));
	}

	SUBCASE("Delayed component setting of an existing entity") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		auto e = wld.add();

		cb.add<Position>(e);
		cb.set<Position>(e, {1, 2, 3});
		CHECK_FALSE(wld.has<Position>(e));

		cb.commit();
		CHECK(wld.has<Position>(e));

		auto p = wld.get<Position>(e);
		CHECK(p.x == 1);
		CHECK(p.y == 2);
		CHECK(p.z == 3);
	}

	SUBCASE("Delayed 2 components setting of an existing entity") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		auto e = wld.add();

		cb.add<Position>(e);
		cb.set<Position>(e, {1, 2, 3});
		cb.add<Acceleration>(e);
		cb.set<Acceleration>(e, {4, 5, 6});
		CHECK_FALSE(wld.has<Position>(e));
		CHECK_FALSE(wld.has<Acceleration>(e));

		cb.commit();
		CHECK(wld.has<Position>(e));
		CHECK(wld.has<Acceleration>(e));

		auto p = wld.get<Position>(e);
		CHECK(p.x == 1.f);
		CHECK(p.y == 2.f);
		CHECK(p.z == 3.f);

		auto a = wld.get<Acceleration>(e);
		CHECK(a.x == 4.f);
		CHECK(a.y == 5.f);
		CHECK(a.z == 6.f);
	}

	SUBCASE("Delayed component setting of a to-be-created entity") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		(void)wld.add<Position>();

		const auto s = wld.size();
		auto tmp = cb.add();
		CHECK(wld.size() == s);

		cb.add<Position>(tmp);
		CHECK(wld.size() == s);
		cb.set<Position>(tmp, {1, 2, 3});
		cb.commit();
		CHECK(wld.size() == s + 1); // + new entity

		auto e = wld.get(s);
		CHECK(wld.has<Position>(e));

		auto p = wld.get<Position>(e);
		CHECK(p.x == 1.f);
		CHECK(p.y == 2.f);
		CHECK(p.z == 3.f);
	}

	SUBCASE("Delayed component add+delete of a temporary entity") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		(void)wld.add<Position>();

		const auto s = wld.size();
		auto tmp = cb.add();
		CHECK(wld.size() == s);

		cb.add<Position>(tmp);
		CHECK(wld.size() == s);
		cb.del<Position>(tmp);
		cb.commit();
		CHECK(wld.size() == s + 1); // + new entity

		auto e = wld.get(s);
		CHECK_FALSE(wld.has<Position>(e));
	}

	SUBCASE("Delayed component add+set+set") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		(void)wld.add<Position>();

		const auto s = wld.size();
		auto tmp = cb.add();
		CHECK(wld.size() == s);

		cb.add<Position>(tmp);
		cb.set<Position>(tmp, {1, 2, 3});
		CHECK(wld.size() == s);
		cb.set<Position>(tmp, {4, 5, 6});
		cb.commit();
		CHECK(wld.size() == s + 1); // + new entity

		auto e = wld.get(s);
		CHECK(wld.has<Position>(e));

		auto p = wld.get<Position>(e);
		CHECK(p.x == 4.f);
		CHECK(p.y == 5.f);
		CHECK(p.z == 6.f);
	}

	SUBCASE("Delayed component add+set+set+del") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		(void)wld.add<Position>();

		const auto s = wld.size();
		auto tmp = cb.add();
		CHECK(wld.size() == s);

		cb.add<Position>(tmp);
		cb.set<Position>(tmp, {1, 2, 3});
		CHECK(wld.size() == s);
		cb.set<Position>(tmp, {4, 5, 6});
		cb.del(tmp);
		cb.commit();
		CHECK(wld.size() == s);
	}

	SUBCASE("Delayed 2 components setting of a to-be-created entity") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		(void)wld.add<Position>();
		(void)wld.add<Acceleration>();

		const auto s = wld.size();
		auto tmp = cb.add();
		CHECK(wld.size() == s);

		cb.add<Position>(tmp);
		cb.add<Acceleration>(tmp);
		cb.set<Position>(tmp, {1, 2, 3});
		cb.set<Acceleration>(tmp, {4, 5, 6});
		cb.commit();
		CHECK(wld.size() == s + 1); // + new entity

		auto e = wld.get(s);
		CHECK(wld.has<Position>(e));
		CHECK(wld.has<Acceleration>(e));

		auto p = wld.get<Position>(e);
		CHECK(p.x == 1.f);
		CHECK(p.y == 2.f);
		CHECK(p.z == 3.f);

		auto a = wld.get<Acceleration>(e);
		CHECK(a.x == 4.f);
		CHECK(a.y == 5.f);
		CHECK(a.z == 6.f);
	}

	SUBCASE("Delayed component add with setting of a to-be-created entity") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		(void)wld.add<Position>();

		const auto s = wld.size();
		auto tmp = cb.add();
		CHECK(wld.size() == s);

		cb.add<Position>(tmp, {1, 2, 3});
		cb.commit();
		CHECK(wld.size() == s + 1); // + new entity

		auto e = wld.get(s);
		CHECK(wld.has<Position>(e));

		auto p = wld.get<Position>(e);
		CHECK(p.x == 1.f);
		CHECK(p.y == 2.f);
		CHECK(p.z == 3.f);
	}

	SUBCASE("Delayed 2 components add with setting of a to-be-created entity") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		(void)wld.add<Position>();
		(void)wld.add<Acceleration>();

		const auto s = wld.size();
		auto tmp = cb.add();
		CHECK(wld.size() == s);

		cb.add<Position>(tmp, {1, 2, 3});
		cb.add<Acceleration>(tmp, {4, 5, 6});
		cb.commit();
		CHECK(wld.size() == s + 1); // + new entity

		auto e = wld.get(s);
		CHECK(wld.has<Position>(e));
		CHECK(wld.has<Acceleration>(e));

		auto p = wld.get<Position>(e);
		CHECK(p.x == 1.f);
		CHECK(p.y == 2.f);
		CHECK(p.z == 3.f);

		auto a = wld.get<Acceleration>(e);
		CHECK(a.x == 4.f);
		CHECK(a.y == 5.f);
		CHECK(a.z == 6.f);
	}

	SUBCASE("Delayed component removal from an existing entity") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		auto e = wld.add();
		wld.add<Position>(e, {1, 2, 3});

		cb.del<Position>(e);
		CHECK(wld.has<Position>(e));
		{
			auto p = wld.get<Position>(e);
			CHECK(p.x == 1.f);
			CHECK(p.y == 2.f);
			CHECK(p.z == 3.f);
		}

		cb.commit();
		CHECK_FALSE(wld.has<Position>(e));
	}

	SUBCASE("Delayed 2 component removal from an existing entity") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		auto e = wld.add();
		wld.add<Position>(e, {1, 2, 3});
		wld.add<Acceleration>(e, {4, 5, 6});

		cb.del<Position>(e);
		cb.del<Acceleration>(e);
		CHECK(wld.has<Position>(e));
		CHECK(wld.has<Acceleration>(e));
		{
			auto p = wld.get<Position>(e);
			CHECK(p.x == 1.f);
			CHECK(p.y == 2.f);
			CHECK(p.z == 3.f);

			auto a = wld.get<Acceleration>(e);
			CHECK(a.x == 4.f);
			CHECK(a.y == 5.f);
			CHECK(a.z == 6.f);
		}

		cb.commit();
		CHECK_FALSE(wld.has<Position>(e));
		CHECK_FALSE(wld.has<Acceleration>(e));
	}

	SUBCASE("Delayed non-trivial component setting of an existing entity") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		auto e = wld.add();

		cb.add<StringComponent>(e);
		cb.set<StringComponent>(e, {StringComponentDefaultValue});
		cb.add<StringComponent2>(e);
		CHECK_FALSE(wld.has<StringComponent>(e));
		CHECK_FALSE(wld.has<StringComponent2>(e));

		cb.commit();
		CHECK(wld.has<StringComponent>(e));
		CHECK(wld.has<StringComponent2>(e));

		auto s1 = wld.get<StringComponent>(e);
		CHECK(s1.value == StringComponentDefaultValue);
		auto s2 = wld.get<StringComponent2>(e);
		CHECK(s2.value == StringComponent2DefaultValue);
	}

	SUBCASE("Delayed entity deletion") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		auto e = wld.add();

		cb.del(e);
		CHECK(wld.has(e));

		cb.commit();
		CHECK_FALSE(wld.has(e));
	}
}

TEST_CASE("Query Filter - no systems") {
	TestWorld twld;
	ecs::Query q = wld.query().all<Position>().changed<Position>();

	auto e = wld.add();
	wld.add<Position>(e);

	// System-less filters
	{
		uint32_t cnt = 0;
		q.each([&]([[maybe_unused]] const Position& a) {
			++cnt;
		});
		CHECK(cnt == 1); // first run always happens
	}
	{
		uint32_t cnt = 0;
		q.each([&]([[maybe_unused]] const Position& a) {
			++cnt;
		});
		CHECK(cnt == 0); // no change of position so this shouldn't run
	}
	{
		wld.set<Position>(e) = {};
	}
	{
		uint32_t cnt = 0;
		q.each([&]([[maybe_unused]] const Position& a) {
			++cnt;
		});
		CHECK(cnt == 1);
	}
	{
		uint32_t cnt = 0;
		q.each([&]([[maybe_unused]] const Position& a) {
			++cnt;
		});
		CHECK(cnt == 0);
	}
	{
		wld.sset<Position>(e) = {};
	}
	{
		uint32_t cnt = 0;
		q.each([&]([[maybe_unused]] const Position& a) {
			++cnt;
		});
		CHECK(cnt == 0);
	}
	{
		auto* ch = wld.get_chunk(e);
		auto p = ch->sview_mut<Position>();
		p[0] = {};
	}
	{
		uint32_t cnt = 0;
		q.each([&]([[maybe_unused]] const Position& a) {
			++cnt;
		});
		CHECK(cnt == 0);
	}
	auto e2 = wld.copy(e);
	(void)e2;
	{
		uint32_t cnt = 0;
		q.each([&]([[maybe_unused]] const Position& a) {
			++cnt;
		});
		CHECK(cnt == 2); // adding an entity triggers the change
	}
	{
		uint32_t cnt = 0;
		q.each([&]([[maybe_unused]] const Position& a) {
			++cnt;
		});
		CHECK(cnt == 0); // no new change since the last time
	}
}

TEST_CASE("Query Filter - Iter direct mutable views track changes correctly") {
	SUBCASE("AoS") {
		TestWorld twld;

		auto e = wld.add();
		wld.add<Position>(e, {1, 2, 3});

		ecs::Query qChanged = wld.query().all<Position>().changed<Position>();
		ecs::Query qMut = wld.query().all<Position&>();

		uint32_t cnt = 0;
		qChanged.each([&]([[maybe_unused]] const Position& p) {
			++cnt;
		});
		CHECK(cnt == 1);

		cnt = 0;
		qChanged.each([&]([[maybe_unused]] const Position& p) {
			++cnt;
		});
		CHECK(cnt == 0);

		qMut.each([&](ecs::Iter& it) {
			auto posView = it.sview_mut<Position>(0);
			GAIA_EACH(it) {
				posView[i].x += 10.0f;
			}
		});

		cnt = 0;
		qChanged.each([&]([[maybe_unused]] const Position& p) {
			++cnt;
		});
		CHECK(cnt == 0);

		qMut.each([&](ecs::Iter& it) {
			auto posView = it.view_mut<Position>(0);
			GAIA_EACH(it) {
				posView[i].y += 20.0f;
			}
		});

		cnt = 0;
		qChanged.each([&]([[maybe_unused]] const Position& p) {
			++cnt;
		});
		CHECK(cnt == 1);
	}

	SUBCASE("SoA") {
		TestWorld twld;

		auto e = wld.add();
		wld.add<PositionSoA>(e, {1, 2, 3});

		ecs::Query qChanged = wld.query().all<PositionSoA>().changed<PositionSoA>();
		ecs::Query qMut = wld.query().all<PositionSoA&>();

		uint32_t cnt = 0;
		qChanged.each([&]([[maybe_unused]] const PositionSoA& p) {
			++cnt;
		});
		CHECK(cnt == 1);

		cnt = 0;
		qChanged.each([&]([[maybe_unused]] const PositionSoA& p) {
			++cnt;
		});
		CHECK(cnt == 0);

		qMut.each([&](ecs::Iter& it) {
			auto posView = it.sview_mut<PositionSoA>(0);
			auto xs = posView.template set<0>();
			GAIA_EACH(it) {
				xs[i] += 10.0f;
			}
		});

		cnt = 0;
		qChanged.each([&]([[maybe_unused]] const PositionSoA& p) {
			++cnt;
		});
		CHECK(cnt == 0);

		qMut.each([&](ecs::Iter& it) {
			auto posView = it.view_mut<PositionSoA>(0);
			auto ys = posView.template set<1>();
			GAIA_EACH(it) {
				ys[i] += 20.0f;
			}
		});

		cnt = 0;
		qChanged.each([&]([[maybe_unused]] const PositionSoA& p) {
			++cnt;
		});
		CHECK(cnt == 1);
	}
}

TEST_CASE("Query Filter - Iter auto mutable views track changes correctly") {
	SUBCASE("AoS") {
		TestWorld twld;

		auto e = wld.add();
		wld.add<Position>(e, {1, 2, 3});

		ecs::Query qChanged = wld.query().all<Position>().changed<Position>();
		ecs::Query qMut = wld.query().all<Position&>();

		uint32_t cnt = 0;
		qChanged.each([&]([[maybe_unused]] const Position& p) {
			++cnt;
		});
		CHECK(cnt == 1);

		cnt = 0;
		qChanged.each([&]([[maybe_unused]] const Position& p) {
			++cnt;
		});
		CHECK(cnt == 0);

		qMut.each([&](ecs::Iter& it) {
			auto posView = it.sview_auto_any<Position&>();
			auto posViewDirect = it.sview_auto<Position&>();
			GAIA_EACH(it) {
				posView[i].x += 10.0f;
				posViewDirect[i].y += 10.0f;
			}
		});

		cnt = 0;
		qChanged.each([&]([[maybe_unused]] const Position& p) {
			++cnt;
		});
		CHECK(cnt == 0);

		qMut.each([&](ecs::Iter& it) {
			auto posView = it.view_auto_any<Position&>();
			auto posViewDirect = it.view_auto<Position&>();
			GAIA_EACH(it) {
				posView[i].x += 20.0f;
				posViewDirect[i].y += 20.0f;
			}
		});

		cnt = 0;
		qChanged.each([&]([[maybe_unused]] const Position& p) {
			++cnt;
		});
		CHECK(cnt == 1);
	}

	SUBCASE("SoA") {
		TestWorld twld;

		auto e = wld.add();
		wld.add<PositionSoA>(e, {1, 2, 3});

		ecs::Query qChanged = wld.query().all<PositionSoA>().changed<PositionSoA>();
		ecs::Query qMut = wld.query().all<PositionSoA&>();

		uint32_t cnt = 0;
		qChanged.each([&]([[maybe_unused]] const PositionSoA& p) {
			++cnt;
		});
		CHECK(cnt == 1);

		cnt = 0;
		qChanged.each([&]([[maybe_unused]] const PositionSoA& p) {
			++cnt;
		});
		CHECK(cnt == 0);

		qMut.each([&](ecs::Iter& it) {
			auto posView = it.sview_auto_any<PositionSoA&>();
			auto posViewDirect = it.sview_auto<PositionSoA&>();
			auto xs = posView.template set<0>();
			auto ys = posViewDirect.template set<1>();
			GAIA_EACH(it) {
				xs[i] += 10.0f;
				ys[i] += 10.0f;
			}
		});

		cnt = 0;
		qChanged.each([&]([[maybe_unused]] const PositionSoA& p) {
			++cnt;
		});
		CHECK(cnt == 0);

		qMut.each([&](ecs::Iter& it) {
			auto posView = it.view_auto_any<PositionSoA&>();
			auto posViewDirect = it.view_auto<PositionSoA&>();
			auto xs = posView.template set<0>();
			auto ys = posViewDirect.template set<1>();
			GAIA_EACH(it) {
				xs[i] += 20.0f;
				ys[i] += 20.0f;
			}
		});

		cnt = 0;
		qChanged.each([&]([[maybe_unused]] const PositionSoA& p) {
			++cnt;
		});
		CHECK(cnt == 1);
	}
}

template <typename TQuery>
void Test_Query_Filter_Changed_Order_NoSystems() {
	constexpr bool UseCachedQuery = std::is_same_v<TQuery, ecs::Query>;

	TestWorld twld;
	struct Marker {};
	struct A {
		int value;
	};
	struct B {
		int value;
	};

	const auto e = wld.add();
	wld.add<Marker>(e);
	wld.add<A>(e, {1});
	wld.add<B>(e, {2});

	// Intentionally reversed relative to canonical component order.
	auto q = wld.query<UseCachedQuery>() //
							 .template all<Marker>()
							 .template all<A>()
							 .template all<B>()
							 .template changed<B>()
							 .template changed<A>();

	CHECK(q.count() == 1);
	expect_exact_entities(q, {e});

	// No writes between runs.
	CHECK(q.count() == 0);
	expect_exact_entities(q, {});

	wld.set<A>(e) = {3};
	CHECK(q.count() == 1);
	expect_exact_entities(q, {e});

	CHECK(q.count() == 0);
	expect_exact_entities(q, {});

	wld.set<B>(e) = {4};
	CHECK(q.count() == 1);
	expect_exact_entities(q, {e});
}

TEST_CASE("Query Filter - changed order no systems") {
	SUBCASE("Cached query") {
		Test_Query_Filter_Changed_Order_NoSystems<ecs::Query>();
	}
	SUBCASE("Non-cached query") {
		Test_Query_Filter_Changed_Order_NoSystems<ecs::QueryUncached>();
	}
}

template <typename TQuery>
void Test_Query_Filter_Changed_Or_Missing_Component() {
	constexpr bool UseCachedQuery = std::is_same_v<TQuery, ecs::Query>;

	TestWorld twld;
	struct Marker {};
	struct A {
		int value;
	};
	struct B {
		int value;
	};

	const auto eA = wld.add();
	wld.add<Marker>(eA);
	wld.add<A>(eA, {1});

	const auto eB = wld.add();
	wld.add<Marker>(eB);
	wld.add<B>(eB, {2});

	// Archetypes can match with only one of OR terms present.
	// Both changed filters must remain safe.
	auto q = wld.query<UseCachedQuery>()
							 .template all<Marker>()
							 .template or_<A>()
							 .template or_<B>()
							 .template changed<B>()
							 .template changed<A>();

	CHECK(q.count() == 2);
	expect_exact_entities(q, {eA, eB});

	CHECK(q.count() == 0);
	expect_exact_entities(q, {});

	wld.set<A>(eA) = {3};
	CHECK(q.count() == 1);
	expect_exact_entities(q, {eA});

	wld.set<B>(eB) = {4};
	CHECK(q.count() == 1);
	expect_exact_entities(q, {eB});
}

TEST_CASE("Query Filter - changed OR terms") {
	SUBCASE("Cached query") {
		Test_Query_Filter_Changed_Or_Missing_Component<ecs::Query>();
	}
	SUBCASE("Non-cached query") {
		Test_Query_Filter_Changed_Or_Missing_Component<ecs::QueryUncached>();
	}
}

TEST_CASE("Query Filter - changed order cache keys") {
	TestWorld twld;
	struct Marker {};
	struct A {
		int value;
	};
	struct B {
		int value;
	};

	const auto e = wld.add();
	wld.add<Marker>(e);
	wld.add<A>(e, {1});
	wld.add<B>(e, {2});

	ecs::Query qAB = wld.query() //
											 .template all<Marker>()
											 .template all<A>()
											 .template all<B>()
											 .template changed<A>()
											 .template changed<B>();
	ecs::Query qBA = wld.query() //
											 .template all<Marker>()
											 .template all<A>()
											 .template all<B>()
											 .template changed<B>()
											 .template changed<A>();

	CHECK(qAB.count() == 1);
	CHECK(qBA.count() == 1);
	CHECK(qAB.id() == qBA.id());
	CHECK(qAB.gen() == qBA.gen());
}

TEST_CASE("Query Filter - cached changed queries with instance-local reporting state") {
	TestWorld twld;
	struct Marker {};
	struct A {
		int value;
	};

	const auto e = wld.add();
	wld.add<Marker>(e);
	wld.add<A>(e, {1});

	ecs::Query q0 = wld.query().template all<Marker>().template all<A>().template changed<A>();
	ecs::Query q1 = wld.query().template all<Marker>().template all<A>().template changed<A>();

	CHECK(q0.id() == q1.id());
	CHECK(q0.gen() == q1.gen());

	expect_changed_probe_state(q0, 1);
	expect_changed_consume_exact(q0, {e});
	expect_changed_probe_state(q1, 1);
	expect_changed_consume_exact(q1, {e});

	wld.set<A>(e) = {2};

	expect_changed_probe_state(q0, 1);
	expect_changed_consume_exact(q0, {e});
	expect_changed_probe_state(q1, 1);
	expect_changed_consume_exact(q1, {e});
}

TEST_CASE("Query Filter - cached changed query count is non-consuming and instance-local") {
	TestWorld twld;
	struct Marker {};
	struct A {
		int value;
	};

	const auto e = wld.add();
	wld.add<Marker>(e);
	wld.add<A>(e, {1});

	ecs::Query q0 = wld.query().template all<Marker>().template all<A>().template changed<A>();
	ecs::Query q1 = wld.query().template all<Marker>().template all<A>().template changed<A>();

	CHECK(q0.id() == q1.id());
	CHECK(q0.gen() == q1.gen());

	expect_changed_probe_state(q0, 1);
	expect_changed_probe_state(q1, 1);

	expect_changed_consume_exact(q0, {e});
	expect_changed_probe_state(q1, 1);

	expect_changed_consume_exact(q1, {e});
}

TEST_CASE("Query Filter - cached changed query empty is non-consuming and instance-local") {
	TestWorld twld;
	struct Marker {};
	struct A {
		int value;
	};

	const auto e = wld.add();
	wld.add<Marker>(e);
	wld.add<A>(e, {1});

	ecs::Query q0 = wld.query().template all<Marker>().template all<A>().template changed<A>();
	ecs::Query q1 = wld.query().template all<Marker>().template all<A>().template changed<A>();

	CHECK(q0.id() == q1.id());
	CHECK(q0.gen() == q1.gen());

	expect_changed_probe_state(q0, 1);
	expect_changed_probe_state(q1, 1);

	expect_changed_consume_exact(q0, {e});
	expect_changed_probe_state(q1, 1);

	expect_changed_consume_exact(q1, {e});
}

TEST_CASE("Query Filter - cached changed queries with instance-local var bindings") {
	TestWorld twld;
	struct Ship {};
	struct Planet {};
	struct Status {
		int value;
	};

	const auto dockedTo = wld.add();
	const auto earth = wld.add();
	const auto mars = wld.add();
	wld.add<Planet>(earth);
	wld.add<Planet>(mars);

	const auto shipEarth = wld.add();
	wld.add<Ship>(shipEarth);
	wld.add<Status>(shipEarth, {1});
	wld.add(shipEarth, ecs::Pair(dockedTo, earth));

	const auto shipMars = wld.add();
	wld.add<Ship>(shipMars);
	wld.add<Status>(shipMars, {2});
	wld.add(shipMars, ecs::Pair(dockedTo, mars));

	auto makeQuery = [&] {
		return wld.query()
				.template all<Ship>()
				.template all<Status>()
				.all(ecs::Pair(dockedTo, ecs::Var0))
				.template all<Planet>(ecs::QueryTermOptions{}.src(ecs::Var0))
				.template changed<Status>();
	};

	auto qEarth = makeQuery();
	auto qMars = makeQuery();

	qEarth.set_var(ecs::Var0, earth);
	qMars.set_var(ecs::Var0, mars);

	CHECK(qEarth.id() == qMars.id());
	CHECK(qEarth.gen() == qMars.gen());

	expect_changed_probe_state(qEarth, 1);
	expect_changed_consume_exact(qEarth, {shipEarth});
	expect_changed_probe_state(qMars, 1);
	expect_changed_consume_exact(qMars, {shipMars});

	wld.set<Status>(shipEarth) = {3};

	expect_changed_probe_state(qEarth, 1);
	expect_changed_consume_exact(qEarth, {shipEarth});
	expect_changed_probe_state(qMars, 0);
	expect_changed_consume_exact(qMars, {});

	wld.set<Status>(shipMars) = {4};

	expect_changed_probe_state(qEarth, 0);
	expect_changed_consume_exact(qEarth, {});
	expect_changed_probe_state(qMars, 1);
	expect_changed_consume_exact(qMars, {shipMars});
}

TEST_CASE("Query Filter - cached changed queries with instance-local group filters") {
	TestWorld twld;
	struct Position {
		float x, y, z;
	};

	const auto eats = wld.add();
	const auto carrot = wld.add();
	const auto salad = wld.add();

	const auto eCarrotA = wld.add();
	wld.add<Position>(eCarrotA, {1, 0, 0});
	wld.add(eCarrotA, ecs::Pair(eats, carrot));

	const auto eCarrotB = wld.add();
	wld.add<Position>(eCarrotB, {2, 0, 0});
	wld.add(eCarrotB, ecs::Pair(eats, carrot));

	const auto eSaladA = wld.add();
	wld.add<Position>(eSaladA, {3, 0, 0});
	wld.add(eSaladA, ecs::Pair(eats, salad));

	const auto eSaladB = wld.add();
	wld.add<Position>(eSaladB, {4, 0, 0});
	wld.add(eSaladB, ecs::Pair(eats, salad));

	auto qCarrot = wld.query().all<Position>().group_by(eats).changed<Position>();
	auto qSalad = wld.query().all<Position>().group_by(eats).changed<Position>();

	qCarrot.group_id(carrot);
	qSalad.group_id(salad);

	CHECK(qCarrot.id() == qSalad.id());
	CHECK(qCarrot.gen() == qSalad.gen());

	expect_changed_probe_state(qCarrot, 2);
	expect_changed_consume_exact(qCarrot, {eCarrotA, eCarrotB});
	expect_changed_probe_state(qSalad, 2);
	expect_changed_consume_exact(qSalad, {eSaladA, eSaladB});

	wld.set<Position>(eCarrotA) = {10, 0, 0};

	expect_changed_probe_state(qCarrot, 2);
	expect_changed_consume_exact(qCarrot, {eCarrotA, eCarrotB});
	expect_changed_probe_state(qSalad, 0);
	expect_changed_consume_exact(qSalad, {});

	wld.set<Position>(eSaladB) = {20, 0, 0};

	expect_changed_probe_state(qCarrot, 0);
	expect_changed_consume_exact(qCarrot, {});
	expect_changed_probe_state(qSalad, 2);
	expect_changed_consume_exact(qSalad, {eSaladA, eSaladB});
}

TEST_CASE("Query Filter - cached changed traversed source queries with instance-local var bindings") {
	TestWorld twld;
	struct Status {
		int value;
	};

	const auto rootEarth = wld.add();
	const auto rootMars = wld.add();
	wld.add<Acceleration>(rootEarth);
	wld.add<Acceleration>(rootMars);

	const auto parentEarth = wld.add();
	const auto parentMars = wld.add();
	wld.child(parentEarth, rootEarth);
	wld.child(parentMars, rootMars);

	const auto childEarth = wld.add();
	const auto childMars = wld.add();
	wld.add<Status>(childEarth, {1});
	wld.add<Status>(childMars, {2});
	wld.child(childEarth, parentEarth);
	wld.child(childMars, parentMars);

	auto makeQuery = [&] {
		return wld.query()
				.template all<Status>()
				.all(ecs::Pair(ecs::ChildOf, ecs::Var0))
				.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
				.template changed<Status>();
	};

	auto qEarth = makeQuery();
	auto qMars = makeQuery();

	qEarth.set_var(ecs::Var0, parentEarth);
	qMars.set_var(ecs::Var0, parentMars);

	CHECK(qEarth.id() == qMars.id());
	CHECK(qEarth.gen() == qMars.gen());

	expect_changed_probe_state(qEarth, 1);
	expect_changed_consume_exact(qEarth, {childEarth});
	expect_changed_probe_state(qMars, 1);
	expect_changed_consume_exact(qMars, {childMars});

	wld.set<Status>(childEarth) = {3};

	expect_changed_probe_state(qEarth, 1);
	expect_changed_consume_exact(qEarth, {childEarth});
	expect_changed_probe_state(qMars, 0);
	expect_changed_consume_exact(qMars, {});

	wld.set<Status>(childMars) = {4};

	expect_changed_probe_state(qEarth, 0);
	expect_changed_consume_exact(qEarth, {});
	expect_changed_probe_state(qMars, 1);
	expect_changed_consume_exact(qMars, {childMars});
}

TEST_CASE("Query Filter - systems") {
	uint32_t expectedCnt = 0;
	uint32_t actualCnt = 0;
	uint32_t wsCnt = 0;
	uint32_t wssCnt = 0;
	TestWorld twld;

	auto e = wld.add();
	wld.add<Position>(e);

	// WriterSystem
	auto ws = wld.system()
								.name("WriterSystem")
								.no<ecs::System_>()
								.all<Position&>()
								.on_each([&](Position& a) {
									++wsCnt;
									(void)a;
								})
								.entity();
	// WriterSystemSilent
	auto wss = wld.system()
								 .name("WriterSystemSilent")
								 .no<ecs::System_>()
								 .all<Position&>()
								 .on_each([&](ecs::Iter& it) {
									 ++wssCnt;
									 auto posRWView = it.sview_mut<Position>();
									 (void)posRWView;
								 })
								 .entity();
	// ReaderSystem
	auto rs = wld.system()
								.name("ReaderSystem")
								.no<ecs::System_>()
								.all<Position>()
								.changed<Position>()
								.on_each([&](ecs::Iter& it) {
									GAIA_EACH(it)++ actualCnt;
								})
								.entity();
	(void)rs;

	// first run always happens
	{
		wld.enable(ws, false);
		wld.enable(wss, false);
		CHECK_FALSE(wld.enabled(ws));
		CHECK_FALSE(wld.enabled(wss));
		CHECK(wld.enabled(rs));

		expectedCnt = 1;
		actualCnt = 0;
		wsCnt = 0;
		wssCnt = 0;
		wld.update();

		CHECK_FALSE(wld.enabled(ws));
		CHECK_FALSE(wld.enabled(wss));
		CHECK(wld.enabled(rs));

		CHECK(actualCnt == expectedCnt);
		CHECK(wsCnt == 0);
		CHECK(wssCnt == 0);
	}
	// no change of position so ReaderSystem should't see any changes
	{
		expectedCnt = 0;
		actualCnt = 0;
		wsCnt = 0;
		wssCnt = 0;
		wld.update();

		CHECK_FALSE(wld.enabled(ws));
		CHECK_FALSE(wld.enabled(wss));
		CHECK(wld.enabled(rs));

		CHECK(actualCnt == expectedCnt);
		CHECK(wsCnt == 0);
		CHECK(wssCnt == 0);
	}
	// update position so ReaderSystem should detect a change
	{
		wld.enable(ws, true);
		CHECK(wld.enabled(ws));
		CHECK_FALSE(wld.enabled(wss));
		CHECK(wld.enabled(rs));

		expectedCnt = 1;
		actualCnt = 0;
		wsCnt = 0;
		wssCnt = 0;
		wld.update();

		CHECK(wld.enabled(ws));
		CHECK_FALSE(wld.enabled(wss));
		CHECK(wld.enabled(rs));

		CHECK(actualCnt == expectedCnt);
		CHECK(wsCnt > 0);
		CHECK(wssCnt == 0);
	}
	// no change of position so ReaderSystem shouldn't see any changes
	{
		wld.enable(ws, false);
		CHECK_FALSE(wld.enabled(ws));
		CHECK_FALSE(wld.enabled(wss));
		CHECK(wld.enabled(rs));

		expectedCnt = 0;
		actualCnt = 0;
		wsCnt = 0;
		wssCnt = 0;
		wld.update();

		CHECK_FALSE(wld.enabled(ws));
		CHECK_FALSE(wld.enabled(wss));
		CHECK(wld.enabled(rs));

		CHECK(actualCnt == expectedCnt);
		CHECK(wsCnt == 0);
		CHECK(wssCnt == 0);
	}
	// silent writer enabled again. If should not trigger an update
	{
		wld.enable(ws, false);
		wld.enable(wss, true);
		CHECK_FALSE(wld.enabled(ws));
		CHECK(wld.enabled(wss));
		CHECK(wld.enabled(rs));

		expectedCnt = 0;
		actualCnt = 0;
		wsCnt = 0;
		wssCnt = 0;
		wld.update();

		CHECK_FALSE(wld.enabled(ws));
		CHECK(wld.enabled(wss));
		CHECK(wld.enabled(rs));

		CHECK(actualCnt == expectedCnt);
		CHECK(wsCnt == 0);
		CHECK(wssCnt > 0);
	}
}

struct Eats {};
struct Healthy {};
static uint32_t g_query_sort_cmp_cnt = 0;
static int compare_position_counted([[maybe_unused]] const ecs::World& world, const void* pData0, const void* pData1) {
	++g_query_sort_cmp_cnt;

	const auto& p0 = *static_cast<const Position*>(pData0);
	const auto& p1 = *static_cast<const Position*>(pData1);
	if (p0.x < p1.x)
		return -1;
	if (p0.x > p1.x)
		return 1;
	return 0;
}
ecs::GroupId
group_by_rel([[maybe_unused]] const ecs::World& world, const ecs::Archetype& archetype, ecs::Entity groupBy) {
	if (archetype.pairs() > 0) {
		auto ids = archetype.ids_view();
		for (auto id: ids) {
			if (!id.pair() || id.id() != groupBy.id())
				continue;

			// Consider the pair's target the groupId
			return id.gen();
		}
	}

	// No group
	return 0;
}

TEST_CASE("Query - group") {
	TestWorld twld;

	ecs::Entity eats = wld.add(); // 16
	ecs::Entity carrot = wld.add(); // 17
	ecs::Entity salad = wld.add(); // 18
	ecs::Entity apple = wld.add(); // 19

	ecs::Entity ents[6];
	GAIA_FOR(6) ents[i] = wld.add(); // 20, 21, 22, 23, 24, 25
	{
		// 26 - Position
		// 27 - Healthy
		wld.build(ents[0]).add<Position>().add({eats, salad}); // 20 <-- Pos, {Eats,Salad}
		wld.build(ents[1]).add<Position>().add({eats, carrot});
		wld.build(ents[2]).add<Position>().add({eats, apple});

		wld.build(ents[3]).add<Position>().add({eats, apple}).add<Healthy>();
		wld.build(ents[4]).add<Position>().add({eats, salad}).add<Healthy>();
		wld.build(ents[5]).add<Position>().add({eats, carrot}).add<Healthy>();
	}
	// This query is going to group entities by what they eat.
	// The query cache is going to contain following 6 archetypes in 3 groups as follows:
	//  - Eats:carrot:
	//     - Position, (Eats, carrot)
	//     - Position, (Eats, carrot), Healthy
	//  - Eats:salad:
	//     - Position, (Eats, salad)
	//     - Position, (Eats, salad), Healthy
	//  - Eats::apple:
	//     - Position, (Eats, apple)
	//     - Position, (Eats, apple), Healthy
	ecs::Entity ents_expected[] = {ents[1], ents[5], // carrot, 21, 25
																 ents[0], ents[4], // salad, 20, 24
																 ents[2], ents[3]}; // apple, 22, 23

	auto checkQuery = [&](ecs::Query& q, //
												std::span<ecs::Entity> ents_expected_view) {
		{
			uint32_t j = 0;
			q.each([&](ecs::Iter& it) {
				auto ents = it.view<ecs::Entity>();
				GAIA_EACH(it) {
					const auto e = ents[i];
					const auto e_wanted = ents_expected_view[j++];
					CHECK(e == e_wanted);
				}
			});
			CHECK(j == (uint32_t)ents_expected_view.size());
		}
		{
			uint32_t j = 0;
			q.each([&](ecs::Entity e) {
				const auto e_wanted = ents_expected_view[j++];
				CHECK(e == e_wanted);
			});
			CHECK(j == (uint32_t)ents_expected_view.size());
		}
	};

	{
		auto qq = wld.query().all<Position>().group_by(eats);

		// Grouping on, no group enforced
		checkQuery(qq, {&ents_expected[0], 6});
		// Grouping on, a group is enforced
		qq.group_id(carrot);
		checkQuery(qq, {&ents_expected[0], 2});
		qq.group_id(salad);
		checkQuery(qq, {&ents_expected[2], 2});
		qq.group_id(apple);
		checkQuery(qq, {&ents_expected[4], 2});
	}

	{
		auto qq = wld.query().all<Position>().group_by(eats, group_by_rel);

		// Grouping on, no group enforced
		checkQuery(qq, {&ents_expected[0], 6});
		// Grouping on, a group is enforced
		qq.group_id(carrot);
		checkQuery(qq, {&ents_expected[0], 2});
		qq.group_id(salad);
		checkQuery(qq, {&ents_expected[2], 2});
		qq.group_id(apple);
		checkQuery(qq, {&ents_expected[4], 2});
	}
}

TEST_CASE("Query - sort") {
	TestWorld twld;

	ecs::Entity e0 = wld.add();
	ecs::Entity e1 = wld.add();
	ecs::Entity e2 = wld.add();
	ecs::Entity e3 = wld.add();

	wld.add<Position>(e0, {2, 0, 0});
	wld.add<Position>(e1, {4, 0, 0});
	wld.add<Position>(e2, {1, 0, 0});
	wld.add<Position>(e3, {3, 0, 0});

	SUBCASE("By entity index") {
		auto q = wld.query().all<Position>().sort_by(
				ecs::EntityBad, []([[maybe_unused]] const ecs::World& world, const void* pData0, const void* pData1) {
					const auto& e0 = *static_cast<const ecs::Entity*>(pData0);
					const auto& e1 = *static_cast<const ecs::Entity*>(pData1);
					return (int)e0.id() - (int)e1.id();
				});
		q.each([&](ecs::Iter& it) {
			auto ents = it.view<ecs::Entity>();
			CHECK(ents[0] == e0);
			CHECK(ents[1] == e1);
			CHECK(ents[2] == e2);
			CHECK(ents[3] == e3);
		});
	}

	SUBCASE("By component value (1)") {
		auto q = wld.query().all<Position>().sort_by<Position>(
				[]([[maybe_unused]] const ecs::World& world, const void* pData0, const void* pData1) {
					const auto& p0 = *static_cast<const Position*>(pData0);
					const auto& p1 = *static_cast<const Position*>(pData1);
					const float diff = p0.x - p1.x;
					if (diff < 0.f)
						return -1;
					if (diff > 0.f)
						return 1;
					return 0;
				});
		q.each([&](ecs::Iter& it) {
			auto ents = it.view<ecs::Entity>();
			CHECK(ents[0] == e2);
			CHECK(ents[1] == e0);
			CHECK(ents[2] == e3);
			CHECK(ents[3] == e1);
		});
	}

	SUBCASE("By component value (2)") {
		auto q = wld.query().all<Position>().sort_by(
				wld.get<Position>(), //
				[]([[maybe_unused]] const ecs::World& world, const void* pData0, const void* pData1) {
					const auto& p0 = *static_cast<const Position*>(pData0);
					const auto& p1 = *static_cast<const Position*>(pData1);
					const float diff = p0.x - p1.x;
					if (diff < 0.f)
						return -1;
					if (diff > 0.f)
						return 1;
					return 0;
				});
		q.each([&](ecs::Iter& it) {
			auto ents = it.view<ecs::Entity>();
			CHECK(ents[0] == e2);
			CHECK(ents[1] == e0);
			CHECK(ents[2] == e3);
			CHECK(ents[3] == e1);
		});

		cnt::darr<ecs::Entity> tmp;

		// Change some archetype
		{
			wld.add<Something>(e0, {false});
			q.each([&tmp](ecs::Iter& it) {
				auto ents = it.view<ecs::Entity>();
				GAIA_EACH(ents) tmp.push_back(ents[i]);
			});

			CHECK(tmp[0] == e2);
			CHECK(tmp[1] == e0);
			CHECK(tmp[2] == e3);
			CHECK(tmp[3] == e1);
		}

		// Add new entity
		auto e4 = wld.add();
		{
			wld.add<Position>(e4, {0, 0, 0});
			tmp.clear();
			q.each([&tmp](ecs::Iter& it) {
				auto ents = it.view<ecs::Entity>();
				GAIA_EACH(ents) tmp.push_back(ents[i]);
			});

			CHECK(tmp[0] == e4);
			CHECK(tmp[1] == e2);
			CHECK(tmp[2] == e0);
			CHECK(tmp[3] == e3);
			CHECK(tmp[4] == e1);
		}

		// Delete entity
		{
			wld.del(e0);
			tmp.clear();
			q.each([&tmp](ecs::Iter& it) {
				auto ents = it.view<ecs::Entity>();
				GAIA_EACH(ents) tmp.push_back(ents[i]);
			});
			CHECK(tmp[0] == e4);
			CHECK(tmp[1] == e2);
			CHECK(tmp[2] == e3);
			CHECK(tmp[3] == e1);
		}
	}

	SUBCASE("Doesn't resort after unrelated component write") {
		wld.add<Something>(e0, {false});
		wld.add<Something>(e1, {false});
		wld.add<Something>(e2, {false});
		wld.add<Something>(e3, {false});

		g_query_sort_cmp_cnt = 0;
		auto q = wld.query().all<Position>().all<Something>().sort_by<Position>(compare_position_counted);

		q.each([](ecs::Iter&) {});
		CHECK(g_query_sort_cmp_cnt > 0);

		g_query_sort_cmp_cnt = 0;
		wld.set<Something>(e0).value = true;
		q.each([](ecs::Iter&) {});
		CHECK(g_query_sort_cmp_cnt == 0);
	}

	SUBCASE("Resorts after entity order changes") {
		wld.add<Something>(e0, {false});
		wld.add<Something>(e1, {false});
		wld.add<Something>(e2, {false});
		wld.add<Something>(e3, {false});

		g_query_sort_cmp_cnt = 0;
		auto q = wld.query().all<Position>().all<Something>().sort_by<Position>(compare_position_counted);

		q.each([](ecs::Iter&) {});
		g_query_sort_cmp_cnt = 0;

		auto e4 = wld.add();
		wld.add<Position>(e4, {0, 0, 0});
		wld.add<Something>(e4, {true});
		q.each([](ecs::Iter&) {});
		CHECK(g_query_sort_cmp_cnt > 0);
	}
}
