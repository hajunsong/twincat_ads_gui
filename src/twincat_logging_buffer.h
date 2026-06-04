#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

/** TC3 C++ module ADS symbol-by-offset index group (F0200). */
inline constexpr std::uint32_t kTwinCatLoggingBufferIndexGroup = 0x1010010;
inline constexpr std::uint32_t kTcModuleAdsIndexGroup = kTwinCatLoggingBufferIndexGroup;
inline constexpr std::uint32_t kTwinCatLoggingBufferIndexOffset = 0x85000000;
inline constexpr std::size_t kTwinCatLogMotorCount = 31;
inline constexpr std::size_t kTwinCatLogSampleCount = 200;

/** TMC wire stride (MotorSt 16 B, MotorCmd 15 B). */
inline constexpr std::size_t kMotorStWireSize = 16;
inline constexpr std::size_t kMotorCmdWireSize = 15;
inline constexpr std::size_t kMotorStWireStride = kMotorStWireSize;
inline constexpr std::size_t kMotorCmdWireStrideUpperBody = kMotorCmdWireSize;
/** Whole-body ADS/logging slots: 496 B / 31 modules. */
inline constexpr std::size_t kMotorCmdWireStrideWholeBody = 16;

/**
 * Upper Body (TcUpperBodyController/Controller.tmc) ADS map.
 * DataArea AreaNo 3=ServerToClient, 4=ClientToServer, 5=LoggingBuffer.
 */
inline constexpr std::uint32_t kUpperBodyDataMotorStIndexOffset = 0x83000000;
inline constexpr std::uint32_t kUpperBodyDataMotorCmdIndexOffset = 0x83000100;
inline constexpr std::uint32_t kUpperBodyMainCmdIndexOffset = 0x84000000;
inline constexpr std::uint32_t kUpperBodySubCmdIndexOffset = 0x84000002;
inline constexpr std::uint32_t kUpperBodyPathCmdIndexOffset = 0x84000004;
inline constexpr std::uint32_t kUpperBodyLoggingBufferIndexOffset = 0x85000000;
inline constexpr std::size_t kUpperBodyDataMotorStByteLen = 256;
inline constexpr std::size_t kUpperBodyDataMotorCmdByteLen = 240;

/** Upper Body LoggingBuffer: Flag + MotorStBuf[200][16] + MotorCmdBuf[200][16] (MotorCmd). */
inline constexpr std::size_t kUpperBodyLogMotorCount = 16;
inline constexpr std::size_t kUpperBodyLoggingBufferByteLen = 99201;
inline constexpr std::size_t kUpperBodyLoggingStRegionByteLen =
		kTwinCatLogSampleCount * kUpperBodyLogMotorCount * kMotorStWireStride;
inline constexpr std::size_t kUpperBodyLoggingCmdRegionByteLen =
		kTwinCatLogSampleCount * kUpperBodyLogMotorCount * kMotorCmdWireStrideUpperBody;
inline constexpr std::size_t kUpperBodyLoggingStOffset = 1;
inline constexpr std::size_t kUpperBodyLoggingCmdOffset =
		kUpperBodyLoggingStOffset + kUpperBodyLoggingStRegionByteLen;

static_assert(kUpperBodyLoggingCmdOffset + kUpperBodyLoggingCmdRegionByteLen ==
							kUpperBodyLoggingBufferByteLen,
						"Upper Body LoggingBuffer wire layout (TMC: 1+51200+48000=99201)");
static_assert(15 * kMotorStWireStride + kMotorStWireSize == kUpperBodyDataMotorStByteLen,
							"Upper Body DataMotorSt slot 15 must fit in 256 B");
static_assert(15 * kMotorCmdWireStrideUpperBody + kMotorCmdWireSize ==
							kUpperBodyDataMotorCmdByteLen,
							"Upper Body DataMotorCmd slot 15 must fit in 240 B");

