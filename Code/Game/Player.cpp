#include "Game/Player.hpp"
#include "Game/Map.hpp"
#include "Game/Game.hpp"
#include "Game/Actor.hpp"
#include "Engine/Core/Engine.hpp"
#include "Engine/Core/Clock.hpp"
#include "Engine/Renderer/Camera.hpp"
#include "Engine/Renderer/BitmapFont.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Math/Mat44.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Core/DebugRenderSystem.hpp"
#include "Engine/Input/InputSystem.hpp"

Player::Player(Map* map)
	: Controller(map)
{
	m_worldCamera = new Camera();
	Mat44 cameraToRender = Mat44(Vec3(0, 0, 1), Vec3(-1, 0, 0), Vec3(0, 1, 0), Vec3(0, 0, 0));
	m_worldCamera->SetCameraToRenderTransform(cameraToRender);

	m_screenCamera = new Camera();
	m_screenCamera->SetOrthographicView(
		Vec2(0.f, 0.f),
		Vec2((float)g_engine->m_window->GetClientDimensions().x,
			(float)g_engine->m_window->GetClientDimensions().y)
	);
}

Player::~Player()
{
	delete m_worldCamera;
	delete m_screenCamera;

	m_worldCamera = nullptr;
	m_screenCamera = nullptr;
}

void Player::Update()
{
	if (g_engine->m_window == nullptr || !g_engine->m_window->HasFocus())
	{
		return;
	}

	Actor* actor = GetActor();

	if (m_isFreeFlyMode && m_worldCamera != nullptr)
	{
		m_worldCamera->SetPositionAndOrientation(m_freeFlyPosition, m_freeFlyOrientation);

		IntVec2 dims = g_engine->m_window->GetClientDimensions();
		float aspect = (dims.y > 0.f) ? ((float)dims.x / (float)dims.y) : 1.f;

		m_worldCamera->SetPerspectiveView(
			aspect,
			60.f,
			0.1f,
			300.f
		);
	}
	else if (actor != nullptr && m_worldCamera != nullptr)
	{
		Vec3 cameraPos = actor->m_position;

		if (actor->m_isDead)
		{
			float deathDuration = actor->m_corpseLifetime;
			if (deathDuration <= 0.f)
			{
				deathDuration = 1.f;
			}

			float deathT = actor->m_timeSinceDeath / deathDuration;
			deathT = GetClamped(deathT, 0.f, 1.f);

			float currentEyeHeight = Interpolate(actor->m_eyeHeight, 0.05f, deathT);
			cameraPos.z += currentEyeHeight;
		}
		else
		{
			cameraPos.z += actor->m_eyeHeight;
		}

		m_worldCamera->SetPositionAndOrientation(cameraPos, actor->m_orientation);

		IntVec2 dims = g_engine->m_window->GetClientDimensions();
		float aspect = (dims.y > 0.f) ? (dims.x / dims.y) : 1.f;

		m_worldCamera->SetPerspectiveView(
			aspect,
			actor->m_cameraFOVDegrees,
			0.1f,
			300.f
		);
	}

	if (m_screenCamera != nullptr)
	{
		IntVec2 dims = g_engine->m_window->GetClientDimensions();

		m_screenCamera->SetOrthographicView(
			Vec2(0.f, 0.f),
			Vec2((float)dims.x, (float)dims.y)
		);
	}

	if (actor == nullptr || actor->m_isDead)
	{
		return;
	}

	if (m_controllerIndex < 0)
	{
		UpdateKeyboardMouseInput();
		UpdateSensitivity();
	}
	else
	{
		UpdateControllerInput();
	}
}

