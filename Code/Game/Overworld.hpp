#pragma once

#include "Game/Map.hpp"
#include "Engine/Math/IntVec2.hpp"
#include "Engine/Math/Vec3.hpp"

class Game;
class MapDefinition;
class Actor;
class Camera;

struct SnowParticle
{
	Vec2 m_position;
	float m_speed = 0.f;
	float m_size = 8.f;
};

class Overworld : public Map
{
public:
	Overworld(Game* game, MapDefinition const* definition);
	virtual ~Overworld() = default;

	void Update() override;
	void Render(Camera const& camera, Actor const* controlledActor) const override;
	
	//Snow Effect
	void InitializeSnow();
	void UpdateSnow();
	void RenderSnow(Camera const& screenCamera) const;


private:
	void UpdateOverworldCamera();
	void UpdateGridMovement();
	bool CanMoveToTile(IntVec2 const& tileCoords) const;
	void StartBattleIfTrainerSeesPlayer();

private:
	IntVec2 m_playerTile = IntVec2::ZERO;
	bool m_hasInitializedPlayerTile = false;
	IntVec2 m_moveTargetTile = IntVec2::ZERO;
	bool m_isMovingTile = false;
	float m_moveT = 0.f;
	float m_secondsPerTileMove = 0.18f;

	//Snow Effect
	std::vector<SnowParticle> m_snowParticles;
	Texture* m_snowTexture = nullptr;
};