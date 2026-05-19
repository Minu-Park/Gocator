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
- `src/main.cpp`: 통합 entry point (CLI/UI).
- `src/cli_main.cpp`: CLI 로직.
- `src/ui_main.cpp`: Qt Widgets debug UI 로직.

## 빌드

```bash
mkdir build && cd build
cmake ..
make -j4
```

Qt debug UI는 Qt6 Widgets이 있을 때 함께 빌드됩니다.

## 실행

```bash
# GUI 실행 (인자 없음)
./build/gocator

# GUI 강제 실행
./build/gocator --ui

# CLI 실행 (인자 있음)
./build/gocator discover
./build/gocator <sensor-ip> info
./build/gocator <sensor-ip> read /scan/visibleSensors/
```

## 이미지 디버깅 안내

- Gocator 연결 후 "images are not coming out"인 경우:
  1. **List Sources** 버튼으로 현재 센서가 지원하는 출력 소스 목록을 확인하세요.
  2. **Surface Output** 버튼을 눌러 Surface(2D) 출력을 활성화해보세요.
  3. **Set Output**에 `topIntensityImage` 등의 소스를 직접 입력하여 이미지를 요청할 수 있습니다.
  4. 16비트 데이터(Surface heightmap 등)는 자동으로 정규화(Normalization)되어 미리보기에 표시됩니다.

No-argument execution is the CLion smoke path: it tries SDK discovery first, then falls back to `192.168.1.10 info`.

`profile-output`은 장비를 정지 가능한 상태로 만든 뒤 profile mode, Gocator Protocol, GDP output만 설정함.

The debug UI can connect, inspect scanner info, tune scan mode/intensity/uniform spacing/optional exposure, configure one GDP output source, and grab frames until an image or valid profile is received. Preview is available for common grayscale/RGB image payloads and profile payloads.
