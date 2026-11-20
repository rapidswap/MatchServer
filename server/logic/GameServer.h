#pragma once
#include "net/INetworkHandler.h"
#include "net/ISession.h"
#include <nlohmann/json.hpp>

class GameServer :public INetworkHandler
{
public:
	void OnConnected(std::shared_ptr<ISession> session) override;
	void OnPacket(std::shared_ptr<ISession> session, PacketId id, std::vector<uint8_t> body) override;
	void OnDisconnected(std::shared_ptr<ISession> session) override;

	void HandleLogin(const std::shared_ptr<ISession>& session, const nlohmann::json& req);
	void HandleRegister(const std::shared_ptr<ISession>& session, const nlohmann::json& req);
};