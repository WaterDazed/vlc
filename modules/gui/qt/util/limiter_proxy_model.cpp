/*****************************************************************************
 * Copyright (C) 2025 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#include "limiter_proxy_model.hpp"

LimiterProxyModel::LimiterProxyModel(QObject *parent)
    : QSortFilterProxyModel{parent}
{

}

int LimiterProxyModel::maximumRowCount() const
{
    return m_maximumRowCount;
}

void LimiterProxyModel::setMaximumRowCount(int newMaximumRowCount)
{
    if (m_maximumRowCount == newMaximumRowCount)
        return;

#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    beginFilterChange();
#endif
    m_maximumRowCount = newMaximumRowCount;
    invalidateRowsFilter();

    emit maximumRowCountChanged();
}

int LimiterProxyModel::maximumColumnCount() const
{
    return m_maximumColumnCount;
}

void LimiterProxyModel::setMaximumColumnCount(int newMaximumColumnCount)
{
    if (m_maximumColumnCount == newMaximumColumnCount)
        return;

#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    beginFilterChange();
#endif
    m_maximumColumnCount = newMaximumColumnCount;
    invalidateColumnsFilter();

    emit maximumColumnCountChanged();
}

int LimiterProxyModel::rowCount(const QModelIndex &parent) const
{
    const auto c = QSortFilterProxyModel::rowCount(parent);
    if (m_maximumRowCount >= 0)
        return std::min(c, m_maximumRowCount);
    else
        return c;
}

int LimiterProxyModel::columnCount(const QModelIndex &parent) const
{
    const auto c = QSortFilterProxyModel::columnCount(parent);
    if (m_maximumColumnCount >= 0)
        return std::min(c, m_maximumColumnCount);
    else
        return c;
}

bool LimiterProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    if (m_maximumRowCount >= 0 && source_row >= m_maximumRowCount)
        return false;

    return QSortFilterProxyModel::filterAcceptsRow(source_row, source_parent);
}

bool LimiterProxyModel::filterAcceptsColumn(int source_column, const QModelIndex &source_parent) const
{
    if (m_maximumColumnCount >= 0 && source_column >= m_maximumColumnCount)
        return false;

    return QSortFilterProxyModel::filterAcceptsRow(source_column, source_parent);
}
