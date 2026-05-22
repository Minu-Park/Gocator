# Gocator

> LMI Gocator 3D 센서의 제어 및 GoPxL SDK 기반의 데이터 취득을 담당하는 C++ 핵심 라이브러리 모듈입니다.

이 모듈은 Gocator 센서 네트워크 검색, 연결 유지, 고속 데이터 스트림(GDP) 수신 및 REST API 연동을 통한 동적 파라미터 제어를 전담합니다.

---

## 📌 주요 특징 (Key Features)
- **통합 파사드 제공 (`Gocator` Facade)**: Gocator 시스템 연결 수명주기, 데이터 취득 상태 모니터링, 단일/연속 Grabbing 및 주요 파라미터(노출 시간, 스캔 모드 등) 조정을 제어하는 통합 퍼블릭 API를 제공합니다.
- **REST API 제어 및 동적 트리 생성**: LMI Gocator의 동적 REST 파라미터 트리를 실시간으로 탐색하고 값을 읽고 쓰는 매니저 모듈(`GocatorSettingsManager`, `GocatorResourceClient`)을 탑재하고 있습니다.
- **Qt6 제어 GUI 위젯 (`QGocatorWidget`)**: Camera 위젯과 레이아웃 디자인 스펙을 일치시킨 통합 제어 위젯을 제공합니다. 사용자는 UI에서 센서를 검색/선택하고 연결하여 실시간으로 파라미터 트리를 튜닝할 수 있습니다.
- **의존성 결합 격리**: 타 모듈의 특정 하드웨어 SDK 의존성을 원천 차단하기 위해, 취득된 GoPxL GDP 데이터셋을 중립 3D 씬 데이터로 변환하는 `GocatorDataSetScene3DAdapter` 소스 파일은 호스트 애플리케이션 빌드 타겟에 포함되어 컴파일됩니다.

## 🛠️ 요구 사양 및 의존성 (Prerequisites & Dependencies)
- **OS**: macOS / Windows
- **언어 표준**: C++17 이상
- **필수 의존성**:
  - **LMI GoPxL SDK**: 모듈 루트 하위의 `GoPxL-SDK/` 디렉토리에 로컬 SDK 카피본이 구성되어 있어야 합니다.
  - **CMake**: 버전 3.16 이상
- **선택적 의존성**:
  - **Qt6**: GUI 위젯(`QGocatorWidget`) 구성 시 요구됩니다.

## 🚀 시작하기 (Quick Start & Build)

### 1. 빌드 방법
Playground 통합 워크스페이스 내에서 이 모듈은 `gocator_core` 정적 라이브러리 타겟으로 자동 빌드됩니다. 단독으로 빌드하고자 할 경우 다음과 같이 수행합니다.

```bash
# Gocator 단독 빌드 절차
mkdir build && cd build
cmake ..
make -j4
```

### 2. 사용 예제 (API Usage)
```cpp
#include "Gocator.h"
#include <iostream>
#include <memory>

int main()
{
    auto gocator = std::make_unique<Gocator>();

    // 특정 IP 주소를 통한 강제 연결 또는 Discovery 연동
    // gocator->connect("192.168.1.10");

    // GDP 데이터 수신 콜백 등록
    const auto callbackId = gocator->registerGrabCallback(
        [](const GoPxLSdk::GoDataSet& dataSet, size_t frameIndex) {
            std::cout << "Gocator 수신 프레임 #" << frameIndex << std::endl;
            // 수신 데이터 처리 (호스트 측에서 어댑터를 통해 GraphicsEngine으로 라우팅)
        });

    // 스캔 시작
    // gocator->start();

    // ... 스캔 및 프레임 처리 ...

    // 리소스 정리
    // gocator->deregisterGrabCallback(callbackId);
    // gocator->stop();
    // gocator->close();
}
```

---

## 📂 디렉토리 구조 (Directory Structure)

```text
Gocator/
├── CMakeLists.txt                # gocator_core 빌드 타겟 정의 및 GoPxL SDK 링크 설정
├── GoPxL-SDK/                    # 로컬 GoPxL SDK 라이브러리 및 헤더 파일들
└── C++/
    ├── Gocator.h/.cpp            # 퍼블릭 인터페이스 파사드 클래스
    ├── GocatorTypes.h            # 네트워크 접속 정보 및 내부 전역 상수 선언
    ├── Internal/
    │   ├── GocatorAcquisition.h/.cpp # GDP 싱글 프레임 취득용 내부 래퍼
    │   ├── GocatorConnection.h/.cpp  # SDK 런타임 연결 관리 및 장치 수명주기 래퍼
    │   ├── GocatorDiscovery.h/.cpp   # 네트워크 상의 센서 탐색 및 수동 IP 매핑
    │   ├── GocatorResourceClient.h/.cpp # REST 리소스 억세스용 클라이언트
    │   └── GocatorSettingsManager.h/.cpp # REST 기반의 스캔 파라미터 트래버스 및 값 갱신
    └── Utility/
        ├── Qt/
        │   └── QGocatorWidget.h/.cpp  # Qt 제어 위젯 및 동적 파라미터 리스트 UI
        └── GraphicsEngine/
            └── GocatorDataSetScene3DAdapter.h/.cpp # GoPxL 데이터를 중립 RangeFrame으로 변환
```

---

## ⚠️ 아키텍처 규칙 및 제약 (Boundaries & Rules)
- **SDK 노출 금지**: `Gocator` 모듈을 연동하는 호스트 앱 외에 다른 라이브러리 모듈(특히 `GraphicsEngine`)은 GoPxL SDK 전용 타입 및 헤더를 참조하지 않아야 합니다. 센서 제어와 라이프사이클의 세부 사항은 본 모듈 내부로 완전히 캡슐화됩니다.
- **이미지 비노출 시 디버깅 체크리스트**:
  * 연결 수립 후 데이터를 수신함에도 화면에 3D 이미지가 렌더링되지 않을 경우 다음 단계를 점검하십시오.
  1. **List Sources** 조회를 통해 디바이스가 제공하는 출력 소스 명칭을 탐색합니다.
  2. **Surface Output** 설정을 정상 활성화했는지 검사합니다.
  3. **Set Output** 설정을 활용하여 `topIntensityImage` 또는 `topRangeImage` 등의 소스를 명시적으로 전송해줄 것을 디바이스에 요청했는지 확인합니다.
  4. 16비트 높이맵 데이터는 이미지 화면 출력을 위해 정규화 과정을 거치게 됩니다.

## 📝 라이선스 (License)
본 프로젝트는 독점 상용 라이선스를 따르며 무단 공유와 도용을 엄격히 제한합니다.
