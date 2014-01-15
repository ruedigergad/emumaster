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

#include "profiler.h"

NesProfiler::NesProfiler(const QList<ProfilerItem> &items, QObject *parent) :
	QAbstractListModel(parent),
	m_items(items)
{
	QHash<int, QByteArray> roles;
	roles.insert(PcRole, "pc");
	roles.insert(CountRole, "cnt");
	setRoleNames(roles);
}

int NesProfiler::rowCount(const QModelIndex &parent) const
{
	Q_UNUSED(parent)
	return m_items.size();
}

void NesProfiler::reset()
{
	QAbstractListModel::reset();
}

QVariant NesProfiler::data(const QModelIndex &index, int role) const
{
	if (role == PcRole) {
		return QString("%1").arg(m_items.at(index.row()).first,
								 4, 16, QLatin1Char('0'));
	} else if (role == CountRole) {
		return m_items.at(index.row()).second;
	}
	return QVariant();
}
