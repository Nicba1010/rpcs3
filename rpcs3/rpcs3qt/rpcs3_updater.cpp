#include "rpcs3_updater.h"
#include "ui_rpcs3_updater.h"
#include <QMessageBox>
#include <QLabel>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QNetworkRequest>

rpcs3_updater::rpcs3_updater(QWidget* parent)
	: QDialog(parent), ui(new Ui::rpcs3_updater)
{
	//================================================================================
	// General
	//================================================================================

	ui->setupUi(this);

	network_access_manager.reset(new QNetworkAccessManager());
	network_access_manager->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);

	clean_up(qApp->applicationDirPath());

	//================================================================================
	// Menu
	//================================================================================

	// connect 'File->Exit' to quit the application
	connect(ui->actionExit, &QAction::triggered, this, &rpcs3_updater::close);

	// connect 'Help->About' to the about dialog
	connect(ui->actionAbout, &QAction::triggered, this, &rpcs3_updater::on_about);

	// connect 'Help->About Qt' to the pre shipped Qt about dialog
	connect(ui->actionAbout_Qt, &QAction::triggered, qApp, &QApplication::aboutQt);

	//================================================================================
	// Buttons
	//================================================================================

	connect(ui->updateButton, &QPushButton::clicked, this, &rpcs3_updater::on_update);
	connect(ui->downloadButton, &QPushButton::clicked, this, &rpcs3_updater::on_download);
	connect(ui->cancelButton, &QPushButton::clicked, this, &rpcs3_updater::on_cancel);
}

void rpcs3_updater::on_about()
{
}

void rpcs3_updater::on_cancel()
{
	if (!network_reply)
	{
		return;
	}

	network_reply->abort();
}

void rpcs3_updater::on_update()
{
	// Check for SSL and abort in case it is not supported
	if (!QSslSocket::supportsSsl())
	{
		QMessageBox::critical(nullptr, tr("Warning!"),
		                      tr("Can not retrieve the Update! Please make sure your system supports SSL."));
		return;
	}

	ui->cancelButton->setEnabled(true);
	ui->updateButton->setEnabled(false);
	ui->downloadButton->setEnabled(false);

	// Send network request and wait for response
	QNetworkRequest network_request = QNetworkRequest(QUrl(api));
	network_reply = network_access_manager->get(network_request);

	// Initialise and show progress bar
	show_download_progress(tr("Downloading build info."));

	// Handle response according to its contents
	connect(network_reply, &QNetworkReply::finished, this, &rpcs3_updater::on_update_finished);
}

void rpcs3_updater::on_update_finished() const
{
	if (!network_reply)
	{
		return;
	}

	// Handle Errors
	switch (network_reply->error())
	{
	case QNetworkReply::NoError:
		read_json(network_reply->readAll());
		ui->progressLabel->setText(tr("Build info retrieved"));
		break;
	case QNetworkReply::OperationCanceledError:
		ui->progressLabel->setText(tr("Build info canceled"));
		break;
	default:
		ui->progressLabel->setText(tr("Build info error"));
		QMessageBox::critical(nullptr, tr("Error!"), network_reply->errorString());
		break;
	}

	// Clean up network reply
	network_reply->deleteLater();

	ui->cancelButton->setEnabled(false);
	ui->updateButton->setEnabled(true);
	ui->downloadButton->setEnabled(network_reply->error() == QNetworkReply::NoError);
}

void rpcs3_updater::on_download()
{
	const QUrl latest(ui->download_data->text());

	if (!latest.isValid())
	{
		return;
	}

	ui->cancelButton->setEnabled(true);
	ui->updateButton->setEnabled(false);
	ui->downloadButton->setEnabled(false);

	const QNetworkRequest network_request = QNetworkRequest(latest);
	network_reply = network_access_manager->get(network_request);

	// Initialise and show progress bar
	show_download_progress(tr("Downloading latest build."));

	// Handle response according to its contents
	connect(network_reply, &QNetworkReply::finished, this, &rpcs3_updater::on_download_finished);
}

void rpcs3_updater::on_download_finished()
{
	if (!network_reply)
	{
		return;
	}

	// Handle Errors
	switch (network_reply->error())
	{
	case QNetworkReply::NoError:
		update(save_file(network_reply));
		ui->progressLabel->setText(tr("Download finished"));
		break;
	case QNetworkReply::OperationCanceledError:
		ui->progressLabel->setText(tr("Download canceled"));
		break;
	default:
		ui->progressLabel->setText(tr("Download error"));
		QMessageBox::critical(nullptr, tr("Error!"), network_reply->errorString());
		break;
	}

	ui->cancelButton->setEnabled(false);
	ui->updateButton->setEnabled(true);
	ui->downloadButton->setEnabled(true);

	// Clean up network reply
	network_reply->deleteLater();
}

