# API / Qt Split Checklist

목표: Gocator 실제 구동에 필요한 기능을 API class에 먼저 모으고, Qt는 API 확정 뒤 얇게 붙인다.

## 운영 규칙

- 변경 단위마다 git commit.
- 원인/결과는 commit message와 `AGENTS.md` 로그에 남김.
- remote 없으면 commit까지만 수행. remote 설정 후 push.

## 단계

- [x] 프로젝트 생성.
- [x] `GoPxL-SDK` 로컬 사본 배치.
- [x] CMake SDK include/lib/rpath 연결.
- [x] 초기 `GocatorSettingsManager` 작성.
- [x] git repo 초기화 및 기준선 commit.
- [x] `GocatorDiscovery` 추가: 자동 리스트, manual IP fallback.
- [x] CLion no-arg smoke path: SDK discovery first, manual `192.168.1.10` info fallback.
- [x] `GocatorConnection` 추가: connect/disconnect/start/stop/port/timeout.
- [x] `GocatorResourceClient` 추가: read/update/call/schema/child URI.
- [ ] `GocatorSettingsCatalog` 추가: schema 기반 writable/min/max/enum 추출.
- [ ] `GocatorScanSettings` 추가: scanner detect, scan mode, intensity, spacing.
- [ ] `GocatorOutputSettings` 추가: protocol enable, output clear/add, source 선택.
- [ ] `GocatorAcquisition` 추가: GDP sync/async, 데이터 callback, stop/clear.
- [ ] API sample CLI 정리.
- [ ] Qt adapter 설계.

## 경계

- API: SDK lifetime, discovery, connection, REST 설정, GDP acquisition.
- Qt: UI state, form binding, signal/slot, status 표시.
- 보류: Qt 구현, visualizer, render path.
