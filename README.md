# Gocator

GoPxL SDK 기반 Gocator 설정 관리 프로젝트.

## 목표

API class와 Qt class를 분리한다. 현재 단계는 API class 우선 구현이며, Qt는 API 경계 확정 뒤 진행한다.

## 구성

- `GoPxL-SDK/`: 로컬 SDK 사본.
- `src/gocator/GocatorDiscovery.*`: Gocator 자동 검색 및 manual IP config 생성.
- `src/gocator/GocatorSettingsManager.*`: Gocator 설정 read/update/call 전용 클래스.
- `src/main.cpp`: 설정 작업 확인용 CLI.
- `docs/API_QT_SPLIT_CHECKLIST.md`: API/Qt 분리 작업 체크리스트.

## 빌드

```bash
cmake -S . -B cmake-build-debug
cmake --build cmake-build-debug -j 8
```

## 실행

```bash
./cmake-build-debug/gocator_settings discover
./cmake-build-debug/gocator_settings <sensor-ip> info
./cmake-build-debug/gocator_settings <sensor-ip> read /scan/visibleSensors/
./cmake-build-debug/gocator_settings <sensor-ip> profile-output
```

`profile-output`은 장비를 정지 가능한 상태로 만든 뒤 profile mode, Gocator Protocol, GDP output만 설정함. 데이터 수집은 이 클래스 책임 아님.
