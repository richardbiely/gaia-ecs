#include <gaia.h>

using namespace gaia;

int main() {
	ecs::World w;
	auto e = w.CreateEntity();
	GAIA_LOG_N("entity %u.%u", e.id(), e.gen());
	return 0;
}
