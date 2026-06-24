#include "Game/Weapon.hpp"
#include "Game/Actor.hpp"
#include "Game/Map.hpp"
#include "Game/MapDefinition.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Math/RandomNumberGenerator.hpp"
#include "Engine/Core/DebugRenderSystem.hpp"
#include "Engine/Core/Engine.hpp"
#include "Engine/Core/Vertex.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include <cstdlib>

Weapon::Weapon(WeaponDefinition const* definition)
	: m_definition(definition)
{
}

void Weapon::Update(float deltaSeconds)
{
	if (m_cooldownSeconds > 0.f)
	{
		m_cooldownSeconds -= deltaSeconds;
		if (m_cooldownSeconds < 0.f)
		{
			m_cooldownSeconds = 0.f;
		}
	}

	m_animTime += deltaSeconds;

	WeaponAnimationDefinition const* anim = GetCurrentAnimation();
	if (anim != nullptr && m_currentAnimName == "Attack")
	{
		if (m_animTime >= anim->m_totalDuration)
		{
			PlayAnimation("Idle");
		}
	}
}

bool Weapon::CanFire() const
{
	return m_definition != nullptr && m_cooldownSeconds <= 0.f;
}

void Weapon::Fire(Actor* owner)
{
	if (owner == nullptr || owner->m_map == nullptr || m_definition == nullptr)
	{
		return;
	}

	if (!CanFire())
	{
		return;
	}

	if (m_definition->m_fireSound != MISSING_SOUND_ID)
	{
		g_engine->m_audio->StartSoundAt(
			m_definition->m_fireSound,
			owner->m_position,
			false,
			1.f
		);
	}
	PlayAnimation("Attack");

	Vec3 shotStart = owner->m_position;
	shotStart.z += owner->m_eyeHeight;

	Vec3 meleeStart = owner->m_position;
	meleeStart.z += owner->m_physicsHeight * 0.5f;

	// Melee Attacks
	if (m_definition->m_meleeCount > 0)
	{
		Vec3 iForward;
		Vec3 jLeft;
		Vec3 kUp;
		owner->m_orientation.GetAsVectors_IFwd_JLeft_KUp(iForward, jLeft, kUp);
		Vec3 forward = iForward.GetNormalized();

		for (int i = 0; i < m_definition->m_meleeCount; ++i)
		{
			Actor* hitActor = owner->m_map->GetClosestVisibleEnemyInSector(
				meleeStart,
				forward,
				m_definition->m_meleeRange,
				m_definition->m_meleeArc,
				owner
			);

			if (hitActor != nullptr)
			{
				float damage = 0.5f * (m_definition->m_meleeDamage.m_min + m_definition->m_meleeDamage.m_max);
				hitActor->Damage(damage, owner->m_handle);

				if (m_definition->m_meleeImpulse > 0.f)
				{
					Vec3 impulseDir = (hitActor->m_position - owner->m_position).GetNormalized();
					hitActor->AddImpulse(impulseDir * m_definition->m_meleeImpulse);
				}
			}
		}

		m_cooldownSeconds = m_definition->m_refireTime;
		return;
	}

	//Projectile Weapons
	if (m_definition->m_projectileCount > 0 && !m_definition->m_projectileActor.empty())
	{
		Vec3 iForward;
		Vec3 jLeft;
		Vec3 kUp;
		owner->m_orientation.GetAsVectors_IFwd_JLeft_KUp(iForward, jLeft, kUp);

		Vec3 baseForward = iForward.GetNormalized();

		for (int i = 0; i < m_definition->m_projectileCount; ++i)
		{
			Vec3 shotDir = baseForward;

			if (m_definition->m_projectileCone > 0.f)
			{
				shotDir = GetRandomDirectionInCone(baseForward * 0.25, m_definition->m_projectileCone);
				shotDir = shotDir.GetNormalized();
			}

			EulerAngles shotOrientation = owner->m_orientation;
			shotOrientation.m_yawDegrees = Atan2Degrees(shotDir.y, shotDir.x);

			float horizontalLength = sqrtf((shotDir.x * shotDir.x) + (shotDir.y * shotDir.y));
			shotOrientation.m_pitchDegrees = Atan2Degrees(shotDir.z, horizontalLength);

			SpawnInfo projectileSpawn;
			projectileSpawn.m_actor = m_definition->m_projectileActor;
			projectileSpawn.m_position = shotStart + (shotDir * 0.25f);
			projectileSpawn.m_orientation = shotOrientation;
			projectileSpawn.m_faction = owner->m_faction;

			owner->m_map->SpawnProjectile(
				projectileSpawn,
				owner->m_handle,
				shotDir * m_definition->m_projectileSpeed
			);
		}

		m_cooldownSeconds = m_definition->m_refireTime;
		return;
	}

	//Hitscan weapons
	if (m_definition->m_rayCount > 0)
	{
		int rayCount = m_definition->m_rayCount;
		if (rayCount < 1)
		{
			rayCount = 1;
		}

		for (int rayIndex = 0; rayIndex < rayCount; ++rayIndex)
		{
			EulerAngles shotOrientation = owner->m_orientation;

			if (m_definition->m_rayCone > 0.f)
			{
				float halfCone = 0.5f * m_definition->m_rayCone;
				float yawOffset = RangeMapClamped((float)rand() / (float)RAND_MAX, 0.f, 1.f, -halfCone, halfCone);
				float pitchOffset = RangeMapClamped((float)rand() / (float)RAND_MAX, 0.f, 1.f, -halfCone, halfCone);

				shotOrientation.m_yawDegrees += yawOffset;
				shotOrientation.m_pitchDegrees += pitchOffset;
			}

			Vec3 iForward;
			Vec3 jLeft;
			Vec3 kUp;
			shotOrientation.GetAsVectors_IFwd_JLeft_KUp(iForward, jLeft, kUp);

			Vec3 shotDir = iForward.GetNormalized();

			RaycastResult3D hitResult = owner->m_map->RaycastAll(
				shotStart,
				shotDir,
				m_definition->m_rayRange,
				owner
			);

			Actor* hitActor = owner->m_map->GetClosestVisibleActorInRay(
				shotStart,
				shotDir,
				m_definition->m_rayRange,
				owner
			);

			if (hitActor != nullptr)
			{
				float damage = 0.5f * (m_definition->m_rayDamage.m_min + m_definition->m_rayDamage.m_max);
				hitActor->Damage(damage, owner->m_handle);

				if (m_definition->m_rayImpulse > 0.f)
				{
					hitActor->AddImpulse(shotDir * m_definition->m_rayImpulse);
				}
			}

			if (hitResult.m_didImpact)
			{
				SpawnInfo impactSpawn;

				if (hitActor != nullptr)
				{
					impactSpawn.m_actor = "BloodSplatter";
					impactSpawn.m_position = hitResult.m_impactPosition;
				}
				else
				{
					impactSpawn.m_actor = "BulletHit";
					impactSpawn.m_position = hitResult.m_impactPosition;
				}

				impactSpawn.m_orientation = EulerAngles();
				impactSpawn.m_faction = owner->m_faction;

				owner->m_map->SpawnActor(impactSpawn);
			}
		}

		m_cooldownSeconds = m_definition->m_refireTime;
		return;
	}

	m_cooldownSeconds = m_definition->m_refireTime;
}

