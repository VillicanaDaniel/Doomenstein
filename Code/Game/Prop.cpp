#include "Game/Prop.hpp"
#include "Game/Game.hpp"
#include "Game/Map.hpp"
#include "Game/Player.hpp"
#include "Engine/Core/Engine.hpp"
#include "Engine/Core/Clock.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Renderer/Texture.hpp"
#include "Engine/Math/MathUtils.hpp"

void Prop::Update()
{
	Actor::Update();

	if (m_isDead)
	{
		return;
	}
}

void Prop::Render([[maybe_unused]]Camera const& camera) const
{
	if (!m_submeshes.empty())
	{
		if (!m_visible || m_isDead)
		{
			return;
		}

		Mat44 model = m_orientation.GetAsMatrix_IFwd_JLeft_KUp();
		model.SetTranslation3D(m_position);

		for (PropSubmesh const& sm : m_submeshes)
		{
			if (sm.m_verts.empty())
				continue;

			std::vector<Vertex> temp = sm.m_verts;
			Mat44 modelCorrection;
			modelCorrection.AppendXRotation(90.f);

			Mat44 modelToWorld = GetModelToWorldTransform();
			modelToWorld.Append(modelCorrection);

			g_engine->m_render->SetModelConstants(modelToWorld, m_color);

			g_engine->m_render->BindTexture(sm.m_texture);
			g_engine->m_render->DrawVertexArray((int)temp.size(), temp.data());
		}

		g_engine->m_render->SetModelConstants();
		return;
	}

	// Old Way of Handling
	if (m_vertexes.empty())
		return;

	g_engine->m_render->SetModelConstants(GetModelToWorldTransform(), m_color);

	g_engine->m_render->SetBlendMode(BlendMode::OPAQUE);
	g_engine->m_render->BindTexture(m_texture);

	g_engine->m_render->DrawVertexArray((int)m_vertexes.size(), m_vertexes.data());
}