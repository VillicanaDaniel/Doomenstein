#include "Game/ActorDefinition.hpp"
#include "Engine/Core/Engine.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/XmlUtils.hpp"
#include <cstdlib>

std::vector<ActorDefinition*> ActorDefinition::s_definitions;

BillboardType ActorDefinition::GetBillboardTypeFromString(std::string const& text)
{
	if (text == "WorldUpFacing") return BillboardType::WORLD_UP_FACING;
	if (text == "WorldUpOpposing") return BillboardType::WORLD_UP_OPPOSING;
	if (text == "FullFacing") return BillboardType::FULL_FACING;
	if (text == "FullOpposing") return BillboardType::FULL_OPPOSING;

	return BillboardType::NONE;
}

ActorDefinition::ActorDefinition(XmlElement const& element)
{
	m_name = ParseXmlAttribute(element, "name", "");
	GUARANTEE_OR_DIE(!m_name.empty(), "ActorDefinition missing required name attribute");

	m_visible = ParseXmlAttribute(element, "visible", m_visible);
	m_health = ParseXmlAttribute(element, "health", m_health);
	m_corpseLifetime = ParseXmlAttribute(element, "corpseLifetime", m_corpseLifetime);
	m_canBePossessed = ParseXmlAttribute(element, "canBePossessed", m_canBePossessed);
	m_dieOnSpawn = ParseXmlAttribute(element, "dieOnSpawn", m_dieOnSpawn);

	std::string factionString = ParseXmlAttribute(element, "faction", std::string("Neutral"));
	m_faction = GetFactionFromString(factionString);

	XmlElement const* collisionElem = element.FirstChildElement("Collision");
	if (collisionElem != nullptr)
	{
		m_physicsRadius = ParseXmlAttribute(*collisionElem, "radius", m_physicsRadius);
		m_physicsHeight = ParseXmlAttribute(*collisionElem, "height", m_physicsHeight);
		m_collidesWithWorld = ParseXmlAttribute(*collisionElem, "collidesWithWorld", m_collidesWithWorld);
		m_collidesWithActors = ParseXmlAttribute(*collisionElem, "collidesWithActors", m_collidesWithActors);
		m_dieOnCollide = ParseXmlAttribute(*collisionElem, "dieOnCollide", m_dieOnCollide);

		std::string damageRangeStr = ParseXmlAttribute(*collisionElem, "damageOnCollide", std::string("0~0"));
		size_t tildePos = damageRangeStr.find('~');
		if (tildePos != std::string::npos)
		{
			std::string minStr = damageRangeStr.substr(0, tildePos);
			std::string maxStr = damageRangeStr.substr(tildePos + 1);

			m_damageOnCollide.m_min = (float)atof(minStr.c_str());
			m_damageOnCollide.m_max = (float)atof(maxStr.c_str());
		}
		else
		{
			float value = (float)atof(damageRangeStr.c_str());
			m_damageOnCollide.m_min = value;
			m_damageOnCollide.m_max = value;
		}

		m_impulseOnCollide = ParseXmlAttribute(*collisionElem, "impulseOnCollide", m_impulseOnCollide);
	}

	XmlElement const* physicsElem = element.FirstChildElement("Physics");
	if (physicsElem != nullptr)
	{
		m_simulated = ParseXmlAttribute(*physicsElem, "simulated", m_simulated);
		m_flying = ParseXmlAttribute(*physicsElem, "flying", m_flying);
		m_walkSpeed = ParseXmlAttribute(*physicsElem, "walkSpeed", m_walkSpeed);
		m_runSpeed = ParseXmlAttribute(*physicsElem, "runSpeed", m_runSpeed);
		m_drag = ParseXmlAttribute(*physicsElem, "drag", m_drag);
		m_turnSpeed = ParseXmlAttribute(*physicsElem, "turnSpeed", m_turnSpeed);
	}

	XmlElement const* cameraElem = element.FirstChildElement("Camera");
	if (cameraElem != nullptr)
	{
		m_eyeHeight = ParseXmlAttribute(*cameraElem, "eyeHeight", m_eyeHeight);
		m_cameraFOVDegrees = ParseXmlAttribute(*cameraElem, "cameraFOV", m_cameraFOVDegrees);
	}

	XmlElement const* aiElem = element.FirstChildElement("AI");
	if (aiElem != nullptr)
	{
		m_aiEnabled = ParseXmlAttribute(*aiElem, "aiEnabled", m_aiEnabled);
		m_sightRadius = ParseXmlAttribute(*aiElem, "sightRadius", m_sightRadius);
		m_sightAngle = ParseXmlAttribute(*aiElem, "sightAngle", m_sightAngle);
	}

	XmlElement const* inventoryElem = element.FirstChildElement("Inventory");
	if (inventoryElem != nullptr)
	{
		for (XmlElement const* weaponElem = inventoryElem->FirstChildElement("Weapon");
			weaponElem != nullptr;
			weaponElem = weaponElem->NextSiblingElement("Weapon"))
		{
			std::string weaponName = ParseXmlAttribute(*weaponElem, "name", std::string(""));
			if (!weaponName.empty())
			{
				m_weaponNames.push_back(weaponName);
			}
		}
	}

	//For visuals
	XmlElement const* baseElem = element.FirstChildElement("Base");
	if (baseElem != nullptr)
	{
		m_dieOnSpawn = ParseXmlAttribute(*baseElem, "dieOnSpawn", m_dieOnSpawn);
	}

	XmlElement const* visualsElem = element.FirstChildElement("Visuals");
	if (visualsElem != nullptr)
	{
		m_visuals.m_size = ParseXmlAttribute(*visualsElem, "size", m_visuals.m_size);
		m_visuals.m_pivot = ParseXmlAttribute(*visualsElem, "pivot", m_visuals.m_pivot);
		m_visuals.m_renderLit = ParseXmlAttribute(*visualsElem, "renderLit", m_visuals.m_renderLit);
		m_visuals.m_renderRounded = ParseXmlAttribute(*visualsElem, "renderRounded", m_visuals.m_renderRounded);
		m_visuals.m_shaderName = ParseXmlAttribute(*visualsElem, "shader", m_visuals.m_shaderName);
		m_visuals.m_spriteSheetName = ParseXmlAttribute(*visualsElem, "spriteSheet", m_visuals.m_spriteSheetName);
		m_visuals.m_cellCount = ParseXmlAttribute(*visualsElem, "cellCount", m_visuals.m_cellCount);

		std::string billboardType = ParseXmlAttribute(*visualsElem, "billboardType", std::string("None"));
		m_visuals.m_billboardType = GetBillboardTypeFromString(billboardType);
	}

	//For Sprite Animation
	if (visualsElem != nullptr)
	{
		Texture* texture = g_engine->m_render->CreateOrGetTextureFromFile(
			m_visuals.m_spriteSheetName.c_str()
		);

		m_spriteSheet = new SpriteSheet(*texture, m_visuals.m_cellCount);

		for (XmlElement const* animGroupElem = visualsElem->FirstChildElement("AnimationGroup");
			animGroupElem != nullptr;
			animGroupElem = animGroupElem->NextSiblingElement("AnimationGroup"))
		{
			ActorAnimationGroupDefinition* group = new ActorAnimationGroupDefinition();

			group->m_name = ParseXmlAttribute(*animGroupElem, "name", std::string(""));
			group->m_secondsPerFrame = ParseXmlAttribute(*animGroupElem, "secondsPerFrame", group->m_secondsPerFrame);
			group->m_scaleBySpeed = ParseXmlAttribute(*animGroupElem, "scaleBySpeed", group->m_scaleBySpeed);

			std::string playback = ParseXmlAttribute(*animGroupElem, "playbackMode", std::string("Once"));
			group->m_playbackType = GetPlaybackTypeFromString(playback);

			for (XmlElement const* dirElem = animGroupElem->FirstChildElement("Direction");
				dirElem != nullptr;
				dirElem = dirElem->NextSiblingElement("Direction"))
			{
				Vec3 direction = ParseXmlAttribute(*dirElem, "vector", Vec3(1.f, 0.f, 0.f));

				XmlElement const* animElem = dirElem->FirstChildElement("Animation");
				if (animElem == nullptr)
				{
					continue;
				}

				ActorAnimationDirectionDefinition animDir;

				int startFrame = ParseXmlAttribute(*animElem, "startFrame", 0);
				int endFrame = ParseXmlAttribute(*animElem, "endFrame", 0);
				animDir.m_startFrame = startFrame;
				animDir.m_endFrame = endFrame;

				animDir.m_direction = direction.GetNormalized();
				int frameCount = (endFrame - startFrame) + 1;
				float totalDuration = group->m_secondsPerFrame * (float)frameCount;

				animDir.m_animDef = new SpriteAnimDefinition(
					*m_spriteSheet,
					startFrame,
					endFrame,
					group->m_secondsPerFrame, // IMPORTANT: pass seconds PER FRAME here
					group->m_playbackType
				);

				group->m_totalDuration = totalDuration; // use this only for returning to Walk

				group->m_directions.push_back(animDir);
			}

			m_animationGroups.push_back(group);
		}
	}
	//For Sounds
	XmlElement const* soundsElem = element.FirstChildElement("Sounds");
	if (soundsElem != nullptr)
	{
		for (XmlElement const* soundElem = soundsElem->FirstChildElement("Sound");
			soundElem != nullptr;
			soundElem = soundElem->NextSiblingElement("Sound"))
		{
			std::string soundType = ParseXmlAttribute(*soundElem, "sound", std::string(""));
			std::string soundPath = ParseXmlAttribute(*soundElem, "name", std::string(""));

			if (!soundType.empty() && !soundPath.empty())
			{
				m_sounds[soundType] = g_engine->m_audio->CreateOrGetSound(soundPath, true);
			}
		}
	}
}

