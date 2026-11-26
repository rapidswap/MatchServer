#include "MySqlAccountStore.h"
#include <mysql/mysql.h>

MySqlAccountStore::MySqlAccountStore(DbConfig config, asio::io_context& logicIo)
	:config_(std::move(config)),logicIo_(logicIo),dbGuard_(asio::make_work_guard(dbIo_))
{
}

MySqlAccountStore::~MySqlAccountStore()
{
	Stop();
}

bool MySqlAccountStore::Start()
{
	// 스레드를 띄우기 전에 메인 스레드에서 접속한다. 실패를 바로 알 수 있고,
	// 스레드가 생성되는 시점에서 conn_이 DB 스레드로 넘어간다 (그 뒤로 메인은 안 건드림).
	if (!Connect())
	{
		return false;
	}

	dbThread_ = std::jthread([this]
		{
			mysql_thread_init(); // 이 스레드에서 MySQL API를 쓰기 전에 한 번.
			spdlog::info("[DB] thread started");
			
			try
			{
				dbIo_.run();
			}
			catch (const std::exception& e)
			{
				spdlog::critical("[DB] thread error: {}", e.what());
			}
			mysql_thread_end(); // 스레드별 자원 정리.
		});
	return true;
}

void MySqlAccountStore::Stop()
{
	dbGuard_.reset(); // 큐에 남은 작업을 다 처리하면 run()이 리턴한다.
	if (dbThread_.joinable())
	{
		dbThread_.join();
	}
	if (conn_)
	{
		mysql_close(conn_);
		conn_ = nullptr;
	}
}

// 4-1 임시: 스레드 왕복만 확인. 4-2/ 4-3 에서 실제 쿼리로 바꾼다.
void MySqlAccountStore::Register(std::string loginId, std::string password, std::string nickname, RegisterCallback registerCallback)
{
	asio::post(dbIo_, [this, registerCallback = std::move(registerCallback)]()mutable
		{
			spdlog::info("[DB] register (stub)");
			asio::post(logicIo_, [registerCallback = std::move(registerCallback)]
				{
					registerCallback(AuthResult::ServerError);
				});
		});
}

void MySqlAccountStore::Login(std::string loginId, std::string password, LoginCallback loginCallback)
{
	asio::post(dbIo_, [this, loginCallback = std::move(loginCallback)]()mutable
		{
			spdlog::info("[DB] login (stub)");
			asio::post(logicIo_, [loginCallback = std::move(loginCallback)]
				{
					loginCallback(AuthResult::ServerError, {});
				});
		});
}

bool MySqlAccountStore::Connect()
{
	conn_ = mysql_init(nullptr);
	if (!conn_)
	{
		spdlog::critical("[DB] mysql_init failed (out of memory)");
		return false;
	}

	mysql_options(conn_, MYSQL_SET_CHARSET_NAME, "utf8mb4");

	if (!mysql_real_connect(conn_, config_.host.c_str(), config_.user.c_str(), config_.password.c_str(), config_.database.c_str(), config_.port, nullptr, 0))
	{
		spdlog::critical("[DB] connect failed: {}", mysql_error(conn_));
		mysql_close(conn_);
		conn_ = nullptr;
		return false;
	}

	spdlog::info("[DB] connected (Server {}, charset {})", mysql_get_server_info(conn_), mysql_character_set_name(conn_));
	return true;
}
