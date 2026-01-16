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

#include "rlottie_module.hpp"

#if __has_include(<QtGui/rhi/qrhi.h>)
// RHI is semi-public since Qt 6.6, but still requires gui-private.
#define RHI_PUBLIC
#include <QtGui/rhi/qrhi.h>
#elif __has_include(<QtGui/private/qrhi_p.h>) && __has_include(<QtQuick/private/qquickwindow_p.h>)
#warning "It is recommended to use Qt 6.6 or greater."
#include <QtGui/private/qrhi_p.h>
#include <QtQuick/private/qquickwindow_p.h>
#else
#error "Qt rlottie module requires rhi headers, are Qt gui private classes available?"
#endif

#include <QUrl>
#include <QUrlQuery>
#include <QRunnable>
#include <QFile>
#include <QThreadPool>
#include <QSGSimpleTextureNode>
#include <QQmlFile>
#include <QThread>
#include <QTimer>

#include <vlc_plugin.h>

#ifdef QT_STATIC
// Defining this is necessary for static build before including `rlottie.h`, rlottie bug?
#define RLOTTIE_BUILD
#endif
#include <rlottie.h>

#ifndef QT_RLOTTIE_TIMER_TYPE
// If there are multiple lottie animations running at the same time, using
// coarse timer can be more advantageous. If there is only going to be one
// animation running at a time, precise timer makes more sense. By default,
// it is expected that multiple animations may be running at the same time,
// hence coarse timer is used. See the Qt docs for details.
#define QT_RLOTTIE_TIMER_TYPE 1 // 0: Precise, 1: Coarse, 2: Very coarse
#endif

/// <compression>
/// Images are compressed by default. Disabling compression increases system memory usage.
/// Uncompression is done per frame in the rendering thread, but it does not affect the
/// rendering performance normally, provided that rendering speed is greater than the
/// refresh rate with additional room that the uncompression can use, because rendering
/// thread is already throttled by v-sync. Note that that using a higher compression level
/// does not increase the compression in most cases, but nevertheless increases the time it
/// takes to compress (and to some extent uncompress) the images. Also note that since
/// lottie animations are vector animations, the raster result can be compressed well. For
/// that reason, it is strongly recommended to not disable compression.
#ifndef QT_RLOTTIE_COMPRESSION_USE_LZAV
#define QT_RLOTTIE_COMPRESSION_USE_LZAV 1
#endif

#if QT_RLOTTIE_COMPRESSION_USE_LZAV
// Use LZAV by default, which is much faster than zlib or zstd that `qCompress()` uses:
#if __has_include(<util/lzav.h>)
#include <util/lzav.h>
#undef QT_RLOTTIE_COMPRESSION_LEVEL
#define QT_RLOTTIE_COMPRESSION_LEVEL -1
#else
#warning "lzav.h is not available, LZAV (default) will not be used for compression!"
#undef QT_RLOTTIE_COMPRESSION_USE_LZAV
#define QT_RLOTTIE_COMPRESSION_USE_LZAV 0
#endif
#endif

#if !defined(QT_NO_COMPRESS) && !QT_RLOTTIE_COMPRESSION_USE_LZAV
#ifndef QT_RLOTTIE_COMPRESSION_LEVEL
#define QT_RLOTTIE_COMPRESSION_LEVEL 1 // Between 0 to 9. Use 0 to disable compression. Only relevant with `qCompress()`
#endif
#endif

#if QT_RLOTTIE_COMPRESSION_USE_LZAV || (QT_RLOTTIE_COMPRESSION_LEVEL > 0) // Only relevant when compression is enabled
static std::atomic<bool> g_shouldCompress = true;
// If certain amount of frames skipped, compression is going to be automatically disabled.
// The threshold may be set to 0 to disable this behavior. Note that both the tracking of
// skipped frames and compression disabler override are done globally.
#ifndef QT_RLOTTIE_COMPRESSION_AUTO_DISABLE_SKIPPED_FRAME_THRESHOLD
#define QT_RLOTTIE_COMPRESSION_AUTO_DISABLE_SKIPPED_FRAME_THRESHOLD 64
#endif

#if (QT_RLOTTIE_COMPRESSION_AUTO_DISABLE_SKIPPED_FRAME_THRESHOLD > 0)
#define QT_RLOTTIE_COMPRESSION_AUTO_DISABLE
static size_t g_skippedFrames = 0;
#endif

#else
#warning "Qt is built without compression support and LZAV is not used. Qt rlottie module will not use compression."
#endif

/// </compression>

static QRhi* getRhiFromWindow(const QQuickWindow* window)
{
    assert(window);
#ifdef RHI_PUBLIC
    QRhi* const rhi = window->rhi();
#else
    const QQuickWindowPrivate *const privateWindow = QQuickWindowPrivate::get(window);
    QRhi* const rhi = privateWindow->rhi;
#endif
    return rhi;
}

class TextureProviderCleaner : public QRunnable
{
public:
    explicit TextureProviderCleaner(QSGTextureProvider *textureProvider)
        : m_textureProvider(textureProvider) { }

    void run() override
    {
        delete m_textureProvider;
    }

private:
    const QPointer<QSGTextureProvider> m_textureProvider;
};

