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
	sessions_.Add(session);
	spdlog::info("[Logic] session {} connected", session->Id());
}

void GameServer::OnPacket(std::shared_ptr<ISession> session, PacketId id, std::vector<uint8_t> body)
{
	PlayerSession* ps = sessions_.Find(session->Id());
	if (!ps)
	{
		return; // 이미 해제 처리된 연결.
	}

	const bool isAuthPacket = (id == PacketId::C2S_Register || id == PacketId::C2S_Login);
	if (ps->state != LoginState::LoggedIn && !isAuthPacket)
	{
		spdlog::warn("[Logic] session {} packet id={} before a login, disconnect", session->Id(),static_cast<uint16_t>(id));
		session->Disconnect();
		return;
	}

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
	sessions_.Remove(session->Id());
	spdlog::info("[Logic] session {} disconnected", session->Id());
}

void GameServer::OnTick(std::chrono::steady_clock::time_point now)
{
	constexpr auto kLoginTimeout = std::chrono::seconds(10);

	std::vector <std::shared_ptr<ISession>> expired;
	sessions_.ForEach([&](PlayerSession& ps)
		{
			if (ps.state != LoginState::LoggedIn && now - ps.connectedAt >= kLoginTimeout)
			{
				expired.push_back(ps.session);
			}
		});

	for (auto& s : expired)
	{
		spdlog::info("[Logic] session {} login timeout, disconnect", s->Id());
		s->Disconnect();
	}
}

void GameServer::HandleLogin(const std::shared_ptr<ISession>& session, const nlohmann::json& req)
{
	PlayerSession* ps = sessions_.Find(session->Id());
	if (!ps || ps->state != LoginState::Connected)
	{
		// 이미 로그인 중(LoggingIn)이거나 완료(LoggedIn) → 거절
		SendJson(*session, PacketId::S2C_LoginResult, { {"result", AuthResult::InvalidRequest} });
		return;
	}

	std::string loginId, password;

	if (!GetString(req, "id", 32, loginId) || !GetString(req, "password", 64, password))
	{
		SendJson(*session, PacketId::S2C_LoginResult, { {"result", AuthResult::InvalidRequest} });
		return;
	}

	spdlog::info("[Logic] session {} login request id={}", session->Id(), loginId);

	// 입력 검사를 통과하고 저장소로 요청이 실제로 나가는 순간이 LoggingIn의 시작
	ps->state = LoginState::LoggingIn;

	const uint64_t sessionId = session->Id();
	store_.Login(std::move(loginId), std::move(password),
		[this,sessionId](AuthResult result, AccountInfo info)
		{
			PlayerSession* ps = sessions_.Find(sessionId);
			if (!ps)
			{
				return; // 결과 오기 전에 끊김.
			}

			if (result != AuthResult::Ok)
			{
				ps->state = LoginState::Connected;
				SendJson(*ps->session, PacketId::S2C_LoginResult, { {"result", result} });
				return;
			}
		

			ps->state = LoginState::LoggedIn;
			ps->accountId = info.accountId;
			ps->nickname = info.nickname;

			spdlog::info("[Logic] session {} login ok accountId={}", sessionId, info.accountId);
			SendJson(*ps->session, PacketId::S2C_LoginResult,
				{ {"result", result}, {"accountId", info.accountId}, {"nickname", info.nickname} });
		});
}

void GameServer::HandleRegister(const std::shared_ptr<ISession>& session, const nlohmann::json& req)
{
	constexpr size_t kMaxIdLen = 32;
	constexpr size_t kMaxPasswordLen = 64;
	constexpr size_t kMaxNicknameLen = 32;

	// 가입은 로그인 전(Connected)에만. 로그인 중이거나 이미 로그인한 연결은 거절
	PlayerSession* ps = sessions_.Find(session->Id());
	if (!ps || ps->state != LoginState::Connected)
	{
		SendJson(*session, PacketId::S2C_RegisterResult, { {"result", AuthResult::InvalidRequest} });
		return;
	}

	std::string loginId, password, nickname;
	if (!GetString(req, "id", kMaxIdLen, loginId) || !GetString(req, "password", kMaxPasswordLen, password) || !GetString(req, "nickname", kMaxNicknameLen, nickname))
	{
		SendJson(*session, PacketId::S2C_RegisterResult, { {"result",AuthResult::InvalidRequest} });
		return;
	}

	spdlog::info("[Logic] session {} register request id={} nickname={}", session->Id(), loginId, nickname);

	const uint64_t sessionId = session->Id();
	store_.Register(std::move(loginId), std::move(password), std::move(nickname),
		[this, sessionId](AuthResult result)
		{
			PlayerSession* ps = sessions_.Find(sessionId);
			if (!ps)
			{
				return; // 결과 오기 전에 끊김
			}
			SendJson(*ps->session, PacketId::S2C_RegisterResult, { {"result", result} });
		});
}
