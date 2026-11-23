#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include "logic/IAccountStore.h"


class MemoryAccountStore :public IAccountStore
{
public:
	void Register(std::string loginId, std::string password, std::string nickname, RegisterCallback cb) override;
	void Login(std::string loginId, std::string password, LoginCallback cb) override;

private:

	struct Account
	{
		uint64_t id = 0;
		std::string password;
		std::string nickname;
	};

	std::unordered_map<std::string, Account>accounts_;
	std::unordered_set<std::string> nicknames_;
	uint64_t nextId_ = 1;
};