#pragma pack(push, 1)
struct MotorStWire {
	std::uint16_t nStatusWord;
	std::int32_t nActualPosition;
	std::int32_t nActualVelocity;
	std::int16_t nActualTorque;
	std::uint16_t nErrorCode;
	std::int8_t nModeOfOperationDisplay;
	std::uint8_t nWcState;
};

struct MotorCmdWire {
	std::uint16_t nControlWord;
	std::int32_t nTargetPosition;
	std::int32_t nTargetVelocity;
	std::int16_t nTargetTorque;
	std::uint16_t nMaxTorque;
	std::int8_t nModeOfOperation;
};
#pragma pack(pop)

static_assert(sizeof(MotorStWire) == 16, "MotorStWire must match TMC layout");
static_assert(sizeof(MotorCmdWire) == 15, "MotorCmdWire must match TMC layout");
static_assert(offsetof(MotorStWire, nActualPosition) == 2, "");
static_assert(offsetof(MotorStWire, nActualVelocity) == 6, "");
static_assert(offsetof(MotorStWire, nActualTorque) == 10, "");
static_assert(offsetof(MotorStWire, nErrorCode) == 12, "");
static_assert(offsetof(MotorStWire, nModeOfOperationDisplay) == 14, "");
static_assert(offsetof(MotorStWire, nWcState) == 15, "");
static_assert(offsetof(MotorCmdWire, nTargetPosition) == 2, "");
static_assert(offsetof(MotorCmdWire, nTargetVelocity) == 6, "");
static_assert(offsetof(MotorCmdWire, nTargetTorque) == 10, "");
static_assert(offsetof(MotorCmdWire, nMaxTorque) == 12, "");
static_assert(offsetof(MotorCmdWire, nModeOfOperation) == 14, "");

/**
 * Lower Body (TcLowerBodyController/Controller.tmc) ADS map.
 * DataArea AreaNo 3=ServerToClient, 4=ClientToServer, 5=LoggingBuffer
 * → index offset high byte 0x83 / 0x84 / 0x85 (0x80 + area).
 */
inline constexpr std::uint32_t kLowerBodyDataMotorStIndexOffset = 0x83000000;
inline constexpr std::uint32_t kLowerBodyDataMotorCmdIndexOffset = 0x830000F0;
inline constexpr std::uint32_t kLowerBodyMainCmdIndexOffset = 0x84000000;
inline constexpr std::uint32_t kLowerBodySubCmdIndexOffset = 0x84000002;
inline constexpr std::uint32_t kLowerBodyPathCmdIndexOffset = 0x84000004;
inline constexpr std::uint32_t kLowerBodyLoggingBufferIndexOffset = 0x85000000;
inline constexpr std::size_t kLowerBodyDataMotorStByteLen = 240;
inline constexpr std::size_t kLowerBodyDataMotorCmdByteLen = 225;

/** Lower Body LoggingBuffer: Flag + MotorStBuf[200][15] + MotorCmdBuf[200][15] (MotorCmd). */
inline constexpr std::size_t kLowerBodyLogMotorCount = 15;
inline constexpr std::size_t kLowerBodyLoggingBufferByteLen = 93001;
inline constexpr std::size_t kLowerBodyLoggingStRegionByteLen =
		kTwinCatLogSampleCount * kLowerBodyLogMotorCount * kMotorStWireStride;
inline constexpr std::size_t kLowerBodyLoggingCmdRegionByteLen =
		kTwinCatLogSampleCount * kLowerBodyLogMotorCount * kMotorCmdWireStrideUpperBody;
inline constexpr std::size_t kLowerBodyLoggingStOffset = 1;
inline constexpr std::size_t kLowerBodyLoggingCmdOffset =
		kLowerBodyLoggingStOffset + kLowerBodyLoggingStRegionByteLen;

static_assert(kLowerBodyLoggingCmdOffset + kLowerBodyLoggingCmdRegionByteLen ==
							kLowerBodyLoggingBufferByteLen,
						"Lower Body LoggingBuffer wire layout (TMC: 1+48000+45000=93001)");
