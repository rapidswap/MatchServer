cmake_minimum_required(VERSION 3.20)
project(MatchServer CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
	add_compile_options(/utf-8)
endif()

find_package(asio CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

find_package(unofficial-libmysql CONFIG REQUIRED)
find_package(unofficial-sodium CONFIG REQUIRED)

add_executable(server
	server/Main.cpp
	server/net/Listener.cpp
	server/net/Session.cpp
	server/logic/GameServer.cpp
	server/logic/SessionManager.cpp
	server/db/MemoryAccountStore.cpp
	server/db/DbConfig.cpp
	server/db/PasswordHasher.cpp
)

target_link_libraries(server PRIVATE asio::asio spdlog::spdlog nlohmann_json::nlohmann_json
	unofficial::libmysql::libmysql unofficial-sodium::sodium)
target_compile_definitions(server PRIVATE _WIN32_WINNT=0x0A00)
target_include_directories(server PRIVATE shared server)
target_precompile_headers(server PRIVATE server/pch.h)

add_executable(dummy_client dummy_client/main.cpp)
target_include_directories(dummy_client PRIVATE shared)
target_link_libraries(dummy_client PRIVATE asio::asio spdlog::spdlog nlohmann_json::nlohmann_json)
target_compile_definitions(dummy_client PRIVATE _WIN32_WINNT=0x0A00)