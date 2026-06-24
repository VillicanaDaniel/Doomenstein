#include "Game/Actor.hpp"
#include "Game/ActorDefinition.hpp"
#include "Game/Game.hpp"
#include "Game/Map.hpp"
#include "Game/Weapon.hpp"
#include "Game/Controller.hpp"
#include "Game/AIController.hpp"
#include "Game/Player.hpp"
#include "Game/App.hpp"
#include "Engine/Core/Engine.hpp"
#include "Engine/Core/Clock.hpp"
#include "Engine/Core/Vertex.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Renderer/Camera.hpp"
#include "Engine/Renderer/SpriteAnimDefinition.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Math/AABB2.hpp"

Actor::Actor(Game* game, Map* map)
	: m_game(game)
	, m_map(map)
{
}

Actor::~Actor()
{
	for (Weapon*& weapon : m_weapons)
	{
		delete weapon;
		weapon = nullptr;
	}
	m_weapons.clear();
}

void Actor::Update()
{
	if (m_game == nullptr || m_game->m_gameClock == nullptr)
	{
		return;
	}

	if ((m_destroyAboveZ >= 0.f && m_position.z >= m_destroyAboveZ) || m_position.z < -10.f)
	{
		m_isDead = true;
		m_health = 0.f;
		m_velocity = Vec3::ZERO;
		m_acceleration = Vec3::ZERO;
		PlayAnimationGroup("Death");
	}
	if (m_position.x < -10.f || m_position.x > m_map->m_dimensions.x + 10.f)
	{
		m_isDead = true;
		m_health = 0.f;
		m_velocity = Vec3::ZERO;
		m_acceleration = Vec3::ZERO;
		PlayAnimationGroup("Death");
	}
	if (m_position.y < -10.f || m_position.y > m_map->m_dimensions.y + 10.f)
	{
		m_isDead = true;
		m_health = 0.f;
		m_velocity = Vec3::ZERO;
		m_acceleration = Vec3::ZERO;
		PlayAnimationGroup("Death");
	}

	float dt = (float)m_game->m_gameClock->GetDeltaSeconds();

	if (m_invincibilityTimer > 0.f)
	{
		m_invincibilityTimer -= dt;
		if (m_invincibilityTimer < 0.f)
		{
			m_invincibilityTimer = 0.f;
		}
	}

	ActorAnimationGroupDefinition const* activeGroup = GetCurrentAnimGroup();

	float animTimeScale = 1.f;

	if (activeGroup != nullptr &&
		activeGroup->m_scaleBySpeed &&
		m_definition != nullptr &&
		m_definition->m_runSpeed > 0.f)
	{
		animTimeScale = m_velocity.GetLength() / m_definition->m_runSpeed;
		animTimeScale = GetClamped(animTimeScale, 0.25f, 2.f);
	}

	//Only advance animation time when moving
	if (!m_onlyAnimateWhenMoving || m_isOverworldMoving)
	{
		m_animTime += dt;
	}
	else
	{
		m_animTime = 0.f;
	}

	if (m_isDead)
	{
		m_timeSinceDeath += dt;
		return;
	}

	for (Weapon* weapon : m_weapons)
	{
		if (weapon != nullptr)
		{
			weapon->Update(dt);
		}
	}

	if (activeGroup != nullptr)
	{
		bool isOneShotAnim =
			m_currentAnimGroupName == "Attack" ||
			m_currentAnimGroupName == "Hurt";

		if (isOneShotAnim && activeGroup->m_totalDuration > 0.f)
		{
			if (m_animTime >= activeGroup->m_totalDuration)
			{
				PlayAnimationGroup("Walk");
			}
		}
	}

	UpdatePhysics();
}

void Actor::UpdatePhysics()
{
	if (!m_simulated || m_game == nullptr || m_game->m_gameClock == nullptr)
	{
		return;
	}

	float dt = (float)m_game->m_gameClock->GetDeltaSeconds();

	AddForce(-m_velocity * m_drag);

	m_velocity += m_acceleration * dt;
	m_position += m_velocity * dt;

	m_acceleration = Vec3::ZERO;
}

