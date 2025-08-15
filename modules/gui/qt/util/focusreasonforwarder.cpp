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
#include "focusreasonforwarder.hpp"

#include <QEvent>
#include <QFocusEvent>

FocusReasonForwarder::FocusReasonForwarder(QObject *parent)
    : QObject{parent}
{
    if (parent)
        setTarget(parent);
}

FocusReasonForwarder::~FocusReasonForwarder()
{
    if (m_target)
        m_target->removeEventFilter(this);
}

QObject *FocusReasonForwarder::target() const
{
    return m_target;
}

void FocusReasonForwarder::setTarget(QObject *newTarget)
{
    if (m_target == newTarget)
        return;

    if (m_target)
        m_target->removeEventFilter(this);

    m_target = newTarget;

    if (m_target)
        m_target->installEventFilter(this);

    emit targetChanged();
}

void FocusReasonForwarder::resetTarget()
{
    setTarget(nullptr);
}

bool FocusReasonForwarder::eventFilter(QObject *, QEvent *event)
{
    assert(event);
    const auto eventType = event->type();
    if (eventType == QEvent::FocusAboutToChange ||
        eventType == QEvent::FocusIn ||
        eventType == QEvent::FocusOut)
    {
        const auto ev = static_cast<QFocusEvent*>(event);
        const Qt::FocusReason newFocusReason = ev->reason();
        if (m_focusReason != newFocusReason)
        {
            m_focusReason = newFocusReason;
            emit focusReasonChanged();
        }
    }
    return false;
}

Qt::FocusReason FocusReasonForwarder::focusReason() const
{
    return m_focusReason;
}
