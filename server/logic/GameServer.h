#pragma once
#include "net/INetworkHandler.h"
#include <nlohmann/json.hpp>
#include "SessionManager.h"

class IAccountStore;

class GameServer :public INetworkHandler
{
public:
	explicit GameServer(IAccountStore& store) :store_(store) {}

	void OnConnected(std::shared_ptr<ISession> session) override;
	void OnPacket(std::shared_ptr<ISession> session, PacketId id, std::vector<uint8_t> body) override;
	void OnDisconnected(std::shared_ptr<ISession> session) override;

	void OnTick(std::chrono::steady_clock::time_point now);

private:
	void HandleLogin(const std::shared_ptr<ISession>& session, const nlohmann::json& req);
	void HandleRegister(const std::shared_ptr<ISession>& session, const nlohmann::json& req);

private:
	IAccountStore& store_;

	SessionManager sessions_;
};