bool rpcs3_updater::read_json(const QByteArray& data) const
{
	// Read JSON data
	const QJsonObject json_data = QJsonDocument::fromJson(data).object();
	const int return_code = json_data["return_code"].toInt();

	if (return_code < -1/*0*/)
	{
		// We failed to retrieve a new update
		QString error_message;

		switch (return_code)
		{
			//case -1:
			//	error_message = tr("Server Error - Internal Error");
			//	break;
		case -2: error_message = tr("Server Error - Maintenance Mode");
			break;
		default: error_message = tr("Server Error - Unknown Error");
			break;
		}

		QMessageBox::critical(nullptr, tr("Error code %0!").arg(return_code), error_message + "\n\n" + api);
		return false;
	}

	// Check for latest_build node
	if (!json_data["latest_build"].isObject())
	{
		QMessageBox::critical(nullptr, tr("Error!"), tr("No latest build found!"));
		return false;
	}

	const QJsonObject latest_build = json_data["latest_build"].toObject();

#ifdef _WIN32
	const QJsonObject os = latest_build["windows"].toObject();
#elif __linux__
	const QJsonObject os = latest_build["linux"].toObject();
#endif

	ui->pr_data->setText(latest_build.value("pr").toString());
	ui->datetime_data->setText(os.value("datetime").toString());
	ui->download_data->setText(os.value("download").toString());

	return true;
}

QString rpcs3_updater::save_file(QNetworkReply* network_reply)
{
	if (!network_reply)
	{
		return "";
	}

	download_directory.reset(new QTemporaryDir());

	const QString filename = download_directory->path() + "/" + network_reply->url().fileName();
	QFile file(filename);

	if (!file.open(QIODevice::WriteOnly))
	{
		QMessageBox::critical(
			nullptr,
			tr("Error!"),
			tr("Could not open %0 for writing: %1").arg(filename).arg(file.errorString())
		);
		return nullptr;
	}

	file.write(network_reply->readAll());
	file.close();

	return filename;
}

void rpcs3_updater::update(const QString& path)
{
	if (path.isEmpty())
	{
		return;
	}

#ifdef _WIN32
	extraction_directory.reset(new QTemporaryDir());
	extract_process.reset(new QProcess());

	connect(extract_process.get(), &QProcess::errorOccurred, this, &rpcs3_updater::on_error_occured);

	const QString file = QString(R"(./7za.exe x -aoa -o"%1" "%2")").arg(extraction_directory->path(), path);
	qDebug() << "Extraction file: " << file;
	qDebug() << "Extraction dir: " << extraction_directory->path();
	connect(extract_process.get(), QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
	        [=](const int exit_code, const QProcess::ExitStatus exit_status)
	        {
		        LOG_SUCCESS(GENERAL, "Extraction finished");

		        switch (exit_status)
		        {
		        case QProcess::NormalExit:
			        LOG_SUCCESS(GENERAL, "Extraction successfull, exit code: %d", exit_code);
			        qDebug() << extract_process->readAll();
			        if (update_files())
			        {
				        LOG_SUCCESS(GENERAL, "Successfully updated all files");
			        }
			        else
			        {
						LOG_ERROR(GENERAL, "Unable to update all files");
						// TODO: Revert
						QMessageBox::critical(nullptr, tr("Error!"), tr("All files were not updater, changes have been reverted!"));
			        }
			        break;
		        case QProcess::CrashExit:
		        default:
			        LOG_ERROR(GENERAL, "Extraction Error: %d", exit_code);
			        QMessageBox::critical(nullptr, tr("Error!"), tr("Error occurred during file extraction!"));
			        qDebug() << extract_process->readAll();
		        }

		        extract_process->close();

		        if (download_directory->isValid() && !download_directory->remove())
		        {
			        LOG_ERROR(GENERAL, "Could not remove download_directory!");
		        }

		        if (extraction_directory->isValid() && !extraction_directory->remove())
		        {
			        LOG_ERROR(GENERAL, "Could not remove extraction_directory!");
		        }
		        extract_process->deleteLater();
	        });
	extract_process->start(file);
	LOG_SUCCESS(GENERAL, "Extracting file: %s", QFileInfo(path).fileName());
#elif __linux__
	QProcessEnvironment env = QProcessEnvironment();
	if (!env.contains("APPIMAGE")) return;
	const QString app_image_path = env.value("APPIMAGE", nullptr);
	if (app_image_path != nullptr)
	{
		QMessageBox::critical(nullptr, tr("Error!"), tr("Unable to get AppImage path!"));
		return;
	}
	const QFileInfo info = QFileInfo(app_image_path);
	if (!info.exists())
	{
		QMessageBox::critical(nullptr, tr("Error!"), tr("AppImage path points to non existent file!"));
		return;
	}
	QFile current_app_image_file(app_image_path);
	QFile updated_app_image_file(path);
	if (!current_app_image_file.remove())
	{
		QMessageBox::critical(nullptr, tr("Error!"), tr("Unable to delete original AppImage file!"));
		return;
	}
	if (!updated_app_image_file.rename(app_image_path))
	{
		QMessageBox::critical(nullptr, tr("Error!"), tr("Unable to move updated AppImage file!!"));
		return;
	}
	QFileDevice::Permissions permissions = updated_app_image_file.permissions();
	permissions.setFlag(QFileDevice::Permission::ExeOwner);
	permissions.setFlag(QFileDevice::Permission::ExeGroup);
	permissions.setFlag(QFileDevice::Permission::ExeUser);
	permissions.setFlag(QFileDevice::Permission::ExeOther);
	if (updated_app_image_file.setPermissions(permissions))
	{
		// TODO: success
	}
	else
	{
		QMessageBox::critical(nullptr, tr("Error!"), tr("Unable to change permissions, please change permissions to a+x manually!"));
	}
#endif
}

