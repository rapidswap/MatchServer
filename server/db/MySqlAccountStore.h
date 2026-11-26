#pragma once
#include <asio.hpp>
#include <thread>
#include "logic/IAccountStore.h"
#include "db/DbConfig.h"

struct MYSQL; // mysql.h 전체를 헤더에 끌고 오지 않으려고 전방 선언만.

// MySQL 계정 저장소. 쿼리와 해시는 전용 DB 스레드 하나에서만 돈다.
// 결과 콜백은 logicIo로 post해서 로직 스레드에서 호출한다. (IAccountStore).

class MySqlAccountStore :public IAccountStore
{
public:
	MySqlAccountStore(DbConfig config, asio::io_context& logicIo);
	~MySqlAccountStore() override;

	MySqlAccountStore(const MySqlAccountStore&) = delete;
	MySqlAccountStore& operator=(const MySqlAccountStore&) = delete;

	// DB에 접속하고 DB 스레드를 띄운다. false면 서버를 띄우면 안된다.
	bool Start();
	// 남은 작업을 다 처리하고 스레드 종료 -> 연겨 닫기. 여러 번 불려도 된다.
	void Stop();

	void Register(std::string loginId, std::string password, std::string nickname, RegisterCallback registerCallback) override;
	void Login(std::string loginId, std::string password, LoginCallback loginCallback) override;

private:
	bool Connect();

private:
	DbConfig config_;
	asio::io_context& logicIo_;
	asio::io_context dbIo_;
	asio::executor_work_guard<asio::io_context::executor_type> dbGuard_;

	MYSQL* conn_ = nullptr; // DB 스레드 전용 (Start 전/ Stop 후에만 메인 스레드가 안정)

	std::jthread dbThread_; // 마지막 선언해야 파괴될 때 가장 먼저 join  한다.


};