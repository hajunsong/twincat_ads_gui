#include "graphdisplaywindow.h"

#include "qcustomplot.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace {
/** mainwindow.cpp MotorStTableCol 과 동일해야 콤보 → 테이블 열 매핑이 맞음 */
constexpr int kColErr = 2;
constexpr int kColMode = 3;
constexpr int kColWc = 4;
constexpr int kColPos = 5;
constexpr int kColVel = 6;
constexpr int kColTrq = 7;
constexpr int kColTgtPos = 8;
constexpr int kColFolErr = 9;
}  // namespace

GraphDisplayWindow::GraphDisplayWindow(QWidget *parent)
		: QWidget(parent, Qt::Window) {
	setWindowTitle(tr("Graph View"));
	setMinimumSize(480, 400);
	resize(720, 560);

	auto *mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(8, 8, 8, 8);

	auto *topBar = new QHBoxLayout();
	auto *cbLegend = new QCheckBox(tr("Show legend (M0–M30)"), this);
	cbLegend->setChecked(false);
	topBar->addWidget(cbLegend);
	topBar->addStretch();
	mainLayout->addLayout(topBar);

	plotAllPosition_ = new QCustomPlot(this);
	plotAllPosition_->setMinimumHeight(200);
	plotAllPosition_->addGraph();
	const int nMot = static_cast<int>(kTwinCatLogMotorCount);
	for (int i = 1; i < nMot; ++i) {
		plotAllPosition_->addGraph();
	}
	plotAllPosition_->xAxis->setLabel(tr("Time (s)"));
	plotAllPosition_->yAxis->setLabel(tr("Position"));
	plotAllPosition_->xAxis->setRange(0, 10);
	plotAllPosition_->legend->setVisible(false);
	plotAllPosition_->legend->setFont(QFont(QStringLiteral("Arial"), 8));
	for (int i = 0; i < nMot; ++i) {
		QPen p;
		p.setColor(QColor::fromHsv((i * 360 / nMot) % 360, 200, 200));
		plotAllPosition_->graph(i)->setPen(p);
		plotAllPosition_->graph(i)->setName(tr("M%1").arg(i));
	}
	connect(cbLegend, &QCheckBox::toggled, this, [this](bool on) {
		if (plotAllPosition_) {
			plotAllPosition_->legend->setVisible(on);
			plotAllPosition_->replot();
		}
	});
	mainLayout->addWidget(plotAllPosition_);

	auto *comboLayout = new QHBoxLayout();
	comboLayout->addWidget(new QLabel(tr("Module:")));
	comboModule_ = new QComboBox(this);
	for (int m = 0; m < nMot; ++m) {
		comboModule_->addItem(tr("M%1").arg(m), m);
	}
	comboLayout->addWidget(comboModule_);
	comboLayout->addWidget(new QLabel(tr("Data:")));
	comboData_ = new QComboBox(this);
	comboData_->addItem(tr("Mode (disp)"), kColMode);
	comboData_->addItem(tr("Position"), kColPos);
	comboData_->addItem(tr("Velocity"), kColVel);
	comboData_->addItem(tr("Torque"), kColTrq);
	comboData_->addItem(tr("Error code"), kColErr);
	comboData_->addItem(tr("Following error"), kColFolErr);
	comboData_->addItem(tr("Target position"), kColTgtPos);
	comboData_->addItem(tr("WC (raw)"), kColWc);
	comboLayout->addWidget(comboData_);
	comboLayout->addStretch();
	mainLayout->addLayout(comboLayout);

	plotSelected_ = new QCustomPlot(this);
	plotSelected_->setMinimumHeight(200);
	plotSelected_->addGraph();
	plotSelected_->xAxis->setLabel(tr("Time (s)"));
	plotSelected_->yAxis->setLabel(tr("Value"));
	plotSelected_->xAxis->setRange(0, 10);
	mainLayout->addWidget(plotSelected_);

	connect(comboModule_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
					&GraphDisplayWindow::selectionChanged);
	connect(comboData_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
					&GraphDisplayWindow::selectionChanged);
}

GraphDisplayWindow::~GraphDisplayWindow() = default;

void GraphDisplayWindow::setVisibleModuleRange(int firstModule, int lastModule) {
	visibleFirstModule_ = qBound(0, firstModule, static_cast<int>(kTwinCatLogMotorCount) - 1);
	visibleLastModule_ =
			qBound(0, lastModule, static_cast<int>(kTwinCatLogMotorCount) - 1);
	if (visibleFirstModule_ > visibleLastModule_) {
		qSwap(visibleFirstModule_, visibleLastModule_);
	}

	if (comboModule_) {
		comboModule_->blockSignals(true);
		comboModule_->clear();
		for (int m = visibleFirstModule_; m <= visibleLastModule_; ++m) {
			comboModule_->addItem(tr("M%1").arg(m), m);
		}
		comboModule_->blockSignals(false);
	}

	const int nMot = static_cast<int>(kTwinCatLogMotorCount);
	if (plotAllPosition_) {
		for (int i = 0; i < nMot; ++i) {
			const bool visible = (i >= visibleFirstModule_ && i <= visibleLastModule_);
			plotAllPosition_->graph(i)->setVisible(visible);
		}
		plotAllPosition_->replot();
	}
	emit selectionChanged();
}

void GraphDisplayWindow::updateDisplay(const QVector<double> &time,
																			 const QVector<QVector<double>> &posByModule,
																			 const QVector<double> &selX,
																			 const QVector<double> &selY) {
	if (!plotAllPosition_ || !plotSelected_) {
		return;
	}

	const double xRangeLen = 10.0;
	const double xMax = time.isEmpty() ? 10.0 : time.last() + 0.5;
	const double xMin = qMax(0.0, xMax - xRangeLen);

	const int n = qMin(static_cast<int>(kTwinCatLogMotorCount), posByModule.size());
	for (int j = 0; j < n; ++j) {
		if (j >= visibleFirstModule_ && j <= visibleLastModule_) {
			plotAllPosition_->graph(j)->setData(time, posByModule[j]);
		} else {
			plotAllPosition_->graph(j)->data()->clear();
		}
	}
	plotAllPosition_->rescaleAxes(true);
	plotAllPosition_->xAxis->setRange(xMin, xMax);
	plotAllPosition_->replot();

	plotSelected_->graph(0)->setData(selX, selY);
	if (!selX.isEmpty()) {
		plotSelected_->rescaleAxes(true);
		double yMin = plotSelected_->yAxis->range().lower;
		double yMax = plotSelected_->yAxis->range().upper;
		if (qAbs(yMax - yMin) < 1e-9) {
			const double c = (yMax + yMin) * 0.5;
			plotSelected_->yAxis->setRange(c - 1, c + 1);
		}
		const double selSpan = selX.last() - selX.first();
		if (selSpan < 9.0) {
			plotSelected_->xAxis->setRange(qMax(0.0, selX.first() - 0.2), selX.last() + 0.5);
		} else {
			plotSelected_->xAxis->setRange(xMin, xMax);
		}
	} else {
		plotSelected_->yAxis->setRange(0, 1);
		plotSelected_->xAxis->setRange(xMin, xMax);
	}
	plotSelected_->replot();
}
