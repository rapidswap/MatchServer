#pragma once
#include "net/INetworkHandler.h"

class GameServer :public INetworkHandler
{
public:
	void OnConnected(std::shared_ptr<ISession> session) override;
	void OnPacket(std::shared_ptr<ISession> session, PacketId id, std::vector<uint8_t> body) override;
	void OnDisconnected(std::shared_ptr<ISession> session) override;
};