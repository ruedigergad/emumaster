/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "disklistmodel.h"
#include <base/pathmanager.h>
#include <QFileInfo>
#include <QSettings>
#include <QUrl>
#include <QProcess>
#include <QVector>
#include <QFont>
#include <QFontMetrics>

DiskListModel::DiskListModel(QObject *parent) :
	QAbstractListModel(parent),
	m_screenShotUpdateCounter(0)
{
	_roles.insert(TitleRole, "title");
	_roles.insert(TitleElidedRole, "titleElided");
	_roles.insert(EmuNameRole, "emuName");
	_roles.insert(AlphabetRole, "alphabet");
	_roles.insert(ImageSourceRole, "imageSource");
	_roles.insert(ScreenShotUpdateRole, "screenShotUpdate");

	setupFilters();
	loadFav();

	QFont font;
	font.setFamily("Nokia Pure Text");
	font.setPointSize(18);

	m_fontMetrics = new QFontMetrics(font);
}

void DiskListModel::setCollection(const QString &name)
{
	// do not compare with m_collection with name - needed for search cleaning
	beginResetModel();
	m_fullList.clear();
	m_fullListEmu.clear();
	m_collection = name;

	if (m_collection == "fav")
		setCollectionFav();
	else if (!m_collection.isEmpty())
		setCollectionEmu();

	m_list = m_fullList;
	m_listEmu = m_fullListEmu;

	endResetModel();
	emit collectionChanged();
}

void DiskListModel::setCollectionEmu()
{
	QDir dir(pathManager.diskDirPath(m_collection));
	DiskFilter diskFilter = m_diskFilters.value(m_collection);

	m_fullList = dir.entryList(diskFilter.included,
							   QDir::Files,
							   QDir::Name|QDir::IgnoreCase);
	if (diskFilter.includeDirs)
		includeSubDirs(dir);

	QStringList excluded;
	for (int i = 0; i < diskFilter.excluded.size(); i++)
		excluded.append(m_fullList.filter(diskFilter.excluded.at(i)));
	for (int i = 0; i < excluded.size(); i++)
		m_fullList.removeOne(excluded.at(i));

	int emuId = pathManager.emus().indexOf(m_collection);
	m_fullListEmu = QList<int>::fromVector(QVector<int>(m_fullList.size(), emuId));
}

void DiskListModel::setCollectionFav()
{
	m_fullList = m_favList;
	m_fullListEmu = m_favListEmu;
}

static bool caseInsensitiveLessThan(const QString &s1, const QString &s2)
{
	return s1.toLower() < s2.toLower();
}

void DiskListModel::includeSubDirs(QDir &dir)
{
	QStringList subdirs = dir.entryList(QDir::Dirs|QDir::NoDotAndDotDot);
	m_fullList += subdirs;
	qSort(m_fullList.begin(), m_fullList.end(), caseInsensitiveLessThan);
}

QVariant DiskListModel::data(const QModelIndex &index, int role) const
{
	if (role == TitleRole) {
		return getDiskTitle(index.row());
	} else if (role == TitleElidedRole) {
		return getDiskTitleElided(index.row());
	} else if (role == EmuNameRole) {
		return getDiskEmuName(index.row());
	} else if (role == AlphabetRole) {
		QString title = getDiskTitle(index.row());
		return title.isEmpty() ? QVariant() : title.at(0).toUpper();
	} else if (role == ImageSourceRole) {
		QString emuName = getDiskEmuName(index.row());
		QString title = getDiskTitle(index.row());
		int screenShotUpdate = getScreenShotUpdate(index.row());
		return QString("image://disk/%1/%2*%3").arg(emuName).arg(title)
				.arg(screenShotUpdate);
	} else if (role == ScreenShotUpdateRole) {
		return getScreenShotUpdate(index.row());
	}
	return QVariant();
}

int DiskListModel::rowCount(const QModelIndex &parent) const
{
	Q_UNUSED(parent)
	return count();
}

int DiskListModel::count() const
{
	return m_list.size();
}

QString DiskListModel::getAlphabet(int i) const
{
	if (i < 0 || i >= m_list.size())
		return QString();
	return m_list.at(i).at(0).toUpper();
}

QString DiskListModel::getDiskTitle(int i) const
{
	if (i < 0 || i >= m_list.size())
		return QString();
	return QFileInfo(m_list.at(i)).completeBaseName();
}

QString DiskListModel::getDiskTitleElided(int i) const
{
	QString title = getDiskTitle(i);
	if (title.size() < 20)
		return title;
	return m_fontMetrics->elidedText(title, Qt::ElideRight, 270);
}

QString DiskListModel::getDiskFileName(int i) const
{
	if (i < 0 || i >= m_list.size())
		return QString();
	return m_list.at(i);
}

QString DiskListModel::getDiskEmuName(int i) const
{
	if (i < 0 || i >= m_list.size())
		return QString();
	int emuId = m_listEmu.at(i);
	return pathManager.emus().at(emuId);
}

