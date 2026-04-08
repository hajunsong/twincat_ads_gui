#include "topologywidget.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QMouseEvent>
#include <QPainter>
#include <QSizePolicy>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPixmap>
#include <QFontMetrics>
#include <QVector>
#include <array>
#include <cmath>

namespace {
QString figureDirectoryPath() {
  const QString appDir = QCoreApplication::applicationDirPath();
  const QString cwd = QDir::currentPath();
  // 기본: 프로젝트 루트의 figure/ (예: module_test/figure/state_*.png)
  const QStringList candidates = {
      appDir + QStringLiteral("/figure"),
      appDir + QStringLiteral("/../figure"),
      cwd + QStringLiteral("/figure"),
      cwd + QStringLiteral("/../figure"),
  };

  for (const QString &path : candidates) {
    if (QFileInfo::exists(path) && QFileInfo(path).isDir()) {
      return QDir(path).absolutePath();
    }
  }
  return appDir + QStringLiteral("/figure");
}

QString statusImagePath(TopologyWidget::StatusIcon icon) {
  const QString figurePath = figureDirectoryPath();
  switch (icon) {
    case TopologyWidget::StatusIcon::Disconnected:
      return figurePath + QStringLiteral("/state_disconnected.png");
    case TopologyWidget::StatusIcon::Normal:
      return figurePath + QStringLiteral("/state_normal.png");
    case TopologyWidget::StatusIcon::Fault:
      return figurePath + QStringLiteral("/state_fault.png");
  }
  return figurePath + QStringLiteral("/state_disconnected.png");
}

constexpr std::array<QPointF, 31> kBodyLayout = {
    QPointF(0.50, 0.00), QPointF(0.50, 0.08), QPointF(0.28, 0.22), QPointF(0.34, 0.22),
    QPointF(0.40, 0.22), QPointF(0.60, 0.22), QPointF(0.66, 0.22), QPointF(0.72, 0.22),
    QPointF(0.28, 0.32), QPointF(0.28, 0.42), QPointF(0.28, 0.52), QPointF(0.28, 0.62),
    QPointF(0.72, 0.32), QPointF(0.72, 0.42), QPointF(0.72, 0.52), QPointF(0.72, 0.62),
    QPointF(0.50, 0.35), QPointF(0.47, 0.43), QPointF(0.53, 0.43), QPointF(0.42, 0.50),
    QPointF(0.42, 0.60), QPointF(0.42, 0.70), QPointF(0.42, 0.80), QPointF(0.42, 0.90),
    QPointF(0.42, 1.00), QPointF(0.58, 0.50), QPointF(0.58, 0.60), QPointF(0.58, 0.70),
    QPointF(0.58, 0.80), QPointF(0.58, 0.90), QPointF(0.58, 1.00)};

/** 화면 위치별 모듈 인덱스 0…30 (이전 M1…M31에서 −1). */
constexpr std::array<int, 31> kModuleNumberByPosition = {
    1,  0,  7,  6,  5,  12, 13, 14, 8,  9,  10, 11, 15, 16, 17, 18, 2,  3,  4,
    19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30};

QVector<QPoint> buildModuleCenters(const QRect &panel, int nodeRadius) {
  QVector<QPoint> modules;
  modules.reserve(static_cast<int>(kBodyLayout.size()));

  qreal minX = kBodyLayout.front().x();
  qreal maxX = kBodyLayout.front().x();
  qreal minY = kBodyLayout.front().y();
  qreal maxY = kBodyLayout.front().y();
  for (const QPointF &pt : kBodyLayout) {
    minX = std::min(minX, pt.x());
    maxX = std::max(maxX, pt.x());
    minY = std::min(minY, pt.y());
    maxY = std::max(maxY, pt.y());
  }

  const qreal spanX = std::max<qreal>(maxX - minX, 1e-6);
  const qreal spanY = std::max<qreal>(maxY - minY, 1e-6);
  const QRect safeRect = panel.adjusted(nodeRadius, nodeRadius, -nodeRadius, -nodeRadius);
  for (const QPointF &pt : kBodyLayout) {
    const qreal nx = (pt.x() - minX) / spanX;
    const qreal ny = (pt.y() - minY) / spanY;
    const int x = safeRect.left() + static_cast<int>(nx * safeRect.width());
    const int y = safeRect.top() + static_cast<int>(ny * safeRect.height());
    modules.append(QPoint(x, y));
  }
  return modules;
}
}  // namespace