void Actor::Render(Camera const& camera) const
{
	if (!m_visible || m_definition == nullptr)
	{
		return;
	}

	ActorAnimationGroupDefinition const* group = GetCurrentAnimGroup();
	if (group == nullptr)
	{
		return;
	}

	g_engine->m_render->SetLightConstants(
		Vec3(0.f, 0.f, -1.f),
		0.f,
		1.f
	);

	ActorAnimationDirectionDefinition const* bestDir = GetBestAnimDirectionForCamera(*group, camera);

	if (m_useSpriteFacingDirection)
	{
		bestDir = GetBestAnimDirectionForFacing(*group);
	}
	else
	{
		bestDir = GetBestAnimDirectionForCamera(*group, camera);
	}

	if (bestDir == nullptr && !group->m_directions.empty())
	{
		bestDir = &group->m_directions[0];
	}

	if (bestDir == nullptr || bestDir->m_animDef == nullptr)
	{
		return;
	}

	int startFrame = bestDir->m_startFrame;
	int endFrame = bestDir->m_endFrame;
	int frameCount = (endFrame - startFrame) + 1;

	if (frameCount <= 0)
	{
		return;
	}

	float secondsPerFrame = group->m_secondsPerFrame;
	if (secondsPerFrame <= 0.f)
	{
		secondsPerFrame = 0.1f;
	}

	int frameOffset = 0;

	if (group->m_playbackType == SpriteAnimPlaybackType::LOOP)
	{
		frameOffset = ((int)(m_animTime / secondsPerFrame)) % frameCount;
	}
	else
	{
		frameOffset = (int)(m_animTime / secondsPerFrame);
		if (frameOffset >= frameCount)
		{
			frameOffset = frameCount - 1;
		}
	}

	int spriteFrame = startFrame + frameOffset;

	SpriteDef const& spriteDef = m_definition->m_spriteSheet->GetSpriteDef(spriteFrame);

	AABB2 uv;
	spriteDef.GetUVs(uv.m_mins, uv.m_maxs);

	std::vector<Vertex> verts;
	AddVertsForActorBillboard(verts, uv, camera);

	g_engine->m_render->BindTexture(&spriteDef.GetTexture());
	g_engine->m_render->SetRasterizerMode(RasterizerMode::SOLID_CULL_NONE);
	g_engine->m_render->DrawVertexArray(verts);
	g_engine->m_render->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
}

void Actor::OnPossessed()
{
	m_isPossessed = true;
}

void Actor::OnUnpossessed()
{
	m_isPossessed = false;
}

void Actor::AddForce(Vec3 const& force)
{
	m_acceleration += force;
}

void Actor::AddImpulse(Vec3 const& impulse)
{
	m_velocity += impulse;
}

void Actor::MoveInDirection(Vec3 const& direction, float speed)
{
	if (m_isDead)
	{
		return;
	}

	Vec3 moveDir = direction;
	if (moveDir.GetLengthSquared() <= 0.f)
	{
		return;
	}

	moveDir = moveDir.GetNormalized();
	AddForce(moveDir * speed * m_drag);
}

void Actor::TurnInDirection(Vec3 const& direction, float maxTurnDegrees)
{
	if (m_isDead)
	{
		return;
	}

	Vec3 flatDir = direction;
	flatDir.z = 0.f;

	if (flatDir.GetLengthSquared() <= 0.f)
	{
		return;
	}

	flatDir = flatDir.GetNormalized();

	float targetYaw = Atan2Degrees(flatDir.y, flatDir.x);
	float currentYaw = m_orientation.m_yawDegrees;
	float deltaYaw = GetShortestAngularDispDegrees(currentYaw, targetYaw);
	deltaYaw = GetClamped(deltaYaw, -maxTurnDegrees, maxTurnDegrees);

	m_orientation.m_yawDegrees += deltaYaw;
}

void Actor::AddYawInput(float deltaYaw)
{
	m_orientation.m_yawDegrees += deltaYaw;
}

void Actor::AddPitchInput(float deltaPitch)
{
	m_orientation.m_pitchDegrees += deltaPitch;
	m_orientation.m_pitchDegrees = GetClamped(m_orientation.m_pitchDegrees, -85.f, 85.f);
}

void Actor::Attack()
{
	if (m_isDead)
	{
		return;
	}

	Weapon* weapon = GetEquippedWeapon();
	if (weapon == nullptr)
	{
		return;
	}

	if (weapon->CanFire())
	{
		PlayAnimationGroup("Attack");
		weapon->Fire(this);
	}
}