void Player::UpdateKeyboardMouseInput()
{
	InputSystem* input = g_engine->m_input;
	float dt = (float)m_map->m_game->m_gameClock->GetDeltaSeconds();

	Actor* actor = GetActor();
	if (actor == nullptr)
	{
		return;
	}

	// Toggle free-fly / first-person
	if (input->WasKeyJustPressed('F'))
	{
		bool isSinglePlayer = (m_map != nullptr && m_map->m_players.size() == 1);

		if (isSinglePlayer)
		{
			m_isFreeFlyMode = !m_isFreeFlyMode;

			if (m_isFreeFlyMode)
			{
				m_freeFlyPosition = actor->m_position;
				m_freeFlyPosition.z += actor->m_eyeHeight;
				m_freeFlyOrientation = actor->m_orientation;
			}
		}
	}

	// Possess next actor
	if (input->WasKeyJustPressed('N'))
	{
		m_map->DebugPossessNext();
		actor = GetActor();
		if (actor == nullptr)
		{
			return;
		}

		if (!m_isFreeFlyMode)
		{
			m_freeFlyOrientation = actor->m_orientation;
		}
	}


	Vec2 mouseDelta = input->GetCursorClientDelta();
	float yawPerPixel = m_yawPerPixel;
	float pitchPerPixel = m_pitchPerPixel;

	if (m_isFreeFlyMode)
	{
		m_freeFlyOrientation.m_yawDegrees -= mouseDelta.x * yawPerPixel;
		m_freeFlyOrientation.m_pitchDegrees += mouseDelta.y * pitchPerPixel;

		m_freeFlyOrientation.m_pitchDegrees = GetClamped(m_freeFlyOrientation.m_pitchDegrees, -85.f, 85.f);
	}
	else
	{
		actor->AddYawInput(-mouseDelta.x * yawPerPixel);
		actor->AddPitchInput(mouseDelta.y * pitchPerPixel);
	}

	float forward = 0.f;
	float right = 0.f;
	float up = 0.f;

	if (input->IsKeyDown('W'))
		forward += 1.f;
	if (input->IsKeyDown('S'))
		forward -= 1.f;
	if (input->IsKeyDown('D'))
		right += 1.f;
	if (input->IsKeyDown('A'))
		right -= 1.f;

	if (m_isFreeFlyMode)
	{
		if (input->IsKeyDown('C'))
			up += 1.f;
		if (input->IsKeyDown('Z'))
			up -= 1.f;
	}

	if (m_isFreeFlyMode)
	{
		float moveSpeed = (input->IsKeyDown(KEYCODE_SHIFT)) ? 15.f : 1.f;

		Vec3 iForward;
		Vec3 jLeft;
		Vec3 kUp;
		m_freeFlyOrientation.GetAsVectors_IFwd_JLeft_KUp(iForward, jLeft, kUp);
		Vec3 iRight = -jLeft;

		Vec3 moveDir = Vec3::ZERO;
		moveDir += iForward * forward;
		moveDir += iRight * right;
		moveDir += Vec3(0.f, 0.f, 1.f) * up;

		if (moveDir.GetLengthSquared() > 0.f)
		{
			moveDir = moveDir.GetNormalized();
			m_freeFlyPosition += moveDir * moveSpeed * dt;
		}
	}
	else
	{
		float moveSpeed = (input->IsKeyDown(KEYCODE_SHIFT))
			? actor->m_runSpeed
			: actor->m_walkSpeed;

		Vec3 iForward;
		Vec3 jLeft;
		Vec3 kUp;
		EulerAngles moveOrientation = actor->m_orientation;
		moveOrientation.m_pitchDegrees = 0.f;
		moveOrientation.m_rollDegrees = 0.f;
		moveOrientation.GetAsVectors_IFwd_JLeft_KUp(iForward, jLeft, kUp);
		Vec3 iRight = -jLeft;

		Vec3 moveDir = Vec3::ZERO;
		moveDir += iForward * forward;
		moveDir += iRight * right;

		if (moveDir.GetLengthSquared() > 0.f)
		{
			actor->MoveInDirection(moveDir, moveSpeed);
		}

		if (input->IsKeyDown(KEYCODE_LEFT_MOUSE))
		{
			actor->Attack();
		}
	}

	if (input->WasKeyJustPressed('1'))
	{
		actor->EquipWeapon(0);
	}
	if (input->WasKeyJustPressed('2'))
	{
		actor->EquipWeapon(1);
	}
}

