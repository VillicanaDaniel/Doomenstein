#pragma once
#include "Game/WeaponDefinition.hpp"

class Actor;
struct Vec3;

class Weapon
{
public:
	explicit Weapon(WeaponDefinition const* definition);
	~Weapon() = default;

	void Update(float deltaSeconds);
	bool CanFire() const;
	void Fire(Actor* owner);
	Vec3 GetRandomDirectionInCone(Vec3 const& forward, float coneDegrees);
	void PlayAnimation(std::string const& animName);
	WeaponAnimationDefinition const* GetCurrentAnimation() const;
	void RenderHUD(AABB2 const& screenBounds) const;

public:
	WeaponDefinition const* m_definition = nullptr;
	float m_cooldownSeconds = 0.f;

	std::string m_currentAnimName = "Idle";
	float m_animTime = 0.f;
};