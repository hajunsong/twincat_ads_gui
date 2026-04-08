#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

/** TwinCAT `ARRAY [0..30] OF PathParameter` under ClientToServer. */
inline constexpr std::size_t kTwinCatPathCmdCount = 31;
/** ADS IOffs of PathCmd (immediately after MainCmd+SubCmd @ 0x84000000). */
inline constexpr std::uint32_t kTwinCatPathCmdIndexOffset = 0x84000004;

#pragma pack(push, 1)
struct PathParameter {
  double nTotalTime;
  double nStepSize;
  double nAccTime;
  std::uint16_t nProfileMode;
  std::uint8_t nUpdate;  // BOOL
  std::int32_t nSetPosition;  // DINT
};
#pragma pack(pop)

static_assert(sizeof(PathParameter) == 31,
              "PathParameter: 3×LREAL + UINT + BOOL + DINT (packed), match TMC");

inline constexpr std::size_t kTwinCatPathCmdByteLen =
    sizeof(PathParameter) * kTwinCatPathCmdCount;
static_assert(kTwinCatPathCmdByteLen == 961, "");

/** MainCmd + SubCmd @ 0x84000000, PathCmd @ 0x84000004 — one ADS write. */
#pragma pack(push, 1)
struct ClientToServerPathWrite {
  std::uint16_t mainCmd;
  std::uint16_t subCmd;
  std::array<PathParameter, kTwinCatPathCmdCount> pathCmd;
};
#pragma pack(pop)

static_assert(sizeof(ClientToServerPathWrite) == 4 + sizeof(PathParameter) * kTwinCatPathCmdCount,
              "");
