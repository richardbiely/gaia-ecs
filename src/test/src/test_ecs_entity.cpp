#include "test_common.h"

// ECS
//-----------------------------------------------------------------

GAIA_GCC_WARNING_PUSH()
GAIA_GCC_WARNING_DISABLE("-Wmissing-field-initializers")
GAIA_CLANG_WARNING_PUSH()
GAIA_CLANG_WARNING_DISABLE("-Wmissing-field-initializers")

template <typename T>
void TestDataLayoutAoS() {
	constexpr uint32_t N = 100;
	cnt::sarray<T, N> data;

	GAIA_FOR(N) {
		const auto f = (float)(i + 1);
		data[i] = {f, f, f};

		auto val = data[i];
		CHECK(val.x == f);
		CHECK(val.y == f);
		CHECK(val.z == f);
	}

	SUBCASE("Make sure that all values are correct (e.g. that they were not overriden by one of the loop iteration)") {
		GAIA_FOR(N) {
			const auto f = (float)(i + 1);

			auto val = data[i];
			CHECK(val.x == f);
			CHECK(val.y == f);
			CHECK(val.z == f);
		}
	}
}

TEST_CASE("DataLayout AoS") {
	TestDataLayoutAoS<Position>();
	TestDataLayoutAoS<Rotation>();
}

template <typename T>
void TestDataLayoutSoA() {
	constexpr uint32_t N = 100;
	auto test = [](const auto& data, auto* f, uint32_t i) {
		T val = data[i];
		CHECK(val.x == f[0]);
		CHECK(val.y == f[1]);
		CHECK(val.z == f[2]);

		const float ff[] = {data.template view<0>()[i], data.template view<1>()[i], data.template view<2>()[i]};
		CHECK(ff[0] == f[0]);
		CHECK(ff[1] == f[1]);
		CHECK(ff[2] == f[2]);
	};

	{
		cnt::sarray_soa<T, N> data;

		GAIA_FOR(N) {
			const float f[] = {(float)(i + 1), (float)(i + 2), (float)(i + 3)};
			data[i] = {f[0], f[1], f[2]};
			test(data, f, i);
		}

		{
			uint32_t i = 0;
			for (const auto& val: std::as_const(data)) {
				const float f[] = {(float)(i + 1), (float)(i + 2), (float)(i + 3)};
				CHECK(val.x == f[0]);
				CHECK(val.y == f[1]);
				CHECK(val.z == f[2]);
				++i;
			}
		}

		// Make sure that all values are correct (e.g. that they were not overriden by one of the loop iteration)
		GAIA_FOR(N) {
			const float f[] = {(float)(i + 1), (float)(i + 2), (float)(i + 3)};
			test(data, f, i);
		}
	}
	{
		cnt::darray_soa<T> data;

		GAIA_FOR(N) {
			const float f[] = {(float)(i + 1), (float)(i + 2), (float)(i + 3)};
			data.push_back({f[0], f[1], f[2]});
			test(data, f, i);
		}

		{
			uint32_t i = 0;
			for (const auto& val: std::as_const(data)) {
				const float f[] = {(float)(i + 1), (float)(i + 2), (float)(i + 3)};
				CHECK(val.x == f[0]);
				CHECK(val.y == f[1]);
				CHECK(val.z == f[2]);
				++i;
			}
		}

		// Make sure that all values are correct (e.g. that they were not overriden by one of the loop iteration)
		GAIA_FOR(N) {
			const float f[] = {(float)(i + 1), (float)(i + 2), (float)(i + 3)};
			test(data, f, i);
		}
	}
}

template <>
void TestDataLayoutSoA<DummySoA>() {
	constexpr uint32_t N = 100;
	auto test = [](const auto& data, auto* f, uint32_t i) {
		DummySoA val = data[i];
		CHECK(val.x == f[0]);
		CHECK(val.y == f[1]);
		CHECK(val.b == true);
		CHECK(val.w == f[2]);

		const float ff[] = {data.template view<0>()[i], data.template view<1>()[i], data.template view<3>()[i]};
		const bool b = data.template view<2>()[i];
		CHECK(ff[0] == f[0]);
		CHECK(ff[1] == f[1]);
		CHECK(b == true);
		CHECK(ff[2] == f[2]);
	};

	{
		cnt::sarray_soa<DummySoA, N> data{};

		GAIA_FOR(N) {
			float f[] = {(float)(i + 1), (float)(i + 2), (float)(i + 3)};
			data[i] = {f[0], f[1], true, f[2]};
			test(data, f, i);
		}

		{
			uint32_t i = 0;
			for (const auto& val: std::as_const(data)) {
				const float f[] = {(float)(i + 1), (float)(i + 2), (float)(i + 3)};
				const bool b = data.template view<2>()[i];
				CHECK(val.x == f[0]);
				CHECK(val.y == f[1]);
				CHECK(val.b == b);
				CHECK(val.w == f[2]);
				++i;
			}
		}

		// Make sure that all values are correct (e.g. that they were not overriden by one of the loop iteration)
		GAIA_FOR(N) {
			const float f[] = {(float)(i + 1), (float)(i + 2), (float)(i + 3)};
			test(data, f, i);
		}
	}
	{
		cnt::darray_soa<DummySoA> data;

		GAIA_FOR(N) {
			float f[] = {(float)(i + 1), (float)(i + 2), (float)(i + 3)};
			data.push_back({f[0], f[1], true, f[2]});
			test(data, f, i);
		}

		{
			uint32_t i = 0;
			for (const auto& val: std::as_const(data)) {
				const float f[] = {(float)(i + 1), (float)(i + 2), (float)(i + 3)};
				const bool b = data.template view<2>()[i];
				CHECK(val.x == f[0]);
				CHECK(val.y == f[1]);
				CHECK(val.b == b);
				CHECK(val.w == f[2]);
				++i;
			}
		}

		// Make sure that all values are correct (e.g. that they were not overriden by one of the loop iteration)
		GAIA_FOR(N) {
			const float f[] = {(float)(i + 1), (float)(i + 2), (float)(i + 3)};
			test(data, f, i);
		}
	}
}

TEST_CASE("DataLayout SoA") {
	TestDataLayoutSoA<PositionSoA>();
	TestDataLayoutSoA<RotationSoA>();
	TestDataLayoutSoA<DummySoA>();
}

TEST_CASE("DataLayout SoA8") {
	TestDataLayoutSoA<PositionSoA8>();
	TestDataLayoutSoA<RotationSoA8>();
}

TEST_CASE("DataLayout SoA16") {
	TestDataLayoutSoA<PositionSoA16>();
	TestDataLayoutSoA<RotationSoA16>();
}

GAIA_CLANG_WARNING_POP()
GAIA_GCC_WARNING_POP()

TEST_CASE("Entity - valid/has") {
	TestWorld twld;

	auto e = wld.add();
	CHECK(wld.valid(e));
	CHECK(wld.has(e));

	wld.del(e);
	CHECK_FALSE(wld.valid(e));
	CHECK_FALSE(wld.has(e));
}

TEST_CASE("Entity - EntityBad") {
	CHECK(ecs::Entity{} == ecs::EntityBad);

	TestWorld twld;
	CHECK_FALSE(wld.valid(ecs::EntityBad));
	CHECK_FALSE(wld.has(ecs::EntityBad));

	auto e = wld.add();
	CHECK(e != ecs::EntityBad);
	CHECK_FALSE(e == ecs::EntityBad);
	CHECK(e.entity());
}

TEST_CASE("Entity copy") {
	TestWorld twld;

	auto e1 = wld.add();
	auto e2 = wld.add();
	wld.add(e1, e2);
	auto e3 = wld.copy(e1);

	CHECK(wld.has(e3, e2));
}

TEST_CASE("Entity - exact has/get across archetype moves") {
	TestWorld twld;

	auto rel = wld.add();
	auto tgt = wld.add();
	auto e = wld.add();

	wld.add<Position>(e, {1, 2, 3});
	wld.add(e, ecs::Pair(rel, tgt));

	CHECK(wld.has<Position>(e));
	CHECK(wld.has(e, ecs::Pair(rel, tgt)));
	{
		const auto& p = wld.get<Position>(e);
		CHECK(p.x == 1);
		CHECK(p.y == 2);
		CHECK(p.z == 3);
	}

	wld.add<Rotation>(e, {4, 5, 6, 0});

	CHECK(wld.has<Position>(e));
	CHECK(wld.has<Rotation>(e));
	CHECK(wld.has(e, ecs::Pair(rel, tgt)));
	{
		const auto& p = wld.get<Position>(e);
		const auto& r = wld.get<Rotation>(e);
		CHECK(p.x == 1);
		CHECK(p.y == 2);
		CHECK(p.z == 3);
		CHECK(r.x == 4);
		CHECK(r.y == 5);
		CHECK(r.z == 6);
	}

	wld.del<Position>(e);

	CHECK_FALSE(wld.has<Position>(e));
	CHECK(wld.has<Rotation>(e));
	CHECK(wld.has(e, ecs::Pair(rel, tgt)));
	{
		const auto& r = wld.get<Rotation>(e);
		CHECK(r.x == 4);
		CHECK(r.y == 5);
		CHECK(r.z == 6);
	}
}

#if GAIA_USE_SAFE_ENTITY
TEST_CASE("Entity safe") {
	SUBCASE("safe, no delete") {
		TestWorld twld;

		auto e = wld.add();
		CHECK(wld.valid(e));
		CHECK(wld.has(e));

		{
			auto se = ecs::SafeEntity(wld, e);
			CHECK(wld.valid(se));
			CHECK(wld.has(se));
			CHECK(wld.valid(e));
			CHECK(wld.has(e));
		}

		// SafeEntity went out of scope but we didn't request delete.
		// The entity needs to stay alive.
		CHECK(wld.valid(e));
		CHECK(wld.has(e));
	}

	SUBCASE("safe with delete") {
		TestWorld twld;

		auto e = wld.add();
		CHECK(wld.valid(e));
		CHECK(wld.has(e));

		{
			auto se = ecs::SafeEntity(wld, e);
			CHECK(se == e);

			CHECK(wld.valid(se));
			CHECK(wld.has(se));
			CHECK(wld.valid(e));
			CHECK(wld.has(e));

			// We can call del as many times as we want. So long a SafeEntity is around,
			// we won't be able to delete the entity.
			GAIA_FOR(10) {
				wld.del(e);
				CHECK(wld.valid(e));
				CHECK(wld.has(e));
			}
		}

		// SafeEntity went out of scope and we did request delete.
		// The entity needs to be dead now.
		CHECK_FALSE(wld.valid(e));
		CHECK_FALSE(wld.has(e));
	}

	SUBCASE("many safes with delete") {
		TestWorld twld;

		auto e = wld.add();
		CHECK(wld.valid(e));
		CHECK(wld.has(e));

		{
			auto se0 = ecs::SafeEntity(wld, e);

			{
				auto se = ecs::SafeEntity(wld, e);
				CHECK(se == e);
				CHECK(se == se0);

				CHECK(wld.valid(se));
				CHECK(wld.has(se));
				CHECK(wld.valid(e));
				CHECK(wld.has(e));

				// We can call del as many times as we want. So long a SafeEntity is around,
				// we won't be able to delete the entity.
				GAIA_FOR(10) {
					wld.del(e);
					CHECK(wld.valid(e));
					CHECK(wld.has(e));
				}
			}

			// SafeEntity went out of scope and we did request delete.
			// However, se0 is still in scope, so no delete should happen no matter
			// how many times we try.
			GAIA_FOR(10) {
				wld.del(e);
				CHECK(wld.valid(e));
				CHECK(wld.has(e));
			}
		}

		// SafeEntity went out of scope and we did request delete.
		// The entity needs to be dead now.
		CHECK_FALSE(wld.valid(e));
		CHECK_FALSE(wld.has(e));
	}

	SUBCASE("component") {
		TestWorld twld;

		struct SafeComponent {
			ecs::SafeEntity entity;
		};

		auto e = wld.add();

		auto e2 = wld.add();
		wld.add<SafeComponent>(e2, {ecs::SafeEntity(wld, e)});

		const auto& sc = wld.get<SafeComponent>(e2);
		CHECK(sc.entity == e);

		CHECK(wld.valid(e));
		CHECK(wld.has(e));

		// Nothings gets deleted because the reference is held in the component
		wld.del(e);
		CHECK(wld.valid(e));
		CHECK(wld.has(e));

		// Delete the entity holding the ref
		wld.del(e2);
		CHECK_FALSE(wld.valid(e));
		CHECK_FALSE(wld.has(e));
	}

	SUBCASE("component survives world cleanup and destruction") {
		struct SafeComponent {
			ecs::SafeEntity entity;
		};

		// 0 = live target, 1 = deleted target, 2 = ChildOf child of a deleted parent
		auto setup = [](ecs::World& w, uint32_t targetState) {
			auto e = w.add();
			auto holder = w.add();
			w.add<SafeComponent>(holder, {ecs::SafeEntity(w, e)});
			if (targetState == 1)
				w.del(e);
			else if (targetState == 2) {
				auto parent = w.add();
				w.child(e, parent);
				w.del(parent);
			}
			w.update();
		};

		GAIA_FOR(3) {
			// World destruction
			{
				ecs::World w;
				setup(w, i);
			}

			// World cleanup, then the world stays usable
			{
				ecs::World w;
				setup(w, i);
				w.cleanup();

				auto e = w.add();
				auto holder = w.add();
				w.add<SafeComponent>(holder, {ecs::SafeEntity(w, e)});
				w.del(e);
				CHECK(w.valid(e));
				w.del(holder);
				w.update();
				CHECK_FALSE(w.valid(e));
			}
		}
	}

	SUBCASE("copy move serialize and compare helpers") {
		TestWorld twld;

		auto e = wld.add();
		CHECK(wld.valid(e));

		{
			ecs::SafeEntity base(wld, e);
			ecs::SafeEntity copied(base);
			ecs::SafeEntity assigned;
			assigned = base;
			ecs::SafeEntity movedCtor(GAIA_MOV(copied));
			ecs::SafeEntity movedAssign;
			movedAssign = GAIA_MOV(assigned);

			CHECK(base == movedCtor);
			CHECK(base == movedAssign);
			CHECK(e == base);
			CHECK(e == movedCtor);
			CHECK_FALSE(base != e);
			CHECK_FALSE(e != movedAssign);
			CHECK((ecs::Entity)copied == ecs::EntityBad);
			CHECK((ecs::Entity)assigned == ecs::EntityBad);

			ser::bin_stream stream;
			auto ctx = ser::serializer::bind_ctx(stream);
			ser::serializer s{ctx};
			CHECK(s.valid());
			movedAssign.save(s);
			s.seek(0);

			ecs::SafeEntity loaded;
			loaded.load(s);
			CHECK((ecs::Entity)loaded == e);

			wld.del(e);
			CHECK(wld.valid(e));
		}

		CHECK_FALSE(wld.valid(e));
	}

	SUBCASE("reassignment releases the previous reference") {
		TestWorld twld;

		auto e0 = wld.add();
		auto e1 = wld.add();
		auto e2 = wld.add();
		{
			ecs::SafeEntity copied(wld, e0);
			ecs::SafeEntity moved(wld, e1);
			ecs::SafeEntity source(wld, e2);
			wld.del(e0);
			wld.del(e1);
			wld.del(e2);
			CHECK(wld.valid(e0));
			CHECK(wld.valid(e1));

			// Each handle held the last reference to its old entity
			copied = source;
			CHECK_FALSE(wld.valid(e0));
			moved = GAIA_MOV(source);
			CHECK_FALSE(wld.valid(e1));
			CHECK(wld.valid(e2));
		}

		CHECK_FALSE(wld.valid(e2));
	}

	SUBCASE("copying a moved-from handle yields an empty handle") {
		TestWorld twld;

		auto e = wld.add();
		auto e2 = wld.add();
		{
			ecs::SafeEntity source(wld, e);
			ecs::SafeEntity owner(GAIA_MOV(source));

			ecs::SafeEntity copied(source);
			CHECK((ecs::Entity)copied == ecs::EntityBad);

			ecs::SafeEntity assigned(wld, e2);
			wld.del(e2);
			assigned = source;
			CHECK((ecs::Entity)assigned == ecs::EntityBad);
			CHECK_FALSE(wld.valid(e2));

			wld.del(e);
			CHECK(wld.valid(e));
		}

		CHECK_FALSE(wld.valid(e));
	}
}
#endif

