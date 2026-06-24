#include "Game/ActorHandle.hpp"

ActorHandle const ActorHandle::INVALID(0x0000ffffu, 0x0000ffffu);

ActorHandle::ActorHandle()
	: m_data(INVALID.m_data)
{
}

ActorHandle::ActorHandle(unsigned int uid, unsigned int index)
{
	unsigned int clampedUID = uid & 0x0000ffffu;
	unsigned int clampedIndex = index & 0x0000ffffu;
	m_data = (clampedUID << 16) | clampedIndex;
}

bool ActorHandle::IsValid() const
{
	return m_data != INVALID.m_data;
}

unsigned int ActorHandle::GetIndex() const
{
	return m_data & 0x0000ffffu;
}

unsigned int ActorHandle::GetUID() const
{
	return (m_data >> 16) & 0x0000ffffu;
}

bool ActorHandle::operator==(ActorHandle const& other) const
{
	return m_data == other.m_data;
}

bool ActorHandle::operator!=(ActorHandle const& other) const
{
	return m_data != other.m_data;
}