RlottieItem::RlottieItem(QQuickItem *parent)
    : LottieItem(parent)
{
    setFlag(QQuickItem::ItemHasContents);

    // We don't need this for the properties that causes texture change,
    // such as `sourceChanged` or `sourceSizeChanged`, since update is
    // already requested in that case.
    connect(this, &RlottieItem::animatingChanged, this, &QQuickItem::update);
    // These are sampler properties, they do not have anything to do with
    // the texture itself:
    connect(this, &QQuickItem::widthChanged, this, &QQuickItem::update);
    connect(this, &QQuickItem::heightChanged, this, &QQuickItem::update);
    connect(this, &QQuickItem::smoothChanged, this, &QQuickItem::update);
}

RlottieItem::~RlottieItem()
{
    {
        if (m_textureProvider)
        {
            // https://doc.qt.io/qt-6/qquickitem.html#graphics-resource-handling

            // `QQuickItem::releaseResources()` is called before the item is disassociated from its window:
            // "This happens when the item is about to be removed from the window it was previously rendering to."
            // Therefore, we can not have a texture provider during destruction, but not the window:
            assert(window());
            window()->scheduleRenderJob(new TextureProviderCleaner(m_textureProvider), QQuickWindow::BeforeSynchronizingStage);
            m_textureProvider = nullptr;
        }
    }
}

bool RlottieItem::isTextureProvider() const
{
    return true;
}

QSGTextureProvider *RlottieItem::textureProvider() const
{
    // This method is called from the rendering thread.

    if (!m_textureProvider)
    {
        m_textureProvider = new RlottieTextureProvider(window(), const_cast<RlottieItem*>(this));

        static const bool threadedRenderLoop = [this]() {
            const bool ret = (QThread::currentThread() != thread());
#ifndef NDEBUG
            if (!ret)
            {
                qDebug("RlottieItem: rendering is not threaded. You may want to enable " \
                       "threaded render loop with `QSG_RENDER_LOOP=threaded` for smoother " \
                       "animations (all animations in the interface). This option may not be " \
                       "applicable in all situations.");
            }
#endif
            return ret;
        }();

        m_threadedRenderLoop = threadedRenderLoop;
        assert(threadedRenderLoop ? QThread::currentThread() != thread() : QThread::currentThread() == thread());

        // Akin to `TextureProviderItem`, but unlike there where texture change may be signalled rapidly, it is not the
        // case here. This means that we can be more lax here:
        const auto synchronizeState = [weakThis = QPointer(const_cast<RlottieItem*>(this)), provider = m_textureProvider, threadedRendering = m_threadedRenderLoop]() {
            // Bidirectional synchronization, between the texture and the item.

            if (!Q_UNLIKELY(weakThis))
                return;

            const auto texture = provider->texture();
            const auto dynamicTexture = qobject_cast<RlottieDynamicTexture*>(texture);
            if (!texture)
                return;

            QSize implicitSize;

            if (dynamicTexture)
            {
                // Item -> Texture
                dynamicTexture->setFrameRate(weakThis->m_frameRate);
                dynamicTexture->setAnimating(weakThis->m_animating);
                dynamicTexture->setStopWhenFinished(weakThis->m_stopWhenFinished);

                // Texture -> Item
                weakThis->setImplicitFrameRate(dynamicTexture->implicitFrameRate());
                weakThis->setFrameCount(dynamicTexture->frameCount());
                weakThis->setFrameRate(dynamicTexture->frameRate()); // if different, texture's value overrides

                implicitSize = dynamicTexture->implicitSize();
            }
            else
            {
                // Not much to do...
                implicitSize = texture->textureSize();
            }

            // A queued connection is used, because implicit size getters/setters of QQuickItem are not thread safe. As noted above, we can
            // use a queued connection because we do not expect this slot to be called often. It is called only when texture change is
            // signalled, which is usually when the source or sourceSize changes. Note that texture changed is intentionally not signalled
            // per animation frame. There is a frame changed signal, which is used to mark the node dirty (when applicable), and is not
            // queued.
            QMetaObject::invokeMethod(weakThis, [weakThis, implicitSize]() {
                assert(weakThis);
                weakThis->setImplicitSize(implicitSize.width(), implicitSize.height());
            }, threadedRendering ? Qt::QueuedConnection : Qt::DirectConnection);
        };

        const auto establishConnections = [this]() {
            const RlottieDynamicTexture* texture;

            if (Q_LIKELY(m_textureProvider)) // this is possible, if it is queued
                texture = qobject_cast<RlottieDynamicTexture*>(m_textureProvider->texture());
            else
                texture = nullptr;

            if (texture == m_oldTexture)
                return;

            if (m_oldTexture)
            {
                disconnect(this, nullptr, m_oldTexture, nullptr);
                disconnect(m_oldTexture, nullptr, this, nullptr);
            }

            m_oldTexture = texture;

            if (!texture)
                return;

            // Item -> Texture
            connect(this, &RlottieItem::animatingChanged, texture, &RlottieDynamicTexture::setAnimating);
            connect(this, &RlottieItem::frameRateChanged, texture, &RlottieDynamicTexture::setFrameRate);
            connect(this, &RlottieItem::stopWhenFinishedChanged, texture, &RlottieDynamicTexture::setStopWhenFinished);

            // Texture -> Item
            connect(texture, &RlottieDynamicTexture::animatingOverridden, this, &RlottieItem::setAnimating); // if different, texture's value overrides
            connect(texture, &RlottieDynamicTexture::frameChangeCommitted, this, &RlottieItem::markDirtyFrame, Qt::DirectConnection);
        };

        // textureChanged() is only signalled when the source changes, not per frame. The
        // reason for that is neither the scene graph texture nor the rhi texture actually
        // changes. Rather, we overwrite the data of the existing texture. This is done
        // to improve rendering performance, since that way there is no need to do
        // reallocation for the texture. This is also the reason why we need to handle
        // dirtiness manually.
        connect(m_textureProvider, &RlottieTextureProvider::textureChanged, m_textureProvider, synchronizeState); // Executed in texture provider's thread
        connect(m_textureProvider, &RlottieTextureProvider::textureChanged, this, establishConnections); // Queued (with threaded render loop)
        connect(m_textureProvider, &RlottieTextureProvider::generationSucceeded, this, [this]() {
            const_cast<RlottieItem*>(this)->setStatus(Status::Ready);
        }, Qt::DirectConnection);
        connect(m_textureProvider, &RlottieTextureProvider::generationFailed, this, [this](const QString& reason) {
            qCritical() << reason; // Already formatted by the runnable.
            const_cast<RlottieItem*>(this)->setStatus(Status::Error);
        }, Qt::DirectConnection);
        connect(m_textureProvider, &RlottieTextureProvider::progressChanged, this, [this](const double progress) {
            const_cast<RlottieItem*>(this)->setProgress(progress);
        }, Qt::DirectConnection);

        // These are queued (with threaded render loop), as expected:
        connect(m_textureProvider, &RlottieTextureProvider::textureChanged, this, &QQuickItem::update); // only relevant when visible
        connect(this, &QQuickItem::windowChanged, m_textureProvider, &RlottieTextureProvider::setWindow); // only relevant for its initialization (for now)

        connect(this, &RlottieItem::sourceChanged, this, &RlottieItem::regenerateTexture);
        connect(this, &RlottieItem::sourceSizeChanged, this, &RlottieItem::regenerateTexture);

        const auto path = QQmlFile::urlToLocalFileOrQrc(m_source);
        if (!m_source.isEmpty())
        {
            const_cast<RlottieItem*>(this)->setStatus(Status::Loading);
            m_textureProvider->regenerate(path, sourceSize(), m_cache, m_retainWhileLoading, m_asynchronous, m_animating);
        }
    }

    // If layered, return the layer instead of own texture provider.
    // This behavior is similar to how `QQuickImage` behaves. Note
    // that we still need to create our own texture provider, because
    // the scene graph node uses the texture it provides as its
    // source (see `::updatePaintNode()`).
    if (QQuickItem::isTextureProvider())
        return QQuickItem::textureProvider();
    else
        return m_textureProvider;
}