#if GAIA_USE_WEAK_ENTITY
TEST_CASE("Entity weak") {
	SUBCASE("weak, no delete") {
		TestWorld twld;

		auto e = wld.add();
		CHECK(wld.valid(e));
		CHECK(wld.has(e));

		auto we = ecs::WeakEntity(wld, e);
		CHECK(we == e);

		CHECK(wld.valid(we));
		CHECK(wld.has(we));
		CHECK(wld.valid(e));
		CHECK(wld.has(e));

		wld.del(e);
		CHECK_FALSE(wld.valid(e));
		CHECK_FALSE(wld.has(e));
		CHECK_FALSE(wld.valid(we));
		CHECK_FALSE(wld.has(we));
	}

	SUBCASE("weak with delete") {
		TestWorld twld;

		auto e = wld.add();
		CHECK(wld.valid(e));
		CHECK(wld.has(e));

		auto we = ecs::WeakEntity(wld, e);

		{
			auto se = ecs::SafeEntity(wld, e);
			CHECK(se == e);

			CHECK(wld.valid(we));
			CHECK(wld.has(we));
			CHECK(wld.valid(e));
			CHECK(wld.has(e));

			// We can call del as many times as we want. So long a SafeEntity is around,
			// we won't be able to delete the entity.
			GAIA_FOR(10) {
				wld.del(e);
				CHECK(wld.valid(we));
				CHECK(wld.has(we));
				CHECK(wld.valid(e));
				CHECK(wld.has(e));
			}
		}

		// SafeEntity went out of scope and we did request delete.
		// The entity needs to be dead now.
		CHECK_FALSE(wld.valid(e));
		CHECK_FALSE(wld.has(e));
		CHECK_FALSE(wld.valid(we));
		CHECK_FALSE(wld.has(we));
	}

	SUBCASE("weak, safes with delete") {
		TestWorld twld;

		auto e = wld.add();
		CHECK(wld.valid(e));
		CHECK(wld.has(e));

		auto we = ecs::WeakEntity(wld, e);

		{
			auto se0 = ecs::SafeEntity(wld, e);

			{
				auto se = ecs::SafeEntity(wld, e);
				CHECK(se == e);

				CHECK(wld.valid(we));
				CHECK(wld.has(we));
				CHECK(wld.valid(e));
				CHECK(wld.has(e));

				// We can call del as many times as we want. So long a SafeEntity is around,
				// we won't be able to delete the entity.
				GAIA_FOR(10) {
					wld.del(e);
					CHECK(wld.valid(we));
					CHECK(wld.has(we));
					CHECK(wld.valid(e));
					CHECK(wld.has(e));
				}
			}

			// SafeEntity went out of scope and we did request delete.
			// However, se0 is still in scope, so no delete should happen no matter
			// how many times we try.
			GAIA_FOR(10) {
				wld.del(e);
				CHECK(wld.valid(we));
				CHECK(wld.has(we));
				CHECK(wld.valid(e));
				CHECK(wld.has(e));
			}
		}

		// SafeEntity went out of scope and we did request delete.
		// The entity needs to be dead now.
		CHECK_FALSE(wld.valid(we));
		CHECK_FALSE(wld.has(we));
		CHECK_FALSE(wld.valid(e));
		CHECK_FALSE(wld.has(e));
	}

	SUBCASE("component") {
		TestWorld twld;

		struct WeakComponent {
			ecs::WeakEntity entity;
		};

		auto e = wld.add();

		auto e2 = wld.add();
		wld.add<WeakComponent>(e2, {ecs::WeakEntity(wld, e)});

		const auto& wc = wld.get<WeakComponent>(e2);
		CHECK(wc.entity == e);

		CHECK(wld.valid(wc.entity));
		CHECK(wld.has(wc.entity));

		wld.del(e);
		CHECK_FALSE(wld.valid(wc.entity));
		CHECK_FALSE(wld.has(wc.entity));
	}

	SUBCASE("copy assignment replaces previous tracking") {
		TestWorld twld;

		auto e0 = wld.add();
		auto e1 = wld.add();
		auto dst = ecs::WeakEntity(wld, e0);
		auto src = ecs::WeakEntity(wld, e1);

		dst = src;
		CHECK(dst == e1);
		CHECK(src == e1);

		wld.del(e0);
		CHECK(dst == e1);
		CHECK(src == e1);

		wld.del(e1);
		CHECK(dst == ecs::EntityBad);
		CHECK(src == ecs::EntityBad);
	}

	SUBCASE("move assignment transfers tracking and releases previous one") {
		TestWorld twld;

		auto e0 = wld.add();
		auto e1 = wld.add();
		auto dst = ecs::WeakEntity(wld, e0);
		auto src = ecs::WeakEntity(wld, e1);

		dst = GAIA_MOV(src);
		CHECK(dst == e1);
		CHECK(src == ecs::EntityBad);

		wld.del(e0);
		CHECK(dst == e1);

		wld.del(e1);
		CHECK(dst == ecs::EntityBad);
	}

	SUBCASE("remaining weak handles still invalidate after another handle is reset") {
		TestWorld twld;

		auto e = wld.add();
		auto first = ecs::WeakEntity(wld, e);
		auto second = ecs::WeakEntity(wld, e);

		first = ecs::WeakEntity();
		CHECK(first == ecs::EntityBad);
		CHECK(second == e);

		wld.del(e);
		CHECK(second == ecs::EntityBad);
	}

	SUBCASE("copy move serialize and compare helpers") {
		TestWorld twld;

		auto e = wld.add();
		ecs::WeakEntity base(wld, e);
		ecs::WeakEntity copied(base);
		ecs::WeakEntity assigned;
		assigned = base;
		ecs::WeakEntity movedCtor(GAIA_MOV(copied));
		ecs::WeakEntity movedAssign;
		movedAssign = GAIA_MOV(assigned);

		CHECK(base == movedCtor);
		CHECK(base == movedAssign);
		CHECK(e == base);
		CHECK(e == movedCtor);
		CHECK_FALSE(base != e);
		CHECK_FALSE(e != movedAssign);
		CHECK(copied == ecs::EntityBad);
		CHECK(assigned == ecs::EntityBad);

		ser::bin_stream stream;
		auto ctx = ser::serializer::bind_ctx(stream);
		ser::serializer s{ctx};
		CHECK(s.valid());
		movedAssign.save(s);
		s.seek(0);

		ecs::WeakEntity loaded(wld, e);
		loaded.load(s);
		CHECK(loaded == e);

		wld.del(e);
		CHECK(base == ecs::EntityBad);
		CHECK(movedCtor == ecs::EntityBad);
		CHECK(movedAssign == ecs::EntityBad);
		CHECK(loaded == ecs::EntityBad);
	}

	SUBCASE("stress random copy move delete sequences") {
		TestWorld twld;

		constexpr uint32_t Iterations = 20'000;
		constexpr uint32_t WeakSlots = 128;
		constexpr uint32_t MaxLiveEntities = 256;

		cnt::darr<ecs::WeakEntity> weaks;
		weaks.resize(WeakSlots);

		cnt::darr<ecs::Entity> live;
		live.reserve(MaxLiveEntities);

		auto verify = [&] {
			for (const auto& weak: weaks) {
				const auto e = weak.entity();
				if (e == ecs::EntityBad) {
					CHECK_FALSE(wld.valid(e));
					continue;
				}

				CHECK(wld.valid(e));
				CHECK(wld.has(e));
			}
		};

		rnd::pseudo_random rng(0xC0FFEEu);

		for (uint32_t it = 0; it < Iterations; ++it) {
			const auto op = rng.range(0, 8);
			switch (op) {
				case 0: {
					if (live.size() < MaxLiveEntities)
						live.push_back(wld.add());
					break;
				}
				case 1: {
					if (live.empty())
						break;
					const uint32_t idx = rng.range(0, (uint32_t)live.size() - 1);
					wld.del(live[idx]);
					live[idx] = live.back();
					live.pop_back();
					break;
				}
				case 2: {
					if (live.empty())
						break;
					const uint32_t wi = rng.range(0, WeakSlots - 1);
					const uint32_t ei = rng.range(0, (uint32_t)live.size() - 1);
					weaks[wi] = ecs::WeakEntity(wld, live[ei]);
					break;
				}
				case 3: {
					const uint32_t wi = rng.range(0, WeakSlots - 1);
					weaks[wi] = ecs::WeakEntity();
					break;
				}
				case 4: {
					const uint32_t a = rng.range(0, WeakSlots - 1);
					const uint32_t b = rng.range(0, WeakSlots - 1);
					weaks[a] = weaks[b];
					break;
				}
				case 5: {
					const uint32_t a = rng.range(0, WeakSlots - 1);
					const uint32_t b = rng.range(0, WeakSlots - 1);
					weaks[a] = GAIA_MOV(weaks[b]);
					break;
				}
				case 6: {
					const uint32_t a = rng.range(0, WeakSlots - 1);
					const uint32_t b = rng.range(0, WeakSlots - 1);
					auto tmp = weaks[b];
					weaks[a] = GAIA_MOV(tmp);
					break;
				}
				case 7: {
					if (!live.empty()) {
						const uint32_t idx = rng.range(0, (uint32_t)live.size() - 1);
						wld.del(live[idx]);
						live[idx] = live.back();
						live.pop_back();
					}
					break;
				}
				case 8: {
					if (!live.empty()) {
						const uint32_t wi = rng.range(0, WeakSlots - 1);
						const uint32_t ei = rng.range(0, (uint32_t)live.size() - 1);
						const auto e = live[ei];
						weaks[wi] = ecs::WeakEntity(wld, e);
						wld.del(e);
						live[ei] = live.back();
						live.pop_back();
					}
					break;
				}
				default:
					break;
			}

			if ((it % 127) == 0)
				verify();
		}

		while (!live.empty()) {
			wld.del(live.back());
			live.pop_back();
		}

		verify();
		for (const auto& weak: weaks)
			CHECK(weak == ecs::EntityBad);
	}

	SUBCASE("world teardown with live weak component") {
		struct WeakComponent {
			ecs::WeakEntity entity;
		};

		{
			ecs::World world;
			auto e = world.add();
			auto holder = world.add();
			world.add<WeakComponent>(holder, {ecs::WeakEntity(world, e)});
		}

		CHECK(true);
	}
}
#endif

