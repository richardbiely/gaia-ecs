#define _ITERATOR_DEBUG_LEVEL 0
#include <gaia.h>

GAIA_INIT

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

using namespace gaia;

struct Position {
	float x, y, z;
};
struct Acceleration {
	float x, y, z;
};
struct Rotation {
	float x, y, z, w;
};
struct Scale {
	float x, y, z;
};
struct Something {
	bool value;
};
struct Else {
	bool value;
};

TEST_CASE("EntityNull") {
	REQUIRE_FALSE(ecs::Entity{} == ecs::EntityNull);

	REQUIRE(ecs::EntityNull == ecs::EntityNull);
	REQUIRE_FALSE(ecs::EntityNull != ecs::EntityNull);

	ecs::World w;
	REQUIRE_FALSE(w.IsEntityValid(ecs::EntityNull));

	auto e = w.CreateEntity();
	REQUIRE(e != ecs::EntityNull);
	REQUIRE(ecs::EntityNull != e);
	REQUIRE_FALSE(e == ecs::EntityNull);
	REQUIRE_FALSE(ecs::EntityNull == e);
}

TEST_CASE("Compile-time sort") {
	std::array<int, 5> in = {4, 2, 1, 3, 0};
	auto out = utils::sort(in);
	REQUIRE(out[0] == 0);
	REQUIRE(out[1] == 1);
	REQUIRE(out[2] == 2);
	REQUIRE(out[3] == 3);
	REQUIRE(out[4] == 4);
}

TEST_CASE("EntityQuery & EntityQuery2 - 2 components") {
	// Compile-time queries
	ecs::EntityQuery2<
			ecs::AllTypes<Position, Rotation>, ecs::AnyTypes<>, ecs::NoneTypes<>>
			q1;
	ecs::EntityQuery2<
			ecs::AllTypes<Rotation, Position>, ecs::AnyTypes<>, ecs::NoneTypes<>>
			q2;
	REQUIRE(decltype(q1)::all::matcherHash == decltype(q2)::all::matcherHash);
	REQUIRE(decltype(q1)::all::lookupHash == decltype(q2)::all::lookupHash);

	// Real-time queries
	ecs::EntityQuery qq1, qq2;
	qq1.All<Position, Rotation>();
	qq2.All<Rotation, Position>();
	qq1.Commit(0);
	qq2.Commit(0);
	REQUIRE(
			qq1.GetData(ecs::ComponentType::CT_Generic).hashAll ==
			qq2.GetData(ecs::ComponentType::CT_Generic).hashAll);

	// Results of both types of querries must match
	REQUIRE(
			decltype(q1)::all::matcherHash ==
			qq1.GetData(ecs::ComponentType::CT_Generic).hashAll);
}

TEST_CASE("EntityQuery & EntityQuery2 - 4 components") {
	// Compile-time queries
	ecs::EntityQuery2<
			ecs::AllTypes<Position, Rotation, Acceleration, Something>,
			ecs::AnyTypes<>, ecs::NoneTypes<>>
			q1;
	ecs::EntityQuery2<
			ecs::AllTypes<Rotation, Something, Position, Acceleration>,
			ecs::AnyTypes<>, ecs::NoneTypes<>>
			q2;
	REQUIRE(decltype(q1)::all::matcherHash == decltype(q2)::all::matcherHash);
	REQUIRE(decltype(q1)::all::lookupHash == decltype(q2)::all::lookupHash);

	// Real-time queries
	ecs::EntityQuery qq1, qq2;
	qq1.All<Position, Rotation, Acceleration, Something>();
	qq2.All<Rotation, Something, Position, Acceleration>();
	qq1.Commit(0);
	qq2.Commit(0);
	REQUIRE(
			qq1.GetData(ecs::ComponentType::CT_Generic).hashAll ==
			qq2.GetData(ecs::ComponentType::CT_Generic).hashAll);

	// Results of both types of querries must match
	REQUIRE(
			decltype(q1)::all::matcherHash ==
			qq1.GetData(ecs::ComponentType::CT_Generic).hashAll);
}

