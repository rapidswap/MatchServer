# MatchServer

언리얼 데디케이티드 서버 게임용 로그인 / 방 / 자동매칭 TCP 서버.
C++20, Asio, spdlog, CMake + vcpkg(manifest 모드). 포트폴리오 겸 팀 프로젝트의 서버 파트.

설계 배경과 결정 사항은 `docs/DESIGN.md`를 먼저 읽을 것.

## 작업 방식 (중요)
- 사용자는 서버 프로그래머로서 직접 구현하며 학습 중이다.
  코드를 통째로 써주기보다 설명, 힌트, 코드 리뷰, 에러 원인 분석을 우선한다.
  사용자가 "만들어줘"라고 명시한 경우에만 구현 코드를 작성한다.
- 스레딩, 연결 관리, 비동기 흐름은 면접에서 설명할 수 있어야 하므로 이유를 함께 설명한다.
- 한 단계가 끝날 때마다 커밋 단위로 끊는다.
- 작업 하는 내용은 커밋 별로 학습을 위해 "C:\Users\user\Documents\GameDev Knowledge" 에 정리한다.

## 구조
- server/ : 매칭 서버
- dummy_client/ : 부하 테스트용 더미 클라이언트 (언리얼 없이 접속/로그인/매칭 흉내)
- shared/ : 패킷 정의 (PacketDefs.h). 언리얼 프로젝트와 공유한다
- 네트워크 계층(Asio)과 게임 로직(방, 매칭)은 분리한다. 나중에 IOCP 등으로 교체 가능해야 한다

## 규칙
- 패킷 프레임: `[uint16 BodyLen][uint16 PacketId][Body]`, 본문 최대 16KB, 초과 시 연결 종료
- TCP는 스트림이므로 수신 버퍼에 쌓고 완성된 패킷만 꺼내 처리한다
- IO 스레드는 읽고 쓰기만, 로직은 단일 스레드 큐에서 처리한다 (락 최소화)
- 비동기 콜백에서 Session 수명은 shared_from_this()로 보장한다
- 로그인 전 연결은 다른 패킷을 거부하고, 일정 시간 안에 로그인하지 않으면 끊는다
- 비밀값(EOS Client Secret, DB 정보)은 저장소에 커밋하지 않는다

## 빌드 (프로젝트 루트에서)
```
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Debug
```
실행 파일: build\Debug\server.exe, build\Debug\dummy_client.exe
