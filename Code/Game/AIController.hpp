#pragma once

#include "Game/Controller.hpp"
#include "Game/ActorHandle.hpp"

class AIController : public Controller
{
public:
	explicit AIController(Map* map);
	virtual ~AIController() = default;

	virtual void Update() override;

	void DamagedBy(ActorHandle attackerHandle);

public:
	ActorHandle m_targetActorHandle = ActorHandle::INVALID;

	float m_detectionRange = 12.f;
	float m_attackRange = 1.5f;
	float m_turnRateDegreesPerSecond = 180.f;
	float m_moveSpeed = 4.f;
};