#pragma once
#include <asio.hpp>
#include "PacketDefs.h"
#include <array>
#include <vector>
#include <cstdint>
#include <memory>
#include <deque>
#include "ISession.h"
#include "INetworkHandler.h"

class Session :public ISession,public std::enable_shared_from_this<Session>
{
public:
	Session(asio::ip::tcp::socket socket, asio::io_context& logicIo,INetworkHandler& handler,uint64_t id);
	~Session();
	
	void Start();



	
private:
	void DoRead();

	void Close(const std::error_code& ec);

	bool ProcessRecvBuffer();

	void DoWrite();


private:
	asio::ip::tcp::socket socket_;
	asio::io_context& logicIo_;
	INetworkHandler& handler_;
	uint64_t id_;

	std::array<uint8_t, 4096> readChunk_ {};
	std::vector<uint8_t> recvBuffer_;
	bool closed_ = false;

	std::deque<std::vector<uint8_t>> sendQueue_;


public:
	// Inherited via ISession
	uint64_t Id() const override;
	void Disconnect() override;
	void Send(PacketId id, const uint8_t* body, uint16_t bodyLen) override;
};