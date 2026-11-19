#pragma once
#include <asio.hpp>
#include "PacketDefs.h"
#include <array>
#include <vector>
#include <cstdint>
#include <memory>
#include <deque>

class Session :public std::enable_shared_from_this<Session>
{
public:
	Session(asio::ip::tcp::socket socket, uint64_t id);
	~Session();
	
	void Start();

	void Send(PacketId id, const uint8_t* body, uint16_t bodyLen);
private:
	void DoRead();

	void Close(const std::error_code& ec);

	bool ProcessRecvBuffer();

	void DoWrite();


private:
	asio::ip::tcp::socket socket_;
	uint64_t id_;
	std::array<uint8_t, 4096> readChunk_ {};
	std::vector<uint8_t> recvBuffer_;
	bool closed_ = false;

	std::deque<std::vector<uint8_t>> sendQueue_;
};