TopologyWidget::TopologyWidget(QWidget *parent) : QWidget(parent) {
  setMinimumSize(320, 420);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  setAutoFillBackground(true);
  m_moduleStatus.fill(StatusIcon::Disconnected);
}

void TopologyWidget::ensureStatusPixmaps() {
  if (m_pixmapsLoaded) {
    return;
  }
  m_pixDisconnected = QPixmap(statusImagePath(StatusIcon::Disconnected));
  m_pixNormal = QPixmap(statusImagePath(StatusIcon::Normal));
  m_pixFault = QPixmap(statusImagePath(StatusIcon::Fault));
  m_pixmapsLoaded = true;
}

const QPixmap &TopologyWidget::pixmapFor(StatusIcon icon) const {
  switch (icon) {
    case StatusIcon::Normal:
      return m_pixNormal;
    case StatusIcon::Fault:
      return m_pixFault;
    case StatusIcon::Disconnected:
    default:
      return m_pixDisconnected;
  }
}

void TopologyWidget::setStatusForAll(StatusIcon icon) {
  m_moduleStatus.fill(icon);
  update();
}

void TopologyWidget::setModuleStatus(int moduleId, StatusIcon icon) {
  if (moduleId < 0 || moduleId > 30) {
    return;
  }
  m_moduleStatus[static_cast<size_t>(moduleId)] = icon;
  update();
}

