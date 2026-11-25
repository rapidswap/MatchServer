#pragma once
#include <cstdint>
#include <optional>
#include <string>

// DB 접속 정보. config/local.json의 "db" 객체에서 읽는다 (커밋 안 하는 파일).
// password는 로그에 절대 찍지 않는다.
struct DbConfig
{
	std::string host;
	uint16_t port = 3306;
	std::string user;
	std::string password;
	std::string database;
};

// 파일이 없거나 형식이 틀리면 에러 로그를 남기고 nullopt.
std::optional<DbConfig> LoadDbConfig(const std::string& path);
