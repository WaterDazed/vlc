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
#ifndef LIMITER_PROXY_MODEL_HPP
#define LIMITER_PROXY_MODEL_HPP

#include <QSortFilterProxyModel>

class LimiterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

    // TODO: Qt 7: Switch to size type when Qt switches itself. Until then, negative value means no limitation.
    Q_PROPERTY(int maximumRowCount READ maximumRowCount WRITE setMaximumRowCount NOTIFY maximumRowCountChanged FINAL)
    Q_PROPERTY(int maximumColumnCount READ maximumColumnCount WRITE setMaximumColumnCount NOTIFY maximumColumnCountChanged FINAL)

public:
    explicit LimiterProxyModel(QObject *parent = nullptr);

    int maximumRowCount() const;
    void setMaximumRowCount(int newMaximumRowCount);
    int maximumColumnCount() const;
    void setMaximumColumnCount(int newMaximumColumnCount);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

signals:
    void maximumRowCountChanged();
    void maximumColumnCountChanged();

protected:
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;
    bool filterAcceptsColumn(int source_column, const QModelIndex &source_parent) const override;

private:
    int m_maximumRowCount = -1;
    int m_maximumColumnCount = -1;
};

#endif // LIMITER_PROXY_MODEL_HPP