TEST_CASE("CreateEntity - no components") {
	ecs::World w;

	auto create = [&](uint32_t id) {
		auto e = w.CreateEntity();
		w.AddComponent<Position>(e);
		const bool ok = e.id() == id && e.gen() == 0;
		REQUIRE(ok);
		return e;
	};

	const uint32_t N = 100;
	for (uint32_t i = 0; i < N; i++)
		create(i);
}

TEST_CASE("CreateEntity - 1 component") {
	ecs::World w;

	auto create = [&](uint32_t id) {
		auto e = w.CreateEntity();
		const bool ok = e.id() == id && e.gen() == 0;
		REQUIRE(ok);
		return e;
	};

	const uint32_t N = 10000;
	for (uint32_t i = 0; i < N; i++)
		create(i);
}

TEST_CASE("CreateAndRemoveEntity - no components") {
	ecs::World w;

	auto create = [&](uint32_t id) {
		auto e = w.CreateEntity();
		const bool ok = e.id() == id && e.gen() == 0;
		REQUIRE(ok);
		return e;
	};
	auto remove = [&](ecs::Entity e) {
		w.DeleteEntity(e);
		auto de = w.GetEntity(e.id());
		const bool ok = de.gen() == e.gen() + 1;
		REQUIRE(ok);
		auto ch = w.GetEntityChunk(e);
		const bool ok2 = ch == nullptr;
		REQUIRE(ok2);
	};

	// 100,000 picked so we create enough entites that they overflow
	// into another chunk
	const uint32_t N = 100'000;
	std::vector<ecs::Entity> arr;
	arr.reserve(N);

	// Create entities
	for (uint32_t i = 0; i < N; i++)
		arr.push_back(create(i));
	// Remove entities
	for (uint32_t i = 0; i < N; i++)
		remove(arr[i]);
}

TEST_CASE("CreateAndRemoveEntity - 1 component") {
	ecs::World w;

	auto create = [&](uint32_t id) {
		auto e = w.CreateEntity();
		const bool ok = e.id() == id && e.gen() == 0;
		REQUIRE(ok);
		return e;
	};
	auto remove = [&](ecs::Entity e) {
		w.DeleteEntity(e);
		auto de = w.GetEntity(e.id());
		const bool ok = de.gen() == e.gen() + 1;
		REQUIRE(ok);
		auto ch = w.GetEntityChunk(e);
		const bool ok2 = ch == nullptr;
		REQUIRE(ok2);
	};

	// 100,000 picked so we create enough entites that they overflow
	// into another chunk
	const uint32_t N = 100'000;
	std::vector<ecs::Entity> arr;
	arr.reserve(N);

	for (uint32_t i = 0; i < N; i++)
		arr.push_back(create(i));
	for (uint32_t i = 0; i < N; i++)
		remove(arr[i]);
}

TEST_CASE("CreateComponent") {
	ecs::World w;

	auto e1 = w.CreateEntity();
	w.AddComponent<Position, Acceleration>(e1, {}, {});

	{
		const bool hasPosition = w.HasComponents<Position>(e1);
		REQUIRE(hasPosition);
		const bool hasAcceleration = w.HasComponents<Acceleration>(e1);
		REQUIRE(hasAcceleration);
	}
}

