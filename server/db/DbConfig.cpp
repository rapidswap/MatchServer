#include "DbConfig.h"
#include <fstream>

std::optional<DbConfig> LoadDbConfig(const std::string& path)
{
	std::ifstream file(path);
	if (!file)
	{
		spdlog::error("[DbConfig] cannot open {}", path);
		return std::nullopt;
	}

	try
	{
		const nlohmann::json j = nlohmann::json::parse(file);
		const nlohmann::json& db = j.at("db");

		DbConfig cfg;
		cfg.host = db.at("host").get<std::string>();
		cfg.port = db.at("port").get<uint16_t>();
		cfg.user = db.at("user").get<std::string>();
		cfg.password = db.at("password").get<std::string>();
		cfg.database = db.at("database").get<std::string>();

		spdlog::info("[DbConfig] {}@{}:{}/{}", cfg.user, cfg.host, cfg.port, cfg.database);
		return cfg;
	}
	catch (const std::exception& e)
	{
		// parse_error / out_of_range(키 없음) / type_error(타입 틀림)
		spdlog::error("[DbConfig] invalid {}: {}", path, e.what());
		return std::nullopt;
	}
}
