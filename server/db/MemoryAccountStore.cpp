#include "MemoryAccountStore.h"

void MemoryAccountStore::Register(std::string loginId, std::string password, std::string nickname, RegisterCallback cb)
{
	if (accounts_.contains(loginId))
	{
		cb(AuthResult::IdTaken);
		return;
	}
	if (nicknames_.contains(nickname))
	{
		cb(AuthResult::NicknameTaken);
		return;
	}

	const uint64_t id = nextId_++;
	nicknames_.insert(nickname);
	accounts_.emplace(std::move(loginId), Account{ id,std::move(password),std::move(nickname) });

	spdlog::info("[AccountStore] registered accountId={}", id);
	cb(AuthResult::Ok);
}

void MemoryAccountStore::Login(std::string loginId, std::string password, LoginCallback cb)
{
	auto it = accounts_.find(loginId);
	if (it == accounts_.end() || it->second.password != password)
	{
		cb(AuthResult::WrongCredentials, {});
		return;
	}

	const Account& acc = it->second;
	spdlog::info("[AccountStore] login accountId={}", acc.id);
	cb(AuthResult::Ok, AccountInfo{ acc.id, acc.nickname });
}