TEST_CASE("SetComponent - generic") {
	ecs::World w;

	constexpr uint32_t N = 100;
	std::vector<ecs::Entity> arr;
	arr.reserve(N);

		for (uint32_t i = 0; i < N; ++i) {
			arr.push_back(w.CreateEntity());
			w.AddComponent<Rotation, Scale, Else>(arr.back(), {}, {}, {});
		}

		// Default values
		for (const auto ent: arr) {
			Rotation r;
			w.GetComponent(ent, r);
			REQUIRE(r.x == 0);
			REQUIRE(r.y == 0);
			REQUIRE(r.z == 0);

			Scale s;
			w.GetComponent(ent, s);
			REQUIRE(s.x == 0);
			REQUIRE(s.y == 0);
			REQUIRE(s.z == 0);

			Else e;
			w.GetComponent(ent, e);
			REQUIRE(e.value == false);
		}

	// Modify values
	{
		ecs::EntityQuery q;
		q.All<Rotation, Scale, Else>();

		w.ForEachChunk(q, [&](ecs::Chunk& chunk) {
			 auto rotationView = chunk.ViewRW<Rotation>();
			 auto scaleView = chunk.ViewRW<Scale>();
			 auto elseView = chunk.ViewRW<Else>();

				 for (uint32_t i = 0; i < chunk.GetItemCount(); ++i) {
					 rotationView[i] = {1, 2, 3};
					 scaleView[i] = {11, 22, 33};
					 elseView[i] = {true};
				 }
		 }).Run(0);

			for (const auto ent: arr) {
				Rotation r;
				w.GetComponent(ent, r);
				REQUIRE(r.x == 1);
				REQUIRE(r.y == 2);
				REQUIRE(r.z == 3);

				Scale s;
				w.GetComponent(ent, s);
				REQUIRE(s.x == 11);
				REQUIRE(s.y == 22);
				REQUIRE(s.z == 33);

				Else e;
				w.GetComponent(ent, e);
				REQUIRE(e.value == true);
			}
	}
}

TEST_CASE("SetComponent - generic & chunk") {
	ecs::World w;

	constexpr uint32_t N = 100;
	std::vector<ecs::Entity> arr;
	arr.reserve(N);

		for (uint32_t i = 0; i < N; ++i) {
			arr.push_back(w.CreateEntity());
			w.AddComponent<Rotation, Scale, Else>(arr.back(), {}, {}, {});
			w.AddChunkComponent<Position>(arr.back(), {});
		}

		// Default values
		for (const auto ent: arr) {
			Rotation r;
			w.GetComponent(ent, r);
			REQUIRE(r.x == 0);
			REQUIRE(r.y == 0);
			REQUIRE(r.z == 0);

			Scale s;
			w.GetComponent(ent, s);
			REQUIRE(s.x == 0);
			REQUIRE(s.y == 0);
			REQUIRE(s.z == 0);

			Else e;
			w.GetComponent(ent, e);
			REQUIRE(e.value == false);

			Position p;
			w.GetChunkComponent(ent, p);
			REQUIRE(p.x == 0);
			REQUIRE(p.y == 0);
			REQUIRE(p.z == 0);
		}

	// Modify values
	{
		ecs::EntityQuery q;
		q.All<Rotation, Scale, Else>();

		w.ForEachChunk(q, [&](ecs::Chunk& chunk) {
			 auto rotationView = chunk.ViewRW<Rotation>();
			 auto scaleView = chunk.ViewRW<Scale>();
			 auto elseView = chunk.ViewRW<Else>();

			 chunk.SetChunkComponent<Position>({111, 222, 333});

				 for (uint32_t i = 0; i < chunk.GetItemCount(); ++i) {
					 rotationView[i] = {1, 2, 3};
					 scaleView[i] = {11, 22, 33};
					 elseView[i] = {true};
				 }
		 }).Run(0);

		{
			Position p;
			w.GetChunkComponent<Position>(arr[0], p);
			REQUIRE(p.x == 111);
			REQUIRE(p.y == 222);
			REQUIRE(p.z == 333);
		}
		{
				for (const auto ent: arr) {
					Rotation r;
					w.GetComponent(ent, r);
					REQUIRE(r.x == 1);
					REQUIRE(r.y == 2);
					REQUIRE(r.z == 3);

					Scale s;
					w.GetComponent(ent, s);
					REQUIRE(s.x == 11);
					REQUIRE(s.y == 22);
					REQUIRE(s.z == 33);

					Else e;
					w.GetComponent(ent, e);
					REQUIRE(e.value == true);
				}
		}
		{
			Position p;
			w.GetChunkComponent<Position>(arr[0], p);
			REQUIRE(p.x == 111);
			REQUIRE(p.y == 222);
			REQUIRE(p.z == 333);
		}
	}
}

