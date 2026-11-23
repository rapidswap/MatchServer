#pragma once
#include <chrono>
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
	std::chrono::steady_clock::time_point connectedAt; // 로그인 타임아웃 기준 (Add에서 기록)
};

class SessionManager
{
public:
	void Add(const std::shared_ptr<ISession>& session);
	void Remove(uint64_t sessionId);
	PlayerSession* Find(uint64_t sessionId); // 없으면 nullptr

	// 모든 세션을 돌며 fn(PlayerSession&) 호출. 원소를 보거나 고치는 것까지만 (추가/삭제 금지)
	// 템플릿이라 정의가 헤더에 있어야 한다
	template <typename F>
	void ForEach(F&& fn)
	{
		for (auto& [id, ps] : sessions_)
		{
			fn(ps);
		}
	}

private:
	std::unordered_map<uint64_t, PlayerSession> sessions_;
};
