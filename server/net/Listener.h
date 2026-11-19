#pragma once
#include <asio.hpp>
#include <cstdint>

class Listener
{
public:
	Listener(asio::io_context& io, uint16_t port);
	
	void Start();
	
	void Stop();
private:
	void DoAccept();


private:
	asio::io_context& io_;
	asio::ip::tcp::acceptor acceptor_;


	uint64_t nextSessionId_ = 1;
};