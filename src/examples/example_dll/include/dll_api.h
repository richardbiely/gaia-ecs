#pragma once
#include <gaia.h>

#ifdef BUILDING_DLL
	#define ENGINE_API GAIA_EXPORT
#else
	#define ENGINE_API GAIA_IMPORT
#endif

class WorldTest {
public:
	// Windows enforces explicit symbol import/export across DLL boundaries. Therefore, for the constructors
	// to be visible inside executables we need to also export the constuctor and destructor. Or mark the whole
	// class ENGINE_API.
	WorldTest() = default;
	virtual ~WorldTest() = default;

	ENGINE_API void cleanup();

private:
	gaia::ecs::World m_world;
};