void RlottieItem::invalidateSceneGraph()
{
    // https://doc.qt.io/qt-6/qquickitem.html#graphics-resource-handling

    // This slot is called from the rendering thread.
    {
        if (m_textureProvider)
        {
            delete m_textureProvider;
        }
    }
}

void RlottieItem::releaseResources()
{
    // https://doc.qt.io/qt-6/qquickitem.html#graphics-resource-handling

    // This method is called from the GUI thread.

    // QQuickItem::releaseResources() is guaranteed to have a valid window when it is called:
    assert(window());
    {
        if (m_textureProvider)
        {
            window()->scheduleRenderJob(new TextureProviderCleaner(m_textureProvider), QQuickWindow::BeforeSynchronizingStage);
            m_textureProvider = nullptr;
        }
    }

    QQuickItem::releaseResources();
}

QSGNode *RlottieItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    // Can use static cast because it can not be any other type if it is valid:
    QSGSimpleTextureNode *node = static_cast<QSGSimpleTextureNode*>(oldNode);

    const auto tp = textureProvider(); // We can call this because this is called in the rendering thread.
    assert(tp);

    const auto texture = tp->texture();
    if (!texture)
    {
        delete node;
        return nullptr;
    }

    if (!node)
    {
        node = new QSGSimpleTextureNode;
        node->setOwnsTexture(false); // Texture provider owns the texture.
    }

    node->setRect(0.0, 0.0, width(), height());
    node->setFiltering(smooth() ? QSGTexture::Linear : QSGTexture::Nearest);
    if (node->texture() != texture)
        node->setTexture(texture);

    if (m_animating && qobject_cast<RlottieDynamicTexture*>(texture))
    {
        if (m_dirtyFrame.exchange(false))
            node->markDirty(QSGNode::DirtyMaterial);

        // Immediately schedule a new update for the next frame:
        update();
    }

    return node;
}

bool RlottieItem::regenerateTexture()
{
    if (!m_textureProvider)
        return false;

    if (m_source.isEmpty())
        setStatus(Status::Null);
    else
        setStatus(Status::Loading);

    // This is queued because the texture provider lives in the render thread.
    // It is intentional we capture the necessary members here, since we can
    // not access them in the render/tp thread.
    QMetaObject::invokeMethod(m_textureProvider, [tp = m_textureProvider,
                                                  source = m_source,
                                                  size = m_sourceSize,
                                                  retainWhileLoading = m_retainWhileLoading,
                                                  cache = m_cache,
                                                  asynchronous = m_asynchronous,
                                                  autoStart = m_animating.load()]() {
        assert(tp);
        const auto path = QQmlFile::urlToLocalFileOrQrc(source);
        if (path.isEmpty())
        {
            tp->releaseTexture();
        }
        else
        {
            tp->regenerate(path, size, cache, retainWhileLoading, asynchronous, autoStart);
        }
    }, m_threadedRenderLoop ? Qt::QueuedConnection : Qt::DirectConnection);

    return true;
}

