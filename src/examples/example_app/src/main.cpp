#include "dll_api.h"
#include "gaia.h"

using namespace gaia;

int main() {
	// Create a custom app world
	ecs::World w;
	// Create a dll world
	WorldTest dll_world;

	w.cleanup();
	dll_world.cleanup();
	return 0;
}