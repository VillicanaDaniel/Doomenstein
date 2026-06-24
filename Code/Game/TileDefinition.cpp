#include "Game/TileDefinition.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"

std::vector<TileDefinition*> TileDefinition::s_definitions;

TileDefinition::TileDefinition(XmlElement const& element)
{
	m_name = ParseXmlAttribute(element, "name", "");
	m_isSolid = ParseXmlAttribute(element, "isSolid", false);
	m_isOpenAir = ParseXmlAttribute(element, "isOpenAir", false);
	m_isWalkable = ParseXmlAttribute(element, "isWalkable", true);
	m_mapImagePixelColor = ParseXmlAttribute(element, "mapImagePixelColor", Rgba8(255,255,255,255));
	m_floorSpriteCoords = ParseXmlAttribute(element, "floorSpriteCoords", IntVec2::ZERO);
	m_ceilingSpriteCoords = ParseXmlAttribute(element, "ceilingSpriteCoords", IntVec2::ZERO);
	m_wallSpriteCoords = ParseXmlAttribute(element, "wallSpriteCoords", IntVec2::ZERO);
}

void TileDefinition::InitializeTileDefs(char const* xmlFilePath)
{
	XmlDocument doc;
	doc.LoadFile(xmlFilePath);

	XmlElement* root = doc.RootElement();
	GUARANTEE_OR_DIE(root != nullptr, "TileDefinition::InitializeTileDefs - failed to load xml root");

	for (XmlElement const* elem = root->FirstChildElement("TileDefinition"); elem != nullptr; elem = elem->NextSiblingElement("TileDefinition"))
	{
		TileDefinition* def = new TileDefinition(*elem);
		s_definitions.push_back(def);
	}
}

TileDefinition const* TileDefinition::GetByName(std::string const& name)
{
	for (TileDefinition const* def : s_definitions)
	{
		if (def->m_name == name)
		{
			return def;
		}
	}
	return nullptr;
}

TileDefinition const* TileDefinition::GetByColor(Rgba8 const& color)
{
	for (TileDefinition const* def : s_definitions)
	{
		Rgba8 const& c = def->m_mapImagePixelColor;
		if (c.r == color.r && c.g == color.g && c.b == color.b && c.a == color.a)
		{
			return def;
		}
	}
	return nullptr;
}