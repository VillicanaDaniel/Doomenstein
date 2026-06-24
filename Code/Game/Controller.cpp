#include "Game/Controller.hpp"
#include "Game/Map.hpp"
#include "Game/Actor.hpp"

Controller::Controller(Map* map)
	: m_map(map)
{
}

Actor* Controller::GetActor() const
{
	if (m_map == nullptr)
	{
		return nullptr;
	}

	return m_map->GetActorByHandle(m_actorHandle);
}

void Controller::Possess(ActorHandle handle)
{
	Actor* currentActor = GetActor();
	if (currentActor != nullptr)
	{
		currentActor->m_controller = nullptr;
		currentActor->OnUnpossessed();
	}

	m_actorHandle = handle;

	Actor* newActor = GetActor();
	if (newActor != nullptr)
	{
		newActor->m_controller = this;
		newActor->OnPossessed();
	}
}