static_assert(14 * kMotorStWireStride + kMotorStWireSize == kLowerBodyDataMotorStByteLen,
							"Lower Body DataMotorSt slot 14 must fit in 240 B");
static_assert(14 * kMotorCmdWireStrideUpperBody + kMotorCmdWireSize ==
							kLowerBodyDataMotorCmdByteLen,
							"Lower Body DataMotorCmd slot 14 must fit in 225 B");

using LogMotorStSample = MotorStWire;

/**
 * Whole-body logging ring buffer row: 16 B MotorCmd slot (15 B wire + 1 B pad per TMC ADS map).
 */
#pragma pack(push, 1)
struct LogMotorCmdSample {
	std::uint16_t nControlWord;
	std::int32_t nTargetPosition;
	std::int32_t nTargetVelocity;
	std::int16_t nTargetTorque;
	std::uint16_t nMaxTorque;
	std::int8_t nModeOfOperation;
	std::uint8_t _wirePad;
};
#pragma pack(pop)

static_assert(sizeof(LogMotorCmdSample) == 16, "");

inline const MotorStWire *motorStWireAt(const std::uint8_t *base, std::size_t index,
																				std::size_t stride = kMotorStWireStride) {
	return reinterpret_cast<const MotorStWire *>(base + index * stride);
}

inline const MotorCmdWire *motorCmdWireAt(const std::uint8_t *base, std::size_t index,
																					std::size_t stride) {
	return reinterpret_cast<const MotorCmdWire *>(base + index * stride);
}

inline bool motorStWireFits(std::size_t byteLength, std::size_t index,
														std::size_t stride = kMotorStWireStride) {
	return index * stride + sizeof(MotorStWire) <= byteLength;
}

inline bool motorCmdWireFits(std::size_t byteLength, std::size_t index, std::size_t stride) {
	return index * stride + sizeof(MotorCmdWire) <= byteLength;
}

inline std::size_t motorStWireCount(std::size_t byteLength) {
	std::size_t count = 0;
	while (motorStWireFits(byteLength, count)) {
		++count;
	}
	return count;
}

inline std::size_t motorCmdWireCount(std::size_t byteLength, std::size_t stride) {
	if (stride == 0) {
		return 0;
	}
	std::size_t count = 0;
	while (motorCmdWireFits(byteLength, count, stride)) {
		++count;
	}
	return count;
}

inline const MotorStWire *upperBodyLogMotorStAt(const std::uint8_t *base, std::size_t sampleIndex,
																								std::size_t localModule) {
	return motorStWireAt(base + kUpperBodyLoggingStOffset,
											 sampleIndex * kUpperBodyLogMotorCount + localModule, kMotorStWireStride);
}

inline const MotorCmdWire *upperBodyLogMotorCmdAt(const std::uint8_t *base, std::size_t sampleIndex,
																									std::size_t localModule) {
	return motorCmdWireAt(base + kUpperBodyLoggingCmdOffset,
												sampleIndex * kUpperBodyLogMotorCount + localModule,
												kMotorCmdWireStrideUpperBody);
}

inline const MotorStWire *lowerBodyLogMotorStAt(const std::uint8_t *base, std::size_t sampleIndex,
																								std::size_t localModule) {
	return motorStWireAt(base + kLowerBodyLoggingStOffset,
											 sampleIndex * kLowerBodyLogMotorCount + localModule, kMotorStWireStride);
}

inline const MotorCmdWire *lowerBodyLogMotorCmdAt(const std::uint8_t *base, std::size_t sampleIndex,
																									std::size_t localModule) {
	return motorCmdWireAt(base + kLowerBodyLoggingCmdOffset,
												sampleIndex * kLowerBodyLogMotorCount + localModule,
												kMotorCmdWireStrideUpperBody);
}

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