#if GAIA_USE_WEAK_ENTITY
TEST_CASE("Entity weak - recycled ids do not resurrect stale handles") {
	TestWorld twld;

	const auto original = wld.add();
	const auto weak = ecs::WeakEntity(wld, original);
	const auto originalId = original.id();
	const auto originalGen = original.gen();

	wld.del(original);
	CHECK_FALSE(wld.valid(original));
	CHECK_FALSE(wld.valid(weak));
	CHECK(weak == ecs::EntityBad);

	wld.update();
	const auto recycled = wld.add();
	CHECK(recycled.id() == originalId);
	CHECK(recycled.gen() != originalGen);
	CHECK(recycled != original);
	CHECK(wld.valid(recycled));
	CHECK(weak == ecs::EntityBad);
}

struct WeakSelfRef {
	ecs::WeakEntity self;
};

TEST_CASE("Entity weak - component referencing its own entity survives deletion") {
	TestWorld twld;

	// Deleting b swap-removes c into b's chunk slot. That move-assign destroys b's self reference
	// after b stopped being valid, while the reference is still the head of b's tracker list.
	const auto a = wld.add();
	const auto b = wld.add();
	const auto c = wld.add();
	// Created before the self references, so each sits behind its self reference in the list.
	const auto watchB = ecs::WeakEntity(wld, b);
	const auto watchC = ecs::WeakEntity(wld, c);
	wld.add<WeakSelfRef>(a, {ecs::WeakEntity(wld, a)});
	wld.add<WeakSelfRef>(b, {ecs::WeakEntity(wld, b)});
	wld.add<WeakSelfRef>(c, {ecs::WeakEntity(wld, c)});
	const auto watchA = ecs::WeakEntity(wld, a);

	wld.del(b);
	CHECK_FALSE(wld.valid(b));
	CHECK(watchB == ecs::EntityBad);
	CHECK(wld.get<WeakSelfRef>(a).self == a);
	CHECK(wld.get<WeakSelfRef>(c).self == c);

	// Coverage for the other destruction path: the last entity of its chunk has its component
	// destroyed rather than overwritten.
	wld.del(c);
	CHECK_FALSE(wld.valid(c));
	CHECK(watchC == ecs::EntityBad);
	wld.update();

	// a's list holds the watcher in front of the self reference; both clear when a goes.
	CHECK(watchA == a);
	wld.del(a);
	CHECK(watchA == ecs::EntityBad);
	wld.update();
}

TEST_CASE("Entity weak - bulk archetype deletion resets weak references") {
	TestWorld twld;

	// Deleting an (OnDelete, Delete) tag deletes whole archetypes at once, bypassing the
	// per-entity deletion path.
	const auto tag = wld.add();
	wld.add(tag, ecs::Pair(ecs::OnDelete, ecs::Delete));
	const auto a = wld.add();
	const auto b = wld.add();
	wld.add(a, tag);
	wld.add(b, tag);
	wld.add<WeakSelfRef>(a, {ecs::WeakEntity(wld, a)});
	wld.add<WeakSelfRef>(b, {ecs::WeakEntity(wld, b)});
	const auto watchA = ecs::WeakEntity(wld, a);
	const auto watchB = ecs::WeakEntity(wld, b);

	// Reset at the deletion request, as on the per-entity path, not only when update() finalizes it.
	wld.del(tag);
	CHECK(watchA == ecs::EntityBad);
	CHECK(watchB == ecs::EntityBad);
	wld.update();
	CHECK_FALSE(wld.valid(a));
	CHECK_FALSE(wld.valid(b));
	CHECK(watchA == ecs::EntityBad);
	CHECK(watchB == ecs::EntityBad);
}

TEST_CASE("Entity weak - non-fragmenting relation cascade resets weak references") {
	// Deleting t cascades to s through an exclusive DontFragment relation, which requests s's
	// deletion without going through the regular per-entity delete.
	ecs::WeakEntity outside;
	{
		ecs::World w;
		const auto r = w.add();
		w.add(r, ecs::Exclusive);
		w.add(r, ecs::DontFragment);
		w.add(r, ecs::Pair(ecs::OnDeleteTarget, ecs::Delete));
		const auto t = w.add();
		const auto s = w.add();
		w.add(s, ecs::Pair(r, t));
		const auto inside = ecs::WeakEntity(w, s);
		outside = ecs::WeakEntity(w, s);

		w.del(t);
		CHECK(inside == ecs::EntityBad);
		GAIA_FOR(200) w.update();
		CHECK_FALSE(w.valid(s));
		CHECK(inside == ecs::EntityBad);
	}
	// s's record was freed before the world died; the outside reference must not touch either.
	CHECK(outside == ecs::EntityBad);
}

TEST_CASE("Entity weak - reference created after a deletion request resets when the record goes") {
	TestWorld twld;

	const auto tag = wld.add();
	const auto e = wld.add();
	wld.add(e, tag);
	wld.del(e);
	// e is delete-requested but its record still exists until update() finalizes the deletion.
	const auto late = ecs::WeakEntity(wld, e);
	twld.update();
	CHECK(late == ecs::EntityBad);
}

TEST_CASE("Entity weak - re-adding a delete-requested pair resets weak references") {
	ecs::WeakEntity outside;
	{
		ecs::World w;
		const auto r = w.add();
		const auto t = w.add();
		const auto x = w.add();
		const auto y = w.add();
		const ecs::Pair pair(r, t);
		w.add(x, pair);
		w.del(pair);
		// The pair record is delete-requested but still allocated, so the references attach to it.
		outside = ecs::WeakEntity(w, pair);
		const auto inside = ecs::WeakEntity(w, pair);

		// Re-adding revives the record as a new incarnation of the pair.
		w.add(y, pair);
		CHECK(w.valid(pair));
		CHECK(inside == ecs::EntityBad);
		CHECK(outside == ecs::EntityBad);
	}
	// Destroying the reference after its world must not touch the dead world.
	CHECK(outside == ecs::EntityBad);
}

TEST_CASE("Entity weak - world cleanup resets weak references") {
	TestWorld twld;

	const auto e = wld.add();
	const auto weak = ecs::WeakEntity(wld, e);

	wld.cleanup();
	CHECK(weak == ecs::EntityBad);

	// Cleanup restarts ids, so a new entity can take e's exact id and generation. The weak
	// reference must not resurrect onto it.
	const auto replacement = wld.add();
	CHECK(weak == ecs::EntityBad);
	CHECK(wld.valid(replacement));
}

TEST_CASE("Entity weak - weak reference outliving its world") {
	ecs::WeakEntity outside;
	{
		ecs::World w;
		const auto e = w.add();
		outside = ecs::WeakEntity(w, e);
		CHECK(outside == e);
	}
	// Destroying the world resets the reference, and destroying the reference later must not
	// touch the dead world.
	CHECK(outside == ecs::EntityBad);
}

	#if GAIA_ASSERT_ENABLED
TEST_CASE("Entity validity distinguishes stale handles from recycled live slots") {
	TestWorld twld;

	const auto original = wld.add();
	const auto originalId = original.id();

	wld.del(original);
	wld.update();

	const auto recycled = wld.add();
	CHECK(recycled.id() == originalId);
	CHECK(recycled.gen() != original.gen());

	CHECK_FALSE(wld.valid(original));
	CHECK(wld.valid(recycled));
	CHECK(wld.try_get(originalId) == recycled);
}
	#endif
#endif

// Deleting an (OnDelete, Delete) tag deletes every archetype holding it at once. Each row must
// still be deleted the way deleting it on its own would.

//! Creates entities until one reuses the id of \a dead. Returns EntityBad if none does.
static ecs::Entity add_until_id_reused(ecs::World& w, ecs::Entity dead) {
	GAIA_FOR(64) {
		const auto e = w.add();
		if (e.id() == dead.id())
			return e;
	}
	return ecs::EntityBad;
}

TEST_CASE("Entity bulk deletion - frees records and recycles ids") {
	TestWorld twld;

	const auto tag = wld.add();
	wld.add(tag, ecs::Pair(ecs::OnDelete, ecs::Delete));
	(void)wld.add<Position>();
	const auto sizeBefore = wld.size();

	cnt::darray<ecs::Entity> rows;
	GAIA_FOR(100) {
		const auto e = wld.add();
		wld.add(e, tag);
		wld.add<Position>(e, {(float)i, 0, 0});
		rows.push_back(e);
	}
	CHECK(wld.size() == sizeBefore + 100);

	wld.del(tag);
	twld.update();
	for (auto e: rows)
		CHECK_FALSE(wld.valid(e));
	CHECK(wld.size() == sizeBefore - 1);

	const auto fresh = add_until_id_reused(wld, rows[0]);
	CHECK(fresh != ecs::EntityBad);
	if (fresh == ecs::EntityBad)
		return;
	CHECK(wld.valid(fresh));
	CHECK_FALSE(wld.has<Position>(fresh));
}

TEST_CASE("Entity bulk deletion - names and aliases are released") {
	TestWorld twld;

	const auto tag = wld.add();
	wld.add(tag, ecs::Pair(ecs::OnDelete, ecs::Delete));
	const auto named = wld.add();
	wld.add(named, tag);
	wld.name(named, "bulk_named");
	const auto aliased = wld.add();
	wld.add(aliased, tag);
	wld.name(aliased, "bulk_aliased");
	wld.alias(aliased, "bulk_alias");

	wld.del(tag);
	CHECK(wld.get("bulk_named") == ecs::EntityBad);
	CHECK(wld.get("bulk_aliased") == ecs::EntityBad);
	CHECK(wld.alias("bulk_alias") == ecs::EntityBad);
	twld.update();
	CHECK_FALSE(wld.valid(named));
	CHECK_FALSE(wld.valid(aliased));

	// The names are free for new entities.
	const auto reuse = wld.add();
	wld.name(reuse, "bulk_named");
	CHECK(wld.get("bulk_named") == reuse);
}

TEST_CASE("Entity deletion - a pair leaves other pairs with its target alone") {
	// Deleting a pair, or its relation, must not apply the target's OnDeleteTarget rules. Those belong
	// to deleting the target itself.
	// how: 0 = del(relation), 1 = del(pair), 2 = a cleanup rule deletes the pairs' own archetype rows
	GAIA_FOR(3) {
		const uint32_t how = i;
		TestWorld twld;

		const auto parent = wld.add();
		const auto child = wld.add();
		wld.child(child, parent);
		const auto rel = wld.add();
		const auto rel2 = wld.add();
		const auto src = wld.add();
		const auto other = wld.add();
		wld.add(src, ecs::Pair(rel, parent));
		wld.add(other, ecs::Pair(rel2, parent));
		// A target without delete rules, whose pairs are removed rather than deleted
		const auto tgt = wld.add();
		wld.add(src, ecs::Pair(rel, tgt));
		wld.add(other, ecs::Pair(rel2, tgt));

		if (how == 0)
			wld.del(rel);
		else if (how == 1) {
			wld.del(ecs::Pair(rel, parent));
			wld.del(ecs::Pair(rel, tgt));
		} else {
			const auto tag = wld.add();
			wld.add(tag, ecs::Pair(ecs::OnDelete, ecs::Delete));
			wld.add(ecs::Pair(rel, parent), tag);
			wld.add(ecs::Pair(rel, tgt), tag);
			wld.del(tag);
		}
		twld.update();

		CHECK(wld.valid(parent));
		CHECK(wld.valid(src));
		CHECK_FALSE(wld.has(src, ecs::Pair(rel, parent)));
		// The ChildOf child and the sibling pair are untouched
		CHECK(wld.valid(child));
		CHECK(wld.has(child, ecs::Pair(ecs::ChildOf, parent)));
		CHECK(wld.valid(other));
		CHECK(wld.has(other, ecs::Pair(rel2, parent)));
		CHECK_FALSE(wld.has(src, ecs::Pair(rel, tgt)));
		CHECK(wld.has(other, ecs::Pair(rel2, tgt)));

		// Deleting the target itself still applies its rules
		wld.del(parent);
		twld.update();
		CHECK_FALSE(wld.valid(child));
		CHECK_FALSE(wld.has(other, ecs::Pair(rel2, parent)));
	}
}

TEST_CASE("Entity deletion - alias is released") {
	TestWorld twld;

	const auto e = wld.add();
	wld.alias(e, "single_alias");
	wld.del(e);
	CHECK(wld.alias("single_alias") == ecs::EntityBad);
	twld.update();
	CHECK(wld.alias("single_alias") == ecs::EntityBad);
}

TEST_CASE("Entity bulk deletion - singleton row") {
	TestWorld twld;

	const auto tag = wld.add();
	wld.add(tag, ecs::Pair(ecs::OnDelete, ecs::Delete));
	const auto e = wld.add();
	wld.add(e, tag);
	wld.add(e, e);
	const auto sizeBefore = wld.size();

	wld.del(tag);
	twld.update();
	twld.update();
	CHECK_FALSE(wld.valid(e));
	CHECK(wld.size() == sizeBefore - 2);
}

