# ADS Client 인수인계 문서

## 1. 프로젝트 개요
- 목적: TwinCAT TcCOM 모듈과 ADS 통신하여 모터 상태 조회/제어/로깅을 수행하는 Qt GUI 클라이언트
- 주요 기능:
  - ADS 연결/해제
  - 모터 상태 Polling 및 테이블 표시
  - Main/Sub/Jog/Path 명령 송신
  - LoggingBuffer 기반 CSV 로깅
  - Python 스크립트 기반 로깅 그래프 생성

## 2. 주요 파일
- 메인 UI/통신 로직: `src/mainwindow.cpp`, `src/mainwindow.h`
- ADS wire/layout 정의: `src/twincat_logging_buffer.h`
- Path 파라미터 정의: `src/twincat_path_cmd.h`
- 로그 플로팅: `plotting/plot_logging.py`

## 3. 현재 코드 상태 (중요)
- Heartbeat 송신 기능: **현재 제거됨**
  - 과거에 추가되었지만 현재 `MainWindow`에 heartbeat timer/slot/변수 없음
- F6/F7 SetPosition 프리셋: **활성**
  - `setupSetPosShortcuts()`에서 M16~M30 값 일괄 입력
- `Deg` 컬럼: **활성**
  - LowerBody M16~M30에 대해 zero offset 기반으로 계산
- 로깅 토크 단위 변환: **없음**
  - `nActualTorque` raw(int16) 값을 그대로 CSV에 기록/플롯
- `UpperBodyMini` 스코프: **활성**
  - 모듈 범위 `M2~M8` (7개), Topology active range도 동일
  - Logging 미지원 (UI에서 비활성)
  - Mini `DataMotorSt`에는 `nWcState`가 없어 WC는 `Conn` 고정 표시

## 4. 통신/스코프 구조
- ADS 접근은 `IndexGroup/IndexOffset` 상수 기반
- BodyScope:
  - `WholeBody`, `UpperBody`, `UpperBodyMini`, `LowerBody`
  - scope별 byte length/offset/slot 해석이 다름
- 실시간 상태는 `pollTimer_`로 주기적으로 읽어 테이블에 반영

### UpperBodyMini ADS 맵(현재 반영)
- Port: `350`
- DataMotorSt: `IG=0x1010010`, `IO=0x83000000`, `Len=105`
- DataMotorCmd: `IG=0x1010010`, `IO=0x83000069`, `Len=105`
- MainCmd: `IG=0x1010010`, `IO=0x84000000`, `Len=2`
- SubCmd: `IG=0x1010010`, `IO=0x84000002`, `Len=2`
- PathCmd(배열 시작): `IG=0x1010010`, `IO=0x84000004`, `Len=147`
- JogCmd: `IG=0x1010010`, `IO=0x84000097`, `Len=4`
- LoggingBuffer: 없음

### UpperBodyMini PathCmd 주의
- `PathCmd[0]` 원소 길이가 `21` 바이트(`nTotalTime`, `nStepSize`, `nUpdate`, `nSetPosition`)로 확인됨
- 현재 코드에 UpperBodyMini 전용 직렬화(`21B`)를 추가하여 Path Generation 동작함
- Mini Path에서는 `nAccTime`, `nProfileMode` 필드가 없으므로 해당 UI 값은 반영되지 않음

## 5. 모터 상태 테이블 컬럼
- 현재 컬럼:
  - `#`, `SW`, `Err`, `Mode`, `WC`, `Pos`, `Vel`, `Trq`, `TgtPos`, `FolErr`, `Set pos`, `Set vel`, `Set trq`, `Deg`
- `Set pos`: 셀 직접 편집 가능
- `Set vel`, `Set trq`: setpoint row 위젯으로 입력

## 6. 로깅 동작 요약
- 저장 경로: `logging/<session>/M0.csv ... M30.csv`
- CSV 헤더:
  - `utc_ms,batch,bufferIndex,nStatusWord,nActualPosition,nActualVelocity,nActualTorque,nErrorCode,nModeOfOperationDisp,nWcState`
- scope에 포함된 모듈만 기록
- ADS read 실패 시 로깅 중지 및 경고 표시
- `UpperBodyMini` 선택 시 Logging은 지원하지 않음

## 7. 플로팅 스크립트 출력 (`plotting/plot_logging.py`)
- 전체 위치: `*_all_M0-M30_nActualPosition.png`
- 전체 토크: `*_all_M0-M30_nActualTorque.png`
- LowerBody 토크 3x5 그리드: `*_M16-M30_nActualTorque_grid_3x5.png`
- 일부 모듈 개별 위치(EXTRA_MODULES) 그래프도 생성

## 8. 운영 시 주의사항
- 토크 값은 raw 스케일이므로 물리 단위(Nm) 해석 시 별도 변환식 필요
- `Deg`는 현재 `counts/deg = 1000.0` 가정값 기반
- F6/F7 프리셋 값은 운영 중 재캘리브레이션 가능성 높음
- 서버 watchdog 정책이 바뀌어 heartbeat가 필요하면 별도 재구현 필요
- `UpperBodyMini`는 WC 신호가 없으므로 WC 컬럼은 연결 고정으로 보임(추후 WC 추가 시 로직 업데이트 필요)
- `UpperBodyMini` Path는 전용 21B 구조로 처리되므로, TMC 구조 변경 시 직렬화 코드 동기화 필요

## 9. 빠른 점검 체크리스트
- [ ] Connect 후 `pollDataMotorSt()` 정상 갱신 여부
- [ ] BodyScope 전환 시 모듈 범위/표시 정상 여부
- [ ] F6/F7 프리셋 입력 값 정상 적용 여부
- [ ] Logging 시작/중지 및 CSV 생성 여부
- [ ] `plotting/plot_logging.py` 실행 후 PNG 생성 여부
