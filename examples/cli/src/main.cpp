#include "gaia/gaia.h"

using namespace std;
using namespace gaia;

GAIA_INIT

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

int main() {
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
					.AddChunkComponent<Acceleration>()); // enity with Position and chunk
																							 // component Acceleration
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
	assert(ch1 != nullptr);
	const bool ch1b = ch1->HasComponent<Rotation>();
	LOG_N(" e1 hasRotation = %d", ch1b);

	{
		bool hasRotation = world.HasAnyComponents<Acceleration>(
				e1); // true if there's Acceleration
						 // component present in archetype
		bool hasRotationAndScale = world.HasAllComponents<
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

	world.SetComponentData<Position>(e1, {1, 2, 3});

	Position p = {3, 4, 5};
	world.SetComponentData(e1, p);
	Position& pp = p;
	world.SetComponentData(e1, pp);
	world.SetComponentData(e1, std::move(p));
	world.SetComponentData<Something, Acceleration, Position>(
			e3, {false}, {1, 2, 3}, {10, 11, 12});
	world.SetChunkComponentData<Else>(e3, {false});

	{
		ecs::EntityQuery q;
		// We'll query all entities which carry Something or Else components, don't
		// carry Scale and carry chunk component Acceleration
		q.WithAny<Something, Else>()
				.WithNone<Scale>()
				.WithAllChunkComponents<Acceleration>();
		// In addition to the above both Position and Acceleration must be there. We
		// extract data for them.
		world
				.ForEach(
						q,
						[](const Position& p, const Acceleration& a) {
							LOG_N(
									"pos=[%f,%f,%f], acc=[%f,%f,%f]\n", p.x, p.y, p.z, a.x, a.y,
									a.z);
						})
				.Run(0);
	}
	{
		// We'll query all entities which carry Something or Else components, don't
		// carry Scale and carry chunk component Acceleration In addition to the
		// above both Position and Acceleration must be there. We extract data for
		// them.
		world
				.ForEach(
						ecs::EntityQuery()
								.WithAny<Something, Else>()
								.WithNone<Scale>()
								.WithAllChunkComponents<Acceleration>(),
						[](const Position& p, const Acceleration& a) {
							LOG_N(
									"pos=[%f,%f,%f], acc=[%f,%f,%f]\n", p.x, p.y, p.z, a.x, a.y,
									a.z);
						})
				.Run(0);
	}
	{
		// We iterate over all entities with Position and Acceleration components.
		// Acceleration is read-only, position is write-enabled
		float t = 1.5f;
		world
				.ForEach([t](Position& p, const Acceleration& a) {
					p.x += a.x * t;
					p.y += a.y * t;
					p.z += a.z * t;
				})
				.Run(0);

		world.Diag();
		ecs::DumpAllocatorStats();
	}

	return 0;
}
