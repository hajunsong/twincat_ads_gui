#pragma once

#include <QPixmap>
#include <QWidget>
#include <array>

class TopologyWidget : public QWidget {
  Q_OBJECT

 public:
  enum class StatusIcon {
    Disconnected,
    Normal,
    Fault
  };

  explicit TopologyWidget(QWidget *parent = nullptr);
  void setStatusForAll(StatusIcon icon);
  /**
   * 모듈 인덱스 0…30 (표시 M0…M30). MainWindow 폴링:
   * Fault = nErrorCode != 0, Normal = nWcState==0, Disconnected = nWcState!=0 (오류 없음).
   */
  void setModuleStatus(int moduleId, StatusIcon icon);
  /** Active module IDs inclusive (M0…M30). Inactive modules stay drawn but dimmed. */
  void setActiveModuleRange(int firstModule, int lastModule);

 signals:
  /** 클릭한 모듈 인덱스 0…30 (M0…M30). */
  void moduleClicked(int moduleId);

 protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;

 private:
  void ensureStatusPixmaps();
  const QPixmap &pixmapFor(StatusIcon icon) const;
  bool isModuleActive(int moduleId) const;

  int m_activeFirstModule = 0;
  int m_activeLastModule = 30;
  std::array<StatusIcon, 31> m_moduleStatus{};
  mutable QPixmap m_pixDisconnected;
  mutable QPixmap m_pixNormal;
  mutable QPixmap m_pixFault;
  mutable bool m_pixmapsLoaded = false;
};
