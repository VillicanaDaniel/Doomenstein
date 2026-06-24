#pragma once

#include "Engine/Core/XmlUtils.hpp"
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Math/FloatRange.hpp"
#include "Engine/Renderer/SpriteAnimDefinition.hpp"
#include "Engine/Audio/AudioSystem.hpp"
#include <map>
#include <string>
#include <vector>

class WeaponDefinition;

enum class Faction
{
	NEUTRAL = 0,
	MARINE,
	DEMON
};

struct ActorVisualsDefinition
{
	Vec2 m_size = Vec2(1.f, 1.f);
	Vec2 m_pivot = Vec2(0.5f, 0.5f);
	BillboardType m_billboardType = BillboardType::NONE;
	bool m_renderLit = false;
	bool m_renderRounded = false;
	std::string m_shaderName = "Default";
	std::string m_spriteSheetName = "Default";
	IntVec2 m_cellCount = IntVec2(1, 1);
};

struct ActorAnimationDirectionDefinition
{
	Vec3 m_direction = Vec3(1.f, 0.f, 0.f);
	SpriteAnimDefinition* m_animDef = nullptr;

	int m_startFrame = 0;
	int m_endFrame = 0;
};

struct ActorAnimationGroupDefinition
{
	std::string m_name;
	float m_secondsPerFrame = 1.f;
	SpriteAnimPlaybackType m_playbackType = SpriteAnimPlaybackType::ONCE;
	bool m_scaleBySpeed = false;
	std::vector<ActorAnimationDirectionDefinition> m_directions;
	float m_totalDuration = 0.f;
};

class ActorDefinition
{
public:
	ActorDefinition() = default;
	explicit ActorDefinition(XmlElement const& element);

	static void InitializeActorDefs(char const* xmlFilePath);
	static void LoadActorDefsFromXml(char const* xmlFilePath);
	static ActorDefinition const* GetByName(std::string const& name);
	static void ClearDefinitions();

	static Faction GetFactionFromString(std::string const& factionString);
	BillboardType GetBillboardTypeFromString(std::string const& text);
	SpriteAnimPlaybackType GetPlaybackTypeFromString(std::string const& text)
	{
		if (text == "Loop" || text == "LOOP")
		{
			return SpriteAnimPlaybackType::LOOP;
		}

		if (text == "PingPong" || text == "PINGPONG")
		{
			return SpriteAnimPlaybackType::PINGPONG;
		}

		return SpriteAnimPlaybackType::ONCE;
	}


public:
	std::string m_name = "";

	// Base
	bool m_visible = false;
	float m_health = 1.f;
	float m_corpseLifetime = 0.f;
	Faction m_faction = Faction::NEUTRAL;
	bool m_canBePossessed = false;

	// Collision
	float m_physicsRadius = 0.f;
	float m_physicsHeight = 0.f;
	bool m_collidesWithWorld = false;
	bool m_collidesWithActors = false;
	bool m_dieOnCollide = false;
	FloatRange m_damageOnCollide = FloatRange(0.f, 0.f);
	float m_impulseOnCollide = 0.f;

	// Physics
	bool m_simulated = false;
	bool m_flying = false;
	float m_walkSpeed = 0.f;
	float m_runSpeed = 0.f;
	float m_drag = 0.f;
	float m_turnSpeed = 0.f;

	// Camera
	float m_eyeHeight = 0.f;
	float m_cameraFOVDegrees = 60.f;

	// AI
	bool m_aiEnabled = false;
	float m_sightRadius = 0.f;
	float m_sightAngle = 0.f;

	// Weapons
	std::vector<std::string> m_weaponNames;

public:
	static std::vector<ActorDefinition*> s_definitions;
	ActorVisualsDefinition m_visuals;
	bool m_dieOnSpawn = false;
	std::vector<ActorAnimationGroupDefinition*> m_animationGroups;
	SpriteSheet* m_spriteSheet = nullptr;

	std::map<std::string, SoundID> m_sounds;
};