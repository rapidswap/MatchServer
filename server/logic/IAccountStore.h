#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include "PacketDefs.h"

struct AccountInfo
{
	uint64_t accountId = 0;
	std::string nickname;
};

class IAccountStore
{
public:
	using RegisterCallback = std::function<void(AuthResult)>;
	using LoginCallback = std::function<void(AuthResult, AccountInfo)>;

	virtual ~IAccountStore() = default;

	// 약속: cb는 반드시 로직 스레드에서 호출된다.
	virtual void Register(std::string loginId, std::string password, std::string nickname, RegisterCallback cb) = 0;
	virtual void Login(std::string loginId, std::string password, LoginCallback cb) = 0;
};
