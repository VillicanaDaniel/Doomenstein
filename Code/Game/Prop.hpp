#pragma once
#include "Game/Actor.hpp"
#include "Engine/Core/Vertex.hpp"
#include "Engine/Core/Rgba8.hpp"
#include <vector>

class Texture;

struct PropSubmesh
{
	std::vector<Vertex> m_verts;
	Texture* m_texture = nullptr;
};

class Prop : public Actor
{
public:
	using Actor::Actor;
	~Prop() = default;

	void Update();
	void Render(Camera const& camera) const override;

public:
	std::vector<Vertex> m_vertexes;
	Rgba8 m_color = Rgba8(255,255,255);
	Texture* m_texture = nullptr;
	std::string m_displayName;
	float m_maxHealth = 100.f;

	std::vector<PropSubmesh> m_submeshes; //Optional Addition to support obj files with multiple textures
};