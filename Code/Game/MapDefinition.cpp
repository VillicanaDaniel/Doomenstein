#include "Game/MapDefinition.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/XmlUtils.hpp"
#include "ThirdParty/tinyxml2/tinyxml2.h"

std::vector<MapDefinition*> MapDefinition::s_definitions;

static Vec3 ParseVec3FromString(std::string const& text, Vec3 const& defaultValue)
{
	Strings parts = SplitStringOnDelimiter(text, ',');
	if (parts.size() != 3)
	{
		return defaultValue;
	}

	return Vec3(
		(float)atof(parts[0].c_str()),
		(float)atof(parts[1].c_str()),
		(float)atof(parts[2].c_str())
	);
}

static EulerAngles ParseEulerAnglesFromString(std::string const& text, EulerAngles const& defaultValue)
{
	Strings parts = SplitStringOnDelimiter(text, ',');
	if (parts.size() != 3)
	{
		return defaultValue;
	}

	return EulerAngles(
		(float)atof(parts[0].c_str()),
		(float)atof(parts[1].c_str()),
		(float)atof(parts[2].c_str())
	);
}

MapDefinition::MapDefinition(XmlElement const& element)
{
	m_name = ParseXmlAttribute(element, "name", "");
	m_image = ParseXmlAttribute(element, "image", "");
	m_shader = ParseXmlAttribute(element, "shader", "");
	m_spriteSheetTexture = ParseXmlAttribute(element, "spriteSheetTexture", "");
	m_spriteSheetCellCount = ParseXmlAttribute(element, "spriteSheetCellCount", IntVec2(8, 8));

	XmlElement const* spawnInfosElem = element.FirstChildElement("SpawnInfos");
	if (spawnInfosElem != nullptr)
	{
		for (XmlElement const* spawnElem = spawnInfosElem->FirstChildElement("SpawnInfo"); spawnElem != nullptr; spawnElem = spawnElem->NextSiblingElement("SpawnInfo"))
		{
			SpawnInfo info;
			info.m_actor = ParseXmlAttribute(*spawnElem, "actor", "");
			info.m_faction = ParseXmlAttribute(*spawnElem, "faction", "");

			std::string positionText = ParseXmlAttribute(*spawnElem, "position", "");
			std::string orientationText = ParseXmlAttribute(*spawnElem, "orientation", "");

			info.m_position = ParseVec3FromString(positionText, Vec3::ZERO);
			info.m_orientation = ParseEulerAnglesFromString(orientationText, EulerAngles());

			m_spawnInfos.push_back(info);
		}
	}

	// Backward-compatible single image support
	if (!m_image.empty())
	{
		m_layerImagePaths.push_back(m_image);
	}

	// New layered image support
	XmlElement const* layersElem = element.FirstChildElement("Layers");
	if (layersElem != nullptr)
	{
		for (XmlElement const* layerElem = layersElem->FirstChildElement("Layer");
			layerElem != nullptr;
			layerElem = layerElem->NextSiblingElement("Layer"))
		{
			std::string layerImage = ParseXmlAttribute(*layerElem, "image", "");

			if (!layerImage.empty())
			{
				m_layerImagePaths.push_back(layerImage);
			}
		}
	}
}

void MapDefinition::InitializeMapDefs(char const* xmlFilePath)
{
	XmlDocument doc;
	doc.LoadFile(xmlFilePath);

	XmlElement* root = doc.RootElement();
	GUARANTEE_OR_DIE(root != nullptr, "MapDefinition::InitializeMapDefs - failed to load xml root");

	for (XmlElement const* elem = root->FirstChildElement("MapDefinition"); elem != nullptr; elem = elem->NextSiblingElement("MapDefinition"))
	{
		MapDefinition* def = new MapDefinition(*elem);
		s_definitions.push_back(def);
	}
}

MapDefinition const* MapDefinition::GetByName(std::string const& name)
{
	for (MapDefinition const* def : s_definitions)
	{
		if (def->m_name == name)
		{
			return def;
		}
	}
	return nullptr;
}

AABB2 MapDefinition::GetSpriteUVs(IntVec2 const& spriteCoords) const
{
	Vec2 cellSize(
		1.f / (float)m_spriteSheetCellCount.x,
		1.f / (float)m_spriteSheetCellCount.y
	);


	int invertedY = (m_spriteSheetCellCount.y - 1) - spriteCoords.y;
	float offset = 0.001f;

	Vec2 mins(
		(float)spriteCoords.x * cellSize.x + offset,
		(float)invertedY * cellSize.y + offset
	);

	Vec2 maxs = Vec2(
		((float)spriteCoords.x + 1.f) * cellSize.x - offset,
		((float)invertedY + 1.f) * cellSize.y - offset
	);
	return AABB2(mins, maxs);
}