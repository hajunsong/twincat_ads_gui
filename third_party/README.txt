오프라인 CMake 빌드를 위해 Beckhoff ADS 소스를 이 디렉터리에 둡니다.

배치 경로(고정):
  third_party/Beckhoff.ADS/
  └── AdsLib/ ...

한 번(인터넷 가능할 때) 받기:
  git clone https://github.com/Beckhoff/ADS.git third_party/Beckhoff.ADS

또는 아카이브를 풀어 같은 구조로 맞춥니다.

Qt5는 시스템에 설치하거나, 오프라인 환경에서는 Qt 설치 트리를 미리 복사한 뒤
cmake -DCMAKE_PREFIX_PATH=/path/to/Qt/5.x/gcc_64 .. 로 지정하세요.

루트 CMake에서 경로만 바꿀 때:
  cmake -DBECKHOFF_ADS_ROOT=/절대/경로/Beckhoff.ADS ..
