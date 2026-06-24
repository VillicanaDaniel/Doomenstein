#define WIN32_LEAN_AND_MEAN	// Always #define this before #including <windows.h>
#include <windows.h>	// #include this (massive, platform-specific) header in VERY few places (and .CPPs only)

#include <math.h>
#include <cassert>
#include <crtdbg.h>

#include "Game/App.hpp"
#include "Engine/Core/Engine.hpp"
#include "Engine/Input/InputSystem.hpp"

//-----------------------------------------------------------------------------------------------
// #SD1ToDo: Later we will move this useful macro to a more central place, e.g. Engine/Core/EngineCommon.hpp
//
#define UNUSED(x) (void)(x);

HDC g_displayDeviceContext = nullptr;				// ...becomes void* Window::m_displayContext

//-----------------------------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE applicationInstanceHandle, HINSTANCE, LPSTR commandLineString, int)
{
	UNUSED(applicationInstanceHandle);
	UNUSED(commandLineString);

	/*CreateOSWindow(applicationInstanceHandle, CLIENT_ASPECT);*/

	g_theApp = new App();

	while (!g_theApp->IsQuitting())			// #SD1ToDo: ...becomes:  !g_theApp->IsQuitting()
	{
		g_theApp->RunFrame();
	}

	delete g_theApp;
	g_theApp = nullptr;

	return 0;
}