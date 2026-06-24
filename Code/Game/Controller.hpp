#pragma once

#include "Game/ActorHandle.hpp"

class Map;
class Actor;

class Controller
{
public:
	explicit Controller(Map* map);
	virtual ~Controller() = default;

	virtual void Update() = 0;

	void Possess(ActorHandle handle);
	Actor* GetActor() const;

public:
	Map* m_map = nullptr;
	ActorHandle m_actorHandle = ActorHandle::INVALID;
};