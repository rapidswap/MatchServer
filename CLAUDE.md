# MatchServer

언리얼 데디케이티드 서버 게임용 로그인 / 방 / 데디 진입 TCP 서버.
C++20, Asio, spdlog, MySQL, CMake + vcpkg(manifest 모드). 포트폴리오 겸 팀 프로젝트의 서버 파트.

설계 배경과 결정 사항은 `docs/DESIGN.md`를 먼저 읽을 것.

## 작업 방식 (중요)
- 사용자는 서버 프로그래머로서 직접 구현하며 학습 중이다.
  코드를 통째로 써주기보다 설명, 힌트, 코드 리뷰, 에러 원인 분석을 우선한다.
  사용자가 "만들어줘"라고 명시한 경우에만 구현 코드를 작성한다.
- 스레딩, 연결 관리, 비동기 흐름은 면접에서 설명할 수 있어야 하므로 이유를 함께 설명한다.
- 한 단계가 끝날 때마다 커밋 단위로 끊는다.
- 작업 하는 내용은 커밋 별로 학습을 위해 "C:\Users\user\Documents\GameDev Knowledge" 에 정리한다.
  (허브: `20 프로젝트/MatchServer/MatchServer.md`, 일지: `50 일지/Wanted5th_MatchServer/`)

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
- 로그인은 ID + 비밀번호, 계정은 MySQL에 저장한다 (EOS 등 외부 인증은 쓰지 않는다)
- DB 쿼리는 로직 스레드에서 직접 하지 않는다. DB 스레드에 넘기고 결과를 로직 스레드로 post한다
- 비밀번호는 salt + 느린 해시로만 저장하고, SQL은 prepared statement로만 실행한다
- 비밀값(DB 접속 정보 등)은 저장소에 커밋하지 않는다 (로컬 설정 파일, .gitignore 참고)

## 빌드 (프로젝트 루트에서)
```
cmake --preset my-vs2026
cmake --build --preset my-debug
```
- `my-*` 프리셋은 개인용 `CMakeUserPresets.json`(커밋 안 함)에 있고 `VCPKG_ROOT=C:/vcpkg`를 넣어준다.
  팀원은 `VCPKG_ROOT` 환경 변수를 설정하고 `vs2026` / `debug` 프리셋을 쓴다
- 실행 파일: build\Debug\server.exe, build\Debug\dummy_client.exe (상대 경로는 프로젝트 루트에서 실행)

## 실행·접속 방식 (배포)
- 서버는 별도의 서버 PC에서 실행하고, 포트는 `7777`(TCP)이다. 그 PC에서 이 저장소의 코드를 빌드해 실행한다. 클라이언트 접속은 확인 완료.
- 실제 서버 주소는 공개 저장소에 올리지 않는다. 주소와 접속 메모는 `CLAUDE.local.md`(커밋 안 함, `.gitignore`)에 둔다.
- 클라이언트(언리얼)는 주소를 코드에 박지 않고 언리얼 프로젝트의 `Config/DefaultGame.ini`에서 읽는다.
  ```
  [/Script/ExpeditionKamen.EKNetSubsystem]
  ServerHost=<서버 PC 주소>
  ServerPort=7777
  ```
  (언리얼 저장소: `2nd_TeamProject/Expedition-Kamen`, 비공개 팀 저장소라 그쪽 ini에는 실제 주소가 들어간다. `EKNetSubsystem`이 읽는다.) 서버 주소·포트를 바꾸면 이 값도 같이 맞춘다.
- 서버 주소, 비밀번호, DB 접속 정보는 이 저장소에 커밋하지 않는다(`CLAUDE.local.md`, `config/local.json`, `secrets/`).
- 서버 코드를 만들 때는 "다른 PC에서 접속받는 서버"로 생각한다: `0.0.0.0` 바인딩, 방화벽 TCP 7777 허용, DB는 서버와 같은 PC의 로컬 MySQL.
- 패킷 정의는 `shared/PacketDefs.h`와 언리얼의 `EKPacket.h`를 항상 같은 값으로 유지한다.
