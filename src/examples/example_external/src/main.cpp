#include <gaia.h>

using namespace gaia;

struct Foo {};

void print_info(ecs::Entity e) {
	GAIA_LOG_N("entity %u:%u", e.id(), e.gen());
}

int main() {
	ecs::World w;
	auto e = w.add();
	auto e2 = w.add();
	w.add<Foo>(e2);
	print_info(e);

	auto q = w.query().all<Foo>();
	q.each([](ecs::Entity ent) {
		print_info(ent);
	});

	return 0;
}
