#pragma once
#include <gaia.h>

#ifdef _WIN32
	#ifdef BUILDING_DLL
		#define ENGINE_API __declspec(dllexport)
	#else
		#define ENGINE_API __declspec(dllimport)
	#endif
#else
	#define ENGINE_API
#endif

class WorldTest {
public:
	WorldTest();
	virtual ~WorldTest();

	ENGINE_API void cleanup();

private:
	gaia::ecs::World m_world;
};