void Player::UpdateControllerInput()
{
	InputSystem* input = g_engine->m_input;
	XboxController const& controller = input->GetController(0);
	float dt = (float)m_map->m_game->m_gameClock->GetDeltaSeconds();

	Actor* actor = GetActor();
	if (actor == nullptr)
	{
		return;
	}

	Vec2 rightStick = controller.GetRightStick().GetPosition();
	float turnRate = m_isFreeFlyMode ? 180.f : actor->m_turnSpeed;

	if (m_isFreeFlyMode)
	{
		m_freeFlyOrientation.m_yawDegrees -= rightStick.x * turnRate * dt;
		m_freeFlyOrientation.m_pitchDegrees -= rightStick.y * turnRate * dt;

		m_freeFlyOrientation.m_pitchDegrees = GetClamped(m_freeFlyOrientation.m_pitchDegrees, -85.f, 85.f);
	}
	else
	{
		actor->AddYawInput(-rightStick.x * turnRate * dt);
		actor->AddPitchInput(-rightStick.y * turnRate * dt);
	}
	float forward = 0.f;
	float right = 0.f;
	float up = 0.f;

	Vec2 leftStick = controller.GetLeftStick().GetPosition();
	forward += leftStick.y;
	right += leftStick.x;

	if (m_isFreeFlyMode)
	{
		if (controller.IsButtonDown(XboxButtonID::RIGHT_SHOULDER))
			up += 1.f;
		if (controller.IsButtonDown(XboxButtonID::LEFT_SHOULDER))
			up -= 1.f;
	}

	if (m_isFreeFlyMode)
	{
		float moveSpeed = (controller.IsButtonDown(XboxButtonID::A)) ? 15.f : 1.f;

		Vec3 iForward;
		Vec3 jLeft;
		Vec3 kUp;
		m_freeFlyOrientation.GetAsVectors_IFwd_JLeft_KUp(iForward, jLeft, kUp);
		Vec3 iRight = -jLeft;

		Vec3 moveDir = Vec3::ZERO;
		moveDir += iForward * forward;
		moveDir += iRight * right;
		moveDir += Vec3(0.f, 0.f, 1.f) * up;

		if (moveDir.GetLengthSquared() > 0.f)
		{
			moveDir = moveDir.GetNormalized();
			m_freeFlyPosition += moveDir * moveSpeed * dt;
		}
	}
	else
	{
		float moveSpeed = (controller.IsButtonDown(XboxButtonID::A))
			? actor->m_runSpeed
			: actor->m_walkSpeed;

		Vec3 iForward;
		Vec3 jLeft;
		Vec3 kUp;
		EulerAngles moveOrientation = actor->m_orientation;
		moveOrientation.m_pitchDegrees = 0.f;
		moveOrientation.m_rollDegrees = 0.f;
		moveOrientation.GetAsVectors_IFwd_JLeft_KUp(iForward, jLeft, kUp);
		Vec3 iRight = -jLeft;

		Vec3 moveDir = Vec3::ZERO;
		moveDir += iForward * forward;
		moveDir += iRight * right;

		if (moveDir.GetLengthSquared() > 0.f)
		{
			actor->MoveInDirection(moveDir, moveSpeed);
		}

		if (controller.GetRightTrigger() > 0.5f)
		{
			actor->Attack();
		}
	}

	if (controller.WasButtonJustPressed(XboxButtonID::LEFT_SHOULDER))
	{
		int weaponCount = (int)actor->m_weapons.size();
		if (weaponCount > 0)
		{
			int newIndex = actor->m_equippedWeaponIndex - 1;
			if (newIndex < 0)
			{
				newIndex = weaponCount - 1;
			}
			actor->EquipWeapon(newIndex);
		}
	}

	if (controller.WasButtonJustPressed(XboxButtonID::RIGHT_SHOULDER))
	{
		int weaponCount = (int)actor->m_weapons.size();
		if (weaponCount > 0)
		{
			int newIndex = actor->m_equippedWeaponIndex + 1;
			if (newIndex >= weaponCount)
			{
				newIndex = 0;
			}
			actor->EquipWeapon(newIndex);
		}
	}

}

