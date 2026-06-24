#pragma once
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/EulerAngles.hpp"

class Game;

class Entity
{
public:
	explicit Entity(Game* owner);
	virtual ~Entity() = default;

	virtual void Update() = 0;
	virtual void Render() const = 0;

	virtual Mat44 GetModelToWorldTransform() const;

public:
	Game* m_game = nullptr;
	Vec3        m_position;
	Vec3        m_velocity;
	EulerAngles m_orientation;
	EulerAngles m_angularVelocity;
	Rgba8 m_color = Rgba8(255,255,255);
};