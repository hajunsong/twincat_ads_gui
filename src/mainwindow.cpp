#include "mainwindow.h"

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
#include <QDir>
#include <QFile>
#include <QDoubleSpinBox>
#include <QFont>
#include <QGuiApplication>
#include <QHeaderView>
#include <QLineEdit>
#include <QScreen>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
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
#include <limits>

namespace {
constexpr char kSettingsGroup[] = "connection";
constexpr char kWindowSettingsGroup[] = "window";
constexpr char kMotorStColsSettingsGroup[] = "motorStCols";
/** ADSERR_DEVICE_SERVICENOTSUPP — task ADS ports often reject ReadDeviceInfo. */
constexpr uint32_t kAdsErrServiceNotSupp = 1793;

/** TMC ADS: ServerToClient — DataMotorSt @ 0x83000000, DataMotorCmd @ 0x830001F0 (contiguous). */
constexpr uint32_t kDataMotorStIndexGroup = 0x1010010;
constexpr uint32_t kDataMotorStIndexOffset = 0x83000000;
constexpr size_t kDataMotorStByteLen = 496;
constexpr size_t kDataMotorCmdByteLen = 496;
constexpr size_t kServerToClientStCmdReadLen = kDataMotorStByteLen + kDataMotorCmdByteLen;
constexpr size_t kMotorCount = 31;
constexpr int kMotorSetpointRow = static_cast<int>(kMotorCount);
constexpr int kMotorTableRowCount = kMotorSetpointRow + 1;
constexpr int kMotorTableColumnCount = 13;
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
  kColSetPos = 10,
  kColSetVel = 11,
  kColSetTrq = 12,
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
constexpr uint16_t kCmdMainPath = 3;
constexpr uint16_t kCmdSubPathTrapezoidal = 1;
constexpr uint16_t kCmdSubPathSCurve = 2;
constexpr uint16_t kCmdSubPathOpRun = 3;
constexpr uint16_t kCmdSubPathOpRepeat = 4;
constexpr uint16_t kCmdSubPathOpCyclic = 5;
constexpr uint16_t kCmdSubPathOpStop = 6;

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

/**
 * Packed layout: TwinCAT MotorSt (16 B × 31 = 496). If values look wrong,
 * compare byte layout in your .tmc (alignment / type sizes).
 */
#pragma pack(push, 1)
struct MotorSt {
  uint16_t nStatusWord;
  int32_t nActualPosition;
  int32_t nActualVelocity;
  int16_t nActualTorque;
  uint16_t nErrorCode;
  int8_t nModeOfOperationDisp;
  uint8_t nWcState;
};
#pragma pack(pop)

static_assert(sizeof(MotorSt) == 16, "MotorSt must be 16 bytes for 31×496 TMC");
static_assert(sizeof(MotorSt) * kMotorCount == kDataMotorStByteLen, "");

/**
 * Packed layout: TwinCAT MotorCmd (16 B × 31 = 496). Matches TMC after DataMotorSt.
 * nModeOfOperation is 2 B (e.g. INT); MotorSt uses nModeOfOperationDisp as 1 B (SINT).
 */
#pragma pack(push, 1)
struct MotorCmd {
  uint16_t nControlWord;
  int32_t nTargetPosition;
  int32_t nTargetVelocity;
  int16_t nTargetTorque;
  uint16_t nMaxTorque;
  int16_t nModeOfOperation;
};
#pragma pack(pop)

static_assert(sizeof(MotorCmd) == 16, "MotorCmd must be 16 bytes for 31×496 TMC");
static_assert(sizeof(MotorCmd) * kMotorCount == kDataMotorCmdByteLen, "");
static_assert(kServerToClientStCmdReadLen == 992, "");

#pragma pack(push, 1)
struct ClientMainSubCmd {
  uint16_t mainCmd;
  uint16_t subCmd;
};
#pragma pack(pop)

static_assert(sizeof(ClientMainSubCmd) == 4, "MainCmd+SubCmd UINT pair");

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
       QStringLiteral("FolErr"), QStringLiteral("Set pos"), QStringLiteral("Set vel"),
       QStringLiteral("Set trq")});
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
      } else if (c == kColSetPos) {
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
        (c >= kColSetPos) ? QString() : QStringLiteral("—"));
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
      {ui_->cbMotorColSetPos, kColSetPos}, {ui_->cbMotorColSetVel, kColSetVel},
      {ui_->cbMotorColSetTrq, kColSetTrq},
  };
  for (const auto &t : toggles) {
    tv->setColumnHidden(t.col, !t.cb->isChecked());
  }
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
    const bool wcConn = (lastMotorWc_[static_cast<std::size_t>(r)] == 0);
    tv->setRowHidden(r, onlyConnected && !wcConn);
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

