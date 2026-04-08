# TwinCAT ADS GUI

TwinCAT와 [Beckhoff ADS](https://github.com/Beckhoff/ADS) C++ 라이브러리로 통신하는 Qt 5 데스크톱 앱입니다. **Connect**로 ADS 라우트를 열고 `GetDeviceInfo()` 등으로 PLC를 확인하며, 연결 중에는 버튼이 **Disconnect**로 바뀝니다. 연결 필드는 `QSettings`로 저장되어 다음 실행 시 복원됩니다.

토폴로지(휴머노이드 모듈 배치), 그래프 창(QCustomPlot), 경로/로깅 관련 UI가 포함되어 있습니다.

## 요구 사항

- CMake 3.16+
- C++17
- Qt 5.12+ — `Widgets`, `Network`, `PrintSupport`
- Beckhoff ADS 소스 — 저장소의 `third_party/Beckhoff.ADS` (configure 시 자동 다운로드 없음)

## Beckhoff ADS 준비 (오프라인 가능)

네트워크가 될 때 한 번만 클론하거나 복사합니다.

```bash
git clone https://github.com/Beckhoff/ADS.git third_party/Beckhoff.ADS
```

ADS가 다른 경로에 있으면:

```bash
cmake -DBECKHOFF_ADS_ROOT=/path/to/Beckhoff.ADS ..
```

## 빌드

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/path/to/Qt/5.15.2/gcc_64   # Qt가 PATH에 없을 때
cmake --build build
```

생성 실행 파일: `build/twincat_ads_gui` (생성기에 따라 경로는 다를 수 있음).

## 실행

- 일반적으로 `build/`에서 실행하면 `applicationDirPath()`의 상위에 있는 프로젝트 루트 `figure/`를 찾습니다.
- 작업 디렉터리가 프로젝트 루트이면 `./figure`도 후보입니다.

## Topology 이미지 (`figure/`)

토폴로지 위젯 상태 아이콘용 PNG를 프로젝트 루트 **`figure/`** 에 둡니다.

| 파일 |
|------|
| `state_disconnected.png` |
| `state_normal.png` |
| `state_fault.png` |

경로 탐색 로직은 `src/topologywidget.cpp`의 `figureDirectoryPath()`를 참고하세요.

## 로깅 CSV & 플롯 스크립트

앱이 기록하는 CSV는 보통 프로젝트 루트 **`logging/<세션타임스탬프>/M0.csv` … `M30.csv`** 형태입니다.

`plotting/plot_logging.py`는 `logging/`에서 가장 최근 세션을 골라 `nActualPosition` 등을 그래프로 **`plotting/output/`** 에 저장합니다.

```bash
cd plotting
python3 -m venv venv
source venv/bin/activate   # Windows: venv\Scripts\activate
pip install -r requirements.txt
python plot_logging.py --help
```

## `.gitignore` 요약

버전 관리에서 제외하는 것: `build/`, `plotting/venv/`, `logging/`, `plotting/output/`, Qt `*.user` 등. 리소스용 `figure/`는 커밋 대상으로 둡니다.

## UI 기본값

| 항목 | 기본값 |
|------|--------|
| Target host | `192.168.0.142` |
| AMS NetId | `221.102.111.71.1.1` |
| Port | `350` |

연결 중에는 host / AMS NetId / port 입력이 비활성화됩니다. **Disconnect**로 라우트와 로컬 ADS 포트를 해제합니다.

## 설정 저장

앱 종료 시 조직/이름(`TwinCatAdsGui` / `twincat_ads_gui`) 아래에 저장됩니다. Linux에서는 대개 `~/.config/TwinCatAdsGui/twincat_ads_gui.conf` (INI).

## UI 편집

`src/mainwindow.ui`를 **Qt Designer** 또는 Qt Creator로 연 뒤 다시 빌드하면 `uic`가 헤더를 갱신합니다.

## 라이선스

애플리케이션 코드는 프로젝트 정책에 따릅니다. ADS 라이브러리는 Beckhoff 조건을 따르며, `third_party/Beckhoff.ADS`를 참고하세요.
