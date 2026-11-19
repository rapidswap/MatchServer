#include "Session.h"

#include <string_view>
#include <spdlog/spdlog.h>

Session::Session(asio::ip::tcp::socket socket, uint64_t id)
	:socket_(std::move(socket)),id_(id)
{
}

Session::~Session()
{
	spdlog::info("[Session {}] Destroyed", id_);
}

void Session::Start()
{
	// 1) 주소를 먼저 얻고(error_code 버전), 2) 성공했을 때만 로그를 찍는다
	std::error_code ec;
	auto ep = socket_.remote_endpoint(ec);
	if (!ec)
	{
		spdlog::info("[Session {}] Connected from {}:{}", id_, ep.address().to_string(), ep.port());
	}

	// 주소를 못 얻었더라도 읽기는 시작한다
	DoRead();
}

void Session::DoRead()
{
	auto self = shared_from_this();
	socket_.async_read_some(asio::buffer(readBuffer_),
		[this, self](std::error_code ec, std::size_t bytes)
		{
			if (ec)
			{
				// 연결이 끝났거나 에러: 더 이상 작업을 걸지 않는다 → self가 사라지며 Session 소멸
				Close(ec);
				return;
			}

			// 받은 만큼만 문자열로 보여준다
			std::string_view text(reinterpret_cast<const char*>(readBuffer_.data()), bytes);
			spdlog::info("[Session {}] recv {} bytes: {}", id_, bytes, text);

			DoWrite(bytes);   // 다음 데이터 대기
		});
}

void Session::DoWrite(std::size_t length)
{
	auto self = shared_from_this();
	asio::async_write(socket_, asio::buffer(readBuffer_.data(), length),
		[this, self](std::error_code ec, std::size_t bytes)
		{
			if (ec)
			{
				// 연결이 끝났거나 에러: 더 이상 작업을 걸지 않는다 → self가 사라지며 Session 소멸
				Close(ec);
				return;
			}
			std::string_view text(reinterpret_cast<const char*>(readBuffer_.data()), bytes);
			spdlog::info("[Session {}] sent {} bytes: {}", id_, bytes, text);
			DoRead();
		}
	);
}

void Session::Close(const std::error_code& ec)
{
	if (closed_)
	{
		return;
	}

	closed_ = true;

	if (ec == asio::error::eof)
	{
		spdlog::info("[Session {}] Disconnected",id_);
	}
	else if (ec == asio::error::connection_reset)
	{
		spdlog::info("[Session {}] Disconnected (reset)",id_);
	}
	else if (ec == asio::error::operation_aborted)
	{
	}
	else
	{
		spdlog::warn("[Session {}] error: {}",id_,ec.message());
	}

	std::error_code ignore;
	socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ignore);
	socket_.close(ignore);
}
