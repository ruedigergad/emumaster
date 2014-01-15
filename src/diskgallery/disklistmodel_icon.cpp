#include "disklistmodel.h"
#include "diskimageprovider.h"
#include <base/pathmanager.h>
#include <QPainter>
#include <QTextStream>

QImage DiskListModel::applyMaskAndOverlay(const QImage &icon)
{
	QString iconMaskPath = QString("%1/data/icon_mask.png")
			.arg(pathManager.installationDirPath());
	QString iconOverlayPath = QString("%1/data/icon_overlay.png")
			.arg(pathManager.installationDirPath());

	QImage iconConverted = icon.convertToFormat(QImage::Format_ARGB32);
	QImage result(80, 80, QImage::Format_ARGB32);
	QImage mask;
	mask.load(iconMaskPath);
	mask = mask.convertToFormat(QImage::Format_ARGB32);
	int pixelCount = mask.width() * mask.height();
	QRgb *data = (QRgb *)mask.bits();
	for (int i = 0; i < pixelCount; ++i) {
		uint val = data[i];
		uint r = qRed(val);
		uint g = qGreen(val);
		uint b = qBlue(val);
		uint a = (r+g+b)/3;
		data[i] = qRgba(r, g, b, a);
	}

	QImage overlay;
	overlay.load(iconOverlayPath);
	overlay = overlay.convertToFormat(QImage::Format_ARGB32);

	QPainter painter;
	painter.begin(&result);
	painter.setRenderHints(QPainter::SmoothPixmapTransform|QPainter::HighQualityAntialiasing);
	painter.fillRect(result.rect(), Qt::transparent);
	painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
	painter.drawImage(0, 0, iconConverted);
	painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
	painter.drawImage(0, 0, mask);
	painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
	painter.drawImage(0, 0, overlay);
	painter.end();
	return result;
}

bool DiskListModel::addIconToHomeScreen(int i, qreal scale, int x, int y)
{
	QString diskFileName = getDiskFileName(i);
	QString diskTitle = getDiskTitle(i);
	QString diskEmuName = getDiskEmuName(i);
	DiskImageProvider imgProvider;
	QImage imgSrc = imgProvider.requestImage(QString("%1/%2*%3")
											 .arg(diskEmuName)
											 .arg(diskTitle)
											 .arg(qrand()), 0, QSize());
	QImage scaled = imgSrc.scaled(qreal(imgSrc.width())*scale,
								  qreal(imgSrc.height())*scale,
								  Qt::IgnoreAspectRatio,
								  Qt::SmoothTransformation);
	QImage icon = scaled.copy(x, y, 80, 80);
	if (icon.width() != 80 || icon.height() != 80)
		return false;
	icon = applyMaskAndOverlay(icon);

	QString desktopFilePath = pathManager.desktopFilePath(diskEmuName, diskTitle);
	QString iconFilePath = pathManager.homeScreenIconPath(diskEmuName, diskTitle);
	if (!icon.save(iconFilePath))
		return false;
	QString exec = QString(
		#if defined(MEEGO_EDITION_HARMATTAN)
			 "/usr/bin/single-instance %1/bin/%2 \"%3\"")
		#elif defined(Q_WS_MAEMO_5)
			"%1/bin/%2 \"%3\"")
		#endif
			.arg(pathManager.installationDirPath())
			.arg(diskEmuName)
			.arg(diskFileName);
	return createDesktopFile(desktopFilePath,
							 diskTitle,
							 exec,
							 iconFilePath,
							 "Game;Emulator;");
}

bool DiskListModel::createDesktopFile(const QString &fileName,
									 const QString &title,
									 const QString &exec,
									 const QString &icon,
									 const QString &categories)
{
	QString desktopFileContent = QString(
				"[Desktop Entry]\n"
				"Version=1.0\n"
				"Type=Application\n"
				"Name=%1\n"
				"Exec=%2\n"
				"Icon=%3\n"
				"Terminal=false\n"
				"Categories=%4\n")
			.arg(title)
			.arg(exec)
			.arg(icon)
			.arg(categories);

	QFile file(fileName);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
		return false;
	QTextStream out(&file);
	out << desktopFileContent;
	file.close();
	return true;
}

void DiskListModel::removeIconFromHomeScreen(int i)
{
	QString diskTitle = getDiskTitle(i);
	QString diskEmuName = getDiskEmuName(i);
	if (diskEmuName.isEmpty())
		return;

	QFile desktopFile(pathManager.desktopFilePath(diskEmuName, diskTitle));
	QFile iconFile(pathManager.homeScreenIconPath(diskEmuName, diskTitle));
	desktopFile.remove();
	iconFile.remove();
}

bool DiskListModel::iconInHomeScreenExists(int i)
{
	QString diskTitle = getDiskTitle(i);
	QString diskEmuName = getDiskEmuName(i);
	if (diskEmuName.isEmpty())
		return false;
	QString path = pathManager.desktopFilePath(diskEmuName, diskTitle);
	return QFile::exists(path);
}