void TopologyWidget::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);

  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.fillRect(rect(), QColor(245, 247, 250));

  const QRect panel = rect().adjusted(14, 14, -14, -14);
  p.setPen(QPen(QColor(180, 188, 200), 1));
  p.setBrush(Qt::NoBrush);
  p.drawRoundedRect(panel, 10, 10);

  constexpr int kNodeDiameter = 36;
  constexpr int kNodeRadius = kNodeDiameter / 2;

  const QPoint core(panel.center().x(), panel.center().y());
  const QVector<QPoint> modules = buildModuleCenters(panel, kNodeRadius);

  p.setPen(QPen(QColor(70, 90, 120), 2));
  const std::array<std::pair<int, int>, 31> links = {{
      {0, 1},
      {2, 3},
      {3, 4},
      {5, 6},
      {6, 7},
      {2, 8},
      {8, 9},
      {9, 10},
      {10, 11},
      {7, 12},
      {12, 13},
      {13, 14},
      {14, 15},
      {16, 17},
      {17, 18},
      {19, 20},
      {20, 21},
      {21, 22},
      {22, 23},
      {23, 24},
      {24, 25},
      {25, 26},
      {26, 27},
      {27, 28},
      {28, 29},
      {29, 30},
  }};
  for (const auto &edge : links) {
    p.drawLine(modules[edge.first], modules[edge.second]);
  }

  int topologyCenterX = core.x();
  if (!modules.isEmpty()) {
    int minMx = modules.constFirst().x();
    int maxMx = minMx;
    for (const QPoint &pt : modules) {
      minMx = std::min(minMx, pt.x());
      maxMx = std::max(maxMx, pt.x());
    }
    topologyCenterX = (minMx + maxMx) / 2 + 1;
  }

  const QString controllerLabel = QStringLiteral("PC");
  QFont controllerFont = p.font();
  controllerFont.setPointSize(10);
  const QFontMetrics controllerFm(controllerFont);
  constexpr int kControllerPadH = 14;
  constexpr int kControllerPadV = 10;
  const int controllerW =
      std::max(controllerFm.horizontalAdvance(controllerLabel) + 2 * kControllerPadH, 48);
  const int controllerH = std::max(controllerFm.height() + 2 * kControllerPadV, 36);
  // 가로: 모든 모듈(원)의 경계 박스 중심. 세로: 패널 core 기준 오프셋(픽셀)
  constexpr int kControllerOffsetBelowCenterY = -148;
  const QPoint controllerCenter(topologyCenterX, core.y() + kControllerOffsetBelowCenterY);
  const QRect controllerRect(controllerCenter.x() - controllerW / 2,
                             controllerCenter.y() - controllerH / 2, controllerW, controllerH);

  p.setPen(QPen(QColor(52, 73, 94), 2));
  p.setBrush(QColor(224, 232, 245));
  p.drawRoundedRect(controllerRect, 8, 8);
  p.setFont(controllerFont);
  p.setPen(QColor(40, 55, 80));
  p.drawText(controllerRect, Qt::AlignCenter, controllerLabel);

  p.setPen(QPen(QColor(70, 90, 120), 2));
  p.drawLine(QPoint(controllerRect.center().x(), controllerRect.top()), modules[1]);
  p.drawLine(QPoint(controllerRect.center().x(), controllerRect.bottom()), modules[16]);
  p.drawLine(QPoint(controllerRect.left(), controllerRect.center().y()), modules[4]);
  p.drawLine(QPoint(controllerRect.right(), controllerRect.center().y()), modules[5]);
  p.drawLine(QPoint(controllerRect.left(), controllerRect.bottom()), modules[19]);

  ensureStatusPixmaps();
  for (int i = 0; i < modules.size(); ++i) {
    const QPoint &pt = modules[i];
    const int modNum = kModuleNumberByPosition[static_cast<size_t>(i)];
    const StatusIcon st = m_moduleStatus[static_cast<size_t>(modNum)];
    const QPixmap &statusPixmap = pixmapFor(st);

    const QRect circleRect(pt.x() - kNodeRadius, pt.y() - kNodeRadius, kNodeDiameter,
                           kNodeDiameter);
    if (!statusPixmap.isNull()) {
      QPainterPath clipPath;
      clipPath.addEllipse(circleRect);
      p.save();
      p.setClipPath(clipPath);
      p.drawPixmap(circleRect,
                   statusPixmap.scaled(circleRect.size(), Qt::KeepAspectRatioByExpanding,
                                       Qt::SmoothTransformation));
      p.restore();
      p.setPen(QPen(QColor(120, 130, 145), 1));
      p.setBrush(Qt::NoBrush);
      p.drawEllipse(circleRect);
    } else {
      p.setPen(QColor(180, 50, 50));
      p.setBrush(QColor(245, 245, 245));
      p.drawEllipse(circleRect);
    }

    QFont f = p.font();
    f.setPointSize(8);
    p.setFont(f);
    p.setPen(QColor(255, 255, 255));
    p.drawText(circleRect, Qt::AlignCenter, QStringLiteral("M%1").arg(modNum));
  }
}

void TopologyWidget::mousePressEvent(QMouseEvent *event) {
  constexpr int kNodeDiameter = 60;
  constexpr int kNodeRadius = kNodeDiameter / 2;

  const QRect panel = rect().adjusted(14, 14, -14, -14);
  const QVector<QPoint> modules = buildModuleCenters(panel, kNodeRadius);
  const QPoint clickPos = event->pos();

  for (int i = 0; i < modules.size(); ++i) {
    const QPoint delta = clickPos - modules[i];
    if ((delta.x() * delta.x()) + (delta.y() * delta.y()) <= (kNodeRadius * kNodeRadius)) {
      emit moduleClicked(kModuleNumberByPosition[static_cast<size_t>(i)]);
      event->accept();
      return;
    }
  }

  QWidget::mousePressEvent(event);
}
