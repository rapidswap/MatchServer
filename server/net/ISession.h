#pragma once
#include <cstdint>
#include "PacketDefs.h"

class ISession
{
public:
	virtual ~ISession() = default;

	virtual uint64_t Id() const = 0;
	virtual void Send(PacketId id, const uint8_t* body, uint16_t bodyLen) = 0;
	virtual void Disconnect() = 0;
};