void Actor::Damage(float amount, ActorHandle sourceHandle)
{
	if (m_isDead)
	{
		return;
	}
	if (IsInvincible())
	{
		return;
	}

	if (m_controller != nullptr)
	{
		AIController* ai = dynamic_cast<AIController*>(m_controller);
		if (ai != nullptr && sourceHandle.IsValid())
		{
			ai->DamagedBy(sourceHandle);
		}
	}

	if (m_definition != nullptr && m_definition->m_name == "Squirrel3D" && sourceHandle == m_map->m_players[0]->GetActor()->m_handle)
	{
		ERROR_AND_DIE("WHOA! Why would you try to shoot Squirrel?");
// 		g_theApp->SetIsQuitting();
// 		return;
	}

	m_health -= amount;

	if (m_health <= 0.f)
	{
		m_isDead = true;
		m_velocity = Vec3::ZERO;
		m_acceleration = Vec3::ZERO;
		PlayAnimationGroup("Death");
		PlaySound("Death");

		if (m_map != nullptr)
		{
			Player* deadPlayer = m_map->GetPlayerByActorHandle(m_handle);

			if (deadPlayer != nullptr)
			{
				deadPlayer->m_deaths++;

				Player* killerPlayer = m_map->GetPlayerByActorHandle(sourceHandle);
				if (killerPlayer != nullptr && killerPlayer != deadPlayer)
				{
					killerPlayer->m_kills++;
				}
			}
		}
	}
	else
	{
		PlayAnimationGroup("Hurt");
		PlaySound("Hurt");
	}
}

Mat44 Actor::GetModelToWorldTransform() const
{
	Mat44 rot = m_orientation.GetAsMatrix_IFwd_JLeft_KUp();
	Mat44 trans = Mat44::MakeTranslation3D(m_position);

	Mat44 modelToWorld = trans;
	modelToWorld.Append(rot);
	return modelToWorld;
}

void Actor::UpdateWeapons()
{

}

void Actor::EquipWeapon(int index)
{
	if (m_weapons.empty())
	{
		return;
	}

	int weaponCount = (int)m_weapons.size();

	if (index < 0)
	{
		index = weaponCount - 1;
	}
	else if (index >= weaponCount)
	{
		index = 0;
	}

	m_equippedWeaponIndex = index;
}

Weapon* Actor::GetEquippedWeapon() const
{
	if (m_equippedWeaponIndex < 0 || m_equippedWeaponIndex >= (int)m_weapons.size())
	{
		return nullptr;
	}

	return m_weapons[m_equippedWeaponIndex];
}

ActorAnimationGroupDefinition const* Actor::GetCurrentAnimGroup() const
{
	if (m_definition == nullptr)
	{
		return nullptr;
	}

	for (ActorAnimationGroupDefinition const* group : m_definition->m_animationGroups)
	{
		if (group != nullptr && group->m_name == m_currentAnimGroupName)
		{
			return group;
		}
	}

	if (!m_definition->m_animationGroups.empty())
	{
		return m_definition->m_animationGroups[0];
	}

	return nullptr;
}

ActorAnimationDirectionDefinition const* Actor::GetBestAnimDirectionForCamera(ActorAnimationGroupDefinition const& group, Camera const& camera) const
{
	Vec3 cameraPos = camera.GetPosition();

	Vec3 cameraToActor = m_position - cameraPos;
	cameraToActor.z = 0.f;

	if (cameraToActor.GetLengthSquared() <= 0.f)
	{
		return nullptr;
	}

	cameraToActor = cameraToActor.GetNormalized();

	ActorAnimationDirectionDefinition const* best = nullptr;
	float bestDot = -999.f;

	for (ActorAnimationDirectionDefinition const& dir : group.m_directions)
	{
		Vec3 worldDir = m_orientation.GetAsMatrix_IFwd_JLeft_KUp().TransformVectorQuantity3D(dir.m_direction);
		worldDir.z = 0.f;
		worldDir = worldDir.GetNormalized();

		float dot = DotProduct3D(worldDir, cameraToActor);

		if (dot > bestDot)
		{
			bestDot = dot;
			best = &dir;
		}
	}

	return best;
}

ActorAnimationDirectionDefinition const* Actor::GetBestAnimDirectionForFacing(
	ActorAnimationGroupDefinition const& group
) const
{
	Vec3 facing = m_spriteFacingDirection;
	facing.z = 0.f;

	if (facing.GetLengthSquared() <= 0.f)
	{
		facing = Vec3(0.f, -1.f, 0.f);
	}

	facing = facing.GetNormalized();

	ActorAnimationDirectionDefinition const* best = nullptr;
	float bestDot = -999.f;

	for (ActorAnimationDirectionDefinition const& dir : group.m_directions)
	{
		Vec3 animDir = dir.m_direction;
		animDir.z = 0.f;

		if (animDir.GetLengthSquared() <= 0.f)
		{
			continue;
		}

		animDir = animDir.GetNormalized();

		float dot = DotProduct3D(animDir, facing);

		if (dot > bestDot)
		{
			bestDot = dot;
			best = &dir;
		}
	}

	return best;
}

