#pragma once
#include <gaia.h>

class WorldTest {
public:
	// Windows enforces explicit symbol import/export across DLL boundaries. Therefore, for the constructors
	// to be visible inside executables we need to also export the constuctor and destructor. Or mark the whole
	// class ENGINE_API.
	WorldTest() = default;
	virtual ~WorldTest() = default;

	GAIA_API void cleanup();

private:
	gaia::ecs::World m_world;
};
