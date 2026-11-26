#pragma once

#include <optional>
#include <string>

// libsodium(argon2id) 래퍼. 결과 문자열에 salt와 파라미터가 같이 들어가므로 DB에는 이 문자열 하나만 저장한다.
// 해시와 검증은 일부러 느리게한다. 로직 스레드에서 부르지 말고 DB 스레드에서 부른다.

// 서버 시작 시 한 번. 실패하면 서버를 띄우면 안된다.
bool InitPasswordHasher();

// 실패 (메모리 부족)하면 nullopt.
std::optional<std::string> HashPassword(const std::string& password);

// hash: DB에 저장된 문자열. 비밀번호가 맞으면 true;
bool VerifyPassword(const std::string& hash, const std::string& password);