void Actor::AddVertsForActorBillboard(std::vector<Vertex>& verts, AABB2 const& uv, Camera const& camera) const
{
	Vec3 cameraPos = camera.GetPosition();
	Vec3 cameraToActor = m_position - cameraPos;

	Vec2 size = m_definition->m_visuals.m_size;
	Vec2 pivot = m_definition->m_visuals.m_pivot;

	float leftOffset = -pivot.x * size.x;
	float rightOffset = (1.f - pivot.x) * size.x;
	float downOffset = -pivot.y * size.y;
	float upOffset = (1.f - pivot.y) * size.y;

	Rgba8 color = Rgba8::WHITE;

	bool isTopDownCamera = cameraToActor.z < -5.f;

	if (isTopDownCamera)
	{
		Vec3 right = Vec3(0.f, -1.f, 0.f);
		Vec3 up = Vec3(1.f, 0.f, 0.f);

		Vec3 spriteCenter = m_position + Vec3(0.f, 0.f, 0.05f);

		Vec3 bottomLeft = spriteCenter + right * leftOffset + up * downOffset;
		Vec3 bottomRight = spriteCenter + right * rightOffset + up * downOffset;
		Vec3 topRight = spriteCenter + right * rightOffset + up * upOffset;
		Vec3 topLeft = spriteCenter + right * leftOffset + up * upOffset;

		verts.push_back(Vertex(bottomLeft, color, Vec2(uv.m_mins.x, uv.m_mins.y)));
		verts.push_back(Vertex(bottomRight, color, Vec2(uv.m_maxs.x, uv.m_mins.y)));
		verts.push_back(Vertex(topRight, color, Vec2(uv.m_maxs.x, uv.m_maxs.y)));

		verts.push_back(Vertex(bottomLeft, color, Vec2(uv.m_mins.x, uv.m_mins.y)));
		verts.push_back(Vertex(topRight, color, Vec2(uv.m_maxs.x, uv.m_maxs.y)));
		verts.push_back(Vertex(topLeft, color, Vec2(uv.m_mins.x, uv.m_maxs.y)));

		return;
	}

	Vec3 toCamera = cameraPos - m_position;
	toCamera.z = 0.f;

	if (toCamera.GetLengthSquared() <= 0.f)
	{
		toCamera = Vec3(1.f, 0.f, 0.f);
	}

	toCamera = toCamera.GetNormalized();

	Vec3 worldUp = Vec3(0.f, 0.f, 1.f);
	Vec3 left = CrossProduct3D(worldUp, toCamera).GetNormalized();
	Vec3 up = worldUp;

	Vec3 bottomLeft = m_position + left * leftOffset + up * downOffset;
	Vec3 bottomRight = m_position + left * rightOffset + up * downOffset;
	Vec3 topRight = m_position + left * rightOffset + up * upOffset;
	Vec3 topLeft = m_position + left * leftOffset + up * upOffset;

	verts.push_back(Vertex(bottomLeft, color, Vec2(uv.m_mins.x, uv.m_mins.y)));
	verts.push_back(Vertex(bottomRight, color, Vec2(uv.m_maxs.x, uv.m_mins.y)));
	verts.push_back(Vertex(topRight, color, Vec2(uv.m_maxs.x, uv.m_maxs.y)));

	verts.push_back(Vertex(bottomLeft, color, Vec2(uv.m_mins.x, uv.m_mins.y)));
	verts.push_back(Vertex(topRight, color, Vec2(uv.m_maxs.x, uv.m_maxs.y)));
	verts.push_back(Vertex(topLeft, color, Vec2(uv.m_mins.x, uv.m_maxs.y)));
}

void Actor::PlayAnimationGroup(std::string const& animName)
{
	if (m_currentAnimGroupName == animName)
	{
		return;
	}

	m_currentAnimGroupName = animName;
	m_animTime = 0.f;
}

void Actor::PlaySound(std::string const& soundName)
{
	if (m_definition == nullptr || m_map == nullptr)
	{
		return;
	}

	auto found = m_definition->m_sounds.find(soundName);
	if (found == m_definition->m_sounds.end())
	{
		return;
	}

	SoundID soundID = found->second;
	if (soundID == MISSING_SOUND_ID)
	{
		return;
	}

	g_engine->m_audio->StartSoundAt(soundID, m_position, false, 1.f);
}

bool Actor::IsInvincible() const
{
	return m_invincibilityTimer > 0.f;
}

void Actor::BecomeInvincible(float durationSeconds)
{
	m_invincibilityTimer = durationSeconds;
}