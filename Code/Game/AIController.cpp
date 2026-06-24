#include "Game/AIController.hpp"
#include "Game/Map.hpp"
#include "Game/Actor.hpp"
#include "Game/Game.hpp"
#include "Game/WeaponDefinition.hpp"
#include "Game/Weapon.hpp"
#include "Engine/Core/Clock.hpp"
#include "Engine/Math/MathUtils.hpp"

AIController::AIController(Map* map)
	: Controller(map)
{
}

void AIController::DamagedBy(ActorHandle attackerHandle)
{
	m_targetActorHandle = attackerHandle;
}

void AIController::Update()
{
	if (m_map == nullptr || m_map->m_game == nullptr)
	{
		return;
	}

	Actor* self = GetActor();
	if (self == nullptr || self->m_isDead)
	{
		return;
	}

	Actor* target = m_map->GetActorByHandle(m_targetActorHandle);

	if (target == nullptr || target->m_isDead)
	{
		Vec3 forward;
		Vec3 left;
		Vec3 up;
		self->m_orientation.GetAsVectors_IFwd_JLeft_KUp(forward, left, up);

		Vec3 selfEyes = self->m_position;
		selfEyes.z += self->m_eyeHeight;

		target = m_map->GetClosestVisibleEnemy(
			selfEyes,
			forward.GetNormalized(),
			self->m_sightRadius,
			self
		);

		if (target != nullptr)
		{
			m_targetActorHandle = target->m_handle;
		}
		else
		{
			m_targetActorHandle = ActorHandle::INVALID;
			return;
		}
	}

	Vec3 selfEyes = self->m_position;
	selfEyes.z += self->m_eyeHeight;

	Vec3 targetEyes = target->m_position;
	targetEyes.z += target->m_eyeHeight;

	Vec3 toTarget = targetEyes - selfEyes;
	float distSq = toTarget.GetLengthSquared();
	if (distSq <= 0.f)
	{
		return;
	}

	Vec3 dirToTarget = toTarget.GetNormalized();
	float dt = (float)m_map->m_game->m_gameClock->GetDeltaSeconds();

	self->TurnInDirection(dirToTarget, self->m_turnSpeed * dt);

	float attackRange = 1.0f;
	Weapon* weapon = self->GetEquippedWeapon();
	if (weapon != nullptr && weapon->m_definition != nullptr)
	{
		if (weapon->m_definition->m_meleeCount > 0)
		{
			attackRange = weapon->m_definition->m_meleeRange;
		}
		else if (weapon->m_definition->m_projectileCount > 0)
		{
			attackRange = 8.0f;
		}
		else if (weapon->m_definition->m_rayCount > 0)
		{
			attackRange = weapon->m_definition->m_rayRange;
		}
	}

	float dist = sqrtf(distSq);

	if (dist > attackRange * 0.9f)
	{
		Vec3 moveDir = dirToTarget;
		moveDir.z = 0.f;

		if (moveDir.GetLengthSquared() > 0.f)
		{
			moveDir = moveDir.GetNormalized();
			self->MoveInDirection(moveDir, self->m_walkSpeed);
		}
	}

	if (dist <= attackRange)
	{
		self->Attack();
	}
}