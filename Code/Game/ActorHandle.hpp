#pragma once

struct ActorHandle
{
public:
	ActorHandle();
	ActorHandle(unsigned int uid, unsigned int index);

	bool IsValid() const;
	unsigned int GetIndex() const;
	unsigned int GetUID() const;

	bool operator==(ActorHandle const& other) const;
	bool operator!=(ActorHandle const& other) const;

	static ActorHandle const INVALID;
	static unsigned int const MAX_ACTOR_UID = 0x0000fffeu;
	static unsigned int const MAX_ACTOR_INDEX = 0x0000ffffu;

private:
	unsigned int m_data = 0xffffffffu;
};