void RlottieItem::markDirtyFrame()
{
    m_dirtyFrame = true;
}

int RlottieItem::currentFrame() const
{
    if (m_textureProvider)
    {
        size_t currentFrame = 0;

        QMetaObject::invokeMethod(m_textureProvider, [tp = m_textureProvider, &currentFrame]() {
            assert(tp);
            const auto texture = qobject_cast<RlottieDynamicTexture*>(tp->texture());
            if (const auto dynamicTexture = qobject_cast<RlottieDynamicTexture*>(texture))
                currentFrame = texture->currentFrame();
            else if (texture)
                qWarning("RlottieItem::currentFrame() : This method is only functional with rhi.");
            else
                qWarning("RlottieItem::currentFrame() : Texture is not available.");
        },
        m_threadedRenderLoop ? Qt::BlockingQueuedConnection : Qt::DirectConnection);

        return currentFrame;
    }

    return 0;
}

void RlottieItem::gotoFrame(int frame)
{
    if (m_textureProvider)
    {
        QMetaObject::invokeMethod(m_textureProvider, [frame,
                                                      tp = m_textureProvider]() {
            assert(tp);
            const auto texture = qobject_cast<RlottieDynamicTexture*>(tp->texture());
            if (const auto dynamicTexture = qobject_cast<RlottieDynamicTexture*>(texture))
                dynamicTexture->setCurrentFrame(frame);
            else if (texture)
                qWarning("RlottieItem::gotoFrame() : This method is only functional with rhi.");
            else
                qWarning("RlottieItem::gotoFrame() : Texture is not available.");
        }, m_threadedRenderLoop ? Qt::QueuedConnection : Qt::DirectConnection);
    }
}

void RlottieItem::restart()
{
    gotoFrame(0);
    if (!m_animating.exchange(true))
    {
        emit animatingChanged(true);
    }
}

RlottieDynamicTexture::RlottieDynamicTexture(QQuickWindow *window, const RlottieLottieData &data, bool autoStart)
    : QSGDynamicTexture()
    , m_data(data)
{
    assert(window);

    QRhi* const rhi = getRhiFromWindow(window);
    assert(rhi);

    m_rhiTexture = rhi->newTexture(m_data.isBGR ? QRhiTexture::BGRA8 : QRhiTexture::RGBA8, data.size);
    if (!m_rhiTexture)
    {
        qWarning() << "RlottieDynamicTexture: QRhi::newTexture() failed!";
    }

    if (!m_rhiTexture->create())
    {
        qWarning() << "RlottieDynamicTexture: QRhiTexture::create() failed!";
        delete m_rhiTexture;
        m_rhiTexture = nullptr;
    }

    // This is static, so only one thread for all textures.
    // Since the timer action is not time taking, this is
    // not considered a problem. We can not afford a thread
    // per item. The reason we use a separate thread for the
    // timer is that, when the rendering thread is busy the
    // timer would be delayed which in turn would make frame
    // not as wanted.
    const auto renderingThread = thread();
    static const QPointer<QThread> timerThread = [renderingThread]() {
        const auto thread = new QThread(renderingThread);
        thread->setObjectName("QtRlottieTimerThread");
        connect(renderingThread, &QObject::destroyed, thread, [thread]() {
            thread->quit();
            thread->wait();
            // Deletion is done automatically through ownership.
        });
        thread->start();
        return thread;
    }();

    assert(timerThread);

    m_timer = new QTimer;
    m_timer->setSingleShot(false);
    m_timer->setTimerType(static_cast<Qt::TimerType>(QT_RLOTTIE_TIMER_TYPE));
    m_timer->moveToThread(timerThread);
    connect(timerThread, &QThread::finished, m_timer, &QObject::deleteLater); // We stop and delete in the destructor, but just in case.
    connect(m_timer, &QTimer::timeout, this, &RlottieDynamicTexture::tick, Qt::DirectConnection);

    setFrameRate({});

    if (autoStart)
        setAnimating(true);
}

RlottieDynamicTexture::~RlottieDynamicTexture()
{
    assert(m_timer);
    QMetaObject::invokeMethod(m_timer, [timer = m_timer]() {
        delete timer;
    }, Qt::BlockingQueuedConnection);

    delete m_rhiTexture;
    m_rhiTexture = nullptr;
}

bool RlottieDynamicTexture::isAnimating() const
{
    assert(m_timer);
    return m_timer->isActive();
}

void RlottieDynamicTexture::setAnimating(bool enabled)
{
    assert(m_timer);
    QMetaObject::invokeMethod(m_timer, [timer = m_timer, enabled]() {
        if (enabled)
        {
            if (timer->isActive())
                return;

            timer->start();
        }
        else
        {
            timer->stop();
        }
    }, Qt::QueuedConnection);
}

bool RlottieDynamicTexture::stopWhenFinished() const
{
    return m_stopWhenFinished;
}

void RlottieDynamicTexture::setStopWhenFinished(bool stop)
{
    m_stopWhenFinished = stop;
}

size_t RlottieDynamicTexture::currentFrame() const
{
    return m_frame;
}

void RlottieDynamicTexture::setCurrentFrame(const size_t frame)
{
    m_frame = (frame % m_data.totalFrame);
}

double RlottieDynamicTexture::frameRate() const
{
    return m_frameRate;
}

