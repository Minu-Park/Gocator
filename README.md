# Gocator

> LMI Gocator 3D 센서의 제어 및 GoPxL SDK 기반의 데이터 취득을 처리하는 C++ 핵심 라이브러리 모듈입니다.

---

## 📌 주요 특징
- **Gocator Facade 제어**: 센서 연결 수명주기, 데이터 Grabbing 상태 모니터링, 단일/연속 획득 및 센서 노출 시간 등의 파라미터를 제어하는 API를 제공합니다.
- **REST 파라미터 제어**: Gocator 내장 REST API 트래버스를 통해 원격으로 장치 속성을 읽거나 설정할 수 있는 클라이언트를 내장하고 있습니다.
- **Qt6 제어 위젯 지원**: UI 상에서 실시간으로 장치를 모니터링하고 파라미터 트리를 조회할 수 있는 `QGocatorWidget`을 제공합니다.

## 🛠️ 요구 사양 및 의존성
- **OS**: macOS / Windows
- **언어 표준**: C++17 이상
- **의존 라이브러리**:
  - LMI GoPxL SDK (프로젝트 루트 하위 `GoPxL-SDK/` 디렉토리에 구성 필요)
  - CMake 3.16+
  - Qt6 (선택 사항, GUI 위젯 활성화 시 필요)

## 🚀 빌드 및 사용 예제

### 1. 빌드 방법
상위 CMake 프로젝트에서 라이브러리 타겟으로 연동하여 링크합니다.
단독 빌드 시 아래 명령을 사용합니다.
```bash
mkdir build && cd build
cmake ../C++
make -j4
```

### 2. 사용 예제
```cpp
#include "Gocator.h"
#include <memory>

int main()
{
    auto gocator = std::make_unique<Gocator>();

    // GDP 데이터 수신 콜백 등록
    gocator->registerGrabCallback([](const GoPxLSdk::GoDataSet& dataSet, size_t frame) {
        // 데이터셋 분석 및 처리 ...
    });

    // ... 스캔 기동 ...
}
```

## ⚠️ 이미지 취득 관련 참고사항
연결 후 데이터 취득 상태임에도 실시간 렌더링에 실패하는 경우 아래의 출력 설정을 점검하십시오.
1. **List Sources**를 호출하여 센서가 제공하는 지원 출력 소스들을 조회합니다.
2. **Surface Output** 설정을 켜서 3D 표면 출력을 활성화합니다.
3. **Set Output**으로 적절한 데이터 소스(예: `topIntensityImage` 또는 `topRangeImage`)가 전달되도록 지정합니다.
