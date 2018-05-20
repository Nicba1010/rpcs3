#pragma once

#include <QDialog>
#include <QProgressDialog>
#include <QTimer>
#include <QPushButton>
#include <QTemporaryDir>
#include <QProcess>
#include <QDirIterator>

#include <QJsonObject>

#include <QNetworkAccessManager>
#include <QNetworkReply>

#include <memory>
#include <qapplication.h>

static const QStringList forbidden_directories =
{
	"dev_hdd0", "dev_hdd1", "data", "dev_flash", "dev_usb000", "shaderlog"
};
static const QString deprecated_extension = "rpcs3-deprecated";
static const QString api                  = "https://update.rpcs3.net/?c=XXXXXXXX";

namespace Ui
{
	class rpcs3_updater;
}

class rpcs3_updater : public QDialog
{
Q_OBJECT

public:
	explicit rpcs3_updater(QWidget* parent = Q_NULLPTR);

private:
	bool ReadJSON(QByteArray data);
	QString SaveFile(QNetworkReply* network_reply);
	void ShowDownloadProgress(const QString& message);
	void Update(const QString& path);
	static QByteArray GetFileHash(QFile* file, QCryptographicHash::Algorithm algorithm = QCryptographicHash::Algorithm::Md5);
	static void CleanUp(const QDir& directory = QDir(qApp->applicationDirPath()));
	void UpdateFiles();

	Ui::rpcs3_updater* ui;

	std::unique_ptr<QNetworkAccessManager> network_access_manager;
	std::unique_ptr<QTemporaryDir> extraction_directory, download_directory;
	std::unique_ptr<QProcess> extract_process;
	QNetworkReply* network_reply;

private Q_SLOTS:
	void on_about();
	void on_cancel();
	void on_download();
	void on_download_finished();
	void on_update();
	void on_update_finished();
	void on_error_occured(QProcess::ProcessError error);
};
