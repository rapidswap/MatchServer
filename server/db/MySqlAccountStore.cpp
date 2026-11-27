#include "MySqlAccountStore.h"
#include "mysql/mysql.h"
#include "db/PasswordHasher.h"

namespace
{
	constexpr unsigned int kErDupEntry = 1062; // ER_DUP_ENTRY: UNIQUE 위반.

	// mysql_stmt_close를 자동으로, 중간 return이 많아서 RAII로 닫는다.
	struct StmtDeleter
	{
		void operator()(MYSQL_STMT* stmt) const { mysql_stmt_close(stmt); }
	};
	using StmtPtr = std::unique_ptr<MYSQL_STMT, StmtDeleter>;

	// 입력용 문자열 바인딩. s는 execute가 끝날 때까지 살아 있어야 한다.(포인터만 넘어감).
	void BindString(MYSQL_BIND& bind, const std::string& str)
	{
		bind.buffer_type = MYSQL_TYPE_STRING;
		bind.buffer = const_cast<char*>(str.data()); // C API라 const가 없지만 입력용은 수정하지 않는다.
		bind.buffer_length = static_cast<unsigned long>(str.size());

	}

}


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


void MySqlAccountStore::Register(std::string loginId, std::string password, std::string nickname, RegisterCallback registerCallback)
{
	// 문자열은 move로 람다에 옮긴다. 이 함수가 리턴한 뒤에도 DB 스레드에서 써야 하므로 참조로 잡으면 안된다.

	asio::post(dbIo_, [this,loginId=std::move(loginId),password=std::move(password),nickname=std::move(nickname), registerCallback = std::move(registerCallback)]()mutable
		{
			const AuthResult result = DoRegister(loginId, password, nickname);
			asio::post(logicIo_, [registerCallback = std::move(registerCallback),result]
				{
					registerCallback(result);
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

AuthResult MySqlAccountStore::DoRegister(const std::string& loginId, const std::string& password, const std::string& nickname)
{
	// 1) 해시(느림, 그래서 DB스레드에서).
	const std::optional<std::string> hash = HashPassword(password);
	if (!hash)
	{
		return AuthResult::ServerError;
	}

	// 2) prepare
	StmtPtr stmt(mysql_stmt_init(conn_));
	if (!stmt)
	{
		spdlog::error("[DB] mysql_stmt_init failed: {}", mysql_error(conn_));
		return AuthResult::ServerError;
	}

	constexpr std::string_view sql = "INSERT INTO accounts (login_id, password_hash, nickname) VALUES (?, ?, ?)";
	if (mysql_stmt_prepare(stmt.get(), sql.data(), static_cast<unsigned long>(sql.size())) != 0)
	{
		spdlog::error("[DB] register prepare failed: {}", mysql_stmt_error(stmt.get()));
		return AuthResult::ServerError;
	}

	// 3) bind: ? 순서대로.
	MYSQL_BIND params[3] = {}; // 0 초기화는 필수.
	BindString(params[0], loginId);
	BindString(params[1], *hash);
	BindString(params[2], nickname);

	if (mysql_stmt_bind_param(stmt.get(), params)) // 이 함ㅅ수는 실패하면 true.
	{
		spdlog::error("[DB] register bind error: {}", mysql_stmt_error(stmt.get()));
		return AuthResult::ServerError;
	}

	// 4) execute.
	if (mysql_stmt_execute(stmt.get())!=0)
	{
		if (mysql_stmt_errno(stmt.get()) == kErDupEntry)
		{
			// "Duplicate entry '...' for key 'accounts.nickname'": 끝의 키 이름으로 구분.
			const std::string_view msg = mysql_stmt_error(stmt.get());
			return msg.ends_with("'accounts.nickname'") ? AuthResult::NicknameTaken : AuthResult::IdTaken;
		}
		spdlog::error("[DB] register execute failed ({}): {}", mysql_stmt_errno(stmt.get()), mysql_stmt_error(stmt.get()));
		return AuthResult::ServerError;
	}

	spdlog::info("[DB] registered accountId={}", mysql_stmt_insert_id(stmt.get()));
	return AuthResult::Ok;
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
