#include "PasswordHasher.h"
#include <sodium.h>

bool InitPasswordHasher()
{
	// 0: 성공, 1: 이미 초기화 됨(성공), -1: 실패.
	if (sodium_init() < 0)
	{
		spdlog::critical("[Hasher] sodium_init failed");
		return false;
	}
	return true;
}

std::optional<std::string> HashPassword(const std::string& password)
{
	char out[crypto_pwhash_STRBYTES]; // 128. DB 정렬 VARCHAR(128)이 이 값에서 나왔다.

	if (crypto_pwhash_str(out, password.c_str(), password.size(),crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0)
	{
		// 실패 원인은 사실상 메모리 할당 실패뿐이다. (MEMLIMIT만큼 잡지 못함).
		spdlog::error("[Hasher] crypto_pwhash_str failed (out of memory)");
		return std::nullopt;
	}

	return std::string(out); // out은 '\0'으로 끝나는 문자열.
}

bool VerifyPassword(const std::string& hash, const std::string& password)
{
	return crypto_pwhash_str_verify(hash.c_str(), password.c_str(), password.size()) == 0;
}