TEST_CASE("Entity bulk deletion - pair targets follow OnDeleteTarget") {
	TestWorld twld;

	const auto tag = wld.add();
	wld.add(tag, ecs::Pair(ecs::OnDelete, ecs::Delete));
	const auto likes = wld.add();

	const auto parent = wld.add();
	wld.add(parent, tag);
	const auto child = wld.add();
	wld.add(child, ecs::Pair(ecs::ChildOf, parent));
	const auto fan = wld.add();
	wld.add(fan, ecs::Pair(likes, parent));

	wld.del(tag);
	twld.update();
	twld.update();
	CHECK_FALSE(wld.valid(parent));
	// ChildOf deletes its sources with the target; other relations are removed from them.
	CHECK_FALSE(wld.valid(child));
	CHECK(wld.valid(fan));
	CHECK_FALSE(wld.has(fan, ecs::Pair(likes, ecs::All)));

	// A new entity reusing the parent's id must not inherit its relations.
	const auto fresh = add_until_id_reused(wld, parent);
	CHECK(fresh != ecs::EntityBad);
	if (fresh == ecs::EntityBad)
		return;
	CHECK_FALSE(wld.has(fan, ecs::Pair(likes, fresh)));
	CHECK(wld.query().all(ecs::Pair(likes, fresh)).count() == 0);
}

TEST_CASE("Entity bulk deletion - row used as a relation and as an id") {
	TestWorld twld;

	const auto tag = wld.add();
	wld.add(tag, ecs::Pair(ecs::OnDelete, ecs::Delete));
	const auto rel = wld.add();
	wld.add(rel, tag);
	const auto marker = wld.add();
	wld.add(marker, tag);
	const auto tgt = wld.add();
	const auto user = wld.add();
	wld.add(user, ecs::Pair(rel, tgt));
	wld.add(user, marker);

	wld.del(tag);
	twld.update();
	twld.update();
	CHECK_FALSE(wld.valid(rel));
	CHECK_FALSE(wld.valid(marker));
	CHECK(wld.valid(user));
	CHECK_FALSE(wld.has(user, ecs::Pair(ecs::All, tgt)));
	CHECK(wld.valid(tgt));
}

TEST_CASE("Entity bulk deletion - parent cycle is deleted whole") {
	TestWorld twld;

	// root -ChildOf-> b -Parent-> a -ChildOf-> root. Deleting root reaches a's archetype through the
	// cycle while root itself is still being deleted.
	const auto root = wld.add();
	const auto a = wld.add();
	const auto b = wld.add();
	wld.child(a, root);
	wld.parent(b, a);
	wld.child(root, b);
	const auto sizeBefore = wld.size();

	wld.del(root);
	CHECK_FALSE(wld.valid(root));
	CHECK_FALSE(wld.valid(a));
	CHECK_FALSE(wld.valid(b));
	twld.update();
	twld.update();
	CHECK(wld.size() == sizeBefore - 3);
}

TEST_CASE("Entity bulk deletion - Parent children follow their parent") {
	TestWorld twld;

	const auto tag = wld.add();
	wld.add(tag, ecs::Pair(ecs::OnDelete, ecs::Delete));
	const auto parent = wld.add();
	wld.add(parent, tag);
	const auto child = wld.add();
	wld.parent(child, parent);

	wld.del(tag);
	twld.update();
	twld.update();
	CHECK_FALSE(wld.valid(parent));
	CHECK_FALSE(wld.valid(child));
}

TEST_CASE("Entity bulk deletion - non-fragmenting relation target and relation rows") {
	TestWorld twld;

	const auto tag = wld.add();
	wld.add(tag, ecs::Pair(ecs::OnDelete, ecs::Delete));
	const auto rel = wld.add();
	wld.add(rel, ecs::DontFragment);
	wld.add(rel, ecs::Exclusive);

	// Target row: the source loses the pair.
	const auto target = wld.add();
	wld.add(target, tag);
	const auto source = wld.add();
	wld.add(source, ecs::Pair(rel, target));

	// Relation row: the source loses every pair of that relation.
	const auto relRow = wld.add();
	wld.add(relRow, ecs::DontFragment);
	wld.add(relRow, ecs::Exclusive);
	wld.add(relRow, tag);
	const auto other = wld.add();
	const auto source2 = wld.add();
	wld.add(source2, ecs::Pair(relRow, other));

	wld.del(tag);
	twld.update();
	twld.update();
	CHECK_FALSE(wld.valid(target));
	CHECK_FALSE(wld.valid(relRow));
	CHECK(wld.valid(source));
	CHECK(wld.valid(source2));
	CHECK_FALSE(wld.has(source, ecs::Pair(rel, ecs::All)));

	// New entities reusing the ids must not inherit the pairs.
	GAIA_FOR(64) {
		const auto e = wld.add();
		if (e.id() == target.id())
			CHECK_FALSE(wld.has(source, ecs::Pair(rel, e)));
		if (e.id() == relRow.id())
			CHECK_FALSE(wld.has(source2, ecs::Pair(e, other)));
	}
}

#if GAIA_SYSTEMS_ENABLED
TEST_CASE("Entity deletion - system whose own target rule deletes its archetype is unregistered") {
	TestWorld twld;

	const auto rel = wld.add();
	wld.add(rel, ecs::Pair(ecs::OnDeleteTarget, ecs::Delete));
	uint32_t runs = 0;
	const auto sys = wld.system()
											 .all<Position>()
											 .on_each([&](ecs::Iter& it) {
												 runs += it.size();
											 })
											 .entity();
	const auto e = wld.add();
	wld.add<Position>(e, {});
	wld.update();
	CHECK(runs == 1);

	// Deleting sys deletes its own archetype through (rel, sys).
	wld.add(sys, ecs::Pair(rel, sys));
	wld.del(sys);
	CHECK_FALSE(wld.valid(sys));
	CHECK(wld.systems().data_try(sys) == nullptr);
	runs = 0;
	twld.update();
	CHECK(runs == 0);
}
#endif

TEST_CASE("Entity bulk deletion - prefab deleted with its instances") {
	// Cached queries hold the (Is, X) entries whose invalidation walks up from the deleted prefab
	GAIA_FOR(2) {
		const bool withQueries = i == 1;
		TestWorld twld;

		const auto tag = wld.add();
		wld.add(tag, ecs::Pair(ecs::OnDelete, ecs::Delete));
		const auto base = wld.prefab();
		const auto prefab = wld.prefab();
		wld.as(prefab, base);
		wld.add<Position>(prefab, {5, 0, 0});
		wld.add(prefab, tag);
		const auto inst0 = wld.instantiate(prefab);
		const auto inst1 = wld.instantiate(prefab);
		const auto survivor = wld.add();
		wld.as(survivor, base);

		auto qBase = wld.query().all(ecs::Pair(ecs::Is, base));
		auto qPrefab = wld.query().all(ecs::Pair(ecs::Is, prefab));
		uint32_t baseCnt = 0;
		if (withQueries) {
			baseCnt = qBase.count();
			CHECK(qPrefab.count() == 2);
		}

		wld.del(tag);
		twld.update();

		CHECK_FALSE(wld.valid(prefab));
		CHECK_FALSE(wld.valid(inst0));
		CHECK_FALSE(wld.valid(inst1));
		CHECK(wld.valid(base));
		CHECK(wld.valid(survivor));
		if (withQueries)
			CHECK(qBase.count() == baseCnt - 2);
		CHECK(wld.query().all<Position>().count() == 0);
	}
}

#if GAIA_OBSERVERS_ENABLED
TEST_CASE("Entity bulk deletion - OnDel observer on many rows") {
	// More rows than the deletion guard scans linearly, so the guard switches to its hashed mirror
	constexpr uint32_t N = 100;
	TestWorld twld;

	const auto tag = wld.add();
	wld.add(tag, ecs::Pair(ecs::OnDelete, ecs::Delete));
	cnt::darr<ecs::Entity> rows;
	for (uint32_t i = 0; i < N; ++i) {
		const auto e = wld.add();
		wld.add<Position>(e, {(float)i, 0, 0});
		wld.add(e, tag);
		rows.push_back(e);
	}
	const auto extra = wld.add();
	wld.add<Position>(extra, {});

	uint32_t hits = 0;
	bool bulk = true;
	bool extraDeleted = false;
	wld.observer()
			.all<Position>()
			.event(ecs::ObserverEvent::OnDel)
			.on_each([&](ecs::Iter& it) {
				hits += it.size();
				if (!bulk)
					return;
				// Rows being deleted are guarded against a second notification
				for (auto e: rows)
					wld.del(e);
				// A nested deletion of an unrelated entity while every row is on the guard
				if (!extraDeleted) {
					extraDeleted = true;
					wld.del(extra);
				}
			})
			.entity();

	wld.del(tag);
	twld.update();

	CHECK(hits == N + 1);
	CHECK_FALSE(wld.valid(extra));
	for (auto e: rows)
		CHECK_FALSE(wld.valid(e));

	// The guard is back to its shallow state and still suppresses nothing it should not
	bulk = false;
	const auto after = wld.add();
	wld.add<Position>(after, {});
	wld.del(after);
	twld.update();
	CHECK(hits == N + 2);
	CHECK_FALSE(wld.valid(after));
}
#endif

#if GAIA_USE_SAFE_ENTITY
TEST_CASE("Entity deletion - SafeEntity on a pair holds its endpoints") {
	// endpoint: 0 = relation, 1 = target
	// how: 0 = del(), 1 = (OnDelete, Delete) tag, 2 = ChildOf parent deleted
	for (uint32_t endpointIdx = 0; endpointIdx < 2; ++endpointIdx) {
		for (uint32_t how = 0; how < 3; ++how) {
			TestWorld twld;

			const auto tag = wld.add();
			wld.add(tag, ecs::Pair(ecs::OnDelete, ecs::Delete));
			const auto parent = wld.add();
			const auto rel = wld.add();
			const auto tgt = wld.add();
			const auto src = wld.add();
			wld.add(src, ecs::Pair(rel, tgt));
			const auto endpoint = endpointIdx == 0 ? rel : tgt;

			auto reuses_endpoint_id = [&]() {
				for (uint32_t i = 0; i < 64; ++i) {
					const auto e = wld.add();
					if (e.id() == endpoint.id())
						return e;
				}
				return ecs::EntityBad;
			};

			{
				const auto held = ecs::SafeEntity(wld, ecs::Pair(rel, tgt));
				if (how == 0)
					wld.del(endpoint);
				else if (how == 1) {
					wld.add(endpoint, tag);
					wld.del(tag);
				} else {
					wld.child(endpoint, parent);
					wld.del(parent);
				}
				twld.update();

				if (how == 0) {
					// A deletion request waits for the handle, like one on the entity itself
					CHECK(wld.valid(endpoint));
					CHECK(wld.has(src, ecs::Pair(rel, tgt)));
				} else {
					// A cleanup rule deletes the endpoint anyway, and the pair with it
					CHECK_FALSE(wld.valid(endpoint));
					CHECK_FALSE(wld.has(src, ecs::Pair(rel, tgt)));
				}
				CHECK(wld.valid(src));

				// The endpoint's id is not reused while the handle holds it
				CHECK(reuses_endpoint_id() == ecs::EntityBad);
			}
			twld.update();

			CHECK_FALSE(wld.valid(endpoint));
			CHECK_FALSE(wld.has(src, ecs::Pair(rel, tgt)));
			CHECK(wld.valid(endpointIdx == 0 ? tgt : rel));

			// Once released, the id is reused without aliasing the old pair
			const auto fresh = reuses_endpoint_id();
			CHECK(fresh != ecs::EntityBad);
			if (fresh != ecs::EntityBad) {
				const auto alias = endpointIdx == 0 ? ecs::Pair(fresh, tgt) : ecs::Pair(rel, fresh);
				CHECK_FALSE(wld.has(src, alias));
				CHECK(wld.query().all(alias).count() == 0);
			}
		}
	}
}

TEST_CASE("Entity deletion - SafeEntity on a self pair and copies after rule deletion") {
	SUBCASE("self pair") {
		TestWorld twld;

		const auto tag = wld.add();
		wld.add(tag, ecs::Pair(ecs::OnDelete, ecs::Delete));
		const auto rel = wld.add();
		const auto src = wld.add();
		wld.add(src, ecs::Pair(rel, rel));
		{
			const auto held = ecs::SafeEntity(wld, ecs::Pair(rel, rel));
			wld.del(rel);
			twld.update();
			CHECK(wld.valid(rel));
			CHECK(wld.has(src, ecs::Pair(rel, rel)));

			wld.add(rel, tag);
			wld.del(tag);
			twld.update();
			CHECK_FALSE(wld.valid(rel));
			CHECK_FALSE(wld.has(src, ecs::Pair(rel, rel)));
		}
		twld.update();
		CHECK_FALSE(wld.valid(rel));
		CHECK(wld.valid(src));
	}

	SUBCASE("copies made after a rule deleted an endpoint") {
		TestWorld twld;

		const auto tag = wld.add();
		wld.add(tag, ecs::Pair(ecs::OnDelete, ecs::Delete));
		const auto rel = wld.add();
		const auto tgt = wld.add();
		const auto src = wld.add();
		wld.add(src, ecs::Pair(rel, tgt));
		wld.add(rel, tag);
		{
			ecs::SafeEntity held(wld, ecs::Pair(rel, tgt));
			wld.del(tag);
			twld.update();
			CHECK_FALSE(wld.valid(rel));

			// Copies reference the reserved records, and each one is released in turn
			ecs::SafeEntity copied(held);
			ecs::SafeEntity assigned;
			assigned = held;
			held = ecs::SafeEntity{};
			CHECK((ecs::Entity)copied == ecs::Pair(rel, tgt));
		}
		twld.update();
		CHECK(wld.valid(tgt));
		CHECK(wld.valid(src));
		CHECK_FALSE(wld.has(src, ecs::Pair(rel, tgt)));
	}
}