void ActorDefinition::InitializeActorDefs(char const* xmlFilePath)
{
	ClearDefinitions();
	LoadActorDefsFromXml(xmlFilePath);

}


void ActorDefinition::LoadActorDefsFromXml(char const* xmlFilePath)
{
	XmlDocument doc;
	int result = doc.LoadFile(xmlFilePath);
	GUARANTEE_OR_DIE(result == 0, "Failed to load actor definition xml");

	XmlElement* root = doc.RootElement();
	GUARANTEE_OR_DIE(root != nullptr, "Actor definition xml missing root element");

	for (XmlElement const* actorElem = root->FirstChildElement("ActorDefinition");
		actorElem != nullptr;
		actorElem = actorElem->NextSiblingElement("ActorDefinition"))
	{
		ActorDefinition* newDef = new ActorDefinition(*actorElem);
		s_definitions.push_back(newDef);
	}
}

ActorDefinition const* ActorDefinition::GetByName(std::string const& name)
{
	for (ActorDefinition const* def : s_definitions)
	{
		if (def != nullptr && def->m_name == name)
		{
			return def;
		}
	}
	return nullptr;
}

void ActorDefinition::ClearDefinitions()
{
	for (ActorDefinition*& def : s_definitions)
	{
		delete def;
		def = nullptr;
	}
	s_definitions.clear();
}

Faction ActorDefinition::GetFactionFromString(std::string const& factionString)
{
	if (factionString == "MARINE" || factionString == "Marine")
	{
		return Faction::MARINE;
	}
	if (factionString == "DEMON" || factionString == "Demon")
	{
		return Faction::DEMON;
	}
	return Faction::NEUTRAL;
}