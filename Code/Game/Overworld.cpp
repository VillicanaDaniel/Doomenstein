#include "Game/Overworld.hpp"
#include "Game/Game.hpp"
#include "Game/Player.hpp"
#include "Game/Actor.hpp"
#include "Game/Tile.hpp"
#include "Game/TileDefinition.hpp"
#include "Engine/Core/Engine.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Core/Clock.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Math/RandomNumberGenerator.hpp"
#include "Engine/Core/DebugRenderSystem.hpp"
#include "Engine/Renderer/Camera.hpp"

Overworld::Overworld(Game* game, MapDefinition const* definition)
	: Map(game, definition)
{
	Actor* playerActor = nullptr;

	if (m_player != nullptr)
	{
		playerActor = m_player->GetActor();
	}

	if (playerActor != nullptr)
	{
		m_playerTile = IntVec2(
			RoundDownToInt(playerActor->m_position.x),
			RoundDownToInt(playerActor->m_position.y)
		);

		m_moveTargetTile = m_playerTile;
		playerActor->m_position = Vec3((float)m_playerTile.x + 0.5f, (float)m_playerTile.y + 0.5f, 0.f);
		playerActor->m_useSpriteFacingDirection = true;
	}
	m_snowTexture = g_engine->m_render->CreateOrGetTextureFromFile("Data/Images/snowflake.png");
	InitializeSnow();
}

void Overworld::Update()
{
	if (m_player != nullptr)
	{
		m_player->m_disableMovement = true;

		Actor* actor = m_player->GetActor();
		if (actor != nullptr)
		{
			actor->m_velocity = Vec3::ZERO;
			actor->m_acceleration = Vec3::ZERO;
			actor->m_simulated = false;
		}
	}

	Map::Update();

	UpdateGridMovement();
	UpdateSnow();
	UpdateOverworldCamera();
	StartBattleIfTrainerSeesPlayer();
}

void Overworld::Render(Camera const& camera, Actor const* controlledActor) const
{
	Map::Render(camera, controlledActor);
}

void Overworld::UpdateOverworldCamera()
{
	if (m_player == nullptr || m_player->m_worldCamera == nullptr)
	{
		return;
	}

	Actor* actor = m_player->GetActor();
	if (actor == nullptr)
	{
		return;
	}

	Camera* cam = m_player->m_worldCamera;

	Mat44 cameraToRender = Mat44(
		Vec3(0, 0, 1),
		Vec3(-1, 0, 0),
		Vec3(0, 1, 0),
		Vec3(0, 0, 0)
	);
	cam->SetCameraToRenderTransform(cameraToRender);

	cam->SetPerspectiveView(
		16.f / 9.f,
		60.f,
		0.1f,
		300.f
	);

	cam->SetPositionAndOrientation(
		Vec3(actor->m_position.x, actor->m_position.y, 10.f),
		EulerAngles(0.f, 90.f, 0.f)
	);
}

void Overworld::UpdateGridMovement()
{
	Actor* playerActor = nullptr;

	if (m_player != nullptr)
	{
		playerActor = m_player->GetActor();
	}

	if (playerActor == nullptr)
	{
		return;
	}

	if (!m_hasInitializedPlayerTile)
	{
		m_playerTile = IntVec2(
			RoundDownToInt(playerActor->m_position.x),
			RoundDownToInt(playerActor->m_position.y)
		);

		m_moveTargetTile = m_playerTile;

		playerActor->m_position = Vec3(
			(float)m_playerTile.x + 0.5f,
			(float)m_playerTile.y + 0.5f,
			playerActor->m_position.z
		);

		playerActor->m_onlyAnimateWhenMoving = true;
		playerActor->m_useSpriteFacingDirection = true;
		playerActor->m_isOverworldMoving = m_isMovingTile;
		m_hasInitializedPlayerTile = true;

	}

	float dt = (float)m_game->m_gameClock->GetDeltaSeconds();

	if (m_isMovingTile)
	{
		m_moveT += dt / m_secondsPerTileMove;
		m_moveT = GetClamped(m_moveT, 0.f, 1.f);

		Vec3 startPos((float)m_playerTile.x + 0.5f, (float)m_playerTile.y + 0.5f, 0.f);
		Vec3 endPos((float)m_moveTargetTile.x + 0.5f, (float)m_moveTargetTile.y + 0.5f, 0.f);

		playerActor->m_position = startPos + ((endPos - startPos) * m_moveT);

		if (m_moveT >= 1.f)
		{
			m_playerTile = m_moveTargetTile;
			m_isMovingTile = false;
			m_moveT = 0.f;

			playerActor->m_isOverworldMoving = false;
			playerActor->m_animTime = 0.f;

			playerActor->m_position = Vec3(
				(float)m_playerTile.x + 0.5f,
				(float)m_playerTile.y + 0.5f,
				playerActor->m_position.z
			);
		}

		return;
	}

	InputSystem* input = g_engine->m_input;

	IntVec2 moveDir = IntVec2::ZERO;

	if (input->IsKeyDown('W'))
	{
		moveDir = IntVec2(1, 0);
		playerActor->m_spriteFacingDirection = Vec3(1.f, 0.f, 0.f);
	}
	else if (input->IsKeyDown('S'))
	{
		moveDir = IntVec2(-1, 0);
		playerActor->m_spriteFacingDirection = Vec3(-1.f, 0.f, 0.f);
	}
	else if (input->IsKeyDown('A'))
	{
		moveDir = IntVec2(0, 1);
		playerActor->m_spriteFacingDirection = Vec3(0.f, 1.f, 0.f);
	}
	else if (input->IsKeyDown('D'))
	{
		moveDir = IntVec2(0, -1);
		playerActor->m_spriteFacingDirection = Vec3(0.f, -1.f, 0.f);
	}

	if (moveDir.x == 0 && moveDir.y == 0)
	{
		return;
	}

	IntVec2 nextTile = m_playerTile + moveDir;
	if (!CanMoveToTile(nextTile))
	{
		return;
	}

	m_moveTargetTile = nextTile;
	m_isMovingTile = true;
	m_moveT = 0.f;

	playerActor->m_isOverworldMoving = true;
}