Vec3 Weapon::GetRandomDirectionInCone(Vec3 const& forward, float coneDegrees)
{
	Vec3 fwd = forward.GetNormalized();

	Vec3 worldUp = Vec3(0.f, 0.f, 1.f);

	Vec3 right = CrossProduct3D(fwd, worldUp);
	if (right.GetLengthSquared() < 0.0001f)
	{
		right = CrossProduct3D(fwd, Vec3(1.f, 0.f, 0.f));
	}
	right = right.GetNormalized();

	Vec3 up = CrossProduct3D(right, fwd).GetNormalized();

	float halfAngle = coneDegrees * 0.5f;

	float randomYaw = g_rng->RollRandomFloatInRange(0.f, 360.f);
	float randomPitch = g_rng->RollRandomFloatInRange(0.f, halfAngle);

	float yawRad = ConvertDegreesToRadians(randomYaw);
	float pitchRad = ConvertDegreesToRadians(randomPitch);

	float cosPitch = cosf(pitchRad);
	float sinPitch = sinf(pitchRad);
	float cosYaw = cosf(yawRad);
	float sinYaw = sinf(yawRad);

	Vec3 direction =
		(fwd * cosPitch) +
		(right * (sinPitch * cosYaw)) +
		(up * (sinPitch * sinYaw));

	return direction.GetNormalized();
}

