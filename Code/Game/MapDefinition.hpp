#pragma once

#include "Engine/Core/XmlUtils.hpp"
#include "Engine/Math/AABB2.hpp"
#include "Engine/Math/EulerAngles.hpp"
#include "Engine/Math/IntVec2.hpp"
#include "Engine/Math/Vec3.hpp"
#include <string>
#include <vector>

struct SpawnInfo
{
	std::string m_actor;
	std::string m_faction;
	Vec3 m_position = Vec3::ZERO;
	EulerAngles m_orientation = EulerAngles();
};

class MapDefinition
{
public:
	MapDefinition() = default;
	explicit MapDefinition(XmlElement const& element);

	static void InitializeMapDefs(char const* xmlFilePath);
	static MapDefinition const* GetByName(std::string const& name);

	AABB2 GetSpriteUVs(IntVec2 const& spriteCoords) const;

public:
	std::string m_name;
	std::string m_image;
	std::string m_shader;
	std::string m_spriteSheetTexture;
	IntVec2 m_spriteSheetCellCount = IntVec2(8, 8);
	std::vector<SpawnInfo> m_spawnInfos;

	std::vector<std::string> m_layerImagePaths;
	static std::vector<MapDefinition*> s_definitions;
};