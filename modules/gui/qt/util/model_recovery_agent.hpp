/*****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
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

#ifndef MODEL_RECOVERY_AGENT_HPP
#define MODEL_RECOVERY_AGENT_HPP

#include <QFile>
#include <QTimer>
#include <QSettings>
#include <QPointer>
#include <QFileInfo>
#include <QMessageBox>
#include <QTemporaryFile>
#include <QAbstractItemModel>
#include <QLockFile>

#include <cstdio>

#include "qt.hpp"

class ModelRecoveryAgent
{
    const QPointer<QSettings> m_settings;
    const QString m_key;
    QString m_recoveryFileName;
    QTimer m_timer;
    bool m_conditionDismissInitialDirtiness = false;
    std::unique_ptr<QLockFile> m_lockFile;

private:

    template<class T>
    ModelRecoveryAgent(class QSettings *settings,
                       const QString& modelIdentifier,
                       const QString& recoveryFileName,
                       bool conditionDismissInitialDirtiness,
                       T* model)
        : m_settings(settings)
        , m_key(modelIdentifier + QStringLiteral("/RecoveryFilePath"))
        , m_recoveryFileName(recoveryFileName)
        , m_conditionDismissInitialDirtiness(conditionDismissInitialDirtiness)
    {
        m_timer.setInterval(10000); // 10 seconds
        m_timer.setSingleShot(true);

        QObject::connect(&m_timer, &QTimer::timeout, model, [this, model]() {
            if (m_conditionDismissInitialDirtiness)
            {
                m_conditionDismissInitialDirtiness = false;
                return;
            }

            assert(!m_recoveryFileName.isEmpty());

            const QByteArray tmpFileName = (m_recoveryFileName + QStringLiteral(".part")).toLatin1();
            const QByteArray recoveryFileName = m_recoveryFileName.toLatin1();

            if (model->isEmpty())
            {
                remove(recoveryFileName.constData());
                return;
            }

            if (model->serialize(tmpFileName.constData()) != VLC_SUCCESS)
                return;

            remove(recoveryFileName.constData());
            if (!rename(tmpFileName.constData(), recoveryFileName.constData()))
            {
                assert(m_settings);
                m_settings->sync();
                if (!m_settings->contains(m_key))
                {
                    m_settings->setValue(m_key, m_recoveryFileName);
                    m_settings->sync();
                }
                else if (m_settings->value(m_key) != m_recoveryFileName)
                {
                    QObject::disconnect(model, nullptr, &m_timer, nullptr);
                    m_timer.stop();
                }
            }
        });

        QObject::connect(model, &T::itemsReset, &m_timer, QOverload<>::of(&QTimer::start));
        QObject::connect(model, &T::itemsAdded, &m_timer, QOverload<>::of(&QTimer::start));
        QObject::connect(model, &T::itemsMoved, &m_timer, QOverload<>::of(&QTimer::start));
        QObject::connect(model, &T::itemsRemoved, &m_timer, QOverload<>::of(&QTimer::start));
        QObject::connect(model, &T::itemsUpdated, &m_timer, QOverload<>::of(&QTimer::start));
    }

public:
    // NOTE: settings and model must outlive the instance of this class.
    template<class T>
    static std::unique_ptr<ModelRecoveryAgent>
    recover(class QSettings *settings, const QString& modelIdentifier, T* model)
    {
        assert(settings);
        assert(model);

        QString key(modelIdentifier + QStringLiteral("/RecoveryFilePath"));
        QString recoveryFileName;
        std::unique_ptr<QLockFile> lockFile;
        bool conditionDismissInitialDirtiness = false;

        settings->sync();
        if (settings->contains(key))
        {
            assert(settings->value(key).typeId() == QMetaType::QString);
            recoveryFileName = settings->value(key).toString();
            if (!recoveryFileName.isEmpty())
            {
                {
                    lockFile = std::make_unique<QLockFile>(recoveryFileName + QStringLiteral(".lock"));
                    lockFile->setStaleLockTime(0); // if the process crashed, QLockFile considers the lock stale regardless of the lock time
                    if (!lockFile->tryLock()) // if the older instance is still alive, it would have the lock
                        return nullptr;
                }
                const QFileInfo fileInfo(recoveryFileName);
                if (fileInfo.size() > 0)
                {
                    QMessageBox msgBox;
                    msgBox.setText(qtr("The application closed abruptly."));
                    msgBox.setInformativeText(qtr("Do you want to restore the %1 model from %2?").arg(modelIdentifier.toLower(),
                                                                                                      fileInfo.lastModified().toString()));
                    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
                    msgBox.setDefaultButton(QMessageBox::Yes);
                    if (msgBox.exec() == QMessageBox::Yes)
                    {
                        model->append(recoveryFileName);
                        conditionDismissInitialDirtiness = true;
                    }
                }
            }
        }

        if (recoveryFileName.isEmpty())
        {
            QTemporaryFile temporaryFile;
            temporaryFile.setAutoRemove(false);
            if (!temporaryFile.open())
                return nullptr;
            recoveryFileName = temporaryFile.fileName();
            {
                assert(!lockFile);
                lockFile = std::make_unique<QLockFile>(recoveryFileName + QStringLiteral(".lock"));
                lockFile->setStaleLockTime(0); // if the process crashed, QLockFile considers the lock stale regardless of the lock time
                assert(!lockFile->isLocked()); // the file name is new here, it can not be locked before
                if (!lockFile->tryLock())
                    return nullptr;
            }
            settings->setValue(key, recoveryFileName);
            settings->sync();
        }

        auto agent = std::unique_ptr<ModelRecoveryAgent>(new ModelRecoveryAgent(
            settings, std::move(key), std::move(recoveryFileName),
            conditionDismissInitialDirtiness, model));
        agent->m_lockFile = std::move(lockFile);
        return agent;
    }

    ~ModelRecoveryAgent()
    {
        if (!m_recoveryFileName.isEmpty())
        {
            QFile::remove(m_recoveryFileName);
        }

        assert(m_settings);

        m_settings->sync();

        if (m_settings->value(m_key) == m_recoveryFileName)
        {
            m_settings->remove(m_key);
            m_settings->sync();
        }
    }
};

#endif // MODEL_RECOVERY_AGENT_HPP