void RlottieDynamicTexture::setFrameRate(std::optional<double> frameRate)
{
    assert(m_timer);

    double newFrameRate;

    if (frameRate && *frameRate > 0.0)
        newFrameRate = *frameRate;
    else
        newFrameRate = m_data.frameRate; // the natural/implicit frame rate

    if (!qFuzzyCompare(m_frameRate, newFrameRate))
    {
        m_frameRate = newFrameRate;

        // Note that if the refresh rate of the window is slower than the effective frame rate,
        // some frames will be skipped during animation. This is important to keep the intended
        // pace. We do not expect momentary stuttering in the interface, so we are not backlogging
        // frames and increasing speed as a remedy like some video /players do.
        QMetaObject::invokeMethod(m_timer, [timer = m_timer, newFrameRate]() {
            assert(timer);
            timer->setInterval(std::floor(1000.0 / newFrameRate));
        }, Qt::QueuedConnection);
    }
}

double RlottieDynamicTexture::implicitFrameRate() const
{
    return m_data.frameRate;
}

QSize RlottieDynamicTexture::implicitSize() const
{
    return m_data.implicitSize;
}

size_t RlottieDynamicTexture::frameCount() const
{
    return m_data.totalFrame;
}

static void ImageCleanupHandler(void *data)
{
    delete static_cast<QByteArray*>(data);
}

QImage RlottieDynamicTexture::retrieveImage(const RlottieLottieData &data, const size_t index)
{
    assert(index < data.images.size());

    const auto imageData = new QByteArray;
#if QT_RLOTTIE_COMPRESSION_USE_LZAV || (QT_RLOTTIE_COMPRESSION_LEVEL > 0)
    if (data.compressed)
    {
#if QT_RLOTTIE_COMPRESSION_USE_LZAV
        qsizetype srcSize;
        QByteArray compressedData;
        {
            QDataStream stream(data.images[index]);
            stream >> srcSize >> compressedData;
        }

        QByteArray decompressedData(srcSize, Qt::Initialization::Uninitialized);

        const auto decompressedSize = lzav_decompress(compressedData.constData(), decompressedData.data(), compressedData.size(), srcSize);

        if (Q_UNLIKELY(decompressedSize < 0))
        {
            qCritical() << "RlottieDynamicTexture: LZAV failed to decompress the image.";
            delete imageData;
            return QImage();
        }

        decompressedData.resize(decompressedSize);

        *imageData = std::move(decompressedData);
#else
        *imageData = qUncompress(data.images[index]);
#endif
    }
    else
#endif
    {
        *imageData = data.images[index];
    }

    return QImage(reinterpret_cast<const uchar*>(imageData->constData()),
                  data.size.width(), data.size.height(),
                  data.isBGR ? QImage::Format_ARGB32_Premultiplied
                             : QImage::Format_RGBA8888_Premultiplied,
                  ImageCleanupHandler, imageData);
}

qint64 RlottieDynamicTexture::comparisonKey() const
{
    // Akin to `QSGPlainTexture::comparisonKey()`:
    if (m_rhiTexture)
        return qint64(m_rhiTexture);

    return qint64(this);
}

QRhiTexture *RlottieDynamicTexture::rhiTexture() const
{
    return m_rhiTexture;
}

QSize RlottieDynamicTexture::textureSize() const
{
    return m_data.size;
}

bool RlottieDynamicTexture::hasAlphaChannel() const
{
    return true;
}

bool RlottieDynamicTexture::hasMipmaps() const
{
    return false;
}

void RlottieDynamicTexture::commitTextureOperations(QRhi *rhi, QRhiResourceUpdateBatch *resourceUpdates)
{
    QSGDynamicTexture::commitTextureOperations(rhi, resourceUpdates); // Possibly unnecessary?

    if (!m_rhiTexture)
        return;

    assert(resourceUpdates);

    // We do not create a new texture, but overwrite its data through rhi.
    // One might ask, why not pregenerate textures instead of images and
    // not bother with uploading texture. There are two reasons:
    // - It would consume as much video memory as we are consuming system
    //   memory at the moment. Video memory is known to be scarce, we
    //   need to be reasonable.
    // - Blocking the render thread is not a problem, provided that the
    //   blocking time is reasonable (in the order of milliseconds), as
    //   vsync already blocks the render thread, if we block the thread
    //   ourselves by doing operations it would just block less.
    const size_t frame = m_frame;
    if (frame == m_lastFrame)
        return;

    resourceUpdates->uploadTexture(m_rhiTexture, retrieveImage(m_data, frame));

    if (frame > (m_lastFrame + 1))
    {
        const auto skippedFrames = (frame - m_lastFrame - 1);
        qDebug() << "RlottieDynamicTexture: skipped" << skippedFrames
                 << "frame(s). Interface frame rate is too low to accomodate the frame rate "
                 << "of the animation:" << m_frameRate << ". Use threaded render loop or upgrade your gpu.";
#ifdef QT_RLOTTIE_COMPRESSION_AUTO_DISABLE
        g_skippedFrames += skippedFrames;
        if (g_skippedFrames >= QT_RLOTTIE_COMPRESSION_AUTO_DISABLE_SKIPPED_FRAME_THRESHOLD)
        {
            if (g_shouldCompress.exchange(false))
            {
                qDebug() << "Skipped" << g_skippedFrames << "in total, which is over the " \
                            "compression auto disable threshold:"
                         << QT_RLOTTIE_COMPRESSION_AUTO_DISABLE_SKIPPED_FRAME_THRESHOLD
                         << ". Compression is now disabled.";
            }
        }
#endif
    }

    m_lastFrame = frame;

    emit frameChangeCommitted(); // It is crucial to not queue this signal.
}

