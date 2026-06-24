#pragma once
#include "Engine/Core/Engine.hpp"
#include "Engine/Core/EventSystem.hpp"
#include "Engine/Renderer/Camera.hpp"

class App;
class Game;
class Clock;

extern App* g_theApp;

class App
{
	public:
		App();
		~App();

		void RunFrame();
		void Update();
		void Render() const;

		void SetIsQuitting();
		static bool SetIsQuittingEvent(EventArgs &args);
		bool IsQuitting() const;
		void UpdateFromKeyboard();
		bool HandleKeyDown(EventArgs& args);
		static bool OnKeyDownEvent(EventArgs& args);

	public:
		Camera* m_gameCamera = nullptr;
		Game* m_game = nullptr;

		bool m_isQuitting = false;
		bool m_isPaused = false;
		bool m_isSlowMo = false;
		bool m_isFastMo = false;
		bool m_pauseAfterNextUpdate = false;
		bool m_stepSingleFrameRequested = false;

		float m_lastFrameTime = 0;
		float m_timeScale = 1.f;
		float m_timeScaleBeforePause = 1.f;

		Clock* m_systemClock = nullptr;
};