#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

class ISession;

enum class LoginState :uint8_t
{
	Connected, // 접속만 함. Register/Login만 허용
	LoggingIn, // 로그인 요청을 저장소에 보냄, 결과 대기 중
	LoggedIn,  // 로그인 완료, 게임 패킷 허용
};

struct PlayerSession
{
	std::shared_ptr<ISession> session;
	LoginState state = LoginState::Connected;
	uint64_t accountId = 0;
	std::string nickname;
};

class SessionManager
{
public:
	void Add(const std::shared_ptr<ISession>& session);
	void Remove(uint64_t sessionId);
	PlayerSession* Find(uint64_t sessionId); // 없으면 nullptr

private:
	std::unordered_map<uint64_t, PlayerSession> sessions_;
};