#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "Game/App.hpp"
#include "Game/Game.hpp"
#include "Engine/Core/Engine.hpp"
#include "Engine/Core/Clock.hpp"
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Core/Time.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/DebugRenderSystem.hpp"
#include "Engine/Math/AABB3.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Renderer/Camera.hpp"



App* g_theApp = nullptr;

App::App()
{
	g_theApp = this;
	EngineConfig config;
	config.m_windowConfig.m_clientAspect = 2.f;
	config.m_windowConfig.m_windowTitle = "Doomenstein";
	g_engine = new Engine( config );

	DebugRenderConfig debugConfig;
	debugConfig.m_renderer = g_engine->m_render;
	DebugRenderSystemStartup(debugConfig);

	m_systemClock = new Clock();
	m_systemClock->SetTimeScale(1.0);

	m_game = new Game();

	g_engine->m_event->SubscribeEventCallbackFunction("Quit", &App::SetIsQuittingEvent);
	g_engine->m_event->SubscribeEventCallbackFunction("KeyPressed", &App::OnKeyDownEvent);
}

App::~App()
{
	delete m_game;
	m_game = nullptr;

	DebugRenderSystemShutdown();

	delete g_engine;
	g_engine = nullptr;
}

void App::RunFrame()
{
	m_systemClock->Tick();

	g_engine->BeginFrame();
	DebugRenderBeginFrame();

	Update();
	Render();

	DebugRenderEndFrame();
	g_engine->EndFrame();
}


void App::Update()
{
	m_gameCamera = m_game->m_worldCamera;
	UpdateFromKeyboard();
	bool hasFocus = g_engine->m_window->HasFocus();
	bool consoleOpen = g_engine->m_devConsole->IsOpen();
	bool inAttract = m_game->IsInAttractMode();

	if (!hasFocus || consoleOpen || inAttract)
	{
		g_engine->m_input->SetCursorMode(CursorMode::POINTER);
	}
	else
	{
		g_engine->m_input->SetCursorMode(CursorMode::FPS);
	}

	if (m_stepSingleFrameRequested && m_isPaused)
	{
		m_stepSingleFrameRequested = false;
		m_isPaused = false;
		m_pauseAfterNextUpdate = true;

		float restore = (m_timeScaleBeforePause > 0.0f) ? m_timeScaleBeforePause : 1.0f;
		m_game->GetClock()->SetTimeScale(restore);
	}

	if (m_isPaused)
	{
		m_game->GetClock()->SetTimeScale(0.0f);
	}

	m_game->Update();

	if (m_pauseAfterNextUpdate)
	{
		m_pauseAfterNextUpdate = false;
		m_isPaused = true;
		m_game->GetClock()->SetTimeScale(0.0f);
	}
}


void App::Render() const
{
	g_engine->m_render->BeginCamera(*m_gameCamera);

	m_game->Render();

	g_engine->m_render->EndCamera(*m_gameCamera);
}


void App::SetIsQuitting()
{
	m_isQuitting = true;
}


bool App::SetIsQuittingEvent([[maybe_unused]] EventArgs& args)
{
	g_theApp->m_isQuitting = true;
	return true;
}


bool App::IsQuitting() const
{
	return m_isQuitting;
}


void App::UpdateFromKeyboard()
{
	InputSystem* input = g_engine->m_input;

	if (input->WasKeyJustPressed(KEYCODE_ESC) && m_game->m_gamemode == GAMEMODE_ATTRACT)
	{
		SetIsQuitting();
	}
}

bool App::HandleKeyDown(EventArgs& args)
{
	unsigned long long keyCodeULL = args.GetValue("keyCode", 0ULL);
	unsigned char keyCode = (unsigned char)keyCodeULL;
	if (keyCode == 'P')
	{
		m_game->GetClock()->TogglePause();
		return true;
	}

	if (keyCode == 'O')
	{
		m_stepSingleFrameRequested = true;
		m_game->GetClock()->StepSingleFrame();
		return true;
	}

	if (keyCode == 'T')
	{
		float current = m_isPaused ? m_timeScaleBeforePause : (float)m_game->GetClock()->GetTimeScale();
		float next = (fabsf(current - 1.f) < 0.0001f) ? 0.1f : 1.f;

		if (m_isPaused)
		{
			m_timeScaleBeforePause = next;
		}
		else
		{
			m_game->GetClock()->SetTimeScale(next);
		}
		return true;
	}
	return false;
}

bool App::OnKeyDownEvent(EventArgs& args)
{
	return g_theApp->HandleKeyDown(args);
}
