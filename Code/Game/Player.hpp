#pragma once

#include "Game/Controller.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/EulerAngles.hpp"
#include "Engine/Math/AABB2.hpp"	
#include <string>

class Camera;
struct Rgba8;

class Player : public Controller
{
public:
	explicit Player(Map* map);
	virtual ~Player();
	virtual void Update() override;
	void UpdateKeyboardMouseInput();
	void UpdateControllerInput();

	void RenderDeathOverlay(AABB2 const& screenBounds) const;
	void RenderHUDText(AABB2 const& screenBounds) const;
	void DrawHUDText(std::string const& text, Vec2 const& position, float cellHeight, Rgba8 const& color) const;
	void UpdateSensitivity();

public:
	bool m_isFreeFlyMode = false;
	bool m_disableMovement = false;

	float m_yawPerPixel = 0.035f;
	float m_pitchPerPixel = 0.035f;

	Vec3 m_freeFlyPosition = Vec3::ZERO;
	EulerAngles m_freeFlyOrientation = EulerAngles(0.f, 0.f, 0.f);

	int m_playerIndex = 0;
	int m_controllerIndex = -1;

	Camera* m_worldCamera = nullptr;
	Camera* m_screenCamera = nullptr;

	AABB2 m_viewport = AABB2::ZERO_TO_ONE;

	int m_kills = 0;
	int m_deaths = 0;
};