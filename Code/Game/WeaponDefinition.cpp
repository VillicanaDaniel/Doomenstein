#include "Game/WeaponDefinition.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/XmlUtils.hpp"
#include "Engine/Core/Engine.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include <cstdlib>

std::vector<WeaponDefinition*> WeaponDefinition::s_definitions;

static FloatRange ParseFloatRangeFromString(std::string const& text)
{
	FloatRange range;
	size_t tildePos = text.find('~');

	if (tildePos != std::string::npos)
	{
		std::string minStr = text.substr(0, tildePos);
		std::string maxStr = text.substr(tildePos + 1);

		range.m_min = (float)atof(minStr.c_str());
		range.m_max = (float)atof(maxStr.c_str());
	}
	else
	{
		float value = (float)atof(text.c_str());
		range.m_min = value;
		range.m_max = value;
	}

	return range;
}

WeaponDefinition::WeaponDefinition(XmlElement const& element)
{
	m_name = ParseXmlAttribute(element, "name", "");
	GUARANTEE_OR_DIE(!m_name.empty(), "WeaponDefinition missing required name attribute");

	m_refireTime = ParseXmlAttribute(element, "refireTime", m_refireTime);

	// Raycast fields
	m_rayCount = ParseXmlAttribute(element, "rayCount", m_rayCount);
	m_rayCone = ParseXmlAttribute(element, "rayCone", m_rayCone);
	m_rayRange = ParseXmlAttribute(element, "rayRange", m_rayRange);

	{
		std::string rayDamageStr = ParseXmlAttribute(element, "rayDamage", std::string("0~0"));
		m_rayDamage = ParseFloatRangeFromString(rayDamageStr);
	}

	m_rayImpulse = ParseXmlAttribute(element, "rayImpulse", m_rayImpulse);

	// Projectile fields
	m_projectileCount = ParseXmlAttribute(element, "projectileCount", m_projectileCount);
	m_projectileActor = ParseXmlAttribute(element, "projectileActor", std::string(""));
	m_projectileCone = ParseXmlAttribute(element, "projectileCone", m_projectileCone);
	m_projectileSpeed = ParseXmlAttribute(element, "projectileSpeed", m_projectileSpeed);

	// Melee fields
	m_meleeCount = ParseXmlAttribute(element, "meleeCount", m_meleeCount);
	m_meleeArc = ParseXmlAttribute(element, "meleeArc", m_meleeArc);
	m_meleeRange = ParseXmlAttribute(element, "meleeRange", m_meleeRange);

	{
		std::string meleeDamageStr = ParseXmlAttribute(element, "meleeDamage", std::string("0~0"));
		m_meleeDamage = ParseFloatRangeFromString(meleeDamageStr);
	}

	m_meleeImpulse = ParseXmlAttribute(element, "meleeImpulse", m_meleeImpulse);

	//For HUD
	XmlElement const* hudElem = element.FirstChildElement("HUD");
	if (hudElem != nullptr)
	{
		m_hud.m_shaderName = ParseXmlAttribute(*hudElem, "shader", m_hud.m_shaderName);
		m_hud.m_baseTextureName = ParseXmlAttribute(*hudElem, "baseTexture", m_hud.m_baseTextureName);
		m_hud.m_reticleTextureName = ParseXmlAttribute(*hudElem, "reticleTexture", m_hud.m_reticleTextureName);
		m_hud.m_reticleSize = ParseXmlAttribute(*hudElem, "reticleSize", m_hud.m_reticleSize);
		m_hud.m_spriteSize = ParseXmlAttribute(*hudElem, "spriteSize", m_hud.m_spriteSize);
		m_hud.m_spritePivot = ParseXmlAttribute(*hudElem, "spritePivot", m_hud.m_spritePivot);

		if (!m_hud.m_baseTextureName.empty())
		{
			m_hud.m_baseTexture = g_engine->m_render->CreateOrGetTextureFromFile(
				m_hud.m_baseTextureName.c_str()
			);
		}

		if (!m_hud.m_reticleTextureName.empty())
		{
			m_hud.m_reticleTexture = g_engine->m_render->CreateOrGetTextureFromFile(
				m_hud.m_reticleTextureName.c_str()
			);
		}
	}

	//For weapons
	if (hudElem != nullptr)
	{
		for (XmlElement const* animElem = hudElem->FirstChildElement("Animation");
			animElem != nullptr;
			animElem = animElem->NextSiblingElement("Animation"))
		{
			WeaponAnimationDefinition* anim = new WeaponAnimationDefinition();

			anim->m_name = ParseXmlAttribute(*animElem, "name", anim->m_name);
			anim->m_shaderName = ParseXmlAttribute(*animElem, "shader", anim->m_shaderName);
			anim->m_spriteSheetName = ParseXmlAttribute(*animElem, "spriteSheet", anim->m_spriteSheetName);
			anim->m_cellCount = ParseXmlAttribute(*animElem, "cellCount", anim->m_cellCount);
			anim->m_secondsPerFrame = ParseXmlAttribute(*animElem, "secondsPerFrame", anim->m_secondsPerFrame);
			anim->m_startFrame = ParseXmlAttribute(*animElem, "startFrame", anim->m_startFrame);
			anim->m_endFrame = ParseXmlAttribute(*animElem, "endFrame", anim->m_endFrame);

			if (!anim->m_spriteSheetName.empty())
			{
				Texture* texture = g_engine->m_render->CreateOrGetTextureFromFile(
					anim->m_spriteSheetName.c_str()
				);

				anim->m_spriteSheet = new SpriteSheet(*texture, anim->m_cellCount);

				int frameCount = anim->m_endFrame - anim->m_startFrame + 1;
				anim->m_totalDuration = anim->m_secondsPerFrame * (float)frameCount;

				anim->m_animDef = new SpriteAnimDefinition(
					*anim->m_spriteSheet,
					anim->m_startFrame,
					anim->m_endFrame,
					anim->m_totalDuration,
					anim->m_name == "Idle" ? SpriteAnimPlaybackType::LOOP : SpriteAnimPlaybackType::ONCE
				);

				m_animations.push_back(anim);
			}
		}
	}
	//For weapon Audio
	XmlElement const* soundsElem = element.FirstChildElement("Sounds");
	if (soundsElem != nullptr)
	{
		for (XmlElement const* soundElem = soundsElem->FirstChildElement("Sound");
			soundElem != nullptr;
			soundElem = soundElem->NextSiblingElement("Sound"))
		{
			std::string soundType = ParseXmlAttribute(*soundElem, "sound", std::string(""));
			std::string soundPath = ParseXmlAttribute(*soundElem, "name", std::string(""));

			if (soundType == "Fire" && !soundPath.empty())
			{
				m_fireSound = g_engine->m_audio->CreateOrGetSound(soundPath, true);
			}
		}
	}
}

void WeaponDefinition::InitializeWeaponDefs(char const* xmlFilePath)
{ 
	ClearDefinitions();

	XmlDocument doc;
	int result = doc.LoadFile(xmlFilePath);
	GUARANTEE_OR_DIE(result == 0, "Failed to load WeaponDefinitions.xml");

	XmlElement* root = doc.RootElement();
	GUARANTEE_OR_DIE(root != nullptr, "WeaponDefinitions.xml missing root element");

	for (XmlElement const* weaponElem = root->FirstChildElement("WeaponDefinition");
		weaponElem != nullptr;
		weaponElem = weaponElem->NextSiblingElement("WeaponDefinition"))
	{
		WeaponDefinition* newDef = new WeaponDefinition(*weaponElem);
		s_definitions.push_back(newDef);
	}
}

WeaponDefinition const* WeaponDefinition::GetByName(std::string const& name)
{
	for (WeaponDefinition const* def : s_definitions)
	{
		if (def != nullptr && def->m_name == name)
		{
			return def;
		}
	}
	return nullptr;
}

void WeaponDefinition::ClearDefinitions()
{
	for (WeaponDefinition*& def : s_definitions)
	{
		delete def;
		def = nullptr;
	}
	s_definitions.clear();
}