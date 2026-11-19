#pragma once
#include <memory>
#include <vector>
#include <cstdint>
#include "PacketDefs.h"

class ISession;

class INetworkHandler
{
public:
	virtual ~INetworkHandler() = default;

	virtual void OnConnected(std::shared_ptr<ISession> session) = 0;
	virtual void OnPacket(std::shared_ptr<ISession> session, PacketId id, std::vector<uint8_t>body) = 0;
	virtual void OnDisconnected(std::shared_ptr<ISession> session) = 0;
};