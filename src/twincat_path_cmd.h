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
              "PathParameter wire layout: 3×LREAL + UINT + BOOL + DINT");
static_assert(offsetof(PathParameter, nStepSize) == 8, "");
static_assert(offsetof(PathParameter, nAccTime) == 16, "");
static_assert(offsetof(PathParameter, nProfileMode) == 24, "");
static_assert(offsetof(PathParameter, nUpdate) == 26, "");
static_assert(offsetof(PathParameter, nSetPosition) == 27, "");

inline constexpr std::size_t kTwinCatPathCmdByteLen =
    sizeof(PathParameter) * kTwinCatPathCmdCount;
static_assert(kTwinCatPathCmdByteLen == 961, "");

/** Upper Body: PathCmd for M0–M15 (16 × PathParameter). */
inline constexpr std::size_t kUpperBodyPathCmdModuleCount = 16;
inline constexpr std::size_t kUpperBodyPathCmdByteLen =
    sizeof(PathParameter) * kUpperBodyPathCmdModuleCount;
static_assert(kUpperBodyPathCmdByteLen == 496, "");

/** Lower Body: PathCmd for M16–M30 (15 × PathParameter, local slots 0–14). */
inline constexpr std::size_t kLowerBodyPathCmdModuleCount = 15;
inline constexpr std::size_t kLowerBodyPathCmdByteLen =
    sizeof(PathParameter) * kLowerBodyPathCmdModuleCount;
static_assert(kLowerBodyPathCmdByteLen == 465, "");
static_assert(14 * sizeof(PathParameter) + sizeof(PathParameter) == kLowerBodyPathCmdByteLen,
              "Lower Body PathCmd slot 14 must fit in 465 B");

inline bool isScopedPathCmdByteLength(std::uint32_t byteLength) {
  return byteLength == kUpperBodyPathCmdByteLen || byteLength == kLowerBodyPathCmdByteLen;
}

inline bool pathParameterFits(std::size_t byteLength, std::size_t index) {
  return index * sizeof(PathParameter) + sizeof(PathParameter) <= byteLength;
}

inline std::size_t pathParameterCount(std::size_t byteLength) {
  std::size_t count = 0;
  while (pathParameterFits(byteLength, count)) {
    ++count;
  }
  return count;
}

inline const PathParameter *pathParameterAt(const std::uint8_t *base, std::size_t index) {
  return reinterpret_cast<const PathParameter *>(base + index * sizeof(PathParameter));
}

inline PathParameter *pathParameterAt(std::uint8_t *base, std::size_t index) {
  return reinterpret_cast<PathParameter *>(base + index * sizeof(PathParameter));
}

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