TEST_CASE("Entity bulk deletion - SafeEntity keeps the record until released") {
	TestWorld twld;

	const auto tag = wld.add();
	wld.add(tag, ecs::Pair(ecs::OnDelete, ecs::Delete));
	const auto e = wld.add();
	wld.add(e, tag);
	const auto sizeBefore = wld.size();
	{
		const auto safe = ecs::SafeEntity(wld, e);
		wld.del(tag);
		twld.update();
		// The row is gone with its archetype, but the handle still owns the record.
		CHECK_FALSE(wld.valid(e));
		CHECK(wld.size() == sizeBefore - 1);
	}
	twld.update();
	CHECK_FALSE(wld.valid(e));
	CHECK(wld.size() == sizeBefore - 2);
}
#endif

TEST_CASE("Add - no components") {
	const uint32_t N = 1'500;

	TestWorld twld;
	cnt::darr<ecs::Entity> ents, arr;
	ents.reserve(N);
	arr.reserve(N);

	auto create = [&]() {
		auto e = wld.add();
		ents.push_back(e);
	};
	auto verify = [&](uint32_t i) {
		const auto a = arr[i + 2];
		const auto e = ents[i];
		CHECK(a == e);
	};

	GAIA_FOR(N) create();

	auto q = wld.query().no<ecs::Component>().no<ecs::Core_>();
	q.arr(arr);
	CHECK(arr.size() - 2 == ents.size()); // 2 for (OnDelete, Error) and (OnTargetDelete, Error)

	GAIA_FOR(N) verify(i);
}

TEST_CASE("Add - 1 component") {
	const uint32_t N = 1'500;

	TestWorld twld;
	cnt::darr<ecs::Entity> ents;
	ents.reserve(N);

	auto create = [&](uint32_t i) {
		auto e = wld.add();
		ents.push_back(e);
		wld.add<Int3>(e, {i, i, i});
	};
	auto verify = [&](uint32_t i) {
		auto e = ents[i];

		CHECK(wld.has<Int3>(e));
		auto val = wld.get<Int3>(e);
		CHECK(val.x == i);
		CHECK(val.y == i);
		CHECK(val.z == i);
	};

	GAIA_FOR(N) create(i);
	GAIA_FOR(N) verify(i);

	{
		const auto& p = wld.add<Int3>();
		auto e = wld.add();
		wld.add(e, p.entity, Int3{1, 2, 3});

		CHECK(wld.has(e, p.entity));
		CHECK(wld.has<Int3>(e));
		auto val0 = wld.get<Int3>(e);
		CHECK(val0.x == 1);
		CHECK(val0.y == 2);
		CHECK(val0.z == 3);
	}
}

namespace dummy {
	struct Position {
		float x;
		float y;
		float z;
	};
} // namespace dummy

TEST_CASE("Add - namespaces") {
	TestWorld twld;
	auto e = wld.add();
	wld.add<Position>(e, {1, 1, 1});
	wld.add<dummy::Position>(e);
	auto e2 = wld.add();
	wld.add<Position>(e2);
	auto a3 = wld.add();
	wld.add<dummy::Position>(a3, {7, 7, 7});
	auto a4 = wld.copy(a3);
	(void)a4;

	CHECK(wld.has<Position>(e));
	CHECK(wld.has<dummy::Position>(e));

	{
		auto p1 = wld.get<Position>(e);
		CHECK(p1.x == 1.f);
		CHECK(p1.y == 1.f);
		CHECK(p1.z == 1.f);
		// auto p2 = wld.get<dummy::Position>(e); commented, value added without being initialized to anything
		// CHECK(p2.x == 0.f);
		// CHECK(p2.y == 0.f);
		// CHECK(p2.z == 0.f);
	}
	{
		// auto p = wld.get<Position>(e2); commented, value added without being initialized to anything
		// CHECK(p.x == 1.f);
		// CHECK(p.y == 1.f);
		// CHECK(p.z == 1.f);
	}
	{
		auto p1 = wld.get<dummy::Position>(a3);
		CHECK(p1.x == 7.f);
		CHECK(p1.y == 7.f);
		CHECK(p1.z == 7.f);
		auto p2 = wld.get<dummy::Position>(a4);
		CHECK(p2.x == 7.f);
		CHECK(p2.y == 7.f);
		CHECK(p2.z == 7.f);
	}
	{
		auto q0 = wld.query().all<PositionSoA>();
		const auto c0 = q0.count();
		CHECK(c0 == 0); // nothing

		auto q1 = wld.query().all<Position>();
		const auto c1 = q1.count();
		CHECK(c1 == 2); // e, e2

		auto q2 = wld.query().all<dummy::Position>();
		const auto c2 = q2.count();
		CHECK(c2 == 3); // e, a3, a4

		auto q3 = wld.query().no<Position>();
		const auto c3 = q3.count();
		CHECK(c3 > 0); // It's going to be a bunch

		auto q4 = wld.query().all<dummy::Position>().no<Position>();
		const auto c4 = q4.count();
		CHECK(c4 == 2); // a3, a4
	}
}

TEST_CASE("Add - many components") {
	TestWorld twld;

	auto create = [&]() {
		auto e = wld.add();

		wld.add<Int3>(e, {3, 3, 3});
		wld.add<Position>(e, {1, 1, 1});
		wld.add<Empty>(e);
		wld.add<Else>(e, {true});
		wld.add<Rotation>(e, {2, 2, 2, 2});
		wld.add<Scale>(e, {4, 4, 4});

		CHECK(wld.has<Int3>(e));
		CHECK(wld.has<Position>(e));
		CHECK(wld.has<Empty>(e));
		CHECK(wld.has<Rotation>(e));
		CHECK(wld.has<Scale>(e));

		{
			auto val = wld.get<Int3>(e);
			CHECK(val.x == 3);
			CHECK(val.y == 3);
			CHECK(val.z == 3);
		}
		{
			auto val = wld.get<Position>(e);
			CHECK(val.x == 1.f);
			CHECK(val.y == 1.f);
			CHECK(val.z == 1.f);
		}
		{
			auto val = wld.get<Rotation>(e);
			CHECK(val.x == 2.f);
			CHECK(val.y == 2.f);
			CHECK(val.z == 2.f);
			CHECK(val.w == 2.f);
		}
		{
			auto val = wld.get<Scale>(e);
			CHECK(val.x == 4.f);
			CHECK(val.y == 4.f);
			CHECK(val.z == 4.f);
		}
	};

	const uint32_t N = 1'500;
	GAIA_FOR(N) create();
}

TEST_CASE("Add - maximum archetype id capacity is usable") {
	TestWorld twld;

	ecs::Entity ids[ecs::ChunkHeader::MAX_COMPONENTS]{};
	const auto entity = wld.add();
	GAIA_FOR(ecs::ChunkHeader::MAX_COMPONENTS) {
		ids[i] = wld.add();
		wld.add(entity, ids[i]);
	}

	GAIA_FOR(ecs::ChunkHeader::MAX_COMPONENTS) CHECK(wld.has(entity, ids[i]));
	CHECK(wld.fetch(entity).pArchetype->ids_view().size() == ecs::ChunkHeader::MAX_COMPONENTS);
}

TEST_CASE("EntityBuilder - pending commit ownership moves") {
	static_assert(!std::is_copy_constructible_v<ecs::EntityBuilder>);
	static_assert(std::is_move_constructible_v<ecs::EntityBuilder>);

	TestWorld twld;
	const auto entity = wld.add();
	uint32_t addHits = 0;
	(void)wld.observer()
			.event(ecs::ObserverEvent::OnAdd)
			.all<Position>()
			.on_each([&](ecs::Iter&) {
				++addHits;
			})
			.entity();

	{
		auto builder = wld.build(entity);
		builder.add<Position>();
		ecs::EntityBuilder moved(GAIA_MOV(builder));
		(void)moved;
	}

	CHECK(wld.has<Position>(entity));
	CHECK(addHits == 1);
}

TEST_CASE("Add - many components, bulk") {
	TestWorld twld;

	auto create = [&]() {
		auto e = wld.add();

		auto b = wld.build(e);
		(void)b;
		wld.build(e).add<Int3>().add<Position>().add<Empty>().add<Else>().add<Rotation>().add<Scale>();

		CHECK(wld.has<Int3>(e));
		CHECK(wld.has<Position>(e));
		CHECK(wld.has<Empty>(e));
		CHECK(wld.has<Else>(e));
		CHECK(wld.has<Rotation>(e));
		CHECK(wld.has<Scale>(e));

		wld.acc_mut(e)
				.set<Int3>({3, 3, 3})
				.set<Position>({1, 1, 1})
				.set<Else>({true})
				.set<Rotation>({2, 2, 2, 2})
				.set<Scale>({4, 4, 4});

		{
			auto val = wld.get<Int3>(e);
			CHECK(val.x == 3);
			CHECK(val.y == 3);
			CHECK(val.z == 3);
		}
		{
			auto val = wld.get<Position>(e);
			CHECK(val.x == 1.f);
			CHECK(val.y == 1.f);
			CHECK(val.z == 1.f);
		}
		{
			auto val = wld.get<Rotation>(e);
			CHECK(val.x == 2.f);
			CHECK(val.y == 2.f);
			CHECK(val.z == 2.f);
			CHECK(val.w == 2.f);
		}
		{
			auto val = wld.get<Scale>(e);
			CHECK(val.x == 4.f);
			CHECK(val.y == 4.f);
			CHECK(val.z == 4.f);
		}

		{
			auto setter = wld.acc_mut(e);
			auto& pos = setter.mut<Int3>();
			pos = {30, 30, 30};

			{
				auto val = wld.get<Int3>(e);
				CHECK(val.x == 30);
				CHECK(val.y == 30);
				CHECK(val.z == 30);

				val = setter.get<Int3>();
				CHECK(val.x == 30);
				CHECK(val.y == 30);
				CHECK(val.z == 30);
			}
		}

		wld.clear(e);
		CHECK_FALSE(wld.has<Int3>(e));
		CHECK_FALSE(wld.has<Position>(e));
		CHECK_FALSE(wld.has<Empty>(e));
		CHECK_FALSE(wld.has<Else>(e));
		CHECK_FALSE(wld.has<Rotation>(e));
		CHECK_FALSE(wld.has<Scale>(e));
	};

	const uint32_t N = 1'500;
	GAIA_FOR(N) create();
}

TEST_CASE("Add - many components, bulk 2") {
	TestWorld twld;
	auto e = wld.add();

	auto checkInt3 = [&]() {
		CHECK(wld.has<Int3>(e));
		wld.acc_mut(e).set<Int3>({3, 3, 3});

		{
			auto val = wld.get<Int3>(e);
			CHECK(val.x == 3);
			CHECK(val.y == 3);
			CHECK(val.z == 3);
		}

		{
			auto setter = wld.acc_mut(e);
			auto& pos = setter.mut<Int3>();
			pos = {30, 30, 30};

			{
				auto val = wld.get<Int3>(e);
				CHECK(val.x == 30);
				CHECK(val.y == 30);
				CHECK(val.z == 30);

				val = setter.get<Int3>();
				CHECK(val.x == 30);
				CHECK(val.y == 30);
				CHECK(val.z == 30);
			}
		}
	};

	// This should result in adding just Int3
	wld.build(e).add<Position>().add<Int3>().del<Position>();
	CHECK_FALSE(wld.has<Position>(e));
	checkInt3();

	// This should do nothing
	wld.build(e).add<Position>().del<Position>();
	CHECK_FALSE(wld.has<Position>(e));
	checkInt3();
}

TEST_CASE("Accessor - repeated component access") {
	TestWorld twld;
	auto e = wld.add();
	const auto& runtimeInt3 = wld.add<Int3>();

	wld.add<Position>(e, {1.f, 2.f, 3.f});
	wld.add<Rotation>(e, {4.f, 5.f, 6.f, 7.f});
	wld.add(e, runtimeInt3.entity, Int3{8, 9, 10});

	{
		auto getter = wld.acc(e);
		auto pos = getter.get<Position>();
		CHECK(pos.x == 1.f);
		CHECK(pos.y == 2.f);
		CHECK(pos.z == 3.f);

		auto rot = getter.get<Rotation>();
		CHECK(rot.x == 4.f);
		CHECK(rot.y == 5.f);
		CHECK(rot.z == 6.f);
		CHECK(rot.w == 7.f);

		pos = getter.get<Position>();
		CHECK(pos.x == 1.f);
		CHECK(pos.y == 2.f);
		CHECK(pos.z == 3.f);

		auto i3 = getter.get<Int3>(runtimeInt3.entity);
		CHECK(i3.x == 8);
		CHECK(i3.y == 9);
		CHECK(i3.z == 10);
	}

	{
		auto setter = wld.acc_mut(e);
		setter.set<Position>({10.f, 20.f, 30.f});
		setter.sset<Rotation>({40.f, 50.f, 60.f, 70.f});
		setter.set<Int3>(runtimeInt3.entity, Int3{80, 90, 100});
	}

	{
		auto getter = wld.acc(e);
		auto pos = getter.get<Position>();
		CHECK(pos.x == 10.f);
		CHECK(pos.y == 20.f);
		CHECK(pos.z == 30.f);

		auto rot = getter.get<Rotation>();
		CHECK(rot.x == 40.f);
		CHECK(rot.y == 50.f);
		CHECK(rot.z == 60.f);
		CHECK(rot.w == 70.f);

		auto i3 = getter.get<Int3>(runtimeInt3.entity);
		CHECK(i3.x == 80);
		CHECK(i3.y == 90);
		CHECK(i3.z == 100);
	}
}

