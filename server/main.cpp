#include <asio.hpp>
#include <thread>
#include <csignal>
#include "net/Listener.h"
#include "logic/GameServer.h"
#include "db/MemoryAccountStore.h"
#include "db/DbConfig.h"
#include "db/PasswordHasher.h"
#include <mysql/mysql.h>
#include <chrono>

// 3-B-2 임시: 설정대로 DB에 붙었다가 바로 끊어 본다. 3-B-4에서 MySqlAccountStore(DB 스레드)로 옮긴다.
static bool CheckDbConnection(const DbConfig& cfg)
{
	MYSQL* conn = mysql_init(nullptr);
	if (!conn)
	{
		spdlog::critical("[DB] mysql_init failed (out of memory)");
		return false;
	}

	// 지정 안 하면 클라 기본 문자셋으로 주고받아 한글 닉네임이 깨질 수 있다.
	mysql_options(conn, MYSQL_SET_CHARSET_NAME, "utf8mb4");

	if (!mysql_real_connect(conn, cfg.host.c_str(), cfg.user.c_str(), cfg.password.c_str(),
		cfg.database.c_str(), cfg.port, nullptr, 0))
	{
		spdlog::critical("[DB] connect failed: {}", mysql_error(conn));
		mysql_close(conn);
		return false;
	}

	spdlog::info("[DB] connected (server {}, charset {})", mysql_get_server_info(conn), mysql_character_set_name(conn));
	mysql_close(conn);
	return true;
}

int main()
{
	spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] [t%t] %v");

	if (!InitPasswordHasher())
	{
		return 1;
	}

	// 3-B-3 임시 테스트: 3-B-4에서 지운다.
	{
		const auto start = std::chrono::steady_clock::now();
		const std::optional<std::string> h1 = HashPassword("test1234");
		const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
		const std::optional<std::string> h2 = HashPassword("test1234");

		if (!h1 || !h2)
		{
			return 1;
		}

		spdlog::info("[HasherTest] len={} time={}ms", h1->size(), elapsed);
		spdlog::info("[HasherTest] same pw, different hash: {}", *h1 != *h2);        // true
		spdlog::info("[HasherTest] verify correct: {}", VerifyPassword(*h1, "test1234")); // true
		spdlog::info("[HasherTest] verify wrong: {}", VerifyPassword(*h1, "test1235"));   // false
		spdlog::info("[HasherTest] verify h2: {}", VerifyPassword(*h2, "test1234"));      // true
	}

	// DB 없이는 가입/로그인을 못 하므로 설정이나 접속이 안 되면 서버를 띄우지 않는다.
	const std::optional<DbConfig> dbConfig = LoadDbConfig("config/local.json");
	if (!dbConfig || !CheckDbConnection(*dbConfig))
	{
		return 1;
	}

	try
	{
		asio::io_context io;
		MemoryAccountStore accountStore;
		GameServer gameServer(accountStore);
		asio::io_context logicIo;
		auto logicGuard = asio::make_work_guard(logicIo);

		// 1초마다 로직 틱. 타이머를 logicIo에 걸었으므로 콜백(OnTick)은 로직 스레드에서 실행된다.
		// 스레드들보다 먼저 선언해야 스레드 join 뒤에 파괴된다 (선언 역순 파괴).
		asio::steady_timer tickTimer(logicIo);
		std::function<void()> scheduleTick = [&]
			{
				tickTimer.expires_after(std::chrono::seconds(1));
				tickTimer.async_wait([&](const std::error_code& ec)
					{
						if (ec)
						{
							return; // 취소됨 (종료 중)
						}
						gameServer.OnTick(std::chrono::steady_clock::now());
						scheduleTick();
					});
			};
		scheduleTick();

		Listener listener(io, logicIo, gameServer, 7777);   // 포트가 이미 사용 중이면 여기서 예외

		listener.Start();

		asio::signal_set signals(io, SIGINT, SIGTERM);
		signals.async_wait([&](std::error_code, int)
			{
				spdlog::info("shutting down...");
				listener.Stop();
				logicGuard.reset();
				logicIo.stop();
				io.stop();
			});

		std::jthread logicThread([&logicIo]
			{
				try
				{
					logicIo.run();
				}
				catch (const std::exception& e)
				{
					spdlog::critical("logic thread error: {}", e.what());
				}
			});

		const unsigned threadCount = 4;
		spdlog::info("starting {} IO Threads", threadCount); 

		std::vector<std::jthread> threads;

		for (size_t i = 0; i < threadCount;++i)
		{
			threads.emplace_back([&io] {
				try
				{
					io.run();
				}
				catch (const std::exception& e)
				{
					spdlog::critical("io thread error: {}", e.what());
				}
				});
		}
	}
	catch (const std::exception& e)
	{
		spdlog::critical("server error: {}", e.what());
		return 1;
	}
}
