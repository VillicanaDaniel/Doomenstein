#pragma once
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/EulerAngles.hpp"
#include <vector>
#include <string>

class Map;
class Prop;
class Actor;

struct PokemonSpawnInfo
{
	std::string m_name;
	char const* m_objPath = nullptr;
	char const* m_bodyMaterialName = nullptr;
	char const* m_bodyTexturePath = nullptr;
	char const* m_eyeMaterialName = nullptr;
	char const* m_eyeTexturePath = nullptr;
	char const* m_weaponName = nullptr;
	float m_maxHealth;
	Vec3 m_position;
	EulerAngles m_orientation;
};

//Gengar Boss Stuff
enum class GengarAttackState
{
	None,
	Sinking,
	Warning,
	SpawningProjectiles,
	Rising
};

enum class GengarAttackType
{
	None,
	RingAttack,
	ShadowBallStorm
};

class PokemonParty
{
public:
	explicit PokemonParty(Map* map);

	void Update();
	void UpdateActivePokemonMovement();
	void UpdatePokemonAttack();

	void InitializeDefaultParty();
	void SpawnNextPokemon();
	void NotifyActorDeleted(Prop* prop);

	Prop* GetActivePokemon() const 
	{ 
		return m_activePokemon; 
	}

public:
	Map* m_map = nullptr;
	std::vector<PokemonSpawnInfo> m_party;
	int m_activeIndex = -1;
	Prop* m_activePokemon = nullptr;

	float m_movementTimer = 0.f;
	float m_chatotMoveDir = 1.f;

	//Porygon Stuff
	Vec3 m_porygonTargetPos = Vec3::ZERO;
	bool m_porygonDiveActive = false;
	float m_porygonDiveTimer = 2.5f;
	float m_porygonDiveCooldown = 3.5f;
	Vec3 m_porygonDiveTarget = Vec3::ZERO;

	void UpdatePorygonDiveAttack(Actor* playerActor);
	void SpawnPorygonProjectileCircle();

	//Gengar Stuff
	void UpdateGengarAttack(Actor* playerActor);
	void StartGengarAttack(Actor* playerActor);
	void SpawnGengarRings(Actor* playerActor);
	void SpawnGengarShadowBall(Vec3 const& ringPos);
	void SpawnShadowBallStorm(Actor* playerActor);
	void RenderGengarAttackWarning() const;

	GengarAttackState m_gengarAttackState = GengarAttackState::None;
	GengarAttackType m_gengarAttackType = GengarAttackType::None;

	float m_gengarAttackTimer = 2.f;
	float m_gengarStateTimer = 0.f;

	Vec3 m_gengarSinkStartPos = Vec3::ZERO;
	Vec3 m_gengarRiseTargetPos = Vec3::ZERO;

	float m_gengarRingRadius = 3.0f;
	int m_shadowBallsPerRing = 20;

	float m_gengarShotTimer = 0.f;
	float m_gengarStormDuration = 2.5f;

	std::vector<Vec3> m_gengarRingPositions;
	int m_gengarNextRingIndex = 0;
};