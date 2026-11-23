#include "SessionManager.h"
#include "net/ISession.h"

void SessionManager::Add(const std::shared_ptr<ISession>& session)
{
	sessions_.emplace(session->Id(), PlayerSession{ session });
}

void SessionManager::Remove(uint64_t sessionId)
{
	sessions_.erase(sessionId);
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
