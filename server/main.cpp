#include <asio.hpp>
#include <thread>
#include <csignal>
#include "net/Listener.h"
#include "logic/GameServer.h"
#include "db/DbConfig.h"
#include "db/PasswordHasher.h"
#include "db/MySqlAccountStore.h"
#include <mysql/mysql.h>

int main()
{
	spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] [t%t] %v");

	// 스레드가 생기기 전에 한 번, mysql_init이 자도응로 해주긴 한지만 그건 스레드가 안전하지 않다.
	if (mysql_library_init(0, nullptr, nullptr) != 0)
	{
		spdlog::critical("[DB] mysql_library_init failed");
		return 1;
	}

	if (!InitPasswordHasher())
	{
		return 1;
	}

	// DB 없이는 가입/로그인을 못 하므로 설정이나 접속이 안 되면 서버를 띄우지 않는다.
	const std::optional<DbConfig> dbConfig = LoadDbConfig("config/local.json");
	if (!dbConfig)
	{
		return 1;
	}

	try
	{
		asio::io_context io;
		asio::io_context logicIo;
		auto logicGuard = asio::make_work_guard(logicIo);

		// logicIo를 참조하므로 logicIo 보다 뒤에서 선언->logicIo보다 먼저 파괴 된다.
		MySqlAccountStore accountStore(*dbConfig, logicIo);
		if (!accountStore.Start())
		{
			return 1;
		}

		GameServer gameServer(accountStore);


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
	mysql_library_end();
	return 0;
}
