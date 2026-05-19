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
