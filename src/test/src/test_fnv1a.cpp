#include <cstdint>
#include <cstring>

#if !defined(GAIA_ECS_HASH)
	#error GAIA_ECS_HASH must be defined by the build
#endif

#include <gaia.h>

#if GAIA_ECS_HASH != GAIA_ECS_HASH_FNV1A
	#error This target verifies a build that hashes with FNV-1a
#endif

namespace {
	struct Position {
		float x, y;
	};

	//! FNV-1a of @a size bytes at @a bytes, each read as unsigned.
	uint64_t fnv1a(const uint8_t* bytes, uint32_t size) {
		uint64_t hash = 0xcbf29ce484222325ULL;
		for (uint32_t i = 0; i < size; ++i)
			hash = (hash ^ bytes[i]) * 0x100000001b3ULL;
		return hash;
	}
} // namespace

int main() {
	// Both string overloads are FNV-1a of the bytes, read as unsigned wherever char is signed.
	static_assert(gaia::core::calculate_hash64("gaia") == gaia::core::calculate_hash64("gaia", 4));
	char text[20];
	uint8_t bytes[20];
	for (uint32_t i = 0; i < 19; ++i) {
		bytes[i] = (uint8_t)(0xf0U - i * 13U);
		memcpy(&text[i], &bytes[i], 1);
	}
	text[19] = 0;
	for (uint32_t len = 0; len <= 19; ++len) {
		if (gaia::core::calculate_hash64(text, len) != fnv1a(bytes, len))
			return 1;
	}
	if (gaia::core::calculate_hash64(text) != fnv1a(bytes, 19))
		return 2;

	// The ECS runs on it: components register, entities take them, queries and names find them.
	gaia::ecs::World world;
	const auto entity = world.add();
	world.add<Position>(entity, {1.0f, 2.0f});
	world.name(entity, "fnv1a");
	if (world.get("fnv1a") != entity)
		return 3;
	uint32_t found = 0;
	world.query().all<Position>().each([&](const Position& position) {
		found += position.y == 2.0f ? 1U : 0U;
	});
	return found == 1 ? 0 : 4;
}
