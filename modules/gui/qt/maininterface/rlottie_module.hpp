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
#ifndef RLOTTIE_MODULE_HPP
#define RLOTTIE_MODULE_HPP

#include "lottie_module.hpp"

#include <QSGDynamicTexture>
#include <QQuickImageResponse>
#include <QQuickAsyncImageProvider>
#include <QQmlEngine>
#include <QRunnable>
#include <QBasicTimer>
#include <QQuickItem>
#include <QSGTextureProvider>

#include <vlc_common.h>

int RlottieOpen(vlc_object_t* const p_this);

// This item can paint the lottie animation itself, but it is also a texture provider.
// If you want to delegate another item, such as `ShaderEffect` or `ImageExt`, to paint
// the texture, make this item invisible. The main purpose of this item is when the
// image provider is not enough, which is only useful to display lottie animations
// constantly animated whereas this item allows finer control.
class RlottieItem : public LottieItem
{
    Q_OBJECT

    QML_ELEMENT

public:
    explicit RlottieItem(QQuickItem *parent = nullptr);
    ~RlottieItem() override;

    bool isTextureProvider() const override;
    QSGTextureProvider *textureProvider() const override;

    Q_INVOKABLE int currentFrame() const override; // Avoid using this method with the threaded render loop.
    Q_INVOKABLE void gotoFrame(int frame) override;
    Q_INVOKABLE void restart() override;

public slots:
    void invalidateSceneGraph();

protected:
    void releaseResources() override;
    QSGNode *updatePaintNode(QSGNode *, UpdatePaintNodeData *) override;

private:
    bool regenerateTexture();

private slots:
    void markDirtyFrame();

private:
    mutable QPointer<class RlottieTextureProvider> m_textureProvider;

    mutable bool m_threadedRenderLoop = false; // This can be controlled with `QSG_RENDER_LOOP`.

    mutable QPointer<const class RlottieDynamicTexture> m_oldTexture;

    std::atomic<bool> m_dirtyFrame = false;
};

// Just for aliasing:
using RlottieTextureProviderItem = RlottieItem;

struct RlottieLottieData
{
    // NOTE: `QByteArray` is implicitly shared.
    std::vector<QByteArray> images;
    bool isBGR;
    bool compressed;

    QSize size; // Same as image size.
    QSize implicitSize; // Natural size of the lottie file.

    double duration;
    double frameRate;
    size_t totalFrame;
};

class RlottieDynamicTexture : public QSGDynamicTexture
{
    Q_OBJECT

public:
    explicit RlottieDynamicTexture(QQuickWindow *window, const RlottieLottieData& data, bool autoStart = true);
    ~RlottieDynamicTexture() override;

    bool isAnimating() const;
    void setAnimating(bool enabled);

    bool stopWhenFinished() const;
    void setStopWhenFinished(bool stop);

    size_t currentFrame() const;
    void setCurrentFrame(const size_t frame);

    double frameRate() const;
    void setFrameRate(std::optional<double> frameRate);

    double implicitFrameRate() const;
    QSize implicitSize() const;

    size_t frameCount() const;

    static QImage retrieveImage(const RlottieLottieData& data, const size_t index);

    // QSGTexture interface:
    qint64 comparisonKey() const override;
    QRhiTexture *rhiTexture() const override;
    QSize textureSize() const override;
    bool hasAlphaChannel() const override;
    bool hasMipmaps() const override;
    void commitTextureOperations(QRhi *, QRhiResourceUpdateBatch *) override;

    // QSGDynamicTexture interface:
    bool updateTexture() override;

signals:
    void animatingOverridden(bool);
    void frameChangeCommitted(); // Make sure to not queue this signal.

private:
    QImage retrieveImage(const size_t index);

private slots:
    void tick(); // Increase frame by one. This slot is thread-safe.

private:
    class QRhiTexture *m_rhiTexture = nullptr;

    RlottieLottieData m_data;

    QPointer<QTimer> m_timer;

    std::atomic<size_t> m_frame = 0;
    size_t m_lastFrame = 0;

    double m_frameRate; // effective frame rate

    bool m_stopWhenFinished = false;
};

class RlottieTextureFactory : public QQuickTextureFactory
{
    Q_OBJECT

public:
    explicit RlottieTextureFactory(const RlottieLottieData& data, bool autoStart = true);

    QSGTexture *createTexture(QQuickWindow *window) const override;
    QSize textureSize() const override;
    int textureByteCount() const override;
    QImage image() const override;

private:
    const RlottieLottieData m_data;
    const bool m_autoStart;
};

class RlottieRenderRunnable : public QObject, public QRunnable
{
    Q_OBJECT

public:
    explicit RlottieRenderRunnable(const QString& path,
                                   const std::optional<QSize>& requestedSize,
                                   const bool cache = true,
                                   const bool rgbSwap = false);

    void run() override;

    void requestCancel();

signals:
    void succeeded(const RlottieLottieData& images);
    void failed(const QString& reason);
    void progressChanged(const double progress);

private:
    const QString m_path;
    const std::optional<QSize> m_requestedSize;
    const bool m_cache;
    const bool m_rgbSwap;

    std::atomic<bool> m_cancelRequest = { false };
};

class RlottieTextureProvider : public QSGTextureProvider
{
    Q_OBJECT

public:
    explicit RlottieTextureProvider(QQuickWindow* window, RlottieItem* item = nullptr);
    ~RlottieTextureProvider() override;

    QPointer<QQuickWindow> window() const;
    void setWindow(QPointer<QQuickWindow> window);

    // Generates a new texture:
    void regenerate(const QString& path,
                    const std::optional<QSize>& size,
                    const bool cache = true,
                    const bool retainWhileLoading = true,
                    const bool asynchronous = true,
                    const bool autoStart = true);
    // Releases the existing texture:
    void releaseTexture();

    QSGTexture *texture() const override;

signals:
    void generationSucceeded();
    void generationFailed(const QString& reason);
    void progressChanged(const double progress);

private:
    void generateTexture(const RlottieLottieData&, const bool autoStart);

private:
    QPointer<QQuickWindow> m_window;
    QPointer<RlottieItem> m_item;
    QPointer<QSGTexture> m_texture; // RlottieDynamic texture if rhi, not otherwise
    QPointer<RlottieRenderRunnable> m_runnable;
};

class RlottieImageResponse : public QQuickImageResponse
{
    Q_OBJECT

public:
    explicit RlottieImageResponse(const QString& id, const QSize& requestedSize);

    QQuickTextureFactory *textureFactory() const override;
    QString errorString() const override;

public Q_SLOTS:
    void cancel() override;

private:
    QPointer<RlottieRenderRunnable> m_runnable;
    QString m_errorString;

    std::optional<RlottieLottieData> m_data;
};

class RlottieImageProvider : public QQuickAsyncImageProvider
{
    Q_OBJECT

public:
    explicit RlottieImageProvider();

    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) override;
};

#endif // RLOTTIE_MODULE_HPP
