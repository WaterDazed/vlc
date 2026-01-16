/*****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
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
#ifndef LOTTIE_MODULE_HPP
#define LOTTIE_MODULE_HPP

#include <QQuickItem>
#include <QQmlEngine>

#include <optional>
#include <atomic>
#include <memory>

#include <vlc_common.h>

class QQuickImageProvider;

struct LottieModule
{
    vlc_object_t obj;

    module_t *p_module = nullptr;
    void *p_sys = nullptr;

    // This function returns a new image provider, which provides an animated
    // texture. Note that it is the consumer's responsibility to make sure
    // Qt renderer to call `commitTextureOperations()`. This can be satisfied
    // by having a `Connections` that listens to the window's `afterAnimating()`
    // signal and to call `update()` to schedule an update for each frame. Also
    // note that only with RHI animation will work.
    QQuickImageProvider* (*newImageProvider)();

    // This function registers `VLC.Lottie` module, and `LottieAnimation` qml type.
    // `LottieAnimation` is expected to able to paint animated lottie, and at
    // the same time being a texture provider (so that painting can be delegated,
    // such as to `ShaderEffect` or `ImageExt`). For basic cases, where fine
    // control is not needed (such as, being able to stop the animation), it
    // is recommended to use the image provider instead.
    void (*registerQmlModuleAndTypes)();
};

class LottieItem : public QQuickItem
{
    Q_OBJECT

    // These properties are not final, they may be overridden by the derived type if necessary.
    // Alternatively, the virtual methods may be overridden, which should be preferred by default.

    Q_PROPERTY(bool animating READ animating WRITE setAnimating NOTIFY animatingChanged)
    Q_PROPERTY(bool stopWhenFinished READ stopWhenFinished WRITE setStopWhenFinished NOTIFY stopWhenFinishedChanged)
    Q_PROPERTY(double frameRate READ frameRate WRITE setFrameRate RESET resetFrameRate NOTIFY frameRateChanged)
    Q_PROPERTY(double implicitFrameRate READ implicitFrameRate NOTIFY implicitFrameRateChanged)
    Q_PROPERTY(int frameCount READ frameCount NOTIFY frameCountChanged) // int and not size_t because of QML
    Q_PROPERTY(bool cache MEMBER m_cache NOTIFY cacheChanged)
    Q_PROPERTY(bool asynchronous MEMBER m_asynchronous NOTIFY asynchronousChanged)
    Q_PROPERTY(Status status READ status NOTIFY statusChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(bool retainWhileLoading MEMBER m_retainWhileLoading NOTIFY retainWhileLoadingChanged)

    // These properties are not meant to be animated or rapidly changed:
    Q_PROPERTY(QUrl source MEMBER m_source NOTIFY sourceChanged)
    Q_PROPERTY(QSize sourceSize READ sourceSize WRITE setSourceSize RESET resetSourceSize NOTIFY sourceSizeChanged)

    // These properties are overridden as readonly, so that they can not be modified in qml:
    Q_PROPERTY(qreal implicitWidth READ implicitWidth NOTIFY implicitWidthChanged)
    Q_PROPERTY(qreal implicitHeight READ implicitHeight NOTIFY implicitHeightChanged)

    QML_ELEMENT

public:
    enum Status
    {
        Null,
        Ready,
        Loading,
        Error
    };
    Q_ENUM(Status);

    explicit LottieItem(QQuickItem *parent = nullptr)
        : QQuickItem(parent)
    {

    }

    virtual QSize sourceSize() const
    {
        return m_sourceSize ? *m_sourceSize : QSize();
    }

    virtual void setSourceSize(const QSize &newSourceSize)
    {
        if (m_sourceSize == newSourceSize)
            return;
        m_sourceSize = newSourceSize;
        emit sourceSizeChanged(newSourceSize);
    }

    virtual void resetSourceSize()
    {
        if (m_sourceSize)
        {
            m_sourceSize.reset();
            emit sourceSizeChanged(QSize());
        }
    }

    virtual double frameRate() const
    {
        return m_frameRate;
    }

    virtual void setFrameRate(double newFrameRate)
    {
        if (!qFuzzyCompare(m_frameRate.exchange(newFrameRate), newFrameRate))
            emit frameRateChanged(newFrameRate);
    }

    virtual void resetFrameRate()
    {
        if (!qFuzzyCompare(m_frameRate.exchange(0.0), 0.0))
            emit frameRateChanged(0.0);
    }

    virtual double implicitFrameRate() const
    {
        return m_implicitFrameRate;
    }

    virtual size_t frameCount() const
    {
        return m_frameCount;
    }

    virtual bool animating() const
    {
        return m_animating;
    }

    virtual void setAnimating(bool newAnimating)
    {
        if (m_animating.exchange(newAnimating) != newAnimating)
            emit animatingChanged(newAnimating);
    }

    virtual bool stopWhenFinished() const
    {
        return m_stopWhenFinished;
    }

    virtual void setStopWhenFinished(bool newStopWhenFinished)
    {
        if (m_stopWhenFinished.exchange(newStopWhenFinished) != newStopWhenFinished)
            emit stopWhenFinishedChanged(newStopWhenFinished);
    }

    virtual Status status() const
    {
        return m_status;
    }

    virtual double progress() const
    {
        return m_progress;
    }

    Q_INVOKABLE virtual int currentFrame() const // Avoid using this method with the threaded render loop.
    {
        qWarning() << "LottieItem: currentFrame() is unimplemented.";
        return 0;
    }

    Q_INVOKABLE virtual void gotoFrame(int frame)
    {
        Q_UNUSED(frame);
        qWarning() << "LottieItem: gotoFrame() is unimplemented.";
    }

    Q_INVOKABLE virtual void restart()
    {
        qWarning() << "LottieItem: restart() is unimplemented.";
    }

signals:
    void sourceChanged(const QUrl&);
    void sourceSizeChanged(const QSize&);
    void animatingChanged(bool);
    void retainWhileLoadingChanged(bool);
    void frameRateChanged(double);
    void implicitFrameRateChanged(double);
    void frameCountChanged(size_t);
    void cacheChanged(bool);
    void stopWhenFinishedChanged(bool);
    void asynchronousChanged(bool);
    void statusChanged(LottieItem::Status);
    void progressChanged(double);

protected:
    // Do not expose these publicly, or through the property system:
    virtual void setImplicitFrameRate(const double rate)
    {
        if (!qFuzzyCompare(m_implicitFrameRate.exchange(rate), rate))
            emit implicitFrameRateChanged(rate);
    }

    virtual void setStatus(const Status status)
    {
        if (m_status.exchange(status) != status)
            emit statusChanged(status);

        switch (status)
        {
        case Status::Null:
        case Status::Error:
            setProgress(0.0);
        default:
            break;
        }
    }

    virtual void setFrameCount(const size_t frameCount)
    {
        if (m_frameCount.exchange(frameCount) != frameCount)
            emit frameCountChanged(frameCount);
    }

    virtual void setProgress(const double progress)
    {
        if (m_progress.exchange(progress) != progress)
            emit progressChanged(progress);
    }

protected:
    QUrl m_source;
    std::optional<QSize> m_sourceSize;

    std::atomic<bool> m_animating = { true };
    std::atomic<double> m_frameRate = { -1.0 };
    std::atomic<double> m_implicitFrameRate = { 0.0 };
    std::atomic<size_t> m_frameCount = 0;
    std::atomic<bool> m_stopWhenFinished = false;
    std::atomic<Status> m_status = Null;
    std::atomic<double> m_progress = 0.0;

    bool m_retainWhileLoading = false;
    bool m_cache = true;
    bool m_asynchronous = true;
};

class LottieFallbackItem : public LottieItem
{
    Q_OBJECT

    // It is expected to have one QML engine instance throughout the
    // application, for that reason a map is not used. If the next
    // instance is bound to a different engine, we overwrite the
    // static factory:
    static inline std::unique_ptr<QQmlComponent> fallbackIndicatorFactory;

public:
    explicit LottieFallbackItem(QQuickItem *parent = nullptr)
        : LottieItem(parent)
    {

    }

protected:
    void componentComplete() override
    {
        LottieItem::componentComplete();

        const auto engine = qmlEngine(this);
        assert(engine);

        // In QML, such property may be defined to disable the indicator for any reason.
        if (!property("noFallbackIndicator").toBool())
        {
            // We could also use `QSGTextNode`, but it is not as trivial to use and it is not available
            // before Qt 6.7.

            if (!fallbackIndicatorFactory || fallbackIndicatorFactory->engine() != engine)
            {
                fallbackIndicatorFactory = std::make_unique<QQmlComponent>(engine);
                fallbackIndicatorFactory->setData(QByteArrayLiteral("import QtQuick;" \
                                                                    "import QtQuick.Controls;" \
                                                                    "Text {" \
                                                                        "id: _fallbackIndicator;" \
                                                                        "objectName: 'fallbackIndicator';" \
                                                                        "anchors.fill: parent;" \
                                                                        "text: 'L';" \
                                                                        "color: parent?.animating ? 'cyan' : 'purple';" \
                                                                        "fontSizeMode: Text.Fit;" \
                                                                        "font.pixelSize: height;" \

                                                                        "ToolTip.visible: hoverHandler.hovered;" \
                                                                        "ToolTip.delay: 700;" \
                                                                        "ToolTip.text: 'Lottie animations are not available.\nSource URL: ' + parent?.source;" \

                                                                        "HoverHandler { id: hoverHandler; } " \
                                                                    "}"), {});

#ifndef NDEBUG
                if (fallbackIndicatorFactory->isError())
                {
                    qDebug() << "LottieFallbackItem: could not create fallback item: " << fallbackIndicatorFactory->errors();
                }
#endif
            }

            // `createObject()` is protected, so we use `createWithInitialProperties()` instead:
            const auto fallbackIndicator = static_cast<QQuickItem*>(fallbackIndicatorFactory->createWithInitialProperties({{QStringLiteral("parent"), QVariant::fromValue(this)}},
                                                                                                                          qmlContext(this)));
            assert(fallbackIndicator);

            fallbackIndicator->setParent(this);
            // fallbackIndicator->setParentItem(this); // Set through initial properties above
        }
    }
};

#endif // LOTTIE_MODULE_HPP