void Weapon::PlayAnimation(std::string const& animName)
{
	if (m_currentAnimName == animName)
	{
		return;
	}

	m_currentAnimName = animName;
	m_animTime = 0.f;
}

WeaponAnimationDefinition const* Weapon::GetCurrentAnimation() const
{
	if (m_definition == nullptr)
	{
		return nullptr;
	}

	for (WeaponAnimationDefinition const* anim : m_definition->m_animations)
	{
		if (anim != nullptr && anim->m_name == m_currentAnimName)
		{
			return anim;
		}
	}

	if (!m_definition->m_animations.empty())
	{
		return m_definition->m_animations[0];
	}

	return nullptr;
}

void Weapon::RenderHUD(AABB2 const& screenBounds) const
{
	if (m_definition == nullptr)
	{
		return;
	}

	//HUD
	if (m_definition->m_hud.m_baseTexture != nullptr)
	{
		float hudHeight = screenBounds.GetDimensions().y * 0.15f;

		AABB2 hudBox(
			Vec2(screenBounds.m_mins.x, screenBounds.m_mins.y),
			Vec2(screenBounds.m_maxs.x, screenBounds.m_mins.y + hudHeight)
		);

		std::vector<Vertex> verts;
		AddVertsForAABB2(verts, hudBox, Rgba8::WHITE, Vec2(0.f,0.f), Vec2(1.f, 1.f));

		g_engine->m_render->BindTexture(m_definition->m_hud.m_baseTexture);
		g_engine->m_render->DrawVertexArray(verts);
	}

	//Reticle
	if (m_definition->m_hud.m_reticleTexture != nullptr)
	{
		Vec2 center = screenBounds.GetCenter();
		Vec2 halfSize = m_definition->m_hud.m_reticleSize * 0.7f;

		AABB2 reticleBox(center - halfSize, center + halfSize);

		std::vector<Vertex> verts;
		AddVertsForAABB2(verts, reticleBox, Rgba8::WHITE, Vec2(0.f,0.f), Vec2(1.f, 1.f));

		g_engine->m_render->BindTexture(m_definition->m_hud.m_reticleTexture);
		g_engine->m_render->DrawVertexArray(verts);
	}

	//Animated weapon sprite
	WeaponAnimationDefinition const* anim = GetCurrentAnimation();
	if (anim != nullptr && anim->m_animDef != nullptr)
	{
		SpriteDef const& spriteDef = anim->m_animDef->GetSpriteDefAtTime(m_animTime);

		AABB2 uv;
		spriteDef.GetUVs(uv.m_mins, uv.m_maxs);

		Vec2 size = m_definition->m_hud.m_spriteSize * 1.75;
		Vec2 pivot = m_definition->m_hud.m_spritePivot;

		float hudHeight = screenBounds.GetDimensions().y * 0.15f;
		float hudTopY = screenBounds.m_mins.y + hudHeight;

		Vec2 basePos(
			screenBounds.GetCenter().x,
			hudTopY
		);

		Vec2 mins(
			basePos.x - pivot.x * size.x,
			basePos.y - pivot.y * size.y
		);

		Vec2 maxs(
			mins.x + size.x,
			mins.y + size.y
		);

		AABB2 weaponBox(mins, maxs);

		std::vector<Vertex> verts;
		AddVertsForAABB2(verts, weaponBox, Rgba8::WHITE, uv.m_mins, uv.m_maxs);

		g_engine->m_render->BindTexture(&spriteDef.GetTexture());
		g_engine->m_render->DrawVertexArray(verts);
	}
}