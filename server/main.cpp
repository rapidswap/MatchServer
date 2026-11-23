#include <asio.hpp>
#include <thread>
#include <csignal>
#include "net/Listener.h"
#include "logic/GameServer.h"
#include "db/MemoryAccountStore.h"

int main()
{
	spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] [t%t] %v");
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