bool RlottieDynamicTexture::updateTexture()
{
    // We should not do anything time taking here, because it may also be called during scene graph
    // synchronization, which means the ui thread is blocked. Instead of uploading the texture here,
    // we do it in `commitTextureOperations()`, this is also aligned with the behavior of `QSGPlainTexture`.

    if (m_lastFrame != m_frame)
    {
        // Actual update takes place in `commitTextureOperations()`. This is just in case the method
        // is called:
        return true;
    }

    return false;
}

QImage RlottieDynamicTexture::retrieveImage(const size_t index)
{
    return retrieveImage(m_data, index);
}

void RlottieDynamicTexture::tick()
{
    if (m_frame >= (m_data.totalFrame - 1))
    {
        if (m_stopWhenFinished)
        {
            if (isAnimating())
            {
                setAnimating(false);
                emit animatingOverridden(false);
            }
            return;
        }
        else
        {
            m_frame = 0;
        }
    }
    else
    {
        ++m_frame;
    }
}

RlottieTextureFactory::RlottieTextureFactory(const RlottieLottieData &data, bool autoStart)
    : QQuickTextureFactory()
    , m_data(data)
    , m_autoStart(autoStart)
{

}

QSGTexture *RlottieTextureFactory::createTexture(QQuickWindow *window) const
{
    assert(m_data.images.size() > 0);
    assert(window);

    const QRhi* const rhi = getRhiFromWindow(window);

    if (!rhi)
    {
        // If non-rhi (such as, software mode), first frame is used as fallback:
        qWarning() << "RlottieImageProvider: rhi is not available, lottie animations will not work.";
        return window->createTextureFromImage(RlottieDynamicTexture::retrieveImage(m_data, 0), QQuickWindow::TextureHasAlphaChannel);
    }

    return new RlottieDynamicTexture(window, m_data, m_autoStart);
}

QSize RlottieTextureFactory::textureSize() const
{
    if (m_data.images.size() > 0)
        return m_data.size;
    else
        return {};
}

int RlottieTextureFactory::textureByteCount() const
{
    if (m_data.images.size() > 0)
        return (m_data.images.size() * m_data.images[0].size());
    else
        return 0;
}

QImage RlottieTextureFactory::image() const
{
    if (m_data.images.size() > 0)
        return RlottieDynamicTexture::retrieveImage(m_data, 0);
    return {};
}

RlottieRenderRunnable::RlottieRenderRunnable(const QString &path, const std::optional<QSize> &requestedSize, const bool cache, const bool rgbSwap)
    : QObject()
    , QRunnable()
    , m_path(path)
    , m_requestedSize(requestedSize)
    , m_cache(cache)
    , m_rgbSwap(rgbSwap)
{

}

void RlottieRenderRunnable::run()
{
    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly))
    {
        emit failed(QStringLiteral("RlottieRenderRunnable: can not open file \"%1\" for read.").arg(m_path));
        return;
    }

    const std::unique_ptr<rlottie::Animation> animation = rlottie::Animation::loadFromData(file.readAll().toStdString(),
                                                                                           m_path.toStdString(),
                                                                                           {},
                                                                                           m_cache);

    emit progressChanged(0.25); // Loading data accounts fixed amount of progress (25%) for now.

    const size_t totalFrame = animation->totalFrame();

    if (totalFrame == 0)
    {
        emit failed(QStringLiteral("RlottieRenderRunnable: rlottie reports no frame for file \"%1\".").arg(m_path));
        return;
    }

    RlottieLottieData data;
    data.images.reserve(totalFrame);

    size_t implicitWidth, implicitHeight;
    animation->size(implicitWidth, implicitHeight);
    data.implicitSize = {static_cast<int>(implicitWidth), static_cast<int>(implicitHeight)};

    QSize size;
    if (m_requestedSize && m_requestedSize->isValid())
    {
        size = *m_requestedSize;
    }
    else
    {
        if (implicitWidth == 0 || implicitHeight == 0)
        {
            emit failed(QStringLiteral("RlottieRenderRunnable: rlottie reports invalid size for file \"%1\".").arg(m_path));
            return;
        }

        size = data.implicitSize;
    }

    data.size = size;
    data.frameRate = animation->frameRate();
    data.totalFrame = animation->totalFrame();
    data.duration = animation->duration();
    data.isBGR = !m_rgbSwap; // rlottie default
#if QT_RLOTTIE_COMPRESSION_USE_LZAV || (QT_RLOTTIE_COMPRESSION_LEVEL > 0)
#ifndef NDEBUG
    qsizetype cumulativeUncompressedSize = 0;
    qsizetype cumulativeCompressedSize = 0;
#endif
    if (g_shouldCompress)
    {
        data.compressed = true;        
    }
    else
