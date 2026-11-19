#include "Listener.h"
#include "Session.h"

#include <asio.hpp>
#include <spdlog/spdlog.h>

using tcp = asio::ip::tcp;

Listener::Listener(asio::io_context& io, asio::io_context& logicIo, uint16_t port)
	:io_(io), logicIo_(logicIo), acceptor_(io, tcp::endpoint(tcp::v4(), port), false)
{
}

void Listener::Start()
{
    spdlog::info("listening on port {}", acceptor_.local_endpoint().port());
    DoAccept();   // accept를 "걸어 두기만" 하고 바로 반환. 실제 대기는 main의 io.run()
}

void Listener::DoAccept()
{
    acceptor_.async_accept(
        asio::make_strand(io_),
        [this](std::error_code ec, tcp::socket socket)
        {
            if (ec == asio::error::operation_aborted)
            {
                return;
            }
            if (!ec)
            {
                std::make_shared<Session>(std::move(socket),logicIo_, nextSessionId_++)->Start();
            }
            DoAccept();   // 다음 접속 대기. 이게 없으면 한 명만 받고 끝
        });
}

void Listener::Stop()
{
    std::error_code ignore;

    acceptor_.close(ignore);
}
