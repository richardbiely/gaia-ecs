#include "dll_api.h"

WorldTest::WorldTest() {}
WorldTest::~WorldTest() {}

void WorldTest::cleanup() {
	m_world.cleanup();
}
