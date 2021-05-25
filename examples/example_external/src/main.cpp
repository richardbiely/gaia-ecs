#define _ITERATOR_DEBUG_LEVEL 0
#include <gaia.h>

GAIA_INIT

using namespace gaia;

int main() {
	ecs::World w;
	auto e = w.CreateEntity();
	LOG_N("entity %u.%u", e.id(), e.gen());
	return 0;
}