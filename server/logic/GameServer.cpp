#include "GameServer.h"
#include "net/ISession.h"
#include "logic/IAccountStore.h"

namespace
{
	void SendJson(ISession& session, PacketId id, const nlohmann::json& j)
	{
		const std::string s = j.dump();
		if (s.size() > kMaxBodySize)
		{
			spdlog::error("[Logic] session {} SendJson too large {}", session.Id(), s.size());
			return;
		}
		session.Send(id, reinterpret_cast<const uint8_t*>(s.data()), static_cast<uint16_t>(s.size()));
	}

	bool GetString(const nlohmann::json& req, const char* key, size_t maxLen, std::string& out)
	{
		auto it = req.find(key);
		if (it == req.end() || !it->is_string())
		{
			return false;
		}

		const auto& s = it->get_ref<const std::string&>();
		if (s.empty() || s.size() > maxLen)
		{
			return false;
		}
		out = s;

		return true;
	}
}

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

	case PacketId::C2S_Register:
	case PacketId::C2S_Login:
	{
		nlohmann::json req = nlohmann::json::parse(body.begin(), body.end(), nullptr, false);
		if (req.is_discarded() || !req.is_object())
		{
			spdlog::warn("[Logic] session {} bad json (id={})", session->Id(), static_cast<uint16_t>(id));
			session->Disconnect();
			return;
		}

		if (id == PacketId::C2S_Register)
		{
			HandleRegister(session, req);
		}
		else
		{
			HandleLogin(session, req);
		}
		break;
	}

	default:
		spdlog::warn("[Logic] session {} unknown packet id={}", session->Id(), static_cast<uint16_t>(id));
		break;
	}
}

void GameServer::OnDisconnected(std::shared_ptr<ISession> session)
{
	spdlog::info("[Logic] session {} disconnected", session->Id());
}

void GameServer::HandleLogin(const std::shared_ptr<ISession>& session, const nlohmann::json& req)
{
	std::string loginId, password;

	if (!GetString(req, "id", 32, loginId) || !GetString(req, "password", 64, password))
	{
		SendJson(*session, PacketId::S2C_LoginResult, { {"result", AuthResult::InvalidRequest} });
		return;
	}

	spdlog::info("[Logic] session {} login request id={}", session->Id(), loginId);

	std::weak_ptr<ISession> weak = session;
	store_.Login(std::move(loginId), std::move(password),
		[weak](AuthResult result, AccountInfo info)
		{
			auto s = weak.lock();
			if (!s)
			{
				return;
			}

			if (result != AuthResult::Ok)
			{
				SendJson(*s, PacketId::S2C_LoginResult, { {"result", result} });
				return;
			}

			spdlog::info("[Logic] session {} login ok accountId={}", s->Id(), info.accountId);
			SendJson(*s, PacketId::S2C_LoginResult,
				{ {"result", result}, {"accountId", info.accountId}, {"nickname", info.nickname} });
		});
}

void GameServer::HandleRegister(const std::shared_ptr<ISession>& session, const nlohmann::json& req)
{
	constexpr size_t kMaxIdLen = 32;
	constexpr size_t kMaxPasswordLen = 64;
	constexpr size_t kMaxNicknameLen = 32;

	std::string loginId, password, nickname;
	if (!GetString(req, "id", kMaxIdLen, loginId) || !GetString(req, "password", kMaxPasswordLen, password) || !GetString(req, "nickname", kMaxNicknameLen, nickname))
	{
		SendJson(*session, PacketId::S2C_RegisterResult, { {"result",AuthResult::InvalidRequest} });
		return;
	}

	spdlog::info("[Logic] session {} register request id={} nickname={}", session->Id(), loginId, nickname);

	std::weak_ptr<ISession> weak = session;
	store_.Register(std::move(loginId), std::move(password), std::move(nickname),
		[weak](AuthResult result)
		{
			auto s = weak.lock();
			if (!s)
			{
				return;
			}
			SendJson(*s, PacketId::S2C_RegisterResult, { {"result", result} });
		});

}
