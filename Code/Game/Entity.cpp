#include "Game/Entity.hpp"
#include "Engine/Math/Mat44.hpp"
#include "Engine/Math/EulerAngles.hpp"


Entity::Entity(Game* owner)
	: m_game(owner)
{
}

Mat44 Entity::GetModelToWorldTransform() const
{
	Mat44 rot = m_orientation.GetAsMatrix_IFwd_JLeft_KUp();

	Mat44 trans = Mat44::MakeTranslation3D(m_position);

	Mat44 modelToWorld = trans;
	modelToWorld.Append(rot);
	return modelToWorld;
}