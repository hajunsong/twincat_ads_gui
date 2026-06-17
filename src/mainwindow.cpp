#include "mainwindow.h"

#include "body_scope.h"
#include "graphdisplaywindow.h"
#include "topologywidget.h"
#include "twincat_logging_buffer.h"
#include "twincat_path_cmd.h"
#include "ui_mainwindow.h"

#include "AdsDevice.h"
#include "AdsException.h"
#include "AdsLib.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QDoubleSpinBox>
#include <QFont>
#include <QGuiApplication>
#include <QGridLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QScreen>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollBar>
#include <QSettings>
#include <QSignalBlocker>
#include <QShortcut>
#include <QStandardItem>
#include <QStatusBar>
#include <QString>
#include <QStringList>
#include <QTableView>
#include <QTextCodec>
#include <QTextStream>
#include <QtGlobal>

#include <array>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

namespace {
constexpr char kSettingsGroup[] = "connection";
constexpr char kWindowSettingsGroup[] = "window";
constexpr char kMotorStColsSettingsGroup[] = "motorStCols";
constexpr char kSavePosSettingsGroup[] = "savePos";
constexpr char kBodyScopeSettingKey[] = "bodyScope";

struct BodyScopeEndpoint {
	const char *host;
	const char *netId;
};

constexpr BodyScopeEndpoint kUpperBodyEndpoint = {"192.168.0.142", "221.102.111.71.1.1"};
constexpr BodyScopeEndpoint kLowerBodyEndpoint = {"192.168.0.132", "221.103.14.13.1.1"};

constexpr std::uint32_t kUpperBodyMiniDataMotorStIndexOffset = 0x83000000;
constexpr std::uint32_t kUpperBodyMiniDataMotorCmdIndexOffset = 0x83000069;
constexpr std::uint32_t kUpperBodyMiniMainCmdIndexOffset = 0x84000000;
constexpr std::uint32_t kUpperBodyMiniSubCmdIndexOffset = 0x84000002;
constexpr std::uint32_t kUpperBodyMiniPathCmdIndexOffset = 0x84000004;
constexpr std::uint32_t kUpperBodyMiniJogCmdIndexOffset = 0x84000097;
constexpr std::size_t kUpperBodyMiniModuleCount = 7;
constexpr std::size_t kUpperBodyMiniDataMotorStByteLen = 105;
constexpr std::size_t kUpperBodyMiniDataMotorCmdByteLen = 105;
constexpr std::size_t kUpperBodyMiniPathCmdByteLen = 147;
static_assert(kUpperBodyMiniDataMotorStByteLen == kUpperBodyMiniModuleCount * 15, "");
static_assert(kUpperBodyMiniDataMotorCmdByteLen == kUpperBodyMiniModuleCount * 15, "");

constexpr std::uint32_t kModuleDataMotorStIndexOffset = 0x83000000;
constexpr std::uint32_t kModuleDataMotorCmdIndexOffset = 0x8300000F;
constexpr std::uint32_t kModuleMainCmdIndexOffset = 0x84000000;
constexpr std::uint32_t kModuleSubCmdIndexOffset = 0x84000002;
constexpr std::uint32_t kModulePathCmdIndexOffset = 0x84000004;
constexpr std::uint32_t kModuleJogCmdIndexOffset = 0x84000019;
constexpr std::size_t kModuleDataMotorStByteLen = 15;
constexpr std::size_t kModuleDataMotorCmdByteLen = 15;
constexpr std::size_t kModulePathCmdByteLen = 21;

constexpr AdsBodyScopeProfile kWholeBodyAdsProfile = {
		350,
		{0x1010010, 0x83000000, 496u},
		{0x1010010, 0x830001F0, 496u},
		{0x1010010, 0x84000000, 2u},
		{0x1010010, 0x84000002, 2u},
		{0x1010010, 0x84000004, static_cast<std::uint32_t>(kTwinCatPathCmdByteLen)},
		{0x1010010, 0x840001D5, 4u},
		{0x1010010, 0x85000000, static_cast<std::uint32_t>(sizeof(TwinCatLoggingBufferData))},
		true,
};

constexpr AdsBodyScopeProfile kUpperBodyAdsProfile = {
		350,
		{kTcModuleAdsIndexGroup, kUpperBodyDataMotorStIndexOffset,
		 static_cast<std::uint32_t>(kUpperBodyDataMotorStByteLen)},
		{kTcModuleAdsIndexGroup, kUpperBodyDataMotorCmdIndexOffset,
		 static_cast<std::uint32_t>(kUpperBodyDataMotorCmdByteLen)},
		{kTcModuleAdsIndexGroup, kUpperBodyMainCmdIndexOffset, 2u},
		{kTcModuleAdsIndexGroup, kUpperBodySubCmdIndexOffset, 2u},
		{kTcModuleAdsIndexGroup, kUpperBodyPathCmdIndexOffset,
		 static_cast<std::uint32_t>(kUpperBodyPathCmdByteLen)},
		{kTcModuleAdsIndexGroup, 0x840001D5, 4u},
		{kTcModuleAdsIndexGroup, kUpperBodyLoggingBufferIndexOffset,
		 static_cast<std::uint32_t>(kUpperBodyLoggingBufferByteLen)},
		false,
};

constexpr AdsBodyScopeProfile kUpperBodyMiniAdsProfile = {
		350,
		{kTcModuleAdsIndexGroup, kUpperBodyMiniDataMotorStIndexOffset,
		 static_cast<std::uint32_t>(kUpperBodyMiniDataMotorStByteLen)},
		{kTcModuleAdsIndexGroup, kUpperBodyMiniDataMotorCmdIndexOffset,
		 static_cast<std::uint32_t>(kUpperBodyMiniDataMotorCmdByteLen)},
		{kTcModuleAdsIndexGroup, kUpperBodyMiniMainCmdIndexOffset, 2u},
		{kTcModuleAdsIndexGroup, kUpperBodyMiniSubCmdIndexOffset, 2u},
		{kTcModuleAdsIndexGroup, kUpperBodyMiniPathCmdIndexOffset,
		 static_cast<std::uint32_t>(kUpperBodyMiniPathCmdByteLen)},
		{kTcModuleAdsIndexGroup, kUpperBodyMiniJogCmdIndexOffset, 4u},
		{0u, 0u, 0u},
		false,
};

constexpr AdsBodyScopeProfile kModuleAdsProfile = {
		350,
		{kTcModuleAdsIndexGroup, kModuleDataMotorStIndexOffset,
		 static_cast<std::uint32_t>(kModuleDataMotorStByteLen)},
		{kTcModuleAdsIndexGroup, kModuleDataMotorCmdIndexOffset,
		 static_cast<std::uint32_t>(kModuleDataMotorCmdByteLen)},
		{kTcModuleAdsIndexGroup, kModuleMainCmdIndexOffset, 2u},
		{kTcModuleAdsIndexGroup, kModuleSubCmdIndexOffset, 2u},
		{kTcModuleAdsIndexGroup, kModulePathCmdIndexOffset,
		 static_cast<std::uint32_t>(kModulePathCmdByteLen)},
		{kTcModuleAdsIndexGroup, kModuleJogCmdIndexOffset, 4u},
		{0u, 0u, 0u},
		false,
};

constexpr AdsBodyScopeProfile kLowerBodyAdsProfile = {
		350,
		{kTcModuleAdsIndexGroup, kLowerBodyDataMotorStIndexOffset,
		 static_cast<std::uint32_t>(kLowerBodyDataMotorStByteLen)},
		{kTcModuleAdsIndexGroup, kLowerBodyDataMotorCmdIndexOffset,
		 static_cast<std::uint32_t>(kLowerBodyDataMotorCmdByteLen)},
		{kTcModuleAdsIndexGroup, kLowerBodyMainCmdIndexOffset, 2u},
		{kTcModuleAdsIndexGroup, kLowerBodySubCmdIndexOffset, 2u},
		{kTcModuleAdsIndexGroup, kLowerBodyPathCmdIndexOffset,
		 static_cast<std::uint32_t>(kLowerBodyPathCmdByteLen)},
		{kTcModuleAdsIndexGroup, 0x840001D5, 4u},
		{kTcModuleAdsIndexGroup, kLowerBodyLoggingBufferIndexOffset,
		 static_cast<std::uint32_t>(kLowerBodyLoggingBufferByteLen)},
		false,
};

/** ADSERR_DEVICE_SERVICENOTSUPP — task ADS ports often reject ReadDeviceInfo. */
constexpr uint32_t kAdsErrServiceNotSupp = 1793;

/** TMC ADS: ServerToClient — DataMotorSt @ 0x83000000, DataMotorCmd @ 0x830001F0 (contiguous). */
constexpr uint32_t kDataMotorStIndexGroup = 0x1010010;
constexpr uint32_t kDataMotorStIndexOffset = 0x83000000;
constexpr size_t kDataMotorStByteLen = 496;
constexpr size_t kDataMotorCmdByteLen = 496;
constexpr size_t kServerToClientStCmdReadLen = kDataMotorStByteLen + kDataMotorCmdByteLen;
static_assert(kServerToClientStCmdReadLen == 992, "");
constexpr size_t kMotorCount = 31;
constexpr int kMotorSetpointRow = static_cast<int>(kMotorCount);
constexpr int kMotorTableRowCount = kMotorSetpointRow + 1;
constexpr int kMotorTableColumnCount = 15;
constexpr int kMotorPollIntervalMs = 100;
constexpr int kLoggingAppendIntervalMs = 100;
constexpr char kLoggingDirName[] = "logging";
constexpr double kLoggingMaxDurationSec = 30.0;
constexpr int kSetPosShortcutFillValue = 500000;
/** Assumed WC before first ADS read / after disconnect (Disconnected). */
constexpr uint8_t kDefaultWcState = 1;

/** QStandardItemModel column indices (must match header order in setupMotorStTable). */
enum MotorStTableCol : int {
	kColIdx = 0,
	kColSw = 1,
	kColErr = 2,
	kColMode = 3,
	kColWc = 4,
	kColPos = 5,
	kColVel = 6,
	kColTrq = 7,
	kColTgtPos = 8,
	kColFolErr = 9,
	kColCmdPos = 10,
	kColSetPos = 11,
	kColSetVel = 12,
	kColSetTrq = 13,
	kColDeg = 14,
};

constexpr int kLowerBodyZeroOffsetStartModule = 16;
constexpr std::array<std::int32_t, 15> kLowerBodyZeroPositionOffset = {
		55788, -38370, -40105, 3293,  -11998, -42117, -38805, -49858,
		-34338, -24624, 22229, -10071, 4976,  8510,   -4324,
};
constexpr double kCountsPerDegree = 1000.0;

enum MotorIndexRoles {
	kMotorTableRoleRealModule = Qt::UserRole + 1,
	kPathComboRoleLocalSlot = Qt::UserRole + 1,
};

/** ClientToServer MainCmd/SubCmd values (twin write @ 0x84000000). */
constexpr uint16_t kCmdMainInit = 0;
constexpr uint16_t kCmdSubInit = 0;
constexpr uint16_t kCmdMainSystem = 1;
constexpr uint16_t kCmdSubStart = 1;
constexpr uint16_t kCmdSubStop = 2;
constexpr uint16_t kCmdSubReset = 3;
constexpr uint16_t kCmdMainMode = 2;
constexpr uint16_t kCmdSubCsp = 1;
constexpr uint16_t kCmdSubCsv = 2;
constexpr uint16_t kCmdSubCst = 3;
constexpr uint16_t kCmdMainPath = 3;
constexpr uint16_t kCmdSubPathTrapezoidal = 1;
constexpr uint16_t kCmdSubPathSCurve = 2;
constexpr uint16_t kCmdSubPathOpRun = 3;
constexpr uint16_t kCmdSubPathOpRepeat = 4;
constexpr uint16_t kCmdSubPathOpCyclic = 5;
constexpr uint16_t kCmdSubPathOpStop = 6;
constexpr uint16_t kCmdMainJog = 4;
constexpr uint16_t kCmdSubJogPlus = 1;
constexpr uint16_t kCmdSubJogMinus = 2;
constexpr uint16_t kCmdSubJogStop = 3;

constexpr const char *kMotorStColSettingKeys[] = {"mode", "wc",    "err",   "trq",    "tgtPos",
																									"setPos", "setVel", "setTrq"};
constexpr int kMotorColSettingCount = static_cast<int>(sizeof(kMotorStColSettingKeys) /
																											 sizeof(kMotorStColSettingKeys[0]));
static_assert(kMotorColSettingCount == 8, "");

static_assert(kMotorCount == kTwinCatPathCmdCount,
							"Motor count must match PathCmd array size in TMC");
static_assert(kMotorCount == kTwinCatLogMotorCount,
							"Motor count must match LoggingBuffer MotorStBuffer");

/** ClientToServer: MainCmd @ 0x84000000, SubCmd @ 0x84000002 — one write, 4 bytes. */
constexpr uint32_t kClientMainSubIndexGroup = 0x1010010;
constexpr uint32_t kClientMainSubIndexOffset = 0x84000000;
std::size_t motorCmdWireStrideForProfile(const AdsBodyScopeProfile &profile) {
	if (profile.contiguousStCmdRead) {
		return kMotorCmdWireStrideWholeBody;
	}
	return kMotorCmdWireStrideUpperBody;
}

const char *bodyScopeLabel(BodyScope scope) {
	switch (scope) {
		case BodyScope::WholeBody:
			return "WholeBody";
		case BodyScope::UpperBody:
			return "UpperBody";
		case BodyScope::UpperBodyMini:
			return "UpperBodyMini";
		case BodyScope::LowerBody:
			return "LowerBody";
		case BodyScope::Module:
			return "Module";
	}
	return "Unknown";
}

bool bodyScopeSupportsLogging(BodyScope scope) {
	return scope != BodyScope::UpperBodyMini && scope != BodyScope::Module;
}

bool bodyScopeUsesCompactMotorSt(BodyScope scope) {
	return scope == BodyScope::UpperBodyMini || scope == BodyScope::Module;
}

#pragma pack(push, 1)
struct ClientMainSubCmd {
	uint16_t mainCmd;
	uint16_t subCmd;
};
#pragma pack(pop)

static_assert(sizeof(ClientMainSubCmd) == 4, "MainCmd+SubCmd UINT pair");

#pragma pack(push, 1)
struct ClientJogPayload {
	uint16_t id;
	uint16_t tick;
};
#pragma pack(pop)

static_assert(sizeof(ClientJogPayload) == 4, "Jog ID+Tick UINT pair");

#pragma pack(push, 1)
struct UpperBodyMiniMotorStWire {
	std::uint16_t nStatusWord;
	std::int32_t nActualPosition;
	std::int32_t nActualVelocity;
	std::int16_t nActualTorque;
	std::uint16_t nErrorCode;
	std::int8_t nModeOfOperationDisplay;
};
#pragma pack(pop)

static_assert(sizeof(UpperBodyMiniMotorStWire) == 15, "");

#pragma pack(push, 1)
struct UpperBodyMiniPathParameter {
	double nTotalTime;
	double nStepSize;
	std::uint8_t nUpdate;	// BOOL
	std::int32_t nSetPosition;
};
#pragma pack(pop)

static_assert(sizeof(UpperBodyMiniPathParameter) == 21, "");

/** CiA 402–style statusword (0x6041): state from bits 0–3,5–6 + common flags. */
QString formatMotorStatusWord(uint16_t sw) {
	const uint16_t state = static_cast<uint16_t>(sw & 0x006Fu);
	QString base;
	switch (state) {
		case 0x0000:
			base = QStringLiteral("Not ready");
			break;
		case 0x0040:
			base = QStringLiteral("Sw-off");
			break;
		case 0x0021:
			base = QStringLiteral("Ready");
			break;
		case 0x0023:
			base = QStringLiteral("On");
			break;
		case 0x0027:
			base = QStringLiteral("Op en");
			break;
		case 0x0007:
			base = QStringLiteral("QStop");
			break;
		case 0x000F:
			base = QStringLiteral("Fault rxn");
			break;
		case 0x0008:
			base = QStringLiteral("Fault");
			break;
		default:
			base = QStringLiteral("? 0x%1").arg(sw, 4, 16, QChar('0'));
			break;
	}
	QStringList tags;
	if ((sw & (1u << 7)) != 0) {
		tags << QStringLiteral("Warn");
	}
	if ((sw & (1u << 10)) != 0) {
		tags << QStringLiteral("Tgt");
	}
	if ((sw & (1u << 11)) != 0) {
		tags << QStringLiteral("Lim");
	}
	if (!tags.isEmpty()) {
		base += QLatin1Char(' ') + tags.join(QLatin1Char('/'));
	}
	return base;
}

QString formatWcState(uint8_t wc) {
	switch (wc) {
		case 0:
			return QStringLiteral("Conn");
		case 1:
			return QStringLiteral("Disc");
		default:
			return QStringLiteral("? %1").arg(wc);
	}
}

bool lowerBodyZeroOffsetForModule(int moduleId, std::int32_t *offsetOut) {
	if (moduleId < kLowerBodyZeroOffsetStartModule ||
			moduleId >= kLowerBodyZeroOffsetStartModule +
												static_cast<int>(kLowerBodyZeroPositionOffset.size()) ||
			offsetOut == nullptr) {
		return false;
	}
	const std::size_t idx =
			static_cast<std::size_t>(moduleId - kLowerBodyZeroOffsetStartModule);
	*offsetOut = kLowerBodyZeroPositionOffset[idx];
	return true;
}

QString formatDegForModuleFromActualPosition(int moduleId, std::int32_t actualPosition) {
	std::int32_t zeroOffset = 0;
	if (!lowerBodyZeroOffsetForModule(moduleId, &zeroOffset)) {
		return QStringLiteral("—");
	}
	const double deg = static_cast<double>(actualPosition - zeroOffset) / kCountsPerDegree;
	return QString::number(deg, 'f', 3);
}

bool isMotorSetpointColumn(int column) {
	return column == kColCmdPos || column == kColSetPos || column == kColSetVel ||
				 column == kColSetTrq;
}

/** CiA 402 / CANopen-style device error (0x603F class); unknown → "Unknown (0x…)". */
QString formatMotorErrorCode(uint16_t ec) {
	struct Entry {
		uint16_t code;
		const char *en;
	};
	static const Entry k[] = {
			{0x0000, "No error"},
			{0x1000, "Generic error"},
			{0x2310, "Short circuit"},
			{0x2320, "Earth leakage"},
			{0x2330, "Earth fault"},
			{0x3110, "Mains undervoltage"},
			{0x3120, "Mains overvoltage"},
			{0x3210, "DC link undervoltage"},
			{0x3220, "DC link overvoltage"},
			{0x4210, "Drive overtemperature"},
			{0x4310, "Interior overtemperature"},
			{0x5112, "Supply low voltage"},
			{0x5113, "Mains phase missing"},
			{0x6100, "Internal SW error"},
			{0x6320, "Parameter error"},
			{0x7111, "Motor stalled"},
			{0x7112, "Motor stalled"},
			{0x7113, "Motor error"},
			{0x7300, "Sensor error"},
			{0x7305, "Incremental sensor"},
			{0x7306, "Absolute sensor"},
			{0x7500, "Communication error"},
			{0x8110, "CAN overrun"},
			{0x8111, "CAN warning"},
			{0x8120, "Heartbeat error"},
			{0x8130, "CAN bus off"},
			{0x8200, "Protocol error"},
			{0x8210, "Negative limit"},
			{0x8220, "Positive limit"},
			{0x8400, "Velocity error"},
			{0x8611, "Following error"},
			{0x8A00, "Amplifier error"},
			{0x8B00, "Position error"},
			{0xFF01, "Vendor specific"},
			{0xFF02, "Vendor specific"},
			{0xFF03, "Vendor specific"},
	};
	for (const Entry &e : k) {
		if (e.code == ec) {
			return QString::fromUtf8(e.en);
		}
	}
	if ((ec & 0xFF00u) == 0xFF00u) {
		return QStringLiteral("Vendor specific (0x%1)").arg(ec, 4, 16, QChar('0'));
	}
	return QStringLiteral("Unknown (0x%1)").arg(ec, 4, 16, QChar('0'));
}

QString formatAdsState(ADSSTATE s) {
	switch (s) {
		case ADSSTATE_INVALID:
			return QStringLiteral("INVALID");
		case ADSSTATE_RUN:
			return QStringLiteral("RUN");
		case ADSSTATE_STOP:
			return QStringLiteral("STOP");
		case ADSSTATE_RESET:
			return QStringLiteral("RESET");
		case ADSSTATE_INIT:
			return QStringLiteral("INIT");
		case ADSSTATE_IDLE:
			return QStringLiteral("IDLE");
		case ADSSTATE_START:
			return QStringLiteral("START");
		case ADSSTATE_CONFIG:
			return QStringLiteral("CONFIG");
		default:
			return QStringLiteral("%1").arg(static_cast<int>(s));
	}
}
}  // namespace

