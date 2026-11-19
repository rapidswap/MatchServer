#include "Session.h"
#include "PacketDefs.h"     
#include <cstring>          
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

void Session::Send(PacketId id, const uint8_t* body, uint16_t bodyLen)
{
	// 크기 검사.
	if (bodyLen > kMaxBodySize)
	{
		spdlog::warn("[Session {}] Send: body too large {}", id_, bodyLen);
		return;
	}

	// 헤더 만들기.
	PacketHeader hdr{};
	hdr.bodyLen = bodyLen;
	hdr.id = static_cast<uint16_t>(id);

	// [헤더][본문]을 담은 버퍼를 새로 만들고 복사.
	std::vector<uint8_t> buf(kHeaderSize + bodyLen);
	std::memcpy(buf.data(), &hdr, kHeaderSize);
	if (bodyLen > 0)
	{
		std::memcpy(buf.data() + kHeaderSize, body, bodyLen);
	}
	// 세션 strand로 넘기기.
	asio::post(socket_.get_executor(),
		[self = shared_from_this(), buf = std::move(buf)]() mutable
		{
			// 여기부터는 세션 strand 안에서 실행된다.
			if (self->closed_)
			{
				return;
			}

			// 이미 쓰는 중인지 판단.
			const bool writing = !self->sendQueue_.empty();

			// 큐에 넣기.
			self->sendQueue_.push_back(std::move(buf));

			// 쓰는 중이 아니었으면 내가 시작.
			if (!writing)
			{
				self->DoWrite();
			}
		});
}

void Session::DoRead()
{
	auto self = shared_from_this();
	socket_.async_read_some(asio::buffer(readChunk_),
		[this, self](std::error_code ec, std::size_t bytes)
		{
			if (ec)
			{
				// 연결이 끝났거나 에러: 더 이상 작업을 걸지 않는다 → self가 사라지며 Session 소멸
				Close(ec);
				return;
			}

			recvBuffer_.insert(recvBuffer_.end(), readChunk_.begin(), readChunk_.begin() + bytes);
			if (!ProcessRecvBuffer())
			{
				return;
			}

			DoRead();   // 다음 데이터 대기
		});
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
		spdlog::info("[Session {}] Disconnected", id_);
	}
	else if (ec == asio::error::connection_reset)
	{
		spdlog::info("[Session {}] Disconnected (reset)", id_);
	}
	else if (ec == asio::error::operation_aborted)
	{
	}
	else
	{
		spdlog::warn("[Session {}] error: {}", id_, ec.message());
	}

	std::error_code ignore;
	socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ignore);
	socket_.close(ignore);
}

bool Session::ProcessRecvBuffer()
{
	size_t offset = 0;

	while (true)
	{
		const size_t remain = recvBuffer_.size() - offset;
		if (remain < kHeaderSize)
		{
			break;
		}

		PacketHeader hdr;
		std::memcpy(&hdr, recvBuffer_.data() + offset, kHeaderSize);

		if(hdr.bodyLen>kMaxBodySize)
		{
			spdlog::warn("[Session {}] invalid bodyLen {}",id_,hdr.bodyLen);
			Close(asio::error::make_error_code(asio::error::message_size));
			return false;
		}

		const size_t packetSize = kHeaderSize + hdr.bodyLen;
		if (remain < packetSize)
		{
			break;
		}

		const uint8_t* body = recvBuffer_.data() + offset + kHeaderSize;
		spdlog::info("[Session {}] packet id={} len={}", id_, hdr.id, hdr.bodyLen);

		Send(PacketId::S2C_Echo, body, hdr.bodyLen); //  추가: 임시 에코.

		offset += packetSize;
	}

	recvBuffer_.erase(recvBuffer_.begin(), recvBuffer_.begin() + offset);
	return true;
}

void Session::DoWrite()
{
	auto self = shared_from_this();
	asio::async_write(socket_, asio::buffer(sendQueue_.front()),
		[this, self](std::error_code ec, std::size_t /*bytes*/)
		{
			if (ec)
			{
				Close(ec);
				return;
			}

			sendQueue_.pop_front();

			if (!sendQueue_.empty())
			{
				DoWrite();
			}
		});
	
}
