#pragma once
#include <asio.hpp>
#include <cstdint>
#include "INetworkHandler.h"

class Listener
{
public:
	Listener(asio::io_context& io,asio::io_context& logicIo,INetworkHandler& handler ,uint16_t port);
	
	void Start();
	
	void Stop();
private:
	void DoAccept();


private:
	asio::io_context& io_;
	asio::io_context& logicIo_;
	INetworkHandler& handler_;

	asio::ip::tcp::acceptor acceptor_;


	uint64_t nextSessionId_ = 1;

};