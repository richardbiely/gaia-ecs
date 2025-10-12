#pragma once
#include <gaia.h>

#ifdef BUILDING_DLL
	#define ENGINE_API GAIA_EXPORT
#else
	#define ENGINE_API GAIA_IMPORT
#endif

class WorldTest {
public:
	WorldTest();
	virtual ~WorldTest();

	ENGINE_API void cleanup();

private:
	gaia::ecs::World m_world;
};
