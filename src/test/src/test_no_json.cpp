#include <cstdint>

#if !defined(GAIA_JSON_ENABLED)
	#error GAIA_JSON_ENABLED must be defined by the build
#endif
#if GAIA_JSON_ENABLED
	#error This target verifies a JSON-disabled build
#endif

// Fail compilation if JSON declarations leak through the umbrella, source, or generated headers.
#define ser_json static_assert(false, "JSON support must not be included")
#define RuntimeJsonEncoding static_assert(false, "JSON metadata must not be included")
#include <gaia.h>
#if !GAIA_TEST_SINGLE_HEADER
	#include <gaia/ecs/impl/world_json.h>
	#include <gaia/ecs/impl/world_json_patch.h>
	#include <gaia/ecs/impl/world_schema_json.h>
	#include <gaia/ser/ser_json.h>
#endif
#undef RuntimeJsonEncoding
#undef ser_json

int main() {
	gaia::ser::ser_buffer_binary binary;
	(void)binary;

	gaia::ecs::World world;

	const auto entity = world.add();
	if (!world.has(entity))
		return 1;

	world.del(entity);
	return world.has(entity) ? 2 : 0;
}
