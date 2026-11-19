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
	C2S_Echo = 1,
	S2C_Echo = 2,

	// 100 ~ 199: 데디 -> 서버 
	D2S_Register = 100,
	D2S_Status = 101,
	D2S_Result = 102,

	// 200 ~ 299: 서버 -> 데디
	S2D_RegisterAck=200,
	S2D_Allocate=201,

};