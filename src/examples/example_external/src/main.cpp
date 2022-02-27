#include <gaia.h>

using namespace gaia;

int main() {
	ecs::World w;
	auto e = w.CreateEntity();
	LOG_N("entity %u.%u", e.id(), e.gen());
	return 0;
}