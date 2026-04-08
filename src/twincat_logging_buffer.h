#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

/** `Controller_Obj.LoggingBuffer.Data` — Controller.tmc DataArea LoggingBuffer, Symbol Data. */
inline constexpr std::uint32_t kTwinCatLoggingBufferIndexGroup = 0x1010010;
inline constexpr std::uint32_t kTwinCatLoggingBufferIndexOffset = 0x85000000;
inline constexpr std::size_t kTwinCatLogMotorCount = 31;
inline constexpr std::size_t kTwinCatLogSampleCount = 200;

/**
 * TMC MotorSt (GUID …754fd): UINT/DINT/DINT/INT/UINT/SINT/BOOL — packed 16 B.
 * MotorStBuffer: [200][31] (첫 차원 샘플 인덱스, 둘째 차원 모듈).
 */
#pragma pack(push, 1)
struct LogMotorStSample {
  std::uint16_t nStatusWord;
  std::int32_t nActualPosition;
  std::int32_t nActualVelocity;
  std::int16_t nActualTorque;
  std::uint16_t nErrorCode;
  std::int8_t nModeOfOperationDisp;
  std::uint8_t nWcState;
};
#pragma pack(pop)

static_assert(sizeof(LogMotorStSample) == 16, "");

/**
 * TMC MotorCmd (GUID …8597): UINT + 3×DINT/INT/UINT/INT — packed 16 B.
 * MotorCmdBuffer: [200][31].
 */
#pragma pack(push, 1)
struct LogMotorCmdSample {
  std::uint16_t nControlWord;
  std::int32_t nTargetPosition;
  std::int32_t nTargetVelocity;
  std::int16_t nTargetTorque;
  std::uint16_t nMaxTorque;
  std::int16_t nModeOfOperation;
};
#pragma pack(pop)

static_assert(sizeof(LogMotorCmdSample) == 16, "");

/**
 * Data = Flag(BOOL) + MotorStBuffer[200][31] + MotorCmdBuffer[200][31]
 * → 1 + 6200×16 + 6200×16 = 198401 (TMC SubItem 순서).
 */
#pragma pack(push, 1)
struct TwinCatLoggingBufferData {
  std::uint8_t flag;
  std::array<std::array<LogMotorStSample, kTwinCatLogMotorCount>, kTwinCatLogSampleCount>
      motorStBuffer;
  std::array<std::array<LogMotorCmdSample, kTwinCatLogMotorCount>, kTwinCatLogSampleCount>
      motorCmdBuffer;
};
#pragma pack(pop)

static_assert(sizeof(TwinCatLoggingBufferData) == 198401, "");
