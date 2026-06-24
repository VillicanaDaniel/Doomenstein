#pragma once

#include "Engine/Core/XmlUtils.hpp"
#include "Engine/Math/FloatRange.hpp"
#include "Engine/Renderer/SpriteAnimDefinition.hpp"
#include "Engine/Audio/AudioSystem.hpp"
#include <string>
#include <vector>

struct WeaponHUDDefinition
{
	std::string m_shaderName = "Default";
	std::string m_baseTextureName = "";
	std::string m_reticleTextureName = "";

	Vec2 m_reticleSize = Vec2(32.f, 32.f);
	Vec2 m_spriteSize = Vec2(256.f, 256.f);
	Vec2 m_spritePivot = Vec2(0.5f, 0.f);

	Texture* m_baseTexture = nullptr;
	Texture* m_reticleTexture = nullptr;
};

struct WeaponAnimationDefinition
{
	std::string m_name = "Idle";
	std::string m_shaderName = "Default";
	std::string m_spriteSheetName = "";
	IntVec2 m_cellCount = IntVec2(1, 1);

	float m_secondsPerFrame = 1.f;
	int m_startFrame = 0;
	int m_endFrame = 0;
	float m_totalDuration = 1.f;

	SpriteSheet* m_spriteSheet = nullptr;
	SpriteAnimDefinition* m_animDef = nullptr;
};


class WeaponDefinition
{
public:
	WeaponDefinition() = default;
	explicit WeaponDefinition(XmlElement const& element);

	static void InitializeWeaponDefs(char const* xmlFilePath);
	static WeaponDefinition const* GetByName(std::string const& name);
	static void ClearDefinitions();

public:
	std::string m_name = "";

	float m_refireTime = 0.25f;

	// Raycast weapon data
	int m_rayCount = 0;
	float m_rayCone = 0.f;
	float m_rayRange = 0.f;
	FloatRange m_rayDamage = FloatRange(0.f, 0.f);
	float m_rayImpulse = 0.f;

	// Projectile weapon data
	int m_projectileCount = 0;
	std::string m_projectileActor = "";
	float m_projectileCone = 0.f;
	float m_projectileSpeed = 0.f;

	// Melee weapon data
	int m_meleeCount = 0;
	float m_meleeArc = 0.f;
	float m_meleeRange = 0.f;
	FloatRange m_meleeDamage = FloatRange(0.f, 0.f);
	float m_meleeImpulse = 0.f;

	static std::vector<WeaponDefinition*> s_definitions;
	WeaponHUDDefinition m_hud;
	std::vector<WeaponAnimationDefinition*> m_animations;

	SoundID m_fireSound = MISSING_SOUND_ID;
};