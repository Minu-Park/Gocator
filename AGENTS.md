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
- 2026-05-19: `gocator_debug_ui` 추가. 원인: CLI 반복 디버깅 비용. 결과: Qt Widgets에서 discovery/connect/read/profile-output 수동 확인 가능, API class는 유지.
- 2026-05-19: `GocatorAcquisition` 및 UI grab 추가. 원인: 이미지/GDP 취득 디버깅 필요. 결과: single-frame GDP receive, image metadata/pixel copy, UI 상태/정보/preview 분리.
- 2026-05-19: GDP profile preview 추가. 원인: 현재 출력 source가 image가 아닌 `UNIFORM_PROFILE`. 결과: profile metadata/valid points 추출 및 UI line preview 표시.
- 2026-05-19: GDP grab retry/raw stats 추가. 원인: 첫 frame 또는 scene 상태에 따라 profile valid=0. 결과: valid profile까지 N frame 수신, range/intensity 통계로 원인 구분.
- 2026-05-19: Scan tuning UI 추가. 원인: valid profile 확보를 위해 scan mode/intensity/spacing/exposure 조절 필요. 결과: scanner read/schema와 안전한 optional exposure 적용 경로 확보.