TEST_CASE("Usage 1 - simple query, 1 component") {
	ecs::World w;

	auto e = w.CreateEntity();
	w.AddComponent<Position>(e);

	{
		uint32_t cnt = 0;
		w.ForEach([&](const Acceleration& a) { ++cnt; }).Run(0);
		REQUIRE(cnt == 0);
	}
	{
		uint32_t cnt = 0;
		w.ForEach([&](const Position& a) { ++cnt; }).Run(0);
		REQUIRE(cnt == 1);
	}

	auto e1 = w.CreateEntity(e);
	auto e2 = w.CreateEntity(e);
	auto e3 = w.CreateEntity(e);

	{
		uint32_t cnt = 0;
		w.ForEach([&](const Position& a) { ++cnt; }).Run(0);
		REQUIRE(cnt == 4);
	}

	w.DeleteEntity(e1);

	{
		uint32_t cnt = 0;
		w.ForEach([&](const Position& a) { ++cnt; }).Run(0);
		REQUIRE(cnt == 3);
	}
}

TEST_CASE("Usage 1 - simple query, 1 chunk component") {
	ecs::World w;

	auto e = w.CreateEntity();
	w.AddChunkComponent<Position>(e);

	{
		uint32_t cnt = 0;
		w.ForEach([&](const Position& a) { ++cnt; }).Run(0);
		REQUIRE(cnt == 0);
	}
	{
		uint32_t cnt = 0;
		w.ForEachChunk(
				 ecs::EntityQuery().AllChunk<Position>(),
				 [&](const ecs::Chunk& chunk) { ++cnt; })
				.Run(0);
		REQUIRE(cnt == 1);
	}

	auto e1 = w.CreateEntity(e);
	auto e2 = w.CreateEntity(e);
	auto e3 = w.CreateEntity(e);

	{
		uint32_t cnt = 0;
		w.ForEachChunk(
				 ecs::EntityQuery().AllChunk<Position>(),
				 [&](const ecs::Chunk& chunk) { ++cnt; })
				.Run(0);
		REQUIRE(cnt == 1);
	}

	w.DeleteEntity(e1);

	{
		uint32_t cnt = 0;
		w.ForEachChunk(
				 ecs::EntityQuery().AllChunk<Position>(),
				 [&](const ecs::Chunk& chunk) { ++cnt; })
				.Run(0);
		REQUIRE(cnt == 1);
	}

	w.DeleteEntity(e2);
	w.DeleteEntity(e3);
	w.DeleteEntity(e);

	{
		uint32_t cnt = 0;
		w.ForEachChunk(
				 ecs::EntityQuery().AllChunk<Position>(),
				 [&](const ecs::Chunk& chunk) { ++cnt; })
				.Run(0);
		REQUIRE(cnt == 0);
	}
}

