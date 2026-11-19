#include "GameServer.h"
#include "net/ISession.h"
#include <spdlog/spdlog.h>

void GameServer::OnConnected(std::shared_ptr<ISession> session)
{
	spdlog::info("[Logic] session {} connected", session->Id());
}

void GameServer::OnPacket(std::shared_ptr<ISession> session, PacketId id, std::vector<uint8_t> body)
{
	switch (id)
	{
	case PacketId::C2S_Echo:
		session->Send(PacketId::S2C_Echo, body.data(), static_cast<uint16_t>(body.size()));
		break;

	default:
		spdlog::warn("[Logic] session {} unknown packet id={}", session->Id(), static_cast<uint16_t>(id));
		break;
	}
}

void GameServer::OnDisconnected(std::shared_ptr<ISession> session)
{
	spdlog::info("[Logic] session {} disconnected", session->Id());
}