bool Overworld::CanMoveToTile(IntVec2 const& tileCoords) const
{
	if (!AreCoordsInBounds(tileCoords.x, tileCoords.y))
	{
		return false;
	}

	Tile const* tile = GetTile(tileCoords.x, tileCoords.y);
	if (tile == nullptr || tile->m_tileDef == nullptr)
	{
		return false;
	}

	if (tile->m_tileDef->m_isSolid)
	{
		return false;
	}

	if (!tile->m_tileDef->m_isWalkable)
	{
		return false;
	}

	return true;
}

void Overworld::StartBattleIfTrainerSeesPlayer()
{
	if (m_game->IsDialogueActive())
	{
		return;
	}

	if (m_player == nullptr)
	{
		return;
	}

	Actor* playerActor = m_player->GetActor();
	if (playerActor == nullptr || playerActor->m_isDead)
	{
		return;
	}

	Vec3 playerPos = playerActor->m_position + Vec3(0.f, 0.f, playerActor->m_physicsHeight * 0.5f);

	for (Actor* trainer : m_actors)
	{
		if (trainer == nullptr || trainer == playerActor || trainer->m_isDead)
		{
			continue;
		}

		if (trainer->m_faction != "Trainer" && trainer->m_faction != "TRAINER")
		{
			continue;
		}

		float sightDistance = trainer->m_sightRadius;

		if (sightDistance <= 0.f)
		{
			sightDistance = 1.f;
		}

		Vec3 trainerEyePos = trainer->m_position + Vec3(0.f, 0.f, trainer->m_eyeHeight);

		Vec3 forward;
		Vec3 left;
		Vec3 up;
		trainer->m_orientation.GetAsVectors_IFwd_JLeft_KUp(forward, left, up);

		forward.z = 0.f;
		forward = forward.GetNormalized();

		Vec3 toPlayer = playerPos - trainerEyePos;
		toPlayer.z = 0.f;

		float distanceToPlayer = toPlayer.GetLength();
		if (distanceToPlayer <= 0.f || distanceToPlayer > sightDistance)
		{
			continue;
		}

		Vec3 dirToPlayer = toPlayer.GetNormalized();

		float dot = DotProduct3D(forward, dirToPlayer);
		if (dot < 0.99f)
		{
			continue;
		}

		RaycastResult3D wallHit = RaycastWorldXY(trainerEyePos, dirToPlayer, distanceToPlayer);

		if (!wallHit.m_didImpact)
		{
			DialogueRequest request;
			request.m_lines.push_back("Squirrel: This has been a long time coming Butler.");
			request.m_lines.push_back("Squirrel: There really only is one way to prove who the better SD professor is...");
			request.m_lines.push_back("Squirrel: I will defeat you here and now!");
			request.m_changeMapAfterDialogue = true;
			request.m_nextMapName = "TestMap";

			m_game->StartDialogue(request);
			return;
		}
	}
}

void Overworld::InitializeSnow()
{
	m_snowParticles.clear();

	IntVec2 dims = g_engine->m_window->GetClientDimensions();

	for (int i = 0; i < 120; ++i)
	{
		SnowParticle flake;
		flake.m_position = Vec2(
			g_rng->RollRandomFloatInRange(0.f, (float)dims.x),
			g_rng->RollRandomFloatInRange(0.f, (float)dims.y)
		);

		flake.m_speed = g_rng->RollRandomFloatInRange(600.f, 800.f);
		flake.m_size = g_rng->RollRandomFloatInRange(15.f, 20.f);

		m_snowParticles.push_back(flake);
	}
}

void Overworld::UpdateSnow()
{
	float dt = (float)m_game->m_gameClock->GetDeltaSeconds();
	IntVec2 dims = g_engine->m_window->GetClientDimensions();

	for (SnowParticle& flake : m_snowParticles)
	{
		flake.m_position.y -= flake.m_speed * dt;
		flake.m_position.x += 400.f * dt;

		if (flake.m_position.y < -flake.m_size)
		{
			flake.m_position.y = (float)dims.y + flake.m_size;
			flake.m_position.x = g_rng->RollRandomFloatInRange(0.f, (float)dims.x);
		}

		if (flake.m_position.x > (float)dims.x + flake.m_size)
		{
			flake.m_position.x = -flake.m_size;
		}
	}
}

void Overworld::RenderSnow([[maybe_unused]]Camera const& screenCamera) const
{
	if (m_snowTexture == nullptr)
	{
		return;
	}

	std::vector<Vertex> verts;

	for (SnowParticle const& flake : m_snowParticles)
	{
		float halfSize = flake.m_size * 0.5f;

		AABB2 bounds(
			Vec2(flake.m_position.x - halfSize, flake.m_position.y - halfSize),
			Vec2(flake.m_position.x + halfSize, flake.m_position.y + halfSize)
		);

		AddVertsForAABB2(verts, bounds, Rgba8::WHITE);
	}

	g_engine->m_render->SetBlendMode(BlendMode::ALPHA);
	g_engine->m_render->BindTexture(m_snowTexture);
	g_engine->m_render->DrawVertexArray((int)verts.size(), verts.data());
}