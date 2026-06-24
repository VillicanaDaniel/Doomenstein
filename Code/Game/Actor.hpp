#pragma once

#include "Game/ActorHandle.hpp"
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/EulerAngles.hpp"
#include "Engine/Math/Mat44.hpp"
#include "Engine/Math/FloatRange.hpp"
#include <string>
#include <vector>

class Controller;
class Game;
class Map;
class ActorDefinition;
class Weapon;
class Camera;
struct ActorAnimationGroupDefinition;
struct ActorAnimationDirectionDefinition;
struct Vertex;
struct AABB2;

class Actor
{
public:
	Actor(Game* game, Map* map);
	virtual ~Actor();

	virtual void Update();
	virtual void UpdatePhysics();
	virtual void Render(Camera const& camera) const;

	virtual void OnPossessed();
	virtual void OnUnpossessed();

	virtual void AddForce(Vec3 const& force);
	virtual void AddImpulse(Vec3 const& impulse);
	virtual void MoveInDirection(Vec3 const& direction, float speed);
	virtual void TurnInDirection(Vec3 const& direction, float maxTurnDegrees);
	virtual void AddYawInput(float deltaYaw);
	virtual void AddPitchInput(float deltaPitch);
	virtual void Attack();

	virtual void Damage(float amount, ActorHandle sourceHandle = ActorHandle::INVALID);
	virtual Mat44 GetModelToWorldTransform() const;

	void UpdateWeapons();
	void EquipWeapon(int index);
	Weapon* GetEquippedWeapon() const;

	ActorAnimationGroupDefinition const* GetCurrentAnimGroup() const;
	ActorAnimationDirectionDefinition const* GetBestAnimDirectionForCamera( ActorAnimationGroupDefinition const& group, Camera const& camera) const;
	ActorAnimationDirectionDefinition const* GetBestAnimDirectionForFacing( ActorAnimationGroupDefinition const& group) const;
	void AddVertsForActorBillboard(std::vector<Vertex>& verts, AABB2 const& uv, Camera const& camera) const;

	void PlayAnimationGroup(std::string const& animName);
	void PlaySound(std::string const& soundName);

public:
	ActorHandle m_handle = ActorHandle::INVALID;
	unsigned int m_actorUID = 0;

	Game* m_game = nullptr;
	Map* m_map = nullptr;
	ActorDefinition const* m_definition = nullptr;

	Vec3 m_position = Vec3::ZERO;
	EulerAngles m_orientation = EulerAngles(0.f, 0.f, 0.f);
	Rgba8 m_color = Rgba8::WHITE;

	float m_physicsRadius = 0.5f;
	float m_physicsHeight = 1.0f;

	bool m_isStatic = false;
	bool m_canFly = false;

	EulerAngles m_angularVelocity = EulerAngles(0.f, 0.f, 0.f);

	Vec3 m_velocity = Vec3::ZERO;
	Vec3 m_acceleration = Vec3::ZERO;

	float m_health = 100.f;
	float m_maxHealth = 100.f; //For Health Bars
	bool m_isDead = false;
	float m_timeSinceDeath = 0.f;
	float m_corpseLifetime = 2.0f;
	float m_destroyAboveZ = 8.f; //For cleanup
	bool m_ignoreFloorCollision = false; //For Gengar

	Controller* m_controller = nullptr;
	Controller* m_aiController = nullptr;
	ActorHandle m_owner = ActorHandle::INVALID;

	bool m_isPossessed = false;
	bool m_visible = true;
	bool m_canBePossessed = false;

	bool m_collidesWithWorld = true;
	bool m_collidesWithActors = true;
	bool m_dieOnCollide = false;
	FloatRange m_damageOnCollide = FloatRange(0.f, 0.f);
	float m_impulseOnCollide = 0.f;
	bool m_canBeHitByRaycast = true;

	bool m_simulated = true;
	float m_walkSpeed = 4.f;
	float m_runSpeed = 8.f;
	float m_drag = 3.f;
	float m_turnSpeed = 120.f;

	float m_eyeHeight = 0.5f;
	float m_cameraFOVDegrees = 60.f;

	bool m_aiEnabled = false;
	float m_sightRadius = 0.f;
	float m_sightAngle = 0.f;

	std::string m_faction = "NEUTRAL";

	std::vector<Weapon*> m_weapons;
	int m_equippedWeaponIndex = 0;

	//For Gold
	Vec3 m_spriteFacingDirection = Vec3(0.f, -1.f, 0.f);
	bool m_useSpriteFacingDirection = false;
	bool m_onlyAnimateWhenMoving = false;
	bool m_isOverworldMoving = false;

	float m_invincibilityTimer = 0.f;
	float m_invincibilityDuration = 1.5f;

	bool IsInvincible() const;
	void BecomeInvincible(float durationSeconds);

	//Animation
	std::string m_currentAnimGroupName;
	float m_animTime = 0.f;
};