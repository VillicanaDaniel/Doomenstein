#pragma once

#include "Engine/Core/Rgba8.hpp"
#include "Engine/Core/XmlUtils.hpp"
#include "Engine/Math/IntVec2.hpp"
#include <string>
#include <vector>

class TileDefinition
{
public:
	TileDefinition() = default;
	explicit TileDefinition(XmlElement const& element);

	static void InitializeTileDefs(char const* xmlFilePath);
	static TileDefinition const* GetByName(std::string const& name);
	static TileDefinition const* GetByColor(Rgba8 const& color);

public:
	std::string m_name;
	bool m_isSolid = false;
	bool m_isOpenAir = false;
	bool m_isWalkable = true;
	Rgba8 m_mapImagePixelColor = Rgba8(255,255,255,255);
	IntVec2 m_floorSpriteCoords = IntVec2::ZERO;
	IntVec2 m_ceilingSpriteCoords = IntVec2::ZERO;
	IntVec2 m_wallSpriteCoords = IntVec2::ZERO;

	static std::vector<TileDefinition*> s_definitions;
};