#endif
    {
        data.compressed = false;
    }

    for (size_t i = 0; i < totalFrame; ++i)
    {
        if (m_cancelRequest)
        {
            emit failed(QStringLiteral("RlottieRenderRunnable: explicit cancel request received."));
            return;
        }

        const auto width = size.width();
        const auto height = size.height();

        QByteArray imageData(width * height * 4, Qt::Initialization::Uninitialized);

        rlottie::Surface surface(reinterpret_cast<uint32_t*>(imageData.data()),
                                 width,
                                 height,
                                 static_cast<size_t>(width * 4));

        // For now, frame rendering is not multi threaded. If one frame is expected to take few
        // milliseconds, overhead of threads would not worth it. At the same time, it is also
        // expected to load multiple lottie animations at the same time, so we are already
        // exhausting the thread pool.
        animation->renderSync(i, surface);

        if (m_rgbSwap)
        {
            // BGR -> RGB
            const auto imageDataPtr = imageData.data();

            // Taken from `QImage::rgbSwapped_inplace()`:
            for (int i = 0; i < height; i++)
            {
                uint *p = (uint*)(imageDataPtr + (i * width * 4));
                uint *end = p + width;
                while (p < end)
                {
                    uint c = *p;
                    *p = ((c << 16) & 0xff0000) | ((c >> 16) & 0xff) | (c & 0xff00ff00);
                    p++;
                }
            }
        }

#if QT_RLOTTIE_COMPRESSION_USE_LZAV || (QT_RLOTTIE_COMPRESSION_LEVEL > 0)
        if (data.compressed)
        {
#ifndef NDEBUG
            cumulativeUncompressedSize += imageData.size();
#endif

#if QT_RLOTTIE_COMPRESSION_USE_LZAV
            const qsizetype srcSize = imageData.size();
            const auto dstSize = lzav_compress_bound(srcSize);
            QByteArray compressedData(dstSize, Qt::Initialization::Uninitialized);
            assert(compressedData.size() == dstSize);

            const auto compressedSize = lzav_compress_default(imageData.constData(), compressedData.data(), srcSize, dstSize);

            if (Q_UNLIKELY(compressedSize == 0 && srcSize != 0))
            {
                qCritical() << "RlottieRenderRunnable: LZAV failed to compress the image. Compression is disabled.";
                data.compressed = false;
            }
            else
            {
                compressedData.resize(compressedSize);

                QByteArray result;
                {
                    QDataStream stream(&result, QIODeviceBase::WriteOnly);
                    stream << srcSize << compressedData;
                }
                imageData = std::move(result);

#ifndef NDEBUG
                cumulativeCompressedSize += compressedSize;
#endif
            }
#else
            imageData = qCompress(imageData, QT_RLOTTIE_COMPRESSION_LEVEL);
#ifndef NDEBUG
            cumulativeCompressedSize += imageData.size();
#endif
#endif
        }
#endif

        data.images.push_back(imageData);

        if (i % (totalFrame / 10) == 0)
            emit progressChanged(0.25 + 0.75 * i / totalFrame);
    }

#if QT_RLOTTIE_COMPRESSION_USE_LZAV || (QT_RLOTTIE_COMPRESSION_LEVEL > 0)
#ifndef NDEBUG
    if (data.compressed)
    {
        const auto cumulativeUncompressedSizeKb = cumulativeUncompressedSize / 1024;
        const auto cumulativeCompressedSizeKb = cumulativeCompressedSize / 1024;
        qDebug() << "RlottieRenderRunnable: Original size:"
                 << cumulativeUncompressedSizeKb
                 << "KB, compressed size:"
                 << cumulativeCompressedSizeKb
                 << "KB. Saved"
                 << (cumulativeUncompressedSizeKb - cumulativeCompressedSizeKb)
                 << "KB.";
    }
#endif
#endif

    emit progressChanged(1.0);

    emit succeeded(data);
}

void RlottieRenderRunnable::requestCancel()
{
    m_cancelRequest = true;
}

RlottieTextureProvider::RlottieTextureProvider(QQuickWindow *window, RlottieItem *item)
    : QSGTextureProvider()
    , m_window(window)
    , m_item(item)
{

}

RlottieTextureProvider::~RlottieTextureProvider()
{
    releaseTexture();
}

QPointer<QQuickWindow> RlottieTextureProvider::window() const
{
    return m_window;
}

void RlottieTextureProvider::setWindow(QPointer<QQuickWindow> window)
{
    m_window = window;
}

QSGTexture *RlottieTextureProvider::texture() const
{
    return m_texture;
}

void RlottieTextureProvider::generateTexture(const RlottieLottieData &data, const bool autoStart)
{
    if (Q_UNLIKELY(!m_window))
    {
        qCritical() << "RlottieTextureProvider: no valid window provided, can not generate the texture.";
        return;
    }

    RlottieTextureFactory factory(data, autoStart);
    delete m_texture;
    m_texture = factory.createTexture(m_window);
    emit textureChanged();
}

