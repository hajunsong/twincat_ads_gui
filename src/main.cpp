#include "mainwindow.h"

#include "Log.h"

#include <QApplication>
#include <csignal>

int main(int argc, char *argv[]) {
	// AdsLib logs WARN when TwinCAT sends ADS notifications we did not subscribe to
	// ("No dispatcher found for notification"). Errors only keeps stderr readable.
	Logger::logLevel = 3;
	// Prevent process termination on socket broken pipe; handle ADS errors in return codes.
	std::signal(SIGPIPE, SIG_IGN);

	QApplication app(argc, argv);
	QApplication::setOrganizationName(QStringLiteral("TwinCatAdsGui"));
	QApplication::setApplicationName(QStringLiteral("twincat_ads_gui"));

	MainWindow w;
	w.show();
	return app.exec();
}