TEST_CASE("Usage 2 - simple query, many components") {
	ecs::World w;

	auto e1 = w.CreateEntity();
	w.AddComponent<Position, Acceleration, Else>(e1, {}, {}, {});
	auto e2 = w.CreateEntity();
	w.AddComponent<Rotation, Scale, Else>(e2, {}, {}, {});
	auto e3 = w.CreateEntity();
	w.AddComponent<Position, Acceleration, Scale>(e3, {}, {}, {});

	{
		uint32_t cnt = 0;
		w.ForEach([&](const Position& a) { ++cnt; }).Run(0);
		REQUIRE(cnt == 2);
	}
	{
		uint32_t cnt = 0;
		w.ForEach([&](const Acceleration& a) { ++cnt; }).Run(0);
		REQUIRE(cnt == 2);
	}
	{
		uint32_t cnt = 0;
		w.ForEach([&](const Rotation& a) { ++cnt; }).Run(0);
		REQUIRE(cnt == 1);
	}
	{
		uint32_t cnt = 0;
		w.ForEach([&](const Scale& a) { ++cnt; }).Run(0);
		REQUIRE(cnt == 2);
	}
	{
		uint32_t cnt = 0;
		w.ForEach([&](const Position& p, const Acceleration& s) { ++cnt; }).Run(0);
		REQUIRE(cnt == 2);
	}
	{
		uint32_t cnt = 0;
		w.ForEach([&](const Position& p, const Scale& s) { ++cnt; }).Run(0);
		REQUIRE(cnt == 1);
	}
	{
		ecs::EntityQuery q;
		q.Any<Position, Acceleration>();

		uint32_t cnt = 0;
		w.ForEachChunk(q, [&](ecs::Chunk& chunk) {
			 ++cnt;

			 const bool ok1 =
					 chunk.HasComponent<Position>() || chunk.HasComponent<Acceleration>();
			 REQUIRE(ok1);
			 const bool ok2 =
					 chunk.HasComponent<Acceleration>() || chunk.HasComponent<Position>();
			 REQUIRE(ok2);
		 }).Run(0);
		REQUIRE(cnt == 2);
	}
	{
		ecs::EntityQuery q;
		q.Any<Position, Acceleration>().All<Scale>();

		uint32_t cnt = 0;
		w.ForEachChunk(q, [&](ecs::Chunk& chunk) {
			 ++cnt;

			 REQUIRE(chunk.GetItemCount() == 1);

			 const bool ok1 =
					 chunk.HasComponent<Position>() || chunk.HasComponent<Acceleration>();
			 REQUIRE(ok1);
			 const bool ok2 =
					 chunk.HasComponent<Acceleration>() || chunk.HasComponent<Position>();
			 REQUIRE(ok2);
		 }).Run(0);
		REQUIRE(cnt == 1);
	}
	{
		ecs::EntityQuery q;
		q.Any<Position, Acceleration>().None<Scale>();

		uint32_t cnt = 0;
		w.ForEachChunk(q, [&](ecs::Chunk& chunk) {
			 ++cnt;

			 REQUIRE(chunk.GetItemCount() == 1);
		 }).Run(0);
		REQUIRE(cnt == 1);
	}
}

TEST_CASE("Usage 2 - simple query, many chunk components") {
	ecs::World w;

	auto e1 = w.CreateEntity();
	w.AddChunkComponent<Position, Acceleration, Else>(e1, {}, {}, {});
	auto e2 = w.CreateEntity();
	w.AddChunkComponent<Rotation, Scale, Else>(e2, {}, {}, {});
	auto e3 = w.CreateEntity();
	w.AddChunkComponent<Position, Acceleration, Scale>(e3, {}, {}, {});

	{
		uint32_t cnt = 0;
		w.ForEachChunk(
				 ecs::EntityQuery().AllChunk<Position>(),
				 [&](const ecs::Chunk& chunk) { ++cnt; })
				.Run(0);
		REQUIRE(cnt == 2);
	}
	{
		uint32_t cnt = 0;
		w.ForEachChunk(
				 ecs::EntityQuery().AllChunk<Acceleration>(),
				 [&](const ecs::Chunk& chunk) { ++cnt; })
				.Run(0);
		REQUIRE(cnt == 2);
	}
	{
		uint32_t cnt = 0;
		w.ForEachChunk(
				 ecs::EntityQuery().AllChunk<Rotation>(),
				 [&](const ecs::Chunk& chunk) { ++cnt; })
				.Run(0);
		REQUIRE(cnt == 1);
	}
	{
		uint32_t cnt = 0;
		w.ForEachChunk(
				 ecs::EntityQuery().AllChunk<Else>(),
				 [&](const ecs::Chunk& chunk) { ++cnt; })
				.Run(0);
		REQUIRE(cnt == 2);
	}
	{
		uint32_t cnt = 0;
		w.ForEachChunk(
				 ecs::EntityQuery().AllChunk<Scale>(),
				 [&](const ecs::Chunk& chunk) { ++cnt; })
				.Run(0);
		REQUIRE(cnt == 2);
	}
	{
		ecs::EntityQuery q;
		q.AnyChunk<Position, Acceleration>();

		uint32_t cnt = 0;
		w.ForEachChunk(q, [&](ecs::Chunk& chunk) {
			 ++cnt;

			 const bool ok1 = chunk.HasChunkComponent<Position>() ||
												chunk.HasChunkComponent<Acceleration>();
			 REQUIRE(ok1);
			 const bool ok2 = chunk.HasChunkComponent<Acceleration>() ||
												chunk.HasChunkComponent<Position>();
			 REQUIRE(ok2);
		 }).Run(0);
		REQUIRE(cnt == 2);
	}
	{
		ecs::EntityQuery q;
		q.AnyChunk<Position, Acceleration>().AllChunk<Scale>();

		uint32_t cnt = 0;
		w.ForEachChunk(q, [&](ecs::Chunk& chunk) {
			 ++cnt;

			 REQUIRE(chunk.GetItemCount() == 1);

			 const bool ok1 = chunk.HasChunkComponent<Position>() ||
												chunk.HasChunkComponent<Acceleration>();
			 REQUIRE(ok1);
			 const bool ok2 = chunk.HasChunkComponent<Acceleration>() ||
												chunk.HasChunkComponent<Position>();
			 REQUIRE(ok2);
		 }).Run(0);
		REQUIRE(cnt == 1);
	}
	{
		ecs::EntityQuery q;
		q.AnyChunk<Position, Acceleration>().NoneChunk<Scale>();

		uint32_t cnt = 0;
		w.ForEachChunk(q, [&](ecs::Chunk& chunk) {
			 ++cnt;

			 REQUIRE(chunk.GetItemCount() == 1);
		 }).Run(0);
		REQUIRE(cnt == 1);
	}
}

