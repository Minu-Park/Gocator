# Gocator

GoPxL SDK 기반 Gocator 설정 관리 프로젝트.

## 목표

API class와 Qt class를 분리한다. 현재 단계는 API class 우선 구현이며, Qt는 API 경계 확정 뒤 진행한다.

## 구성

- `GoPxL-SDK/`: 로컬 SDK 사본.
- `src/gocator/GocatorConnection.*`: Gocator SDK runtime, manual IP connection, lifecycle wrapper.
- `src/gocator/GocatorDiscovery.*`: Gocator 자동 검색 및 manual IP config 생성.
- `src/gocator/GocatorAcquisition.*`: GDP single-frame acquisition wrapper.
- `src/gocator/GocatorSettingsManager.*`: Gocator 설정 read/update/call 전용 클래스.
- `src/ui/main.cpp`: Qt Widgets debug UI entry point.
- `src/main.cpp`: 설정 작업 확인용 CLI.
- `docs/API_QT_SPLIT_CHECKLIST.md`: API/Qt 분리 작업 체크리스트.

## 빌드

```bash
cmake -S . -B cmake-build-debug
cmake --build cmake-build-debug -j 8
```

Qt debug UI is built when Qt6 Widgets is available. The default Qt prefix is `/opt/Qt/6.7.2/gcc_64`.

```bash
cmake -S . -B cmake-build-debug -DCMAKE_PREFIX_PATH=/opt/Qt/6.7.2/gcc_64
cmake --build cmake-build-debug -j 8
```

## 실행

```bash
./cmake-build-debug/gocator_settings
./cmake-build-debug/gocator_settings discover
./cmake-build-debug/gocator_settings <sensor-ip> info
./cmake-build-debug/gocator_settings <sensor-ip> read /scan/visibleSensors/
./cmake-build-debug/gocator_settings <sensor-ip> profile-output
./cmake-build-debug/gocator_debug_ui
```

No-argument execution is the CLion smoke path: it tries SDK discovery first, then falls back to `192.168.1.10 info`.

`profile-output`은 장비를 정지 가능한 상태로 만든 뒤 profile mode, Gocator Protocol, GDP output만 설정함.

The debug UI can connect, inspect scanner info, tune scan mode/intensity/uniform spacing/optional exposure, configure one GDP output source, and grab frames until an image or valid profile is received. Preview is available for common grayscale/RGB image payloads and profile payloads.
