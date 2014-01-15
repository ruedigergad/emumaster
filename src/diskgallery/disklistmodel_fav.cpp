#include "disklistmodel.h"
#include <base/pathmanager.h>
#include <QDataStream>

void DiskListModel::loadFav()
{
	QFile file(QString("%1/favourites").arg(pathManager.userDataDirPath()));
	if (!file.open(QIODevice::ReadOnly))
		return;
	QDataStream stream(&file);
	stream >> m_favList;
	stream >> m_favListEmu;
}

void DiskListModel::saveFav()
{
	QFile file(QString("%1/favourites").arg(pathManager.userDataDirPath()));
	if (!file.open(QIODevice::WriteOnly|QIODevice::Truncate))
		return;
	QDataStream stream(&file);
	stream << m_favList;
	stream << m_favListEmu;
}

void DiskListModel::addToFav(int i)
{
	Q_ASSERT(m_collection != "fav");
	QString fileName = getDiskFileName(i);
	QString emuName = getDiskEmuName(i);
	if (emuName.isEmpty())
		return;
	int emuId = pathManager.emus().indexOf(emuName);

	int favIndex = m_favList.indexOf(fileName);
	if (favIndex >= 0) {
		if (emuId == m_favListEmu.at(favIndex))
			return;
	}
	for (i = 0; i < m_favList.size(); i++) {
		if (fileName < m_favList.at(i))
			break;
	}
	m_favList.insert(i, fileName);
	m_favListEmu.insert(i, emuId);
	saveFav();
}

void DiskListModel::removeFromFav(int i)
{
	if (m_collection != "fav") {
		QString fileName = getDiskFileName(i);
		QString emuName = getDiskEmuName(i);
		if (emuName.isEmpty())
			return;

		int emuId = pathManager.emus().indexOf(emuName);
		i = m_favList.indexOf(fileName);
		if (i < 0)
			return;
		if (emuId != m_favListEmu.at(i))
			return;
	}
	if (i < 0 || i >= m_favList.size())
		return;

	bool favModelCurrent = (m_collection == "fav");
	if (favModelCurrent)
		beginRemoveRows(QModelIndex(), i, i);
	m_favList.removeAt(i);
	m_favListEmu.removeAt(i);
	if (favModelCurrent) {
		m_list = m_favList;
		m_listEmu = m_favListEmu;
		endRemoveRows();
	}
	saveFav();
}

bool DiskListModel::diskInFavExists(int i)
{
	QString fileName = getDiskFileName(i);
	QString emuName = getDiskEmuName(i);
	if (emuName.isEmpty())
		return false;

	int emuId = pathManager.emus().indexOf(emuName);
	int favIndex = m_favList.indexOf(fileName);
	if (favIndex >= 0) {
		if (emuId == m_favListEmu.at(favIndex))
			return true;
	}
	return false;
}