void DiskListModel::updateScreenShot(const QString &name)
{
	int i = m_list.indexOf(name);
	if (i < 0)
		return;
	m_screenShotUpdateCounter++;
	emit dataChanged(index(i), index(i));
}

int DiskListModel::getScreenShotUpdate(int i) const
{
	QString diskTitle = getDiskTitle(i);
	QString diskEmuName = getDiskEmuName(i);
	if (diskTitle.isEmpty())
		return -1;
	QString path = pathManager.screenShotPath(diskEmuName, diskTitle);
	if (QFile::exists(path))
		return m_screenShotUpdateCounter;
	else
		return -1;
}

void DiskListModel::trash(int i)
{
	QString title = getDiskTitle(i);
	QString fileName = getDiskFileName(i);
	QString emu = getDiskEmuName(i);
	if (emu.isEmpty())
		return;

	beginRemoveRows(QModelIndex(), i, i);
	// delete the disk
	QStringList args;
	args << "-R";
	args << QString("%1/%2")
			.arg(pathManager.diskDirPath(emu))
			.arg(fileName);
	QProcess::startDetached("rm", args);
	// delete an icon from the home screen
	args.removeLast();
	args << pathManager.homeScreenIconPath(emu, title);
	QProcess::startDetached("rm", args);
	// delete stored states
	args.removeLast();
	args << pathManager.stateDirPath(emu, title);
	QProcess::startDetached("rm", args);
	// delete screenshot
	args.removeLast();
	args << pathManager.screenShotPath(emu, title);
	QProcess::startDetached("rm", args);
	// delete from fav
	int favIndex = m_favList.indexOf(fileName);
	if (favIndex >= 0) {
		if (emu == pathManager.emus().at(m_favListEmu.at(favIndex))) {
			m_favList.removeAt(favIndex);
			m_favListEmu.removeAt(favIndex);
			saveFav();
		}
	}
	// delete from memory
	m_list.removeAt(i);
	if (i < m_listEmu.size())
		m_listEmu.removeAt(i);
	endRemoveRows();
}

void DiskListModel::setDiskCover(int i, const QUrl &coverUrl)
{
	QString coverPath = coverUrl.toLocalFile();
	if (!QFile::exists(coverPath))
		return;

	QString diskTitle = getDiskTitle(i);
	QString diskEmuName = getDiskEmuName(i);
	if (diskTitle.isEmpty())
		return;
	QString path = pathManager.screenShotPath(diskEmuName, diskTitle);

	QFile::remove(path);
	QFile::copy(coverPath, path);
	m_screenShotUpdateCounter++;
	emit dataChanged(index(i), index(i));
}

void DiskListModel::setupNesFilter()
{
	DiskFilter filter;
	filter.included << "*.nes";
	m_diskFilters.insert("nes", filter);
}

void DiskListModel::setupGbaFilter()
{
	DiskFilter filter;
	filter.included << "*.gba";
	m_diskFilters.insert("gba", filter);
}

void DiskListModel::setupSnesFilter()
{
	DiskFilter filter;
	filter.included << "*.smc";
	m_diskFilters.insert("snes", filter);
}

void DiskListModel::setupPsxFilter()
{
	DiskFilter filter;
	filter.included << "*.iso";
	filter.included << "*.bin";
	filter.included << "*.img";
	filter.excluded.append(QRegExp("scph*.bin", Qt::CaseSensitive, QRegExp::Wildcard));
	m_diskFilters.insert("psx", filter);
}

void DiskListModel::setupAmigaFilter()
{
	DiskFilter filter;
	filter.included << "*.adf";
	m_diskFilters.insert("amiga", filter);
}

void DiskListModel::setupPicoFilter()
{
	DiskFilter filter;
	filter.included << "*.gen";
	filter.included << "*.smd";
	filter.included << "*.bin";
	filter.included << "*.iso";
	filter.includeDirs = true;
	filter.excluded.append(QRegExp("*_scd*.bin", Qt::CaseSensitive, QRegExp::Wildcard));
	filter.excluded.append(QRegExp("*_mcd*.bin", Qt::CaseSensitive, QRegExp::Wildcard));
	m_diskFilters.insert("pico", filter);
}

void DiskListModel::setupFilters()
{
	setupNesFilter();
	setupGbaFilter();
	setupSnesFilter();
	setupPsxFilter();
	setupAmigaFilter();
	setupPicoFilter();
	// TODO on every new emulated system add a disk filter
}

void DiskListModel::setNameFilter(const QString &filter)
{
	beginResetModel();
	m_list.clear();
	m_listEmu.clear();
	for (int i = 0; i < m_fullList.size(); i++) {
		if (m_fullList.at(i).contains(filter, Qt::CaseInsensitive)) {
			m_list.append(m_fullList.at(i));
			m_listEmu.append(m_fullListEmu.at(i));
		}
	}
	endResetModel();
}