bool rpcs3_updater::update_files() const
{
	QDirIterator it(extraction_directory->path(), QDirIterator::Subdirectories);
	while (it.hasNext())
	{
		const QFileInfo info = QFileInfo(it.next());
		if (!info.exists())
		{
			continue;
		}

		const QString old_path = info.absoluteFilePath().replace(extraction_directory->path(), qApp->applicationDirPath());

		if (info.isFile())
		{
			QFile new_file(info.absoluteFilePath());
			QFile old_file(old_path);

			if (get_file_hash(&new_file) != get_file_hash(&old_file))
			{
				if (old_file.rename(old_file.fileName() + "." + deprecated_extension) && new_file.rename(old_path))
				{
					LOG_SUCCESS(GENERAL, "Updated file: %s", old_file.fileName());
				}
				else
				{
					LOG_ERROR(GENERAL, "Could not update file: %s", old_file.fileName());
					return false;
				}
			}
		}
		else if (info.isDir())
		{
			QDir dir(old_path);
			if (!dir.exists() && !dir.mkpath(old_path))
			{
				LOG_ERROR(GENERAL, "Cannot create folder: %s", dir.dirName());
				return false;
			}
			else
			{
				LOG_SUCCESS(GENERAL, "Successfully created folder: %s", dir.dirName());
			}
		}
	}
	return true;
}

QByteArray rpcs3_updater::get_file_hash(QFile* file, const QCryptographicHash::Algorithm algorithm)
{
	if (!file || !file->exists())
	{
		return QByteArray();
	}

	file->open(QIODevice::ReadOnly);
	QByteArray hash = QCryptographicHash::hash(file->readAll(), algorithm);
	file->close();

	return hash;
}

void rpcs3_updater::clean_up(const QDir& directory)
{
	const QFileInfoList files = directory.entryInfoList(QDir::NoDotAndDotDot | QDir::Dirs | QDir::Files);

	for (const QFileInfo& file_info : files)
	{
		if (forbidden_directories.contains(file_info.fileName()) || !file_info.exists())
		{
			continue;
		}

		if (file_info.isDir())
		{
			clean_up(QDir(file_info.absoluteFilePath()));
			continue;
		}

		if (file_info.suffix().compare(deprecated_extension) == 0)
		{
			QFile deprecated_file(file_info.absoluteFilePath());

			if (deprecated_file.remove())
			{
				LOG_SUCCESS(GENERAL, "Successfully deleted deprecated file: %s", deprecated_file.fileName());
			}
			else
			{
				qDebug() << "Error occured while deleting deprecated file... Check your privilege!";
				qDebug() << "Error message: " << deprecated_file.errorString();
				qDebug() << "Offending file: " << deprecated_file;
				// TODO: Handle error?
			}
		}
	}
}

void rpcs3_updater::show_download_progress(const QString& message)
{
	ui->progressBar->setValue(0);
	ui->progressBar->setEnabled(true);

	ui->progressLabel->setText(message + tr(" Please wait..."));

	// Handle new progress
	connect(network_reply, &QNetworkReply::downloadProgress, [this](qint64 bytesReceived, qint64 bytesTotal)
	{
		ui->progressBar->setMaximum(bytesTotal);
		ui->progressBar->setValue(bytesReceived);
	});

	// Clean Up
	connect(network_reply, &QNetworkReply::finished, [this]()
	{
		ui->progressBar->setEnabled(false);

		if (network_reply && network_reply->error() == QNetworkReply::NoError)
		{
			QApplication::beep();
		}
	});
}

void rpcs3_updater::on_error_occured(const QProcess::ProcessError error) const
{
	QString error_message;
	switch (error)
	{
	case QProcess::FailedToStart: error_message = tr("Failed To Start");
		break;
	case QProcess::Crashed: error_message = tr("Crashed");
		break;
	case QProcess::Timedout: error_message = tr("Timed Out");
		break;
	case QProcess::ReadError: error_message = tr("Read Error");
		break;
	case QProcess::WriteError: error_message = tr("Write Error");
		break;
	case QProcess::UnknownError: error_message = tr("Unknown Error");
		break;
	default: error_message = tr("Unknown Error: %0").arg(error);
		break;
	}
	QMessageBox::critical(nullptr, tr("Error!"), error_message);
}
