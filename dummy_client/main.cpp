#include <spdlog/spdlog.h>
#include <asio.hpp>
#include "PacketDefs.h"
#include <chrono>
#include <cstring>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>
#include <nlohmann/json.hpp>

std::vector<uint8_t> MakePacket(PacketId id, std::string_view body)
{
	PacketHeader hdr{};
	hdr.bodyLen = static_cast<uint16_t>(body.size());
	hdr.id = static_cast<uint16_t>(id);

	std::vector<uint8_t> buf(kHeaderSize + body.size());

	std::memcpy(buf.data(), &hdr, kHeaderSize);
	std::memcpy(buf.data() + kHeaderSize, body.data(), body.size());

	return buf;
}

// 패킷 하나를 받는다 (헤더 → 본문). 서버가 끊으면 asio 예외
std::pair<uint16_t, std::vector<uint8_t>> RecvPacket(asio::ip::tcp::socket& socket)
{
	PacketHeader hdr{};
	asio::read(socket, asio::buffer(&hdr, kHeaderSize));
	
	std::vector<uint8_t> body(hdr.bodyLen);
	if (hdr.bodyLen > 0)
	{
		asio::read(socket, asio::buffer(body));
	}

	return { hdr.id,std::move(body) };
}

void RunNormal(asio::ip::tcp::socket& socket, int clientNo)
{
	for (int i = 0; i < 5; ++i)
	{
		std::string msg = "client" + std::to_string(clientNo) + " msg" + std::to_string(i);
		auto pkt = MakePacket(PacketId::C2S_Echo, msg);
		asio::write(socket, asio::buffer(pkt));
		spdlog::info("[client {}] sent id={} len={} : {}", clientNo,
			static_cast<uint16_t>(PacketId::C2S_Echo), msg.size(), msg);

		auto [rid, rbody] = RecvPacket(socket);                                            // ①
		std::string_view text(reinterpret_cast<const char*>(rbody.data()), rbody.size());  // ②
		spdlog::info("[client {}] recv id={} len={} : {}", clientNo, rid, rbody.size(), text);

		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
}

// 패킷 하나를 cut 바이트에서 둘로 잘라 보낸다.
// cut < 4  : 헤더도 덜 온 상태  → 서버 ProcessRecvBuffer의 첫 번째 break
// cut >= 4 : 헤더는 왔지만 본문이 덜 온 상태 → 두 번째 break
void RunSplit(asio::ip::tcp::socket& socket, int clientNo, std::size_t cut)
{
	std::string msg = "client" + std::to_string(clientNo) + " split";
	auto pkt = MakePacket(PacketId::C2S_Echo, msg);

	asio::write(socket, asio::buffer(pkt.data(), cut));
	spdlog::info("[client {}] sent first {} bytes (of {}), waiting 2s...", clientNo, cut, pkt.size());
	std::this_thread::sleep_for(std::chrono::seconds(2));

	asio::write(socket, asio::buffer(pkt.data() + cut, pkt.size() - cut));
	spdlog::info("[client {}] sent remaining {} bytes", clientNo, pkt.size() - cut);
	std::this_thread::sleep_for(std::chrono::seconds(1));
}

// 패킷 두 개를 버퍼 하나에 이어 붙여 write 한 번으로 보낸다
void RunMerge(asio::ip::tcp::socket& socket, int clientNo)
{
	auto a = MakePacket(PacketId::C2S_Echo, "first");
	auto b = MakePacket(PacketId::C2S_Echo, "second");
	a.insert(a.end(), b.begin(), b.end());

	asio::write(socket, asio::buffer(a));
	spdlog::info("[client {}] sent 2 packets in one write ({} bytes)", clientNo, a.size());
	std::this_thread::sleep_for(std::chrono::seconds(1));
}

// bodyLen이 최대치(16KB)를 넘는 헤더만 보낸다. 서버가 끊어야 정상
void RunOversize(asio::ip::tcp::socket& socket, int clientNo)
{
	PacketHeader hdr{};
	hdr.bodyLen = 20000;
	hdr.id = static_cast<uint16_t>(PacketId::C2S_Echo);
	asio::write(socket, asio::buffer(&hdr, kHeaderSize));
	spdlog::info("[client {}] sent header with bodyLen={}", clientNo, hdr.bodyLen);

	// 서버가 끊었는지 확인: 끊겼으면 read가 eof/reset 예외를 던진다
	uint8_t dummy;
	asio::read(socket, asio::buffer(&dummy, 1));
	spdlog::error("[client {}] server did NOT close the connection", clientNo);
}

// JSON 요청 하나를 보내고 응답 하나를 받아 출력한다. 받은 JSON을 돌려준다 (깨졌으면 is_discarded()).
nlohmann::json SendJsonAndPrint(asio::ip::tcp::socket& socket, int clientNo, PacketId id, const nlohmann::json& req)
{
	auto packet = MakePacket(id, req.dump());
	asio::write(socket, asio::buffer(packet));
	spdlog::info("[client {}] sent id={} body={}", clientNo, static_cast<uint16_t>(id), req.dump());

	auto [respId, body] = RecvPacket(socket);
	nlohmann::json resp = nlohmann::json::parse(body.begin(), body.end(), nullptr, false);
	if (resp.is_discarded())
	{
		spdlog::error("[client {}] recv id={} invalid json", clientNo, respId);
		return resp;
	}
	spdlog::info("[client {}] recv id={} body={}", clientNo, respId, resp.dump());
	return resp;
}

// Echo 하나를 보내고 응답을 받아 출력한다.
void EchoOnce(asio::ip::tcp::socket& socket, int clientNo, const std::string& msg)
{
	auto pkt = MakePacket(PacketId::C2S_Echo, msg);
	asio::write(socket, asio::buffer(pkt));
	spdlog::info("[client {}] sent echo : {}", clientNo, msg);

	auto [rid, rbody] = RecvPacket(socket);
	std::string_view text(reinterpret_cast<const char*>(rbody.data()), rbody.size());
	spdlog::info("[client {}] recv id={} : {}", clientNo, rid, text);
}

// 한 연결에서: 로그인 → (성공하면) Echo 3번 → 같은 연결로 로그인 한 번 더 (거절돼야 정상)
// 3-3 로그인 전 차단 / 로그인 후 허용 / 중복 로그인 요청 거절을 확인한다.
void RunLoginEcho(asio::ip::tcp::socket& socket, int clientNo, const std::string& id, const std::string& pw)
{
	const nlohmann::json loginReq = { {"id", id}, {"password", pw} };

	nlohmann::json resp = SendJsonAndPrint(socket, clientNo, PacketId::C2S_Login, loginReq);
	auto it = resp.find("result");
	if (resp.is_discarded() || it == resp.end() || !it->is_number() || it->get<int>() != static_cast<int>(AuthResult::Ok))
	{
		spdlog::warn("[client {}] login failed, stop (echo now would be disconnected)", clientNo);
		return;
	}

	for (int i = 0; i < 3; ++i)
	{
		EchoOnce(socket, clientNo, "client" + std::to_string(clientNo) + " after-login " + std::to_string(i));
	}

	spdlog::info("[client {}] login again on same connection (expect result 1 InvalidRequest)", clientNo);
	SendJsonAndPrint(socket, clientNo, PacketId::C2S_Login, loginReq);
}

void RunRegister(asio::ip::tcp::socket& socket, int clientNo, const std::string& id, const std::string& pw, const std::string& nickname)
{
	SendJsonAndPrint(socket, clientNo, PacketId::C2S_Register, { {"id", id}, {"password", pw}, {"nickname", nickname} });
}

void RunLogin(asio::ip::tcp::socket& socket, int clientNo, const std::string& id, const std::string& pw)
{
	SendJsonAndPrint(socket, clientNo, PacketId::C2S_Login, { {"id", id}, {"password", pw} });
}

int main(int argc, char* argv[])
{
	// 사용법: dummy_client.exe [번호] [모드] [모드별 인자...]
	//   모드: normal(기본) | split | split6 | merge | oversize
	//         register <id> <password> <nickname>
	//         login    <id> <password>
	//         loginecho <id> <password>   (로그인 → Echo 3번 → 재로그인 거절 확인)
	//   주의: 3-3부터 normal/split/merge는 로그인 없이 Echo를 보내므로 서버가 끊는 게 정상
	int clientNo = argc > 1 ? std::stoi(argv[1]) : 0;
	std::string mode = argc > 2 ? argv[2] : "normal";

	try
	{
		asio::io_context io;
		asio::ip::tcp::socket socket(io);
		asio::ip::tcp::resolver resolver(io);

		// 127.0.0.1:7777 -> endpoint로 바꾸고 연결.
		asio::connect(socket, resolver.resolve("127.0.0.1", "7777"));
		spdlog::info("[client {}] connected to server (my port {})", clientNo, socket.local_endpoint().port());

		if (mode == "normal")        RunNormal(socket, clientNo);
		else if (mode == "split")    RunSplit(socket, clientNo, 3);
		else if (mode == "split6")   RunSplit(socket, clientNo, 6);
		else if (mode == "merge")    RunMerge(socket, clientNo);
		else if (mode == "oversize") RunOversize(socket, clientNo);
		else if (mode == "register")
		{
			if (argc < 6)
			{
				spdlog::error("usage: dummy_client.exe <no> register <id> <password> <nickname>");
				return 1;
			}
			RunRegister(socket, clientNo, argv[3], argv[4], argv[5]);
		}
		else if (mode == "login")
		{
			if (argc < 5)
			{
				spdlog::error("usage: dummy_client.exe <no> login <id> <password>");
				return 1;
			}
			RunLogin(socket, clientNo, argv[3], argv[4]);
		}
		else if (mode == "loginecho")
		{
			if (argc < 5)
			{
				spdlog::error("usage: dummy_client.exe <no> loginecho <id> <password>");
				return 1;
			}
			RunLoginEcho(socket, clientNo, argv[3], argv[4]);
		}
		else
		{
			spdlog::error("unknown mode: {} (normal|split|split6|merge|oversize|register|login|loginecho)", mode);
			return 1;
		}

		spdlog::info("[client {}] closing", clientNo);
	}
	catch (const std::exception& e)
	{
		spdlog::error("[client {}] client error: {}", clientNo, e.what());
		return 1;
	}
}
