#include <spdlog/spdlog.h>
#include <asio.hpp>

#include <array>
#include <chrono>
#include <string>
#include <string_view>
#include <thread>

int main(int argc, char* argv[])
{
	// 실행할 때 번호를 받는다: dummy_client.exe 1  → clientNo = 1
	// 번호가 없으면 0. 창마다 번호를 다르게 주면 누가 보낸 메시지인지 구분된다.
	int clientNo = argc > 1 ? std::stoi(argv[1]) : 0;

	try
	{
		asio::io_context io;
		asio::ip::tcp::socket socket(io);
		asio::ip::tcp::resolver resolver(io);

		// 127.0.0.1:7777 -> endpoint로 바꾸고 연결.
		asio::connect(socket, resolver.resolve("127.0.0.1", "7777"));
		spdlog::info("[client {}] connected to server (my port {})", clientNo, socket.local_endpoint().port());

		for (int i = 0; i < 5; ++i)
		{
			// 1) 보낼 메시지 만들기: "client1 msg0", "client1 msg1", ...
			std::string msg = "client" + std::to_string(clientNo) + " msg" + std::to_string(i);

			// 2) 보내기 (동기). 다 보낼 때까지 여기서 멈춘다
			asio::write(socket, asio::buffer(msg));
			spdlog::info("[client {}] sent: {}", clientNo, msg);

			// 3) 받기 (동기). 서버가 돌려줄 때까지 여기서 멈춘다
			//    read_some은 "도착한 만큼" 돌려주고, 받은 바이트 수를 반환한다
			std::array<char, 1024> buf;
			std::size_t n = socket.read_some(asio::buffer(buf));

			// 4) 받은 n바이트만 문자열로 보기
			spdlog::info("[client {}] recv: {}", clientNo, std::string_view(buf.data(), n));

			// 5) 1초 쉬기: 다른 창에서 클라를 더 띄울 시간을 번다
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}

		spdlog::info("[client {}] closing", clientNo);
	}
	catch (const std::exception& e)
	{
		spdlog::error("[client {}] client error: {}", clientNo, e.what());
		return 1;
	}
}
