# 작업 지침

- Be ultra concise (keywords)
- Korean for chat, English for docs
- No translation unless asked
- Clear cause → effect
- Ask before crossing module boundaries or hurting performance
- 현재 목표: API class 먼저 구현, Qt class는 다음 단계에서 분리 설계.

## 작업 로그

- 2026-05-19: 프로젝트 생성, `GoPxL-SDK` 사본 배치, `GocatorSettingsManager` 설정 전용 클래스 추가.
- 2026-05-19: git 운영 규칙 추가, API/Qt 분리 체크리스트 작성.
- 2026-05-19: `GocatorDiscovery` 추가. 원인: IP 자동 리스트/manual target 필요. 결과: discovery CLI와 API config 생성 경로 확보.
- 2026-05-19: no-arg smoke 실행 경로 추가. 원인: CLion Run config 인자 입력 비용과 SDK discovery 실패 가능성. 결과: discovery 우선, 실패 시 `192.168.1.10` info 수동 확인.
- 2026-05-19: `GocatorConnection` 추가. 원인: 연결 생명주기와 설정 작업 책임 혼재. 결과: connect/disconnect/start/stop/port/timeout API 분리, settings manager는 연결 객체 재사용.
- 2026-05-19: `GocatorResourceClient` 추가. 원인: REST primitive와 설정 workflow 책임 혼재. 결과: read/update/call/schema/child URI API 분리, settings manager는 resource client 재사용.
