#include "SessionManager.h"
#include "net/ISession.h"

void SessionManager::Add(const std::shared_ptr<ISession>& session)
{
	PlayerSession ps{ session };
	ps.connectedAt = std::chrono::steady_clock::now();
	sessions_.emplace(session->Id(), std::move(ps));
}

void SessionManager::Remove(uint64_t sessionId)
{
	auto it = sessions_.find(sessionId);
	if (it == sessions_.end())
	{
		return;
	}

	const uint64_t accountId = it->second.accountId;
	auto acc = byAccount_.find(accountId);
	if (accountId != 0 && acc != byAccount_.end() && acc->second == sessionId) // 아직 나를 가리킬 때만
	{
		byAccount_.erase(acc);
	}
	sessions_.erase(it);
}

PlayerSession* SessionManager::Find(uint64_t sessionId)
{
	auto it = sessions_.find(sessionId);
	if (it == sessions_.end())
	{
		return nullptr;
	}
	return &it->second;
}

uint64_t SessionManager::FindSessionByAccount(uint64_t accountId)
{
	auto it = byAccount_.find(accountId);
	if (it == byAccount_.end())
	{
		return 0;
	}
	return it->second;
}

void SessionManager::BindAccount(uint64_t sessionId, uint64_t accountId)
{
	byAccount_[accountId] = sessionId; // 덮어쓰기가 목적이라 operator[]
}
