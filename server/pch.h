#pragma once
// 서버 전용 PCH. CMake가 server의 모든 .cpp 앞에 강제로 넣는다 (각 .cpp에서 include 안 해도 됨).
// 규칙
//  - 자주 쓰고 잘 안 바뀌는 외부/표준 헤더만 넣는다. 우리 프로젝트 헤더는 넣지 않는다 (바뀔 때마다 전체 재빌드).
//  - asio는 넣지 않는다. 넣으면 logic/ 에서도 asio가 보여서 "로직은 asio를 모른다" 규칙을 컴파일러가 못 잡는다.
//  - .h 파일은 PCH에 기대지 않고 필요한 include를 직접 쓴다 (헤더 단독으로 컴파일 가능해야 함, shared/는 UE로도 간다).

// 표준
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <deque>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <utility>

// 외부 라이브러리
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