void RlottieTextureProvider::regenerate(const QString &path,
                                        const std::optional<QSize> &size,
                                        const bool cache,
                                        const bool retainWhileLoading,
                                        const bool asynchronous,
                                        const bool autoStart)
{
    assert(!path.isEmpty());

    if (m_runnable)
    {
        disconnect(m_runnable, nullptr, this, nullptr);
        m_runnable->requestCancel();
        // Since auto delete is true, it will be deleted by the thread pool. We should not forcefully delete
        // the runnable while it is running, since `run()` may directly or indirectly access the class members
        // at any point.
    }

    static std::optional<bool> rgbSwap;
    if (!rgbSwap)
    {
        if (m_window)
        {
            const QRhi* const rhi = getRhiFromWindow(m_window);
            if (rhi)
            {
                if (rhi->isTextureFormatSupported(QRhiTexture::BGRA8))
                {
                    rgbSwap = false;
                    qDebug() << "RlottieTextureProvider: The graphics stack supports BGRA8 textures, RGB " \
                                "swapping will not be performed.";
                }
                else
                {
                    rgbSwap = true;
                    // Note that RGBA8 is noted to be always supported.
                    qWarning() << "RlottieTextureProvider: The graphics stack does not support BGRA8 textures, " \
                                  "loading lottie animations will take more time due to RGB swapping. " \
                                  "This does not affect the rendering performance.";
                }
            }
        }
        else
        {
            qWarning() << "RlottieTextureProvider: window is still not provided, can not determine supported texture type.";
        }
    }

    if (rgbSwap)
        m_runnable = new RlottieRenderRunnable(path, size, cache, *rgbSwap);
    else
        m_runnable = new RlottieRenderRunnable(path, size, cache);

    // FIXME: With recent Qt version(s), if there is no update done in the interface, the rendering thread may
    //        not respect the signals connected. This is reproduced at startup with Qt 6.10.1, when the user does
    //        not interact with the interface.
    if (m_item)
    {
        // This is less expensive than calling `update()` on the window itself. Again, we do not
        // need to update the item here (before the texture is ready), it is just to make sure
        // the signals are respected. In any case, this is perfectly fine because these signals
        // are fired only when source or source size changes (which is rare).
        connect(m_runnable, &RlottieRenderRunnable::succeeded, m_item, &QQuickItem::update);
        connect(m_runnable, &RlottieRenderRunnable::failed, m_item, &QQuickItem::update);
    }

    connect(m_runnable, &RlottieRenderRunnable::succeeded, this, [this, autoStart](const RlottieLottieData& data) {
        generateTexture(data, autoStart);
        // This may also be emitted before `generateTexture()` (image(s) are ready, texture is not):
        emit generationSucceeded();
    });
    connect(m_runnable, &RlottieRenderRunnable::failed, this, [this](const QString& errorString) {
        releaseTexture();
        emit generationFailed(errorString);
    });
    connect(m_runnable, &RlottieRenderRunnable::progressChanged, this, &RlottieTextureProvider::progressChanged);

    if (!retainWhileLoading)
        releaseTexture();

    if (asynchronous)
    {
        const auto tp = QThreadPool::globalInstance();
        assert(tp);
        tp->start(m_runnable); // takes ownership (auto delete)
    }
    else
    {
        m_runnable->run();
        delete m_runnable;
    }
}

void RlottieTextureProvider::releaseTexture()
{
    if (m_texture)
    {
        delete m_texture;
        emit textureChanged();
    }
}

RlottieImageResponse::RlottieImageResponse(const QString &id, const QSize &requestedSize)
    : QQuickImageResponse()
    , m_runnable(new RlottieRenderRunnable(id, requestedSize))
{
    connect(m_runnable, &RlottieRenderRunnable::succeeded, this, [this](const RlottieLottieData& data) {
        m_data = data;
        m_errorString.clear();
        emit finished();
    });

    connect(m_runnable, &RlottieRenderRunnable::failed, this, [this](const QString& errorString) {
        m_data.reset();
        m_errorString = errorString;
        emit finished();
    });

    const auto tp = QThreadPool::globalInstance();
    assert(tp);
    tp->start(m_runnable); // takes ownership (auto delete)
}

QQuickTextureFactory *RlottieImageResponse::textureFactory() const
{
    if (m_data)
        return new RlottieTextureFactory(*m_data);
    else
        return nullptr;
}

QString RlottieImageResponse::errorString() const
{
    return m_errorString;
}

void RlottieImageResponse::cancel()
{
    if (m_runnable)
        m_runnable->requestCancel();
}

RlottieImageProvider::RlottieImageProvider() : QQuickAsyncImageProvider()
{
    qRegisterMetaType<RlottieLottieData>();
}

QQuickImageResponse *RlottieImageProvider::requestImageResponse(const QString &id, const QSize &requestedSize)
{
    return new RlottieImageResponse(QQmlFile::urlToLocalFileOrQrc(id), requestedSize);
}

static QQuickImageProvider* NewImageProvider()
{
    return new RlottieImageProvider();
}

static void RegisterQmlModuleAndTypes()
{
    const char* uri = "VLC.Lottie";
    const int versionMajor = 1;
    const int versionMinor = 0;

    // @uri VLC.Lottie
    qmlRegisterModule(uri, versionMajor, versionMinor);
    qmlRegisterType<RlottieItem>(uri, versionMajor, versionMinor, "LottieAnimation");
    qmlProtectModule(uri, versionMajor);
}

int RlottieOpen(vlc_object_t* const p_this)
{
    assert(p_this);

    const auto obj = reinterpret_cast<LottieModule*>(p_this);
    obj->newImageProvider = NewImageProvider;
    obj->registerQmlModuleAndTypes = RegisterQmlModuleAndTypes;

    if constexpr (QT_RLOTTIE_COMPRESSION_USE_LZAV)
        qDebug() << "QtRlottie: using LZAV for compression.";

    return VLC_SUCCESS;
}

#ifndef QT_STATIC // submodule otherwise
vlc_module_begin()
    add_shortcut("QtRlottie")
    set_description("Provides lottie support through rlottie.")
    set_capability("qtlottie", 10)
    set_callback(RlottieOpen)
vlc_module_end()
#endif