TEST_CASE("Add-del") {
	// This is a do-not-crash unit test.
	// This is done to properly test cases of [] -> [A] -> [A,B] -> [A,B,C].
	// If B disappears and an operation later needs that transition, the graph need to re-link correctly (
	// where a direct transition is semantically valid for that entity operation).
	// Create a one-direction cached path, delete a type/archetype in that path, then repeatedly run
	// remove-component transitions(del) plus update() cleanup.
	TestWorld twld;

	// 1) Create three entities
	auto A = wld.add();
	auto B = wld.add();
	auto C = wld.add();

	// 2) Build archetype chain using one add-order only:
	//    [] -> [A] -> [A,B] -> [A,B,C]
	auto e = wld.add();
	wld.add(e, A);
	wld.add(e, B);
	wld.add(e, C);

	// 3) Delete A
	wld.del(A);

	// 4) Keep mutating entities through remove paths so del-edge lookup is used
	for (int i = 0; i < 2000; ++i) {
		auto x = wld.add();
		wld.add(x, B);
		wld.add(x, C);
		wld.del(x, B); // hits foc_archetype_del path
		wld.del(x);
		wld.update(); // runs gc/archetype cleanup
	}
}

TEST_CASE("Chunk delete queue handles multiple queued chunks") {
	TestWorld twld;

	struct ChunkQueueTag {};

	cnt::darray<ecs::Entity> entities;
	entities.reserve(5000);

	for (uint32_t i = 0; i < 5000; ++i) {
		auto e = wld.add();
		wld.add<ChunkQueueTag>(e);
		entities.push_back(e);
	}

	for (auto e: entities)
		wld.del(e);

	wld.update();

	for (uint32_t i = 0; i < 5000; ++i) {
		auto e = wld.add();
		wld.add<ChunkQueueTag>(e);
		entities[i] = e;
	}

	for (uint32_t i = 0; i < 2500; ++i)
		wld.del(entities[i]);

	wld.update();

	auto q = wld.query().all<ChunkQueueTag>();
	CHECK(q.count() == 2500);
}

TEST_CASE("Pair") {
	{
		TestWorld twld;
		auto a = wld.add();
		auto b = wld.add();
		auto p = ecs::Pair(a, b);
		CHECK(p.first() == a);
		CHECK(p.second() == b);
		auto pe = (ecs::Entity)p;
		CHECK(wld.get(pe.id()) == a);
		CHECK(wld.get(pe.gen()) == b);
	}
	{
		TestWorld twld;
		const auto relation = wld.add();
		const auto target = wld.add();
		const auto source = wld.add();
		const auto pair = ecs::Pair(relation, target);
		wld.add(source, pair);

		wld.add(pair, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));
		CHECK(wld.has_direct((ecs::Entity)pair, ecs::Pair(ecs::OnInstantiate, ecs::Inherit)));
		CHECK(wld.target((ecs::Entity)pair, ecs::OnInstantiate) == ecs::Inherit);
	}
	{
		TestWorld twld;
		auto a = wld.add<Position>().entity;
		auto b = wld.add<ecs::Requires_>().entity;
		auto p = ecs::Pair(a, b);
		CHECK(ecs::is_pair<decltype(p)>::value);
		CHECK(p.first() == a);
		CHECK(p.second() == b);
		auto pe = (ecs::Entity)p;
		CHECK_FALSE(ecs::is_pair<decltype(pe)>::value);
		CHECK(wld.get(pe.id()) == a);
		CHECK(wld.get(pe.gen()) == b);
	}
	struct Start {};
	struct Stop {};
	{
		CHECK(ecs::is_pair<ecs::pair<Start, Position>>::value);
		CHECK(ecs::is_pair<ecs::pair<Position, Start>>::value);

		using Pair1 = ecs::pair<Start, Position>;
		static_assert(std::is_same_v<Pair1::rel, Start>);
		static_assert(std::is_same_v<Pair1::tgt, Position>);
		using Pair1Actual = ecs::actual_type_t<Pair1>;
		static_assert(std::is_same_v<Pair1Actual::Type, Position>);

		TestWorld twld;
		const auto& pci = wld.add<Position>();
		using TestPair = ecs::pair<Position, Acceleration>;
		const auto& pci2 = wld.add<TestPair::rel>();
		CHECK(pci.entity == pci2.entity);
	}
	{
		TestWorld twld;
		auto eStart = wld.add<Start>().entity;
		auto eStop = wld.add<Stop>().entity;
		auto ePos = wld.add<Position>().entity;
		auto e = wld.add();

		wld.add<Position>(e, {5, 5, 5});
		wld.add(e, {eStart, ePos});
		wld.add(e, {eStop, ePos});
		auto p = wld.get<Position>(e);
		CHECK(p.x == 5);
		CHECK(p.y == 5);
		CHECK(p.z == 5);

		CHECK(p.x == 5);
		CHECK(p.y == 5);
		CHECK(p.z == 5);

		wld.add<ecs::pair<Start, Position>>(e, {100, 100, 100}); // 14:18
		auto sp = wld.get<ecs::pair<Start, Position>>(e);
		CHECK(sp.x == 100);
		CHECK(sp.y == 100);
		CHECK(sp.z == 100);

		p = wld.get<Position>(e);
		CHECK(p.x == 5);
		CHECK(p.y == 5);
		CHECK(p.z == 5);

		{
			uint32_t i = 0;
			auto q = wld.query().all<ecs::pair<Start, Position>>();
			q.each([&]() {
				++i;
			});
			CHECK(i == 1);
		}
	}
	{
		TestWorld twld;

		auto wolf = wld.add();
		auto rabbit = wld.add();
		auto carrot = wld.add();
		auto wine = wld.add();
		auto water = wld.add();
		auto eats = wld.add();
		auto drinks = wld.add();

		wld.add(rabbit, {eats, carrot});
		wld.add(wolf, {eats, rabbit});
		wld.add(rabbit, {drinks, water});
		wld.add(wolf, {drinks, wine});

		{
			uint32_t i = 0;
			auto q = wld.query().all(ecs::Pair{eats, ecs::All});
			q.each([&]() {
				++i;
			});
			CHECK(i == 2);
		}
		{
			uint32_t i = 0;
			auto q = wld.query().all(ecs::Pair{drinks, ecs::All});
			q.each([&]() {
				++i;
			});
			CHECK(i == 2);
		}
		{
			uint32_t i = 0;
			auto q = wld.query().all(ecs::Pair{drinks, water}).all(ecs::Pair{eats, ecs::All});
			q.each([&]() {
				++i;
			});
			CHECK(i == 1);
		}
		{
			uint32_t i = 0;
			auto q = wld.query().all(ecs::Pair{eats, ecs::All}).all(ecs::Pair{drinks, water});
			q.each([&]() {
				++i;
			});
			CHECK(i == 1);
		}
		{
			uint32_t i = 0;
			auto q = wld.query().all(ecs::Pair{eats, ecs::All}).all(ecs::Pair{drinks, ecs::All});
			q.each([&]() {
				++i;
			});
			CHECK(i == 2);
		}
	}
}

TEST_CASE("CantCombine") {
	SUBCASE("One") {
		TestWorld twld;
		auto weak = wld.add();
		auto strong = wld.add();
		wld.add(weak, {ecs::CantCombine, strong});

		auto dummy = wld.add();
		wld.add(dummy, strong);
#if !GAIA_ASSERT_ENABLED
		// Can be tested only with asserts disabled because the situation is assert-protected.
		wld.add(dummy, weak);
		CHECK(wld.has(dummy, strong));
		CHECK_FALSE(wld.has(dummy, weak));
#endif

		// Unset
		wld.del(weak, {ecs::CantCombine, strong});
		wld.add(dummy, weak);
		CHECK(wld.has(dummy, strong));
		CHECK(wld.has(dummy, weak));
	}
	SUBCASE("Two") {
		TestWorld twld;
		auto weak = wld.add();
		auto strong = wld.add();
		auto stronger = wld.add();
		wld.add(weak, {ecs::CantCombine, strong});
		wld.add(weak, {ecs::CantCombine, stronger});

		auto dummy = wld.add();
		wld.add(dummy, strong);
#if !GAIA_ASSERT_ENABLED
		// Can be tested only with asserts disabled because the situation is assert-protected.
		wld.add(dummy, weak);
		CHECK(wld.has(dummy, strong));
		CHECK_FALSE(wld.has(dummy, weak));
#endif
		wld.add(dummy, stronger);
#if !GAIA_ASSERT_ENABLED
		// Can be tested only with asserts disabled because the situation is assert-protected.
		wld.add(dummy, weak);
		CHECK(wld.has(dummy, strong));
		CHECK(wld.has(dummy, stronger));
		CHECK_FALSE(wld.has(dummy, weak));
#endif

		// Unset strong. Still should not be able to add because of stronger.
		wld.del(weak, {ecs::CantCombine, strong});
#if !GAIA_ASSERT_ENABLED
		// Can be tested only with asserts disabled because the situation is assert-protected.
		wld.add(dummy, weak);
		CHECK(wld.has(dummy, strong));
		CHECK(wld.has(dummy, stronger));
		CHECK_FALSE(wld.has(dummy, weak));
#endif

		// Unset. Finally should be able to add because there are no more restrictions
		wld.del(weak, {ecs::CantCombine, stronger});
		wld.add(dummy, weak);
		CHECK(wld.has(dummy, strong));
		CHECK(wld.has(dummy, stronger));
		CHECK(wld.has(dummy, weak));
	}
}

TEST_CASE("Requires") {
	TestWorld twld;
	auto rabbit = wld.add();
	auto animal = wld.add();
	auto herbivore = wld.add();
	auto carrot = wld.add();
	wld.add(carrot, {ecs::Requires, herbivore});
	wld.add(herbivore, {ecs::Requires, animal});

	wld.add(rabbit, carrot);
	CHECK(wld.has(rabbit, herbivore));
	CHECK(wld.has(rabbit, animal));

	wld.del(rabbit, animal);
	CHECK(wld.has(rabbit, animal));

	wld.del(rabbit, herbivore);
	CHECK(wld.has(rabbit, herbivore));

	wld.del(rabbit, carrot);
	CHECK_FALSE(wld.has(rabbit, carrot));
}