void Player::RenderDeathOverlay(AABB2 const& screenBounds) const
{
	Actor* actor = GetActor();
	if (actor == nullptr || !actor->m_isDead)
	{
		return;
	}

	std::vector<Vertex> verts;
	AddVertsForAABB2(
		verts,
		screenBounds,
		Rgba8(0, 0, 0, 100),
		Vec2(0.f,0.f),
		Vec2(1.f,1.f)
	);

	g_engine->m_render->BindTexture(nullptr);
	g_engine->m_render->DrawVertexArray(verts);
}

void Player::RenderHUDText(AABB2 const& screenBounds) const
{
	Actor* actor = GetActor();

	float health = 0.f;
	if (actor != nullptr)
	{
		health = actor->m_health;
	}

	Vec2 dims = screenBounds.GetDimensions();

	float hudHeight = dims.y * 0.175f;
	float cellHeight = hudHeight * 0.3f;
	float textY = screenBounds.m_mins.y + hudHeight * 0.32f;

	Vec2 killsPos(
		screenBounds.m_mins.x + dims.x * 0.055f,
		textY
	);

	Vec2 healthPos(
		screenBounds.m_mins.x + dims.x * 0.275f,
		textY
	);

	Vec2 deathsPos(
		screenBounds.m_maxs.x - dims.x * 0.08f,
		textY
	);

	DrawHUDText(Stringf("%i", m_kills), killsPos, cellHeight, Rgba8::WHITE);
	DrawHUDText(Stringf("%.0f", health), healthPos, cellHeight, Rgba8::WHITE);
	DrawHUDText(Stringf("%i", m_deaths), deathsPos, cellHeight, Rgba8::WHITE);

	float fps = 0.f;


	float dt = (float)m_map->m_game->m_gameClock->GetDeltaSeconds();

	if (dt > 0.f)
	{
		fps = 1.f / dt;
	}

	Vec2 fpsPos(
		screenBounds.m_maxs.x - dims.x * .075f,
		screenBounds.m_maxs.y - dims.y * 0.02f
	);

	DrawHUDText(
		Stringf("[FPS]: %.0f", fps),
		fpsPos,
		cellHeight * 0.25f,
		Rgba8(255, 255, 255, 255)
	);
}

void Player::DrawHUDText(std::string const& text, Vec2 const& position, float cellHeight, Rgba8 const& color) const
{
	BitmapFont* font = g_engine->m_render->CreateOrGetBitmapFont("Data/Fonts/SquirrelFixedFont");
	if (font == nullptr)
	{
		return;
	}

	std::vector<Vertex> verts;

	font->AddVertsForText2D(
		verts,
		position,
		cellHeight,
		text,
		color
	);

	g_engine->m_render->BindTexture(&font->GetTexture());
	g_engine->m_render->DrawVertexArray(verts);
}

void Player::UpdateSensitivity()
{
	InputSystem* input = g_engine->m_input;

	if (input->WasKeyJustPressed('9'))
	{
		m_yawPerPixel -= 0.01f;
		m_pitchPerPixel -= 0.01f;

		m_yawPerPixel = GetClamped(m_yawPerPixel, 0.01f, 1.f);
		m_pitchPerPixel = GetClamped(m_pitchPerPixel, 0.01f, 1.f);

		DebugAddMessage(
			Stringf("Mouse Sensitivity: %.2f", m_yawPerPixel),
			2.f,
			Rgba8(0,0,255,255),
			Rgba8(0,0,255,255)
		);
	}

	if (input->WasKeyJustPressed('0'))
	{
		m_yawPerPixel += 0.01f;
		m_pitchPerPixel += 0.01f;

		m_yawPerPixel = GetClamped(m_yawPerPixel, 0.01f, 1.f);
		m_pitchPerPixel = GetClamped(m_pitchPerPixel, 0.01f, 1.f);

		DebugAddMessage(
			Stringf("Mouse Sensitivity: %.2f", m_yawPerPixel),
			2.f,
			Rgba8(0,0,255,255),
			Rgba8(0,0,255,255)
		);
	}
}
