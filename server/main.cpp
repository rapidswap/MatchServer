#include <spdlog/spdlog.h>
#include <asio.hpp>
#include <thread>
#include <vector>
#include <algorithm>
#include <csignal>
#include "net/Listener.h"
#include "logic/GameServer.h"

int main()
{
	spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] [t%t] %v");
	try
	{
		asio::io_context io;
		GameServer gameServer;
		asio::io_context logicIo;
		auto logicGuard = asio::make_work_guard(logicIo);

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