TEST_CASE("Inheritance (Is)") {
	TestWorld twld;
	ecs::Entity animal = wld.add();
	ecs::Entity herbivore = wld.add();
	ecs::Entity carnivore = wld.add();
	wld.add<Position>(herbivore, {});
	wld.add<Rotation>(herbivore, {});
	ecs::Entity rabbit = wld.add();
	ecs::Entity hare = wld.add();
	ecs::Entity wolf = wld.add();

	wld.as(carnivore, animal);
	wld.as(herbivore, animal);
	wld.as(rabbit, herbivore);
	wld.as(hare, herbivore);
	wld.as(wolf, carnivore);

	CHECK_FALSE(wld.has(animal, animal));
	CHECK_FALSE(wld.has(herbivore, herbivore));
	CHECK_FALSE(wld.has(carnivore, carnivore));
	CHECK_FALSE(wld.has_direct(animal, ecs::Pair(ecs::Is, animal)));
	CHECK_FALSE(wld.has_direct(herbivore, ecs::Pair(ecs::Is, herbivore)));
	CHECK_FALSE(wld.has_direct(carnivore, ecs::Pair(ecs::Is, carnivore)));

	CHECK(wld.is(carnivore, animal));
	CHECK(wld.is(herbivore, animal));
	CHECK(wld.is(rabbit, animal));
	CHECK(wld.is(hare, animal));
	CHECK(wld.is(wolf, animal));
	CHECK(wld.has(carnivore, ecs::Pair(ecs::Is, animal)));
	CHECK(wld.has(herbivore, ecs::Pair(ecs::Is, animal)));
	CHECK(wld.has(rabbit, ecs::Pair(ecs::Is, animal)));
	CHECK(wld.has(hare, ecs::Pair(ecs::Is, animal)));
	CHECK(wld.has(wolf, ecs::Pair(ecs::Is, animal)));
	CHECK(wld.is(rabbit, herbivore));
	CHECK(wld.is(hare, herbivore));
	CHECK(wld.is(wolf, carnivore));

	CHECK(wld.is(animal, animal));
	CHECK(wld.is(herbivore, herbivore));
	CHECK(wld.is(carnivore, carnivore));
	CHECK(wld.has(animal, ecs::Pair(ecs::Is, animal)));
	CHECK(wld.has(herbivore, ecs::Pair(ecs::Is, herbivore)));
	CHECK(wld.has(carnivore, ecs::Pair(ecs::Is, carnivore)));
	CHECK(wld.has_direct(carnivore, ecs::Pair(ecs::Is, animal)));
	CHECK(wld.has_direct(herbivore, ecs::Pair(ecs::Is, animal)));
	CHECK(wld.has_direct(rabbit, ecs::Pair(ecs::Is, herbivore)));
	CHECK_FALSE(wld.has_direct(rabbit, ecs::Pair(ecs::Is, animal)));
	CHECK_FALSE(wld.has_direct(wolf, ecs::Pair(ecs::Is, animal)));

	CHECK_FALSE(wld.is(animal, herbivore));
	CHECK_FALSE(wld.is(animal, carnivore));
	CHECK_FALSE(wld.is(wolf, herbivore));
	CHECK_FALSE(wld.is(rabbit, carnivore));
	CHECK_FALSE(wld.is(hare, carnivore));

	ecs::QueryTermOptions directOpts;
	directOpts.direct();
	auto qDirectAnimal = wld.uquery().all(ecs::Pair(ecs::Is, animal), directOpts);
	CHECK(qDirectAnimal.count() == 2);
	cnt::darr<ecs::Entity> directAnimalEntities;
	qDirectAnimal.each([&](ecs::Entity entity) {
		directAnimalEntities.push_back(entity);
	});
	CHECK(directAnimalEntities.size() == 2);
	CHECK(core::has(directAnimalEntities, herbivore));
	CHECK(core::has(directAnimalEntities, carnivore));

	{
		uint32_t i = 0;
		ecs::Query q = wld.query().all(ecs::Pair(ecs::Is, animal));
		q.each([&](ecs::Entity entity) {
			const bool isOK = entity == animal || entity == hare || entity == rabbit || entity == herbivore ||
												entity == carnivore || entity == wolf;
			CHECK(isOK);

			++i;
		});
		CHECK(i == 6);
	}
	{
		uint32_t i = 0;
		ecs::Query q = wld.query().all(ecs::Pair(ecs::Is, herbivore));
		q.each([&](ecs::Entity entity) {
			const bool isOK = entity == hare || entity == rabbit || entity == herbivore;
			CHECK(isOK);

			++i;
		});
		CHECK(i == 3);
	}
	{
		uint32_t i = 0;
		ecs::Query q = wld.query().all(ecs::Pair(ecs::Is, animal)).no(herbivore);
		q.each([&](ecs::Entity entity) {
			const bool isOK = entity == animal || entity == hare || entity == rabbit || entity == wolf ||
												entity == carnivore || entity == herbivore;
			CHECK(isOK);

			++i;
		});
		CHECK(i == 6);
	}
	{
		uint32_t i = 0;
		ecs::Query q = wld.query().all(ecs::Pair(ecs::Is, animal)).no(ecs::Pair(ecs::Is, herbivore));
		q.each([&](ecs::Entity entity) {
			const bool isOK = entity == animal || entity == wolf || entity == carnivore;
			CHECK(isOK);

			++i;
		});
		CHECK(i == 3);
	}
	{
		uint32_t i = 0;
		ecs::Query q = wld.query().all(ecs::Pair(ecs::Is, animal)).no(ecs::Pair(ecs::Is, herbivore));
		q.each([&](ecs::Entity entity) {
			const bool isOK = entity == animal || entity == wolf || entity == carnivore;
			CHECK(isOK);

			++i;
		});
		CHECK(i == 3);
	}
	{
		uint32_t i = 0;
		ecs::Query q = wld.query().all(ecs::Pair(ecs::Is, animal)).no(ecs::Pair(ecs::Is, carnivore));
		q.each([&](ecs::Entity entity) {
			const bool isOK = entity == animal || entity == hare || entity == rabbit || entity == herbivore;
			CHECK(isOK);

			++i;
		});
		CHECK(i == 4);
	}
}

TEST_CASE("Inheritance (Is) - pair records are not subjects or bases") {
	TestWorld twld;

	const auto animal = wld.add();
	const auto rabbit = wld.add();
	wld.as(rabbit, animal);

	const auto relation = wld.add();
	const auto target = wld.add();
	const auto owner = wld.add();
	const auto pair = ecs::Pair(relation, target);
	wld.add(owner, pair);
	const auto pairEntity = (ecs::Entity)pair;

	CHECK(pairEntity.pair());
	CHECK(wld.valid(pairEntity));

	CHECK(wld.is(rabbit, animal));
	CHECK(wld.in(rabbit, animal));
	CHECK_FALSE(wld.is(pairEntity, animal));
	CHECK_FALSE(wld.in(pairEntity, animal));
	CHECK_FALSE(wld.is(rabbit, pairEntity));
	CHECK_FALSE(wld.is(animal, pairEntity));
	CHECK_FALSE(wld.is(pairEntity, pairEntity));
	CHECK_FALSE(wld.is_base(pairEntity));
	CHECK(wld.is_base(animal));
	CHECK_FALSE(wld.has_direct(pairEntity, ecs::Pair(ecs::Is, animal)));
	CHECK_FALSE(wld.has_direct(rabbit, ecs::Pair(ecs::Is, relation)));

	const auto parent = wld.add();
	wld.child(pair, parent);
	CHECK(wld.is_child(pairEntity, parent));
	CHECK_FALSE(wld.is(pairEntity, animal));

	{
		uint32_t n = 0;
		bool sawPair = false;
		wld.query().is(animal).each([&](ecs::Entity entity) {
			sawPair |= entity.pair();
			++n;
		});
		CHECK(n == 2);
		CHECK_FALSE(sawPair);
	}
}

TEST_CASE("Inheritance (Is) - change") {
	TestWorld twld;
	ecs::Entity animal = wld.add();
	ecs::Entity herbivore = wld.add();
	ecs::Entity carnivore = wld.add();
	ecs::Entity wolf = wld.add();

	wld.as(carnivore, animal);
	wld.as(herbivore, animal);
	wld.as(wolf, carnivore);

	CHECK(wld.is(carnivore, animal));
	CHECK(wld.is(herbivore, animal));
	CHECK(wld.is(wolf, animal));

	CHECK(wld.is(animal, animal));
	CHECK(wld.is(herbivore, herbivore));
	CHECK(wld.is(carnivore, carnivore));
	CHECK(wld.is(wolf, carnivore));

	CHECK_FALSE(wld.is(animal, herbivore));
	CHECK_FALSE(wld.is(animal, carnivore));
	CHECK_FALSE(wld.is(wolf, herbivore));

	ecs::Query q = wld.query().all(ecs::Pair(ecs::Is, animal));

	{
		uint32_t i = 0;
		q.each([&](ecs::Entity entity) {
			const bool isOK = entity == animal || entity == herbivore || entity == carnivore || entity == wolf;
			CHECK(isOK);

			++i;
		});
		CHECK(i == 4);
	}

	// Carnivore is no longer an animal
	wld.del(carnivore, {ecs::Is, animal});
	CHECK(wld.is(wolf, carnivore));
	CHECK_FALSE(wld.is(carnivore, animal));
	CHECK_FALSE(wld.is(wolf, animal));

	{
		uint32_t i = 0;
		q.each([&](ecs::Entity entity) {
			const bool isOK = entity == animal || entity == herbivore;
			CHECK(isOK);

			++i;
		});
		CHECK(i == 2);
	}

	// Make carnivore an animal again
	wld.as(carnivore, animal);

	{
		uint32_t i = 0;
		q.each([&](ecs::Entity entity) {
			const bool isOK = entity == animal || entity == herbivore || entity == carnivore || entity == wolf;
			CHECK(isOK);

			++i;
		});
		CHECK(i == 4);
	}

	// Wolf is no longer a carnivore and thus no longer an animal.
	// It should no longer match q
	wld.del(wolf, {ecs::Is, carnivore});
	CHECK_FALSE(wld.is(wolf, carnivore));
	CHECK(wld.is(carnivore, animal));
	CHECK_FALSE(wld.is(wolf, animal));

	{
		uint32_t i = 0;
		q.each([&](ecs::Entity entity) {
			const bool isOK = entity == animal || entity == herbivore || entity == carnivore;
			CHECK(isOK);

			++i;
		});
		CHECK(i == 3);
	}
}

TEST_CASE("AddAndDel_entity - no components") {
	const uint32_t N = 1'500;

	TestWorld twld;
	cnt::darr<ecs::Entity> arr;
	arr.reserve(N);

	auto create = [&]() {
		auto e = wld.add();
		arr.push_back(e);
	};
	auto remove = [&](ecs::Entity e) {
		wld.del(e);
		const bool isValid = wld.valid(e);
		const bool hasEntity = wld.has(e);
		CHECK_FALSE(isValid);
		CHECK_FALSE(hasEntity);
	};

	// Create entities
	GAIA_FOR(N) create();
	// Remove entities
	GAIA_FOR(N) remove(arr[i]);

	wld.update();
	GAIA_FOR(N) {
		const auto e = arr[i];
		const bool isValid = wld.valid(e);
		const bool hasEntity = wld.has(e);
		CHECK_FALSE(isValid);
		CHECK_FALSE(hasEntity);
	}
}

TEST_CASE("AddAndDel_entity - 1 component") {
	const uint32_t N = 1'500;

	TestWorld twld;
	cnt::darr<ecs::Entity> arr;
	arr.reserve(N);

	auto create = [&](uint32_t id) {
		auto e = wld.add();
		arr.push_back(e);

		wld.add<Int3>(e, {id, id, id});
		auto pos = wld.get<Int3>(e);
		CHECK(pos.x == id);
		CHECK(pos.y == id);
		CHECK(pos.z == id);
		return e;
	};
	auto remove = [&](ecs::Entity e) {
		wld.del(e);
		const bool isValid = wld.valid(e);
		const bool hasEntity = wld.has(e);
		CHECK_FALSE(isValid);
		CHECK_FALSE(hasEntity);
	};

	GAIA_FOR(N) create(i);
	GAIA_FOR(N) remove(arr[i]);

	wld.update();
	GAIA_FOR(N) {
		const auto e = arr[i];
		const bool isValid = wld.valid(e);
		const bool hasEntity = wld.has(e);
		CHECK_FALSE(isValid);
		CHECK_FALSE(hasEntity);
	}
}

void verify_entity_has(const ecs::ComponentCache& cc, ecs::Entity entity) {
	const auto* res = cc.find(entity);
	CHECK(res != nullptr);
}

template <typename T>
void verify_name_has(const ecs::World& world, const ecs::ComponentCache& cc, const char* str) {
	const auto& item = cc.get<T>();
	const auto name = item.symbol_name();
	CHECK(name.data() != nullptr);
	CHECK(name.size() > 1);
	CHECK(world.symbol(str) == item.entity);
}

void verify_name_has_not(const ecs::World& world, const char* str) {
	CHECK(world.symbol(str) == ecs::EntityBad);
}

template <typename T>
void verify_name_has_not(const ecs::ComponentCache& cc) {
	const auto* item = cc.find<T>();
	CHECK(item == nullptr);
}