void MainWindow::clearMotorStTable() {
  lastMotorWc_.fill(kDefaultWcState);
  for (int r = 0; r < kMotorSetpointRow; ++r) {
    for (int c = 1; c < kMotorTableColumnCount; ++c) {
      if (QStandardItem *it = motorStModel_.item(r, c)) {
        if (c == kColWc) {
          it->setText(formatWcState(kDefaultWcState));
          it->setToolTip(QStringLiteral("raw=1 — Disconnected"));
        } else if (c == kColSetPos) {
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
  for (int i = 0; i < static_cast<int>(kMotorCount); ++i) {
    ui_->cbIndex->addItem(QString::number(i), i);
  }
  ui_->cbIndex->setMaxVisibleItems(10);

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
}

void MainWindow::setupSetPosShortcuts() {
  auto *f3 = new QShortcut(QKeySequence(Qt::Key_F3), this);
  f3->setContext(Qt::WindowShortcut);
  connect(f3, &QShortcut::activated, this, [this]() {
    for (int r = 0; r < kMotorSetpointRow; ++r) {
      if (QStandardItem *it = motorStModel_.item(r, kColSetPos)) {
        it->setText(QString::number(kSetPosShortcutFillValue));
      }
    }
  });
  auto *f4 = new QShortcut(QKeySequence(Qt::Key_F4), this);
  f4->setContext(Qt::WindowShortcut);
  connect(f4, &QShortcut::activated, this, [this]() {
    for (int r = 0; r < kMotorSetpointRow; ++r) {
      if (QStandardItem *it = motorStModel_.item(r, kColSetPos)) {
        it->setText(QStringLiteral("0"));
      }
    }
  });
}

void MainWindow::setupLoggingUi() {
  connect(ui_->pushButton, &QPushButton::clicked, this, [this]() { startLogging(); });
  connect(ui_->pushButton_2, &QPushButton::clicked, this, [this]() { stopLogging(); });
}

void MainWindow::updateLoggingButtons() {
  const bool conn = device_ != nullptr;
  ui_->pushButton->setEnabled(conn && !loggingActive_);
  ui_->pushButton_2->setEnabled(conn && loggingActive_);
}

void MainWindow::startLogging() {
  if (loggingActive_) {
    statusBar()->showMessage(QStringLiteral("Logging is already running."), 3000);
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
  for (const auto &s : loggingCsvStreams_) {
    if (!s) {
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
  std::array<std::uint8_t, sizeof(TwinCatLoggingBufferData)> raw{};
  std::uint32_t bytesRead = 0;
  const long err =
      device_->ReadReqEx2(kTwinCatLoggingBufferIndexGroup, kTwinCatLoggingBufferIndexOffset,
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

  const qint64 utcMs = QDateTime::currentMSecsSinceEpoch();
  const qint64 batch = loggingBatchIndex_++;

  for (std::size_t mod = 0; mod < kTwinCatLogMotorCount; ++mod) {
    QTextStream *out = loggingCsvStreams_[mod].get();
    if (!out) {
      continue;
    }
    for (int idx = startIdx; idx <= endIdx; ++idx) {
      const LogMotorStSample &s = data.motorStBuffer[static_cast<std::size_t>(idx)][mod];
      *out << utcMs << ',' << batch << ',' << idx << ',' << s.nStatusWord << ','
           << s.nActualPosition << ',' << s.nActualVelocity << ',' << s.nActualTorque << ','
           << s.nErrorCode << ',' << static_cast<int>(s.nModeOfOperationDisp) << ','
           << static_cast<unsigned>(s.nWcState) << '\n';
    }
    out->flush();
  }
}

void MainWindow::setupOperatingUi() {
  connect(ui_->pushRun, &QPushButton::clicked, this, [this]() {
    if (!device_) {
      return;
    }
    writeClientMainSubCmd(kCmdMainPath, kCmdSubPathOpRun);
  });
  connect(ui_->btnStop, &QPushButton::clicked, this, [this]() {
    if (!device_) {
      return;
    }
    writeClientMainSubCmd(kCmdMainPath, kCmdSubPathOpStop);
  });
  connect(ui_->btnRepeat, &QPushButton::clicked, this, [this]() {
    if (!device_) {
      return;
    }
    writeClientMainSubCmd(kCmdMainPath, kCmdSubPathOpRepeat);
  });
  connect(ui_->btnCyclic, &QPushButton::clicked, this, [this]() {
    if (!device_) {
      return;
    }
    writeClientMainSubCmd(kCmdMainPath, kCmdSubPathOpCyclic);
  });
}

void MainWindow::writeClientMainSubCmd(uint16_t mainCmd, uint16_t subCmd) {
  if (!device_) {
    return;
  }
  const ClientMainSubCmd payload{mainCmd, subCmd};
  const long err =
      device_->WriteReqEx(kClientMainSubIndexGroup, kClientMainSubIndexOffset,
                          sizeof(payload), &payload);
  if (err != 0) {
    statusBar()->showMessage(QStringLiteral("MainCmd/SubCmd write failed: ADS %1 (Main=%2 Sub=%3)")
                                 .arg(err)
                                 .arg(mainCmd)
                                 .arg(subCmd),
                             6000);
  }
}

std::uint16_t MainWindow::pathProfileSubCmdFromComboIndex(int comboIndex) {
  if (comboIndex == 1) {
    return kCmdSubPathSCurve;
  }
  return kCmdSubPathTrapezoidal;
}

int MainWindow::resolvedPathMotorIndex() const {
  bool ok = false;
  int v = ui_->cbIndex->currentData().toInt(&ok);
  if (ok && v >= 0 && v < static_cast<int>(kMotorCount)) {
    return v;
  }
  v = ui_->cbIndex->currentText().trimmed().toInt(&ok);
  if (ok && v >= 0 && v < static_cast<int>(kMotorCount)) {
    return v;
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

bool MainWindow::readMotorActualPositions(
    std::array<std::int32_t, kTwinCatPathCmdCount> *out) const {
  if (!device_ || !out) {
    return false;
  }
  std::array<std::uint8_t, kDataMotorStByteLen> raw{};
  std::uint32_t bytesRead = 0;
  const long err =
      device_->ReadReqEx2(kDataMotorStIndexGroup, kDataMotorStIndexOffset, raw.size(),
                          raw.data(), &bytesRead);
  if (err != 0 || bytesRead != kDataMotorStByteLen) {
    return false;
  }
  const auto *motors = reinterpret_cast<const MotorSt *>(raw.data());
  for (std::size_t i = 0; i < kTwinCatPathCmdCount; ++i) {
    (*out)[i] = motors[i].nActualPosition;
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
      QMessageBox::warning(
          this, QStringLiteral("Path generation"),
          QStringLiteral("Select a valid module index (0–%1), or enable All.")
              .arg(static_cast<int>(kMotorCount) - 1));
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
    QStandardItem *sp = motorStModel_.item(row, kColSetPos);
    return parseSetPositionCell(sp ? sp->text() : QString(),
                                actualPos[static_cast<std::size_t>(row)]);
  };

  ClientToServerPathWrite payload{};
  payload.mainCmd = kCmdMainPath;
  payload.subCmd = subCmd;

  const double totalTime = ui_->dsbTotalTime->value();
  const double stepSize = ui_->dsbStepSize->value();
  const double accTime = ui_->dsbAccTime->value();

  if (!all) {
    std::array<std::uint8_t, kTwinCatPathCmdByteLen> pathCmdFromPlc{};
    std::uint32_t bytesRead = 0;
    const long readErr =
        device_->ReadReqEx2(kClientMainSubIndexGroup, kTwinCatPathCmdIndexOffset,
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
    one.nTotalTime = totalTime;
    one.nStepSize = stepSize;
    one.nAccTime = accTime;
    one.nProfileMode = profileMode;
    one.nUpdate = 1;
    one.nSetPosition = nSetPositionForRow(onlyIdx);
  } else {
    for (std::size_t i = 0; i < kTwinCatPathCmdCount; ++i) {
      PathParameter &p = payload.pathCmd[i];
      p.nTotalTime = totalTime;
      p.nStepSize = stepSize;
      p.nAccTime = accTime;
      p.nProfileMode = profileMode;
      p.nUpdate = 1;
      p.nSetPosition = nSetPositionForRow(static_cast<int>(i));
    }
  }

  const long err =
      device_->WriteReqEx(kClientMainSubIndexGroup, kClientMainSubIndexOffset, sizeof(payload),
                          &payload);
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
          .arg(all ? QStringLiteral("all slots updated")
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
  ui_->btnGenerate->setEnabled(connected);
  ui_->btnStop->setEnabled(connected);
  ui_->btnRepeat->setEnabled(connected);
  ui_->btnCyclic->setEnabled(connected);
  ui_->pushRun->setEnabled(connected);
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
  ui_->editHost->setText(s.value(QStringLiteral("host"), ui_->editHost->text()).toString());
  ui_->editNetId->setText(s.value(QStringLiteral("netId"), ui_->editNetId->text()).toString());
  ui_->editPort->setText(s.value(QStringLiteral("port"), ui_->editPort->text()).toString());
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
  applyMotorStColumnVisibility();

  setConnectedUi(false);
  device_.reset();

  applyFixedMainWindowSize();
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
}

void MainWindow::pollDataMotorSt() {
  if (!device_) {
    return;
  }
  std::array<uint8_t, kServerToClientStCmdReadLen> raw{};
  uint32_t bytesRead = 0;
  const long err =
      device_->ReadReqEx2(kDataMotorStIndexGroup, kDataMotorStIndexOffset, raw.size(),
                          raw.data(), &bytesRead);
  if (err != 0) {
    const auto e = static_cast<uint32_t>(err);
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
  const auto *motors = reinterpret_cast<const MotorSt *>(raw.data());
  const auto *cmds =
      reinterpret_cast<const MotorCmd *>(raw.data() + kDataMotorStByteLen);
  for (size_t i = 0; i < kMotorCount; ++i) {
    const MotorSt &m = motors[i];
    const MotorCmd &c = cmds[i];
    const int r = static_cast<int>(i);
    QStandardItem *swItem = motorStModel_.item(r, kColSw);
    swItem->setText(formatMotorStatusWord(m.nStatusWord));
    swItem->setToolTip(
        QStringLiteral("0x%1").arg(m.nStatusWord, 4, 16, QChar('0')));
    QStandardItem *errItem = motorStModel_.item(r, kColErr);
    errItem->setText(formatMotorErrorCode(m.nErrorCode));
    errItem->setToolTip(
        QStringLiteral("code=0x%1").arg(m.nErrorCode, 4, 16, QChar('0')));
    motorStModel_.item(r, kColMode)->setText(QString::number(m.nModeOfOperationDisp));
    QStandardItem *wcItem = motorStModel_.item(r, kColWc);
    wcItem->setText(formatWcState(m.nWcState));
    if (m.nWcState == 0) {
      wcItem->setToolTip(QStringLiteral("raw=0 — Connected"));
    } else if (m.nWcState == 1) {
      wcItem->setToolTip(QStringLiteral("raw=1 — Disconnected"));
    } else {
      wcItem->setToolTip(
          QStringLiteral("raw=%1").arg(static_cast<unsigned>(m.nWcState)));
    }
    motorStModel_.item(r, kColPos)->setText(QString::number(m.nActualPosition));
    motorStModel_.item(r, kColVel)->setText(QString::number(m.nActualVelocity));
    motorStModel_.item(r, kColTrq)->setText(QString::number(m.nActualTorque));
    motorStModel_.item(r, kColTgtPos)->setText(QString::number(c.nTargetPosition));
    const qint64 following =
        static_cast<qint64>(c.nTargetPosition) - static_cast<qint64>(m.nActualPosition);
    motorStModel_.item(r, kColFolErr)->setText(QString::number(following));
    lastMotorWc_[i] = m.nWcState;
  }
  if (topologyWidget_) {
    topologyWidget_->setStatusForAll(TopologyWidget::StatusIcon::Disconnected);
    for (size_t i = 0; i < kMotorCount; ++i) {
      const MotorSt &m = motors[i];
      TopologyWidget::StatusIcon st;
      if (m.nErrorCode != 0) {
        st = TopologyWidget::StatusIcon::Fault;
      } else if (m.nWcState == 0) {
        st = TopologyWidget::StatusIcon::Normal;
      } else {
        st = TopologyWidget::StatusIcon::Disconnected;
      }
      topologyWidget_->setModuleStatus(static_cast<int>(i), st);
    }
  }
  applyMotorStWcRowFilter();
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
  const int row = topologyModuleId;
  QStandardItem *swItem = motorStModel_.item(row, kColSw);
  QStandardItem *posItem = motorStModel_.item(row, kColPos);
  QStandardItem *errItem = motorStModel_.item(row, kColErr);
  const QString sw = swItem ? swItem->text() : QStringLiteral("—");
  const QString pos = posItem ? posItem->text() : QStringLiteral("—");
  const QString err = errItem ? errItem->text() : QStringLiteral("—");
  statusBar()->showMessage(
      QStringLiteral("M%1 · SW=%2 · Pos=%3 · Err=%4")
          .arg(row)
          .arg(sw)
          .arg(pos)
          .arg(err),
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

void MainWindow::onConnectClicked() {
  if (device_) {
    stopLogging();
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
    statusBar()->showMessage(
        summary + QStringLiteral(" · polling DataMotorSt+Cmd (%1 B) every %2 ms")
                      .arg(static_cast<int>(kServerToClientStCmdReadLen))
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
