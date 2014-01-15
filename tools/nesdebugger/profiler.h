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

#ifndef PROFILER_H
#define PROFILER_H

#include <base/emu.h>
#include <QAbstractListModel>

class ProfilerItem : public QPair<u16, int>
{
public:
	bool operator <(const ProfilerItem &i) const
	{
		if (second > i.second)
			return true;
		if (second == i.second && first < i.first)
			return true;
		return false;
	}
};

class NesProfiler : public QAbstractListModel
{
	Q_OBJECT
public:
	enum Role {
		PcRole = Qt::UserRole+1,
		CountRole
	};

	explicit NesProfiler(const QList<ProfilerItem> &items, QObject *parent = 0);
	int rowCount(const QModelIndex &parent) const;
	void reset();
	QVariant data(const QModelIndex &index, int role) const;
private:
	const QList<ProfilerItem> &m_items;
};

#endif // PROFILER_H