MainWindow::MainWindow(QWidget *parent)
		: QMainWindow(parent), ui_(new Ui::MainWindow), pollTimer_(this), loggingTimer_(this) {
	ui_->setupUi(this);
	setupMotorStTable();
	setupMotorStTableView();
	installMotorSetpointRowWidgets();
	setupMotorStColumnVisibility();
	setupMotorStWcConnectedFilter();
	pollTimer_.setInterval(kMotorPollIntervalMs);
	connect(&pollTimer_, &QTimer::timeout, this, &MainWindow::pollDataMotorSt);
	connect(ui_->btnConnect, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
	setupPathGenerationUi();
	setupSystemControlUi();
	setupModeOfOperationUi();
	setupJogModeUi();
	setupOperatingUi();
	setupSetPosShortcuts();
	setupLoggingUi();
	connect(&loggingTimer_, &QTimer::timeout, this, &MainWindow::appendLoggingSnapshot);

	graphDisplayWindow_ = new GraphDisplayWindow(this);
	connect(graphDisplayWindow_, &GraphDisplayWindow::selectionChanged, this,
					&MainWindow::onGraphDisplaySelectionChanged);
	connect(ui_->btnGraphView, &QPushButton::clicked, this, &MainWindow::onGraphViewClicked);
	graphDisplayPosByModule_.resize(static_cast<int>(kTwinCatLogMotorCount));

	setupEmbeddedTopology();
	setupBodyScopeUi();

	loadSettings();
}

MainWindow::~MainWindow() {
	stopLogging(true);
	delete ui_;
}

void MainWindow::closeEvent(QCloseEvent *event) {
	stopLogging();
	writeClientMainSubCmd(kCmdMainSystem, kCmdSubStop);
	saveSettings();
	QMainWindow::closeEvent(event);
}

void MainWindow::setupMotorStTableView() {
	QTableView *tv = ui_->tableViewMotorSt;
	tv->setModel(&motorStModel_);
	tv->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
	tv->setSelectionBehavior(QAbstractItemView::SelectRows);
	tv->setSelectionMode(QAbstractItemView::SingleSelection);
	tv->verticalHeader()->setVisible(false);
	tv->horizontalHeader()->setStretchLastSection(true);
	tv->horizontalHeader()->setMinimumSectionSize(64);
	QFont mono(QStringLiteral("monospace"));
	if (!mono.exactMatch()) {
		mono = QFont(QStringLiteral("Monospace"));
	}
	mono.setStyleHint(QFont::TypeWriter);
	mono.setPointSize(qMax(9, font().pointSize()));
	tv->setFont(mono);
}

void MainWindow::setupMotorStTable() {
	lastMotorWc_.fill(kDefaultWcState);
	motorStModel_.clear();
	motorStModel_.setRowCount(kMotorTableRowCount);
	motorStModel_.setColumnCount(kMotorTableColumnCount);
	motorStModel_.setHorizontalHeaderLabels(
			{QStringLiteral("#"), QStringLiteral("SW"), QStringLiteral("Err"),
			 QStringLiteral("Mode"), QStringLiteral("WC"), QStringLiteral("Pos"),
			 QStringLiteral("Vel"), QStringLiteral("Trq"), QStringLiteral("TgtPos"),
			 QStringLiteral("FolErr"), QStringLiteral("CmdPos"), QStringLiteral("Save Pos"),
			 QStringLiteral("Set vel"), QStringLiteral("Set trq"), QStringLiteral("Deg")});
	const Qt::ItemFlags ro = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
	const Qt::ItemFlags roSetPos = ro | Qt::ItemIsEditable;
	for (int r = 0; r < kMotorSetpointRow; ++r) {
		auto *c0 = new QStandardItem(QString::number(r));
		c0->setFlags(ro);
		motorStModel_.setItem(r, 0, c0);
		for (int c = 1; c < kMotorTableColumnCount; ++c) {
			QStandardItem *cell = nullptr;
			if (c == kColWc) {
				cell = new QStandardItem(formatWcState(kDefaultWcState));
				cell->setToolTip(QStringLiteral("raw=1 — Disconnected"));
				cell->setFlags(ro);
			} else if (c == kColCmdPos || c == kColSetPos) {
				cell = new QStandardItem(QString());
				cell->setFlags(roSetPos);
			} else {
				cell = new QStandardItem(QStringLiteral("—"));
				cell->setFlags(ro);
			}
			motorStModel_.setItem(r, c, cell);
		}
	}
	auto *rowLabel = new QStandardItem(QString());
	rowLabel->setFlags(ro);
	motorStModel_.setItem(kMotorSetpointRow, 0, rowLabel);
	for (int c = 1; c < kMotorTableColumnCount; ++c) {
		auto *cell = new QStandardItem(
				isMotorSetpointColumn(c) ? QString() : QStringLiteral("—"));
		cell->setFlags(ro);
		motorStModel_.setItem(kMotorSetpointRow, c, cell);
	}
}

void MainWindow::applyMotorStColumnVisibility() {
	QTableView *tv = ui_->tableViewMotorSt;
	const struct {
		QCheckBox *cb;
		int col;
	} toggles[] = {
			{ui_->cbMotorColMode, kColMode},   {ui_->cbMotorColWc, kColWc},
			{ui_->cbMotorColErr, kColErr},     {ui_->cbMotorColTrq, kColTrq},
			{ui_->cbMotorColTgtPos, kColTgtPos},
			{ui_->cbMotorColSetPos, kColCmdPos}, {ui_->cbMotorColSetVel, kColSetVel},
			{ui_->cbMotorColSetTrq, kColSetTrq},
	};
	for (const auto &t : toggles) {
		tv->setColumnHidden(t.col, !t.cb->isChecked());
	}
	tv->setColumnHidden(kColSetPos, !ui_->cbMotorColSetPos->isChecked());
}

void MainWindow::setupMotorStColumnVisibility() {
	for (QCheckBox *cb :
			 {ui_->cbMotorColMode, ui_->cbMotorColWc, ui_->cbMotorColErr, ui_->cbMotorColTrq,
				ui_->cbMotorColTgtPos, ui_->cbMotorColSetPos, ui_->cbMotorColSetVel,
				ui_->cbMotorColSetTrq}) {
		connect(cb, &QCheckBox::toggled, this,
						[this](bool) { applyMotorStColumnVisibility(); });
	}
	applyMotorStColumnVisibility();
}

void MainWindow::setupMotorStWcConnectedFilter() {
	connect(ui_->cbMotorStWcConnectedOnly, &QCheckBox::toggled, this,
					[this](bool) { applyMotorStWcRowFilter(); });
}

void MainWindow::applyMotorStWcRowFilter() {
	QTableView *tv = ui_->tableViewMotorSt;
	const bool onlyConnected = ui_->cbMotorStWcConnectedOnly->isChecked();
	for (int r = 0; r < kMotorSetpointRow; ++r) {
		const bool inScope = moduleInCurrentScope(r);
		const bool wcConn = (lastMotorWc_[static_cast<std::size_t>(r)] == 0);
		tv->setRowHidden(r, !inScope || (onlyConnected && !wcConn));
	}
	tv->setRowHidden(kMotorSetpointRow, false);
}

void MainWindow::installMotorSetpointRowWidgets() {
	if (motorSetpointVelEdit_ != nullptr) {
		return;
	}
	const QFont tableFont = ui_->tableViewMotorSt->font();
	auto makeEdit = [this, &tableFont](const QString &ph) {
		auto *e = new QLineEdit(ui_->tableViewMotorSt);
		e->setFont(tableFont);
		e->setPlaceholderText(ph);
		e->setAlignment(Qt::AlignRight);
		return e;
	};
	motorSetpointVelEdit_ = makeEdit(QStringLiteral("target velocity"));
	motorSetpointTrqEdit_ = makeEdit(QStringLiteral("target torque"));
	ui_->tableViewMotorSt->setIndexWidget(
			motorStModel_.index(kMotorSetpointRow, kColSetVel), motorSetpointVelEdit_);
	ui_->tableViewMotorSt->setIndexWidget(
			motorStModel_.index(kMotorSetpointRow, kColSetTrq), motorSetpointTrqEdit_);
}

void MainWindow::clearMotorStPollDataForRow(int row) {
	if (row < 0 || row >= kMotorSetpointRow) {
		return;
	}
	for (int c = 1; c < kMotorTableColumnCount; ++c) {
		if (isMotorSetpointColumn(c)) {
			continue;
		}
		if (QStandardItem *it = motorStModel_.item(row, c)) {
			if (c == kColWc) {
				it->setText(formatWcState(kDefaultWcState));
				it->setToolTip(QStringLiteral("raw=1 — Disconnected"));
			} else {
				it->setText(QStringLiteral("—"));
				it->setToolTip(QString());
			}
		}
	}
	lastMotorWc_[static_cast<std::size_t>(row)] = kDefaultWcState;
}

void MainWindow::clearMotorStPollDataOutsideScope() {
	const BodyScopeRange range = currentBodyScopeRange();
	for (int r = 0; r < kMotorSetpointRow; ++r) {
		if (!moduleInCurrentScope(r)) {
			clearMotorStPollDataForRow(r);
		}
	}
}

void MainWindow::resetMotorTableScroll() {
	if (QScrollBar *sb = ui_->tableViewMotorSt->verticalScrollBar()) {
		sb->setValue(0);
	}
}

void MainWindow::updateMotorTableDisplayIndices() {
	for (int r = 0; r < kMotorSetpointRow; ++r) {
		QStandardItem *idxItem = motorStModel_.item(r, kColIdx);
		if (!idxItem) {
			continue;
		}
		idxItem->setData(r, kMotorTableRoleRealModule);
		idxItem->setText(QString::number(r));
		idxItem->setToolTip(QString());
	}
}

void MainWindow::printBodyScopeDiagnostics() const {
	if (bodyScope_ == BodyScope::WholeBody || bodyScopeDiagPrinted_) {
		return;
	}
	bodyScopeDiagPrinted_ = true;
	const BodyScopeRange range = currentBodyScopeRange();
	const AdsBodyScopeProfile &profile = currentAdsProfile();
	const int lastLocalSlot = range.moduleCount - 1;
	const int lastRealModule = realModuleForLocalSlot(lastLocalSlot, bodyScope_);
	const std::size_t slot = static_cast<std::size_t>(lastLocalSlot);
	const char *label = bodyScopeLabel(bodyScope_);
	qDebug().nospace()
			<< label << " scope: firstRealModule=" << range.firstModule
			<< " lastRealModule=" << range.lastModule << " moduleCount=" << range.moduleCount
			<< " localSlot" << lastLocalSlot << "->M" << lastRealModule
			<< " DataMotorStLen=" << profile.dataMotorSt.byteLength
			<< " DataMotorCmdLen=" << profile.dataMotorCmd.byteLength
			<< " PathCmdLen=" << profile.pathCmd.byteLength
			<< " slot" << lastLocalSlot
			<< " StOff="
			<< (slot * (bodyScopeUsesCompactMotorSt(bodyScope_)
								 ? sizeof(UpperBodyMiniMotorStWire)
								 : kMotorStWireStride))
			<< " CmdOff=" << (slot * kMotorCmdWireStrideUpperBody)
			<< " PathOff=" << (profile.pathCmd.byteLength / qMax(1, range.moduleCount) * slot);
}

void MainWindow::printBodyScopePollDiagnostics(const std::uint8_t *stRaw, std::size_t stByteLen,
																							 const std::uint8_t *cmdRaw, std::size_t cmdByteLen,
																							 std::size_t cmdStride) const {
	if (bodyScope_ == BodyScope::WholeBody || bodyScopePollDiagPrinted_ || stRaw == nullptr) {
		return;
	}
	bodyScopePollDiagPrinted_ = true;
	const BodyScopeRange range = currentBodyScopeRange();
	const AdsBodyScopeProfile &profile = currentAdsProfile();
	const std::size_t lastSlot = static_cast<std::size_t>(range.moduleCount - 1);
	const int lastRealModule = realModuleForLocalSlot(static_cast<int>(lastSlot), bodyScope_);
	const bool stOk =
			(bodyScopeUsesCompactMotorSt(bodyScope_))
					? (lastSlot * sizeof(UpperBodyMiniMotorStWire) + sizeof(UpperBodyMiniMotorStWire) <=
						 stByteLen)
					: motorStWireFits(stByteLen, lastSlot);
	const bool cmdOk = cmdRaw != nullptr && motorCmdWireFits(cmdByteLen, lastSlot, cmdStride);
	const bool pathOk = pathParameterFits(profile.pathCmd.byteLength, lastSlot);
	const char *label = bodyScopeLabel(bodyScope_);
	qDebug().nospace()
			<< label << " poll slot" << lastSlot << ": stFits=" << stOk << " cmdFits=" << cmdOk
			<< " pathFits=" << pathOk << " stBytes=" << stByteLen << " cmdBytes=" << cmdByteLen;
	if (stOk) {
		if (bodyScopeUsesCompactMotorSt(bodyScope_)) {
			const auto &m =
					*reinterpret_cast<const UpperBodyMiniMotorStWire *>(
							stRaw + lastSlot * sizeof(UpperBodyMiniMotorStWire));
			qDebug().nospace() << label << " slot" << lastSlot << "->M" << lastRealModule
												 << " MotorSt: pos=" << m.nActualPosition
												 << " vel=" << m.nActualVelocity
												 << " mode=" << m.nModeOfOperationDisplay;
		} else {
			const MotorStWire &m = *motorStWireAt(stRaw, lastSlot);
			qDebug().nospace() << label << " slot" << lastSlot << "->M" << lastRealModule
												 << " MotorSt: pos=" << m.nActualPosition
												 << " vel=" << m.nActualVelocity
												 << " mode=" << m.nModeOfOperationDisplay
												 << " wc=" << static_cast<unsigned>(m.nWcState);
		}
	}
	if (cmdOk) {
		const MotorCmdWire &c = *motorCmdWireAt(cmdRaw, lastSlot, cmdStride);
		qDebug().nospace() << label << " slot" << lastSlot << "->M" << lastRealModule
											 << " MotorCmd: tgtPos=" << c.nTargetPosition
											 << " mode=" << static_cast<int>(c.nModeOfOperation);
	}
}

void MainWindow::clearMotorStTable() {
	lastMotorWc_.fill(kDefaultWcState);
	for (int r = 0; r < kMotorSetpointRow; ++r) {
		for (int c = 1; c < kMotorTableColumnCount; ++c) {
			if (QStandardItem *it = motorStModel_.item(r, c)) {
				if (c == kColWc) {
					it->setText(formatWcState(kDefaultWcState));
					it->setToolTip(QStringLiteral("raw=1 — Disconnected"));
				} else if (isMotorSetpointColumn(c)) {
					it->setText(QString());
					it->setToolTip(QString());
				} else {
					it->setText(QStringLiteral("—"));
					it->setToolTip(QString());
				}
			}
		}
	}
	applyMotorStWcRowFilter();
}

void MainWindow::setupPathGenerationUi() {
	ui_->dsbTotalTime->setRange(0.0, 1.0e9);
	ui_->dsbTotalTime->setDecimals(2);
	ui_->dsbTotalTime->setSingleStep(0.5);
	ui_->dsbTotalTime->setValue(1.0);

	ui_->dsbStepSize->setRange(0.0, 1.0e6);
	ui_->dsbStepSize->setDecimals(3);
	ui_->dsbStepSize->setSingleStep(0.001);
	ui_->dsbStepSize->setValue(0.001);

	ui_->dsbAccTime->setRange(0.0, 1.0e9);
	ui_->dsbAccTime->setDecimals(2);
	ui_->dsbAccTime->setSingleStep(0.1);
	ui_->dsbAccTime->setValue(0.1);

	ui_->cbIndex->clear();
	rebuildPathIndexCombo();
	ui_->cbIndex->setMaxVisibleItems(10);
	ui_->cbProfileMode->setCurrentIndex(1);

	connect(ui_->cbAll, &QCheckBox::toggled, this, [this](bool) {
		ui_->cbIndex->setEnabled(!ui_->cbAll->isChecked());
	});
	ui_->cbIndex->setEnabled(!ui_->cbAll->isChecked());

	connect(ui_->btnGenerate, &QPushButton::clicked, this, &MainWindow::sendPathGeneration);
}

void MainWindow::setupSystemControlUi() {
	connect(ui_->btnEnable, &QPushButton::clicked, this, [this]() {
		writeClientMainSubCmd(kCmdMainSystem, kCmdSubStart);
	});
	connect(ui_->btnSystemStop, &QPushButton::clicked, this, [this]() {
		writeClientMainSubCmd(kCmdMainSystem, kCmdSubStop);
	});
	connect(ui_->btnReset, &QPushButton::clicked, this, [this]() {
		writeClientMainSubCmd(kCmdMainSystem, kCmdSubReset);
	});
}

void MainWindow::setupModeOfOperationUi() {
	connect(ui_->btnCSP, &QPushButton::clicked, this, [this]() {
		writeClientMainSubCmd(kCmdMainMode, kCmdSubCsp);
	});
	connect(ui_->btnCSV, &QPushButton::clicked, this, [this]() {
		writeClientMainSubCmd(kCmdMainMode, kCmdSubCsv);
	});
	connect(ui_->btnCST, &QPushButton::clicked, this, [this]() {
		writeClientMainSubCmd(kCmdMainMode, kCmdSubCst);
	});
}

void MainWindow::setupJogModeUi() {
	ui_->jogTick->setText(QStringLiteral("3000"));
	connect(ui_->btnJogPlus, &QPushButton::clicked, this, [this]() {
		sendJogCommand(kCmdSubJogPlus);
	});
	connect(ui_->btnJogMinus, &QPushButton::clicked, this, [this]() {
		sendJogCommand(kCmdSubJogMinus);
	});
	connect(ui_->btnJogStop, &QPushButton::clicked, this, [this]() {
		sendJogCommand(kCmdSubJogStop);
	});
}

void MainWindow::setupSetPosShortcuts() {
	auto *f3 = new QShortcut(QKeySequence(Qt::Key_F3), this);
	f3->setContext(Qt::WindowShortcut);
	connect(f3, &QShortcut::activated, this, [this]() {
		for (int r = 0; r < kMotorSetpointRow; ++r) {
			if (!moduleInCurrentScope(r)) {
				continue;
			}
			if (QStandardItem *it = motorStModel_.item(r, kColCmdPos)) {
				it->setText(QString::number(kSetPosShortcutFillValue));
			}
		}
	});
	auto *f4 = new QShortcut(QKeySequence(Qt::Key_F4), this);
	f4->setContext(Qt::WindowShortcut);
	connect(f4, &QShortcut::activated, this, [this]() {
		for (int r = 0; r < kMotorSetpointRow; ++r) {
			if (!moduleInCurrentScope(r)) {
				continue;
			}
			if (QStandardItem *it = motorStModel_.item(r, kColCmdPos)) {
				it->setText(QStringLiteral("0"));
			}
		}
	});
	auto *f5 = new QShortcut(QKeySequence(Qt::Key_F5), this);
	f5->setContext(Qt::WindowShortcut);
	connect(f5, &QShortcut::activated, this, [this]() {
		const BodyScopeRange range = currentBodyScopeRange();
		QStringList lines;
		lines.reserve(range.moduleCount);
		for (int module = range.firstModule; module <= range.lastModule; ++module) {
			lines << QStringLiteral("M%1=%2")
									 .arg(module)
									 .arg(motorTablePosition(module));
		}
		qDebug().noquote() << QStringLiteral("Current positions [%1]: %2")
																.arg(QString::fromUtf8(bodyScopeLabel(bodyScope_)))
																.arg(lines.join(QStringLiteral(", ")));
		statusBar()->showMessage(
				QStringLiteral("Current positions printed to debug log (%1 motors).")
						.arg(range.moduleCount),
				3000);
	});

	const auto applySetPosPreset =
			[this](const std::vector<std::pair<int, std::int32_t>> &values,
						 const QString &label) {
				int applied = 0;
				for (const auto &entry : values) {
					const int row = entry.first;
					if (row < 0 || row >= kMotorSetpointRow) {
						continue;
					}
					if (QStandardItem *cell = motorStModel_.item(row, kColCmdPos)) {
						cell->setText(QString::number(entry.second));
						++applied;
					}
				}
				statusBar()->showMessage(
						QStringLiteral("%1 preset applied to %2 motors.").arg(label).arg(applied),
						3000);
			};

	auto *f6 = new QShortcut(QKeySequence(Qt::Key_F6), this);
	f6->setContext(Qt::WindowShortcut);
	connect(f6, &QShortcut::activated, this, [applySetPosPreset]() {
		const std::vector<std::pair<int, std::int32_t>> rightLegUp = {
				{16, 55172}, {17, -39042}, {18, -34460}, {19, -25848}, {20, 15697},
				{21, -28610}, {22, -3652}, {23, 50746},  {24, 50478},  {25, 17411},
				{26, 57095}, {27, -6641},  {28, -46502}, {29, 20351},  {30, 39921}};
		applySetPosPreset(rightLegUp, QStringLiteral("Right leg up"));
	});

	auto *f7 = new QShortcut(QKeySequence(Qt::Key_F7), this);
	f7->setContext(Qt::WindowShortcut);
	connect(f7, &QShortcut::activated, this, [applySetPosPreset]() {
		const std::vector<std::pair<int, std::int32_t>> leftLegUp = {
				{16, 55193}, {17, -38507}, {18, -48547}, {19, -68134}, {20, 6021},
				{21, -44554}, {22, -4071}, {23, 50764},  {24, 50455},  {25, -35181},
				{26, 50993}, {27, -27209}, {28, -60745}, {29, 20349},  {30, 39923}};
		applySetPosPreset(leftLegUp, QStringLiteral("Left leg up"));
	});

	auto *f9 = new QShortcut(QKeySequence(Qt::Key_F9), this);
	f9->setContext(Qt::WindowShortcut);
	connect(f9, &QShortcut::activated, this, [this]() {
		int applied = 0;
		for (int r = 0; r < kMotorSetpointRow; ++r) {
			if (!moduleInCurrentScope(r)) {
				continue;
			}
			if (QStandardItem *cell = motorStModel_.item(r, kColSetPos)) {
				cell->setText(QString::number(motorTablePosition(r)));
				++applied;
			}
		}
		statusBar()->showMessage(
				QStringLiteral("Current position copied to Save Pos for %1 motors.").arg(applied),
				3000);
	});
}

void MainWindow::setupLoggingUi() {
	connect(ui_->pushButton, &QPushButton::clicked, this, [this]() { startLogging(); });
	connect(ui_->pushButton_2, &QPushButton::clicked, this, [this]() { stopLogging(); });
}

void MainWindow::updateLoggingButtons() {
	const bool conn = device_ != nullptr;
	const bool supportsLogging = bodyScopeSupportsLogging(bodyScope_);
	ui_->pushButton->setEnabled(supportsLogging && conn && !loggingActive_);
	ui_->pushButton_2->setEnabled(supportsLogging && conn && loggingActive_);
	ui_->lineEdit->setEnabled(supportsLogging);
}

void MainWindow::startLogging() {
	if (loggingActive_) {
		statusBar()->showMessage(QStringLiteral("Logging is already running."), 3000);
		return;
	}
	if (!bodyScopeSupportsLogging(bodyScope_)) {
		QMessageBox::information(
				this, QStringLiteral("Logging"),
				QStringLiteral("Logging is not supported for %1.").arg(QString::fromUtf8(bodyScopeLabel(bodyScope_))));
		return;
	}
	if (!device_) {
		QMessageBox::warning(this, QStringLiteral("Logging"),
												 QStringLiteral("Connect to the PLC first."));
		return;
	}
	QDir root(QDir::current());
	const QString logDir = QString::fromUtf8(kLoggingDirName);
	if (!root.exists(logDir)) {
		if (!root.mkdir(logDir)) {
			QMessageBox::warning(this, QStringLiteral("Logging"),
													 QStringLiteral("Could not create folder: %1").arg(logDir));
			return;
		}
	}
	const QString sessionName =
			QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_hh-mm-ss"));
	const QString relSession = logDir + QDir::separator() + sessionName;
	if (!root.mkpath(relSession)) {
		QMessageBox::warning(this, QStringLiteral("Logging"),
												 QStringLiteral("Could not create folder:\n%1").arg(relSession));
		return;
	}
	const QString sessionPath = root.filePath(relSession);
	loggingSessionFolderPath_ = sessionPath;

	const auto abortSession = [this]() {
		for (auto &s : loggingCsvStreams_) {
			s.reset();
		}
		for (auto &f : loggingCsvFiles_) {
			if (f) {
				f->close();
			}
			f.reset();
		}
		loggingSessionFolderPath_.clear();
	};

	const QString header = QStringLiteral(
			"utc_ms,batch,bufferIndex,nStatusWord,nActualPosition,nActualVelocity,"
			"nActualTorque,nErrorCode,nModeOfOperationDisp,nWcState\n");

	for (std::size_t m = 0; m < kTwinCatLogMotorCount; ++m) {
		if (!moduleInCurrentScope(static_cast<int>(m))) {
			continue;
		}
		const QString motorPath =
				QDir(sessionPath).filePath(QStringLiteral("M%1.csv").arg(static_cast<int>(m)));
		auto file = std::make_unique<QFile>(motorPath);
		if (!file->open(QIODevice::WriteOnly | QIODevice::Text)) {
			abortSession();
			QMessageBox::warning(
					this, QStringLiteral("Logging"),
					QStringLiteral("Could not open file for writing:\n%1").arg(motorPath));
			return;
		}
		loggingCsvFiles_[m] = std::move(file);
		loggingCsvStreams_[m] = std::make_unique<QTextStream>(loggingCsvFiles_[m].get());
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
		loggingCsvStreams_[m]->setCodec("UTF-8");
#endif
		*loggingCsvStreams_[m] << header;
		loggingCsvStreams_[m]->flush();
	}

	bool ok = false;
	const double sec = ui_->lineEdit->text().trimmed().toDouble(&ok);
	if (ok && sec > 0.0) {
		loggingDurationSec_ = qMin(sec, kLoggingMaxDurationSec);
	} else {
		loggingDurationSec_ = kLoggingMaxDurationSec;
	}

	loggingBatchIndex_ = 0;
	loggingSessionElapsed_.start();
	loggingActive_ = true;
	loggingTimer_.start(kLoggingAppendIntervalMs);
	updateLoggingButtons();
	statusBar()->showMessage(
			QStringLiteral("Logging → %1 (M0–M30.csv) · auto-stop after %2 s (max %3 s; Stop ends "
										 "earlier)")
					.arg(sessionPath)
					.arg(loggingDurationSec_, 0, 'f', 1)
					.arg(kLoggingMaxDurationSec, 0, 'f', 0),
			8000);
}

QString MainWindow::stopLogging(bool silentStatus) {
	QString closedPath;
	if (!loggingActive_) {
		return closedPath;
	}
	loggingTimer_.stop();
	for (auto &s : loggingCsvStreams_) {
		s.reset();
	}
	closedPath = loggingSessionFolderPath_;
	for (auto &f : loggingCsvFiles_) {
		if (f) {
			f->close();
		}
		f.reset();
	}
	loggingSessionFolderPath_.clear();
	loggingActive_ = false;
	loggingDurationSec_ = 0.0;
	updateLoggingButtons();
	if (!closedPath.isEmpty() && !silentStatus) {
		statusBar()->showMessage(QStringLiteral("Logging stopped: %1").arg(closedPath), 6000);
	}
	return closedPath;
}

void MainWindow::appendLoggingSnapshot() {
	if (!loggingActive_ || !device_) {
		return;
	}
	bool streamsOk = true;
	for (std::size_t m = 0; m < kTwinCatLogMotorCount; ++m) {
		if (!moduleInCurrentScope(static_cast<int>(m))) {
			continue;
		}
		if (!loggingCsvStreams_[m]) {
			streamsOk = false;
			break;
		}
	}
	if (!streamsOk) {
		return;
	}
	if (loggingDurationSec_ > 0.0) {
		const qint64 limitMs =
				static_cast<qint64>(loggingDurationSec_ * 1000.0 + 0.5);
		if (loggingSessionElapsed_.elapsed() >= limitMs) {
			const QString path = stopLogging(true);
			statusBar()->showMessage(
					path.isEmpty()
							? QStringLiteral("Logging stopped (time limit reached).")
							: QStringLiteral("Logging stopped (time limit): %1").arg(path),
					6000);
			return;
		}
	}

	const AdsBodyScopeProfile &profile = currentAdsProfile();
	const qint64 utcMs = QDateTime::currentMSecsSinceEpoch();
	const qint64 batch = loggingBatchIndex_++;

	if (profile.loggingBuffer.byteLength == sizeof(TwinCatLoggingBufferData)) {
		std::array<std::uint8_t, sizeof(TwinCatLoggingBufferData)> raw{};
		std::uint32_t bytesRead = 0;
		const long err =
				device_->ReadReqEx2(profile.loggingBuffer.indexGroup, profile.loggingBuffer.indexOffset,
														raw.size(), raw.data(), &bytesRead);
		if (err != 0 || bytesRead != raw.size()) {
			QMessageBox::warning(this, QStringLiteral("Logging"),
													 QStringLiteral("ADS read failed while logging; logging stopped."));
			stopLogging(false);
			return;
		}
		TwinCatLoggingBufferData data{};
		std::memcpy(&data, raw.data(), sizeof(data));

		int startIdx = 0;
		int endIdx = 99;
		if (data.flag == 0) {
			startIdx = 100;
			endIdx = 199;
		}

		const BodyScopeRange range = currentBodyScopeRange();
		for (int mod = range.firstModule; mod <= range.lastModule; ++mod) {
			QTextStream *out = loggingCsvStreams_[static_cast<std::size_t>(mod)].get();
			if (!out) {
				continue;
			}
			for (int idx = startIdx; idx <= endIdx; ++idx) {
				const LogMotorStSample &s =
						data.motorStBuffer[static_cast<std::size_t>(idx)][static_cast<std::size_t>(mod)];
				*out << utcMs << ',' << batch << ',' << idx << ',' << s.nStatusWord << ','
						 << s.nActualPosition << ',' << s.nActualVelocity << ',' << s.nActualTorque << ','
						 << s.nErrorCode << ',' << static_cast<int>(s.nModeOfOperationDisplay) << ','
						 << static_cast<unsigned>(s.nWcState) << '\n';
			}
			out->flush();
		}
		return;
	}

	if (profile.loggingBuffer.byteLength == kUpperBodyLoggingBufferByteLen) {
		std::vector<std::uint8_t> raw(profile.loggingBuffer.byteLength, 0);
		std::uint32_t bytesRead = 0;
		const long err =
				device_->ReadReqEx2(profile.loggingBuffer.indexGroup, profile.loggingBuffer.indexOffset,
														raw.size(), raw.data(), &bytesRead);
		if (err != 0 || bytesRead != raw.size()) {
			QMessageBox::warning(this, QStringLiteral("Logging"),
													 QStringLiteral("ADS read failed while logging; logging stopped."));
			stopLogging(false);
			return;
		}

		int startIdx = 0;
		int endIdx = 99;
		if (raw[0] == 0) {
			startIdx = 100;
			endIdx = 199;
		}

		const BodyScopeRange range = currentBodyScopeRange();
		for (int mod = range.firstModule; mod <= range.lastModule; ++mod) {
			const std::size_t localMod =
					static_cast<std::size_t>(localSlotForModule(mod, bodyScope_));
			if (localMod >= kUpperBodyLogMotorCount) {
				continue;
			}
			QTextStream *out = loggingCsvStreams_[static_cast<std::size_t>(mod)].get();
			if (!out) {
				continue;
			}
			for (int idx = startIdx; idx <= endIdx; ++idx) {
				const MotorStWire *sample =
						upperBodyLogMotorStAt(raw.data(), static_cast<std::size_t>(idx), localMod);
				*out << utcMs << ',' << batch << ',' << idx << ',' << sample->nStatusWord << ','
						 << sample->nActualPosition << ',' << sample->nActualVelocity << ','
						 << sample->nActualTorque << ',' << sample->nErrorCode << ','
						 << static_cast<int>(sample->nModeOfOperationDisplay) << ','
						 << static_cast<unsigned>(sample->nWcState) << '\n';
			}
			out->flush();
		}
		return;
	}

	if (profile.loggingBuffer.byteLength == kLowerBodyLoggingBufferByteLen) {
		std::vector<std::uint8_t> raw(profile.loggingBuffer.byteLength, 0);
		std::uint32_t bytesRead = 0;
		const long err =
				device_->ReadReqEx2(profile.loggingBuffer.indexGroup, profile.loggingBuffer.indexOffset,
														raw.size(), raw.data(), &bytesRead);
		if (err != 0 || bytesRead != raw.size()) {
			QMessageBox::warning(this, QStringLiteral("Logging"),
													 QStringLiteral("ADS read failed while logging; logging stopped."));
			stopLogging(false);
			return;
		}

		int startIdx = 0;
		int endIdx = 99;
		if (raw[0] == 0) {
			startIdx = 100;
			endIdx = 199;
		}

		const BodyScopeRange range = currentBodyScopeRange();
		for (int mod = range.firstModule; mod <= range.lastModule; ++mod) {
			const std::size_t localMod =
					static_cast<std::size_t>(localSlotForModule(mod, bodyScope_));
			if (localMod >= kLowerBodyLogMotorCount) {
				continue;
			}
			QTextStream *out = loggingCsvStreams_[static_cast<std::size_t>(mod)].get();
			if (!out) {
				continue;
			}
			for (int idx = startIdx; idx <= endIdx; ++idx) {
				const MotorStWire *sample = lowerBodyLogMotorStAt(raw.data(), static_cast<std::size_t>(idx),
																													localMod);
				*out << utcMs << ',' << batch << ',' << idx << ',' << sample->nStatusWord << ','
						 << sample->nActualPosition << ',' << sample->nActualVelocity << ','
						 << sample->nActualTorque << ',' << sample->nErrorCode << ','
						 << static_cast<int>(sample->nModeOfOperationDisplay) << ','
						 << static_cast<unsigned>(sample->nWcState) << '\n';
			}
			out->flush();
		}
		return;
	}
}

void MainWindow::setupOperatingUi() {
	ui_->progressBarPath->setRange(0, 100);
	ui_->progressBarPath->setValue(0);
	ui_->labelPathProgressDetail->setText(QStringLiteral("—"));

	const auto onPathMotionStart = [this]() {
		if (!device_) {
			return;
		}
		startPathMotionTracking();
	};
	const auto onPathMotionStop = [this]() {
		if (!device_) {
			return;
		}
		stopPathMotionTracking();
	};

	connect(ui_->pushRun, &QPushButton::clicked, this, [this, onPathMotionStart]() {
		if (!device_) {
			return;
		}
		sendPathGeneration();
		onPathMotionStart();
		writeClientMainSubCmd(kCmdMainPath, kCmdSubPathOpRun);
	});
	connect(ui_->btnStop, &QPushButton::clicked, this, [this, onPathMotionStop]() {
		if (!device_) {
			return;
		}
		writeClientMainSubCmd(kCmdMainPath, kCmdSubPathOpStop);
		onPathMotionStop();
	});
	connect(ui_->btnRepeat, &QPushButton::clicked, this, [this, onPathMotionStart]() {
		if (!device_) {
			return;
		}
		onPathMotionStart();
		writeClientMainSubCmd(kCmdMainPath, kCmdSubPathOpRepeat);
	});
	connect(ui_->btnCyclic, &QPushButton::clicked, this, [this, onPathMotionStart]() {
		if (!device_) {
			return;
		}
		onPathMotionStart();
		writeClientMainSubCmd(kCmdMainPath, kCmdSubPathOpCyclic);
	});

	auto *operatingLayout = ui_->groupBox_5->findChild<QGridLayout *>(QStringLiteral("gridLayout_6"));
	if (operatingLayout) {
		auto *btnZero = ui_->groupBox_5->findChild<QPushButton *>(QStringLiteral("btnZero"));
		if (!btnZero) {
			btnZero = new QPushButton(QStringLiteral("Zero"), ui_->groupBox_5);
			btnZero->setObjectName(QStringLiteral("btnZero"));
			btnZero->setEnabled(false);
			operatingLayout->addWidget(btnZero, 0, 2);
		}
		auto *btnSaveRun =
				ui_->groupBox_5->findChild<QPushButton *>(QStringLiteral("btnSaveRun"));
		if (!btnSaveRun) {
			btnSaveRun = new QPushButton(QStringLiteral("Save Run"), ui_->groupBox_5);
			btnSaveRun->setObjectName(QStringLiteral("btnSaveRun"));
			btnSaveRun->setEnabled(false);
			operatingLayout->addWidget(btnSaveRun, 1, 2);
		}
		connect(btnZero, &QPushButton::clicked, this, [this, onPathMotionStart]() {
			if (!device_) {
				return;
			}
			const BodyScopeRange range = currentBodyScopeRange();
			for (int r = range.firstModule; r <= range.lastModule; ++r) {
				if (QStandardItem *cell = motorStModel_.item(r, kColCmdPos)) {
					cell->setText(QStringLiteral("0"));
				}
			}
			ui_->cbAll->setChecked(true);
			sendPathGeneration();
			onPathMotionStart();
			writeClientMainSubCmd(kCmdMainPath, kCmdSubPathOpRun);
		});
		connect(btnSaveRun, &QPushButton::clicked, this, [this, onPathMotionStart]() {
			if (!device_) {
				return;
			}
			const BodyScopeRange range = currentBodyScopeRange();
			for (int r = range.firstModule; r <= range.lastModule; ++r) {
				QStandardItem *savePos = motorStModel_.item(r, kColSetPos);
				QStandardItem *cmdPos = motorStModel_.item(r, kColCmdPos);
				if (!savePos || !cmdPos) {
					continue;
				}
				cmdPos->setText(savePos->text());
			}
			ui_->cbAll->setChecked(true);
			sendPathGeneration();
			onPathMotionStart();
			writeClientMainSubCmd(kCmdMainPath, kCmdSubPathOpRun);
		});
	}
}

int MainWindow::pathProgressPercent(std::int32_t start, std::int32_t target,
																		std::int32_t actual) {
	const double total = static_cast<double>(target) - static_cast<double>(start);
	if (std::abs(total) < 1.0) {
		return (actual == target) ? 100 : 0;
	}
	const double ratio =
			(static_cast<double>(actual) - static_cast<double>(start)) / total * 100.0;
	return qBound(0, static_cast<int>(std::lround(ratio)), 100);
}

std::int32_t MainWindow::motorTablePosition(int row) const {
	if (row < 0 || row >= kMotorSetpointRow) {
		return 0;
	}
	QStandardItem *posIt = motorStModel_.item(row, kColPos);
	const QString ptxt = posIt ? posIt->text().trimmed() : QString();
	if (ptxt.isEmpty() || ptxt == QStringLiteral("—")) {
		return 0;
	}
	bool ok = false;
	const qint64 v = ptxt.toLongLong(&ok);
	if (!ok || v > std::numeric_limits<std::int32_t>::max() ||
			v < std::numeric_limits<std::int32_t>::min()) {
		return 0;
	}
	return static_cast<std::int32_t>(v);
}

std::int32_t MainWindow::motorTableSetPosition(int row) const {
	if (row < 0 || row >= kMotorSetpointRow) {
		return 0;
	}
	QStandardItem *sp = motorStModel_.item(row, kColCmdPos);
	return parseSetPositionCell(sp ? sp->text() : QString(), motorTablePosition(row));
}

bool MainWindow::readPathParametersFromPlc(std::vector<PathParameter> *params) const {
	if (!device_ || !params) {
		return false;
	}
	const AdsBodyScopeProfile &profile = currentAdsProfile();
	if (profile.pathCmd.byteLength == 0 ||
			profile.pathCmd.byteLength % sizeof(PathParameter) != 0) {
		return false;
	}
	std::vector<std::uint8_t> raw(profile.pathCmd.byteLength, 0);
	std::uint32_t bytesRead = 0;
	const long err =
			device_->ReadReqEx2(profile.pathCmd.indexGroup, profile.pathCmd.indexOffset, raw.size(),
													raw.data(), &bytesRead);
	if (err != 0 || bytesRead != raw.size()) {
		return false;
	}
	const std::size_t count = pathParameterCount(raw.size());
	params->resize(count);
	std::memcpy(params->data(), raw.data(), count * sizeof(PathParameter));
	return true;
}

void MainWindow::startPathMotionTracking() {
	pathTracking_.fill(false);
	pathStartPos_.fill(0);
	pathTargetPos_.fill(0);

	const BodyScopeRange range = currentBodyScopeRange();
	std::vector<PathParameter> pathParams;
	const bool havePathParams = readPathParametersFromPlc(&pathParams);

	const auto trackModule = [this](int moduleId, std::int32_t start, std::int32_t target) {
		if (!moduleInCurrentScope(moduleId)) {
			return;
		}
		pathTracking_[static_cast<std::size_t>(moduleId)] = true;
		pathStartPos_[static_cast<std::size_t>(moduleId)] = start;
		pathTargetPos_[static_cast<std::size_t>(moduleId)] = target;
	};

	if (havePathParams) {
		for (int i = range.firstModule; i <= range.lastModule; ++i) {
			int localSlot = 0;
			if (!localSlotForModuleChecked(i, bodyScope_, &localSlot)) {
				continue;
			}
			const std::size_t slot = static_cast<std::size_t>(localSlot);
			if (slot >= pathParams.size() || pathParams[slot].nUpdate == 0) {
				continue;
			}
			trackModule(i, motorTablePosition(i), pathParams[slot].nSetPosition);
		}
	}

	bool anyTracked = false;
	for (std::size_t i = 0; i < pathTracking_.size(); ++i) {
		if (pathTracking_[i]) {
			anyTracked = true;
			break;
		}
	}

	if (!anyTracked) {
		if (ui_->cbAll->isChecked()) {
			for (int i = range.firstModule; i <= range.lastModule; ++i) {
				trackModule(i, motorTablePosition(i), motorTableSetPosition(i));
			}
		} else {
			const int onlyIdx = resolvedPathMotorIndex();
			if (onlyIdx >= 0 && moduleInCurrentScope(onlyIdx)) {
				trackModule(onlyIdx, motorTablePosition(onlyIdx), motorTableSetPosition(onlyIdx));
			}
		}
	}

	anyTracked = false;
	for (std::size_t i = 0; i < pathTracking_.size(); ++i) {
		if (pathTracking_[i]) {
			anyTracked = true;
			break;
		}
	}
	pathMotionActive_ = anyTracked;
	updatePathProgressDisplay();
}

void MainWindow::stopPathMotionTracking() {
	pathMotionActive_ = false;
	pathTracking_.fill(false);
	ui_->progressBarPath->setValue(0);
	ui_->labelPathProgressDetail->setText(QStringLiteral("—"));
}

void MainWindow::updatePathProgressDisplay() {
	if (!pathMotionActive_) {
		return;
	}

	int trackedCount = 0;
	int progressSum = 0;
	int minModule = -1;
	int maxModule = -1;
	int minProgress = 100;

	for (int i = 0; i < kMotorSetpointRow; ++i) {
		if (!pathTracking_[static_cast<std::size_t>(i)]) {
			continue;
		}
		const std::int32_t actual = motorTablePosition(i);
		const int pct = pathProgressPercent(pathStartPos_[static_cast<std::size_t>(i)],
																				pathTargetPos_[static_cast<std::size_t>(i)], actual);
		++trackedCount;
		progressSum += pct;
		minProgress = qMin(minProgress, pct);
		if (minModule < 0) {
			minModule = i;
			maxModule = i;
		} else {
			minModule = qMin(minModule, i);
			maxModule = qMax(maxModule, i);
		}
	}

	if (trackedCount == 0) {
		stopPathMotionTracking();
		return;
	}

	const int avgProgress = progressSum / trackedCount;
	ui_->progressBarPath->setValue(avgProgress);

	if (trackedCount == 1) {
		ui_->labelPathProgressDetail->setText(
				QStringLiteral("M%1 · %2%").arg(minModule).arg(avgProgress));
	} else {
		ui_->labelPathProgressDetail->setText(
				QStringLiteral("M%1–M%2 · avg %3% (min %4%)")
						.arg(minModule)
						.arg(maxModule)
						.arg(avgProgress)
						.arg(minProgress));
	}
}

void MainWindow::writeClientMainSubCmd(uint16_t mainCmd, uint16_t subCmd) {
	if (!device_) {
		return;
	}
	const AdsBodyScopeProfile &profile = currentAdsProfile();
	const long errMain =
			device_->WriteReqEx(profile.mainCmd.indexGroup, profile.mainCmd.indexOffset,
													profile.mainCmd.byteLength, &mainCmd);
	const long errSub =
			device_->WriteReqEx(profile.subCmd.indexGroup, profile.subCmd.indexOffset,
													profile.subCmd.byteLength, &subCmd);
	const long err = (errMain != 0) ? errMain : errSub;
	if (err != 0) {
		statusBar()->showMessage(QStringLiteral("MainCmd/SubCmd write failed: ADS %1 (Main=%2 Sub=%3)")
																 .arg(err)
																 .arg(mainCmd)
																 .arg(subCmd),
														 6000);
	}
}

void MainWindow::sendJogCommand(std::uint16_t subCmd) {
	if (!device_) {
		return;
	}
	bool ok = false;
	const std::uint16_t id =
			static_cast<std::uint16_t>(ui_->JogID->currentText().trimmed().toUShort(&ok));
	if (!ok) {
		statusBar()->showMessage(QStringLiteral("Invalid jog ID."), 3000);
		return;
	}
	const std::uint16_t tick =
			static_cast<std::uint16_t>(ui_->jogTick->text().trimmed().toUShort(&ok));
	if (!ok) {
		statusBar()->showMessage(QStringLiteral("Invalid jog tick value."), 3000);
		return;
	}

	ClientJogPayload payload{};
	payload.id = id;
	payload.tick = tick;

	writeClientMainSubCmd(kCmdMainJog, subCmd);
	const AdsBodyScopeProfile &profile = currentAdsProfile();
	const long err = device_->WriteReqEx(profile.jogCmd.indexGroup, profile.jogCmd.indexOffset,
																			 profile.jogCmd.byteLength, &payload);
	if (err != 0) {
		statusBar()->showMessage(
				QStringLiteral("Jog payload write failed: ADS %1 (Main=%2 Sub=%3, ID=%4 Tick=%5)")
						.arg(err)
						.arg(kCmdMainJog)
						.arg(subCmd)
						.arg(id)
						.arg(tick),
				6000);
		return;
	}
	statusBar()->showMessage(QStringLiteral("Jog sent — Main=%1 Sub=%2, ID=%3, Tick=%4")
													 .arg(kCmdMainJog)
													 .arg(subCmd)
													 .arg(id)
													 .arg(tick),
													 3000);
}

std::uint16_t MainWindow::pathProfileSubCmdFromComboIndex(int comboIndex) {
	if (comboIndex == 1) {
		return kCmdSubPathSCurve;
	}
	return kCmdSubPathTrapezoidal;
}

int MainWindow::resolvedPathMotorIndex() const {
	bool ok = false;
	const int fromData = ui_->cbIndex->currentData(Qt::UserRole).toInt(&ok);
	if (ok && moduleInCurrentScope(fromData)) {
		return fromData;
	}
	if (bodyScope_ == BodyScope::LowerBody || bodyScope_ == BodyScope::UpperBodyMini ||
			bodyScope_ == BodyScope::Module) {
		const int displayIdx = ui_->cbIndex->currentText().trimmed().toInt(&ok);
		if (ok) {
			const BodyScopeRange range = currentBodyScopeRange();
			if (displayIdx >= 0 && displayIdx < range.moduleCount) {
				const int realModule = realModuleFromDisplayIndex(displayIdx, bodyScope_);
				if (moduleInCurrentScope(realModule)) {
					return realModule;
				}
			}
		}
		return -1;
	}
	const int fromText = ui_->cbIndex->currentText().trimmed().toInt(&ok);
	if (ok && moduleInCurrentScope(fromText)) {
		return fromText;
	}
	return -1;
}

std::int32_t MainWindow::parseSetPositionCell(const QString &text,
																							std::int32_t fallbackActual) {
	const QString t = text.trimmed();
	if (t.isEmpty()) {
		return fallbackActual;
	}
	bool ok = false;
	const qint64 v = t.toLongLong(&ok);
	if (!ok) {
		return fallbackActual;
	}
	constexpr qint64 hi = std::numeric_limits<std::int32_t>::max();
	constexpr qint64 lo = std::numeric_limits<std::int32_t>::min();
	if (v > hi || v < lo) {
		return fallbackActual;
	}
	return static_cast<std::int32_t>(v);
}

bool MainWindow::readDataMotorStRaw(std::vector<std::uint8_t> *out) const {
	if (!device_ || !out) {
		return false;
	}
	const AdsBodyScopeProfile &profile = currentAdsProfile();
	out->assign(profile.dataMotorSt.byteLength, 0);
	std::uint32_t bytesRead = 0;
	const long err =
			device_->ReadReqEx2(profile.dataMotorSt.indexGroup, profile.dataMotorSt.indexOffset,
													out->size(), out->data(), &bytesRead);
	return err == 0 && bytesRead == profile.dataMotorSt.byteLength;
}

bool MainWindow::readDataMotorCmdRaw(std::vector<std::uint8_t> *out) const {
	if (!device_ || !out) {
		return false;
	}
	const AdsBodyScopeProfile &profile = currentAdsProfile();
	out->assign(profile.dataMotorCmd.byteLength, 0);
	std::uint32_t bytesRead = 0;
	const long err =
			device_->ReadReqEx2(profile.dataMotorCmd.indexGroup, profile.dataMotorCmd.indexOffset,
													out->size(), out->data(), &bytesRead);
	return err == 0 && bytesRead == profile.dataMotorCmd.byteLength;
}

bool MainWindow::readMotorActualPositions(
		std::array<std::int32_t, kTwinCatPathCmdCount> *out) const {
	if (!device_ || !out) {
		return false;
	}
	std::vector<std::uint8_t> raw;
	if (!readDataMotorStRaw(&raw)) {
		return false;
	}
	const BodyScopeRange range = currentBodyScopeRange();
	for (int i = range.firstModule; i <= range.lastModule; ++i) {
		int localSlot = 0;
		if (!localSlotForModuleChecked(i, bodyScope_, &localSlot)) {
			continue;
		}
		const std::size_t slot = static_cast<std::size_t>(localSlot);
		if (bodyScopeUsesCompactMotorSt(bodyScope_)) {
			const std::size_t byteOffset = slot * sizeof(UpperBodyMiniMotorStWire);
			if (byteOffset + sizeof(UpperBodyMiniMotorStWire) <= raw.size()) {
				const auto *m =
						reinterpret_cast<const UpperBodyMiniMotorStWire *>(raw.data() + byteOffset);
				(*out)[static_cast<std::size_t>(i)] = m->nActualPosition;
			}
			continue;
		}
		if (motorStWireFits(raw.size(), slot)) {
			(*out)[static_cast<std::size_t>(i)] = motorStWireAt(raw.data(), slot)->nActualPosition;
		}
	}
	return true;
}

void MainWindow::sendPathGeneration() {
	if (!device_) {
		return;
	}
	const bool all = ui_->cbAll->isChecked();
	int onlyIdx = -1;
	if (!all) {
		onlyIdx = resolvedPathMotorIndex();
		if (onlyIdx < 0) {
			const BodyScopeRange range = currentBodyScopeRange();
			const QString indexHint =
					(bodyScope_ == BodyScope::LowerBody || bodyScope_ == BodyScope::UpperBodyMini ||
					 bodyScope_ == BodyScope::Module)
							? QStringLiteral("0–%1 (M%2–M%3)")
										.arg(range.moduleCount - 1)
										.arg(range.firstModule)
										.arg(range.lastModule)
							: QStringLiteral("M%1–M%2").arg(range.firstModule).arg(range.lastModule);
			QMessageBox::warning(
					this, QStringLiteral("Path generation"),
					QStringLiteral("Select a valid module index (%1), or enable All.").arg(indexHint));
			return;
		}
		if (!moduleInCurrentScope(onlyIdx)) {
			const BodyScopeRange range = currentBodyScopeRange();
			QMessageBox::warning(
					this, QStringLiteral("Path generation"),
					QStringLiteral("Module M%1 is outside the current body scope (M%2–M%3).")
							.arg(onlyIdx)
							.arg(range.firstModule)
							.arg(range.lastModule));
			return;
		}
	}

	const std::uint16_t subCmd =
			pathProfileSubCmdFromComboIndex(ui_->cbProfileMode->currentIndex());
	const std::uint16_t profileMode = subCmd;

	std::array<std::int32_t, kTwinCatPathCmdCount> actualPos{};
	if (!readMotorActualPositions(&actualPos)) {
		for (std::size_t i = 0; i < kTwinCatPathCmdCount; ++i) {
			QStandardItem *posIt = motorStModel_.item(static_cast<int>(i), kColPos);
			QString ptxt = posIt ? posIt->text().trimmed() : QString();
			if (ptxt == QStringLiteral("—")) {
				ptxt.clear();
			}
			bool ok = false;
			const qint64 v = ptxt.toLongLong(&ok);
			if (ok && v <= std::numeric_limits<std::int32_t>::max() &&
					v >= std::numeric_limits<std::int32_t>::min()) {
				actualPos[i] = static_cast<std::int32_t>(v);
			} else {
				actualPos[i] = 0;
			}
		}
	}

	auto nSetPositionForRow = [this, &actualPos](int row) -> std::int32_t {
		QStandardItem *sp = motorStModel_.item(row, kColCmdPos);
		return parseSetPositionCell(sp ? sp->text() : QString(),
																actualPos[static_cast<std::size_t>(row)]);
	};

	const AdsBodyScopeProfile &adsProfile = currentAdsProfile();
	const double totalTime = ui_->dsbTotalTime->value();
	const double stepSize = ui_->dsbStepSize->value();
	const double accTime = ui_->dsbAccTime->value();

	auto fillPathParameter = [&](PathParameter *p, int row) {
		p->nTotalTime = totalTime;
		p->nStepSize = stepSize;
		p->nAccTime = accTime;
		p->nProfileMode = profileMode;
		p->nUpdate = 1;
		p->nSetPosition = nSetPositionForRow(row);
	};

	auto fillUpperBodyMiniPathParameter = [&](UpperBodyMiniPathParameter *p, int row) {
		p->nTotalTime = totalTime;
		p->nStepSize = stepSize;
		p->nUpdate = 1u;
		p->nSetPosition = nSetPositionForRow(row);
	};

	if (bodyScopeUsesCompactMotorSt(bodyScope_)) {
		const std::size_t pathCmdLen = adsProfile.pathCmd.byteLength;
		std::vector<std::uint8_t> pathCmdRaw(pathCmdLen);
		std::uint32_t bytesRead = 0;
		const long readErr =
				device_->ReadReqEx2(adsProfile.pathCmd.indexGroup, adsProfile.pathCmd.indexOffset,
														pathCmdRaw.size(), pathCmdRaw.data(), &bytesRead);
		if (readErr != 0 || bytesRead != pathCmdLen) {
			QMessageBox::warning(
					this, QStringLiteral("Path generation"),
					QStringLiteral("Could not read current PathCmd from PLC (ADS %1, %2 / %3 B).")
							.arg(readErr)
							.arg(bytesRead)
							.arg(static_cast<int>(pathCmdLen)));
			return;
		}
		const BodyScopeRange range = currentBodyScopeRange();
		if (!all) {
			int localSlot = 0;
			if (!localSlotForModuleChecked(onlyIdx, bodyScope_, &localSlot)) {
				QMessageBox::warning(this, QStringLiteral("Path generation"),
													 QStringLiteral("Module M%1 is outside the current body scope.")
															 .arg(onlyIdx));
				return;
			}
			const std::size_t byteOffset =
					static_cast<std::size_t>(localSlot) * sizeof(UpperBodyMiniPathParameter);
			if (byteOffset + sizeof(UpperBodyMiniPathParameter) > pathCmdRaw.size()) {
				QMessageBox::warning(this, QStringLiteral("Path generation"),
													 QStringLiteral("PathCmd slot out of range for M%1.")
															 .arg(onlyIdx));
				return;
			}
			auto *one = reinterpret_cast<UpperBodyMiniPathParameter *>(pathCmdRaw.data() + byteOffset);
			fillUpperBodyMiniPathParameter(one, onlyIdx);
		} else {
			for (int i = range.firstModule; i <= range.lastModule; ++i) {
				int localSlot = 0;
				if (!localSlotForModuleChecked(i, bodyScope_, &localSlot)) {
					continue;
				}
				const std::size_t byteOffset =
						static_cast<std::size_t>(localSlot) * sizeof(UpperBodyMiniPathParameter);
				if (byteOffset + sizeof(UpperBodyMiniPathParameter) > pathCmdRaw.size()) {
					continue;
				}
				auto *one =
						reinterpret_cast<UpperBodyMiniPathParameter *>(pathCmdRaw.data() + byteOffset);
				fillUpperBodyMiniPathParameter(one, i);
			}
		}
		writeClientMainSubCmd(kCmdMainPath, subCmd);
		const long writeErr =
				device_->WriteReqEx(adsProfile.pathCmd.indexGroup, adsProfile.pathCmd.indexOffset,
														static_cast<std::uint32_t>(pathCmdLen), pathCmdRaw.data());
		if (writeErr != 0) {
			statusBar()->showMessage(
					QStringLiteral("PathCmd write failed: ADS %1 (Main=%2 Sub=%3, %4 B)")
							.arg(writeErr)
							.arg(kCmdMainPath)
							.arg(subCmd)
							.arg(static_cast<int>(pathCmdLen)),
					8000);
			return;
		}
		statusBar()->showMessage(
				QStringLiteral("PathCmd sent — Main=%1 Sub=%2 (%3), %4 B")
						.arg(kCmdMainPath)
						.arg(subCmd)
						.arg(all ? QStringLiteral("index 0–%1 (M%2–M%3) updated, rest from PLC")
														 .arg(range.moduleCount - 1)
														 .arg(range.firstModule)
														 .arg(range.lastModule)
											 : QStringLiteral("index %1 (M%2) updated, rest from PLC")
														 .arg(displayIndexForRealModule(onlyIdx, bodyScope_))
														 .arg(onlyIdx))
						.arg(static_cast<int>(pathCmdLen)),
				5000);
		return;
	}

	if (isScopedPathCmdByteLength(adsProfile.pathCmd.byteLength)) {
		const std::size_t pathCmdLen = adsProfile.pathCmd.byteLength;
		std::vector<std::uint8_t> pathCmdRaw(pathCmdLen);
		std::uint32_t bytesRead = 0;
		const long readErr =
				device_->ReadReqEx2(adsProfile.pathCmd.indexGroup, adsProfile.pathCmd.indexOffset,
														pathCmdRaw.size(), pathCmdRaw.data(), &bytesRead);
		if (readErr != 0 || bytesRead != pathCmdLen) {
			QMessageBox::warning(
					this, QStringLiteral("Path generation"),
					QStringLiteral("Could not read current PathCmd from PLC (ADS %1, %2 / %3 B).")
							.arg(readErr)
							.arg(bytesRead)
							.arg(static_cast<int>(pathCmdLen)));
			return;
		}

		auto *pathParams = pathParameterAt(pathCmdRaw.data(), 0);
		const BodyScopeRange range = currentBodyScopeRange();

		if (!all) {
			int localSlot = 0;
			if (!localSlotForModuleChecked(onlyIdx, bodyScope_, &localSlot) ||
					!pathParameterFits(pathCmdLen, static_cast<std::size_t>(localSlot))) {
				QMessageBox::warning(this, QStringLiteral("Path generation"),
													 QStringLiteral("Module M%1 is outside the current body scope.")
															 .arg(onlyIdx));
				return;
			}
			fillPathParameter(&pathParams[localSlot], onlyIdx);
		} else {
			for (int i = range.firstModule; i <= range.lastModule; ++i) {
				int localSlot = 0;
				if (!localSlotForModuleChecked(i, bodyScope_, &localSlot) ||
						!pathParameterFits(pathCmdLen, static_cast<std::size_t>(localSlot))) {
					continue;
				}
				fillPathParameter(&pathParams[localSlot], i);
			}
		}

		writeClientMainSubCmd(kCmdMainPath, subCmd);
		const long writeErr =
				device_->WriteReqEx(adsProfile.pathCmd.indexGroup, adsProfile.pathCmd.indexOffset,
														static_cast<std::uint32_t>(pathCmdLen), pathCmdRaw.data());
		if (writeErr != 0) {
			statusBar()->showMessage(
					QStringLiteral("PathCmd write failed: ADS %1 (Main=%2 Sub=%3, %4 B)")
							.arg(writeErr)
							.arg(kCmdMainPath)
							.arg(subCmd)
							.arg(static_cast<int>(pathCmdLen)),
					8000);
			return;
		}
		statusBar()->showMessage(
				QStringLiteral("PathCmd sent — Main=%1 Sub=%2 (%3), %4 B")
						.arg(kCmdMainPath)
						.arg(subCmd)
						.arg(all ? QStringLiteral("index 0–%1 (M%2–M%3) updated, rest from PLC")
														 .arg(range.moduleCount - 1)
														 .arg(range.firstModule)
														 .arg(range.lastModule)
											 : ((bodyScope_ == BodyScope::LowerBody || bodyScope_ == BodyScope::UpperBodyMini ||
														bodyScope_ == BodyScope::Module)
															? QStringLiteral("index %1 (M%2) updated, rest from PLC")
																		.arg(displayIndexForRealModule(onlyIdx, bodyScope_))
																		.arg(onlyIdx)
															: QStringLiteral("M%1 updated, rest from PLC").arg(onlyIdx)))
						.arg(static_cast<int>(pathCmdLen)),
				5000);
		return;
	}

	ClientToServerPathWrite payload{};
	payload.mainCmd = kCmdMainPath;
	payload.subCmd = subCmd;

	if (!all) {
		std::array<std::uint8_t, kTwinCatPathCmdByteLen> pathCmdFromPlc{};
		std::uint32_t bytesRead = 0;
		const long readErr =
				device_->ReadReqEx2(adsProfile.pathCmd.indexGroup, adsProfile.pathCmd.indexOffset,
														pathCmdFromPlc.size(), pathCmdFromPlc.data(), &bytesRead);
		if (readErr != 0 || bytesRead != kTwinCatPathCmdByteLen) {
			QMessageBox::warning(
					this, QStringLiteral("Path generation"),
					QStringLiteral("Could not read current PathCmd from PLC (ADS %1, %2 / %3 B).")
							.arg(readErr)
							.arg(bytesRead)
							.arg(static_cast<int>(kTwinCatPathCmdByteLen)));
			return;
		}
		std::memcpy(payload.pathCmd.data(), pathCmdFromPlc.data(), kTwinCatPathCmdByteLen);

		PathParameter &one = payload.pathCmd[static_cast<std::size_t>(onlyIdx)];
		fillPathParameter(&one, onlyIdx);
	} else {
		for (std::size_t i = 0; i < kTwinCatPathCmdCount; ++i) {
			fillPathParameter(&payload.pathCmd[i], static_cast<int>(i));
		}
	}

	const long err =
			device_->WriteReqEx(adsProfile.mainCmd.indexGroup, adsProfile.mainCmd.indexOffset,
													sizeof(payload), &payload);
	if (err != 0) {
		statusBar()->showMessage(
				QStringLiteral("PathCmd write failed: ADS %1 (Main=%2 Sub=%3, %4 B)")
						.arg(err)
						.arg(kCmdMainPath)
						.arg(subCmd)
						.arg(static_cast<int>(sizeof(payload))),
				8000);
		return;
	}
	statusBar()->showMessage(
			QStringLiteral("PathCmd sent — Main=%1 Sub=%2 (%3), %4 B")
					.arg(kCmdMainPath)
					.arg(subCmd)
					.arg(all ? (bodyScope_ == BodyScope::WholeBody
													? QStringLiteral("all slots updated")
													: QStringLiteral("M%1–M%2 updated, rest from PLC")
																.arg(currentBodyScopeRange().firstModule)
																.arg(currentBodyScopeRange().lastModule))
									 : QStringLiteral("slot %1 updated, rest from PLC").arg(onlyIdx))
					.arg(static_cast<int>(sizeof(payload))),
			5000);
}

void MainWindow::setConnectedUi(bool connected) {
	ui_->btnConnect->setText(connected ? QStringLiteral("Disconnect")
																		 : QStringLiteral("Connect"));
	ui_->editHost->setEnabled(!connected);
	ui_->editNetId->setEnabled(!connected);
	ui_->editPort->setEnabled(!connected);
	ui_->btnEnable->setEnabled(connected);
	ui_->btnSystemStop->setEnabled(connected);
	ui_->btnReset->setEnabled(connected);
	ui_->btnCSP->setEnabled(connected);
	ui_->btnCSV->setEnabled(connected);
	ui_->btnCST->setEnabled(connected);
	ui_->btnGenerate->setEnabled(connected);
	ui_->btnStop->setEnabled(connected);
	ui_->btnRepeat->setEnabled(connected);
	ui_->btnCyclic->setEnabled(connected);
	ui_->pushRun->setEnabled(connected);
	if (auto *btnZero = ui_->groupBox_5->findChild<QPushButton *>(QStringLiteral("btnZero"))) {
		btnZero->setEnabled(connected);
	}
	if (auto *btnSaveRun =
					ui_->groupBox_5->findChild<QPushButton *>(QStringLiteral("btnSaveRun"))) {
		btnSaveRun->setEnabled(connected);
	}
	ui_->btnJogPlus->setEnabled(connected);
	ui_->btnJogMinus->setEnabled(connected);
	ui_->btnJogStop->setEnabled(connected);
	updateLoggingButtons();
}

bool MainWindow::parseConnection(QString *errorMessage, std::string *host,
																 std::string *netId, uint16_t *port) {
	const QString h = ui_->editHost->text().trimmed();
	const QString n = ui_->editNetId->text().trimmed();
	const QString p = ui_->editPort->text().trimmed();

	if (h.isEmpty() || n.isEmpty() || p.isEmpty()) {
		*errorMessage = QStringLiteral("Please enter target host, AMS NetId, and port.");
		return false;
	}

	bool portOk = false;
	const int portVal = p.toInt(&portOk);
	if (!portOk || portVal < 1 || portVal > 65535) {
		*errorMessage = QStringLiteral("Port must be a number between 1 and 65535.");
		return false;
	}

	*host = h.toStdString();
	*netId = n.toStdString();
	*port = static_cast<uint16_t>(portVal);
	return true;
}

void MainWindow::loadSettings() {
	QSettings s;
	s.beginGroup(kWindowSettingsGroup);
	const QByteArray geometry = s.value(QStringLiteral("geometry")).toByteArray();
	const QVariant savedWindowPos = s.value(QStringLiteral("windowPosition"));
	const QByteArray graphGeometry = s.value(QStringLiteral("graphViewGeometry")).toByteArray();
	s.endGroup();
	if (geometry.size() >= 16 && geometry.size() < 4096) {
		restoreGeometry(geometry);
	}
	if (graphDisplayWindow_ && graphGeometry.size() >= 16 && graphGeometry.size() < 512) {
		if (graphDisplayWindow_->restoreGeometry(graphGeometry)) {
			graphWindowGeometryRestored_ = true;
		}
	}

	s.beginGroup(kSettingsGroup);
	ui_->editPort->setText(s.value(QStringLiteral("port"), ui_->editPort->text()).toString());
	const int savedScope =
			s.value(QLatin1String(kBodyScopeSettingKey), static_cast<int>(BodyScope::WholeBody))
					.toInt();
	if (savedScope >= static_cast<int>(BodyScope::WholeBody) &&
			savedScope <= static_cast<int>(BodyScope::Module)) {
		bodyScope_ = static_cast<BodyScope>(savedScope);
		{
			const QSignalBlocker scopeBlocker(ui_->comboBodyScope);
			ui_->comboBodyScope->setCurrentIndex(savedScope);
		}
	}
	applyEndpointForBodyScope(bodyScope_);
	s.endGroup();

	s.beginGroup(QLatin1String(kMotorStColsSettingsGroup));
	QCheckBox *const motorColCbs[] = {
			ui_->cbMotorColMode,   ui_->cbMotorColWc,    ui_->cbMotorColErr,
			ui_->cbMotorColTrq,   ui_->cbMotorColTgtPos, ui_->cbMotorColSetPos,
			ui_->cbMotorColSetVel, ui_->cbMotorColSetTrq,
	};
	for (int i = 0; i < kMotorColSettingCount; ++i) {
		motorColCbs[i]->setChecked(
				s.value(QLatin1String(kMotorStColSettingKeys[i]), false).toBool());
	}
	s.endGroup();

	s.beginGroup(QLatin1String(kSavePosSettingsGroup));
	for (int r = 0; r < kMotorSetpointRow; ++r) {
		QStandardItem *savePosCell = motorStModel_.item(r, kColSetPos);
		if (!savePosCell) {
			continue;
		}
		const QString key = QStringLiteral("M%1").arg(r);
		const QString value = s.value(key, QString()).toString().trimmed();
		savePosCell->setText(value);
	}
	s.endGroup();
	applyMotorStColumnVisibility();

	applyBodyScope();

	setConnectedUi(false);
	device_.reset();

	// applyFixedMainWindowSize();
	if (savedWindowPos.isValid() && savedWindowPos.canConvert<QPoint>()) {
		move(savedWindowPos.toPoint());
	}
}

void MainWindow::applyFixedMainWindowSize() {
	const QPoint corner = pos();
	const QSize sz = size();
	setFixedSize(sz);
	move(corner);
	setWindowFlag(Qt::WindowMaximizeButtonHint, false);
	move(corner);
}

void MainWindow::saveSettings() {
	QSettings s;
	s.beginGroup(kWindowSettingsGroup);
	s.setValue(QStringLiteral("geometry"), saveGeometry());
	s.setValue(QStringLiteral("windowPosition"), pos());
	if (graphDisplayWindow_) {
		s.setValue(QStringLiteral("graphViewGeometry"), graphDisplayWindow_->saveGeometry());
	}
	s.endGroup();

	s.beginGroup(kSettingsGroup);
	s.setValue(QStringLiteral("host"), ui_->editHost->text());
	s.setValue(QStringLiteral("netId"), ui_->editNetId->text());
	s.setValue(QStringLiteral("port"), ui_->editPort->text());
	s.setValue(QLatin1String(kBodyScopeSettingKey), static_cast<int>(bodyScope_));
	s.endGroup();

	s.beginGroup(QLatin1String(kMotorStColsSettingsGroup));
	const QCheckBox *const motorColCbs[] = {
			ui_->cbMotorColMode,   ui_->cbMotorColWc,    ui_->cbMotorColErr,
			ui_->cbMotorColTrq,   ui_->cbMotorColTgtPos, ui_->cbMotorColSetPos,
			ui_->cbMotorColSetVel, ui_->cbMotorColSetTrq,
	};
	for (int i = 0; i < kMotorColSettingCount; ++i) {
		s.setValue(QLatin1String(kMotorStColSettingKeys[i]), motorColCbs[i]->isChecked());
	}
	s.endGroup();

	s.beginGroup(QLatin1String(kSavePosSettingsGroup));
	for (int r = 0; r < kMotorSetpointRow; ++r) {
		QStandardItem *savePosCell = motorStModel_.item(r, kColSetPos);
		if (!savePosCell) {
			continue;
		}
		s.setValue(QStringLiteral("M%1").arg(r), savePosCell->text().trimmed());
	}
	s.endGroup();
}

void MainWindow::pollDataMotorSt() {
	if (!device_) {
		return;
	}
	const AdsBodyScopeProfile &profile = currentAdsProfile();
	const BodyScopeRange range = currentBodyScopeRange();

	std::vector<std::uint8_t> stRaw;
	std::vector<std::uint8_t> cmdRaw;
	if (profile.contiguousStCmdRead) {
		std::array<std::uint8_t, kServerToClientStCmdReadLen> raw{};
		std::uint32_t bytesRead = 0;
		const long err =
				device_->ReadReqEx2(profile.dataMotorSt.indexGroup, profile.dataMotorSt.indexOffset,
														raw.size(), raw.data(), &bytesRead);
		if (err != 0) {
			const auto e = static_cast<std::uint32_t>(err);
			if (e != lastMotorPollError_) {
				lastMotorPollError_ = e;
				statusBar()->showMessage(
						QStringLiteral("DataMotorSt/Cmd read failed: ADS error %1").arg(err), 8000);
			}
			return;
		}
		lastMotorPollError_ = 0;
		if (bytesRead != kServerToClientStCmdReadLen) {
			if (!motorStByteLenWarned_) {
				motorStByteLenWarned_ = true;
				statusBar()->showMessage(QStringLiteral("DataMotorSt+Cmd: expected %1 bytes, got %2")
																		 .arg(static_cast<int>(kServerToClientStCmdReadLen))
																		 .arg(bytesRead),
																 8000);
			}
			return;
		}
		motorStByteLenWarned_ = false;
		stRaw.assign(raw.begin(), raw.begin() + profile.dataMotorSt.byteLength);
		cmdRaw.assign(raw.begin() + profile.dataMotorSt.byteLength,
									raw.begin() + profile.dataMotorSt.byteLength + profile.dataMotorCmd.byteLength);
	} else {
		if (!readDataMotorStRaw(&stRaw) || !readDataMotorCmdRaw(&cmdRaw)) {
			if (lastMotorPollError_ == 0) {
				statusBar()->showMessage(QStringLiteral("DataMotorSt/Cmd read failed."), 8000);
			}
			return;
		}
		lastMotorPollError_ = 0;
		motorStByteLenWarned_ = false;
	}

	const std::size_t stByteLen = stRaw.size();
	const std::size_t cmdByteLen = cmdRaw.size();
	const std::size_t cmdStride = motorCmdWireStrideForProfile(profile);

	for (int i = range.firstModule; i <= range.lastModule; ++i) {
		const int r = i;
		int localSlot = 0;
		if (!localSlotForModuleChecked(i, bodyScope_, &localSlot)) {
			clearMotorStPollDataForRow(r);
			continue;
		}
		const std::size_t slot = static_cast<std::size_t>(localSlot);
		const bool miniScope = bodyScopeUsesCompactMotorSt(bodyScope_);
		if (miniScope) {
			const std::size_t byteOffset = slot * sizeof(UpperBodyMiniMotorStWire);
			if (byteOffset + sizeof(UpperBodyMiniMotorStWire) > stByteLen) {
				clearMotorStPollDataForRow(r);
				continue;
			}
		} else if (!motorStWireFits(stByteLen, slot)) {
			clearMotorStPollDataForRow(r);
			continue;
		}
		const MotorCmdWire *c =
				motorCmdWireFits(cmdByteLen, slot, cmdStride)
						? motorCmdWireAt(cmdRaw.data(), slot, cmdStride)
						: nullptr;
		const UpperBodyMiniMotorStWire *mini =
				miniScope
						? reinterpret_cast<const UpperBodyMiniMotorStWire *>(
										stRaw.data() + slot * sizeof(UpperBodyMiniMotorStWire))
						: nullptr;
		const MotorStWire *full = miniScope ? nullptr : motorStWireAt(stRaw.data(), slot);
		QStandardItem *swItem = motorStModel_.item(r, kColSw);
		swItem->setText(formatMotorStatusWord(miniScope ? mini->nStatusWord : full->nStatusWord));
		swItem->setToolTip(
				QStringLiteral("0x%1").arg(miniScope ? mini->nStatusWord : full->nStatusWord, 4, 16, QChar('0')));
		QStandardItem *errItem = motorStModel_.item(r, kColErr);
		errItem->setText(formatMotorErrorCode(miniScope ? mini->nErrorCode : full->nErrorCode));
		errItem->setToolTip(
				QStringLiteral("code=0x%1").arg(miniScope ? mini->nErrorCode : full->nErrorCode, 4, 16, QChar('0')));
		motorStModel_.item(r, kColMode)->setText(
				QString::number(miniScope ? mini->nModeOfOperationDisplay : full->nModeOfOperationDisplay));
		QStandardItem *wcItem = motorStModel_.item(r, kColWc);
		const std::uint8_t wc = miniScope ? static_cast<std::uint8_t>(0) : full->nWcState;
		wcItem->setText(formatWcState(wc));
		if (wc == 0) {
			wcItem->setToolTip(QStringLiteral("raw=0 — Connected"));
		} else if (wc == 1) {
			wcItem->setToolTip(QStringLiteral("raw=1 — Disconnected"));
		} else {
			wcItem->setToolTip(
					QStringLiteral("raw=%1").arg(static_cast<unsigned>(wc)));
		}
		const std::int32_t actualPosition = miniScope ? mini->nActualPosition : full->nActualPosition;
		motorStModel_.item(r, kColPos)->setText(QString::number(actualPosition));
		motorStModel_.item(r, kColVel)->setText(
				QString::number(miniScope ? mini->nActualVelocity : full->nActualVelocity));
		motorStModel_.item(r, kColTrq)->setText(
				QString::number(miniScope ? mini->nActualTorque : full->nActualTorque));
		motorStModel_.item(r, kColDeg)->setText(
				formatDegForModuleFromActualPosition(r, actualPosition));
		if (c) {
			motorStModel_.item(r, kColTgtPos)->setText(QString::number(c->nTargetPosition));
			const qint64 following =
					static_cast<qint64>(c->nTargetPosition) - static_cast<qint64>(actualPosition);
			motorStModel_.item(r, kColFolErr)->setText(QString::number(following));
		} else {
			motorStModel_.item(r, kColTgtPos)->setText(QStringLiteral("—"));
			motorStModel_.item(r, kColFolErr)->setText(QStringLiteral("—"));
		}
		lastMotorWc_[static_cast<std::size_t>(i)] = wc;
	}
	if (topologyWidget_) {
		topologyWidget_->setStatusForAll(TopologyWidget::StatusIcon::Disconnected);
		for (int i = range.firstModule; i <= range.lastModule; ++i) {
			int localSlot = 0;
			if (!localSlotForModuleChecked(i, bodyScope_, &localSlot)) {
				continue;
			}
			const std::size_t slot = static_cast<std::size_t>(localSlot);
			const bool miniScope = bodyScopeUsesCompactMotorSt(bodyScope_);
			if (miniScope) {
				const std::size_t byteOffset = slot * sizeof(UpperBodyMiniMotorStWire);
				if (byteOffset + sizeof(UpperBodyMiniMotorStWire) > stByteLen) {
					continue;
				}
				const auto &m =
						*reinterpret_cast<const UpperBodyMiniMotorStWire *>(stRaw.data() + byteOffset);
				TopologyWidget::StatusIcon st;
				if (m.nErrorCode != 0) {
					st = TopologyWidget::StatusIcon::Fault;
				} else {
					st = TopologyWidget::StatusIcon::Normal;
				}
				topologyWidget_->setModuleStatus(i, st);
				continue;
			}
			if (!motorStWireFits(stByteLen, slot)) {
				continue;
			}
			const MotorStWire &m = *motorStWireAt(stRaw.data(), slot);
			TopologyWidget::StatusIcon st;
			if (m.nErrorCode != 0) {
				st = TopologyWidget::StatusIcon::Fault;
			} else if (m.nWcState == 0) {
				st = TopologyWidget::StatusIcon::Normal;
			} else {
				st = TopologyWidget::StatusIcon::Disconnected;
			}
			topologyWidget_->setModuleStatus(i, st);
		}
	}
	printBodyScopePollDiagnostics(stRaw.data(), stByteLen, cmdRaw.data(), cmdByteLen, cmdStride);
	applyMotorStWcRowFilter();
	updatePathProgressDisplay();
	updateGraphDisplayFromPoll();
}

void MainWindow::onGraphViewClicked() {
	if (!graphDisplayWindow_) {
		return;
	}
	if (!graphWindowGeometryRestored_) {
		const QPoint globalTopRight = mapToGlobal(rect().topRight());
		constexpr int kGap = 20;
		int x = globalTopRight.x() + kGap;
		int y = globalTopRight.y();
		if (QScreen *screen = QGuiApplication::screenAt(mapToGlobal(QPoint(0, 0)))) {
			const QRect available = screen->availableGeometry();
			if (x + graphDisplayWindow_->width() > available.right()) {
				x = available.right() - graphDisplayWindow_->width();
			}
			if (x < available.left()) {
				x = available.left();
			}
			if (y + graphDisplayWindow_->height() > available.bottom()) {
				y = available.bottom() - graphDisplayWindow_->height();
			}
			if (y < available.top()) {
				y = available.top();
			}
		}
		graphDisplayWindow_->move(x, y);
	}
	graphDisplayWindow_->show();
	graphDisplayWindow_->raise();
	graphDisplayWindow_->activateWindow();
}

void MainWindow::setupEmbeddedTopology() {
	topologyWidget_ = new TopologyWidget();
	ui_->scrollAreaTopology->setWidget(topologyWidget_);
	connect(topologyWidget_, &TopologyWidget::moduleClicked, this,
					&MainWindow::onTopologyModuleClicked);
	ui_->gridLayout_7->setColumnStretch(0, 0);
	ui_->gridLayout_7->setColumnStretch(1, 1);
	ui_->gridLayout_7->setColumnStretch(2, 0);
	ui_->gridLayout_7->setColumnStretch(3, 0);
}

void MainWindow::onTopologyModuleClicked(int topologyModuleId) {
	if (topologyModuleId < 0 || topologyModuleId >= kMotorSetpointRow) {
		return;
	}
	if (!moduleInCurrentScope(topologyModuleId)) {
		return;
	}
	const int row = topologyModuleId;
	QStandardItem *swItem = motorStModel_.item(row, kColSw);
	QStandardItem *posItem = motorStModel_.item(row, kColPos);
	QStandardItem *errItem = motorStModel_.item(row, kColErr);
	const QString sw = swItem ? swItem->text() : QStringLiteral("—");
	const QString pos = posItem ? posItem->text() : QStringLiteral("—");
	const QString err = errItem ? errItem->text() : QStringLiteral("—");
	statusBar()->showMessage(
			QStringLiteral("M%1 · SW=%2 · Pos=%3 · Err=%4").arg(row).arg(sw).arg(pos).arg(err),
			6000);
}

void MainWindow::onGraphDisplaySelectionChanged() {
	graphDisplaySelX_.clear();
	graphDisplaySelY_.clear();
}

void MainWindow::resetGraphDisplayData() {
	graphDisplayTime_.clear();
	for (QVector<double> &v : graphDisplayPosByModule_) {
		v.clear();
	}
	graphDisplaySelX_.clear();
	graphDisplaySelY_.clear();
	graphDisplayTimeSec_ = 0.0;
}

void MainWindow::updateGraphDisplayFromPoll() {
	if (!graphDisplayWindow_ || !graphDisplayWindow_->isVisible() || !device_) {
		return;
	}
	graphDisplayTime_.append(graphDisplayTimeSec_);
	for (int j = 0; j < static_cast<int>(kMotorCount); ++j) {
		QStandardItem *posIt = motorStModel_.item(j, kColPos);
		const double pa = posIt ? posIt->text().toDouble() : 0.0;
		graphDisplayPosByModule_[j].append(pa);
	}
	const int modRow = graphDisplayWindow_->comboModule()->currentData().toInt();
	const int dataCol = graphDisplayWindow_->comboData()->currentData().toInt();
	double val = 0.0;
	if (dataCol == kColWc) {
		val = static_cast<double>(lastMotorWc_[static_cast<std::size_t>(modRow)]);
	} else {
		QStandardItem *it = motorStModel_.item(modRow, dataCol);
		val = it ? it->text().toDouble() : 0.0;
	}
	graphDisplaySelX_.append(graphDisplayTimeSec_);
	graphDisplaySelY_.append(val);
	graphDisplayTimeSec_ += static_cast<double>(kMotorPollIntervalMs) / 1000.0;
	while (graphDisplayTime_.size() > kGraphDisplayMaxPoints) {
		graphDisplayTime_.removeFirst();
		for (int j = 0; j < static_cast<int>(kMotorCount); ++j) {
			graphDisplayPosByModule_[j].removeFirst();
		}
	}
	while (graphDisplaySelX_.size() > kGraphDisplayMaxPoints) {
		graphDisplaySelX_.removeFirst();
		graphDisplaySelY_.removeFirst();
	}
	graphDisplayWindow_->updateDisplay(graphDisplayTime_, graphDisplayPosByModule_, graphDisplaySelX_,
																		 graphDisplaySelY_);
}

BodyScopeRange MainWindow::currentBodyScopeRange() const {
	return bodyScopeRange(bodyScope_);
}

bool MainWindow::moduleInCurrentScope(int moduleId) const {
	return moduleInBodyScope(moduleId, bodyScope_);
}

void MainWindow::rebuildPathIndexCombo() {
	const BodyScopeRange range = currentBodyScopeRange();
	const int previousReal = ui_->cbIndex->currentData(Qt::UserRole).toInt();
	ui_->cbIndex->clear();
	if (bodyScope_ == BodyScope::LowerBody || bodyScope_ == BodyScope::UpperBodyMini ||
			bodyScope_ == BodyScope::Module) {
		for (int local = 0; local < range.moduleCount; ++local) {
			const int realModule = realModuleForLocalSlot(local, bodyScope_);
			ui_->cbIndex->addItem(QString::number(local), realModule);
			const int row = ui_->cbIndex->count() - 1;
			ui_->cbIndex->setItemData(row, local, kPathComboRoleLocalSlot);
		}
	} else {
		for (int i = range.firstModule; i <= range.lastModule; ++i) {
			ui_->cbIndex->addItem(QString::number(i), i);
		}
	}
	int selectIndex = ui_->cbIndex->findData(previousReal);
	if (selectIndex < 0) {
		selectIndex = 0;
	}
	if (ui_->cbIndex->count() > 0) {
		ui_->cbIndex->setCurrentIndex(selectIndex);
	}
}

void MainWindow::setupBodyScopeUi() {
	ui_->comboBodyScope->clear();
	ui_->comboBodyScope->addItem(QStringLiteral("Whole Body"));
	ui_->comboBodyScope->addItem(QStringLiteral("Upper Body"));
	ui_->comboBodyScope->addItem(QStringLiteral("Upper Body Mini"));
	ui_->comboBodyScope->addItem(QStringLiteral("Lower Body"));
	ui_->comboBodyScope->addItem(QStringLiteral("Module"));
	ui_->comboBodyScope->setCurrentIndex(static_cast<int>(BodyScope::WholeBody));
	connect(ui_->comboBodyScope, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
					&MainWindow::onBodyScopeChanged);
}

void MainWindow::onBodyScopeChanged(int index) {
	if (index < static_cast<int>(BodyScope::WholeBody) ||
			index > static_cast<int>(BodyScope::Module)) {
		return;
	}
	bodyScope_ = static_cast<BodyScope>(index);
	if (loggingActive_ && !bodyScopeSupportsLogging(bodyScope_)) {
		stopLogging(true);
		statusBar()->showMessage(
				QStringLiteral("Logging stopped: %1 does not support logging.")
						.arg(QString::fromUtf8(bodyScopeLabel(bodyScope_))),
				4000);
	}
	applyBodyScope();
	applyEndpointForBodyScope(bodyScope_);
	saveConnectionSettings();
}

void MainWindow::applyEndpointForBodyScope(BodyScope scope) {
	const BodyScopeEndpoint *endpoint = nullptr;
	switch (scope) {
		case BodyScope::WholeBody:
		case BodyScope::UpperBody:
		case BodyScope::UpperBodyMini:
			endpoint = &kUpperBodyEndpoint;
			break;
		case BodyScope::LowerBody:
		case BodyScope::Module:
			endpoint = &kLowerBodyEndpoint;
			break;
	}
	const QSignalBlocker hostBlocker(ui_->editHost);
	const QSignalBlocker netIdBlocker(ui_->editNetId);
	ui_->editHost->setText(QString::fromUtf8(endpoint->host));
	ui_->editNetId->setText(QString::fromUtf8(endpoint->netId));
	if (scope == BodyScope::UpperBody) {
		const QSignalBlocker portBlocker(ui_->editPort);
		ui_->editPort->setText(QString::number(kUpperBodyAdsProfile.defaultPort));
	} else if (scope == BodyScope::UpperBodyMini) {
		const QSignalBlocker portBlocker(ui_->editPort);
		ui_->editPort->setText(QString::number(kUpperBodyMiniAdsProfile.defaultPort));
	} else if (scope == BodyScope::LowerBody) {
		const QSignalBlocker portBlocker(ui_->editPort);
		ui_->editPort->setText(QString::number(kLowerBodyAdsProfile.defaultPort));
	} else if (scope == BodyScope::Module) {
		const QSignalBlocker portBlocker(ui_->editPort);
		ui_->editPort->setText(QString::number(kModuleAdsProfile.defaultPort));
	}
}

const AdsBodyScopeProfile &MainWindow::currentAdsProfile() const {
	switch (bodyScope_) {
		case BodyScope::UpperBody:
			return kUpperBodyAdsProfile;
		case BodyScope::UpperBodyMini:
			return kUpperBodyMiniAdsProfile;
		case BodyScope::LowerBody:
			return kLowerBodyAdsProfile;
		case BodyScope::Module:
			return kModuleAdsProfile;
		case BodyScope::WholeBody:
		default:
			return kWholeBodyAdsProfile;
	}
}

void MainWindow::saveConnectionSettings() {
	QSettings s;
	s.beginGroup(kSettingsGroup);
	s.setValue(QStringLiteral("host"), ui_->editHost->text());
	s.setValue(QStringLiteral("netId"), ui_->editNetId->text());
	s.setValue(QLatin1String(kBodyScopeSettingKey), static_cast<int>(bodyScope_));
	s.endGroup();
}

void MainWindow::applyBodyScope() {
	const BodyScopeRange range = currentBodyScopeRange();

	bodyScopeDiagPrinted_ = false;
	bodyScopePollDiagPrinted_ = false;
	printBodyScopeDiagnostics();

	if (topologyWidget_) {
		topologyWidget_->setActiveModuleRange(range.firstModule, range.lastModule);
	}
	if (graphDisplayWindow_) {
		graphDisplayWindow_->setVisibleModuleRange(range.firstModule, range.lastModule);
		onGraphDisplaySelectionChanged();
	}

	rebuildPathIndexCombo();
	updateMotorTableDisplayIndices();
	for (int r = range.firstModule; r <= range.lastModule; ++r) {
		clearMotorStPollDataForRow(r);
	}
	clearMotorStPollDataOutsideScope();
	resetMotorTableScroll();
	applyMotorStWcRowFilter();
}

void MainWindow::onConnectClicked() {
	if (device_) {
		stopLogging();
		stopPathMotionTracking();
		pollTimer_.stop();
		lastMotorPollError_ = 0;
		motorStByteLenWarned_ = false;
		resetGraphDisplayData();
		if (topologyWidget_) {
			topologyWidget_->setStatusForAll(TopologyWidget::StatusIcon::Disconnected);
		}
		clearMotorStTable();
		device_.reset();
		setConnectedUi(false);
		statusBar()->showMessage(QStringLiteral("Disconnected."), 4000);
		return;
	}

	QString err;
	std::string host;
	std::string netIdStr;
	uint16_t port = 0;
	if (!parseConnection(&err, &host, &netIdStr, &port)) {
		QMessageBox::warning(this, QStringLiteral("Input required"), err);
		return;
	}

	statusBar()->showMessage(
			QStringLiteral("Connecting to %1 …").arg(ui_->editHost->text()), 3000);

	try {
		const AmsNetId remoteNetId{netIdStr};
		auto dev = std::make_unique<AdsDevice>(host, remoteNetId, port);
		QString detail;
		try {
			const DeviceInfo info = dev->GetDeviceInfo();
			const QString name =
					QString::fromLatin1(info.name, strnlen(info.name, DEVICE_NAME_LENGTH));
			detail = name.isEmpty() ? QStringLiteral("ADS") : name;
		} catch (const AdsException &infoEx) {
			if (infoEx.errorCode != kAdsErrServiceNotSupp) {
				throw;
			}
			const AdsDeviceState st = dev->GetState();
			if (st.ads == ADSSTATE_INVALID && st.device == ADSSTATE_INVALID) {
				detail = QStringLiteral("ADS port %1 OK (no run-state on task port)")
										 .arg(port);
			} else {
				detail = QStringLiteral("ADS %1 / device %2")
										 .arg(formatAdsState(st.ads), formatAdsState(st.device));
			}
		}
		device_ = std::move(dev);
		setConnectedUi(true);
		const QString summary = QStringLiteral("Connected — %1").arg(detail);
		const AdsBodyScopeProfile &profile = currentAdsProfile();
		const int scopedReadBytes = static_cast<int>(profile.dataMotorSt.byteLength +
																								 profile.dataMotorCmd.byteLength);
		statusBar()->showMessage(
				summary + QStringLiteral(" · polling DataMotorSt+Cmd (%1 B) every %2 ms")
											.arg(scopedReadBytes)
											.arg(pollTimer_.interval()),
				6000);
		lastMotorPollError_ = 0;
		motorStByteLenWarned_ = false;
		pollTimer_.start();
		writeClientMainSubCmd(kCmdMainInit, kCmdSubInit);
	} catch (const AdsException &ex) {
		QMessageBox::critical(this, QStringLiteral("Connection failed"),
													QStringLiteral("ADS error %1: %2")
															.arg(ex.errorCode)
															.arg(QString::fromUtf8(ex.what())));
		statusBar()->showMessage(
				QStringLiteral("Connection failed: ADS %1").arg(ex.errorCode), 8000);
	} catch (const std::exception &ex) {
		QMessageBox::critical(this, QStringLiteral("Connection failed"),
													QString::fromUtf8(ex.what()));
		statusBar()->showMessage(QStringLiteral("Connection failed."), 8000);
	}
}
