#pragma once

#include <QElapsedTimer>
#include <QFile>
#include <QMainWindow>
#include <QStandardItemModel>
#include <QTextStream>
#include <QTimer>
#include <QVector>
#include <array>
#include <cstdint>
#include <memory>

#include "twincat_logging_buffer.h"
#include "twincat_path_cmd.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class AdsDevice;
class QCloseEvent;
class GraphDisplayWindow;
class QLineEdit;
class TopologyWidget;

/** Main window (Qt 5): ADS connection fields, connect/disconnect, persisted settings. */
class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow() override;

 protected:
  void closeEvent(QCloseEvent *event) override;

 private slots:
  void onConnectClicked();
  void onGraphViewClicked();
  void onTopologyModuleClicked(int topologyModuleId);
  void onGraphDisplaySelectionChanged();
  void pollDataMotorSt();
  void appendLoggingSnapshot();

 private:
  void loadSettings();
  void saveSettings();
  /** 저장/기본 크기로 메인 창을 고정하고 리사이즈·최대화 비활성화. */
  void applyFixedMainWindowSize();
  void setConnectedUi(bool connected);
  bool parseConnection(QString *errorMessage, std::string *host, std::string *netId,
                       uint16_t *port);
  void setupMotorStTable();
  void setupMotorStTableView();
  void setupMotorStColumnVisibility();
  void applyMotorStColumnVisibility();
  void setupMotorStWcConnectedFilter();
  void applyMotorStWcRowFilter();
  void clearMotorStTable();
  void installMotorSetpointRowWidgets();
  void setupPathGenerationUi();
  void setupSystemControlUi();
  void setupModeOfOperationUi();
  void setupOperatingUi();
  void setupSetPosShortcuts();
  void setupLoggingUi();
  void startLogging();
  /** @return closed file path if any. @param silentStatus skip status-bar message */
  QString stopLogging(bool silentStatus = false);
  void updateLoggingButtons();
  void writeClientMainSubCmd(std::uint16_t mainCmd, std::uint16_t subCmd);
  void sendPathGeneration();
  int resolvedPathMotorIndex() const;
  static std::uint16_t pathProfileSubCmdFromComboIndex(int comboIndex);
  static std::int32_t parseSetPositionCell(const QString &text, std::int32_t fallbackActual);
  bool readMotorActualPositions(std::array<std::int32_t, kTwinCatPathCmdCount> *out) const;
  void resetGraphDisplayData();
  void updateGraphDisplayFromPoll();
  void setupEmbeddedTopology();

  Ui::MainWindow *ui_;
  std::unique_ptr<AdsDevice> device_;
  QTimer pollTimer_;
  QTimer loggingTimer_;
  QString loggingSessionFolderPath_;
  std::array<std::unique_ptr<QFile>, kTwinCatLogMotorCount> loggingCsvFiles_{};
  std::array<std::unique_ptr<QTextStream>, kTwinCatLogMotorCount> loggingCsvStreams_{};
  bool loggingActive_ = false;
  double loggingDurationSec_ = 0.0;
  qint64 loggingBatchIndex_ = 0;
  QElapsedTimer loggingSessionElapsed_;
  QStandardItemModel motorStModel_;
  /** Last raw nWcState per motor row (0 = Connect). Used for WC-only row filter. */
  std::array<std::uint8_t, kTwinCatLogMotorCount> lastMotorWc_{};
  std::uint32_t lastMotorPollError_ = 0;
  bool motorStByteLenWarned_ = false;
  QLineEdit *motorSetpointVelEdit_ = nullptr;
  QLineEdit *motorSetpointTrqEdit_ = nullptr;

  GraphDisplayWindow *graphDisplayWindow_ = nullptr;
  bool graphWindowGeometryRestored_ = false;
  QVector<double> graphDisplayTime_;
  QVector<QVector<double>> graphDisplayPosByModule_;
  QVector<double> graphDisplaySelX_;
  QVector<double> graphDisplaySelY_;
  double graphDisplayTimeSec_ = 0.0;
  static constexpr int kGraphDisplayMaxPoints = 500;

  TopologyWidget *topologyWidget_ = nullptr;
};
