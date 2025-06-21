#include "dll_api.h"

WorldTest::WorldTest() = default;
WorldTest::~WorldTest() = default;

void WorldTest::cleanup() {
	m_world.cleanup();
}
