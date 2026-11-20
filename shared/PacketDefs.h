#pragma once
#include <cstdint>

constexpr uint16_t kServerPort = 7777;
// 16KB 넘으면 종료.
constexpr uint16_t kMaxBodySize = 16 * 1024;

#pragma pack(push,1)
struct PacketHeader
{
	uint16_t bodyLen;
	uint16_t id;
};

#pragma pack(pop)

constexpr uint16_t kHeaderSize = sizeof(PacketHeader);
static_assert(kHeaderSize == 4);

enum class PacketId : uint16_t
{
	// 0 ~ 99:  클라 <-> 서버
	C2S_Echo			= 1,
	S2C_Echo			= 2,

	C2S_Register		= 10,  // {"id":"...", "password":"...", "nickname":"..."}
	S2C_RegisterResult	= 11,  // {"result":0}
	C2S_Login			= 12,  // {"id":"...", "password":"..."}
	S2C_LoginResult		= 13,  // {"result":0, "accountId":1, "nickname":"..."}  (성공일 때만 accountId, nickname)

	// 100 ~ 199: 데디 -> 서버 
	D2S_Register		= 100,
	D2S_Status			= 101,
	D2S_Result			= 102,

	// 200 ~ 299: 서버 -> 데디
	S2D_RegisterAck		= 200,
	S2D_Allocate		= 201,
};

enum class AuthResult : uint8_t
{
	Ok					= 0,
	InvalidRequest		= 1, // 필드 누락, 길이 초과, JSON꺼짐
	IdTaken				= 2, // 가입: 이미 있는 아이디.
	NicknameTaken		= 3, // 가입: 이미 있는 닉네임
	WrongCredentials	= 4, // 로그인: ID가 없거나 비밀번호가 틀림 (구분 안함)
	ServerError			= 5, // DB 오류 등'
};