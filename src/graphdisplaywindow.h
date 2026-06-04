#pragma once

#include <QVector>
#include <QWidget>

#include "twincat_logging_buffer.h"

class QComboBox;
class QCustomPlot;

/**
 * 별도 창 실시간 그래프 (module_monitor DisplayWindow 패턴, QCustomPlot).
 * 상단: 모든 모듈 Position vs 시간, 하단: 선택 모듈·신호.
 */
class GraphDisplayWindow : public QWidget {
	Q_OBJECT

 public:
	explicit GraphDisplayWindow(QWidget *parent = nullptr);
	~GraphDisplayWindow() override;

	void updateDisplay(const QVector<double> &time,
										 const QVector<QVector<double>> &posByModule,
										 const QVector<double> &selX, const QVector<double> &selY);

	QComboBox *comboModule() const { return comboModule_; }
	QComboBox *comboData() const { return comboData_; }

	void setVisibleModuleRange(int firstModule, int lastModule);

 signals:
	void selectionChanged();

 private:
	QCustomPlot *plotAllPosition_ = nullptr;
	QCustomPlot *plotSelected_ = nullptr;
	QComboBox *comboModule_ = nullptr;
	QComboBox *comboData_ = nullptr;
	int visibleFirstModule_ = 0;
	int visibleLastModule_ = 30;
};
