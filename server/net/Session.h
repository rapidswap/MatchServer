#pragma once
#include <asio.hpp>
#include <array>
#include <cstdint>
#include <memory>

class Session :public std::enable_shared_from_this<Session>
{
public:
	Session(asio::ip::tcp::socket socket, uint64_t id);
	~Session();
	
	void Start();

private:
	void DoRead();

	void DoWrite(std::size_t length);

	void Close(const std::error_code& ec);

private:
	asio::ip::tcp::socket socket_;
	uint64_t id_;
	std::array<uint8_t, 4096> readBuffer_{};
	bool closed_ = false;
};