TEST_CASE("Example") {

	ecs::World world;

	// Creation query

	LOG_N(">>>>>> %s", utils::type_info::full_name<Else>());
	LOG_N(
			">>>>>> %.*s", (int)utils::type_info::name<Else>().length(),
			utils::type_info::name<Else>().data());
	LOG_N(">>>>>> %llu", utils::type_info::hash<Else>());

	ecs::CreationQuery q1;
	q1.AddComponent<Position, Acceleration, Rotation, Something>();
	ecs::CreationQuery q2;
	q2.AddComponent<Position, Acceleration, Something>()
			.AddChunkComponent<Something>();

	// Entity creation

	auto e1 = world.CreateEntity(q1); // entity form creation query
	auto e3 = world.CreateEntity(ecs::CreationQuery()
																	 .AddComponent<
																			 Position, Acceleration,
																			 Something>()); // entity with Position,
																											// Acceleration, Something
	auto e4 = world.CreateEntity(
			ecs::CreationQuery()
					.AddComponent<
							Something, Acceleration,
							Position>()); // entity with Position, Acceleration,
														// Something (matches the above)
	auto e5 = world.CreateEntity(
			ecs::CreationQuery()
					.AddComponent<Position>()
					.AddChunkComponent<Acceleration>()); // enity with Position and
																							 // chunk component
																							 // Acceleration
	(void)e5;

	// Adding components

	// world.AddComponent<Acceleration, Position>(e1);
	// world.AddComponent<Scale, Something>(e4, { 1,2,3 }, { true }); // Add
	// components and set their values right away
	world.AddChunkComponent<Else>(
			e3, {false}); // Add a chunk component Else and set its value to false

	auto e6 = world.CreateEntity(
			e4); // clones enity e4 along with all its generic component data
	(void)e6;
	// Removing components

	world.RemoveComponent<Acceleration>(e1);
	world.RemoveChunkComponent<Acceleration>(e1);

	// Checking for presence of components
	const auto ch1 = world.GetEntityChunk(e1);
	GAIA_ASSERT(ch1 != nullptr);
	const bool ch1b = ch1->HasComponent<Rotation>();
	LOG_N(" e1 hasRotation = %d", ch1b);

	{
		bool hasRotation = world.HasAnyComponents<Acceleration>(
				e1); // true if there's Acceleration
						 // component present in archetype
		bool hasRotationAndScale = world.HasComponents<
				Acceleration,
				Scale>(e1); // true if there're both Acceleration and
										// Scale component present in archetype
		bool hasRotationOrScale = world.HasAnyComponents<
				Acceleration,
				Scale>(e1); // true if either Acceleration or Scale
										// component are present in archetype
		bool hasNoRotationAndNoScale = world.HasNoneComponents<
				Acceleration,
				Scale>(e1); // true if neither Acceleration not Scale
										// component are present in archetype
		bool hasNoChunkRotation = world.HasNoneChunkComponents<Acceleration>(
				e1); // true if chunk component
						 // Acceleration is not
						 // present in archetype

		LOG_N("Archetype e1");
		LOG_N(" hasRotation = %d", hasRotation);
		LOG_N(" hasRotationAndScale = %d", hasRotationAndScale);
		LOG_N(" hasRotationOrScale = %d", hasRotationOrScale);
		LOG_N(" hasNoRotationAndNoScale = %d", hasNoRotationAndNoScale);
		LOG_N(" hasNoChunkRotation = %d", hasNoChunkRotation);
	}

	// Setting component data

	world.SetComponent<Position>(e1, {1, 2, 3});
	Position e1p;
	world.GetComponent<Position>(e1, e1p);
	LOG_N("e1 position = [%.2f,%.2f,%.2f]", e1p.x, e1p.y, e1p.z);

	Position p = {3, 4, 5};
	world.SetComponent(e1, p);
	Position& pp = p;
	world.SetComponent(e1, pp);
	world.SetComponent(e1, std::move(p));
	world.SetComponent<Something, Acceleration, Position>(
			e3, {false}, {1, 2, 3}, {10, 11, 12});
	world.SetChunkComponent<Else>(e3, {false});

	{
		ecs::EntityQuery q;
		// We'll query all entities which carry Something or Else components,
		// don't carry Scale and carry chunk component Acceleration
		q.Any<Something, Else>().None<Scale>().AllChunk<Acceleration>();
		// In addition to the above both Position and Acceleration must be
		// there. We extract data for them.
		world
				.ForEach(
						q,
						[](const Position& p, const Acceleration& a) {
							LOG_N(
									"pos=[%f,%f,%f], acc=[%f,%f,%f]", p.x, p.y, p.z, a.x, a.y,
									a.z);
						})
				.Run(0);
	}
	{
		ecs::EntityQuery q;
		// We'll query all entities which carry Something or Else components,
		// don't carry Scale and carry chunk component Acceleration
		q.Any<Something, Else>().None<Scale>().AllChunk<Acceleration>();
		// In addition to the above both Position and Acceleration must be
		// there. We extract data for them.
		world
				.ForEachChunk(
						q,
						[](ecs::Chunk& chunk) {
							auto pp = chunk.View<Position>();
							auto aa = chunk.View<Acceleration>();

								for (uint16_t i = 0; i < chunk.GetItemCount(); i++) {
									const Position& p = pp[i];
									const Acceleration& a = aa[i];
									LOG_N(
											"pos=[%f,%f,%f], acc=[%f,%f,%f]", p.x, p.y, p.z, a.x, a.y,
											a.z);
								}
						})
				.Run(0);
	}
	{
		// We'll query all entities which carry Something or Else components,
		// don't carry Scale and carry chunk component Acceleration In addition
		// to the above both Position and Acceleration must be there. We extract
		// data for them.
		world
				.ForEach(
						ecs::EntityQuery()
								.Any<Something, Else>()
								.None<Scale>()
								.AllChunk<Acceleration>(),
						[](const Position& p, const Acceleration& a) {
							LOG_N(
									"pos=[%f,%f,%f], acc=[%f,%f,%f]", p.x, p.y, p.z, a.x, a.y,
									a.z);
						})
				.Run(0);
	}
	{
		// We iterate over all entities with Position and Acceleration
		// components. Acceleration is read-only, position is write-enabled
		float t = 1.5f;
		world
				.ForEach([t](Position& p, const Acceleration& a) {
					p.x += a.x * t;
					p.y += a.y * t;
					p.z += a.z * t;
				})
				.Run(0);
		(void)t;
	}

	ecs::EntityQuery2<
			ecs::AllTypes<Acceleration>, ecs::NoneTypes<>,
			ecs::AnyTypes<Position, Rotation>>
			qq;
	DiagQuery(qq);

	// ecs::EntityQuery2<ecs::AllTypes<Acceleration>> qqq;
	// DiagQuery(qqq);

	// Delete some entity
	world.DeleteEntity(e1);
	world.DeleteEntity(e3);

	world.Diag();
	REQUIRE(true);
}