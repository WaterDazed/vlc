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
#ifndef FOCUSREASONFORWARDER_HPP
#define FOCUSREASONFORWARDER_HPP

#include <QObject>
#include <QQmlEngine>
#include <QPointer>

// This class is an ad hoc solution for getting the last focus reason of a QQuickItem,
// without using private headers. When Qt makes the focus reason public in QQuickItem,
// similar to QQuickControl, this class will serve no more purpose.
// This class can be useful in focus scopes (such as views ListView, GridView, ...),
// if a delegate or a leaf item gets the active focus but not the focus reason of the
// focus scope.

// An example usage would be as follows:
// FocusScope {
//   readonly property int focusReason: FocusReasonForwarder.focusReason
// }
// Refrain using this within the delegate directly, as that would unnecessarily create
// as many FocusReasonForwarder as delegate instances, as well as signal connections.
// Instead, use it within the target when applicable.
class FocusReasonForwarder : public QObject
{
    Q_OBJECT

    // NOTE: target is the parent object by default, unless overridden
    Q_PROPERTY(QObject* target READ target WRITE setTarget RESET resetTarget NOTIFY targetChanged FINAL)
    Q_PROPERTY(Qt::FocusReason focusReason READ focusReason NOTIFY focusReasonChanged FINAL)

    QML_ELEMENT
    QML_ATTACHED(FocusReasonForwarder)

public:
    static FocusReasonForwarder *qmlAttachedProperties(QObject *object)
    {
        return new FocusReasonForwarder(object);
    }

    explicit FocusReasonForwarder(QObject *parent = nullptr);
    virtual ~FocusReasonForwarder();

    QObject *target() const;
    void setTarget(QObject *newTarget);
    void resetTarget();

    bool eventFilter(QObject *watched, QEvent *event) override;

    Qt::FocusReason focusReason() const;

signals:
    void targetChanged();
    void focusReasonChanged();

private:
    QPointer<QObject> m_target;
    Qt::FocusReason m_focusReason = Qt::NoFocusReason;
};

#endif // FOCUSREASONFORWARDER_HPP