#define verify_name_has(name) verify_name_has<name>(wld, cc, #name);
#define verify_name_has_not(name)                                                                                      \
	{                                                                                                                    \
		verify_name_has_not<name>(cc);                                                                                     \
		verify_name_has_not(wld, #name);                                                                                   \
	}

TEST_CASE("Component names") {
	SUBCASE("direct registration") {
		TestWorld twld;
		const auto& cc = wld.comp_cache();

		verify_name_has_not(Int3);
		verify_name_has_not(Position);
		verify_name_has_not(dummy::Position);

		auto e_pos = wld.add<Position>().entity;
		verify_entity_has(cc, e_pos);
		verify_name_has_not(Int3);
		verify_name_has(Position);
		verify_name_has_not(dummy::Position);

		auto e_int = wld.add<Int3>().entity;
		verify_entity_has(cc, e_pos);
		verify_entity_has(cc, e_int);
		verify_name_has(Int3);
		verify_name_has(Position);
		verify_name_has_not(dummy::Position);

		auto e_dpos = wld.add<dummy::Position>().entity;
		verify_entity_has(cc, e_pos);
		verify_entity_has(cc, e_int);
		verify_entity_has(cc, e_dpos);
		verify_name_has(Int3);
		verify_name_has(Position);
		verify_name_has(dummy::Position);
	}
	SUBCASE("entity+component") {
		TestWorld twld;
		const auto& cc = wld.comp_cache();
		auto e = wld.add();

		verify_name_has_not(Int3);
		verify_name_has_not(Position);
		verify_name_has_not(dummy::Position);

		wld.add<Position>(e);
		verify_name_has_not(Int3);
		verify_name_has(Position);
		verify_name_has_not(dummy::Position);

		wld.add<Int3>(e);
		verify_name_has(Int3);
		verify_name_has(Position);
		verify_name_has_not(dummy::Position);

		wld.del<Position>(e);
		verify_name_has(Int3);
		verify_name_has(Position);
		verify_name_has_not(dummy::Position);

		wld.add<dummy::Position>(e);
		verify_name_has(Int3);
		verify_name_has(Position);
		verify_name_has(dummy::Position);
	}

	SUBCASE("component scope controls default paths and relative lookup") {
		TestWorld twld;

		const auto gameplay = wld.add();
		wld.name(gameplay, "gameplay");

		const auto render = wld.add();
		wld.name(render, "render");
		wld.child(render, gameplay);

		const auto tools = wld.add();
		wld.name(tools, "tools");
		wld.child(tools, gameplay);

		ecs::Entity gameplayPos = ecs::EntityBad;
		ecs::Entity renderPos = ecs::EntityBad;
		ecs::Entity renderDevice = ecs::EntityBad;

		wld.scope(gameplay, [&] {
			gameplayPos = wld.add<Position>().entity;
			CHECK(wld.path(gameplayPos) == "gameplay.Position");
			CHECK(wld.get("Position") == gameplayPos);

			wld.scope(render, [&] {
				renderPos = wld.add<dummy::Position>().entity;
				CHECK(wld.path(renderPos) == "gameplay.render.Position");
				CHECK(wld.get("Position") == renderPos);

				renderDevice = add_runtime_component(wld, "Device", 0, ecs::DataStorageType::Table, 1).entity;
				CHECK(wld.path(renderDevice) == "gameplay.render.Device");
				CHECK(wld.get("Device") == renderDevice);
				CHECK(wld.get("gameplay.render.Device") == renderDevice);
			});

			CHECK(wld.get("Position") == gameplayPos);
		});

		CHECK(wld.scope() == ecs::EntityBad);
		CHECK(wld.alias("Position") == ecs::EntityBad);
		CHECK(wld.get("Position") == gameplayPos);

		CHECK(wld.scope(tools) == ecs::EntityBad);
		CHECK(wld.get("Position") == gameplayPos);
		CHECK(wld.get("gameplay.render.Device") == renderDevice);

		CHECK(wld.scope(render) == tools);
		CHECK(wld.get("Position") == renderPos);
		CHECK(wld.get("Device") == renderDevice);

		CHECK(wld.scope(ecs::EntityBad) == render);
		CHECK(wld.get("Position") == gameplayPos);
	}

	SUBCASE("component scope resolves string queries at construction time") {
		TestWorld twld;

		const auto gameplay = wld.add();
		wld.name(gameplay, "gameplay");

		const auto render = wld.add();
		wld.name(render, "render");
		wld.child(render, gameplay);

		const auto gameplayComp = wld.add<Position>().entity;
		ecs::Entity renderComp = ecs::EntityBad;
		wld.scope(render, [&] {
			renderComp = wld.add<dummy::Position>().entity;
		});

		const auto eGameplay = wld.add();
		wld.add(eGameplay, gameplayComp);

		const auto eRender = wld.add();
		wld.add(eRender, renderComp);

		auto qGlobal = wld.uquery().add("Position");
		CHECK(qGlobal.count() == 1);
		qGlobal.each([&](ecs::Entity e) {
			CHECK(e == eGameplay);
		});

		auto qScoped = wld.uquery();
		wld.scope(render, [&] {
			qScoped.add("Position");
		});
		CHECK(qScoped.count() == 1);
		qScoped.each([&](ecs::Entity e) {
			CHECK(e == eRender);
		});

		auto qPath = wld.uquery().add("gameplay.render.Position");
		CHECK(qPath.count() == 1);
		qPath.each([&](ecs::Entity e) {
			CHECK(e == eRender);
		});
	}

	SUBCASE("component scope diagnostics include parent fallback") {
		TestWorld twld;

		const auto gameplay = wld.add();
		wld.name(gameplay, "gameplay");

		const auto render = wld.add();
		wld.name(render, "render");
		wld.child(render, gameplay);

		ecs::Entity gameplayPos = ecs::EntityBad;
		ecs::Entity renderPos = ecs::EntityBad;
		wld.scope(gameplay, [&] {
			gameplayPos = wld.add<Position>().entity;

			wld.scope(render, [&] {
				renderPos = wld.add<dummy::Position>().entity;

				cnt::darray<ecs::Entity> out;
				wld.resolve(out, "Position");
				CHECK(out.size() == 2);
				CHECK((out[0] == renderPos || out[1] == renderPos));
				CHECK((out[0] == gameplayPos || out[1] == gameplayPos));
				CHECK(wld.get("Position") == renderPos);
			});
		});
	}

	SUBCASE("lookup path resolves unqualified component names in configured order") {
		TestWorld twld;

		const auto gameplay = wld.add();
		wld.name(gameplay, "gameplay");

		const auto render = wld.add();
		wld.name(render, "render");
		wld.child(render, gameplay);

		const auto tools = wld.add();
		wld.name(tools, "tools");
		wld.child(tools, gameplay);

		ecs::Entity gameplayDevice = ecs::EntityBad;
		ecs::Entity toolsDevice = ecs::EntityBad;

		wld.scope(gameplay, [&] {
			gameplayDevice = add_runtime_component(wld, "Gameplay::Device", 0, ecs::DataStorageType::Table, 1).entity;
		});
		wld.scope(tools, [&] {
			toolsDevice = add_runtime_component(wld, "Tools::Device", 0, ecs::DataStorageType::Table, 1).entity;
		});

		CHECK(wld.get("Device") == ecs::EntityBad);

		const ecs::Entity lookupRender[] = {render};
		wld.lookup_path(lookupRender);
		CHECK(wld.get("Device") == gameplayDevice);

		const ecs::Entity lookupToolsRender[] = {tools, render};
		wld.lookup_path(lookupToolsRender);
		CHECK(wld.get("Device") == toolsDevice);

		const ecs::Entity lookupRenderTools[] = {render, tools};
		wld.lookup_path(lookupRenderTools);
		CHECK(wld.get("Device") == gameplayDevice);

		wld.lookup_path(std::span<const ecs::Entity>{});
		CHECK(wld.lookup_path().empty());
		CHECK(wld.get("Device") == ecs::EntityBad);
	}

	SUBCASE("lookup path resolves string queries at construction time") {
		TestWorld twld;

		const auto gameplay = wld.add();
		wld.name(gameplay, "gameplay");

		const auto render = wld.add();
		wld.name(render, "render");
		wld.child(render, gameplay);

		const auto gameplayComp = wld.add<Position>().entity;
		ecs::Entity renderComp = ecs::EntityBad;
		wld.scope(render, [&] {
			renderComp = wld.add<dummy::Position>().entity;
		});

		const auto eGameplay = wld.add();
		wld.add(eGameplay, gameplayComp);

		const auto eRender = wld.add();
		wld.add(eRender, renderComp);

		auto qDefault = wld.uquery().add("Position");
		CHECK(qDefault.count() == 1);
		qDefault.each([&](ecs::Entity e) {
			CHECK(e == eGameplay);
		});

		const ecs::Entity lookupRender[] = {render};
		wld.lookup_path(lookupRender);

		auto qLookup = wld.uquery().add("Position");
		CHECK(qLookup.count() == 1);
		qLookup.each([&](ecs::Entity e) {
			CHECK(e == eRender);
		});

		wld.lookup_path(std::span<const ecs::Entity>{});
		CHECK(qLookup.count() == 1);
		qLookup.each([&](ecs::Entity e) {
			CHECK(e == eRender);
		});
	}

	SUBCASE("module creates named scope hierarchies and scopes registration") {
		TestWorld twld;

		const auto moduleEntity = wld.module("gameplay.render");
		CHECK(moduleEntity != ecs::EntityBad);
		CHECK(wld.name(moduleEntity) == "render");
		CHECK(wld.get("gameplay.render") == moduleEntity);

		const auto gameplay = wld.get("gameplay");
		CHECK(gameplay != ecs::EntityBad);
		CHECK(wld.has(moduleEntity, ecs::Pair(ecs::ChildOf, gameplay)));

		const auto physicsModule = wld.module("gameplay.render.physics");
		CHECK(physicsModule != ecs::EntityBad);
		wld.scope(physicsModule, [&] {
			const auto& comp = wld.add<dummy::Position>();
			CHECK(wld.path(comp.entity) == "gameplay.render.physics.Position");
		});

		CHECK(wld.scope() == ecs::EntityBad);
	}

	SUBCASE("world exposes unified lookup and component naming metadata") {
		TestWorld twld;

		const auto gameplay = wld.add();
		wld.name(gameplay, "gameplay");

		const auto render = wld.add();
		wld.name(render, "render");
		wld.child(render, gameplay);

		ecs::Entity gameplayPos = ecs::EntityBad;
		ecs::Entity renderPos = ecs::EntityBad;
		wld.scope(gameplay, [&] {
			gameplayPos = wld.add<Position>().entity;

			wld.scope(render, [&] {
				renderPos = wld.add<dummy::Position>().entity;
			});
		});

		CHECK(wld.get("gameplay") == gameplay);
		CHECK(wld.get("gameplay.render") == render);
		CHECK(wld.get("gameplay.Position") == gameplayPos);
		CHECK(wld.get("gameplay.render.Position") == renderPos);
		CHECK(wld.get("Position") == gameplayPos);

		CHECK(wld.symbol(renderPos) == "dummy::Position");
		CHECK(wld.path(renderPos) == "gameplay.render.Position");
		CHECK(wld.alias(renderPos).empty());
		CHECK(wld.display_name(renderPos) == "gameplay.render.Position");

		cnt::darray<ecs::Entity> out;
		wld.resolve(out, "Position");
		CHECK(out.size() == 1);
		CHECK(out[0] == gameplayPos);

		CHECK(wld.alias(renderPos, "RenderPosition"));
		CHECK(wld.alias(renderPos) == "RenderPosition");
		CHECK(wld.display_name(renderPos) == "RenderPosition");
		CHECK(wld.get("RenderPosition") == renderPos);

		CHECK(wld.path(renderPos, "gameplay.render.RenderPosition"));
		CHECK(wld.path(renderPos) == "gameplay.render.RenderPosition");
		CHECK(wld.get("gameplay.render.RenderPosition") == renderPos);

		wld.resolve(out, "gameplay.render");
		CHECK(out.size() == 1);
		CHECK(out[0] == render);
	}

	SUBCASE("world lookup prefers ordinary entity names over scoped component matches") {
		TestWorld twld;

		const auto gameplay = wld.add();
		wld.name(gameplay, "gameplay");

		const auto render = wld.add();
		wld.name(render, "render");
		wld.child(render, gameplay);

		const auto namedEntity = wld.add();
		wld.name(namedEntity, "Device");

		ecs::Entity scopedDevice = ecs::EntityBad;
		wld.scope(render, [&] {
			scopedDevice = add_runtime_component(wld, "Render::Device", 0, ecs::DataStorageType::Table, 1).entity;
		});

		CHECK(wld.alias("Device") == ecs::EntityBad);

		wld.scope(render, [&] {
			CHECK(wld.get("Device") == namedEntity);
		});

		cnt::darray<ecs::Entity> out;
		wld.resolve(out, "Device");
		CHECK(out.size() == 1);
		CHECK(out[0] == namedEntity);
	}

	SUBCASE("world lookup prefers scoped component over global component name") {
		TestWorld twld;

		const auto gameplay = wld.add();
		wld.name(gameplay, "gameplay");

		const auto render = wld.add();
		wld.name(render, "render");
		wld.child(render, gameplay);

		const auto globalComp = add_runtime_component(wld, "Device", 0, ecs::DataStorageType::Table, 1).entity;

		ecs::Entity scopedComp = ecs::EntityBad;
		wld.scope(render, [&] {
			scopedComp = add_runtime_component(wld, "Render::Device", 0, ecs::DataStorageType::Table, 1).entity;
			CHECK(wld.get("Device") == scopedComp);
		});

		CHECK(wld.get("Device") == globalComp);
	}

	SUBCASE("world lookup prefers entity paths over component path collisions") {
		TestWorld twld;

		const auto gameplay = wld.add();
		wld.name(gameplay, "gameplay");

		const auto render = wld.add();
		wld.name(render, "render");
		wld.child(render, gameplay);

		ecs::Entity renderComp = ecs::EntityBad;
		wld.scope(gameplay, [&] {
			renderComp = add_runtime_component(wld, "Gameplay::render", 0, ecs::DataStorageType::Table, 1).entity;
		});

		CHECK(wld.path("gameplay.render") == renderComp);
		CHECK(wld.get("gameplay.render") == render);

		cnt::darray<ecs::Entity> out;
		wld.resolve(out, "gameplay.render");
		CHECK(out.size() == 2);
		CHECK((out[0] == render || out[1] == render));
		CHECK((out[0] == renderComp || out[1] == renderComp));
	}

	SUBCASE("world lookup prefers exact component symbols over aliases") {
		TestWorld twld;

		const auto exactSymbolComp =
				add_runtime_component(wld, "Gameplay::Device", 0, ecs::DataStorageType::Table, 1).entity;
		const auto aliasComp = add_runtime_component(wld, "OtherDevice", 0, ecs::DataStorageType::Table, 1).entity;
		CHECK(wld.alias(aliasComp, "Gameplay::Device"));

		CHECK(wld.alias("Gameplay::Device") == aliasComp);
		CHECK(wld.get("Gameplay::Device") == exactSymbolComp);

		cnt::darray<ecs::Entity> out;
		wld.resolve(out, "Gameplay::Device");
		CHECK(out.size() == 2);
		CHECK((out[0] == exactSymbolComp || out[1] == exactSymbolComp));
		CHECK((out[0] == aliasComp || out[1] == aliasComp));
	}

	SUBCASE("duplicate aliases are rejected") {
		NonUniqueNameOpsGuard guard;
		TestWorld twld;

		const auto entityA = wld.add();
		const auto entityB = wld.add();

		CHECK(wld.alias(entityA, "Device"));
		CHECK_FALSE(wld.alias(entityB, "Device"));
		CHECK(wld.alias(entityA) == "Device");
		CHECK(wld.alias(entityB).empty());
		CHECK(wld.alias("Device") == entityA);
		CHECK(wld.get("Device") == entityA);

		cnt::darray<ecs::Entity> out;
		wld.resolve(out, "Device");
		CHECK(out.size() == 1);
		CHECK(out[0] == entityA);
	}
}
